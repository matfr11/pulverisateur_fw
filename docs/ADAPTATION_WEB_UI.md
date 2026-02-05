# Guide d'Adaptation - Web UI Existante → Architecture MQTT

## 📋 Vue d'ensemble

La Web UI existante (ui_main.h, ui_settings.h) utilise des appels HTTP directs.  
Elle doit être adaptée pour communiquer via MQTT tout en conservant la même logique visuelle.

---

## 🔄 Principe de transformation

### AVANT (Web UI actuelle)
```
Navigateur → HTTP Request → ESP32 → Actionneur direct
```

### APRÈS (Architecture MQTT)
```
Navigateur → HTTP Request → ESP32 → MQTT Publish → Broker → ESP32(s) → Actionneurs
                                    ↑
                                    └─ Souscription états MQTT → WebSocket → Navigateur
```

---

## 🛠️ Modifications nécessaires

### 1. Backend ESP32 - Serveur Web

**Fichier à créer**: `carte_relais/components/web_server/web_server.cpp`

```cpp
/**
 * @file web_server.cpp
 * @brief Serveur HTTP + WebSocket pour Web UI
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include <cJSON.h>

static const char* TAG = "WEB_SERVER";

// ============================================================================
// ANCIEN CODE (à remplacer)
// ============================================================================

// AVANT: Commande directe
void handle_pump_toggle(httpd_req_t *req) {
    gpio_set_level(GPIO_PUMP, !gpio_get_level(GPIO_PUMP)); // ❌ Direct
    httpd_resp_send(req, "OK", 2);
}

// ============================================================================
// NOUVEAU CODE (via MQTT)
// ============================================================================

// APRÈS: Commande via MQTT
void handle_pump_toggle(httpd_req_t *req) {
    // Récupérer action depuis query string
    char query[100];
    httpd_req_get_url_query_str(req, query, sizeof(query));
    
    char action[10];
    httpd_query_key_value(query, "action", action, sizeof(action));
    
    // Construire payload JSON
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "action", action);
    char *payload = cJSON_PrintUnformatted(json);
    
    // Publier via MQTT
    extern void mqtt_publish(const char* topic, const char* payload, int qos);
    mqtt_publish("pulverisateur/commandes/avant/pompe", payload, 1);
    
    cJSON_Delete(json);
    free(payload);
    
    // Réponse HTTP
    httpd_resp_send(req, "OK", 2);
}
```

### 2. Endpoint /status - États en temps réel

**ANCIEN** (lecture directe GPIO):
```cpp
String getStatus() {
    String json = "{";
    json += "\"p\":" + String(digitalRead(GPIO_PUMP)) + ",";
    json += "\"v\":" + String(digitalRead(GPIO_VALVE)) + ",";
    // ... lecture directe GPIO
    json += "}";
    return json;
}
```

**NOUVEAU** (depuis état système MQTT):
```cpp
esp_err_t handle_status(httpd_req_t *req) {
    // Récupérer état depuis structure globale (alimentée par MQTT)
    const etat_complet_systeme_t* etat = app_get_etat_systeme();
    
    // Construire JSON
    cJSON *json = cJSON_CreateObject();
    
    // Pompe
    cJSON_AddNumberToObject(json, "p", 
        etat->actionneurs_avant.pompe == ETAT_POMPE_MARCHE ? 1 : 0);
    
    // Vanne 3 voies
    cJSON_AddNumberToObject(json, "v", 
        etat->actionneurs_avant.vanne_3voies == POSITION_VANNE_3V_TRANSFERT ? 1 : 0);
    
    // Débitmètre
    cJSON_AddNumberToObject(json, "av_flow", 
        etat->debitmetre.debit_instantane_lpm);
    
    // Cuve vide
    cJSON_AddBoolToObject(json, "av_vide", 
        etat->automatismes.etat_cuve_avant == ETAT_CUVE_AVANT_VIDE);
    
    // Automatismes
    cJSON_AddBoolToObject(json, "m_tr", 
        etat->automatismes.etat_transfert == ETAT_TRANSFERT_EN_COURS);
    cJSON_AddNumberToObject(json, "session_vol", 
        etat->automatismes.volume_transfere_cycle_litres);
    cJSON_AddNumberToObject(json, "tr_target", 
        etat->config.automatismes.volume_transfert_litres);
    
    // Vannes arrière (si carte arrière connectée via MQTT)
    const char* v2m_state = "S"; // Par défaut STOP
    if (etat->actionneurs_arriere.vanne_2m == ETAT_VANNE_3FILS_OUVERTURE) {
        v2m_state = "O";
    } else if (etat->actionneurs_arriere.vanne_2m == ETAT_VANNE_3FILS_FERMETURE) {
        v2m_state = "F";
    }
    cJSON_AddStringToObject(json, "v2m", v2m_state);
    
    // Convertir en string et envoyer
    char *json_string = cJSON_PrintUnformatted(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    cJSON_Delete(json);
    free(json_string);
    
    return ESP_OK;
}
```

