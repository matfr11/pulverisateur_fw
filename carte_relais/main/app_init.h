/**
 * @file app_init.h
 * @brief Déclarations des fonctions d'initialisation
 * @version 1.0
 * @date 2026-02-02
 */

#ifndef APP_INIT_H
#define APP_INIT_H

#include "esp_err.h"
#include "types_communs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h" // Pour EventGroupHandle_t

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CONFIGURATION
// ============================================================================

/**
 * @brief Initialise la configuration depuis NVS
 * @param config Pointeur vers la structure de configuration
 * @return ESP_OK si succès
 */
esp_err_t app_init_configuration(configuration_systeme_t* config);

/**
 * @brief Initialise la configuration avec valeurs par défaut
 * @param config Pointeur vers la structure de configuration
 */
void app_init_configuration_defaut(configuration_systeme_t* config);

/**
 * @brief Sauvegarde la configuration dans NVS
 * @param config Pointeur vers la configuration à sauvegarder
 * @return ESP_OK si succès
 */
esp_err_t app_save_configuration(const configuration_systeme_t* config);

// ============================================================================
// ACTIONNEURS
// ============================================================================

/**
 * @brief Initialise les GPIO des actionneurs
 * @return ESP_OK si succès
 */
esp_err_t app_init_actionneurs(void);

/**
 * @brief Démarre la tâche de gestion des actionneurs
 */
void app_demarrer_taches_actionneurs(void);

// ============================================================================
// CAPTEURS
// ============================================================================

/**
 * @brief Initialise les capteurs (débitmètre, sonde niveau)
 * @return ESP_OK si succès
 */
esp_err_t app_init_capteurs(void);

/**
 * @brief Démarre la tâche de lecture des capteurs
 */
void app_demarrer_taches_capteurs(void);

// ============================================================================
// AUTOMATISMES
// ============================================================================

/**
 * @brief Démarre les tâches des automatismes
 */
void app_demarrer_taches_automatismes(void);

// ============================================================================
// SÉCURITÉS
// ============================================================================

/**
 * @brief Démarre les tâches de surveillance sécurité
 */
void app_demarrer_taches_securites(void);

// ============================================================================
// WIFI
// ============================================================================

/**
 * @brief Initialise le WiFi selon le rôle
 * @param role ROLE_MASTER ou ROLE_SLAVE
 * @return ESP_OK si succès
 */
esp_err_t app_init_wifi(role_carte_t role);

/**
 * @brief Vérifie si le WiFi est connecté
 * @return true si connecté
 */
bool app_wifi_est_connecte(void);

/**
 * @brief Obtient le RSSI WiFi
 * @return RSSI en dBm
 */
int8_t app_wifi_get_rssi(void);

// ============================================================================
// MQTT
// ============================================================================

/**
 * @brief Initialise MQTT (broker ou client selon rôle)
 * @param role ROLE_MASTER ou ROLE_SLAVE
 * @return ESP_OK si succès
 */
esp_err_t app_init_mqtt(role_carte_t role);

/**
 * @brief Vérifie si MQTT est connecté
 * @return true si connecté
 */
bool app_mqtt_est_connecte(void);

/**
 * @brief Tente une reconnexion MQTT
 */
void app_mqtt_reconnect(void);

/**
 * @brief Publie la présence de la carte
 * @param en_ligne true si carte opérationnelle
 */
void app_mqtt_publier_presence(bool en_ligne);

/**
 * @brief Publie l'état complet du système
 * @param etat Pointeur vers l'état système
 */
void app_mqtt_publier_etat_systeme(const etat_complet_systeme_t* etat);

/**
 * @brief Demande un instantané de configuration
 */
void app_mqtt_demander_configuration(void);

// ============================================================================
// ACCÈS ÉTAT SYSTÈME
// ============================================================================

/**
 * @brief Retourne un pointeur vers l'état système (lecture seule)
 */
const etat_complet_systeme_t* app_get_etat_systeme(void);

/**
 * @brief Retourne un pointeur vers l'état système (lecture/écriture)
 * ATTENTION: Réservé aux composants internes
 */
etat_complet_systeme_t* app_get_etat_systeme_rw(void);

/**
 * @brief Retourne l'event group système
 */
EventGroupHandle_t app_get_event_group_systeme(void);

#ifdef __cplusplus
}
#endif

#endif // APP_INIT_H
