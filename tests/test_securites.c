/*
 * Tests : machine à états sécurité cuve vide
 * (gestion_securites.c — CARTE_AVANT)
 */

#include "mocks/esp_log.h"
#include "mocks/esp_err.h"
#include "mocks/esp_timer.h"
#include "mocks/board_config.h"

#include "../carte_relais/components/gestion_securites/gestion_securites.c"

#include "test_runner.h"

/* Config de test */
static configuration_t cfg_test = {
    .seuil_debit_cuve_vide = 1.2f,
    .delai_detection_ms    = 3000,
};

static void reset(void) {
    mock_time_us = 0;
    securites_initialiser();
}

/* ================================================================
 * TESTS
 * ================================================================ */

static void test_sec_init_state(void) {
    reset();
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_OK);
    TEST_ASSERT(!securites_cuve_est_vide());
}

static void test_sec_no_trigger_pompe_off(void) {
    reset();
    securites_update(false, 0.0f, &cfg_test);
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_OK);
}

static void test_sec_no_trigger_debit_ok(void) {
    reset();
    securites_update(true, 45.0f, &cfg_test);  /* débit largement au-dessus du seuil */
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_OK);
}

static void test_sec_no_trigger_debit_exactement_seuil(void) {
    reset();
    /* Débit == seuil : la condition est strictement < seuil, donc pas de déclenchement */
    securites_update(true, 1.2f, &cfg_test);
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_OK);
}

static void test_sec_transition_to_detection(void) {
    reset();
    securites_update(true, 0.5f, &cfg_test);  /* pompe ON + débit < seuil */
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_DETECTION);
}

static void test_sec_detection_false_alarm_debit_revient(void) {
    reset();
    securites_update(true, 0.5f, &cfg_test);   /* → DETECTION */
    securites_update(true, 45.0f, &cfg_test);  /* débit revient */
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_OK);
}

static void test_sec_detection_false_alarm_pompe_off(void) {
    reset();
    securites_update(true, 0.5f, &cfg_test);    /* → DETECTION */
    securites_update(false, 0.5f, &cfg_test);   /* pompe s'arrête */
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_OK);
}

static void test_sec_detection_to_vide_apres_delai(void) {
    reset();
    securites_update(true, 0.5f, &cfg_test);    /* → DETECTION, t=0 */

    /* Avancer juste avant le délai → encore en DETECTION */
    mock_time_avancer_ms(2999);
    securites_update(true, 0.5f, &cfg_test);
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_DETECTION);

    /* Dépasser le délai → CUVE_VIDE */
    mock_time_avancer_ms(2);
    securites_update(true, 0.5f, &cfg_test);
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_VIDE);
    TEST_ASSERT(securites_cuve_est_vide());
}

static void test_sec_vide_reste_vide_sans_rearmement(void) {
    reset();
    securites_update(true, 0.5f, &cfg_test);
    mock_time_avancer_ms(4000);
    securites_update(true, 0.5f, &cfg_test);   /* → VIDE */
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_VIDE);

    /* La pompe s'arrête, débit nul : on reste VIDE (pas de réarmement auto sans débit) */
    securites_update(false, 0.0f, &cfg_test);
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_VIDE);
}

static void test_sec_rearmement_manuel(void) {
    reset();
    securites_update(true, 0.5f, &cfg_test);
    mock_time_avancer_ms(4000);
    securites_update(true, 0.5f, &cfg_test);   /* → VIDE */

    securites_rearmement_cuve();
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_OK);
    TEST_ASSERT(!securites_cuve_est_vide());
}

static void test_sec_vide_auto_rearm_pompe_et_debit(void) {
    reset();
    securites_update(true, 0.5f, &cfg_test);
    mock_time_avancer_ms(4000);
    securites_update(true, 0.5f, &cfg_test);   /* → VIDE */

    /* Opérateur relance pompe ET débit revient → réarmement auto */
    securites_update(true, 45.0f, &cfg_test);
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_OK);
}

static void test_sec_vide_no_rearm_debit_seul(void) {
    reset();
    securites_update(true, 0.5f, &cfg_test);
    mock_time_avancer_ms(4000);
    securites_update(true, 0.5f, &cfg_test);   /* → VIDE */

    /* Débit OK mais pompe OFF → pas de réarmement */
    securites_update(false, 45.0f, &cfg_test);
    TEST_ASSERT_EQ(securites_get_etat_cuve(), SEC_CUVE_VIDE);
}

/* ================================================================
 * main
 * ================================================================ */
int main(void) {
    SUITE("Sécurité cuve vide");
    RUN(test_sec_init_state);
    RUN(test_sec_no_trigger_pompe_off);
    RUN(test_sec_no_trigger_debit_ok);
    RUN(test_sec_no_trigger_debit_exactement_seuil);
    RUN(test_sec_transition_to_detection);
    RUN(test_sec_detection_false_alarm_debit_revient);
    RUN(test_sec_detection_false_alarm_pompe_off);
    RUN(test_sec_detection_to_vide_apres_delai);
    RUN(test_sec_vide_reste_vide_sans_rearmement);
    RUN(test_sec_rearmement_manuel);
    RUN(test_sec_vide_auto_rearm_pompe_et_debit);
    RUN(test_sec_vide_no_rearm_debit_seul);

    SUMMARY();
    return EXIT_CODE();
}
