/**
 * @file ui_slider.cpp
 * @brief Implémentation du widget Curseur (Slider)
 * 
 * Ce fichier est optionnel. L'implémentation de UISlider
 * se trouve déjà dans ui_widgets.cpp (section 5).
 * 
 * Fonctionnalités :
 * - Rendu graphique avec track, fill et thumb
 * - Gestion tactile pour le glissement
 * - Pas (step) configurable
 * - Styles prédéfinis (normal, volume, brightness)
 * - Callbacks de changement de valeur
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_slider.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// FONCTIONS DE DESSIN
// ============================================================

/**
 * @brief Dessine le slider avec track, fill et thumb
 */
static void slider_draw(UIWidget* widget)
{
    UISlider* slider = (UISlider*)widget;
    if (slider == NULL) return;
    
    SliderAppearance* app = &slider->appearance;
    UIRect* r = &widget->rect;
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    // --- Calculer la position Y centrée pour le track ---
    uint16_t trackY = r->y + r->height / 2 - app->trackHeight / 2;
    uint16_t trackWidth = r->width;
    
    // Laisser de l'espace pour le thumb aux extrémités
    uint16_t thumbDiameter = app->thumbRadius * 2;
    uint16_t usableWidth = trackWidth - thumbDiameter;
    uint16_t trackX = r->x + app->thumbRadius;
    
    // --- Dessiner le track (fond) ---
    if (app->trackHeight >= 4)
    {
        display_fill_round_rect(trackX, trackY,
                                 trackX + usableWidth - 1, trackY + app->trackHeight - 1,
                                 app->trackHeight / 2, app->trackColor);
    }
    else
    {
        display_fill_rect(trackX, trackY,
                         trackX + usableWidth - 1, trackY + app->trackHeight - 1,
                         app->trackColor);
    }
    
    // --- Dessiner le fill (remplissage) ---
    uint16_t fillWidth = 0;
    
    if (slider->maxValue > slider->minValue)
    {
        fillWidth = (uint16_t)((uint32_t)usableWidth * 
                    (slider->value - slider->minValue) / 
                    (slider->maxValue - slider->minValue));
    }
    
    if (fillWidth > 0)
    {
        uint16_t fillColor = app->fillColor;
        
        // Couleur différente si désactivé
        if (!widget->enabled)
        {
            fillColor = colors->disabled;
        }
        
        if (app->trackHeight >= 4)
        {
            display_fill_round_rect(trackX, trackY,
                                     trackX + fillWidth - 1, trackY + app->trackHeight - 1,
                                     app->trackHeight / 2, fillColor);
        }
        else
        {
            display_fill_rect(trackX, trackY,
                             trackX + fillWidth - 1, trackY + app->trackHeight - 1,
                             fillColor);
        }
    }
    
    // --- Dessiner le thumb (poignée) ---
    uint16_t thumbX = trackX + fillWidth;
    
    // Limiter le thumb aux extrémités
    if (thumbX < r->x + app->thumbRadius) thumbX = r->x + app->thumbRadius;
    if (thumbX > r->x + r->width - app->thumbRadius) 
        thumbX = r->x + r->width - app->thumbRadius;
    
    uint16_t thumbY = r->y + r->height / 2;
    
    // Ombre du thumb (si actif)
    if (widget->enabled)
    {
        display_fill_circle(thumbX, thumbY + 1, app->thumbRadius + 1, colors->shadow);
    }
    
    // Corps du thumb
    uint16_t thumbColor = app->thumbColor;
    if (!widget->enabled) thumbColor = colors->disabled;
    if (slider->dragging) thumbColor = lighten_color(thumbColor, 30);
    if (widget->hasFocus) thumbColor = lighten_color(thumbColor, 15);
    
    display_fill_circle(thumbX, thumbY, app->thumbRadius, thumbColor);
    
    // Bordure du thumb
    display_draw_circle(thumbX, thumbY, app->thumbRadius, 
                        darken_color(thumbColor, 20));
    
    // --- Afficher la valeur numérique ---
    if (slider->showValue)
    {
        char valueStr[16];
        snprintf(valueStr, sizeof(valueStr), "%d%s", slider->value, slider->valueSuffix);
        
        display_set_font(&font_5x7);
        display_set_text_color(colors->textPrimary);
        
        uint16_t textWidth = display_text_width(valueStr, 1);
        uint16_t textX = r->x + r->width - textWidth - 4;
        uint16_t textY = r->y - 2;
        
        if (textY < 0) textY = r->y + r->height + 2;
        
        display_draw_text(textX, textY, valueStr, colors->textPrimary, 1);
    }
    
    // --- Afficher les labels min/max ---
    if (slider->showLabels)
    {
        display_set_font(&font_5x7);
        display_set_text_color(colors->textSecondary);
        
        char minStr[8], maxStr[8];
        snprintf(minStr, sizeof(minStr), "%d", slider->minValue);
        snprintf(maxStr, sizeof(maxStr), "%d", slider->maxValue);
        
        uint16_t minY = r->y + r->height + 2;
        uint16_t maxY = r->y + r->height + 2;
        
        display_draw_text(r->x, minY, minStr, colors->textSecondary, 1);
        
        uint16_t maxWidth = display_text_width(maxStr, 1);
        display_draw_text(r->x + r->width - maxWidth, maxY, maxStr, colors->textSecondary, 1);
    }
}

// ============================================================
// FONCTIONS TACTILES
// ============================================================

/**
 * @brief Gestion des événements tactiles pour le slider
 */
static void slider_touch(UIWidget* widget, uint16_t x, uint16_t y, TouchEvent event)
{
    UISlider* slider = (UISlider*)widget;
    if (slider == NULL || !widget->enabled) return;
    
    switch (event)
    {
        case TOUCH_EVENT_PRESS:
            slider->dragging = true;
            widget->state = WIDGET_STATE_PRESSED;
            
            if (slider->onDragStart) slider->onDragStart(slider);
            
            // Calculer la valeur immédiatement
            update_slider_value_from_position(slider, x);
            break;
            
        case TOUCH_EVENT_MOVE:
            if (slider->dragging)
            {
                update_slider_value_from_position(slider, x);
            }
            break;
            
        case TOUCH_EVENT_RELEASE:
            if (slider->dragging)
            {
                slider->dragging = false;
                widget->state = WIDGET_STATE_NORMAL;
                
                if (slider->onDragEnd) slider->onDragEnd(slider);
                
                // Notifier la valeur finale
                if (slider->onValueChanged)
                {
                    slider->onValueChanged(slider, slider->value);
                }
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief Calcule la valeur du slider à partir de la position X tactile
 */
static void update_slider_value_from_position(UISlider* slider, uint16_t x)
{
    if (slider == NULL) return;
    
    UIRect* r = &slider->base.rect;
    uint16_t thumbDiameter = slider->appearance.thumbRadius * 2;
    uint16_t usableWidth = r->width - thumbDiameter;
    
    // Convertir la position X en valeur
    uint16_t relativeX = x - r->x - slider->appearance.thumbRadius;
    
    uint8_t newValue;
    
    if (relativeX <= 0)
    {
        newValue = slider->minValue;
    }
    else if (relativeX >= usableWidth)
    {
        newValue = slider->maxValue;
    }
    else
    {
        newValue = slider->minValue + (uint8_t)(
            (uint32_t)relativeX * (slider->maxValue - slider->minValue) / usableWidth);
    }
    
    // Appliquer le step
    if (slider->step > 0)
    {
        newValue = ((newValue - slider->minValue) / slider->step) * slider->step + slider->minValue;
    }
    
    // Limiter
    if (newValue < slider->minValue) newValue = slider->minValue;
    if (newValue > slider->maxValue) newValue = slider->maxValue;
    
    // Mettre à jour si changé
    if (newValue != slider->value)
    {
        slider->value = newValue;
        slider->base.needsRedraw = true;
        
        // Callback en temps réel
        if (slider->onValueChanging)
        {
            slider->onValueChanging(slider, newValue);
        }
    }
}

/**
 * @brief Gestion des touches clavier
 */
static void slider_key(UIWidget* widget, KeyCode key, KeyEvent event)
{
    UISlider* slider = (UISlider*)widget;
    if (slider == NULL || !widget->enabled) return;
    
    if (event == KEY_EVENT_PRESS || event == KEY_EVENT_REPEAT)
    {
        switch (key)
        {
            case KEY_LEFT:
                ui_slider_decrement(slider);
                break;
                
            case KEY_RIGHT:
                ui_slider_increment(slider);
                break;
                
            case KEY_UP:
                // Incrémenter plus rapidement
                for (uint8_t i = 0; i < 5; i++) ui_slider_increment(slider);
                break;
                
            case KEY_DOWN:
                // Décrémenter plus rapidement
                for (uint8_t i = 0; i < 5; i++) ui_slider_decrement(slider);
                break;
                
            default:
                break;
        }
        
        if (slider->onValueChanged)
        {
            slider->onValueChanged(slider, slider->value);
        }
    }
}

// ============================================================
// CRÉATION
// ============================================================

UISlider* ui_slider_create(const char* name, UIRect rect, uint8_t min, uint8_t max, uint8_t value)
{
    UISlider* slider = (UISlider*)calloc(1, sizeof(UISlider));
    if (slider == NULL) return NULL;
    
    // --- Initialiser le widget de base ---
    slider->base.type = WIDGET_TYPE_SLIDER;
    if (name) strncpy(slider->base.name, name, 31);
    slider->base.rect = rect;
    slider->base.visible = true;
    slider->base.enabled = true;
    slider->base.canFocus = true;
    slider->base.state = WIDGET_STATE_NORMAL;
    
    // --- Assigner les fonctions virtuelles ---
    slider->base.draw = slider_draw;
    slider->base.onTouch = slider_touch;
    slider->base.onKey = slider_key;
    slider->base.onUpdate = NULL;
    
    // --- Valeurs ---
    slider->minValue = min;
    slider->maxValue = max;
    slider->value = value;
    slider->step = 1;
    
    // --- Apparence ---
    slider->showValue = false;
    slider->showLabels = false;
    slider->valueSuffix[0] = '\0';
    
    // --- Style par défaut ---
    ui_slider_set_style(slider, SLIDER_STYLE_NORMAL);
    
    return slider;
}

// ============================================================
// CONFIGURATION DU STYLE
// ============================================================

void ui_slider_set_style(UISlider* slider, SliderStyle style)
{
    if (slider == NULL) return;
    slider->appearance = ui_style_get_slider(style);
    slider->base.needsRedraw = true;
}

void ui_slider_set_colors(UISlider* slider, uint16_t trackColor,
                           uint16_t fillColor, uint16_t thumbColor)
{
    if (slider == NULL) return;
    slider->appearance.trackColor = trackColor;
    slider->appearance.fillColor = fillColor;
    slider->appearance.thumbColor = thumbColor;
    slider->base.needsRedraw = true;
}

void ui_slider_set_track_height(UISlider* slider, uint8_t height)
{
    if (slider == NULL) return;
    slider->appearance.trackHeight = height;
    slider->base.needsRedraw = true;
}

void ui_slider_set_thumb_radius(UISlider* slider, uint8_t radius)
{
    if (slider == NULL) return;
    slider->appearance.thumbRadius = radius;
    slider->base.needsRedraw = true;
}

// ============================================================
// GESTION DE LA VALEUR
// ============================================================

void ui_slider_set_value(UISlider* slider, uint8_t value)
{
    if (slider == NULL) return;
    
    // Limiter
    if (value < slider->minValue) value = slider->minValue;
    if (value > slider->maxValue) value = slider->maxValue;
    
    // Appliquer le step
    if (slider->step > 0)
    {
        value = ((value - slider->minValue) / slider->step) * slider->step + slider->minValue;
    }
    
    if (value != slider->value)
    {
        slider->value = value;
        slider->base.needsRedraw = true;
    }
}

uint8_t ui_slider_get_value(UISlider* slider)
{
    return (slider != NULL) ? slider->value : 0;
}

void ui_slider_increment(UISlider* slider)
{
    if (slider == NULL) return;
    
    uint8_t inc = (slider->step > 0) ? slider->step : 1;
    uint8_t newValue = slider->value + inc;
    
    if (newValue > slider->maxValue) newValue = slider->maxValue;
    
    ui_slider_set_value(slider, newValue);
}

void ui_slider_decrement(UISlider* slider)
{
    if (slider == NULL) return;
    
    uint8_t dec = (slider->step > 0) ? slider->step : 1;
    uint8_t newValue = slider->value - dec;
    
    if (newValue < slider->minValue) newValue = slider->minValue;
    
    ui_slider_set_value(slider, newValue);
}

void ui_slider_set_range(UISlider* slider, uint8_t min, uint8_t max)
{
    if (slider == NULL || min >= max) return;
    
    slider->minValue = min;
    slider->maxValue = max;
    
    // Ajuster la valeur si nécessaire
    if (slider->value < min) slider->value = min;
    if (slider->value > max) slider->value = max;
    
    slider->base.needsRedraw = true;
}

void ui_slider_set_step(UISlider* slider, uint8_t step)
{
    if (slider == NULL) return;
    slider->step = step;
}

uint8_t ui_slider_get_percent(UISlider* slider)
{
    if (slider == NULL || slider->maxValue == slider->minValue) return 0;
    return (uint8_t)((uint32_t)(slider->value - slider->minValue) * 100 / 
                     (slider->maxValue - slider->minValue));
}

void ui_slider_set_percent(UISlider* slider, uint8_t percent)
{
    if (slider == NULL) return;
    if (percent > 100) percent = 100;
    
    uint8_t value = slider->minValue + (uint8_t)(
        (uint32_t)percent * (slider->maxValue - slider->minValue) / 100);
    
    ui_slider_set_value(slider, value);
}

// ============================================================
// AFFICHAGE
// ============================================================

void ui_slider_show_value(UISlider* slider, bool show)
{
    if (slider == NULL) return;
    slider->showValue = show;
    slider->base.needsRedraw = true;
}

void ui_slider_show_labels(UISlider* slider, bool show)
{
    if (slider == NULL) return;
    slider->showLabels = show;
    slider->base.needsRedraw = true;
}

void ui_slider_set_value_suffix(UISlider* slider, const char* suffix)
{
    if (slider == NULL) return;
    if (suffix) strncpy(slider->valueSuffix, suffix, 7);
    else slider->valueSuffix[0] = '\0';
    slider->base.needsRedraw = true;
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