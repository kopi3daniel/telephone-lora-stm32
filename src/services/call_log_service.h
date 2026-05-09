/**
 * @file call_log_service.h
 * @brief Service de journal d'appels (Call Log)
 * 
 * Ce fichier implémente le service de gestion de l'historique
 * des appels (journal) :
 * - Enregistrement des appels (entrants, sortants, manqués)
 * - Consultation par type (tous, manqués, entrants, sortants)
 * - Statistiques (durée totale, nombre d'appels)
 * - Nettoyage automatique
 * - Filtrage par contact
 * 
 * Types d'appels enregistrés :
 * - INCOMING : Appel entrant (répondu)
 * - OUTGOING : Appel sortant
 * - MISSED   : Appel manqué
 * - REJECTED : Appel refusé
 * 
 * Chaque entrée contient :
 * - Numéro de téléphone
 * - Nom du contact (si connu)
 * - Type d'appel
 * - Date et heure
 * - Durée (pour les appels répondus)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef CALL_LOG_SERVICE_H
#define CALL_LOG_SERVICE_H

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
#include "../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du service */
#define CALL_LOG_VERSION                "1.0.0"

/** @brief Nombre maximum d'entrées dans le journal */
#define CALL_LOG_MAX_ENTRIES            100

/** @brief Nombre maximum d'entrées par page */
#define CALL_LOG_PAGE_SIZE              20

/** @brief Durée de conservation (jours, 0 = illimité) */
#define CALL_LOG_RETENTION_DAYS         90

// ============================================================
// SECTION 2 : TYPES D'APPELS
// ============================================================

/**
 * @brief Type d'appel dans le journal
 */
typedef enum {
    CALL_LOG_TYPE_ALL       = 0,    // Tous les types
    CALL_LOG_TYPE_INCOMING  = 1,    // Appels entrants (répondus)
    CALL_LOG_TYPE_OUTGOING  = 2,    // Appels sortants
    CALL_LOG_TYPE_MISSED    = 3,    // Appels manqués
    CALL_LOG_TYPE_REJECTED  = 4     // Appels refusés
} CallLogType;

/**
 * @brief Entrée du journal d'appels
 */
typedef struct {
    uint32_t id;                                // Identifiant unique
    
    // Contact
    char number[IDENTITY_PHONE_NUMBER_MAX];     // Numéro de téléphone
    char contactName[CONTACT_NAME_MAX_LENGTH];  // Nom du contact
    
    // Type
    CallLogType type;                           // Type d'appel
    bool isRead;                                // Consulté ? (pour les manqués)
    
    // Timings
    uint32_t timestamp;                         // Date/heure de l'appel
    uint32_t duration;                          // Durée en secondes (0 si manqué)
    
    // Informations supplémentaires
    CallEndReason endReason;                    // Raison de fin
    int16_t rssi;                               // Qualité du signal
    uint8_t quality;                            // Qualité estimée (0-100)
    
} CallLogEntry;

// ============================================================
// SECTION 3 : ÉTAT DU SERVICE
// ============================================================

/**
 * @brief État du service de journal d'appels
 */
typedef struct {
    bool initialized;                           // Service initialisé
    
    // Journal
    CallLogEntry entries[CALL_LOG_MAX_ENTRIES];
    uint16_t entryCount;
    uint32_t nextId;                            // Prochain ID
    
    // Compteurs par type
    uint16_t incomingCount;
    uint16_t outgoingCount;
    uint16_t missedCount;
    uint16_t rejectedCount;
    
    // Statistiques
    uint32_t totalDuration;                     // Durée totale
    uint32_t longestCall;                       // Appel le plus long
    uint32_t shortestCall;                      // Appel le plus court
    char mostCalledNumber[IDENTITY_PHONE_NUMBER_MAX]; // Numéro le plus appelé
    
    // Tri
    bool sortAscending;                         // Ordre croissant
    
} CallLogState;

// ============================================================
// SECTION 4 : CALLBACKS
// ============================================================

typedef void (*CallLog_EntryAddedCallback)(const CallLogEntry* entry);
typedef void (*CallLog_ClearedCallback)(void);

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

bool call_log_init(void);
void call_log_deinit(void);
bool call_log_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS D'AJOUT
// ============================================================

bool call_log_add_entry(const char* number, CallLogType type, uint32_t duration);
bool call_log_add_incoming(const char* number, uint32_t duration);
bool call_log_add_outgoing(const char* number, uint32_t duration);
bool call_log_add_missed(const char* number);
bool call_log_add_rejected(const char* number, CallEndReason reason);

// ============================================================
// SECTION 7 : FONCTIONS DE CONSULTATION
// ============================================================

uint16_t call_log_get_count(CallLogType type);
uint16_t call_log_get_entries(CallLogType type, CallLogEntry* entries, uint16_t maxCount);
uint16_t call_log_get_page(CallLogType type, uint8_t page, CallLogEntry* entries);
CallLogEntry* call_log_get_entry(uint32_t id);
CallLogEntry* call_log_get_last_entry(void);

// ============================================================
// SECTION 8 : FONCTIONS DE RECHERCHE
// ============================================================

uint16_t call_log_find_by_number(const char* number, CallLogEntry* results, uint16_t maxResults);
uint16_t call_log_find_by_date(uint32_t startTime, uint32_t endTime, 
                                CallLogEntry* results, uint16_t maxResults);
uint16_t call_log_get_calls_with_contact(const char* number);

// ============================================================
// SECTION 9 : FONCTIONS DE STATISTIQUES
// ============================================================

uint16_t call_log_get_incoming_count(void);
uint16_t call_log_get_outgoing_count(void);
uint16_t call_log_get_missed_count(void);
uint16_t call_log_get_rejected_count(void);
uint32_t call_log_get_total_duration(void);
uint32_t call_log_get_average_duration(void);
const char* call_log_get_most_called_number(void);

// ============================================================
// SECTION 10 : FONCTIONS DE GESTION
// ============================================================

void call_log_mark_all_read(void);
void call_log_clear(CallLogType type);
void call_log_clear_all(void);
void call_log_cleanup(void);
bool call_log_delete_entry(uint32_t id);

// ============================================================
// SECTION 11 : FONCTIONS DE PERSISTANCE
// ============================================================

bool call_log_save(void);
bool call_log_load(void);

// ============================================================
// SECTION 12 : FONCTIONS DE CALLBACKS
// ============================================================

void call_log_set_entry_added_callback(CallLog_EntryAddedCallback callback);
void call_log_set_cleared_callback(CallLog_ClearedCallback callback);

// ============================================================
// SECTION 13 : FONCTIONS DE DÉBOGAGE
// ============================================================

void call_log_print_all(void);
void call_log_print_type(CallLogType type);
void call_log_print_entry(const CallLogEntry* entry);
void call_log_print_statistics(void);
bool call_log_self_test(void);

// ============================================================
// SECTION 14 : MACROS UTILITAIRES
// ============================================================

#define CALL_LOG_HAS_MISSED()           (call_log_get_missed_count() > 0)
#define CALL_LOG_GET_MISSED_COUNT()     call_log_get_missed_count()
#define CALL_LOG_GET_TOTAL_COUNT()      call_log_get_count(CALL_LOG_TYPE_ALL)

// ============================================================
// SECTION 15 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define CALL_LOG_DEBUG(fmt, ...)    printf("[CALL_LOG] " fmt, ##__VA_ARGS__)
#else
    #define CALL_LOG_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 16 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // CALL_LOG_SERVICE_H