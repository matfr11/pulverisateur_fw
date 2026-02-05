/**
 * @file securites_manager.cpp
 * @brief Gestionnaire principal sécurités
 * 
 * Coordination de toutes les sécurités système
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#include "securites.h"
#include "actionneurs.h"
#include "automatismes.h"
#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "cJSON.h"          // Pour les erreurs cJSON
#include "esp_timer.h"      // Pour l'erreur esp_timer_get_time

extern "C" {
    esp_err_t mqtt_publish(const char* topic, const char* payload, int qos, bool retain);
}
// Déclaration externe de la fonction de synchro
extern "C" EventGroupHandle_t app_get_event_group_systeme(void);
#define BIT_CAPTEURS_PRETS (1 << 2)

static const char* TAG = "SECURITES";

// ============================================================================
// VARIABLES PRIVÉES
// ============================================================================

// État arrêt d'urgence global
static bool g_arret_urgence_actif = false;
static SemaphoreHandle_t g_mutex_urgence = NULL;

// Compteurs alertes
static uint32_t g_nombre_alertes_totales = 0;

// ============================================================================
// DÉCLARATIONS FORWARD
// ============================================================================

#ifdef BOARD_TYPE_AVANT
extern esp_err_t detection_cuve_vide_init(void);
extern void detection_cuve_vide_surveiller(void);
#endif

#ifdef BOARD_TYPE_ARRIERE
// Pas de détection cuve sur carte arrière
#endif

static void tache_securites(void* pvParameters);

// ============================================================================
// INITIALISATION
// ============================================================================

/**
 * @brief Initialise le système de sécurités
 */
esp_err_t securites_init(void) {
    ESP_LOGI(TAG, "Initialisation système de sécurités...");
    
    // Créer mutex
    g_mutex_urgence = xSemaphoreCreateMutex();
    if (g_mutex_urgence == NULL) {
        ESP_LOGE(TAG, "Erreur création mutex");
        return ESP_FAIL;
    }
    
    // État initial
    g_arret_urgence_actif = false;
    g_nombre_alertes_totales = 0;
    
#ifdef BOARD_TYPE_AVANT
    // Initialiser détection cuve vide
    esp_err_t ret = detection_cuve_vide_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init détection cuve vide");
        return ret;
    }
#endif
    
    ESP_LOGI(TAG, "✓ Sécurités initialisées");
    return ESP_OK;
}

// ============================================================================
// TÂCHE PÉRIODIQUE
// ============================================================================

/**
 * @brief Démarre la tâche de surveillance
 */
void securites_demarrer_tache(void) {
    BaseType_t ret = xTaskCreate(
        tache_securites,
        "securites",
        3072,
        NULL,
        5, // Priorité haute pour sécurités
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Erreur création tâche sécurités");
    } else {
        ESP_LOGI(TAG, "Tâche sécurités démarrée");
    }
}

/**
 * @brief Tâche périodique surveillance sécurités
 * 
 * Fréquence: 500ms (2 Hz) - suffisant pour sécurités
 */
static void tache_securites(void* pvParameters) {
    (void)pvParameters;
    
    // --- NOUVEAU : ATTENTE SYNCHRO ---
    EventGroupHandle_t ev_sys = app_get_event_group_systeme();
    if (ev_sys != NULL) {
        ESP_LOGI(TAG, "Tâche sécurités en attente des capteurs...");
        xEventGroupWaitBits(ev_sys, BIT_CAPTEURS_PRETS, pdFALSE, pdTRUE, portMAX_DELAY);
    }
    
    TickType_t derniere_execution = xTaskGetTickCount();
    const TickType_t periode = pdMS_TO_TICKS(500);
    
    ESP_LOGI(TAG, "Tâche sécurités opérationnelle (période: 500 ms)");
    
    while (1) {
        vTaskDelayUntil(&derniere_execution, periode);
        
        // Si arrêt d'urgence actif, ne rien faire d'autre
        if (g_arret_urgence_actif) {
            continue;
        }
        
        // ====================================================================
        // CARTE AVANT - Détection cuve vide
        // ====================================================================
        
#ifdef BOARD_TYPE_AVANT
        detection_cuve_vide_surveiller();
#endif
        
        // ====================================================================
        // CARTE ARRIÈRE - Vérification timeouts vannes
        // ====================================================================
        
#ifdef BOARD_TYPE_ARRIERE
        // Les timeouts vannes sont déjà gérés par actionneurs_arriere.cpp
        // Rien à faire ici
#endif
    }
}

// ============================================================================
// ARRÊT D'URGENCE
// ============================================================================

/**
 * @brief Arrêt d'urgence global
 */
void securites_arret_urgence_global(void) {
    if (xSemaphoreTake(g_mutex_urgence, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    
    ESP_LOGE(TAG, "========================================");
    ESP_LOGE(TAG, "  ARRÊT D'URGENCE GLOBAL");
    ESP_LOGE(TAG, "========================================");
    
    g_arret_urgence_actif = true;
    
    // Arrêter tous automatismes
    extern void automatismes_arret_urgence(void);
    automatismes_arret_urgence();
    
    // Arrêter tous actionneurs
    extern void actionneurs_arret_urgence(void);
    actionneurs_arret_urgence();
    
    // Publier état
    extern void securites_publier_arret_urgence(bool);
    securites_publier_arret_urgence(true);
    
    // Publier alerte
    securites_publier_alerte("ARRET_URGENCE", 
                            "Arrêt d'urgence global activé", 
                            2);
    
    xSemaphoreGive(g_mutex_urgence);
    
    ESP_LOGE(TAG, "Système en arrêt d'urgence");
}

/**
 * @brief Vérifie arrêt d'urgence
 */
bool securites_est_en_arret_urgence(void) {
    return g_arret_urgence_actif;
}

/**
 * @brief Libère arrêt d'urgence
 */
void securites_liberer_arret_urgence(void) {
    if (xSemaphoreTake(g_mutex_urgence, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    
    if (!g_arret_urgence_actif) {
        ESP_LOGW(TAG, "Arrêt d'urgence déjà libéré");
        xSemaphoreGive(g_mutex_urgence);
        return;
    }
    
    ESP_LOGW(TAG, "Libération arrêt d'urgence");
    
    g_arret_urgence_actif = false;
    
    // Publier état
    extern void securites_publier_arret_urgence(bool);
    securites_publier_arret_urgence(false);
    
    // Publier alerte
    securites_publier_alerte("ARRET_URGENCE", 
                            "Arrêt d'urgence libéré - système opérationnel", 
                            1);
    
    xSemaphoreGive(g_mutex_urgence);
    
    ESP_LOGI(TAG, "Système libéré - opérations possibles");
}

// ============================================================================
// STATISTIQUES
// ============================================================================

/**
 * @brief Obtient nombre alertes
 */
uint32_t securites_get_nombre_alertes(void) {
    return g_nombre_alertes_totales;
}

/**
 * @brief Affiche statistiques
 */
void securites_get_stats(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  STATISTIQUES SÉCURITÉS");
    ESP_LOGI(TAG, "========================================");
    
    ESP_LOGI(TAG, "Arrêt d'urgence: %s", 
             g_arret_urgence_actif ? "ACTIF" : "Libéré");
    ESP_LOGI(TAG, "Nombre alertes total: %lu", g_nombre_alertes_totales);
    
#ifdef BOARD_TYPE_AVANT
    ESP_LOGI(TAG, "Cuve avant: %s", 
             securites_cuve_avant_est_vide() ? "VIDE" : "OK");
    extern uint32_t detection_cuve_vide_get_nombre_detections(void);
    ESP_LOGI(TAG, "Détections cuve vide: %lu", 
             detection_cuve_vide_get_nombre_detections());
#endif
    
    ESP_LOGI(TAG, "========================================");
}

// ============================================================================
// COMPATIBILITÉ AVEC AUTRES COMPOSANTS
// ============================================================================

/**
 * @brief Incrémente compteur alertes
 * Appelé par securites_publier_alerte
 */
void securites_incrementer_compteur_alertes(void) {
    g_nombre_alertes_totales++;
}

// Surcharge de securites_publier_alerte pour incrémenter compteur
void securites_publier_alerte(const char* type, const char* description, int niveau) {
    extern void securites_publier_alerte_mqtt(const char*, const char*, int);
    
    // Incrémenter compteur
    securites_incrementer_compteur_alertes();
    
    // Publier via MQTT (implémenté dans securites_mqtt.cpp)
    // On utilise un nom différent pour éviter récursion
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "type", type);
    cJSON_AddStringToObject(json, "description", description);
    cJSON_AddNumberToObject(json, "niveau", niveau);
    cJSON_AddNumberToObject(json, "timestamp_ms", esp_timer_get_time() / 1000);
    
    const char* niveau_str = "INFO";
    if (niveau == 1) niveau_str = "WARNING";
    else if (niveau >= 2) niveau_str = "CRITIQUE";
    cJSON_AddStringToObject(json, "niveau_str", niveau_str);
    
    char* payload = cJSON_PrintUnformatted(json);
    
    extern esp_err_t mqtt_publish(const char*, const char*, int, bool);
    mqtt_publish("pulverisateur/securites/alertes", payload, 1, false);
    
    cJSON_Delete(json);
    free(payload);
    
    ESP_LOGW(TAG, "Alerte [%s] %s (niveau %d)", type, description, niveau);
}
