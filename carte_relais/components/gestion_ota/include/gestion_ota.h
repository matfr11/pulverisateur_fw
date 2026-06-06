#pragma once
#include "esp_err.h"
#include "types_pulverisateur.h"

/* Hostnames mDNS des cartes — source de vérité unique */
#define OTA_HOSTNAME_AVANT   "pulve-av"
#define OTA_HOSTNAME_ARRIERE "pulve-ar"
#define OTA_HOSTNAME_SERVEUR "pulve-srv"

/**
 * Retourne le hostname mDNS associé à une carte relais cible.
 * Retourne NULL si l'ID ne correspond pas à une carte connue.
 */
const char *ota_hostname_pour_carte(carte_id_t id);

esp_err_t ota_serveur_demarrer(void);
void      ota_serveur_arreter(void);
