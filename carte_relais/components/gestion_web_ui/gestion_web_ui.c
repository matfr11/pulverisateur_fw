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
#include "gestion_capteurs.h"
#include "protocole_mqtt.h"
#include "mqtt_topics.h"
#include "gestion_configuration.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "mdns.h"
#include "gestion_ota.h"
#include "freertos/semphr.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "WEB_UI";
static httpd_handle_t s_serveur = NULL;

/* État local mis à jour via MQTT */
static etat_carte_avant_t   s_etat_avant = {0};
static etat_carte_arriere_t s_etat_arriere = {0};
static SemaphoreHandle_t    s_mutex_etat = NULL;
static configuration_t      s_config_courante = CONFIG_DEFAUT;
/* Initialisés très dans le passé : au démarrage, aucune carte n'est considérée présente */
static int64_t s_derniere_maj_avant   = -10000LL;
static int64_t s_derniere_maj_arriere = -10000LL;
static carte_id_t s_carte_master = CARTE_ID_AVANT;  /* Mis à jour au démarrage */

/* ====================================================================
 * CSS COMMUN (adapté de common_ui.h)
 * ==================================================================== */
static const char CSS_COMMUN[] =
"<style>"
":root{--bg:#121212;--bg2:#1a1a1a;--bg3:#262626;--bg4:#151515;--bg5:#1d1d1d;"
"--brd:#333;--brd2:#383838;--brd3:#444;--txt:#fff;--txt2:#eee;--txt3:#444;"
"--ico:#666;--btn:#333;--gbg:#000;--lbl:#ccc;--lbl2:#666}"
"[data-theme=light]{--bg:#d8dce2;--bg2:#e4e8ed;--bg3:#dde1e7;--bg4:#cdd1d8;--bg5:#dde1e7;"
"--brd:#c4c8ce;--brd2:#cdd1d7;--brd3:#adb2b8;--txt:#1a1a1a;--txt2:#2a2a2a;--txt3:#8a8e94;"
"--ico:#5a5e64;--btn:#cdd1d7;--gbg:#bfc3c9;--lbl:#3a3e44;--lbl2:#6a6e74}"
"body{margin:0;padding:0;background:var(--bg);color:var(--txt);font-family:sans-serif;"
"height:100vh;width:100vw;overflow:hidden;display:flex;flex-direction:column}"
".top-bar{display:flex;align-items:center;justify-content:space-between;"
"background:var(--bg2);padding:0 12px;border-bottom:1px solid var(--brd);height:38px}"
"#status-tag{font-size:.65rem;font-weight:bold;padding:3px 8px;border-radius:4px}"
".ver-info{font-size:.6rem;color:var(--txt3)}"
".settings-icon{color:var(--ico);text-decoration:none;font-size:1.1rem}"
"#thm_btn{background:none;border:none;font-size:1.1rem;cursor:pointer;padding:0 4px;line-height:1;color:var(--ico)}"
".online{background:#1a592e;color:#2ecc71}"
".partial{background:#5c3a00;color:#f39c12}"
".offline{background:#591a1a;color:#e74c3c}"
".dashboard{display:flex;flex:1;width:100%;overflow-x:auto;scroll-snap-type:x mandatory;"
"scroll-behavior:smooth}"
".slide{flex:0 0 100vw;height:100%;scroll-snap-align:start;box-sizing:border-box;"
"padding:8px;background:var(--bg2);overflow-y:auto}"
".slide-auto{background:var(--bg5)!important;border-radius:15px;padding:10px!important}"
"h2{color:var(--txt);text-align:center;font-size:1rem;margin:4px 0 10px 0;"
"text-transform:uppercase;letter-spacing:1.5px;border-bottom:2px solid #39f;"
"width:100%;padding-bottom:3px}"
".control-card{background:var(--bg3);border-radius:10px;padding:6px 8px;"
"margin-bottom:6px;border:1px solid var(--brd2)}"
".card-title{font-size:.65rem;color:var(--txt);margin-bottom:3px;text-transform:uppercase;"
"text-align:center}"
".h-gauge-container{width:100%;height:20px;background:var(--gbg);border-radius:5px;"
"border:1px solid var(--brd3);position:relative;overflow:hidden;margin-bottom:4px}"
".h-gauge-fill{height:100%;width:0%;transition:width .5s ease;position:absolute}"
".h-gauge-text{position:absolute;width:100%;text-align:center;line-height:20px;"
"font-weight:bold;z-index:2;color:white;text-shadow:1px 1px 2px black;font-size:.75rem}"
".btn-full{width:100%;padding:14px;border-radius:8px;border:none;background:var(--btn);"
"color:var(--txt2);font-weight:bold;cursor:pointer;margin-bottom:4px;font-size:.8rem}"
".btn-group{display:flex;gap:4px;background:var(--bg);padding:3px;border-radius:6px}"
".btn-v{flex:1;padding:12px 2px;border:none;border-radius:4px;background:var(--btn);"
"color:var(--txt);font-weight:bold;cursor:pointer;font-size:.75rem}"
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
".slide{height:100%!important;padding:5px!important;border-right:1px solid var(--brd)}"
".slide:nth-child(1),.slide:nth-child(3){flex:2 1 40%}"
".slide:nth-child(2){flex:1 1 20%;min-width:180px;background:var(--bg4)!important}"
".slide:nth-child(2) .btn-full{font-size:.7rem;padding:10px 5px}"
"h2{font-size:.85rem;margin-bottom:5px}}"
"@media(min-width:768px){"
".top-bar{height:48px}"
"#status-tag{font-size:.9rem;padding:5px 12px}"
".ver-info{font-size:.75rem}"
".settings-icon{font-size:1.5rem}"
"#thm_btn{font-size:1.5rem}"
"h2{font-size:1.4rem;margin:8px 0 14px 0;padding-bottom:6px}"
".card-title{font-size:1rem}"
".h-gauge-container{height:36px;margin-bottom:8px}"
".h-gauge-text{line-height:36px;font-size:1.15rem}"
".btn-full{padding:22px;margin-bottom:8px;font-size:1.15rem}"
".btn-v{padding:20px 4px;font-size:1rem}"
".control-card{padding:10px 12px;margin-bottom:10px}"
".slide{padding:12px!important}"
".slide:nth-child(1),.slide:nth-child(3){flex:3 1 35%}"
".slide:nth-child(2){flex:2 1 30%;min-width:240px}"
".slide:nth-child(2) .btn-full{font-size:1.05rem;padding:20px 10px}}"
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
"<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>"
"<script>(function(){var t=localStorage.getItem('theme');if(t==='light')document.documentElement.setAttribute('data-theme','light');})();</script>";

static const char PAGE_PART2[] =
"</head><body>"
"<div class='top-bar'>"
"<div id='status-tag' class='offline'>INITIALISATION...</div>"
"<div id='av-empty-alert' class='alert-mini' style='display:none'>&#9888; CUVE VIDE</div>"
"<div class='ver-info'>v" VERSION_FIRMWARE "</div>"
"<button id='thm_btn' onclick='toggleTheme()'>&#9789;</button>"
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
"<div class='slide slide-auto'>"
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
"    var avOk=d.link_av,arOk=d.link_ar;"
"    var avTxt=avOk?'AV ✓':'AV Absente';"
"    var arTxt=arOk?'AR ✓':'AR Absente';"
"    if(avOk&&arOk){st.className='online';}"
"    else if(avOk||arOk){st.className='partial';}"
"    else{st.className='offline';}"
"    st.innerText='Serveur OK / '+avTxt+' / '+arTxt;"
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
"    if(d.sonde_ok&&d.niveau_ar_max>0){"
"      var pct=d.niveau_ar/d.niveau_ar_max*100;"
"      fillNiv.style.width=Math.min(pct,100)+'%';"
"      fillNiv.style.background=pct<15?'#e74c3c':pct<30?'#f39c12':'#3399ff';"
"      textNiv.innerText=Math.round(d.niveau_ar)+' / '+d.niveau_ar_max+' L';"
"    }else{fillNiv.style.width='0%';textNiv.innerText='AR OFFLINE';}"
"  }).catch(function(){"
"    document.getElementById('status-tag').className='offline';"
"    document.getElementById('status-tag').innerText='LIAISON PERDUE';"
"  });"
"}"
"setInterval(refresh,300);"
"function toggleTheme(){"
"  var t=document.documentElement.getAttribute('data-theme')==='light'?'dark':'light';"
"  document.documentElement.setAttribute('data-theme',t);"
"  localStorage.setItem('theme',t);"
"  document.getElementById('thm_btn').innerHTML=t==='light'?'&#9728;':'&#9789;';"
"}"
"(function(){var b=document.getElementById('thm_btn');"
"b.innerHTML=document.documentElement.getAttribute('data-theme')==='light'?'&#9728;':'&#9789;';})();"
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
    etat_carte_avant_t   etat_av;
    etat_carte_arriere_t etat_ar;

    xSemaphoreTake(s_mutex_etat, portMAX_DELAY);
    etat_av = s_etat_avant;
    etat_ar = s_etat_arriere;
    xSemaphoreGive(s_mutex_etat);

    cJSON *json = cJSON_CreateObject();

    /* État carte AVANT */
    cJSON_AddBoolToObject(json, "p", etat_av.pompe == POMPE_EN_MARCHE);
    cJSON_AddBoolToObject(json, "v", etat_av.vanne_3v == V3V_TRANSFERT);
    cJSON_AddBoolToObject(json, "l", etat_av.phares_avant);
    cJSON_AddBoolToObject(json, "av_ok", etat_av.debitmetre_ok);
    cJSON_AddNumberToObject(json, "av_flow", etat_av.debit_instantane);
    cJSON_AddBoolToObject(json, "av_vide", etat_av.securite_cuve == SEC_CUVE_VIDE);
    cJSON_AddNumberToObject(json, "session_vol", etat_av.volume_session);
    cJSON_AddBoolToObject(json, "m_tr", etat_av.auto_transfert == AUTO_TR_EN_COURS);
    cJSON_AddNumberToObject(json, "tr_target", etat_av.transfert_volume_cible);
    cJSON_AddBoolToObject(json, "m_br", etat_av.auto_brassage != AUTO_BR_INACTIF);
    cJSON_AddStringToObject(json, "br_label", etat_av.brassage_label);
    cJSON_AddNumberToObject(json, "br_rem", etat_av.brassage_temps_restant);
    cJSON_AddNumberToObject(json, "br_pct", etat_av.brassage_pourcentage);

    cJSON_AddNumberToObject(json, "dbg_raw", capteurs_sonde_get_debug_raw());
    cJSON_AddNumberToObject(json, "dbg_mv", capteurs_sonde_get_debug_mv());

    /* État carte ARRIÈRE */
    const char *map_vanne[] = {"?", "O", "F", "S", "T"};
    int idx_v2m = (int)etat_ar.vanne_2m;
    int idx_vbt = (int)etat_ar.vanne_bout_rampe;
    cJSON_AddStringToObject(json, "v2m",
        (idx_v2m >= 0 && idx_v2m < 5) ? map_vanne[idx_v2m] : "?");
    cJSON_AddStringToObject(json, "vbt",
        (idx_vbt >= 0 && idx_vbt < 5) ? map_vanne[idx_vbt] : "?");
    cJSON_AddBoolToObject(json, "li", etat_ar.phares_arriere);
    float litres_ar = etat_ar.niveau_cuve_arriere * s_config_courante.volume_cuve_ar / 100.0f;
    cJSON_AddNumberToObject(json, "niveau_ar", litres_ar);
    cJSON_AddNumberToObject(json, "niveau_ar_max", s_config_courante.volume_cuve_ar);
    cJSON_AddBoolToObject(json, "sonde_ok", etat_ar.sonde_niveau_ok);
    /* Info réseau */
    int64_t now = esp_timer_get_time() / 1000;
    cJSON_AddStringToObject(json, "master",
        s_carte_master == CARTE_ID_AVANT   ? "AV" :
        s_carte_master == CARTE_ID_ARRIERE ? "AR" : "SRV");
    cJSON_AddBoolToObject(json, "link_av", (now - s_derniere_maj_avant) < 5000);
    cJSON_AddBoolToObject(json, "link_ar", (now - s_derniere_maj_arriere) < 5000);
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
    char *page = malloc(14336);
    if (!page) return ESP_ERR_NO_MEM;

    int len = snprintf(page, 14336,
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>"
        "<script>(function(){var t=localStorage.getItem('theme');if(t==='light')document.documentElement.setAttribute('data-theme','light');})();</script>"
        "%s"
        "<style>"
        ".setting-row{display:flex;align-items:center;justify-content:space-between;"
        "padding:12px 0;border-bottom:1px solid var(--brd)}"
        "input[type='number']{background:var(--bg);border:1px solid var(--brd3);color:var(--txt);"
        "padding:8px;border-radius:5px;width:90px;text-align:center;font-size:1rem}"
        "label{font-size:.95rem;color:var(--lbl)}"
        ".unit{font-size:.75rem;color:var(--lbl2);margin-left:5px}"
        "h2{color:#39f;font-size:1.1rem;margin-top:20px;border-left:3px solid #39f;"
        "padding-left:10px;border-bottom:none;text-align:left}"
        "</style></head><body>"
        "<div class='top-bar'>"
        "<a href='/' style='color:#39f;text-decoration:none;font-weight:bold'>&#9664; RETOUR</a>"
        "<div class='ver-info'>CONFIG v" VERSION_FIRMWARE "</div>"
        "<button id='thm_btn' onclick='toggleTheme()'>&#9789;</button></div>"
        "<div style='padding:15px;overflow-y:auto;flex:1'>"
        "<h2>TRANSFERT AUTOMATIQUE</h2>"
        "<div class='control-card'>"
        "<div class='setting-row'><label>Volume <span class='unit'>(L)</span></label>"
        "<input type='number' id='t_tgt' value='%u'></div>"
        "<div class='setting-row'><label>Facteur K</label>"
        "<input type='number' step='0.1' id='k_fact' value='%.1f'></div></div>"
        "<h2>S&Eacute;CURIT&Eacute; D&Eacute;BIT</h2>"
        "<div class='control-card'>"
        "<div class='setting-row'><label>Debit mini desamorcage<span class='unit'>(L/min)</span></label>"
        "<input type='number' step='0.1' id='e_flow' value='%.1f'></div>"
        "<div class='setting-row'><label>D&eacute;lai desamorcage<span class='unit'>(sec)</span></label>"
        "<input type='number' id='e_out' value='%u'></div>"
        "<div class='setting-row'><label>Timeout Vannes <span class='unit'>(sec)</span></label>"
        "<input type='number' id='v_timeout' value='%u'></div></div>"
        "<h2>CYCLES DE BRASSAGE</h2>"
        "<div class='control-card'>"
        "<div class='setting-row'><label>Marche <span class='unit'>(sec)</span></label>"
        "<input type='number' id='br_on' value='%u'></div>"
        "<div class='setting-row'><label>Repos <span class='unit'>(sec)</span></label>"
        "<input type='number' id='br_off' value='%u'></div></div>"
        "<h2>CUVE ARRI&Egrave;RE</h2>"
        "<div class='control-card'>"
        "<div class='setting-row'><label>Volume total <span class='unit'>(L)</span></label>"
        "<input type='number' id='vol_ar' value='%u'></div>"
        "<div class='setting-row'><label>Hauteur max sonde <span class='unit'>(mm)</span></label>"
        "<input type='number' id='s_hmax' value='%u'></div>"
        "<div class='setting-row'><label>Offset sonde <span class='unit'>(mm)</span></label>"
        "<input type='number' id='s_off' value='%u'></div></div>"
        "<div class='setting-row'><label>Hauteur cuve <span class='unit'>(mm)</span></label>"
        "<input type='number' id='h_cuve' value='%u'></div>"
        "<h2>MISE &Agrave; JOUR FIRMWARE</h2>"
        "<div class='control-card'>"
        "<div class='setting-row'>"
        "<label>Fichier .bin</label>"
        "<input type='file' id='ota_file' accept='.bin' style='color:#ccc;font-size:0.85rem'>"
        "</div>"
        "<div id='ota_progress' style='display:none;margin:8px 0'>"
        "<div class='h-gauge-container'>"
        "<div id='jFillOta' class='h-gauge-fill' style='background:#f39c12'></div>"
        "<div id='jTextOta' class='h-gauge-text'>0%%</div>"
        "</div></div>"
        "<div style='display:flex;gap:6px;margin-top:8px'>"
        "<button onclick=\"doOta('/api/ota')\""
        " style='flex:1;padding:8px;background:#1a3a1a;color:#2ecc71;border:1px solid #2ecc71;border-radius:4px;cursor:pointer'>"
        "SERVEUR</button>"
        "<button onclick=\"doOta('/api/ota/avant')\""
        " style='flex:1;padding:8px;background:#1a2a3a;color:#39f;border:1px solid #39f;border-radius:4px;cursor:pointer'>"
        "CARTE AVANT</button>"
        "<button onclick=\"doOta('/api/ota/arriere')\""
        " style='flex:1;padding:8px;background:#2a1a3a;color:#a060ff;border:1px solid #a060ff;border-radius:4px;cursor:pointer'>"
        "CARTE ARRI&Egrave;RE</button>"
        "</div>"
        "</div>"
        "<br><button class='btn-full active-green' onclick='saveSettings()'>"
        "ENREGISTRER</button></div>"
        "<script>"
        "function doOta(url){"
        "  var f=document.getElementById('ota_file').files[0];"
        "  if(!f){alert('Choisir un fichier .bin');return;}"
        "  var cible=url=='/api/ota'?'SERVEUR':url=='/api/ota/avant'?'CARTE AVANT':'CARTE ARRIERE';"
        "  if(!confirm('Flasher '+f.name+' ('+Math.round(f.size/1024)+' KB) sur '+cible+' ?\\nLa carte va redémarrer.')){return;}"
        "  var pg=document.getElementById('ota_progress');pg.style.display='block';"
        "  document.getElementById('jFillOta').style.width='0%%';"
        "  document.getElementById('jTextOta').innerText='0%%';"
        "  document.getElementById('jFillOta').style.background='#f39c12';"
        "  var xhr=new XMLHttpRequest();"
        "  xhr.open('POST',url);"
        "  xhr.upload.onprogress=function(e){"
        "    if(e.lengthComputable){"
        "      var p=Math.round(e.loaded/e.total*100);"
        "      document.getElementById('jFillOta').style.width=p+'%%';"
        "      document.getElementById('jTextOta').innerText=p+'%%';"
        "    }"
        "  };"
        "  xhr.onload=function(){"
        "    if(xhr.status==200){"
        "      document.getElementById('jTextOta').innerText='OK - Redemarrage...';"
        "      document.getElementById('jFillOta').style.background='#2ecc71';"
        "      document.getElementById('jFillOta').style.width='100%%';"
        "    }else{alert('Erreur OTA: '+xhr.responseText);}"
        "  };"
        "  xhr.onerror=function(){alert('Erreur réseau');};"
        "  xhr.send(f);"
        "}"
        "function saveSettings(){"
        "  var cfg={"
        "    volume_transfert:+document.getElementById('t_tgt').value,"
        "    facteur_k:+document.getElementById('k_fact').value,"
        "    seuil_debit:+document.getElementById('e_flow').value,"
        "    delai_detection:+document.getElementById('e_out').value,"
        "    timeout_vanne:+document.getElementById('v_timeout').value,"
        "    temps_on:+document.getElementById('br_on').value,"
        "    temps_off:+document.getElementById('br_off').value,"
        "    volume_cuve_ar:+document.getElementById('vol_ar').value,"
        "    sonde_hauteur_max:+document.getElementById('s_hmax').value,"
        "    sonde_offset:+document.getElementById('s_off').value,"
        "    hauteur_cuve:+document.getElementById('h_cuve').value"
        "  };"
        "  fetch('/api/save_config',{method:'POST',"
        "    headers:{'Content-Type':'application/json'},"
        "    body:JSON.stringify(cfg)"
        "  }).then(r=>{if(r.ok)alert('OK');else alert('Erreur');})"
        "    .catch(e=>alert('Erreur: '+e));"
        "}"
        "function toggleTheme(){"
        "  var t=document.documentElement.getAttribute('data-theme')==='light'?'dark':'light';"
        "  document.documentElement.setAttribute('data-theme',t);"
        "  localStorage.setItem('theme',t);"
        "  document.getElementById('thm_btn').innerHTML=t==='light'?'&#9728;':'&#9789;';"
        "}"
        "(function(){var b=document.getElementById('thm_btn');"
        "b.innerHTML=document.documentElement.getAttribute('data-theme')==='light'?'&#9728;':'&#9789;';})();"
        "</script></body></html>",
        CSS_COMMUN,
        (unsigned)s_config_courante.volume_transfert,
        s_config_courante.facteur_k_debitmetre,
        s_config_courante.seuil_debit_cuve_vide,
        (unsigned)s_config_courante.delai_detection_ms / 1000,
        (unsigned)s_config_courante.timeout_vanne_ms / 1000,
        (unsigned)s_config_courante.temps_brassage_on,
        (unsigned)s_config_courante.temps_brassage_off,
        (unsigned)s_config_courante.volume_cuve_ar,
        (unsigned)s_config_courante.sonde_hauteur_max_mm,
        (unsigned)s_config_courante.sonde_offset_mm,
        (unsigned)s_config_courante.hauteur_cuve_mm
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
    if (item) s_config_courante.delai_detection_ms = item->valueint * 1000;
    item = cJSON_GetObjectItem(json, "timeout_vanne");
    if (item) s_config_courante.timeout_vanne_ms = item->valueint * 1000;
    item = cJSON_GetObjectItem(json, "temps_on");
    if (item) s_config_courante.temps_brassage_on = item->valueint;
    item = cJSON_GetObjectItem(json, "temps_off");
    if (item) s_config_courante.temps_brassage_off = item->valueint;
    item = cJSON_GetObjectItem(json, "volume_cuve_ar");
    if (item) s_config_courante.volume_cuve_ar = item->valueint;
    item = cJSON_GetObjectItem(json, "sonde_hauteur_max");
    if (item) s_config_courante.sonde_hauteur_max_mm = item->valueint;
    item = cJSON_GetObjectItem(json, "sonde_offset");
    if (item) s_config_courante.sonde_offset_mm = item->valueint;
    item = cJSON_GetObjectItem(json, "hauteur_cuve");
    if (item) s_config_courante.hauteur_cuve_mm = item->valueint;
    cJSON_Delete(json);

    /* Incrémenter la version */
    s_config_courante.version++;

    /*
     * Publier la configuration avec retain=true sur le topic "instantane".
     * Ceci met à jour le message retenu dans le broker : toute carte relais
     * qui se (re)connectera ultérieurement recevra automatiquement la config
     * à jour, sans demande explicite.
     * Les cartes relais actuellement connectées la reçoivent aussi immédiatement
     * car elles sont souscrites à "configuration/#".
     */
    mqtt_publier_configuration(&s_config_courante);

    /* Sauvegarder localement aussi */
    configuration_sauvegarder(&s_config_courante);

    httpd_resp_send(req, "OK", 2);
    ESP_LOGI(TAG, "Configuration mise à jour (v%lu)", (unsigned long)s_config_courante.version);
    return ESP_OK;
}
/* ====================================================================
 * mises a jour ots
 * ==================================================================== */

 /** POST /api/ota → Réception du firmware et flash OTA */
static esp_err_t handler_ota(httpd_req_t *req)
{
    ESP_LOGI(TAG, "OTA : réception firmware (%d octets)...", req->content_len);

    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    if (!update) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Pas de partition OTA");
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    char *buf = malloc(4096);
    if (!buf) {
        esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Malloc failed");
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    int received_total = 0;

    while (remaining > 0) {
        int received = httpd_req_recv(req, buf, (remaining < 4096) ? remaining : 4096);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ESP_LOGE(TAG, "OTA : erreur réception");
            free(buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv error");
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA : erreur écriture");
            free(buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write error");
            return ESP_FAIL;
        }

        remaining -= received;
        received_total += received;

        if (received_total % 65536 < 4096) {
            ESP_LOGI(TAG, "OTA : %d / %d octets", received_total, req->content_len);
        }
    }

    free(buf);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA : validation échouée: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA : set boot partition failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA OK ! Redémarrage dans 2s...");
    httpd_resp_send(req, "OK", 2);

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;  /* jamais atteint */
}

/* ====================================================================
 * PROXY OTA VERS LES CARTES RELAIS
 * ==================================================================== */

/**
 * Reçoit un firmware du navigateur par chunks et le retransmet en streaming
 * vers la carte relais (hostname mDNS). Jamais plus de 4 KB en RAM.
 */
static esp_err_t proxy_ota_vers_relais(httpd_req_t *req, const char *hostname)
{
    if (req->content_len == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content-Length manquant");
        return ESP_FAIL;
    }

    /* Résolution mDNS → adresse IP */
    esp_ip4_addr_t addr = {0};
    esp_err_t err = mdns_query_a(hostname, 2000, &addr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS : '%s' introuvable (%s)", hostname, esp_err_to_name(err));
        httpd_resp_set_status(req, "502 Bad Gateway");
        httpd_resp_send(req, "Carte non joignable", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    char url[48];
    snprintf(url, sizeof(url), "http://" IPSTR "/ota", IP2STR(&addr));
    ESP_LOGI(TAG, "OTA proxy → %s (%d octets)", url, req->content_len);

    esp_http_client_config_t client_cfg = {
        .url            = url,
        .method         = HTTP_METHOD_POST,
        .timeout_ms     = 120000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&client_cfg);
    if (!client) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Client HTTP init failed");
        return ESP_FAIL;
    }

    char content_len_str[16];
    snprintf(content_len_str, sizeof(content_len_str), "%d", req->content_len);
    esp_http_client_set_header(client, "Content-Length", content_len_str);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

    err = esp_http_client_open(client, req->content_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Connexion HTTP vers relais impossible: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        httpd_resp_set_status(req, "502 Bad Gateway");
        httpd_resp_send(req, "Carte non joignable", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    char *buf = malloc(4096);
    if (!buf) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Malloc failed");
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    while (remaining > 0) {
        int chunk = (remaining < 4096) ? remaining : 4096;
        int received = httpd_req_recv(req, buf, chunk);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ESP_LOGE(TAG, "OTA proxy : erreur réception navigateur");
            free(buf);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv error");
            return ESP_FAIL;
        }
        int written = esp_http_client_write(client, buf, received);
        if (written < 0) {
            ESP_LOGE(TAG, "OTA proxy : erreur envoi vers relais");
            free(buf);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            httpd_resp_set_status(req, "502 Bad Gateway");
            httpd_resp_send(req, "Erreur transfert", HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }
        remaining -= received;
    }
    free(buf);

    int status = esp_http_client_fetch_headers(client);
    int http_status = esp_http_client_get_status_code(client);
    (void)status;

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (http_status == 200) {
        ESP_LOGI(TAG, "OTA proxy : succès vers %s", hostname);
        httpd_resp_send(req, "OK", 2);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "OTA proxy : la carte a répondu %d", http_status);
        httpd_resp_set_status(req, "502 Bad Gateway");
        httpd_resp_send(req, "OTA échouée sur la carte", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
}

static esp_err_t handler_ota_avant(httpd_req_t *req)
{
    return proxy_ota_vers_relais(req, OTA_HOSTNAME_AVANT);
}

static esp_err_t handler_ota_arriere(httpd_req_t *req)
{
    return proxy_ota_vers_relais(req, OTA_HOSTNAME_ARRIERE);
}

/* ====================================================================
 * DÉMARRAGE / ARRÊT DU SERVEUR
 * ==================================================================== */
void web_ui_demarrer(const configuration_t *config)
{
    if (!s_mutex_etat) {
        s_mutex_etat = xSemaphoreCreateMutex();
    }

    if (config) {
        s_config_courante = *config;
    }

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.max_uri_handlers = 14;
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
    httpd_uri_t uri_ota    = { .uri = "/api/ota",         .method = HTTP_POST,
                               .handler = handler_ota };
    httpd_uri_t uri_ota_av = { .uri = "/api/ota/avant",   .method = HTTP_POST,
                               .handler = handler_ota_avant };
    httpd_uri_t uri_ota_ar = { .uri = "/api/ota/arriere", .method = HTTP_POST,
                               .handler = handler_ota_arriere };
    httpd_register_uri_handler(s_serveur, &uri_main);
    httpd_register_uri_handler(s_serveur, &uri_status);
    httpd_register_uri_handler(s_serveur, &uri_cmd);
    httpd_register_uri_handler(s_serveur, &uri_settings);
    httpd_register_uri_handler(s_serveur, &uri_save);
    httpd_register_uri_handler(s_serveur, &uri_ota);
    httpd_register_uri_handler(s_serveur, &uri_ota_av);
    httpd_register_uri_handler(s_serveur, &uri_ota_ar);

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
    if (etat) {
        xSemaphoreTake(s_mutex_etat, portMAX_DELAY);
        s_etat_avant = *etat;
        xSemaphoreGive(s_mutex_etat);
        s_derniere_maj_avant = esp_timer_get_time() / 1000;
    }
}

void web_ui_update_etat_arriere(const etat_carte_arriere_t *etat)
{
    if (etat) {
        xSemaphoreTake(s_mutex_etat, portMAX_DELAY);
        s_etat_arriere = *etat;
        xSemaphoreGive(s_mutex_etat);
        s_derniere_maj_arriere = esp_timer_get_time() / 1000;
    }
}

void web_ui_set_carte_master(carte_id_t id)
{
    s_carte_master = id;
}