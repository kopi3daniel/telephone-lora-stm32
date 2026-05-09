/**
 * @file screen_base.h
 * @brief Classe de base pour tous les écrans de l'application
 * 
 * Ce fichier définit la structure et les fonctions communes
 * à tous les écrans de l'interface utilisateur.
 * 
 * Chaque écran hérite de cette structure et implémente
 * ses propres fonctions d'initialisation, de dessin,
 * de gestion des événements et de nettoyage.
 * 
 * Cycle de vie d'un écran :
 * 1. create()    → Allocation et initialisation
 * 2. onEnter()   → L'écran devient visible
 * 3. onUpdate()  → Appelé périodiquement
 * 4. onDraw()    → Rendu graphique
 * 5. onExit()    → L'écran n'est plus visible
 * 6. destroy()   → Libération des ressources
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SCREEN_BASE_H
#define SCREEN_BASE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "../ui/ui_core.h"
#include "../ui/ui_widgets.h"
#include "../ui/ui_theme.h"
#include "../drivers/keypad/keypad_manager.h"
#include "../drivers/touch/touch_manager.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define SCREEN_BASE_VERSION             "1.0.0"

/** @brief Nombre maximum de widgets par écran */
#define SCREEN_MAX_WIDGETS              UI_MAX_WIDGETS_PER_SCREEN

/** @brief Nombre maximum de timers par écran */
#define SCREEN_MAX_TIMERS               8

// ============================================================
// SECTION 2 : TYPES DE TRANSITIONS
// ============================================================

/**
 * @brief Types de transitions entre écrans
 */
typedef enum {
    SCREEN_TRANSITION_NONE          = 0,    // Pas de transition
    SCREEN_TRANSITION_SLIDE_LEFT    = 1,    // Glissement gauche
    SCREEN_TRANSITION_SLIDE_RIGHT   = 2,    // Glissement droite
    SCREEN_TRANSITION_SLIDE_UP      = 3,    // Glissement haut
    SCREEN_TRANSITION_SLIDE_DOWN    = 4,    // Glissement bas
    SCREEN_TRANSITION_FADE          = 5,    // Fondu
    SCREEN_TRANSITION_ZOOM          = 6     // Zoom
} ScreenTransition;

/**
 * @brief Résultat retourné par un écran
 */
typedef enum {
    SCREEN_RESULT_NONE              = 0,    // Pas de résultat
    SCREEN_RESULT_OK                = 1,    // Succès
    SCREEN_RESULT_CANCEL            = 2,    // Annulé
    SCREEN_RESULT_BACK              = 3,    // Retour
    SCREEN_RESULT_ERROR             = 4,    // Erreur
    SCREEN_RESULT_CUSTOM            = 100   // Personnalisé (100+)
} ScreenResult;

// ============================================================
// SECTION 3 : STRUCTURE DE BASE D'UN ÉCRAN
// ============================================================

/**
 * @brief Structure de base d'un écran
 * 
 * Tous les écrans de l'application héritent de cette structure.
 * Chaque écran doit implémenter les callbacks appropriés.
 */
