/**
 * @file ui_core.cpp
 * @brief Implémentation du noyau du framework UI
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans ui_core.h.
 * 
 * Il gère :
 * - La navigation entre écrans (pile d'écrans)
 * - Les fenêtres modales
 * - Le rendu graphique
 * - La distribution des événements
 * - Les animations
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_core.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// THÈMES PRÉDÉFINIS
// ============================================================

/** @brief Thème clair */
const UITheme UI_THEME_LIGHT = {
    .background     = ILI9488_WHITE,
    .surface        = 0xF7F7F7,
    .primary        = 0x07E0,      // Vert
    .primaryDark    = 0x03A0,
    .primaryLight   = 0x8FD0,
    .secondary      = 0x5AEB,
    .accent         = 0x001F,      // Bleu
    .textPrimary    = 0x0000,      // Noir
    .textSecondary  = 0x8410,      // Gris
    .textOnPrimary  = 0xFFFF,      // Blanc
    .border         = 0xC618,
    .error          = 0xF800,      // Rouge
    .success        = 0x07E0,      // Vert
    .warning        = 0xFFE0,      // Jaune
    .disabled       = 0xAD55
};

/** @brief Thème sombre */
const UITheme UI_THEME_DARK = {
    .background     = 0x0000,      // Noir
    .surface        = 0x18E3,
    .primary        = 0x07E0,
    .primaryDark    = 0x03A0,
    .primaryLight   = 0x8FD0,
    .secondary      = 0x5AEB,
    .accent         = 0x001F,
    .textPrimary    = 0xFFFF,      // Blanc
    .textSecondary  = 0x8410,
    .textOnPrimary  = 0xFFFF,
    .border         = 0x4208,
    .error          = 0xF800,
    .success        = 0x07E0,
    .warning        = 0xFFE0,
    .disabled       = 0x39C7
};

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État global du framework UI */
static UICoreState ui_state;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le framework UI
 */
bool ui_core_init(void)
{
    UI_DEBUG("Initialisation du framework UI...\n");
    
    memset(&ui_state, 0, sizeof(UICoreState));
    
    // Thème par défaut
    ui_state.theme = UI_THEME_LIGHT;
    
    // Initialiser l'affichage
    display_init();
    
    // Initialiser le tactile
    touch_manager_init(NULL);
    
    // Effacer l'écran
    display_clear(ui_state.theme.background);
    display_swap_buffers();
    
    ui_state.initialized = true;
    
    UI_DEBUG("Framework UI initialisé\n");
    return true;
}

void ui_core_deinit(void)
{
    ui_clear_screen_stack();
    ui_dismiss_all_modals();
    ui_state.initialized = false;
}

bool ui_core_is_ready(void)
{
    return ui_state.initialized;
}

// ============================================================
// SECTION 2 : NAVIGATION
// ============================================================

bool ui_push_screen(UIScreen* screen)
{
    if (!ui_state.initialized || screen == NULL) return false;
    if (ui_state.screenStackTop >= UI_MAX_SCREEN_STACK) return false;
    
    UI_DEBUG("Push screen: %s\n", screen->name);
    
    // Cacher l'écran actif
    if (ui_state.activeScreen && ui_state.activeScreen->onHide)
    {
        ui_state.activeScreen->onHide(ui_state.activeScreen);
    }
    
    // Ajouter à la pile
    ui_state.screenStack[ui_state.screenStackTop++] = screen;
    ui_state.activeScreen = screen;
    
    // Initialiser le nouvel écran
    if (!screen->initialized)
    {
        if (screen->onInit) screen->onInit(screen);
        screen->initialized = true;
    }
    
    screen->visible = true;
    screen->needsRedraw = true;
    
    if (screen->onShow) screen->onShow(screen);
    
    ui_request_redraw();
    
    return true;
}

UIScreen* ui_pop_screen(void)
{
    if (!ui_state.initialized) return NULL;
    if (ui_state.screenStackTop == 0) return NULL;
    
    UIScreen* screen = ui_state.screenStack[--ui_state.screenStackTop];
    
    UI_DEBUG("Pop screen: %s\n", screen->name);
    
    screen->visible = false;
    
    if (screen->onHide) screen->onHide(screen);
    
    // Activer l'écran précédent
    if (ui_state.screenStackTop > 0)
    {
        ui_state.activeScreen = ui_state.screenStack[ui_state.screenStackTop - 1];
        ui_state.activeScreen->visible = true;
        
        if (ui_state.activeScreen->onShow) 
            ui_state.activeScreen->onShow(ui_state.activeScreen);
    }
    else
    {
        ui_state.activeScreen = NULL;
    }
    
    ui_request_redraw();
    
    return screen;
}

UIScreen* ui_get_active_screen(void)
{
    return ui_state.activeScreen;
}

uint8_t ui_get_screen_count(void)
{
    return ui_state.screenStackTop;
}

bool ui_replace_screen(UIScreen* screen)
{
    if (!ui_state.initialized || screen == NULL) return false;
    
    // Retirer l'écran actuel
    if (ui_state.screenStackTop > 0)
    {
        UIScreen* old = ui_state.screenStack[--ui_state.screenStackTop];
        if (old->onHide) old->onHide(old);
    }
    
    // Pousser le nouveau
    return ui_push_screen(screen);
}

void ui_clear_screen_stack(void)
{
    while (ui_state.screenStackTop > 0)
    {
        UIScreen* screen = ui_state.screenStack[--ui_state.screenStackTop];
        if (screen->onHide) screen->onHide(screen);
        if (screen->onDestroy) screen->onDestroy(screen);
    }
    ui_state.activeScreen = NULL;
}

// ============================================================
// SECTION 3 : MODALES
// ============================================================

bool ui_show_modal(UIModal* modal)
{
    if (!ui_state.initialized || modal == NULL) return false;
    if (ui_state.modalCount >= UI_MAX_MODALS) return false;
    
    UI_DEBUG("Show modal: %s\n", modal->title);
    
    memcpy(&ui_state.modalStack[ui_state.modalCount], modal, sizeof(UIModal));
    ui_state.modalCount++;
    
    ui_request_redraw();
    
    return true;
}

void ui_dismiss_modal(UIResult result)
{
    if (ui_state.modalCount == 0) return;
    
    UIModal* modal = &ui_state.modalStack[--ui_state.modalCount];
    
    if (modal->onDismiss) modal->onDismiss(result);
    
    ui_request_redraw();
}

void ui_dismiss_all_modals(void)
{
    while (ui_state.modalCount > 0)
    {
        UIModal* modal = &ui_state.modalStack[--ui_state.modalCount];
        if (modal->onDismiss) modal->onDismiss(UI_RESULT_CANCEL);
    }
}

bool ui_has_modal(void)
{
    return (ui_state.modalCount > 0);
}

// ============================================================
// SECTION 4 : RENDU
// ============================================================

void ui_render(void)
{
    if (!ui_state.initialized) return;
    
    uint32_t now = HAL_GetTick();
    
    // Limiter le taux de rafraîchissement
    if ((now - ui_state.lastRenderTime) < UI_REFRESH_INTERVAL_MS)
    {
        return;
    }
    
    // Effacer l'écran
    display_clear(ui_state.theme.background);
    
    // Dessiner l'écran actif
    if (ui_state.activeScreen && ui_state.activeScreen->visible)
    {
        if (ui_state.activeScreen->onDraw)
        {
            ui_state.activeScreen->onDraw(ui_state.activeScreen);
        }
        
        // Dessiner tous les widgets de l'écran
        for (uint8_t i = 0; i < ui_state.activeScreen->widgetCount; i++)
        {
            UIWidget* widget = ui_state.activeScreen->widgets[i];
            
            if (widget && widget->visible)
            {
                if (widget->draw) widget->draw(widget);
                widget->needsRedraw = false;
            }
        }
        
        ui_state.activeScreen->needsRedraw = false;
    }
    
    // Dessiner les modales
    for (uint8_t i = 0; i < ui_state.modalCount; i++)
    {
        draw_modal(&ui_state.modalStack[i]);
    }
    
    // Échanger les buffers
    display_swap_buffers();
    
    ui_state.needsRedraw = false;
    ui_state.lastRenderTime = now;
    ui_state.frameCount++;
}

void ui_request_redraw(void)
{
    ui_state.needsRedraw = true;
    
    if (ui_state.activeScreen)
    {
        ui_state.activeScreen->needsRedraw = true;
    }
}

void ui_process_animations(void)
{
    if (!ui_state.initialized) return;
    
    if (ui_state.animating)
    {
        uint32_t elapsed = HAL_GetTick() - ui_state.animationStartTime;
        
        if (elapsed >= ui_state.animationDuration)
        {
            ui_state.animating = false;
        }
        
        ui_request_redraw();
    }
}

// ============================================================
// SECTION 5 : DISTRIBUTION DES ÉVÉNEMENTS
// ============================================================

void ui_process_events(void)
{
    if (!ui_state.initialized) return;
    
    // Scanner le tactile
    touch_manager_process();
    
    if (touch_manager_is_touched())
    {
        uint16_t x, y;
        if (touch_manager_get_position(&x, &y))
        {
            TouchEvent event = touch_manager_get_event();
            ui_handle_touch(x, y, event);
        }
    }
}

void ui_handle_touch(uint16_t x, uint16_t y, TouchEvent event)
{
    if (!ui_state.initialized) return;
    
    // Vérifier d'abord les modales (priorité)
    if (ui_state.modalCount > 0)
    {
        UIModal* modal = &ui_state.modalStack[ui_state.modalCount - 1];
        
        if (event == TOUCH_EVENT_PRESS)
        {
            if (modal->dismissOnOutside)
            {
                // Vérifier si le toucher est en dehors de la modale
                ui_dismiss_modal(UI_RESULT_CANCEL);
                return;
            }
        }
        
        // Si la modale a un écran, lui transmettre
        if (modal->screen)
        {
            if (modal->screen->onTouch)
                modal->screen->onTouch(modal->screen, x, y, event);
        }
        return;
    }
    
    // Transmettre à l'écran actif
    if (ui_state.activeScreen && ui_state.activeScreen->onTouch)
    {
        ui_state.activeScreen->onTouch(ui_state.activeScreen, x, y, event);
        
        // Chercher le widget touché
        for (int8_t i = ui_state.activeScreen->widgetCount - 1; i >= 0; i--)
        {
            UIWidget* widget = ui_state.activeScreen->widgets[i];
            
            if (widget && widget->visible && widget->enabled)
            {
                if (is_point_in_rect(x, y, &widget->rect))
                {
                    // Donner le focus
                    if (widget->canFocus)
                    {
                        ui_widget_set_focus(widget);
                    }
                    
                    if (widget->onTouch)
                    {
                        widget->onTouch(widget, x - widget->rect.x, 
                                       y - widget->rect.y, event);
                    }
                    
                    if (event == TOUCH_EVENT_RELEASE && widget->onClick)
                    {
                        widget->onClick(widget);
                    }
                    
                    break;  // Un seul widget reçoit l'événement
                }
            }
        }
    }
}

void ui_handle_key(KeyCode key, KeyEvent event)
{
    if (!ui_state.initialized) return;
    
    // Vérifier les modales
    if (ui_state.modalCount > 0)
    {
        UIModal* modal = &ui_state.modalStack[ui_state.modalCount - 1];
        
        if (key == KEY_BACK && modal->dismissOnBack)
        {
            ui_dismiss_modal(UI_RESULT_CANCEL);
            return;
        }
        
        if (modal->screen && modal->screen->onKey)
        {
            modal->screen->onKey(modal->screen, key, event);
        }
        return;
    }
    
    // Touche BACK : revenir à l'écran précédent
    if (key == KEY_BACK && event == KEY_EVENT_PRESS)
    {
        if (ui_state.screenStackTop > 1)
        {
            ui_pop_screen();
            return;
        }
    }
    
    // Transmettre au widget avec le focus
    if (ui_state.focusedWidget && ui_state.focusedWidget->onKey)
    {
        ui_state.focusedWidget->onKey(ui_state.focusedWidget, key, event);
        return;
    }
    
    // Transmettre à l'écran actif
    if (ui_state.activeScreen && ui_state.activeScreen->onKey)
    {
        ui_state.activeScreen->onKey(ui_state.activeScreen, key, event);
    }
}

// ============================================================
// SECTION 6 : THÈME
// ============================================================

void ui_set_theme(const UITheme* theme)
{
    if (theme)
    {
        memcpy(&ui_state.theme, theme, sizeof(UITheme));
        ui_apply_theme();
    }
}

const UITheme* ui_get_theme(void)
{
    return &ui_state.theme;
}

void ui_apply_theme(void)
{
    ui_request_redraw();
}

// ============================================================
// SECTION 7 : GESTION DES WIDGETS
// ============================================================

UIWidget* ui_widget_create(UIWidgetType type, const char* name)
{
    UIWidget* widget = (UIWidget*)calloc(1, sizeof(UIWidget));
    if (widget == NULL) return NULL;
    
    widget->id = ++ui_state.widgetIdCounter;
    widget->type = type;
    
    if (name) strncpy(widget->name, name, 31);
    
    widget->visible = true;
    widget->enabled = true;
    widget->state = WIDGET_STATE_NORMAL;
    widget->canFocus = true;
    
    return widget;
}

bool ui_widget_add_to_screen(UIScreen* screen, UIWidget* widget)
{
    if (screen == NULL || widget == NULL) return false;
    if (screen->widgetCount >= UI_MAX_WIDGETS_PER_SCREEN) return false;
    
    screen->widgets[screen->widgetCount++] = widget;
    widget->parent = screen;
    
    return true;
}

bool ui_widget_remove_from_screen(UIScreen* screen, UIWidget* widget)
{
    if (screen == NULL || widget == NULL) return false;
    
    for (uint8_t i = 0; i < screen->widgetCount; i++)
    {
        if (screen->widgets[i] == widget)
        {
            if (i < screen->widgetCount - 1)
            {
                memmove(&screen->widgets[i], &screen->widgets[i + 1],
                        (screen->widgetCount - i - 1) * sizeof(UIWidget*));
            }
            screen->widgetCount--;
            widget->parent = NULL;
            return true;
        }
    }
    return false;
}

UIWidget* ui_find_widget_by_id(UIScreen* screen, uint32_t id)
{
    if (screen == NULL) return NULL;
    
    for (uint8_t i = 0; i < screen->widgetCount; i++)
    {
        if (screen->widgets[i] && screen->widgets[i]->id == id)
            return screen->widgets[i];
    }
    return NULL;
}

UIWidget* ui_find_widget_by_name(UIScreen* screen, const char* name)
{
    if (screen == NULL || name == NULL) return NULL;
    
    for (uint8_t i = 0; i < screen->widgetCount; i++)
    {
        if (screen->widgets[i] && strcmp(screen->widgets[i]->name, name) == 0)
            return screen->widgets[i];
    }
    return NULL;
}

void ui_widget_set_focus(UIWidget* widget)
{
    // Retirer le focus de l'ancien widget
    if (ui_state.focusedWidget && ui_state.focusedWidget != widget)
    {
        ui_state.focusedWidget->hasFocus = false;
        ui_state.focusedWidget->state = WIDGET_STATE_NORMAL;
        
        if (ui_state.focusedWidget->onFocus)
            ui_state.focusedWidget->onFocus(ui_state.focusedWidget, false);
    }
    
    ui_state.focusedWidget = widget;
    
    if (widget)
    {
        widget->hasFocus = true;
        widget->state = WIDGET_STATE_FOCUSED;
        
        if (widget->onFocus)
            widget->onFocus(widget, true);
    }
}

UIWidget* ui_get_focused_widget(void)
{
    return ui_state.focusedWidget;
}

void ui_widget_destroy(UIWidget* widget)
{
    if (widget == NULL) return;
    
    if (widget->parent)
    {
        ui_widget_remove_from_screen(widget->parent, widget);
    }
    
    if (ui_state.focusedWidget == widget)
    {
        ui_state.focusedWidget = NULL;
    }
    
    free(widget);
}

// ============================================================
// SECTION 8 : FONCTIONS INTERNES
// ============================================================

/**
 * @brief Vérifie si un point est dans un rectangle
 */
static bool is_point_in_rect(uint16_t x, uint16_t y, const UIRect* rect)
{
    if (rect == NULL) return false;
    return (x >= rect->x && x < rect->x + rect->width &&
            y >= rect->y && y < rect->y + rect->height);
}

