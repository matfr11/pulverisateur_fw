/**
 * @file mqtt_manager.h
 * @brief Interface publique du gestionnaire MQTT
 * @version 1.1
 * @date 2026-02-05
 */

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "esp_err.h"
#include "types_communs.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// INITIALISATION
// ============================================================================

/**
 * @brief Initialise le client MQTT
 * @param role Rôle de la carte (MASTER ou SLAVE)
 * @return ESP_OK si succès
 */
esp_err_t mqtt_init(role_carte_t role);

/**
 * @brief Arrête le client MQTT
 * @return ESP_OK si succès
 */
esp_err_t mqtt_stop(void);

// ============================================================================
// PUBLICATION GÉNÉRIQUE
// ============================================================================

/**
 * @brief Publie un message MQTT
 * @param topic Topic MQTT
 * @param payload Données JSON
 * @param qos Quality of Service (0 ou 1)
 * @param retain Message retained ou non
 * @return ESP_OK si succès
 */
esp_err_t mqtt_publish(const char* topic, const char* payload, int qos, bool retain);

// ============================================================================
// PUBLICATIONS SPÉCIFIQUES
// ============================================================================

/**
 * @brief Publie l'état de la pompe
 */
void mqtt_publier_etat_pompe(etat_pompe_t etat);

/**
 * @brief Publie l'état de la vanne 3 voies
 */
void mqtt_publier_etat_vanne_3voies(position_vanne_3voies_t position);

/**
 * @brief Publie l'état d'une vanne 3 fils
 */
void mqtt_publier_etat_vanne_3fils(const char* vanne_id, etat_vanne_3fils_t etat);

/**
 * @brief Publie l'état des phares
 */
void mqtt_publier_etat_phares(const char* type, bool allume);

/**
 * @brief Publie les données du débitmètre
 */
void mqtt_publier_debitmetre(float debit_lpm, float volume_total_litres);

/**
 * @brief Publie l'état du transfert automatique
 */
void mqtt_publier_etat_transfert(etat_transfert_t etat, float volume_transfere);

/**
 * @brief Publie l'état du brassage automatique
 */
void mqtt_publier_etat_brassage(etat_brassage_t etat, uint32_t temps_restant_sec);

/**
 * @brief Publie une alerte de sécurité
 */
void mqtt_publier_alerte_securite(type_alerte_securite_t type, bool active);

/**
 * @brief Publie la configuration système complète
 * @param config Configuration à publier
 */
void mqtt_publier_configuration(const configuration_systeme_t* config);

/**
 * @brief Publie le heartbeat/présence de la carte
 */
void mqtt_publier_presence(role_carte_t role, bool online);

// ============================================================================
// DEMANDES ET REQUÊTES
// ============================================================================

/**
 * @brief Demande la configuration au master (pour SLAVE)
 */
void app_mqtt_demander_configuration(void);

// ============================================================================
// CALLBACKS (À IMPLÉMENTER PAR L'APPLICATION)
// ============================================================================

/**
 * @brief Callback appelé lors de la réception d'une commande pour carte avant
 * @param actionneur Nom de l'actionneur (ex: "pompe", "vanne_3voies")
 * @param commande Commande JSON
 */
void mqtt_callback_commande_avant(const char* actionneur, const char* commande) __attribute__((weak));

/**
 * @brief Callback appelé lors de la réception d'une commande pour carte arrière
 * @param actionneur Nom de l'actionneur (ex: "vanne_2m", "vanne_bout_rampe")
 * @param commande Commande JSON
 */
void mqtt_callback_commande_arriere(const char* actionneur, const char* commande) __attribute__((weak));

/**
 * @brief Callback appelé lors de la réception d'une commande automatisme
 * @param automatisme Nom de l'automatisme (ex: "transfert", "brassage")
 * @param commande Commande JSON
 */
void mqtt_callback_automatisme(const char* automatisme, const char* commande) __attribute__((weak));

/**
 * @brief Callback appelé lors de la réception d'une configuration
 * @param config_json Configuration complète en JSON
 */
void mqtt_callback_configuration(const char* config_json) __attribute__((weak));

/**
 * @brief Callback appelé lors du changement d'état de connexion MQTT
 * @param connected true si connecté, false si déconnecté
 */
void mqtt_callback_connection_status(bool connected) __attribute__((weak));

#ifdef __cplusplus
}
#endif

#endif // MQTT_MANAGER_H