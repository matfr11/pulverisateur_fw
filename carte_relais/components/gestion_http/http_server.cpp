/**
 * @file http_server.cpp
 * @brief Serveur HTTP embarqué pour interface web
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include <string.h>

static const char* TAG = "HTTP";

// Déclarations des handlers (implémentés dans http_handlers.cpp)
extern "C" {
    esp_err_t http_handler_root(httpd_req_t *req);
    esp_err_t http_handler_settings(httpd_req_t *req);
    esp_err_t http_handler_status(httpd_req_t *req);
    esp_err_t http_handler_api_cmd(httpd_req_t *req);
    esp_err_t http_handler_api_save(httpd_req_t *req);
    esp_err_t http_handler_toggle(httpd_req_t *req);

    esp_err_t http_server_start(void);
    esp_err_t http_server_stop(void);
}

// Handle du serveur
static httpd_handle_t g_server = NULL;

/**
 * @brief Configuration des URI handlers
 */
static const httpd_uri_t uri_root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = http_handler_root,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_settings = {
    .uri       = "/settings",
    .method    = HTTP_GET,
    .handler   = http_handler_settings,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_status = {
    .uri       = "/status",
    .method    = HTTP_GET,
    .handler   = http_handler_status,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_api_cmd = {
    .uri       = "/api/cmd",
    .method    = HTTP_GET,
    .handler   = http_handler_api_cmd,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_api_save = {
    .uri       = "/api/save_all",
    .method    = HTTP_GET,
    .handler   = http_handler_api_save,
    .user_ctx  = NULL
};

// Raccourcis toggle
static const httpd_uri_t uri_toggle_p = {
    .uri       = "/pT",
    .method    = HTTP_GET,
    .handler   = http_handler_toggle,
    .user_ctx  = (void*)"p"
};

static const httpd_uri_t uri_toggle_v = {
    .uri       = "/vT",
    .method    = HTTP_GET,
    .handler   = http_handler_toggle,
    .user_ctx  = (void*)"v"
};

static const httpd_uri_t uri_toggle_l = {
    .uri       = "/lT",
    .method    = HTTP_GET,
    .handler   = http_handler_toggle,
    .user_ctx  = (void*)"l"
};

/**
 * @brief Démarre le serveur HTTP
 */
esp_err_t http_server_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    config.max_resp_headers = 16;
    
    ESP_LOGI(TAG, "Démarrage serveur HTTP sur port %d", config.server_port);
    
    esp_err_t ret = httpd_start(&g_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur démarrage serveur HTTP: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Enregistrer les handlers
    httpd_register_uri_handler(g_server, &uri_root);
    httpd_register_uri_handler(g_server, &uri_settings);
    httpd_register_uri_handler(g_server, &uri_status);
    httpd_register_uri_handler(g_server, &uri_api_cmd);
    httpd_register_uri_handler(g_server, &uri_api_save);
    httpd_register_uri_handler(g_server, &uri_toggle_p);
    httpd_register_uri_handler(g_server, &uri_toggle_v);
    httpd_register_uri_handler(g_server, &uri_toggle_l);
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  SERVEUR HTTP DÉMARRÉ");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  URL: http://192.168.4.1");
    ESP_LOGI(TAG, "========================================");
    
    return ESP_OK;
}

/**
 * @brief Arrête le serveur HTTP
 */
esp_err_t http_server_stop(void) {
    if (g_server) {
        ESP_LOGI(TAG, "Arrêt serveur HTTP");
        return httpd_stop(g_server);
    }
    return ESP_OK;
}
