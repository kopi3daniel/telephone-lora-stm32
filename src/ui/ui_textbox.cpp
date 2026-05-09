/**
 * @file ui_textbox.cpp
 * @brief Implémentation du widget Zone de Texte (TextBox)
 * 
 * Ce fichier est optionnel. L'implémentation de UITextBox
 * se trouve déjà dans ui_widgets.cpp (section 3).
 * 
 * Fonctionnalités :
 * - Rendu graphique avec styles (normal, outlined, filled, underlined)
 * - Gestion du curseur clignotant
 * - Saisie et édition de texte
 * - Mode mot de passe
 * - Validation de saisie
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_textbox.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// ============================================================
// FONCTIONS DE DESSIN
// ============================================================

/**
 * @brief Dessine la zone de texte selon son style
 */
static void textbox_draw(UIWidget* widget)
{
    UITextBox* textbox = (UITextBox*)widget;
    if (textbox == NULL) return;
    
    TextBoxAppearance* app = &textbox->appearance;
    UIRect* r = &widget->rect;
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    // --- Déterminer les couleurs selon l'état ---
    uint16_t bgColor = app->bgColor;
    uint16_t borderColor = app->borderColor;
    
    if (!widget->enabled)
    {
        bgColor = colors->disabled;
        borderColor = colors->disabled;
    }
    else if (widget->hasFocus)
    {
        borderColor = app->cursorColor;  // Couleur d'accentuation quand focus
    }
    
    // --- Dessiner le fond selon le style ---
    switch (app->style)
    {
        case TEXTBOX_STYLE_FILLED:
            // Fond plein
            display_fill_round_rect(r->x, r->y, r->x + r->width - 1, 
                                     r->y + r->height - 1, app->cornerRadius, bgColor);
            break;
            
        case TEXTBOX_STYLE_OUTLINED:
            // Fond transparent avec bordure
            if (app->cornerRadius > 0)
            {
                display_draw_round_rect(r->x, r->y, r->x + r->width - 1,
                                         r->y + r->height - 1, app->cornerRadius, borderColor);
            }
            else
            {
                display_draw_rect(r->x, r->y, r->x + r->width - 1,
                                 r->y + r->height - 1, borderColor);
            }
            break;
            
        case TEXTBOX_STYLE_UNDERLINED:
            // Ligne en bas uniquement
            display_draw_hline(r->x, r->y + r->height - 2, r->x + r->width - 1, borderColor);
            break;
            
        case TEXTBOX_STYLE_NORMAL:
        default:
            // Fond + bordure légère
            display_fill_round_rect(r->x, r->y, r->x + r->width - 1,
                                     r->y + r->height - 1, app->cornerRadius, bgColor);
            if (app->borderWidth > 0)
            {
                display_draw_round_rect(r->x, r->y, r->x + r->width - 1,
                                         r->y + r->height - 1, app->cornerRadius, borderColor);
            }
            break;
    }
    
    // --- Préparer le texte à afficher ---
    const char* displayText;
    char secureText[256];
    uint16_t textColor;
    
    if (strlen(textbox->text) > 0)
    {
        // Texte saisi
        if (textbox->secure)
        {
            // Mode mot de passe : afficher des étoiles ou des points
            uint16_t len = strlen(textbox->text);
            for (uint16_t i = 0; i < len && i < 255; i++)
            {
                secureText[i] = '*';  // Ou utiliser '•' (U+2022)
            }
            secureText[len] = '\0';
            displayText = secureText;
        }
        else
        {
            displayText = textbox->text;
        }
        textColor = app->textColor;
    }
    else
    {
        // Placeholder
        displayText = textbox->placeholder;
        textColor = colors->textHint;
    }
    
    // --- Afficher le texte ---
    if (strlen(displayText) > 0)
    {
        display_set_font(app->font);
        display_set_text_color(textColor);
        
        uint16_t textX = r->x + app->padding.left;
        uint16_t textY = r->y + (r->height - display_text_height(app->fontSize)) / 2;
        
        display_draw_text(textX, textY, displayText, textColor, app->fontSize);
    }
    
    // --- Dessiner le curseur clignotant (si focus et éditable) ---
    if (textbox->editable && widget->hasFocus && textbox->cursorVisible)
    {
        uint16_t textBeforeCursor[256];
        strncpy((char*)textBeforeCursor, textbox->text, textbox->cursorPos);
        ((char*)textBeforeCursor)[textbox->cursorPos] = '\0';
        
        uint16_t cursorX = r->x + app->padding.left + 
                          display_text_width((char*)textBeforeCursor, app->fontSize);
        uint16_t cursorY = r->y + app->padding.top;
        uint16_t cursorH = r->height - app->padding.top - app->padding.bottom;
        
        if (cursorH < 8) cursorH = 8;
        
        display_fill_rect(cursorX, cursorY, cursorX + 2, cursorY + cursorH, app->cursorColor);
    }
}

