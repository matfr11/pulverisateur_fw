#pragma once
#include <stdbool.h>

extern float mock_debit_lpm;
extern float mock_volume_session;

static inline float capteurs_debitmetre_get_debit(void)          { return mock_debit_lpm; }
static inline float capteurs_debitmetre_get_volume_session(void)  { return mock_volume_session; }
static inline void  capteurs_debitmetre_reset_session(void)       { mock_volume_session = 0.0f; }
static inline bool  capteurs_debitmetre_est_ok(void)              { return true; }
