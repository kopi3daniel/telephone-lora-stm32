/**
 * @file backlight_control.cpp
 * @brief Implémentation du contrôleur de rétroéclairage
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans backlight_control.h.
 * 
 * Il gère :
 * - Le contrôle PWM de l'écran TFT (TIM9 CH4)
 * - Le contrôle PWM du clavier (TIM4 CH3)
 * - Les transitions en douceur (fade)
 * - L'extinction automatique après inactivité
 * - Le mode nuit
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "backlight_control.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// HANDLES EXTERNES
// ============================================================

/** @brief Handle Timer pour l'écran TFT */
extern TIM_HandleTypeDef htim9;

/** @brief Handle Timer pour le clavier */
extern TIM_HandleTypeDef htim4;

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du contrôleur */
static Backlight_State backlight_state;

/** @brief Configuration */
static Backlight_Config backlight_config = {
    .normalBrightness = BACKLIGHT_DEFAULT_BRIGHTNESS,
    .dimBrightness = BACKLIGHT_DIM_BRIGHTNESS,
    .nightBrightness = BACKLIGHT_NIGHT_BRIGHTNESS,
    .dimTimeoutS = BACKLIGHT_DIM_TIMEOUT_S,
    .offTimeoutS = BACKLIGHT_OFF_TIMEOUT_S,
    .keypadOffTimeoutS = BACKLIGHT_KEYPAD_OFF_TIMEOUT_S,
    .enableFade = true,
    .fadeDurationMs = BACKLIGHT_DEFAULT_FADE_MS,
    .enableNightMode = false,
    .nightModeStartHour = 22,
    .nightModeEndHour = 6,
    .enableAutoOff = true,
    .enableAutoDim = true,
    .keypadBacklightEnabled = true,
    .keypadBrightness = BACKLIGHT_DEFAULT_BRIGHTNESS,
    .keypadFollowTft = true
};

/** @brief Callback */
static Backlight_StateCallback state_callback = NULL;

/** @brief État précédent (pour détecter les changements) */
static BacklightMode previous_mode = BACKLIGHT_MODE_NORMAL;
static BacklightState previous_tft_state = BACKLIGHT_STATE_OFF;
static BacklightState previous_keypad_state = BACKLIGHT_STATE_OFF;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le contrôleur de rétroéclairage
 */
bool backlight_init(const Backlight_Config* config)
{
    BACKLIGHT_DEBUG("Initialisation du contrôleur de rétroéclairage...\n");
    
    if (config != NULL)
    {
        memcpy(&backlight_config, config, sizeof(Backlight_Config));
    }
    
    // Initialiser l'état
    memset(&backlight_state, 0, sizeof(Backlight_State));
    backlight_state.config = backlight_config;
    backlight_state.tftBrightness = 0;
    backlight_state.keypadBrightness = 0;
    backlight_state.tftState = BACKLIGHT_STATE_OFF;
    backlight_state.keypadState = BACKLIGHT_STATE_OFF;
    backlight_state.currentMode = BACKLIGHT_MODE_NORMAL;
    
    // --- Configuration GPIO pour l'écran TFT ---
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOE_CLK_ENABLE();
    
    // PE4 = TIM9_CH4 (backlight TFT)
    GPIO_InitStruct.Pin = BACKLIGHT_TFT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF3_TIM9;
    HAL_GPIO_Init(BACKLIGHT_TFT_PORT, &GPIO_InitStruct);
    
    // --- Configuration GPIO pour le clavier ---
    if (backlight_config.keypadBacklightEnabled)
    {
        __HAL_RCC_GPIOG_CLK_ENABLE();
        
        // PG13 = TIM4_CH3 (backlight clavier)
        GPIO_InitStruct.Pin = BACKLIGHT_KEYPAD_PIN;
        GPIO_InitStruct.Alternate = GPIO_AF2_TIM4;
        HAL_GPIO_Init(BACKLIGHT_KEYPAD_PORT, &GPIO_InitStruct);
    }
    
    // --- Configuration Timer pour l'écran (TIM9) ---
    __HAL_RCC_TIM9_CLK_ENABLE();
    
    htim9.Instance = BACKLIGHT_TFT_TIMER;
    htim9.Init.Prescaler = 0;  // Pas de prédiviseur pour l'instant
    htim9.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim9.Init.Period = BACKLIGHT_MAX_BRIGHTNESS;  // 0-255
    htim9.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim9.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    
    if (HAL_TIM_PWM_Init(&htim9) != HAL_OK)
    {
        BACKLIGHT_DEBUG("Échec initialisation TIM9\n");
        return false;
    }
    
    // Configurer le canal PWM
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;  // Démarrer éteint
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    
    if (HAL_TIM_PWM_ConfigChannel(&htim9, &sConfigOC, BACKLIGHT_TFT_CHANNEL) != HAL_OK)
    {
        BACKLIGHT_DEBUG("Échec configuration canal PWM TFT\n");
        return false;
    }
    
    // --- Configuration Timer pour le clavier (TIM4) ---
    if (backlight_config.keypadBacklightEnabled)
    {
        __HAL_RCC_TIM4_CLK_ENABLE();
        
        htim4.Instance = BACKLIGHT_KEYPAD_TIMER;
        htim4.Init.Prescaler = 0;
        htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
        htim4.Init.Period = BACKLIGHT_MAX_BRIGHTNESS;
        htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
        
        if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
        {
            BACKLIGHT_DEBUG("Échec initialisation TIM4\n");
            return false;
        }
        
        sConfigOC.Pulse = 0;
        
        if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, BACKLIGHT_KEYPAD_CHANNEL) != HAL_OK)
        {
            BACKLIGHT_DEBUG("Échec configuration canal PWM clavier\n");
            return false;
        }
    }
    
    // Démarrer les PWM
    HAL_TIM_PWM_Start(&htim9, BACKLIGHT_TFT_CHANNEL);
    
    if (backlight_config.keypadBacklightEnabled)
    {
        HAL_TIM_PWM_Start(&htim4, BACKLIGHT_KEYPAD_CHANNEL);
    }
    
    backlight_state.initialized = true;
    backlight_state.lastActivityTime = HAL_GetTick();
    
    // Allumer au niveau normal
    backlight_set_brightness(BACKLIGHT_TARGET_TFT, backlight_config.normalBrightness);
    
    if (backlight_config.keypadBacklightEnabled)
    {
        backlight_set_brightness(BACKLIGHT_TARGET_KEYPAD, backlight_config.keypadBrightness);
    }
    
    BACKLIGHT_DEBUG("Contrôleur initialisé (TFT=%d, Keypad=%d)\n",
                   backlight_config.normalBrightness,
                   backlight_config.keypadBrightness);
    
    return true;
}

/**
 * @brief Désinitialise
 */
void backlight_deinit(void)
{
    HAL_TIM_PWM_Stop(&htim9, BACKLIGHT_TFT_CHANNEL);
    
    if (backlight_config.keypadBacklightEnabled)
    {
        HAL_TIM_PWM_Stop(&htim4, BACKLIGHT_KEYPAD_CHANNEL);
    }
    
    backlight_state.initialized = false;
}

/**
 * @brief Vérifie si prêt
 */
bool backlight_is_ready(void)
{
    return backlight_state.initialized;
}

/**
 * @brief Récupère l'état
 */
Backlight_State* backlight_get_state(void)
{
    return &backlight_state;
}

// ============================================================
// SECTION 2 : CONTRÔLE
// ============================================================

/**
 * @brief Allume le rétroéclairage
 */
void backlight_on(BacklightTarget target)
{
    if (target & BACKLIGHT_TARGET_TFT)
    {
        backlight_set_brightness(BACKLIGHT_TARGET_TFT, backlight_config.normalBrightness);
        backlight_state.tftState = BACKLIGHT_STATE_ON;
    }
    
    if (target & BACKLIGHT_TARGET_KEYPAD)
    {
        backlight_set_brightness(BACKLIGHT_TARGET_KEYPAD, backlight_config.keypadBrightness);
        backlight_state.keypadState = BACKLIGHT_STATE_ON;
    }
}

/**
 * @brief Éteint le rétroéclairage
 */
void backlight_off(BacklightTarget target)
{
    if (target & BACKLIGHT_TARGET_TFT)
    {
        backlight_set_brightness(BACKLIGHT_TARGET_TFT, 0);
        backlight_state.tftState = BACKLIGHT_STATE_OFF;
    }
    
    if (target & BACKLIGHT_TARGET_KEYPAD)
    {
        backlight_set_brightness(BACKLIGHT_TARGET_KEYPAD, 0);
        backlight_state.keypadState = BACKLIGHT_STATE_OFF;
    }
}

/**
 * @brief Bascule l'état
 */
void backlight_toggle(BacklightTarget target)
{
    if (target & BACKLIGHT_TARGET_TFT)
    {
        if (backlight_state.tftBrightness > 0)
            backlight_off(BACKLIGHT_TARGET_TFT);
        else
            backlight_on(BACKLIGHT_TARGET_TFT);
    }
    
    if (target & BACKLIGHT_TARGET_KEYPAD)
    {
        if (backlight_state.keypadBrightness > 0)
            backlight_off(BACKLIGHT_TARGET_KEYPAD);
        else
            backlight_on(BACKLIGHT_TARGET_KEYPAD);
    }
}

/**
 * @brief Définit la luminosité
 */
void backlight_set_brightness(BacklightTarget target, uint8_t brightness)
{
    if (!backlight_state.initialized) return;
    
    if (brightness > BACKLIGHT_MAX_BRIGHTNESS) brightness = BACKLIGHT_MAX_BRIGHTNESS;
    
    if (target & BACKLIGHT_TARGET_TFT)
    {
        __HAL_TIM_SET_COMPARE(&htim9, BACKLIGHT_TFT_CHANNEL, brightness);
        backlight_state.tftBrightness = brightness;
    }
    
    if (target & BACKLIGHT_TARGET_KEYPAD)
    {
        if (backlight_config.keypadBacklightEnabled)
        {
            __HAL_TIM_SET_COMPARE(&htim4, BACKLIGHT_KEYPAD_CHANNEL, brightness);
            backlight_state.keypadBrightness = brightness;
        }
    }
}

/**
 * @brief Récupère la luminosité
 */
uint8_t backlight_get_brightness(BacklightTarget target)
{
    if (target == BACKLIGHT_TARGET_TFT)
        return backlight_state.tftBrightness;
    else if (target == BACKLIGHT_TARGET_KEYPAD)
        return backlight_state.keypadBrightness;
    else
        return 0;
}

/**
 * @brief Définit le mode
 */
void backlight_set_mode(BacklightMode mode)
{
    previous_mode = backlight_state.currentMode;
    backlight_state.currentMode = mode;
    
    switch (mode)
    {
        case BACKLIGHT_MODE_NORMAL:
            backlight_set_brightness(BACKLIGHT_TARGET_TFT, backlight_config.normalBrightness);
            backlight_state.tftState = BACKLIGHT_STATE_ON;
            break;
            
        case BACKLIGHT_MODE_DIM:
            backlight_set_brightness(BACKLIGHT_TARGET_TFT, backlight_config.dimBrightness);
            backlight_state.tftState = BACKLIGHT_STATE_DIM;
            break;
            
        case BACKLIGHT_MODE_NIGHT:
            backlight_set_brightness(BACKLIGHT_TARGET_TFT, backlight_config.nightBrightness);
            backlight_state.tftState = BACKLIGHT_STATE_DIM;
            break;
            
        case BACKLIGHT_MODE_OFF:
            backlight_set_brightness(BACKLIGHT_TARGET_TFT, 0);
            backlight_state.tftState = BACKLIGHT_STATE_OFF;
            break;
            
        case BACKLIGHT_MODE_MAX:
            backlight_set_brightness(BACKLIGHT_TARGET_TFT, BACKLIGHT_MAX_BRIGHTNESS);
            backlight_state.tftState = BACKLIGHT_STATE_ON;
            break;
    }
    
    if (backlight_config.keypadFollowTft)
    {
        backlight_set_brightness(BACKLIGHT_TARGET_KEYPAD, backlight_state.tftBrightness);
        backlight_state.keypadState = backlight_state.tftState;
    }
    
    BACKLIGHT_DEBUG("Mode: %d (TFT=%d)\n", mode, backlight_state.tftBrightness);
}

/**
 * @brief Récupère le mode
 */
BacklightMode backlight_get_mode(void)
{
    return backlight_state.currentMode;
}

// ============================================================
// SECTION 3 : FONDU (FADE)
// ============================================================

/**
 * @brief Fondu vers une luminosité cible
 */
void backlight_fade_to(uint8_t targetBrightness, uint16_t durationMs)
{
    if (!backlight_config.enableFade) return;
    
    backlight_state.fading = true;
    backlight_state.fadeFrom = backlight_state.tftBrightness;
    backlight_state.fadeTo = targetBrightness;
    backlight_state.fadeStartTime = HAL_GetTick();
    backlight_state.fadeDurationMs = durationMs;
    
    BACKLIGHT_DEBUG("Fade: %d → %d (%d ms)\n", 
                   backlight_state.fadeFrom, backlight_state.fadeTo, durationMs);
}

/**
 * @brief Fondu entrant
 */
void backlight_fade_in(uint16_t durationMs)
{
    backlight_fade_to(backlight_config.normalBrightness, durationMs);
}

/**
 * @brief Fondu sortant
 */
void backlight_fade_out(uint16_t durationMs)
{
    backlight_fade_to(0, durationMs);
}

/**
 * @brief Vérifie si un fondu est en cours
 */
bool backlight_is_fading(void)
{
    return backlight_state.fading;
}

/**
 * @brief Traite le fondu en cours
 */
static void backlight_process_fade(void)
{
    if (!backlight_state.fading) return;
    
    uint32_t elapsed = HAL_GetTick() - backlight_state.fadeStartTime;
    
    if (elapsed >= backlight_state.fadeDurationMs)
    {
        // Fondu terminé
        backlight_set_brightness(BACKLIGHT_TARGET_TFT, backlight_state.fadeTo);
        backlight_state.fading = false;
        backlight_state.tftState = (backlight_state.fadeTo > 0) ? 
                                    BACKLIGHT_STATE_ON : BACKLIGHT_STATE_OFF;
        
        if (backlight_config.keypadFollowTft)
        {
            backlight_set_brightness(BACKLIGHT_TARGET_KEYPAD, backlight_state.fadeTo);
            backlight_state.keypadState = backlight_state.tftState;
        }
    }
    else
    {
        // Interpolation linéaire
        float progress = (float)elapsed / backlight_state.fadeDurationMs;
        uint8_t currentBrightness = (uint8_t)(backlight_state.fadeFrom + 
                                     (backlight_state.fadeTo - backlight_state.fadeFrom) * progress);
        
        backlight_set_brightness(BACKLIGHT_TARGET_TFT, currentBrightness);
    }
}

// ============================================================
// SECTION 4 : GESTION D'ACTIVITÉ
// ============================================================

/**
 * @brief Signale une activité utilisateur
 */
void backlight_activity(void)
{
    backlight_state.lastActivityTime = HAL_GetTick();
    
    // Si l'écran était éteint, le rallumer
    if (backlight_state.tftState == BACKLIGHT_STATE_OFF)
    {
        if (backlight_config.enableFade)
        {
            backlight_fade_in(backlight_config.fadeDurationMs);
        }
        else
        {
            backlight_set_mode(BACKLIGHT_MODE_NORMAL);
        }
    }
    else if (backlight_state.tftState == BACKLIGHT_STATE_DIM)
    {
        // Revenir au mode normal
        backlight_set_mode(BACKLIGHT_MODE_NORMAL);
    }
    
    // Réactiver le clavier si nécessaire
    if (backlight_state.keypadState == BACKLIGHT_STATE_OFF && 
        backlight_config.keypadBacklightEnabled)
    {
        backlight_set_brightness(BACKLIGHT_TARGET_KEYPAD, backlight_config.keypadBrightness);
        backlight_state.keypadState = BACKLIGHT_STATE_ON;
    }
}

/**
 * @brief Signale une activité clavier
 */
void backlight_keypad_activity(void)
{
    backlight_state.lastKeypadActivity = HAL_GetTick();
    
    // Si le clavier était éteint, le rallumer
    if (backlight_state.keypadState == BACKLIGHT_STATE_OFF && 
        backlight_config.keypadBacklightEnabled)
    {
        backlight_set_brightness(BACKLIGHT_TARGET_KEYPAD, backlight_config.keypadBrightness);
        backlight_state.keypadState = BACKLIGHT_STATE_ON;
    }
}

/**
 * @brief Traitement périodique
 */
void backlight_process(void)
{
    if (!backlight_state.initialized) return;
    
    // Traiter le fondu en cours
    backlight_process_fade();
    
    if (!backlight_config.enableAutoOff && !backlight_config.enableAutoDim) return;
    
    uint32_t now = HAL_GetTick();
    uint32_t idleTime = (now - backlight_state.lastActivityTime) / 1000;
    uint32_t keypadIdleTime = (now - backlight_state.lastKeypadActivity) / 1000;
    
    // Vérifier le mode nuit
    if (backlight_config.enableNightMode && backlight_is_night_mode())
    {
        if (backlight_state.currentMode != BACKLIGHT_MODE_NIGHT)
        {
            backlight_set_mode(BACKLIGHT_MODE_NIGHT);
        }
        return;  // Ne pas appliquer les autres timeouts en mode nuit
    }
    
    // Réduction automatique (dim)
    if (backlight_config.enableAutoDim && 
        backlight_state.tftState == BACKLIGHT_STATE_ON &&
        idleTime >= backlight_config.dimTimeoutS)
    {
        BACKLIGHT_DEBUG("Dim auto après %lu s\n", (unsigned long)idleTime);
        backlight_set_mode(BACKLIGHT_MODE_DIM);
        backlight_state.totalDimTime += idleTime;
    }
    
    // Extinction automatique écran
    if (backlight_config.enableAutoOff &&
        backlight_state.tftState != BACKLIGHT_STATE_OFF &&
        idleTime >= backlight_config.offTimeoutS)
    {
        BACKLIGHT_DEBUG("Off auto après %lu s\n", (unsigned long)idleTime);
        
        if (backlight_config.enableFade)
        {
            backlight_fade_out(backlight_config.fadeDurationMs);
        }
        else
        {
            backlight_set_mode(BACKLIGHT_MODE_OFF);
        }
        backlight_state.totalOffTime += idleTime;
    }
    
    // Extinction automatique clavier
    if (backlight_config.keypadBacklightEnabled &&
        backlight_state.keypadState != BACKLIGHT_STATE_OFF &&
        keypadIdleTime >= backlight_config.keypadOffTimeoutS)
    {
        backlight_set_brightness(BACKLIGHT_TARGET_KEYPAD, 0);
        backlight_state.keypadState = BACKLIGHT_STATE_OFF;
    }
}

// ============================================================
// SECTION 5 : MODE NUIT
// ============================================================

void backlight_night_mode_enable(bool enable)
{
    backlight_config.enableNightMode = enable;
}

bool backlight_is_night_mode(void)
{
    // Simuler l'heure (à remplacer par RTC)
    uint8_t currentHour = 12;  // TODO: Récupérer l'heure réelle
    
    if (backlight_config.nightModeStartHour < backlight_config.nightModeEndHour)
    {
        return (currentHour >= backlight_config.nightModeStartHour && 
                currentHour < backlight_config.nightModeEndHour);
    }
    else
    {
        // Période à cheval sur minuit (ex: 22h-6h)
        return (currentHour >= backlight_config.nightModeStartHour || 
                currentHour < backlight_config.nightModeEndHour);
    }
}

void backlight_set_night_mode_hours(uint8_t startHour, uint8_t endHour)
{
    backlight_config.nightModeStartHour = startHour;
    backlight_config.nightModeEndHour = endHour;
}

// ============================================================
// SECTION 6 : CALLBACKS
// ============================================================

void backlight_set_state_callback(Backlight_StateCallback callback)
{
    state_callback = callback;
}

// ============================================================
// SECTION 7 : DÉBOGAGE
// ============================================================

void backlight_print_state(void)
{
    printf("\n═══ ÉTAT RÉTROÉCLAIRAGE ═══\n");
    printf("TFT          : %d/255 (%s)\n", 
           backlight_state.tftBrightness,
           backlight_state.tftState == BACKLIGHT_STATE_ON ? "ON" :
           backlight_state.tftState == BACKLIGHT_STATE_DIM ? "DIM" : "OFF");
    printf("Clavier      : %d/255 (%s)\n",
           backlight_state.keypadBrightness,
           backlight_state.keypadState == BACKLIGHT_STATE_ON ? "ON" : "OFF");
    printf("Mode         : %s\n",
           backlight_state.currentMode == BACKLIGHT_MODE_NORMAL ? "Normal" :
           backlight_state.currentMode == BACKLIGHT_MODE_DIM ? "Dim" :
           backlight_state.currentMode == BACKLIGHT_MODE_NIGHT ? "Nuit" : "Off");
    printf("Fondu        : %s\n", backlight_state.fading ? "En cours" : "Non");
    printf("Mode nuit    : %s (%dh-%dh)\n", 
           backlight_config.enableNightMode ? "ON" : "OFF",
           backlight_config.nightModeStartHour,
           backlight_config.nightModeEndHour);
    printf("Idle TFT     : %lu s\n", 
           (unsigned long)((HAL_GetTick() - backlight_state.lastActivityTime) / 1000));
    printf("══════════════════════════\n\n");
}

void backlight_print_config(void)
{
    printf("\n═══ CONFIG RÉTROÉCLAIRAGE ═══\n");
    printf("Normal       : %d/255\n", backlight_config.normalBrightness);
    printf("Dim          : %d/255\n", backlight_config.dimBrightness);
    printf("Nuit         : %d/255\n", backlight_config.nightBrightness);
    printf("Timeout dim  : %d s\n", backlight_config.dimTimeoutS);
    printf("Timeout off  : %d s\n", backlight_config.offTimeoutS);
    printf("Fondu        : %s (%d ms)\n", 
           backlight_config.enableFade ? "ON" : "OFF",
           backlight_config.fadeDurationMs);
    printf("════════════════════════════\n\n");
}

bool backlight_self_test(void)
{
    BACKLIGHT_DEBUG("Auto-test...\n");
    
    if (!backlight_state.initialized)
    {
        BACKLIGHT_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : allumer/éteindre l'écran
    backlight_set_brightness(BACKLIGHT_TARGET_TFT, 100);
    HAL_Delay(200);
    
    if (backlight_state.tftBrightness != 100)
    {
        BACKLIGHT_DEBUG("Échec : luminosité incorrecte\n");
        return false;
    }
    
    backlight_set_brightness(BACKLIGHT_TARGET_TFT, 0);
    HAL_Delay(200);
    
    // Restaurer
    backlight_set_brightness(BACKLIGHT_TARGET_TFT, backlight_config.normalBrightness);
    
    // Test fondu
    backlight_fade_out(200);
    HAL_Delay(300);
    backlight_fade_in(200);
    HAL_Delay(300);
    
    BACKLIGHT_DEBUG("Auto-test OK\n");
    return true;
}