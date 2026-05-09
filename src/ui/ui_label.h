/**
 * @file ui_label.h
 * @brief Widget Label (texte) - Définition et fonctions
 * 
 * Ce fichier est optionnel. La définition complète de UILabel
 * se trouve déjà dans ui_widgets.h (section 2).
 * 
 * Il est fourni ici comme référence pour un accès rapide
 * au widget label sans inclure tous les autres widgets.
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_LABEL_H
#define UI_LABEL_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "ui_core.h"
#include "ui_styles.h"

// ============================================================
// STRUCTURE DU LABEL
// ============================================================

/**
 * @brief Widget Label (texte)
 * 
 * Hérite de UIWidget et ajoute :
 * - Un texte (jusqu'à 256 caractères)
 * - Une apparence stylée (via LabelAppearance)
 * - Gestion du retour à la ligne automatique
 * - Alignement horizontal
 * - Possibilité d'ajuster la taille automatiquement
 */
typedef struct UILabel {
    UIWidget base;                      // Widget de base (héritage)
    char text[256];                     // Texte à afficher
    LabelAppearance appearance;         // Apparence (couleur, police, taille)
    bool autoSize;                      // Ajuster automatiquement la taille du widget
    uint16_t maxWidth;                  // Largeur maximale (pour retour à la ligne)
    bool wordWrap;                      // Activer le retour à la ligne
} UILabel;

// ============================================================
// FONCTIONS DE CRÉATION
// ============================================================

/**
 * @brief Crée un nouveau label
 * @param name Nom unique du widget
 * @param text Texte initial
 * @param rect Position et taille
 * @return Pointeur vers le label créé
 */
UILabel* ui_label_create(const char* name, const char* text, UIRect rect);

// ============================================================
// FONCTIONS DE CONFIGURATION
// ============================================================

/**
 * @brief Définit le style du label
 * @param label Label à modifier
 * @param style Style prédéfini (TITLE, SUBTITLE, BODY, CAPTION, HINT, ERROR, SUCCESS)
 */
void ui_label_set_style(UILabel* label, LabelStyle style);

/**
 * @brief Change le texte du label
 * @param label Label à modifier
 * @param text Nouveau texte (max 255 caractères)
 */
void ui_label_set_text(UILabel* label, const char* text);

/**
 * @brief Obtient le texte actuel du label
 * @param label Label
 * @return Pointeur vers le texte (lecture seule)
 */
const char* ui_label_get_text(UILabel* label);

/**
 * @brief Change la couleur du texte
 * @param label Label à modifier
 * @param color Couleur RGB565
 */
void ui_label_set_color(UILabel* label, uint16_t color);

/**
 * @brief Définit l'alignement horizontal du texte
 * @param label Label à modifier
 * @param align Alignement (UI_ALIGN_LEFT, UI_ALIGN_CENTER, UI_ALIGN_RIGHT)
 */
void ui_label_set_alignment(UILabel* label, UIAlign align);

/**
 * @brief Active/désactive le retour à la ligne automatique
 * @param label Label à modifier
 * @param wrap true pour activer le word wrap
 */
void ui_label_set_word_wrap(UILabel* label, bool wrap);

/**
 * @brief Définit la largeur maximale (pour le retour à la ligne)
 * @param label Label à modifier
 * @param maxWidth Largeur maximale en pixels (0 = pas de limite)
 */
void ui_label_set_max_width(UILabel* label, uint16_t maxWidth);

/**
 * @brief Active/désactive l'ajustement automatique de la taille
 * @param label Label à modifier
 * @param auto true pour ajuster automatiquement
 */
void ui_label_set_auto_size(UILabel* label, bool autoSize);

/**
 * @brief Ajoute du texte à la fin du label
 * @param label Label à modifier
 * @param text Texte à ajouter
 */
void ui_label_append_text(UILabel* label, const char* text);

/**
 * @brief Efface le contenu du label
 * @param label Label à modifier
 */
void ui_label_clear(UILabel* label);

// ============================================================
// MACROS RAPIDES
// ============================================================

#define UI_LABEL_CREATE(name, text, x, y, w, h) \
    ui_label_create(name, text, UI_RECT(x, y, w, h))

#define UI_LABEL_SET_STYLE(label, style) \
    ui_label_set_style(label, style)

#define UI_LABEL_SET_TEXT(label, text) \
    ui_label_set_text(label, text)

// ============================================================
// COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_LABEL_H