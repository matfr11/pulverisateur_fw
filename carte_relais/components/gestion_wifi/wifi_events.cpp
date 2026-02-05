/**
 * @file wifi_events.cpp
 * @brief Gestionnaire d'événements WiFi
 * @version 1.0
 * @date 2026-02-02
 */

#include "wifi_manager.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

static const char* TAG = LOG_TAG_WIFI;

// Déclarations forward des fonctions internes
extern void wifi_sta_on_connected(esp_netif_ip_info_t* ip_info);
extern void wifi_sta_on_disconnected(void);
extern void wifi_ap_on_sta_connect(void);
extern void wifi_ap_on_sta_disconnect(void);

// ============================================================================
// GESTIONNAIRE D'ÉVÉNEMENTS WIFI
// ============================================================================

/**
 * @brief Gestionnaire centralisé des événements WiFi
 */
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                       int32_t event_id, void* event_data) {
    
    // ========================================================================
    // ÉVÉNEMENTS WIFI GÉNÉRIQUES
    // ========================================================================
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            
            // ----------------------------------------------------------------
            // MODE STATION
            // ----------------------------------------------------------------
            
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "Station démarrée - tentative connexion");
                esp_wifi_connect();
                break;
                
            case WIFI_EVENT_STA_CONNECTED: {
                wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*)event_data;
                ESP_LOGI(TAG, "Connecté au master (canal %d)", event->channel);
                break;
            }
                
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
                ESP_LOGW(TAG, "Déconnecté du master (raison: %d)", event->reason);
                
                // Notifier les modules internes
                wifi_sta_on_disconnected();
                
                // Tentative de reconnexion automatique
                ESP_LOGI(TAG, "Tentative de reconnexion automatique...");
                esp_wifi_connect();
                break;
            }
            
            // ----------------------------------------------------------------
            // MODE ACCESS POINT
            // ----------------------------------------------------------------
            
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "Access Point démarré");
                break;
                
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "Access Point arrêté");
                break;
                
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
                ESP_LOGI(TAG, "Station connectée à notre AP: "
                         "%02x:%02x:%02x:%02x:%02x:%02x (AID %d)",
                         event->mac[0], event->mac[1], event->mac[2],
                         event->mac[3], event->mac[4], event->mac[5],
                         event->aid);
                
                // Notifier modules internes
                wifi_ap_on_sta_connect();
                
                // Notifier application
                app_wifi_on_sta_connected(event->mac);
                break;
            }
                
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
                ESP_LOGI(TAG, "Station déconnectée de notre AP: "
                         "%02x:%02x:%02x:%02x:%02x:%02x (AID %d)",
                         event->mac[0], event->mac[1], event->mac[2],
                         event->mac[3], event->mac[4], event->mac[5],
                         event->aid);
                
                // Notifier modules internes
                wifi_ap_on_sta_disconnect();
                
                // Notifier application
                app_wifi_on_sta_disconnected(event->mac);
                break;
            }
            
            default:
                ESP_LOGD(TAG, "Événement WiFi non géré: %ld", event_id);
                break;
        }
    }
    
    // ========================================================================
    // ÉVÉNEMENTS IP (pour mode Station)
    // ========================================================================
    
    else if (event_base == IP_EVENT) {
        switch (event_id) {
            
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
                ESP_LOGI(TAG, "Adresse IP obtenue: " IPSTR, IP2STR(&event->ip_info.ip));
                ESP_LOGI(TAG, "Masque: " IPSTR, IP2STR(&event->ip_info.netmask));
                ESP_LOGI(TAG, "Passerelle: " IPSTR, IP2STR(&event->ip_info.gw));
                
                // Notifier module station
                wifi_sta_on_connected(&event->ip_info);
                break;
            }
                
            case IP_EVENT_STA_LOST_IP:
                ESP_LOGW(TAG, "Adresse IP perdue");
                break;
                
            default:
                ESP_LOGD(TAG, "Événement IP non géré: %ld", event_id);
                break;
        }
    }
}

/**
 * @brief Enregistre le gestionnaire d'événements WiFi
 */
esp_err_t wifi_register_event_handlers(void) {
    esp_err_t ret;
    
    // Événements WiFi
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                     &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur enregistrement handler WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Événements IP
    ret = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                     &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur enregistrement handler IP: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Handlers d'événements enregistrés");
    return ESP_OK;
}

/**
 * @brief Désenregistre les handlers d'événements
 */
esp_err_t wifi_unregister_event_handlers(void) {
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    
    ESP_LOGI(TAG, "Handlers d'événements désenregistrés");
    return ESP_OK;
}
