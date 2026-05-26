/**
 * @file protocole_wifi.h
 * @brief Gestion WiFi : Point d'Accès (carte serveur) ou Client STA (cartes relais).
 *
 * Architecture simplifiée avec carte serveur dédiée :
 *
 *   CARTE_SERVEUR  → démarre toujours en Point d'Accès (AP).
 *                    Crée le réseau "PULVE_AP" auquel les relais se connectent.
 *                    Rôle fixe : ROLE_MASTER. Jamais de failover.
 *
 *   CARTE_AVANT    → se connecte toujours au réseau "PULVE_AP" (mode STA).
 *   CARTE_ARRIERE  → idem. Rôle fixe : ROLE_SLAVE.
 *                    En cas de déconnexion, reconnexion automatique.
 *
 * Cette architecture élimine les conflits master/slave qui se produisaient
 * lors des redémarrages simultanés dans l'ancienne version.
 */
#ifndef PROTOCOLE_WIFI_H
#define PROTOCOLE_WIFI_H

#include "types_pulverisateur.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise le WiFi selon le type de carte.
 *
 * Pour la carte serveur (est_serveur = true) :
 *   → Démarre directement en mode Point d'Accès, sans scanner le réseau.
 *   → Crée le réseau "PULVE_AP" (IP serveur : 192.168.4.1).
 *   → Écrit ROLE_MASTER dans role_out.
 *
 * Pour les cartes relais (est_serveur = false) :
 *   → Se connecte au réseau "PULVE_AP" créé par la carte serveur.
 *   → Attend jusqu'à 15 secondes. Si le serveur n'est pas encore prêt,
 *     les tentatives de connexion continuent automatiquement en arrière-plan.
 *   → Écrit ROLE_SLAVE dans role_out.
 *
 * @param[out] role_out    Reçoit ROLE_MASTER (serveur) ou ROLE_SLAVE (relais).
 * @param[in]  est_serveur Passer A_EST_SERVEUR (défini dans board_config.h).
 *                         true = carte serveur, false = carte relais.
 * @return ESP_OK si l'initialisation s'est bien passée.
 */
esp_err_t wifi_initialiser(role_reseau_t *role_out, bool est_serveur);

/**
 * @brief Retourne le rôle réseau actuel de cette carte.
 *
 * Toujours ROLE_MASTER pour le serveur, ROLE_SLAVE pour les relais.
 */
role_reseau_t wifi_obtenir_role(void);

/**
 * @brief Retourne true si la connexion WiFi est active.
 *
 * Pour le serveur (AP) : true dès que le point d'accès est démarré.
 * Pour les relais (STA) : true quand connecté au serveur et IP obtenue.
 */
bool wifi_est_connecte(void);

/**
 * @brief Handler interne des événements WiFi (connexion, déconnexion, etc.)
 *
 * Enregistré automatiquement par wifi_initialiser(). Ne pas appeler directement.
 * Gère la reconnexion automatique des cartes relais en cas de perte de signal.
 */
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOLE_WIFI_H */
