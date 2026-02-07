/**
 * @file main.c
 * @brief Point d'entrée carte écran pulvérisateur.
 *
 * Initialise l'écran (BSP), le WiFi STA, le client MQTT,
 * crée l'UI LVGL et lance une tâche de rafraîchissement.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "lvgl.h"

#include "ui_pulverisateur.h"
#include "mqtt_ecran.h"

static const char *TAG = "MAIN";

#define PULVE_SSID      "PULVE_AP"
#define PULVE_PASS      "pulve1234"
#define UI_REFRESH_MS   500

/* ====================================================================
 * WIFI STA
 * ==================================================================== */

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT  BIT0

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi déconnecté, reconnexion...");
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "IP obtenue: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = PULVE_SSID,
            .password = PULVE_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi STA démarré, connexion à %s...", PULVE_SSID);
}

/* ====================================================================
 * TÂCHE DE RAFRAÎCHISSEMENT UI
 * ==================================================================== */

static void tache_ui_refresh(void *arg)
{
    etat_systeme_t etat;

    /* Attendre la connexion WiFi avant de démarrer MQTT */
    ESP_LOGI(TAG, "Attente connexion WiFi...");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "WiFi connecté, démarrage MQTT...");
    mqtt_ecran_init();

    for (;;) {
        mqtt_ecran_get_etat(&etat);

        if (bsp_display_lock(100)) {
            ui_rafraichir(&etat);
            bsp_display_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(UI_REFRESH_MS));
    }
}

/* ====================================================================
 * APP MAIN
 * ==================================================================== */

void app_main(void)
{
    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Écran */
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    /* Créer l'UI LVGL */
    bsp_display_lock(0);
    ui_creer();
    bsp_display_unlock();

    ESP_LOGI(TAG, "UI initialisée.");

    /* WiFi */
    wifi_init_sta();

    /* Tâche de rafraîchissement */
    xTaskCreate(tache_ui_refresh, "ui_refresh", 8192, NULL, 5, NULL);
}
