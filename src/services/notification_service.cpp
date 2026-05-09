/**
 * @file notification_service.cpp
 * @brief Implémentation du service de notifications
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans notification_service.h.
 * 
 * Il gère :
 * - La création et l'envoi de notifications
 * - L'affichage des notifications (visuel, audio, vibreur, LED)
 * - Les modes "Ne pas déranger" et silencieux
 * - L'historique des notifications
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "notification_service.h"
#include "../drivers/audio/audio_manager.h"
#include "../drivers/display/display_manager.h"
#include "../drivers/power/power_manager.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du service */
static NotificationServiceState notify_state;

/** @brief Callbacks */
static NotificationService_NewCallback new_cb = NULL;
static NotificationService_DismissedCallback dismissed_cb = NULL;
static NotificationService_ClickedCallback clicked_cb = NULL;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le service de notifications
 */
bool notification_service_init(void)
{
    NOTIFY_DEBUG("Initialisation du service de notifications...\n");
    
    memset(&notify_state, 0, sizeof(NotificationServiceState));
    
    // Configurations par défaut pour chaque type
    configure_default_types();
    
    notify_state.initialized = true;
    
    NOTIFY_DEBUG("Service initialisé\n");
    return true;
}

/**
 * @brief Configure les types de notifications par défaut
 */
static void configure_default_types(void)
{
    // CALL
    notify_state.typeConfigs[NOTIFY_TYPE_CALL].type = NOTIFY_TYPE_CALL;
    notify_state.typeConfigs[NOTIFY_TYPE_CALL].defaultMethods = 
        NOTIFY_METHOD_VISUAL | NOTIFY_METHOD_AUDIO | NOTIFY_METHOD_VIBRATE;
    notify_state.typeConfigs[NOTIFY_TYPE_CALL].defaultDuration = 0;  // Persistant
    notify_state.typeConfigs[NOTIFY_TYPE_CALL].enabled = true;
    
    // SMS
    notify_state.typeConfigs[NOTIFY_TYPE_SMS].type = NOTIFY_TYPE_SMS;
    notify_state.typeConfigs[NOTIFY_TYPE_SMS].defaultMethods = 
        NOTIFY_METHOD_VISUAL | NOTIFY_METHOD_AUDIO | NOTIFY_METHOD_VIBRATE;
    notify_state.typeConfigs[NOTIFY_TYPE_SMS].defaultDuration = NOTIFICATION_DEFAULT_DURATION;
    notify_state.typeConfigs[NOTIFY_TYPE_SMS].enabled = true;
    
    // BATTERY
    notify_state.typeConfigs[NOTIFY_TYPE_BATTERY].type = NOTIFY_TYPE_BATTERY;
    notify_state.typeConfigs[NOTIFY_TYPE_BATTERY].defaultMethods = 
        NOTIFY_METHOD_VISUAL | NOTIFY_METHOD_AUDIO;
    notify_state.typeConfigs[NOTIFY_TYPE_BATTERY].defaultDuration = 5000;
    notify_state.typeConfigs[NOTIFY_TYPE_BATTERY].enabled = true;
    
    // NETWORK
    notify_state.typeConfigs[NOTIFY_TYPE_NETWORK].type = NOTIFY_TYPE_NETWORK;
    notify_state.typeConfigs[NOTIFY_TYPE_NETWORK].defaultMethods = NOTIFY_METHOD_VISUAL;
    notify_state.typeConfigs[NOTIFY_TYPE_NETWORK].defaultDuration = 2000;
    notify_state.typeConfigs[NOTIFY_TYPE_NETWORK].enabled = true;
    
    // SYSTEM
    notify_state.typeConfigs[NOTIFY_TYPE_SYSTEM].type = NOTIFY_TYPE_SYSTEM;
    notify_state.typeConfigs[NOTIFY_TYPE_SYSTEM].defaultMethods = 
        NOTIFY_METHOD_VISUAL | NOTIFY_METHOD_AUDIO;
    notify_state.typeConfigs[NOTIFY_TYPE_SYSTEM].defaultDuration = 3000;
    notify_state.typeConfigs[NOTIFY_TYPE_SYSTEM].enabled = true;
    
    // ERROR
    notify_state.typeConfigs[NOTIFY_TYPE_ERROR].type = NOTIFY_TYPE_ERROR;
    notify_state.typeConfigs[NOTIFY_TYPE_ERROR].defaultMethods = 
        NOTIFY_METHOD_VISUAL | NOTIFY_METHOD_AUDIO | NOTIFY_METHOD_LED;
    notify_state.typeConfigs[NOTIFY_TYPE_ERROR].defaultDuration = 0;  // Persistant
    notify_state.typeConfigs[NOTIFY_TYPE_ERROR].enabled = true;
    
    // INFO
    notify_state.typeConfigs[NOTIFY_TYPE_INFO].type = NOTIFY_TYPE_INFO;
    notify_state.typeConfigs[NOTIFY_TYPE_INFO].defaultMethods = NOTIFY_METHOD_VISUAL;
    notify_state.typeConfigs[NOTIFY_TYPE_INFO].defaultDuration = 2000;
    notify_state.typeConfigs[NOTIFY_TYPE_INFO].enabled = true;
    
    // ALARM
    notify_state.typeConfigs[NOTIFY_TYPE_ALARM].type = NOTIFY_TYPE_ALARM;
    notify_state.typeConfigs[NOTIFY_TYPE_ALARM].defaultMethods = 
        NOTIFY_METHOD_VISUAL | NOTIFY_METHOD_AUDIO | NOTIFY_METHOD_VIBRATE;
    notify_state.typeConfigs[NOTIFY_TYPE_ALARM].defaultDuration = 0;
    notify_state.typeConfigs[NOTIFY_TYPE_ALARM].enabled = true;
    
    // REMINDER
    notify_state.typeConfigs[NOTIFY_TYPE_REMINDER].type = NOTIFY_TYPE_REMINDER;
    notify_state.typeConfigs[NOTIFY_TYPE_REMINDER].defaultMethods = 
        NOTIFY_METHOD_VISUAL | NOTIFY_METHOD_AUDIO;
    notify_state.typeConfigs[NOTIFY_TYPE_REMINDER].defaultDuration = 5000;
    notify_state.typeConfigs[NOTIFY_TYPE_REMINDER].enabled = true;
}

