/**
 * @file app_init.c
 * @brief Initialisation séquentielle et tâche principale périodique.
 */
#include "app_init.h"
#include "board_config.h"
#include "protocole_wifi.h"
#include "protocole_mqtt.h"
#include "broker_mqtt.h"
#include "mqtt_topics.h"
#include "gestion_configuration.h"
#include "nvs_flash.h"

/*
 * La carte serveur n'a pas de relais ni de capteurs physiques.
 * On n'inclut ces modules que pour les cartes relais.
 */
#if !A_EST_SERVEUR
#include "gestion_actionneurs.h"
#include "gestion_capteurs.h"
#endif

#if A_DEBITMETRE
#include "gestion_automatismes.h"
#include "gestion_securites.h"
#endif

#if A_WEB_UI
#include "gestion_web_ui.h"
#endif

#include "gestion_ota.h"
#include "mdns.h"

#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "APP";

/* ====================================================================
 * VARIABLES GLOBALES
 * ==================================================================== */
static role_reseau_t        s_role = ROLE_INDEFINI;
static etat_systeme_t       s_etat_systeme = ETAT_SYS_INITIALISATION;
static configuration_t      s_config = CONFIG_DEFAUT;
static SemaphoreHandle_t    s_mutex_config = NULL;
static uint32_t             s_compteur_publication = 0;

/* Intervalle de publication d'état (en ticks de la tâche à 100ms) */
#define INTERVALLE_PUBLICATION  3  /* = 1 seconde */

/* ====================================================================
 * CALLBACKS MQTT
 * ==================================================================== */

/*
 * Le serveur envoie des commandes aux cartes relais, il n'en reçoit pas
 * (il n'a pas d'actionneurs). Ce callback n'est compilé que pour les relais.
 */
#if !A_EST_SERVEUR

/** Callback de réception de commandes MQTT */
static void on_commande_recue(const char *topic, const char *payload, int len)
{
    ESP_LOGI(TAG, "Commande reçue [%s]: %.*s", topic, len, payload);

    /* Parser le JSON de la commande */
    cJSON *json = cJSON_Parse(payload);
    if (!json) {
        ESP_LOGW(TAG, "JSON invalide dans la commande.");
        return;
    }

    cJSON *cmd_item = cJSON_GetObjectItem(json, JSON_CMD_KEY);
    cJSON *val_item = cJSON_GetObjectItem(json, JSON_CMD_VAL);

    if (!cmd_item || !cJSON_IsString(cmd_item)) {
        cJSON_Delete(json);
        return;
    }

    const char *cmd = cmd_item->valuestring;
    const char *val = (val_item && cJSON_IsString(val_item)) ? val_item->valuestring : NULL;

    /* Arrêt d'urgence global */
    if (strstr(topic, "arret_urgence")) {
        ESP_LOGW(TAG, ">>> ARRÊT D'URGENCE REÇU <<<");
#if A_DEBITMETRE
        automatismes_arreter_tout();
#endif
        actionneurs_tout_arreter();
        cJSON_Delete(json);
        return;
    }

#ifdef CARTE_AVANT
    /* Commandes carte AVANT */
    if (strcmp(cmd, CMD_STR_POMPE_TOGGLE) == 0) {
        actionneurs_pompe_toggle();
    } else if (strcmp(cmd, CMD_STR_V3V_TOGGLE) == 0) {
        actionneurs_v3v_toggle();
    } else if (strcmp(cmd, CMD_STR_PHARES_AV_TOGGLE) == 0) {
        actionneurs_phares_avant_toggle();
    } else if (strcmp(cmd, CMD_STR_AUTO_TRANSFERT) == 0) {
        if (val && strcmp(val, CMD_VAL_ACTIVER) == 0) {
            configuration_t cfg;
            xSemaphoreTake(s_mutex_config, portMAX_DELAY);
            cfg = s_config;
            xSemaphoreGive(s_mutex_config);
            automatismes_transfert_activer(&cfg);
        } else {
            automatismes_transfert_arreter();
        }
    } else if (strcmp(cmd, CMD_STR_AUTO_BRASSAGE) == 0) {
        if (val && strcmp(val, CMD_VAL_ACTIVER) == 0) {
            configuration_t cfg;
            xSemaphoreTake(s_mutex_config, portMAX_DELAY);
            cfg = s_config;
            xSemaphoreGive(s_mutex_config);
            automatismes_brassage_activer(&cfg);
        } else {
            automatismes_brassage_arreter();
        }
    }
#endif

#ifdef CARTE_ARRIERE
    /* Commandes carte ARRIÈRE */
    if (strcmp(cmd, CMD_STR_V2M_OUVRIR) == 0) {
        actionneurs_vanne_commander(VANNE_ID_2M, VANNE_OUVRE);
    } else if (strcmp(cmd, CMD_STR_V2M_FERMER) == 0) {
        actionneurs_vanne_commander(VANNE_ID_2M, VANNE_FERME);
    } else if (strcmp(cmd, CMD_STR_V2M_STOP) == 0) {
        actionneurs_vanne_commander(VANNE_ID_2M, VANNE_STOP);
    } else if (strcmp(cmd, CMD_STR_VBR_OUVRIR) == 0) {
        actionneurs_vanne_commander(VANNE_ID_BDR, VANNE_OUVRE);
    } else if (strcmp(cmd, CMD_STR_VBR_FERMER) == 0) {
        actionneurs_vanne_commander(VANNE_ID_BDR, VANNE_FERME);
    } else if (strcmp(cmd, CMD_STR_VBR_STOP) == 0) {
        actionneurs_vanne_commander(VANNE_ID_BDR, VANNE_STOP);
    } else if (strcmp(cmd, CMD_STR_PHARES_AR_TOGGLE) == 0) {
        actionneurs_phares_arriere_toggle();
    }
#endif

    cJSON_Delete(json);
}

#endif /* !A_EST_SERVEUR */

/**
 * Callback déclenché à chaque réception d'un message MQTT de configuration.
 *
 * Ce callback est appelé sur TOUTES les cartes (serveur et relais).
 *
 * Sur la carte serveur :
 *   → Ce callback est déclenché quand le broker envoie un message retain
 *     au démarrage (lors de la souscription à "configuration/#").
 *   → La config Web UI est gérée directement dans handler_save_config
 *     (gestion_web_ui.c) qui appelle mqtt_publier_configuration() lui-même.
 *
 * Sur une carte relais :
 *   → La config vient du serveur (retain au démarrage ou mise à jour en direct).
 *   → On l'applique, on la sauvegarde en NVS local (cache de secours),
 *     et si on était en attente de config, on passe en mode OPÉRATIONNEL.
 */
static void on_configuration_recue(const configuration_t *config)
{
    ESP_LOGI(TAG, "Configuration reçue (version %lu)", (unsigned long)config->version);

    /* On n'applique la config que si elle est plus récente que celle qu'on a */
    configuration_t config_locale;
    bool plus_recente;

    xSemaphoreTake(s_mutex_config, portMAX_DELAY);
    plus_recente = (config->version >= s_config.version);
    if (plus_recente) {
        s_config = *config;
    }
    config_locale = s_config;
    xSemaphoreGive(s_mutex_config);

    if (plus_recente) {

        /* Sauvegarder en mémoire non-volatile */
        configuration_sauvegarder(&config_locale);

#if A_EST_SERVEUR
        mqtt_publier_configuration(&config_locale);
        ESP_LOGI(TAG, "Config retain republié (version %lu).", (unsigned long)config_locale.version);
#endif

#if A_DEBITMETRE
        capteurs_debitmetre_set_facteur_k(config_locale.facteur_k_debitmetre);
#endif
#if A_SONDE_NIVEAU
        capteurs_sonde_set_config(
            config_locale.sonde_hauteur_max_mm,
            config_locale.sonde_offset_mm,
            config_locale.hauteur_cuve_mm
        );
#endif

#if !A_EST_SERVEUR
        if (s_etat_systeme == ETAT_SYS_SYNCHRONISATION) {
            s_etat_systeme = ETAT_SYS_OPERATIONNEL;
            ESP_LOGI(TAG, "Config reçue du serveur → système OPÉRATIONNEL.");
        }
#endif
    }
}

#if A_WEB_UI
/** Callback de réception des états MQTT → alimente la Web UI */
static void on_etat_recu(const char *topic, const char *payload, int len)
{
    if (strstr(topic, "/etat/avant")) {
        etat_carte_avant_t etat;
        if (json_deserialiser_etat_avant(payload, &etat)) {
            web_ui_update_etat_avant(&etat);
        }
    } else if (strstr(topic, "/etat/arriere")) {
        etat_carte_arriere_t etat;
        if (json_deserialiser_etat_arriere(payload, &etat)) {
            web_ui_update_etat_arriere(&etat);
        }
    }
}
#endif

/* ====================================================================
 * INITIALISATION
 * ==================================================================== */
void app_initialiser(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " PULVERISATEUR AGRICOLE - %s",
#ifdef CARTE_AVANT
             "CARTE AVANT"
#elif defined(CARTE_ARRIERE)
             "CARTE ARRIERE"
#else
             "CARTE SERVEUR"
#endif
    );
    ESP_LOGI(TAG, " Firmware %s | Protocole v%d", VERSION_FIRMWARE, VERSION_PROTOCOLE);
#ifdef MODE_SIMULATION
    ESP_LOGW(TAG, " *** MODE SIMULATION ACTIF ***");
#endif
    ESP_LOGI(TAG, "========================================");

    s_mutex_config = xSemaphoreCreateMutex();

    /* 0. Initialiser NVS (nécessaire pour config ET WiFi) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 1. Charger la configuration depuis NVS */
    configuration_initialiser();
    if (!configuration_charger(&s_config)) {
        ESP_LOGW(TAG, "Pas de config NVS, utilisation des valeurs par défaut.");
        s_config = (configuration_t)CONFIG_DEFAUT;
    }

    /*
     * 2 & 3. Initialiser les actionneurs (relais GPIO) et les capteurs.
     * La carte serveur n'a aucun matériel physique : on saute cette étape.
     *
     * Note : le délai de 4 secondes pour CARTE_ARRIERE n'est plus nécessaire.
     * Dans l'ancienne architecture, la carte ARRIERE attendait que AVANT ait eu
     * le temps de scanner le réseau et de devenir MASTER. Avec la carte serveur
     * dédiée, les rôles sont fixes : cette attente n'a plus de sens.
     */
#if !A_EST_SERVEUR
    actionneurs_initialiser();
    capteurs_initialiser();
#endif

    #if A_DEBITMETRE
    capteurs_debitmetre_set_facteur_k(s_config.facteur_k_debitmetre);
    #endif

    #if A_SONDE_NIVEAU
    capteurs_sonde_set_config(
        s_config.sonde_hauteur_max_mm,
        s_config.sonde_offset_mm,
        s_config.hauteur_cuve_mm
    );
    #endif

    /*
     * 4. Démarrer le WiFi.
     * A_EST_SERVEUR est défini dans board_config.h :
     *   - true (1) pour CARTE_SERVEUR → démarre en Point d'Accès (AP)
     *   - false (0) pour CARTE_AVANT/ARRIERE → se connecte en client (STA)
     * Le rôle est désormais fixe, il n'y a plus de négociation dynamique.
     */
    wifi_initialiser(&s_role, A_EST_SERVEUR);
    ESP_LOGI(TAG, "Rôle réseau : %s", s_role == ROLE_MASTER ? "SERVEUR (MASTER)" : "RELAIS (SLAVE)");

    /* 4b. mDNS : chaque carte annonce son hostname sur le réseau local */
    mdns_init();
    mdns_hostname_set(
#ifdef CARTE_AVANT
        OTA_HOSTNAME_AVANT
#elif defined(CARTE_ARRIERE)
        OTA_HOSTNAME_ARRIERE
#else
        OTA_HOSTNAME_SERVEUR
#endif
    );
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    /*
     * 4b. Démarrer le broker MQTT embarqué (carte serveur uniquement).
     *
     * Le broker MQTT est le "hub" de communication : toutes les cartes
     * (y compris le serveur lui-même) s'y connectent comme clients.
     * Il doit être démarré avant la connexion du client MQTT local.
     */
