/**
 * @file ui_main.h
 * @brief Page HTML principale du dashboard
 */

#ifndef UI_MAIN_H
#define UI_MAIN_H

#include "common_ui.h"
#include <stdio.h>  

const char HTML_MAIN_PAGE[] = R"=====(
<!DOCTYPE html><html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
)=====";

const char HTML_MAIN_PAGE_2[] = R"=====(
    <style>
        .dashboard { grid-template-columns: 1fr; }
        .slide { margin-bottom: 20px; border-bottom: 2px solid #333; padding-bottom: 20px; }
        .h-gauge-container { height: 35px; }
        button:focus { outline: none !important; }
    </style>
</head>
<body>
    <div class="top-bar">
        <div id="status-tag" class="offline">INITIALISATION...</div>
        <div id="av-empty-alert" class="alert-mini" style="display:none;">⚠️ CUVE VIDE</div>
        <div class="ver-info">v3.0 ESP32</div>
        <a href="/settings" class="settings-icon">⚙</a>
    </div>

    <div class="dashboard">
        <div class="slide">
            <h2>UNITÉ DE POMPAGE</h2>
            <div class="control-card">
                <div class="card-title">Débit Instantané</div>
                <div class="h-gauge-container">
                    <div id="jFillFlow" class="h-gauge-fill" style="background: #2ecc71;"></div>
                    <div id="jTextFlow" class="h-gauge-text">-- L/min</div>
                </div>
            </div>
            <button id="p_btn" class="btn-full" onclick="cmd('p')">CHARGEMENT...</button>
            <button id="v_btn" class="btn-full" onclick="cmd('v')">BRASSAGE / TRANSFERT</button>
            <button id="l_btn" class="btn-full" onclick="cmd('l')">PHARE AVANT</button>
        </div>

        <div class="slide" style="background:#1d1d1d; border-radius: 15px; padding: 10px;">
            <h2>AUTOMATISMES</h2>
            <div class="control-card">
                <button id="mode_tr" class="btn-full" onclick="api('tr','A')">LANCER TRANSFERT</button>
                <div class="h-gauge-container">
                    <div id="jFillTr" class="h-gauge-fill" style="background: #3399ff;"></div>
                    <div id="jTextTr" class="h-gauge-text">-- / -- L</div>
                </div>
            </div>
            <div class="control-card">
                <button id="mode_br" class="btn-full" onclick="api('br','A')">AUTO BRASSAGE</button>
                <div class="h-gauge-container">
                    <div id="jFillBr" class="h-gauge-fill" style="background: #2ecc71;"></div>
                    <div id="jTextBr" class="h-gauge-text">-- min</div>
                </div>
            </div>
            <button class="btn-full" onclick="api('id','S')" style="background:#331111; color:#ff4444; border: 1px solid #552222; font-weight:bold;">ARRÊT D'URGENCE AUTO</button>
        </div>

        <div class="slide">
            <h2>VANNES & PHARES AR</h2>
            <div class="control-card">
                <div class="card-title">Vanne 2m</div>
                <div class="btn-group">
                    <button id="v2m_F" class="btn-v" onclick="api('v2','F')">FERMER</button>
                    <button id="v2m_S" class="btn-v" onclick="api('v2','S')">STOP</button>
                    <button id="v2m_O" class="btn-v" onclick="api('v2','O')">OUVRIR</button>
                </div>
            </div>

            <div class="control-card">
                <div class="card-title">Bout de rampe</div>
                <div class="btn-group">
                    <button id="vbt_F" class="btn-v" onclick="api('vb','F')">FERMER</button>
                    <button id="vbt_S" class="btn-v" onclick="api('vb','S')">STOP</button>
                    <button id="vbt_O" class="btn-v" onclick="api('vb','O')">OUVRIR</button>
                </div>
            </div>
            <button id="li_btn" class="btn-full" onclick="api('li','T')">PHARE ARRIÈRE</button>
        </div>
    </div>

<script>
function cmd(t) { fetch('/' + t + 'T'); }
function api(t, a) { fetch('/api/cmd?t=' + t + '&a=' + a); }

function refresh() {
    fetch('/status').then(r => r.json()).then(d => {
        let st = document.getElementById('status-tag');
        st.className = "online"; st.innerText = "SYSTÈME CONNECTÉ";

        // ALERTE VIDE & POMPE
        let alertBox = document.getElementById('av-empty-alert');
        let pBtn = document.getElementById('p_btn');
        if (d.av_ok && d.av_vide) {
            alertBox.style.display = "inline-block";
            pBtn.className = d.p ? 'btn-full active-green' : 'btn-full active-red blink-border';
            pBtn.innerText = d.p ? 'RÉARMEMENT...' : 'CUVE VIDE (RÉARMER)';
        } else {
            alertBox.style.display = "none";
            pBtn.className = d.p ? 'btn-full active-green' : 'btn-full';
            pBtn.innerText = d.p ? 'POMPE EN MARCHE' : 'POMPE ARRÊTÉE';
        }

        // DÉBITMÈTRE
        let fillFlow = document.getElementById('jFillFlow');
        let textFlow = document.getElementById('jTextFlow');
        if (d.av_ok) {
            fillFlow.style.width = Math.min((d.av_flow/60)*100, 100) + '%';
            textFlow.innerText = d.av_flow.toFixed(1) + " L/min";
        } else {
            fillFlow.style.width = '0%'; textFlow.innerText = "AV OFFLINE";
        }

        // VANNE 3 VOIES
        let v3v = document.getElementById('v_btn');
        v3v.className = d.v ? 'btn-full active-blue' : 'btn-full active-green';
        v3v.innerText = d.v ? 'SORTIE : TRANSFERT' : 'SORTIE : BRASSAGE';

        // PHARES
        document.getElementById('l_btn').className = d.l ? 'btn-full active-yellow' : 'btn-full';
        document.getElementById('li_btn').className = d.li ? 'btn-full active-yellow' : 'btn-full';

        // TRANSFERT
        document.getElementById('mode_tr').className = d.m_tr ? 'btn-full active-blue' : 'btn-full';
        document.getElementById('jFillTr').style.width = Math.min((d.session_vol/d.tr_target*100), 100) + '%';
        document.getElementById('jTextTr').innerText = Math.round(d.session_vol) + " / " + d.tr_target + " L";

        // BRASSAGE
        document.getElementById('mode_br').className = d.m_br ? 'btn-full active-blue' : 'btn-full';
        document.getElementById('jTextBr').innerText = d.br_label + " : " + d.br_rem + " min";
        document.getElementById('jFillBr').style.width = d.br_pct + '%';

        // VANNES ARRIÈRE
        ['v2m','vbt'].forEach(v => {
            document.getElementById(v+'_O').className = 'btn-v ' + (d[v] == 'O' ? 'active-green' : '');
            document.getElementById(v+'_S').className = 'btn-v ' + (d[v] == 'S' ? 'active-off' : '');
            document.getElementById(v+'_F').className = 'btn-v ' + (d[v] == 'F' ? 'active-red' : '');
        });

    }).catch(() => {
        document.getElementById('status-tag').className = "offline";
        document.getElementById('status-tag').innerText = "LIAISON PERDUE";
    });
}
setInterval(refresh, 1000);
</script></body></html>
)=====";

/**
 * @brief Retourne la page HTML principale complète
 */
extern "C" const char* http_get_main_page(void) {
    // Pour l'instant on retourne juste la page statique
    // TODO: Générer dynamiquement avec sprintf si besoin de valeurs
    static char page[16384];
    snprintf(page, sizeof(page), "%s%s%s", HTML_MAIN_PAGE, COMMON_STYLE, HTML_MAIN_PAGE_2);
    return page;
}

#endif // UI_MAIN_H
