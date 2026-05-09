/**
 * @file call_protocol.cpp
 * @brief Implémentation du protocole de gestion des appels
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans call_protocol.h.
 * 
 * Il gère :
 * - L'établissement des appels (call setup)
 * - La signalisation (ringing, accept, reject)
 * - Le maintien de l'appel (keep-alive)
 * - La libération (call end)
 * - L'historique des appels (CDR)
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "call_protocol.h"
#include "../drivers/lora/lora_driver.h"
#include "../drivers/audio/audio_manager.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du protocole d'appel */
static CallProtocolState call_state;

/** @brief Callbacks */
static CallProtocol_IncomingCallback incoming_cb = NULL;
static CallProtocol_RingingCallback ringing_cb = NULL;
static CallProtocol_ConnectedCallback connected_cb = NULL;
static CallProtocol_EndedCallback ended_cb = NULL;
static CallProtocol_RejectedCallback rejected_cb = NULL;
static CallProtocol_MissedCallback missed_cb = NULL;
static CallProtocol_StateCallback state_cb = NULL;

/** @brief Compteur d'ID d'appel */
static uint32_t call_id_counter = 0;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le protocole d'appel
 */
bool call_protocol_init(void)
{
    CALL_DEBUG("Initialisation du protocole d'appel...\n");
    
    memset(&call_state, 0, sizeof(CallProtocolState));
    call_state.currentState = CALL_STATE_IDLE;
    call_state.inCall = false;
    call_state.hasWaitingCall = false;
    
    call_state.initialized = true;
    
    CALL_DEBUG("Protocole d'appel initialisé\n");
    return true;
}

void call_protocol_deinit(void)
{
    call_state.initialized = false;
}

bool call_protocol_is_ready(void)
{
    return call_state.initialized;
}

// ============================================================
// SECTION 2 : CHANGEMENT D'ÉTAT
// ============================================================

/**
 * @brief Change l'état de l'appel et notifie le callback
 */
static void change_state(CallState newState)
{
    if (call_state.currentState == newState) return;
    
    CallState oldState = call_state.currentState;
    call_state.currentState = newState;
    
    CALL_DEBUG("État: %d → %d\n", oldState, newState);
    
    if (state_cb)
    {
        state_cb(oldState, newState);
    }
}

// ============================================================
// SECTION 3 : ÉTABLISSEMENT D'APPEL
// ============================================================

/**
 * @brief Initie un appel sortant
 */
