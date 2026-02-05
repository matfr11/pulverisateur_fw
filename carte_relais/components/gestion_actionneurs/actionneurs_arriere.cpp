/**
 * @file actionneurs_arriere.cpp
 * @brief Gestion actionneurs carte ARRIÈRE
 * 
 * Actionneurs:
 * - Vanne 2m (3 fils: 2 relais ouvrir/fermer)
 * - Vanne bout de rampe (3 fils: 2 relais ouvrir/fermer)
 * - Phares arrière (relais simple ON/OFF)
 * 
 * SÉCURITÉ CRITIQUE:
 * - Interlock: JAMAIS ouvrir + fermer simultanément
 * - Timeout: Coupure automatique après X secondes
 * - État machines pour chaque vanne
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#include "actionneurs.h"
#include "board_config.h"
#include "mqtt_manager.h"
#include "mqtt_topics.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#ifdef HAVE_HTTP_SERVER
#include "http_server.h"
#endif

#ifdef BOARD_TYPE_ARRIERE

static const char* TAG = LOG_TAG_ACTIONNEURS;
extern "C" {
// ============================================================================
// STRUCTURES PRIVÉES
// ============================================================================

typedef struct {
    const char* nom;
    gpio_num_t gpio_ouvrir;
    gpio_num_t gpio_fermer;
    etat_vanne_3fils_t etat;
    uint64_t timestamp_debut_action_us;
    uint32_t timeout_ms;
    bool timeout_active;
} vanne_3fils_t;

// ============================================================================
// VARIABLES PRIVÉES
// ============================================================================

// Vannes 3 fils
static vanne_3fils_t g_vanne_2m = {
    .nom = "vanne_2m",
    .gpio_ouvrir = GPIO_VANNE_2M_OUVRIR,
    .gpio_fermer = GPIO_VANNE_2M_FERMER,
    .etat = ETAT_VANNE_3FILS_INACTIF,
    .timestamp_debut_action_us = 0,
    .timeout_ms = TIMEOUT_VANNE_3FILS_DEFAUT_MS,
    .timeout_active = false
};

static vanne_3fils_t g_vanne_bout = {
    .nom = "vanne_bout_rampe",
    .gpio_ouvrir = GPIO_VANNE_BOUT_OUVRIR,
    .gpio_fermer = GPIO_VANNE_BOUT_FERMER,
    .etat = ETAT_VANNE_3FILS_INACTIF,
    .timestamp_debut_action_us = 0,
    .timeout_ms = TIMEOUT_VANNE_3FILS_DEFAUT_MS,
    .timeout_active = false
};

// Phares arrière
static bool g_phares_arriere = false;

// Mode simulation
#if MODE_SIMULATION
static bool g_mode_simulation = true;
#else
static bool g_mode_simulation = false;
#endif

// ============================================================================
// FONCTIONS PRIVÉES - INTERLOCK ET SÉCURITÉ
// ============================================================================

/**
 * @brief Arrête une vanne (tous relais OFF)
 * SÉCURITÉ CRITIQUE
 */
static void vanne_arreter(vanne_3fils_t* vanne) {
    if (vanne == NULL) return;
    
    if (g_mode_simulation) {
        ESP_LOGI(TAG, "[SIMULATION] %s → STOP", vanne->nom);
    } else {
        gpio_set_level(vanne->gpio_ouvrir, 0);
        gpio_set_level(vanne->gpio_fermer, 0);
        ESP_LOGI(TAG, "%s → STOP (relais OFF)", vanne->nom);
    }
    
    vanne->etat = ETAT_VANNE_3FILS_INACTIF;
    vanne->timeout_active = false;
}

/**
 * @brief Démarre ouverture vanne avec sécurités
 */
static esp_err_t vanne_ouvrir(vanne_3fils_t* vanne) {
    if (vanne == NULL) return ESP_ERR_INVALID_ARG;
    
    // INTERLOCK: S'assurer que fermeture est OFF
    if (!g_mode_simulation) {
        gpio_set_level(vanne->gpio_fermer, 0);
        vTaskDelay(pdMS_TO_TICKS(10)); // Délai sécurité
    }
    
    // Activer ouverture
    if (g_mode_simulation) {
        ESP_LOGI(TAG, "[SIMULATION] %s → OUVRIR", vanne->nom);
    } else {
        gpio_set_level(vanne->gpio_ouvrir, 1);
        ESP_LOGI(TAG, "%s → OUVRIR (relais ON)", vanne->nom);
    }
    
    vanne->etat = ETAT_VANNE_3FILS_OUVERTURE;
    vanne->timestamp_debut_action_us = esp_timer_get_time();
    vanne->timeout_active = true;
    
    return ESP_OK;
}

/**
 * @brief Démarre fermeture vanne avec sécurités
 */
static esp_err_t vanne_fermer(vanne_3fils_t* vanne) {
    if (vanne == NULL) return ESP_ERR_INVALID_ARG;
    
    // INTERLOCK: S'assurer que ouverture est OFF
    if (!g_mode_simulation) {
        gpio_set_level(vanne->gpio_ouvrir, 0);
        vTaskDelay(pdMS_TO_TICKS(10)); // Délai sécurité
    }
    
    // Activer fermeture
    if (g_mode_simulation) {
        ESP_LOGI(TAG, "[SIMULATION] %s → FERMER", vanne->nom);
    } else {
        gpio_set_level(vanne->gpio_fermer, 1);
        ESP_LOGI(TAG, "%s → FERMER (relais ON)", vanne->nom);
    }
    
    vanne->etat = ETAT_VANNE_3FILS_FERMETURE;
    vanne->timestamp_debut_action_us = esp_timer_get_time();
    vanne->timeout_active = true;
    
    return ESP_OK;
}

/**
 * @brief Vérifie timeout vanne et coupe si nécessaire
 */
static void vanne_verifier_timeout(vanne_3fils_t* vanne) {
    if (vanne == NULL || !vanne->timeout_active) return;
    
    uint64_t temps_ecoule_us = esp_timer_get_time() - vanne->timestamp_debut_action_us;
    uint64_t temps_ecoule_ms = temps_ecoule_us / 1000;
    
    if (temps_ecoule_ms > vanne->timeout_ms) {
        ESP_LOGE(TAG, "TIMEOUT %s ! (%llu ms > %lu ms)", 
                 vanne->nom, temps_ecoule_ms, vanne->timeout_ms);
        
        // SÉCURITÉ: Arrêt immédiat
        vanne_arreter(vanne);
        vanne->etat = ETAT_VANNE_3FILS_TIMEOUT;
        
        // Publier alerte
        extern esp_err_t mqtt_publish(const char* topic, const char* payload, int qos, bool retain);
        char payload[128];
        snprintf(payload, sizeof(payload), 
                 "{\"vanne\":\"%s\",\"timeout_ms\":%lu}", 
                 vanne->nom, vanne->timeout_ms);
        mqtt_publish(TOPIC_SECURITE_TIMEOUT_VANNE, payload, 1, false);
    }
}

// ============================================================================
// FONCTIONS PUBLIQUES - VANNE 2M
// ============================================================================

/**
 * @brief Commande vanne 2m
 */
esp_err_t actionneur_vanne_2m(const char* action) {
    if (action == NULL) return ESP_ERR_INVALID_ARG;
    
    esp_err_t ret = ESP_OK;
    
    if (strcmp(action, "OUVRIR") == 0) {
        ret = vanne_ouvrir(&g_vanne_2m);
    } else if (strcmp(action, "FERMER") == 0) {
        ret = vanne_fermer(&g_vanne_2m);
    } else if (strcmp(action, "STOP") == 0) {
        vanne_arreter(&g_vanne_2m);
    } else {
        ESP_LOGE(TAG, "Action vanne 2m invalide: %s", action);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Publier état
    mqtt_publier_etat_actionneur(TOPIC_ETAT_VANNE_2M, (int)g_vanne_2m.etat);
    
    // Mise à jour HTTP
#ifdef HAVE_HTTP_SERVER
    http_status_update_vanne_2m(g_vanne_2m.etat);
#endif
    
    return ret;
}

/**
 * @brief Obtient état vanne 2m
 */
etat_vanne_3fils_t actionneur_vanne_2m_get(void) {
    return g_vanne_2m.etat;
}

// ============================================================================
// FONCTIONS PUBLIQUES - VANNE BOUT DE RAMPE
// ============================================================================

/**
 * @brief Commande vanne bout de rampe
 */
esp_err_t actionneur_vanne_bout_rampe(const char* action) {
    if (action == NULL) return ESP_ERR_INVALID_ARG;
    
    esp_err_t ret = ESP_OK;
    
    if (strcmp(action, "OUVRIR") == 0) {
        ret = vanne_ouvrir(&g_vanne_bout);
    } else if (strcmp(action, "FERMER") == 0) {
        ret = vanne_fermer(&g_vanne_bout);
    } else if (strcmp(action, "STOP") == 0) {
        vanne_arreter(&g_vanne_bout);
    } else {
        ESP_LOGE(TAG, "Action vanne bout invalide: %s", action);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Publier état
    mqtt_publier_etat_actionneur(TOPIC_ETAT_VANNE_BOUT_RAMPE, (int)g_vanne_bout.etat);
    
    // Mise à jour HTTP
#ifdef HAVE_HTTP_SERVER
    http_status_update_vanne_bout(g_vanne_bout.etat);
#endif
    
    return ret;
}

/**
 * @brief Obtient état vanne bout de rampe
 */
etat_vanne_3fils_t actionneur_vanne_bout_rampe_get(void) {
    return g_vanne_bout.etat;
}

// ============================================================================
// FONCTIONS PUBLIQUES - PHARES ARRIÈRE
// ============================================================================

/**
 * @brief Commande phares arrière
 */
esp_err_t actionneur_phares_arriere_set(bool etat) {
    if (g_phares_arriere == etat) {
        ESP_LOGD(TAG, "Phares arrière déjà à l'état demandé");
        return ESP_OK;
    }
    
    g_phares_arriere = etat;
    
    if (g_mode_simulation) {
        ESP_LOGI(TAG, "[SIMULATION] Phares arrière → %s", etat ? "ON" : "OFF");
    } else {
        gpio_set_level(GPIO_RELAIS_PHARES_ARRIERE, etat ? 1 : 0);
        ESP_LOGI(TAG, "Phares arrière → %s", etat ? "ON" : "OFF");
    }
    
    // Publier état
    mqtt_publier_etat_actionneur(TOPIC_ETAT_PHARES_ARRIERE, etat ? 1 : 0);
    
    // Mise à jour HTTP
#ifdef HAVE_HTTP_SERVER
    http_status_update_phares_arriere(etat);
#endif
    
    return ESP_OK;
}

/**
 * @brief Toggle phares arrière
 */
esp_err_t actionneur_phares_arriere_toggle(void) {
    return actionneur_phares_arriere_set(!g_phares_arriere);
}

// ============================================================================
// SÉCURITÉ GLOBALE
// ============================================================================

/**
 * @brief Arrête toutes les vannes (sécurité)
 */
void actionneurs_arreter_toutes_vannes(void) {
    ESP_LOGW(TAG, "ARRÊT D'URGENCE TOUTES VANNES");
    vanne_arreter(&g_vanne_2m);
    vanne_arreter(&g_vanne_bout);
}

/**
 * @brief Vérifie si vanne en timeout
 */
bool actionneur_vanne_est_en_timeout(const char* vanne) {
    if (vanne == NULL) return false;
    
    if (strcmp(vanne, "vanne_2m") == 0) {
        return g_vanne_2m.etat == ETAT_VANNE_3FILS_TIMEOUT;
    } else if (strcmp(vanne, "vanne_bout_rampe") == 0) {
        return g_vanne_bout.etat == ETAT_VANNE_3FILS_TIMEOUT;
    }
    
    return false;
}

/**
 * @brief Tâche de surveillance timeouts vannes
 * Appelée périodiquement par actionneurs_manager.cpp
 */
void actionneurs_surveiller_timeouts(void) {
    vanne_verifier_timeout(&g_vanne_2m);
    vanne_verifier_timeout(&g_vanne_bout);
}

/**
 * @brief Configure timeout vanne
 */
esp_err_t actionneur_set_timeout_vanne(const char* vanne, uint32_t timeout_ms) {
    if (vanne == NULL) return ESP_ERR_INVALID_ARG;
    
    if (strcmp(vanne, "vanne_2m") == 0) {
        g_vanne_2m.timeout_ms = timeout_ms;
        ESP_LOGI(TAG, "Timeout vanne 2m: %lu ms", timeout_ms);
    } else if (strcmp(vanne, "vanne_bout_rampe") == 0) {
        g_vanne_bout.timeout_ms = timeout_ms;
        ESP_LOGI(TAG, "Timeout vanne bout: %lu ms", timeout_ms);
    } else {
        return ESP_ERR_NOT_FOUND;
    }
    
    return ESP_OK;
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
#endif // BOARD_TYPE_ARRIERE