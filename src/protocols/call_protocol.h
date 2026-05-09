/**
 * @file call_protocol.h
 * @brief Protocole de gestion des appels téléphoniques LoRa
 * 
 * Ce fichier implémente le protocole de signalisation des appels :
 * - Établissement d'appel (call setup)
 * - Acceptation/refus d'appel
 * - Maintien de l'appel (keep-alive)
 * - Libération d'appel (call release)
 * - Appels d'urgence
 * - Double appel et mise en attente
 * 
 * Diagramme de séquence d'un appel :
 * 
 *  Appelant                     Appelé
 *     │                            │
 *     │── CALL_REQUEST ───────────►│
 *     │                            │ (sonnerie)
 *     │◄── CALL_RINGING ──────────│
 *     │                            │
 *     │◄── CALL_ACCEPT ───────────│ (décroche)
 *     │                            │
 *     │═══ AUDIO_DATA ═══════════►│ (flux audio bidirectionnel)
 *     │◄══ AUDIO_DATA ════════════│
 *     │                            │
 *     │── CALL_END ──────────────►│ (raccroche)
 *     │                            │
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef CALL_PROTOCOL_H
#define CALL_PROTOCOL_H

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
#define CALL_PROTOCOL_VERSION           "1.0.0"

/** @brief Version du protocole d'appel */
#define CALL_PROTOCOL_VERSION_NUM       1

/** @brief Durée maximale d'un appel (secondes) - 0 = illimité */
#define CALL_MAX_DURATION_S             7200    // 2 heures

/** @brief Timeout de sonnerie (secondes) */
#define CALL_RING_TIMEOUT_S             30

/** @brief Timeout de réponse (secondes) */
#define CALL_ANSWER_TIMEOUT_S           60

/** @brief Intervalle keep-alive (secondes) */
#define CALL_KEEPALIVE_INTERVAL_S       10

/** @brief Nombre de tentatives d'appel */
#define CALL_MAX_RETRIES                3

/** @brief Délai entre les tentatives (secondes) */
#define CALL_RETRY_DELAY_S              5

// ============================================================
// SECTION 2 : ÉTATS D'UN APPEL
// ============================================================

/**
 * @brief États possibles d'un appel
 */
typedef enum {
    CALL_STATE_IDLE             = 0,    // Aucun appel
    CALL_STATE_DIALING          = 1,    // Numérotation en cours
    CALL_STATE_RINGING          = 2,    // Sonnerie (côté appelant)
    CALL_STATE_INCOMING         = 3,    // Appel entrant (côté appelé)
    CALL_STATE_CONNECTING       = 4,    // Connexion en cours
    CALL_STATE_CONNECTED        = 5,    // Communication établie
    CALL_STATE_HOLDING          = 6,    // Appel en attente
    CALL_STATE_DISCONNECTING    = 7,    // Fin d'appel en cours
    CALL_STATE_ENDED            = 8,    // Appel terminé
    CALL_STATE_REJECTED         = 9,    // Appel rejeté
    CALL_STATE_BUSY             = 10,   // Occupé
    CALL_STATE_NO_ANSWER        = 11,   // Pas de réponse
    CALL_STATE_ERROR            = 12    // Erreur
} CallState;

/**
 * @brief Types de messages d'appel
 */
typedef enum {
    CALL_MSG_REQUEST        = 0x10,     // Demande d'appel
    CALL_MSG_RINGING        = 0x11,     // Indication de sonnerie
    CALL_MSG_ACCEPT         = 0x12,     // Acceptation
    CALL_MSG_REJECT         = 0x13,     // Refus
    CALL_MSG_BUSY           = 0x14,     // Occupé
    CALL_MSG_END            = 0x15,     // Fin d'appel
    CALL_MSG_HOLD           = 0x16,     // Mise en attente
    CALL_MSG_RESUME         = 0x17,     // Reprise
    CALL_MSG_KEEPALIVE      = 0x18,     // Maintien de connexion
    CALL_MSG_EMERGENCY      = 0x19,     // Appel d'urgence
    CALL_MSG_CALL_WAITING   = 0x1A,     // Double appel
    CALL_MSG_MISSED         = 0x1B      // Appel manqué (notification)
} CallMessageType;

/**
 * @brief Raisons de fin d'appel
 */
typedef enum {
    CALL_END_NORMAL         = 0,    // Fin normale
    CALL_END_USER_REJECT    = 1,    // Refusé par l'utilisateur
    CALL_END_BUSY           = 2,    // Occupé
    CALL_END_NO_ANSWER      = 3,    // Pas de réponse
    CALL_END_TIMEOUT        = 4,    // Timeout
    CALL_END_NETWORK_ERROR  = 5,    // Erreur réseau
    CALL_END_AUDIO_ERROR    = 6,    // Erreur audio
    CALL_END_POWER_OFF      = 7,    // Téléphone éteint
    CALL_END_OUT_OF_RANGE   = 8,    // Hors de portée
    CALL_END_EMERGENCY      = 9     // Interrompu par appel urgence
} CallEndReason;

// ============================================================
// SECTION 3 : INFORMATIONS D'APPEL
// ============================================================

/**
 * @brief Enregistrement d'un appel (Call Detail Record)
 */
typedef struct {
    uint32_t callId;                    // Identifiant unique de l'appel
    CallState state;                    // État actuel
    
    // Participants
    char callerMsisdn[IDENTITY_PHONE_NUMBER_MAX];   // Numéro appelant
    char callerName[IDENTITY_DEVICE_NAME_MAX];      // Nom appelant
    char calleeMsisdn[IDENTITY_PHONE_NUMBER_MAX];   // Numéro appelé
    char calleeName[IDENTITY_DEVICE_NAME_MAX];      // Nom appelé
    
    // Timings
    uint32_t startTime;                 // Début de l'appel
    uint32_t connectTime;               // Connexion établie
    uint32_t endTime;                   // Fin de l'appel
    uint32_t duration;                  // Durée (secondes)
    
    // Fin d'appel
    CallEndReason endReason;            // Raison de fin
    bool isIncoming;                    // Appel entrant ?
    bool isEmergency;                   // Appel d'urgence ?
    bool isMissed;                      // Appel manqué ?
    
    // Qualité
    int16_t avgRssi;                    // RSSI moyen pendant l'appel
    uint8_t quality;                    // Qualité estimée (0-100)
    
    // Statistiques
    uint32_t audioPacketsSent;         // Paquets audio envoyés
    uint32_t audioPacketsReceived;     // Paquets audio reçus
    uint32_t audioPacketsLost;         // Paquets audio perdus
} CallRecord;

/**
 * @brief Format d'un message d'appel
 */
typedef struct __attribute__((packed)) {
    uint8_t messageType;                // Type de message (CallMessageType)
    uint8_t protocolVersion;            // Version du protocole
    
    uint32_t callId;                    // Identifiant de l'appel
    
    // Participants
    char callerMsisdn[IDENTITY_PHONE_NUMBER_MAX];
    char callerName[IDENTITY_DEVICE_NAME_MAX];
    char calleeMsisdn[IDENTITY_PHONE_NUMBER_MAX];
    
    // Informations
    uint8_t callTypeFlags;              // Flags (urgence, etc.)
    uint8_t endReason;                  // Raison de fin (si applicable)
    uint32_t timestamp;                 // Horodatage
    
    // Réservé
    uint8_t reserved[16];
} CallMessage;

// ============================================================
// SECTION 4 : ÉTAT DU PROTOCOLE D'APPEL
// ============================================================

/**
 * @brief État du module de gestion d'appels
 */
typedef struct {
    bool initialized;                   // Module initialisé
    
    // Appel actif
    CallRecord activeCall;              // Appel en cours
    CallState currentState;             // État actuel
    bool inCall;                        // En communication
    
    // Appel en attente
    CallRecord waitingCall;             // Appel en attente
    bool hasWaitingCall;                // Double appel
    
    // Historique
    #define CALL_HISTORY_MAX            50
    CallRecord callHistory[CALL_HISTORY_MAX];
    uint8_t callHistoryCount;
    
    // Timers
    uint32_t ringStartTime;             // Début de la sonnerie
    uint32_t lastKeepaliveTime;         // Dernier keep-alive
    
    // Statistiques
    uint32_t totalCallsMade;            // Appels émis
    uint32_t totalCallsReceived;        // Appels reçus
    uint32_t totalCallsMissed;          // Appels manqués
    uint32_t totalCallDuration;         // Durée totale
    
} CallProtocolState;

// ============================================================
// SECTION 5 : CALLBACKS
// ============================================================

typedef void (*CallProtocol_IncomingCallback)(const char* callerNumber, const char* callerName);
typedef void (*CallProtocol_RingingCallback)(void);
typedef void (*CallProtocol_ConnectedCallback)(const char* number, const char* name);
typedef void (*CallProtocol_EndedCallback)(const char* number, uint32_t duration, CallEndReason reason);
typedef void (*CallProtocol_RejectedCallback)(const char* number, CallEndReason reason);
typedef void (*CallProtocol_MissedCallback)(const char* callerNumber);
typedef void (*CallProtocol_StateCallback)(CallState oldState, CallState newState);

// ============================================================
// SECTION 6 : FONCTIONS D'INITIALISATION
// ============================================================

bool call_protocol_init(void);
void call_protocol_deinit(void);
bool call_protocol_is_ready(void);

// ============================================================
// SECTION 7 : FONCTIONS D'APPEL
// ============================================================

bool call_protocol_make_call(const char* calleeNumber);
bool call_protocol_answer_call(void);
bool call_protocol_reject_call(CallEndReason reason);
bool call_protocol_end_call(CallEndReason reason);
bool call_protocol_hold_call(void);
bool call_protocol_resume_call(void);

// ============================================================
// SECTION 8 : FONCTIONS D'ÉTAT
// ============================================================

CallState call_protocol_get_state(void);
bool call_protocol_is_in_call(void);
const char* call_protocol_get_remote_number(void);
const char* call_protocol_get_remote_name(void);
uint32_t call_protocol_get_call_duration(void);
CallRecord* call_protocol_get_active_call(void);

// ============================================================
// SECTION 9 : FONCTIONS DE TRAITEMENT
// ============================================================

void call_protocol_process(void);
void call_protocol_process_message(const CallMessage* message, int16_t rssi);
void call_protocol_build_message(CallMessage* message, CallMessageType type);

// ============================================================
// SECTION 10 : FONCTIONS D'HISTORIQUE
// ============================================================

uint8_t call_protocol_get_history(CallRecord* records, uint8_t maxCount);
uint8_t call_protocol_get_missed_count(void);
void call_protocol_clear_history(void);

// ============================================================
// SECTION 11 : FONCTIONS DE CALLBACKS
// ============================================================

void call_protocol_set_incoming_callback(CallProtocol_IncomingCallback callback);
void call_protocol_set_ringing_callback(CallProtocol_RingingCallback callback);
void call_protocol_set_connected_callback(CallProtocol_ConnectedCallback callback);
void call_protocol_set_ended_callback(CallProtocol_EndedCallback callback);
void call_protocol_set_rejected_callback(CallProtocol_RejectedCallback callback);
void call_protocol_set_missed_callback(CallProtocol_MissedCallback callback);
void call_protocol_set_state_callback(CallProtocol_StateCallback callback);

// ============================================================
// SECTION 12 : FONCTIONS DE DÉBOGAGE
// ============================================================

void call_protocol_print_state(void);
void call_protocol_print_active_call(void);
void call_protocol_print_history(void);
bool call_protocol_self_test(void);

// ============================================================
// SECTION 13 : MACROS UTILITAIRES
// ============================================================

#define CALL_IS_ACTIVE()                call_protocol_is_in_call()
#define CALL_GET_DURATION()             call_protocol_get_call_duration()
#define CALL_GET_STATE()                call_protocol_get_state()

// ============================================================
// SECTION 14 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define CALL_DEBUG(fmt, ...)        printf("[CALL] " fmt, ##__VA_ARGS__)
#else
    #define CALL_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // CALL_PROTOCOL_H