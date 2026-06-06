/*
 * Tests : logique interne du broker MQTT
 *   - topic_correspond() : wildcards MQTT + / #
 *   - paquet_lire_longueur_restante() / encoder_longueur_restante()
 *
 * Stratégie : on inclut broker_mqtt.c directement après avoir fourni
 * tous les headers nécessaires, ce qui rend les fonctions static accessibles.
 */

/* ---- Mocks pour satisfaire les includes de broker_mqtt.c ---- */
#include "mocks/esp_log.h"
#include "mocks/esp_err.h"
#include "mocks/esp_timer.h"
#include "mocks/freertos/FreeRTOS.h"
#include "mocks/freertos/task.h"
#include "mocks/freertos/semphr.h"
#include "mocks/lwip/sockets.h"
#include "mocks/lwip/netdb.h"

/* Inclure le code source réel */
#include "../commun/broker_mqtt/broker_mqtt.c"

/* ---- Framework de test ---- */
#include "test_runner.h"

/* ================================================================
 * TESTS : topic_correspond()
 * ================================================================ */

static void test_topic_exact_match(void) {
    TEST_ASSERT(topic_correspond("a/b/c", "a/b/c"));
}

static void test_topic_no_match(void) {
    TEST_ASSERT(!topic_correspond("a/b/c", "a/b/d"));
    TEST_ASSERT(!topic_correspond("a/b/c", "a/b"));
    TEST_ASSERT(!topic_correspond("a/b", "a/b/c"));
}

static void test_topic_plus_one_level(void) {
    TEST_ASSERT(topic_correspond("a/+/c", "a/foo/c"));
    TEST_ASSERT(topic_correspond("a/+/c", "a/bar/c"));
    TEST_ASSERT(!topic_correspond("a/+/c", "a/foo/bar/c"));  /* + ne couvre pas plusieurs niveaux */
    TEST_ASSERT(!topic_correspond("a/+/c", "a/c"));
}

static void test_topic_plus_first_level(void) {
    TEST_ASSERT(topic_correspond("+/b", "a/b"));
    TEST_ASSERT(topic_correspond("+/b", "x/b"));
    TEST_ASSERT(!topic_correspond("+/b", "a/c"));
}

static void test_topic_hash_multilevel(void) {
    TEST_ASSERT(topic_correspond("a/#", "a/b/c/d"));
    TEST_ASSERT(topic_correspond("a/#", "a/b"));
    TEST_ASSERT(topic_correspond("a/#", "a/b/c"));
    TEST_ASSERT(!topic_correspond("a/#", "b/c"));
}

static void test_topic_hash_edge_same_level(void) {
    /* MQTT : "a/b/#" doit matcher "a/b" (cas spécial) */
    TEST_ASSERT(topic_correspond("a/b/#", "a/b"));
    TEST_ASSERT(topic_correspond("a/b/#", "a/b/c"));
}

static void test_topic_root_hash(void) {
    TEST_ASSERT(topic_correspond("#", "a/b/c"));
    TEST_ASSERT(topic_correspond("#", "anything"));
}

static void test_topic_real_pulve_topics(void) {
    /* Topics réels du système */
    TEST_ASSERT(topic_correspond("pulverisateur/configuration/#",
                                  "pulverisateur/configuration/instantane"));
    TEST_ASSERT(topic_correspond("pulverisateur/configuration/#",
                                  "pulverisateur/configuration/demande"));
    TEST_ASSERT(topic_correspond("pulverisateur/etat/#",
                                  "pulverisateur/etat/avant"));
    TEST_ASSERT(topic_correspond("pulverisateur/etat/#",
                                  "pulverisateur/etat/arriere"));
    TEST_ASSERT(!topic_correspond("pulverisateur/cmd/avant",
                                   "pulverisateur/cmd/arriere"));
}

/* ================================================================
 * TESTS : paquet_lire_longueur_restante() / encoder_longueur_restante()
 * ================================================================ */

static void test_rl_1byte_zero(void) {
    uint8_t buf[4];
    int n = encoder_longueur_restante(buf, 0);
    TEST_ASSERT_EQ(n, 1);
    TEST_ASSERT_EQ(buf[0], 0x00);

    int lus;
    int val = paquet_lire_longueur_restante(buf, 4, &lus);
    TEST_ASSERT_EQ(val, 0);
    TEST_ASSERT_EQ(lus, 1);
}

static void test_rl_1byte_max(void) {
    uint8_t buf[4];
    int n = encoder_longueur_restante(buf, 127);
    TEST_ASSERT_EQ(n, 1);
    int lus;
    int val = paquet_lire_longueur_restante(buf, 4, &lus);
    TEST_ASSERT_EQ(val, 127);
    TEST_ASSERT_EQ(lus, 1);
}

static void test_rl_2bytes(void) {
    uint8_t buf[4];
    int n = encoder_longueur_restante(buf, 128);
    TEST_ASSERT_EQ(n, 2);
    int lus;
    int val = paquet_lire_longueur_restante(buf, 4, &lus);
    TEST_ASSERT_EQ(val, 128);
    TEST_ASSERT_EQ(lus, 2);

    n = encoder_longueur_restante(buf, 16383);
    TEST_ASSERT_EQ(n, 2);
    val = paquet_lire_longueur_restante(buf, 4, &lus);
    TEST_ASSERT_EQ(val, 16383);
}

static void test_rl_3bytes(void) {
    uint8_t buf[4];
    int n = encoder_longueur_restante(buf, 16384);
    TEST_ASSERT_EQ(n, 3);
    int lus;
    int val = paquet_lire_longueur_restante(buf, 4, &lus);
    TEST_ASSERT_EQ(val, 16384);
    TEST_ASSERT_EQ(lus, 3);
}

static void test_rl_4bytes_max(void) {
    uint8_t buf[4];
    int n = encoder_longueur_restante(buf, 268435455);
    TEST_ASSERT_EQ(n, 4);
    int lus;
    int val = paquet_lire_longueur_restante(buf, 4, &lus);
    TEST_ASSERT_EQ(val, 268435455);
    TEST_ASSERT_EQ(lus, 4);
}

static void test_rl_roundtrip_serie(void) {
    int valeurs[] = { 0, 1, 127, 128, 300, 16383, 16384, 100000, 268435455 };
    int n = (int)(sizeof(valeurs) / sizeof(valeurs[0]));
    for (int i = 0; i < n; i++) {
        uint8_t buf[4];
        int enc = encoder_longueur_restante(buf, valeurs[i]);
        (void)enc;
        int lus;
        int dec = paquet_lire_longueur_restante(buf, 4, &lus);
        TEST_ASSERT_EQ(dec, valeurs[i]);
    }
}

static void test_rl_incomplete_data(void) {
    /* Buffer trop court pour décoder */
    uint8_t buf[4] = { 0x80, 0x80, 0x80, 0x80 };  /* continuation bits partout */
    int lus;
    int val = paquet_lire_longueur_restante(buf, 2, &lus);  /* seulement 2 octets dispo */
    /* Le 4ème octet manque pour terminer → doit retourner -1 */
    TEST_ASSERT_EQ(val, -1);
}

static void test_rl_malformed_5bytes(void) {
    /* 4 octets avec continuation bit → 5ème octet nécessaire → malformé */
    uint8_t buf[5] = { 0x80, 0x80, 0x80, 0x80, 0x01 };
    int lus;
    int val = paquet_lire_longueur_restante(buf, 5, &lus);
    TEST_ASSERT_EQ(val, -1);
}

/* ================================================================
 * main
 * ================================================================ */
int main(void) {
    SUITE("topic_correspond()");
    RUN(test_topic_exact_match);
    RUN(test_topic_no_match);
    RUN(test_topic_plus_one_level);
    RUN(test_topic_plus_first_level);
    RUN(test_topic_hash_multilevel);
    RUN(test_topic_hash_edge_same_level);
    RUN(test_topic_root_hash);
    RUN(test_topic_real_pulve_topics);

    SUITE("Codec longueur MQTT");
    RUN(test_rl_1byte_zero);
    RUN(test_rl_1byte_max);
    RUN(test_rl_2bytes);
    RUN(test_rl_3bytes);
    RUN(test_rl_4bytes_max);
    RUN(test_rl_roundtrip_serie);
    RUN(test_rl_incomplete_data);
    RUN(test_rl_malformed_5bytes);

    SUMMARY();
    return EXIT_CODE();
}
