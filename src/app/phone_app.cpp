/**
 * @file    phone_app.cpp
 * @brief   Implémentation de l'application principale
 * @author  Votre Nom
 * @date    2026
 *
 * Implémente le cycle de vie complet de l'application téléphone LoRa.
 *
 * ORDRE D'INITIALISATION (CRITIQUE - NE PAS MODIFIER) :
 *
 * 1. HAL et horloge système (180 MHz)
 * 2. SDRAM (8 Mo) via FMC
 * 3. Drivers :
 *    a. Display (LTDC + DMA2D + SDRAM framebuffers)
 *    b. Backlight (PWM TIM1)
 *    c. LoRa (SPI + SX1278)
 *    d. Audio (ADC + DAC + DMA)
 *    e. Power (modes veille)
 * 4. Services :
 *    a. Settings (doit être premier car les autres en dépendent)
 *    b. Contacts
 *    c. CallLog
 *    d. Phone
 *    e. SMS
 *    f. Notifications
 *    g. Ringtone
 * 5. Protocoles :
 *    a. PacketRouter
 *    b. CallProtocol
 *    c. SMSProtocol
 * 6. Écrans :
 *    a. Splash (affiche la progression)
 *    b. Lock
 *    c. Home
 *    d. Dialer, CallActive, CallIncoming
 *    e. CallLog, Messages, Contacts
 *    f. Settings et sous-écrans
 * 7. Timers applicatifs
 * 8. Démarrage boucle principale
 *
 * BOUCLE PRINCIPALE (super-loop) :
 *
 * while (running) {
 *     1. PhoneApp_ProcessEvents()     → Traiter les événements en file
 *     2. PhoneApp_UpdateServices()    → Services (call check, timers)
 *     3. Screen_Update(active_screen) → Rafraîchir l'écran actif
 *     4. HAL_Delay(APP_LOOP_DELAY_MS) → Pause (ou yield si FreeRTOS)
 * }
 *
 * GESTION DES INTERRUPTIONS :
 *
 * - EXTI (Touch IRQ, DIO0 LoRa) → Post événement dans la file
 * - DMA (Audio ADC/DAC)          → Buffer circulaire
 * - TIM (Tick 1ms)               → Incrémentation uptime
 * - SPI (LoRa TX/RX)             → DMA transfer complete
 *
 * Les ISR ne font QUE poster des événements (aucun traitement lourd).
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "phone_app.h"
#include "app_state_machine.h"
#include "app_events.h"
#include "app_tasks.h"
#include "app_watchdog.h"

/* HAL */
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_tim.h"

/* Utilitaires */
#include "../utils/debug_utils.h"
#include "../utils/timer_utils.h"
#include "../utils/string_utils.h"

/* Standard */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs */
#define TAG                                 "PhoneApp"

/** Timeout chien de garde (ms) */
#define WATCHDOG_TIMEOUT_MS                 5000

/** Délai après reset avant redémarrage (ms) */
#define SHUTDOWN_DELAY_MS                   1500

/** Seuil batterie critique (arrêt) */
#define BATTERY_CRITICAL_PERCENT            3

/** Seuil batterie faible (alerte) */
#define BATTERY_LOW_PERCENT                 15

/** Seuil inactivité avant verrouillage (secondes) */
#define INACTIVITY_LOCK_TIMEOUT_SEC         30

/* ======================================================================== */
/*                VARIABLES GLOBALES                                        */
/* ======================================================================== */

/**
 * @brief Instance globale unique de l'application
 *
 * Déclarée extern dans phone_app.h.
 * C'est LE singleton central de tout le firmware.
 */
PhoneApp_t g_app;

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

/* --- Initialisation --- */
static void init_hal(void);
static void init_sdram(void);
static void init_drivers(PhoneApp_t* app);
static void init_services(PhoneApp_t* app);
static void init_protocols(PhoneApp_t* app);
static void init_screens(PhoneApp_t* app);
static void init_timers(PhoneApp_t* app);

/* --- Boucle principale --- */
static void process_events(PhoneApp_t* app);
static void update_services(PhoneApp_t* app);
static void check_incoming_calls(PhoneApp_t* app);
static void check_battery(PhoneApp_t* app);
static void check_inactivity(PhoneApp_t* app);

/* --- Gestion événements --- */
static void handle_touch_event(PhoneApp_t* app, const AppEvent_t* event);
static void handle_key_event(PhoneApp_t* app, const AppEvent_t* event);
static void handle_lora_event(PhoneApp_t* app, const AppEvent_t* event);
static void handle_call_event(PhoneApp_t* app, const AppEvent_t* event);
static void handle_message_event(PhoneApp_t* app, const AppEvent_t* event);
static void handle_system_event(PhoneApp_t* app, const AppEvent_t* event);

/* --- Callbacks écrans --- */
static void on_splash_finished(void);
static void on_lock_unlocked(void);
static void on_home_dialer_requested(void);
static void on_home_settings_requested(void);
static void on_home_call_log_requested(void);
static void on_home_contacts_requested(void);
static void on_home_messages_requested(void);
static void on_call_ended(void);
static void on_settings_back(void);

/* --- Callbacks services --- */
static void on_incoming_call_detected(const char* caller_id,
                                      const char* caller_number);
static void on_new_message_received(const char* sender,
                                    const char* content);

/* --- Timers --- */
static void statusbar_timer_callback(TimerHandle_t timer);
static void inactivity_timer_callback(TimerHandle_t timer);
static void call_check_timer_callback(TimerHandle_t timer);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise l'application complète
 *
 * Cette fonction est appelée une seule fois au démarrage.
 * Elle initialise TOUS les composants dans l'ordre strict défini.
 */
void PhoneApp_Init(PhoneApp_t* app)
{
    if (!app) return;

    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  LoRa Phone - Démarrage v%s", FIRMWARE_VERSION);
    DEBUG_INFO(TAG, "  STM32F429 @ 180 MHz");
    DEBUG_INFO(TAG, "========================================");

    /* Mise à zéro complète */
    memset(app, 0, sizeof(PhoneApp_t));

    /* État initial */
    app->state = APP_STATE_INIT;
    app->running = true;
    app->uptime_ms = 0;
    app->last_activity_ms = 0;

    /* Identité */
    strncpy(app->device_name, DEVICE_DEFAULT_NAME, sizeof(app->device_name) - 1);
    strncpy(app->firmware_version, FIRMWARE_VERSION, sizeof(app->firmware_version) - 1);

    /* ---- Étape 1 : HAL et horloge ---- */
    DEBUG_INFO(TAG, "[1/8] Initialisation HAL...");
    init_hal();

    /* ---- Étape 2 : SDRAM ---- */
    DEBUG_INFO(TAG, "[2/8] Initialisation SDRAM...");
    init_sdram();

    /* ---- Étape 3 : Drivers ---- */
    DEBUG_INFO(TAG, "[3/8] Initialisation drivers...");
    init_drivers(app);

    /* ---- Étape 4 : Services ---- */
    DEBUG_INFO(TAG, "[4/8] Initialisation services...");
    init_services(app);

    /* ---- Étape 5 : Protocoles ---- */
    DEBUG_INFO(TAG, "[5/8] Initialisation protocoles...");
    init_protocols(app);

    /* ---- Étape 6 : Écrans ---- */
    DEBUG_INFO(TAG, "[6/8] Initialisation écrans...");
    init_screens(app);

    /* ---- Étape 7 : Timers applicatifs ---- */
    DEBUG_INFO(TAG, "[7/8] Initialisation timers...");
    init_timers(app);

    /* ---- Étape 8 : Finalisation ---- */
    DEBUG_INFO(TAG, "[8/8] Système prêt !");

    /* Configuration des callbacks */
    app->splash_screen.on_finished = on_splash_finished;
    app->lock_screen.on_unlocked = on_lock_unlocked;

    /* Démarrer avec le splash screen */
    app->active_screen = (ScreenBase_t*)&app->splash_screen;
    ScreenSplash_Show(&app->splash_screen);

    /* Simuler les étapes d'initialisation sur le splash */
    ScreenSplash_SetStep(&app->splash_screen, "Demarrage processeur...", 5);
    ScreenSplash_SetStep(&app->splash_screen, "Horloge 180 MHz configuree", 15);
    ScreenSplash_SetStep(&app->splash_screen, "SDRAM 8 Mo initialisee", 25);
    ScreenSplash_SetStep(&app->splash_screen, "Ecran LTDC initialise", 35);
    ScreenSplash_SetStep(&app->splash_screen, "Module LoRa SX1278 pret", 50);
    ScreenSplash_SetStep(&app->splash_screen, "Audio ADC/DAC configure", 65);
    ScreenSplash_SetStep(&app->splash_screen, "Services demarres", 80);
    ScreenSplash_SetStep(&app->splash_screen, "Parametres charges", 90);
    ScreenSplash_SetStep(&app->splash_screen, "Prer !", 100);
    ScreenSplash_Complete(&app->splash_screen);

    app->state = APP_STATE_SPLASH;

    DEBUG_INFO(TAG, "Initialisation terminée - Démarrage boucle principale");
}

/**
 * @brief Lance la boucle principale
 *
 * Boucle infinie qui traite les événements et met à jour l'écran.
 * Ne retourne jamais sauf en cas d'erreur fatale ou shutdown.
 */
void PhoneApp_Run(PhoneApp_t* app)
{
    if (!app) return;

    DEBUG_INFO(TAG, "Boucle principale démarrée");

    while (app->running) {
        /* Réarmer le chien de garde */
        AppWatchdog_Refresh();

        /* 1. Traiter les événements en attente */
        process_events(app);

        /* 2. Mettre à jour les services */
        update_services(app);

        /* 3. Mettre à jour l'écran actif */
        if (app->active_screen && app->active_screen->is_visible) {
            ScreenBase_Update(app->active_screen);
        }

        /* 4. Vérifier les appels entrants */
        check_incoming_calls(app);

        /* 5. Vérifier la batterie */
        check_battery(app);

        /* 6. Vérifier l'inactivité */
        check_inactivity(app);

        /* 7. Pause (évite de saturer le CPU) */
        HAL_Delay(APP_LOOP_DELAY_MS);

        /* Incrémenter l'uptime */
        app->uptime_ms += APP_LOOP_DELAY_MS;
    }

    /* Sortie de boucle = shutdown */
    DEBUG_INFO(TAG, "Boucle principale terminée - Extinction");
    PhoneApp_Shutdown(app);
}

/**
 * @brief Arrête l'application proprement
 */
void PhoneApp_Shutdown(PhoneApp_t* app)
{
    if (!app) return;

    DEBUG_INFO(TAG, "Arrêt de l'application...");

    app->state = APP_STATE_SHUTDOWN;
    app->running = false;

    /* Sauvegarder les paramètres */
    if (SettingsService_IsDirty(&app->settings_service)) {
        SettingsService_Save(&app->settings_service);
    }

    /* Éteindre l'écran */
    BacklightControl_SetPWM(app->backlight, 0);
    Display_FillRect(0, 0, 320, 480, 0x0000);
    Display_SwapBuffers();

    /* Notification shutdown */
    if (app->on_shutdown) {
        app->on_shutdown();
    }

    /* Attendre et redémarrer ou éteindre */
    HAL_Delay(SHUTDOWN_DELAY_MS);
    HAL_NVIC_SystemReset();
}

