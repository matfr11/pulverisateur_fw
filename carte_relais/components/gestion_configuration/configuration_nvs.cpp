/**
 * @file configuration_nvs.cpp
 * @brief Chargement/sauvegarde configuration NVS
 * @version 1.0
 */

#include "configuration.h"
#include "board_config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char* TAG = "CONFIG_NVS";

esp_err_t configuration_charger_nvs(configuration_systeme_t* config) {
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_SYSTEME, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ret; // Pas de config sauvegardée
    }
    
    // Charger la structure complète
    size_t required_size = sizeof(configuration_systeme_t);
    ret = nvs_get_blob(handle, "config", config, &required_size);
    
    nvs_close(handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration chargée depuis NVS (version %lu)", 
                 config->version_config);
    }
    
    return ret;
}

esp_err_t configuration_sauvegarder_nvs(const configuration_systeme_t* config) {
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_SYSTEME, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur ouverture NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Sauvegarder structure complète
    ret = nvs_set_blob(handle, "config", config, sizeof(configuration_systeme_t));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    
    nvs_close(handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration sauvegardée (version %lu)", config->version_config);
    } else {
        ESP_LOGE(TAG, "Erreur sauvegarde: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t configuration_effacer(void) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_SYSTEME, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = nvs_erase_key(handle, "config");
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Configuration effacée");
    return ret;
}
