/**
 * @file app_role.h
 * @brief Gestion du rôle Master/Slave dans le système
 * 
 * Logique:
 * - La PREMIÈRE carte qui démarre devient MASTER
 * - Elle crée le WiFi AP et le broker MQTT
 * - Les cartes suivantes deviennent SLAVE
 * - En cas de panne du MASTER, un SLAVE prend le relais
 * - L'ancien MASTER qui revient reste SLAVE (pas de conflit)
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#ifndef APP_ROLE_H
#define APP_ROLE_H

#include "types_communs.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// FONCTIONS PUBLIQUES
// ============================================================================

/**
 * @brief Détermine le rôle de la carte (Master ou Slave)
 * 
 * Algorithme:
 * 1. Scan WiFi pendant WIFI_TIMEOUT_DETECTION_MS
 * 2. Si SSID "PulveriAG" détecté → SLAVE
 * 3. Sinon → MASTER (première carte)
 * 
 * @return role_carte_t Le rôle détecté
 */
role_carte_t app_role_determiner_role(void);

/**
 * @brief Surveille la présence du Master (tâche continue pour SLAVE)
 * 
 * Si le SLAVE perd la connexion pendant plus de X secondes:
 * - Il tente de se reconnecter
 * - Si échec répété → il devient MASTER
 * - Il crée le WiFi et MQTT
 */
void app_role_surveiller_master(void);

/**
 * @brief Force le passage en mode Master
 * 
 * Utilisé quand un SLAVE détecte la perte du Master
 * 
 * @return ESP_OK si la transition réussit
 */
esp_err_t app_role_devenir_master(void);

/**
 * @brief Retourne le rôle actuel de la carte
 * 
 * @return role_carte_t ROLE_MASTER ou ROLE_SLAVE
 */
role_carte_t app_role_get_role_actuel(void);

/**
 * @brief Callback appelé quand le WiFi se déconnecte (SLAVE uniquement)
 * 
 * Déclenche la surveillance et éventuellement la promotion en Master
 */
void app_role_on_wifi_disconnected(void);

/**
 * @brief Callback appelé quand MQTT se déconnecte (SLAVE uniquement)
 * 
 * Idem que WiFi, surveille et peut promouvoir en Master
 */
void app_role_on_mqtt_disconnected(void);

#ifdef __cplusplus
}
#endif

#endif // APP_ROLE_H
