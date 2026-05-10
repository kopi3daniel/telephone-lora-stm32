/**
 * @file    fifo_buffer.cpp
 * @brief   Implémentation du buffer FIFO minimaliste
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente un buffer FIFO optimisé pour l'audio et les flux DMA.
 * 
 * FONCTIONNEMENT :
 * 
 *   Le buffer utilise un masque (mask = capacity - 1) au lieu du modulo (%)
 *   pour des performances maximales. Cela nécessite que capacity soit
 *   une puissance de 2.
 * 
 *   Si capacity n'est pas une puissance de 2, le buffer utilise
 *   l'opération modulo classique (plus lente).
 * 
 * ALGORITHME AVEC MASQUE (puissance de 2) :
 * 
 *   head = (head + 1) & mask;  // 1 cycle CPU
 * 
 * ALGORITHME SANS MASQUE (fallback) :
 * 
 *   head = (head + 1) % capacity;  // ~20 cycles CPU
 * 
 * GESTION DU WRAP-AROUND POUR ZERO-COPY :
 * 
 *   GetReadPtr retourne un pointeur vers les données contiguës
 *   à partir de head. Si les données "tournent" autour du buffer,
 *   seul le premier segment contigu est retourné.
 * 
 *   Exemple (capacity=8, head=5, tail=2, count=5) :
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 *   │ B │ C │   │   │   │ F │ G │ H │
 *   └───┴───┴───┴───┴───┴───┴───┴───┘
 *     ↑ tail=2               ↑ head=5
 *   
 *   GetReadPtr → &buffer[5], available=3  (F, G, H)
 *   Après CommitRead(3) → head=0
 *   GetReadPtr → &buffer[0], available=2  (B, C)
 * 
 * WAIT FOR DATA/SPACE :
 * 
 *   Fonctions bloquantes pour simplifier le code consommateur/producteur.
 *   Utilisent une boucle d'attente active avec timeout.
 *   À utiliser uniquement hors ISR.
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "fifo_buffer.h"

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
#define TAG                                 "FIFO"

/** Incrémente head ou tail avec masque ou modulo */
#define INCREMENT(index, mask, cap, use_mask) \
    do { \
        if (use_mask) { \
            (index) = ((index) + 1) & (mask); \
        } else { \
            (index) = ((index) + 1) % (cap); \
        } \
    } while(0)

/** Ajoute une valeur à un index avec wrap */
#define ADD_INDEX(index, value, mask, cap, use_mask) \
    do { \
        if (use_mask) { \
            (index) = ((index) + (value)) & (mask); \
        } else { \
            (index) = ((index) + (value)) % (cap); \
        } \
    } while(0)

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

static inline uint16_t min_u16(uint16_t a, uint16_t b);
static inline bool is_power_of_two(uint16_t n);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise un buffer FIFO
 */
bool FIFO_Init(FIFOBuffer_t* fifo, uint8_t* buffer, uint16_t capacity, const char* name)
{
    if (!fifo || !buffer || capacity == 0) {
        return false;
    }

    if (capacity > FIFO_MAX_CAPACITY) {
        DEBUG_ERROR(TAG, "Capacité %u > max %u", capacity, FIFO_MAX_CAPACITY);
        return false;
    }

    /* Mise à zéro */
    memset(fifo, 0, sizeof(FIFOBuffer_t));

    /* Configuration */
    fifo->buffer = buffer;
    fifo->capacity = capacity;
    fifo->is_power_of_two = is_power_of_two(capacity);
    
    if (fifo->is_power_of_two) {
        fifo->mask = capacity - 1;
    } else {
        fifo->mask = 0;
        DEBUG_WARN(TAG, "Capacité %u n'est pas une puissance de 2 (perf dégradée)", capacity);
    }

    /* Index */
    fifo->head = 0;
    fifo->tail = 0;
    fifo->count = 0;

    /* Nom */
    if (name) {
        strncpy(fifo->name, name, FIFO_MAX_NAME_LENGTH - 1);
        fifo->name[FIFO_MAX_NAME_LENGTH - 1] = '\0';
    } else {
        snprintf(fifo->name, sizeof(fifo->name), "FIFO_%p", (void*)fifo);
    }

    fifo->is_initialized = true;

    /* Effacer */
    memset(fifo->buffer, 0, capacity);

    DEBUG_VERBOSE(TAG, "Buffer '%s' initialisé: %u octets (pow2=%s)",
                  fifo->name, capacity, fifo->is_power_of_two ? "oui" : "non");

    return true;
}

/**
 * @brief Réinitialise le buffer
 */
void FIFO_Reset(FIFOBuffer_t* fifo)
{
    if (!fifo) return;

    fifo->head = 0;
    fifo->tail = 0;
    fifo->count = 0;
    
    memset(fifo->buffer, 0, fifo->capacity);
    
    DEBUG_VERBOSE(TAG, "Buffer '%s' réinitialisé", fifo->name);
}

/* ---- Opérations d'écriture ---- */

/**
 * @brief Écrit un octet
 */
bool FIFO_WriteByte(FIFOBuffer_t* fifo, uint8_t byte)
{
    if (!fifo || !fifo->is_initialized) return false;
    
    if (fifo->count >= fifo->capacity) {
        return false;  /* Plein */
    }
    
    fifo->buffer[fifo->tail] = byte;
    INCREMENT(fifo->tail, fifo->mask, fifo->capacity, fifo->is_power_of_two);
    fifo->count++;
    
    return true;
}

/**
 * @brief Écrit plusieurs octets
 */
uint16_t FIFO_Write(FIFOBuffer_t* fifo, const uint8_t* data, uint16_t length)
{
    if (!fifo || !fifo->is_initialized || !data || length == 0) {
        return 0;
    }
    
    /* Limiter à l'espace disponible */
    uint16_t free = FIFO_GetFree(fifo);
    if (length > free) {
        length = free;
    }
    
    if (length == 0) return 0;
    
    uint16_t written = 0;
    
    /* Première partie : de tail à la fin du buffer */
    uint16_t first_chunk = fifo->capacity - fifo->tail;
    if (first_chunk > length) first_chunk = length;
    
    memcpy(fifo->buffer + fifo->tail, data, first_chunk);
    written += first_chunk;
    
    /* Deuxième partie : du début du buffer (wrap-around) */
    if (written < length) {
        uint16_t second_chunk = length - written;
        memcpy(fifo->buffer, data + written, second_chunk);
        written += second_chunk;
    }
    
    /* Mettre à jour tail */
    ADD_INDEX(fifo->tail, written, fifo->mask, fifo->capacity, fifo->is_power_of_two);
    fifo->count += written;
    
    return written;
}

/* ---- Opérations de lecture ---- */

/**
 * @brief Lit un octet
 */
bool FIFO_ReadByte(FIFOBuffer_t* fifo, uint8_t* byte)
{
    if (!fifo || !fifo->is_initialized || !byte) return false;
    
    if (fifo->count == 0) {
        return false;  /* Vide */
    }
    
    *byte = fifo->buffer[fifo->head];
    INCREMENT(fifo->head, fifo->mask, fifo->capacity, fifo->is_power_of_two);
    fifo->count--;
    
    return true;
}

/**
 * @brief Lit plusieurs octets
 */
uint16_t FIFO_Read(FIFOBuffer_t* fifo, uint8_t* data, uint16_t length)
{
    if (!fifo || !fifo->is_initialized || !data || length == 0) {
        return 0;
    }
    
    /* Limiter au nombre d'octets disponibles */
    if (length > fifo->count) {
        length = fifo->count;
    }
    
    if (length == 0) return 0;
    
    uint16_t read = 0;
    
    /* Première partie : de head à la fin du buffer */
    uint16_t first_chunk = fifo->capacity - fifo->head;
    if (first_chunk > length) first_chunk = length;
    
    memcpy(data, fifo->buffer + fifo->head, first_chunk);
    read += first_chunk;
    
    /* Deuxième partie : du début du buffer (wrap-around) */
    if (read < length) {
        uint16_t second_chunk = length - read;
        memcpy(data + read, fifo->buffer, second_chunk);
        read += second_chunk;
    }
    
    /* Mettre à jour head */
    ADD_INDEX(fifo->head, read, fifo->mask, fifo->capacity, fifo->is_power_of_two);
    fifo->count -= read;
    
    return read;
}

/**
 * @brief Consulte sans retirer
 */
uint16_t FIFO_Peek(FIFOBuffer_t* fifo, uint8_t* data, uint16_t length)
{
    if (!fifo || !fifo->is_initialized || !data || length == 0) {
        return 0;
    }
    
    if (length > fifo->count) {
        length = fifo->count;
    }
    
    if (length == 0) return 0;
    
    uint16_t peeked = 0;
    
    uint16_t first_chunk = fifo->capacity - fifo->head;
    if (first_chunk > length) first_chunk = length;
    
    memcpy(data, fifo->buffer + fifo->head, first_chunk);
    peeked += first_chunk;
    
    if (peeked < length) {
        uint16_t second_chunk = length - peeked;
        memcpy(data + peeked, fifo->buffer, second_chunk);
        peeked += second_chunk;
    }
    
    /* Ne pas modifier head ni count */
    
    return peeked;
}

/**
 * @brief Consulte un octet à une position relative
 */
uint8_t FIFO_PeekAt(FIFOBuffer_t* fifo, uint16_t offset)
{
    if (!fifo || offset >= fifo->count) {
        return 0;
    }
    
    uint16_t index;
    if (fifo->is_power_of_two) {
        index = (fifo->head + offset) & fifo->mask;
    } else {
        index = (fifo->head + offset) % fifo->capacity;
    }
    
    return fifo->buffer[index];
}

/**
 * @brief Avance le pointeur de lecture
 */
uint16_t FIFO_Skip(FIFOBuffer_t* fifo, uint16_t count)
{
    if (!fifo) return 0;
    
    if (count > fifo->count) {
        count = fifo->count;
    }
    
    ADD_INDEX(fifo->head, count, fifo->mask, fifo->capacity, fifo->is_power_of_two);
    fifo->count -= count;
    
    return count;
}

/**
 * @brief Vide le buffer
 */
void FIFO_Flush(FIFOBuffer_t* fifo)
{
    if (!fifo) return;
    
    fifo->head = 0;
    fifo->tail = 0;
    fifo->count = 0;
    
    memset(fifo->buffer, 0, fifo->capacity);
    
    DEBUG_VERBOSE(TAG, "Buffer '%s' vidé", fifo->name);
}

/* ---- État ---- */

bool FIFO_IsEmpty(FIFOBuffer_t* fifo)
{
    return fifo ? fifo->count == 0 : true;
}

bool FIFO_IsFull(FIFOBuffer_t* fifo)
{
    return fifo ? fifo->count >= fifo->capacity : false;
}

uint16_t FIFO_GetCount(FIFOBuffer_t* fifo)
{
    return fifo ? fifo->count : 0;
}

uint16_t FIFO_GetFree(FIFOBuffer_t* fifo)
{
    return fifo ? fifo->capacity - fifo->count : 0;
}

uint16_t FIFO_GetCapacity(FIFOBuffer_t* fifo)
{
    return fifo ? fifo->capacity : 0;
}

uint8_t FIFO_GetUsagePercent(FIFOBuffer_t* fifo)
{
    if (!fifo || fifo->capacity == 0) return 0;
    return (uint8_t)((fifo->count * 100) / fifo->capacity);
}

/* ---- Zero-copy DMA ---- */

/**
 * @brief Pointeur direct vers la zone de lecture
 */
uint8_t* FIFO_GetReadPtr(FIFOBuffer_t* fifo, uint16_t* available)
{
    if (!fifo || !available) return NULL;
    
    if (fifo->count == 0) {
        *available = 0;
        return NULL;
    }
    
    /* Calculer les octets consécutifs disponibles */
    uint16_t contiguous;
    
    if (fifo->tail > fifo->head) {
        /* Données de head à tail (contiguës) */
        contiguous = fifo->tail - fifo->head;
    } else if (fifo->tail < fifo->head || fifo->count == fifo->capacity) {
        /* Données de head à la fin du buffer */
        contiguous = fifo->capacity - fifo->head;
    } else {
        /* Buffer vide */
        *available = 0;
        return NULL;
    }
    
    /* Limiter au nombre réel d'octets */
    if (contiguous > fifo->count) {
        contiguous = fifo->count;
    }
    
    *available = contiguous;
    return fifo->buffer + fifo->head;
}