// ============================================================
// FONCTIONS D'INTERACTION
// ============================================================

/**
 * @brief Gestion des touches clavier
 */
static void textbox_key(UIWidget* widget, KeyCode key, KeyEvent event)
{
    UITextBox* textbox = (UITextBox*)widget;
    if (textbox == NULL || !textbox->editable) return;
    
    if (event != KEY_EVENT_PRESS && event != KEY_EVENT_REPEAT) return;
    
    switch (key)
    {
        // --- Navigation ---
        case KEY_LEFT:
            ui_textbox_cursor_left(textbox);
            break;
            
        case KEY_RIGHT:
            ui_textbox_cursor_right(textbox);
            break;
            
        case KEY_UP:
            ui_textbox_cursor_home(textbox);
            break;
            
        case KEY_DOWN:
            ui_textbox_cursor_end(textbox);
            break;
            
        // --- Édition ---
        case KEY_BACK:
            ui_textbox_delete_char(textbox);
            break;
            
        case KEY_OK:
            if (textbox->onSubmit)
            {
                textbox->onSubmit(textbox);
            }
            break;
            
        // --- Touches de saisie (gérées par le keypad_manager) ---
        default:
            break;
    }
}

/**
 * @brief Gestion du focus
 */
static void textbox_focus(UIWidget* widget, bool focused)
{
    UITextBox* textbox = (UITextBox*)widget;
    if (textbox == NULL) return;
    
    textbox->cursorVisible = focused;
    textbox->cursorBlinkTime = HAL_GetTick();
    
    if (textbox->onFocus)
    {
        textbox->onFocus(textbox, focused);
    }
    
    widget->needsRedraw = true;
}

/**
 * @brief Mise à jour périodique (clignotement du curseur)
 */
static void textbox_update(UIWidget* widget)
{
    UITextBox* textbox = (UITextBox*)widget;
    if (textbox == NULL || !widget->hasFocus) return;
    
    uint32_t now = HAL_GetTick();
    
    if ((now - textbox->cursorBlinkTime) >= textbox->cursorBlinkIntervalMs)
    {
        textbox->cursorVisible = !textbox->cursorVisible;
        textbox->cursorBlinkTime = now;
        widget->needsRedraw = true;
    }
}

// ============================================================
// CRÉATION
// ============================================================

UITextBox* ui_textbox_create(const char* name, UIRect rect)
{
    UITextBox* textbox = (UITextBox*)calloc(1, sizeof(UITextBox));
    if (textbox == NULL) return NULL;
    
    // --- Initialiser le widget de base ---
    textbox->base.type = WIDGET_TYPE_TEXTBOX;
    if (name) strncpy(textbox->base.name, name, 31);
    textbox->base.rect = rect;
    textbox->base.visible = true;
    textbox->base.enabled = true;
    textbox->base.canFocus = true;
    textbox->base.state = WIDGET_STATE_NORMAL;
    
    // --- Assigner les fonctions virtuelles ---
    textbox->base.draw = textbox_draw;
    textbox->base.onKey = textbox_key;
    textbox->base.onFocus = textbox_focus;
    textbox->base.onUpdate = textbox_update;
    textbox->base.onTouch = NULL;
    
    // --- Valeurs par défaut ---
    textbox->maxLength = 255;
    textbox->minLength = 0;
    textbox->cursorPos = 0;
    textbox->editable = true;
    textbox->cursorVisible = false;
    textbox->cursorBlinkIntervalMs = 500;
    textbox->secure = false;
    textbox->allowedChars = NULL;
    textbox->validator = NULL;
    
    // --- Style par défaut ---
    ui_textbox_set_style(textbox, TEXTBOX_STYLE_NORMAL);
    
    return textbox;
}

// ============================================================
// CONFIGURATION DU STYLE
// ============================================================

void ui_textbox_set_style(UITextBox* textbox, TextBoxStyle style)
{
    if (textbox == NULL) return;
    textbox->appearance = ui_style_get_textbox(style);
    textbox->base.needsRedraw = true;
}

// ============================================================
// GESTION DU TEXTE
// ============================================================

void ui_textbox_set_text(UITextBox* textbox, const char* text)
{
    if (textbox == NULL) return;
    
    if (text)
    {
        strncpy(textbox->text, text, textbox->maxLength);
        textbox->text[textbox->maxLength] = '\0';
    }
    else
    {
        textbox->text[0] = '\0';
    }
    
    textbox->cursorPos = strlen(textbox->text);
    textbox->base.needsRedraw = true;
    
    if (textbox->onTextChanged) textbox->onTextChanged(textbox);
}

const char* ui_textbox_get_text(UITextBox* textbox)
{
    return (textbox != NULL) ? textbox->text : "";
}

uint16_t ui_textbox_get_length(UITextBox* textbox)
{
    return (textbox != NULL) ? strlen(textbox->text) : 0;
}

void ui_textbox_set_placeholder(UITextBox* textbox, const char* placeholder)
{
    if (textbox == NULL) return;
    if (placeholder) strncpy(textbox->placeholder, placeholder, 63);
    else textbox->placeholder[0] = '\0';
    textbox->base.needsRedraw = true;
}

void ui_textbox_clear(UITextBox* textbox)
{
    if (textbox == NULL) return;
    memset(textbox->text, 0, sizeof(textbox->text));
    textbox->cursorPos = 0;
    textbox->base.needsRedraw = true;
    
    if (textbox->onTextChanged) textbox->onTextChanged(textbox);
}

// ============================================================
// FONCTIONS D'ÉDITION
// ============================================================

bool ui_textbox_insert_char(UITextBox* textbox, char c)
{
    if (textbox == NULL || !textbox->editable) return false;
    
    uint16_t len = strlen(textbox->text);
    
    // Vérifier la longueur maximale
    if (len >= textbox->maxLength - 1) return false;
    
    // Vérifier le validateur
    if (textbox->validator && !textbox->validator(textbox, c)) return false;
    
    // Vérifier les caractères autorisés
    if (textbox->allowedChars && !strchr(textbox->allowedChars, c)) return false;
    
    // Décaler les caractères après le curseur
    if (textbox->cursorPos < len)
    {
        memmove(&textbox->text[textbox->cursorPos + 1],
                &textbox->text[textbox->cursorPos],
                len - textbox->cursorPos + 1);
    }
    
    textbox->text[textbox->cursorPos] = c;
    textbox->cursorPos++;
    textbox->base.needsRedraw = true;
    
    // Réinitialiser le clignotement du curseur
    textbox->cursorVisible = true;
    textbox->cursorBlinkTime = HAL_GetTick();
    
    if (textbox->onTextChanged) textbox->onTextChanged(textbox);
    
    return true;
}

void ui_textbox_delete_char(UITextBox* textbox)
{
    if (textbox == NULL || textbox->cursorPos == 0) return;
    
    uint16_t len = strlen(textbox->text);
    
    // Décaler les caractères
    memmove(&textbox->text[textbox->cursorPos - 1],
            &textbox->text[textbox->cursorPos],
            len - textbox->cursorPos + 1);
    
    textbox->cursorPos--;
    textbox->base.needsRedraw = true;
    
    textbox->cursorVisible = true;
    textbox->cursorBlinkTime = HAL_GetTick();
    
    if (textbox->onTextChanged) textbox->onTextChanged(textbox);
}

void ui_textbox_delete_forward(UITextBox* textbox)
{
    if (textbox == NULL) return;
    
    uint16_t len = strlen(textbox->text);
    if (textbox->cursorPos >= len) return;
    
    memmove(&textbox->text[textbox->cursorPos],
            &textbox->text[textbox->cursorPos + 1],
            len - textbox->cursorPos);
    
    textbox->base.needsRedraw = true;
    textbox->cursorVisible = true;
    textbox->cursorBlinkTime = HAL_GetTick();
    
    if (textbox->onTextChanged) textbox->onTextChanged(textbox);
}

// ============================================================
// DÉPLACEMENT DU CURSEUR
// ============================================================

void ui_textbox_cursor_left(UITextBox* textbox)
{
    if (textbox == NULL || textbox->cursorPos == 0) return;
    textbox->cursorPos--;
    textbox->cursorVisible = true;
    textbox->cursorBlinkTime = HAL_GetTick();
    textbox->base.needsRedraw = true;
}

void ui_textbox_cursor_right(UITextBox* textbox)
{
    if (textbox == NULL) return;
    uint16_t len = strlen(textbox->text);
    if (textbox->cursorPos >= len) return;
    textbox->cursorPos++;
    textbox->cursorVisible = true;
    textbox->cursorBlinkTime = HAL_GetTick();
    textbox->base.needsRedraw = true;
}

void ui_textbox_cursor_home(UITextBox* textbox)
{
    if (textbox == NULL) return;
    textbox->cursorPos = 0;
    textbox->cursorVisible = true;
    textbox->cursorBlinkTime = HAL_GetTick();
    textbox->base.needsRedraw = true;
}

void ui_textbox_cursor_end(UITextBox* textbox)
{
    if (textbox == NULL) return;
    textbox->cursorPos = strlen(textbox->text);
    textbox->cursorVisible = true;
    textbox->cursorBlinkTime = HAL_GetTick();
    textbox->base.needsRedraw = true;
}

// ============================================================
// COMPORTEMENT
// ============================================================

void ui_textbox_set_secure(UITextBox* textbox, bool secure)
{
    if (textbox == NULL) return;
    textbox->secure = secure;
    textbox->base.needsRedraw = true;
}

void ui_textbox_set_editable(UITextBox* textbox, bool editable)
{
    if (textbox == NULL) return;
    textbox->editable = editable;
    if (!editable) textbox->cursorVisible = false;
    textbox->base.needsRedraw = true;
}

void ui_textbox_set_max_length(UITextBox* textbox, uint16_t maxLength)
{
    if (textbox == NULL) return;
    textbox->maxLength = (maxLength < 255) ? maxLength : 255;
}

void ui_textbox_set_min_length(UITextBox* textbox, uint16_t minLength)
{
    if (textbox == NULL) return;
    textbox->minLength = minLength;
}

bool ui_textbox_is_valid(UITextBox* textbox)
{
    if (textbox == NULL) return false;
    uint16_t len = strlen(textbox->text);
    return (len >= textbox->minLength && len <= textbox->maxLength);
}

// ============================================================
// VALIDATION
// ============================================================

void ui_textbox_set_validator(UITextBox* textbox, bool (*validator)(UITextBox*, char))
{
    if (textbox == NULL) return;
    textbox->validator = validator;
}

void ui_textbox_set_numeric_only(UITextBox* textbox)
{
    if (textbox == NULL) return;
    textbox->allowedChars = "0123456789";
    textbox->validator = ui_textbox_validator_numeric;
}

void ui_textbox_set_alpha_only(UITextBox* textbox)
{
    if (textbox == NULL) return;
    textbox->allowedChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    textbox->validator = ui_textbox_validator_alpha;
}

void ui_textbox_set_alphanumeric_only(UITextBox* textbox)
{
    if (textbox == NULL) return;
    textbox->allowedChars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    textbox->validator = ui_textbox_validator_alphanumeric;
}

void ui_textbox_set_phone_only(UITextBox* textbox)
{
    if (textbox == NULL) return;
    textbox->allowedChars = "0123456789+*#";
    textbox->validator = ui_textbox_validator_phone;
}

// ============================================================
// VALIDATEURS PRÉDÉFINIS
// ============================================================

bool ui_textbox_validator_numeric(UITextBox* textbox, char c)
{
    (void)textbox;
    return (c >= '0' && c <= '9');
}

bool ui_textbox_validator_alpha(UITextBox* textbox, char c)
{
    (void)textbox;
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

bool ui_textbox_validator_alphanumeric(UITextBox* textbox, char c)
{
    (void)textbox;
    return ((c >= '0' && c <= '9') || 
            (c >= 'a' && c <= 'z') || 
            (c >= 'A' && c <= 'Z'));
}

bool ui_textbox_validator_phone(UITextBox* textbox, char c)
{
    (void)textbox;
    return ((c >= '0' && c <= '9') || c == '+' || c == '*' || c == '#' || c == ' ');
}

bool ui_textbox_validator_email(UITextBox* textbox, char c)
{
    (void)textbox;
    return ((c >= 'a' && c <= 'z') || 
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '@' || c == '.' || c == '_' || c == '-' || c == '+');
}

bool ui_textbox_validator_ip_address(UITextBox* textbox, char c)
{
    (void)textbox;
    return ((c >= '0' && c <= '9') || c == '.');
}

// ============================================================
// CALLBACKS
// ============================================================

void ui_textbox_set_on_text_changed(UITextBox* textbox, void (*callback)(UITextBox*))
{
    if (textbox == NULL) return;
    textbox->onTextChanged = callback;
}

void ui_textbox_set_on_submit(UITextBox* textbox, void (*callback)(UITextBox*))
{
    if (textbox == NULL) return;
    textbox->onSubmit = callback;
}

void ui_textbox_set_on_focus(UITextBox* textbox, void (*callback)(UITextBox*, bool))
{
    if (textbox == NULL) return;
    textbox->onFocus = callback;
}