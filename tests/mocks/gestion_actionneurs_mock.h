#pragma once
#include "types_pulverisateur.h"
#include <stdbool.h>

/* État mock des actionneurs — lisible depuis les tests */
extern bool             mock_pompe_active;
extern etat_vanne_3v_t  mock_v3v;

/* Implémentations mock (définies dans test_automatismes.c) */
static inline void actionneurs_pompe_set(bool active)    { mock_pompe_active = active; }
static inline bool actionneurs_pompe_est_active(void)    { return mock_pompe_active; }
static inline void actionneurs_v3v_set(etat_vanne_3v_t v){ mock_v3v = v; }
static inline bool actionneurs_v3v_est_transfert(void)   { return mock_v3v == V3V_TRANSFERT; }
static inline void actionneurs_pompe_toggle(void)        { mock_pompe_active = !mock_pompe_active; }
static inline void actionneurs_v3v_toggle(void)          { mock_v3v = (mock_v3v == V3V_BRASSAGE) ? V3V_TRANSFERT : V3V_BRASSAGE; }
static inline void actionneurs_tout_arreter(void)        { mock_pompe_active = false; mock_v3v = V3V_BRASSAGE; }
