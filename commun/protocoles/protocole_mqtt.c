/**
 * @file protocole_mqtt.c
 * @brief Client MQTT avec sérialisation JSON cJSON.
 */
#include "protocole_mqtt.h"
#include "mqtt_topics.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "MQTT";

/* ====================================================================
 * VARIABLES INTERNES
 * ==================================================================== */
static esp_mqtt_client_handle_t s_client = NULL;
static bool                     s_connecte = false;
static carte_id_t               s_carte_id = CARTE_ID_AVANT;

static mqtt_callback_commande_t s_cb_commande = NULL;
static mqtt_callback_config_t   s_cb_config = NULL;
static mqtt_callback_etat_t     s_cb_etat = NULL;

/* Buffer pour la sérialisation JSON */
#define JSON_BUFFER_SIZE    1024
static char s_json_buffer[JSON_BUFFER_SIZE];

/* ====================================================================
 * HANDLER ÉVÉNEMENTS MQTT
 * ==================================================================== */
static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t evt = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connecté au broker MQTT.");
        s_connecte = true;

        /* Souscriptions selon la carte */
        if (s_carte_id == CARTE_ID_AVANT) {
            esp_mqtt_client_subscribe(s_client, TOPIC_SUB_CMD_AVANT, 1);
        } else if (s_carte_id == CARTE_ID_ARRIERE) {
            esp_mqtt_client_subscribe(s_client, TOPIC_SUB_CMD_ARRIERE, 1);
        }

        /* Toutes les cartes souscrivent à la configuration et à l'arrêt d'urgence */
        esp_mqtt_client_subscribe(s_client, TOPIC_SUB_CONFIG_ALL, 1);
        esp_mqtt_client_subscribe(s_client, TOPIC_SUB_CMD_URGENCE, 1);

        /* Si un callback d'état est enregistré (cas MASTER avec Web UI),
         * souscrire aux états de toutes les cartes */
        if (s_cb_etat) {
            esp_mqtt_client_subscribe(s_client, TOPIC_SUB_ETAT_ALL, 0);
            ESP_LOGI(TAG, "Souscrit aux états (Web UI active).");
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Déconnecté du broker MQTT.");
        s_connecte = false;
        break;

    case MQTT_EVENT_DATA: {
        /* Extraire topic et payload */
        char topic[128] = {0};
        int topic_len = (evt->topic_len < 127) ? evt->topic_len : 127;
        strncpy(topic, evt->topic, topic_len);

        char payload[512] = {0};
        int data_len = (evt->data_len < 511) ? evt->data_len : 511;
        strncpy(payload, evt->data, data_len);

        ESP_LOGD(TAG, "Reçu [%s] : %s", topic, payload);

        /* Router vers le bon callback */
        if (strstr(topic, "/cmd/") && s_cb_commande) {
            s_cb_commande(topic, payload, data_len);
        } else if (strstr(topic, "/etat/") && s_cb_etat) {
            s_cb_etat(topic, payload, data_len);
        } else if (strstr(topic, "/configuration/") && s_cb_config) {
            /* Tenter de désérialiser la configuration */
            if (strstr(topic, "instantane") || strstr(topic, "mise_a_jour")) {
                configuration_t cfg;
                if (json_deserialiser_configuration(payload, &cfg)) {
                    s_cb_config(&cfg);
                }
            }
        } else if (strstr(topic, "arret_urgence") && s_cb_commande) {
            s_cb_commande(topic, payload, data_len);
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Erreur MQTT !");
        break;

    default:
        break;
    }
}

/* ====================================================================
 * FONCTIONS PUBLIQUES - INITIALISATION
 * ==================================================================== */
esp_err_t mqtt_initialiser(const char *broker_uri, carte_id_t id_carte)
{
    s_carte_id = id_carte;

    char client_id[32];
    snprintf(client_id, sizeof(client_id), "pulve_carte_%02x", id_carte);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .credentials.client_id = client_id,
        .session.keepalive = 15,
        .network.reconnect_timeout_ms = 3000,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Échec initialisation client MQTT.");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(s_client);
}

void mqtt_enregistrer_callback_commande(mqtt_callback_commande_t cb)
{
    s_cb_commande = cb;
}

void mqtt_enregistrer_callback_config(mqtt_callback_config_t cb)
{
    s_cb_config = cb;
}

void mqtt_enregistrer_callback_etat(mqtt_callback_etat_t cb)
{
    s_cb_etat = cb;
}

void mqtt_souscrire_etats(void)
{
    if (s_client && s_connecte) {
        esp_mqtt_client_subscribe(s_client, TOPIC_SUB_ETAT_ALL, 0);
        ESP_LOGI(TAG, "Souscription aux états activée.");
    }
}

bool mqtt_est_connecte(void)
{
    return s_connecte;
}

/* ====================================================================
 * PUBLICATION DES ÉTATS
 * ==================================================================== */
esp_err_t mqtt_publier_etat_avant(const etat_carte_avant_t *etat)
{
    int len = json_serialiser_etat_avant(etat, s_json_buffer, JSON_BUFFER_SIZE);
    if (len < 0) return ESP_FAIL;

    int msg_id = esp_mqtt_client_publish(s_client, TOPIC_ETAT_AVANT,
                                          s_json_buffer, len, 0, 1 /* retain */);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publier_etat_arriere(const etat_carte_arriere_t *etat)
{
    int len = json_serialiser_etat_arriere(etat, s_json_buffer, JSON_BUFFER_SIZE);
    if (len < 0) return ESP_FAIL;

    int msg_id = esp_mqtt_client_publish(s_client, TOPIC_ETAT_ARRIERE,
                                          s_json_buffer, len, 0, 1 /* retain */);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

/* ====================================================================
 * PUBLICATION DES COMMANDES
 * ==================================================================== */
static esp_err_t mqtt_publier_commande(const char *topic, const char *cmd_str, const char *val_str)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, JSON_CMD_KEY, cmd_str);
    if (val_str) {
        cJSON_AddStringToObject(json, JSON_CMD_VAL, val_str);
    }
    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (!str) return ESP_FAIL;

    int msg_id = esp_mqtt_client_publish(s_client, topic, str, strlen(str), 1, 0);
    free(str);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publier_commande_avant(const char *cmd_str, const char *val_str)
{
    return mqtt_publier_commande(TOPIC_CMD_AVANT, cmd_str, val_str);
}

esp_err_t mqtt_publier_commande_arriere(const char *cmd_str, const char *val_str)
{
    return mqtt_publier_commande(TOPIC_CMD_ARRIERE, cmd_str, val_str);
}

esp_err_t mqtt_publier_arret_urgence(void)
{
    return mqtt_publier_commande(TOPIC_CMD_ARRET_URGENCE, "stop", NULL);
}

/* ====================================================================
 * CONFIGURATION
 * ==================================================================== */
esp_err_t mqtt_publier_configuration(const configuration_t *config)
{
    int len = json_serialiser_configuration(config, s_json_buffer, JSON_BUFFER_SIZE);
    if (len < 0) return ESP_FAIL;

    int msg_id = esp_mqtt_client_publish(s_client, TOPIC_CONFIG_INSTANTANE,
                                          s_json_buffer, len, 1, 1 /* retain */);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_demander_configuration(void)
{
    const char *payload = "{\"demande\":true}";
    int msg_id = esp_mqtt_client_publish(s_client, TOPIC_CONFIG_DEMANDE,
                                          payload, strlen(payload), 1, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publier_mise_a_jour_config(const configuration_t *config)
{
    int len = json_serialiser_configuration(config, s_json_buffer, JSON_BUFFER_SIZE);
    if (len < 0) return ESP_FAIL;

    int msg_id = esp_mqtt_client_publish(s_client, TOPIC_CONFIG_MISE_A_JOUR,
                                          s_json_buffer, len, 1, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

/* ====================================================================
 * SÉRIALISATION JSON
 * ==================================================================== */
int json_serialiser_etat_avant(const etat_carte_avant_t *etat, char *buffer, size_t taille)
{
    cJSON *json = cJSON_CreateObject();

    /* Actionneurs */
    cJSON_AddBoolToObject(json, "p", etat->pompe == POMPE_EN_MARCHE);
    cJSON_AddBoolToObject(json, "v", etat->vanne_3v == V3V_TRANSFERT);
    cJSON_AddBoolToObject(json, "l", etat->phares_avant);

    /* Capteurs */
    cJSON_AddBoolToObject(json, "av_ok", etat->debitmetre_ok);
    cJSON_AddNumberToObject(json, "av_flow", etat->debit_instantane);
    cJSON_AddNumberToObject(json, "av_vide", etat->securite_cuve == SEC_CUVE_VIDE);
    cJSON_AddNumberToObject(json, "session_vol", etat->volume_session);

    /* Automatismes */
    cJSON_AddBoolToObject(json, "m_tr", etat->auto_transfert == AUTO_TR_EN_COURS);
    cJSON_AddNumberToObject(json, "tr_target", etat->transfert_volume_cible);
    cJSON_AddBoolToObject(json, "m_br", etat->auto_brassage != AUTO_BR_INACTIF);
    cJSON_AddStringToObject(json, "br_label", etat->brassage_label);
    cJSON_AddNumberToObject(json, "br_rem", etat->brassage_temps_restant);
    cJSON_AddNumberToObject(json, "br_pct", etat->brassage_pourcentage);

    /* Système */
    cJSON_AddNumberToObject(json, "sys", etat->etat_systeme);

    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (!str) return -1;

    int len = snprintf(buffer, taille, "%s", str);
    free(str);
    return len;
}

int json_serialiser_etat_arriere(const etat_carte_arriere_t *etat, char *buffer, size_t taille)
{
    cJSON *json = cJSON_CreateObject();

    /* Vannes - encoder comme caractères pour compatibilité Web UI */
    const char *etat_vanne_str[] = {"?", "O", "F", "S", "T"};
    cJSON_AddStringToObject(json, "v2m",
        etat_vanne_str[(int)etat->vanne_2m < 5 ? (int)etat->vanne_2m : 0]);
    cJSON_AddStringToObject(json, "vbt",
        etat_vanne_str[(int)etat->vanne_bout_rampe < 5 ? (int)etat->vanne_bout_rampe : 0]);

    cJSON_AddBoolToObject(json, "li", etat->phares_arriere);
    cJSON_AddNumberToObject(json, "niveau_ar", etat->niveau_cuve_arriere);
    cJSON_AddBoolToObject(json, "sonde_ok", etat->sonde_niveau_ok);
    cJSON_AddNumberToObject(json, "sys", etat->etat_systeme);

    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (!str) return -1;

    int len = snprintf(buffer, taille, "%s", str);
    free(str);
    return len;
}

int json_serialiser_configuration(const configuration_t *config, char *buffer, size_t taille)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", config->version);

    cJSON *sec = cJSON_AddObjectToObject(root, "securite");
    cJSON_AddNumberToObject(sec, "seuil_debit_cuve_vide", config->seuil_debit_cuve_vide);
    cJSON_AddNumberToObject(sec, "delai_detection", config->delai_detection_ms);

    cJSON *autom = cJSON_AddObjectToObject(root, "automatismes");
    cJSON_AddNumberToObject(autom, "volume_transfert", config->volume_transfert);
    cJSON_AddNumberToObject(autom, "temps_brassage_on", config->temps_brassage_on);
    cJSON_AddNumberToObject(autom, "temps_brassage_pause", config->temps_brassage_off);

    cJSON *capt = cJSON_AddObjectToObject(root, "capteurs");
    cJSON_AddNumberToObject(capt, "facteur_k_debitmetre", config->facteur_k_debitmetre);

    cJSON *act = cJSON_AddObjectToObject(root, "actionneurs");
    cJSON_AddNumberToObject(act, "timeout_vanne", config->timeout_vanne_ms);

    cJSON *cuve = cJSON_AddObjectToObject(root, "cuve_ar");
    cJSON_AddNumberToObject(cuve, "volume_total", config->volume_cuve_ar);
    cJSON_AddNumberToObject(cuve, "hauteur_max", config->sonde_hauteur_max_mm);
    cJSON_AddNumberToObject(cuve, "offset", config->sonde_offset_mm);
    cJSON_AddNumberToObject(cuve, "hauteur_cuve", config->hauteur_cuve_mm);

    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!str) return -1;

    int len = snprintf(buffer, taille, "%s", str);
    free(str);
    return len;
}

bool json_deserialiser_configuration(const char *json_str, configuration_t *config_out)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Erreur parsing JSON configuration.");
        return false;
    }

    cJSON *item;

    item = cJSON_GetObjectItem(root, "version");
    if (item) config_out->version = item->valueint;

    cJSON *sec = cJSON_GetObjectItem(root, "securite");
    if (sec) {
        item = cJSON_GetObjectItem(sec, "seuil_debit_cuve_vide");
        if (item) config_out->seuil_debit_cuve_vide = (float)item->valuedouble;
        item = cJSON_GetObjectItem(sec, "delai_detection");
        if (item) config_out->delai_detection_ms = item->valueint;
    }

    cJSON *autom = cJSON_GetObjectItem(root, "automatismes");
    if (autom) {
        item = cJSON_GetObjectItem(autom, "volume_transfert");
        if (item) config_out->volume_transfert = item->valueint;
        item = cJSON_GetObjectItem(autom, "temps_brassage_on");
        if (item) config_out->temps_brassage_on = item->valueint;
        item = cJSON_GetObjectItem(autom, "temps_brassage_pause");
        if (item) config_out->temps_brassage_off = item->valueint;
    }

    cJSON *capt = cJSON_GetObjectItem(root, "capteurs");
    if (capt) {
        item = cJSON_GetObjectItem(capt, "facteur_k_debitmetre");
        if (item) config_out->facteur_k_debitmetre = (float)item->valuedouble;
    }

    cJSON *act = cJSON_GetObjectItem(root, "actionneurs");
    if (act) {
        item = cJSON_GetObjectItem(act, "timeout_vanne");
        if (item) config_out->timeout_vanne_ms = item->valueint;
    }

    cJSON *cuve = cJSON_GetObjectItem(root, "cuve_ar");
    if (cuve) {
        item = cJSON_GetObjectItem(cuve, "volume_total");
        if (item) config_out->volume_cuve_ar = item->valueint;
        item = cJSON_GetObjectItem(cuve, "hauteur_max");
        if (item) config_out->sonde_hauteur_max_mm = item->valueint;
        item = cJSON_GetObjectItem(cuve, "offset");
        if (item) config_out->sonde_offset_mm = item->valueint;
        item = cJSON_GetObjectItem(cuve, "hauteur_cuve");
        if (item) config_out->hauteur_cuve_mm = item->valueint;
    }
    config_out->version_protocole = VERSION_PROTOCOLE;

    cJSON_Delete(root);
    return true;
}

bool json_deserialiser_etat_avant(const char *json_str, etat_carte_avant_t *etat_out)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) return false;

    cJSON *item;
    memset(etat_out, 0, sizeof(*etat_out));

    item = cJSON_GetObjectItem(root, "p");
    if (item) etat_out->pompe = cJSON_IsTrue(item) ? POMPE_EN_MARCHE : POMPE_ARRETEE;

    item = cJSON_GetObjectItem(root, "v");
    if (item) etat_out->vanne_3v = cJSON_IsTrue(item) ? V3V_TRANSFERT : V3V_BRASSAGE;

    item = cJSON_GetObjectItem(root, "l");
    if (item) etat_out->phares_avant = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(root, "av_ok");
    if (item) etat_out->debitmetre_ok = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(root, "av_flow");
    if (item) etat_out->debit_instantane = (float)item->valuedouble;

    item = cJSON_GetObjectItem(root, "av_vide");
    if (item) etat_out->securite_cuve = (cJSON_IsTrue(item) || item->valueint) ? SEC_CUVE_VIDE : SEC_CUVE_OK;

    item = cJSON_GetObjectItem(root, "session_vol");
    if (item) etat_out->volume_session = (float)item->valuedouble;

    item = cJSON_GetObjectItem(root, "m_tr");
    if (item) etat_out->auto_transfert = cJSON_IsTrue(item) ? AUTO_TR_EN_COURS : AUTO_TR_INACTIF;

    item = cJSON_GetObjectItem(root, "tr_target");
    if (item) etat_out->transfert_volume_cible = (uint32_t)item->valueint;

    item = cJSON_GetObjectItem(root, "m_br");
    if (item) etat_out->auto_brassage = cJSON_IsTrue(item) ? AUTO_BR_MARCHE : AUTO_BR_INACTIF;

    item = cJSON_GetObjectItem(root, "br_label");
    if (item && item->valuestring) {
        strncpy(etat_out->brassage_label, item->valuestring, sizeof(etat_out->brassage_label) - 1);
    }

    item = cJSON_GetObjectItem(root, "br_rem");
    if (item) etat_out->brassage_temps_restant = (float)item->valuedouble;

    item = cJSON_GetObjectItem(root, "br_pct");
    if (item) etat_out->brassage_pourcentage = (uint8_t)item->valueint;

    item = cJSON_GetObjectItem(root, "sys");
    if (item) etat_out->etat_systeme = (etat_systeme_t)item->valueint;

    cJSON_Delete(root);
    return true;
}

bool json_deserialiser_etat_arriere(const char *json_str, etat_carte_arriere_t *etat_out)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) return false;

    cJSON *item;
    memset(etat_out, 0, sizeof(*etat_out));

    /* Vannes : décodage depuis caractère "O"/"F"/"S"/"T" */
    item = cJSON_GetObjectItem(root, "v2m");
    if (item && item->valuestring) {
        switch (item->valuestring[0]) {
            case 'O': etat_out->vanne_2m = VANNE_ETAT_EN_OUVERTURE; break;
            case 'F': etat_out->vanne_2m = VANNE_ETAT_EN_FERMETURE; break;
            case 'S': etat_out->vanne_2m = VANNE_ETAT_ARRETEE; break;
            case 'T': etat_out->vanne_2m = VANNE_ETAT_TIMEOUT; break;
            default:  etat_out->vanne_2m = VANNE_ETAT_ARRETEE; break;
        }
    }

    item = cJSON_GetObjectItem(root, "vbt");
    if (item && item->valuestring) {
        switch (item->valuestring[0]) {
            case 'O': etat_out->vanne_bout_rampe = VANNE_ETAT_EN_OUVERTURE; break;
            case 'F': etat_out->vanne_bout_rampe = VANNE_ETAT_EN_FERMETURE; break;
            case 'S': etat_out->vanne_bout_rampe = VANNE_ETAT_ARRETEE; break;
            case 'T': etat_out->vanne_bout_rampe = VANNE_ETAT_TIMEOUT; break;
            default:  etat_out->vanne_bout_rampe = VANNE_ETAT_ARRETEE; break;
        }
    }

    item = cJSON_GetObjectItem(root, "li");
    if (item) etat_out->phares_arriere = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(root, "niveau_ar");
    if (item) etat_out->niveau_cuve_arriere = (float)item->valuedouble;

    item = cJSON_GetObjectItem(root, "sonde_ok");
    if (item) etat_out->sonde_niveau_ok = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(root, "sys");
    if (item) etat_out->etat_systeme = (etat_systeme_t)item->valueint;

    cJSON_Delete(root);
    return true;
}
