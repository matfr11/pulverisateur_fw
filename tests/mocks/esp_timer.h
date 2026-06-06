#pragma once
#include <stdint.h>

/* Temps injecté par les tests (en microsecondes) */
static int64_t mock_time_us = 0;

static inline int64_t esp_timer_get_time(void) {
    return mock_time_us;
}

/* Helper pour avancer le temps dans les tests */
static inline void mock_time_avancer_ms(int64_t ms) {
    mock_time_us += ms * 1000LL;
}