typedef struct ScreenBase {
    // --- Identification ---
    uint32_t screenId;                  // Identifiant unique
    char name[32];                      // Nom de l'écran
    char title[64];                     // Titre affiché
    
    // --- Widgets ---
    UIWidget* widgets[SCREEN_MAX_WIDGETS];  // Liste des widgets
    uint8_t widgetCount;                    // Nombre de widgets
    
    // --- Navigation ---
    ScreenResult result;                // Résultat à retourner
    void* returnData;                   // Données de retour
    ScreenTransition enterTransition;   // Transition à l'entrée
    ScreenTransition exitTransition;    // Transition à la sortie
    uint32_t transitionDurationMs;      // Durée de la transition
    
    // --- État ---
    bool initialized;                   // Écran initialisé ?
    bool visible;                       // Écran visible ?
    bool needsRedraw;                   // Redessiner ?
    bool needsRefresh;                  // Rafraîchir les données ?
    
    // --- Timers ---
    struct {
        uint32_t intervalMs;            // Intervalle
        uint32_t lastTrigger;           // Dernier déclenchement
        void (*callback)(void);         // Callback
        bool active;                    // Actif ?
    } timers[SCREEN_MAX_TIMERS];
    uint8_t timerCount;
    
    // --- Callbacks du cycle de vie ---
    void (*onCreate)(struct ScreenBase* screen);       // Création
    void (*onEnter)(struct ScreenBase* screen);         // Entrée dans l'écran
    void (*onUpdate)(struct ScreenBase* screen);        // Mise à jour périodique
    void (*onDraw)(struct ScreenBase* screen);           // Dessin
    void (*onExit)(struct ScreenBase* screen);           // Sortie de l'écran
    void (*onDestroy)(struct ScreenBase* screen);        // Destruction
    
    // --- Callbacks d'événements ---
    void (*onTouch)(struct ScreenBase* screen, uint16_t x, uint16_t y, TouchEvent event);
    void (*onKeyPress)(struct ScreenBase* screen, KeyCode key, KeyEvent event);
    void (*onKeyHold)(struct ScreenBase* screen, KeyCode key, uint32_t durationMs);
    void (*onResultReceived)(struct ScreenBase* screen, ScreenResult result, void* data);
    
    // --- Callbacks de confort ---
    bool (*onBackPressed)(struct ScreenBase* screen);     // Retour (true = consommé)
    void (*onRefresh)(struct ScreenBase* screen);          // Rafraîchissement
    
} ScreenBase;

// ============================================================
// SECTION 4 : FONCTIONS DE CRÉATION
// ============================================================

ScreenBase* screen_create(const char* name);
void screen_destroy(ScreenBase* screen);

// ============================================================
// SECTION 5 : FONCTIONS DE GESTION DES WIDGETS
// ============================================================

bool screen_add_widget(ScreenBase* screen, UIWidget* widget);
bool screen_remove_widget(ScreenBase* screen, UIWidget* widget);
UIWidget* screen_find_widget(ScreenBase* screen, const char* name);
void screen_remove_all_widgets(ScreenBase* screen);

// ============================================================
// SECTION 6 : FONCTIONS DE NAVIGATION
// ============================================================

void screen_set_result(ScreenBase* screen, ScreenResult result, void* data);
ScreenResult screen_get_result(ScreenBase* screen);
void screen_set_transition(ScreenBase* screen, ScreenTransition enter, ScreenTransition exit, uint32_t durationMs);
void screen_go_back(ScreenBase* screen);

// ============================================================
// SECTION 7 : FONCTIONS DE TIMERS
// ============================================================

bool screen_add_timer(ScreenBase* screen, uint32_t intervalMs, void (*callback)(void));
bool screen_remove_timer(ScreenBase* screen, uint8_t index);
void screen_process_timers(ScreenBase* screen);

// ============================================================
// SECTION 8 : FONCTIONS DE CYCLE DE VIE
// ============================================================

void screen_enter(ScreenBase* screen);
void screen_exit(ScreenBase* screen);
void screen_update(ScreenBase* screen);
void screen_draw(ScreenBase* screen);
void screen_handle_touch(ScreenBase* screen, uint16_t x, uint16_t y, TouchEvent event);
void screen_handle_key(ScreenBase* screen, KeyCode key, KeyEvent event);

// ============================================================
// SECTION 9 : FONCTIONS UTILITAIRES
// ============================================================

void screen_set_title(ScreenBase* screen, const char* title);
const char* screen_get_title(ScreenBase* screen);
void screen_request_redraw(ScreenBase* screen);
void screen_request_refresh(ScreenBase* screen);

// ============================================================
// SECTION 10 : FONCTIONS DE DÉBOGAGE
// ============================================================

void screen_print_info(ScreenBase* screen);
void screen_print_widgets(ScreenBase* screen);

// ============================================================
// SECTION 11 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define SCREEN_DEBUG(fmt, ...)      printf("[SCREEN] " fmt, ##__VA_ARGS__)
#else
    #define SCREEN_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 12 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SCREEN_BASE_H