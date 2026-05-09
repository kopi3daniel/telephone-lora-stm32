/**
 * @file ui_dialog.cpp
 * @brief Implémentation du widget Boîte de Dialogue (UIDialog)
 * 
 * Fonctionnalités :
 * - Rendu graphique avec overlay semi-transparent
 * - Types de dialogues (info, confirm, warning, error, input, progress, custom)
 * - Gestion tactile des boutons
 * - Saisie de texte (type INPUT)
 * - Barre de progression (type PROGRESS)
 * - Callbacks de résultat
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_dialog.h"
#include "../drivers/display/display_manager.h"
#include "../drivers/touch/touch_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Dialogue actif */
static UIDialog* active_dialog = NULL;

/** @brief États des boutons */
static bool btn_ok_pressed = false;
static bool btn_cancel_pressed = false;

// ============================================================
// DIMENSIONS PAR DÉFAUT
// ============================================================

#define DIALOG_DEFAULT_WIDTH        280
#define DIALOG_DEFAULT_HEIGHT       200
#define DIALOG_TITLE_HEIGHT         40
#define DIALOG_BUTTON_HEIGHT        40
#define DIALOG_PADDING              16
#define DIALOG_ICON_SIZE            40

// ============================================================
// FONCTIONS DE DESSIN
// ============================================================

/**
 * @brief Dessine le dialogue complet
 */
