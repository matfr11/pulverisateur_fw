/**
 * @file app_init.cpp
 * @brief Implémentation des fonctions d'initialisation
 * @version 1.0
 * @date 2026-02-02
 */

#include "app_init.h"
#include "board_config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include "driver/gpio.h"    // Pour toutes les fonctions et types GPIO


static const char* TAG = LOG_TAG_CONFIG;
// ============================================================================
// DÉCLARATIONS EXTERNES (fonctions qu'on appelle)
// ============================================================================

extern "C" {
    void capteurs_demarrer_tache(void);
    void automatismes_demarrer_taches(void);
    void securites_demarrer_tache(void);

    // Actionneurs (implémentés dans actionneurs_manager.cpp)
    esp_err_t actionneur_pompe_set(etat_pompe_t etat);
    etat_pompe_t actionneur_pompe_get(void);
    esp_err_t actionneur_vanne_3voies_set(position_vanne_3voies_t position);
    position_vanne_3voies_t actionneur_vanne_3voies_get(void);
    
    // WiFi (implémenté dans wifi_manager.cpp)
    esp_err_t app_init_wifi(role_carte_t role);
    // Capteurs (implémenté dans debitmetre.cpp)
    esp_err_t debitmetre_init(float facteur_k);
    
    // Automatismes (implémentés dans transfert.cpp, brassage.cpp)
    esp_err_t transfert_init(void);
    esp_err_t brassage_init(void);
    
    // Sécurités (implémenté dans detection_cuve_vide.cpp)
    esp_err_t detection_cuve_vide_init(void);
}
// ============================================================================
// IMPLÉMENTATIONS (fonctions qu'on exporte)
// ============================================================================

extern "C" {

void app_demarrer_taches_capteurs(void) {
#if CAPACITE_DEBITMETRE
    capteurs_demarrer_tache();
#endif
}

void app_demarrer_taches_automatismes(void) {
#if CAPACITE_AUTOMATISMES
    // Initialiser les modules avant de démarrer les tâches
    esp_err_t transfert_init(void);
    esp_err_t brassage_init(void);
    
    transfert_init();
    brassage_init();
    automatismes_demarrer_taches();
#endif
}

void app_demarrer_taches_securites(void) {
#if CAPACITE_AUTOMATISMES
    // Initialiser le module sécurités
    esp_err_t detection_cuve_vide_init(void);
    
    detection_cuve_vide_init();
    
    securites_demarrer_tache();
#endif
}


} // extern "C"
// ============================================================================
// CONFIGURATION - NVS
// ============================================================================

/**
 * @brief Initialise la configuration depuis NVS
 */
esp_err_t app_init_configuration(configuration_systeme_t* config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Ouvrir le namespace de configuration
    err = nvs_open(NVS_NAMESPACE_CONFIG, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Impossible d'ouvrir NVS config: %s", esp_err_to_name(err));
        ESP_LOGI(TAG, "Utilisation de la configuration par défaut");
        app_init_configuration_defaut(config);
        return ESP_ERR_NOT_FOUND;
    }

    // Lire la version de configuration
    uint32_t version = 0;
    err = nvs_get_u32(nvs_handle, NVS_KEY_CONFIG_VERSION, &version);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Version config non trouvée");
        nvs_close(nvs_handle);
        app_init_configuration_defaut(config);
        return ESP_ERR_NOT_FOUND;
    }

    // Lire les données de configuration
    size_t required_size = sizeof(configuration_systeme_t);
    err = nvs_get_blob(nvs_handle, NVS_KEY_CONFIG_DATA, config, &required_size);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur lecture config: %s", esp_err_to_name(err));
        app_init_configuration_defaut(config);
        return err;
    }

    ESP_LOGI(TAG, "Configuration chargée (version %lu)", version);
    return ESP_OK;
}

/**
 * @brief Initialise la configuration avec valeurs par défaut
 */
void app_init_configuration_defaut(configuration_systeme_t* config) {
    if (config == NULL) return;

    memset(config, 0, sizeof(configuration_systeme_t));

    config->version_config = 1;

    // Sécurités
    config->securite.seuil_debit_cuve_vide = CONFIG_DEFAULT_SEUIL_DEBIT_VIDE;
    config->securite.delai_detection_ms = CONFIG_DEFAULT_DELAI_DETECTION_VIDE_MS;
    config->securite.timeout_vanne_3fils_ms = TIMEOUT_VANNE_3FILS_DEFAUT_MS;

    // Automatismes
    config->automatismes.volume_transfert_litres = CONFIG_DEFAULT_VOLUME_TRANSFERT_L;
    config->automatismes.temps_brassage_on_sec = CONFIG_DEFAULT_TEMPS_BRASSAGE_ON_SEC;
    config->automatismes.temps_brassage_pause_sec = CONFIG_DEFAULT_TEMPS_BRASSAGE_PAUSE_SEC;
    config->automatismes.seuil_niveau_bas_litres = 50.0f;
    config->automatismes.volume_cible_transfert_litres = 100.0f;

    // Capteurs
    config->capteurs.facteur_k_debitmetre = CONFIG_DEFAULT_FACTEUR_K_DEBITMETRE;
    config->capteurs.sonde_niveau_disponible = false;

    // Actionneurs
    config->actionneurs.phares_auto = false;

    ESP_LOGI(TAG, "Configuration par défaut initialisée");
}

/**
 * @brief Sauvegarde la configuration dans NVS
 */
esp_err_t app_save_configuration(const configuration_systeme_t* config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Ouvrir le namespace en lecture/écriture
    err = nvs_open(NVS_NAMESPACE_CONFIG, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur ouverture NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Sauvegarder la version
    err = nvs_set_u32(nvs_handle, NVS_KEY_CONFIG_VERSION, config->version_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur sauvegarde version: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Sauvegarder les données
    err = nvs_set_blob(nvs_handle, NVS_KEY_CONFIG_DATA, config, sizeof(configuration_systeme_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur sauvegarde données: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Valider les changements
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Configuration sauvegardée (version %lu)", config->version_config);
    } else {
        ESP_LOGE(TAG, "Erreur commit NVS: %s", esp_err_to_name(err));
    }

    return err;
}

// ============================================================================
// ACTIONNEURS - INITIALISATION GPIO
// ============================================================================

/**
 * @brief Initialise les GPIO des actionneurs
 */
esp_err_t app_init_actionneurs(void) {
    ESP_LOGI(TAG, "Initialisation des actionneurs...");

#ifdef BOARD_TYPE_AVANT
    // Configuration GPIO pour carte AVANT
    gpio_config_t io_conf = {};
    
    // Relais en sortie
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_RELAIS_POMPE) |
                           (1ULL << GPIO_RELAIS_VANNE_3VOIES) |
                           (1ULL << GPIO_RELAIS_PHARES_AVANT) |
                           (1ULL << GPIO_RELAIS_RESERVE);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    // État initial: tout à l'arrêt
    gpio_set_level(GPIO_RELAIS_POMPE, 0);
    gpio_set_level(GPIO_RELAIS_VANNE_3VOIES, 0);  // Brassage
    gpio_set_level(GPIO_RELAIS_PHARES_AVANT, 0);
    gpio_set_level(GPIO_RELAIS_RESERVE, 0);

    ESP_LOGI(TAG, "✓ GPIO carte AVANT configurés");

#elif defined(BOARD_TYPE_ARRIERE)
    // Configuration GPIO pour carte ARRIÈRE
    gpio_config_t io_conf = {};
    
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_VANNE_2M_OUVRIR) |
                           (1ULL << GPIO_VANNE_2M_FERMER) |
                           (1ULL << GPIO_VANNE_BOUT_OUVRIR) |
                           (1ULL << GPIO_VANNE_BOUT_FERMER) |
                           (1ULL << GPIO_RELAIS_PHARES_ARRIERE) |
                           (1ULL << GPIO_RELAIS_RESERVE_1) |
                           (1ULL << GPIO_RELAIS_RESERVE_2) |
                           (1ULL << GPIO_RELAIS_RESERVE_3);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    // État initial: toutes vannes inactives (SÉCURITÉ CRITIQUE)
    gpio_set_level(GPIO_VANNE_2M_OUVRIR, 0);
    gpio_set_level(GPIO_VANNE_2M_FERMER, 0);
    gpio_set_level(GPIO_VANNE_BOUT_OUVRIR, 0);
    gpio_set_level(GPIO_VANNE_BOUT_FERMER, 0);
    gpio_set_level(GPIO_RELAIS_PHARES_ARRIERE, 0);
    gpio_set_level(GPIO_RELAIS_RESERVE_1, 0);
    gpio_set_level(GPIO_RELAIS_RESERVE_2, 0);
    gpio_set_level(GPIO_RELAIS_RESERVE_3, 0);

    ESP_LOGI(TAG, "✓ GPIO carte ARRIÈRE configurés");
#endif

    return ESP_OK;
}

/**
 * @brief Démarre la tâche de gestion des actionneurs
 */
void app_demarrer_taches_actionneurs(void) {
    // Les actionneurs n'ont peut-être pas de tâche dédiée
    // Ils sont gérés via MQTT callbacks
    ESP_LOGI(TAG, "Actionneurs prêts (gérés via MQTT)");
}

// ============================================================================
// CAPTEURS - INITIALISATION
// ============================================================================

/**
 * @brief Initialise les capteurs
 */
esp_err_t app_init_capteurs(void) {
#if CAPACITE_DEBITMETRE
    ESP_LOGI(TAG, "Initialisation des capteurs...");
    
    // Configuration GPIO débitmètre (entrée avec interruption)
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_DEBITMETRE_IMPULSION);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = (DEBITMETRE_PULL_MODE == GPIO_PULLUP_ONLY) ? 
                         GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config GPIO débitmètre: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialiser le module débitmètre (mutex, variables)
    esp_err_t debitmetre_init(float facteur_k);
    ret = debitmetre_init(CONFIG_DEFAULT_FACTEUR_K_DEBITMETRE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init débitmètre: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✓ Débitmètre configuré (GPIO %d)", GPIO_DEBITMETRE_IMPULSION);
    return ESP_OK;
#else
    ESP_LOGI(TAG, "Pas de capteurs sur cette carte");
    return ESP_OK;
#endif
}