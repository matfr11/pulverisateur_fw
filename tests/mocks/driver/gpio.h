#pragma once
#include <stdint.h>

#define IRAM_ATTR

typedef uint64_t gpio_num_t;
typedef enum { GPIO_MODE_INPUT } gpio_mode_t;
typedef enum { GPIO_PULLUP_DISABLE, GPIO_PULLUP_ENABLE } gpio_pullup_t;
typedef enum { GPIO_PULLDOWN_DISABLE, GPIO_PULLDOWN_ENABLE } gpio_pulldown_t;
typedef enum { GPIO_INTR_DISABLE, GPIO_INTR_POSEDGE } gpio_int_type_t;
typedef void (*gpio_isr_t)(void *);

typedef struct {
    uint64_t pin_bit_mask;
    gpio_mode_t mode;
    gpio_pullup_t pull_up_en;
    gpio_pulldown_t pull_down_en;
    gpio_int_type_t intr_type;
} gpio_config_t;

static inline int gpio_config(const gpio_config_t *c) { (void)c; return 0; }
static inline int gpio_install_isr_service(int f) { (void)f; return 0; }
static inline int gpio_isr_handler_add(gpio_num_t p, gpio_isr_t h, void *a) { (void)p;(void)h;(void)a; return 0; }