static void dialog_draw(UIWidget* widget)
{
    UIDialog* dialog = (UIDialog*)widget;
    if (dialog == NULL) return;
    
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    // --- Dimensions ---
    uint16_t dialogW = DIALOG_DEFAULT_WIDTH;
    uint16_t dialogH = DIALOG_DEFAULT_HEIGHT;
    uint16_t dialogX = (DISPLAY_WIDTH - dialogW) / 2;
    uint16_t dialogY = (DISPLAY_HEIGHT - dialogH) / 2;
    
    // Ajuster pour les dialogues plus grands
    if (dialog->type == DIALOG_TYPE_INPUT) dialogH += 20;
    if (dialog->type == DIALOG_TYPE_PROGRESS) dialogH = 180;
    
    // --- Overlay semi-transparent ---
    display_fill_rect(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1, 0x0000);
    // Note: la transparence réelle n'est pas supportée, on simule avec un motif
    
    // --- Fond du dialogue ---
    display_fill_round_rect(dialogX, dialogY, dialogX + dialogW - 1, dialogY + dialogH - 1,
                             dialog->cornerRadius, colors->surface);
    
    // --- Bordure ---
    display_draw_round_rect(dialogX, dialogY, dialogX + dialogW - 1, dialogY + dialogH - 1,
                             dialog->cornerRadius, colors->border);
    
    // --- Ombre (simulée) ---
    display_fill_round_rect(dialogX + 2, dialogY + 2, 
                             dialogX + dialogW + 1, dialogY + dialogH + 1,
                             dialog->cornerRadius + 2, colors->shadow);
    // Redessiner le fond par-dessus
    display_fill_round_rect(dialogX, dialogY, dialogX + dialogW - 1, dialogY + dialogH - 1,
                             dialog->cornerRadius, colors->surface);
    
    // --- Icône ---
    if (dialog->icon != DIALOG_ICON_NONE)
    {
        draw_dialog_icon(dialogX + dialogW / 2 - DIALOG_ICON_SIZE / 2, 
                         dialogY + 20, dialog->icon);
    }
    
    uint16_t contentY = dialogY + (dialog->icon != DIALOG_ICON_NONE ? 70 : 16);
    
    // --- Titre ---
    if (strlen(dialog->title) > 0)
    {
        display_set_font(&font_8x16);
        display_set_text_color(dialog->titleColor);
        display_draw_text_center(contentY, dialog->title, dialog->titleColor, 1);
        contentY += 30;
    }
    
    // --- Message ---
    if (strlen(dialog->message) > 0)
    {
        display_set_font(&font_5x7);
        display_set_text_color(dialog->messageColor);
        
        // Centrer le message (avec word wrap simple)
        uint16_t msgWidth = display_text_width(dialog->message, 1);
        uint16_t msgX = dialogX + DIALOG_PADDING;
        
        if (msgWidth < dialogW - 2 * DIALOG_PADDING)
        {
            msgX = dialogX + (dialogW - msgWidth) / 2;
        }
        
        display_draw_text(msgX, contentY, dialog->message, dialog->messageColor, 1);
        contentY += 40;
    }
    
    // --- Zone de saisie (type INPUT) ---
    if (dialog->type == DIALOG_TYPE_INPUT)
    {
        uint16_t inputX = dialogX + DIALOG_PADDING;
        uint16_t inputW = dialogW - 2 * DIALOG_PADDING;
        uint16_t inputH = 36;
        
        // Fond de la zone de saisie
        display_fill_round_rect(inputX, contentY, inputX + inputW - 1, contentY + inputH - 1,
                                 4, colors->inputBg);
        display_draw_round_rect(inputX, contentY, inputX + inputW - 1, contentY + inputH - 1,
                                 4, colors->border);
        
        // Texte saisi ou placeholder
        const char* displayText = (strlen(dialog->inputText) > 0) ? 
                                   dialog->inputText : dialog->inputPlaceholder;
        uint16_t textColor = (strlen(dialog->inputText) > 0) ? 
                              colors->textPrimary : colors->textHint;
        
        // Mode sécurisé
        char secureText[128];
        if (dialog->inputSecure && strlen(dialog->inputText) > 0)
        {
            uint16_t len = strlen(dialog->inputText);
            for (uint16_t i = 0; i < len && i < 127; i++) secureText[i] = '*';
            secureText[len] = '\0';
            displayText = secureText;
        }
        
        display_draw_text(inputX + 8, contentY + 10, displayText, textColor, 1);
        
        contentY += inputH + 16;
    }
    
    // --- Barre de progression (type PROGRESS) ---
    if (dialog->type == DIALOG_TYPE_PROGRESS)
    {
        uint16_t progX = dialogX + DIALOG_PADDING;
        uint16_t progW = dialogW - 2 * DIALOG_PADDING;
        uint16_t progH = 12;
        uint16_t progY = contentY;
        
        // Fond
        display_fill_round_rect(progX, progY, progX + progW - 1, progY + progH - 1,
                                 progH / 2, colors->disabled);
        
        // Remplissage
        uint16_t fillW = (uint16_t)((uint32_t)progW * dialog->progressValue / 100);
        if (fillW > 0)
        {
            display_fill_round_rect(progX, progY, progX + fillW - 1, progY + progH - 1,
                                     progH / 2, colors->primary);
        }
        
        // Animation indéterminée
        if (dialog->progressIndeterminate)
        {
            uint16_t animX = progX + ((HAL_GetTick() / 10) % (progW - 40));
            display_fill_round_rect(animX, progY, animX + 40, progY + progH - 1,
                                     progH / 2, colors->primary);
        }
        
        contentY += progH + 16;
    }
    
    // --- Contenu personnalisé (type CUSTOM) ---
    if (dialog->type == DIALOG_TYPE_CUSTOM && dialog->customDraw)
    {
        UIRect contentRect = {dialogX + DIALOG_PADDING, contentY, 
                               dialogW - 2 * DIALOG_PADDING, 
                               dialogH - contentY + dialogY - DIALOG_BUTTON_HEIGHT - DIALOG_PADDING};
        dialog->customDraw(dialog, &contentRect);
    }
    
    // --- Boutons ---
    uint16_t buttonY = dialogY + dialogH - DIALOG_BUTTON_HEIGHT - 12;
    
    // Séparateur
    display_fill_rect(dialogX, buttonY - 4, dialogX + dialogW - 1, buttonY - 3, colors->divider);
    
    if (dialog->showCancel)
    {
        // Deux boutons : Annuler | OK
        uint16_t btnW = (dialogW - 3 * DIALOG_PADDING) / 2;
        uint16_t btnCancelX = dialogX + DIALOG_PADDING;
        uint16_t btnOkX = dialogX + dialogW - DIALOG_PADDING - btnW;
        
        // Bouton Annuler
        uint16_t cancelBg = btn_cancel_pressed ? colors->disabled : colors->surface;
        display_fill_round_rect(btnCancelX, buttonY, btnCancelX + btnW - 1, 
                                 buttonY + DIALOG_BUTTON_HEIGHT - 1, 8, cancelBg);
        display_set_text_color(colors->textSecondary);
        display_draw_text_center(buttonY + 10, dialog->buttonCancelText, 
                                  colors->textSecondary, 1);
        
        // Bouton OK
        uint16_t okBg = btn_ok_pressed ? darken_color(colors->primary, 20) : colors->primary;
        display_fill_round_rect(btnOkX, buttonY, btnOkX + btnW - 1, 
                                 buttonY + DIALOG_BUTTON_HEIGHT - 1, 8, okBg);
        display_set_text_color(colors->onPrimary);
        display_draw_text_center(buttonY + 10, dialog->buttonOkText, 
                                  colors->onPrimary, 1);
    }
    else
    {
        // Un seul bouton OK centré
        uint16_t btnW = dialogW - 4 * DIALOG_PADDING;
        uint16_t btnX = dialogX + 2 * DIALOG_PADDING;
        
        uint16_t okBg = btn_ok_pressed ? darken_color(colors->primary, 20) : colors->primary;
        display_fill_round_rect(btnX, buttonY, btnX + btnW - 1, 
                                 buttonY + DIALOG_BUTTON_HEIGHT - 1, 8, okBg);
        display_set_text_color(colors->onPrimary);
        display_draw_text_center(buttonY + 10, dialog->buttonOkText, 
                                  colors->onPrimary, 1);
    }
}

