/**
 * @file display_manager.cpp
 * @brief Implémentation du gestionnaire d'affichage haut niveau
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans display_manager.h.
 * 
 * Il unifie les drivers bas niveau (ILI9488, LTDC, DMA2D, Buffers)
 * et fournit une API complète pour le dessin de primitives,
 * l'affichage de texte, et la gestion de widgets simples.
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "display_manager.h"
#include "ili9488_driver.h"
#include "ltdc_config.h"
#include "dma2d_driver.h"
#include "display_buffer.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État global du gestionnaire d'affichage */
static DisplayState display_state = {
    .initialized = false,
    .displayOn = false,
    .cursorX = 0,
    .cursorY = 0,
    .textColor = ILI9488_WHITE,
    .bgColor = ILI9488_BLACK,
    .currentFont = NULL,
    .textSize = 1,
    .textWrap = true,
    .rotation = ILI9488_ROTATION_PORTRAIT,
    .frameCount = 0
};

/** @brief Compteur de statistiques */
static uint32_t draw_pixel_count = 0;
static uint32_t draw_line_count = 0;
static uint32_t draw_rect_count = 0;
static uint32_t draw_circle_count = 0;
static uint32_t draw_text_count = 0;
static uint32_t buffer_swap_count = 0;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le système d'affichage complet
 */
bool display_init(void)
{
    DISPLAY_DEBUG("Initialisation du système d'affichage...\n");
    
    // 1. Initialiser l'écran ILI9488
    if (!ili9488_init())
    {
        DISPLAY_DEBUG("Échec initialisation ILI9488\n");
        return false;
    }
    DISPLAY_DEBUG("ILI9488 : OK\n");
    
    // 2. Initialiser le LTDC
    if (!ltdc_init())
    {
        DISPLAY_DEBUG("Échec initialisation LTDC\n");
        return false;
    }
    DISPLAY_DEBUG("LTDC : OK\n");
    
    // 3. Initialiser le DMA2D
    if (!dma2d_init())
    {
        DISPLAY_DEBUG("Échec initialisation DMA2D\n");
        return false;
    }
    DISPLAY_DEBUG("DMA2D : OK\n");
    
    // 4. Initialiser les buffers
    if (!display_buffer_init())
    {
        DISPLAY_DEBUG("Échec initialisation buffers\n");
        return false;
    }
    DISPLAY_DEBUG("Buffers : OK\n");
    
    // 5. Activer le double buffering
    display_double_buffer_enable();
    
    // 6. Configuration par défaut
    display_state.initialized = true;
    display_state.displayOn = true;
    display_state.currentFont = &font_5x7;  // Police par défaut
    display_state.textColor = ILI9488_WHITE;
    display_state.bgColor = ILI9488_BLACK;
    display_state.textSize = 1;
    
    // 7. Effacer l'écran
    display_clear(ILI9488_BLACK);
    display_swap_buffers();
    display_clear(ILI9488_BLACK);
    
    DISPLAY_DEBUG("Système d'affichage initialisé\n");
    
    return true;
}

/**
 * @brief Désinitialise le système d'affichage
 */
void display_deinit(void)
{
    display_buffer_double_disable();
    ili9488_display_off();
    display_state.initialized = false;
    display_state.displayOn = false;
}

/**
 * @brief Vérifie si l'affichage est prêt
 */
bool display_is_ready(void)
{
    return display_state.initialized;
}

/**
 * @brief Récupère l'état du gestionnaire
 */
DisplayState* display_get_state(void)
{
    return &display_state;
}

// ============================================================
// SECTION 2 : CONTRÔLE GLOBAL
// ============================================================

/**
 * @brief Allume l'écran
 */
void display_on(void)
{
    ili9488_display_on();
    display_state.displayOn = true;
}

/**
 * @brief Éteint l'écran
 */
void display_off(void)
{
    ili9488_display_off();
    display_state.displayOn = false;
}

