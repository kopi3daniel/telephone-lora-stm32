/**
 * @file    circular_queue.cpp
 * @brief   Implémentation de la file circulaire générique
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente une file FIFO thread-safe pour éléments de taille fixe.
 * 
 * ALGORITHME DE PUSH :
 * 
 *   1. Vérifier paramètres
 *   2. Si file pleine :
 *      a. Mode OVERWRITE : avancer head (écraser le plus ancien)
 *      b. Mode BLOCKING : retourner false
 *   3. Copier l'élément à la position tail
 *   4. Avancer tail (circulaire)
 *   5. Incrémenter count
 *   6. Mettre à jour statistiques
 * 
 * ALGORITHME DE POP :
 * 
 *   1. Vérifier paramètres
 *   2. Si file vide : retourner false
 *   3. Copier l'élément depuis la position head
 *   4. Avancer head (circulaire)
 *   5. Décrémenter count
 *   6. Mettre à jour statistiques
 * 
 * PARTICULARITÉS :
 * 
 * - PopLast / PeekLast : accès LIFO au dernier élément
 *   Utile pour "annuler" la dernière opération.
 * 
 * - Contains : recherche linéaire avec fonction de comparaison
 *   Utile pour éviter les doublons.
 * 
 * - ForEach : itération sans dépiler
 *   Utile pour l'affichage ou le traitement par lot.
 * 
 * - ToArray : export en tableau contigu
 *   Gère automatiquement le wrap-around.
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "circular_queue.h"

/* HAL */
#include "stm32f4xx_hal.h"

/* Utilitaires */
#include "debug_utils.h"

/* Standard */
#include <string.h>
#include <stdio.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs */
#define TAG                                 "CircularQueue"

/** Section critique */
#define CRITICAL_ENTER()                    __disable_irq()
#define CRITICAL_EXIT()                     __enable_irq()

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

static size_t get_byte_offset(CircularQueue_t* cq, size_t index);
static void update_max_count(CircularQueue_t* cq);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise une file circulaire
 */
CircularQueueError_t CircularQueue_Init(CircularQueue_t* cq,
                                         void* buffer,
                                         size_t capacity,
                                         size_t element_size,
                                         CircularQueueMode_t mode,
                                         const char* name)
{
    if (!cq || !buffer || capacity == 0 || element_size == 0) {
        return CQ_ERROR_NULL;
    }

    if (capacity > CQ_MAX_CAPACITY) {
        DEBUG_ERROR(TAG, "Capacité %u > max %u", (unsigned int)capacity, CQ_MAX_CAPACITY);
        return CQ_ERROR_SIZE_MISMATCH;
    }

    /* Mise à zéro */
    memset(cq, 0, sizeof(CircularQueue_t));

    /* Configuration */
    cq->buffer = (uint8_t*)buffer;
    cq->capacity = capacity;
    cq->element_size = element_size;
    cq->total_size_bytes = capacity * element_size;
    cq->mode = mode;
    cq->is_initialized = true;
    cq->overwrite_on_full = (mode == CQ_MODE_OVERWRITE);

    /* Nom */
    if (name) {
        strncpy(cq->name, name, CQ_MAX_NAME_LENGTH - 1);
        cq->name[CQ_MAX_NAME_LENGTH - 1] = '\0';
    } else {
        snprintf(cq->name, sizeof(cq->name), "CQ_%p", (void*)cq);
    }

    /* Index */
    cq->head = 0;
    cq->tail = 0;
    cq->count = 0;

    /* Stats */
    cq->total_pushes = 0;
    cq->total_pops = 0;
    cq->total_overflows = 0;
    cq->total_underruns = 0;
    cq->max_count = 0;

    /* Callback */
    cq->on_overflow = NULL;
    cq->overflow_context = NULL;

    /* Verrou */
    cq->locked = false;

    /* Effacer le buffer */
    memset(cq->buffer, 0, cq->total_size_bytes);

    DEBUG_VERBOSE(TAG, "File '%s' initialisée: %u x %u octets (mode=%s)",
                  cq->name,
                  (unsigned int)capacity,
                  (unsigned int)element_size,
                  mode == CQ_MODE_OVERWRITE ? "OVERWRITE" :
                  mode == CQ_MODE_BLOCKING ? "BLOCKING" : "EXPANDING");

    return CQ_OK;
}

