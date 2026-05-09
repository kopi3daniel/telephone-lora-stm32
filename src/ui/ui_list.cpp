/**
 * @file ui_list.cpp
 * @brief Implémentation du widget Liste (UIList)
 * 
 * Ce fichier est optionnel. L'implémentation de UIList
 * se trouve déjà dans ui_widgets.cpp (section 4).
 * 
 * Fonctionnalités :
 * - Rendu graphique avec styles (plain, grouped, card, compact)
 * - Défilement vertical avec barre de défilement
 * - Sélection simple et multiple
 * - Recherche et tri
 * - Callbacks de sélection et défilement
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_list.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// FONCTIONS DE DESSIN
// ============================================================

/**
 * @brief Dessine la liste avec ses éléments visibles
 */
static void list_draw(UIWidget* widget)
{
    UIList* list = (UIList*)widget;
    if (list == NULL || list->itemCount == 0) return;
    
    ListAppearance* app = &list->appearance;
    UIRect* r = &widget->rect;
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    // --- Fond de la liste ---
    display_fill_rect(r->x, r->y, r->x + r->width - 1, r->y + r->height - 1, app->bgColor);
    
    // --- Calculer les éléments visibles ---
    uint16_t itemHeight = app->itemHeight;
    uint16_t visibleItems = (r->height / itemHeight);
    if (visibleItems > list->itemCount) visibleItems = list->itemCount;
    list->visibleItems = visibleItems;
    
    // --- Ajuster le défilement ---
    int16_t maxOffset = list->itemCount - visibleItems;
    if (maxOffset < 0) maxOffset = 0;
    if (list->scrollOffset < 0) list->scrollOffset = 0;
    if (list->scrollOffset > maxOffset) list->scrollOffset = maxOffset;
    
    // --- Dessiner les éléments visibles ---
    for (uint16_t i = 0; i < visibleItems; i++)
    {
        uint16_t itemIndex = list->scrollOffset + i;
        if (itemIndex >= list->itemCount) break;
        
        UIListItem* item = &list->items[itemIndex];
        uint16_t itemY = r->y + i * itemHeight;
        
        // Déterminer les couleurs de l'élément
        uint16_t itemBg;
        if (!item->enabled)
        {
            itemBg = colors->disabled;
        }
        else if (item->selected || itemIndex == list->selectedIndex)
        {
            itemBg = app->itemSelectedColor;
        }
        else
        {
            itemBg = app->itemBgColor;
        }
        
        // --- Fond de l'élément ---
        if (app->cornerRadius > 0 && (i == 0 || i == visibleItems - 1))
        {
            uint8_t radius = (i == 0) ? app->cornerRadius : 0;
            uint8_t radiusBottom = (i == visibleItems - 1) ? app->cornerRadius : 0;
            
            display_fill_round_rect(r->x, itemY, r->x + r->width - 1, 
                                     itemY + itemHeight - 1, app->cornerRadius, itemBg);
        }
        else
        {
            display_fill_rect(r->x, itemY, r->x + r->width - 1, 
                             itemY + itemHeight - 1, itemBg);
        }
        
        // --- Séparateur ---
        if (i > 0 && app->dividerColor != itemBg)
        {
            display_fill_rect(r->x + app->itemPadding.left, itemY,
                             r->x + r->width - app->itemPadding.right - 1, itemY,
                             app->dividerColor);
        }
        
        // --- Icône (si présente) ---
        uint16_t textX = r->x + app->itemPadding.left;
        
        if (item->iconIndex > 0)
        {
            // Icône à dessiner (simplifié)
            uint16_t iconSize = itemHeight - 8;
            uint16_t iconY = itemY + (itemHeight - iconSize) / 2;
            
            // Fond de l'icône (cercle coloré)
            display_fill_circle(textX + iconSize / 2, iconY + iconSize / 2,
                               iconSize / 2, colors->primary);
            
            textX += iconSize + 8;
        }
        
        // --- Texte principal ---
        display_set_font(app->font);
        uint16_t textColor = item->enabled ? app->textColor : colors->textDisabled;
        
        uint16_t textY = itemY + app->itemPadding.top;
        display_draw_text(textX, textY, item->text, textColor, app->fontSize);
        
        // --- Sous-texte (si présent) ---
        if (strlen(item->subtext) > 0)
        {
            uint16_t subtextY = textY + display_text_height(app->fontSize) + 4;
            display_set_text_color(colors->textSecondary);
            display_draw_text(textX, subtextY, item->subtext, 
                             colors->textSecondary, 1);
        }
        
        // --- Cocher si sélectionné (mode multi-sélection) ---
        if (list->multiSelect && item->selected)
        {
            uint16_t checkX = r->x + r->width - app->itemPadding.right - 20;
            uint16_t checkY = itemY + (itemHeight - 16) / 2;
            
            display_fill_round_rect(checkX, checkY, checkX + 16, checkY + 16, 3, colors->primary);
            display_draw_text(checkX + 3, checkY + 2, "✓", colors->onPrimary, 1);
        }
        
        // --- Callback de dessin personnalisé ---
        if (list->onItemDraw)
        {
            UIRect itemRect = {r->x, itemY, r->width, itemHeight};
            list->onItemDraw(list, itemIndex, &itemRect);
        }
    }
    
    // --- Barre de défilement ---
    if (list->showScrollbar && list->itemCount > visibleItems)
    {
        uint16_t scrollbarX = r->x + r->width - list->scrollbarWidth - 2;
        uint16_t scrollbarH = (uint16_t)((uint32_t)r->height * visibleItems / list->itemCount);
        uint16_t scrollbarY = r->y + (uint16_t)((uint32_t)r->height * list->scrollOffset / list->itemCount);
        
        if (scrollbarH < 20) scrollbarH = 20;
        if (scrollbarY + scrollbarH > r->y + r->height) 
            scrollbarY = r->y + r->height - scrollbarH;
        
        display_fill_round_rect(scrollbarX, scrollbarY, 
                                 scrollbarX + list->scrollbarWidth - 1, 
                                 scrollbarY + scrollbarH - 1,
                                 list->scrollbarWidth / 2, colors->textDisabled);
    }
}

