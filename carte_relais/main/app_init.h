/**
 * @file app_init.h
 * @brief Initialisation globale de l'application carte relais.
 */
#ifndef APP_INIT_H
#define APP_INIT_H

#include "types_pulverisateur.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise tous les sous-systèmes de la carte.
 *
 * Séquence :
 *   1. Configuration NVS + chargement paramètres
 *   2. Initialisation GPIO (actionneurs + capteurs)
 *   3. Démarrage WiFi (détermination rôle MASTER/SLAVE)
 *   4. Connexion MQTT
 *   5. Démarrage Web UI (si MASTER + carte avant)
 *   6. Souscription MQTT et synchronisation
 *   7. Démarrage de la tâche périodique principale
 */
void app_initialiser(void);

/**
 * @brief Tâche principale exécutée périodiquement (toutes les 100ms).
 *
 * Responsabilités :
 *   - Lecture capteurs
 *   - Mise à jour machines à états (automatismes, sécurités)
 *   - Publication des états MQTT
 */
void app_tache_principale(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* APP_INIT_H */
