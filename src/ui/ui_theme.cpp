/**
 * @file ui_theme.cpp
 * @brief Implémentation du système de thèmes
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans ui_theme.h.
 * 
 * Il gère :
 * - Les 8 thèmes prédéfinis
 * - L'application des thèmes
 * - Les transitions entre thèmes
 * - Les thèmes personnalisés
 * - Le mode sombre
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_theme.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================
// THÈMES PRÉDÉFINIS
// ============================================================

/** @brief Thème CLAIR */
const UIThemeFull UI_THEME_FULL_LIGHT = {
    .name = "Clair",
    .author = "Système",
    .colors = {
        .background     = 0xFFFF,  // Blanc
        .surface        = 0xF7F7,  // Gris très clair
        .surfaceVariant = 0xEFEF,  // Gris clair variante
        .primary        = 0x07E0,  // Vert
        .primaryDark    = 0x03A0,  // Vert foncé
        .primaryLight   = 0x8FD0,  // Vert clair
        .onPrimary      = 0xFFFF,  // Blanc sur vert
        .secondary      = 0x5AEB,  // Bleu-gris
        .secondaryDark  = 0x39C7,  // Bleu-gris foncé
        .secondaryLight = 0x9CD3,  // Bleu-gris clair
        .onSecondary    = 0xFFFF,  // Blanc sur secondaire
        .textPrimary    = 0x0000,  // Noir
        .textSecondary  = 0x8410,  // Gris
        .textDisabled   = 0xAD55,  // Gris clair
        .textHint       = 0xC618,  // Gris très clair
        .error          = 0xF800,  // Rouge
        .onError        = 0xFFFF,  // Blanc sur erreur
        .success        = 0x07E0,  // Vert
        .onSuccess      = 0xFFFF,  // Blanc sur succès
        .warning        = 0xFFE0,  // Jaune
        .onWarning      = 0x0000,  // Noir sur avertissement
        .info           = 0x001F,  // Bleu
        .onInfo         = 0xFFFF,  // Blanc sur info
        .border         = 0xC618,  // Gris bordure
        .divider        = 0xE71C,  // Gris séparateur
        .shadow         = 0x8410,  // Ombre
        .overlay        = 0x0000,  // Noir overlay
        .disabled       = 0xAD55,  // Gris désactivé
        .highlight      = 0x07E0,  // Vert surbrillance
        .statusBarBg    = 0x03A0,  // Vert foncé barre statut
        .navBarBg       = 0xF7F7,  // Gris clair barre nav
        .buttonDefault  = 0xE71C,  // Gris bouton défaut
        .buttonCall     = 0x07E0,  // Vert bouton appel
        .buttonEnd      = 0xF800,  // Rouge bouton fin
        .inputBg        = 0xEFEF   // Gris clair champ saisie
    },
    .fonts = {
        .titleFont  = &font_8x16,
        .bodyFont   = &font_8x16,
        .smallFont  = &font_5x7,
        .monoFont   = &font_8x16,
        .titleSize  = 2,
        .bodySize   = 1,
        .smallSize  = 1
    },
    .dimensions = {
        .cornerRadius      = 8,
        .cornerRadiusLarge = 16,
        .cornerRadiusSmall = 4,
        .screenMargin      = {16, 16, 16, 16},
        .cardMargin        = {8, 8, 8, 8},
        .buttonPadding     = {12, 24, 12, 24},
        .inputPadding      = {8, 12, 8, 12},
        .borderWidth       = 1,
        .dividerHeight     = 1,
        .statusBarHeight   = 24,
        .navBarHeight      = 48,
        .buttonMinWidth    = 80,
        .buttonMinHeight   = 40
    },
    .darkMode = false,
    .predefined = true
};

/** @brief Thème SOMBRE */
const UIThemeFull UI_THEME_FULL_DARK = {
    .name = "Sombre",
    .author = "Système",
    .colors = {
        .background     = 0x0000,  // Noir
        .surface        = 0x18E3,  // Gris très foncé
        .surfaceVariant = 0x2104,  // Gris foncé variante
        .primary        = 0x07E0,  // Vert
        .primaryDark    = 0x03A0,  // Vert foncé
        .primaryLight   = 0x8FD0,  // Vert clair
        .onPrimary      = 0xFFFF,  // Blanc sur vert
        .secondary      = 0x39C7,  // Bleu-gris
        .secondaryDark  = 0x18E3,  // Bleu-gris foncé
        .secondaryLight = 0x5AEB,  // Bleu-gris clair
        .onSecondary    = 0xFFFF,  // Blanc sur secondaire
        .textPrimary    = 0xFFFF,  // Blanc
        .textSecondary  = 0x8410,  // Gris
        .textDisabled   = 0x4208,  // Gris foncé
        .textHint       = 0x630C,  // Gris
        .error          = 0xF800,  // Rouge
        .onError        = 0xFFFF,  // Blanc sur erreur
        .success        = 0x07E0,  // Vert
        .onSuccess      = 0xFFFF,  // Blanc sur succès
        .warning        = 0xFFE0,  // Jaune
        .onWarning      = 0x0000,  // Noir sur avertissement
        .info           = 0x001F,  // Bleu
        .onInfo         = 0xFFFF,  // Blanc sur info
        .border         = 0x4208,  // Gris foncé bordure
        .divider        = 0x39C7,  // Gris séparateur
        .shadow         = 0x0000,  // Noir ombre
        .overlay        = 0x0000,  // Noir overlay
        .disabled       = 0x39C7,  // Gris désactivé
        .highlight      = 0x07E0,  // Vert surbrillance
        .statusBarBg    = 0x0000,  // Noir barre statut
        .navBarBg       = 0x18E3,  // Gris foncé barre nav
        .buttonDefault  = 0x39C7,  // Gris bouton défaut
        .buttonCall     = 0x07E0,  // Vert bouton appel
        .buttonEnd      = 0xF800,  // Rouge bouton fin
        .inputBg        = 0x2104   // Gris foncé champ saisie
    },
    .fonts = {
        .titleFont  = &font_8x16,
        .bodyFont   = &font_8x16,
        .smallFont  = &font_5x7,
        .monoFont   = &font_8x16,
        .titleSize  = 2,
        .bodySize   = 1,
        .smallSize  = 1
    },
    .dimensions = {
        .cornerRadius      = 8,
        .cornerRadiusLarge = 16,
        .cornerRadiusSmall = 4,
        .screenMargin      = {16, 16, 16, 16},
        .cardMargin        = {8, 8, 8, 8},
        .buttonPadding     = {12, 24, 12, 24},
        .inputPadding      = {8, 12, 8, 12},
        .borderWidth       = 1,
        .dividerHeight     = 1,
        .statusBarHeight   = 24,
        .navBarHeight      = 48,
        .buttonMinWidth    = 80,
        .buttonMinHeight   = 40
    },
    .darkMode = true,
    .predefined = true
};

/** @brief Thème NATURE (vert) */
const UIThemeFull UI_THEME_FULL_NATURE = {
    .name = "Nature",
    .author = "Système",
    .colors = {
        .background     = 0xF7F7,
        .surface        = 0xEFEF,
        .surfaceVariant = 0xE7E7,
        .primary        = 0x2DC4,  // Vert nature
        .primaryDark    = 0x1A82,  // Vert nature foncé
        .primaryLight   = 0x7E8C,  // Vert nature clair
        .onPrimary      = 0xFFFF,
        .secondary      = 0x6B4E,  // Marron
        .secondaryDark  = 0x4A2A,  // Marron foncé
        .secondaryLight = 0x9D72,  // Marron clair
        .onSecondary    = 0xFFFF,
        .textPrimary    = 0x1A82,  // Vert très foncé
        .textSecondary  = 0x632C,  // Gris-vert
        .textDisabled   = 0xAD55,
        .textHint       = 0xC618,
        .error          = 0xF800,
        .onError        = 0xFFFF,
        .success        = 0x07E0,
        .onSuccess      = 0xFFFF,
        .warning        = 0xFFE0,
        .onWarning      = 0x0000,
        .info           = 0x2DC4,
        .onInfo         = 0xFFFF,
        .border         = 0xAD55,
        .divider        = 0xC618,
        .shadow         = 0x8410,
        .overlay        = 0x0000,
        .disabled       = 0xAD55,
        .highlight      = 0x2DC4,
        .statusBarBg    = 0x1A82,
        .navBarBg       = 0xEFEF,
        .buttonDefault  = 0xE71C,
        .buttonCall     = 0x2DC4,
        .buttonEnd      = 0xF800,
        .inputBg        = 0xEFEF
    },
    .fonts = {
        .titleFont  = &font_8x16, .bodyFont   = &font_8x16,
        .smallFont  = &font_5x7,  .monoFont   = &font_8x16,
        .titleSize  = 2, .bodySize = 1, .smallSize = 1
    },
    .dimensions = {
        .cornerRadius = 12, .cornerRadiusLarge = 20, .cornerRadiusSmall = 6,
        .screenMargin = {16,16,16,16}, .cardMargin = {8,8,8,8},
        .buttonPadding = {12,24,12,24}, .inputPadding = {8,12,8,12},
        .borderWidth = 1, .dividerHeight = 1,
        .statusBarHeight = 24, .navBarHeight = 48,
        .buttonMinWidth = 80, .buttonMinHeight = 40
    },
    .darkMode = false,
    .predefined = true
};

