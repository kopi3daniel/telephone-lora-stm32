/**
 * @file settings_service.cpp
 * @brief Implémentation du service de gestion des paramètres
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans settings_service.h.
 * 
 * Il gère :
 * - La lecture/écriture de tous les paramètres
 * - Les profils de configuration
 * - La sauvegarde/restauration
 * - La réinitialisation usine
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "settings_service.h"
#include "../drivers/storage/settings_nvram.h"
#include "../drivers/audio/audio_manager.h"
#include "../drivers/display/display_manager.h"
#include "../drivers/power/power_manager.h"
#include "../drivers/power/backlight_control.h"
#include "../drivers/keypad/keypad_manager.h"
#include "../protocols/identity.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du service */
static SettingsServiceState settings_svc_state;

/** @brief Callbacks */
static SettingsService_ChangedCallback changed_cb = NULL;
static SettingsService_ProfileChangedCallback profile_cb = NULL;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le service de paramètres
 */
bool settings_service_init(void)
{
    SETTINGS_SVC_DEBUG("Initialisation du service de paramètres...\n");
    
    memset(&settings_svc_state, 0, sizeof(SettingsServiceState));
    
    // Initialiser le module NVRAM
    if (!settings_nvram_init())
    {
        SETTINGS_SVC_DEBUG("Échec initialisation NVRAM\n");
        return false;
    }
    
    // Charger les paramètres
    settings_service_load();
    
    settings_svc_state.activeProfile = SETTINGS_PROFILE_NORMAL;
    settings_svc_state.initialized = true;
    
    SETTINGS_SVC_DEBUG("Service initialisé\n");
    return true;
}

void settings_service_deinit(void)
{
    if (settings_svc_state.modified)
    {
        settings_service_save();
    }
    settings_svc_state.initialized = false;
}

bool settings_service_is_ready(void)
{
    return settings_svc_state.initialized;
}

// ============================================================
// SECTION 2 : CHARGEMENT/SAUVEGARDE
// ============================================================

bool settings_service_load(void)
{
    bool result = settings_nvram_load_all();
    
    if (result)
    {
        // Appliquer les paramètres chargés aux drivers
        apply_all_settings_to_hardware();
    }
    
    return result;
}

bool settings_service_save(void)
{
    if (!settings_svc_state.modified) return true;
    
    bool result = settings_nvram_save_all();
    
    if (result)
    {
        settings_svc_state.modified = false;
        settings_svc_state.lastSave = HAL_GetTick();
        settings_svc_state.saveCount++;
        SETTINGS_SVC_DEBUG("Paramètres sauvegardés\n");
    }
    
    return result;
}

bool settings_service_save_category(SettingsCategory category)
{
    bool result = false;
    
    switch (category)
    {
        case SETTINGS_CAT_GENERAL:  result = settings_nvram_save_general(); break;
        case SETTINGS_CAT_NETWORK:  result = settings_nvram_save_network(); break;
        case SETTINGS_CAT_AUDIO:    result = settings_nvram_save_audio(); break;
        case SETTINGS_CAT_DISPLAY:  result = settings_nvram_save_display(); break;
        case SETTINGS_CAT_DEVICE:   result = settings_nvram_save_device(); break;
        case SETTINGS_CAT_SECURITY: result = settings_nvram_save_security(); break;
        case SETTINGS_CAT_POWER:    result = settings_nvram_save_power(); break;
        default: break;
    }
    
    return result;
}

void settings_service_factory_reset(void)
{
    settings_nvram_factory_reset();
    apply_all_settings_to_hardware();
    settings_svc_state.modified = false;
    
    SETTINGS_SVC_DEBUG("Réinitialisation usine effectuée\n");
}

void settings_service_factory_reset_category(SettingsCategory category)
{
    switch (category)
    {
        case SETTINGS_CAT_AUDIO:
            settings_set_speaker_volume(80);
            settings_set_mic_gain(100);
            settings_set_ringtone_volume(80);
            settings_set_ringtone(0);
            settings_set_mute(false);
            break;
        case SETTINGS_CAT_DISPLAY:
            settings_set_brightness(200);
            settings_set_screen_timeout(30);
            settings_set_night_mode(true);
            settings_set_rotation(0);
            settings_set_theme(0);
            break;
        default:
            break;
    }
    
    settings_service_save_category(category);
}

// ============================================================
// SECTION 3 : PROFILS
// ============================================================

bool settings_service_apply_profile(SettingsProfile profile)
{
    SETTINGS_SVC_DEBUG("Application du profil %d\n", profile);
    
    switch (profile)
    {
        case SETTINGS_PROFILE_NORMAL:
            settings_set_speaker_volume(80);
            settings_set_brightness(200);
            settings_set_screen_timeout(30);
            settings_set_sleep_timeout(30);
            settings_set_tx_power(17);
            settings_set_spreading_factor(7);
            break;
            
        case SETTINGS_PROFILE_POWER_SAVE:
            settings_set_speaker_volume(50);
            settings_set_brightness(80);
            settings_set_screen_timeout(10);
            settings_set_sleep_timeout(15);
            settings_set_tx_power(10);
            settings_set_power_saving(true);
            break;
            
        case SETTINGS_PROFILE_OUTDOOR:
            settings_set_speaker_volume(100);
            settings_set_brightness(255);
            settings_set_screen_timeout(60);
            settings_set_tx_power(20);
            settings_set_spreading_factor(10);
            settings_set_power_saving(false);
            break;
            
        case SETTINGS_PROFILE_INDOOR:
            settings_set_speaker_volume(60);
            settings_set_brightness(150);
            settings_set_screen_timeout(30);
            settings_set_tx_power(5);
            settings_set_spreading_factor(7);
            settings_set_power_saving(false);
            break;
            
        case SETTINGS_PROFILE_CUSTOM:
            // Le profil custom est défini par l'utilisateur
            break;
            
        default:
            return false;
    }
    
    SettingsProfile oldProfile = settings_svc_state.activeProfile;
    settings_svc_state.activeProfile = profile;
    settings_service_save();
    
    if (profile_cb) profile_cb(oldProfile, profile);
    
    return true;
}

SettingsProfile settings_service_get_profile(void)
{
    return settings_svc_state.activeProfile;
}

bool settings_service_save_current_as_profile(SettingsProfile profile)
{
    // Sauvegarder les paramètres actuels comme profil custom
    return settings_service_save();
}

bool settings_service_reset_profile(SettingsProfile profile)
{
    return settings_service_apply_profile(profile);
}

// ============================================================
// SECTION 4 : GETTERS/SETTERS - GÉNÉRAL
// ============================================================

uint8_t settings_get_language(void) { return settings_nvram_get_language(); }
void settings_set_language(uint8_t lang) { 
    settings_nvram_set_language(lang); 
    settings_svc_state.modified = true;
    if (changed_cb) changed_cb(SETTINGS_CAT_GENERAL);
}

uint8_t settings_get_timezone(void) { return settings_nvram_get_timezone(); }
void settings_set_timezone(uint8_t tz) { 
    settings_nvram_set_timezone(tz); 
    settings_svc_state.modified = true;
}

bool settings_get_keypad_tones(void) { return settings_nvram_get_keypad_tones(); }
void settings_set_keypad_tones(bool enable) { 
    settings_nvram_set_keypad_tones(enable); 
    settings_svc_state.modified = true;
}

bool settings_get_vibration(void) { return settings_nvram_get_vibration(); }
void settings_set_vibration(bool enable) { 
    settings_nvram_set_vibration(enable); 
    settings_svc_state.modified = true;
}

uint16_t settings_get_auto_lock_timeout(void) { return settings_nvram_get_auto_lock_timeout(); }
void settings_set_auto_lock_timeout(uint16_t sec) { 
    settings_nvram_set_auto_lock_timeout(sec); 
    settings_svc_state.modified = true;
}

// ============================================================
// SECTION 5 : GETTERS/SETTERS - RÉSEAU
// ============================================================

uint32_t settings_get_frequency(void) { return settings_nvram_get_frequency(); }
void settings_set_frequency(uint32_t hz) { 
    settings_nvram_set_frequency(hz); 
    lora_driver_set_frequency(hz);
    settings_svc_state.modified = true;
    if (changed_cb) changed_cb(SETTINGS_CAT_NETWORK);
}

uint8_t settings_get_spreading_factor(void) { return settings_nvram_get_spreading_factor(); }
void settings_set_spreading_factor(uint8_t sf) { 
    settings_nvram_set_spreading_factor(sf); 
    lora_driver_set_spreading_factor(sf);
    settings_svc_state.modified = true;
}

uint32_t settings_get_bandwidth(void) { return settings_nvram_get_bandwidth(); }
void settings_set_bandwidth(uint32_t bw) { 
    settings_nvram_set_bandwidth(bw); 
    lora_driver_set_bandwidth(bw);
    settings_svc_state.modified = true;
}

uint8_t settings_get_coding_rate(void) { return settings_nvram_get_coding_rate(); }
void settings_set_coding_rate(uint8_t cr) { 
    settings_nvram_set_coding_rate(cr); 
    lora_driver_set_coding_rate(cr);
    settings_svc_state.modified = true;
}

int8_t settings_get_tx_power(void) { return settings_nvram_get_tx_power(); }
void settings_set_tx_power(int8_t dbm) { 
    settings_nvram_set_tx_power(dbm); 
    lora_driver_set_tx_power(dbm);
    settings_svc_state.modified = true;
}

bool settings_get_discovery(void) { return settings_nvram_get_discovery_enabled(); }
void settings_set_discovery(bool enable) { 
    settings_nvram_set_discovery_enabled(enable); 
    settings_svc_state.modified = true;
}

// ============================================================
// SECTION 6 : GETTERS/SETTERS - AUDIO
// ============================================================

uint8_t settings_get_speaker_volume(void) { return settings_nvram_get_speaker_volume(); }
void settings_set_speaker_volume(uint8_t vol) { 
    settings_nvram_set_speaker_volume(vol); 
    audio_manager_set_volume(vol);
    settings_svc_state.modified = true;
    if (changed_cb) changed_cb(SETTINGS_CAT_AUDIO);
}

uint8_t settings_get_mic_gain(void) { return settings_nvram_get_mic_gain(); }
void settings_set_mic_gain(uint8_t gain) { 
    settings_nvram_set_mic_gain(gain); 
    audio_manager_set_mic_gain(gain);
    settings_svc_state.modified = true;
}

uint8_t settings_get_ringtone_volume(void) { return settings_nvram_get_ringtone_volume(); }
void settings_set_ringtone_volume(uint8_t vol) { 
    settings_nvram_set_ringtone_volume(vol); 
    settings_svc_state.modified = true;
}

uint8_t settings_get_ringtone(void) { return settings_nvram_get_ringtone_index(); }
void settings_set_ringtone(uint8_t idx) { 
    settings_nvram_set_ringtone_index(idx); 
    settings_svc_state.modified = true;
}

bool settings_get_mute(void) { return settings_nvram_get_mute(); }
void settings_set_mute(bool mute) { 
    settings_nvram_set_mute(mute); 
    audio_manager_set_mute(mute);
    settings_svc_state.modified = true;
}

bool settings_get_adpcm(void) { return settings_nvram_get_adpcm_enabled(); }
void settings_set_adpcm(bool enable) { 
    settings_nvram_set_adpcm_enabled(enable); 
    settings_svc_state.modified = true;
}

// ============================================================
// SECTION 7 : GETTERS/SETTERS - AFFICHAGE
// ============================================================

uint8_t settings_get_brightness(void) { return settings_nvram_get_brightness(); }
void settings_set_brightness(uint8_t bright) { 
    settings_nvram_set_brightness(bright); 
    display_set_brightness(bright);
    settings_svc_state.modified = true;
    if (changed_cb) changed_cb(SETTINGS_CAT_DISPLAY);
}

uint16_t settings_get_screen_timeout(void) { return settings_nvram_get_screen_timeout(); }
void settings_set_screen_timeout(uint16_t sec) { 
    settings_nvram_set_screen_timeout(sec); 
    settings_svc_state.modified = true;
}

bool settings_get_night_mode(void) { return settings_nvram_get_night_mode(); }
void settings_set_night_mode(bool enable) { 
    settings_nvram_set_night_mode(enable); 
    settings_svc_state.modified = true;
}

uint8_t settings_get_rotation(void) { return settings_nvram_get_rotation(); }
void settings_set_rotation(uint8_t rot) { 
    settings_nvram_set_rotation(rot); 
    display_set_rotation((ILI9488_Rotation)rot);
    settings_svc_state.modified = true;
}

uint8_t settings_get_theme(void) { return settings_nvram_get_theme(); }
void settings_set_theme(uint8_t theme) { 
    settings_nvram_set_theme(theme); 
    settings_svc_state.modified = true;
}

// ============================================================
// SECTION 8 : GETTERS/SETTERS - DISPOSITIF
// ============================================================

const char* settings_get_device_name(void) { return settings_nvram_get_device_name(); }
void settings_set_device_name(const char* name) { 
    settings_nvram_set_device_name(name); 
    identity_set_device_name(name);
    settings_svc_state.modified = true;
    if (changed_cb) changed_cb(SETTINGS_CAT_DEVICE);
}

const char* settings_get_phone_number(void) { return settings_nvram_get_phone_number(); }
void settings_set_phone_number(const char* num) { 
    settings_nvram_set_phone_number(num); 
    identity_set_msisdn(num);
    settings_svc_state.modified = true;
}

bool settings_is_first_boot(void) { return settings_nvram_is_first_boot(); }
void settings_set_first_boot_done(void) { 
    settings_nvram_set_first_boot_done(); 
    settings_svc_state.modified = true;
}

// ============================================================
// SECTION 9 : GETTERS/SETTERS - SÉCURITÉ
// ============================================================

bool settings_get_pin_enabled(void) { return settings_nvram_get_pin_enabled(); }
void settings_set_pin_enabled(bool enable) { 
    settings_nvram_set_pin_enabled(enable); 
    settings_svc_state.modified = true;
    if (changed_cb) changed_cb(SETTINGS_CAT_SECURITY);
}

bool settings_verify_pin(const char* pin)
{
    if (pin == NULL) return false;
    const char* storedPin = settings_nvram_get_pin();
    return (strcmp(pin, storedPin) == 0);
}

bool settings_change_pin(const char* oldPin, const char* newPin)
{
    if (!settings_verify_pin(oldPin)) return false;
    if (newPin == NULL || strlen(newPin) < 4) return false;
    
    settings_nvram_set_pin(newPin);
    settings_svc_state.modified = true;
    return true;
}

bool settings_get_lock_on_startup(void) { return settings_nvram_get_lock_on_startup(); }
void settings_set_lock_on_startup(bool enable) { 
    settings_nvram_set_lock_on_startup(enable); 
    settings_svc_state.modified = true;
}

// ============================================================
// SECTION 10 : GETTERS/SETTERS - ALIMENTATION
// ============================================================

uint16_t settings_get_sleep_timeout(void) { return settings_nvram_get_sleep_timeout(); }
void settings_set_sleep_timeout(uint16_t sec) { 
    settings_nvram_set_sleep_timeout(sec); 
    power_manager_set_sleep_timeout(sec);
    settings_svc_state.modified = true;
    if (changed_cb) changed_cb(SETTINGS_CAT_POWER);
}

bool settings_get_auto_sleep(void) { return settings_nvram_get_auto_sleep(); }
void settings_set_auto_sleep(bool enable) { 
    settings_nvram_set_auto_sleep(enable); 
    settings_svc_state.modified = true;
}

bool settings_get_power_saving(void) { return settings_nvram_get_power_saving(); }
void settings_set_power_saving(bool enable) { 
    settings_nvram_set_power_saving(enable); 
    settings_svc_state.modified = true;
}

// ============================================================
// SECTION 11 : GETTERS/SETTERS - CLAVIER
// ============================================================

uint8_t settings_get_keypad_brightness(void)
{
    return backlight_get_brightness(BACKLIGHT_TARGET_KEYPAD);
}

void settings_set_keypad_brightness(uint8_t brightness)
{
    backlight_set_brightness(BACKLIGHT_TARGET_KEYPAD, brightness);
    settings_svc_state.modified = true;
}

uint16_t settings_get_keypad_timeout(void)
{
    return backlight_get_keypad_timeout();
}

void settings_set_keypad_timeout(uint16_t seconds)
{
    backlight_set_keypad_timeout(seconds);
    settings_svc_state.modified = true;
}

// ============================================================
// SECTION 12 : APPLICATION AU HARDWARE
// ============================================================

/**
 * @brief Applique tous les paramètres chargés aux drivers matériels
 */
static void apply_all_settings_to_hardware(void)
{
    // Audio
    audio_manager_set_volume(settings_get_speaker_volume());
    audio_manager_set_mute(settings_get_mute());
    audio_manager_set_mic_gain(settings_get_mic_gain());
    
    // Affichage
    display_set_brightness(settings_get_brightness());
    display_set_rotation((ILI9488_Rotation)settings_get_rotation());
    
    // Réseau
    lora_driver_set_frequency(settings_get_frequency());
    lora_driver_set_spreading_factor(settings_get_spreading_factor());
    lora_driver_set_bandwidth(settings_get_bandwidth());
    lora_driver_set_coding_rate(settings_get_coding_rate());
    lora_driver_set_tx_power(settings_get_tx_power());
    
    // Dispositif
    identity_set_msisdn(settings_get_phone_number());
    identity_set_device_name(settings_get_device_name());
    
    // Alimentation
    power_manager_set_sleep_timeout(settings_get_sleep_timeout());
    
    SETTINGS_SVC_DEBUG("Paramètres appliqués au matériel\n");
}

// ============================================================
// SECTION 13 : CALLBACKS
// ============================================================

void settings_service_set_changed_callback(SettingsService_ChangedCallback cb) { changed_cb = cb; }
void settings_service_set_profile_changed_callback(SettingsService_ProfileChangedCallback cb) { profile_cb = cb; }

// ============================================================
// SECTION 14 : DÉBOGAGE
// ============================================================

void settings_service_print_all(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║         TOUS LES PARAMÈTRES                   ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    
    // Général
    printf("║ [GÉNÉRAL]                                    ║\n");
    printf("║  Langue        : %-27d ║\n", settings_get_language());
    printf("║  Fuseau horaire: UTC%+-24d ║\n", settings_get_timezone());
    printf("║  Tonalités clavier: %-21s ║\n", settings_get_keypad_tones() ? "ON" : "OFF");
    printf("║  Vibreur       : %-27s ║\n", settings_get_vibration() ? "ON" : "OFF");
    printf("║  Verrouillage auto: %-20d s ║\n", settings_get_auto_lock_timeout());
    
    // Réseau
    printf("║ [RÉSEAU]                                     ║\n");
    printf("║  Fréquence     : %lu Hz                        ║\n", (unsigned long)settings_get_frequency());
    printf("║  SF / BW / CR  : %d / %lu Hz / 4/%d           ║\n", 
           settings_get_spreading_factor(),
           (unsigned long)settings_get_bandwidth(),
           settings_get_coding_rate());
    printf("║  Puissance TX  : %d dBm                       ║\n", settings_get_tx_power());
    printf("║  Découverte    : %-27s ║\n", settings_get_discovery() ? "ON" : "OFF");
    
    // Audio
    printf("║ [AUDIO]                                      ║\n");
    printf("║  Volume HP     : %-27d ║\n", settings_get_speaker_volume());
    printf("║  Gain Micro    : %-27d ║\n", settings_get_mic_gain());
    printf("║  Volume sonnerie: %-24d ║\n", settings_get_ringtone_volume());
    printf("║  Sonnerie      : %-27d ║\n", settings_get_ringtone());
    printf("║  Muet          : %-27s ║\n", settings_get_mute() ? "ON" : "OFF");
    
    // Affichage
    printf("║ [AFFICHAGE]                                  ║\n");
    printf("║  Luminosité    : %-27d ║\n", settings_get_brightness());
    printf("║  Timeout écran : %-24d s ║\n", settings_get_screen_timeout());
    printf("║  Mode nuit     : %-27s ║\n", settings_get_night_mode() ? "ON" : "OFF");
    printf("║  Rotation      : %-27d ║\n", settings_get_rotation());
    printf("║  Thème         : %-27d ║\n", settings_get_theme());
    
    // Dispositif
    printf("║ [DISPOSITIF]                                 ║\n");
    printf("║  Nom           : %-27s ║\n", settings_get_device_name());
    printf("║  Numéro        : %-27s ║\n", settings_get_phone_number());
    printf("║  Premier démarrage: %-21s ║\n", settings_is_first_boot() ? "Oui" : "Non");
    
    // Sécurité
    printf("║ [SÉCURITÉ]                                   ║\n");
    printf("║  PIN activé    : %-27s ║\n", settings_get_pin_enabled() ? "Oui" : "Non");
    printf("║  Verrouillage au démarrage: %-13s ║\n", settings_get_lock_on_startup() ? "Oui" : "Non");
    
    // Alimentation
    printf("║ [ALIMENTATION]                               ║\n");
    printf("║  Timeout veille: %-24d s ║\n", settings_get_sleep_timeout());
    printf("║  Veille auto   : %-27s ║\n", settings_get_auto_sleep() ? "ON" : "OFF");
    printf("║  Économie énergie: %-22s ║\n", settings_get_power_saving() ? "ON" : "OFF");
    
    printf("╚══════════════════════════════════════════════╝\n\n");
}

void settings_service_print_category(SettingsCategory category)
{
    switch (category)
    {
        case SETTINGS_CAT_AUDIO:
            printf("\n═══ PARAMÈTRES AUDIO ═══\n");
            printf("Volume HP      : %d%%\n", settings_get_speaker_volume());
            printf("Gain Micro     : %d%%\n", settings_get_mic_gain());
            printf("Volume sonnerie: %d%%\n", settings_get_ringtone_volume());
            printf("Sonnerie       : %d\n", settings_get_ringtone());
            printf("Muet           : %s\n", settings_get_mute() ? "ON" : "OFF");
            printf("══════════════════════\n\n");
            break;
            
        case SETTINGS_CAT_DISPLAY:
            printf("\n═══ PARAMÈTRES AFFICHAGE ═══\n");
            printf("Luminosité     : %d/255\n", settings_get_brightness());
            printf("Timeout écran  : %d s\n", settings_get_screen_timeout());
            printf("Mode nuit      : %s\n", settings_get_night_mode() ? "ON" : "OFF");
            printf("Rotation       : %d\n", settings_get_rotation());
            printf("Thème          : %d\n", settings_get_theme());
            printf("══════════════════════\n\n");
            break;
            
        default:
            printf("[SETTINGS] Catégorie %d\n", category);
            break;
    }
}

void settings_service_print_state(void)
{
    printf("\n═══ ÉTAT SERVICE PARAMÈTRES ═══\n");
    printf("Initialisé : %s\n", settings_svc_state.initialized ? "Oui" : "Non");
    printf("Modifié    : %s\n", settings_svc_state.modified ? "Oui" : "Non");
    printf("Verrouillé : %s\n", settings_svc_state.locked ? "Oui" : "Non");
    printf("Profil     : %d\n", settings_svc_state.activeProfile);
    printf("Sauvegardes: %lu\n", (unsigned long)settings_svc_state.saveCount);
    printf("════════════════════════════\n\n");
}

bool settings_service_self_test(void)
{
    SETTINGS_SVC_DEBUG("Auto-test...\n");
    
    if (!settings_svc_state.initialized)
    {
        SETTINGS_SVC_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : lecture/écriture volume
    uint8_t originalVolume = settings_get_speaker_volume();
    settings_set_speaker_volume(50);
    
    if (settings_get_speaker_volume() != 50)
    {
        SETTINGS_SVC_DEBUG("Échec : lecture/écriture volume\n");
        return false;
    }
    
    // Restaurer
    settings_set_speaker_volume(originalVolume);
    
    SETTINGS_SVC_DEBUG("Auto-test OK\n");
    return true;
}