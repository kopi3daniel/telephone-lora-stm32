/**
 * @file battery_monitor.cpp
 * @brief Implémentation du moniteur de batterie
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans battery_monitor.h.
 * 
 * Il gère :
 * - La mesure de la tension batterie via ADC
 * - La conversion tension → pourcentage
 * - L'estimation de l'autonomie restante
 * - La détection de charge/décharge
 * - Les alertes batterie faible/critique
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "battery_monitor.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================
// HANDLES EXTERNES
// ============================================================

/** @brief Handle ADC pour la mesure batterie */
extern ADC_HandleTypeDef hadc1;

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du moniteur */
static BatteryMonitor_State battery_state;

/** @brief Configuration */
static BatteryMonitor_Config battery_config = {
    .fullVoltageMv = BATTERY_FULL_VOLTAGE_MV,
    .emptyVoltageMv = BATTERY_EMPTY_VOLTAGE_MV,
    .nominalVoltageMv = BATTERY_NOMINAL_VOLTAGE_MV,
    .capacityMah = BATTERY_CAPACITY_MAH,
    .lowPercent = POWER_BATTERY_LOW_THRESHOLD,
    .criticalPercent = POWER_BATTERY_CRITICAL_THRESHOLD,
    .lowVoltageMv = 3600,
    .criticalVoltageMv = BATTERY_CRITICAL_VOLTAGE_MV,
    .sampleCount = BATTERY_SAMPLE_COUNT,
    .measureIntervalS = BATTERY_MEASURE_INTERVAL_S,
    .voltageDivider = BATTERY_VOLTAGE_DIVIDER,
    .enableFilter = true,
    .enableTemperatureComp = false,
    .alertOnLow = true,
    .alertOnCritical = true,
    .autoShutdown = true,
    .shutdownDelayS = 30,
    .currentCallMa = BATTERY_CURRENT_CALL_MA,
    .currentIdleMa = BATTERY_CURRENT_IDLE_MA,
    .currentStandbyUa = BATTERY_CURRENT_STANDBY_UA
};

/** @brief Callbacks */
static BatteryMonitor_PercentCallback percent_callback = NULL;
static BatteryMonitor_LowCallback low_callback = NULL;
static BatteryMonitor_CriticalCallback critical_callback = NULL;
static BatteryMonitor_ChargeCallback charge_callback = NULL;

/** @brief État précédent (pour détecter les changements) */
static BatteryState previous_state = BATTERY_STATE_UNKNOWN;
static uint8_t previous_percent = 100;
static bool low_alert_sent = false;
static bool critical_alert_sent = false;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le moniteur de batterie
 */
bool battery_monitor_init(const BatteryMonitor_Config* config)
{
    BATTERY_DEBUG("Initialisation du moniteur de batterie...\n");
    
    if (config != NULL)
    {
        memcpy(&battery_config, config, sizeof(BatteryMonitor_Config));
    }
    
    // Initialiser l'état
    memset(&battery_state, 0, sizeof(BatteryMonitor_State));
    battery_state.config = battery_config;
    battery_state.voltageMin = 9999;
    battery_state.voltageMax = 0;
    battery_state.percent = 100;
    battery_state.state = BATTERY_STATE_UNKNOWN;
    
    // Configurer le GPIO pour l'ADC batterie
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // GPIO pour la mesure batterie (généralement un canal ADC sur un pont diviseur)
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = GPIO_PIN_0;  // PB0 = ADC1_IN8 (exemple)
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // Faire une première mesure
    battery_monitor_measure_now();
    
    battery_state.initialized = true;
    
    BATTERY_DEBUG("Moniteur initialisé\n");
    BATTERY_DEBUG("Tension: %d mV, Niveau: %d%%\n", 
                 battery_state.voltageMv, battery_state.percent);
    
    return true;
}

/**
 * @brief Désinitialise
 */
void battery_monitor_deinit(void)
{
    battery_state.initialized = false;
}

/**
 * @brief Vérifie si prêt
 */
bool battery_monitor_is_ready(void)
{
    return battery_state.initialized;
}

/**
 * @brief Récupère l'état
 */
BatteryMonitor_State* battery_monitor_get_state(void)
{
    return &battery_state;
}

// ============================================================
// SECTION 2 : MESURE
// ============================================================

/**
 * @brief Lit la tension batterie (une seule mesure brute)
 */
uint16_t battery_monitor_read_voltage(void)
{
    uint16_t adcValue = 0;
    
    // Configurer le canal ADC
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = BATTERY_ADC_CHANNEL;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;  // Échantillonnage long pour précision
    
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    
    // Démarrer la conversion
    HAL_ADC_Start(&hadc1);
    
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
        adcValue = HAL_ADC_GetValue(&hadc1);
    }
    
    HAL_ADC_Stop(&hadc1);
    
    // Convertir la valeur ADC en tension (mV)
    // Avec diviseur de tension : tension réelle = tension mesurée × ratio
    uint16_t voltageMv = (uint16_t)((float)adcValue * BATTERY_ADC_REF_VOLTAGE / 
                                     BATTERY_ADC_MAX_VALUE * 
                                     battery_config.voltageDivider);
    
    battery_state.rawAdc = adcValue;
    
    return voltageMv;
}

/**
 * @brief Mesure la tension avec filtrage et moyenne
 */
uint16_t battery_monitor_measure_voltage(void)
{
    uint32_t sum = 0;
    uint16_t minVal = 9999;
    uint16_t maxVal = 0;
    
    // Prendre N échantillons
    for (uint8_t i = 0; i < battery_config.sampleCount; i++)
    {
        uint16_t voltage = battery_monitor_read_voltage();
        
        sum += voltage;
        
        if (voltage < minVal) minVal = voltage;
        if (voltage > maxVal) maxVal = voltage;
        
        HAL_Delay(5);  // Pause entre les échantillons
    }
    
    // Si le filtrage est activé, exclure les extrêmes
    uint16_t avgVoltage;
    
    if (battery_config.enableFilter && battery_config.sampleCount >= 4)
    {
        sum -= minVal;
        sum -= maxVal;
        avgVoltage = sum / (battery_config.sampleCount - 2);
    }
    else
    {
        avgVoltage = sum / battery_config.sampleCount;
    }
    
    return avgVoltage;
}

/**
 * @brief Force une mesure immédiate
 */
void battery_monitor_measure_now(void)
{
    // Mesurer la tension
    uint16_t voltage = battery_monitor_measure_voltage();
    
    // Mettre à jour les stats
    battery_state.voltageMv = voltage;
    battery_state.totalMeasurements++;
    battery_state.lastMeasureTime = HAL_GetTick();
    
    // Mise à jour min/max
    if (voltage < battery_state.voltageMin) battery_state.voltageMin = voltage;
    if (voltage > battery_state.voltageMax) battery_state.voltageMax = voltage;
    
    // Ajouter à l'historique des échantillons
    battery_state.samples[battery_state.sampleIndex % BATTERY_SAMPLE_COUNT] = voltage;
    battery_state.sampleIndex++;
    
    // Calculer la tendance (différence entre le plus récent et le plus ancien)
    if (battery_state.totalMeasurements >= BATTERY_SAMPLE_COUNT)
    {
        uint16_t oldest = battery_state.samples[(battery_state.sampleIndex - BATTERY_SAMPLE_COUNT) % BATTERY_SAMPLE_COUNT];
        
        // Tendance en mV par minute (approximation)
        uint32_t timeDiff = BATTERY_MEASURE_INTERVAL_S * BATTERY_SAMPLE_COUNT;
        if (timeDiff > 0)
        {
            battery_state.voltageTrend = (int16_t)((int32_t)(voltage - oldest) * 60 / timeDiff);
        }
    }
    
    // Convertir en pourcentage
    uint8_t newPercent = battery_monitor_voltage_to_percent(voltage);
    
    // Déterminer l'état
    BatteryState newState;
    
    if (newPercent >= 99)
    {
        newState = BATTERY_STATE_FULL;
    }
    else if (newPercent <= battery_config.criticalPercent)
    {
        newState = BATTERY_STATE_CRITICAL;
    }
    else if (newPercent <= battery_config.lowPercent)
    {
        newState = BATTERY_STATE_LOW;
    }
    else
    {
        newState = BATTERY_STATE_DISCHARGING;
    }
    
    // Détecter les changements
    bool percentChanged = (abs(newPercent - battery_state.percent) >= 1);
    bool stateChanged = (newState != battery_state.state);
    
    battery_state.percent = newPercent;
    battery_state.state = newState;
    
    // Estimer l'autonomie
    battery_state.estimatedMinutesCall = battery_monitor_estimate_call_time();
    battery_state.estimatedHoursIdle = battery_monitor_estimate_idle_time();
    battery_state.estimatedDaysStandby = battery_monitor_estimate_standby_time();
    
    // Notifier les changements
    if (percentChanged && percent_callback)
    {
        percent_callback(newPercent, newState);
    }
    
    // Vérifier les alertes
    if (battery_config.alertOnLow || battery_config.alertOnCritical)
    {
        battery_monitor_check_alerts();
    }
    
    BATTERY_DEBUG("Mesure: %d mV → %d%% (état=%d, tendance=%d mV/min)\n",
                 voltage, newPercent, newState, battery_state.voltageTrend);
}

/**
 * @brief Traitement périodique
 */
void battery_monitor_process(void)
{
    if (!battery_state.initialized) return;
    
    uint32_t now = HAL_GetTick();
    
    if ((now - battery_state.lastMeasureTime) >= (battery_config.measureIntervalS * 1000))
    {
        battery_monitor_measure_now();
    }
}

// ============================================================
// SECTION 3 : CALCULS
// ============================================================

/**
 * @brief Convertit une tension en pourcentage
 * 
 * Utilise une courbe de décharge Li-Po réaliste
 * (non linéaire pour une meilleure précision)
 */
uint8_t battery_monitor_voltage_to_percent(uint16_t voltageMv)
{
    // Courbe de décharge Li-Po (points caractéristiques)
    // Tension (mV) → Pourcentage
    static const uint16_t curve_mv[] = {
        4200, 4150, 4100, 4050, 4000, 3950, 3900, 3850,
        3800, 3770, 3740, 3710, 3680, 3650, 3620, 3590,
        3560, 3530, 3500, 3450, 3400, 3350, 3300
    };
    
    static const uint8_t curve_pct[] = {
        100, 97, 93, 88, 82, 75, 67, 58,
        50, 43, 37, 31, 25, 20, 15, 11,
        8, 5, 3, 2, 1, 0, 0
    };
    
    const uint8_t curveSize = sizeof(curve_mv) / sizeof(curve_mv[0]);
    
    // En dessous du minimum
    if (voltageMv <= curve_mv[curveSize - 1]) return 0;
    
    // Au-dessus du maximum
    if (voltageMv >= curve_mv[0]) return 100;
    
    // Interpolation linéaire entre deux points
    for (uint8_t i = 0; i < curveSize - 1; i++)
    {
        if (voltageMv >= curve_mv[i + 1] && voltageMv <= curve_mv[i])
        {
            // Interpolation
            int32_t dV = curve_mv[i] - curve_mv[i + 1];
            int32_t dP = curve_pct[i] - curve_pct[i + 1];
            
            if (dV > 0)
            {
                int32_t percent = curve_pct[i + 1] + 
                                  (int32_t)(voltageMv - curve_mv[i + 1]) * dP / dV;
                return (uint8_t)percent;
            }
        }
    }
    
    // Fallback : formule linéaire simple
    return BATTERY_CALC_PERCENT(voltageMv);
}

/**
 * @brief Convertit un pourcentage en tension
 */
uint16_t battery_monitor_percent_to_voltage(uint8_t percent)
{
    if (percent >= 100) return battery_config.fullVoltageMv;
    if (percent == 0) return battery_config.emptyVoltageMv;
    
    return BATTERY_CALC_VOLTAGE(percent);
}

/**
 * @brief Estime l'autonomie en appel (minutes)
 */
uint16_t battery_monitor_estimate_call_time(void)
{
    if (battery_config.currentCallMa == 0) return 0;
    
    float remainingCapacityMah = (float)battery_config.capacityMah * 
                                  battery_state.percent / 100.0f;
    
    float hours = remainingCapacityMah / battery_config.currentCallMa;
    return (uint16_t)(hours * 60);
}

/**
 * @brief Estime l'autonomie en veille (heures)
 */
uint16_t battery_monitor_estimate_idle_time(void)
{
    if (battery_config.currentIdleMa == 0) return 0;
    
    float remainingCapacityMah = (float)battery_config.capacityMah * 
                                  battery_state.percent / 100.0f;
    
    return (uint16_t)(remainingCapacityMah / battery_config.currentIdleMa);
}

/**
 * @brief Estime l'autonomie en veille profonde (jours)
 */
uint16_t battery_monitor_estimate_standby_time(void)
{
    if (battery_config.currentStandbyUa == 0) return 0;
    
    float remainingCapacityUah = (float)battery_config.capacityMah * 
                                  battery_state.percent / 100.0f * 1000.0f;
    
    float hours = remainingCapacityUah / battery_config.currentStandbyUa;
    return (uint16_t)(hours / 24);
}

// ============================================================
// SECTION 4 : ÉTAT
// ============================================================

uint16_t battery_monitor_get_voltage(void)    { return battery_state.voltageMv; }
uint8_t battery_monitor_get_percent(void)     { return battery_state.percent; }
BatteryState battery_monitor_get_state(void)   { return battery_state.state; }
int16_t battery_monitor_get_trend(void)        { return battery_state.voltageTrend; }

ChargeState battery_monitor_get_charge_state(void)
{
    return battery_state.chargeState;
}

bool battery_monitor_is_charging(void)
{
    return (battery_state.chargeState >= CHARGE_STATE_TRICKLE && 
            battery_state.chargeState <= CHARGE_STATE_CV);
}

bool battery_monitor_is_low(void)
{
    return (battery_state.state == BATTERY_STATE_LOW);
}

bool battery_monitor_is_critical(void)
{
    return (battery_state.state == BATTERY_STATE_CRITICAL || 
            battery_state.state == BATTERY_STATE_EMPTY);
}

bool battery_monitor_is_full(void)
{
    return (battery_state.state == BATTERY_STATE_FULL);
}

// ============================================================
// SECTION 5 : ALERTES
// ============================================================

/**
 * @brief Vérifie et déclenche les alertes
 */
void battery_monitor_check_alerts(void)
{
    // Alerte batterie faible
    if (battery_state.state == BATTERY_STATE_LOW && !low_alert_sent)
    {
        low_alert_sent = true;
        BATTERY_DEBUG("ALERTE: Batterie faible (%d%%)\n", battery_state.percent);
        
        if (low_callback) low_callback();
    }
    
    // Alerte batterie critique
    if (battery_state.state == BATTERY_STATE_CRITICAL && !critical_alert_sent)
    {
        critical_alert_sent = true;
        BATTERY_DEBUG("ALERTE: Batterie critique (%d%%)\n", battery_state.percent);
        
        if (critical_callback) critical_callback();
        
        // Extinction automatique si configurée
        if (battery_config.autoShutdown)
        {
            BATTERY_DEBUG("Extinction automatique dans %d secondes\n", 
                         battery_config.shutdownDelayS);
            // Le power_manager gère l'extinction après le délai
        }
    }
    
    // Réinitialiser les alertes si la batterie remonte
    if (battery_state.state == BATTERY_STATE_DISCHARGING || 
        battery_state.state == BATTERY_STATE_FULL)
    {
        low_alert_sent = false;
        critical_alert_sent = false;
    }
    
    // Réinitialiser si charge détectée
    if (battery_state.chargeState >= CHARGE_STATE_TRICKLE)
    {
        low_alert_sent = false;
        critical_alert_sent = false;
    }
}

void battery_monitor_auto_shutdown_enable(bool enable)
{
    battery_config.autoShutdown = enable;
}

// ============================================================
// SECTION 6 : CALLBACKS
// ============================================================

void battery_monitor_set_percent_callback(BatteryMonitor_PercentCallback callback)
    { percent_callback = callback; }

void battery_monitor_set_low_callback(BatteryMonitor_LowCallback callback)
    { low_callback = callback; }

void battery_monitor_set_critical_callback(BatteryMonitor_CriticalCallback callback)
    { critical_callback = callback; }

void battery_monitor_set_charge_callback(BatteryMonitor_ChargeCallback callback)
    { charge_callback = callback; }

// ============================================================
// SECTION 7 : DÉBOGAGE
// ============================================================

void battery_monitor_print_state(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║     ÉTAT DE LA BATTERIE                       ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Tension       : %-4d mV                      ║\n", battery_state.voltageMv);
    printf("║ Niveau        : %-3d%%                        ║\n", battery_state.percent);
    printf("║ État          : %-31s ║\n", 
           battery_state.state == BATTERY_STATE_FULL ? "Pleine" :
           battery_state.state == BATTERY_STATE_CHARGING ? "En charge" :
           battery_state.state == BATTERY_STATE_LOW ? "Faible" :
           battery_state.state == BATTERY_STATE_CRITICAL ? "CRITIQUE" : "Décharge");
    printf("║ Tendance      : %-4d mV/min                  ║\n", battery_state.voltageTrend);
    printf("║ Min/Max       : %d/%d mV                   ║\n", battery_state.voltageMin, battery_state.voltageMax);
    printf("║ Appel restant : %-4d min                     ║\n", battery_state.estimatedMinutesCall);
    printf("║ Veille rest.  : %-4d h                       ║\n", battery_state.estimatedHoursIdle);
    printf("║ Mesures       : %-31lu ║\n", (unsigned long)battery_state.totalMeasurements);
    printf("╚══════════════════════════════════════════════╝\n\n");
}

void battery_monitor_print_voltage(void)
{
    printf("[BATTERY] %d mV = %d%% ", battery_state.voltageMv, battery_state.percent);
    
    // Barre visuelle
    int bars = (battery_state.percent + 9) / 10;
    printf("[");
    for (int i = 0; i < 10; i++) {
        printf("%c", i < bars ? '█' : '░');
    }
    printf("] %s\n", battery_monitor_is_charging() ? "⚡" : "");
}

void battery_monitor_print_estimate(void)
{
    printf("\n═══ AUTONOMIE ESTIMÉE ═══\n");
    printf("En appel      : %d minutes (%d h)\n", 
           battery_state.estimatedMinutesCall,
           battery_state.estimatedMinutesCall / 60);
    printf("En veille     : %d heures (%d jours)\n",
           battery_state.estimatedHoursIdle,
           battery_state.estimatedHoursIdle / 24);
    printf("Veille prof.  : %d jours\n", battery_state.estimatedDaysStandby);
    printf("═══════════════════════════\n\n");
}

bool battery_monitor_self_test(void)
{
    BATTERY_DEBUG("Auto-test...\n");
    
    if (!battery_state.initialized)
    {
        BATTERY_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Tester la mesure
    uint16_t voltage = battery_monitor_read_voltage();
    BATTERY_DEBUG("Tension brute : %d mV\n", voltage);
    
    // Vérifier la cohérence
    if (voltage < 2000 || voltage > 5000)
    {
        BATTERY_DEBUG("Échec : tension hors limites\n");
        return false;
    }
    
    // Tester la conversion
    uint8_t percent = battery_monitor_voltage_to_percent(voltage);
    BATTERY_DEBUG("Conversion : %d mV → %d%%\n", voltage, percent);
    
    BATTERY_DEBUG("Auto-test OK\n");
    return true;
}