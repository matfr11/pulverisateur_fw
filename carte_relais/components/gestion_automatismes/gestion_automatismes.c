/**
 * @file gestion_automatismes.c
 * @brief Machines à états pour transfert automatique et brassage cyclique.
 *
 * CARTE AVANT UNIQUEMENT.
 */
#include "gestion_automatismes.h"
#include "gestion_actionneurs.h"
#include "gestion_capteurs.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "AUTO";

#ifdef CARTE_AVANT

/* ====================================================================
 * ÉTAT INTERNE DU TRANSFERT
 * ==================================================================== */
static struct {
    etat_auto_transfert_t etat;
    uint32_t    volume_cible;       /* Litres */
    float       volume_debut;       /* Volume session au démarrage */
    float       volume_transfere;   /* Litres transférés (auto + manuel) */
    bool        en_transfert_manuel; /* Pompe ON + vanne transfert sans auto */
} s_transfert = {
    .etat = AUTO_TR_INACTIF,
    .volume_cible = 0,
    .volume_debut = 0.0f,
    .volume_transfere = 0.0f,
    .en_transfert_manuel = false,
};
/* ====================================================================
 * ÉTAT INTERNE DU BRASSAGE
 * ==================================================================== */
static struct {
    etat_auto_brassage_t etat;
    uint32_t    temps_on_ms;        /* Durée pompe ON (ms) */
    uint32_t    temps_off_ms;       /* Durée pompe OFF (ms) */
    int64_t     timestamp_phase;    /* Début de la phase courante (ms) */
} s_brassage = {
    .etat = AUTO_BR_INACTIF,
    .temps_on_ms = 0,
    .temps_off_ms = 0,
    .timestamp_phase = 0,
};

/* ====================================================================
 * INITIALISATION
 * ==================================================================== */
void automatismes_initialiser(void)
{
    s_transfert.etat = AUTO_TR_INACTIF;
    s_brassage.etat = AUTO_BR_INACTIF;
    ESP_LOGI(TAG, "Automatismes initialisés.");
}

/* ====================================================================
 * TRANSFERT AUTOMATIQUE
 * ==================================================================== */
void automatismes_transfert_activer(const configuration_t *config)
{
    if (s_transfert.etat == AUTO_TR_EN_COURS) {
        ESP_LOGW(TAG, "Transfert déjà en cours.");
        return;
    }

    s_transfert.volume_cible = config->volume_transfert;
    s_transfert.volume_debut = capteurs_debitmetre_get_volume_session();
    s_transfert.etat = AUTO_TR_EN_COURS;
    s_transfert.volume_transfere = 0.0f; //  Remettre à zéro à l'activation du transfert auto
    /* Positionner la vanne 3 voies en mode transfert */
    actionneurs_v3v_set(V3V_TRANSFERT);

    /* Démarrer la pompe */
    actionneurs_pompe_set(true);

    /* Suspendre le brassage si actif */
    if (s_brassage.etat == AUTO_BR_MARCHE || s_brassage.etat == AUTO_BR_PAUSE) {
        s_brassage.etat = AUTO_BR_SUSPENDU;
        ESP_LOGI(TAG, "Brassage suspendu (transfert prioritaire).");
    }

    ESP_LOGI(TAG, "Transfert activé : cible = %lu L", (unsigned long)s_transfert.volume_cible);
}

void automatismes_transfert_arreter(void)
{
    if (s_transfert.etat == AUTO_TR_INACTIF) return;

    /* Arrêter la pompe et revenir en brassage */
    actionneurs_pompe_set(false);
    actionneurs_v3v_set(V3V_BRASSAGE);

    etat_auto_transfert_t ancien = s_transfert.etat;
    s_transfert.etat = AUTO_TR_INACTIF;

    ESP_LOGI(TAG, "Transfert arrêté (état précédent: %d).", ancien);

    /* Reprendre le brassage si suspendu */
    if (s_brassage.etat == AUTO_BR_SUSPENDU) {
        s_brassage.etat = AUTO_BR_MARCHE;
        s_brassage.timestamp_phase = esp_timer_get_time() / 1000;
        actionneurs_v3v_set(V3V_BRASSAGE);
        actionneurs_pompe_set(true);
        ESP_LOGI(TAG, "Brassage repris après transfert.");
    }
}

etat_auto_transfert_t automatismes_get_etat_transfert(void)
{
    return s_transfert.etat;
}

/* ====================================================================
 * BRASSAGE AUTOMATIQUE
 * ==================================================================== */
void automatismes_brassage_activer(const configuration_t *config)
{
    if (s_brassage.etat != AUTO_BR_INACTIF) {
        ESP_LOGW(TAG, "Brassage déjà actif.");
        return;
    }

    s_brassage.temps_on_ms = config->temps_brassage_on * 1000;
    s_brassage.temps_off_ms = config->temps_brassage_off * 1000;
    s_brassage.timestamp_phase = esp_timer_get_time() / 1000;

    /* Si transfert en cours, démarrer en mode suspendu */
    if (s_transfert.etat == AUTO_TR_EN_COURS) {
        s_brassage.etat = AUTO_BR_SUSPENDU;
        ESP_LOGI(TAG, "Brassage activé (suspendu, transfert en cours).");
    } else {
        s_brassage.etat = AUTO_BR_MARCHE;
        actionneurs_v3v_set(V3V_BRASSAGE);
        actionneurs_pompe_set(true);
        ESP_LOGI(TAG, "Brassage activé : ON=%lus, OFF=%lus",
                 (unsigned long)(s_brassage.temps_on_ms/1000),
                 (unsigned long)(s_brassage.temps_off_ms/1000));
    }
}

void automatismes_brassage_arreter(void)
{
    if (s_brassage.etat == AUTO_BR_INACTIF) return;

    /* Ne couper la pompe que si le transfert n'est pas actif */
    if (s_transfert.etat != AUTO_TR_EN_COURS) {
        actionneurs_pompe_set(false);
    }

    s_brassage.etat = AUTO_BR_INACTIF;
    ESP_LOGI(TAG, "Brassage arrêté.");
}

etat_auto_brassage_t automatismes_get_etat_brassage(void)
{
    return s_brassage.etat;
}

void automatismes_get_brassage_info(char *label_out, size_t label_size,
                                     float *temps_restant_out,
                                     float *pourcentage_out)
{
    int64_t maintenant = esp_timer_get_time() / 1000;
    int64_t ecart_ms = maintenant - s_brassage.timestamp_phase;

    switch (s_brassage.etat) {
    case AUTO_BR_MARCHE: {
        snprintf(label_out, label_size, "MARCHE");
        int64_t restant_ms = (int64_t)s_brassage.temps_on_ms - ecart_ms;
        if (restant_ms < 0) restant_ms = 0;
        *temps_restant_out = (float)restant_ms / 1000.0f;  /* en secondes */
        *pourcentage_out = (s_brassage.temps_on_ms > 0)
            ? ((float)ecart_ms / (float)s_brassage.temps_on_ms * 100.0f)
            : 0.0f;
        break;
    }
    case AUTO_BR_PAUSE: {
        snprintf(label_out, label_size, "PAUSE");
        int64_t restant_ms = (int64_t)s_brassage.temps_off_ms - ecart_ms;
        if (restant_ms < 0) restant_ms = 0;
        *temps_restant_out = (float)restant_ms / 1000.0f;  /* en secondes */
        *pourcentage_out = (s_brassage.temps_off_ms > 0)
            ? ((float)ecart_ms / (float)s_brassage.temps_off_ms * 100.0f)
            : 0.0f;
        break;
    }
    case AUTO_BR_SUSPENDU:
        snprintf(label_out, label_size, "SUSPENDU");
        *temps_restant_out = 0.0f;
        *pourcentage_out = 0.0f;
        break;
    default:
        snprintf(label_out, label_size, "INACTIF");
        *temps_restant_out = 0.0f;
        *pourcentage_out = 0.0f;
        break;
    }

    /* Clamp */
    if (*pourcentage_out > 100.0f) *pourcentage_out = 100.0f;
    if (*pourcentage_out < 0.0f)   *pourcentage_out = 0.0f;
}

/* ====================================================================
 * MISE À JOUR PÉRIODIQUE
 * ==================================================================== */
void automatismes_update(float debit_lpm, float volume_session)
{
    int64_t maintenant = esp_timer_get_time() / 1000;

    /* L'état TERMINE a été publié par app_tache_principale lors de l'itération
     * précédente. On peut maintenant finaliser l'arrêt. */
    if (s_transfert.etat == AUTO_TR_TERMINE) {
        automatismes_transfert_arreter();
    }

/* --- TRANSFERT (auto + manuel) --- */
    bool pompe_active = actionneurs_pompe_est_active();
    bool vanne_transfert = actionneurs_v3v_est_transfert();
    bool en_transfert = pompe_active && vanne_transfert;

    /* Détection début transfert manuel (pompe + vanne transfert sans auto) */
    if (en_transfert && s_transfert.etat == AUTO_TR_INACTIF && !s_transfert.en_transfert_manuel) {
        s_transfert.en_transfert_manuel = true;
        s_transfert.volume_debut = volume_session;
        s_transfert.volume_transfere = 0.0f;
        ESP_LOGI(TAG, "Transfert manuel détecté, compteur remis à zéro.");
    }
    /* Fin du transfert manuel */
    if (s_transfert.en_transfert_manuel && !en_transfert) {
        s_transfert.en_transfert_manuel = false;
    }

    /* Mise à jour du volume transféré (auto ou manuel) */
    if (en_transfert) {
        s_transfert.volume_transfere = volume_session - s_transfert.volume_debut;
    }

    /* Auto-transfert : vérifier si la cible est atteinte */
    if (s_transfert.etat == AUTO_TR_EN_COURS) {
        if (s_transfert.volume_transfere >= (float)s_transfert.volume_cible) {
            ESP_LOGI(TAG, "Transfert terminé : %.1f L transférés.", s_transfert.volume_transfere);
            actionneurs_pompe_set(false);
            actionneurs_v3v_set(V3V_BRASSAGE);
            s_transfert.etat = AUTO_TR_TERMINE;
            /* arreter() sera appelé à l'itération suivante (voir début de fonction),
             * après que app_tache_principale a publié l'état TERMINE via MQTT. */
        }
    }

    /* --- BRASSAGE --- */
    if (s_brassage.etat == AUTO_BR_MARCHE) {
        int64_t ecart = maintenant - s_brassage.timestamp_phase;
        if (ecart >= (int64_t)s_brassage.temps_on_ms) {
            /* Fin de la phase MARCHE → passer en PAUSE */
            actionneurs_pompe_set(false);
            s_brassage.etat = AUTO_BR_PAUSE;
            s_brassage.timestamp_phase = maintenant;
            ESP_LOGD(TAG, "Brassage : MARCHE → PAUSE");
        }
    } else if (s_brassage.etat == AUTO_BR_PAUSE) {
        int64_t ecart = maintenant - s_brassage.timestamp_phase;
        if (ecart >= (int64_t)s_brassage.temps_off_ms) {
            /* Fin de la phase PAUSE → reprendre MARCHE */
            actionneurs_v3v_set(V3V_BRASSAGE);
            actionneurs_pompe_set(true);
            s_brassage.etat = AUTO_BR_MARCHE;
            s_brassage.timestamp_phase = maintenant;
            ESP_LOGD(TAG, "Brassage : PAUSE → MARCHE");
        }
    }
    /* AUTO_BR_SUSPENDU : rien à faire, le transfert gère */
}

/* ====================================================================
 * ARRÊT GLOBAL
 * ==================================================================== */
void automatismes_arreter_tout(void)
{
    if (s_transfert.etat == AUTO_TR_INACTIF && s_brassage.etat == AUTO_BR_INACTIF) {
        return;
    }
    ESP_LOGW(TAG, "Arrêt de tous les automatismes.");

    /* Ne couper les actionneurs que si un automatisme les pilotait */
    if (s_transfert.etat == AUTO_TR_EN_COURS) {
        actionneurs_pompe_set(false);
        actionneurs_v3v_set(V3V_BRASSAGE);
    } else if (s_brassage.etat == AUTO_BR_MARCHE) {
        actionneurs_pompe_set(false);
    }

    s_transfert.etat = AUTO_TR_INACTIF;
    s_brassage.etat = AUTO_BR_INACTIF;
}
float automatismes_get_volume_transfere(void)
{
    return s_transfert.volume_transfere;
}
#else
/* Stubs pour la carte ARRIÈRE (pas d'automatismes) */
float automatismes_get_volume_transfere(void) { return 0.0f; }
void automatismes_initialiser(void) {}
void automatismes_transfert_activer(const configuration_t *c) { (void)c; }
void automatismes_transfert_arreter(void) {}
etat_auto_transfert_t automatismes_get_etat_transfert(void) { return AUTO_TR_INACTIF; }
void automatismes_brassage_activer(const configuration_t *c) { (void)c; }
void automatismes_brassage_arreter(void) {}
etat_auto_brassage_t automatismes_get_etat_brassage(void) { return AUTO_BR_INACTIF; }
void automatismes_get_brassage_info(char *l, size_t s, float *t, float *p) {
    snprintf(l, s, "N/A"); *t = 0; *p = 0;
}
void automatismes_update(float d, float v) { (void)d; (void)v; }
void automatismes_arreter_tout(void) {}
#endif /* CARTE_AVANT */
