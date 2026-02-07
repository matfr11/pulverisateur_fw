#ifndef ECRAN_UI_H
#define ECRAN_UI_H

#include "lvgl.h"
#include "types_pulverisateur.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback pour envoyer une commande MQTT depuis l'UI.
 * @param cible  "avant", "arriere", ou "urgence"
 * @param cmd    Nom de la commande (ex: "pompe_toggle", "auto_transfert")
 * @param val    Valeur optionnelle ("A", "S", "O", "F", NULL)
 */
typedef void (*ecran_cmd_cb_t)(const char *cible, const char *cmd, const char *val);

/**
 * @brief Callback pour sauvegarder la configuration modifiée.
 */
typedef void (*ecran_save_cfg_cb_t)(const configuration_t *cfg);

/**
 * @brief Créer l'interface LVGL (écran principal + settings).
 *        Appeler APRÈS bsp_display_start et ENTRE lock/unlock.
 */
void ecran_ui_creer(ecran_cmd_cb_t cmd_cb, ecran_save_cfg_cb_t save_cb);

/**
 * @brief Mettre à jour l'affichage avec l'état de la carte AVANT.
 *        Appeler depuis la tâche LVGL ou entre lock/unlock.
 */
void ecran_ui_update_avant(const etat_carte_avant_t *etat);

/**
 * @brief Mettre à jour l'affichage avec l'état de la carte ARRIÈRE.
 */
void ecran_ui_update_arriere(const etat_carte_arriere_t *etat);

/**
 * @brief Mettre à jour les infos réseau dans la barre supérieure.
 */
void ecran_ui_update_reseau(const char *master, bool link_av, bool link_ar);

/**
 * @brief Charger la configuration dans la page settings.
 */
void ecran_ui_set_config(const configuration_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* ECRAN_UI_H */
