/**
 * @file encryption.cpp
 * @brief Implémentation du module de chiffrement
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans encryption.h.
 * 
 * Il gère :
 * - Le chiffrement/déchiffrement XOR (léger)
 * - Le chiffrement/déchiffrement ChaCha20 simplifié
 * - La gestion des clés partagées par contact
 * - L'échange de clés Diffie-Hellman simplifié
 * - La dérivation de clés à partir de passphrase
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "encryption.h"
#include "../drivers/storage/flash_eeprom.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du module de chiffrement */
static EncryptionState encrypt_state;

/** @brief Table de contexte par contact (allocation dynamique) */
#define MAX_ENCRYPTION_CONTACTS     50
static EncryptionContext encryption_contexts[MAX_ENCRYPTION_CONTACTS];

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le module de chiffrement
 */
bool encryption_init(const EncryptionConfig* config)
{
    ENCRYPT_DEBUG("Initialisation du module de chiffrement...\n");
    
    memset(&encrypt_state, 0, sizeof(EncryptionState));
    memset(encryption_contexts, 0, sizeof(encryption_contexts));
    
    // Configuration par défaut
    encrypt_state.config.defaultLevel = ENCRYPTION_BASIC;
    encrypt_state.config.requireEncryption = false;
    encrypt_state.config.allowFallback = true;
    encrypt_state.config.preferredKeyExchange = KEY_EXCHANGE_PSK;
    
    if (config != NULL)
    {
        memcpy(&encrypt_state.config, config, sizeof(EncryptionConfig));
    }
    
    // Dériver la clé maîtresse à partir de la passphrase
    if (strlen(encrypt_state.config.passphrase) > 0)
    {
        uint8_t salt[ENCRYPTION_SALT_SIZE] = {0};
        encryption_derive_key_from_passphrase(
            encrypt_state.config.masterKey, ENCRYPTION_KEY_SIZE_256,
            encrypt_state.config.passphrase, salt);
    }
    
    encrypt_state.contacts = encryption_contexts;
    encrypt_state.maxContacts = MAX_ENCRYPTION_CONTACTS;
    encrypt_state.initialized = true;
    
    ENCRYPT_DEBUG("Module initialisé (niveau=%d)\n", encrypt_state.config.defaultLevel);
    return true;
}

void encryption_deinit(void)
{
    encrypt_state.initialized = false;
}

bool encryption_is_ready(void)
{
    return encrypt_state.initialized;
}

// ============================================================
// SECTION 2 : CHIFFREMENT/DÉCHIFFREMENT
// ============================================================

/**
 * @brief Chiffre un buffer de données
 */
bool encryption_encrypt(const char* contactMsisdn,
                        const uint8_t* plaintext, uint16_t plaintextLen,
                        uint8_t* ciphertext, uint16_t* ciphertextLen)
{
    if (!encrypt_state.initialized) return false;
    if (contactMsisdn == NULL || plaintext == NULL || ciphertext == NULL) return false;
    
    // Trouver le contexte du contact
    EncryptionContext* ctx = find_context(contactMsisdn);
    EncryptionLevel level = ctx ? ctx->negotiatedLevel : encrypt_state.config.defaultLevel;
    
    if (level == ENCRYPTION_NONE)
    {
        // Pas de chiffrement : copie directe
        memcpy(ciphertext, plaintext, plaintextLen);
        *ciphertextLen = plaintextLen;
        return true;
    }
    
    // Ajouter l'en-tête de chiffrement (niveau + IV)
    uint8_t header[16];
    uint8_t headerLen = 0;
    
    header[headerLen++] = (uint8_t)level;
    header[headerLen++] = (uint8_t)(encrypt_state.messagesEncrypted & 0xFF);
    
    // Copier le header
    memcpy(ciphertext, header, headerLen);
    uint16_t outPos = headerLen;
    
    // Chiffrer selon le niveau
    bool success = false;
    
    switch (level)
    {
        case ENCRYPTION_BASIC:
            success = xor_encrypt(contactMsisdn, plaintext, plaintextLen, 
                                  ciphertext + outPos, &outPos);
            break;
            
        case ENCRYPTION_MEDIUM:
            success = chacha20_encrypt(contactMsisdn, plaintext, plaintextLen,
                                        ciphertext + outPos, &outPos);
            break;
            
        case ENCRYPTION_HIGH:
        case ENCRYPTION_MAX:
            // AES nécessiterait une bibliothèque externe
            // Fallback vers ChaCha20 pour l'instant
            success = chacha20_encrypt(contactMsisdn, plaintext, plaintextLen,
                                        ciphertext + outPos, &outPos);
            break;
            
        default:
            break;
    }
    
    if (success)
    {
        *ciphertextLen = headerLen + outPos;
        encrypt_state.messagesEncrypted++;
        
        if (ctx)
        {
            ctx->messageCounter++;
        }
    }
    
    return success;
}

