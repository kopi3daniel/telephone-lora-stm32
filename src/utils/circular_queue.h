/**
 * @file    circular_queue.h
 * @brief   File circulaire générique (FIFO) - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente une file circulaire (FIFO) pour tout type de données.
 * Contrairement au ring_buffer qui travaille sur des octets, circular_queue
 * travaille sur des ÉLÉMENTS de taille fixe (structs, pointeurs, uint32...).
 * 
 * DIFFÉRENCE AVEC RING_BUFFER :
 * 
 *   RingBuffer     : Buffer d'octets, élément_size variable, DMA direct
 *   CircularQueue  : File d'éléments, élément_size fixe, FIFO d'objets
 * 
 *   Utiliser RingBuffer pour : Audio, UART, données brutes
 *   Utiliser CircularQueue pour : Événements, messages, commandes
 * 
 * AVANTAGES :
 * 
 * 1. TYPÉ : Travaille avec n'importe quel type (struct, pointeur, int...)
 * 2. ALLOCATION STATIQUE : Pas de malloc/free, pas de fragmentation
 * 3. THREAD-SAFE : Protection par section critique
 * 4. ISR-SAFE : Peut être utilisée dans les interruptions
 * 5. PRÉVISIBLE : Latence constante O(1)
 * 
 * FONCTIONNEMENT :
 * 
 *   File de 4 éléments (int) :
 * 
 *   Initial :  [  ] [  ] [  ] [  ]     count=0, head=0, tail=0
 * 
 *   Push(10) : [10] [  ] [  ] [  ]     count=1, head=0, tail=1
 *   Push(20) : [10] [20] [  ] [  ]     count=2, head=0, tail=2
 *   Push(30) : [10] [20] [30] [  ]     count=3, head=0, tail=3
 * 
 *   Pop()→10 : [  ] [20] [30] [  ]     count=2, head=1, tail=3
 *   Pop()→20 : [  ] [  ] [30] [  ]     count=1, head=2, tail=3
 * 
 *   Push(40) : [  ] [  ] [30] [40]     count=2, head=2, tail=0
 *   Push(50) : [50] [  ] [30] [40]     count=3, head=2, tail=1
 * 
 * EXEMPLES D'UTILISATION :
 * 
 *   // File d'événements
 *   typedef struct { int type; float value; } Event;
 *   CIRCULAR_QUEUE_DECLARE(event_queue, Event, 32);
 *   CircularQueue_Push(&event_queue, &my_event);
 *   Event evt;
 *   CircularQueue_Pop(&event_queue, &evt);
 * 
 *   // File de pointeurs
 *   CIRCULAR_QUEUE_DECLARE(ptr_queue, void*, 16);
 *   void* ptr = malloc(100);
 *   CircularQueue_Push(&ptr_queue, &ptr);
 */

#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H

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

/** Taille maximale du nom (debug) */
#define CQ_MAX_NAME_LENGTH                      16

/** Capacité maximale autorisée */
#define CQ_MAX_CAPACITY                         65535

/* ======================================================================== */
/*                     TYPES                                                 */
/* ======================================================================== */

/**
 * @brief Mode de gestion du débordement
 */
typedef enum {
    CQ_MODE_OVERWRITE = 0,              /**< Écraser le plus ancien si plein  */
    CQ_MODE_BLOCKING,                   /**< Refuser l'ajout si plein          */
    CQ_MODE_EXPANDING,                  /**< Expansion dynamique (TODO)        */
} CircularQueueMode_t;

/**
 * @brief Codes de retour
 */
typedef enum {
    CQ_OK = 0,                          /**< Succès                           */
    CQ_ERROR_NULL,                      /**< Pointeur NULL                    */
    CQ_ERROR_FULL,                      /**< File pleine (mode BLOCKING)      */
    CQ_ERROR_EMPTY,                     /**< File vide                        */
    CQ_ERROR_NOT_INITIALIZED,           /**< Non initialisée                  */
    CQ_ERROR_SIZE_MISMATCH,             /**< Taille élément différente        */
} CircularQueueError_t;

/**
 * @brief Structure de la file circulaire
 * 
 * Taille : ~48 octets + données
 * 
 * @note Thread-safe pour 1 producteur + 1 consommateur
 */
typedef struct {
    /* ---- Identification ---- */
    char            name[CQ_MAX_NAME_LENGTH];   /**< Nom de la file           */

    /* ---- Données ---- */
    uint8_t*        buffer;                     /**< Mémoire des éléments      */
    size_t          capacity;                   /**< Capacité max (éléments)   */
    size_t          element_size;               /**< Taille d'un élément       */
    size_t          total_size_bytes;           /**< Capacité * element_size   */

    /* ---- Index ---- */
    volatile size_t head;                       /**< Index de lecture          */
    volatile size_t tail;                       /**< Index d'écriture          */
    volatile size_t count;                      /**< Nombre d'éléments         */

    /* ---- Configuration ---- */
    CircularQueueMode_t mode;                   /**< Mode débordement          */
    bool            is_initialized;             /**< File initialisée          */
    bool            overwrite_on_full;          /**< Alias pour OVERWRITE      */

    /* ---- Statistiques ---- */
    uint32_t        total_pushes;               /**< Total ajouts              */
    uint32_t        total_pops;                 /**< Total retraits            */
    uint32_t        total_overflows;            /**< Total débordements        */
    uint32_t        total_underruns;            /**< Total retraits à vide     */
    uint32_t        max_count;                  /**< Utilisation maximale      */

    /* ---- Callback ---- */
    void (*on_overflow)(void* context);         /**< Débordement              */
    void*           overflow_context;           /**< Contexte callback         */

    /* ---- Verrou (si multi-thread) ---- */
    volatile bool   locked;                     /**< Verrou (simple)           */

} CircularQueue_t;

/**
 * @brief Statistiques de la file
 */
typedef struct {
    uint32_t        total_pushes;               /**< Total ajouts              */
    uint32_t        total_pops;                 /**< Total retraits            */
    uint32_t        total_overflows;            /**< Débordements              */
    uint32_t        total_underruns;            /**< Retraits à vide           */
    uint32_t        max_count;                  /**< Utilisation max           */
    uint32_t        current_count;              /**< Utilisation actuelle      */
    size_t          capacity;                   /**< Capacité totale           */
    float           usage_percent;              /**< Taux d'utilisation        */
} CircularQueueStats_t;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/* ---- Initialisation ---- */

/**
 * @brief Initialise une file circulaire
 * 
 * @param cq            File à initialiser
 * @param buffer        Mémoire allouée (capacité * element_size octets)
 * @param capacity      Nombre maximum d'éléments
 * @param element_size  Taille d'un élément en octets
 * @param mode          Mode de gestion du débordement
 * @param name          Nom (debug)
 * @return              Code de retour
 */
CircularQueueError_t CircularQueue_Init(CircularQueue_t* cq,
                                         void* buffer,
                                         size_t capacity,
                                         size_t element_size,
                                         CircularQueueMode_t mode,
                                         const char* name);

/**
 * @brief Réinitialise la file (vide tout)
 * @param cq            File
 */
void CircularQueue_Reset(CircularQueue_t* cq);

/* ---- Opérations d'écriture ---- */

/**
 * @brief Ajoute un élément dans la file
 * 
 * @param cq            File
 * @param element       Pointeur vers l'élément à copier
 * @return              true si ajouté
 */
bool CircularQueue_Push(CircularQueue_t* cq, const void* element);

/**
 * @brief Ajoute plusieurs éléments
 * 
 * @param cq            File
 * @param elements      Tableau d'éléments
 * @param count         Nombre d'éléments
 * @return              Nombre d'éléments ajoutés
 */
size_t CircularQueue_PushMultiple(CircularQueue_t* cq,
                                  const void* elements,
                                  size_t count);

/**
 * @brief Ajoute un élément à une position absolue (écrase)
 * 
 * ⚠️ Usage avancé : ne vérifie pas la validité de l'index.
 * 
 * @param cq            File
 * @param index         Position absolue dans le buffer
 * @param element       Élément à écrire
 * @return              true si écrit
 */
bool CircularQueue_PushAt(CircularQueue_t* cq,
                          size_t index,
                          const void* element);

/* ---- Opérations de lecture ---- */

/**
 * @brief Retire le prochain élément de la file
 * 
 * @param cq            File
 * @param element       [out] Élément lu
 * @return              true si lu
 */
bool CircularQueue_Pop(CircularQueue_t* cq, void* element);

/**
 * @brief Retire plusieurs éléments
 * 
 * @param cq            File
 * @param elements      [out] Tableau de destination
 * @param count         Nombre maximum d'éléments
 * @return              Nombre d'éléments lus
 */
size_t CircularQueue_PopMultiple(CircularQueue_t* cq,
                                 void* elements,
                                 size_t count);

/**
 * @brief Consulte le prochain élément sans le retirer
 * 
 * @param cq            File
 * @param element       [out] Élément
 * @return              true si consulté
 */
bool CircularQueue_Peek(CircularQueue_t* cq, void* element);

/**
 * @brief Consulte un élément à une position relative
 * 
 * @param cq            File
 * @param offset        Position (0 = prochain à sortir)
 * @param element       [out] Élément
 * @return              true si trouvé
 */
bool CircularQueue_PeekAt(CircularQueue_t* cq,
                          size_t offset,
                          void* element);

/**
 * @brief Retire le prochain élément sans le copier (skip)
 * 
 * @param cq            File
 * @return              true si retiré
 */
bool CircularQueue_Skip(CircularQueue_t* cq);

/* ---- Opérations sur le dernier élément ---- */

/**
 * @brief Consulte le dernier élément ajouté
 * 
 * @param cq            File
 * @param element       [out] Dernier élément
 * @return              true si trouvé
 */
bool CircularQueue_PeekLast(CircularQueue_t* cq, void* element);

/**
 * @brief Retire le dernier élément ajouté (LIFO)
 * 
 * @param cq            File
 * @param element       [out] Élément retiré
 * @return              true si retiré
 */
bool CircularQueue_PopLast(CircularQueue_t* cq, void* element);

/* ---- État et informations ---- */

/**
 * @brief Vérifie si la file est vide
 * @param cq            File
 * @return              true si vide
 */
bool CircularQueue_IsEmpty(CircularQueue_t* cq);

/**
 * @brief Vérifie si la file est pleine
 * @param cq            File
 * @return              true si pleine
 */
bool CircularQueue_IsFull(CircularQueue_t* cq);

/**
 * @brief Retourne le nombre d'éléments
 * @param cq            File
 * @return              Nombre d'éléments
 */
size_t CircularQueue_GetCount(CircularQueue_t* cq);

/**
 * @brief Retourne l'espace libre
 * @param cq            File
 * @return              Nombre d'emplacements libres
 */
size_t CircularQueue_GetFree(CircularQueue_t* cq);

/**
 * @brief Retourne la capacité
 * @param cq            File
 * @return              Capacité max
 */
size_t CircularQueue_GetCapacity(CircularQueue_t* cq);

/**
 * @brief Vérifie si la file contient un élément (recherche)
 * 
 * @param cq            File
 * @param element       Élément à rechercher
 * @param compare_fn    Fonction de comparaison (NULL = memcmp)
 * @return              true si trouvé
 */
bool CircularQueue_Contains(CircularQueue_t* cq,
                            const void* element,
                            int (*compare_fn)(const void* a, const void* b));

/* ---- Statistiques ---- */

/**
 * @brief Récupère les statistiques
 * @param cq            File
 * @param stats         [out] Statistiques
 */
void CircularQueue_GetStats(CircularQueue_t* cq,
                            CircularQueueStats_t* stats);

/**
 * @brief Réinitialise les statistiques
 * @param cq            File
 */
void CircularQueue_ResetStats(CircularQueue_t* cq);

/**
 * @brief Imprime les statistiques (debug)
 * @param cq            File
 */
void CircularQueue_PrintStats(CircularQueue_t* cq);

/* ---- Configuration ---- */

/**
 * @brief Définit le callback de débordement
 * @param cq            File
 * @param callback      Fonction
 * @param context       Contexte
 */
void CircularQueue_SetOverflowCallback(CircularQueue_t* cq,
                                       void (*callback)(void* context),
                                       void* context);

/**
 * @brief Verrouille la file (empêche écriture)
 * @param cq            File
 */
void CircularQueue_Lock(CircularQueue_t* cq);

/**
 * @brief Déverrouille la file
 * @param cq            File
 */
void CircularQueue_Unlock(CircularQueue_t* cq);

/* ---- Itération ---- */

/**
 * @brief Type de fonction d'itération
 * 
 * @param element       Élément courant
 * @param index         Position dans la file
 * @param context       Contexte utilisateur
 * @return              true pour continuer, false pour arrêter
 */
typedef bool (*CircularQueueIterator_t)(void* element,
                                        size_t index,
                                        void* context);

/**
 * @brief Parcourt tous les éléments de la file
 * 
 * @param cq            File
 * @param iterator      Fonction appelée pour chaque élément
 * @param context       Contexte passé à l'itérateur
 * @return              Nombre d'éléments parcourus
 */
size_t CircularQueue_ForEach(CircularQueue_t* cq,
                             CircularQueueIterator_t iterator,
                             void* context);

/* ---- Conversion ---- */

/**
 * @brief Convertit la file en tableau (copie)
 * 
 * @param cq            File
 * @param array         [out] Tableau de destination
 * @param max_count     Taille max du tableau
 * @return              Nombre d'éléments copiés (dans l'ordre FIFO)
 */
size_t CircularQueue_ToArray(CircularQueue_t* cq,
                             void* array,
                             size_t max_count);

/* ---- Utilitaires ---- */

/**
 * @brief Retourne le nom lisible d'un code d'erreur
 * @param error         Code
 * @return              Chaîne statique
 */
const char* CircularQueue_GetErrorName(CircularQueueError_t error);

/**
 * @brief Vérifie l'intégrité de la file (debug)
 * @param cq            File
 * @return              true si intègre
 */
bool CircularQueue_CheckIntegrity(CircularQueue_t* cq);

/* ======================================================================== */
/*              MACROS UTILITAIRES                                           */
/* ======================================================================== */

/**
 * @brief Déclare et initialise une file circulaire statique
 * 
 * Utilisation :
 *   CIRCULAR_QUEUE_DECLARE(my_queue, MyStruct, 32);
 *   → Crée my_queue_storage[32 * sizeof(MyStruct)] et my_queue
 * 
 * @param name      Nom de la variable
 * @param type      Type des éléments
 * @param capacity  Capacité maximale
 */
#define CIRCULAR_QUEUE_DECLARE(name, type, capacity) \
    static type name##_storage[(capacity)]; \
    static CircularQueue_t name = { \
        .buffer = (uint8_t*)name##_storage, \
        .capacity = (capacity), \
        .element_size = sizeof(type), \
        .total_size_bytes = sizeof(name##_storage), \
        .head = 0, \
        .tail = 0, \
        .count = 0, \
        .is_initialized = true \
    }

/**
 * @brief Déclare une file de pointeurs
 * 
 * Utilisation :
 *   CIRCULAR_QUEUE_PTR_DECLARE(ptr_queue, 16);
 * 
 * @param name      Nom de la variable
 * @param capacity  Capacité
 */
#define CIRCULAR_QUEUE_PTR_DECLARE(name, capacity) \
    CIRCULAR_QUEUE_DECLARE(name, void*, capacity)

/**
 * @brief Push typé (évite le cast)
 */
#define CQ_PUSH(cq, element_ptr) \
    CircularQueue_Push((cq), (const void*)(element_ptr))

/**
 * @brief Pop typé (évite le cast)
 */
#define CQ_POP(cq, element_ptr) \
    CircularQueue_Pop((cq), (void*)(element_ptr))

/**
 * @brief Peek typé
 */
#define CQ_PEEK(cq, element_ptr) \
    CircularQueue_Peek((cq), (void*)(element_ptr))

/**
 * @brief Vérifie si la file est vide (macro rapide)
 */
#define CQ_IS_EMPTY(cq)             ((cq)->count == 0)

/**
 * @brief Vérifie si la file est pleine (macro rapide)
 */
#define CQ_IS_FULL(cq)              ((cq)->count >= (cq)->capacity)

/**
 * @brief Nombre d'éléments (macro rapide)
 */
#define CQ_COUNT(cq)                ((cq)->count)

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */