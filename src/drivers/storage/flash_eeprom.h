/**
 * @file flash_eeprom.h
 * @brief Émulation EEPROM dans la mémoire Flash du STM32F429
 * 
 * Ce fichier gère le stockage persistant de données dans la
 * mémoire Flash interne, en émulant le comportement d'une EEPROM.
 * 
 * Caractéristiques :
 * - Utilise les secteurs Flash 11 et 12 (2 × 128 Ko)
 * - Wear leveling (répartition de l'usure)
 * - Double buffer pour la sécurité des données
 * - CRC32 pour l'intégrité
 * - Format des données versionné
 * 
 * Architecture mémoire Flash (STM32F429 2 Mo) :
 * ┌─────────────────────────────────────────────────────────────┐
 * │ Secteur 0-11  : Application (0x08000000 - 0x080E0000)     │
 * │ Secteur 11    : EEPROM Buffer A (0x080E0000, 128 Ko)      │
 * │ Secteur 12    : EEPROM Buffer B (0x080C0000, 128 Ko)      │
 * │ Secteur 23    : Fin de la Flash (2 Mo)                    │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * Format d'une page de données :
 * ┌──────────┬──────────┬──────────┬──────────┬──────────────┐
 * │ Magic    │ Version  │ ID       │ Size     │ Data + CRC   │
 * │ 4 octets │ 2 octets │ 2 octets │ 2 octets │ N + 4 octets │
 * └──────────┴──────────┴──────────┴──────────┴──────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef FLASH_EEPROM_H
#define FLASH_EEPROM_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "../../config.h"
#include "../../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define FLASH_EEPROM_VERSION            "1.0.0"

/** @brief Signature magique */
#define FLASH_EEPROM_MAGIC              0x45455052  // "EEPROM" en little-endian

/** @brief Version du format de données */
#define FLASH_EEPROM_DATA_VERSION       1

/** @brief Taille d'un secteur Flash (128 Ko) */
#define FLASH_EEPROM_SECTOR_SIZE        0x20000     // 131072 octets

/** @brief Taille d'une page de données */
#define FLASH_EEPROM_PAGE_SIZE          64          // 64 octets

/** @brief Nombre maximum d'entrées */
#define FLASH_EEPROM_MAX_ENTRIES        64

/** @brief Nombre maximum de variables */
#define FLASH_EEPROM_MAX_VARIABLES      128

/** @brief Adresse de base du buffer A */
#define FLASH_EEPROM_BUFFER_A_ADDR      0x080E0000  // Secteur 11

/** @brief Adresse de base du buffer B */
#define FLASH_EEPROM_BUFFER_B_ADDR      0x080C0000  // Secteur 12

// ============================================================
// SECTION 2 : IDENTIFIANTS DE VARIABLES
// ============================================================

/**
 * @brief Identifiants des variables sauvegardées
 * 
 * Chaque variable a un ID unique pour l'identification.
 */
typedef enum {
    EEPROM_ID_SETTINGS          = 0x0001,   // Paramètres généraux
    EEPROM_ID_CONTACTS          = 0x0002,   // Contacts
    EEPROM_ID_CALL_LOG          = 0x0003,   // Journal d'appels
    EEPROM_ID_SMS               = 0x0004,   // Messages SMS
    EEPROM_ID_CALIBRATION       = 0x0005,   // Calibration tactile
    EEPROM_ID_PHONE_NUMBER      = 0x0006,   // Numéro de téléphone
    EEPROM_ID_DEVICE_NAME       = 0x0007,   // Nom du dispositif
    EEPROM_ID_LORA_CONFIG       = 0x0008,   // Configuration LoRa
    EEPROM_ID_AUDIO_CONFIG      = 0x0009,   // Configuration audio
    EEPROM_ID_DISPLAY_CONFIG    = 0x000A,   // Configuration affichage
    EEPROM_ID_KEYPAD_CONFIG     = 0x000B,   // Configuration clavier
    EEPROM_ID_POWER_CONFIG      = 0x000C,   // Configuration alimentation
    EEPROM_ID_NETWORK_CONFIG    = 0x000D,   // Configuration réseau
    EEPROM_ID_SECURITY_CONFIG   = 0x000E,   // Configuration sécurité
    EEPROM_ID_USER_DATA_1       = 0x0100,   // Données utilisateur 1
    EEPROM_ID_USER_DATA_2       = 0x0101,   // Données utilisateur 2
    EEPROM_ID_USER_DATA_3       = 0x0102    // Données utilisateur 3
} EEPROM_VariableID;

