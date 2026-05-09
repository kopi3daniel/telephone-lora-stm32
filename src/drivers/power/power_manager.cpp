/**
 * @file power_manager.cpp
 * @brief Implémentation du gestionnaire d'alimentation
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans power_manager.h.
 * 
 * Il gère :
 * - Les modes de veille (SLEEP, STOP, STANDBY)
 * - La surveillance de la batterie
 * - Les timeouts d'inactivité
 * - Le réveil par interruptions
 * - L'extinction automatique
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "power_manager.h"
#include "battery_monitor.h"
#include "backlight_control.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du gestionnaire */
static PowerManager_State power_state;

/** @brief Configuration */
static PowerManager_Config power_config = {
    .sleepTimeoutS = POWER_SLEEP_TIMEOUT_S,
    .stopTimeoutS = POWER_STOP_TIMEOUT_S,
    .standbyTimeoutS = POWER_STANDBY_TIMEOUT_S,
    .backlightTimeoutS = POWER_BACKLIGHT_TIMEOUT_S,
    .batteryFullMv = POWER_BATTERY_FULL_MV,
    .batteryEmptyMv = POWER_BATTERY_EMPTY_MV,
    .batteryLowPercent = POWER_BATTERY_LOW_THRESHOLD,
    .batteryCriticalPercent = POWER_BATTERY_CRITICAL_THRESHOLD,
    .wakeupSources = POWER_WAKEUP_BUTTON | POWER_WAKEUP_KEYPAD | POWER_WAKEUP_LORA,
    .wakeupOnLora = true,
    .wakeupOnKeypad = true,
    .backlightAutoOff = true,
    .backlightDimLevel = 30,
    .statusLedEnabled = true,
    .statusLedBlinkOnLow = true
};

/** @brief Callbacks */
static PowerManager_BatteryCallback battery_cb = NULL;
static PowerManager_LowBatteryCallback low_battery_cb = NULL;
static PowerManager_ModeCallback mode_cb = NULL;
static PowerManager_WakeupCallback wakeup_cb = NULL;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le gestionnaire d'alimentation
 */
bool power_manager_init(const PowerManager_Config* config)
{
    POWER_DEBUG("Initialisation du gestionnaire d'alimentation...\n");
    
    if (config != NULL)
    {
        memcpy(&power_config, config, sizeof(PowerManager_Config));
    }
    
    memset(&power_state, 0, sizeof(PowerManager_State));
    power_state.config = power_config;
    power_state.currentMode = POWER_MODE_RUN;
    power_state.lastActivityTime = HAL_GetTick();
    power_state.lastBacklightTime = HAL_GetTick();
    
    // Initialiser le moniteur de batterie
    battery_monitor_init(NULL);
    
    // Configurer les broches de wake-up
    configure_wakeup_pins();
    
    // Configurer le RTC pour le réveil périodique (si disponible)
    configure_rtc_wakeup();
    
    power_state.initialized = true;
    
    POWER_DEBUG("Gestionnaire initialisé\n");
    POWER_DEBUG("Timeouts : SLEEP=%ds, STOP=%ds, STANDBY=%ds\n",
               power_config.sleepTimeoutS,
               power_config.stopTimeoutS,
               power_config.standbyTimeoutS);
    
    return true;
}

/**
 * @brief Configure les broches de réveil
 */
static void configure_wakeup_pins(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Bouton ON/OFF (PA0) - Réveil par EXTI0
    if (power_config.wakeupSources & POWER_WAKEUP_BUTTON)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        
        GPIO_InitStruct.Pin = GPIO_PIN_0;
        GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        
        HAL_NVIC_SetPriority(EXTI0_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    }
    
    // Clavier - Réveil par EXTI (n'importe quelle touche)
    if (power_config.wakeupSources & POWER_WAKEUP_KEYPAD)
    {
        // Les interruptions clavier sont déjà configurées dans keypad_matrix
    }
    
    // LoRa DIO0 - Réveil sur paquet reçu
    if (power_config.wakeupSources & POWER_WAKEUP_LORA)
    {
        // L'interruption LoRa DIO0 est déjà configurée
    }
}

/**
 * @brief Configure le réveil RTC
 */
static void configure_rtc_wakeup(void)
{
    // Le RTC peut être utilisé pour un réveil périodique
    // (vérification de la batterie, beacon de présence, etc.)
    // Pour l'instant, on utilise les timeouts logiciels
}

void power_manager_deinit(void)
{
    power_state.initialized = false;
}

bool power_manager_is_ready(void)
{
    return power_state.initialized;
}

PowerManager_State* power_manager_get_state(void)
{
    return &power_state;
}

// ============================================================
// SECTION 2 : GESTION D'ÉNERGIE
// ============================================================

/**
 * @brief Traitement périodique
 */
void power_manager_process(void)
{
    if (!power_state.initialized) return;
    
    uint32_t now = HAL_GetTick();
    
    // Vérifier la batterie périodiquement
    battery_monitor_process();
    
    // Mettre à jour le temps de fonctionnement
    if (power_state.currentMode == POWER_MODE_RUN)
    {
        power_state.totalRunTime++;
    }
    else
    {
        power_state.totalSleepTime++;
    }
    
    // Vérifier les timeouts d'inactivité
    uint32_t idleTime = (now - power_state.lastActivityTime) / 1000;
    
    // Timeout SLEEP
    if (power_config.sleepTimeoutS > 0 &&
        power_state.currentMode == POWER_MODE_RUN &&
        idleTime >= power_config.sleepTimeoutS)
    {
        POWER_DEBUG("Inactivité %lu s → SLEEP\n", (unsigned long)idleTime);
        power_manager_enter_mode(POWER_MODE_SLEEP);
    }
    
    // Timeout STOP
    if (power_config.stopTimeoutS > 0 &&
        power_state.currentMode == POWER_MODE_SLEEP &&
        idleTime >= power_config.stopTimeoutS)
    {
        POWER_DEBUG("Inactivité %lu s → STOP\n", (unsigned long)idleTime);
        power_manager_enter_mode(POWER_MODE_STOP);
    }
    
    // Timeout STANDBY
    if (power_config.standbyTimeoutS > 0 &&
        power_state.currentMode == POWER_MODE_STOP &&
        idleTime >= power_config.standbyTimeoutS)
    {
        POWER_DEBUG("Inactivité %lu s → STANDBY\n", (unsigned long)idleTime);
        power_manager_enter_mode(POWER_MODE_STANDBY);
    }
    
    // Vérifier la batterie critique → STANDBY forcé
    if (power_state.batteryCritical && power_state.currentMode != POWER_MODE_STANDBY)
    {
        POWER_DEBUG("Batterie critique → STANDBY forcé\n");
        power_manager_enter_mode(POWER_MODE_STANDBY);
    }
}

/**
 * @brief Signale une activité utilisateur
 */
void power_manager_activity(void)
{
    power_state.lastActivityTime = HAL_GetTick();
    
    // Réveiller si en veille
    if (power_state.currentMode >= POWER_MODE_SLEEP)
    {
        power_manager_wakeup(POWER_WAKEUP_KEYPAD);
    }
    
    // Réactiver le rétroéclairage
    if (power_config.backlightAutoOff)
    {
        backlight_activity();
    }
}

/**
 * @brief Signale une activité LoRa
 */
void power_manager_lora_activity(void)
{
    power_state.lastLoraActivity = HAL_GetTick();
    
    // Réveiller si en veille et configuré pour le réveil LoRa
    if (power_config.wakeupOnLora && power_state.currentMode >= POWER_MODE_SLEEP)
    {
        power_manager_wakeup(POWER_WAKEUP_LORA);
    }
}

/**
 * @brief Force le passage en mode veille
 */
void power_manager_enter_mode(PowerMode mode)
{
    if (!power_state.initialized) return;
    if (mode == power_state.currentMode) return;
    
    PowerMode oldMode = power_state.currentMode;
    
    POWER_DEBUG("Changement mode : %d → %d\n", oldMode, mode);
    
    switch (mode)
    {
        case POWER_MODE_RUN:
            // Déjà en RUN
            break;
            
        case POWER_MODE_SLEEP:
            // SLEEP : CPU stoppé, périphériques actifs
            backlight_set_mode(BACKLIGHT_MODE_DIM);
            __WFI();  // Wait For Interrupt
            break;
            
        case POWER_MODE_STOP:
            // STOP : horloges stoppées
            backlight_set_mode(BACKLIGHT_MODE_OFF);
            
            // Configurer les sources de réveil
            HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);  // PA0 (bouton)
            
            // Entrer en mode STOP
            HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
            
            // Au réveil, reconfigurer les horloges
            SystemClock_Config();
            break;
            
        case POWER_MODE_STANDBY:
            // STANDBY : veille profonde
            backlight_set_mode(BACKLIGHT_MODE_OFF);
            display_off();
            
            // Sauvegarder les paramètres importants
            settings_nvram_save_all();
            
            // Activer le wake-up pin
            HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);
            
            // Effacer le flag de wake-up
            __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
            
            // Entrer en STANDBY
            HAL_PWR_EnterSTANDBYMode();
            // Le CPU redémarre d'ici après un wake-up
            break;
            
        case POWER_MODE_SHUTDOWN:
            // Extinction complète
            backlight_set_mode(BACKLIGHT_MODE_OFF);
            display_off();
            audio_manager_deinit();
            lora_driver_sleep();
            
            // Sauvegarder tout
            settings_nvram_save_all();
            
            // Entrer en STANDBY (pas de retour sans appui bouton)
            HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);
            __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
            HAL_PWR_EnterSTANDBYMode();
            break;
    }
    
    power_state.currentMode = mode;
    
    if (mode_cb)
    {
        mode_cb(oldMode, mode);
    }
}

/**
 * @brief Réveille du mode veille
 */
void power_manager_wakeup(PowerWakeupSource source)
{
    if (power_state.currentMode == POWER_MODE_RUN) return;
    
    POWER_DEBUG("Réveil par source %d\n", source);
    
    // Restaurer le mode RUN
    PowerMode oldMode = power_state.currentMode;
    power_state.currentMode = POWER_MODE_RUN;
    power_state.lastWakeupSource = source;
    power_state.wakeupCount++;
    power_state.lastActivityTime = HAL_GetTick();
    
    // Réactiver les périphériques
    backlight_set_mode(BACKLIGHT_MODE_NORMAL);
    
    if (wakeup_cb)
    {
        wakeup_cb(source);
    }
    
    if (mode_cb)
    {
        mode_cb(oldMode, POWER_MODE_RUN);
    }
}

/**
 * @brief Vérifie si le réveil est autorisé
 */
bool power_manager_can_wakeup(PowerWakeupSource source)
{
    return (power_config.wakeupSources & source) != 0;
}

// ============================================================
// SECTION 3 : BATTERIE
// ============================================================

uint16_t power_manager_get_battery_voltage(void)
{
    return battery_monitor_get_voltage();
}

uint8_t power_manager_get_battery_percent(void)
{
    return battery_monitor_get_percent();
}

BatteryState power_manager_get_battery_state(void)
{
    return battery_monitor_get_state();
}

bool power_manager_is_battery_low(void)
{
    return battery_monitor_is_low();
}

bool power_manager_is_battery_critical(void)
{
    return battery_monitor_is_critical();
}

bool power_manager_is_charging(void)
{
    return battery_monitor_is_charging();
}

void power_manager_check_battery(void)
{
    battery_monitor_measure_now();
    
    power_state.batteryVoltageMv = battery_monitor_get_voltage();
    power_state.batteryPercent = battery_monitor_get_percent();
    power_state.batteryState = battery_monitor_get_state();
    power_state.batteryLow = battery_monitor_is_low();
    power_state.batteryCritical = battery_monitor_is_critical();
    
    // Notifier les callbacks
    if (battery_cb)
    {
        battery_cb(power_state.batteryPercent, power_state.batteryState);
    }
    
    if (power_state.batteryLow && low_battery_cb)
    {
        low_battery_cb();
    }
    
    if (power_state.batteryCritical)
    {
        POWER_DEBUG("BATTERIE CRITIQUE : %d%%\n", power_state.batteryPercent);
    }
}

// ============================================================
// SECTION 4 : RÉTROÉCLAIRAGE
// ============================================================

void power_manager_backlight_on(void)  { backlight_on(BACKLIGHT_TARGET_ALL); }
void power_manager_backlight_off(void) { backlight_off(BACKLIGHT_TARGET_ALL); }
void power_manager_backlight_dim(void) { backlight_set_mode(BACKLIGHT_MODE_DIM); }
void power_manager_backlight_activity(void) { backlight_activity(); }

// ============================================================
// SECTION 5 : CALLBACKS
// ============================================================

void power_manager_set_battery_callback(PowerManager_BatteryCallback cb) { battery_cb = cb; }
void power_manager_set_low_battery_callback(PowerManager_LowBatteryCallback cb) { low_battery_cb = cb; }
void power_manager_set_mode_callback(PowerManager_ModeCallback cb) { mode_cb = cb; }
void power_manager_set_wakeup_callback(PowerManager_WakeupCallback cb) { wakeup_cb = cb; }

// ============================================================
// SECTION 6 : DÉBOGAGE
// ============================================================

void power_manager_print_state(void)
{
    const char* modeStr = "INCONNU";
    switch (power_state.currentMode)
    {
        case POWER_MODE_RUN:     modeStr = "RUN"; break;
        case POWER_MODE_SLEEP:   modeStr = "SLEEP"; break;
        case POWER_MODE_STOP:    modeStr = "STOP"; break;
        case POWER_MODE_STANDBY: modeStr = "STANDBY"; break;
        default: break;
    }
    
    uint32_t idleTime = (HAL_GetTick() - power_state.lastActivityTime) / 1000;
    
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║     ÉTAT GESTIONNAIRE ALIMENTATION            ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Mode         : %-31s ║\n", modeStr);
    printf("║ Batterie     : %d%% (%d mV)                    ║\n", 
           power_state.batteryPercent, power_state.batteryVoltageMv);
    printf("║ État batterie: %-31d ║\n", power_state.batteryState);
    printf("║ Inactivité   : %lu s                            ║\n", (unsigned long)idleTime);
    printf("║ Réveils      : %lu                               ║\n", (unsigned long)power_state.wakeupCount);
    printf("║ Temps RUN    : %lu s                            ║\n", (unsigned long)power_state.totalRunTime);
    printf("║ Temps veille : %lu s                            ║\n", (unsigned long)power_state.totalSleepTime);
    printf("╚══════════════════════════════════════════════╝\n\n");
}

void power_manager_print_battery(void)
{
    battery_monitor_print_voltage();
}

void power_manager_print_statistics(void)
{
    printf("\n═══ STATISTIQUES ALIMENTATION ═══\n");
    printf("Mode actuel       : %d\n", power_state.currentMode);
    printf("Temps RUN         : %lu s\n", (unsigned long)power_state.totalRunTime);
    printf("Temps veille      : %lu s\n", (unsigned long)power_state.totalSleepTime);
    printf("Réveils           : %lu\n", (unsigned long)power_state.wakeupCount);
    printf("Batterie          : %d%%\n", power_state.batteryPercent);
    printf("══════════════════════════════════\n\n");
}

bool power_manager_self_test(void)
{
    POWER_DEBUG("Auto-test...\n");
    
    if (!power_state.initialized)
    {
        POWER_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Vérifier la batterie
    uint8_t percent = power_manager_get_battery_percent();
    if (percent > 100)
    {
        POWER_DEBUG("Échec : pourcentage batterie invalide\n");
        return false;
    }
    
    POWER_DEBUG("Auto-test OK\n");
    return true;
}

// ============================================================
// SECTION 7 : HANDLER INTERRUPTION WAKE-UP
// ============================================================

/**
 * @brief Handler EXTI0 (bouton ON/OFF)
 */
void EXTI0_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_0) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
        
        // Réveiller le système
        if (power_manager_can_wakeup(POWER_WAKEUP_BUTTON))
        {
            power_manager_wakeup(POWER_WAKEUP_BUTTON);
        }
    }
    
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}