bool call_protocol_make_call(const char* calleeNumber)
{
    if (!call_state.initialized) return false;
    if (call_state.currentState != CALL_STATE_IDLE) return false;
    if (calleeNumber == NULL) return false;
    
    CALL_DEBUG("Appel de %s...\n", calleeNumber);
    
    // Créer l'enregistrement d'appel
    CallRecord* call = &call_state.activeCall;
    memset(call, 0, sizeof(CallRecord));
    
    call->callId = ++call_id_counter;
    call->state = CALL_STATE_DIALING;
    call->isIncoming = false;
    call->startTime = HAL_GetTick();
    
    // Remplir les informations de l'appelant
    DeviceIdentity* identity = identity_get();
    strncpy(call->callerMsisdn, identity->msisdn, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(call->callerName, identity->deviceName, IDENTITY_DEVICE_NAME_MAX - 1);
    strncpy(call->calleeMsisdn, calleeNumber, IDENTITY_PHONE_NUMBER_MAX - 1);
    
    // Construire le message de demande d'appel
    CallMessage message;
    call_protocol_build_message(&message, CALL_MSG_REQUEST);
    message.callId = call->callId;
    strncpy(message.calleeMsisdn, calleeNumber, IDENTITY_PHONE_NUMBER_MAX - 1);
    
    // Passer en mode audio
    lora_driver_set_profile(PROFILE_AUDIO);
    
    // Envoyer la demande
    LoRaPacket packet;
    if (!lora_driver_send_call_request(&packet, calleeNumber))
    {
        CALL_DEBUG("Échec envoi demande d'appel\n");
        change_state(CALL_STATE_ERROR);
        return false;
    }
    
    change_state(CALL_STATE_DIALING);
    call_state.totalCallsMade++;
    
    return true;
}

/**
 * @brief Répond à un appel entrant
 */
bool call_protocol_answer_call(void)
{
    if (call_state.currentState != CALL_STATE_INCOMING) return false;
    
    CALL_DEBUG("Appel accepté\n");
    
    CallRecord* call = &call_state.activeCall;
    
    // Construire le message d'acceptation
    CallMessage message;
    call_protocol_build_message(&message, CALL_MSG_ACCEPT);
    message.callId = call->callId;
    
    // Envoyer l'acceptation
    LoRaPacket packet;
    lora_driver_send_call_accept(&packet, call->callerMsisdn);
    
    // Mettre à jour l'état
    call->connectTime = HAL_GetTick();
    call->state = CALL_STATE_CONNECTED;
    change_state(CALL_STATE_CONNECTED);
    call_state.inCall = true;
    
    // Démarrer l'audio
    audio_manager_start_call();
    
    if (connected_cb)
    {
        connected_cb(call->callerMsisdn, call->callerName);
    }
    
    return true;
}

/**
 * @brief Refuse un appel entrant
 */
bool call_protocol_reject_call(CallEndReason reason)
{
    if (call_state.currentState != CALL_STATE_INCOMING && 
        call_state.currentState != CALL_STATE_RINGING) return false;
    
    CALL_DEBUG("Appel refusé (raison=%d)\n", reason);
    
    CallRecord* call = &call_state.activeCall;
    
    // Envoyer le message de refus
    CallMessage message;
    call_protocol_build_message(&message, CALL_MSG_REJECT);
    message.callId = call->callId;
    message.endReason = reason;
    
    LoRaPacket packet;
    lora_driver_send_call_reject(&packet, call->callerMsisdn, reason);
    
    // Finaliser l'appel
    call->endTime = HAL_GetTick();
    call->duration = (call->endTime - call->startTime) / 1000;
    call->endReason = reason;
    call->state = CALL_STATE_REJECTED;
    
    // Ajouter à l'historique
    add_to_history(call);
    
    // Réinitialiser
    change_state(CALL_STATE_IDLE);
    call_state.inCall = false;
    memset(&call_state.activeCall, 0, sizeof(CallRecord));
    
    if (rejected_cb)
    {
        rejected_cb(call->calleeMsisdn, reason);
    }
    
    return true;
}

/**
 * @brief Termine un appel
 */
bool call_protocol_end_call(CallEndReason reason)
{
    if (!call_state.inCall && 
        call_state.currentState != CALL_STATE_DIALING &&
        call_state.currentState != CALL_STATE_RINGING &&
        call_state.currentState != CALL_STATE_INCOMING) return false;
    
    CALL_DEBUG("Fin d'appel (raison=%d)\n", reason);
    
    CallRecord* call = &call_state.activeCall;
    
    // Envoyer le message de fin
    CallMessage message;
    call_protocol_build_message(&message, CALL_MSG_END);
    message.callId = call->callId;
    message.endReason = reason;
    
    LoRaPacket packet;
    const char* remoteNumber = call->isIncoming ? call->callerMsisdn : call->calleeMsisdn;
    lora_driver_send_call_end(&packet, remoteNumber, reason);
    
    // Arrêter l'audio
    audio_manager_stop_call();
    
    // Finaliser l'appel
    call->endTime = HAL_GetTick();
    call->duration = (call->endTime - call->connectTime) / 1000;
    call->endReason = reason;
    call->state = CALL_STATE_ENDED;
    
    call_state.totalCallDuration += call->duration;
    
    // Ajouter à l'historique
    add_to_history(call);
    
    if (ended_cb)
    {
        ended_cb(remoteNumber, call->duration, reason);
    }
    
    // Réinitialiser
    change_state(CALL_STATE_IDLE);
    call_state.inCall = false;
    memset(&call_state.activeCall, 0, sizeof(CallRecord));
    
    // Repasser en mode équilibré
    lora_driver_set_profile(PROFILE_BALANCED);
    
    return true;
}

/**
 * @brief Met l'appel en attente
 */
bool call_protocol_hold_call(void)
{
    if (call_state.currentState != CALL_STATE_CONNECTED) return false;
    
    CALL_DEBUG("Appel mis en attente\n");
    
    CallMessage message;
    call_protocol_build_message(&message, CALL_MSG_HOLD);
    
    LoRaPacket packet;
    lora_driver_send_packet(&packet, false);
    
    change_state(CALL_STATE_HOLDING);
    
    return true;
}

/**
 * @brief Reprend l'appel
 */
bool call_protocol_resume_call(void)
{
    if (call_state.currentState != CALL_STATE_HOLDING) return false;
    
    CALL_DEBUG("Reprise de l'appel\n");
    
    CallMessage message;
    call_protocol_build_message(&message, CALL_MSG_RESUME);
    
    LoRaPacket packet;
    lora_driver_send_packet(&packet, false);
    
    change_state(CALL_STATE_CONNECTED);
    
    return true;
}

// ============================================================
// SECTION 4 : ÉTAT
// ============================================================

CallState call_protocol_get_state(void) { return call_state.currentState; }

bool call_protocol_is_in_call(void) { return call_state.inCall; }

const char* call_protocol_get_remote_number(void)
{
    if (!call_state.inCall) return NULL;
    return call_state.activeCall.isIncoming ? 
           call_state.activeCall.callerMsisdn : 
           call_state.activeCall.calleeMsisdn;
}

const char* call_protocol_get_remote_name(void)
{
    if (!call_state.inCall) return NULL;
    return call_state.activeCall.isIncoming ?
           call_state.activeCall.callerName :
           call_state.activeCall.calleeName;
}

uint32_t call_protocol_get_call_duration(void)
{
    if (!call_state.inCall) return 0;
    if (call_state.activeCall.connectTime == 0) return 0;
    return (HAL_GetTick() - call_state.activeCall.connectTime) / 1000;
}

CallRecord* call_protocol_get_active_call(void)
{
    return &call_state.activeCall;
}

// ============================================================
// SECTION 5 : TRAITEMENT
// ============================================================

/**
 * @brief Traitement périodique
 */
void call_protocol_process(void)
{
    if (!call_state.initialized) return;
    
    // Vérifier le timeout de sonnerie
    if (call_state.currentState == CALL_STATE_RINGING || 
        call_state.currentState == CALL_STATE_DIALING)
    {
        uint32_t elapsed = (HAL_GetTick() - call_state.activeCall.startTime) / 1000;
        
        if (elapsed > CALL_RING_TIMEOUT_S)
        {
            CALL_DEBUG("Timeout sonnerie, pas de réponse\n");
            call_protocol_end_call(CALL_END_NO_ANSWER);
        }
    }
    
    // Vérifier le keep-alive pendant un appel
    if (call_state.currentState == CALL_STATE_CONNECTED)
    {
        uint32_t elapsed = (HAL_GetTick() - call_state.lastKeepaliveTime) / 1000;
        
        if (elapsed >= CALL_KEEPALIVE_INTERVAL_S)
        {
            // Envoyer un keep-alive
            CallMessage message;
            call_protocol_build_message(&message, CALL_MSG_KEEPALIVE);
            
            LoRaPacket packet;
            lora_driver_send_packet(&packet, false);
            
            call_state.lastKeepaliveTime = HAL_GetTick();
        }
    }
    
    // Vérifier le timeout de connexion
    if (call_state.currentState == CALL_STATE_CONNECTED)
    {
        uint32_t maxDuration = CALL_MAX_DURATION_S * 1000;
        if (call_state.activeCall.connectTime > 0 &&
            (HAL_GetTick() - call_state.activeCall.connectTime) > maxDuration)
        {
            CALL_DEBUG("Durée maximale d'appel atteinte\n");
            call_protocol_end_call(CALL_END_TIMEOUT);
        }
    }
}

/**
 * @brief Traite un message d'appel reçu
 */
void call_protocol_process_message(const CallMessage* message, int16_t rssi)
{
    if (message == NULL) return;
    
    CALL_DEBUG("Message reçu: type=0x%02X, callId=%lu\n", 
              message->messageType, (unsigned long)message->callId);
    
    switch (message->messageType)
    {
        case CALL_MSG_REQUEST:
            handle_call_request(message, rssi);
            break;
            
        case CALL_MSG_RINGING:
            handle_call_ringing(message);
            break;
            
        case CALL_MSG_ACCEPT:
            handle_call_accept(message);
            break;
            
        case CALL_MSG_REJECT:
        case CALL_MSG_BUSY:
            handle_call_reject(message);
            break;
            
        case CALL_MSG_END:
            handle_call_end(message);
            break;
            
        case CALL_MSG_HOLD:
            change_state(CALL_STATE_HOLDING);
            break;
            
        case CALL_MSG_RESUME:
            change_state(CALL_STATE_CONNECTED);
            break;
            
        case CALL_MSG_KEEPALIVE:
            // Mettre à jour le RSSI
            call_state.activeCall.avgRssi = (call_state.activeCall.avgRssi + rssi) / 2;
            break;
            
        case CALL_MSG_MISSED:
            handle_missed_call(message);
            break;
            
        default:
            CALL_DEBUG("Type de message inconnu: 0x%02X\n", message->messageType);
            break;
    }
}

/**
 * @brief Gère une demande d'appel entrante
 */
static void handle_call_request(const CallMessage* message, int16_t rssi)
{
    CALL_DEBUG("Demande d'appel de %s (%s)\n", message->callerName, message->callerMsisdn);
    
    if (call_state.currentState != CALL_STATE_IDLE)
    {
        // Occupé - envoyer BUSY
        CALL_DEBUG("Occupé, envoi BUSY\n");
        
        CallMessage busyMsg;
        call_protocol_build_message(&busyMsg, CALL_MSG_BUSY);
        busyMsg.callId = message->callId;
        
        LoRaPacket packet;
        const char* caller = message->callerMsisdn;
        lora_driver_send_call_busy(&packet, caller);
        return;
    }
    
    // Créer l'enregistrement d'appel
    CallRecord* call = &call_state.activeCall;
    memset(call, 0, sizeof(CallRecord));
    
    call->callId = message->callId;
    call->state = CALL_STATE_INCOMING;
    call->isIncoming = true;
    call->startTime = HAL_GetTick();
    call->avgRssi = rssi;
    
    strncpy(call->callerMsisdn, message->callerMsisdn, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(call->callerName, message->callerName, IDENTITY_DEVICE_NAME_MAX - 1);
    
    DeviceIdentity* identity = identity_get();
    strncpy(call->calleeMsisdn, identity->msisdn, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(call->calleeName, identity->deviceName, IDENTITY_DEVICE_NAME_MAX - 1);
    
    change_state(CALL_STATE_INCOMING);
    call_state.totalCallsReceived++;
    
    // Envoyer l'indication de sonnerie
    CallMessage ringingMsg;
    call_protocol_build_message(&ringingMsg, CALL_MSG_RINGING);
    ringingMsg.callId = message->callId;
    
    LoRaPacket packet;
    lora_driver_send_call_ringing(&packet, message->callerMsisdn);
    
    // Notifier l'utilisateur
    if (incoming_cb)
    {
        incoming_cb(message->callerMsisdn, message->callerName);
    }
}

/**
 * @brief Gère l'indication de sonnerie
 */
static void handle_call_ringing(const CallMessage* message)
{
    CALL_DEBUG("Sonnerie en cours...\n");
    
    if (call_state.currentState == CALL_STATE_DIALING)
    {
        change_state(CALL_STATE_RINGING);
        
        if (ringing_cb)
        {
            ringing_cb();
        }
    }
}

/**
 * @brief Gère l'acceptation d'appel
 */
static void handle_call_accept(const CallMessage* message)
{
    CALL_DEBUG("Appel accepté par le correspondant\n");
    
    if (call_state.currentState == CALL_STATE_RINGING ||
        call_state.currentState == CALL_STATE_DIALING)
    {
        CallRecord* call = &call_state.activeCall;
        call->connectTime = HAL_GetTick();
        call->state = CALL_STATE_CONNECTED;
        
        change_state(CALL_STATE_CONNECTED);
        call_state.inCall = true;
        
        // Démarrer l'audio
        audio_manager_start_call();
        
        if (connected_cb)
        {
            connected_cb(call->calleeMsisdn, call->calleeName);
        }
    }
}

/**
 * @brief Gère le refus d'appel
 */
static void handle_call_reject(const CallMessage* message)
{
    CallEndReason reason = (CallEndReason)message->endReason;
    
    CALL_DEBUG("Appel refusé (raison=%d)\n", reason);
    
    if (call_state.currentState == CALL_STATE_RINGING ||
        call_state.currentState == CALL_STATE_DIALING)
    {
        CallRecord* call = &call_state.activeCall;
        call->endTime = HAL_GetTick();
        call->duration = 0;
        call->endReason = reason;
        call->state = CALL_STATE_REJECTED;
        
        add_to_history(call);
        
        change_state(CALL_STATE_IDLE);
        call_state.inCall = false;
        
        if (rejected_cb)
        {
            rejected_cb(call->calleeMsisdn, reason);
        }
        
        memset(&call_state.activeCall, 0, sizeof(CallRecord));
    }
}

/**
 * @brief Gère la fin d'appel
 */
static void handle_call_end(const CallMessage* message)
{
    CallEndReason reason = (CallEndReason)message->endReason;
    
    CALL_DEBUG("Fin d'appel reçue (raison=%d)\n", reason);
    
    if (call_state.currentState == CALL_STATE_CONNECTED ||
        call_state.currentState == CALL_STATE_HOLDING ||
        call_state.currentState == CALL_STATE_RINGING ||
        call_state.currentState == CALL_STATE_INCOMING)
    {
        // Arrêter l'audio
        audio_manager_stop_call();
        
        CallRecord* call = &call_state.activeCall;
        call->endTime = HAL_GetTick();
        if (call->connectTime > 0)
        {
            call->duration = (call->endTime - call->connectTime) / 1000;
        }
        call->endReason = reason;
        call->state = CALL_STATE_ENDED;
        
        call_state.totalCallDuration += call->duration;
        
        add_to_history(call);
        
        const char* remote = call->isIncoming ? call->callerMsisdn : call->calleeMsisdn;
        
        if (ended_cb)
        {
            ended_cb(remote, call->duration, reason);
        }
        
        change_state(CALL_STATE_IDLE);
        call_state.inCall = false;
        memset(&call_state.activeCall, 0, sizeof(CallRecord));
        
        // Repasser en mode équilibré
        lora_driver_set_profile(PROFILE_BALANCED);
    }
}

/**
 * @brief Gère un appel manqué
 */
static void handle_missed_call(const CallMessage* message)
{
    CALL_DEBUG("Notification d'appel manqué de %s\n", message->callerName);
    
    call_state.totalCallsMissed++;
    
    if (missed_cb)
    {
        missed_cb(message->callerMsisdn);
    }
}

/**
 * @brief Construit un message d'appel
 */
void call_protocol_build_message(CallMessage* message, CallMessageType type)
{
    if (message == NULL) return;
    
    memset(message, 0, sizeof(CallMessage));
    
    message->messageType = type;
    message->protocolVersion = CALL_PROTOCOL_VERSION_NUM;
    message->timestamp = HAL_GetTick();
    
    DeviceIdentity* identity = identity_get();
    strncpy(message->callerMsisdn, identity->msisdn, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(message->callerName, identity->deviceName, IDENTITY_DEVICE_NAME_MAX - 1);
}

// ============================================================
// SECTION 6 : HISTORIQUE
// ============================================================

/**
 * @brief Ajoute un appel à l'historique
 */
static void add_to_history(CallRecord* call)
{
    if (call == NULL) return;
    
    // Décaler l'historique si plein
    if (call_state.callHistoryCount >= CALL_HISTORY_MAX)
    {
        memmove(&call_state.callHistory[0], 
                &call_state.callHistory[1],
                (CALL_HISTORY_MAX - 1) * sizeof(CallRecord));
        call_state.callHistoryCount = CALL_HISTORY_MAX - 1;
    }
    
    // Ajouter à la fin
    memcpy(&call_state.callHistory[call_state.callHistoryCount], 
           call, sizeof(CallRecord));
    call_state.callHistoryCount++;
}

uint8_t call_protocol_get_history(CallRecord* records, uint8_t maxCount)
{
    if (records == NULL) return 0;
    
    uint8_t count = (call_state.callHistoryCount < maxCount) ? 
                     call_state.callHistoryCount : maxCount;
    
    memcpy(records, call_state.callHistory, count * sizeof(CallRecord));
    return count;
}

uint8_t call_protocol_get_missed_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < call_state.callHistoryCount; i++)
    {
        if (call_state.callHistory[i].isMissed) count++;
    }
    return count;
}

void call_protocol_clear_history(void)
{
    call_state.callHistoryCount = 0;
    memset(call_state.callHistory, 0, sizeof(call_state.callHistory));
}

// ============================================================
// SECTION 7 : CALLBACKS
// ============================================================

void call_protocol_set_incoming_callback(CallProtocol_IncomingCallback cb) { incoming_cb = cb; }
void call_protocol_set_ringing_callback(CallProtocol_RingingCallback cb) { ringing_cb = cb; }
void call_protocol_set_connected_callback(CallProtocol_ConnectedCallback cb) { connected_cb = cb; }
void call_protocol_set_ended_callback(CallProtocol_EndedCallback cb) { ended_cb = cb; }
void call_protocol_set_rejected_callback(CallProtocol_RejectedCallback cb) { rejected_cb = cb; }
void call_protocol_set_missed_callback(CallProtocol_MissedCallback cb) { missed_cb = cb; }
void call_protocol_set_state_callback(CallProtocol_StateCallback cb) { state_cb = cb; }

// ============================================================
// SECTION 8 : DÉBOGAGE
// ============================================================

void call_protocol_print_state(void)
{
    const char* stateStr = "INCONNU";
    switch (call_state.currentState)
    {
        case CALL_STATE_IDLE:        stateStr = "IDLE"; break;
        case CALL_STATE_DIALING:     stateStr = "DIALING"; break;
        case CALL_STATE_RINGING:     stateStr = "RINGING"; break;
        case CALL_STATE_INCOMING:    stateStr = "INCOMING"; break;
        case CALL_STATE_CONNECTED:   stateStr = "CONNECTED"; break;
        case CALL_STATE_HOLDING:     stateStr = "HOLDING"; break;
        case CALL_STATE_ENDED:       stateStr = "ENDED"; break;
        default: break;
    }
    
    printf("\n═══ ÉTAT PROTOCOLE APPEL ═══\n");
    printf("État          : %s\n", stateStr);
    printf("En appel      : %s\n", call_state.inCall ? "Oui" : "Non");
    printf("Double appel  : %s\n", call_state.hasWaitingCall ? "Oui" : "Non");
    printf("Appels émis   : %lu\n", (unsigned long)call_state.totalCallsMade);
    printf("Appels reçus  : %lu\n", (unsigned long)call_state.totalCallsReceived);
    printf("Appels manqués: %lu\n", (unsigned long)call_state.totalCallsMissed);
    printf("Durée totale  : %lu s\n", (unsigned long)call_state.totalCallDuration);
    printf("══════════════════════════\n\n");
}

void call_protocol_print_active_call(void)
{
    CallRecord* call = &call_state.activeCall;
    
    if (call->callId == 0)
    {
        printf("[CALL] Aucun appel actif\n");
        return;
    }
    
    printf("\n═══ APPEL ACTIF ═══\n");
    printf("ID           : %lu\n", (unsigned long)call->callId);
    printf("Sens         : %s\n", call->isIncoming ? "Entrant" : "Sortant");
    printf("Correspondant: %s (%s)\n", 
           call->isIncoming ? call->callerName : call->calleeName,
           call->isIncoming ? call->callerMsisdn : call->calleeMsisdn);
    printf("Durée        : %lu s\n", (unsigned long)call_protocol_get_call_duration());
    printf("État         : %d\n", call->state);
    printf("══════════════════\n\n");
}

void call_protocol_print_history(void)
{
    printf("\n═══ HISTORIQUE APPELS (%d) ═══\n", call_state.callHistoryCount);
    printf("%-4s %-16s %-12s %-8s %-10s\n", "Sens", "Correspondant", "Date", "Durée", "Raison");
    printf("──────────────────────────────────────────────────────\n");
    
    for (uint8_t i = 0; i < call_state.callHistoryCount; i++)
    {
        CallRecord* call = &call_state.callHistory[i];
        
        const char* sens = call->isIncoming ? "⬅" : "➡";
        const char* name = call->isIncoming ? call->callerName : call->calleeName;
        const char* reasonStr = "?";
        
        switch (call->endReason)
        {
            case CALL_END_NORMAL:       reasonStr = "Normal"; break;
            case CALL_END_USER_REJECT:  reasonStr = "Refusé"; break;
            case CALL_END_BUSY:         reasonStr = "Occupé"; break;
            case CALL_END_NO_ANSWER:    reasonStr = "Pas rép."; break;
            case CALL_END_TIMEOUT:      reasonStr = "Timeout"; break;
            default: break;
        }
        
        printf("%-4s %-16s %-8lu %-6lu %-10s\n",
               sens, name, 
               (unsigned long)(call->startTime / 1000),
               (unsigned long)call->duration,
               reasonStr);
    }
    printf("══════════════════════════════\n\n");
}

bool call_protocol_self_test(void)
{
    CALL_DEBUG("Auto-test...\n");
    
    if (!call_state.initialized)
    {
        CALL_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : construire un message
    CallMessage msg;
    call_protocol_build_message(&msg, CALL_MSG_REQUEST);
    
    if (msg.messageType != CALL_MSG_REQUEST)
    {
        CALL_DEBUG("Échec : type message incorrect\n");
        return false;
    }
    
    CALL_DEBUG("Auto-test OK\n");
    return true;
}