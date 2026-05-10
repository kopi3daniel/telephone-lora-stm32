/**
 * @file    fifo_buffer.h
 * @brief   Buffer FIFO simplifié - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente un buffer FIFO minimaliste optimisé pour les transferts
 * audio et les flux de données continus.
 * 
 * CONTEXTE D'UTILISATION :
 * 
 *   Ce buffer est une version ALLÉGÉE du ring_buffer, spécialement
 *   conçue pour les cas d'usage où la performance est critique :
 * 
 *   - Buffer audio micro → LoRa (flux continu, 8 kHz)
 *   - Buffer audio LoRa → HP (streaming, latence minimale)
 *   - Buffer DMA (transferts rapides, zero-copy)
 *   - File de paquets réseau (throughput élevé)
 * 
 * DIFFÉRENCES AVEC RING_BUFFER ET CIRCULAR_QUEUE :
 * 
 *   RingBuffer     : Générique, element_size variable, DMA zero-copy
 *   CircularQueue  : Éléments typés, itération, recherche
 *   FIFOBuffer     : Minimaliste, optimisation octets, API simplifiée
 * 
 *   Utiliser FIFOBuffer pour  : Audio streaming, DMA, flux bruts
 *   Utiliser RingBuffer pour  : Buffer générique avec stats
 *   Utiliser CircularQueue pour : Files d'objets avec recherche
 * 
 * OPTIMISATIONS :
 * 
 *   1. Pas de section critique par défaut (l'appelant gère la synchronisation)
 *   2. Macros inline pour les opérations simples (évite les appels de fonction)
 *   3. Taille puissance de 2 recommandée (masque au lieu de modulo)
 *   4. Pas de statistiques (économie RAM et cycles CPU)
 *   5. Zero-copy via pointeurs directs (GetReadPtr/GetWritePtr)
 * 
 * EXEMPLE TYPIQUE (Audio) :
 * 
 *   // Déclaration
 *   FIFO_DECLARE(audio_fifo, 1024);
 * 
 *   // ISR DMA (producteur) : écrit les échantillons
 *   void DMA_IRQ(void) {
 *       uint16_t avail;
 *       uint8_t* dst = FIFO_GetWritePtr(&audio_fifo, &avail);
 *       memcpy(dst, dma_buffer, avail);
 *       FIFO_CommitWrite(&audio_fifo, avail);
 *   }
 * 
 *   // Boucle principale (consommateur) : lit les échantillons
 *   void Process(void) {
 *       uint16_t avail;
 *       uint8_t* src = FIFO_GetReadPtr(&audio_fifo, &avail);
 *       process_audio(src, avail);
 *       FIFO_CommitRead(&audio_fifo, avail);
 *   }
 */

#ifndef FIFO_BUFFER_H
#define FIFO_BUFFER_H

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

/** Taille maximale du nom */
#define FIFO_MAX_NAME_LENGTH                    12

/** Capacité maximale (16-bit pour économiser la RAM) */
#define FIFO_MAX_CAPACITY                       65535

/** Alignement recommandé pour DMA */
#define FIFO_DMA_ALIGNMENT                      4

/* ======================================================================== */
/*                     TYPES                                                 */
/* ======================================================================== */

/**
 * @brief Structure du buffer FIFO minimaliste
 * 
 * Taille : 20 octets + buffer de données
 * 
 * @note Optimisé pour la vitesse, pas de section critique interne.
 * @note Pour usage ISR, l'appelant doit gérer la synchronisation.
 */
typedef struct {
    /* ---- Données ---- */
    uint8_t*            buffer;         /**< Mémoire du buffer               */
    uint16_t            capacity;       /**< Capacité (doit être puissance de 2) */
    uint16_t            mask;           /**< Masque = capacity - 1            */

    /* ---- Index ---- */
    volatile uint16_t   head;           /**< Index de lecture                */
    volatile uint16_t   tail;           /**< Index d'écriture                */
    volatile uint16_t   count;          /**< Nombre d'octets dans le buffer  */

    /* ---- Nom (debug) ---- */
    char                name[FIFO_MAX_NAME_LENGTH];

    /* ---- Flags ---- */
    bool                is_initialized; /**< Buffer initialisé               */
    bool                is_power_of_two;/**< Capacity est puissance de 2     */
} FIFOBuffer_t;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/**
 * @brief Initialise un buffer FIFO
 * 
 * @param fifo          Buffer à initialiser
 * @param buffer        Mémoire allouée
 * @param capacity      Capacité en octets (puissance de 2 recommandée)
 * @param name          Nom (debug)
 * @return              true si succès
 */
bool FIFO_Init(FIFOBuffer_t* fifo, uint8_t* buffer, uint16_t capacity, const char* name);

/**
 * @brief Réinitialise le buffer (vide tout)
 * @param fifo          Buffer
 */
void FIFO_Reset(FIFOBuffer_t* fifo);

/**
 * @brief Écrit un seul octet
 * 
 * @param fifo          Buffer
 * @param byte          Octet à écrire
 * @return              true si écrit (false si plein)
 */
bool FIFO_WriteByte(FIFOBuffer_t* fifo, uint8_t byte);

/**
 * @brief Écrit plusieurs octets
 * 
 * @param fifo          Buffer
 * @param data          Données source
 * @param length        Nombre d'octets
 * @return              Nombre d'octets écrits
 */
uint16_t FIFO_Write(FIFOBuffer_t* fifo, const uint8_t* data, uint16_t length);

/**
 * @brief Lit un seul octet
 * 
 * @param fifo          Buffer
 * @param byte          [out] Octet lu
 * @return              true si lu
 */
bool FIFO_ReadByte(FIFOBuffer_t* fifo, uint8_t* byte);

/**
 * @brief Lit plusieurs octets
 * 
 * @param fifo          Buffer
 * @param data          [out] Destination
 * @param length        Nombre max d'octets
 * @return              Nombre d'octets lus
 */
uint16_t FIFO_Read(FIFOBuffer_t* fifo, uint8_t* data, uint16_t length);

/**
 * @brief Consulte sans retirer (peek)
 * 
 * @param fifo          Buffer
 * @param data          [out] Destination
 * @param length        Nombre max d'octets
 * @return              Nombre d'octets consultés
 */
uint16_t FIFO_Peek(FIFOBuffer_t* fifo, uint8_t* data, uint16_t length);

/**
 * @brief Consulte un octet à une position relative
 * 
 * @param fifo          Buffer
 * @param offset        Position (0 = prochain)
 * @return              Octet (0 si hors limites)
 */
uint8_t FIFO_PeekAt(FIFOBuffer_t* fifo, uint16_t offset);

/**
 * @brief Avance le pointeur de lecture (skip)
 * 
 * @param fifo          Buffer
 * @param count         Nombre d'octets à sauter
 * @return              Nombre d'octets sautés
 */
uint16_t FIFO_Skip(FIFOBuffer_t* fifo, uint16_t count);

/**
 * @brief Vide le buffer
 * @param fifo          Buffer
 */
void FIFO_Flush(FIFOBuffer_t* fifo);

/**
 * @brief Vérifie si vide
 * @param fifo          Buffer
 * @return              true si vide
 */
bool FIFO_IsEmpty(FIFOBuffer_t* fifo);

/**
 * @brief Vérifie si plein
 * @param fifo          Buffer
 * @return              true si plein
 */
bool FIFO_IsFull(FIFOBuffer_t* fifo);

/**
 * @brief Nombre d'octets disponibles
 * @param fifo          Buffer
 * @return              Nombre d'octets
 */
uint16_t FIFO_GetCount(FIFOBuffer_t* fifo);

/**
 * @brief Espace libre
 * @param fifo          Buffer
 * @return              Octets libres
 */
uint16_t FIFO_GetFree(FIFOBuffer_t* fifo);

/**
 * @brief Capacité totale
 * @param fifo          Buffer
 * @return              Capacité
 */
uint16_t FIFO_GetCapacity(FIFOBuffer_t* fifo);

/**
 * @brief Taux de remplissage (0-100)
 * @param fifo          Buffer
 * @return              Pourcentage
 */
uint8_t FIFO_GetUsagePercent(FIFOBuffer_t* fifo);

/* ---- Zero-copy DMA ---- */

/**
 * @brief Obtient un pointeur direct vers la zone de lecture
 * 
 * Permet un accès DMA sans copie.
 * Valable jusqu'au prochain CommitRead.
 * 
 * @param fifo          Buffer
 * @param available     [out] Octets consécutifs disponibles
 * @return              Pointeur (NULL si vide)
 */
uint8_t* FIFO_GetReadPtr(FIFOBuffer_t* fifo, uint16_t* available);

/**
 * @brief Obtient un pointeur direct vers la zone d'écriture
 * 
 * @param fifo          Buffer
 * @param available     [out] Octets consécutifs disponibles
 * @return              Pointeur (NULL si plein)
 */
uint8_t* FIFO_GetWritePtr(FIFOBuffer_t* fifo, uint16_t* available);

/**
 * @brief Valide les octets écrits après GetWritePtr
 * 
 * @param fifo          Buffer
 * @param count         Nombre d'octets écrits
 */
void FIFO_CommitWrite(FIFOBuffer_t* fifo, uint16_t count);

/**
 * @brief Valide les octets lus après GetReadPtr
 * 
 * @param fifo          Buffer
 * @param count         Nombre d'octets lus
 */
void FIFO_CommitRead(FIFOBuffer_t* fifo, uint16_t count);

/* ---- Opérations spéciales ---- */

/**
 * @brief Écrit en écrasant les anciennes données si nécessaire
 * 
 * Version "force write" qui garantit que toutes les données
 * sont écrites, quitte à écraser les plus anciennes.
 * 
 * @param fifo          Buffer
 * @param data          Données
 * @param length        Nombre d'octets
 * @return              Nombre d'octets écrits (toujours = length)
 */
uint16_t FIFO_WriteForce(FIFOBuffer_t* fifo, const uint8_t* data, uint16_t length);

/**
 * @brief Attend que le buffer atteigne un certain niveau
 * 
 * Bloque l'exécution jusqu'à ce que count >= min_count
 * ou que le timeout soit atteint.
 * 
 * @param fifo          Buffer
 * @param min_count     Nombre minimum d'octets attendus
 * @param timeout_ms    Timeout en ms (0 = infini)
 * @return              Nombre d'octets disponibles
 */
uint16_t FIFO_WaitForData(FIFOBuffer_t* fifo, uint16_t min_count, uint32_t timeout_ms);

/**
 * @brief Attend que le buffer ait de l'espace libre
 * 
 * @param fifo          Buffer
 * @param min_free      Espace minimum requis
 * @param timeout_ms    Timeout en ms
 * @return              Espace libre disponible
 */
uint16_t FIFO_WaitForSpace(FIFOBuffer_t* fifo, uint16_t min_free, uint32_t timeout_ms);

/* ---- Diagnostic ---- */

/**
 * @brief Vérifie l'intégrité du buffer
 * 
 * @param fifo          Buffer
 * @return              true si pas d'anomalie
 */
bool FIFO_CheckIntegrity(FIFOBuffer_t* fifo);

/**
 * @brief Affiche les informations de diagnostic
 * @param fifo          Buffer
 */
void FIFO_PrintInfo(FIFOBuffer_t* fifo);

/* ======================================================================== */
/*              MACROS UTILITAIRES                                           */
/* ======================================================================== */

/**
 * @brief Déclare et initialise un buffer FIFO statique
 * 
 * Utilisation :
 *   FIFO_DECLARE(audio_fifo, 1024);
 *   → Crée audio_fifo_storage[1024] et audio_fifo (FIFOBuffer_t)
 * 
 * @param name      Nom de la variable
 * @param cap       Capacité en octets (puissance de 2 recommandée)
 */
#define FIFO_DECLARE(name, cap) \
    static uint8_t name##_storage[(cap)]; \
    static FIFOBuffer_t name = { \
        .buffer = name##_storage, \
        .capacity = (cap), \
        .mask = (cap) - 1, \
        .head = 0, \
        .tail = 0, \
        .count = 0, \
        .is_initialized = true, \
        .is_power_of_two = (((cap) & ((cap) - 1)) == 0) \
    }

/**
 * @brief Version rapide de WriteByte (sans vérification)
 * 
 * ⚠️ N'effectue AUCUNE vérification de débordement.
 * Utiliser uniquement quand on est SÛR qu'il y a de la place.
 */
#define FIFO_WRITE_BYTE_FAST(fifo, byte) \
    do { \
        (fifo)->buffer[(fifo)->tail] = (byte); \
        (fifo)->tail = ((fifo)->tail + 1) & (fifo)->mask; \
        (fifo)->count++; \
    } while(0)

/**
 * @brief Version rapide de ReadByte (sans vérification)
 * 
 * ⚠️ N'effectue AUCUNE vérification de buffer vide.
 */
#define FIFO_READ_BYTE_FAST(fifo, byte_ptr) \
    do { \
        *(byte_ptr) = (fifo)->buffer[(fifo)->head]; \
        (fifo)->head = ((fifo)->head + 1) & (fifo)->mask; \
        (fifo)->count--; \
    } while(0)

/**
 * @brief Vérifie si vide (macro rapide)
 */
#define FIFO_EMPTY(fifo)                ((fifo)->count == 0)

/**
 * @brief Vérifie si plein (macro rapide)
 */
#define FIFO_FULL(fifo)                 ((fifo)->count >= (fifo)->capacity)

/**
 * @brief Nombre d'octets (macro rapide)
 */
#define FIFO_COUNT(fifo)                ((fifo)->count)

/**
 * @brief Espace libre (macro rapide)
 */
#define FIFO_FREE(fifo)                 ((fifo)->capacity - (fifo)->count)

/**
 * @brief Taux remplissage (macro rapide)
 */
#define FIFO_USAGE(fifo)                (((fifo)->count * 100) / (fifo)->capacity)

/* ======================================================================== */
/*              MACROS D'ACCÈS DMA (INTÉGRATION ISR)                        */
/* ======================================================================== */

/**
 * @brief Macro pour utilisation typique en ISR DMA
 * 
 * Exemple :
 *   FIFO_DMA_WRITE_ISR(audio_fifo, dma_buffer, half_complete_flag);
 */
#define FIFO_DMA_WRITE_ISR(fifo, src, count) \
    do { \
        uint16_t _avail; \
        uint8_t* _dst = FIFO_GetWritePtr((fifo), &_avail); \
        if (_dst && _avail >= (count)) { \
            memcpy(_dst, (src), (count)); \
            FIFO_CommitWrite((fifo), (count)); \
        } \
    } while(0)

/**
 * @brief Macro pour lecture DMA
 */
#define FIFO_DMA_READ_ISR(fifo, dst, count) \
    do { \
        uint16_t _avail; \
        uint8_t* _src = FIFO_GetReadPtr((fifo), &_avail); \
        if (_src && _avail >= (count)) { \
            memcpy((dst), _src, (count)); \
            FIFO_CommitRead((fifo), (count)); \
        } \
    } while(0)

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */