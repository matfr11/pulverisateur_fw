/**
 * @file gestion_web_ui.c
 * @brief Serveur HTTP embarqué – pages de contrôle et configuration.
 *
 * ADAPTATIONS depuis la Web UI Arduino existante :
 *
 * 1. SERVEUR : esp_http_server remplace le WebServer Arduino
 *
 * 2. COMMANDES : Avant, les boutons appelaient directement les fonctions
 *    (ex: fetch('/pT')). Maintenant, les boutons appellent /api/cmd qui
 *    publie sur MQTT. Les cartes cibles réagissent aux commandes MQTT.
 *
 * 3. ÉTAT : Le endpoint /status agrège les états reçus via MQTT
 *    des cartes AVANT et ARRIÈRE dans un JSON unique.
 *
 * 4. SETTINGS : La page settings envoie la nouvelle configuration
 *    via MQTT (topic mise_a_jour) au lieu d'un appel direct.
 *
 * Le HTML/CSS/JS est quasi-identique à l'existant (common_ui.h, ui_main.h).
 * Les adaptations sont minimales côté frontend : seuls les endpoints changent.
 */
#include "gestion_web_ui.h"
#include "protocole_mqtt.h"
#include "mqtt_topics.h"
#include "gestion_configuration.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "WEB_UI";
static httpd_handle_t s_serveur = NULL;

/* État local mis à jour via MQTT */
static etat_carte_avant_t   s_etat_avant = {0};
static etat_carte_arriere_t s_etat_arriere = {0};
static configuration_t      s_config_courante = CONFIG_DEFAUT;

/* ====================================================================
 * CSS COMMUN (adapté de common_ui.h)
 * ==================================================================== */
static const char CSS_COMMUN[] =
"<style>"
"body{margin:0;padding:0;background:#121212;color:white;font-family:sans-serif;"
"height:100vh;width:100vw;overflow:hidden;display:flex;flex-direction:column}"
".top-bar{display:flex;align-items:center;justify-content:space-between;"
"background:#1a1a1a;padding:0 12px;border-bottom:1px solid #333;height:38px}"
"#status-tag{font-size:.65rem;font-weight:bold;padding:3px 8px;border-radius:4px}"
".ver-info{font-size:.6rem;color:#444}"
".settings-icon{color:#666;text-decoration:none;font-size:1.1rem}"
".online{background:#1a592e;color:#2ecc71}"
".offline{background:#591a1a;color:#e74c3c}"
".dashboard{display:flex;flex:1;width:100%;overflow-x:auto;scroll-snap-type:x mandatory;"
"scroll-behavior:smooth}"
".slide{flex:0 0 100vw;height:100%;scroll-snap-align:start;box-sizing:border-box;"
"padding:8px;background:#1a1a1a;overflow-y:auto}"
"h2{color:#fff;text-align:center;font-size:1rem;margin:4px 0 10px 0;"
"text-transform:uppercase;letter-spacing:1.5px;border-bottom:2px solid #39f;"
"width:100%;padding-bottom:3px}"
".control-card{background:#262626;border-radius:10px;padding:6px 8px;"
"margin-bottom:6px;border:1px solid #383838}"
".card-title{font-size:.65rem;color:#777;margin-bottom:3px;text-transform:uppercase;"
"text-align:center}"
".h-gauge-container{width:100%;height:20px;background:#000;border-radius:5px;"
"border:1px solid #444;position:relative;overflow:hidden;margin-bottom:4px}"
".h-gauge-fill{height:100%;width:0%;transition:width .5s ease;position:absolute}"
".h-gauge-text{position:absolute;width:100%;text-align:center;line-height:20px;"
"font-weight:bold;z-index:2;color:white;text-shadow:1px 1px 2px black;font-size:.75rem}"
".btn-full{width:100%;padding:14px;border-radius:8px;border:none;background:#333;"
"color:#eee;font-weight:bold;cursor:pointer;margin-bottom:4px;font-size:.8rem}"
".btn-group{display:flex;gap:4px;background:#121212;padding:3px;border-radius:6px}"
".btn-v{flex:1;padding:12px 2px;border:none;border-radius:4px;background:#333;"
"color:#666;font-weight:bold;cursor:pointer;font-size:.75rem}"
".active-green{background:#2ecc71!important;color:white!important}"
".active-red{background:#e74c3c!important;color:white!important}"
".active-blue{background:#39f!important;color:white!important}"
".active-yellow{background:#f1c40f!important;color:black!important}"
".active-off{background:#444!important;color:white!important}"
".alert-mini{background:#e74c3c;color:white;padding:1px 6px;border-radius:4px;"
"font-size:.65em;font-weight:bold;animation:blink 1s infinite;margin-left:auto;"
"margin-right:5px}"
"@keyframes blink{50%{opacity:.3}}"
".blink-border{border:2px solid #e74c3c;animation:blink 1s infinite}"
"@media(orientation:landscape){.dashboard{overflow-x:hidden!important;"
"scroll-snap-type:none!important;display:flex!important}"
".slide{height:100%!important;padding:5px!important;border-right:1px solid #333}"
".slide:nth-child(1),.slide:nth-child(3){flex:2 1 40%}"
".slide:nth-child(2){flex:1 1 20%;min-width:180px;background:#151515!important}"
".slide:nth-child(2) .btn-full{font-size:.7rem;padding:10px 5px}"
"h2{font-size:.85rem;margin-bottom:5px}}"
"</style>";

