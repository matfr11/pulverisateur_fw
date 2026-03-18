/**
 * @file board_config.h
 * @brief Configuration matérielle de la carte relais.
 *
 * MODIFIER CE FICHIER POUR SÉLECTIONNER LA CARTE :
 *   - Décommenter CARTE_AVANT  pour la carte cuve avant (4 relais)
 *   - Décommenter CARTE_ARRIERE pour la carte cuve arrière (8 relais)
 *
 * Un seul define doit être actif à la fois !
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* ====================================================================
 * SÉLECTION DE LA CARTE (décommenter UNE seule ligne)
 * ==================================================================== */
#define CARTE_AVANT      1
//#define CARTE_ARRIERE    1

/* Vérification qu'une seule carte est sélectionnée */
#if defined(CARTE_AVANT) && defined(CARTE_ARRIERE)
    #error "Une seule carte peut être sélectionnée : CARTE_AVANT ou CARTE_ARRIERE"
#endif
#if !defined(CARTE_AVANT) && !defined(CARTE_ARRIERE)
    #error "Aucune carte sélectionnée : définir CARTE_AVANT ou CARTE_ARRIERE"
#endif

/* ====================================================================
 * MODE SIMULATION (décommenter pour activer)
 * ==================================================================== */
//#define MODE_SIMULATION  1

/* ====================================================================
 * CONFIGURATION GPIO - CARTE AVANT (4 relais)
 *
 * Carte relais 4 sorties ESP32 :
 *   Relais 1 → Pompe
 *   Relais 2 → Vanne 3 voies (OFF=brassage, ON=transfert)
 *   Relais 3 → Phares avant
 *   Relais 4 → Réserve
 * ==================================================================== */
#ifdef CARTE_AVANT

    #include "types_pulverisateur.h"

    #define ID_CARTE                CARTE_ID_AVANT

    /* GPIO des relais (ADAPTER selon votre carte relais) */
    #define GPIO_RELAIS_POMPE       GPIO_NUM_33
    #define GPIO_RELAIS_V3V         GPIO_NUM_32
    #define GPIO_RELAIS_PHARES_AV   GPIO_NUM_25
    #define GPIO_RELAIS_RESERVE_AV  GPIO_NUM_26

    /* Logique relais : LOW = actif (relais actif au niveau bas) */
    #define RELAIS_NIVEAU_ACTIF     1

    /* GPIO du débitmètre (entrée impulsions) */
    #define GPIO_DEBITMETRE         GPIO_NUM_13

    /* Pas de vannes motorisées sur la carte avant */
    #define A_DEBITMETRE            1
    #define A_VANNES_MOTORISEES     0
    #define A_SONDE_NIVEAU          0

    /* Web UI embarquée sur carte avant (MASTER) */
    #define A_WEB_UI                1

#endif /* CARTE_AVANT */

/* ====================================================================
 * CONFIGURATION GPIO - CARTE ARRIÈRE (8 relais)
 *
 * Carte relais 8 sorties ESP32 :
 *   Relais 1 → Vanne 2m OUVRIR
 *   Relais 2 → Vanne 2m FERMER
 *   Relais 3 → Vanne bout de rampe OUVRIR
 *   Relais 4 → Vanne bout de rampe FERMER
 *   Relais 5 → Phares arrière
 *   Relais 6 → Réserve
 *   Relais 7 → Réserve
 *   Relais 8 → Réserve
 * ==================================================================== */
#ifdef CARTE_ARRIERE

    #include "types_pulverisateur.h"

    #define ID_CARTE                CARTE_ID_ARRIERE

    /* GPIO des relais vannes 2m */
    #define GPIO_V2M_OUVRIR         GPIO_NUM_32
    #define GPIO_V2M_FERMER         GPIO_NUM_33

    /* GPIO des relais vanne bout de rampe */
    #define GPIO_VBR_OUVRIR         GPIO_NUM_25
    #define GPIO_VBR_FERMER         GPIO_NUM_26

    /* GPIO phares arrière */
    #define GPIO_RELAIS_PHARES_AR   GPIO_NUM_27

    /* GPIO réserves */
    #define GPIO_RELAIS_RESERVE_1   GPIO_NUM_14
    #define GPIO_RELAIS_RESERVE_2   GPIO_NUM_12
    #define GPIO_RELAIS_RESERVE_3   GPIO_NUM_13

    /* Logique relais */
    #define RELAIS_NIVEAU_ACTIF     1

    /* GPIO sonde niveau (ADC, entrée analogique 4-20mA) */
    #define GPIO_SONDE_NIVEAU       GPIO_NUM_34
    #define ADC_CANAL_SONDE         ADC_CHANNEL_6

    /* Capacités */
    #define A_DEBITMETRE            0
    #define A_VANNES_MOTORISEES     1
    #define A_SONDE_NIVEAU          1

    /* Pas de Web UI sur la carte arrière */
    #define A_WEB_UI                1

#endif /* CARTE_ARRIERE */

#endif /* BOARD_CONFIG_H */
