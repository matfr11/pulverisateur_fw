#pragma once
/* Mock board_config.h pour CARTE_AVANT (tests des automatismes et sécurités) */

#define CARTE_AVANT         1

#include "types_pulverisateur.h"

#define ID_CARTE            CARTE_ID_AVANT
#define GPIO_RELAIS_POMPE   33
#define GPIO_RELAIS_V3V     32
#define GPIO_DEBITMETRE     13
#define RELAIS_NIVEAU_ACTIF 1

#define A_DEBITMETRE        1
#define A_VANNES_MOTORISEES 0
#define A_SONDE_NIVEAU      0
#define A_WEB_UI            0
#define A_EST_SERVEUR       0

#undef  VERSION_FIRMWARE
#define VERSION_FIRMWARE    "3.0.0-test"
