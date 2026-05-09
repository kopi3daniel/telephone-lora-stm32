/**
 * @file settings_service.h
 * @brief Service de gestion des paramètres de configuration
 * 
 * Ce fichier implémente le service central de gestion de TOUS
 * les paramètres de configuration du téléphone LoRa.
 * 
 * Il unifie les modules de paramètres (settings_nvram) et fournit
 * une API de haut niveau pour l'interface utilisateur :
 * - Lecture/écriture des paramètres par catégorie
 * - Restauration des valeurs d'usine
 * - Export/Import de la configuration
 * - Profils de configuration (normal, économie, extérieur, etc.)
 * - Verrouillage des paramètres critiques
 * 
 * Catégories de paramètres :
 * - Général (langue, date/heure, tonalités)
 * - Réseau (fréquence, puissance, SF, BW)
 * - Audio (volume, sonnerie, micro)
 * - Affichage (luminosité, timeout, thème, rotation)
 * - Dispositif (nom, numéro)
 * - Sécurité (PIN, verrouillage)
 * - Alimentation (timeouts veille, économie)
 * - Clavier (rétroéclairage, tonalités)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SETTINGS_SERVICE_H
#define SETTINGS_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "../drivers/storage/settings_nvram.h"
#include "../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du service */
#define SETTINGS_SERVICE_VERSION        "1.0.0"

/** @brief Nombre maximum de profils */
#define SETTINGS_MAX_PROFILES           5

/** @brief Nom du profil par défaut */
#define SETTINGS_DEFAULT_PROFILE_NAME   "Standard"

// ============================================================
// SECTION 2 : PROFILS DE CONFIGURATION
// ============================================================

/**
 * @brief Profils de configuration prédéfinis
 */
typedef enum {
    SETTINGS_PROFILE_NORMAL     = 0,    // Usage normal
    SETTINGS_PROFILE_POWER_SAVE = 1,    // Économie d'énergie
    SETTINGS_PROFILE_OUTDOOR    = 2,    // Extérieur (longue portée)
    SETTINGS_PROFILE_INDOOR     = 3,    // Intérieur (basse puissance)
    SETTINGS_PROFILE_CUSTOM     = 4     // Personnalisé
} SettingsProfile;

/**
 * @brief Catégories de paramètres
 */
typedef enum {
    SETTINGS_CAT_GENERAL    = 0,    // Paramètres généraux
    SETTINGS_CAT_NETWORK    = 1,    // Réseau LoRa
    SETTINGS_CAT_AUDIO      = 2,    // Audio
    SETTINGS_CAT_DISPLAY    = 3,    // Affichage
    SETTINGS_CAT_DEVICE     = 4,    // Dispositif
    SETTINGS_CAT_SECURITY   = 5,    // Sécurité
    SETTINGS_CAT_POWER      = 6,    // Alimentation
    SETTINGS_CAT_KEYPAD     = 7,    // Clavier
    SETTINGS_CAT_ALL        = 0xFF  // Toutes les catégories
} SettingsCategory;

// ============================================================
// SECTION 3 : ÉTAT DU SERVICE
// ============================================================

/**
 * @brief État du service de paramètres
 */
typedef struct {
    bool initialized;                   // Service initialisé
    bool modified;                      // Paramètres modifiés
    bool locked;                        // Paramètres verrouillés
    SettingsProfile activeProfile;      // Profil actif
    uint32_t lastSave;                  // Dernière sauvegarde
    uint32_t saveCount;                 // Nombre de sauvegardes
} SettingsServiceState;

// ============================================================
// SECTION 4 : CALLBACKS
// ============================================================

typedef void (*SettingsService_ChangedCallback)(SettingsCategory category);
typedef void (*SettingsService_ProfileChangedCallback)(SettingsProfile oldProfile, 
                                                        SettingsProfile newProfile);

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

bool settings_service_init(void);
void settings_service_deinit(void);
bool settings_service_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS DE CHARGEMENT/SAUVEGARDE
// ============================================================

bool settings_service_load(void);
bool settings_service_save(void);
bool settings_service_save_category(SettingsCategory category);
void settings_service_factory_reset(void);
void settings_service_factory_reset_category(SettingsCategory category);

// ============================================================
// SECTION 7 : FONCTIONS DE PROFILS
// ============================================================

bool settings_service_apply_profile(SettingsProfile profile);
SettingsProfile settings_service_get_profile(void);
bool settings_service_save_current_as_profile(SettingsProfile profile);
bool settings_service_reset_profile(SettingsProfile profile);

// ============================================================
// SECTION 8 : GETTERS/SETTERS - GÉNÉRAL
// ============================================================

uint8_t settings_get_language(void);
void settings_set_language(uint8_t language);
uint8_t settings_get_timezone(void);
void settings_set_timezone(uint8_t timezone);
bool settings_get_keypad_tones(void);
void settings_set_keypad_tones(bool enable);
bool settings_get_vibration(void);
void settings_set_vibration(bool enable);
uint16_t settings_get_auto_lock_timeout(void);
void settings_set_auto_lock_timeout(uint16_t seconds);

// ============================================================
// SECTION 9 : GETTERS/SETTERS - RÉSEAU
// ============================================================

uint32_t settings_get_frequency(void);
void settings_set_frequency(uint32_t hz);
uint8_t settings_get_spreading_factor(void);
void settings_set_spreading_factor(uint8_t sf);
uint32_t settings_get_bandwidth(void);
void settings_set_bandwidth(uint32_t bw);
uint8_t settings_get_coding_rate(void);
void settings_set_coding_rate(uint8_t cr);
int8_t settings_get_tx_power(void);
void settings_set_tx_power(int8_t dbm);
bool settings_get_discovery(void);
void settings_set_discovery(bool enable);

// ============================================================
// SECTION 10 : GETTERS/SETTERS - AUDIO
// ============================================================

uint8_t settings_get_speaker_volume(void);
void settings_set_speaker_volume(uint8_t volume);
uint8_t settings_get_mic_gain(void);
void settings_set_mic_gain(uint8_t gain);
uint8_t settings_get_ringtone_volume(void);
void settings_set_ringtone_volume(uint8_t volume);
uint8_t settings_get_ringtone(void);
void settings_set_ringtone(uint8_t index);
bool settings_get_mute(void);
void settings_set_mute(bool mute);
bool settings_get_adpcm(void);
void settings_set_adpcm(bool enable);

// ============================================================
// SECTION 11 : GETTERS/SETTERS - AFFICHAGE
// ============================================================

uint8_t settings_get_brightness(void);
void settings_set_brightness(uint8_t brightness);
uint16_t settings_get_screen_timeout(void);
void settings_set_screen_timeout(uint16_t seconds);
bool settings_get_night_mode(void);
void settings_set_night_mode(bool enable);
uint8_t settings_get_rotation(void);
void settings_set_rotation(uint8_t rotation);
uint8_t settings_get_theme(void);
void settings_set_theme(uint8_t theme);

// ============================================================
// SECTION 12 : GETTERS/SETTERS - DISPOSITIF
// ============================================================

const char* settings_get_device_name(void);
void settings_set_device_name(const char* name);
const char* settings_get_phone_number(void);
void settings_set_phone_number(const char* number);
bool settings_is_first_boot(void);
void settings_set_first_boot_done(void);

// ============================================================
// SECTION 13 : GETTERS/SETTERS - SÉCURITÉ
// ============================================================

bool settings_get_pin_enabled(void);
void settings_set_pin_enabled(bool enable);
bool settings_verify_pin(const char* pin);
bool settings_change_pin(const char* oldPin, const char* newPin);
bool settings_get_lock_on_startup(void);
void settings_set_lock_on_startup(bool enable);

// ============================================================
// SECTION 14 : GETTERS/SETTERS - ALIMENTATION
// ============================================================

uint16_t settings_get_sleep_timeout(void);
void settings_set_sleep_timeout(uint16_t seconds);
bool settings_get_auto_sleep(void);
void settings_set_auto_sleep(bool enable);
bool settings_get_power_saving(void);
void settings_set_power_saving(bool enable);

// ============================================================
// SECTION 15 : GETTERS/SETTERS - CLAVIER
// ============================================================

uint8_t settings_get_keypad_brightness(void);
void settings_set_keypad_brightness(uint8_t brightness);
uint16_t settings_get_keypad_timeout(void);
void settings_set_keypad_timeout(uint16_t seconds);

// ============================================================
// SECTION 16 : FONCTIONS DE CALLBACKS
// ============================================================

void settings_service_set_changed_callback(SettingsService_ChangedCallback callback);
void settings_service_set_profile_changed_callback(SettingsService_ProfileChangedCallback callback);

// ============================================================
// SECTION 17 : FONCTIONS DE DÉBOGAGE
// ============================================================

void settings_service_print_all(void);
void settings_service_print_category(SettingsCategory category);
void settings_service_print_state(void);
bool settings_service_self_test(void);

// ============================================================
// SECTION 18 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define SETTINGS_SVC_DEBUG(fmt, ...) printf("[SETTINGS_SVC] " fmt, ##__VA_ARGS__)
#else
    #define SETTINGS_SVC_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 19 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SETTINGS_SERVICE_H