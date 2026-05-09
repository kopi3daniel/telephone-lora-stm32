/**
 * @file ui_textbox.h
 * @brief Widget Zone de Texte (TextBox) - Définition et fonctions
 * 
 * Ce fichier est optionnel. La définition complète de UITextBox
 * se trouve déjà dans ui_widgets.h (section 3).
 * 
 * Fonctionnalités :
 * - Saisie de texte avec curseur clignotant
 * - Mode mot de passe (caractères masqués)
 * - Texte indicatif (placeholder)
 * - Longueur maximale configurable
 * - Callbacks onTextChanged et onSubmit
 * - Styles visuels (normal, outlined, filled, underlined)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_TEXTBOX_H
#define UI_TEXTBOX_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "ui_core.h"
#include "ui_styles.h"

// ============================================================
// STRUCTURE DE LA ZONE DE TEXTE
// ============================================================

/**
 * @brief Widget Zone de Texte
 * 
 * Hérite de UIWidget et ajoute :
 * - Un buffer de texte (256 caractères max)
 * - Un curseur d'édition avec position
 * - Un texte indicatif (placeholder) affiché quand vide
 * - Un mode sécurisé (mot de passe)
 * - Des callbacks pour les changements et la validation
 */
typedef struct UITextBox {
    UIWidget base;                      // Widget de base (héritage)
    
    // --- Contenu ---
    char text[256];                     // Texte saisi
    uint16_t maxLength;                 // Longueur maximale
    uint16_t cursorPos;                 // Position du curseur (0 = début)
    char placeholder[64];               // Texte indicatif (affiché si vide)
    
    // --- Apparence ---
    TextBoxAppearance appearance;       // Style visuel
    
    // --- Comportement ---
    bool secure;                        // Mode mot de passe (affiche ***)
    bool editable;                      // Éditable ou lecture seule ?
    bool cursorVisible;                 // Curseur visible ?
    uint32_t cursorBlinkTime;           // Timer clignotement curseur
    uint16_t cursorBlinkIntervalMs;     // Intervalle clignotement (défaut: 500ms)
    
    // --- Callbacks ---
    void (*onTextChanged)(struct UITextBox* textbox);  // Texte modifié
    void (*onSubmit)(struct UITextBox* textbox);        // Validation (Enter/OK)
    void (*onFocus)(struct UITextBox* textbox, bool focused); // Focus changé
    
    // --- Validation ---
    bool (*validator)(struct UITextBox* textbox, char c);  // Valide un caractère
    char* allowedChars;                 // Caractères autorisés (NULL = tous)
    uint16_t minLength;                 // Longueur minimale
} UITextBox;

// ============================================================
// FONCTIONS DE CRÉATION
// ============================================================

/**
 * @brief Crée une nouvelle zone de texte
 * @param name Nom unique du widget
 * @param rect Position et taille
 * @return Pointeur vers la zone de texte créée
 */
UITextBox* ui_textbox_create(const char* name, UIRect rect);

// ============================================================
// FONCTIONS DE CONFIGURATION DU STYLE
// ============================================================

void ui_textbox_set_style(UITextBox* textbox, TextBoxStyle style);

// ============================================================
// FONCTIONS DE GESTION DU TEXTE
// ============================================================

/**
 * @brief Définit le texte complet
 */
void ui_textbox_set_text(UITextBox* textbox, const char* text);

/**
 * @brief Récupère le texte saisi
 * @return Pointeur vers le texte (lecture seule)
 */
const char* ui_textbox_get_text(UITextBox* textbox);

/**
 * @brief Récupère la longueur du texte
 */
uint16_t ui_textbox_get_length(UITextBox* textbox);

/**
 * @brief Définit le texte indicatif (placeholder)
 */
void ui_textbox_set_placeholder(UITextBox* textbox, const char* placeholder);

/**
 * @brief Efface tout le texte
 */
void ui_textbox_clear(UITextBox* textbox);

// ============================================================
// FONCTIONS D'ÉDITION
// ============================================================

/**
 * @brief Insère un caractère à la position du curseur
 * @return true si le caractère a été inséré
 */
bool ui_textbox_insert_char(UITextBox* textbox, char c);

/**
 * @brief Supprime le caractère avant le curseur (Backspace)
 */
void ui_textbox_delete_char(UITextBox* textbox);

/**
 * @brief Supprime le caractère après le curseur (Delete)
 */
void ui_textbox_delete_forward(UITextBox* textbox);

/**
 * @brief Déplace le curseur à gauche
 */
void ui_textbox_cursor_left(UITextBox* textbox);

/**
 * @brief Déplace le curseur à droite
 */
void ui_textbox_cursor_right(UITextBox* textbox);

/**
 * @brief Déplace le curseur au début
 */
void ui_textbox_cursor_home(UITextBox* textbox);

/**
 * @brief Déplace le curseur à la fin
 */
void ui_textbox_cursor_end(UITextBox* textbox);

// ============================================================
// FONCTIONS DE COMPORTEMENT
// ============================================================

void ui_textbox_set_secure(UITextBox* textbox, bool secure);
void ui_textbox_set_editable(UITextBox* textbox, bool editable);
void ui_textbox_set_max_length(UITextBox* textbox, uint16_t maxLength);
void ui_textbox_set_min_length(UITextBox* textbox, uint16_t minLength);
bool ui_textbox_is_valid(UITextBox* textbox);

// ============================================================
// FONCTIONS DE VALIDATION
// ============================================================

/**
 * @brief Définit un validateur personnalisé
 * @param validator Fonction retournant true si le caractère est accepté
 */
void ui_textbox_set_validator(UITextBox* textbox, bool (*validator)(UITextBox*, char));

/**
 * @brief Restreint aux chiffres uniquement (mode numérique)
 */
void ui_textbox_set_numeric_only(UITextBox* textbox);

/**
 * @brief Restreint aux lettres uniquement
 */
void ui_textbox_set_alpha_only(UITextBox* textbox);

/**
 * @brief Restreint aux caractères alphanumériques
 */
void ui_textbox_set_alphanumeric_only(UITextBox* textbox);

/**
 * @brief Restreint à un numéro de téléphone (chiffres, +, espaces)
 */
void ui_textbox_set_phone_only(UITextBox* textbox);

// ============================================================
// FONCTIONS DE CALLBACKS
// ============================================================

void ui_textbox_set_on_text_changed(UITextBox* textbox, void (*callback)(UITextBox*));
void ui_textbox_set_on_submit(UITextBox* textbox, void (*callback)(UITextBox*));
void ui_textbox_set_on_focus(UITextBox* textbox, void (*callback)(UITextBox*, bool));

// ============================================================
// MACROS RAPIDES
// ============================================================

#define UI_TEXTBOX_CREATE(name, x, y, w, h) \
    ui_textbox_create(name, UI_RECT(x, y, w, h))

#define UI_TEXTBOX_SET_TEXT(tb, text) \
    ui_textbox_set_text(tb, text)

#define UI_TEXTBOX_GET_TEXT(tb) \
    ui_textbox_get_text(tb)

// ============================================================
// VALIDATEURS PRÉDÉFINIS
// ============================================================

bool ui_textbox_validator_numeric(UITextBox* textbox, char c);
bool ui_textbox_validator_alpha(UITextBox* textbox, char c);
bool ui_textbox_validator_alphanumeric(UITextBox* textbox, char c);
bool ui_textbox_validator_phone(UITextBox* textbox, char c);
bool ui_textbox_validator_email(UITextBox* textbox, char c);
bool ui_textbox_validator_ip_address(UITextBox* textbox, char c);

// ============================================================
// COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_TEXTBOX_H