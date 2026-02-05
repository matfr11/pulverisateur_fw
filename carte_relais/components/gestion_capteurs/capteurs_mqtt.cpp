/**
 * @file capteurs_mqtt.cpp
 * @brief Publication MQTT des données capteurs
 * @version 1.0
 * @date 2026-02-02
 */

#include "capteurs.h"
#include "mqtt_manager.h"
#include "mqtt_topics.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

#ifdef BOARD_TYPE_AVANT

static const char* TAG = "CAPTEURS_MQTT";

// ============================================================================
// PUBLICATION DÉBITMÈTRE
// ============================================================================

/**
 * @brief Publie les données débitmètre via MQTT
 * 
 * Appelé périodiquement (1 Hz) par la tâche capteurs
 */
void capteurs_publier_debitmetre(float debit, float volume) {
    // Utiliser la fonction mqtt_publier_debitmetre() de gestion_mqtt
    mqtt_publier_debitmetre(debit, volume);
    
    ESP_LOGD(TAG, "Débitmètre publié: %.2f L/min, %.2f L", debit, volume);
}

/**
 * @brief Publie les données complètes débitmètre (détaillé)
 * 
 * Utilisé pour diagnostics ou interface avancée
 */
void capteurs_publier_debitmetre_detaille(void) {
    donnees_debitmetre_t donnees;
    
    if (debitmetre_get_donnees(&donnees) != ESP_OK) {
        ESP_LOGW(TAG, "Erreur lecture débitmètre");
        return;
    }
    
    // Construire JSON détaillé
    extern esp_err_t mqtt_publish(const char*, const char*, int, bool);
    //extern const char TOPIC_CAPTEUR_DEBITMETRE[];
    
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"debit_lpm\":%.2f,\"volume_l\":%.2f,\"impulsions\":%lu,"
             "\"facteur_k\":%.2f,\"timestamp_ms\":%lu}",
             donnees.debit_instantane_lpm,
             donnees.volume_total_litres,
             donnees.impulsions_totales,
             debitmetre_get_facteur_k(),
             donnees.timestamp_derniere_impulsion_ms);
    
    mqtt_publish(TOPIC_CAPTEUR_DEBITMETRE, payload, MQTT_QOS_ETAT, false);
    
    ESP_LOGD(TAG, "Débitmètre détaillé publié");
}

// ============================================================================
// PUBLICATION SONDE NIVEAU (FUTURE)
// ============================================================================

/**
 * @brief Publie les données sonde niveau
 */
void capteurs_publier_niveau(void) {
    if (!sonde_niveau_est_disponible()) {
        return;
    }
    
    donnees_niveau_t donnees;
    if (sonde_niveau_get_donnees(&donnees) != ESP_OK) {
        return;
    }
    
    extern esp_err_t mqtt_publish(const char*, const char*, int, bool);
    //extern const char TOPIC_CAPTEUR_NIVEAU_ARRIERE[];
    
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"niveau_l\":%.1f,\"disponible\":%s,\"timestamp_ms\":%lu}",
             donnees.niveau_litres,
             donnees.disponible ? "true" : "false",
             donnees.timestamp_ms);
    
    mqtt_publish(TOPIC_CAPTEUR_NIVEAU_ARRIERE, payload, MQTT_QOS_ETAT, false);
    
    ESP_LOGD(TAG, "Niveau publié: %.1f L", donnees.niveau_litres);
}

// ============================================================================
// PUBLICATION ÉVÉNEMENTS
// ============================================================================

/**
 * @brief Publie un événement de reset volume
 */
void capteurs_publier_event_reset_volume(void) {
    extern esp_err_t mqtt_publish(const char*, const char*, int, bool);
    
    char payload[64];
    snprintf(payload, sizeof(payload),
             "{\"event\":\"reset_volume\",\"timestamp_ms\":%llu}",
             esp_timer_get_time() / 1000);
    
    mqtt_publish("pulverisateur/capteurs/events", payload, 1, false);
    
    ESP_LOGI(TAG, "Événement reset volume publié");
}

/**
 * @brief Publie fin de calibration
 */
void capteurs_publier_event_calibration(float ancien_k, float nouveau_k) {
    extern esp_err_t mqtt_publish(const char*, const char*, int, bool);
    
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"event\":\"calibration\",\"ancien_k\":%.2f,\"nouveau_k\":%.2f,"
             "\"difference_pct\":%.1f}",
             ancien_k, nouveau_k,
             ((nouveau_k - ancien_k) / ancien_k) * 100.0f);
    
    mqtt_publish("pulverisateur/capteurs/events", payload, 1, false);
    
    ESP_LOGI(TAG, "Événement calibration publié");
}

#endif // BOARD_TYPE_AVANT
