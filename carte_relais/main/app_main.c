/**
 * @file app_main.c
 * @brief Point d'entrée du firmware carte relais.
 */
#include "app_init.h"
#include "esp_log.h"

void app_main(void)
{
    app_initialiser();
    /* La tâche principale est lancée dans app_initialiser().
     * app_main() retourne normalement – FreeRTOS gère le scheduling. */
}
