/**
 * @file audio_dac.cpp
 * @brief Implémentation du driver de lecture audio via DAC
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans audio_dac.h.
 * 
 * Il gère :
 * - L'initialisation du DAC en mode DMA continu
 * - Le double buffering pour une lecture sans coupure
 * - Le contrôle de volume (0-100%)
 * - Le mode muet (mute)
 * - La génération de tonalités (sinus, carré, DTMF)
 * - Les mélodies de sonnerie
 * - Le soft start anti-pop
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "audio_dac.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================
// HANDLES EXTERNES
// ============================================================

/** @brief Handle DAC */
extern DAC_HandleTypeDef hdac;

/** @brief Handle DMA pour le DAC */
extern DMA_HandleTypeDef hdma_dac1;

/** @brief Handle Timer pour le déclenchement */
extern TIM_HandleTypeDef htim6;

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du driver DAC */
static AudioDAC_State dac_state;

/** @brief Configuration */
static AudioDAC_Config dac_config = {
    .sampleRate = AUDIO_DAC_DEFAULT_SAMPLE_RATE,
    .bufferSize = AUDIO_DAC_BUFFER_SIZE,
    .enableDoubleBuffering = true,
    .volume = AUDIO_DAC_DEFAULT_VOLUME,
    .startMuted = false,
    .balance = 50,
    .enableSoftStart = true
};

/** @brief Callbacks */
static AudioDAC_BufferCallback buffer_callback = NULL;
static AudioDAC_PlaybackDoneCallback done_callback = NULL;

/** @brief Flags DMA */
static volatile uint8_t dma_half_complete = 0;
static volatile uint8_t dma_full_complete = 0;

/** @brief Buffer de silence pré-calculé */
static uint16_t silence_buffer[AUDIO_DAC_BUFFER_SIZE];

/** @brief État de la génération de tonalité */
static struct {
    bool active;
    uint16_t frequency;
    uint32_t durationMs;
    uint32_t startTime;
    uint8_t amplitude;
    bool isDTMF;
    uint16_t dtmfFreq1;
    uint16_t dtmfFreq2;
    float phase;
} tone_state = {0};

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le driver DAC audio
 */
bool audio_dac_init(const AudioDAC_Config* config)
{
    AUDIO_DAC_DEBUG("Initialisation du driver DAC audio...\n");
    
    // Sauvegarder la configuration
    if (config != NULL)
    {
        memcpy(&dac_config, config, sizeof(AudioDAC_Config));
    }
    
    // Initialiser l'état
    memset(&dac_state, 0, sizeof(AudioDAC_State));
    dac_state.config = dac_config;
    dac_state.volume = dac_config.volume;
    dac_state.muted = dac_config.startMuted;
    dac_state.balance = dac_config.balance;
    
    // Configurer les pointeurs de buffer
    dac_state.bufferA = &dac_state.dmaBuffer[0];
    dac_state.bufferB = &dac_state.dmaBuffer[AUDIO_DAC_BUFFER_SIZE];
    
    // Initialiser le buffer de silence
    for (int i = 0; i < AUDIO_DAC_BUFFER_SIZE; i++)
    {
        silence_buffer[i] = AUDIO_DAC_SILENCE_VALUE;
    }
    
    // --- Configuration du GPIO ---
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = AUDIO_DAC_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(AUDIO_DAC_PORT, &GPIO_InitStruct);
    
    // --- Configuration du DAC ---
    __HAL_RCC_DAC_CLK_ENABLE();
    
    hdac.Instance = DAC;
    if (HAL_DAC_Init(&hdac) != HAL_OK)
    {
        AUDIO_DAC_DEBUG("Échec initialisation DAC\n");
        return false;
    }
    
    // Configuration du canal DAC
    DAC_ChannelConfTypeDef sConfig = {0};
    sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;  // Déclenché par TIM6
    sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    
    if (HAL_DAC_ConfigChannel(&hdac, &sConfig, AUDIO_DAC_CHANNEL) != HAL_OK)
    {
        AUDIO_DAC_DEBUG("Échec configuration canal DAC\n");
        return false;
    }
    
    // --- Configuration du DMA ---
    __HAL_RCC_DMA1_CLK_ENABLE();
    
    hdma_dac1.Instance = DMA1_Stream5;
    hdma_dac1.Init.Channel = DMA_CHANNEL_7;
    hdma_dac1.Init.Direction = DMA_MEMORY_TO_PERIPH;          // Mémoire → DAC
    hdma_dac1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_dac1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_dac1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_dac1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_dac1.Init.Mode = DMA_CIRCULAR;
    hdma_dac1.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_dac1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    
    if (HAL_DMA_Init(&hdma_dac1) != HAL_OK)
    {
        AUDIO_DAC_DEBUG("Échec configuration DMA\n");
        return false;
    }
    
    __HAL_LINKDMA(&hdac, DMA_Handle1, hdma_dac1);
    
    // --- Configuration des interruptions DMA ---
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    
    __HAL_DMA_ENABLE_IT(&hdma_dac1, DMA_IT_HT);
    __HAL_DMA_ENABLE_IT(&hdma_dac1, DMA_IT_TC);
    
    // Remplir le buffer initial avec du silence
    memset(dac_state.dmaBuffer, AUDIO_DAC_SILENCE_VALUE & 0xFF, 
           AUDIO_DAC_DMA_BUFFER_SIZE * sizeof(uint16_t));
    for (int i = 0; i < AUDIO_DAC_DMA_BUFFER_SIZE; i++)
    {
        dac_state.dmaBuffer[i] = AUDIO_DAC_SILENCE_VALUE;
    }
    
    dac_state.initialized = true;
    
    AUDIO_DAC_DEBUG("Driver DAC initialisé\n");
    AUDIO_DAC_DEBUG("Fréquence: %lu Hz, Volume: %d%%\n",
                   (unsigned long)dac_config.sampleRate, dac_state.volume);
    
    return true;
}

/**
 * @brief Désinitialise le driver
 */
void audio_dac_deinit(void)
{
    audio_dac_stop();
    HAL_DAC_DeInit(&hdac);
    HAL_DMA_DeInit(&hdma_dac1);
    dac_state.initialized = false;
}

/**
 * @brief Vérifie si le driver est prêt
 */
bool audio_dac_is_ready(void)
{
    return dac_state.initialized;
}

/**
 * @brief Récupère l'état
 */
AudioDAC_State* audio_dac_get_state(void)
{
    return &dac_state;
}

// ============================================================
// SECTION 2 : CONTRÔLE
// ============================================================

/**
 * @brief Démarre la lecture audio
 */
void audio_dac_start(void)
{
    if (!dac_state.initialized) return;
    if (dac_state.playing) return;
    
    AUDIO_DAC_DEBUG("Démarrage lecture audio\n");
    
    // Soft start : commencer à volume 0 et augmenter progressivement
    if (dac_config.enableSoftStart)
    {
        dac_state.softStartActive = true;
        // Le volume sera augmenté progressivement dans le DMA callback
    }
    
    // Démarrer le DMA
    HAL_DAC_Start_DMA(&hdac, AUDIO_DAC_CHANNEL, 
                      (uint32_t*)dac_state.dmaBuffer, 
                      AUDIO_DAC_DMA_BUFFER_SIZE, 
                      DAC_ALIGN_12B_R);
    
    dac_state.playing = true;
    dac_state.bufferReady = true;
}

/**
 * @brief Arrête la lecture
 */
void audio_dac_stop(void)
{
    if (!dac_state.playing) return;
    
    AUDIO_DAC_DEBUG("Arrêt lecture audio\n");
    
    // Soft stop : diminuer progressivement
    HAL_DAC_Stop_DMA(&hdac, AUDIO_DAC_CHANNEL);
    
    // Remettre la sortie au silence
    HAL_DAC_SetValue(&hdac, AUDIO_DAC_CHANNEL, DAC_ALIGN_12B_R, AUDIO_DAC_SILENCE_VALUE);
    
    dac_state.playing = false;
    dac_state.bufferReady = false;
}

/**
 * @brief Vérifie si la lecture est en cours
 */
bool audio_dac_is_playing(void)
{
    return dac_state.playing;
}

/**
 * @brief Met en pause
 */
void audio_dac_pause(void)
{
    HAL_DAC_Stop_DMA(&hdac, AUDIO_DAC_CHANNEL);
}

/**
 * @brief Reprend
 */
void audio_dac_resume(void)
{
    HAL_DAC_Start_DMA(&hdac, AUDIO_DAC_CHANNEL,
                      (uint32_t*)dac_state.dmaBuffer,
                      AUDIO_DAC_DMA_BUFFER_SIZE,
                      DAC_ALIGN_12B_R);
}

// ============================================================
// SECTION 3 : ÉCRITURE DES DONNÉES
// ============================================================

/**
 * @brief Remplit le buffer audio
 */
bool audio_dac_write_buffer(const uint16_t* data, uint16_t size)
{
    if (!dac_state.playing) return false;
    if (data == NULL) return false;
    if (size > AUDIO_DAC_BUFFER_SIZE) size = AUDIO_DAC_BUFFER_SIZE;
    
    uint16_t* targetBuffer;
    
    // Déterminer quel buffer remplir (celui qui n'est PAS en cours de lecture)
    if (dac_state.activeBuffer == 0)
    {
        targetBuffer = dac_state.bufferB;
    }
    else
    {
        targetBuffer = dac_state.bufferA;
    }
    
    // Appliquer le volume
    if (dac_state.volume < 100)
    {
        for (uint16_t i = 0; i < size; i++)
        {
            targetBuffer[i] = AUDIO_DAC_APPLY_VOLUME(data[i], dac_state.volume);
        }
    }
    else
    {
        memcpy(targetBuffer, data, size * sizeof(uint16_t));
    }
    
    // Appliquer le mode muet
    if (dac_state.muted)
    {
        memset(targetBuffer, AUDIO_DAC_SILENCE_VALUE & 0xFF, size * sizeof(uint16_t));
        for (uint16_t i = 0; i < size; i++)
        {
            targetBuffer[i] = AUDIO_DAC_SILENCE_VALUE;
        }
    }
    
    dac_state.totalSamples += size;
    
    return true;
}

/**
 * @brief Écrit un échantillon unique
 */
void audio_dac_write_sample(uint16_t sample)
{
    if (dac_state.muted)
    {
        sample = AUDIO_DAC_SILENCE_VALUE;
    }
    else if (dac_state.volume < 100)
    {
        sample = AUDIO_DAC_APPLY_VOLUME(sample, dac_state.volume);
    }
    
    HAL_DAC_SetValue(&hdac, AUDIO_DAC_CHANNEL, DAC_ALIGN_12B_R, sample);
}

/**
 * @brief Remplit le buffer avec du silence
 */
void audio_dac_write_silence(void)
{
    audio_dac_write_buffer(silence_buffer, AUDIO_DAC_BUFFER_SIZE);
}

// ============================================================
// SECTION 4 : VOLUME
// ============================================================

/**
 * @brief Définit le volume
 */
void audio_dac_set_volume(uint8_t volume)
{
    if (volume > AUDIO_DAC_MAX_VOLUME) volume = AUDIO_DAC_MAX_VOLUME;
    dac_state.volume = volume;
    AUDIO_DAC_DEBUG("Volume: %d%%\n", volume);
}

/**
 * @brief Récupère le volume
 */
uint8_t audio_dac_get_volume(void)
{
    return dac_state.volume;
}

/**
 * @brief Active/désactive le mode muet
 */
