/**
 * @file ui_slider.h
 * @brief Widget Curseur (UISlider) - Définition et fonctions
 * 
 * Ce fichier est optionnel. La définition complète de UISlider
 * se trouve déjà dans ui_widgets.h (section 5).
 * 
 * Fonctionnalités :
 * - Curseur horizontal avec valeur 0-100
 * - Track (fond) et fill (remplissage) personnalisables
 * - Thumb (poignée) redimensionnable
 * - Pas (step) configurable
 * - Styles prédéfinis (normal, volume, brightness)
 * - Callback onValueChanged
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_SLIDER_H
#define UI_SLIDER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "ui_core.h"
#include "ui_styles.h"

// ============================================================
// STRUCTURE DU SLIDER
// ============================================================

/**
 * @brief Widget Curseur (Slider)
 * 
 * Hérite de UIWidget et ajoute :
 * - Une valeur (0-100 par défaut)
 * - Des bornes min/max
 * - Un pas (step) pour les valeurs discrètes
 * - Un track, un fill et un thumb personnalisables
 * - Des callbacks de changement de valeur
 */
typedef struct UISlider {
    UIWidget base;                      // Widget de base (héritage)
    
    // --- Valeur ---
    uint8_t value;                      // Valeur actuelle
    uint8_t minValue;                   // Valeur minimale
    uint8_t maxValue;                   // Valeur maximale
    uint8_t step;                       // Incrément (0 = continu)
    
    // --- Apparence ---
    SliderAppearance appearance;        // Style visuel
    
    // --- Comportement ---
    bool showValue;                     // Afficher la valeur numérique
    bool showLabels;                    // Afficher min/max
    char valueSuffix[8];                // Suffixe de la valeur (ex: "%", " dBm")
    
    // --- Callbacks ---
    void (*onValueChanged)(struct UISlider* slider, uint8_t value);   // Valeur changée
    void (*onValueChanging)(struct UISlider* slider, uint8_t value);  // En cours de changement
    void (*onDragStart)(struct UISlider* slider);                      // Début du glissement
    void (*onDragEnd)(struct UISlider* slider);                        // Fin du glissement
    
    // --- État ---
    bool dragging;                      // En cours de glissement ?
} UISlider;

// ============================================================
// FONCTIONS DE CRÉATION
// ============================================================

/**
 * @brief Crée un nouveau slider
 * @param name Nom unique du widget
 * @param rect Position et taille
 * @param min Valeur minimale
 * @param max Valeur maximale
 * @param value Valeur initiale
 * @return Pointeur vers le slider créé
 */
UISlider* ui_slider_create(const char* name, UIRect rect, uint8_t min, uint8_t max, uint8_t value);

// ============================================================
// FONCTIONS DE CONFIGURATION DU STYLE
// ============================================================

void ui_slider_set_style(UISlider* slider, SliderStyle style);

/**
 * @brief Personnalise les couleurs du slider
 */
void ui_slider_set_colors(UISlider* slider, uint16_t trackColor, 
                           uint16_t fillColor, uint16_t thumbColor);

/**
 * @brief Définit l'épaisseur du track
 */
void ui_slider_set_track_height(UISlider* slider, uint8_t height);

/**
 * @brief Définit le rayon du thumb (poignée)
 */
void ui_slider_set_thumb_radius(UISlider* slider, uint8_t radius);

// ============================================================
// FONCTIONS DE VALEUR
// ============================================================

void ui_slider_set_value(UISlider* slider, uint8_t value);
uint8_t ui_slider_get_value(UISlider* slider);

/**
 * @brief Incrémente la valeur
 */
void ui_slider_increment(UISlider* slider);

/**
 * @brief Décrémente la valeur
 */
void ui_slider_decrement(UISlider* slider);

/**
 * @brief Définit les bornes min/max
 */
void ui_slider_set_range(UISlider* slider, uint8_t min, uint8_t max);

/**
 * @brief Définit le pas (0 = continu)
 */
void ui_slider_set_step(UISlider* slider, uint8_t step);

/**
 * @brief Obtient la valeur sous forme de pourcentage (0-100)
 */
uint8_t ui_slider_get_percent(UISlider* slider);

/**
 * @brief Définit la valeur par pourcentage
 */
void ui_slider_set_percent(UISlider* slider, uint8_t percent);

// ============================================================
// FONCTIONS D'AFFICHAGE
// ============================================================

void ui_slider_show_value(UISlider* slider, bool show);
void ui_slider_show_labels(UISlider* slider, bool show);
void ui_slider_set_value_suffix(UISlider* slider, const char* suffix);

// ============================================================
// MACROS RAPIDES
// ============================================================

#define UI_SLIDER_CREATE(name, x, y, w, h, min, max, val) \
    ui_slider_create(name, UI_RECT(x, y, w, h), min, max, val)

#define UI_SLIDER_GET_VALUE(slider) \
    ui_slider_get_value(slider)

#define UI_SLIDER_SET_VALUE(slider, val) \
    ui_slider_set_value(slider, val)

// ============================================================
// COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_SLIDER_H