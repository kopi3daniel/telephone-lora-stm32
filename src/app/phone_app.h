/**
 * @file    phone_app.h
 * @brief   Application principale - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Ce fichier est le CERVEAU de l'application téléphone LoRa.
 * Il orchestre TOUS les composants : écrans, services, drivers.
 * 
 * ARCHITECTURE DE L'APPLICATION :
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                       PHONE APP (main.cpp)                      │
 * │                                                                 │
 * │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
 * │  │ Splash   │  │  Lock    │  │  Home    │  │ Appels   │       │
 * │  │ Screen   │─→│  Screen  │─→│  Screen  │─→│  Screen  │       │
 * │  └──────────┘  └──────────┘  └──────────┘  └──────────┘       │
 * │       │              │              │              │            │
 * │       └──────────────┴──────────────┴──────────────┘            │
 * │                          │                                      │
 * │              ┌───────────┴───────────┐                          │
 * │              │   SCREEN MANAGER      │                          │
 * │              │   (Navigation Pile)   │                          │
 * │              └───────────────────────┘                          │
 * │                          │                                      │
 * │  ┌─────────────────────────────────────────────────────────┐   │
 * │  │                    SERVICES                              │   │
 * │  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌───────┐ │   │
 * │  │  │ Phone  │ │  SMS   │ │Contact │ │Settings│ │ Audio │ │   │
 * │  │  │Service │ │Service │ │Service │ │Service │ │Service│ │   │
 * │  │  └────────┘ └────────┘ └────────┘ └────────┘ └───────┘ │   │
 * │  └─────────────────────────────────────────────────────────┘   │
 * │                          │                                      │
 * │  ┌─────────────────────────────────────────────────────────┐   │
 * │  │                    DRIVERS                               │   │
 * │  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐           │   │
 * │  │  │ LoRa   │ │Display │ │ Audio  │ │ Power  │           │   │
 * │  │  │ SX1278 │ │ LTDC   │ │ ADC/DAC│ │Manager │           │   │
 * │  │  └────────┘ └────────┘ └────────┘ └────────┘           │   │
 * │  └─────────────────────────────────────────────────────────┘   │
 * │                          │                                      │
 * │  ┌─────────────────────────────────────────────────────────┐   │
 * │  │              STM32F429 HARDWARE                          │   │
 * │  │  Cortex-M4 @ 180 MHz | SDRAM 8 Mo | Flash 2 Mo          │   │
 * │  └─────────────────────────────────────────────────────────┘   │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * CYCLE DE VIE DE L'APPLICATION :
 * 
 * 1. DÉMARRAGE (Reset)
 *    └──→ SystemInit() : Horloge, SDRAM, NVIC
 *         └──→ main()
 *              └──→ PhoneApp_Init()
 *                   ├──→ Drivers init (LTDC, DMA2D, SPI, Audio)
 *                   ├──→ Services init (Settings, Contacts, CallLog)
 *                   ├──→ Screens init (Splash, Lock, Home, Dialer...)
 *                   └──→ PhoneApp_Run()
 *                        └──→ Boucle infinie
 *                             ├──→ Événements (Touch, Keys, LoRa, Timers)
 *                             ├──→ Screen_Update()
 *                             └──→ Services_Process()
 * 
 * 2. BOUCLE PRINCIPALE (super-loop ou FreeRTOS)
 *    while (1) {
 *        PhoneApp_ProcessEvents();   // Touch, Keys, LoRa RX
 *        PhoneApp_UpdateUI();        // Rafraîchir l'écran actif
 *        PhoneApp_ProcessServices(); // Call, SMS, timers
 *    }
 * 
 * 3. TRANSITIONS D'ÉCRANS
 *    ScreenManager gère une pile LIFO :
 *    - Push(screen) : Empiler un nouvel écran
 *    - Pop()         : Revenir à l'écran précédent
 *    - Replace(screen) : Remplacer l'écran courant
 * 
 *    Exemple de navigation :
 *    Home → Push(Settings) → Push(Network) → Pop() → Settings → Pop() → Home
 * 
 * 4. GESTION DES ÉVÉNEMENTS
 *    File d'événements prioritaire :
 *    1. Appel entrant (priorité haute)
 *    2. Événements tactiles
 *    3. Touches physiques
 *    4. Timers expirés
 *    5. Paquets LoRa reçus
 */

