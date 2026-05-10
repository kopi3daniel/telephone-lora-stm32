/**
 * @file    ring_buffer.cpp
 * @brief   Implémentation du buffer circulaire générique
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente un buffer circulaire (FIFO) thread-safe.
 * 
 * ALGORITHME DE WRITE :
 * 
 *   1. Calculer l'espace disponible
 *   2. Si OVERWRITE et pas assez d'espace :
 *      a. Écraser les données les plus anciennes
 *      b. Avancer head en conséquence
 *   3. Si BLOCKING et pas assez d'espace :
 *      a. Écrire ce qui peut l'être
 *      b. Retourner le nombre d'éléments écrits
 *   4. Si assez d'espace : écriture normale
 *   5. Gérer le wrap-around (tail < head quand buffer boucle)
 *   6. Mettre à jour les statistiques
 * 
 * ALGORITHME DE READ :
 * 
 *   1. Vérifier si buffer vide
 *   2. Calculer le nombre d'éléments lisibles
 *   3. Copier les données (gestion wrap-around)
 *   4. Avancer head
 *   5. Mettre à jour les statistiques
 * 
 * GESTION DU WRAP-AROUND :
 * 
 *   Le buffer est circulaire : quand tail atteint size, il revient à 0.
 *   Une écriture peut nécessiter DEUX copies si elle dépasse la fin :
 * 
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 *   │   │   │   │   │ A │ B │ C │ D │  ← Écrire E, F, G (3 éléments) à tail=5
 *   └───┴───┴───┴───┴───┴───┴───┴───┘
 *                                    ↑ tail=5, size=8
 * 
 *   Copie 1 : E, F à tail=5,6 (2 éléments)
 *   Copie 2 : G à tail=0 (1 élément, wrap-around)
 * 
 *   Résultat :
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 *   │ G │   │   │   │ A │ E │ F │ D │
 *   └───┴───┴───┴───┴───┴───┴───┴───┘
 *     ↑ tail=1
 * 
 * THREAD SAFETY :
 * 
 *   Le buffer est conçu pour un PRODUCTEUR et un CONSOMMATEUR uniques.
 *   Si l'ISR écrit (producteur) et la boucle principale lit (consommateur),
 *   le buffer est thread-safe sans mutex car :
 * 
 *   - L'ISR ne modifie QUE tail et count (incrémentation)
 *   - La boucle principale ne modifie QUE head et count (décrémentation)
 *   - Aucune variable n'est modifiée par les deux simultanément
 * 
 *   Pour PLUSIEURS producteurs ou consommateurs, ajouter un mutex.
 * 
 * OPTIMISATIONS :
 * 
 *   - Utilisation de memcpy pour les copies en bloc
 *   - Pas de division modulo si size est une puissance de 2 (utiliser &)
 *   - Accès DMA direct via GetReadPointer/GetWritePointer (zero-copy)
 *   - Macros rapides pour les buffers d'octets
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "ring_buffer.h"

/* HAL (pour __disable_irq si nécessaire) */
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
#define TAG                                 "RingBuffer"

/** Version optimisée du modulo si size est puissance de 2 */
#define MOD_POW2(index, size)               ((index) & ((size) - 1))

/** Section critique (désactiver interruptions) */
#define CRITICAL_ENTER()                    __disable_irq()
#define CRITICAL_EXIT()                     __enable_irq()

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

static inline bool is_power_of_two(size_t n);
static size_t min_size(size_t a, size_t b);
static void update_max_usage(RingBuffer_t* rb);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise un buffer circulaire
 */
