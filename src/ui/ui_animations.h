/**
 * @file ui_animations.h
 * @brief Animations pour l'interface utilisateur
 * 
 * Ce fichier définit le système d'animations pour :
 * - Transitions entre écrans (slide, fade, zoom)
 * - Animations de widgets (apparition, disparition, pulsation)
 * - Animations d'état (chargement, progression)
 * - Animations d'icônes (rotation, clignotement)
 * - Courbes d'accélération (ease-in, ease-out, bounce)
 * 
 * Types d'animations :
 * - FADE        : Fondu (opacité)
 * - SLIDE       : Glissement (haut, bas, gauche, droite)
 * - ZOOM        : Zoom avant/arrière
 * - ROTATE      : Rotation
 * - PULSE       : Pulsation (échelle)
 * - SHAKE       : Secousse
 * - BOUNCE      : Rebond
 * - COLOR       : Transition de couleur
 * 
 * Courbes d'accélération (easing) :
 * - LINEAR      : Linéaire
 * - EASE_IN     : Démarrage lent
 * - EASE_OUT    : Fin lente
 * - EASE_IN_OUT : Démarrage et fin lents
 * - BOUNCE      : Rebondissement
 * - ELASTIC     : Élastique
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_ANIMATIONS_H
#define UI_ANIMATIONS_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "ui_core.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define UI_ANIMATIONS_VERSION           "1.0.0"

/** @brief Nombre maximum d'animations simultanées */
#define UI_ANIMATIONS_MAX_ACTIVE        10

/** @brief Durée par défaut d'une animation (ms) */
#define UI_ANIMATION_DEFAULT_DURATION   300

// ============================================================
// SECTION 2 : TYPES D'ANIMATIONS
// ============================================================

/**
 * @brief Types d'animations
 */
typedef enum {
    ANIM_TYPE_NONE          = 0,    // Aucune
    ANIM_TYPE_FADE          = 1,    // Fondu (opacité)
    ANIM_TYPE_SLIDE_LEFT    = 2,    // Glissement vers la gauche
    ANIM_TYPE_SLIDE_RIGHT   = 3,    // Glissement vers la droite
    ANIM_TYPE_SLIDE_UP      = 4,    // Glissement vers le haut
    ANIM_TYPE_SLIDE_DOWN    = 5,    // Glissement vers le bas
    ANIM_TYPE_ZOOM_IN       = 6,    // Zoom avant
    ANIM_TYPE_ZOOM_OUT      = 7,    // Zoom arrière
    ANIM_TYPE_ROTATE        = 8,    // Rotation
    ANIM_TYPE_PULSE         = 9,    // Pulsation
    ANIM_TYPE_SHAKE         = 10,   // Secousse
    ANIM_TYPE_BOUNCE        = 11,   // Rebond
    ANIM_TYPE_COLOR         = 12    // Transition de couleur
} AnimationType;

/**
 * @brief Courbes d'accélération (easing functions)
 */
typedef enum {
    EASING_LINEAR           = 0,    // Linéaire
    EASING_EASE_IN          = 1,    // Accélération progressive
    EASING_EASE_OUT         = 2,    // Décélération progressive
    EASING_EASE_IN_OUT      = 3,    // Accélération + décélération
    EASING_BOUNCE           = 4,    // Rebond
    EASING_ELASTIC          = 5,    // Élastique
    EASING_BACK_IN          = 6,    // Recul avant d'avancer
    EASING_BACK_OUT         = 7     // Dépasse avant de s'arrêter
} EasingType;

/**
 * @brief Direction des animations
 */
typedef enum {
    ANIM_DIR_FORWARD        = 0,    // Avant (apparition)
    ANIM_DIR_REVERSE        = 1,    // Arrière (disparition)
    ANIM_DIR_ALTERNATE      = 2     // Alterné (aller-retour)
} AnimationDirection;

/**
 * @brief État d'une animation
 */
typedef enum {
    ANIM_STATE_IDLE         = 0,    // En attente
    ANIM_STATE_RUNNING      = 1,    // En cours
    ANIM_STATE_PAUSED       = 2,    // En pause
    ANIM_STATE_COMPLETED    = 3,    // Terminée
    ANIM_STATE_CANCELLED    = 4     // Annulée
} AnimationState;

// ============================================================
// SECTION 3 : STRUCTURE D'ANIMATION
// ============================================================

/**
 * @brief Valeurs interpolables d'une animation
 */
typedef struct {
    float from;                         // Valeur de départ
    float to;                           // Valeur d'arrivée
    float current;                      // Valeur actuelle
} AnimationValue;

/**
 * @brief Animation
 */
typedef struct {
    uint32_t id;                        // Identifiant unique
    AnimationType type;                 // Type d'animation
    AnimationState state;               // État actuel
    
    // Timing
    uint32_t startTime;                 // Début de l'animation
    uint32_t durationMs;                // Durée totale
    uint32_t delayMs;                   // Délai avant démarrage
    
    // Progression
    float progress;                     // Progression (0.0 à 1.0)
    AnimationDirection direction;       // Direction
    EasingType easing;                  // Courbe d'accélération
    uint8_t repeatCount;                // Nombre de répétitions (0 = infini)
    uint8_t repeatIndex;                // Répétition actuelle
    
    // Valeurs
    AnimationValue opacity;             // Opacité (0.0-1.0)
    AnimationValue scaleX;              // Échelle X
    AnimationValue scaleY;              // Échelle Y
    AnimationValue positionX;           // Position X
    AnimationValue positionY;           // Position Y
    AnimationValue rotation;            // Rotation (degrés)
    AnimationValue colorR;              // Couleur Rouge (0-31)
    AnimationValue colorG;              // Couleur Vert (0-63)
    AnimationValue colorB;              // Couleur Bleu (0-31)
    
    // Cible (widget ou écran)
    UIWidget* target;                   // Widget cible
    UIRect originalRect;                // Rectangle original
    
    // Callbacks
    void (*onStart)(struct UIAnimation* anim);
    void (*onUpdate)(struct UIAnimation* anim, float progress);
    void (*onComplete)(struct UIAnimation* anim);
    void (*onCancel)(struct UIAnimation* anim);
    
    // Rendu personnalisé
    void (*customDraw)(struct UIAnimation* anim, float progress);
    
} UIAnimation;

// ============================================================
// SECTION 4 : ÉTAT DU MODULE
// ============================================================

/**
 * @brief État du module d'animations
 */
typedef struct {
    bool initialized;                           // Module initialisé
    UIAnimation* activeAnimations[UI_ANIMATIONS_MAX_ACTIVE];
    uint8_t activeCount;
    uint32_t nextId;
    bool globalPause;                           // Pause globale
} UIAnimationsState;

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

bool ui_animations_init(void);
void ui_animations_deinit(void);
bool ui_animations_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS DE CRÉATION
// ============================================================

UIAnimation* ui_anim_create(AnimationType type, uint32_t durationMs);
UIAnimation* ui_anim_create_fade(bool fadeIn, uint32_t durationMs);
UIAnimation* ui_anim_create_slide_left(bool slideIn, uint32_t durationMs);
UIAnimation* ui_anim_create_slide_right(bool slideIn, uint32_t durationMs);
UIAnimation* ui_anim_create_slide_up(bool slideIn, uint32_t durationMs);
UIAnimation* ui_anim_create_slide_down(bool slideIn, uint32_t durationMs);
UIAnimation* ui_anim_create_zoom(bool zoomIn, uint32_t durationMs);
UIAnimation* ui_anim_create_pulse(uint32_t durationMs);
UIAnimation* ui_anim_create_shake(uint32_t durationMs);

// ============================================================
// SECTION 7 : FONCTIONS DE CONFIGURATION
// ============================================================

void ui_anim_set_target(UIAnimation* anim, UIWidget* target);
void ui_anim_set_easing(UIAnimation* anim, EasingType easing);
void ui_anim_set_delay(UIAnimation* anim, uint32_t delayMs);
void ui_anim_set_repeat(UIAnimation* anim, uint8_t count);
void ui_anim_set_direction(UIAnimation* anim, AnimationDirection direction);
void ui_anim_set_opacity(UIAnimation* anim, float from, float to);
void ui_anim_set_scale(UIAnimation* anim, float fromX, float toX, float fromY, float toY);
void ui_anim_set_position(UIAnimation* anim, float fromX, float toX, float fromY, float toY);

// ============================================================
// SECTION 8 : FONCTIONS DE CONTRÔLE
// ============================================================

bool ui_anim_start(UIAnimation* anim);
void ui_anim_stop(UIAnimation* anim);
void ui_anim_pause(UIAnimation* anim);
void ui_anim_resume(UIAnimation* anim);
void ui_anim_cancel(UIAnimation* anim);
void ui_anim_stop_all(void);
void ui_anim_pause_all(void);
void ui_anim_resume_all(void);

// ============================================================
// SECTION 9 : FONCTIONS DE TRANSITION D'ÉCRAN
// ============================================================

/**
 * @brief Transition entre deux écrans
 */
void ui_anim_transition_push(UIScreen* from, UIScreen* to, AnimationType type, uint32_t durationMs);
void ui_anim_transition_pop(UIScreen* from, UIScreen* to, AnimationType type, uint32_t durationMs);

// ============================================================
// SECTION 10 : FONCTIONS DE TRAITEMENT
// ============================================================

void ui_animations_process(void);

// ============================================================
// SECTION 11 : FONCTIONS UTILITAIRES
// ============================================================

float ui_anim_easing_calculate(EasingType easing, float t);
uint16_t ui_anim_interpolate_color(uint16_t from, uint16_t to, float progress);

// ============================================================
// SECTION 12 : FONCTIONS DE DÉBOGAGE
// ============================================================

void ui_animations_print_state(void);
void ui_animations_print_active(void);
bool ui_animations_self_test(void);

// ============================================================
// SECTION 13 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define ANIM_DEBUG(fmt, ...)        printf("[ANIM] " fmt, ##__VA_ARGS__)
#else
    #define ANIM_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 14 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_ANIMATIONS_H