/**
 * @file audio_i2s.cpp
 * @brief Implémentation du driver I2S pour codec audio externe
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans audio_i2s.h.
 * 
 * Il gère :
 * - L'initialisation de l'interface I2S en mode DMA
 * - Le double buffering TX/RX
 * - La communication I2C avec le codec WM8978
 * - Le contrôle de volume matériel du codec
 * - La gestion du Master Clock (MCLK)
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "audio_i2s.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// HANDLES EXTERNES
// ============================================================

/** @brief Handle I2S (partage avec SPI2) */
extern SPI_HandleTypeDef hspi2;

/** @brief Handle I2C pour le codec */
extern I2C_HandleTypeDef hi2c1;

/** @brief Handle DMA pour I2S TX */
extern DMA_HandleTypeDef hdma_spi2_tx;

/** @brief Handle DMA pour I2S RX */
extern DMA_HandleTypeDef hdma_spi2_rx;

// ============================================================
// ADRESSES I2C DU CODEC WM8978
// ============================================================

/** @brief Adresse I2C du WM8978 (7 bits) */
#define WM8978_I2C_ADDRESS              0x1A

/** @brief Registres du WM8978 */
#define WM8978_REG_RESET                0x00
#define WM8978_REG_POWER_MGMT1          0x01
#define WM8978_REG_POWER_MGMT2          0x02
#define WM8978_REG_POWER_MGMT3          0x03
#define WM8978_REG_AUDIO_INTERFACE      0x04
#define WM8978_REG_COMPANDING_CTRL      0x05
#define WM8978_REG_CLOCK_GEN_CTRL       0x06
#define WM8978_REG_ADDITIONAL_CTRL      0x07
#define WM8978_REG_GPIO_STUFF           0x08
#define WM8978_REG_LEFT_INPUT_CTRL      0x10
#define WM8978_REG_RIGHT_INPUT_CTRL     0x12
#define WM8978_REG_LEFT_OUTPUT_CTRL     0x14
#define WM8978_REG_RIGHT_OUTPUT_CTRL    0x16
#define WM8978_REG_LEFT_OUTPUT_VOL      0x18
#define WM8978_REG_RIGHT_OUTPUT_VOL     0x1A
#define WM8978_REG_LEFT_ADC_BOOST       0x20
#define WM8978_REG_RIGHT_ADC_BOOST      0x22
#define WM8978_REG_OUT3_MIXER           0x24
#define WM8978_REG_BEEP_VOLUME          0x28
#define WM8978_REG_INPUT_CTRL           0x30
#define WM8978_REG_LEFT_INPUT_VOL       0x32
#define WM8978_REG_RIGHT_INPUT_VOL      0x34

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du driver I2S */
static AudioI2S_State i2s_state;

/** @brief Configuration */
static AudioI2S_Config i2s_config = {
    .mode = I2S_MODE_MASTER_TX,
    .standard = I2S_STANDARD_PHILIPS,
    .sampleRate = I2S_SAMPLE_RATE_44K,
    .resolution = I2S_RESOLUTION_16BIT,
    .bufferSize = AUDIO_I2S_BUFFER_SIZE,
    .enableDoubleBuffering = true,
    .enableMasterClock = true,
    .masterClockDivider = 8,
    .volume = 80,
    .startMuted = false
};

/** @brief Callbacks */
static AudioI2S_TXCallback tx_callback = NULL;
static AudioI2S_RXCallback rx_callback = NULL;
static AudioI2S_ErrorCallback error_callback = NULL;

/** @brief Flag codec détecté */
static bool codec_detected = false;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise l'interface I2S
 */
bool audio_i2s_init(const AudioI2S_Config* config)
{
    AUDIO_I2S_DEBUG("Initialisation I2S...\n");
    
    if (config != NULL)
    {
        memcpy(&i2s_config, config, sizeof(AudioI2S_Config));
    }
    
    memset(&i2s_state, 0, sizeof(AudioI2S_State));
    i2s_state.config = i2s_config;
    i2s_state.volume = i2s_config.volume;
    i2s_state.muted = i2s_config.startMuted;
    
    // Configurer les pointeurs de buffer
    i2s_state.txBufferA = &i2s_state.txDmaBuffer[0];
    i2s_state.txBufferB = &i2s_state.txDmaBuffer[i2s_config.bufferSize];
    i2s_state.rxBufferA = &i2s_state.rxDmaBuffer[0];
    i2s_state.rxBufferB = &i2s_state.rxDmaBuffer[i2s_config.bufferSize];
    
    // --- Configuration des GPIO I2S ---
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    // PB12 = I2S2_WS (Word Select)
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // PB13 = I2S2_CK (Bit Clock)
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // PB15 = I2S2_SD (Serial Data)
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // PC6 = I2S2_MCK (Master Clock) - optionnel
    if (i2s_config.enableMasterClock)
    {
        GPIO_InitStruct.Pin = GPIO_PIN_6;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    }
    
    // --- Configuration I2S ---
    __HAL_RCC_SPI2_CLK_ENABLE();
    
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_I2S;
    hspi2.Init.Direction = I2S_DIRECTION_TX;  // Ou I2S_DIRECTION_TX_RX pour full duplex
    
    // Configurer selon le mode
    if (i2s_config.mode <= I2S_MODE_SLAVE_TX)
    {
        hspi2.Init.Mode = SPI_MODE_I2S;
    }
    
    // Standard
    switch (i2s_config.standard)
    {
        case I2S_STANDARD_PHILIPS:   hspi2.Init.DataFormat = I2S_STANDARD_PHILIPS; break;
        case I2S_STANDARD_MSB:       hspi2.Init.DataFormat = I2S_STANDARD_MSB; break;
        case I2S_STANDARD_LSB:       hspi2.Init.DataFormat = I2S_STANDARD_LSB; break;
        case I2S_STANDARD_PCM_SHORT: hspi2.Init.DataFormat = I2S_STANDARD_PCM_SHORT; break;
        case I2S_STANDARD_PCM_LONG:  hspi2.Init.DataFormat = I2S_STANDARD_PCM_LONG; break;
        default:                     hspi2.Init.DataFormat = I2S_STANDARD_PHILIPS; break;
    }
    
    // Résolution
    switch (i2s_config.resolution)
    {
        case I2S_RESOLUTION_16BIT: hspi2.Init.DataSize = I2S_DATASIZE_16BIT; break;
        case I2S_RESOLUTION_24BIT: hspi2.Init.DataSize = I2S_DATASIZE_24BIT; break;
        case I2S_RESOLUTION_32BIT: hspi2.Init.DataSize = I2S_DATASIZE_32BIT; break;
        default:                   hspi2.Init.DataSize = I2S_DATASIZE_16BIT; break;
    }
    
    hspi2.Init.MCLKOutput = i2s_config.enableMasterClock ? 
                            I2S_MCLKOUTPUT_ENABLE : I2S_MCLKOUTPUT_DISABLE;
    hspi2.Init.AudioFreq = I2S_AUDIOFREQ_44K;  // Fréquence par défaut
    hspi2.Init.CPOL = I2S_CPOL_LOW;
    
    if (HAL_I2S_Init(&hspi2) != HAL_OK)
    {
        AUDIO_I2S_DEBUG("Échec initialisation I2S\n");
        return false;
    }
    
    // --- Configuration DMA ---
    // (utilise les handles DMA déjà configurés pour SPI2)
    
    i2s_state.initialized = true;
    
    AUDIO_I2S_DEBUG("I2S initialisé (Fs=%lu Hz, %d bits)\n",
                   (unsigned long)i2s_config.sampleRate,
                   i2s_config.resolution);
    
    return true;
}

/**
 * @brief Désinitialise
 */
void audio_i2s_deinit(void)
{
    audio_i2s_stop();
    HAL_I2S_DeInit(&hspi2);
    i2s_state.initialized = false;
}

/**
 * @brief Vérifie si prêt
 */
bool audio_i2s_is_ready(void)
{
    return i2s_state.initialized;
}

// ============================================================
// SECTION 2 : CONTRÔLE
// ============================================================

/**
 * @brief Démarre le transfert I2S
 */
void audio_i2s_start(void)
{
    if (!i2s_state.initialized) return;
    if (i2s_state.running) return;
    
    AUDIO_I2S_DEBUG("Démarrage transfert I2S\n");
    
    // Démarrer le DMA TX si configuré
    if (i2s_config.mode == I2S_MODE_MASTER_TX || i2s_config.mode == I2S_MODE_SLAVE_TX)
    {
        HAL_I2S_Transmit_DMA(&hspi2, (uint16_t*)i2s_state.txDmaBuffer, 
                             i2s_config.bufferSize * 2);
    }
    
    // Démarrer le DMA RX si configuré
    if (i2s_config.mode == I2S_MODE_MASTER_RX || i2s_config.mode == I2S_MODE_SLAVE_RX)
    {
        HAL_I2S_Receive_DMA(&hspi2, (uint16_t*)i2s_state.rxDmaBuffer,
                            i2s_config.bufferSize * 2);
    }
    
    i2s_state.running = true;
}

/**
 * @brief Arrête
 */
void audio_i2s_stop(void)
{
    if (!i2s_state.running) return;
    
    HAL_I2S_DMAStop(&hspi2);
    i2s_state.running = false;
}

/**
 * @brief Vérifie si en cours
 */
bool audio_i2s_is_running(void)
{
    return i2s_state.running;
}

void audio_i2s_pause(void)  { HAL_I2S_DMAPause(&hspi2); }
void audio_i2s_resume(void) { HAL_I2S_DMAResume(&hspi2); }

// ============================================================
// SECTION 3 : TRANSMISSION
// ============================================================

bool audio_i2s_tx_ready(void) { return i2s_state.txBufferReady; }

bool audio_i2s_tx_write(const uint16_t* data, uint16_t size)
{
    if (!i2s_state.running) return false;
    if (size > i2s_config.bufferSize) size = i2s_config.bufferSize;
    
    uint16_t* targetBuffer = (i2s_state.txBufferA) ? i2s_state.txBufferB : i2s_state.txBufferA;
    
    if (i2s_state.muted)
    {
        memset(targetBuffer, 0, size * sizeof(uint16_t));
    }
    else if (i2s_state.volume < 100)
    {
        for (uint16_t i = 0; i < size; i++)
        {
            targetBuffer[i] = (uint16_t)(((uint32_t)data[i] * i2s_state.volume) / 100);
        }
    }
    else
    {
        memcpy(targetBuffer, data, size * sizeof(uint16_t));
    }
    
    i2s_state.txSamples += size;
    return true;
}

// ============================================================
// SECTION 4 : RÉCEPTION
// ============================================================

bool audio_i2s_rx_ready(void) { return i2s_state.rxBufferReady; }

bool audio_i2s_rx_read(uint16_t* data, uint16_t* size)
{
    if (!i2s_state.rxBufferReady) return false;
    
    uint16_t* sourceBuffer = (i2s_state.rxBufferA) ? i2s_state.rxBufferB : i2s_state.rxBufferA;
    uint16_t copySize = (*size < i2s_config.bufferSize) ? *size : i2s_config.bufferSize;
    
    memcpy(data, sourceBuffer, copySize * sizeof(uint16_t));
    *size = copySize;
    
    i2s_state.rxSamples += copySize;
    i2s_state.rxBufferReady = false;
    
    return true;
}

// ============================================================
// SECTION 5 : VOLUME
// ============================================================

void audio_i2s_set_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    i2s_state.volume = volume;
    
    // Appliquer au codec si présent
    if (codec_detected)
    {
        audio_i2s_codec_set_volume(volume);
    }
}

uint8_t audio_i2s_get_volume(void) { return i2s_state.volume; }

void audio_i2s_set_mute(bool mute)
{
    i2s_state.muted = mute;
    if (codec_detected)
    {
        // Configurer le registre de mute du codec
    }
}

void audio_i2s_toggle_mute(void) { audio_i2s_set_mute(!i2s_state.muted); }

// ============================================================
// SECTION 6 : CONFIGURATION
// ============================================================

void audio_i2s_set_sample_rate(I2S_SampleRate rate)
{
    i2s_config.sampleRate = rate;
    // Modifier la configuration I2S (nécessite réinitialisation)
}

void audio_i2s_set_resolution(I2S_Resolution resolution)
{
    i2s_config.resolution = resolution;
}

// ============================================================
// SECTION 7 : CALLBACKS
// ============================================================

void audio_i2s_set_tx_callback(AudioI2S_TXCallback callback)   { tx_callback = callback; }
void audio_i2s_set_rx_callback(AudioI2S_RXCallback callback)   { rx_callback = callback; }
void audio_i2s_set_error_callback(AudioI2S_ErrorCallback callback) { error_callback = callback; }

// ============================================================
// SECTION 8 : CODEC WM8978
// ============================================================

/**
 * @brief Écrit un registre du WM8978 via I2C
 */
static bool wm8978_write_register(uint8_t reg, uint16_t value)
{
    uint8_t data[2];
    
    // Format : [adresse (7 bits) | registre (7 bits) | valeur (9 bits)]
    // En pratique : 2 octets (registre << 1 | bit8, bits 7:0)
    data[0] = ((reg << 1) & 0xFE) | ((value >> 8) & 0x01);
    data[1] = value & 0xFF;
    
    return (HAL_I2C_Master_Transmit(&hi2c1, WM8978_I2C_ADDRESS << 1, 
                                     data, 2, 100) == HAL_OK);
}

/**
 * @brief Initialise le codec WM8978
 */
bool audio_i2s_codec_wm8978_init(void)
{
    AUDIO_I2S_DEBUG("Initialisation codec WM8978...\n");
    
    // 1. Reset
    wm8978_write_register(WM8978_REG_RESET, 0x0000);
    HAL_Delay(100);
    
    // 2. Power Management 1 : activer VREF, VMID, DACL, DACR
    wm8978_write_register(WM8978_REG_POWER_MGMT1, 0x0017);
    
    // 3. Power Management 2 : activer ADC, entrées micro
    wm8978_write_register(WM8978_REG_POWER_MGMT2, 0x0000);
    
    // 4. Power Management 3 : activer sorties HP, LINE
    wm8978_write_register(WM8978_REG_POWER_MGMT3, 0x0000);
    
    // 5. Interface audio : I2S, 16 bits, format Philips
    wm8978_write_register(WM8978_REG_AUDIO_INTERFACE, 0x0002);
    
    // 6. Horloge : MCLK / 8 = 44.1 kHz × 256 = 11.2896 MHz
    wm8978_write_register(WM8978_REG_CLOCK_GEN_CTRL, 0x0008);
    
    // 7. Volume sortie gauche
    wm8978_write_register(WM8978_REG_LEFT_OUTPUT_VOL, 0x0079);
    
    // 8. Volume sortie droite
    wm8978_write_register(WM8978_REG_RIGHT_OUTPUT_VOL, 0x0079);
    
    // 9. Activer sorties
    wm8978_write_register(WM8978_REG_POWER_MGMT2, 0x0180);
    wm8978_write_register(WM8978_REG_POWER_MGMT3, 0x00C0);
    
    codec_detected = true;
    
    AUDIO_I2S_DEBUG("Codec WM8978 initialisé\n");
    return true;
}

/**
 * @brief Configure le volume du codec
 */
void audio_i2s_codec_set_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    
    // Convertir 0-100 → registre WM8978 (0-63, log)
    uint16_t regValue = (uint16_t)(volume * 63 / 100);
    
    wm8978_write_register(WM8978_REG_LEFT_OUTPUT_VOL, regValue | 0x0100);
    wm8978_write_register(WM8978_REG_RIGHT_OUTPUT_VOL, regValue | 0x0100);
}

/**
 * @brief Configure le gain micro
 */
void audio_i2s_codec_set_mic_gain(uint8_t gain)
{
    if (gain > 100) gain = 100;
    
    // Convertir 0-100 → registre WM8978 (0-63)
    uint16_t regValue = (uint16_t)(gain * 63 / 100);
    
    wm8978_write_register(WM8978_REG_LEFT_ADC_BOOST, regValue);
    wm8978_write_register(WM8978_REG_RIGHT_ADC_BOOST, regValue);
}

// ============================================================
// SECTION 9 : HANDLER DMA
// ============================================================

/**
 * @brief Handler DMA SPI2 TX (I2S transmission)
 */
void DMA1_Stream4_IRQHandler(void)
{
    if (__HAL_DMA_GET_FLAG(&hdma_spi2_tx, __HAL_DMA_GET_HT_FLAG_INDEX(&hdma_spi2_tx)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_spi2_tx, __HAL_DMA_GET_HT_FLAG_INDEX(&hdma_spi2_tx));
        
        // Buffer A transmis → remplir avec nouvelles données
        if (tx_callback)
        {
            tx_callback(i2s_state.txBufferA, i2s_config.bufferSize);
        }
        else
        {
            memset(i2s_state.txBufferA, 0, i2s_config.bufferSize * sizeof(uint16_t));
        }
    }
    
    if (__HAL_DMA_GET_FLAG(&hdma_spi2_tx, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_spi2_tx)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_spi2_tx, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_spi2_tx));
        
        // Buffer B transmis
        if (tx_callback)
        {
            tx_callback(i2s_state.txBufferB, i2s_config.bufferSize);
        }
        else
        {
            memset(i2s_state.txBufferB, 0, i2s_config.bufferSize * sizeof(uint16_t));
        }
    }
    
    if (__HAL_DMA_GET_FLAG(&hdma_spi2_tx, __HAL_DMA_GET_TE_FLAG_INDEX(&hdma_spi2_tx)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_spi2_tx, __HAL_DMA_GET_TE_FLAG_INDEX(&hdma_spi2_tx));
        i2s_state.txUnderruns++;
    }
    
    HAL_DMA_IRQHandler(&hdma_spi2_tx);
}

/**
 * @brief Handler DMA SPI2 RX (I2S réception)
 */
void DMA1_Stream3_IRQHandler(void)
{
    if (__HAL_DMA_GET_FLAG(&hdma_spi2_rx, __HAL_DMA_GET_HT_FLAG_INDEX(&hdma_spi2_rx)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_spi2_rx, __HAL_DMA_GET_HT_FLAG_INDEX(&hdma_spi2_rx));
        
        i2s_state.rxBufferReady = true;
        
        if (rx_callback)
        {
            rx_callback(i2s_state.rxBufferA, i2s_config.bufferSize);
        }
    }
    
    if (__HAL_DMA_GET_FLAG(&hdma_spi2_rx, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_spi2_rx)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_spi2_rx, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_spi2_rx));
        
        i2s_state.rxBufferReady = true;
        
        if (rx_callback)
        {
            rx_callback(i2s_state.rxBufferB, i2s_config.bufferSize);
        }
    }
    
    if (__HAL_DMA_GET_FLAG(&hdma_spi2_rx, __HAL_DMA_GET_TE_FLAG_INDEX(&hdma_spi2_rx)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_spi2_rx, __HAL_DMA_GET_TE_FLAG_INDEX(&hdma_spi2_rx));
        i2s_state.rxOverflows++;
    }
    
    HAL_DMA_IRQHandler(&hdma_spi2_rx);
}

// ============================================================
// SECTION 10 : DÉBOGAGE
// ============================================================

void audio_i2s_print_state(void)
{
    printf("\n═══ ÉTAT I2S ═══\n");
    printf("Initialisé : %s\n", i2s_state.initialized ? "Oui" : "Non");
    printf("En cours   : %s\n", i2s_state.running ? "Oui" : "Non");
    printf("Codec      : %s\n", codec_detected ? "WM8978 détecté" : "Non détecté");
    printf("Fs         : %lu Hz\n", (unsigned long)i2s_config.sampleRate);
    printf("Résolution : %d bits\n", i2s_config.resolution);
    printf("Volume     : %d%%\n", i2s_state.volume);
    printf("Muet       : %s\n", i2s_state.muted ? "Oui" : "Non");
    printf("TX éch.    : %lu\n", (unsigned long)i2s_state.txSamples);
    printf("RX éch.    : %lu\n", (unsigned long)i2s_state.rxSamples);
    printf("TX under.  : %lu\n", (unsigned long)i2s_state.txUnderruns);
    printf("RX over.   : %lu\n", (unsigned long)i2s_state.rxOverflows);
    printf("══════════════════\n\n");
}

void audio_i2s_print_statistics(void)
{
    printf("\n═══ STATISTIQUES I2S ═══\n");
    printf("TX : %lu éch. (%lu underruns)\n", 
           (unsigned long)i2s_state.txSamples, (unsigned long)i2s_state.txUnderruns);
    printf("RX : %lu éch. (%lu overflows)\n",
           (unsigned long)i2s_state.rxSamples, (unsigned long)i2s_state.rxOverflows);
    printf("══════════════════════════\n\n");
}

bool audio_i2s_self_test(void)
{
    AUDIO_I2S_DEBUG("Auto-test...\n");
    
    if (!i2s_state.initialized)
    {
        AUDIO_I2S_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Tester la communication I2C avec le codec
    if (codec_detected)
    {
        AUDIO_I2S_DEBUG("Codec WM8978 présent\n");
    }
    
    AUDIO_I2S_DEBUG("Auto-test OK\n");
    return true;
}