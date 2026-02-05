/**
 * @file capteurs_manager.cpp
 * @brief Gestionnaire principal capteurs
 * 
 * Responsabilités:
 * - Initialisation capteurs
 * - Tâche périodique de calcul et publication
 * - Coordination débitmètre + sonde niveau
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#include "capteurs.h"
#include "board_config.h"
#include "mqtt_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
//#include "app_main.h"
static const char* TAG = "CAPTEURS";

// Déclarations des fonctions globales situées dans app_main.cpp
extern "C" {
    EventGroupHandle_t app_get_event_group_systeme(void);
}
extern "C" {
    void debitmetre_calculer_debit(void);
}
// On définit le bit ici s'il n'est pas accessible via un header commun
#ifndef BIT_CAPTEURS_PRETS
#define BIT_CAPTEURS_PRETS (1 << 2) 
#endif
// ============================================================================
// DÉCLARATIONS FORWARD
// ============================================================================

#ifdef BOARD_TYPE_AVANT
extern esp_err_t debitmetre_init(void);
extern void debitmetre_calculer_debit(void);
#endif

static void tache_capteurs(void* pvParameters);

// ============================================================================
// INITIALISATION
// ============================================================================

/**
 * @brief Initialise le système de capteurs
 */
esp_err_t capteurs_init(void) {
    ESP_LOGI(TAG, "Initialisation capteurs...");
    
#ifdef BOARD_TYPE_AVANT
    // Initialiser débitmètre
    esp_err_t ret = debitmetre_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur initialisation débitmètre");
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Débitmètre initialisé");
    
    // Sonde niveau (future)
    // TODO: Initialiser si disponible
    
#else
    ESP_LOGI(TAG, "Pas de capteurs sur cette carte");
#endif
    
    return ESP_OK;
}

// ============================================================================
// TÂCHE PÉRIODIQUE
// ============================================================================

/**
 * @brief Démarre la tâche de gestion des capteurs
 */
void capteurs_demarrer_tache(void) {
#ifdef BOARD_TYPE_AVANT
    BaseType_t ret = xTaskCreate(
        tache_capteurs,
        "capteurs",
        TASK_STACK_CAPTEURS,
        NULL,
        TASK_PRIORITY_CAPTEURS,
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Erreur création tâche capteurs");
    } else {
        ESP_LOGI(TAG, "Tâche capteurs démarrée");
    }
#else
    ESP_LOGI(TAG, "Pas de tâche capteurs (carte sans capteurs)");
#endif
}

/**
 * @brief Tâche périodique de gestion des capteurs
 * 
 * Fréquence: 100ms (10 Hz) pour lecture capteurs
 * Publication MQTT: 1 Hz
 * 
 * Responsabilités:
 * - Calcul débit instantané
 * - Calcul volume total
 * - Publication MQTT
 * - Lecture sonde niveau (si présente)
 */
static void tache_capteurs(void* pvParameters) {
    (void)pvParameters;
    
#ifdef BOARD_TYPE_AVANT
    
// --- NOUVEAU : Barrière de synchronisation ---
    // On attend que app_main mette le bit BIT_CAPTEURS_PRETS à 1
    // Cela garantit que debitmetre_init() est terminé à 100%
    EventGroupHandle_t ev_sys = app_get_event_group_systeme();
    if (ev_sys != NULL) {
        ESP_LOGI(TAG, "Tâche capteurs en attente de l'initialisation matérielle...");
        xEventGroupWaitBits(ev_sys, BIT_CAPTEURS_PRETS, pdFALSE, pdTRUE, portMAX_DELAY);
    }
    // ----------------------------------------------

    TickType_t derniere_execution = xTaskGetTickCount();
    const TickType_t periode = pdMS_TO_TICKS(PERIODE_LECTURE_CAPTEURS);
    
    uint32_t compteur_cycles = 0;
    
    ESP_LOGI(TAG, "Tâche capteurs opérationnelle (période: %d ms)", 
             PERIODE_LECTURE_CAPTEURS);
    
    while (1) {
        vTaskDelayUntil(&derniere_execution, periode);
        compteur_cycles++;
        
        // ====================================================================
        // DÉBITMÈTRE - Calcul débit
        // ====================================================================
        
        // Calculer débit à chaque cycle (100ms)
        // Cela donne une bonne réactivité
        debitmetre_calculer_debit();
        
        // ====================================================================
        // SONDE NIVEAU - Lecture (future)
        // ====================================================================
        
        // TODO: Lire sonde niveau si disponible
        // if (sonde_niveau_est_disponible()) {
        //     sonde_niveau_lire();
        // }
        
        // ====================================================================
        // PUBLICATION MQTT
        // ====================================================================
        
        // Publier toutes les secondes (10 cycles x 100ms)
        if (compteur_cycles % 10 == 0) {
            float debit = debitmetre_get_debit();
            float volume = debitmetre_get_volume_total();
            
            // Publier via MQTT
            extern void capteurs_publier_debitmetre(float debit, float volume);
            capteurs_publier_debitmetre(debit, volume);
            
            ESP_LOGD(TAG, "Publication: %.2f L/min, %.2f L total", debit, volume);
        }
        
        // ====================================================================
        // LOGS PÉRIODIQUES (debug)
        // ====================================================================
        
        if (compteur_cycles % 50 == 0) { // Toutes les 5 secondes
            uint32_t impulsions = debitmetre_get_impulsions();
            float debit = debitmetre_get_debit();
            float volume = debitmetre_get_volume_total();
            
            ESP_LOGI(TAG, "Débitmètre: %.2f L/min, %.2f L total (%lu imp)",
                     debit, volume, impulsions);
        }
    }
    
#endif // BOARD_TYPE_AVANT
}

