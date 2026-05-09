/**
 * @file ui_label.cpp
 * @brief Implémentation du widget Label (texte)
 * 
 * Ce fichier est optionnel. L'implémentation complète de UILabel
 * se trouve déjà dans ui_widgets.cpp (section 2).
 * 
 * Il est fourni ici comme référence pour un accès rapide
 * au widget label sans inclure tous les autres widgets.
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_label.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// FONCTIONS DE DESSIN
// ============================================================

/**
 * @brief Fonction de dessin du label
 * 
 * Gère :
 * - L'alignement horizontal (gauche, centre, droite)
 * - Le retour à la ligne automatique (word wrap)
 * - L'ajustement automatique de la taille
 * - Les couleurs de fond et de texte
 */
static void label_draw(UIWidget* widget)
{
    UILabel* label = (UILabel*)widget;
    if (label == NULL || strlen(label->text) == 0) return;
    
    LabelAppearance* app = &label->appearance;
    UIRect* r = &widget->rect;
    
    // --- Fond (si couleur différente du thème) ---
    uint16_t themeBg = ui_theme_get_active()->colors.background;
    if (app->bgColor != themeBg && app->bgColor != 0x0000)
    {
        display_fill_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1, app->bgColor);
    }
    
    // --- Configuration de la police ---
    display_set_font(app->font);
    display_set_text_color(app->textColor);
    
    uint16_t lineHeight = display_text_height(app->fontSize) + 2;  // +2 pour l'interligne
    uint16_t totalTextWidth = display_text_width(label->text, app->fontSize);
    
    // --- Ajustement automatique de la taille ---
    if (label->autoSize)
    {
        uint16_t textHeight = display_text_height(app->fontSize);
        
        if (label->wordWrap && label->maxWidth > 0 && totalTextWidth > label->maxWidth)
        {
            // Calculer le nombre de lignes nécessaires
            uint16_t charWidth = app->font->charWidth * app->fontSize;
            uint16_t charsPerLine = label->maxWidth / charWidth;
            uint16_t totalChars = strlen(label->text);
            uint16_t numLines = (totalChars + charsPerLine - 1) / charsPerLine;
            
            uint16_t newHeight = numLines * lineHeight + app->fontSize;
            if (newHeight != widget->rect.height)
            {
                widget->rect.height = newHeight;
            }
        }
        else
        {
            if (totalTextWidth != widget->rect.width)
            {
                widget->rect.width = totalTextWidth;
            }
        }
    }
    
    // --- Affichage simple (sans word wrap) ---
    if (!label->wordWrap || label->maxWidth == 0 || totalTextWidth <= label->maxWidth)
    {
        // Calculer la position X selon l'alignement
        uint16_t textX = r->x;
        uint16_t textY = r->y + (r->height - display_text_height(app->fontSize)) / 2;
        
        switch (app->textAlign)
        {
            case UI_ALIGN_CENTER:
                textX = r->x + (r->width - totalTextWidth) / 2;
                break;
            case UI_ALIGN_RIGHT:
                textX = r->x + r->width - totalTextWidth;
                break;
            case UI_ALIGN_LEFT:
            default:
                textX = r->x;
                break;
        }
        
        // Vérifier les limites
        if (textX < r->x) textX = r->x;
        
        display_draw_text(textX, textY, label->text, app->textColor, app->fontSize);
    }
    // --- Affichage avec word wrap ---
    else
    {
        uint16_t charWidth = app->font->charWidth * app->fontSize;
        uint16_t charsPerLine = label->maxWidth / charWidth;
        
        if (charsPerLine == 0) charsPerLine = 1;
        
        uint16_t lineY = r->y;
        uint16_t pos = 0;
        uint16_t len = strlen(label->text);
        
        while (pos < len && lineY < r->y + r->height)
        {
            // Extraire une ligne
            char line[128];
            uint16_t lineLen = (pos + charsPerLine < len) ? charsPerLine : (len - pos);
            
            // Ajuster pour ne pas couper un mot (reculer jusqu'à un espace)
            if (pos + charsPerLine < len && lineLen > 10)
            {
                uint16_t endPos = pos + charsPerLine;
                while (endPos > pos + charsPerLine / 2 && label->text[endPos] != ' ')
                {
                    endPos--;
                }
                if (label->text[endPos] == ' ')
                {
                    lineLen = endPos - pos + 1;  // +1 pour inclure l'espace
                }
            }
            
            strncpy(line, label->text + pos, lineLen);
            line[lineLen] = '\0';
            
            // Calculer la position X selon l'alignement
            uint16_t lineWidth = display_text_width(line, app->fontSize);
            uint16_t textX = r->x;
            
            switch (app->textAlign)
            {
                case UI_ALIGN_CENTER:
                    textX = r->x + (r->width - lineWidth) / 2;
                    break;
                case UI_ALIGN_RIGHT:
                    textX = r->x + r->width - lineWidth;
                    break;
                default:
                    textX = r->x;
                    break;
            }
            
            display_draw_text(textX, lineY, line, app->textColor, app->fontSize);
            
            pos += lineLen;
            // Sauter les espaces en début de ligne suivante
            while (pos < len && label->text[pos] == ' ') pos++;
            
            lineY += lineHeight;
        }
    }
}

// ============================================================
// FONCTIONS DE MISE À JOUR
// ============================================================

/**
 * @brief Mise à jour périodique du label (pour les animations)
 */
static void label_update(UIWidget* widget)
{
    // Les labels n'ont généralement pas besoin de mise à jour périodique
    // Cette fonction peut être utilisée pour des animations de texte
}

// ============================================================
// CRÉATION
// ============================================================

UILabel* ui_label_create(const char* name, const char* text, UIRect rect)
{
    UILabel* label = (UILabel*)calloc(1, sizeof(UILabel));
    if (label == NULL) return NULL;
    
    // --- Initialiser le widget de base ---
    label->base.type = WIDGET_TYPE_LABEL;
    if (name) strncpy(label->base.name, name, 31);
    label->base.rect = rect;
    label->base.visible = true;
    label->base.enabled = true;
    label->base.canFocus = false;  // Les labels ne prennent pas le focus par défaut
    label->base.state = WIDGET_STATE_NORMAL;
    
    // --- Assigner les fonctions virtuelles ---
    label->base.draw = label_draw;
    label->base.onUpdate = label_update;
    label->base.onTouch = NULL;  // Pas d'interaction tactile par défaut
    label->base.onKey = NULL;    // Pas d'interaction clavier par défaut
    
    // --- Texte initial ---
    if (text) strncpy(label->text, text, 255);
    
    // --- Configuration par défaut ---
    label->autoSize = false;
    label->maxWidth = 0;
    label->wordWrap = false;
    
    // --- Style par défaut ---
    ui_label_set_style(label, LABEL_STYLE_BODY);
    
    return label;
}

// ============================================================
// CONFIGURATION
// ============================================================

void ui_label_set_style(UILabel* label, LabelStyle style)
{
    if (label == NULL) return;
    label->appearance = ui_style_get_label(style);
    label->base.needsRedraw = true;
}

void ui_label_set_text(UILabel* label, const char* text)
{
    if (label == NULL) return;
    if (text) strncpy(label->text, text, 255);
    else label->text[0] = '\0';
    label->base.needsRedraw = true;
}

const char* ui_label_get_text(UILabel* label)
{
    if (label == NULL) return "";
    return label->text;
}

void ui_label_set_color(UILabel* label, uint16_t color)
{
    if (label == NULL) return;
    label->appearance.textColor = color;
    label->base.needsRedraw = true;
}

void ui_label_set_alignment(UILabel* label, UIAlign align)
{
    if (label == NULL) return;
    label->appearance.textAlign = align;
    label->base.needsRedraw = true;
}

void ui_label_set_word_wrap(UILabel* label, bool wrap)
{
    if (label == NULL) return;
    label->wordWrap = wrap;
    label->base.needsRedraw = true;
}

void ui_label_set_max_width(UILabel* label, uint16_t maxWidth)
{
    if (label == NULL) return;
    label->maxWidth = maxWidth;
    label->base.needsRedraw = true;
}

void ui_label_set_auto_size(UILabel* label, bool autoSize)
{
    if (label == NULL) return;
    label->autoSize = autoSize;
    label->base.needsRedraw = true;
}

void ui_label_append_text(UILabel* label, const char* text)
{
    if (label == NULL || text == NULL) return;
    
    uint16_t currentLen = strlen(label->text);
    uint16_t appendLen = strlen(text);
    
    if (currentLen + appendLen < 255)
    {
        strncat(label->text, text, 255 - currentLen);
        label->base.needsRedraw = true;
    }
}

void ui_label_clear(UILabel* label)
{
    if (label == NULL) return;
    label->text[0] = '\0';
    label->base.needsRedraw = true;
}

// ============================================================
// FONCTIONS AVANCÉES
// ============================================================

/**
 * @brief Définit un texte formaté (printf-like)
 */
void ui_label_set_text_formatted(UILabel* label, const char* format, ...)
{
    if (label == NULL || format == NULL) return;
    
    va_list args;
    va_start(args, format);
    vsnprintf(label->text, 255, format, args);
    va_end(args);
    
    label->base.needsRedraw = true;
}

/**
 * @brief Anime un défilement de texte horizontal
 */
void ui_label_scroll_text(UILabel* label, uint16_t speedMs)
{
    if (label == NULL) return;
    // Animation de défilement à implémenter si nécessaire
    // Utiliserait label_update() pour l'animation
}

/**
 * @brief Fait clignoter le texte (utile pour les alertes)
 */
void ui_label_blink(UILabel* label, bool enable)
{
    if (label == NULL) return;
    // Clignotement à implémenter si nécessaire
    // Alternerait label->base.visible
}