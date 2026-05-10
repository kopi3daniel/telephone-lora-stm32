/**
 * @file    app_events.cpp
 * @brief   Implémentation du gestionnaire d'événements
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente la file d'événements thread-safe.
 * 
 * FONCTIONNEMENT DU CIRCULAR BUFFER :
 * 
 *   Écriture (Post) : write_index avance, count++
 *   Lecture  (Get)  : read_index avance, count--
 * 
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 *   │ E │ E │ E │   │   │   │   │   │
 *   └───┴───┴───┴───┴───┴───┴───┴───┘
 *     ↑ read=0         ↑ write=3  count=3
 * 
 *   Après Get() :
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 *   │   │ E │ E │   │   │   │   │   │
 *   └───┴───┴───┴───┴───┴───┴───┴───┘
 *         ↑ read=1     ↑ write=3  count=2
 * 
 *   File pleine (count == QUEUE_SIZE) :
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 *   │ E │ E │ E │ E │ E │ E │ E │ E │
 *   └───┴───┴───┴───┴───┴───┴───┴───┘
 *     ↑ read=0                     count=8 (pleine)
 * 
 *   Si overwrite_oldest = true :
 *     → read_index avance (écrase le plus ancien)
 *     → write_index prend sa place
 *   Si overwrite_oldest = false :
 *     → L'événement est perdu
 *     → total_lost++
 * 
 * THREAD SAFETY :
 * 
 *   Les fonctions Post sont protégées par __disable_irq() / __enable_irq().
 *   Cela garantit l'atomicité des opérations sur les index même si
 *   appelées depuis une ISR.
 * 
 *   Les fonctions Get ne nécessitent pas de protection car elles
 *   sont appelées uniquement depuis la boucle principale (thread unique).
 *   Cependant, on désactive quand même les IRQ pour éviter qu'une ISR
 *   ne modifie les index pendant la lecture.
 * 
 * STATISTIQUES :
 * 
 *   total_posted   : Nombre total d'événements ajoutés (réussis ou non)
 *   total_consumed : Nombre total d'événements lus
 *   total_lost     : Événements perdus (file pleine sans overwrite)
 *   total_errors   : Erreurs diverses
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "app_events.h"

/* Utilitaires */
#include "../utils/debug_utils.h"

/* HAL */
#include "stm32f4xx_hal.h"

/* Standard */
#include <string.h>
#include <stdio.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs */
#define TAG                                 "EventQueue"

/** Macro pour entrer en section critique */
#define CRITICAL_ENTER()                    __disable_irq()

/** Macro pour sortir de section critique */
#define CRITICAL_EXIT()                     __enable_irq()

/** Niveau de log maximal (évite de spammer) */
#define MAX_LOG_PER_SECOND                  10

/* ======================================================================== */
/*                VARIABLES STATIQUES                                       */
/* ======================================================================== */