/**
 * @brief Dessine une fenêtre modale
 */
static void draw_modal(const UIModal* modal)
{
    if (modal == NULL) return;
    
    // Fond semi-transparent
    display_fill_rect(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1, 
                      0x0000);  // Noir avec alpha (simulé)
    
    // Cadre de la modale
    uint16_t modalWidth = 280;
    uint16_t modalHeight = 200;
    uint16_t modalX = (DISPLAY_WIDTH - modalWidth) / 2;
    uint16_t modalY = (DISPLAY_HEIGHT - modalHeight) / 2;
    
    display_fill_round_rect(modalX, modalY, modalX + modalWidth, modalY + modalHeight,
                             10, ui_state.theme.surface);
    display_draw_round_rect(modalX, modalY, modalX + modalWidth, modalY + modalHeight,
                             10, ui_state.theme.border);
    
    // Titre
    if (strlen(modal->title) > 0)
    {
        display_set_text_color(ui_state.theme.textPrimary);
        display_draw_text_center(modalY + 20, modal->title, ui_state.theme.textPrimary, 2);
    }
    
    // Message
    if (strlen(modal->message) > 0)
    {
        display_set_text_color(ui_state.theme.textSecondary);
        display_draw_text(modalX + 20, modalY + 60, modal->message, 
                         ui_state.theme.textSecondary, 1);
    }
    
    // Si la modale a un écran, le dessiner
    if (modal->screen && modal->screen->onDraw)
    {
        modal->screen->onDraw(modal->screen);
    }
}

// ============================================================
// SECTION 9 : DÉBOGAGE
// ============================================================

void ui_core_print_state(void)
{
    printf("\n═══ ÉTAT FRAMEWORK UI ═══\n");
    printf("Initialisé     : %s\n", ui_state.initialized ? "Oui" : "Non");
    printf("Écrans dans pile: %d\n", ui_state.screenStackTop);
    printf("Modales        : %d\n", ui_state.modalCount);
    printf("Widget focus   : %s\n", ui_state.focusedWidget ? ui_state.focusedWidget->name : "Aucun");
    printf("Frames         : %lu\n", (unsigned long)ui_state.frameCount);
    printf("Animations     : %s\n", ui_state.animating ? "En cours" : "Non");
    printf("══════════════════════════\n\n");
}

void ui_core_print_screen_stack(void)
{
    printf("\n═══ PILE D'ÉCRANS (%d) ═══\n", ui_state.screenStackTop);
    
    for (uint8_t i = 0; i < ui_state.screenStackTop; i++)
    {
        UIScreen* screen = ui_state.screenStack[i];
        printf("[%d] %s %s %s\n", i, screen->name,
               screen == ui_state.activeScreen ? "◄ ACTIF" : "",
               screen->visible ? "" : "(caché)");
    }
    printf("══════════════════════════\n\n");
}

void ui_core_print_widgets(UIScreen* screen)
{
    if (screen == NULL) return;
    
    printf("\n═══ WIDGETS DE %s (%d) ═══\n", screen->name, screen->widgetCount);
    
    for (uint8_t i = 0; i < screen->widgetCount; i++)
    {
        UIWidget* w = screen->widgets[i];
        printf("[%d] %-20s type=%d rect=(%d,%d,%d,%d) %s %s\n",
               i, w->name, w->type,
               w->rect.x, w->rect.y, w->rect.width, w->rect.height,
               w->visible ? "" : "(caché)",
               w->enabled ? "" : "(désactivé)");
    }
    printf("══════════════════════════════\n\n");
}

bool ui_core_self_test(void)
{
    UI_DEBUG("Auto-test...\n");
    
    if (!ui_state.initialized)
    {
        UI_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : créer un widget
    UIWidget* widget = ui_widget_create(WIDGET_TYPE_BUTTON, "testWidget");
    if (widget == NULL || widget->id == 0)
    {
        UI_DEBUG("Échec : création widget\n");
        return false;
    }
    
    ui_widget_destroy(widget);
    
    UI_DEBUG("Auto-test OK\n");
    return true;
}