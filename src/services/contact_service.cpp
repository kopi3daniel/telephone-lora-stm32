/**
 * @file contact_service.cpp
 * @brief Implémentation du service de gestion des contacts
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans contact_service.h.
 * 
 * Il gère :
 * - Le CRUD des contacts
 * - Les groupes
 * - Les favoris
 * - La numérotation rapide
 * - Le tri et le filtrage
 * - L'import/export
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "contact_service.h"
#include "../drivers/storage/flash_eeprom.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du service */
static ContactServiceState contact_state;

/** @brief Callbacks */
static ContactService_ChangedCallback changed_cb = NULL;
static ContactService_ContactCallback contact_cb = NULL;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le service de contacts
 */
bool contact_service_init(void)
{
    CONTACT_DEBUG("Initialisation du service de contacts...\n");
    
    memset(&contact_state, 0, sizeof(ContactServiceState));
    
    // Configuration par défaut
    contact_state.sortByName = true;
    contact_state.sortAscending = true;
    contact_state.filterGroup[0] = '\0';
    contact_state.showFavoritesOnly = false;
    
    // Charger les contacts depuis la Flash
    if (!contact_service_import_from_flash())
    {
        CONTACT_DEBUG("Aucun contact sauvegardé, initialisation vide\n");
    }
    
    contact_state.initialized = true;
    
    CONTACT_DEBUG("Service initialisé (%d contacts)\n", contact_state.contactCount);
    return true;
}

void contact_service_deinit(void)
{
    contact_service_export_to_flash();
    contact_state.initialized = false;
}

bool contact_service_is_ready(void)
{
    return contact_state.initialized;
}

// ============================================================
// SECTION 2 : CRUD
// ============================================================

bool contact_service_add(const char* name, const char* number)
{
    if (!contact_state.initialized) return false;
    if (name == NULL || number == NULL) return false;
    if (contact_state.contactCount >= CONTACT_MAX_COUNT) return false;
    
    // Vérifier si le numéro existe déjà
    if (contact_service_find_by_number(number) >= 0)
    {
        CONTACT_DEBUG("Numéro déjà existant : %s\n", number);
        return false;
    }
    
    Contact* contact = &contact_state.contacts[contact_state.contactCount];
    memset(contact, 0, sizeof(Contact));
    
    strncpy(contact->name, name, CONTACT_NAME_MAX_LENGTH - 1);
    strncpy(contact->number, number, IDENTITY_PHONE_NUMBER_MAX - 1);
    contact->creationTime = HAL_GetTick();
    contact->modificationTime = HAL_GetTick();
    
    contact_state.contactCount++;
    
    CONTACT_DEBUG("Contact ajouté : %s (%s)\n", name, number);
    
    if (changed_cb) changed_cb();
    
    return true;
}

bool contact_service_update(uint16_t index, const Contact* contact)
{
    if (!contact_state.initialized) return false;
    if (index >= contact_state.contactCount) return false;
    if (contact == NULL) return false;
    
    // Sauvegarder l'ancien numéro pour vérifier les doublons
    char oldNumber[IDENTITY_PHONE_NUMBER_MAX];
    strncpy(oldNumber, contact_state.contacts[index].number, IDENTITY_PHONE_NUMBER_MAX - 1);
    
    // Copier les nouvelles données
    memcpy(&contact_state.contacts[index], contact, sizeof(Contact));
    
    // Restaurer les champs qui ne doivent pas être écrasés
    contact_state.contacts[index].modificationTime = HAL_GetTick();
    
    // Vérifier si le numéro a changé et s'il existe déjà
    if (strcmp(oldNumber, contact->number) != 0)
    {
        int16_t existingIndex = contact_service_find_by_number(contact->number);
        if (existingIndex >= 0 && existingIndex != index)
        {
            // Restaurer l'ancien numéro
            strncpy(contact_state.contacts[index].number, oldNumber, IDENTITY_PHONE_NUMBER_MAX - 1);
            CONTACT_DEBUG("Numéro déjà utilisé par un autre contact\n");
            return false;
        }
    }
    
    CONTACT_DEBUG("Contact %d mis à jour\n", index);
    
    if (changed_cb) changed_cb();
    
    return true;
}

