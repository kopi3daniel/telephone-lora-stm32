/**
 * @file audio_dac.h
 * @brief Driver de lecture audio via le DAC (haut-parleur)
 * 
 * Ce fichier gère la lecture audio vers le haut-parleur
 * connecté à la sortie analogique DAC_OUT2 (PA5).
 * 
 * Caractéristiques :
 * - Résolution : 12 bits (0-4095)
 * - Fréquence d'échantillonnage : 8 kHz (configurable)
 * - Mode : Continu avec DMA double buffering
 * - Contrôle de volume (0-100%)
 * - Mode muet (mute)
 * - Génération de tonalités (sinus, carré, DTMF)
 * 
 * Architecture DMA double buffering :
 * ┌─────────────────────────────────────────────────────────┐
 * │ Buffer A (128 échantillons) │ Buffer B (128 échantillons)│
 * │ ← DMA lit en continu →      │                           │
 * │                             │ ← DMA lit en continu →   │
 * └─────────────────────────────────────────────────────────┘
 * 
 * Pendant que le DMA lit le buffer B,
 * le CPU peut remplir le buffer A (et vice-versa).
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef AUDIO_DAC_H
#define AUDIO_DAC_H

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
#define AUDIO_DAC_VERSION               "1.0.0"

/** @brief Fréquence d'échantillonnage par défaut (Hz) */
#define AUDIO_DAC_DEFAULT_SAMPLE_RATE   8000

/** @brief Résolution du DAC (bits) */
#define AUDIO_DAC_RESOLUTION            12

/** @brief Valeur maximale DAC */
#define AUDIO_DAC_MAX_VALUE             4095

/** @brief Valeur médiane (silence) */
#define AUDIO_DAC_SILENCE_VALUE         2048

/** @brief Taille du buffer DMA (échantillons) */
#define AUDIO_DAC_BUFFER_SIZE           128

/** @brief Nombre de buffers DMA */
#define AUDIO_DAC_BUFFER_COUNT          2

/** @brief Taille totale du buffer DMA */
#define AUDIO_DAC_DMA_BUFFER_SIZE       (AUDIO_DAC_BUFFER_SIZE * AUDIO_DAC_BUFFER_COUNT)

/** @brief Canal DAC utilisé */
#define AUDIO_DAC_CHANNEL               DAC_CHANNEL_2

/** @brief Instance DAC */
#define AUDIO_DAC_INSTANCE              DAC1

/** @brief Pin du haut-parleur */
#define AUDIO_DAC_PORT                  GPIOA
#define AUDIO_DAC_PIN                   GPIO_PIN_5

/** @brief Volume par défaut (%) */
#define AUDIO_DAC_DEFAULT_VOLUME        80

/** @brief Volume minimum */
#define AUDIO_DAC_MIN_VOLUME            0

/** @brief Volume maximum */
#define AUDIO_DAC_MAX_VOLUME            100

// ============================================================
// SECTION 2 : CONFIGURATION
// ============================================================

/**
 * @brief Configuration de la lecture audio
 */
typedef struct {
    uint32_t sampleRate;            // Fréquence d'échantillonnage (Hz)
    uint16_t bufferSize;            // Taille du buffer (échantillons)
    bool enableDoubleBuffering;     // Double buffering DMA
    uint8_t volume;                 // Volume initial (0-100)
    bool startMuted;                // Démarrer en mode muet
    uint8_t balance;                // Balance (0=gauche, 50=centre, 100=droite)
    bool enableSoftStart;           // Démarrage progressif (anti-pop)
} AudioDAC_Config;

// ============================================================
// SECTION 3 : ÉTAT
// ============================================================

/**
 * @brief État de la lecture audio
 */
typedef struct {
    bool initialized;               // Driver initialisé
    bool playing;                   // Lecture en cours
    bool bufferReady;               // Buffer prêt à être rempli
    uint8_t activeBuffer;           // Buffer actif (0 ou 1)
    
    // Buffers DMA
    uint16_t dmaBuffer[AUDIO_DAC_DMA_BUFFER_SIZE];  // Buffer DMA complet
    uint16_t* bufferA;              // Pointeur vers buffer A
    uint16_t* bufferB;              // Pointeur vers buffer B
    
    // Volume et contrôle
    uint8_t volume;                 // Volume (0-100)
    bool muted;                     // Mode muet
    uint8_t balance;                // Balance (0-100)
    bool softStartActive;           // Soft start en cours
    
    // Statistiques
    uint32_t totalSamples;          // Nombre total d'échantillons lus
    uint32_t underruns;             // Nombre de sous-dépassements
    uint32_t lastPlayTime;          // Dernière lecture
    
    // Configuration
    AudioDAC_Config config;         // Configuration actuelle
} AudioDAC_State;

// ============================================================
// SECTION 4 : CALLBACKS
// ============================================================

/**
 * @brief Callback appelé quand un buffer doit être rempli
 * @param buffer Pointeur vers le buffer à remplir
 * @param size Nombre d'échantillons à fournir
 * 
 * L'application doit remplir ce buffer avec les données audio.
 */
typedef void (*AudioDAC_BufferCallback)(uint16_t* buffer, uint16_t size);

/**
 * @brief Callback appelé quand la lecture est terminée
 */
typedef void (*AudioDAC_PlaybackDoneCallback)(void);

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise le driver DAC audio
 * @param config Configuration (NULL = défaut)
 * @return true si succès
 */
bool audio_dac_init(const AudioDAC_Config* config);

/**
 * @brief Désinitialise le driver
 */
void audio_dac_deinit(void);

/**
 * @brief Vérifie si le driver est prêt
 * @return true si initialisé
 */
bool audio_dac_is_ready(void);

/**
 * @brief Récupère l'état du driver
 * @return Pointeur vers l'état
 */
AudioDAC_State* audio_dac_get_state(void);

// ============================================================
// SECTION 6 : FONCTIONS DE CONTRÔLE
// ============================================================

/**
 * @brief Démarre la lecture audio
 */
void audio_dac_start(void);

/**
 * @brief Arrête la lecture audio
 */
void audio_dac_stop(void);

/**
 * @brief Vérifie si la lecture est en cours
 * @return true si en cours
 */
bool audio_dac_is_playing(void);

/**
 * @brief Met en pause la lecture
 */
void audio_dac_pause(void);

/**
 * @brief Reprend la lecture
 */
void audio_dac_resume(void);

// ============================================================
// SECTION 7 : FONCTIONS DE LECTURE
// ============================================================

/**
 * @brief Vérifie si un buffer peut être rempli
 * @return true si un buffer est disponible
 */
bool audio_dac_is_buffer_ready(void);

/**
 * @brief Remplit le buffer audio avec des données
 * @param data Données audio (12 bits)
 * @param size Nombre d'échantillons
 * @return true si le buffer a été rempli
 */
bool audio_dac_write_buffer(const uint16_t* data, uint16_t size);

/**
 * @brief Écrit un échantillon unique
 * @param sample Valeur DAC 12 bits
 */
void audio_dac_write_sample(uint16_t sample);

/**
 * @brief Remplit le buffer avec du silence
 */
void audio_dac_write_silence(void);

// ============================================================
// SECTION 8 : FONCTIONS DE VOLUME
// ============================================================

/**
 * @brief Définit le volume
 * @param volume Volume (0-100)
 */
void audio_dac_set_volume(uint8_t volume);

/**
 * @brief Récupère le volume
 * @return Volume (0-100)
 */
uint8_t audio_dac_get_volume(void);

/**
 * @brief Active/désactive le mode muet
 * @param mute true = muet
 */
void audio_dac_set_mute(bool mute);

/**
 * @brief Bascule le mode muet
 */
void audio_dac_toggle_mute(void);

/**
 * @brief Vérifie si le mode muet est actif
 * @return true si muet
 */
bool audio_dac_is_muted(void);

/**
 * @brief Définit la balance
 * @param balance Balance (0=gauche, 50=centre, 100=droite)
 */
void audio_dac_set_balance(uint8_t balance);

// ============================================================
// SECTION 9 : FONCTIONS DE GÉNÉRATION DE TONALITÉS
// ============================================================

/**
 * @brief Joue une tonalité sinusoïdale
 * @param frequency Fréquence en Hz
 * @param durationMs Durée en ms (0 = continu)
 * @param amplitude Amplitude (0-100)
 */
void audio_dac_play_tone_sine(uint16_t frequency, uint32_t durationMs, uint8_t amplitude);

/**
 * @brief Joue une tonalité carrée
 * @param frequency Fréquence en Hz
 * @param durationMs Durée en ms
 * @param amplitude Amplitude (0-100)
 */
void audio_dac_play_tone_square(uint16_t frequency, uint32_t durationMs, uint8_t amplitude);

/**
 * @brief Joue une tonalité DTMF (téléphone)
 * @param digit Chiffre DTMF ('0'-'9', '*', '#', 'A'-'D')
 * @param durationMs Durée en ms
 */
void audio_dac_play_dtmf(char digit, uint32_t durationMs);

/**
 * @brief Joue une mélodie de sonnerie
 * @param melodyIndex Index de la mélodie (0 = défaut)
 */
void audio_dac_play_ringtone(uint8_t melodyIndex);

/**
 * @brief Arrête la tonalité en cours
 */
void audio_dac_stop_tone(void);

// ============================================================
// SECTION 10 : FONCTIONS DE CONFIGURATION
// ============================================================

/**
 * @brief Définit la fréquence d'échantillonnage
 * @param sampleRate Fréquence en Hz
 */
void audio_dac_set_sample_rate(uint32_t sampleRate);

/**
 * @brief Active/désactive le soft start (anti-pop)
 * @param enable true pour activer
 */
void audio_dac_soft_start_enable(bool enable);

// ============================================================
// SECTION 11 : FONCTIONS DE CALLBACKS
// ============================================================

/**
 * @brief Enregistre le callback de buffer à remplir
 * @param callback Fonction à appeler
 */
void audio_dac_set_buffer_callback(AudioDAC_BufferCallback callback);

/**
 * @brief Enregistre le callback de fin de lecture
 * @param callback Fonction à appeler
 */
void audio_dac_set_done_callback(AudioDAC_PlaybackDoneCallback callback);

// ============================================================
// SECTION 12 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état du driver
 */
void audio_dac_print_state(void);

/**
 * @brief Affiche les statistiques
 */
void audio_dac_print_statistics(void);

/**
 * @brief Test de fonctionnement
 * @return true si OK
 */
bool audio_dac_self_test(void);

// ============================================================
// SECTION 13 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Applique le volume à un échantillon
 * @param sample Échantillon brut
 * @param volume Volume (0-100)
 * @return Échantillon ajusté
 */
#define AUDIO_DAC_APPLY_VOLUME(sample, volume) \
    ((uint16_t)(((uint32_t)(sample) * (volume)) / 100))

/**
 * @brief Convertit un niveau (0-100) en valeur DAC
 * @param level Niveau (0-100)
 * @return Valeur DAC 12 bits
 */
#define AUDIO_DAC_LEVEL_TO_SAMPLE(level) \
    ((uint16_t)(((uint32_t)(level) * AUDIO_DAC_MAX_VALUE) / 100))

/**
 * @brief Vérifie si le volume est à zéro
 */
#define AUDIO_DAC_IS_SILENT()           (adc_state.volume == 0 || adc_state.muted)

// ============================================================
// SECTION 14 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define AUDIO_DAC_DEBUG(fmt, ...)   printf("[AUDIO_DAC] " fmt, ##__VA_ARGS__)
#else
    #define AUDIO_DAC_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 15 : FRÉQUENCES DTMF
// ============================================================

/**
 * @brief Table des fréquences DTMF
 * 
 *         1209 Hz  1336 Hz  1477 Hz  1633 Hz
 * 697 Hz    1        2        3        A
 * 770 Hz    4        5        6        B
 * 852 Hz    7        8        9        C
 * 941 Hz    *        0        #        D
 */

/** @brief Fréquences basses DTMF */
static const uint16_t DTMF_LOW_FREQ[] = {697, 770, 852, 941};

/** @brief Fréquences hautes DTMF */
static const uint16_t DTMF_HIGH_FREQ[] = {1209, 1336, 1477, 1633};

/**
 * @brief Mapping des touches DTMF
 */
static const uint8_t DTMF_MAP[4][4] = {
    {1, 2, 3, 10},     // 10 = A
    {4, 5, 6, 11},     // 11 = B
    {7, 8, 9, 12},     // 12 = C
    {20, 0, 21, 13}    // 20 = *, 21 = #, 13 = D
};

// ============================================================
// SECTION 16 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // AUDIO_DAC_H