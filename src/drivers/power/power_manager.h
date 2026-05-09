/**
 * @file power_manager.h
 * @brief Gestionnaire d'alimentation et modes de veille
 * 
 * Ce fichier gère l'alimentation du téléphone LoRa :
 * - Surveillance de la batterie (niveau, tension)
 * - Modes de veille (Sleep, Stop, Standby)
 * - Réveil par interruptions (clavier, LoRa, appel)
 * - Gestion du rétroéclairage
 * - Optimisation de la consommation
 * 
 * Modes de consommation :
 * ┌──────────────┬──────────┬─────────────────────────┐
 * │ MODE         │ COURANT  │ RÉVEIL PAR              │
 * ├──────────────┼──────────┼─────────────────────────┤
 * │ RUN (actif)  │ ~120 mA  │ -                       │
 * │ SLEEP        │ ~50 mA   │ Tout événement          │
 * │ STOP         │ ~1 mA    │ EXTI, RTC, USB          │
 * │ STANDBY      │ ~2.5 µA  │ Bouton ON/OFF, RTC      │
 * └──────────────┴──────────┴─────────────────────────┘
 * 
 * Stratégie d'économie d'énergie :
 * - Inactivité 30s → Rétroéclairage réduit
 * - Inactivité 5min → Écran éteint, CPU en SLEEP
 * - Inactivité 30min → Mode STOP (réveil par LoRa ou clavier)
 * - Batterie < 5% → Mode STANDBY (réveil par bouton uniquement)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

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
#define POWER_MANAGER_VERSION           "1.0.0"

/** @brief Tension de référence ADC (mV) */
#define POWER_ADC_REF_VOLTAGE           3300

/** @brief Résolution ADC */
#define POWER_ADC_RESOLUTION            4096

/** @brief Tension batterie pleine (mV) - Li-Po 4.2V */
#define POWER_BATTERY_FULL_MV           4200

/** @brief Tension batterie vide (mV) - Li-Po 3.3V */
#define POWER_BATTERY_EMPTY_MV          3300

/** @brief Tension batterie critique (mV) */
#define POWER_BATTERY_CRITICAL_MV       3400

/** @brief Seuil batterie faible (%) */
#define POWER_BATTERY_LOW_THRESHOLD     15

/** @brief Seuil batterie critique (%) */
#define POWER_BATTERY_CRITICAL_THRESHOLD 5

/** @brief Canal ADC pour la batterie */
#define POWER_BATTERY_ADC_CHANNEL       ADC_CHANNEL_8

/** @brief Timeouts de veille (secondes) */
#define POWER_SLEEP_TIMEOUT_S           30      // Mode SLEEP
#define POWER_STOP_TIMEOUT_S            300     // Mode STOP (5 minutes)
#define POWER_STANDBY_TIMEOUT_S         1800    // Mode STANDBY (30 minutes)

/** @brief Timeout rétroéclairage (secondes) */
#define POWER_BACKLIGHT_TIMEOUT_S       10

// ============================================================
// SECTION 2 : MODES D'ALIMENTATION
// ============================================================

/**
 * @brief Modes de fonctionnement
 */
typedef enum {
    POWER_MODE_RUN      = 0,    // Mode normal (tout actif)
    POWER_MODE_SLEEP    = 1,    // Sommeil léger (CPU stoppé, périph. actifs)
    POWER_MODE_STOP     = 2,    // Stop (horloges stoppées, réveil EXTI/RTC)
    POWER_MODE_STANDBY  = 3,    // Veille profonde (seul le bouton ON/OFF réveille)
    POWER_MODE_SHUTDOWN = 4     // Extinction complète
} PowerMode;

/**
 * @brief Sources de réveil
 */
typedef enum {
    POWER_WAKEUP_NONE       = 0,
    POWER_WAKEUP_BUTTON     = (1 << 0),   // Bouton ON/OFF
    POWER_WAKEUP_KEYPAD     = (1 << 1),   // Clavier
    POWER_WAKEUP_LORA       = (1 << 2),   // Paquet LoRa reçu
    POWER_WAKEUP_RTC        = (1 << 3),   // Alarme RTC
    POWER_WAKEUP_USB        = (1 << 4),   // Connexion USB
    POWER_WAKEUP_CHARGER    = (1 << 5),   // Chargeur connecté
    POWER_WAKEUP_TOUCH      = (1 << 6)    // Écran tactile
} PowerWakeupSource;

/**
 * @brief État de la batterie
 */
typedef enum {
    BATTERY_STATE_UNKNOWN   = 0,
    BATTERY_STATE_CHARGING  = 1,
    BATTERY_STATE_DISCHARGING = 2,
    BATTERY_STATE_FULL      = 3,
    BATTERY_STATE_LOW       = 4,
    BATTERY_STATE_CRITICAL  = 5
} BatteryState;

// ============================================================
// SECTION 3 : CONFIGURATION
// ============================================================

/**
 * @brief Configuration du gestionnaire d'alimentation
 */
typedef struct {
    // Timeouts (secondes, 0 = désactivé)
    uint16_t sleepTimeoutS;         // Avant SLEEP
    uint16_t stopTimeoutS;          // Avant STOP
    uint16_t standbyTimeoutS;       // Avant STANDBY
    uint16_t backlightTimeoutS;     // Rétroéclairage
    
    // Batterie
    uint16_t batteryFullMv;         // Tension pleine
    uint16_t batteryEmptyMv;        // Tension vide
    uint8_t batteryLowPercent;      // Seuil faible
    uint8_t batteryCriticalPercent; // Seuil critique
    
    // Wake-up
    uint32_t wakeupSources;         // Sources de réveil autorisées
    bool wakeupOnLora;              // Réveil sur paquet LoRa
    bool wakeupOnKeypad;            // Réveil sur touche clavier
    
    // Écran
    bool backlightAutoOff;          // Extinction automatique
    uint8_t backlightDimLevel;      // Niveau réduit (0-255)
    
    // LED
    bool statusLedEnabled;          // LED de statut activée
    bool statusLedBlinkOnLow;       // Clignoter si batterie faible
} PowerManager_Config;

// ============================================================
// SECTION 4 : ÉTAT
// ============================================================

/**
 * @brief État du gestionnaire d'alimentation
 */
typedef struct {
    bool initialized;               // Module initialisé
    PowerMode currentMode;          // Mode actuel
    
    // Batterie
    uint16_t batteryVoltageMv;      // Tension (mV)
    uint8_t batteryPercent;         // Pourcentage (0-100)
    BatteryState batteryState;      // État
    bool batteryLow;                // Batterie faible
    bool batteryCritical;           // Batterie critique
    
    // Timers d'inactivité
    uint32_t lastActivityTime;      // Dernière activité utilisateur
    uint32_t lastBacklightTime;     // Dernière activité rétroéclairage
    uint32_t lastLoraActivity;      // Dernière activité LoRa
    
    // Wake-up
    PowerWakeupSource lastWakeupSource;  // Dernière source de réveil
    uint32_t wakeupCount;           // Nombre de réveils
    
    // Statistiques
    uint32_t totalRunTime;          // Temps total en RUN (secondes)
    uint32_t totalSleepTime;        // Temps total en veille (secondes)
    uint32_t batteryCycles;         // Cycles de charge
    
    // Configuration
    PowerManager_Config config;
} PowerManager_State;

// ============================================================
// SECTION 5 : CALLBACKS
// ============================================================

/**
 * @brief Callback changement d'état batterie
 * @param percent Pourcentage (0-100)
 * @param state État de la batterie
 */
typedef void (*PowerManager_BatteryCallback)(uint8_t percent, BatteryState state);

/**
 * @brief Callback batterie faible
 */
typedef void (*PowerManager_LowBatteryCallback)(void);

/**
 * @brief Callback changement de mode
 * @param oldMode Ancien mode
 * @param newMode Nouveau mode
 */
typedef void (*PowerManager_ModeCallback)(PowerMode oldMode, PowerMode newMode);

/**
 * @brief Callback réveil
 * @param source Source du réveil
 */
typedef void (*PowerManager_WakeupCallback)(PowerWakeupSource source);

// ============================================================
// SECTION 6 : FONCTIONS D'INITIALISATION
// ============================================================

bool power_manager_init(const PowerManager_Config* config);
void power_manager_deinit(void);
bool power_manager_is_ready(void);
PowerManager_State* power_manager_get_state(void);

// ============================================================
// SECTION 7 : FONCTIONS DE GESTION D'ÉNERGIE
// ============================================================

/**
 * @brief Traitement périodique (vérifie les timeouts)
 */
void power_manager_process(void);

/**
 * @brief Signale une activité utilisateur (réinitialise les timeouts)
 */
void power_manager_activity(void);

/**
 * @brief Signale une activité LoRa
 */
void power_manager_lora_activity(void);

/**
 * @brief Force le passage en mode veille
 * @param mode Mode souhaité
 */
void power_manager_enter_mode(PowerMode mode);

/**
 * @brief Réveille du mode veille
 * @param source Source du réveil
 */
void power_manager_wakeup(PowerWakeupSource source);

/**
 * @brief Vérifie si le réveil est autorisé
 */
bool power_manager_can_wakeup(PowerWakeupSource source);

// ============================================================
// SECTION 8 : FONCTIONS DE BATTERIE
// ============================================================

uint16_t power_manager_get_battery_voltage(void);
uint8_t power_manager_get_battery_percent(void);
BatteryState power_manager_get_battery_state(void);
bool power_manager_is_battery_low(void);
bool power_manager_is_battery_critical(void);
bool power_manager_is_charging(void);
void power_manager_check_battery(void);

// ============================================================
// SECTION 9 : FONCTIONS DE RÉTROÉCLAIRAGE
// ============================================================

void power_manager_backlight_on(void);
void power_manager_backlight_off(void);
void power_manager_backlight_dim(void);
void power_manager_backlight_activity(void);

// ============================================================
// SECTION 10 : FONCTIONS DE CALLBACKS
// ============================================================

void power_manager_set_battery_callback(PowerManager_BatteryCallback callback);
void power_manager_set_low_battery_callback(PowerManager_LowBatteryCallback callback);
void power_manager_set_mode_callback(PowerManager_ModeCallback callback);
void power_manager_set_wakeup_callback(PowerManager_WakeupCallback callback);

// ============================================================
// SECTION 11 : FONCTIONS DE DÉBOGAGE
// ============================================================

void power_manager_print_state(void);
void power_manager_print_battery(void);
void power_manager_print_statistics(void);
bool power_manager_self_test(void);

// ============================================================
// SECTION 12 : MACROS UTILITAIRES
// ============================================================

#define POWER_IS_ACTIVE()               (power_state.currentMode == POWER_MODE_RUN)
#define POWER_IS_SLEEPING()             (power_state.currentMode >= POWER_MODE_SLEEP)
#define POWER_BATTERY_OK()              (!power_state.batteryLow)

// ============================================================
// SECTION 13 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define POWER_DEBUG(fmt, ...)       printf("[POWER] " fmt, ##__VA_ARGS__)
#else
    #define POWER_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 14 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // POWER_MANAGER_H