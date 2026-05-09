/**
 * @file settings_nvram.h
 * @brief Gestion des paramètres persistants (NVRAM)
 * 
 * Ce fichier gère le stockage et la restauration de TOUS les
 * paramètres de configuration du téléphone LoRa.
 * 
 * Il utilise le module flash_eeprom pour le stockage physique
 * et fournit une API de haut niveau pour :
 * - Les paramètres généraux (volume, luminosité, etc.)
 * - Les paramètres réseau (fréquence, puissance, etc.)
 * - Les paramètres audio (volume, mode muet, etc.)
 * - Les paramètres d'affichage (rotation, timeout, etc.)
 * - Les informations du dispositif (nom, numéro, etc.)
 * 
 * Structure des données sauvegardées :
 * ┌──────────────────────────────────────────────────────────┐
 * │ Settings_General      (32 octets)                       │
 * │ Settings_Network      (64 octets)                       │
 * │ Settings_Audio        (32 octets)                       │
 * │ Settings_Display      (16 octets)                       │
 * │ Settings_Device       (48 octets)                       │
 * │ Settings_Security     (16 octets)                       │
 * │ Settings_Power        (16 octets)                       │
 * └──────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SETTINGS_NVRAM_H
#define SETTINGS_NVRAM_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "flash_eeprom.h"
#include "../../config.h"
#include "../../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define SETTINGS_NVRAM_VERSION          "1.0.0"

/** @brief Signature magique pour validation */
#define SETTINGS_MAGIC                  0x53455454  // "SETT" en little-endian

/** @brief Version du format des paramètres */
#define SETTINGS_FORMAT_VERSION         1

/** @brief Taille maximale d'un nom de dispositif */
#define SETTINGS_DEVICE_NAME_MAX        16

/** @brief Taille maximale d'un numéro de téléphone */
#define SETTINGS_PHONE_NUMBER_MAX       16

/** @brief Taille maximale d'un code PIN */
#define SETTINGS_PIN_MAX                5

// ============================================================
// SECTION 2 : STRUCTURES DE PARAMÈTRES
// ============================================================

/**
 * @brief Paramètres généraux
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;                     // Signature magique
    uint16_t version;                   // Version du format
    uint8_t language;                   // Langue (0=FR, 1=EN, etc.)
    uint8_t timezone;                   // Fuseau horaire
    bool keypadTones;                   // Tonalités clavier
    bool vibrationEnabled;              // Vibreur activé
    uint8_t startupMelody;              // Mélodie de démarrage
    uint8_t shutdownMelody;            // Mélodie d'arrêt
    uint16_t autoLockTimeout;           // Verrouillage auto (secondes)
    uint8_t reserved[16];              // Réservé pour futur usage
} Settings_General;

/**
 * @brief Paramètres réseau LoRa
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint32_t frequency;                 // Fréquence (Hz)
    uint8_t spreadingFactor;            // Spreading Factor (6-12)
    uint32_t bandwidth;                 // Bande passante (Hz)
    uint8_t codingRate;                 // Coding Rate (5-8)
    int8_t txPower;                     // Puissance TX (dBm)
    uint8_t syncWord;                   // Mot de synchronisation
    bool crcEnabled;                    // CRC activé
    bool implicitHeader;                // Header implicite
    uint8_t maxRetries;                 // Tentatives max
    uint16_t txTimeoutMs;               // Timeout TX (ms)
    uint16_t rxTimeoutMs;               // Timeout RX (ms)
    bool discoveryEnabled;              // Découverte réseau activée
    uint8_t reserved[32];              // Réservé
} Settings_Network;

/**
 * @brief Paramètres audio
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint8_t speakerVolume;              // Volume HP (0-100)
    uint8_t micGain;                    // Gain micro (0-100)
    uint8_t ringtoneVolume;            // Volume sonnerie (0-100)
    uint8_t dtmfVolume;                // Volume DTMF (0-100)
    bool muteEnabled;                   // Mode muet
    bool speakerEnabled;                // HP activé
    uint8_t ringtoneIndex;             // Sonnerie sélectionnée
    uint32_t sampleRate;               // Fréquence échantillonnage
    bool adpcmEnabled;                  // Compression ADPCM
    uint8_t adpcmMode;                  // Mode ADPCM
    uint8_t reserved[16];              // Réservé
} Settings_Audio;

/**
 * @brief Paramètres d'affichage
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint8_t brightness;                 // Luminosité (0-255)
    uint8_t rotation;                   // Rotation (0, 90, 180, 270)
    uint16_t screenTimeoutS;            // Extinction écran (s)
    bool nightModeEnabled;              // Mode nuit activé
    uint8_t nightModeStartHour;         // Heure début mode nuit
    uint8_t nightModeEndHour;           // Heure fin mode nuit
    uint8_t themeIndex;                 // Thème de couleurs
    bool largeFonts;                    // Grandes polices
    uint8_t reserved[4];               // Réservé
} Settings_Display;

/**
 * @brief Informations du dispositif
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    char deviceName[SETTINGS_DEVICE_NAME_MAX];      // Nom du dispositif
    char phoneNumber[SETTINGS_PHONE_NUMBER_MAX];    // Numéro de téléphone
    uint32_t deviceUID;                             // UID matériel
    bool firstBoot;                                 // Premier démarrage
    uint32_t totalUptime;                           // Temps de fonctionnement total
    uint8_t reserved[10];                           // Réservé
} Settings_Device;

/**
 * @brief Paramètres de sécurité
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    char pinCode[SETTINGS_PIN_MAX];     // Code PIN
    bool pinEnabled;                    // PIN activé
    bool lockOnStartup;                 // Verrouiller au démarrage
    uint8_t maxPinAttempts;            // Tentatives PIN max
    bool factoryResetProtection;       // Protection réinitialisation
    uint8_t reserved[8];               // Réservé
} Settings_Security;

/**
 * @brief Paramètres d'alimentation
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t sleepTimeoutS;             // Avant veille (s)
    uint16_t stopTimeoutS;              // Avant stop (s)
    uint16_t standbyTimeoutS;           // Avant standby (s)
    bool autoSleepEnabled;              // Veille automatique
    bool autoStopEnabled;               // Stop automatique
    uint8_t lowBatteryThreshold;        // Seuil batterie faible
    bool powerSavingMode;               // Mode économie
    uint8_t reserved[8];               // Réservé
} Settings_Power;

// ============================================================
// SECTION 3 : ÉTAT DU MODULE
// ============================================================

/**
 * @brief État du module de paramètres
 */
typedef struct {
    bool initialized;                   // Module initialisé
    bool loaded;                        // Paramètres chargés
    bool modified;                      // Paramètres modifiés
    uint32_t lastSave;                  // Dernière sauvegarde
    uint32_t saveCount;                 // Nombre de sauvegardes
} Settings_State;

// ============================================================
// SECTION 4 : FONCTIONS D'INITIALISATION
// ============================================================

bool settings_nvram_init(void);
void settings_nvram_deinit(void);
bool settings_nvram_is_ready(void);

// ============================================================
// SECTION 5 : FONCTIONS DE CHARGEMENT/SAUVEGARDE
// ============================================================

/**
 * @brief Charge tous les paramètres depuis la Flash
 * @return true si chargés avec succès
 */
bool settings_nvram_load_all(void);

/**
 * @brief Sauvegarde tous les paramètres modifiés
 * @return true si sauvegardés
 */
bool settings_nvram_save_all(void);

/**
 * @brief Sauvegarde un type de paramètres spécifique
 */
bool settings_nvram_save_general(void);
bool settings_nvram_save_network(void);
bool settings_nvram_save_audio(void);
bool settings_nvram_save_display(void);
bool settings_nvram_save_device(void);
bool settings_nvram_save_security(void);
bool settings_nvram_save_power(void);

/**
 * @brief Restaure les paramètres d'usine
 */
void settings_nvram_factory_reset(void);

/**
 * @brief Vérifie si les paramètres ont été modifiés
 */
bool settings_nvram_is_modified(void);

// ============================================================
// SECTION 6 : GETTERS/SETTERS - GÉNÉRAL
// ============================================================

uint8_t settings_get_language(void);
void settings_set_language(uint8_t language);
uint8_t settings_get_timezone(void);
void settings_set_timezone(uint8_t timezone);
bool settings_get_keypad_tones(void);
void settings_set_keypad_tones(bool enabled);
bool settings_get_vibration(void);
void settings_set_vibration(bool enabled);
uint16_t settings_get_auto_lock_timeout(void);
void settings_set_auto_lock_timeout(uint16_t seconds);

// ============================================================
// SECTION 7 : GETTERS/SETTERS - RÉSEAU
// ============================================================

uint32_t settings_get_frequency(void);
void settings_set_frequency(uint32_t frequency);
uint8_t settings_get_spreading_factor(void);
void settings_set_spreading_factor(uint8_t sf);
uint32_t settings_get_bandwidth(void);
void settings_set_bandwidth(uint32_t bw);
uint8_t settings_get_coding_rate(void);
void settings_set_coding_rate(uint8_t cr);
int8_t settings_get_tx_power(void);
void settings_set_tx_power(int8_t power);
bool settings_get_discovery_enabled(void);
void settings_set_discovery_enabled(bool enabled);

// ============================================================
// SECTION 8 : GETTERS/SETTERS - AUDIO
// ============================================================

uint8_t settings_get_speaker_volume(void);
void settings_set_speaker_volume(uint8_t volume);
uint8_t settings_get_mic_gain(void);
void settings_set_mic_gain(uint8_t gain);
uint8_t settings_get_ringtone_volume(void);
void settings_set_ringtone_volume(uint8_t volume);
bool settings_get_mute(void);
void settings_set_mute(bool mute);
uint8_t settings_get_ringtone_index(void);
void settings_set_ringtone_index(uint8_t index);
bool settings_get_adpcm_enabled(void);
void settings_set_adpcm_enabled(bool enabled);

// ============================================================
// SECTION 9 : GETTERS/SETTERS - AFFICHAGE
// ============================================================

uint8_t settings_get_brightness(void);
void settings_set_brightness(uint8_t brightness);
uint16_t settings_get_screen_timeout(void);
void settings_set_screen_timeout(uint16_t seconds);
bool settings_get_night_mode(void);
void settings_set_night_mode(bool enabled);
uint8_t settings_get_theme(void);
void settings_set_theme(uint8_t theme);

// ============================================================
// SECTION 10 : GETTERS/SETTERS - DISPOSITIF
// ============================================================

const char* settings_get_device_name(void);
void settings_set_device_name(const char* name);
const char* settings_get_phone_number(void);
void settings_set_phone_number(const char* number);
bool settings_is_first_boot(void);
void settings_set_first_boot_done(void);

// ============================================================
// SECTION 11 : GETTERS/SETTERS - SÉCURITÉ
// ============================================================

const char* settings_get_pin(void);
void settings_set_pin(const char* pin);
bool settings_get_pin_enabled(void);
void settings_set_pin_enabled(bool enabled);
uint8_t settings_get_max_pin_attempts(void);

// ============================================================
// SECTION 12 : GETTERS/SETTERS - ALIMENTATION
// ============================================================

uint16_t settings_get_sleep_timeout(void);
void settings_set_sleep_timeout(uint16_t seconds);
bool settings_get_auto_sleep(void);
void settings_set_auto_sleep(bool enabled);
bool settings_get_power_saving(void);
void settings_set_power_saving(bool enabled);

// ============================================================
// SECTION 13 : FONCTIONS DE DÉBOGAGE
// ============================================================

void settings_nvram_print_all(void);
void settings_nvram_print_general(void);
void settings_nvram_print_network(void);
void settings_nvram_print_audio(void);
void settings_nvram_print_display(void);
void settings_nvram_print_device(void);
bool settings_nvram_self_test(void);

// ============================================================
// SECTION 14 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define SETTINGS_DEBUG(fmt, ...)    printf("[SETTINGS] " fmt, ##__VA_ARGS__)
#else
    #define SETTINGS_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SETTINGS_NVRAM_H