/* ====================================================================
 * PAGE PRINCIPALE (adaptée de ui_main.h)
 *
 * CHANGEMENTS vs Arduino :
 *   - cmd('p') → api('avant','pompe_toggle')
 *   - api('tr','A') → api('avant','auto_transfert','A')
 *   - fetch('/status') reste identique (même format JSON)
 * ==================================================================== */
static const char PAGE_PART1[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>";

static const char PAGE_PART2[] =
"</head><body>"
"<div class='top-bar'>"
"<div id='status-tag' class='offline'>INITIALISATION...</div>"
"<div id='av-empty-alert' class='alert-mini' style='display:none'>&#9888; CUVE VIDE</div>"
"<div class='ver-info'>v3.0.0 (MQTT)</div>"
"<a href='/settings' class='settings-icon'>&#9881;</a>"
"</div>"
"<div class='dashboard'>"

/* Slide 1 : Unité de pompage */
"<div class='slide'>"
"<h2>UNIT&Eacute; DE POMPAGE</h2>"
"<div class='control-card'>"
"<div class='card-title'>D&eacute;bit Instantan&eacute;</div>"
"<div class='h-gauge-container'>"
"<div id='jFillFlow' class='h-gauge-fill' style='background:#2ecc71'></div>"
"<div id='jTextFlow' class='h-gauge-text'>-- L/min</div>"
"</div></div>"
"<button id='p_btn' class='btn-full' onclick=\"api('avant','pompe_toggle')\">CHARGEMENT...</button>"
"<button id='v_btn' class='btn-full' onclick=\"api('avant','v3v_toggle')\">BRASSAGE / TRANSFERT</button>"
"<button id='l_btn' class='btn-full' onclick=\"api('avant','phares_av_toggle')\">PHARE AVANT</button>"
"</div>"

/* Slide 2 : Automate */
"<div class='slide' style='background:#1d1d1d;border-radius:15px;padding:10px'>"
"<h2>AUTOMATE</h2>"
"<div class='control-card'>"
"<button id='mode_tr' class='btn-full' onclick=\"toggleTr()\">LANCER TRANSFERT</button>"
"<div class='h-gauge-container'>"
"<div id='jFillTr' class='h-gauge-fill' style='background:#39f'></div>"
"<div id='jTextTr' class='h-gauge-text'>-- / -- L</div>"
"</div></div>"
"<div class='control-card'>"
"<button id='mode_br' class='btn-full' onclick=\"toggleBr()\">AUTO BRASSAGE</button>"
"<div class='h-gauge-container'>"
"<div id='jFillBr' class='h-gauge-fill' style='background:#2ecc71'></div>"
"<div id='jTextBr' class='h-gauge-text'>-- min</div>"
"</div></div>"
"<button class='btn-full' onclick=\"api('urgence','stop')\" "
"style='background:#331111;color:#f44;border:1px solid #522;font-weight:bold'>"
"ARR&Ecirc;T D'URGENCE</button>"
"</div>"

/* Slide 3 : Vannes & Phares AR */
"<div class='slide'>"
"<h2>VANNES &amp; PHARES AR</h2>"
"<div class='control-card'>"
"<div class='card-title'>Niveau Cuve Arri&egrave;re</div>"
"<div class='h-gauge-container'>"
"<div id='jFillNiv' class='h-gauge-fill' style='background:#3399ff'></div>"
"<div id='jTextNiv' class='h-gauge-text'>-- %</div>"
"</div></div>"
"<div class='control-card'>"
"<div class='card-title'>Vanne 2m</div>"
"<div class='btn-group'>"
"<button id='v2m_F' class='btn-v' onclick=\"api('arriere','v2m_fermer')\">FERMER</button>"
"<button id='v2m_S' class='btn-v' onclick=\"api('arriere','v2m_stop')\">STOP</button>"
"<button id='v2m_O' class='btn-v' onclick=\"api('arriere','v2m_ouvrir')\">OUVRIR</button>"
"</div></div>"
"<div class='control-card'>"
"<div class='card-title'>Bout de rampe</div>"
"<div class='btn-group'>"
"<button id='vbt_F' class='btn-v' onclick=\"api('arriere','vbr_fermer')\">FERMER</button>"
"<button id='vbt_S' class='btn-v' onclick=\"api('arriere','vbr_stop')\">STOP</button>"
"<button id='vbt_O' class='btn-v' onclick=\"api('arriere','vbr_ouvrir')\">OUVRIR</button>"
"</div></div>"
"<button id='li_btn' class='btn-full' onclick=\"api('arriere','phares_ar_toggle')\">PHARE ARRI&Egrave;RE</button>"
"</div>"

"</div>" /* fin dashboard */

"<script>"
/* FONCTION API UNIVERSELLE
 * Remplace les anciens cmd() et api() par une seule fonction
 * qui publie les commandes via le serveur HTTP → MQTT */
"function api(cible,cmd,val){"
"  var url='/api/cmd?cible='+cible+'&cmd='+cmd;"
"  if(val) url+='&val='+val;"
"  fetch(url);"
"}"
"var trActif=false,brActif=false;"
"function toggleTr(){api('avant','auto_transfert',trActif?'S':'A');}"
"function toggleBr(){api('avant','auto_brassage',brActif?'S':'A');}"
/* RAFRAÎCHISSEMENT DE L'ÉTAT (identique à l'existant) */
"function refresh(){"
"  fetch('/status').then(r=>r.json()).then(d=>{"
"    var st=document.getElementById('status-tag');"
"    st.className='online';st.innerText='SYSTEME CONNECTE';"
"    var alertBox=document.getElementById('av-empty-alert');"
"    var pBtn=document.getElementById('p_btn');"
"    if(d.av_ok&&d.av_vide){"
"      alertBox.style.display='inline-block';"
"      pBtn.className=d.p?'btn-full active-green':'btn-full active-red blink-border';"
"      pBtn.innerText=d.p?'REARMEMENT...':'CUVE VIDE (REARMER)';"
"    }else{"
"      alertBox.style.display='none';"
"      pBtn.className=d.p?'btn-full active-green':'btn-full';"
"      pBtn.innerText=d.p?'POMPE EN MARCHE':'POMPE ARRETEE';"
"    }"
"    var fillFlow=document.getElementById('jFillFlow');"
"    var textFlow=document.getElementById('jTextFlow');"
"    if(d.av_ok){"
"      fillFlow.style.width=Math.min((d.av_flow/60)*100,100)+'%';"
"      textFlow.innerText=d.av_flow.toFixed(1)+' L/min';"
"    }else{fillFlow.style.width='0%';textFlow.innerText='AV OFFLINE';}"
"    var v3v=document.getElementById('v_btn');"
"    v3v.className=d.v?'btn-full active-blue':'btn-full active-green';"
"    v3v.innerText=d.v?'SORTIE : TRANSFERT':'SORTIE : BRASSAGE';"
"    document.getElementById('l_btn').className=d.l?'btn-full active-yellow':'btn-full';"
"    var liAR=document.getElementById('li_btn');"
"    if(liAR) liAR.className=d.li?'btn-full active-yellow':'btn-full';"
"    document.getElementById('mode_tr').className=d.m_tr?'btn-full active-blue':'btn-full';"
"    trActif=d.m_tr;"
"    document.getElementById('mode_tr').innerText=d.m_tr?'ARRETER TRANSFERT':'LANCER TRANSFERT';"
"    document.getElementById('jFillTr').style.width="
"      Math.min((d.session_vol/d.tr_target*100),100)+'%';"
"    document.getElementById('jTextTr').innerText="
"      Math.round(d.session_vol)+' / '+d.tr_target+' L';"
"    document.getElementById('mode_br').className=d.m_br?'btn-full active-blue':'btn-full';"
"    brActif=d.m_br;"
"    document.getElementById('mode_br').innerText=d.m_br?'ARRETER BRASSAGE':'AUTO BRASSAGE';"
"    var bm=Math.floor(d.br_rem/60);var bs=Math.floor(d.br_rem%60);"
"    document.getElementById('jTextBr').innerText=d.br_label+' : '+bm+':'+(bs<10?'0':'')+bs;"
"    document.getElementById('jFillBr').style.width=d.br_pct+'%';"
"    ['v2m','vbt'].forEach(function(v){"
"      var oEl=document.getElementById(v+'_O');"
"      var sEl=document.getElementById(v+'_S');"
"      var fEl=document.getElementById(v+'_F');"
"      if(oEl) oEl.className='btn-v '+(d[v]=='O'?'active-green':'');"
"      if(sEl) sEl.className='btn-v '+(d[v]=='S'?'active-off':'');"
"      if(fEl) fEl.className='btn-v '+(d[v]=='F'?'active-red':'');"
"    });"
"    var fillNiv=document.getElementById('jFillNiv');"
"    var textNiv=document.getElementById('jTextNiv');"
"    if(d.sonde_ok){"
"      fillNiv.style.width=Math.min(d.niveau_ar,100)+'%';"
"      fillNiv.style.background=d.niveau_ar<15?'#e74c3c':d.niveau_ar<30?'#f39c12':'#3399ff';"
"      textNiv.innerText=Math.round(d.niveau_ar)+' %';"
"    }else{fillNiv.style.width='0%';textNiv.innerText='AR OFFLINE';}"
"  }).catch(function(){"
"    document.getElementById('status-tag').className='offline';"
"    document.getElementById('status-tag').innerText='LIAISON PERDUE';"
"  });"
"}"
"setInterval(refresh,300);"
"</script></body></html>";

/* ====================================================================
 * HANDLERS HTTP
 * ==================================================================== */

/** GET / → Page principale */
static esp_err_t handler_page_principale(httpd_req_t *req)
{
    /* Construire la page avec le CSS injecté */
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, PAGE_PART1, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, CSS_COMMUN, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, PAGE_PART2, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/** GET /status → JSON agrégé des états AVANT + ARRIÈRE */
static esp_err_t handler_status(httpd_req_t *req)
{
    cJSON *json = cJSON_CreateObject();

    /* État carte AVANT */
    cJSON_AddBoolToObject(json, "p", s_etat_avant.pompe == POMPE_EN_MARCHE);
    cJSON_AddBoolToObject(json, "v", s_etat_avant.vanne_3v == V3V_TRANSFERT);
    cJSON_AddBoolToObject(json, "l", s_etat_avant.phares_avant);
    cJSON_AddBoolToObject(json, "av_ok", s_etat_avant.debitmetre_ok);
    cJSON_AddNumberToObject(json, "av_flow", s_etat_avant.debit_instantane);
    cJSON_AddBoolToObject(json, "av_vide", s_etat_avant.securite_cuve == SEC_CUVE_VIDE);
    cJSON_AddNumberToObject(json, "session_vol", s_etat_avant.volume_session);
    cJSON_AddBoolToObject(json, "m_tr", s_etat_avant.auto_transfert == AUTO_TR_EN_COURS);
    cJSON_AddNumberToObject(json, "tr_target", s_etat_avant.transfert_volume_cible);
    cJSON_AddBoolToObject(json, "m_br", s_etat_avant.auto_brassage != AUTO_BR_INACTIF);
    cJSON_AddStringToObject(json, "br_label", s_etat_avant.brassage_label);
    cJSON_AddNumberToObject(json, "br_rem", s_etat_avant.brassage_temps_restant);
    cJSON_AddNumberToObject(json, "br_pct", s_etat_avant.brassage_pourcentage);

    /* État carte ARRIÈRE */
    const char *map_vanne[] = {"?", "O", "F", "S", "T"};
    int idx_v2m = (int)s_etat_arriere.vanne_2m;
    int idx_vbt = (int)s_etat_arriere.vanne_bout_rampe;
    cJSON_AddStringToObject(json, "v2m",
        (idx_v2m >= 0 && idx_v2m < 5) ? map_vanne[idx_v2m] : "?");
    cJSON_AddStringToObject(json, "vbt",
        (idx_vbt >= 0 && idx_vbt < 5) ? map_vanne[idx_vbt] : "?");
    cJSON_AddBoolToObject(json, "li", s_etat_arriere.phares_arriere);
        /* niveau cuve arriere*/
    cJSON_AddNumberToObject(json, "niveau_ar", s_etat_arriere.niveau_cuve_arriere);
    cJSON_AddBoolToObject(json, "sonde_ok", s_etat_arriere.sonde_niveau_ok);
    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, str, strlen(str));
    free(str);
    return ESP_OK;
}

/**
 * GET /api/cmd?cible=avant&cmd=pompe_toggle&val=A
 *
 * Route les commandes vers MQTT selon la cible.
 */
static esp_err_t handler_api_cmd(httpd_req_t *req)
{
    char query[128] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));

    char cible[16] = {0};
    char cmd[32] = {0};
    char val[16] = {0};

    httpd_query_key_value(query, "cible", cible, sizeof(cible));
    httpd_query_key_value(query, "cmd", cmd, sizeof(cmd));
    httpd_query_key_value(query, "val", val, sizeof(val));

    ESP_LOGI(TAG, "CMD HTTP: cible=%s cmd=%s val=%s", cible, cmd, val);

    if (strcmp(cible, "avant") == 0) {
        mqtt_publier_commande_avant(cmd, val[0] ? val : NULL);
    } else if (strcmp(cible, "arriere") == 0) {
        mqtt_publier_commande_arriere(cmd, val[0] ? val : NULL);
    } else if (strcmp(cible, "urgence") == 0) {
        mqtt_publier_arret_urgence();
    }

    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