// ============================================================
// FONCTIONS TACTILES
// ============================================================

/**
 * @brief Gestion des événements tactiles
 */
static void list_touch(UIWidget* widget, uint16_t x, uint16_t y, TouchEvent event)
{
    UIList* list = (UIList*)widget;
    if (list == NULL || event != TOUCH_EVENT_PRESS) return;
    
    uint16_t itemHeight = list->appearance.itemHeight;
    
    // Calculer l'index de l'élément touché
    int16_t touchedIndex = list->scrollOffset + (y / itemHeight);
    
    if (touchedIndex >= 0 && touchedIndex < (int16_t)list->itemCount)
    {
        UIListItem* item = &list->items[touchedIndex];
        
        if (!item->enabled) return;
        
        if (list->multiSelect)
        {
            // Multi-sélection : basculer
            ui_list_toggle_selection(list, touchedIndex);
        }
        else
        {
            // Sélection simple
            ui_list_select(list, touchedIndex);
        }
    }
}

/**
 * @brief Gestion des touches clavier
 */
static void list_key(UIWidget* widget, KeyCode key, KeyEvent event)
{
    UIList* list = (UIList*)widget;
    if (list == NULL || event != KEY_EVENT_PRESS) return;
    
    switch (key)
    {
        case KEY_UP:
            if (list->wrapScrolling && list->selectedIndex <= 0)
            {
                // Défilement circulaire : aller au dernier
                ui_list_select(list, list->itemCount - 1);
                ui_list_scroll_to_bottom(list);
            }
            else if (list->selectedIndex > 0)
            {
                ui_list_select(list, list->selectedIndex - 1);
                
                // Ajuster le défilement si nécessaire
                if (list->selectedIndex < list->scrollOffset)
                {
                    ui_list_scroll_to(list, list->selectedIndex);
                }
            }
            break;
            
        case KEY_DOWN:
            if (list->wrapScrolling && list->selectedIndex >= (int16_t)(list->itemCount - 1))
            {
                // Défilement circulaire : aller au premier
                ui_list_select(list, 0);
                ui_list_scroll_to_top(list);
            }
            else if (list->selectedIndex < (int16_t)(list->itemCount - 1))
            {
                ui_list_select(list, list->selectedIndex + 1);
                
                // Ajuster le défilement
                if (list->selectedIndex >= list->scrollOffset + (int16_t)list->visibleItems)
                {
                    ui_list_scroll_to(list, list->selectedIndex);
                }
            }
            break;
            
        case KEY_OK:
            if (list->onSelect && list->selectedIndex >= 0)
            {
                list->onSelect(list, list->selectedIndex);
            }
            break;
            
        default:
            break;
    }
}

// ============================================================
// CRÉATION
// ============================================================

UIList* ui_list_create(const char* name, UIRect rect, uint16_t maxItems)
{
    UIList* list = (UIList*)calloc(1, sizeof(UIList));
    if (list == NULL) return NULL;
    
    // --- Initialiser le widget de base ---
    list->base.type = WIDGET_TYPE_LIST;
    if (name) strncpy(list->base.name, name, 31);
    list->base.rect = rect;
    list->base.visible = true;
    list->base.enabled = true;
    list->base.canFocus = true;
    list->base.state = WIDGET_STATE_NORMAL;
    
    // --- Assigner les fonctions virtuelles ---
    list->base.draw = list_draw;
    list->base.onTouch = list_touch;
    list->base.onKey = list_key;
    list->base.onUpdate = NULL;
    
    // --- Allocation des éléments ---
    list->maxItems = maxItems;
    list->items = (UIListItem*)calloc(maxItems, sizeof(UIListItem));
    list->itemCount = 0;
    list->selectedIndex = -1;
    
    // --- Défilement ---
    list->scrollOffset = 0;
    list->showScrollbar = true;
    list->scrollbarWidth = 4;
    list->wrapScrolling = false;
    
    // --- Sélection ---
    list->multiSelect = false;
    list->selectedIndices = NULL;
    list->selectedCount = 0;
    
    // --- Style par défaut ---
    ui_list_set_style(list, LIST_STYLE_PLAIN);
    
    return list;
}

// ============================================================
// GESTION DES ÉLÉMENTS
// ============================================================

bool ui_list_add_item(UIList* list, const char* text, const char* subtext, void* userData)
{
    return ui_list_insert_item(list, list->itemCount, text, subtext, userData);
}

bool ui_list_insert_item(UIList* list, uint16_t index, const char* text, const char* subtext, void* userData)
{
    if (list == NULL || list->itemCount >= list->maxItems) return false;
    if (index > list->itemCount) index = list->itemCount;
    
    // Décaler les éléments suivants
    if (index < list->itemCount)
    {
        memmove(&list->items[index + 1], &list->items[index],
                (list->itemCount - index) * sizeof(UIListItem));
    }
    
    // Insérer le nouvel élément
    UIListItem* item = &list->items[index];
    memset(item, 0, sizeof(UIListItem));
    
    if (text) strncpy(item->text, text, 127);
    if (subtext) strncpy(item->subtext, subtext, 127);
    item->userData = userData;
    item->enabled = true;
    
    list->itemCount++;
    list->base.needsRedraw = true;
    
    return true;
}

bool ui_list_remove_item(UIList* list, uint16_t index)
{
    if (list == NULL || index >= list->itemCount) return false;
    
    // Mettre à jour la sélection
    if (list->selectedIndex == (int16_t)index)
    {
        list->selectedIndex = -1;
    }
    else if (list->selectedIndex > (int16_t)index)
    {
        list->selectedIndex--;
    }
    
    // Décaler les éléments
    if (index < list->itemCount - 1)
    {
        memmove(&list->items[index], &list->items[index + 1],
                (list->itemCount - index - 1) * sizeof(UIListItem));
    }
    
    list->itemCount--;
    list->base.needsRedraw = true;
    
    return true;
}

bool ui_list_update_item(UIList* list, uint16_t index, const char* text, const char* subtext)
{
    if (list == NULL || index >= list->itemCount) return false;
    
    UIListItem* item = &list->items[index];
    if (text) strncpy(item->text, text, 127);
    if (subtext) strncpy(item->subtext, subtext, 127);
    
    list->base.needsRedraw = true;
    return true;
}

void ui_list_clear(UIList* list)
{
    if (list == NULL) return;
    list->itemCount = 0;
    list->selectedIndex = -1;
    list->scrollOffset = 0;
    list->selectedCount = 0;
    list->base.needsRedraw = true;
}

UIListItem* ui_list_get_item(UIList* list, uint16_t index)
{
    if (list == NULL || index >= list->itemCount) return NULL;
    return &list->items[index];
}

uint16_t ui_list_get_item_count(UIList* list)
{
    return (list != NULL) ? list->itemCount : 0;
}

// ============================================================
// SÉLECTION
// ============================================================

void ui_list_select(UIList* list, int16_t index)
{
    if (list == NULL) return;
    
    // Désélectionner l'ancien
    if (list->selectedIndex >= 0 && list->selectedIndex < (int16_t)list->itemCount)
    {
        list->items[list->selectedIndex].selected = false;
    }
    
    list->selectedIndex = index;
    
    // Sélectionner le nouveau
    if (index >= 0 && index < (int16_t)list->itemCount)
    {
        list->items[index].selected = true;
    }
    
    list->base.needsRedraw = true;
    
    if (list->onSelect) list->onSelect(list, index);
}

int16_t ui_list_get_selected(UIList* list)
{
    return (list != NULL) ? list->selectedIndex : -1;
}

void ui_list_deselect_all(UIList* list)
{
    if (list == NULL) return;
    list->selectedIndex = -1;
    
    for (uint16_t i = 0; i < list->itemCount; i++)
    {
        list->items[i].selected = false;
    }
    
    list->selectedCount = 0;
    list->base.needsRedraw = true;
}

bool ui_list_is_selected(UIList* list, uint16_t index)
{
    if (list == NULL || index >= list->itemCount) return false;
    return list->items[index].selected;
}

// ============================================================
// SÉLECTION MULTIPLE
// ============================================================

void ui_list_set_multi_select(UIList* list, bool enable)
{
    if (list == NULL) return;
    list->multiSelect = enable;
    
    if (enable)
    {
        if (list->selectedIndices == NULL)
        {
            list->selectedIndices = (uint16_t*)calloc(list->maxItems, sizeof(uint16_t));
        }
    }
    else
    {
        if (list->selectedIndices)
        {
            free(list->selectedIndices);
            list->selectedIndices = NULL;
        }
        ui_list_deselect_all(list);
    }
}

bool ui_list_toggle_selection(UIList* list, uint16_t index)
{
    if (list == NULL || index >= list->itemCount || !list->multiSelect) return false;
    
    UIListItem* item = &list->items[index];
    item->selected = !item->selected;
    
    if (item->selected)
    {
        // Ajouter à la liste des sélectionnés
        list->selectedIndices[list->selectedCount++] = index;
    }
    else
    {
        // Retirer de la liste
        for (uint16_t i = 0; i < list->selectedCount; i++)
        {
            if (list->selectedIndices[i] == index)
            {
                memmove(&list->selectedIndices[i], &list->selectedIndices[i + 1],
                        (list->selectedCount - i - 1) * sizeof(uint16_t));
                list->selectedCount--;
                break;
            }
        }
    }
    
    list->base.needsRedraw = true;
    
    if (list->onMultiSelect)
    {
        list->onMultiSelect(list, list->selectedIndices, list->selectedCount);
    }
    
    return true;
}

uint16_t ui_list_get_selected_count(UIList* list)
{
    return (list != NULL) ? list->selectedCount : 0;
}

uint16_t* ui_list_get_selected_indices(UIList* list, uint16_t* count)
{
    if (list == NULL || count == NULL) return NULL;
    *count = list->selectedCount;
    return list->selectedIndices;
}

// ============================================================
// DÉFILEMENT
// ============================================================

void ui_list_scroll_up(UIList* list)
{
    if (list == NULL || list->scrollOffset <= 0) return;
    list->scrollOffset--;
    list->base.needsRedraw = true;
    
    if (list->onScroll) list->onScroll(list, list->scrollOffset);
}

void ui_list_scroll_down(UIList* list)
{
    if (list == NULL) return;
    int16_t maxOffset = list->itemCount - list->visibleItems;
    if (maxOffset < 0) maxOffset = 0;
    if (list->scrollOffset >= maxOffset) return;
    
    list->scrollOffset++;
    list->base.needsRedraw = true;
    
    if (list->onScroll) list->onScroll(list, list->scrollOffset);
}

void ui_list_scroll_to(UIList* list, int16_t index)
{
    if (list == NULL || index < 0 || index >= (int16_t)list->itemCount) return;
    
    // Calculer le nouvel offset pour que l'élément soit visible
    if (index < list->scrollOffset)
    {
        list->scrollOffset = index;
    }
    else if (index >= list->scrollOffset + (int16_t)list->visibleItems)
    {
        list->scrollOffset = index - list->visibleItems + 1;
    }
    
    // Limiter
    int16_t maxOffset = list->itemCount - list->visibleItems;
    if (maxOffset < 0) maxOffset = 0;
    if (list->scrollOffset < 0) list->scrollOffset = 0;
    if (list->scrollOffset > maxOffset) list->scrollOffset = maxOffset;
    
    list->base.needsRedraw = true;
}

void ui_list_scroll_to_top(UIList* list)
{
    if (list == NULL) return;
    list->scrollOffset = 0;
    list->base.needsRedraw = true;
}

void ui_list_scroll_to_bottom(UIList* list)
{
    if (list == NULL) return;
    int16_t maxOffset = list->itemCount - list->visibleItems;
    if (maxOffset < 0) maxOffset = 0;
    list->scrollOffset = maxOffset;
    list->base.needsRedraw = true;
}

void ui_list_page_up(UIList* list)
{
    if (list == NULL) return;
    list->scrollOffset -= list->visibleItems;
    if (list->scrollOffset < 0) list->scrollOffset = 0;
    list->base.needsRedraw = true;
}

void ui_list_page_down(UIList* list)
{
    if (list == NULL) return;
    int16_t maxOffset = list->itemCount - list->visibleItems;
    if (maxOffset < 0) maxOffset = 0;
    list->scrollOffset += list->visibleItems;
    if (list->scrollOffset > maxOffset) list->scrollOffset = maxOffset;
    list->base.needsRedraw = true;
}

int16_t ui_list_get_scroll_offset(UIList* list)
{
    return (list != NULL) ? list->scrollOffset : 0;
}

int16_t ui_list_get_first_visible(UIList* list)
{
    return (list != NULL) ? list->scrollOffset : 0;
}

int16_t ui_list_get_last_visible(UIList* list)
{
    if (list == NULL) return 0;
    int16_t last = list->scrollOffset + list->visibleItems - 1;
    if (last >= (int16_t)list->itemCount) last = list->itemCount - 1;
    return last;
}

// ============================================================
// STYLE
// ============================================================

void ui_list_set_style(UIList* list, ListStyle style)
{
    if (list == NULL) return;
    list->appearance = ui_style_get_list(style);
    list->base.needsRedraw = true;
}

void ui_list_set_item_height(UIList* list, uint8_t height)
{
    if (list == NULL) return;
    list->appearance.itemHeight = height;
    list->base.needsRedraw = true;
}

// ============================================================
// RECHERCHE
// ============================================================

int16_t ui_list_find_by_text(UIList* list, const char* text)
{
    if (list == NULL || text == NULL) return -1;
    
    for (uint16_t i = 0; i < list->itemCount; i++)
    {
        if (strstr(list->items[i].text, text) != NULL ||
            strstr(list->items[i].subtext, text) != NULL)
        {
            return i;
        }
    }
    return -1;
}

int16_t ui_list_find_by_tag(UIList* list, uint16_t tag)
{
    if (list == NULL) return -1;
    
    for (uint16_t i = 0; i < list->itemCount; i++)
    {
        if (list->items[i].tag == tag) return i;
    }
    return -1;
}

int16_t ui_list_find_by_user_data(UIList* list, void* userData)
{
    if (list == NULL || userData == NULL) return -1;
    
    for (uint16_t i = 0; i < list->itemCount; i++)
    {
        if (list->items[i].userData == userData) return i;
    }
    return -1;
}

// ============================================================
// TRI
// ============================================================

static int compare_text_asc(const void* a, const void* b)
{
    return strcmp(((UIListItem*)a)->text, ((UIListItem*)b)->text);
}

static int compare_text_desc(const void* a, const void* b)
{
    return strcmp(((UIListItem*)b)->text, ((UIListItem*)a)->text);
}

static int compare_tag_asc(const void* a, const void* b)
{
    return ((UIListItem*)a)->tag - ((UIListItem*)b)->tag;
}

static int compare_tag_desc(const void* a, const void* b)
{
    return ((UIListItem*)b)->tag - ((UIListItem*)a)->tag;
}

void ui_list_sort_by_text(UIList* list, bool ascending)
{
    if (list == NULL || list->itemCount < 2) return;
    qsort(list->items, list->itemCount, sizeof(UIListItem),
          ascending ? compare_text_asc : compare_text_desc);
    list->base.needsRedraw = true;
}

void ui_list_sort_by_tag(UIList* list, bool ascending)
{
    if (list == NULL || list->itemCount < 2) return;
    qsort(list->items, list->itemCount, sizeof(UIListItem),
          ascending ? compare_tag_asc : compare_tag_desc);
    list->base.needsRedraw = true;
}