// ============================================================
// FONCTIONS TACTILES
// ============================================================

/**
 * @brief Vérifie si un point est dans les limites du dialogue
 */
static bool is_point_in_dialog_area(uint16_t x, uint16_t y)
{
    uint16_t dialogW = DIALOG_DEFAULT_WIDTH;
    uint16_t dialogH = DIALOG_DEFAULT_HEIGHT;
    uint16_t dialogX = (DISPLAY_WIDTH - dialogW) / 2;
    uint16_t dialogY = (DISPLAY_HEIGHT - dialogH) / 2;
    
    return (x >= dialogX && x <= dialogX + dialogW &&
            y >= dialogY && y <= dialogY + dialogH);
}

/**
 * @brief Vérifie si un point est sur le bouton OK
 */
static bool is_point_on_ok_button(uint16_t x, uint16_t y)
{
    uint16_t dialogW = DIALOG_DEFAULT_WIDTH;
    uint16_t dialogH = DIALOG_DEFAULT_HEIGHT;
    uint16_t dialogX = (DISPLAY_WIDTH - dialogW) / 2;
    uint16_t dialogY = (DISPLAY_HEIGHT - dialogH) / 2;
    uint16_t buttonY = dialogY + dialogH - DIALOG_BUTTON_HEIGHT - 12;
    
    if (active_dialog && active_dialog->showCancel)
    {
        uint16_t btnW = (dialogW - 3 * DIALOG_PADDING) / 2;
        uint16_t btnOkX = dialogX + dialogW - DIALOG_PADDING - btnW;
        return (x >= btnOkX && x <= btnOkX + btnW &&
                y >= buttonY && y <= buttonY + DIALOG_BUTTON_HEIGHT);
    }
    else
    {
        uint16_t btnW = dialogW - 4 * DIALOG_PADDING;
        uint16_t btnX = dialogX + 2 * DIALOG_PADDING;
        return (x >= btnX && x <= btnX + btnW &&
                y >= buttonY && y <= buttonY + DIALOG_BUTTON_HEIGHT);
    }
}

