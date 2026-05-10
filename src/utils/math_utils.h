/**
 * @file    math_utils.h
 * @brief   Utilitaires mathématiques - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Fournit des fonctions mathématiques optimisées pour l'embarqué.
 * 
 * PARTICULARITÉS STM32F429 :
 * 
 * Le Cortex-M4 dispose d'une unité FPU (Floating Point Unit)
 * simple précision. Les calculs float sont donc très efficaces.
 * Les calculs double sont émulés (plus lents).
 * 
 * FPU : Présente, simple précision (float)
 * SIMD : Non (pas de NEON sur Cortex-M4)
 * 
 * CATÉGORIES DE FONCTIONS :
 * 
 * 1. LIMITES :
 *    - MIN, MAX, CLAMP (bornage)
 *    - Limitation avec wrap-around
 * 
 * 2. ARRONDI :
 *    - Round, Floor, Ceil
 *    - Arrondi à N décimales
 * 
 * 3. CONVERSIONS :
 *    - Degrés ↔ Radians
 *    - Coordonnées polaires ↔ cartésiennes
 *    - Endianness (little ↔ big)
 * 
 * 4. ARITHMÉTIQUE :
 *    - Moyenne, Médiane
 *    - Puissance, Racine carrée rapide
 *    - Module (toujours positif)
 * 
 * 5. STATISTIQUES (fenêtre glissante) :
 *    - Moyenne glissante
 *    - Min/Max glissant
 *    - Détection de pics
 * 
 * 6. SIGNAUX :
 *    - Sinusoïde rapide (table lookup)
 *    - Génération de bruit blanc
 *    - Filtre passe-bas simple
 * 
 * 7. ALÉATOIRE :
 *    - Générateur congruentiel linéaire (LCG)
 *    - Entier aléatoire dans une plage
 * 
 * EXEMPLES :
 * 
 *   float val = CLAMP(sensor_value, 0.0f, 100.0f);
 *   float avg = MathUtils_RunningAverage(&filter, new_sample);
 *   int dice = MathUtils_RandomRange(1, 6);
 *   float rad = MathUtils_DegToRad(180.0f);
 */

#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

/* ======================================================================== */
/*                     CONSTANTES SYMBOLIQUES                                */
/* ======================================================================== */

/** Pi (haute précision pour float) */
#define MATH_PI                             3.14159265358979323846f

/** Pi/2 */
#define MATH_PI_2                           1.57079632679489661923f

/** Pi/4 */
#define MATH_PI_4                           0.78539816339744830962f

/** 2*Pi */
#define MATH_2PI                            6.28318530717958647692f

/** Conversion degrés → radians */
#define MATH_DEG_TO_RAD                     0.01745329251994329577f

/** Conversion radians → degrés */
#define MATH_RAD_TO_DEG                     57.29577951308232087680f

/** Constante d'Euler (e) */
#define MATH_E                              2.71828182845904523536f

/** Racine de 2 */
#define MATH_SQRT2                          1.41421356237309504880f

/** Très petite valeur (epsilon) */
#define MATH_EPSILON                        1e-6f

/* ======================================================================== */
/*                     MACROS DE BASE                                       */
/* ======================================================================== */

/**
 * @brief Minimum de deux valeurs
 */
#define MATH_MIN(a, b)                      ((a) < (b) ? (a) : (b))

/**
 * @brief Maximum de deux valeurs
 */
#define MATH_MAX(a, b)                      ((a) > (b) ? (a) : (b))

/**
 * @brief Minimum de trois valeurs
 */
#define MATH_MIN3(a, b, c)                  MATH_MIN(MATH_MIN(a, b), c)

/**
 * @brief Maximum de trois valeurs
 */
#define MATH_MAX3(a, b, c)                  MATH_MAX(MATH_MAX(a, b), c)

/**
 * @brief Borne une valeur entre min et max
 */
#define MATH_CLAMP(val, min_val, max_val)   \
    MATH_MAX(MATH_MIN(val, max_val), min_val)

/**
 * @brief Valeur absolue
 */
#define MATH_ABS(x)                         ((x) < 0 ? -(x) : (x))

/**
 * @brief Signe d'une valeur (-1, 0, 1)
 */