/* ---- Événements ---- */

/**
 * @brief Ajoute un événement dans la file
 */
bool PhoneApp_PostEvent(PhoneApp_t* app,
                        AppEventType_t type,
                        AppEventPriority_t priority)
{
    if (!app) return false;

    /* Vérifier si la file est pleine */
    if (app->event_count >= APP_EVENT_QUEUE_SIZE) {
        DEBUG_WARN(TAG, "File d'événements pleine ! Événement %d perdu", type);
        return false;
    }

    /* Désactiver les interruptions pour l'atomicité */
    __disable_irq();

    /* Écrire l'événement */
    AppEvent_t* event = &app->event_queue[app->event_write_index];
    memset(event, 0, sizeof(AppEvent_t));
    event->type = type;
    event->priority = priority;
    event->timestamp_ms = app->uptime_ms;

    /* Avancer l'index d'écriture (buffer circulaire) */
    app->event_write_index = (app->event_write_index + 1) % APP_EVENT_QUEUE_SIZE;
    app->event_count++;

    __enable_irq();

    return true;
}

/**
 * @brief Récupère le prochain événement
 */
bool PhoneApp_GetEvent(PhoneApp_t* app, AppEvent_t* event)
{
    if (!app || !event) return false;

    if (app->event_count == 0) {
        return false;
    }

    __disable_irq();

    /* Lire l'événement */
    memcpy(event, &app->event_queue[app->event_read_index], sizeof(AppEvent_t));

    /* Avancer l'index de lecture */
    app->event_read_index = (app->event_read_index + 1) % APP_EVENT_QUEUE_SIZE;
    app->event_count--;

    __enable_irq();

    return true;
}

/**
 * @brief Traite tous les événements en attente
 */
void PhoneApp_ProcessEvents(PhoneApp_t* app)
{
    if (!app) return;

    AppEvent_t event;

    /* Traiter jusqu'à 5 événements par itération (évite de bloquer) */
    for (int i = 0; i < 5; i++) {
        if (PhoneApp_GetEvent(app, &event)) {
            PhoneApp_HandleEvent(app, &event);
        } else {
            break;
        }
    }
}

/**
 * @brief Traite un seul événement
 */
void PhoneApp_HandleEvent(PhoneApp_t* app, const AppEvent_t* event)
{
    if (!app || !event) return;

    DEBUG_VERBOSE(TAG, "Traitement événement: %d (priorité %d)",
                  event->type, event->priority);

    switch (event->type) {
        /* Événements tactiles */
        case APP_EVENT_TOUCH:
        case APP_EVENT_TOUCH_RELEASE:
        case APP_EVENT_TOUCH_SWIPE:
            handle_touch_event(app, event);
            break;

        /* Événements clavier */
        case APP_EVENT_KEY_PRESS:
        case APP_EVENT_KEY_LONG_PRESS:
            handle_key_event(app, event);
            break;

        /* Événements LoRa */
        case APP_EVENT_LORA_PACKET_RECEIVED:
        case APP_EVENT_LORA_TX_COMPLETE:
        case APP_EVENT_LORA_RX_TIMEOUT:
        case APP_EVENT_LORA_ERROR:
            handle_lora_event(app, event);
            break;

        /* Événements téléphonie */
        case APP_EVENT_INCOMING_CALL:
        case APP_EVENT_CALL_ACCEPTED:
        case APP_EVENT_CALL_REJECTED:
        case APP_EVENT_CALL_ENDED:
        case APP_EVENT_CALL_MISSED:
            handle_call_event(app, event);
            break;

        /* Événements messages */
        case APP_EVENT_NEW_MESSAGE:
        case APP_EVENT_MESSAGE_SENT:
            handle_message_event(app, event);
            break;

        /* Événements système */
        case APP_EVENT_BATTERY_LOW:
        case APP_EVENT_BATTERY_CRITICAL:
        case APP_EVENT_CHARGING_START:
        case APP_EVENT_CHARGING_STOP:
        case APP_EVENT_SCREEN_TIMEOUT:
        case APP_EVENT_SYSTEM_ERROR:
            handle_system_event(app, event);
            break;

        default:
            DEBUG_WARN(TAG, "Événement inconnu: %d", event->type);
            break;
    }
}

/* ---- Navigation ---- */

/**
 * @brief Change l'écran actif
 */
void PhoneApp_SwitchScreen(PhoneApp_t* app, ScreenBase_t* new_screen)
{
    if (!app || !new_screen) return;

    DEBUG_INFO(TAG, "Changement écran: %s → %s",
               app->active_screen ? app->active_screen->name : "NONE",
               new_screen->name);

    /* Masquer l'écran précédent */
    if (app->active_screen && app->active_screen->is_visible) {
        if (app->active_screen->hide) {
            app->active_screen->hide(app->active_screen);
        }
    }

    /* Afficher le nouvel écran */
    app->active_screen = new_screen;
    if (new_screen->show) {
        new_screen->show(new_screen);
    }

    /* Réinitialiser le timer d'inactivité */
    PhoneApp_ResetActivity(app);
}

/**
 * @brief Retour à l'écran précédent
 */
