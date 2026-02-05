/**
 * @file capteurs.h
 * @brief Gestionnaire de capteurs - Débitmètre et sonde niveau
 * @version 1.0
 * @date 2026-02-02
 */

#ifndef CAPTEURS_H
#define CAPTEURS_H

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
 * @brief Initialise le système de capteurs
 * GPIO déjà configurés dans app_init.cpp
 * @return ESP_OK si succès
 */
esp_err_t capteurs_init(void);

/**
 * @brief Démarre la tâche de gestion des capteurs
 */
void capteurs_demarrer_tache(void);

// ============================================================================
// DÉBITMÈTRE
// ============================================================================

/**
 * @brief Obtient les données du débitmètre
 * @param donnees Pointeur vers structure à remplir
 * @return ESP_OK si succès
 */
esp_err_t debitmetre_get_donnees(donnees_debitmetre_t* donnees);

/**
 * @brief Obtient le débit instantané
 * @return Débit en L/min
 */
float debitmetre_get_debit(void);

/**
 * @brief Calcule le débit (A AJOUTER ICI)
 */
void debitmetre_calculer_debit(void);

/**
 * @brief Obtient le volume total
 * @return Volume en litres
 */
float debitmetre_get_volume_total(void);

/**
 * @brief Obtient le nombre d'impulsions total
 * @return Nombre d'impulsions
 */
uint32_t debitmetre_get_impulsions(void);

/**
 * @brief Réinitialise le compteur de volume
 */
void debitmetre_reset_volume(void);

/**
 * @brief Configure le facteur K du débitmètre
 * @param facteur_k Impulsions par litre
 */
void debitmetre_set_facteur_k(float facteur_k);

/**
 * @brief Obtient le facteur K actuel
 * @return Facteur K
 */
float debitmetre_get_facteur_k(void);

// ============================================================================
// SONDE NIVEAU (optionnelle - future)
// ============================================================================

/**
 * @brief Obtient les données de la sonde niveau
 * @param donnees Pointeur vers structure à remplir
 * @return ESP_OK si succès, ESP_ERR_NOT_FOUND si non disponible
 */
esp_err_t sonde_niveau_get_donnees(donnees_niveau_t* donnees);

/**
 * @brief Obtient le niveau en litres
 * @return Niveau en L, ou -1 si non disponible
 */
float sonde_niveau_get_litres(void);

/**
 * @brief Vérifie si la sonde niveau est disponible
 * @return true si capteur présent
 */
bool sonde_niveau_est_disponible(void);

// ============================================================================
// MODE SIMULATION
// ============================================================================

#if MODE_SIMULATION
/**
 * @brief Active le mode simulation
 * Les impulsions sont simulées
 */
void capteurs_set_simulation(bool enable);

/**
 * @brief Simule une impulsion débitmètre
 */
void debitmetre_simuler_impulsion(void);

/**
 * @brief Configure un débit simulé constant
 * @param debit_lpm Débit en L/min
 */
void debitmetre_simuler_debit_constant(float debit_lpm);
#endif

// ============================================================================
// CALIBRATION
// ============================================================================

/**
 * @brief Démarre une session de calibration débitmètre
 * 
 * Procédure:
 * 1. Appeler cette fonction
 * 2. Faire passer un volume connu (ex: 100L)
 * 3. Appeler debitmetre_calibration_terminer(volume_litres)
 * 4. Le facteur K est calculé et sauvegardé
 */
void debitmetre_calibration_demarrer(void);

/**
 * @brief Termine la calibration et calcule le facteur K
 * @param volume_reel_litres Volume réel écoulé
 * @return Facteur K calculé
 */
float debitmetre_calibration_terminer(float volume_reel_litres);

/**
 * @brief Vérifie si une calibration est en cours
 * @return true si calibration active
 */
bool debitmetre_est_en_calibration(void);

#ifdef __cplusplus
}
#endif

#endif // CAPTEURS_H