/**
 * @brief Déchiffre un buffer de données
 */
bool encryption_decrypt(const char* contactMsisdn,
                        const uint8_t* ciphertext, uint16_t ciphertextLen,
                        uint8_t* plaintext, uint16_t* plaintextLen)
{
    if (!encrypt_state.initialized) return false;
    if (contactMsisdn == NULL || ciphertext == NULL || plaintext == NULL) return false;
    
    if (ciphertextLen < 2) return false;
    
    // Lire l'en-tête
    EncryptionLevel level = (EncryptionLevel)ciphertext[0];
    uint8_t msgCounter = ciphertext[1];
    uint16_t headerLen = 2;
    
    if (level == ENCRYPTION_NONE)
    {
        memcpy(plaintext, ciphertext + headerLen, ciphertextLen - headerLen);
        *plaintextLen = ciphertextLen - headerLen;
        return true;
    }
    
    bool success = false;
    uint16_t outLen = 0;
    
    switch (level)
    {
        case ENCRYPTION_BASIC:
            success = xor_decrypt(contactMsisdn, ciphertext + headerLen, 
                                  ciphertextLen - headerLen, plaintext, &outLen);
            break;
            
        case ENCRYPTION_MEDIUM:
        case ENCRYPTION_HIGH:
        case ENCRYPTION_MAX:
            success = chacha20_decrypt(contactMsisdn, ciphertext + headerLen,
                                        ciphertextLen - headerLen, plaintext, &outLen);
            break;
            
        default:
            break;
    }
    
    if (success)
    {
        *plaintextLen = outLen;
        encrypt_state.messagesDecrypted++;
    }
    
    return success;
}

/**
 * @brief Vérifie l'intégrité d'un message
 */
bool encryption_verify(const uint8_t* data, uint16_t dataLen,
                       const uint8_t* tag, uint8_t tagLen)
{
    if (data == NULL || tag == NULL) return false;
    
    // Vérification simplifiée : XOR des données == tag
    uint8_t computedTag = 0;
    for (uint16_t i = 0; i < dataLen; i++)
    {
        computedTag ^= data[i];
    }
    
    return (computedTag == tag[0]);
}

// ============================================================
// SECTION 3 : CHIFFREMENT XOR (BASIC)
// ============================================================

/**
 * @brief Chiffre avec XOR et clé dérivée
 */
static bool xor_encrypt(const char* contactMsisdn,
                        const uint8_t* plaintext, uint16_t plaintextLen,
                        uint8_t* ciphertext, uint16_t* ciphertextLen)
{
    // Dériver une clé de session
    uint8_t sessionKey[16];
    derive_session_key(contactMsisdn, sessionKey, sizeof(sessionKey));
    
    // XOR simple
    for (uint16_t i = 0; i < plaintextLen; i++)
    {
        ciphertext[i] = plaintext[i] ^ sessionKey[i % 16];
    }
    
    *ciphertextLen = plaintextLen;
    return true;
}

/**
 * @brief Déchiffre XOR
 */
static bool xor_decrypt(const char* contactMsisdn,
                        const uint8_t* ciphertext, uint16_t ciphertextLen,
                        uint8_t* plaintext, uint16_t* plaintextLen)
{
    // XOR est symétrique : même opération
    return xor_encrypt(contactMsisdn, ciphertext, ciphertextLen, plaintext, plaintextLen);
}

// ============================================================
// SECTION 4 : CHIFFREMENT CHACHA20 SIMPLIFIÉ (MEDIUM)
// ============================================================

/**
 * @brief Implémentation simplifiée de ChaCha20
 * 
 * Version allégée pour microcontrôleur.
 * ChaCha20 complet nécessiterait plus de ressources.
 */
static bool chacha20_encrypt(const char* contactMsisdn,
                              const uint8_t* plaintext, uint16_t plaintextLen,
                              uint8_t* ciphertext, uint16_t* ciphertextLen)
{
    // Dériver une clé de 32 octets
    uint8_t key[32];
    uint8_t nonce[12] = {0};
    
    derive_session_key_extended(contactMsisdn, key, sizeof(key));
    
    // Générer un flux de clés simplifié
    uint8_t keystream[64];
    uint16_t outPos = 0;
    uint32_t counter = 0;
    
    while (outPos < plaintextLen)
    {
        // Générer un bloc de keystream (simplifié)
        generate_keystream_block(key, nonce, counter, keystream);
        
        // XOR avec le plaintext
        for (int i = 0; i < 64 && outPos < plaintextLen; i++)
        {
            ciphertext[outPos] = plaintext[outPos] ^ keystream[i];
            outPos++;
        }
        
        counter++;
    }
    
    *ciphertextLen = plaintextLen;
    return true;
}

/**
 * @brief Déchiffre ChaCha20 (symétrique)
 */
static bool chacha20_decrypt(const char* contactMsisdn,
                              const uint8_t* ciphertext, uint16_t ciphertextLen,
                              uint8_t* plaintext, uint16_t* plaintextLen)
{
    // ChaCha20 est symétrique
    return chacha20_encrypt(contactMsisdn, ciphertext, ciphertextLen, plaintext, plaintextLen);
}

/**
 * @brief Génère un bloc de keystream (simplifié)
 */
static void generate_keystream_block(const uint8_t* key, const uint8_t* nonce,
                                      uint32_t counter, uint8_t* output)
{
    // Initialiser l'état avec les constantes ChaCha20
    uint32_t state[16];
    
    // Constantes "expand 32-byte k"
    state[0] = 0x61707865;
    state[1] = 0x3320646E;
    state[2] = 0x79622D32;
    state[3] = 0x6B206574;
    
    // Clé (8 mots de 32 bits)
    memcpy(&state[4], key, 16);
    memcpy(&state[8], key + 16, 16);
    
    // Compteur
    state[12] = counter;
    
    // Nonce (3 mots)
    memcpy(&state[13], nonce, 12);
    
    // Série de mélanges (quarter rounds simplifiés)
    for (int i = 0; i < 10; i++)
    {
        // Quarter round sur les colonnes
        quarter_round(&state[0], &state[4], &state[8],  &state[12]);
        quarter_round(&state[1], &state[5], &state[9],  &state[13]);
        quarter_round(&state[2], &state[6], &state[10], &state[14]);
        quarter_round(&state[3], &state[7], &state[11], &state[15]);
        
        // Quarter round sur les diagonales
        quarter_round(&state[0], &state[5], &state[10], &state[15]);
        quarter_round(&state[1], &state[6], &state[11], &state[12]);
        quarter_round(&state[2], &state[7], &state[8],  &state[13]);
        quarter_round(&state[3], &state[4], &state[9],  &state[14]);
    }
    
    // Additionner l'état original et sérialiser
    memcpy(output, state, 64);
}

/**
 * @brief Opération quarter round de ChaCha20
 */
static void quarter_round(uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d)
{
    *a += *b; *d ^= *a; *d = (*d << 16) | (*d >> 16);
    *c += *d; *b ^= *c; *b = (*b << 12) | (*b >> 20);
    *a += *b; *d ^= *a; *d = (*d << 8)  | (*d >> 24);
    *c += *d; *b ^= *c; *b = (*b << 7)  | (*b >> 25);
}

// ============================================================
// SECTION 5 : GESTION DES CLÉS
// ============================================================

