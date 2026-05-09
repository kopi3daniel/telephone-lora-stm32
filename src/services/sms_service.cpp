/**
 * @file sms_service.cpp
 * @brief Implémentation du service de messagerie SMS
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans sms_service.h.
 * 
 * Il orchestre :
 * - Le protocole SMS (sms_protocol)
 * - Les conversations
 * - Les brouillons
 * - Les modèles de messages
 * - Les notifications
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "sms_service.h"
#include "phone_service.h"  // Pour les contacts
#include "../protocols/sms_protocol.h"
#include "../drivers/storage/flash_eeprom.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du service SMS */
static SMSServiceState sms_state;

/** @brief Callbacks */
static SMSService_ReceivedCallback received_cb = NULL;
static SMSService_SentCallback sent_cb = NULL;
static SMSService_FailedCallback failed_cb = NULL;
static SMSService_NewMessageCallback new_message_cb = NULL;

/** @brief Dernier expéditeur (pour répondre) */
static char last_sender[IDENTITY_PHONE_NUMBER_MAX] = "";

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le service SMS
 */
bool sms_service_init(void)
{
    SMS_SERVICE_DEBUG("Initialisation du service SMS...\n");
    
    memset(&sms_state, 0, sizeof(SMSServiceState));
    
    // Initialiser le protocole SMS
    if (!sms_protocol_init())
    {
        SMS_SERVICE_DEBUG("Échec initialisation protocole SMS\n");
        return false;
    }
    
    // Enregistrer les callbacks du protocole
    sms_protocol_set_received_callback(on_sms_received);
    sms_protocol_set_sent_callback(on_sms_sent);
    sms_protocol_set_failed_callback(on_sms_failed);
    sms_protocol_set_new_message_callback(on_new_message);
    
    // Charger les données sauvegardées
    sms_service_load_data();
    
    sms_state.initialized = true;
    
    SMS_SERVICE_DEBUG("Service initialisé\n");
    return true;
}

void sms_service_deinit(void)
{
    sms_service_save_data();
    sms_protocol_deinit();
    sms_state.initialized = false;
}

bool sms_service_is_ready(void)
{
    return sms_state.initialized;
}

// ============================================================
// SECTION 2 : ENVOI
// ============================================================

bool sms_service_send(const char* recipient, const char* message)
{
    if (!sms_state.initialized) return false;
    if (recipient == NULL || message == NULL) return false;
    
    SMS_SERVICE_DEBUG("Envoi SMS à %s\n", recipient);
    
    // Envoyer via le protocole
    if (sms_protocol_send(recipient, message))
    {
        // Ajouter à la conversation
        SMSConversation* conv = sms_service_get_or_create_conversation(recipient);
        if (conv)
        {
            SMSMessage* msg = &conv->messages[conv->messageCount++];
            memset(msg, 0, sizeof(SMSMessage));
            strncpy(msg->sender, identity_get_msisdn(), IDENTITY_PHONE_NUMBER_MAX - 1);
            strncpy(msg->receiver, recipient, IDENTITY_PHONE_NUMBER_MAX - 1);
            strncpy(msg->message, message, SMS_MAX_LENGTH);
            msg->messageLength = strlen(message);
            msg->timestamp = HAL_GetTick();
            msg->state = SMS_STATE_SENT;
            msg->isIncoming = false;
            msg->isRead = true;
            
            conv->lastActivity = HAL_GetTick();
        }
        
        sms_state.totalSent++;
        
        // Sauvegarder le dernier destinataire pour répondre
        strncpy(last_sender, recipient, IDENTITY_PHONE_NUMBER_MAX - 1);
        
        return true;
    }
    
    sms_state.totalFailed++;
    return false;
}

bool sms_service_send_template(const char* recipient, uint8_t templateIndex)
{
    if (templateIndex >= sms_state.templateCount) return false;
    return sms_service_send(recipient, sms_state.templates[templateIndex].content);
}

bool sms_service_reply_to_last(const char* message)
{
    if (strlen(last_sender) == 0) return false;
    return sms_service_send(last_sender, message);
}

bool sms_service_forward(const SMSMessage* msg, const char* newRecipient)
{
    if (msg == NULL || newRecipient == NULL) return false;
    return sms_service_send(newRecipient, msg->message);
}

// ============================================================
// SECTION 3 : RÉCEPTION
// ============================================================

uint16_t sms_service_get_unread_count(void)
{
    return sms_state.totalUnread;
}

uint16_t sms_service_get_total_count(void)
{
    uint16_t total = 0;
    for (uint16_t i = 0; i < sms_state.conversationCount; i++)
    {
        total += sms_state.conversations[i].messageCount;
    }
    return total;
}

void sms_service_mark_all_read(void)
{
    for (uint16_t i = 0; i < sms_state.conversationCount; i++)
    {
        sms_state.conversations[i].unreadCount = 0;
        
        for (uint16_t j = 0; j < sms_state.conversations[i].messageCount; j++)
        {
            sms_state.conversations[i].messages[j].isRead = true;
        }
    }
    sms_state.totalUnread = 0;
}

// ============================================================
// SECTION 4 : CONVERSATIONS
// ============================================================

SMSConversation* sms_service_get_conversation(const char* contactNumber)
{
    if (contactNumber == NULL) return NULL;
    
    for (uint16_t i = 0; i < sms_state.conversationCount; i++)
    {
        if (strcmp(sms_state.conversations[i].contactNumber, contactNumber) == 0)
        {
            return &sms_state.conversations[i];
        }
    }
    return NULL;
}

SMSConversation* sms_service_get_or_create_conversation(const char* contactNumber)
{
    SMSConversation* conv = sms_service_get_conversation(contactNumber);
    if (conv != NULL) return conv;
    
    if (sms_state.conversationCount >= SMS_MAX_CONVERSATIONS)
    {
        // Supprimer la plus ancienne
        remove_oldest_conversation();
    }
    
    conv = &sms_state.conversations[sms_state.conversationCount++];
    memset(conv, 0, sizeof(SMSConversation));
    strncpy(conv->contactNumber, contactNumber, IDENTITY_PHONE_NUMBER_MAX - 1);
    
    // Chercher le nom du contact
    PhoneContact* contact = phone_service_find_contact(contactNumber);
    if (contact)
    {
        strncpy(conv->contactName, contact->name, 31);
    }
    else
    {
        strncpy(conv->contactName, contactNumber, 31);
    }
    
    return conv;
}

uint16_t sms_service_get_conversation_count(void)
{
    return sms_state.conversationCount;
}

bool sms_service_delete_conversation(const char* contactNumber)
{
    for (uint16_t i = 0; i < sms_state.conversationCount; i++)
    {
        if (strcmp(sms_state.conversations[i].contactNumber, contactNumber) == 0)
        {
            // Mettre à jour le compteur de non lus
            sms_state.totalUnread -= sms_state.conversations[i].unreadCount;
            
            // Décaler
            if (i < sms_state.conversationCount - 1)
            {
                memmove(&sms_state.conversations[i], 
                        &sms_state.conversations[i + 1],
                        (sms_state.conversationCount - i - 1) * sizeof(SMSConversation));
            }
            sms_state.conversationCount--;
            return true;
        }
    }
    return false;
}

void sms_service_pin_conversation(const char* contactNumber, bool pin)
{
    SMSConversation* conv = sms_service_get_conversation(contactNumber);
    if (conv) conv->pinned = pin;
}

void sms_service_mute_conversation(const char* contactNumber, bool mute)
{
    SMSConversation* conv = sms_service_get_conversation(contactNumber);
    if (conv) conv->muted = mute;
}

// ============================================================
// SECTION 5 : BROUILLONS
// ============================================================

bool sms_service_save_draft(const char* recipient, const char* content)
{
    if (recipient == NULL || content == NULL) return false;
    if (sms_state.draftCount >= SMS_MAX_DRAFTS) return false;
    
    SMSDraft* draft = &sms_state.drafts[sms_state.draftCount++];
    memset(draft, 0, sizeof(SMSDraft));
    strncpy(draft->recipient, recipient, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(draft->content, content, SMS_MAX_LENGTH);
    draft->timestamp = HAL_GetTick();
    
    return true;
}

bool sms_service_delete_draft(uint8_t index)
{
    if (index >= sms_state.draftCount) return false;
    
    if (index < sms_state.draftCount - 1)
    {
        memmove(&sms_state.drafts[index], &sms_state.drafts[index + 1],
                (sms_state.draftCount - index - 1) * sizeof(SMSDraft));
    }
    sms_state.draftCount--;
    return true;
}

SMSDraft* sms_service_get_draft(uint8_t index)
{
    if (index >= sms_state.draftCount) return NULL;
    return &sms_state.drafts[index];
}

uint8_t sms_service_get_draft_count(void)
{
    return sms_state.draftCount;
}

// ============================================================
// SECTION 6 : MODÈLES
// ============================================================

bool sms_service_add_template(const char* name, const char* content)
{
    if (name == NULL || content == NULL) return false;
    if (sms_state.templateCount >= SMS_MAX_TEMPLATES) return false;
    
    SMSTemplate* tmpl = &sms_state.templates[sms_state.templateCount++];
    memset(tmpl, 0, sizeof(SMSTemplate));
    strncpy(tmpl->name, name, 31);
    strncpy(tmpl->content, content, SMS_TEMPLATE_MAX_LENGTH);
    
    return true;
}

bool sms_service_delete_template(uint8_t index)
{
    if (index >= sms_state.templateCount) return false;
    
    if (index < sms_state.templateCount - 1)
    {
        memmove(&sms_state.templates[index], &sms_state.templates[index + 1],
                (sms_state.templateCount - index - 1) * sizeof(SMSTemplate));
    }
    sms_state.templateCount--;
    return true;
}

SMSTemplate* sms_service_get_template(uint8_t index)
{
    if (index >= sms_state.templateCount) return NULL;
    return &sms_state.templates[index];
}

uint8_t sms_service_get_template_count(void)
{
    return sms_state.templateCount;
}

// ============================================================
// SECTION 7 : RECHERCHE
// ============================================================

uint16_t sms_service_search(const char* query, SMSMessage* results, uint16_t maxResults)
{
    if (query == NULL || results == NULL) return 0;
    
    uint16_t found = 0;
    
    for (uint16_t c = 0; c < sms_state.conversationCount && found < maxResults; c++)
    {
        SMSConversation* conv = &sms_state.conversations[c];
        
        for (uint16_t m = 0; m < conv->messageCount && found < maxResults; m++)
        {
            SMSMessage* msg = &conv->messages[m];
            
            // Chercher dans le message
            if (strstr(msg->message, query) != NULL)
            {
                memcpy(&results[found], msg, sizeof(SMSMessage));
                found++;
            }
        }
    }
    
    return found;
}

// ============================================================
// SECTION 8 : CALLBACKS INTERNES
// ============================================================

static void on_sms_received(const SMSMessage* sms)
{
    if (sms == NULL) return;
    
    SMS_SERVICE_DEBUG("SMS reçu de %s\n", sms->sender);
    
    // Ajouter à la conversation
    SMSConversation* conv = sms_service_get_or_create_conversation(sms->sender);
    if (conv && conv->messageCount < SMS_MAX_MESSAGES_PER_CONV)
    {
        memcpy(&conv->messages[conv->messageCount], sms, sizeof(SMSMessage));
        conv->messages[conv->messageCount].isIncoming = true;
        conv->messages[conv->messageCount].isRead = false;
        conv->messageCount++;
        conv->unreadCount++;
        conv->lastActivity = HAL_GetTick();
    }
    
    sms_state.totalReceived++;
    sms_state.totalUnread++;
    
    // Sauvegarder le dernier expéditeur
    strncpy(last_sender, sms->sender, IDENTITY_PHONE_NUMBER_MAX - 1);
    
    // Notifier
    if (received_cb) received_cb(sms->sender, sms->message);
    if (new_message_cb) new_message_cb(sms_state.totalUnread);
    
    // Jouer une notification sonore si la conversation n'est pas muette
    if (conv && !conv->muted)
    {
        audio_manager_play_beep(1000, 100);
    }
}

static void on_sms_sent(const SMSMessage* sms)
{
    if (sms == NULL) return;
    
    SMS_SERVICE_DEBUG("SMS envoyé à %s\n", sms->receiver);
    
    sms_state.totalSent++;
    
    if (sent_cb) sent_cb(sms->receiver);
}

static void on_sms_failed(const SMSMessage* sms, uint8_t errorCode)
{
    if (sms == NULL) return;
    
    SMS_SERVICE_DEBUG("Échec envoi SMS à %s (erreur %d)\n", sms->receiver, errorCode);
    
    sms_state.totalFailed++;
    
    if (failed_cb) failed_cb(sms->receiver, errorCode);
}

static void on_new_message(uint16_t unreadCount)
{
    sms_state.totalUnread = unreadCount;
    
    if (new_message_cb) new_message_cb(unreadCount);
}

// ============================================================
// SECTION 9 : PERSISTANCE
// ============================================================

static void sms_service_save_data(void)
{
    flash_eeprom_write(EEPROM_ID_SMS, (uint8_t*)&sms_state, sizeof(SMSServiceState));
}

static void sms_service_load_data(void)
{
    uint16_t readSize;
    FlashEEPROM_Error err = flash_eeprom_read(EEPROM_ID_SMS,
                                               (uint8_t*)&sms_state,
                                               sizeof(SMSServiceState),
                                               &readSize);
    
    if (err == FLASH_EEPROM_OK)
    {
        SMS_SERVICE_DEBUG("Données chargées (%d conversations)\n", 
                         sms_state.conversationCount);
    }
}

// ============================================================
// SECTION 10 : TRAITEMENT
// ============================================================

void sms_service_process(void)
{
    if (!sms_state.initialized) return;
    sms_protocol_process();
}

// ============================================================
// SECTION 11 : CALLBACKS
// ============================================================

void sms_service_set_received_callback(SMSService_ReceivedCallback cb) { received_cb = cb; }
void sms_service_set_sent_callback(SMSService_SentCallback cb) { sent_cb = cb; }
void sms_service_set_failed_callback(SMSService_FailedCallback cb) { failed_cb = cb; }
void sms_service_set_new_message_callback(SMSService_NewMessageCallback cb) { new_message_cb = cb; }

// ============================================================
// SECTION 12 : DÉBOGAGE
// ============================================================

void sms_service_print_state(void)
{
    printf("\n═══ ÉTAT SERVICE SMS ═══\n");
    printf("Conversations : %d\n", sms_state.conversationCount);
    printf("Brouillons    : %d\n", sms_state.draftCount);
    printf("Modèles       : %d\n", sms_state.templateCount);
    printf("Non lus       : %d\n", sms_state.totalUnread);
    printf("Envoyés       : %lu\n", (unsigned long)sms_state.totalSent);
    printf("Reçus         : %lu\n", (unsigned long)sms_state.totalReceived);
    printf("Échoués       : %lu\n", (unsigned long)sms_state.totalFailed);
    printf("══════════════════════\n\n");
}

void sms_service_print_conversations(void)
{
    printf("\n═══ CONVERSATIONS (%d) ═══\n", sms_state.conversationCount);
    
    for (uint16_t i = 0; i < sms_state.conversationCount; i++)
    {
        SMSConversation* conv = &sms_state.conversations[i];
        
        printf("%s %s %s (%d msg, %d non lus)\n",
               conv->pinned ? "📌" : "  ",
               conv->muted ? "🔇" : "  ",
               conv->contactName,
               conv->messageCount,
               conv->unreadCount);
        
        // Afficher le dernier message
        if (conv->messageCount > 0)
        {
            SMSMessage* last = &conv->messages[conv->messageCount - 1];
            printf("    %s : %s\n", last->isIncoming ? "←" : "→", last->message);
        }
    }
    printf("══════════════════════════\n\n");
}

void sms_service_print_conversation(const char* contactNumber)
{
    SMSConversation* conv = sms_service_get_conversation(contactNumber);
    
    if (conv == NULL)
    {
        printf("[SMS] Aucune conversation avec %s\n", contactNumber);
        return;
    }
    
    printf("\n═══ CONVERSATION : %s (%d msg) ═══\n", conv->contactName, conv->messageCount);
    
    for (uint16_t i = 0; i < conv->messageCount; i++)
    {
        SMSMessage* msg = &conv->messages[i];
        printf("[%s] %s : %s %s\n",
               msg->isIncoming ? "←" : "→",
               msg->isIncoming ? conv->contactName : "Moi",
               msg->message,
               msg->isRead ? "" : "●");
    }
    printf("══════════════════════════════════\n\n");
}

bool sms_service_self_test(void)
{
    SMS_SERVICE_DEBUG("Auto-test...\n");
    
    if (!sms_state.initialized)
    {
        SMS_SERVICE_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : ajouter un modèle
    sms_service_add_template("Test", "Message de test");
    if (sms_service_get_template_count() != 1)
    {
        SMS_SERVICE_DEBUG("Échec : modèle non ajouté\n");
        return false;
    }
    
    // Nettoyer
    sms_service_delete_template(0);
    
    SMS_SERVICE_DEBUG("Auto-test OK\n");
    return true;
}