/**
 * @file settings_nvram.cpp
 * @brief Implémentation du gestionnaire de paramètres persistants
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans settings_nvram.h.
 * 
 * Il gère :
 * - Le chargement de tous les paramètres depuis la Flash
 * - La sauvegarde des paramètres modifiés
 * - La restauration des valeurs d'usine
 * - Les getters/setters pour chaque catégorie
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "settings_nvram.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du module */
static Settings_State settings_state;

/** @brief Instances des paramètres (en RAM) */
static Settings_General   settings_general;
static Settings_Network   settings_network;
static Settings_Audio     settings_audio;
static Settings_Display   settings_display;
static Settings_Device    settings_device;
static Settings_Security  settings_security;
static Settings_Power     settings_power;

// ============================================================
// SECTION 1 : VALEURS PAR DÉFAUT
// ============================================================

/**
 * @brief Charge les valeurs par défaut (usine)
 */
static void load_defaults(void)
{
    SETTINGS_DEBUG("Chargement valeurs par défaut...\n");
    
    // --- Paramètres généraux ---
    memset(&settings_general, 0, sizeof(Settings_General));
    settings_general.magic = SETTINGS_MAGIC;
    settings_general.version = SETTINGS_FORMAT_VERSION;
    settings_general.language = 0;          // Français
    settings_general.timezone = 1;          // UTC+1 (Paris)
    settings_general.keypadTones = true;
    settings_general.vibrationEnabled = true;
    settings_general.startupMelody = 0;
    settings_general.shutdownMelody = 0;
    settings_general.autoLockTimeout = 30;  // 30 secondes
    
    // --- Paramètres réseau ---
    memset(&settings_network, 0, sizeof(Settings_Network));
    settings_network.magic = SETTINGS_MAGIC;
    settings_network.version = SETTINGS_FORMAT_VERSION;
    settings_network.frequency = 868000000;     // 868 MHz (Europe)
    settings_network.spreadingFactor = 7;        // SF7
    settings_network.bandwidth = 125000;        // 125 kHz
    settings_network.codingRate = 5;             // 4/5
    settings_network.txPower = 17;               // 17 dBm
    settings_network.syncWord = 0x34;            // Public
    settings_network.crcEnabled = true;
    settings_network.implicitHeader = false;
    settings_network.maxRetries = 3;
    settings_network.txTimeoutMs = 5000;
    settings_network.rxTimeoutMs = 10000;
    settings_network.discoveryEnabled = true;
    
    // --- Paramètres audio ---
    memset(&settings_audio, 0, sizeof(Settings_Audio));
    settings_audio.magic = SETTINGS_MAGIC;
    settings_audio.version = SETTINGS_FORMAT_VERSION;
    settings_audio.speakerVolume = 80;
    settings_audio.micGain = 100;
    settings_audio.ringtoneVolume = 80;
    settings_audio.dtmfVolume = 60;
    settings_audio.muteEnabled = false;
    settings_audio.speakerEnabled = true;
    settings_audio.ringtoneIndex = 0;
    settings_audio.sampleRate = 8000;
    settings_audio.adpcmEnabled = true;
    settings_audio.adpcmMode = 1;  // ADPCM 4 bits
    
    // --- Paramètres affichage ---
    memset(&settings_display, 0, sizeof(Settings_Display));
    settings_display.magic = SETTINGS_MAGIC;
    settings_display.version = SETTINGS_FORMAT_VERSION;
    settings_display.brightness = 200;
    settings_display.rotation = 0;
    settings_display.screenTimeoutS = 30;
    settings_display.nightModeEnabled = true;
    settings_display.nightModeStartHour = 22;
    settings_display.nightModeEndHour = 6;
    settings_display.themeIndex = 0;
    settings_display.largeFonts = false;
    
    // --- Informations dispositif ---
    memset(&settings_device, 0, sizeof(Settings_Device));
    settings_device.magic = SETTINGS_MAGIC;
    settings_device.version = SETTINGS_FORMAT_VERSION;
    strncpy(settings_device.deviceName, "LoRaPhone", SETTINGS_DEVICE_NAME_MAX - 1);
    strncpy(settings_device.phoneNumber, "0600000000", SETTINGS_PHONE_NUMBER_MAX - 1);
    settings_device.firstBoot = true;
    settings_device.totalUptime = 0;
    
    // --- Paramètres sécurité ---
    memset(&settings_security, 0, sizeof(Settings_Security));
    settings_security.magic = SETTINGS_MAGIC;
    settings_security.version = SETTINGS_FORMAT_VERSION;
    strncpy(settings_security.pinCode, "0000", SETTINGS_PIN_MAX - 1);
    settings_security.pinEnabled = false;
    settings_security.lockOnStartup = false;
    settings_security.maxPinAttempts = 3;
    settings_security.factoryResetProtection = false;
    
    // --- Paramètres alimentation ---
    memset(&settings_power, 0, sizeof(Settings_Power));
    settings_power.magic = SETTINGS_MAGIC;
    settings_power.version = SETTINGS_FORMAT_VERSION;
    settings_power.sleepTimeoutS = 30;
    settings_power.stopTimeoutS = 300;
    settings_power.standbyTimeoutS = 1800;
    settings_power.autoSleepEnabled = true;
    settings_power.autoStopEnabled = true;
    settings_power.lowBatteryThreshold = 15;
    settings_power.powerSavingMode = false;
}