// ============================================================
// SECTION 3 : CODES D'ERREUR
// ============================================================

/**
 * @brief Codes d'erreur du module Flash EEPROM
 */
typedef enum {
    FLASH_EEPROM_OK             = 0,    // Succès
    FLASH_EEPROM_ERROR_INIT     = -1,   // Échec initialisation
    FLASH_EEPROM_ERROR_ERASE    = -2,   // Échec effacement
    FLASH_EEPROM_ERROR_WRITE    = -3,   // Échec écriture
    FLASH_EEPROM_ERROR_READ     = -4,   // Échec lecture
    FLASH_EEPROM_ERROR_FULL     = -5,   // Mémoire pleine
    FLASH_EEPROM_ERROR_NOT_FOUND = -6,  // Variable non trouvée
    FLASH_EEPROM_ERROR_CRC      = -7,   // Erreur CRC
    FLASH_EEPROM_ERROR_SIZE     = -8,   // Taille dépassée
    FLASH_EEPROM_ERROR_LOCKED   = -9    // Flash verrouillée
} FlashEEPROM_Error;

// ============================================================
// SECTION 4 : STRUCTURES DE DONNÉES
// ============================================================

/**
 * @brief En-tête d'une entrée dans la Flash
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;                 // Signature magique (0x45455052)
    uint16_t version;               // Version du format
    uint16_t variableId;            // Identifiant de la variable
    uint16_t dataSize;              // Taille des données
    uint16_t flags;                 // Flags (réservé)
    uint32_t crc;                   // CRC32 de l'entrée
} FlashEEPROM_Header;

/**
 * @brief Entrée complète (en-tête + données)
 */
typedef struct __attribute__((packed)) {
    FlashEEPROM_Header header;      // En-tête
    uint8_t data[FLASH_EEPROM_PAGE_SIZE - sizeof(FlashEEPROM_Header) - 4];  // Données
    uint32_t dataCrc;               // CRC32 des données
} FlashEEPROM_Entry;

/**
 * @brief État du module
 */
typedef struct {
    bool initialized;               // Module initialisé
    bool locked;                    // Flash verrouillée
    uint32_t activeBuffer;          // Buffer actif (A ou B)
    uint32_t currentOffset;         // Offset actuel dans le buffer
    uint32_t totalWrites;           // Nombre total d'écritures
    uint32_t totalErases;           // Nombre total d'effacements
    uint32_t availableSpace;        // Espace disponible
    uint32_t usedSpace;             // Espace utilisé
} FlashEEPROM_State;

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise le module Flash EEPROM
 * @return Code d'erreur
 */
FlashEEPROM_Error flash_eeprom_init(void);

/**
 * @brief Désinitialise le module
 */
void flash_eeprom_deinit(void);

/**
 * @brief Vérifie si le module est prêt
 */
bool flash_eeprom_is_ready(void);

/**
 * @brief Récupère l'état du module
 */
FlashEEPROM_State* flash_eeprom_get_state(void);

// ============================================================
// SECTION 6 : FONCTIONS D'ÉCRITURE
// ============================================================

/**
 * @brief Écrit une variable dans la Flash
 * @param variableId Identifiant de la variable
 * @param data Pointeur vers les données
 * @param size Taille des données (max 48 octets)
 * @return Code d'erreur
 */
FlashEEPROM_Error flash_eeprom_write(uint16_t variableId, const uint8_t* data, uint16_t size);

/**
 * @brief Écrit un entier 8 bits
 */
FlashEEPROM_Error flash_eeprom_write_u8(uint16_t variableId, uint8_t value);

/**
 * @brief Écrit un entier 16 bits
 */
FlashEEPROM_Error flash_eeprom_write_u16(uint16_t variableId, uint16_t value);

/**
 * @brief Écrit un entier 32 bits
 */
FlashEEPROM_Error flash_eeprom_write_u32(uint16_t variableId, uint32_t value);

/**
 * @brief Écrit une chaîne de caractères
 */
FlashEEPROM_Error flash_eeprom_write_string(uint16_t variableId, const char* str);

/**
 * @brief Écrit un tableau d'octets
 */
FlashEEPROM_Error flash_eeprom_write_buffer(uint16_t variableId, const uint8_t* buffer, uint16_t size);

// ============================================================
// SECTION 7 : FONCTIONS DE LECTURE
// ============================================================

/**
 * @brief Lit une variable depuis la Flash
 * @param variableId Identifiant de la variable
 * @param data Buffer de réception
 * @param size Taille du buffer
 * @param readSize Taille lue (sortie)
 * @return Code d'erreur
 */
FlashEEPROM_Error flash_eeprom_read(uint16_t variableId, uint8_t* data, uint16_t size, uint16_t* readSize);

/**
 * @brief Lit un entier 8 bits
 */
FlashEEPROM_Error flash_eeprom_read_u8(uint16_t variableId, uint8_t* value);

/**
 * @brief Lit un entier 16 bits
 */
FlashEEPROM_Error flash_eeprom_read_u16(uint16_t variableId, uint16_t* value);

/**
 * @brief Lit un entier 32 bits
 */
FlashEEPROM_Error flash_eeprom_read_u32(uint16_t variableId, uint32_t* value);

/**
 * @brief Lit une chaîne de caractères
 */
FlashEEPROM_Error flash_eeprom_read_string(uint16_t variableId, char* buffer, uint16_t bufferSize);

/**
 * @brief Vérifie si une variable existe
 */
bool flash_eeprom_exists(uint16_t variableId);

/**
 * @brief Récupère la taille d'une variable
 */
uint16_t flash_eeprom_get_size(uint16_t variableId);

// ============================================================
// SECTION 8 : FONCTIONS DE GESTION
// ============================================================

/**
 * @brief Efface une variable
 * @param variableId Identifiant de la variable
 * @return Code d'erreur
 */
FlashEEPROM_Error flash_eeprom_erase(uint16_t variableId);

/**
 * @brief Efface toutes les données (formatage)
 * @return Code d'erreur
 */
FlashEEPROM_Error flash_eeprom_format(void);

/**
 * @brief Défragmente la mémoire (compacte les données valides)
 * @return Code d'erreur
 */
FlashEEPROM_Error flash_eeprom_defragment(void);

/**
 * @brief Vérifie l'intégrité des données
 * @return true si toutes les données sont valides
 */
bool flash_eeprom_verify(void);

/**
 * @brief Récupère l'espace disponible
 * @return Octets disponibles
 */
uint32_t flash_eeprom_get_free_space(void);

// ============================================================
// SECTION 9 : FONCTIONS DE CRC
// ============================================================

uint32_t flash_eeprom_crc32(const uint8_t* data, uint16_t size);
bool flash_eeprom_verify_crc(const uint8_t* data, uint16_t size, uint32_t expectedCrc);

// ============================================================
// SECTION 10 : FONCTIONS DE DÉBOGAGE
// ============================================================

void flash_eeprom_print_state(void);
void flash_eeprom_print_entry(uint16_t variableId);
void flash_eeprom_dump(uint16_t startOffset, uint16_t length);
bool flash_eeprom_self_test(void);

// ============================================================
// SECTION 11 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define FLASH_EEPROM_DEBUG(fmt, ...) printf("[FLASH_EEPROM] " fmt, ##__VA_ARGS__)
#else
    #define FLASH_EEPROM_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 12 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // FLASH_EEPROM_H