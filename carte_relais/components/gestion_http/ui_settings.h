/**
 * @file ui_settings.h
 * @brief Page de configuration
 */

#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include <stdio.h>
#include "types_communs.h"
#include "configuration.h"

// ============================================================================
// FONCTION DE GÉNÉRATION DYNAMIQUE
// ============================================================================

/**
 * @brief Génère la page HTML de configuration avec les valeurs actuelles
 */
extern "C" const char* http_get_settings_page(void) {
    static char page[12288]; // Buffer statique (12KB)
    
    // Récupérer la config actuelle
    const configuration_systeme_t* cfg = configuration_get_ptr();
    
    // Générer le HTML avec les valeurs dynamiques
    int offset = 0;
    
    // Header
    offset += snprintf(page + offset, sizeof(page) - offset,
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>"
        "<style>"
        "body { margin: 0; padding: 0; background: #121212; color: white; font-family: sans-serif; }"
        ".setting-row { display: flex; align-items: center; justify-content: space-between; padding: 12px 0; border-bottom: 1px solid #333; }"
        ".setting-row:last-child { border-bottom: none; }"
        "input[type='number'], input[type='checkbox'] { "
        "    background: #121212; border: 1px solid #444; color: white; "
        "    padding: 8px; border-radius: 5px; text-align: center; font-size: 1rem;"
        "}"
        "input[type='number'] { width: 90px; }"
        "input[type='checkbox'] { width: 24px; height: 24px; cursor: pointer; }"
        "label { font-size: 0.95rem; color: #ccc; flex: 1; }"
        ".unit { font-size: 0.75rem; color: #666; margin-left: 5px; }"
        "h2 { color: #3399ff; font-size: 1.1rem; margin-top: 20px; border-left: 3px solid #3399ff; padding-left: 10px; }"
        ".section-desc { font-size: 0.85rem; color: #888; margin-top: -10px; margin-bottom: 10px; font-style: italic; }"
        "</style>"
    );
    
    // Ajouter COMMON_STYLE
    extern const char COMMON_STYLE[];
    offset += snprintf(page + offset, sizeof(page) - offset, "%s", COMMON_STYLE);
    
    offset += snprintf(page + offset, sizeof(page) - offset,
        "</head><body>"
        "<div class='top-bar'>"
        "<a href='/' style='color:#3399ff; text-decoration:none; font-weight:bold;'>◀ RETOUR</a>"
        "<div class='ver-info'>CONFIG v3.0</div>"
        "<div></div></div>"
        "<div style='padding:15px; overflow-y:auto; flex:1;'>"
    );
    
    // SECTION TRANSFERT
    offset += snprintf(page + offset, sizeof(page) - offset,
        "<h2>🔄 TRANSFERT AUTOMATIQUE</h2>"
        "<div class='control-card'>"
        "  <div class='setting-row'><label>Volume à transférer <span class='unit'>(L)</span></label><input type='number' id='t_tgt' value='%.0f'></div>"
        "  <div class='setting-row'><label>Facteur K Débitmètre</label><input type='number' step='0.1' id='k_fact' value='%.2f'></div>"
        "</div>",
        cfg->automatismes.volume_transfert_litres,
        cfg->capteurs.facteur_k_debitmetre
    );
    
    // SECTION SÉCURITÉ
    offset += snprintf(page + offset, sizeof(page) - offset,
        "<h2>🛡️ SÉCURITÉ DÉBIT (VIDE)</h2>"
        "<div class='control-card'>"
        "  <div class='setting-row'><label>Seuil mini débit <span class='unit'>(L/min)</span></label><input type='number' step='0.1' id='e_flow' value='%.1f'></div>"
        "  <div class='setting-row'><label>Délai avant coupure <span class='unit'>(sec)</span></label><input type='number' id='e_out' value='%lu'></div>"
        "  <div class='setting-row'><label>Timeout Vannes <span class='unit'>(sec)</span></label><input type='number' id='v_timeout' value='%lu'></div>"
        "</div>",
        cfg->securite.seuil_debit_cuve_vide,
        cfg->securite.delai_detection_ms / 1000,
        cfg->securite.timeout_vanne_3fils_ms / 1000
    );
    
    // SECTION BRASSAGE
    offset += snprintf(page + offset, sizeof(page) - offset,
        "<h2>🌀 CYCLES DE BRASSAGE</h2>"
        "<div class='control-card'>"
        "  <div class='setting-row'><label>Temps Marche <span class='unit'>(min)</span></label><input type='number' id='br_on' value='%lu'></div>"
        "  <div class='setting-row'><label>Temps Repos <span class='unit'>(min)</span></label><input type='number' id='br_off' value='%lu'></div>"
        "</div>",
        cfg->automatismes.temps_brassage_on_sec / 60,
        cfg->automatismes.temps_brassage_pause_sec / 60
    );
    
    // BOUTONS + JAVASCRIPT
    offset += snprintf(page + offset, sizeof(page) - offset,
        "<button class='btn-full active-green' onclick='saveSettings()'>💾 ENREGISTRER LA CONFIGURATION</button>"
        "</div>"
        
        "<script>"
        "function saveSettings() {"
        "  const params = new URLSearchParams();"
        "  params.append('t_tgt', document.getElementById('t_tgt').value);"
        "  params.append('k_fact', document.getElementById('k_fact').value);"
        "  params.append('e_flow', document.getElementById('e_flow').value);"
        "  params.append('e_out', document.getElementById('e_out').value);"
        "  params.append('v_timeout', document.getElementById('v_timeout').value);"
        "  params.append('br_on', document.getElementById('br_on').value);"
        "  params.append('br_off', document.getElementById('br_off').value);"
        "  fetch('/api/save_all?' + params.toString())"
        "    .then(r => r.ok ? (alert('✅ Config sauvegardée!'), location.href='/') : alert('❌ Erreur'))"
        "    .catch(e => alert('❌ Erreur réseau'));"
        "}"
        "</script>"
        "</body></html>"
    );
    
    return page;
}

#endif // UI_SETTINGS_H