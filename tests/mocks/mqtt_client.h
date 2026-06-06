#pragma once
#include "esp_err.h"
typedef void* esp_mqtt_client_handle_t;
typedef int esp_mqtt_event_id_t;
typedef void* esp_event_base_t;
#define ESP_EVENT_ANY_ID -1
#define MQTT_EVENT_CONNECTED    0
#define MQTT_EVENT_DISCONNECTED 1
#define MQTT_EVENT_DATA         2
#define MQTT_EVENT_ERROR        3

typedef struct {
    char *topic;
    int   topic_len;
    char *data;
    int   data_len;
} *esp_mqtt_event_handle_t;

typedef struct { struct { struct { const char *uri; } address; } broker;
                 struct { const char *client_id; } credentials;
                 struct { int keepalive; } session;
                 struct { int reconnect_timeout_ms; } network; } esp_mqtt_client_config_t;

static inline esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *c) { (void)c; return (void*)1; }
static inline esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t c) { (void)c; return ESP_OK; }
static inline esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t c, esp_mqtt_event_id_t id, void *cb, void *arg) { (void)c;(void)id;(void)cb;(void)arg; return ESP_OK; }
static inline int esp_mqtt_client_publish(esp_mqtt_client_handle_t c, const char *t, const char *d, int l, int q, int r) { (void)c;(void)t;(void)d;(void)l;(void)q;(void)r; return 1; }
static inline int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t c, const char *t, int q) { (void)c;(void)t;(void)q; return 1; }