// ============================================================
// SECTION 2 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le module de paramètres
 */
bool settings_nvram_init(void)
{
    SETTINGS_DEBUG("Initialisation du module de paramètres...\n");
    
    memset(&settings_state, 0, sizeof(Settings_State));
    
    // Initialiser la Flash EEPROM
    if (flash_eeprom_init() != FLASH_EEPROM_OK)
    {
        SETTINGS_DEBUG("Échec initialisation Flash EEPROM\n");
        return false;
    }
    
    // Tenter de charger les paramètres
    if (!settings_nvram_load_all())
    {
        SETTINGS_DEBUG("Aucun paramètre valide, valeurs par défaut\n");
        load_defaults();
    }
    
    settings_state.initialized = true;
    settings_state.loaded = true;
    
    SETTINGS_DEBUG("Module initialisé\n");
    return true;
}

/**
 * @brief Désinitialise
 */
void settings_nvram_deinit(void)
{
    if (settings_state.modified)
    {
        settings_nvram_save_all();
    }
    settings_state.initialized = false;
}

/**
 * @brief Vérifie si prêt
 */
bool settings_nvram_is_ready(void)
{
    return settings_state.initialized;
}

// ============================================================
// SECTION 3 : CHARGEMENT/SAUVEGARDE
// ============================================================

/**
 * @brief Charge tous les paramètres depuis la Flash
 */
bool settings_nvram_load_all(void)
{
    SETTINGS_DEBUG("Chargement de tous les paramètres...\n");
    
    bool allLoaded = true;
    uint16_t readSize;
    FlashEEPROM_Error err;
    
    // Charger chaque catégorie
    err = flash_eeprom_read(EEPROM_ID_SETTINGS, (uint8_t*)&settings_general, 
                            sizeof(Settings_General), &readSize);
    if (err != FLASH_EEPROM_OK || settings_general.magic != SETTINGS_MAGIC)
    {
        SETTINGS_DEBUG("Paramètres généraux invalides\n");
        allLoaded = false;
    }
    
    err = flash_eeprom_read(EEPROM_ID_LORA_CONFIG, (uint8_t*)&settings_network,
                            sizeof(Settings_Network), &readSize);
    if (err != FLASH_EEPROM_OK || settings_network.magic != SETTINGS_MAGIC)
    {
        SETTINGS_DEBUG("Paramètres réseau invalides\n");
        allLoaded = false;
    }
    
    err = flash_eeprom_read(EEPROM_ID_AUDIO_CONFIG, (uint8_t*)&settings_audio,
                            sizeof(Settings_Audio), &readSize);
    if (err != FLASH_EEPROM_OK || settings_audio.magic != SETTINGS_MAGIC)
    {
        SETTINGS_DEBUG("Paramètres audio invalides\n");
        allLoaded = false;
    }
    
    err = flash_eeprom_read(EEPROM_ID_DISPLAY_CONFIG, (uint8_t*)&settings_display,
                            sizeof(Settings_Display), &readSize);
    if (err != FLASH_EEPROM_OK || settings_display.magic != SETTINGS_MAGIC)
    {
        SETTINGS_DEBUG("Paramètres affichage invalides\n");
        allLoaded = false;
    }
    
    err = flash_eeprom_read(EEPROM_ID_DEVICE_NAME, (uint8_t*)&settings_device,
                            sizeof(Settings_Device), &readSize);
    if (err != FLASH_EEPROM_OK || settings_device.magic != SETTINGS_MAGIC)
    {
        SETTINGS_DEBUG("Paramètres dispositif invalides\n");
        allLoaded = false;
    }
    
    err = flash_eeprom_read(EEPROM_ID_SECURITY_CONFIG, (uint8_t*)&settings_security,
                            sizeof(Settings_Security), &readSize);
    if (err != FLASH_EEPROM_OK || settings_security.magic != SETTINGS_MAGIC)
    {
        SETTINGS_DEBUG("Paramètres sécurité invalides\n");
        allLoaded = false;
    }
    
    err = flash_eeprom_read(EEPROM_ID_POWER_CONFIG, (uint8_t*)&settings_power,
                            sizeof(Settings_Power), &readSize);
    if (err != FLASH_EEPROM_OK || settings_power.magic != SETTINGS_MAGIC)
    {
        SETTINGS_DEBUG("Paramètres alimentation invalides\n");
        allLoaded = false;
    }
    
    if (!allLoaded)
    {
        load_defaults();
        settings_state.modified = true;
        return false;
    }
    
    settings_state.loaded = true;
    SETTINGS_DEBUG("Paramètres chargés avec succès\n");
    return true;
}

/**
 * @brief Sauvegarde tous les paramètres modifiés
 */
bool settings_nvram_save_all(void)
{
    SETTINGS_DEBUG("Sauvegarde de tous les paramètres...\n");
    
    bool allSaved = true;
    
    if (!settings_nvram_save_general()) allSaved = false;
    if (!settings_nvram_save_network()) allSaved = false;
    if (!settings_nvram_save_audio()) allSaved = false;
    if (!settings_nvram_save_display()) allSaved = false;
    if (!settings_nvram_save_device()) allSaved = false;
    if (!settings_nvram_save_security()) allSaved = false;
    if (!settings_nvram_save_power()) allSaved = false;
    
    if (allSaved)
    {
        settings_state.modified = false;
        settings_state.lastSave = HAL_GetTick();
        settings_state.saveCount++;
        SETTINGS_DEBUG("Tous les paramètres sauvegardés\n");
    }
    
    return allSaved;
}

/**
 * @brief Sauvegarde les paramètres généraux
 */
bool settings_nvram_save_general(void)
{
    FlashEEPROM_Error err = flash_eeprom_write(EEPROM_ID_SETTINGS,
                                                (uint8_t*)&settings_general,
                                                sizeof(Settings_General));
    return (err == FLASH_EEPROM_OK);
}

bool settings_nvram_save_network(void)
{
    FlashEEPROM_Error err = flash_eeprom_write(EEPROM_ID_LORA_CONFIG,
                                                (uint8_t*)&settings_network,
                                                sizeof(Settings_Network));
    return (err == FLASH_EEPROM_OK);
}

bool settings_nvram_save_audio(void)
{
    FlashEEPROM_Error err = flash_eeprom_write(EEPROM_ID_AUDIO_CONFIG,
                                                (uint8_t*)&settings_audio,
                                                sizeof(Settings_Audio));
    return (err == FLASH_EEPROM_OK);
}

bool settings_nvram_save_display(void)
{
    FlashEEPROM_Error err = flash_eeprom_write(EEPROM_ID_DISPLAY_CONFIG,
                                                (uint8_t*)&settings_display,
                                                sizeof(Settings_Display));
    return (err == FLASH_EEPROM_OK);
}

bool settings_nvram_save_device(void)
{
    FlashEEPROM_Error err = flash_eeprom_write(EEPROM_ID_DEVICE_NAME,
                                                (uint8_t*)&settings_device,
                                                sizeof(Settings_Device));
    return (err == FLASH_EEPROM_OK);
}

