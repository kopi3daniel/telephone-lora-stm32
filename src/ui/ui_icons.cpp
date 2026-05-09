/**
 * @file ui_icons.cpp
 * @brief Implémentation du module d'icônes bitmap
 * 
 * Fonctionnalités :
 * - 78 icônes prédéfinies en bitmap RGB565
 * - Dessin simple et coloré
 * - Icônes de statut (batterie, signal, notifications)
 * - Icônes centrées dans des rectangles
 * - Icônes personnalisées chargeables
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_icons.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// DONNÉES DES ICÔNES (BITMAPS RGB565)
// ============================================================

// Les icônes sont stockées en Flash pour économiser la RAM.
// Chaque icône fait 16×16 pixels = 256 pixels × 2 octets = 512 octets.

/** @brief Icône RETOUR (flèche gauche) 16×16 */
static const uint16_t icon_back_16x16[] = {
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0xFFFF,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0xFFFF,0xFFFF,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0xFFFF,0xFFFF,0xFFFF,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0x0000,0x0000,
    0x0000,0x0000,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0x0000,0x0000,
    0x0000,0x0000,0x0000,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0xFFFF,0xFFFF,0xFFFF,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0xFFFF,0xFFFF,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0xFFFF,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
};

/** @brief Icône MENU (trois lignes) 16×16 */
static const uint16_t icon_menu_16x16[] = {
    0x0000,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0x0000,
    0x0000,0xFFFF,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0xFFFF,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0x0000,
    0x0000,0xFFFF,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0xFFFF,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0x0000,
    0x0000,0xFFFF,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0xFFFF,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
};

/** @brief Icône APPEL (combiné) 16×16 */
static const uint16_t icon_call_16x16[] = {
    0x0000,0x0000,0x0000,0x0000,0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,0x0000,0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x07E0,0x07E0,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x07E0,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x07E0,0x0000,0x0000,0x0000,
    0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x07E0,0x07E0,0x0000,
    0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x07E0,0x07E0,0x0000,
    0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x07E0,0x07E0,0x0000,
    0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x07E0,0x07E0,0x0000,
    0x0000,0x0000,0x07E0,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x07E0,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x07E0,0x07E0,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,0x0000,0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x07E0,0x07E0,0x07E0,0x0000,0x0000,0x07E0,0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x07E0,0x07E0,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
};

// Les 75 autres icônes sont similaires (données omises par souci de place)
// Dans un vrai projet, toutes les icônes seraient définies ici avec leurs bitmaps

// ============================================================
// TABLE DES ICÔNES
// ============================================================

/** @brief Tableau de toutes les icônes prédéfinies */
static const UIIcon icons_table[ICON_COUNT] = {
    // Navigation (0-9)
    [ICON_BACK]           = { icon_back_16x16, 16, 16, "back", ICON_BACK, true },
    [ICON_MENU]           = { icon_menu_16x16, 16, 16, "menu", ICON_MENU, true },
    [ICON_HOME]           = { NULL, 16, 16, "home", ICON_HOME, true },
    [ICON_SETTINGS]       = { NULL, 16, 16, "settings", ICON_SETTINGS, true },
    [ICON_SEARCH]         = { NULL, 16, 16, "search", ICON_SEARCH, true },
    [ICON_CLOSE]          = { NULL, 16, 16, "close", ICON_CLOSE, true },
    [ICON_ARROW_UP]       = { NULL, 16, 16, "arrow_up", ICON_ARROW_UP, true },
    [ICON_ARROW_DOWN]     = { NULL, 16, 16, "arrow_down", ICON_ARROW_DOWN, true },
    [ICON_ARROW_LEFT]     = { NULL, 16, 16, "arrow_left", ICON_ARROW_LEFT, true },
    [ICON_ARROW_RIGHT]    = { NULL, 16, 16, "arrow_right", ICON_ARROW_RIGHT, true },
    
    // Communication (10-19)
    [ICON_CALL]           = { icon_call_16x16, 16, 16, "call", ICON_CALL, true },
    [ICON_CALL_END]       = { NULL, 16, 16, "call_end", ICON_CALL_END, true },
    [ICON_CALL_INCOMING]  = { NULL, 16, 16, "call_incoming", ICON_CALL_INCOMING, true },
    [ICON_CALL_OUTGOING]  = { NULL, 16, 16, "call_outgoing", ICON_CALL_OUTGOING, true },
    [ICON_CALL_MISSED]    = { NULL, 16, 16, "call_missed", ICON_CALL_MISSED, true },
    [ICON_MESSAGE]        = { NULL, 16, 16, "message", ICON_MESSAGE, true },
    [ICON_CONTACTS]       = { NULL, 16, 16, "contacts", ICON_CONTACTS, true },
    [ICON_DIALER]         = { NULL, 16, 16, "dialer", ICON_DIALER, true },
    [ICON_SPEAKER]        = { NULL, 16, 16, "speaker", ICON_SPEAKER, true },
    [ICON_MUTE]           = { NULL, 16, 16, "mute", ICON_MUTE, true },
    
    // Pour les autres icônes, on utilise NULL (icône vide par défaut)
    // Dans un projet complet, chaque icône aurait ses données bitmap
};

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du module */
static UIIconsState icons_state;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

bool ui_icons_init(void)
{
    ICONS_DEBUG("Initialisation du module d'icônes...\n");
    
    memset(&icons_state, 0, sizeof(UIIconsState));
    icons_state.initialized = true;
    
    ICONS_DEBUG("Module initialisé (%d icônes)\n", ICON_COUNT);
    return true;
}

void ui_icons_deinit(void)
{
    for (uint8_t i = 0; i < icons_state.customIconCount; i++)
    {
        if (icons_state.customIcons[i].data)
            free(icons_state.customIcons[i].data);
    }
    icons_state.initialized = false;
}

bool ui_icons_is_ready(void)
{
    return icons_state.initialized;
}

// ============================================================
// SECTION 2 : RECHERCHE D'ICÔNES
// ============================================================

const UIIcon* ui_icons_get(IconID id)
{
    if (id >= ICON_COUNT) return NULL;
    return &icons_table[id];
}

const UIIcon* ui_icons_get_by_name(const char* name)
{
    if (name == NULL) return NULL;
    
    for (uint16_t i = 0; i < ICON_COUNT; i++)
    {
        if (icons_table[i].name && strcmp(icons_table[i].name, name) == 0)
            return &icons_table[i];
    }
    
    for (uint8_t i = 0; i < icons_state.customIconCount; i++)
    {
        if (strcmp(icons_state.customIcons[i].icon.name, name) == 0)
            return &icons_state.customIcons[i].icon;
    }
    
    return NULL;
}

// ============================================================
// SECTION 3 : DESSIN D'ICÔNES
// ============================================================

void ui_icons_draw(IconID id, uint16_t x, uint16_t y, uint16_t color)
{
    const UIIcon* icon = ui_icons_get(id);
    if (icon == NULL || icon->data == NULL) return;
    
    for (uint16_t row = 0; row < icon->height; row++)
    {
        for (uint16_t col = 0; col < icon->width; col++)
        {
            uint16_t pixel = icon->data[row * icon->width + col];
            
            // Remplacer les pixels non noirs par la couleur spécifiée
            if (pixel != 0x0000)
            {
                display_draw_pixel(x + col, y + row, color);
            }
        }
    }
}

void ui_icons_draw_colored(IconID id, uint16_t x, uint16_t y,
                             uint16_t primaryColor, uint16_t bgColor)
{
    const UIIcon* icon = ui_icons_get(id);
    if (icon == NULL || icon->data == NULL) return;
    
    for (uint16_t row = 0; row < icon->height; row++)
    {
        for (uint16_t col = 0; col < icon->width; col++)
        {
            uint16_t pixel = icon->data[row * icon->width + col];
            
            if (pixel == 0x0000)
            {
                // Fond transparent
                if (bgColor != 0x0000)
                {
                    display_draw_pixel(x + col, y + row, bgColor);
                }
            }
            else if (pixel == 0xFFFF)
            {
                // Blanc → couleur principale
                display_draw_pixel(x + col, y + row, primaryColor);
            }
            else
            {
                // Autre couleur → conserver
                display_draw_pixel(x + col, y + row, pixel);
            }
        }
    }
}

void ui_icons_draw_centered(IconID id, UIRect* rect, uint16_t color)
{
    const UIIcon* icon = ui_icons_get(id);
    if (icon == NULL || rect == NULL) return;
    
    uint16_t x = rect->x + (rect->width - icon->width) / 2;
    uint16_t y = rect->y + (rect->height - icon->height) / 2;
    
    ui_icons_draw(id, x, y, color);
}

// ============================================================
// SECTION 4 : ICÔNES DE STATUT
// ============================================================

void ui_icons_draw_battery(uint16_t x, uint16_t y, uint8_t percent, bool charging)
{
    // Utiliser l'icône appropriée selon le niveau
    IconID batteryIcon;
    
    if (charging)
    {
        batteryIcon = ICON_BATTERY_CHARGING;
    }
    else if (percent > 75)
    {
        batteryIcon = ICON_BATTERY_FULL;
    }
    else if (percent > 25)
    {
        batteryIcon = ICON_BATTERY_HALF;
    }
    else
    {
        batteryIcon = ICON_BATTERY_LOW;
    }
    
    uint16_t color = (percent < 15) ? ILI9488_RED : ILI9488_WHITE;
    ui_icons_draw(batteryIcon, x, y, color);
}

void ui_icons_draw_signal(uint16_t x, uint16_t y, uint8_t level)
{
    IconID signalIcon = (level > 2) ? ICON_SIGNAL_FULL : ICON_SIGNAL_LOW;
    uint16_t color = (level == 0) ? ILI9488_RED : ILI9488_WHITE;
    ui_icons_draw(signalIcon, x, y, color);
}

void ui_icons_draw_lock(uint16_t x, uint16_t y, bool locked)
{
    ui_icons_draw(locked ? ICON_LOCK : ICON_UNLOCK, x, y, ILI9488_WHITE);
}

void ui_icons_draw_bell(uint16_t x, uint16_t y, bool muted)
{
    ui_icons_draw(muted ? ICON_BELL_MUTE : ICON_BELL, x, y, ILI9488_WHITE);
}

// ============================================================
// SECTION 5 : ICÔNES PERSONNALISÉES
// ============================================================

bool ui_icons_load_custom(const char* name, const uint16_t* data, uint8_t width, uint8_t height)
{
    if (name == NULL || data == NULL) return false;
    if (icons_state.customIconCount >= UI_ICONS_MAX_CUSTOM) return false;
    
    UICustomIcon* custom = &icons_state.customIcons[icons_state.customIconCount];
    memset(custom, 0, sizeof(UICustomIcon));
    
    uint16_t pixelCount = width * height;
    custom->data = (uint16_t*)malloc(pixelCount * sizeof(uint16_t));
    if (custom->data == NULL) return false;
    memcpy(custom->data, data, pixelCount * sizeof(uint16_t));
    
    custom->icon.data = custom->data;
    custom->icon.width = width;
    custom->icon.height = height;
    custom->icon.name = name;
    custom->icon.id = (IconID)(ICON_COUNT + icons_state.customIconCount);
    custom->icon.predefined = false;
    custom->loaded = true;
    
    icons_state.customIconCount++;
    
    ICONS_DEBUG("Icône personnalisée chargée: %s (%dx%d)\n", name, width, height);
    return true;
}

bool ui_icons_unload_custom(uint8_t index)
{
    if (index >= icons_state.customIconCount) return false;
    
    if (icons_state.customIcons[index].data)
        free(icons_state.customIcons[index].data);
    
    if (index < icons_state.customIconCount - 1)
    {
        memmove(&icons_state.customIcons[index], &icons_state.customIcons[index + 1],
                (icons_state.customIconCount - index - 1) * sizeof(UICustomIcon));
    }
    icons_state.customIconCount--;
    
    return true;
}

const UIIcon* ui_icons_get_custom(uint8_t index)
{
    if (index >= icons_state.customIconCount) return NULL;
    return &icons_state.customIcons[index].icon;
}

uint8_t ui_icons_get_custom_count(void)
{
    return icons_state.customIconCount;
}

// ============================================================
// SECTION 6 : DÉBOGAGE
// ============================================================

void ui_icons_print_list(void)
{
    printf("\n═══ ICÔNES DISPONIBLES (%d) ═══\n", ICON_COUNT);
    
    uint8_t count = 0;
    for (uint16_t i = 0; i < ICON_COUNT && count < 20; i++)
    {
        const UIIcon* icon = &icons_table[i];
        if (icon->name)
        {
            printf("  [%2d] %-20s %dx%d %s\n", i, icon->name,
                   icon->width, icon->height,
                   icon->data ? "(données)" : "(vide)");
            count++;
        }
    }
    if (ICON_COUNT > 20) printf("  ... et %d autres\n", ICON_COUNT - 20);
    printf("══════════════════════════════\n\n");
}

void ui_icons_print_info(IconID id)
{
    const UIIcon* icon = ui_icons_get(id);
    if (icon == NULL || icon->name == NULL)
    {
        printf("[ICONS] Icône %d : non définie\n", id);
        return;
    }
    
    printf("\n═══ ICÔNE : %s (ID=%d) ═══\n", icon->name, id);
    printf("Dimensions : %d × %d pixels\n", icon->width, icon->height);
    printf("Prédéfinie : %s\n", icon->predefined ? "Oui" : "Non");
    printf("Données    : %s\n", icon->data ? "Présentes" : "Vides");
    printf("══════════════════════════\n\n");
}

bool ui_icons_self_test(void)
{
    ICONS_DEBUG("Auto-test...\n");
    
    if (!icons_state.initialized)
    {
        ICONS_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Vérifier que les icônes principales sont accessibles
    const UIIcon* icon = ui_icons_get(ICON_BACK);
    if (icon == NULL)
    {
        ICONS_DEBUG("Échec : icône BACK inaccessible\n");
        return false;
    }
    
    // Test de dessin rapide
    ui_icons_draw(ICON_BACK, 10, 10, ILI9488_WHITE);
    ui_icons_draw_battery(280, 5, 75, false);
    ui_icons_draw_signal(260, 5, 4);
    
    ICONS_DEBUG("Auto-test OK\n");
    return true;
}