void PhoneApp_GoBack(PhoneApp_t* app)
{
    if (!app) return;

    DEBUG_INFO(TAG, "Navigation: Retour");

    /* La pile de navigation est gérée par UINavigation */
    ScreenBase_t* previous = UINavigation_Pop();
    if (previous) {
        PhoneApp_SwitchScreen(app, previous);
    } else {
        /* Plus d'écran dans la pile → Home */
        PhoneApp_GoHome(app);
    }
}

/**
 * @brief Retour à l'écran d'accueil
 */
void PhoneApp_GoHome(PhoneApp_t* app)
{
    if (!app) return;

    DEBUG_INFO(TAG, "Navigation: Accueil");

    /* Si verrouillé, afficher l'écran de verrouillage */
    if (app->state == APP_STATE_LOCKED) {
        PhoneApp_SwitchScreen(app, (ScreenBase_t*)&app->lock_screen);
        return;
    }

    PhoneApp_SwitchScreen(app, (ScreenBase_t*)&app->home_screen);
    app->state = APP_STATE_IDLE;
}

/* ---- Gestion des appels ---- */

void PhoneApp_OnIncomingCall(PhoneApp_t* app,
                             const char* caller_id,
                             const char* caller_number)
{
    if (!app) return;

    DEBUG_INFO(TAG, "Appel entrant: %s (%s)", caller_id, caller_number);

    app->state = APP_STATE_INCOMING_CALL;

    /* Configurer l'écran d'appel entrant */
    ScreenCallIncoming_SetCaller(&app->call_incoming_screen,
                                 caller_id, caller_number);

    /* Afficher l'écran d'appel entrant (même si verrouillé) */
    PhoneApp_SwitchScreen(app, (ScreenBase_t*)&app->call_incoming_screen);

    /* Jouer la sonnerie */
    RingtoneService_Play(&app->ringtone_service);

    /* Vibreur */
    NotificationService_Vibrate(&app->notification_svc, 500);
}

void PhoneApp_OnCallAccepted(PhoneApp_t* app)
{
    if (!app) return;

    DEBUG_INFO(TAG, "Appel accepté");

    app->state = APP_STATE_IN_CALL;

    /* Arrêter la sonnerie */
    RingtoneService_Stop(&app->ringtone_service);

    /* Afficher l'écran d'appel actif */
    ScreenCallActive_StartCall(&app->call_active_screen);
    PhoneApp_SwitchScreen(app, (ScreenBase_t*)&app->call_active_screen);
}

void PhoneApp_OnCallRejected(PhoneApp_t* app)
{
    if (!app) return;

    DEBUG_INFO(TAG, "Appel refusé");

    /* Arrêter la sonnerie */
    RingtoneService_Stop(&app->ringtone_service);

    /* Envoyer le rejet via LoRa */
    CallProtocol_SendReject(&app->call_protocol);

    /* Retour à l'écran précédent */
    PhoneApp_GoHome(app);
}

void PhoneApp_OnCallEnded(PhoneApp_t* app)
{
    if (!app) return;

    DEBUG_INFO(TAG, "Appel terminé");

    app->state = APP_STATE_IDLE;

    /* Enregistrer dans l'historique */
    CallLogService_AddEntry(&app->call_log_service,
                           ScreenCallActive_GetRemoteNumber(&app->call_active_screen),
                           ScreenCallActive_GetRemoteName(&app->call_active_screen),
                           ScreenCallActive_GetDuration(&app->call_active_screen),
                           CALL_TYPE_OUTGOING);

    /* Retour à l'accueil */
    PhoneApp_GoHome(app);
}

void PhoneApp_StartOutgoingCall(PhoneApp_t* app, const char* number)
{
    if (!app || !number) return;

    DEBUG_INFO(TAG, "Appel sortant: %s", number);

    app->state = APP_STATE_DIALING;

    /* Envoyer la demande d'appel via LoRa */
    CallProtocol_SendCallRequest(&app->call_protocol, number);

    /* Afficher l'écran d'appel actif (mode appel sortant) */
    ScreenCallActive_OutgoingCall(&app->call_active_screen, number);
    PhoneApp_SwitchScreen(app, (ScreenBase_t*)&app->call_active_screen);
}

/* ---- Gestion des messages ---- */

void PhoneApp_OnNewMessage(PhoneApp_t* app,
                           const char* sender,
                           const char* content)
{
    if (!app) return;

    DEBUG_INFO(TAG, "Nouveau message de: %s", sender);

    /* Sauvegarder le message */
    SMSService_AddMessage(&app->sms_service, sender, content, false);

    /* Notifier l'utilisateur */
    NotificationService_Notify(&app->notification_svc,
                              "Nouveau message",
                              sender);

    /* Rafraîchir l'écran de messages s'il est visible */
    if (app->active_screen == (ScreenBase_t*)&app->messages_screen) {
        ScreenMessagesList_Refresh(&app->messages_screen);
    }
}

/* ---- Utilitaires ---- */

uint32_t PhoneApp_GetIdleTime(PhoneApp_t* app)
{
    if (!app) return 0;
    return (app->uptime_ms - app->last_activity_ms) / 1000;
}

void PhoneApp_ResetActivity(PhoneApp_t* app)
{
    if (!app) return;
    app->last_activity_ms = app->uptime_ms;
}

bool PhoneApp_IsLocked(PhoneApp_t* app)
{
    if (!app) return false;
    return app->state == APP_STATE_LOCKED;
}

SettingsService_t* PhoneApp_GetSettings(PhoneApp_t* app)
{
    return app ? &app->settings_service : NULL;
}

ContactService_t* PhoneApp_GetContacts(PhoneApp_t* app)
{
    return app ? &app->contact_service : NULL;
}

CallLogService_t* PhoneApp_GetCallLog(PhoneApp_t* app)
{
    return app ? &app->call_log_service : NULL;
}

/* ======================================================================== */
/*              INITIALISATION                                              */
/* ======================================================================== */

/**
 * @brief Initialise le HAL STM32
 */
static void init_hal(void)
{
    /* Priorité du groupe NVIC */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    /* Initialiser le systick (1ms) */
    HAL_Init();

    /* Configurer l'horloge système à 180 MHz */
    SystemClock_Config();

    /* Chien de garde indépendant */
    AppWatchdog_Init(WATCHDOG_TIMEOUT_MS);
}

/**
 * @brief Initialise la SDRAM via FMC
 */
static void init_sdram(void)
{
    /* La SDRAM est initialisée par le BSP ou CubeMX generated code */
    /* BSP_SDRAM_Init() ou MX_FMC_Init() */
}

/**
 * @brief Initialise tous les drivers
 */
static void init_drivers(PhoneApp_t* app)
{
    if (!app) return;

    /* 1. Affichage (LTDC + DMA2D) */
    app->display = DisplayManager_GetInstance();
    DisplayManager_Init(app->display);

    /* 2. Rétroéclairage */
    app->backlight = BacklightControl_GetInstance();
    BacklightControl_Init(app->backlight);
    BacklightControl_SetPWM(app->backlight, 4095);  /* Pleine luminosité */

    /* 3. LoRa SX1278 */
    app->lora = LoRaDriver_GetInstance();
    LoRaDriver_Init(app->lora);
    LoRaDriver_SetFrequency(app->lora, 868000000);  /* 868 MHz */
    LoRaDriver_SetTxPower(app->lora, 20);           /* 20 dBm */
    LoRaDriver_SetSpreadingFactor(app->lora, 7);    /* SF7 */
    LoRaDriver_SetBandwidth(app->lora, 250000);     /* 250 kHz */
    LoRaDriver_SetCodingRate(app->lora, 5);         /* 4/5 */

    /* 4. Audio */
    app->audio = AudioManager_GetInstance();
    AudioManager_Init(app->audio);

    /* 5. Gestion énergie */
    app->power = PowerManager_GetInstance();
    PowerManager_Init(app->power);
}

/**
 * @brief Initialise tous les services
 */
static void init_services(PhoneApp_t* app)
{
    if (!app) return;

    /* 1. Settings (doit être premier) */
    SettingsService_Init(&app->settings_service);
    SettingsService_Load(&app->settings_service);

    /* 2. Contacts */
    ContactService_Init(&app->contact_service);
    ContactService_Load(&app->contact_service, &app->settings_service);

    /* 3. Journal d'appels */
    CallLogService_Init(&app->call_log_service);
    CallLogService_Load(&app->call_log_service, &app->settings_service);

    /* 4. Téléphonie */
    PhoneService_Init(&app->phone_service);
    PhoneService_SetOnIncomingCall(&app->phone_service, on_incoming_call_detected);

    /* 5. SMS */
    SMSService_Init(&app->sms_service);
    SMSService_SetOnNewMessage(&app->sms_service, on_new_message_received);

    /* 6. Notifications */
    NotificationService_Init(&app->notification_svc);

    /* 7. Sonneries */
    RingtoneService_Init(&app->ringtone_service);
}

/**
 * @brief Initialise les protocoles de communication
 */
static void init_protocols(PhoneApp_t* app)
{
    if (!app) return;

    /* Routeur de paquets */
    PacketRouter_Init(&app->packet_router, app->lora);

    /* Protocole d'appel */
    CallProtocol_Init(&app->call_protocol, &app->packet_router);
    CallProtocol_SetOnIncomingCall(&app->call_protocol, on_incoming_call_detected);

    /* Protocole SMS */
    SMSProtocol_Init(&app->sms_protocol, &app->packet_router);
    SMSProtocol_SetOnNewMessage(&app->sms_protocol, on_new_message_received);
}

/**
 * @brief Initialise tous les écrans
 */
static void init_screens(PhoneApp_t* app)
{
    if (!app) return;

    DEBUG_INFO(TAG, "Initialisation des écrans...");

    /* Splash */
    ScreenSplash_Init(&app->splash_screen);
    app->splash_screen.on_finished = on_splash_finished;

    /* Verrouillage */
    ScreenLock_Init(&app->lock_screen, &app->settings_service);
    app->lock_screen.on_unlocked = on_lock_unlocked;

    /* Accueil */
    ScreenHome_Init(&app->home_screen);
    app->home_screen.on_dialer_requested = on_home_dialer_requested;
    app->home_screen.on_settings_requested = on_home_settings_requested;
    app->home_screen.on_call_log_requested = on_home_call_log_requested;
    app->home_screen.on_contacts_requested = on_home_contacts_requested;
    app->home_screen.on_messages_requested = on_home_messages_requested;

    /* Composeur */
    ScreenDialer_Init(&app->dialer_screen);
    app->dialer_screen.on_call_requested = 
        (void(*)(void*, const char*))PhoneApp_StartOutgoingCall;

    /* Appel actif */
    ScreenCallActive_Init(&app->call_active_screen);
    app->call_active_screen.on_call_ended = on_call_ended;

    /* Appel entrant */
    ScreenCallIncoming_Init(&app->call_incoming_screen);
    app->call_incoming_screen.on_accept = 
        (void(*)(void*))PhoneApp_OnCallAccepted;
    app->call_incoming_screen.on_reject = 
        (void(*)(void*))PhoneApp_OnCallRejected;

    /* Journal d'appels */
    ScreenCallLog_Init(&app->call_log_screen,
                       &app->call_log_service,
                       &app->contact_service);

    /* Messages */
    ScreenMessagesList_Init(&app->messages_screen, &app->sms_service);
    ScreenMessageCompose_Init(&app->compose_screen, &app->sms_service);

    /* Contacts */
    ScreenContactsList_Init(&app->contacts_screen, &app->contact_service);
    ScreenContactDetail_Init(&app->contact_detail_screen, &app->contact_service);

    /* Paramètres */
    ScreenSettings_Init(&app->settings_screen,
                        &app->settings_service,
                        app->backlight);
    app->settings_screen.on_back_pressed = on_settings_back;

    ScreenSettingsNetwork_Init(&app->network_settings,
                               &app->settings_service);
    ScreenSettingsAudio_Init(&app->audio_settings,
                             &app->settings_service);
    ScreenSettingsDisplay_Init(&app->display_settings,
                               &app->settings_service,
                               app->backlight);

    DEBUG_INFO(TAG, "Tous les écrans initialisés");
}

/**
 * @brief Initialise les timers applicatifs
 */
static void init_timers(PhoneApp_t* app)
{
    if (!app) return;

    /* Timer barre de statut (1 seconde) */
    app->statusbar_timer = Timer_Create("StatusBar",
                                        APP_STATUSBAR_UPDATE_MS,
                                        true,
                                        statusbar_timer_callback,
                                        app);

    /* Timer inactivité (1 seconde) */
    app->inactivity_timer = Timer_Create("Inactivity",
                                         1000,
                                         true,
                                         inactivity_timer_callback,
                                         app);

    /* Timer vérification appels (100ms) */
    app->call_check_timer = Timer_Create("CallCheck",
                                         APP_CALL_CHECK_INTERVAL_MS,
                                         true,
                                         call_check_timer_callback,
                                         app);

    /* Démarrer les timers */
    Timer_Start(app->statusbar_timer);
    Timer_Start(app->inactivity_timer);
    Timer_Start(app->call_check_timer);
}

/* ======================================================================== */
/*              BOUCLE PRINCIPALE (INTERNE)                                 */
/* ======================================================================== */

static void process_events(PhoneApp_t* app)
{
    if (!app) return;

    /* Traiter tous les événements en file (max 10 par itération) */
    AppEvent_t event;
    int processed = 0;

    while (processed < 10 && PhoneApp_GetEvent(app, &event)) {
        PhoneApp_HandleEvent(app, &event);
        processed++;
    }
}

static void update_services(PhoneApp_t* app)
{
    if (!app) return;

    /* Traiter les timers expirés */
    Timer_ProcessExpired();

    /* Mettre à jour le service téléphonie */
    PhoneService_Update(&app->phone_service);

    /* Mettre à jour le service notifications */
    NotificationService_Update(&app->notification_svc);
}

static void check_incoming_calls(PhoneApp_t* app)
{
    if (!app || !app->lora) return;

    /* Vérifier si un paquet d'appel entrant est en attente */
    if (app->state != APP_STATE_IN_CALL && 
        app->state != APP_STATE_INCOMING_CALL) {
        CallProtocol_CheckIncoming(&app->call_protocol);
    }
}

static void check_battery(PhoneApp_t* app)
{
    if (!app || !app->power) return;

    static uint32_t last_check = 0;
    uint32_t now = app->uptime_ms;

    /* Vérifier toutes les 10 secondes */
    if (now - last_check < 10000) return;
    last_check = now;

    uint8_t battery_percent = PowerManager_GetBatteryPercent(app->power);
    bool is_charging = PowerManager_IsCharging(app->power);

    if (battery_percent <= BATTERY_CRITICAL_PERCENT && !is_charging) {
        DEBUG_WARN(TAG, "Batterie critique: %d%% - Arrêt imminent", battery_percent);
        PhoneApp_PostEvent(app, APP_EVENT_BATTERY_CRITICAL, APP_PRIORITY_CRITICAL);
    } else if (battery_percent <= BATTERY_LOW_PERCENT && !is_charging) {
        PhoneApp_PostEvent(app, APP_EVENT_BATTERY_LOW, APP_PRIORITY_HIGH);
    }
}

static void check_inactivity(PhoneApp_t* app)
{
    if (!app) return;

    /* Vérifier si le verrouillage auto est activé */
    if (app->state == APP_STATE_IDLE || 
        app->state == APP_STATE_ACTIVE) {
        uint32_t idle_sec = PhoneApp_GetIdleTime(app);
        if (idle_sec >= INACTIVITY_LOCK_TIMEOUT_SEC) {
            DEBUG_INFO(TAG, "Verrouillage auto après %lu secondes", idle_sec);
            app->state = APP_STATE_LOCKED;
            PhoneApp_SwitchScreen(app, (ScreenBase_t*)&app->lock_screen);
        }
    }
}

/* ======================================================================== */
/*              GESTION ÉVÉNEMENTS (INTERNE)                                */
/* ======================================================================== */

static void handle_touch_event(PhoneApp_t* app, const AppEvent_t* event)
{
    if (!app || !app->active_screen || !event) return;

    /* Réinitialiser le timer d'inactivité */
    PhoneApp_ResetActivity(app);

    /* Déléguer à l'écran actif */
    if (app->active_screen->handle_touch) {
        TouchEvent_t touch = {
            .x = event->data.touch.x,
            .y = event->data.touch.y,
            .type = (event->type == APP_EVENT_TOUCH) ? TOUCH_EVENT_TAP :
                    (event->type == APP_EVENT_TOUCH_SWIPE) ? TOUCH_EVENT_SWIPE_UP :
                    TOUCH_EVENT_RELEASE
        };
        app->active_screen->handle_touch(app->active_screen, &touch);
    }
}

