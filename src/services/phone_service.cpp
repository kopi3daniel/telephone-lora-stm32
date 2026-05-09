/**
 * @file phone_service.cpp
 * @brief Implémentation du service de téléphonie
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans phone_service.h.
 * 
 * Il orchestre :
 * - Le protocole d'appel (call_protocol)
 * - L'audio (audio_manager)
 * - Les contacts et favoris
 * - Le journal d'appels
 * - Les numéros bloqués
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "phone_service.h"
#include "../protocols/call_protocol.h"
#include "../protocols/identity.h"
#include "../drivers/audio/audio_manager.h"
#include "../drivers/storage/flash_eeprom.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du service */
static PhoneServiceState phone_state;

/** @brief Callbacks */
static PhoneService_StateCallback state_cb = NULL;
static PhoneService_IncomingCallback incoming_cb = NULL;
static PhoneService_ConnectedCallback connected_cb = NULL;
static PhoneService_EndedCallback ended_cb = NULL;
static PhoneService_MissedCallback missed_cb = NULL;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le service de téléphonie
 */
bool phone_service_init(void)
{
    PHONE_DEBUG("Initialisation du service de téléphonie...\n");
    
    memset(&phone_state, 0, sizeof(PhoneServiceState));
    phone_state.state = PHONE_STATE_IDLE;
    
    // Initialiser les sous-systèmes
    if (!call_protocol_init())
    {
        PHONE_DEBUG("Échec initialisation protocole d'appel\n");
        return false;
    }
    
    if (!audio_manager_init(NULL))
    {
        PHONE_DEBUG("Échec initialisation audio\n");
        return false;
    }
    
    // Enregistrer les callbacks du protocole d'appel
    call_protocol_set_incoming_callback(on_incoming_call);
    call_protocol_set_ringing_callback(on_ringing);
    call_protocol_set_connected_callback(on_call_connected);
    call_protocol_set_ended_callback(on_call_ended);
    call_protocol_set_rejected_callback(on_call_rejected);
    call_protocol_set_missed_callback(on_missed_call);
    
    // Charger les contacts et le journal
    phone_service_load_data();
    
    phone_state.initialized = true;
    
    PHONE_DEBUG("Service initialisé\n");
    return true;
}

void phone_service_deinit(void)
{
    phone_service_save_data();
    call_protocol_deinit();
    audio_manager_deinit();
    phone_state.initialized = false;
}

bool phone_service_is_ready(void)
{
    return phone_state.initialized;
}

// ============================================================
// SECTION 2 : APPELS
// ============================================================

bool phone_service_dial(const char* number)
{
    if (!phone_state.initialized) return false;
    if (number == NULL) return false;
    if (phone_state.state != PHONE_STATE_IDLE) return false;
    
    // Vérifier si le numéro est bloqué
    if (phone_service_is_blocked(number))
    {
        PHONE_DEBUG("Numéro bloqué : %s\n", number);
        return false;
    }
    
    PHONE_DEBUG("Appel de %s...\n", number);
    
    // Chercher le contact
    PhoneContact* contact = phone_service_find_contact(number);
    const char* contactName = contact ? contact->name : number;
    
    // Mettre à jour l'état
    strncpy(phone_state.currentNumber, number, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(phone_state.currentName, contactName, 31);
    phone_state.state = PHONE_STATE_DIALING;
    phone_state.isIncoming = false;
    phone_state.callStartTime = HAL_GetTick();
    
    // Lancer l'appel via le protocole
    if (call_protocol_make_call(number))
    {
        phone_state.totalCallsMade++;
        
        if (contact)
        {
            contact->lastContact = HAL_GetTick();
            contact->callCount++;
        }
        
        if (state_cb) state_cb(PHONE_STATE_IDLE, PHONE_STATE_DIALING);
        return true;
    }
    
    phone_state.state = PHONE_STATE_IDLE;
    return false;
}

bool phone_service_answer(void)
{
    if (phone_state.state != PHONE_STATE_INCOMING) return false;
    
    PHONE_DEBUG("Appel accepté\n");
    
    phone_state.state = PHONE_STATE_CONNECTED;
    
    if (call_protocol_answer_call())
    {
        if (state_cb) state_cb(PHONE_STATE_INCOMING, PHONE_STATE_CONNECTED);
        return true;
    }
    
    return false;
}

bool phone_service_reject(void)
{
    if (phone_state.state != PHONE_STATE_INCOMING) return false;
    
    PHONE_DEBUG("Appel refusé\n");
    
    call_protocol_reject_call(CALL_END_USER_REJECT);
    phone_state.state = PHONE_STATE_IDLE;
    
    if (state_cb) state_cb(PHONE_STATE_INCOMING, PHONE_STATE_IDLE);
    
    return true;
}

bool phone_service_hangup(void)
{
    if (phone_state.state == PHONE_STATE_IDLE) return false;
    
    PHONE_DEBUG("Raccroché\n");
    
    call_protocol_end_call(CALL_END_NORMAL);
    audio_manager_stop_call();
    
    phone_state.callDuration = (HAL_GetTick() - phone_state.callStartTime) / 1000;
    phone_state.totalCallDuration += phone_state.callDuration;
    
    PhoneState oldState = phone_state.state;
    phone_state.state = PHONE_STATE_IDLE;
    
    if (state_cb) state_cb(oldState, PHONE_STATE_IDLE);
    
    return true;
}

bool phone_service_redial(void)
{
    if (strlen(phone_state.currentNumber) == 0) return false;
    return phone_service_dial(phone_state.currentNumber);
}

// ============================================================
// SECTION 3 : CONTRÔLE D'APPEL
// ============================================================

void phone_service_mute(bool mute)
{
    phone_state.isMuted = mute;
    audio_manager_set_mute(mute);
}

void phone_service_toggle_mute(void)
{
    phone_service_mute(!phone_state.isMuted);
}

bool phone_service_is_muted(void)
{
    return phone_state.isMuted;
}

void phone_service_speaker(bool on)
{
    phone_state.isSpeakerOn = on;
    if (on)
    {
        audio_manager_set_volume(100);  // Volume max pour haut-parleur
    }
    else
    {
        audio_manager_set_volume(80);   // Volume normal
    }
}

void phone_service_toggle_speaker(void)
{
    phone_service_speaker(!phone_state.isSpeakerOn);
}

bool phone_service_is_speaker_on(void)
{
    return phone_state.isSpeakerOn;
}

// ============================================================
// SECTION 4 : ÉTAT
// ============================================================

PhoneState phone_service_get_state(void)
{
    return phone_state.state;
}

const char* phone_service_get_current_number(void)
{
    return phone_state.currentNumber;
}

const char* phone_service_get_current_name(void)
{
    return phone_state.currentName;
}

uint32_t phone_service_get_call_duration(void)
{
    if (phone_state.state == PHONE_STATE_CONNECTED)
    {
        return (HAL_GetTick() - phone_state.callStartTime) / 1000;
    }
    return phone_state.callDuration;
}

bool phone_service_is_in_call(void)
{
    return (phone_state.state == PHONE_STATE_CONNECTED || 
            phone_state.state == PHONE_STATE_DIALING ||
            phone_state.state == PHONE_STATE_RINGING);
}

// ============================================================
// SECTION 5 : CONTACTS
// ============================================================

bool phone_service_add_contact(const char* name, const char* number)
{
    if (name == NULL || number == NULL) return false;
    if (phone_state.contactCount >= PHONE_MAX_CONTACTS) return false;
    
    // Vérifier si le contact existe déjà
    if (phone_service_find_contact(number) != NULL)
    {
        PHONE_DEBUG("Contact déjà existant : %s\n", number);
        return false;
    }
    
    PhoneContact* contact = &phone_state.contacts[phone_state.contactCount++];
    memset(contact, 0, sizeof(PhoneContact));
    strncpy(contact->name, name, 31);
    strncpy(contact->number, number, IDENTITY_PHONE_NUMBER_MAX - 1);
    
    PHONE_DEBUG("Contact ajouté : %s (%s)\n", name, number);
    return true;
}

bool phone_service_update_contact(uint16_t index, const char* name, const char* number)
{
    if (index >= phone_state.contactCount) return false;
    
    PhoneContact* contact = &phone_state.contacts[index];
    
    if (name != NULL) strncpy(contact->name, name, 31);
    if (number != NULL) strncpy(contact->number, number, IDENTITY_PHONE_NUMBER_MAX - 1);
    
    return true;
}

bool phone_service_delete_contact(uint16_t index)
{
    if (index >= phone_state.contactCount) return false;
    
    // Supprimer des favoris si nécessaire
    if (phone_service_is_favorite(index))
    {
        phone_service_remove_favorite(index);
    }
    
    // Décaler les contacts suivants
    if (index < phone_state.contactCount - 1)
    {
        memmove(&phone_state.contacts[index], 
                &phone_state.contacts[index + 1],
                (phone_state.contactCount - index - 1) * sizeof(PhoneContact));
    }
    phone_state.contactCount--;
    
    return true;
}

PhoneContact* phone_service_find_contact(const char* number)
{
    if (number == NULL) return NULL;
    
    for (uint16_t i = 0; i < phone_state.contactCount; i++)
    {
        if (strcmp(phone_state.contacts[i].number, number) == 0)
        {
            return &phone_state.contacts[i];
        }
    }
    return NULL;
}

PhoneContact* phone_service_find_contact_by_name(const char* name)
{
    if (name == NULL) return NULL;
    
    for (uint16_t i = 0; i < phone_state.contactCount; i++)
    {
        if (strcasecmp(phone_state.contacts[i].name, name) == 0)
        {
            return &phone_state.contacts[i];
        }
    }
    return NULL;
}

uint16_t phone_service_get_contact_count(void)
{
    return phone_state.contactCount;
}

void phone_service_sort_contacts(void)
{
    // Tri par nom (bulle simple)
    for (uint16_t i = 0; i < phone_state.contactCount - 1; i++)
    {
        for (uint16_t j = 0; j < phone_state.contactCount - i - 1; j++)
        {
            if (strcmp(phone_state.contacts[j].name, phone_state.contacts[j + 1].name) > 0)
            {
                PhoneContact temp = phone_state.contacts[j];
                phone_state.contacts[j] = phone_state.contacts[j + 1];
                phone_state.contacts[j + 1] = temp;
            }
        }
    }
}

// ============================================================
// SECTION 6 : FAVORIS
// ============================================================

bool phone_service_add_favorite(uint16_t contactIndex)
{
    if (contactIndex >= phone_state.contactCount) return false;
    if (phone_state.favoriteCount >= PHONE_MAX_FAVORITES) return false;
    
    // Vérifier si déjà favori
    if (phone_service_is_favorite(contactIndex)) return true;
    
    phone_state.contacts[contactIndex].favorite = true;
    phone_state.favorites[phone_state.favoriteCount++] = contactIndex;
    
    return true;
}

bool phone_service_remove_favorite(uint16_t contactIndex)
{
    // Chercher dans la liste des favoris
    for (uint8_t i = 0; i < phone_state.favoriteCount; i++)
    {
        if (phone_state.favorites[i] == contactIndex)
        {
            phone_state.contacts[contactIndex].favorite = false;
            
            // Décaler
            if (i < phone_state.favoriteCount - 1)
            {
                memmove(&phone_state.favorites[i], &phone_state.favorites[i + 1],
                        (phone_state.favoriteCount - i - 1) * sizeof(uint8_t));
            }
            phone_state.favoriteCount--;
            return true;
        }
    }
    return false;
}

bool phone_service_is_favorite(uint16_t contactIndex)
{
    return phone_state.contacts[contactIndex].favorite;
}

uint8_t phone_service_get_favorite_count(void)
{
    return phone_state.favoriteCount;
}

void phone_service_get_favorites(PhoneContact* favorites, uint8_t maxCount)
{
    if (favorites == NULL) return;
    
    uint8_t count = (phone_state.favoriteCount < maxCount) ? 
                     phone_state.favoriteCount : maxCount;
    
    for (uint8_t i = 0; i < count; i++)
    {
        memcpy(&favorites[i], &phone_state.contacts[phone_state.favorites[i]], sizeof(PhoneContact));
    }
}

// ============================================================
// SECTION 7 : JOURNAL
// ============================================================

void phone_service_add_to_call_log(CallRecord* call)
{
    if (call == NULL) return;
    
    // Décaler si plein
    if (phone_state.callLogCount >= PHONE_MAX_CALL_LOG)
    {
        memmove(&phone_state.callLog[0], &phone_state.callLog[1],
                (PHONE_MAX_CALL_LOG - 1) * sizeof(CallRecord));
        phone_state.callLogCount = PHONE_MAX_CALL_LOG - 1;
    }
    
    memcpy(&phone_state.callLog[phone_state.callLogCount++], call, sizeof(CallRecord));
}

uint16_t phone_service_get_call_log(CallRecord* records, uint16_t maxCount)
{
    if (records == NULL) return 0;
    
    uint16_t count = (phone_state.callLogCount < maxCount) ? 
                      phone_state.callLogCount : maxCount;
    
    memcpy(records, phone_state.callLog, count * sizeof(CallRecord));
    return count;
}

uint16_t phone_service_get_missed_count(void)
{
    uint16_t count = 0;
    for (uint16_t i = 0; i < phone_state.callLogCount; i++)
    {
        if (phone_state.callLog[i].isMissed) count++;
    }
    return count;
}

void phone_service_clear_call_log(void)
{
    phone_state.callLogCount = 0;
    memset(phone_state.callLog, 0, sizeof(phone_state.callLog));
}

// ============================================================
// SECTION 8 : BLOCAGE
// ============================================================

bool phone_service_block_number(const char* number)
{
    if (number == NULL) return false;
    if (phone_state.blockedCount >= PHONE_MAX_BLOCKED) return false;
    
    strncpy(phone_state.blockedNumbers[phone_state.blockedCount++], 
            number, IDENTITY_PHONE_NUMBER_MAX - 1);
    
    return true;
}

bool phone_service_unblock_number(const char* number)
{
    for (uint8_t i = 0; i < phone_state.blockedCount; i++)
    {
        if (strcmp(phone_state.blockedNumbers[i], number) == 0)
        {
            if (i < phone_state.blockedCount - 1)
            {
                memmove(phone_state.blockedNumbers[i], phone_state.blockedNumbers[i + 1],
                        (phone_state.blockedCount - i - 1) * IDENTITY_PHONE_NUMBER_MAX);
            }
            phone_state.blockedCount--;
            return true;
        }
    }
    return false;
}

bool phone_service_is_blocked(const char* number)
{
    if (number == NULL) return false;
    
    for (uint8_t i = 0; i < phone_state.blockedCount; i++)
    {
        if (strcmp(phone_state.blockedNumbers[i], number) == 0) return true;
    }
    return false;
}

uint8_t phone_service_get_blocked_count(void)
{
    return phone_state.blockedCount;
}

// ============================================================
// SECTION 9 : CALLBACKS INTERNES
// ============================================================

static void on_incoming_call(const char* callerNumber, const char* callerName)
{
    // Vérifier si le numéro est bloqué
    if (phone_service_is_blocked(callerNumber))
    {
        PHONE_DEBUG("Appel bloqué de %s\n", callerNumber);
        call_protocol_reject_call(CALL_END_USER_REJECT);
        return;
    }
    
    phone_state.state = PHONE_STATE_INCOMING;
    phone_state.isIncoming = true;
    phone_state.callStartTime = HAL_GetTick();
    strncpy(phone_state.currentNumber, callerNumber, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(phone_state.currentName, callerName ? callerName : callerNumber, 31);
    
    phone_state.totalCallsReceived++;
    
    // Chercher le contact
    PhoneContact* contact = phone_service_find_contact(callerNumber);
    if (contact)
    {
        strncpy(phone_state.currentName, contact->name, 31);
    }
    
    // Jouer la sonnerie
    audio_manager_play_ringtone(contact ? contact->ringtoneIndex : 0);
    
    if (incoming_cb) incoming_cb(callerNumber, phone_state.currentName);
    if (state_cb) state_cb(PHONE_STATE_IDLE, PHONE_STATE_INCOMING);
}

static void on_ringing(void)
{
    phone_state.state = PHONE_STATE_RINGING;
    if (state_cb) state_cb(PHONE_STATE_DIALING, PHONE_STATE_RINGING);
}

static void on_call_connected(const char* number, const char* name)
{
    phone_state.state = PHONE_STATE_CONNECTED;
    audio_manager_stop_ringtone();
    audio_manager_start_call();
    
    if (connected_cb) connected_cb(number);
    if (state_cb) state_cb(PHONE_STATE_RINGING, PHONE_STATE_CONNECTED);
}

static void on_call_ended(const char* number, uint32_t duration, CallEndReason reason)
{
    phone_state.callDuration = duration;
    phone_state.totalCallDuration += duration;
    
    audio_manager_stop_call();
    
    // Ajouter au journal
    CallRecord record;
    memset(&record, 0, sizeof(CallRecord));
    record.duration = duration;
    record.isIncoming = phone_state.isIncoming;
    record.endReason = reason;
    strncpy(record.callerMsisdn, phone_state.isIncoming ? number : identity_get()->msisdn, 
            IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(record.calleeMsisdn, phone_state.isIncoming ? identity_get()->msisdn : number,
            IDENTITY_PHONE_NUMBER_MAX - 1);
    
    phone_service_add_to_call_log(&record);
    
    PhoneState oldState = phone_state.state;
    phone_state.state = PHONE_STATE_IDLE;
    
    if (ended_cb) ended_cb(number, duration);
    if (state_cb) state_cb(oldState, PHONE_STATE_IDLE);
}

static void on_call_rejected(const char* number, CallEndReason reason)
{
    audio_manager_stop_ringtone();
    phone_state.state = PHONE_STATE_IDLE;
    
    if (state_cb) state_cb(PHONE_STATE_INCOMING, PHONE_STATE_IDLE);
}

static void on_missed_call(const char* callerNumber)
{
    phone_state.totalCallsMissed++;
    
    if (missed_cb) missed_cb(callerNumber);
}

// ============================================================
// SECTION 10 : PERSISTANCE
// ============================================================

static void phone_service_save_data(void)
{
    // Sauvegarder les contacts en Flash
    flash_eeprom_write(EEPROM_ID_CONTACTS, (uint8_t*)phone_state.contacts,
                       phone_state.contactCount * sizeof(PhoneContact));
}

static void phone_service_load_data(void)
{
    uint16_t readSize;
    FlashEEPROM_Error err = flash_eeprom_read(EEPROM_ID_CONTACTS,
                                               (uint8_t*)phone_state.contacts,
                                               sizeof(phone_state.contacts),
                                               &readSize);
    
    if (err == FLASH_EEPROM_OK)
    {
        phone_state.contactCount = readSize / sizeof(PhoneContact);
        PHONE_DEBUG("%d contacts chargés\n", phone_state.contactCount);
    }
}

// ============================================================
// SECTION 11 : TRAITEMENT
// ============================================================

void phone_service_process(void)
{
    if (!phone_state.initialized) return;
    
    call_protocol_process();
}

// ============================================================
// SECTION 12 : CALLBACKS
// ============================================================

void phone_service_set_state_callback(PhoneService_StateCallback cb) { state_cb = cb; }
void phone_service_set_incoming_callback(PhoneService_IncomingCallback cb) { incoming_cb = cb; }
void phone_service_set_connected_callback(PhoneService_ConnectedCallback cb) { connected_cb = cb; }
void phone_service_set_ended_callback(PhoneService_EndedCallback cb) { ended_cb = cb; }
void phone_service_set_missed_callback(PhoneService_MissedCallback cb) { missed_cb = cb; }

// ============================================================
// SECTION 13 : DÉBOGAGE
// ============================================================

void phone_service_print_state(void)
{
    const char* stateStr = "INCONNU";
    switch (phone_state.state)
    {
        case PHONE_STATE_IDLE:      stateStr = "IDLE"; break;
        case PHONE_STATE_DIALING:   stateStr = "DIALING"; break;
        case PHONE_STATE_RINGING:   stateStr = "RINGING"; break;
        case PHONE_STATE_INCOMING:  stateStr = "INCOMING"; break;
        case PHONE_STATE_CONNECTED: stateStr = "CONNECTED"; break;
        default: break;
    }
    
    printf("\n═══ ÉTAT SERVICE TÉLÉPHONIE ═══\n");
    printf("État          : %s\n", stateStr);
    printf("Numéro        : %s\n", phone_state.currentNumber);
    printf("Nom           : %s\n", phone_state.currentName);
    printf("Durée         : %lu s\n", (unsigned long)phone_service_get_call_duration());
    printf("Muet          : %s\n", phone_state.isMuted ? "Oui" : "Non");
    printf("Haut-parleur  : %s\n", phone_state.isSpeakerOn ? "Oui" : "Non");
    printf("Contacts      : %d\n", phone_state.contactCount);
    printf("Favoris       : %d\n", phone_state.favoriteCount);
    printf("Appels émis   : %lu\n", (unsigned long)phone_state.totalCallsMade);
    printf("Appels reçus  : %lu\n", (unsigned long)phone_state.totalCallsReceived);
    printf("Appels manqués: %lu\n", (unsigned long)phone_state.totalCallsMissed);
    printf("══════════════════════════\n\n");
}

void phone_service_print_contacts(void)
{
    printf("\n═══ CONTACTS (%d) ═══\n", phone_state.contactCount);
    printf("%-4s %-20s %-16s %-6s\n", "Fav", "Nom", "Numéro", "Appels");
    printf("──────────────────────────────────────────────\n");
    
    for (uint16_t i = 0; i < phone_state.contactCount; i++)
    {
        PhoneContact* c = &phone_state.contacts[i];
        printf("%-4s %-20s %-16s %-6lu\n",
               c->favorite ? "⭐" : "  ",
               c->name, c->number, (unsigned long)c->callCount);
    }
    printf("══════════════════════════════\n\n");
}

void phone_service_print_call_log(void)
{
    printf("\n═══ JOURNAL APPELS (%d) ═══\n", phone_state.callLogCount);
    
    for (uint16_t i = 0; i < phone_state.callLogCount; i++)
    {
        CallRecord* c = &phone_state.callLog[i];
        const char* direction = c->isIncoming ? "←" : "→";
        const char* number = c->isIncoming ? c->callerMsisdn : c->calleeMsisdn;
        
        printf("[%s] %s : %lu s\n", direction, number, (unsigned long)c->duration);
    }
    printf("══════════════════════════════\n\n");
}

bool phone_service_self_test(void)
{
    PHONE_DEBUG("Auto-test...\n");
    
    if (!phone_state.initialized)
    {
        PHONE_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : ajouter un contact
    phone_service_add_contact("Test", "0600000000");
    PhoneContact* contact = phone_service_find_contact("0600000000");
    
    if (contact == NULL || strcmp(contact->name, "Test") != 0)
    {
        PHONE_DEBUG("Échec : contact non trouvé\n");
        return false;
    }
    
    // Nettoyer
    phone_service_delete_contact(phone_state.contactCount - 1);
    
    PHONE_DEBUG("Auto-test OK\n");
    return true;
}