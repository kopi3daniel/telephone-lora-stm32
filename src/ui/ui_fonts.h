/**
 * @file ui_fonts.h
 * @brief Gestion des polices de caractères pour l'interface
 * 
 * Ce fichier définit les polices disponibles et les fonctions
 * associées pour l'affichage de texte.
 * 
 * Polices intégrées :
 * - font_5x7    : Police compacte (5×7 pixels)
 * - font_8x16   : Police standard (8×16 pixels)
 * - font_12x24  : Police moyenne (12×24 pixels)
 * - font_16x32  : Grande police (16×32 pixels)
 * - font_6x10   : Police compacte lisible (6×10 pixels)
 * 
 * Styles de texte :
 * - Normal
 * - Gras (simulé)
 * - Italique (simulé)
 * - Souligné
 * - Barré
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_FONTS_H
#define UI_FONTS_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "../drivers/display/display_manager.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define UI_FONTS_VERSION                "1.0.0"

/** @brief Nombre maximum de polices personnalisées */
#define UI_FONTS_MAX_CUSTOM             10

/** @brief Nombre de caractères dans une police ASCII */
#define UI_FONTS_CHAR_COUNT             96      // Espace (0x20) à ~ (0x7E)

// ============================================================
// SECTION 2 : TYPES DE POLICES
// ============================================================

/**
 * @brief Styles de texte disponibles
 */
typedef enum {
    FONT_STYLE_NORMAL       = 0,        // Normal
    FONT_STYLE_BOLD         = (1 << 0), // Gras
    FONT_STYLE_ITALIC       = (1 << 1), // Italique
    FONT_STYLE_UNDERLINE    = (1 << 2), // Souligné
    FONT_STYLE_STRIKETHROUGH = (1 << 3) // Barré
} FontStyle;

/**
 * @brief Alignement vertical du texte
 */
typedef enum {
    FONT_VALIGN_TOP         = 0,    // Haut
    FONT_VALIGN_MIDDLE       = 1,    // Milieu
    FONT_VALIGN_BOTTOM       = 2     // Bas
} FontVAlign;

/**
 * @brief Structure d'une police de caractères
 */
typedef struct {
    const uint8_t* data;            // Données bitmap de la police
    const char* name;               // Nom de la police (ex: "5x7", "8x16")
    uint8_t charWidth;              // Largeur d'un caractère (pixels)
    uint8_t charHeight;             // Hauteur d'un caractère (pixels)
    uint8_t bytesPerChar;           // Octets par caractère
    char firstChar;                 // Premier caractère (généralement ' ' = 0x20)
    char lastChar;                  // Dernier caractère (généralement '~' = 0x7E)
    uint8_t defaultSize;            // Taille par défaut (multiplicateur)
} UIFont;

/**
 * @brief Police personnalisée (chargée dynamiquement)
 */
typedef struct {
    UIFont font;                    // Structure de base
    uint8_t* data;                  // Données allouées dynamiquement
    bool loaded;                    // Chargée ?
} UICustomFont;

// ============================================================
// SECTION 3 : POLICES INTÉGRÉES
// ============================================================

/** @brief Police 5×7 compacte */
extern const UIFont UI_FONT_5X7;

/** @brief Police 6×10 compacte lisible */
extern const UIFont UI_FONT_6X10;

/** @brief Police 8×16 standard */
extern const UIFont UI_FONT_8X16;

/** @brief Police 12×24 moyenne */
extern const UIFont UI_FONT_12X24;

/** @brief Police 16×32 grande */
extern const UIFont UI_FONT_16X32;

/** @brief Tableau des polices intégrées */
extern const UIFont* UI_FONTS_BUILTIN[];
extern const uint8_t UI_FONTS_BUILTIN_COUNT;

// ============================================================
// SECTION 4 : ÉTAT DU MODULE
// ============================================================

/**
 * @brief État du module de polices
 */
typedef struct {
    bool initialized;                       // Module initialisé
    UICustomFont customFonts[UI_FONTS_MAX_CUSTOM];  // Polices personnalisées
    uint8_t customFontCount;                // Nombre de polices personnalisées
} UIFontsState;

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

bool ui_fonts_init(void);
void ui_fonts_deinit(void);
bool ui_fonts_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS DE POLICES
// ============================================================

/**
 * @brief Récupère une police par son nom
 */
const UIFont* ui_fonts_get_by_name(const char* name);

/**
 * @brief Récupère une police par sa taille
 */
const UIFont* ui_fonts_get_by_size(uint8_t charWidth, uint8_t charHeight);

/**
 * @brief Récupère la police par défaut (8x16)
 */
const UIFont* ui_fonts_get_default(void);

/**
 * @brief Récupère la police pour les titres
 */
const UIFont* ui_fonts_get_title(void);

/**
 * @brief Récupère la police pour le corps du texte
 */
const UIFont* ui_fonts_get_body(void);

/**
 * @brief Récupère la police pour les petits textes
 */
const UIFont* ui_fonts_get_small(void);

// ============================================================
// SECTION 7 : FONCTIONS DE MESURE
// ============================================================

/**
 * @brief Mesure la largeur d'un texte en pixels
 */
uint16_t ui_fonts_measure_text_width(const UIFont* font, const char* text, uint8_t size);

/**
 * @brief Mesure la hauteur d'une ligne de texte en pixels
 */
uint16_t ui_fonts_measure_text_height(const UIFont* font, uint8_t size);

/**
 * @brief Compte le nombre de caractères qui tiennent dans une largeur donnée
 */
uint16_t ui_fonts_fit_text(const UIFont* font, const char* text, uint8_t size, uint16_t maxWidth);

/**
 * @brief Découpe un texte en lignes selon une largeur maximale
 * @return Nombre de lignes
 */
uint16_t ui_fonts_wrap_text(const UIFont* font, const char* text, uint8_t size, 
                             uint16_t maxWidth, char lines[][128], uint16_t maxLines);

// ============================================================
// SECTION 8 : FONCTIONS DE DESSIN AVANCÉ
// ============================================================

/**
 * @brief Dessine un texte avec un style spécifique
 */
void ui_fonts_draw_text_styled(const UIFont* font, uint16_t x, uint16_t y, 
                                const char* text, uint16_t color, uint8_t size, FontStyle style);

/**
 * @brief Dessine un texte aligné horizontalement et verticalement
 */
void ui_fonts_draw_text_aligned(const UIFont* font, uint16_t x1, uint16_t y1, 
                                 uint16_t x2, uint16_t y2, const char* text, 
                                 uint16_t color, uint8_t size, 
                                 UIAlign hAlign, FontVAlign vAlign);

/**
 * @brief Dessine un texte avec dégradé de couleurs
 */
void ui_fonts_draw_text_gradient(const UIFont* font, uint16_t x, uint16_t y,
                                  const char* text, uint16_t colorTop, uint16_t colorBottom, 
                                  uint8_t size);

/**
 * @brief Dessine un texte avec ombre
 */
void ui_fonts_draw_text_shadow(const UIFont* font, uint16_t x, uint16_t y,
                                const char* text, uint16_t color, uint16_t shadowColor, 
                                uint8_t size, uint8_t shadowOffset);

// ============================================================
// SECTION 9 : FONCTIONS DE POLICES PERSONNALISÉES
// ============================================================

bool ui_fonts_load_custom(const char* name, const uint8_t* data, 
                           uint8_t width, uint8_t height, uint8_t bytesPerChar);
bool ui_fonts_unload_custom(uint8_t index);
const UIFont* ui_fonts_get_custom(uint8_t index);
uint8_t ui_fonts_get_custom_count(void);

// ============================================================
// SECTION 10 : FONCTIONS DE DÉBOGAGE
// ============================================================

void ui_fonts_print_list(void);
void ui_fonts_print_info(const UIFont* font);
bool ui_fonts_self_test(void);

// ============================================================
// SECTION 11 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define FONTS_DEBUG(fmt, ...)       printf("[FONTS] " fmt, ##__VA_ARGS__)
#else
    #define FONTS_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 12 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_FONTS_H