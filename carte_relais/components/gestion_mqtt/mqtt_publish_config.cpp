/**
 * @file mqtt_publish_config.cpp
 * @brief Publication configuration système via MQTT
 * @version 1.0
 * @date 2026-02-05
 */

#include "mqtt_manager.h"
#include "mqtt_topics.h"
#include "types_communs.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char* TAG = "MQTT_PUB_CONFIG";

/**
 * @brief Publie la configuration système complète via MQTT
 * 
 * Format JSON:
 * {
 *   "version": 1,
 *   "securite": {
 *     "seuil_debit_cuve_vide": 0.5,
 *     "delai_detection_ms": 5000,
 *     "timeout_vanne_3fils_ms": 30000
 *   },
 *   "automatismes": {
 *     "volume_transfert_litres": 100.0,
 *     "temps_brassage_on_sec": 300,
 *     "temps_brassage_pause_sec": 600
 *   },
 *   "capteurs": {
 *     "facteur_k_debitmetre": 4.72,
 *     "sonde_niveau_disponible": false
 *   }
 * }
 */
void mqtt_publier_configuration(const configuration_systeme_t* config) {
    if (config == NULL) {
        ESP_LOGE(TAG, "Config NULL");
        return;
    }
    
    ESP_LOGI(TAG, "Publication configuration (version %lu)", config->version_config);
    
    // Créer objet JSON
    cJSON* root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Erreur création JSON");
        return;
    }
    
    // Version
    cJSON_AddNumberToObject(root, "version", config->version_config);
    
    // Sécurités
    cJSON* securite = cJSON_CreateObject();
    cJSON_AddNumberToObject(securite, "seuil_debit_cuve_vide", 
                            config->securite.seuil_debit_cuve_vide);
    cJSON_AddNumberToObject(securite, "delai_detection_ms", 
                            config->securite.delai_detection_ms);
    cJSON_AddNumberToObject(securite, "timeout_vanne_3fils_ms", 
                            config->securite.timeout_vanne_3fils_ms);
    cJSON_AddItemToObject(root, "securite", securite);
    
    // Automatismes
    cJSON* automatismes = cJSON_CreateObject();
    cJSON_AddNumberToObject(automatismes, "volume_transfert_litres", 
                            config->automatismes.volume_transfert_litres);
    cJSON_AddNumberToObject(automatismes, "temps_brassage_on_sec", 
                            config->automatismes.temps_brassage_on_sec);
    cJSON_AddNumberToObject(automatismes, "temps_brassage_pause_sec", 
                            config->automatismes.temps_brassage_pause_sec);
    cJSON_AddNumberToObject(automatismes, "seuil_niveau_bas_litres", 
                            config->automatismes.seuil_niveau_bas_litres);
    cJSON_AddNumberToObject(automatismes, "volume_cible_transfert_litres", 
                            config->automatismes.volume_cible_transfert_litres);
    cJSON_AddItemToObject(root, "automatismes", automatismes);
    
    // Capteurs
    cJSON* capteurs = cJSON_CreateObject();
    cJSON_AddNumberToObject(capteurs, "facteur_k_debitmetre", 
                            config->capteurs.facteur_k_debitmetre);
    cJSON_AddBoolToObject(capteurs, "sonde_niveau_disponible", 
                          config->capteurs.sonde_niveau_disponible);
    cJSON_AddItemToObject(root, "capteurs", capteurs);
    
    // Convertir en string
    char* json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) {
        ESP_LOGE(TAG, "Erreur conversion JSON");
        return;
    }
    
    // Publier avec retain
    extern esp_err_t mqtt_publish(const char* topic, const char* payload, int qos, bool retain);
    esp_err_t ret = mqtt_publish(TOPIC_CONFIG_INSTANTANE, json_str, 1, true);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Configuration publiée (%d bytes)", strlen(json_str));
    } else {
        ESP_LOGE(TAG, "Erreur publication: %s", esp_err_to_name(ret));
    }
    
    free(json_str);
}

/**
 * @brief Demande la configuration au master via MQTT
 * Publie sur le topic de demande
 */
void app_mqtt_demander_configuration(void) {
    ESP_LOGI(TAG, "Demande configuration au master");
    
    extern esp_err_t mqtt_publish(const char* topic, const char* payload, int qos, bool retain);
    mqtt_publish(TOPIC_CONFIG_DEMANDE, "{\"request\":\"config\"}", 1, false);
}