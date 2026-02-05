/**
 * @file app_main.cpp
 * @brief Point d'entrée principal de l'application
 * 
 * Responsabilités:
 * - Initialisation du système
 * - Démarrage des tâches
 * - Gestion du rôle Master/Slave
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "board_config.h"
#include "types_communs.h"
#include "app_init.h"
#include "app_role.h"

#include "esp_timer.h"
#include "esp_netif.h"    // Pour esp_netif_init()
#include "esp_event.h"    // Pour esp_event_loop_create_default()

#include "http_server.h"
#include "configuration.h"

extern "C" {
    esp_err_t wifi_start_ap(void);
    esp_err_t wifi_start_sta(void);
}

// ============================================================================
// CONSTANTES ET VARIABLES GLOBALES
// ============================================================================

static const char* TAG = LOG_TAG_MAIN;

// État global du système
static etat_complet_systeme_t g_etat_systeme = {};

// Event group pour synchronisation
static EventGroupHandle_t g_event_group_systeme = NULL;

// Bits d'événements
#define BIT_WIFI_PRET           (1 << 0)
#define BIT_MQTT_PRET           (1 << 1)
#define BIT_CAPTEURS_PRETS      (1 << 2)
#define BIT_ACTIONNEURS_PRETS   (1 << 3)
#define BIT_CONFIG_CHARGEE      (1 << 4)
#define BIT_SYSTEME_ACTIF       (1 << 5)

// ============================================================================
// DÉCLARATIONS FORWARD
// ============================================================================

extern "C" {
    void app_main(void);
}

static void tache_supervision_systeme(void* pvParameters);
static void tache_heartbeat(void* pvParameters);
static void initialiser_etat_systeme(void);
static void afficher_informations_demarrage(void);


// ============================================================================
// FONCTION PRINCIPALE
// ============================================================================

// ============================================================================
// FONCTION PRINCIPALE
// ============================================================================

/**
 * @brief Point d'entrée de l'application ESP-IDF
 */
void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  SYSTEME PULVERISATEUR AGRICOLE");
    ESP_LOGI(TAG, "  Carte: %s", IDENTIFIANT_CARTE);
    ESP_LOGI(TAG, "  Protocole: %d.%d.%d", 
              VERSION_PROTOCOLE_MAJEURE, 
              VERSION_PROTOCOLE_MINEURE, 
              VERSION_PROTOCOLE_PATCH);
#if MODE_SIMULATION
    ESP_LOGW(TAG, "  MODE SIMULATION ACTIF");
#endif
    ESP_LOGI(TAG, "========================================");

    // --- PHASE 1: Initialisation du matériel ---
    ESP_LOGI(TAG, "Phase 1: Initialisation matérielle");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    g_event_group_systeme = xEventGroupCreate();
    initialiser_etat_systeme();
    
    // --- PHASE 2: Initialisation des composants ---
    ESP_LOGI(TAG, "Phase 2: Initialisation composants");
    
    if (app_init_configuration(&g_etat_systeme.config) != ESP_OK) {
        ESP_LOGE(TAG, "Erreur initialisation configuration");
        app_init_configuration_defaut(&g_etat_systeme.config);
    }
    xEventGroupSetBits(g_event_group_systeme, BIT_CONFIG_CHARGEE);

    if (app_init_actionneurs() != ESP_OK) {
        ESP_LOGE(TAG, "Erreur initialisation actionneurs");
        g_etat_systeme.etat_systeme = ETAT_SYSTEME_ERREUR_CRITIQUE;
        return;
    }
    xEventGroupSetBits(g_event_group_systeme, BIT_ACTIONNEURS_PRETS);

#if CAPACITE_DEBITMETRE
    if (app_init_capteurs() != ESP_OK) {
        ESP_LOGE(TAG, "Erreur initialisation capteurs");
    } else {
        xEventGroupSetBits(g_event_group_systeme, BIT_CAPTEURS_PRETS);
    }
#endif

    // --- PHASE 3: Détection du rôle ---
    ESP_LOGI(TAG, "Phase 3: Détection du rôle");
    role_carte_t role_detecte = app_role_determiner_role();
    g_etat_systeme.role = role_detecte;
    
    // --- PHASE 4: Initialisation WiFi et MQTT ---
    ESP_LOGI(TAG, "Phase 4: Initialisation réseau");

    // ICI : On se contente d'appeler la fonction définie plus haut
    if (app_init_wifi(role_detecte) != ESP_OK) {
        ESP_LOGE(TAG, "Erreur initialisation WiFi");
        g_etat_systeme.etat_systeme = ETAT_SYSTEME_ERREUR_CRITIQUE;
        return;
    }

    xEventGroupSetBits(g_event_group_systeme, BIT_WIFI_PRET);

    if (app_init_mqtt(role_detecte) != ESP_OK) {
        ESP_LOGE(TAG, "Erreur initialisation MQTT");
        return;
    }
    xEventGroupSetBits(g_event_group_systeme, BIT_MQTT_PRET);


    // ========================================================================
    // PHASE 5: Démarrage des tâches applicatives
    // ========================================================================
    
    ESP_LOGI(TAG, "Phase 5: Démarrage des tâches");

    // Attendre que WiFi et MQTT soient prêts
    EventBits_t bits = xEventGroupWaitBits(
        g_event_group_systeme,
        BIT_WIFI_PRET | BIT_MQTT_PRET,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(30000)
    );

    if ((bits & (BIT_WIFI_PRET | BIT_MQTT_PRET)) != (BIT_WIFI_PRET | BIT_MQTT_PRET)) {
        ESP_LOGE(TAG, "Timeout attente WiFi/MQTT");
        g_etat_systeme.etat_systeme = ETAT_SYSTEME_ERREUR_CRITIQUE;
        return;
    }

    // Tâche de supervision générale
    xTaskCreate(
        tache_supervision_systeme,
        "supervision",
        TASK_STACK_UI,
        NULL,
        TASK_PRIORITY_UI,
        NULL
    );

    // Tâche heartbeat (présence)
    xTaskCreate(
        tache_heartbeat,
        "heartbeat",
        2048,
        NULL,
        1,
        NULL
    );

    // Démarrer les tâches des composants métier
    app_demarrer_taches_actionneurs();
    app_demarrer_taches_capteurs();
    
#if CAPACITE_AUTOMATISMES
    app_demarrer_taches_automatismes();
    app_demarrer_taches_securites();
#endif

    // ========================================================================
    // PHASE 6: Synchronisation avec le système
    // ========================================================================
    
    ESP_LOGI(TAG, "Phase 6: Synchronisation système");
    
    // Demander un instantané de configuration au master
    app_mqtt_demander_configuration();
    
    // Publier notre présence
    app_mqtt_publier_presence(true);
    
    // Marquer le système comme actif
    g_etat_systeme.etat_systeme = ETAT_SYSTEME_ACTIF;
    xEventGroupSetBits(g_event_group_systeme, BIT_SYSTEME_ACTIF);
 
    // === PHASE 7: Configuration ===
    ESP_LOGI(TAG, "=== PHASE 7: Configuration ===");
    if (configuration_init() != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init configuration");
        return;
    }
    ESP_LOGI(TAG, "✅ Configuration chargée (version %lu)", 
             configuration_get_version());
    
    // === PHASE 8: Serveur HTTP ===
    ESP_LOGI(TAG, "=== PHASE 8: Serveur HTTP ===");
    if (http_server_start() != ESP_OK) {
        ESP_LOGE(TAG, "Erreur démarrage serveur HTTP");
    } else {
        ESP_LOGI(TAG, "✅ Interface web disponible");
        ESP_LOGI(TAG, "   URL: http://192.168.4.1");
    }
    
    //     
    // ========================================================================
    // DÉMARRAGE TERMINÉ
    // ========================================================================
    
    afficher_informations_demarrage();
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  SYSTEME OPERATIONNEL");
    ESP_LOGI(TAG, "========================================");

    // La tâche main peut se terminer, les tâches créées continuent
}

// ============================================================================
// TÂCHES SYSTÈME
// ============================================================================

/**
 * @brief Tâche de supervision du système
 * 
 * Surveille l'état général, gère les erreurs, publie les diagnostics
 */
static void tache_supervision_systeme(void* pvParameters) {
    (void)pvParameters;
    
    TickType_t derniere_execution = xTaskGetTickCount();
    const TickType_t periode = pdMS_TO_TICKS(PERIODE_PUBLICATION_ETAT);
    
    ESP_LOGI(TAG, "Tâche supervision démarrée");
    
    while (1) {
        vTaskDelayUntil(&derniere_execution, periode);
        
        // Mettre à jour uptime
        g_etat_systeme.uptime_ms = esp_timer_get_time() / 1000;
        
        // Vérifier état WiFi
        g_etat_systeme.diagnostics.wifi_connecte = app_wifi_est_connecte();
        
        // Vérifier état MQTT
        g_etat_systeme.diagnostics.mqtt_connecte = app_mqtt_est_connecte();
        
        // Publier l'état du système
        app_mqtt_publier_etat_systeme(&g_etat_systeme);
        
        // Vérifier les erreurs critiques
        if (!g_etat_systeme.diagnostics.mqtt_connecte) {
            ESP_LOGW(TAG, "MQTT déconnecté - tentative de reconnexion");
            app_mqtt_reconnect();
        }
    }
}

/**
 * @brief Tâche heartbeat - signale la présence de la carte
 */
static void tache_heartbeat(void* pvParameters) {
    (void)pvParameters;
    
    TickType_t derniere_execution = xTaskGetTickCount();
    const TickType_t periode = pdMS_TO_TICKS(PERIODE_HEARTBEAT);
    
    ESP_LOGI(TAG, "Tâche heartbeat démarrée");
    
    while (1) {
        vTaskDelayUntil(&derniere_execution, periode);
        
        // Publier présence
        app_mqtt_publier_presence(true);
    }
}

// ============================================================================
// FONCTIONS UTILITAIRES
// ============================================================================

/**
 * @brief Initialise la structure d'état système avec valeurs par défaut
 */
static void initialiser_etat_systeme(void) {
    memset(&g_etat_systeme, 0, sizeof(etat_complet_systeme_t));
    
    // Identité
    g_etat_systeme.type_carte = TYPE_CARTE;
    strncpy(g_etat_systeme.identifiant, IDENTIFIANT_CARTE, 
            sizeof(g_etat_systeme.identifiant) - 1);
    
    // État initial
    g_etat_systeme.etat_systeme = ETAT_SYSTEME_INITIALISATION;
    g_etat_systeme.role = ROLE_INCONNU;
    
    // Actionneurs au repos
#ifdef BOARD_TYPE_AVANT
    g_etat_systeme.actionneurs_avant.pompe = ETAT_POMPE_ARRET;
    g_etat_systeme.actionneurs_avant.vanne_3voies = POSITION_VANNE_3V_BRASSAGE;
    g_etat_systeme.actionneurs_avant.phares_avant = false;
#endif

#ifdef BOARD_TYPE_ARRIERE
    g_etat_systeme.actionneurs_arriere.vanne_2m = ETAT_VANNE_3FILS_INACTIF;
    g_etat_systeme.actionneurs_arriere.vanne_bout_rampe = ETAT_VANNE_3FILS_INACTIF;
    g_etat_systeme.actionneurs_arriere.phares_arriere = false;
#endif

    // Automatismes inactifs
    g_etat_systeme.automatismes.mode_transfert = MODE_TRANSFERT_INACTIF;
    g_etat_systeme.automatismes.etat_transfert = ETAT_TRANSFERT_INACTIF;
    g_etat_systeme.automatismes.etat_brassage = ETAT_BRASSAGE_INACTIF;
    g_etat_systeme.automatismes.etat_cuve_avant = ETAT_CUVE_AVANT_NORMALE;
}

/**
 * @brief Affiche les informations de démarrage
 */
static void afficher_informations_demarrage(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Configuration active:");
    ESP_LOGI(TAG, "  - Seuil débit cuve vide: %.2f L/min", 
             g_etat_systeme.config.securite.seuil_debit_cuve_vide);
    ESP_LOGI(TAG, "  - Délai détection: %lu ms", 
             g_etat_systeme.config.securite.delai_detection_ms);
    ESP_LOGI(TAG, "  - Facteur K débitmètre: %.2f imp/L", 
             g_etat_systeme.config.capteurs.facteur_k_debitmetre);
    
#if CAPACITE_AUTOMATISMES
    ESP_LOGI(TAG, "  - Volume transfert: %.1f L", 
             g_etat_systeme.config.automatismes.volume_transfert_litres);
    ESP_LOGI(TAG, "  - Brassage ON: %lu s / PAUSE: %lu s",
             g_etat_systeme.config.automatismes.temps_brassage_on_sec,
             g_etat_systeme.config.automatismes.temps_brassage_pause_sec);
#endif
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Capacités de cette carte:");
#if CAPACITE_POMPE
    ESP_LOGI(TAG, "  ✓ Pompe");
#endif
#if CAPACITE_VANNE_3VOIES
    ESP_LOGI(TAG, "  ✓ Vanne 3 voies");
#endif
#if CAPACITE_DEBITMETRE
    ESP_LOGI(TAG, "  ✓ Débitmètre");
#endif
#if CAPACITE_AUTOMATISMES
    ESP_LOGI(TAG, "  ✓ Automatismes (transfert, brassage)");
#endif
#if CAPACITE_VANNES_3FILS
    ESP_LOGI(TAG, "  ✓ Vannes 3 fils (2m, bout de rampe)");
#endif
    ESP_LOGI(TAG, "");
    
    // Informations mémoire
    ESP_LOGI(TAG, "Mémoire:");
    ESP_LOGI(TAG, "  - Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "  - Min free heap: %lu bytes", esp_get_minimum_free_heap_size());
}

/**
 * @brief Retourne un pointeur vers l'état système (lecture seule)
 */
const etat_complet_systeme_t* app_get_etat_systeme(void) {
    return &g_etat_systeme;
}

/**
 * @brief Retourne un pointeur vers l'état système (lecture/écriture)
 * ATTENTION: À utiliser uniquement par les composants autorisés
 */
etat_complet_systeme_t* app_get_etat_systeme_rw(void) {
    return &g_etat_systeme;
}

/**
 * @brief Retourne l'event group système
 */
extern "C" EventGroupHandle_t app_get_event_group_systeme(void) {
    return g_event_group_systeme;
}
