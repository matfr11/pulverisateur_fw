#include "actionneurs.h"
#include "board_config.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

#ifdef HAVE_HTTP_SERVER
#include "http_server.h"
#endif

static const char* TAG = LOG_TAG_ACTIONNEURS;

//a voir avec claude
extern void mqtt_callback_automatisme(const char* topic, const char* data, int len); 
// Ajuste les arguments selon ta signature réelle
extern "C" {

#ifdef BOARD_TYPE_AVANT
void mqtt_callback_commande_avant(const char* actionneur, const char* commande) {
    ESP_LOGI(TAG, "📨 Commande AVANT: %s", actionneur);
    cJSON* json = cJSON_Parse(commande);
    if (!json) return;
    
    cJSON* action_json = cJSON_GetObjectItem(json, "action");
    if (action_json && cJSON_IsString(action_json)) {
        const char* action = action_json->valuestring;
        
        if (strcasecmp(actionneur, "pompe") == 0) {
            if (strcasecmp(action, "ON") == 0) actionneur_pompe_set(ETAT_POMPE_MARCHE);
            else if (strcasecmp(action, "OFF") == 0) actionneur_pompe_set(ETAT_POMPE_ARRET);
            else if (strcasecmp(action, "TOGGLE") == 0) actionneur_pompe_toggle();
            
#ifdef HAVE_HTTP_SERVER
            // Sync HTTP (Clé "p")
            http_status_update_pompe(actionneur_pompe_get() == ETAT_POMPE_MARCHE);
#endif
        } 
        else if (strcasecmp(actionneur, "vanne_3voies") == 0) {
            if (strcasecmp(action, "BRASSAGE") == 0) actionneur_vanne_3voies_set(POSITION_VANNE_3V_BRASSAGE);
            else if (strcasecmp(action, "TRANSFERT") == 0) actionneur_vanne_3voies_set(POSITION_VANNE_3V_TRANSFERT);
            else if (strcasecmp(action, "TOGGLE") == 0) actionneur_vanne_3voies_toggle();
            
#ifdef HAVE_HTTP_SERVER
            // Sync HTTP (Clé "v": false=brassage, true=transfert)
            http_status_update_vanne_3v(actionneur_vanne_3voies_get() == POSITION_VANNE_3V_TRANSFERT);
#endif
        }
        else if (strcasecmp(actionneur, "phares") == 0) {
            if (strcasecmp(action, "ON") == 0) {
                actionneur_phares_avant_set(true);
#ifdef HAVE_HTTP_SERVER
                http_status_update_phares_avant(true);
#endif
            }
            else if (strcasecmp(action, "OFF") == 0) {
                actionneur_phares_avant_set(false);
#ifdef HAVE_HTTP_SERVER
                http_status_update_phares_avant(false);
#endif
            }
            else if (strcasecmp(action, "TOGGLE") == 0) {
                actionneur_phares_avant_toggle();
                // Pas de _get() pour phares, donc pas de sync HTTP
            }
        }
    }
    cJSON_Delete(json);
}
#else
void mqtt_callback_commande_avant(const char* a, const char* c) { (void)a; (void)c; }
#endif

#ifdef BOARD_TYPE_ARRIERE
void mqtt_callback_commande_arriere(const char* actionneur, const char* commande) {
    ESP_LOGI(TAG, "📨 Commande ARRIERE: %s", actionneur);
    cJSON* json = cJSON_Parse(commande);
    if (!json) return;
    
    cJSON* action_json = cJSON_GetObjectItem(json, "action");
    if (action_json && cJSON_IsString(action_json)) {
        const char* action = action_json->valuestring;
        
        if (strcasecmp(actionneur, "vanne_2m") == 0) {
            actionneur_vanne_2m(action);
#ifdef HAVE_HTTP_SERVER
            // ✅ CORRIGÉ: Utiliser la fonction _get() au lieu de action[0]
            http_status_update_vanne_2m(actionneur_vanne_2m_get());
#endif
        } 
        else if (strcasecmp(actionneur, "vanne_bout_rampe") == 0) {
            actionneur_vanne_bout_rampe(action);
#ifdef HAVE_HTTP_SERVER
            // ✅ CORRIGÉ: Utiliser la fonction _get() au lieu de action[0]
            http_status_update_vanne_bout(actionneur_vanne_bout_rampe_get());
#endif
        }
        else if (strcasecmp(actionneur, "phares") == 0) {
            if (strcasecmp(action, "ON") == 0) {
                actionneur_phares_arriere_set(true);
#ifdef HAVE_HTTP_SERVER
                http_status_update_phares_arriere(true);
#endif
            }
            else if (strcasecmp(action, "OFF") == 0) {
                actionneur_phares_arriere_set(false);
#ifdef HAVE_HTTP_SERVER
                http_status_update_phares_arriere(false);
#endif
            }
            else if (strcasecmp(action, "TOGGLE") == 0) {
                actionneur_phares_arriere_toggle();
                // ✅ CORRIGÉ: Pas de _get() pour phares arrière
            }
        }
    }
    cJSON_Delete(json);
}
#else
void mqtt_callback_commande_arriere(const char* a, const char* c) { (void)a; (void)c; }
#endif

void mqtt_callback_configuration(const char* config_json) {
    ESP_LOGI(TAG, "⚙️ Config reçue: %s", config_json);
}


} // extern "C"