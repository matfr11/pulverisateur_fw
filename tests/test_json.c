/*
 * Tests : sérialisation/désérialisation JSON (round-trips)
 * Couvre les 3 types : etat_avant, etat_arriere, configuration
 */

#include "mocks/esp_log.h"
#include "mocks/esp_err.h"
#include "mocks/mqtt_client.h"

/* cJSON depuis vendor/ */
#include "vendor/cJSON.h"

/* Types et topics réels */
#include "../commun/include/types_pulverisateur.h"
#include "../commun/include/mqtt_topics.h"

/* Inclure le code source réel (JSON functions are in protocole_mqtt.c) */
#include "../commun/protocoles/protocole_mqtt.c"

#include "test_runner.h"
#include <string.h>
#include <math.h>

/* ================================================================
 * TESTS : etat_carte_avant_t round-trip
 * ================================================================ */

static void test_json_etat_avant_roundtrip(void) {
    etat_carte_avant_t src = {
        .pompe             = POMPE_EN_MARCHE,
        .vanne_3v          = V3V_TRANSFERT,
        .phares_avant      = true,
        .debit_instantane  = 42.5f,
        .volume_session    = 123.4f,
        .debitmetre_ok     = true,
        .auto_transfert    = AUTO_TR_EN_COURS,
        .auto_brassage     = AUTO_BR_MARCHE,
        .transfert_volume_cible = 200,
        .brassage_temps_restant = 7.5f,
        .brassage_pourcentage   = 35.0f,
        .securite_cuve     = SEC_CUVE_OK,
        .etat_systeme      = ETAT_SYS_OPERATIONNEL,
    };
    strncpy(src.brassage_label, "MARCHE", sizeof(src.brassage_label));

    char buf[1024];
    int len = json_serialiser_etat_avant(&src, buf, sizeof(buf));
    TEST_ASSERT(len > 0);

    etat_carte_avant_t dst;
    bool ok = json_deserialiser_etat_avant(buf, &dst);
    TEST_ASSERT(ok);

    TEST_ASSERT_EQ(dst.pompe,            src.pompe);
    TEST_ASSERT_EQ(dst.vanne_3v,         src.vanne_3v);
    TEST_ASSERT_EQ(dst.phares_avant,     src.phares_avant);
    TEST_ASSERT_FLOAT_EQ(dst.debit_instantane, src.debit_instantane, 0.01f);
    TEST_ASSERT_FLOAT_EQ(dst.volume_session,   src.volume_session,   0.1f);
    TEST_ASSERT_EQ(dst.debitmetre_ok,    src.debitmetre_ok);
    TEST_ASSERT_EQ(dst.transfert_volume_cible, src.transfert_volume_cible);
    TEST_ASSERT_FLOAT_EQ(dst.brassage_temps_restant, src.brassage_temps_restant, 0.01f);
    TEST_ASSERT_EQ(dst.etat_systeme,     src.etat_systeme);
    TEST_ASSERT_EQ(dst.securite_cuve,    src.securite_cuve);
    TEST_ASSERT_EQ(strcmp(dst.brassage_label, "MARCHE"), 0);
}

static void test_json_etat_avant_pompe_arretee(void) {
    etat_carte_avant_t src = {0};
    src.pompe = POMPE_ARRETEE;
    src.vanne_3v = V3V_BRASSAGE;

    char buf[512];
    json_serialiser_etat_avant(&src, buf, sizeof(buf));

    etat_carte_avant_t dst;
    json_deserialiser_etat_avant(buf, &dst);
    TEST_ASSERT_EQ(dst.pompe, POMPE_ARRETEE);
    TEST_ASSERT_EQ(dst.vanne_3v, V3V_BRASSAGE);
}

static void test_json_etat_avant_securite_cuve_vide(void) {
    etat_carte_avant_t src = {0};
    src.securite_cuve = SEC_CUVE_VIDE;

    char buf[512];
    json_serialiser_etat_avant(&src, buf, sizeof(buf));

    etat_carte_avant_t dst;
    json_deserialiser_etat_avant(buf, &dst);
    TEST_ASSERT_EQ(dst.securite_cuve, SEC_CUVE_VIDE);
}

static void test_json_etat_avant_brassage_label_max(void) {
    etat_carte_avant_t src = {0};
    /* Remplir brassage_label jusqu'à la limite (15 chars + \0) */
    strncpy(src.brassage_label, "123456789012345", sizeof(src.brassage_label) - 1);

    char buf[512];
    json_serialiser_etat_avant(&src, buf, sizeof(buf));

    etat_carte_avant_t dst;
    json_deserialiser_etat_avant(buf, &dst);
    TEST_ASSERT_EQ(strcmp(dst.brassage_label, src.brassage_label), 0);
}

/* ================================================================
 * TESTS : etat_carte_arriere_t round-trip
 * ================================================================ */

static void test_json_etat_arriere_roundtrip(void) {
    etat_carte_arriere_t src = {
        .vanne_2m         = VANNE_ETAT_EN_OUVERTURE,
        .vanne_bout_rampe = VANNE_ETAT_EN_FERMETURE,
        .phares_arriere   = true,
        .niveau_cuve_arriere = 67.3f,
        .sonde_niveau_ok  = true,
        .etat_systeme     = ETAT_SYS_OPERATIONNEL,
    };

    char buf[512];
    int len = json_serialiser_etat_arriere(&src, buf, sizeof(buf));
    TEST_ASSERT(len > 0);

    etat_carte_arriere_t dst;
    bool ok = json_deserialiser_etat_arriere(buf, &dst);
    TEST_ASSERT(ok);

    TEST_ASSERT_EQ(dst.vanne_2m,         src.vanne_2m);
    TEST_ASSERT_EQ(dst.vanne_bout_rampe, src.vanne_bout_rampe);
    TEST_ASSERT_EQ(dst.phares_arriere,   src.phares_arriere);
    TEST_ASSERT_FLOAT_EQ(dst.niveau_cuve_arriere, src.niveau_cuve_arriere, 0.01f);
    TEST_ASSERT_EQ(dst.sonde_niveau_ok,  src.sonde_niveau_ok);
    TEST_ASSERT_EQ(dst.etat_systeme,     src.etat_systeme);
}