/**
 * @brief Réinitialise la file
 */
void CircularQueue_Reset(CircularQueue_t* cq)
{
    if (!cq) return;

    CRITICAL_ENTER();

    cq->head = 0;
    cq->tail = 0;
    cq->count = 0;

    memset(cq->buffer, 0, cq->total_size_bytes);

    CRITICAL_EXIT();

    DEBUG_VERBOSE(TAG, "File '%s' réinitialisée", cq->name);
}

/* ---- Opérations d'écriture ---- */

/**
 * @brief Ajoute un élément
 */
bool CircularQueue_Push(CircularQueue_t* cq, const void* element)
{
    if (!cq || !cq->is_initialized || !element) {
        return false;
    }

    /* Vérifier verrou */
    if (cq->locked) {
        return false;
    }

    CRITICAL_ENTER();

    /* Vérifier si plein */
    if (cq->count >= cq->capacity) {
        if (cq->mode == CQ_MODE_OVERWRITE) {
            /* Écraser le plus ancien : avancer head */
            cq->head = (cq->head + 1) % cq->capacity;
            cq->count--;  /* Sera réincrémenté après */
            cq->total_overflows++;

            if (cq->on_overflow) {
                cq->on_overflow(cq->overflow_context);
            }
        } else {
            /* Mode BLOCKING : refuser */
            cq->total_overflows++;
            CRITICAL_EXIT();
            return false;
        }
    }

    /* Copier l'élément à la position tail */
    size_t offset = get_byte_offset(cq, cq->tail);
    memcpy(cq->buffer + offset, element, cq->element_size);

    /* Avancer tail */
    cq->tail = (cq->tail + 1) % cq->capacity;
    cq->count++;

    /* Stats */
    cq->total_pushes++;
    update_max_count(cq);

    CRITICAL_EXIT();

    return true;
}

/**
 * @brief Ajoute plusieurs éléments
 */
size_t CircularQueue_PushMultiple(CircularQueue_t* cq,
                                  const void* elements,
                                  size_t count)
{
    if (!cq || !elements || count == 0) return 0;

    size_t pushed = 0;
    const uint8_t* src = (const uint8_t*)elements;

    CRITICAL_ENTER();

    for (size_t i = 0; i < count; i++) {
        if (cq->count >= cq->capacity) {
            if (cq->mode == CQ_MODE_OVERWRITE) {
                cq->head = (cq->head + 1) % cq->capacity;
                cq->count--;
                cq->total_overflows++;
            } else {
                break;  /* Plus de place */
            }
        }

        size_t offset = get_byte_offset(cq, cq->tail);
        memcpy(cq->buffer + offset, src + i * cq->element_size, cq->element_size);

        cq->tail = (cq->tail + 1) % cq->capacity;
        cq->count++;
        pushed++;
    }

    cq->total_pushes++;
    update_max_count(cq);

    CRITICAL_EXIT();

    return pushed;
}

/**
 * @brief Écrit à une position absolue
 */
bool CircularQueue_PushAt(CircularQueue_t* cq, size_t index, const void* element)
{
    if (!cq || !element || index >= cq->capacity) return false;

    CRITICAL_ENTER();

    size_t offset = get_byte_offset(cq, index);
    memcpy(cq->buffer + offset, element, cq->element_size);

    CRITICAL_EXIT();

    return true;
}

/* ---- Opérations de lecture ---- */

/**
 * @brief Retire le prochain élément
 */
bool CircularQueue_Pop(CircularQueue_t* cq, void* element)
{
    if (!cq || !cq->is_initialized || !element) {
        return false;
    }

    CRITICAL_ENTER();

    if (cq->count == 0) {
        cq->total_underruns++;
        CRITICAL_EXIT();
        return false;
    }

    /* Copier depuis head */
    size_t offset = get_byte_offset(cq, cq->head);
    memcpy(element, cq->buffer + offset, cq->element_size);

    /* Effacer l'emplacement (optionnel, pour sécurité) */
    memset(cq->buffer + offset, 0, cq->element_size);

    /* Avancer head */
    cq->head = (cq->head + 1) % cq->capacity;
    cq->count--;

    /* Stats */
    cq->total_pops++;

    CRITICAL_EXIT();

    return true;
}

/**
 * @brief Retire plusieurs éléments
 */
size_t CircularQueue_PopMultiple(CircularQueue_t* cq,
                                 void* elements,
                                 size_t count)
{
    if (!cq || !elements || count == 0) return 0;

    uint8_t* dst = (uint8_t*)elements;
    size_t popped = 0;

    CRITICAL_ENTER();

    while (popped < count && cq->count > 0) {
        size_t offset = get_byte_offset(cq, cq->head);
        memcpy(dst + popped * cq->element_size, cq->buffer + offset, cq->element_size);
        memset(cq->buffer + offset, 0, cq->element_size);

        cq->head = (cq->head + 1) % cq->capacity;
        cq->count--;
        popped++;
    }

    if (popped > 0) {
        cq->total_pops++;
    }

    if (popped < count) {
        cq->total_underruns++;
    }

    CRITICAL_EXIT();

    return popped;
}

/**
 * @brief Consulte sans retirer
 */
bool CircularQueue_Peek(CircularQueue_t* cq, void* element)
{
    if (!cq || !cq->is_initialized || !element) {
        return false;
    }

    CRITICAL_ENTER();

    if (cq->count == 0) {
        CRITICAL_EXIT();
        return false;
    }

    size_t offset = get_byte_offset(cq, cq->head);
    memcpy(element, cq->buffer + offset, cq->element_size);

    CRITICAL_EXIT();

    return true;
}

/**
 * @brief Consulte à une position relative
 */
bool CircularQueue_PeekAt(CircularQueue_t* cq, size_t offset, void* element)
{
    if (!cq || !element) return false;

    CRITICAL_ENTER();

    if (offset >= cq->count) {
        CRITICAL_EXIT();
        return false;
    }

    size_t index = (cq->head + offset) % cq->capacity;
    size_t byte_offset = get_byte_offset(cq, index);
    memcpy(element, cq->buffer + byte_offset, cq->element_size);

    CRITICAL_EXIT();

    return true;
}

/**
 * @brief Retire sans copier
 */
bool CircularQueue_Skip(CircularQueue_t* cq)
{
    if (!cq) return false;

    CRITICAL_ENTER();

    if (cq->count == 0) {
        CRITICAL_EXIT();
        return false;
    }

    /* Effacer et avancer */
    size_t offset = get_byte_offset(cq, cq->head);
    memset(cq->buffer + offset, 0, cq->element_size);
    cq->head = (cq->head + 1) % cq->capacity;
    cq->count--;
    cq->total_pops++;

    CRITICAL_EXIT();

    return true;
}

/* ---- Opérations LIFO ---- */

/**
 * @brief Consulte le dernier élément (LIFO)
 */
bool CircularQueue_PeekLast(CircularQueue_t* cq, void* element)
{
    if (!cq || !element) return false;

    CRITICAL_ENTER();

    if (cq->count == 0) {
        CRITICAL_EXIT();
        return false;
    }

    /* Le dernier élément est juste avant tail */
    size_t last_index = (cq->tail == 0) ? cq->capacity - 1 : cq->tail - 1;
    size_t offset = get_byte_offset(cq, last_index);
    memcpy(element, cq->buffer + offset, cq->element_size);

    CRITICAL_EXIT();

    return true;
}

/**
 * @brief Retire le dernier élément (LIFO)
 */
bool CircularQueue_PopLast(CircularQueue_t* cq, void* element)
{
    if (!cq || !element) return false;

    CRITICAL_ENTER();

    if (cq->count == 0) {
        CRITICAL_EXIT();
        return false;
    }

    /* Reculer tail */
    if (cq->tail == 0) {
        cq->tail = cq->capacity - 1;
    } else {
        cq->tail--;
    }

    size_t offset = get_byte_offset(cq, cq->tail);
    memcpy(element, cq->buffer + offset, cq->element_size);
    memset(cq->buffer + offset, 0, cq->element_size);

    cq->count--;
    cq->total_pops++;

    CRITICAL_EXIT();

    return true;
}

/* ---- État ---- */

bool CircularQueue_IsEmpty(CircularQueue_t* cq)
{
    if (!cq) return true;
    return cq->count == 0;
}

bool CircularQueue_IsFull(CircularQueue_t* cq)
{
    if (!cq) return false;
    return cq->count >= cq->capacity;
}

size_t CircularQueue_GetCount(CircularQueue_t* cq)
{
    if (!cq) return 0;
    return cq->count;
}

size_t CircularQueue_GetFree(CircularQueue_t* cq)
{
    if (!cq) return 0;
    return cq->capacity - cq->count;
}

size_t CircularQueue_GetCapacity(CircularQueue_t* cq)
{
    if (!cq) return 0;
    return cq->capacity;
}

/**
 * @brief Recherche un élément
 */
bool CircularQueue_Contains(CircularQueue_t* cq,
                            const void* element,
                            int (*compare_fn)(const void* a, const void* b))
{
    if (!cq || !element) return false;

    CRITICAL_ENTER();

    if (cq->count == 0) {
        CRITICAL_EXIT();
        return false;
    }

    bool found = false;

    for (size_t i = 0; i < cq->count; i++) {
        size_t index = (cq->head + i) % cq->capacity;
        size_t offset = get_byte_offset(cq, index);

        int result;
        if (compare_fn) {
            result = compare_fn(element, cq->buffer + offset);
        } else {
            result = memcmp(element, cq->buffer + offset, cq->element_size);
        }

        if (result == 0) {
            found = true;
            break;
        }
    }

    CRITICAL_EXIT();

    return found;
}

/* ---- Statistiques ---- */

void CircularQueue_GetStats(CircularQueue_t* cq, CircularQueueStats_t* stats)
{
    if (!cq || !stats) return;

    CRITICAL_ENTER();

    stats->total_pushes = cq->total_pushes;
    stats->total_pops = cq->total_pops;
    stats->total_overflows = cq->total_overflows;
    stats->total_underruns = cq->total_underruns;
    stats->max_count = cq->max_count;
    stats->current_count = cq->count;
    stats->capacity = cq->capacity;
    stats->usage_percent = (cq->capacity > 0) ?
                           (float)cq->count / (float)cq->capacity * 100.0f : 0.0f;

    CRITICAL_EXIT();
}

void CircularQueue_ResetStats(CircularQueue_t* cq)
{
    if (!cq) return;

    CRITICAL_ENTER();

    cq->total_pushes = 0;
    cq->total_pops = 0;
    cq->total_overflows = 0;
    cq->total_underruns = 0;
    cq->max_count = 0;

    CRITICAL_EXIT();
}

void CircularQueue_PrintStats(CircularQueue_t* cq)
{
    if (!cq) return;

    CircularQueueStats_t stats;
    CircularQueue_GetStats(cq, &stats);

    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  STATS FILE: %s", cq->name);
    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  Capacité   : %u éléments", (unsigned int)stats.capacity);
    DEBUG_INFO(TAG, "  Éléments   : %u/%u (%.1f%%)",
               (unsigned int)stats.current_count,
               (unsigned int)stats.capacity,
               stats.usage_percent);
    DEBUG_INFO(TAG, "  Max        : %u", (unsigned int)stats.max_count);
    DEBUG_INFO(TAG, "  Pushes     : %lu", stats.total_pushes);
    DEBUG_INFO(TAG, "  Pops       : %lu", stats.total_pops);
    DEBUG_INFO(TAG, "  Overflows  : %lu", stats.total_overflows);
    DEBUG_INFO(TAG, "  Underruns  : %lu", stats.total_underruns);
    DEBUG_INFO(TAG, "========================================");
}

/* ---- Configuration ---- */

void CircularQueue_SetOverflowCallback(CircularQueue_t* cq,
                                       void (*callback)(void* context),
                                       void* context)
{
    if (!cq) return;
    cq->on_overflow = callback;
    cq->overflow_context = context;
}

void CircularQueue_Lock(CircularQueue_t* cq)
{
    if (!cq) return;
    cq->locked = true;
}

void CircularQueue_Unlock(CircularQueue_t* cq)
{
    if (!cq) return;
    cq->locked = false;
}

/* ---- Itération ---- */

size_t CircularQueue_ForEach(CircularQueue_t* cq,
                             CircularQueueIterator_t iterator,
                             void* context)
{
    if (!cq || !iterator) return 0;

    CRITICAL_ENTER();

    size_t count = cq->count;
    size_t iterated = 0;

    for (size_t i = 0; i < count; i++) {
        size_t index = (cq->head + i) % cq->capacity;
        size_t offset = get_byte_offset(cq, index);

        if (!iterator(cq->buffer + offset, i, context)) {
            break;  /* L'itérateur a demandé l'arrêt */
        }
        iterated++;
    }

    CRITICAL_EXIT();

    return iterated;
}

/* ---- Conversion ---- */

size_t CircularQueue_ToArray(CircularQueue_t* cq,
                             void* array,
                             size_t max_count)
{
    if (!cq || !array || max_count == 0) return 0;

    CRITICAL_ENTER();

    size_t count = cq->count;
    if (count > max_count) {
        count = max_count;
    }

    uint8_t* dst = (uint8_t*)array;

    for (size_t i = 0; i < count; i++) {
        size_t index = (cq->head + i) % cq->capacity;
        size_t offset = get_byte_offset(cq, index);
        memcpy(dst + i * cq->element_size, cq->buffer + offset, cq->element_size);
    }

    CRITICAL_EXIT();

    return count;
}

/* ---- Utilitaires ---- */

const char* CircularQueue_GetErrorName(CircularQueueError_t error)
{
    switch (error) {
        case CQ_OK:                     return "OK";
        case CQ_ERROR_NULL:             return "NULL";
        case CQ_ERROR_FULL:             return "FULL";
        case CQ_ERROR_EMPTY:            return "EMPTY";
        case CQ_ERROR_NOT_INITIALIZED:  return "NOT_INIT";
        case CQ_ERROR_SIZE_MISMATCH:    return "SIZE_MISMATCH";
        default:                        return "UNKNOWN";
    }
}

bool CircularQueue_CheckIntegrity(CircularQueue_t* cq)
{
    if (!cq) return false;
    if (!cq->is_initialized) return false;
    if (!cq->buffer) return false;
    if (cq->capacity == 0 || cq->element_size == 0) return false;
    if (cq->count > cq->capacity) return false;
    if (cq->head >= cq->capacity) return false;
    if (cq->tail >= cq->capacity) return false;

    return true;
}

/* ======================================================================== */
/*              FONCTIONS PRIVÉES                                           */
/* ======================================================================== */

/**
 * @brief Calcule l'offset en octets pour un index
 */
static size_t get_byte_offset(CircularQueue_t* cq, size_t index)
{
    return index * cq->element_size;
}

/**
 * @brief Met à jour le compteur d'utilisation maximale
 */
static void update_max_count(CircularQueue_t* cq)
{
    if (!cq) return;
    if (cq->count > cq->max_count) {
        cq->max_count = cq->count;
    }
}

/* ======================================================================== */
/*              EXEMPLES D'UTILISATION                                      */
/* ======================================================================== */

#if 0  /* Exemples - Non compilés */

/* --- Exemple 1 : File d'événements --- */

typedef struct {
    uint32_t id;
    uint32_t timestamp;
    uint8_t  data[16];
} Event_t;

CIRCULAR_QUEUE_DECLARE(event_queue, Event_t, 32);

void producer(void) {
    Event_t evt = { .id = 1, .timestamp = HAL_GetTick() };
    CQ_PUSH(&event_queue, &evt);
}

void consumer(void) {
    Event_t evt;
    if (CQ_POP(&event_queue, &evt)) {
        printf("Event: %lu\n", evt.id);
    }
}

/* --- Exemple 2 : File de commandes --- */

typedef struct {
    char cmd[16];
    int  value;
} Command_t;

CIRCULAR_QUEUE_DECLARE(cmd_queue, Command_t, 16);

/* --- Exemple 3 : Affichage sans vider --- */

bool print_event(void* element, size_t index, void* context) {
    Event_t* evt = (Event_t*)element;
    printf("[%u] Event %lu\n", (unsigned int)index, evt->id);
    return true;  /* Continuer */
}

void display_all(void) {
    CircularQueue_ForEach(&event_queue, print_event, NULL);
}

/* --- Exemple 4 : Export tableau --- */

void export_to_array(void) {
    Event_t buffer[32];
    size_t count = CircularQueue_ToArray(&event_queue, buffer, 32);
    /* buffer contient maintenant 'count' événements dans l'ordre FIFO */
}

#endif

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */