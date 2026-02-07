/**
 * @file gestion_web_ui.h
 * @brief Serveur HTTP embarqué servant les pages de contrôle et settings.
 *
 * ADAPTATION de la Web UI existante (Arduino) vers ESP-IDF :
 *   - esp_http_server remplace le WebServer Arduino
 *   - Les commandes passent par MQTT au lieu d'appels directs
 *   - Le /status retourne l'état publié par les cartes via MQTT
 */
#ifndef GESTION_WEB_UI_H
#define GESTION_WEB_UI_H

#include "types_pulverisateur.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Démarre le serveur HTTP avec toutes les routes.
 * @param config  Configuration initiale (pour la page settings).
 */
void web_ui_demarrer(const configuration_t *config);

/**
 * @brief Arrête le serveur HTTP.
 */
void web_ui_arreter(void);

/**
 * @brief Met à jour l'état interne pour le endpoint /status.
 * Appelé quand un état MQTT est reçu.
 */
void web_ui_update_etat_avant(const etat_carte_avant_t *etat);
void web_ui_update_etat_arriere(const etat_carte_arriere_t *etat);
void web_ui_set_carte_master(carte_id_t id);
#ifdef __cplusplus
}
#endif

#endif /* GESTION_WEB_UI_H */
