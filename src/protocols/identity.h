/**
 * @file identity.h
 * @brief Gestion de l'identité du dispositif sur le réseau LoRa
 * 
 * Ce fichier définit l'identité unique de chaque téléphone :
 * - UID matériel (issu du microcontrôleur)
 * - Numéro de téléphone (MSISDN-like)
 * - Identifiant réseau (IMSI-like)
 * - Adresse MAC-like pour la couche liaison
 * - Nom du dispositif
 * - Capacités du dispositif
 * 
 * Format des identifiants :
 * - UID : 32 bits (0xXXXXXXXX)
 * - IMSI : 64 bits (20801XXXXXXXXXX) - MCC+MNC+MSIN
 * - MSISDN : 15 chiffres max (06XXXXXXXX)
 * - MAC : 6 octets (AA:BB:CC:DD:EE:FF)
 * - TMSI : 32 bits (identifiant temporaire)
 * 
 * Ces identifiants sont utilisés pour :
 * - L'enregistrement sur le réseau
 * - L'authentification
 * - Le routage des appels et messages
 * - La découverte des pairs
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef IDENTITY_H
#define IDENTITY_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define IDENTITY_VERSION                "1.0.0"

/** @brief Longueur maximale d'un numéro de téléphone */
#define IDENTITY_PHONE_NUMBER_MAX       16

/** @brief Longueur maximale d'un nom de dispositif */
#define IDENTITY_DEVICE_NAME_MAX        16

/** @brief Longueur d'une adresse MAC */
#define IDENTITY_MAC_SIZE               6

/** @brief Longueur d'un IMSI (15 chiffres max) */
#define IDENTITY_IMSI_SIZE              16

/** @brief Préfixe MCC+MNC par défaut (20801 = France) */
#define IDENTITY_DEFAULT_MCCMNC         "20801"

/** @brief Préfixe MSISDN par défaut */
#define IDENTITY_DEFAULT_MSISDN_PREFIX  "06"

// ============================================================
// SECTION 2 : TYPES D'IDENTIFIANTS
// ============================================================

/**
 * @brief Types d'identifiants supportés
 */
typedef enum {
    IDENTITY_TYPE_UID       = 0,    // Identifiant matériel unique
    IDENTITY_TYPE_IMSI      = 1,    // International Mobile Subscriber Identity
    IDENTITY_TYPE_MSISDN    = 2,    // Numéro de téléphone
    IDENTITY_TYPE_TMSI      = 3,    // Temporary Mobile Subscriber Identity
    IDENTITY_TYPE_MAC       = 4,    // Adresse MAC-like
    IDENTITY_TYPE_IMEI      = 5     // International Mobile Equipment Identity
} IdentityType;

// ============================================================
// SECTION 3 : CAPACITÉS DU DISPOSITIF
// ============================================================

/**
 * @brief Capacités du dispositif (flags)
 */
typedef enum {
    CAPABILITY_NONE             = 0,
    CAPABILITY_VOICE_CALL       = (1 << 0),   // Appels vocaux
    CAPABILITY_SMS              = (1 << 1),   // Messagerie texte
    CAPABILITY_ENCRYPTION       = (1 << 2),   // Chiffrement
    CAPABILITY_FILE_TRANSFER    = (1 << 3),   // Transfert de fichiers
    CAPABILITY_CONFERENCE       = (1 << 4),   // Appel conférence
    CAPABILITY_GPS              = (1 << 5),   // Géolocalisation
    CAPABILITY_REPEATER         = (1 << 6),   // Mode répéteur
    CAPABILITY_EMERGENCY        = (1 << 7),   // Appels d'urgence
    CAPABILITY_LOW_POWER        = (1 << 8),   // Mode basse consommation
    CAPABILITY_FIRMWARE_UPDATE  = (1 << 9),   // Mise à jour OTA
    CAPABILITY_DTMF             = (1 << 10),  // Tonalités DTMF
    CAPABILITY_RINGTONE         = (1 << 11),  // Sonneries
    CAPABILITY_VIBRATION        = (1 << 12),  // Vibreur
    CAPABILITY_SPEAKERPHONE     = (1 << 13),  // Haut-parleur mains libres
    CAPABILITY_DISPLAY          = (1 << 14),  // Écran intégré
    CAPABILITY_KEYPAD           = (1 << 15)   // Clavier physique
} DeviceCapability;

/** @brief Capacités par défaut du téléphone LoRa */
#define IDENTITY_DEFAULT_CAPABILITIES \
    (CAPABILITY_VOICE_CALL | CAPABILITY_SMS | \
     CAPABILITY_DTMF | CAPABILITY_RINGTONE | \
     CAPABILITY_DISPLAY | CAPABILITY_KEYPAD | \
     CAPABILITY_LOW_POWER)

// ============================================================
// SECTION 4 : STRUCTURE D'IDENTITÉ
// ============================================================

/**
 * @brief Identité complète du dispositif
 */
typedef struct {
    // Identifiants uniques
    uint32_t uid;                               // UID matériel (32 bits)
    uint64_t imsi;                              // IMSI (64 bits)
    char msisdn[IDENTITY_PHONE_NUMBER_MAX];     // Numéro de téléphone
    uint32_t tmsi;                              // Identifiant temporaire
    uint8_t mac[IDENTITY_MAC_SIZE];             // Adresse MAC-like
    
    // Informations
    char deviceName[IDENTITY_DEVICE_NAME_MAX];  // Nom du dispositif
    char deviceModel[IDENTITY_DEVICE_NAME_MAX]; // Modèle
    char manufacturer[IDENTITY_DEVICE_NAME_MAX];// Fabricant
    char softwareVersion[8];                    // Version logicielle
    char hardwareVersion[8];                    // Version matérielle
    
    // Capacités
    uint32_t capabilities;                      // Flags de capacités
    
    // État réseau
    bool registered;                            // Enregistré sur le réseau
    bool attached;                              // Attaché au réseau
    uint32_t lastRegistration;                  // Dernier enregistrement
    
    // Sécurité
    uint8_t ki[16];                             // Clé secrète d'authentification
    uint8_t opc[16];                            // OPc (Operator Code)
    uint32_t authSequenceNumber;                // Numéro de séquence auth
    
    // Statistiques
    uint32_t totalCalls;                        // Nombre total d'appels
    uint32_t totalSMS;                          // Nombre total de SMS
    uint32_t totalUptime;                       // Temps de fonctionnement
    
} DeviceIdentity;

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise l'identité du dispositif
 * 
 * Charge l'identité depuis la Flash ou génère une nouvelle identité.
 * 
 * @return true si l'identité est valide
 */
bool identity_init(void);

/**
 * @brief Génère une nouvelle identité (premier démarrage)
 */
void identity_generate_new(void);

/**
 * @brief Récupère l'identité complète
 * @return Pointeur vers la structure d'identité
 */
DeviceIdentity* identity_get(void);

// ============================================================
// SECTION 6 : FONCTIONS D'IDENTIFIANTS
// ============================================================

/**
 * @brief Récupère l'UID matériel unique
 * @return UID 32 bits
 */
uint32_t identity_get_uid(void);

/**
 * @brief Récupère l'IMSI
 * @return IMSI 64 bits
 */
uint64_t identity_get_imsi(void);

/**
 * @brief Formate l'IMSI en chaîne lisible
 * @param buffer Buffer de sortie (min 16 octets)
 */
void identity_get_imsi_string(char* buffer);

/**
 * @brief Récupère le numéro de téléphone (MSISDN)
 * @return Chaîne du numéro
 */
const char* identity_get_msisdn(void);

/**
 * @brief Définit le numéro de téléphone
 * @param msisdn Nouveau numéro
 */
void identity_set_msisdn(const char* msisdn);

/**
 * @brief Récupère le TMSI actuel
 * @return TMSI 32 bits
 */
uint32_t identity_get_tmsi(void);

/**
 * @brief Définit un nouveau TMSI
 * @param tmsi Nouveau TMSI
 */
void identity_set_tmsi(uint32_t tmsi);

/**
 * @brief Récupère l'adresse MAC-like
 * @param mac Buffer de sortie (6 octets)
 */
void identity_get_mac(uint8_t* mac);

/**
 * @brief Formate l'adresse MAC en chaîne
 * @param buffer Buffer de sortie (min 18 octets)
 */
void identity_get_mac_string(char* buffer);

// ============================================================
// SECTION 7 : FONCTIONS D'INFORMATIONS
// ============================================================

const char* identity_get_device_name(void);
void identity_set_device_name(const char* name);
const char* identity_get_software_version(void);
uint32_t identity_get_capabilities(void);
bool identity_has_capability(DeviceCapability capability);
void identity_add_capability(DeviceCapability capability);
void identity_remove_capability(DeviceCapability capability);

// ============================================================
// SECTION 8 : FONCTIONS RÉSEAU
// ============================================================

bool identity_is_registered(void);
void identity_set_registered(bool registered);
bool identity_is_attached(void);
void identity_set_attached(bool attached);
uint32_t identity_get_last_registration(void);

// ============================================================
// SECTION 9 : FONCTIONS DE SÉCURITÉ
// ============================================================

void identity_get_ki(uint8_t* ki);
void identity_set_ki(const uint8_t* ki);
void identity_get_opc(uint8_t* opc);
void identity_set_opc(const uint8_t* opc);
uint32_t identity_get_auth_sequence(void);
uint32_t identity_increment_auth_sequence(void);

/**
 * @brief Génère une réponse d'authentification
 * @param randChallenge Challenge aléatoire (16 octets)
 * @param response Réponse calculée (sortie, 16 octets)
 */
void identity_generate_auth_response(const uint8_t* randChallenge, uint8_t* response);

// ============================================================
// SECTION 10 : FONCTIONS DE PERSISTANCE
// ============================================================

bool identity_save(void);
bool identity_load(void);
void identity_factory_reset(void);

// ============================================================
// SECTION 11 : FONCTIONS DE CONVERSION
// ============================================================

/**
 * @brief Convertit un UID en numéro de téléphone
 * @param uid UID matériel
 * @param msisdn Buffer de sortie
 */
void identity_uid_to_msisdn(uint32_t uid, char* msisdn);

/**
 * @brief Convertit un UID en IMSI
 * @param uid UID matériel
 * @return IMSI 64 bits
 */
uint64_t identity_uid_to_imsi(uint32_t uid);

/**
 * @brief Convertit un UID en adresse MAC
 * @param uid UID matériel
 * @param mac Buffer de sortie (6 octets)
 */
void identity_uid_to_mac(uint32_t uid, uint8_t* mac);

/**
 * @brief Vérifie si un numéro de téléphone est valide
 * @param msisdn Numéro à vérifier
 * @return true si valide
 */
bool identity_is_valid_msisdn(const char* msisdn);

/**
 * @brief Compare deux identités
 * @param a Première identité
 * @param b Seconde identité
 * @return true si identiques
 */
bool identity_compare(const DeviceIdentity* a, const DeviceIdentity* b);

// ============================================================
// SECTION 12 : FONCTIONS DE DÉBOGAGE
// ============================================================

void identity_print(void);
void identity_print_short(void);
void identity_print_capabilities(void);
bool identity_self_test(void);

// ============================================================
// SECTION 13 : MACROS UTILITAIRES
// ============================================================

#define IDENTITY_HAS_CAPABILITY(cap)    identity_has_capability(cap)
#define IDENTITY_IS_VALID()             (identity_get_uid() != 0)

// ============================================================
// SECTION 14 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define IDENTITY_DEBUG(fmt, ...)    printf("[IDENTITY] " fmt, ##__VA_ARGS__)
#else
    #define IDENTITY_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // IDENTITY_H