#ifndef PHONE_APP_H
#define PHONE_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include <stdint.h>
#include <stdbool.h>

/* Écrans */
#include "../screens/screen_splash.h"
#include "../screens/screen_lock.h"
#include "../screens/screen_home.h"
#include "../screens/screen_dialer.h"
#include "../screens/screen_call_active.h"
#include "../screens/screen_call_incoming.h"
#include "../screens/screen_call_log.h"
#include "../screens/screen_messages_list.h"
#include "../screens/screen_message_compose.h"
#include "../screens/screen_contacts_list.h"
#include "../screens/screen_contact_detail.h"
#include "../screens/screen_settings.h"
#include "../screens/screen_settings_network.h"
#include "../screens/screen_settings_audio.h"
#include "../screens/screen_settings_display.h"

/* Services */
#include "../services/phone_service.h"
#include "../services/sms_service.h"
#include "../services/contact_service.h"
#include "../services/call_log_service.h"
#include "../services/settings_service.h"
#include "../services/notification_service.h"
#include "../services/ringtone_service.h"

/* Protocoles */
#include "../protocols/call_protocol.h"
#include "../protocols/sms_protocol.h"
#include "../protocols/packet_router.h"

/* Drivers */
#include "../drivers/display/display_manager.h"
#include "../drivers/lora/lora_driver.h"
#include "../drivers/audio/audio_manager.h"
#include "../drivers/power/power_manager.h"
#include "../drivers/power/backlight_control.h"

/* UI */
#include "../ui/ui_core.h"
#include "../ui/ui_navigation.h"
#include "../ui/ui_theme.h"

/* Utils */
#include "../utils/debug_utils.h"
#include "../utils/timer_utils.h"

/* HAL */
#include "stm32f4xx_hal.h"

/* Version */
#include "../version.h"

/* ======================================================================== */
/*                     CONSTANTES SYMBOLIQUES                                */
/* ======================================================================== */

/**
 * @brief Version du firmware (définie dans version.h)
 */
#ifndef FIRMWARE_VERSION
    #define FIRMWARE_VERSION                    "1.0.0"
#endif

/**
 * @brief Nom du périphérique par défaut
 */
#define DEVICE_DEFAULT_NAME                     "LoRa Phone"

/**
 * @brief Taille de la file d'événements
 * 
 * Nombre maximum d'événements en attente de traitement.
 * Au-delà, les événements les plus anciens sont écrasés.
 */
#define APP_EVENT_QUEUE_SIZE                    32

/**
 * @brief Période de la boucle principale (ms)
 * 
 * Si FreeRTOS n'est pas utilisé, délai entre chaque itération.
 */
#define APP_LOOP_DELAY_MS                       10

/**
 * @brief Timeout de vérification des appels (ms)
 */
#define APP_CALL_CHECK_INTERVAL_MS              100

/**
 * @brief Timeout de mise à jour de la barre de statut (ms)
 */
#define APP_STATUSBAR_UPDATE_MS                 1000

/* ======================================================================== */
/*                     TYPES D'ÉVÉNEMENTS                                    */
/* ======================================================================== */

/**
 * @brief Types d'événements de l'application
 * 
 * Chaque événement est placé dans la file et traité
 * dans la boucle principale par ordre de priorité.
 */
