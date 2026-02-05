/**
 * @file actionneurs_avant.cpp
 * @brief Gestion actionneurs carte AVANT
 * 
 * Actionneurs:
 * - Pompe (relais simple ON/OFF)
 * - Vanne 3 voies (relais simple: OFF=brassage, ON=transfert)
 * - Phares avant (relais simple ON/OFF)
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#include "actionneurs.h"
#include "board_config.h"
#include "mqtt_manager.h"
#include "mqtt_topics.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>

#ifdef HAVE_HTTP_SERVER
#include "http_server.h"
#endif

#ifdef BOARD_TYPE_AVANT

static const char* TAG = LOG_TAG_ACTIONNEURS;

extern "C" {

// ============================================================================
// VARIABLES PRIVÉES
// ============================================================================

// États actuels
static etat_pompe_t g_etat_pompe = ETAT_POMPE_ARRET;
static position_vanne_3voies_t g_position_vanne_3v = POSITION_VANNE_3V_BRASSAGE;
static bool g_phares_avant = false;

// Mode simulation
#if MODE_SIMULATION
static bool g_mode_simulation = true;
#else
static bool g_mode_simulation = false;
#endif

// ============================================================================
// FONCTIONS PRIVÉES
// ============================================================================

/**
 * @brief Applique physiquement l'état de la pompe
 */
static void appliquer_etat_pompe(etat_pompe_t etat) {
    if (g_mode_simulation) {
        ESP_LOGI(TAG, "[SIMULATION] Pompe → %s", 
                 etat == ETAT_POMPE_MARCHE ? "MARCHE" : "ARRÊT");
        return;
    }
    
    gpio_set_level(GPIO_RELAIS_POMPE, etat == ETAT_POMPE_MARCHE ? 1 : 0);
    ESP_LOGI(TAG, "Pompe → %s", etat == ETAT_POMPE_MARCHE ? "MARCHE" : "ARRÊT");
}

/**
 * @brief Applique physiquement la position vanne 3 voies
 */
static void appliquer_position_vanne_3v(position_vanne_3voies_t position) {
    if (g_mode_simulation) {
        ESP_LOGI(TAG, "[SIMULATION] Vanne 3V → %s",
                 position == POSITION_VANNE_3V_TRANSFERT ? "TRANSFERT" : "BRASSAGE");
        return;
    }
    
    gpio_set_level(GPIO_RELAIS_VANNE_3VOIES, 
                   position == POSITION_VANNE_3V_TRANSFERT ? 1 : 0);
    ESP_LOGI(TAG, "Vanne 3V → %s", 
             position == POSITION_VANNE_3V_TRANSFERT ? "TRANSFERT" : "BRASSAGE");
}

/**
 * @brief Applique physiquement l'état des phares
 */
static void appliquer_etat_phares(bool etat) {
    if (g_mode_simulation) {
        ESP_LOGI(TAG, "[SIMULATION] Phares avant → %s", etat ? "ON" : "OFF");
        return;
    }
    
    gpio_set_level(GPIO_RELAIS_PHARES_AVANT, etat ? 1 : 0);
    ESP_LOGI(TAG, "Phares avant → %s", etat ? "ON" : "OFF");
}

// ============================================================================
// FONCTIONS PUBLIQUES - POMPE
// ============================================================================

/**
 * @brief Commande la pompe
 */
esp_err_t actionneur_pompe_set(etat_pompe_t etat) {
    if (g_etat_pompe == etat) {
        ESP_LOGD(TAG, "Pompe déjà à l'état demandé");
        return ESP_OK;
    }
    
    g_etat_pompe = etat;
    appliquer_etat_pompe(etat);
    
    // Publier état via MQTT
    mqtt_publier_etat_actionneur(TOPIC_ETAT_POMPE, (int)etat);
    
    // Mise à jour HTTP
#ifdef HAVE_HTTP_SERVER
    http_status_update_pompe(etat == ETAT_POMPE_MARCHE);
#endif
    
    return ESP_OK;
}

/**
 * @brief Toggle pompe
 */
esp_err_t actionneur_pompe_toggle(void) {
    etat_pompe_t nouvel_etat = (g_etat_pompe == ETAT_POMPE_ARRET) ? 
                                ETAT_POMPE_MARCHE : ETAT_POMPE_ARRET;
    return actionneur_pompe_set(nouvel_etat);
}

/**
 * @brief Obtient l'état pompe
 */
etat_pompe_t actionneur_pompe_get(void) {
    return g_etat_pompe;
}

// ============================================================================
// FONCTIONS PUBLIQUES - VANNE 3 VOIES
// ============================================================================

/**
 * @brief Commande vanne 3 voies
 */
esp_err_t actionneur_vanne_3voies_set(position_vanne_3voies_t position) {
    if (g_position_vanne_3v == position) {
        ESP_LOGD(TAG, "Vanne 3V déjà à la position demandée");
        return ESP_OK;
    }
    
    g_position_vanne_3v = position;
    appliquer_position_vanne_3v(position);
    
    // Publier état via MQTT
    mqtt_publier_etat_actionneur(TOPIC_ETAT_VANNE_3VOIES, (int)position);
    
    // Mise à jour HTTP
#ifdef HAVE_HTTP_SERVER
http_status_update_vanne_3v(position == POSITION_VANNE_3V_TRANSFERT);
#endif
    
    return ESP_OK;
}

/**
 * @brief Toggle vanne 3 voies
 */
esp_err_t actionneur_vanne_3voies_toggle(void) {
    position_vanne_3voies_t nouvelle_pos = 
        (g_position_vanne_3v == POSITION_VANNE_3V_BRASSAGE) ?
        POSITION_VANNE_3V_TRANSFERT : POSITION_VANNE_3V_BRASSAGE;
    return actionneur_vanne_3voies_set(nouvelle_pos);
}

/**
 * @brief Obtient position vanne 3 voies
 */
position_vanne_3voies_t actionneur_vanne_3voies_get(void) {
    return g_position_vanne_3v;
}

// ============================================================================
// FONCTIONS PUBLIQUES - PHARES AVANT
// ============================================================================

/**
 * @brief Commande phares avant
 */
esp_err_t actionneur_phares_avant_set(bool etat) {
    if (g_phares_avant == etat) {
        ESP_LOGD(TAG, "Phares avant déjà à l'état demandé");
        return ESP_OK;
    }
    
    g_phares_avant = etat;
    appliquer_etat_phares(etat);
    
    // Publier état via MQTT
    mqtt_publier_etat_actionneur(TOPIC_ETAT_PHARES_AVANT, etat ? 1 : 0);
    
    // Mise à jour HTTP
#ifdef HAVE_HTTP_SERVER
    http_status_update_phares_avant(etat);
#endif
    
    return ESP_OK;
}

/**
 * @brief Toggle phares avant
 */
esp_err_t actionneur_phares_avant_toggle(void) {
    return actionneur_phares_avant_set(!g_phares_avant);
}

// ============================================================================
// MODE SIMULATION
// ============================================================================

#if MODE_SIMULATION
void actionneurs_set_simulation(bool enable) {
    g_mode_simulation = enable;
    ESP_LOGI(TAG, "Mode simulation: %s", enable ? "ACTIVÉ" : "DÉSACTIVÉ");
}

bool actionneurs_est_simulation(void) {
    return g_mode_simulation;
}
#endif
} // extern "C"
#endif // BOARD_TYPE_AVANT