/*
 * Tests : logique OTA proxy
 *
 * Parties testables sans réseau :
 *   - Construction de l'URL http://<IP>/ota à partir d'une esp_ip4_addr_t
 *   - Mapping carte_id_t → hostname mDNS via ota_hostname_pour_carte()
 */

#include "mocks/esp_log.h"
#include "mocks/esp_err.h"
#include "mocks/esp_timer.h"
#include "mocks/board_config.h"
#include "mocks/freertos/FreeRTOS.h"
#include "mocks/freertos/task.h"
#include "mocks/esp_http_server.h"
#include "mocks/esp_ota_ops.h"
#include "mocks/esp_partition.h"

/* esp_restart stub */
static inline void esp_restart(void) {}

/* Include du code source réel */
#include "../carte_relais/components/gestion_ota/gestion_ota.c"

/* lwip IPSTR/IP2STR (suffixe lwip ou standard) */
#include <stdint.h>
#ifndef IPSTR
#define IPSTR "%u.%u.%u.%u"
#endif

typedef struct { uint32_t addr; } esp_ip4_addr_t;

#ifndef IP2STR
#define IP2STR(a) \
    (unsigned)((a)->addr & 0xff), \
    (unsigned)(((a)->addr >> 8) & 0xff), \
    (unsigned)(((a)->addr >> 16) & 0xff), \
    (unsigned)(((a)->addr >> 24) & 0xff)
#endif

#include "test_runner.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * TESTS : construction d'URL
 * ================================================================ */

static void test_url_construction_ip_192_168_4_2(void) {
    esp_ip4_addr_t addr;
    addr.addr = (192u) | (168u << 8) | (4u << 16) | (2u << 24);

    char url[48];
    snprintf(url, sizeof(url), "http://" IPSTR "/ota", IP2STR(&addr));
    TEST_ASSERT_EQ(strcmp(url, "http://192.168.4.2/ota"), 0);
}

static void test_url_construction_ip_10_0_0_1(void) {
    esp_ip4_addr_t addr;
    addr.addr = (10u) | (0u << 8) | (0u << 16) | (1u << 24);

    char url[48];
    snprintf(url, sizeof(url), "http://" IPSTR "/ota", IP2STR(&addr));
    TEST_ASSERT_EQ(strcmp(url, "http://10.0.0.1/ota"), 0);
}

/* ================================================================
 * TESTS : ota_hostname_pour_carte()
 * ================================================================ */

static void test_hostname_avant(void) {
    const char *h = ota_hostname_pour_carte(CARTE_ID_AVANT);
    TEST_ASSERT(h != NULL);
    TEST_ASSERT_EQ(strcmp(h, OTA_HOSTNAME_AVANT), 0);
    TEST_ASSERT_EQ(strcmp(h, "pulve-av"), 0);
}

static void test_hostname_arriere(void) {
    const char *h = ota_hostname_pour_carte(CARTE_ID_ARRIERE);
    TEST_ASSERT(h != NULL);
    TEST_ASSERT_EQ(strcmp(h, OTA_HOSTNAME_ARRIERE), 0);
    TEST_ASSERT_EQ(strcmp(h, "pulve-ar"), 0);
}

static void test_hostname_serveur_retourne_null(void) {
    const char *h = ota_hostname_pour_carte(CARTE_ID_SERVEUR);
    TEST_ASSERT(h == NULL);
}

static void test_hostname_constantes_distinctes(void) {
    /* Les 3 hostnames doivent être différents */
    TEST_ASSERT(strcmp(OTA_HOSTNAME_AVANT,   OTA_HOSTNAME_ARRIERE) != 0);
    TEST_ASSERT(strcmp(OTA_HOSTNAME_AVANT,   OTA_HOSTNAME_SERVEUR) != 0);
    TEST_ASSERT(strcmp(OTA_HOSTNAME_ARRIERE, OTA_HOSTNAME_SERVEUR) != 0);
}

/* ================================================================
 * main
 * ================================================================ */
int main(void) {
    SUITE("Construction URL OTA");
    RUN(test_url_construction_ip_192_168_4_2);
    RUN(test_url_construction_ip_10_0_0_1);

    SUITE("Mapping hostname mDNS");
    RUN(test_hostname_avant);
    RUN(test_hostname_arriere);
    RUN(test_hostname_serveur_retourne_null);
    RUN(test_hostname_constantes_distinctes);

    SUMMARY();
    return EXIT_CODE();
}
