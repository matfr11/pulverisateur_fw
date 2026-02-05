/**
 * @file configuration_manager.cpp
 * @brief Gestionnaire principal configuration
 * @version 1.1
 * @date 2026-02-05
 */

#include "configuration.h"
#include "board_config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char* TAG = "CONFIG";

// Configuration globale en RAM
static configuration_systeme_t g_config_actuelle;
static SemaphoreHandle_t g_mutex_config = NULL;

// ============================================================================
// INITIALISATION
// ============================================================================

esp_err_t configuration_init(void) {
    ESP_LOGI(TAG, "Initialisation configuration...");
    
    // Créer mutex
    g_mutex_config = xSemaphoreCreateMutex();
    if (g_mutex_config == NULL) {
        ESP_LOGE(TAG, "Erreur création mutex");
        return ESP_FAIL;
    }
    
    // Charger depuis NVS ou utiliser valeurs par défaut
    if (configuration_charger(&g_config_actuelle) != ESP_OK) {
        ESP_LOGI(TAG, "Pas de configuration sauvegardée, utilisation valeurs par défaut");
        configuration_charger_defaut(&g_config_actuelle);
        configuration_sauvegarder(&g_config_actuelle);
    }
    
    ESP_LOGI(TAG, "✓ Configuration initialisée (version %lu)", 
             g_config_actuelle.version_config);
    return ESP_OK;
}

// ============================================================================
// CONFIGURATION PAR DÉFAUT
// ============================================================================

void configuration_charger_defaut(configuration_systeme_t* config) {
    if (config == NULL) return;
    
    memset(config, 0, sizeof(configuration_systeme_t));
    
    // Version
    config->version_config = 1;
    
    // Sécurités
    config->securite.seuil_debit_cuve_vide = 0.5f;          // 0.5 L/min
    config->securite.delai_detection_ms = 5000;             // 5 secondes
    config->securite.timeout_vanne_3fils_ms = 30000;        // 30 secondes
    
    // Automatismes
    config->automatismes.volume_transfert_litres = 100.0f;  // 100L par défaut
    config->automatismes.temps_brassage_on_sec = 300;       // 5 min
    config->automatismes.temps_brassage_pause_sec = 600;    // 10 min
    config->automatismes.seuil_niveau_bas_litres = 50.0f;   // 50L
    config->automatismes.volume_cible_transfert_litres = 150.0f; // 150L
    
    // Capteurs
    config->capteurs.facteur_k_debitmetre = 4.72f;          // Impulsions/litre
    config->capteurs.sonde_niveau_disponible = false;       // Pas de sonde pour l'instant
    
    ESP_LOGI(TAG, "Configuration par défaut chargée");
}

// ============================================================================
// NVS
// ============================================================================

esp_err_t configuration_charger(configuration_systeme_t* config) {
    extern esp_err_t configuration_charger_nvs(configuration_systeme_t*);
    return configuration_charger_nvs(config);
}

esp_err_t configuration_sauvegarder(const configuration_systeme_t* config) {
    extern esp_err_t configuration_sauvegarder_nvs(const configuration_systeme_t*);
    return configuration_sauvegarder_nvs(config);
}

// ============================================================================
// ACCESSEURS
// ============================================================================

esp_err_t configuration_get(configuration_systeme_t* config) {
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    
    if (xSemaphoreTake(g_mutex_config, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(config, &g_config_actuelle, sizeof(configuration_systeme_t));
    xSemaphoreGive(g_mutex_config);
    return ESP_OK;
}

const configuration_systeme_t* configuration_get_ptr(void) {
    // Retourne un pointeur constant (lecture seule)
    // Plus rapide que configuration_get() mais pas de copie thread-safe
    // À utiliser uniquement pour lecture rapide
    return &g_config_actuelle;
}

esp_err_t configuration_set(const configuration_systeme_t* config) {
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    
    if (xSemaphoreTake(g_mutex_config, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Incrémenter version
    configuration_systeme_t nouvelle_config;
    memcpy(&nouvelle_config, config, sizeof(configuration_systeme_t));
    nouvelle_config.version_config = g_config_actuelle.version_config + 1;
    
    // Sauvegarder
    memcpy(&g_config_actuelle, &nouvelle_config, sizeof(configuration_systeme_t));
    esp_err_t ret = configuration_sauvegarder(&g_config_actuelle);
    
    xSemaphoreGive(g_mutex_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration mise à jour (version %lu)", 
                 g_config_actuelle.version_config);
    }
    
    return ret;
}

uint32_t configuration_get_version(void) {
    return g_config_actuelle.version_config;
}