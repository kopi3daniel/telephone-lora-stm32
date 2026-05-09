/**
 * @file ui_draw_primitives.h
 * @brief Primitives de dessin pour l'interface utilisateur
 * 
 * Ce fichier définit des fonctions de dessin avancées :
 * - Lignes (horizontales, verticales, épaisses, pointillées)
 * - Rectangles (pleins, contours, arrondis, dégradés)
 * - Cercles (pleins, contours, concentriques)
 * - Triangles (pleins, contours)
 * - Polygones
 * - Courbes de Bézier simples
 * - Ombres portées
 * - Bordures partielles
 * - Motifs de remplissage
 * 
 * Toutes les fonctions utilisent les coordonnées écran
 * et les couleurs au format RGB565.
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_DRAW_PRIMITIVES_H
#define UI_DRAW_PRIMITIVES_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "ui_core.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define UI_DRAW_VERSION                 "1.0.0"

// ============================================================
// SECTION 2 : FONCTIONS DE LIGNES
// ============================================================

void ui_draw_hline(uint16_t x1, uint16_t x2, uint16_t y, uint16_t color);
void ui_draw_vline(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color);
void ui_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void ui_draw_thick_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t thickness, uint16_t color);
void ui_draw_dashed_hline(uint16_t x1, uint16_t x2, uint16_t y, uint16_t color, uint8_t dashLen, uint8_t gapLen);
void ui_draw_dashed_vline(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color, uint8_t dashLen, uint8_t gapLen);

// ============================================================
// SECTION 3 : FONCTIONS DE RECTANGLES
// ============================================================

void ui_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ui_draw_rect_xy(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void ui_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ui_fill_rect_xy(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void ui_draw_round_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t radius, uint16_t color);
void ui_fill_round_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t radius, uint16_t color);
void ui_draw_round_rect_xy(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t radius, uint16_t color);
void ui_fill_round_rect_xy(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t radius, uint16_t color);

// ============================================================
// SECTION 4 : FONCTIONS DE RECTANGLES AVEC BORDURES PARTIELLES
// ============================================================

void ui_draw_rect_top_border(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t width, uint16_t color);
void ui_draw_rect_bottom_border(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t width, uint16_t color);
void ui_draw_rect_left_border(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t width, uint16_t color);
void ui_draw_rect_right_border(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t width, uint16_t color);

// ============================================================
// SECTION 5 : FONCTIONS DE CERCLES
// ============================================================

void ui_draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);
void ui_fill_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);
void ui_draw_ring(uint16_t x0, uint16_t y0, uint16_t radius, uint8_t thickness, uint16_t color);
void ui_draw_arc(uint16_t x0, uint16_t y0, uint16_t radius, int16_t startAngle, int16_t endAngle, uint16_t color);
void ui_fill_arc(uint16_t x0, uint16_t y0, uint16_t radius, int16_t startAngle, int16_t endAngle, uint16_t color);

// ============================================================
// SECTION 6 : FONCTIONS DE TRIANGLES
// ============================================================

void ui_draw_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);
void ui_fill_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);

// ============================================================
// SECTION 7 : FONCTIONS D'OMBRES
// ============================================================

void ui_draw_shadow_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t radius, uint8_t elevation, uint16_t color);
void ui_draw_shadow_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint8_t elevation, uint16_t color);

// ============================================================
// SECTION 8 : FONCTIONS DE DÉGRADÉS
// ============================================================

void ui_fill_rect_gradient_h(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colorLeft, uint16_t colorRight);
void ui_fill_rect_gradient_v(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colorTop, uint16_t colorBottom);
void ui_fill_circle_gradient(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t colorCenter, uint16_t colorEdge);

// ============================================================
// SECTION 9 : FONCTIONS DE MOTIFS
// ============================================================

void ui_fill_rect_checkerboard(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t cellSize, uint16_t color1, uint16_t color2);
void ui_fill_rect_dotted(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t dotSpacing, uint16_t color);

// ============================================================
// SECTION 10 : FONCTIONS DE MESURE
// ============================================================

uint16_t ui_draw_measure_text_width(const UIFont* font, const char* text, uint8_t size);
uint16_t ui_draw_measure_text_height(const UIFont* font, uint8_t size);

// ============================================================
// SECTION 11 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define DRAW_DEBUG(fmt, ...)        printf("[DRAW] " fmt, ##__VA_ARGS__)
#else
    #define DRAW_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 12 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_DRAW_PRIMITIVES_H