#if A_EST_SERVEUR
    {
        broker_mqtt_config_t broker_cfg = { .port = MQTT_BROKER_PORT };
        esp_err_t err_broker = broker_mqtt_demarrer(&broker_cfg);
        if (err_broker != ESP_OK) {
            ESP_LOGE(TAG, "Échec démarrage broker MQTT !");
        } else {
            ESP_LOGI(TAG, "Broker MQTT démarré sur le port %d.", MQTT_BROKER_PORT);
            /* Petit délai pour que le broker soit prêt à accepter des connexions */
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
#endif

    /* 5. Démarrer le client MQTT et enregistrer les callbacks */
    mqtt_initialiser(MQTT_BROKER_URI_MASTER, ID_CARTE);

    /* Les cartes relais reçoivent et exécutent des commandes (pompe, vannes...) */
#if !A_EST_SERVEUR
    mqtt_enregistrer_callback_commande(on_commande_recue);
#endif

    /* Toutes les cartes reçoivent les mises à jour de configuration */
    mqtt_enregistrer_callback_config(on_configuration_recue);

    /*
     * Le serveur reçoit les états MQTT des deux cartes relais
     * pour alimenter sa Web UI en temps réel.
     */
#if A_EST_SERVEUR
    mqtt_enregistrer_callback_etat(on_etat_recu);
#endif

    /* Attendre la connexion MQTT (max 10s) */
    for (int i = 0; i < 100 && !mqtt_est_connecte(); i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (mqtt_est_connecte()) {

#if A_EST_SERVEUR
        /*
         * 6a. CARTE SERVEUR : publier la configuration de référence avec retain=true.
         *
         * Toutes les cartes relais qui se connecteront (maintenant ou plus tard)
         * recevront automatiquement ce message retenu et appliqueront la config.
         * C'est le mécanisme qui remplace la demande explicite de config.
         */
        mqtt_publier_configuration(&s_config);
        ESP_LOGI(TAG, "Configuration de référence publiée (version %lu).",
                 (unsigned long)s_config.version);
        s_etat_systeme = ETAT_SYS_OPERATIONNEL;

#else
        /*
         * 6b. CARTE RELAIS : envoyer une demande de configuration au serveur.
         *
         * On passe en SYNCHRONISATION : on attend que le serveur nous envoie
         * sa configuration avant de se considérer pleinement opérationnel.
         * La transition vers OPERATIONNEL se fait dans on_configuration_recue().
         *
         * Note : même si la carte relais a une config en NVS (d'un démarrage
         * précédent), on attend la config du serveur pour s'assurer d'avoir
         * la version la plus récente.
         */
        mqtt_demander_configuration();
        s_etat_systeme = ETAT_SYS_SYNCHRONISATION;
        ESP_LOGI(TAG, "Demande de config envoyée. En attente de réponse du serveur...");
#endif

    } else {

#if A_EST_SERVEUR
        /* Le broker local devrait toujours être joignable : c'est une erreur grave */
        ESP_LOGE(TAG, "Impossible de se connecter au broker MQTT local !");
        s_etat_systeme = ETAT_SYS_DEGRADE;
#else
        /*
         * Le serveur n'est pas encore joignable (normal si démarrage simultané).
         * On reste en SYNCHRONISATION : la connexion MQTT se fera automatiquement
         * quand le serveur sera disponible, et la config arrivera ensuite.
         */
        ESP_LOGW(TAG, "Serveur non joignable au démarrage. Synchronisation en attente...");
        s_etat_systeme = ETAT_SYS_SYNCHRONISATION;
#endif
    }

    /*
     * 7. Démarrer la Web UI (carte serveur uniquement).
     *
     * A_WEB_UI = 1 uniquement pour CARTE_SERVEUR (défini dans board_config.h).
     * Pour les cartes relais, ce bloc est entièrement ignoré à la compilation.
     */
#if A_WEB_UI
    web_ui_demarrer(&s_config);
    web_ui_set_carte_master(ID_CARTE);
    ESP_LOGI(TAG, "Interface Web démarrée → http://192.168.4.1");
#endif

#if A_DEBITMETRE
    /* 8. Initialiser les automatismes et sécurités */
    automatismes_initialiser();
    securites_initialiser();
#endif

    /* 9. Démarrer le serveur OTA (cartes relais uniquement) */
#if !A_EST_SERVEUR
    ota_serveur_demarrer();
#endif

    /* 10. Lancer la tâche principale */
    xTaskCreate(app_tache_principale, "tache_princ", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Initialisation terminée. Système %s.",
             s_etat_systeme == ETAT_SYS_OPERATIONNEL ? "OPÉRATIONNEL" : "DÉGRADÉ");
}

/* ====================================================================
 * TÂCHE PRINCIPALE (100ms)
 * ==================================================================== */
void app_tache_principale(void *pvParameters)
{
    TickType_t dernier_reveil = xTaskGetTickCount();

    while (1) {
        configuration_t config;
        xSemaphoreTake(s_mutex_config, portMAX_DELAY);
        config = s_config;
        xSemaphoreGive(s_mutex_config);

        /* --- Lecture des capteurs --- */
#if A_DEBITMETRE
        capteurs_debitmetre_update();
#endif
#if A_SONDE_NIVEAU
        capteurs_sonde_niveau_update();
#endif

#ifdef CARTE_AVANT
        /* --- Sécurités (cuve vide) --- */
        bool pompe_active = actionneurs_pompe_est_active();
        float debit = capteurs_debitmetre_get_debit();
        securites_update(pompe_active, debit, &config);

        /* Si cuve vide → arrêter automatismes */
        if (securites_cuve_est_vide()) {
            automatismes_arreter_tout();
        }

        /* --- Automatismes --- */
        automatismes_update(debit, capteurs_debitmetre_get_volume_session());
#endif

#ifdef CARTE_ARRIERE
        /* --- Timeout vannes motorisées --- */
        actionneurs_vannes_update_timeout(config.timeout_vanne_ms);
#endif

        /* --- Publication MQTT périodique (toutes les INTERVALLE_PUBLICATION × 100ms) --- */
        s_compteur_publication++;
        if (s_compteur_publication >= INTERVALLE_PUBLICATION) {
            s_compteur_publication = 0;

            if (mqtt_est_connecte()) {
#ifdef CARTE_AVANT
                etat_carte_avant_t etat_av = {
                    .pompe = actionneurs_pompe_est_active() ? POMPE_EN_MARCHE : POMPE_ARRETEE,
                    .vanne_3v = actionneurs_v3v_est_transfert() ? V3V_TRANSFERT : V3V_BRASSAGE,
                    .phares_avant = actionneurs_phares_avant_actifs(),
                    .debit_instantane = capteurs_debitmetre_get_debit(),
                    .volume_session = automatismes_get_volume_transfere(),
                    .debitmetre_ok = capteurs_debitmetre_est_ok(),
                    .auto_transfert = automatismes_get_etat_transfert(),
                    .auto_brassage = automatismes_get_etat_brassage(),
                    .transfert_volume_cible = config.volume_transfert,
                    .securite_cuve = securites_get_etat_cuve(),
                    .etat_systeme = s_etat_systeme,
                };
                automatismes_get_brassage_info(
                    etat_av.brassage_label, sizeof(etat_av.brassage_label),
                    &etat_av.brassage_temps_restant,
                    &etat_av.brassage_pourcentage);

                mqtt_publier_etat_avant(&etat_av);
                /* A_WEB_UI = 0 pour CARTE_AVANT → ce bloc est ignoré à la compilation */
                #if A_WEB_UI
                    web_ui_update_etat_avant(&etat_av);
                #endif
#endif

#ifdef CARTE_ARRIERE
                etat_carte_arriere_t etat_ar = {
                    .vanne_2m = actionneurs_vanne_get_etat(VANNE_ID_2M),
                    .vanne_bout_rampe = actionneurs_vanne_get_etat(VANNE_ID_BDR),
                    .phares_arriere = actionneurs_phares_arriere_actifs(),
                    .niveau_cuve_arriere = capteurs_sonde_get_niveau(),
                    .sonde_niveau_ok = capteurs_sonde_est_ok(),
                    .etat_systeme = s_etat_systeme,
                };
                mqtt_publier_etat_arriere(&etat_ar);
                /* A_WEB_UI = 0 pour CARTE_ARRIERE → ce bloc est ignoré à la compilation */
                #if A_WEB_UI
                    web_ui_update_etat_arriere(&etat_ar);
                #endif
#endif
            }
        }

        /* Délai 100ms non-bloquant */
        vTaskDelayUntil(&dernier_reveil, pdMS_TO_TICKS(100));
    }
}