// ============================================================================
// SONDE NIVEAU (FUTURE - OPTIONNELLE)
// ============================================================================

/**
 * @brief Obtient données sonde niveau
 */
esp_err_t sonde_niveau_get_donnees(donnees_niveau_t* donnees) {
    if (donnees == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
#ifdef SONDE_NIVEAU_PRESENTE
    // TODO: Implémenter lecture sonde niveau
    // Pour l'instant, non disponible
    donnees->disponible = false;
    donnees->niveau_litres = 0.0f;
    donnees->timestamp_ms = 0;
    return ESP_ERR_NOT_FOUND;
#else
    donnees->disponible = false;
    donnees->niveau_litres = 0.0f;
    donnees->timestamp_ms = 0;
    return ESP_ERR_NOT_FOUND;
#endif
}

/**
 * @brief Obtient niveau en litres
 */
float sonde_niveau_get_litres(void) {
    return -1.0f; // Non disponible
}

/**
 * @brief Vérifie disponibilité sonde
 */
bool sonde_niveau_est_disponible(void) {
#ifdef SONDE_NIVEAU_PRESENTE
    return true;
#else
    return false;
#endif
}

// ============================================================================
// STATISTIQUES
// ============================================================================

/**
 * @brief Affiche statistiques capteurs
 */
void capteurs_get_stats(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  STATISTIQUES CAPTEURS");
    ESP_LOGI(TAG, "========================================");
    
#ifdef BOARD_TYPE_AVANT
    donnees_debitmetre_t donnees;
    if (debitmetre_get_donnees(&donnees) == ESP_OK) {
        ESP_LOGI(TAG, "Débitmètre:");
        ESP_LOGI(TAG, "  - Débit instantané: %.2f L/min", donnees.debit_instantane_lpm);
        ESP_LOGI(TAG, "  - Volume total: %.2f L", donnees.volume_total_litres);
        ESP_LOGI(TAG, "  - Impulsions: %lu", donnees.impulsions_totales);
        ESP_LOGI(TAG, "  - Facteur K: %.2f imp/L", debitmetre_get_facteur_k());
        
        uint32_t temps_depuis_impulsion = 
            (uint32_t)(esp_timer_get_time() / 1000) - 
            donnees.timestamp_derniere_impulsion_ms;
        ESP_LOGI(TAG, "  - Dernière impulsion: il y a %lu ms", temps_depuis_impulsion);
    }
    
    if (sonde_niveau_est_disponible()) {
        ESP_LOGI(TAG, "Sonde niveau:");
        ESP_LOGI(TAG, "  - Niveau: %.1f L", sonde_niveau_get_litres());
    } else {
        ESP_LOGI(TAG, "Sonde niveau: Non disponible");
    }
#else
    ESP_LOGI(TAG, "Carte sans capteurs");
#endif
    
    ESP_LOGI(TAG, "========================================");
}
