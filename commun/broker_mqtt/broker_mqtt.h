/**
 * @file broker_mqtt.h
 * @brief Broker MQTT embarqué minimaliste pour ESP32.
 *
 * Fonctionnalités :
 * - TCP listener sur port configurable (défaut 1883)
 * - MQTT 3.1.1 (CONNECT, PUBLISH, SUBSCRIBE, PINGREQ, DISCONNECT)
 * - QoS 0 et QoS 1 (PUBACK)
 * - Retain : stockage du dernier message par topic
 * - Wildcards : + (un niveau) et # (multi-niveaux)
 * - Max 8 clients simultanés
 * - Max 32 souscriptions totales
 * - Max 16 messages retain
 *
 * Limitations volontaires (pas nécessaire pour ce projet) :
 * - Pas de QoS 2
 * - Pas de Will message
 * - Pas de SSL/TLS
 * - Pas de persistance des sessions
 */
#ifndef BROKER_MQTT_H
#define BROKER_MQTT_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 * CONFIGURATION
 * ==================================================================== */
#define BROKER_PORT_DEFAUT          1883
#define BROKER_MAX_CLIENTS          8
#define BROKER_MAX_SOUSCRIPTIONS    32
#define BROKER_MAX_RETAIN           16
#define BROKER_TAILLE_BUFFER        2048    /* Buffer réception par client */
#define BROKER_TAILLE_TOPIC_MAX     128
#define BROKER_TAILLE_PAYLOAD_MAX   1024
#define BROKER_TASK_STACK           6144
#define BROKER_TASK_PRIORITE        5

/* ====================================================================
 * TYPES
 * ==================================================================== */
typedef struct {
    uint16_t port;              /**< Port d'écoute TCP (défaut 1883) */
} broker_mqtt_config_t;

/* ====================================================================
 * FONCTIONS PUBLIQUES
 * ==================================================================== */

/**
 * @brief Démarre le broker MQTT embarqué.
 *
 * Crée un socket TCP en écoute et lance une tâche FreeRTOS
 * qui accepte les connexions et traite les paquets MQTT.
 *
 * @param config  Configuration (NULL pour valeurs par défaut)
 * @return ESP_OK en cas de succès
 */
esp_err_t broker_mqtt_demarrer(const broker_mqtt_config_t *config);

/**
 * @brief Arrête proprement le broker.
 *
 * Ferme toutes les connexions clientes et le socket d'écoute.
 */
esp_err_t broker_mqtt_arreter(void);

/**
 * @brief Vérifie si le broker est actif.
 */
bool broker_mqtt_est_actif(void);

/**
 * @brief Retourne le nombre de clients actuellement connectés.
 */
int broker_mqtt_nb_clients(void);

#ifdef __cplusplus
}
#endif

#endif /* BROKER_MQTT_H */
