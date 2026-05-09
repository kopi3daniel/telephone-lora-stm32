/**
 * @file sms_protocol.h
 * @brief Protocole de messagerie SMS pour le réseau LoRa
 * 
 * Ce fichier implémente le protocole de messagerie texte :
 * - Envoi de SMS (point à point)
 * - Accusé de réception (ACK)
 * - Notification de livraison
 * - Stockage et retransmission
 * - Messages longs (concaténation)
 * - Messages de groupe (broadcast)
 * 
 * Format d'un SMS :
 * ┌──────────┬──────────┬──────────┬──────────────┬──────────┐
 * │ Sender   │ Receiver │ Timestamp│ Message      │ CRC      │
 * │ 16 chars │ 16 chars │ 4 bytes  │ 0-160 chars  │ 2 bytes  │
 * └──────────┴──────────┴──────────┴──────────────┴──────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SMS_PROTOCOL_H
#define SMS_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "identity.h"
#include "../drivers/lora/lora_driver.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du protocole */
#define SMS_PROTOCOL_VERSION            "1.0.0"

/** @brief Longueur maximale d'un SMS (caractères) */
#define SMS_MAX_LENGTH                  160

/** @brief Longueur maximale d'un SMS long (caractères) */
#define SMS_MAX_LONG_LENGTH             640     // 4 segments

/** @brief Nombre maximum de SMS stockés */
#define SMS_MAX_STORED                  100

/** @brief Nombre maximum de conversations */
#define SMS_MAX_CONVERSATIONS           50

/** @brief Timeout d'envoi (secondes) */
#define SMS_SEND_TIMEOUT_S              30

/** @brief Nombre de tentatives d'envoi */
#define SMS_MAX_RETRIES                 3

/** @brief Délai entre les tentatives (secondes) */
#define SMS_RETRY_DELAY_S               10

/** @brief Taille maximale d'un message sur LoRa */
#define SMS_MAX_PACKET_SIZE             255

// ============================================================
// SECTION 2 : TYPES DE MESSAGES SMS
// ============================================================

/**
 * @brief Types de messages SMS
 */
typedef enum {
    SMS_TYPE_SUBMIT         = 0x30,     // Envoi d'un SMS
    SMS_TYPE_DELIVER        = 0x31,     // Remise d'un SMS
    SMS_TYPE_ACK            = 0x32,     // Accusé de réception
    SMS_TYPE_ERROR          = 0x33,     // Erreur
    SMS_TYPE_STATUS_REPORT  = 0x34,     // Rapport de statut
    SMS_TYPE_COMMAND        = 0x35      // Commande SMS
} SMSType;

/**
 * @brief États d'un SMS
 */
typedef enum {
    SMS_STATE_DRAFT         = 0,    // Brouillon
    SMS_STATE_SENDING       = 1,    // En cours d'envoi
    SMS_STATE_SENT          = 2,    // Envoyé
    SMS_STATE_DELIVERED     = 3,    // Remis au destinataire
    SMS_STATE_FAILED        = 4,    // Échec
    SMS_STATE_RECEIVED      = 5,    // Reçu
    SMS_STATE_READ          = 6     // Lu
} SMSState;

/**
 * @brief Priorité du SMS
 */
typedef enum {
    SMS_PRIORITY_NORMAL     = 0,    // Normal
    SMS_PRIORITY_HIGH       = 1,    // Important
    SMS_PRIORITY_URGENT     = 2     // Urgent
} SMSPriority;

// ============================================================
// SECTION 3 : STRUCTURES DE DONNÉES
// ============================================================

/**
 * @brief Structure d'un SMS
 */
typedef struct {
    uint32_t smsId;                     // Identifiant unique
    
    // Expéditeur / Destinataire
    char sender[IDENTITY_PHONE_NUMBER_MAX];     // Numéro expéditeur
    char receiver[IDENTITY_PHONE_NUMBER_MAX];   // Numéro destinataire
    
    // Contenu
    char message[SMS_MAX_LENGTH + 1];           // Message (max 160 chars)
    uint16_t messageLength;                     // Longueur réelle
    
    // Métadonnées
    uint32_t timestamp;                         // Horodatage
    SMSState state;                             // État
    SMSPriority priority;                       // Priorité
    bool isRead;                                // Lu ?
    bool isIncoming;                            // Entrant ?
    
    // SMS long
    bool isConcatenated;                        // SMS concaténé ?
    uint8_t totalSegments;                      // Nombre total de segments
    uint8_t segmentIndex;                       // Index de ce segment
    uint32_t concatReference;                   // Référence de concaténation
    
    // Livraison
    uint8_t retryCount;                         // Nombre de tentatives
    uint32_t lastRetryTime;                     // Dernière tentative
    
} SMSMessage;

/**
 * @brief Format du paquet SMS sur LoRa
 */
typedef struct __attribute__((packed)) {
    uint8_t type;                               // Type (SMSType)
    uint32_t smsId;                             // Identifiant
    
    char sender[IDENTITY_PHONE_NUMBER_MAX];     // Expéditeur
    char receiver[IDENTITY_PHONE_NUMBER_MAX];   // Destinataire
    
    uint32_t timestamp;                         // Horodatage
    uint8_t priority;                           // Priorité
    uint8_t flags;                              // Flags (concaténé, etc.)
    
    uint16_t messageLength;                     // Longueur du message
    char message[SMS_MAX_LENGTH];               // Message
    
    uint8_t totalSegments;                      // Segments total
    uint8_t segmentIndex;                       // Index segment
    uint32_t concatReference;                   // Réf. concaténation
    
    uint16_t crc;                               // CRC16
} SMSPacket;

/**
 * @brief Conversation (fil de discussion)
 */
typedef struct {
    char contact[IDENTITY_PHONE_NUMBER_MAX];    // Numéro du contact
    char contactName[IDENTITY_DEVICE_NAME_MAX]; // Nom du contact
    SMSMessage messages[SMS_MAX_STORED];        // Messages de la conversation
    uint16_t messageCount;                      // Nombre de messages
    uint32_t lastActivity;                      // Dernière activité
    uint16_t unreadCount;                       // Messages non lus
} SMSConversation;

// ============================================================
// SECTION 4 : ÉTAT DU PROTOCOLE SMS
// ============================================================

/**
 * @brief État du module SMS
 */
typedef struct {
    bool initialized;                           // Module initialisé
    
    // Messages
    SMSMessage inbox[SMS_MAX_STORED];           // Boîte de réception
    uint16_t inboxCount;                        // Nombre de messages reçus
    
    SMSMessage outbox[SMS_MAX_STORED];          // Boîte d'envoi
    uint16_t outboxCount;                       // Nombre de messages envoyés
    
    SMSMessage pendingSend[SMS_MAX_STORED];     // Messages en attente d'envoi
    uint16_t pendingCount;                      // Nombre en attente
    
    // Conversations
    SMSConversation conversations[SMS_MAX_CONVERSATIONS];
    uint16_t conversationCount;
    
    // Statistiques
    uint32_t totalSent;                         // Total envoyés
    uint32_t totalReceived;                     // Total reçus
    uint32_t totalFailed;                       // Total échoués
    
    // Timers
    uint32_t lastSendTime;                      // Dernier envoi
    uint32_t lastReceiveTime;                   // Dernière réception
    
} SMSProtocolState;

// ============================================================
// SECTION 5 : CALLBACKS
// ============================================================

typedef void (*SMS_ReceivedCallback)(const SMSMessage* sms);
typedef void (*SMS_SentCallback)(const SMSMessage* sms);
typedef void (*SMS_FailedCallback)(const SMSMessage* sms, uint8_t errorCode);
typedef void (*SMS_DeliveredCallback)(const SMSMessage* sms);
typedef void (*SMS_NewMessageCallback)(uint16_t unreadCount);

// ============================================================
// SECTION 6 : FONCTIONS D'INITIALISATION
// ============================================================

bool sms_protocol_init(void);
void sms_protocol_deinit(void);
bool sms_protocol_is_ready(void);

// ============================================================
// SECTION 7 : FONCTIONS D'ENVOI
// ============================================================

bool sms_protocol_send(const char* receiver, const char* message);
bool sms_protocol_send_priority(const char* receiver, const char* message, SMSPriority priority);
bool sms_protocol_send_long(const char* receiver, const char* message);
bool sms_protocol_resend(uint32_t smsId);
bool sms_protocol_cancel_send(uint32_t smsId);

// ============================================================
// SECTION 8 : FONCTIONS DE RÉCEPTION
// ============================================================

const SMSMessage* sms_protocol_get_message(uint32_t smsId);
uint16_t sms_protocol_get_inbox(SMSMessage* messages, uint16_t maxCount);
uint16_t sms_protocol_get_unread_count(void);
void sms_protocol_mark_as_read(uint32_t smsId);
void sms_protocol_mark_all_read(void);
bool sms_protocol_delete_message(uint32_t smsId);
void sms_protocol_delete_all(void);

// ============================================================
// SECTION 9 : FONCTIONS DE CONVERSATIONS
// ============================================================

uint16_t sms_protocol_get_conversations(SMSConversation* conversations, uint16_t maxCount);
SMSConversation* sms_protocol_get_conversation(const char* contact);
uint16_t sms_protocol_get_conversation_count(void);
bool sms_protocol_delete_conversation(const char* contact);

// ============================================================
// SECTION 10 : FONCTIONS DE TRAITEMENT
// ============================================================

void sms_protocol_process(void);
void sms_protocol_process_packet(const SMSPacket* packet);
void sms_protocol_build_packet(SMSPacket* packet, const SMSMessage* sms);

// ============================================================
// SECTION 11 : FONCTIONS DE STOCKAGE
// ============================================================

bool sms_protocol_save_all(void);
bool sms_protocol_load_all(void);
void sms_protocol_clear_all(void);

// ============================================================
// SECTION 12 : FONCTIONS DE CALLBACKS
// ============================================================

void sms_protocol_set_received_callback(SMS_ReceivedCallback callback);
void sms_protocol_set_sent_callback(SMS_SentCallback callback);
void sms_protocol_set_failed_callback(SMS_FailedCallback callback);
void sms_protocol_set_delivered_callback(SMS_DeliveredCallback callback);
void sms_protocol_set_new_message_callback(SMS_NewMessageCallback callback);

// ============================================================
// SECTION 13 : FONCTIONS DE DÉBOGAGE
// ============================================================

void sms_protocol_print_state(void);
void sms_protocol_print_message(const SMSMessage* sms);
void sms_protocol_print_inbox(void);
void sms_protocol_print_conversation(const char* contact);
bool sms_protocol_self_test(void);

// ============================================================
// SECTION 14 : MACROS UTILITAIRES
// ============================================================

#define SMS_GET_UNREAD()                sms_protocol_get_unread_count()
#define SMS_HAS_UNREAD()               (sms_protocol_get_unread_count() > 0)

// ============================================================
// SECTION 15 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define SMS_DEBUG(fmt, ...)         printf("[SMS] " fmt, ##__VA_ARGS__)
#else
    #define SMS_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 16 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SMS_PROTOCOL_H