/**
 * @file sms_protocol.cpp
 * @brief Implémentation du protocole de messagerie SMS
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans sms_protocol.h.
 * 
 * Il gère :
 * - L'envoi de SMS avec accusé de réception
 * - La réception et le stockage des messages
 * - Les conversations (fils de discussion)
 * - Les SMS longs (concaténation)
 * - La persistance des messages
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "sms_protocol.h"
#include "../drivers/lora/lora_driver.h"
#include "../drivers/storage/flash_eeprom.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du protocole SMS */
static SMSProtocolState sms_state;

/** @brief Callbacks */
static SMS_ReceivedCallback received_cb = NULL;
static SMS_SentCallback sent_cb = NULL;
static SMS_FailedCallback failed_cb = NULL;
static SMS_DeliveredCallback delivered_cb = NULL;
static SMS_NewMessageCallback new_message_cb = NULL;

/** @brief Compteur d'ID SMS */
static uint32_t sms_id_counter = 0;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le protocole SMS
 */
bool sms_protocol_init(void)
{
    SMS_DEBUG("Initialisation du protocole SMS...\n");
    
    memset(&sms_state, 0, sizeof(SMSProtocolState));
    
    // Tenter de charger les messages sauvegardés
    if (!sms_protocol_load_all())
    {
        SMS_DEBUG("Aucun message sauvegardé\n");
    }
    
    sms_state.initialized = true;
    
    SMS_DEBUG("Protocole SMS initialisé\n");
    return true;
}

void sms_protocol_deinit(void)
{
    sms_protocol_save_all();
    sms_state.initialized = false;
}

bool sms_protocol_is_ready(void)
{
    return sms_state.initialized;
}

// ============================================================
// SECTION 2 : ENVOI DE SMS
// ============================================================

/**
 * @brief Envoie un SMS
 */
bool sms_protocol_send(const char* receiver, const char* message)
{
    return sms_protocol_send_priority(receiver, message, SMS_PRIORITY_NORMAL);
}

/**
 * @brief Envoie un SMS avec priorité
 */
