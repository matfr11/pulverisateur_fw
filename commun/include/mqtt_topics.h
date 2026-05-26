/**
 * @file mqtt_topics.h
 * @brief Définition centralisée de TOUS les topics MQTT du système.
 *
 * Convention :
 *   - États : retain=true,  QoS 0
 *   - Commandes : retain=false, QoS 1
 *   - Configuration : retain=true, QoS 1
 */
#ifndef MQTT_TOPICS_H
#define MQTT_TOPICS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 * PRÉFIXE RACINE
 * ==================================================================== */
#define MQTT_PREFIXE                    "pulverisateur"

/* ====================================================================
 * ÉTATS (publiés par chaque carte, retain=true)
 * ==================================================================== */
#define TOPIC_ETAT_AVANT                MQTT_PREFIXE "/etat/avant"
#define TOPIC_ETAT_ARRIERE              MQTT_PREFIXE "/etat/arriere"

/* ====================================================================
 * COMMANDES (publiées par les interfaces, retain=false, QoS 1)
 * ==================================================================== */
#define TOPIC_CMD_AVANT                 MQTT_PREFIXE "/cmd/avant"
#define TOPIC_CMD_ARRIERE               MQTT_PREFIXE "/cmd/arriere"

/* Commande d'arrêt d'urgence global */
#define TOPIC_CMD_ARRET_URGENCE         MQTT_PREFIXE "/cmd/arret_urgence"

/* ====================================================================
 * CONFIGURATION (retain=true, QoS 1)
 * ==================================================================== */
#define TOPIC_CONFIG_INSTANTANE          MQTT_PREFIXE "/configuration/instantane"
#define TOPIC_CONFIG_MISE_A_JOUR         MQTT_PREFIXE "/configuration/mise_a_jour"
#define TOPIC_CONFIG_DEMANDE             MQTT_PREFIXE "/configuration/demande"
#define TOPIC_CONFIG_VERSION             MQTT_PREFIXE "/configuration/version"

/* ====================================================================
 * SYSTÈME
 * ==================================================================== */
#define TOPIC_SYSTEME_VERSION_PROTO      MQTT_PREFIXE "/systeme/version_protocole"
#define TOPIC_SYSTEME_ROLE               MQTT_PREFIXE "/systeme/role"
#define TOPIC_SYSTEME_HEARTBEAT          MQTT_PREFIXE "/systeme/heartbeat"

/* Souscriptions wildcard */
#define TOPIC_SUB_CMD_AVANT              TOPIC_CMD_AVANT
#define TOPIC_SUB_CMD_ARRIERE            TOPIC_CMD_ARRIERE
#define TOPIC_SUB_CONFIG_ALL             MQTT_PREFIXE "/configuration/#"
#define TOPIC_SUB_CMD_URGENCE            TOPIC_CMD_ARRET_URGENCE
#define TOPIC_SUB_ETAT_ALL               MQTT_PREFIXE "/etat/#"

/* ====================================================================
 * CLÉS JSON POUR LES COMMANDES
 *
 * Format d'une commande :
 *   { "cmd": "<CLÉ>", "val": "<VALEUR_OPTIONNELLE>" }
 * ==================================================================== */
#define JSON_CMD_KEY                     "cmd"
#define JSON_CMD_VAL                     "val"

/* Commandes carte AVANT */
#define CMD_STR_POMPE_TOGGLE             "pompe_toggle"
#define CMD_STR_V3V_TOGGLE               "v3v_toggle"
#define CMD_STR_PHARES_AV_TOGGLE         "phares_av_toggle"
#define CMD_STR_AUTO_TRANSFERT           "auto_transfert"
#define CMD_STR_AUTO_BRASSAGE            "auto_brassage"

/* Commandes carte ARRIÈRE */
#define CMD_STR_V2M_OUVRIR               "v2m_ouvrir"
#define CMD_STR_V2M_FERMER               "v2m_fermer"
#define CMD_STR_V2M_STOP                 "v2m_stop"
#define CMD_STR_VBR_OUVRIR               "vbr_ouvrir"
#define CMD_STR_VBR_FERMER               "vbr_fermer"
#define CMD_STR_VBR_STOP                 "vbr_stop"
#define CMD_STR_PHARES_AR_TOGGLE         "phares_ar_toggle"

/* Valeurs pour les automatismes */
#define CMD_VAL_ACTIVER                  "A"
#define CMD_VAL_ARRETER                  "S"

/* ====================================================================
 * RÉSEAU
 * ==================================================================== */
#define WIFI_SSID_AP                     "PULVE_AP"
#define WIFI_PASS_AP                     "pulve2025"
#define WIFI_CHANNEL_AP                  6
#define WIFI_MAX_CONN_AP                 4

#define MQTT_BROKER_URI_MASTER           "mqtt://192.168.4.1:1883"
#define MQTT_BROKER_PORT                 1883

/* Délai avant qu'un SLAVE tente de devenir MASTER (ms) */
#define WIFI_FAILOVER_TIMEOUT_MS         10000

/* Intervalle heartbeat (ms) */
#define HEARTBEAT_INTERVAL_MS            5000

#ifdef __cplusplus
}
#endif

#endif /* MQTT_TOPICS_H */
