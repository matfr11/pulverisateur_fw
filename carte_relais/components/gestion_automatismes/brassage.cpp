/**
 * @file brassage.cpp
 * @brief Automatisme de brassage cyclique
 * 
 * Machine à états pour brassage automatique.
 * 
 * Principe:
 * - Cycles ON/OFF paramétrables
 * - Phase ON: Pompe MARCHE, Vanne BRASSAGE
 * - Phase PAUSE: Pompe ARRÊT
 * - Répétition automatique
 * 
 * Séquence:
 * 1. INACTIF → Attente activation
 * 2. MARCHE → Pompe en marche
 *    - Pompe → MARCHE
 *    - Vanne 3V → BRASSAGE
 *    - Durée: temps_marche_sec
 * 3. PAUSE → Pompe arrêtée
 *    - Pompe → ARRÊT
 *    - Durée: temps_pause_sec
 * 4. Retour à MARCHE (cycle infini)
 * 
 * États spéciaux:
 * - SUSPENDU: Pendant transfert (reprendra après)
 * 
 * Sécurités:
 * - Suspension automatique pendant transfert
 * - Reprise automatique après transfert
 * - Commande manuelle prioritaire
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#include "automatismes.h"
#include "actionneurs.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifdef BOARD_TYPE_AVANT

static const char* TAG = "BRASSAGE";

//extern "C" {
//    esp_err_t actionneur_pompe_set(etat_pompe_t etat);
//    esp_err_t actionneur_vanne_3voies_set(position_vanne_3voies_t position);
//    position_vanne_3voies_t actionneur_vanne_3voies_get(void);
//}

// ============================================================================
// VARIABLES PRIVÉES
// ============================================================================

// État machine brassage
static etat_brassage_t g_etat_brassage = ETAT_BRASSAGE_INACTIF;

// État avant suspension (pour reprendre)
static etat_brassage_t g_etat_avant_suspension = ETAT_BRASSAGE_INACTIF;

// Temps configurés (secondes)
static uint32_t g_temps_marche_sec = 0;
static uint32_t g_temps_pause_sec = 0;

// Temps restant dans phase actuelle (secondes)
static uint32_t g_temps_restant_sec = 0;

// Timestamp dernière mise à jour
static int64_t g_timestamp_derniere_maj_us = 0;

// Compteurs statistiques
static uint32_t g_nombre_cycles_completes = 0;

// Mutex
static SemaphoreHandle_t g_mutex_brassage = NULL;

// ============================================================================
// FONCTIONS PRIVÉES
// ============================================================================

/**
 * @brief Démarre phase marche
 */
static void brassage_demarrer_marche(void) {
    ESP_LOGI(TAG, "Phase MARCHE (durée: %lu sec)", g_temps_marche_sec);
    
    // Vérifier vanne en position brassage
    if (actionneur_vanne_3voies_get() != POSITION_VANNE_3V_BRASSAGE) {
        actionneur_vanne_3voies_set(POSITION_VANNE_3V_BRASSAGE);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    
    // Démarrer pompe
    actionneur_pompe_set(ETAT_POMPE_MARCHE);
    
    // Changer état
    g_etat_brassage = ETAT_BRASSAGE_MARCHE;
    g_temps_restant_sec = g_temps_marche_sec;
    g_timestamp_derniere_maj_us = esp_timer_get_time();
}

/**
 * @brief Démarre phase pause
 */
static void brassage_demarrer_pause(void) {
    ESP_LOGI(TAG, "Phase PAUSE (durée: %lu sec)", g_temps_pause_sec);
    
    // Arrêter pompe
    actionneur_pompe_set(ETAT_POMPE_ARRET);
    
    // Changer état
    g_etat_brassage = ETAT_BRASSAGE_PAUSE;
    g_temps_restant_sec = g_temps_pause_sec;
    g_timestamp_derniere_maj_us = esp_timer_get_time();
}

/**
 * @brief Met à jour temps restant
 */
static void brassage_maj_temps_restant(void) {
    int64_t maintenant_us = esp_timer_get_time();
    int64_t delta_us = maintenant_us - g_timestamp_derniere_maj_us;
    uint32_t delta_sec = delta_us / 1000000;
    
    if (delta_sec > 0) {
        if (g_temps_restant_sec >= delta_sec) {
            g_temps_restant_sec -= delta_sec;
        } else {
            g_temps_restant_sec = 0;
        }
        g_timestamp_derniere_maj_us = maintenant_us;
    }
}

// ============================================================================
// FONCTIONS PUBLIQUES
// ============================================================================

/**
 * @brief Initialise le module brassage
 */
extern "C" {
esp_err_t brassage_init(void) {
    ESP_LOGI(TAG, "Initialisation brassage...");
    
    // Créer mutex
    g_mutex_brassage = xSemaphoreCreateMutex();
    if (g_mutex_brassage == NULL) {
        ESP_LOGE(TAG, "Erreur création mutex");
        return ESP_FAIL;
    }
    
    // État initial
    g_etat_brassage = ETAT_BRASSAGE_INACTIF;
    g_temps_marche_sec = 0;
    g_temps_pause_sec = 0;
    g_temps_restant_sec = 0;
    g_nombre_cycles_completes = 0;
    
    ESP_LOGI(TAG, "✓ Brassage initialisé");
    return ESP_OK;
}
}   // extern "C"

/**
 * @brief Active le brassage
 */
esp_err_t brassage_activer(uint32_t temps_marche_sec, uint32_t temps_pause_sec) {
    if (xSemaphoreTake(g_mutex_brassage, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ACTIVATION BRASSAGE");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Temps marche: %lu sec (%lu min)", 
             temps_marche_sec, temps_marche_sec / 60);
    ESP_LOGI(TAG, "Temps pause: %lu sec (%lu min)", 
             temps_pause_sec, temps_pause_sec / 60);
    
    // Valider paramètres
    if (temps_marche_sec == 0 || temps_pause_sec == 0) {
        ESP_LOGE(TAG, "Paramètres invalides");
        xSemaphoreGive(g_mutex_brassage);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Sauvegarder paramètres
    g_temps_marche_sec = temps_marche_sec;
    g_temps_pause_sec = temps_pause_sec;
    g_nombre_cycles_completes = 0;
    
    // Démarrer phase marche
    brassage_demarrer_marche();
    
    // Publier état MQTT
    extern void automatismes_publier_etat_brassage(void);
    automatismes_publier_etat_brassage();
    
    xSemaphoreGive(g_mutex_brassage);
    
    ESP_LOGI(TAG, "Brassage activé");
    return ESP_OK;
}

/**
 * @brief Désactive le brassage
 */
esp_err_t brassage_desactiver(void) {
    if (xSemaphoreTake(g_mutex_brassage, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (g_etat_brassage == ETAT_BRASSAGE_INACTIF) {
        ESP_LOGD(TAG, "Brassage déjà inactif");
        xSemaphoreGive(g_mutex_brassage);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Désactivation brassage");
    ESP_LOGI(TAG, "Cycles complétés: %lu", g_nombre_cycles_completes);
    
    // Arrêter pompe si en marche
    if (g_etat_brassage == ETAT_BRASSAGE_MARCHE) {
        actionneur_pompe_set(ETAT_POMPE_ARRET);
    }
    
    // Changer état
    g_etat_brassage = ETAT_BRASSAGE_INACTIF;
    
    // Publier état
    extern void automatismes_publier_etat_brassage(void);
    automatismes_publier_etat_brassage();
    
    xSemaphoreGive(g_mutex_brassage);
    
    return ESP_OK;
}

/**
 * @brief Suspend le brassage (pendant transfert)
 */
void brassage_suspendre(void) {
    if (xSemaphoreTake(g_mutex_brassage, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    
    if (g_etat_brassage == ETAT_BRASSAGE_INACTIF || 
        g_etat_brassage == ETAT_BRASSAGE_PAUSE) {
        xSemaphoreGive(g_mutex_brassage);
        return;
    }
    
    ESP_LOGI(TAG, "Suspension brassage (transfert en cours)");
    
    // Sauvegarder état actuel
    g_etat_avant_suspension = g_etat_brassage;
    
    // Arrêter pompe
    actionneur_pompe_set(ETAT_POMPE_ARRET);
    
    // Changer état
    g_etat_brassage = ETAT_BRASSAGE_PAUSE;
    
    xSemaphoreGive(g_mutex_brassage);
}

/**
 * @brief Reprend le brassage (après transfert)
 */
void brassage_reprendre(void) {
    if (xSemaphoreTake(g_mutex_brassage, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    
    if (g_etat_brassage != ETAT_BRASSAGE_SUSPENDU) {
        xSemaphoreGive(g_mutex_brassage);
        return;
    }
    
    ESP_LOGI(TAG, "Reprise brassage (transfert terminé)");
    
    // Reprendre où on en était
    if (g_etat_avant_suspension == ETAT_BRASSAGE_MARCHE) {
        brassage_demarrer_marche();
    } else if (g_etat_avant_suspension == ETAT_BRASSAGE_PAUSE) {
        brassage_demarrer_pause();
    }
    
    xSemaphoreGive(g_mutex_brassage);
}

/**
 * @brief Machine à états brassage (appelée périodiquement)
 */
void brassage_machine_etats(void) {
    if (xSemaphoreTake(g_mutex_brassage, 0) != pdTRUE) {
        return; // Occupé
    }
    
    if (g_etat_brassage == ETAT_BRASSAGE_INACTIF || 
        g_etat_brassage == ETAT_BRASSAGE_PAUSE) {
        xSemaphoreGive(g_mutex_brassage);
        return;
    }
    
    // Mettre à jour temps restant
    brassage_maj_temps_restant();
    
    // ========================================================================
    // ÉTAT: MARCHE
    // ========================================================================
    
    if (g_etat_brassage == ETAT_BRASSAGE_MARCHE) {
        if (g_temps_restant_sec == 0) {
            ESP_LOGI(TAG, "Fin phase MARCHE");
            brassage_demarrer_pause();
        }
    }
    
    // ========================================================================
    // ÉTAT: PAUSE
    // ========================================================================
    
    else if (g_etat_brassage == ETAT_BRASSAGE_PAUSE) {
        if (g_temps_restant_sec == 0) {
            ESP_LOGI(TAG, "Fin phase PAUSE");
            g_nombre_cycles_completes++;
            ESP_LOGI(TAG, "Cycle %lu terminé", g_nombre_cycles_completes);
            brassage_demarrer_marche();
        }
    }
    
    xSemaphoreGive(g_mutex_brassage);
}

/**
 * @brief Obtient état brassage
 */
etat_brassage_t brassage_get_etat(void) {
    return g_etat_brassage;
}

/**
 * @brief Obtient temps restant
 */
uint32_t brassage_get_temps_restant(void) {
    return g_temps_restant_sec;
}

/**
 * @brief Obtient pourcentage
 */
float brassage_get_pourcentage(void) {
    if (g_etat_brassage == ETAT_BRASSAGE_MARCHE) {
        if (g_temps_marche_sec == 0) return 0.0f;
        float temps_ecoule = g_temps_marche_sec - g_temps_restant_sec;
        return (temps_ecoule / g_temps_marche_sec) * 100.0f;
    } else if (g_etat_brassage == ETAT_BRASSAGE_PAUSE) {
        if (g_temps_pause_sec == 0) return 0.0f;
        float temps_ecoule = g_temps_pause_sec - g_temps_restant_sec;
        return (temps_ecoule / g_temps_pause_sec) * 100.0f;
    }
    return 0.0f;
}

/**
 * @brief Vérifie si actif
 */
bool brassage_est_actif(void) {
    return g_etat_brassage == ETAT_BRASSAGE_MARCHE || 
           g_etat_brassage == ETAT_BRASSAGE_PAUSE;
}

/**
 * @brief Obtient nombre cycles
 */
uint32_t brassage_get_nombre_cycles(void) {
    return g_nombre_cycles_completes;
}

#endif // BOARD_TYPE_AVANT
