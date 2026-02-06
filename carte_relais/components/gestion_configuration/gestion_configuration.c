/**
 * @file gestion_configuration.c
 * @brief Persistance de la configuration via NVS (stockage blob).
 */
#include "gestion_configuration.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <string.h>

static const char *TAG = "CONFIG";

#define NVS_NAMESPACE   "pulve_cfg"
#define NVS_KEY_CONFIG  "config"

void configuration_initialiser(void)
{
    /* NVS est déjà initialisé dans protocole_wifi.c (nvs_flash_init).
     * Ici on vérifie juste que le namespace est accessible. */
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        nvs_close(handle);
        ESP_LOGI(TAG, "Namespace NVS '%s' accessible.", NVS_NAMESPACE);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Namespace NVS '%s' n'existe pas encore.", NVS_NAMESPACE);
    } else {
        ESP_LOGW(TAG, "Erreur NVS : %s", esp_err_to_name(err));
    }
}

bool configuration_charger(configuration_t *config_out)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Impossible d'ouvrir NVS en lecture : %s", esp_err_to_name(err));
        return false;
    }

    size_t taille = sizeof(configuration_t);
    err = nvs_get_blob(handle, NVS_KEY_CONFIG, config_out, &taille);
    nvs_close(handle);

    if (err == ESP_OK && taille == sizeof(configuration_t)) {
        ESP_LOGI(TAG, "Configuration chargée (version %lu).",
                 (unsigned long)config_out->version);

        /* Vérifier la compatibilité du protocole */
        if (config_out->version_protocole != VERSION_PROTOCOLE) {
            ESP_LOGW(TAG, "Version protocole NVS (%lu) != courante (%d). "
                     "Utilisation des valeurs par défaut.",
                     (unsigned long)config_out->version_protocole, VERSION_PROTOCOLE);
            return false;
        }
        return true;
    }

    ESP_LOGW(TAG, "Pas de configuration en NVS ou taille incorrecte.");
    return false;
}

bool configuration_sauvegarder(const configuration_t *config)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Impossible d'ouvrir NVS en écriture : %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(handle, NVS_KEY_CONFIG, config, sizeof(configuration_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur écriture blob NVS : %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Configuration sauvegardée (version %lu).",
                 (unsigned long)config->version);
        return true;
    }

    ESP_LOGE(TAG, "Erreur commit NVS : %s", esp_err_to_name(err));
    return false;
}

void configuration_effacer(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Configuration NVS effacée.");
    }
}
