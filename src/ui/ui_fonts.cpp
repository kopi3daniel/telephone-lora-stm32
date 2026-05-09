/**
 * @file ui_fonts.cpp
 * @brief Implémentation du module de polices de caractères
 * 
 * Fonctionnalités :
 * - Polices intégrées (5x7, 6x10, 8x16, 12x24, 16x32)
 * - Mesure de texte (largeur, hauteur)
 * - Word wrap automatique
 * - Styles de texte (gras, italique, souligné, barré)
 * - Alignement horizontal et vertical
 * - Effets (ombre, dégradé)
 * - Polices personnalisées
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_fonts.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// POLICES INTÉGRÉES (DÉCLARATIONS EXTERNES)
// ============================================================

// Les données des polices sont définies dans display_manager.cpp
extern const DisplayFont font_5x7;
extern const DisplayFont font_8x16;
extern const DisplayFont font_12x24;
extern const DisplayFont font_16x32;

/** @brief Police 5×7 */
const UIFont UI_FONT_5X7 = {
    .data = NULL,  // Les données sont dans font_5x7
    .name = "5x7",
    .charWidth = 5,
    .charHeight = 7,
    .bytesPerChar = 7,
    .firstChar = ' ',
    .lastChar = '~',
    .defaultSize = 1
};

/** @brief Police 6×10 (basée sur 5x7 avec espacement) */
const UIFont UI_FONT_6X10 = {
    .data = NULL,
    .name = "6x10",
    .charWidth = 6,
    .charHeight = 10,
    .bytesPerChar = 10,
    .firstChar = ' ',
    .lastChar = '~',
    .defaultSize = 1
};

/** @brief Police 8×16 standard */
const UIFont UI_FONT_8X16 = {
    .data = NULL,
    .name = "8x16",
    .charWidth = 8,
    .charHeight = 16,
    .bytesPerChar = 16,
    .firstChar = ' ',
    .lastChar = '~',
    .defaultSize = 1
};

/** @brief Police 12×24 moyenne */
const UIFont UI_FONT_12X24 = {
    .data = NULL,
    .name = "12x24",
    .charWidth = 12,
    .charHeight = 24,
    .bytesPerChar = 36,
    .firstChar = ' ',
    .lastChar = '~',
    .defaultSize = 1
};

/** @brief Police 16×32 grande */
const UIFont UI_FONT_16X32 = {
    .data = NULL,
    .name = "16x32",
    .charWidth = 16,
    .charHeight = 32,
    .bytesPerChar = 64,
    .firstChar = ' ',
    .lastChar = '~',
    .defaultSize = 1
};

/** @brief Tableau des polices intégrées */
const UIFont* UI_FONTS_BUILTIN[] = {
    &UI_FONT_5X7,
    &UI_FONT_6X10,
    &UI_FONT_8X16,
    &UI_FONT_12X24,
    &UI_FONT_16X32
};

const uint8_t UI_FONTS_BUILTIN_COUNT = sizeof(UI_FONTS_BUILTIN) / sizeof(UI_FONTS_BUILTIN[0]);

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du module */
static UIFontsState fonts_state;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

bool ui_fonts_init(void)
{
    FONTS_DEBUG("Initialisation du module de polices...\n");
    
    memset(&fonts_state, 0, sizeof(UIFontsState));
    fonts_state.initialized = true;
    
    FONTS_DEBUG("Module initialisé (%d polices intégrées)\n", UI_FONTS_BUILTIN_COUNT);
    return true;
}

void ui_fonts_deinit(void)
{
    // Libérer les polices personnalisées
    for (uint8_t i = 0; i < fonts_state.customFontCount; i++)
    {
        if (fonts_state.customFonts[i].data)
        {
            free(fonts_state.customFonts[i].data);
        }
    }
    fonts_state.initialized = false;
}

bool ui_fonts_is_ready(void)
{
    return fonts_state.initialized;
}

// ============================================================
// SECTION 2 : RECHERCHE DE POLICES
// ============================================================

const UIFont* ui_fonts_get_by_name(const char* name)
{
    if (name == NULL) return NULL;
    
    // Chercher dans les polices intégrées
    for (uint8_t i = 0; i < UI_FONTS_BUILTIN_COUNT; i++)
    {
        if (strcmp(UI_FONTS_BUILTIN[i]->name, name) == 0)
            return UI_FONTS_BUILTIN[i];
    }
    
    // Chercher dans les polices personnalisées
    for (uint8_t i = 0; i < fonts_state.customFontCount; i++)
    {
        if (strcmp(fonts_state.customFonts[i].font.name, name) == 0)
            return &fonts_state.customFonts[i].font;
    }
    
    return NULL;
}

