/**
 * @file gestion_automatismes.h
 * @brief Machines à états pour le transfert automatique et le brassage.
 *
 * CARTE AVANT UNIQUEMENT.
 *
 * Règle de priorité : MANUEL > SÉCURITÉ > AUTOMATISME
 */
#ifndef GESTION_AUTOMATISMES_H
#define GESTION_AUTOMATISMES_H

#include <stddef.h>
#include "types_pulverisateur.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise les automatismes (machines à états au repos) */
void automatismes_initialiser(void);

/* ====================================================================
 * TRANSFERT AUTOMATIQUE
 *
 * Mode SANS sonde : transfère le volume défini puis s'arrête.
 * Mode AVEC sonde : cycle tant que cuve avant non vide.
 * ==================================================================== */

/** Active le transfert automatique */
void automatismes_transfert_activer(const configuration_t *config);

/** Arrête le transfert automatique */
void automatismes_transfert_arreter(void);

/** Retourne l'état du transfert */
etat_auto_transfert_t automatismes_get_etat_transfert(void);

//**volume transferé */
float automatismes_get_volume_transfere(void);

/* ====================================================================
 * BRASSAGE AUTOMATIQUE
 *
 * Cycle : pompe ON pendant temps_on → OFF pendant temps_off → ON...
 * Suspendu automatiquement si le transfert est actif.
 * ==================================================================== */

/** Active le brassage automatique */
void automatismes_brassage_activer(const configuration_t *config);

/** Arrête le brassage automatique */
void automatismes_brassage_arreter(void);

/** Retourne l'état du brassage */
etat_auto_brassage_t automatismes_get_etat_brassage(void);

/**
 * @brief Récupère les infos d'affichage du brassage.
 * @param label_out   Buffer pour le label ("MARCHE" / "PAUSE" / "SUSPENDU")
 * @param label_size  Taille du buffer
 * @param temps_restant_out  Temps restant dans la phase courante (minutes)
 * @param pourcentage_out    Pourcentage de la phase courante (0-100)
 */
void automatismes_get_brassage_info(char *label_out, size_t label_size,
                                     float *temps_restant_out,
                                     float *pourcentage_out);

/* ====================================================================
 * MISE À JOUR PÉRIODIQUE
 *
 * Doit être appelée toutes les ~100ms depuis la tâche principale.
 * ==================================================================== */

/**
 * @brief Met à jour les machines à états des automatismes.
 * @param debit_lpm      Débit instantané en L/min
 * @param volume_session Volume cumulé de la session en litres
 */
void automatismes_update(float debit_lpm, float volume_session);

/** Arrête tous les automatismes */
void automatismes_arreter_tout(void);

#ifdef __cplusplus
}
#endif

#endif /* GESTION_AUTOMATISMES_H */
