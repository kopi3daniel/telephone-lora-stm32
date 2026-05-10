/**
 * @file    endian_utils.h
 * @brief   Utilitaires de conversion endianness - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Fournit des fonctions de conversion entre big-endian et little-endian.
 * 
 * CONTEXTE :
 * 
 * Le STM32F429 est little-endian (comme tous les Cortex-M).
 * Les communications réseau (LoRa, protocoles) utilisent généralement
 * le big-endian (network byte order).
 * 
 * Une conversion est nécessaire pour :
 *   - Les paquets LoRa (big-endian sur le réseau)
 *   - Les données multi-octets (uint16, uint32, uint64)
 *   - Les structures sérialisées
 *   - L'interopérabilité avec d'autres systèmes
 * 
 * INSTRUCTIONS MATÉRIELLES (Cortex-M4) :
 * 
 *   REV   : Inverse les octets d'un mot 32 bits (1 cycle)
 *   REV16 : Inverse les octets de chaque demi-mot 16 bits (1 cycle)
 *   REVSH : Inverse les octets d'un demi-mot signé (1 cycle)
 * 
 *   Ces instructions sont utilisées via les builtins GCC :
 *     __REV()   → uint32_t
 *     __REV16() → uint32_t (chaque half-word)
 *     __REVSH() → int32_t (signé 16 bits)
 * 
 * EXEMPLES :
 * 
 *   Little-endian (STM32) : 0x12345678 stocké comme 78 56 34 12
 *   Big-endian (Réseau)    : 0x12345678 stocké comme 12 34 56 78
 * 
 *   Conversion :
 *     uint32_t le = 0x12345678;
 *     uint32_t be = ENDIAN_SWAP32(le);  // → 0x78563412
 */

#ifndef ENDIAN_UTILS_H
#define ENDIAN_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* Architecture CMSIS (pour __REV, __REV16, __REVSH) */
#include "cmsis_compiler.h"

/* ======================================================================== */
/*                     DÉTECTION ENDIANNESS                                  */
/* ======================================================================== */

/**
 * @brief Vérifie si le système est little-endian
 * 
 * @return          true si little-endian (toujours true sur STM32)
 */
bool Endian_IsLittleEndian(void);

/**
 * @brief Vérifie si le système est big-endian
 * 
 * @return          true si big-endian (toujours false sur STM32)
 */
bool Endian_IsBigEndian(void);

/* ======================================================================== */
/*              PROTOTYPES - SWAP 16 BITS                                   */
/* ======================================================================== */

/**
 * @brief Inverse les octets d'un uint16
 * 
 * 0x1234 → 0x3412
 * 
 * @param value     Valeur à inverser
 * @return          Valeur inversée
 */
uint16_t Endian_Swap16(uint16_t value);

/**
 * @brief Inverse les octets de chaque uint16 dans un buffer
 * 
 * @param data      Buffer (modifié sur place)
 * @param count     Nombre de uint16
 */
void Endian_Swap16Buffer(uint16_t* data, size_t count);

/* ======================================================================== */
/*              PROTOTYPES - SWAP 32 BITS                                   */
/* ======================================================================== */

/**
 * @brief Inverse les octets d'un uint32
 * 
 * 0x12345678 → 0x78563412
 * 
 * @param value     Valeur à inverser
 * @return          Valeur inversée
 */
uint32_t Endian_Swap32(uint32_t value);

/**
 * @brief Inverse les octets de chaque uint32 dans un buffer
 * 
 * @param data      Buffer
 * @param count     Nombre de uint32
 */
void Endian_Swap32Buffer(uint32_t* data, size_t count);

/* ======================================================================== */
/*              PROTOTYPES - SWAP 64 BITS                                   */
/* ======================================================================== */

/**
 * @brief Inverse les octets d'un uint64
 * 
 * @param value     Valeur à inverser
 * @return          Valeur inversée
 */
uint64_t Endian_Swap64(uint64_t value);

/**
 * @brief Inverse les octets de chaque uint64 dans un buffer
 * 
 * @param data      Buffer
 * @param count     Nombre de uint64
 */
void Endian_Swap64Buffer(uint64_t* data, size_t count);

/* ======================================================================== */
/*              PROTOTYPES - CONVERSION RÉSEAU (HOST ↔ NETWORK)             */
/* ======================================================================== */

/**
 * @brief Host → Network (uint16)
 * 
 * Sur little-endian : swap, sur big-endian : identité
 * 
 * @param host_value Valeur host
 * @return          Valeur network (big-endian)
 */
uint16_t Endian_HTON16(uint16_t host_value);

/**
 * @brief Network → Host (uint16)
 * 
 * @param net_value Valeur network (big-endian)
 * @return          Valeur host
 */
uint16_t Endian_NTOH16(uint16_t net_value);

/**
 * @brief Host → Network (uint32)
 * 
 * @param host_value Valeur host
 * @return          Valeur network
 */
uint32_t Endian_HTON32(uint32_t host_value);

/**
 * @brief Network → Host (uint32)
 * 
 * @param net_value Valeur network
 * @return          Valeur host
 */
uint32_t Endian_NTOH32(uint32_t net_value);

/**
 * @brief Host → Network (uint64)
 * 
 * @param host_value Valeur host
 * @return          Valeur network
 */
uint64_t Endian_HTON64(uint64_t host_value);

/**
 * @brief Network → Host (uint64)
 * 
 * @param net_value Valeur network
 * @return          Valeur host
 */
uint64_t Endian_NTOH64(uint64_t net_value);

/* ======================================================================== */
/*              PROTOTYPES - LECTURE/ÉCRITURE AVEC ENDIAN                   */
/* ======================================================================== */

/**
 * @brief Lit un uint16 big-endian depuis un buffer
 * 
 * @param buffer    Buffer source (big-endian)
 * @return          Valeur en ordre host
 */
uint16_t Endian_ReadU16BE(const uint8_t* buffer);

/**
 * @brief Lit un uint16 little-endian depuis un buffer
 * 
 * @param buffer    Buffer source (little-endian)
 * @return          Valeur en ordre host
 */
uint16_t Endian_ReadU16LE(const uint8_t* buffer);

/**
 * @brief Lit un uint32 big-endian depuis un buffer
 * 
 * @param buffer    Buffer source (big-endian)
 * @return          Valeur en ordre host
 */
uint32_t Endian_ReadU32BE(const uint8_t* buffer);

/**
 * @brief Lit un uint32 little-endian depuis un buffer
 * 
 * @param buffer    Buffer source (little-endian)
 * @return          Valeur en ordre host
 */
uint32_t Endian_ReadU32LE(const uint8_t* buffer);

/**
 * @brief Lit un uint64 big-endian depuis un buffer
 * 
 * @param buffer    Buffer source
 * @return          Valeur host
 */
uint64_t Endian_ReadU64BE(const uint8_t* buffer);

/**
 * @brief Lit un uint64 little-endian depuis un buffer
 * 
 * @param buffer    Buffer source
 * @return          Valeur host
 */
uint64_t Endian_ReadU64LE(const uint8_t* buffer);

/**
 * @brief Écrit un uint16 en big-endian dans un buffer
 * 
 * @param buffer    Buffer destination
 * @param value     Valeur host
 */
void Endian_WriteU16BE(uint8_t* buffer, uint16_t value);

/**
 * @brief Écrit un uint16 en little-endian dans un buffer
 * 
 * @param buffer    Buffer destination
 * @param value     Valeur host
 */
void Endian_WriteU16LE(uint8_t* buffer, uint16_t value);

/**
 * @brief Écrit un uint32 en big-endian dans un buffer
 * 
 * @param buffer    Buffer destination
 * @param value     Valeur host
 */
void Endian_WriteU32BE(uint8_t* buffer, uint32_t value);

/**
 * @brief Écrit un uint32 en little-endian dans un buffer
 * 
 * @param buffer    Buffer destination
 * @param value     Valeur host
 */
void Endian_WriteU32LE(uint8_t* buffer, uint32_t value);

/**
 * @brief Écrit un uint64 en big-endian dans un buffer
 * 
 * @param buffer    Buffer destination
 * @param value     Valeur host
 */
void Endian_WriteU64BE(uint8_t* buffer, uint64_t value);

/**
 * @brief Écrit un uint64 en little-endian dans un buffer
 * 
 * @param buffer    Buffer destination
 * @param value     Valeur host
 */
void Endian_WriteU64LE(uint8_t* buffer, uint64_t value);

/* ======================================================================== */
/*              PROTOTYPES - STRUCTURES                                     */
/* ======================================================================== */

/**
 * @brief Convertit une structure du réseau vers l'hôte
 * 
 * Parcourt les champs et applique les conversions nécessaires.
 * 
 * @param struct_ptr    Structure
 * @param field_sizes   Tableau des tailles de champs (0 = fin)
 *                      Exemple : {4, 2, 1, 4, 0}
 * @param count         Nombre de champs
 */
void Endian_SwapStruct(void* struct_ptr, const uint8_t* field_sizes, size_t count);

