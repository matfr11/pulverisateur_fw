/**
 * @file app_main.c
 * @brief Point d'entrée du firmware carte relais.
 */
#include "app_init.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

void app_main(void)
{
    /* Valider le firmware OTA (empêche le rollback automatique) */
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI("OTA", "Premier boot après OTA, validation du firmware.");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

    app_initialiser();
    /* La tâche principale est lancée dans app_initialiser().
     * app_main() retourne normalement – FreeRTOS gère le scheduling. */
}
