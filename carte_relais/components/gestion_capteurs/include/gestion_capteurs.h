/**
 * @file gestion_capteurs.h
 * @brief Lecture du débitmètre à impulsions et de la sonde de niveau.
 */
#ifndef GESTION_CAPTEURS_H
#define GESTION_CAPTEURS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 * INITIALISATION
 * ==================================================================== */

/** Initialise les capteurs selon board_config.h */
void capteurs_initialiser(void);

/* ====================================================================
 * DÉBITMÈTRE (carte AVANT)
 * ==================================================================== */

/** Mise à jour périodique (calculer débit depuis les impulsions) */
void capteurs_debitmetre_update(void);

/** Retourne le débit instantané en L/min */
float capteurs_debitmetre_get_debit(void);

/** Retourne le volume cumulé de la session en litres */
float capteurs_debitmetre_get_volume_session(void);

/** Remet le volume de session à zéro */
void capteurs_debitmetre_reset_session(void);

/** Vérifie si le débitmètre est opérationnel */
bool capteurs_debitmetre_est_ok(void);

/** Met à jour le facteur K (impulsions/litre) */
void capteurs_debitmetre_set_facteur_k(float k);

/* ====================================================================
 * SONDE DE NIVEAU (carte ARRIÈRE, future)
 * ==================================================================== */

/** Mise à jour périodique de la lecture de niveau */
void capteurs_sonde_niveau_update(void);

/** Retourne le niveau en pourcentage (0-100) */
float capteurs_sonde_get_niveau(void);

/** Vérifie si la sonde est opérationnelle */
bool capteurs_sonde_est_ok(void);

#ifdef __cplusplus
}
#endif

#endif /* GESTION_CAPTEURS_H */
