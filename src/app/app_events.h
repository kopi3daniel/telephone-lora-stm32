/**
 * @file    app_events.h
 * @brief   Gestionnaire d'événements - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Gère la file d'événements de l'application.
 * Les événements sont le mécanisme principal de communication
 * entre les interruptions (ISR), les drivers et l'interface utilisateur.
 * 
 * ARCHITECTURE DE LA FILE D'ÉVÉNEMENTS :
 * 
 * ┌─────────────────────────────────────────────────────────────┐
 * │                      PRODUCTEURS                            │
 * │                                                             │
 * │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
 * │  │ ISR Touch│  │ ISR Keys │  │ ISR LoRa │  │ Timers   │   │
 * │  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘   │
 * │       │              │              │              │        │
 * │       └──────────────┴──────────────┴──────────────┘        │
 * │                          │                                  │
 * │                          ▼                                  │
 * │              ┌───────────────────────┐                      │
 * │              │   FILE D'ÉVÉNEMENTS   │                      │
 * │              │  Circular Buffer      │                      │
 * │              │  Capacity: 32         │                      │
 * │              │  Thread-Safe          │                      │
 * │              └───────────┬───────────┘                      │
 * │                          │                                  │
 * │                          ▼                                  │
 * │              ┌───────────────────────┐                      │
 * │              │   CONSOMMATEUR        │                      │
 * │              │  PhoneApp_Run()       │                      │
 * │              │  Boucle principale    │                      │
 * │              └───────────────────────┘                      │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * FORMAT D'UN ÉVÉNEMENT :
 * 
 *   ┌──────────────────────────────────────────────────────┐
 *   │  AppEvent_t                                          │
 *   │  ┌────────────────────────────────────────────────┐  │
 *   │  │ type       : AppEventType_t   (1 octet)        │  │
 *   │  │ priority   : AppEventPriority_t (1 octet)      │  │
 *   │  │ timestamp  : uint32_t          (4 octets)      │  │
 *   │  │ data       : union { ... }     (max 64 octets) │  │
 *   │  └────────────────────────────────────────────────┘  │
 *   └──────────────────────────────────────────────────────┘
 * 
 * PRIORITÉS DE TRAITEMENT :
 * 
 *   1. CRITICAL  → Erreurs système, batterie critique
 *   2. HIGH      → Appels entrants, messages reçus
 *   3. NORMAL    → Interactions utilisateur (touch, keys)
 *   4. LOW       → Timers, mises à jour périodiques
 * 
 * La file est vidée dans l'ordre FIFO, mais les événements
 * de priorité supérieure peuvent être traités avant.
 * 
 * THREAD SAFETY :
 * 
 * La file est protégée par désactivation des interruptions
 * (critical section avec __disable_irq / __enable_irq).
 * 
 * Les ISR peuvent poster des événements en toute sécurité.
 * La boucle principale consomme les événements sans IRQ disable.
 */

#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ======================================================================== */
/*                     CONSTANTES SYMBOLIQUES                                */
/* ======================================================================== */

/**
 * @brief Taille maximale de la file d'événements
 * 
 * Au-delà, les nouveaux événements sont perdus.
 * Dimensionné pour gérer les pics (ex: plusieurs touches rapides).
 */
#define APP_EVENT_QUEUE_SIZE                    32

/**
 * @brief Taille maximale du nom d'un événement (pour logs)
 */
#define APP_EVENT_NAME_MAX_LENGTH               24

/**
 * @brief Taille maximale des données d'un événement
 */
#define APP_EVENT_DATA_MAX_SIZE                 64

/**
 * @brief Taille maximale d'un message dans un événement
 */
#define APP_EVENT_MESSAGE_MAX_LENGTH            256

/**
 * @brief Taille maximale d'un numéro de téléphone
 */
#define APP_EVENT_PHONE_MAX_LENGTH              20

/**
 * @brief Taille maximale d'un identifiant
 */
#define APP_EVENT_ID_MAX_LENGTH                 32

/* ======================================================================== */
/*                     TYPES D'ÉVÉNEMENTS                                    */
/* ======================================================================== */

/**
 * @brief Types d'événements de l'application
 * 
 * Chaque type correspond à une source ou une action spécifique.
 * Les valeurs sont volontairement espacées pour permettre
 * l'ajout futur de nouveaux types.
 */
typedef enum {
    /* ---- Événements système (0-9) ---- */
    APP_EVENT_NONE                  = 0,    /**< Aucun événement               */
    APP_EVENT_SYSTEM_TICK           = 1,    /**< Tick système (1ms)            */
    APP_EVENT_TIMER_EXPIRED         = 2,    /**< Timer logiciel expiré         */
    APP_EVENT_WATCHDOG_WARNING      = 3,    /**< Avertissement chien de garde  */

    /* ---- Événements utilisateur (10-19) ---- */
    APP_EVENT_TOUCH_PRESS           = 10,   /**< Écran tactile pressé          */
    APP_EVENT_TOUCH_RELEASE         = 11,   /**< Écran tactile relâché         */
    APP_EVENT_TOUCH_MOVE            = 12,   /**< Déplacement doigt             */
    APP_EVENT_TOUCH_SWIPE_UP        = 13,   /**< Glissement vers le haut       */
    APP_EVENT_TOUCH_SWIPE_DOWN      = 14,   /**< Glissement vers le bas        */
    APP_EVENT_TOUCH_SWIPE_LEFT      = 15,   /**< Glissement vers la gauche     */
    APP_EVENT_TOUCH_SWIPE_RIGHT     = 16,   /**< Glissement vers la droite     */
    APP_EVENT_TOUCH_LONG_PRESS      = 17,   /**< Appui long (> 1s)             */
    APP_EVENT_TOUCH_DOUBLE_TAP      = 18,   /**< Double appui rapide           */
    APP_EVENT_KEY_PRESS             = 19,   /**< Touche physique pressée       */
    APP_EVENT_KEY_RELEASE           = 20,   /**< Touche physique relâchée      */
    APP_EVENT_KEY_LONG_PRESS        = 21,   /**< Touche appui long             */
    APP_EVENT_KEY_REPEAT            = 22,   /**< Touche répétition auto        */

    /* ---- Événements communication (30-39) ---- */
    APP_EVENT_LORA_PACKET_RECEIVED  = 30,   /**< Paquet LoRa reçu              */
    APP_EVENT_LORA_TX_COMPLETE      = 31,   /**< Transmission terminée         */
    APP_EVENT_LORA_RX_TIMEOUT       = 32,   /**< Timeout réception             */
    APP_EVENT_LORA_RX_ERROR         = 33,   /**< Erreur réception              */
    APP_EVENT_LORA_CRC_ERROR        = 34,   /**< Erreur CRC paquet             */
    APP_EVENT_LORA_CAD_DETECTED     = 35,   /**< Activité canal détectée       */

    /* ---- Événements téléphonie (50-59) ---- */
    APP_EVENT_INCOMING_CALL         = 50,   /**< Appel entrant détecté         */
    APP_EVENT_CALL_ACCEPTED         = 51,   /**< Appel accepté (local)         */
    APP_EVENT_CALL_REJECTED         = 52,   /**< Appel refusé (local)          */
    APP_EVENT_CALL_ENDED            = 53,   /**< Appel terminé                 */
    APP_EVENT_CALL_MISSED           = 54,   /**< Appel manqué                  */
    APP_EVENT_CALL_CONNECTED        = 55,   /**< Appel connecté (remote ACK)   */
    APP_EVENT_CALL_TIMEOUT          = 56,   /**< Timeout appel sans réponse    */
    APP_EVENT_CALL_BUSY             = 57,   /**< Correspondant occupé          */

    /* ---- Événements messages (70-79) ---- */
    APP_EVENT_NEW_MESSAGE           = 70,   /**< Nouveau message reçu          */
    APP_EVENT_MESSAGE_SENT          = 71,   /**< Message envoyé avec succès    */
    APP_EVENT_MESSAGE_FAILED        = 72,   /**< Échec envoi message           */
    APP_EVENT_MESSAGE_DELIVERED     = 73,   /**< Accusé réception message      */
    APP_EVENT_MESSAGE_READ          = 74,   /**< Accusé lecture message        */

    /* ---- Événements système avancés (90-99) ---- */
    APP_EVENT_BATTERY_LOW           = 90,   /**< Batterie faible (15%)         */
    APP_EVENT_BATTERY_CRITICAL      = 91,   /**< Batterie critique (3%)        */
    APP_EVENT_BATTERY_NORMAL        = 92,   /**< Batterie revenue normale      */
    APP_EVENT_CHARGING_START        = 93,   /**< Charge démarrée               */
    APP_EVENT_CHARGING_STOP         = 94,   /**< Charge arrêtée                */
    APP_EVENT_SCREEN_TIMEOUT        = 95,   /**< Timeout écran                 */
    APP_EVENT_SCREEN_WAKEUP         = 96,   /**< Réveil écran                  */
    APP_EVENT_SYSTEM_ERROR          = 97,   /**< Erreur système                */
    APP_EVENT_SYSTEM_WARNING        = 98,   /**< Avertissement système         */
    APP_EVENT_FACTORY_RESET         = 99,   /**< Reset usine demandé           */

    APP_EVENT_COUNT                         /**< Nombre total de types         */
} AppEventType_t;

/**
 * @brief Priorité des événements
 */
typedef enum {
    APP_PRIORITY_LOW        = 0,        /**< Basse : timers, mises à jour     */
    APP_PRIORITY_NORMAL     = 1,        /**< Normale : UI, touches            */
    APP_PRIORITY_HIGH       = 2,        /**< Haute : appels, messages         */
    APP_PRIORITY_CRITICAL   = 3,        /**< Critique : erreurs, batterie     */
    APP_PRIORITY_COUNT                  /**< Nombre de niveaux                */
} AppEventPriority_t;

/* ======================================================================== */
/*                     STRUCTURES DE DONNÉES                                */
/* ======================================================================== */

/**
 * @brief Données pour un événement tactile
 */
typedef struct {
    uint16_t    x;                      /**< Coordonnée X (0-319)             */
    uint16_t    y;                      /**< Coordonnée Y (0-479)             */
    uint16_t    pressure;               /**< Pression (0-4095)                */
    uint8_t     gesture;                /**< Type de geste (si reconnu)       */
    uint8_t     touch_id;               /**< Identifiant du point tactile     */
} AppEventTouchData_t;

/**
 * @brief Données pour un événement clavier
 */
typedef struct {
    uint16_t    key_code;               /**< Code de la touche                */
    uint8_t     modifier;               /**< Modificateurs (Shift, Alt...)    */
    bool        repeated;               /**< Répétition automatique           */
    uint16_t    repeat_count;           /**< Nombre de répétitions            */
} AppEventKeyData_t;

/**
 * @brief Données pour un événement LoRa
 */
typedef struct {
    uint8_t*    data;                   /**< Pointeur vers les données        */
    uint16_t    length;                 /**< Longueur des données             */
    int16_t     rssi;                   /**< RSSI en dBm                      */
    int8_t      snr;                    /**< Rapport signal/bruit en dB       */
    uint8_t     sf;                     /**< Spreading Factor utilisé         */
    uint32_t    frequency_hz;           /**< Fréquence utilisée               */
} AppEventLoRaData_t;

/**
 * @brief Données pour un événement d'appel
 */
typedef struct {
    char        caller_id[APP_EVENT_ID_MAX_LENGTH];     /**< Identifiant appelant  */
    char        caller_number[APP_EVENT_PHONE_MAX_LENGTH];/**< Numéro appelant      */
    uint32_t    call_id;                /**< Identifiant unique de l'appel     */
    uint32_t    duration_sec;           /**< Durée de l'appel (si terminé)     */
    uint8_t     call_type;              /**< Type d'appel (entrant/sortant)    */
} AppEventCallData_t;

/**
 * @brief Données pour un événement message
 */
typedef struct {
    char        sender[APP_EVENT_ID_MAX_LENGTH];        /**< Expéditeur       */
    char        content[APP_EVENT_MESSAGE_MAX_LENGTH];  /**< Contenu          */
    uint32_t    message_id;             /**< Identifiant unique du message    */
    uint16_t    content_length;         /**< Longueur réelle du contenu       */
    bool        is_read;                /**< Message déjà lu                  */
    bool        is_emergency;           /**< Message d'urgence                */
} AppEventMessageData_t;

/**
 * @brief Données pour un événement batterie
 */
typedef struct {
    uint8_t     battery_percent;        /**< Pourcentage (0-100)              */
    uint16_t    battery_mv;             /**< Tension en millivolts            */
    bool        is_charging;            /**< En charge                        */
    uint8_t     temperature_c;          /**< Température en °C                */
} AppEventBatteryData_t;

/**
 * @brief Données pour un événement erreur système
 */
typedef struct {
    uint32_t    error_code;             /**< Code erreur                      */
    char        error_msg[APP_EVENT_MESSAGE_MAX_LENGTH];/**< Message          */
    char        file[64];               /**< Fichier source                   */
    uint32_t    line;                   /**< Ligne source                     */
    bool        is_fatal;               /**< Erreur fatale                    */
} AppEventErrorData_t;

/**
 * @brief Données pour un événement timer
 */
typedef struct {
    uint32_t    timer_id;               /**< Identifiant du timer             */
    void*       timer_handle;           /**< Handle du timer                  */
    uint32_t    elapsed_ms;             /**< Temps écoulé                     */
} AppEventTimerData_t;

/* ======================================================================== */
/*                     STRUCTURE D'ÉVÉNEMENT                                */
/* ======================================================================== */

/**
 * @brief Structure complète d'un événement
 * 
 * Taille totale : environ 72 octets (selon la plus grande union)
 * 
 * Les unions permettent d'économiser de la mémoire en partageant
 * le même espace pour différents types de données.
 */
typedef struct {
    /* ---- En-tête ---- */
    AppEventType_t      type;           /**< Type d'événement                 */
    AppEventPriority_t  priority;       /**< Priorité                         */
    uint32_t            timestamp_ms;   /**< Timestamp de création            */
    bool                handled;        /**< Événement traité                 */
    uint8_t             reserved[3];    /**< Padding / alignement             */

    /* ---- Données spécifiques (union) ---- */
    union {
        /* Touch */
        AppEventTouchData_t     touch;

        /* Keys */
        AppEventKeyData_t       key;

        /* LoRa */
        AppEventLoRaData_t      lora;

        /* Call */
        AppEventCallData_t      call;

        /* Message */
        AppEventMessageData_t   message;

        /* Battery */
        AppEventBatteryData_t   battery;

        /* Error */
        AppEventErrorData_t     error;

        /* Timer */
        AppEventTimerData_t     timer;

        /* Données brutes (accès générique) */
        uint8_t                 raw_data[APP_EVENT_DATA_MAX_SIZE];
    } data;

} AppEvent_t;

/* ======================================================================== */
/*                     FILE D'ÉVÉNEMENTS                                    */
/* ======================================================================== */

/**
 * @brief Structure de la file d'événements (circular buffer)
 * 
 * Thread-safe : protégée par critical section.
 * 
 * Implémentation FIFO avec écrasement des plus anciens
 * si la file est pleine (configurable).
 */
typedef struct {
    /* ---- Buffer circulaire ---- */
    AppEvent_t          buffer[APP_EVENT_QUEUE_SIZE]; /**< Tableau d'événements */
    volatile uint8_t    read_index;     /**< Index de lecture                  */
    volatile uint8_t    write_index;    /**< Index d'écriture                  */
    volatile uint8_t    count;          /**< Nombre d'événements en file       */

    /* ---- Statistiques ---- */
    uint32_t            total_posted;   /**< Total événements postés           */
    uint32_t            total_consumed; /**< Total événements consommés        */
    uint32_t            total_lost;     /**< Total événements perdus (file pleine) */
    uint32_t            total_errors;   /**< Total erreurs de traitement       */

    /* ---- Configuration ---- */
    bool                overwrite_oldest;/**< Écraser le plus ancien si plein   */
    bool                log_events;     /**< Logger chaque événement           */

    /* ---- Callback ---- */
    void (*on_overflow)(void);          /**< Appelé si la file déborde         */

} AppEventQueue_t;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/**
 * @brief Initialise la file d'événements
 * 
 * @param queue     File à initialiser
 * @param overwrite Si true, écrase les anciens événements si file pleine
 */
void AppEventQueue_Init(AppEventQueue_t* queue, bool overwrite);

/**
 * @brief Réinitialise la file (vide tous les événements)
 * @param queue     File à réinitialiser
 */
void AppEventQueue_Reset(AppEventQueue_t* queue);

/**
 * @brief Poste un événement dans la file
 * 
 * THREAD-SAFE : peut être appelée depuis une ISR.
 * 
 * @param queue     File d'événements
 * @param event     Événement à poster (copié)
 * @return          true si l'événement a été ajouté
 */
bool AppEventQueue_Post(AppEventQueue_t* queue, const AppEvent_t* event);

/**
 * @brief Poste un événement simple (sans données)
 * 
 * Version allégée pour les événements sans données.
 * 
 * @param queue     File d'événements
 * @param type      Type d'événement
 * @param priority  Priorité
 * @return          true si ajouté
 */
bool AppEventQueue_PostSimple(AppEventQueue_t* queue,
                              AppEventType_t type,
                              AppEventPriority_t priority);

/**
 * @brief Récupère le prochain événement de la file
 * 
 * @param queue     File d'événements
 * @param event     [out] Événement dépilé
 * @return          true si un événement était disponible
 */
bool AppEventQueue_Get(AppEventQueue_t* queue, AppEvent_t* event);

/**
 * @brief Consulte le prochain événement sans le retirer
 * 
 * @param queue     File d'événements
 * @param event     [out] Événement consulté
 * @return          true si un événement est disponible
 */
bool AppEventQueue_Peek(AppEventQueue_t* queue, AppEvent_t* event);

/**
 * @brief Vide complètement la file
 * @param queue     File d'événements
 */
void AppEventQueue_Flush(AppEventQueue_t* queue);

/**
 * @brief Retourne le nombre d'événements en attente
 * @param queue     File d'événements
 * @return          Nombre d'événements
 */
uint8_t AppEventQueue_GetCount(AppEventQueue_t* queue);

/**
 * @brief Vérifie si la file est vide
 * @param queue     File d'événements
 * @return          true si vide
 */
bool AppEventQueue_IsEmpty(AppEventQueue_t* queue);

/**
 * @brief Vérifie si la file est pleine
 * @param queue     File d'événements
 * @return          true si pleine
 */
bool AppEventQueue_IsFull(AppEventQueue_t* queue);

/**
 * @brief Récupère les statistiques de la file
 * @param queue         File d'événements
 * @param total_posted  [out] Total postés
 * @param total_lost    [out] Total perdus
 * @param total_errors  [out] Total erreurs
 */
void AppEventQueue_GetStats(AppEventQueue_t* queue,
                            uint32_t* total_posted,
                            uint32_t* total_lost,
                            uint32_t* total_errors);

/**
 * @brief Définit le callback de débordement
 * @param queue     File d'événements
 * @param callback  Fonction à appeler si la file déborde
 */
void AppEventQueue_SetOverflowCallback(AppEventQueue_t* queue,
                                       void (*callback)(void));

/**
 * @brief Active/désactive les logs d'événements
 * @param queue     File d'événements
 * @param enable    true pour activer
 */
void AppEventQueue_SetLogging(AppEventQueue_t* queue, bool enable);

/* ======================================================================== */
/*              FONCTIONS UTILITAIRES                                       */
/* ======================================================================== */

/**
 * @brief Retourne le nom lisible d'un type d'événement
 * 
 * @param type      Type d'événement
 * @return          Chaîne statique (ne pas libérer)
 */
const char* AppEvent_GetTypeName(AppEventType_t type);

/**
 * @brief Retourne le nom lisible d'une priorité
 * 
 * @param priority  Priorité
 * @return          Chaîne statique
 */
const char* AppEvent_GetPriorityName(AppEventPriority_t priority);

/**
 * @brief Crée un événement tactile
 * 
 * @param event     [out] Événement créé
 * @param type      Type (PRESS, RELEASE, MOVE...)
 * @param x         Coordonnée X
 * @param y         Coordonnée Y
 */
void AppEvent_CreateTouchEvent(AppEvent_t* event,
                               AppEventType_t type,
                               uint16_t x,
                               uint16_t y);

/**
 * @brief Crée un événement clavier
 * 
 * @param event     [out] Événement créé
 * @param key_code  Code de la touche
 * @param repeated  Répétition automatique
 */
void AppEvent_CreateKeyEvent(AppEvent_t* event,
                             uint16_t key_code,
                             bool repeated);

/**
 * @brief Crée un événement d'appel entrant
 * 
 * @param event         [out] Événement
 * @param caller_id     Identifiant appelant
 * @param caller_number Numéro appelant
 */
void AppEvent_CreateIncomingCallEvent(AppEvent_t* event,
                                      const char* caller_id,
                                      const char* caller_number);

/**
 * @brief Crée un événement message
 * 
 * @param event     [out] Événement
 * @param sender    Expéditeur
 * @param content   Contenu
 */
void AppEvent_CreateMessageEvent(AppEvent_t* event,
                                 const char* sender,
                                 const char* content);

/**
 * @brief Crée un événement erreur système
 * 
 * @param event     [out] Événement
 * @param code      Code erreur
 * @param msg       Message descriptif
 * @param file      Fichier source
 * @param line      Ligne
 */
void AppEvent_CreateErrorEvent(AppEvent_t* event,
                               uint32_t code,
                               const char* msg,
                               const char* file,
                               uint32_t line);

/**
 * @brief Vérifie si un événement est de haute priorité
 * 
 * @param event     Événement
 * @return          true si priorité HIGH ou CRITICAL
 */
bool AppEvent_IsHighPriority(const AppEvent_t* event);

/**
 * @brief Clone un événement
 * 
 * @param dest      [out] Destination
 * @param src       Source
 */
void AppEvent_Clone(AppEvent_t* dest, const AppEvent_t* src);

/* ======================================================================== */
/*              MACROS UTILITAIRES                                           */
/* ======================================================================== */

/**
 * @brief Poste un événement simple (macro pratique)
 */
#define APP_POST_EVENT(queue, type, prio) \
    AppEventQueue_PostSimple((queue), (type), (prio))

/**
 * @brief Vérifie si un événement est d'un type donné
 */
#define APP_EVENT_IS_TYPE(event, t)     ((event).type == (t))

/**
 * @brief Vérifie si un événement est dans une catégorie
 */
#define APP_EVENT_IS_TOUCH(event)       ((event).type >= APP_EVENT_TOUCH_PRESS && (event).type <= APP_EVENT_TOUCH_DOUBLE_TAP)
#define APP_EVENT_IS_KEY(event)         ((event).type >= APP_EVENT_KEY_PRESS && (event).type <= APP_EVENT_KEY_REPEAT)
#define APP_EVENT_IS_LORA(event)        ((event).type >= APP_EVENT_LORA_PACKET_RECEIVED && (event).type <= APP_EVENT_LORA_CAD_DETECTED)
#define APP_EVENT_IS_CALL(event)        ((event).type >= APP_EVENT_INCOMING_CALL && (event).type <= APP_EVENT_CALL_BUSY)
#define APP_EVENT_IS_MESSAGE(event)     ((event).type >= APP_EVENT_NEW_MESSAGE && (event).type <= APP_EVENT_MESSAGE_READ)
#define APP_EVENT_IS_BATTERY(event)     ((event).type >= APP_EVENT_BATTERY_LOW && (event).type <= APP_EVENT_BATTERY_NORMAL)
#define APP_EVENT_IS_ERROR(event)       ((event).type == APP_EVENT_SYSTEM_ERROR || (event).type == APP_EVENT_SYSTEM_WARNING)

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */