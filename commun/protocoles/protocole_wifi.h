/**
 * @file protocole_wifi.h
 * @brief Gestion WiFi AP/STA avec failover automatique MASTER/SLAVE.
 */
#ifndef PROTOCOLE_WIFI_H
#define PROTOCOLE_WIFI_H

#include "types_pulverisateur.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise le sous-système WiFi et détermine le rôle (MASTER/SLAVE).
 *
 * Séquence :
 *   1. Init NVS + netif + event loop
 *   2. Scan rapide pour chercher PULVE_AP
 *   3. Si trouvé → mode STA (SLAVE)
 *   4. Sinon → mode AP (MASTER)
 *
 * @param[out] role_out  Rôle déterminé après initialisation.
 * @return ESP_OK en cas de succès.
 */
esp_err_t wifi_initialiser(role_reseau_t *role_out);

/**
 * @brief demande de failover.
 */
bool wifi_failover_est_demande(void);

/**
 * @brief Retourne le rôle réseau actuel.
 */
role_reseau_t wifi_obtenir_role(void);

/**
 * @brief Vérifie si la connexion WiFi est active.
 */
bool wifi_est_connecte(void);

/**
 * @brief Lance le failover : bascule de SLAVE à MASTER.
 *
 * Appelé quand la connexion au MASTER est perdue depuis trop longtemps.
 * Arrête le mode STA, démarre le mode AP.
 */
esp_err_t wifi_failover_vers_master(void);

/**
 * @brief Callback interne pour les événements WiFi.
 * Gère la reconnexion et la détection de perte.
 */
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOLE_WIFI_H */