/** GET /settings → Page de configuration */
static esp_err_t handler_page_settings(httpd_req_t *req)
{
    /* Page settings simplifiée – même structure que ui_settings.h */
    char *page = malloc(10240);
    if (!page) return ESP_ERR_NO_MEM;

    int len = snprintf(page, 10240,
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>"
        "%s"
        "<style>"
        ".setting-row{display:flex;align-items:center;justify-content:space-between;"
        "padding:12px 0;border-bottom:1px solid #333}"
        "input[type='number']{background:#121212;border:1px solid #444;color:white;"
        "padding:8px;border-radius:5px;width:90px;text-align:center;font-size:1rem}"
        "label{font-size:.95rem;color:#ccc}"
        ".unit{font-size:.75rem;color:#666;margin-left:5px}"
        "h2{color:#39f;font-size:1.1rem;margin-top:20px;border-left:3px solid #39f;"
        "padding-left:10px;border-bottom:none;text-align:left}"
        "</style></head><body>"
        "<div class='top-bar'>"
        "<a href='/' style='color:#39f;text-decoration:none;font-weight:bold'>&#9664; RETOUR</a>"
        "<div class='ver-info'>CONFIG v3.0.0</div><div></div></div>"
        "<div style='padding:15px;overflow-y:auto;flex:1'>"
        "<h2>TRANSFERT AUTOMATIQUE</h2>"
        "<div class='control-card'>"
        "<div class='setting-row'><label>Volume <span class='unit'>(L)</span></label>"
        "<input type='number' id='t_tgt' value='%u'></div>"
        "<div class='setting-row'><label>Facteur K</label>"
        "<input type='number' step='0.1' id='k_fact' value='%.1f'></div></div>"
        "<h2>S&Eacute;CURIT&Eacute; D&Eacute;BIT</h2>"
        "<div class='control-card'>"
        "<div class='setting-row'><label>Seuil mini <span class='unit'>(L/min)</span></label>"
        "<input type='number' step='0.1' id='e_flow' value='%.1f'></div>"
        "<div class='setting-row'><label>D&eacute;lai <span class='unit'>(ms)</span></label>"
        "<input type='number' id='e_out' value='%u'></div>"
        "<div class='setting-row'><label>Timeout Vannes <span class='unit'>(ms)</span></label>"
        "<input type='number' id='v_timeout' value='%u'></div></div>"
        "<h2>CYCLES DE BRASSAGE</h2>"
        "<div class='control-card'>"
        "<div class='setting-row'><label>Marche <span class='unit'>(sec)</span></label>"
        "<input type='number' id='br_on' value='%u'></div>"
        "<div class='setting-row'><label>Repos <span class='unit'>(sec)</span></label>"
        "<input type='number' id='br_off' value='%u'></div></div>"
        "<br><button class='btn-full active-green' onclick='saveSettings()'>"
        "ENREGISTRER</button></div>"
        "<script>"
        "function saveSettings(){"
        "  var cfg={"
        "    volume_transfert:+document.getElementById('t_tgt').value,"
        "    facteur_k:+document.getElementById('k_fact').value,"
        "    seuil_debit:+document.getElementById('e_flow').value,"
        "    delai_detection:+document.getElementById('e_out').value,"
        "    timeout_vanne:+document.getElementById('v_timeout').value,"
        "    temps_on:+document.getElementById('br_on').value,"
        "    temps_off:+document.getElementById('br_off').value"
        "  };"
        "  fetch('/api/save_config',{method:'POST',"
        "    headers:{'Content-Type':'application/json'},"
        "    body:JSON.stringify(cfg)"
        "  }).then(r=>{if(r.ok)alert('OK');else alert('Erreur');})"
        "    .catch(e=>alert('Erreur: '+e));"
        "}"
        "</script></body></html>",
        CSS_COMMUN,
        (unsigned)s_config_courante.volume_transfert,
        s_config_courante.facteur_k_debitmetre,
        s_config_courante.seuil_debit_cuve_vide,
        (unsigned)s_config_courante.delai_detection_ms,
        (unsigned)s_config_courante.timeout_vanne_ms,
        (unsigned)s_config_courante.temps_brassage_on,
        (unsigned)s_config_courante.temps_brassage_off
    );

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, page, len);
    free(page);
    return ESP_OK;
}

/** POST /api/save_config → Sauvegarde la configuration via MQTT */
static esp_err_t handler_save_config(httpd_req_t *req)
{
    char body[512] = {0};
    int ret = httpd_req_recv(req, body, sizeof(body) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }

    cJSON *json = cJSON_Parse(body);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    /* Mettre à jour la configuration locale */
    cJSON *item;
    item = cJSON_GetObjectItem(json, "volume_transfert");
    if (item) s_config_courante.volume_transfert = item->valueint;
    item = cJSON_GetObjectItem(json, "facteur_k");
    if (item) s_config_courante.facteur_k_debitmetre = (float)item->valuedouble;
    item = cJSON_GetObjectItem(json, "seuil_debit");
    if (item) s_config_courante.seuil_debit_cuve_vide = (float)item->valuedouble;
    item = cJSON_GetObjectItem(json, "delai_detection");
    if (item) s_config_courante.delai_detection_ms = item->valueint;
    item = cJSON_GetObjectItem(json, "timeout_vanne");
    if (item) s_config_courante.timeout_vanne_ms = item->valueint;
    item = cJSON_GetObjectItem(json, "temps_on");
    if (item) s_config_courante.temps_brassage_on = item->valueint;
    item = cJSON_GetObjectItem(json, "temps_off");
    if (item) s_config_courante.temps_brassage_off = item->valueint;

    cJSON_Delete(json);

    /* Incrémenter la version */
    s_config_courante.version++;

    /* Publier via MQTT (le MASTER la diffusera) */
    mqtt_publier_mise_a_jour_config(&s_config_courante);

    /* Sauvegarder localement aussi */
    configuration_sauvegarder(&s_config_courante);

    httpd_resp_send(req, "OK", 2);
    ESP_LOGI(TAG, "Configuration mise à jour (v%lu)", (unsigned long)s_config_courante.version);
    return ESP_OK;
}

