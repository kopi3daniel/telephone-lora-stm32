/**
 * @file ui_list.h
 * @brief Widget Liste (UIList) - Définition et fonctions
 * 
 * Ce fichier est optionnel. La définition complète de UIList
 * se trouve déjà dans ui_widgets.h (section 4).
 * 
 * Fonctionnalités :
 * - Liste d'éléments avec défilement vertical
 * - Sélection simple ou multiple
 * - Styles visuels (plain, grouped, card, compact)
 * - Éléments avec texte principal, sous-texte et icône
 * - Callbacks de sélection et de défilement
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_LIST_H
#define UI_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "ui_core.h"
#include "ui_styles.h"

// ============================================================
// STRUCTURES
// ============================================================

/**
 * @brief Élément d'une liste
 */
typedef struct UIListItem {
    char text[128];                     // Texte principal
    char subtext[128];                  // Texte secondaire (optionnel)
    uint8_t iconIndex;                  // Index de l'icône (0 = pas d'icône)
    bool selected;                      // Élément sélectionné ?
    bool enabled;                       // Élément activé ?
    void* userData;                     // Données utilisateur (pointeur libre)
    uint16_t tag;                       // Tag numérique
} UIListItem;

/**
 * @brief Widget Liste
 * 
 * Hérite de UIWidget et ajoute :
 * - Tableau d'éléments (UIListItem)
 * - Défilement vertical
 * - Sélection simple ou multiple
 * - Styles visuels
 * - Callbacks
 */
typedef struct UIList {
    UIWidget base;                      // Widget de base (héritage)
    
    // --- Éléments ---
    UIListItem* items;                  // Tableau d'éléments
    uint16_t itemCount;                 // Nombre d'éléments actuels
    uint16_t maxItems;                  // Capacité maximale
    uint16_t visibleItems;             // Nombre d'éléments visibles
    
    // --- Sélection ---
    int16_t selectedIndex;              // Index sélectionné (-1 = aucun)
    bool multiSelect;                   // Sélection multiple ?
    uint16_t* selectedIndices;          // Indices sélectionnés (multi)
    uint16_t selectedCount;             // Nombre d'éléments sélectionnés
    
    // --- Défilement ---
    int16_t scrollOffset;               // Décalage de défilement (en éléments)
    bool showScrollbar;                 // Afficher la barre de défilement
    uint8_t scrollbarWidth;             // Largeur de la barre
    
    // --- Apparence ---
    ListAppearance appearance;          // Style visuel
    
    // --- Callbacks ---
    void (*onSelect)(struct UIList* list, int16_t index);          // Sélection simple
    void (*onMultiSelect)(struct UIList* list, uint16_t* indices, uint16_t count); // Multi-sélection
    void (*onScroll)(struct UIList* list, int16_t offset);         // Défilement
    void (*onItemDraw)(struct UIList* list, uint16_t index, UIRect* itemRect); // Dessin personnalisé
    
    // --- Comportement ---
    bool wrapScrolling;                 // Défilement circulaire ?
    uint16_t scrollAnimationMs;         // Durée animation défilement
} UIList;

// ============================================================
// FONCTIONS DE CRÉATION
// ============================================================

UIList* ui_list_create(const char* name, UIRect rect, uint16_t maxItems);

// ============================================================
// FONCTIONS DE GESTION DES ÉLÉMENTS
// ============================================================

bool ui_list_add_item(UIList* list, const char* text, const char* subtext, void* userData);
bool ui_list_insert_item(UIList* list, uint16_t index, const char* text, const char* subtext, void* userData);
bool ui_list_remove_item(UIList* list, uint16_t index);
bool ui_list_update_item(UIList* list, uint16_t index, const char* text, const char* subtext);
void ui_list_clear(UIList* list);
UIListItem* ui_list_get_item(UIList* list, uint16_t index);
uint16_t ui_list_get_item_count(UIList* list);

// ============================================================
// FONCTIONS DE SÉLECTION
// ============================================================

void ui_list_select(UIList* list, int16_t index);
int16_t ui_list_get_selected(UIList* list);
void ui_list_deselect_all(UIList* list);
bool ui_list_is_selected(UIList* list, uint16_t index);

// ============================================================
// FONCTIONS DE SÉLECTION MULTIPLE
// ============================================================

void ui_list_set_multi_select(UIList* list, bool enable);
bool ui_list_toggle_selection(UIList* list, uint16_t index);
uint16_t ui_list_get_selected_count(UIList* list);
uint16_t* ui_list_get_selected_indices(UIList* list, uint16_t* count);

// ============================================================
// FONCTIONS DE DÉFILEMENT
// ============================================================

void ui_list_scroll_up(UIList* list);
void ui_list_scroll_down(UIList* list);
void ui_list_scroll_to(UIList* list, int16_t index);
void ui_list_scroll_to_top(UIList* list);
void ui_list_scroll_to_bottom(UIList* list);
void ui_list_page_up(UIList* list);
void ui_list_page_down(UIList* list);
int16_t ui_list_get_scroll_offset(UIList* list);
int16_t ui_list_get_first_visible(UIList* list);
int16_t ui_list_get_last_visible(UIList* list);

// ============================================================
// FONCTIONS DE STYLE
// ============================================================

void ui_list_set_style(UIList* list, ListStyle style);
void ui_list_set_item_height(UIList* list, uint8_t height);

// ============================================================
// FONCTIONS DE RECHERCHE
// ============================================================

int16_t ui_list_find_by_text(UIList* list, const char* text);
int16_t ui_list_find_by_tag(UIList* list, uint16_t tag);
int16_t ui_list_find_by_user_data(UIList* list, void* userData);

// ============================================================
// FONCTIONS DE TRI
// ============================================================

void ui_list_sort_by_text(UIList* list, bool ascending);
void ui_list_sort_by_tag(UIList* list, bool ascending);

// ============================================================
// MACROS RAPIDES
// ============================================================

#define UI_LIST_CREATE(name, x, y, w, h, maxItems) \
    ui_list_create(name, UI_RECT(x, y, w, h), maxItems)

#define UI_LIST_ADD_ITEM(list, text, subtext, data) \
    ui_list_add_item(list, text, subtext, data)

#define UI_LIST_GET_SELECTED(list) \
    ui_list_get_selected(list)

// ============================================================
// COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_LIST_H