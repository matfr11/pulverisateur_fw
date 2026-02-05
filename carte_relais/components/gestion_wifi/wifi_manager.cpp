/**
 * @file wifi_manager.cpp
 * @brief Gestionnaire WiFi principal - Fonctions communes
 * @version 1.0
 * @date 2026-02-02
 */

#include "wifi_manager.h"
#include "board_config.h"
#include "app_role.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"

static const char* TAG = LOG_TAG_WIFI;

// État d'initialisation
static bool g_wifi_initialized = false;

// Déclarations des fonctions internes
extern esp_err_t wifi_register_event_handlers(void);
extern esp_err_t wifi_unregister_event_handlers(void);
extern bool wifi_sta_est_connecte(void);
extern int8_t wifi_sta_get_rssi(void);

// ============================================================================
// FONCTIONS PUBLIQUES
// ============================================================================
extern "C" {
/**
 * @brief Initialise le WiFi selon le rôle
 * Fonction appelée depuis app_init.cpp
 */
esp_err_t app_init_wifi(role_carte_t role) {
    ESP_LOGI(TAG, "Initialisation WiFi (rôle: %s)", 
             role == ROLE_MASTER ? "MASTER" : "SLAVE");
    
    if (g_wifi_initialized) {
        ESP_LOGW(TAG, "WiFi déjà initialisé");
        return ESP_OK;
    }
    
    // Initialiser netif
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Erreur init netif: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Créer event loop par défaut
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Erreur création event loop: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Enregistrer les handlers d'événements
    ret = wifi_register_event_handlers();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Démarrer selon le rôle
    if (role == ROLE_MASTER) {
        ret = wifi_start_ap();
    } else {
        ret = wifi_start_sta();
    }
    
    if (ret == ESP_OK) {
        g_wifi_initialized = true;
    }
    
    return ret;
}

/**
 * @brief Vérifie si le WiFi est connecté
 */
bool app_wifi_est_connecte(void) {
    role_carte_t role = app_role_get_role_actuel();
    
    if (role == ROLE_MASTER) {
        // En mode AP, on est toujours "connecté" dès que l'AP est actif
        return g_wifi_initialized;
    } else {
        // En mode Station, vérifier la connexion réelle
        return wifi_sta_est_connecte();
    }
}

/**
 * @brief Obtient le RSSI actuel
 */
int8_t app_wifi_get_rssi(void) {
    role_carte_t role = app_role_get_role_actuel();
    
    if (role == ROLE_MASTER) {
        // En mode AP, pas de RSSI
        return 0;
    } else {
        return wifi_sta_get_rssi();
    }
}
} // extern "C"
// ============================================================================
// CALLBACKS PAR DÉFAUT (weak)
// ============================================================================

/**
 * @brief Callback par défaut: WiFi connecté
 * Peut être surchargée par l'application
 */
__attribute__((weak)) void app_wifi_on_connected(void) {
    ESP_LOGI(TAG, "Callback: WiFi connecté (implémentation par défaut)");
    
    // Notifier app_role si on est SLAVE
    role_carte_t role = app_role_get_role_actuel();
    if (role == ROLE_SLAVE) {
        // Démarrer la surveillance du master
        app_role_surveiller_master();
    }
}

/**
 * @brief Callback par défaut: WiFi déconnecté
 */
__attribute__((weak)) void app_wifi_on_disconnected(void) {
    ESP_LOGW(TAG, "Callback: WiFi déconnecté (implémentation par défaut)");
    
    // Notifier app_role
    app_role_on_wifi_disconnected();
}

/**
 * @brief Callback par défaut: Station connectée à notre AP
 */
__attribute__((weak)) void app_wifi_on_sta_connected(uint8_t* mac) {
    ESP_LOGI(TAG, "Callback: Station connectée %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * @brief Callback par défaut: Station déconnectée de notre AP
 */
__attribute__((weak)) void app_wifi_on_sta_disconnected(uint8_t* mac) {
    ESP_LOGI(TAG, "Callback: Station déconnectée %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