RingBufferError_t RingBuffer_Init(RingBuffer_t* rb,
                                  void* buffer,
                                  size_t size,
                                  size_t element_size,
                                  RingBufferMode_t mode,
                                  const char* name)
{
    if (!rb || !buffer || size == 0 || element_size == 0) {
        return RING_BUFFER_ERROR_NULL;
    }

    /* Mise à zéro */
    memset(rb, 0, sizeof(RingBuffer_t));

    /* Configuration */
    rb->buffer = (uint8_t*)buffer;
    rb->size = size / element_size;  /* Taille en éléments */
    rb->element_size = element_size;
    rb->mode = mode;
    rb->is_initialized = true;

    /* Nom (pour debug) */
    if (name) {
        strncpy(rb->name, name, RING_BUFFER_MAX_NAME_LENGTH - 1);
        rb->name[RING_BUFFER_MAX_NAME_LENGTH - 1] = '\0';
    } else {
        snprintf(rb->name, sizeof(rb->name), "RB_%p", (void*)rb);
    }

    /* Index */
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;

    /* Stats */
    rb->total_writes = 0;
    rb->total_reads = 0;
    rb->total_overflows = 0;
    rb->total_underruns = 0;
    rb->max_usage = 0;

    /* Callback */
    rb->on_overflow = NULL;
    rb->overflow_context = NULL;

    /* Effacer le buffer */
    memset(rb->buffer, 0, size);

    DEBUG_VERBOSE(TAG, "Buffer '%s' initialisé: %u éléments de %u octets (mode=%s)",
                  rb->name, (unsigned int)rb->size, (unsigned int)element_size,
                  mode == RING_BUFFER_MODE_OVERWRITE ? "OVERWRITE" : "BLOCKING");

    return RING_BUFFER_OK;
}

/**
 * @brief Réinitialise le buffer
 */
void RingBuffer_Reset(RingBuffer_t* rb)
{
    if (!rb) return;

    CRITICAL_ENTER();

    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;

    /* Effacer le buffer (pas nécessaire mais plus propre) */
    memset(rb->buffer, 0, rb->size * rb->element_size);

    CRITICAL_EXIT();

    DEBUG_VERBOSE(TAG, "Buffer '%s' réinitialisé", rb->name);
}

/**
 * @brief Écrit des données dans le buffer
 */
size_t RingBuffer_Write(RingBuffer_t* rb, const void* data, size_t count)
{
    if (!rb || !rb->is_initialized || !data || count == 0) {
        return 0;
    }

    size_t written = 0;
    const uint8_t* src = (const uint8_t*)data;
    size_t bytes_to_write = count * rb->element_size;

    CRITICAL_ENTER();

    /* Espace libre */
    size_t free_elements = RingBuffer_GetFree(rb);
    size_t free_bytes = free_elements * rb->element_size;

    if (bytes_to_write > free_bytes) {
        if (rb->mode == RING_BUFFER_MODE_OVERWRITE) {
            /* Calculer combien d'octets écraser */
            size_t overflow_bytes = bytes_to_write - free_bytes;
            size_t overflow_elements = (overflow_bytes + rb->element_size - 1) / rb->element_size;

            /* Avancer head pour faire de la place */
            rb->head = (rb->head + overflow_elements) % rb->size;
            if (rb->count >= overflow_elements) {
                rb->count -= overflow_elements;
            } else {
                rb->count = 0;
            }

            rb->total_overflows++;
            written = count;  /* Tout sera écrit */

            if (rb->on_overflow) {
                rb->on_overflow(rb->overflow_context);
            }
        } else {
            /* Mode BLOCKING : écrire ce qui peut l'être */
            count = free_elements;
            bytes_to_write = count * rb->element_size;
            if (count == 0) {
                CRITICAL_EXIT();
                return 0;
            }
        }
    }

    /* Écriture avec gestion du wrap-around */
    size_t tail_byte = rb->tail * rb->element_size;
    size_t buffer_size_bytes = rb->size * rb->element_size;

    if (tail_byte + bytes_to_write <= buffer_size_bytes) {
        /* Pas de wrap-around */
        memcpy(rb->buffer + tail_byte, src, bytes_to_write);
    } else {
        /* Wrap-around : deux copies */
        size_t first_chunk = buffer_size_bytes - tail_byte;
        memcpy(rb->buffer + tail_byte, src, first_chunk);
        memcpy(rb->buffer, src + first_chunk, bytes_to_write - first_chunk);
    }

    /* Mettre à jour tail et count */
    rb->tail = (rb->tail + count) % rb->size;
    rb->count += count;

    /* Si pas encore écrit (mode normal) */
    if (written == 0) {
        written = count;
    }

    /* Stats */
    rb->total_writes++;
    update_max_usage(rb);

    CRITICAL_EXIT();

    return written;
}

/**
 * @brief Écrit un seul élément
 */
bool RingBuffer_WriteOne(RingBuffer_t* rb, const void* data)
{
    if (!rb || !data) return false;
    return RingBuffer_Write(rb, data, 1) == 1;
}

/**
 * @brief Écrit un octet
 */
bool RingBuffer_WriteByte(RingBuffer_t* rb, uint8_t byte)
{
    if (!rb || rb->element_size != 1) return false;
    return RingBuffer_Write(rb, &byte, 1) == 1;
}

/**
 * @brief Lit des données depuis le buffer
 */
size_t RingBuffer_Read(RingBuffer_t* rb, void* data, size_t count)
{
    if (!rb || !rb->is_initialized || !data || count == 0) {
        return 0;
    }

    CRITICAL_ENTER();

    /* Vérifier si vide */
    if (RingBuffer_IsEmpty(rb)) {
        rb->total_underruns++;
        CRITICAL_EXIT();
        return 0;
    }

    /* Limiter au nombre d'éléments disponibles */
    if (count > rb->count) {
        count = rb->count;
    }

    uint8_t* dst = (uint8_t*)data;
    size_t bytes_to_read = count * rb->element_size;
    size_t head_byte = rb->head * rb->element_size;
    size_t buffer_size_bytes = rb->size * rb->element_size;

    /* Lecture avec gestion wrap-around */
    if (head_byte + bytes_to_read <= buffer_size_bytes) {
        /* Pas de wrap-around */
        memcpy(dst, rb->buffer + head_byte, bytes_to_read);
    } else {
        /* Wrap-around : deux copies */
        size_t first_chunk = buffer_size_bytes - head_byte;
        memcpy(dst, rb->buffer + head_byte, first_chunk);
        memcpy(dst + first_chunk, rb->buffer, bytes_to_read - first_chunk);
    }

    /* Mettre à jour head et count */
    rb->head = (rb->head + count) % rb->size;
    rb->count -= count;

    /* Stats */
    rb->total_reads++;

    CRITICAL_EXIT();

    return count;
}

/**
 * @brief Lit un seul élément
 */
bool RingBuffer_ReadOne(RingBuffer_t* rb, void* data)
{
    if (!rb || !data) return false;
    return RingBuffer_Read(rb, data, 1) == 1;
}

/**
 * @brief Lit un octet
 */
bool RingBuffer_ReadByte(RingBuffer_t* rb, uint8_t* byte)
{
    if (!rb || !byte || rb->element_size != 1) return false;
    return RingBuffer_Read(rb, byte, 1) == 1;
}

/**
 * @brief Consulte sans retirer (peek)
 */
size_t RingBuffer_Peek(RingBuffer_t* rb, void* data, size_t count)
{
    if (!rb || !rb->is_initialized || !data || count == 0) {
        return 0;
    }

    CRITICAL_ENTER();

    if (RingBuffer_IsEmpty(rb)) {
        CRITICAL_EXIT();
        return 0;
    }

    /* Limiter */
    if (count > rb->count) {
        count = rb->count;
    }

    uint8_t* dst = (uint8_t*)data;
    size_t bytes_to_read = count * rb->element_size;
    size_t head_byte = rb->head * rb->element_size;
    size_t buffer_size_bytes = rb->size * rb->element_size;

    /* Copie sans modifier head */
    if (head_byte + bytes_to_read <= buffer_size_bytes) {
        memcpy(dst, rb->buffer + head_byte, bytes_to_read);
    } else {
        size_t first_chunk = buffer_size_bytes - head_byte;
        memcpy(dst, rb->buffer + head_byte, first_chunk);
        memcpy(dst + first_chunk, rb->buffer, bytes_to_read - first_chunk);
    }

    CRITICAL_EXIT();

    return count;
}