### 3. Endpoint /api/cmd - Commandes génériques

**NOUVEAU**:
```cpp
esp_err_t handle_api_cmd(httpd_req_t *req) {
    char query[200];
    httpd_req_get_url_query_str(req, query, sizeof(query));
    
    char type[10], action[10];
    httpd_query_key_value(query, "t", type, sizeof(type));
    httpd_query_key_value(query, "a", action, sizeof(action));
    
    char topic[128];
    cJSON *json = cJSON_CreateObject();
    
    // Vanne 2m
    if (strcmp(type, "v2") == 0) {
        snprintf(topic, sizeof(topic), "pulverisateur/commandes/arriere/vanne_2m");
        if (strcmp(action, "O") == 0) {
            cJSON_AddStringToObject(json, "action", "OUVRIR");
        } else if (strcmp(action, "F") == 0) {
            cJSON_AddStringToObject(json, "action", "FERMER");
        } else {
            cJSON_AddStringToObject(json, "action", "STOP");
        }
    }
    // Vanne bout de rampe
    else if (strcmp(type, "vb") == 0) {
        snprintf(topic, sizeof(topic), "pulverisateur/commandes/arriere/vanne_bout_rampe");
        if (strcmp(action, "O") == 0) {
            cJSON_AddStringToObject(json, "action", "OUVRIR");
        } else if (strcmp(action, "F") == 0) {
            cJSON_AddStringToObject(json, "action", "FERMER");
        } else {
            cJSON_AddStringToObject(json, "action", "STOP");
        }
    }
    // Transfert
    else if (strcmp(type, "tr") == 0) {
        if (strcmp(action, "A") == 0) {
            snprintf(topic, sizeof(topic), "pulverisateur/automatismes/transfert/activer");
            cJSON_AddStringToObject(json, "mode", "SANS_SONDE");
        } else {
            snprintf(topic, sizeof(topic), "pulverisateur/automatismes/transfert/desactiver");
        }
    }
    // Brassage
    else if (strcmp(type, "br") == 0) {
        if (strcmp(action, "A") == 0) {
            snprintf(topic, sizeof(topic), "pulverisateur/automatismes/brassage/activer");
        } else {
            snprintf(topic, sizeof(topic), "pulverisateur/automatismes/brassage/desactiver");
        }
    }
    // Arrêt urgence tous automatismes
    else if (strcmp(type, "id") == 0 && strcmp(action, "S") == 0) {
        // Publier sur plusieurs topics
        mqtt_publish("pulverisateur/automatismes/transfert/desactiver", "{}", 1);
        mqtt_publish("pulverisateur/automatismes/brassage/desactiver", "{}", 1);
        
        httpd_resp_send(req, "OK", 2);
        cJSON_Delete(json);
        return ESP_OK;
    }
    
    // Publier commande
    char *payload = cJSON_PrintUnformatted(json);
    mqtt_publish(topic, payload, 1);
    
    cJSON_Delete(json);
    free(payload);
    
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}
```

### 4. Endpoint /api/save_all - Sauvegarde configuration

**NOUVEAU**:
```cpp
esp_err_t handle_save_all(httpd_req_t *req) {
    char query[512];
    httpd_req_get_url_query_str(req, query, sizeof(query));
    
    // Parser tous les paramètres
    char buf[32];
    configuration_systeme_t new_config;
    memcpy(&new_config, app_get_etat_systeme()->config, sizeof(configuration_systeme_t));
    
    // Volume transfert
    if (httpd_query_key_value(query, "t_tgt", buf, sizeof(buf)) == ESP_OK) {
        new_config.automatismes.volume_transfert_litres = atof(buf);
    }
    
    // Facteur K
    if (httpd_query_key_value(query, "k_fact", buf, sizeof(buf)) == ESP_OK) {
        new_config.capteurs.facteur_k_debitmetre = atof(buf);
    }
    
    // Seuil débit vide
    if (httpd_query_key_value(query, "e_flow", buf, sizeof(buf)) == ESP_OK) {
        new_config.securite.seuil_debit_cuve_vide = atof(buf);
    }
    
    // Délai détection
    if (httpd_query_key_value(query, "e_out", buf, sizeof(buf)) == ESP_OK) {
        new_config.securite.delai_detection_ms = atoi(buf) * 1000; // sec -> ms
    }
    
    // Timeout vannes
    if (httpd_query_key_value(query, "v_timeout", buf, sizeof(buf)) == ESP_OK) {
        new_config.securite.timeout_vanne_3fils_ms = atoi(buf) * 1000;
    }
    
    // Brassage ON
    if (httpd_query_key_value(query, "br_on", buf, sizeof(buf)) == ESP_OK) {
        new_config.automatismes.temps_brassage_on_sec = atoi(buf) * 60; // min -> sec
    }
    
    // Brassage OFF
    if (httpd_query_key_value(query, "br_off", buf, sizeof(buf)) == ESP_OK) {
        new_config.automatismes.temps_brassage_pause_sec = atoi(buf) * 60;
    }
    
    // Incrémenter version
    new_config.version_config++;
    
    // Construire JSON de configuration
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "version", new_config.version_config);
    
    cJSON *securite = cJSON_CreateObject();
    cJSON_AddNumberToObject(securite, "seuil_debit_cuve_vide", 
        new_config.securite.seuil_debit_cuve_vide);
    cJSON_AddNumberToObject(securite, "delai_detection_ms", 
        new_config.securite.delai_detection_ms);
    cJSON_AddNumberToObject(securite, "timeout_vanne_3fils_ms", 
        new_config.securite.timeout_vanne_3fils_ms);
    cJSON_AddItemToObject(json, "securite", securite);
    
    cJSON *automatismes = cJSON_CreateObject();
    cJSON_AddNumberToObject(automatismes, "volume_transfert_litres", 
        new_config.automatismes.volume_transfert_litres);
    cJSON_AddNumberToObject(automatismes, "temps_brassage_on_sec", 
        new_config.automatismes.temps_brassage_on_sec);
    cJSON_AddNumberToObject(automatismes, "temps_brassage_pause_sec", 
        new_config.automatismes.temps_brassage_pause_sec);
    cJSON_AddItemToObject(json, "automatismes", automatismes);
    
    cJSON *capteurs = cJSON_CreateObject();
    cJSON_AddNumberToObject(capteurs, "facteur_k_debitmetre", 
        new_config.capteurs.facteur_k_debitmetre);
    cJSON_AddItemToObject(json, "capteurs", capteurs);
    
    // Publier sur MQTT (retain pour que tous reçoivent)
    char *payload = cJSON_PrintUnformatted(json);
    mqtt_publish_retain("pulverisateur/configuration/mise_a_jour", payload);
    
    cJSON_Delete(json);
    free(payload);
    
    // Sauvegarder localement
    app_save_configuration(&new_config);
    
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}
```

### 5. HTML - Pas de modification nécessaire

Les fichiers `ui_main.h` et `ui_settings.h` **n'ont pas besoin d'être modifiés**.

