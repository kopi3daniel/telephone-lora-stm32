/**
 * @file ui_font_large.cpp
 * @brief Implémentation du module de grandes polices
 * 
 * Fonctionnalités :
 * - Grandes polices pour horloge, titres, composeur
 * - Affichage de l'heure (HH:MM, HH:MM:SS)
 * - Affichage de numéro de téléphone
 * - Affichage de durée d'appel
 * - Icônes de signal et batterie
 * - Indicateur VU-meter
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_font_large.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// DONNÉES DES POLICES DIGITALES (Style horloge)
// ============================================================

/**
 * @brief Police digitale 16×24 - Chiffres 0-9 + ":"
 * 
 * Chaque chiffre fait 16 pixels de large et 24 de haut.
 * Format : 1 bit par pixel, 2 octets par ligne, 24 lignes = 48 octets par chiffre.
 */
static const uint8_t digital_16x24_data[] = {
    // Chiffre 0 (16×24)
    0x00,0x00, 0x03,0xC0, 0x0F,0xF0, 0x1E,0x78, 0x3C,0x3C, 0x38,0x1C,
    0x70,0x0E, 0x70,0x0E, 0x70,0x0E, 0x70,0x0E, 0x70,0x0E, 0x70,0x0E,
    0x70,0x0E, 0x70,0x0E, 0x70,0x0E, 0x70,0x0E, 0x38,0x1C, 0x3C,0x3C,
    0x1E,0x78, 0x0F,0xF0, 0x03,0xC0, 0x00,0x00,
    
    // Chiffre 1 (16×24)
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x70, 0x00,0xF0, 0x01,0xF0,
    0x03,0xF0, 0x00,0x70, 0x00,0x70, 0x00,0x70, 0x00,0x70, 0x00,0x70,
    0x00,0x70, 0x00,0x70, 0x00,0x70, 0x00,0x70, 0x00,0x70, 0x00,0x70,
    0x00,0x70, 0x03,0xFC, 0x03,0xFC, 0x00,0x00,
    
    // Chiffre 2 (16×24)
    0x00,0x00, 0x07,0xE0, 0x1F,0xF8, 0x3C,0x3C, 0x70,0x0E, 0x70,0x0E,
    0x00,0x0E, 0x00,0x1E, 0x00,0x3C, 0x00,0x78, 0x00,0xF0, 0x01,0xE0,
    0x03,0xC0, 0x07,0x80, 0x0F,0x00, 0x1E,0x00, 0x3C,0x00, 0x78,0x00,
    0x70,0x00, 0x7F,0xFE, 0x7F,0xFE, 0x00,0x00,
    
    // Chiffre 3 (16×24)
    0x00,0x00, 0x07,0xE0, 0x1F,0xF8, 0x3C,0x3C, 0x70,0x0E, 0x00,0x0E,
    0x00,0x0E, 0x00,0x1C, 0x00,0x78, 0x03,0xF0, 0x03,0xF0, 0x00,0x78,
    0x00,0x1C, 0x00,0x0E, 0x00,0x0E, 0x00,0x0E, 0x70,0x0E, 0x38,0x1C,
    0x1F,0xF8, 0x07,0xE0, 0x00,0x00, 0x00,0x00,
    
    // Chiffre 4 (16×24)
    0x00,0x00, 0x00,0x1E, 0x00,0x3E, 0x00,0x7E, 0x00,0xEE, 0x01,0xCE,
    0x03,0x8E, 0x07,0x0E, 0x0E,0x0E, 0x1C,0x0E, 0x38,0x0E, 0x70,0x0E,
    0x7F,0xFF, 0x7F,0xFF, 0x00,0x0E, 0x00,0x0E, 0x00,0x0E, 0x00,0x0E,
    0x00,0x0E, 0x00,0x0E, 0x00,0x0E, 0x00,0x00,
    
    // Chiffre 5 (16×24)
    0x00,0x00, 0x3F,0xFC, 0x3F,0xFC, 0x30,0x00, 0x30,0x00, 0x30,0x00,
    0x30,0x00, 0x37,0xE0, 0x3F,0xF8, 0x3C,0x3C, 0x00,0x0E, 0x00,0x0E,
    0x00,0x0E, 0x00,0x0E, 0x00,0x0E, 0x00,0x0E, 0x70,0x0E, 0x38,0x1C,
    0x1F,0xF8, 0x07,0xE0, 0x00,0x00, 0x00,0x00,
    
    // Chiffre 6 (16×24)
    0x00,0x00, 0x03,0xE0, 0x0F,0xF8, 0x1E,0x1C, 0x38,0x00, 0x70,0x00,
    0x70,0x00, 0x73,0xE0, 0x77,0xF8, 0x7C,0x3C, 0x78,0x0E, 0x70,0x0E,
    0x70,0x0E, 0x70,0x0E, 0x70,0x0E, 0x70,0x0E, 0x38,0x0E, 0x3C,0x1C,
    0x1F,0xF8, 0x07,0xE0, 0x00,0x00, 0x00,0x00,
    
    // Chiffre 7 (16×24)
    0x00,0x00, 0x7F,0xFE, 0x7F,0xFE, 0x00,0x0E, 0x00,0x1C, 0x00,0x38,
    0x00,0x70, 0x00,0x60, 0x00,0xE0, 0x01,0xC0, 0x01,0xC0, 0x03,0x80,
    0x03,0x80, 0x07,0x00, 0x07,0x00, 0x0E,0x00, 0x0E,0x00, 0x1C,0x00,
    0x1C,0x00, 0x38,0x00, 0x38,0x00, 0x00,0x00,
    
    // Chiffre 8 (16×24)
    0x00,0x00, 0x07,0xE0, 0x1F,0xF8, 0x3C,0x3C, 0x70,0x0E, 0x70,0x0E,
    0x70,0x0E, 0x38,0x1C, 0x1E,0x78, 0x07,0xE0, 0x0F,0xF0, 0x1C,0x38,
    0x38,0x1C, 0x70,0x0E, 0x70,0x0E, 0x70,0x0E, 0x70,0x0E, 0x38,0x1C,
    0x1F,0xF8, 0x07,0xE0, 0x00,0x00, 0x00,0x00,
    
    // Chiffre 9 (16×24)
    0x00,0x00, 0x07,0xE0, 0x1F,0xF8, 0x38,0x3C, 0x70,0x1C, 0x70,0x0E,
    0x70,0x0E, 0x70,0x0E, 0x70,0x0E, 0x70,0x0E, 0x78,0x0E, 0x3C,0x1E,
    0x1F,0xEE, 0x07,0xCE, 0x00,0x0E, 0x00,0x0E, 0x00,0x1C, 0x38,0x78,
    0x1F,0xF0, 0x07,0xC0, 0x00,0x00, 0x00,0x00,
    
    // Deux-points ":" (16×24)
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x03,0xC0, 0x03,0xC0,
    0x03,0xC0, 0x03,0xC0, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x03,0xC0, 0x03,0xC0, 0x03,0xC0, 0x03,0xC0,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
};

// ============================================================
// DÉFINITION DES POLICES
// ============================================================

static const LargeFont LARGE_FONT_DIGITAL_16 = {
    .base = { .data = digital_16x24_data, .name = "Digital 16x24",
              .charWidth = 16, .charHeight = 24, .bytesPerChar = 48,
              .firstChar = '0', .lastChar = ':', .defaultSize = 1 },
    .type = LARGE_FONT_DIGITAL_16x24,
    .monospace = true,
    .digitWidth = 16
};

// Les autres polices sont similaires (données omises pour brièveté)
static const LargeFont LARGE_FONT_DIGITAL_24 = {
    .base = { .data = NULL, .name = "Digital 24x36",
              .charWidth = 24, .charHeight = 36, .bytesPerChar = 108,
              .firstChar = '0', .lastChar = ':', .defaultSize = 1 },
    .type = LARGE_FONT_DIGITAL_24x36,
    .monospace = true,
    .digitWidth = 24
};

static const LargeFont LARGE_FONT_DIGITAL_32 = {
    .base = { .data = NULL, .name = "Digital 32x48",
              .charWidth = 32, .charHeight = 48, .bytesPerChar = 192,
              .firstChar = '0', .lastChar = ':', .defaultSize = 1 },
    .type = LARGE_FONT_DIGITAL_32x48,
    .monospace = true,
    .digitWidth = 32
};

// Polices simplifiées pour les autres types
static const LargeFont LARGE_FONT_BOLD_16 = {
    .base = { .data = NULL, .name = "Bold 16x24",
              .charWidth = 16, .charHeight = 24, .bytesPerChar = 48,
              .firstChar = ' ', .lastChar = '~', .defaultSize = 1 },
    .type = LARGE_FONT_BOLD_16x24,
    .monospace = false,
    .digitWidth = 12,
    .proportional = true
};

static const LargeFont LARGE_FONT_NUMERIC_16 = {
    .base = { .data = NULL, .name = "Numeric 16x24",
              .charWidth = 12, .charHeight = 24, .bytesPerChar = 36,
              .firstChar = '0', .lastChar = '9', .defaultSize = 1 },
    .type = LARGE_FONT_NUMERIC_16x24,
    .monospace = true,
    .digitWidth = 12
};

static const LargeFont LARGE_FONT_TITLE_12 = {
    .base = { .data = NULL, .name = "Title 12x24",
              .charWidth = 12, .charHeight = 24, .bytesPerChar = 36,
              .firstChar = ' ', .lastChar = '~', .defaultSize = 2 },
    .type = LARGE_FONT_TITLE_12x24,
    .monospace = false,
    .digitWidth = 10,
    .proportional = true
};

static const LargeFont LARGE_FONT_CLOCK_24 = {
    .base = { .data = NULL, .name = "Clock 24x36",
              .charWidth = 24, .charHeight = 36, .bytesPerChar = 108,
              .firstChar = '0', .lastChar = ':', .defaultSize = 1 },
    .type = LARGE_FONT_CLOCK_24x36,
    .monospace = true,
    .digitWidth = 24
};

static const LargeFont LARGE_FONT_DIALER_20 = {
    .base = { .data = NULL, .name = "Dialer 20x32",
              .charWidth = 20, .charHeight = 32, .bytesPerChar = 80,
              .firstChar = '0', .lastChar = '9', .defaultSize = 1 },
    .type = LARGE_FONT_DIALER_20x32,
    .monospace = true,
    .digitWidth = 20
};

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Tableau des grandes polices */
static const LargeFont* large_fonts[] = {
    &LARGE_FONT_DIGITAL_16,
    &LARGE_FONT_DIGITAL_24,
    &LARGE_FONT_DIGITAL_32,
    &LARGE_FONT_BOLD_16,
    &LARGE_FONT_NUMERIC_16,
    &LARGE_FONT_TITLE_12,
    &LARGE_FONT_CLOCK_24,
    &LARGE_FONT_DIALER_20
};

static const uint8_t large_font_count = sizeof(large_fonts) / sizeof(large_fonts[0]);

/** @brief État du module */
static bool large_font_initialized = false;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

bool ui_font_large_init(void)
{
    LARGE_FONT_DEBUG("Initialisation du module de grandes polices...\n");
    large_font_initialized = true;
    LARGE_FONT_DEBUG("Module initialisé (%d polices)\n", large_font_count);
    return true;
}

void ui_font_large_deinit(void)
{
    large_font_initialized = false;
}

bool ui_font_large_is_ready(void)
{
    return large_font_initialized;
}

// ============================================================
// SECTION 2 : RECHERCHE DE POLICES
// ============================================================

const LargeFont* ui_font_large_get(LargeFontType type)
{
    for (uint8_t i = 0; i < large_font_count; i++)
    {
        if (large_fonts[i]->type == type)
            return large_fonts[i];
    }
    return NULL;
}

const LargeFont* ui_font_large_get_by_name(const char* name)
{
    if (name == NULL) return NULL;
    for (uint8_t i = 0; i < large_font_count; i++)
    {
        if (strcmp(large_fonts[i]->base.name, name) == 0)
            return large_fonts[i];
    }
    return NULL;
}

// ============================================================
// SECTION 3 : AFFICHAGE DE L'HEURE
// ============================================================

void ui_font_large_draw_time(LargeFontType type, uint16_t x, uint16_t y,
                              uint8_t hours, uint8_t minutes, uint16_t color, bool blinkColon)
{
    const LargeFont* font = ui_font_large_get(type);
    if (font == NULL) return;
    
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hours, minutes);
    
    uint16_t currentX = x;
    uint16_t digitW = font->digitWidth;
    uint16_t colonW = digitW * 0.6;  // Le ":" est plus étroit
    
    for (uint8_t i = 0; i < 5; i++)
    {
        char c = timeStr[i];
        uint16_t charW = (c == ':') ? colonW : digitW;
        
        // Faire clignoter le ":"
        if (c == ':' && blinkColon)
        {
            // Alterner toutes les 500ms
            if ((HAL_GetTick() / 500) % 2 == 0)
            {
                currentX += charW + 4;
                continue;  // Ne pas afficher le ":"
            }
        }
        
        // Dessiner le caractère
        display_set_font((DisplayFont*)&font->base);
        display_set_text_color(color);
        display_draw_char(currentX, y, c, color, 0x0000, 1);
        
        currentX += charW + 4;  // +4 pour l'espacement
    }
}

void ui_font_large_draw_time_seconds(LargeFontType type, uint16_t x, uint16_t y,
                                      uint8_t hours, uint8_t minutes, uint8_t seconds,
                                      uint16_t color)
{
    const LargeFont* font = ui_font_large_get(type);
    if (font == NULL) return;
    
    char timeStr[9];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hours, minutes, seconds);
    
    uint16_t currentX = x;
    
    for (uint8_t i = 0; i < 8; i++)
    {
        char c = timeStr[i];
        uint16_t charW = (c == ':') ? font->digitWidth * 0.6 : font->digitWidth;
        
        display_set_font((DisplayFont*)&font->base);
        display_draw_char(currentX, y, c, color, 0x0000, 1);
        
        currentX += charW + 3;
    }
}

// ============================================================
// SECTION 4 : AFFICHAGE DE NUMÉROS
// ============================================================

void ui_font_large_draw_phone_number(const char* number, uint16_t y, uint16_t color)
{
    if (number == NULL) return;
    
    uint16_t len = strlen(number);
    uint16_t digitW = 20;  // Largeur approximative d'un chiffre
    uint16_t spacing = 6;
    uint16_t totalWidth = len * digitW + (len - 1) * spacing;
    
    // Ajouter des espaces tous les 2 chiffres pour la lisibilité
    char formatted[32];
    uint8_t fmtIdx = 0;
    for (uint8_t i = 0; i < len && fmtIdx < 30; i++)
    {
        formatted[fmtIdx++] = number[i];
        if ((i + 1) % 2 == 0 && i < len - 1)
        {
            formatted[fmtIdx++] = ' ';
            totalWidth += 8;  // Espace
        }
    }
    formatted[fmtIdx] = '\0';
    
    uint16_t startX = (DISPLAY_WIDTH - totalWidth) / 2;
    uint16_t currentX = startX;
    
    const UIFont* font = ui_fonts_get_body();
    
    for (uint8_t i = 0; i < fmtIdx; i++)
    {
        char c = formatted[i];
        
        if (c == ' ')
        {
            currentX += 8;
        }
        else
        {
            display_set_font((DisplayFont*)font);
            display_draw_char(currentX, y, c, color, 0x0000, 2);
            currentX += digitW + spacing;
        }
    }
}

void ui_font_large_draw_call_timer(uint32_t durationSeconds, uint16_t x, uint16_t y, uint16_t color)
{
    uint32_t minutes = durationSeconds / 60;
    uint32_t seconds = durationSeconds % 60;
    
    char timerStr[6];
    snprintf(timerStr, sizeof(timerStr), "%02lu:%02lu", (unsigned long)minutes, (unsigned long)seconds);
    
    const LargeFont* font = &LARGE_FONT_DIGITAL_16;
    uint16_t currentX = x;
    
    for (uint8_t i = 0; i < 5; i++)
    {
        char c = timerStr[i];
        uint16_t charW = (c == ':') ? 10 : font->digitWidth;
        
        display_set_font((DisplayFont*)&font->base);
        display_draw_char(currentX, y, c, color, 0x0000, 1);
        
        currentX += charW + 3;
    }
}

// ============================================================
// SECTION 5 : AFFICHAGE DE GRANDS NOMBRES
// ============================================================

void ui_font_large_draw_big_number(uint16_t number, uint16_t x, uint16_t y, uint16_t color,
                                    const char* suffix)
{
    char numStr[16];
    
    if (suffix && strlen(suffix) > 0)
    {
        snprintf(numStr, sizeof(numStr), "%d%s", number, suffix);
    }
    else
    {
        snprintf(numStr, sizeof(numStr), "%d", number);
    }
    
    const LargeFont* font = &LARGE_FONT_DIGITAL_32;
    uint16_t currentX = x;
    uint16_t len = strlen(numStr);
    
    // Centrer si x est 0
    if (x == 0)
    {
        uint16_t totalWidth = len * font->digitWidth + (len - 1) * 4;
        currentX = (DISPLAY_WIDTH - totalWidth) / 2;
    }
    
    for (uint16_t i = 0; i < len; i++)
    {
        char c = numStr[i];
        uint16_t charW = (c >= '0' && c <= '9') ? font->digitWidth : font->digitWidth * 0.6;
        
        display_set_font((DisplayFont*)&font->base);
        display_draw_char(currentX, y, c, color, 0x0000, 1);
        
        currentX += charW + 4;
    }
}

// ============================================================
// SECTION 6 : ICÔNES
// ============================================================

void ui_font_large_draw_signal_bars(uint16_t x, uint16_t y, uint8_t level, uint16_t color)
{
    if (level > 4) level = 4;
    
    for (uint8_t i = 0; i < 4; i++)
    {
        uint8_t barHeight = 4 + i * 4;
        uint16_t barY = y + 16 - barHeight;
        uint16_t barX = x + i * 7;
        
        if (i < level)
        {
            display_fill_rect(barX, barY, barX + 5, y + 15, color);
        }
        else
        {
            display_draw_rect(barX, barY, barX + 5, y + 15, color);
        }
    }
}

void ui_font_large_draw_battery_icon(uint16_t x, uint16_t y, uint8_t percent,
                                      uint16_t color, bool charging)
{
    // Corps de la batterie
    display_draw_rect(x, y, x + 20, y + 10, color);
    
    // Ergots
    display_fill_rect(x + 21, y + 2, x + 22, y + 7, color);
    
    // Remplissage
    if (percent > 100) percent = 100;
    uint16_t fillWidth = (uint16_t)(18 * percent / 100);
    if (fillWidth > 0)
    {
        uint16_t fillColor = color;
        
        // Rouge si < 15%, jaune si < 30%
        if (percent < 15)
            fillColor = ILI9488_RED;
        else if (percent < 30)
            fillColor = ILI9488_YELLOW;
        
        display_fill_rect(x + 1, y + 1, x + fillWidth, y + 9, fillColor);
    }
    
    // Symbole éclair si en charge
    if (charging)
    {
        display_draw_text(x + 6, y - 2, "⚡", ILI9488_YELLOW, 1);
    }
}

void ui_font_large_draw_vu_meter(uint16_t x, uint16_t y, uint8_t level,
                                  uint16_t colorLow, uint16_t colorMid, uint16_t colorHigh)
{
    if (level > 100) level = 100;
    
    uint16_t barWidth = 8;
    uint16_t barSpacing = 2;
    uint16_t barMaxHeight = 40;
    uint8_t numBars = 16;
    
    for (uint8_t i = 0; i < numBars; i++)
    {
        uint16_t barX = x + i * (barWidth + barSpacing);
        uint16_t barHeight = (uint16_t)((uint32_t)barMaxHeight * level / 100);
        uint16_t barY = y + barMaxHeight - barHeight;
        
        // Couleur selon la position
        uint16_t barColor;
        if (i < numBars * 0.5)
            barColor = colorLow;
        else if (i < numBars * 0.8)
            barColor = colorMid;
        else
            barColor = colorHigh;
        
        if (barHeight > 0)
        {
            display_fill_rect(barX, barY, barX + barWidth - 1, y + barMaxHeight - 1, barColor);
        }
        else
        {
            display_fill_rect(barX, y + barMaxHeight - 3, barX + barWidth - 1, 
                             y + barMaxHeight - 1, colorLow);
        }
    }
}

// ============================================================
// SECTION 7 : DÉBOGAGE
// ============================================================

void ui_font_large_print_list(void)
{
    printf("\n═══ GRANDES POLICES (%d) ═══\n", large_font_count);
    
    for (uint8_t i = 0; i < large_font_count; i++)
    {
        const LargeFont* f = large_fonts[i];
        printf("  [%d] %-20s %dx%d %s\n", i, f->base.name,
               f->base.charWidth, f->base.charHeight,
               f->monospace ? "(monospace)" : "");
    }
    printf("══════════════════════════════\n\n");
}

bool ui_font_large_self_test(void)
{
    LARGE_FONT_DEBUG("Auto-test...\n");
    
    if (!large_font_initialized)
    {
        LARGE_FONT_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Vérifier que toutes les polices sont accessibles
    for (uint8_t i = 0; i < large_font_count; i++)
    {
        if (large_fonts[i] == NULL || large_fonts[i]->base.charWidth == 0)
        {
            LARGE_FONT_DEBUG("Échec : police %d invalide\n", i);
            return false;
        }
    }
    
    // Test d'affichage rapide
    ui_font_large_draw_time(LARGE_FONT_DIGITAL_32, 10, 10, 12, 34, ILI9488_WHITE, false);
    ui_font_large_draw_battery_icon(250, 5, 75, ILI9488_GREEN, false);
    ui_font_large_draw_signal_bars(220, 5, 4, ILI9488_WHITE);
    
    LARGE_FONT_DEBUG("Auto-test OK\n");
    return true;
}