void notification_service_deinit(void)
{
    notification_dismiss_all();
    notify_state.initialized = false;
}

bool notification_service_is_ready(void)
{
    return notify_state.initialized;
}

// ============================================================
// SECTION 2 : ENVOI DE NOTIFICATIONS
// ============================================================

uint32_t notification_send(NotificationType type, const char* title, const char* message)
{
    NotificationTypeConfig* config = &notify_state.typeConfigs[type];
    return notification_send_priority(type, title, message, 
                                       (NotificationPriority)(type == NOTIFY_TYPE_ERROR ? 
                                        NOTIFY_PRIORITY_CRITICAL : NOTIFY_PRIORITY_NORMAL));
}

uint32_t notification_send_priority(NotificationType type, const char* title,
                                     const char* message, NotificationPriority priority)
{
    if (!notify_state.initialized) return 0;
    if (title == NULL && message == NULL) return 0;
    
    NotificationTypeConfig* config = &notify_state.typeConfigs[type];
    
    // Vérifier si le type est activé
    if (!config->enabled) return 0;
    
    // Vérifier le mode "Ne pas déranger"
    if (notify_state.doNotDisturb && priority < NOTIFY_PRIORITY_CRITICAL)
    {
        // Seules les notifications critiques passent
        if (priority < NOTIFY_PRIORITY_CRITICAL)
        {
            NOTIFY_DEBUG("Notification filtrée (DND) : %s\n", title);
            return 0;
        }
    }
    
    // Créer la notification
    Notification* notif;
    
    // Ajouter aux notifications en attente
    if (notify_state.pendingCount >= NOTIFICATION_MAX_PENDING)
    {
        // Supprimer la plus ancienne
        memmove(&notify_state.pending[0], &notify_state.pending[1],
                (NOTIFICATION_MAX_PENDING - 1) * sizeof(Notification));
        notify_state.pendingCount = NOTIFICATION_MAX_PENDING - 1;
    }
    
    notif = &notify_state.pending[notify_state.pendingCount++];
    memset(notif, 0, sizeof(Notification));
    
    notif->id = ++notify_state.nextId;
    notif->type = type;
    notif->priority = priority;
    
    if (title) strncpy(notif->title, title, NOTIFICATION_TITLE_MAX - 1);
    if (message) strncpy(notif->message, message, NOTIFICATION_MESSAGE_MAX - 1);
    
    notif->timestamp = HAL_GetTick();
    notif->duration = config->defaultDuration;
    notif->methods = config->defaultMethods;
    notif->persistent = (config->defaultDuration == 0);
    notif->iconIndex = type;  // Icône par défaut = type
    
    // Appliquer le mode silencieux
    if (notify_state.silentMode)
    {
        notif->methods &= ~NOTIFY_METHOD_AUDIO;
    }
    
    // Exécuter les méthodes de notification
    execute_notification_methods(notif);
    
    // Ajouter à l'historique
    add_to_history(notif);
    
    notify_state.totalNotifications++;
    
    // Définir comme notification courante
    notify_state.currentDisplayed = notif;
    notify_state.displayStartTime = HAL_GetTick();
    
    NOTIFY_DEBUG("Notification #%lu : [%d] %s - %s\n", 
                (unsigned long)notif->id, type, title, message);
    
    if (new_cb) new_cb(notif);
    
    return notif->id;
}

