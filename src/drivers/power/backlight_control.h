/**
 * @file backlight_control.h
 * @brief Contrôle du rétroéclairage de l'écran et du clavier
 * 
 * Ce fichier gère le rétroéclairage :
 * - Écran TFT (PWM via TIM9)
 * - Clavier (PWM via TIM4)
 * - Gradation automatique (dimming)
 * - Extinction automatique après inactivité
 * - Transition en douceur (fade)
 * - Modes d'économie d'énergie
 * 
 * Niveaux de luminosité :
 * - 0-255 : PWM duty cycle (0 = éteint, 255 = max)
 * - Pourcentage : 0-100%
 * 
 * Stratégie d'économie :
 * - Activité → Luminosité normale (configurable)
 * - Inactivité 10s → Luminosité réduite (dim)
 * - Inactivité 30s → Extinction écran
 * - Inactivité 60s → Extinction clavier
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef BACKLIGHT_CONTROL_H
#define BACKLIGHT_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "../../config.h"
#include "../../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define BACKLIGHT_VERSION               "1.0.0"

/** @brief Luminosité maximale (PWM) */
#define BACKLIGHT_MAX_BRIGHTNESS        255

/** @brief Luminosité minimale (PWM) */
#define BACKLIGHT_MIN_BRIGHTNESS        0

/** @brief Luminosité par défaut (PWM) */
#define BACKLIGHT_DEFAULT_BRIGHTNESS    200

/** @brief Luminosité réduite (dim) */
#define BACKLIGHT_DIM_BRIGHTNESS        30

/** @brief Luminosité nocturne */
#define BACKLIGHT_NIGHT_BRIGHTNESS      10

/** @brief Fréquence PWM (Hz) */
#define BACKLIGHT_PWM_FREQUENCY         5000

/** @brief Résolution PWM (bits) */
#define BACKLIGHT_PWM_RESOLUTION        8       // 0-255

/** @brief Durée de fondu par défaut (ms) */
#define BACKLIGHT_DEFAULT_FADE_MS       200

/** @brief Timeout avant réduction (secondes) */
#define BACKLIGHT_DIM_TIMEOUT_S         10

/** @brief Timeout avant extinction écran (secondes) */
#define BACKLIGHT_OFF_TIMEOUT_S         30

/** @brief Timeout avant extinction clavier (secondes) */
#define BACKLIGHT_KEYPAD_OFF_TIMEOUT_S  60

/** @brief Instance Timer pour l'écran */
#define BACKLIGHT_TFT_TIMER             TIM9
#define BACKLIGHT_TFT_CHANNEL           TIM_CHANNEL_4
#define BACKLIGHT_TFT_PORT              GPIOE
#define BACKLIGHT_TFT_PIN               GPIO_PIN_4

/** @brief Instance Timer pour le clavier */
#define BACKLIGHT_KEYPAD_TIMER          TIM4
#define BACKLIGHT_KEYPAD_CHANNEL        TIM_CHANNEL_3
#define BACKLIGHT_KEYPAD_PORT           GPIOG
#define BACKLIGHT_KEYPAD_PIN            GPIO_PIN_13

// ============================================================
// SECTION 2 : TYPES DE RÉTROÉCLAIRAGE
// ============================================================

/**
 * @brief Cibles du rétroéclairage
 */
typedef enum {
    BACKLIGHT_TARGET_TFT    = (1 << 0),   // Écran TFT
    BACKLIGHT_TARGET_KEYPAD = (1 << 1),   // Clavier
    BACKLIGHT_TARGET_ALL    = (BACKLIGHT_TARGET_TFT | BACKLIGHT_TARGET_KEYPAD)
} BacklightTarget;

/**
 * @brief Modes de luminosité
 */
typedef enum {
    BACKLIGHT_MODE_NORMAL   = 0,    // Luminosité normale
    BACKLIGHT_MODE_DIM      = 1,    // Luminosité réduite
    BACKLIGHT_MODE_NIGHT    = 2,    // Mode nuit
    BACKLIGHT_MODE_OFF      = 3,    // Éteint
    BACKLIGHT_MODE_MAX      = 4     // Maximum (temporaire)
} BacklightMode;

/**
 * @brief État du rétroéclairage
 */
typedef enum {
    BACKLIGHT_STATE_ON      = 0,    // Allumé
    BACKLIGHT_STATE_DIM     = 1,    // Réduit
    BACKLIGHT_STATE_OFF     = 2,    // Éteint
    BACKLIGHT_STATE_FADING  = 3     // En transition
} BacklightState;

// ============================================================
// SECTION 3 : CONFIGURATION
// ============================================================

/**
 * @brief Configuration du rétroéclairage
 */
typedef struct {
    // Luminosités
    uint8_t normalBrightness;       // Luminosité normale (0-255)
    uint8_t dimBrightness;          // Luminosité réduite
    uint8_t nightBrightness;        // Luminosité nocturne
    
    // Timeouts (secondes, 0 = désactivé)
    uint16_t dimTimeoutS;           // Avant réduction
    uint16_t offTimeoutS;           // Avant extinction écran
    uint16_t keypadOffTimeoutS;     // Avant extinction clavier
    
    // Fondu
    bool enableFade;                // Activer les transitions douces
    uint16_t fadeDurationMs;        // Durée du fondu
    
    // Mode nuit
    bool enableNightMode;           // Activer le mode nuit automatique
    uint8_t nightModeStartHour;     // Heure de début (0-23)
    uint8_t nightModeEndHour;       // Heure de fin
    
    // Économie d'énergie
    bool enableAutoOff;             // Extinction automatique
    bool enableAutoDim;             // Réduction automatique
    
    // Clavier
    bool keypadBacklightEnabled;    // Rétroéclairage clavier
    uint8_t keypadBrightness;       // Luminosité clavier
    bool keypadFollowTft;           // Suivre l'état de l'écran
} Backlight_Config;

// ============================================================
// SECTION 4 : ÉTAT
// ============================================================

/**
 * @brief État du contrôleur de rétroéclairage
 */
typedef struct {
    bool initialized;               // Module initialisé
    
    // État actuel
    uint8_t tftBrightness;          // Luminosité écran (0-255)
    uint8_t keypadBrightness;       // Luminosité clavier (0-255)
    BacklightState tftState;        // État de l'écran
    BacklightState keypadState;     // État du clavier
    BacklightMode currentMode;      // Mode actuel
    
    // Fondu
    bool fading;                    // Fondu en cours
    uint8_t fadeFrom;               // Luminosité de départ
    uint8_t fadeTo;                 // Luminosité d'arrivée
    uint32_t fadeStartTime;        // Début du fondu
    uint16_t fadeDurationMs;       // Durée
    
    // Timers d'inactivité
    uint32_t lastActivityTime;     // Dernière activité
    uint32_t lastKeypadActivity;   // Dernière activité clavier
    
    // Statistiques
    uint32_t totalOnTime;           // Temps total allumé (secondes)
    uint32_t totalDimTime;          // Temps total réduit
    uint32_t totalOffTime;          // Temps total éteint
    
    // Configuration
    Backlight_Config config;
} Backlight_State;

// ============================================================
// SECTION 5 : CALLBACKS
// ============================================================

typedef void (*Backlight_StateCallback)(BacklightState state);

// ============================================================
// SECTION 6 : FONCTIONS D'INITIALISATION
// ============================================================

bool backlight_init(const Backlight_Config* config);
void backlight_deinit(void);
bool backlight_is_ready(void);
Backlight_State* backlight_get_state(void);

// ============================================================
// SECTION 7 : FONCTIONS DE CONTRÔLE
// ============================================================

/**
 * @brief Allume le rétroéclairage
 */
void backlight_on(BacklightTarget target);

/**
 * @brief Éteint le rétroéclairage
 */
void backlight_off(BacklightTarget target);

/**
 * @brief Bascule l'état
 */
void backlight_toggle(BacklightTarget target);

/**
 * @brief Définit la luminosité
 * @param brightness Luminosité (0-255)
 */
void backlight_set_brightness(BacklightTarget target, uint8_t brightness);

/**
 * @brief Récupère la luminosité
 */
uint8_t backlight_get_brightness(BacklightTarget target);

/**
 * @brief Définit le mode
 */
void backlight_set_mode(BacklightMode mode);

/**
 * @brief Récupère le mode actuel
 */
BacklightMode backlight_get_mode(void);

// ============================================================
// SECTION 8 : FONCTIONS DE TRANSITION
// ============================================================

/**
 * @brief Fondu vers une luminosité
 * @param targetBrightness Luminosité cible (0-255)
 * @param durationMs Durée du fondu
 */
void backlight_fade_to(uint8_t targetBrightness, uint16_t durationMs);

/**
 * @brief Fondu entrant (fade in)
 */
void backlight_fade_in(uint16_t durationMs);

/**
 * @brief Fondu sortant (fade out)
 */
void backlight_fade_out(uint16_t durationMs);

/**
 * @brief Vérifie si un fondu est en cours
 */
bool backlight_is_fading(void);

// ============================================================
// SECTION 9 : FONCTIONS DE GESTION D'ACTIVITÉ
// ============================================================

/**
 * @brief Signale une activité (réinitialise les timeouts)
 */
void backlight_activity(void);

/**
 * @brief Signale une activité clavier
 */
void backlight_keypad_activity(void);

/**
 * @brief Traitement périodique
 */
void backlight_process(void);

// ============================================================
// SECTION 10 : FONCTIONS DE MODE NUIT
// ============================================================

void backlight_night_mode_enable(bool enable);
bool backlight_is_night_mode(void);
void backlight_set_night_mode_hours(uint8_t startHour, uint8_t endHour);

// ============================================================
// SECTION 11 : FONCTIONS DE CALLBACKS
// ============================================================

void backlight_set_state_callback(Backlight_StateCallback callback);

// ============================================================
// SECTION 12 : FONCTIONS DE DÉBOGAGE
// ============================================================

void backlight_print_state(void);
void backlight_print_config(void);
bool backlight_self_test(void);

// ============================================================
// SECTION 13 : MACROS UTILITAIRES
// ============================================================

#define BACKLIGHT_TFT_ON()              backlight_on(BACKLIGHT_TARGET_TFT)
#define BACKLIGHT_TFT_OFF()             backlight_off(BACKLIGHT_TARGET_TFT)
#define BACKLIGHT_KEYPAD_ON()           backlight_on(BACKLIGHT_TARGET_KEYPAD)
#define BACKLIGHT_KEYPAD_OFF()          backlight_off(BACKLIGHT_TARGET_KEYPAD)
#define BACKLIGHT_ALL_ON()              backlight_on(BACKLIGHT_TARGET_ALL)
#define BACKLIGHT_ALL_OFF()             backlight_off(BACKLIGHT_TARGET_ALL)

// ============================================================
// SECTION 14 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define BACKLIGHT_DEBUG(fmt, ...)   printf("[BACKLIGHT] " fmt, ##__VA_ARGS__)
#else
    #define BACKLIGHT_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // BACKLIGHT_CONTROL_H