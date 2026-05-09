/**
 * @file audio_adc.h
 * @brief Driver de capture audio via l'ADC (microphone)
 * 
 * Ce fichier gère la capture audio à partir du microphone
 * connecté à l'entrée analogique ADC1_IN0 (PA0).
 * 
 * Caractéristiques :
 * - Résolution : 12 bits (0-4095)
 * - Fréquence d'échantillonnage : 8 kHz (configurable)
 * - Mode : Continu avec DMA double buffering
 * - Filtrage optionnel (médian, moyenne)
 * - Détection de niveau sonore (VU meter)
 * 
 * Architecture DMA double buffering :
 * ┌─────────────────────────────────────────────────────────┐
 * │ Buffer A (128 échantillons) │ Buffer B (128 échantillons)│
 * │ ← DMA écrit en continu →    │                           │
 * │                             │ ← DMA écrit en continu →  │
 * └─────────────────────────────────────────────────────────┘
 * 
 * Pendant que le DMA remplit le buffer B,
 * le CPU peut traiter le buffer A (et vice-versa).
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef AUDIO_ADC_H
#define AUDIO_ADC_H

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

/** @brief Version du driver */
#define AUDIO_ADC_VERSION               "1.0.0"

/** @brief Fréquence d'échantillonnage par défaut (Hz) */
#define AUDIO_ADC_DEFAULT_SAMPLE_RATE   8000

/** @brief Fréquences supportées */
#define AUDIO_ADC_SAMPLE_RATE_8K        8000
#define AUDIO_ADC_SAMPLE_RATE_16K       16000
#define AUDIO_ADC_SAMPLE_RATE_22K       22050
#define AUDIO_ADC_SAMPLE_RATE_44K       44100

/** @brief Résolution de l'ADC (bits) */
#define AUDIO_ADC_RESOLUTION            12

/** @brief Valeur maximale ADC */
#define AUDIO_ADC_MAX_VALUE             4095

/** @brief Valeur médiane (silence) */
#define AUDIO_ADC_SILENCE_VALUE         2048

/** @brief Taille du buffer DMA (échantillons) */
#define AUDIO_ADC_BUFFER_SIZE           128

/** @brief Nombre de buffers DMA (double buffering) */
#define AUDIO_ADC_BUFFER_COUNT          2

/** @brief Taille totale du buffer DMA en octets */
#define AUDIO_ADC_DMA_BUFFER_SIZE       (AUDIO_ADC_BUFFER_SIZE * AUDIO_ADC_BUFFER_COUNT)

/** @brief Canal ADC utilisé */
#define AUDIO_ADC_CHANNEL               ADC_CHANNEL_0

/** @brief Instance ADC */
#define AUDIO_ADC_INSTANCE              ADC1

/** @brief Pin du microphone */
#define AUDIO_ADC_PORT                  GPIOA
#define AUDIO_ADC_PIN                   GPIO_PIN_0

// ============================================================
// SECTION 2 : CONFIGURATION
// ============================================================

/**
 * @brief Configuration de la capture audio
 */
typedef struct {
    uint32_t sampleRate;            // Fréquence d'échantillonnage (Hz)
    uint16_t bufferSize;            // Taille du buffer (échantillons)
    bool enableDoubleBuffering;     // Double buffering DMA
    bool enableMedianFilter;        // Filtre médian
    bool enableAverageFilter;       // Filtre moyenne
    uint8_t filterWindow;           // Taille de la fenêtre de filtrage
    uint8_t gain;                   // Gain (1-100, 100 = pas de gain)
    bool enableVUMeter;             // Activer le VU meter
    uint8_t vuMeterDecay;           // Décroissance du VU meter (ms)
} AudioADC_Config;

// ============================================================
// SECTION 3 : ÉTAT
// ============================================================

/**
 * @brief État de la capture audio
 */
