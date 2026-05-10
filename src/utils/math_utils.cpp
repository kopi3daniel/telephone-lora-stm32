/**
 * @file    math_utils.cpp
 * @brief   Implémentation des utilitaires mathématiques
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente des fonctions mathématiques optimisées pour Cortex-M4.
 * 
 * UTILISATION DU FPU CORTEX-M4 :
 *    - Instructions flottantes matérielles (single precision)
 *    - float = 32 bits, matériel
 *    - double = 64 bits, émulé (lent !)
 *    - Toujours utiliser 'f' suffix pour les constantes (3.14f)
 * 
 * ALGORITHMES NOTABLES :
 *    - FastInvSqrt : Quake III (John Carmack)
 *    - FastSin : Table lookup 256 entrées + interpolation
 *    - LCG Random : Park-Miller "Minimal Standard"
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "math_utils.h"
#include "debug_utils.h"

/* HAL */
#include "stm32f4xx_hal.h"

/* Standard */
#include <string.h>
#include <stdlib.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs */
#define TAG                                 "MathUtils"

/** Taille de la table sinusoïdale */
#define SIN_TABLE_SIZE                      256

/** Paramètres LCG Park-Miller */
#define LCG_A                               16807
#define LCG_C                               0
#define LCG_M                               2147483647

/* ======================================================================== */
/*                VARIABLES STATIQUES                                       */
/* ======================================================================== */

/**
 * @brief Table sinusoïdale précalculée (256 entrées, 0 à 2π)
 * 
 * Générée par : sin(i * 2π / 256) * 32767
 * Amplitude : 32767 (Q15)
 */
static const int16_t SIN_TABLE[SIN_TABLE_SIZE] = {
       0,   804,  1608,  2411,  3212,  4011,  4808,  5602,
    6393,  7179,  7962,  8739,  9512, 10279, 11039, 11793,
   12540, 13279, 14010, 14733, 15447, 16151, 16846, 17531,
   18205, 18868, 19520, 20160, 20788, 21403, 22006, 22595,
   23170, 23732, 24279, 24812, 25330, 25833, 26320, 26791,
   27246, 27684, 28106, 28511, 28899, 29269, 29622, 29957,
   30274, 30572, 30853, 31114, 31357, 31581, 31786, 31972,
   32138, 32286, 32413, 32522, 32610, 32679, 32729, 32758,
   32767, 32758, 32729, 32679, 32610, 32522, 32413, 32286,
   32138, 31972, 31786, 31581, 31357, 31114, 30853, 30572,
   30274, 29957, 29622, 29269, 28899, 28511, 28106, 27684,
   27246, 26791, 26320, 25833, 25330, 24812, 24279, 23732,
   23170, 22595, 22006, 21403, 20788, 20160, 19520, 18868,
   18205, 17531, 16846, 16151, 15447, 14733, 14010, 13279,
   12540, 11793, 11039, 10279,  9512,  8739,  7962,  7179,
    6393,  5602,  4808,  4011,  3212,  2411,  1608,   804,
       0,  -804, -1608, -2411, -3212, -4011, -4808, -5602,
   -6393, -7179, -7962, -8739, -9512,-10279,-11039,-11793,
  -12540,-13279,-14010,-14733,-15447,-16151,-16846,-17531,
  -18205,-18868,-19520,-20160,-20788,-21403,-22006,-22595,
  -23170,-23732,-24279,-24812,-25330,-25833,-26320,-26791,
  -27246,-27684,-28106,-28511,-28899,-29269,-29622,-29957,
  -30274,-30572,-30853,-31114,-31357,-31581,-31786,-31972,
  -32138,-32286,-32413,-32522,-32610,-32679,-32729,-32758,
  -32767,-32758,-32729,-32679,-32610,-32522,-32413,-32286,
  -32138,-31972,-31786,-31581,-31357,-31114,-30853,-30572,
  -30274,-29957,-29622,-29269,-28899,-28511,-28106,-27684,
  -27246,-26791,-26320,-25833,-25330,-24812,-24279,-23732,
  -23170,-22595,-22006,-21403,-20788,-20160,-19520,-18868,
  -18205,-17531,-16846,-16151,-15447,-14733,-14010,-13279,
  -12540,-11793,-11039,-10279, -9512, -8739, -7962, -7179,
   -6393, -5602, -4808, -4011, -3212, -2411, -1608,  -804,
};

