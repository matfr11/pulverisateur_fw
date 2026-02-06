/**
 * @file gestion_configuration.h
 * @brief Sauvegarde/chargement de la configuration en NVS (Non-Volatile Storage).
 */
#ifndef GESTION_CONFIGURATION_H
#define GESTION_CONFIGURATION_H

#include "types_pulverisateur.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise le namespace NVS pour la configuration */
void configuration_initialiser(void);

/**
 * @brief Charge la configuration depuis le NVS.
 * @param config_out  Structure à remplir.
 * @return true si une configuration valide a été chargée.
 */
bool configuration_charger(configuration_t *config_out);

/**
 * @brief Sauvegarde la configuration dans le NVS.
 * @param config  Configuration à sauvegarder.
 * @return true si la sauvegarde a réussi.
 */
bool configuration_sauvegarder(const configuration_t *config);

/**
 * @brief Efface la configuration NVS et remet les valeurs par défaut.
 */
void configuration_effacer(void);

#ifdef __cplusplus
}
#endif

#endif /* GESTION_CONFIGURATION_H */
