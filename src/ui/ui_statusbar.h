/**
 * @file ui_statusbar.h
 * @brief Widget Barre de Statut - Définition et fonctions
 * 
 * Ce fichier définit le widget de barre de statut qui affiche :
 * - L'heure actuelle
 * - Le niveau de batterie (icône + pourcentage)
 * - La force du signal LoRa (barres)
 * - Les notifications (icône cloche)
 * - Le mode vibreur/silencieux
 * - Le verrouillage
 * 
 * Disposition typique (320px de large) :
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 12:45              ▂▄▆█  🔋 85%  🔔  🔒                    │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_STATUSBAR_H
#define UI_STATUSBAR_H

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
#define UI_STATUSBAR_VERSION            "1.0.0"

/** @brief Hauteur par défaut de la barre de statut */
#define STATUSBAR_DEFAULT_HEIGHT        24

/** @brief Nombre maximum d'icônes de notification */
#define STATUSBAR_MAX_NOTIFICATIONS     5

// ============================================================
// SECTION 2 : STRUCTURE DE LA BARRE DE STATUT
// ============================================================

/**
 * @brief Widget Barre de Statut
 */
typedef struct UIStatusBar {
    UIWidget base;                      // Widget de base (héritage)
    
    // --- Apparence ---
    uint16_t backgroundColor;           // Couleur de fond
    uint16_t textColor;                 // Couleur du texte
    uint16_t iconColor;                 // Couleur des icônes
    uint8_t height;                     // Hauteur
    
    // --- Données affichées ---
    uint8_t hours;                      // Heure (0-23)
    uint8_t minutes;                    // Minutes (0-59)
    uint8_t batteryPercent;             // Batterie (0-100)
    bool batteryCharging;               // En charge ?
    uint8_t signalLevel;                // Signal (0-4)
    bool notificationsMuted;            // Notifications muettes ?
    bool vibrationEnabled;              // Vibreur activé ?
    bool locked;                        // Verrouillé ?
    
    // --- Icônes de notification supplémentaires ---
    IconID extraIcons[STATUSBAR_MAX_NOTIFICATIONS];
    uint8_t extraIconCount;
    
    // --- Comportement ---
    bool showSeconds;                   // Afficher les secondes
    bool showBatteryPercent;            // Afficher le pourcentage batterie
    bool showSignalLabel;               // Afficher "LoRa" à côté du signal
    
    // --- Callbacks ---
    void (*onTap)(struct UIStatusBar* statusbar);  // Appui sur la barre
    
} UIStatusBar;

// ============================================================
// SECTION 3 : FONCTIONS DE CRÉATION
// ============================================================

UIStatusBar* ui_statusbar_create(const char* name);

// ============================================================
// SECTION 4 : FONCTIONS DE MISE À JOUR
// ============================================================

void ui_statusbar_set_time(UIStatusBar* statusbar, uint8_t hours, uint8_t minutes);
void ui_statusbar_set_battery(UIStatusBar* statusbar, uint8_t percent, bool charging);
void ui_statusbar_set_signal(UIStatusBar* statusbar, uint8_t level);
void ui_statusbar_set_notifications(UIStatusBar* statusbar, bool muted);
void ui_statusbar_set_vibration(UIStatusBar* statusbar, bool enabled);
void ui_statusbar_set_locked(UIStatusBar* statusbar, bool locked);

// ============================================================
// SECTION 5 : FONCTIONS D'ICÔNES SUPPLÉMENTAIRES
// ============================================================

bool ui_statusbar_add_icon(UIStatusBar* statusbar, IconID icon);
bool ui_statusbar_remove_icon(UIStatusBar* statusbar, IconID icon);
void ui_statusbar_clear_icons(UIStatusBar* statusbar);

// ============================================================
// SECTION 6 : FONCTIONS DE STYLE
// ============================================================

void ui_statusbar_set_colors(UIStatusBar* statusbar, uint16_t bg, uint16_t text, uint16_t icons);
void ui_statusbar_set_height(UIStatusBar* statusbar, uint8_t height);
void ui_statusbar_show_battery_percent(UIStatusBar* statusbar, bool show);
void ui_statusbar_show_signal_label(UIStatusBar* statusbar, bool show);

// ============================================================
// SECTION 7 : MACROS RAPIDES
// ============================================================

#define UI_STATUSBAR_CREATE() \
    ui_statusbar_create("statusbar")

#define UI_STATUSBAR_UPDATE_TIME(sb, h, m) \
    ui_statusbar_set_time(sb, h, m)

#define UI_STATUSBAR_UPDATE_BATTERY(sb, pct, chg) \
    ui_statusbar_set_battery(sb, pct, chg)

// ============================================================
// SECTION 8 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_STATUSBAR_H