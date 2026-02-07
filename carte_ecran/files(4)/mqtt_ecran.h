#ifndef MQTT_ECRAN_H
#define MQTT_ECRAN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 * STRUCTURES D'ÉTAT (miroir des cartes relais)
 * ==================================================================== */

typedef struct {
    bool     pompe;
    bool     vanne_transfert;   /* true = transfert, false = brassage */
    bool     phares_avant;
    bool     debitmetre_ok;
    float    debit_lpm;
    float    volume_session;
    bool     cuve_vide;

    /* Automatismes */
    bool     auto_transfert;
    uint32_t transfert_cible;
    bool     auto_brassage;
    char     brassage_label[16];
    float    brassage_temps_restant;  /* secondes */
    float    brassage_pourcentage;
} etat_avant_t;

typedef struct {
    char     vanne_2m;          /* 'O', 'F', 'S', '?' */
    char     vanne_bdr;         /* 'O', 'F', 'S', '?' */
    bool     phares_arriere;
    float    niveau_ar;         /* litres */
    uint32_t niveau_ar_max;     /* litres max */
    bool     sonde_ok;
} etat_arriere_t;

typedef struct {
    etat_avant_t   avant;
    etat_arriere_t arriere;
    bool           link_avant;
    bool           link_arriere;
    bool           connecte;        /* MQTT connecté */
    char           master[4];       /* "AV" ou "AR" */
} etat_systeme_t;

/* ====================================================================
 * API
 * ==================================================================== */

/**
 * @brief Initialiser le client MQTT (après WiFi connecté).
 */
void mqtt_ecran_init(void);

/**
 * @brief Obtenir une copie thread-safe de l'état système.
 */
void mqtt_ecran_get_etat(etat_systeme_t *etat_out);

/**
 * @brief Publier une commande vers la carte AVANT.
 * @param cmd  Clé de commande (ex: "pompe_toggle")
 * @param val  Valeur optionnelle (ex: "A") ou NULL
 */
void mqtt_ecran_cmd_avant(const char *cmd, const char *val);

/**
 * @brief Publier une commande vers la carte ARRIÈRE.
 */
void mqtt_ecran_cmd_arriere(const char *cmd, const char *val);

/**
 * @brief Publier un arrêt d'urgence.
 */
void mqtt_ecran_arret_urgence(void);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_ECRAN_H */
