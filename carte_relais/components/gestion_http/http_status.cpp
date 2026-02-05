/**
 * @file http_status.cpp
 * @brief Génération du JSON d'état système pour /status
 */

#include "esp_log.h"
#include "cJSON.h"
#include "types_communs.h"
#include <stdlib.h>
#include <string.h>

static const char* TAG = "HTTP_STATUS";

// Variables globales d'état (à synchroniser avec MQTT)
// Ces variables seront mises à jour par des callbacks MQTT
static bool g_pompe_marche = false;
static bool g_vanne_3v_transfert = false;
static bool g_phares_avant = false;
static bool g_phares_arriere = false;
static float g_debit_lmin = 0.0f;
static float g_volume_session = 0.0f;
static bool g_carte_avant_ok = true;
static bool g_cuve_vide = false;
static etat_vanne_3fils_t g_vanne_2m_etat = ETAT_VANNE_3FILS_INACTIF;
static etat_vanne_3fils_t g_vanne_bout_etat = ETAT_VANNE_3FILS_INACTIF;
static bool g_transfert_actif = false;
static bool g_brassage_actif = false;
static uint32_t g_transfert_target = 1000;
static uint32_t g_brassage_temps_restant = 0;
static uint8_t g_brassage_pct = 0;
static const char* g_brassage_label = "REPOS";

/**
 * @brief Callbacks pour mise à jour des états depuis MQTT
 */
extern "C" {

void http_status_update_pompe(bool marche) {
    g_pompe_marche = marche;
}

void http_status_update_vanne_3v(bool transfert) {
    g_vanne_3v_transfert = transfert;
}

void http_status_update_phares_avant(bool on) {
    g_phares_avant = on;
}

void http_status_update_phares_arriere(bool on) {
    g_phares_arriere = on;
}

void http_status_update_debit(float debit_lmin, float volume_total) {
    g_debit_lmin = debit_lmin;
    g_volume_session = volume_total;
}

void http_status_update_vanne_2m(etat_vanne_3fils_t etat) {
    g_vanne_2m_etat = etat;
}

void http_status_update_vanne_bout(etat_vanne_3fils_t etat) {
    g_vanne_bout_etat = etat;
}

void http_status_update_cuve_vide(bool vide) {
    g_cuve_vide = vide;
}

void http_status_update_transfert(bool actif, float volume_session, uint32_t target) {
    g_transfert_actif = actif;
    g_volume_session = volume_session;
    g_transfert_target = target;
}

void http_status_update_brassage(bool actif, uint32_t temps_restant, uint8_t pct, const char* label) {
    g_brassage_actif = actif;
    g_brassage_temps_restant = temps_restant;
    g_brassage_pct = pct;
    g_brassage_label = label;
}

} // extern "C"

/**
 * @brief Convertit état vanne en caractère
 */
static char vanne_etat_to_char(etat_vanne_3fils_t etat) {
    switch (etat) {
        case ETAT_VANNE_3FILS_OUVERTURE:
        case ETAT_VANNE_3FILS_OUVERTE:
            return 'O';
        case ETAT_VANNE_3FILS_FERMETURE:
        case ETAT_VANNE_3FILS_FERMEE:
            return 'F';
        default:
            return 'S'; // STOP/INACTIF
    }
}

/**
 * @brief Génère le JSON d'état système
 * 
 * Format attendu par l'UI:
 * {
 *   "p": bool,           // Pompe
 *   "v": bool,           // Vanne 3V (false=brassage, true=transfert)
 *   "l": bool,           // Phares avant
 *   "li": bool,          // Phares arrière
 *   "av_flow": float,    // Débit L/min
 *   "av_ok": bool,       // Carte avant OK
 *   "av_vide": bool,     // Cuve vide
 *   "v2m": "O/F/S",      // Vanne 2m
 *   "vbt": "O/F/S",      // Vanne bout
 *   "m_tr": bool,        // Mode transfert actif
 *   "tr_target": int,    // Volume cible transfert
 *   "session_vol": float,// Volume session
 *   "m_br": bool,        // Mode brassage actif
 *   "br_rem": int,       // Temps restant brassage (min)
 *   "br_pct": int,       // Pourcentage brassage
 *   "br_label": string   // Label brassage
 * }
 */
char* http_get_status_json(void) {
    cJSON* root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Erreur création JSON");
        return NULL;
    }
    
    // États actionneurs
    cJSON_AddBoolToObject(root, "p", g_pompe_marche);
    cJSON_AddBoolToObject(root, "v", g_vanne_3v_transfert);
    cJSON_AddBoolToObject(root, "l", g_phares_avant);
    cJSON_AddBoolToObject(root, "li", g_phares_arriere);
    
    // Débitmètre
    cJSON_AddNumberToObject(root, "av_flow", g_debit_lmin);
    cJSON_AddBoolToObject(root, "av_ok", g_carte_avant_ok);
    cJSON_AddBoolToObject(root, "av_vide", g_cuve_vide);
    
    // Vannes arrière
    char v2m_str[2] = {vanne_etat_to_char(g_vanne_2m_etat), '\0'};
    char vbt_str[2] = {vanne_etat_to_char(g_vanne_bout_etat), '\0'};
    cJSON_AddStringToObject(root, "v2m", v2m_str);
    cJSON_AddStringToObject(root, "vbt", vbt_str);
    
    // Automatismes - Transfert
    cJSON_AddBoolToObject(root, "m_tr", g_transfert_actif);
    cJSON_AddNumberToObject(root, "tr_target", g_transfert_target);
    cJSON_AddNumberToObject(root, "session_vol", g_volume_session);
    
    // Automatismes - Brassage
    cJSON_AddBoolToObject(root, "m_br", g_brassage_actif);
    cJSON_AddNumberToObject(root, "br_rem", g_brassage_temps_restant);
    cJSON_AddNumberToObject(root, "br_pct", g_brassage_pct);
    cJSON_AddStringToObject(root, "br_label", g_brassage_label);
    
    // Convertir en string
    char* json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}