static void test_json_etat_arriere_tous_etats_vannes(void) {
    etat_vanne_mot_t etats[] = {
        VANNE_ETAT_INCONNU,
        VANNE_ETAT_EN_OUVERTURE,
        VANNE_ETAT_EN_FERMETURE,
        VANNE_ETAT_ARRETEE,
        VANNE_ETAT_TIMEOUT,
    };
    char buf[256];
    for (int i = 0; i < 5; i++) {
        etat_carte_arriere_t src = {0};
        src.vanne_2m = etats[i];
        json_serialiser_etat_arriere(&src, buf, sizeof(buf));
        etat_carte_arriere_t dst;
        json_deserialiser_etat_arriere(buf, &dst);
        /* INCONNU → encodé '?' → décodé ARRETEE par défaut */
        if (etats[i] == VANNE_ETAT_INCONNU) {
            TEST_ASSERT_EQ(dst.vanne_2m, VANNE_ETAT_ARRETEE);
        } else {
            TEST_ASSERT_EQ(dst.vanne_2m, etats[i]);
        }
    }
}

/* ================================================================
 * TESTS : configuration_t round-trip
 * ================================================================ */

static void test_json_config_roundtrip(void) {
    configuration_t src = {
        .version                = 42,
        .seuil_debit_cuve_vide  = 1.2f,
        .delai_detection_ms     = 3000,
        .volume_transfert       = 120,
        .temps_brassage_on      = 600,
        .temps_brassage_off     = 300,
        .facteur_k_debitmetre   = 4.72f,
        .timeout_vanne_ms       = 30000,
        .volume_cuve_ar         = 12500,
        .sonde_hauteur_max_mm   = 2000,
        .sonde_offset_mm        = 50,
        .hauteur_cuve_mm        = 1550,
        .version_protocole      = VERSION_PROTOCOLE,
    };

    char buf[1024];
    int len = json_serialiser_configuration(&src, buf, sizeof(buf));
    TEST_ASSERT(len > 0);

    configuration_t dst = {0};
    bool ok = json_deserialiser_configuration(buf, &dst);
    TEST_ASSERT(ok);

    TEST_ASSERT_EQ(dst.version,               src.version);
    TEST_ASSERT_FLOAT_EQ(dst.seuil_debit_cuve_vide, src.seuil_debit_cuve_vide, 0.001f);
    TEST_ASSERT_EQ(dst.delai_detection_ms,    src.delai_detection_ms);
    TEST_ASSERT_EQ(dst.volume_transfert,      src.volume_transfert);
    TEST_ASSERT_EQ(dst.temps_brassage_on,     src.temps_brassage_on);
    TEST_ASSERT_EQ(dst.temps_brassage_off,    src.temps_brassage_off);
    TEST_ASSERT_FLOAT_EQ(dst.facteur_k_debitmetre, src.facteur_k_debitmetre, 0.001f);
    TEST_ASSERT_EQ(dst.timeout_vanne_ms,      src.timeout_vanne_ms);
    TEST_ASSERT_EQ(dst.volume_cuve_ar,        src.volume_cuve_ar);
    TEST_ASSERT_EQ(dst.sonde_hauteur_max_mm,  src.sonde_hauteur_max_mm);
    TEST_ASSERT_EQ(dst.sonde_offset_mm,       src.sonde_offset_mm);
    TEST_ASSERT_EQ(dst.hauteur_cuve_mm,       src.hauteur_cuve_mm);
}

static void test_json_config_facteur_k_precision(void) {
    /* 4.72 doit survivre au round-trip JSON avec précision raisonnable */
    configuration_t src = (configuration_t)CONFIG_DEFAUT;
    char buf[1024];
    json_serialiser_configuration(&src, buf, sizeof(buf));
    configuration_t dst = {0};
    json_deserialiser_configuration(buf, &dst);
    TEST_ASSERT_FLOAT_EQ(dst.facteur_k_debitmetre, 4.72f, 0.01f);
}

static void test_json_config_invalid_input(void) {
    configuration_t dst;
    bool ok = json_deserialiser_configuration("not json {{{", &dst);
    TEST_ASSERT(!ok);
}

static void test_json_etat_avant_invalid_input(void) {
    etat_carte_avant_t dst;
    bool ok = json_deserialiser_etat_avant("{invalid", &dst);
    TEST_ASSERT(!ok);
}

/* ================================================================
 * main
 * ================================================================ */
int main(void) {
    SUITE("JSON etat_carte_avant_t");
    RUN(test_json_etat_avant_roundtrip);
    RUN(test_json_etat_avant_pompe_arretee);
    RUN(test_json_etat_avant_securite_cuve_vide);
    RUN(test_json_etat_avant_brassage_label_max);

    SUITE("JSON etat_carte_arriere_t");
    RUN(test_json_etat_arriere_roundtrip);
    RUN(test_json_etat_arriere_tous_etats_vannes);

    SUITE("JSON configuration_t");
    RUN(test_json_config_roundtrip);
    RUN(test_json_config_facteur_k_precision);
    RUN(test_json_config_invalid_input);
    RUN(test_json_etat_avant_invalid_input);

    SUMMARY();
    return EXIT_CODE();
}