bool sms_protocol_send_priority(const char* receiver, const char* message, SMSPriority priority)
{
    if (!sms_state.initialized) return false;
    if (receiver == NULL || message == NULL) return false;
    
    uint16_t msgLen = strlen(message);
    if (msgLen == 0) return false;
    
    // Vérifier si on doit faire un SMS long
    if (msgLen > SMS_MAX_LENGTH)
    {
        return sms_protocol_send_long(receiver, message);
    }
    
    SMS_DEBUG("Envoi SMS à %s (%d caractères)\n", receiver, msgLen);
    
    // Créer le message
    SMSMessage sms;
    memset(&sms, 0, sizeof(SMSMessage));
    
    sms.smsId = ++sms_id_counter;
    DeviceIdentity* identity = identity_get();
    strncpy(sms.sender, identity->msisdn, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(sms.receiver, receiver, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(sms.message, message, SMS_MAX_LENGTH);
    sms.messageLength = msgLen;
    sms.timestamp = HAL_GetTick();
    sms.state = SMS_STATE_SENDING;
    sms.priority = priority;
    sms.isIncoming = false;
    
    // Construire le paquet
    SMSPacket packet;
    sms_protocol_build_packet(&packet, &sms);
    
    // Passer en mode SMS pour une meilleure portée
    lora_driver_set_profile(PROFILE_SMS);
    
    // Envoyer via LoRa
    LoRaPacket loraPacket;
    loraPacket.type = PACKET_SMS;
    loraPacket.packetId = sms.smsId;
    strncpy(loraPacket.sender, sms.sender, 15);
    strncpy(loraPacket.receiver, receiver, 15);
    loraPacket.dataLength = sizeof(SMSPacket);
    memcpy(loraPacket.data, &packet, sizeof(SMSPacket));
    
    if (lora_driver_send_packet(&loraPacket, true))  // Attendre ACK
    {
        sms.state = SMS_STATE_SENT;
        sms_state.totalSent++;
        
        // Ajouter à la boîte d'envoi
        add_to_outbox(&sms);
        
        // Mettre à jour la conversation
        update_conversation(receiver, &sms);
        
        SMS_DEBUG("SMS envoyé (ID=%lu)\n", (unsigned long)sms.smsId);
        
        if (sent_cb) sent_cb(&sms);
        
        // Revenir au profil précédent
        lora_driver_set_profile(PROFILE_BALANCED);
        
        return true;
    }
    else
    {
        // Échec - mettre en file d'attente
        sms.state = SMS_STATE_FAILED;
        sms.retryCount = 1;
        sms.lastRetryTime = HAL_GetTick();
        
        add_to_pending(&sms);
        sms_state.totalFailed++;
        
        SMS_DEBUG("Échec envoi SMS, mis en attente\n");
        
        if (failed_cb) failed_cb(&sms, 1);
        
        lora_driver_set_profile(PROFILE_BALANCED);
        return false;
    }
}

/**
 * @brief Envoie un SMS long (concaténé)
 */
bool sms_protocol_send_long(const char* receiver, const char* message)
{
    if (receiver == NULL || message == NULL) return false;
    
    uint16_t totalLen = strlen(message);
    uint8_t segments = (totalLen + SMS_MAX_LENGTH - 1) / SMS_MAX_LENGTH;
    
    if (segments > 4) segments = 4;  // Max 4 segments
    
    SMS_DEBUG("Envoi SMS long : %d caractères en %d segments\n", totalLen, segments);
    
    uint32_t concatRef = ++sms_id_counter;
    bool allSent = true;
    
    for (uint8_t i = 0; i < segments; i++)
    {
        SMSMessage sms;
        memset(&sms, 0, sizeof(SMSMessage));
        
        sms.smsId = ++sms_id_counter;
        DeviceIdentity* identity = identity_get();
        strncpy(sms.sender, identity->msisdn, IDENTITY_PHONE_NUMBER_MAX - 1);
        strncpy(sms.receiver, receiver, IDENTITY_PHONE_NUMBER_MAX - 1);
        
        // Copier le segment
        uint16_t start = i * SMS_MAX_LENGTH;
        uint16_t segLen = (start + SMS_MAX_LENGTH <= totalLen) ? SMS_MAX_LENGTH : (totalLen - start);
        strncpy(sms.message, message + start, segLen);
        sms.message[segLen] = '\0';
        sms.messageLength = segLen;
        
        sms.timestamp = HAL_GetTick();
        sms.state = SMS_STATE_SENDING;
        sms.isIncoming = false;
        sms.isConcatenated = true;
        sms.totalSegments = segments;
        sms.segmentIndex = i;
        sms.concatReference = concatRef;
        
        // Construire et envoyer
        SMSPacket packet;
        sms_protocol_build_packet(&packet, &sms);
        
        LoRaPacket loraPacket;
        loraPacket.type = PACKET_SMS;
        loraPacket.packetId = sms.smsId;
        strncpy(loraPacket.sender, sms.sender, 15);
        strncpy(loraPacket.receiver, receiver, 15);
        loraPacket.dataLength = sizeof(SMSPacket);
        memcpy(loraPacket.data, &packet, sizeof(SMSPacket));
        
        if (lora_driver_send_packet(&loraPacket, true))
        {
            sms.state = SMS_STATE_SENT;
            add_to_outbox(&sms);
            sms_state.totalSent++;
        }
        else
        {
            sms.state = SMS_STATE_FAILED;
            add_to_pending(&sms);
            allSent = false;
        }
        
        // Pause entre les segments
        HAL_Delay(500);
    }
    
    update_conversation(receiver, NULL);
    
    return allSent;
}

/**
 * @brief Renvoie un SMS en échec
 */
bool sms_protocol_resend(uint32_t smsId)
{
    // Chercher dans les messages en attente
    for (uint16_t i = 0; i < sms_state.pendingCount; i++)
    {
        if (sms_state.pendingSend[i].smsId == smsId)
        {
            SMSMessage* sms = &sms_state.pendingSend[i];
            sms->retryCount++;
            sms->lastRetryTime = HAL_GetTick();
            
            return sms_protocol_send_priority(sms->receiver, sms->message, sms->priority);
        }
    }
    return false;
}

/**
 * @brief Annule l'envoi d'un SMS
 */
bool sms_protocol_cancel_send(uint32_t smsId)
{
    for (uint16_t i = 0; i < sms_state.pendingCount; i++)
    {
        if (sms_state.pendingSend[i].smsId == smsId)
        {
            // Supprimer de la file d'attente
            if (i < sms_state.pendingCount - 1)
            {
                memmove(&sms_state.pendingSend[i], 
                        &sms_state.pendingSend[i + 1],
                        (sms_state.pendingCount - i - 1) * sizeof(SMSMessage));
            }
            sms_state.pendingCount--;
            return true;
        }
    }
    return false;
}

// ============================================================
// SECTION 3 : RÉCEPTION
// ============================================================

/**
 * @brief Récupère un message par son ID
 */
const SMSMessage* sms_protocol_get_message(uint32_t smsId)
{
    // Chercher dans la boîte de réception
    for (uint16_t i = 0; i < sms_state.inboxCount; i++)
    {
        if (sms_state.inbox[i].smsId == smsId)
            return &sms_state.inbox[i];
    }
    
    // Chercher dans la boîte d'envoi
    for (uint16_t i = 0; i < sms_state.outboxCount; i++)
    {
        if (sms_state.outbox[i].smsId == smsId)
            return &sms_state.outbox[i];
    }
    
    return NULL;
}

uint16_t sms_protocol_get_inbox(SMSMessage* messages, uint16_t maxCount)
{
    if (messages == NULL) return 0;
    uint16_t count = (sms_state.inboxCount < maxCount) ? sms_state.inboxCount : maxCount;
    memcpy(messages, sms_state.inbox, count * sizeof(SMSMessage));
    return count;
}

uint16_t sms_protocol_get_unread_count(void)
{
    uint16_t count = 0;
    for (uint16_t i = 0; i < sms_state.inboxCount; i++)
    {
        if (!sms_state.inbox[i].isRead) count++;
    }
    return count;
}

void sms_protocol_mark_as_read(uint32_t smsId)
{
    for (uint16_t i = 0; i < sms_state.inboxCount; i++)
    {
        if (sms_state.inbox[i].smsId == smsId)
        {
            sms_state.inbox[i].isRead = true;
            SMS_DEBUG("Message %lu marqué comme lu\n", (unsigned long)smsId);
            return;
        }
    }
}

void sms_protocol_mark_all_read(void)
{
    for (uint16_t i = 0; i < sms_state.inboxCount; i++)
    {
        sms_state.inbox[i].isRead = true;
    }
}

bool sms_protocol_delete_message(uint32_t smsId)
{
    // Chercher dans la boîte de réception
    for (uint16_t i = 0; i < sms_state.inboxCount; i++)
    {
        if (sms_state.inbox[i].smsId == smsId)
        {
            if (i < sms_state.inboxCount - 1)
            {
                memmove(&sms_state.inbox[i], &sms_state.inbox[i + 1],
                        (sms_state.inboxCount - i - 1) * sizeof(SMSMessage));
            }
            sms_state.inboxCount--;
            return true;
        }
    }
    return false;
}

void sms_protocol_delete_all(void)
{
    sms_state.inboxCount = 0;
    sms_state.outboxCount = 0;
    sms_state.pendingCount = 0;
    sms_state.conversationCount = 0;
    memset(sms_state.inbox, 0, sizeof(sms_state.inbox));
    memset(sms_state.outbox, 0, sizeof(sms_state.outbox));
    memset(sms_state.pendingSend, 0, sizeof(sms_state.pendingSend));
    memset(sms_state.conversations, 0, sizeof(sms_state.conversations));
}

// ============================================================
// SECTION 4 : CONVERSATIONS
// ============================================================

uint16_t sms_protocol_get_conversations(SMSConversation* conversations, uint16_t maxCount)
{
    if (conversations == NULL) return 0;
    uint16_t count = (sms_state.conversationCount < maxCount) ? 
                      sms_state.conversationCount : maxCount;
    memcpy(conversations, sms_state.conversations, count * sizeof(SMSConversation));
    return count;
}

SMSConversation* sms_protocol_get_conversation(const char* contact)
{
    if (contact == NULL) return NULL;
    
    for (uint16_t i = 0; i < sms_state.conversationCount; i++)
    {
        if (strcmp(sms_state.conversations[i].contact, contact) == 0)
            return &sms_state.conversations[i];
    }
    return NULL;
}

uint16_t sms_protocol_get_conversation_count(void)
{
    return sms_state.conversationCount;
}

bool sms_protocol_delete_conversation(const char* contact)
{
    if (contact == NULL) return false;
    
    for (uint16_t i = 0; i < sms_state.conversationCount; i++)
    {
        if (strcmp(sms_state.conversations[i].contact, contact) == 0)
        {
            if (i < sms_state.conversationCount - 1)
            {
                memmove(&sms_state.conversations[i], &sms_state.conversations[i + 1],
                        (sms_state.conversationCount - i - 1) * sizeof(SMSConversation));
            }
            sms_state.conversationCount--;
            return true;
        }
    }
    return false;
}

// ============================================================
// SECTION 5 : TRAITEMENT
// ============================================================

/**
 * @brief Traitement périodique
 */
void sms_protocol_process(void)
{
    if (!sms_state.initialized) return;
    
    // Vérifier les messages en attente de renvoi
    for (uint16_t i = 0; i < sms_state.pendingCount; i++)
    {
        SMSMessage* sms = &sms_state.pendingSend[i];
        
        uint32_t elapsed = (HAL_GetTick() - sms->lastRetryTime) / 1000;
        
        if (elapsed >= SMS_RETRY_DELAY_S)
        {
            if (sms->retryCount < SMS_MAX_RETRIES)
            {
                SMS_DEBUG("Nouvelle tentative pour SMS %lu (%d/%d)\n",
                         (unsigned long)sms->smsId, sms->retryCount + 1, SMS_MAX_RETRIES);
                
                sms_protocol_resend(sms->smsId);
            }
            else
            {
                SMS_DEBUG("SMS %lu abandonné après %d tentatives\n",
                         (unsigned long)sms->smsId, SMS_MAX_RETRIES);
                
                sms->state = SMS_STATE_FAILED;
                if (failed_cb) failed_cb(sms, 0xFF);
                
                // Supprimer de la file d'attente
                sms_protocol_cancel_send(sms->smsId);
            }
        }
    }
}

/**
 * @brief Traite un paquet SMS reçu
 */
void sms_protocol_process_packet(const SMSPacket* packet)
{
    if (packet == NULL) return;
    
    SMS_DEBUG("Paquet SMS reçu : type=0x%02X, ID=%lu\n", 
             packet->type, (unsigned long)packet->smsId);
    
    switch (packet->type)
    {
        case SMS_TYPE_SUBMIT:
        case SMS_TYPE_DELIVER:
            handle_incoming_sms(packet);
            break;
            
        case SMS_TYPE_ACK:
            handle_ack(packet);
            break;
            
        case SMS_TYPE_STATUS_REPORT:
            handle_status_report(packet);
            break;
            
        case SMS_TYPE_ERROR:
            handle_error(packet);
            break;
            
        default:
            SMS_DEBUG("Type SMS inconnu: 0x%02X\n", packet->type);
            break;
    }
}

/**
 * @brief Gère un SMS entrant
 */
static void handle_incoming_sms(const SMSPacket* packet)
{
    SMS_DEBUG("SMS reçu de %s\n", packet->sender);
    
    // Créer le message
    SMSMessage sms;
    memset(&sms, 0, sizeof(SMSMessage));
    
    sms.smsId = packet->smsId;
    strncpy(sms.sender, packet->sender, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(sms.receiver, packet->receiver, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(sms.message, packet->message, SMS_MAX_LENGTH);
    sms.messageLength = packet->messageLength;
    sms.timestamp = packet->timestamp;
    sms.state = SMS_STATE_RECEIVED;
    sms.priority = (SMSPriority)packet->priority;
    sms.isIncoming = true;
    sms.isRead = false;
    sms.isConcatenated = (packet->totalSegments > 1);
    sms.totalSegments = packet->totalSegments;
    sms.segmentIndex = packet->segmentIndex;
    sms.concatReference = packet->concatReference;
    
    // Ajouter à la boîte de réception
    add_to_inbox(&sms);
    sms_state.totalReceived++;
    sms_state.lastReceiveTime = HAL_GetTick();
    
    // Mettre à jour la conversation
    update_conversation(packet->sender, &sms);
    
    // Envoyer l'ACK
    send_ack(packet);
    
    // Notifier
    if (received_cb) received_cb(&sms);
    if (new_message_cb) new_message_cb(sms_protocol_get_unread_count());
}

/**
 * @brief Gère un accusé de réception
 */
static void handle_ack(const SMSPacket* packet)
{
    SMS_DEBUG("ACK reçu pour SMS %lu\n", (unsigned long)packet->smsId);
    
    // Mettre à jour l'état du message
    for (uint16_t i = 0; i < sms_state.outboxCount; i++)
    {
        if (sms_state.outbox[i].smsId == packet->smsId)
        {
            sms_state.outbox[i].state = SMS_STATE_DELIVERED;
            
            if (delivered_cb) delivered_cb(&sms_state.outbox[i]);
            break;
        }
    }
}

/**
 * @brief Gère un rapport de statut
 */
static void handle_status_report(const SMSPacket* packet)
{
    SMS_DEBUG("Status report pour SMS %lu\n", (unsigned long)packet->smsId);
}

/**
 * @brief Gère une erreur
 */
static void handle_error(const SMSPacket* packet)
{
    SMS_DEBUG("Erreur SMS %lu\n", (unsigned long)packet->smsId);
    
    for (uint16_t i = 0; i < sms_state.outboxCount; i++)
    {
        if (sms_state.outbox[i].smsId == packet->smsId)
        {
            sms_state.outbox[i].state = SMS_STATE_FAILED;
            if (failed_cb) failed_cb(&sms_state.outbox[i], 0xFE);
            break;
        }
    }
}

/**
 * @brief Envoie un accusé de réception
 */
static void send_ack(const SMSPacket* originalPacket)
{
    SMSPacket ack;
    memset(&ack, 0, sizeof(SMSPacket));
    
    ack.type = SMS_TYPE_ACK;
    ack.smsId = originalPacket->smsId;
    ack.timestamp = HAL_GetTick();
    
    DeviceIdentity* identity = identity_get();
    strncpy(ack.sender, identity->msisdn, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(ack.receiver, originalPacket->sender, IDENTITY_PHONE_NUMBER_MAX - 1);
    
    LoRaPacket loraPacket;
    loraPacket.type = PACKET_SMS_ACK;
    loraPacket.packetId = ack.smsId;
    strncpy(loraPacket.sender, ack.sender, 15);
    strncpy(loraPacket.receiver, ack.receiver, 15);
    loraPacket.dataLength = sizeof(SMSPacket);
    memcpy(loraPacket.data, &ack, sizeof(SMSPacket));
    
    lora_driver_send_packet(&loraPacket, false);
}

/**
 * @brief Construit un paquet SMS
 */
void sms_protocol_build_packet(SMSPacket* packet, const SMSMessage* sms)
{
    if (packet == NULL || sms == NULL) return;
    
    memset(packet, 0, sizeof(SMSPacket));
    
    packet->type = SMS_TYPE_SUBMIT;
    packet->smsId = sms->smsId;
    packet->timestamp = sms->timestamp;
    packet->priority = sms->priority;
    packet->messageLength = sms->messageLength;
    
    strncpy(packet->sender, sms->sender, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(packet->receiver, sms->receiver, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(packet->message, sms->message, SMS_MAX_LENGTH);
    
    if (sms->isConcatenated)
    {
        packet->totalSegments = sms->totalSegments;
        packet->segmentIndex = sms->segmentIndex;
        packet->concatReference = sms->concatReference;
    }
}

// ============================================================
// SECTION 6 : GESTION INTERNE
// ============================================================

static void add_to_inbox(SMSMessage* sms)
{
    if (sms_state.inboxCount >= SMS_MAX_STORED)
    {
        // Supprimer le plus ancien
        memmove(&sms_state.inbox[0], &sms_state.inbox[1],
                (SMS_MAX_STORED - 1) * sizeof(SMSMessage));
        sms_state.inboxCount = SMS_MAX_STORED - 1;
    }
    
    memcpy(&sms_state.inbox[sms_state.inboxCount], sms, sizeof(SMSMessage));
    sms_state.inboxCount++;
}

static void add_to_outbox(SMSMessage* sms)
{
    if (sms_state.outboxCount >= SMS_MAX_STORED)
    {
        memmove(&sms_state.outbox[0], &sms_state.outbox[1],
                (SMS_MAX_STORED - 1) * sizeof(SMSMessage));
        sms_state.outboxCount = SMS_MAX_STORED - 1;
    }
    
    memcpy(&sms_state.outbox[sms_state.outboxCount], sms, sizeof(SMSMessage));
    sms_state.outboxCount++;
}

static void add_to_pending(SMSMessage* sms)
{
    if (sms_state.pendingCount >= SMS_MAX_STORED) return;
    
    memcpy(&sms_state.pendingSend[sms_state.pendingCount], sms, sizeof(SMSMessage));
    sms_state.pendingCount++;
}

static void update_conversation(const char* contact, SMSMessage* sms)
{
    if (contact == NULL) return;
    
    // Chercher une conversation existante
    SMSConversation* conv = NULL;
    
    for (uint16_t i = 0; i < sms_state.conversationCount; i++)
    {
        if (strcmp(sms_state.conversations[i].contact, contact) == 0)
        {
            conv = &sms_state.conversations[i];
            break;
        }
    }
    
    // Créer une nouvelle conversation si nécessaire
    if (conv == NULL)
    {
        if (sms_state.conversationCount >= SMS_MAX_CONVERSATIONS) return;
        
        conv = &sms_state.conversations[sms_state.conversationCount++];
        memset(conv, 0, sizeof(SMSConversation));
        strncpy(conv->contact, contact, IDENTITY_PHONE_NUMBER_MAX - 1);
    }
    
    // Ajouter le message
    if (sms != NULL && conv->messageCount < SMS_MAX_STORED)
    {
        memcpy(&conv->messages[conv->messageCount], sms, sizeof(SMSMessage));
        conv->messageCount++;
        
        if (!sms->isRead) conv->unreadCount++;
    }
    
    conv->lastActivity = HAL_GetTick();
}

// ============================================================
// SECTION 7 : STOCKAGE
// ============================================================

bool sms_protocol_save_all(void)
{
    // Sauvegarder les messages en Flash
    FlashEEPROM_Error err;
    
    err = flash_eeprom_write(EEPROM_ID_SMS, (uint8_t*)&sms_state, sizeof(SMSProtocolState));
    return (err == FLASH_EEPROM_OK);
}

bool sms_protocol_load_all(void)
{
    uint16_t readSize;
    FlashEEPROM_Error err = flash_eeprom_read(EEPROM_ID_SMS,
                                               (uint8_t*)&sms_state,
                                               sizeof(SMSProtocolState),
                                               &readSize);
    return (err == FLASH_EEPROM_OK && readSize >= sizeof(SMSProtocolState));
}

void sms_protocol_clear_all(void)
{
    sms_protocol_delete_all();
    sms_protocol_save_all();
}

// ============================================================
// SECTION 8 : CALLBACKS
// ============================================================

void sms_protocol_set_received_callback(SMS_ReceivedCallback cb) { received_cb = cb; }
void sms_protocol_set_sent_callback(SMS_SentCallback cb) { sent_cb = cb; }
void sms_protocol_set_failed_callback(SMS_FailedCallback cb) { failed_cb = cb; }
void sms_protocol_set_delivered_callback(SMS_DeliveredCallback cb) { delivered_cb = cb; }
void sms_protocol_set_new_message_callback(SMS_NewMessageCallback cb) { new_message_cb = cb; }

// ============================================================
// SECTION 9 : DÉBOGAGE
// ============================================================

void sms_protocol_print_state(void)
{
    printf("\n═══ ÉTAT PROTOCOLE SMS ═══\n");
    printf("Boîte réception : %d messages\n", sms_state.inboxCount);
    printf("Boîte envoi      : %d messages\n", sms_state.outboxCount);
    printf("En attente       : %d messages\n", sms_state.pendingCount);
    printf("Conversations    : %d\n", sms_state.conversationCount);
    printf("Non lus          : %d\n", sms_protocol_get_unread_count());
    printf("Envoyés          : %lu\n", (unsigned long)sms_state.totalSent);
    printf("Reçus            : %lu\n", (unsigned long)sms_state.totalReceived);
    printf("Échoués          : %lu\n", (unsigned long)sms_state.totalFailed);
    printf("══════════════════════════\n\n");
}

void sms_protocol_print_message(const SMSMessage* sms)
{
    if (sms == NULL) return;
    
    const char* direction = sms->isIncoming ? "←" : "→";
    const char* stateStr = "?";
    switch (sms->state)
    {
        case SMS_STATE_SENT:      stateStr = "Envoyé"; break;
        case SMS_STATE_DELIVERED: stateStr = "Remis"; break;
        case SMS_STATE_RECEIVED:  stateStr = "Reçu"; break;
        case SMS_STATE_FAILED:    stateStr = "Échec"; break;
        default: break;
    }
    
    printf("[%s] %s: %s (%s) %s\n",
           direction, sms->isIncoming ? sms->sender : sms->receiver,
           sms->message, stateStr, sms->isRead ? "" : "●");
}

void sms_protocol_print_inbox(void)
{
    printf("\n═══ BOÎTE DE RÉCEPTION (%d) ═══\n", sms_state.inboxCount);
    
    for (uint16_t i = 0; i < sms_state.inboxCount; i++)
    {
        sms_protocol_print_message(&sms_state.inbox[i]);
    }
    printf("══════════════════════════════\n\n");
}

void sms_protocol_print_conversation(const char* contact)
{
    SMSConversation* conv = sms_protocol_get_conversation(contact);
    
    if (conv == NULL)
    {
        printf("[SMS] Aucune conversation avec %s\n", contact);
        return;
    }
    
    printf("\n═══ CONVERSATION : %s (%d msg) ═══\n", contact, conv->messageCount);
    
    for (uint16_t i = 0; i < conv->messageCount; i++)
    {
        sms_protocol_print_message(&conv->messages[i]);
    }
    printf("══════════════════════════════════\n\n");
}

bool sms_protocol_self_test(void)
{
    SMS_DEBUG("Auto-test...\n");
    
    if (!sms_state.initialized)
    {
        SMS_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : construire un paquet
    SMSMessage testSms;
    memset(&testSms, 0, sizeof(SMSMessage));
    testSms.smsId = 1;
    strcpy(testSms.sender, "0600000000");
    strcpy(testSms.receiver, "0611111111");
    strcpy(testSms.message, "Test");
    testSms.messageLength = 4;
    
    SMSPacket packet;
    sms_protocol_build_packet(&packet, &testSms);
    
    if (packet.smsId != 1 || strcmp(packet.message, "Test") != 0)
    {
        SMS_DEBUG("Échec : paquet incorrect\n");
        return false;
    }
    
    SMS_DEBUG("Auto-test OK\n");
    return true;
}