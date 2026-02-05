# Composant gestion_http

## 📋 Description

Serveur HTTP embarqué pour l'interface web du pulvérisateur.

## 🎯 Fonctionnalités

- ✅ Serveur HTTP sur port 80
- ✅ Interface web responsive (mobile/desktop)
- ✅ Dashboard temps réel (refresh 1 Hz)
- ✅ Page de configuration
- ✅ API REST pour commandes
- ✅ Raccourcis toggle actionneurs

## 📡 Endpoints

### Pages HTML

- `GET /` → Dashboard principal
- `GET /settings` → Page configuration

### API REST

- `GET /status` → État système (JSON)
- `GET /api/cmd?t=TYPE&a=ACTION` → Commande actionneur
- `GET /api/save_all?...` → Sauvegarde configuration

### Raccourcis Toggle

- `GET /pT` → Toggle pompe
- `GET /vT` → Toggle vanne 3 voies
- `GET /lT` → Toggle phares avant

## 🔌 Intégration dans app_main.cpp

### 1. Ajouter au CMakeLists.txt de main/

```cmake
idf_component_register(
    SRCS 
        "app_main.cpp"
        "app_init.cpp"
        "app_role.cpp"
    REQUIRES 
        # ... autres composants ...
        gestion_http  # ← AJOUTER
)
```

### 2. Ajouter dans app_main.cpp

```cpp
#include "http_server.h"

void app_main(void) {
    // ... initialisation WiFi, MQTT, etc. ...
    
    // Démarrer serveur HTTP (après WiFi opérationnel)
    ESP_LOGI(TAG, "=== PHASE 8: Serveur HTTP ===");
    esp_err_t ret = http_server_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur démarrage serveur HTTP");
    } else {
        ESP_LOGI(TAG, "✅ Serveur HTTP démarré");
        ESP_LOGI(TAG, "   URL: http://192.168.4.1");
    }
}
```

### 3. Connecter les états MQTT → HTTP

Dans les callbacks MQTT (mqtt_publish.cpp ou actionneurs_mqtt.cpp), ajouter :

```cpp
#include "http_server.h"

// Exemple: callback pompe
void mqtt_callback_commande_avant(const char* actionneur, const char* commande) {
    // ... traiter commande ...
    
    if (strcasecmp(actionneur, "pompe") == 0) {
        if (strcasecmp(action, "ON") == 0) {
            actionneur_pompe_set(ETAT_POMPE_MARCHE);
            http_status_update_pompe(true);  // ← Mettre à jour HTTP
        } else {
            actionneur_pompe_set(ETAT_POMPE_ARRET);
            http_status_update_pompe(false); // ← Mettre à jour HTTP
        }
    }
}
```

### 4. Mettre à jour depuis les publications MQTT

Dans les fonctions de publication, ajouter les mises à jour HTTP :

```cpp
void capteurs_publier_debit(float debit, float volume) {
    // Publier MQTT
    mqtt_publish(...);
    
    // Mettre à jour HTTP
    http_status_update_debit(debit, volume);
}
```

## 🎨 Format JSON /status

```json
{
  "p": true,              // Pompe marche
  "v": false,             // Vanne 3V (false=brassage)
  "l": false,             // Phares avant
  "li": false,            // Phares arrière
  "av_flow": 12.5,        // Débit L/min
  "av_ok": true,          // Carte avant OK
  "av_vide": false,       // Cuve vide
  "v2m": "S",             // Vanne 2m (O/F/S)
  "vbt": "F",             // Vanne bout (O/F/S)
  "m_tr": false,          // Transfert actif
  "tr_target": 1000,      // Volume cible (L)
  "session_vol": 0,       // Volume transféré (L)
  "m_br": true,           // Brassage actif
  "br_rem": 8,            // Temps restant (min)
  "br_pct": 60,           // Pourcentage cycle
  "br_label": "MARCHE"    // Label état
}
```

## 🔐 Sécurité

- Port 80 (HTTP non sécurisé)
- Pas d'authentification (réseau WiFi privé)
- Accès limité au réseau WiFi "Pulve"

## 📝 TODO

- [ ] Charger valeurs config depuis NVS dans page /settings
- [ ] Implémenter sauvegarde config via /api/save_all
- [ ] Ajouter endpoint arrêt d'urgence
- [ ] Ajouter logs système dans interface
- [ ] WebSocket pour updates temps réel (alternative au polling)

## 🚀 Utilisation

1. Connecter au WiFi "Pulve"
2. Ouvrir navigateur : `http://192.168.4.1`
3. Contrôler les actionneurs via l'interface
4. Configurer via ⚙️ (coin supérieur droit)
