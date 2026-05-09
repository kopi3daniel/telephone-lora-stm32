/**
 * @file ui_widgets.cpp
 * @brief Implémentation des widgets concrets
 * 
 * Ce fichier contient l'implémentation de tous les widgets
 * déclarés dans ui_widgets.h.
 * 
 * Chaque widget a ses fonctions de création, configuration
 * et dessin.
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_widgets.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// SECTION 1 : BOUTON (UIButton)
// ============================================================

/**
 * @brief Fonction de dessin du bouton
 */
static void button_draw(UIWidget* widget)
{
    UIButton* button = (UIButton*)widget;
    if (button == NULL) return;
    
    ButtonAppearance* app = &button->appearance;
    UIRect* r = &widget->rect;
    
    // Couleur de fond selon l'état
    uint16_t bgColor = app->bgColor;
    uint16_t textColor = app->textColor;
    
    if (!widget->enabled)
    {
        bgColor = ui_theme_get_active()->colors.disabled;
        textColor = ui_theme_get_active()->colors.textDisabled;
    }
    else if (button->pressed || widget->state == WIDGET_STATE_PRESSED)
    {
        // Assombrir légèrement
        bgColor = darken_color(bgColor, 20);
    }
    
    // Fond
    if (app->cornerRadius > 0)
    {
        display_fill_round_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1,
                                 app->cornerRadius, bgColor);
    }
    else
    {
        display_fill_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1, bgColor);
    }
    
    // Bordure
    if (app->borderWidth > 0)
    {
        if (app->cornerRadius > 0)
        {
            display_draw_round_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1,
                                     app->cornerRadius, app->borderColor);
        }
        else
        {
            display_draw_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1,
                             app->borderColor);
        }
    }
    
    // Texte centré
    if (strlen(button->text) > 0)
    {
        display_set_font(app->font);
        display_set_text_color(textColor);
        
        uint16_t textWidth = display_text_width(button->text, app->fontSize);
        uint16_t textHeight = display_text_height(app->fontSize);
        
        uint16_t textX = r->x + (r->width - textWidth) / 2;
        uint16_t textY = r->y + (r->height - textHeight) / 2;
        
        display_draw_text(textX, textY, button->text, textColor, app->fontSize);
    }
}

/**
 * @brief Fonction tactile du bouton
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
                
                // Déclencher le callback
                if (button->onClick)
                {
                    button->onClick(button);
                }
            }
            break;
            
        case TOUCH_EVENT_MOVE:
            // Vérifier si on est toujours dans le bouton
            {
                bool inside = (x >= 0 && x < widget->rect.width &&
                              y >= 0 && y < widget->rect.height);
                if (button->pressed != inside)
                {
                    button->pressed = inside;
                    widget->needsRedraw = true;
                }
            }
            break;
            
        default:
            break;
    }
}

UIButton* ui_button_create(const char* name, const char* text, UIRect rect)
{
    UIButton* button = (UIButton*)calloc(1, sizeof(UIButton));
    if (button == NULL) return NULL;
    
    // Initialiser le widget de base
    button->base.type = WIDGET_TYPE_BUTTON;
    if (name) strncpy(button->base.name, name, 31);
    button->base.rect = rect;
    button->base.visible = true;
    button->base.enabled = true;
    button->base.canFocus = true;
    
    // Assigner les fonctions virtuelles
    button->base.draw = button_draw;
    button->base.onTouch = button_touch;
    
    // Texte
    if (text) strncpy(button->text, text, 63);
    
    // Style par défaut
    ui_button_set_style(button, BUTTON_STYLE_PRIMARY);
    
    return button;
}

void ui_button_set_style(UIButton* button, ButtonStyle style)
{
    button->appearance = ui_style_get_button(style);
    button->base.needsRedraw = true;
}

void ui_button_set_text(UIButton* button, const char* text)
{
    if (text) strncpy(button->text, text, 63);
    button->base.needsRedraw = true;
}

void ui_button_set_enabled(UIButton* button, bool enabled)
{
    button->base.enabled = enabled;
    button->base.state = enabled ? WIDGET_STATE_NORMAL : WIDGET_STATE_DISABLED;
    button->base.needsRedraw = true;
}

void ui_button_set_callback(UIButton* button, void (*callback)(UIButton*))
{
    button->onClick = callback;
}

// ============================================================
// SECTION 2 : LABEL (UILabel)
// ============================================================

static void label_draw(UIWidget* widget)
{
    UILabel* label = (UILabel*)widget;
    if (label == NULL) return;
    
    LabelAppearance* app = &label->appearance;
    UIRect* r = &widget->rect;
    
    // Fond (si non transparent)
    if (app->bgColor != 0x0000 && app->bgColor != 0xFFFF)
    {
        display_fill_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1, app->bgColor);
    }
    
    if (strlen(label->text) == 0) return;
    
    display_set_font(app->font);
    display_set_text_color(app->textColor);
    
    // Calculer la position selon l'alignement
    uint16_t textWidth = display_text_width(label->text, app->fontSize);
    uint16_t textHeight = display_text_height(app->fontSize);
    
    uint16_t textX = r->x;
    uint16_t textY = r->y;
    
    switch (app->textAlign)
    {
        case UI_ALIGN_CENTER:
            textX = r->x + (r->width - textWidth) / 2;
            break;
        case UI_ALIGN_RIGHT:
            textX = r->x + r->width - textWidth;
            break;
        default:
            break;
    }
    
    textY = r->y + (r->height - textHeight) / 2;
    
    // Gérer le retour à la ligne si texte trop long
    if (label->maxWidth > 0 && textWidth > label->maxWidth)
    {
        // Découpage simple (à améliorer avec word wrap)
        uint16_t charWidth = app->font->charWidth * app->fontSize;
        uint16_t charsPerLine = label->maxWidth / charWidth;
        
        uint16_t lineY = textY;
        uint16_t pos = 0;
        uint16_t len = strlen(label->text);
        
        while (pos < len && lineY < r->y + r->height)
        {
            char line[128];
            uint16_t lineLen = (pos + charsPerLine < len) ? charsPerLine : (len - pos);
            strncpy(line, label->text + pos, lineLen);
            line[lineLen] = '\0';
            
            display_draw_text(textX, lineY, line, app->textColor, app->fontSize);
            
            pos += lineLen;
            lineY += textHeight + 2;
        }
    }
    else
    {
        display_draw_text(textX, textY, label->text, app->textColor, app->fontSize);
    }
}

UILabel* ui_label_create(const char* name, const char* text, UIRect rect)
{
    UILabel* label = (UILabel*)calloc(1, sizeof(UILabel));
    if (label == NULL) return NULL;
    
    label->base.type = WIDGET_TYPE_LABEL;
    if (name) strncpy(label->base.name, name, 31);
    label->base.rect = rect;
    label->base.visible = true;
    label->base.enabled = true;
    label->base.canFocus = false;
    
    label->base.draw = label_draw;
    
    if (text) strncpy(label->text, text, 255);
    
    ui_label_set_style(label, LABEL_STYLE_BODY);
    
    return label;
}

void ui_label_set_style(UILabel* label, LabelStyle style)
{
    label->appearance = ui_style_get_label(style);
    label->base.needsRedraw = true;
}

void ui_label_set_text(UILabel* label, const char* text)
{
    if (text) strncpy(label->text, text, 255);
    label->base.needsRedraw = true;
}

void ui_label_set_color(UILabel* label, uint16_t color)
{
    label->appearance.textColor = color;
    label->base.needsRedraw = true;
}

void ui_label_set_alignment(UILabel* label, UIAlign align)
{
    label->appearance.textAlign = align;
    label->base.needsRedraw = true;
}

// ============================================================
// SECTION 3 : ZONE DE TEXTE (UITextBox)
// ============================================================

static void textbox_draw(UIWidget* widget)
{
    UITextBox* textbox = (UITextBox*)widget;
    if (textbox == NULL) return;
    
    TextBoxAppearance* app = &textbox->appearance;
    UIRect* r = &widget->rect;
    
    // Fond
    if (app->cornerRadius > 0)
    {
        display_fill_round_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1,
                                 app->cornerRadius, app->bgColor);
    }
    else
    {
        display_fill_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1, app->bgColor);
    }
    
    // Bordure
    if (app->borderWidth > 0)
    {
        uint16_t borderColor = widget->hasFocus ? app->cursorColor : app->borderColor;
        
        if (app->cornerRadius > 0)
        {
            display_draw_round_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1,
                                     app->cornerRadius, borderColor);
        }
        else
        {
            display_draw_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1, borderColor);
        }
    }
    
    // Texte ou placeholder
    const char* displayText = (strlen(textbox->text) > 0) ? textbox->text : textbox->placeholder;
    uint16_t textColor = (strlen(textbox->text) > 0) ? app->textColor : 
                          ui_theme_get_active()->colors.textHint;
    
    // Mode sécurisé : afficher des étoiles
    char secureText[256];
    if (textbox->secure && strlen(textbox->text) > 0)
    {
        uint16_t len = strlen(textbox->text);
        for (uint16_t i = 0; i < len && i < 255; i++) secureText[i] = '*';
        secureText[len] = '\0';
        displayText = secureText;
    }
    
    if (strlen(displayText) > 0)
    {
        display_set_font(app->font);
        display_set_text_color(textColor);
        
        uint16_t textX = r->x + app->padding.left;
        uint16_t textY = r->y + app->padding.top;
        
        display_draw_text(textX, textY, displayText, textColor, app->fontSize);
    }
    
    // Curseur clignotant
    if (textbox->editable && widget->hasFocus && textbox->cursorVisible)
    {
        uint16_t cursorX = r->x + app->padding.left + 
                          display_text_width(textbox->text, app->fontSize);
        uint16_t cursorY = r->y + app->padding.top;
        uint16_t cursorH = display_text_height(app->fontSize);
        
        display_fill_rect(cursorX, cursorY, cursorX + 2, cursorY + cursorH, app->cursorColor);
    }
}

static void textbox_key(UIWidget* widget, KeyCode key, KeyEvent event)
{
    UITextBox* textbox = (UITextBox*)widget;
    if (textbox == NULL || !textbox->editable) return;
    
    if (event == KEY_EVENT_PRESS || event == KEY_EVENT_REPEAT)
    {
        switch (key)
        {
            case KEY_BACK:
                ui_textbox_delete_char(textbox);
                break;
                
            case KEY_OK:
                if (textbox->onSubmit) textbox->onSubmit(textbox);
                break;
                
            default:
                // Les touches de saisie sont gérées par le keypad_manager
                break;
        }
    }
}

UITextBox* ui_textbox_create(const char* name, UIRect rect)
{
    UITextBox* textbox = (UITextBox*)calloc(1, sizeof(UITextBox));
    if (textbox == NULL) return NULL;
    
    textbox->base.type = WIDGET_TYPE_TEXTBOX;
    if (name) strncpy(textbox->base.name, name, 31);
    textbox->base.rect = rect;
    textbox->base.visible = true;
    textbox->base.enabled = true;
    textbox->base.canFocus = true;
    
    textbox->base.draw = textbox_draw;
    textbox->base.onKey = textbox_key;
    
    textbox->maxLength = 255;
    textbox->editable = true;
    textbox->cursorVisible = true;
    
    ui_textbox_set_style(textbox, TEXTBOX_STYLE_NORMAL);
    
    return textbox;
}

void ui_textbox_set_style(UITextBox* textbox, TextBoxStyle style)
{
    textbox->appearance = ui_style_get_textbox(style);
    textbox->base.needsRedraw = true;
}

void ui_textbox_set_text(UITextBox* textbox, const char* text)
{
    if (text) strncpy(textbox->text, text, textbox->maxLength);
    textbox->cursorPos = strlen(textbox->text);
    textbox->base.needsRedraw = true;
}

const char* ui_textbox_get_text(UITextBox* textbox)
{
    return textbox->text;
}

void ui_textbox_set_placeholder(UITextBox* textbox, const char* placeholder)
{
    if (placeholder) strncpy(textbox->placeholder, placeholder, 63);
    textbox->base.needsRedraw = true;
}

void ui_textbox_set_secure(UITextBox* textbox, bool secure)
{
    textbox->secure = secure;
    textbox->base.needsRedraw = true;
}

void ui_textbox_set_editable(UITextBox* textbox, bool editable)
{
    textbox->editable = editable;
    textbox->base.needsRedraw = true;
}

void ui_textbox_insert_char(UITextBox* textbox, char c)
{
    uint16_t len = strlen(textbox->text);
    if (len >= textbox->maxLength - 1) return;
    
    // Décaler les caractères après le curseur
    memmove(&textbox->text[textbox->cursorPos + 1], 
            &textbox->text[textbox->cursorPos],
            len - textbox->cursorPos + 1);
    
    textbox->text[textbox->cursorPos] = c;
    textbox->cursorPos++;
    textbox->base.needsRedraw = true;
    
    if (textbox->onTextChanged) textbox->onTextChanged(textbox);
}

void ui_textbox_delete_char(UITextBox* textbox)
{
    if (textbox->cursorPos == 0) return;
    
    uint16_t len = strlen(textbox->text);
    memmove(&textbox->text[textbox->cursorPos - 1],
            &textbox->text[textbox->cursorPos],
            len - textbox->cursorPos + 1);
    
    textbox->cursorPos--;
    textbox->base.needsRedraw = true;
    
    if (textbox->onTextChanged) textbox->onTextChanged(textbox);
}

void ui_textbox_clear(UITextBox* textbox)
{
    memset(textbox->text, 0, sizeof(textbox->text));
    textbox->cursorPos = 0;
    textbox->base.needsRedraw = true;
    
    if (textbox->onTextChanged) textbox->onTextChanged(textbox);
}

// ============================================================
// SECTION 4 : LISTE (UIList)
// ============================================================

static void list_draw(UIWidget* widget)
{
    UIList* list = (UIList*)widget;
    if (list == NULL) return;
    
    ListAppearance* app = &list->appearance;
    UIRect* r = &widget->rect;
    
    // Fond de la liste
    display_fill_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1, app->bgColor);
    
    // Calculer les éléments visibles
    uint16_t visibleItems = (r->height / app->itemHeight);
    if (visibleItems > list->itemCount) visibleItems = list->itemCount;
    
    for (uint16_t i = 0; i < visibleItems; i++)
    {
        uint16_t itemIndex = list->scrollOffset + i;
        if (itemIndex >= list->itemCount) break;
        
        UIListItem* item = &list->items[itemIndex];
        uint16_t itemY = r->y + i * app->itemHeight;
        
        // Fond de l'élément
        uint16_t itemBg = (itemIndex == list->selectedIndex) ? 
                          app->itemSelectedColor : app->itemBgColor;
        
        display_fill_rect(r->x, itemY, r->x + r->width - 1, itemY + app->itemHeight - 1, itemBg);
        
        // Séparateur
        if (i > 0 && app->dividerColor != itemBg)
        {
            display_fill_rect(r->x, itemY, r->x + r->width - 1, itemY, app->dividerColor);
        }
        
        // Texte principal
        display_set_font(app->font);
        display_set_text_color(app->textColor);
        
        uint16_t textX = r->x + app->itemPadding.left;
        uint16_t textY = itemY + app->itemPadding.top;
        
        display_draw_text(textX, textY, item->text, app->textColor, app->fontSize);
        
        // Sous-texte (si présent)
        if (strlen(item->subtext) > 0)
        {
            uint16_t subtextY = textY + display_text_height(app->fontSize) + 2;
            display_draw_text(textX, subtextY, item->subtext, 
                             ui_theme_get_active()->colors.textSecondary, 1);
        }
    }
}

static void list_touch(UIWidget* widget, uint16_t x, uint16_t y, TouchEvent event)
{
    UIList* list = (UIList*)widget;
    if (list == NULL || event != TOUCH_EVENT_PRESS) return;
    
    ListAppearance* app = &list->appearance;
    
    // Calculer l'index de l'élément touché
    uint16_t itemIndex = list->scrollOffset + (y / app->itemHeight);
    
    if (itemIndex < list->itemCount)
    {
        ui_list_select(list, itemIndex);
    }
}

UIList* ui_list_create(const char* name, UIRect rect, uint16_t maxItems)
{
    UIList* list = (UIList*)calloc(1, sizeof(UIList));
    if (list == NULL) return NULL;
    
    list->base.type = WIDGET_TYPE_LIST;
    if (name) strncpy(list->base.name, name, 31);
    list->base.rect = rect;
    list->base.visible = true;
    list->base.enabled = true;
    list->base.canFocus = true;
    
    list->base.draw = list_draw;
    list->base.onTouch = list_touch;
    
    list->maxItems = maxItems;
    list->items = (UIListItem*)calloc(maxItems, sizeof(UIListItem));
    list->selectedIndex = -1;
    
    ui_list_set_style(list, LIST_STYLE_PLAIN);
    
    return list;
}

bool ui_list_add_item(UIList* list, const char* text, const char* subtext, void* userData)
{
    if (list == NULL || list->itemCount >= list->maxItems) return false;
    
    UIListItem* item = &list->items[list->itemCount];
    if (text) strncpy(item->text, text, 127);
    if (subtext) strncpy(item->subtext, subtext, 127);
    item->userData = userData;
    
    list->itemCount++;
    list->visibleItems = list->base.rect.height / list->appearance.itemHeight;
    list->base.needsRedraw = true;
    
    return true;
}

bool ui_list_remove_item(UIList* list, uint16_t index)
{
    if (list == NULL || index >= list->itemCount) return false;
    
    if (index < list->itemCount - 1)
    {
        memmove(&list->items[index], &list->items[index + 1],
                (list->itemCount - index - 1) * sizeof(UIListItem));
    }
    list->itemCount--;
    list->base.needsRedraw = true;
    
    return true;
}

void ui_list_clear(UIList* list)
{
    if (list == NULL) return;
    list->itemCount = 0;
    list->selectedIndex = -1;
    list->scrollOffset = 0;
    list->base.needsRedraw = true;
}

void ui_list_set_style(UIList* list, ListStyle style)
{
    list->appearance = ui_style_get_list(style);
    list->base.needsRedraw = true;
}

void ui_list_select(UIList* list, int16_t index)
{
    if (list == NULL) return;
    list->selectedIndex = index;
    list->base.needsRedraw = true;
    
    if (list->onSelect) list->onSelect(list, index);
}

int16_t ui_list_get_selected(UIList* list)
{
    return list ? list->selectedIndex : -1;
}

UIListItem* ui_list_get_item(UIList* list, uint16_t index)
{
    if (list == NULL || index >= list->itemCount) return NULL;
    return &list->items[index];
}

void ui_list_scroll_up(UIList* list)
{
    if (list == NULL || list->scrollOffset == 0) return;
    list->scrollOffset--;
    list->base.needsRedraw = true;
}

void ui_list_scroll_down(UIList* list)
{
    if (list == NULL) return;
    uint16_t maxOffset = (list->itemCount > list->visibleItems) ? 
                         (list->itemCount - list->visibleItems) : 0;
    if (list->scrollOffset < maxOffset)
    {
        list->scrollOffset++;
        list->base.needsRedraw = true;
    }
}

// ============================================================
// SECTION 5 : SLIDER (UISlider)
// ============================================================

static void slider_draw(UIWidget* widget)
{
    UISlider* slider = (UISlider*)widget;
    if (slider == NULL) return;
    
    SliderAppearance* app = &slider->appearance;
    UIRect* r = &widget->rect;
    
    // Track (fond)
    uint16_t trackY = r->y + r->height / 2 - app->trackHeight / 2;
    display_fill_round_rect(r->x, trackY, r->x + r->width - 1, trackY + app->trackHeight - 1,
                             app->trackHeight / 2, app->trackColor);
    
    // Fill (remplissage)
    uint16_t fillWidth = (uint16_t)((uint32_t)r->width * slider->value / 100);
    if (fillWidth > 0)
    {
        display_fill_round_rect(r->x, trackY, r->x + fillWidth - 1, trackY + app->trackHeight - 1,
                                 app->trackHeight / 2, app->fillColor);
    }
    
    // Thumb (curseur)
    uint16_t thumbX = r->x + fillWidth - app->thumbRadius;
    if (thumbX < r->x) thumbX = r->x;
    if (thumbX > r->x + r->width - app->thumbRadius * 2) 
        thumbX = r->x + r->width - app->thumbRadius * 2;
    
    uint16_t thumbY = r->y + r->height / 2 - app->thumbRadius;
    
    display_fill_circle(thumbX + app->thumbRadius, thumbY + app->thumbRadius, 
                        app->thumbRadius, app->thumbColor);
}

static void slider_touch(UIWidget* widget, uint16_t x, uint16_t y, TouchEvent event)
{
    UISlider* slider = (UISlider*)widget;
    if (slider == NULL || !widget->enabled) return;
    
    if (event == TOUCH_EVENT_PRESS || event == TOUCH_EVENT_MOVE)
    {
        // Calculer la valeur à partir de la position X
        uint8_t newValue = (uint8_t)((uint32_t)x * 100 / widget->rect.width);
        
        // Appliquer le step
        if (slider->step > 0)
        {
            newValue = (newValue / slider->step) * slider->step;
        }
        
        // Limiter
        if (newValue < slider->minValue) newValue = slider->minValue;
        if (newValue > slider->maxValue) newValue = slider->maxValue;
        
        if (newValue != slider->value)
        {
            slider->value = newValue;
            widget->needsRedraw = true;
            
            if (slider->onValueChanged) slider->onValueChanged(slider, newValue);
        }
    }
}

UISlider* ui_slider_create(const char* name, UIRect rect, uint8_t min, uint8_t max, uint8_t value)
{
    UISlider* slider = (UISlider*)calloc(1, sizeof(UISlider));
    if (slider == NULL) return NULL;
    
    slider->base.type = WIDGET_TYPE_SLIDER;
    if (name) strncpy(slider->base.name, name, 31);
    slider->base.rect = rect;
    slider->base.visible = true;
    slider->base.enabled = true;
    slider->base.canFocus = true;
    
    slider->base.draw = slider_draw;
    slider->base.onTouch = slider_touch;
    
    slider->minValue = min;
    slider->maxValue = max;
    slider->value = value;
    slider->step = 1;
    
    ui_slider_set_style(slider, SLIDER_STYLE_NORMAL);
    
    return slider;
}

void ui_slider_set_style(UISlider* slider, SliderStyle style)
{
    slider->appearance = ui_style_get_slider(style);
    slider->base.needsRedraw = true;
}

void ui_slider_set_value(UISlider* slider, uint8_t value)
{
    if (value > slider->maxValue) value = slider->maxValue;
    if (value < slider->minValue) value = slider->minValue;
    slider->value = value;
    slider->base.needsRedraw = true;
}

uint8_t ui_slider_get_value(UISlider* slider)
{
    return slider ? slider->value : 0;
}

// ============================================================
// SECTION 6 : CHECKBOX (UICheckbox)
// ============================================================

static void checkbox_draw(UIWidget* widget)
{
    UICheckbox* cb = (UICheckbox*)widget;
    if (cb == NULL) return;
    
    UIRect* r = &widget->rect;
    uint16_t boxSize = r->height - 4;
    uint16_t boxY = r->y + (r->height - boxSize) / 2;
    
    // Case
    uint16_t bgColor = cb->checked ? ui_theme_get_primary() : ui_theme_get_surface();
    display_fill_round_rect(r->x, boxY, r->x + boxSize - 1, boxY + boxSize - 1, 4, bgColor);
    display_draw_round_rect(r->x, boxY, r->x + boxSize - 1, boxY + boxSize - 1, 4, 
                            ui_theme_get_active()->colors.border);
    
    // Cocher si checked
    if (cb->checked)
    {
        // Dessiner un V simplifié
        display_draw_line(r->x + 4, boxY + boxSize / 2, r->x + boxSize / 2, boxY + boxSize - 4, 
                         ui_theme_get_active()->colors.onPrimary);
        display_draw_line(r->x + boxSize / 2, boxY + boxSize - 4, r->x + boxSize - 4, boxY + 4,
                         ui_theme_get_active()->colors.onPrimary);
    }
    
    // Texte
    if (strlen(cb->text) > 0)
    {
        uint16_t textX = r->x + boxSize + 8;
        uint16_t textY = r->y + (r->height - display_text_height(1)) / 2;
        
        display_set_text_color(ui_theme_get_active()->colors.textPrimary);
        display_draw_text(textX, textY, cb->text, 
                         ui_theme_get_active()->colors.textPrimary, 1);
    }
}

UICheckbox* ui_checkbox_create(const char* name, const char* text, UIRect rect)
{
    UICheckbox* cb = (UICheckbox*)calloc(1, sizeof(UICheckbox));
    if (cb == NULL) return NULL;
    
    cb->base.type = WIDGET_TYPE_CHECKBOX;
    if (name) strncpy(cb->base.name, name, 31);
    cb->base.rect = rect;
    cb->base.visible = true;
    cb->base.enabled = true;
    
    cb->base.draw = checkbox_draw;
    
    if (text) strncpy(cb->text, text, 127);
    
    return cb;
}

void ui_checkbox_set_checked(UICheckbox* checkbox, bool checked)
{
    checkbox->checked = checked;
    checkbox->base.needsRedraw = true;
    if (checkbox->onToggle) checkbox->onToggle(checkbox, checked);
}

bool ui_checkbox_is_checked(UICheckbox* checkbox)
{
    return checkbox ? checkbox->checked : false;
}

void ui_checkbox_toggle(UICheckbox* checkbox)
{
    ui_checkbox_set_checked(checkbox, !checkbox->checked);
}

// ============================================================
// SECTION 7 : PROGRESS BAR (UIProgressBar)
// ============================================================

static void progress_draw(UIWidget* widget)
{
    UIProgressBar* progress = (UIProgressBar*)widget;
    if (progress == NULL) return;
    
    ProgressAppearance* app = &progress->appearance;
    UIRect* r = &widget->rect;
    
    // Fond
    if (app->cornerRadius > 0)
    {
        display_fill_round_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1,
                                 app->cornerRadius, app->bgColor);
    }
    else
    {
        display_fill_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1, app->bgColor);
    }
    
    // Remplissage
    uint16_t fillWidth = (uint16_t)((uint32_t)r->width * progress->value / 100);
    if (fillWidth > 0)
    {
        if (app->cornerRadius > 0)
        {
            display_fill_round_rect(r->x, r->y, r->x + fillWidth - 1, r->y + r->height - 1,
                                     app->cornerRadius, app->fillColor);
        }
        else
        {
            display_fill_rect(r->x, r->y, r->x + fillWidth - 1, r->y + r->height - 1, app->fillColor);
        }
    }
    
    // Pourcentage
    if (progress->showPercentage && strlen(progress->label) > 0)
    {
        display_set_text_color(ui_theme_get_active()->colors.onPrimary);
        display_draw_text_center(r->y + (r->height - display_text_height(1)) / 2,
                                 progress->label, 
                                 ui_theme_get_active()->colors.onPrimary, 1);
    }
}

UIProgressBar* ui_progress_create(const char* name, UIRect rect)
{
    UIProgressBar* progress = (UIProgressBar*)calloc(1, sizeof(UIProgressBar));
    if (progress == NULL) return NULL;
    
    progress->base.type = WIDGET_TYPE_PROGRESS;
    if (name) strncpy(progress->base.name, name, 31);
    progress->base.rect = rect;
    progress->base.visible = true;
    
    progress->base.draw = progress_draw;
    
    ui_progress_set_style(progress, PROGRESS_STYLE_NORMAL);
    
    return progress;
}

void ui_progress_set_style(UIProgressBar* progress, ProgressStyle style)
{
    progress->appearance = ui_style_get_progress(style);
    progress->base.needsRedraw = true;
}

void ui_progress_set_value(UIProgressBar* progress, uint8_t value)
{
    if (value > 100) value = 100;
    progress->value = value;
    
    if (progress->showPercentage)
    {
        snprintf(progress->label, sizeof(progress->label), "%d%%", value);
    }
    
    progress->base.needsRedraw = true;
}

uint8_t ui_progress_get_value(UIProgressBar* progress)
{
    return progress ? progress->value : 0;
}

// ============================================================
// SECTION 8 : IMAGE (UIImage)
// ============================================================

static void image_draw(UIWidget* widget)
{
    UIImage* image = (UIImage*)widget;
    if (image == NULL || image->bitmap == NULL) return;
    
    UIRect* r = &widget->rect;
    
    if (image->scaleToFit)
    {
        // Redimensionnement simple
        uint16_t drawW = r->width;
        uint16_t drawH = r->height;
        uint16_t drawX = r->x;
        uint16_t drawY = r->y;
        
        for (uint16_t y = 0; y < drawH; y++)
        {
            uint16_t srcY = (y * image->bitmapHeight) / drawH;
            
            for (uint16_t x = 0; x < drawW; x++)
            {
                uint16_t srcX = (x * image->bitmapWidth) / drawW;
                uint16_t color = image->bitmap[srcY * image->bitmapWidth + srcX];
                
                display_draw_pixel(drawX + x, drawY + y, color);
            }
        }
    }
    else
    {
        // Affichage direct (dans les limites du rectangle)
        uint16_t drawW = (r->width < image->bitmapWidth) ? r->width : image->bitmapWidth;
        uint16_t drawH = (r->height < image->bitmapHeight) ? r->height : image->bitmapHeight;
        
        for (uint16_t y = 0; y < drawH; y++)
        {
            for (uint16_t x = 0; x < drawW; x++)
            {
                uint16_t color = image->bitmap[y * image->bitmapWidth + x];
                display_draw_pixel(r->x + x, r->y + y, color);
            }
        }
    }
}

UIImage* ui_image_create(const char* name, UIRect rect, const uint16_t* bitmap,
                          uint16_t width, uint16_t height)
{
    UIImage* image = (UIImage*)calloc(1, sizeof(UIImage));
    if (image == NULL) return NULL;
    
    image->base.type = WIDGET_TYPE_IMAGE;
    if (name) strncpy(image->base.name, name, 31);
    image->base.rect = rect;
    image->base.visible = true;
    
    image->base.draw = image_draw;
    
    image->bitmap = bitmap;
    image->bitmapWidth = width;
    image->bitmapHeight = height;
    image->scaleToFit = false;
    
    return image;
}

void ui_image_set_bitmap(UIImage* image, const uint16_t* bitmap, uint16_t w, uint16_t h)
{
    if (image == NULL) return;
    image->bitmap = bitmap;
    image->bitmapWidth = w;
    image->bitmapHeight = h;
    image->base.needsRedraw = true;
}

// ============================================================
// SECTION 9 : PANNEAU (UIPanel)
// ============================================================

static void panel_draw(UIWidget* widget)
{
    UIPanel* panel = (UIPanel*)widget;
    if (panel == NULL) return;
    
    PanelAppearance* app = &panel->appearance;
    UIRect* r = &widget->rect;
    
    // Fond
    display_fill_round_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1,
                             app->cornerRadius, app->bgColor);
    
    // Bordure
    if (app->borderWidth > 0)
    {
        display_draw_round_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1,
                                 app->cornerRadius, app->borderColor);
    }
    
    // Titre
    if (strlen(panel->title) > 0)
    {
        display_set_font(ui_theme_get_title_font());
        display_set_text_color(ui_theme_get_text_primary());
        display_draw_text(r->x + app->padding.left, r->y + app->padding.top,
                         panel->title, ui_theme_get_text_primary(), 1);
    }
    
    // Dessiner les widgets enfants
    for (uint8_t i = 0; i < panel->childCount; i++)
    {
        UIWidget* child = panel->children[i];
        if (child && child->visible && child->draw)
        {
            child->draw(child);
        }
    }
}

UIPanel* ui_panel_create(const char* name, UIRect rect, uint8_t maxChildren)
{
    UIPanel* panel = (UIPanel*)calloc(1, sizeof(UIPanel));
    if (panel == NULL) return NULL;
    
    panel->base.type = WIDGET_TYPE_PANEL;
    if (name) strncpy(panel->base.name, name, 31);
    panel->base.rect = rect;
    panel->base.visible = true;
    
    panel->base.draw = panel_draw;
    
    panel->maxChildren = maxChildren;
    panel->children = (UIWidget**)calloc(maxChildren, sizeof(UIWidget*));
    
    ui_panel_set_style(panel, PANEL_STYLE_ELEVATED);
    
    return panel;
}

bool ui_panel_add_child(UIPanel* panel, UIWidget* widget)
{
    if (panel == NULL || widget == NULL) return false;
    if (panel->childCount >= panel->maxChildren) return false;
    
    panel->children[panel->childCount++] = widget;
    return true;
}

void ui_panel_set_style(UIPanel* panel, PanelStyle style)
{
    panel->appearance = ui_style_get_panel(style);
    panel->base.needsRedraw = true;
}

void ui_panel_set_title(UIPanel* panel, const char* title)
{
    if (title) strncpy(panel->title, title, 63);
    panel->base.needsRedraw = true;
}

void ui_panel_toggle_collapse(UIPanel* panel)
{
    panel->collapsed = !panel->collapsed;
    panel->base.needsRedraw = true;
}

// ============================================================
// SECTION 10 : FONCTIONS UTILITAIRES
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

void ui_widgets_init(void)
{
    WIDGET_DEBUG("Module de widgets initialisé\n");
    // Les fonctions de dessin sont déjà assignées dans chaque create
}