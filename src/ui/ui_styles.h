/**
 * @file ui_styles.h
 * @brief Styles prédéfinis pour les widgets de l'interface
 * 
 * Ce fichier définit des styles prêts à l'emploi pour tous
 * les types de widgets, basés sur le thème actif.
 * 
 * Chaque widget peut avoir un style qui définit :
 * - Les couleurs (fond, texte, bordure)
 * - Les dimensions (hauteur, padding, radius)
 * - Les polices (taille, style)
 * - Les états (normal, hover, pressed, disabled)
 * 
 * Styles disponibles par widget :
 * - Button   : 10 styles (primary, secondary, call, end, etc.)
 * - Label    : 8 styles (title, subtitle, body, caption, etc.)
 * - TextBox  : 5 styles (normal, outlined, filled, etc.)
 * - List     : 4 styles (plain, grouped, card, etc.)
 * - Slider   : 3 styles
 * - Checkbox : 3 styles
 * - Progress : 4 styles
 * - Panel    : 4 styles
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_STYLES_H
#define UI_STYLES_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "ui_core.h"
#include "ui_theme.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define UI_STYLES_VERSION               "1.0.0"

// ============================================================
// SECTION 2 : STYLES DE BOUTON
// ============================================================

/**
 * @brief Styles de boutons disponibles
 */
typedef enum {
    BUTTON_STYLE_PRIMARY    = 0,    // Principal (fond primary)
    BUTTON_STYLE_SECONDARY  = 1,    // Secondaire (fond secondary)
    BUTTON_STYLE_OUTLINE    = 2,    // Contourné
    BUTTON_STYLE_TEXT       = 3,    // Texte seul
    BUTTON_STYLE_CALL       = 4,    // Appel (vert)
    BUTTON_STYLE_END        = 5,    // Raccrocher (rouge)
    BUTTON_STYLE_DANGER     = 6,    // Danger
    BUTTON_STYLE_SUCCESS    = 7,    // Succès
    BUTTON_STYLE_WARNING    = 8,    // Avertissement
    BUTTON_STYLE_INFO       = 9     // Information
} ButtonStyle;

/**
 * @brief Apparence complète d'un bouton
 */
typedef struct {
    ButtonStyle style;                  // Style
    uint16_t bgColor;                   // Couleur de fond
    uint16_t textColor;                 // Couleur du texte
    uint16_t borderColor;               // Couleur de bordure
    uint8_t borderWidth;                // Épaisseur bordure
    uint8_t cornerRadius;               // Rayon des coins
    UIMargin padding;                   // Padding interne
    const DisplayFont* font;            // Police
    uint8_t fontSize;                   // Taille du texte
} ButtonAppearance;

/**
 * @brief Obtient l'apparence d'un bouton selon son style
 */
ButtonAppearance ui_style_get_button(ButtonStyle style);

// ============================================================
// SECTION 3 : STYLES DE LABEL
// ============================================================

/**
 * @brief Styles de labels disponibles
 */
typedef enum {
    LABEL_STYLE_TITLE       = 0,    // Titre (grand, gras)
    LABEL_STYLE_SUBTITLE    = 1,    // Sous-titre
    LABEL_STYLE_HEADING     = 2,    // En-tête
    LABEL_STYLE_BODY        = 3,    // Corps de texte
    LABEL_STYLE_CAPTION     = 4,    // Légende (petit)
    LABEL_STYLE_HINT        = 5,    // Indice (gris, petit)
    LABEL_STYLE_ERROR       = 6,    // Erreur (rouge)
    LABEL_STYLE_SUCCESS     = 7     // Succès (vert)
} LabelStyle;

/**
 * @brief Apparence complète d'un label
 */
typedef struct {
    LabelStyle style;                   // Style
    uint16_t textColor;                 // Couleur du texte
    uint16_t bgColor;                   // Couleur de fond (transparent par défaut)
    const DisplayFont* font;            // Police
    uint8_t fontSize;                   // Taille du texte
    UIAlign textAlign;                  // Alignement horizontal
    bool bold;                          // Gras ?
    bool italic;                        // Italique ?
} LabelAppearance;

LabelAppearance ui_style_get_label(LabelStyle style);

// ============================================================
// SECTION 4 : STYLES DE ZONE DE TEXTE
// ============================================================

typedef enum {
    TEXTBOX_STYLE_NORMAL    = 0,    // Normal
    TEXTBOX_STYLE_OUTLINED  = 1,    // Contourné
    TEXTBOX_STYLE_FILLED    = 2,    // Fond plein
    TEXTBOX_STYLE_UNDERLINED = 3   // Souligné
} TextBoxStyle;

typedef struct {
    TextBoxStyle style;
    uint16_t bgColor;
    uint16_t textColor;
    uint16_t borderColor;
    uint16_t cursorColor;
    uint8_t borderWidth;
    uint8_t cornerRadius;
    UIMargin padding;
    const DisplayFont* font;
    uint8_t fontSize;
} TextBoxAppearance;

TextBoxAppearance ui_style_get_textbox(TextBoxStyle style);

// ============================================================
// SECTION 5 : STYLES DE LISTE
// ============================================================

typedef enum {
    LIST_STYLE_PLAIN        = 0,    // Simple
    LIST_STYLE_GROUPED      = 1,    // Groupé (iOS style)
    LIST_STYLE_CARD         = 2,    // Cartes
    LIST_STYLE_COMPACT      = 3     // Compact
} ListStyle;

typedef struct {
    ListStyle style;
    uint16_t bgColor;
    uint16_t itemBgColor;
    uint16_t itemSelectedColor;
    uint16_t textColor;
    uint16_t dividerColor;
    uint8_t itemHeight;
    uint8_t cornerRadius;
    UIMargin itemPadding;
    const DisplayFont* font;
    uint8_t fontSize;
} ListAppearance;

ListAppearance ui_style_get_list(ListStyle style);

// ============================================================
// SECTION 6 : STYLES DE SLIDER
// ============================================================

typedef enum {
    SLIDER_STYLE_NORMAL     = 0,
    SLIDER_STYLE_VOLUME     = 1,
    SLIDER_STYLE_BRIGHTNESS = 2
} SliderStyle;

typedef struct {
    SliderStyle style;
    uint16_t trackColor;
    uint16_t fillColor;
    uint16_t thumbColor;
    uint8_t trackHeight;
    uint8_t thumbRadius;
} SliderAppearance;

SliderAppearance ui_style_get_slider(SliderStyle style);

// ============================================================
// SECTION 7 : STYLES DE PROGRESS BAR
// ============================================================

typedef enum {
    PROGRESS_STYLE_NORMAL   = 0,
    PROGRESS_STYLE_BATTERY  = 1,
    PROGRESS_STYLE_SIGNAL   = 2,
    PROGRESS_STYLE_VOLUME   = 3
} ProgressStyle;

typedef struct {
    ProgressStyle style;
    uint16_t bgColor;
    uint16_t fillColor;
    uint16_t borderColor;
    uint8_t height;
    uint8_t cornerRadius;
    bool showPercentage;
} ProgressAppearance;

ProgressAppearance ui_style_get_progress(ProgressStyle style);

// ============================================================
// SECTION 8 : STYLES DE PANNEAU (PANEL)
// ============================================================

typedef enum {
    PANEL_STYLE_FLAT        = 0,    // Plat
    PANEL_STYLE_ELEVATED    = 1,    // Surélevé (ombre)
    PANEL_STYLE_BORDERED    = 2,    // Avec bordure
    PANEL_STYLE_DIALOG      = 3     // Dialogue
} PanelStyle;

typedef struct {
    PanelStyle style;
    uint16_t bgColor;
    uint16_t borderColor;
    uint16_t shadowColor;
    uint8_t borderWidth;
    uint8_t cornerRadius;
    uint8_t elevation;
    UIMargin padding;
} PanelAppearance;

PanelAppearance ui_style_get_panel(PanelStyle style);

// ============================================================
// SECTION 9 : STYLES DE BARRE DE STATUT
// ============================================================

typedef struct {
    uint16_t bgColor;
    uint16_t textColor;
    uint16_t iconColor;
    uint8_t height;
    const DisplayFont* font;
    uint8_t fontSize;
} StatusBarAppearance;

StatusBarAppearance ui_style_get_statusbar(void);

// ============================================================
// SECTION 10 : STYLES DE BARRE DE NAVIGATION
// ============================================================

typedef struct {
    uint16_t bgColor;
    uint16_t textColor;
    uint16_t buttonColor;
    uint8_t height;
    const DisplayFont* titleFont;
    uint8_t titleSize;
} NavBarAppearance;

NavBarAppearance ui_style_get_navbar(void);

// ============================================================
// SECTION 11 : FONCTIONS DE STYLE GLOBAL
// ============================================================

void ui_style_init(void);
void ui_style_refresh(void);  // Recalculer tous les styles (après changement de thème)

// ============================================================
// SECTION 12 : MACROS DE STYLES RAPIDES
// ============================================================

#define STYLE_BUTTON_PRIMARY()      ui_style_get_button(BUTTON_STYLE_PRIMARY)
#define STYLE_BUTTON_CALL()         ui_style_get_button(BUTTON_STYLE_CALL)
#define STYLE_BUTTON_END()          ui_style_get_button(BUTTON_STYLE_END)
#define STYLE_LABEL_TITLE()         ui_style_get_label(LABEL_STYLE_TITLE)
#define STYLE_LABEL_BODY()          ui_style_get_label(LABEL_STYLE_BODY)
#define STYLE_PANEL_ELEVATED()      ui_style_get_panel(PANEL_STYLE_ELEVATED)

// ============================================================
// SECTION 13 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define STYLE_DEBUG(fmt, ...)       printf("[STYLE] " fmt, ##__VA_ARGS__)
#else
    #define STYLE_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 14 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_STYLES_H