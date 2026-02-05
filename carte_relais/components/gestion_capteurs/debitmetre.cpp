/**
 * @file debitmetre.cpp
 * @brief Gestion débitmètre à impulsions
 * 
 * Principe de fonctionnement:
 * - Débitmètre génère des impulsions (fronts descendants)
 * - Chaque impulsion = 1/K litres (K = facteur débitmètre)
 * - ISR compte les impulsions
 * - Calcul périodique du débit instantané
 * 
 * Calculs:
 * - Volume (L) = impulsions / facteur_k
 * - Débit (L/min) = delta_impulsions / facteur_k / delta_temps_min
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#include "capteurs.h"
#include "board_config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#ifdef BOARD_TYPE_AVANT

static const char* TAG = "DEBITMETRE";

// ============================================================================
// VARIABLES PRIVÉES
// ============================================================================

// Données débitmètre
static donnees_debitmetre_t g_donnees_debitmetre = {0};

// Facteur K (impulsions par litre)
static float g_facteur_k = CONFIG_DEFAULT_FACTEUR_K_DEBITMETRE;

// Pour calcul débit instantané
static uint32_t g_impulsions_precedentes = 0;
static int64_t g_timestamp_precedent_us = 0;

// Sémaphore pour protéger accès concurrent
static SemaphoreHandle_t g_mutex_debitmetre = NULL;

// Mode simulation
#if MODE_SIMULATION
static bool g_mode_simulation = true;
static float g_debit_simule_lpm = 0.0f;
static TimerHandle_t g_timer_simulation = NULL;
#else
static bool g_mode_simulation = false;
#endif

// Calibration
static bool g_calibration_active = false;
static uint32_t g_calibration_impulsions_debut = 0;

// ============================================================================
// ISR DÉBITMÈTRE
// ============================================================================

/**
 * @brief ISR appelée à chaque impulsion débitmètre
 * 
 * CRITIQUE: Cette fonction doit être TRÈS rapide
 * Pas de malloc, pas de printf, pas de float
 */
static void IRAM_ATTR debitmetre_isr_handler(void* arg) {
    (void)arg;
    
    // Incrémenter compteur atomiquement
    g_donnees_debitmetre.impulsions_totales++;
    
    // Sauvegarder timestamp
    g_donnees_debitmetre.timestamp_derniere_impulsion_ms = 
        (uint32_t)(esp_timer_get_time() / 1000);
}

// ============================================================================
// INITIALISATION
// ============================================================================
extern "C" {
esp_err_t debitmetre_init(float facteur_k) {
    ESP_LOGI(TAG, "Initialisation débitmètre...");

        // Sauvegarder le facteur K passé en paramètre
    g_facteur_k = facteur_k; 
    
    // 1. D'ABORD les ressources logicielles (Mutex et données)
    if (g_mutex_debitmetre == NULL) {
        g_mutex_debitmetre = xSemaphoreCreateMutex();
    }
    
    if (g_mutex_debitmetre == NULL) {
        ESP_LOGE(TAG, "Erreur création mutex");
        return ESP_FAIL;
    }
    
    // Initialiser les structures en mémoire
    memset(&g_donnees_debitmetre, 0, sizeof(donnees_debitmetre_t));
    g_timestamp_precedent_us = esp_timer_get_time();
    
    // 2. ENSUITE la partie matérielle
    if (!g_mode_simulation) {
        // Installation du service ISR (si pas déjà fait)
        esp_err_t ret = gpio_install_isr_service(0);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Erreur ISR service: %s", esp_err_to_name(ret));
            return ret;
        }
        
        // SÉCURITÉ : On s'assure que l'interruption est propre avant d'ajouter le handler
        gpio_intr_disable(GPIO_DEBITMETRE_IMPULSION); 

        ret = gpio_isr_handler_add(GPIO_DEBITMETRE_IMPULSION, 
                                   debitmetre_isr_handler, 
                                   NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erreur ISR handler: %s", esp_err_to_name(ret));
            return ret;
        }
        
        // 3. ENFIN, on active l'interruption une fois que tout est prêt
        gpio_intr_enable(GPIO_DEBITMETRE_IMPULSION);
        
        ESP_LOGI(TAG, "✓ ISR débitmètre installée et activée (GPIO %d)", GPIO_DEBITMETRE_IMPULSION);
    } else {
        ESP_LOGI(TAG, "Mode simulation activé");
    }
    
    ESP_LOGI(TAG, "Facteur K: %.2f imp/L", g_facteur_k);
    return ESP_OK;
}

// ============================================================================
// CALCUL DÉBIT ET VOLUME
// ============================================================================

/**
 * @brief Calcule le débit instantané
 * Appelé périodiquement par la tâche capteurs
 */
void debitmetre_calculer_debit(void) {
    if (g_mutex_debitmetre == NULL) return;

    // 1. On tente de prendre le mutex. Si on échoue (déjà pris), on sort.
    if (xSemaphoreTake(g_mutex_debitmetre, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }

    // --- ZONE PROTÉGÉE ---
    int64_t maintenant_us = esp_timer_get_time();
    
    // On travaille sur une copie locale pour éviter les sauts de valeur pendant le calcul
    uint32_t impulsions_actuelles = g_donnees_debitmetre.impulsions_totales;
    
    float delta_temps_min = (float)(maintenant_us - g_timestamp_precedent_us) / 60000000.0f;
    uint32_t delta_impulsions = impulsions_actuelles - g_impulsions_precedentes;
    
    if (delta_temps_min > 0.0f && g_facteur_k > 0.0f) {
        float delta_litres = (float)delta_impulsions / g_facteur_k;
        g_donnees_debitmetre.debit_instantane_lpm = delta_litres / delta_temps_min;
        g_donnees_debitmetre.volume_total_litres = (float)impulsions_actuelles / g_facteur_k;
    } else {
        g_donnees_debitmetre.debit_instantane_lpm = 0.0f;
    }
    
    // Sauvegarde pour le prochain cycle
    g_impulsions_precedentes = impulsions_actuelles;
    g_timestamp_precedent_us = maintenant_us;
    
    // Timeout : si pas d'impulsions depuis 2s, débit = 0
    uint32_t depuis_derniere_ms = (uint32_t)(maintenant_us / 1000) - g_donnees_debitmetre.timestamp_derniere_impulsion_ms;
    if (depuis_derniere_ms > 2000) {
        g_donnees_debitmetre.debit_instantane_lpm = 0.0f;
    }

    // 2. LIBÉRATION DU MUTEX (Crucial !)
    xSemaphoreGive(g_mutex_debitmetre);
    // --- FIN ZONE PROTÉGÉE ---

    // Log avec les impulsions comme tu le souhaitais
    ESP_LOGD(TAG, "Débit: %.2f L/min, Volume: %.2f L, Impulsions: %lu",
             g_donnees_debitmetre.debit_instantane_lpm,
             g_donnees_debitmetre.volume_total_litres,
             impulsions_actuelles);
}

// ============================================================================
// FONCTIONS PUBLIQUES
// ============================================================================

/**
 * @brief Obtient les données débitmètre
 */
esp_err_t debitmetre_get_donnees(donnees_debitmetre_t* donnees) {
    if (donnees == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutex_debitmetre, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(donnees, &g_donnees_debitmetre, sizeof(donnees_debitmetre_t));
    
    xSemaphoreGive(g_mutex_debitmetre);
    
    return ESP_OK;
}

/**
 * @brief Obtient débit instantané (Thread-safe et Safe-init)
 */
float debitmetre_get_debit(void) {
    float debit = 0.0f;
    
    // 1. PROTECTION CRUCIALE : On vérifie si le mutex existe
    if (g_mutex_debitmetre == NULL) {
        return 0.0f; // On retourne 0 au lieu de crasher le CPU
    }
    
    // 2. TENTATIVE DE LECTURE : On utilise un timeout court (10ms)
    if (xSemaphoreTake(g_mutex_debitmetre, pdMS_TO_TICKS(10)) == pdTRUE) {
        debit = g_donnees_debitmetre.debit_instantane_lpm;
        xSemaphoreGive(g_mutex_debitmetre);
    } else {
        ESP_LOGW("DEBIT", "Timeout mutex debitmetre");
    }
    
    return debit;
}
/**
 * @brief Obtient volume total (Thread-safe et protégé contre init précoce)
 */
float debitmetre_get_volume_total(void) {
    float volume = 0.0f;
    
    // 1. PROTECTION : Empêche le crash si le mutex n'est pas encore créé
    if (g_mutex_debitmetre == NULL) {
        return 0.0f; 
    }
    
    // 2. ACCÈS SÉCURISÉ : Avec timeout pour éviter de bloquer la tâche capteurs
    if (xSemaphoreTake(g_mutex_debitmetre, pdMS_TO_TICKS(10)) == pdTRUE) {
        volume = g_donnees_debitmetre.volume_total_litres;
        xSemaphoreGive(g_mutex_debitmetre);
    }
    
    return volume;
}

/**
 * @brief Obtient impulsions totales
 */
uint32_t debitmetre_get_impulsions(void) {
    return g_donnees_debitmetre.impulsions_totales;
}

/**
 * @brief Reset volume
 */
void debitmetre_reset_volume(void) {
    if (xSemaphoreTake(g_mutex_debitmetre, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_donnees_debitmetre.impulsions_totales = 0;
        g_donnees_debitmetre.volume_total_litres = 0.0f;
        g_impulsions_precedentes = 0;
        
        ESP_LOGI(TAG, "Volume réinitialisé");
        
        xSemaphoreGive(g_mutex_debitmetre);
    }
}

/**
 * @brief Configure facteur K
 */
void debitmetre_set_facteur_k(float facteur_k) {
    if (facteur_k <= 0.0f) {
        ESP_LOGE(TAG, "Facteur K invalide: %.2f", facteur_k);
        return;
    }
    
    g_facteur_k = facteur_k;
    ESP_LOGI(TAG, "Facteur K configuré: %.2f impulsions/litre", g_facteur_k);
}

/**
 * @brief Obtient facteur K
 */
float debitmetre_get_facteur_k(void) {
    return g_facteur_k;
}
} // extern "C"
// ============================================================================
// CALIBRATION
// ============================================================================

/**
 * @brief Démarre calibration
 */
void debitmetre_calibration_demarrer(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  CALIBRATION DÉBITMÈTRE");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "1. Préparer un volume connu (ex: 100L)");
    ESP_LOGI(TAG, "2. Faire passer le liquide");
    ESP_LOGI(TAG, "3. Appeler debitmetre_calibration_terminer(volume)");
    
    g_calibration_active = true;
    g_calibration_impulsions_debut = g_donnees_debitmetre.impulsions_totales;
    
    ESP_LOGI(TAG, "Impulsions au départ: %lu", g_calibration_impulsions_debut);
}

/**
 * @brief Termine calibration
 */
float debitmetre_calibration_terminer(float volume_reel_litres) {
    if (!g_calibration_active) {
        ESP_LOGE(TAG, "Aucune calibration en cours");
        return g_facteur_k;
    }
    
    uint32_t impulsions_totales = g_donnees_debitmetre.impulsions_totales;
    uint32_t impulsions_mesurees = impulsions_totales - g_calibration_impulsions_debut;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  FIN CALIBRATION");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Impulsions mesurées: %lu", impulsions_mesurees);
    ESP_LOGI(TAG, "Volume réel: %.2f L", volume_reel_litres);
    
    if (volume_reel_litres <= 0.0f) {
        ESP_LOGE(TAG, "Volume réel invalide");
        g_calibration_active = false;
        return g_facteur_k;
    }
    
    // Calculer nouveau facteur K
    float nouveau_k = (float)impulsions_mesurees / volume_reel_litres;
    
    ESP_LOGI(TAG, "Ancien facteur K: %.2f", g_facteur_k);
    ESP_LOGI(TAG, "Nouveau facteur K: %.2f", nouveau_k);
    ESP_LOGI(TAG, "Différence: %.1f%%", 
             ((nouveau_k - g_facteur_k) / g_facteur_k) * 100.0f);
    
    g_facteur_k = nouveau_k;
    g_calibration_active = false;
    
    // Sauvegarder en NVS
    // TODO: Appeler fonction sauvegarde config
    
    ESP_LOGI(TAG, "Calibration terminée");
    
    return nouveau_k;
}

/**
 * @brief Vérifie calibration active
 */
bool debitmetre_est_en_calibration(void) {
    return g_calibration_active;
}

// ============================================================================
// MODE SIMULATION
// ============================================================================

#if MODE_SIMULATION

/**
 * @brief Callback timer simulation
 */
static void timer_simulation_callback(TimerHandle_t xTimer) {
    (void)xTimer;
    
    // Simuler impulsion
    if (g_debit_simule_lpm > 0.0f) {
        debitmetre_simuler_impulsion();
    }
}

void capteurs_set_simulation(bool enable) {
    g_mode_simulation = enable;
    ESP_LOGI(TAG, "Mode simulation: %s", enable ? "ACTIVÉ" : "DÉSACTIVÉ");
}

void debitmetre_simuler_impulsion(void) {
    if (g_mode_simulation) {
        g_donnees_debitmetre.impulsions_totales++;
        g_donnees_debitmetre.timestamp_derniere_impulsion_ms = 
            (uint32_t)(esp_timer_get_time() / 1000);
    }
}

void debitmetre_simuler_debit_constant(float debit_lpm) {
    g_debit_simule_lpm = debit_lpm;
    
    if (debit_lpm > 0.0f) {
        // Calculer période timer
        // debit_lpm = impulsions/s * 60 / facteur_k
        // impulsions/s = debit_lpm * facteur_k / 60
        float impulsions_par_sec = debit_lpm * g_facteur_k / 60.0f;
        uint32_t periode_ms = (uint32_t)(1000.0f / impulsions_par_sec);
        
        ESP_LOGI(TAG, "Simulation débit: %.2f L/min (%.1f imp/s, période %lu ms)",
                 debit_lpm, impulsions_par_sec, periode_ms);
        
        // Créer timer si nécessaire
        if (g_timer_simulation == NULL) {
            g_timer_simulation = xTimerCreate(
                "sim_debit",
                pdMS_TO_TICKS(periode_ms),
                pdTRUE, // Auto-reload
                NULL,
                timer_simulation_callback
            );
        } else {
            xTimerChangePeriod(g_timer_simulation, pdMS_TO_TICKS(periode_ms), 0);
        }
        
        xTimerStart(g_timer_simulation, 0);
    } else {
        // Arrêter simulation
        if (g_timer_simulation != NULL) {
            xTimerStop(g_timer_simulation, 0);
        }
        ESP_LOGI(TAG, "Simulation débit arrêtée");
    }
}

#endif // MODE_SIMULATION

#endif // BOARD_TYPE_AVANT
