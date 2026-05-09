/**
 * @file ui_core.h
 * @brief Noyau du framework d'interface utilisateur
 * 
 * Ce fichier définit le framework UI de base pour le téléphone :
 * - Gestion des écrans (Screen)
 * - Navigation entre écrans
 * - Boucle d'événements
 * - Rendu graphique
 * - Gestion du focus
 * 
 * Architecture :
 * ┌─────────────────────────────────────────────────────────┐
 * │                    UI CORE                              │
 * │                                                         │
 * │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
 * │  │ Screen Stack │  │ Event Loop   │  │ Renderer     │ │
 * │  │ (navigation) │  │ (inputs)     │  │ (display)    │ │
 * │  └──────────────┘  └──────────────┘  └──────────────┘ │
 * └─────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_CORE_H
#define UI_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "../drivers/display/display_manager.h"
#include "../drivers/touch/touch_manager.h"
#include "../drivers/keypad/keypad_manager.h"
#include "../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du framework UI */
#define UI_CORE_VERSION                 "1.0.0"

/** @brief Nombre maximum d'écrans dans la pile */
#define UI_MAX_SCREEN_STACK             10

/** @brief Nombre maximum de fenêtres modales */
#define UI_MAX_MODALS                   5

/** @brief Nombre maximum de widgets par écran */
#define UI_MAX_WIDGETS_PER_SCREEN       50

/** @brief Fréquence de rafraîchissement (FPS) */
#define UI_DEFAULT_FPS                  30

/** @brief Intervalle de rafraîchissement (ms) */
#define UI_REFRESH_INTERVAL_MS          (1000 / UI_DEFAULT_FPS)

/** @brief Durée d'animation par défaut (ms) */
#define UI_DEFAULT_ANIMATION_MS         200

// ============================================================
// SECTION 2 : COULEURS DU THÈME
// ============================================================

/**
 * @brief Palette de couleurs du thème
 */
typedef struct {
    uint16_t background;            // Fond d'écran
    uint16_t surface;               // Surface (cartes, panneaux)
    uint16_t primary;               // Couleur principale
    uint16_t primaryDark;           // Principale foncée
    uint16_t primaryLight;          // Principale claire
    uint16_t secondary;             // Couleur secondaire
    uint16_t accent;                // Accentuation
    uint16_t textPrimary;           // Texte principal
    uint16_t textSecondary;         // Texte secondaire
    uint16_t textOnPrimary;         // Texte sur couleur principale
    uint16_t border;                // Bordures
    uint16_t error;                 // Erreur
    uint16_t success;               // Succès
    uint16_t warning;               // Avertissement
    uint16_t disabled;              // Désactivé
} UITheme;

/** @brief Thème clair par défaut */
extern const UITheme UI_THEME_LIGHT;

/** @brief Thème sombre */
extern const UITheme UI_THEME_DARK;

// ============================================================
// SECTION 3 : TYPES DE BASE
// ============================================================

/**
 * @brief Rectangle (position + taille)
 */
typedef struct {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
} UIRect;

/**
 * @brief Marges (padding/margin)
 */
typedef struct {
    uint8_t top;
    uint8_t right;
    uint8_t bottom;
    uint8_t left;
} UIMargin;

/**
 * @brief Alignement
 */
typedef enum {
    UI_ALIGN_LEFT       = 0,
    UI_ALIGN_CENTER     = 1,
    UI_ALIGN_RIGHT      = 2,
    UI_ALIGN_TOP        = 0,
    UI_ALIGN_MIDDLE     = 1,
    UI_ALIGN_BOTTOM     = 2
} UIAlign;

/**
 * @brief Résultat d'une action
 */
typedef enum {
    UI_RESULT_OK        = 0,
    UI_RESULT_CANCEL    = 1,
    UI_RESULT_BACK      = 2,
    UI_RESULT_ERROR     = 3
} UIResult;

// ============================================================
// SECTION 4 : SCREEN (ÉCRAN)
// ============================================================

/** @brief Forward declaration */
typedef struct UIScreen UIScreen;
typedef struct UIWidget UIWidget;

/**
 * @brief Structure de base d'un écran
 */