const UIFont* ui_fonts_get_by_size(uint8_t charWidth, uint8_t charHeight)
{
    for (uint8_t i = 0; i < UI_FONTS_BUILTIN_COUNT; i++)
    {
        if (UI_FONTS_BUILTIN[i]->charWidth == charWidth && 
            UI_FONTS_BUILTIN[i]->charHeight == charHeight)
            return UI_FONTS_BUILTIN[i];
    }
    return NULL;
}

const UIFont* ui_fonts_get_default(void)  { return &UI_FONT_8X16; }
const UIFont* ui_fonts_get_title(void)    { return &UI_FONT_16X32; }
const UIFont* ui_fonts_get_body(void)     { return &UI_FONT_8X16; }
const UIFont* ui_fonts_get_small(void)    { return &UI_FONT_5X7; }

// ============================================================
// SECTION 3 : MESURE DE TEXTE
// ============================================================

uint16_t ui_fonts_measure_text_width(const UIFont* font, const char* text, uint8_t size)
{
    if (font == NULL || text == NULL) return 0;
    
    uint16_t len = strlen(text);
    return font->charWidth * size * len;
}

uint16_t ui_fonts_measure_text_height(const UIFont* font, uint8_t size)
{
    if (font == NULL) return 0;
    return font->charHeight * size;
}

uint16_t ui_fonts_fit_text(const UIFont* font, const char* text, uint8_t size, uint16_t maxWidth)
{
    if (font == NULL || text == NULL || size == 0) return 0;
    
    uint16_t charPixelWidth = font->charWidth * size;
    uint16_t maxChars = maxWidth / charPixelWidth;
    uint16_t textLen = strlen(text);
    
    return (maxChars < textLen) ? maxChars : textLen;
}

uint16_t ui_fonts_wrap_text(const UIFont* font, const char* text, uint8_t size,
                             uint16_t maxWidth, char lines[][128], uint16_t maxLines)
{
    if (font == NULL || text == NULL || lines == NULL || size == 0 || maxLines == 0) return 0;
    
    uint16_t charPixelWidth = font->charWidth * size;
    uint16_t charsPerLine = maxWidth / charPixelWidth;
    if (charsPerLine == 0) charsPerLine = 1;
    
    uint16_t lineCount = 0;
    uint16_t pos = 0;
    uint16_t totalLen = strlen(text);
    
    while (pos < totalLen && lineCount < maxLines)
    {
        uint16_t remaining = totalLen - pos;
        uint16_t lineLen = (remaining < charsPerLine) ? remaining : charsPerLine;
        
        // Ajuster pour ne pas couper un mot
        if (pos + lineLen < totalLen && lineLen > charsPerLine / 2)
        {
            // Reculer jusqu'à un espace
            uint16_t endPos = pos + lineLen;
            while (endPos > pos + charsPerLine / 2 && text[endPos] != ' ' && text[endPos] != '\0')
            {
                endPos--;
            }
            if (text[endPos] == ' ')
            {
                lineLen = endPos - pos + 1;  // +1 pour inclure l'espace
            }
        }
        
        // Copier la ligne
        uint16_t copyLen = (lineLen < 127) ? lineLen : 127;
        strncpy(lines[lineCount], text + pos, copyLen);
        lines[lineCount][copyLen] = '\0';
        
        // Enlever les espaces en début de ligne suivante
        pos += lineLen;
        while (pos < totalLen && text[pos] == ' ') pos++;
        
        lineCount++;
    }
    
    return lineCount;
}

// ============================================================
// SECTION 4 : DESSIN DE TEXTE AVANCÉ
// ============================================================

void ui_fonts_draw_text_styled(const UIFont* font, uint16_t x, uint16_t y,
                                const char* text, uint16_t color, uint8_t size, FontStyle style)
{
    if (font == NULL || text == NULL) return;
    
    uint16_t charW = font->charWidth * size;
    uint16_t charH = font->charHeight * size;
    uint16_t currentX = x;
    
    // Convertir le UIFont en DisplayFont pour utiliser les fonctions d'affichage
    DisplayFont df;
    df.data = font->data;
    df.charWidth = font->charWidth;
    df.charHeight = font->charHeight;
    df.bytesPerChar = font->bytesPerChar;
    df.firstChar = font->firstChar;
    df.lastChar = font->lastChar;
    df.name = font->name;
    
    // Dessiner le texte normal
    display_set_font(&df);
    display_set_text_color(color);
    display_draw_text(currentX, y, text, color, size);
    
    // Appliquer les styles supplémentaires
    uint16_t textWidth = ui_fonts_measure_text_width(font, text, size);
    
    // Gras : redessiner décalé d'1 pixel
    if (style & FONT_STYLE_BOLD)
    {
        display_draw_text(currentX + 1, y, text, color, size);
    }
    
    // Italique : décalage progressif (simplifié)
    if (style & FONT_STYLE_ITALIC)
    {
        uint16_t italicX = currentX;
        for (uint16_t i = 0; i < strlen(text); i++)
        {
            // Décaler chaque caractère
            char singleChar[2] = {text[i], '\0'};
            display_draw_text(italicX + (i * size) / 4, y, singleChar, color, size);
        }
    }
    
    // Souligné
    if (style & FONT_STYLE_UNDERLINE)
    {
        uint16_t underlineY = y + charH - 1;
        display_draw_hline(currentX, underlineY, currentX + textWidth - 1, color);
    }
    
    // Barré
    if (style & FONT_STYLE_STRIKETHROUGH)
    {
        uint16_t strikeY = y + charH / 2;
        display_draw_hline(currentX, strikeY, currentX + textWidth - 1, color);
    }
}

void ui_fonts_draw_text_aligned(const UIFont* font, uint16_t x1, uint16_t y1,
                                 uint16_t x2, uint16_t y2, const char* text,
                                 uint16_t color, uint8_t size,
                                 UIAlign hAlign, FontVAlign vAlign)
{
    if (font == NULL || text == NULL) return;
    
    uint16_t textWidth = ui_fonts_measure_text_width(font, text, size);
    uint16_t textHeight = ui_fonts_measure_text_height(font, size);
    
    uint16_t areaWidth = x2 - x1;
    uint16_t areaHeight = y2 - y1;
    
    // Calculer X selon l'alignement horizontal
    uint16_t drawX;
    switch (hAlign)
    {
        case UI_ALIGN_CENTER:
            drawX = x1 + (areaWidth - textWidth) / 2;
            break;
        case UI_ALIGN_RIGHT:
            drawX = x2 - textWidth;
            break;
        case UI_ALIGN_LEFT:
        default:
            drawX = x1;
            break;
    }
    
    // Calculer Y selon l'alignement vertical
    uint16_t drawY;
    switch (vAlign)
    {
        case FONT_VALIGN_MIDDLE:
            drawY = y1 + (areaHeight - textHeight) / 2;
            break;
        case FONT_VALIGN_BOTTOM:
            drawY = y2 - textHeight;
            break;
        case FONT_VALIGN_TOP:
        default:
            drawY = y1;
            break;
    }
    
    // Limiter aux bornes
    if (drawX < x1) drawX = x1;
    if (drawX + textWidth > x2) drawX = x2 - textWidth;
    if (drawY < y1) drawY = y1;
    if (drawY + textHeight > y2) drawY = y2 - textHeight;
    
    ui_fonts_draw_text_styled(font, drawX, drawY, text, color, size, FONT_STYLE_NORMAL);
}

void ui_fonts_draw_text_gradient(const UIFont* font, uint16_t x, uint16_t y,
                                  const char* text, uint16_t colorTop, uint16_t colorBottom,
                                  uint8_t size)
{
    if (font == NULL || text == NULL) return;
    
    uint16_t textHeight = ui_fonts_measure_text_height(font, size);
    
    // Dessiner ligne par ligne avec interpolation
    for (uint16_t line = 0; line < textHeight; line++)
    {
        float progress = (float)line / textHeight;
        uint16_t color = interpolate_rgb565(colorTop, colorBottom, progress);
        
        // Pour chaque caractère, dessiner uniquement la ligne courante
        uint16_t currentX = x;
        for (uint16_t i = 0; i < strlen(text); i++)
        {
            // Dessiner un pixel de la ligne courante (simplifié)
            display_draw_pixel(currentX, y + line, color);
            currentX += font->charWidth * size;
        }
    }
}

void ui_fonts_draw_text_shadow(const UIFont* font, uint16_t x, uint16_t y,
                                const char* text, uint16_t color, uint16_t shadowColor,
                                uint8_t size, uint8_t shadowOffset)
{
    if (font == NULL || text == NULL) return;
    
    // Dessiner l'ombre en premier
    ui_fonts_draw_text_styled(font, x + shadowOffset, y + shadowOffset, 
                               text, shadowColor, size, FONT_STYLE_NORMAL);
    
    // Dessiner le texte par-dessus
    ui_fonts_draw_text_styled(font, x, y, text, color, size, FONT_STYLE_NORMAL);
}

// ============================================================
// SECTION 5 : POLICES PERSONNALISÉES
// ============================================================