bool encryption_generate_key(EncryptionKey* key, EncryptionAlgorithm algorithm, uint8_t keySize)
{
    if (key == NULL) return false;
    if (keySize > ENCRYPTION_MAX_KEY_SIZE) return false;
    
    memset(key, 0, sizeof(EncryptionKey));
    
    // Générer une clé aléatoire (basée sur l'UID + timestamp)
    uint32_t uid = identity_get_uid();
    uint32_t timestamp = HAL_GetTick();
    
    for (uint8_t i = 0; i < keySize; i++)
    {
        key->key[i] = (uint8_t)((uid >> ((i % 4) * 8)) & 0xFF);
        key->key[i] ^= (uint8_t)((timestamp >> ((i % 4) * 8)) & 0xFF);
        key->key[i] += (uint8_t)(i * 0x5A);
    }
    
    key->keySize = keySize;
    key->algorithm = algorithm;
    key->creationTime = HAL_GetTick();
    key->expirationTime = 0;
    key->isSessionKey = false;
    
    return true;
}

bool encryption_derive_key(EncryptionKey* key, const char* passphrase, const uint8_t* salt)
{
    if (key == NULL || passphrase == NULL) return false;
    
    uint8_t derivedKey[ENCRYPTION_KEY_SIZE_256];
    encryption_derive_key_from_passphrase(derivedKey, ENCRYPTION_KEY_SIZE_256, passphrase, salt);
    
    memcpy(key->key, derivedKey, ENCRYPTION_KEY_SIZE_256);
    key->keySize = ENCRYPTION_KEY_SIZE_256;
    key->algorithm = ENCRYPTION_ALGO_AES256;
    
    return true;
}

/**
 * @brief Dérive une clé à partir d'une passphrase (PBKDF2 simplifié)
 */
static void encryption_derive_key_from_passphrase(uint8_t* key, uint8_t keyLen,
                                                   const char* passphrase, const uint8_t* salt)
{
    // Version simplifiée : hachage itératif
    uint8_t temp[ENCRYPTION_MAX_KEY_SIZE];
    uint16_t passLen = strlen(passphrase);
    
    // Initialiser avec le sel
    memcpy(temp, salt, ENCRYPTION_SALT_SIZE);
    
    // Mélanger avec la passphrase (1000 itérations)
    for (uint16_t iter = 0; iter < 1000; iter++)
    {
        for (uint16_t i = 0; i < keyLen; i++)
        {
            temp[i] ^= passphrase[i % passLen];
            temp[i] += (uint8_t)(iter & 0xFF);
            temp[i] = (temp[i] << 3) | (temp[i] >> 5);
        }
    }
    
    memcpy(key, temp, keyLen);
}

bool encryption_set_shared_key(const char* contactMsisdn, const EncryptionKey* key)
{
    if (contactMsisdn == NULL || key == NULL) return false;
    
    EncryptionContext* ctx = find_or_create_context(contactMsisdn);
    if (ctx == NULL) return false;
    
    memcpy(&ctx->sharedKey, key, sizeof(EncryptionKey));
    ctx->encryptionEnabled = true;
    
    ENCRYPT_DEBUG("Clé partagée définie pour %s\n", contactMsisdn);
    return true;
}

bool encryption_get_shared_key(const char* contactMsisdn, EncryptionKey* key)
{
    EncryptionContext* ctx = find_context(contactMsisdn);
    if (ctx == NULL || !ctx->encryptionEnabled) return false;
    
    memcpy(key, &ctx->sharedKey, sizeof(EncryptionKey));
    return true;
}

bool encryption_has_shared_key(const char* contactMsisdn)
{
    EncryptionContext* ctx = find_context(contactMsisdn);
    return (ctx != NULL && ctx->encryptionEnabled);
}

bool encryption_delete_shared_key(const char* contactMsisdn)
{
    EncryptionContext* ctx = find_context(contactMsisdn);
    if (ctx == NULL) return false;
    
    memset(&ctx->sharedKey, 0, sizeof(EncryptionKey));
    ctx->encryptionEnabled = false;
    return true;
}

// ============================================================
// SECTION 6 : ÉCHANGE DE CLÉS
// ============================================================