/** Noms lisibles des types d'événements */
static const char* EVENT_TYPE_NAMES[] = {
    [APP_EVENT_NONE]                = "NONE",
    [APP_EVENT_SYSTEM_TICK]         = "SYSTEM_TICK",
    [APP_EVENT_TIMER_EXPIRED]       = "TIMER_EXPIRED",
    [APP_EVENT_WATCHDOG_WARNING]    = "WATCHDOG_WARNING",
    [APP_EVENT_TOUCH_PRESS]         = "TOUCH_PRESS",
    [APP_EVENT_TOUCH_RELEASE]       = "TOUCH_RELEASE",
    [APP_EVENT_TOUCH_MOVE]          = "TOUCH_MOVE",
    [APP_EVENT_TOUCH_SWIPE_UP]      = "TOUCH_SWIPE_UP",
    [APP_EVENT_TOUCH_SWIPE_DOWN]    = "TOUCH_SWIPE_DOWN",
    [APP_EVENT_TOUCH_SWIPE_LEFT]    = "TOUCH_SWIPE_LEFT",
    [APP_EVENT_TOUCH_SWIPE_RIGHT]   = "TOUCH_SWIPE_RIGHT",
    [APP_EVENT_TOUCH_LONG_PRESS]    = "TOUCH_LONG_PRESS",
    [APP_EVENT_TOUCH_DOUBLE_TAP]    = "TOUCH_DOUBLE_TAP",
    [APP_EVENT_KEY_PRESS]           = "KEY_PRESS",
    [APP_EVENT_KEY_RELEASE]         = "KEY_RELEASE",
    [APP_EVENT_KEY_LONG_PRESS]      = "KEY_LONG_PRESS",
    [APP_EVENT_KEY_REPEAT]          = "KEY_REPEAT",
    [APP_EVENT_LORA_PACKET_RECEIVED]= "LORA_PACKET",
    [APP_EVENT_LORA_TX_COMPLETE]    = "LORA_TX_DONE",
    [APP_EVENT_LORA_RX_TIMEOUT]     = "LORA_RX_TIMEOUT",
    [APP_EVENT_LORA_RX_ERROR]       = "LORA_RX_ERROR",
    [APP_EVENT_LORA_CRC_ERROR]      = "LORA_CRC_ERROR",
    [APP_EVENT_LORA_CAD_DETECTED]   = "LORA_CAD",
    [APP_EVENT_INCOMING_CALL]       = "INCOMING_CALL",
    [APP_EVENT_CALL_ACCEPTED]       = "CALL_ACCEPTED",
    [APP_EVENT_CALL_REJECTED]       = "CALL_REJECTED",
    [APP_EVENT_CALL_ENDED]          = "CALL_ENDED",
    [APP_EVENT_CALL_MISSED]         = "CALL_MISSED",
    [APP_EVENT_CALL_CONNECTED]      = "CALL_CONNECTED",
    [APP_EVENT_CALL_TIMEOUT]        = "CALL_TIMEOUT",
    [APP_EVENT_CALL_BUSY]           = "CALL_BUSY",
    [APP_EVENT_NEW_MESSAGE]         = "NEW_MESSAGE",
    [APP_EVENT_MESSAGE_SENT]        = "MESSAGE_SENT",
    [APP_EVENT_MESSAGE_FAILED]      = "MESSAGE_FAILED",
    [APP_EVENT_MESSAGE_DELIVERED]   = "MESSAGE_DELIVERED",
    [APP_EVENT_MESSAGE_READ]        = "MESSAGE_READ",
    [APP_EVENT_BATTERY_LOW]         = "BATTERY_LOW",
    [APP_EVENT_BATTERY_CRITICAL]    = "BATTERY_CRITICAL",
    [APP_EVENT_BATTERY_NORMAL]      = "BATTERY_NORMAL",
    [APP_EVENT_CHARGING_START]      = "CHARGING_START",
    [APP_EVENT_CHARGING_STOP]       = "CHARGING_STOP",
    [APP_EVENT_SCREEN_TIMEOUT]      = "SCREEN_TIMEOUT",
    [APP_EVENT_SCREEN_WAKEUP]       = "SCREEN_WAKEUP",
    [APP_EVENT_SYSTEM_ERROR]        = "SYSTEM_ERROR",
    [APP_EVENT_SYSTEM_WARNING]      = "SYSTEM_WARNING",
    [APP_EVENT_FACTORY_RESET]       = "FACTORY_RESET",
};

/** Noms lisibles des priorités */
static const char* PRIORITY_NAMES[] = {
    [APP_PRIORITY_LOW]      = "LOW",
    [APP_PRIORITY_NORMAL]   = "NORMAL",
    [APP_PRIORITY_HIGH]     = "HIGH",
    [APP_PRIORITY_CRITICAL] = "CRITICAL",
};

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

static bool is_queue_full(AppEventQueue_t* queue);
static bool is_queue_empty(AppEventQueue_t* queue);
static void log_event(const AppEvent_t* event, const char* action);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise la file d'événements
 */
void AppEventQueue_Init(AppEventQueue_t* queue, bool overwrite)
{
    if (!queue) return;

    DEBUG_INFO(TAG, "Initialisation de la file d'événements...");

    /* Mise à zéro */
    memset(queue, 0, sizeof(AppEventQueue_t));

    /* Configuration */
    queue->overwrite_oldest = overwrite;
    queue->log_events = false;  /* Désactivé par défaut (évite spam) */

    /* Index */
    queue->read_index = 0;
    queue->write_index = 0;
    queue->count = 0;

    /* Stats */
    queue->total_posted = 0;
    queue->total_consumed = 0;
    queue->total_lost = 0;
    queue->total_errors = 0;

    /* Callback */
    queue->on_overflow = NULL;

    DEBUG_INFO(TAG, "File initialisée (capacité=%d, overwrite=%s)",
               APP_EVENT_QUEUE_SIZE, overwrite ? "oui" : "non");
}

/**
 * @brief Réinitialise la file
 */
void AppEventQueue_Reset(AppEventQueue_t* queue)
{
    if (!queue) return;

    CRITICAL_ENTER();

    queue->read_index = 0;
    queue->write_index = 0;
    queue->count = 0;

    /* Vider le buffer (sécurité) */
    memset(queue->buffer, 0, sizeof(queue->buffer));

    CRITICAL_EXIT();

    DEBUG_INFO(TAG, "File réinitialisée");
}

/**
 * @brief Poste un événement dans la file
 */
bool AppEventQueue_Post(AppEventQueue_t* queue, const AppEvent_t* event)
{
    if (!queue || !event) return false;

    CRITICAL_ENTER();

    /* Incrémenter le compteur total */
    queue->total_posted++;

    /* Vérifier si la file est pleine */
    if (is_queue_full(queue)) {
        if (queue->overwrite_oldest) {
            /* Écraser le plus ancien : avancer read_index */
            queue->read_index = (queue->read_index + 1) % APP_EVENT_QUEUE_SIZE;
            queue->count--;
            queue->total_lost++;  /* Comptabilisé comme perdu */

            if (queue->log_events) {
                DEBUG_WARN(TAG, "File pleine - Écrasement du plus ancien événement");
            }
        } else {
            /* File pleine, événement perdu */
            queue->total_lost++;

            if (queue->on_overflow) {
                queue->on_overflow();
            }

            if (queue->log_events) {
                DEBUG_WARN(TAG, "File pleine - Événement %s perdu (#%lu)",
                           AppEvent_GetTypeName(event->type),
                           queue->total_lost);
            }

            CRITICAL_EXIT();
            return false;
        }
    }

    /* Copier l'événement dans le buffer */
    memcpy(&queue->buffer[queue->write_index], event, sizeof(AppEvent_t));

    /* Mettre à jour le timestamp si non défini */
    if (queue->buffer[queue->write_index].timestamp_ms == 0) {
        queue->buffer[queue->write_index].timestamp_ms = HAL_GetTick();
    }

    /* Avancer l'index d'écriture */
    queue->write_index = (queue->write_index + 1) % APP_EVENT_QUEUE_SIZE;
    queue->count++;

    CRITICAL_EXIT();

    /* Logger (hors section critique) */
    if (queue->log_events) {
        log_event(event, "POST");
    }

    return true;
}

/**
 * @brief Poste un événement simple (sans données)
 */
bool AppEventQueue_PostSimple(AppEventQueue_t* queue,
                              AppEventType_t type,
                              AppEventPriority_t priority)
{
    AppEvent_t event;
    memset(&event, 0, sizeof(event));

    event.type = type;
    event.priority = priority;
    event.timestamp_ms = HAL_GetTick();

    return AppEventQueue_Post(queue, &event);
}

/**
 * @brief Récupère le prochain événement
 */
