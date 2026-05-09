/**
 * @file xpt2046_defs.h
 * @brief Définitions complètes pour le contrôleur tactile XPT2046
 * 
 * Ce fichier contient toutes les définitions nécessaires pour
 * utiliser le contrôleur tactile XPT2046 :
 * - Commandes de conversion
 * - Registres de configuration
 * - Paramètres de calibration
 * - Structures de données
 * 
 * Le XPT2046 est un contrôleur tactile résistif 4 fils
 * compatible avec l'écran ILI9488 3.5".
 * 
 * Communication : SPI (ou I2C selon la configuration)
 * Résolution : 12 bits (4096 × 4096)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef XPT2046_DEFS_H
#define XPT2046_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// SECTION 1 : IDENTIFICATION
// ============================================================

/** @brief Version du driver */
#define XPT2046_DRIVER_VERSION          "1.0.0"

/** @brief Adresse I2C par défaut (si mode I2C) */
#define XPT2046_I2C_ADDRESS             0x38

/** @brief Fréquence SPI maximale */
#define XPT2046_SPI_MAX_FREQ            2000000     // 2 MHz

/** @brief Fréquence I2C */
#define XPT2046_I2C_FREQ                400000      // 400 kHz (Fast Mode)

// ============================================================
// SECTION 2 : COMMANDES DE CONVERSION
// ============================================================

/**
 * @brief Commandes de conversion SPI
 * 
 * Format d'une commande (8 bits) :
 * ┌───────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
 * │ START │ A2  │ A1  │ A0  │ MODE│ SER │ PD1 │ PD0 │
 * │   1   │     │     │     │     │     │     │     │
 * └───────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
 * 
 * START : Toujours 1 (début de commande)
 * A2-A0 : Canal à convertir
 * MODE  : 0 = 12 bits, 1 = 8 bits
 * SER   : 0 = différentiel, 1 = single-ended
 * PD1-0 : Mode puissance (00 = power down, 11 = always on)
 */

/** @brief Bit START (toujours à 1) */
#define XPT2046_START                   0x80

/** @brief Sélection du canal */
#define XPT2046_CHANNEL_X               0x10    // Mesure X
#define XPT2046_CHANNEL_Y               0x50    // Mesure Y
#define XPT2046_CHANNEL_Z1              0x30    // Pression Z1
#define XPT2046_CHANNEL_Z2              0x40    // Pression Z2
#define XPT2046_CHANNEL_TEMP0           0x00    // Température 0
#define XPT2046_CHANNEL_TEMP1           0x08    // Température 1
#define XPT2046_CHANNEL_VBAT            0x20    // Batterie
#define XPT2046_CHANNEL_AUX             0x60    // Entrée auxiliaire

/** @brief Masque du canal */
#define XPT2046_CHANNEL_MASK            0x70

/** @brief Résolution 12 bits */
#define XPT2046_RESOLUTION_12BIT        0x00
/** @brief Résolution 8 bits (plus rapide) */
#define XPT2046_RESOLUTION_8BIT         0x08

/** @brief Mode single-ended (référencé à GND) */
#define XPT2046_SINGLE_ENDED            0x04
/** @brief Mode différentiel (plus précis) */
#define XPT2046_DIFFERENTIAL            0x00

/** @brief Mode puissance */
#define XPT2046_POWER_DOWN              0x00    // Power down entre conversions
#define XPT2046_POWER_REF_OFF           0x01    // Référence OFF
#define XPT2046_POWER_REF_ON            0x02    // Référence ON
#define XPT2046_POWER_ALWAYS_ON         0x03    // Toujours allumé

/** @brief Masque du mode puissance */
#define XPT2046_POWER_MASK              0x03

// ============================================================
// SECTION 3 : COMMANDES PRÉDÉFINIES
// ============================================================

/**
 * @brief Commandes prêtes à l'emploi
 */

/** @brief Lire la position X (12 bits, différentiel, always on) */
#define XPT2046_CMD_READ_X              (XPT2046_START | XPT2046_CHANNEL_X | \
                                         XPT2046_RESOLUTION_12BIT | XPT2046_DIFFERENTIAL | \
                                         XPT2046_POWER_ALWAYS_ON)

/** @brief Lire la position Y (12 bits, différentiel, always on) */
#define XPT2046_CMD_READ_Y              (XPT2046_START | XPT2046_CHANNEL_Y | \
                                         XPT2046_RESOLUTION_12BIT | XPT2046_DIFFERENTIAL | \
                                         XPT2046_POWER_ALWAYS_ON)

/** @brief Lire la pression Z1 */
#define XPT2046_CMD_READ_Z1             (XPT2046_START | XPT2046_CHANNEL_Z1 | \
                                         XPT2046_RESOLUTION_12BIT | XPT2046_DIFFERENTIAL | \
                                         XPT2046_POWER_ALWAYS_ON)

/** @brief Lire la pression Z2 */
#define XPT2046_CMD_READ_Z2             (XPT2046_START | XPT2046_CHANNEL_Z2 | \
                                         XPT2046_RESOLUTION_12BIT | XPT2046_DIFFERENTIAL | \
                                         XPT2046_POWER_ALWAYS_ON)

/** @brief Lire X en 8 bits (plus rapide, moins précis) */
#define XPT2046_CMD_READ_X_FAST         (XPT2046_START | XPT2046_CHANNEL_X | \
                                         XPT2046_RESOLUTION_8BIT | XPT2046_DIFFERENTIAL | \
                                         XPT2046_POWER_ALWAYS_ON)

/** @brief Lire Y en 8 bits */
#define XPT2046_CMD_READ_Y_FAST         (XPT2046_START | XPT2046_CHANNEL_Y | \
                                         XPT2046_RESOLUTION_8BIT | XPT2046_DIFFERENTIAL | \
                                         XPT2046_POWER_ALWAYS_ON)

// ============================================================
// SECTION 4 : PARAMÈTRES DE LECTURE
// ============================================================

/** @brief Nombre d'échantillons pour la moyenne */
#define XPT2046_SAMPLES_DEFAULT         5

/** @brief Nombre minimum d'échantillons */
#define XPT2046_SAMPLES_MIN             1

/** @brief Nombre maximum d'échantillons */
#define XPT2046_SAMPLES_MAX             20

/** @brief Seuil de détection de toucher (pression) */
#define XPT2046_TOUCH_THRESHOLD         100

/** @brief Seuil de stabilité (écart max entre échantillons) */
#define XPT2046_STABILITY_THRESHOLD     50

/** @brief Délai entre les lectures (ms) */
#define XPT2046_READ_DELAY_MS           1

/** @brief Délai entre les scans (ms) */
#define XPT2046_SCAN_INTERVAL_MS        20

/** @brief Timeout de conversion SPI (ms) */
#define XPT2046_SPI_TIMEOUT_MS          100

/** @brief Valeur maximale ADC (12 bits) */
#define XPT2046_ADC_MAX                 4095

/** @brief Valeur maximale ADC (8 bits) */
#define XPT2046_ADC_MAX_8BIT            255

// ============================================================
// SECTION 5 : ÉTATS DU TACTILE
// ============================================================

/**
 * @brief États possibles de l'écran tactile
 */
typedef enum {
    XPT2046_STATE_IDLE      = 0,    // En attente (pas de toucher)
    XPT2046_STATE_PRESSED   = 1,    // Écran touché
    XPT2046_STATE_HELD      = 2,    // Toucher maintenu
    XPT2046_STATE_RELEASED  = 3,    // Toucher relâché
    XPT2046_STATE_ERROR     = 4     // Erreur de lecture
} XPT2046_TouchState;

/**
 * @brief Événements tactiles
 */
typedef enum {
    XPT2046_EVENT_NONE      = 0,    // Aucun événement
    XPT2046_EVENT_PRESS     = 1,    // Appui détecté
    XPT2046_EVENT_MOVE      = 2,    // Déplacement
    XPT2046_EVENT_RELEASE   = 3,    // Relâchement
    XPT2046_EVENT_HOLD      = 4     // Maintien (> 500ms)
} XPT2046_TouchEvent;

// ============================================================
// SECTION 6 : STRUCTURES DE DONNÉES
// ============================================================

/**
 * @brief Point de coordonnées brutes (ADC)
 */
typedef struct {
    uint16_t x;                     // Position X brute (0-4095)
    uint16_t y;                     // Position Y brute (0-4095)
    uint16_t z;                     // Pression (0-4095, 0 = pas de toucher)
} XPT2046_RawPoint;

/**
 * @brief Point de coordonnées calibrées (pixels)
 */
typedef struct {
    uint16_t x;                     // Position X en pixels (0-319)
    uint16_t y;                     // Position Y en pixels (0-479)
    bool valid;                     // Point valide
} XPT2046_PixelPoint;

/**
 * @brief Configuration de calibration
 * 
 * La calibration se fait avec 3 points :
 * - Point A : coin supérieur gauche
 * - Point B : coin supérieur droit
 * - Point C : coin inférieur gauche
 */
typedef struct {
    // Points de calibration (coordonnées écran)
    uint16_t screenAx, screenAy;    // Point A écran
    uint16_t screenBx, screenBy;    // Point B écran
    uint16_t screenCx, screenCy;    // Point C écran
    
    // Points de calibration (valeurs ADC)
    uint16_t adcAx, adcAy;          // Point A ADC
    uint16_t adcBx, adcBy;          // Point B ADC
    uint16_t adcCx, adcCy;          // Point C ADC
    
    // Coefficients calculés
    float alphaX, betaX, deltaX;    // Coefficients pour X
    float alphaY, betaY, deltaY;    // Coefficients pour Y
    
    bool calibrated;                // Calibration effectuée
} XPT2046_Calibration;

/**
 * @brief Configuration du contrôleur tactile
 */
typedef struct {
    // Interface
    bool useI2C;                    // true = I2C, false = SPI
    uint8_t i2cAddress;             // Adresse I2C
    
    // Lecture
    uint8_t samples;                // Nombre d'échantillons
    uint16_t touchThreshold;        // Seuil de détection
    uint16_t stabilityThreshold;    // Seuil de stabilité
    uint8_t readDelayMs;            // Délai entre lectures
    uint8_t scanIntervalMs;         // Intervalle de scan
    
    // Filtrage
    bool enableMedianFilter;        // Filtre médian
    bool enableAverageFilter;       // Filtre moyenne
    uint8_t filterWindow;           // Taille fenêtre de filtrage
    
    // Calibration
    XPT2046_Calibration calibration; // Données de calibration
    
    // Orientation
    bool swapXY;                    // Échanger X et Y
    bool invertX;                   // Inverser X
    bool invertY;                   // Inverser Y
} XPT2046_Config;

/**
 * @brief État du contrôleur tactile
 */
typedef struct {
    XPT2046_TouchState state;       // État actuel
    XPT2046_TouchEvent lastEvent;   // Dernier événement
    XPT2046_RawPoint rawPoint;      // Point brut actuel
    XPT2046_PixelPoint pixelPoint;  // Point calibré actuel
    XPT2046_PixelPoint lastPoint;   // Dernier point valide
    uint32_t pressTime;             // Timestamp de l'appui
    uint32_t releaseTime;           // Timestamp du relâchement
    uint32_t holdDuration;          // Durée du maintien
    bool touched;                   // Écran actuellement touché
    uint32_t touchCount;            // Nombre total de touchés
    uint32_t errorCount;            // Nombre d'erreurs
} XPT2046_State;

// ============================================================
// SECTION 7 : GESTES RECONNUS
// ============================================================

/**
 * @brief Types de gestes détectés
 */
typedef enum {
    XPT2046_GESTURE_NONE        = 0,    // Pas de geste
    XPT2046_GESTURE_TAP         = 1,    // Tap simple
    XPT2046_GESTURE_DOUBLE_TAP  = 2,    // Double tap
    XPT2046_GESTURE_LONG_PRESS  = 3,    // Appui long
    XPT2046_GESTURE_SWIPE_LEFT  = 4,    // Glissement gauche
    XPT2046_GESTURE_SWIPE_RIGHT = 5,    // Glissement droit
    XPT2046_GESTURE_SWIPE_UP    = 6,    // Glissement haut
    XPT2046_GESTURE_SWIPE_DOWN  = 7,    // Glissement bas
    XPT2046_GESTURE_PINCH_IN    = 8,    // Pincement (zoom out)
    XPT2046_GESTURE_PINCH_OUT   = 9     // Écartement (zoom in)
} XPT2046_Gesture;

/**
 * @brief Configuration de la détection de gestes
 */
typedef struct {
    bool enableGestures;            // Activer la détection
    uint16_t tapTimeout;            // Timeout tap (ms)
    uint16_t doubleTapTimeout;      // Timeout double tap (ms)
    uint16_t longPressTimeout;      // Timeout appui long (ms)
    uint16_t swipeThreshold;        // Seuil glissement (pixels)
    uint16_t pinchThreshold;        // Seuil pincement (pixels)
} XPT2046_GestureConfig;

// ============================================================
// SECTION 8 : CODES D'ERREUR
// ============================================================

/**
 * @brief Codes d'erreur du driver tactile
 */
typedef enum {
    XPT2046_OK                  = 0,    // Succès
    XPT2046_ERROR_INIT          = -1,   // Échec initialisation
    XPT2046_ERROR_SPI           = -2,   // Erreur communication SPI
    XPT2046_ERROR_I2C           = -3,   // Erreur communication I2C
    XPT2046_ERROR_NOT_PRESENT   = -4,   // Contrôleur non détecté
    XPT2046_ERROR_TIMEOUT       = -5,   // Timeout
    XPT2046_ERROR_CALIBRATION   = -6,   // Erreur calibration
    XPT2046_ERROR_PARAM         = -7,   // Paramètre invalide
    XPT2046_ERROR_NO_TOUCH      = -8    // Pas de toucher (normal)
} XPT2046_Error;

// ============================================================
// SECTION 9 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Convertit une valeur ADC 12 bits en pixels X
 * @param adc Valeur ADC brute
 * @return Position en pixels
 */
#define XPT2046_ADC_TO_PIXEL_X(adc, cal) \
    ((uint16_t)(cal.alphaX * (adc) + cal.betaX * (adc) + cal.deltaX))

/**
 * @brief Vérifie si une pression est valide
 * @param z Valeur de pression
 * @return true si l'écran est touché
 */
#define XPT2046_IS_TOUCHED(z)           ((z) > XPT2046_TOUCH_THRESHOLD)

/**
 * @brief Calcule la distance entre deux points
 */
#define XPT2046_DISTANCE(x1, y1, x2, y2) \
    ((uint16_t)sqrtf((float)((x2)-(x1))*((x2)-(x1)) + ((y2)-(y1))*((y2)-(y1))))

// ============================================================
// SECTION 10 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define XPT2046_DEBUG(fmt, ...)     printf("[XPT2046] " fmt, ##__VA_ARGS__)
#else
    #define XPT2046_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 11 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // XPT2046_DEFS_H