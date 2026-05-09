/**
 * @file ui_widgets.h
 * @brief Widgets concrets pour l'interface utilisateur
 * 
 * Ce fichier définit les widgets utilisables dans les écrans :
 * - Button (bouton cliquable)
 * - Label (texte)
 * - TextBox (zone de saisie)
 * - List (liste déroulante)
 * - Slider (curseur)
 * - Checkbox (case à cocher)
 * - RadioButton (bouton radio)
 * - ProgressBar (barre de progression)
 * - Image (icône/image)
 * - Panel (conteneur)
 * 
 * Chaque widget hérite de UIWidget et ajoute ses propriétés
 * spécifiques. Les widgets utilisent les styles de ui_styles.h.
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "ui_core.h"
#include "ui_theme.h"
#include "ui_styles.h"

// ============================================================
// SECTION 1 : BOUTON (UIButton)
// ============================================================

/**
 * @brief Widget bouton
 */
typedef struct {
    UIWidget base;                      // Widget de base
    char text[64];                      // Texte du bouton
    ButtonAppearance appearance;        // Apparence
    bool pressed;                       // État pressé
    void (*onClick)(struct UIButton* button);  // Callback clic
    void (*onLongPress)(struct UIButton* button); // Callback appui long
} UIButton;

UIButton* ui_button_create(const char* name, const char* text, UIRect rect);
void ui_button_set_style(UIButton* button, ButtonStyle style);
void ui_button_set_text(UIButton* button, const char* text);
void ui_button_set_enabled(UIButton* button, bool enabled);
void ui_button_set_callback(UIButton* button, void (*callback)(UIButton*));

// ============================================================
// SECTION 2 : LABEL (UILabel)
// ============================================================

/**
 * @brief Widget label (texte)
 */
typedef struct {
    UIWidget base;                      // Widget de base
    char text[256];                     // Texte
    LabelAppearance appearance;         // Apparence
    bool autoSize;                      // Ajuster la taille automatiquement
    uint16_t maxWidth;                  // Largeur max (pour retour à la ligne)
} UILabel;

UILabel* ui_label_create(const char* name, const char* text, UIRect rect);
void ui_label_set_style(UILabel* label, LabelStyle style);
void ui_label_set_text(UILabel* label, const char* text);
void ui_label_set_color(UILabel* label, uint16_t color);
void ui_label_set_alignment(UILabel* label, UIAlign align);

// ============================================================
// SECTION 3 : ZONE DE TEXTE (UITextBox)
// ============================================================

/**
 * @brief Widget zone de saisie
 */
typedef struct {
    UIWidget base;                      // Widget de base
    char text[256];                     // Texte saisi
    uint16_t maxLength;                 // Longueur maximale
    uint16_t cursorPos;                 // Position du curseur
    char placeholder[64];               // Texte indicatif
    TextBoxAppearance appearance;       // Apparence
    bool secure;                        // Mode mot de passe (***)
    bool editable;                      // Éditable ?
    bool cursorVisible;                 // Curseur visible
    uint32_t cursorBlinkTime;           // Temporisation clignotement
    void (*onTextChanged)(struct UITextBox* textbox);  // Callback
    void (*onSubmit)(struct UITextBox* textbox);        // Callback validation
} UITextBox;

UITextBox* ui_textbox_create(const char* name, UIRect rect);
void ui_textbox_set_style(UITextBox* textbox, TextBoxStyle style);
void ui_textbox_set_text(UITextBox* textbox, const char* text);
const char* ui_textbox_get_text(UITextBox* textbox);
void ui_textbox_set_placeholder(UITextBox* textbox, const char* placeholder);
void ui_textbox_set_secure(UITextBox* textbox, bool secure);
void ui_textbox_set_editable(UITextBox* textbox, bool editable);
void ui_textbox_insert_char(UITextBox* textbox, char c);
void ui_textbox_delete_char(UITextBox* textbox);
void ui_textbox_clear(UITextBox* textbox);

// ============================================================
// SECTION 4 : LISTE (UIList)
// ============================================================

/**
 * @brief Élément d'une liste
 */
typedef struct {
    char text[128];                     // Texte principal
    char subtext[128];                  // Texte secondaire
    uint8_t iconIndex;                  // Icône (0 = pas d'icône)
    void* userData;                     // Données utilisateur
} UIListItem;

/**
 * @brief Widget liste
 */
typedef struct {
    UIWidget base;                      // Widget de base
    UIListItem* items;                  // Éléments de la liste
    uint16_t itemCount;                 // Nombre d'éléments
    uint16_t maxItems;                  // Capacité maximale
    int16_t selectedIndex;              // Index sélectionné (-1 = aucun)
    int16_t scrollOffset;               // Décalage de défilement
    uint16_t visibleItems;              // Nombre d'éléments visibles
    ListAppearance appearance;          // Apparence
    void (*onSelect)(struct UIList* list, int16_t index);  // Callback sélection
} UIList;

UIList* ui_list_create(const char* name, UIRect rect, uint16_t maxItems);
bool ui_list_add_item(UIList* list, const char* text, const char* subtext, void* userData);
bool ui_list_remove_item(UIList* list, uint16_t index);
void ui_list_clear(UIList* list);
void ui_list_set_style(UIList* list, ListStyle style);
void ui_list_select(UIList* list, int16_t index);
int16_t ui_list_get_selected(UIList* list);
UIListItem* ui_list_get_item(UIList* list, uint16_t index);
void ui_list_scroll_up(UIList* list);
void ui_list_scroll_down(UIList* list);

// ============================================================
// SECTION 5 : SLIDER (UISlider)
// ============================================================

/**
 * @brief Widget curseur
 */
typedef struct {
    UIWidget base;                      // Widget de base
    uint8_t value;                      // Valeur (0-100)
    uint8_t minValue;                   // Valeur minimale
    uint8_t maxValue;                   // Valeur maximale
    uint8_t step;                       // Incrément
    SliderAppearance appearance;        // Apparence
    bool showValue;                     // Afficher la valeur
    void (*onValueChanged)(struct UISlider* slider, uint8_t value);
} UISlider;

UISlider* ui_slider_create(const char* name, UIRect rect, uint8_t min, uint8_t max, uint8_t value);
void ui_slider_set_style(UISlider* slider, SliderStyle style);
void ui_slider_set_value(UISlider* slider, uint8_t value);
uint8_t ui_slider_get_value(UISlider* slider);

// ============================================================
// SECTION 6 : CHECKBOX (UICheckbox)
// ============================================================

typedef struct {
    UIWidget base;
    char text[128];
    bool checked;
    void (*onToggle)(struct UICheckbox* checkbox, bool checked);
} UICheckbox;

UICheckbox* ui_checkbox_create(const char* name, const char* text, UIRect rect);
void ui_checkbox_set_checked(UICheckbox* checkbox, bool checked);
bool ui_checkbox_is_checked(UICheckbox* checkbox);
void ui_checkbox_toggle(UICheckbox* checkbox);

// ============================================================
// SECTION 7 : BARRE DE PROGRESSION (UIProgressBar)
// ============================================================

typedef struct {
    UIWidget base;
    uint8_t value;                      // Valeur (0-100)
    ProgressAppearance appearance;
    char label[32];
    bool showPercentage;
} UIProgressBar;

UIProgressBar* ui_progress_create(const char* name, UIRect rect);
void ui_progress_set_style(UIProgressBar* progress, ProgressStyle style);
void ui_progress_set_value(UIProgressBar* progress, uint8_t value);
uint8_t ui_progress_get_value(UIProgressBar* progress);

// ============================================================
// SECTION 8 : IMAGE/ICÔNE (UIImage)
// ============================================================

typedef struct {
    UIWidget base;
    const uint16_t* bitmap;             // Données bitmap (RGB565)
    uint16_t bitmapWidth;
    uint16_t bitmapHeight;
    bool scaleToFit;
} UIImage;

UIImage* ui_image_create(const char* name, UIRect rect, const uint16_t* bitmap, 
                          uint16_t width, uint16_t height);
void ui_image_set_bitmap(UIImage* image, const uint16_t* bitmap, uint16_t w, uint16_t h);

// ============================================================
// SECTION 9 : PANNEAU (UIPanel)
// ============================================================

typedef struct {
    UIWidget base;
    char title[64];
    UIWidget** children;
    uint8_t childCount;
    uint8_t maxChildren;
    PanelAppearance appearance;
    bool collapsible;
    bool collapsed;
} UIPanel;

UIPanel* ui_panel_create(const char* name, UIRect rect, uint8_t maxChildren);
bool ui_panel_add_child(UIPanel* panel, UIWidget* widget);
void ui_panel_set_style(UIPanel* panel, PanelStyle style);
void ui_panel_set_title(UIPanel* panel, const char* title);
void ui_panel_toggle_collapse(UIPanel* panel);

// ============================================================
// SECTION 10 : FONCTIONS DE DESSIN
// ============================================================

void ui_widgets_init(void);  // Initialiser le module de widgets (enregistrer les fonctions draw)

// ============================================================
// SECTION 11 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define WIDGET_DEBUG(fmt, ...)      printf("[WIDGET] " fmt, ##__VA_ARGS__)
#else
    #define WIDGET_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 12 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_WIDGETS_H