static void handle_key_event(PhoneApp_t* app, const AppEvent_t* event)
{
    if (!app || !app->active_screen || !event) return;

    PhoneApp_ResetActivity(app);

    if (app->active_screen->handle_key) {
        app->active_screen->handle_key(app->active_screen, 
                                       (KeyCode_t)event->data.key.key_code);
    }
}

static void handle_lora_event(PhoneApp_t* app, const AppEvent_t* event)
{
    if (!app || !event) return;

    switch (event->type) {
        case APP_EVENT_LORA_PACKET_RECEIVED:
            /* Router le paquet vers le protocole approprié */
            PacketRouter_Route(&app->packet_router,
                              event->data.lora_packet.data,
                              event->data.lora_packet.length,
                              event->data.lora_packet.rssi);
            break;

        case APP_EVENT_LORA_TX_COMPLETE:
            /* Notifier PhoneService */
            PhoneService_OnTxComplete(&app->phone_service);
            break;

        case APP_EVENT_LORA_ERROR:
            DEBUG_ERROR(TAG, "Erreur LoRa");
            break;

        default:
            break;
    }
}

static void handle_call_event(PhoneApp_t* app, const AppEvent_t* event)
{
    if (!app || !event) return;

    switch (event->type) {
        case APP_EVENT_INCOMING_CALL:
            PhoneApp_OnIncomingCall(app,
                                   event->data.call.caller_id,
                                   event->data.call.caller_number);
            break;

        case APP_EVENT_CALL_ACCEPTED:
            PhoneApp_OnCallAccepted(app);
            break;

        case APP_EVENT_CALL_REJECTED:
            PhoneApp_OnCallRejected(app);
            break;

        case APP_EVENT_CALL_ENDED:
            PhoneApp_OnCallEnded(app);
            break;

        case APP_EVENT_CALL_MISSED:
            RingtoneService_Stop(&app->ringtone_service);
            CallLogService_AddEntry(&app->call_log_service,
                                   event->data.call.caller_number,
                                   event->data.call.caller_id,
                                   0,
                                   CALL_TYPE_MISSED);
            break;

        default:
            break;
    }
}

static void handle_message_event(PhoneApp_t* app, const AppEvent_t* event)
{
    if (!app || !event) return;

    switch (event->type) {
        case APP_EVENT_NEW_MESSAGE:
            PhoneApp_OnNewMessage(app,
                                 event->data.message.sender_id,
                                 event->data.message.content);
            break;

        case APP_EVENT_MESSAGE_SENT:
            DEBUG_INFO(TAG, "Message envoyé avec succès");
            break;

        default:
            break;
    }
}

static void handle_system_event(PhoneApp_t* app, const AppEvent_t* event)
{
    if (!app || !event) return;

    switch (event->type) {
        case APP_EVENT_BATTERY_LOW:
            NotificationService_Notify(&app->notification_svc,
                                      "Batterie faible",
                                      "Rechargez le telephone");
            break;

        case APP_EVENT_BATTERY_CRITICAL:
            DEBUG_WARN(TAG, "Arrêt critique - Batterie épuisée");
            PhoneApp_Shutdown(app);
            break;

        case APP_EVENT_CHARGING_START:
            DEBUG_INFO(TAG, "Charge démarrée");
            break;

        case APP_EVENT_CHARGING_STOP:
            DEBUG_INFO(TAG, "Charge arrêtée");
            break;

        case APP_EVENT_SCREEN_TIMEOUT:
            if (app->state != APP_STATE_LOCKED &&
                app->state != APP_STATE_IN_CALL) {
                app->state = APP_STATE_LOCKED;
                PhoneApp_SwitchScreen(app, (ScreenBase_t*)&app->lock_screen);
            }
            break;

        case APP_EVENT_SYSTEM_ERROR:
            DEBUG_ERROR(TAG, "Erreur système: %s", event->data.error.error_msg);
            if (app->on_error) {
                app->on_error(event->data.error.error_msg);
            }
            break;

        default:
            break;
    }
}

/* ======================================================================== */
/*              CALLBACKS ÉCRANS                                            */
/* ======================================================================== */

static void on_splash_finished(void)
{
    DEBUG_INFO(TAG, "Splash terminé → Vérification verrouillage");

    PhoneApp_t* app = &g_app;

    if (ScreenLock_IsPinConfigured(&app->lock_screen)) {
        /* PIN configuré → Verrouillage */
        app->state = APP_STATE_LOCKED;
        PhoneApp_SwitchScreen(app, (ScreenBase_t*)&app->lock_screen);
    } else {
        /* Pas de PIN → Accueil direct */
        app->state = APP_STATE_IDLE;
        PhoneApp_SwitchScreen(app, (ScreenBase_t*)&app->home_screen);
    }
}

static void on_lock_unlocked(void)
{
    DEBUG_INFO(TAG, "Téléphone déverrouillé → Accueil");

    PhoneApp_t* app = &g_app;
    app->state = APP_STATE_IDLE;
    PhoneApp_SwitchScreen(app, (ScreenBase_t*)&app->home_screen);
}

static void on_home_dialer_requested(void)
{
    DEBUG_INFO(TAG, "Navigation: Accueil → Composeur");
    PhoneApp_SwitchScreen(&g_app, (ScreenBase_t*)&g_app.dialer_screen);
}

static void on_home_settings_requested(void)
{
    DEBUG_INFO(TAG, "Navigation: Accueil → Paramètres");
    PhoneApp_SwitchScreen(&g_app, (ScreenBase_t*)&g_app.settings_screen);
}

static void on_home_call_log_requested(void)
{
    DEBUG_INFO(TAG, "Navigation: Accueil → Journal");
    PhoneApp_SwitchScreen(&g_app, (ScreenBase_t*)&g_app.call_log_screen);
}

static void on_home_contacts_requested(void)
{
    DEBUG_INFO(TAG, "Navigation: Accueil → Contacts");
    PhoneApp_SwitchScreen(&g_app, (ScreenBase_t*)&g_app.contacts_screen);
}

static void on_home_messages_requested(void)
{
    DEBUG_INFO(TAG, "Navigation: Accueil → Messages");
    PhoneApp_SwitchScreen(&g_app, (ScreenBase_t*)&g_app.messages_screen);
}

static void on_call_ended(void)
{
    DEBUG_INFO(TAG, "Appel terminé par l'utilisateur");
    PhoneApp_OnCallEnded(&g_app);
}

static void on_settings_back(void)
{
    DEBUG_INFO(TAG, "Retour paramètres");
    PhoneApp_GoBack(&g_app);
}

/* ======================================================================== */
/*              CALLBACKS SERVICES                                          */
/* ======================================================================== */

static void on_incoming_call_detected(const char* caller_id,
                                      const char* caller_number)
{
    DEBUG_INFO(TAG, "Callback: Appel entrant détecté");

    /* Poster l'événement */
    PhoneApp_t* app = &g_app;
    AppEvent_t event;
    memset(&event, 0, sizeof(event));
    event.type = APP_EVENT_INCOMING_CALL;
    event.priority = APP_PRIORITY_HIGH;
    strncpy(event.data.call.caller_id, caller_id, 31);
    strncpy(event.data.call.caller_number, caller_number, 19);

    PhoneApp_PostEvent(app, event.type, event.priority);

    /* Mettre les données dans la file (simplifié) */
    /* En production, utiliser une copie complète */
}

static void on_new_message_received(const char* sender,
                                    const char* content)
{
    DEBUG_INFO(TAG, "Callback: Nouveau message reçu");

    PhoneApp_t* app = &g_app;
    AppEvent_t event;
    memset(&event, 0, sizeof(event));
    event.type = APP_EVENT_NEW_MESSAGE;
    event.priority = APP_PRIORITY_HIGH;
    strncpy(event.data.message.sender_id, sender, 31);
    strncpy(event.data.message.content, content, 255);

    PhoneApp_PostEvent(app, event.type, event.priority);
}

/* ======================================================================== */
/*              TIMERS                                                      */
/* ======================================================================== */

static void statusbar_timer_callback(TimerHandle_t timer)
{
    /* Mise à jour périodique de la barre de statut */
    /* (géré par chaque écran individuellement via Update) */
}

static void inactivity_timer_callback(TimerHandle_t timer)
{
    PhoneApp_t* app = (PhoneApp_t*)Timer_GetContext(timer);
    if (!app) return;

    check_inactivity(app);
}

static void call_check_timer_callback(TimerHandle_t timer)
{
    PhoneApp_t* app = (PhoneApp_t*)Timer_GetContext(timer);
    if (!app) return;

    check_incoming_calls(app);
}

/* ======================================================================== */
/*              POINT D'ENTRÉE PRINCIPAL (main)                              */
/* ======================================================================== */

/**
 * @brief Fonction main() - Point d'entrée du firmware
 *
 * Appelée par le code de démarrage (startup_stm32f429xx.s)
 * après l'initialisation du runtime C (bss, data).
 */
int main(void)
{
    /* Initialiser l'application */
    PhoneApp_Init(&g_app);

    /* Lancer la boucle principale (ne retourne jamais) */
    PhoneApp_Run(&g_app);

    /* Ne devrait jamais arriver ici */
    return 0;
}

/* ======================================================================== */
/*              ISR (Interrupt Service Routines)                            */
/* ======================================================================== */

/**
 * @brief Handler interruption externe Touch IRQ
 */
void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);

    /* Poster événement tactile */
    AppEvent_t event;
    memset(&event, 0, sizeof(event));
    event.type = APP_EVENT_TOUCH;
    event.priority = APP_PRIORITY_NORMAL;
    /* Les coordonnées seront lues dans le handler */

    PhoneApp_PostEvent(&g_app, event.type, event.priority);
}

/**
 * @brief Handler interruption DIO0 LoRa (TxDone/RxDone)
 */
void EXTI1_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);

    AppEvent_t event;
    memset(&event, 0, sizeof(event));
    event.type = APP_EVENT_LORA_PACKET_RECEIVED;
    event.priority = APP_PRIORITY_HIGH;

    PhoneApp_PostEvent(&g_app, event.type, event.priority);
}

/**
 * @brief Handler erreur matérielle (Hard Fault)
 */
void HardFault_Handler(void)
{
    DEBUG_ERROR(TAG, "HARD FAULT DETECTED!");

    /* Tenter de sauvegarder les paramètres */
    SettingsService_Save(&g_app.settings_service);

    /* Afficher l'erreur */
    Display_FillRect(0, 0, 320, 480, 0xF800);
    Display_DrawText(60, 220, "ERREUR SYSTEME", &font_large, 0xFFFF, 0xF800);
    Display_DrawText(40, 260, "Redemarrez le telephone", &font_medium, 0xFFFF, 0xF800);

    while (1) {}
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */