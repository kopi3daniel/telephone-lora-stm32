/**
 * @file call_log_service.cpp
 * @brief Implémentation du service de journal d'appels
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans call_log_service.h.
 * 
 * Il gère :
 * - L'enregistrement des appels
 * - La consultation par type
 * - Les statistiques
 * - Le nettoyage automatique
 * - La persistance
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "call_log_service.h"
#include "contact_service.h"
#include "../drivers/storage/flash_eeprom.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du service */
static CallLogState call_log;

/** @brief Callbacks */
static CallLog_EntryAddedCallback entry_added_cb = NULL;
static CallLog_ClearedCallback cleared_cb = NULL;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le journal d'appels
 */
bool call_log_init(void)
{
    CALL_LOG_DEBUG("Initialisation du journal d'appels...\n");
    
    memset(&call_log, 0, sizeof(CallLogState));
    call_log.sortAscending = false;  // Plus récent en premier
    call_log.shortestCall = 0xFFFFFFFF;
    
    // Charger depuis la Flash
    if (!call_log_load())
    {
        CALL_LOG_DEBUG("Aucun journal sauvegardé\n");
    }
    
    call_log.initialized = true;
    
    CALL_LOG_DEBUG("Journal initialisé (%d entrées)\n", call_log.entryCount);
    return true;
}

void call_log_deinit(void)
{
    call_log_save();
    call_log.initialized = false;
}

bool call_log_is_ready(void)
{
    return call_log.initialized;
}

// ============================================================
// SECTION 2 : AJOUT D'ENTRÉES
// ============================================================

bool call_log_add_entry(const char* number, CallLogType type, uint32_t duration)
{
    if (!call_log.initialized) return false;
    if (number == NULL) return false;
    
    // Décaler si plein
    if (call_log.entryCount >= CALL_LOG_MAX_ENTRIES)
    {
        // Supprimer la plus ancienne
        memmove(&call_log.entries[0], &call_log.entries[1],
                (CALL_LOG_MAX_ENTRIES - 1) * sizeof(CallLogEntry));
        call_log.entryCount = CALL_LOG_MAX_ENTRIES - 1;
    }
    
    // Créer l'entrée
    CallLogEntry* entry = &call_log.entries[call_log.entryCount++];
    memset(entry, 0, sizeof(CallLogEntry));
    
    entry->id = ++call_log.nextId;
    strncpy(entry->number, number, IDENTITY_PHONE_NUMBER_MAX - 1);
    entry->type = type;
    entry->duration = duration;
    entry->timestamp = HAL_GetTick();
    entry->isRead = (type != CALL_LOG_TYPE_MISSED);  // Les manqués sont non lus
    
    // Chercher le contact
    int16_t contactIndex = contact_service_find_by_number(number);
    if (contactIndex >= 0)
    {
        Contact* contact = contact_service_get(contactIndex);
        if (contact)
        {
            strncpy(entry->contactName, contact->name, CONTACT_NAME_MAX_LENGTH - 1);
            contact_service_increment_call_count(contactIndex);
            contact_service_update_last_call(contactIndex);
        }
    }
    else
    {
        strncpy(entry->contactName, number, CONTACT_NAME_MAX_LENGTH - 1);
    }
    
    // Mettre à jour les compteurs
    switch (type)
    {
        case CALL_LOG_TYPE_INCOMING:
            call_log.incomingCount++;
            break;
        case CALL_LOG_TYPE_OUTGOING:
            call_log.outgoingCount++;
            break;
        case CALL_LOG_TYPE_MISSED:
            call_log.missedCount++;
            break;
        case CALL_LOG_TYPE_REJECTED:
            call_log.rejectedCount++;
            break;
        default:
            break;
    }
    
    // Mettre à jour les statistiques
    if (duration > 0)
    {
        call_log.totalDuration += duration;
        
        if (duration > call_log.longestCall)
            call_log.longestCall = duration;
        
        if (duration < call_log.shortestCall)
            call_log.shortestCall = duration;
    }
    
    // Mettre à jour le numéro le plus appelé
    update_most_called(number);
    
    // Trier
    sort_entries();
    
    CALL_LOG_DEBUG("Entrée ajoutée : %s (%d)\n", number, type);
    
    if (entry_added_cb) entry_added_cb(entry);
    
    return true;
}

