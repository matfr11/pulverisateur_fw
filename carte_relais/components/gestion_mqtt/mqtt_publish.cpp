/**
 * @file mqtt_publish.cpp
 * @brief Fonctions de publication MQTT
 */

#include "mqtt_manager.h"
#include "mqtt_topics.h"
#include "board_config.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_timer.h"
#include "mqtt_client.h"

#ifdef HAVE_HTTP_SERVER
#include "http_server.h"
#endif

static const char* TAG = LOG_TAG_MQTT;

extern esp_mqtt_client_handle_t g_mqtt_client;

// ============================================================================
// PUBLICATION MQTT GÉNÉRIQUE
// ============================================================================

/**
 * @brief Publication MQTT générique
 */
esp_err_t mqtt_publish(const char* topic, const char* payload, int qos, bool retain) {
    if (!topic || !payload) {
        ESP_LOGE(TAG, "Topic ou payload NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_mqtt_client) {
        ESP_LOGE(TAG, "Client MQTT non initialisé");
        return ESP_FAIL;
    }
    
    int msg_id = esp_mqtt_client_publish(g_mqtt_client, topic, payload, 0, qos, retain ? 1 : 0);
    
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Erreur publication MQTT sur %s", topic);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Publication MQTT OK: %s (msg_id=%d)", topic, msg_id);
    return ESP_OK;
}

// ============================================================================
// PUBLICATIONS CAPTEURS
// ============================================================================

/**
 * @brief Publie les données du débitmètre
 */
void mqtt_publier_debitmetre(float debit, float volume) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "debit_lpm", debit);
    cJSON_AddNumberToObject(json, "volume_total_l", volume);
    
    char* payload = cJSON_PrintUnformatted(json);
    mqtt_publish(TOPIC_CAPTEUR_DEBITMETRE, payload, MQTT_QOS_ETAT, false);
    
#ifdef HAVE_HTTP_SERVER
    http_status_update_debit(debit, volume);
#endif
    
    cJSON_Delete(json);
    free(payload);
}

// ============================================================================
// PUBLICATIONS ACTIONNEURS
// ============================================================================

/**
 * @brief Publie l'état d'un actionneur
 */
esp_err_t mqtt_publier_etat_actionneur(const char* topic, int etat) {
    if (!topic) return ESP_ERR_INVALID_ARG;
    
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"etat\":%d}", etat);
    
    return mqtt_publish(topic, payload, MQTT_QOS_ETAT, true);
}

// ============================================================================
// PUBLICATIONS SYSTÈME
// ============================================================================

/**
 * @brief Publie la présence de la carte
 */
void app_mqtt_publier_presence(bool en_ligne) {
    const char* topic = 
#ifdef BOARD_TYPE_AVANT
    TOPIC_PRESENCE_AVANT;
#elif defined(BOARD_TYPE_ARRIERE)
    TOPIC_PRESENCE_ARRIERE;
#else
    TOPIC_PRESENCE_ECRAN;
#endif

    cJSON* json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "online", en_ligne);
    
    char* payload = cJSON_PrintUnformatted(json);
    mqtt_publish(topic, payload, MQTT_QOS_ETAT, true);
    
    cJSON_Delete(json);
    free(payload);
}

/**
 * @brief Demande la configuration au master
 */
void app_mqtt_demander_configuration(void) {
    ESP_LOGI(TAG, "Demande configuration au master");
    mqtt_publish(TOPIC_CONFIG_DEMANDE, "{\"request\":\"config\"}", 0, false);
}

/**
 * @brief Publie l'état complet du système
 */
void app_mqtt_publier_etat_systeme(const etat_complet_systeme_t* etat) {
    if (!etat) return;
    
    ESP_LOGD(TAG, "Publication état système");
    
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "uptime", etat->uptime_ms);
    cJSON_AddBoolToObject(json, "wifi_ok", etat->diagnostics.wifi_connecte);
    cJSON_AddBoolToObject(json, "mqtt_ok", etat->diagnostics.mqtt_connecte);
    
    char* payload = cJSON_PrintUnformatted(json);
    
#ifdef BOARD_TYPE_AVANT
    mqtt_publish(TOPIC_PRESENCE_AVANT, payload, MQTT_QOS_ETAT, false);
#elif defined(BOARD_TYPE_ARRIERE)
    mqtt_publish(TOPIC_PRESENCE_ARRIERE, payload, MQTT_QOS_ETAT, false);
#else
    mqtt_publish(TOPIC_PRESENCE_ECRAN, payload, MQTT_QOS_ETAT, false);
#endif
    
    cJSON_Delete(json);
    free(payload);
}