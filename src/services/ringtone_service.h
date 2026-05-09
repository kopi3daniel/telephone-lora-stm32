/**
 * @file ringtone_service.h
 * @brief Service de gestion des sonneries et mélodies
 * 
 * Ce fichier implémente le service de sonneries qui gère :
 * - Les sonneries d'appel (ringtones)
 * - Les sons de notification
 * - Les tonalités DTMF
 * - Les mélodies d'alarme
 * - Les sons système (démarrage, arrêt, erreur)
 * 
 * Types de sons :
 * - RINGTONE  : Sonnerie d'appel entrant
 * - NOTIFICATION : Son pour les SMS et notifications
 * - ALARM     : Son d'alarme
 * - DTMF      : Tonalités de numérotation
 * - SYSTEM    : Sons système
 * 
 * Chaque son est défini par :
 * - Une mélodie (suite de notes)
 * - Un volume
 * - Une durée
 * - Une répétition (pour les sonneries)
 * 
 * Format d'une note :
 * ┌──────────┬──────────┬──────────┐
 * │ Fréquence│ Durée    │ Pause    │
 * │ (Hz)     │ (ms)     │ (ms)     │
 * └──────────┴──────────┴──────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef RINGTONE_SERVICE_H
#define RINGTONE_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du service */
#define RINGTONE_VERSION                "1.0.0"

/** @brief Nombre maximum de sonneries */
#define RINGTONE_MAX_COUNT              20

/** @brief Nombre maximum de notes par mélodie */
#define RINGTONE_MAX_NOTES              64

/** @brief Nombre maximum de sons de notification */
#define RINGTONE_MAX_NOTIFICATIONS      10

/** @brief Nombre maximum de sons DTMF personnalisés */
#define RINGTONE_MAX_DTMF               16

/** @brief Durée par défaut d'un bip (ms) */
#define RINGTONE_DEFAULT_BEEP_MS        100

/** @brief Fréquence par défaut d'un bip (Hz) */
#define RINGTONE_DEFAULT_BEEP_HZ        1000

// ============================================================
// SECTION 2 : TYPES DE SONS
// ============================================================

/**
 * @brief Types de sons
 */
typedef enum {
    RINGTONE_TYPE_CALL      = 0,    // Sonnerie d'appel
    RINGTONE_TYPE_SMS       = 1,    // Notification SMS
    RINGTONE_TYPE_ALARM     = 2,    // Alarme
    RINGTONE_TYPE_SYSTEM    = 3,    // Son système
    RINGTONE_TYPE_DTMF      = 4,    // Tonalité DTMF
    RINGTONE_TYPE_CUSTOM    = 5     // Personnalisé
} RingtoneType;

/**
 * @brief Note de musique
 */
typedef struct {
    uint16_t frequency;             // Fréquence en Hz (0 = silence)
    uint16_t durationMs;            // Durée en ms
    uint16_t pauseMs;               // Pause après la note (ms)
} MusicalNote;

/**
 * @brief Définition d'une sonnerie
 */
typedef struct {
    char name[32];                              // Nom de la sonnerie
    RingtoneType type;                          // Type
    MusicalNote notes[RINGTONE_MAX_NOTES];      // Notes de la mélodie
    uint16_t noteCount;                         // Nombre de notes
    uint16_t tempo;                             // Tempo (BPM)
    bool repeat;                                // Répéter ?
    uint8_t repeatCount;                        // Nombre de répétitions (0=infini)
    uint16_t pauseBetweenRepeatsMs;             // Pause entre répétitions
    uint8_t volume;                             // Volume (0-100)
    bool predefined;                            // Prédéfinie (non modifiable)
} Ringtone;

/**
 * @brief Son de notification
 */
typedef struct {
    char name[32];                  // Nom
    uint16_t frequency;             // Fréquence (Hz)
    uint16_t durationMs;            // Durée (ms)
    uint8_t repeatCount;            // Nombre de répétitions
    uint16_t repeatIntervalMs;      // Intervalle entre répétitions
} NotificationSound;

// ============================================================
// SECTION 3 : ÉTAT DU SERVICE
// ============================================================

/**
 * @brief État du service de sonneries
 */
typedef struct {
    bool initialized;                           // Service initialisé
    
    // Sonneries
    Ringtone ringtones[RINGTONE_MAX_COUNT];
    uint8_t ringtoneCount;
    
    // Sonnerie active pour les appels
    uint8_t activeRingtoneIndex;
    
    // Sons de notification
    NotificationSound notifications[RINGTONE_MAX_NOTIFICATIONS];
    uint8_t notificationCount;
    uint8_t activeNotificationIndex;
    
    // Lecture en cours
    bool playing;                               // Son en cours ?
    uint8_t currentRingtoneIndex;               // Sonnerie en cours
    uint16_t currentNoteIndex;                  // Note en cours
    uint32_t noteStartTime;                     // Début de la note
    bool stopped;                               // Arrêt demandé ?
    
    // Volume
    uint8_t ringtoneVolume;                     // Volume sonnerie (0-100)
    uint8_t notificationVolume;                 // Volume notification (0-100)
    
    // Statistiques
    uint32_t totalRingtonePlays;
    uint32_t totalNotificationPlays;
    
} RingtoneServiceState;

// ============================================================
// SECTION 4 : CALLBACKS
// ============================================================

typedef void (*RingtoneService_NoteCallback)(uint16_t frequency, uint16_t durationMs);
typedef void (*RingtoneService_FinishedCallback)(void);

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

bool ringtone_service_init(void);
void ringtone_service_deinit(void);
bool ringtone_service_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS DE LECTURE
// ============================================================

bool ringtone_service_play(uint8_t ringtoneIndex);
bool ringtone_service_play_call_ringtone(void);
bool ringtone_service_play_notification(void);
bool ringtone_service_play_alarm(void);
bool ringtone_service_play_system_sound(const char* soundName);
bool ringtone_service_play_beep(uint16_t frequency, uint16_t durationMs);
bool ringtone_service_play_double_beep(uint16_t freq1, uint16_t freq2, uint16_t durationMs);
bool ringtone_service_play_melody(const MusicalNote* notes, uint16_t noteCount);
void ringtone_service_stop(void);
bool ringtone_service_is_playing(void);

// ============================================================
// SECTION 7 : FONCTIONS DE GESTION
// ============================================================

bool ringtone_service_add(const Ringtone* ringtone);
bool ringtone_service_update(uint8_t index, const Ringtone* ringtone);
bool ringtone_service_delete(uint8_t index);
uint8_t ringtone_service_get_count(void);
Ringtone* ringtone_service_get(uint8_t index);
Ringtone* ringtone_service_get_active(void);
void ringtone_service_set_active(uint8_t index);

// ============================================================
// SECTION 8 : FONCTIONS DE VOLUME
// ============================================================

void ringtone_service_set_volume(uint8_t volume);
uint8_t ringtone_service_get_volume(void);
void ringtone_service_set_notification_volume(uint8_t volume);
uint8_t ringtone_service_get_notification_volume(void);

// ============================================================
// SECTION 9 : FONCTIONS DE NOTIFICATIONS
// ============================================================

bool ringtone_service_add_notification(const NotificationSound* sound);
uint8_t ringtone_service_get_notification_count(void);
NotificationSound* ringtone_service_get_notification(uint8_t index);
void ringtone_service_set_active_notification(uint8_t index);

// ============================================================
// SECTION 10 : FONCTIONS DE TRAITEMENT
// ============================================================

void ringtone_service_process(void);

// ============================================================
// SECTION 11 : FONCTIONS DE CALLBACKS
// ============================================================

void ringtone_service_set_note_callback(RingtoneService_NoteCallback callback);
void ringtone_service_set_finished_callback(RingtoneService_FinishedCallback callback);

// ============================================================
// SECTION 12 : FONCTIONS DE DÉBOGAGE
// ============================================================

void ringtone_service_print_all(void);
void ringtone_service_print_ringtone(uint8_t index);
void ringtone_service_print_state(void);
bool ringtone_service_self_test(void);

// ============================================================
// SECTION 13 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define RINGTONE_DEBUG(fmt, ...)    printf("[RINGTONE] " fmt, ##__VA_ARGS__)
#else
    #define RINGTONE_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 14 : SONNERIES PRÉDÉFINIES
// ============================================================

/** @brief Sonnerie classique (type téléphone) */
extern const Ringtone RINGTONE_CLASSIC;

/** @brief Sonnerie moderne */
extern const Ringtone RINGTONE_MODERN;

/** @brief Sonnerie simple (bip) */
extern const Ringtone RINGTONE_SIMPLE;

/** @brief Sonnerie urgente */
extern const Ringtone RINGTONE_URGENT;

/** @brief Sonnerie mélodique */
extern const Ringtone RINGTONE_MELODIC;

// ============================================================
// SECTION 15 : SONS DE NOTIFICATION PRÉDÉFINIS
// ============================================================

extern const NotificationSound NOTIFY_SOUND_SMS;
extern const NotificationSound NOTIFY_SOUND_EMAIL;
extern const NotificationSound NOTIFY_SOUND_ALARM;
extern const NotificationSound NOTIFY_SOUND_ERROR;

// ============================================================
// SECTION 16 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // RINGTONE_SERVICE_H