struct UIScreen {
    char name[32];                              // Nom de l'écran
    
    // Widgets
    UIWidget* widgets[UI_MAX_WIDGETS_PER_SCREEN];
    uint8_t widgetCount;
    
    // État
    bool initialized;                           // Initialisé ?
    bool visible;                               // Visible ?
    bool needsRedraw;                           // Redessiner ?
    
    // Méthodes virtuelles (callbacks)
    void (*onInit)(UIScreen* screen);           // Initialisation
    void (*onDraw)(UIScreen* screen);           // Dessin
    void (*onUpdate)(UIScreen* screen);         // Mise à jour périodique
    void (*onTouch)(UIScreen* screen, uint16_t x, uint16_t y, TouchEvent event);  // Événement tactile
    void (*onKey)(UIScreen* screen, KeyCode key, KeyEvent event);  // Événement clavier
    void (*onShow)(UIScreen* screen);           // Affichage
    void (*onHide)(UIScreen* screen);           // Masquage
    void (*onResult)(UIScreen* screen, UIResult result);  // Résultat
    void (*onDestroy)(UIScreen* screen);        // Destruction
    
    // Données utilisateur
    void* userData;
};

// ============================================================
// SECTION 5 : WIDGET (COMPOSANT)
// ============================================================

/**
 * @brief Types de widgets
 */
typedef enum {
    WIDGET_TYPE_NONE        = 0,
    WIDGET_TYPE_BUTTON      = 1,
    WIDGET_TYPE_LABEL       = 2,
    WIDGET_TYPE_TEXTBOX     = 3,
    WIDGET_TYPE_LIST        = 4,
    WIDGET_TYPE_SLIDER      = 5,
    WIDGET_TYPE_CHECKBOX    = 6,
    WIDGET_TYPE_RADIO       = 7,
    WIDGET_TYPE_PROGRESS    = 8,
    WIDGET_TYPE_IMAGE       = 9,
    WIDGET_TYPE_PANEL       = 10,
    WIDGET_TYPE_CUSTOM      = 100
} UIWidgetType;

/**
 * @brief État d'un widget
 */
typedef enum {
    WIDGET_STATE_NORMAL     = 0,
    WIDGET_STATE_HOVER      = 1,
    WIDGET_STATE_PRESSED    = 2,
    WIDGET_STATE_DISABLED   = 3,
    WIDGET_STATE_FOCUSED    = 4
} UIWidgetState;

/**
 * @brief Structure de base d'un widget
 */
struct UIWidget {
    uint32_t id;                                // Identifiant unique
    UIWidgetType type;                          // Type de widget
    char name[32];                              // Nom
    
    UIRect rect;                                // Position et taille
    UIMargin margin;                            // Marges
    UIMargin padding;                           // Padding
    bool visible;                               // Visible ?
    bool enabled;                               // Activé ?
    UIWidgetState state;                        // État
    bool needsRedraw;                           // Redessiner ?
    
    // Focus
    bool canFocus;                              // Peut recevoir le focus ?
    bool hasFocus;                              // A le focus ?
    
    // Méthodes virtuelles
    void (*draw)(UIWidget* widget);             // Dessiner
    void (*onTouch)(UIWidget* widget, uint16_t x, uint16_t y, TouchEvent event);
    void (*onKey)(UIWidget* widget, KeyCode key, KeyEvent event);
    void (*onFocus)(UIWidget* widget, bool focused);
    void (*onStateChanged)(UIWidget* widget, UIWidgetState oldState, UIWidgetState newState);
    void (*onClick)(UIWidget* widget);
    
    // Parent
    UIScreen* parent;
    
    // Données utilisateur
    void* userData;
};

// ============================================================
// SECTION 6 : MODAL (FENÊTRE MODALE)
// ============================================================

/**
 * @brief Fenêtre modale (dialogue, popup)
 */
typedef struct {
    char title[64];                             // Titre
    char message[256];                          // Message
    UIScreen* screen;                           // Écran contenu (NULL = texte simple)
    bool dismissOnBack;                         // Fermer sur retour ?
    bool dismissOnOutside;                      // Fermer si clic à l'extérieur ?
    void (*onDismiss)(UIResult result);          // Callback de fermeture
} UIModal;

