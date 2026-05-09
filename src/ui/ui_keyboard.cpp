/**
 * @file ui_keyboard.cpp
 * @brief Implémentation du widget Clavier Virtuel (UIKeyboard)
 * 
 * Fonctionnalités :
 * - Dispositions AZERTY, QWERTY, QWERTZ, numérique
 * - Modes minuscules, majuscules, chiffres, symboles, téléphone
 * - Gestion tactile des touches
 * - Retour visuel (touche pressée)
 * - Callbacks de saisie
 * - Association avec un widget UITextBox
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_keyboard.h"
#include "ui_textbox.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// DÉFINITIONS DES DISPOSITIONS
// ============================================================

/**
 * @brief Layout AZERTY - Lettres minuscules
 */
static const char* LAYOUT_AZERTY_LOWER[KEYBOARD_ROWS] = {
    "azertyuiop",
    "qsdfghjklm",
    " wxcvbn ",
    "123,    ."
};

/**
 * @brief Layout AZERTY - Lettres majuscules
 */
static const char* LAYOUT_AZERTY_UPPER[KEYBOARD_ROWS] = {
    "AZERTYUIOP",
    "QSDFGHJKLM",
    " WXCVBN ",
    "123,    ."
};

/**
 * @brief Layout QWERTY - Lettres minuscules
 */
static const char* LAYOUT_QWERTY_LOWER[KEYBOARD_ROWS] = {
    "qwertyuiop",
    "asdfghjkl",
    " zxcvbnm",
    "123,    ."
};

/**
 * @brief Layout QWERTY - Lettres majuscules
 */
static const char* LAYOUT_QWERTY_UPPER[KEYBOARD_ROWS] = {
    "QWERTYUIOP",
    "ASDFGHJKL",
    " ZXCVBNM",
    "123,    ."
};

/**
 * @brief Layout Numérique
 */
static const char* LAYOUT_NUMBERS[KEYBOARD_ROWS] = {
    "1234567890",
    "-/:;()$&@\"",
    ".,?!'     ",
    "ABC,    ."
};

/**
 * @brief Layout Symboles
 */
static const char* LAYOUT_SYMBOLS[KEYBOARD_ROWS] = {
    "[]{}#%^*+=",
    "_\\|~<>€£¥",
    ".,?!'     ",
    "123,    ."
};

/**
 * @brief Layout Téléphone (3 colonnes)
 */
static const char* LAYOUT_PHONE[KEYBOARD_ROWS] = {
    "123",
    "456",
    "789",
    "*0#"
};

// ============================================================
// FONCTIONS DE DESSIN
// ============================================================

/**
 * @brief Calcule la position de chaque touche
 */
static void keyboard_layout_keys(UIKeyboard* keyboard)
{
    if (keyboard == NULL) return;
    
    UIRect* r = &keyboard->base.rect;
    uint16_t spacing = KEYBOARD_KEY_SPACING;
    uint16_t keyH = keyboard->keyHeight;
    uint16_t totalWidth = r->width;
    
    for (uint8_t row = 0; row < KEYBOARD_ROWS; row++)
    {
        uint8_t keyCount = keyboard->keysPerRow[row];
        uint16_t totalSpacing = (keyCount + 1) * spacing;
        uint16_t availableWidth = totalWidth - totalSpacing;
        
        // Certaines touches ont une largeur différente
        uint8_t normalKeys = keyCount;
        uint8_t wideKeys = 0;
        
        // Identifier les touches larges (ESPACE, SHIFT, etc.)
        for (uint8_t k = 0; k < keyCount; k++)
        {
            if (keyboard->keys[row][k].type == KEY_TYPE_SPACE ||
                keyboard->keys[row][k].type == KEY_TYPE_SHIFT)
            {
                wideKeys++;
                normalKeys--;
            }
        }
        
        uint16_t normalKeyWidth = (normalKeys > 0) ? 
            (availableWidth - wideKeys * 60) / normalKeys : availableWidth / keyCount;
        uint16_t wideKeyWidth = normalKeyWidth * 2 + spacing;
        
        uint16_t x = r->x + spacing;
        uint16_t y = r->y + row * (keyH + spacing);
        
        for (uint8_t k = 0; k < keyCount; k++)
        {
            KeyboardKey* key = &keyboard->keys[row][k];
            
            if (key->type == KEY_TYPE_SPACE)
            {
                key->w = totalWidth - 4 * spacing - 2 * normalKeyWidth;
                key->x = r->x + 2 * spacing + normalKeyWidth;
            }
            else if (key->type == KEY_TYPE_SHIFT || key->type == KEY_TYPE_BACKSPACE)
            {
                key->w = wideKeyWidth;
            }
            else if (key->type == KEY_TYPE_MODE || key->type == KEY_TYPE_SYMBOLS || key->type == KEY_TYPE_ENTER)
            {
                key->w = normalKeyWidth * 1.5;
            }
            else
            {
                key->w = normalKeyWidth;
            }
            
            key->x = x;
            key->y = y;
            key->h = keyH;
            
            x += key->w + spacing;
        }
    }
    
    keyboard->keyboardWidth = totalWidth;
    keyboard->keyboardHeight = KEYBOARD_ROWS * (keyH + spacing) + spacing;
}

/**
 * @brief Charge la disposition des touches selon le mode
 */
static void keyboard_load_layout(UIKeyboard* keyboard)
{
    if (keyboard == NULL) return;
    
    const char** layout = NULL;
    
    // Sélectionner la disposition selon le mode et le layout
    switch (keyboard->mode)
    {
        case KEYBOARD_MODE_LOWERCASE:
            layout = (keyboard->layout == KEYBOARD_LAYOUT_AZERTY) ? 
                      LAYOUT_AZERTY_LOWER : LAYOUT_QWERTY_LOWER;
            break;
            
        case KEYBOARD_MODE_UPPERCASE:
            layout = (keyboard->layout == KEYBOARD_LAYOUT_AZERTY) ? 
                      LAYOUT_AZERTY_UPPER : LAYOUT_QWERTY_UPPER;
            break;
            
        case KEYBOARD_MODE_NUMBERS:
            layout = LAYOUT_NUMBERS;
            break;
            
        case KEYBOARD_MODE_SYMBOLS:
            layout = LAYOUT_SYMBOLS;
            break;
            
        case KEYBOARD_MODE_PHONE:
            layout = LAYOUT_PHONE;
            break;
    }
    
    if (layout == NULL) return;
    
    // Effacer les touches existantes
    memset(keyboard->keys, 0, sizeof(keyboard->keys));
    memset(keyboard->keysPerRow, 0, sizeof(keyboard->keysPerRow));
    
    // Charger les touches
    for (uint8_t row = 0; row < KEYBOARD_ROWS; row++)
    {
        const char* rowStr = layout[row];
        uint8_t len = strlen(rowStr);
        keyboard->keysPerRow[row] = len;
        
        for (uint8_t col = 0; col < len; col++)
        {
            KeyboardKey* key = &keyboard->keys[row][col];
            char c = rowStr[col];
            
            if (c == ' ')
            {
                key->type = KEY_TYPE_SPACE;
                strcpy(key->label, "ESPACE");
                key->primaryChar = ' ';
            }
            else if (c == '1' && row == 3 && col == 0)
            {
                key->type = KEY_TYPE_MODE;
                strcpy(key->label, "123");
                key->primaryChar = 0;
            }
            else if (c == 'A' && row == 3 && col == 0)
            {
                key->type = KEY_TYPE_MODE;
                strcpy(key->label, "ABC");
                key->primaryChar = 0;
            }
            else if (c == ',' && row == 3 && col == 1)
            {
                key->type = KEY_TYPE_COMMA;
                key->label[0] = c; key->label[1] = '\0';
                key->primaryChar = c;
            }
            else if (c == '.' && row == 3 && col == 3)
            {
                key->type = KEY_TYPE_DOT;
                key->label[0] = c; key->label[1] = '\0';
                key->primaryChar = c;
            }
            else
            {
                key->type = KEY_TYPE_NORMAL;
                key->label[0] = c; key->label[1] = '\0';
                key->primaryChar = c;
                key->secondaryChar = (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : 
                                     (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
            }
        }
        
        // Ajouter les touches spéciales de fin de ligne
        if (row == 1 && keyboard->mode < KEYBOARD_MODE_NUMBERS)
        {
            // Ajouter ENTER à la fin de la 2ème ligne
            uint8_t col = keyboard->keysPerRow[row];
            keyboard->keys[row][col].type = KEY_TYPE_ENTER;
            strcpy(keyboard->keys[row][col].label, "OK");
            keyboard->keys[row][col].primaryChar = '\n';
            keyboard->keysPerRow[row]++;
        }
        else if (row == 2 && keyboard->mode < KEYBOARD_MODE_NUMBERS)
        {
            // Ajouter SHIFT au début et BACKSPACE à la fin
            // SHIFT est déjà le premier caractère (espace avant)
            uint8_t col = keyboard->keysPerRow[row];
            keyboard->keys[row][col].type = KEY_TYPE_BACKSPACE;
            strcpy(keyboard->keys[row][col].label, "⌫");
            keyboard->keys[row][col].primaryChar = 0;
            keyboard->keysPerRow[row]++;
        }
    }
    
    // Recalculer les positions
    keyboard_layout_keys(keyboard);
}

/**
 * @brief Dessine le clavier complet
 */
static void keyboard_draw(UIWidget* widget)
{
    UIKeyboard* keyboard = (UIKeyboard*)widget;
    if (keyboard == NULL) return;
    
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    // --- Fond du clavier ---
    display_fill_rect(widget->rect.x, widget->rect.y,
                     widget->rect.x + widget->rect.width - 1,
                     widget->rect.y + widget->rect.height - 1,
                     colors->surface);
    
    // --- Ligne de séparation ---
    display_draw_hline(widget->rect.x, widget->rect.y,
                       widget->rect.x + widget->rect.width - 1, colors->border);
    
    // --- Dessiner chaque touche ---
    for (uint8_t row = 0; row < KEYBOARD_ROWS; row++)
    {
        for (uint8_t col = 0; col < keyboard->keysPerRow[row]; col++)
        {
            KeyboardKey* key = &keyboard->keys[row][col];
            
            // Couleur de fond
            uint16_t bgColor;
            if (key->pressed)
            {
                bgColor = colors->primary;
            }
            else if (key->type == KEY_TYPE_NORMAL || key->type == KEY_TYPE_SPACE ||
                     key->type == KEY_TYPE_DOT || key->type == KEY_TYPE_COMMA)
            {
                bgColor = keyboard->keyBgColor;
            }
            else
            {
                bgColor = keyboard->specialKeyBgColor;
            }
            
            // Fond de la touche
            display_fill_round_rect(key->x, key->y, key->x + key->w - 1, key->y + key->h - 1,
                                     keyboard->keyCornerRadius, bgColor);
            
            // Ombre légère
            display_fill_round_rect(key->x, key->y + 1, key->x + key->w - 1, key->y + key->h,
                                     keyboard->keyCornerRadius, darken_color(bgColor, 10));
            display_fill_round_rect(key->x, key->y, key->x + key->w - 1, key->y + key->h - 1,
                                     keyboard->keyCornerRadius, bgColor);
            
            // Texte de la touche
            uint16_t textColor = key->pressed ? colors->onPrimary : keyboard->keyTextColor;
            display_set_font(&font_5x7);
            display_set_text_color(textColor);
            
            uint16_t textW = display_text_width(key->label, 1);
            uint16_t textH = display_text_height(1);
            uint16_t textX = key->x + (key->w - textW) / 2;
            uint16_t textY = key->y + (key->h - textH) / 2;
            
            display_draw_text(textX, textY, key->label, textColor, 1);
        }
    }
}

// ============================================================
// FONCTIONS TACTILES
// ============================================================

/**
 * @brief Trouve la touche à une position donnée
 */
static KeyboardKey* keyboard_find_key(UIKeyboard* keyboard, uint16_t x, uint16_t y)
{
    if (keyboard == NULL) return NULL;
    
    for (uint8_t row = 0; row < KEYBOARD_ROWS; row++)
    {
        for (uint8_t col = 0; col < keyboard->keysPerRow[row]; col++)
        {
            KeyboardKey* key = &keyboard->keys[row][col];
            
            if (x >= key->x && x < key->x + key->w &&
                y >= key->y && y < key->y + key->h)
            {
                return key;
            }
        }
    }
    return NULL;
}

/**
 * @brief Gestion tactile du clavier
 */
static void keyboard_touch(UIWidget* widget, uint16_t x, uint16_t y, TouchEvent event)
{
    UIKeyboard* keyboard = (UIKeyboard*)widget;
    if (keyboard == NULL) return;
    
    switch (event)
    {
        case TOUCH_EVENT_PRESS:
        {
            KeyboardKey* key = keyboard_find_key(keyboard, x, y);
            if (key)
            {
                key->pressed = true;
                keyboard->pressedKey = key;
                widget->needsRedraw = true;
            }
            break;
        }
        
        case TOUCH_EVENT_MOVE:
        {
            KeyboardKey* key = keyboard_find_key(keyboard, x, y);
            
            // Relâcher l'ancienne touche si on en sort
            if (keyboard->pressedKey && keyboard->pressedKey != key)
            {
                keyboard->pressedKey->pressed = false;
                keyboard->pressedKey = NULL;
                widget->needsRedraw = true;
            }
            
            // Presser la nouvelle touche
            if (key && key != keyboard->pressedKey)
            {
                key->pressed = true;
                keyboard->pressedKey = key;
                widget->needsRedraw = true;
            }
            break;
        }
        
        case TOUCH_EVENT_RELEASE:
        {
            if (keyboard->pressedKey)
            {
                KeyboardKey* key = keyboard->pressedKey;
                key->pressed = false;
                keyboard->pressedKey = NULL;
                widget->needsRedraw = true;
                
                // Traiter l'action de la touche
                switch (key->type)
                {
                    case KEY_TYPE_NORMAL:
                    case KEY_TYPE_COMMA:
                    case KEY_TYPE_DOT:
                        if (key->primaryChar != 0)
                        {
                            if (keyboard->onKeyPress)
                                keyboard->onKeyPress(keyboard, key->primaryChar, key->type);
                            
                            // Insérer dans le widget cible
                            if (keyboard->targetWidget)
                                ui_textbox_insert_char((UITextBox*)keyboard->targetWidget, key->primaryChar);
                        }
                        break;
                        
                    case KEY_TYPE_SHIFT:
                        ui_keyboard_toggle_shift(keyboard);
                        break;
                        
                    case KEY_TYPE_BACKSPACE:
                        if (keyboard->onBackspace)
                            keyboard->onBackspace(keyboard);
                        
                        if (keyboard->targetWidget)
                            ui_textbox_delete_char((UITextBox*)keyboard->targetWidget);
                        break;
                        
                    case KEY_TYPE_SPACE:
                        if (keyboard->onKeyPress)
                            keyboard->onKeyPress(keyboard, ' ', KEY_TYPE_SPACE);
                        
                        if (keyboard->targetWidget)
                            ui_textbox_insert_char((UITextBox*)keyboard->targetWidget, ' ');
                        break;
                        
                    case KEY_TYPE_ENTER:
                        if (keyboard->onEnter)
                            keyboard->onEnter(keyboard);
                        break;
                        
                    case KEY_TYPE_MODE:
                        // Basculer entre lettres et chiffres
                        if (keyboard->mode < KEYBOARD_MODE_NUMBERS)
                            ui_keyboard_set_mode(keyboard, KEYBOARD_MODE_NUMBERS);
                        else
                            ui_keyboard_set_mode(keyboard, KEYBOARD_MODE_LOWERCASE);
                        break;
                        
                    case KEY_TYPE_SYMBOLS:
                        if (keyboard->mode == KEYBOARD_MODE_SYMBOLS)
                            ui_keyboard_set_mode(keyboard, KEYBOARD_MODE_LOWERCASE);
                        else
                            ui_keyboard_set_mode(keyboard, KEYBOARD_MODE_SYMBOLS);
                        break;
                        
                    default:
                        break;
                }
            }
            break;
        }
        
        default:
            break;
    }
}

// ============================================================
// CRÉATION
// ============================================================

UIKeyboard* ui_keyboard_create(const char* name, UIRect rect)
{
    UIKeyboard* keyboard = (UIKeyboard*)calloc(1, sizeof(UIKeyboard));
    if (keyboard == NULL) return NULL;
    
    // --- Initialiser le widget de base ---
    if (name) strncpy(keyboard->base.name, name, 31);
    keyboard->base.rect = rect;
    keyboard->base.visible = true;
    keyboard->base.enabled = true;
    keyboard->base.canFocus = false;
    keyboard->base.state = WIDGET_STATE_NORMAL;
    
    // --- Assigner les fonctions virtuelles ---
    keyboard->base.draw = keyboard_draw;
    keyboard->base.onTouch = keyboard_touch;
    
    // --- Valeurs par défaut ---
    keyboard->mode = KEYBOARD_MODE_LOWERCASE;
    keyboard->layout = KEYBOARD_LAYOUT_AZERTY;
    keyboard->keyHeight = KEYBOARD_KEY_HEIGHT;
    keyboard->keyCornerRadius = 6;
    keyboard->shiftLock = false;
    keyboard->pressedKey = NULL;
    keyboard->targetWidget = NULL;
    
    // Couleurs par défaut
    keyboard->keyBgColor = 0xFFFF;      // Blanc
    keyboard->keyTextColor = 0x0000;    // Noir
    keyboard->specialKeyBgColor = 0xC618; // Gris
    
    // Charger la disposition
    keyboard_load_layout(keyboard);
    
    // Ajuster la hauteur
    keyboard->base.rect.height = keyboard->keyboardHeight;
    
    return keyboard;
}

UIKeyboard* ui_keyboard_create_full_width(const char* name, uint16_t y)
{
    UIRect rect = {0, y, DISPLAY_WIDTH, KEYBOARD_ROWS * (KEYBOARD_KEY_HEIGHT + KEYBOARD_KEY_SPACING)};
    return ui_keyboard_create(name, rect);
}

// ============================================================
// CONFIGURATION
// ============================================================

void ui_keyboard_set_mode(UIKeyboard* keyboard, KeyboardMode mode)
{
    if (keyboard == NULL) return;
    
    KeyboardMode oldMode = keyboard->mode;
    keyboard->mode = mode;
    
    // Recharger la disposition
    keyboard_load_layout(keyboard);
    keyboard->base.needsRedraw = true;
    
    if (keyboard->onModeChange && oldMode != mode)
    {
        keyboard->onModeChange(keyboard, mode);
    }
}

void ui_keyboard_set_layout(UIKeyboard* keyboard, KeyboardLayout layout)
{
    if (keyboard == NULL) return;
    keyboard->layout = layout;
    keyboard_load_layout(keyboard);
    keyboard->base.needsRedraw = true;
}

void ui_keyboard_toggle_shift(UIKeyboard* keyboard)
{
    if (keyboard == NULL) return;
    
    if (keyboard->mode == KEYBOARD_MODE_LOWERCASE)
    {
        ui_keyboard_set_mode(keyboard, KEYBOARD_MODE_UPPERCASE);
    }
    else if (keyboard->mode == KEYBOARD_MODE_UPPERCASE)
    {
        ui_keyboard_set_mode(keyboard, KEYBOARD_MODE_LOWERCASE);
    }
}

void ui_keyboard_set_target(UIKeyboard* keyboard, void* targetWidget)
{
    if (keyboard == NULL) return;
    keyboard->targetWidget = targetWidget;
}

void ui_keyboard_set_colors(UIKeyboard* keyboard, uint16_t keyBg, uint16_t keyText, uint16_t specialBg)
{
    if (keyboard == NULL) return;
    keyboard->keyBgColor = keyBg;
    keyboard->keyTextColor = keyText;
    keyboard->specialKeyBgColor = specialBg;
    keyboard->base.needsRedraw = true;
}

// ============================================================
// ÉTAT
// ============================================================

KeyboardMode ui_keyboard_get_mode(UIKeyboard* keyboard)
{
    return keyboard ? keyboard->mode : KEYBOARD_MODE_LOWERCASE;
}

bool ui_keyboard_is_shift_active(UIKeyboard* keyboard)
{
    return keyboard ? (keyboard->mode == KEYBOARD_MODE_UPPERCASE) : false;
}

void ui_keyboard_reset(UIKeyboard* keyboard)
{
    if (keyboard == NULL) return;
    ui_keyboard_set_mode(keyboard, KEYBOARD_MODE_LOWERCASE);
    keyboard->pressedKey = NULL;
    keyboard->base.needsRedraw = true;
}

// ============================================================
// FONCTIONS UTILITAIRES
// ============================================================

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