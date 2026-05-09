/**
 * @file ui_navigation.cpp
 * @brief Implémentation du widget Barre de Navigation
 * 
 * Fonctionnalités :
 * - Barre supérieure (titre + retour + menu)
 * - Barre d'onglets (tabs) avec badges
 * - Barre d'outils (actions)
 * - Styles (défaut, transparent, surélevé, compact)
 * - Callbacks de navigation
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_navigation.h"
#include "ui_fonts.h"
#include "ui_icons.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// FONCTIONS DE DESSIN
// ============================================================

/**
 * @brief Dessine la barre de navigation selon son type
 */
static void navbar_draw(UIWidget* widget)
{
    UINavigationBar* navbar = (UINavigationBar*)widget;
    if (navbar == NULL) return;
    
    switch (navbar->type)
    {
        case NAV_TYPE_TOP_BAR:
            draw_top_bar(navbar);
            break;
        case NAV_TYPE_TAB_BAR:
            draw_tab_bar(navbar);
            break;
        case NAV_TYPE_TOOLBAR:
            draw_toolbar(navbar);
            break;
    }
}

/**
 * @brief Dessine la barre supérieure
 */
static void draw_top_bar(UINavigationBar* navbar)
{
    UIRect* r = &navbar->base.rect;
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    uint16_t bgColor = navbar->backgroundColor ? navbar->backgroundColor : colors->primaryDark;
    uint16_t titleColor = navbar->titleColor ? navbar->titleColor : colors->onPrimary;
    uint16_t iconColor = navbar->iconColor ? navbar->iconColor : colors->onPrimary;
    
    // Mise à jour de la hauteur si nécessaire
    if (r->height == 0) r->height = NAVBAR_TOP_HEIGHT;
    
    // Fond
    if (navbar->style == NAV_STYLE_TRANSPARENT)
    {
        // Pas de fond (transparent)
    }
    else
    {
        display_fill_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1, bgColor);
    }
    
    // Ombre si style élevé
    if (navbar->style == NAV_STYLE_ELEVATED)
    {
        display_fill_rect(r->x, r->y + r->height - 2, r->x + r->width - 1, 
                         r->y + r->height - 1, colors->shadow);
    }
    
    // Séparateur en bas
    if (navbar->separatorColor)
    {
        display_draw_hline(r->x, r->y + r->height - 1, r->x + r->width - 1, 
                          navbar->separatorColor);
    }
    else
    {
        display_draw_hline(r->x, r->y + r->height - 1, r->x + r->width - 1, colors->border);
    }
    
    uint16_t iconY = r->y + (r->height - ICON_SIZE_16) / 2;
    
    // --- Bouton Retour (à gauche) ---
    if (navbar->showBackButton)
    {
        ui_icons_draw(ICON_BACK, r->x + 8, iconY, iconColor);
    }
    
    // --- Titre (centré) ---
    if (strlen(navbar->title) > 0)
    {
        display_set_font(&font_8x16);
        display_set_text_color(titleColor);
        
        uint16_t titleWidth = display_text_width(navbar->title, 1);
        uint16_t titleX = r->x + (r->width - titleWidth) / 2;
        uint16_t titleY = r->y + (r->height - display_text_height(1)) / 2;
        
        // Limiter à l'espace disponible (entre les boutons)
        uint16_t maxTitleWidth = r->width - 100;  // Espace pour les boutons
        if (titleWidth > maxTitleWidth)
        {
            titleWidth = maxTitleWidth;
            // Tronquer le titre
            char truncatedTitle[64];
            uint16_t charsToFit = ui_fonts_fit_text(&UI_FONT_8X16, navbar->title, 1, maxTitleWidth - 10);
            strncpy(truncatedTitle, navbar->title, charsToFit - 3);
            strcat(truncatedTitle, "...");
            display_draw_text(titleX, titleY, truncatedTitle, titleColor, 1);
        }
        else
        {
            display_draw_text(titleX, titleY, navbar->title, titleColor, 1);
        }
    }
    
    // --- Bouton Menu (à droite) ---
    if (navbar->showMenuButton)
    {
        uint16_t menuX = r->x + r->width - ICON_SIZE_16 - 8;
        ui_icons_draw(ICON_MENU, menuX, iconY, iconColor);
    }
}

/**
 * @brief Dessine la barre d'onglets
 */
static void draw_tab_bar(UINavigationBar* navbar)
{
    UIRect* r = &navbar->base.rect;
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    if (navbar->tabCount == 0) return;
    if (r->height == 0) r->height = NAVBAR_TAB_HEIGHT;
    
    // Fond
    uint16_t bgColor = navbar->backgroundColor ? navbar->backgroundColor : colors->surface;
    display_fill_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1, bgColor);
    
    // Séparateur en haut
    display_draw_hline(r->x, r->y, r->x + r->width - 1, colors->border);
    
    // Largeur de chaque onglet
    uint16_t tabWidth = r->width / navbar->tabCount;
    uint16_t selectedColor = navbar->selectedColor ? navbar->selectedColor : colors->primary;
    uint16_t iconColor = navbar->iconColor ? navbar->iconColor : colors->textSecondary;
    
    for (uint8_t i = 0; i < navbar->tabCount; i++)
    {
        NavTab* tab = &navbar->tabs[i];
        uint16_t tabX = r->x + i * tabWidth;
        bool selected = (i == navbar->selectedTab);
        
        // Couleurs selon sélection
        uint16_t currentIconColor = selected ? selectedColor : iconColor;
        uint16_t currentTextColor = selected ? selectedColor : colors->textSecondary;
        
        // Indicateur de sélection (ligne en haut)
        if (selected)
        {
            display_fill_rect(tabX + tabWidth/4, r->y, tabX + 3*tabWidth/4 - 1, r->y + 2, selectedColor);
        }
        
        // Icône
        IconID icon = selected ? tab->selectedIcon : tab->icon;
        if (icon == 0) icon = tab->icon;
        
        uint16_t iconX = tabX + (tabWidth - ICON_SIZE_16) / 2;
        uint16_t iconY = r->y + 6;
        ui_icons_draw(icon, iconX, iconY, currentIconColor);
        
        // Titre
        display_set_font(&font_5x7);
        display_set_text_color(currentTextColor);
        
        uint16_t textWidth = display_text_width(tab->title, 1);
        uint16_t textX = tabX + (tabWidth - textWidth) / 2;
        uint16_t textY = r->y + r->height - display_text_height(1) - 4;
        
        display_draw_text(textX, textY, tab->title, currentTextColor, 1);
        
        // Badge de notification
        if (tab->badgeCount > 0)
        {
            uint16_t badgeX = iconX + ICON_SIZE_16 - 6;
            uint16_t badgeY = iconY - 4;
            uint16_t badgeSize = 14;
            
            // Fond rouge
            display_fill_circle(badgeX + badgeSize/2, badgeY + badgeSize/2, 
                               badgeSize/2, colors->error);
            
            // Texte blanc
            char badgeStr[4];
            snprintf(badgeStr, sizeof(badgeStr), "%d", tab->badgeCount > 99 ? 99 : tab->badgeCount);
            
            display_set_text_color(colors->onError);
            uint16_t badgeTextW = display_text_width(badgeStr, 1);
            display_draw_text(badgeX + (badgeSize - badgeTextW)/2, badgeY + 2, 
                            badgeStr, colors->onError, 1);
        }
    }
}

/**
 * @brief Dessine la barre d'outils
 */
static void draw_toolbar(UINavigationBar* navbar)
{
    UIRect* r = &navbar->base.rect;
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    if (navbar->actionCount == 0) return;
    if (r->height == 0) r->height = NAVBAR_TOP_HEIGHT;
    
    // Fond
    uint16_t bgColor = navbar->backgroundColor ? navbar->backgroundColor : colors->surface;
    display_fill_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1, bgColor);
    
    // Largeur par action
    uint16_t actionWidth = r->width / navbar->actionCount;
    uint16_t iconColor = navbar->iconColor ? navbar->iconColor : colors->textPrimary;
    
    for (uint8_t i = 0; i < navbar->actionCount; i++)
    {
        NavAction* action = &navbar->actions[i];
        uint16_t actionX = r->x + i * actionWidth;
        
        uint16_t currentColor = action->enabled ? iconColor : colors->disabled;
        
        // Icône
        uint16_t iconX = actionX + (actionWidth - ICON_SIZE_16) / 2;
        uint16_t iconY = r->y + 4;
        ui_icons_draw(action->icon, iconX, iconY, currentColor);
        
        // Label
        if (strlen(action->label) > 0)
        {
            display_set_font(&font_5x7);
            display_set_text_color(currentColor);
            
            uint16_t textWidth = display_text_width(action->label, 1);
            uint16_t textX = actionX + (actionWidth - textWidth) / 2;
            uint16_t textY = r->y + r->height - display_text_height(1) - 4;
            
            display_draw_text(textX, textY, action->label, currentColor, 1);
        }
    }
}

// ============================================================
// FONCTIONS TACTILES
// ============================================================

static void navbar_touch(UIWidget* widget, uint16_t x, uint16_t y, TouchEvent event)
{
    UINavigationBar* navbar = (UINavigationBar*)widget;
    if (navbar == NULL || event != TOUCH_EVENT_PRESS) return;
    
    UIRect* r = &widget->rect;
    
    switch (navbar->type)
    {
        case NAV_TYPE_TOP_BAR:
        {
            // Bouton retour (zone gauche)
            if (navbar->showBackButton && x < 50)
            {
                if (navbar->onBack) navbar->onBack(navbar);
                return;
            }
            
            // Bouton menu (zone droite)
            if (navbar->showMenuButton && x > r->width - 50)
            {
                if (navbar->onMenu) navbar->onMenu(navbar);
                return;
            }
            
            // Tap sur le titre
            if (navbar->onTitleTap && x > 50 && x < r->width - 50)
            {
                navbar->onTitleTap(navbar);
            }
            break;
        }
        
        case NAV_TYPE_TAB_BAR:
        {
            if (navbar->tabCount == 0) return;
            
            uint16_t tabWidth = r->width / navbar->tabCount;
            uint8_t tappedTab = x / tabWidth;
            
            if (tappedTab < navbar->tabCount)
            {
                ui_navbar_select_tab(navbar, tappedTab);
            }
            break;
        }
        
        case NAV_TYPE_TOOLBAR:
        {
            if (navbar->actionCount == 0) return;
            
            uint16_t actionWidth = r->width / navbar->actionCount;
            uint8_t tappedAction = x / actionWidth;
            
            if (tappedAction < navbar->actionCount && navbar->actions[tappedAction].enabled)
            {
                if (navbar->actions[tappedAction].onAction)
                {
                    navbar->actions[tappedAction].onAction();
                }
            }
            break;
        }
    }
}

// ============================================================
// CRÉATION
// ============================================================

UINavigationBar* ui_navbar_create_top(const char* name, const char* title)
{
    UINavigationBar* navbar = (UINavigationBar*)calloc(1, sizeof(UINavigationBar));
    if (navbar == NULL) return NULL;
    
    if (name) strncpy(navbar->base.name, name, 31);
    navbar->base.type = WIDGET_TYPE_CUSTOM + 20;
    navbar->base.rect = (UIRect){0, 0, DISPLAY_WIDTH, NAVBAR_TOP_HEIGHT};
    navbar->base.visible = true;
    navbar->base.enabled = true;
    navbar->base.canFocus = false;
    
    navbar->base.draw = navbar_draw;
    navbar->base.onTouch = navbar_touch;
    
    navbar->type = NAV_TYPE_TOP_BAR;
    navbar->style = NAV_STYLE_DEFAULT;
    navbar->showBackButton = true;
    navbar->showMenuButton = false;
    
    if (title) strncpy(navbar->title, title, 63);
    
    return navbar;
}

UINavigationBar* ui_navbar_create_tabs(const char* name)
{
    UINavigationBar* navbar = (UINavigationBar*)calloc(1, sizeof(UINavigationBar));
    if (navbar == NULL) return NULL;
    
    if (name) strncpy(navbar->base.name, name, 31);
    navbar->base.type = WIDGET_TYPE_CUSTOM + 21;
    navbar->base.rect = (UIRect){0, DISPLAY_HEIGHT - NAVBAR_TAB_HEIGHT, DISPLAY_WIDTH, NAVBAR_TAB_HEIGHT};
    navbar->base.visible = true;
    navbar->base.enabled = true;
    
    navbar->base.draw = navbar_draw;
    navbar->base.onTouch = navbar_touch;
    
    navbar->type = NAV_TYPE_TAB_BAR;
    navbar->style = NAV_STYLE_DEFAULT;
    navbar->selectedTab = 0;
    
    return navbar;
}

UINavigationBar* ui_navbar_create_toolbar(const char* name)
{
    UINavigationBar* navbar = (UINavigationBar*)calloc(1, sizeof(UINavigationBar));
    if (navbar == NULL) return NULL;
    
    if (name) strncpy(navbar->base.name, name, 31);
    navbar->base.type = WIDGET_TYPE_CUSTOM + 22;
    navbar->base.rect = (UIRect){0, DISPLAY_HEIGHT - NAVBAR_TOP_HEIGHT, DISPLAY_WIDTH, NAVBAR_TOP_HEIGHT};
    navbar->base.visible = true;
    navbar->base.enabled = true;
    
    navbar->base.draw = navbar_draw;
    navbar->base.onTouch = navbar_touch;
    
    navbar->type = NAV_TYPE_TOOLBAR;
    navbar->style = NAV_STYLE_DEFAULT;
    
    return navbar;
}

// ============================================================
// CONFIGURATION TOP BAR
// ============================================================

void ui_navbar_set_title(UINavigationBar* navbar, const char* title)
{
    if (navbar == NULL) return;
    if (title) strncpy(navbar->title, title, 63);
    else navbar->title[0] = '\0';
    navbar->base.needsRedraw = true;
}

void ui_navbar_show_back(UINavigationBar* navbar, bool show)
{
    if (navbar == NULL) return;
    navbar->showBackButton = show;
    navbar->base.needsRedraw = true;
}

void ui_navbar_show_menu(UINavigationBar* navbar, bool show)
{
    if (navbar == NULL) return;
    navbar->showMenuButton = show;
    navbar->base.needsRedraw = true;
}

void ui_navbar_set_colors(UINavigationBar* navbar, uint16_t bg, uint16_t title, uint16_t icons)
{
    if (navbar == NULL) return;
    navbar->backgroundColor = bg;
    navbar->titleColor = title;
    navbar->iconColor = icons;
    navbar->base.needsRedraw = true;
}

void ui_navbar_set_style(UINavigationBar* navbar, NavBarStyle style)
{
    if (navbar == NULL) return;
    navbar->style = style;
    navbar->base.needsRedraw = true;
}

// ============================================================
// CONFIGURATION TAB BAR
// ============================================================

bool ui_navbar_add_tab(UINavigationBar* navbar, const char* title, IconID icon, UIScreen* screen)
{
    if (navbar == NULL || navbar->tabCount >= NAVBAR_MAX_TABS) return false;
    
    NavTab* tab = &navbar->tabs[navbar->tabCount];
    memset(tab, 0, sizeof(NavTab));
    
    if (title) strncpy(tab->title, title, 15);
    tab->icon = icon;
    tab->selectedIcon = icon;  // Par défaut, même icône
    tab->screen = screen;
    tab->selected = false;
    
    navbar->tabCount++;
    navbar->base.needsRedraw = true;
    
    return true;
}

bool ui_navbar_remove_tab(UINavigationBar* navbar, uint8_t index)
{
    if (navbar == NULL || index >= navbar->tabCount) return false;
    
    if (index < navbar->tabCount - 1)
    {
        memmove(&navbar->tabs[index], &navbar->tabs[index + 1],
                (navbar->tabCount - index - 1) * sizeof(NavTab));
    }
    navbar->tabCount--;
    navbar->base.needsRedraw = true;
    
    return true;
}

void ui_navbar_select_tab(UINavigationBar* navbar, uint8_t index)
{
    if (navbar == NULL || index >= navbar->tabCount) return;
    
    navbar->selectedTab = index;
    
    // Désélectionner les autres
    for (uint8_t i = 0; i < navbar->tabCount; i++)
    {
        navbar->tabs[i].selected = (i == index);
    }
    
    navbar->base.needsRedraw = true;
    
    if (navbar->onTabSelected) navbar->onTabSelected(navbar, index);
}

uint8_t ui_navbar_get_selected_tab(UINavigationBar* navbar)
{
    return (navbar != NULL) ? navbar->selectedTab : 0;
}

void ui_navbar_set_tab_badge(UINavigationBar* navbar, uint8_t index, uint8_t count)
{
    if (navbar == NULL || index >= navbar->tabCount) return;
    navbar->tabs[index].badgeCount = count;
    navbar->base.needsRedraw = true;
}

// ============================================================
// CONFIGURATION TOOLBAR
// ============================================================

bool ui_navbar_add_action(UINavigationBar* navbar, IconID icon, const char* label, void (*action)(void))
{
    if (navbar == NULL || navbar->actionCount >= NAVBAR_MAX_ACTIONS) return false;
    
    NavAction* act = &navbar->actions[navbar->actionCount];
    memset(act, 0, sizeof(NavAction));
    
    act->icon = icon;
    if (label) strncpy(act->label, label, 15);
    act->onAction = action;
    act->enabled = true;
    
    navbar->actionCount++;
    navbar->base.needsRedraw = true;
    
    return true;
}

void ui_navbar_enable_action(UINavigationBar* navbar, uint8_t index, bool enable)
{
    if (navbar == NULL || index >= navbar->actionCount) return;
    navbar->actions[index].enabled = enable;
    navbar->base.needsRedraw = true;
}