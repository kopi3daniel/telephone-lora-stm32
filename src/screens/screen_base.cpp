/**
 * @file screen_base.cpp
 * @brief Implémentation de la classe de base des écrans
 * 
 * Fonctionnalités :
 * - Allocation et destruction des écrans
 * - Gestion des widgets (ajout, suppression, recherche)
 * - Navigation (résultat, transitions, retour)
 * - Timers internes
 * - Cycle de vie (enter, exit, update, draw)
 * - Gestion des événements tactiles et clavier
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "screen_base.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Compteur d'ID d'écrans */
static uint32_t screen_id_counter = 0;

// ============================================================
// SECTION 1 : CRÉATION ET DESTRUCTION
// ============================================================

ScreenBase* screen_create(const char* name)
{
    ScreenBase* screen = (ScreenBase*)calloc(1, sizeof(ScreenBase));
    if (screen == NULL)
    {
        SCREEN_DEBUG("Échec allocation mémoire pour l'écran '%s'\n", name ? name : "?");
        return NULL;
    }
    
    screen->screenId = ++screen_id_counter;
    
    if (name)
        strncpy(screen->name, name, 31);
    else
        snprintf(screen->name, 32, "Screen_%lu", (unsigned long)screen->screenId);
    
    // Valeurs par défaut
    screen->visible = false;
    screen->needsRedraw = true;
    screen->needsRefresh = false;
    screen->enterTransition = SCREEN_TRANSITION_NONE;
    screen->exitTransition = SCREEN_TRANSITION_NONE;
    screen->transitionDurationMs = 250;
    screen->result = SCREEN_RESULT_NONE;
    screen->returnData = NULL;
    
    SCREEN_DEBUG("Écran créé : %s (ID=%lu)\n", screen->name, (unsigned long)screen->screenId);
    
    // Appeler le callback de création si défini
    if (screen->onCreate)
        screen->onCreate(screen);
    
    return screen;
}

void screen_destroy(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    SCREEN_DEBUG("Destruction de l'écran : %s\n", screen->name);
    
    // Appeler le callback de destruction
    if (screen->onDestroy)
        screen->onDestroy(screen);
    
    // Libérer tous les widgets
    screen_remove_all_widgets(screen);
    
    // Libérer l'écran
    free(screen);
}

// ============================================================
// SECTION 2 : GESTION DES WIDGETS
// ============================================================

bool screen_add_widget(ScreenBase* screen, UIWidget* widget)
{
    if (screen == NULL || widget == NULL) return false;
    if (screen->widgetCount >= SCREEN_MAX_WIDGETS) return false;
    
    screen->widgets[screen->widgetCount++] = widget;
    widget->parent = (UIScreen*)screen;
    
    SCREEN_DEBUG("Widget '%s' ajouté à l'écran '%s' (%d widgets)\n",
                widget->name, screen->name, screen->widgetCount);
    
    return true;
}

bool screen_remove_widget(ScreenBase* screen, UIWidget* widget)
{
    if (screen == NULL || widget == NULL) return false;
    
    for (uint8_t i = 0; i < screen->widgetCount; i++)
    {
        if (screen->widgets[i] == widget)
        {
            // Décaler les widgets suivants
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

UIWidget* screen_find_widget(ScreenBase* screen, const char* name)
{
    if (screen == NULL || name == NULL) return NULL;
    
    for (uint8_t i = 0; i < screen->widgetCount; i++)
    {
        if (screen->widgets[i] && strcmp(screen->widgets[i]->name, name) == 0)
            return screen->widgets[i];
    }
    return NULL;
}

void screen_remove_all_widgets(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    for (uint8_t i = 0; i < screen->widgetCount; i++)
    {
        if (screen->widgets[i])
        {
            ui_widget_destroy(screen->widgets[i]);
        }
    }
    screen->widgetCount = 0;
}

// ============================================================
// SECTION 3 : NAVIGATION
// ============================================================

void screen_set_result(ScreenBase* screen, ScreenResult result, void* data)
{
    if (screen == NULL) return;
    
    screen->result = result;
    screen->returnData = data;
    
    SCREEN_DEBUG("Résultat défini pour '%s' : %d\n", screen->name, result);
}

ScreenResult screen_get_result(ScreenBase* screen)
{
    return screen ? screen->result : SCREEN_RESULT_NONE;
}

void screen_set_transition(ScreenBase* screen, ScreenTransition enter, 
                            ScreenTransition exit, uint32_t durationMs)
{
    if (screen == NULL) return;
    
    screen->enterTransition = enter;
    screen->exitTransition = exit;
    screen->transitionDurationMs = durationMs;
}

void screen_go_back(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    screen_set_result(screen, SCREEN_RESULT_BACK, NULL);
    ui_pop_screen();
}

// ============================================================
// SECTION 4 : TIMERS
// ============================================================

bool screen_add_timer(ScreenBase* screen, uint32_t intervalMs, void (*callback)(void))
{
    if (screen == NULL || callback == NULL) return false;
    if (screen->timerCount >= SCREEN_MAX_TIMERS) return false;
    
    screen->timers[screen->timerCount].intervalMs = intervalMs;
    screen->timers[screen->timerCount].lastTrigger = HAL_GetTick();
    screen->timers[screen->timerCount].callback = callback;
    screen->timers[screen->timerCount].active = true;
    screen->timerCount++;
    
    return true;
}

bool screen_remove_timer(ScreenBase* screen, uint8_t index)
{
    if (screen == NULL || index >= screen->timerCount) return false;
    
    screen->timers[index].active = false;
    
    if (index < screen->timerCount - 1)
    {
        memmove(&screen->timers[index], &screen->timers[index + 1],
                (screen->timerCount - index - 1) * sizeof(screen->timers[0]));
    }
    screen->timerCount--;
    
    return true;
}

void screen_process_timers(ScreenBase* screen)
{
    if (screen == NULL || !screen->visible) return;
    
    uint32_t now = HAL_GetTick();
    
    for (uint8_t i = 0; i < screen->timerCount; i++)
    {
        if (screen->timers[i].active && screen->timers[i].callback)
        {
            if ((now - screen->timers[i].lastTrigger) >= screen->timers[i].intervalMs)
            {
                screen->timers[i].lastTrigger = now;
                screen->timers[i].callback();
            }
        }
    }
}

// ============================================================
// SECTION 5 : CYCLE DE VIE
// ============================================================

void screen_enter(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    screen->visible = true;
    screen->needsRedraw = true;
    screen->needsRefresh = true;
    
    SCREEN_DEBUG("Entrée dans l'écran : %s\n", screen->name);
    
    // Initialiser les timers
    uint32_t now = HAL_GetTick();
    for (uint8_t i = 0; i < screen->timerCount; i++)
    {
        screen->timers[i].lastTrigger = now;
    }
    
    if (screen->onEnter)
        screen->onEnter(screen);
}

void screen_exit(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    screen->visible = false;
    
    SCREEN_DEBUG("Sortie de l'écran : %s\n", screen->name);
    
    if (screen->onExit)
        screen->onExit(screen);
}

void screen_update(ScreenBase* screen)
{
    if (screen == NULL || !screen->visible) return;
    
    // Traiter les timers
    screen_process_timers(screen);
    
    // Rafraîchir les données si nécessaire
    if (screen->needsRefresh)
    {
        screen->needsRefresh = false;
        if (screen->onRefresh)
            screen->onRefresh(screen);
    }
    
    // Callback de mise à jour
    if (screen->onUpdate)
        screen->onUpdate(screen);
    
    // Mettre à jour les widgets qui en ont besoin
    for (uint8_t i = 0; i < screen->widgetCount; i++)
    {
        UIWidget* widget = screen->widgets[i];
        if (widget && widget->visible && widget->onUpdate)
        {
            widget->onUpdate(widget);
        }
    }
}

void screen_draw(ScreenBase* screen)
{
    if (screen == NULL || !screen->visible) return;
    
    // Effacer le fond
    display_clear(ui_theme_get_active()->colors.background);
    
    // Dessiner l'écran
    if (screen->onDraw)
        screen->onDraw(screen);
    
    // Dessiner tous les widgets
    for (uint8_t i = 0; i < screen->widgetCount; i++)
    {
        UIWidget* widget = screen->widgets[i];
        if (widget && widget->visible && widget->draw && widget->needsRedraw)
        {
            widget->draw(widget);
            widget->needsRedraw = false;
        }
    }
    
    screen->needsRedraw = false;
}

void screen_handle_touch(ScreenBase* screen, uint16_t x, uint16_t y, TouchEvent event)
{
    if (screen == NULL || !screen->visible) return;
    
    // Transmettre à l'écran d'abord
    if (screen->onTouch)
        screen->onTouch(screen, x, y, event);
    
    // Chercher le widget touché (parcours inverse pour la superposition)
    for (int8_t i = screen->widgetCount - 1; i >= 0; i--)
    {
        UIWidget* widget = screen->widgets[i];
        
        if (widget && widget->visible && widget->enabled)
        {
            UIRect* r = &widget->rect;
            
            if (x >= r->x && x < r->x + r->width &&
                y >= r->y && y < r->y + r->height)
            {
                if (widget->onTouch)
                    widget->onTouch(widget, x - r->x, y - r->y, event);
                
                if (event == TOUCH_EVENT_PRESS && widget->canFocus)
                    ui_widget_set_focus(widget);
                
                break;  // Un seul widget reçoit l'événement
            }
        }
    }
}

void screen_handle_key(ScreenBase* screen, KeyCode key, KeyEvent event)
{
    if (screen == NULL || !screen->visible) return;
    
    // Touche retour
    if (key == KEY_BACK && event == KEY_EVENT_PRESS)
    {
        if (screen->onBackPressed)
        {
            if (screen->onBackPressed(screen))
                return;  // Événement consommé par l'écran
        }
        // Comportement par défaut : retour
        screen_go_back(screen);
        return;
    }
    
    // Callback clavier de l'écran
    if (screen->onKeyPress)
        screen->onKeyPress(screen, key, event);
    
    // Appui long
    if (event == KEY_EVENT_HOLD && screen->onKeyHold)
        screen->onKeyHold(screen, key, (HAL_GetTick() - 0));  // TODO: durée réelle
    
    // Transmettre au widget avec le focus
    UIWidget* focused = ui_get_focused_widget();
    if (focused && focused->onKey)
        focused->onKey(focused, key, event);
}

// ============================================================
// SECTION 6 : UTILITAIRES
// ============================================================

void screen_set_title(ScreenBase* screen, const char* title)
{
    if (screen == NULL) return;
    if (title) strncpy(screen->title, title, 63);
    else screen->title[0] = '\0';
}

const char* screen_get_title(ScreenBase* screen)
{
    return screen ? screen->title : "";
}

void screen_request_redraw(ScreenBase* screen)
{
    if (screen)
    {
        screen->needsRedraw = true;
        // Propager aux widgets
        for (uint8_t i = 0; i < screen->widgetCount; i++)
        {
            if (screen->widgets[i])
                screen->widgets[i]->needsRedraw = true;
        }
    }
}

void screen_request_refresh(ScreenBase* screen)
{
    if (screen)
        screen->needsRefresh = true;
}

// ============================================================
// SECTION 7 : DÉBOGAGE
// ============================================================

void screen_print_info(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    printf("\n═══ ÉCRAN : %s ═══\n", screen->name);
    printf("ID            : %lu\n", (unsigned long)screen->screenId);
    printf("Titre         : %s\n", screen->title[0] ? screen->title : "(aucun)");
    printf("Visible       : %s\n", screen->visible ? "Oui" : "Non");
    printf("Widgets       : %d\n", screen->widgetCount);
    printf("Timers        : %d\n", screen->timerCount);
    printf("Transition    : entrée=%d sortie=%d\n", screen->enterTransition, screen->exitTransition);
    printf("Résultat      : %d\n", screen->result);
    printf("══════════════════════════\n\n");
}

void screen_print_widgets(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    printf("\n═══ WIDGETS DE '%s' (%d) ═══\n", screen->name, screen->widgetCount);
    
    for (uint8_t i = 0; i < screen->widgetCount; i++)
    {
        UIWidget* w = screen->widgets[i];
        if (w)
        {
            printf("  [%d] %-20s type=%d rect=(%d,%d %dx%d) %s %s\n",
                   i, w->name, w->type,
                   w->rect.x, w->rect.y, w->rect.width, w->rect.height,
                   w->visible ? "" : "(caché)",
                   w->enabled ? "" : "(désactivé)");
        }
    }
    printf("══════════════════════════════\n\n");
}