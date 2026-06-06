#pragma once
#include "esp_err.h"
#include <stddef.h>

typedef int esp_ota_handle_t;
#define OTA_SIZE_UNKNOWN 0

static inline const void *esp_ota_get_next_update_partition(const void *p)
    { (void)p; return NULL; }
static inline esp_err_t esp_ota_begin(const void *p, size_t s, esp_ota_handle_t *h)
    { (void)p; (void)s; (void)h; return ESP_OK; }
static inline esp_err_t esp_ota_write(esp_ota_handle_t h, const void *d, size_t s)
    { (void)h; (void)d; (void)s; return ESP_OK; }
static inline esp_err_t esp_ota_end(esp_ota_handle_t h)
    { (void)h; return ESP_OK; }
static inline esp_err_t esp_ota_abort(esp_ota_handle_t h)
    { (void)h; return ESP_OK; }
static inline esp_err_t esp_ota_set_boot_partition(const void *p)
    { (void)p; return ESP_OK; }
