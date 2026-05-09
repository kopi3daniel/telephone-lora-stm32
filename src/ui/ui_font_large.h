/**
 * @file ui_font_large.h
 * @brief Grandes polices pour titres et affichages spéciaux
 * 
 * Ce fichier définit des polices de grande taille pour :
 * - Affichage de l'heure (horloge)
 * - Titres d'écran
 * - Numérotation (composeur)
 * - Indicateurs (batterie, signal)
 * 
 * Polices disponibles :
 * - DIGITAL_16x24  : Style digital pour horloge
 * - DIGITAL_24x36  : Style digital large
 * - DIGITAL_32x48  : Style digital très large
 * - BOLD_16x24     : Gras pour titres
 * - NUMERIC_16x24  : Chiffres uniquement, optimisé
 * 
 * Caractères spéciaux :
 * - Icône batterie : ▂▄▆█
 * - Icône signal   : ▂▄▆█
 * - Flèches        : ▲▼◄►
 * - Symboles       : ☎⚙🔊🔋📶
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_FONT_LARGE_H
#define UI_FONT_LARGE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "ui_fonts.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define UI_FONT_LARGE_VERSION           "1.0.0"

/** @brief Nombre maximum de grandes polices */
#define UI_FONT_LARGE_MAX               10

// ============================================================
// SECTION 2 : TYPES DE GRANDES POLICES
// ============================================================

/**
 * @brief Types de grandes polices prédéfinies
 */
typedef enum {
    LARGE_FONT_DIGITAL_16x24    = 0,    // Horloge petite
    LARGE_FONT_DIGITAL_24x36    = 1,    // Horloge moyenne
    LARGE_FONT_DIGITAL_32x48    = 2,    // Horloge grande
    LARGE_FONT_BOLD_16x24       = 3,    // Titres gras
    LARGE_FONT_NUMERIC_16x24    = 4,    // Chiffres optimisés
    LARGE_FONT_TITLE_12x24      = 5,    // Titres standards
    LARGE_FONT_CLOCK_24x36      = 6,    // Horloge avec ":"
    LARGE_FONT_DIALER_20x32     = 7     // Composeur numérique
} LargeFontType;

/**
 * @brief Structure d'une grande police
 */
typedef struct {
    UIFont base;                        // Police de base
    LargeFontType type;                 // Type prédéfini
    bool monospace;                     // Chasse fixe ?
    uint8_t digitWidth;                 // Largeur spécifique des chiffres
    bool proportional;                  // Largeur proportionnelle ?
} LargeFont;

// ============================================================
// SECTION 3 : POLICES PRÉDÉFINIES
// ============================================================

extern const LargeFont LARGE_FONT_DIGITAL_16;
extern const LargeFont LARGE_FONT_DIGITAL_24;
extern const LargeFont LARGE_FONT_DIGITAL_32;
extern const LargeFont LARGE_FONT_BOLD_16;
extern const LargeFont LARGE_FONT_NUMERIC_16;
extern const LargeFont LARGE_FONT_TITLE_12;
extern const LargeFont LARGE_FONT_CLOCK_24;
extern const LargeFont LARGE_FONT_DIALER_20;

// ============================================================
// SECTION 4 : FONCTIONS D'INITIALISATION
// ============================================================

bool ui_font_large_init(void);
void ui_font_large_deinit(void);
bool ui_font_large_is_ready(void);

// ============================================================
// SECTION 5 : FONCTIONS DE POLICES
// ============================================================

const LargeFont* ui_font_large_get(LargeFontType type);
const LargeFont* ui_font_large_get_by_name(const char* name);

// ============================================================
// SECTION 6 : FONCTIONS D'AFFICHAGE SPÉCIALISÉ
// ============================================================

/**
 * @brief Affiche l'heure au format HH:MM
 */
void ui_font_large_draw_time(LargeFontType type, uint16_t x, uint16_t y,
                              uint8_t hours, uint8_t minutes, uint16_t color, bool blinkColon);

/**
 * @brief Affiche l'heure au format HH:MM:SS
 */
void ui_font_large_draw_time_seconds(LargeFontType type, uint16_t x, uint16_t y,
                                      uint8_t hours, uint8_t minutes, uint8_t seconds,
                                      uint16_t color);

/**
 * @brief Affiche un numéro de téléphone centré
 */
void ui_font_large_draw_phone_number(const char* number, uint16_t y, uint16_t color);

/**
 * @brief Affiche la durée d'un appel (MM:SS)
 */
void ui_font_large_draw_call_timer(uint32_t durationSeconds, uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Affiche un grand nombre (pourcentage, niveau)
 */
void ui_font_large_draw_big_number(uint16_t number, uint16_t x, uint16_t y, uint16_t color,
                                    const char* suffix);

/**
 * @brief Affiche une icône de signal (barres)
 */
void ui_font_large_draw_signal_bars(uint16_t x, uint16_t y, uint8_t level, uint16_t color);

/**
 * @brief Affiche une icône de batterie
 */
void ui_font_large_draw_battery_icon(uint16_t x, uint16_t y, uint8_t percent, 
                                      uint16_t color, bool charging);

/**
 * @brief Affiche un indicateur VU-meter
 */
void ui_font_large_draw_vu_meter(uint16_t x, uint16_t y, uint8_t level, 
                                  uint16_t colorLow, uint16_t colorMid, uint16_t colorHigh);

// ============================================================
// SECTION 7 : FONCTIONS DE DÉBOGAGE
// ============================================================

void ui_font_large_print_list(void);
bool ui_font_large_self_test(void);

// ============================================================
// SECTION 8 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define LARGE_FONT_DEBUG(fmt, ...)  printf("[LARGE_FONT] " fmt, ##__VA_ARGS__)
#else
    #define LARGE_FONT_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 9 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_FONT_LARGE_H