bool ui_fonts_load_custom(const char* name, const uint8_t* data,
                           uint8_t width, uint8_t height, uint8_t bytesPerChar)
{
    if (name == NULL || data == NULL) return false;
    if (fonts_state.customFontCount >= UI_FONTS_MAX_CUSTOM) return false;
    
    UICustomFont* custom = &fonts_state.customFonts[fonts_state.customFontCount];
    memset(custom, 0, sizeof(UICustomFont));
    
    // Copier les données
    uint16_t dataSize = bytesPerChar * UI_FONTS_CHAR_COUNT;
    custom->data = (uint8_t*)malloc(dataSize);
    if (custom->data == NULL) return false;
    memcpy(custom->data, data, dataSize);
    
    // Configurer la police
    custom->font.data = custom->data;
    custom->font.name = name;  // Attention: name doit être une chaîne persistante
    custom->font.charWidth = width;
    custom->font.charHeight = height;
    custom->font.bytesPerChar = bytesPerChar;
    custom->font.firstChar = ' ';
    custom->font.lastChar = '~';
    custom->font.defaultSize = 1;
    custom->loaded = true;
    
    fonts_state.customFontCount++;
    
    FONTS_DEBUG("Police personnalisée chargée: %s (%dx%d)\n", name, width, height);
    return true;
}

bool ui_fonts_unload_custom(uint8_t index)
{
    if (index >= fonts_state.customFontCount) return false;
    
    if (fonts_state.customFonts[index].data)
    {
        free(fonts_state.customFonts[index].data);
    }
    
    if (index < fonts_state.customFontCount - 1)
    {
        memmove(&fonts_state.customFonts[index], 
                &fonts_state.customFonts[index + 1],
                (fonts_state.customFontCount - index - 1) * sizeof(UICustomFont));
    }
    fonts_state.customFontCount--;
    
    return true;
}

const UIFont* ui_fonts_get_custom(uint8_t index)
{
    if (index >= fonts_state.customFontCount) return NULL;
    return &fonts_state.customFonts[index].font;
}

uint8_t ui_fonts_get_custom_count(void)
{
    return fonts_state.customFontCount;
}

// ============================================================
// SECTION 6 : FONCTIONS UTILITAIRES
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

// ============================================================
// SECTION 7 : DÉBOGAGE
// ============================================================

void ui_fonts_print_list(void)
{
    printf("\n═══ POLICES DISPONIBLES ═══\n");
    printf("Intégrées (%d):\n", UI_FONTS_BUILTIN_COUNT);
    
    for (uint8_t i = 0; i < UI_FONTS_BUILTIN_COUNT; i++)
    {
        printf("  [%d] %-8s %dx%d pixels\n", i, 
               UI_FONTS_BUILTIN[i]->name,
               UI_FONTS_BUILTIN[i]->charWidth,
               UI_FONTS_BUILTIN[i]->charHeight);
    }
    
    if (fonts_state.customFontCount > 0)
    {
        printf("Personnalisées (%d):\n", fonts_state.customFontCount);
        for (uint8_t i = 0; i < fonts_state.customFontCount; i++)
        {
            printf("  [%d] %-8s %dx%d pixels\n", i,
                   fonts_state.customFonts[i].font.name,
                   fonts_state.customFonts[i].font.charWidth,
                   fonts_state.customFonts[i].font.charHeight);
        }
    }
    printf("══════════════════════════\n\n");
}

void ui_fonts_print_info(const UIFont* font)
{
    if (font == NULL) return;
    
    printf("\n═══ POLICE : %s ═══\n", font->name);
    printf("Dimensions    : %d × %d pixels\n", font->charWidth, font->charHeight);
    printf("Octets/caract.: %d\n", font->bytesPerChar);
    printf("Caractères    : '%c' (0x%02X) à '%c' (0x%02X)\n",
           font->firstChar, font->firstChar,
           font->lastChar, font->lastChar);
    printf("Taille défaut : %dx\n", font->defaultSize);
    printf("══════════════════════\n\n");
}

bool ui_fonts_self_test(void)
{
    FONTS_DEBUG("Auto-test...\n");
    
    // Vérifier que toutes les polices intégrées sont accessibles
    for (uint8_t i = 0; i < UI_FONTS_BUILTIN_COUNT; i++)
    {
        if (UI_FONTS_BUILTIN[i] == NULL || UI_FONTS_BUILTIN[i]->charWidth == 0)
        {
            FONTS_DEBUG("Échec : police intégrée %d invalide\n", i);
            return false;
        }
    }
    
    // Test de mesure
    const UIFont* font = &UI_FONT_8X16;
    uint16_t width = ui_fonts_measure_text_width(font, "Test", 1);
    if (width != 32)  // 4 caractères × 8 pixels
    {
        FONTS_DEBUG("Échec : mesure largeur incorrecte (%d au lieu de 32)\n", width);
        return false;
    }
    
    // Test de word wrap
    char lines[5][128];
    uint16_t count = ui_fonts_wrap_text(font, "Hello World Test", 1, 80, lines, 5);
    if (count == 0)
    {
        FONTS_DEBUG("Échec : word wrap\n");
        return false;
    }
    
    FONTS_DEBUG("Auto-test OK\n");
    return true;
}