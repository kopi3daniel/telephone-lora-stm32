/**
 * @file sms_service.h
 * @brief Service de messagerie SMS haut niveau
 * 
 * Ce fichier implémente le service de messagerie qui orchestre
 * tous les sous-systèmes pour gérer les SMS :
 * - Protocole SMS (sms_protocol)
 * - Stockage des messages
 * - Conversations
 * - Notifications
 * 
 * Fonctionnalités :
 * - Envoi de SMS (simple, long, avec accusé)
 * - Réception et stockage
 * - Conversations par contact
 * - Messages non lus
 * - Brouillons
 * - Modèles de messages
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SMS_SERVICE_H
#define SMS_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "../protocols/sms_protocol.h"
#include "../protocols/identity.h"
#include "../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du service */
#define SMS_SERVICE_VERSION             "1.0.0"

/** @brief Nombre maximum de conversations */
#define SMS_MAX_CONVERSATIONS           50

/** @brief Nombre maximum de messages par conversation */
#define SMS_MAX_MESSAGES_PER_CONV       200

/** @brief Nombre maximum de brouillons */
#define SMS_MAX_DRAFTS                  10

/** @brief Nombre maximum de modèles */
#define SMS_MAX_TEMPLATES               20

/** @brief Longueur maximale d'un modèle */
#define SMS_TEMPLATE_MAX_LENGTH         160

// ============================================================
// SECTION 2 : TYPES DE DONNÉES
// ============================================================

/**
 * @brief Conversation SMS
 */
typedef struct {
    char contactNumber[IDENTITY_PHONE_NUMBER_MAX];  // Numéro du contact
    char contactName[32];                           // Nom du contact
    SMSMessage messages[SMS_MAX_MESSAGES_PER_CONV]; // Messages
    uint16_t messageCount;                          // Nombre de messages
    uint16_t unreadCount;                           // Messages non lus
    uint32_t lastActivity;                          // Dernière activité
    bool pinned;                                    // Épinglée ?
    bool muted;                                     // Notifications muettes ?
} SMSConversation;

/**
 * @brief Modèle de message
 */
typedef struct {
    char name[32];                      // Nom du modèle
    char content[SMS_TEMPLATE_MAX_LENGTH]; // Contenu
} SMSTemplate;

/**
 * @brief Brouillon
 */
typedef struct {
    char recipient[IDENTITY_PHONE_NUMBER_MAX];  // Destinataire
    char content[SMS_MAX_LENGTH];               // Contenu
    uint32_t timestamp;                         // Date
} SMSDraft;

// ============================================================
// SECTION 3 : ÉTAT DU SERVICE
// ============================================================

/**
 * @brief État du service SMS
 */
typedef struct {
    bool initialized;                   // Service initialisé
    
    // Conversations
    SMSConversation conversations[SMS_MAX_CONVERSATIONS];
    uint16_t conversationCount;
    
    // Brouillons
    SMSDraft drafts[SMS_MAX_DRAFTS];
    uint8_t draftCount;
    
    // Modèles
    SMSTemplate templates[SMS_MAX_TEMPLATES];
    uint8_t templateCount;
    
    // Statistiques
    uint32_t totalSent;
    uint32_t totalReceived;
    uint32_t totalFailed;
    uint16_t totalUnread;
    
} SMSServiceState;

// ============================================================
// SECTION 4 : CALLBACKS
// ============================================================

typedef void (*SMSService_ReceivedCallback)(const char* sender, const char* message);
typedef void (*SMSService_SentCallback)(const char* recipient);
typedef void (*SMSService_FailedCallback)(const char* recipient, uint8_t error);
typedef void (*SMSService_NewMessageCallback)(uint16_t unreadCount);

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

bool sms_service_init(void);
void sms_service_deinit(void);
bool sms_service_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS D'ENVOI
// ============================================================

bool sms_service_send(const char* recipient, const char* message);
bool sms_service_send_template(const char* recipient, uint8_t templateIndex);
bool sms_service_reply_to_last(const char* message);
bool sms_service_forward(const SMSMessage* msg, const char* newRecipient);

// ============================================================
// SECTION 7 : FONCTIONS DE RÉCEPTION
// ============================================================

uint16_t sms_service_get_unread_count(void);
uint16_t sms_service_get_total_count(void);
void sms_service_mark_all_read(void);

// ============================================================
// SECTION 8 : FONCTIONS DE CONVERSATIONS
// ============================================================

SMSConversation* sms_service_get_conversation(const char* contactNumber);
SMSConversation* sms_service_get_or_create_conversation(const char* contactNumber);
uint16_t sms_service_get_conversation_count(void);
bool sms_service_delete_conversation(const char* contactNumber);
void sms_service_pin_conversation(const char* contactNumber, bool pin);
void sms_service_mute_conversation(const char* contactNumber, bool mute);

// ============================================================
// SECTION 9 : FONCTIONS DE BROUILLONS
// ============================================================

bool sms_service_save_draft(const char* recipient, const char* content);
bool sms_service_delete_draft(uint8_t index);
SMSDraft* sms_service_get_draft(uint8_t index);
uint8_t sms_service_get_draft_count(void);

// ============================================================
// SECTION 10 : FONCTIONS DE MODÈLES
// ============================================================

bool sms_service_add_template(const char* name, const char* content);
bool sms_service_delete_template(uint8_t index);
SMSTemplate* sms_service_get_template(uint8_t index);
uint8_t sms_service_get_template_count(void);

// ============================================================
// SECTION 11 : FONCTIONS DE RECHERCHE
// ============================================================

uint16_t sms_service_search(const char* query, SMSMessage* results, uint16_t maxResults);

// ============================================================
// SECTION 12 : FONCTIONS DE CALLBACKS
// ============================================================

void sms_service_set_received_callback(SMSService_ReceivedCallback callback);
void sms_service_set_sent_callback(SMSService_SentCallback callback);
void sms_service_set_failed_callback(SMSService_FailedCallback callback);
void sms_service_set_new_message_callback(SMSService_NewMessageCallback callback);

// ============================================================
// SECTION 13 : FONCTIONS DE TRAITEMENT
// ============================================================

void sms_service_process(void);

// ============================================================
// SECTION 14 : FONCTIONS DE DÉBOGAGE
// ============================================================

void sms_service_print_state(void);
void sms_service_print_conversations(void);
void sms_service_print_conversation(const char* contactNumber);
bool sms_service_self_test(void);

// ============================================================
// SECTION 15 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define SMS_SERVICE_DEBUG(fmt, ...) printf("[SMS_SVC] " fmt, ##__VA_ARGS__)
#else
    #define SMS_SERVICE_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 16 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SMS_SERVICE_H