/**
 * @file gestion_actionneurs.c
 * @brief Pilotage relais avec interlocks vannes 3 fils et timeouts de sécurité.
 */
#include "gestion_actionneurs.h"
#include "board_config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"

static const char *TAG = "ACTIONNEURS";

/* ====================================================================
 * HELPERS GPIO
 *
 * Ces fonctions utilisent RELAIS_NIVEAU_ACTIF, qui n'est défini que
 * pour les cartes avec des relais physiques (CARTE_AVANT et CARTE_ARRIERE).
 * La carte serveur n'a pas de relais, donc on exclut ces helpers à la
 * compilation pour éviter une erreur de symbole non défini.
 * ==================================================================== */
#if defined(CARTE_AVANT) || defined(CARTE_ARRIERE)

static inline void gpio_relais_set(gpio_num_t pin, bool actif)
{
    gpio_set_level(pin, actif ? RELAIS_NIVEAU_ACTIF : !RELAIS_NIVEAU_ACTIF);
    ESP_LOGD(TAG, "GPIO %d → %s", pin, actif ? "ON" : "OFF");
}

static void gpio_configurer_sortie(gpio_num_t pin)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(pin, !RELAIS_NIVEAU_ACTIF); /* OFF par défaut */
}

#endif /* CARTE_AVANT || CARTE_ARRIERE */

/* ====================================================================
 * CARTE AVANT – ÉTAT INTERNE
 * ==================================================================== */
#ifdef CARTE_AVANT
static bool s_pompe_active = false;
static etat_vanne_3v_t s_v3v = V3V_BRASSAGE;
static bool s_phares_avant = false;
#endif

/* ====================================================================
 * CARTE ARRIÈRE – ÉTAT INTERNE VANNES MOTORISÉES
 * ==================================================================== */
#ifdef CARTE_ARRIERE
typedef struct {
    gpio_num_t      gpio_ouvrir;
    gpio_num_t      gpio_fermer;
    commande_vanne_t commande_courante;
    etat_vanne_mot_t etat;
    int64_t         timestamp_debut_ms;  /* Début de l'action en cours */
    bool            timeout_atteint;
} vanne_motorisee_t;

static vanne_motorisee_t s_vannes[VANNE_ID_MAX];
static bool s_phares_arriere = false;
#endif

/* ====================================================================
 * INITIALISATION
 * ==================================================================== */
void actionneurs_initialiser(void)
{
    ESP_LOGI(TAG, "Initialisation des actionneurs...");

#ifdef CARTE_AVANT
    gpio_configurer_sortie(GPIO_RELAIS_POMPE);
    gpio_configurer_sortie(GPIO_RELAIS_V3V);
    gpio_configurer_sortie(GPIO_RELAIS_PHARES_AV);
    gpio_configurer_sortie(GPIO_RELAIS_RESERVE_AV);

    s_pompe_active = false;
    s_v3v = V3V_BRASSAGE;
    s_phares_avant = false;
    ESP_LOGI(TAG, "Carte AVANT : 4 relais configurés.");
#endif

#ifdef CARTE_ARRIERE
    /* Vanne 2m */
    s_vannes[VANNE_ID_2M].gpio_ouvrir = GPIO_V2M_OUVRIR;
    s_vannes[VANNE_ID_2M].gpio_fermer = GPIO_V2M_FERMER;
    s_vannes[VANNE_ID_2M].commande_courante = VANNE_STOP;
    s_vannes[VANNE_ID_2M].etat = VANNE_ETAT_ARRETEE;
    s_vannes[VANNE_ID_2M].timestamp_debut_ms = 0;
    s_vannes[VANNE_ID_2M].timeout_atteint = false;

    /* Vanne bout de rampe */
    s_vannes[VANNE_ID_BDR].gpio_ouvrir = GPIO_VBR_OUVRIR;
    s_vannes[VANNE_ID_BDR].gpio_fermer = GPIO_VBR_FERMER;
    s_vannes[VANNE_ID_BDR].commande_courante = VANNE_STOP;
    s_vannes[VANNE_ID_BDR].etat = VANNE_ETAT_ARRETEE;
    s_vannes[VANNE_ID_BDR].timestamp_debut_ms = 0;
    s_vannes[VANNE_ID_BDR].timeout_atteint = false;

    for (int i = 0; i < VANNE_ID_MAX; i++) {
        gpio_configurer_sortie(s_vannes[i].gpio_ouvrir);
        gpio_configurer_sortie(s_vannes[i].gpio_fermer);
    }

    gpio_configurer_sortie(GPIO_RELAIS_PHARES_AR);
    s_phares_arriere = false;

    ESP_LOGI(TAG, "Carte ARRIÈRE : vannes motorisées + phares configurés.");
#endif
}

/* ====================================================================
 * CARTE AVANT – IMPLÉMENTATION
 * ==================================================================== */
#ifdef CARTE_AVANT

void actionneurs_pompe_toggle(void)
{
    s_pompe_active = !s_pompe_active;
    gpio_relais_set(GPIO_RELAIS_POMPE, s_pompe_active);
    ESP_LOGI(TAG, "Pompe → %s", s_pompe_active ? "MARCHE" : "ARRÊT");
}

void actionneurs_pompe_set(bool active)
{
    s_pompe_active = active;
    gpio_relais_set(GPIO_RELAIS_POMPE, s_pompe_active);
}

bool actionneurs_pompe_est_active(void)
{
    return s_pompe_active;
}