/**
 * @brief Exécute les méthodes de notification (visuel, audio, vibreur, LED)
 */
static void execute_notification_methods(const Notification* notif)
{
    if (notif == NULL) return;
    
    // Méthode visuelle
    if (notif->methods & NOTIFY_METHOD_VISUAL)
    {
        // Afficher une bannière/popup
        display_show_notification(notif->title, notif->message, notif->iconIndex);
    }
    
    // Méthode audio
    if (notif->methods & NOTIFY_METHOD_AUDIO)
    {
        switch (notif->type)
        {
            case NOTIFY_TYPE_CALL:
                audio_manager_play_ringtone(0);  // Sonnerie
                break;
            case NOTIFY_TYPE_SMS:
                audio_manager_play_beep(1000, 100);  // Bip court
                HAL_Delay(100);
                audio_manager_play_beep(2000, 100);  // Double bip
                break;
            case NOTIFY_TYPE_BATTERY:
                audio_manager_play_beep(500, 200);  // Bip grave
                break;
            case NOTIFY_TYPE_ERROR:
                audio_manager_play_beep(200, 500);  // Bip long et grave
                break;
            default:
                audio_manager_play_beep(1500, 50);  // Bip simple
                break;
        }
    }
    
    // Méthode vibreur
    if (notif->methods & NOTIFY_METHOD_VIBRATE)
    {
        // Activer le vibreur
        gpio_set_vibrator(true);
        HAL_Delay(200);
        gpio_set_vibrator(false);
    }
    
    // Méthode LED
    if (notif->methods & NOTIFY_METHOD_LED)
    {
        // Configurer le clignotement de la LED
        led_set_blinking(true, 500);  // 500ms
    }
}

// ============================================================
// SECTION 3 : GESTION DES NOTIFICATIONS
// ============================================================

bool notification_dismiss(uint32_t notificationId)
{
    for (uint8_t i = 0; i < notify_state.pendingCount; i++)
    {
        if (notify_state.pending[i].id == notificationId)
        {
            // Mettre à jour la notification courante
            if (notify_state.currentDisplayed && 
                notify_state.currentDisplayed->id == notificationId)
            {
                notify_state.currentDisplayed = NULL;
            }
            
            // Supprimer de la liste
            if (i < notify_state.pendingCount - 1)
            {
                memmove(&notify_state.pending[i], &notify_state.pending[i + 1],
                        (notify_state.pendingCount - i - 1) * sizeof(Notification));
            }
            notify_state.pendingCount--;
            
            // Arrêter la LED si plus de notifications
            if (notify_state.pendingCount == 0)
            {
                led_set_blinking(false, 0);
            }
            
            if (dismissed_cb) dismissed_cb(notificationId);
            
            return true;
        }
    }
    return false;
}

void notification_dismiss_all(void)
{
    notify_state.pendingCount = 0;
    notify_state.currentDisplayed = NULL;
    led_set_blinking(false, 0);
    memset(notify_state.pending, 0, sizeof(notify_state.pending));
}

void notification_acknowledge(uint32_t notificationId)
{
    for (uint8_t i = 0; i < notify_state.pendingCount; i++)
    {
        if (notify_state.pending[i].id == notificationId)
        {
            notify_state.pending[i].acknowledged = true;
            
            if (notify_state.pending[i].onClick)
            {
                notify_state.pending[i].onClick(notificationId);
            }
            
            if (clicked_cb) clicked_cb(notificationId);
            
            break;
        }
    }
}

void notification_acknowledge_all(void)
{
    for (uint8_t i = 0; i < notify_state.pendingCount; i++)
    {
        notify_state.pending[i].acknowledged = true;
    }
}

// ============================================================
// SECTION 4 : NOTIFICATIONS SPÉCIFIQUES
// ============================================================

uint32_t notification_call_missed(const char* number)
{
    char title[NOTIFICATION_TITLE_MAX];
    char message[NOTIFICATION_MESSAGE_MAX];
    
    snprintf(title, NOTIFICATION_TITLE_MAX, "Appel manqué");
    snprintf(message, NOTIFICATION_MESSAGE_MAX, "Appel de %s", number ? number : "Inconnu");
    
    return notification_send_priority(NOTIFY_TYPE_CALL, title, message, NOTIFY_PRIORITY_HIGH);
}

uint32_t notification_call_incoming(const char* number, const char* name)
{
    char title[NOTIFICATION_TITLE_MAX];
    char message[NOTIFICATION_MESSAGE_MAX];
    
    snprintf(title, NOTIFICATION_TITLE_MAX, "Appel entrant");
    snprintf(message, NOTIFICATION_MESSAGE_MAX, "%s vous appelle", 
             name ? name : (number ? number : "Inconnu"));
    
    return notification_send_priority(NOTIFY_TYPE_CALL, title, message, NOTIFY_PRIORITY_HIGH);
}

uint32_t notification_sms_received(const char* sender, const char* preview)
{
    char title[NOTIFICATION_TITLE_MAX];
    char message[NOTIFICATION_MESSAGE_MAX];
    
    snprintf(title, NOTIFICATION_TITLE_MAX, "Nouveau message");
    snprintf(message, NOTIFICATION_MESSAGE_MAX, "%s : %s", 
             sender ? sender : "Inconnu", preview ? preview : "");
    
    return notification_send(NOTIFY_TYPE_SMS, title, message);
}

uint32_t notification_battery_low(uint8_t percent)
{
    char title[NOTIFICATION_TITLE_MAX];
    char message[NOTIFICATION_MESSAGE_MAX];
    
    snprintf(title, NOTIFICATION_TITLE_MAX, "Batterie faible");
    snprintf(message, NOTIFICATION_MESSAGE_MAX, "Niveau de batterie : %d%%", percent);
    
    return notification_send_priority(NOTIFY_TYPE_BATTERY, title, message, NOTIFY_PRIORITY_HIGH);
}

uint32_t notification_battery_critical(uint8_t percent)
{
    char title[NOTIFICATION_TITLE_MAX];
    char message[NOTIFICATION_MESSAGE_MAX];
    
    snprintf(title, NOTIFICATION_TITLE_MAX, "BATTERIE CRITIQUE");
    snprintf(message, NOTIFICATION_MESSAGE_MAX, "Arrêt imminent ! Niveau : %d%%", percent);
    
    return notification_send_priority(NOTIFY_TYPE_BATTERY, title, message, NOTIFY_PRIORITY_CRITICAL);
}

uint32_t notification_network_peer_found(const char* name)
{
    char title[NOTIFICATION_TITLE_MAX];
    
    snprintf(title, NOTIFICATION_TITLE_MAX, "%s est en ligne", name ? name : "Contact");
    
    return notification_send(NOTIFY_TYPE_NETWORK, title, NULL);
}

uint32_t notification_network_peer_lost(const char* name)
{
    char title[NOTIFICATION_TITLE_MAX];
    
    snprintf(title, NOTIFICATION_TITLE_MAX, "%s n'est plus joignable", name ? name : "Contact");
    
    return notification_send(NOTIFY_TYPE_NETWORK, title, NULL);
}

uint32_t notification_system_error(const char* errorMessage)
{
    return notification_send_priority(NOTIFY_TYPE_ERROR, "Erreur système", 
                                       errorMessage, NOTIFY_PRIORITY_HIGH);
}

uint32_t notification_system_info(const char* infoMessage)
{
    return notification_send(NOTIFY_TYPE_INFO, "Information", infoMessage);
}

// ============================================================
// SECTION 5 : GESTION
// ============================================================

void notification_set_do_not_disturb(bool enable)
{
    notify_state.doNotDisturb = enable;
    NOTIFY_DEBUG("DND : %s\n", enable ? "ON" : "OFF");
}

bool notification_get_do_not_disturb(void)
{
    return notify_state.doNotDisturb;
}

void notification_set_silent_mode(bool enable)
{
    notify_state.silentMode = enable;
    NOTIFY_DEBUG("Silencieux : %s\n", enable ? "ON" : "OFF");
}

bool notification_get_silent_mode(void)
{
    return notify_state.silentMode;
}

void notification_configure_type(NotificationType type, const NotificationTypeConfig* config)
{
    if (config)
    {
        memcpy(&notify_state.typeConfigs[type], config, sizeof(NotificationTypeConfig));
        notify_state.typeConfigs[type].type = type;
    }
}

NotificationTypeConfig* notification_get_type_config(NotificationType type)
{
    return &notify_state.typeConfigs[type];
}

void notification_enable_type(NotificationType type, bool enable)
{
    notify_state.typeConfigs[type].enabled = enable;
}

// ============================================================
// SECTION 6 : TRAITEMENT
// ============================================================

void notification_process(void)
{
    if (!notify_state.initialized) return;
    
    // Vérifier si la notification courante a expiré
    if (notify_state.currentDisplayed && !notify_state.currentDisplayed->persistent)
    {
        uint32_t elapsed = HAL_GetTick() - notify_state.displayStartTime;
        
        if (elapsed >= notify_state.currentDisplayed->duration)
        {
            notification_dismiss(notify_state.currentDisplayed->id);
        }
    }
}

const Notification* notification_get_current(void)
{
    return notify_state.currentDisplayed;
}

uint8_t notification_get_pending_count(void)
{
    return notify_state.pendingCount;
}

uint16_t notification_get_history(Notification* notifications, uint16_t maxCount)
{
    if (notifications == NULL) return 0;
    
    uint16_t count = (notify_state.historyCount < maxCount) ? 
                      notify_state.historyCount : maxCount;
    
    memcpy(notifications, notify_state.history, count * sizeof(Notification));
    return count;
}

void notification_clear_history(void)
{
    notify_state.historyCount = 0;
    memset(notify_state.history, 0, sizeof(notify_state.history));
}

// ============================================================
// SECTION 7 : HISTORIQUE
// ============================================================

static void add_to_history(Notification* notif)
{
    if (notif == NULL) return;
    
    // Décaler si plein
    if (notify_state.historyCount >= NOTIFICATION_HISTORY_MAX)
    {
        memmove(&notify_state.history[0], &notify_state.history[1],
                (NOTIFICATION_HISTORY_MAX - 1) * sizeof(Notification));
        notify_state.historyCount = NOTIFICATION_HISTORY_MAX - 1;
    }
    
    memcpy(&notify_state.history[notify_state.historyCount++], notif, sizeof(Notification));
}

// ============================================================
// SECTION 8 : CALLBACKS
// ============================================================

void notification_set_new_callback(NotificationService_NewCallback cb) { new_cb = cb; }
void notification_set_dismissed_callback(NotificationService_DismissedCallback cb) { dismissed_cb = cb; }
void notification_set_clicked_callback(NotificationService_ClickedCallback cb) { clicked_cb = cb; }

// ============================================================
// SECTION 9 : DÉBOGAGE
// ============================================================

void notification_print_state(void)
{
    printf("\n═══ ÉTAT SERVICE NOTIFICATIONS ═══\n");
    printf("En attente     : %d\n", notify_state.pendingCount);
    printf("Historique     : %d\n", notify_state.historyCount);
    printf("Total          : %lu\n", (unsigned long)notify_state.totalNotifications);
    printf("DND            : %s\n", notify_state.doNotDisturb ? "ON" : "OFF");
    printf("Silencieux     : %s\n", notify_state.silentMode ? "ON" : "OFF");
    printf("══════════════════════════════\n\n");
}

void notification_print_pending(void)
{
    printf("\n═══ NOTIFICATIONS EN ATTENTE (%d) ═══\n", notify_state.pendingCount);
    
    if (notify_state.pendingCount == 0)
    {
        printf("  (aucune)\n");
    }
    else
    {
        for (uint8_t i = 0; i < notify_state.pendingCount; i++)
        {
            Notification* n = &notify_state.pending[i];
            const char* typeStr = "?";
            switch (n->type)
            {
                case NOTIFY_TYPE_CALL:    typeStr = "APPEL"; break;
                case NOTIFY_TYPE_SMS:     typeStr = "SMS"; break;
                case NOTIFY_TYPE_BATTERY: typeStr = "BATT"; break;
                case NOTIFY_TYPE_NETWORK: typeStr = "RESEAU"; break;
                case NOTIFY_TYPE_SYSTEM:  typeStr = "SYST"; break;
                case NOTIFY_TYPE_ERROR:   typeStr = "ERR"; break;
                case NOTIFY_TYPE_INFO:    typeStr = "INFO"; break;
                default: break;
            }
            
            printf("[%d] %s %s : %s - %s %s\n",
                   i + 1, typeStr,
                   n->acknowledged ? "✓" : "●",
                   n->title, n->message,
                   n->persistent ? "[PERSISTANT]" : "");
        }
    }
    printf("══════════════════════════════════\n\n");
}

void notification_print_history(void)
{
    printf("\n═══ HISTORIQUE NOTIFICATIONS (%d) ═══\n", notify_state.historyCount);
    
    for (uint16_t i = 0; i < notify_state.historyCount && i < 10; i++)
    {
        Notification* n = &notify_state.history[i];
        printf("  #%lu : %s\n", (unsigned long)n->id, n->title);
    }
    
    if (notify_state.historyCount > 10)
    {
        printf("  ... et %d autres\n", notify_state.historyCount - 10);
    }
    printf("══════════════════════════════════\n\n");
}

void notification_print_config(void)
{
    printf("\n═══ CONFIG NOTIFICATIONS ═══\n");
    printf("%-12s %-8s %-8s\n", "Type", "Activé", "DND");
    printf("────────────────────────────\n");
    
    for (uint8_t i = 0; i < 9; i++)
    {
        const char* typeStr = "?";
        switch (i)
        {
            case NOTIFY_TYPE_CALL:    typeStr = "Appel"; break;
            case NOTIFY_TYPE_SMS:     typeStr = "SMS"; break;
            case NOTIFY_TYPE_BATTERY: typeStr = "Batterie"; break;
            case NOTIFY_TYPE_NETWORK: typeStr = "Réseau"; break;
            case NOTIFY_TYPE_SYSTEM:  typeStr = "Système"; break;
            case NOTIFY_TYPE_ERROR:   typeStr = "Erreur"; break;
            case NOTIFY_TYPE_INFO:    typeStr = "Info"; break;
            case NOTIFY_TYPE_ALARM:   typeStr = "Alarme"; break;
            case NOTIFY_TYPE_REMINDER:typeStr = "Rappel"; break;
        }
        
        printf("%-12s %-8s %-8s\n", typeStr,
               notify_state.typeConfigs[i].enabled ? "ON" : "OFF",
               notify_state.typeConfigs[i].doNotDisturb ? "OUI" : "NON");
    }
    printf("══════════════════════════\n\n");
}

bool notification_self_test(void)
{
    NOTIFY_DEBUG("Auto-test...\n");
    
    if (!notify_state.initialized)
    {
        NOTIFY_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : envoyer une notification de test
    uint32_t id = notification_send(NOTIFY_TYPE_INFO, "Test", "Notification de test");
    
    if (id == 0 || notify_state.pendingCount == 0)
    {
        NOTIFY_DEBUG("Échec : notification non créée\n");
        return false;
    }
    
    // Nettoyer
    notification_dismiss(id);
    
    NOTIFY_DEBUG("Auto-test OK\n");
    return true;
}