/**
 * @file ui_icons.h
 * @brief Icônes bitmap pour l'interface utilisateur
 * 
 * Ce fichier définit les icônes utilisées dans l'interface :
 * - Icônes de navigation (retour, menu, options)
 * - Icônes de statut (batterie, signal, WiFi, Bluetooth)
 * - Icônes d'action (appeler, raccrocher, message, contacts)
 * - Icônes de notification (alerte, info, erreur)
 * - Icônes de média (volume, muet, vibreur)
 * - Icônes diverses (lampe, cadenas, horloge, etc.)
 * 
 * Chaque icône est une bitmap RGB565 de taille fixe :
 * - 16×16 pixels (petites)
 * - 24×24 pixels (moyennes)
 * - 32×32 pixels (grandes)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_ICONS_H
#define UI_ICONS_H

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
#define UI_ICONS_VERSION                "1.0.0"

/** @brief Tailles d'icônes disponibles */
typedef enum {
    ICON_SIZE_16    = 16,       // Petite (barre de statut)
    ICON_SIZE_24    = 24,       // Moyenne (listes)
    ICON_SIZE_32    = 32,       // Grande (boutons)
    ICON_SIZE_48    = 48        // Très grande (accueil)
} IconSize;

/** @brief Nombre maximum d'icônes personnalisées */
#define UI_ICONS_MAX_CUSTOM             50

// ============================================================
// SECTION 2 : IDENTIFIANTS D'ICÔNES
// ============================================================

/**
 * @brief Identifiants des icônes intégrées
 */
typedef enum {
    // --- Navigation (0-9) ---
    ICON_BACK                = 0,
    ICON_MENU                = 1,
    ICON_HOME                = 2,
    ICON_SETTINGS            = 3,
    ICON_SEARCH              = 4,
    ICON_CLOSE               = 5,
    ICON_ARROW_UP            = 6,
    ICON_ARROW_DOWN          = 7,
    ICON_ARROW_LEFT          = 8,
    ICON_ARROW_RIGHT         = 9,
    
    // --- Communication (10-19) ---
    ICON_CALL                = 10,
    ICON_CALL_END            = 11,
    ICON_CALL_INCOMING       = 12,
    ICON_CALL_OUTGOING       = 13,
    ICON_CALL_MISSED         = 14,
    ICON_MESSAGE             = 15,
    ICON_CONTACTS            = 16,
    ICON_DIALER              = 17,
    ICON_SPEAKER             = 18,
    ICON_MUTE                = 19,
    
    // --- Statut (20-29) ---
    ICON_BATTERY_FULL        = 20,
    ICON_BATTERY_HALF        = 21,
    ICON_BATTERY_LOW         = 22,
    ICON_BATTERY_CHARGING    = 23,
    ICON_SIGNAL_FULL         = 24,
    ICON_SIGNAL_LOW          = 25,
    ICON_WIFI                = 26,
    ICON_BLUETOOTH           = 27,
    ICON_LOCK                = 28,
    ICON_UNLOCK              = 29,
    
    // --- Notifications (30-39) ---
    ICON_ALERT               = 30,
    ICON_INFO                = 31,
    ICON_ERROR               = 32,
    ICON_SUCCESS             = 33,
    ICON_WARNING             = 34,
    ICON_QUESTION            = 35,
    ICON_BELL                = 36,
    ICON_BELL_MUTE           = 37,
    ICON_STAR                = 38,
    ICON_HEART               = 39,
    
    // --- Média (40-49) ---
    ICON_VOLUME              = 40,
    ICON_VOLUME_MUTE         = 41,
    ICON_VOLUME_UP           = 42,
    ICON_VOLUME_DOWN         = 43,
    ICON_VIBRATE             = 44,
    ICON_MUSIC               = 45,
    ICON_CAMERA              = 46,
    ICON_GALLERY             = 47,
    ICON_VIDEO               = 48,
    ICON_RECORD              = 49,
    
    // --- Utilitaires (50-59) ---
    ICON_CLOCK               = 50,
    ICON_CALENDAR            = 51,
    ICON_LAMP                = 52,
    ICON_FLASHLIGHT          = 53,
    ICON_POWER               = 54,
    ICON_POWER_SAVE          = 55,
    ICON_SHIELD              = 56,
    ICON_KEY                 = 57,
    ICON_TOOLS               = 58,
    ICON_CHIP                = 59,
    
    // --- Utilisateur (60-69) ---
    ICON_USER                = 60,
    ICON_USER_ADD            = 61,
    ICON_USER_DELETE         = 62,
    ICON_USER_GROUP          = 63,
    ICON_FAVORITE            = 64,
    ICON_FAVORITE_EMPTY      = 65,
    ICON_PIN                 = 66,
    ICON_FLAG                = 67,
    
    // --- Divers (70-79) ---
    ICON_SUN                 = 70,
    ICON_MOON                = 71,
    ICON_CLOUD               = 72,
    ICON_UMBRELLA            = 73,
    ICON_THERMOMETER         = 74,
    ICON_COMPASS             = 75,
    ICON_MAP                 = 76,
    ICON_TARGET              = 77,
    
    ICON_COUNT               = 78
} IconID;

// ============================================================
// SECTION 3 : STRUCTURE D'UNE ICÔNE
// ============================================================

/**
 * @brief Icône bitmap
 */
typedef struct {
    const uint16_t* data;           // Données bitmap RGB565
    uint8_t width;                  // Largeur (pixels)
    uint8_t height;                 // Hauteur (pixels)
    const char* name;               // Nom de l'icône
    IconID id;                      // Identifiant
    bool predefined;                // Prédéfinie (en Flash)
} UIIcon;

/**
 * @brief Icône personnalisée (en RAM)
 */
typedef struct {
    UIIcon icon;                    // Icône de base
    uint16_t* data;                 // Données allouées dynamiquement
    bool loaded;                    // Chargée ?
} UICustomIcon;

// ============================================================
// SECTION 4 : ÉTAT DU MODULE
// ============================================================

typedef struct {
    bool initialized;                       // Module initialisé
    UICustomIcon customIcons[UI_ICONS_MAX_CUSTOM];  // Icônes personnalisées
    uint8_t customIconCount;                // Nombre d'icônes personnalisées
} UIIconsState;

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

bool ui_icons_init(void);
void ui_icons_deinit(void);
bool ui_icons_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS D'ICÔNES
// ============================================================

/**
 * @brief Récupère une icône par son ID
 */
const UIIcon* ui_icons_get(IconID id);

/**
 * @brief Récupère une icône par son nom
 */
const UIIcon* ui_icons_get_by_name(const char* name);

/**
 * @brief Dessine une icône à une position donnée
 */
void ui_icons_draw(IconID id, uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Dessine une icône avec une couleur de remplacement
 * (remplace la couleur principale de l'icône)
 */
void ui_icons_draw_colored(IconID id, uint16_t x, uint16_t y, 
                             uint16_t primaryColor, uint16_t bgColor);

/**
 * @brief Dessine une icône centrée dans un rectangle
 */
void ui_icons_draw_centered(IconID id, UIRect* rect, uint16_t color);

// ============================================================
// SECTION 7 : FONCTIONS D'ICÔNES DE STATUT
// ============================================================

void ui_icons_draw_battery(uint16_t x, uint16_t y, uint8_t percent, bool charging);
void ui_icons_draw_signal(uint16_t x, uint16_t y, uint8_t level);
void ui_icons_draw_lock(uint16_t x, uint16_t y, bool locked);
void ui_icons_draw_bell(uint16_t x, uint16_t y, bool muted);

// ============================================================
// SECTION 8 : FONCTIONS D'ICÔNES PERSONNALISÉES
// ============================================================

bool ui_icons_load_custom(const char* name, const uint16_t* data, uint8_t width, uint8_t height);
bool ui_icons_unload_custom(uint8_t index);
const UIIcon* ui_icons_get_custom(uint8_t index);
uint8_t ui_icons_get_custom_count(void);

// ============================================================
// SECTION 9 : FONCTIONS DE DÉBOGAGE
// ============================================================

void ui_icons_print_list(void);
void ui_icons_print_info(IconID id);
bool ui_icons_self_test(void);

// ============================================================
// SECTION 10 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define ICONS_DEBUG(fmt, ...)       printf("[ICONS] " fmt, ##__VA_ARGS__)
#else
    #define ICONS_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 11 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_ICONS_H