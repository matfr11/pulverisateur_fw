#pragma once
#include "esp_err.h"
typedef void* esp_event_loop_handle_t;
#define WIFI_EVENT NULL
#define IP_EVENT   NULL
#define ESP_EVENT_ANY_ID -1
static inline esp_err_t esp_event_handler_instance_register(void *b, int32_t id, void *h, void *a, void **inst) {
    (void)b;(void)id;(void)h;(void)a;(void)inst; return ESP_OK;
}
static inline esp_err_t esp_event_loop_create_default(void) { return ESP_OK; }
