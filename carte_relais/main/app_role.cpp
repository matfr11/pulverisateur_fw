/**
 * @file app_role.cpp
 * @brief Implémentation de la gestion Master/Slave
 * 
 * Logique de détection:
 * 1. Au démarrage, scan WiFi pendant 3 secondes
 * 2. Si SSID "PulveriAG" détecté → SLAVE
 * 3. Sinon → MASTER (première carte du système)
 * 
 * Logique de failover:
 * - SLAVE surveille présence master via heartbeat MQTT
 * - Si perte > 15 secondes → promotion en MASTER
 * - Ancien master qui revient reste SLAVE (pas de conflit)
 * 
 * @version 1.0
 * @date 2026-02-02
 */
#include <stdio.h>
#include <string.h>
#include "app_role.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "nvs.h"

//#include "mqtt_manager.h"
#include "app_init.h"

#include "wifi_manager.h"  // <--- Indispensable pour wifi_start_ap, wifi_reconnect, etc.
#include "mqtt_manager.h"  // <--- Si vous l'utilisez aussi ici

// ------------------------

// ============================================================================
// CONSTANTES ET VARIABLES PRIVÉES
// ============================================================================

static const char* TAG = "ROLE";

// État du rôle actuel
static role_carte_t g_role_actuel = ROLE_INCONNU;

// Timestamp dernière réception heartbeat master (pour SLAVE uniquement)
static uint32_t g_dernier_heartbeat_master_ms = 0;

// Flag pour indiquer transition en cours
static bool g_transition_en_cours = false;

// Event group pour synchronisation
static EventGroupHandle_t g_event_group_role = NULL;
#define BIT_MASTER_DETECTE  (1 << 0)
#define BIT_PROMOTION_EN_COURS (1 << 1)

// ============================================================================
// DÉCLARATIONS FORWARD
// ============================================================================

static bool scanner_reseau_wifi(void);
static esp_err_t charger_role_preferentiel(role_carte_t* role);
static esp_err_t sauvegarder_role(role_carte_t role);
static void tache_surveillance_master(void* pvParameters);

// ============================================================================
// FONCTIONS PUBLIQUES
// ============================================================================

/**
 * @brief Détermine le rôle de la carte au démarrage
 */
role_carte_t app_role_determiner_role(void) {
    ESP_LOGI(TAG, "Détermination du rôle...");
    
    // 1. Initialisation de l'Event Group
    if (g_event_group_role == NULL) {
        g_event_group_role = xEventGroupCreate();
    }

    // 2. SCAN WIFI : On regarde d'abord ce qui se passe dehors
    ESP_LOGI(TAG, "Étape 1: Scan WiFi pour détecter un master...");
    bool master_detecte = scanner_reseau_wifi();
    
    if (master_detecte) {
        // Règle absolue : Si un Master existe déjà, je me tais et je deviens SLAVE
        ESP_LOGW(TAG, "-> Master trouvé ! Je me configure en SLAVE.");
        g_role_actuel = ROLE_SLAVE;
        // On ne sauvegarde PAS en NVS ici, pour ne pas écraser son identité d'origine
        return ROLE_SLAVE;
    }

    // 3. SI AUCUN MASTER N'EST TROUVÉ : On regarde qui on est censé être (NVS)
    role_carte_t role_pref;
    if (charger_role_preferentiel(&role_pref) == ESP_OK) {
        ESP_LOGI(TAG, "Étape 2: Aucun master trouvé. Rôle NVS: %s", 
                 role_pref == ROLE_MASTER ? "MASTER" : "SLAVE");
        
        g_role_actuel = role_pref;
        return role_pref;
    }

    // 4. PAR DÉFAUT (si NVS vide) : On devient Master pour ne pas bloquer le système
    ESP_LOGW(TAG, "Étape 3: Ni master détecté, ni config NVS. Par défaut: MASTER");
    g_role_actuel = ROLE_MASTER;
    return ROLE_MASTER;
}
/**
 * @brief Démarre la surveillance du master (pour SLAVE uniquement)
 */
void app_role_surveiller_master(void) {
    if (g_role_actuel != ROLE_SLAVE) {
        ESP_LOGW(TAG, "Surveillance master demandée mais carte est MASTER");
        return;
    }
    
    ESP_LOGI(TAG, "Démarrage surveillance du master");
    
    // Créer tâche de surveillance
    BaseType_t ret = xTaskCreate(
        tache_surveillance_master,
        "surveillance_master",
        3072,
        NULL,
        2,
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Erreur création tâche surveillance");
    }
}

/**
 * @brief Force le passage en mode Master
 */
esp_err_t app_role_devenir_master(void) {
    if (g_role_actuel == ROLE_MASTER) {
        ESP_LOGW(TAG, "Déjà MASTER");
        return ESP_OK;
    }
    
    if (g_transition_en_cours) {
        ESP_LOGW(TAG, "Transition déjà en cours");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, "  PROMOTION: SLAVE → MASTER");
    ESP_LOGW(TAG, "========================================");
    
    g_transition_en_cours = true;
    xEventGroupSetBits(g_event_group_role, BIT_PROMOTION_EN_COURS);
    
    // Étape 1: Déconnecter du WiFi master actuel
    ESP_LOGI(TAG, "1. Déconnexion WiFi...");
    esp_wifi_disconnect();
    esp_wifi_stop(); // On stoppe proprement le driver station
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Étape 2: Arrêter le client MQTT (Crucial pour vider les buffers d'erreur)
    ESP_LOGI(TAG, "2. Arrêt client MQTT...");
    extern void mqtt_client_stop(void);
    mqtt_client_stop(); 
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Étape 3: Changer de rôle en mémoire
    g_role_actuel = ROLE_MASTER;
    sauvegarder_role(ROLE_MASTER);
    
    // Étape 4: Démarrage mode MASTER (WiFi AP)
    ESP_LOGI(TAG, "3. Démarrage mode MASTER (WiFi AP)...");
    extern esp_err_t wifi_start_ap(void);
    esp_err_t ret = wifi_start_ap();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur démarrage WiFi AP");
        g_transition_en_cours = false;
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Étape 5: Démarrer le broker et RÉ-INITIALISER le client
    ESP_LOGI(TAG, "4. Démarrage Broker et Client local...");
    extern esp_err_t app_init_mqtt(role_carte_t role);
    
    // app_init_mqtt va appeler mqtt_broker_start() et configurer l'IP 192.168.4.1
    ret = app_init_mqtt(ROLE_MASTER); 
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur réinitialisation MQTT mode MASTER");
    }
    
    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, "  PROMOTION TERMINÉE - MASTER ACTIF");
    ESP_LOGW(TAG, "========================================");
    
    g_transition_en_cours = false;
    xEventGroupClearBits(g_event_group_role, BIT_PROMOTION_EN_COURS);
    
    return ESP_OK;
}

/**
 * @brief Retourne le rôle actuel
 */
role_carte_t app_role_get_role_actuel(void) {
    return g_role_actuel;
}

/**
 * @brief Callback: WiFi déconnecté (SLAVE uniquement)
 */
void app_role_on_wifi_disconnected(void) {
    if (g_role_actuel != ROLE_SLAVE) return;

    ESP_LOGW(TAG, "Lien WiFi perdu.");

    // On ne force pas la promotion ici. 
    // On laisse la tâche de surveillance (ci-dessus) faire ses 3 scans.
    // Mais on peut tenter une reconnexion immédiate au cas où c'est juste un glitch.
    esp_wifi_connect(); 
}

/**
 * @brief Callback: MQTT déconnecté (SLAVE uniquement)
 */
void app_role_on_mqtt_disconnected(void) {
    if (g_role_actuel != ROLE_SLAVE) {
        return;
    }
    
    ESP_LOGW(TAG, "MQTT déconnecté - vérification présence master");
    g_dernier_heartbeat_master_ms = 0;
}

/**
 * @brief Met à jour le timestamp du dernier heartbeat master reçu
 * Appelé par le gestionnaire MQTT quand un heartbeat master est reçu
 */
void app_role_heartbeat_master_recu(void) {
    if (g_role_actuel != ROLE_SLAVE) {
        return;
    }
    
    g_dernier_heartbeat_master_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
}

// ============================================================================
// FONCTIONS PRIVÉES
// ============================================================================

/**
 * @brief Scanne le réseau WiFi pour détecter le SSID du master
 * @return true si master détecté, false sinon
 */
static bool scanner_reseau_wifi(void) {
    esp_err_t ret;
    
    // Initialiser WiFi en mode station pour le scan
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init WiFi pour scan: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur set mode STA: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        return false;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur start WiFi: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        return false;
    }
    
    // Configuration du scan
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300
            }
        }
    };
    
    ESP_LOGI(TAG, "Scan en cours (timeout %d ms)...", WIFI_TIMEOUT_DETECTION_MS);
    
    ret = esp_wifi_scan_start(&scan_config, true); // Bloquant
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur scan: %s", esp_err_to_name(ret));
        esp_wifi_stop();
        esp_wifi_deinit();
        return false;
    }
    
    // Récupérer les résultats
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) {
        ESP_LOGI(TAG, "Aucun réseau WiFi détecté");
        esp_wifi_stop();
        esp_wifi_deinit();
        return false;
    }
    
    ESP_LOGI(TAG, "%d réseau(x) trouvé(s)", ap_count);
    
    wifi_ap_record_t* ap_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_records == NULL) {
        ESP_LOGE(TAG, "Erreur allocation mémoire");
        esp_wifi_stop();
        esp_wifi_deinit();
        return false;
    }
    
    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur récupération résultats: %s", esp_err_to_name(ret));
        free(ap_records);
        esp_wifi_stop();
        esp_wifi_deinit();
        return false;
    }
    
    // Chercher notre SSID
    bool master_trouve = false;
    for (int i = 0; i < ap_count; i++) {
        ESP_LOGD(TAG, "  [%d] SSID: %s, RSSI: %d", i, ap_records[i].ssid, ap_records[i].rssi);
        
        if (strcmp((char*)ap_records[i].ssid, WIFI_AP_SSID) == 0) {
            ESP_LOGI(TAG, "✓ Master détecté! SSID: %s, RSSI: %d dBm", 
                     ap_records[i].ssid, ap_records[i].rssi);
            master_trouve = true;
            break;
        }
    }
    
    free(ap_records);
    esp_wifi_stop();
    esp_wifi_deinit();
    
    return master_trouve;
}

/**
 * @brief Charge le rôle préférentiel depuis NVS
 */
static esp_err_t charger_role_preferentiel(role_carte_t* role) {
    if (role == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_SYSTEME, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint8_t role_value;
    ret = nvs_get_u8(nvs_handle, NVS_KEY_ROLE_PREFERENTIEL, &role_value);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        *role = (role_carte_t)role_value;
    }
    
    return ret;
}

/**
 * @brief Sauvegarde le rôle dans NVS
 */
static esp_err_t sauvegarder_role(role_carte_t role) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_SYSTEME, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur ouverture NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_set_u8(nvs_handle, NVS_KEY_ROLE_PREFERENTIEL, (uint8_t)role);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Rôle sauvegardé: %s", role == ROLE_MASTER ? "MASTER" : "SLAVE");
    }
    
    return ret;
}

/**
 * @brief Tâche de surveillance du master (pour SLAVE uniquement)
 * 
 * Surveille la réception des heartbeats master
 * Si aucun heartbeat pendant 15 secondes → promotion en MASTER
 */
static void tache_surveillance_master(void* pvParameters) {
    (void)pvParameters;
    
    const uint32_t TIMEOUT_HEARTBEAT_MS = 15000;
    const uint32_t PERIODE_VERIFICATION_MS = 2000;
    
    ESP_LOGI(TAG, "Tâche surveillance master démarrée");
    g_dernier_heartbeat_master_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(PERIODE_VERIFICATION_MS));
        
        if (g_role_actuel != ROLE_SLAVE) break;
        
        uint32_t maintenant_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t temps_ecoule_ms = (g_dernier_heartbeat_master_ms == 0) ? 
                                   (TIMEOUT_HEARTBEAT_MS + 1) : 
                                   (maintenant_ms - g_dernier_heartbeat_master_ms);
        
        if (temps_ecoule_ms > TIMEOUT_HEARTBEAT_MS) {
            ESP_LOGW(TAG, "MASTER SILENCIEUX (%lu ms) - Vérification réseau...", temps_ecoule_ms);
            
            // ✅ AJOUT : Vérifier AVANT de scanner si WiFi encore connecté
            extern bool app_wifi_est_connecte(void);
            if (app_wifi_est_connecte()) {
                ESP_LOGI(TAG, "WiFi toujours connecté - probablement MQTT temporairement bloqué");
                
                // Tenter de réparer MQTT sans scanner WiFi
                extern void app_mqtt_reconnect(void);
                app_mqtt_reconnect();
                
                // Repousser le timeout
                g_dernier_heartbeat_master_ms = maintenant_ms - (TIMEOUT_HEARTBEAT_MS / 2);
                continue; // ← IMPORTANT : Skip le scan
            }
            
            // --- SI WIFI DÉCONNECTÉ, ALORS SCANNER ---
            extern bool scanner_reseau_wifi(void); 
            bool master_physiquement_present = scanner_reseau_wifi();
            if (master_physiquement_present) {
                ESP_LOGI(TAG, "Master détecté par scan WiFi. Reconnexion...");
                
                extern esp_err_t wifi_reconnect(void);
                wifi_reconnect();
                
                // Repousser le timeout
                g_dernier_heartbeat_master_ms = maintenant_ms - (TIMEOUT_HEARTBEAT_MS / 2);
                continue;
            } 
            
            // --- SI LE SCAN NE TROUVE RIEN ---
            ESP_LOGE(TAG, "Master introuvable au scan WiFi après %lu ms de silence.", temps_ecoule_ms);
            ESP_LOGE(TAG, "Promotion en MASTER imminente...");
            
            esp_err_t ret = app_role_devenir_master();
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Promotion réussie - tâche terminée");
                break;
            } else {
                ESP_LOGE(TAG, "Échec promotion - nouvelle tentative dans 5 sec");
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
    }
    
    ESP_LOGI(TAG, "Tâche surveillance terminée");
    vTaskDelete(NULL);
}