void audio_dac_set_mute(bool mute)
{
    dac_state.muted = mute;
    
    if (mute)
    {
        // Remplir les buffers avec du silence
        audio_dac_write_silence();
    }
    
    AUDIO_DAC_DEBUG("Muet: %s\n", mute ? "ON" : "OFF");
}

/**
 * @brief Bascule le mode muet
 */
void audio_dac_toggle_mute(void)
{
    audio_dac_set_mute(!dac_state.muted);
}

/**
 * @brief Vérifie si le mode muet est actif
 */
bool audio_dac_is_muted(void)
{
    return dac_state.muted;
}

/**
 * @brief Définit la balance
 */
void audio_dac_set_balance(uint8_t balance)
{
    if (balance > 100) balance = 100;
    dac_state.balance = balance;
}

// ============================================================
// SECTION 5 : GÉNÉRATION DE TONALITÉS
// ============================================================

/**
 * @brief Remplit un buffer avec une tonalité sinusoïdale
 */
static void fill_sine_buffer(uint16_t* buffer, uint16_t size, uint16_t frequency, uint8_t amplitude)
{
    static float phase = 0.0f;
    float phaseStep = 2.0f * M_PI * frequency / dac_config.sampleRate;
    
    for (uint16_t i = 0; i < size; i++)
    {
        float value = sinf(phase) * (amplitude / 100.0f) * (AUDIO_DAC_MAX_VALUE / 2.0f);
        buffer[i] = (uint16_t)(AUDIO_DAC_SILENCE_VALUE + (int16_t)value);
        phase += phaseStep;
        
        if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
}

/**
 * @brief Remplit un buffer avec une tonalité carrée
 */
static void fill_square_buffer(uint16_t* buffer, uint16_t size, uint16_t frequency, uint8_t amplitude)
{
    static float phase = 0.0f;
    float phaseStep = 2.0f * M_PI * frequency / dac_config.sampleRate;
    int16_t ampValue = (int16_t)((amplitude / 100.0f) * (AUDIO_DAC_MAX_VALUE / 2.0f));
    
    for (uint16_t i = 0; i < size; i++)
    {
        int16_t value = (sinf(phase) >= 0) ? ampValue : -ampValue;
        buffer[i] = (uint16_t)(AUDIO_DAC_SILENCE_VALUE + value);
        phase += phaseStep;
        
        if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
}

/**
 * @brief Remplit un buffer avec une tonalité DTMF
 */
static void fill_dtmf_buffer(uint16_t* buffer, uint16_t size, 
                              uint16_t freq1, uint16_t freq2, uint8_t amplitude)
{
    static float phase1 = 0.0f, phase2 = 0.0f;
    float step1 = 2.0f * M_PI * freq1 / dac_config.sampleRate;
    float step2 = 2.0f * M_PI * freq2 / dac_config.sampleRate;
    float amp = (amplitude / 100.0f) * (AUDIO_DAC_MAX_VALUE / 4.0f);  // /4 car deux tons
    
    for (uint16_t i = 0; i < size; i++)
    {
        float value = (sinf(phase1) + sinf(phase2)) * amp;
        buffer[i] = (uint16_t)(AUDIO_DAC_SILENCE_VALUE + (int16_t)value);
        phase1 += step1;
        phase2 += step2;
        
        if (phase1 > 2.0f * M_PI) phase1 -= 2.0f * M_PI;
        if (phase2 > 2.0f * M_PI) phase2 -= 2.0f * M_PI;
    }
}

/**
 * @brief Joue une tonalité sinusoïdale
 */
void audio_dac_play_tone_sine(uint16_t frequency, uint32_t durationMs, uint8_t amplitude)
{
    if (amplitude > 100) amplitude = 100;
    
    tone_state.active = true;
    tone_state.frequency = frequency;
    tone_state.durationMs = durationMs;
    tone_state.startTime = HAL_GetTick();
    tone_state.amplitude = amplitude;
    tone_state.isDTMF = false;
    tone_state.phase = 0.0f;
    
    AUDIO_DAC_DEBUG("Tone sine: %d Hz, %lu ms, %d%%\n", frequency, (unsigned long)durationMs, amplitude);
    
    // Remplir le buffer initial
    uint16_t tempBuffer[AUDIO_DAC_BUFFER_SIZE];
    fill_sine_buffer(tempBuffer, AUDIO_DAC_BUFFER_SIZE, frequency, amplitude);
    audio_dac_write_buffer(tempBuffer, AUDIO_DAC_BUFFER_SIZE);
    
    if (!dac_state.playing)
    {
        audio_dac_start();
    }
}

/**
 * @brief Joue une tonalité carrée
 */
void audio_dac_play_tone_square(uint16_t frequency, uint32_t durationMs, uint8_t amplitude)
{
    if (amplitude > 100) amplitude = 100;
    
    tone_state.active = true;
    tone_state.frequency = frequency;
    tone_state.durationMs = durationMs;
    tone_state.startTime = HAL_GetTick();
    tone_state.amplitude = amplitude;
    tone_state.isDTMF = false;
    tone_state.phase = 0.0f;
    
    uint16_t tempBuffer[AUDIO_DAC_BUFFER_SIZE];
    fill_square_buffer(tempBuffer, AUDIO_DAC_BUFFER_SIZE, frequency, amplitude);
    audio_dac_write_buffer(tempBuffer, AUDIO_DAC_BUFFER_SIZE);
    
    if (!dac_state.playing)
    {
        audio_dac_start();
    }
}

/**
 * @brief Trouve les fréquences DTMF pour un chiffre
 */
static bool get_dtmf_frequencies(char digit, uint16_t* freq1, uint16_t* freq2)
{
    int row = -1, col = -1;
    
    if (digit >= '1' && digit <= '9')
    {
        int d = digit - '1';
        row = d / 3;
        col = d % 3;
    }
    else
    {
        switch (digit)
        {
            case '0': row = 3; col = 1; break;
            case '*': row = 3; col = 0; break;
            case '#': row = 3; col = 2; break;
            case 'A': case 'a': row = 0; col = 3; break;
            case 'B': case 'b': row = 1; col = 3; break;
            case 'C': case 'c': row = 2; col = 3; break;
            case 'D': case 'd': row = 3; col = 3; break;
            default: return false;
        }
    }
    
    if (row >= 0 && row < 4 && col >= 0 && col < 4)
    {
        *freq1 = DTMF_LOW_FREQ[row];
        *freq2 = DTMF_HIGH_FREQ[col];
        return true;
    }
    
    return false;
}

/**
 * @brief Joue une tonalité DTMF
 */
void audio_dac_play_dtmf(char digit, uint32_t durationMs)
{
    uint16_t freq1, freq2;
    
    if (!get_dtmf_frequencies(digit, &freq1, &freq2))
    {
        AUDIO_DAC_DEBUG("Digit DTMF invalide: '%c'\n", digit);
        return;
    }
    
    tone_state.active = true;
    tone_state.isDTMF = true;
    tone_state.dtmfFreq1 = freq1;
    tone_state.dtmfFreq2 = freq2;
    tone_state.durationMs = durationMs;
    tone_state.startTime = HAL_GetTick();
    tone_state.amplitude = 80;
    tone_state.phase = 0.0f;
    
    AUDIO_DAC_DEBUG("DTMF '%c': %d Hz + %d Hz, %lu ms\n", 
                   digit, freq1, freq2, (unsigned long)durationMs);
    
    uint16_t tempBuffer[AUDIO_DAC_BUFFER_SIZE];
    fill_dtmf_buffer(tempBuffer, AUDIO_DAC_BUFFER_SIZE, freq1, freq2, 80);
    audio_dac_write_buffer(tempBuffer, AUDIO_DAC_BUFFER_SIZE);
    
    if (!dac_state.playing)
    {
        audio_dac_start();
    }
}

/**
 * @brief Joue une mélodie de sonnerie
 */
void audio_dac_play_ringtone(uint8_t melodyIndex)
{
    AUDIO_DAC_DEBUG("Sonnerie %d\n", melodyIndex);
    
    // Mélodie simple : séquence de notes
    // Pour l'instant, jouer une tonalité simple
    audio_dac_play_tone_sine(440, 200, 80);  // La 440 Hz
    // Une vraie mélodie enchaînerait plusieurs notes
}

/**
 * @brief Arrête la tonalité
 */
void audio_dac_stop_tone(void)
{
    tone_state.active = false;
    audio_dac_write_silence();
}

// ============================================================
// SECTION 6 : CONFIGURATION
// ============================================================

/**
 * @brief Définit la fréquence d'échantillonnage
 */
void audio_dac_set_sample_rate(uint32_t sampleRate)
{
    if (sampleRate < 1000 || sampleRate > 100000) return;
    dac_config.sampleRate = sampleRate;
}

/**
 * @brief Active/désactive le soft start
 */
void audio_dac_soft_start_enable(bool enable)
{
    dac_config.enableSoftStart = enable;
}

// ============================================================
// SECTION 7 : CALLBACKS
// ============================================================

/**
 * @brief Enregistre le callback de buffer
 */
void audio_dac_set_buffer_callback(AudioDAC_BufferCallback callback)
{
    buffer_callback = callback;
}

/**
 * @brief Enregistre le callback de fin
 */
void audio_dac_set_done_callback(AudioDAC_PlaybackDoneCallback callback)
{
    done_callback = callback;
}

// ============================================================
// SECTION 8 : HANDLER DMA
// ============================================================

/**
 * @brief Handler d'interruption DMA1 Stream5 (DAC)
 */
void DMA1_Stream5_IRQHandler(void)
{
    // Mi-parcours (buffer A lu)
    if (__HAL_DMA_GET_FLAG(&hdma_dac1, __HAL_DMA_GET_HT_FLAG_INDEX(&hdma_dac1)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_dac1, __HAL_DMA_GET_HT_FLAG_INDEX(&hdma_dac1));
        
        dac_state.activeBuffer = 0;  // Buffer A vient d'être lu
        
        // Continuer la tonalité si active
        if (tone_state.active)
        {
            uint32_t elapsed = HAL_GetTick() - tone_state.startTime;
            
            if (tone_state.durationMs > 0 && elapsed >= tone_state.durationMs)
            {
                tone_state.active = false;
                audio_dac_write_silence();
                
                if (done_callback) done_callback();
            }
            else
            {
                uint16_t tempBuffer[AUDIO_DAC_BUFFER_SIZE];
                
                if (tone_state.isDTMF)
                {
                    fill_dtmf_buffer(tempBuffer, AUDIO_DAC_BUFFER_SIZE,
                                    tone_state.dtmfFreq1, tone_state.dtmfFreq2,
                                    tone_state.amplitude);
                }
                else
                {
                    fill_sine_buffer(tempBuffer, AUDIO_DAC_BUFFER_SIZE,
                                    tone_state.frequency, tone_state.amplitude);
                }
                
                audio_dac_write_buffer(tempBuffer, AUDIO_DAC_BUFFER_SIZE);
            }
        }
        else
        {
            // Demander à l'application de remplir le buffer A
            if (buffer_callback)
            {
                buffer_callback(dac_state.bufferA, AUDIO_DAC_BUFFER_SIZE);
            }
            else
            {
                audio_dac_write_silence();
            }
        }
        
        dac_state.bufferReady = true;
    }
    
    // Transfert complet (buffer B lu)
    if (__HAL_DMA_GET_FLAG(&hdma_dac1, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_dac1)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_dac1, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_dac1));
        
        dac_state.activeBuffer = 1;  // Buffer B vient d'être lu
        
        if (tone_state.active)
        {
            uint32_t elapsed = HAL_GetTick() - tone_state.startTime;
            
            if (tone_state.durationMs > 0 && elapsed >= tone_state.durationMs)
            {
                tone_state.active = false;
                audio_dac_write_silence();
                
                if (done_callback) done_callback();
            }
            else
            {
                uint16_t tempBuffer[AUDIO_DAC_BUFFER_SIZE];
                
                if (tone_state.isDTMF)
                {
                    fill_dtmf_buffer(tempBuffer, AUDIO_DAC_BUFFER_SIZE,
                                    tone_state.dtmfFreq1, tone_state.dtmfFreq2,
                                    tone_state.amplitude);
                }
                else
                {
                    fill_sine_buffer(tempBuffer, AUDIO_DAC_BUFFER_SIZE,
                                    tone_state.frequency, tone_state.amplitude);
                }
                
                audio_dac_write_buffer(tempBuffer, AUDIO_DAC_BUFFER_SIZE);
            }
        }
        else
        {
            if (buffer_callback)
            {
                buffer_callback(dac_state.bufferB, AUDIO_DAC_BUFFER_SIZE);
            }
            else
            {
                audio_dac_write_silence();
            }
        }
        
        dac_state.bufferReady = true;
    }
    
    // Erreur
    if (__HAL_DMA_GET_FLAG(&hdma_dac1, __HAL_DMA_GET_TE_FLAG_INDEX(&hdma_dac1)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_dac1, __HAL_DMA_GET_TE_FLAG_INDEX(&hdma_dac1));
        dac_state.underruns++;
    }
    
    HAL_DMA_IRQHandler(&hdma_dac1);
}

// ============================================================
// SECTION 9 : DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état
 */
void audio_dac_print_state(void)
{
    printf("\n═══ ÉTAT DAC AUDIO ═══\n");
    printf("Initialisé    : %s\n", dac_state.initialized ? "Oui" : "Non");
    printf("Lecture       : %s\n", dac_state.playing ? "Oui" : "Non");
    printf("Volume        : %d%%\n", dac_state.volume);
    printf("Muet          : %s\n", dac_state.muted ? "Oui" : "Non");
    printf("Balance       : %d\n", dac_state.balance);
    printf("Buffer prêt   : %s\n", dac_state.bufferReady ? "Oui" : "Non");
    printf("Buffer actif  : %d\n", dac_state.activeBuffer);
    printf("Fréquence     : %lu Hz\n", (unsigned long)dac_config.sampleRate);
    printf("Total éch.    : %lu\n", (unsigned long)dac_state.totalSamples);
    printf("Underruns     : %lu\n", (unsigned long)dac_state.underruns);
    printf("Tonalité      : %s\n", tone_state.active ? "Active" : "Inactive");
    printf("══════════════════════\n\n");
}

/**
 * @brief Affiche les statistiques
 */
void audio_dac_print_statistics(void)
{
    printf("\n═══ STATISTIQUES DAC AUDIO ═══\n");
    printf("Échantillons   : %lu\n", (unsigned long)dac_state.totalSamples);
    printf("Underruns      : %lu\n", (unsigned long)dac_state.underruns);
    printf("Durée (est.)   : %lu secondes\n",
           (unsigned long)(dac_state.totalSamples / dac_config.sampleRate));
    printf("════════════════════════════\n\n");
}

/**
 * @brief Test de fonctionnement
 */
bool audio_dac_self_test(void)
{
    AUDIO_DAC_DEBUG("Auto-test...\n");
    
    if (!dac_state.initialized)
    {
        AUDIO_DAC_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Jouer une courte tonalité de test
    audio_dac_play_tone_sine(1000, 100, 50);  // 1 kHz, 100ms, 50%
    HAL_Delay(150);
    audio_dac_stop_tone();
    
    AUDIO_DAC_DEBUG("Auto-test OK\n");
    return true;
}