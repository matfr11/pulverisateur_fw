/**
 * @file ui_pulverisateur.c
 * @brief Interface LVGL pour la carte écran pulvérisateur.
 *
 * Layout 1024×600 en 3 colonnes :
 *   Gauche  (40%) : Unité de pompage
 *   Centre  (20%) : Automatismes
 *   Droite  (40%) : Vannes & Cuve arrière
 */
#include "ui_pulverisateur.h"
#include "mqtt_ecran.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "UI";

/* ====================================================================
 * COULEURS (identiques à la Web UI)
 * ==================================================================== */
#define COL_BG          lv_color_hex(0x121212)
#define COL_CARD        lv_color_hex(0x262626)
#define COL_CARD_BORDER lv_color_hex(0x383838)
#define COL_TOPBAR      lv_color_hex(0x1A1A1A)
#define COL_TEXT        lv_color_hex(0xFFFFFF)
#define COL_TEXT_DIM    lv_color_hex(0x777777)
#define COL_GREEN       lv_color_hex(0x2ECC71)
#define COL_RED         lv_color_hex(0xE74C3C)
#define COL_BLUE        lv_color_hex(0x3399FF)
#define COL_YELLOW      lv_color_hex(0xF1C40F)
#define COL_ORANGE      lv_color_hex(0xF39C12)
#define COL_BTN_OFF     lv_color_hex(0x333333)
#define COL_BTN_EMER    lv_color_hex(0x331111)
#define COL_CENTER_BG   lv_color_hex(0x151515)

/* ====================================================================
 * RÉFÉRENCES AUX ÉLÉMENTS DYNAMIQUES
 * ==================================================================== */
static struct {
    /* Status bar */
    lv_obj_t *lbl_status;
    lv_obj_t *lbl_alerte;

    /* Pompage */
    lv_obj_t *bar_debit;
    lv_obj_t *lbl_debit;
    lv_obj_t *btn_pompe;
    lv_obj_t *lbl_pompe;
    lv_obj_t *btn_v3v;
    lv_obj_t *lbl_v3v;
    lv_obj_t *btn_phare_av;
    lv_obj_t *lbl_phare_av;

    /* Automate */
    lv_obj_t *btn_transfert;
    lv_obj_t *lbl_transfert;
    lv_obj_t *bar_transfert;
    lv_obj_t *lbl_transfert_vol;
    lv_obj_t *btn_brassage;
    lv_obj_t *lbl_brassage;
    lv_obj_t *bar_brassage;
    lv_obj_t *lbl_brassage_info;

    /* Vannes */
    lv_obj_t *bar_niveau;
    lv_obj_t *lbl_niveau;
    lv_obj_t *btn_v2m[3];   /* F, S, O */
    lv_obj_t *btn_vbr[3];   /* F, S, O */
    lv_obj_t *btn_phare_ar;
    lv_obj_t *lbl_phare_ar;
} ui;

/* État local pour toggle */
static bool s_auto_tr = false;
static bool s_auto_br = false;

/* ====================================================================
 * STYLES
 * ==================================================================== */
static lv_style_t style_card;
static lv_style_t style_btn;
static lv_style_t style_btn_small;
static lv_style_t style_bar_bg;
static lv_style_t style_title;

static void styles_init(void)
{
    /* Carte */
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, COL_CARD);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_color(&style_card, COL_CARD_BORDER);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_radius(&style_card, 10);
    lv_style_set_pad_all(&style_card, 8);

    /* Bouton standard */
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, COL_BTN_OFF);
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_radius(&style_btn, 8);
    lv_style_set_text_color(&style_btn, COL_TEXT);
    lv_style_set_text_font(&style_btn, &lv_font_montserrat_18);
    lv_style_set_border_width(&style_btn, 0);

    /* Bouton petit (vannes) */
    lv_style_init(&style_btn_small);
    lv_style_set_bg_color(&style_btn_small, COL_BTN_OFF);
    lv_style_set_bg_opa(&style_btn_small, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_small, 4);
    lv_style_set_text_color(&style_btn_small, COL_TEXT_DIM);
    lv_style_set_text_font(&style_btn_small, &lv_font_montserrat_16);
    lv_style_set_border_width(&style_btn_small, 0);

    /* Fond de barre */
    lv_style_init(&style_bar_bg);
    lv_style_set_bg_color(&style_bar_bg, lv_color_hex(0x000000));
    lv_style_set_border_color(&style_bar_bg, lv_color_hex(0x444444));
    lv_style_set_border_width(&style_bar_bg, 1);
    lv_style_set_radius(&style_bar_bg, 5);

    /* Titre de section */
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, COL_TEXT);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_16);
    lv_style_set_text_letter_space(&style_title, 2);
}

/* ====================================================================
 * HELPERS
 * ==================================================================== */

/** Créer un titre de section avec ligne bleue */
static lv_obj_t *creer_titre(lv_obj_t *parent, const char *texte)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(cont, 2, 0);
    lv_obj_set_style_border_color(cont, COL_BLUE, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_pad_bottom(cont, 4, 0);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, texte);
    lv_obj_add_style(lbl, &style_title, 0);
    lv_obj_center(lbl);

    return cont;
}

/** Créer une jauge horizontale (bar + label superposé) */
static void creer_jauge(lv_obj_t *parent, lv_obj_t **bar_out, lv_obj_t **lbl_out,
                         lv_color_t couleur, int hauteur)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), hauteur + 4);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);

    lv_obj_t *bar = lv_bar_create(cont);
    lv_obj_set_size(bar, lv_pct(100), hauteur);
    lv_obj_center(bar);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_add_style(bar, &style_bar_bg, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, couleur, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 5, LV_PART_INDICATOR);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, "-- ");
    lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);

    *bar_out = bar;
    *lbl_out = lbl;
}

/** Créer un bouton pleine largeur */
static lv_obj_t *creer_bouton(lv_obj_t *parent, const char *texte,
                               lv_obj_t **lbl_out, int hauteur,
                               lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, lv_pct(100), hauteur);
    lv_obj_add_style(btn, &style_btn, 0);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, texte);
    lv_obj_center(lbl);

    if (lbl_out) *lbl_out = lbl;
    return btn;
}

/** Créer une carte (container avec fond) */
static lv_obj_t *creer_carte(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 4, 0);
    return card;
}

/** Helper pour changer la couleur d'un bouton */
static void btn_set_color(lv_obj_t *btn, lv_color_t bg, lv_color_t txt)
{
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_text_color(btn, txt, LV_PART_MAIN | LV_STATE_DEFAULT);
    /* Appliquer aussi au label enfant */
    lv_obj_t *child = lv_obj_get_child(btn, 0);
    if (child) lv_obj_set_style_text_color(child, txt, 0);
}

/* ====================================================================
 * CALLBACKS BOUTONS
 * ==================================================================== */

static void cb_pompe(lv_event_t *e) {
    (void)e;
    mqtt_ecran_cmd_avant("pompe_toggle", NULL);
}

static void cb_v3v(lv_event_t *e) {
    (void)e;
    mqtt_ecran_cmd_avant("v3v_toggle", NULL);
}

static void cb_phare_av(lv_event_t *e) {
    (void)e;
    mqtt_ecran_cmd_avant("phares_av_toggle", NULL);
}

static void cb_transfert(lv_event_t *e) {
    (void)e;
    mqtt_ecran_cmd_avant("auto_transfert", s_auto_tr ? "S" : "A");
}

static void cb_brassage(lv_event_t *e) {
    (void)e;
    mqtt_ecran_cmd_avant("auto_brassage", s_auto_br ? "S" : "A");
}

static void cb_urgence(lv_event_t *e) {
    (void)e;
    mqtt_ecran_arret_urgence();
}

static void cb_v2m_f(lv_event_t *e) { (void)e; mqtt_ecran_cmd_arriere("v2m_fermer", NULL); }
static void cb_v2m_s(lv_event_t *e) { (void)e; mqtt_ecran_cmd_arriere("v2m_stop", NULL); }
static void cb_v2m_o(lv_event_t *e) { (void)e; mqtt_ecran_cmd_arriere("v2m_ouvrir", NULL); }
static void cb_vbr_f(lv_event_t *e) { (void)e; mqtt_ecran_cmd_arriere("vbr_fermer", NULL); }
static void cb_vbr_s(lv_event_t *e) { (void)e; mqtt_ecran_cmd_arriere("vbr_stop", NULL); }
static void cb_vbr_o(lv_event_t *e) { (void)e; mqtt_ecran_cmd_arriere("vbr_ouvrir", NULL); }

static void cb_phare_ar(lv_event_t *e) {
    (void)e;
    mqtt_ecran_cmd_arriere("phares_ar_toggle", NULL);
}

/* ====================================================================
 * CRÉATION DE L'UI
 * ==================================================================== */

static void creer_status_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 1024, 38);
    lv_obj_set_style_bg_color(bar, COL_TOPBAR, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, 12, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui.lbl_status = lv_label_create(bar);
    lv_label_set_text(ui.lbl_status, "INITIALISATION...");
    lv_obj_set_style_text_color(ui.lbl_status, COL_RED, 0);
    lv_obj_set_style_text_font(ui.lbl_status, &lv_font_montserrat_14, 0);

    ui.lbl_alerte = lv_label_create(bar);
    lv_label_set_text(ui.lbl_alerte, "");
    lv_obj_set_style_text_color(ui.lbl_alerte, COL_RED, 0);
    lv_obj_set_style_text_font(ui.lbl_alerte, &lv_font_montserrat_14, 0);

    lv_obj_t *lbl_ver = lv_label_create(bar);
    lv_label_set_text(lbl_ver, "v2.6.5 ECRAN");
    lv_obj_set_style_text_color(lbl_ver, lv_color_hex(0x444444), 0);
    lv_obj_set_style_text_font(lbl_ver, &lv_font_montserrat_12, 0);
}

static void creer_panneau_pompage(lv_obj_t *parent)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 390, 562);
    lv_obj_set_style_bg_color(panel, COL_BG, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_border_side(panel, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, 6, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    creer_titre(panel, "UNITE DE POMPAGE");

    /* Jauge débit */
    lv_obj_t *card_debit = creer_carte(panel);
    lv_obj_t *lbl_title = lv_label_create(card_debit);
    lv_label_set_text(lbl_title, "Debit Instantane");
    lv_obj_set_style_text_color(lbl_title, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, 0);
    creer_jauge(card_debit, &ui.bar_debit, &ui.lbl_debit, COL_GREEN, 28);

    /* Boutons */
    ui.btn_pompe = creer_bouton(panel, "POMPE ARRETEE", &ui.lbl_pompe, 60, cb_pompe, NULL);
    ui.btn_v3v = creer_bouton(panel, "SORTIE : BRASSAGE", &ui.lbl_v3v, 60, cb_v3v, NULL);
    ui.btn_phare_av = creer_bouton(panel, "PHARE AVANT", &ui.lbl_phare_av, 55, cb_phare_av, NULL);
}

static void creer_panneau_automate(lv_obj_t *parent)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 224, 562);
    lv_obj_set_style_bg_color(panel, COL_CENTER_BG, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_border_side(panel, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, 6, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    creer_titre(panel, "AUTOMATE");

    /* Transfert */
    lv_obj_t *card_tr = creer_carte(panel);
    ui.btn_transfert = creer_bouton(card_tr, "LANCER TRANSFERT", &ui.lbl_transfert, 48, cb_transfert, NULL);
    creer_jauge(card_tr, &ui.bar_transfert, &ui.lbl_transfert_vol, COL_BLUE, 22);

    /* Brassage */
    lv_obj_t *card_br = creer_carte(panel);
    ui.btn_brassage = creer_bouton(card_br, "AUTO BRASSAGE", &ui.lbl_brassage, 48, cb_brassage, NULL);
    creer_jauge(card_br, &ui.bar_brassage, &ui.lbl_brassage_info, COL_GREEN, 22);

    /* Arrêt d'urgence */
    lv_obj_t *btn_urg = lv_btn_create(panel);
    lv_obj_set_size(btn_urg, lv_pct(100), 55);
    lv_obj_set_style_bg_color(btn_urg, COL_BTN_EMER, 0);
    lv_obj_set_style_bg_opa(btn_urg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn_urg, lv_color_hex(0x552222), 0);
    lv_obj_set_style_border_width(btn_urg, 1, 0);
    lv_obj_set_style_radius(btn_urg, 8, 0);
    lv_obj_add_event_cb(btn_urg, cb_urgence, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_urg = lv_label_create(btn_urg);
    lv_label_set_text(lbl_urg, "ARRET\nD'URGENCE");
    lv_obj_set_style_text_color(lbl_urg, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_text_font(lbl_urg, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(lbl_urg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_urg);
}

static void creer_groupe_vanne(lv_obj_t *parent, const char *titre,
                                lv_obj_t *btns[3],
                                lv_event_cb_t cb_f, lv_event_cb_t cb_s, lv_event_cb_t cb_o)
{
    lv_obj_t *card = creer_carte(parent);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, titre);
    lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

    lv_obj_t *row = lv_obj_create(card);
    lv_obj_set_size(row, lv_pct(100), 48);
    lv_obj_set_style_bg_color(row, COL_BG, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 3, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 4, 0);

    const char *labels[] = {"FERMER", "STOP", "OUVRIR"};
    lv_event_cb_t cbs[] = {cb_f, cb_s, cb_o};

    for (int i = 0; i < 3; i++) {
        btns[i] = lv_btn_create(row);
        lv_obj_set_flex_grow(btns[i], 1);
        lv_obj_set_height(btns[i], 40);
        lv_obj_add_style(btns[i], &style_btn_small, 0);
        lv_obj_add_event_cb(btns[i], cbs[i], LV_EVENT_CLICKED, NULL);
        lv_obj_t *l = lv_label_create(btns[i]);
        lv_label_set_text(l, labels[i]);
        lv_obj_center(l);
    }
}

static void creer_panneau_vannes(lv_obj_t *parent)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 390, 562);
    lv_obj_set_style_bg_color(panel, COL_BG, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, 6, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    creer_titre(panel, "VANNES & CUVE AR");

    /* Jauge niveau cuve */
    lv_obj_t *card_niv = creer_carte(panel);
    lv_obj_t *lbl_niv_title = lv_label_create(card_niv);
    lv_label_set_text(lbl_niv_title, "Niveau Cuve Arriere");
    lv_obj_set_style_text_color(lbl_niv_title, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_niv_title, &lv_font_montserrat_12, 0);
    creer_jauge(card_niv, &ui.bar_niveau, &ui.lbl_niveau, COL_BLUE, 28);

    /* Vannes */
    creer_groupe_vanne(panel, "Vanne 2m", ui.btn_v2m, cb_v2m_f, cb_v2m_s, cb_v2m_o);
    creer_groupe_vanne(panel, "Bout de rampe", ui.btn_vbr, cb_vbr_f, cb_vbr_s, cb_vbr_o);

    /* Phare arrière */
    ui.btn_phare_ar = creer_bouton(panel, "PHARE ARRIERE", &ui.lbl_phare_ar, 55, cb_phare_ar, NULL);
}

void ui_creer(void)
{
    styles_init();

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Barre de statut en haut */
    creer_status_bar(scr);

    /* Container pour les 3 panneaux */
    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_set_size(body, 1024, 562);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    creer_panneau_pompage(body);
    creer_panneau_automate(body);
    creer_panneau_vannes(body);

    ESP_LOGI(TAG, "UI créée (1024x600, 3 panneaux).");
}

/* ====================================================================
 * MISE À JOUR DE L'UI
 * ==================================================================== */

static void maj_vannes(lv_obj_t *btns[3], char etat)
{
    /* btns[0]=F, btns[1]=S, btns[2]=O */
    btn_set_color(btns[0], etat == 'F' ? COL_RED : COL_BTN_OFF,
                  etat == 'F' ? COL_TEXT : COL_TEXT_DIM);
    btn_set_color(btns[1], etat == 'S' ? lv_color_hex(0x444444) : COL_BTN_OFF,
                  etat == 'S' ? COL_TEXT : COL_TEXT_DIM);
    btn_set_color(btns[2], etat == 'O' ? COL_GREEN : COL_BTN_OFF,
                  etat == 'O' ? COL_TEXT : COL_TEXT_DIM);
}

void ui_rafraichir(const etat_systeme_t *e)
{
    char buf[64];

    /* ---- STATUS BAR ---- */
    if (!e->connecte) {
        lv_label_set_text(ui.lbl_status, "LIAISON PERDUE");
        lv_obj_set_style_text_color(ui.lbl_status, COL_RED, 0);
    } else {
        const char *master_lbl = (strcmp(e->master, "AV") == 0) ? "AV Master" : "AR Master";
        const char *link_lbl;
        if (strcmp(e->master, "AV") == 0) {
            link_lbl = e->link_arriere ? "Link AR Actif" : "Link AR Perdu";
        } else {
            link_lbl = e->link_avant ? "Link AV Actif" : "Link AV Perdu";
        }
        snprintf(buf, sizeof(buf), "%s / %s", master_lbl, link_lbl);
        lv_label_set_text(ui.lbl_status, buf);
        lv_obj_set_style_text_color(ui.lbl_status, COL_GREEN, 0);
    }

    /* Alerte cuve vide */
    if (e->avant.debitmetre_ok && e->avant.cuve_vide) {
        lv_label_set_text(ui.lbl_alerte, LV_SYMBOL_WARNING " CUVE VIDE");
    } else {
        lv_label_set_text(ui.lbl_alerte, "");
    }

    /* ---- DÉBIT ---- */
    if (e->avant.debitmetre_ok) {
        int pct = (int)((e->avant.debit_lpm / 60.0f) * 100.0f);
        if (pct > 100) pct = 100;
        lv_bar_set_value(ui.bar_debit, pct, LV_ANIM_ON);
        snprintf(buf, sizeof(buf), "%.1f L/min", e->avant.debit_lpm);
        lv_label_set_text(ui.lbl_debit, buf);
    } else {
        lv_bar_set_value(ui.bar_debit, 0, LV_ANIM_OFF);
        lv_label_set_text(ui.lbl_debit, "AV OFFLINE");
    }

    /* ---- POMPE ---- */
    if (e->avant.debitmetre_ok && e->avant.cuve_vide) {
        btn_set_color(ui.btn_pompe,
                      e->avant.pompe ? COL_GREEN : COL_RED,
                      COL_TEXT);
        lv_label_set_text(ui.lbl_pompe,
                          e->avant.pompe ? "REARMEMENT..." : "CUVE VIDE (REARMER)");
    } else {
        btn_set_color(ui.btn_pompe,
                      e->avant.pompe ? COL_GREEN : COL_BTN_OFF,
                      COL_TEXT);
        lv_label_set_text(ui.lbl_pompe,
                          e->avant.pompe ? "POMPE EN MARCHE" : "POMPE ARRETEE");
    }

    /* ---- VANNE 3V ---- */
    btn_set_color(ui.btn_v3v,
                  e->avant.vanne_transfert ? COL_BLUE : COL_GREEN,
                  COL_TEXT);
    lv_label_set_text(ui.lbl_v3v,
                      e->avant.vanne_transfert ? "SORTIE : TRANSFERT" : "SORTIE : BRASSAGE");

    /* ---- PHARE AVANT ---- */
    btn_set_color(ui.btn_phare_av,
                  e->avant.phares_avant ? COL_YELLOW : COL_BTN_OFF,
                  e->avant.phares_avant ? lv_color_hex(0x000000) : COL_TEXT);

    /* ---- TRANSFERT AUTO ---- */
    s_auto_tr = e->avant.auto_transfert;
    btn_set_color(ui.btn_transfert,
                  s_auto_tr ? COL_BLUE : COL_BTN_OFF, COL_TEXT);
    lv_label_set_text(ui.lbl_transfert,
                      s_auto_tr ? "ARRETER TRANSFERT" : "LANCER TRANSFERT");

    if (e->avant.transfert_cible > 0) {
        int pct_tr = (int)(e->avant.volume_session / (float)e->avant.transfert_cible * 100.0f);
        if (pct_tr > 100) pct_tr = 100;
        lv_bar_set_value(ui.bar_transfert, pct_tr, LV_ANIM_ON);
        snprintf(buf, sizeof(buf), "%d / %lu L",
                 (int)e->avant.volume_session,
                 (unsigned long)e->avant.transfert_cible);
    } else {
        lv_bar_set_value(ui.bar_transfert, 0, LV_ANIM_OFF);
        snprintf(buf, sizeof(buf), "-- / -- L");
    }
    lv_label_set_text(ui.lbl_transfert_vol, buf);

    /* ---- BRASSAGE AUTO ---- */
    s_auto_br = e->avant.auto_brassage;
    btn_set_color(ui.btn_brassage,
                  s_auto_br ? COL_BLUE : COL_BTN_OFF, COL_TEXT);
    lv_label_set_text(ui.lbl_brassage,
                      s_auto_br ? "ARRETER BRASSAGE" : "AUTO BRASSAGE");

    lv_bar_set_value(ui.bar_brassage, (int)e->avant.brassage_pourcentage, LV_ANIM_ON);
    int br_min = (int)(e->avant.brassage_temps_restant / 60.0f);
    int br_sec = (int)e->avant.brassage_temps_restant % 60;
    snprintf(buf, sizeof(buf), "%s : %d:%02d", e->avant.brassage_label, br_min, br_sec);
    lv_label_set_text(ui.lbl_brassage_info, buf);

    /* ---- NIVEAU CUVE AR ---- */
    if (e->arriere.sonde_ok && e->arriere.niveau_ar_max > 0) {
        int pct_niv = (int)(e->arriere.niveau_ar / (float)e->arriere.niveau_ar_max * 100.0f);
        if (pct_niv > 100) pct_niv = 100;
        lv_bar_set_value(ui.bar_niveau, pct_niv, LV_ANIM_ON);

        /* Couleur selon niveau */
        lv_color_t col = pct_niv < 15 ? COL_RED : (pct_niv < 30 ? COL_ORANGE : COL_BLUE);
        lv_obj_set_style_bg_color(ui.bar_niveau, col, LV_PART_INDICATOR);

        snprintf(buf, sizeof(buf), "%d / %lu L",
                 (int)e->arriere.niveau_ar,
                 (unsigned long)e->arriere.niveau_ar_max);
        lv_label_set_text(ui.lbl_niveau, buf);
    } else {
        lv_bar_set_value(ui.bar_niveau, 0, LV_ANIM_OFF);
        lv_label_set_text(ui.lbl_niveau, "AR OFFLINE");
    }

    /* ---- VANNES ---- */
    maj_vannes(ui.btn_v2m, e->arriere.vanne_2m);
    maj_vannes(ui.btn_vbr, e->arriere.vanne_bdr);

    /* ---- PHARE AR ---- */
    btn_set_color(ui.btn_phare_ar,
                  e->arriere.phares_arriere ? COL_YELLOW : COL_BTN_OFF,
                  e->arriere.phares_arriere ? lv_color_hex(0x000000) : COL_TEXT);
}
