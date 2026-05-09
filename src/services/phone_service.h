/**
 * @file phone_service.h
 * @brief Service de téléphonie haut niveau
 * 
 * Ce fichier implémente le service de téléphonie qui orchestre
 * tous les sous-systèmes pour gérer les appels :
 * - Protocole d'appel (call_protocol)
 * - Audio (audio_manager)
 * - Interface utilisateur (notifications)
 * - Journal d'appels
 * 
 * Il fournit une API simple pour l'application :
 * - appeler(numéro)
 * - raccrocher()
 * - répondre()
 * - refuser()
 * 
 * États d'un appel :
 * ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
 * │  IDLE    │───►│ DIALING  │───►│ RINGING  │───►│CONNECTED │
 * └──────────┘    └──────────┘    └──────────┘    └──────────┘
 *       ▲                                              │
 *       └──────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef PHONE_SERVICE_H
#define PHONE_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "../protocols/call_protocol.h"
#include "../protocols/identity.h"
#include "../drivers/audio/audio_manager.h"
#include "../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du service */
#define PHONE_SERVICE_VERSION           "1.0.0"

/** @brief Nombre maximum de contacts */
#define PHONE_MAX_CONTACTS              200

/** @brief Nombre maximum de favoris */
#define PHONE_MAX_FAVORITES             20

/** @brief Nombre maximum d'appels dans le journal */
#define PHONE_MAX_CALL_LOG              100

/** @brief Nombre maximum de numéros bloqués */
#define PHONE_MAX_BLOCKED               50

// ============================================================
// SECTION 2 : TYPES DE DONNÉES
// ============================================================

/**
 * @brief Contact téléphonique
 */
typedef struct {
    char name[32];                      // Nom du contact
    char number[IDENTITY_PHONE_NUMBER_MAX];  // Numéro de téléphone
    bool favorite;                      // Favori ?
    uint8_t ringtoneIndex;              // Sonnerie personnalisée
    uint32_t lastContact;               // Dernier contact
    uint32_t callCount;                 // Nombre d'appels
    uint32_t totalDuration;             // Durée totale
} PhoneContact;

/**
 * @brief État du service téléphonie
 */
typedef enum {
    PHONE_STATE_IDLE        = 0,    // En attente
    PHONE_STATE_DIALING     = 1,    // Numérotation
    PHONE_STATE_RINGING     = 2,    // Sonnerie (appel sortant)
    PHONE_STATE_INCOMING    = 3,    // Appel entrant
    PHONE_STATE_CONNECTED   = 4,    // En communication
    PHONE_STATE_ENDED       = 5     // Appel terminé
} PhoneState;

// ============================================================
// SECTION 3 : ÉTAT DU SERVICE
// ============================================================

/**
 * @brief État du service téléphonie
 */
typedef struct {
    bool initialized;                   // Service initialisé
    
    // État de l'appel
    PhoneState state;                   // État actuel
    char currentNumber[IDENTITY_PHONE_NUMBER_MAX];  // Numéro actuel
    char currentName[32];               // Nom du correspondant
    uint32_t callStartTime;             // Début de l'appel
    uint32_t callDuration;              // Durée (secondes)
    bool isIncoming;                    // Appel entrant ?
    bool isMuted;                       // Mode muet ?
    bool isSpeakerOn;                   // Haut-parleur ?
    
    // Contacts
    PhoneContact contacts[PHONE_MAX_CONTACTS];
    uint16_t contactCount;
    
    // Favoris
    uint8_t favorites[PHONE_MAX_FAVORITES];  // Indices des contacts favoris
    uint8_t favoriteCount;
    
    // Journal d'appels
    CallRecord callLog[PHONE_MAX_CALL_LOG];
    uint16_t callLogCount;
    
    // Numéros bloqués
    char blockedNumbers[PHONE_MAX_BLOCKED][IDENTITY_PHONE_NUMBER_MAX];
    uint8_t blockedCount;
    
    // Statistiques
    uint32_t totalCallsMade;
    uint32_t totalCallsReceived;
    uint32_t totalCallsMissed;
    uint32_t totalCallDuration;
    
} PhoneServiceState;

// ============================================================
// SECTION 4 : CALLBACKS
// ============================================================

typedef void (*PhoneService_StateCallback)(PhoneState oldState, PhoneState newState);
typedef void (*PhoneService_IncomingCallback)(const char* number, const char* name);
typedef void (*PhoneService_ConnectedCallback)(const char* number);
typedef void (*PhoneService_EndedCallback)(const char* number, uint32_t duration);
typedef void (*PhoneService_MissedCallback)(const char* number);

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

bool phone_service_init(void);
void phone_service_deinit(void);
bool phone_service_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS D'APPEL
// ============================================================

bool phone_service_dial(const char* number);
bool phone_service_answer(void);
bool phone_service_reject(void);
bool phone_service_hangup(void);
bool phone_service_redial(void);

// ============================================================
// SECTION 7 : FONCTIONS DE CONTRÔLE D'APPEL
// ============================================================

void phone_service_mute(bool mute);
void phone_service_toggle_mute(void);
bool phone_service_is_muted(void);
void phone_service_speaker(bool on);
void phone_service_toggle_speaker(void);
bool phone_service_is_speaker_on(void);

// ============================================================
// SECTION 8 : FONCTIONS D'ÉTAT
// ============================================================

PhoneState phone_service_get_state(void);
const char* phone_service_get_current_number(void);
const char* phone_service_get_current_name(void);
uint32_t phone_service_get_call_duration(void);
bool phone_service_is_in_call(void);

// ============================================================
// SECTION 9 : FONCTIONS DE CONTACTS
// ============================================================

bool phone_service_add_contact(const char* name, const char* number);
bool phone_service_update_contact(uint16_t index, const char* name, const char* number);
bool phone_service_delete_contact(uint16_t index);
PhoneContact* phone_service_find_contact(const char* number);
PhoneContact* phone_service_find_contact_by_name(const char* name);
uint16_t phone_service_get_contact_count(void);
void phone_service_sort_contacts(void);

// ============================================================
// SECTION 10 : FONCTIONS DE FAVORIS
// ============================================================

bool phone_service_add_favorite(uint16_t contactIndex);
bool phone_service_remove_favorite(uint16_t contactIndex);
bool phone_service_is_favorite(uint16_t contactIndex);
uint8_t phone_service_get_favorite_count(void);
void phone_service_get_favorites(PhoneContact* favorites, uint8_t maxCount);

// ============================================================
// SECTION 11 : FONCTIONS DE JOURNAL
// ============================================================

void phone_service_add_to_call_log(CallRecord* call);
uint16_t phone_service_get_call_log(CallRecord* records, uint16_t maxCount);
uint16_t phone_service_get_missed_count(void);
void phone_service_clear_call_log(void);

// ============================================================
// SECTION 12 : FONCTIONS DE BLOCAGE
// ============================================================

bool phone_service_block_number(const char* number);
bool phone_service_unblock_number(const char* number);
bool phone_service_is_blocked(const char* number);
uint8_t phone_service_get_blocked_count(void);

// ============================================================
// SECTION 13 : FONCTIONS DE CALLBACKS
// ============================================================

void phone_service_set_state_callback(PhoneService_StateCallback callback);
void phone_service_set_incoming_callback(PhoneService_IncomingCallback callback);
void phone_service_set_connected_callback(PhoneService_ConnectedCallback callback);
void phone_service_set_ended_callback(PhoneService_EndedCallback callback);
void phone_service_set_missed_callback(PhoneService_MissedCallback callback);

// ============================================================
// SECTION 14 : FONCTIONS DE TRAITEMENT
// ============================================================

void phone_service_process(void);

// ============================================================
// SECTION 15 : FONCTIONS DE DÉBOGAGE
// ============================================================

void phone_service_print_state(void);
void phone_service_print_contacts(void);
void phone_service_print_call_log(void);
bool phone_service_self_test(void);

// ============================================================
// SECTION 16 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define PHONE_DEBUG(fmt, ...)       printf("[PHONE] " fmt, ##__VA_ARGS__)
#else
    #define PHONE_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 17 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // PHONE_SERVICE_H