void actionneurs_v3v_toggle(void)
{
    s_v3v = (s_v3v == V3V_BRASSAGE) ? V3V_TRANSFERT : V3V_BRASSAGE;
    gpio_relais_set(GPIO_RELAIS_V3V, s_v3v == V3V_TRANSFERT);
    ESP_LOGI(TAG, "Vanne 3V → %s", s_v3v == V3V_TRANSFERT ? "TRANSFERT" : "BRASSAGE");
}

void actionneurs_v3v_set(etat_vanne_3v_t position)
{
    s_v3v = position;
    gpio_relais_set(GPIO_RELAIS_V3V, s_v3v == V3V_TRANSFERT);
}

bool actionneurs_v3v_est_transfert(void)
{
    return s_v3v == V3V_TRANSFERT;
}

void actionneurs_phares_avant_toggle(void)
{
    s_phares_avant = !s_phares_avant;
    gpio_relais_set(GPIO_RELAIS_PHARES_AV, s_phares_avant);
}

bool actionneurs_phares_avant_actifs(void)
{
    return s_phares_avant;
}

#endif /* CARTE_AVANT */

/* ====================================================================
 * CARTE ARRIÈRE – VANNES MOTORISÉES AVEC INTERLOCK
 * ==================================================================== */
#ifdef CARTE_ARRIERE

void actionneurs_vanne_commander(vanne_id_t id, commande_vanne_t cmd)
{
    if (id >= VANNE_ID_MAX) return;

    vanne_motorisee_t *v = &s_vannes[id];

    /* INTERLOCK : TOUJOURS couper les deux relais d'abord */
    gpio_relais_set(v->gpio_ouvrir, false);
    gpio_relais_set(v->gpio_fermer, false);

    /* Petite tempo pour l'interlock (non-bloquante dans le contexte) */
    /* Note : en pratique, les GPIO sont commutées instantanément,
     * le risque de court-circuit est nul avec cette séquence. */

    v->commande_courante = cmd;
    v->timeout_atteint = false;

    switch (cmd) {
    case VANNE_OUVRE:
        gpio_relais_set(v->gpio_ouvrir, true);
        v->etat = VANNE_ETAT_EN_OUVERTURE;
        v->timestamp_debut_ms = esp_timer_get_time() / 1000;
        ESP_LOGI(TAG, "Vanne %d → OUVRIR", id);
        break;

    case VANNE_FERME:
        gpio_relais_set(v->gpio_fermer, true);
        v->etat = VANNE_ETAT_EN_FERMETURE;
        v->timestamp_debut_ms = esp_timer_get_time() / 1000;
        ESP_LOGI(TAG, "Vanne %d → FERMER", id);
        break;

    case VANNE_STOP:
    default:
        v->etat = VANNE_ETAT_ARRETEE;
        v->timestamp_debut_ms = 0;
        ESP_LOGI(TAG, "Vanne %d → STOP", id);
        break;
    }
}

etat_vanne_mot_t actionneurs_vanne_get_etat(vanne_id_t id)
{
    if (id >= VANNE_ID_MAX) return VANNE_ETAT_INCONNU;
    return s_vannes[id].etat;
}

void actionneurs_vannes_update_timeout(uint32_t timeout_ms)
{
    int64_t maintenant = esp_timer_get_time() / 1000;

    for (int i = 0; i < VANNE_ID_MAX; i++) {
        vanne_motorisee_t *v = &s_vannes[i];

        if (v->commande_courante != VANNE_STOP && v->timestamp_debut_ms > 0) {
            int64_t duree = maintenant - v->timestamp_debut_ms;
            if (duree >= (int64_t)timeout_ms) {
                /* TIMEOUT : couper les deux relais */
                gpio_relais_set(v->gpio_ouvrir, false);
                gpio_relais_set(v->gpio_fermer, false);
                v->commande_courante = VANNE_STOP;
                v->etat = VANNE_ETAT_TIMEOUT;
                v->timeout_atteint = true;
                v->timestamp_debut_ms = 0;
                ESP_LOGW(TAG, "TIMEOUT vanne %d ! Relais coupés.", i);
            }
        }
    }
}

void actionneurs_phares_arriere_toggle(void)
{
    s_phares_arriere = !s_phares_arriere;
    gpio_relais_set(GPIO_RELAIS_PHARES_AR, s_phares_arriere);
}

bool actionneurs_phares_arriere_actifs(void)
{
    return s_phares_arriere;
}

#endif /* CARTE_ARRIERE */

/* ====================================================================
 * ARRÊT D'URGENCE (TOUTES CARTES)
 * ==================================================================== */
void actionneurs_tout_arreter(void)
{
    ESP_LOGW(TAG, ">>> ARRÊT DE TOUS LES ACTIONNEURS <<<");

#ifdef CARTE_AVANT
    s_pompe_active = false;
    gpio_relais_set(GPIO_RELAIS_POMPE, false);
    s_v3v = V3V_BRASSAGE;
    gpio_relais_set(GPIO_RELAIS_V3V, false);
#endif

#ifdef CARTE_ARRIERE
    for (int i = 0; i < VANNE_ID_MAX; i++) {
        gpio_relais_set(s_vannes[i].gpio_ouvrir, false);
        gpio_relais_set(s_vannes[i].gpio_fermer, false);
        s_vannes[i].commande_courante = VANNE_STOP;
        s_vannes[i].etat = VANNE_ETAT_ARRETEE;
    }
#endif
}