// ============================================================
// SECTION 7 : ÉTAT DU FRAMEWORK UI
// ============================================================

/**
 * @brief État global du framework UI
 */
typedef struct {
    bool initialized;                           // Framework initialisé
    
    // Écrans
    UIScreen* screenStack[UI_MAX_SCREEN_STACK];
    uint8_t screenStackTop;
    UIScreen* activeScreen;
    
    // Modales
    UIModal modalStack[UI_MAX_MODALS];
    uint8_t modalCount;
    
    // Thème
    UITheme theme;
    
    // Widget avec le focus
    UIWidget* focusedWidget;
    
    // Animations
    bool animating;
    uint32_t animationStartTime;
    uint32_t animationDuration;
    
    // Compteurs
    uint32_t frameCount;
    uint32_t lastRenderTime;
    uint32_t widgetIdCounter;
    
    // Flags
    bool needsRedraw;
    bool screenTransitionInProgress;
    
} UICoreState;

// ============================================================
// SECTION 8 : FONCTIONS D'INITIALISATION
// ============================================================

bool ui_core_init(void);
void ui_core_deinit(void);
bool ui_core_is_ready(void);

// ============================================================
// SECTION 9 : FONCTIONS DE NAVIGATION
// ============================================================

bool ui_push_screen(UIScreen* screen);
UIScreen* ui_pop_screen(void);
UIScreen* ui_get_active_screen(void);
uint8_t ui_get_screen_count(void);
bool ui_replace_screen(UIScreen* screen);
void ui_clear_screen_stack(void);

// ============================================================
// SECTION 10 : FONCTIONS DE MODALES
// ============================================================

bool ui_show_modal(UIModal* modal);
void ui_dismiss_modal(UIResult result);
void ui_dismiss_all_modals(void);
bool ui_has_modal(void);

// ============================================================
// SECTION 11 : FONCTIONS DE RENDU
// ============================================================

void ui_render(void);
void ui_request_redraw(void);
void ui_process_animations(void);

// ============================================================
// SECTION 12 : FONCTIONS D'ÉVÉNEMENTS
// ============================================================

void ui_process_events(void);
void ui_handle_touch(uint16_t x, uint16_t y, TouchEvent event);
void ui_handle_key(KeyCode key, KeyEvent event);

// ============================================================
// SECTION 13 : FONCTIONS DE THÈME
// ============================================================

void ui_set_theme(const UITheme* theme);
const UITheme* ui_get_theme(void);
void ui_apply_theme(void);

// ============================================================
// SECTION 14 : FONCTIONS DE WIDGETS
// ============================================================

UIWidget* ui_widget_create(UIWidgetType type, const char* name);
bool ui_widget_add_to_screen(UIScreen* screen, UIWidget* widget);
bool ui_widget_remove_from_screen(UIScreen* screen, UIWidget* widget);
UIWidget* ui_find_widget_by_id(UIScreen* screen, uint32_t id);
UIWidget* ui_find_widget_by_name(UIScreen* screen, const char* name);
void ui_widget_set_focus(UIWidget* widget);
UIWidget* ui_get_focused_widget(void);
void ui_widget_destroy(UIWidget* widget);

// ============================================================
// SECTION 15 : FONCTIONS DE DÉBOGAGE
// ============================================================

void ui_core_print_state(void);
void ui_core_print_screen_stack(void);
void ui_core_print_widgets(UIScreen* screen);
bool ui_core_self_test(void);

// ============================================================
// SECTION 16 : MACROS UTILITAIRES
// ============================================================

#define UI_RECT(x, y, w, h)             ((UIRect){(x), (y), (w), (h)})
#define UI_MARGIN(t, r, b, l)           ((UIMargin){(t), (r), (b), (l)})
#define UI_MARGIN_ALL(v)                ((UIMargin){(v), (v), (v), (v)})
#define UI_IS_VISIBLE(screen)           ((screen) && (screen)->visible)

// ============================================================
// SECTION 17 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define UI_DEBUG(fmt, ...)          printf("[UI] " fmt, ##__VA_ARGS__)
#else
    #define UI_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 18 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_CORE_H