/**
 * @brief Vérifie si un point est sur le bouton Annuler
 */
static bool is_point_on_cancel_button(uint16_t x, uint16_t y)
{
    if (active_dialog == NULL || !active_dialog->showCancel) return false;
    
    uint16_t dialogW = DIALOG_DEFAULT_WIDTH;
    uint16_t dialogH = DIALOG_DEFAULT_HEIGHT;
    uint16_t dialogX = (DISPLAY_WIDTH - dialogW) / 2;
    uint16_t dialogY = (DISPLAY_HEIGHT - dialogH) / 2;
    uint16_t buttonY = dialogY + dialogH - DIALOG_BUTTON_HEIGHT - 12;
    
    uint16_t btnW = (dialogW - 3 * DIALOG_PADDING) / 2;
    uint16_t btnCancelX = dialogX + DIALOG_PADDING;
    
    return (x >= btnCancelX && x <= btnCancelX + btnW &&
            y >= buttonY && y <= buttonY + DIALOG_BUTTON_HEIGHT);
}

/**
 * @brief Gestion tactile du dialogue
 */
void ui_dialog_handle_touch(uint16_t x, uint16_t y, TouchEvent event)
{
    if (active_dialog == NULL) return;
    
    switch (event)
    {
        case TOUCH_EVENT_PRESS:
            if (is_point_on_ok_button(x, y))
            {
                btn_ok_pressed = true;
                active_dialog->base.needsRedraw = true;
            }
            else if (is_point_on_cancel_button(x, y))
            {
                btn_cancel_pressed = true;
                active_dialog->base.needsRedraw = true;
            }
            break;
            
        case TOUCH_EVENT_RELEASE:
            if (btn_ok_pressed && is_point_on_ok_button(x, y))
            {
                btn_ok_pressed = false;
                ui_dialog_dismiss(active_dialog, DIALOG_RESULT_OK);
            }
            else if (btn_cancel_pressed && is_point_on_cancel_button(x, y))
            {
                btn_cancel_pressed = false;
                ui_dialog_dismiss(active_dialog, DIALOG_RESULT_CANCEL);
            }
            else if (active_dialog->dismissOnOutside && !is_point_in_dialog_area(x, y))
            {
                ui_dialog_dismiss(active_dialog, DIALOG_RESULT_CLOSE);
            }
            else
            {
                btn_ok_pressed = false;
                btn_cancel_pressed = false;
                active_dialog->base.needsRedraw = true;
            }
            break;
            
        default:
            break;
    }
}

// ============================================================
// CRÉATION
// ============================================================

UIDialog* ui_dialog_create_info(const char* title, const char* message)
{
    UIDialog* dialog = (UIDialog*)calloc(1, sizeof(UIDialog));
    if (dialog == NULL) return NULL;
    
    dialog->base.type = WIDGET_TYPE_CUSTOM + 1;  // Type custom pour dialog
    dialog->base.visible = true;
    dialog->base.enabled = true;
    dialog->base.needsRedraw = true;
    dialog->base.draw = dialog_draw;
    
    dialog->type = DIALOG_TYPE_INFO;
    dialog->icon = DIALOG_ICON_INFO;
    dialog->showCancel = false;
    dialog->dismissOnBack = true;
    dialog->dismissOnOutside = false;
    dialog->cornerRadius = 16;
    dialog->titleColor = ui_theme_get_active()->colors.textPrimary;
    dialog->messageColor = ui_theme_get_active()->colors.textSecondary;
    
    if (title) strncpy(dialog->title, title, 63);
    if (message) strncpy(dialog->message, message, 255);
    
    strncpy(dialog->buttonOkText, "OK", 15);
    
    return dialog;
}

