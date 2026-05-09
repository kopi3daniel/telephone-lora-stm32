/**
 * @file touch_manager.cpp
 * @brief Implémentation du gestionnaire tactile haut niveau
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans touch_manager.h.
 * 
 * Il unifie les drivers tactiles (XPT2046, calibration) et fournit
 * une API simple pour l'interface utilisateur.
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "touch_manager.h"
#include "xpt2046_driver.h"
#include "touch_calibration.h"
#include "../display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Configuration du gestionnaire tactile */
static TouchManager_Config touch_config = {
    .scanIntervalMs = 20,
    .debounceMs = 30,
    .longPressMs = 500,
    .doubleTapMs = 300,
    .swipeThreshold = 30,
    .enableGestures = true,
    .enableMultiTouch = false,
    .invertX = false,
    .invertY = false,
    .swapXY = false
};

/** @brief État du gestionnaire */
static TouchManager_State touch_state = {
    .currentState = TOUCH_STATE_IDLE,
    .lastEvent = TOUCH_EVENT_NONE,
    .touchCount = 0,
    .errorCount = 0,
    .gestureDetected = TOUCH_GESTURE_NONE
};

/** @brief Callbacks */
static TouchManager_Callback touch_callback = NULL;
static TouchManager_GestureCallback gesture_callback = NULL;

/** @brief Historique des positions (pour les gestes) */
#define TOUCH_HISTORY_SIZE      20
static TouchPoint touch_history[TOUCH_HISTORY_SIZE];
static uint8_t history_index = 0;
static uint8_t history_count = 0;

/** @brief Timers */
static uint32_t last_scan_time = 0;
static uint32_t press_start_time = 0;
static uint32_t last_release_time = 0;
static uint32_t last_tap_time = 0;

/** @brief Points de suivi */
static TouchPoint current_point = {0};
static TouchPoint last_point = {0};
static TouchPoint press_point = {0};

/** @brief Flags d'état */
static bool initialized = false;
static bool calibration_done = false;
static bool touch_active = false;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le gestionnaire tactile
 */
bool touch_manager_init(const TouchManager_Config* config)
{
    TOUCH_DEBUG("Initialisation du gestionnaire tactile...\n");
    
    // Sauvegarder la configuration
    if (config != NULL)
    {
        memcpy(&touch_config, config, sizeof(TouchManager_Config));
    }
    
    // Initialiser le driver XPT2046
    XPT2046_Config xpt_config = {0};
    xpt_config.samples = 5;
    xpt_config.touchThreshold = 100;
    xpt_config.scanIntervalMs = touch_config.scanIntervalMs;
    xpt_config.enableMedianFilter = true;
    xpt_config.enableAverageFilter = true;
    
    XPT2046_Error err = xpt2046_init(&xpt_config);
    if (err != XPT2046_OK)
    {
        TOUCH_DEBUG("Échec initialisation XPT2046\n");
        return false;
    }
    
    // Initialiser la calibration
    calibration_init();
    
    // Enregistrer le callback du driver
    xpt2046_set_callback(touch_driver_callback);
    
    // Configurer la détection de gestes
    if (touch_config.enableGestures)
    {
        XPT2046_GestureConfig gc = {0};
        gc.enableGestures = true;
        gc.longPressTimeout = touch_config.longPressMs;
        gc.doubleTapTimeout = touch_config.doubleTapMs;
        gc.swipeThreshold = touch_config.swipeThreshold;
        xpt2046_gesture_enable(&gc);
    }
    
    // Réinitialiser l'état
    memset(&touch_state, 0, sizeof(TouchManager_State));
    memset(touch_history, 0, sizeof(touch_history));
    history_index = 0;
    history_count = 0;
    
    initialized = true;
    
    TOUCH_DEBUG("Gestionnaire tactile initialisé\n");
    return true;
}

/**
 * @brief Désinitialise le gestionnaire
 */
void touch_manager_deinit(void)
{
    xpt2046_deinit();
    initialized = false;
}

/**
 * @brief Vérifie si le gestionnaire est prêt
 */
bool touch_manager_is_ready(void)
{
    return initialized;
}

// ============================================================
// SECTION 2 : SCAN ET TRAITEMENT
// ============================================================

/**
 * @brief Traitement périodique (à appeler dans la boucle principale)
 */
void touch_manager_process(void)
{
    if (!initialized) return;
    
    uint32_t now = HAL_GetTick();
    
    // Respecter l'intervalle de scan
    if ((now - last_scan_time) < touch_config.scanIntervalMs)
    {
        return;
    }
    last_scan_time = now;
    
    // Scanner le contrôleur tactile
    xpt2046_scan();
    
    // Mettre à jour l'état
    update_touch_state();
    
    // Vérifier les gestes si activés
    if (touch_config.enableGestures)
    {
        check_gestures();
    }
}

/**
 * @brief Callback interne du driver XPT2046
 */
static void touch_driver_callback(XPT2046_TouchEvent event, uint16_t x, uint16_t y)
{
    switch (event)
    {
        case XPT2046_EVENT_PRESS:
            handle_touch_press(x, y);
            break;
            
        case XPT2046_EVENT_MOVE:
            handle_touch_move(x, y);
            break;
            
        case XPT2046_EVENT_RELEASE:
            handle_touch_release(x, y);
            break;
            
        default:
            break;
    }
}

/**
 * @brief Gère l'événement d'appui
 */
static void handle_touch_press(uint16_t x, uint16_t y)
{
    // Appliquer les transformations d'orientation
    apply_orientation(&x, &y);
    
    // Vérifier la validité des coordonnées
    if (!calibration_is_valid_pixel(x, y)) return;
    
    // Mettre à jour l'état
    touch_state.currentState = TOUCH_STATE_PRESSED;
    touch_state.lastEvent = TOUCH_EVENT_PRESS;
    touch_active = true;
    touch_state.touchCount++;
    
    // Sauvegarder le point d'appui
    current_point.x = x;
    current_point.y = y;
    current_point.timestamp = HAL_GetTick();
    current_point.valid = true;
    
    press_point = current_point;
    last_point = current_point;
    press_start_time = current_point.timestamp;
    
    // Ajouter à l'historique
    add_to_history(&current_point);
    
    // Vérifier le double tap
    if ((current_point.timestamp - last_tap_time) < touch_config.doubleTapMs)
    {
        touch_state.gestureDetected = TOUCH_GESTURE_DOUBLE_TAP;
        if (gesture_callback) gesture_callback(TOUCH_GESTURE_DOUBLE_TAP, x, y);
    }
    
    // Notifier l'application
    if (touch_callback)
    {
        touch_callback(TOUCH_EVENT_PRESS, x, y);
    }
    
    TOUCH_DEBUG("Press (%d, %d)\n", x, y);
}

/**
 * @brief Gère l'événement de déplacement
 */
static void handle_touch_move(uint16_t x, uint16_t y)
{
    if (!touch_active) return;
    
    // Appliquer les transformations
    apply_orientation(&x, &y);
    if (!calibration_is_valid_pixel(x, y)) return;
    
    // Calculer le déplacement
    int16_t dx = (int16_t)x - (int16_t)last_point.x;
    int16_t dy = (int16_t)y - (int16_t)last_point.y;
    
    // Mettre à jour l'état
    touch_state.currentState = TOUCH_STATE_HELD;
    touch_state.lastEvent = TOUCH_EVENT_MOVE;
    touch_state.totalDistance += sqrtf(dx*dx + dy*dy);
    
    // Mettre à jour le point courant
    current_point.x = x;
    current_point.y = y;
    current_point.timestamp = HAL_GetTick();
    
    last_point = current_point;
    
    // Ajouter à l'historique
    add_to_history(&current_point);
    
    // Détecter le swipe pendant le mouvement
    if (touch_config.enableGestures && history_count >= 3)
    {
        TouchPoint first = touch_history[(history_index - history_count) % TOUCH_HISTORY_SIZE];
        int16_t totalDx = (int16_t)x - (int16_t)first.x;
        int16_t totalDy = (int16_t)y - (int16_t)first.y;
        
        if (abs(totalDx) > touch_config.swipeThreshold || 
            abs(totalDy) > touch_config.swipeThreshold)
        {
            TouchGesture gesture;
            
            if (abs(totalDx) > abs(totalDy))
            {
                gesture = (totalDx > 0) ? TOUCH_GESTURE_SWIPE_RIGHT : TOUCH_GESTURE_SWIPE_LEFT;
            }
            else
            {
                gesture = (totalDy > 0) ? TOUCH_GESTURE_SWIPE_DOWN : TOUCH_GESTURE_SWIPE_UP;
            }
            
            if (gesture != touch_state.gestureDetected)
            {
                touch_state.gestureDetected = gesture;
                if (gesture_callback) gesture_callback(gesture, x, y);
            }
        }
    }
    
    // Notifier l'application
    if (touch_callback)
    {
        touch_callback(TOUCH_EVENT_MOVE, x, y);
    }
}

