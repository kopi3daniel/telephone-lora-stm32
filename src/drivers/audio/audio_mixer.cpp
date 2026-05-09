/**
 * @file audio_mixer.cpp
 * @brief Implémentation du mélangeur audio (mixer)
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans audio_mixer.h.
 * 
 * Il gère :
 * - Le mixage de N sources audio avec volumes individuels
 * - Les priorités entre sources
 * - Les fondus (fade in/out) et crossfades
 * - Le limiteur anti-saturation
 * - Les mesures de niveau (peak, RMS)
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "audio_mixer.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du mixer */
static AudioMixer_State mixer_state;

/** @brief Buffer temporaire pour le mixage */
static int32_t temp_buffer[AUDIO_DAC_BUFFER_SIZE];  // 32 bits pour éviter l'overflow

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le mixer audio
 */
bool audio_mixer_init(const AudioMixer_Config* config)
{
    AUDIO_MIXER_DEBUG("Initialisation du mixer audio...\n");
    
    memset(&mixer_state, 0, sizeof(AudioMixer_State));
    
    // Configuration par défaut
    mixer_state.config.masterVolume = AUDIO_MIXER_DEFAULT_MASTER;
    mixer_state.config.masterMute = false;
    mixer_state.config.balance = 50;
    mixer_state.config.enableLimiter = true;
    mixer_state.config.limiterThreshold = 30000;
    mixer_state.config.enableCrossfade = true;
    mixer_state.config.crossfadeMs = AUDIO_MIXER_DEFAULT_FADE_MS;
    
    // Appliquer la configuration utilisateur
    if (config != NULL)
    {
        mixer_state.config.masterVolume = config->masterVolume;
        mixer_state.config.masterMute = config->masterMute;
        mixer_state.config.balance = config->balance;
        mixer_state.config.enableLimiter = config->enableLimiter;
        mixer_state.config.limiterThreshold = config->limiterThreshold;
        mixer_state.config.enableCrossfade = config->enableCrossfade;
        mixer_state.config.crossfadeMs = config->crossfadeMs;
        
        // Copier les configurations des sources
        for (int i = 0; i < AUDIO_MIXER_MAX_SOURCES; i++)
        {
            memcpy(&mixer_state.config.sources[i], &config->sources[i], 
                   sizeof(AudioMixer_SourceConfig));
        }
    }
    else
    {
        // Initialiser les sources avec des valeurs par défaut
        for (int i = 0; i < AUDIO_MIXER_MAX_SOURCES; i++)
        {
            mixer_state.config.sources[i].type = (AudioSourceType)i;
            mixer_state.config.sources[i].priority = (AudioPriority)i;
            mixer_state.config.sources[i].volume = 80;
            mixer_state.config.sources[i].enabled = (i == AUDIO_SOURCE_VOICE);  // Seule la voix active
            mixer_state.config.sources[i].muted = false;
            mixer_state.config.sources[i].solo = false;
            mixer_state.config.sources[i].balance = 50;
            mixer_state.config.sources[i].fadeLevel = 1.0f;
            mixer_state.config.sources[i].fading = false;
        }
    }
    
    mixer_state.initialized = true;
    
    AUDIO_MIXER_DEBUG("Mixer initialisé (master=%d%%, sources=%d)\n",
                     mixer_state.config.masterVolume,
                     AUDIO_MIXER_MAX_SOURCES);
    
    return true;
}

/**
 * @brief Désinitialise le mixer
 */
void audio_mixer_deinit(void)
{
    mixer_state.initialized = false;
}

/**
 * @brief Vérifie si le mixer est prêt
 */
bool audio_mixer_is_ready(void)
{
    return mixer_state.initialized;
}

/**
 * @brief Récupère l'état
 */
AudioMixer_State* audio_mixer_get_state(void)
{
    return &mixer_state;
}

// ============================================================
// SECTION 2 : CONTRÔLE GLOBAL
// ============================================================

/**
 * @brief Définit le volume master
 */
void audio_mixer_set_master_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    mixer_state.config.masterVolume = volume;
}

/**
 * @brief Récupère le volume master
 */
uint8_t audio_mixer_get_master_volume(void)
{
    return mixer_state.config.masterVolume;
}

