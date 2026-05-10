/**
 * @file    endian_utils.cpp
 * @brief   Implémentation des conversions endianness
 * @author  Votre Nom
 * @date    2026
 *
 * Implémente les fonctions de conversion big/little-endian.
 *
 * UTILISATION DES INSTRUCTIONS CORTEX-M4 :
 *
 *   __REV(val)   : REV  - Inverse 4 octets (32 bits)
 *   __REV16(val) : REV16 - Inverse 2 octets dans chaque half-word
 *   __REVSH(val) : REVSH - Inverse 2 octets + extension signe 16→32
 *
 *   Ces instructions s'exécutent en 1 cycle. Les fonctions
 *   de ce module les utilisent automatiquement.
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "endian_utils.h"

/* ======================================================================== */
/*              DÉTECTION ENDIANNESS                                        */
/* ======================================================================== */

/**
 * @brief Vérifie si le système est little-endian
 *
 * Utilise une union pour tester l'ordre des octets en mémoire.
 */
bool Endian_IsLittleEndian(void)
{
    static const union {
        uint16_t value;
        uint8_t  bytes[2];
    } test = { .value = 0x0001 };

    return test.bytes[0] == 0x01;  /* LSB en premier = little-endian */
}

/**
 * @brief Vérifie si le système est big-endian
 */
bool Endian_IsBigEndian(void)
{
    return !Endian_IsLittleEndian();
}

/* ======================================================================== */
/*              SWAP 16 BITS                                                */
/* ======================================================================== */

/**
 * @brief Inverse les octets d'un uint16
 *
 * Utilise __REVSH puis décale pour obtenir le résultat non signé.
 */
uint16_t Endian_Swap16(uint16_t value)
{
    /* __REVSH fait swap + extension signe, on masque */
    return (uint16_t)(__REVSH((int16_t)value) & 0xFFFF);
}

/**
 * @brief Inverse les octets de chaque uint16 dans un buffer
 */
void Endian_Swap16Buffer(uint16_t* data, size_t count)
{
    if (!data || count == 0) return;

    for (size_t i = 0; i < count; i++) {
        data[i] = Endian_Swap16(data[i]);
    }
}

/* ======================================================================== */
/*              SWAP 32 BITS                                                */
/* ======================================================================== */

/**
 * @brief Inverse les octets d'un uint32
 *
 * Utilise l'instruction REV (1 cycle).
 */
uint32_t Endian_Swap32(uint32_t value)
{
    return __REV(value);
}

/**
 * @brief Inverse les octets de chaque uint32 dans un buffer
 */
void Endian_Swap32Buffer(uint32_t* data, size_t count)
{
    if (!data || count == 0) return;

    for (size_t i = 0; i < count; i++) {
        data[i] = Endian_Swap32(data[i]);
    }
}

/* ======================================================================== */
/*              SWAP 64 BITS                                                */
/* ======================================================================== */

/**
 * @brief Inverse les octets d'un uint64
 *
 * Combine deux swap 32 bits + inversion des deux moitiés.
 */
uint64_t Endian_Swap64(uint64_t value)
{
    uint32_t high = (uint32_t)(value >> 32);
    uint32_t low  = (uint32_t)(value & 0xFFFFFFFF);

    high = Endian_Swap32(high);
    low  = Endian_Swap32(low);

    return ((uint64_t)low << 32) | (uint64_t)high;
}

/**
 * @brief Inverse les octets de chaque uint64 dans un buffer
 */
void Endian_Swap64Buffer(uint64_t* data, size_t count)
{
    if (!data || count == 0) return;

    for (size_t i = 0; i < count; i++) {
        data[i] = Endian_Swap64(data[i]);
    }
}

/* ======================================================================== */
/*              CONVERSION RÉSEAU (HOST ↔ NETWORK)                          */
/* ======================================================================== */

uint16_t Endian_HTON16(uint16_t host_value)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return host_value;
#else
    return Endian_Swap16(host_value);
#endif
}

uint16_t Endian_NTOH16(uint16_t net_value)
{
    /* HTON et NTOH sont identiques (inversion symétrique) */
    return Endian_HTON16(net_value);
}

uint32_t Endian_HTON32(uint32_t host_value)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return host_value;
#else
    return Endian_Swap32(host_value);
#endif
}

uint32_t Endian_NTOH32(uint32_t net_value)
{
    return Endian_HTON32(net_value);
}

uint64_t Endian_HTON64(uint64_t host_value)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return host_value;
#else
    return Endian_Swap64(host_value);
#endif
}

uint64_t Endian_NTOH64(uint64_t net_value)
{
    return Endian_HTON64(net_value);
}

/* ======================================================================== */
/*              LECTURE DEPUIS BUFFER                                       */
/* ======================================================================== */

/**
 * @brief Lit un uint16 big-endian
 *
 * Les deux octets sont lus dans l'ordre big-endian :
 *   buffer[0] = poids fort, buffer[1] = poids faible
 */
uint16_t Endian_ReadU16BE(const uint8_t* buffer)
{
    if (!buffer) return 0;

    return ((uint16_t)buffer[0] << 8) | (uint16_t)buffer[1];
}

/**
 * @brief Lit un uint16 little-endian
 */
uint16_t Endian_ReadU16LE(const uint8_t* buffer)
{
    if (!buffer) return 0;

    return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

/**
 * @brief Lit un uint32 big-endian
 */
uint32_t Endian_ReadU32BE(const uint8_t* buffer)
{
    if (!buffer) return 0;

    return ((uint32_t)buffer[0] << 24) |
           ((uint32_t)buffer[1] << 16) |
           ((uint32_t)buffer[2] << 8)  |
           (uint32_t)buffer[3];
}

/**
 * @brief Lit un uint32 little-endian
 */
uint32_t Endian_ReadU32LE(const uint8_t* buffer)
{
    if (!buffer) return 0;

    return (uint32_t)buffer[0] |
           ((uint32_t)buffer[1] << 8)  |
           ((uint32_t)buffer[2] << 16) |
           ((uint32_t)buffer[3] << 24);
}

/**
 * @brief Lit un uint64 big-endian
 */
uint64_t Endian_ReadU64BE(const uint8_t* buffer)
{
    if (!buffer) return 0;

    return ((uint64_t)buffer[0] << 56) |
           ((uint64_t)buffer[1] << 48) |
           ((uint64_t)buffer[2] << 40) |
           ((uint64_t)buffer[3] << 32) |
           ((uint64_t)buffer[4] << 24) |
           ((uint64_t)buffer[5] << 16) |
           ((uint64_t)buffer[6] << 8)  |
           (uint64_t)buffer[7];
}

/**
 * @brief Lit un uint64 little-endian
 */
uint64_t Endian_ReadU64LE(const uint8_t* buffer)
{
    if (!buffer) return 0;

    return (uint64_t)buffer[0] |
           ((uint64_t)buffer[1] << 8)  |
           ((uint64_t)buffer[2] << 16) |
           ((uint64_t)buffer[3] << 24) |
           ((uint64_t)buffer[4] << 32) |
           ((uint64_t)buffer[5] << 40) |
           ((uint64_t)buffer[6] << 48) |
           ((uint64_t)buffer[7] << 56);
}

/* ======================================================================== */
/*              ÉCRITURE DANS BUFFER                                        */
/* ======================================================================== */

void Endian_WriteU16BE(uint8_t* buffer, uint16_t value)
{
    if (!buffer) return;

    buffer[0] = (uint8_t)(value >> 8);
    buffer[1] = (uint8_t)(value & 0xFF);
}

void Endian_WriteU16LE(uint8_t* buffer, uint16_t value)
{
    if (!buffer) return;

    buffer[0] = (uint8_t)(value & 0xFF);
    buffer[1] = (uint8_t)(value >> 8);
}

void Endian_WriteU32BE(uint8_t* buffer, uint32_t value)
{
    if (!buffer) return;

    buffer[0] = (uint8_t)(value >> 24);
    buffer[1] = (uint8_t)(value >> 16);
    buffer[2] = (uint8_t)(value >> 8);
    buffer[3] = (uint8_t)(value & 0xFF);
}

void Endian_WriteU32LE(uint8_t* buffer, uint32_t value)
{
    if (!buffer) return;

    buffer[0] = (uint8_t)(value & 0xFF);
    buffer[1] = (uint8_t)(value >> 8);
    buffer[2] = (uint8_t)(value >> 16);
    buffer[3] = (uint8_t)(value >> 24);
}

void Endian_WriteU64BE(uint8_t* buffer, uint64_t value)
{
    if (!buffer) return;

    buffer[0] = (uint8_t)(value >> 56);
    buffer[1] = (uint8_t)(value >> 48);
    buffer[2] = (uint8_t)(value >> 40);
    buffer[3] = (uint8_t)(value >> 32);
    buffer[4] = (uint8_t)(value >> 24);
    buffer[5] = (uint8_t)(value >> 16);
    buffer[6] = (uint8_t)(value >> 8);
    buffer[7] = (uint8_t)(value & 0xFF);
}

void Endian_WriteU64LE(uint8_t* buffer, uint64_t value)
{
    if (!buffer) return;

    buffer[0] = (uint8_t)(value & 0xFF);
    buffer[1] = (uint8_t)(value >> 8);
    buffer[2] = (uint8_t)(value >> 16);
    buffer[3] = (uint8_t)(value >> 24);
    buffer[4] = (uint8_t)(value >> 32);
    buffer[5] = (uint8_t)(value >> 40);
    buffer[6] = (uint8_t)(value >> 48);
    buffer[7] = (uint8_t)(value >> 56);
}

/* ======================================================================== */
/*              STRUCTURES                                                 */
/* ======================================================================== */

/**
 * @brief Convertit une structure octet par octet
 *
 * Parcourt les champs selon leurs tailles et applique les swaps
 * nécessaires. La taille 1 est ignorée (pas de swap pour un octet).
 *
 * Exemple d'utilisation :
 *   struct Packet {
 *       uint32_t freq;    // 4 octets → swap
 *       uint16_t power;   // 2 octets → swap
 *       uint8_t  flags;   // 1 octet  → ignoré
 *       uint32_t crc;     // 4 octets → swap
 *   };
 *   uint8_t sizes[] = {4, 2, 1, 4, 0};  // 0 = fin
 *   Endian_SwapStruct(&packet, sizes, 4);
 */
void Endian_SwapStruct(void* struct_ptr, const uint8_t* field_sizes, size_t count)
{
    if (!struct_ptr || !field_sizes || count == 0) return;

    uint8_t* ptr = (uint8_t*)struct_ptr;

    for (size_t i = 0; i < count && field_sizes[i] != 0; i++) {
        switch (field_sizes[i]) {
            case 2:
                Endian_Swap16Buffer((uint16_t*)ptr, 1);
                ptr += 2;
                break;

            case 4:
                Endian_Swap32Buffer((uint32_t*)ptr, 1);
                ptr += 4;
                break;

            case 8:
                Endian_Swap64Buffer((uint64_t*)ptr, 1);
                ptr += 8;
                break;

            default:
                /* Taille 1 ou autre : pas de swap */
                ptr += field_sizes[i];
                break;
        }
    }
}

/* ======================================================================== */
/*              FLOAT                                                       */
/* ======================================================================== */

/**
 * @brief Lit un float big-endian
 *
 * Utilise une union pour réinterpréter les bits sans conversion.
 */
float Endian_ReadFloatBE(const uint8_t* buffer)
{
    if (!buffer) return 0.0f;

    union {
        uint32_t raw;
        float    value;
    } converter;

    converter.raw = Endian_ReadU32BE(buffer);
    return converter.value;
}

/**
 * @brief Lit un float little-endian
 */
float Endian_ReadFloatLE(const uint8_t* buffer)
{
    if (!buffer) return 0.0f;

    union {
        uint32_t raw;
        float    value;
    } converter;

    converter.raw = Endian_ReadU32LE(buffer);
    return converter.value;
}

/**
 * @brief Écrit un float big-endian
 */
void Endian_WriteFloatBE(uint8_t* buffer, float value)
{
    if (!buffer) return;

    union {
        uint32_t raw;
        float    value;
    } converter;

    converter.value = value;
    Endian_WriteU32BE(buffer, converter.raw);
}

/**
 * @brief Écrit un float little-endian
 */
void Endian_WriteFloatLE(uint8_t* buffer, float value)
{
    if (!buffer) return;

    union {
        uint32_t raw;
        float    value;
    } converter;

    converter.value = value;
    Endian_WriteU32LE(buffer, converter.raw);
}

/* ======================================================================== */
/*              EXEMPLES D'UTILISATION                                      */
/* ======================================================================== */

#if 0  /* Exemples - Non compilés */

void endian_examples(void)
{
    uint8_t buffer[8];

    // ===== ÉCRITURE BIG-ENDIAN (POUR PAQUET LORA) =====
    uint32_t frequency = 868000000;  // 868 MHz en Hz
    Endian_WriteU32BE(buffer, frequency);
    // buffer = {0x33, 0xBB, 0x46, 0x00}

    uint16_t tx_power = 20;  // 20 dBm
    Endian_WriteU16BE(buffer + 4, tx_power);
    // buffer = {..., 0x00, 0x14}

    // ===== LECTURE BIG-ENDIAN (DEPUIS PAQUET LORA) =====
    uint8_t rx_packet[] = {0x00, 0x03, 0x0D, 0x40, 0xFF, 0xEC};
    uint32_t rx_freq = Endian_ReadU32BE(rx_packet);
    // → 200000 Hz
    uint16_t rx_rssi = Endian_ReadU16BE(rx_packet + 4);
    // → 0xFFEC = -20 dBm (en complément à 2)

    // ===== CONVERSION RÉSEAU =====
    uint32_t host_val = 0x12345678;
    uint32_t net_val = HTON32(host_val);  // → 0x78563412
    uint32_t back = NTOH32(net_val);      // → 0x12345678

    // ===== SWAP BUFFER ENTIER =====
    uint32_t words[] = {0x12345678, 0xABCDEF00, 0x11223344};
    Endian_Swap32Buffer(words, 3);
    // words = {0x78563412, 0x00EFCDAB, 0x44332211}

    // ===== FLOAT =====
    float temperature = 25.5f;
    Endian_WriteFloatBE(buffer, temperature);
    float decoded = Endian_ReadFloatBE(buffer);
    // decoded == 25.5f

    // ===== STRUCTURE =====
    struct __attribute__((packed)) ConfigPacket {
        uint32_t frequency;
        uint16_t power;
        uint8_t  spreading_factor;
        uint16_t bandwidth;
    } config;

    // Remplir depuis le réseau (big-endian)
    config.frequency = Endian_ReadU32BE(rx_buffer);
    config.power = Endian_ReadU16BE(rx_buffer + 4);
    config.spreading_factor = rx_buffer[6];  // 1 octet, pas de swap
    config.bandwidth = Endian_ReadU16BE(rx_buffer + 7);

    // Écrire pour le réseau
    Endian_WriteU32BE(tx_buffer, config.frequency);
    Endian_WriteU16BE(tx_buffer + 4, config.power);
    tx_buffer[6] = config.spreading_factor;
    Endian_WriteU16BE(tx_buffer + 7, config.bandwidth);
}

#endif

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */