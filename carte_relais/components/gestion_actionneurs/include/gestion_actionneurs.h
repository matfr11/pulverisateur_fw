/**
 * @file gestion_actionneurs.h
 * @brief Pilotage des relais, vannes motorisées avec interlocks et timeouts.
 */
#ifndef GESTION_ACTIONNEURS_H
#define GESTION_ACTIONNEURS_H

#include "types_pulverisateur.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Identifiants des vannes motorisées (carte arrière) */
typedef enum {
    VANNE_ID_2M  = 0,
    VANNE_ID_BDR = 1,
    VANNE_ID_MAX
} vanne_id_t;

/* ====================================================================
 * INITIALISATION
 * ==================================================================== */

/** Initialise toutes les GPIO des actionneurs selon board_config.h */
void actionneurs_initialiser(void);

/* ====================================================================
 * CARTE AVANT – RELAIS SIMPLES
 * ==================================================================== */

/** Toggle la pompe (ON↔OFF) */
void actionneurs_pompe_toggle(void);

/** Active/désactive la pompe directement */
void actionneurs_pompe_set(bool active);

/** Retourne true si la pompe est active */
bool actionneurs_pompe_est_active(void);

/** Toggle la vanne 3 voies (brassage↔transfert) */
void actionneurs_v3v_toggle(void);

/** Positionne la vanne 3 voies */
void actionneurs_v3v_set(etat_vanne_3v_t position);

/** Retourne true si la vanne est en mode transfert */
bool actionneurs_v3v_est_transfert(void);

/** Toggle les phares avant */
void actionneurs_phares_avant_toggle(void);

/** Retourne true si phares avant allumés */
bool actionneurs_phares_avant_actifs(void);

/* ====================================================================
 * CARTE ARRIÈRE – VANNES MOTORISÉES 3 FILS
 * ==================================================================== */

/**
 * @brief Commande une vanne motorisée avec INTERLOCK.
 *
 * RÈGLES DE SÉCURITÉ :
 *   - Ouvrir et fermer JAMAIS simultanés
 *   - Avant d'activer un sens, l'autre est coupé
 *   - Un timer de timeout est démarré
 *
 * @param id   Identifiant de la vanne (2m ou bout de rampe)
 * @param cmd  Commande souhaitée (OUVRE, FERME, STOP)
 */
void actionneurs_vanne_commander(vanne_id_t id, commande_vanne_t cmd);

/**
 * @brief Retourne l'état actuel d'une vanne motorisée.
 */
etat_vanne_mot_t actionneurs_vanne_get_etat(vanne_id_t id);

/**
 * @brief Mise à jour des timeouts des vannes. Appeler périodiquement.
 * @param timeout_ms  Durée maximale avant coupure automatique.
 */
void actionneurs_vannes_update_timeout(uint32_t timeout_ms);

/** Toggle les phares arrière */
void actionneurs_phares_arriere_toggle(void);

/** Retourne true si phares arrière allumés */
bool actionneurs_phares_arriere_actifs(void);

/* ====================================================================
 * ARRÊT D'URGENCE
 * ==================================================================== */

/** Coupe TOUS les actionneurs immédiatement */
void actionneurs_tout_arreter(void);

#ifdef __cplusplus
}
#endif

#endif /* GESTION_ACTIONNEURS_H */
