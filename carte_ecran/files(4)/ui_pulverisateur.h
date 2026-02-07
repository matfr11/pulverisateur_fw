#ifndef UI_PULVERISATEUR_H
#define UI_PULVERISATEUR_H

#include "mqtt_ecran.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Créer l'interface LVGL complète.
 *        Doit être appelé avec le lock LVGL actif.
 */
void ui_creer(void);

/**
 * @brief Mettre à jour l'UI avec l'état système courant.
 *        Doit être appelé avec le lock LVGL actif.
 */
void ui_rafraichir(const etat_systeme_t *etat);

#ifdef __cplusplus
}
#endif

#endif /* UI_PULVERISATEUR_H */
