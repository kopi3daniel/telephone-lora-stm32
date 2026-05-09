/**
 * @file audio_mixer.h
 * @brief Mélangeur audio (mixer) pour combiner plusieurs sources
 * 
 * Ce fichier gère le mixage de plusieurs sources audio :
 * - Audio reçu via LoRa (voix)
 * - Tonalités générées (DTMF, sonnerie)
 * - Alertes et notifications
 * - Silence
 * 
 * Fonctionnalités :
 * - Mixage de N sources avec volume individuel
 * - Contrôle de volume global
 * - Balance stéréo (si disponible)
 * - Fondu entrant/sortant (fade in/out)
 * - Transition en douceur entre les sources
 * - Limiteur pour éviter la saturation
 * 
 * Architecture :
 * ┌──────────┐   ┌──────────┐   ┌──────────┐
 * │ Source 1 │   │ Source 2 │   │ Source 3 │
 * │ (Voix)   │   │ (DTMF)   │   │ (Alerte) │
 * └─────┬────┘   └─────┬────┘   └─────┬────┘
 *       │              │              │
 *       ▼              ▼              ▼
 * ┌─────────────────────────────────────────┐
 * │            AUDIO MIXER                  │
 * │  ┌─────┐  ┌─────┐  ┌─────┐  ┌───────┐ │
 * │  │Vol.1│  │Vol.2│  │Vol.3│  │Master │ │
 * │  └──┬──┘  └──┬──┘  └──┬──┘  │Volume │ │
 * │     └────────┼────────┘     └───┬───┘ │
 * │              ▼                  ▼      │
 * │         [Addition] ──────► [Limiteur] │
 * └─────────────────────────────────────────┘
 *                     │
 *                     ▼
 *              Sortie audio (DAC)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "audio_dac.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define AUDIO_MIXER_VERSION             "1.0.0"

/** @brief Nombre maximum de sources audio */
#define AUDIO_MIXER_MAX_SOURCES         5

/** @brief Nombre maximum de canaux */
#define AUDIO_MIXER_MAX_CHANNELS        2       // Stéréo

/** @brief Volume master par défaut (%) */
#define AUDIO_MIXER_DEFAULT_MASTER      80

/** @brief Valeur maximale d'un échantillon mixé */
#define AUDIO_MIXER_MAX_SAMPLE          32767

/** @brief Valeur minimale d'un échantillon mixé */
#define AUDIO_MIXER_MIN_SAMPLE          -32768

/** @brief Durée de fondu par défaut (ms) */
#define AUDIO_MIXER_DEFAULT_FADE_MS     50

// ============================================================
// SECTION 2 : TYPES DE SOURCES
// ============================================================

/**
 * @brief Types de sources audio disponibles
 */
typedef enum {
    AUDIO_SOURCE_VOICE      = 0,    // Voix (réception LoRa)
    AUDIO_SOURCE_TONE       = 1,    // Tonalités (DTMF, sonnerie)
    AUDIO_SOURCE_ALERT      = 2,    // Alertes et notifications
    AUDIO_SOURCE_MIC        = 3,    // Microphone (monitoring)
    AUDIO_SOURCE_TEST       = 4     // Signal de test
} AudioSourceType;

/**
 * @brief Priorité des sources (0 = plus haute priorité)
 */
typedef enum {
    AUDIO_PRIORITY_EMERGENCY = 0,   // Urgence (priorité absolue)
    AUDIO_PRIORITY_CALL     = 1,    // Appel en cours
    AUDIO_PRIORITY_RINGTONE = 2,    // Sonnerie
    AUDIO_PRIORITY_ALERT    = 3,    // Alerte/Notification
    AUDIO_PRIORITY_VOICE    = 4,    // Voix
    AUDIO_PRIORITY_BACKGROUND = 5   // Fond (musique, etc.)
} AudioPriority;

// ============================================================
// SECTION 3 : CONFIGURATION D'UNE SOURCE
// ============================================================

/**
 * @brief Configuration d'une source audio
 */
typedef struct {
    AudioSourceType type;           // Type de source
    AudioPriority priority;         // Priorité
    uint8_t volume;                 // Volume (0-100)
    bool enabled;                   // Source active
    bool muted;                     // Source muette
    bool solo;                      // Mode solo (coupe les autres)
    uint8_t balance;                // Balance (0=gauche, 50=centre, 100=droite)
    float fadeLevel;                // Niveau de fondu actuel (0.0-1.0)
    uint32_t fadeStartTime;        // Début du fondu
    uint32_t fadeDurationMs;       // Durée du fondu
    bool fading;                    // Fondu en cours
} AudioMixer_SourceConfig;

// ============================================================
// SECTION 4 : CONFIGURATION GLOBALE
// ============================================================

/**
 * @brief Configuration du mixer
 */
typedef struct {
    uint8_t masterVolume;           // Volume master (0-100)
    bool masterMute;                // Muet global
    uint8_t balance;                // Balance globale
    bool enableLimiter;             // Activer le limiteur
    int16_t limiterThreshold;       // Seuil du limiteur
    bool enableCrossfade;           // Activer le crossfade
    uint16_t crossfadeMs;          // Durée du crossfade
    AudioMixer_SourceConfig sources[AUDIO_MIXER_MAX_SOURCES];  // Sources
} AudioMixer_Config;

// ============================================================
// SECTION 5 : ÉTAT DU MIXER
// ============================================================

/**
 * @brief État du mixer audio
 */
typedef struct {
    bool initialized;               // Mixer initialisé
    AudioMixer_Config config;       // Configuration
    
    // Buffers de mixage
    int16_t mixBuffer[AUDIO_DAC_BUFFER_SIZE];       // Buffer de mixage
    int16_t sourceBuffers[AUDIO_MIXER_MAX_SOURCES][AUDIO_DAC_BUFFER_SIZE];  // Buffers par source
    
    // Niveaux
    int16_t peakLevel;              // Niveau crête actuel
    int16_t rmsLevel;               // Niveau RMS actuel
    bool clipping;                  // Saturation détectée
    
    // Statistiques
    uint32_t totalSamplesMixed;     // Nombre d'échantillons mixés
    uint32_t clippingCount;         // Nombre de saturations
} AudioMixer_State;

// ============================================================
// SECTION 6 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise le mixer audio
 * @param config Configuration (NULL = défaut)
 * @return true si succès
 */
bool audio_mixer_init(const AudioMixer_Config* config);

/**
 * @brief Désinitialise le mixer
 */
void audio_mixer_deinit(void);

/**
 * @brief Vérifie si le mixer est prêt
 * @return true si initialisé
 */
bool audio_mixer_is_ready(void);

/**
 * @brief Récupère l'état du mixer
 */
AudioMixer_State* audio_mixer_get_state(void);

// ============================================================
// SECTION 7 : FONCTIONS DE CONTRÔLE GLOBAL
// ============================================================

/**
 * @brief Définit le volume master
 * @param volume Volume (0-100)
 */
void audio_mixer_set_master_volume(uint8_t volume);

/**
 * @brief Récupère le volume master
 * @return Volume (0-100)
 */
uint8_t audio_mixer_get_master_volume(void);

/**
 * @brief Active/désactive le muet global
 */
void audio_mixer_set_master_mute(bool mute);

/**
 * @brief Bascule le muet global
 */
void audio_mixer_toggle_master_mute(void);

/**
 * @brief Définit la balance globale
 * @param balance Balance (0=gauche, 50=centre, 100=droite)
 */
void audio_mixer_set_balance(uint8_t balance);

// ============================================================
// SECTION 8 : FONCTIONS DE GESTION DES SOURCES
// ============================================================

/**
 * @brief Configure une source audio
 * @param sourceIndex Index de la source (0 à MAX_SOURCES-1)
 * @param config Configuration
 */
void audio_mixer_set_source(uint8_t sourceIndex, const AudioMixer_SourceConfig* config);

/**
 * @brief Active une source
 */
void audio_mixer_source_enable(uint8_t sourceIndex);

/**
 * @brief Désactive une source
 */
void audio_mixer_source_disable(uint8_t sourceIndex);

/**
 * @brief Définit le volume d'une source
 */
void audio_mixer_source_set_volume(uint8_t sourceIndex, uint8_t volume);

/**
 * @brief Active/désactive le muet d'une source
 */
void audio_mixer_source_set_mute(uint8_t sourceIndex, bool mute);

/**
 * @brief Active le mode solo pour une source
 */
void audio_mixer_source_set_solo(uint8_t sourceIndex, bool solo);

/**
 * @brief Définit la priorité d'une source
 */
void audio_mixer_source_set_priority(uint8_t sourceIndex, AudioPriority priority);

// ============================================================
// SECTION 9 : FONCTIONS DE MIXAGE
// ============================================================

/**
 * @brief Mixe les sources audio dans le buffer de sortie
 * 
 * Combine toutes les sources actives en tenant compte
 * des volumes, priorités et fondus.
 * 
 * @param output Buffer de sortie (échantillons 16 bits)
 * @param sampleCount Nombre d'échantillons
 */
void audio_mixer_process(int16_t* output, uint16_t sampleCount);

/**
 * @brief Écrit des données dans le buffer d'une source
 * @param sourceIndex Index de la source
 * @param data Données audio
 * @param sampleCount Nombre d'échantillons
 */
void audio_mixer_source_write(uint8_t sourceIndex, const int16_t* data, uint16_t sampleCount);

/**
 * @brief Efface le buffer d'une source (remplit de silence)
 */
void audio_mixer_source_clear(uint8_t sourceIndex);

// ============================================================
// SECTION 10 : FONCTIONS DE FONDU (FADE)
// ============================================================

/**
 * @brief Démarre un fondu entrant sur une source
 * @param sourceIndex Index de la source
 * @param durationMs Durée du fondu
 */
void audio_mixer_source_fade_in(uint8_t sourceIndex, uint32_t durationMs);

/**
 * @brief Démarre un fondu sortant sur une source
 * @param sourceIndex Index de la source
 * @param durationMs Durée du fondu
 */
void audio_mixer_source_fade_out(uint8_t sourceIndex, uint32_t durationMs);

/**
 * @brief Démarre un crossfade entre deux sources
 * @param sourceOut Index de la source qui disparaît
 * @param sourceIn Index de la source qui apparaît
 * @param durationMs Durée du crossfade
 */
void audio_mixer_crossfade(uint8_t sourceOut, uint8_t sourceIn, uint32_t durationMs);

// ============================================================
// SECTION 11 : FONCTIONS DE LIMITEUR
// ============================================================

/**
 * @brief Active/désactive le limiteur
 */
void audio_mixer_limiter_enable(bool enable);

/**
 * @brief Définit le seuil du limiteur
 * @param threshold Seuil (0-32767)
 */
void audio_mixer_limiter_set_threshold(int16_t threshold);

/**
 * @brief Vérifie si une saturation s'est produite
 * @return true si saturation
 */
bool audio_mixer_is_clipping(void);

// ============================================================
// SECTION 12 : FONCTIONS DE MESURE
// ============================================================

/**
 * @brief Récupère le niveau crête actuel
 * @return Niveau crête (0-32767)
 */
int16_t audio_mixer_get_peak_level(void);

/**
 * @brief Récupère le niveau crête en dB
 * @return Niveau en dB (négatif, 0 = max)
 */
float audio_mixer_get_peak_db(void);

/**
 * @brief Récupère le niveau RMS actuel
 * @return Niveau RMS (0-32767)
 */
int16_t audio_mixer_get_rms_level(void);

// ============================================================
// SECTION 13 : FONCTIONS DE DÉBOGAGE
// ============================================================

void audio_mixer_print_state(void);
void audio_mixer_print_sources(void);
void audio_mixer_print_levels(void);
bool audio_mixer_self_test(void);

// ============================================================
// SECTION 14 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Applique un gain à un échantillon
 */
#define AUDIO_MIXER_APPLY_GAIN(sample, gain) \
    ((int16_t)(((int32_t)(sample) * (gain)) / 100))

/**
 * @brief Limite un échantillon à la plage 16 bits
 */
#define AUDIO_MIXER_CLAMP_SAMPLE(sample) \
    ((sample) > AUDIO_MIXER_MAX_SAMPLE ? AUDIO_MIXER_MAX_SAMPLE : \
     (sample) < AUDIO_MIXER_MIN_SAMPLE ? AUDIO_MIXER_MIN_SAMPLE : (sample))

/**
 * @brief Convertit un volume linéaire (0-100) en facteur (0.0-1.0)
 */
#define AUDIO_MIXER_VOLUME_TO_FACTOR(vol)   ((float)(vol) / 100.0f)

/**
 * @brief Vérifie si une source est active
 */
#define AUDIO_MIXER_SOURCE_IS_ACTIVE(state, idx) \
    ((state)->config.sources[idx].enabled && !(state)->config.sources[idx].muted)

// ============================================================
// SECTION 15 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define AUDIO_MIXER_DEBUG(fmt, ...) printf("[AUDIO_MIXER] " fmt, ##__VA_ARGS__)
#else
    #define AUDIO_MIXER_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 16 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // AUDIO_MIXER_H