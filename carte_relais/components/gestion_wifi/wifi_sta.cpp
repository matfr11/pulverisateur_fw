/**
 * @file wifi_sta.cpp
 * @brief Implémentation WiFi Station (mode Slave)
 * @version 1.0
 * @date 2026-02-02
 */

#include "wifi_manager.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char* TAG = LOG_TAG_WIFI;

// Netif pour Station
static esp_netif_t* g_netif_sta = NULL;

// Event group pour synchronisation
static EventGroupHandle_t g_wifi_event_group = NULL;
#define BIT_CONNECTE    (1 << 0)
#define BIT_ECHEC       (1 << 1)

// État connexion
static bool g_est_connecte = false;
static int g_tentatives_reconnexion = 0;
static int8_t g_rssi = 0;

// IP obtenue
static char g_ip_address[16] = {0};

// ============================================================================
// DÉCLARATIONS FORWARD
// ============================================================================

extern void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);

// ============================================================================
// FONCTIONS MODE STATION
// ============================================================================

/**
 * @brief Démarre le WiFi en mode Station
 */
esp_err_t wifi_start_sta(void) {
    ESP_LOGI(TAG, "Démarrage mode Station...");
    
    // Créer event group si nécessaire
    if (g_wifi_event_group == NULL) {
        g_wifi_event_group = xEventGroupCreate();
        if (g_wifi_event_group == NULL) {
            ESP_LOGE(TAG, "Erreur création event group");
            return ESP_FAIL;
        }
    }
    
    // Créer netif Station si pas déjà fait
    if (g_netif_sta == NULL) {
        g_netif_sta = esp_netif_create_default_wifi_sta();
        if (g_netif_sta == NULL) {
            ESP_LOGE(TAG, "Erreur création netif Station");
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
    
    // Configuration Station
    wifi_config_t wifi_config = {};
    
    // SSID du master
    strncpy((char*)wifi_config.sta.ssid, WIFI_STA_SSID, sizeof(wifi_config.sta.ssid) - 1);
    
    // Password
    strncpy((char*)wifi_config.sta.password, WIFI_STA_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    
    // Paramètres de connexion
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    // Configurer mode Station
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur set mode STA: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config STA: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Démarrer WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Connexion au master '%s'...", WIFI_STA_SSID);
    
    // Attendre connexion
    EventBits_t bits = xEventGroupWaitBits(
        g_wifi_event_group,
        BIT_CONNECTE | BIT_ECHEC,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(WIFI_TIMEOUT_CONNEXION_MS)
    );
    
    if (bits & BIT_CONNECTE) {
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "  Connecté au master");
        ESP_LOGI(TAG, "  SSID: %s", WIFI_STA_SSID);
        ESP_LOGI(TAG, "  IP: %s", g_ip_address);
        ESP_LOGI(TAG, "  RSSI: %d dBm", g_rssi);
        ESP_LOGI(TAG, "========================================");
        return ESP_OK;
    } else if (bits & BIT_ECHEC) {
        ESP_LOGE(TAG, "Échec connexion au master");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Timeout connexion au master");
        return ESP_ERR_TIMEOUT;
    }
}

/**
 * @brief Arrête le mode Station
 */
esp_err_t wifi_stop_sta(void) {
    ESP_LOGI(TAG, "Arrêt mode Station");
    
    esp_wifi_disconnect();
    
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur stop WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur deinit WiFi: %s", esp_err_to_name(ret));
    }
    
    if (g_netif_sta != NULL) {
        esp_netif_destroy(g_netif_sta);
        g_netif_sta = NULL;
    }
    
    g_est_connecte = false;
    g_ip_address[0] = '\0';
    g_rssi = 0;
    
    return ESP_OK;
}

/**
 * @brief Tente une reconnexion
 */
esp_err_t wifi_reconnect(void) {
    if (g_est_connecte) {
        ESP_LOGI(TAG, "Déjà connecté");
        return ESP_OK;
    }
    
    if (g_tentatives_reconnexion >= WIFI_STA_MAX_RETRY) {
        ESP_LOGE(TAG, "Trop de tentatives de reconnexion (%d)", g_tentatives_reconnexion);
        return ESP_FAIL;
    }
    
    g_tentatives_reconnexion++;
    ESP_LOGI(TAG, "Tentative de reconnexion %d/%d", 
             g_tentatives_reconnexion, WIFI_STA_MAX_RETRY);
    
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur reconnexion: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Attendre résultat
    EventBits_t bits = xEventGroupWaitBits(
        g_wifi_event_group,
        BIT_CONNECTE | BIT_ECHEC,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(WIFI_TIMEOUT_CONNEXION_MS)
    );
    
    if (bits & BIT_CONNECTE) {
        ESP_LOGI(TAG, "Reconnexion réussie");
        g_tentatives_reconnexion = 0;
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

/**
 * @brief Vérifie si connecté
 */
bool wifi_sta_est_connecte(void) {
    return g_est_connecte;
}

/**
 * @brief Obtient l'IP actuelle
 */
esp_err_t wifi_sta_get_ip(char* ip) {
    if (ip == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_est_connecte) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    
    strncpy(ip, g_ip_address, 16);
    return ESP_OK;
}

/**
 * @brief Callback interne: Connexion réussie
 */
void wifi_sta_on_connected(esp_netif_ip_info_t* ip_info) {
    g_est_connecte = true;
    g_tentatives_reconnexion = 0;
    
    snprintf(g_ip_address, sizeof(g_ip_address), IPSTR, IP2STR(&ip_info->ip));
    
    // Obtenir RSSI
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        g_rssi = ap_info.rssi;
    }
    
    xEventGroupSetBits(g_wifi_event_group, BIT_CONNECTE);
    
    // Notifier l'application
    extern void app_wifi_on_connected(void);
    app_wifi_on_connected();
}

/**
 * @brief Callback interne: Déconnexion
 */
void wifi_sta_on_disconnected(void) {
    g_est_connecte = false;
    g_ip_address[0] = '\0';
    g_rssi = 0;
    
    xEventGroupSetBits(g_wifi_event_group, BIT_ECHEC);
    
    // Notifier l'application
    extern void app_wifi_on_disconnected(void);
    app_wifi_on_disconnected();
}

/**
 * @brief Obtient le RSSI actuel
 */
int8_t wifi_sta_get_rssi(void) {
    if (!g_est_connecte) {
        return 0;
    }
    
    // Rafraîchir RSSI
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        g_rssi = ap_info.rssi;
    }
    
    return g_rssi;
}