/**
 * @brief Pointeur direct vers la zone d'écriture
 */
uint8_t* FIFO_GetWritePtr(FIFOBuffer_t* fifo, uint16_t* available)
{
    if (!fifo || !available) return NULL;
    
    if (fifo->count >= fifo->capacity) {
        *available = 0;
        return NULL;
    }
    
    /* Calculer l'espace contigu disponible */
    uint16_t contiguous;
    
    if (fifo->head > fifo->tail) {
        /* Espace de tail à head */
        contiguous = fifo->head - fifo->tail;
    } else if (fifo->head <= fifo->tail && fifo->count == 0) {
        /* Buffer vide : tout l'espace */
        contiguous = fifo->capacity - fifo->tail;
    } else {
        /* Espace de tail à la fin */
        contiguous = fifo->capacity - fifo->tail;
    }
    
    /* Limiter à l'espace libre */
    uint16_t free = FIFO_GetFree(fifo);
    if (contiguous > free) {
        contiguous = free;
    }
    
    *available = contiguous;
    return fifo->buffer + fifo->tail;
}

/**
 * @brief Valide les octets écrits
 */
void FIFO_CommitWrite(FIFOBuffer_t* fifo, uint16_t count)
{
    if (!fifo || count == 0) return;
    
    if (count > FIFO_GetFree(fifo)) {
        /* Écrasement : ajuster head */
        uint16_t overflow = count - FIFO_GetFree(fifo);
        ADD_INDEX(fifo->head, overflow, fifo->mask, fifo->capacity, fifo->is_power_of_two);
        if (fifo->count >= overflow) {
            fifo->count -= overflow;
        }
    }
    
    ADD_INDEX(fifo->tail, count, fifo->mask, fifo->capacity, fifo->is_power_of_two);
    fifo->count += count;
}

/**
 * @brief Valide les octets lus
 */
void FIFO_CommitRead(FIFOBuffer_t* fifo, uint16_t count)
{
    if (!fifo || count == 0) return;
    
    if (count > fifo->count) {
        count = fifo->count;
    }
    
    ADD_INDEX(fifo->head, count, fifo->mask, fifo->capacity, fifo->is_power_of_two);
    fifo->count -= count;
}

/* ---- Opérations spéciales ---- */

/**
 * @brief Écriture forcée (écrase si nécessaire)
 */
uint16_t FIFO_WriteForce(FIFOBuffer_t* fifo, const uint8_t* data, uint16_t length)
{
    if (!fifo || !data || length == 0) return 0;
    
    /* Si les données sont plus grandes que la capacité, n'écrire que la fin */
    if (length > fifo->capacity) {
        data += length - fifo->capacity;
        length = fifo->capacity;
    }
    
    /* Calculer combien d'octets vont déborder */
    uint16_t free = FIFO_GetFree(fifo);
    
    if (length > free) {
        /* Avancer head pour faire de la place */
        uint16_t overflow = length - free;
        ADD_INDEX(fifo->head, overflow, fifo->mask, fifo->capacity, fifo->is_power_of_two);
        if (fifo->count >= overflow) {
            fifo->count -= overflow;
        }
    }
    
    /* Écrire normalement */
    return FIFO_Write(fifo, data, length);
}

/**
 * @brief Attend que le buffer ait assez de données
 */
uint16_t FIFO_WaitForData(FIFOBuffer_t* fifo, uint16_t min_count, uint32_t timeout_ms)
{
    if (!fifo) return 0;
    
    uint32_t start = HAL_GetTick();
    
    while (fifo->count < min_count) {
        if (timeout_ms > 0) {
            uint32_t elapsed = HAL_GetTick() - start;
            if (elapsed >= timeout_ms) {
                break;  /* Timeout */
            }
        }
        /* Yield pour ne pas saturer le CPU */
        __NOP();
    }
    
    return fifo->count;
}

/**
 * @brief Attend que le buffer ait de l'espace
 */
uint16_t FIFO_WaitForSpace(FIFOBuffer_t* fifo, uint16_t min_free, uint32_t timeout_ms)
{
    if (!fifo) return 0;
    
    uint32_t start = HAL_GetTick();
    uint16_t free;
    
    while ((free = FIFO_GetFree(fifo)) < min_free) {
        if (timeout_ms > 0) {
            uint32_t elapsed = HAL_GetTick() - start;
            if (elapsed >= timeout_ms) {
                break;  /* Timeout */
            }
        }
        __NOP();
    }
    
    return FIFO_GetFree(fifo);
}

/* ---- Diagnostic ---- */

/**
 * @brief Vérifie l'intégrité
 */
bool FIFO_CheckIntegrity(FIFOBuffer_t* fifo)
{
    if (!fifo) return false;
    if (!fifo->is_initialized) return false;
    if (!fifo->buffer) return false;
    if (fifo->capacity == 0) return false;
    if (fifo->count > fifo->capacity) return false;
    if (fifo->head >= fifo->capacity) return false;
    if (fifo->tail >= fifo->capacity) return false;
    
    return true;
}

/**
 * @brief Affiche les informations
 */
void FIFO_PrintInfo(FIFOBuffer_t* fifo)
{
    if (!fifo) return;
    
    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  BUFFER FIFO: %s", fifo->name);
    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  Capacité   : %u octets", fifo->capacity);
    DEBUG_INFO(TAG, "  Occupé     : %u/%u (%u%%)",
               fifo->count, fifo->capacity, FIFO_GetUsagePercent(fifo));
    DEBUG_INFO(TAG, "  Head       : %u", fifo->head);
    DEBUG_INFO(TAG, "  Tail       : %u", fifo->tail);
    DEBUG_INFO(TAG, "  Pow2       : %s", fifo->is_power_of_two ? "oui" : "non");
    DEBUG_INFO(TAG, "  Intégrité  : %s", FIFO_CheckIntegrity(fifo) ? "OK" : "ERREUR");
    DEBUG_INFO(TAG, "========================================");
}

/* ======================================================================== */
/*              FONCTIONS PRIVÉES                                           */
/* ======================================================================== */

/**
 * @brief Minimum de deux uint16
 */
static inline uint16_t min_u16(uint16_t a, uint16_t b)
{
    return (a < b) ? a : b;
}

/**
 * @brief Vérifie si puissance de 2
 */
static inline bool is_power_of_two(uint16_t n)
{
    return (n != 0) && ((n & (n - 1)) == 0);
}

/* ======================================================================== */
/*              EXEMPLE D'INTÉGRATION AUDIO                                 */
/* ======================================================================== */

#if 0  /* Exemple - Non compilé */

/* Déclaration des buffers audio */
FIFO_DECLARE(audio_tx_fifo, 1024);  /* Micro → LoRa */
FIFO_DECLARE(audio_rx_fifo, 1024);  /* LoRa → HP   */

/* Buffer DMA double buffering */
static uint8_t dma_tx_buffer[512];
static uint8_t dma_rx_buffer[512];

/**
 * @brief ISR DMA microphone (producteur)
 * Appelée quand le buffer DMA est plein
 */
void Audio_TX_DMA_Complete_ISR(void)
{
    /* Écrire les échantillons dans le FIFO */
    FIFO_DMA_WRITE_ISR(&audio_tx_fifo, dma_tx_buffer, sizeof(dma_tx_buffer));
}

/**
 * @brief ISR DMA haut-parleur (consommateur)
 * Appelée quand le buffer DMA a besoin de données
 */
void Audio_RX_DMA_Request_ISR(void)
{
    /* Lire les échantillons depuis le FIFO */
    FIFO_DMA_READ_ISR(&audio_rx_fifo, dma_rx_buffer, sizeof(dma_rx_buffer));
}

/**
 * @brief Boucle principale : transfert LoRa → HP
 */
void ProcessAudioPlayback(void)
{
    uint16_t avail;
    uint8_t* data = FIFO_GetReadPtr(&audio_rx_fifo, &avail);
    
    if (data && avail > 0) {
        /* Envoyer au DAC ou codec audio */
        Audio_OutputSamples(data, avail);
        FIFO_CommitRead(&audio_rx_fifo, avail);
    }
}

/**
 * @brief ISR LoRa : paquet audio reçu
 */
void LoRa_AudioPacket_Received(uint8_t* packet, uint16_t length)
{
    /* Ajouter au buffer de lecture audio */
    FIFO_WriteForce(&audio_rx_fifo, packet, length);
}

#endif

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */