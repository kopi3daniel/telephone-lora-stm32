/**
 * @file audio_adc.cpp
 * @brief Implémentation du driver de capture audio via ADC
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans audio_adc.h.
 * 
 * Il gère :
 * - L'initialisation de l'ADC en mode DMA continu
 * - Le double buffering pour une capture sans perte
 * - Les callbacks de buffer plein
 * - Le filtrage audio (médian, moyenne)
 * - Le calcul du niveau VU
 * - L'application du gain
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "audio_adc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// ============================================================
// HANDLES EXTERNES
// ============================================================

/** @brief Handle ADC */
extern ADC_HandleTypeDef hadc1;

/** @brief Handle DMA pour l'ADC */
extern DMA_HandleTypeDef hdma_adc1;

/** @brief Handle Timer pour le déclenchement */
extern TIM_HandleTypeDef htim6;

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du driver ADC */
static AudioADC_State adc_state;

/** @brief Configuration du driver */
static AudioADC_Config adc_config = {
    .sampleRate = AUDIO_ADC_DEFAULT_SAMPLE_RATE,
    .bufferSize = AUDIO_ADC_BUFFER_SIZE,
    .enableDoubleBuffering = true,
    .enableMedianFilter = false,
    .enableAverageFilter = false,
    .filterWindow = 5,
    .gain = 100,
    .enableVUMeter = true,
    .vuMeterDecay = 200
};

/** @brief Callbacks */
static AudioADC_BufferCallback buffer_callback = NULL;
static AudioADC_VUCallback vu_callback = NULL;

/** @brief Compteurs DMA */
static volatile uint8_t dma_half_complete = 0;
static volatile uint8_t dma_full_complete = 0;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le driver ADC audio
 */
bool audio_adc_init(const AudioADC_Config* config)
{
    AUDIO_ADC_DEBUG("Initialisation du driver ADC audio...\n");
    
    // Sauvegarder la configuration
    if (config != NULL)
    {
        memcpy(&adc_config, config, sizeof(AudioADC_Config));
    }
    
    // Initialiser l'état
    memset(&adc_state, 0, sizeof(AudioADC_State));
    adc_state.config = adc_config;
    
    // Configurer les pointeurs de buffer
    adc_state.bufferA = &adc_state.dmaBuffer[0];
    adc_state.bufferB = &adc_state.dmaBuffer[AUDIO_ADC_BUFFER_SIZE];
    
    // --- Configuration du GPIO pour l'ADC ---
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // PA0 = ADC1_IN0 (entrée analogique)
    GPIO_InitStruct.Pin = AUDIO_ADC_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(AUDIO_ADC_PORT, &GPIO_InitStruct);
    
    // --- Configuration de l'ADC ---
    __HAL_RCC_ADC1_CLK_ENABLE();
    
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;     // 90/4 = 22.5 MHz
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;                         // Un seul canal
    hadc1.Init.ContinuousConvMode = DISABLE;                   // Déclenché par timer
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T6_TRGO; // Déclenché par TIM6
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = ENABLE;                 // DMA continu
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        AUDIO_ADC_DEBUG("Échec initialisation ADC\n");
        return false;
    }
    
    // --- Configuration du canal ADC ---
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = AUDIO_ADC_CHANNEL;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;  // Échantillonnage rapide
    sConfig.Offset = 0;
    
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        AUDIO_ADC_DEBUG("Échec configuration canal ADC\n");
        return false;
    }
    
    // --- Configuration du timer de déclenchement (TIM6) ---
    __HAL_RCC_TIM6_CLK_ENABLE();
    
    htim6.Instance = TIM6;
    htim6.Init.Prescaler = 0;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.Period = (180000000 / adc_config.sampleRate) - 1;  // 180MHz / 8000 = 22500
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    
    if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
    {
        AUDIO_ADC_DEBUG("Échec configuration TIM6\n");
        return false;
    }
    
    // Configurer le trigger output (TRGO) pour l'ADC
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;  // TRGO à chaque update
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    
    if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
    {
        AUDIO_ADC_DEBUG("Échec configuration TRGO\n");
        return false;
    }
    
    // --- Configuration du DMA ---
    __HAL_RCC_DMA2_CLK_ENABLE();
    
    hdma_adc1.Instance = DMA2_Stream0;
    hdma_adc1.Init.Channel = DMA_CHANNEL_0;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;          // ADC → Mémoire
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;               // Registre ADC fixe
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;                   // Buffer incrémenté
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;                        // Mode circulaire
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;               // Priorité haute (audio)
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
    {
        AUDIO_ADC_DEBUG("Échec configuration DMA\n");
        return false;
    }
    
    // Lier le DMA à l'ADC
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);
    
    // --- Configuration des interruptions DMA ---
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 2, 0);  // Priorité haute
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    
    // Activer les interruptions mi-parcours et fin de transfert
    __HAL_DMA_ENABLE_IT(&hdma_adc1, DMA_IT_HT);  // Half Transfer
    __HAL_DMA_ENABLE_IT(&hdma_adc1, DMA_IT_TC);  // Transfer Complete
    
    adc_state.initialized = true;
    
    AUDIO_ADC_DEBUG("Driver ADC initialisé\n");
    AUDIO_ADC_DEBUG("Fréquence: %lu Hz, Buffer: %d échantillons\n",
                   (unsigned long)adc_config.sampleRate, adc_config.bufferSize);
    
    return true;
}

/**
 * @brief Désinitialise le driver
 */
void audio_adc_deinit(void)
{
    audio_adc_stop();
    HAL_ADC_DeInit(&hadc1);
    HAL_DMA_DeInit(&hdma_adc1);
    HAL_TIM_Base_DeInit(&htim6);
    adc_state.initialized = false;
}

/**
 * @brief Vérifie si le driver est prêt
 */
bool audio_adc_is_ready(void)
{
    return adc_state.initialized;
}

/**
 * @brief Récupère l'état
 */
AudioADC_State* audio_adc_get_state(void)
{
    return &adc_state;
}

// ============================================================
// SECTION 2 : CONTRÔLE
// ============================================================

/**
 * @brief Démarre la capture audio
 */
void audio_adc_start(void)
{
    if (!adc_state.initialized) return;
    if (adc_state.recording) return;
    
    AUDIO_ADC_DEBUG("Démarrage capture audio\n");
    
    // Réinitialiser les compteurs DMA
    dma_half_complete = 0;
    dma_full_complete = 0;
    adc_state.bufferReady = false;
    adc_state.activeBuffer = 0;
    
    // Démarrer le DMA
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_state.dmaBuffer, AUDIO_ADC_DMA_BUFFER_SIZE);
    
    // Démarrer le timer de déclenchement
    HAL_TIM_Base_Start(&htim6);
    
    adc_state.recording = true;
}

/**
 * @brief Arrête la capture
 */
void audio_adc_stop(void)
{
    if (!adc_state.recording) return;
    
    AUDIO_ADC_DEBUG("Arrêt capture audio\n");
    
    // Arrêter le timer
    HAL_TIM_Base_Stop(&htim6);
    
    // Arrêter le DMA
    HAL_ADC_Stop_DMA(&hadc1);
    
    adc_state.recording = false;
    adc_state.bufferReady = false;
}

/**
 * @brief Vérifie si la capture est en cours
 */
bool audio_adc_is_recording(void)
{
    return adc_state.recording;
}

/**
 * @brief Met en pause
 */
void audio_adc_pause(void)
{
    HAL_TIM_Base_Stop(&htim6);
}

/**
 * @brief Reprend
 */
void audio_adc_resume(void)
{
    HAL_TIM_Base_Start(&htim6);
}

// ============================================================
// SECTION 3 : LECTURE DES DONNÉES
// ============================================================

/**
 * @brief Vérifie si un buffer est disponible
 */
bool audio_adc_is_buffer_ready(void)
{
    return adc_state.bufferReady;
}

/**
 * @brief Récupère le buffer audio courant
 */
bool audio_adc_get_buffer(uint16_t** buffer, uint16_t* size)
{
    if (!adc_state.bufferReady) return false;
    
    // Retourner le buffer qui vient d'être rempli
    if (adc_state.activeBuffer == 0)
    {
        *buffer = adc_state.bufferB;  // B vient d'être rempli
    }
    else
    {
        *buffer = adc_state.bufferA;  // A vient d'être rempli
    }
    
    *size = adc_config.bufferSize;
    
    // Appliquer les filtres si activés
    if (adc_config.enableMedianFilter)
    {
        audio_adc_median_filter(*buffer, *size, adc_config.filterWindow);
    }
    
    if (adc_config.enableAverageFilter)
    {
        audio_adc_average_filter(*buffer, *size, adc_config.filterWindow);
    }
    
    // Appliquer le gain
    if (adc_config.gain != 100)
    {
        audio_adc_apply_gain(*buffer, *size, adc_config.gain);
    }
    
    // Calculer le niveau VU
    if (adc_config.enableVUMeter)
    {
        uint8_t vuLevel = audio_adc_calculate_vu(*buffer, *size);
        adc_state.vuLevel = vuLevel;
        
        // Mise à jour du peak
        if (vuLevel > adc_state.vuPeak)
        {
            adc_state.vuPeak = vuLevel;
        }
        
        if (vu_callback)
        {
            vu_callback(vuLevel, adc_state.vuPeak);
        }
    }
    
    adc_state.bufferReady = false;
    adc_state.totalSamples += *size;
    
    return true;
}

/**
 * @brief Lit un échantillon unique (bloquant)
 */
uint16_t audio_adc_read_sample(void)
{
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint16_t sample = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return sample;
}

/**
 * @brief Lit plusieurs échantillons (bloquant)
 */
void audio_adc_read_samples(uint16_t* buffer, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++)
    {
        buffer[i] = audio_adc_read_sample();
    }
}

// ============================================================
// SECTION 4 : TRAITEMENT AUDIO
// ============================================================

/**
 * @brief Convertit une valeur ADC en niveau (0-100)
 */
uint8_t audio_adc_to_level(uint16_t sample)
{
    // Centrer autour du silence (2048)
    int32_t centered = (int32_t)sample - AUDIO_ADC_SILENCE_VALUE;
    uint32_t absolute = abs(centered);
    
    // Normaliser à 0-100
    uint32_t level = (absolute * 100) / AUDIO_ADC_SILENCE_VALUE;
    
    return (uint8_t)((level > 100) ? 100 : level);
}

/**
 * @brief Applique un gain au buffer
 */
void audio_adc_apply_gain(uint16_t* buffer, uint16_t size, uint8_t gain)
{
    if (gain == 100) return;  // Pas de changement
    
    float gainFactor = (float)gain / 100.0f;
    
    for (uint16_t i = 0; i < size; i++)
    {
        // Appliquer le gain autour du silence
        int32_t sample = (int32_t)buffer[i] - AUDIO_ADC_SILENCE_VALUE;
        sample = (int32_t)(sample * gainFactor);
        sample += AUDIO_ADC_SILENCE_VALUE;
        
        // Limiter
        if (sample < 0) sample = 0;
        if (sample > AUDIO_ADC_MAX_VALUE) sample = AUDIO_ADC_MAX_VALUE;
        
        buffer[i] = (uint16_t)sample;
    }
}

/**
 * @brief Comparateur pour qsort
 */
static int compare_uint16(const void* a, const void* b)
{
    return (*(uint16_t*)a - *(uint16_t*)b);
}

/**
 * @brief Filtre médian
 */
void audio_adc_median_filter(uint16_t* buffer, uint16_t size, uint8_t window)
{
    if (window < 2 || window > size) return;
    
    uint16_t* temp = (uint16_t*)malloc(size * sizeof(uint16_t));
    if (temp == NULL) return;
    
    memcpy(temp, buffer, size * sizeof(uint16_t));
    
    uint16_t* sortBuf = (uint16_t*)malloc(window * sizeof(uint16_t));
    if (sortBuf == NULL)
    {
        free(temp);
        return;
    }
    
    uint8_t halfWindow = window / 2;
    
    for (uint16_t i = halfWindow; i < size - halfWindow; i++)
    {
        // Copier la fenêtre
        for (uint8_t j = 0; j < window; j++)
        {
            sortBuf[j] = temp[i - halfWindow + j];
        }
        
        // Trier
        qsort(sortBuf, window, sizeof(uint16_t), compare_uint16);
        
        // Prendre la médiane
        buffer[i] = sortBuf[halfWindow];
    }
    
    free(temp);
    free(sortBuf);
}

/**
 * @brief Filtre moyenne
 */
void audio_adc_average_filter(uint16_t* buffer, uint16_t size, uint8_t window)
{
    if (window < 2 || window > size) return;
    
    uint16_t* temp = (uint16_t*)malloc(size * sizeof(uint16_t));
    if (temp == NULL) return;
    
    memcpy(temp, buffer, size * sizeof(uint16_t));
    
    uint8_t halfWindow = window / 2;
    
    for (uint16_t i = halfWindow; i < size - halfWindow; i++)
    {
        uint32_t sum = 0;
        
        for (uint8_t j = 0; j < window; j++)
        {
            sum += temp[i - halfWindow + j];
        }
        
        buffer[i] = (uint16_t)(sum / window);
    }
    
    free(temp);
}

/**
 * @brief Calcule le niveau VU
 */
uint8_t audio_adc_calculate_vu(const uint16_t* buffer, uint16_t size)
{
    if (size == 0) return 0;
    
    uint32_t sum = 0;
    uint16_t peak = 0;
    
    for (uint16_t i = 0; i < size; i++)
    {
        int32_t sample = (int32_t)buffer[i] - AUDIO_ADC_SILENCE_VALUE;
        uint32_t absolute = abs(sample);
        
        sum += absolute;
        
        if (absolute > peak)
        {
            peak = absolute;
        }
    }
    
    // Niveau moyen
    uint16_t avg = sum / size;
    
    // Normaliser à 0-100
    uint8_t level = (uint8_t)((avg * 100) / AUDIO_ADC_SILENCE_VALUE);
    
    return (level > 100) ? 100 : level;
}

// ============================================================
// SECTION 5 : CONFIGURATION
// ============================================================

/**
 * @brief Définit la fréquence d'échantillonnage
 */
void audio_adc_set_sample_rate(uint32_t sampleRate)
{
    if (sampleRate < 1000 || sampleRate > 100000) return;
    
    adc_config.sampleRate = sampleRate;
    
    // Mettre à jour le timer
    uint32_t period = (180000000 / sampleRate) - 1;
    __HAL_TIM_SET_AUTORELOAD(&htim6, period);
    
    AUDIO_ADC_DEBUG("Fréquence: %lu Hz (ARR=%lu)\n", (unsigned long)sampleRate, (unsigned long)period);
}

/**
 * @brief Définit le gain
 */
void audio_adc_set_gain(uint8_t gain)
{
    if (gain < 1) gain = 1;
    if (gain > 200) gain = 200;
    adc_config.gain = gain;
}

/**
 * @brief Active/désactive le filtre médian
 */
void audio_adc_filter_median_enable(bool enable)
{
    adc_config.enableMedianFilter = enable;
}

/**
 * @brief Active/désactive le filtre moyenne
 */
void audio_adc_filter_average_enable(bool enable)
{
    adc_config.enableAverageFilter = enable;
}

// ============================================================
// SECTION 6 : CALLBACKS
// ============================================================

/**
 * @brief Enregistre le callback de buffer
 */
void audio_adc_set_buffer_callback(AudioADC_BufferCallback callback)
{
    buffer_callback = callback;
}

/**
 * @brief Enregistre le callback VU
 */
void audio_adc_set_vu_callback(AudioADC_VUCallback callback)
{
    vu_callback = callback;
}

// ============================================================
// SECTION 7 : HANDLER DMA
// ============================================================

/**
 * @brief Handler d'interruption DMA2 Stream0 (ADC)
 */
void DMA2_Stream0_IRQHandler(void)
{
    // Vérifier si mi-parcours (buffer A plein)
    if (__HAL_DMA_GET_FLAG(&hdma_adc1, __HAL_DMA_GET_HT_FLAG_INDEX(&hdma_adc1)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_adc1, __HAL_DMA_GET_HT_FLAG_INDEX(&hdma_adc1));
        
        adc_state.activeBuffer = 0;  // Buffer A est plein
        adc_state.bufferReady = true;
        
        if (buffer_callback)
        {
            buffer_callback(adc_state.bufferA, adc_config.bufferSize);
        }
    }
    
    // Vérifier si transfert complet (buffer B plein)
    if (__HAL_DMA_GET_FLAG(&hdma_adc1, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_adc1)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_adc1, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_adc1));
        
        adc_state.activeBuffer = 1;  // Buffer B est plein
        adc_state.bufferReady = true;
        
        if (buffer_callback)
        {
            buffer_callback(adc_state.bufferB, adc_config.bufferSize);
        }
    }
    
    // Vérifier les erreurs
    if (__HAL_DMA_GET_FLAG(&hdma_adc1, __HAL_DMA_GET_TE_FLAG_INDEX(&hdma_adc1)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_adc1, __HAL_DMA_GET_TE_FLAG_INDEX(&hdma_adc1));
        adc_state.overflows++;
    }
    
    HAL_DMA_IRQHandler(&hdma_adc1);
}

// ============================================================
// SECTION 8 : DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état
 */
void audio_adc_print_state(void)
{
    printf("\n═══ ÉTAT ADC AUDIO ═══\n");
    printf("Initialisé    : %s\n", adc_state.initialized ? "Oui" : "Non");
    printf("Enregistrement: %s\n", adc_state.recording ? "Oui" : "Non");
    printf("Buffer prêt   : %s\n", adc_state.bufferReady ? "Oui" : "Non");
    printf("Buffer actif  : %d\n", adc_state.activeBuffer);
    printf("Fréquence     : %lu Hz\n", (unsigned long)adc_config.sampleRate);
    printf("Taille buffer : %d éch.\n", adc_config.bufferSize);
    printf("Gain          : %d%%\n", adc_config.gain);
    printf("VU Level      : %d\n", adc_state.vuLevel);
    printf("VU Peak       : %d\n", adc_state.vuPeak);
    printf("Total éch.    : %lu\n", (unsigned long)adc_state.totalSamples);
    printf("Overflows     : %lu\n", (unsigned long)adc_state.overflows);
    printf("══════════════════════\n\n");
}

/**
 * @brief Affiche les statistiques
 */
void audio_adc_print_statistics(void)
{
    printf("\n═══ STATISTIQUES ADC AUDIO ═══\n");
    printf("Échantillons   : %lu\n", (unsigned long)adc_state.totalSamples);
    printf("Overflows      : %lu\n", (unsigned long)adc_state.overflows);
    printf("Durée (est.)   : %lu secondes\n", 
           (unsigned long)(adc_state.totalSamples / adc_config.sampleRate));
    printf("Taux erreur    : %.4f%%\n",
           adc_state.totalSamples > 0 ? 
           100.0f * adc_state.overflows / adc_state.totalSamples : 0.0f);
    printf("════════════════════════════\n\n");
}

/**
 * @brief Test de fonctionnement
 */
bool audio_adc_self_test(void)
{
    AUDIO_ADC_DEBUG("Auto-test...\n");
    
    if (!adc_state.initialized)
    {
        AUDIO_ADC_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Lire un échantillon
    uint16_t sample = audio_adc_read_sample();
    AUDIO_ADC_DEBUG("Échantillon test: %d\n", sample);
    
    // Vérifier que la valeur est plausible
    if (sample > AUDIO_ADC_MAX_VALUE)
    {
        AUDIO_ADC_DEBUG("Échec : valeur hors limites\n");
        return false;
    }
    
    AUDIO_ADC_DEBUG("Auto-test OK\n");
    return true;
}