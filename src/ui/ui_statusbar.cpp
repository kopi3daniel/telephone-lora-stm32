/**
 * @file ui_statusbar.cpp
 * @brief Implémentation du widget Barre de Statut
 * 
 * Fonctionnalités :
 * - Affichage de l'heure (HH:MM ou HH:MM:SS)
 * - Icône et pourcentage de batterie
 * - Barres de signal LoRa
 * - Icônes de notification, vibreur, cadenas
 * - Icônes supplémentaires
 * - Callback onTap
 * - Personnalisation des couleurs
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_statusbar.h"
#include "ui_fonts.h"
#include "ui_icons.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// FONCTIONS DE DESSIN
// ============================================================

/**
 * @brief Dessine la barre de statut complète
 */
static void statusbar_draw(UIWidget* widget)
{
    UIStatusBar* sb = (UIStatusBar*)widget;
    if (sb == NULL) return;
    
    UIRect* r = &widget->rect;
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    // --- Fond ---
    uint16_t bgColor = sb->backgroundColor;
    if (bgColor == 0) bgColor = colors->statusBarBg;
    
    display_fill_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1, bgColor);
    
    // Couleurs
    uint16_t textColor = sb->textColor ? sb->textColor : colors->textOnPrimary;
    uint16_t iconColor = sb->iconColor ? sb->iconColor : colors->textOnPrimary;
    
    // Police
    display_set_font(&font_5x7);
    display_set_text_color(textColor);
    
    // --- Heure (à gauche) ---
    uint16_t timeX = r->x + 4;
    uint16_t timeY = r->y + (r->height - display_text_height(1)) / 2;
    
    char timeStr[16];
    if (sb->showSeconds)
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", sb->hours, sb->minutes, (HAL_GetTick()/1000)%60);
    else
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", sb->hours, sb->minutes);
    
    display_draw_text(timeX, timeY, timeStr, textColor, 1);
    
    // --- Éléments de droite ---
    uint16_t rightX = r->x + r->width - 4;  // Marge droite
    uint16_t iconY = r->y + (r->height - ICON_SIZE_16) / 2;
    
    // Cadenas (verrouillage)
    if (sb->locked)
    {
        rightX -= ICON_SIZE_16 + 4;
        ui_icons_draw(ICON_LOCK, rightX, iconY, iconColor);
    }
    
    // Vibreur
    if (sb->vibrationEnabled)
    {
        rightX -= ICON_SIZE_16 + 4;
        ui_icons_draw(ICON_VIBRATE, rightX, iconY, iconColor);
    }
    else if (sb->notificationsMuted)
    {
        rightX -= ICON_SIZE_16 + 4;
        ui_icons_draw(ICON_VOLUME_MUTE, rightX, iconY, iconColor);
    }
    
    // Cloche (notifications)
    rightX -= ICON_SIZE_16 + 4;
    ui_icons_draw(sb->notificationsMuted ? ICON_BELL_MUTE : ICON_BELL, 
                  rightX, iconY, iconColor);
    
    // Icônes supplémentaires (SMS, etc.)
    for (int8_t i = sb->extraIconCount - 1; i >= 0; i--)
    {
        rightX -= ICON_SIZE_16 + 4;
        ui_icons_draw(sb->extraIcons[i], rightX, iconY, iconColor);
    }
    
    // Batterie
    if (sb->showBatteryPercent)
    {
        char battStr[8];
        snprintf(battStr, sizeof(battStr), "%d%%", sb->batteryPercent);
        uint16_t battTextW = display_text_width(battStr, 1);
        rightX -= battTextW + 2;
        display_draw_text(rightX, timeY, battStr, textColor, 1);
        rightX -= 4;
    }
    
    // Icône batterie
    rightX -= ICON_SIZE_16 + 4;
    ui_icons_draw_battery(rightX, iconY, sb->batteryPercent, sb->batteryCharging);
    
    // Signal LoRa
    if (sb->showSignalLabel)
    {
        rightX -= 28;
        display_draw_text(rightX, timeY, "LoRa", textColor, 1);
        rightX -= 4;
    }
    
    // Barres de signal
    rightX -= 22;
    ui_icons_draw_signal(rightX, iconY + 2, sb->signalLevel);
    
    // --- Ligne de séparation en bas (optionnelle) ---
    display_draw_hline(r->x, r->y + r->height - 1, r->x + r->width - 1, colors->border);
}

/**
 * @brief Gestion tactile (tap sur la barre)
 */
static void statusbar_touch(UIWidget* widget, uint16_t x, uint16_t y, TouchEvent event)
{
    UIStatusBar* sb = (UIStatusBar*)widget;
    if (sb == NULL) return;
    
    if (event == TOUCH_EVENT_PRESS && sb->onTap)
    {
        sb->onTap(sb);
    }
}

/**
 * @brief Mise à jour périodique (pour l'horloge)
 */
static void statusbar_update(UIWidget* widget)
{
    UIStatusBar* sb = (UIStatusBar*)widget;
    if (sb == NULL) return;
    
    // Mettre à jour l'heure toutes les secondes
    static uint32_t lastSecond = 0;
    uint32_t now = HAL_GetTick() / 1000;
    
    if (now != lastSecond)
    {
        lastSecond = now;
        
        // Récupérer l'heure depuis le RTC ou le timer système
        uint32_t totalSeconds = now % 86400;  // Secondes depuis minuit
        sb->hours = totalSeconds / 3600;
        sb->minutes = (totalSeconds % 3600) / 60;
        
        if (sb->showSeconds)
        {
            widget->needsRedraw = true;
        }
    }
    
    // Redessiner toutes les minutes (ou secondes si affichées)
    if (sb->showSeconds || (now % 60) == 0)
    {
        widget->needsRedraw = true;
    }
}

// ============================================================
// CRÉATION
// ============================================================

UIStatusBar* ui_statusbar_create(const char* name)
{
    UIStatusBar* sb = (UIStatusBar*)calloc(1, sizeof(UIStatusBar));
    if (sb == NULL) return NULL;
    
    // --- Initialiser le widget de base ---
    if (name) strncpy(sb->base.name, name, 31);
    sb->base.type = WIDGET_TYPE_CUSTOM + 10;  // Type personnalisé
    sb->base.rect = (UIRect){0, 0, DISPLAY_WIDTH, STATUSBAR_DEFAULT_HEIGHT};
    sb->base.visible = true;
    sb->base.enabled = true;
    sb->base.canFocus = false;
    sb->base.state = WIDGET_STATE_NORMAL;
    
    // --- Assigner les fonctions virtuelles ---
    sb->base.draw = statusbar_draw;
    sb->base.onTouch = statusbar_touch;
    sb->base.onUpdate = statusbar_update;
    
    // --- Valeurs par défaut ---
    sb->height = STATUSBAR_DEFAULT_HEIGHT;
    sb->hours = 12;
    sb->minutes = 0;
    sb->batteryPercent = 100;
    sb->batteryCharging = false;
    sb->signalLevel = 4;
    sb->notificationsMuted = false;
    sb->vibrationEnabled = true;
    sb->locked = false;
    sb->showSeconds = false;
    sb->showBatteryPercent = true;
    sb->showSignalLabel = false;
    sb->extraIconCount = 0;
    
    // Couleurs par défaut (seront écrasées par le thème)
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    sb->backgroundColor = colors->statusBarBg;
    sb->textColor = colors->textOnPrimary;
    sb->iconColor = colors->textOnPrimary;
    
    return sb;
}

// ============================================================
// MISE À JOUR
// ============================================================

void ui_statusbar_set_time(UIStatusBar* sb, uint8_t hours, uint8_t minutes)
{
    if (sb == NULL) return;
    sb->hours = hours % 24;
    sb->minutes = minutes % 60;
    sb->base.needsRedraw = true;
}

void ui_statusbar_set_battery(UIStatusBar* sb, uint8_t percent, bool charging)
{
    if (sb == NULL) return;
    sb->batteryPercent = (percent > 100) ? 100 : percent;
    sb->batteryCharging = charging;
    sb->base.needsRedraw = true;
}

void ui_statusbar_set_signal(UIStatusBar* sb, uint8_t level)
{
    if (sb == NULL) return;
    sb->signalLevel = (level > 4) ? 4 : level;
    sb->base.needsRedraw = true;
}

void ui_statusbar_set_notifications(UIStatusBar* sb, bool muted)
{
    if (sb == NULL) return;
    sb->notificationsMuted = muted;
    sb->base.needsRedraw = true;
}

void ui_statusbar_set_vibration(UIStatusBar* sb, bool enabled)
{
    if (sb == NULL) return;
    sb->vibrationEnabled = enabled;
    sb->base.needsRedraw = true;
}

void ui_statusbar_set_locked(UIStatusBar* sb, bool locked)
{
    if (sb == NULL) return;
    sb->locked = locked;
    sb->base.needsRedraw = true;
}

// ============================================================
// ICÔNES SUPPLÉMENTAIRES
// ============================================================

bool ui_statusbar_add_icon(UIStatusBar* sb, IconID icon)
{
    if (sb == NULL) return false;
    if (sb->extraIconCount >= STATUSBAR_MAX_NOTIFICATIONS) return false;
    
    // Vérifier si l'icône est déjà présente
    for (uint8_t i = 0; i < sb->extraIconCount; i++)
    {
        if (sb->extraIcons[i] == icon) return true;  // Déjà présente
    }
    
    sb->extraIcons[sb->extraIconCount++] = icon;
    sb->base.needsRedraw = true;
    return true;
}

bool ui_statusbar_remove_icon(UIStatusBar* sb, IconID icon)
{
    if (sb == NULL) return false;
    
    for (uint8_t i = 0; i < sb->extraIconCount; i++)
    {
        if (sb->extraIcons[i] == icon)
        {
            if (i < sb->extraIconCount - 1)
            {
                memmove(&sb->extraIcons[i], &sb->extraIcons[i + 1],
                        (sb->extraIconCount - i - 1) * sizeof(IconID));
            }
            sb->extraIconCount--;
            sb->base.needsRedraw = true;
            return true;
        }
    }
    return false;
}

void ui_statusbar_clear_icons(UIStatusBar* sb)
{
    if (sb == NULL) return;
    sb->extraIconCount = 0;
    sb->base.needsRedraw = true;
}

// ============================================================
// STYLE
// ============================================================

void ui_statusbar_set_colors(UIStatusBar* sb, uint16_t bg, uint16_t text, uint16_t icons)
{
    if (sb == NULL) return;
    sb->backgroundColor = bg;
    sb->textColor = text;
    sb->iconColor = icons;
    sb->base.needsRedraw = true;
}

void ui_statusbar_set_height(UIStatusBar* sb, uint8_t height)
{
    if (sb == NULL) return;
    sb->height = height;
    sb->base.rect.height = height;
    sb->base.needsRedraw = true;
}

void ui_statusbar_show_battery_percent(UIStatusBar* sb, bool show)
{
    if (sb == NULL) return;
    sb->showBatteryPercent = show;
    sb->base.needsRedraw = true;
}

void ui_statusbar_show_signal_label(UIStatusBar* sb, bool show)
{
    if (sb == NULL) return;
    sb->showSignalLabel = show;
    sb->base.needsRedraw = true;
}