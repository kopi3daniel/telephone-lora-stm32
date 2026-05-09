/**
 * @file ui_draw_primitives.cpp
 * @brief Implémentation des primitives de dessin
 * 
 * Fonctionnalités :
 * - Lignes (simples, épaisses, pointillées)
 * - Rectangles (simples, arrondis, avec bordures partielles)
 * - Cercles (pleins, anneaux, arcs)
 * - Triangles
 * - Ombres portées
 * - Dégradés (horizontaux, verticaux, radiaux)
 * - Motifs (damier, pointillé)
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_draw_primitives.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <math.h>

// ============================================================
// SECTION 1 : LIGNES
// ============================================================

void ui_draw_hline(uint16_t x1, uint16_t x2, uint16_t y, uint16_t color)
{
    if (x1 > x2) { uint16_t t = x1; x1 = x2; x2 = t; }
    display_fill_rect(x1, y, x2, y, color);
}

void ui_draw_vline(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color)
{
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    display_fill_rect(x, y1, x, y2, color);
}

void ui_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
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
}

void ui_draw_thick_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, 
                         uint8_t thickness, uint16_t color)
{
    // Dessiner plusieurs lignes parallèles
    int16_t dx = (int16_t)x2 - (int16_t)x1;
    int16_t dy = (int16_t)y2 - (int16_t)y1;
    float length = sqrtf(dx * dx + dy * dy);
    
    if (length < 1.0f) return;
    
    float offsetX = -dy / length * (thickness / 2.0f);
    float offsetY = dx / length * (thickness / 2.0f);
    
    for (int8_t i = -thickness/2; i <= thickness/2; i++)
    {
        float t = (float)i / (thickness / 2.0f);
        ui_draw_line(
            (uint16_t)(x1 + offsetX * t), (uint16_t)(y1 + offsetY * t),
            (uint16_t)(x2 + offsetX * t), (uint16_t)(y2 + offsetY * t),
            color
        );
    }
}

void ui_draw_dashed_hline(uint16_t x1, uint16_t x2, uint16_t y, uint16_t color,
                            uint8_t dashLen, uint8_t gapLen)
{
    if (x1 > x2) { uint16_t t = x1; x1 = x2; x2 = t; }
    
    uint16_t x = x1;
    bool dash = true;
    
    while (x <= x2)
    {
        uint16_t segmentLen = dash ? dashLen : gapLen;
        uint16_t endX = x + segmentLen - 1;
        if (endX > x2) endX = x2;
        
        if (dash)
        {
            display_fill_rect(x, y, endX, y, color);
        }
        
        x = endX + 1;
        dash = !dash;
    }
}

void ui_draw_dashed_vline(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color,
                            uint8_t dashLen, uint8_t gapLen)
{
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    
    uint16_t y = y1;
    bool dash = true;
    
    while (y <= y2)
    {
        uint16_t segmentLen = dash ? dashLen : gapLen;
        uint16_t endY = y + segmentLen - 1;
        if (endY > y2) endY = y2;
        
        if (dash)
        {
            display_fill_rect(x, y, x, endY, color);
        }
        
        y = endY + 1;
        dash = !dash;
    }
}

// ============================================================
// SECTION 2 : RECTANGLES
// ============================================================

void ui_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    ui_draw_hline(x, x + w - 1, y, color);
    ui_draw_hline(x, x + w - 1, y + h - 1, color);
    ui_draw_vline(x, y, y + h - 1, color);
    ui_draw_vline(x + w - 1, y, y + h - 1, color);
}

void ui_draw_rect_xy(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    ui_draw_rect(x1, y1, x2 - x1 + 1, y2 - y1 + 1, color);
}

void ui_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    display_fill_rect(x, y, x + w - 1, y + h - 1, color);
}

void ui_fill_rect_xy(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    display_fill_rect(x1, y1, x2, y2, color);
}

void ui_draw_round_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, 
                         uint8_t radius, uint16_t color)
{
    if (radius == 0)
    {
        ui_draw_rect(x, y, w, h, color);
        return;
    }
    
    // Lignes horizontales
    ui_draw_hline(x + radius, x + w - radius - 1, y, color);
    ui_draw_hline(x + radius, x + w - radius - 1, y + h - 1, color);
    
    // Lignes verticales
    ui_draw_vline(x, y + radius, y + h - radius - 1, color);
    ui_draw_vline(x + w - 1, y + radius, y + h - radius - 1, color);
    
    // Arcs aux coins
    int16_t f = 1 - radius;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * radius;
    int16_t xi = 0;
    int16_t yi = radius;
    
    while (xi < yi)
    {
        if (f >= 0) { yi--; ddF_y += 2; f += ddF_y; }
        xi++;
        ddF_x += 2;
        f += ddF_x;
        
        // Coin haut-gauche
        display_draw_pixel(x + radius - xi, y + radius - yi, color);
        display_draw_pixel(x + radius - yi, y + radius - xi, color);
        
        // Coin haut-droit
        display_draw_pixel(x + w - radius - 1 + xi, y + radius - yi, color);
        display_draw_pixel(x + w - radius - 1 + yi, y + radius - xi, color);
        
        // Coin bas-gauche
        display_draw_pixel(x + radius - xi, y + h - radius - 1 + yi, color);
        display_draw_pixel(x + radius - yi, y + h - radius - 1 + xi, color);
        
        // Coin bas-droit
        display_draw_pixel(x + w - radius - 1 + xi, y + h - radius - 1 + yi, color);
        display_draw_pixel(x + w - radius - 1 + yi, y + h - radius - 1 + xi, color);
    }
}

void ui_fill_round_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, 
                         uint8_t radius, uint16_t color)
{
    if (radius == 0)
    {
        ui_fill_rect(x, y, w, h, color);
        return;
    }
    
    // Partie centrale
    ui_fill_rect(x + radius, y, w - 2 * radius, h, color);
    ui_fill_rect(x, y + radius, w, h - 2 * radius, color);
    
    // Coins (cercles pleins)
    ui_fill_circle(x + radius, y + radius, radius, color);
    ui_fill_circle(x + w - radius - 1, y + radius, radius, color);
    ui_fill_circle(x + radius, y + h - radius - 1, radius, color);
    ui_fill_circle(x + w - radius - 1, y + h - radius - 1, radius, color);
}

void ui_draw_round_rect_xy(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, 
                            uint8_t radius, uint16_t color)
{
    if (x1 > x2) { uint16_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    ui_draw_round_rect(x1, y1, x2 - x1 + 1, y2 - y1 + 1, radius, color);
}

void ui_fill_round_rect_xy(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                            uint8_t radius, uint16_t color)
{
    if (x1 > x2) { uint16_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    ui_fill_round_rect(x1, y1, x2 - x1 + 1, y2 - y1 + 1, radius, color);
}

// ============================================================
// SECTION 3 : BORDURES PARTIELLES
// ============================================================

void ui_draw_rect_top_border(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, 
                              uint8_t width, uint16_t color)
{
    ui_fill_rect_xy(x1, y1, x2, y1 + width - 1, color);
}

void ui_draw_rect_bottom_border(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                 uint8_t width, uint16_t color)
{
    ui_fill_rect_xy(x1, y2 - width + 1, x2, y2, color);
}

void ui_draw_rect_left_border(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                               uint8_t width, uint16_t color)
{
    ui_fill_rect_xy(x1, y1, x1 + width - 1, y2, color);
}

void ui_draw_rect_right_border(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                uint8_t width, uint16_t color)
{
    ui_fill_rect_xy(x2 - width + 1, y1, x2, y2, color);
}

// ============================================================
// SECTION 4 : CERCLES
// ============================================================

void ui_draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    int16_t f = 1 - radius;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * radius;
    int16_t x = 0;
    int16_t y = radius;
    
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
        
        display_draw_pixel(x0 + x, y0 + y, color);
        display_draw_pixel(x0 - x, y0 + y, color);
        display_draw_pixel(x0 + x, y0 - y, color);
        display_draw_pixel(x0 - x, y0 - y, color);
        display_draw_pixel(x0 + y, y0 + x, color);
        display_draw_pixel(x0 - y, y0 + x, color);
        display_draw_pixel(x0 + y, y0 - x, color);
        display_draw_pixel(x0 - y, y0 - x, color);
    }
}

void ui_fill_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    for (int16_t y = -radius; y <= (int16_t)radius; y++)
    {
        int16_t x_max = (int16_t)sqrtf((float)(radius * radius - y * y));
        ui_draw_hline(x0 - x_max, x0 + x_max, y0 + y, color);
    }
}

void ui_draw_ring(uint16_t x0, uint16_t y0, uint16_t radius, uint8_t thickness, uint16_t color)
{
    // Dessiner un cercle épais
    for (uint16_t r = radius - thickness/2; r <= radius + thickness/2; r++)
    {
        ui_draw_circle(x0, y0, r, color);
    }
}

void ui_draw_arc(uint16_t x0, uint16_t y0, uint16_t radius, 
                  int16_t startAngle, int16_t endAngle, uint16_t color)
{
    // Normaliser les angles (0-359)
    while (startAngle < 0) startAngle += 360;
    while (endAngle < 0) endAngle += 360;
    if (startAngle > endAngle) { int16_t t = startAngle; startAngle = endAngle; endAngle = t; }
    
    for (int16_t angle = startAngle; angle <= endAngle; angle++)
    {
        float rad = angle * M_PI / 180.0f;
        uint16_t x = x0 + (uint16_t)(radius * cosf(rad));
        uint16_t y = y0 + (uint16_t)(radius * sinf(rad));
        display_draw_pixel(x, y, color);
    }
}

void ui_fill_arc(uint16_t x0, uint16_t y0, uint16_t radius,
                  int16_t startAngle, int16_t endAngle, uint16_t color)
{
    while (startAngle < 0) startAngle += 360;
    while (endAngle < 0) endAngle += 360;
    if (startAngle > endAngle) { int16_t t = startAngle; startAngle = endAngle; endAngle = t; }
    
    for (int16_t angle = startAngle; angle <= endAngle; angle++)
    {
        float rad = angle * M_PI / 180.0f;
        uint16_t x = x0 + (uint16_t)(radius * cosf(rad));
        uint16_t y = y0 + (uint16_t)(radius * sinf(rad));
        ui_draw_line(x0, y0, x, y, color);
    }
}

// ============================================================
// SECTION 5 : TRIANGLES
// ============================================================

void ui_draw_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, 
                       uint16_t x3, uint16_t y3, uint16_t color)
{
    ui_draw_line(x1, y1, x2, y2, color);
    ui_draw_line(x2, y2, x3, y3, color);
    ui_draw_line(x3, y3, x1, y1, color);
}

void ui_fill_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                       uint16_t x3, uint16_t y3, uint16_t color)
{
    // Trier les sommets par Y croissant
    if (y1 > y2) { uint16_t t; t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    if (y2 > y3) { uint16_t t; t = x2; x2 = x3; x3 = t; t = y2; y2 = y3; y3 = t; }
    if (y1 > y2) { uint16_t t; t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    
    // Remplir ligne par ligne
    for (uint16_t y = y1; y <= y3; y++)
    {
        float progress = (y - y1) / (float)(y3 - y1 + 1);
        uint16_t x_left, x_right;
        
        if (y < y2)
        {
            float progress1 = (y - y1) / (float)(y2 - y1 + 1);
            x_left = x1 + (uint16_t)((x2 - x1) * progress1);
            float progress2 = (y - y1) / (float)(y3 - y1 + 1);
            x_right = x1 + (uint16_t)((x3 - x1) * progress2);
        }
        else
        {
            float progress1 = (y - y2) / (float)(y3 - y2 + 1);
            x_left = x2 + (uint16_t)((x3 - x2) * progress1);
            float progress2 = (y - y1) / (float)(y3 - y1 + 1);
            x_right = x1 + (uint16_t)((x3 - x1) * progress2);
        }
        
        if (x_left > x_right) { uint16_t t = x_left; x_left = x_right; x_right = t; }
        ui_draw_hline(x_left, x_right, y, color);
    }
}

// ============================================================
// SECTION 6 : OMBRES
// ============================================================

void ui_draw_shadow_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                          uint8_t radius, uint8_t elevation, uint16_t color)
{
    // Dessiner plusieurs rectangles décalés pour simuler l'ombre
    for (uint8_t i = 0; i < elevation; i++)
    {
        uint8_t alpha = 255 - (i * 255 / elevation);
        uint16_t shadowColor = darken_color_alpha(color, alpha);
        
        ui_fill_round_rect(x + i, y + i, w, h, radius + i, shadowColor);
    }
}

void ui_draw_shadow_circle(uint16_t x0, uint16_t y0, uint16_t radius,
                            uint8_t elevation, uint16_t color)
{
    for (uint8_t i = 0; i < elevation; i++)
    {
        uint8_t alpha = 255 - (i * 255 / elevation);
        uint16_t shadowColor = darken_color_alpha(color, alpha);
        
        ui_fill_circle(x0 + i, y0 + i, radius + i, shadowColor);
    }
}

// ============================================================
// SECTION 7 : DÉGRADÉS
// ============================================================

void ui_fill_rect_gradient_h(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                              uint16_t colorLeft, uint16_t colorRight)
{
    for (uint16_t col = 0; col < w; col++)
    {
        float progress = (float)col / w;
        uint16_t color = interpolate_rgb565(colorLeft, colorRight, progress);
        ui_draw_vline(x + col, y, y + h - 1, color);
    }
}

void ui_fill_rect_gradient_v(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                              uint16_t colorTop, uint16_t colorBottom)
{
    for (uint16_t row = 0; row < h; row++)
    {
        float progress = (float)row / h;
        uint16_t color = interpolate_rgb565(colorTop, colorBottom, progress);
        ui_draw_hline(x, x + w - 1, y + row, color);
    }
}

void ui_fill_circle_gradient(uint16_t x0, uint16_t y0, uint16_t radius,
                               uint16_t colorCenter, uint16_t colorEdge)
{
    for (uint16_t r = 0; r <= radius; r++)
    {
        float progress = (float)r / radius;
        uint16_t color = interpolate_rgb565(colorCenter, colorEdge, progress);
        ui_draw_circle(x0, y0, r, color);
    }
}

// ============================================================
// SECTION 8 : MOTIFS
// ============================================================

void ui_fill_rect_checkerboard(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                uint8_t cellSize, uint16_t color1, uint16_t color2)
{
    for (uint16_t row = 0; row < h; row += cellSize)
    {
        for (uint16_t col = 0; col < w; col += cellSize)
        {
            bool isColor1 = ((row / cellSize + col / cellSize) % 2) == 0;
            uint16_t color = isColor1 ? color1 : color2;
            
            uint16_t cellW = (col + cellSize <= w) ? cellSize : (w - col);
            uint16_t cellH = (row + cellSize <= h) ? cellSize : (h - row);
            
            ui_fill_rect(x + col, y + row, cellW, cellH, color);
        }
    }
}

void ui_fill_rect_dotted(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           uint8_t dotSpacing, uint16_t color)
{
    for (uint16_t row = 0; row < h; row += dotSpacing)
    {
        for (uint16_t col = 0; col < w; col += dotSpacing)
        {
            display_draw_pixel(x + col, y + row, color);
        }
    }
}

// ============================================================
// SECTION 9 : FONCTIONS UTILITAIRES
// ============================================================

static uint16_t interpolate_rgb565(uint16_t color1, uint16_t color2, float progress)
{
    uint8_t r1 = (color1 >> 11) & 0x1F;
    uint8_t g1 = (color1 >> 5) & 0x3F;
    uint8_t b1 = color1 & 0x1F;
    
    uint8_t r2 = (color2 >> 11) & 0x1F;
    uint8_t g2 = (color2 >> 5) & 0x3F;
    uint8_t b2 = color2 & 0x1F;
    
    uint8_t r = (uint8_t)(r1 + (r2 - r1) * progress);
    uint8_t g = (uint8_t)(g1 + (g2 - g1) * progress);
    uint8_t b = (uint8_t)(b1 + (b2 - b1) * progress);
    
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
}

static uint16_t darken_color_alpha(uint16_t color, uint8_t alpha)
{
    // Simuler la transparence en mélangeant avec le noir
    return interpolate_rgb565(0x0000, color, alpha / 255.0f);
}

uint16_t ui_draw_measure_text_width(const UIFont* font, const char* text, uint8_t size)
{
    if (font == NULL || text == NULL) return 0;
    return font->charWidth * size * strlen(text);
}

uint16_t ui_draw_measure_text_height(const UIFont* font, uint8_t size)
{
    if (font == NULL) return 0;
    return font->charHeight * size;
}