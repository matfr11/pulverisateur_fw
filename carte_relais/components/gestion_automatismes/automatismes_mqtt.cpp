/**
 * @file automatismes_mqtt.cpp
 * @brief Callbacks et publications MQTT pour automatismes
 * @version 1.0
 * @date 2026-02-02
 */

#include "automatismes.h"
#include "mqtt_manager.h"
#include "mqtt_topics.h"
#include "board_config.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include "esp_timer.h"

#ifdef BOARD_TYPE_AVANT

static const char* TAG = "AUTO_MQTT";

// ============================================================================
// CALLBACKS MQTT - Surcharge weak de gestion_mqtt
// ============================================================================

/**
 * @brief Callback: Commande automatisme reçue via MQTT
 */
void mqtt_callback_automatisme(const char* automatisme, const char* action, const char* parametres) {
    ESP_LOGI(TAG, "Commande automatisme: %s → %s", automatisme, action);
    
    // ========================================================================
    // TRANSFERT
    // ========================================================================
    
    if (strcmp(automatisme, "transfert") == 0) {
        if (strcmp(action, "ACTIVER") == 0) {
            // Parser paramètres JSON
            float volume_cible = 100.0f; // Défaut
            mode_transfert_t mode = MODE_TRANSFERT_SANS_SONDE;
            
            if (parametres != NULL) {
                cJSON* json = cJSON_Parse(parametres);
                if (json != NULL) {
                    cJSON* vol = cJSON_GetObjectItem(json, "volume");
                    if (vol != NULL && cJSON_IsNumber(vol)) {
                        volume_cible = vol->valuedouble;
                    }
                    
                    cJSON* mode_json = cJSON_GetObjectItem(json, "mode");
                    if (mode_json != NULL && cJSON_IsString(mode_json)) {
                        if (strcmp(mode_json->valuestring, "AVEC_SONDE") == 0) {
                            mode = MODE_TRANSFERT_AVEC_SONDE;
                        }
                    }
                    
                    cJSON_Delete(json);
                }
            }
            
            // Activer transfert
            esp_err_t ret = transfert_activer(mode, volume_cible);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Erreur activation transfert");
            }
        } 
        else if (strcmp(action, "DESACTIVER") == 0) {
            transfert_desactiver();
        }
    }
    
    // ========================================================================
    // BRASSAGE
    // ========================================================================
    
    else if (strcmp(automatisme, "brassage") == 0) {
        if (strcmp(action, "ACTIVER") == 0) {
            // Parser paramètres JSON (optionnels, sinon config par défaut)
            uint32_t temps_marche_sec = 300; // 5 min défaut
            uint32_t temps_pause_sec = 300;  // 5 min défaut
            
            if (parametres != NULL) {
                cJSON* json = cJSON_Parse(parametres);
                if (json != NULL) {
                    cJSON* marche = cJSON_GetObjectItem(json, "temps_marche_sec");
                    if (marche != NULL && cJSON_IsNumber(marche)) {
                        temps_marche_sec = marche->valueint;
                    }
                    
                    cJSON* pause = cJSON_GetObjectItem(json, "temps_pause_sec");
                    if (pause != NULL && cJSON_IsNumber(pause)) {
                        temps_pause_sec = pause->valueint;
                    }
                    
                    cJSON_Delete(json);
                }
            }
            
            // Activer brassage
            esp_err_t ret = brassage_activer(temps_marche_sec, temps_pause_sec);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Erreur activation brassage");
            }
        }
        else if (strcmp(action, "DESACTIVER") == 0) {
            brassage_desactiver();
        }
    }
    
    else {
        ESP_LOGW(TAG, "Automatisme inconnu: %s", automatisme);
    }
}

// ============================================================================
// PUBLICATIONS MQTT
// ============================================================================

/**
 * @brief Publie l'état transfert
 */
void automatismes_publier_etat_transfert(void) {
    etat_transfert_t etat = transfert_get_etat();
    float volume_cycle = transfert_get_volume_cycle();
    float pourcentage = transfert_get_pourcentage();
    
    // Construire JSON
    cJSON* json = cJSON_CreateObject();
    
    // État
    const char* etat_str = "INACTIF";
    switch (etat) {
        case ETAT_TRANSFERT_EN_COURS: etat_str = "EN_COURS"; break;
        case ETAT_TRANSFERT_TERMINE: etat_str = "TERMINE"; break;
        case ETAT_TRANSFERT_ERREUR: etat_str = "ERREUR"; break;
        default: break;
    }
    cJSON_AddStringToObject(json, "etat", etat_str);
    
    // Données
    cJSON_AddNumberToObject(json, "volume_cycle_l", volume_cycle);
    cJSON_AddNumberToObject(json, "pourcentage", pourcentage);
    cJSON_AddNumberToObject(json, "timestamp_ms", esp_timer_get_time() / 1000);
    
    char* payload = cJSON_PrintUnformatted(json);
    
    // Publier avec retain
    extern esp_err_t mqtt_publish(const char*, const char*, int, bool);
    mqtt_publish(TOPIC_ETAT_TRANSFERT, payload, MQTT_QOS_ETAT, true);
    
    cJSON_Delete(json);
    free(payload);
    
    ESP_LOGD(TAG, "État transfert publié: %s, %.1f L, %.0f%%", 
             etat_str, volume_cycle, pourcentage);
}

/**
 * @brief Publie l'état brassage
 */
void automatismes_publier_etat_brassage(void) {
    etat_brassage_t etat = brassage_get_etat();
    uint32_t temps_restant = brassage_get_temps_restant();
    float pourcentage = brassage_get_pourcentage();
    
    // Construire JSON
    cJSON* json = cJSON_CreateObject();
    
    // État
    const char* etat_str = "INACTIF";
    switch (etat) {
        case ETAT_BRASSAGE_MARCHE: etat_str = "MARCHE"; break;
        case ETAT_BRASSAGE_PAUSE: etat_str = "PAUSE"; break;
        case ETAT_BRASSAGE_SUSPENDU: etat_str = "SUSPENDU"; break;
        default: break;
    }
    cJSON_AddStringToObject(json, "etat", etat_str);
    
    // Données
    cJSON_AddNumberToObject(json, "temps_restant_sec", temps_restant);
    cJSON_AddNumberToObject(json, "temps_restant_min", temps_restant / 60);
    cJSON_AddNumberToObject(json, "pourcentage", pourcentage);
    cJSON_AddNumberToObject(json, "timestamp_ms", esp_timer_get_time() / 1000);
    
    char* payload = cJSON_PrintUnformatted(json);
    
    // Publier avec retain
    extern esp_err_t mqtt_publish(const char*, const char*, int, bool);
    mqtt_publish(TOPIC_ETAT_BRASSAGE, payload, MQTT_QOS_ETAT, true);
    
    cJSON_Delete(json);
    free(payload);
    
    ESP_LOGD(TAG, "État brassage publié: %s, %lu sec, %.0f%%", 
             etat_str, temps_restant, pourcentage);
}

#endif // BOARD_TYPE_AVANT