typedef enum {
    /* ---- Événements système ---- */
    APP_EVENT_NONE = 0,                     /**< Aucun événement               */
    APP_EVENT_SYSTEM_TICK,                  /**< Tick système (1ms)            */
    APP_EVENT_TIMER_EXPIRED,                /**< Timer logiciel expiré         */

    /* ---- Événements utilisateur ---- */
    APP_EVENT_TOUCH,                        /**< Écran tactile pressé          */
    APP_EVENT_TOUCH_RELEASE,                /**< Écran tactile relâché         */
    APP_EVENT_TOUCH_SWIPE,                  /**< Glissement détecté            */
    APP_EVENT_KEY_PRESS,                    /**< Touche physique pressée       */
    APP_EVENT_KEY_RELEASE,                  /**< Touche physique relâchée      */
    APP_EVENT_KEY_LONG_PRESS,               /**< Appui long (> 1s)             */

    /* ---- Événements communication ---- */
    APP_EVENT_LORA_PACKET_RECEIVED,         /**< Paquet LoRa reçu              */
    APP_EVENT_LORA_TX_COMPLETE,             /**< Transmission terminée         */
    APP_EVENT_LORA_RX_TIMEOUT,              /**< Timeout réception             */
    APP_EVENT_LORA_ERROR,                   /**< Erreur communication LoRa     */

    /* ---- Événements téléphonie ---- */
    APP_EVENT_INCOMING_CALL,                /**< Appel entrant détecté         */
    APP_EVENT_CALL_ACCEPTED,                /**< Appel accepté                 */
    APP_EVENT_CALL_REJECTED,                /**< Appel refusé                  */
    APP_EVENT_CALL_ENDED,                   /**< Appel terminé                 */
    APP_EVENT_CALL_MISSED,                  /**< Appel manqué                  */
    APP_EVENT_NEW_MESSAGE,                  /**< Nouveau message reçu          */
    APP_EVENT_MESSAGE_SENT,                 /**< Message envoyé                */

    /* ---- Événements système ---- */
    APP_EVENT_BATTERY_LOW,                  /**< Batterie faible               */
    APP_EVENT_BATTERY_CRITICAL,             /**< Batterie critique (arrêt)     */
    APP_EVENT_CHARGING_START,               /**< Charge démarrée               */
    APP_EVENT_CHARGING_STOP,                /**< Charge arrêtée                */
    APP_EVENT_SCREEN_TIMEOUT,               /**< Timeout écran → verrouillage  */
    APP_EVENT_SYSTEM_ERROR,                 /**< Erreur système grave          */

    APP_EVENT_COUNT                         /**< Nombre total d'événements     */
} AppEventType_t;

/**
 * @brief Priorité des événements
 * 
 * Les événements de priorité haute sont traités en premier.
 */
typedef enum {
    APP_PRIORITY_LOW = 0,                   /**< Basse (timers, updates)       */
    APP_PRIORITY_NORMAL,                    /**< Normale (touches, UI)         */
    APP_PRIORITY_HIGH,                      /**< Haute (appels, messages)      */
    APP_PRIORITY_CRITICAL,                  /**< Critique (erreurs, batterie)  */
} AppEventPriority_t;

/**
 * @brief Structure d'un événement applicatif
 */
typedef struct {
    AppEventType_t      type;               /**< Type d'événement              */
    AppEventPriority_t  priority;           /**< Priorité                      */
    uint32_t            timestamp_ms;       /**< Timestamp de l'événement      */
    
    /* Données spécifiques selon le type */
    union {
        struct {
            uint16_t x;                     /**< Coordonnée X tactile          */
            uint16_t y;                     /**< Coordonnée Y tactile          */
            uint8_t  gesture;               /**< Type de geste                */
        } touch;

        struct {
            uint16_t key_code;              /**< Code de la touche             */
            bool     repeated;              /**< Répétition automatique        */
        } key;

        struct {
            uint8_t* data;                  /**< Données du paquet             */
            uint16_t length;                /**< Longueur                      */
            int8_t   rssi;                  /**< RSSI en dBm                   */
            uint8_t  snr;                   /**< Rapport signal/bruit          */
        } lora_packet;

        struct {
            char     caller_id[32];         /**< Identifiant appelant          */
            char     caller_number[20];     /**< Numéro appelant               */
            uint32_t call_id;               /**< ID unique de l'appel          */
        } call;

        struct {
            char     sender_id[32];         /**< Expéditeur                    */
            char     content[256];          /**< Contenu du message            */
            uint32_t message_id;            /**< ID unique du message          */
            bool     is_read;               /**< Message lu                    */
        } message;

        struct {
            uint8_t  battery_percent;       /**< Pourcentage batterie          */
            bool     is_charging;           /**< En charge                     */
        } battery;

        struct {
            uint32_t error_code;            /**< Code erreur                   */
            char     error_msg[128];        /**< Message descriptif            */
        } error;
    } data;

} AppEvent_t;

/* ======================================================================== */
/*                     ÉTATS DE L'APPLICATION                                */
/* ======================================================================== */

/**
 * @brief États globaux de l'application
 */
typedef enum {
    APP_STATE_INIT,                         /**< Initialisation en cours       */
    APP_STATE_SPLASH,                       /**< Écran de démarrage            */
    APP_STATE_LOCKED,                       /**< Téléphone verrouillé          */
    APP_STATE_IDLE,                         /**< Écran d'accueil (inactif)     */
    APP_STATE_ACTIVE,                       /**< Interface active              */
    APP_STATE_IN_CALL,                      /**< Appel en cours                */
    APP_STATE_INCOMING_CALL,                /**< Appel entrant                 */
    APP_STATE_DIALING,                      /**< Numérotation en cours         */
    APP_STATE_MESSAGING,                    /**< Édition message               */
    APP_STATE_SETTINGS,                     /**< Paramètres                    */
    APP_STATE_ERROR,                        /**< Erreur système                */
    APP_STATE_SHUTDOWN,                     /**< Extinction en cours           */
} AppState_t;

/* ======================================================================== */
/*                     STRUCTURE PRINCIPALE                                  */
/* ======================================================================== */

/**
 * @brief Structure principale de l'application
 * 
 * C'EST LE POINT D'ENTRÉE UNIQUE de toute l'application.
 * Tous les composants sont accessibles depuis cette structure.
 * 
 * Taille approximative : ~15-20 Ko (avec tous les écrans alloués statiquement)
 */
typedef struct {
    /* ================================================================ */
    /*  ÉTAT GLOBAL                                                     */
    /* ================================================================ */

    AppState_t              state;              /**< État actuel                 */
    volatile bool           running;            /**< Application en cours d'exéc. */
    uint32_t                uptime_ms;          /**< Temps depuis démarrage      */
    uint32_t                last_activity_ms;   /**< Dernière activité utilisateur */

    /* ================================================================ */
    /*  IDENTITÉ DU DISPOSITIF                                          */
    /* ================================================================ */

    char                    device_name[32];    /**< Nom du téléphone            */
    char                    device_uid[25];     /**< UID unique (basé sur MCU)   */
    char                    firmware_version[16];/**< Version firmware            */
    uint16_t                network_id;         /**< ID réseau LoRa              */

    /* ================================================================ */
    /*  ÉCRANS (alloués statiquement)                                   */
    /* ================================================================ */

    ScreenSplash_t          splash_screen;      /**< Écran de démarrage          */
    ScreenLock_t            lock_screen;        /**< Écran de verrouillage       */
    ScreenHome_t            home_screen;        /**< Écran d'accueil             */
    ScreenDialer_t          dialer_screen;      /**< Composeur numérique         */
    ScreenCallActive_t      call_active_screen; /**< Appel en cours              */
    ScreenCallIncoming_t    call_incoming_screen;/**< Appel entrant              */
    ScreenCallLog_t         call_log_screen;    /**< Journal d'appels            */
    ScreenMessagesList_t    messages_screen;    /**< Liste des messages          */
    ScreenMessageCompose_t  compose_screen;     /**< Éditeur de message          */
    ScreenContactsList_t    contacts_screen;    /**< Liste des contacts          */
    ScreenContactDetail_t   contact_detail_screen;/**< Détail d'un contact       */
    ScreenSettings_t        settings_screen;    /**< Paramètres                  */
    ScreenSettingsNetwork_t network_settings;   /**< Réglages réseau             */
    ScreenSettingsAudio_t   audio_settings;     /**< Réglages audio              */
    ScreenSettingsDisplay_t display_settings;   /**< Réglages affichage          */

    /* Pointeur vers l'écran actif */
    ScreenBase_t*           active_screen;      /**< Écran actuellement affiché  */

    /* ================================================================ */
    /*  SERVICES (alloués statiquement)                                 */
    /* ================================================================ */

    PhoneService_t          phone_service;      /**< Service téléphonie          */
    SMSService_t            sms_service;        /**< Service messagerie          */
    ContactService_t        contact_service;    /**< Service contacts            */
    CallLogService_t        call_log_service;   /**< Service historique appels   */
    SettingsService_t       settings_service;   /**< Service paramètres          */
    NotificationService_t   notification_svc;   /**< Service notifications       */
    RingtoneService_t       ringtone_service;   /**< Service sonneries           */

    /* ================================================================ */
    /*  PROTOCOLES                                                      */
    /* ================================================================ */

    PacketRouter_t          packet_router;      /**< Routeur de paquets          */
    CallProtocol_t          call_protocol;      /**< Protocole d'appel           */
    SMSProtocol_t           sms_protocol;       /**< Protocole SMS               */

    /* ================================================================ */
    /*  DRIVERS                                                         */
    /* ================================================================ */

    DisplayManager_t*       display;            /**< Gestionnaire affichage      */
    LoRaDriver_t*           lora;               /**< Driver LoRa SX1278          */
    AudioManager_t*         audio;              /**< Gestionnaire audio          */
    PowerManager_t*         power;              /**< Gestionnaire énergie        */
    BacklightControl_t*     backlight;          /**< Contrôle rétroéclairage     */

    /* ================================================================ */
    /*  FILE D'ÉVÉNEMENTS                                               */
    /* ================================================================ */

    AppEvent_t              event_queue[APP_EVENT_QUEUE_SIZE];/**< File circulaire */
    volatile uint8_t        event_read_index;   /**< Index lecture               */
    volatile uint8_t        event_write_index;  /**< Index écriture              */
    volatile uint8_t        event_count;        /**< Nombre d'événements en file */

    /* ================================================================ */
    /*  TIMERS APPLICATIFS                                              */
    /* ================================================================ */

    TimerHandle_t           statusbar_timer;    /**< Mise à jour barre statut    */
    TimerHandle_t           inactivity_timer;   /**< Détection inactivité        */
    TimerHandle_t           call_check_timer;   /**< Vérification appels entrants */

    /* ================================================================ */
    /*  CALLBACKS                                                       */
    /* ================================================================ */

    void (*on_error)(const char* message);      /**< Erreur fatale               */
    void (*on_shutdown)(void);                  /**< Extinction                  */

} PhoneApp_t;