typedef struct {
    bool initialized;               // Driver initialisé
    bool recording;                 // Capture en cours
    bool bufferReady;               // Buffer prêt à être traité
    uint8_t activeBuffer;           // Buffer actif (0 ou 1)
    
    // Buffers DMA
    uint16_t dmaBuffer[AUDIO_ADC_DMA_BUFFER_SIZE];  // Buffer DMA complet
    uint16_t* bufferA;              // Pointeur vers buffer A
    uint16_t* bufferB;              // Pointeur vers buffer B
    
    // Statistiques
    uint32_t totalSamples;          // Nombre total d'échantillons
    uint32_t overflows;             // Nombre de débordements
    uint32_t lastSampleTime;        // Dernier échantillonnage
    
    // VU Meter
    uint16_t vuLevel;               // Niveau actuel (0-4095)
    uint16_t vuPeak;                // Niveau crête
    uint32_t vuDecayTime;           // Temps de décroissance
    
    // Configuration
    AudioADC_Config config;         // Configuration actuelle
} AudioADC_State;

// ============================================================
// SECTION 4 : CALLBACKS
// ============================================================

/**
 * @brief Callback appelé quand un buffer est plein
 * @param buffer Pointeur vers le buffer plein
 * @param size Nombre d'échantillons
 */
typedef void (*AudioADC_BufferCallback)(uint16_t* buffer, uint16_t size);

/**
 * @brief Callback appelé quand le niveau VU change
 * @param level Niveau actuel (0-100)
 * @param peak Niveau crête (0-100)
 */
typedef void (*AudioADC_VUCallback)(uint8_t level, uint8_t peak);

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise le driver ADC audio
 * @param config Configuration (NULL = défaut)
 * @return true si succès
 */
bool audio_adc_init(const AudioADC_Config* config);

/**
 * @brief Désinitialise le driver
 */
void audio_adc_deinit(void);

/**
 * @brief Vérifie si le driver est prêt
 * @return true si initialisé
 */
bool audio_adc_is_ready(void);

/**
 * @brief Récupère l'état du driver
 * @return Pointeur vers l'état
 */
AudioADC_State* audio_adc_get_state(void);

// ============================================================
// SECTION 6 : FONCTIONS DE CONTRÔLE
// ============================================================

/**
 * @brief Démarre la capture audio
 */
void audio_adc_start(void);

/**
 * @brief Arrête la capture audio
 */
void audio_adc_stop(void);

/**
 * @brief Vérifie si la capture est en cours
 * @return true si en cours
 */
bool audio_adc_is_recording(void);

/**
 * @brief Met en pause la capture
 */
void audio_adc_pause(void);

/**
 * @brief Reprend la capture
 */
void audio_adc_resume(void);

// ============================================================
// SECTION 7 : FONCTIONS DE LECTURE
// ============================================================

/**
 * @brief Vérifie si un buffer est disponible
 * @return true si un buffer plein est prêt
 */
bool audio_adc_is_buffer_ready(void);

/**
 * @brief Récupère le buffer audio courant
 * @param buffer Pointeur vers le buffer (sortie)
 * @param size Nombre d'échantillons (sortie)
 * @return true si un buffer a été lu
 */
bool audio_adc_get_buffer(uint16_t** buffer, uint16_t* size);

/**
 * @brief Lit un échantillon unique (bloquant)
 * @return Valeur ADC 12 bits
 */
uint16_t audio_adc_read_sample(void);

/**
 * @brief Lit plusieurs échantillons (bloquant)
 * @param buffer Buffer de destination
 * @param count Nombre d'échantillons
 */
void audio_adc_read_samples(uint16_t* buffer, uint16_t count);

// ============================================================
// SECTION 8 : FONCTIONS DE TRAITEMENT
// ============================================================

/**
 * @brief Convertit une valeur ADC en niveau (0-100)
 * @param sample Valeur ADC 12 bits
 * @return Niveau (0-100)
 */
