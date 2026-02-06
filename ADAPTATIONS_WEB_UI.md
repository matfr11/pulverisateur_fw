# ADAPTATIONS DE LA WEB UI EXISTANTE

## Résumé des changements

La Web UI existante (common_ui.h, ui_main.h, ui_settings.h) a été portée de l'architecture
Arduino (appels HTTP directs) vers ESP-IDF avec MQTT comme bus de commande.

---

## 1. Architecture des commandes

### AVANT (Arduino) :
```javascript
// Chaque bouton appelait directement un endpoint spécifique
function cmd(t) { fetch('/' + t + 'T'); }        // ex: /pT = toggle pompe
function api(t, a) { fetch('/api/cmd?t=' + t + '&a=' + a); }  // ex: /api/cmd?t=tr&a=A
```

Le serveur Arduino exécutait directement les fonctions :
```cpp
server.on("/pT", []() { togglePompe(); server.send(200); });
```

### APRÈS (ESP-IDF + MQTT) :
```javascript
// Tous les boutons passent par une API unique avec cible + commande
function api(cible, cmd, val) {
    var url = '/api/cmd?cible=' + cible + '&cmd=' + cmd;
    if (val) url += '&val=' + val;
    fetch(url);
}
```

Le serveur HTTP publie la commande sur MQTT :
```
api('avant', 'pompe_toggle')     → MQTT: pulverisateur/cmd/avant {"cmd":"pompe_toggle"}
api('arriere', 'v2m_ouvrir')     → MQTT: pulverisateur/cmd/arriere {"cmd":"v2m_ouvrir"}
api('avant', 'auto_transfert','A') → MQTT: pulverisateur/cmd/avant {"cmd":"auto_transfert","val":"A"}
```

---

## 2. Mapping des commandes bouton par bouton

| Bouton UI | Ancien appel | Nouveau appel |
|-----------|-------------|---------------|
| Pompe ON/OFF | `cmd('p')` → `fetch('/pT')` | `api('avant','pompe_toggle')` |
| Vanne 3V | `cmd('v')` → `fetch('/vT')` | `api('avant','v3v_toggle')` |
| Phare AV | `cmd('l')` → `fetch('/lT')` | `api('avant','phares_av_toggle')` |
| Transfert | `api('tr','A')` | `api('avant','auto_transfert','A')` |
| Brassage | `api('br','A')` | `api('avant','auto_brassage','A')` |
| Arrêt urgence | `api('id','S')` | `api('urgence','stop')` |
| Vanne 2m Ouvrir | `api('v2','O')` | `api('arriere','v2m_ouvrir')` |
| Vanne 2m Fermer | `api('v2','F')` | `api('arriere','v2m_fermer')` |
| Vanne 2m Stop | `api('v2','S')` | `api('arriere','v2m_stop')` |
| Vanne BDR Ouvrir | `api('vb','O')` | `api('arriere','vbr_ouvrir')` |
| Vanne BDR Fermer | `api('vb','F')` | `api('arriere','vbr_fermer')` |
| Vanne BDR Stop | `api('vb','S')` | `api('arriere','vbr_stop')` |
| Phare AR | `api('li','T')` | `api('arriere','phares_ar_toggle')` |

---

## 3. Endpoint /status

### Format JSON : IDENTIQUE à l'existant
Le JavaScript côté client (`refresh()`) n'a pas changé. Le format JSON renvoyé
par `/status` est strictement compatible :

```json
{
    "p": true,           // Pompe active
    "v": false,          // Vanne 3V (true=transfert)
    "l": true,           // Phares avant
    "av_ok": true,       // Débitmètre connecté
    "av_flow": 12.5,     // Débit L/min
    "av_vide": false,    // Cuve vide
    "session_vol": 45.2, // Volume session
    "m_tr": false,       // Mode transfert actif
    "tr_target": 120,    // Volume cible transfert
    "m_br": true,        // Mode brassage actif
    "br_label": "MARCHE",
    "br_rem": 3.5,       // Temps restant (min)
    "br_pct": 42,        // Pourcentage phase
    "v2m": "O",          // Vanne 2m état
    "vbt": "S",          // Vanne BDR état
    "li": false          // Phares arrière
}
```

### Différence interne :
- **AVANT** : le handler `/status` lisait directement les variables globales
- **APRÈS** : le handler `/status` lit `s_etat_avant` et `s_etat_arriere`
  qui sont mis à jour via MQTT (messages retain des cartes)

---

## 4. Page Settings

### AVANT (Arduino) :
```javascript
// Les paramètres étaient envoyés en query string
fetch('/api/save_all?t_tgt=120&k_fact=4.72&...')
```

### APRÈS (ESP-IDF) :
```javascript
// Les paramètres sont envoyés en JSON via POST
fetch('/api/save_config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
        volume_transfert: 120,
        facteur_k: 4.72,
        seuil_debit: 1.2,
        delai_detection: 3000,
        timeout_vanne: 30000,
        temps_on: 600,
        temps_off: 300
    })
});
```

Le handler POST :
1. Parse le JSON
2. Met à jour la configuration locale
3. Incrémente le numéro de version
4. Publie sur MQTT `pulverisateur/configuration/mise_a_jour`
5. Sauvegarde en NVS

---

## 5. Ce qui n'a PAS changé

- **Le CSS** : identique à common_ui.h (dark theme, responsive, jauges)
- **Le layout HTML** : 3 slides (Pompage, Automate, Vannes)
- **Le JavaScript refresh()** : identique, même clés JSON
- **L'UX** : l'opérateur ne voit aucune différence

---

## 6. Flux complet d'une commande

```
[Opérateur appuie POMPE]
     │
     ▼
[Browser] → fetch('/api/cmd?cible=avant&cmd=pompe_toggle')
     │
     ▼
[Serveur HTTP carte AVANT] → mqtt_publier_commande_avant("pompe_toggle", NULL)
     │
     ▼
[MQTT Broker] → pulverisateur/cmd/avant {"cmd":"pompe_toggle"}
     │
     ▼
[Carte AVANT reçoit] → on_commande_recue() → actionneurs_pompe_toggle()
     │
     ▼
[Tâche principale] → mqtt_publier_etat_avant(état)
     │
     ▼
[MQTT Broker] → pulverisateur/etat/avant {JSON état complet} (retain)
     │
     ▼
[Serveur HTTP] → web_ui_update_etat_avant() (si souscrit aux états)
     │
     ▼
[Browser refresh()] → fetch('/status') → JSON → mise à jour de l'interface
```

**Note** : Sur la carte AVANT, comme le serveur HTTP et le handler MQTT sont
sur la même carte, la commande fait un aller-retour local via MQTT.
C'est intentionnel pour maintenir l'architecture uniforme (l'écran LVGL
et d'autres clients utilisent exactement le même chemin).
