/**
 * @file screen_home.cpp
 * @brief Implémentation de l'écran d'accueil
 * 
 * Fonctionnalités :
 * - Affichage de l'heure et de la date
 * - Barre de statut (batterie, signal, notifications)
 * - Boutons principaux (Appeler, Contacts, Messages, Paramètres)
 * - Barre d'onglets de navigation
 * - Notifications visuelles (SMS non lus, appels manqués)
 * - Fond d'écran et mise en page
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "screen_home.h"
#include "screen_dialer.h"
#include "screen_contacts.h"
#include "screen_messages.h"
#include "screen_settings.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_icons.h"
#include "../ui/ui_fonts.h"
#include "../ui/ui_animations.h"
#include "../services/phone_service.h"
#include "../services/sms_service.h"
#include "../services/notification_service.h"
#include "../drivers/power/battery_monitor.h"
#include "../drivers/power/power_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES
// ============================================================

/** @brief État de l'écran d'accueil */
static HomeScreenState homeState;

// ============================================================
// CALLBACKS
// ============================================================

/**
 * @brief Callback de création de l'écran
 */
static void home_on_create(ScreenBase* screen)
{
    HOME_DEBUG("Création de l'écran d'accueil\n");
    
    memset(&homeState, 0, sizeof(HomeScreenState));
    homeState.showClock = true;
    homeState.showDate = true;
    
    // Créer les widgets
    screen_home_init_widgets(screen);
    
    // Configurer les transitions
    screen_set_transition(screen, SCREEN_TRANSITION_FADE, SCREEN_TRANSITION_FADE, 200);
}

/**
 * @brief Callback d'entrée dans l'écran
 */
static void home_on_enter(ScreenBase* screen)
{
    HOME_DEBUG("Entrée dans l'écran d'accueil\n");
    
    // Rafraîchir les données
    screen_home_refresh(screen);
    
    // Mettre à jour les badges
    homeState.unreadSMS = sms_service_get_unread_count();
    homeState.missedCalls = call_log_get_missed_count();
    
    if (homeState.tabBar)
    {
        ui_navbar_set_tab_badge(homeState.tabBar, 1, homeState.missedCalls);
        ui_navbar_set_tab_badge(homeState.tabBar, 2, homeState.unreadSMS);
    }
}

/**
 * @brief Callback de mise à jour périodique
 */
static void home_on_update(ScreenBase* screen)
{
    // Mettre à jour l'horloge toutes les secondes
    static uint32_t lastClockUpdate = 0;
    uint32_t now = HAL_GetTick();
    
    if (now - lastClockUpdate >= 1000)
    {
        lastClockUpdate = now;
        screen_home_refresh(screen);
    }
}

/**
 * @brief Callback de dessin (fond d'écran)
 */
static void home_on_draw(ScreenBase* screen)
{
    // Fond dégradé
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    // Dégradé vertical du haut vers le bas
    for (uint16_t y = 0; y < DISPLAY_HEIGHT; y++)
    {
        float progress = (float)y / DISPLAY_HEIGHT;
        uint16_t color = interpolate_color(colors->background, 
                                            darken_color(colors->background, 10), 
                                            progress);
        display_draw_hline(0, DISPLAY_WIDTH - 1, y, color);
    }
}

/**
 * @brief Callback de pression sur une touche
 */
static void home_on_key(ScreenBase* screen, KeyCode key, KeyEvent event)
{
    if (event != KEY_EVENT_PRESS) return;
    
    switch (key)
    {
        case KEY_CALL:
            // Ouvrir le composeur
            {
                ScreenBase* dialer = screen_dialer_create();
                if (dialer)
                {
                    screen_set_transition(dialer, SCREEN_TRANSITION_SLIDE_UP, 
                                          SCREEN_TRANSITION_SLIDE_DOWN, 250);
                    ui_push_screen((UIScreen*)dialer);
                }
            }
            break;
            
        case KEY_1:
            // Raccourci : numérotation rapide 1
            {
                int16_t contactIndex = contact_service_get_speed_dial(1);
                if (contactIndex >= 0)
                {
                    Contact* contact = contact_service_get(contactIndex);
                    if (contact) phone_service_dial(contact->number);
                }
            }
            break;
            
        case KEY_BACK:
            // Demander confirmation avant de quitter ?
            // Pour l'instant, on ne fait rien (l'écran d'accueil est la racine)
            break;
            
        default:
            break;
    }
}

// ============================================================
// CRÉATION DE L'ÉCRAN
// ============================================================

ScreenBase* screen_home_create(void)
{
    ScreenBase* screen = screen_create(SCREEN_HOME_NAME);
    if (screen == NULL) return NULL;
    
    // Assigner les callbacks
    screen->onCreate = home_on_create;
    screen->onEnter = home_on_enter;
    screen->onUpdate = home_on_update;
    screen->onDraw = home_on_draw;
    screen->onKeyPress = home_on_key;
    screen->onBackPressed = [](ScreenBase* s) -> bool {
        // Ne pas quitter l'écran d'accueil (c'est la racine)
        return true;  // true = événement consommé
    };
    
    // Titre
    strncpy(screen->title, "Accueil", 63);
    
    // Créer les widgets après avoir défini les callbacks
    screen_home_init_widgets(screen);
    
    return screen;
}

// ============================================================
// INITIALISATION DES WIDGETS
// ============================================================

void screen_home_init_widgets(ScreenBase* screen)
{
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    // --- Barre de statut ---
    homeState.statusBar = UI_STATUSBAR_CREATE();
    homeState.statusBar->base.rect = (UIRect){0, 0, DISPLAY_WIDTH, 24};
    screen_add_widget(screen, (UIWidget*)homeState.statusBar);
    
    // --- Horloge ---
    homeState.clockLabel = ui_label_create("clock", "12:45", 
                                            UI_RECT(0, 60, DISPLAY_WIDTH, 40));
    ui_label_set_style(homeState.clockLabel, LABEL_STYLE_TITLE);
    ui_label_set_alignment(homeState.clockLabel, UI_ALIGN_CENTER);
    homeState.clockLabel->base.visible = true;
    screen_add_widget(screen, (UIWidget*)homeState.clockLabel);
    
    // --- Date ---
    homeState.dateLabel = ui_label_create("date", "Lun 15 Janvier", 
                                           UI_RECT(0, 100, DISPLAY_WIDTH, 20));
    ui_label_set_style(homeState.dateLabel, LABEL_STYLE_CAPTION);
    ui_label_set_alignment(homeState.dateLabel, UI_ALIGN_CENTER);
    screen_add_widget(screen, (UIWidget*)homeState.dateLabel);
    
    // --- Titre de l'application ---
    homeState.titleLabel = ui_label_create("appTitle", "LoRa Phone",
                                            UI_RECT(0, 130, DISPLAY_WIDTH, 30));
    ui_label_set_style(homeState.titleLabel, LABEL_STYLE_BODY);
    ui_label_set_alignment(homeState.titleLabel, UI_ALIGN_CENTER);
    ui_label_set_color(homeState.titleLabel, colors->primary);
    screen_add_widget(screen, (UIWidget*)homeState.titleLabel);
    
    // --- Bouton Appeler ---
    homeState.btnCall = ui_button_create("btnCall", "📞  Appeler",
                                          UI_RECT(40, 180, 240, 50));
    ui_button_set_style(homeState.btnCall, BUTTON_STYLE_CALL);
    homeState.btnCall->onClick = [](UIButton* btn) {
        HOME_DEBUG("Ouverture du composeur\n");
        ScreenBase* dialer = screen_dialer_create();
        if (dialer)
        {
            screen_set_transition(dialer, SCREEN_TRANSITION_SLIDE_UP,
                                  SCREEN_TRANSITION_SLIDE_DOWN, 250);
            ui_push_screen((UIScreen*)dialer);
        }
    };
    screen_add_widget(screen, (UIWidget*)homeState.btnCall);
    
    // --- Bouton Contacts ---
    homeState.btnContacts = ui_button_create("btnContacts", "👤  Contacts",
                                              UI_RECT(40, 245, 240, 50));
    ui_button_set_style(homeState.btnContacts, BUTTON_STYLE_SECONDARY);
    homeState.btnContacts->onClick = [](UIButton* btn) {
        HOME_DEBUG("Ouverture des contacts\n");
        ScreenBase* contacts = screen_contacts_create();
        if (contacts)
        {
            screen_set_transition(contacts, SCREEN_TRANSITION_SLIDE_LEFT,
                                  SCREEN_TRANSITION_SLIDE_RIGHT, 250);
            ui_push_screen((UIScreen*)contacts);
        }
    };
    screen_add_widget(screen, (UIWidget*)homeState.btnContacts);
    
    // --- Bouton Messages ---
    homeState.btnMessages = ui_button_create("btnMessages", "💬  Messages",
                                              UI_RECT(40, 310, 240, 50));
    ui_button_set_style(homeState.btnMessages, BUTTON_STYLE_PRIMARY);
    homeState.btnMessages->onClick = [](UIButton* btn) {
        HOME_DEBUG("Ouverture des messages\n");
        ScreenBase* messages = screen_messages_create();
        if (messages)
        {
            screen_set_transition(messages, SCREEN_TRANSITION_SLIDE_LEFT,
                                  SCREEN_TRANSITION_SLIDE_RIGHT, 250);
            ui_push_screen((UIScreen*)messages);
        }
    };
    screen_add_widget(screen, (UIWidget*)homeState.btnMessages);
    
    // --- Bouton Paramètres ---
    homeState.btnSettings = ui_button_create("btnSettings", "⚙  Paramètres",
                                              UI_RECT(40, 375, 240, 50));
    ui_button_set_style(homeState.btnSettings, BUTTON_STYLE_OUTLINE);
    homeState.btnSettings->onClick = [](UIButton* btn) {
        HOME_DEBUG("Ouverture des paramètres\n");
        ScreenBase* settings = screen_settings_create();
        if (settings)
        {
            screen_set_transition(settings, SCREEN_TRANSITION_SLIDE_LEFT,
                                  SCREEN_TRANSITION_SLIDE_RIGHT, 250);
            ui_push_screen((UIScreen*)settings);
        }
    };
    screen_add_widget(screen, (UIWidget*)homeState.btnSettings);
    
    HOME_DEBUG("Widgets de l'écran d'accueil initialisés (%d widgets)\n", screen->widgetCount);
}

// ============================================================
// RAFRAÎCHISSEMENT
// ============================================================

void screen_home_refresh(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    // Mettre à jour l'heure
    if (homeState.clockLabel)
    {
        uint32_t now = HAL_GetTick() / 1000;
        uint8_t hours = (now / 3600) % 24;
        uint8_t minutes = (now % 3600) / 60;
        
        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hours, minutes);
        ui_label_set_text(homeState.clockLabel, timeStr);
    }
    
    // Mettre à jour la date
    if (homeState.dateLabel)
    {
        // Date simulée (à remplacer par RTC)
        const char* days[] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
        const char* months[] = {"Jan", "Fév", "Mar", "Avr", "Mai", "Juin",
                                "Juil", "Aoû", "Sep", "Oct", "Nov", "Déc"};
        
        char dateStr[32];
        snprintf(dateStr, sizeof(dateStr), "%s 15 %s", days[1], months[0]);
        ui_label_set_text(homeState.dateLabel, dateStr);
    }
    
    // Mettre à jour la barre de statut
    if (homeState.statusBar)
    {
        uint32_t now = HAL_GetTick() / 1000;
        ui_statusbar_set_time(homeState.statusBar, (now / 3600) % 24, (now % 3600) / 60);
        ui_statusbar_set_battery(homeState.statusBar, 
                                  battery_monitor_get_percent(), 
                                  battery_monitor_is_charging());
        ui_statusbar_set_signal(homeState.statusBar, lora_driver_get_signal_level());
    }
    
    // Mettre à jour les badges des onglets
    if (homeState.tabBar)
    {
        homeState.unreadSMS = sms_service_get_unread_count();
        homeState.missedCalls = call_log_get_missed_count();
        
        ui_navbar_set_tab_badge(homeState.tabBar, 1, homeState.missedCalls);
        ui_navbar_set_tab_badge(homeState.tabBar, 2, homeState.unreadSMS);
    }
}

// ============================================================
// NOTIFICATIONS
// ============================================================

void screen_home_add_notification(ScreenBase* screen, IconID icon)
{
    if (screen == NULL) return;
    
    if (homeState.notificationCount < HOME_MAX_VISIBLE_NOTIFICATIONS)
    {
        homeState.notificationIcons[homeState.notificationCount++] = icon;
        
        // Ajouter à la barre de statut
        if (homeState.statusBar)
        {
            ui_statusbar_add_icon(homeState.statusBar, icon);
        }
    }
}

void screen_home_clear_notifications(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    homeState.notificationCount = 0;
    
    if (homeState.statusBar)
    {
        ui_statusbar_clear_icons(homeState.statusBar);
    }
}

// ============================================================
// FONCTIONS UTILITAIRES
// ============================================================

static uint16_t interpolate_color(uint16_t color1, uint16_t color2, float progress)
{
    uint8_t r1 = (color1 >> 11) & 0x1F;
    uint8_t g1 = (color1 >> 5) & 0x3F;
    uint8_t b1 = color1 & 0x1F;
    
    uint8_t r2 = (color2 >> 11) & 0x1F;
    uint8_t g2 = (color2 >> 5) & 0x3F;
    uint8_t b2 = color2 & 0x1F;
    
    uint8_t r = (uint8_t)(r1 + (r2 - r1) * progress);
    uint8_t g = (uint8_t)(g1 + (g2 - g1) * progress);
    uint8_t b = (uint8_t)(b1 + (b2 - b1) * progress);
    
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
}

static uint16_t darken_color(uint16_t color, uint8_t amount)
{
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    
    r = (r * (100 - amount)) / 100;
    g = (g * (100 - amount)) / 100;
    b = (b * (100 - amount)) / 100;
    
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
}