uint8_t audio_adc_to_level(uint16_t sample);

/**
 * @brief Applique un gain au buffer
 * @param buffer Buffer à modifier
 * @param size Nombre d'échantillons
 * @param gain Gain (1-100)
 */
void audio_adc_apply_gain(uint16_t* buffer, uint16_t size, uint8_t gain);

/**
 * @brief Filtre le buffer (médian)
 * @param buffer Buffer à filtrer
 * @param size Nombre d'échantillons
 * @param window Taille de la fenêtre
 */
void audio_adc_median_filter(uint16_t* buffer, uint16_t size, uint8_t window);

/**
 * @brief Filtre le buffer (moyenne)
 * @param buffer Buffer à filtrer
 * @param size Nombre d'échantillons
 * @param window Taille de la fenêtre
 */
void audio_adc_average_filter(uint16_t* buffer, uint16_t size, uint8_t window);

/**
 * @brief Détecte le niveau VU
 * @param buffer Buffer audio
 * @param size Nombre d'échantillons
 * @return Niveau VU (0-100)
 */
uint8_t audio_adc_calculate_vu(const uint16_t* buffer, uint16_t size);

// ============================================================
// SECTION 9 : FONCTIONS DE CONFIGURATION
// ============================================================

/**
 * @brief Définit la fréquence d'échantillonnage
 * @param sampleRate Fréquence en Hz
 */
void audio_adc_set_sample_rate(uint32_t sampleRate);

/**
 * @brief Définit le gain
 * @param gain Gain (1-100)
 */
void audio_adc_set_gain(uint8_t gain);

/**
 * @brief Active/désactive le filtre médian
 * @param enable true pour activer
 */
void audio_adc_filter_median_enable(bool enable);

/**
 * @brief Active/désactive le filtre moyenne
 * @param enable true pour activer
 */
void audio_adc_filter_average_enable(bool enable);

// ============================================================
// SECTION 10 : FONCTIONS DE CALLBACKS
// ============================================================

/**
 * @brief Enregistre le callback de buffer plein
 * @param callback Fonction à appeler
 */
void audio_adc_set_buffer_callback(AudioADC_BufferCallback callback);

/**
 * @brief Enregistre le callback de VU meter
 * @param callback Fonction à appeler
 */
void audio_adc_set_vu_callback(AudioADC_VUCallback callback);

// ============================================================
// SECTION 11 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état du driver
 */
void audio_adc_print_state(void);

/**
 * @brief Affiche les statistiques
 */
void audio_adc_print_statistics(void);

/**
 * @brief Test de fonctionnement
 * @return true si OK
 */
bool audio_adc_self_test(void);

// ============================================================
// SECTION 12 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Convertit une valeur ADC en tension
 * @param adc Valeur ADC 12 bits
 * @return Tension en millivolts
 */
#define AUDIO_ADC_TO_MV(adc)            ((uint32_t)(adc) * 3300 / AUDIO_ADC_MAX_VALUE)

/**
 * @brief Vérifie si le niveau dépasse un seuil
 * @param sample Échantillon
 * @param threshold Seuil (0-4095)
 */
#define AUDIO_ADC_ABOVE_THRESHOLD(sample, threshold)  ((sample) > (threshold))

/**
 * @brief Calcule la valeur absolue par rapport au silence
 */
#define AUDIO_ADC_ABS_SAMPLE(sample)    ((sample) > AUDIO_ADC_SILENCE_VALUE ? \
                                         (sample) - AUDIO_ADC_SILENCE_VALUE : \
                                         AUDIO_ADC_SILENCE_VALUE - (sample))

// ============================================================
// SECTION 13 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define AUDIO_ADC_DEBUG(fmt, ...)   printf("[AUDIO_ADC] " fmt, ##__VA_ARGS__)
#else
    #define AUDIO_ADC_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 14 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // AUDIO_ADC_H