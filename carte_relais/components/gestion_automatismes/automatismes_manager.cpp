/**
 * @file automatismes_manager.cpp
 * @brief Gestionnaire principal automatismes
 * 
 * Coordination transfert + brassage
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#include "automatismes.h"
#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef BOARD_TYPE_AVANT

static const char* TAG = "AUTOMATISMES";

// ============================================================================
// DÉCLARATIONS FORWARD
// ============================================================================

extern esp_err_t transfert_init(void);
extern esp_err_t brassage_init(void);
extern void transfert_machine_etats(void);
extern void brassage_machine_etats(void);
extern void automatismes_publier_etat_transfert(void);
extern void automatismes_publier_etat_brassage(void);

static void tache_automatismes(void* pvParameters);

// ============================================================================
// INITIALISATION
// ============================================================================

/**
 * @brief Initialise le système d'automatismes
 */
esp_err_t automatismes_init(void) {
    ESP_LOGI(TAG, "Initialisation automatismes...");
    
    // Initialiser transfert
    esp_err_t ret = transfert_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init transfert");
        return ret;
    }
    
    // Initialiser brassage
    ret = brassage_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init brassage");
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Automatismes initialisés");
    return ESP_OK;
}

// ============================================================================
// TÂCHES
// ============================================================================

/**
 * @brief Démarre les tâches d'automatismes
 */
void automatismes_demarrer_taches(void) {
    BaseType_t ret = xTaskCreate(
        tache_automatismes,
        "automatismes",
        TASK_STACK_AUTOMATISMES,
        NULL,
        TASK_PRIORITY_AUTOMATISMES,
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Erreur création tâche automatismes");
    } else {
        ESP_LOGI(TAG, "Tâche automatismes démarrée");
    }
}

/**
 * @brief Tâche périodique automatismes
 * 
 * Fréquence: 1 Hz (1000ms)
 * 
 * Responsabilités:
 * - Exécuter machines à états
 * - Publier états MQTT
 */
static void tache_automatismes(void* pvParameters) {
    (void)pvParameters;
    
    TickType_t derniere_execution = xTaskGetTickCount();
    const TickType_t periode = pdMS_TO_TICKS(PERIODE_MAJ_AUTOMATISMES);
    
    uint32_t compteur_cycles = 0;
    
    ESP_LOGI(TAG, "Tâche automatismes opérationnelle (période: %d ms)", 
             PERIODE_MAJ_AUTOMATISMES);
    
    while (1) {
        vTaskDelayUntil(&derniere_execution, periode);
        compteur_cycles++;
        
        // ====================================================================
        // MACHINES À ÉTATS
        // ====================================================================
        
        // Transfert
        transfert_machine_etats();
        
        // Brassage
        brassage_machine_etats();
        
        // ====================================================================
        // PUBLICATION MQTT (toutes les 2 secondes)
        // ====================================================================
        
        if (compteur_cycles % 2 == 0) {
            if (transfert_est_actif()) {
                automatismes_publier_etat_transfert();
            }
            
            if (brassage_est_actif()) {
                automatismes_publier_etat_brassage();
            }
        }
        
        // ====================================================================
        // LOGS PÉRIODIQUES (toutes les 10 secondes)
        // ====================================================================
        
        if (compteur_cycles % 10 == 0) {
            if (transfert_est_actif()) {
                ESP_LOGI(TAG, "Transfert: %.1f L / %.1f L (%.0f%%)",
                         transfert_get_volume_cycle(),
                         transfert_get_volume_cycle(), // TODO: volume cible
                         transfert_get_pourcentage());
            }
            
            if (brassage_est_actif()) {
                ESP_LOGI(TAG, "Brassage: %s, %lu min restantes",
                         brassage_get_etat() == ETAT_BRASSAGE_MARCHE ? "MARCHE" : "PAUSE",
                         brassage_get_temps_restant() / 60);
            }
        }
    }
}

// ============================================================================
// ARRÊT D'URGENCE
// ============================================================================

/**
 * @brief Arrêt d'urgence tous automatismes
 */
void automatismes_arret_urgence(void) {
    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, "  ARRÊT D'URGENCE AUTOMATISMES");
    ESP_LOGW(TAG, "========================================");
    
    transfert_desactiver();
    brassage_desactiver();
    
    ESP_LOGW(TAG, "Tous automatismes arrêtés");
}

/**
 * @brief Désactive tous les automatismes
 */
void automatismes_desactiver_tout(void) {
    ESP_LOGI(TAG, "Désactivation de tous les automatismes");
    
    transfert_desactiver();
    brassage_desactiver();
}

// ============================================================================
// ÉTAT ET DIAGNOSTICS
// ============================================================================

/**
 * @brief Obtient état complet
 */
esp_err_t automatismes_get_etat_complet(etat_automatismes_t* etat) {
    if (etat == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Transfert
    etat->etat_transfert = transfert_get_etat();
    etat->volume_transfere_cycle_litres = transfert_get_volume_cycle();
    
    // Brassage
    etat->etat_brassage = brassage_get_etat();
    
    // Cuve (TODO: intégration gestion_securites)
    etat->etat_cuve_avant = ETAT_CUVE_AVANT_NORMALE;    
    return ESP_OK;
}

/**
 * @brief Affiche statistiques
 */
void automatismes_get_stats(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  STATISTIQUES AUTOMATISMES");
    ESP_LOGI(TAG, "========================================");
    
    // Transfert
    ESP_LOGI(TAG, "Transfert:");
    etat_transfert_t etat_tr = transfert_get_etat();
    const char* etat_tr_str = "INACTIF";
    switch (etat_tr) {
        case ETAT_TRANSFERT_EN_COURS: etat_tr_str = "EN_COURS"; break;
        case ETAT_TRANSFERT_TERMINE: etat_tr_str = "TERMINE"; break;
        //case : etat_tr_str = "ERREUR"; break;
        default: break;
    }
    ESP_LOGI(TAG, "  - État: %s", etat_tr_str);
    ESP_LOGI(TAG, "  - Volume cycle: %.1f L", transfert_get_volume_cycle());
    ESP_LOGI(TAG, "  - Progression: %.0f%%", transfert_get_pourcentage());
    
    // Brassage
    ESP_LOGI(TAG, "Brassage:");
    etat_brassage_t etat_br = brassage_get_etat();
    const char* etat_br_str = "INACTIF";
    switch (etat_br) {
        case ETAT_BRASSAGE_MARCHE: etat_br_str = "MARCHE"; break;
        case ETAT_BRASSAGE_PAUSE: etat_br_str = "PAUSE"; break;
        //case ETAT_BRASSAGE_PAUSE: etat_br_str = "PAUSE"; break;
        default: break;
    }
    ESP_LOGI(TAG, "  - État: %s", etat_br_str);
    ESP_LOGI(TAG, "  - Temps restant: %lu min", brassage_get_temps_restant() / 60);
    //ESP_LOGI(TAG, "  - Cycles: %lu", brassage_get_nombre_cycles());
    
    ESP_LOGI(TAG, "========================================");
}

#endif // BOARD_TYPE_AVANT
