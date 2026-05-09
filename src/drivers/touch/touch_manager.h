/**
 * @file touch_manager.h
 * @brief Gestionnaire tactile haut niveau
 * 
 * Ce fichier unifie les drivers tactiles et fournit une API
 * simple pour l'interface utilisateur :
 * - Initialisation automatique des sous-systèmes
 * - Détection des événements (press, move, release)
 * - Reconnaissance de gestes (tap, swipe, long press)
 * - Calibration interactive
 * - Callbacks utilisateur
 * 
 * Architecture :
 * ┌─────────────────────────────────────────────────────────┐
 * │                 TOUCH MANAGER                           │
 * │                                                         │
 * │  ┌──────────┐    ┌──────────────┐    ┌──────────────┐ │
 * │  │ XPT2046  │───►│ Calibration  │───►│ Événements   │ │
 * │  │ Driver   │    │ ADC → Pixels │    │ + Gestes     │ │
 * │  └──────────┘    └──────────────┘    └──────────────┘ │
 * └─────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "xpt2046_defs.h"
#include "touch_calibration.h"
#include "../../config.h"
#include "../../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du gestionnaire tactile */
#define TOUCH_MANAGER_VERSION           "1.0.0"

/** @brief Intervalle de scan par défaut (ms) */
#define TOUCH_SCAN_INTERVAL_MS          20

/** @brief Timeout appui long (ms) */
#define TOUCH_LONG_PRESS_MS             500

/** @brief Timeout double tap (ms) */
#define TOUCH_DOUBLE_TAP_MS             300

/** @brief Seuil de swipe (pixels) */
#define TOUCH_SWIPE_THRESHOLD           30

/** @brief Nombre de points dans l'historique */
#define TOUCH_HISTORY_SIZE              10

// ============================================================
// SECTION 2 : ÉVÉNEMENTS TACTILES
// ============================================================

/**
 * @brief Types d'événements tactiles
 */
typedef enum {
    TOUCH_EVENT_NONE        = 0,    // Aucun événement
    TOUCH_EVENT_PRESS       = 1,    // Appui détecté
    TOUCH_EVENT_MOVE        = 2,    // Déplacement
    TOUCH_EVENT_RELEASE     = 3,    // Relâchement
    TOUCH_EVENT_HOLD        = 4     // Appui long (> 500ms)
} TouchEvent;

/**
 * @brief Types de gestes reconnus
 */
typedef enum {
    TOUCH_GESTURE_NONE          = 0,    // Pas de geste
    TOUCH_GESTURE_TAP           = 1,    // Tap simple
    TOUCH_GESTURE_DOUBLE_TAP    = 2,    // Double tap
    TOUCH_GESTURE_LONG_PRESS    = 3,    // Appui long
    TOUCH_GESTURE_SWIPE_LEFT    = 4,    // Glissement gauche
    TOUCH_GESTURE_SWIPE_RIGHT   = 5,    // Glissement droit
    TOUCH_GESTURE_SWIPE_UP      = 6,    // Glissement haut
    TOUCH_GESTURE_SWIPE_DOWN    = 7     // Glissement bas
} TouchGesture;

/**
 * @brief États du tactile
 */
typedef enum {
    TOUCH_STATE_IDLE        = 0,    // En attente
    TOUCH_STATE_PRESSED     = 1,    // Appuyé
    TOUCH_STATE_HELD        = 2,    // Maintenu
    TOUCH_STATE_RELEASED    = 3     // Relâché
} TouchState;

// ============================================================
// SECTION 3 : POINT TACTILE
// ============================================================

/**
 * @brief Point de coordonnées tactiles
 */
typedef struct {
    uint16_t x;                     // Position X (pixels)
    uint16_t y;                     // Position Y (pixels)
    bool valid;                     // Point valide ?
    uint32_t timestamp;             // Horodatage
} TouchPoint;

// ============================================================
// SECTION 4 : CONFIGURATION
// ============================================================

/**
 * @brief Configuration du gestionnaire tactile
 */
typedef struct {
    uint8_t scanIntervalMs;         // Intervalle de scan
    uint16_t longPressMs;           // Seuil appui long
    uint16_t doubleTapMs;           // Timeout double tap
    uint16_t swipeThreshold;        // Seuil swipe (pixels)
    bool enableGestures;            // Détection de gestes
    bool enableMultiTouch;          // Support multi-touch
    bool invertX;                   // Inverser X
    bool invertY;                   // Inverser Y
    bool swapXY;                    // Échanger X/Y
} TouchManager_Config;

// ============================================================
// SECTION 5 : ÉTAT
// ============================================================

/**
 * @brief État du gestionnaire tactile
 */
typedef struct {
    bool initialized;               // Module initialisé
    TouchState currentState;        // État actuel
    TouchEvent lastEvent;           // Dernier événement
    TouchGesture gestureDetected;   // Geste détecté
    uint32_t touchCount;            // Nombre de touchés
    uint32_t errorCount;            // Nombre d'erreurs
    uint32_t lastScanTime;          // Dernier scan
    float totalDistance;            // Distance totale parcourue
    TouchPoint currentPoint;        // Point actuel
    TouchPoint lastPoint;           // Point précédent
    TouchPoint pressPoint;          // Point d'appui initial
    TouchPoint history[TOUCH_HISTORY_SIZE];  // Historique
    uint8_t historyIndex;
    uint8_t historyCount;
    bool touchActive;               // Écran touché
    uint32_t pressStartTime;        // Début de l'appui
    uint32_t lastTapTime;           // Dernier tap
} TouchManager_State;

// ============================================================
// SECTION 6 : CALLBACKS
// ============================================================

typedef void (*TouchManager_Callback)(TouchEvent event, uint16_t x, uint16_t y);
typedef void (*TouchManager_GestureCallback)(TouchGesture gesture, uint16_t x, uint16_t y);

// ============================================================
// SECTION 7 : FONCTIONS D'INITIALISATION
// ============================================================

bool touch_manager_init(const TouchManager_Config* config);
void touch_manager_deinit(void);
bool touch_manager_is_ready(void);

// ============================================================
// SECTION 8 : FONCTIONS DE TRAITEMENT
// ============================================================

void touch_manager_process(void);
bool touch_manager_is_touched(void);
bool touch_manager_get_position(uint16_t* x, uint16_t* y);
TouchEvent touch_manager_get_event(void);
TouchGesture touch_manager_get_gesture(void);
bool touch_manager_is_gesture(TouchGesture gesture);
bool touch_manager_wait_for_event(uint32_t timeoutMs);
uint32_t touch_manager_get_hold_duration(void);
float touch_manager_get_total_distance(void);

// ============================================================
// SECTION 9 : FONCTIONS DE CONFIGURATION
// ============================================================

void touch_manager_set_orientation(bool swapXY, bool invertX, bool invertY);
void touch_manager_set_scan_interval(uint8_t ms);
void touch_manager_gesture_enable(bool enable);

// ============================================================
// SECTION 10 : FONCTIONS DE CALIBRATION
// ============================================================

bool touch_manager_is_calibrated(void);
bool touch_manager_start_calibration(void);

// ============================================================
// SECTION 11 : FONCTIONS DE CALLBACKS
// ============================================================

void touch_manager_set_callback(TouchManager_Callback callback);
void touch_manager_set_gesture_callback(TouchManager_GestureCallback callback);

// ============================================================
// SECTION 12 : FONCTIONS DE DÉBOGAGE
// ============================================================

void touch_manager_print_info(void);
void touch_manager_print_state(void);
bool touch_manager_self_test(void);

// ============================================================
// SECTION 13 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define TOUCH_DEBUG(fmt, ...)       printf("[TOUCH] " fmt, ##__VA_ARGS__)
#else
    #define TOUCH_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 14 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // TOUCH_MANAGER_H