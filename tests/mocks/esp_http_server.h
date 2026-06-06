#pragma once
#include "esp_err.h"
#include <stddef.h>

typedef struct { int content_len; } httpd_req_t;
typedef void *httpd_handle_t;
typedef struct {
    const char *uri;
    int         method;
    esp_err_t (*handler)(httpd_req_t *req);
    void       *user_ctx;
} httpd_uri_t;
typedef struct {
    int    server_port;
    int    max_uri_handlers;
    size_t stack_size;
} httpd_config_t;

#define HTTPD_DEFAULT_CONFIG() \
    ((httpd_config_t){.server_port = 80, .max_uri_handlers = 8, .stack_size = 4096})

#define HTTP_GET  1
#define HTTP_POST 3

#define HTTPD_400_BAD_REQUEST            400
#define HTTPD_404_NOT_FOUND              404
#define HTTPD_500_INTERNAL_SERVER_ERROR  500
#define HTTPD_SOCK_ERR_TIMEOUT           (-1)
#define HTTPD_RESP_USE_STRLEN            (-1)

static inline esp_err_t httpd_start(httpd_handle_t *h, const httpd_config_t *c)
    { (void)h; (void)c; return ESP_OK; }
static inline esp_err_t httpd_stop(httpd_handle_t h)
    { (void)h; return ESP_OK; }
static inline esp_err_t httpd_register_uri_handler(httpd_handle_t h, const httpd_uri_t *u)
    { (void)h; (void)u; return ESP_OK; }
static inline esp_err_t httpd_resp_send_err(httpd_req_t *r, int c, const char *m)
    { (void)r; (void)c; (void)m; return ESP_OK; }
static inline esp_err_t httpd_resp_send(httpd_req_t *r, const char *b, int l)
    { (void)r; (void)b; (void)l; return ESP_OK; }
static inline esp_err_t httpd_resp_set_status(httpd_req_t *r, const char *s)
    { (void)r; (void)s; return ESP_OK; }
static inline int httpd_req_recv(httpd_req_t *r, char *b, int l)
    { (void)r; (void)b; (void)l; return 0; }
