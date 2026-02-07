/**
 * @file mqtt_ecran.c
 * @brief Client MQTT pour la carte écran.
 *
 * Se connecte au broker PULVE_AP (192.168.4.1:1883),
 * souscrit aux topics d'état et publie les commandes.
 */
#include "mqtt_ecran.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "MQTT_ECR";

#define BROKER_URI      "mqtt://192.168.4.1:1883"
#define TOPIC_ETAT_AV   "pulverisateur/etat/avant"
#define TOPIC_ETAT_AR   "pulverisateur/etat/arriere"
#define TOPIC_CMD_AV    "pulverisateur/cmd/avant"
#define TOPIC_CMD_AR    "pulverisateur/cmd/arriere"
#define TOPIC_URGENCE   "pulverisateur/arret_urgence"
#define LINK_TIMEOUT_MS 5000

static esp_mqtt_client_handle_t s_client = NULL;
static SemaphoreHandle_t        s_mutex  = NULL;
static etat_systeme_t           s_etat   = {0};
static int64_t                  s_ts_avant  = 0;
static int64_t                  s_ts_arriere = 0;

/* ====================================================================
 * DÉSÉRIALISATION JSON
 * ==================================================================== */

static void parse_etat_avant(const char *json_str, int len)
{
    cJSON *j = cJSON_ParseWithLength(json_str, len);
    if (!j) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    cJSON *item;
    item = cJSON_GetObjectItem(j, "p");
    if (item) s_etat.avant.pompe = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(j, "v");
    if (item) s_etat.avant.vanne_transfert = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(j, "l");
    if (item) s_etat.avant.phares_avant = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(j, "av_ok");
    if (item) s_etat.avant.debitmetre_ok = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(j, "av_flow");
    if (item) s_etat.avant.debit_lpm = (float)item->valuedouble;

    item = cJSON_GetObjectItem(j, "vol_s");
    if (item) s_etat.avant.volume_session = (float)item->valuedouble;

    item = cJSON_GetObjectItem(j, "sec_cv");
    if (item) s_etat.avant.cuve_vide = (item->valueint == 1);

    item = cJSON_GetObjectItem(j, "a_tr");
    if (item) s_etat.avant.auto_transfert = (item->valueint == 1);

    item = cJSON_GetObjectItem(j, "tr_cib");
    if (item) s_etat.avant.transfert_cible = (uint32_t)item->valueint;

    item = cJSON_GetObjectItem(j, "a_br");
    if (item) s_etat.avant.auto_brassage = (item->valueint != 0);

    item = cJSON_GetObjectItem(j, "br_lbl");
    if (item && cJSON_IsString(item)) {
        strncpy(s_etat.avant.brassage_label, item->valuestring,
                sizeof(s_etat.avant.brassage_label) - 1);
    }

    item = cJSON_GetObjectItem(j, "br_rem");
    if (item) s_etat.avant.brassage_temps_restant = (float)item->valuedouble;

    item = cJSON_GetObjectItem(j, "br_pct");
    if (item) s_etat.avant.brassage_pourcentage = (float)item->valuedouble;

    s_ts_avant = esp_timer_get_time() / 1000;

    xSemaphoreGive(s_mutex);
    cJSON_Delete(j);
}

static void parse_etat_arriere(const char *json_str, int len)
{
    cJSON *j = cJSON_ParseWithLength(json_str, len);
    if (!j) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    cJSON *item;
    item = cJSON_GetObjectItem(j, "v2m");
    if (item && cJSON_IsString(item)) s_etat.arriere.vanne_2m = item->valuestring[0];

    item = cJSON_GetObjectItem(j, "vbt");
    if (item && cJSON_IsString(item)) s_etat.arriere.vanne_bdr = item->valuestring[0];

    item = cJSON_GetObjectItem(j, "li");
    if (item) s_etat.arriere.phares_arriere = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(j, "niv");
    if (item) s_etat.arriere.niveau_ar = (float)item->valuedouble;

    item = cJSON_GetObjectItem(j, "niv_max");
    if (item) s_etat.arriere.niveau_ar_max = (uint32_t)item->valueint;

    item = cJSON_GetObjectItem(j, "sonde_ok");
    if (item) s_etat.arriere.sonde_ok = cJSON_IsTrue(item);

    s_ts_arriere = esp_timer_get_time() / 1000;

    xSemaphoreGive(s_mutex);
    cJSON_Delete(j);
}

/* ====================================================================
 * HANDLER MQTT
 * ==================================================================== */

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connecté au broker");
        esp_mqtt_client_subscribe(s_client, TOPIC_ETAT_AV, 0);
        esp_mqtt_client_subscribe(s_client, TOPIC_ETAT_AR, 0);
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_etat.connecte = true;
        xSemaphoreGive(s_mutex);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT déconnecté");
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_etat.connecte = false;
        xSemaphoreGive(s_mutex);
        break;

    case MQTT_EVENT_DATA:
        if (event->topic_len > 0 && event->data_len > 0) {
            if (strncmp(event->topic, TOPIC_ETAT_AV, event->topic_len) == 0) {
                parse_etat_avant(event->data, event->data_len);
            } else if (strncmp(event->topic, TOPIC_ETAT_AR, event->topic_len) == 0) {
                parse_etat_arriere(event->data, event->data_len);
            }
        }
        break;

    default:
        break;
    }
}

/* ====================================================================
 * API PUBLIQUE
 * ==================================================================== */

void mqtt_ecran_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    s_etat.arriere.vanne_2m = '?';
    s_etat.arriere.vanne_bdr = '?';
    strcpy(s_etat.master, "?");

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = BROKER_URI,
        .network.reconnect_timeout_ms = 2000,
    };

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                    mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
    ESP_LOGI(TAG, "Client MQTT démarré → %s", BROKER_URI);
}

void mqtt_ecran_get_etat(etat_systeme_t *etat_out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* Mettre à jour les flags de link */
    int64_t now = esp_timer_get_time() / 1000;
    s_etat.link_avant  = (s_ts_avant > 0)  && ((now - s_ts_avant) < LINK_TIMEOUT_MS);
    s_etat.link_arriere = (s_ts_arriere > 0) && ((now - s_ts_arriere) < LINK_TIMEOUT_MS);

    *etat_out = s_etat;
    xSemaphoreGive(s_mutex);
}

static void publier_cmd(const char *topic, const char *cmd, const char *val)
{
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "cmd", cmd);
    if (val) {
        cJSON_AddStringToObject(j, "val", val);
    }
    char *str = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);

    if (str && s_client) {
        esp_mqtt_client_publish(s_client, topic, str, 0, 0, 0);
        ESP_LOGD(TAG, "CMD → %s : %s", topic, str);
        free(str);
    }
}

void mqtt_ecran_cmd_avant(const char *cmd, const char *val)
{
    publier_cmd(TOPIC_CMD_AV, cmd, val);
}

void mqtt_ecran_cmd_arriere(const char *cmd, const char *val)
{
    publier_cmd(TOPIC_CMD_AR, cmd, val);
}

void mqtt_ecran_arret_urgence(void)
{
    publier_cmd(TOPIC_URGENCE, "stop", NULL);
}
