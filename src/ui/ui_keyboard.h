/**
 * @file ui_keyboard.h
 * @brief Widget Clavier Virtuel (UIKeyboard) - Définition et fonctions
 * 
 * Ce fichier définit le widget de clavier virtuel pour :
 * - Saisie de texte sans clavier physique
 * - Modes : lettres (ABC), chiffres (123), symboles (#+=)
 * - Dispositions : AZERTY, QWERTY, numérique, téléphone
 * - Touches spéciales : Shift, Backspace, Espace, Entrée
 * - Retour haptique visuel (touche pressée)
 * 
 * Disposition AZERTY (minuscules) :
 * ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
 * │  a  │  z  │  e  │  r  │  t  │  y  │  u  │  i  │  o  │  p  │
 * ├─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┤
 * │  q  │  s  │  d  │  f  │  g  │  h  │  j  │  k  │  l  │  m  │
 * ├─────┴─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┴─────┤
 * │   MAJ    │  w  │  x  │  c  │  v  │  b  │  n  │  EFFACER  │
 * ├──────────┴─────┼─────┴─────┴─────┴─────┼─────┴────────────┤
 * │    123 #+=    │       ESPACE           │    ENTREE       │
 * └───────────────┴─────────────────────────┴─────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_KEYBOARD_H
#define UI_KEYBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "ui_core.h"
#include "ui_theme.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Nombre de lignes du clavier */
#define KEYBOARD_ROWS           4

/** @brief Nombre maximum de touches par ligne */
#define KEYBOARD_MAX_KEYS       10

/** @brief Hauteur d'une touche */
#define KEYBOARD_KEY_HEIGHT     40

/** @brief Espacement entre les touches */
#define KEYBOARD_KEY_SPACING    2

// ============================================================
// SECTION 2 : MODES DU CLAVIER
// ============================================================

/**
 * @brief Modes du clavier virtuel
 */
typedef enum {
    KEYBOARD_MODE_LOWERCASE = 0,    // Lettres minuscules (abc)
    KEYBOARD_MODE_UPPERCASE = 1,    // Lettres majuscules (ABC)
    KEYBOARD_MODE_NUMBERS   = 2,    // Chiffres et symboles (123)
    KEYBOARD_MODE_SYMBOLS   = 3,    // Symboles supplémentaires (#+=)
    KEYBOARD_MODE_PHONE     = 4     // Clavier téléphonique (3x4)
} KeyboardMode;

/**
 * @brief Dispositions du clavier
 */
typedef enum {
    KEYBOARD_LAYOUT_AZERTY  = 0,    // Français
    KEYBOARD_LAYOUT_QWERTY  = 1,    // Anglais
    KEYBOARD_LAYOUT_QWERTZ  = 2,    // Allemand
    KEYBOARD_LAYOUT_NUMERIC = 3     // Numérique uniquement
} KeyboardLayout;

// ============================================================
// SECTION 3 : TOUCHE DU CLAVIER
// ============================================================

/**
 * @brief Types de touches spéciales
 */
typedef enum {
    KEY_TYPE_NORMAL     = 0,    // Caractère normal
    KEY_TYPE_SHIFT      = 1,    // Majuscule
    KEY_TYPE_BACKSPACE  = 2,    // Effacer
    KEY_TYPE_SPACE      = 3,    // Espace
    KEY_TYPE_ENTER      = 4,    // Entrée/Validation
    KEY_TYPE_MODE       = 5,    // Changement de mode
    KEY_TYPE_SYMBOLS    = 6,    // Symboles
    KEY_TYPE_DOT        = 7,    // Point
    KEY_TYPE_COMMA      = 8     // Virgule
} KeyType;

/**
 * @brief Structure d'une touche du clavier
 */
typedef struct {
    char label[8];          // Texte affiché sur la touche
    char primaryChar;       // Caractère principal (mode normal)
    char secondaryChar;     // Caractère secondaire (mode shift)
    KeyType type;           // Type de touche
    uint8_t width;          // Largeur relative (1 = normal, 2 = double)
    uint16_t x;             // Position X calculée
    uint16_t y;             // Position Y calculée
    uint16_t w;             // Largeur en pixels
    uint16_t h;             // Hauteur en pixels
    bool pressed;           // État pressé
} KeyboardKey;

// ============================================================
// SECTION 4 : STRUCTURE DU CLAVIER
// ============================================================

/**
 * @brief Widget Clavier Virtuel
 */
typedef struct UIKeyboard {
    UIWidget base;                      // Widget de base (héritage)
    
    // --- Configuration ---
    KeyboardMode mode;                  // Mode actuel
    KeyboardLayout layout;              // Disposition
    KeyboardKey keys[KEYBOARD_ROWS][KEYBOARD_MAX_KEYS]; // Matrice de touches
    uint8_t keysPerRow[KEYBOARD_ROWS];  // Nombre de touches par ligne
    
    // --- État ---
    bool shiftLock;                     // Verrouillage majuscule
    KeyboardKey* pressedKey;            // Touche actuellement pressée
    
    // --- Dimensions ---
    uint16_t keyboardWidth;             // Largeur totale
    uint16_t keyboardHeight;            // Hauteur totale
    uint16_t keyHeight;                 // Hauteur d'une touche
    
    // --- Apparence ---
    uint16_t keyBgColor;                // Fond des touches
    uint16_t keyTextColor;              // Texte des touches
    uint16_t specialKeyBgColor;         // Fond touches spéciales
    uint8_t keyCornerRadius;            // Rayon des coins
    
    // --- Callbacks ---
    void (*onKeyPress)(struct UIKeyboard* keyboard, char character, KeyType type);
    void (*onEnter)(struct UIKeyboard* keyboard);
    void (*onBackspace)(struct UIKeyboard* keyboard);
    void (*onModeChange)(struct UIKeyboard* keyboard, KeyboardMode newMode);
    
    // --- Widget associé (UITextBox) ---
    void* targetWidget;                 // Widget qui reçoit la saisie
} UIKeyboard;

// ============================================================
// SECTION 5 : FONCTIONS DE CRÉATION
// ============================================================

UIKeyboard* ui_keyboard_create(const char* name, UIRect rect);
UIKeyboard* ui_keyboard_create_full_width(const char* name, uint16_t y);

// ============================================================
// SECTION 6 : FONCTIONS DE CONFIGURATION
// ============================================================

void ui_keyboard_set_mode(UIKeyboard* keyboard, KeyboardMode mode);
void ui_keyboard_set_layout(UIKeyboard* keyboard, KeyboardLayout layout);
void ui_keyboard_toggle_shift(UIKeyboard* keyboard);
void ui_keyboard_set_target(UIKeyboard* keyboard, void* targetWidget);
void ui_keyboard_set_colors(UIKeyboard* keyboard, uint16_t keyBg, uint16_t keyText, uint16_t specialBg);

// ============================================================
// SECTION 7 : FONCTIONS D'ÉTAT
// ============================================================

KeyboardMode ui_keyboard_get_mode(UIKeyboard* keyboard);
bool ui_keyboard_is_shift_active(UIKeyboard* keyboard);
void ui_keyboard_reset(UIKeyboard* keyboard);

// ============================================================
// SECTION 8 : MACROS RAPIDES
// ============================================================

#define UI_KEYBOARD_CREATE(name, y) \
    ui_keyboard_create_full_width(name, y)

#define UI_KEYBOARD_SET_TARGET(kb, target) \
    ui_keyboard_set_target(kb, target)

// ============================================================
// SECTION 9 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_KEYBOARD_H