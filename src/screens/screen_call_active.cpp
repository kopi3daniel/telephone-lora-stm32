/**
 * @file screen_call_active.cpp
 * @brief Implémentation de l'écran d'appel en cours
 * 
 * Fonctionnalités :
 * - Affichage du nom/numéro du correspondant
 * - Timer de durée d'appel (MM:SS)
 * - VU-meter (indicateur de niveau audio)
 * - Contrôles : Muet, Haut-parleur, Clavier DTMF
 * - Bouton Raccrocher
 * - États : Numérotation, Sonnerie, Connecté, Terminé
 * - Indicateur de qualité du signal
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "screen_call_active.h"
#include "screen_dialer.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_icons.h"
#include "../ui/ui_fonts.h"
#include "../ui/ui_animations.h"
#include "../services/phone_service.h"
#include "../drivers/audio/audio_manager.h"
#include "../drivers/lora/lora_driver.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES
// ============================================================

/** @brief État de l'écran d'appel */
static CallActiveScreenState callState;

// ============================================================
// CALLBACKS
// ============================================================

/**
 * @brief Callback de création de l'écran
 */
static void call_active_on_create(ScreenBase* screen)
{
    CALL_ACTIVE_DEBUG("Création de l'écran d'appel\n");
    
    memset(&callState, 0, sizeof(CallActiveScreenState));
    callState.displayState = CALL_DISPLAY_DIALING;
    callState.signalQuality = 100;
    
    screen_call_active_init_widgets(screen);
}

/**
 * @brief Callback d'entrée dans l'écran
 */
static void call_active_on_enter(ScreenBase* screen)
{
    CALL_ACTIVE_DEBUG("Entrée dans l'écran d'appel\n");
    
    // Démarrer le timer
    callState.callStartTime = HAL_GetTick();
    
    // Configurer l'audio
    audio_manager_set_mute(false);
    audio_manager_start_call();
}

/**
 * @brief Callback de mise à jour périodique
 */
static void call_active_on_update(ScreenBase* screen)
{
    // Mettre à jour le timer toutes les secondes
    static uint32_t lastUpdate = 0;
    uint32_t now = HAL_GetTick();
    
    if (now - lastUpdate >= 1000)
    {
        lastUpdate = now;
        screen_call_active_update_duration(screen);
    }
}

/**
 * @brief Callback de sortie de l'écran
 */
static void call_active_on_exit(ScreenBase* screen)
{
    CALL_ACTIVE_DEBUG("Sortie de l'écran d'appel\n");
    
    // Arrêter l'audio
    audio_manager_stop_call();
}

/**
 * @brief Callback de pression sur une touche
 */
static void call_active_on_key(ScreenBase* screen, KeyCode key, KeyEvent event)
{
    if (event != KEY_EVENT_PRESS) return;
    
    switch (key)
    {
        case KEY_END:
            // Raccrocher
            screen_call_active_end_call(screen);
            break;
            
        case KEY_MUTE:
            // Basculer le mode muet
            screen_call_active_toggle_mute(screen);
            break;
            
        case KEY_VOL:
            // Basculer le haut-parleur
            screen_call_active_toggle_speaker(screen);
            break;
            
        case KEY_0: case KEY_1: case KEY_2: case KEY_3: case KEY_4:
        case KEY_5: case KEY_6: case KEY_7: case KEY_8: case KEY_9:
        case KEY_STAR: case KEY_HASH:
            // Jouer les tonalités DTMF
            {
                char digit = (key >= KEY_0 && key <= KEY_9) ? '0' + (key - KEY_0) :
                             (key == KEY_STAR) ? '*' : '#';
                audio_manager_play_dtmf(digit, 150);
            }
            break;
            
        default:
            break;
    }
}

// ============================================================
// CRÉATION DE L'ÉCRAN
// ============================================================

ScreenBase* screen_call_active_create(const char* phoneNumber, const char* contactName)
{
    ScreenBase* screen = screen_create(SCREEN_CALL_ACTIVE_NAME);
    if (screen == NULL) return NULL;
    
    screen->onCreate = call_active_on_create;
    screen->onEnter = call_active_on_enter;
    screen->onUpdate = call_active_on_update;
    screen->onExit = call_active_on_exit;
    screen->onKeyPress = call_active_on_key;
    
    strncpy(screen->title, "En appel", 63);
    
    // Sauvegarder les informations
    if (phoneNumber) strncpy(callState.phoneNumber, phoneNumber, 15);
    if (contactName) strncpy(callState.contactName, contactName, 31);
    else strncpy(callState.contactName, phoneNumber ? phoneNumber : "Inconnu", 31);
    
    screen_call_active_init_widgets(screen);
    
    // Ajouter un timer pour le rafraîchissement
    screen_add_timer(screen, 1000, []() {
        // Timer déjà géré dans onUpdate
    });
    
    return screen;
}

// ============================================================
// INITIALISATION DES WIDGETS
// ============================================================

void screen_call_active_init_widgets(ScreenBase* screen)
{
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    uint16_t yPos = 60;
    
    // --- Nom du correspondant ---
    callState.nameLabel = ui_label_create("name", callState.contactName,
                                           UI_RECT(20, yPos, 280, 36));
    ui_label_set_style(callState.nameLabel, LABEL_STYLE_TITLE);
    ui_label_set_alignment(callState.nameLabel, UI_ALIGN_CENTER);
    callState.nameLabel->base.rect.height = 36;
    screen_add_widget(screen, (UIWidget*)callState.nameLabel);
    yPos += 44;
    
    // --- Numéro du correspondant ---
    callState.numberLabel = ui_label_create("number", callState.phoneNumber,
                                             UI_RECT(20, yPos, 280, 24));
    ui_label_set_style(callState.numberLabel, LABEL_STYLE_SUBTITLE);
    ui_label_set_alignment(callState.numberLabel, UI_ALIGN_CENTER);
    callState.numberLabel->base.rect.height = 24;
    screen_add_widget(screen, (UIWidget*)callState.numberLabel);
    yPos += 36;
    
    // --- Statut (Numérotation... / En appel) ---
    callState.statusLabel = ui_label_create("status", "Numérotation en cours...",
                                             UI_RECT(20, yPos, 280, 20));
    ui_label_set_style(callState.statusLabel, LABEL_STYLE_BODY);
    ui_label_set_alignment(callState.statusLabel, UI_ALIGN_CENTER);
    ui_label_set_color(callState.statusLabel, colors->textSecondary);
    screen_add_widget(screen, (UIWidget*)callState.statusLabel);
    yPos += 40;
    
    // --- Timer de durée ---
    callState.durationLabel = ui_label_create("duration", "00:00",
                                               UI_RECT(0, yPos, DISPLAY_WIDTH, 50));
    ui_label_set_style(callState.durationLabel, LABEL_STYLE_TITLE);
    ui_label_set_alignment(callState.durationLabel, UI_ALIGN_CENTER);
    // Utiliser une taille plus grande pour le timer
    display_set_text_size(2);
    screen_add_widget(screen, (UIWidget*)callState.durationLabel);
    yPos += 60;
    
    // --- Indicateur de qualité du signal ---
    callState.signalLabel = ui_label_create("signal", "Signal: Excellent",
                                             UI_RECT(20, yPos, 280, 20));
    ui_label_set_style(callState.signalLabel, LABEL_STYLE_CAPTION);
    ui_label_set_alignment(callState.signalLabel, UI_ALIGN_CENTER);
    ui_label_set_color(callState.signalLabel, colors->success);
    screen_add_widget(screen, (UIWidget*)callState.signalLabel);
    yPos += 50;
    
    // --- Contrôles d'appel (rangée de 3 boutons) ---
    uint16_t btnY = yPos;
    uint16_t btnW = 85;
    uint16_t btnH = 60;
    uint16_t spacing = 12;
    uint16_t totalWidth = 3 * btnW + 2 * spacing;
    uint16_t startX = (DISPLAY_WIDTH - totalWidth) / 2;
    
    // Bouton Muet
    callState.btnMute = ui_button_create("btnMute", "🔇\nMuet",
                                          UI_RECT(startX, btnY, btnW, btnH));
    ui_button_set_style(callState.btnMute, BUTTON_STYLE_SECONDARY);
    callState.btnMute->base.rect = (UIRect){startX, btnY, btnW, btnH};
    callState.btnMute->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_call_active_toggle_mute(parent);
    };
    screen_add_widget(screen, (UIWidget*)callState.btnMute);
    
    // Bouton Haut-parleur
    callState.btnSpeaker = ui_button_create("btnSpeaker", "🔊\nHP",
                                             UI_RECT(startX + btnW + spacing, btnY, btnW, btnH));
    ui_button_set_style(callState.btnSpeaker, BUTTON_STYLE_SECONDARY);
    callState.btnSpeaker->base.rect = (UIRect){startX + btnW + spacing, btnY, btnW, btnH};
    callState.btnSpeaker->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_call_active_toggle_speaker(parent);
    };
    screen_add_widget(screen, (UIWidget*)callState.btnSpeaker);
    
    // Bouton Clavier
    callState.btnDialpad = ui_button_create("btnDialpad", "📞\nClavier",
                                             UI_RECT(startX + 2*(btnW + spacing), btnY, btnW, btnH));
    ui_button_set_style(callState.btnDialpad, BUTTON_STYLE_SECONDARY);
    callState.btnDialpad->base.rect = (UIRect){startX + 2*(btnW + spacing), btnY, btnW, btnH};
    callState.btnDialpad->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_call_active_toggle_dialpad(parent);
    };
    screen_add_widget(screen, (UIWidget*)callState.btnDialpad);
    
    yPos = btnY + btnH + 20;
    
    // --- Bouton Raccrocher ---
    callState.btnEnd = ui_button_create("btnEnd", "⏹  Raccrocher",
                                         UI_RECT(40, yPos, 240, 55));
    ui_button_set_style(callState.btnEnd, BUTTON_STYLE_END);
    callState.btnEnd->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_call_active_end_call(parent);
    };
    screen_add_widget(screen, (UIWidget*)callState.btnEnd);
    
    CALL_ACTIVE_DEBUG("Widgets de l'écran d'appel initialisés (%d widgets)\n", screen->widgetCount);
}

// ============================================================
// FONCTIONS DE MISE À JOUR
// ============================================================

void screen_call_active_update_duration(ScreenBase* screen)
{
    if (screen == NULL || callState.displayState != CALL_DISPLAY_CONNECTED) return;
    
    uint32_t elapsed = (HAL_GetTick() - callState.callStartTime) / 1000;
    callState.callDuration = elapsed;
    
    uint32_t minutes = elapsed / 60;
    uint32_t seconds = elapsed % 60;
    
    char durationStr[16];
    snprintf(durationStr, sizeof(durationStr), "%02lu:%02lu", (unsigned long)minutes, (unsigned long)seconds);
    
    if (callState.durationLabel)
    {
        ui_label_set_text(callState.durationLabel, durationStr);
    }
}

void screen_call_active_update_vu(ScreenBase* screen, uint8_t level, uint8_t peak)
{
    if (screen == NULL) return;
    
    callState.vuLevel = level;
    callState.vuPeak = peak;
    
    // Le VU-meter est dessiné directement dans le draw de l'écran
    screen->needsRedraw = true;
}

void screen_call_active_update_signal(ScreenBase* screen, int16_t rssi)
{
    if (screen == NULL) return;
    
    callState.signalRssi = rssi;
    
    // Calculer la qualité (0-100)
    if (rssi >= -50) callState.signalQuality = 100;
    else if (rssi <= -130) callState.signalQuality = 0;
    else callState.signalQuality = (uint8_t)((rssi + 130) * 100 / 80);
    
    // Mettre à jour le label
    if (callState.signalLabel)
    {
        const char* qualityText;
        uint16_t qualityColor;
        
        if (callState.signalQuality > 75)
        {
            qualityText = "Signal: Excellent";
            qualityColor = ui_theme_get_active()->colors.success;
        }
        else if (callState.signalQuality > 40)
        {
            qualityText = "Signal: Bon";
            qualityColor = ui_theme_get_active()->colors.primary;
        }
        else if (callState.signalQuality > 15)
        {
            qualityText = "Signal: Faible";
            qualityColor = ui_theme_get_active()->colors.warning;
        }
        else
        {
            qualityText = "Signal: Très faible";
            qualityColor = ui_theme_get_active()->colors.error;
        }
        
        ui_label_set_text(callState.signalLabel, qualityText);
        ui_label_set_color(callState.signalLabel, qualityColor);
    }
}

// ============================================================
// FONCTIONS DE CONTRÔLE
// ============================================================

void screen_call_active_toggle_mute(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    callState.muted = !callState.muted;
    audio_manager_set_mute(callState.muted);
    
    // Mettre à jour le texte du bouton
    if (callState.btnMute)
    {
        ui_button_set_text(callState.btnMute, callState.muted ? "🔇\nMuet ON" : "🎤\nMuet OFF");
    }
    
    CALL_ACTIVE_DEBUG("Muet : %s\n", callState.muted ? "ON" : "OFF");
}

void screen_call_active_toggle_speaker(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    callState.speakerOn = !callState.speakerOn;
    phone_service_speaker(callState.speakerOn);
    
    if (callState.btnSpeaker)
    {
        ui_button_set_text(callState.btnSpeaker, callState.speakerOn ? "🔊\nHP ON" : "🔈\nHP OFF");
    }
    
    CALL_ACTIVE_DEBUG("Haut-parleur : %s\n", callState.speakerOn ? "ON" : "OFF");
}

void screen_call_active_toggle_dialpad(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    callState.dialpadVisible = !callState.dialpadVisible;
    
    // TODO: Afficher/masquer un clavier DTMF
    CALL_ACTIVE_DEBUG("Clavier : %s\n", callState.dialpadVisible ? "Visible" : "Caché");
}

void screen_call_active_end_call(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    CALL_ACTIVE_DEBUG("Fin d'appel (durée: %lu s)\n", (unsigned long)callState.callDuration);
    
    // Terminer l'appel
    phone_service_hangup();
    
    // Mettre à jour l'écran (afficher "Appel terminé")
    screen_call_active_set_state(screen, CALL_DISPLAY_ENDED);
    
    // Attendre 2 secondes puis revenir à l'écran précédent
    // (géré par un timer)
    screen_add_timer(screen, 2000, []() {
        // Revenir à l'écran précédent
        ui_pop_screen();
    });
}

// ============================================================
// ÉTAT
// ============================================================

void screen_call_active_set_state(ScreenBase* screen, CallDisplayState state)
{
    if (screen == NULL) return;
    
    CallDisplayState oldState = callState.displayState;
    callState.displayState = state;
    
    switch (state)
    {
        case CALL_DISPLAY_DIALING:
            if (callState.statusLabel)
                ui_label_set_text(callState.statusLabel, "Numérotation en cours...");
            break;
            
        case CALL_DISPLAY_RINGING:
            if (callState.statusLabel)
                ui_label_set_text(callState.statusLabel, "Sonnerie...");
            break;
            
        case CALL_DISPLAY_CONNECTED:
            if (callState.statusLabel)
                ui_label_set_text(callState.statusLabel, "En communication");
            callState.callStartTime = HAL_GetTick();
            // Activer les contrôles
            if (callState.btnMute) callState.btnMute->base.enabled = true;
            if (callState.btnSpeaker) callState.btnSpeaker->base.enabled = true;
            if (callState.btnDialpad) callState.btnDialpad->base.enabled = true;
            break;
            
        case CALL_DISPLAY_ENDED:
            if (callState.statusLabel)
                ui_label_set_text(callState.statusLabel, "Appel terminé");
            if (callState.nameLabel)
                ui_label_set_color(callState.nameLabel, ui_theme_get_active()->colors.textSecondary);
            // Désactiver les contrôles
            if (callState.btnMute) callState.btnMute->base.enabled = false;
            if (callState.btnSpeaker) callState.btnSpeaker->base.enabled = false;
            if (callState.btnDialpad) callState.btnDialpad->base.enabled = false;
            if (callState.btnEnd) callState.btnEnd->base.enabled = false;
            break;
    }
    
    screen->needsRedraw = true;
}

CallDisplayState screen_call_active_get_state(ScreenBase* screen)
{
    return screen ? callState.displayState : CALL_DISPLAY_DIALING;
}