/**
 * @brief Consulte à une position donnée
 */
bool RingBuffer_PeekAt(RingBuffer_t* rb, size_t index, void* data)
{
    if (!rb || !data) return false;

    CRITICAL_ENTER();

    if (index >= rb->count) {
        CRITICAL_EXIT();
        return false;
    }

    size_t pos = (rb->head + index) % rb->size;
    size_t byte_pos = pos * rb->element_size;

    memcpy(data, rb->buffer + byte_pos, rb->element_size);

    CRITICAL_EXIT();

    return true;
}

/**
 * @brief Avance l'index de lecture sans copier
 */
size_t RingBuffer_Skip(RingBuffer_t* rb, size_t count)
{
    if (!rb) return 0;

    CRITICAL_ENTER();

    if (count > rb->count) {
        count = rb->count;
    }

    rb->head = (rb->head + count) % rb->size;
    rb->count -= count;
    rb->total_reads++;

    CRITICAL_EXIT();

    return count;
}

/**
 * @brief Vide complètement le buffer
 */
void RingBuffer_Flush(RingBuffer_t* rb)
{
    if (!rb) return;

    CRITICAL_ENTER();

    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;

    /* Effacer le buffer (optionnel) */
    memset(rb->buffer, 0, rb->size * rb->element_size);

    CRITICAL_EXIT();

    DEBUG_VERBOSE(TAG, "Buffer '%s' vidé", rb->name);
}

/**
 * @brief Vérifie si le buffer est vide
 */
bool RingBuffer_IsEmpty(RingBuffer_t* rb)
{
    if (!rb) return true;
    return rb->count == 0;
}

/**
 * @brief Vérifie si le buffer est plein
 */
bool RingBuffer_IsFull(RingBuffer_t* rb)
{
    if (!rb) return false;
    return rb->count >= rb->size;
}

/**
 * @brief Retourne le nombre d'éléments
 */
size_t RingBuffer_GetCount(RingBuffer_t* rb)
{
    if (!rb) return 0;
    return rb->count;
}

/**
 * @brief Retourne l'espace libre
 */
size_t RingBuffer_GetFree(RingBuffer_t* rb)
{
    if (!rb) return 0;
    return rb->size - rb->count;
}

/**
 * @brief Retourne la taille totale
 */
size_t RingBuffer_GetSize(RingBuffer_t* rb)
{
    if (!rb) return 0;
    return rb->size;
}

/**
 * @brief Retourne le taux d'utilisation
 */
float RingBuffer_GetUsagePercent(RingBuffer_t* rb)
{
    if (!rb || rb->size == 0) return 0.0f;
    return (float)rb->count / (float)rb->size * 100.0f;
}

/**
 * @brief Récupère les statistiques
 */
void RingBuffer_GetStats(RingBuffer_t* rb, RingBufferStats_t* stats)
{
    if (!rb || !stats) return;

    CRITICAL_ENTER();

    stats->total_writes = rb->total_writes;
    stats->total_reads = rb->total_reads;
    stats->total_overflows = rb->total_overflows;
    stats->total_underruns = rb->total_underruns;
    stats->max_usage = rb->max_usage;
    stats->current_usage = rb->count;
    stats->size = rb->size;
    stats->usage_percent = RingBuffer_GetUsagePercent(rb);

    CRITICAL_EXIT();
}

/**
 * @brief Réinitialise les statistiques
 */
void RingBuffer_ResetStats(RingBuffer_t* rb)
{
    if (!rb) return;

    CRITICAL_ENTER();

    rb->total_writes = 0;
    rb->total_reads = 0;
    rb->total_overflows = 0;
    rb->total_underruns = 0;
    rb->max_usage = 0;

    CRITICAL_EXIT();
}

/**
 * @brief Définit le callback de débordement
 */
void RingBuffer_SetOverflowCallback(RingBuffer_t* rb,
                                    void (*callback)(void* context),
                                    void* context)
{
    if (!rb) return;

    rb->on_overflow = callback;
    rb->overflow_context = context;
}

/**
 * @brief Pointeur direct vers la zone de lecture (zero-copy)
 */
const uint8_t* RingBuffer_GetReadPointer(RingBuffer_t* rb, size_t* available)
{
    if (!rb || !available) return NULL;

    CRITICAL_ENTER();

    if (rb->count == 0) {
        *available = 0;
        CRITICAL_EXIT();
        return NULL;
    }

    /* Calculer le nombre d'octets consécutifs disponibles */
    size_t head_byte = rb->head * rb->element_size;
    size_t tail_byte = rb->tail * rb->element_size;
    size_t buffer_size_bytes = rb->size * rb->element_size;

    if (tail_byte > head_byte) {
        /* Données contiguës de head à tail */
        *available = (tail_byte - head_byte) / rb->element_size;
    } else if (tail_byte < head_byte || rb->count == rb->size) {
        /* Données de head à la fin du buffer */
        *available = (buffer_size_bytes - head_byte) / rb->element_size;
    } else {
        /* Buffer vide */
        *available = 0;
        CRITICAL_EXIT();
        return NULL;
    }

    /* Limiter au nombre d'éléments dans le buffer */
    if (*available > rb->count) {
        *available = rb->count;
    }

    const uint8_t* ptr = rb->buffer + head_byte;

    CRITICAL_EXIT();

    return ptr;
}

/**
 * @brief Pointeur direct vers la zone d'écriture (zero-copy)
 */
uint8_t* RingBuffer_GetWritePointer(RingBuffer_t* rb, size_t* available)
{
    if (!rb || !available) return NULL;

    CRITICAL_ENTER();

    if (RingBuffer_IsFull(rb)) {
        if (rb->mode == RING_BUFFER_MODE_OVERWRITE) {
            /* Écraser les plus anciennes : avancer head */
            /* Pour simplifier, on retourne juste l'espace après tail */
            /* L'appelant devra gérer l'écrasement */
        } else {
            *available = 0;
            CRITICAL_EXIT();
            return NULL;
        }
    }

    size_t tail_byte = rb->tail * rb->element_size;
    size_t head_byte = rb->head * rb->element_size;
    size_t buffer_size_bytes = rb->size * rb->element_size;

    if (head_byte > tail_byte) {
        /* Espace de tail à head */
        *available = (head_byte - tail_byte) / rb->element_size;
    } else if (head_byte <= tail_byte && rb->count == 0) {
        /* Buffer vide : tout l'espace */
        *available = rb->size - rb->tail;
    } else {
        /* Espace de tail à la fin */
        *available = (buffer_size_bytes - tail_byte) / rb->element_size;
    }

    /* Limiter à l'espace libre */
    size_t free = RingBuffer_GetFree(rb);
    if (*available > free) {
        *available = free;
    }

    uint8_t* ptr = rb->buffer + tail_byte;

    CRITICAL_EXIT();

    return ptr;
}

/**
 * @brief Valide l'écriture après GetWritePointer
 */
void RingBuffer_CommitWrite(RingBuffer_t* rb, size_t count)
{
    if (!rb || count == 0) return;

    CRITICAL_ENTER();

    if (count > RingBuffer_GetFree(rb)) {
        /* Débordement : ajuster head */
        size_t overflow = count - RingBuffer_GetFree(rb);
        rb->head = (rb->head + overflow) % rb->size;
        if (rb->count >= overflow) {
            rb->count -= overflow;
        }
        rb->total_overflows++;
    }

    rb->tail = (rb->tail + count) % rb->size;
    rb->count += count;
    rb->total_writes++;
    update_max_usage(rb);

    CRITICAL_EXIT();
}

/**
 * @brief Valide la lecture après GetReadPointer
 */
void RingBuffer_CommitRead(RingBuffer_t* rb, size_t count)
{
    if (!rb || count == 0) return;

    CRITICAL_ENTER();

    if (count > rb->count) {
        count = rb->count;
    }

    rb->head = (rb->head + count) % rb->size;
    rb->count -= count;
    rb->total_reads++;

    CRITICAL_EXIT();
}

