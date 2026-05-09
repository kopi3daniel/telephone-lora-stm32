/**
 * @file ui_button.h
 * @brief Widget Bouton - Définition et fonctions
 * 
 * Ce fichier est optionnel. La définition complète de UIButton
 * se trouve déjà dans ui_widgets.h.
 * 
 * Il est fourni ici comme référence pour un accès rapide
 * au widget bouton sans inclure tous les autres widgets.
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_BUTTON_H
#define UI_BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "ui_core.h"
#include "ui_styles.h"

// ============================================================
// STRUCTURE DU BOUTON
// ============================================================

/**
 * @brief Widget bouton
 * 
 * Hérite de UIWidget et ajoute :
 * - Un texte affiché au centre
 * - Une apparence stylée (via ButtonAppearance)
 * - Un état pressé
 * - Des callbacks onClick et onLongPress
 */
typedef struct UIButton {
    UIWidget base;                      // Widget de base (héritage)
    char text[64];                      // Texte du bouton
    ButtonAppearance appearance;        // Apparence (couleurs, polices, coins)
    bool pressed;                       // État pressé (pour feedback visuel)
    void (*onClick)(struct UIButton* button);       // Callback clic simple
    void (*onLongPress)(struct UIButton* button);   // Callback appui long
} UIButton;

// ============================================================
// FONCTIONS DE CRÉATION
// ============================================================

/**
 * @brief Crée un nouveau bouton
 * @param name Nom unique du widget
 * @param text Texte affiché
 * @param rect Position et taille
 * @return Pointeur vers le bouton créé
 */
UIButton* ui_button_create(const char* name, const char* text, UIRect rect);

// ============================================================
// FONCTIONS DE CONFIGURATION
// ============================================================

void ui_button_set_style(UIButton* button, ButtonStyle style);
void ui_button_set_text(UIButton* button, const char* text);
void ui_button_set_enabled(UIButton* button, bool enabled);
void ui_button_set_callback(UIButton* button, void (*callback)(UIButton*));
void ui_button_set_long_press_callback(UIButton* button, void (*callback)(UIButton*));

// ============================================================
// MACROS RAPIDES
// ============================================================

#define UI_BUTTON_CREATE(name, text, x, y, w, h) \
    ui_button_create(name, text, UI_RECT(x, y, w, h))

#define UI_BUTTON_SET_STYLE(btn, style) \
    ui_button_set_style(btn, style)

// ============================================================
// COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_BUTTON_H