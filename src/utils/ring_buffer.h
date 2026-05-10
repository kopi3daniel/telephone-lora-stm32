/**
 * @file    ring_buffer.h
 * @brief   Buffer circulaire générique - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente un buffer circulaire (FIFO) thread-safe pour le stockage
 * temporaire de données. Utilisé principalement pour :
 * 
 * - Buffer audio (microphone → DMA → ring buffer → LoRa)
 * - Buffer audio (LoRa → ring buffer → DMA → haut-parleur)
 * - File d'événements
 * - Buffer de paquets LoRa
 * - Buffer série (UART debug)
 * - Toute file FIFO générique
 * 
 * AVANTAGES DU BUFFER CIRCULAIRE :
 * 
 * 1. PAS DE COPIE : Les données sont écrites/lues directement dans le buffer
 * 2. TAILLE FIXE : Pas d'allocation dynamique, mémoire statique
 * 3. THREAD-SAFE : Protection par section critique (IRQ disable)
 * 4. ÉCONOMIE RAM : Pas de pointeurs de liste chaînée
 * 5. RAPIDE : O(1) pour écriture et lecture
 * 
 * FONCTIONNEMENT :
 * 
 *   État initial (vide) :
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 *   │   │   │   │   │   │   │   │   │
 *   └───┴───┴───┴───┴───┴───┴───┴───┘
 *     ↑ head=0
 *     ↑ tail=0
 *     count=0
 * 
 *   Après 3 écritures (A, B, C) :
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 *   │ A │ B │ C │   │   │   │   │   │
 *   └───┴───┴───┴───┴───┴───┴───┴───┘
 *     ↑ head=0       ↑ tail=3
 *     count=3
 * 
 *   Après 2 lectures :
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 *   │   │   │ C │   │   │   │   │   │
 *   └───┴───┴───┴───┴───┴───┴───┴───┘
 *             ↑ head=2   ↑ tail=3
 *             count=1
 * 
 *   Après écriture de D, E, F, G, H (buffer plein) :
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 *   │ H │   │ C │ D │ E │ F │ G │ H │  ← C est le prochain à lire
 *   └───┴───┴───┴───┴───┴───┴───┴───┘
 *     ↑ tail=1   ↑ head=2
 *     count=7
 * 
 *   Note : tail peut être < head quand le buffer "boucle"
 * 
 * MODES DE DÉBORDEMENT :
 * 
 *   Mode OVERWRITE (par défaut) :
 *     Si le buffer est plein, les données les plus anciennes
 *     sont écrasées silencieusement. head avance avec tail.
 *     Utile pour l'audio (ne pas bloquer la capture).
 * 
 *   Mode BLOCKING :
 *     Si le buffer est plein, l'écriture est refusée.
 *     L'appelant doit réessayer plus tard.
 *     Utile pour les événements (ne pas perdre de données).
 * 
 * EXEMPLE D'UTILISATION :
 * 
 *   // Créer un buffer de 1024 octets
 *   uint8_t buffer_memory[1024];
 *   RingBuffer_t rb;
 *   RingBuffer_Init(&rb, buffer_memory, 1024, 1, RING_BUFFER_MODE_OVERWRITE);
 * 
 *   // Écrire des données (ex: depuis une ISR DMA)
 *   RingBuffer_Write(&rb, data, length);
 * 
 *   // Lire des données (ex: dans la boucle principale)
 *   uint8_t read_buffer[256];
 *   size_t read = RingBuffer_Read(&rb, read_buffer, sizeof(read_buffer));
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* ======================================================================== */
/*                     CONSTANTES SYMBOLIQUES                                */
/* ======================================================================== */

/**
 * @brief Taille maximale du nom du buffer (pour debug)
 */
#define RING_BUFFER_MAX_NAME_LENGTH             16

/* ======================================================================== */
/*                     TYPES                                                 */
/* ======================================================================== */

/**
 * @brief Mode de gestion du débordement
 */
