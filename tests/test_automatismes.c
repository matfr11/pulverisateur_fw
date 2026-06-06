/*
 * Tests : machines à états transfert + brassage automatique
 * (gestion_automatismes.c — CARTE_AVANT)
 */

#include "mocks/esp_log.h"
#include "mocks/esp_err.h"
#include "mocks/esp_timer.h"
#include "mocks/board_config.h"

/* Mocks actionneurs et capteurs (avant l'include du .c) */
#include "mocks/gestion_actionneurs_mock.h"
#include "mocks/gestion_capteurs_mock.h"

/* État global des mocks */
bool            mock_pompe_active  = false;
etat_vanne_3v_t mock_v3v           = V3V_BRASSAGE;
float           mock_debit_lpm     = 0.0f;
float           mock_volume_session = 0.0f;

/* Remplacer les includes dans gestion_automatismes.c par nos mocks */
#define GESTION_ACTIONNEURS_H
#define GESTION_CAPTEURS_H

/* Fournir les includes directs */
#include "mocks/gestion_actionneurs_mock.h"
#include "mocks/gestion_capteurs_mock.h"

#include "../carte_relais/components/gestion_automatismes/gestion_automatismes.c"

#include "test_runner.h"
#include <stdio.h>

/* Config de test */
static configuration_t cfg_test = {
    .volume_transfert   = 100,    /* 100 L */
    .temps_brassage_on  = 10,     /* 10 secondes */
    .temps_brassage_off = 5,      /* 5 secondes */
};

static void reset(void) {
    mock_time_us      = 0;
    mock_pompe_active = false;
    mock_v3v          = V3V_BRASSAGE;
    mock_debit_lpm    = 0.0f;
    mock_volume_session = 0.0f;
    automatismes_initialiser();
}

/* ================================================================
 * TESTS : Transfert automatique
 * ================================================================ */

static void test_tr_start_sets_pompe_et_vanne(void) {
    reset();
    automatismes_transfert_activer(&cfg_test);
    TEST_ASSERT(mock_pompe_active);
    TEST_ASSERT(mock_v3v == V3V_TRANSFERT);
    TEST_ASSERT_EQ(automatismes_get_etat_transfert(), AUTO_TR_EN_COURS);
}

static void test_tr_start_idempotent(void) {
    reset();
    automatismes_transfert_activer(&cfg_test);
    automatismes_transfert_activer(&cfg_test);  /* 2ème appel ignoré */
    TEST_ASSERT_EQ(automatismes_get_etat_transfert(), AUTO_TR_EN_COURS);
}

static void test_tr_arret_manuel(void) {
    reset();
    automatismes_transfert_activer(&cfg_test);
    automatismes_transfert_arreter();
    TEST_ASSERT(!mock_pompe_active);
    TEST_ASSERT(mock_v3v == V3V_BRASSAGE);
    TEST_ASSERT_EQ(automatismes_get_etat_transfert(), AUTO_TR_INACTIF);
}

static void test_tr_volume_tracking(void) {
    reset();
    automatismes_transfert_activer(&cfg_test);
    mock_volume_session = 50.0f;   /* 50 L transférés */
    /* Simuler pompe ON + vanne transfert */
    automatismes_update(mock_debit_lpm, mock_volume_session);
    TEST_ASSERT_FLOAT_EQ(automatismes_get_volume_transfere(), 50.0f, 0.1f);
}

static void test_tr_auto_stop_quand_cible_atteinte(void) {
    reset();
    automatismes_transfert_activer(&cfg_test);   /* cible = 100 L */
    mock_volume_session = 100.0f;
    automatismes_update(mock_debit_lpm, mock_volume_session);
    /* Auto-stop : pompe OFF, vanne brassage, état TERMINE visible 1 itération */
    TEST_ASSERT(!mock_pompe_active);
    TEST_ASSERT(mock_v3v == V3V_BRASSAGE);
    TEST_ASSERT_EQ(automatismes_get_etat_transfert(), AUTO_TR_TERMINE);
    /* Itération suivante : TERMINE nettoyé → INACTIF */
    automatismes_update(mock_debit_lpm, mock_volume_session);
    TEST_ASSERT_EQ(automatismes_get_etat_transfert(), AUTO_TR_INACTIF);
}

static void test_tr_suspend_brassage(void) {
    reset();
    automatismes_brassage_activer(&cfg_test);    /* brassage actif */
    TEST_ASSERT_EQ(automatismes_get_etat_brassage(), AUTO_BR_MARCHE);

    automatismes_transfert_activer(&cfg_test);   /* transfert → brassage suspendu */
    TEST_ASSERT_EQ(automatismes_get_etat_brassage(), AUTO_BR_SUSPENDU);
}

static void test_tr_resume_brassage_apres_arret(void) {
    reset();
    automatismes_brassage_activer(&cfg_test);
    automatismes_transfert_activer(&cfg_test);   /* brassage suspendu */
    automatismes_transfert_arreter();            /* transfert arrêté → brassage reprend */
    TEST_ASSERT_EQ(automatismes_get_etat_brassage(), AUTO_BR_MARCHE);
    TEST_ASSERT(mock_pompe_active);
}

/* ================================================================
 * TESTS : Brassage automatique
 * ================================================================ */

static void test_br_start(void) {
    reset();
    automatismes_brassage_activer(&cfg_test);
    TEST_ASSERT_EQ(automatismes_get_etat_brassage(), AUTO_BR_MARCHE);
    TEST_ASSERT(mock_pompe_active);
    TEST_ASSERT(mock_v3v == V3V_BRASSAGE);
}

static void test_br_start_idempotent(void) {
    reset();
    automatismes_brassage_activer(&cfg_test);
    automatismes_brassage_activer(&cfg_test);   /* 2ème appel ignoré */
    TEST_ASSERT_EQ(automatismes_get_etat_brassage(), AUTO_BR_MARCHE);
}

static void test_br_marche_to_pause_apres_temps_on(void) {
    reset();
    automatismes_brassage_activer(&cfg_test);   /* temps_on = 10 s */

    /* Avancer juste avant la fin */
    mock_time_avancer_ms(9999);
    automatismes_update(0.0f, 0.0f);
    TEST_ASSERT_EQ(automatismes_get_etat_brassage(), AUTO_BR_MARCHE);

    /* Dépasser temps_on */
    mock_time_avancer_ms(2);
    automatismes_update(0.0f, 0.0f);
    TEST_ASSERT_EQ(automatismes_get_etat_brassage(), AUTO_BR_PAUSE);
    TEST_ASSERT(!mock_pompe_active);
}

static void test_br_pause_to_marche_apres_temps_off(void) {
    reset();
    automatismes_brassage_activer(&cfg_test);

    /* Aller en PAUSE */
    mock_time_avancer_ms(10001);
    automatismes_update(0.0f, 0.0f);
    TEST_ASSERT_EQ(automatismes_get_etat_brassage(), AUTO_BR_PAUSE);

    /* Avancer de temps_off = 5 s */
    mock_time_avancer_ms(5001);
    automatismes_update(0.0f, 0.0f);
    TEST_ASSERT_EQ(automatismes_get_etat_brassage(), AUTO_BR_MARCHE);
    TEST_ASSERT(mock_pompe_active);
}

static void test_br_arret(void) {
    reset();
    automatismes_brassage_activer(&cfg_test);
    automatismes_brassage_arreter();
    TEST_ASSERT_EQ(automatismes_get_etat_brassage(), AUTO_BR_INACTIF);
    TEST_ASSERT(!mock_pompe_active);
}

static void test_br_suspendu_pendant_transfert(void) {
    reset();
    automatismes_transfert_activer(&cfg_test);  /* transfert en cours */
    automatismes_brassage_activer(&cfg_test);   /* démarrer brassage pendant transfert */
    TEST_ASSERT_EQ(automatismes_get_etat_brassage(), AUTO_BR_SUSPENDU);
}