bool call_log_add_incoming(const char* number, uint32_t duration)
{
    return call_log_add_entry(number, CALL_LOG_TYPE_INCOMING, duration);
}

bool call_log_add_outgoing(const char* number, uint32_t duration)
{
    return call_log_add_entry(number, CALL_LOG_TYPE_OUTGOING, duration);
}

bool call_log_add_missed(const char* number)
{
    return call_log_add_entry(number, CALL_LOG_TYPE_MISSED, 0);
}

bool call_log_add_rejected(const char* number, CallEndReason reason)
{
    CallLogEntry* entry = NULL;
    
    if (call_log.entryCount > 0)
    {
        entry = &call_log.entries[call_log.entryCount - 1];
        entry->endReason = reason;
    }
    
    return call_log_add_entry(number, CALL_LOG_TYPE_REJECTED, 0);
}

// ============================================================
// SECTION 3 : CONSULTATION
// ============================================================

uint16_t call_log_get_count(CallLogType type)
{
    if (!call_log.initialized) return 0;
    
    if (type == CALL_LOG_TYPE_ALL) return call_log.entryCount;
    
    uint16_t count = 0;
    for (uint16_t i = 0; i < call_log.entryCount; i++)
    {
        if (call_log.entries[i].type == type) count++;
    }
    return count;
}

uint16_t call_log_get_entries(CallLogType type, CallLogEntry* entries, uint16_t maxCount)
{
    if (!call_log.initialized || entries == NULL) return 0;
    
    uint16_t count = 0;
    
    for (uint16_t i = 0; i < call_log.entryCount && count < maxCount; i++)
    {
        if (type == CALL_LOG_TYPE_ALL || call_log.entries[i].type == type)
        {
            memcpy(&entries[count], &call_log.entries[i], sizeof(CallLogEntry));
            count++;
        }
    }
    
    return count;
}

uint16_t call_log_get_page(CallLogType type, uint8_t page, CallLogEntry* entries)
{
    if (!call_log.initialized || entries == NULL) return 0;
    
    uint16_t startIndex = page * CALL_LOG_PAGE_SIZE;
    uint16_t count = 0;
    
    for (uint16_t i = startIndex; i < call_log.entryCount && count < CALL_LOG_PAGE_SIZE; i++)
    {
        if (type == CALL_LOG_TYPE_ALL || call_log.entries[i].type == type)
        {
            memcpy(&entries[count], &call_log.entries[i], sizeof(CallLogEntry));
            count++;
        }
    }
    
    return count;
}

CallLogEntry* call_log_get_entry(uint32_t id)
{
    for (uint16_t i = 0; i < call_log.entryCount; i++)
    {
        if (call_log.entries[i].id == id)
            return &call_log.entries[i];
    }
    return NULL;
}

CallLogEntry* call_log_get_last_entry(void)
{
    if (call_log.entryCount == 0) return NULL;
    return &call_log.entries[call_log.entryCount - 1];
}

// ============================================================
// SECTION 4 : RECHERCHE
// ============================================================

uint16_t call_log_find_by_number(const char* number, CallLogEntry* results, uint16_t maxResults)
{
    if (!call_log.initialized || number == NULL || results == NULL) return 0;
    
    uint16_t found = 0;
    
    for (uint16_t i = 0; i < call_log.entryCount && found < maxResults; i++)
    {
        if (strcmp(call_log.entries[i].number, number) == 0)
        {
            memcpy(&results[found], &call_log.entries[i], sizeof(CallLogEntry));
            found++;
        }
    }
    
    return found;
}

uint16_t call_log_find_by_date(uint32_t startTime, uint32_t endTime,
                                CallLogEntry* results, uint16_t maxResults)
{
    if (!call_log.initialized || results == NULL) return 0;
    
    uint16_t found = 0;
    
    for (uint16_t i = 0; i < call_log.entryCount && found < maxResults; i++)
    {
        if (call_log.entries[i].timestamp >= startTime && 
            call_log.entries[i].timestamp <= endTime)
        {
            memcpy(&results[found], &call_log.entries[i], sizeof(CallLogEntry));
            found++;
        }
    }
    
    return found;
}

uint16_t call_log_get_calls_with_contact(const char* number)
{
    return call_log_find_by_number(number, NULL, 0);  // Count only
}

// ============================================================
// SECTION 5 : STATISTIQUES
// ============================================================

uint16_t call_log_get_incoming_count(void) { return call_log.incomingCount; }
uint16_t call_log_get_outgoing_count(void) { return call_log.outgoingCount; }
uint16_t call_log_get_missed_count(void)  { return call_log.missedCount; }
uint16_t call_log_get_rejected_count(void) { return call_log.rejectedCount; }

uint32_t call_log_get_total_duration(void)
{
    return call_log.totalDuration;
}

uint32_t call_log_get_average_duration(void)
{
    uint16_t answeredCalls = call_log.incomingCount + call_log.outgoingCount;
    if (answeredCalls == 0) return 0;
    return call_log.totalDuration / answeredCalls;
}

const char* call_log_get_most_called_number(void)
{
    return call_log.mostCalledNumber;
}

// ============================================================
// SECTION 6 : GESTION
// ============================================================

void call_log_mark_all_read(void)
{
    for (uint16_t i = 0; i < call_log.entryCount; i++)
    {
        call_log.entries[i].isRead = true;
    }
}

void call_log_clear(CallLogType type)
{
    if (type == CALL_LOG_TYPE_ALL)
    {
        call_log_clear_all();
        return;
    }
    
    uint16_t writeIndex = 0;
    
    for (uint16_t i = 0; i < call_log.entryCount; i++)
    {
        if (call_log.entries[i].type != type)
        {
            if (writeIndex != i)
            {
                memcpy(&call_log.entries[writeIndex], &call_log.entries[i], sizeof(CallLogEntry));
            }
            writeIndex++;
        }
    }
    
    // Mettre à jour les compteurs
    switch (type)
    {
        case CALL_LOG_TYPE_INCOMING: call_log.incomingCount = 0; break;
        case CALL_LOG_TYPE_OUTGOING: call_log.outgoingCount = 0; break;
        case CALL_LOG_TYPE_MISSED:   call_log.missedCount = 0; break;
        case CALL_LOG_TYPE_REJECTED: call_log.rejectedCount = 0; break;
        default: break;
    }
    
    call_log.entryCount = writeIndex;
}

void call_log_clear_all(void)
{
    call_log.entryCount = 0;
    call_log.incomingCount = 0;
    call_log.outgoingCount = 0;
    call_log.missedCount = 0;
    call_log.rejectedCount = 0;
    call_log.totalDuration = 0;
    call_log.longestCall = 0;
    call_log.shortestCall = 0xFFFFFFFF;
    
    memset(call_log.entries, 0, sizeof(call_log.entries));
    memset(call_log.mostCalledNumber, 0, sizeof(call_log.mostCalledNumber));
    
    if (cleared_cb) cleared_cb();
}

void call_log_cleanup(void)
{
    if (CALL_LOG_RETENTION_DAYS == 0) return;
    
    uint32_t cutoffTime = HAL_GetTick() - (CALL_LOG_RETENTION_DAYS * 86400 * 1000);
    uint16_t writeIndex = 0;
    
    for (uint16_t i = 0; i < call_log.entryCount; i++)
    {
        if (call_log.entries[i].timestamp >= cutoffTime)
        {
            if (writeIndex != i)
            {
                memcpy(&call_log.entries[writeIndex], &call_log.entries[i], sizeof(CallLogEntry));
            }
            writeIndex++;
        }
    }
    
    call_log.entryCount = writeIndex;
    CALL_LOG_DEBUG("Nettoyage : %d entrées conservées\n", call_log.entryCount);
}

bool call_log_delete_entry(uint32_t id)
{
    for (uint16_t i = 0; i < call_log.entryCount; i++)
    {
        if (call_log.entries[i].id == id)
        {
            // Mettre à jour les compteurs
            switch (call_log.entries[i].type)
            {
                case CALL_LOG_TYPE_INCOMING: call_log.incomingCount--; break;
                case CALL_LOG_TYPE_OUTGOING: call_log.outgoingCount--; break;
                case CALL_LOG_TYPE_MISSED:   call_log.missedCount--; break;
                case CALL_LOG_TYPE_REJECTED: call_log.rejectedCount--; break;
                default: break;
            }
            
            // Décaler
            if (i < call_log.entryCount - 1)
            {
                memmove(&call_log.entries[i], &call_log.entries[i + 1],
                        (call_log.entryCount - i - 1) * sizeof(CallLogEntry));
            }
            call_log.entryCount--;
            return true;
        }
    }
    return false;
}

// ============================================================
// SECTION 7 : PERSISTANCE
// ============================================================

bool call_log_save(void)
{
    FlashEEPROM_Error err = flash_eeprom_write(EEPROM_ID_CALL_LOG,
                                                (uint8_t*)&call_log,
                                                sizeof(CallLogState));
    return (err == FLASH_EEPROM_OK);
}

bool call_log_load(void)
{
    uint16_t readSize;
    FlashEEPROM_Error err = flash_eeprom_read(EEPROM_ID_CALL_LOG,
                                               (uint8_t*)&call_log,
                                               sizeof(CallLogState),
                                               &readSize);
    return (err == FLASH_EEPROM_OK && readSize >= sizeof(CallLogState));
}

// ============================================================
// SECTION 8 : CALLBACKS
// ============================================================

void call_log_set_entry_added_callback(CallLog_EntryAddedCallback cb) { entry_added_cb = cb; }
void call_log_set_cleared_callback(CallLog_ClearedCallback cb) { cleared_cb = cb; }

// ============================================================
// SECTION 9 : FONCTIONS INTERNES
// ============================================================

static void sort_entries(void)
{
    // Tri par timestamp décroissant (plus récent en premier)
    for (uint16_t i = 0; i < call_log.entryCount - 1; i++)
    {
        for (uint16_t j = 0; j < call_log.entryCount - i - 1; j++)
        {
            bool shouldSwap = call_log.sortAscending ?
                (call_log.entries[j].timestamp > call_log.entries[j + 1].timestamp) :
                (call_log.entries[j].timestamp < call_log.entries[j + 1].timestamp);
            
            if (shouldSwap)
            {
                CallLogEntry temp = call_log.entries[j];
                call_log.entries[j] = call_log.entries[j + 1];
                call_log.entries[j + 1] = temp;
            }
        }
    }
}

static void update_most_called(const char* number)
{
    // Compter les appels pour ce numéro
    uint16_t maxCount = 0;
    char maxNumber[IDENTITY_PHONE_NUMBER_MAX] = "";
    
    // Structure temporaire pour compter
    typedef struct {
        char number[IDENTITY_PHONE_NUMBER_MAX];
        uint16_t count;
    } NumberCount;
    
    NumberCount counts[50] = {0};
    uint8_t countIndex = 0;
    
    for (uint16_t i = 0; i < call_log.entryCount; i++)
    {
        const char* num = call_log.entries[i].number;
        
        // Chercher si déjà compté
        bool found = false;
        for (uint8_t j = 0; j < countIndex; j++)
        {
            if (strcmp(counts[j].number, num) == 0)
            {
                counts[j].count++;
                found = true;
                break;
            }
        }
        
        if (!found && countIndex < 50)
        {
            strncpy(counts[countIndex].number, num, IDENTITY_PHONE_NUMBER_MAX - 1);
            counts[countIndex].count = 1;
            countIndex++;
        }
    }
    
    // Trouver le maximum
    for (uint8_t i = 0; i < countIndex; i++)
    {
        if (counts[i].count > maxCount)
        {
            maxCount = counts[i].count;
            strncpy(maxNumber, counts[i].number, IDENTITY_PHONE_NUMBER_MAX - 1);
        }
    }
    
    strncpy(call_log.mostCalledNumber, maxNumber, IDENTITY_PHONE_NUMBER_MAX - 1);
}

// ============================================================
// SECTION 10 : DÉBOGAGE
// ============================================================

void call_log_print_all(void)
{
    printf("\n═══ JOURNAL APPELS (%d entrées) ═══\n", call_log.entryCount);
    printf("%-4s %-16s %-20s %-10s %-8s\n", "Type", "Numéro", "Contact", "Durée", "Date");
    printf("──────────────────────────────────────────────────────────\n");
    
    for (uint16_t i = 0; i < call_log.entryCount; i++)
    {
        CallLogEntry* e = &call_log.entries[i];
        
        const char* typeStr = "?";
        switch (e->type)
        {
            case CALL_LOG_TYPE_INCOMING: typeStr = "← ENT."; break;
            case CALL_LOG_TYPE_OUTGOING: typeStr = "→ SOR."; break;
            case CALL_LOG_TYPE_MISSED:   typeStr = "✗ MAN."; break;
            case CALL_LOG_TYPE_REJECTED: typeStr = "✗ REJ."; break;
            default: break;
        }
        
        printf("%-8s %-16s %-20s %-6lu s %-8lu\n",
               typeStr, e->number, e->contactName,
               (unsigned long)e->duration,
               (unsigned long)(e->timestamp / 1000));
    }
    printf("══════════════════════════════════════════\n\n");
}

void call_log_print_type(CallLogType type)
{
    const char* typeName = "INCONNU";
    switch (type)
    {
        case CALL_LOG_TYPE_ALL:      typeName = "TOUS"; break;
        case CALL_LOG_TYPE_INCOMING: typeName = "ENTRANTS"; break;
        case CALL_LOG_TYPE_OUTGOING: typeName = "SORTANTS"; break;
        case CALL_LOG_TYPE_MISSED:   typeName = "MANQUÉS"; break;
        case CALL_LOG_TYPE_REJECTED: typeName = "REFUSÉS"; break;
    }
    
    uint16_t count = call_log_get_count(type);
    printf("\n═══ APPELS %s (%d) ═══\n", typeName, count);
    
    CallLogEntry entries[20];
    uint16_t shown = call_log_get_entries(type, entries, 20);
    
    for (uint16_t i = 0; i < shown; i++)
    {
        call_log_print_entry(&entries[i]);
    }
    printf("══════════════════════════\n\n");
}

void call_log_print_entry(const CallLogEntry* entry)
{
    if (entry == NULL) return;
    
    const char* typeStr = "?";
    switch (entry->type)
    {
        case CALL_LOG_TYPE_INCOMING: typeStr = "←"; break;
        case CALL_LOG_TYPE_OUTGOING: typeStr = "→"; break;
        case CALL_LOG_TYPE_MISSED:   typeStr = "✗"; break;
        case CALL_LOG_TYPE_REJECTED: typeStr = "✗"; break;
        default: break;
    }
    
    printf("[%s] %s (%s) - %lu s %s\n",
           typeStr, entry->contactName, entry->number,
           (unsigned long)entry->duration,
           entry->isRead ? "" : "●");
}

void call_log_print_statistics(void)
{
    printf("\n═══ STATISTIQUES APPELS ═══\n");
    printf("Total entrées    : %d\n", call_log.entryCount);
    printf("Appels entrants  : %d\n", call_log.incomingCount);
    printf("Appels sortants  : %d\n", call_log.outgoingCount);
    printf("Appels manqués   : %d\n", call_log.missedCount);
    printf("Appels refusés   : %d\n", call_log.rejectedCount);
    printf("Durée totale     : %lu s\n", (unsigned long)call_log.totalDuration);
    printf("Durée moyenne    : %lu s\n", (unsigned long)call_log_get_average_duration());
    printf("Appel le + long  : %lu s\n", (unsigned long)call_log.longestCall);
    printf("Appel le + court : %lu s\n", (unsigned long)call_log.shortestCall);
    printf("N° le + appelé   : %s\n", call_log.mostCalledNumber);
    printf("══════════════════════════\n\n");
}

bool call_log_self_test(void)
{
    CALL_LOG_DEBUG("Auto-test...\n");
    
    if (!call_log.initialized)
    {
        CALL_LOG_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : ajouter une entrée
    uint16_t beforeCount = call_log.entryCount;
    call_log_add_outgoing("0600000000", 60);
    
    if (call_log.entryCount != beforeCount + 1)
    {
        CALL_LOG_DEBUG("Échec : entrée non ajoutée\n");
        return false;
    }
    
    // Nettoyer
    CallLogEntry* last = call_log_get_last_entry();
    if (last) call_log_delete_entry(last->id);
    
    CALL_LOG_DEBUG("Auto-test OK\n");
    return true;
}