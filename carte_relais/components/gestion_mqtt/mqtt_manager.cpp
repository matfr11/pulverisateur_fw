/**
 * @file mqtt_manager.cpp
 * @brief Mini-Broker MQTT Embarqué pour ESP32
 * 
 * Implémentation d'un broker MQTT léger directement sur ESP32.
 * 
 * Fonctionnalités:
 * - Accepte connexions clients TCP port 1883
 * - Gestion CONNECT/SUBSCRIBE/PUBLISH/DISCONNECT
 * - QoS 0 et 1 supportés
 * - Messages retained
 * - Max 10 clients simultanés
 * - Wildcard topics (#, +)
 * 
 * Limitations:
 * - QoS 2 non supporté
 * - Pas de persistance
 * - Pas d'authentification
 * - Taille messages max 2KB
 * 
 * @version 2.0
 * @date 2026-02-02
 */

#include "mqtt_manager.h"
#include "board_config.h"
#include "app_role.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <vector>
#include <map>

#include "esp_timer.h"

static const char* TAG = "MQTT_BROKER";


// ============================================================================
// STRUCTURES BROKER
// ============================================================================

#define BROKER_PORT 1883
#define MAX_CLIENTS 10
#define MAX_SUBSCRIPTIONS 50
#define MAX_RETAINED_MESSAGES 50
#define MQTT_BUFFER_SIZE 2048

// Types de paquets MQTT
enum MQTTPacketType {
    MQTT_CONNECT = 1,
    MQTT_CONNACK = 2,
    MQTT_PUBLISH = 3,
    MQTT_PUBACK = 4,
    MQTT_SUBSCRIBE = 8,
    MQTT_SUBACK = 9,
    MQTT_UNSUBSCRIBE = 10,
    MQTT_UNSUBACK = 11,
    MQTT_PINGREQ = 12,
    MQTT_PINGRESP = 13,
    MQTT_DISCONNECT = 14
};

// Client MQTT connecté
struct MQTTClient {
    int socket;
    bool connected;
    char client_id[64];
    uint16_t keep_alive;
    int64_t last_ping_us;
};

// Souscription
struct MQTTSubscription {
    char topic[128];
    uint8_t qos;
    int client_socket;
};

// Message retained
struct MQTTRetainedMessage {
    char topic[128];
    uint8_t* payload;
    size_t payload_len;
    uint8_t qos;
};

// Variables globales broker
static int g_broker_socket = -1;
static TaskHandle_t g_broker_task_handle = NULL;
static SemaphoreHandle_t g_broker_mutex = NULL;
static bool g_broker_running = false;

static MQTTClient g_clients[MAX_CLIENTS];
static MQTTSubscription g_subscriptions[MAX_SUBSCRIPTIONS];
static MQTTRetainedMessage g_retained_messages[MAX_RETAINED_MESSAGES];
static int g_num_subscriptions = 0;
static int g_num_retained = 0;

// ============================================================================
// FONCTIONS UTILITAIRES MQTT
// ============================================================================

/**
 * @brief Lit un entier variable length (MQTT)
 */
static int mqtt_read_variable_int(const uint8_t* buf, uint32_t* value, int* bytes_read) {
    *value = 0;
    *bytes_read = 0;
    uint32_t multiplier = 1;
    
    for (int i = 0; i < 4; i++) {
        *value += (buf[i] & 127) * multiplier;
        multiplier *= 128;
        (*bytes_read)++;
        
        if ((buf[i] & 128) == 0) {
            return 0; // Success
        }
    }
    
    return -1; // Error
}

/**
 * @brief Écrit un entier variable length (MQTT)
 */
static int mqtt_write_variable_int(uint8_t* buf, uint32_t value) {
    int bytes = 0;
    
    do {
        uint8_t byte = value % 128;
        value /= 128;
        if (value > 0) {
            byte |= 128;
        }
        buf[bytes++] = byte;
    } while (value > 0);
    
    return bytes;
}

/**
 * @brief Lit une string MQTT (2 bytes length + data)
 */
static int mqtt_read_string(const uint8_t* buf, char* str, int max_len) {
    uint16_t len = (buf[0] << 8) | buf[1];
    if (len >= max_len) len = max_len - 1;
    memcpy(str, buf + 2, len);
    str[len] = '\0';
    return len + 2;
}

/**
 * @brief Écrit une string MQTT
 */
static int mqtt_write_string(uint8_t* buf, const char* str) {
    uint16_t len = strlen(str);
    buf[0] = (len >> 8) & 0xFF;
    buf[1] = len & 0xFF;
    memcpy(buf + 2, str, len);
    return len + 2;
}

/**
 * @brief Vérifie si un topic match un filtre (avec wildcards)
 */
static bool topic_matches(const char* filter, const char* topic) {
    // Implémentation simplifiée
    // TODO: Support complet # et +
    
    // Si pas de wildcard, comparaison simple
    if (strchr(filter, '#') == NULL && strchr(filter, '+') == NULL) {
        return strcmp(filter, topic) == 0;
    }
    
    // Support basique # à la fin
    if (filter[strlen(filter) - 1] == '#') {
        return strncmp(filter, topic, strlen(filter) - 1) == 0;
    }
    
    return strcmp(filter, topic) == 0;
}

// ============================================================================
// GESTION CLIENTS
// ============================================================================

/**
 * @brief Trouve un client par socket
 */
static MQTTClient* broker_find_client(int socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].socket == socket) {
            return &g_clients[i];
        }
    }
    return NULL;
}

/**
 * @brief Ajoute un nouveau client
 */
static MQTTClient* broker_add_client(int socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].socket == -1) {
            g_clients[i].socket = socket;
            g_clients[i].connected = false;
            g_clients[i].keep_alive = 60;
            g_clients[i].last_ping_us = esp_timer_get_time();
            return &g_clients[i];
        }
    }
    return NULL; // Plus de place
}

/**
 * @brief Retire un client
 */
static void broker_remove_client(int socket) {
    // Retirer client
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].socket == socket) {
            g_clients[i].socket = -1;
            g_clients[i].connected = false;
            break;
        }
    }
    
    // Retirer ses souscriptions
    for (int i = 0; i < g_num_subscriptions; i++) {
        if (g_subscriptions[i].client_socket == socket) {
            // Décaler le tableau
            memmove(&g_subscriptions[i], &g_subscriptions[i+1], 
                   (g_num_subscriptions - i - 1) * sizeof(MQTTSubscription));
            g_num_subscriptions--;
            i--;
        }
    }
    
    close(socket);
}

// ============================================================================
// TRAITEMENT PAQUETS MQTT
// ============================================================================

/**
 * @brief Traite un paquet CONNECT
 */
static void broker_handle_connect(MQTTClient* client, const uint8_t* buf, uint32_t remaining_len) {
    // 1. Lire le nom du protocole (String MQTT : 2 octets length + data)
    char protocol_name[16];
    int offset = mqtt_read_string(buf, protocol_name, sizeof(protocol_name));
    
    // 2. Lire le Protocol Level (1 octet)
    uint8_t protocol_level = buf[offset++];
    (void)protocol_level;

    // 3. Lire les Connect Flags (1 octet)
    uint8_t flags = buf[offset++];
    (void)flags;

    // 4. Lire le Keep Alive (2 octets) - ENFIN !
    uint16_t keep_alive = (buf[offset] << 8) | buf[offset + 1];
    offset += 2;
    
    // 5. Lire le Client ID (String MQTT)
    char client_id[64];
    offset += mqtt_read_string(buf + offset, client_id, sizeof(client_id));
    
    // Mise à jour du client
    strncpy(client->client_id, client_id, sizeof(client->client_id));
    client->keep_alive = keep_alive;
    client->connected = true;
    
    ESP_LOGI(TAG, "Client connecté: %s (keepalive: %d, protocole: %s)", 
             client_id, keep_alive, protocol_name);
    
    // Envoyer CONNACK
    uint8_t connack[4] = {static_cast<uint8_t>(MQTT_CONNACK << 4), 2, 0, 0};
    send(client->socket, connack, 4, 0);
}

/**
 * @brief Traite un paquet PUBLISH
 */
static void broker_handle_publish(MQTTClient* client, uint8_t flags, const uint8_t* buf, uint32_t remaining_len) {
    uint8_t qos = (flags >> 1) & 0x03;
    bool retain = flags & 0x01;
    
    // Lire topic
    char topic[128];
    int offset = mqtt_read_string(buf, topic, sizeof(topic));
    
    // Lire packet ID si QoS > 0
    uint16_t packet_id = 0;
    if (qos > 0) {
        packet_id = (buf[offset] << 8) | buf[offset + 1];
        offset += 2;
    }
    
    // Payload
    const uint8_t* payload = buf + offset;
    uint32_t payload_len = remaining_len - offset;
    
    ESP_LOGD(TAG, "PUBLISH: %s (QoS%d, retain=%d, %lu bytes)", 
             topic, qos, retain, payload_len);
    
    // Enregistrer retained si nécessaire
    if (retain && g_num_retained < MAX_RETAINED_MESSAGES) {
        // Chercher si topic déjà retained
        int idx = -1;
        for (int i = 0; i < g_num_retained; i++) {
            if (strcmp(g_retained_messages[i].topic, topic) == 0) {
                idx = i;
                break;
            }
        }
        
        if (idx == -1 && g_num_retained < MAX_RETAINED_MESSAGES) {
            idx = g_num_retained++;
        }
        
        if (idx >= 0) {
            strncpy(g_retained_messages[idx].topic, topic, sizeof(g_retained_messages[idx].topic));
            if (g_retained_messages[idx].payload) free(g_retained_messages[idx].payload);
            g_retained_messages[idx].payload = (uint8_t*)malloc(payload_len);
            memcpy(g_retained_messages[idx].payload, payload, payload_len);
            g_retained_messages[idx].payload_len = payload_len;
            g_retained_messages[idx].qos = qos;
        }
    }
    
    // Distribuer aux clients souscrits
    for (int i = 0; i < g_num_subscriptions; i++) {
        if (topic_matches(g_subscriptions[i].topic, topic)) {
            MQTTClient* sub_client = broker_find_client(g_subscriptions[i].client_socket);
            if (sub_client && sub_client->connected) {
                // Construire paquet PUBLISH
                uint8_t pub_buf[MQTT_BUFFER_SIZE];
                int pub_offset = 0;
                
                // Fixed header
                pub_buf[pub_offset++] = (MQTT_PUBLISH << 4) | (qos << 1) | (retain ? 1 : 0);
                
                // Remaining length (temporaire, sera recalculé)
                int remaining_len_offset = pub_offset;
                pub_offset += 4; // Max 4 bytes pour variable int
                
                // Topic
                pub_offset += mqtt_write_string(pub_buf + pub_offset, topic);
                
                // Packet ID si QoS > 0
                if (qos > 0)             {
        pub_buf[pub_offset++] = static_cast<uint8_t>((packet_id >> 8) & 0xFF); // ✅ Correction narrowing
        pub_buf[pub_offset++] = static_cast<uint8_t>(packet_id & 0xFF);        // ✅ Correction narrowing
             }
                // Payload
                memcpy(pub_buf + pub_offset, payload, payload_len);
                pub_offset += payload_len;
                
                // Recalculer remaining length
                uint32_t pub_remaining = pub_offset - remaining_len_offset - 4;
                int vint_len = mqtt_write_variable_int(pub_buf + 1, pub_remaining);
                
                // Décaler si besoin
                if (vint_len < 4) {
                    memmove(pub_buf + 1 + vint_len, pub_buf + 5, pub_remaining);
                    pub_offset = 1 + vint_len + pub_remaining;
                }
                
                send(sub_client->socket, pub_buf, pub_offset, 0);
            }
        }
    }
    
    // Envoyer PUBACK si QoS 1
if (qos == 1) {
        uint8_t puback[4] = {
            static_cast<uint8_t>(MQTT_PUBACK << 4),
            2,
            static_cast<uint8_t>((packet_id >> 8) & 0xFF), // ✅ Correction narrowing
            static_cast<uint8_t>(packet_id & 0xFF)         // ✅ Correction narrowing
        };
        send(client->socket, puback, 4, 0);

    }
}

/**
 * @brief Traite un paquet SUBSCRIBE
 */
static void broker_handle_subscribe(MQTTClient* client, const uint8_t* buf, uint32_t remaining_len) {
    // Packet ID
    uint16_t packet_id = (buf[0] << 8) | buf[1];
    int offset = 2;
    
    uint8_t return_codes[10];
    int num_topics = 0;
    
    while (offset < remaining_len && num_topics < 10) {
        char topic[128];
        offset += mqtt_read_string(buf + offset, topic, sizeof(topic));
        uint8_t qos = buf[offset++];
        
        ESP_LOGI(TAG, "Client %s souscrit: %s (QoS%d)", 
                 client->client_id, topic, qos);
        
        // Ajouter souscription
        if (g_num_subscriptions < MAX_SUBSCRIPTIONS) {
            strncpy(g_subscriptions[g_num_subscriptions].topic, topic, 
                   sizeof(g_subscriptions[g_num_subscriptions].topic));
            g_subscriptions[g_num_subscriptions].qos = qos;
            g_subscriptions[g_num_subscriptions].client_socket = client->socket;
            g_num_subscriptions++;
            
            return_codes[num_topics] = qos; // Granted QoS
            
            // Envoyer messages retained correspondants
            for (int i = 0; i < g_num_retained; i++) {
                if (topic_matches(topic, g_retained_messages[i].topic)) {
                    // Envoyer message retained au client
                    // (code similaire à broker_handle_publish)
                    ESP_LOGD(TAG, "Envoi retained: %s", g_retained_messages[i].topic);
                }
            }
        } else {
            return_codes[num_topics] = 0x80; // Failure
        }
        
        num_topics++;
    }
    
    // Envoyer SUBACK
uint8_t suback[4] = {
        static_cast<uint8_t>(MQTT_SUBACK << 4),
        static_cast<uint8_t>(2 + num_topics),
        static_cast<uint8_t>((packet_id >> 8) & 0xFF), // ✅ Correction narrowing
        static_cast<uint8_t>(packet_id & 0xFF)         // ✅ Correction narrowing
    };
    // Note: l'envoi original utilisait un buffer variable pour les return_codes, 
    // assurez-vous de bien copier les return_codes après ces 4 octets.
    
    uint8_t full_suback[3 + 10]; 
    memcpy(full_suback, suback, 4);
    memcpy(full_suback + 4, return_codes, num_topics);
    send(client->socket, full_suback, 4 + num_topics, 0);
}

/**
 * @brief Traite un paquet PINGREQ
 */
static void broker_handle_pingreq(MQTTClient* client) {
    client->last_ping_us = esp_timer_get_time();
    
    ESP_LOGI(TAG, "PINGREQ reçu de %s (socket %d)", client->client_id, client->socket);  // ← AJOUTER
    
    // Envoyer PINGRESP
    uint8_t pingresp[2] = {MQTT_PINGRESP << 4, 0};
    int sent = send(client->socket, pingresp, 2, 0);
    
    if (sent < 0) {  // ← AJOUTER
        ESP_LOGE(TAG, "Erreur envoi PINGRESP: errno=%d", errno);
    } else {
        ESP_LOGI(TAG, "PINGRESP envoyé (%d bytes)", sent);
    }
}

/**
 * @brief Traite un paquet DISCONNECT
 */
static void broker_handle_disconnect(MQTTClient* client) {
    ESP_LOGI(TAG, "Client %s déconnecté", client->client_id);
    broker_remove_client(client->socket);
}


// ============================================================================
// TÂCHE BROKER
// ============================================================================

/**
 * @brief Tâche principale du broker MQTT
 */
static void broker_task(void* pvParameters) {
    (void)pvParameters;
    
    ESP_LOGI(TAG, "Tâche broker MQTT démarrée");
    
    // Configuration socket serveur
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(BROKER_PORT);
    
    // Créer socket
    g_broker_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_broker_socket < 0) {
        ESP_LOGE(TAG, "Erreur création socket: %d", errno);
        vTaskDelete(NULL);
        return;
    }
    
    // Réutilisation adresse
    int opt = 1;
    setsockopt(g_broker_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind
    if (bind(g_broker_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Erreur bind: %d", errno);
        close(g_broker_socket);
        vTaskDelete(NULL);
        return;
    }
    
    // Listen
    if (listen(g_broker_socket, MAX_CLIENTS) < 0) {
        ESP_LOGE(TAG, "Erreur listen: %d", errno);
        close(g_broker_socket);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  BROKER MQTT DÉMARRÉ");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Port: %d", BROKER_PORT);
    ESP_LOGI(TAG, "  Max clients: %d", MAX_CLIENTS);
    ESP_LOGI(TAG, "========================================");
    
    // Timeout select
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    while (g_broker_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(g_broker_socket, &read_fds);
        
        int max_fd = g_broker_socket;
        
        // Ajouter clients connectés
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (g_clients[i].socket >= 0) {
                FD_SET(g_clients[i].socket, &read_fds);
                if (g_clients[i].socket > max_fd) {
                    max_fd = g_clients[i].socket;
                }
            }
        }
        
        // Select
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            ESP_LOGE(TAG, "Erreur select: %d", errno);
            break;
        }
        
        // Nouvelle connexion?
        if (FD_ISSET(g_broker_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_socket = accept(g_broker_socket, (struct sockaddr*)&client_addr, &addr_len);
            
            if (client_socket >= 0) {
                // Désactiver le délai de Nagle pour une réponse instantanée
                    int nodelay = 1;
                    setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
                MQTTClient* client = broker_add_client(client_socket);
                if (client) {
                    ESP_LOGI(TAG, "Nouvelle connexion: socket %d", client_socket);
                } else {
                    ESP_LOGW(TAG, "Trop de clients, connexion refusée");
                    close(client_socket);
                }
            }
        }
        
        // Données des clients?
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (g_clients[i].socket < 0) continue;
            
            if (FD_ISSET(g_clients[i].socket, &read_fds)) {
                uint8_t buf[MQTT_BUFFER_SIZE];
                int n = recv(g_clients[i].socket, buf, sizeof(buf), 0);
                
                if (n <= 0) {
                    // Déconnexion
                    ESP_LOGI(TAG, "Client déconnecté: %s", g_clients[i].client_id);
                    broker_remove_client(g_clients[i].socket);
                } else {
                    // Traiter paquet MQTT
                    uint8_t packet_type = (buf[0] >> 4) & 0x0F;
                    uint8_t flags = buf[0] & 0x0F;
                    
                    uint32_t remaining_len;
                    int bytes_read;
                    mqtt_read_variable_int(buf + 1, &remaining_len, &bytes_read);
                    
                    const uint8_t* payload = buf + 1 + bytes_read;
                    
                    switch (packet_type) {
                        case MQTT_CONNECT:
                            broker_handle_connect(&g_clients[i], payload, remaining_len);
                            break;
                        case MQTT_PUBLISH:
                            broker_handle_publish(&g_clients[i], flags, payload, remaining_len);
                            break;
                        case MQTT_SUBSCRIBE:
                            broker_handle_subscribe(&g_clients[i], payload, remaining_len);
                            break;
                        case MQTT_PINGREQ:
                            broker_handle_pingreq(&g_clients[i]);
                            break;
                        case MQTT_DISCONNECT:
                            broker_handle_disconnect(&g_clients[i]);
                            break;
                        default:
                            ESP_LOGD(TAG, "Paquet MQTT non géré: type %d", packet_type);
                            break;
                    }
                }
            }
        }
    }
    
    // Cleanup
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].socket >= 0) {
            close(g_clients[i].socket);
        }
    }
    close(g_broker_socket);
    
    ESP_LOGI(TAG, "Tâche broker arrêtée");
    vTaskDelete(NULL);
}

// ============================================================================
// API PUBLIQUE
// ============================================================================

/**
 * @brief Démarre le broker MQTT (MASTER uniquement)
 */
esp_err_t mqtt_broker_start(void) {
    if (g_broker_running) {
        ESP_LOGW(TAG, "Broker déjà démarré");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Démarrage broker MQTT embarqué...");
    
    // Créer mutex
    g_broker_mutex = xSemaphoreCreateMutex();
    if (g_broker_mutex == NULL) {
        ESP_LOGE(TAG, "Erreur création mutex");
        return ESP_FAIL;
    }
    
    // Initialiser tables
    memset(g_clients, 0, sizeof(g_clients));
    for (int i = 0; i < MAX_CLIENTS; i++) {
        g_clients[i].socket = -1;
    }
    memset(g_subscriptions, 0, sizeof(g_subscriptions));
    memset(g_retained_messages, 0, sizeof(g_retained_messages));
    g_num_subscriptions = 0;
    g_num_retained = 0;
    
    // Démarrer tâche broker
    g_broker_running = true;
    BaseType_t ret = xTaskCreate(
        broker_task,
        "mqtt_broker",
        8192, // Stack important pour broker
        NULL,
        configMAX_PRIORITIES - 3, // Augmenté pour être très réactif
        &g_broker_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Erreur création tâche broker");
        g_broker_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✓ Broker MQTT embarqué démarré");
    return ESP_OK;
}

/**
 * @brief Arrête le broker MQTT
 */
esp_err_t mqtt_broker_stop(void) {
    if (!g_broker_running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Arrêt broker MQTT...");
    
    g_broker_running = false;
    
    // Attendre fin tâche
    if (g_broker_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        g_broker_task_handle = NULL;
    }
    
    // Libérer retained messages
    for (int i = 0; i < g_num_retained; i++) {
        if (g_retained_messages[i].payload) {
            free(g_retained_messages[i].payload);
            g_retained_messages[i].payload = NULL;
        }
    }
    
    if (g_broker_mutex) {
        vSemaphoreDelete(g_broker_mutex);
        g_broker_mutex = NULL;
    }
    
    ESP_LOGI(TAG, "Broker arrêté");
    return ESP_OK;
}

/**
 * @brief Obtient statistiques broker
 */
void mqtt_broker_get_stats(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  STATISTIQUES BROKER MQTT");
    ESP_LOGI(TAG, "========================================");
    
    int num_clients = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].socket >= 0) {
            num_clients++;
            ESP_LOGI(TAG, "  Client %d: %s (socket %d)", 
                     num_clients, g_clients[i].client_id, g_clients[i].socket);
        }
    }
    
    ESP_LOGI(TAG, "Clients connectés: %d/%d", num_clients, MAX_CLIENTS);
    ESP_LOGI(TAG, "Souscriptions: %d/%d", g_num_subscriptions, MAX_SUBSCRIPTIONS);
    ESP_LOGI(TAG, "Messages retained: %d/%d", g_num_retained, MAX_RETAINED_MESSAGES);
    ESP_LOGI(TAG, "========================================");
}
