/**
 * @file battery_monitor.h
 * @brief Surveillance de la batterie (tension, niveau, état)
 * 
 * Ce fichier gère la surveillance de la batterie Li-Po/Li-Ion :
 * - Mesure de la tension via ADC
 * - Conversion tension → pourcentage
 * - Estimation de l'autonomie restante
 * - Détection de charge/décharge
 * - Alertes batterie faible/critique
 * - Compensation en température
 * - Filtrage des mesures
 * 
 * Caractéristiques de la batterie :
 * - Type : Li-Po 1S (3.7V nominal, 4.2V max, 3.3V min)
 * - Capacité : 2000 mAh (configurable)
 * - Tension de coupure : 3.0V (protection hardware)
 * 
 * Courbe de décharge Li-Po :
 * 4.20V ████████████████████████ 100%
 * 4.00V █████████████████████░░░  90%
 * 3.85V █████████████████░░░░░░░  70%
 * 3.75V ████████████████░░░░░░░░  50%
 * 3.70V █████████████░░░░░░░░░░░  30%
 * 3.65V ██████████░░░░░░░░░░░░░░  15%
 * 3.50V ████████░░░░░░░░░░░░░░░░   5%
 * 3.30V ██░░░░░░░░░░░░░░░░░░░░░░   0%
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

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
#define BATTERY_MONITOR_VERSION         "1.0.0"

/** @brief Tension de référence ADC (mV) */
#define BATTERY_ADC_REF_VOLTAGE         3300

/** @brief Résolution ADC (12 bits) */
#define BATTERY_ADC_MAX_VALUE           4095

/** @brief Canal ADC pour la mesure batterie */
#define BATTERY_ADC_CHANNEL             ADC_CHANNEL_8

/** @brief Instance ADC */
#define BATTERY_ADC_INSTANCE            ADC1

/** @brief Rapport du diviseur de tension (si utilisé) */
#define BATTERY_VOLTAGE_DIVIDER         2.0f    // R1=R2=10k → ratio 2:1

/** @brief Tension pleine (mV) */
#define BATTERY_FULL_VOLTAGE_MV         4200

/** @brief Tension vide (mV) */
#define BATTERY_EMPTY_VOLTAGE_MV        3300

/** @brief Tension nominale (mV) */
#define BATTERY_NOMINAL_VOLTAGE_MV      3700

/** @brief Tension critique (mV) */
#define BATTERY_CRITICAL_VOLTAGE_MV     3400

/** @brief Capacité de la batterie (mAh) */
#define BATTERY_CAPACITY_MAH            2000

/** @brief Consommation en appel (mA) */
#define BATTERY_CURRENT_CALL_MA         120

/** @brief Consommation en veille (mA) */
#define BATTERY_CURRENT_IDLE_MA         30

/** @brief Consommation en veille profonde (µA) */
#define BATTERY_CURRENT_STANDBY_UA      100

/** @brief Nombre d'échantillons pour la moyenne */
#define BATTERY_SAMPLE_COUNT            10

/** @brief Intervalle de mesure (secondes) */
#define BATTERY_MEASURE_INTERVAL_S      30

// ============================================================
// SECTION 2 : ÉTATS DE LA BATTERIE
// ============================================================

/**
 * @brief État de la batterie
 */
typedef enum {
    BATTERY_STATE_UNKNOWN       = 0,    // État inconnu
    BATTERY_STATE_CHARGING      = 1,    // En charge
    BATTERY_STATE_DISCHARGING   = 2,    // En décharge
    BATTERY_STATE_FULL          = 3,    // Chargée à 100%
    BATTERY_STATE_LOW           = 4,    // Niveau faible (< 15%)
    BATTERY_STATE_CRITICAL      = 5,    // Niveau critique (< 5%)
    BATTERY_STATE_EMPTY         = 6     // Vide (arrêt imminent)
} BatteryState;

/**
 * @brief État de charge
 */
typedef enum {
    CHARGE_STATE_NONE           = 0,    // Pas de chargeur
    CHARGE_STATE_TRICKLE        = 1,    // Charge lente (< 3.0V)
    CHARGE_STATE_CC             = 2,    // Courant constant (3.0-4.1V)
    CHARGE_STATE_CV             = 3,    // Tension constante (4.1-4.2V)
    CHARGE_STATE_COMPLETE       = 4     // Charge terminée
} ChargeState;

// ============================================================
// SECTION 3 : CONFIGURATION
// ============================================================

/**
 * @brief Configuration du moniteur de batterie
 */
typedef struct {
    // Paramètres batterie
    uint16_t fullVoltageMv;         // Tension pleine (mV)
    uint16_t emptyVoltageMv;        // Tension vide (mV)
    uint16_t nominalVoltageMv;      // Tension nominale (mV)
    uint16_t capacityMah;           // Capacité (mAh)
    
    // Seuils
    uint8_t lowPercent;             // Seuil faible (%)
    uint8_t criticalPercent;        // Seuil critique (%)
    uint16_t lowVoltageMv;          // Tension seuil faible
    uint16_t criticalVoltageMv;     // Tension seuil critique
    
    // Mesure
    uint8_t sampleCount;            // Nombre d'échantillons
    uint16_t measureIntervalS;      // Intervalle mesure (s)
    float voltageDivider;           // Ratio diviseur de tension
    bool enableFilter;              // Activer le filtrage
    bool enableTemperatureComp;     // Compensation en température
    
    // Alertes
    bool alertOnLow;                // Alerter si batterie faible
    bool alertOnCritical;           // Alerter si batterie critique
    bool autoShutdown;              // Extinction automatique si critique
    uint16_t shutdownDelayS;        // Délai avant extinction
    
    // Estimation autonomie
    uint16_t currentCallMa;         // Courant en appel (mA)
    uint16_t currentIdleMa;         // Courant en veille (mA)
    uint16_t currentStandbyUa;      // Courant en veille profonde (µA)
} BatteryMonitor_Config;

// ============================================================
// SECTION 4 : DONNÉES DE MESURE
// ============================================================

/**
 * @brief Point de mesure
 */
typedef struct {
    uint16_t voltageMv;             // Tension mesurée (mV)
    uint8_t percent;                // Pourcentage calculé
    BatteryState state;             // État
    uint32_t timestamp;             // Horodatage
} BatterySample;

/**
 * @brief État du moniteur
 */
typedef struct {
    bool initialized;               // Module initialisé
    
    // Mesures actuelles
    uint16_t voltageMv;             // Tension actuelle (mV)
    uint16_t rawAdc;                // Valeur ADC brute
    uint8_t percent;                // Pourcentage (0-100)
    BatteryState state;             // État actuel
    ChargeState chargeState;        // État de charge
    
    // Tendance
    int16_t voltageTrend;           // Tendance (mV/minute)
    uint16_t voltageMin;            // Tension minimum enregistrée
    uint16_t voltageMax;            // Tension maximum enregistrée
    
    // Estimation autonomie
    uint16_t estimatedMinutesCall;  // Minutes restantes en appel
    uint16_t estimatedHoursIdle;    // Heures restantes en veille
    uint16_t estimatedDaysStandby;  // Jours restants en veille profonde
    
    // Compteurs
    uint32_t totalMeasurements;     // Nombre de mesures
    uint32_t lastMeasureTime;       // Dernière mesure
    uint32_t lowBatteryTime;        // Durée en mode faible
    uint32_t chargeCycles;          // Cycles de charge estimés
    
    // Échantillons récents (pour filtrage)
    uint16_t samples[BATTERY_SAMPLE_COUNT];
    uint8_t sampleIndex;
    
    // Configuration
    BatteryMonitor_Config config;
} BatteryMonitor_State;

// ============================================================
// SECTION 5 : CALLBACKS
// ============================================================

/**
 * @brief Callback changement niveau batterie
 * @param percent Nouveau pourcentage
 * @param state Nouvel état
 */
typedef void (*BatteryMonitor_PercentCallback)(uint8_t percent, BatteryState state);

/**
 * @brief Callback batterie faible
 */
typedef void (*BatteryMonitor_LowCallback)(void);

/**
 * @brief Callback batterie critique
 */
typedef void (*BatteryMonitor_CriticalCallback)(void);

/**
 * @brief Callback charge détectée
 */
typedef void (*BatteryMonitor_ChargeCallback)(ChargeState state);

// ============================================================
// SECTION 6 : FONCTIONS D'INITIALISATION
// ============================================================

bool battery_monitor_init(const BatteryMonitor_Config* config);
void battery_monitor_deinit(void);
bool battery_monitor_is_ready(void);
BatteryMonitor_State* battery_monitor_get_state(void);

// ============================================================
// SECTION 7 : FONCTIONS DE MESURE
// ============================================================

/**
 * @brief Mesure la tension batterie (brute, une seule mesure)
 * @return Tension en mV
 */
uint16_t battery_monitor_read_voltage(void);

/**
 * @brief Mesure la tension (avec filtrage et moyenne)
 * @return Tension en mV
 */
uint16_t battery_monitor_measure_voltage(void);

/**
 * @brief Force une mesure immédiate
 */
void battery_monitor_measure_now(void);

/**
 * @brief Traitement périodique (à appeler régulièrement)
 */
void battery_monitor_process(void);

// ============================================================
// SECTION 8 : FONCTIONS DE CALCUL
// ============================================================

/**
 * @brief Convertit une tension en pourcentage
 * @param voltageMv Tension en mV
 * @return Pourcentage (0-100)
 */
uint8_t battery_monitor_voltage_to_percent(uint16_t voltageMv);

/**
 * @brief Convertit un pourcentage en tension
 * @param percent Pourcentage (0-100)
 * @return Tension en mV
 */
uint16_t battery_monitor_percent_to_voltage(uint8_t percent);

/**
 * @brief Calcule l'autonomie restante en appel
 * @return Minutes restantes
 */
uint16_t battery_monitor_estimate_call_time(void);

/**
 * @brief Calcule l'autonomie restante en veille
 * @return Heures restantes
 */
uint16_t battery_monitor_estimate_idle_time(void);

/**
 * @brief Calcule l'autonomie en veille profonde
 * @return Jours restants
 */
uint16_t battery_monitor_estimate_standby_time(void);

// ============================================================
// SECTION 9 : FONCTIONS D'ÉTAT
// ============================================================

uint16_t battery_monitor_get_voltage(void);
uint8_t battery_monitor_get_percent(void);
BatteryState battery_monitor_get_state(void);
ChargeState battery_monitor_get_charge_state(void);
bool battery_monitor_is_charging(void);
bool battery_monitor_is_low(void);
bool battery_monitor_is_critical(void);
bool battery_monitor_is_full(void);
int16_t battery_monitor_get_trend(void);

// ============================================================
// SECTION 10 : FONCTIONS D'ALERTE
// ============================================================

/**
 * @brief Vérifie et déclenche les alertes si nécessaire
 */
void battery_monitor_check_alerts(void);

/**
 * @brief Active/désactive l'extinction automatique
 */
void battery_monitor_auto_shutdown_enable(bool enable);

// ============================================================
// SECTION 11 : FONCTIONS DE CALLBACKS
// ============================================================

void battery_monitor_set_percent_callback(BatteryMonitor_PercentCallback callback);
void battery_monitor_set_low_callback(BatteryMonitor_LowCallback callback);
void battery_monitor_set_critical_callback(BatteryMonitor_CriticalCallback callback);
void battery_monitor_set_charge_callback(BatteryMonitor_ChargeCallback callback);

// ============================================================
// SECTION 12 : FONCTIONS DE DÉBOGAGE
// ============================================================

void battery_monitor_print_state(void);
void battery_monitor_print_voltage(void);
void battery_monitor_print_estimate(void);
bool battery_monitor_self_test(void);

// ============================================================
// SECTION 13 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Calcule le pourcentage à partir d'une tension
 */
#define BATTERY_CALC_PERCENT(mv) \
    ((uint8_t)(((int32_t)(mv) - BATTERY_EMPTY_VOLTAGE_MV) * 100 / \
               (BATTERY_FULL_VOLTAGE_MV - BATTERY_EMPTY_VOLTAGE_MV)))

/**
 * @brief Calcule la tension à partir d'un pourcentage
 */
#define BATTERY_CALC_VOLTAGE(pct) \
    ((uint16_t)(BATTERY_EMPTY_VOLTAGE_MV + \
                (uint32_t)(pct) * (BATTERY_FULL_VOLTAGE_MV - BATTERY_EMPTY_VOLTAGE_MV) / 100))

/**
 * @brief Vérifie si la batterie est OK
 */
#define BATTERY_IS_OK()                 (battery_monitor_get_percent() > 10)

/**
 * @brief Récupère le niveau sous forme d'icône
 * @return 0=vide, 1=faible, 2=moyen, 3=plein
 */
#define BATTERY_ICON_LEVEL() \
    ((battery_monitor_get_percent() > 80) ? 3 : \
     (battery_monitor_get_percent() > 30) ? 2 : \
     (battery_monitor_get_percent() > 10) ? 1 : 0)

// ============================================================
// SECTION 14 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define BATTERY_DEBUG(fmt, ...)     printf("[BATTERY] " fmt, ##__VA_ARGS__)
#else
    #define BATTERY_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // BATTERY_MONITOR_H