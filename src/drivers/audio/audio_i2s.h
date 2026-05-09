/**
 * @file audio_i2s.h
 * @brief Driver pour l'interface audio I2S (Inter-IC Sound)
 * 
 * Ce fichier gère la communication audio digitale via l'interface
 * I2S (ou SAI) pour connecter un codec audio externe.
 * 
 * L'I2S permet une qualité audio supérieure à l'ADC/DAC interne :
 * - Résolution : 16/24/32 bits
 * - Fréquence : jusqu'à 192 kHz
 * - Full duplex (entrée + sortie simultanées)
 * - Codecs supportés : WM8978, VS1053, PCM5102, etc.
 * 
 * Brochage I2S2 sur STM32F429 :
 * - PB12 : I2S2_WS  (Word Select / Left-Right Clock)
 * - PB13 : I2S2_CK  (Serial Clock / Bit Clock)
 * - PB15 : I2S2_SD  (Serial Data)
 * - PC6  : I2S2_MCK (Master Clock, optionnel)
 * 
 * Utilisation typique avec un codec WM8978 :
 * - I2S pour les données audio
 * - I2C pour la configuration du codec
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H

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
#define AUDIO_I2S_VERSION               "1.0.0"

/** @brief Instance I2S utilisée (I2S2) */
#define AUDIO_I2S_INSTANCE              SPI2

/** @brief Handle I2S */
#define AUDIO_I2S_HANDLE                hspi2

/** @brief Fréquences d'échantillonnage supportées */
typedef enum {
    I2S_SAMPLE_RATE_8K   = 8000,       // 8 kHz (téléphonie)
    I2S_SAMPLE_RATE_16K  = 16000,      // 16 kHz (VoIP HD)
    I2S_SAMPLE_RATE_22K  = 22050,      // 22.05 kHz
    I2S_SAMPLE_RATE_44K  = 44100,      // 44.1 kHz (CD)
    I2S_SAMPLE_RATE_48K  = 48000,      // 48 kHz (DVD)
    I2S_SAMPLE_RATE_96K  = 96000,      // 96 kHz (HD)
    I2S_SAMPLE_RATE_192K = 192000      // 192 kHz (Studio)
} I2S_SampleRate;

/** @brief Résolution audio supportée */
typedef enum {
    I2S_RESOLUTION_16BIT = 16,         // 16 bits
    I2S_RESOLUTION_24BIT = 24,         // 24 bits
    I2S_RESOLUTION_32BIT = 32          // 32 bits
} I2S_Resolution;

/** @brief Mode I2S */
typedef enum {
    I2S_MODE_SLAVE_RX  = 0,            // Réception esclave
    I2S_MODE_SLAVE_TX  = 1,            // Transmission esclave
    I2S_MODE_MASTER_RX = 2,            // Réception maître
    I2S_MODE_MASTER_TX = 3             // Transmission maître
} I2S_Mode;

/** @brief Standard I2S */
typedef enum {
    I2S_STANDARD_PHILIPS   = 0,        // Standard Philips
    I2S_STANDARD_MSB       = 1,        // MSB Justifié
    I2S_STANDARD_LSB       = 2,        // LSB Justifié
    I2S_STANDARD_PCM_SHORT = 3,        // PCM court
    I2S_STANDARD_PCM_LONG  = 4         // PCM long
} I2S_Standard;

/** @brief Taille du buffer DMA */
#define AUDIO_I2S_BUFFER_SIZE           1024     // Échantillons par buffer

/** @brief Nombre de buffers DMA (double buffering) */
#define AUDIO_I2S_BUFFER_COUNT          2

// ============================================================
// SECTION 2 : CONFIGURATION
// ============================================================

/**
 * @brief Configuration de l'interface I2S
 */
typedef struct {
    I2S_Mode mode;                      // Mode (maître/esclave, RX/TX)
    I2S_Standard standard;              // Standard (Philips, MSB, LSB...)
    I2S_SampleRate sampleRate;          // Fréquence d'échantillonnage
    I2S_Resolution resolution;          // Résolution (16/24/32 bits)
    uint16_t bufferSize;                // Taille du buffer DMA
    bool enableDoubleBuffering;         // Double buffering
    bool enableMasterClock;             // Activer MCLK (pour codec)
    uint8_t masterClockDivider;         // Diviseur MCLK
    uint8_t volume;                     // Volume initial (0-100)
    bool startMuted;                    // Démarrer en muet
} AudioI2S_Config;

// ============================================================
// SECTION 3 : ÉTAT
// ============================================================

/**
 * @brief État de l'interface I2S
 */
typedef struct {
    bool initialized;                   // Driver initialisé
    bool running;                       // Transfert en cours
    bool txBufferReady;                 // Buffer TX prêt
    bool rxBufferReady;                 // Buffer RX prêt
    
    // Buffers DMA TX
    uint16_t txDmaBuffer[AUDIO_I2S_BUFFER_SIZE * AUDIO_I2S_BUFFER_COUNT];
    uint16_t* txBufferA;
    uint16_t* txBufferB;
    
    // Buffers DMA RX
    uint16_t rxDmaBuffer[AUDIO_I2S_BUFFER_SIZE * AUDIO_I2S_BUFFER_COUNT];
    uint16_t* rxBufferA;
    uint16_t* rxBufferB;
    
    // Volume
    uint8_t volume;
    bool muted;
    
    // Statistiques
    uint32_t txSamples;
    uint32_t rxSamples;
    uint32_t txUnderruns;
    uint32_t rxOverflows;
    
    AudioI2S_Config config;
} AudioI2S_State;

// ============================================================
// SECTION 4 : CALLBACKS
// ============================================================

/**
 * @brief Callback pour remplir le buffer TX
 * @param buffer Buffer à remplir
 * @param size Nombre d'échantillons
 */
typedef void (*AudioI2S_TXCallback)(uint16_t* buffer, uint16_t size);

/**
 * @brief Callback quand un buffer RX est plein
 * @param buffer Buffer reçu
 * @param size Nombre d'échantillons
 */
typedef void (*AudioI2S_RXCallback)(const uint16_t* buffer, uint16_t size);

/**
 * @brief Callback d'erreur
 * @param error Code d'erreur
 */
typedef void (*AudioI2S_ErrorCallback)(uint32_t error);

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise l'interface I2S
 * @param config Configuration (NULL = défaut)
 * @return true si succès
 */
bool audio_i2s_init(const AudioI2S_Config* config);

/**
 * @brief Désinitialise l'interface
 */
void audio_i2s_deinit(void);

/**
 * @brief Vérifie si l'interface est prête
 * @return true si initialisé
 */
bool audio_i2s_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS DE CONTRÔLE
// ============================================================

/**
 * @brief Démarre le transfert I2S
 */
void audio_i2s_start(void);

/**
 * @brief Arrête le transfert
 */
void audio_i2s_stop(void);

/**
 * @brief Vérifie si le transfert est en cours
 */
bool audio_i2s_is_running(void);

/**
 * @brief Met en pause
 */
void audio_i2s_pause(void);

/**
 * @brief Reprend
 */
void audio_i2s_resume(void);

// ============================================================
// SECTION 7 : FONCTIONS DE TRANSMISSION
// ============================================================

/**
 * @brief Vérifie si le buffer TX est prêt
 */
bool audio_i2s_tx_ready(void);

/**
 * @brief Écrit des données dans le buffer TX
 * @param data Données audio
 * @param size Nombre d'échantillons
 * @return true si écrit
 */
bool audio_i2s_tx_write(const uint16_t* data, uint16_t size);

// ============================================================
// SECTION 8 : FONCTIONS DE RÉCEPTION
// ============================================================

/**
 * @brief Vérifie si le buffer RX est prêt
 */
bool audio_i2s_rx_ready(void);

/**
 * @brief Lit les données du buffer RX
 * @param data Buffer de réception
 * @param size Nombre d'échantillons
 * @return true si lu
 */
bool audio_i2s_rx_read(uint16_t* data, uint16_t* size);

// ============================================================
// SECTION 9 : FONCTIONS DE VOLUME
// ============================================================

/**
 * @brief Définit le volume
 * @param volume Volume (0-100)
 */
void audio_i2s_set_volume(uint8_t volume);

/**
 * @brief Récupère le volume
 */
uint8_t audio_i2s_get_volume(void);

/**
 * @brief Active/désactive le muet
 */
void audio_i2s_set_mute(bool mute);

/**
 * @brief Bascule le muet
 */
void audio_i2s_toggle_mute(void);

// ============================================================
// SECTION 10 : FONCTIONS DE CONFIGURATION
// ============================================================

/**
 * @brief Change la fréquence d'échantillonnage
 */
void audio_i2s_set_sample_rate(I2S_SampleRate rate);

/**
 * @brief Change la résolution
 */
void audio_i2s_set_resolution(I2S_Resolution resolution);

// ============================================================
// SECTION 11 : FONCTIONS DE CALLBACKS
// ============================================================

void audio_i2s_set_tx_callback(AudioI2S_TXCallback callback);
void audio_i2s_set_rx_callback(AudioI2S_RXCallback callback);
void audio_i2s_set_error_callback(AudioI2S_ErrorCallback callback);

// ============================================================
// SECTION 12 : FONCTIONS CODEC EXTERNE (WM8978)
// ============================================================

/**
 * @brief Initialise le codec WM8978 via I2C
 * @return true si détecté
 */
bool audio_i2s_codec_wm8978_init(void);

/**
 * @brief Configure le volume du codec
 * @param volume Volume (0-100)
 */
void audio_i2s_codec_set_volume(uint8_t volume);

/**
 * @brief Configure le gain micro du codec
 * @param gain Gain (0-100)
 */
void audio_i2s_codec_set_mic_gain(uint8_t gain);

// ============================================================
// SECTION 13 : FONCTIONS DE DÉBOGAGE
// ============================================================

void audio_i2s_print_state(void);
void audio_i2s_print_statistics(void);
bool audio_i2s_self_test(void);

// ============================================================
// SECTION 14 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define AUDIO_I2S_DEBUG(fmt, ...)   printf("[AUDIO_I2S] " fmt, ##__VA_ARGS__)
#else
    #define AUDIO_I2S_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // AUDIO_I2S_H