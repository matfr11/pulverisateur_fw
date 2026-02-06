/**
 * @file gestion_securites.c
 * @brief Détection cuve avant vide : pompe active + débit < seuil pendant un délai.
 *
 * Machine à états :
 *   SEC_CUVE_OK → SEC_CUVE_DETECTION → SEC_CUVE_VIDE
 *                        ↓ (débit revient)
 *                   SEC_CUVE_OK
 *
 * Sortie de SEC_CUVE_VIDE : réarmement manuel uniquement.
 */
#include "gestion_securites.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "SECURITES";

#ifdef CARTE_AVANT

static etat_securite_cuve_t s_etat_cuve = SEC_CUVE_OK;
static int64_t              s_timestamp_detection_ms = 0;

void securites_initialiser(void)
{
    s_etat_cuve = SEC_CUVE_OK;
    s_timestamp_detection_ms = 0;
    ESP_LOGI(TAG, "Sécurités initialisées.");
}

void securites_update(bool pompe_active, float debit_lpm, const configuration_t *config)
{
    int64_t maintenant = esp_timer_get_time() / 1000;

    switch (s_etat_cuve) {
    case SEC_CUVE_OK:
        /* Condition de déclenchement : pompe active ET débit bas */
        if (pompe_active && debit_lpm < config->seuil_debit_cuve_vide) {
            s_etat_cuve = SEC_CUVE_DETECTION;
            s_timestamp_detection_ms = maintenant;
            ESP_LOGW(TAG, "Débit faible détecté (%.1f < %.1f L/min). Surveillance...",
                     debit_lpm, config->seuil_debit_cuve_vide);
        }
        break;

    case SEC_CUVE_DETECTION:
        if (!pompe_active || debit_lpm >= config->seuil_debit_cuve_vide) {
            /* Le débit est revenu ou la pompe est coupée → fausse alerte */
            s_etat_cuve = SEC_CUVE_OK;
            ESP_LOGI(TAG, "Débit revenu à la normale. Fausse alerte.");
        } else {
            /* Vérifier si le délai est dépassé */
            int64_t ecart = maintenant - s_timestamp_detection_ms;
            if (ecart >= (int64_t)config->delai_detection_ms) {
                /* CUVE VIDE CONFIRMÉE */
                s_etat_cuve = SEC_CUVE_VIDE;
                ESP_LOGE(TAG, ">>> CUVE AVANT VIDE CONFIRMÉE <<<");
                ESP_LOGE(TAG, "Débit faible pendant %ld ms. Automatismes arrêtés.",
                         (long)ecart);
            }
        }
        break;

    case SEC_CUVE_VIDE:
        /* Reste en état VIDE jusqu'au réarmement manuel.
         * Si l'opérateur relance la pompe et que le débit revient,
         * securites_rearmement_cuve() est appelé. */
        if (pompe_active && debit_lpm >= config->seuil_debit_cuve_vide) {
            /* L'opérateur a relancé la pompe et le débit est bon → réarmer */
            securites_rearmement_cuve();
        }
        break;
    }
}

bool securites_cuve_est_vide(void)
{
    return s_etat_cuve == SEC_CUVE_VIDE;
}

etat_securite_cuve_t securites_get_etat_cuve(void)
{
    return s_etat_cuve;
}

void securites_rearmement_cuve(void)
{
    if (s_etat_cuve == SEC_CUVE_VIDE) {
        s_etat_cuve = SEC_CUVE_OK;
        s_timestamp_detection_ms = 0;
        ESP_LOGI(TAG, "Sécurité cuve réarmée.");
    }
}

#else
/* Stubs pour la carte ARRIÈRE */
void securites_initialiser(void) {}
void securites_update(bool p, float d, const configuration_t *c) { (void)p; (void)d; (void)c; }
bool securites_cuve_est_vide(void) { return false; }
etat_securite_cuve_t securites_get_etat_cuve(void) { return SEC_CUVE_OK; }
void securites_rearmement_cuve(void) {}
#endif /* CARTE_AVANT */
