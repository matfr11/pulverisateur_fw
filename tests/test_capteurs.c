/*
 * Tests : détection débitmètre déconnecté (gestion_capteurs.c — CARTE_AVANT)
 *
 * Teste que capteurs_debitmetre_est_ok() passe à false après ABSENCE_TIMEOUT_CYCLES
 * cycles sans impulsions, et repasse à true dès que des impulsions arrivent.
 */

#include "mocks/esp_log.h"
#include "mocks/esp_err.h"
#include "mocks/esp_timer.h"
#include "mocks/board_config.h"
#include "mocks/gestion_actionneurs_mock.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "../carte_relais/components/gestion_capteurs/gestion_capteurs.c"
#pragma GCC diagnostic pop

#include "test_runner.h"

/* Simuler des impulsions : incrémenter le compteur brut */
static void simuler_impulsions(int n)
{
    s_impulsions_brutes += n;
}

static void reset_capteurs(void)
{
    s_impulsions_brutes = 0;
    s_impulsions_precedentes = 0;
    s_debitmetre_ok = false;
    s_compteur_absence = 0;
    s_debit_lpm = 0.0f;
    s_volume_session_l = 0.0f;
    s_timestamp_dernier_calcul = 0;
    mock_time_us = 0;
}

/* ================================================================
 * TESTS
 * ================================================================ */

static void test_debitmetre_ok_apres_impulsions(void) {
    reset_capteurs();
    simuler_impulsions(10);
    mock_time_us = 1000000;  /* 1 seconde */
    capteurs_debitmetre_update();
    TEST_ASSERT(capteurs_debitmetre_est_ok());
}

static void test_debitmetre_ko_apres_timeout(void) {
    reset_capteurs();
    /* Initialiser comme si le capteur était en bon état */
    simuler_impulsions(5);
    mock_time_us = 1000000;
    capteurs_debitmetre_update();
    TEST_ASSERT(capteurs_debitmetre_est_ok());

    /* Plus d'impulsions : appeler ABSENCE_TIMEOUT_CYCLES+1 fois sans nouvelles impulsions */
    for (int i = 0; i <= ABSENCE_TIMEOUT_CYCLES; i++) {
        mock_time_us += 100000;
        capteurs_debitmetre_update();
    }
    TEST_ASSERT(!capteurs_debitmetre_est_ok());
}

static void test_debitmetre_rearm_apres_impulsions(void) {
    reset_capteurs();
    /* Simuler un capteur déconnecté : absence counter maxed */
    s_debitmetre_ok = false;
    s_compteur_absence = ABSENCE_TIMEOUT_CYCLES + 1;

    /* De nouvelles impulsions arrivent : le capteur est de nouveau OK */
    simuler_impulsions(3);
    mock_time_us = 1000000;
    capteurs_debitmetre_update();
    TEST_ASSERT(capteurs_debitmetre_est_ok());
    TEST_ASSERT_EQ(s_compteur_absence, 0);
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main(void)
{
    SUITE("Débitmètre OK/KO");
    RUN(test_debitmetre_ok_apres_impulsions);
    RUN(test_debitmetre_ko_apres_timeout);
    RUN(test_debitmetre_rearm_apres_impulsions);
    SUMMARY();
    return EXIT_CODE();
}