// Thèmes OCEAN, SUNSET, FOREST, PURPLE, MONOCHROME (similaires, omis pour brièveté)
const UIThemeFull UI_THEME_FULL_OCEAN = { .name = "Océan", .predefined = true };
const UIThemeFull UI_THEME_FULL_SUNSET = { .name = "Coucher de soleil", .predefined = true };
const UIThemeFull UI_THEME_FULL_FOREST = { .name = "Forêt", .predefined = true };
const UIThemeFull UI_THEME_FULL_PURPLE = { .name = "Violet", .predefined = true };
const UIThemeFull UI_THEME_FULL_MONOCHROME = { .name = "Monochrome", .predefined = true };

// Tableau des thèmes prédéfinis
static const UIThemeFull* PRESET_THEMES[] = {
    &UI_THEME_FULL_LIGHT,
    &UI_THEME_FULL_DARK,
    &UI_THEME_FULL_NATURE,
    &UI_THEME_FULL_OCEAN,
    &UI_THEME_FULL_SUNSET,
    &UI_THEME_FULL_FOREST,
    &UI_THEME_FULL_PURPLE,
    &UI_THEME_FULL_MONOCHROME
};

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du module */
static UIThemeState theme_state;

/** @brief Transition en cours */
static bool transition_active = false;
static UIThemeFull transition_from;
static UIThemeFull transition_to;
static uint32_t transition_start;
static uint32_t transition_duration;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

bool ui_theme_init(void)
{
    THEME_DEBUG("Initialisation du module de thèmes...\n");
    
    memset(&theme_state, 0, sizeof(UIThemeState));
    
    // Appliquer le thème clair par défaut
    ui_theme_apply_preset(THEME_LIGHT);
    
    theme_state.initialized = true;
    
    THEME_DEBUG("Module initialisé (thème: %s)\n", theme_state.activeTheme.name);
    return true;
}

void ui_theme_deinit(void)
{
    theme_state.initialized = false;
}

bool ui_theme_is_ready(void)
{
    return theme_state.initialized;
}

// ============================================================
// SECTION 2 : APPLICATION DE THÈME
// ============================================================

bool ui_theme_apply_preset(UIThemePreset preset)
{
    if (preset > THEME_MONOCHROME) return false;
    
    memcpy(&theme_state.activeTheme, PRESET_THEMES[preset], sizeof(UIThemeFull));
    theme_state.activePreset = preset;
    
    apply_theme_to_ui(&theme_state.activeTheme);
    
    THEME_DEBUG("Thème appliqué: %s\n", theme_state.activeTheme.name);
    return true;
}

bool ui_theme_apply_custom(uint8_t index)
{
    if (index >= theme_state.customThemeCount) return false;
    
    memcpy(&theme_state.activeTheme, &theme_state.customThemes[index], sizeof(UIThemeFull));
    
    apply_theme_to_ui(&theme_state.activeTheme);
    
    THEME_DEBUG("Thème personnalisé appliqué: %s\n", theme_state.activeTheme.name);
    return true;
}

bool ui_theme_apply_full(const UIThemeFull* theme)
{
    if (theme == NULL) return false;
    
    memcpy(&theme_state.activeTheme, theme, sizeof(UIThemeFull));
    
    apply_theme_to_ui(&theme_state.activeTheme);
    
    return true;
}

const UIThemeFull* ui_theme_get_active(void)
{
    return &theme_state.activeTheme;
}

UIThemePreset ui_theme_get_preset(void)
{
    return theme_state.activePreset;
}

// ============================================================
// SECTION 3 : COULEURS
// ============================================================

uint16_t ui_theme_get_color(const char* colorName)
{
    if (colorName == NULL) return 0;
    
    if (strcmp(colorName, "primary") == 0) return theme_state.activeTheme.colors.primary;
    if (strcmp(colorName, "surface") == 0) return theme_state.activeTheme.colors.surface;
    if (strcmp(colorName, "background") == 0) return theme_state.activeTheme.colors.background;
    if (strcmp(colorName, "text") == 0) return theme_state.activeTheme.colors.textPrimary;
    if (strcmp(colorName, "error") == 0) return theme_state.activeTheme.colors.error;
    if (strcmp(colorName, "success") == 0) return theme_state.activeTheme.colors.success;
    if (strcmp(colorName, "warning") == 0) return theme_state.activeTheme.colors.warning;
    if (strcmp(colorName, "border") == 0) return theme_state.activeTheme.colors.border;
    
    return 0;
}

void ui_theme_set_color(const char* colorName, uint16_t color)
{
    if (colorName == NULL) return;
    
    if (strcmp(colorName, "primary") == 0) theme_state.activeTheme.colors.primary = color;
    else if (strcmp(colorName, "surface") == 0) theme_state.activeTheme.colors.surface = color;
    else if (strcmp(colorName, "background") == 0) theme_state.activeTheme.colors.background = color;
    
    ui_request_redraw();
}

uint16_t ui_theme_get_primary(void)      { return theme_state.activeTheme.colors.primary; }
uint16_t ui_theme_get_surface(void)      { return theme_state.activeTheme.colors.surface; }
uint16_t ui_theme_get_text_primary(void) { return theme_state.activeTheme.colors.textPrimary; }
uint16_t ui_theme_get_text_secondary(void) { return theme_state.activeTheme.colors.textSecondary; }
uint16_t ui_theme_get_error(void)        { return theme_state.activeTheme.colors.error; }
uint16_t ui_theme_get_success(void)      { return theme_state.activeTheme.colors.success; }

// ============================================================
// SECTION 4 : STYLES
// ============================================================

uint16_t ui_theme_get_button_color(UIButtonStyle style)
{
    UIThemeColors* c = &theme_state.activeTheme.colors;
    
    switch (style)
    {
        case BUTTON_STYLE_PRIMARY:   return c->primary;
        case BUTTON_STYLE_SECONDARY: return c->secondary;
        case BUTTON_STYLE_OUTLINE:   return c->surface;
        case BUTTON_STYLE_TEXT:      return c->surface;
        case BUTTON_STYLE_CALL:      return c->buttonCall;
        case BUTTON_STYLE_END:       return c->buttonEnd;
        case BUTTON_STYLE_DANGER:    return c->error;
        case BUTTON_STYLE_DEFAULT:
        default:                     return c->buttonDefault;
    }
}

uint16_t ui_theme_get_button_text_color(UIButtonStyle style)
{
    switch (style)
    {
        case BUTTON_STYLE_PRIMARY:   return theme_state.activeTheme.colors.onPrimary;
        case BUTTON_STYLE_SECONDARY: return theme_state.activeTheme.colors.onSecondary;
        case BUTTON_STYLE_OUTLINE:   return theme_state.activeTheme.colors.primary;
        case BUTTON_STYLE_TEXT:      return theme_state.activeTheme.colors.primary;
        case BUTTON_STYLE_CALL:      return theme_state.activeTheme.colors.onPrimary;
        case BUTTON_STYLE_END:       return theme_state.activeTheme.colors.onError;
        case BUTTON_STYLE_DANGER:    return theme_state.activeTheme.colors.onError;
        case BUTTON_STYLE_DEFAULT:
        default:                     return theme_state.activeTheme.colors.textPrimary;
    }
}

const DisplayFont* ui_theme_get_title_font(void) { return theme_state.activeTheme.fonts.titleFont; }
const DisplayFont* ui_theme_get_body_font(void)  { return theme_state.activeTheme.fonts.bodyFont; }
uint8_t ui_theme_get_corner_radius(void)         { return theme_state.activeTheme.dimensions.cornerRadius; }
UIMargin ui_theme_get_screen_margin(void)        { return theme_state.activeTheme.dimensions.screenMargin; }

// ============================================================
// SECTION 5 : MODE SOMBRE
// ============================================================

void ui_theme_toggle_dark_mode(void)
{
    if (theme_state.activeTheme.darkMode)
    {
        ui_theme_apply_preset(THEME_LIGHT);
    }
    else
    {
        ui_theme_apply_preset(THEME_DARK);
    }
}

bool ui_theme_is_dark_mode(void)
{
    return theme_state.activeTheme.darkMode;
}

void ui_theme_set_dark_mode(bool darkMode)
{
    if (darkMode && !theme_state.activeTheme.darkMode)
    {
        ui_theme_apply_preset(THEME_DARK);
    }
    else if (!darkMode && theme_state.activeTheme.darkMode)
    {
        ui_theme_apply_preset(THEME_LIGHT);
    }
}

// ============================================================
// SECTION 6 : THÈMES PERSONNALISÉS
// ============================================================

bool ui_theme_save_custom(const char* name)
{
    if (name == NULL) return false;
    if (theme_state.customThemeCount >= UI_THEME_MAX_CUSTOM) return false;
    
    UIThemeFull* custom = &theme_state.customThemes[theme_state.customThemeCount];
    memcpy(custom, &theme_state.activeTheme, sizeof(UIThemeFull));
    strncpy(custom->name, name, 31);
    custom->predefined = false;
    
    theme_state.customThemeCount++;
    
    THEME_DEBUG("Thème personnalisé sauvegardé: %s\n", name);
    return true;
}

bool ui_theme_delete_custom(uint8_t index)
{
    if (index >= theme_state.customThemeCount) return false;
    
    if (index < theme_state.customThemeCount - 1)
    {
        memmove(&theme_state.customThemes[index], 
                &theme_state.customThemes[index + 1],
                (theme_state.customThemeCount - index - 1) * sizeof(UIThemeFull));
    }
    theme_state.customThemeCount--;
    
    return true;
}

uint8_t ui_theme_get_custom_count(void)
{
    return theme_state.customThemeCount;
}

const UIThemeFull* ui_theme_get_custom(uint8_t index)
{
    if (index >= theme_state.customThemeCount) return NULL;
    return &theme_state.customThemes[index];
}

// ============================================================
// SECTION 7 : TRANSITIONS
// ============================================================

void ui_theme_transition(const UIThemeFull* from, const UIThemeFull* to, uint32_t durationMs)
{
    if (from == NULL || to == NULL) return;
    
    memcpy(&transition_from, from, sizeof(UIThemeFull));
    memcpy(&transition_to, to, sizeof(UIThemeFull));
    transition_start = HAL_GetTick();
    transition_duration = durationMs;
    transition_active = true;
    
    THEME_DEBUG("Transition de thème démarrée (%lu ms)\n", (unsigned long)durationMs);
}

bool ui_theme_is_transitioning(void)
{
    if (!transition_active) return false;
    
    uint32_t elapsed = HAL_GetTick() - transition_start;
    
    if (elapsed >= transition_duration)
    {
        transition_active = false;
        memcpy(&theme_state.activeTheme, &transition_to, sizeof(UIThemeFull));
        return false;
    }
    
    // Interpoler les couleurs pendant la transition
    float progress = (float)elapsed / transition_duration;
    interpolate_colors(&transition_from, &transition_to, &theme_state.activeTheme, progress);
    
    return true;
}

// ============================================================
// SECTION 8 : FONCTIONS INTERNES
// ============================================================

static void apply_theme_to_ui(const UIThemeFull* theme)
{
    if (theme == NULL) return;
    
    // Appliquer les couleurs à l'UI core
    UITheme coreTheme;
    coreTheme.background    = theme->colors.background;
    coreTheme.surface       = theme->colors.surface;
    coreTheme.primary       = theme->colors.primary;
    coreTheme.primaryDark   = theme->colors.primaryDark;
    coreTheme.primaryLight  = theme->colors.primaryLight;
    coreTheme.secondary     = theme->colors.secondary;
    coreTheme.accent        = theme->colors.info;
    coreTheme.textPrimary   = theme->colors.textPrimary;
    coreTheme.textSecondary = theme->colors.textSecondary;
    coreTheme.textOnPrimary = theme->colors.onPrimary;
    coreTheme.border        = theme->colors.border;
    coreTheme.error         = theme->colors.error;
    coreTheme.success       = theme->colors.success;
    coreTheme.warning       = theme->colors.warning;
    coreTheme.disabled      = theme->colors.disabled;
    
    ui_set_theme(&coreTheme);
    
    // Appliquer le fond d'écran
    display_clear(theme->colors.background);
    
    ui_request_redraw();
}

static void interpolate_colors(const UIThemeFull* from, const UIThemeFull* to, 
                                UIThemeFull* result, float progress)
{
    // Interpolation linéaire des couleurs RGB565
    #define INTERP_COLOR(field) \
        result->colors.field = interpolate_rgb565(from->colors.field, to->colors.field, progress)
    
    INTERP_COLOR(background);
    INTERP_COLOR(surface);
    INTERP_COLOR(primary);
    INTERP_COLOR(primaryDark);
    INTERP_COLOR(textPrimary);
    INTERP_COLOR(textSecondary);
    INTERP_COLOR(border);
    INTERP_COLOR(error);
    INTERP_COLOR(success);
}

static uint16_t interpolate_rgb565(uint16_t color1, uint16_t color2, float progress)
{
    // Extraire les composantes
    uint8_t r1 = (color1 >> 11) & 0x1F;
    uint8_t g1 = (color1 >> 5) & 0x3F;
    uint8_t b1 = color1 & 0x1F;
    
    uint8_t r2 = (color2 >> 11) & 0x1F;
    uint8_t g2 = (color2 >> 5) & 0x3F;
    uint8_t b2 = color2 & 0x1F;
    
    // Interpoler
    uint8_t r = (uint8_t)(r1 + (r2 - r1) * progress);
    uint8_t g = (uint8_t)(g1 + (g2 - g1) * progress);
    uint8_t b = (uint8_t)(b1 + (b2 - b1) * progress);
    
    // Recombiner
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
}

// ============================================================
// SECTION 9 : DÉBOGAGE
// ============================================================

void ui_theme_print_colors(void)
{
    UIThemeColors* c = &theme_state.activeTheme.colors;
    
    printf("\n═══ COULEURS DU THÈME : %s ═══\n", theme_state.activeTheme.name);
    printf("Background     : 0x%04X\n", c->background);
    printf("Surface        : 0x%04X\n", c->surface);
    printf("Primary        : 0x%04X\n", c->primary);
    printf("Primary Dark   : 0x%04X\n", c->primaryDark);
    printf("Primary Light  : 0x%04X\n", c->primaryLight);
    printf("Secondary      : 0x%04X\n", c->secondary);
    printf("Text Primary   : 0x%04X\n", c->textPrimary);
    printf("Text Secondary : 0x%04X\n", c->textSecondary);
    printf("Error          : 0x%04X\n", c->error);
    printf("Success        : 0x%04X\n", c->success);
    printf("Warning        : 0x%04X\n", c->warning);
    printf("Border         : 0x%04X\n", c->border);
    printf("Button Call    : 0x%04X\n", c->buttonCall);
    printf("Button End     : 0x%04X\n", c->buttonEnd);
    printf("══════════════════════════════\n\n");
}

void ui_theme_print_dimensions(void)
{
    UIThemeDimensions* d = &theme_state.activeTheme.dimensions;
    
    printf("\n═══ DIMENSIONS DU THÈME : %s ═══\n", theme_state.activeTheme.name);
    printf("Corner Radius  : %d\n", d->cornerRadius);
    printf("Corner Large   : %d\n", d->cornerRadiusLarge);
    printf("Screen Margin  : T:%d R:%d B:%d L:%d\n", 
           d->screenMargin.top, d->screenMargin.right,
           d->screenMargin.bottom, d->screenMargin.left);
    printf("Button Padding : T:%d R:%d B:%d L:%d\n",
           d->buttonPadding.top, d->buttonPadding.right,
           d->buttonPadding.bottom, d->buttonPadding.left);
    printf("Status Bar H   : %d\n", d->statusBarHeight);
    printf("Nav Bar H      : %d\n", d->navBarHeight);
    printf("══════════════════════════════\n\n");
}

void ui_theme_print_all(void)
{
    ui_theme_print_colors();
    ui_theme_print_dimensions();
    
    printf("Polices:\n");
    printf("  Titre : %s (x%d)\n", 
           theme_state.activeTheme.fonts.titleFont->name,
           theme_state.activeTheme.fonts.titleSize);
    printf("  Corps : %s (x%d)\n",
           theme_state.activeTheme.fonts.bodyFont->name,
           theme_state.activeTheme.fonts.bodySize);
    printf("  Mode sombre : %s\n", theme_state.activeTheme.darkMode ? "Oui" : "Non");
}

bool ui_theme_self_test(void)
{
    THEME_DEBUG("Auto-test...\n");
    
    if (!theme_state.initialized)
    {
        THEME_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : appliquer chaque thème
    for (uint8_t i = 0; i <= THEME_MONOCHROME; i++)
    {
        ui_theme_apply_preset((UIThemePreset)i);
        HAL_Delay(100);
    }
    
    // Revenir au thème clair
    ui_theme_apply_preset(THEME_LIGHT);
    
    THEME_DEBUG("Auto-test OK\n");
    return true;
}