/**
 * @brief Active/désactive le muet global
 */
void audio_mixer_set_master_mute(bool mute)
{
    mixer_state.config.masterMute = mute;
}

/**
 * @brief Bascule le muet global
 */
void audio_mixer_toggle_master_mute(void)
{
    mixer_state.config.masterMute = !mixer_state.config.masterMute;
}

/**
 * @brief Définit la balance globale
 */
void audio_mixer_set_balance(uint8_t balance)
{
    if (balance > 100) balance = 100;
    mixer_state.config.balance = balance;
}

// ============================================================
// SECTION 3 : GESTION DES SOURCES
// ============================================================

/**
 * @brief Configure une source audio
 */
void audio_mixer_set_source(uint8_t sourceIndex, const AudioMixer_SourceConfig* config)
{
    if (sourceIndex >= AUDIO_MIXER_MAX_SOURCES || config == NULL) return;
    memcpy(&mixer_state.config.sources[sourceIndex], config, sizeof(AudioMixer_SourceConfig));
}

/**
 * @brief Active une source
 */
void audio_mixer_source_enable(uint8_t sourceIndex)
{
    if (sourceIndex >= AUDIO_MIXER_MAX_SOURCES) return;
    mixer_state.config.sources[sourceIndex].enabled = true;
    mixer_state.config.sources[sourceIndex].fadeLevel = 1.0f;
}

/**
 * @brief Désactive une source
 */
void audio_mixer_source_disable(uint8_t sourceIndex)
{
    if (sourceIndex >= AUDIO_MIXER_MAX_SOURCES) return;
    mixer_state.config.sources[sourceIndex].enabled = false;
    mixer_state.config.sources[sourceIndex].fadeLevel = 0.0f;
}

/**
 * @brief Définit le volume d'une source
 */
void audio_mixer_source_set_volume(uint8_t sourceIndex, uint8_t volume)
{
    if (sourceIndex >= AUDIO_MIXER_MAX_SOURCES) return;
    if (volume > 100) volume = 100;
    mixer_state.config.sources[sourceIndex].volume = volume;
}

/**
 * @brief Active/désactive le muet d'une source
 */
void audio_mixer_source_set_mute(uint8_t sourceIndex, bool mute)
{
    if (sourceIndex >= AUDIO_MIXER_MAX_SOURCES) return;
    mixer_state.config.sources[sourceIndex].muted = mute;
}

/**
 * @brief Active le mode solo
 */
void audio_mixer_source_set_solo(uint8_t sourceIndex, bool solo)
{
    if (sourceIndex >= AUDIO_MIXER_MAX_SOURCES) return;
    
    if (solo)
    {
        // Couper toutes les autres sources
        for (int i = 0; i < AUDIO_MIXER_MAX_SOURCES; i++)
        {
            mixer_state.config.sources[i].muted = (i != sourceIndex);
        }
    }
    else
    {
        // Réactiver toutes les sources
        for (int i = 0; i < AUDIO_MIXER_MAX_SOURCES; i++)
        {
            mixer_state.config.sources[i].muted = false;
        }
    }
    
    mixer_state.config.sources[sourceIndex].solo = solo;
}

/**
 * @brief Définit la priorité d'une source
 */
void audio_mixer_source_set_priority(uint8_t sourceIndex, AudioPriority priority)
{
    if (sourceIndex >= AUDIO_MIXER_MAX_SOURCES) return;
    mixer_state.config.sources[sourceIndex].priority = priority;
}

// ============================================================
// SECTION 4 : MIXAGE
// ============================================================

/**
 * @brief Mixe les sources audio dans le buffer de sortie
 */
