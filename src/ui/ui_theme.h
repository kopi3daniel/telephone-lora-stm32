/**
 * @file ui_theme.h
 * @brief Gestion des thèmes de l'interface utilisateur
 * 
 * Ce fichier définit le système de thèmes pour l'interface :
 * - Thèmes prédéfinis (clair, sombre, nature, océan, etc.)
 * - Palettes de couleurs complètes
 * - Styles de widgets (normaux, plats, arrondis)
 * - Polices de caractères
 * - Tailles et espacements
 * - Transitions et animations
 * 
 * Structure d'un thème :
 * ┌─────────────────────────────────────────────────────────┐
 * │ UITheme                                                 │
 * │  ├── Couleurs (16 couleurs)                            │
 * │  ├── Polices (titres, texte, petits)                   │
 * │  ├── Dimensions (coins arrondis, marges, padding)      │
 * │  └── Styles (boutons, labels, etc.)                    │
 * └─────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_THEME_H
#define UI_THEME_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "ui_core.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define UI_THEME_VERSION                "1.0.0"

/** @brief Nombre maximum de thèmes personnalisés */
#define UI_THEME_MAX_CUSTOM             10

/** @brief Rayon des coins par défaut */
#define UI_THEME_DEFAULT_CORNER_RADIUS   8

// ============================================================
// SECTION 2 : PALETTE DE COULEURS ÉTENDUE
// ============================================================

/**
 * @brief Palette de couleurs complète pour un thème
 */
typedef struct {
    // --- Couleurs de base ---
    uint16_t background;            // Fond d'écran principal
    uint16_t surface;               // Surface des cartes/panneaux
    uint16_t surfaceVariant;        // Variante de surface
    
    // --- Couleurs principales ---
    uint16_t primary;               // Couleur principale (actions)
    uint16_t primaryDark;           // Principale foncée (status bar)
    uint16_t primaryLight;          // Principale claire (hover)
    uint16_t onPrimary;             // Texte/icônes sur primary
    
    // --- Couleurs secondaires ---
    uint16_t secondary;             // Couleur secondaire
    uint16_t secondaryDark;         // Secondaire foncée
    uint16_t secondaryLight;        // Secondaire claire
    uint16_t onSecondary;           // Texte sur secondaire
    
    // --- Couleurs de texte ---
    uint16_t textPrimary;           // Texte principal
    uint16_t textSecondary;         // Texte secondaire
    uint16_t textDisabled;          // Texte désactivé
    uint16_t textHint;              // Texte d'indice
    
    // --- Couleurs fonctionnelles ---
    uint16_t error;                 // Erreur
    uint16_t onError;               // Texte sur erreur
    uint16_t success;               // Succès
    uint16_t onSuccess;             // Texte sur succès
    uint16_t warning;               // Avertissement
    uint16_t onWarning;             // Texte sur avertissement
    uint16_t info;                  // Information
    uint16_t onInfo;                // Texte sur info
    
    // --- Éléments UI ---
    uint16_t border;                // Bordures
    uint16_t divider;               // Séparateurs
    uint16_t shadow;                // Ombres
    uint16_t overlay;               // Superposition (modales)
    uint16_t disabled;              // Éléments désactivés
    uint16_t highlight;             // Surbrillance
    
    // --- Composants spécifiques ---
    uint16_t statusBarBg;           // Fond barre de statut
    uint16_t navBarBg;              // Fond barre de navigation
    uint16_t buttonDefault;         // Bouton par défaut
    uint16_t buttonCall;            // Bouton d'appel (vert)
    uint16_t buttonEnd;             // Bouton raccrocher (rouge)
    uint16_t inputBg;               // Fond champ de saisie
    
} UIThemeColors;

// ============================================================
// SECTION 3 : POLICES
// ============================================================

/**
 * @brief Configuration des polices du thème
 */
typedef struct {
    const DisplayFont* titleFont;       // Police pour les titres
    const DisplayFont* bodyFont;        // Police pour le corps
    const DisplayFont* smallFont;       // Police pour les petits textes
    const DisplayFont* monoFont;        // Police monospace
    
    uint8_t titleSize;                  // Taille des titres (multiplicateur)
    uint8_t bodySize;                   // Taille du corps
    uint8_t smallSize;                  // Taille des petits textes
} UIThemeFonts;

// ============================================================
// SECTION 4 : DIMENSIONS ET STYLES
// ============================================================

/**
 * @brief Dimensions du thème
 */
typedef struct {
    uint8_t cornerRadius;               // Rayon des coins (boutons, cartes)
    uint8_t cornerRadiusLarge;          // Rayon des coins (grands éléments)
    uint8_t cornerRadiusSmall;          // Rayon des coins (petits éléments)
    
    UIMargin screenMargin;              // Marge de l'écran
    UIMargin cardMargin;                // Marge des cartes
    UIMargin buttonPadding;             // Padding des boutons
    UIMargin inputPadding;              // Padding des champs de saisie
    
    uint8_t borderWidth;                // Épaisseur des bordures
    uint8_t dividerHeight;              // Hauteur des séparateurs
    uint8_t statusBarHeight;            // Hauteur barre de statut
    uint8_t navBarHeight;               // Hauteur barre de navigation
    
    uint16_t buttonMinWidth;            // Largeur minimale des boutons
    uint16_t buttonMinHeight;           // Hauteur minimale des boutons
} UIThemeDimensions;

/**
 * @brief Styles de boutons
 */
typedef enum {
    BUTTON_STYLE_DEFAULT    = 0,    // Style par défaut
    BUTTON_STYLE_PRIMARY    = 1,    // Bouton principal
    BUTTON_STYLE_SECONDARY  = 2,    // Bouton secondaire
    BUTTON_STYLE_OUTLINE    = 3,    // Bouton avec contour
    BUTTON_STYLE_TEXT       = 4,    // Bouton texte seul
    BUTTON_STYLE_CALL       = 5,    // Bouton d'appel
    BUTTON_STYLE_END        = 6,    // Bouton raccrocher
    BUTTON_STYLE_DANGER     = 7     // Bouton danger
} UIButtonStyle;

// ============================================================
// SECTION 5 : THÈME COMPLET
// ============================================================

/**
 * @brief Thème complet
 */
typedef struct {
    char name[32];                      // Nom du thème
    char author[32];                    // Auteur
    
    UIThemeColors colors;               // Palette de couleurs
    UIThemeFonts fonts;                 // Polices
    UIThemeDimensions dimensions;       // Dimensions
    
    bool darkMode;                      // Mode sombre ?
    bool predefined;                    // Prédéfini (non modifiable)
    
} UIThemeFull;

// ============================================================
// SECTION 6 : THÈMES PRÉDÉFINIS
// ============================================================

/**
 * @brief Thèmes disponibles
 */
typedef enum {
    THEME_LIGHT         = 0,        // Thème clair
    THEME_DARK          = 1,        // Thème sombre
    THEME_NATURE        = 2,        // Thème nature (vert)
    THEME_OCEAN         = 3,        // Thème océan (bleu)
    THEME_SUNSET        = 4,        // Thème coucher de soleil (orange)
    THEME_FOREST        = 5,        // Thème forêt (vert foncé)
    THEME_PURPLE        = 6,        // Thème violet
    THEME_MONOCHROME    = 7         // Thème noir et blanc
} UIThemePreset;

/** @brief Thèmes prédéfinis */
extern const UIThemeFull UI_THEME_FULL_LIGHT;
extern const UIThemeFull UI_THEME_FULL_DARK;
extern const UIThemeFull UI_THEME_FULL_NATURE;
extern const UIThemeFull UI_THEME_FULL_OCEAN;
extern const UIThemeFull UI_THEME_FULL_SUNSET;
extern const UIThemeFull UI_THEME_FULL_FOREST;
extern const UIThemeFull UI_THEME_FULL_PURPLE;
extern const UIThemeFull UI_THEME_FULL_MONOCHROME;

