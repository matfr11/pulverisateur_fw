/**
 * @file actionneurs_manager.cpp
 * @brief Gestionnaire principal actionneurs
 * 
 * Responsabilités:
 * - Initialisation
 * - Tâche périodique de surveillance
 * - Vérification timeouts
 * - Vérification interlock
 * - Publication états périodiques
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#include "actionneurs.h"
#include "board_config.h"
#include "mqtt_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = LOG_TAG_ACTIONNEURS;

// ============================================================================
// DÉCLARATIONS FORWARD
// ============================================================================

#ifdef BOARD_TYPE_ARRIERE
extern void actionneurs_surveiller_timeouts(void);
extern void interlock_verifier_tout(void);
#endif

static void tache_actionneurs(void* pvParameters);

// ============================================================================
// INITIALISATION
// ============================================================================

/**
 * @brief Initialise le système d'actionneurs
 */
esp_err_t actionneurs_init(void) {
    ESP_LOGI(TAG, "Initialisation actionneurs...");
    
    // Les GPIO sont déjà configurés dans app_init.cpp
    // Ici on initialise juste les états logiques
    
#ifdef BOARD_TYPE_AVANT
    ESP_LOGI(TAG, "Mode AVANT: Pompe, vanne 3V, phares");
    
    // S'assurer que tout est à l'arrêt au démarrage
    actionneur_pompe_set(ETAT_POMPE_ARRET);
    actionneur_vanne_3voies_set(POSITION_VANNE_3V_BRASSAGE);
    actionneur_phares_avant_set(false);
    
    ESP_LOGI(TAG, "✓ Actionneurs AVANT initialisés");
#endif

#ifdef BOARD_TYPE_ARRIERE
    ESP_LOGI(TAG, "Mode ARRIERE: Vannes 3 fils, phares");
    
    // S'assurer que toutes les vannes sont inactives (SÉCURITÉ)
    actionneurs_arreter_toutes_vannes();
    actionneur_phares_arriere_set(false);
    
    ESP_LOGI(TAG, "✓ Actionneurs ARRIÈRE initialisés");
#endif

    return ESP_OK;
}

// ============================================================================
// TÂCHE PÉRIODIQUE
// ============================================================================

/**
 * @brief Démarre la tâche de gestion des actionneurs
 */
void actionneurs_demarrer_tache(void) {
    BaseType_t ret = xTaskCreate(
        tache_actionneurs,
        "actionneurs",
        TASK_STACK_ACTIONNEURS,
        NULL,
        TASK_PRIORITY_ACTIONNEURS,
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Erreur création tâche actionneurs");
    } else {
        ESP_LOGI(TAG, "Tâche actionneurs démarrée");
    }
}

/**
 * @brief Tâche périodique de gestion des actionneurs
 * 
 * Fréquence: 50ms (20 Hz)
 * 
 * Responsabilités:
 * - Surveillance timeouts vannes
 * - Vérification interlock
 * - Publication états périodiques
 */
static void tache_actionneurs(void* pvParameters) {
    (void)pvParameters;
    
    TickType_t derniere_execution = xTaskGetTickCount();
    const TickType_t periode = pdMS_TO_TICKS(PERIODE_MAJ_ACTIONNEURS);
    
    uint32_t compteur_cycles = 0;
    
    ESP_LOGI(TAG, "Tâche actionneurs opérationnelle (période: %d ms)", 
             PERIODE_MAJ_ACTIONNEURS);
    
    while (1) {
        vTaskDelayUntil(&derniere_execution, periode);
        compteur_cycles++;
        
        // ====================================================================
        // CARTE AVANT - Surveillance
        // ====================================================================
        
#ifdef BOARD_TYPE_AVANT
        // Pas de surveillance particulière pour carte avant
        // Les actionneurs sont simples (relais ON/OFF)
        
        // Publication états périodique (toutes les secondes = 20 cycles)
        if (compteur_cycles % 20 == 0) {
            ESP_LOGD(TAG, "Pompe: %d, Vanne3V: %d", 
                     actionneur_pompe_get(), 
                     actionneur_vanne_3voies_get());
        }
#endif
        
        // ====================================================================
        // CARTE ARRIÈRE - Surveillance critique
        // ====================================================================
        
#ifdef BOARD_TYPE_ARRIERE
        // Vérifier timeouts vannes (CRITIQUE)
        actionneurs_surveiller_timeouts();
        
        // Vérifier interlock (CRITIQUE)
        // Toutes les 10 cycles = 500ms
        if (compteur_cycles % 10 == 0) {
            interlock_verifier_tout();
        }
        
        // Logs périodiques (toutes les secondes)
        if (compteur_cycles % 20 == 0) {
            ESP_LOGD(TAG, "Vanne 2m: %d, Vanne bout: %d",
                     actionneur_vanne_2m_get(),
                     actionneur_vanne_bout_rampe_get());
        }
#endif
    }
}

// ============================================================================
// ARRÊT D'URGENCE GLOBAL
// ============================================================================

/**
 * @brief Arrêt d'urgence de tous les actionneurs
 * 
 * Appelé en cas d'erreur critique ou demande utilisateur
 */
void actionneurs_arret_urgence(void) {
    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, "  ARRÊT D'URGENCE ACTIONNEURS");
    ESP_LOGW(TAG, "========================================");
    
#ifdef BOARD_TYPE_AVANT
    actionneur_pompe_set(ETAT_POMPE_ARRET);
    actionneur_vanne_3voies_set(POSITION_VANNE_3V_BRASSAGE); // Position sûre
    ESP_LOGI(TAG, "Pompe arrêtée, vanne en brassage");
#endif

#ifdef BOARD_TYPE_ARRIERE
    actionneurs_arreter_toutes_vannes();
    ESP_LOGI(TAG, "Toutes vannes arrêtées");
#endif
    
    ESP_LOGW(TAG, "Arrêt d'urgence terminé");
}

// ============================================================================
// STATISTIQUES
// ============================================================================

/**
 * @brief Obtient les statistiques actionneurs
 */
void actionneurs_get_stats(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  STATISTIQUES ACTIONNEURS");
    ESP_LOGI(TAG, "========================================");
    
#ifdef BOARD_TYPE_AVANT
    ESP_LOGI(TAG, "Carte AVANT:");
    ESP_LOGI(TAG, "  - Pompe: %s", 
             actionneur_pompe_get() == ETAT_POMPE_MARCHE ? "MARCHE" : "ARRÊT");
    ESP_LOGI(TAG, "  - Vanne 3V: %s",
             actionneur_vanne_3voies_get() == POSITION_VANNE_3V_TRANSFERT ? 
             "TRANSFERT" : "BRASSAGE");
#endif

#ifdef BOARD_TYPE_ARRIERE
    ESP_LOGI(TAG, "Carte ARRIÈRE:");
    
    const char* etat_v2m;
    switch (actionneur_vanne_2m_get()) {
        case ETAT_VANNE_3FILS_OUVERTURE: etat_v2m = "OUVERTURE"; break;
        case ETAT_VANNE_3FILS_FERMETURE: etat_v2m = "FERMETURE"; break;
        case ETAT_VANNE_3FILS_TIMEOUT: etat_v2m = "TIMEOUT"; break;
        default: etat_v2m = "INACTIF"; break;
    }
    ESP_LOGI(TAG, "  - Vanne 2m: %s", etat_v2m);
    
    const char* etat_vb;
    switch (actionneur_vanne_bout_rampe_get()) {
        case ETAT_VANNE_3FILS_OUVERTURE: etat_vb = "OUVERTURE"; break;
        case ETAT_VANNE_3FILS_FERMETURE: etat_vb = "FERMETURE"; break;
        case ETAT_VANNE_3FILS_TIMEOUT: etat_vb = "TIMEOUT"; break;
        default: etat_vb = "INACTIF"; break;
    }
    ESP_LOGI(TAG, "  - Vanne bout: %s", etat_vb);
#endif
    
    ESP_LOGI(TAG, "========================================");
}
