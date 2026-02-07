/**
 * @file gestion_capteurs.c
 * @brief Débitmètre à impulsions (ISR) et sonde de niveau hydrostatique 4-20mA.
 */
#include "gestion_capteurs.h"
#include "board_config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#ifdef CARTE_ARRIERE
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#endif

static const char *TAG = "CAPTEURS";

/* ====================================================================
 * DÉBITMÈTRE (carte AVANT)
 *
 * Le débitmètre génère des impulsions proportionnelles au débit.
 * Formule : débit (L/min) = fréquence (Hz) / facteur_K
 * Volume : volume (L) = nb_impulsions / facteur_K
 * ==================================================================== */
#if A_DEBITMETRE

static volatile uint32_t s_impulsions_brutes = 0;
static uint32_t          s_impulsions_precedentes = 0;
static float             s_facteur_k = 4.72f;        /* Impulsions/litre (par défaut) */
static float             s_debit_lpm = 0.0f;          /* Débit en L/min */
static float             s_volume_session_l = 0.0f;   /* Volume cumulé */
static int64_t           s_timestamp_dernier_calcul = 0;
static bool              s_debitmetre_ok = false;

/* Compteur d'absence de signal pour détecter capteur déconnecté */
static int               s_compteur_absence = 0;
#define ABSENCE_TIMEOUT_CYCLES  30  /* ~3 secondes à 100ms */

/** ISR : compteur d'impulsions (front montant) */
static void IRAM_ATTR debitmetre_isr_handler(void *arg)
{
    s_impulsions_brutes++;
}

static void debitmetre_init(void)
{
#ifndef MODE_SIMULATION
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_DEBITMETRE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_DEBITMETRE, debitmetre_isr_handler, NULL);
#endif

    s_timestamp_dernier_calcul = esp_timer_get_time();
    s_debitmetre_ok = true;
    ESP_LOGI(TAG, "Débitmètre initialisé (GPIO %d, K=%.2f)", GPIO_DEBITMETRE, s_facteur_k);
}

void capteurs_debitmetre_update(void)
{
    int64_t maintenant = esp_timer_get_time();
    int64_t delta_us = maintenant - s_timestamp_dernier_calcul;
    s_timestamp_dernier_calcul = maintenant;

    /* Lire le compteur d'impulsions (atomique sur ESP32 pour uint32_t) */
    uint32_t impulsions = s_impulsions_brutes;
    uint32_t delta_impulsions = impulsions - s_impulsions_precedentes;
    s_impulsions_precedentes = impulsions;

#ifdef MODE_SIMULATION
    /* En simulation, générer un débit fictif si pompe active */
    extern bool actionneurs_pompe_est_active(void);
    if (actionneurs_pompe_est_active()) {
        s_debit_lpm = 45.0f + (float)(esp_timer_get_time() % 50) / 10.0f;
        /* Accumuler le volume directement (débit × durée du tick) */
        float delta_s = (float)delta_us / 1000000.0f;
        s_volume_session_l += s_debit_lpm * delta_s / 60.0f;
    } else {
        s_debit_lpm = 0.0f;
    }
    delta_impulsions = 0;  /* Pas d'accumulation par impulsions en simu */
    s_debitmetre_ok = true;
#else

    /* Calcul du débit : freq = impulsions / durée, débit = freq / K */
    if (delta_us > 0 && s_facteur_k > 0.0f) {
        float delta_s = (float)delta_us / 1000000.0f;
        float freq_hz = (float)delta_impulsions / delta_s;
        s_debit_lpm = (freq_hz / s_facteur_k) * 60.0f;
    }
#endif
    /* Cumul du volume */
    if (s_facteur_k > 0.0f) {
        s_volume_session_l += (float)delta_impulsions / s_facteur_k;
    }

    /* Détection capteur déconnecté */
    if (delta_impulsions == 0) {
        s_compteur_absence++;
        if (s_compteur_absence > ABSENCE_TIMEOUT_CYCLES) {
            /* Pas d'impulsions depuis longtemps, mais on garde le capteur OK
             * tant que la pompe pourrait être arrêtée. La sécurité cuve vide
             * gère le cas pompe active + pas de débit. */
        }
    } else {
        s_compteur_absence = 0;
        s_debitmetre_ok = true;
    }
}

