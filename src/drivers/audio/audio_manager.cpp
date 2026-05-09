/**
 * @file audio_manager.cpp
 * @brief Implémentation du gestionnaire audio haut niveau
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans audio_manager.h.
 * 
 * Il unifie tous les modules audio et fournit :
 * - La gestion des appels full duplex
 * - La compression/décompression automatique
 * - Le mixage des sources audio
 * - Le contrôle du volume et mode muet
 * - Les tonalités DTMF et sonneries
 * - L'indicateur VU meter
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "audio_manager.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Configuration */
static AudioManager_Config audio_config = {
    .sampleRate = 8000,
    .volume = 80,
    .startMuted = false,
    .micGain = 100,
    .micFilterEnabled = false,
    .speakerEnabled = true,
    .compressionEnabled = true,
    .compressionMode = ADPCM_MODE_4BIT,
    .mixerEnabled = true,
    .ringtoneVolume = 80,
    .dtmfVolume = 60,
    .dtmfDurationMs = 200,
    .vuMeterEnabled = true,
    .vuMeterIntervalMs = 100
};

/** @brief Mode actuel */
static AudioManager_Mode current_mode = AUDIO_MODE_OFF;

/** @brief État d'appel */
static bool in_call = false;
static bool tx_active = false;
static bool rx_active = false;

/** @brief Encodeur/décodeur ADPCM */
static ADPCM_EncoderState adpcm_encoder;
static ADPCM_DecoderState adpcm_decoder;

/** @brief Buffers audio */
static uint8_t tx_compressed_buffer[128];   // Buffer compressé à envoyer
static uint16_t tx_compressed_size = 0;
static bool tx_buffer_ready = false;

static uint8_t rx_compressed_buffer[128];   // Buffer compressé reçu
static uint16_t rx_compressed_size = 0;
static bool rx_buffer_ready = false;

/** @brief Niveau VU */
static uint8_t vu_level = 0;
static uint8_t vu_peak = 0;
static uint32_t last_vu_update = 0;

/** @brief Callbacks */
static AudioManager_TXCallback tx_callback = NULL;
static AudioManager_RXCallback rx_callback = NULL;
static AudioManager_VUCallback vu_callback = NULL;
static AudioManager_EventCallback event_callback = NULL;

/** @brief État d'initialisation */
static bool initialized = false;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le gestionnaire audio
 */
bool audio_manager_init(const AudioManager_Config* config)
{
    AUDIO_MGR_DEBUG("Initialisation du gestionnaire audio...\n");
    
    if (config != NULL)
    {
        memcpy(&audio_config, config, sizeof(AudioManager_Config));
    }
    
    // 1. Initialiser l'ADC (microphone)
    AudioADC_Config adcConfig = {
        .sampleRate = audio_config.sampleRate,
        .bufferSize = AUDIO_ADC_BUFFER_SIZE,
        .enableDoubleBuffering = true,
        .enableMedianFilter = audio_config.micFilterEnabled,
        .gain = audio_config.micGain,
        .enableVUMeter = audio_config.vuMeterEnabled
    };
    
    if (!audio_adc_init(&adcConfig))
    {
        AUDIO_MGR_DEBUG("Échec initialisation ADC\n");
        return false;
    }
    
    // 2. Initialiser le DAC (haut-parleur)
    AudioDAC_Config dacConfig = {
        .sampleRate = audio_config.sampleRate,
        .bufferSize = AUDIO_DAC_BUFFER_SIZE,
        .volume = audio_config.volume,
        .startMuted = audio_config.startMuted,
        .enableSoftStart = true
    };
    
    if (!audio_dac_init(&dacConfig))
    {
        AUDIO_MGR_DEBUG("Échec initialisation DAC\n");
        return false;
    }
    
    // 3. Initialiser le codec ADPCM
    adpcm_encoder_init(&adpcm_encoder, audio_config.compressionMode);
    adpcm_decoder_init(&adpcm_decoder, audio_config.compressionMode);
    
    // 4. Initialiser le mixer
    if (audio_config.mixerEnabled)
    {
        AudioMixer_Config mixerConfig = {
            .masterVolume = audio_config.volume,
            .enableLimiter = true,
            .limiterThreshold = 30000
        };
        audio_mixer_init(&mixerConfig);
    }
    
    // 5. Configurer les callbacks internes
    audio_adc_set_buffer_callback(on_adc_buffer_ready);
    audio_dac_set_buffer_callback(on_dac_buffer_needed);
    audio_adc_set_vu_callback(on_vu_level_changed);
    
    // 6. Démarrer en mode IDLE
    current_mode = AUDIO_MODE_IDLE;
    initialized = true;
    
    AUDIO_MGR_DEBUG("Gestionnaire audio initialisé (Fs=%lu Hz)\n", 
                   (unsigned long)audio_config.sampleRate);
    
    return true;
}

/**
 * @brief Désinitialise
 */
void audio_manager_deinit(void)
{
    audio_manager_stop_call();
    audio_adc_deinit();
    audio_dac_deinit();
    initialized = false;
    current_mode = AUDIO_MODE_OFF;
}

/**
 * @brief Vérifie si prêt
 */
bool audio_manager_is_ready(void)
{
    return initialized;
}

// ============================================================
// SECTION 2 : CONTRÔLE DE MODE
// ============================================================

/**
 * @brief Définit le mode audio
 */
void audio_manager_set_mode(AudioManager_Mode mode)
{
    if (!initialized) return;
    
    AUDIO_MGR_DEBUG("Changement mode: %d → %d\n", current_mode, mode);
    
    // Arrêter le mode actuel
    switch (current_mode)
    {
        case AUDIO_MODE_CALL:
        case AUDIO_MODE_TX_ONLY:
            audio_adc_stop();
            break;
        case AUDIO_MODE_CALL:
        case AUDIO_MODE_RX_ONLY:
        case AUDIO_MODE_RINGTONE:
        case AUDIO_MODE_TONE:
            audio_dac_stop();
            break;
        default:
            break;
    }
    
    // Démarrer le nouveau mode
    switch (mode)
    {
        case AUDIO_MODE_CALL:
            audio_adc_start();
            audio_dac_start();
            tx_active = true;
            rx_active = true;
            break;
            
        case AUDIO_MODE_TX_ONLY:
            audio_adc_start();
            tx_active = true;
            rx_active = false;
            break;
            
        case AUDIO_MODE_RX_ONLY:
            audio_dac_start();
            tx_active = false;
            rx_active = true;
            break;
            
        case AUDIO_MODE_RINGTONE:
            audio_dac_start();
            tx_active = false;
            rx_active = false;
            break;
            
        case AUDIO_MODE_TONE:
            audio_dac_start();
            tx_active = false;
            rx_active = false;
            break;
            
        case AUDIO_MODE_IDLE:
        case AUDIO_MODE_OFF:
        default:
            tx_active = false;
            rx_active = false;
            break;
    }
    
    current_mode = mode;
    
    if (event_callback)
    {
        event_callback(AUDIO_EVENT_NONE);  // Notifier le changement
    }
}

/**
 * @brief Récupère le mode
 */
AudioManager_Mode audio_manager_get_mode(void)
{
    return current_mode;
}

/**
 * @brief Démarre un appel
 */
void audio_manager_start_call(void)
{
    AUDIO_MGR_DEBUG("Démarrage appel audio\n");
    in_call = true;
    audio_manager_set_mode(AUDIO_MODE_CALL);
}

/**
 * @brief Termine un appel
 */
void audio_manager_stop_call(void)
{
    AUDIO_MGR_DEBUG("Fin appel audio\n");
    in_call = false;
    audio_manager_set_mode(AUDIO_MODE_IDLE);
    
    // Réinitialiser les codecs
    adpcm_encoder_reset(&adpcm_encoder);
    adpcm_decoder_reset(&adpcm_decoder);
}

/**
 * @brief Vérifie si en appel
 */
bool audio_manager_is_in_call(void)
{
    return in_call;
}

// ============================================================
// SECTION 3 : VOLUME
// ============================================================

void audio_manager_set_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    audio_config.volume = volume;
    audio_dac_set_volume(volume);
    if (audio_config.mixerEnabled)
    {
        audio_mixer_set_master_volume(volume);
    }
}

uint8_t audio_manager_get_volume(void) { return audio_config.volume; }

void audio_manager_set_mute(bool mute)
{
    audio_dac_set_mute(mute);
}

void audio_manager_toggle_mute(void)
{
    audio_dac_toggle_mute();
}

bool audio_manager_is_muted(void)
{
    return audio_dac_is_muted();
}

void audio_manager_set_mic_gain(uint8_t gain)
{
    audio_config.micGain = gain;
    audio_adc_set_gain(gain);
}

// ============================================================
// SECTION 4 : TRAITEMENT AUDIO
// ============================================================

/**
 * @brief Traitement périodique
 */
void audio_manager_process(void)
{
    if (!initialized) return;
    
    // Traiter le buffer TX (micro → compression → callback)
    if (tx_buffer_ready && tx_callback)
    {
        tx_callback(tx_compressed_buffer, tx_compressed_size);
        tx_buffer_ready = false;
    }
    
    // Traiter le buffer RX (callback → décompression → DAC)
    if (rx_buffer_ready)
    {
        // Décompresser
        uint8_t decompressed[256];
        uint16_t sampleCount = adpcm_decode_4to8(rx_compressed_buffer, 
                                                   rx_compressed_size, 
                                                   decompressed);
        
        // Convertir en int16_t pour le mixer
        int16_t samples16[256];
        for (uint16_t i = 0; i < sampleCount; i++)
        {
            samples16[i] = ((int16_t)decompressed[i] - 128) << 8;
        }
        
        // Écrire dans le mixer (source VOICE)
        if (audio_config.mixerEnabled)
        {
            audio_mixer_source_write(AUDIO_SOURCE_VOICE, samples16, sampleCount);
            
            // Mixer et envoyer au DAC
            int16_t mixed[256];
            audio_mixer_process(mixed, sampleCount);
            audio_dac_write_buffer((uint16_t*)mixed, sampleCount);
        }
        else
        {
            audio_dac_write_buffer((uint16_t*)samples16, sampleCount);
        }
        
        rx_buffer_ready = false;
    }
    
    // Mettre à jour le VU meter
    if (audio_config.vuMeterEnabled)
    {
        uint32_t now = HAL_GetTick();
        if ((now - last_vu_update) >= audio_config.vuMeterIntervalMs)
        {
            last_vu_update = now;
            
            // Lire le niveau depuis l'ADC
            AudioADC_State* adcState = audio_adc_get_state();
            vu_level = adcState->vuLevel;
            vu_peak = adcState->vuPeak;
            
            if (vu_callback)
            {
                vu_callback(vu_level, vu_peak);
            }
        }
    }
}

/**
 * @brief Callback interne : buffer ADC plein
 */
static void on_adc_buffer_ready(uint16_t* buffer, uint16_t size)
{
    if (!tx_active) return;
    
    // Convertir en 8 bits pour la compression
    uint8_t audio8[256];
    for (uint16_t i = 0; i < size; i++)
    {
        audio8[i] = (uint8_t)(buffer[i] >> 4);  // 12 bits → 8 bits
    }
    
    // Compresser
    if (audio_config.compressionEnabled)
    {
        tx_compressed_size = adpcm_encode_8to4(audio8, size, tx_compressed_buffer);
    }
    else
    {
        memcpy(tx_compressed_buffer, audio8, size);
        tx_compressed_size = size;
    }
    
    tx_buffer_ready = true;
    
    if (event_callback)
    {
        event_callback(AUDIO_EVENT_BUFFER_TX);
    }
}

