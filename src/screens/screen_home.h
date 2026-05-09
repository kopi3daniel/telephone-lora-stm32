/**
 * @file screen_home.h
 * @brief Écran d'accueil du téléphone LoRa
 * 
 * Cet écran est la page principale affichée au démarrage.
 * Il présente :
 * - L'heure et la date
 * - Le niveau de batterie et le signal LoRa
 * - Les boutons principaux (Appeler, Contacts, Messages)
 * - Les raccourcis vers les applications
 * - Les notifications en attente
 * - La barre de statut en haut
 * - La barre de navigation en bas (onglets)
 * 
 * Disposition :
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 12:45                              ▂▄▆█ 🔋85% 🔔           │ ← StatusBar
 * ├─────────────────────────────────────────────────────────────┤
 * │                                                             │
 * │                        📱 LoRa Phone                        │
 * │                                                             │
 * │              ┌──────────────────────────────┐               │
 * │              │         📞  Appeler           │               │
 * │              └──────────────────────────────┘               │
 * │              ┌──────────────────────────────┐               │
 * │              │         👤  Contacts          │               │
 * │              └──────────────────────────────┘               │
 * │              ┌──────────────────────────────┐               │
 * │              │         💬  Messages          │               │
 * │              └──────────────────────────────┘               │
 * │                                                             │
 * ├─────────────────────────────────────────────────────────────┤
 * │   📞       👤       💬       ⚙                            │ ← TabBar
 * │  Appels  Contacts  Messages  Paramètres                    │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SCREEN_HOME_H
#define SCREEN_HOME_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "screen_base.h"
#include "../ui/ui_widgets.h"
#include "../ui/ui_statusbar.h"
#include "../ui/ui_navigation.h"
#include "../services/phone_service.h"
#include "../services/sms_service.h"
#include "../drivers/power/battery_monitor.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Nom de l'écran */
#define SCREEN_HOME_NAME                "HomeScreen"

/** @brief Nombre maximum de notifications sur l'écran d'accueil */
#define HOME_MAX_VISIBLE_NOTIFICATIONS  3

// ============================================================
// SECTION 2 : ÉTAT DE L'ÉCRAN D'ACCUEIL
// ============================================================

/**
 * @brief État spécifique à l'écran d'accueil
 */
typedef struct {
    // --- Widgets ---
    UIStatusBar* statusBar;             // Barre de statut
    UILabel* titleLabel;                // Titre "LoRa Phone"
    UILabel* clockLabel;                // Horloge
    UILabel* dateLabel;                 // Date
    UIButton* btnCall;                  // Bouton Appeler
    UIButton* btnContacts;              // Bouton Contacts
    UIButton* btnMessages;              // Bouton Messages
    UIButton* btnSettings;              // Bouton Paramètres (si pas de tabbar)
    UINavigationBar* tabBar;            // Barre d'onglets
    
    // --- État ---
    uint8_t unreadSMS;                  // SMS non lus
    uint8_t missedCalls;                // Appels manqués
    bool showClock;                     // Afficher l'horloge
    bool showDate;                      // Afficher la date
    
    // --- Icônes de notification ---
    IconID notificationIcons[HOME_MAX_VISIBLE_NOTIFICATIONS];
    uint8_t notificationCount;
    
} HomeScreenState;

// ============================================================
// SECTION 3 : FONCTIONS DE CRÉATION
// ============================================================

/**
 * @brief Crée l'écran d'accueil
 * @return Pointeur vers l'écran créé
 */
ScreenBase* screen_home_create(void);

/**
 * @brief Initialise les widgets de l'écran d'accueil
 */
void screen_home_init_widgets(ScreenBase* screen);

/**
 * @brief Met à jour les informations affichées
 */
void screen_home_refresh(ScreenBase* screen);

/**
 * @brief Ajoute une notification sur l'écran d'accueil
 */
void screen_home_add_notification(ScreenBase* screen, IconID icon);

/**
 * @brief Efface toutes les notifications
 */
void screen_home_clear_notifications(ScreenBase* screen);

// ============================================================
// SECTION 4 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define HOME_DEBUG(fmt, ...)        printf("[HOME] " fmt, ##__VA_ARGS__)
#else
    #define HOME_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 5 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SCREEN_HOME_H