static void test_arret_tout(void) {
    reset();
    automatismes_brassage_activer(&cfg_test);
    automatismes_transfert_activer(&cfg_test);
    automatismes_arreter_tout();
    TEST_ASSERT_EQ(automatismes_get_etat_transfert(), AUTO_TR_INACTIF);
    TEST_ASSERT_EQ(automatismes_get_etat_brassage(),  AUTO_BR_INACTIF);
    TEST_ASSERT(!mock_pompe_active);
}

/* ================================================================
 * AUTO_TR_TERMINE observable pendant exactement une itération
 * ================================================================ */
static void test_regression_tr_termine_jamais_observable(void) {
    reset();
    automatismes_transfert_activer(&cfg_test);   /* cible = 100 L */
    mock_volume_session = 100.0f;
    automatismes_update(mock_debit_lpm, mock_volume_session);

    /* Après la première update : TERMINE est visible (app_tache_principale peut le publier) */
    etat_auto_transfert_t etat = automatismes_get_etat_transfert();
    TEST_ASSERT_EQ(etat, AUTO_TR_TERMINE);

    /* Après la deuxième update : TERMINE est nettoyé → INACTIF */
    automatismes_update(mock_debit_lpm, mock_volume_session);
    etat = automatismes_get_etat_transfert();
    TEST_ASSERT_EQ(etat, AUTO_TR_INACTIF);
}

/* ================================================================
 * TESTS : automatismes_get_brassage_info()
 * ================================================================ */
static void test_br_info_inactif(void) {
    reset();
    char label[16];
    float restant, pct;
    automatismes_get_brassage_info(label, sizeof(label), &restant, &pct);
    TEST_ASSERT_EQ(strcmp(label, "INACTIF"), 0);
    TEST_ASSERT_FLOAT_EQ(restant, 0.0f, 0.001f);
    TEST_ASSERT_FLOAT_EQ(pct, 0.0f, 0.001f);
}

static void test_br_info_marche_mi_cycle(void) {
    reset();
    automatismes_brassage_activer(&cfg_test);  /* temps_on = 10 s */
    mock_time_avancer_ms(5000);               /* à mi-cycle */
    char label[16];
    float restant, pct;
    automatismes_get_brassage_info(label, sizeof(label), &restant, &pct);
    TEST_ASSERT_EQ(strcmp(label, "MARCHE"), 0);
    TEST_ASSERT_FLOAT_EQ(restant, 5.0f, 0.1f);   /* ~5 s restantes */
    TEST_ASSERT_FLOAT_EQ(pct, 50.0f, 1.0f);       /* ~50% */
}

/* ================================================================
 * main
 * ================================================================ */
int main(void) {
    SUITE("Transfert automatique");
    RUN(test_tr_start_sets_pompe_et_vanne);
    RUN(test_tr_start_idempotent);
    RUN(test_tr_arret_manuel);
    RUN(test_tr_volume_tracking);
    RUN(test_tr_auto_stop_quand_cible_atteinte);
    RUN(test_tr_suspend_brassage);
    RUN(test_tr_resume_brassage_apres_arret);

    SUITE("Brassage automatique");
    RUN(test_br_start);
    RUN(test_br_start_idempotent);
    RUN(test_br_marche_to_pause_apres_temps_on);
    RUN(test_br_pause_to_marche_apres_temps_off);
    RUN(test_br_arret);
    RUN(test_br_suspendu_pendant_transfert);
    RUN(test_arret_tout);

    SUITE("Infos brassage");
    RUN(test_br_info_inactif);
    RUN(test_br_info_marche_mi_cycle);

    SUITE("Régressions connues");
    RUN(test_regression_tr_termine_jamais_observable);

    SUMMARY();
    return EXIT_CODE();
}