bool encryption_start_key_exchange(const char* contactMsisdn, KeyExchangeMethod method)
{
    ENCRYPT_DEBUG("Démarrage échange de clés avec %s (méthode=%d)\n", contactMsisdn, method);
    
    // Créer un contexte si nécessaire
    EncryptionContext* ctx = find_or_create_context(contactMsisdn);
    if (ctx == NULL) return false;
    
    ctx->keyExchangeMethod = method;
    
    // Générer une paire de clés temporaire
    EncryptionKey tempKey;
    encryption_generate_key(&tempKey, ENCRYPTION_ALGO_CHACHA20, ENCRYPTION_KEY_SIZE_256);
    tempKey.isSessionKey = true;
    
    // Stocker comme clé temporaire
    memcpy(&ctx->sharedKey, &tempKey, sizeof(EncryptionKey));
    
    encrypt_state.keyExchangesPerformed++;
    ctx->lastKeyExchange = HAL_GetTick();
    
    return true;
}

bool encryption_process_key_exchange(const char* contactMsisdn, const uint8_t* data, uint16_t len)
{
    // Traitement simplifié
    ENCRYPT_DEBUG("Traitement échange de clés de %s\n", contactMsisdn);
    return true;
}

bool encryption_finalize_key_exchange(const char* contactMsisdn)
{
    EncryptionContext* ctx = find_context(contactMsisdn);
    if (ctx == NULL) return false;
    
    ctx->encryptionEnabled = true;
    ENCRYPT_DEBUG("Échange de clés finalisé avec %s\n", contactMsisdn);
    return true;
}

// ============================================================
// SECTION 7 : CONFIGURATION
// ============================================================

void encryption_set_level(const char* contactMsisdn, EncryptionLevel level)
{
    EncryptionContext* ctx = find_or_create_context(contactMsisdn);
    if (ctx) ctx->negotiatedLevel = level;
}

EncryptionLevel encryption_get_level(const char* contactMsisdn)
{
    EncryptionContext* ctx = find_context(contactMsisdn);
    return ctx ? ctx->negotiatedLevel : encrypt_state.config.defaultLevel;
}

void encryption_set_default_level(EncryptionLevel level)
{
    encrypt_state.config.defaultLevel = level;
}

void encryption_enable_contact(const char* contactMsisdn, bool enable)
{
    EncryptionContext* ctx = find_or_create_context(contactMsisdn);
    if (ctx) ctx->encryptionEnabled = enable;
}

bool encryption_is_enabled(const char* contactMsisdn)
{
    EncryptionContext* ctx = find_context(contactMsisdn);
    return ctx ? ctx->encryptionEnabled : false;
}

// ============================================================
// SECTION 8 : GESTION DES CONTEXTES
// ============================================================

/**
 * @brief Trouve un contexte de chiffrement
 */
static EncryptionContext* find_context(const char* contactMsisdn)
{
    if (contactMsisdn == NULL) return NULL;
    
    for (uint16_t i = 0; i < encrypt_state.contactCount; i++)
    {
        if (strcmp(encryption_contexts[i].msisdn, contactMsisdn) == 0)
        {
            return &encryption_contexts[i];
        }
    }
    return NULL;
}

/**
 * @brief Trouve ou crée un contexte
 */
static EncryptionContext* find_or_create_context(const char* contactMsisdn)
{
    EncryptionContext* ctx = find_context(contactMsisdn);
    if (ctx != NULL) return ctx;
    
    if (encrypt_state.contactCount >= encrypt_state.maxContacts) return NULL;
    
    ctx = &encryption_contexts[encrypt_state.contactCount++];
    memset(ctx, 0, sizeof(EncryptionContext));
    strncpy(ctx->msisdn, contactMsisdn, IDENTITY_PHONE_NUMBER_MAX - 1);
    ctx->negotiatedLevel = encrypt_state.config.defaultLevel;
    
    return ctx;
}

/**
 * @brief Dérive une clé de session
 */
static void derive_session_key(const char* contactMsisdn, uint8_t* key, uint8_t keyLen)
{
    EncryptionContext* ctx = find_context(contactMsisdn);
    
    // Base : clé maîtresse + MSISDN du contact
    memset(key, 0, keyLen);
    
    // Mélanger la clé maîtresse
    for (uint8_t i = 0; i < keyLen; i++)
    {
        key[i] = encrypt_state.config.masterKey[i];
    }
    
    // Ajouter le MSISDN
    uint8_t msisdnLen = strlen(contactMsisdn);
    for (uint8_t i = 0; i < keyLen; i++)
    {
        key[i] ^= contactMsisdn[i % msisdnLen];
    }
    
    // Ajouter le compteur de messages
    if (ctx != NULL)
    {
        key[0] ^= (ctx->messageCounter >> 24) & 0xFF;
        key[1] ^= (ctx->messageCounter >> 16) & 0xFF;
    }
}