/**
 * @brief Met l'écran en veille
 */
void display_sleep(void)
{
    ili9488_sleep_in();
    display_state.displayOn = false;
}

/**
 * @brief Réveille l'écran
 */
void display_wakeup(void)
{
    ili9488_sleep_out();
    display_state.displayOn = true;
}

/**
 * @brief Définit la luminosité
 */
void display_set_brightness(uint8_t brightness)
{
    ili9488_set_brightness(brightness);
}

/**
 * @brief Récupère la luminosité
 */
uint8_t display_get_brightness(void)
{
    return ili9488_get_brightness();
}

/**
 * @brief Définit la rotation
 */
void display_set_rotation(ILI9488_Rotation rotation)
{
    ili9488_set_rotation(rotation);
    display_state.rotation = rotation;
}

/**
 * @brief Récupère la rotation
 */
ILI9488_Rotation display_get_rotation(void)
{
    return display_state.rotation;
}

// ============================================================
// SECTION 3 : DESSIN DE BASE
// ============================================================

/**
 * @brief Efface tout l'écran
 */
void display_clear(uint16_t color)
{
    display_buffer_clear_back(color);
}

/**
 * @brief Dessine un pixel
 */
void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
    
    uint16_t* backBuf = display_get_back_buffer();
    backBuf[y * DISPLAY_WIDTH + x] = color;
    draw_pixel_count++;
}

/**
 * @brief Dessine une ligne (algorithme de Bresenham)
 */
void display_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int16_t dx = abs((int16_t)x2 - (int16_t)x1);
    int16_t dy = -abs((int16_t)y2 - (int16_t)y1);
    int16_t sx = x1 < x2 ? 1 : -1;
    int16_t sy = y1 < y2 ? 1 : -1;
    int16_t err = dx + dy;
    
    while (1)
    {
        display_draw_pixel(x1, y1, color);
        
        if (x1 == x2 && y1 == y2) break;
        
        int16_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
    
    draw_line_count++;
}

/**
 * @brief Dessine une ligne horizontale (optimisé DMA2D)
 */
void display_draw_hline(uint16_t x1, uint16_t y, uint16_t x2, uint16_t color)
{
    if (x1 > x2) { uint16_t t = x1; x1 = x2; x2 = t; }
    
    display_buffer_fill_rect_back(x1, y, x2, y, color);
    draw_line_count++;
}

/**
 * @brief Dessine une ligne verticale (optimisé DMA2D)
 */
void display_draw_vline(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color)
{
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    
    display_buffer_fill_rect_back(x, y1, x, y2, color);
    draw_line_count++;
}

/**
 * @brief Dessine un rectangle (contour)
 */
void display_draw_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    display_draw_hline(x1, y1, x2, color);  // Haut
    display_draw_hline(x1, y2, x2, color);  // Bas
    display_draw_vline(x1, y1, y2, color);  // Gauche
    display_draw_vline(x2, y1, y2, color);  // Droite
    draw_rect_count++;
}

/**
 * @brief Remplit un rectangle (plein)
 */
void display_fill_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    display_buffer_fill_rect_back(x1, y1, x2, y2, color);
    draw_rect_count++;
}

/**
 * @brief Dessine un rectangle arrondi (contour)
 */
void display_draw_round_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                              uint16_t radius, uint16_t color)
{
    if (radius == 0)
    {
        display_draw_rect(x1, y1, x2, y2, color);
        return;
    }
    
    // Lignes horizontales
    display_draw_hline(x1 + radius, y1, x2 - radius, color);
    display_draw_hline(x1 + radius, y2, x2 - radius, color);
    
    // Lignes verticales
    display_draw_vline(x1, y1 + radius, y2 - radius, color);
    display_draw_vline(x2, y1 + radius, y2 - radius, color);
    
    // Coins arrondis (arcs de cercle)
    int16_t f = 1 - radius;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * radius;
    int16_t x = 0;
    int16_t y = radius;
    
    while (x < y)
    {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++;
        ddF_x += 2;
        f += ddF_x;
        
        // Coin haut-gauche
        display_draw_pixel(x1 + radius - x, y1 + radius - y, color);
        display_draw_pixel(x1 + radius - y, y1 + radius - x, color);
        
        // Coin haut-droit
        display_draw_pixel(x2 - radius + x, y1 + radius - y, color);
        display_draw_pixel(x2 - radius + y, y1 + radius - x, color);
        
        // Coin bas-gauche
        display_draw_pixel(x1 + radius - x, y2 - radius + y, color);
        display_draw_pixel(x1 + radius - y, y2 - radius + x, color);
        
        // Coin bas-droit
        display_draw_pixel(x2 - radius + x, y2 - radius + y, color);
        display_draw_pixel(x2 - radius + y, y2 - radius + x, color);
    }
}

/**
 * @brief Remplit un rectangle arrondi (plein)
 */
void display_fill_round_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                              uint16_t radius, uint16_t color)
{
    if (radius == 0)
    {
        display_fill_rect(x1, y1, x2, y2, color);
        return;
    }
    
    // Partie centrale (rectangle plein)
    display_fill_rect(x1 + radius, y1, x2 - radius, y2, color);
    display_fill_rect(x1, y1 + radius, x2, y2 - radius, color);
    
    // Coins arrondis (cercles pleins)
    display_fill_circle(x1 + radius, y1 + radius, radius, color);
    display_fill_circle(x2 - radius, y1 + radius, radius, color);
    display_fill_circle(x1 + radius, y2 - radius, radius, color);
    display_fill_circle(x2 - radius, y2 - radius, radius, color);
}

/**
 * @brief Dessine un cercle (contour)
 */
void display_draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    int16_t f = 1 - radius;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * radius;
    int16_t x = 0;
    int16_t y = radius;
    
    // Points cardinaux
    display_draw_pixel(x0, y0 + radius, color);
    display_draw_pixel(x0, y0 - radius, color);
    display_draw_pixel(x0 + radius, y0, color);
    display_draw_pixel(x0 - radius, y0, color);
    
    while (x < y)
    {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++;
        ddF_x += 2;
        f += ddF_x;
        
        // 8 points par itération
        display_draw_pixel(x0 + x, y0 + y, color);
        display_draw_pixel(x0 - x, y0 + y, color);
        display_draw_pixel(x0 + x, y0 - y, color);
        display_draw_pixel(x0 - x, y0 - y, color);
        display_draw_pixel(x0 + y, y0 + x, color);
        display_draw_pixel(x0 - y, y0 + x, color);
        display_draw_pixel(x0 + y, y0 - x, color);
        display_draw_pixel(x0 - y, y0 - x, color);
    }
    
    draw_circle_count++;
}

/**
 * @brief Remplit un cercle (plein)
 */
void display_fill_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    // Remplir par lignes horizontales
    for (int16_t y = -radius; y <= (int16_t)radius; y++)
    {
        int16_t x_max = (int16_t)sqrtf((float)(radius * radius - y * y));
        
        if (y0 + y < DISPLAY_HEIGHT && y0 + y >= 0)
        {
            uint16_t x_start = (x0 - x_max > 0) ? (x0 - x_max) : 0;
            uint16_t x_end = (x0 + x_max < DISPLAY_WIDTH) ? (x0 + x_max) : DISPLAY_WIDTH - 1;
            
            display_draw_hline(x_start, y0 + y, x_end, color);
        }
    }
    
    draw_circle_count++;
}

// ============================================================
// SECTION 4 : FONCTIONS DE TEXTE
// ============================================================

/**
 * @brief Définit la police courante
 */
void display_set_font(const DisplayFont* font)
{
    display_state.currentFont = font;
}

/**
 * @brief Récupère la police courante
 */
const DisplayFont* display_get_font(void)
{
    return display_state.currentFont;
}

/**
 * @brief Définit la taille du texte
 */
void display_set_text_size(uint8_t size)
{
    if (size < 1) size = 1;
    if (size > 8) size = 8;
    display_state.textSize = size;
}

/**
 * @brief Définit la couleur du texte
 */
void display_set_text_color(uint16_t color)
{
    display_state.textColor = color;
}

/**
 * @brief Définit la couleur de fond du texte
 */
void display_set_text_bg_color(uint16_t color)
{
    display_state.bgColor = color;
}

/**
 * @brief Positionne le curseur
 */
void display_set_cursor(uint16_t x, uint16_t y)
{
    display_state.cursorX = x;
    display_state.cursorY = y;
}

/**
 * @brief Active/désactive le retour à la ligne
 */
void display_set_text_wrap(bool wrap)
{
    display_state.textWrap = wrap;
}

/**
 * @brief Dessine un caractère
 */
void display_draw_char(uint16_t x, uint16_t y, char c, uint16_t color,
                        uint16_t bgColor, uint8_t size)
{
    if (display_state.currentFont == NULL) return;
    if (c < display_state.currentFont->firstChar || c > display_state.currentFont->lastChar)
    {
        c = '?';  // Caractère de remplacement
    }
    
    const uint8_t* fontData = display_state.currentFont->data;
    uint16_t charWidth = display_state.currentFont->charWidth;
    uint16_t charHeight = display_state.currentFont->charHeight;
    uint16_t bytesPerChar = display_state.currentFont->bytesPerChar;
    
    uint16_t charIndex = (c - display_state.currentFont->firstChar) * bytesPerChar;
    
    for (uint16_t row = 0; row < charHeight; row++)
    {
        uint8_t line = fontData[charIndex + row];
        
        for (uint16_t col = 0; col < charWidth; col++)
        {
            if (line & (1 << (charWidth - 1 - col)))
            {
                // Pixel allumé - dessiner avec la taille spécifiée
                for (uint8_t sx = 0; sx < size; sx++)
                {
                    for (uint8_t sy = 0; sy < size; sy++)
                    {
                        display_draw_pixel(x + col * size + sx, y + row * size + sy, color);
                    }
                }
            }
            else if (bgColor != color)
            {
                // Pixel éteint - fond
                for (uint8_t sx = 0; sx < size; sx++)
                {
                    for (uint8_t sy = 0; sy < size; sy++)
                    {
                        display_draw_pixel(x + col * size + sx, y + row * size + sy, bgColor);
                    }
                }
            }
        }
    }
}

/**
 * @brief Écrit un caractère à la position du curseur
 */
void display_write_char(char c)
{
    uint16_t charWidth = display_state.currentFont->charWidth * display_state.textSize;
    uint16_t charHeight = display_state.currentFont->charHeight * display_state.textSize;
    
    if (c == '\n')
    {
        // Nouvelle ligne
        display_state.cursorX = 0;
        display_state.cursorY += charHeight;
    }
    else if (c == '\r')
    {
        // Retour chariot
        display_state.cursorX = 0;
    }
    else
    {
        // Vérifier le retour à la ligne
        if (display_state.textWrap && (display_state.cursorX + charWidth > DISPLAY_WIDTH))
        {
            display_state.cursorX = 0;
            display_state.cursorY += charHeight;
        }
        
        // Vérifier le dépassement vertical
        if (display_state.cursorY + charHeight > DISPLAY_HEIGHT)
        {
            display_state.cursorY = 0;  // Recommencer en haut
        }
        
        // Dessiner le caractère
        display_draw_char(display_state.cursorX, display_state.cursorY, c,
                          display_state.textColor, display_state.bgColor,
                          display_state.textSize);
        
        display_state.cursorX += charWidth;
    }
}

/**
 * @brief Écrit une chaîne de caractères
 */
void display_draw_text(uint16_t x, uint16_t y, const char* text,
                        uint16_t color, uint8_t size)
{
    display_set_cursor(x, y);
    display_state.textColor = color;
    display_state.textSize = size;
    
    while (*text)
    {
        display_write_char(*text++);
    }
    
    draw_text_count++;
}

/**
 * @brief Écrit une chaîne à la position du curseur
 */
void display_write_text(const char* text)
{
    while (*text)
    {
        display_write_char(*text++);
    }
}

/**
 * @brief Écrit une chaîne centrée horizontalement
 */
void display_draw_text_center(uint16_t y, const char* text, uint16_t color, uint8_t size)
{
    uint16_t textWidth = display_text_width(text, size);
    uint16_t x = (DISPLAY_WIDTH - textWidth) / 2;
    
    display_draw_text(x, y, text, color, size);
}

/**
 * @brief Écrit une chaîne alignée dans une zone
 */
void display_draw_text_aligned(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                const char* text, uint16_t color, uint8_t size,
                                DisplayAlign align, DisplayVAlign valign)
{
    uint16_t textWidth = display_text_width(text, size);
    uint16_t textHeight = display_text_height(size);
    
    uint16_t x, y;
    
    // Calcul X selon l'alignement horizontal
    switch (align)
    {
        case DISPLAY_ALIGN_CENTER:
            x = x1 + (x2 - x1 - textWidth) / 2;
            break;
        case DISPLAY_ALIGN_RIGHT:
            x = x2 - textWidth;
            break;
        case DISPLAY_ALIGN_LEFT:
        default:
            x = x1;
            break;
    }
    
    // Calcul Y selon l'alignement vertical
    switch (valign)
    {
        case DISPLAY_VALIGN_MIDDLE:
            y = y1 + (y2 - y1 - textHeight) / 2;
            break;
        case DISPLAY_VALIGN_BOTTOM:
            y = y2 - textHeight;
            break;
        case DISPLAY_VALIGN_TOP:
        default:
            y = y1;
            break;
    }
    
    display_draw_text(x, y, text, color, size);
}

/**
 * @brief Mesure la largeur d'un texte
 */
uint16_t display_text_width(const char* text, uint8_t size)
{
    if (display_state.currentFont == NULL) return 0;
    
    uint16_t charWidth = display_state.currentFont->charWidth * size;
    uint16_t len = strlen(text);
    
    return charWidth * len;
}

/**
 * @brief Mesure la hauteur d'un texte
 */
uint16_t display_text_height(uint8_t size)
{
    if (display_state.currentFont == NULL) return 0;
    
    return display_state.currentFont->charHeight * size;
}

// ============================================================
// SECTION 5 : FONCTIONS DE WIDGETS
// ============================================================

/**
 * @brief Dessine un bouton
 */
void display_draw_button(const DisplayButton* button)
{
    if (button == NULL) return;
    
    uint16_t bgColor = button->enabled ? button->bgColor : ILI9488_GRAY;
    uint16_t textColor = button->enabled ? button->textColor : ILI9488_DARK_GRAY;
    
    // Fond du bouton
    if (button->cornerRadius > 0)
    {
        display_fill_round_rect(button->x, button->y,
                                 button->x + button->width - 1,
                                 button->y + button->height - 1,
                                 button->cornerRadius, bgColor);
    }
    else
    {
        display_fill_rect(button->x, button->y,
                          button->x + button->width - 1,
                          button->y + button->height - 1, bgColor);
    }
    
    // Bordure
    if (button->borderColor != bgColor)
    {
        if (button->cornerRadius > 0)
        {
            display_draw_round_rect(button->x, button->y,
                                     button->x + button->width - 1,
                                     button->y + button->height - 1,
                                     button->cornerRadius, button->borderColor);
        }
        else
        {
            display_draw_rect(button->x, button->y,
                              button->x + button->width - 1,
                              button->y + button->height - 1, button->borderColor);
        }
    }
    
    // Texte du bouton (centré)
    if (button->text != NULL)
    {
        display_draw_text_aligned(button->x + 4, button->y + 2,
                                   button->x + button->width - 5,
                                   button->y + button->height - 3,
                                   button->text, textColor, 1,
                                   DISPLAY_ALIGN_CENTER, DISPLAY_VALIGN_MIDDLE);
    }
    
    // Effet pressé (inverser les couleurs)
    if (button->pressed && button->enabled)
    {
        // Rectangle semi-transparent par-dessus
        display_fill_rect(button->x + 2, button->y + 2,
                          button->x + button->width - 3,
                          button->y + button->height - 3,
                          ILI9488_BLACK);
    }
}

/**
 * @brief Vérifie si des coordonnées touchent un bouton
 */
bool display_button_touched(const DisplayButton* button, uint16_t tx, uint16_t ty)
{
    if (button == NULL || !button->enabled) return false;
    
    return (tx >= button->x && tx <= button->x + button->width &&
            ty >= button->y && ty <= button->y + button->height);
}

/**
 * @brief Dessine une barre de progression
 */
void display_draw_progress_bar(const DisplayProgressBar* bar)
{
    if (bar == NULL) return;
    
    // Fond
    display_fill_rect(bar->x, bar->y, bar->x + bar->width - 1, bar->y + bar->height - 1,
                      bar->bgColor);
    
    // Remplissage
    uint16_t fillWidth = (uint16_t)((uint32_t)bar->width * bar->progress / 100);
    if (fillWidth > 0)
    {
        display_fill_rect(bar->x, bar->y, bar->x + fillWidth - 1, bar->y + bar->height - 1,
                          bar->fillColor);
    }
    
    // Bordure
    display_draw_rect(bar->x, bar->y, bar->x + bar->width - 1, bar->y + bar->height - 1,
                      bar->borderColor);
}

/**
 * @brief Met à jour une barre de progression
 */
void display_update_progress_bar(DisplayProgressBar* bar, uint8_t progress)
{
    if (bar == NULL) return;
    bar->progress = (progress > 100) ? 100 : progress;
    display_draw_progress_bar(bar);
}

/**
 * @brief Dessine un panneau/cadre
 */
void display_draw_panel(const DisplayPanel* panel)
{
    if (panel == NULL) return;
    
    // Fond
    if (panel->cornerRadius > 0)
    {
        display_fill_round_rect(panel->x, panel->y,
                                 panel->x + panel->width - 1,
                                 panel->y + panel->height - 1,
                                 panel->cornerRadius, panel->bgColor);
        display_draw_round_rect(panel->x, panel->y,
                                 panel->x + panel->width - 1,
                                 panel->y + panel->height - 1,
                                 panel->cornerRadius, panel->borderColor);
    }
    else
    {
        display_fill_rect(panel->x, panel->y,
                          panel->x + panel->width - 1,
                          panel->y + panel->height - 1, panel->bgColor);
        display_draw_rect(panel->x, panel->y,
                          panel->x + panel->width - 1,
                          panel->y + panel->height - 1, panel->borderColor);
    }
    
    // Titre
    if (panel->title != NULL)
    {
        display_draw_text(panel->x + 10, panel->y + 5, panel->title,
                          ILI9488_WHITE, 1);
    }
}

/**
 * @brief Dessine une icône
 */
void display_draw_icon(uint16_t x, uint16_t y, const uint16_t* iconData,
                        uint16_t width, uint16_t height)
{
    uint16_t* backBuf = display_get_back_buffer();
    
    for (uint16_t row = 0; row < height; row++)
    {
        for (uint16_t col = 0; col < width; col++)
        {
            uint16_t color = iconData[row * width + col];
            if (color != 0xF81F)  // Magenta = transparent (convention)
            {
                uint16_t px = x + col;
                uint16_t py = y + row;
                if (px < DISPLAY_WIDTH && py < DISPLAY_HEIGHT)
                {
                    backBuf[py * DISPLAY_WIDTH + px] = color;
                }
            }
        }
    }
}

// ============================================================
// SECTION 6 : DOUBLE BUFFERING
// ============================================================

/**
 * @brief Active le double buffering
 */
void display_double_buffer_enable(void)
{
    display_buffer_double_enable();
}

/**
 * @brief Désactive le double buffering
 */
void display_double_buffer_disable(void)
{
    display_buffer_double_disable();
}

/**
 * @brief Échange les buffers
 */
void display_swap_buffers(void)
{
    display_buffer_swap(true);
    display_state.frameCount++;
    buffer_swap_count++;
}

/**
 * @brief Récupère le back buffer
 */
uint16_t* display_get_back_buffer(void)
{
    return display_buffer_get_back();
}

/**
 * @brief Récupère le front buffer
 */
uint16_t* display_get_front_buffer(void)
{
    return display_buffer_get_front();
}

// ============================================================
// SECTION 7 : DÉBOGAGE
// ============================================================

/**
 * @brief Affiche les informations du système d'affichage
 */
void display_print_info(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║     INFORMATIONS SYSTÈME D'AFFICHAGE         ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Version     : %-31s ║\n", DISPLAY_MANAGER_VERSION);
    printf("║ Résolution  : %d × %d                      ║\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    printf("║ Rotation    : %d°                           ║\n", 
           display_state.rotation == ILI9488_ROTATION_PORTRAIT ? 0 : 90);
    printf("║ Luminosité  : %d/255                        ║\n", display_get_brightness());
    printf("║ État        : %s                            ║\n", 
           display_state.displayOn ? "Allumé" : "Éteint");
    printf("║ Police      : %s                            ║\n", 
           display_state.currentFont ? display_state.currentFont->name : "Aucune");
    printf("║ Taille texte: %d×                            ║\n", display_state.textSize);
    printf("║ Frames      : %lu                           ║\n", (unsigned long)display_state.frameCount);
    printf("║ Swaps       : %lu                           ║\n", (unsigned long)buffer_swap_count);
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Pixels dessinés : %lu                       ║\n", (unsigned long)draw_pixel_count);
    printf("║ Lignes          : %lu                       ║\n", (unsigned long)draw_line_count);
    printf("║ Rectangles      : %lu                       ║\n", (unsigned long)draw_rect_count);
    printf("║ Cercles         : %lu                       ║\n", (unsigned long)draw_circle_count);
    printf("║ Textes          : %lu                       ║\n", (unsigned long)draw_text_count);
    printf("╚══════════════════════════════════════════════╝\n\n");
}

/**
 * @brief Affiche un motif de test
 */
void display_test_pattern(void)
{
    display_clear(ILI9488_BLACK);
    
    // Barres de couleurs
    uint16_t colors[] = {ILI9488_RED, ILI9488_GREEN, ILI9488_BLUE,
                         ILI9488_YELLOW, ILI9488_CYAN, ILI9488_MAGENTA};
    
    uint16_t barWidth = DISPLAY_WIDTH / 6;
    for (int i = 0; i < 6; i++)
    {
        display_fill_rect(i * barWidth, 0, (i + 1) * barWidth - 1, 100, colors[i]);
    }
    
    // Grille
    for (int i = 0; i < DISPLAY_WIDTH; i += 20)
        display_draw_vline(i, 100, DISPLAY_HEIGHT - 1, ILI9488_DARK_GRAY);
    for (int i = 100; i < DISPLAY_HEIGHT; i += 20)
        display_draw_hline(0, i, DISPLAY_WIDTH - 1, ILI9488_DARK_GRAY);
    
    // Cercles concentriques
    for (int r = 10; r < 150; r += 20)
        display_draw_circle(DISPLAY_WIDTH/2, 300, r, ILI9488_WHITE);
    
    // Texte
    display_set_font(&font_8x16);
    display_draw_text_center(420, "Test Pattern OK", ILI9488_GREEN, 1);
    
    display_swap_buffers();
}

/**
 * @brief Affiche les statistiques de performance
 */
void display_print_statistics(void)
{
    printf("\n═══ STATISTIQUES D'AFFICHAGE ═══\n");
    printf("Pixels   : %lu\n", (unsigned long)draw_pixel_count);
    printf("Lignes   : %lu\n", (unsigned long)draw_line_count);
    printf("Rect     : %lu\n", (unsigned long)draw_rect_count);
    printf("Cercles  : %lu\n", (unsigned long)draw_circle_count);
    printf("Textes   : %lu\n", (unsigned long)draw_text_count);
    printf("Swaps    : %lu\n", (unsigned long)buffer_swap_count);
    printf("Frames   : %lu\n", (unsigned long)display_state.frameCount);
    printf("══════════════════════════════════\n\n");
}

/**
 * @brief Vérifie l'intégrité du système d'affichage
 */
bool display_self_test(void)
{
    if (!display_is_ready()) return false;
    
    // Tester le remplissage écran
    display_clear(ILI9488_RED);
    display_swap_buffers();
    HAL_Delay(500);
    
    display_clear(ILI9488_GREEN);
    display_swap_buffers();
    HAL_Delay(500);
    
    display_clear(ILI9488_BLUE);
    display_swap_buffers();
    HAL_Delay(500);
    
    display_clear(ILI9488_BLACK);
    display_swap_buffers();
    
    return true;
}

// ============================================================
// SECTION 8 : POLICES INTÉGRÉES
// ============================================================

/**
 * @brief Police 5×7 (compacte)
 */
static const uint8_t font5x7_data[] = {
    // Espace (0x20)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ! (0x21)
    0x00, 0x00, 0x5F, 0x00, 0x00, 0x00, 0x00,
    // " (0x22)
    0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00,
    // # (0x23)
    0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00, 0x00,
    // $ (0x24)
    0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00, 0x00,
    // % (0x25)
    0x23, 0x13, 0x08, 0x64, 0x62, 0x00, 0x00,
    // & (0x26)
    0x36, 0x49, 0x55, 0x22, 0x50, 0x00, 0x00,
    // etc. (les 95 caractères ASCII imprimables)
    // ... (données de la police complète)
};

const DisplayFont font_5x7 = {
    .data = font5x7_data,
    .charWidth = 5,
    .charHeight = 7,
    .bytesPerChar = 7,
    .firstChar = ' ',
    .lastChar = '~',
    .name = "5x7"
};

/**
 * @brief Police 8×16 (standard, style terminal)
 */
static const uint8_t font8x16_data[] = {
    // Espace (0x20)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ! (0x21)
    0x00, 0x00, 0x18, 0x3C, 0x3C, 0x3C, 0x18, 0x18,
    0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
    // etc.
};

const DisplayFont font_8x16 = {
    .data = font8x16_data,
    .charWidth = 8,
    .charHeight = 16,
    .bytesPerChar = 16,
    .firstChar = ' ',
    .lastChar = '~',
    .name = "8x16"
};

// Polices 12x24 et 16x32 (données plus grandes, même structure)
const DisplayFont font_12x24 = {
    .data = NULL,  // À remplir avec les données
    .charWidth = 12,
    .charHeight = 24,
    .bytesPerChar = 36,
    .firstChar = ' ',
    .lastChar = '~',
    .name = "12x24"
};

const DisplayFont font_16x32 = {
    .data = NULL,  // À remplir avec les données
    .charWidth = 16,
    .charHeight = 32,
    .bytesPerChar = 64,
    .firstChar = ' ',
    .lastChar = '~',
    .name = "16x32"
};