/**
 * @brief Imprime les statistiques
 */
void RingBuffer_PrintStats(RingBuffer_t* rb)
{
    if (!rb) return;

    RingBufferStats_t stats;
    RingBuffer_GetStats(rb, &stats);

    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  STATISTIQUES BUFFER: %s", rb->name);
    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  Taille     : %u éléments", (unsigned int)stats.size);
    DEBUG_INFO(TAG, "  Utilisation: %u/%u (%.1f%%)",
               (unsigned int)stats.current_usage,
               (unsigned int)stats.size,
               stats.usage_percent);
    DEBUG_INFO(TAG, "  Max usage  : %u", (unsigned int)stats.max_usage);
    DEBUG_INFO(TAG, "  Écritures  : %lu", stats.total_writes);
    DEBUG_INFO(TAG, "  Lectures   : %lu", stats.total_reads);
    DEBUG_INFO(TAG, "  Overflows  : %lu", stats.total_overflows);
    DEBUG_INFO(TAG, "  Underruns  : %lu", stats.total_underruns);
    DEBUG_INFO(TAG, "========================================");
}

/**
 * @brief Nom lisible d'un code d'erreur
 */
const char* RingBuffer_GetErrorName(RingBufferError_t error)
{
    switch (error) {
        case RING_BUFFER_OK:                    return "OK";
        case RING_BUFFER_ERROR_NULL:            return "NULL";
        case RING_BUFFER_ERROR_FULL:            return "FULL";
        case RING_BUFFER_ERROR_EMPTY:           return "EMPTY";
        case RING_BUFFER_ERROR_TOO_LARGE:       return "TOO_LARGE";
        case RING_BUFFER_ERROR_NOT_ENOUGH_DATA: return "NOT_ENOUGH_DATA";
        case RING_BUFFER_ERROR_OVERFLOW:        return "OVERFLOW";
        default:                                return "UNKNOWN";
    }
}

/* ======================================================================== */
/*              FONCTIONS PRIVÉES                                           */
/* ======================================================================== */

/**
 * @brief Vérifie si un nombre est une puissance de 2
 */
static inline bool is_power_of_two(size_t n)
{
    return (n != 0) && ((n & (n - 1)) == 0);
}

/**
 * @brief Minimum de deux size_t
 */
static size_t min_size(size_t a, size_t b)
{
    return (a < b) ? a : b;
}

/**
 * @brief Met à jour l'utilisation maximale
 */
static void update_max_usage(RingBuffer_t* rb)
{
    if (!rb) return;
    if (rb->count > rb->max_usage) {
        rb->max_usage = rb->count;
    }
}

/* ======================================================================== */
/*              EXEMPLE D'INTÉGRATION AVEC DMA                              */
/* ======================================================================== */

#if 0  /* Exemple - Non compilé */

/**
 * @brief Exemple : ISR DMA audio → Ring Buffer
 * 
 * Le DMA remplit la moitié du buffer, puis l'autre moitié.
 * L'ISR Half Complete et Complete poussent les données
 * dans le ring buffer.
 */

static RingBuffer_t audio_rx_rb;
static uint8_t audio_rx_storage[4096];
static uint8_t dma_buffer[512];

void Audio_DMA_HalfComplete_ISR(void)
{
    /* Première moitié du buffer DMA remplie */
    RingBuffer_Write(&audio_rx_rb, dma_buffer, 256);
}

void Audio_DMA_Complete_ISR(void)
{
    /* Deuxième moitié du buffer DMA remplie */
    RingBuffer_Write(&audio_rx_rb, dma_buffer + 256, 256);
}

void ProcessAudio(void)
{
    uint8_t chunk[128];
    size_t read = RingBuffer_Read(&audio_rx_rb, chunk, sizeof(chunk));
    if (read > 0) {
        /* Traiter les échantillons audio */
    }
}

#endif

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */