/**
 * @file securites.h
 * @brief Gestionnaire de sécurités système
 * @version 1.0
 * @date 2026-02-02
 */

#ifndef SECURITES_H
#define SECURITES_H

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
 * @brief Initialise le système de sécurités
 * @return ESP_OK si succès
 */
esp_err_t securites_init(void);

/**
 * @brief Démarre la tâche de surveillance sécurités
 */
void securites_demarrer_tache(void);

// ============================================================================
// DÉTECTION CUVE VIDE
// ============================================================================

/**
 * @brief Vérifie si la cuve avant est vide
 * Détection basée sur débit nul pendant pompe active
 * @return true si cuve vide détectée
 */
bool securites_cuve_avant_est_vide(void);

/**
 * @brief Obtient l'état actuel de la cuve avant
 * @return État cuve
 */
etat_cuve_avant_t securites_get_etat_cuve_avant(void);

/**
 * @brief Configure le seuil de détection cuve vide
 * @param seuil_debit_lpm Débit en dessous duquel on considère vide (L/min)
 * @param delai_detection_ms Délai avant confirmation (ms)
 */
void securites_configurer_detection_cuve_vide(float seuil_debit_lpm, uint32_t delai_detection_ms);

// ============================================================================
// TIMEOUTS VANNES
// ============================================================================

/**
 * @brief Vérifie les timeouts des vannes 3 fils
 * Appelé automatiquement par la tâche de surveillance
 */
void securites_verifier_timeouts_vannes(void);

/**
 * @brief Obtient l'état timeout d'une vanne
 * @param vanne Nom de la vanne ("vanne_2m" ou "vanne_bout_rampe")
 * @return true si en timeout
 */
bool securites_vanne_est_en_timeout(const char* vanne);

// ============================================================================
// ARRÊT D'URGENCE
// ============================================================================

/**
 * @brief Déclenche arrêt d'urgence complet du système
 * Arrête tous actionneurs et automatismes
 */
void securites_arret_urgence_global(void);

/**
 * @brief Vérifie si système en arrêt d'urgence
 * @return true si arrêt d'urgence actif
 */
bool securites_est_en_arret_urgence(void);

/**
 * @brief Libère l'arrêt d'urgence
 * Permet de redémarrer le système
 */
void securites_liberer_arret_urgence(void);

// ============================================================================
// ALERTES
// ============================================================================

/**
 * @brief Publie une alerte sécurité via MQTT
 * @param type Type d'alerte
 * @param description Description texte
 * @param niveau Niveau de criticité (0=info, 1=warning, 2=critique)
 */
void securites_publier_alerte(const char* type, const char* description, int niveau);

/**
 * @brief Obtient le nombre d'alertes actives
 * @return Nombre d'alertes
 */
uint32_t securites_get_nombre_alertes(void);

// ============================================================================
// STATISTIQUES
// ============================================================================

/**
 * @brief Affiche les statistiques sécurités
 */
void securites_get_stats(void);

#ifdef __cplusplus
}
#endif

#endif // SECURITES_H