// ============================================================
// SECTION 7 : ÉTAT DU MODULE
// ============================================================

/**
 * @brief État du module de thèmes
 */
typedef struct {
    bool initialized;                   // Module initialisé
    UIThemeFull activeTheme;            // Thème actif
    UIThemePreset activePreset;         // Thème prédéfini actif
    UIThemeFull customThemes[UI_THEME_MAX_CUSTOM];  // Thèmes personnalisés
    uint8_t customThemeCount;           // Nombre de thèmes personnalisés
} UIThemeState;

// ============================================================
// SECTION 8 : FONCTIONS D'INITIALISATION
// ============================================================

bool ui_theme_init(void);
void ui_theme_deinit(void);
bool ui_theme_is_ready(void);

// ============================================================
// SECTION 9 : FONCTIONS DE THÈME
// ============================================================

bool ui_theme_apply_preset(UIThemePreset preset);
bool ui_theme_apply_custom(uint8_t index);
bool ui_theme_apply_full(const UIThemeFull* theme);
const UIThemeFull* ui_theme_get_active(void);
UIThemePreset ui_theme_get_preset(void);

// ============================================================
// SECTION 10 : FONCTIONS DE COULEURS
// ============================================================

uint16_t ui_theme_get_color(const char* colorName);
void ui_theme_set_color(const char* colorName, uint16_t color);
uint16_t ui_theme_get_primary(void);
uint16_t ui_theme_get_surface(void);
uint16_t ui_theme_get_text_primary(void);
uint16_t ui_theme_get_text_secondary(void);
uint16_t ui_theme_get_error(void);
uint16_t ui_theme_get_success(void);

// ============================================================
// SECTION 11 : FONCTIONS DE STYLES
// ============================================================

uint16_t ui_theme_get_button_color(UIButtonStyle style);
uint16_t ui_theme_get_button_text_color(UIButtonStyle style);
const DisplayFont* ui_theme_get_title_font(void);
const DisplayFont* ui_theme_get_body_font(void);
uint8_t ui_theme_get_corner_radius(void);
UIMargin ui_theme_get_screen_margin(void);

// ============================================================
// SECTION 12 : FONCTIONS DE MODE SOMBRE
// ============================================================

void ui_theme_toggle_dark_mode(void);
bool ui_theme_is_dark_mode(void);
void ui_theme_set_dark_mode(bool darkMode);

// ============================================================
// SECTION 13 : FONCTIONS DE THÈMES PERSONNALISÉS
// ============================================================

bool ui_theme_save_custom(const char* name);
bool ui_theme_delete_custom(uint8_t index);
uint8_t ui_theme_get_custom_count(void);
const UIThemeFull* ui_theme_get_custom(uint8_t index);

// ============================================================
// SECTION 14 : FONCTIONS DE TRANSITION
// ============================================================

void ui_theme_transition(const UIThemeFull* from, const UIThemeFull* to, uint32_t durationMs);
bool ui_theme_is_transitioning(void);

// ============================================================
// SECTION 15 : FONCTIONS DE DÉBOGAGE
// ============================================================

void ui_theme_print_colors(void);
void ui_theme_print_dimensions(void);
void ui_theme_print_all(void);
bool ui_theme_self_test(void);

// ============================================================
// SECTION 16 : MACROS UTILITAIRES
// ============================================================

#define UI_COLOR_PRIMARY()              ui_theme_get_primary()
#define UI_COLOR_SURFACE()              ui_theme_get_surface()
#define UI_COLOR_TEXT()                 ui_theme_get_text_primary()
#define UI_COLOR_ERROR()                ui_theme_get_error()

// ============================================================
// SECTION 17 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define THEME_DEBUG(fmt, ...)       printf("[THEME] " fmt, ##__VA_ARGS__)
#else
    #define THEME_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 18 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_THEME_H