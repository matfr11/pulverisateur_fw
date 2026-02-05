/**
 * @file transfert.cpp
 * @brief Automatisme de transfert
 * 
 * Machine à états pour transfert automatique de liquide.
 * 
 * Modes:
 * - SANS SONDE: Transfert volume fixe paramétré
 * - AVEC SONDE: Transfert jusqu'à niveau cible (futur)
 * 
 * Séquence SANS SONDE:
 * 1. INACTIF → Attente commande
 * 2. EN_COURS → Transfert actif
 *    - Vanne 3V → TRANSFERT
 *    - Pompe → MARCHE
 *    - Lecture débitmètre
 *    - Vérification volume < cible
 * 3. TERMINE → Volume atteint
 *    - Pompe → ARRÊT
 *    - Vanne 3V → BRASSAGE (position sûre)
 * 4. ERREUR → Problème détecté
 * 
 * Sécurités:
 * - Arrêt si cuve vide
 * - Timeout si débit nul pendant 10 sec
 * - Commande manuelle prioritaire
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#include "automatismes.h"
#include "actionneurs.h"
#include "capteurs.h"
#include "mqtt_manager.h"
#include "mqtt_topics.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifdef BOARD_TYPE_AVANT

static const char* TAG = "TRANSFERT";

//extern "C" {
//    esp_err_t actionneur_pompe_set(etat_pompe_t etat);
//    etat_pompe_t actionneur_pompe_get(void);
//    esp_err_t actionneur_vanne_3voies_set(position_vanne_3voies_t position);
//    position_vanne_3voies_t actionneur_vanne_3voies_get(void);
//}

// ============================================================================
// VARIABLES PRIVÉES
// ============================================================================

// État machine transfert
static etat_transfert_t g_etat_transfert = ETAT_TRANSFERT_INACTIF;

// Mode transfert
static mode_transfert_t g_mode_transfert = MODE_TRANSFERT_SANS_SONDE;

// Volume cible (L)
static float g_volume_cible_litres = 0.0f;

// Volume au départ du cycle (L)
static float g_volume_debut_cycle_litres = 0.0f;

// Volume transféré dans ce cycle (L)
static float g_volume_cycle_litres = 0.0f;

// Timestamp dernière vérification débit
static int64_t g_timestamp_derniere_verification_us = 0;

// Timeout débit nul (10 secondes)
static const uint32_t TIMEOUT_DEBIT_NUL_MS = 10000;

// Mutex
static SemaphoreHandle_t g_mutex_transfert = NULL;

// ============================================================================
// FONCTIONS PRIVÉES
// ============================================================================

/**
 * @brief Démarre physiquement le transfert
 */
static esp_err_t transfert_demarrer_physique(void) {
    ESP_LOGI(TAG, "Démarrage physique transfert...");
    
    // 1. Positionner vanne 3 voies en TRANSFERT
    esp_err_t ret = actionneur_vanne_3voies_set(POSITION_VANNE_3V_TRANSFERT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur positionnement vanne 3V");
        return ret;
    }
    
    // Attendre stabilisation vanne
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 2. Démarrer pompe
    ret = actionneur_pompe_set(ETAT_POMPE_MARCHE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur démarrage pompe");
        return ret;
    }
    
    // 3. Reset compteur volume débitmètre
    debitmetre_reset_volume();
    g_volume_debut_cycle_litres = 0.0f;
    g_volume_cycle_litres = 0.0f;
    
    // 4. Init timestamp
    g_timestamp_derniere_verification_us = esp_timer_get_time();
    
    ESP_LOGI(TAG, "✓ Transfert démarré physiquement");
    return ESP_OK;
}

/**
 * @brief Arrête physiquement le transfert
 */
static void transfert_arreter_physique(void) {
    ESP_LOGI(TAG, "Arrêt physique transfert...");
    
    // 1. Arrêter pompe
    actionneur_pompe_set(ETAT_POMPE_ARRET);
    
    // 2. Remettre vanne en BRASSAGE (position sûre)
    vTaskDelay(pdMS_TO_TICKS(200));
    actionneur_vanne_3voies_set(POSITION_VANNE_3V_BRASSAGE);
    
    ESP_LOGI(TAG, "✓ Transfert arrêté physiquement");
}

/**
 * @brief Vérifie les conditions de sécurité
 * @return true si OK pour continuer
 */
static bool transfert_verifier_securites(void) {
    // Vérifier débit
    float debit = debitmetre_get_debit();
    
    // Si débit nul pendant trop longtemps → timeout
    if (debit < 0.5f) { // Seuil bas: 0.5 L/min
        int64_t maintenant_us = esp_timer_get_time();
        uint32_t temps_debit_nul_ms = 
            (maintenant_us - g_timestamp_derniere_verification_us) / 1000;
        
        if (temps_debit_nul_ms > TIMEOUT_DEBIT_NUL_MS) {
            ESP_LOGE(TAG, "TIMEOUT: Débit nul pendant %lu ms", temps_debit_nul_ms);
            return false;
        }
    } else {
        // Débit OK, reset timestamp
        g_timestamp_derniere_verification_us = esp_timer_get_time();
    }
    
    // TODO: Vérifier état cuve vide (via gestion_securites)
    
    return true;
}

/**
 * @brief Met à jour volume cycle
 */
static void transfert_maj_volume_cycle(void) {
    float volume_total = debitmetre_get_volume_total();
    g_volume_cycle_litres = volume_total - g_volume_debut_cycle_litres;
}

// ============================================================================
// FONCTIONS PUBLIQUES
// ============================================================================
extern "C" {
/**
 * @brief Initialise le module transfert
 */
esp_err_t transfert_init(void) {
    ESP_LOGI(TAG, "Initialisation transfert...");
    
    // Créer mutex
    g_mutex_transfert = xSemaphoreCreateMutex();
    if (g_mutex_transfert == NULL) {
        ESP_LOGE(TAG, "Erreur création mutex");
        return ESP_FAIL;
    }
    
    // État initial
    g_etat_transfert = ETAT_TRANSFERT_INACTIF;
    g_volume_cible_litres = 0.0f;
    g_volume_cycle_litres = 0.0f;
    
    ESP_LOGI(TAG, "✓ Transfert initialisé");
    return ESP_OK;
}
} // extern "C"

/**
 * @brief Active le transfert
 */
esp_err_t transfert_activer(mode_transfert_t mode, float volume_cible_litres) {
    if (xSemaphoreTake(g_mutex_transfert, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Vérifier si déjà actif
    if (g_etat_transfert == ETAT_TRANSFERT_EN_COURS) {
        ESP_LOGW(TAG, "Transfert déjà actif");
        xSemaphoreGive(g_mutex_transfert);
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ACTIVATION TRANSFERT");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Mode: %s", mode == MODE_TRANSFERT_SANS_SONDE ? 
             "SANS SONDE" : "AVEC SONDE");
    ESP_LOGI(TAG, "Volume cible: %.1f L", volume_cible_litres);
    
    // Valider paramètres
    if (volume_cible_litres <= 0.0f) {
        ESP_LOGE(TAG, "Volume cible invalide");
        xSemaphoreGive(g_mutex_transfert);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Sauvegarder paramètres
    g_mode_transfert = mode;
    g_volume_cible_litres = volume_cible_litres;
    
    // Démarrer physiquement
    esp_err_t ret = transfert_demarrer_physique();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur démarrage physique");
        g_etat_transfert = ETAT_TRANSFERT_TERMINE;
        xSemaphoreGive(g_mutex_transfert);
        return ret;
    }
    
    // Changer état
    g_etat_transfert = ETAT_TRANSFERT_EN_COURS;
    
    // Publier état MQTT
    extern void automatismes_publier_etat_transfert(void);
    automatismes_publier_etat_transfert();
    
    // Suspendre brassage si actif
    extern void brassage_suspendre(void);
    brassage_suspendre();
    
    xSemaphoreGive(g_mutex_transfert);
    
    ESP_LOGI(TAG, "Transfert activé");
    return ESP_OK;
}

/**
 * @brief Désactive le transfert
 */
esp_err_t transfert_desactiver(void) {
    if (xSemaphoreTake(g_mutex_transfert, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (g_etat_transfert == ETAT_TRANSFERT_INACTIF) {
        ESP_LOGD(TAG, "Transfert déjà inactif");
        xSemaphoreGive(g_mutex_transfert);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Désactivation transfert");
    
    // Arrêter physiquement
    transfert_arreter_physique();
    
    // Changer état
    g_etat_transfert = ETAT_TRANSFERT_INACTIF;
    
    // Publier état
    extern void automatismes_publier_etat_transfert(void);
    automatismes_publier_etat_transfert();
    
    // Reprendre brassage si configuré
    extern void brassage_reprendre(void);
    brassage_reprendre();
    
    xSemaphoreGive(g_mutex_transfert);
    
    return ESP_OK;
}

/**
 * @brief Machine à états transfert (appelée périodiquement)
 */
void transfert_machine_etats(void) {
    if (xSemaphoreTake(g_mutex_transfert, 0) != pdTRUE) {
        return; // Occupé, on skip ce cycle
    }
    
    if (g_etat_transfert != ETAT_TRANSFERT_EN_COURS) {
        xSemaphoreGive(g_mutex_transfert);
        return;
    }
    
    // ========================================================================
    // ÉTAT: EN_COURS
    // ========================================================================
    
    // Mettre à jour volume cycle
    transfert_maj_volume_cycle();
    
    // Vérifier sécurités
    if (!transfert_verifier_securites()) {
        ESP_LOGE(TAG, "Erreur sécurité - arrêt transfert");
        g_etat_transfert = ETAT_TRANSFERT_TERMINE;
        transfert_arreter_physique();
        xSemaphoreGive(g_mutex_transfert);
        return;
    }
    
    // Vérifier volume atteint
    if (g_volume_cycle_litres >= g_volume_cible_litres) {
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "  TRANSFERT TERMINÉ");
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "Volume transféré: %.2f L", g_volume_cycle_litres);
        ESP_LOGI(TAG, "Volume cible: %.1f L", g_volume_cible_litres);
        
        // Arrêter
        transfert_arreter_physique();
        g_etat_transfert = ETAT_TRANSFERT_TERMINE;
        
        // Publier état
        extern void automatismes_publier_etat_transfert(void);
        automatismes_publier_etat_transfert();
        
        // Reprendre brassage
        extern void brassage_reprendre(void);
        brassage_reprendre();
    }
    
    xSemaphoreGive(g_mutex_transfert);
}

/**
 * @brief Obtient état transfert
 */
etat_transfert_t transfert_get_etat(void) {
    return g_etat_transfert;
}

/**
 * @brief Obtient volume cycle
 */
float transfert_get_volume_cycle(void) {
    return g_volume_cycle_litres;
}

/**
 * @brief Obtient pourcentage
 */
float transfert_get_pourcentage(void) {
    if (g_volume_cible_litres <= 0.0f) {
        return 0.0f;
    }
    
    float pct = (g_volume_cycle_litres / g_volume_cible_litres) * 100.0f;
    return (pct > 100.0f) ? 100.0f : pct;
}

/**
 * @brief Vérifie si actif
 */
bool transfert_est_actif(void) {
    return g_etat_transfert == ETAT_TRANSFERT_EN_COURS;
}

#endif // BOARD_TYPE_AVANT