/**
 * @brief Lit une float big-endian depuis un buffer
 * 
 * @param buffer    Buffer source (big-endian)
 * @return          Float en ordre host
 */
float Endian_ReadFloatBE(const uint8_t* buffer);

/**
 * @brief Lit une float little-endian depuis un buffer
 * 
 * @param buffer    Buffer source
 * @return          Float host
 */
float Endian_ReadFloatLE(const uint8_t* buffer);

/**
 * @brief Écrit une float en big-endian dans un buffer
 * 
 * @param buffer    Buffer destination
 * @param value     Float host
 */
void Endian_WriteFloatBE(uint8_t* buffer, float value);

/**
 * @brief Écrit une float en little-endian dans un buffer
 * 
 * @param buffer    Buffer destination
 * @param value     Float host
 */
void Endian_WriteFloatLE(uint8_t* buffer, float value);

/* ======================================================================== */
/*              MACROS UTILITAIRES                                           */
/* ======================================================================== */

/**
 * @brief Inverse les octets d'un uint16 (macro rapide)
 */
#define ENDIAN_SWAP16(val) \
    ((uint16_t)((((val) & 0xFF00) >> 8) | \
                (((val) & 0x00FF) << 8)))

/**
 * @brief Inverse les octets d'un uint32 (macro rapide)
 */
#define ENDIAN_SWAP32(val) \
    ((uint32_t)((((val) & 0xFF000000) >> 24) | \
                (((val) & 0x00FF0000) >> 8)  | \
                (((val) & 0x0000FF00) << 8)  | \
                (((val) & 0x000000FF) << 24)))

/**
 * @brief Inverse les octets d'un uint64 (macro)
 */
#define ENDIAN_SWAP64(val) \
    ((uint64_t)((((val) & 0xFF00000000000000ULL) >> 56) | \
                (((val) & 0x00FF000000000000ULL) >> 40) | \
                (((val) & 0x0000FF0000000000ULL) >> 24) | \
                (((val) & 0x000000FF00000000ULL) >> 8)  | \
                (((val) & 0x00000000FF000000ULL) << 8)  | \
                (((val) & 0x0000000000FF0000ULL) << 24) | \
                (((val) & 0x000000000000FF00ULL) << 40) | \
                (((val) & 0x00000000000000FFULL) << 56)))

/**
 * @brief Host → Network 16 bits (macro rapide)
 */
#define HTON16(val)                         Endian_HTON16(val)

/**
 * @brief Network → Host 16 bits (macro rapide)
 */
#define NTOH16(val)                         Endian_NTOH16(val)

/**
 * @brief Host → Network 32 bits (macro rapide)
 */
#define HTON32(val)                         Endian_HTON32(val)

/**
 * @brief Network → Host 32 bits (macro rapide)
 */
#define NTOH32(val)                         Endian_NTOH32(val)

/**
 * @brief Lit un uint32 big-endian (macro)
 */
#define READ_U32_BE(buf)                    Endian_ReadU32BE(buf)

/**
 * @brief Écrit un uint32 big-endian (macro)
 */
#define WRITE_U32_BE(buf, val)              Endian_WriteU32BE((buf), (val))

/**
 * @brief Lit un uint16 big-endian (macro)
 */
#define READ_U16_BE(buf)                    Endian_ReadU16BE(buf)

/**
 * @brief Écrit un uint16 big-endian (macro)
 */
#define WRITE_U16_BE(buf, val)              Endian_WriteU16BE((buf), (val))

/* ======================================================================== */
/*              MACROS CONDITIONNELLES (selon architecture)                  */
/* ======================================================================== */

/* Sur STM32 (little-endian), HTON = swap */
/* Sur big-endian, HTON = identité */

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    /* Big-endian : pas de conversion nécessaire */
    #define ENDIAN_HTON16(val)              (val)
    #define ENDIAN_HTON32(val)              (val)
    #define ENDIAN_HTON64(val)              (val)
    #define ENDIAN_NTOH16(val)              (val)
    #define ENDIAN_NTOH32(val)              (val)
    #define ENDIAN_NTOH64(val)              (val)
#else
    /* Little-endian (STM32) : conversion nécessaire */
    #define ENDIAN_HTON16(val)              Endian_Swap16(val)
    #define ENDIAN_HTON32(val)              Endian_Swap32(val)
    #define ENDIAN_HTON64(val)              Endian_Swap64(val)
    #define ENDIAN_NTOH16(val)              Endian_Swap16(val)
    #define ENDIAN_NTOH32(val)              Endian_Swap32(val)
    #define ENDIAN_NTOH64(val)              Endian_Swap64(val)
#endif

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */