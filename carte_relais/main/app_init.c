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
#include "gestion_actionneurs.h"
#include "gestion_capteurs.h"
#include "gestion_configuration.h"

#if A_DEBITMETRE
#include "gestion_automatismes.h"
#include "gestion_securites.h"
#endif

#if A_WEB_UI
#include "gestion_web_ui.h"
#endif

#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "APP";

/* ====================================================================
 * VARIABLES GLOBALES
 * ==================================================================== */
static role_reseau_t        s_role = ROLE_INDEFINI;
static etat_systeme_t       s_etat_systeme = ETAT_SYS_INITIALISATION;
static configuration_t      s_config = CONFIG_DEFAUT;
static uint32_t             s_compteur_publication = 0;

/* Intervalle de publication d'état (en ticks de la tâche à 100ms) */
#define INTERVALLE_PUBLICATION  10  /* = 1 seconde */

/* ====================================================================
 * CALLBACKS MQTT
 * ==================================================================== */

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
            automatismes_transfert_activer(&s_config);
        } else {
            automatismes_transfert_arreter();
        }
    } else if (strcmp(cmd, CMD_STR_AUTO_BRASSAGE) == 0) {
        if (val && strcmp(val, CMD_VAL_ACTIVER) == 0) {
            automatismes_brassage_activer(&s_config);
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

/** Callback de réception de configuration MQTT */
static void on_configuration_recue(const configuration_t *config)
{
    ESP_LOGI(TAG, "Configuration reçue (version %lu)", (unsigned long)config->version);

    /* Si version plus récente, appliquer */
    if (config->version > s_config.version) {
        s_config = *config;
        configuration_sauvegarder(&s_config);

        /* Si MASTER, rediffuser */
        if (s_role == ROLE_MASTER) {
            mqtt_publier_configuration(&s_config);
        }

#if A_DEBITMETRE
        /* Mettre à jour le facteur K du débitmètre */
        capteurs_debitmetre_set_facteur_k(s_config.facteur_k_debitmetre);
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
#else
             "CARTE ARRIERE"
#endif
    );
    ESP_LOGI(TAG, " Firmware %s | Protocole v%d", VERSION_FIRMWARE, VERSION_PROTOCOLE);
#ifdef MODE_SIMULATION
    ESP_LOGW(TAG, " *** MODE SIMULATION ACTIF ***");
#endif
    ESP_LOGI(TAG, "========================================");

    /* 1. Charger la configuration depuis NVS */
    configuration_initialiser();
    if (!configuration_charger(&s_config)) {
        ESP_LOGW(TAG, "Pas de config NVS, utilisation des valeurs par défaut.");
        s_config = (configuration_t)CONFIG_DEFAUT;
    }

    /* 2. Initialiser les actionneurs (GPIO) */
    actionneurs_initialiser();

    /* 3. Initialiser les capteurs */
    capteurs_initialiser();

    /* 4. Démarrer WiFi et déterminer le rôle */
    wifi_initialiser(&s_role);
    ESP_LOGI(TAG, "Rôle réseau : %s", s_role == ROLE_MASTER ? "MASTER" : "SLAVE");

    /* 4b. Si MASTER, démarrer le broker MQTT embarqué */
    if (s_role == ROLE_MASTER) {
        broker_mqtt_config_t broker_cfg = { .port = 1883 };
        esp_err_t err_broker = broker_mqtt_demarrer(&broker_cfg);
        if (err_broker != ESP_OK) {
            ESP_LOGE(TAG, "Échec démarrage broker MQTT !");
        } else {
            ESP_LOGI(TAG, "Broker MQTT embarqué actif.");
            /* Laisser le broker s'initialiser avant de connecter le client */
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

    /* 5. Connexion MQTT (client) */
    mqtt_initialiser(MQTT_BROKER_URI_MASTER, ID_CARTE);
    mqtt_enregistrer_callback_commande(on_commande_recue);
    mqtt_enregistrer_callback_config(on_configuration_recue);

#if A_WEB_UI
    /* Si MASTER, enregistrer le callback d'état AVANT la connexion
     * pour que la souscription se fasse dans le handler CONNECTED */
    if (s_role == ROLE_MASTER) {
        mqtt_enregistrer_callback_etat(on_etat_recu);
    }
#endif

    /* Attendre la connexion MQTT (max 10s) */
    for (int i = 0; i < 100 && !mqtt_est_connecte(); i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (mqtt_est_connecte()) {
        /* 6. Si MASTER, publier la configuration de référence */
        if (s_role == ROLE_MASTER) {
            mqtt_publier_configuration(&s_config);
        } else {
            /* SLAVE : demander la configuration */
            mqtt_demander_configuration();
        }
        s_etat_systeme = ETAT_SYS_OPERATIONNEL;
    } else {
        ESP_LOGW(TAG, "MQTT non connecté. Mode dégradé.");
        s_etat_systeme = ETAT_SYS_DEGRADE;
    }

#if A_WEB_UI
    /* 7. Démarrer le serveur Web UI (seulement si MASTER) */
    if (s_role == ROLE_MASTER) {
        web_ui_demarrer(&s_config);
        ESP_LOGI(TAG, "Web UI démarrée (MASTER).");
    } else {
        ESP_LOGI(TAG, "Web UI non démarrée (SLAVE).");
    }
#endif

#if A_DEBITMETRE
    /* 8. Initialiser les automatismes et sécurités */
    automatismes_initialiser();
    securites_initialiser();
#endif

    /* 9. Lancer la tâche principale */
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
        securites_update(pompe_active, debit, &s_config);

        /* Si cuve vide → arrêter automatismes */
        if (securites_cuve_est_vide()) {
            automatismes_arreter_tout();
        }

        /* --- Automatismes --- */
        automatismes_update(debit, capteurs_debitmetre_get_volume_session());
#endif

#ifdef CARTE_ARRIERE
        /* --- Timeout vannes motorisées --- */
        actionneurs_vannes_update_timeout(s_config.timeout_vanne_ms);
#endif

        /* --- Publication MQTT périodique --- */
        s_compteur_publication++;
        if (s_compteur_publication >= INTERVALLE_PUBLICATION) {
            s_compteur_publication = 0;

            /* Détecter failover : si on est devenu MASTER et le broker n'est pas actif */
            if (wifi_obtenir_role() == ROLE_MASTER && !broker_mqtt_est_actif()) {
                ESP_LOGW(TAG, "Failover détecté : démarrage du broker MQTT...");
                broker_mqtt_config_t broker_cfg = { .port = 1883 };
                broker_mqtt_demarrer(&broker_cfg);
                s_role = ROLE_MASTER;
                vTaskDelay(pdMS_TO_TICKS(500));
#if A_WEB_UI
                /* Enregistrer le callback d'état AVANT de reconnecter le client MQTT */
                mqtt_enregistrer_callback_etat(on_etat_recu);
#endif
                /* Reconnecter le client MQTT au broker local */
                mqtt_initialiser(MQTT_BROKER_URI_MASTER, ID_CARTE);
#if A_WEB_UI
                /* Démarrer la Web UI qui n'était pas active en tant que SLAVE */
                web_ui_demarrer(&s_config);
                ESP_LOGI(TAG, "Web UI démarrée après failover.");
#endif
            }

            if (mqtt_est_connecte()) {
#ifdef CARTE_AVANT
                etat_carte_avant_t etat_av = {
                    .pompe = actionneurs_pompe_est_active() ? POMPE_EN_MARCHE : POMPE_ARRETEE,
                    .vanne_3v = actionneurs_v3v_est_transfert() ? V3V_TRANSFERT : V3V_BRASSAGE,
                    .phares_avant = actionneurs_phares_avant_actifs(),
                    .debit_instantane = capteurs_debitmetre_get_debit(),
                    .volume_session = capteurs_debitmetre_get_volume_session(),
                    .debitmetre_ok = capteurs_debitmetre_est_ok(),
                    .auto_transfert = automatismes_get_etat_transfert(),
                    .auto_brassage = automatismes_get_etat_brassage(),
                    .transfert_volume_cible = s_config.volume_transfert,
                    .securite_cuve = securites_get_etat_cuve(),
                    .etat_systeme = s_etat_systeme,
                };
                automatismes_get_brassage_info(
                    etat_av.brassage_label, sizeof(etat_av.brassage_label),
                    &etat_av.brassage_temps_restant,
                    &etat_av.brassage_pourcentage);

                mqtt_publier_etat_avant(&etat_av);
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
#endif
            }
        }

        /* Délai 100ms non-bloquant */
        vTaskDelayUntil(&dernier_reveil, pdMS_TO_TICKS(100));
    }
}