#define MATH_SIGN(x)                        ((x) > 0 ? 1 : ((x) < 0 ? -1 : 0))

/**
 * @brief Carré d'une valeur
 */
#define MATH_SQUARE(x)                      ((x) * (x))

/**
 * @brief Vérifie si une valeur est dans un intervalle
 */
#define MATH_IN_RANGE(val, min_val, max_val) \
    ((val) >= (min_val) && (val) <= (max_val))

/**
 * @brief Vérifie si deux flottants sont proches (epsilon)
 */
#define MATH_FLOAT_EQ(a, b)                 (fabsf((a) - (b)) < MATH_EPSILON)

/* ======================================================================== */
/*                     STRUCTURES                                            */
/* ======================================================================== */

/**
 * @brief Filtre moyenne glissante
 */
typedef struct {
    float*      buffer;             /**< Buffer circulaire des échantillons   */
    uint16_t    size;               /**< Taille du buffer (fenêtre)           */
    uint16_t    index;              /**< Position actuelle                    */
    uint16_t    count;              /**< Nombre d'échantillons ajoutés        */
    float       sum;                /**< Somme courante (optimisation)        */
    float       average;            /**< Moyenne actuelle                     */
} MathRunningAverage_t;

/**
 * @brief Filtre min/max glissant
 */
typedef struct {
    float       min_value;          /**< Valeur minimale                      */
    float       max_value;          /**< Valeur maximale                      */
    float       peak_to_peak;       /**< Amplitude crête-à-crête              */
    uint32_t    sample_count;       /**< Nombre d'échantillons                */
} MathRunningMinMax_t;

/**
 * @brief Détecteur de pics
 */
typedef struct {
    float       threshold;          /**< Seuil de détection                   */
    float       last_value;         /**< Dernière valeur                      */
    float       last_peak;          /**< Dernier pic détecté                  */
    uint32_t    dead_time_ms;       /**< Temps mort entre pics (ms)           */
    uint32_t    last_peak_time;     /**< Timestamp du dernier pic             */
    bool        rising;             /**< Front montant en cours               */
} MathPeakDetector_t;

/**
 * @brief Filtre passe-bas simple (exponentiel)
 */
typedef struct {
    float       alpha;              /**< Coefficient de lissage (0-1)         */
    float       output;             /**< Valeur filtrée                       */
    bool        initialized;        /**< Premier échantillon reçu             */
} MathLowPassFilter_t;

/**
 * @brief Générateur de nombres aléatoires (LCG)
 */
typedef struct {
    uint32_t    seed;               /**< Graine actuelle                      */
    uint32_t    a;                  /**< Multiplicateur                       */
    uint32_t    c;                  /**< Incrément                            */
    uint32_t    m;                  /**< Module                               */
} MathRandom_t;

/* ======================================================================== */
/*              PROTOTYPES - LIMITES ET BORNAGE                             */
/* ======================================================================== */

/**
 * @brief Limite avec wrap-around (pour angles, index circulaires)
 * 
 * Exemple : wrap(370, 0, 360) = 10
 * 
 * @param value     Valeur d'entrée
 * @param min_val   Borne inférieure
 * @param max_val   Borne supérieure
 * @return          Valeur bornée avec wrap
 */
int32_t MathUtils_Wrap(int32_t value, int32_t min_val, int32_t max_val);

/**
 * @brief Limite avec wrap-around pour float
 * 
 * @param value     Valeur
 * @param min_val   Minimum
 * @param max_val   Maximum
 * @return          Float borné avec wrap
 */
float MathUtils_WrapFloat(float value, float min_val, float max_val);

/* ======================================================================== */
/*              PROTOTYPES - ARRONDI                                         */
/* ======================================================================== */

/**
 * @brief Arrondit à l'entier le plus proche
 * @param value     Valeur
 * @return          Entier arrondi
 */
int32_t MathUtils_Round(float value);

/**
 * @brief Arrondit à N décimales
 * @param value     Valeur
 * @param decimals  Nombre de décimales
 * @return          Float arrondi
 */
float MathUtils_RoundTo(float value, uint8_t decimals);

/**
 * @brief Tronque vers zéro
 * @param value     Valeur
 * @return          Partie entière
 */
int32_t MathUtils_Trunc(float value);

/**
 * @brief Partie fractionnaire
 * @param value     Valeur
 * @return          Partie fractionnaire (0.0 à 0.999...)
 */
float MathUtils_Frac(float value);

/* ======================================================================== */
/*              PROTOTYPES - CONVERSIONS                                    */
/* ======================================================================== */

/**
 * @brief Degrés → Radians
 * @param deg       Angle en degrés
 * @return          Angle en radians
 */
float MathUtils_DegToRad(float deg);

/**
 * @brief Radians → Degrés
 * @param rad       Angle en radians
 * @return          Angle en degrés
 */
float MathUtils_RadToDeg(float rad);

/**
 * @brief Normalise un angle entre 0 et 360°
 * @param deg       Angle en degrés
 * @return          Angle normalisé
 */
float MathUtils_NormalizeAngle(float deg);

/**
 * @brief Différence angulaire signée la plus courte
 * 
 * @param from      Angle de départ (degrés)
 * @param to        Angle d'arrivée (degrés)
 * @return          Différence en degrés (-180 à 180)
 */
float MathUtils_AngleDiff(float from, float to);

/**
 * @brief Coordonnées polaires → cartésiennes
 * 
 * @param r         Rayon
 * @param theta     Angle en radians
 * @param x         [out] Coordonnée X
 * @param y         [out] Coordonnée Y
 */
void MathUtils_PolarToCartesian(float r, float theta, float* x, float* y);

/**
 * @brief Coordonnées cartésiennes → polaires
 * 
 * @param x         Coordonnée X
 * @param y         Coordonnée Y
 * @param r         [out] Rayon
 * @param theta     [out] Angle en radians
 */
void MathUtils_CartesianToPolar(float x, float y, float* r, float* theta);

/**
 * @brief Mappe une valeur d'une plage à une autre
 * 
 * @param value     Valeur dans [in_min, in_max]
 * @param in_min    Plage entrée min
 * @param in_max    Plage entrée max
 * @param out_min   Plage sortie min
 * @param out_max   Plage sortie max
 * @return          Valeur mappée
 */
float MathUtils_Map(float value,
                    float in_min, float in_max,
                    float out_min, float out_max);

/**
 * @brief Mappe avec limitation (CLAMP)
 */
float MathUtils_MapConstrained(float value,
                               float in_min, float in_max,
                               float out_min, float out_max);

/* ======================================================================== */
/*              PROTOTYPES - ARITHMÉTIQUE                                   */
/* ======================================================================== */

/**
 * @brief Module toujours positif
 * 
 * Contrairement à %, retourne toujours une valeur positive.
 * Exemple : mod(-3, 10) = 7 (et non -3)
 * 
 * @param a         Dividende
 * @param b         Diviseur
 * @return          Reste positif
 */
int32_t MathUtils_Mod(int32_t a, int32_t b);

/**
 * @brief Puissance entière rapide
 * @param base      Base
 * @param exp       Exposant
 * @return          base^exp
 */
int32_t MathUtils_IPow(int32_t base, uint8_t exp);

/**
 * @brief Racine carrée rapide (approx Newton-Raphson)
 * @param value     Valeur
 * @return          Racine carrée approximative
 */
float MathUtils_FastSqrt(float value);

/**
 * @brief Racine carrée inverse rapide (Quake III)
 * @param value     Valeur
 * @return          1/sqrt(value)
 */
float MathUtils_FastInvSqrt(float value);

/**
 * @brief Moyenne d'un tableau
 * @param data      Tableau
 * @param count     Nombre d'éléments
 * @return          Moyenne
 */
float MathUtils_Average(const float* data, uint16_t count);

/**
 * @brief Médiane d'un tableau (modifie l'ordre)
 * @param data      Tableau (modifié)
 * @param count     Nombre d'éléments
 * @return          Médiane
 */
float MathUtils_Median(float* data, uint16_t count);

/**
 * @brief Interpolation linéaire
 * @param a         Valeur à t=0
 * @param b         Valeur à t=1
 * @param t         Facteur (0.0 à 1.0)
 * @return          Valeur interpolée
 */
float MathUtils_Lerp(float a, float b, float t);

/**
 * @brief Interpolation linéaire inverse
 * 
 * Retourne t tel que lerp(a, b, t) = value
 * 
 * @param a         Valeur à t=0
 * @param b         Valeur à t=1
 * @param value     Valeur cible
 * @return          Facteur t
 */
float MathUtils_InverseLerp(float a, float b, float value);

/* ======================================================================== */
/*              PROTOTYPES - STATISTIQUES                                   */
/* ======================================================================== */

/**
 * @brief Initialise un filtre moyenne glissante
 * 
 * @param filter    Filtre à initialiser
 * @param buffer    Buffer pour les échantillons (size * sizeof(float))
 * @param size      Taille de la fenêtre
 */
void MathUtils_RunningAverageInit(MathRunningAverage_t* filter,
                                  float* buffer, uint16_t size);

/**
 * @brief Ajoute un échantillon au filtre
 * @param filter    Filtre
 * @param sample    Nouvel échantillon
 * @return          Nouvelle moyenne
 */
float MathUtils_RunningAverageAdd(MathRunningAverage_t* filter, float sample);

/**
 * @brief Réinitialise le filtre
 * @param filter    Filtre
 */
void MathUtils_RunningAverageReset(MathRunningAverage_t* filter);

/**
 * @brief Initialise un détecteur min/max
 * @param mm        Détecteur
 */
void MathUtils_RunningMinMaxInit(MathRunningMinMax_t* mm);

/**
 * @brief Ajoute un échantillon au min/max
 * @param mm        Détecteur
 * @param sample    Échantillon
 */
void MathUtils_RunningMinMaxAdd(MathRunningMinMax_t* mm, float sample);

/**
 * @brief Réinitialise le min/max
 * @param mm        Détecteur
 */
void MathUtils_RunningMinMaxReset(MathRunningMinMax_t* mm);

/**
 * @brief Initialise un détecteur de pics
 * 
 * @param detector      Détecteur
 * @param threshold     Seuil de détection
 * @param dead_time_ms  Temps mort entre pics
 */
void MathUtils_PeakDetectorInit(MathPeakDetector_t* detector,
                                float threshold, uint32_t dead_time_ms);

/**
 * @brief Analyse un échantillon pour détecter un pic
 * 
 * @param detector  Détecteur
 * @param sample    Échantillon
 * @param timestamp Timestamp en ms
 * @return          true si pic détecté
 */
bool MathUtils_PeakDetectorProcess(MathPeakDetector_t* detector,
                                   float sample, uint32_t timestamp);

/* ======================================================================== */
/*              PROTOTYPES - FILTRES                                        */
/* ======================================================================== */

/**
 * @brief Initialise un filtre passe-bas exponentiel
 * 
 * @param filter    Filtre
 * @param alpha     Coefficient de lissage (0.0 = pas de filtrage, 1.0 = inertie max)
 */
void MathUtils_LowPassFilterInit(MathLowPassFilter_t* filter, float alpha);

/**
 * @brief Filtre un échantillon
 * @param filter    Filtre
 * @param sample    Échantillon brut
 * @return          Échantillon filtré
 */
float MathUtils_LowPassFilterProcess(MathLowPassFilter_t* filter, float sample);

/**
 * @brief Réinitialise le filtre
 * @param filter    Filtre
 */
void MathUtils_LowPassFilterReset(MathLowPassFilter_t* filter);

/* ======================================================================== */
/*              PROTOTYPES - SIGNAUX                                        */
/* ======================================================================== */

/**
 * @brief Sinus rapide via table lookup
 * 
 * Table de 256 entrées, interpolation linéaire.
 * Erreur < 0.1%
 * 
 * @param angle_rad Angle en radians
 * @return          Sinus approximatif
 */
float MathUtils_FastSin(float angle_rad);

/**
 * @brief Cosinus rapide
 * @param angle_rad Angle en radians
 * @return          Cosinus approximatif
 */
float MathUtils_FastCos(float angle_rad);

/**
 * @brief Bruit blanc simple
 * @param seed      Pointeur vers la graine (modifiée)
 * @return          Valeur entre -1.0 et 1.0
 */
float MathUtils_WhiteNoise(uint32_t* seed);

/* ======================================================================== */
/*              PROTOTYPES - ALÉATOIRE                                      */
/* ======================================================================== */

/**
 * @brief Initialise le générateur aléatoire
 * 
 * @param rng       Générateur
 * @param seed      Graine initiale (0 = basé sur ADC ou timer)
 */
void MathUtils_RandomInit(MathRandom_t* rng, uint32_t seed);

/**
 * @brief Génère un entier aléatoire 32 bits
 * @param rng       Générateur
 * @return          Nombre aléatoire
 */
uint32_t MathUtils_RandomU32(MathRandom_t* rng);

/**
 * @brief Génère un entier aléatoire dans [min, max]
 * @param rng       Générateur
 * @param min_val   Minimum inclus
 * @param max_val   Maximum inclus
 * @return          Nombre aléatoire
 */
int32_t MathUtils_RandomRange(MathRandom_t* rng, int32_t min_val, int32_t max_val);

/**
 * @brief Génère un float aléatoire dans [0.0, 1.0)
 * @param rng       Générateur
 * @return          Float aléatoire
 */
float MathUtils_RandomFloat(MathRandom_t* rng);

/* ======================================================================== */
/*              PROTOTYPES - DIVERS                                         */
/* ======================================================================== */

/**
 * @brief Calcule le pourcentage d'une valeur
 * @param value     Valeur
 * @param percent   Pourcentage
 * @return          value * percent / 100
 */
float MathUtils_PercentOf(float value, float percent);

/**
 * @brief Calcule le pourcentage entre deux valeurs
 * @param value     Valeur
 * @param total     Total
 * @return          Pourcentage (0-100)
 */
float MathUtils_PercentBetween(float value, float total);

/**
 * @brief Calcule la distance euclidienne 2D
 * @param x1, y1    Point 1
 * @param x2, y2    Point 2
 * @return          Distance
 */
float MathUtils_Distance2D(float x1, float y1, float x2, float y2);

/**
 * @brief Calcule l'hypoténuse (sqrt(x² + y²)) rapide
 * @param x         Composante X
 * @param y         Composante Y
 * @return          Hypoténuse
 */
float MathUtils_Hypot(float x, float y);

/**
 * @brief Résout une équation quadratique ax² + bx + c = 0
 * 
 * @param a, b, c   Coefficients
 * @param root1     [out] Première racine (si réelle)
 * @param root2     [out] Deuxième racine (si réelle)
 * @return          Nombre de racines réelles (0, 1, 2)
 */
int MathUtils_QuadraticSolver(float a, float b, float c,
                              float* root1, float* root2);

/**
 * @brief Compte le nombre de bits à 1 (population count)
 * @param value     Valeur
 * @return          Nombre de bits à 1
 */
uint8_t MathUtils_PopCount(uint32_t value);

/**
 * @brief Compte le nombre de zéros en tête (leading zeros)
 * @param value     Valeur
 * @return          Nombre de zéros
 */
uint8_t MathUtils_CLZ(uint32_t value);

/**
 * @brief Vérifie si un nombre est une puissance de 2
 * @param value     Valeur
 * @return          true si puissance de 2
 */
bool MathUtils_IsPowerOfTwo(uint32_t value);

/**
 * @brief Arrondit à la puissance de 2 supérieure
 * @param value     Valeur
 * @return          Prochaine puissance de 2
 */
uint32_t MathUtils_NextPowerOfTwo(uint32_t value);

/* ======================================================================== */
/*              MACROS UTILITAIRES                                           */
/* ======================================================================== */

/**
 * @brief Calcule le pourcentage (macro rapide)
 */
#define MATH_PERCENT(value, percent)        ((value) * (percent) / 100.0f)

/**
 * @brief Conversion degrés → radians (macro)
 */
#define MATH_DEG2RAD(deg)                   ((deg) * MATH_DEG_TO_RAD)

/**
 * @brief Conversion radians → degrés (macro)
 */
#define MATH_RAD2DEG(rad)                   ((rad) * MATH_RAD_TO_DEG)

/**
 * @brief Valeur absolue entier rapide
 */
#define MATH_IABS(x)                        ((x) < 0 ? -(x) : (x))

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */