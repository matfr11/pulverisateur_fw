/**
 * @file wifi_ap.cpp
 * @brief Implémentation WiFi Access Point (mode Master)
 * @version 1.0
 * @date 2026-02-02
 */

#include "wifi_manager.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include <string.h>

static const char* TAG = LOG_TAG_WIFI;

// Netif pour AP
static esp_netif_t* g_netif_ap = NULL;

// Compteur de stations connectées
static uint8_t g_station_count = 0;

// ============================================================================
// FONCTIONS MODE ACCESS POINT
// ============================================================================

/**
 * @brief Démarre le WiFi en mode Access Point
 */
esp_err_t wifi_start_ap(void) {
    ESP_LOGI(TAG, "Démarrage mode Access Point...");
    
    // Créer netif AP si pas déjà fait
    if (g_netif_ap == NULL) {
        g_netif_ap = esp_netif_create_default_wifi_ap();
        if (g_netif_ap == NULL) {
            ESP_LOGE(TAG, "Erreur création netif AP");
            return ESP_FAIL;
        }
    }
    
    // Configuration WiFi par défaut
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_MODE) {
        ESP_LOGE(TAG, "Erreur init WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configuration AP
    wifi_config_t wifi_config = {};
    
    // SSID
    strncpy((char*)wifi_config.ap.ssid, WIFI_AP_SSID, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(WIFI_AP_SSID);
    
    // Password
    strncpy((char*)wifi_config.ap.password, WIFI_AP_PASSWORD, sizeof(wifi_config.ap.password) - 1);
    
    // Paramètres
    wifi_config.ap.channel = WIFI_AP_CHANNEL;
    wifi_config.ap.max_connection = WIFI_AP_MAX_CONNECTIONS;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.ap.beacon_interval = WIFI_AP_BEACON_INTERVAL;
    
    // Si pas de mot de passe, mode ouvert
    if (strlen(WIFI_AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    // Configurer mode AP
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur set mode AP: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config AP: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Démarrer WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Afficher informations
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Access Point créé");
    ESP_LOGI(TAG, "  SSID: %s", WIFI_AP_SSID);
    if (wifi_config.ap.authmode != WIFI_AUTH_OPEN) {
        ESP_LOGI(TAG, "  Auth: WPA2-PSK");
    } else {
        ESP_LOGI(TAG, "  Auth: OUVERT");
    }
    ESP_LOGI(TAG, "  Canal: %d", WIFI_AP_CHANNEL);
    ESP_LOGI(TAG, "  Max clients: %d", WIFI_AP_MAX_CONNECTIONS);
    ESP_LOGI(TAG, "  MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "  IP: 192.168.4.1");
    ESP_LOGI(TAG, "========================================");
    
    return ESP_OK;
}

/**
 * @brief Arrête le mode Access Point
 */
esp_err_t wifi_stop_ap(void) {
    ESP_LOGI(TAG, "Arrêt mode Access Point");
    
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur stop WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur deinit WiFi: %s", esp_err_to_name(ret));
    }
    
    if (g_netif_ap != NULL) {
        esp_netif_destroy(g_netif_ap);
        g_netif_ap = NULL;
    }
    
    g_station_count = 0;
    
    return ESP_OK;
}

/**
 * @brief Retourne le nombre de stations connectées
 */
uint8_t wifi_ap_get_station_count(void) {
    return g_station_count;
}

/**
 * @brief Callback interne: Station connectée
 */
void wifi_ap_on_sta_connect(void) {
    g_station_count++;
    ESP_LOGI(TAG, "Station connectée (total: %d)", g_station_count);
}

/**
 * @brief Callback interne: Station déconnectée
 */
void wifi_ap_on_sta_disconnect(void) {
    if (g_station_count > 0) {
        g_station_count--;
    }
    ESP_LOGI(TAG, "Station déconnectée (total: %d)", g_station_count);
}
