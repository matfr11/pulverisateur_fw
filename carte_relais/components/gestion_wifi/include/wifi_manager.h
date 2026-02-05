/**
 * @file wifi_manager.h
 * @brief Gestionnaire WiFi - Mode AP (Master) et Station (Slave)
 * @version 1.0
 * @date 2026-02-02
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// FONCTIONS MODE ACCESS POINT (MASTER)
// ============================================================================

/**
 * @brief Démarre le WiFi en mode Access Point
 * @return ESP_OK si succès
 */
esp_err_t wifi_start_ap(void);

/**
 * @brief Arrête le mode Access Point
 * @return ESP_OK si succès
 */
esp_err_t wifi_stop_ap(void);

/**
 * @brief Retourne le nombre de stations connectées à l'AP
 * @return Nombre de stations
 */
uint8_t wifi_ap_get_station_count(void);

// ============================================================================
// FONCTIONS MODE STATION (SLAVE)
// ============================================================================

/**
 * @brief Démarre le WiFi en mode Station et se connecte au master
 * @return ESP_OK si succès
 */
esp_err_t wifi_start_sta(void);

/**
 * @brief Arrête le mode Station
 * @return ESP_OK si succès
 */
esp_err_t wifi_stop_sta(void);

/**
 * @brief Tente une reconnexion WiFi
 * @return ESP_OK si reconnecté
 */
esp_err_t wifi_reconnect(void);

/**
 * @brief Vérifie si le WiFi est connecté (pour SLAVE)
 * @return true si connecté
 */
bool wifi_sta_est_connecte(void);

/**
 * @brief Obtient l'adresse IP actuelle (mode Station)
 * @param ip Buffer pour stocker l'IP (min 16 bytes)
 * @return ESP_OK si succès
 */
esp_err_t wifi_sta_get_ip(char* ip);

// ============================================================================
// FONCTIONS COMMUNES
// ============================================================================

/**
 * @brief Vérifie si le WiFi est connecté (mode AP ou Station)
 * @return true si opérationnel
 */
bool app_wifi_est_connecte(void);

/**
 * @brief Obtient le RSSI actuel
 * @return RSSI en dBm, 0 si non disponible
 */
int8_t app_wifi_get_rssi(void);

/**
 * @brief Callback: WiFi connecté
 * À implémenter par l'application
 */
void app_wifi_on_connected(void);

/**
 * @brief Callback: WiFi déconnecté
 * À implémenter par l'application
 */
void app_wifi_on_disconnected(void);

/**
 * @brief Callback: Station connectée à notre AP (master uniquement)
 * @param mac Adresse MAC de la station
 */
void app_wifi_on_sta_connected(uint8_t* mac);

/**
 * @brief Callback: Station déconnectée de notre AP (master uniquement)
 * @param mac Adresse MAC de la station
 */
void app_wifi_on_sta_disconnected(uint8_t* mac);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