float capteurs_debitmetre_get_debit(void) { return s_debit_lpm; }
float capteurs_debitmetre_get_volume_session(void) { return s_volume_session_l; }
void capteurs_debitmetre_reset_session(void) { s_volume_session_l = 0.0f; }
bool capteurs_debitmetre_est_ok(void) { return s_debitmetre_ok; }
void capteurs_debitmetre_set_facteur_k(float k) {
    if (k > 0.0f) {
        s_facteur_k = k;
        ESP_LOGI(TAG, "Facteur K mis à jour : %.2f", k);
    }
}

#else
/* Stubs pour la carte arrière */
void capteurs_debitmetre_update(void) {}
float capteurs_debitmetre_get_debit(void) { return 0.0f; }
float capteurs_debitmetre_get_volume_session(void) { return 0.0f; }
void capteurs_debitmetre_reset_session(void) {}
bool capteurs_debitmetre_est_ok(void) { return false; }
void capteurs_debitmetre_set_facteur_k(float k) { (void)k; }
#endif /* A_DEBITMETRE */

/* ====================================================================
 * SONDE DE NIVEAU (carte ARRIÈRE)
 *
 * Sonde hydrostatique 2m 4-20mA :
 *   4mA  → 0% (cuve vide)
 *   20mA → 100% (cuve pleine)
 *
 * Avec une résistance de 250Ω :
 *   4mA × 250Ω  = 1.0V
 *   20mA × 250Ω = 5.0V → Attention, l'ADC ESP32 est 0-3.3V !
 *   → Utiliser un diviseur de tension ou un atténuateur.
 *   → Avec atténuation 11dB, lecture jusqu'à ~2.5V environ.
 *   → Résistance de 150Ω : 4mA=0.6V, 20mA=3.0V ✓
 * ==================================================================== */
#if A_SONDE_NIVEAU

static float s_niveau_pct = 0.0f;
static bool  s_sonde_ok = false;

#ifndef MODE_SIMULATION
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
#endif

/* Calibration : tensions correspondant à 4mA et 20mA (en mV) */
#define TENSION_4MA_MV    600   /* 4mA × 150Ω = 0.6V */
#define TENSION_20MA_MV   3000  /* 20mA × 150Ω = 3.0V */

static void sonde_niveau_init(void)
{
#ifndef MODE_SIMULATION
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_cfg, &s_adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(s_adc_handle, ADC_CANAL_SONDE, &chan_cfg);
#endif
    s_sonde_ok = true;
    ESP_LOGI(TAG, "Sonde de niveau initialisée.");
}

void capteurs_sonde_niveau_update(void)
{
#ifdef MODE_SIMULATION
    s_niveau_pct = 65.0f; /* Valeur fictive */
    s_sonde_ok = true;
    return;
#endif

#ifndef MODE_SIMULATION
    if (!s_adc_handle) return;

    int raw = 0;
    esp_err_t ret = adc_oneshot_read(s_adc_handle, ADC_CANAL_SONDE, &raw);
    if (ret != ESP_OK) {
        s_sonde_ok = false;
        return;
    }

    /* Conversion brute → mV (approximation linéaire avec atténuation 11dB) */
    /* Pour une calibration précise, utiliser adc_cali */
    float tension_mv = (float)raw * 3300.0f / 4095.0f;

    /* Conversion mV → pourcentage */
    if (tension_mv < TENSION_4MA_MV - 100) {
        /* Tension trop basse : sonde déconnectée ou < 4mA */
        s_sonde_ok = false;
        s_niveau_pct = 0.0f;
    } else {
        s_sonde_ok = true;
        float pct = (tension_mv - TENSION_4MA_MV) * 100.0f / (TENSION_20MA_MV - TENSION_4MA_MV);
        if (pct < 0.0f) pct = 0.0f;
        if (pct > 100.0f) pct = 100.0f;
        s_niveau_pct = pct;
    }
#endif
}

float capteurs_sonde_get_niveau(void) { return s_niveau_pct; }
bool capteurs_sonde_est_ok(void) { return s_sonde_ok; }

#else
/* Stubs pour la carte avant */
void capteurs_sonde_niveau_update(void) {}
float capteurs_sonde_get_niveau(void) { return 0.0f; }
bool capteurs_sonde_est_ok(void) { return false; }
#endif /* A_SONDE_NIVEAU */

/* ====================================================================
 * INITIALISATION GÉNÉRALE
 * ==================================================================== */
void capteurs_initialiser(void)
{
    ESP_LOGI(TAG, "Initialisation des capteurs...");

#if A_DEBITMETRE
    debitmetre_init();
#endif

#if A_SONDE_NIVEAU
    sonde_niveau_init();
#endif
}
