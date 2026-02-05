/**
 * @file actionneurs_interlock.cpp
 * @brief Gestion interlock et sécurités vannes 3 fils
 * 
 * SÉCURITÉS CRITIQUES:
 * - Interlock matériel: Vérification avant activation
 * - Timeouts automatiques
 * - Détection conflits
 * - Arrêt d'urgence
 * 
 * @version 1.0
 * @date 2026-02-02
 */

#include "actionneurs.h"
#include "board_config.h"
#include "esp_log.h"
#include "driver/gpio.h"

#ifdef BOARD_TYPE_ARRIERE

static const char* TAG = "INTERLOCK";

// ============================================================================
// VÉRIFICATIONS INTERLOCK
// ============================================================================

/**
 * @brief Vérifie l'interlock avant activation relais
 * 
 * S'assure qu'il n'y a pas de conflit matériel
 * 
 * @param gpio_a Premier GPIO à vérifier
 * @param gpio_b Deuxième GPIO (doit être OFF si gpio_a va être ON)
 * @return true si sécurisé
 */
bool interlock_verifier_gpio(gpio_num_t gpio_a, gpio_num_t gpio_b) {
    // Lire état actuel des GPIO
    int level_a = gpio_get_level(gpio_a);
    int level_b = gpio_get_level(gpio_b);
    
    // VIOLATION: Les deux sont actifs simultanément
    if (level_a == 1 && level_b == 1) {
        ESP_LOGE(TAG, "VIOLATION INTERLOCK! GPIO %d et %d actifs simultanément", 
                 gpio_a, gpio_b);
        
        // SÉCURITÉ: Couper les deux immédiatement
        gpio_set_level(gpio_a, 0);
        gpio_set_level(gpio_b, 0);
        
        // Publier alerte
        extern esp_err_t mqtt_publish(const char*, const char*, int, bool);
        extern const char TOPIC_SECURITE_INTERLOCK[];
        char payload[128];
        snprintf(payload, sizeof(payload), 
                 "{\"gpio_a\":%d,\"gpio_b\":%d,\"action\":\"COUPE_URGENCE\"}", 
                 gpio_a, gpio_b);
        mqtt_publish(TOPIC_SECURITE_INTERLOCK, payload, 1, false);
        
        return false;
    }
    
    return true;
}

/**
 * @brief Vérifie interlock vanne 2m
 */
bool interlock_vanne_2m_ok(void) {
    return interlock_verifier_gpio(GPIO_VANNE_2M_OUVRIR, GPIO_VANNE_2M_FERMER);
}

/**
 * @brief Vérifie interlock vanne bout de rampe
 */
bool interlock_vanne_bout_ok(void) {
    return interlock_verifier_gpio(GPIO_VANNE_BOUT_OUVRIR, GPIO_VANNE_BOUT_FERMER);
}

/**
 * @brief Vérification globale interlock (appelée périodiquement)
 */
void interlock_verifier_tout(void) {
    interlock_vanne_2m_ok();
    interlock_vanne_bout_ok();
}

// ============================================================================
// TEST INTERLOCK
// ============================================================================

/**
 * @brief Test de l'interlock (mode debug uniquement)
 * 
 * ATTENTION: Ne jamais appeler en production!
 * Ce test active volontairement un état dangereux pour vérifier
 * que la sécurité fonctionne.
 */
#ifdef CONFIG_LOG_LEVEL_DEBUG
void interlock_test_violation(void) {
    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, "  TEST INTERLOCK (MODE DEBUG)");
    ESP_LOGW(TAG, "========================================");
    
    // Activer volontairement les deux relais (DANGEREUX!)
    ESP_LOGW(TAG, "Activation simultanée vanne 2m (test violation)...");
    gpio_set_level(GPIO_VANNE_2M_OUVRIR, 1);
    gpio_set_level(GPIO_VANNE_2M_FERMER, 1);
    
    // Attendre 100ms
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Vérifier interlock (devrait détecter et couper)
    bool ok = interlock_vanne_2m_ok();
    
    if (!ok) {
        ESP_LOGI(TAG, "✓ Interlock a détecté et coupé la violation");
    } else {
        ESP_LOGE(TAG, "✗ Interlock n'a PAS détecté la violation!");
    }
    
    // S'assurer que tout est éteint
    gpio_set_level(GPIO_VANNE_2M_OUVRIR, 0);
    gpio_set_level(GPIO_VANNE_2M_FERMER, 0);
    
    ESP_LOGW(TAG, "Test terminé");
}
#endif

#endif // BOARD_TYPE_ARRIERE
