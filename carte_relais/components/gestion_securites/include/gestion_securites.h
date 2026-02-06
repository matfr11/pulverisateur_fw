/**
 * @file gestion_securites.h
 * @brief Détection de la cuve avant vide (pompe active + débit faible).
 */
#ifndef GESTION_SECURITES_H
#define GESTION_SECURITES_H

#include "types_pulverisateur.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise le module de sécurités */
void securites_initialiser(void);

/**
 * @brief Mise à jour périodique de la détection cuve vide.
 *
 * @param pompe_active  true si la pompe est en marche
 * @param debit_lpm     Débit instantané en L/min
 * @param config        Configuration courante (seuils)
 */
void securites_update(bool pompe_active, float debit_lpm, const configuration_t *config);

/** Retourne true si la cuve avant est confirmée vide */
bool securites_cuve_est_vide(void);

/** Retourne l'état détaillé de la sécurité cuve */
etat_securite_cuve_t securites_get_etat_cuve(void);

/**
 * @brief Réarme la sécurité cuve vide.
 *
 * Appelé quand l'opérateur relance manuellement la pompe
 * et que le débit est redevenu normal.
 */
void securites_rearmement_cuve(void);

#ifdef __cplusplus
}
#endif

#endif /* GESTION_SECURITES_H */