UIDialog* ui_dialog_create_confirm(const char* title, const char* message)
{
    UIDialog* dialog = ui_dialog_create_info(title, message);
    if (dialog)
    {
        dialog->type = DIALOG_TYPE_CONFIRM;
        dialog->icon = DIALOG_ICON_QUESTION;
        dialog->showCancel = true;
        strncpy(dialog->buttonCancelText, "Annuler", 15);
    }
    return dialog;
}

UIDialog* ui_dialog_create_warning(const char* title, const char* message)
{
    UIDialog* dialog = ui_dialog_create_confirm(title, message);
    if (dialog)
    {
        dialog->type = DIALOG_TYPE_WARNING;
        dialog->icon = DIALOG_ICON_WARNING;
    }
    return dialog;
}

UIDialog* ui_dialog_create_error(const char* title, const char* message)
{
    UIDialog* dialog = ui_dialog_create_info(title, message);
    if (dialog)
    {
        dialog->type = DIALOG_TYPE_ERROR;
        dialog->icon = DIALOG_ICON_ERROR;
        dialog->titleColor = ui_theme_get_active()->colors.error;
    }
    return dialog;
}

UIDialog* ui_dialog_create_input(const char* title, const char* placeholder)
{
    UIDialog* dialog = ui_dialog_create_confirm(title, "");
    if (dialog)
    {
        dialog->type = DIALOG_TYPE_INPUT;
        dialog->icon = DIALOG_ICON_NONE;
        if (placeholder) strncpy(dialog->inputPlaceholder, placeholder, 63);
    }
    return dialog;
}

UIDialog* ui_dialog_create_progress(const char* title, const char* message)
{
    UIDialog* dialog = ui_dialog_create_info(title, message);
    if (dialog)
    {
        dialog->type = DIALOG_TYPE_PROGRESS;
        dialog->icon = DIALOG_ICON_NONE;
        dialog->showCancel = false;
        dialog->dismissOnBack = false;
        dialog->dismissOnOutside = false;
        dialog->progressValue = 0;
    }
    return dialog;
}

UIDialog* ui_dialog_create_custom(const char* title, UIRect rect)
{
    UIDialog* dialog = ui_dialog_create_info(title, "");
    if (dialog)
    {
        dialog->type = DIALOG_TYPE_CUSTOM;
        dialog->icon = DIALOG_ICON_NONE;
        dialog->base.rect = rect;
    }
    return dialog;
}

// ============================================================
// CONFIGURATION
// ============================================================

void ui_dialog_set_icon(UIDialog* dialog, DialogIcon icon)
{
    if (dialog) dialog->icon = icon;
}

void ui_dialog_set_button_text(UIDialog* dialog, const char* okText, const char* cancelText)
{
    if (dialog == NULL) return;
    if (okText) strncpy(dialog->buttonOkText, okText, 15);
    if (cancelText) strncpy(dialog->buttonCancelText, cancelText, 15);
}

void ui_dialog_set_input_secure(UIDialog* dialog, bool secure)
{
    if (dialog) dialog->inputSecure = secure;
}

void ui_dialog_set_progress(UIDialog* dialog, uint8_t value)
{
    if (dialog)
    {
        if (value > 100) value = 100;
        dialog->progressValue = value;
        dialog->base.needsRedraw = true;
    }
}

void ui_dialog_set_indeterminate(UIDialog* dialog, bool indeterminate)
{
    if (dialog) dialog->progressIndeterminate = indeterminate;
}

void ui_dialog_set_dismiss_policy(UIDialog* dialog, bool onBack, bool onOutside)
{
    if (dialog)
    {
        dialog->dismissOnBack = onBack;
        dialog->dismissOnOutside = onOutside;
    }
}

