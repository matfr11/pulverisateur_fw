#pragma once
#include "FreeRTOS.h"
static inline BaseType_t xTaskCreate(void(*f)(void*), const char *n, uint32_t s, void *p, uint32_t pr, TaskHandle_t *h) {
    (void)f; (void)n; (void)s; (void)p; (void)pr; (void)h; return pdPASS;
}
static inline void vTaskDelete(TaskHandle_t h) { (void)h; }
