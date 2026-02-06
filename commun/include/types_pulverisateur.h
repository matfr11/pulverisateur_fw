/**
 * @file types_pulverisateur.h
 * @brief Types, structures, enums et constantes partagés par toutes les cartes.
 *
 * Ce fichier est LA RÉFÉRENCE pour toutes les définitions de données du système.
 * Il est inclus par le code commun, les cartes relais et la carte écran.
 */
#ifndef TYPES_PULVERISATEUR_H
#define TYPES_PULVERISATEUR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 * VERSION DU PROTOCOLE
 * ==================================================================== */
#define VERSION_PROTOCOLE       1
#define VERSION_FIRMWARE        "3.0.0"

/* ====================================================================
 * IDENTIFICATION DES CARTES
 * ==================================================================== */
typedef enum {
    CARTE_ID_AVANT   = 0x01,
    CARTE_ID_ARRIERE = 0x02,
    CARTE_ID_ECRAN   = 0x03,
} carte_id_t;

/* ====================================================================
 * RÔLE RÉSEAU (MASTER / SLAVE)
 * ==================================================================== */
typedef enum {
    ROLE_INDEFINI = 0,
    ROLE_MASTER,        /* Crée le WiFi AP + héberge le broker MQTT */
    ROLE_SLAVE,         /* Se connecte au WiFi AP existant */
} role_reseau_t;

/* ====================================================================
 * ÉTATS SYSTÈME
 * ==================================================================== */
typedef enum {
    ETAT_SYS_INITIALISATION = 0,
    ETAT_SYS_SYNCHRONISATION,   /* Attente des messages retain MQTT */
    ETAT_SYS_OPERATIONNEL,
    ETAT_SYS_DEGRADE,           /* Erreur détectée, manuels actifs */
    ETAT_SYS_HORS_LIGNE,
} etat_systeme_t;

/* ====================================================================
 * ÉTATS DES ACTIONNEURS
 * ==================================================================== */

/* Pompe */
typedef enum {
    POMPE_ARRETEE = 0,
    POMPE_EN_MARCHE,
} etat_pompe_t;

/* Vanne 3 voies */
typedef enum {
    V3V_BRASSAGE  = 0,   /* Relais OFF = circuit brassage */
    V3V_TRANSFERT = 1,   /* Relais ON  = circuit transfert */
} etat_vanne_3v_t;

/* Vannes motorisées (3 fils : ouvrir / fermer / stop) */
typedef enum {
    VANNE_STOP    = 0,    /* Aucun relais actif */
    VANNE_OUVRE   = 1,    /* Relais ouvrir actif */
    VANNE_FERME   = 2,    /* Relais fermer actif */
} commande_vanne_t;

/* État réel déduit d'une vanne motorisée */
typedef enum {
    VANNE_ETAT_INCONNU = 0,
    VANNE_ETAT_EN_OUVERTURE,
    VANNE_ETAT_EN_FERMETURE,
    VANNE_ETAT_ARRETEE,
    VANNE_ETAT_TIMEOUT,          /* Timeout de sécurité déclenché */
} etat_vanne_mot_t;

/* ====================================================================
 * ÉTATS DES AUTOMATISMES
 * ==================================================================== */

/* Transfert automatique */
typedef enum {
    AUTO_TR_INACTIF = 0,
    AUTO_TR_EN_COURS,
    AUTO_TR_TERMINE,
    AUTO_TR_ERREUR,
    AUTO_TR_CUVE_VIDE,          /* Arrêt car cuve avant vide */
} etat_auto_transfert_t;

/* Brassage automatique */
typedef enum {
    AUTO_BR_INACTIF = 0,
    AUTO_BR_MARCHE,              /* Pompe ON, cycle marche */
    AUTO_BR_PAUSE,               /* Pompe OFF, cycle pause */
    AUTO_BR_SUSPENDU,            /* Suspendu car transfert actif */
    AUTO_BR_ERREUR,
} etat_auto_brassage_t;

/* ====================================================================
 * SÉCURITÉS
 * ==================================================================== */
typedef enum {
    SEC_CUVE_OK = 0,
    SEC_CUVE_DETECTION,          /* Débit bas, compteur en cours */
    SEC_CUVE_VIDE,               /* Confirmé : cuve avant vide */
} etat_securite_cuve_t;

/* ====================================================================
 * STRUCTURES D'ÉTAT PUBLIÉES VIA MQTT
 * ==================================================================== */

/** État complet de la carte AVANT */
typedef struct {
    /* Actionneurs */
    etat_pompe_t        pompe;
    etat_vanne_3v_t     vanne_3v;
    bool                phares_avant;

    /* Capteurs */
    float               debit_instantane;    /* L/min */
    float               volume_session;      /* Litres cumulés depuis reset */
    bool                debitmetre_ok;       /* Capteur connecté */

    /* Automatismes */
    etat_auto_transfert_t   auto_transfert;
    etat_auto_brassage_t    auto_brassage;
    uint32_t                transfert_volume_cible;  /* L */
    float                   brassage_temps_restant;  /* minutes */
    float                   brassage_pourcentage;    /* 0-100 */
    char                    brassage_label[16];      /* "MARCHE" / "PAUSE" */

    /* Sécurités */
    etat_securite_cuve_t    securite_cuve;

    /* Système */
    etat_systeme_t      etat_systeme;
} etat_carte_avant_t;

/** État complet de la carte ARRIÈRE */
typedef struct {
    /* Vannes motorisées */
    etat_vanne_mot_t    vanne_2m;
    etat_vanne_mot_t    vanne_bout_rampe;

    /* Phares */
    bool                phares_arriere;

    /* Capteurs (futurs) */
    float               niveau_cuve_arriere;  /* 0-100 % ou litres */
    bool                sonde_niveau_ok;

    /* Système */
    etat_systeme_t      etat_systeme;
} etat_carte_arriere_t;

/* ====================================================================
 * CONFIGURATION PERSISTANTE (NVS + MQTT)
 * ==================================================================== */
typedef struct {
    uint32_t version;               /* Numéro de version incrémental */

    /* Sécurité */
    float   seuil_debit_cuve_vide;  /* L/min – en-dessous = suspect */
    uint32_t delai_detection_ms;    /* ms avant confirmation cuve vide */

    /* Automatismes */
    uint32_t volume_transfert;      /* Litres – volume cible transfert */
    uint32_t temps_brassage_on;     /* Secondes – durée pompe ON */
    uint32_t temps_brassage_off;    /* Secondes – durée pompe OFF */

    /* Capteurs */
    float   facteur_k_debitmetre;   /* Impulsions par litre */

    /* Actionneurs */
    uint32_t timeout_vanne_ms;      /* ms – timeout vannes motorisées */

    /* Système */
    uint32_t version_protocole;
} configuration_t;

/** Valeurs par défaut de la configuration */
#define CONFIG_DEFAUT { \
    .version                = 1, \
    .seuil_debit_cuve_vide  = 1.2f, \
    .delai_detection_ms     = 3000, \
    .volume_transfert       = 120, \
    .temps_brassage_on      = 600, \
    .temps_brassage_off     = 300, \
    .facteur_k_debitmetre   = 4.72f, \
    .timeout_vanne_ms       = 30000, \
    .version_protocole      = VERSION_PROTOCOLE, \
}

/* ====================================================================
 * COMMANDES MQTT (intentions envoyées par les interfaces)
 * ==================================================================== */
typedef enum {
    CMD_POMPE_TOGGLE = 0,
    CMD_VANNE_3V_TOGGLE,
    CMD_PHARES_AVANT_TOGGLE,
    CMD_PHARES_ARRIERE_TOGGLE,

    CMD_VANNE_2M_OUVRIR,
    CMD_VANNE_2M_FERMER,
    CMD_VANNE_2M_STOP,

    CMD_VANNE_BDR_OUVRIR,
    CMD_VANNE_BDR_FERMER,
    CMD_VANNE_BDR_STOP,

    CMD_AUTO_TRANSFERT_ACTIVER,
    CMD_AUTO_TRANSFERT_ARRETER,
    CMD_AUTO_BRASSAGE_ACTIVER,
    CMD_AUTO_BRASSAGE_ARRETER,

    CMD_ARRET_URGENCE,

    CMD_CONFIG_MISE_A_JOUR,
    CMD_CONFIG_DEMANDE,
} commande_type_t;

typedef struct {
    commande_type_t type;
    carte_id_t      source;         /* Qui envoie la commande */
    char            payload[128];   /* Données JSON optionnelles */
} commande_mqtt_t;

#ifdef __cplusplus
}
#endif

#endif /* TYPES_PULVERISATEUR_H */
