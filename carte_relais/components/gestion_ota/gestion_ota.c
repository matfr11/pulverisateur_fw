/**
 * @file gestion_ota.c
 * @brief Mini serveur HTTP OTA pour les cartes relais (AVANT / ARRIÈRE).
 *
 * Expose un unique endpoint POST /ota sur le port 80.
 * Le serveur (CARTE_SERVEUR) y envoie le firmware en streaming via
 * proxy_ota_vers_relais() — jamais plus de 4 KB en RAM simultanément.
 *
 * Compilé pour toutes les cartes mais actif seulement sur les relais
 * (#if !A_EST_SERVEUR dans app_init.c).
 */
#include "gestion_ota.h"
#include "board_config.h"
#include <stdlib.h>
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "OTA_SRV";
static httpd_handle_t s_serveur_ota = NULL;

const char *ota_hostname_pour_carte(carte_id_t id)
{
    switch (id) {
        case CARTE_ID_AVANT:   return OTA_HOSTNAME_AVANT;
        case CARTE_ID_ARRIERE: return OTA_HOSTNAME_ARRIERE;
        default: return NULL;
    }
}

static esp_err_t handler_ota(httpd_req_t *req)
{
    ESP_LOGI(TAG, "OTA : réception firmware (%d octets)...", req->content_len);

    if (req->content_len == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content-Length manquant");
        return ESP_FAIL;
    }

    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    if (!update) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Pas de partition OTA");
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    char *buf = malloc(4096);
    if (!buf) {
        esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Malloc failed");
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    int received_total = 0;

    while (remaining > 0) {
        int received = httpd_req_recv(req, buf, (remaining < 4096) ? remaining : 4096);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ESP_LOGE(TAG, "OTA : erreur réception");
            free(buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv error");
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA : erreur écriture");
            free(buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write error");
            return ESP_FAIL;
        }

        remaining -= received;
        received_total += received;

        if (received_total % 65536 < 4096) {
            ESP_LOGI(TAG, "OTA : %d / %d octets", received_total, req->content_len);
        }
    }

    free(buf);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA : validation échouée: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA OK ! Redémarrage dans 2s...");
    httpd_resp_send(req, "OK", 2);

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

esp_err_t ota_serveur_demarrer(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port     = 80;
    cfg.max_uri_handlers = 1;
    cfg.stack_size      = 8192;

    if (httpd_start(&s_serveur_ota, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Échec démarrage serveur OTA");
        return ESP_FAIL;
    }

    httpd_uri_t uri = {
        .uri     = "/ota",
        .method  = HTTP_POST,
        .handler = handler_ota,
    };
    httpd_register_uri_handler(s_serveur_ota, &uri);

    ESP_LOGI(TAG, "Serveur OTA démarré (POST /ota port 80)");
    return ESP_OK;
}

void ota_serveur_arreter(void)
{
    if (s_serveur_ota) {
        httpd_stop(s_serveur_ota);
        s_serveur_ota = NULL;
    }
}