void audio_mixer_process(int16_t* output, uint16_t sampleCount)
{
    if (!mixer_state.initialized || output == NULL) return;
    if (sampleCount > AUDIO_DAC_BUFFER_SIZE) sampleCount = AUDIO_DAC_BUFFER_SIZE;
    
    // 1. Initialiser le buffer temporaire à zéro
    memset(temp_buffer, 0, sampleCount * sizeof(int32_t));
    
    // 2. Trouver la source de plus haute priorité
    int8_t highestPrioritySource = -1;
    AudioPriority highestPriority = (AudioPriority)(AUDIO_PRIORITY_BACKGROUND + 1);
    
    for (int s = 0; s < AUDIO_MIXER_MAX_SOURCES; s++)
    {
        AudioMixer_SourceConfig* source = &mixer_state.config.sources[s];
        
        if (source->enabled && !source->muted)
        {
            if (source->priority < highestPriority)
            {
                highestPriority = source->priority;
                highestPrioritySource = s;
            }
        }
    }
    
    // 3. Mixer toutes les sources actives
    for (int s = 0; s < AUDIO_MIXER_MAX_SOURCES; s++)
    {
        AudioMixer_SourceConfig* source = &mixer_state.config.sources[s];
        
        if (!source->enabled || source->muted) continue;
        
        // Gérer le fondu
        float fadeVolume = 1.0f;
        if (source->fading)
        {
            uint32_t elapsed = HAL_GetTick() - source->fadeStartTime;
            
            if (elapsed >= source->fadeDurationMs)
            {
                source->fadeLevel = (source->fadeLevel > 0.5f) ? 1.0f : 0.0f;
                source->fading = false;
                
                if (source->fadeLevel < 0.01f)
                {
                    source->enabled = false;  // Désactiver après fade out
                }
            }
            else
            {
                float progress = (float)elapsed / source->fadeDurationMs;
                
                // Déterminer si c'est un fade in ou fade out
                if (source->fadeLevel < 0.5f)
                {
                    // Fade in : 0 → 1
                    source->fadeLevel = progress;
                }
                else
                {
                    // Fade out : 1 → 0
                    source->fadeLevel = 1.0f - progress;
                }
            }
            
            fadeVolume = source->fadeLevel;
        }
        
        // Calculer le gain : volume source × fade × balance
        float sourceGain = AUDIO_MIXER_VOLUME_TO_FACTOR(source->volume) * fadeVolume;
        
        // Réduire le gain des sources moins prioritaires
        if (highestPrioritySource >= 0 && s != highestPrioritySource)
        {
            if (source->priority > highestPriority)
            {
                sourceGain *= 0.3f;  // -10 dB pour les sources moins prioritaires
            }
        }
        
        // Appliquer le gain et ajouter au buffer temporaire
        int16_t* srcBuffer = mixer_state.sourceBuffers[s];
        
        for (uint16_t i = 0; i < sampleCount; i++)
        {
            temp_buffer[i] += (int32_t)(srcBuffer[i] * sourceGain);
        }
    }
    
    // 4. Appliquer le volume master
    float masterGain = AUDIO_MIXER_VOLUME_TO_FACTOR(mixer_state.config.masterVolume);
    
    if (mixer_state.config.masterMute)
    {
        masterGain = 0.0f;
    }
    
    // 5. Appliquer le limiteur et convertir en 16 bits
    int16_t peak = 0;
    int32_t rmsSum = 0;
    uint32_t clipCount = 0;
    
    for (uint16_t i = 0; i < sampleCount; i++)
    {
        // Appliquer le master volume
        int32_t sample = (int32_t)(temp_buffer[i] * masterGain);
        
        // Limiteur
        if (mixer_state.config.enableLimiter)
        {
            int16_t threshold = mixer_state.config.limiterThreshold;
            
            if (sample > threshold)
            {
                sample = threshold;
                clipCount++;
            }
            else if (sample < -threshold)
            {
                sample = -threshold;
                clipCount++;
            }
        }
        
        // Clamper à la plage 16 bits
        sample = AUDIO_MIXER_CLAMP_SAMPLE(sample);
        
        output[i] = (int16_t)sample;
        
        // Mesurer le peak
        int16_t absSample = abs((int16_t)sample);
        if (absSample > peak) peak = absSample;
        
        // Accumuler pour le RMS
        rmsSum += (int32_t)sample * sample;
    }
    
    // 6. Mettre à jour les statistiques
    mixer_state.peakLevel = peak;
    mixer_state.rmsLevel = (int16_t)sqrtf((float)rmsSum / sampleCount);
    mixer_state.clipping = (clipCount > 0);
    mixer_state.clippingCount += clipCount;
    mixer_state.totalSamplesMixed += sampleCount;
}

