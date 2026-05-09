/**
 * @file screen_call_incoming.cpp
 * @brief Implémentation de l'écran d'appel entrant
 * 
 * Fonctionnalités :
 * - Affichage du nom/numéro de l'appelant
 * - Animation de sonnerie (cercle clignotant)
 * - Boutons Accepter (vert) et Refuser (rouge)
 * - Réponses SMS rapides
 * - Timeout de sonnerie (30 secondes)
 * - Recherche du contact dans le carnet d'adresses
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "screen_call_incoming.h"
#include "screen_call_active.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_icons.h"
#include "../ui/ui_fonts.h"
#include "../ui/ui_animations.h"
#include "../services/phone_service.h"
#include "../services/contact_service.h"
#include "../services/sms_service.h"
#include "../drivers/audio/audio_manager.h"
#include "../drivers/power/power_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES
// ============================================================

/** @brief État de l'écran d'appel entrant */
static CallIncomingScreenState incomingState;

// ============================================================
// CALLBACKS
// ============================================================

/**
 * @brief Callback de création de l'écran
 */
static void incoming_on_create(ScreenBase* screen)
{
    INCOMING_DEBUG("Création de l'écran d'appel entrant\n");
    
    memset(&incomingState, 0, sizeof(CallIncomingScreenState));
    
    // Réponses SMS rapides par défaut
    incomingState.quickSMSReplies[0] = "Je ne peux pas parler maintenant";
    incomingState.quickSMSReplies[1] = "Je vous rappelle dans 5 minutes";
    incomingState.quickSMSReplies[2] = "En réunion, je vous rappelle";
    incomingState.quickSMSReplies[3] = "Envoie-moi un SMS";
    
    screen_call_incoming_init_widgets(screen);
}

/**
 * @brief Callback d'entrée dans l'écran
 */
static void incoming_on_enter(ScreenBase* screen)
{
    INCOMING_DEBUG("Appel entrant de : %s (%s)\n", 
                  incomingState.callerName, incomingState.callerNumber);
    
    // Démarrer la sonnerie
    incomingState.ringStartTime = HAL_GetTick();
    incomingState.blinkState = true;
    
    // Jouer la sonnerie
    audio_manager_play_ringtone(0);
    
    // Activer le vibreur si configuré
    if (settings_get_vibration())
    {
        // Vibrations gérées par le notification_service
    }
    
    // Allumer l'écran
    power_manager_activity();
}

/**
 * @brief Callback de mise à jour périodique
 */
static void incoming_on_update(ScreenBase* screen)
{
    // Mettre à jour l'animation de clignotement
    screen_call_incoming_update_blink(screen);
    
    // Vérifier le timeout
    uint32_t elapsed = (HAL_GetTick() - incomingState.ringStartTime) / 1000;
    incomingState.ringDuration = (uint8_t)elapsed;
    
    if (elapsed >= INCOMING_CALL_TIMEOUT_S)
    {
        INCOMING_DEBUG("Timeout sonnerie, appel manqué\n");
        screen_call_incoming_reject(screen);
    }
}

/**
 * @brief Callback de sortie de l'écran
 */
static void incoming_on_exit(ScreenBase* screen)
{
    // Arrêter la sonnerie
    audio_manager_stop_ringtone();
}

/**
 * @brief Callback de pression sur une touche
 */
static void incoming_on_key(ScreenBase* screen, KeyCode key, KeyEvent event)
{
    if (event != KEY_EVENT_PRESS) return;
    
    switch (key)
    {
        case KEY_CALL:
            // Touche Appel = Accepter
            screen_call_incoming_accept(screen);
            break;
            
        case KEY_END:
            // Touche Raccrocher = Refuser
            screen_call_incoming_reject(screen);
            break;
            
        case KEY_0: case KEY_1: case KEY_2: case KEY_3:
            // Réponse SMS rapide (touches 0-3)
            screen_call_incoming_quick_sms(screen, key - KEY_0);
            break;
            
        default:
            break;
    }
}

// ============================================================
// CRÉATION DE L'ÉCRAN
// ============================================================

ScreenBase* screen_call_incoming_create(const char* callerNumber, const char* callerName)
{
    ScreenBase* screen = screen_create(SCREEN_CALL_INCOMING_NAME);
    if (screen == NULL) return NULL;
    
    screen->onCreate = incoming_on_create;
    screen->onEnter = incoming_on_enter;
    screen->onUpdate = incoming_on_update;
    screen->onExit = incoming_on_exit;
    screen->onKeyPress = incoming_on_key;
    
    strncpy(screen->title, "Appel entrant", 63);
    
    // Sauvegarder les informations de l'appelant
    if (callerNumber) strncpy(incomingState.callerNumber, callerNumber, 15);
    
    // Chercher le contact
    if (callerNumber)
    {
        int16_t contactIndex = contact_service_find_by_number(callerNumber);
        if (contactIndex >= 0)
        {
            Contact* contact = contact_service_get(contactIndex);
            if (contact && contact->name[0])
            {
                strncpy(incomingState.callerName, contact->name, 31);
                incomingState.hasContact = true;
            }
        }
    }
    
    // Si pas de contact, utiliser le nom fourni ou le numéro
    if (incomingState.callerName[0] == '\0')
    {
        if (callerName && callerName[0])
            strncpy(incomingState.callerName, callerName, 31);
        else if (callerNumber)
            strncpy(incomingState.callerName, callerNumber, 31);
    }
    
    screen_call_incoming_init_widgets(screen);
    
    return screen;
}

// ============================================================
// INITIALISATION DES WIDGETS
// ============================================================

void screen_call_incoming_init_widgets(ScreenBase* screen)
{
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    uint16_t yPos = 60;
    
    // --- Titre "Appel entrant" ---
    incomingState.titleLabel = ui_label_create("title", "📞 Appel entrant",
                                                UI_RECT(0, yPos, DISPLAY_WIDTH, 40));
    ui_label_set_style(incomingState.titleLabel, LABEL_STYLE_TITLE);
    ui_label_set_alignment(incomingState.titleLabel, UI_ALIGN_CENTER);
    ui_label_set_color(incomingState.titleLabel, colors->primary);
    screen_add_widget(screen, (UIWidget*)incomingState.titleLabel);
    yPos += 50;
    
    // --- Nom de l'appelant ---
    incomingState.nameLabel = ui_label_create("name", incomingState.callerName,
                                               UI_RECT(20, yPos, 280, 40));
    ui_label_set_style(incomingState.nameLabel, LABEL_STYLE_TITLE);
    ui_label_set_alignment(incomingState.nameLabel, UI_ALIGN_CENTER);
    incomingState.nameLabel->base.rect.height = 40;
    screen_add_widget(screen, (UIWidget*)incomingState.nameLabel);
    yPos += 50;
    
    // --- Numéro de l'appelant ---
    incomingState.numberLabel = ui_label_create("number", incomingState.callerNumber,
                                                 UI_RECT(20, yPos, 280, 24));
    ui_label_set_style(incomingState.numberLabel, LABEL_STYLE_SUBTITLE);
    ui_label_set_alignment(incomingState.numberLabel, UI_ALIGN_CENTER);
    screen_add_widget(screen, (UIWidget*)incomingState.numberLabel);
    yPos += 50;
    
    // --- Statut ---
    incomingState.statusLabel = ui_label_create("status", "Sonnerie...",
                                                 UI_RECT(20, yPos, 280, 20));
    ui_label_set_style(incomingState.statusLabel, LABEL_STYLE_BODY);
    ui_label_set_alignment(incomingState.statusLabel, UI_ALIGN_CENTER);
    ui_label_set_color(incomingState.statusLabel, colors->textSecondary);
    screen_add_widget(screen, (UIWidget*)incomingState.statusLabel);
    yPos += 80;
    
    // --- Boutons Accepter / Refuser ---
    uint16_t btnW = 130;
    uint16_t btnH = 60;
    uint16_t btnY = yPos;
    
    // Bouton Accepter (vert)
    incomingState.btnAccept = ui_button_create("btnAccept", "✅ Accepter",
                                                UI_RECT(25, btnY, btnW, btnH));
    ui_button_set_style(incomingState.btnAccept, BUTTON_STYLE_CALL);
    incomingState.btnAccept->base.rect = (UIRect){25, btnY, btnW, btnH};
    incomingState.btnAccept->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_call_incoming_accept(parent);
    };
    screen_add_widget(screen, (UIWidget*)incomingState.btnAccept);
    
    // Bouton Refuser (rouge)
    incomingState.btnReject = ui_button_create("btnReject", "❌ Refuser",
                                                UI_RECT(25 + btnW + 10, btnY, btnW, btnH));
    ui_button_set_style(incomingState.btnReject, BUTTON_STYLE_END);
    incomingState.btnReject->base.rect = (UIRect){25 + btnW + 10, btnY, btnW, btnH};
    incomingState.btnReject->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_call_incoming_reject(parent);
    };
    screen_add_widget(screen, (UIWidget*)incomingState.btnReject);
    
    yPos = btnY + btnH + 20;
    
    // --- Bouton SMS rapide ---
    incomingState.btnQuickSMS = ui_button_create("btnQuickSMS", "💬 Répondre par SMS",
                                                  UI_RECT(40, yPos, 240, 40));
    ui_button_set_style(incomingState.btnQuickSMS, BUTTON_STYLE_OUTLINE);
    incomingState.btnQuickSMS->onClick = [](UIButton* btn) {
        // Par défaut, envoyer le premier message rapide
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_call_incoming_quick_sms(parent, 0);
    };
    screen_add_widget(screen, (UIWidget*)incomingState.btnQuickSMS);
    
    INCOMING_DEBUG("Widgets de l'écran d'appel entrant initialisés (%d widgets)\n", screen->widgetCount);
}

// ============================================================
// FONCTIONS D'ACTION
// ============================================================

void screen_call_incoming_accept(ScreenBase* screen)
{
    if (screen == NULL || incomingState.answered) return;
    
    INCOMING_DEBUG("Appel accepté\n");
    
    incomingState.answered = true;
    
    // Arrêter la sonnerie
    audio_manager_stop_ringtone();
    
    // Accepter l'appel
    phone_service_answer();
    
    // Ouvrir l'écran d'appel actif
    ScreenBase* callScreen = screen_call_active_create(incomingState.callerNumber,
                                                        incomingState.callerName);
    if (callScreen)
    {
        screen_call_active_set_state(callScreen, CALL_DISPLAY_CONNECTED);
        ui_replace_screen((UIScreen*)callScreen);
    }
}

void screen_call_incoming_reject(ScreenBase* screen)
{
    if (screen == NULL || incomingState.rejected) return;
    
    INCOMING_DEBUG("Appel refusé\n");
    
    incomingState.rejected = true;
    
    // Arrêter la sonnerie
    audio_manager_stop_ringtone();
    
    // Refuser l'appel
    phone_service_reject();
    
    // Ajouter au journal d'appels
    call_log_add_missed(incomingState.callerNumber);
    
    // Revenir à l'écran précédent
    ui_pop_screen();
}

void screen_call_incoming_quick_sms(ScreenBase* screen, uint8_t replyIndex)
{
    if (screen == NULL) return;
    if (replyIndex >= INCOMING_QUICK_SMS_COUNT) return;
    
    INCOMING_DEBUG("SMS rapide #%d : %s\n", replyIndex, 
                  incomingState.quickSMSReplies[replyIndex]);
    
    // Envoyer le SMS
    sms_service_send(incomingState.callerNumber, incomingState.quickSMSReplies[replyIndex]);
    
    // Refuser l'appel
    screen_call_incoming_reject(screen);
}

// ============================================================
// ANIMATION
// ============================================================

void screen_call_incoming_update_blink(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    uint32_t now = HAL_GetTick();
    
    if ((now - incomingState.lastBlinkTime) >= INCOMING_BLINK_INTERVAL_MS)
    {
        incomingState.blinkState = !incomingState.blinkState;
        incomingState.lastBlinkTime = now;
        
        // Mettre à jour l'affichage
        if (incomingState.titleLabel)
        {
            if (incomingState.blinkState)
            {
                ui_label_set_color(incomingState.titleLabel, 
                                  ui_theme_get_active()->colors.primary);
            }
            else
            {
                ui_label_set_color(incomingState.titleLabel, 
                                  ui_theme_get_active()->colors.textSecondary);
            }
        }
        
        // Mettre à jour le statut
        if (incomingState.statusLabel)
        {
            char statusStr[32];
            snprintf(statusStr, sizeof(statusStr), "Sonnerie... (%d s)", 
                     incomingState.ringDuration);
            ui_label_set_text(incomingState.statusLabel, statusStr);
        }
        
        screen->needsRedraw = true;
    }
}