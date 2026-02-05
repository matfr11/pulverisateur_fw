/**
 * @file automatismes.h
 * @brief Gestionnaire d'automatismes - Transfert et Brassage
 * @version 1.0
 * @date 2026-02-02
 */

#ifndef AUTOMATISMES_H
#define AUTOMATISMES_H

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
 * @brief Initialise le système d'automatismes
 * @return ESP_OK si succès
 */
esp_err_t automatismes_init(void);

/**
 * @brief Démarre les tâches d'automatismes
 */
void automatismes_demarrer_taches(void);

// ============================================================================
// TRANSFERT AUTOMATIQUE
// ============================================================================

/**
 * @brief Active le transfert automatique
 * @param mode MODE_TRANSFERT_SANS_SONDE ou MODE_TRANSFERT_AVEC_SONDE
 * @param volume_cible_litres Volume à transférer (mode SANS_SONDE)
 * @return ESP_OK si succès
 */
esp_err_t transfert_activer(mode_transfert_t mode, float volume_cible_litres);

/**
 * @brief Désactive le transfert automatique
 * @return ESP_OK si succès
 */
esp_err_t transfert_desactiver(void);

/**
 * @brief Obtient l'état actuel du transfert
 * @return État transfert
 */
etat_transfert_t transfert_get_etat(void);

/**
 * @brief Obtient le volume transféré dans le cycle actuel
 * @return Volume en litres
 */
float transfert_get_volume_cycle(void);

/**
 * @brief Obtient le pourcentage d'avancement
 * @return Pourcentage 0-100
 */
float transfert_get_pourcentage(void);

/**
 * @brief Vérifie si transfert est actif
 * @return true si actif
 */
bool transfert_est_actif(void);

// ============================================================================
// BRASSAGE AUTOMATIQUE
// ============================================================================

/**
 * @brief Active le brassage automatique
 * @param temps_marche_sec Temps de marche en secondes
 * @param temps_pause_sec Temps de pause en secondes
 * @return ESP_OK si succès
 */
esp_err_t brassage_activer(uint32_t temps_marche_sec, uint32_t temps_pause_sec);

/**
 * @brief Désactive le brassage automatique
 * @return ESP_OK si succès
 */
esp_err_t brassage_desactiver(void);

/**
 * @brief Obtient l'état actuel du brassage
 * @return État brassage
 */
etat_brassage_t brassage_get_etat(void);

/**
 * @brief Obtient le temps restant dans la phase actuelle
 * @return Temps en secondes
 */
uint32_t brassage_get_temps_restant(void);

/**
 * @brief Obtient le pourcentage d'avancement dans le cycle
 * @return Pourcentage 0-100
 */
float brassage_get_pourcentage(void);

/**
 * @brief Vérifie si brassage est actif
 * @return true si actif
 */
bool brassage_est_actif(void);

/**
 * @brief Suspend temporairement le brassage
 * Appelé automatiquement pendant transfert
 */
void brassage_suspendre(void);

/**
 * @brief Reprend le brassage
 * Appelé automatiquement après transfert
 */
void brassage_reprendre(void);

// ============================================================================
// ARRÊT D'URGENCE
// ============================================================================

/**
 * @brief Arrête tous les automatismes immédiatement
 */
void automatismes_arret_urgence(void);

/**
 * @brief Désactive tous les automatismes
 */
void automatismes_desactiver_tout(void);

// ============================================================================
// ÉTAT ET DIAGNOSTICS
// ============================================================================

/**
 * @brief Obtient l'état complet des automatismes
 * @param etat Pointeur vers structure à remplir
 * @return ESP_OK si succès
 */
esp_err_t automatismes_get_etat_complet(etat_automatismes_t* etat);

/**
 * @brief Affiche les statistiques automatismes
 */
void automatismes_get_stats(void);

#ifdef __cplusplus
}
#endif

#endif // AUTOMATISMES_H