bool AppEventQueue_Get(AppEventQueue_t* queue, AppEvent_t* event)
{
    if (!queue || !event) return false;

    /* Vérifier si la file est vide */
    if (is_queue_empty(queue)) {
        return false;
    }

    CRITICAL_ENTER();

    /* Copier l'événement depuis le buffer */
    memcpy(event, &queue->buffer[queue->read_index], sizeof(AppEvent_t));

    /* Marquer comme traité */
    event->handled = true;

    /* Effacer l'entrée (sécurité) */
    memset(&queue->buffer[queue->read_index], 0, sizeof(AppEvent_t));

    /* Avancer l'index de lecture */
    queue->read_index = (queue->read_index + 1) % APP_EVENT_QUEUE_SIZE;
    queue->count--;
    queue->total_consumed++;

    CRITICAL_EXIT();

    /* Logger */
    if (queue->log_events) {
        log_event(event, "GET");
    }

    return true;
}

/**
 * @brief Consulte sans retirer
 */
bool AppEventQueue_Peek(AppEventQueue_t* queue, AppEvent_t* event)
{
    if (!queue || !event) return false;

    if (is_queue_empty(queue)) {
        return false;
    }

    CRITICAL_ENTER();

    /* Copier sans modifier les index */
    memcpy(event, &queue->buffer[queue->read_index], sizeof(AppEvent_t));

    CRITICAL_EXIT();

    return true;
}

/**
 * @brief Vide complètement la file
 */
void AppEventQueue_Flush(AppEventQueue_t* queue)
{
    if (!queue) return;

    CRITICAL_ENTER();

    /* Réinitialiser les index */
    queue->read_index = 0;
    queue->write_index = 0;
    queue->count = 0;

    /* Effacer le buffer */
    memset(queue->buffer, 0, sizeof(queue->buffer));

    CRITICAL_EXIT();

    DEBUG_INFO(TAG, "File vidée");
}

/**
 * @brief Retourne le nombre d'événements
 */
uint8_t AppEventQueue_GetCount(AppEventQueue_t* queue)
{
    if (!queue) return 0;

    uint8_t count;
    CRITICAL_ENTER();
    count = queue->count;
    CRITICAL_EXIT();

    return count;
}

/**
 * @brief Vérifie si la file est vide
 */
bool AppEventQueue_IsEmpty(AppEventQueue_t* queue)
{
    if (!queue) return true;

    bool empty;
    CRITICAL_ENTER();
    empty = is_queue_empty(queue);
    CRITICAL_EXIT();

    return empty;
}

/**
 * @brief Vérifie si la file est pleine
 */
bool AppEventQueue_IsFull(AppEventQueue_t* queue)
{
    if (!queue) return false;

    bool full;
    CRITICAL_ENTER();
    full = is_queue_full(queue);
    CRITICAL_EXIT();

    return full;
}

/**
 * @brief Récupère les statistiques
 */
void AppEventQueue_GetStats(AppEventQueue_t* queue,
                            uint32_t* total_posted,
                            uint32_t* total_lost,
                            uint32_t* total_errors)
{
    if (!queue) return;

    CRITICAL_ENTER();
    if (total_posted) *total_posted = queue->total_posted;
    if (total_lost)   *total_lost   = queue->total_lost;
    if (total_errors) *total_errors = queue->total_errors;
    CRITICAL_EXIT();
}

/**
 * @brief Définit le callback de débordement
 */
void AppEventQueue_SetOverflowCallback(AppEventQueue_t* queue,
                                       void (*callback)(void))
{
    if (!queue) return;
    queue->on_overflow = callback;
}

/**
 * @brief Active/désactive les logs
 */
void AppEventQueue_SetLogging(AppEventQueue_t* queue, bool enable)
{
    if (!queue) return;
    queue->log_events = enable;

    DEBUG_INFO(TAG, "Logs d'événements %s", enable ? "activés" : "désactivés");
}

/* ======================================================================== */
/*              FONCTIONS UTILITAIRES                                       */
/* ======================================================================== */

/**
 * @brief Retourne le nom lisible d'un type d'événement
 */
const char* AppEvent_GetTypeName(AppEventType_t type)
{
    if (type >= APP_EVENT_COUNT) return "UNKNOWN";
    return EVENT_TYPE_NAMES[type] ? EVENT_TYPE_NAMES[type] : "UNNAMED";
}

/**
 * @brief Retourne le nom lisible d'une priorité
 */
const char* AppEvent_GetPriorityName(AppEventPriority_t priority)
{
    if (priority >= APP_PRIORITY_COUNT) return "UNKNOWN";
    return PRIORITY_NAMES[priority] ? PRIORITY_NAMES[priority] : "UNNAMED";
}

/**
 * @brief Crée un événement tactile
 */
void AppEvent_CreateTouchEvent(AppEvent_t* event,
                               AppEventType_t type,
                               uint16_t x,
                               uint16_t y)
{
    if (!event) return;

    memset(event, 0, sizeof(AppEvent_t));
    event->type = type;
    event->priority = APP_PRIORITY_NORMAL;
    event->timestamp_ms = HAL_GetTick();

    event->data.touch.x = x;
    event->data.touch.y = y;
    event->data.touch.pressure = 0;
    event->data.touch.gesture = 0;
    event->data.touch.touch_id = 0;
}

/**
 * @brief Crée un événement clavier
 */
void AppEvent_CreateKeyEvent(AppEvent_t* event,
                             uint16_t key_code,
                             bool repeated)
{
    if (!event) return;

    memset(event, 0, sizeof(AppEvent_t));

    if (repeated) {
        event->type = APP_EVENT_KEY_REPEAT;
    } else {
        event->type = APP_EVENT_KEY_PRESS;
    }
    event->priority = APP_PRIORITY_NORMAL;
    event->timestamp_ms = HAL_GetTick();

    event->data.key.key_code = key_code;
    event->data.key.repeated = repeated;
    event->data.key.repeat_count = 0;
}

/**
 * @brief Crée un événement d'appel entrant
 */
void AppEvent_CreateIncomingCallEvent(AppEvent_t* event,
                                      const char* caller_id,
                                      const char* caller_number)
{
    if (!event) return;

    memset(event, 0, sizeof(AppEvent_t));
    event->type = APP_EVENT_INCOMING_CALL;
    event->priority = APP_PRIORITY_HIGH;
    event->timestamp_ms = HAL_GetTick();

    if (caller_id) {
        strncpy(event->data.call.caller_id, caller_id, 
                APP_EVENT_ID_MAX_LENGTH - 1);
    }
    if (caller_number) {
        strncpy(event->data.call.caller_number, caller_number,
                APP_EVENT_PHONE_MAX_LENGTH - 1);
    }
    event->data.call.call_id = 0;
}

/**
 * @brief Crée un événement message
 */
void AppEvent_CreateMessageEvent(AppEvent_t* event,
                                 const char* sender,
                                 const char* content)
{
    if (!event) return;

    memset(event, 0, sizeof(AppEvent_t));
    event->type = APP_EVENT_NEW_MESSAGE;
    event->priority = APP_PRIORITY_HIGH;
    event->timestamp_ms = HAL_GetTick();

    if (sender) {
        strncpy(event->data.message.sender, sender,
                APP_EVENT_ID_MAX_LENGTH - 1);
    }
    if (content) {
        strncpy(event->data.message.content, content,
                APP_EVENT_MESSAGE_MAX_LENGTH - 1);
        event->data.message.content_length = strlen(content);
    }
}

/**
 * @brief Crée un événement erreur système
 */
void AppEvent_CreateErrorEvent(AppEvent_t* event,
                               uint32_t code,
                               const char* msg,
                               const char* file,
                               uint32_t line)
{
    if (!event) return;

    memset(event, 0, sizeof(AppEvent_t));
    event->type = APP_EVENT_SYSTEM_ERROR;
    event->priority = APP_PRIORITY_CRITICAL;
    event->timestamp_ms = HAL_GetTick();

    event->data.error.error_code = code;
    event->data.error.line = line;
    event->data.error.is_fatal = true;

    if (msg) {
        strncpy(event->data.error.error_msg, msg,
                APP_EVENT_MESSAGE_MAX_LENGTH - 1);
    }
    if (file) {
        strncpy(event->data.error.file, file, 63);
    }
}

/**
 * @brief Vérifie si un événement est de haute priorité
 */
bool AppEvent_IsHighPriority(const AppEvent_t* event)
{
    if (!event) return false;
    return event->priority >= APP_PRIORITY_HIGH;
}

/**
 * @brief Clone un événement
 */
void AppEvent_Clone(AppEvent_t* dest, const AppEvent_t* src)
{
    if (!dest || !src) return;
    memcpy(dest, src, sizeof(AppEvent_t));
}

/* ======================================================================== */
/*              FONCTIONS PRIVÉES                                           */
/* ======================================================================== */

/**
 * @brief Vérifie si la file est pleine
 * 
 * Note : Doit être appelée dans une section critique.
 */
static bool is_queue_full(AppEventQueue_t* queue)
{
    if (!queue) return false;
    return queue->count >= APP_EVENT_QUEUE_SIZE;
}

/**
 * @brief Vérifie si la file est vide
 * 
 * Note : Doit être appelée dans une section critique.
 */
static bool is_queue_empty(AppEventQueue_t* queue)
{
    if (!queue) return true;
    return queue->count == 0;
}

/**
 * @brief Log un événement
 * 
 * Affiche le type, la priorité et le timestamp.
 * Limité à MAX_LOG_PER_SECOND pour éviter le spam.
 */
static void log_event(const AppEvent_t* event, const char* action)
{
    if (!event) return;

    /* Limiter le nombre de logs par seconde */
    static uint32_t last_log_ms = 0;
    static uint8_t log_count = 0;
    uint32_t now = HAL_GetTick();

    /* Réinitialiser le compteur toutes les secondes */
    if (now - last_log_ms > 1000) {
        log_count = 0;
    } else if (log_count >= MAX_LOG_PER_SECOND) {
        return;  /* Trop de logs */
    }
    last_log_ms = now;
    log_count++;

    DEBUG_VERBOSE(TAG, "[%s] %-20s P=%-8s T=%lu",
                  action,
                  AppEvent_GetTypeName(event->type),
                  AppEvent_GetPriorityName(event->priority),
                  event->timestamp_ms);
}

/* ======================================================================== */
/*              EXEMPLE D'INTÉGRATION AVEC LES ISR                          */
/* ======================================================================== */

#if 0  /* Exemple - Non compilé */

/**
 * @brief Exemple d'ISR qui poste un événement tactile
 */
void EXTI_Touch_IRQHandler(void)
{
    /* Acquitter l'interruption */
    HAL_GPIO_EXTI_IRQHandler(TOUCH_IRQ_PIN);

    /* Lire les coordonnées tactiles */
    uint16_t x, y;
    Touch_GetCoordinates(&x, &y);

    /* Créer l'événement */
    AppEvent_t event;
    AppEvent_CreateTouchEvent(&event, APP_EVENT_TOUCH_PRESS, x, y);

    /* Poster dans la file (thread-safe) */
    AppEventQueue_Post(&g_app.event_queue, &event);
}

/**
 * @brief Exemple d'ISR LoRa qui poste un événement paquet reçu
 */
void EXTI_LoRa_DIO0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(LORA_DIO0_PIN);

    /* Lire le paquet depuis le SX1278 */
    uint8_t buffer[256];
    uint16_t length;
    int16_t rssi;
    int8_t snr;

    if (LoRa_ReadPacket(buffer, &length, &rssi, &snr)) {
        AppEvent_t event;
        memset(&event, 0, sizeof(event));
        event.type = APP_EVENT_LORA_PACKET_RECEIVED;
        event.priority = APP_PRIORITY_HIGH;
        event.data.lora.data = buffer;
        event.data.lora.length = length;
        event.data.lora.rssi = rssi;
        event.data.lora.snr = snr;

        AppEventQueue_Post(&g_app.event_queue, &event);
    }
}

#endif

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */