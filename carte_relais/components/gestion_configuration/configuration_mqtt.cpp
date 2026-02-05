/**
 * @file configuration_mqtt.cpp  
 * @brief Synchronisation configuration via MQTT
 * @version 1.0
 */

#include "configuration.h"
#include "mqtt_manager.h"
#include "mqtt_topics.h"
#include "board_config.h"
#include "esp_log.h"
#include "cJSON.h"

static const char* TAG = "CONFIG_MQTT";

void configuration_demander_au_master(void) {
    ESP_LOGI(TAG, "Demande configuration au master...");
    app_mqtt_demander_configuration();
}

void configuration_publier(const configuration_systeme_t* config) {
    if (config == NULL) return;
    extern void mqtt_publier_configuration(const configuration_systeme_t*);
    mqtt_publier_configuration(config);
}

esp_err_t configuration_appliquer_mqtt(const char* config_json) {
    if (config_json == NULL) return ESP_ERR_INVALID_ARG;
    
    // Parser JSON et appliquer
    // Implémenté via callback mqtt_callback_configuration
    ESP_LOGI(TAG, "Application configuration MQTT");
    return ESP_OK;
}
