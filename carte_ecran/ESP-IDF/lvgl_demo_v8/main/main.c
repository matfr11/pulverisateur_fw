/**
 * @file main.c
 * @brief Carte écran 7" — Affichage LVGL + commandes tactiles via MQTT.
 *
 * Rôle : SLAVE WiFi uniquement (pas de Web UI, pas de broker).
 * Se connecte au PULVE_AP, souscrit aux états MQTT, publie les commandes.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "lvgl.h"

#include "board_config.h"
#include "types_pulverisateur.h"
//#include "protocole_wifi.h"
#include "esp_wifi.h"
#include "protocole_mqtt.h"
#include "mqtt_topics.h"

#include "ecran_ui.h"

static const char *TAG = "ECRAN";

/* ====================================================================
 * ÉTAT LOCAL — reçu via MQTT
 * ==================================================================== */
static etat_carte_avant_t    s_etat_avant   = {0};
static etat_carte_arriere_t  s_etat_arriere = {0};
static configuration_t       s_config       = CONFIG_DEFAUT;
static role_reseau_t         s_role         = ROLE_INDEFINI;

/* Timestamps dernière réception pour détecter link perdu */
static int64_t s_ts_avant   = 0;
static int64_t s_ts_arriere = 0;

/* ====================================================================
 * CALLBACK — COMMANDES UI → MQTT
 * ==================================================================== */
static void on_ui_cmd(const char *cible, const char *cmd, const char *val)
{
    ESP_LOGI(TAG, "UI CMD: cible=%s cmd=%s val=%s", cible, cmd, val ? val : "NULL");

    if (strcmp(cible, "urgence") == 0) {
        mqtt_publier_arret_urgence();
    } else if (strcmp(cible, "avant") == 0) {
        mqtt_publier_commande_avant(cmd, val);
    } else if (strcmp(cible, "arriere") == 0) {
        mqtt_publier_commande_arriere(cmd, val);
    }
}

/* ====================================================================
 * CALLBACK — SAUVEGARDE CONFIG UI → MQTT
 * ==================================================================== */
static void on_ui_save_config(const configuration_t *cfg)
{
    ESP_LOGI(TAG, "Config modifiée depuis l'écran, publication MQTT.");
    s_config = *cfg;
    mqtt_publier_mise_a_jour_config(cfg);
}

/* ====================================================================
 * CALLBACK — RÉCEPTION ÉTAT MQTT → UI
 * ==================================================================== */
static void on_etat_recu(const char *topic, const char *payload, int len)
{
    /* Créer une copie null-terminated */
    char *buf = malloc(len + 1);
    if (!buf) return;
    memcpy(buf, payload, len);
    buf[len] = '\0';

    int64_t now = esp_timer_get_time() / 1000;

    if (strstr(topic, "/avant")) {
        if (json_deserialiser_etat_avant(buf, &s_etat_avant)) {
            s_ts_avant = now;
        }
    } else if (strstr(topic, "/arriere")) {
        if (json_deserialiser_etat_arriere(buf, &s_etat_arriere)) {
            s_ts_arriere = now;
        }
    }

    free(buf);
}

/* ====================================================================
 * CALLBACK — RÉCEPTION CONFIG MQTT
 * ==================================================================== */
static void on_config_recue(const configuration_t *cfg)
{
    ESP_LOGI(TAG, "Configuration reçue via MQTT.");
    s_config = *cfg;
}

/* ====================================================================
 * TÂCHE PRINCIPALE — MISE À JOUR PÉRIODIQUE DE L'UI
 * ==================================================================== */
#define UI_REFRESH_MS   500
#define LINK_TIMEOUT_MS 5000

static void tache_ecran(void *arg)
{
    ESP_LOGI(TAG, "Tâche écran démarrée.");

    /* Attendre la connexion WiFi */
/* Attendre la connexion WiFi (polling simple) */
    wifi_ap_record_t ap_info;
    while (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        ESP_LOGI(TAG, "En attente de connexion WiFi...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();  /* Réessayer */
    }
    ESP_LOGI(TAG, "WiFi connecté à %s", WIFI_SSID_AP);

    /* Initialiser MQTT */
    mqtt_initialiser(MQTT_BROKER_URI_MASTER, CARTE_ID_ECRAN);
    mqtt_enregistrer_callback_etat(on_etat_recu);
    mqtt_enregistrer_callback_config(on_config_recue);
    mqtt_souscrire_etats();

    /* Demander la config au MASTER */
    vTaskDelay(pdMS_TO_TICKS(1000));
    mqtt_demander_configuration();

    /* Charger la config dans l'UI */
    bsp_display_lock(0);
    ecran_ui_set_config(&s_config);
    bsp_display_unlock();

    /* Boucle de rafraîchissement */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(UI_REFRESH_MS));

        int64_t now = esp_timer_get_time() / 1000;
        bool link_av = (now - s_ts_avant) < LINK_TIMEOUT_MS;
        bool link_ar = (now - s_ts_arriere) < LINK_TIMEOUT_MS;

        bsp_display_lock(0);

        ecran_ui_update_avant(&s_etat_avant);
        ecran_ui_update_arriere(&s_etat_arriere);
        ecran_ui_update_reseau("AV", link_av, link_ar);

        /* Rafraîchir la config si elle a changé */
        ecran_ui_set_config(&s_config);

        bsp_display_unlock();
    }
}

/* ====================================================================
 * POINT D'ENTRÉE
 * ==================================================================== */
void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  PULVERISATEUR — CARTE ECRAN 7\"");
    ESP_LOGI(TAG, "  Firmware %s | Protocole v%d", VERSION_FIRMWARE, VERSION_PROTOCOLE);
    ESP_LOGI(TAG, "========================================");

    /* 1. NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. Display + LVGL */
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    /* 3. Créer l'interface LVGL */
    bsp_display_lock(0);
    ecran_ui_creer(on_ui_cmd, on_ui_save_config);
    bsp_display_unlock();
    ESP_LOGI(TAG, "Interface LVGL initialisée.");

    /* 4. WiFi STA uniquement (la carte écran ne devient jamais MASTER) */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t sta_cfg = {
        .sta = {
            .ssid = WIFI_SSID_AP,
            .password = WIFI_PASS_AP,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    s_role = ROLE_SLAVE;
    ESP_LOGI(TAG, "WiFi STA démarré, connexion à %s...", WIFI_SSID_AP);

    /* 5. Lancer la tâche de rafraîchissement UI + MQTT */
    xTaskCreatePinnedToCore(tache_ecran, "ecran_task", 8192, NULL, 5, NULL, 1);
}