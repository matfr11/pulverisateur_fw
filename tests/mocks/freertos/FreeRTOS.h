#pragma once
#include <stdint.h>
typedef uint32_t TickType_t;
typedef void* TaskHandle_t;
typedef void* SemaphoreHandle_t;
typedef int BaseType_t;
#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  1
#define portMAX_DELAY 0xFFFFFFFF
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define BIT0 (1 << 0)
#define BIT1 (1 << 1)
static inline void vTaskDelay(TickType_t t) { (void)t; }
static inline TickType_t xTaskGetTickCount(void) { return 0; }
static inline void vTaskDelayUntil(TickType_t *t, TickType_t inc) { (void)t; (void)inc; }
