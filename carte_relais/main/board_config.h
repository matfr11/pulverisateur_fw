/**
 * @file board_config.h
 * @brief Configuration matérielle spécifique à chaque carte
 * 
 * Ce fichier permet de différencier la carte AVANT et ARRIÈRE
 * via des définitions de compilation.
 * 
 * Sélection au moment de la compilation :
 * - idf.py -D BOARD_TYPE=AVANT build
 * - idf.py -D BOARD_TYPE=ARRIERE build
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "types_communs.h"

// ============================================================================
// SÉLECTION TYPE DE CARTE (défini via CMake)
// ============================================================================

// Les valeurs possibles sont définies via -D au moment du build
// BOARD_TYPE_AVANT ou BOARD_TYPE_ARRIERE

#if defined(BOARD_TYPE_AVANT)
    #define TYPE_CARTE TYPE_CARTE_AVANT
    #define IDENTIFIANT_CARTE "AVANT"
#elif defined(BOARD_TYPE_ARRIERE)
    #define TYPE_CARTE TYPE_CARTE_ARRIERE
    #define IDENTIFIANT_CARTE "ARRIERE"
#else
    #error "BOARD_TYPE non défini ! Utilisez -D BOARD_TYPE_AVANT ou -D BOARD_TYPE_ARRIERE"
#endif

// ============================================================================
// CONFIGURATION GPIO - CARTE AVANT (4 relais + débitmètre)
// ============================================================================

#ifdef BOARD_TYPE_AVANT

// Relais (sortie)
#define GPIO_RELAIS_POMPE           GPIO_NUM_33
#define GPIO_RELAIS_VANNE_3VOIES    GPIO_NUM_32
#define GPIO_RELAIS_PHARES_AVANT    GPIO_NUM_25
#define GPIO_RELAIS_RESERVE         GPIO_NUM_26

// Capteurs (entrée)
#define GPIO_DEBITMETRE_IMPULSION   GPIO_NUM_13

// Configuration débitmètre
#define DEBITMETRE_PRESENTE         true
#define DEBITMETRE_PULL_MODE        GPIO_PULLUP_ONLY
#define DEBITMETRE_INTR_TYPE        GPIO_INTR_NEGEDGE

// Capacités de la carte
#define CAPACITE_POMPE              true
#define CAPACITE_VANNE_3VOIES       true
#define CAPACITE_DEBITMETRE         true
#define CAPACITE_AUTOMATISMES       true
#define CAPACITE_VANNES_3FILS       false

#define HAVE_HTTP_SERVER            1

#endif // BOARD_TYPE_AVANT

// ============================================================================
// CONFIGURATION GPIO - CARTE ARRIÈRE (8 relais)
// ============================================================================

#ifdef BOARD_TYPE_ARRIERE

// Relais vanne 2m (3 fils, 2 relais)
#define GPIO_VANNE_2M_OUVRIR        GPIO_NUM_32
#define GPIO_VANNE_2M_FERMER        GPIO_NUM_33

// Relais vanne bout de rampe (3 fils, 2 relais)
#define GPIO_VANNE_BOUT_OUVRIR      GPIO_NUM_25
#define GPIO_VANNE_BOUT_FERMER      GPIO_NUM_26

// Phares et réserve
#define GPIO_RELAIS_PHARES_ARRIERE  GPIO_NUM_27
#define GPIO_RELAIS_RESERVE_1       GPIO_NUM_14
#define GPIO_RELAIS_RESERVE_2       GPIO_NUM_12
#define GPIO_RELAIS_RESERVE_3       GPIO_NUM_13

// Capteur niveau (optionnel, future évolution)
#define GPIO_SONDE_NIVEAU           GPIO_NUM_44
#define SONDE_NIVEAU_PRESENTE       false

// Capacités de la carte
#define CAPACITE_POMPE              false
#define CAPACITE_VANNE_3VOIES       false
#define CAPACITE_DEBITMETRE         false
#define CAPACITE_AUTOMATISMES       false
#define CAPACITE_VANNES_3FILS       true

#define HAVE_HTTP_SERVER            1
#endif // BOARD_TYPE_ARRIERE

// ============================================================================
// CONFIGURATION WiFi (identique pour les deux cartes)
// ============================================================================

// Mode Access Point (quand MASTER)
#define WIFI_AP_SSID                "Pulve"
#define WIFI_AP_PASSWORD            "12345678"
#define WIFI_AP_CHANNEL             1
#define WIFI_AP_MAX_CONNECTIONS     4
#define WIFI_AP_BEACON_INTERVAL     100

// Mode Station (quand SLAVE)
#define WIFI_STA_SSID               WIFI_AP_SSID
#define WIFI_STA_PASSWORD           WIFI_AP_PASSWORD
#define WIFI_STA_MAX_RETRY          5

// Timeouts
#define WIFI_TIMEOUT_DETECTION_MS   3000    // Temps pour détecter un master existant
#define WIFI_TIMEOUT_CONNEXION_MS   10000   // Timeout connexion station

// ============================================================================
// CONFIGURATION MQTT
// ============================================================================

#define MQTT_BROKER_PORT            1883
#define MQTT_BROKER_IP_MASTER       "192.168.4.1"   // IP du master en mode AP
#define MQTT_KEEPALIVE_SEC          60
#define MQTT_RECONNECT_DELAY_MS     5000

// QoS par défaut
#define MQTT_QOS_COMMANDE           1
#define MQTT_QOS_ETAT               0
#define MQTT_QOS_SECURITE           1
#define MQTT_QOS_CONFIG             1

// Buffer sizes
#define MQTT_BUFFER_SIZE            2048
#define MQTT_MAX_TOPIC_LEN          128
#define MQTT_MAX_PAYLOAD_LEN        1024

// ============================================================================
// CONFIGURATION NVS (stockage persistant)
// ============================================================================

#define NVS_NAMESPACE_CONFIG        "config"
#define NVS_NAMESPACE_CALIBRATION   "calib"
#define NVS_NAMESPACE_SYSTEME       "systeme"

#define NVS_KEY_CONFIG_VERSION      "cfg_ver"
#define NVS_KEY_CONFIG_DATA         "cfg_data"
#define NVS_KEY_ROLE_PREFERENTIEL   "role_pref"
#define NVS_KEY_FACTEUR_K_DEBIT     "k_debit"

// ============================================================================
// VALEURS PAR DÉFAUT CAPTEURS
// ============================================================================

// Facteur K débitmètre (impulsions par litre)
#define CONFIG_DEFAULT_FACTEUR_K_DEBITMETRE  4.72f

// Timeout vanne 3 fils (millisecondes)
#define TIMEOUT_VANNE_3FILS_DEFAUT_MS  30000

// ============================================================================
// CONFIGURATION TEMPORISATION ET TÂCHES
// ============================================================================

// Périodes de mise à jour automatismes (millisecondes)
#define PERIODE_MAJ_AUTOMATISMES    1000

// Priorités tâches FreeRTOS
#define TASK_PRIORITY_MQTT          5
#define TASK_PRIORITY_WIFI          5
#define TASK_PRIORITY_CAPTEURS      4
#define TASK_PRIORITY_ACTIONNEURS   4
#define TASK_PRIORITY_AUTOMATISMES  3
#define TASK_PRIORITY_UI            2

// Tailles de pile
#define TASK_STACK_MQTT             4096
#define TASK_STACK_WIFI             4096
#define TASK_STACK_CAPTEURS         3072
#define TASK_STACK_ACTIONNEURS      3072
#define TASK_STACK_AUTOMATISMES     3072
#define TASK_STACK_UI               2048

// Périodes de tâches (ms)
#define PERIODE_LECTURE_CAPTEURS    100
#define PERIODE_MAJ_ACTIONNEURS     50
#define PERIODE_AUTOMATISMES        200
#define PERIODE_HEARTBEAT           5000
#define PERIODE_PUBLICATION_ETAT    1000

// ============================================================================
// MODE SIMULATION (pour tests sans matériel)
// ============================================================================

#ifdef CONFIG_MODE_SIMULATION
    #define MODE_SIMULATION         true
    #warning "Compilation en MODE SIMULATION - capteurs et actionneurs virtuels"
#else
    #define MODE_SIMULATION         false
#endif

// ============================================================================
// CONFIGURATION DEBUG
// ============================================================================

#define LOG_TAG_MAIN                "MAIN"
#define LOG_TAG_WIFI                "WIFI"
#define LOG_TAG_MQTT                "MQTT"
#define LOG_TAG_ACTIONNEURS         "ACTIONNEUR"
#define LOG_TAG_CAPTEURS            "CAPTEUR"
#define LOG_TAG_AUTOMATISMES        "AUTO"
#define LOG_TAG_SECURITES           "SECURITE"
#define LOG_TAG_CONFIG              "CONFIG"

// Niveaux de log
#ifdef CONFIG_LOG_LEVEL_DEBUG
    #define LOG_LEVEL_DEFAULT       ESP_LOG_DEBUG
#else
    #define LOG_LEVEL_DEFAULT       ESP_LOG_INFO
#endif

// ============================================================================
// ASSERTIONS DE COMPILATION
// ============================================================================

// Vérifications de cohérence
#ifdef BOARD_TYPE_AVANT
    _Static_assert(CAPACITE_DEBITMETRE == true, "Carte AVANT doit avoir débitmètre");
    _Static_assert(CAPACITE_AUTOMATISMES == true, "Carte AVANT doit gérer automatismes");
#endif

#ifdef BOARD_TYPE_ARRIERE
    _Static_assert(CAPACITE_VANNES_3FILS == true, "Carte ARRIERE doit avoir vannes 3 fils");
    _Static_assert(CAPACITE_AUTOMATISMES == false, "Carte ARRIERE ne gère PAS automatismes");
#endif

#endif // BOARD_CONFIG_H