typedef enum {
    RING_BUFFER_MODE_OVERWRITE = 0,     /**< Écraser les anciennes données    */
    RING_BUFFER_MODE_BLOCKING,          /**< Refuser l'écriture si plein      */
} RingBufferMode_t;

/**
 * @brief Codes d'erreur du buffer circulaire
 */
typedef enum {
    RING_BUFFER_OK = 0,                 /**< Succès                          */
    RING_BUFFER_ERROR_NULL,             /**< Pointeur NULL                   */
    RING_BUFFER_ERROR_FULL,             /**< Buffer plein (mode BLOCKING)    */
    RING_BUFFER_ERROR_EMPTY,            /**< Buffer vide                     */
    RING_BUFFER_ERROR_TOO_LARGE,        /**< Données trop grandes            */
    RING_BUFFER_ERROR_NOT_ENOUGH_DATA,  /**< Pas assez de données à lire     */
    RING_BUFFER_ERROR_OVERFLOW,         /**< Débordement (perte de données)  */
} RingBufferError_t;

/**
 * @brief Structure du buffer circulaire
 * 
 * Taille : environ 40 octets + buffer de données
 * 
 * Le buffer est thread-safe pour un producteur et un consommateur.
 * Pour plusieurs producteurs, utiliser un mutex supplémentaire.
 */
typedef struct {
    /* ---- Identification ---- */
    char                name[RING_BUFFER_MAX_NAME_LENGTH]; /**< Nom du buffer */

    /* ---- Pointeurs de données ---- */
    uint8_t*            buffer;             /**< Pointeur vers la mémoire      */
    size_t              size;               /**< Taille totale du buffer       */
    size_t              element_size;       /**< Taille d'un élément (1=octet) */

    /* ---- Index (head = lecture, tail = écriture) ---- */
    volatile size_t     head;               /**< Index de lecture              */
    volatile size_t     tail;               /**< Index d'écriture              */
    volatile size_t     count;              /**< Nombre d'éléments dans buffer */

    /* ---- Configuration ---- */
    RingBufferMode_t    mode;               /**< Mode de débordement           */
    bool                is_initialized;     /**< Buffer initialisé             */

    /* ---- Statistiques ---- */
    uint32_t            total_writes;       /**< Total écritures               */
    uint32_t            total_reads;        /**< Total lectures                */
    uint32_t            total_overflows;    /**< Total débordements            */
    uint32_t            total_underruns;    /**< Total lectures buffer vide    */
    uint32_t            max_usage;          /**< Utilisation maximale          */

    /* ---- Callback ---- */
    void (*on_overflow)(void* context);     /**< Appelé en cas de débordement  */
    void*               overflow_context;   /**< Contexte pour le callback     */

} RingBuffer_t;

/**
 * @brief Statistiques détaillées du buffer
 */
typedef struct {
    uint32_t            total_writes;       /**< Nombre total d'écritures      */
    uint32_t            total_reads;        /**< Nombre total de lectures       */
    uint32_t            total_overflows;    /**< Nombre de débordements         */
    uint32_t            total_underruns;    /**< Nombre de lectures à vide      */
    uint32_t            max_usage;          /**< Utilisation maximale (éléments)*/
    uint32_t            current_usage;      /**< Utilisation actuelle           */
    size_t              size;               /**< Taille totale                  */
    float               usage_percent;      /**< Taux d'utilisation (%)         */
} RingBufferStats_t;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/**
 * @brief Initialise un buffer circulaire
 * 
 * @param rb            Buffer à initialiser
 * @param buffer        Mémoire allouée pour le buffer
 * @param size          Taille totale en octets
 * @param element_size  Taille d'un élément (1 pour octet, 2 pour uint16, etc.)
 * @param mode          Mode de gestion du débordement
 * @param name          Nom du buffer (pour debug)
 * @return              RING_BUFFER_OK si succès
 */
RingBufferError_t RingBuffer_Init(RingBuffer_t* rb,
                                  void* buffer,
                                  size_t size,
                                  size_t element_size,
                                  RingBufferMode_t mode,
                                  const char* name);

/**
 * @brief Réinitialise le buffer (vide tout)
 * @param rb            Buffer
 */
void RingBuffer_Reset(RingBuffer_t* rb);

/**
 * @brief Écrit des données dans le buffer
 * 
 * THREAD-SAFE : peut être appelée depuis une ISR.
 * 
 * @param rb            Buffer
 * @param data          Données à écrire
 * @param count         Nombre d'éléments à écrire
 * @return              Nombre d'éléments écrits (peut être < count)
 */
size_t RingBuffer_Write(RingBuffer_t* rb, const void* data, size_t count);

/**
 * @brief Écrit un seul élément
 * 
 * @param rb            Buffer
 * @param data          Pointeur vers l'élément
 * @return              true si écrit
 */
bool RingBuffer_WriteOne(RingBuffer_t* rb, const void* data);

/**
 * @brief Écrit un octet (raccourci pour buffers d'octets)
 * @param rb            Buffer
 * @param byte          Octet à écrire
 * @return              true si écrit
 */
bool RingBuffer_WriteByte(RingBuffer_t* rb, uint8_t byte);

/**
 * @brief Lit des données depuis le buffer
 * 
 * THREAD-SAFE : peut être appelée depuis une ISR.
 * 
 * @param rb            Buffer
 * @param data          [out] Buffer de destination
 * @param count         Nombre maximum d'éléments à lire
 * @return              Nombre d'éléments lus
 */
size_t RingBuffer_Read(RingBuffer_t* rb, void* data, size_t count);

/**
 * @brief Lit un seul élément
 * 
 * @param rb            Buffer
 * @param data          [out] Pointeur vers l'élément
 * @return              true si lu
 */
bool RingBuffer_ReadOne(RingBuffer_t* rb, void* data);

/**
 * @brief Lit un octet (raccourci)
 * @param rb            Buffer
 * @param byte          [out] Octet lu
 * @return              true si lu
 */
bool RingBuffer_ReadByte(RingBuffer_t* rb, uint8_t* byte);

/**
 * @brief Consulte des données sans les retirer (peek)
 * 
 * @param rb            Buffer
 * @param data          [out] Buffer de destination
 * @param count         Nombre maximum d'éléments à consulter
 * @return              Nombre d'éléments consultés
 */
size_t RingBuffer_Peek(RingBuffer_t* rb, void* data, size_t count);

/**
 * @brief Consulte un élément à une position donnée
 * 
 * @param rb            Buffer
 * @param index         Position relative (0 = prochain à lire)
 * @param data          [out] Élément
 * @return              true si trouvé
 */
bool RingBuffer_PeekAt(RingBuffer_t* rb, size_t index, void* data);

/**
 * @brief Avance l'index de lecture sans copier (skip)
 * 
 * Équivalent à Read sans copier les données.
 * 
 * @param rb            Buffer
 * @param count         Nombre d'éléments à sauter
 * @return              Nombre d'éléments sautés
 */
size_t RingBuffer_Skip(RingBuffer_t* rb, size_t count);

/**
 * @brief Vide complètement le buffer
 * @param rb            Buffer
 */
void RingBuffer_Flush(RingBuffer_t* rb);

/**
 * @brief Vérifie si le buffer est vide
 * @param rb            Buffer
 * @return              true si vide
 */
bool RingBuffer_IsEmpty(RingBuffer_t* rb);

/**
 * @brief Vérifie si le buffer est plein
 * @param rb            Buffer
 * @return              true si plein
 */
bool RingBuffer_IsFull(RingBuffer_t* rb);

/**
 * @brief Retourne le nombre d'éléments dans le buffer
 * @param rb            Buffer
 * @return              Nombre d'éléments
 */
size_t RingBuffer_GetCount(RingBuffer_t* rb);

/**
 * @brief Retourne l'espace libre (nombre d'éléments)
 * @param rb            Buffer
 * @return              Espace libre
 */
size_t RingBuffer_GetFree(RingBuffer_t* rb);

/**
 * @brief Retourne la taille totale du buffer
 * @param rb            Buffer
 * @return              Taille en éléments
 */
size_t RingBuffer_GetSize(RingBuffer_t* rb);

/**
 * @brief Retourne le taux d'utilisation (%)
 * @param rb            Buffer
 * @return              Pourcentage (0-100)
 */
float RingBuffer_GetUsagePercent(RingBuffer_t* rb);

/**
 * @brief Récupère les statistiques
 * @param rb            Buffer
 * @param stats         [out] Statistiques
 */
void RingBuffer_GetStats(RingBuffer_t* rb, RingBufferStats_t* stats);

/**
 * @brief Réinitialise les statistiques (garde les données)
 * @param rb            Buffer
 */
void RingBuffer_ResetStats(RingBuffer_t* rb);

/**
 * @brief Définit le callback de débordement
 * @param rb            Buffer
 * @param callback      Fonction à appeler
 * @param context       Contexte utilisateur
 */
void RingBuffer_SetOverflowCallback(RingBuffer_t* rb,
                                    void (*callback)(void* context),
                                    void* context);

/**
 * @brief Retourne un pointeur direct vers la zone de lecture
 * 
 * Permet un accès DMA direct sans copie.
 * 
 * @param rb            Buffer
 * @param available     [out] Nombre d'octets consécutifs disponibles
 * @return              Pointeur vers les données (valide jusqu'à la prochaine opération)
 */
const uint8_t* RingBuffer_GetReadPointer(RingBuffer_t* rb, size_t* available);

/**
 * @brief Retourne un pointeur direct vers la zone d'écriture
 * 
 * @param rb            Buffer
 * @param available     [out] Espace consécutif disponible
 * @return              Pointeur vers la zone d'écriture
 */
uint8_t* RingBuffer_GetWritePointer(RingBuffer_t* rb, size_t* available);

/**
 * @brief Valide l'écriture après un GetWritePointer
 * 
 * @param rb            Buffer
 * @param count         Nombre d'éléments écrits
 */
void RingBuffer_CommitWrite(RingBuffer_t* rb, size_t count);

/**
 * @brief Valide la lecture après un GetReadPointer
 * @param rb            Buffer
 * @param count         Nombre d'éléments lus
 */
void RingBuffer_CommitRead(RingBuffer_t* rb, size_t count);

/**
 * @brief Imprime les statistiques (debug console)
 * @param rb            Buffer
 */
void RingBuffer_PrintStats(RingBuffer_t* rb);

/**
 * @brief Retourne le nom lisible d'un code d'erreur
 * @param error         Code d'erreur
 * @return              Chaîne statique
 */
const char* RingBuffer_GetErrorName(RingBufferError_t error);

/* ======================================================================== */
/*              MACROS UTILITAIRES                                           */
/* ======================================================================== */

/**
 * @brief Déclare et initialise un buffer circulaire statique
 * 
 * Utilisation :
 *   RING_BUFFER_DECLARE(audio_buf, 1024);
 *   → Crée audio_buf_buffer[1024] et audio_buf (RingBuffer_t)
 */
#define RING_BUFFER_DECLARE(name, size) \
    static uint8_t name##_storage[(size)]; \
    static RingBuffer_t name = { \
        .buffer = name##_storage, \
        .size = (size), \
        .element_size = 1, \
        .head = 0, \
        .tail = 0, \
        .count = 0 \
    }

/**
 * @brief Écriture rapide sans vérification (usage interne)
 */
#define RING_BUFFER_WRITE_BYTE_FAST(rb, byte) \
    do { \
        (rb)->buffer[(rb)->tail] = (byte); \
        (rb)->tail = ((rb)->tail + 1) % (rb)->size; \
        (rb)->count++; \
        if ((rb)->count > (rb)->max_usage) (rb)->max_usage = (rb)->count; \
    } while (0)

/**
 * @brief Lecture rapide sans vérification (usage interne)
 */
#define RING_BUFFER_READ_BYTE_FAST(rb, byte_ptr) \
    do { \
        *(byte_ptr) = (rb)->buffer[(rb)->head]; \
        (rb)->head = ((rb)->head + 1) % (rb)->size; \
        (rb)->count--; \
    } while (0)

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */