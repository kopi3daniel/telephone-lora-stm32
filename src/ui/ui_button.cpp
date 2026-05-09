/**
 * @file ui_button.cpp
 * @brief Implémentation du widget Bouton
 * 
 * Ce fichier est optionnel. L'implémentation complète de UIButton
 * se trouve déjà dans ui_widgets.cpp (section 1).
 * 
 * Il est fourni ici comme référence pour un accès rapide
 * au widget bouton sans inclure tous les autres widgets.
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_button.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdlib.h>

// ============================================================
// FONCTIONS DE DESSIN
// ============================================================

/**
 * @brief Fonction de dessin du bouton
 * 
 * Dessine le bouton selon son état :
 * - Normal : couleur de fond normale
 * - Pressé : couleur légèrement assombrie
 * - Désactivé : couleur grisée
 */
static void button_draw(UIWidget* widget)
{
    UIButton* button = (UIButton*)widget;
    if (button == NULL) return;
    
    ButtonAppearance* app = &button->appearance;
    UIRect* r = &widget->rect;
    
    // Déterminer la couleur de fond selon l'état
    uint16_t bgColor = app->bgColor;
    uint16_t textColor = app->textColor;
    
    if (!widget->enabled)
    {
        // État désactivé
        bgColor = ui_theme_get_active()->colors.disabled;
        textColor = ui_theme_get_active()->colors.textDisabled;
    }
    else if (button->pressed || widget->state == WIDGET_STATE_PRESSED)
    {
        // État pressé : assombrir de 20%
        bgColor = darken_color(bgColor, 20);
    }
    else if (widget->hasFocus)
    {
        // État focus : légèrement plus clair
        bgColor = lighten_color(bgColor, 10);
    }
    
    // --- Dessiner le fond ---
    if (app->cornerRadius > 0)
    {
        display_fill_round_rect(r->x, r->y, 
                                 r->x + r->width - 1, r->y + r->height - 1,
                                 app->cornerRadius, bgColor);
    }
    else
    {
        display_fill_rect(r->x, r->y, 
                         r->x + r->width - 1, r->y + r->height - 1, bgColor);
    }
    
    // --- Dessiner la bordure ---
    if (app->borderWidth > 0)
    {
        uint16_t borderColor = widget->hasFocus ? 
                               ui_theme_get_active()->colors.primary : app->borderColor;
        
        if (app->cornerRadius > 0)
        {
            display_draw_round_rect(r->x, r->y,
                                     r->x + r->width - 1, r->y + r->height - 1,
                                     app->cornerRadius, borderColor);
        }
        else
        {
            display_draw_rect(r->x, r->y,
                             r->x + r->width - 1, r->y + r->height - 1, borderColor);
        }
    }
    
    // --- Dessiner le texte centré ---
    if (strlen(button->text) > 0)
    {
        display_set_font(app->font);
        display_set_text_color(textColor);
        
        uint16_t textWidth = display_text_width(button->text, app->fontSize);
        uint16_t textHeight = display_text_height(app->fontSize);
        
        uint16_t textX = r->x + (r->width - textWidth) / 2;
        uint16_t textY = r->y + (r->height - textHeight) / 2;
        
        // Effet d'ombre si pressé (décalage 1 pixel)
        if (button->pressed)
        {
            display_set_text_color(darken_color(textColor, 50));
            display_draw_text(textX + 1, textY + 1, button->text, 
                             darken_color(textColor, 50), app->fontSize);
        }
        
        display_draw_text(textX, textY, button->text, textColor, app->fontSize);
    }
}

// ============================================================
// FONCTIONS TACTILES
// ============================================================

/**
 * @brief Gestion des événements tactiles du bouton
 */
static void button_touch(UIWidget* widget, uint16_t x, uint16_t y, TouchEvent event)
{
    UIButton* button = (UIButton*)widget;
    if (button == NULL || !widget->enabled) return;
    
    switch (event)
    {
        case TOUCH_EVENT_PRESS:
            button->pressed = true;
            widget->state = WIDGET_STATE_PRESSED;
            widget->needsRedraw = true;
            break;
            
        case TOUCH_EVENT_RELEASE:
            if (button->pressed)
            {
                button->pressed = false;
                widget->state = WIDGET_STATE_NORMAL;
                widget->needsRedraw = true;
                
                // Déclencher le callback onClick
                if (button->onClick)
                {
                    button->onClick(button);
                }
            }
            break;
            
        case TOUCH_EVENT_MOVE:
            // Vérifier si le doigt est toujours dans le bouton
            {
                bool inside = (x >= 0 && x < (int16_t)widget->rect.width &&
                              y >= 0 && y < (int16_t)widget->rect.height);
                
                if (button->pressed != inside)
                {
                    button->pressed = inside;
                    widget->needsRedraw = true;
                }
            }
            break;
            
        case TOUCH_EVENT_HOLD:
            // Appui long
            if (button->onLongPress)
            {
                button->onLongPress(button);
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief Gestion des événements clavier du bouton
 */
static void button_key(UIWidget* widget, KeyCode key, KeyEvent event)
{
    UIButton* button = (UIButton*)widget;
    if (button == NULL || !widget->enabled) return;
    
    // Touche OK ou ENTREE = clic
    if ((key == KEY_OK || key == KEY_CALL) && event == KEY_EVENT_PRESS)
    {
        button->pressed = true;
        widget->needsRedraw = true;
    }
    else if ((key == KEY_OK || key == KEY_CALL) && event == KEY_EVENT_RELEASE)
    {
        button->pressed = false;
        widget->needsRedraw = true;
        
        if (button->onClick)
        {
            button->onClick(button);
        }
    }
}

// ============================================================
// CRÉATION
// ============================================================

UIButton* ui_button_create(const char* name, const char* text, UIRect rect)
{
    UIButton* button = (UIButton*)calloc(1, sizeof(UIButton));
    if (button == NULL) return NULL;
    
    // --- Initialiser le widget de base ---
    button->base.type = WIDGET_TYPE_BUTTON;
    if (name) strncpy(button->base.name, name, 31);
    button->base.rect = rect;
    button->base.visible = true;
    button->base.enabled = true;
    button->base.canFocus = true;
    button->base.state = WIDGET_STATE_NORMAL;
    
    // --- Assigner les fonctions virtuelles ---
    button->base.draw = button_draw;
    button->base.onTouch = button_touch;
    button->base.onKey = button_key;
    
    // --- Texte ---
    if (text) strncpy(button->text, text, 63);
    
    // --- Style par défaut ---
    ui_button_set_style(button, BUTTON_STYLE_PRIMARY);
    
    return button;
}

// ============================================================
// CONFIGURATION
// ============================================================

void ui_button_set_style(UIButton* button, ButtonStyle style)
{
    if (button == NULL) return;
    button->appearance = ui_style_get_button(style);
    button->base.needsRedraw = true;
}

void ui_button_set_text(UIButton* button, const char* text)
{
    if (button == NULL) return;
    if (text) strncpy(button->text, text, 63);
    else button->text[0] = '\0';
    button->base.needsRedraw = true;
}

void ui_button_set_enabled(UIButton* button, bool enabled)
{
    if (button == NULL) return;
    button->base.enabled = enabled;
    button->base.state = enabled ? WIDGET_STATE_NORMAL : WIDGET_STATE_DISABLED;
    button->base.needsRedraw = true;
}

void ui_button_set_callback(UIButton* button, void (*callback)(UIButton*))
{
    if (button == NULL) return;
    button->onClick = callback;
}

void ui_button_set_long_press_callback(UIButton* button, void (*callback)(UIButton*))
{
    if (button == NULL) return;
    button->onLongPress = callback;
}

// ============================================================
// FONCTIONS UTILITAIRES
// ============================================================

/**
 * @brief Assombrit une couleur RGB565
 */
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

/**
 * @brief Éclaircit une couleur RGB565
 */
static uint16_t lighten_color(uint16_t color, uint8_t amount)
{
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    
    r += ((31 - r) * amount) / 100;
    g += ((63 - g) * amount) / 100;
    b += ((31 - b) * amount) / 100;
    
    if (r > 31) r = 31;
    if (g > 63) g = 63;
    if (b > 31) b = 31;
    
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
}