void ui_dialog_set_colors(UIDialog* dialog, uint16_t titleColor, uint16_t messageColor)
{
    if (dialog)
    {
        dialog->titleColor = titleColor;
        dialog->messageColor = messageColor;
    }
}

// ============================================================
// AFFICHAGE
// ============================================================

void ui_dialog_show(UIDialog* dialog)
{
    if (dialog == NULL) return;
    
    // Fermer le dialogue précédent
    if (active_dialog != NULL)
    {
        ui_dialog_dismiss(active_dialog, DIALOG_RESULT_CLOSE);
    }
    
    active_dialog = dialog;
    dialog->result = DIALOG_RESULT_NONE;
    btn_ok_pressed = false;
    btn_cancel_pressed = false;
    
    if (dialog->onShow) dialog->onShow(dialog);
    
    ui_request_redraw();
}

void ui_dialog_dismiss(UIDialog* dialog, DialogResult result)
{
    if (dialog == NULL || dialog != active_dialog) return;
    
    dialog->result = result;
    
    // Sauvegarder le texte saisi avant de fermer
    if (dialog->type == DIALOG_TYPE_INPUT)
    {
        // Le texte est déjà dans dialog->inputText
    }
    
    if (dialog->onResult) dialog->onResult(dialog, result);
    if (dialog->onDismiss) dialog->onDismiss(dialog);
    
    active_dialog = NULL;
    ui_request_redraw();
}

bool ui_dialog_is_showing(void)
{
    return (active_dialog != NULL);
}

UIDialog* ui_dialog_get_active(void)
{
    return active_dialog;
}

void ui_dialog_dismiss_all(void)
{
    while (active_dialog != NULL)
    {
        ui_dialog_dismiss(active_dialog, DIALOG_RESULT_CLOSE);
    }
}

// ============================================================
// FONCTIONS INTERNES
// ============================================================

/**
 * @brief Dessine une icône de dialogue
 */
static void draw_dialog_icon(uint16_t x, uint16_t y, DialogIcon icon)
{
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    uint16_t iconColor;
    
    switch (icon)
    {
        case DIALOG_ICON_INFO:    iconColor = colors->info;    break;
        case DIALOG_ICON_QUESTION:iconColor = colors->warning;  break;
        case DIALOG_ICON_WARNING: iconColor = colors->warning;  break;
        case DIALOG_ICON_ERROR:   iconColor = colors->error;    break;
        case DIALOG_ICON_SUCCESS: iconColor = colors->success;  break;
        default: return;
    }
    
    // Cercle coloré
    display_fill_circle(x + DIALOG_ICON_SIZE / 2, y + DIALOG_ICON_SIZE / 2,
                        DIALOG_ICON_SIZE / 2, iconColor);
    
    // Symbole
    display_set_text_color(colors->onPrimary);
    display_set_font(&font_8x16);
    
    const char* symbol = "?";
    switch (icon)
    {
        case DIALOG_ICON_INFO:    symbol = "i"; break;
        case DIALOG_ICON_QUESTION:symbol = "?"; break;
        case DIALOG_ICON_WARNING: symbol = "!"; break;
        case DIALOG_ICON_ERROR:   symbol = "X"; break;
        case DIALOG_ICON_SUCCESS: symbol = "OK"; break;
        default: break;
    }
    
    uint16_t symbolW = display_text_width(symbol, 2);
    display_draw_text(x + DIALOG_ICON_SIZE / 2 - symbolW / 2,
                     y + DIALOG_ICON_SIZE / 2 - 8, symbol, colors->onPrimary, 2);
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

// ============================================================
// DÉBOGAGE
// ============================================================

void ui_dialog_print_state(void)
{
    if (active_dialog)
    {
        printf("[DIALOG] Actif: %s (type=%d, result=%d)\n",
               active_dialog->title, active_dialog->type, active_dialog->result);
    }
    else
    {
        printf("[DIALOG] Aucun dialogue actif\n");
    }
}