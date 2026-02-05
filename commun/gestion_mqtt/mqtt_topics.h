/**
 * @file mqtt_topics.h
 * @brief Définition centralisée de tous les topics MQTT du système
 * @version 1.0
 * @date 2026-02-02
 */

#ifndef MQTT_TOPICS_H
#define MQTT_TOPICS_H
#include <cstring>

// ============================================================================
// PRÉFIXE GLOBAL
// ============================================================================

#define MQTT_PREFIX "pulverisateur"

// ============================================================================
// SYSTÈME
// ============================================================================

#define TOPIC_ETAT_SYSTEME              MQTT_PREFIX "/systeme/etat"

// Version et compatibilité
#define TOPIC_SYSTEME_VERSION           MQTT_PREFIX "/systeme/version_protocole"
#define TOPIC_SYSTEME_ETAT              MQTT_PREFIX "/systeme/etat"
#define TOPIC_SYSTEME_ROLE              MQTT_PREFIX "/systeme/role"
#define TOPIC_SYSTEME_DIAGNOSTICS       MQTT_PREFIX "/systeme/diagnostics"
#define TOPIC_SYSTEME_ERREUR            MQTT_PREFIX "/systeme/erreur"

// Présence cartes (heartbeat)
#define TOPIC_PRESENCE_AVANT            MQTT_PREFIX "/presence/carte_avant"
#define TOPIC_PRESENCE_ARRIERE          MQTT_PREFIX "/presence/carte_arriere"
#define TOPIC_PRESENCE_ECRAN            MQTT_PREFIX "/presence/carte_ecran"

// ============================================================================
// CONFIGURATION
// ============================================================================

#define TOPIC_CONFIG_INSTANTANE         MQTT_PREFIX "/configuration/instantane"
#define TOPIC_CONFIG_MISE_A_JOUR        MQTT_PREFIX "/configuration/mise_a_jour"
#define TOPIC_CONFIG_DEMANDE            MQTT_PREFIX "/configuration/demande"
#define TOPIC_CONFIG_VERSION            MQTT_PREFIX "/configuration/version"
#define TOPIC_CONFIG_ACK                MQTT_PREFIX "/configuration/ack"

// ============================================================================
// COMMANDES MANUELLES - CARTE AVANT
// ============================================================================

#define TOPIC_CMD_POMPE                 MQTT_PREFIX "/commandes/avant/pompe"
#define TOPIC_CMD_VANNE_3VOIES          MQTT_PREFIX "/commandes/avant/vanne_3voies"
#define TOPIC_CMD_PHARES_AVANT          MQTT_PREFIX "/commandes/avant/phares"

// ============================================================================
// COMMANDES MANUELLES - CARTE ARRIÈRE
// ============================================================================

#define TOPIC_CMD_VANNE_2M              MQTT_PREFIX "/commandes/arriere/vanne_2m"
#define TOPIC_CMD_VANNE_BOUT_RAMPE      MQTT_PREFIX "/commandes/arriere/vanne_bout_rampe"
#define TOPIC_CMD_PHARES_ARRIERE        MQTT_PREFIX "/commandes/arriere/phares"

// ============================================================================
// ÉTATS ACTIONNEURS
// ============================================================================

// Carte avant
#define TOPIC_ETAT_POMPE                MQTT_PREFIX "/etats/avant/pompe"
#define TOPIC_ETAT_VANNE_3VOIES         MQTT_PREFIX "/etats/avant/vanne_3voies"
#define TOPIC_ETAT_PHARES_AVANT         MQTT_PREFIX "/etats/avant/phares"

// Carte arrière
#define TOPIC_ETAT_VANNE_2M             MQTT_PREFIX "/etats/arriere/vanne_2m"
#define TOPIC_ETAT_VANNE_BOUT_RAMPE     MQTT_PREFIX "/etats/arriere/vanne_bout_rampe"
#define TOPIC_ETAT_PHARES_ARRIERE       MQTT_PREFIX "/etats/arriere/phares"

// ============================================================================
// CAPTEURS
// ============================================================================

#define TOPIC_CAPTEUR_DEBITMETRE        MQTT_PREFIX "/capteurs/debitmetre"
#define TOPIC_CAPTEUR_NIVEAU_ARRIERE    MQTT_PREFIX "/capteurs/niveau_arriere"

// ============================================================================
// AUTOMATISMES
// ============================================================================

// Commandes automatismes
#define TOPIC_CMD_TRANSFERT_ACTIVER     MQTT_PREFIX "/automatismes/transfert/activer"
#define TOPIC_CMD_TRANSFERT_DESACTIVER  MQTT_PREFIX "/automatismes/transfert/desactiver"
#define TOPIC_CMD_BRASSAGE_ACTIVER      MQTT_PREFIX "/automatismes/brassage/activer"
#define TOPIC_CMD_BRASSAGE_DESACTIVER   MQTT_PREFIX "/automatismes/brassage/desactiver"

// États automatismes
#define TOPIC_ETAT_TRANSFERT            MQTT_PREFIX "/automatismes/transfert/etat"
#define TOPIC_ETAT_BRASSAGE             MQTT_PREFIX "/automatismes/brassage/etat"
#define TOPIC_ETAT_CUVE_AVANT           MQTT_PREFIX "/automatismes/cuve_avant/etat"

// ============================================================================
// SÉCURITÉS
// ============================================================================

#define TOPIC_SECURITE_CUVE_VIDE        MQTT_PREFIX "/securites/cuve_avant_vide"
#define TOPIC_SECURITE_TIMEOUT_VANNE    MQTT_PREFIX "/securites/timeout_vanne"
#define TOPIC_SECURITE_INTERLOCK        MQTT_PREFIX "/securites/violation_interlock"

// ============================================================================
// ACCUSÉS DE RÉCEPTION
// ============================================================================

#define TOPIC_ACK_COMMANDE              MQTT_PREFIX "/ack/commande"
#define TOPIC_ACK_AUTOMATISME           MQTT_PREFIX "/ack/automatisme"

// ============================================================================
// FONCTIONS UTILITAIRES
// ============================================================================

/**
 * @brief Construit un topic pour une carte spécifique
 * @param buffer Buffer de sortie
 * @param buffer_size Taille du buffer
 * @param base_topic Topic de base
 * @param carte_id Identifiant de la carte
 */
static inline void mqtt_build_topic_carte(char* buffer, size_t buffer_size, 
                                          const char* base_topic, const char* carte_id) {
    snprintf(buffer, buffer_size, "%s/%s", base_topic, carte_id);
}

/**
 * @brief Extrait l'identifiant de carte d'un topic
 * @param topic Topic complet
 * @param prefix Préfixe à retirer
 * @param buffer Buffer pour stocker l'ID
 * @param buffer_size Taille du buffer
 * @return true si extraction réussie
 */
static inline bool mqtt_extract_carte_id(const char* topic, const char* prefix,
                                         char* buffer, size_t buffer_size) {
    if (strncmp(topic, prefix, strlen(prefix)) != 0) {
        return false;
    }
    const char* id_start = topic + strlen(prefix);
    if (*id_start == '/') id_start++;
    strncpy(buffer, id_start, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    return true;
}

#endif // MQTT_TOPICS_H