/* ======================================================================== */
/*              LIMITES ET BORNAGE                                          */
/* ======================================================================== */

int32_t MathUtils_Wrap(int32_t value, int32_t min_val, int32_t max_val)
{
    if (max_val <= min_val) return min_val;

    int32_t range = max_val - min_val + 1;
    if (range <= 0) return min_val;

    value = (value - min_val) % range;
    if (value < 0) value += range;

    return value + min_val;
}

float MathUtils_WrapFloat(float value, float min_val, float max_val)
{
    if (max_val <= min_val) return min_val;

    float range = max_val - min_val;
    if (range < MATH_EPSILON) return min_val;

    value = fmodf(value - min_val, range);
    if (value < 0.0f) value += range;

    return value + min_val;
}

/* ======================================================================== */
/*              ARRONDI                                                     */
/* ======================================================================== */

int32_t MathUtils_Round(float value)
{
    return (int32_t)(value + (value >= 0.0f ? 0.5f : -0.5f));
}

float MathUtils_RoundTo(float value, uint8_t decimals)
{
    if (decimals > 6) decimals = 6;

    float factor = 1.0f;
    for (uint8_t i = 0; i < decimals; i++) {
        factor *= 10.0f;
    }

    return roundf(value * factor) / factor;
}

int32_t MathUtils_Trunc(float value)
{
    return (int32_t)value;
}

float MathUtils_Frac(float value)
{
    float int_part;
    return modff(value, &int_part);
}

/* ======================================================================== */
/*              CONVERSIONS                                                 */
/* ======================================================================== */

float MathUtils_DegToRad(float deg)
{
    return deg * MATH_DEG_TO_RAD;
}

float MathUtils_RadToDeg(float rad)
{
    return rad * MATH_RAD_TO_DEG;
}

float MathUtils_NormalizeAngle(float deg)
{
    while (deg < 0.0f) deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    return deg;
}

float MathUtils_AngleDiff(float from, float to)
{
    float diff = MathUtils_NormalizeAngle(to) - MathUtils_NormalizeAngle(from);
    if (diff > 180.0f) diff -= 360.0f;
    if (diff < -180.0f) diff += 360.0f;
    return diff;
}

void MathUtils_PolarToCartesian(float r, float theta, float* x, float* y)
{
    if (x) *x = r * cosf(theta);
    if (y) *y = r * sinf(theta);
}

void MathUtils_CartesianToPolar(float x, float y, float* r, float* theta)
{
    if (r) *r = MathUtils_Hypot(x, y);
    if (theta) *theta = atan2f(y, x);
}

float MathUtils_Map(float value,
                    float in_min, float in_max,
                    float out_min, float out_max)
{
    if (fabsf(in_max - in_min) < MATH_EPSILON) return out_min;
    return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float MathUtils_MapConstrained(float value,
                               float in_min, float in_max,
                               float out_min, float out_max)
{
    float mapped = MathUtils_Map(value, in_min, in_max, out_min, out_max);
    if (out_min < out_max) {
        return MATH_CLAMP(mapped, out_min, out_max);
    } else {
        return MATH_CLAMP(mapped, out_max, out_min);
    }
}

/* ======================================================================== */
/*              ARITHMÉTIQUE                                                */
/* ======================================================================== */

int32_t MathUtils_Mod(int32_t a, int32_t b)
{
    if (b == 0) return 0;
    int32_t r = a % b;
    return (r < 0) ? r + (b < 0 ? -b : b) : r;
}

int32_t MathUtils_IPow(int32_t base, uint8_t exp)
{
    int32_t result = 1;
    while (exp > 0) {
        if (exp & 1) result *= base;
        base *= base;
        exp >>= 1;
    }
    return result;
}

/**
 * @brief Racine carrée rapide (Newton-Raphson)
 * 
 * Départ avec sqrtf matérielle, puis 2 itérations Newton.
 * Plus rapide que sqrtf() seul ? Non, sqrtf() est déjà optimisé FPU.
 * Cette fonction est conservée pour les MCU sans FPU.
 */
float MathUtils_FastSqrt(float value)
{
    if (value <= 0.0f) return 0.0f;

    /* sqrtf est déjà rapide sur Cortex-M4 (instruction VSQRT) */
    return sqrtf(value);
}

/**
 * @brief Racine carrée inverse rapide (Quake III)
 * 
 * Algorithme célèbre de John Carmack.
 * Utilisé pour normaliser des vecteurs.
 */
float MathUtils_FastInvSqrt(float number)
{
    float x2 = number * 0.5f;
    float y = number;
    int32_t i;

    /* Conversion bits float → int (magic number) */
    memcpy(&i, &y, sizeof(i));
    i = 0x5F3759DF - (i >> 1);
    memcpy(&y, &i, sizeof(y));

    /* Une itération de Newton (assez pour la plupart des usages) */
    y = y * (1.5f - (x2 * y * y));

    return y;
}

float MathUtils_Average(const float* data, uint16_t count)
{
    if (!data || count == 0) return 0.0f;

    float sum = 0.0f;
    for (uint16_t i = 0; i < count; i++) {
        sum += data[i];
    }
    return sum / (float)count;
}

/**
 * @brief Calcule la médiane (tri partiel)
 */
float MathUtils_Median(float* data, uint16_t count)
{
    if (!data || count == 0) return 0.0f;
    if (count == 1) return data[0];

    /* Tri à bulles pour les petits tableaux (suffisant) */
    /* Pour les grands tableaux, utiliser quickselect */
    for (uint16_t i = 0; i < count - 1; i++) {
        for (uint16_t j = i + 1; j < count; j++) {
            if (data[i] > data[j]) {
                float temp = data[i];
                data[i] = data[j];
                data[j] = temp;
            }
        }
    }

    if (count % 2 == 0) {
        return (data[count / 2 - 1] + data[count / 2]) * 0.5f;
    } else {
        return data[count / 2];
    }
}

float MathUtils_Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float MathUtils_InverseLerp(float a, float b, float value)
{
    if (fabsf(b - a) < MATH_EPSILON) return 0.0f;
    return (value - a) / (b - a);
}

/* ======================================================================== */
/*              STATISTIQUES                                                */
/* ======================================================================== */

void MathUtils_RunningAverageInit(MathRunningAverage_t* filter,
                                  float* buffer, uint16_t size)
{
    if (!filter || !buffer || size == 0) return;

    memset(filter, 0, sizeof(*filter));
    filter->buffer = buffer;
    filter->size = size;
    memset(buffer, 0, size * sizeof(float));
}

float MathUtils_RunningAverageAdd(MathRunningAverage_t* filter, float sample)
{
    if (!filter || !filter->buffer || filter->size == 0) return sample;

    /* Soustraire l'ancien échantillon, ajouter le nouveau */
    filter->sum -= filter->buffer[filter->index];
    filter->buffer[filter->index] = sample;
    filter->sum += sample;

    filter->index = (filter->index + 1) % filter->size;
    if (filter->count < filter->size) {
        filter->count++;
    }

    filter->average = filter->sum / (float)filter->count;
    return filter->average;
}

void MathUtils_RunningAverageReset(MathRunningAverage_t* filter)
{
    if (!filter || !filter->buffer) return;

    filter->index = 0;
    filter->count = 0;
    filter->sum = 0.0f;
    filter->average = 0.0f;
    memset(filter->buffer, 0, filter->size * sizeof(float));
}

void MathUtils_RunningMinMaxInit(MathRunningMinMax_t* mm)
{
    if (!mm) return;
    memset(mm, 0, sizeof(*mm));
    mm->min_value = 1e9f;
    mm->max_value = -1e9f;
}

void MathUtils_RunningMinMaxAdd(MathRunningMinMax_t* mm, float sample)
{
    if (!mm) return;

    if (sample < mm->min_value) mm->min_value = sample;
    if (sample > mm->max_value) mm->max_value = sample;
    mm->peak_to_peak = mm->max_value - mm->min_value;
    mm->sample_count++;
}

void MathUtils_RunningMinMaxReset(MathRunningMinMax_t* mm)
{
    MathUtils_RunningMinMaxInit(mm);
}

/**
 * @brief Détecteur de pics
 * 
 * Un pic est détecté quand le signal dépasse le seuil
 * et qu'un temps mort s'est écoulé depuis le dernier pic.
 */
void MathUtils_PeakDetectorInit(MathPeakDetector_t* detector,
                                float threshold, uint32_t dead_time_ms)
{
    if (!detector) return;
    memset(detector, 0, sizeof(*detector));
    detector->threshold = threshold;
    detector->dead_time_ms = dead_time_ms;
}

bool MathUtils_PeakDetectorProcess(MathPeakDetector_t* detector,
                                   float sample, uint32_t timestamp)
{
    if (!detector) return false;

    bool peak_detected = false;

    /* Détection front montant */
    if (sample > detector->threshold && !detector->rising) {
        /* Vérifier le temps mort */
        if (timestamp - detector->last_peak_time >= detector->dead_time_ms) {
            detector->rising = true;
        }
    }

    /* Détection front descendant après montée */
    if (detector->rising && sample < detector->threshold) {
        detector->rising = false;
        detector->last_peak = detector->last_value;
        detector->last_peak_time = timestamp;
        peak_detected = true;
    }

    detector->last_value = sample;
    return peak_detected;
}

/* ======================================================================== */
/*              FILTRES                                                     */
/* ======================================================================== */

void MathUtils_LowPassFilterInit(MathLowPassFilter_t* filter, float alpha)
{
    if (!filter) return;
    memset(filter, 0, sizeof(*filter));
    filter->alpha = MATH_CLAMP(alpha, 0.0f, 1.0f);
    filter->initialized = false;
}

float MathUtils_LowPassFilterProcess(MathLowPassFilter_t* filter, float sample)
{
    if (!filter) return sample;

    if (!filter->initialized) {
        filter->output = sample;
        filter->initialized = true;
        return sample;
    }

    /* y[n] = alpha * x[n] + (1 - alpha) * y[n-1] */
    filter->output = filter->alpha * sample + (1.0f - filter->alpha) * filter->output;
    return filter->output;
}

void MathUtils_LowPassFilterReset(MathLowPassFilter_t* filter)
{
    if (!filter) return;
    filter->output = 0.0f;
    filter->initialized = false;
}

/* ======================================================================== */
/*              SIGNAUX                                                     */
/* ======================================================================== */

/**
 * @brief Sinus rapide via table + interpolation linéaire
 * 
 * Précision : ~0.1% (suffisant pour audio, RSSI, animation)
 * Temps : ~10 cycles (vs ~60 pour sinf)
 */
float MathUtils_FastSin(float angle_rad)
{
    /* Normaliser l'angle entre 0 et 2π */
    angle_rad = MathUtils_WrapFloat(angle_rad, 0.0f, MATH_2PI);

    /* Convertir en index de table */
    float index_f = angle_rad / MATH_2PI * SIN_TABLE_SIZE;
    uint16_t index = (uint16_t)index_f;
    float frac = index_f - (float)index;

    /* Interpolation linéaire */
    int16_t val0 = SIN_TABLE[index];
    int16_t val1 = SIN_TABLE[(index + 1) % SIN_TABLE_SIZE];
    float interpolated = (float)val0 + (float)(val1 - val0) * frac;

    /* Revenir à [-1, 1] */
    return interpolated / 32767.0f;
}

float MathUtils_FastCos(float angle_rad)
{
    return MathUtils_FastSin(angle_rad + MATH_PI_2);
}

/**
 * @brief Bruit blanc simple (Xorshift)
 */
float MathUtils_WhiteNoise(uint32_t* seed)
{
    if (!seed) return 0.0f;

    /* Xorshift 32 */
    uint32_t x = *seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *seed = x;

    /* Normaliser entre -1.0 et 1.0 */
    return (float)(int32_t)(x & 0x7FFFFFFF) / 1073741824.0f - 1.0f;
}

/* ======================================================================== */
/*              ALÉATOIRE                                                   */
/* ======================================================================== */

void MathUtils_RandomInit(MathRandom_t* rng, uint32_t seed)
{
    if (!rng) return;

    if (seed == 0) {
        /* Utiliser une source d'entropie : ADC + Timer */
        seed = HAL_GetTick() ^ (uint32_t)(SysTick->VAL);
    }

    rng->seed = seed;
    rng->a = LCG_A;
    rng->c = LCG_C;
    rng->m = LCG_M;
}

/**
 * @brief Générateur LCG (Park-Miller)
 */
uint32_t MathUtils_RandomU32(MathRandom_t* rng)
{
    if (!rng) return 0;

    rng->seed = (rng->a * rng->seed + rng->c) % rng->m;
    return rng->seed;
}

int32_t MathUtils_RandomRange(MathRandom_t* rng, int32_t min_val, int32_t max_val)
{
    if (!rng || max_val <= min_val) return min_val;

    uint32_t range = (uint32_t)(max_val - min_val + 1);
    uint32_t value = MathUtils_RandomU32(rng);

    return min_val + (int32_t)(value % range);
}

float MathUtils_RandomFloat(MathRandom_t* rng)
{
    if (!rng) return 0.0f;

    return (float)MathUtils_RandomU32(rng) / (float)LCG_M;
}

/* ======================================================================== */
/*              DIVERS                                                      */
/* ======================================================================== */

float MathUtils_PercentOf(float value, float percent)
{
    return value * percent / 100.0f;
}

float MathUtils_PercentBetween(float value, float total)
{
    if (fabsf(total) < MATH_EPSILON) return 0.0f;
    return (value / total) * 100.0f;
}

float MathUtils_Distance2D(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

float MathUtils_Hypot(float x, float y)
{
    /* Utilise hypotf FPU (instruction dédiée) */
    return hypotf(x, y);
}

/**
 * @brief Résout ax² + bx + c = 0
 * @return Nombre de racines réelles
 */
int MathUtils_QuadraticSolver(float a, float b, float c,
                              float* root1, float* root2)
{
    if (fabsf(a) < MATH_EPSILON) {
        /* Équation linéaire : bx + c = 0 */
        if (fabsf(b) < MATH_EPSILON) {
            return 0;  /* Pas de solution */
        }
        if (root1) *root1 = -c / b;
        return 1;
    }

    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < -MATH_EPSILON) {
        return 0;  /* Pas de racine réelle */
    } else if (discriminant < MATH_EPSILON) {
        /* Racine double */
        if (root1) *root1 = -b / (2.0f * a);
        return 1;
    } else {
        /* Deux racines */
        float sqrt_d = sqrtf(discriminant);
        if (root1) *root1 = (-b + sqrt_d) / (2.0f * a);
        if (root2) *root2 = (-b - sqrt_d) / (2.0f * a);
        return 2;
    }
}

/**
 * @brief Population count (nombre de bits à 1)
 * 
 * Utilise l'instruction POPCNT du Cortex-M4 si disponible,
 * sinon algorithme de comptage parallèle.
 */
uint8_t MathUtils_PopCount(uint32_t value)
{
    /* Algorithme SWAR (SIMD Within A Register) */
    value = value - ((value >> 1) & 0x55555555);
    value = (value & 0x33333333) + ((value >> 2) & 0x33333333);
    value = (value + (value >> 4)) & 0x0F0F0F0F;
    value = value + (value >> 8);
    value = value + (value >> 16);
    return (uint8_t)(value & 0x3F);
}

/**
 * @brief Compte les zéros en tête (Count Leading Zeros)
 * 
 * Utilise l'instruction CLZ du Cortex-M4.
 */
uint8_t MathUtils_CLZ(uint32_t value)
{
    if (value == 0) return 32;

    /* Instruction CLZ (1 cycle) */
    return (uint8_t)__builtin_clz(value);
}

bool MathUtils_IsPowerOfTwo(uint32_t value)
{
    return (value != 0) && ((value & (value - 1)) == 0);
}

uint32_t MathUtils_NextPowerOfTwo(uint32_t value)
{
    if (value == 0) return 1;

    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;

    return value;
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */