/**
 * @file notification_service.h
 * @brief Service de gestion des notifications
 * 
 * Ce fichier implémente le service de notifications qui gère
 * tous les types d'alertes et de notifications du téléphone :
 * - Notifications d'appels manqués
 * - Notifications de nouveaux SMS
 * - Alertes batterie faible
 * - Notifications réseau (pair découvert, perdu)
 * - Notifications système (erreurs, mises à jour)
 * 
 * Types de notifications :
 * - VISUAL  : Affichage à l'écran (popup, bannière)
 * - AUDIO   : Sonnerie, bip, vibreur
 * - LED     : Clignotement de la LED de statut
 * 
 * Priorités :
 * - CRITICAL : Appel urgence, batterie critique
 * - HIGH     : Appel entrant, alarme
 * - NORMAL   : SMS, appel manqué
 * - LOW      : Réseau, mise à jour disponible
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef NOTIFICATION_SERVICE_H
#define NOTIFICATION_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du service */
#define NOTIFICATION_VERSION           "1.0.0"

/** @brief Nombre maximum de notifications en attente */
#define NOTIFICATION_MAX_PENDING       20

/** @brief Taille maximale du titre */
#define NOTIFICATION_TITLE_MAX         32

/** @brief Taille maximale du message */
#define NOTIFICATION_MESSAGE_MAX       128

/** @brief Durée d'affichage par défaut (ms) */
#define NOTIFICATION_DEFAULT_DURATION  3000

/** @brief Nombre maximum de notifications dans l'historique */
#define NOTIFICATION_HISTORY_MAX       50

// ============================================================
// SECTION 2 : TYPES DE NOTIFICATIONS
// ============================================================

/**
 * @brief Types de notifications
 */
typedef enum {
    NOTIFY_TYPE_INFO        = 0,    // Information générale
    NOTIFY_TYPE_CALL        = 1,    // Appel (entrant/manqué)
    NOTIFY_TYPE_SMS         = 2,    // Nouveau SMS
    NOTIFY_TYPE_BATTERY     = 3,    // Alerte batterie
    NOTIFY_TYPE_NETWORK     = 4,    // Événement réseau
    NOTIFY_TYPE_SYSTEM      = 5,    // Système
    NOTIFY_TYPE_ALARM       = 6,    // Alarme
    NOTIFY_TYPE_REMINDER    = 7,    // Rappel
    NOTIFY_TYPE_ERROR       = 8     // Erreur
} NotificationType;

/**
 * @brief Priorités des notifications
 */
typedef enum {
    NOTIFY_PRIORITY_LOW     = 0,    // Basse (réseau, info)
    NOTIFY_PRIORITY_NORMAL  = 1,    // Normale (SMS)
    NOTIFY_PRIORITY_HIGH    = 2,    // Haute (appel entrant)
    NOTIFY_PRIORITY_CRITICAL = 3   // Critique (batterie vide, urgence)
} NotificationPriority;

/**
 * @brief Manières de notifier
 */
typedef enum {
    NOTIFY_METHOD_NONE      = 0,        // Aucune
    NOTIFY_METHOD_VISUAL    = (1 << 0), // Affichage écran
    NOTIFY_METHOD_AUDIO     = (1 << 1), // Son
    NOTIFY_METHOD_VIBRATE   = (1 << 2), // Vibreur
    NOTIFY_METHOD_LED       = (1 << 3)  // LED
} NotificationMethod;

// ============================================================
// SECTION 3 : STRUCTURES DE DONNÉES
// ============================================================

/**
 * @brief Notification
 */
typedef struct {
    uint32_t id;                                // Identifiant unique
    NotificationType type;                      // Type
    NotificationPriority priority;              // Priorité
    
    char title[NOTIFICATION_TITLE_MAX];         // Titre
    char message[NOTIFICATION_MESSAGE_MAX];     // Message
    
    uint32_t timestamp;                         // Date de création
    uint32_t duration;                          // Durée d'affichage (ms, 0 = persistant)
    
    uint8_t methods;                            // Méthodes de notification (flags)
    bool acknowledged;                          // Acquittée ?
    bool persistent;                            // Persistante ? (reste après redémarrage)
    
    // Callback quand la notification est cliquée
    void (*onClick)(uint32_t notificationId);
    
    // Icône (index dans la table des icônes)
    uint8_t iconIndex;
    
} Notification;

/**
 * @brief Configuration des notifications par type
 */
typedef struct {
    NotificationType type;          // Type concerné
    uint8_t defaultMethods;         // Méthodes par défaut
    uint32_t defaultDuration;       // Durée par défaut (ms)
    bool enabled;                   // Activé ?
    bool doNotDisturb;              // Ne pas déranger ?
} NotificationTypeConfig;

// ============================================================
// SECTION 4 : ÉTAT DU SERVICE
// ============================================================

/**
 * @brief État du service de notifications
 */
typedef struct {
    bool initialized;                           // Service initialisé
    
    // Notifications en attente
    Notification pending[NOTIFICATION_MAX_PENDING];
    uint8_t pendingCount;
    
    // Historique
    Notification history[NOTIFICATION_HISTORY_MAX];
    uint16_t historyCount;
    
    // Configuration par type
    NotificationTypeConfig typeConfigs[9];      // Un par NotificationType
    
    // État global
    bool doNotDisturb;                          // Mode "Ne pas déranger"
    bool silentMode;                            // Mode silencieux
    bool vibrationEnabled;                      // Vibreur global
    
    // Compteurs
    uint32_t totalNotifications;
    uint32_t nextId;
    
    // Notification actuellement affichée
    Notification* currentDisplayed;
    uint32_t displayStartTime;
    
} NotificationServiceState;

// ============================================================
// SECTION 5 : CALLBACKS
// ============================================================

typedef void (*NotificationService_NewCallback)(const Notification* notification);
typedef void (*NotificationService_DismissedCallback)(uint32_t notificationId);
typedef void (*NotificationService_ClickedCallback)(uint32_t notificationId);

// ============================================================
// SECTION 6 : FONCTIONS D'INITIALISATION
// ============================================================

bool notification_service_init(void);
void notification_service_deinit(void);
bool notification_service_is_ready(void);

// ============================================================
// SECTION 7 : FONCTIONS DE NOTIFICATION
// ============================================================

uint32_t notification_send(NotificationType type, const char* title, const char* message);
uint32_t notification_send_priority(NotificationType type, const char* title, 
                                     const char* message, NotificationPriority priority);
bool notification_dismiss(uint32_t notificationId);
void notification_dismiss_all(void);
void notification_acknowledge(uint32_t notificationId);
void notification_acknowledge_all(void);

// ============================================================
// SECTION 8 : FONCTIONS SPÉCIFIQUES
// ============================================================

uint32_t notification_call_missed(const char* number);
uint32_t notification_call_incoming(const char* number, const char* name);
uint32_t notification_sms_received(const char* sender, const char* preview);
uint32_t notification_battery_low(uint8_t percent);
uint32_t notification_battery_critical(uint8_t percent);
uint32_t notification_network_peer_found(const char* name);
uint32_t notification_network_peer_lost(const char* name);
uint32_t notification_system_error(const char* errorMessage);
uint32_t notification_system_info(const char* infoMessage);

// ============================================================
// SECTION 9 : FONCTIONS DE GESTION
// ============================================================

void notification_set_do_not_disturb(bool enable);
bool notification_get_do_not_disturb(void);
void notification_set_silent_mode(bool enable);
bool notification_get_silent_mode(void);
void notification_configure_type(NotificationType type, const NotificationTypeConfig* config);
NotificationTypeConfig* notification_get_type_config(NotificationType type);
void notification_enable_type(NotificationType type, bool enable);

// ============================================================
// SECTION 10 : FONCTIONS DE TRAITEMENT
// ============================================================

void notification_process(void);
const Notification* notification_get_current(void);
uint8_t notification_get_pending_count(void);
uint16_t notification_get_history(Notification* notifications, uint16_t maxCount);
void notification_clear_history(void);

// ============================================================
// SECTION 11 : FONCTIONS DE CALLBACKS
// ============================================================

void notification_set_new_callback(NotificationService_NewCallback callback);
void notification_set_dismissed_callback(NotificationService_DismissedCallback callback);
void notification_set_clicked_callback(NotificationService_ClickedCallback callback);

// ============================================================
// SECTION 12 : FONCTIONS DE DÉBOGAGE
// ============================================================

void notification_print_state(void);
void notification_print_pending(void);
void notification_print_history(void);
void notification_print_config(void);
bool notification_self_test(void);

// ============================================================
// SECTION 13 : MACROS UTILITAIRES
// ============================================================

#define NOTIFY_INFO(title, msg)         notification_send(NOTIFY_TYPE_INFO, title, msg)
#define NOTIFY_ERROR(title, msg)        notification_send(NOTIFY_TYPE_ERROR, title, msg)
#define NOTIFY_HAS_PENDING()            (notification_get_pending_count() > 0)
#define NOTIFY_GET_PENDING_COUNT()      notification_get_pending_count()

// ============================================================
// SECTION 14 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define NOTIFY_DEBUG(fmt, ...)      printf("[NOTIFY] " fmt, ##__VA_ARGS__)
#else
    #define NOTIFY_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // NOTIFICATION_SERVICE_H