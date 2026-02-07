/**
 * @file ecran_ui.c
 * @brief Interface LVGL pour écran 7" 1024×600 (ESP32-P4).
 *
 * Reproduit l'interface Web en 3 panneaux côte à côte :
 *   1. Unité de Pompage (débit, pompe, V3V, phare AV)
 *   2. Automate (transfert auto, brassage auto, arrêt urgence)
 *   3. Vannes & Cuve AR (niveau, vannes motorisées, phare AR)
 *
 * LVGL v8 — toutes les opérations doivent être appelées depuis la tâche LVGL
 * ou entre bsp_display_lock() / bsp_display_unlock().
 */
#include "ecran_ui.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "ECRAN_UI";

/* ====================================================================
 * COULEURS (identiques à la Web UI)
 * ==================================================================== */
#define COL_BG              lv_color_hex(0x121212)
#define COL_CARD            lv_color_hex(0x262626)
#define COL_CARD_BORDER     lv_color_hex(0x383838)
#define COL_TOPBAR          lv_color_hex(0x1a1a1a)
#define COL_PANEL_MID       lv_color_hex(0x151515)
#define COL_GREEN           lv_color_hex(0x2ecc71)
#define COL_RED             lv_color_hex(0xe74c3c)
#define COL_BLUE            lv_color_hex(0x3399ff)
#define COL_YELLOW          lv_color_hex(0xf1c40f)
#define COL_ORANGE          lv_color_hex(0xf39c12)
#define COL_DARK_RED        lv_color_hex(0x331111)
#define COL_DARK_RED_BDR    lv_color_hex(0x552222)
#define COL_TEXT            lv_color_hex(0xeeeeee)
#define COL_TEXT_DIM        lv_color_hex(0x777777)
#define COL_BTN_OFF         lv_color_hex(0x333333)
#define COL_BTN_V_OFF       lv_color_hex(0x333333)
#define COL_GAUGE_BG        lv_color_hex(0x000000)
#define COL_SEPARATOR       lv_color_hex(0x333333)
#define COL_WHITE           lv_color_hex(0xffffff)

/* ====================================================================
 * DIMENSIONS
 * ==================================================================== */
#define SCREEN_W        1024
#define SCREEN_H        600
#define TOPBAR_H        44
#define MAIN_H          (SCREEN_H - TOPBAR_H)
#define PANEL_PAD       10
#define CARD_PAD_H      10
#define CARD_PAD_V      8
#define CARD_RADIUS     10
#define BTN_H           70
#define BTN_RADIUS      8
#define GAUGE_H         34
#define GAUGE_RADIUS    5
#define VANNE_BTN_H     56

/* ====================================================================
 * CALLBACKS
 * ==================================================================== */
static ecran_cmd_cb_t       s_cmd_cb = NULL;
static ecran_save_cfg_cb_t  s_save_cb = NULL;

/* ====================================================================
 * ÉCRANS
 * ==================================================================== */
static lv_obj_t *s_scr_main     = NULL;
static lv_obj_t *s_scr_settings = NULL;

/* ====================================================================
 * WIDGETS — BARRE SUPÉRIEURE
 * ==================================================================== */
static lv_obj_t *s_lbl_status   = NULL;
static lv_obj_t *s_lbl_alert    = NULL;
static lv_obj_t *s_lbl_version  = NULL;

/* ====================================================================
 * WIDGETS — PANNEAU 1 : POMPAGE
 * ==================================================================== */
static lv_obj_t *s_bar_debit    = NULL;
static lv_obj_t *s_lbl_debit    = NULL;
static lv_obj_t *s_btn_pompe    = NULL;
static lv_obj_t *s_lbl_pompe    = NULL;
static lv_obj_t *s_btn_v3v      = NULL;
static lv_obj_t *s_lbl_v3v      = NULL;
static lv_obj_t *s_btn_phare_av = NULL;
static lv_obj_t *s_lbl_phare_av = NULL;

/* ====================================================================
 * WIDGETS — PANNEAU 2 : AUTOMATE
 * ==================================================================== */
static lv_obj_t *s_btn_auto_tr  = NULL;
static lv_obj_t *s_lbl_auto_tr  = NULL;
static lv_obj_t *s_bar_tr       = NULL;
static lv_obj_t *s_lbl_tr_vol   = NULL;
static lv_obj_t *s_btn_auto_br  = NULL;
static lv_obj_t *s_lbl_auto_br  = NULL;
static lv_obj_t *s_bar_br       = NULL;
static lv_obj_t *s_lbl_br_time  = NULL;
static lv_obj_t *s_btn_urgence  = NULL;

/* ====================================================================
 * WIDGETS — PANNEAU 3 : VANNES & CUVE AR
 * ==================================================================== */
static lv_obj_t *s_bar_niveau   = NULL;
static lv_obj_t *s_lbl_niveau   = NULL;
static lv_obj_t *s_btn_v2m[3]   = {NULL};  /* F, S, O */
static lv_obj_t *s_btn_vbt[3]   = {NULL};  /* F, S, O */
static lv_obj_t *s_btn_phare_ar = NULL;
static lv_obj_t *s_lbl_phare_ar = NULL;

/* ====================================================================
 * WIDGETS — SETTINGS
 * ==================================================================== */
#define NB_SETTINGS 10
static lv_obj_t *s_spinbox[NB_SETTINGS] = {NULL};
static configuration_t s_cfg_edit;  /* Copie de travail */

/* État interne pour le toggle des automatismes */
static bool s_auto_tr_actif = false;
static bool s_auto_br_actif = false;

/* ====================================================================
 * STYLES RÉUTILISABLES
 * ==================================================================== */
static lv_style_t sty_card;
static lv_style_t sty_gauge_bg;
static lv_style_t sty_gauge_fill;
static lv_style_t sty_btn;
static lv_style_t sty_topbar;

static void styles_init(void)
{
    /* Carte / conteneur */
    lv_style_init(&sty_card);
    lv_style_set_bg_color(&sty_card, COL_CARD);
    lv_style_set_bg_opa(&sty_card, LV_OPA_COVER);
    lv_style_set_radius(&sty_card, CARD_RADIUS);
    lv_style_set_border_color(&sty_card, COL_CARD_BORDER);
    lv_style_set_border_width(&sty_card, 1);
    lv_style_set_pad_all(&sty_card, CARD_PAD_H);
    lv_style_set_pad_row(&sty_card, 4);

    /* Jauge — fond */
    lv_style_init(&sty_gauge_bg);
    lv_style_set_bg_color(&sty_gauge_bg, COL_GAUGE_BG);
    lv_style_set_bg_opa(&sty_gauge_bg, LV_OPA_COVER);
    lv_style_set_radius(&sty_gauge_bg, GAUGE_RADIUS);
    lv_style_set_border_color(&sty_gauge_bg, lv_color_hex(0x444444));
    lv_style_set_border_width(&sty_gauge_bg, 1);

    /* Jauge — remplissage */
    lv_style_init(&sty_gauge_fill);
    lv_style_set_radius(&sty_gauge_fill, GAUGE_RADIUS);

    /* Bouton par défaut */
    lv_style_init(&sty_btn);
    lv_style_set_bg_color(&sty_btn, COL_BTN_OFF);
    lv_style_set_bg_opa(&sty_btn, LV_OPA_COVER);
    lv_style_set_radius(&sty_btn, BTN_RADIUS);
    lv_style_set_border_width(&sty_btn, 0);
    lv_style_set_text_color(&sty_btn, COL_TEXT);
    lv_style_set_text_font(&sty_btn, &lv_font_montserrat_16);

    /* Barre supérieure */
    lv_style_init(&sty_topbar);
    lv_style_set_bg_color(&sty_topbar, COL_TOPBAR);
    lv_style_set_bg_opa(&sty_topbar, LV_OPA_COVER);
    lv_style_set_border_color(&sty_topbar, COL_SEPARATOR);
    lv_style_set_border_width(&sty_topbar, 1);
    lv_style_set_border_side(&sty_topbar, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_radius(&sty_topbar, 0);
    lv_style_set_pad_hor(&sty_topbar, 12);
    lv_style_set_pad_ver(&sty_topbar, 0);
}

/* ====================================================================
 * HELPERS — CRÉATION DE WIDGETS
 * ==================================================================== */

/** Créer un label de titre de section (ex: "UNITÉ DE POMPAGE") */
static lv_obj_t *create_section_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl, COL_WHITE, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_letter_space(lbl, 2, 0);
    lv_obj_set_style_border_color(lbl, COL_BLUE, 0);
    lv_obj_set_style_border_width(lbl, 2, 0);
    lv_obj_set_style_border_side(lbl, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_bottom(lbl, 8, 0);
    lv_obj_set_style_pad_top(lbl, 4, 0);
    return lbl;
}

/** Créer un label de carte (petit, gris, centré) */
static lv_obj_t *create_card_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    return lbl;
}

/**
 * Créer une jauge horizontale (bar + label superposé).
 * Retourne le bar, et écrit le label dans *lbl_out.
 */
static lv_obj_t *create_gauge(lv_obj_t *parent, lv_obj_t **lbl_out,
                               lv_color_t fill_color, const char *init_text)
{
    /* Conteneur pour superposer bar et label */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), GAUGE_H);

    /* Bar */
    lv_obj_t *bar = lv_bar_create(cont);
    lv_obj_set_size(bar, lv_pct(100), GAUGE_H);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_add_style(bar, &sty_gauge_bg, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, fill_color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_add_style(bar, &sty_gauge_fill, LV_PART_INDICATOR);

    /* Label centré par-dessus */
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, init_text);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(lbl, COL_WHITE, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);

    if (lbl_out) *lbl_out = lbl;
    return bar;
}

/** Créer un bouton pleine largeur avec label centré */
static lv_obj_t *create_btn_full(lv_obj_t *parent, lv_obj_t **lbl_out,
                                  const char *text, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, lv_pct(100), BTN_H);
    lv_obj_add_style(btn, &sty_btn, 0);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

    if (lbl_out) *lbl_out = lbl;
    return btn;
}

/** Créer un petit bouton de vanne (F/S/O) */
static lv_obj_t *create_btn_vanne(lv_obj_t *parent, const char *text,
                                   lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, VANNE_BTN_H);
    lv_obj_set_style_bg_color(btn, COL_BTN_V_OFF, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x666666), 0);

    return btn;
}

/** Appliquer une couleur à un bouton + texte blanc */
static void btn_set_color(lv_obj_t *btn, lv_color_t bg, lv_color_t txt)
{
    lv_obj_set_style_bg_color(btn, bg, 0);
    /* Changer la couleur du label enfant */
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, txt, 0);
}

/* ====================================================================
 * ÉVÉNEMENTS — BOUTONS
 * ==================================================================== */

/* IDs pour user_data des callbacks */
enum {
    CMD_POMPE = 1,
    CMD_V3V,
    CMD_PHARE_AV,
    CMD_AUTO_TR,
    CMD_AUTO_BR,
    CMD_URGENCE,
    CMD_V2M_F, CMD_V2M_S, CMD_V2M_O,
    CMD_VBT_F, CMD_VBT_S, CMD_VBT_O,
    CMD_PHARE_AR,
    CMD_SETTINGS,
    CMD_SETTINGS_BACK,
    CMD_SETTINGS_SAVE,
};

static void evt_cmd(lv_event_t *e)
{
    if (!s_cmd_cb) return;
    int id = (int)(intptr_t)lv_event_get_user_data(e);

    switch (id) {
    case CMD_POMPE:     s_cmd_cb("avant", "pompe_toggle", NULL); break;
    case CMD_V3V:       s_cmd_cb("avant", "v3v_toggle", NULL); break;
    case CMD_PHARE_AV:  s_cmd_cb("avant", "phares_av_toggle", NULL); break;
    case CMD_PHARE_AR:  s_cmd_cb("arriere", "phares_ar_toggle", NULL); break;

    case CMD_AUTO_TR:
        s_cmd_cb("avant", "auto_transfert", s_auto_tr_actif ? "S" : "A");
        break;
    case CMD_AUTO_BR:
        s_cmd_cb("avant", "auto_brassage", s_auto_br_actif ? "S" : "A");
        break;
    case CMD_URGENCE:
        s_cmd_cb("urgence", "arret_urgence", NULL);
        break;

    case CMD_V2M_F:  s_cmd_cb("arriere", "v2m_fermer", NULL); break;
    case CMD_V2M_S:  s_cmd_cb("arriere", "v2m_stop", NULL); break;
    case CMD_V2M_O:  s_cmd_cb("arriere", "v2m_ouvrir", NULL); break;
    case CMD_VBT_F:  s_cmd_cb("arriere", "vbr_fermer", NULL); break;
    case CMD_VBT_S:  s_cmd_cb("arriere", "vbr_stop", NULL); break;
    case CMD_VBT_O:  s_cmd_cb("arriere", "vbr_ouvrir", NULL); break;

    case CMD_SETTINGS:
        if (s_scr_settings) lv_disp_load_scr(s_scr_settings);
        break;
    case CMD_SETTINGS_BACK:
        if (s_scr_main) lv_disp_load_scr(s_scr_main);
        break;
    case CMD_SETTINGS_SAVE:
        if (s_save_cb) {
            /* Lire les spinbox dans la config */
            s_cfg_edit.volume_transfert       = lv_spinbox_get_value(s_spinbox[0]);
            s_cfg_edit.facteur_k_debitmetre   = (float)lv_spinbox_get_value(s_spinbox[1]) / 100.0f;
            s_cfg_edit.seuil_debit_cuve_vide  = (float)lv_spinbox_get_value(s_spinbox[2]) / 10.0f;
            s_cfg_edit.delai_detection_ms      = lv_spinbox_get_value(s_spinbox[3]) * 1000;
            s_cfg_edit.timeout_vanne_ms        = lv_spinbox_get_value(s_spinbox[4]) * 1000;
            s_cfg_edit.temps_brassage_on       = lv_spinbox_get_value(s_spinbox[5]);
            s_cfg_edit.temps_brassage_off      = lv_spinbox_get_value(s_spinbox[6]);
            s_cfg_edit.volume_cuve_ar          = lv_spinbox_get_value(s_spinbox[7]);
            s_cfg_edit.sonde_hauteur_max_mm    = lv_spinbox_get_value(s_spinbox[8]);
            s_cfg_edit.sonde_offset_mm         = lv_spinbox_get_value(s_spinbox[9]);
            s_save_cb(&s_cfg_edit);
            ESP_LOGI(TAG, "Configuration sauvegardée.");
        }
        if (s_scr_main) lv_disp_load_scr(s_scr_main);
        break;
    }
}

/* Callbacks pour spinbox +/- */
static void evt_spinbox_inc(lv_event_t *e)
{
    lv_obj_t *sb = (lv_obj_t *)lv_event_get_user_data(e);
    lv_spinbox_increment(sb);
}
static void evt_spinbox_dec(lv_event_t *e)
{
    lv_obj_t *sb = (lv_obj_t *)lv_event_get_user_data(e);
    lv_spinbox_decrement(sb);
}

/* ====================================================================
 * CRÉATION — BARRE SUPÉRIEURE
 * ==================================================================== */
static void create_topbar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_add_style(bar, &sty_topbar, 0);
    lv_obj_set_size(bar, SCREEN_W, TOPBAR_H);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Status */
    s_lbl_status = lv_label_create(bar);
    lv_label_set_text(s_lbl_status, "INITIALISATION...");
    lv_obj_set_style_text_color(s_lbl_status, COL_RED, 0);
    lv_obj_set_style_text_font(s_lbl_status, &lv_font_montserrat_14, 0);

    /* Alerte cuve vide */
    s_lbl_alert = lv_label_create(bar);
    lv_label_set_text(s_lbl_alert, LV_SYMBOL_WARNING " CUVE VIDE");
    lv_obj_set_style_text_color(s_lbl_alert, COL_RED, 0);
    lv_obj_set_style_text_font(s_lbl_alert, &lv_font_montserrat_12, 0);
    lv_obj_add_flag(s_lbl_alert, LV_OBJ_FLAG_HIDDEN);

    /* Version */
    s_lbl_version = lv_label_create(bar);
    lv_label_set_text(s_lbl_version, "v2.6.5");
    lv_obj_set_style_text_color(s_lbl_version, lv_color_hex(0x444444), 0);
    lv_obj_set_style_text_font(s_lbl_version, &lv_font_montserrat_12, 0);

    /* Bouton Settings (engrenage) */
    lv_obj_t *btn_set = lv_btn_create(bar);
    lv_obj_set_size(btn_set, 40, 36);
    lv_obj_set_style_bg_opa(btn_set, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_set, 0, 0);
    lv_obj_set_style_shadow_width(btn_set, 0, 0);
    lv_obj_add_event_cb(btn_set, evt_cmd, LV_EVENT_CLICKED, (void *)CMD_SETTINGS);
    lv_obj_t *lbl = lv_label_create(btn_set);
    lv_label_set_text(lbl, LV_SYMBOL_SETTINGS);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
}

/* ====================================================================
 * CRÉATION — PANNEAU 1 : UNITÉ DE POMPAGE
 * ==================================================================== */
static void create_panel_pompage(lv_obj_t *parent)
{
    create_section_title(parent, "UNITE DE POMPAGE");

    /* Carte débit */
    lv_obj_t *card_debit = lv_obj_create(parent);
    lv_obj_remove_style_all(card_debit);
    lv_obj_add_style(card_debit, &sty_card, 0);
    lv_obj_set_size(card_debit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card_debit, LV_FLEX_FLOW_COLUMN);
    create_card_title(card_debit, "DEBIT INSTANTANE");
    s_bar_debit = create_gauge(card_debit, &s_lbl_debit, COL_GREEN, "-- L/min");

    /* Boutons */
    s_btn_pompe = create_btn_full(parent, &s_lbl_pompe, "POMPE ARRETEE",
                                   evt_cmd, (void *)CMD_POMPE);
    s_btn_v3v = create_btn_full(parent, &s_lbl_v3v, "SORTIE : BRASSAGE",
                                 evt_cmd, (void *)CMD_V3V);
    s_btn_phare_av = create_btn_full(parent, &s_lbl_phare_av, "PHARE AVANT",
                                      evt_cmd, (void *)CMD_PHARE_AV);
}

/* ====================================================================
 * CRÉATION — PANNEAU 2 : AUTOMATE
 * ==================================================================== */
static void create_panel_automate(lv_obj_t *parent)
{
    create_section_title(parent, "AUTOMATE");

    /* Carte transfert */
    lv_obj_t *card_tr = lv_obj_create(parent);
    lv_obj_remove_style_all(card_tr);
    lv_obj_add_style(card_tr, &sty_card, 0);
    lv_obj_set_size(card_tr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card_tr, LV_FLEX_FLOW_COLUMN);
    s_btn_auto_tr = create_btn_full(card_tr, &s_lbl_auto_tr, "LANCER TRANSFERT",
                                     evt_cmd, (void *)CMD_AUTO_TR);
    s_bar_tr = create_gauge(card_tr, &s_lbl_tr_vol, COL_BLUE, "-- / -- L");

    /* Carte brassage */
    lv_obj_t *card_br = lv_obj_create(parent);
    lv_obj_remove_style_all(card_br);
    lv_obj_add_style(card_br, &sty_card, 0);
    lv_obj_set_size(card_br, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card_br, LV_FLEX_FLOW_COLUMN);
    s_btn_auto_br = create_btn_full(card_br, &s_lbl_auto_br, "AUTO BRASSAGE",
                                     evt_cmd, (void *)CMD_AUTO_BR);
    s_bar_br = create_gauge(card_br, &s_lbl_br_time, COL_GREEN, "-- min");

    /* Arrêt d'urgence */
    s_btn_urgence = create_btn_full(parent, NULL, "ARRET D'URGENCE AUTO",
                                     evt_cmd, (void *)CMD_URGENCE);
    lv_obj_set_style_bg_color(s_btn_urgence, COL_DARK_RED, 0);
    lv_obj_set_style_border_color(s_btn_urgence, COL_DARK_RED_BDR, 0);
    lv_obj_set_style_border_width(s_btn_urgence, 1, 0);
    lv_obj_t *lbl_urg = lv_obj_get_child(s_btn_urgence, 0);
    if (lbl_urg) lv_obj_set_style_text_color(lbl_urg, lv_color_hex(0xff4444), 0);
}

/* ====================================================================
 * CRÉATION — PANNEAU 3 : VANNES & CUVE AR
 * ==================================================================== */
static void create_panel_vannes(lv_obj_t *parent)
{
    create_section_title(parent, "VANNES & PHARES AR");

    /* Carte niveau cuve */
    lv_obj_t *card_niv = lv_obj_create(parent);
    lv_obj_remove_style_all(card_niv);
    lv_obj_add_style(card_niv, &sty_card, 0);
    lv_obj_set_size(card_niv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card_niv, LV_FLEX_FLOW_COLUMN);
    create_card_title(card_niv, "NIVEAU CUVE ARRIERE");
    s_bar_niveau = create_gauge(card_niv, &s_lbl_niveau, COL_BLUE, "-- L");

    /* Carte vanne 2m */
    lv_obj_t *card_v2m = lv_obj_create(parent);
    lv_obj_remove_style_all(card_v2m);
    lv_obj_add_style(card_v2m, &sty_card, 0);
    lv_obj_set_size(card_v2m, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card_v2m, LV_FLEX_FLOW_COLUMN);
    create_card_title(card_v2m, "VANNE 2M");
    lv_obj_t *row_v2m = lv_obj_create(card_v2m);
    lv_obj_remove_style_all(row_v2m);
    lv_obj_set_size(row_v2m, lv_pct(100), VANNE_BTN_H);
    lv_obj_set_flex_flow(row_v2m, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row_v2m, 4, 0);
    s_btn_v2m[0] = create_btn_vanne(row_v2m, "FERMER", evt_cmd, (void *)CMD_V2M_F);
    s_btn_v2m[1] = create_btn_vanne(row_v2m, "STOP",   evt_cmd, (void *)CMD_V2M_S);
    s_btn_v2m[2] = create_btn_vanne(row_v2m, "OUVRIR", evt_cmd, (void *)CMD_V2M_O);

    /* Carte vanne bout de rampe */
    lv_obj_t *card_vbt = lv_obj_create(parent);
    lv_obj_remove_style_all(card_vbt);
    lv_obj_add_style(card_vbt, &sty_card, 0);
    lv_obj_set_size(card_vbt, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card_vbt, LV_FLEX_FLOW_COLUMN);
    create_card_title(card_vbt, "BOUT DE RAMPE");
    lv_obj_t *row_vbt = lv_obj_create(card_vbt);
    lv_obj_remove_style_all(row_vbt);
    lv_obj_set_size(row_vbt, lv_pct(100), VANNE_BTN_H);
    lv_obj_set_flex_flow(row_vbt, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row_vbt, 4, 0);
    s_btn_vbt[0] = create_btn_vanne(row_vbt, "FERMER", evt_cmd, (void *)CMD_VBT_F);
    s_btn_vbt[1] = create_btn_vanne(row_vbt, "STOP",   evt_cmd, (void *)CMD_VBT_S);
    s_btn_vbt[2] = create_btn_vanne(row_vbt, "OUVRIR", evt_cmd, (void *)CMD_VBT_O);

    /* Phare arrière */
    s_btn_phare_ar = create_btn_full(parent, &s_lbl_phare_ar, "PHARE ARRIERE",
                                      evt_cmd, (void *)CMD_PHARE_AR);
}

/* ====================================================================
 * CRÉATION — ÉCRAN PRINCIPAL
 * ==================================================================== */
static void create_main_screen(void)
{
    s_scr_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_main, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_scr_main, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(s_scr_main, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_scr_main, 0, 0);
    lv_obj_set_style_pad_gap(s_scr_main, 0, 0);

    /* Barre supérieure */
    create_topbar(s_scr_main);

    /* Zone principale : 3 panneaux côte à côte */
    lv_obj_t *main_area = lv_obj_create(s_scr_main);
    lv_obj_remove_style_all(main_area);
    lv_obj_set_size(main_area, SCREEN_W, MAIN_H);
    lv_obj_set_flex_flow(main_area, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(main_area, 0, 0);
    lv_obj_set_style_pad_column(main_area, 0, 0);

    /* Panneau 1 — Pompage (flex-grow: 2) */
    lv_obj_t *panel1 = lv_obj_create(main_area);
    lv_obj_remove_style_all(panel1);
    lv_obj_set_flex_grow(panel1, 1);
    lv_obj_set_height(panel1, lv_pct(100));
    lv_obj_set_style_bg_color(panel1, COL_BG, 0);
    lv_obj_set_style_bg_opa(panel1, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(panel1, PANEL_PAD, 0);
    lv_obj_set_flex_flow(panel1, LV_FLEX_FLOW_COLUMN);
    //lv_obj_set_style_pad_row(panel1, 12, 0);
    //Ça supprime le padding fixe et laisse LVGL répartir uniformément les boutons/jauges sur toute la hauteur.
    lv_obj_set_flex_align(panel1, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_color(panel1, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(panel1, 1, 0);
    lv_obj_set_style_border_side(panel1, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_scrollbar_mode(panel1, LV_SCROLLBAR_MODE_OFF);
    create_panel_pompage(panel1);

    /* Panneau 2 — Automate (flex-grow: 1, plus étroit) */
    lv_obj_t *panel2 = lv_obj_create(main_area);
    lv_obj_remove_style_all(panel2);
    lv_obj_set_flex_grow(panel2, 1);
    lv_obj_set_height(panel2, lv_pct(100));
    lv_obj_set_style_bg_color(panel2, COL_PANEL_MID, 0);
    lv_obj_set_style_bg_opa(panel2, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(panel2, PANEL_PAD, 0);
    lv_obj_set_flex_flow(panel2, LV_FLEX_FLOW_COLUMN);
    //lv_obj_set_style_pad_row(panel2, 12, 0);
    //Ça supprime le padding fixe et laisse LVGL répartir uniformément les boutons/jauges sur toute la hauteur.
    lv_obj_set_flex_align(panel2, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_color(panel2, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(panel2, 1, 0);
    lv_obj_set_style_border_side(panel2, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_scrollbar_mode(panel2, LV_SCROLLBAR_MODE_OFF);
    create_panel_automate(panel2);

    /* Panneau 3 — Vannes & Cuve AR (flex-grow: 2) */
    lv_obj_t *panel3 = lv_obj_create(main_area);
    lv_obj_remove_style_all(panel3);
    lv_obj_set_flex_grow(panel3, 1);
    lv_obj_set_height(panel3, lv_pct(100));
    lv_obj_set_style_bg_color(panel3, COL_BG, 0);
    lv_obj_set_style_bg_opa(panel3, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(panel3, PANEL_PAD, 0);
    lv_obj_set_flex_flow(panel3, LV_FLEX_FLOW_COLUMN);
    //lv_obj_set_style_pad_row(panel3, 12, 0);
    //Ça supprime le padding fixe et laisse LVGL répartir uniformément les boutons/jauges sur toute la hauteur.
    lv_obj_set_flex_align(panel3, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(panel3, LV_SCROLLBAR_MODE_OFF);
    create_panel_vannes(panel3);
}

/* ====================================================================
 * CRÉATION — PAGE SETTINGS
 * ==================================================================== */

typedef struct {
    const char *label;
    const char *unit;
    int32_t     min;
    int32_t     max;
    int32_t     step;
    uint8_t     digits;      /* Nombre de chiffres total */
    uint8_t     decimals;    /* Chiffres après la virgule (pour affichage) */
} setting_def_t;

static const setting_def_t s_settings_def[NB_SETTINGS] = {
    { "Volume transfert",       "L",       1,   9999, 10,  4, 0 },
    { "Facteur K debitmetre",   "x100",    1,   9999,  1,  4, 0 },  /* stocké ×100 */
    { "Seuil mini debit",       "x10 L/m", 1,    999,  1,  3, 0 },  /* stocké ×10 */
    { "Delai coupure",          "sec",     1,     60,  1,  2, 0 },
    { "Timeout vannes",         "sec",     5,    120,  5,  3, 0 },
    { "Brassage ON",            "sec",    10,   3600, 10,  4, 0 },
    { "Brassage OFF",           "sec",    10,   3600, 10,  4, 0 },
    { "Volume cuve AR",         "L",      50,   9999, 50,  4, 0 },
    { "Hauteur max sonde",      "mm",    100,   5000, 50,  4, 0 },
    { "Offset sonde",           "mm",      0,   1000, 10,  4, 0 },
};

static void create_settings_screen(void)
{
    s_scr_settings = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_settings, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_scr_settings, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(s_scr_settings, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_scr_settings, 0, 0);
    lv_obj_set_style_pad_gap(s_scr_settings, 0, 0);

    /* Barre supérieure settings */
    lv_obj_t *topbar = lv_obj_create(s_scr_settings);
    lv_obj_remove_style_all(topbar);
    lv_obj_add_style(topbar, &sty_topbar, 0);
    lv_obj_set_size(topbar, SCREEN_W, TOPBAR_H);
    lv_obj_set_flex_flow(topbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topbar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Bouton retour */
    lv_obj_t *btn_back = lv_btn_create(topbar);
    lv_obj_set_size(btn_back, 120, 36);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_add_event_cb(btn_back, evt_cmd, LV_EVENT_CLICKED, (void *)CMD_SETTINGS_BACK);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " RETOUR");
    lv_obj_center(lbl_back);
    lv_obj_set_style_text_color(lbl_back, COL_BLUE, 0);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);

    /* Titre */
    lv_obj_t *lbl_title = lv_label_create(topbar);
    lv_label_set_text(lbl_title, "CONFIGURATION");
    lv_obj_set_style_text_color(lbl_title, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);

    /* Placeholder pour symétrie */
    lv_obj_t *spacer = lv_obj_create(topbar);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 120, 1);

    /* Zone scrollable */
    lv_obj_t *scroll = lv_obj_create(s_scr_settings);
    lv_obj_remove_style_all(scroll);
    lv_obj_set_size(scroll, SCREEN_W, MAIN_H);
    lv_obj_set_style_bg_color(scroll, COL_BG, 0);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scroll, 20, 0);
    lv_obj_set_style_pad_row(scroll, 8, 0);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);

    /* Lignes de réglages */
    for (int i = 0; i < NB_SETTINGS; i++) {
        const setting_def_t *def = &s_settings_def[i];

        lv_obj_t *row = lv_obj_create(scroll);
        lv_obj_remove_style_all(row);
        lv_obj_add_style(row, &sty_card, 0);
        lv_obj_set_size(row, lv_pct(100), 56);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);  /* ← AJOUTER */

        /* Label du paramètre */
        lv_obj_t *lbl = lv_label_create(row);
        char label_txt[64];
        snprintf(label_txt, sizeof(label_txt), "%s (%s)", def->label, def->unit);
        lv_label_set_text(lbl, label_txt);
        lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);

        /* Groupe de contrôle : [-] spinbox [+] */
        lv_obj_t *ctrl = lv_obj_create(row);
        lv_obj_remove_style_all(ctrl);
        lv_obj_set_size(ctrl, 200, 44);
        lv_obj_set_flex_flow(ctrl, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(ctrl, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(ctrl, 4, 0);
        lv_obj_clear_flag(ctrl, LV_OBJ_FLAG_SCROLLABLE);  /* ← AJOUTER */

        /* Bouton [-] */
        lv_obj_t *btn_dec = lv_btn_create(ctrl);
        lv_obj_set_size(btn_dec, 44, 40);
        lv_obj_set_style_bg_color(btn_dec, COL_BTN_OFF, 0);
        lv_obj_set_style_radius(btn_dec, 4, 0);
        lv_obj_t *lbl_dec = lv_label_create(btn_dec);
        lv_label_set_text(lbl_dec, "-");
        lv_obj_center(lbl_dec);
        lv_obj_set_style_text_color(lbl_dec, COL_TEXT, 0);
        lv_obj_set_style_text_font(lbl_dec, &lv_font_montserrat_18, 0);

        /* Spinbox */
        lv_obj_t *sb = lv_spinbox_create(ctrl);
        lv_spinbox_set_range(sb, def->min, def->max);
        lv_spinbox_set_step(sb, def->step);
        lv_spinbox_set_digit_format(sb, def->digits, def->decimals);
        lv_obj_set_size(sb, 100, 40);
        lv_obj_set_style_bg_color(sb, lv_color_hex(0x121212), 0);
        lv_obj_set_style_border_color(sb, lv_color_hex(0x444444), 0);
        lv_obj_set_style_border_width(sb, 1, 0);
        lv_obj_set_style_text_color(sb, COL_WHITE, 0);
        lv_obj_set_style_text_font(sb, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_align(sb, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_all(sb, 6, 0);
        s_spinbox[i] = sb;

        /* Bouton [+] */
        lv_obj_t *btn_inc = lv_btn_create(ctrl);
        lv_obj_set_size(btn_inc, 44, 40);
        lv_obj_set_style_bg_color(btn_inc, COL_BTN_OFF, 0);
        lv_obj_set_style_radius(btn_inc, 4, 0);
        lv_obj_t *lbl_inc = lv_label_create(btn_inc);
        lv_label_set_text(lbl_inc, "+");
        lv_obj_center(lbl_inc);
        lv_obj_set_style_text_color(lbl_inc, COL_TEXT, 0);
        lv_obj_set_style_text_font(lbl_inc, &lv_font_montserrat_18, 0);

        /* Lier les boutons au spinbox */
        lv_obj_add_event_cb(btn_dec, evt_spinbox_dec, LV_EVENT_CLICKED, sb);
        lv_obj_add_event_cb(btn_inc, evt_spinbox_inc, LV_EVENT_CLICKED, sb);
    }

    /* Bouton ENREGISTRER */
    lv_obj_t *btn_save = lv_btn_create(scroll);
    lv_obj_set_size(btn_save, lv_pct(100), 56);
    lv_obj_set_style_bg_color(btn_save, COL_GREEN, 0);
    lv_obj_set_style_radius(btn_save, BTN_RADIUS, 0);
    lv_obj_add_event_cb(btn_save, evt_cmd, LV_EVENT_CLICKED, (void *)CMD_SETTINGS_SAVE);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "ENREGISTRER LA CONFIGURATION");
    lv_obj_center(lbl_save);
    lv_obj_set_style_text_color(lbl_save, COL_WHITE, 0);
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_16, 0);
}

/* ====================================================================
 * API PUBLIQUE — INITIALISATION
 * ==================================================================== */
void ecran_ui_creer(ecran_cmd_cb_t cmd_cb, ecran_save_cfg_cb_t save_cb)
{
    s_cmd_cb = cmd_cb;
    s_save_cb = save_cb;

    styles_init();
    create_main_screen();
    create_settings_screen();

    lv_disp_load_scr(s_scr_main);
    ESP_LOGI(TAG, "UI LVGL créée (1024×600, 3 panneaux).");
}

/* ====================================================================
 * API PUBLIQUE — MISES À JOUR
 * ==================================================================== */
void ecran_ui_update_avant(const etat_carte_avant_t *e)
{
    if (!e) return;
    char buf[64];

    /* --- Alerte cuve vide --- */
    if (e->debitmetre_ok && e->securite_cuve == SEC_CUVE_VIDE) {
        lv_obj_clear_flag(s_lbl_alert, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_lbl_alert, LV_OBJ_FLAG_HIDDEN);
    }

    /* --- Pompe --- */
    bool pompe_on = (e->pompe == POMPE_EN_MARCHE);
    bool cuve_vide = (e->debitmetre_ok && e->securite_cuve == SEC_CUVE_VIDE);
    if (cuve_vide && !pompe_on) {
        btn_set_color(s_btn_pompe, COL_RED, COL_WHITE);
        lv_label_set_text(s_lbl_pompe, "CUVE VIDE (REARMER)");
    } else if (pompe_on) {
        btn_set_color(s_btn_pompe, COL_GREEN, COL_WHITE);
        lv_label_set_text(s_lbl_pompe, "POMPE EN MARCHE");
    } else {
        btn_set_color(s_btn_pompe, COL_BTN_OFF, COL_TEXT);
        lv_label_set_text(s_lbl_pompe, "POMPE ARRETEE");
    }

    /* --- Débit --- */
    if (e->debitmetre_ok) {
        int pct = (int)((e->debit_instantane / 60.0f) * 100.0f);
        if (pct > 100) pct = 100;
        lv_bar_set_value(s_bar_debit, pct, LV_ANIM_ON);
        snprintf(buf, sizeof(buf), "%.1f L/min", e->debit_instantane);
        lv_label_set_text(s_lbl_debit, buf);
    } else {
        lv_bar_set_value(s_bar_debit, 0, LV_ANIM_OFF);
        lv_label_set_text(s_lbl_debit, "AV OFFLINE");
    }

    /* --- Vanne 3 voies --- */
    bool v3v_tr = (e->vanne_3v == V3V_TRANSFERT);
    btn_set_color(s_btn_v3v, v3v_tr ? COL_BLUE : COL_GREEN, COL_WHITE);
    lv_label_set_text(s_lbl_v3v, v3v_tr ? "SORTIE : TRANSFERT" : "SORTIE : BRASSAGE");

    /* --- Phare avant --- */
    btn_set_color(s_btn_phare_av, e->phares_avant ? COL_YELLOW : COL_BTN_OFF,
                  e->phares_avant ? lv_color_black() : COL_TEXT);

    /* --- Transfert auto --- */
    s_auto_tr_actif = (e->auto_transfert == AUTO_TR_EN_COURS);
    btn_set_color(s_btn_auto_tr, s_auto_tr_actif ? COL_BLUE : COL_BTN_OFF, COL_WHITE);
    lv_label_set_text(s_lbl_auto_tr, s_auto_tr_actif ? "ARRETER TRANSFERT" : "LANCER TRANSFERT");

    int tr_pct = 0;
    if (e->transfert_volume_cible > 0) {
        tr_pct = (int)(e->volume_session / (float)e->transfert_volume_cible * 100.0f);
        if (tr_pct > 100) tr_pct = 100;
    }
    lv_bar_set_value(s_bar_tr, tr_pct, LV_ANIM_ON);
    snprintf(buf, sizeof(buf), "%d / %lu L",
             (int)e->volume_session, (unsigned long)e->transfert_volume_cible);
    lv_label_set_text(s_lbl_tr_vol, buf);

    /* --- Brassage auto --- */
    s_auto_br_actif = (e->auto_brassage != AUTO_BR_INACTIF);
    btn_set_color(s_btn_auto_br, s_auto_br_actif ? COL_BLUE : COL_BTN_OFF, COL_WHITE);
    lv_label_set_text(s_lbl_auto_br, s_auto_br_actif ? "ARRETER BRASSAGE" : "AUTO BRASSAGE");

    lv_bar_set_value(s_bar_br, (int)e->brassage_pourcentage, LV_ANIM_ON);
    /* Temps restant en secondes → afficher M:SS */
    int secs = (int)e->brassage_temps_restant;
    int mins = secs / 60;
    int sec_r = secs % 60;
    snprintf(buf, sizeof(buf), "%s : %d:%02d", e->brassage_label, mins, sec_r);
    lv_label_set_text(s_lbl_br_time, buf);
}

void ecran_ui_update_arriere(const etat_carte_arriere_t *e)
{
    if (!e) return;
    char buf[64];

    /* --- Niveau cuve --- */
    if (e->sonde_niveau_ok) {
        /* niveau_cuve_arriere est en % — conversion en litres faite côté appelant si besoin */
        float litres = e->niveau_cuve_arriere * s_cfg_edit.volume_cuve_ar / 100.0f;
        int pct = (int)e->niveau_cuve_arriere;
        if (pct > 100) pct = 100;
        lv_bar_set_value(s_bar_niveau, pct, LV_ANIM_ON);

        /* Couleur selon le niveau */
        lv_color_t col = COL_BLUE;
        if (pct < 15) col = COL_RED;
        else if (pct < 30) col = COL_ORANGE;
        lv_obj_set_style_bg_color(s_bar_niveau, col, LV_PART_INDICATOR);

        snprintf(buf, sizeof(buf), "%d / %lu L",
                 (int)litres, (unsigned long)s_cfg_edit.volume_cuve_ar);
        lv_label_set_text(s_lbl_niveau, buf);
    } else {
        lv_bar_set_value(s_bar_niveau, 0, LV_ANIM_OFF);
        lv_label_set_text(s_lbl_niveau, "AR OFFLINE");
    }

    /* --- Vannes --- */
    typedef struct { lv_obj_t **btns; int etat; } vanne_ui_t;
    vanne_ui_t vannes[] = {
        { s_btn_v2m, (int)e->vanne_2m },
        { s_btn_vbt, (int)e->vanne_bout_rampe },
    };
    for (int v = 0; v < 2; v++) {
        /* Les 3 boutons : [0]=F, [1]=S, [2]=O */
        /* Enum : OUVERTE=1, FERMEE=2, ARRETEE=3, EN_COURS=4 — mapper vers boutons */
        lv_color_t cols[3]     = { COL_BTN_V_OFF, COL_BTN_V_OFF, COL_BTN_V_OFF };
        lv_color_t txt_cols[3] = { lv_color_hex(0x666666), lv_color_hex(0x666666), lv_color_hex(0x666666) };

        switch (vannes[v].etat) {
        case 1: /* OUVERTE */
            cols[2] = COL_GREEN; txt_cols[2] = COL_WHITE; break;
        case 2: /* FERMEE */
            cols[0] = COL_RED; txt_cols[0] = COL_WHITE; break;
        case 3: /* ARRETEE (STOP) */
            cols[1] = lv_color_hex(0x444444); txt_cols[1] = COL_WHITE; break;
        default: break;
        }
        for (int b = 0; b < 3; b++) {
            btn_set_color(vannes[v].btns[b], cols[b], txt_cols[b]);
        }
    }

    /* --- Phare arrière --- */
    btn_set_color(s_btn_phare_ar, e->phares_arriere ? COL_YELLOW : COL_BTN_OFF,
                  e->phares_arriere ? lv_color_black() : COL_TEXT);
}

void ecran_ui_update_reseau(const char *master, bool link_av, bool link_ar)
{
    if (!s_lbl_status) return;
    char buf[80];

    const char *master_lbl = (master && strcmp(master, "AV") == 0) ? "AV Master" : "AR Master";
    const char *other_link;
    if (master && strcmp(master, "AV") == 0) {
        other_link = link_ar ? "Link AR Actif" : "Link AR Perdu";
    } else {
        other_link = link_av ? "Link AV Actif" : "Link AV Perdu";
    }
    snprintf(buf, sizeof(buf), "%s / %s", master_lbl, other_link);
    lv_label_set_text(s_lbl_status, buf);
    lv_obj_set_style_text_color(s_lbl_status, (link_av || link_ar) ? COL_GREEN : COL_RED, 0);
}

void ecran_ui_set_config(const configuration_t *cfg)
{
    if (!cfg) return;
    s_cfg_edit = *cfg;

    if (s_spinbox[0]) lv_spinbox_set_value(s_spinbox[0], cfg->volume_transfert);
    if (s_spinbox[1]) lv_spinbox_set_value(s_spinbox[1], (int32_t)(cfg->facteur_k_debitmetre * 100.0f));
    if (s_spinbox[2]) lv_spinbox_set_value(s_spinbox[2], (int32_t)(cfg->seuil_debit_cuve_vide * 10.0f));
    if (s_spinbox[3]) lv_spinbox_set_value(s_spinbox[3], cfg->delai_detection_ms / 1000);
    if (s_spinbox[4]) lv_spinbox_set_value(s_spinbox[4], cfg->timeout_vanne_ms / 1000);
    if (s_spinbox[5]) lv_spinbox_set_value(s_spinbox[5], cfg->temps_brassage_on);
    if (s_spinbox[6]) lv_spinbox_set_value(s_spinbox[6], cfg->temps_brassage_off);
    if (s_spinbox[7]) lv_spinbox_set_value(s_spinbox[7], cfg->volume_cuve_ar);
    if (s_spinbox[8]) lv_spinbox_set_value(s_spinbox[8], cfg->sonde_hauteur_max_mm);
    if (s_spinbox[9]) lv_spinbox_set_value(s_spinbox[9], cfg->sonde_offset_mm);
}