/**
 * @brief Gère l'événement de relâchement
 */
static void handle_touch_release(uint16_t x, uint16_t y)
{
    if (!touch_active) return;
    
    // Appliquer les transformations
    apply_orientation(&x, &y);
    
    uint32_t releaseTime = HAL_GetTick();
    uint32_t holdDuration = releaseTime - press_start_time;
    
    // Mettre à jour l'état
    touch_state.currentState = TOUCH_STATE_RELEASED;
    touch_state.lastEvent = TOUCH_EVENT_RELEASE;
    touch_active = false;
    
    current_point.x = x;
    current_point.y = y;
    current_point.timestamp = releaseTime;
    
    last_release_time = releaseTime;
    
    // Détecter les gestes de fin
    if (touch_config.enableGestures)
    {
        if (holdDuration < touch_config.doubleTapMs)
        {
            // Tap simple
            touch_state.gestureDetected = TOUCH_GESTURE_TAP;
            last_tap_time = releaseTime;
            
            if (gesture_callback) gesture_callback(TOUCH_GESTURE_TAP, x, y);
        }
        else if (holdDuration > touch_config.longPressMs)
        {
            // Appui long
            touch_state.gestureDetected = TOUCH_GESTURE_LONG_PRESS;
            if (gesture_callback) gesture_callback(TOUCH_GESTURE_LONG_PRESS, x, y);
        }
    }
    
    // Notifier l'application
    if (touch_callback)
    {
        touch_callback(TOUCH_EVENT_RELEASE, x, y);
    }
    
    TOUCH_DEBUG("Release (%d, %d) - Duration: %lu ms\n", x, y, (unsigned long)holdDuration);
}

/**
 * @brief Met à jour l'état tactile
 */
static void update_touch_state(void)
{
    if (!touch_active)
    {
        touch_state.currentState = TOUCH_STATE_IDLE;
        touch_state.lastEvent = TOUCH_EVENT_NONE;
    }
}

/**
 * @brief Vérifie les gestes
 */
static void check_gestures(void)
{
    // La détection de gestes est principalement gérée dans les handlers d'événements
    // Cette fonction peut être utilisée pour des vérifications périodiques
    
    if (touch_active)
    {
        uint32_t holdDuration = HAL_GetTick() - press_start_time;
        
        // Vérifier l'appui long
        if (holdDuration > touch_config.longPressMs && 
            touch_state.gestureDetected != TOUCH_GESTURE_LONG_PRESS)
        {
            touch_state.gestureDetected = TOUCH_GESTURE_LONG_PRESS;
            if (gesture_callback) 
            {
                gesture_callback(TOUCH_GESTURE_LONG_PRESS, 
                                current_point.x, current_point.y);
            }
            TOUCH_DEBUG("Long press détecté\n");
        }
    }
}

// ============================================================
// SECTION 3 : TRANSFORMATIONS
// ============================================================

/**
 * @brief Applique les transformations d'orientation
 */
static void apply_orientation(uint16_t* x, uint16_t* y)
{
    if (x == NULL || y == NULL) return;
    
    uint16_t temp;
    
    // Échanger X et Y si nécessaire
    if (touch_config.swapXY)
    {
        temp = *x;
        *x = *y;
        *y = temp;
    }
    
    // Inverser X si nécessaire
    if (touch_config.invertX)
    {
        *x = DISPLAY_WIDTH - 1 - *x;
    }
    
    // Inverser Y si nécessaire
    if (touch_config.invertY)
    {
        *y = DISPLAY_HEIGHT - 1 - *y;
    }
}

// ============================================================
// SECTION 4 : HISTORIQUE
// ============================================================

/**
 * @brief Ajoute un point à l'historique
 */
static void add_to_history(const TouchPoint* point)
{
    if (point == NULL) return;
    
    touch_history[history_index % TOUCH_HISTORY_SIZE] = *point;
    history_index++;
    
    if (history_count < TOUCH_HISTORY_SIZE)
    {
        history_count++;
    }
}

/**
 * @brief Récupère un point de l'historique
 */
static TouchPoint* get_history_point(uint8_t index)
{
    if (index >= history_count) return NULL;
    
    uint8_t realIndex = (history_index - history_count + index) % TOUCH_HISTORY_SIZE;
    return &touch_history[realIndex];
}

// ============================================================
// SECTION 5 : API PUBLIQUE
// ============================================================

/**
 * @brief Vérifie si l'écran est touché
 */
bool touch_manager_is_touched(void)
{
    return touch_active;
}

/**
 * @brief Récupère la position actuelle
 */
bool touch_manager_get_position(uint16_t* x, uint16_t* y)
{
    if (!touch_active) return false;
    
    if (x) *x = current_point.x;
    if (y) *y = current_point.y;
    
    return current_point.valid;
}

/**
 * @brief Récupère le dernier événement
 */
TouchEvent touch_manager_get_event(void)
{
    TouchEvent event = touch_state.lastEvent;
    touch_state.lastEvent = TOUCH_EVENT_NONE;  // Consommer l'événement
    return event;
}

/**
 * @brief Récupère le geste détecté
 */
TouchGesture touch_manager_get_gesture(void)
{
    TouchGesture gesture = touch_state.gestureDetected;
    touch_state.gestureDetected = TOUCH_GESTURE_NONE;  // Consommer le geste
    return gesture;
}

/**
 * @brief Vérifie si un geste spécifique est détecté
 */
bool touch_manager_is_gesture(TouchGesture gesture)
{
    return (touch_state.gestureDetected == gesture);
}

/**
 * @brief Attend un événement tactile
 */
bool touch_manager_wait_for_event(uint32_t timeoutMs)
{
    uint32_t start = HAL_GetTick();
    
    while ((HAL_GetTick() - start) < timeoutMs)
    {
        touch_manager_process();
        
        if (touch_state.lastEvent != TOUCH_EVENT_NONE)
        {
            return true;
        }
        
        HAL_Delay(10);
    }
    
    return false;
}

/**
 * @brief Récupère la durée de l'appui actuel
 */
uint32_t touch_manager_get_hold_duration(void)
{
    if (!touch_active) return 0;
    return HAL_GetTick() - press_start_time;
}

/**
 * @brief Récupère la distance totale parcourue
 */
float touch_manager_get_total_distance(void)
{
    return touch_state.totalDistance;
}

// ============================================================
// SECTION 6 : CONFIGURATION
// ============================================================

/**
 * @brief Définit l'orientation du tactile
 */
void touch_manager_set_orientation(bool swapXY, bool invertX, bool invertY)
{
    touch_config.swapXY = swapXY;
    touch_config.invertX = invertX;
    touch_config.invertY = invertY;
    
    // Propager au driver
    xpt2046_set_orientation(swapXY, invertX, invertY);
}

/**
 * @brief Définit l'intervalle de scan
 */
void touch_manager_set_scan_interval(uint8_t ms)
{
    touch_config.scanIntervalMs = ms;
    xpt2046_set_scan_interval(ms);
}

/**
 * @brief Active/désactive la détection de gestes
 */
void touch_manager_gesture_enable(bool enable)
{
    touch_config.enableGestures = enable;
    
    if (enable)
    {
        xpt2046_gesture_enable(NULL);
    }
    else
    {
        xpt2046_gesture_disable();
    }
}

// ============================================================
// SECTION 7 : CALLBACKS
// ============================================================

/**
 * @brief Enregistre le callback tactile
 */
void touch_manager_set_callback(TouchManager_Callback callback)
{
    touch_callback = callback;
}

/**
 * @brief Enregistre le callback de gestes
 */
void touch_manager_set_gesture_callback(TouchManager_GestureCallback callback)
{
    gesture_callback = callback;
}

// ============================================================
// SECTION 8 : CALIBRATION
// ============================================================

/**
 * @brief Vérifie si le tactile est calibré
 */
bool touch_manager_is_calibrated(void)
{
    return calibration_is_saved() || xpt2046_is_calibrated();
}

/**
 * @brief Lance la calibration
 */
bool touch_manager_start_calibration(void)
{
    if (!initialized) return false;
    
    // Créer l'interface de calibration
    CalibrationUI calib_ui = {
        .drawPoint = touch_calibration_draw_point,
        .drawInstruction = touch_calibration_draw_text,
        .clearScreen = touch_calibration_clear,
        .confirmCalibration = touch_calibration_confirm,
        .showResults = touch_calibration_show_results
    };
    
    return calibration_start(CALIB_METHOD_3_POINTS, &calib_ui);
}

// ============================================================
// SECTION 9 : CALLBACKS D'INTERFACE DE CALIBRATION
// ============================================================

/**
 * @brief Dessine le point de calibration
 */
static void touch_calibration_draw_point(uint16_t x, uint16_t y, uint16_t radius, uint16_t color)
{
    // Utiliser le display manager pour dessiner
    display_fill_circle(x, y, radius, color);
    display_draw_circle(x, y, radius + 2, ILI9488_WHITE);
    display_swap_buffers();
}

/**
 * @brief Dessine le texte d'instruction
 */
static void touch_calibration_draw_text(const char* text)
{
    display_set_font(&font_8x16);
    display_set_text_color(ILI9488_WHITE);
    display_draw_text_center(DISPLAY_HEIGHT - 30, text, ILI9488_WHITE, 1);
    display_swap_buffers();
}

/**
 * @brief Efface l'écran pour la calibration
 */
static void touch_calibration_clear(void)
{
    display_clear(ILI9488_BLACK);
    display_swap_buffers();
}

/**
 * @brief Demande confirmation
 */
static bool touch_calibration_confirm(void)
{
    // Pour l'instant, toujours confirmer
    return true;
}

/**
 * @brief Affiche les résultats
 */
static void touch_calibration_show_results(CalibrationData* data)
{
    if (data == NULL) return;
    
    display_clear(ILI9488_BLACK);
    display_set_font(&font_5x7);
    display_set_text_color(ILI9488_GREEN);
    display_draw_text_center(50, "Calibration terminee !", ILI9488_GREEN, 2);
    
    char buf[64];
    snprintf(buf, sizeof(buf), "Erreur max: %.1f px", data->matrix.maxError);
    display_draw_text_center(150, buf, ILI9488_WHITE, 1);
    
    snprintf(buf, sizeof(buf), "Erreur moy: %.1f px", data->matrix.avgError);
    display_draw_text_center(180, buf, ILI9488_WHITE, 1);
    
    display_swap_buffers();
    HAL_Delay(2000);
}

// ============================================================
// SECTION 10 : DÉBOGAGE
// ============================================================

/**
 * @brief Affiche les informations
 */
void touch_manager_print_info(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   INFORMATIONS GESTIONNAIRE TACTILE          ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Initialisé  : %-31s ║\n", initialized ? "Oui" : "Non");
    printf("║ Calibré     : %-31s ║\n", touch_manager_is_calibrated() ? "Oui" : "Non");
    printf("║ En toucher  : %-31s ║\n", touch_active ? "Oui" : "Non");
    printf("║ Gestes      : %-31s ║\n", touch_config.enableGestures ? "Activés" : "Désactivés");
    printf("║ Touchers    : %-31lu ║\n", (unsigned long)touch_state.touchCount);
    printf("║ Position    : (%d, %d)                      ║\n", current_point.x, current_point.y);
    printf("║ Intervalle  : %d ms                           ║\n", touch_config.scanIntervalMs);
    printf("╚══════════════════════════════════════════════╝\n\n");
}

/**
 * @brief Affiche l'état
 */
void touch_manager_print_state(void)
{
    const char* stateStr = "Inconnu";
    switch (touch_state.currentState)
    {
        case TOUCH_STATE_IDLE:     stateStr = "En attente"; break;
        case TOUCH_STATE_PRESSED:  stateStr = "Appuyé"; break;
        case TOUCH_STATE_HELD:     stateStr = "Maintenu"; break;
        case TOUCH_STATE_RELEASED: stateStr = "Relâché"; break;
    }
    
    const char* gestureStr = "Aucun";
    switch (touch_state.gestureDetected)
    {
        case TOUCH_GESTURE_TAP:          gestureStr = "Tap"; break;
        case TOUCH_GESTURE_DOUBLE_TAP:   gestureStr = "Double Tap"; break;
        case TOUCH_GESTURE_LONG_PRESS:   gestureStr = "Appui long"; break;
        case TOUCH_GESTURE_SWIPE_LEFT:   gestureStr = "Swipe Gauche"; break;
        case TOUCH_GESTURE_SWIPE_RIGHT:  gestureStr = "Swipe Droite"; break;
        case TOUCH_GESTURE_SWIPE_UP:     gestureStr = "Swipe Haut"; break;
        case TOUCH_GESTURE_SWIPE_DOWN:   gestureStr = "Swipe Bas"; break;
        default: break;
    }
    
    printf("[TACTILE] État: %s | Geste: %s | Pos: (%d,%d) | Durée: %lu ms\n",
           stateStr, gestureStr,
           current_point.x, current_point.y,
           (unsigned long)touch_manager_get_hold_duration());
}