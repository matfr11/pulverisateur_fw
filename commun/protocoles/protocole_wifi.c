/**
 * @file protocole_wifi.c
 * @brief Implémentation WiFi AP/STA avec failover automatique.
 */
#include "protocole_wifi.h"
#include "mqtt_topics.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_mac.h"

#include <string.h>

static const char *TAG = "WIFI";

/* ====================================================================
 * VARIABLES INTERNES
 * ==================================================================== */
static role_reseau_t        s_role = ROLE_INDEFINI;
static bool                 s_connecte = false;
static EventGroupHandle_t   s_wifi_events = NULL;
static TimerHandle_t        s_timer_failover = NULL;
static esp_netif_t         *s_netif_ap = NULL;
static esp_netif_t         *s_netif_sta = NULL;

#define WIFI_BIT_CONNECTE       BIT0
#define WIFI_BIT_DECONNECTE     BIT1
#define WIFI_BIT_AP_DEMARRE     BIT2

static int s_nb_tentatives_reconnexion = 0;
static bool s_en_scan = false; 

#define MAX_TENTATIVES_RECONNEXION  5

/* ====================================================================
 * PROTOTYPES INTERNES
 * ==================================================================== */
static esp_err_t wifi_demarrer_ap(void);
static esp_err_t wifi_demarrer_sta(void);
static bool      wifi_scanner_reseau(void);
static void      timer_failover_cb(TimerHandle_t timer);

/* ====================================================================
 * HANDLER D'ÉVÉNEMENTS
 * ==================================================================== */
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
                case WIFI_EVENT_STA_START:
            if (!s_en_scan) {
                ESP_LOGI(TAG, "STA démarré, connexion en cours...");
                esp_wifi_connect();
            }
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            s_connecte = false;
            xEventGroupSetBits(s_wifi_events, WIFI_BIT_DECONNECTE);
            xEventGroupClearBits(s_wifi_events, WIFI_BIT_CONNECTE);

            s_nb_tentatives_reconnexion++;
            if (s_nb_tentatives_reconnexion < MAX_TENTATIVES_RECONNEXION) {
                ESP_LOGW(TAG, "Déconnecté, tentative %d/%d...",
                         s_nb_tentatives_reconnexion, MAX_TENTATIVES_RECONNEXION);
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_wifi_connect();
            } else {
                ESP_LOGE(TAG, "Connexion perdue. Lancement failover dans %d ms.",
                         WIFI_FAILOVER_TIMEOUT_MS);
                /* Démarrer le timer de failover */
                if (s_timer_failover) {
                    xTimerStart(s_timer_failover, 0);
                }
            }
            break;

        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t *evt = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Station connectée au AP, MAC: " MACSTR, MAC2STR(evt->mac));
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t *evt = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGW(TAG, "Station déconnectée du AP, MAC: " MACSTR, MAC2STR(evt->mac));
            break;
        }

        default:
            break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "IP obtenue : " IPSTR, IP2STR(&evt->ip_info.ip));
            s_connecte = true;
            s_nb_tentatives_reconnexion = 0;
            xEventGroupSetBits(s_wifi_events, WIFI_BIT_CONNECTE);
            xEventGroupClearBits(s_wifi_events, WIFI_BIT_DECONNECTE);

            /* Annuler le timer de failover si actif */
            if (s_timer_failover) {
                xTimerStop(s_timer_failover, 0);
            }
        }
    }
}

/* ====================================================================
 * SCAN RÉSEAU
 * ==================================================================== */
static bool wifi_scanner_reseau(void)
{
    ESP_LOGI(TAG, "Scan des réseaux WiFi...");

    /* Initialisation temporaire en mode STA pour le scan */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    s_en_scan = true;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 150,
        .scan_time.active.max = 500,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true /* bloquant */);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Erreur scan : %s", esp_err_to_name(err));
        esp_wifi_stop();
        esp_wifi_deinit();
        return false;
    }

    uint16_t nb_resultats = 0;
    esp_wifi_scan_get_ap_num(&nb_resultats);

    bool trouve = false;
    if (nb_resultats > 0) {
        wifi_ap_record_t *resultats = malloc(nb_resultats * sizeof(wifi_ap_record_t));
        if (resultats) {
            esp_wifi_scan_get_ap_records(&nb_resultats, resultats);
            for (int i = 0; i < nb_resultats; i++) {
                if (strcmp((char *)resultats[i].ssid, WIFI_SSID_AP) == 0) {
                    ESP_LOGI(TAG, "Réseau '%s' trouvé (RSSI: %d)", WIFI_SSID_AP, resultats[i].rssi);
                    trouve = true;
                    break;
                }
            }
            free(resultats);
        }
    }
    s_en_scan = false;
    esp_wifi_stop();
    esp_wifi_deinit();

    if (!trouve) {
        ESP_LOGI(TAG, "Réseau '%s' NON trouvé.", WIFI_SSID_AP);
    }
    return trouve;
}

/* ====================================================================
 * DÉMARRAGE MODE AP (MASTER)
 * ==================================================================== */
static esp_err_t wifi_demarrer_ap(void)
{
    ESP_LOGI(TAG, "=== DÉMARRAGE EN MODE AP (MASTER) ===");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_SSID_AP,
            .password = WIFI_PASS_AP,
            .ssid_len = strlen(WIFI_SSID_AP),
            .channel = WIFI_CHANNEL_AP,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = WIFI_MAX_CONN_AP,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_role = ROLE_MASTER;
    s_connecte = true;
    xEventGroupSetBits(s_wifi_events, WIFI_BIT_AP_DEMARRE);

    ESP_LOGI(TAG, "AP démarré : SSID='%s', Canal=%d", WIFI_SSID_AP, WIFI_CHANNEL_AP);
    return ESP_OK;
}

/* ====================================================================
 * DÉMARRAGE MODE STA (SLAVE)
 * ==================================================================== */
static esp_err_t wifi_demarrer_sta(void)
{
    ESP_LOGI(TAG, "=== DÉMARRAGE EN MODE STA (SLAVE) ===");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t sta_config = {
        .sta = {
            .ssid = WIFI_SSID_AP,
            .password = WIFI_PASS_AP,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_role = ROLE_SLAVE;

    /* Attendre la connexion (timeout 10s) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_events,
        WIFI_BIT_CONNECTE | WIFI_BIT_DECONNECTE,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & WIFI_BIT_CONNECTE) {
        ESP_LOGI(TAG, "Connecté au MASTER.");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Échec connexion STA. Tentative AP...");
    esp_wifi_stop();
    esp_wifi_deinit();
    return wifi_demarrer_ap();
}

/* ====================================================================
 * TIMER FAILOVER
 * ==================================================================== */
static void timer_failover_cb(TimerHandle_t timer)
{
    ESP_LOGW(TAG, "Timer failover expiré ! Bascule vers MASTER.");
    wifi_failover_vers_master();
}

/* ====================================================================
 * FONCTIONS PUBLIQUES
 * ==================================================================== */
esp_err_t wifi_initialiser(role_reseau_t *role_out)
{
    /* Initialisation NVS (nécessaire pour WiFi) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialisation réseau */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_netif_ap = esp_netif_create_default_wifi_ap();
    s_netif_sta = esp_netif_create_default_wifi_sta();

    /* Créer le groupe d'événements */
    s_wifi_events = xEventGroupCreate();

    /* Enregistrer les handlers d'événements */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    /* Créer le timer de failover (one-shot) */
    s_timer_failover = xTimerCreate("failover",
        pdMS_TO_TICKS(WIFI_FAILOVER_TIMEOUT_MS),
        pdFALSE, NULL, timer_failover_cb);

    /* Scanner pour déterminer le rôle */
    bool reseau_existant = wifi_scanner_reseau();

    if (reseau_existant) {
        ret = wifi_demarrer_sta();
    } else {
        ret = wifi_demarrer_ap();
    }

    if (role_out) {
        *role_out = s_role;
    }
    return ret;
}

role_reseau_t wifi_obtenir_role(void)
{
    return s_role;
}

bool wifi_est_connecte(void)
{
    return s_connecte;
}

esp_err_t wifi_failover_vers_master(void)
{
    ESP_LOGW(TAG, "=== FAILOVER : SLAVE → MASTER ===");

    /* Arrêter le mode STA */
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    s_connecte = false;
    s_nb_tentatives_reconnexion = 0;

    /* Redémarrer en mode AP */
    return wifi_demarrer_ap();
}