Le JavaScript continue d'appeler:
- `fetch('/status')` → Backend renvoie JSON depuis état MQTT
- `fetch('/pT')` → Backend publie sur MQTT
- `fetch('/api/cmd?t=v2&a=O')` → Backend publie sur MQTT
- `fetch('/api/save_all?...')` → Backend publie config sur MQTT

---

## 📡 Flux complet exemple

### Exemple: Activation de la pompe depuis Web UI

```
1. Utilisateur clique sur bouton POMPE dans navigateur
   ↓
2. JavaScript: fetch('/pT')
   ↓
3. ESP32 Web Server: handle_pump_toggle()
   ↓
4. Publie MQTT: "pulverisateur/commandes/avant/pompe" {"action":"TOGGLE"}
   ↓
5. Broker MQTT reçoit et diffuse
   ↓
6. Carte AVANT (gestion_actionneurs) reçoit message MQTT
   ↓
7. Active GPIO_RELAIS_POMPE
   ↓
8. Publie état: "pulverisateur/etats/avant/pompe" {"etat":1} (retain)
   ↓
9. Web Server reçoit état MQTT (callback)
   ↓
10. Met à jour etat_complet_systeme_t.actionneurs_avant.pompe
   ↓
11. JavaScript: setInterval(() => fetch('/status'), 1000)
   ↓
12. ESP32 Web Server: handle_status() renvoie JSON avec pompe=1
   ↓
13. JavaScript met à jour classe bouton: 'btn-full active-green'
```

---

## 🔧 Fichiers à créer

### 1. web_server/CMakeLists.txt
```cmake
idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server mqtt cJSON
)
```

### 2. web_server/include/web_server.h
```cpp
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"

esp_err_t web_server_start(void);
void web_server_stop(void);

#endif
```

### 3. web_server/web_server.cpp
```cpp
#include "web_server.h"
#include "ui_main.h"        // HTML existant
#include "ui_settings.h"    // HTML existant
#include "common_ui.h"      // Styles CSS existants
#include "app_init.h"

// Implémentation complète selon exemples ci-dessus
```

---

## ✅ Checklist d'adaptation

### Backend
- [ ] Créer composant `web_server/`
- [ ] Implémenter `handle_status()` depuis état MQTT
- [ ] Implémenter `handle_pump_toggle()` via MQTT
- [ ] Implémenter `handle_api_cmd()` via MQTT
- [ ] Implémenter `handle_save_all()` via MQTT
- [ ] Ajouter callbacks MQTT pour mise à jour états
- [ ] Intégrer dans `app_main.cpp`

### Frontend (aucune modification)
- [x] ui_main.h reste identique
- [x] ui_settings.h reste identique
- [x] common_ui.h reste identique

### Tests
- [ ] Tester commandes manuelles
- [ ] Tester automatismes
- [ ] Tester sauvegarde configuration
- [ ] Tester multi-utilisateurs simultanés
- [ ] Tester avec carte ARRIÈRE déconnectée

---

## 🎯 Avantages de l'architecture MQTT

### Avant (HTTP direct)
- ❌ Web UI doit être sur la même carte que l'actionneur
- ❌ Pas de synchronisation entre cartes
- ❌ Pas d'interface LVGL possible en parallèle

### Après (MQTT)
- ✅ Web UI peut être sur n'importe quelle carte
- ✅ Synchronisation automatique (retain)
- ✅ Web UI + Interface LVGL en même temps
- ✅ Failover Master/Slave transparent
- ✅ Plusieurs navigateurs synchronisés

---

## 📝 Notes importantes

1. **Retain sur états**: Les topics d'états doivent utiliser `retain=true` pour que tout nouveau client (navigateur, écran LVGL) reçoive l'état actuel immédiatement.

2. **QoS**: 
   - Commandes: QoS 1 (au moins une fois)
   - États: QoS 0 (best effort, compensé par retain)

3. **WebSocket** (optionnel futur): Pour éviter le polling de `/status`, on peut ajouter un WebSocket qui diffuse les changements d'états en temps réel.

4. **Compatibilité**: L'ancien code HTML/JS continue de fonctionner sans modification !

---

**Prochaine étape**: Implémenter le composant `web_server/` avec les handlers ci-dessus.