/**
 * @brief Écrit des données dans le buffer d'une source
 */
void audio_mixer_source_write(uint8_t sourceIndex, const int16_t* data, uint16_t sampleCount)
{
    if (sourceIndex >= AUDIO_MIXER_MAX_SOURCES || data == NULL) return;
    if (sampleCount > AUDIO_DAC_BUFFER_SIZE) sampleCount = AUDIO_DAC_BUFFER_SIZE;
    
    memcpy(mixer_state.sourceBuffers[sourceIndex], data, sampleCount * sizeof(int16_t));
}

/**
 * @brief Efface le buffer d'une source
 */
void audio_mixer_source_clear(uint8_t sourceIndex)
{
    if (sourceIndex >= AUDIO_MIXER_MAX_SOURCES) return;
    memset(mixer_state.sourceBuffers[sourceIndex], 0, AUDIO_DAC_BUFFER_SIZE * sizeof(int16_t));
}

// ============================================================
// SECTION 5 : FONDU (FADE)
// ============================================================

/**
 * @brief Démarre un fondu entrant (fade in)
 */
void audio_mixer_source_fade_in(uint8_t sourceIndex, uint32_t durationMs)
{
    if (sourceIndex >= AUDIO_MIXER_MAX_SOURCES) return;
    
    AudioMixer_SourceConfig* source = &mixer_state.config.sources[sourceIndex];
    
    source->enabled = true;
    source->fadeLevel = 0.0f;
    source->fadeStartTime = HAL_GetTick();
    source->fadeDurationMs = durationMs;
    source->fading = true;
    
    AUDIO_MIXER_DEBUG("Fade in source %d (%lu ms)\n", sourceIndex, (unsigned long)durationMs);
}

/**
 * @brief Démarre un fondu sortant (fade out)
 */
void audio_mixer_source_fade_out(uint8_t sourceIndex, uint32_t durationMs)
{
    if (sourceIndex >= AUDIO_MIXER_MAX_SOURCES) return;
    
    AudioMixer_SourceConfig* source = &mixer_state.config.sources[sourceIndex];
    
    source->fadeLevel = 1.0f;
    source->fadeStartTime = HAL_GetTick();
    source->fadeDurationMs = durationMs;
    source->fading = true;
    
    AUDIO_MIXER_DEBUG("Fade out source %d (%lu ms)\n", sourceIndex, (unsigned long)durationMs);
}

/**
 * @brief Crossfade entre deux sources
 */
void audio_mixer_crossfade(uint8_t sourceOut, uint8_t sourceIn, uint32_t durationMs)
{
    audio_mixer_source_fade_out(sourceOut, durationMs);
    audio_mixer_source_fade_in(sourceIn, durationMs);
    
    AUDIO_MIXER_DEBUG("Crossfade %d → %d (%lu ms)\n", sourceOut, sourceIn, (unsigned long)durationMs);
}

// ============================================================
// SECTION 6 : LIMITEUR
// ============================================================

void audio_mixer_limiter_enable(bool enable)
{
    mixer_state.config.enableLimiter = enable;
}

void audio_mixer_limiter_set_threshold(int16_t threshold)
{
    if (threshold < 0) threshold = 0;
    if (threshold > AUDIO_MIXER_MAX_SAMPLE) threshold = AUDIO_MIXER_MAX_SAMPLE;
    mixer_state.config.limiterThreshold = threshold;
}

bool audio_mixer_is_clipping(void)
{
    return mixer_state.clipping;
}

// ============================================================
// SECTION 7 : MESURES
// ============================================================

int16_t audio_mixer_get_peak_level(void)
{
    return mixer_state.peakLevel;
}

float audio_mixer_get_peak_db(void)
{
    if (mixer_state.peakLevel == 0) return -96.0f;
    return 20.0f * log10f((float)mixer_state.peakLevel / AUDIO_MIXER_MAX_SAMPLE);
}

int16_t audio_mixer_get_rms_level(void)
{
    return mixer_state.rmsLevel;
}

// ============================================================
// SECTION 8 : DÉBOGAGE
// ============================================================