/* ====================================================================
 * DÉMARRAGE / ARRÊT DU SERVEUR
 * ==================================================================== */
void web_ui_demarrer(const configuration_t *config)
{
    if (config) {
        s_config_courante = *config;
    }

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.max_uri_handlers = 10;
    http_config.stack_size = 8192;

    if (httpd_start(&s_serveur, &http_config) != ESP_OK) {
        ESP_LOGE(TAG, "Échec démarrage serveur HTTP.");
        return;
    }

    /* Enregistrer les routes */
    httpd_uri_t uri_main = { .uri = "/", .method = HTTP_GET,
                              .handler = handler_page_principale };
    httpd_uri_t uri_status = { .uri = "/status", .method = HTTP_GET,
                                .handler = handler_status };
    httpd_uri_t uri_cmd = { .uri = "/api/cmd", .method = HTTP_GET,
                             .handler = handler_api_cmd };
    httpd_uri_t uri_settings = { .uri = "/settings", .method = HTTP_GET,
                                  .handler = handler_page_settings };
    httpd_uri_t uri_save = { .uri = "/api/save_config", .method = HTTP_POST,
                              .handler = handler_save_config };

    httpd_register_uri_handler(s_serveur, &uri_main);
    httpd_register_uri_handler(s_serveur, &uri_status);
    httpd_register_uri_handler(s_serveur, &uri_cmd);
    httpd_register_uri_handler(s_serveur, &uri_settings);
    httpd_register_uri_handler(s_serveur, &uri_save);

    ESP_LOGI(TAG, "Serveur HTTP démarré sur le port %d.", http_config.server_port);
}

void web_ui_arreter(void)
{
    if (s_serveur) {
        httpd_stop(s_serveur);
        s_serveur = NULL;
        ESP_LOGI(TAG, "Serveur HTTP arrêté.");
    }
}

void web_ui_update_etat_avant(const etat_carte_avant_t *etat)
{
    if (etat) s_etat_avant = *etat;
}

void web_ui_update_etat_arriere(const etat_carte_arriere_t *etat)
{
    if (etat) s_etat_arriere = *etat;
}
