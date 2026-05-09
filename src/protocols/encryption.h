/**
 * @file encryption.h
 * @brief Chiffrement des communications LoRa
 * 
 * Ce fichier implémente le chiffrement des paquets pour
 * sécuriser les communications du téléphone LoRa.
 * 
 * Algorithmes supportés :
 * - XOR simple (léger, pour les messages courts)
 * - ChaCha20 (recommandé pour la voix)
 * - AES-128 (si accélération matérielle disponible)
 * - AES-256 (sécurité maximale)
 * 
 * Niveaux de sécurité :
 * - NONE   : Pas de chiffrement (compatible tous téléphones)
 * - BASIC  : XOR avec clé partagée (léger)
 * - MEDIUM : ChaCha20 (bon compromis)
 * - HIGH   : AES-128 (standard)
 * - MAX    : AES-256 (militaire)
 * 
 * Échange de clés :
 * - Clé pré-partagée (PSK) pour les contacts connus
 * - Échange Diffie-Hellman sur LoRa (ECDH)
 * - Dérivation de clé de session
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "identity.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define ENCRYPTION_VERSION              "1.0.0"

/** @brief Niveaux de chiffrement */
typedef enum {
    ENCRYPTION_NONE     = 0,        // Pas de chiffrement
    ENCRYPTION_BASIC    = 1,        // XOR simple
    ENCRYPTION_MEDIUM   = 2,        // ChaCha20
    ENCRYPTION_HIGH     = 3,        // AES-128
    ENCRYPTION_MAX      = 4         // AES-256
} EncryptionLevel;

/** @brief Algorithmes disponibles */
typedef enum {
    ENCRYPTION_ALGO_NONE       = 0,
    ENCRYPTION_ALGO_XOR        = 1,
    ENCRYPTION_ALGO_CHACHA20   = 2,
    ENCRYPTION_ALGO_AES128     = 3,
    ENCRYPTION_ALGO_AES256     = 4
} EncryptionAlgorithm;

/** @brief Modes d'échange de clés */
typedef enum {
    KEY_EXCHANGE_NONE       = 0,    // Clé pré-partagée
    KEY_EXCHANGE_PSK        = 1,    // Pre-Shared Key
    KEY_EXCHANGE_DH         = 2,    // Diffie-Hellman
    KEY_EXCHANGE_ECDH       = 3     // Elliptic Curve DH
} KeyExchangeMethod;

/** @brief Tailles des clés */
#define ENCRYPTION_KEY_SIZE_128      16      // 128 bits = 16 octets
#define ENCRYPTION_KEY_SIZE_256      32      // 256 bits = 32 octets
#define ENCRYPTION_IV_SIZE           12      // Vecteur d'initialisation
#define ENCRYPTION_TAG_SIZE          16      // Tag d'authentification
#define ENCRYPTION_SALT_SIZE         16      // Sel pour dérivation
#define ENCRYPTION_MAX_KEY_SIZE      32      // Taille max de clé

/** @brief Longueur maximale d'une phrase de passe */
#define ENCRYPTION_PASSPHRASE_MAX    64

// ============================================================
// SECTION 2 : STRUCTURES DE CLÉS
// ============================================================

/**
 * @brief Clé de chiffrement
 */
typedef struct {
    uint8_t key[ENCRYPTION_MAX_KEY_SIZE];   // Données de la clé
    uint8_t keySize;                         // Taille réelle (16 ou 32)
    EncryptionAlgorithm algorithm;           // Algorithme associé
    uint32_t creationTime;                   // Date de création
    uint32_t expirationTime;                 // Date d'expiration (0 = jamais)
    bool isSessionKey;                       // Clé de session temporaire
} EncryptionKey;

/**
 * @brief Contexte de chiffrement pour un contact
 */
typedef struct {
    char msisdn[IDENTITY_PHONE_NUMBER_MAX];  // Numéro du contact
    EncryptionKey sharedKey;                  // Clé partagée avec ce contact
    EncryptionLevel negotiatedLevel;          // Niveau négocié
    KeyExchangeMethod keyExchangeMethod;      // Méthode d'échange
    bool encryptionEnabled;                   // Chiffrement activé
    uint32_t lastKeyExchange;                 // Dernier échange de clés
    uint8_t sessionIV[ENCRYPTION_IV_SIZE];   // IV de session
    uint32_t messageCounter;                  // Compteur de messages
} EncryptionContext;

/**
 * @brief Configuration globale du chiffrement
 */
typedef struct {
    EncryptionLevel defaultLevel;             // Niveau par défaut
    bool requireEncryption;                   // Exiger le chiffrement
    bool allowFallback;                       // Autoriser le fallback sans chiffrement
    KeyExchangeMethod preferredKeyExchange;   // Méthode d'échange préférée
    char passphrase[ENCRYPTION_PASSPHRASE_MAX]; // Phrase de passe maîtresse
    uint8_t masterKey[ENCRYPTION_KEY_SIZE_256]; // Clé maîtresse dérivée
} EncryptionConfig;

// ============================================================
// SECTION 3 : ÉTAT DU MODULE
// ============================================================

typedef struct {
    bool initialized;
    EncryptionConfig config;
    EncryptionContext* contacts;              // Contextes par contact
    uint16_t contactCount;
    uint16_t maxContacts;
    
    // Statistiques
    uint32_t messagesEncrypted;
    uint32_t messagesDecrypted;
    uint32_t keyExchangesPerformed;
    uint32_t errors;
} EncryptionState;

// ============================================================
// SECTION 4 : FONCTIONS D'INITIALISATION
// ============================================================

bool encryption_init(const EncryptionConfig* config);
void encryption_deinit(void);
bool encryption_is_ready(void);

// ============================================================
// SECTION 5 : FONCTIONS DE CHIFFREMENT/DÉCHIFFREMENT
// ============================================================

/**
 * @brief Chiffre un buffer de données
 * @param contactMsisdn Numéro du destinataire
 * @param plaintext Données en clair
 * @param plaintextLen Longueur des données en clair
 * @param ciphertext Buffer de sortie chiffré
 * @param ciphertextLen Longueur du buffer chiffré (mis à jour)
 * @return true si succès
 */
bool encryption_encrypt(const char* contactMsisdn, 
                        const uint8_t* plaintext, uint16_t plaintextLen,
                        uint8_t* ciphertext, uint16_t* ciphertextLen);

/**
 * @brief Déchiffre un buffer de données
 * @param contactMsisdn Numéro de l'expéditeur
 * @param ciphertext Données chiffrées
 * @param ciphertextLen Longueur des données chiffrées
 * @param plaintext Buffer de sortie déchiffré
 * @param plaintextLen Longueur du buffer déchiffré (mis à jour)
 * @return true si succès
 */
bool encryption_decrypt(const char* contactMsisdn,
                        const uint8_t* ciphertext, uint16_t ciphertextLen,
                        uint8_t* plaintext, uint16_t* plaintextLen);

/**
 * @brief Vérifie l'intégrité d'un message (authentification)
 */
bool encryption_verify(const uint8_t* data, uint16_t dataLen, 
                       const uint8_t* tag, uint8_t tagLen);

// ============================================================
// SECTION 6 : FONCTIONS DE GESTION DES CLÉS
// ============================================================

bool encryption_generate_key(EncryptionKey* key, EncryptionAlgorithm algorithm, uint8_t keySize);
bool encryption_derive_key(EncryptionKey* key, const char* passphrase, const uint8_t* salt);
bool encryption_set_shared_key(const char* contactMsisdn, const EncryptionKey* key);
bool encryption_get_shared_key(const char* contactMsisdn, EncryptionKey* key);
bool encryption_has_shared_key(const char* contactMsisdn);
bool encryption_delete_shared_key(const char* contactMsisdn);

// ============================================================
// SECTION 7 : FONCTIONS D'ÉCHANGE DE CLÉS
// ============================================================

/**
 * @brief Initie un échange de clés Diffie-Hellman
 */
bool encryption_start_key_exchange(const char* contactMsisdn, KeyExchangeMethod method);

/**
 * @brief Traite une réponse d'échange de clés
 */
bool encryption_process_key_exchange(const char* contactMsisdn, const uint8_t* data, uint16_t len);

/**
 * @brief Finalise un échange de clés
 */
bool encryption_finalize_key_exchange(const char* contactMsisdn);

// ============================================================
// SECTION 8 : FONCTIONS DE CONFIGURATION
// ============================================================

void encryption_set_level(const char* contactMsisdn, EncryptionLevel level);
EncryptionLevel encryption_get_level(const char* contactMsisdn);
void encryption_set_default_level(EncryptionLevel level);
void encryption_enable_contact(const char* contactMsisdn, bool enable);
bool encryption_is_enabled(const char* contactMsisdn);

// ============================================================
// SECTION 9 : FONCTIONS UTILITAIRES
// ============================================================

bool encryption_self_test(void);
void encryption_print_state(void);
void encryption_print_contact(const char* contactMsisdn);

// ============================================================
// SECTION 10 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define ENCRYPT_DEBUG(fmt, ...)     printf("[ENCRYPT] " fmt, ##__VA_ARGS__)
#else
    #define ENCRYPT_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 11 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // ENCRYPTION_H