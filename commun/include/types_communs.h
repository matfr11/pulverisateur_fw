/**
 * @file types_communs.h
 * @brief Définitions des types, structures et énumérations communes au système
 * @version 1.1
 * @date 2026-02-05
 */

#ifndef TYPES_COMMUNS_H
#define TYPES_COMMUNS_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// VERSIONING PROTOCOLE
// ============================================================================

#define VERSION_PROTOCOLE_MAJEURE 1
#define VERSION_PROTOCOLE_MINEURE 1
#define VERSION_PROTOCOLE_PATCH   0

// ============================================================================
// ÉNUMÉRATIONS - RÔLES ET ÉTATS
// ============================================================================

/**
 * @brief Rôle de la carte dans le système
 */
typedef enum {
    ROLE_INCONNU = 0,
    ROLE_MASTER,        // Génère WiFi et MQTT broker
    ROLE_SLAVE          // Se connecte au master
} role_carte_t;

/**
 * @brief Type de carte matérielle
 */
typedef enum {
    TYPE_CARTE_INCONNU = 0,
    TYPE_CARTE_AVANT,   // ESP32 4 relais + débitmètre
    TYPE_CARTE_ARRIERE, // ESP32 8 relais
    TYPE_CARTE_ECRAN    // ESP32-P4-C6 + écran
} type_carte_t;

/**
 * @brief État général du système
 */
typedef enum {
    ETAT_SYSTEME_INITIALISATION = 0,
    ETAT_SYSTEME_ACTIF,
    ETAT_SYSTEME_DEGRADE,
    ETAT_SYSTEME_ERREUR_CRITIQUE
} etat_systeme_t;

/**
 * @brief État de la pompe
 */
typedef enum {
    ETAT_POMPE_ARRET = 0,
    ETAT_POMPE_MARCHE
} etat_pompe_t;

/**
 * @brief Position de la vanne 3 voies
 */
typedef enum {
    POSITION_VANNE_3V_BRASSAGE = 0,
    POSITION_VANNE_3V_TRANSFERT
} position_vanne_3voies_t;

/**
 * @brief État d'une vanne 3 fils (2 relais)
 */
typedef enum {
    ETAT_VANNE_3FILS_INACTIF = 0,   // Tous relais OFF
    ETAT_VANNE_3FILS_OUVERTURE,     // Relais ouverture ON
    ETAT_VANNE_3FILS_FERMETURE,     // Relais fermeture ON
    ETAT_VANNE_3FILS_OUVERTE,       // Position atteinte
    ETAT_VANNE_3FILS_FERMEE,        // Position atteinte
    ETAT_VANNE_3FILS_TIMEOUT        // Erreur timeout
} etat_vanne_3fils_t;

/**
 * @brief Mode d'automatisme de transfert
 */
typedef enum {
    MODE_TRANSFERT_INACTIF = 0,
    MODE_TRANSFERT_AVEC_SONDE,      // Utilise sonde niveau cuve arrière
    MODE_TRANSFERT_SANS_SONDE       // Volume fixe défini
} mode_transfert_t;

/**
 * @brief État de l'automatisme de transfert
 */
typedef enum {
    ETAT_TRANSFERT_INACTIF = 0,
    ETAT_TRANSFERT_EN_COURS,
    ETAT_TRANSFERT_TERMINE,
    ETAT_TRANSFERT_PAUSE_CUVE_VIDE,
    ETAT_TRANSFERT_ERREUR        
} etat_transfert_t;

/**
 * @brief État de l'automatisme de brassage
 */
typedef enum {
    ETAT_BRASSAGE_INACTIF = 0,
    ETAT_BRASSAGE_MARCHE,
    ETAT_BRASSAGE_PAUSE,
    ETAT_BRASSAGE_SUSPENDU_TRANSFERT,
    ETAT_BRASSAGE_SUSPENDU
} etat_brassage_t;

/**
 * @brief Détection de cuve avant vide
 */
typedef enum {
    ETAT_CUVE_AVANT_NORMALE = 0,
    ETAT_CUVE_AVANT_DETECTION_VIDE,
    ETAT_CUVE_AVANT_VIDE
} etat_cuve_avant_t;

/**
 * @brief Type d'alerte sécurité
 * @note AJOUTÉ v1.1 - Nécessaire pour mqtt_publier_alerte_securite()
 */
typedef enum {
    ALERTE_SECURITE_AUCUNE = 0,
    ALERTE_SECURITE_CUVE_VIDE,
    ALERTE_SECURITE_TIMEOUT_VANNE_2M,
    ALERTE_SECURITE_TIMEOUT_VANNE_BOUT,
    ALERTE_SECURITE_DEBIT_ANORMAL,
    ALERTE_SECURITE_PERTE_COMMUNICATION,
    ALERTE_SECURITE_ARRET_URGENCE
} type_alerte_securite_t;

// ============================================================================
// STRUCTURES DE DONNÉES
// ============================================================================

/**
 * @brief Configuration système partagée
 */