bool settings_nvram_save_security(void)
{
    FlashEEPROM_Error err = flash_eeprom_write(EEPROM_ID_SECURITY_CONFIG,
                                                (uint8_t*)&settings_security,
                                                sizeof(Settings_Security));
    return (err == FLASH_EEPROM_OK);
}

bool settings_nvram_save_power(void)
{
    FlashEEPROM_Error err = flash_eeprom_write(EEPROM_ID_POWER_CONFIG,
                                                (uint8_t*)&settings_power,
                                                sizeof(Settings_Power));
    return (err == FLASH_EEPROM_OK);
}

/**
 * @brief Restaure les paramètres d'usine
 */
void settings_nvram_factory_reset(void)
{
    SETTINGS_DEBUG("Restauration paramètres d'usine...\n");
    load_defaults();
    settings_nvram_save_all();
}

/**
 * @brief Vérifie si les paramètres ont été modifiés
 */
bool settings_nvram_is_modified(void)
{
    return settings_state.modified;
}

// ============================================================
// SECTION 4 : GETTERS/SETTERS - GÉNÉRAL
// ============================================================

uint8_t settings_get_language(void)          { return settings_general.language; }
void settings_set_language(uint8_t language) { settings_general.language = language; settings_state.modified = true; }

uint8_t settings_get_timezone(void)          { return settings_general.timezone; }
void settings_set_timezone(uint8_t tz)       { settings_general.timezone = tz; settings_state.modified = true; }

bool settings_get_keypad_tones(void)         { return settings_general.keypadTones; }
void settings_set_keypad_tones(bool enabled) { settings_general.keypadTones = enabled; settings_state.modified = true; }

bool settings_get_vibration(void)            { return settings_general.vibrationEnabled; }
void settings_set_vibration(bool enabled)    { settings_general.vibrationEnabled = enabled; settings_state.modified = true; }

uint16_t settings_get_auto_lock_timeout(void)      { return settings_general.autoLockTimeout; }
void settings_set_auto_lock_timeout(uint16_t sec)  { settings_general.autoLockTimeout = sec; settings_state.modified = true; }

// ============================================================
// SECTION 5 : GETTERS/SETTERS - RÉSEAU
// ============================================================

uint32_t settings_get_frequency(void)            { return settings_network.frequency; }
void settings_set_frequency(uint32_t freq)       { settings_network.frequency = freq; settings_state.modified = true; }

uint8_t settings_get_spreading_factor(void)      { return settings_network.spreadingFactor; }
void settings_set_spreading_factor(uint8_t sf)   { settings_network.spreadingFactor = sf; settings_state.modified = true; }

uint32_t settings_get_bandwidth(void)            { return settings_network.bandwidth; }
void settings_set_bandwidth(uint32_t bw)         { settings_network.bandwidth = bw; settings_state.modified = true; }

uint8_t settings_get_coding_rate(void)           { return settings_network.codingRate; }
void settings_set_coding_rate(uint8_t cr)        { settings_network.codingRate = cr; settings_state.modified = true; }

int8_t settings_get_tx_power(void)               { return settings_network.txPower; }
void settings_set_tx_power(int8_t power)         { settings_network.txPower = power; settings_state.modified = true; }

bool settings_get_discovery_enabled(void)        { return settings_network.discoveryEnabled; }
void settings_set_discovery_enabled(bool en)     { settings_network.discoveryEnabled = en; settings_state.modified = true; }

// ============================================================
// SECTION 6 : GETTERS/SETTERS - AUDIO
// ============================================================

uint8_t settings_get_speaker_volume(void)        { return settings_audio.speakerVolume; }
void settings_set_speaker_volume(uint8_t vol)    { settings_audio.speakerVolume = vol; settings_state.modified = true; }

uint8_t settings_get_mic_gain(void)              { return settings_audio.micGain; }
void settings_set_mic_gain(uint8_t gain)         { settings_audio.micGain = gain; settings_state.modified = true; }

uint8_t settings_get_ringtone_volume(void)       { return settings_audio.ringtoneVolume; }
void settings_set_ringtone_volume(uint8_t vol)   { settings_audio.ringtoneVolume = vol; settings_state.modified = true; }

bool settings_get_mute(void)                     { return settings_audio.muteEnabled; }
void settings_set_mute(bool mute)                { settings_audio.muteEnabled = mute; settings_state.modified = true; }

uint8_t settings_get_ringtone_index(void)        { return settings_audio.ringtoneIndex; }
void settings_set_ringtone_index(uint8_t idx)    { settings_audio.ringtoneIndex = idx; settings_state.modified = true; }

bool settings_get_adpcm_enabled(void)            { return settings_audio.adpcmEnabled; }
void settings_set_adpcm_enabled(bool en)         { settings_audio.adpcmEnabled = en; settings_state.modified = true; }

// ============================================================
// SECTION 7 : GETTERS/SETTERS - AFFICHAGE
// ============================================================

uint8_t settings_get_brightness(void)            { return settings_display.brightness; }
void settings_set_brightness(uint8_t bright)     { settings_display.brightness = bright; settings_state.modified = true; }

uint16_t settings_get_screen_timeout(void)       { return settings_display.screenTimeoutS; }
void settings_set_screen_timeout(uint16_t sec)   { settings_display.screenTimeoutS = sec; settings_state.modified = true; }

bool settings_get_night_mode(void)               { return settings_display.nightModeEnabled; }
void settings_set_night_mode(bool en)            { settings_display.nightModeEnabled = en; settings_state.modified = true; }

uint8_t settings_get_theme(void)                 { return settings_display.themeIndex; }
void settings_set_theme(uint8_t theme)           { settings_display.themeIndex = theme; settings_state.modified = true; }

// ============================================================
// SECTION 8 : GETTERS/SETTERS - DISPOSITIF
// ============================================================

const char* settings_get_device_name(void)
{
    return settings_device.deviceName;
}

void settings_set_device_name(const char* name)
{
    if (name != NULL)
    {
        strncpy(settings_device.deviceName, name, SETTINGS_DEVICE_NAME_MAX - 1);
        settings_device.deviceName[SETTINGS_DEVICE_NAME_MAX - 1] = '\0';
        settings_state.modified = true;
    }
}

const char* settings_get_phone_number(void)
{
    return settings_device.phoneNumber;
}

void settings_set_phone_number(const char* number)
{
    if (number != NULL)
    {
        strncpy(settings_device.phoneNumber, number, SETTINGS_PHONE_NUMBER_MAX - 1);
        settings_device.phoneNumber[SETTINGS_PHONE_NUMBER_MAX - 1] = '\0';
        settings_state.modified = true;
    }
}

bool settings_is_first_boot(void)
{
    return settings_device.firstBoot;
}

void settings_set_first_boot_done(void)
{
    settings_device.firstBoot = false;
    settings_state.modified = true;
}

// ============================================================
// SECTION 9 : GETTERS/SETTERS - SÉCURITÉ
// ============================================================

const char* settings_get_pin(void)
{
    return settings_security.pinCode;
}

void settings_set_pin(const char* pin)
{
    if (pin != NULL)
    {
        strncpy(settings_security.pinCode, pin, SETTINGS_PIN_MAX - 1);
        settings_security.pinCode[SETTINGS_PIN_MAX - 1] = '\0';
        settings_state.modified = true;
    }
}

bool settings_get_pin_enabled(void)              { return settings_security.pinEnabled; }
void settings_set_pin_enabled(bool en)           { settings_security.pinEnabled = en; settings_state.modified = true; }

uint8_t settings_get_max_pin_attempts(void)      { return settings_security.maxPinAttempts; }

// ============================================================
// SECTION 10 : GETTERS/SETTERS - ALIMENTATION
// ============================================================

uint16_t settings_get_sleep_timeout(void)        { return settings_power.sleepTimeoutS; }
void settings_set_sleep_timeout(uint16_t sec)    { settings_power.sleepTimeoutS = sec; settings_state.modified = true; }

bool settings_get_auto_sleep(void)               { return settings_power.autoSleepEnabled; }
void settings_set_auto_sleep(bool en)            { settings_power.autoSleepEnabled = en; settings_state.modified = true; }

bool settings_get_power_saving(void)             { return settings_power.powerSavingMode; }
void settings_set_power_saving(bool en)          { settings_power.powerSavingMode = en; settings_state.modified = true; }

// ============================================================
// SECTION 11 : DÉBOGAGE
// ============================================================

void settings_nvram_print_all(void)
{
    settings_nvram_print_general();
    settings_nvram_print_network();
    settings_nvram_print_audio();
    settings_nvram_print_display();
    settings_nvram_print_device();
}

void settings_nvram_print_general(void)
{
    printf("\n═══ PARAMÈTRES GÉNÉRAUX ═══\n");
    printf("Langue        : %d\n", settings_general.language);
    printf("Fuseau horaire: UTC%+d\n", settings_general.timezone);
    printf("Tonalités     : %s\n", settings_general.keypadTones ? "ON" : "OFF");
    printf("Vibreur       : %s\n", settings_general.vibrationEnabled ? "ON" : "OFF");
    printf("Verrouillage  : %d s\n", settings_general.autoLockTimeout);
    printf("══════════════════════════\n\n");
}

void settings_nvram_print_network(void)
{
    printf("\n═══ PARAMÈTRES RÉSEAU ═══\n");
    printf("Fréquence     : %lu Hz\n", (unsigned long)settings_network.frequency);
    printf("SF / BW / CR  : %d / %lu Hz / 4/%d\n", 
           settings_network.spreadingFactor,
           (unsigned long)settings_network.bandwidth,
           settings_network.codingRate);
    printf("Puissance TX  : %d dBm\n", settings_network.txPower);
    printf("Découverte    : %s\n", settings_network.discoveryEnabled ? "ON" : "OFF");
    printf("══════════════════════════\n\n");
}

void settings_nvram_print_audio(void)
{
    printf("\n═══ PARAMÈTRES AUDIO ═══\n");
    printf("Volume HP     : %d%%\n", settings_audio.speakerVolume);
    printf("Gain Micro    : %d%%\n", settings_audio.micGain);
    printf("Volume Sonnerie: %d%%\n", settings_audio.ringtoneVolume);
    printf("Muet          : %s\n", settings_audio.muteEnabled ? "ON" : "OFF");
    printf("ADPCM         : %s\n", settings_audio.adpcmEnabled ? "ON" : "OFF");
    printf("══════════════════════\n\n");
}

void settings_nvram_print_display(void)
{
    printf("\n═══ PARAMÈTRES AFFICHAGE ═══\n");
    printf("Luminosité    : %d/255\n", settings_display.brightness);
    printf("Timeout écran : %d s\n", settings_display.screenTimeoutS);
    printf("Mode nuit     : %s (%dh-%dh)\n", 
           settings_display.nightModeEnabled ? "ON" : "OFF",
           settings_display.nightModeStartHour,
           settings_display.nightModeEndHour);
    printf("Thème         : %d\n", settings_display.themeIndex);
    printf("══════════════════════════\n\n");
}

void settings_nvram_print_device(void)
{
    printf("\n═══ INFORMATIONS DISPOSITIF ═══\n");
    printf("Nom           : %s\n", settings_device.deviceName);
    printf("Numéro        : %s\n", settings_device.phoneNumber);
    printf("Premier démarrage: %s\n", settings_device.firstBoot ? "Oui" : "Non");
    printf("══════════════════════════════\n\n");
}

bool settings_nvram_self_test(void)
{
    SETTINGS_DEBUG("Auto-test...\n");
    
    if (!settings_state.initialized)
    {
        SETTINGS_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test écriture/lecture
    uint8_t originalVolume = settings_get_speaker_volume();
    settings_set_speaker_volume(50);
    settings_nvram_save_audio();
    
    // Recharger
    settings_nvram_load_all();
    uint8_t loadedVolume = settings_get_speaker_volume();
    
    if (loadedVolume != 50)
    {
        SETTINGS_DEBUG("Échec test écriture/lecture\n");
        return false;
    }
    
    // Restaurer
    settings_set_speaker_volume(originalVolume);
    settings_nvram_save_audio();
    
    SETTINGS_DEBUG("Auto-test OK\n");
    return true;
}