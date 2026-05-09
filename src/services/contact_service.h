/**
 * @file contact_service.h
 * @brief Service de gestion des contacts
 * 
 * Ce fichier implémente le service de gestion du carnet d'adresses :
 * - Ajout, modification, suppression de contacts
 * - Recherche par nom, numéro
 * - Groupes de contacts
 * - Favoris
 * - Import/Export
 * - Numérotation rapide
 * - Historique des appels par contact
 * 
 * Structure d'un contact :
 * ┌──────────┬──────────┬──────────┬──────────┬──────────────┐
 * │ Nom      │ Numéro   │ Groupe   │ Favori   │ Sonnerie     │
 * │ 32 chars │ 16 chars │ 16 chars │ bool     │ uint8_t      │
 * └──────────┴──────────┴──────────┴──────────┴──────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef CONTACT_SERVICE_H
#define CONTACT_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "../protocols/identity.h"
#include "../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du service */
#define CONTACT_SERVICE_VERSION         "1.0.0"

/** @brief Nombre maximum de contacts */
#define CONTACT_MAX_COUNT               200

/** @brief Nombre maximum de groupes */
#define CONTACT_MAX_GROUPS              20

/** @brief Nombre maximum de contacts par groupe */
#define CONTACT_MAX_PER_GROUP           50

/** @brief Nombre maximum de favoris */
#define CONTACT_MAX_FAVORITES           20

/** @brief Nombre maximum de numéros rapides */
#define CONTACT_MAX_SPEED_DIAL          9

/** @brief Longueur maximale du nom */
#define CONTACT_NAME_MAX_LENGTH         32

/** @brief Longueur maximale du groupe */
#define CONTACT_GROUP_MAX_LENGTH        16

/** @brief Longueur maximale de l'email */
#define CONTACT_EMAIL_MAX_LENGTH        48

/** @brief Longueur maximale de l'adresse */
#define CONTACT_ADDRESS_MAX_LENGTH      64

/** @brief Longueur maximale des notes */
#define CONTACT_NOTES_MAX_LENGTH        128

// ============================================================
// SECTION 2 : TYPES DE DONNÉES
// ============================================================

/**
 * @brief Structure d'un contact
 */
typedef struct {
    // Informations principales
    char name[CONTACT_NAME_MAX_LENGTH];         // Nom
    char number[IDENTITY_PHONE_NUMBER_MAX];     // Numéro de téléphone
    
    // Informations supplémentaires
    char email[CONTACT_EMAIL_MAX_LENGTH];       // Email
    char address[CONTACT_ADDRESS_MAX_LENGTH];   // Adresse
    char notes[CONTACT_NOTES_MAX_LENGTH];       // Notes
    char group[CONTACT_GROUP_MAX_LENGTH];       // Groupe
    
    // Préférences
    bool favorite;                              // Favori ?
    uint8_t ringtoneIndex;                      // Sonnerie personnalisée (0=défaut)
    uint8_t speedDialIndex;                     // Numéro rapide (0=pas défini, 1-9)
    bool vibrationEnabled;                      // Vibreur pour ce contact
    
    // Photo (index dans la table des photos)
    uint8_t photoIndex;                         // Index de la photo (0=pas de photo)
    
    // Statistiques
    uint32_t callCount;                         // Nombre d'appels
    uint32_t totalCallDuration;                 // Durée totale des appels
    uint32_t lastCallTime;                      // Dernier appel
    uint32_t smsCount;                          // Nombre de SMS
    uint32_t lastSmsTime;                       // Dernier SMS
    
    // Métadonnées
    uint32_t creationTime;                      // Date de création
    uint32_t modificationTime;                  // Dernière modification
    bool blocked;                               // Contact bloqué ?
    
} Contact;

/**
 * @brief Groupe de contacts
 */
typedef struct {
    char name[CONTACT_GROUP_MAX_LENGTH];        // Nom du groupe
    uint16_t contactIndices[CONTACT_MAX_PER_GROUP]; // Indices des contacts
    uint16_t contactCount;                      // Nombre de contacts
    uint8_t ringtoneIndex;                      // Sonnerie du groupe
} ContactGroup;

/**
 * @brief Numéro rapide
 */
typedef struct {
    uint8_t index;                              // Touche (1-9)
    uint16_t contactIndex;                      // Index du contact associé
} SpeedDial;

// ============================================================
// SECTION 3 : ÉTAT DU SERVICE
// ============================================================

/**
 * @brief État du service de contacts
 */
typedef struct {
    bool initialized;                           // Service initialisé
    
    // Contacts
    Contact contacts[CONTACT_MAX_COUNT];
    uint16_t contactCount;
    
    // Groupes
    ContactGroup groups[CONTACT_MAX_GROUPS];
    uint8_t groupCount;
    
    // Favoris
    uint16_t favoriteIndices[CONTACT_MAX_FAVORITES];
    uint8_t favoriteCount;
    
    // Numéros rapides
    SpeedDial speedDials[CONTACT_MAX_SPEED_DIAL];
    uint8_t speedDialCount;
    
    // Statistiques
    uint32_t totalCalls;
    uint32_t totalSMS;
    
    // Tri
    bool sortByName;                            // Trier par nom (sinon par numéro)
    bool sortAscending;                         // Ordre croissant
    
    // Filtre
    char filterGroup[CONTACT_GROUP_MAX_LENGTH]; // Groupe filtré (vide = tous)
    bool showFavoritesOnly;                     // Afficher seulement les favoris
    
} ContactServiceState;

// ============================================================
// SECTION 4 : CALLBACKS
// ============================================================

typedef void (*ContactService_ChangedCallback)(void);
typedef void (*ContactService_ContactCallback)(uint16_t index);

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

bool contact_service_init(void);
void contact_service_deinit(void);
bool contact_service_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS CRUD
// ============================================================

bool contact_service_add(const char* name, const char* number);
bool contact_service_update(uint16_t index, const Contact* contact);
bool contact_service_delete(uint16_t index);
bool contact_service_duplicate(uint16_t index);
void contact_service_clear_all(void);

// ============================================================
// SECTION 7 : FONCTIONS DE RECHERCHE
// ============================================================

int16_t contact_service_find_by_number(const char* number);
int16_t contact_service_find_by_name(const char* name);
uint16_t contact_service_search(const char* query, uint16_t* results, uint16_t maxResults);
Contact* contact_service_get(uint16_t index);
uint16_t contact_service_get_count(void);
Contact* contact_service_get_all(uint16_t* count);

// ============================================================
// SECTION 8 : FONCTIONS DE GROUPES
// ============================================================

bool contact_service_add_group(const char* name);
bool contact_service_delete_group(uint8_t groupIndex);
bool contact_service_add_to_group(uint16_t contactIndex, uint8_t groupIndex);
bool contact_service_remove_from_group(uint16_t contactIndex, uint8_t groupIndex);
uint8_t contact_service_get_group_count(void);
ContactGroup* contact_service_get_group(uint8_t index);
uint16_t contact_service_get_contacts_in_group(uint8_t groupIndex, uint16_t* indices, uint16_t maxCount);

// ============================================================
// SECTION 9 : FONCTIONS DE FAVORIS
// ============================================================

bool contact_service_toggle_favorite(uint16_t index);
bool contact_service_is_favorite(uint16_t index);
uint8_t contact_service_get_favorite_count(void);
uint16_t contact_service_get_favorites(uint16_t* indices, uint16_t maxCount);

// ============================================================
// SECTION 10 : FONCTIONS DE NUMÉROTATION RAPIDE
// ============================================================

bool contact_service_set_speed_dial(uint8_t key, uint16_t contactIndex);
bool contact_service_remove_speed_dial(uint8_t key);
int16_t contact_service_get_speed_dial(uint8_t key);

// ============================================================
// SECTION 11 : FONCTIONS DE TRI ET FILTRAGE
// ============================================================

void contact_service_sort_by_name(bool ascending);
void contact_service_sort_by_number(bool ascending);
void contact_service_sort_by_last_call(void);
void contact_service_sort_by_frequency(void);
void contact_service_filter_by_group(const char* groupName);
void contact_service_filter_favorites(bool onlyFavorites);
void contact_service_clear_filters(void);

// ============================================================
// SECTION 12 : FONCTIONS D'IMPORT/EXPORT
// ============================================================

bool contact_service_export_to_flash(void);
bool contact_service_import_from_flash(void);
uint16_t contact_service_export_to_buffer(char* buffer, uint16_t bufferSize);
bool contact_service_import_from_buffer(const char* buffer, uint16_t bufferSize);
uint16_t contact_service_export_to_csv(char* buffer, uint16_t bufferSize);
bool contact_service_import_from_csv(const char* buffer);

// ============================================================
// SECTION 13 : FONCTIONS DE STATISTIQUES
// ============================================================

void contact_service_increment_call_count(uint16_t index);
void contact_service_increment_sms_count(uint16_t index);
void contact_service_update_last_call(uint16_t index);
Contact* contact_service_get_most_called(uint8_t count);
Contact* contact_service_get_recent(uint8_t count);

// ============================================================
// SECTION 14 : FONCTIONS DE CALLBACKS
// ============================================================

void contact_service_set_changed_callback(ContactService_ChangedCallback callback);
void contact_service_set_contact_callback(ContactService_ContactCallback callback);

// ============================================================
// SECTION 15 : FONCTIONS DE DÉBOGAGE
// ============================================================

void contact_service_print_all(void);
void contact_service_print_contact(uint16_t index);
void contact_service_print_groups(void);
void contact_service_print_favorites(void);
void contact_service_print_speed_dials(void);
void contact_service_print_statistics(void);
bool contact_service_self_test(void);

// ============================================================
// SECTION 16 : MACROS UTILITAIRES
// ============================================================

#define CONTACT_GET_COUNT()             contact_service_get_count()
#define CONTACT_GET(index)              contact_service_get(index)
#define CONTACT_FIND_BY_NUMBER(num)     contact_service_find_by_number(num)
#define CONTACT_IS_FAVORITE(idx)        contact_service_is_favorite(idx)

// ============================================================
// SECTION 17 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define CONTACT_DEBUG(fmt, ...)     printf("[CONTACT] " fmt, ##__VA_ARGS__)
#else
    #define CONTACT_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 18 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // CONTACT_SERVICE_H