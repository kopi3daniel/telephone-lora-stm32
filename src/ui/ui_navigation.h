/**
 * @file ui_navigation.h
 * @brief Widget Barre de Navigation - Définition et fonctions
 * 
 * Ce fichier définit le widget de barre de navigation qui gère :
 * - Le titre de l'écran actuel
 * - Le bouton de retour
 * - Les onglets (tabs) en bas de l'écran
 * - La pile d'écrans (historique de navigation)
 * - Les gestes de navigation (swipe back)
 * 
 * Types de barres :
 * - TOP_BAR    : Barre supérieure (titre + retour)
 * - TAB_BAR    : Barre d'onglets inférieure
 * - TOOLBAR    : Barre d'outils
 * 
 * Disposition TOP_BAR :
 * ┌─────────────────────────────────────────────────────────────┐
 * │ ←  Titre de l'écran                          ⋮  Menu      │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * Disposition TAB_BAR :
 * ┌─────────────────────────────────────────────────────────────┐
 * │   📞       👤       💬       ⚙                            │
 * │  Appels  Contacts  Messages  Paramètres                    │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_NAVIGATION_H
#define UI_NAVIGATION_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "ui_core.h"
#include "ui_theme.h"
#include "ui_icons.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du widget */
#define UI_NAVIGATION_VERSION           "1.0.0"

/** @brief Hauteur de la barre supérieure */
#define NAVBAR_TOP_HEIGHT               44

/** @brief Hauteur de la barre d'onglets */
#define NAVBAR_TAB_HEIGHT               56

/** @brief Nombre maximum d'onglets */
#define NAVBAR_MAX_TABS                 5

/** @brief Nombre maximum d'actions dans la toolbar */
#define NAVBAR_MAX_ACTIONS              4

// ============================================================
// SECTION 2 : TYPES DE BARRES
// ============================================================

/**
 * @brief Types de barres de navigation
 */
typedef enum {
    NAV_TYPE_TOP_BAR    = 0,    // Barre supérieure (titre + retour)
    NAV_TYPE_TAB_BAR    = 1,    // Barre d'onglets inférieure
    NAV_TYPE_TOOLBAR    = 2     // Barre d'outils
} NavBarType;

/**
 * @brief Style de la barre de navigation
 */
typedef enum {
    NAV_STYLE_DEFAULT   = 0,    // Style par défaut
    NAV_STYLE_TRANSPARENT = 1,  // Fond transparent
    NAV_STYLE_ELEVATED  = 2,    // Avec ombre
    NAV_STYLE_COMPACT   = 3     // Compact
} NavBarStyle;

// ============================================================
// SECTION 3 : ONGLET (TAB)
// ============================================================

/**
 * @brief Onglet de navigation
 */
typedef struct {
    char title[16];                 // Titre de l'onglet
    IconID icon;                    // Icône
    IconID selectedIcon;            // Icône sélectionnée
    UIScreen* screen;               // Écran associé
    bool selected;                  // Sélectionné ?
    uint8_t badgeCount;             // Pastille de notification
} NavTab;

/**
 * @brief Action de la barre d'outils
 */
typedef struct {
    IconID icon;                    // Icône
    char label[16];                 // Label
    void (*onAction)(void);         // Callback
    bool enabled;                   // Activée ?
} NavAction;

// ============================================================
// SECTION 4 : STRUCTURE DE LA BARRE DE NAVIGATION
// ============================================================

/**
 * @brief Widget Barre de Navigation
 */
typedef struct UINavigationBar {
    UIWidget base;                      // Widget de base (héritage)
    
    // --- Type ---
    NavBarType type;                    // Type de barre
    NavBarStyle style;                  // Style
    
    // --- Barre supérieure ---
    char title[64];                     // Titre affiché
    bool showBackButton;                // Afficher le bouton retour
    bool showMenuButton;                // Afficher le bouton menu
    
    // --- Barre d'onglets ---
    NavTab tabs[NAVBAR_MAX_TABS];       // Onglets
    uint8_t tabCount;                   // Nombre d'onglets
    uint8_t selectedTab;                // Onglet sélectionné
    
    // --- Barre d'outils ---
    NavAction actions[NAVBAR_MAX_ACTIONS];  // Actions
    uint8_t actionCount;                    // Nombre d'actions
    
    // --- Apparence ---
    uint16_t backgroundColor;           // Couleur de fond
    uint16_t titleColor;               // Couleur du titre
    uint16_t iconColor;                // Couleur des icônes
    uint16_t selectedColor;            // Couleur sélection
    uint16_t separatorColor;           // Couleur séparateur
    
    // --- Callbacks ---
    void (*onBack)(struct UINavigationBar* navbar);         // Retour
    void (*onMenu)(struct UINavigationBar* navbar);         // Menu
    void (*onTabSelected)(struct UINavigationBar* navbar, uint8_t index);  // Onglet
    void (*onTitleTap)(struct UINavigationBar* navbar);     // Tap sur le titre
    
} UINavigationBar;

// ============================================================
// SECTION 5 : FONCTIONS DE CRÉATION
// ============================================================

UINavigationBar* ui_navbar_create_top(const char* name, const char* title);
UINavigationBar* ui_navbar_create_tabs(const char* name);
UINavigationBar* ui_navbar_create_toolbar(const char* name);

// ============================================================
// SECTION 6 : FONCTIONS DE CONFIGURATION (TOP BAR)
// ============================================================

void ui_navbar_set_title(UINavigationBar* navbar, const char* title);
void ui_navbar_show_back(UINavigationBar* navbar, bool show);
void ui_navbar_show_menu(UINavigationBar* navbar, bool show);
void ui_navbar_set_colors(UINavigationBar* navbar, uint16_t bg, uint16_t title, uint16_t icons);
void ui_navbar_set_style(UINavigationBar* navbar, NavBarStyle style);

// ============================================================
// SECTION 7 : FONCTIONS DE CONFIGURATION (TAB BAR)
// ============================================================

bool ui_navbar_add_tab(UINavigationBar* navbar, const char* title, IconID icon, UIScreen* screen);
bool ui_navbar_remove_tab(UINavigationBar* navbar, uint8_t index);
void ui_navbar_select_tab(UINavigationBar* navbar, uint8_t index);
uint8_t ui_navbar_get_selected_tab(UINavigationBar* navbar);
void ui_navbar_set_tab_badge(UINavigationBar* navbar, uint8_t index, uint8_t count);

// ============================================================
// SECTION 8 : FONCTIONS DE CONFIGURATION (TOOLBAR)
// ============================================================

bool ui_navbar_add_action(UINavigationBar* navbar, IconID icon, const char* label, void (*action)(void));
void ui_navbar_enable_action(UINavigationBar* navbar, uint8_t index, bool enable);

// ============================================================
// SECTION 9 : MACROS RAPIDES
// ============================================================

#define UI_NAVBAR_CREATE_TOP(title) \
    ui_navbar_create_top("navbar", title)

#define UI_NAVBAR_CREATE_TABS() \
    ui_navbar_create_tabs("tabbar")

#define UI_NAVBAR_SET_TITLE(nav, title) \
    ui_navbar_set_title(nav, title)

// ============================================================
// SECTION 10 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_NAVIGATION_H