/**
 * @brief Callback interne : buffer DAC à remplir
 */
static void on_dac_buffer_needed(uint16_t* buffer, uint16_t size)
{
    // Le DAC est rempli par le mixer dans audio_manager_process()
    // Ce callback est un fallback : remplir de silence si rien n'est prêt
    if (!rx_active)
    {
        for (uint16_t i = 0; i < size; i++)
        {
            buffer[i] = AUDIO_DAC_SILENCE_VALUE;
        }
    }
}

/**
 * @brief Callback interne : niveau VU changé
 */
static void on_vu_level_changed(uint8_t level, uint8_t peak)
{
    vu_level = level;
    vu_peak = peak;
    
    if (vu_callback)
    {
        vu_callback(level, peak);
    }
}

/**
 * @brief Reçoit des données audio (depuis LoRa)
 */
void audio_manager_receive_data(const uint8_t* data, uint16_t size)
{
    if (!rx_active) return;
    if (size > sizeof(rx_compressed_buffer)) size = sizeof(rx_compressed_buffer);
    
    memcpy(rx_compressed_buffer, data, size);
    rx_compressed_size = size;
    rx_buffer_ready = true;
    
    if (event_callback)
    {
        event_callback(AUDIO_EVENT_BUFFER_RX);
    }
}

/**
 * @brief Récupère les données à transmettre
 */
uint16_t audio_manager_get_transmit_data(uint8_t* data, uint16_t size)
{
    if (!tx_buffer_ready || data == NULL) return 0;
    
    uint16_t copySize = (tx_compressed_size < size) ? tx_compressed_size : size;
    memcpy(data, tx_compressed_buffer, copySize);
    tx_buffer_ready = false;
    
    return copySize;
}

// ============================================================
// SECTION 5 : TONALITÉS
// ============================================================

void audio_manager_play_dtmf(char digit)
{
    AUDIO_MGR_DEBUG("DTMF: '%c'\n", digit);
    audio_manager_set_mode(AUDIO_MODE_TONE);
    audio_dac_play_dtmf(digit, audio_config.dtmfDurationMs);
}

void audio_manager_play_dtmf_string(const char* digits)
{
    if (digits == NULL) return;
    
    AUDIO_MGR_DEBUG("DTMF string: %s\n", digits);
    
    for (const char* p = digits; *p != '\0'; p++)
    {
        audio_manager_play_dtmf(*p);
        HAL_Delay(audio_config.dtmfDurationMs + 50);  // Pause entre les digits
    }
    
    // Revenir au mode précédent après la séquence
    if (in_call)
    {
        audio_manager_set_mode(AUDIO_MODE_CALL);
    }
    else
    {
        audio_manager_set_mode(AUDIO_MODE_IDLE);
    }
}

void audio_manager_play_ringtone(uint8_t index)
{
    AUDIO_MGR_DEBUG("Sonnerie %d\n", index);
    audio_manager_set_mode(AUDIO_MODE_RINGTONE);
    audio_dac_set_volume(audio_config.ringtoneVolume);
    audio_dac_play_ringtone(index);
}

void audio_manager_stop_ringtone(void)
{
    audio_dac_stop_tone();
}

void audio_manager_play_beep(uint16_t frequency, uint16_t durationMs)
{
    audio_manager_set_mode(AUDIO_MODE_TONE);
    audio_dac_play_tone_sine(frequency, durationMs, 60);
}

void audio_manager_play_alert(void)
{
    // Alerte : 3 bips courts
    for (int i = 0; i < 3; i++)
    {
        audio_manager_play_beep(1000, 150);
        HAL_Delay(150);
    }
}

// ============================================================
// SECTION 6 : VU METER
// ============================================================

void audio_manager_vu_enable(bool enable)
{
    audio_config.vuMeterEnabled = enable;
}

uint8_t audio_manager_get_vu_level(void)
{
    return vu_level;
}

uint8_t audio_manager_get_vu_peak(void)
{
    return vu_peak;
}

bool audio_manager_is_speaking(void)
{
    return (vu_level > 20);  // Seuil de détection de parole
}

// ============================================================
// SECTION 7 : CALLBACKS
// ============================================================

void audio_manager_set_tx_callback(AudioManager_TXCallback callback)
{
    tx_callback = callback;
}

void audio_manager_set_rx_callback(AudioManager_RXCallback callback)
{
    rx_callback = callback;
}

void audio_manager_set_vu_callback(AudioManager_VUCallback callback)
{
    vu_callback = callback;
}

void audio_manager_set_event_callback(AudioManager_EventCallback callback)
{
    event_callback = callback;
}

// ============================================================
// SECTION 8 : DÉBOGAGE
// ============================================================

void audio_manager_print_state(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║     ÉTAT GESTIONNAIRE AUDIO                   ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Initialisé   : %-31s ║\n", initialized ? "Oui" : "Non");
    printf("║ Mode         : %-31d ║\n", current_mode);
    printf("║ En appel     : %-31s ║\n", in_call ? "Oui" : "Non");
    printf("║ Volume       : %-31d ║\n", audio_config.volume);
    printf("║ Muet         : %-31s ║\n", audio_dac_is_muted() ? "Oui" : "Non");
    printf("║ Compression  : %-31s ║\n", audio_config.compressionEnabled ? 
         (audio_config.compressionMode == ADPCM_MODE_4BIT ? "ADPCM 4:1" : "ADPCM 8:1") : "Aucune");
    printf("║ Fréquence    : %-31lu ║\n", (unsigned long)audio_config.sampleRate);
    printf("║ VU Level     : %-31d ║\n", vu_level);
    printf("║ VU Peak      : %-31d ║\n", vu_peak);
    printf("║ Parole       : %-31s ║\n", audio_manager_is_speaking() ? "Oui" : "Non");
    printf("║ TX Buffer    : %-31s ║\n", tx_buffer_ready ? "Prêt" : "Vide");
    printf("║ RX Buffer    : %-31s ║\n", rx_buffer_ready ? "Prêt" : "Vide");
    printf("╚══════════════════════════════════════════════╝\n\n");
}

void audio_manager_print_statistics(void)
{
    printf("\n═══ STATISTIQUES AUDIO ═══\n");
    
    AudioADC_State* adcState = audio_adc_get_state();
    printf("ADC : %lu échantillons, %lu overflows\n", 
           (unsigned long)adcState->totalSamples, 
           (unsigned long)adcState->overflows);
    
    AudioDAC_State* dacState = audio_dac_get_state();
    printf("DAC : %lu échantillons, %lu underruns\n",
           (unsigned long)dacState->totalSamples,
           (unsigned long)dacState->underruns);
    
    printf("Compression : %.1f:1\n", adpcm_encoder.compressionRatio);
    printf("══════════════════════════\n\n");
}

bool audio_manager_self_test(void)
{
    AUDIO_MGR_DEBUG("Auto-test...\n");
    
    if (!initialized)
    {
        AUDIO_MGR_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Tester l'ADC
    if (!audio_adc_self_test())
    {
        AUDIO_MGR_DEBUG("Échec test ADC\n");
        return false;
    }
    
    // Tester le DAC
    if (!audio_dac_self_test())
    {
        AUDIO_MGR_DEBUG("Échec test DAC\n");
        return false;
    }
    
    // Tester l'ADPCM
    if (!adpcm_self_test())
    {
        AUDIO_MGR_DEBUG("Échec test ADPCM\n");
        return false;
    }
    
    AUDIO_MGR_DEBUG("Auto-test OK\n");
    return true;
}