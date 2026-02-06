/**
 * @file protocole_mqtt.h
 * @brief Client MQTT + sérialisation/désérialisation JSON des états et commandes.
 */
#ifndef PROTOCOLE_MQTT_H
#define PROTOCOLE_MQTT_H

#include "types_pulverisateur.h"
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 * CALLBACK DE RÉCEPTION
 * ==================================================================== */

/** Callback appelé quand une commande est reçue */
typedef void (*mqtt_callback_commande_t)(const char *topic, const char *payload, int len);

/** Callback appelé quand la configuration est reçue */
typedef void (*mqtt_callback_config_t)(const configuration_t *config);

/** Callback appelé quand un état (avant ou arrière) est reçu via MQTT */
typedef void (*mqtt_callback_etat_t)(const char *topic, const char *payload, int len);

/* ====================================================================
 * INITIALISATION
 * ==================================================================== */

/**
 * @brief Initialise le client MQTT et se connecte au broker.
 *
 * @param broker_uri  URI du broker (ex: "mqtt://192.168.4.1:1883")
 * @param id_carte    Identifiant de la carte (pour le client ID)
 * @return ESP_OK en cas de succès.
 */
esp_err_t mqtt_initialiser(const char *broker_uri, carte_id_t id_carte);

/**
 * @brief Enregistre le callback pour les commandes reçues.
 */
void mqtt_enregistrer_callback_commande(mqtt_callback_commande_t cb);

/**
 * @brief Enregistre le callback pour la configuration reçue.
 */
void mqtt_enregistrer_callback_config(mqtt_callback_config_t cb);

/**
 * @brief Enregistre le callback pour les états reçus (AVANT / ARRIÈRE).
 * Nécessaire pour alimenter la Web UI depuis n'importe quelle carte MASTER.
 */
void mqtt_enregistrer_callback_etat(mqtt_callback_etat_t cb);

/**
 * @brief Souscrit aux topics d'état des autres cartes.
 * Appelé quand la carte devient MASTER et doit héberger la Web UI.
 */
void mqtt_souscrire_etats(void);

/**
 * @brief Vérifie si le client MQTT est connecté.
 */
bool mqtt_est_connecte(void);

/* ====================================================================
 * PUBLICATION DES ÉTATS
 * ==================================================================== */

/**
 * @brief Publie l'état complet de la carte AVANT en JSON (retain=true).
 */
esp_err_t mqtt_publier_etat_avant(const etat_carte_avant_t *etat);

/**
 * @brief Publie l'état complet de la carte ARRIÈRE en JSON (retain=true).
 */
esp_err_t mqtt_publier_etat_arriere(const etat_carte_arriere_t *etat);

/* ====================================================================
 * PUBLICATION DES COMMANDES
 * ==================================================================== */

/**
 * @brief Publie une commande vers la carte AVANT (retain=false, QoS 1).
 * @param cmd_str  Clé de commande (ex: CMD_STR_POMPE_TOGGLE)
 * @param val_str  Valeur optionnelle (NULL si pas de valeur)
 */
esp_err_t mqtt_publier_commande_avant(const char *cmd_str, const char *val_str);

/**
 * @brief Publie une commande vers la carte ARRIÈRE.
 */
esp_err_t mqtt_publier_commande_arriere(const char *cmd_str, const char *val_str);

/**
 * @brief Publie un arrêt d'urgence global.
 */
esp_err_t mqtt_publier_arret_urgence(void);

/* ====================================================================
 * CONFIGURATION
 * ==================================================================== */

/**
 * @brief Publie la configuration complète (retain=true, par le MASTER).
 */
esp_err_t mqtt_publier_configuration(const configuration_t *config);

/**
 * @brief Publie une demande de configuration (SLAVE → MASTER).
 */
esp_err_t mqtt_demander_configuration(void);

/**
 * @brief Publie une mise à jour de configuration (interface → MASTER).
 */
esp_err_t mqtt_publier_mise_a_jour_config(const configuration_t *config);

/* ====================================================================
 * SÉRIALISATION JSON
 * ==================================================================== */

/**
 * @brief Sérialise l'état de la carte AVANT en JSON.
 * @param etat   Pointeur vers la structure d'état.
 * @param buffer Buffer de sortie.
 * @param taille Taille du buffer.
 * @return Nombre de caractères écrits, ou -1 en erreur.
 */
int json_serialiser_etat_avant(const etat_carte_avant_t *etat, char *buffer, size_t taille);

/**
 * @brief Sérialise l'état de la carte ARRIÈRE en JSON.
 */
int json_serialiser_etat_arriere(const etat_carte_arriere_t *etat, char *buffer, size_t taille);

/**
 * @brief Sérialise la configuration en JSON.
 */
int json_serialiser_configuration(const configuration_t *config, char *buffer, size_t taille);

/**
 * @brief Désérialise une configuration depuis un JSON.
 * @return true si le parsing est réussi.
 */
bool json_deserialiser_configuration(const char *json_str, configuration_t *config_out);

/**
 * @brief Désérialise l'état carte AVANT depuis un JSON MQTT.
 * @return true si le parsing est réussi.
 */
bool json_deserialiser_etat_avant(const char *json_str, etat_carte_avant_t *etat_out);

/**
 * @brief Désérialise l'état carte ARRIÈRE depuis un JSON MQTT.
 * @return true si le parsing est réussi.
 */
bool json_deserialiser_etat_arriere(const char *json_str, etat_carte_arriere_t *etat_out);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOLE_MQTT_H */
