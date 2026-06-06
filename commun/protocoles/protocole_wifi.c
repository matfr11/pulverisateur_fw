/**
 * @file protocole_wifi.c
 * @brief Gestion WiFi : Point d'Accès pour le serveur, Client STA pour les relais.
 *
 * Cette version remplace l'ancien mécanisme master/slave dynamique.
 * Les rôles sont maintenant fixes, déterminés au moment de la compilation
 * par le paramètre est_serveur passé à wifi_initialiser().
 *
 * Différences avec l'ancienne version :
 *   - Plus de scan réseau au démarrage (on sait déjà qui on est)
 *   - Plus de failover (le serveur ne peut pas "devenir" esclave et vice versa)
 *   - Plus de conflits au redémarrage simultané
 *   - Reconnexion automatique simple pour les cartes relais (sans timer failover)
 */
#include "protocole_wifi.h"
#include "mqtt_topics.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"

#include <string.h>

static const char *TAG = "WIFI";

/* ====================================================================
 * VARIABLES INTERNES
 * ==================================================================== */
static role_reseau_t        s_role = ROLE_INDEFINI;
static bool                 s_connecte = false;
static EventGroupHandle_t   s_wifi_events = NULL;
static esp_netif_t         *s_netif_ap = NULL;
static esp_netif_t         *s_netif_sta = NULL;

/* Bits d'événements pour la synchronisation de démarrage */
#define WIFI_BIT_CONNECTE       BIT0   /* IP obtenue (mode STA) */
#define WIFI_BIT_AP_DEMARRE     BIT1   /* Point d'accès actif (mode AP) */

/*
 * Stratégie de reconnexion pour les cartes relais :
 *
 *   Phase 1 : MAX_TENTATIVES_RAPIDES tentatives toutes les 2 secondes.
 *             Couvre le cas où le serveur redémarre (reprend en ~5s).
 *
 *   Phase 2 : Tentatives toutes les 15 secondes jusqu'au retour du serveur.
 *             Évite de saturer le réseau si le serveur est hors ligne.
 */
#define MAX_TENTATIVES_RAPIDES  5

static int s_nb_tentatives_reconnexion = 0;
static esp_timer_handle_t s_timer_reconnexion = NULL;

/* ====================================================================
 * PROTOTYPES INTERNES
 * ==================================================================== */
static esp_err_t wifi_demarrer_ap(void);
static esp_err_t wifi_demarrer_sta(void);

static void timer_reconnexion_cb(void *arg)
{
    esp_wifi_connect();
}

/* ====================================================================
 * HANDLER D'ÉVÉNEMENTS
 *
 * Appelé par ESP-IDF à chaque événement WiFi ou IP.
 * Ne pas appeler directement.
 * ==================================================================== */
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {

        switch (event_id) {

        case WIFI_EVENT_STA_START:
            /*
             * La carte relais vient d'activer son mode client WiFi.
             * On tente immédiatement de rejoindre le réseau du serveur.
             */
            ESP_LOGI(TAG, "Mode client WiFi démarré. Connexion au serveur en cours...");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            /*
             * La carte relais a perdu la connexion avec le serveur.
             * On tente de se reconnecter selon la stratégie définie :
             *   - D'abord des tentatives rapides (serveur en train de redémarrer ?)
             *   - Puis des tentatives espacées (serveur hors ligne)
             */
            s_connecte = false;
            xEventGroupClearBits(s_wifi_events, WIFI_BIT_CONNECTE);

            s_nb_tentatives_reconnexion++;

            if (esp_timer_is_active(s_timer_reconnexion)) {
                esp_timer_stop(s_timer_reconnexion);
            }

            if (s_nb_tentatives_reconnexion <= MAX_TENTATIVES_RAPIDES) {
                ESP_LOGW(TAG, "Connexion perdue. Tentative %d/%d dans 2s...",
                         s_nb_tentatives_reconnexion, MAX_TENTATIVES_RAPIDES);
                esp_timer_start_once(s_timer_reconnexion, 2000 * 1000);
            } else {
                /* Serveur probablement hors ligne — espacer les tentatives */
                ESP_LOGW(TAG, "Serveur non joignable. Nouvelle tentative dans 15s...");
                s_nb_tentatives_reconnexion = MAX_TENTATIVES_RAPIDES;
                esp_timer_start_once(s_timer_reconnexion, 15000 * 1000);
            }
            break;

        case WIFI_EVENT_AP_STACONNECTED: {
            /*
             * Un appareil vient de rejoindre le point d'accès du serveur.
             * Peut être une carte relais, un téléphone, une tablette, etc.
             * L'identification réelle se fait au niveau MQTT (ID_CARTE).
             */
            wifi_event_ap_staconnected_t *evt = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Appareil connecté au WiFi. MAC: " MACSTR, MAC2STR(evt->mac));
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED: {
            /*
             * Un appareil s'est déconnecté du point d'accès.
             */
            wifi_event_ap_stadisconnected_t *evt = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGW(TAG, "Appareil déconnecté du WiFi. MAC: " MACSTR, MAC2STR(evt->mac));
            break;
        }

        default:
            break;
        }

    } else if (event_base == IP_EVENT) {

        if (event_id == IP_EVENT_STA_GOT_IP) {
            /*
             * La carte relais a obtenu une adresse IP du serveur.
             * La connexion WiFi est maintenant pleinement établie.
             */
            ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Connecté au serveur ! Adresse IP : " IPSTR,
                     IP2STR(&evt->ip_info.ip));

            s_connecte = true;
            s_nb_tentatives_reconnexion = 0;  /* Réinitialiser pour la prochaine fois */
            xEventGroupSetBits(s_wifi_events, WIFI_BIT_CONNECTE);
        }
    }
}

/* ====================================================================
 * DÉMARRAGE EN MODE POINT D'ACCÈS - CARTE SERVEUR
 *
 * Crée le réseau WiFi auquel toutes les cartes relais se connecteront.
 * L'adresse IP du serveur est fixe : 192.168.4.1 (valeur par défaut ESP-IDF).
 * ==================================================================== */
static esp_err_t wifi_demarrer_ap(void)
{
    ESP_LOGI(TAG, "=== DÉMARRAGE EN MODE POINT D'ACCÈS (CARTE SERVEUR) ===");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t ap_config = {
        .ap = {
            .ssid           = WIFI_SSID_AP,
            .password       = WIFI_PASS_AP,
            .ssid_len       = strlen(WIFI_SSID_AP),
            .channel        = WIFI_CHANNEL_AP,
            .authmode       = WIFI_AUTH_WPA2_PSK,
            .max_connection = WIFI_MAX_CONN_AP,  /* Nombre max de cartes relais */
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_role = ROLE_MASTER;
    s_connecte = true;  /* Le serveur est toujours "connecté" : il EST le réseau */
    xEventGroupSetBits(s_wifi_events, WIFI_BIT_AP_DEMARRE);

    ESP_LOGI(TAG, "Point d'accès actif : SSID='%s', Canal=%d, IP=192.168.4.1",
             WIFI_SSID_AP, WIFI_CHANNEL_AP);
    return ESP_OK;
}

/* ====================================================================
 * DÉMARRAGE EN MODE CLIENT - CARTES RELAIS
 *
 * Se connecte au réseau créé par la carte serveur.
 *
 * Si le serveur n'est pas encore démarré (démarrage simultané), on attend
 * jusqu'à 15 secondes. Passé ce délai, la fonction retourne quand même
 * ESP_OK : le handler d'événements continuera les tentatives en arrière-plan
 * et la connexion s'établira dès que le serveur sera disponible.
 * ==================================================================== */
static esp_err_t wifi_demarrer_sta(void)
{
    ESP_LOGI(TAG, "=== DÉMARRAGE EN MODE CLIENT (CARTE RELAIS) ===");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t sta_config = {
        .sta = {
            .ssid      = WIFI_SSID_AP,       /* SSID créé par la carte serveur */
            .password  = WIFI_PASS_AP,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* La connexion est déclenchée par l'événement WIFI_EVENT_STA_START */

    const esp_timer_create_args_t timer_args = {
        .callback = timer_reconnexion_cb,
        .name = "wifi_reconnexion"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_timer_reconnexion));

    s_role = ROLE_SLAVE;

    /*
     * On attend la connexion 15 secondes maximum.
     * Si le serveur n'est pas encore prêt, on continue sans bloquer :
     * le handler d'événements relancera automatiquement esp_wifi_connect()
     * jusqu'à ce que la connexion soit établie.
     */
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_events,
        WIFI_BIT_CONNECTE,
        pdFALSE,             /* Ne pas effacer les bits après lecture */
        pdFALSE,             /* Un seul bit suffit */
        pdMS_TO_TICKS(15000) /* Attente max 15 secondes */
    );

    if (bits & WIFI_BIT_CONNECTE) {
        ESP_LOGI(TAG, "Connexion au serveur établie avec succès.");
    } else {
        /*
         * Pas encore connecté après 15 secondes.
         * Ce n'est pas forcément une erreur : le serveur démarre peut-être
         * après cette carte. Les reconnexions continuent en arrière-plan.
         */
        ESP_LOGW(TAG, "Serveur non joignable immédiatement. "
                      "Les tentatives de reconnexion continuent en arrière-plan.");
    }

    return ESP_OK;  /* Toujours OK, la reconnexion est gérée par le handler */
}

/* ====================================================================
 * FONCTIONS PUBLIQUES
 * ==================================================================== */

esp_err_t wifi_initialiser(role_reseau_t *role_out, bool est_serveur)
{
    /*
     * Remarque : le NVS est déjà initialisé dans app_initialiser()
     * avant l'appel à cette fonction.
     */

    /* Initialiser la pile réseau TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Créer la boucle d'événements système (WiFi, IP, etc.) */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /*
     * Créer les deux interfaces réseau.
     * On les crée toujours même si on n'utilise qu'une seule.
     * C'est la pratique recommandée par ESP-IDF pour éviter des
     * erreurs si le mode change en cours de vie.
     */
    s_netif_ap  = esp_netif_create_default_wifi_ap();
    s_netif_sta = esp_netif_create_default_wifi_sta();

    /* Créer le groupe d'événements pour synchroniser l'attente de connexion */
    s_wifi_events = xEventGroupCreate();

    /* Enregistrer notre handler pour les événements WiFi et IP */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,    &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,   IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    esp_err_t ret;

    if (est_serveur) {
        /*
         * CARTE SERVEUR : on démarre directement en Point d'Accès.
         * Pas de scan réseau, pas d'attente. Le rôle est connu à l'avance.
         */
        ret = wifi_demarrer_ap();
    } else {
        /*
         * CARTE RELAIS : on se connecte au réseau de la carte serveur.
         * Le rôle est toujours SLAVE, définitivement.
         */
        ret = wifi_demarrer_sta();
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