/* ======================================================================== */
/*              VARIABLE GLOBALE (singleton)                                 */
/* ======================================================================== */

/**
 * @brief Instance globale unique de l'application
 * 
 * Déclarée dans phone_app.cpp :
 *   PhoneApp_t g_app;
 * 
 * Accessible depuis tous les fichiers via :
 *   extern PhoneApp_t g_app;
 */
extern PhoneApp_t g_app;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/* ---- Cycle de vie ---- */

/**
 * @brief Initialise l'application complète
 * 
 * Ordre d'initialisation :
 * 1. HAL et horloge
 * 2. Drivers (Display, LoRa, Audio, Power)
 * 3. Services (Settings, Contacts, CallLog)
 * 4. Écrans (Splash, Lock, Home, ...)
 * 5. Protocoles (Call, SMS)
 * 6. File d'événements
 * 7. Timers applicatifs
 * 
 * @param app   Pointeur vers la structure applicative
 */
void PhoneApp_Init(PhoneApp_t* app);

/**
 * @brief Lance la boucle principale
 * 
 * Boucle infinie qui :
 * - Traite les événements en file
 * - Met à jour l'écran actif
 * - Traite les services
 * - Vérifie les appels entrants
 * 
 * @param app   Pointeur vers la structure applicative
 */
void PhoneApp_Run(PhoneApp_t* app);

/**
 * @brief Arrête l'application proprement
 * @param app   Pointeur vers la structure applicative
 */
void PhoneApp_Shutdown(PhoneApp_t* app);

/* ---- Événements ---- */

/**
 * @brief Ajoute un événement dans la file
 * 
 * Thread-safe : peut être appelée depuis une interruption.
 * 
 * @param app       Application
 * @param type      Type d'événement
 * @param priority  Priorité
 * @return          true si ajouté avec succès (file non pleine)
 */
bool PhoneApp_PostEvent(PhoneApp_t* app,
                        AppEventType_t type,
                        AppEventPriority_t priority);

/**
 * @brief Récupère le prochain événement de la file
 * 
 * @param app       Application
 * @param event     [out] Événement dépilé
 * @return          true si un événement était disponible
 */
bool PhoneApp_GetEvent(PhoneApp_t* app, AppEvent_t* event);

/**
 * @brief Traite tous les événements en attente
 * @param app       Application
 */
void PhoneApp_ProcessEvents(PhoneApp_t* app);

/**
 * @brief Traite un seul événement
 * @param app       Application
 * @param event     Événement à traiter
 */
void PhoneApp_HandleEvent(PhoneApp_t* app, const AppEvent_t* event);

/* ---- Navigation ---- */

/**
 * @brief Change l'écran actif
 * 
 * Masque l'écran précédent, affiche le nouveau.
 * 
 * @param app           Application
 * @param new_screen    Nouvel écran à afficher
 */
void PhoneApp_SwitchScreen(PhoneApp_t* app, ScreenBase_t* new_screen);

/**
 * @brief Retourne à l'écran précédent (navigation pile)
 * @param app           Application
 */
void PhoneApp_GoBack(PhoneApp_t* app);

/**
 * @brief Retourne à l'écran d'accueil
 * @param app           Application
 */
void PhoneApp_GoHome(PhoneApp_t* app);

/* ---- Gestion des appels ---- */

/**
 * @brief Appel entrant détecté
 * @param app           Application
 * @param caller_id     Identifiant de l'appelant
 * @param caller_number Numéro de l'appelant
 */
void PhoneApp_OnIncomingCall(PhoneApp_t* app,
                             const char* caller_id,
                             const char* caller_number);

/**
 * @brief Appel accepté par l'utilisateur
 * @param app           Application
 */
void PhoneApp_OnCallAccepted(PhoneApp_t* app);

/**
 * @brief Appel refusé par l'utilisateur
 * @param app           Application
 */
void PhoneApp_OnCallRejected(PhoneApp_t* app);

/**
 * @brief Appel terminé
 * @param app           Application
 */
void PhoneApp_OnCallEnded(PhoneApp_t* app);

/**
 * @brief Initie un appel sortant
 * @param app           Application
 * @param number        Numéro à appeler
 */
void PhoneApp_StartOutgoingCall(PhoneApp_t* app, const char* number);

/* ---- Gestion des messages ---- */

/**
 * @brief Nouveau message reçu
 * @param app           Application
 * @param sender        Expéditeur
 * @param content       Contenu
 */
void PhoneApp_OnNewMessage(PhoneApp_t* app,
                           const char* sender,
                           const char* content);

/* ---- Utilitaires ---- */

/**
 * @brief Retourne le temps d'inactivité en secondes
 * @param app           Application
 * @return              Secondes depuis la dernière activité
 */
uint32_t PhoneApp_GetIdleTime(PhoneApp_t* app);

/**
 * @brief Réinitialise le timer d'inactivité
 * 
 * Appelé à chaque interaction utilisateur (touch, touche).
 * 
 * @param app           Application
 */
void PhoneApp_ResetActivity(PhoneApp_t* app);

/**
 * @brief Vérifie si l'écran est verrouillé
 * @param app           Application
 * @return              true si verrouillé
 */
bool PhoneApp_IsLocked(PhoneApp_t* app);

/**
 * @brief Obtient le service spécifié
 * @param app           Application
 * @return              Pointeur vers le service
 */
SettingsService_t*  PhoneApp_GetSettings(PhoneApp_t* app);
ContactService_t*   PhoneApp_GetContacts(PhoneApp_t* app);
CallLogService_t*   PhoneApp_GetCallLog(PhoneApp_t* app);

/* ======================================================================== */
/*              MACROS UTILITAIRES                                           */
/* ======================================================================== */

/**
 * @brief Accès rapide à l'instance globale
 */
#define APP                             (&g_app)

/**
 * @brief Accès rapide aux services
 */
#define APP_SETTINGS                    (&g_app.settings_service)
#define APP_CONTACTS                    (&g_app.contact_service)
#define APP_CALL_LOG                    (&g_app.call_log_service)
#define APP_PHONE                       (&g_app.phone_service)
#define APP_SMS                         (&g_app.sms_service)

/**
 * @brief Poste un événement simple
 */
#define APP_POST_EVENT(type, prio)      PhoneApp_PostEvent(APP, type, prio)

/**
 * @brief Vérifie si l'application est en cours d'exécution
 */
#define APP_IS_RUNNING                  (g_app.running)

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */