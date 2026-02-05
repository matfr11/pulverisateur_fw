/**
 * @file actionneurs.h
 * @brief Gestionnaire d'actionneurs - Relais et vannes
 * @version 1.0
 * @date 2026-02-02
 */

#ifndef ACTIONNEURS_H
#define ACTIONNEURS_H

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
 * @brief Initialise le système d'actionneurs
 * GPIO déjà configurés dans app_init.cpp
 * @return ESP_OK si succès
 */
esp_err_t actionneurs_init(void);

/**
 * @brief Démarre la tâche de gestion des actionneurs
 */
void actionneurs_demarrer_tache(void);

// ============================================================================
// COMMANDES CARTE AVANT
// ============================================================================

/**
 * @brief Commande la pompe
 * @param etat ETAT_POMPE_ARRET ou ETAT_POMPE_MARCHE
 * @return ESP_OK si succès
 */
esp_err_t actionneur_pompe_set(etat_pompe_t etat);

/**
 * @brief Toggle la pompe
 * @return ESP_OK si succès
 */
esp_err_t actionneur_pompe_toggle(void);

/**
 * @brief Obtient l'état actuel de la pompe
 * @return État pompe
 */
etat_pompe_t actionneur_pompe_get(void);

/**
 * @brief Commande la vanne 3 voies
 * @param position POSITION_VANNE_3V_BRASSAGE ou POSITION_VANNE_3V_TRANSFERT
 * @return ESP_OK si succès
 */
esp_err_t actionneur_vanne_3voies_set(position_vanne_3voies_t position);

/**
 * @brief Toggle vanne 3 voies
 * @return ESP_OK si succès
 */
esp_err_t actionneur_vanne_3voies_toggle(void);

/**
 * @brief Obtient la position actuelle vanne 3 voies
 * @return Position vanne
 */
position_vanne_3voies_t actionneur_vanne_3voies_get(void);

/**
 * @brief Commande les phares avant
 * @param etat true = ON, false = OFF
 * @return ESP_OK si succès
 */
esp_err_t actionneur_phares_avant_set(bool etat);

/**
 * @brief Toggle phares avant
 * @return ESP_OK si succès
 */
esp_err_t actionneur_phares_avant_toggle(void);

// ============================================================================
// COMMANDES CARTE ARRIÈRE - VANNES 3 FILS
// ============================================================================

/**
 * @brief Commande vanne 2m
 * @param action "OUVRIR", "FERMER", ou "STOP"
 * @return ESP_OK si succès
 */
esp_err_t actionneur_vanne_2m(const char* action);

/**
 * @brief Commande vanne bout de rampe
 * @param action "OUVRIR", "FERMER", ou "STOP"
 * @return ESP_OK si succès
 */
esp_err_t actionneur_vanne_bout_rampe(const char* action);

/**
 * @brief Obtient l'état vanne 2m
 * @return État vanne
 */
etat_vanne_3fils_t actionneur_vanne_2m_get(void);

/**
 * @brief Obtient l'état vanne bout de rampe
 * @return État vanne
 */
etat_vanne_3fils_t actionneur_vanne_bout_rampe_get(void);

/**
 * @brief Commande les phares arrière
 * @param etat true = ON, false = OFF
 * @return ESP_OK si succès
 */
esp_err_t actionneur_phares_arriere_set(bool etat);

/**
 * @brief Toggle phares arrière
 * @return ESP_OK si succès
 */
esp_err_t actionneur_phares_arriere_toggle(void);

// ============================================================================
// SÉCURITÉ VANNES 3 FILS
// ============================================================================

/**
 * @brief Arrête toutes les vannes 3 fils (sécurité)
 */
void actionneurs_arreter_toutes_vannes(void);

/**
 * @brief Vérifie si une vanne est en timeout
 * @param vanne Identifiant vanne ("vanne_2m" ou "vanne_bout_rampe")
 * @return true si timeout actif
 */
bool actionneur_vanne_est_en_timeout(const char* vanne);

// ============================================================================
// MODE SIMULATION
// ============================================================================

#if MODE_SIMULATION
/**
 * @brief Active le mode simulation
 * Les GPIO ne sont pas touchés, états simulés en RAM
 */
void actionneurs_set_simulation(bool enable);

/**
 * @brief Vérifie si mode simulation actif
 */
bool actionneurs_est_simulation(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // ACTIONNEURS_H
