/**
 * @file ringtone_service.cpp
 * @brief Implémentation du service de sonneries et mélodies
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans ringtone_service.h.
 * 
 * Il gère :
 * - La lecture des sonneries d'appel
 * - Les sons de notification
 * - Les bips et doubles bips
 * - Les mélodies personnalisées
 * - Le contrôle du volume
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ringtone_service.h"
#include "../drivers/audio/audio_manager.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// SONNERIES PRÉDÉFINIES
// ============================================================

/** @brief Sonnerie classique (type ancien téléphone) */
const Ringtone RINGTONE_CLASSIC = {
    .name = "Classique",
    .type = RINGTONE_TYPE_CALL,
    .notes = {
        {800, 200, 50}, {1000, 200, 50}, {800, 200, 50}, {1000, 200, 400},
        {800, 200, 50}, {1000, 200, 50}, {800, 200, 50}, {1000, 200, 400},
        {800, 200, 50}, {1000, 200, 50}, {800, 200, 50}, {1000, 200, 800}
    },
    .noteCount = 12,
    .tempo = 120,
    .repeat = true,
    .repeatCount = 0,
    .pauseBetweenRepeatsMs = 1000,
    .volume = 80,
    .predefined = true
};

/** @brief Sonnerie moderne (plus mélodique) */
const Ringtone RINGTONE_MODERN = {
    .name = "Moderne",
    .type = RINGTONE_TYPE_CALL,
    .notes = {
        {523, 150, 50}, {659, 150, 50}, {784, 150, 50}, {1047, 300, 200},
        {784, 150, 50}, {659, 150, 50}, {523, 300, 200},
        {659, 150, 50}, {784, 150, 50}, {659, 150, 50}, {523, 300, 500}
    },
    .noteCount = 11,
    .tempo = 140,
    .repeat = true,
    .repeatCount = 0,
    .pauseBetweenRepeatsMs = 800,
    .volume = 80,
    .predefined = true
};

/** @brief Sonnerie simple (bips courts) */
const Ringtone RINGTONE_SIMPLE = {
    .name = "Simple",
    .type = RINGTONE_TYPE_CALL,
    .notes = {
        {1000, 150, 100}, {0, 0, 100}, {1000, 150, 100}, {0, 0, 100},
        {1000, 150, 100}, {0, 0, 100}, {1000, 150, 600}
    },
    .noteCount = 7,
    .tempo = 100,
    .repeat = true,
    .repeatCount = 0,
    .pauseBetweenRepeatsMs = 1500,
    .volume = 70,
    .predefined = true
};

/** @brief Sonnerie urgente (rythme rapide) */
const Ringtone RINGTONE_URGENT = {
    .name = "Urgence",
    .type = RINGTONE_TYPE_CALL,
    .notes = {
        {1200, 100, 50}, {1200, 100, 50}, {1200, 100, 50}, {0, 0, 200},
        {1200, 100, 50}, {1200, 100, 50}, {1200, 100, 50}, {0, 0, 200},
        {1200, 300, 50}, {1200, 300, 50}, {1200, 300, 500}
    },
    .noteCount = 11,
    .tempo = 180,
    .repeat = true,
    .repeatCount = 0,
    .pauseBetweenRepeatsMs = 500,
    .volume = 100,
    .predefined = true
};

/** @brief Sonnerie mélodique (douce) */
const Ringtone RINGTONE_MELODIC = {
    .name = "Mélodique",
    .type = RINGTONE_TYPE_CALL,
    .notes = {
        {262, 200, 50}, {330, 200, 50}, {392, 200, 50}, {523, 400, 100},
        {392, 200, 50}, {330, 200, 50}, {262, 400, 100},
        {294, 200, 50}, {392, 200, 50}, {349, 200, 50}, {330, 400, 500}
    },
    .noteCount = 11,
    .tempo = 100,
    .repeat = true,
    .repeatCount = 0,
    .pauseBetweenRepeatsMs = 1200,
    .volume = 75,
    .predefined = true
};

/** @brief Son notification SMS */
const NotificationSound NOTIFY_SOUND_SMS = {
    .name = "SMS",
    .frequency = 2000,
    .durationMs = 80,
    .repeatCount = 2,
    .repeatIntervalMs = 100
};

/** @brief Son notification email */
const NotificationSound NOTIFY_SOUND_EMAIL = {
    .name = "Email",
    .frequency = 1500,
    .durationMs = 100,
    .repeatCount = 1,
    .repeatIntervalMs = 0
};

/** @brief Son alarme */
const NotificationSound NOTIFY_SOUND_ALARM = {
    .name = "Alarme",
    .frequency = 800,
    .durationMs = 300,
    .repeatCount = 3,
    .repeatIntervalMs = 200
};

/** @brief Son erreur */
const NotificationSound NOTIFY_SOUND_ERROR = {
    .name = "Erreur",
    .frequency = 200,
    .durationMs = 400,
    .repeatCount = 1,
    .repeatIntervalMs = 0
};

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du service */
static RingtoneServiceState ringtone_state;

/** @brief Callbacks */
static RingtoneService_NoteCallback note_cb = NULL;
static RingtoneService_FinishedCallback finished_cb = NULL;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le service de sonneries
 */
bool ringtone_service_init(void)
{
    RINGTONE_DEBUG("Initialisation du service de sonneries...\n");
    
    memset(&ringtone_state, 0, sizeof(RingtoneServiceState));
    
    // Ajouter les sonneries prédéfinies
    ringtone_service_add(&RINGTONE_CLASSIC);
    ringtone_service_add(&RINGTONE_MODERN);
    ringtone_service_add(&RINGTONE_SIMPLE);
    ringtone_service_add(&RINGTONE_URGENT);
    ringtone_service_add(&RINGTONE_MELODIC);
    
    // Ajouter les sons de notification prédéfinis
    ringtone_service_add_notification(&NOTIFY_SOUND_SMS);
    ringtone_service_add_notification(&NOTIFY_SOUND_EMAIL);
    ringtone_service_add_notification(&NOTIFY_SOUND_ALARM);
    ringtone_service_add_notification(&NOTIFY_SOUND_ERROR);
    
    // Sonnerie active par défaut
    ringtone_state.activeRingtoneIndex = 0;
    ringtone_state.activeNotificationIndex = 0;
    
    // Volumes par défaut
    ringtone_state.ringtoneVolume = 80;
    ringtone_state.notificationVolume = 70;
    
    ringtone_state.initialized = true;
    
    RINGTONE_DEBUG("Service initialisé (%d sonneries, %d notifications)\n",
                  ringtone_state.ringtoneCount, ringtone_state.notificationCount);
    return true;
}

void ringtone_service_deinit(void)
{
    ringtone_service_stop();
    ringtone_state.initialized = false;
}

bool ringtone_service_is_ready(void)
{
    return ringtone_state.initialized;
}

// ============================================================
// SECTION 2 : LECTURE
// ============================================================

bool ringtone_service_play(uint8_t ringtoneIndex)
{
    if (!ringtone_state.initialized) return false;
    if (ringtoneIndex >= ringtone_state.ringtoneCount) return false;
    
    // Arrêter la lecture en cours
    ringtone_service_stop();
    
    Ringtone* ringtone = &ringtone_state.ringtones[ringtoneIndex];
    
    ringtone_state.playing = true;
    ringtone_state.stopped = false;
    ringtone_state.currentRingtoneIndex = ringtoneIndex;
    ringtone_state.currentNoteIndex = 0;
    ringtone_state.noteStartTime = HAL_GetTick();
    
    RINGTONE_DEBUG("Lecture : %s (%d notes)\n", ringtone->name, ringtone->noteCount);
    
    // Jouer la première note immédiatement
    play_current_note();
    
    ringtone_state.totalRingtonePlays++;
    
    return true;
}

bool ringtone_service_play_call_ringtone(void)
{
    return ringtone_service_play(ringtone_state.activeRingtoneIndex);
}

bool ringtone_service_play_notification(void)
{
    if (!ringtone_state.initialized) return false;
    if (ringtone_state.activeNotificationIndex >= ringtone_state.notificationCount) return false;
    
    NotificationSound* sound = &ringtone_state.notifications[ringtone_state.activeNotificationIndex];
    
    RINGTONE_DEBUG("Notification : %s (%d Hz, %d ms)\n", 
                  sound->name, sound->frequency, sound->durationMs);
    
    // Jouer le son de notification (bips répétés)
    for (uint8_t i = 0; i <= sound->repeatCount; i++)
    {
        audio_manager_play_beep(sound->frequency, sound->durationMs);
        
        if (i < sound->repeatCount)
        {
            HAL_Delay(sound->repeatIntervalMs);
        }
    }
    
    ringtone_state.totalNotificationPlays++;
    
    return true;
}

bool ringtone_service_play_alarm(void)
{
    return ringtone_service_play(3);  // Sonnerie urgence par défaut
}

bool ringtone_service_play_system_sound(const char* soundName)
{
    if (strcmp(soundName, "startup") == 0)
    {
        ringtone_service_play_beep(523, 100);  // Do
        HAL_Delay(50);
        ringtone_service_play_beep(659, 100);  // Mi
        HAL_Delay(50);
        ringtone_service_play_beep(784, 200);  // Sol
    }
    else if (strcmp(soundName, "shutdown") == 0)
    {
        ringtone_service_play_beep(784, 150);  // Sol
        HAL_Delay(50);
        ringtone_service_play_beep(659, 150);  // Mi
        HAL_Delay(50);
        ringtone_service_play_beep(523, 300);  // Do
    }
    else if (strcmp(soundName, "error") == 0)
    {
        ringtone_service_play_beep(200, 500);
    }
    else if (strcmp(soundName, "click") == 0)
    {
        ringtone_service_play_beep(3000, 5);
    }
    
    return true;
}

bool ringtone_service_play_beep(uint16_t frequency, uint16_t durationMs)
{
    if (!ringtone_state.initialized) return false;
    
    audio_manager_play_beep(frequency, durationMs);
    return true;
}

bool ringtone_service_play_double_beep(uint16_t freq1, uint16_t freq2, uint16_t durationMs)
{
    ringtone_service_play_beep(freq1, durationMs);
    HAL_Delay(durationMs + 50);
    ringtone_service_play_beep(freq2, durationMs);
    return true;
}

bool ringtone_service_play_melody(const MusicalNote* notes, uint16_t noteCount)
{
    if (!ringtone_state.initialized || notes == NULL) return false;
    
    for (uint16_t i = 0; i < noteCount; i++)
    {
        if (notes[i].frequency > 0)
        {
            ringtone_service_play_beep(notes[i].frequency, notes[i].durationMs);
        }
        
        uint16_t totalDelay = notes[i].durationMs + notes[i].pauseMs;
        if (totalDelay > 0)
        {
            HAL_Delay(totalDelay);
        }
    }
    
    return true;
}

void ringtone_service_stop(void)
{
    ringtone_state.playing = false;
    ringtone_state.stopped = true;
    ringtone_state.currentNoteIndex = 0;
    
    audio_manager_stop_tone();
    
    RINGTONE_DEBUG("Lecture arrêtée\n");
}

bool ringtone_service_is_playing(void)
{
    return ringtone_state.playing;
}

// ============================================================
// SECTION 3 : LECTURE INTERNE
// ============================================================

/**
 * @brief Joue la note courante de la sonnerie
 */
static void play_current_note(void)
{
    if (!ringtone_state.playing) return;
    
    Ringtone* ringtone = &ringtone_state.ringtones[ringtone_state.currentRingtoneIndex];
    
    if (ringtone_state.currentNoteIndex >= ringtone->noteCount)
    {
        // Fin de la mélodie
        if (ringtone->repeat)
        {
            if (ringtone->repeatCount == 0 || 
                ringtone_state.currentNoteIndex / ringtone->noteCount < ringtone->repeatCount)
            {
                // Pause entre les répétitions
                ringtone_state.currentNoteIndex = 0;
                ringtone_state.noteStartTime = HAL_GetTick() + ringtone->pauseBetweenRepeatsMs;
                return;
            }
        }
        
        // Fin complète
        ringtone_state.playing = false;
        
        if (finished_cb) finished_cb();
        return;
    }
    
    MusicalNote* note = &ringtone->notes[ringtone_state.currentNoteIndex];
    
    // Jouer la note
    if (note->frequency > 0)
    {
        if (note_cb)
        {
            note_cb(note->frequency, note->durationMs);
        }
        else
        {
            audio_manager_play_beep(note->frequency, note->durationMs);
        }
    }
    
    ringtone_state.noteStartTime = HAL_GetTick();
}

// ============================================================
// SECTION 4 : GESTION
// ============================================================

bool ringtone_service_add(const Ringtone* ringtone)
{
    if (!ringtone_state.initialized || ringtone == NULL) return false;
    if (ringtone_state.ringtoneCount >= RINGTONE_MAX_COUNT) return false;
    
    memcpy(&ringtone_state.ringtones[ringtone_state.ringtoneCount], ringtone, sizeof(Ringtone));
    ringtone_state.ringtoneCount++;
    
    RINGTONE_DEBUG("Sonnerie ajoutée : %s\n", ringtone->name);
    return true;
}

bool ringtone_service_update(uint8_t index, const Ringtone* ringtone)
{
    if (index >= ringtone_state.ringtoneCount || ringtone == NULL) return false;
    
    // Ne pas modifier les sonneries prédéfinies
    if (ringtone_state.ringtones[index].predefined) return false;
    
    memcpy(&ringtone_state.ringtones[index], ringtone, sizeof(Ringtone));
    return true;
}

bool ringtone_service_delete(uint8_t index)
{
    if (index >= ringtone_state.ringtoneCount) return false;
    
    // Ne pas supprimer les sonneries prédéfinies
    if (ringtone_state.ringtones[index].predefined) return false;
    
    if (index < ringtone_state.ringtoneCount - 1)
    {
        memmove(&ringtone_state.ringtones[index], &ringtone_state.ringtones[index + 1],
                (ringtone_state.ringtoneCount - index - 1) * sizeof(Ringtone));
    }
    ringtone_state.ringtoneCount--;
    
    return true;
}

uint8_t ringtone_service_get_count(void)
{
    return ringtone_state.ringtoneCount;
}

Ringtone* ringtone_service_get(uint8_t index)
{
    if (index >= ringtone_state.ringtoneCount) return NULL;
    return &ringtone_state.ringtones[index];
}

Ringtone* ringtone_service_get_active(void)
{
    return &ringtone_state.ringtones[ringtone_state.activeRingtoneIndex];
}

void ringtone_service_set_active(uint8_t index)
{
    if (index >= ringtone_state.ringtoneCount) return;
    ringtone_state.activeRingtoneIndex = index;
}

// ============================================================
// SECTION 5 : VOLUME
// ============================================================

void ringtone_service_set_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    ringtone_state.ringtoneVolume = volume;
}

uint8_t ringtone_service_get_volume(void)
{
    return ringtone_state.ringtoneVolume;
}

void ringtone_service_set_notification_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    ringtone_state.notificationVolume = volume;
}

uint8_t ringtone_service_get_notification_volume(void)
{
    return ringtone_state.notificationVolume;
}

// ============================================================
// SECTION 6 : NOTIFICATIONS
// ============================================================

bool ringtone_service_add_notification(const NotificationSound* sound)
{
    if (!ringtone_state.initialized || sound == NULL) return false;
    if (ringtone_state.notificationCount >= RINGTONE_MAX_NOTIFICATIONS) return false;
    
    memcpy(&ringtone_state.notifications[ringtone_state.notificationCount], 
           sound, sizeof(NotificationSound));
    ringtone_state.notificationCount++;
    
    return true;
}

uint8_t ringtone_service_get_notification_count(void)
{
    return ringtone_state.notificationCount;
}

NotificationSound* ringtone_service_get_notification(uint8_t index)
{
    if (index >= ringtone_state.notificationCount) return NULL;
    return &ringtone_state.notifications[index];
}

void ringtone_service_set_active_notification(uint8_t index)
{
    if (index >= ringtone_state.notificationCount) return;
    ringtone_state.activeNotificationIndex = index;
}

// ============================================================
// SECTION 7 : TRAITEMENT
// ============================================================

void ringtone_service_process(void)
{
    if (!ringtone_state.initialized || !ringtone_state.playing) return;
    
    Ringtone* ringtone = &ringtone_state.ringtones[ringtone_state.currentRingtoneIndex];
    MusicalNote* note = &ringtone->notes[ringtone_state.currentNoteIndex];
    
    // Calculer la durée de la note (avec pause)
    uint16_t totalDuration = note->durationMs + note->pauseMs;
    
    // Vérifier si la note est terminée
    uint32_t elapsed = HAL_GetTick() - ringtone_state.noteStartTime;
    
    if (elapsed >= totalDuration)
    {
        // Passer à la note suivante
        ringtone_state.currentNoteIndex++;
        play_current_note();
    }
}

// ============================================================
// SECTION 8 : CALLBACKS
// ============================================================

void ringtone_service_set_note_callback(RingtoneService_NoteCallback cb) { note_cb = cb; }
void ringtone_service_set_finished_callback(RingtoneService_FinishedCallback cb) { finished_cb = cb; }

// ============================================================
// SECTION 9 : DÉBOGAGE
// ============================================================

void ringtone_service_print_all(void)
{
    printf("\n═══ SONNERIES (%d) ═══\n", ringtone_state.ringtoneCount);
    
    for (uint8_t i = 0; i < ringtone_state.ringtoneCount; i++)
    {
        Ringtone* r = &ringtone_state.ringtones[i];
        printf("[%d] %-20s %-8s %d notes %s %s\n",
               i, r->name,
               r->type == RINGTONE_TYPE_CALL ? "APPEL" : "AUTRE",
               r->noteCount,
               r->predefined ? "[SYS]" : "",
               i == ringtone_state.activeRingtoneIndex ? "◄ ACTIVE" : "");
    }
    printf("══════════════════════════\n\n");
}

void ringtone_service_print_ringtone(uint8_t index)
{
    Ringtone* r = ringtone_service_get(index);
    if (r == NULL) return;
    
    printf("\n═══ SONNERIE : %s ═══\n", r->name);
    printf("Type    : %d\n", r->type);
    printf("Notes   : %d\n", r->noteCount);
    printf("Tempo   : %d BPM\n", r->tempo);
    printf("Répète  : %s (x%d)\n", r->repeat ? "Oui" : "Non", r->repeatCount);
    printf("Volume  : %d%%\n", r->volume);
    
    printf("Mélodie :\n");
    for (uint16_t i = 0; i < r->noteCount && i < 20; i++)
    {
        printf("  Note %2d : %5d Hz, %4d ms, %4d ms pause\n",
               i + 1, r->notes[i].frequency, r->notes[i].durationMs, r->notes[i].pauseMs);
    }
    if (r->noteCount > 20) printf("  ... et %d autres notes\n", r->noteCount - 20);
    printf("══════════════════════\n\n");
}

void ringtone_service_print_state(void)
{
    printf("\n═══ ÉTAT SERVICE SONNERIES ═══\n");
    printf("Sonneries       : %d\n", ringtone_state.ringtoneCount);
    printf("Active          : %d (%s)\n", ringtone_state.activeRingtoneIndex,
           ringtone_state.ringtones[ringtone_state.activeRingtoneIndex].name);
    printf("Notifications   : %d\n", ringtone_state.notificationCount);
    printf("En lecture      : %s\n", ringtone_state.playing ? "Oui" : "Non");
    printf("Volume sonnerie : %d%%\n", ringtone_state.ringtoneVolume);
    printf("Volume notif.   : %d%%\n", ringtone_state.notificationVolume);
    printf("Total lectures  : %lu\n", (unsigned long)ringtone_state.totalRingtonePlays);
    printf("══════════════════════════\n\n");
}

bool ringtone_service_self_test(void)
{
    RINGTONE_DEBUG("Auto-test...\n");
    
    if (!ringtone_state.initialized)
    {
        RINGTONE_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : jouer un bip
    ringtone_service_play_beep(1000, 50);
    HAL_Delay(100);
    
    // Test : jouer une mélodie simple
    MusicalNote testNotes[] = {
        {523, 100, 50},  // Do
        {659, 100, 50},  // Mi
        {784, 100, 50}   // Sol
    };
    ringtone_service_play_melody(testNotes, 3);
    
    RINGTONE_DEBUG("Auto-test OK\n");
    return true;
}