bool contact_service_delete(uint16_t index)
{
    if (!contact_state.initialized) return false;
    if (index >= contact_state.contactCount) return false;
    
    // Supprimer des favoris si nécessaire
    if (contact_service_is_favorite(index))
    {
        contact_service_toggle_favorite(index);
    }
    
    // Supprimer des groupes
    for (uint8_t g = 0; g < contact_state.groupCount; g++)
    {
        contact_service_remove_from_group(index, g);
    }
    
    // Supprimer de la numérotation rapide
    for (uint8_t s = 0; s < contact_state.speedDialCount; s++)
    {
        if (contact_state.speedDials[s].contactIndex == index)
        {
            contact_state.speedDials[s].contactIndex = 0xFFFF;  // Invalide
        }
    }
    
    // Décaler les contacts suivants
    if (index < contact_state.contactCount - 1)
    {
        memmove(&contact_state.contacts[index], 
                &contact_state.contacts[index + 1],
                (contact_state.contactCount - index - 1) * sizeof(Contact));
    }
    contact_state.contactCount--;
    
    // Mettre à jour les indices dans les groupes
    update_group_indices_after_delete(index);
    
    // Mettre à jour les indices dans les favoris
    update_favorite_indices_after_delete(index);
    
    CONTACT_DEBUG("Contact %d supprimé\n", index);
    
    if (changed_cb) changed_cb();
    
    return true;
}

bool contact_service_duplicate(uint16_t index)
{
    if (!contact_state.initialized) return false;
    if (index >= contact_state.contactCount) return false;
    
    Contact* original = &contact_state.contacts[index];
    
    // Créer une copie avec "(copie)" dans le nom
    char newName[CONTACT_NAME_MAX_LENGTH];
    snprintf(newName, CONTACT_NAME_MAX_LENGTH, "%s (copie)", original->name);
    
    return contact_service_add(newName, original->number);
}

void contact_service_clear_all(void)
{
    contact_state.contactCount = 0;
    contact_state.favoriteCount = 0;
    contact_state.groupCount = 0;
    contact_state.speedDialCount = 0;
    
    memset(contact_state.contacts, 0, sizeof(contact_state.contacts));
    memset(contact_state.groups, 0, sizeof(contact_state.groups));
    memset(contact_state.favoriteIndices, 0, sizeof(contact_state.favoriteIndices));
    memset(contact_state.speedDials, 0, sizeof(contact_state.speedDials));
    
    if (changed_cb) changed_cb();
}

// ============================================================
// SECTION 3 : RECHERCHE
// ============================================================

int16_t contact_service_find_by_number(const char* number)
{
    if (number == NULL) return -1;
    
    for (uint16_t i = 0; i < contact_state.contactCount; i++)
    {
        if (strcmp(contact_state.contacts[i].number, number) == 0)
        {
            return i;
        }
    }
    return -1;
}

int16_t contact_service_find_by_name(const char* name)
{
    if (name == NULL) return -1;
    
    for (uint16_t i = 0; i < contact_state.contactCount; i++)
    {
        if (strcasecmp(contact_state.contacts[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

uint16_t contact_service_search(const char* query, uint16_t* results, uint16_t maxResults)
{
    if (query == NULL || results == NULL) return 0;
    
    uint16_t found = 0;
    
    for (uint16_t i = 0; i < contact_state.contactCount && found < maxResults; i++)
    {
        Contact* c = &contact_state.contacts[i];
        
        // Chercher dans le nom et le numéro
        if (strcasestr(c->name, query) != NULL || 
            strstr(c->number, query) != NULL)
        {
            results[found++] = i;
        }
    }
    
    return found;
}

Contact* contact_service_get(uint16_t index)
{
    if (index >= contact_state.contactCount) return NULL;
    return &contact_state.contacts[index];
}

uint16_t contact_service_get_count(void)
{
    return contact_state.contactCount;
}

Contact* contact_service_get_all(uint16_t* count)
{
    if (count) *count = contact_state.contactCount;
    return contact_state.contacts;
}

// ============================================================
// SECTION 4 : GROUPES
// ============================================================

bool contact_service_add_group(const char* name)
{
    if (name == NULL) return false;
    if (contact_state.groupCount >= CONTACT_MAX_GROUPS) return false;
    
    ContactGroup* group = &contact_state.groups[contact_state.groupCount++];
    memset(group, 0, sizeof(ContactGroup));
    strncpy(group->name, name, CONTACT_GROUP_MAX_LENGTH - 1);
    
    return true;
}

bool contact_service_delete_group(uint8_t groupIndex)
{
    if (groupIndex >= contact_state.groupCount) return false;
    
    if (groupIndex < contact_state.groupCount - 1)
    {
        memmove(&contact_state.groups[groupIndex], 
                &contact_state.groups[groupIndex + 1],
                (contact_state.groupCount - groupIndex - 1) * sizeof(ContactGroup));
    }
    contact_state.groupCount--;
    
    return true;
}

bool contact_service_add_to_group(uint16_t contactIndex, uint8_t groupIndex)
{
    if (contactIndex >= contact_state.contactCount) return false;
    if (groupIndex >= contact_state.groupCount) return false;
    
    ContactGroup* group = &contact_state.groups[groupIndex];
    
    if (group->contactCount >= CONTACT_MAX_PER_GROUP) return false;
    
    // Vérifier si déjà dans le groupe
    for (uint16_t i = 0; i < group->contactCount; i++)
    {
        if (group->contactIndices[i] == contactIndex) return true;  // Déjà présent
    }
    
    group->contactIndices[group->contactCount++] = contactIndex;
    
    return true;
}

bool contact_service_remove_from_group(uint16_t contactIndex, uint8_t groupIndex)
{
    if (groupIndex >= contact_state.groupCount) return false;
    
    ContactGroup* group = &contact_state.groups[groupIndex];
    
    for (uint16_t i = 0; i < group->contactCount; i++)
    {
        if (group->contactIndices[i] == contactIndex)
        {
            if (i < group->contactCount - 1)
            {
                memmove(&group->contactIndices[i], 
                        &group->contactIndices[i + 1],
                        (group->contactCount - i - 1) * sizeof(uint16_t));
            }
            group->contactCount--;
            return true;
        }
    }
    return false;
}

uint8_t contact_service_get_group_count(void)
{
    return contact_state.groupCount;
}

ContactGroup* contact_service_get_group(uint8_t index)
{
    if (index >= contact_state.groupCount) return NULL;
    return &contact_state.groups[index];
}

uint16_t contact_service_get_contacts_in_group(uint8_t groupIndex, uint16_t* indices, uint16_t maxCount)
{
    if (groupIndex >= contact_state.groupCount) return 0;
    
    ContactGroup* group = &contact_state.groups[groupIndex];
    uint16_t count = (group->contactCount < maxCount) ? group->contactCount : maxCount;
    
    memcpy(indices, group->contactIndices, count * sizeof(uint16_t));
    return count;
}

// ============================================================
// SECTION 5 : FAVORIS
// ============================================================

bool contact_service_toggle_favorite(uint16_t index)
{
    if (index >= contact_state.contactCount) return false;
    
    // Chercher si déjà favori
    for (uint8_t i = 0; i < contact_state.favoriteCount; i++)
    {
        if (contact_state.favoriteIndices[i] == index)
        {
            // Supprimer des favoris
            contact_state.contacts[index].favorite = false;
            
            if (i < contact_state.favoriteCount - 1)
            {
                memmove(&contact_state.favoriteIndices[i], 
                        &contact_state.favoriteIndices[i + 1],
                        (contact_state.favoriteCount - i - 1) * sizeof(uint16_t));
            }
            contact_state.favoriteCount--;
            return true;
        }
    }
    
    // Ajouter aux favoris
    if (contact_state.favoriteCount >= CONTACT_MAX_FAVORITES) return false;
    
    contact_state.contacts[index].favorite = true;
    contact_state.favoriteIndices[contact_state.favoriteCount++] = index;
    
    return true;
}

bool contact_service_is_favorite(uint16_t index)
{
    if (index >= contact_state.contactCount) return false;
    return contact_state.contacts[index].favorite;
}

uint8_t contact_service_get_favorite_count(void)
{
    return contact_state.favoriteCount;
}

uint16_t contact_service_get_favorites(uint16_t* indices, uint16_t maxCount)
{
    uint16_t count = (contact_state.favoriteCount < maxCount) ? 
                      contact_state.favoriteCount : maxCount;
    memcpy(indices, contact_state.favoriteIndices, count * sizeof(uint16_t));
    return count;
}

// ============================================================
// SECTION 6 : NUMÉROTATION RAPIDE
// ============================================================

bool contact_service_set_speed_dial(uint8_t key, uint16_t contactIndex)
{
    if (key < 1 || key > 9) return false;
    if (contactIndex >= contact_state.contactCount) return false;
    
    // Chercher si la touche est déjà assignée
    for (uint8_t i = 0; i < contact_state.speedDialCount; i++)
    {
        if (contact_state.speedDials[i].index == key)
        {
            contact_state.speedDials[i].contactIndex = contactIndex;
            return true;
        }
    }
    
    // Ajouter une nouvelle entrée
    if (contact_state.speedDialCount >= CONTACT_MAX_SPEED_DIAL) return false;
    
    contact_state.speedDials[contact_state.speedDialCount].index = key;
    contact_state.speedDials[contact_state.speedDialCount].contactIndex = contactIndex;
    contact_state.speedDialCount++;
    
    return true;
}

bool contact_service_remove_speed_dial(uint8_t key)
{
    for (uint8_t i = 0; i < contact_state.speedDialCount; i++)
    {
        if (contact_state.speedDials[i].index == key)
        {
            if (i < contact_state.speedDialCount - 1)
            {
                memmove(&contact_state.speedDials[i], 
                        &contact_state.speedDials[i + 1],
                        (contact_state.speedDialCount - i - 1) * sizeof(SpeedDial));
            }
            contact_state.speedDialCount--;
            return true;
        }
    }
    return false;
}

int16_t contact_service_get_speed_dial(uint8_t key)
{
    for (uint8_t i = 0; i < contact_state.speedDialCount; i++)
    {
        if (contact_state.speedDials[i].index == key)
        {
            uint16_t idx = contact_state.speedDials[i].contactIndex;
            if (idx < contact_state.contactCount) return idx;
        }
    }
    return -1;
}

// ============================================================
// SECTION 7 : TRI ET FILTRAGE
// ============================================================

void contact_service_sort_by_name(bool ascending)
{
    contact_state.sortByName = true;
    contact_state.sortAscending = ascending;
    
    // Tri à bulles
    for (uint16_t i = 0; i < contact_state.contactCount - 1; i++)
    {
        for (uint16_t j = 0; j < contact_state.contactCount - i - 1; j++)
        {
            int cmp = strcasecmp(contact_state.contacts[j].name, 
                                 contact_state.contacts[j + 1].name);
            
            if ((ascending && cmp > 0) || (!ascending && cmp < 0))
            {
                Contact temp = contact_state.contacts[j];
                contact_state.contacts[j] = contact_state.contacts[j + 1];
                contact_state.contacts[j + 1] = temp;
            }
        }
    }
}

void contact_service_sort_by_number(bool ascending)
{
    contact_state.sortByName = false;
    contact_state.sortAscending = ascending;
    
    for (uint16_t i = 0; i < contact_state.contactCount - 1; i++)
    {
        for (uint16_t j = 0; j < contact_state.contactCount - i - 1; j++)
        {
            int cmp = strcmp(contact_state.contacts[j].number, 
                            contact_state.contacts[j + 1].number);
            
            if ((ascending && cmp > 0) || (!ascending && cmp < 0))
            {
                Contact temp = contact_state.contacts[j];
                contact_state.contacts[j] = contact_state.contacts[j + 1];
                contact_state.contacts[j + 1] = temp;
            }
        }
    }
}

void contact_service_sort_by_last_call(void)
{
    // Tri par ordre décroissant de dernier appel
    for (uint16_t i = 0; i < contact_state.contactCount - 1; i++)
    {
        for (uint16_t j = 0; j < contact_state.contactCount - i - 1; j++)
        {
            if (contact_state.contacts[j].lastCallTime < 
                contact_state.contacts[j + 1].lastCallTime)
            {
                Contact temp = contact_state.contacts[j];
                contact_state.contacts[j] = contact_state.contacts[j + 1];
                contact_state.contacts[j + 1] = temp;
            }
        }
    }
}

void contact_service_sort_by_frequency(void)
{
    // Tri par nombre d'appels décroissant
    for (uint16_t i = 0; i < contact_state.contactCount - 1; i++)
    {
        for (uint16_t j = 0; j < contact_state.contactCount - i - 1; j++)
        {
            if (contact_state.contacts[j].callCount < 
                contact_state.contacts[j + 1].callCount)
            {
                Contact temp = contact_state.contacts[j];
                contact_state.contacts[j] = contact_state.contacts[j + 1];
                contact_state.contacts[j + 1] = temp;
            }
        }
    }
}

void contact_service_filter_by_group(const char* groupName)
{
    if (groupName)
        strncpy(contact_state.filterGroup, groupName, CONTACT_GROUP_MAX_LENGTH - 1);
    else
        contact_state.filterGroup[0] = '\0';
}

void contact_service_filter_favorites(bool onlyFavorites)
{
    contact_state.showFavoritesOnly = onlyFavorites;
}

void contact_service_clear_filters(void)
{
    contact_state.filterGroup[0] = '\0';
    contact_state.showFavoritesOnly = false;
}

// ============================================================
// SECTION 8 : IMPORT/EXPORT
// ============================================================

bool contact_service_export_to_flash(void)
{
    FlashEEPROM_Error err = flash_eeprom_write(EEPROM_ID_CONTACTS,
                                                (uint8_t*)&contact_state,
                                                sizeof(ContactServiceState));
    return (err == FLASH_EEPROM_OK);
}

bool contact_service_import_from_flash(void)
{
    uint16_t readSize;
    FlashEEPROM_Error err = flash_eeprom_read(EEPROM_ID_CONTACTS,
                                               (uint8_t*)&contact_state,
                                               sizeof(ContactServiceState),
                                               &readSize);
    return (err == FLASH_EEPROM_OK && readSize >= sizeof(ContactServiceState));
}

uint16_t contact_service_export_to_buffer(char* buffer, uint16_t bufferSize)
{
    if (buffer == NULL) return 0;
    // Format binaire simple
    uint16_t size = sizeof(ContactServiceState);
    if (size > bufferSize) size = bufferSize;
    memcpy(buffer, &contact_state, size);
    return size;
}

bool contact_service_import_from_buffer(const char* buffer, uint16_t bufferSize)
{
    if (buffer == NULL) return false;
    if (bufferSize < sizeof(ContactServiceState)) return false;
    memcpy(&contact_state, buffer, sizeof(ContactServiceState));
    return true;
}

uint16_t contact_service_export_to_csv(char* buffer, uint16_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0) return 0;
    
    uint16_t pos = 0;
    pos += snprintf(buffer + pos, bufferSize - pos, "Nom,Numéro,Email,Groupe,Favori\n");
    
    for (uint16_t i = 0; i < contact_state.contactCount; i++)
    {
        Contact* c = &contact_state.contacts[i];
        pos += snprintf(buffer + pos, bufferSize - pos, "%s,%s,%s,%s,%s\n",
                       c->name, c->number, c->email, c->group,
                       c->favorite ? "Oui" : "Non");
        
        if (pos >= bufferSize - 1) break;
    }
    
    return pos;
}

bool contact_service_import_from_csv(const char* buffer)
{
    // Implémentation simplifiée
    (void)buffer;
    return false;  // À implémenter complètement si nécessaire
}

// ============================================================
// SECTION 9 : STATISTIQUES
// ============================================================

void contact_service_increment_call_count(uint16_t index)
{
    if (index >= contact_state.contactCount) return;
    contact_state.contacts[index].callCount++;
    contact_state.contacts[index].lastCallTime = HAL_GetTick();
    contact_state.totalCalls++;
}

void contact_service_increment_sms_count(uint16_t index)
{
    if (index >= contact_state.contactCount) return;
    contact_state.contacts[index].smsCount++;
    contact_state.contacts[index].lastSmsTime = HAL_GetTick();
    contact_state.totalSMS++;
}

void contact_service_update_last_call(uint16_t index)
{
    if (index >= contact_state.contactCount) return;
    contact_state.contacts[index].lastCallTime = HAL_GetTick();
}

// ============================================================
// SECTION 10 : CALLBACKS
// ============================================================

void contact_service_set_changed_callback(ContactService_ChangedCallback cb) { changed_cb = cb; }
void contact_service_set_contact_callback(ContactService_ContactCallback cb) { contact_cb = cb; }

// ============================================================
// SECTION 11 : DÉBOGAGE
// ============================================================

void contact_service_print_all(void)
{
    printf("\n═══ CONTACTS (%d) ═══\n", contact_state.contactCount);
    printf("%-4s %-20s %-16s %-10s %-8s %-6s\n", 
           "Idx", "Nom", "Numéro", "Groupe", "Favori", "Appels");
    printf("──────────────────────────────────────────────────────────────\n");
    
    for (uint16_t i = 0; i < contact_state.contactCount; i++)
    {
        Contact* c = &contact_state.contacts[i];
        printf("%-4d %-20s %-16s %-10s %-8s %-6lu\n",
               i, c->name, c->number, c->group,
               c->favorite ? "⭐" : "  ",
               (unsigned long)c->callCount);
    }
    printf("══════════════════════════════════════════════\n\n");
}

void contact_service_print_contact(uint16_t index)
{
    Contact* c = contact_service_get(index);
    if (c == NULL) return;
    
    printf("\n═══ CONTACT %d ═══\n", index);
    printf("Nom       : %s\n", c->name);
    printf("Numéro    : %s\n", c->number);
    printf("Email     : %s\n", c->email);
    printf("Groupe    : %s\n", c->group);
    printf("Favori    : %s\n", c->favorite ? "Oui" : "Non");
    printf("Appels    : %lu\n", (unsigned long)c->callCount);
    printf("SMS       : %lu\n", (unsigned long)c->smsCount);
    printf("══════════════════\n\n");
}

void contact_service_print_groups(void)
{
    printf("\n═══ GROUPES (%d) ═══\n", contact_state.groupCount);
    
    for (uint8_t i = 0; i < contact_state.groupCount; i++)
    {
        ContactGroup* g = &contact_state.groups[i];
        printf("[%d] %s (%d contacts)\n", i, g->name, g->contactCount);
    }
    printf("══════════════════════\n\n");
}

void contact_service_print_favorites(void)
{
    printf("\n═══ FAVORIS (%d) ═══\n", contact_state.favoriteCount);
    
    for (uint8_t i = 0; i < contact_state.favoriteCount; i++)
    {
        uint16_t idx = contact_state.favoriteIndices[i];
        Contact* c = &contact_state.contacts[idx];
        printf("[%d] %s : %s\n", i + 1, c->name, c->number);
    }
    printf("══════════════════\n\n");
}

void contact_service_print_speed_dials(void)
{
    printf("\n═══ NUMÉROS RAPIDES ═══\n");
    
    for (uint8_t key = 1; key <= 9; key++)
    {
        int16_t idx = contact_service_get_speed_dial(key);
        if (idx >= 0)
        {
            printf("  Touche %d : %s (%s)\n", key, 
                   contact_state.contacts[idx].name,
                   contact_state.contacts[idx].number);
        }
    }
    printf("══════════════════════════\n\n");
}

void contact_service_print_statistics(void)
{
    printf("\n═══ STATISTIQUES CONTACTS ═══\n");
    printf("Total contacts : %d\n", contact_state.contactCount);
    printf("Total groupes  : %d\n", contact_state.groupCount);
    printf("Favoris        : %d\n", contact_state.favoriteCount);
    printf("Num. rapides   : %d\n", contact_state.speedDialCount);
    printf("Total appels   : %lu\n", (unsigned long)contact_state.totalCalls);
    printf("Total SMS      : %lu\n", (unsigned long)contact_state.totalSMS);
    printf("══════════════════════════════\n\n");
}

bool contact_service_self_test(void)
{
    CONTACT_DEBUG("Auto-test...\n");
    
    if (!contact_state.initialized)
    {
        CONTACT_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : ajouter un contact
    contact_service_add("Test Contact", "0600000000");
    int16_t idx = contact_service_find_by_number("0600000000");
    
    if (idx < 0)
    {
        CONTACT_DEBUG("Échec : contact non trouvé\n");
        return false;
    }
    
    // Test : favori
    contact_service_toggle_favorite(idx);
    if (!contact_service_is_favorite(idx))
    {
        CONTACT_DEBUG("Échec : favori non activé\n");
        return false;
    }
    
    // Nettoyer
    contact_service_delete(idx);
    
    CONTACT_DEBUG("Auto-test OK\n");
    return true;
}

// ============================================================
// SECTION 12 : FONCTIONS INTERNES
// ============================================================

static void update_group_indices_after_delete(uint16_t deletedIndex)
{
    for (uint8_t g = 0; g < contact_state.groupCount; g++)
    {
        ContactGroup* group = &contact_state.groups[g];
        
        for (uint16_t i = 0; i < group->contactCount; i++)
        {
            if (group->contactIndices[i] > deletedIndex)
            {
                group->contactIndices[i]--;  // Décrémenter les indices supérieurs
            }
            else if (group->contactIndices[i] == deletedIndex)
            {
                // Supprimer cette entrée
                if (i < group->contactCount - 1)
                {
                    memmove(&group->contactIndices[i], 
                            &group->contactIndices[i + 1],
                            (group->contactCount - i - 1) * sizeof(uint16_t));
                }
                group->contactCount--;
                i--;  // Re-vérifier cette position
            }
        }
    }
}

static void update_favorite_indices_after_delete(uint16_t deletedIndex)
{
    for (uint8_t i = 0; i < contact_state.favoriteCount; i++)
    {
        if (contact_state.favoriteIndices[i] > deletedIndex)
        {
            contact_state.favoriteIndices[i]--;
        }
    }
}