void audio_mixer_print_state(void)
{
    printf("\n═══ ÉTAT MIXER AUDIO ═══\n");
    printf("Initialisé   : %s\n", mixer_state.initialized ? "Oui" : "Non");
    printf("Master Vol   : %d%%\n", mixer_state.config.masterVolume);
    printf("Master Mute  : %s\n", mixer_state.config.masterMute ? "Oui" : "Non");
    printf("Limiteur     : %s (seuil=%d)\n", 
           mixer_state.config.enableLimiter ? "ON" : "OFF",
           mixer_state.config.limiterThreshold);
    printf("Peak Level   : %d\n", mixer_state.peakLevel);
    printf("RMS Level    : %d\n", mixer_state.rmsLevel);
    printf("Peak dB      : %.1f dB\n", audio_mixer_get_peak_db());
    printf("Saturation   : %s (%lu fois)\n", 
           mixer_state.clipping ? "OUI !" : "Non",
           (unsigned long)mixer_state.clippingCount);
    printf("Échantillons : %lu\n", (unsigned long)mixer_state.totalSamplesMixed);
    printf("══════════════════════\n\n");
}

void audio_mixer_print_sources(void)
{
    printf("\n═══ SOURCES AUDIO ═══\n");
    
    for (int i = 0; i < AUDIO_MIXER_MAX_SOURCES; i++)
    {
        AudioMixer_SourceConfig* src = &mixer_state.config.sources[i];
        
        const char* typeStr = "Inconnu";
        switch (src->type)
        {
            case AUDIO_SOURCE_VOICE: typeStr = "Voix"; break;
            case AUDIO_SOURCE_TONE:  typeStr = "Tonalité"; break;
            case AUDIO_SOURCE_ALERT: typeStr = "Alerte"; break;
            case AUDIO_SOURCE_MIC:   typeStr = "Micro"; break;
            case AUDIO_SOURCE_TEST:  typeStr = "Test"; break;
        }
        
        const char* prioStr = "Inconnu";
        switch (src->priority)
        {
            case AUDIO_PRIORITY_EMERGENCY:  prioStr = "URGENCE"; break;
            case AUDIO_PRIORITY_CALL:       prioStr = "Appel"; break;
            case AUDIO_PRIORITY_RINGTONE:   prioStr = "Sonnerie"; break;
            case AUDIO_PRIORITY_ALERT:      prioStr = "Alerte"; break;
            case AUDIO_PRIORITY_VOICE:      prioStr = "Voix"; break;
            case AUDIO_PRIORITY_BACKGROUND: prioStr = "Fond"; break;
        }
        
        printf("[%d] %-12s | Prio:%-12s | Vol:%3d%% | %s %s %s | Fade:%.1f\n",
               i, typeStr, prioStr, src->volume,
               src->enabled ? "ON" : "OFF",
               src->muted ? "MUTE" : "",
               src->solo ? "SOLO" : "",
               src->fadeLevel);
    }
    printf("══════════════════════\n\n");
}

void audio_mixer_print_levels(void)
{
    printf("[MIXER] Peak:%d RMS:%d Clip:%s\n",
           mixer_state.peakLevel, mixer_state.rmsLevel,
           mixer_state.clipping ? "YES" : "no");
}

bool audio_mixer_self_test(void)
{
    AUDIO_MIXER_DEBUG("Auto-test...\n");
    
    if (!mixer_state.initialized)
    {
        AUDIO_MIXER_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test 1 : Mixage simple
    int16_t testData[128];
    int16_t output[128];
    
    // Remplir avec une sinusoïde
    for (int i = 0; i < 128; i++)
    {
        testData[i] = (int16_t)(sinf(2.0f * M_PI * 440 * i / 8000) * 10000);
    }
    
    audio_mixer_source_write(AUDIO_SOURCE_VOICE, testData, 128);
    audio_mixer_process(output, 128);
    
    // Vérifier que la sortie n'est pas nulle
    bool hasSound = false;
    for (int i = 0; i < 128; i++)
    {
        if (output[i] != 0)
        {
            hasSound = true;
            break;
        }
    }
    
    if (!hasSound)
    {
        AUDIO_MIXER_DEBUG("Échec test 1 : pas de son\n");
        return false;
    }
    
    AUDIO_MIXER_DEBUG("Auto-test OK\n");
    return true;
}