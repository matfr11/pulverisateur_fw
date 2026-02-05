/**
 * @file http_handlers.cpp
 * @brief Gestionnaires des requêtes HTTP
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "mqtt_topics.h"
#include "configuration.h"
#include <string.h>
#include <stdlib.h>

#include "ui_main.h"
#include "ui_settings.h"

extern char* http_get_status_json(void);
static const char* TAG_HANDLERS = "HTTP_HANDLERS";

// Déclarations externes
extern "C" {
    // Commandes MQTT
    extern esp_err_t mqtt_publish(const char* topic, const char* payload, int qos, bool retain);
}

/**
 * @brief Parser les query params
 */
static esp_err_t get_query_param(httpd_req_t *req, const char* key, char* value, size_t max_len) {
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len <= 1) {
        return ESP_ERR_NOT_FOUND;
    }
    
    char* buf = (char*)malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
        if (httpd_query_key_value(buf, key, value, max_len) == ESP_OK) {
            free(buf);
            return ESP_OK;
        }
    }
    free(buf);
    return ESP_ERR_NOT_FOUND;
}

extern "C" {

/**
 * @brief Handler: Page principale /
 */
esp_err_t http_handler_root(httpd_req_t *req) {
    ESP_LOGI(TAG_HANDLERS, "GET /");
    
    const char* html = http_get_main_page();
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_sendstr(req, html);
}

/**
 * @brief Handler: Page configuration /settings
 */
esp_err_t http_handler_settings(httpd_req_t *req) {
    ESP_LOGI(TAG_HANDLERS, "GET /settings");
    
    const char* html = http_get_settings_page();
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_sendstr(req, html);
}

/**
 * @brief Handler: État système JSON /status
 */
esp_err_t http_handler_status(httpd_req_t *req) {
    char* json = http_get_status_json();
    
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur génération JSON");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    
    esp_err_t ret = httpd_resp_sendstr(req, json);
    free(json);
    
    return ret;
}

/**
 * @brief Handler: Commandes API /api/cmd?t=XX&a=XX
 */
esp_err_t http_handler_api_cmd(httpd_req_t *req) {
    char type[16] = {0};
    char action[16] = {0};
    
    // Parser les paramètres
    if (get_query_param(req, "t", type, sizeof(type)) != ESP_OK ||
        get_query_param(req, "a", action, sizeof(action)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Paramètres manquants");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG_HANDLERS, "API CMD: type=%s action=%s", type, action);
    
    // POMPE
    if (strcmp(type, "p") == 0) {
        const char* topic = TOPIC_CMD_POMPE;
        const char* payload = (action[0] == 'T') ? "{\"action\":\"TOGGLE\"}" :
                              (action[0] == '1' || action[0] == 'O') ? "{\"action\":\"ON\"}" :
                              "{\"action\":\"OFF\"}";
        mqtt_publish(topic, payload, 1, false);
    }
    // VANNE 3 VOIES
    else if (strcmp(type, "v") == 0) {
        const char* topic = TOPIC_CMD_VANNE_3VOIES;
        const char* payload = (action[0] == 'T') ? "{\"action\":\"TOGGLE\"}" :
                              (action[0] == 'B') ? "{\"action\":\"BRASSAGE\"}" :
                              "{\"action\":\"TRANSFERT\"}";
        mqtt_publish(topic, payload, 1, false);
    }
    // PHARES AVANT
    else if (strcmp(type, "l") == 0) {
        mqtt_publish(TOPIC_CMD_PHARES_AVANT, "{\"action\":\"TOGGLE\"}", 1, false);
    }
    // PHARES ARRIÈRE
    else if (strcmp(type, "li") == 0) {
        mqtt_publish(TOPIC_CMD_PHARES_ARRIERE, "{\"action\":\"TOGGLE\"}", 1, false);
    }
    // VANNE 2M
    else if (strcmp(type, "v2") == 0) {
        char payload[64];
        if (action[0] == 'O') {
            snprintf(payload, sizeof(payload), "{\"action\":\"OUVRIR\"}");
        } else if (action[0] == 'F') {
            snprintf(payload, sizeof(payload), "{\"action\":\"FERMER\"}");
        } else {
            snprintf(payload, sizeof(payload), "{\"action\":\"STOP\"}");
        }
        mqtt_publish(TOPIC_CMD_VANNE_2M, payload, 1, false);
    }
    // VANNE BOUT RAMPE
    else if (strcmp(type, "vb") == 0) {
        char payload[64];
        if (action[0] == 'O') {
            snprintf(payload, sizeof(payload), "{\"action\":\"OUVRIR\"}");
        } else if (action[0] == 'F') {
            snprintf(payload, sizeof(payload), "{\"action\":\"FERMER\"}");
        } else {
            snprintf(payload, sizeof(payload), "{\"action\":\"STOP\"}");
        }
        mqtt_publish(TOPIC_CMD_VANNE_BOUT_RAMPE, payload, 1, false);
    }
    // TRANSFERT
    else if (strcmp(type, "tr") == 0) {
        const char* topic = (action[0] == 'A') ? TOPIC_CMD_TRANSFERT_ACTIVER : 
                                                  TOPIC_CMD_TRANSFERT_DESACTIVER;
        mqtt_publish(topic, "{}", 1, false);
    }
    // BRASSAGE
    else if (strcmp(type, "br") == 0) {
        const char* topic = (action[0] == 'A') ? TOPIC_CMD_BRASSAGE_ACTIVER : 
                                                  TOPIC_CMD_BRASSAGE_DESACTIVER;
        mqtt_publish(topic, "{}", 1, false);
    }
    // ARRÊT D'URGENCE
    else if (strcmp(type, "id") == 0) {
        ESP_LOGW(TAG_HANDLERS, "ARRÊT D'URGENCE demandé");
        mqtt_publish(TOPIC_CMD_TRANSFERT_DESACTIVER, "{}", 1, false);
        mqtt_publish(TOPIC_CMD_BRASSAGE_DESACTIVER, "{}", 1, false);
    }
    
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, "OK");
}

/**
 * @brief Handler: Sauvegarde configuration /api/save_all
 */
esp_err_t http_handler_api_save(httpd_req_t *req) {
    ESP_LOGI(TAG_HANDLERS, "API SAVE CONFIG");
    
    // Charger configuration actuelle
    configuration_systeme_t config;
    if (configuration_get(&config) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur lecture config");
        return ESP_FAIL;
    }
    
    char buf[32];
    
    // Volume transfert
    if (get_query_param(req, "t_tgt", buf, sizeof(buf)) == ESP_OK) {
        config.automatismes.volume_transfert_litres = atof(buf);
        ESP_LOGI(TAG_HANDLERS, "  volume_transfert = %.1f L", config.automatismes.volume_transfert_litres);
    }
    
    // Facteur K
    if (get_query_param(req, "k_fact", buf, sizeof(buf)) == ESP_OK) {
        config.capteurs.facteur_k_debitmetre = atof(buf);
        ESP_LOGI(TAG_HANDLERS, "  facteur_k = %.2f", config.capteurs.facteur_k_debitmetre);
    }
    
    // Seuil débit vide
    if (get_query_param(req, "e_flow", buf, sizeof(buf)) == ESP_OK) {
        config.securite.seuil_debit_cuve_vide = atof(buf);
        ESP_LOGI(TAG_HANDLERS, "  seuil_debit = %.1f L/min", config.securite.seuil_debit_cuve_vide);
    }
    
    // Délai détection (sec → ms)
    if (get_query_param(req, "e_out", buf, sizeof(buf)) == ESP_OK) {
        config.securite.delai_detection_ms = atoi(buf) * 1000;
        ESP_LOGI(TAG_HANDLERS, "  delai_detection = %lu ms", config.securite.delai_detection_ms);
    }
    
    // Timeout vannes (sec → ms)
    if (get_query_param(req, "v_timeout", buf, sizeof(buf)) == ESP_OK) {
        config.securite.timeout_vanne_3fils_ms = atoi(buf) * 1000;
        ESP_LOGI(TAG_HANDLERS, "  timeout_vannes = %lu ms", config.securite.timeout_vanne_3fils_ms);
    }
    
    // Brassage ON (min → sec)
    if (get_query_param(req, "br_on", buf, sizeof(buf)) == ESP_OK) {
        config.automatismes.temps_brassage_on_sec = atoi(buf) * 60;
        ESP_LOGI(TAG_HANDLERS, "  brassage_on = %lu sec", config.automatismes.temps_brassage_on_sec);
    }
    
    // Brassage OFF (min → sec)
    if (get_query_param(req, "br_off", buf, sizeof(buf)) == ESP_OK) {
        config.automatismes.temps_brassage_pause_sec = atoi(buf) * 60;
        ESP_LOGI(TAG_HANDLERS, "  brassage_pause = %lu sec", config.automatismes.temps_brassage_pause_sec);
    }
    
    // Sauvegarder (la version sera auto-incrémentée)
    esp_err_t ret = configuration_set(&config);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur sauvegarde");
        return ESP_FAIL;
    }
    
    // Publier sur MQTT
    configuration_publier(&config);
    
    ESP_LOGI(TAG_HANDLERS, "Configuration sauvegardée et publiée (version %lu)", 
             configuration_get_version());
    
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, "OK");
}

/**
 * @brief Handler: Raccourcis toggle /pT, /vT, /lT
 */
esp_err_t http_handler_toggle(httpd_req_t *req) {
    const char* type = (const char*)req->user_ctx;
    
    ESP_LOGI(TAG_HANDLERS, "TOGGLE: %s", type);
    
    if (strcmp(type, "p") == 0) {
        mqtt_publish(TOPIC_CMD_POMPE, "{\"action\":\"TOGGLE\"}", 1, false);
    } else if (strcmp(type, "v") == 0) {
        mqtt_publish(TOPIC_CMD_VANNE_3VOIES, "{\"action\":\"TOGGLE\"}", 1, false);
    } else if (strcmp(type, "l") == 0) {
        mqtt_publish(TOPIC_CMD_PHARES_AVANT, "{\"action\":\"TOGGLE\"}", 1, false);
    }
    
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, "OK");
}

} // extern "C"