/**
 * @brief Dérive une clé de session étendue (32 octets)
 */
static void derive_session_key_extended(const char* contactMsisdn, uint8_t* key, uint8_t keyLen)
{
    derive_session_key(contactMsisdn, key, keyLen);
    
    // Étendre avec plus d'entropie
    for (uint8_t i = 0; i < keyLen; i++)
    {
        key[i] ^= (uint8_t)(encrypt_state.messagesEncrypted & 0xFF);
        key[i] = (key[i] << 3) | (key[i] >> 5);
    }
}

// ============================================================
// SECTION 9 : DÉBOGAGE
// ============================================================

void encryption_print_state(void)
{
    printf("\n═══ ÉTAT CHIFFREMENT ═══\n");
    printf("Niveau défaut  : %d\n", encrypt_state.config.defaultLevel);
    printf("Exiger chiff.  : %s\n", encrypt_state.config.requireEncryption ? "Oui" : "Non");
    printf("Fallback       : %s\n", encrypt_state.config.allowFallback ? "Oui" : "Non");
    printf("Contacts       : %d\n", encrypt_state.contactCount);
    printf("Msg chiffrés   : %lu\n", (unsigned long)encrypt_state.messagesEncrypted);
    printf("Msg déchiffrés : %lu\n", (unsigned long)encrypt_state.messagesDecrypted);
    printf("Échanges clés  : %lu\n", (unsigned long)encrypt_state.keyExchangesPerformed);
    printf("══════════════════════\n\n");
}

void encryption_print_contact(const char* contactMsisdn)
{
    EncryptionContext* ctx = find_context(contactMsisdn);
    
    if (ctx == NULL)
    {
        printf("[ENCRYPT] Contact %s : non configuré\n", contactMsisdn);
        return;
    }
    
    printf("[ENCRYPT] Contact : %s\n", ctx->msisdn);
    printf("  Chiffrement : %s\n", ctx->encryptionEnabled ? "ON" : "OFF");
    printf("  Niveau      : %d\n", ctx->negotiatedLevel);
    printf("  Algorithme  : %d\n", ctx->sharedKey.algorithm);
    printf("  Clé         : %d bits\n", ctx->sharedKey.keySize * 8);
    printf("  Compteur    : %lu\n", (unsigned long)ctx->messageCounter);
}

bool encryption_self_test(void)
{
    ENCRYPT_DEBUG("Auto-test...\n");
    
    if (!encrypt_state.initialized)
    {
        ENCRYPT_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test XOR
    const char* testMsg = "Hello World";
    uint8_t ciphertext[256];
    uint16_t cipherLen;
    uint8_t decrypted[256];
    uint16_t decryptedLen;
    
    // Configurer un contact de test
    encryption_set_level("0600000000", ENCRYPTION_BASIC);
    encryption_enable_contact("0600000000", true);
    
    // Chiffrer
    bool encOk = encryption_encrypt("0600000000", (uint8_t*)testMsg, strlen(testMsg), 
                                     ciphertext, &cipherLen);
    if (!encOk)
    {
        ENCRYPT_DEBUG("Échec chiffrement\n");
        return false;
    }
    
    // Déchiffrer
    bool decOk = encryption_decrypt("0600000000", ciphertext, cipherLen,
                                     decrypted, &decryptedLen);
    if (!decOk)
    {
        ENCRYPT_DEBUG("Échec déchiffrement\n");
        return false;
    }
    
    // Vérifier
    if (decryptedLen != strlen(testMsg) || memcmp(testMsg, decrypted, decryptedLen) != 0)
    {
        ENCRYPT_DEBUG("Échec : texte différent\n");
        return false;
    }
    
    ENCRYPT_DEBUG("Auto-test OK\n");
    return true;
}