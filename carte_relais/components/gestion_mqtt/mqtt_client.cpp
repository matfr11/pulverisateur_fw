/**
 * @file mqtt_client.cpp
 * @brief Client MQTT - Connexion et gestion événements
 * @version 1.0
 * @date 2026-02-02
 */

#include "mqtt_manager.h"
#include "mqtt_topics.h"
#include "board_config.h"
#include "app_role.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <string.h>
#include "esp_random.h"

static const char* TAG = LOG_TAG_MQTT;
extern esp_err_t mqtt_broker_start(void);

// Handle du client MQTT
esp_mqtt_client_handle_t g_mqtt_client = NULL;

// État de connexion
static bool g_mqtt_connecte = false;

// Compteurs statistiques
static uint32_t g_messages_publies = 0;
static uint32_t g_messages_recus = 0;
static uint32_t g_erreurs_connexion = 0;

// ============================================================================
// DÉCLARATIONS FORWARD
// ============================================================================

static void mqtt_event_handler(void* handler_args, esp_event_base_t base, 
                               int32_t event_id, void* event_data);
static void mqtt_traiter_message(const char* topic, const char* payload, int payload_len);

// ============================================================================
// INITIALISATION ET CONNEXION
// ============================================================================

/**
 * @brief Initialise le client MQTT
 */
esp_err_t app_init_mqtt(role_carte_t role) {
    ESP_LOGI(TAG, "Initialisation client MQTT (rôle: %s)", 
             role == ROLE_MASTER ? "MASTER" : "SLAVE");
    
    // Configuration du client MQTT
    esp_mqtt_client_config_t mqtt_cfg = {};
    
    // Broker address
    if (role == ROLE_MASTER) {
        // --- AJOUT CRUCIAL ICI ---
        ESP_LOGI(TAG, "Démarrage du broker MQTT embarqué...");
        if (mqtt_broker_start() != ESP_OK) {
            ESP_LOGE(TAG, "Échec du démarrage du broker !");
            return ESP_FAIL;
        }
        // On laisse 200ms au serveur pour ouvrir son socket 1883
        vTaskDelay(pdMS_TO_TICKS(200)); 
        // -------------------------

        // Maintenant le client peut se connecter à lui-même
        mqtt_cfg.broker.address.uri = "mqtt://192.168.4.1:1883";
    } else {
        mqtt_cfg.broker.address.uri = "mqtt://" MQTT_BROKER_IP_MASTER ":1883";
    }
    
    // Client ID unique basé sur le type de carte
    static char client_id[32];
    snprintf(client_id, sizeof(client_id), "pulve_%s_%02X%02X", 
             IDENTIFIANT_CARTE,
             static_cast<uint8_t>(esp_random() >> 8),
             (uint8_t)esp_random());
    mqtt_cfg.credentials.client_id = client_id;
    
    // Paramètres de connexion
    mqtt_cfg.session.keepalive = MQTT_KEEPALIVE_SEC;
    mqtt_cfg.network.reconnect_timeout_ms = MQTT_RECONNECT_DELAY_MS;
    mqtt_cfg.network.timeout_ms = 5000;
    
    // Buffer sizes
    mqtt_cfg.buffer.size = MQTT_BUFFER_SIZE;
    mqtt_cfg.buffer.out_size = MQTT_BUFFER_SIZE;
    
    // Last Will (testament) - pour détecter déconnexions brutales
    static char will_topic[64];
    if (strcmp(IDENTIFIANT_CARTE, "AVANT") == 0) {
        strncpy(will_topic, TOPIC_PRESENCE_AVANT, sizeof(will_topic));
    } else if (strcmp(IDENTIFIANT_CARTE, "ARRIERE") == 0) {
        strncpy(will_topic, TOPIC_PRESENCE_ARRIERE, sizeof(will_topic));
    } else {
        strncpy(will_topic, TOPIC_PRESENCE_ECRAN, sizeof(will_topic));
    }
    // 1. D'abord on déclare le message (indispensable en premier)
    const char* will_msg = "{\"online\":false}";
    
    // 2. Ensuite on configure le Last Will
    mqtt_cfg.session.last_will.topic = will_topic;
    mqtt_cfg.session.last_will.msg = will_msg;            // On utilise la variable
    mqtt_cfg.session.last_will.msg_len = strlen(will_msg); // OK : will_msg est connu !
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = 1;

    
    // Créer le client
    g_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (g_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Erreur initialisation client MQTT");
        return ESP_FAIL;
    }
    
    // Enregistrer le handler d'événements
    esp_err_t ret = esp_mqtt_client_register_event(g_mqtt_client, 
                                                    static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), 
                                                    mqtt_event_handler, 
                                                    NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur enregistrement handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Démarrer le client
    ret = esp_mqtt_client_start(g_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur démarrage client: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Client MQTT initialisé (ID: %s)", client_id);
    return ESP_OK;
}

/**
 * @brief Démarre le client MQTT
 */
esp_err_t mqtt_client_start(void) {
    if (g_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Client MQTT non initialisé");
        return ESP_FAIL;
    }
    
    return esp_mqtt_client_start(g_mqtt_client);
}

/**
 * @brief Arrête le client MQTT
 */
void mqtt_client_stop(void) {
    if (g_mqtt_client != NULL) {
        ESP_LOGI(TAG, "Arrêt client MQTT");
        esp_mqtt_client_stop(g_mqtt_client);
        g_mqtt_connecte = false;
    }
}

/**
 * @brief Vérifie si MQTT est connecté
 */
bool app_mqtt_est_connecte(void) {
    return g_mqtt_connecte;
}

/**
 * @brief Tente une reconnexion MQTT
 */
void app_mqtt_reconnect(void) {

    if (app_role_get_role_actuel() == ROLE_MASTER) return;

    if (g_mqtt_client == NULL) {
        ESP_LOGW(TAG, "Client MQTT non initialisé");
        return;
    }
    
    if (g_mqtt_connecte) {
        ESP_LOGI(TAG, "Déjà connecté");
        return;
    }
    
    ESP_LOGI(TAG, "Tentative de reconnexion MQTT...");
    esp_mqtt_client_reconnect(g_mqtt_client);
}

// ============================================================================
// GESTIONNAIRE D'ÉVÉNEMENTS MQTT
// ============================================================================

/**
 * @brief Gestionnaire principal des événements MQTT
 */
static void mqtt_event_handler(void* handler_args, esp_event_base_t base,
                              int32_t event_id, void* event_data) {
    
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        
        // ====================================================================
        // CONNEXION
        // ====================================================================
        
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "========================================");
            ESP_LOGI(TAG, "  MQTT CONNECTÉ");
            ESP_LOGI(TAG, "========================================");
            g_mqtt_connecte = true;
            g_erreurs_connexion = 0;
            
            // Souscrire aux topics nécessaires
            mqtt_souscrire_topics();
            
            // Publier présence immédiatement
            app_mqtt_publier_presence(true);
            
            // Demander configuration si SLAVE
            if (app_role_get_role_actuel() == ROLE_SLAVE) {
                app_mqtt_demander_configuration();
            }
            
            break;
            
        // ====================================================================
        // DÉCONNEXION
        // ====================================================================
        
            case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT DÉCONNECTÉ");
            g_mqtt_connecte = false;
            g_erreurs_connexion++;
            
            // --- AJOUT DE SÉCURITÉ ---
            if (app_role_get_role_actuel() == ROLE_MASTER) {
                ESP_LOGW(TAG, "Mode MASTER actif : On ignore la reconnexion auto du client.");
                return; // On sort pour éviter de polluer le CPU et les logs
            }
            // -------------------------
            
            // Notifier app_role si SLAVE
            if (app_role_get_role_actuel() == ROLE_SLAVE) {
                extern void app_role_on_mqtt_disconnected(void);
                app_role_on_mqtt_disconnected();
            }
            break;
            
        // ====================================================================
        // SOUSCRIPTION
        // ====================================================================
        
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Souscription OK (msg_id=%d)", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "Désinscription OK (msg_id=%d)", event->msg_id);
            break;
            
        // ====================================================================
        // PUBLICATION
        // ====================================================================
        
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "Message publié (msg_id=%d)", event->msg_id);
            g_messages_publies++;
            break;
            
        // ====================================================================
        // RÉCEPTION MESSAGE
        // ====================================================================
        
        case MQTT_EVENT_DATA: {
            g_messages_recus++;
            
            // Créer des copies pour traitement
            char topic[MQTT_MAX_TOPIC_LEN];
            char payload[MQTT_MAX_PAYLOAD_LEN];
            
            // Copier topic (peut ne pas être null-terminated)
            int topic_len = event->topic_len > (MQTT_MAX_TOPIC_LEN - 1) ? 
                           (MQTT_MAX_TOPIC_LEN - 1) : event->topic_len;
            memcpy(topic, event->topic, topic_len);
            topic[topic_len] = '\0';
            
            // Copier payload (peut être fragmenté)
            int payload_len = event->data_len > (MQTT_MAX_PAYLOAD_LEN - 1) ? 
                             (MQTT_MAX_PAYLOAD_LEN - 1) : event->data_len;
            memcpy(payload, event->data, payload_len);
            payload[payload_len] = '\0';
            
            ESP_LOGD(TAG, "Message reçu [%s]: %s", topic, payload);
            
            // Traiter le message
            mqtt_traiter_message(topic, payload, payload_len);
            
            break;
        }
            
        // ====================================================================
        // ERREURS
        // ====================================================================
        
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Erreur MQTT");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "  Transport error: 0x%x", event->error_handle->esp_transport_sock_errno);
            }
            break;
            
        default:
            ESP_LOGD(TAG, "Événement MQTT non géré: %ld", event_id);
            break;
    }
}

// ============================================================================
// TRAITEMENT DES MESSAGES REÇUS
// ============================================================================

/**
 * @brief Traite un message MQTT reçu
 */
static void mqtt_traiter_message(const char* topic, const char* payload, int payload_len) {
    // Cette fonction dispatche vers les handlers appropriés
    // Implémenté dans mqtt_handlers.cpp
    extern void mqtt_dispatcher_message(const char* topic, const char* payload, int payload_len);
    mqtt_dispatcher_message(topic, payload, payload_len);
}

/**
 * @brief Retourne les statistiques MQTT
 */
void mqtt_get_stats(uint32_t* publies, uint32_t* recus, uint32_t* erreurs) {
    if (publies) *publies = g_messages_publies;
    if (recus) *recus = g_messages_recus;
    if (erreurs) *erreurs = g_erreurs_connexion;
}