typedef struct {
    uint32_t version_config;        // Numéro de version de config
    
    // Sécurités
    struct {
        float seuil_debit_cuve_vide;    // L/min en dessous = cuve vide
        uint32_t delai_detection_ms;    // Délai avant détection cuve vide
        uint32_t timeout_vanne_3fils_ms; // Timeout sécurité vannes
    } securite;
    
    // Automatismes
    struct {
        float volume_transfert_litres;  // Volume à transférer (mode sans sonde)
        uint32_t temps_brassage_on_sec; // Durée marche brassage
        uint32_t temps_brassage_pause_sec; // Durée pause brassage
        float seuil_niveau_bas_litres;  // Seuil déclenchement transfert (avec sonde)
        float volume_cible_transfert_litres; // Volume par cycle transfert
    } automatismes;
    
    // Capteurs
    struct {
        float facteur_k_debitmetre;     // Impulsions par litre
        bool sonde_niveau_disponible;   // Présence capteur niveau arrière
    } capteurs;
    
    // Actionneurs
    struct {
        bool phares_auto;               // Activation auto des phares
    } actionneurs;
    
} configuration_systeme_t;

/**
 * @brief État des actionneurs carte AVANT
 */
typedef struct {
    etat_pompe_t pompe;
    position_vanne_3voies_t vanne_3voies;
    bool phares_avant;
    bool reserve;
} actionneurs_avant_t;

/**
 * @brief État des actionneurs carte ARRIÈRE
 */
typedef struct {
    etat_vanne_3fils_t vanne_2m;
    etat_vanne_3fils_t vanne_bout_rampe;
    bool phares_arriere;
    bool reserve;
} actionneurs_arriere_t;

/**
 * @brief Données du débitmètre
 */
typedef struct {
    uint32_t impulsions_totales;    // Compteur impulsions
    float debit_instantane_lpm;     // L/min
    float volume_total_litres;      // Volume total mesuré
    uint32_t timestamp_derniere_impulsion_ms;
} donnees_debitmetre_t;

/**
 * @brief Données sonde niveau (optionnelle)
 */
typedef struct {
    bool disponible;
    float niveau_litres;
    uint32_t timestamp_ms;
} donnees_niveau_t;

/**
 * @brief État complet des automatismes
 */
typedef struct {
    mode_transfert_t mode_transfert;
    etat_transfert_t etat_transfert;
    etat_brassage_t etat_brassage;
    etat_cuve_avant_t etat_cuve_avant;
    
    float volume_transfere_cycle_litres;
    uint32_t timestamp_debut_cycle_ms;
} etat_automatismes_t;

/**
 * @brief État complet du système (snapshot)
 */
typedef struct {
    // Identité
    role_carte_t role;
    type_carte_t type_carte;
    char identifiant[32];
    
    // État général
    etat_systeme_t etat_systeme;
    uint32_t uptime_ms;
    
    // Configuration active
    configuration_systeme_t config;
    
    // Actionneurs
    actionneurs_avant_t actionneurs_avant;
    actionneurs_arriere_t actionneurs_arriere;
    
    // Capteurs
    donnees_debitmetre_t debitmetre;
    donnees_niveau_t niveau_arriere;
    
    // Automatismes
    etat_automatismes_t automatismes;
    
    // Diagnostics
    struct {
        bool wifi_connecte;
        bool mqtt_connecte;
        int8_t rssi_wifi;
        uint32_t erreurs_mqtt;
        uint32_t erreurs_capteurs;
    } diagnostics;
    
} etat_complet_systeme_t;

/**
 * @brief Commande générique
 */
typedef struct {
    char source[32];            // Origine de la commande
    uint32_t timestamp_ms;      // Horodatage
    uint32_t id_sequence;       // Numéro de séquence
} commande_base_t;

// ============================================================================
// CONSTANTES
// ============================================================================

// Timeouts
#define TIMEOUT_WIFI_CONNEXION_MS       10000
#define TIMEOUT_MQTT_RECONNEXION_MS     5000
#define TIMEOUT_REPONSE_COMMANDE_MS     2000
#define TIMEOUT_VANNE_3FILS_DEFAUT_MS   30000

// Débitmètre
#define DEBIT_MIN_DETECTION_LPM         0.5f
#define PERIODE_CALCUL_DEBIT_MS         500

// Valeurs par défaut configuration
#define CONFIG_DEFAULT_SEUIL_DEBIT_VIDE         1.2f
#define CONFIG_DEFAULT_DELAI_DETECTION_VIDE_MS  3000
#define CONFIG_DEFAULT_VOLUME_TRANSFERT_L       120.0f
#define CONFIG_DEFAULT_TEMPS_BRASSAGE_ON_SEC    600
#define CONFIG_DEFAULT_TEMPS_BRASSAGE_PAUSE_SEC 300
#define CONFIG_DEFAULT_FACTEUR_K_DEBITMETRE     4.72f

#endif // TYPES_COMMUNS_H