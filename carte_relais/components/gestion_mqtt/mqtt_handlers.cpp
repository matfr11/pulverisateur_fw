/**
 * @file mqtt_handlers.cpp
 * @brief Traitement des messages MQTT reçus
 * @version 1.0
 * @date 2026-02-02
 */

#include "mqtt_manager.h"
#include "mqtt_topics.h"
#include "board_config.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include "mqtt_client.h"

#ifdef HAVE_HTTP_SERVER
#include "http_server.h"
#endif

static const char* TAG = LOG_TAG_MQTT;

// ============================================================================
// CALLBACKS (implémentés dans gestion_actionneurs)
// ============================================================================

extern "C" {
    void mqtt_callback_commande_avant(const char* actionneur, const char* commande);
    void mqtt_callback_commande_arriere(const char* actionneur, const char* commande);
    void mqtt_callback_configuration(const char* payload);
    void mqtt_callback_automatisme(const char* automatisme, const char* commande, const char* payload);
}

// ============================================================================
// SOUSCRIPTION AUX TOPICS
// ============================================================================

/**
 * @brief Souscrit aux topics nécessaires selon le type de carte
 */
esp_err_t mqtt_souscrire_topics(void) {
    extern esp_mqtt_client_handle_t g_mqtt_client;
    
    if (g_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Client MQTT non initialisé");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Souscription aux topics...");
    
    // ========================================================================
    // TOPICS COMMUNS (toutes les cartes)
    // ========================================================================
    
    // Configuration
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_CONFIG_INSTANTANE, MQTT_QOS_CONFIG);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_CONFIG_MISE_A_JOUR, MQTT_QOS_CONFIG);
    
    // Système
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_SYSTEME_VERSION, MQTT_QOS_ETAT);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_SYSTEME_ROLE, MQTT_QOS_ETAT);

    // Présence (heartbeat) - pour surveillance du master
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_PRESENCE_AVANT, MQTT_QOS_ETAT);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_PRESENCE_ARRIERE, MQTT_QOS_ETAT);
 
    // ========================================================================
    // ÉTATS DES ACTIONNEURS - Pour synchronisation interface HTTP
    // ========================================================================
    
#ifdef HAVE_HTTP_SERVER
    // Souscrire à TOUS les états pour mise à jour HTTP locale
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_ETAT_POMPE, 0);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_ETAT_VANNE_3VOIES, 0);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_ETAT_PHARES_AVANT, 0);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_ETAT_VANNE_2M, 0);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_ETAT_VANNE_BOUT_RAMPE, 0);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_ETAT_PHARES_ARRIERE, 0);
    ESP_LOGI(TAG, "Souscription aux états actionneurs pour sync HTTP");
#endif

    // ========================================================================
    // TOPICS SPÉCIFIQUES CARTE AVANT
    // ========================================================================
    
#ifdef BOARD_TYPE_AVANT
    ESP_LOGI(TAG, "Souscription topics AVANT");
    
    // Commandes actionneurs
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_CMD_POMPE, MQTT_QOS_COMMANDE);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_CMD_VANNE_3VOIES, MQTT_QOS_COMMANDE);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_CMD_PHARES_AVANT, MQTT_QOS_COMMANDE);
    
    // Commandes automatismes
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_CMD_TRANSFERT_ACTIVER, MQTT_QOS_COMMANDE);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_CMD_TRANSFERT_DESACTIVER, MQTT_QOS_COMMANDE);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_CMD_BRASSAGE_ACTIVER, MQTT_QOS_COMMANDE);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_CMD_BRASSAGE_DESACTIVER, MQTT_QOS_COMMANDE);
#endif
    
    // ========================================================================
    // TOPICS SPÉCIFIQUES CARTE ARRIÈRE
    // ========================================================================
    
#ifdef BOARD_TYPE_ARRIERE
    ESP_LOGI(TAG, "Souscription topics ARRIERE");
    
    // Commandes actionneurs
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_CMD_VANNE_2M, MQTT_QOS_COMMANDE);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_CMD_VANNE_BOUT_RAMPE, MQTT_QOS_COMMANDE);
    esp_mqtt_client_subscribe(g_mqtt_client, TOPIC_CMD_PHARES_ARRIERE, MQTT_QOS_COMMANDE);
#endif
    
    ESP_LOGI(TAG, "Souscriptions effectuées");
    return ESP_OK;
}

// ============================================================================
// SYNCHRONISATION MQTT → HTTP
// ============================================================================

#ifdef HAVE_HTTP_SERVER
/**
 * @brief Met à jour l'interface HTTP locale avec les états reçus via MQTT
 */
static void mqtt_sync_http_status(const char* topic, const char* payload) {
    // Parser le JSON
    cJSON* json = cJSON_Parse(payload);
    if (!json) return;
    
    cJSON* etat_json = cJSON_GetObjectItem(json, "etat");
    if (!etat_json) {
        cJSON_Delete(json);
        return;
    }
    
    // Synchroniser selon le topic
    if (strcmp(topic, TOPIC_ETAT_POMPE) == 0) {
        bool etat = (etat_json->valueint == 1);
        http_status_update_pompe(etat);
        ESP_LOGD(TAG, "HTTP sync: Pompe = %d", etat);
    }
    else if (strcmp(topic, TOPIC_ETAT_VANNE_3VOIES) == 0) {
        bool etat = (etat_json->valueint == 1); // 1 = TRANSFERT/PULVERISATION
        http_status_update_vanne_3v(etat);
        ESP_LOGD(TAG, "HTTP sync: Vanne3V = %d", etat);
    }
    else if (strcmp(topic, TOPIC_ETAT_PHARES_AVANT) == 0) {
        bool etat = (etat_json->valueint == 1);
        http_status_update_phares_avant(etat);
        ESP_LOGD(TAG, "HTTP sync: Phares avant = %d", etat);
    }
    else if (strcmp(topic, TOPIC_ETAT_VANNE_2M) == 0) {
        etat_vanne_3fils_t etat = (etat_vanne_3fils_t)etat_json->valueint;
        http_status_update_vanne_2m(etat);
        ESP_LOGD(TAG, "HTTP sync: Vanne2m = %d", etat);
    }
    else if (strcmp(topic, TOPIC_ETAT_VANNE_BOUT_RAMPE) == 0) {
        etat_vanne_3fils_t etat = (etat_vanne_3fils_t)etat_json->valueint;
        http_status_update_vanne_bout(etat);
        ESP_LOGD(TAG, "HTTP sync: VanneBout = %d", etat);
    }
    else if (strcmp(topic, TOPIC_ETAT_PHARES_ARRIERE) == 0) {
        bool etat = (etat_json->valueint == 1);
        http_status_update_phares_arriere(etat);
        ESP_LOGD(TAG, "HTTP sync: Phares arrière = %d", etat);
    }
    
    cJSON_Delete(json);
}
#endif

// ============================================================================
// DISPATCHER DE MESSAGES
// ============================================================================

/**
 * @brief Dispatche les messages reçus vers les handlers appropriés
 */
void mqtt_dispatcher_message(const char* topic, const char* payload, int payload_len) {
    (void)payload_len; // Non utilisé pour l'instant
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    if (strcmp(topic, TOPIC_CONFIG_INSTANTANE) == 0 || 
        strcmp(topic, TOPIC_CONFIG_MISE_A_JOUR) == 0) {
        
        ESP_LOGI(TAG, "Configuration reçue");
        mqtt_callback_configuration(payload);
        return;
    }
    
    // ========================================================================
    // HEARTBEAT MASTER (pour surveillance SLAVE)
    // ========================================================================
    
    if (strcmp(topic, TOPIC_PRESENCE_AVANT) == 0 || 
        strcmp(topic, TOPIC_PRESENCE_ARRIERE) == 0) {
        
        // Vérifier si c'est un heartbeat du master
        cJSON* json = cJSON_Parse(payload);
        if (json != NULL) {
            cJSON* online = cJSON_GetObjectItem(json, "online");
            if (online != NULL && cJSON_IsTrue(online)) {
                // Notifier app_role
                extern void app_role_heartbeat_master_recu(void);
                app_role_heartbeat_master_recu();
            }
            cJSON_Delete(json);
        }
        return;
    }
   
    // ========================================================================
    // ÉTATS ACTIONNEURS - Synchronisation HTTP
    // ========================================================================
    
#ifdef HAVE_HTTP_SERVER
    if (strcmp(topic, TOPIC_ETAT_POMPE) == 0 ||
        strcmp(topic, TOPIC_ETAT_VANNE_3VOIES) == 0 ||
        strcmp(topic, TOPIC_ETAT_PHARES_AVANT) == 0 ||
        strcmp(topic, TOPIC_ETAT_VANNE_2M) == 0 ||
        strcmp(topic, TOPIC_ETAT_VANNE_BOUT_RAMPE) == 0 ||
        strcmp(topic, TOPIC_ETAT_PHARES_ARRIERE) == 0) {
        
        mqtt_sync_http_status(topic, payload);
        // Continuer le traitement normal (pas de return)
    }
#endif
    
    // ========================================================================
    // COMMANDES CARTE AVANT
    // ========================================================================
    
#ifdef BOARD_TYPE_AVANT
    
    if (strcmp(topic, TOPIC_CMD_POMPE) == 0) {
        mqtt_callback_commande_avant("pompe", payload);
        return;
    }
    
    if (strcmp(topic, TOPIC_CMD_VANNE_3VOIES) == 0) {
        mqtt_callback_commande_avant("vanne_3voies", payload);
        return;
    }
    
    if (strcmp(topic, TOPIC_CMD_PHARES_AVANT) == 0) {
        mqtt_callback_commande_avant("phares", payload);
        return;
    }
    
    // Automatismes
    if (strcmp(topic, TOPIC_CMD_TRANSFERT_ACTIVER) == 0) {
        mqtt_callback_automatisme("transfert", "ACTIVER", payload);
        return;
    }
    
    if (strcmp(topic, TOPIC_CMD_TRANSFERT_DESACTIVER) == 0) {
        mqtt_callback_automatisme("transfert", "DESACTIVER", NULL);
        return;
    }
    
    if (strcmp(topic, TOPIC_CMD_BRASSAGE_ACTIVER) == 0) {
        mqtt_callback_automatisme("brassage", "ACTIVER", payload);
        return;
    }
    
    if (strcmp(topic, TOPIC_CMD_BRASSAGE_DESACTIVER) == 0) {
        mqtt_callback_automatisme("brassage", "DESACTIVER", NULL);
        return;
    }
    
#endif // BOARD_TYPE_AVANT
    
    // ========================================================================
    // COMMANDES CARTE ARRIÈRE
    // ========================================================================
    
#ifdef BOARD_TYPE_ARRIERE
    
    if (strcmp(topic, TOPIC_CMD_VANNE_2M) == 0) {
        mqtt_callback_commande_arriere("vanne_2m", payload);
        return;
    }
    
    if (strcmp(topic, TOPIC_CMD_VANNE_BOUT_RAMPE) == 0) {
        mqtt_callback_commande_arriere("vanne_bout_rampe", payload);
        return;
    }
    
    if (strcmp(topic, TOPIC_CMD_PHARES_ARRIERE) == 0) {
        mqtt_callback_commande_arriere("phares", payload);
        return;
    }
    
#endif // BOARD_TYPE_ARRIERE
    
    // ========================================================================
    // MESSAGE NON GÉRÉ
    // ========================================================================
    
    ESP_LOGD(TAG, "Message non géré sur topic: %s", topic);
}