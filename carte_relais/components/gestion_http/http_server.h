/**
 * @file http_server.h
 * @brief Interface publique du serveur HTTP
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"
#include "types_communs.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SERVEUR HTTP
// ============================================================================

/**
 * @brief Démarre le serveur HTTP sur port 80
 * @return ESP_OK si succès
 */
esp_err_t http_server_start(void);

/**
 * @brief Arrête le serveur HTTP
 * @return ESP_OK si succès
 */
esp_err_t http_server_stop(void);

// ============================================================================
// MISE À JOUR ÉTATS (depuis MQTT)
// ============================================================================

/**
 * @brief Met à jour l'état de la pompe
 */
void http_status_update_pompe(bool marche);

/**
 * @brief Met à jour l'état de la vanne 3 voies
 * @param transfert false = brassage, true = transfert
 */
void http_status_update_vanne_3v(bool transfert);

/**
 * @brief Met à jour l'état des phares avant
 */
void http_status_update_phares_avant(bool on);

/**
 * @brief Met à jour l'état des phares arrière
 */
void http_status_update_phares_arriere(bool on);

/**
 * @brief Met à jour les données du débitmètre
 */
void http_status_update_debit(float debit_lmin, float volume_total);

/**
 * @brief Met à jour l'état de la vanne 2m
 */
void http_status_update_vanne_2m(etat_vanne_3fils_t etat);

/**
 * @brief Met à jour l'état de la vanne bout de rampe
 */
void http_status_update_vanne_bout(etat_vanne_3fils_t etat);

/**
 * @brief Met à jour l'état de détection cuve vide
 */
void http_status_update_cuve_vide(bool vide);

/**
 * @brief Met à jour l'état du transfert automatique
 */
void http_status_update_transfert(bool actif, float volume_session, uint32_t target);

/**
 * @brief Met à jour l'état du brassage automatique
 */
void http_status_update_brassage(bool actif, uint32_t temps_restant, uint8_t pct, const char* label);

#ifdef __cplusplus
}
#endif

#endif // HTTP_SERVER_H
