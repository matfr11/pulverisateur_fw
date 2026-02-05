/**
 * @file configuration.h
 * @brief Gestionnaire de configuration système
 * @version 1.0
 * @date 2026-02-02
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "esp_err.h"
#include "types_communs.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// INITIALISATION
// ============================================================================

/**
 * @brief Initialise le système de configuration
 * @return ESP_OK si succès
 */
esp_err_t configuration_init(void);

// ============================================================================
// CHARGEMENT ET SAUVEGARDE NVS
// ============================================================================

/**
 * @brief Charge la configuration depuis NVS
 * @param config Pointeur vers structure à remplir
 * @return ESP_OK si succès, ESP_ERR_NOT_FOUND si pas de config sauvegardée
 */
esp_err_t configuration_charger(configuration_systeme_t* config);

/**
 * @brief Sauvegarde la configuration dans NVS
 * @param config Configuration à sauvegarder
 * @return ESP_OK si succès
 */
esp_err_t configuration_sauvegarder(const configuration_systeme_t* config);

/**
 * @brief Charge configuration par défaut
 * @param config Pointeur vers structure à remplir
 */
void configuration_charger_defaut(configuration_systeme_t* config);

/**
 * @brief Efface la configuration NVS
 * @return ESP_OK si succès
 */
esp_err_t configuration_effacer(void);

// ============================================================================
// SYNCHRONISATION MQTT
// ============================================================================

/**
 * @brief Demande la configuration au master via MQTT
 * Pour les SLAVE uniquement
 */
void configuration_demander_au_master(void);

/**
 * @brief Publie la configuration actuelle via MQTT
 * Pour le MASTER uniquement
 */
void configuration_publier(const configuration_systeme_t* config);

/**
 * @brief Applique une configuration reçue via MQTT
 * @param config_json Configuration en JSON
 * @return ESP_OK si succès
 */
esp_err_t configuration_appliquer_mqtt(const char* config_json);

// ============================================================================
// ACCESSEURS CONFIGURATION GLOBALE
// ============================================================================

/**
 * @brief Obtient la configuration actuelle (thread-safe)
 * @param config Pointeur vers structure à remplir
 * @return ESP_OK si succès
 */
esp_err_t configuration_get(configuration_systeme_t* config);

/**
 * @brief Obtient un pointeur constant vers la config (lecture seule, rapide)
 * @return Pointeur constant vers la configuration
 * @note Plus rapide que configuration_get() mais lecture seule
 */
const configuration_systeme_t* configuration_get_ptr(void);

/**
 * @brief Met à jour la configuration
 * @param config Nouvelle configuration
 * @return ESP_OK si succès
 */
esp_err_t configuration_set(const configuration_systeme_t* config);

/**
 * @brief Obtient la version de la configuration
 * @return Numéro de version
 */
uint32_t configuration_get_version(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIGURATION_H