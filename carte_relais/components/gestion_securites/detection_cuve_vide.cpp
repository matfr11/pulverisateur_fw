/**
 * @file detection_cuve_vide.cpp
 * @brief Détection cuve vide par analyse débit
 * 
 * Principe:
 * - Si pompe active ET débit < seuil pendant X secondes → cuve vide
 * - Seuil par défaut: 0.5 L/min
 * - Délai par défaut: 5 secondes
 * 
 * Actions automatiques:
 * - Arrêt automatismes (transfert, brassage)
 * - Alerte MQTT
 * - État système → DEGRADE
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#include "securites.h"
#include "actionneurs.h"
#include "capteurs.h"
#include "automatismes.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef BOARD_TYPE_AVANT

//extern "C" {
//    etat_pompe_t actionneur_pompe_get(void);
//}

static const char* TAG = "SECU_CUVE";

// ============================================================================
// VARIABLES PRIVÉES
// ============================================================================

// État détection cuve vide
static etat_cuve_avant_t g_etat_cuve = ETAT_CUVE_AVANT_NORMALE;

// Configuration détection
static float g_seuil_debit_cuve_vide_lpm = 0.5f;  // 0.5 L/min
static uint32_t g_delai_detection_ms = 5000;      // 5 secondes

// Timestamp début détection
static int64_t g_timestamp_debut_detection_us = 0;
static bool g_detection_en_cours = false;

// Mutex
static SemaphoreHandle_t g_mutex_cuve = NULL;

// Statistiques
static uint32_t g_nombre_detections_cuve_vide = 0;

// ============================================================================
// INITIALISATION
// ============================================================================

/**
 * @brief Initialise la détection cuve vide
 */
extern "C" {
esp_err_t detection_cuve_vide_init(void) {
    ESP_LOGI(TAG, "Initialisation détection cuve vide...");
    
    // Créer mutex
    g_mutex_cuve = xSemaphoreCreateMutex();
    if (g_mutex_cuve == NULL) {
        ESP_LOGE(TAG, "Erreur création mutex");
        return ESP_FAIL;
    }
    
    // État initial
    g_etat_cuve = ETAT_CUVE_AVANT_NORMALE;
    g_detection_en_cours = false;
    
    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  - Seuil débit: %.2f L/min", g_seuil_debit_cuve_vide_lpm);
    ESP_LOGI(TAG, "  - Délai détection: %lu ms", g_delai_detection_ms);
    
    ESP_LOGI(TAG, "✓ Détection cuve vide initialisée");
    return ESP_OK;
}
}   // extern "C"

// ============================================================================
// SURVEILLANCE
// ============================================================================

/**
 * @brief Surveille la cuve (appelé périodiquement)
 */
void detection_cuve_vide_surveiller(void) {
    // 1. PROTECTION ABSOLUE : Vérifier si le pointeur est initialisé
    // On ne passe JAMAIS g_mutex_cuve à une fonction FreeRTOS s'il est NULL
    if (g_mutex_cuve == NULL) {
        return; 
    }
    
    // 2. TENTATIVE DE PRISE : Sans blocage (0 tick)
    // On capture le résultat dans une variable pour être propre
    BaseType_t got_mutex = xQueueSemaphoreTake(g_mutex_cuve, 0);
    
    if (got_mutex != pdTRUE) {
        return; // Mutex occupé ou pas prêt, on ignore ce cycle
    }
    
    // --- À partir d'ici, on détient le mutex en toute sécurité ---

    // Vérifier si pompe active
    etat_pompe_t etat_pompe = actionneur_pompe_get();
    
    if (etat_pompe != ETAT_POMPE_MARCHE) {
        // Pompe arrêtée → reset détection
        if (g_detection_en_cours) {
            ESP_LOGD(TAG, "Pompe arrêtée - reset détection");
            g_detection_en_cours = false;
        }
        xSemaphoreGive(g_mutex_cuve);
        return;
    }
    
    // Pompe active → vérifier débit
    float debit = debitmetre_get_debit();
    
    if (debit < g_seuil_debit_cuve_vide_lpm) {
        // Débit faible détecté
        
        if (!g_detection_en_cours) {
            // Début de détection
            ESP_LOGW(TAG, "Débit faible détecté: %.2f L/min (seuil: %.2f)", 
                     debit, g_seuil_debit_cuve_vide_lpm);
            ESP_LOGW(TAG, "Début détection cuve vide (délai: %lu ms)", 
                     g_delai_detection_ms);
            
            g_detection_en_cours = true;
            g_timestamp_debut_detection_us = esp_timer_get_time();
        } else {
            // Détection en cours → vérifier délai
            int64_t maintenant_us = esp_timer_get_time();
            uint32_t temps_ecoule_ms = 
                (maintenant_us - g_timestamp_debut_detection_us) / 1000;
            
            if (temps_ecoule_ms >= g_delai_detection_ms) {
                // CUVE VIDE CONFIRMÉE !
                ESP_LOGE(TAG, "========================================");
                ESP_LOGE(TAG, "  CUVE VIDE DÉTECTÉE !");
                ESP_LOGE(TAG, "========================================");
                ESP_LOGE(TAG, "Débit: %.2f L/min (< %.2f)", 
                         debit, g_seuil_debit_cuve_vide_lpm);
                ESP_LOGE(TAG, "Durée: %lu ms (> %lu ms)", 
                         temps_ecoule_ms, g_delai_detection_ms);
                
                // Changer état
                g_etat_cuve = ETAT_CUVE_AVANT_VIDE;
                g_nombre_detections_cuve_vide++;
                
                // Arrêter automatismes
                ESP_LOGW(TAG, "Arrêt automatismes...");
                automatismes_arret_urgence();
                
                // Publier alerte
                extern void securites_publier_alerte(const char*, const char*, int);
                char msg[128];
                snprintf(msg, sizeof(msg), 
                         "Cuve vide détectée - Débit %.2f L/min pendant %lu ms",
                         debit, temps_ecoule_ms);
                securites_publier_alerte("CUVE_VIDE", msg, 2);
                
                // Reset détection pour éviter spam
                g_detection_en_cours = false;
            } else {
                ESP_LOGD(TAG, "Détection en cours: %lu / %lu ms", 
                         temps_ecoule_ms, g_delai_detection_ms);
            }
        }
    } else {
        // Débit OK
        if (g_detection_en_cours) {
            ESP_LOGI(TAG, "Débit revenu normal: %.2f L/min - annulation détection", 
                     debit);
            g_detection_en_cours = false;
        }
        
        // Remettre état OK si nécessaire
        if (g_etat_cuve == ETAT_CUVE_AVANT_VIDE) {
            ESP_LOGI(TAG, "Cuve revenue à niveau normal");
            g_etat_cuve = ETAT_CUVE_AVANT_NORMALE;
        }
    }
    
    xSemaphoreGive(g_mutex_cuve);
}

// ============================================================================
// FONCTIONS PUBLIQUES
// ============================================================================

/**
 * @brief Vérifie si cuve vide
 */
bool securites_cuve_avant_est_vide(void) {
    return g_etat_cuve == ETAT_CUVE_AVANT_VIDE;
}

/**
 * @brief Obtient état cuve
 */
etat_cuve_avant_t securites_get_etat_cuve_avant(void) {
    return g_etat_cuve;
}

/**
 * @brief Configure détection
 */
void securites_configurer_detection_cuve_vide(float seuil_debit_lpm, uint32_t delai_detection_ms) {
    if (seuil_debit_lpm <= 0.0f || delai_detection_ms == 0) {
        ESP_LOGE(TAG, "Paramètres invalides");
        return;
    }
    
    g_seuil_debit_cuve_vide_lpm = seuil_debit_lpm;
    g_delai_detection_ms = delai_detection_ms;
    
    ESP_LOGI(TAG, "Configuration détection mise à jour:");
    ESP_LOGI(TAG, "  - Seuil: %.2f L/min", seuil_debit_lpm);
    ESP_LOGI(TAG, "  - Délai: %lu ms", delai_detection_ms);
}

/**
 * @brief Obtient nombre détections
 */
uint32_t detection_cuve_vide_get_nombre_detections(void) {
    return g_nombre_detections_cuve_vide;
}

#endif // BOARD_TYPE_AVANT
