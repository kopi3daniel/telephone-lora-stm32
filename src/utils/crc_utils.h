/**
 * @file    crc_utils.h
 * @brief   Utilitaires de calcul CRC - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Fournit des fonctions de calcul de CRC (Cyclic Redundancy Check)
 * pour la détection d'erreurs dans les transmissions.
 * 
 * CONTEXTE D'UTILISATION :
 * 
 *   - Vérification intégrité paquets LoRa (CRC16)
 *   - Vérification firmware update (CRC32)
 *   - Empreinte rapide pour les paramètres (CRC8)
 *   - Vérification données flash (CRC16/CRC32)
 * 
 * TYPES DE CRC SUPPORTÉS :
 * 
 *   1. CRC8 :
 *      - 8 bits, polynômes standards
 *      - DALLAS/Maxim (1-Wire) : 0x31
 *      - AUTOSAR : 0x2F
 *      - Bluetooth : 0xA7
 * 
 *   2. CRC16 :
 *      - 16 bits, le plus utilisé
 *      - CCITT (XMODEM) : 0x1021
 *      - MODBUS : 0x8005
 *      - IBM (USB) : 0x8005
 * 
 *   3. CRC32 :
 *      - 32 bits, robustesse maximale
 *      - Ethernet/ZIP : 0x04C11DB7
 *      - MPEG2 : 0x04C11DB7
 *      - CKSUM (BSD) : 0x04C11DB7
 * 
 * OPTIMISATIONS :
 * 
 *   1. Table précalculée (256 entrées) :
 *      - Calcul en O(n) au lieu de O(n×8)
 *      - ~8x plus rapide que le calcul bit-à-bit
 * 
 *   2. Accélération matérielle (CRC unit du STM32F4) :
 *      - CRC32 hardware (1 cycle par mot)
 *      - Utilisé automatiquement si disponible
 * 
 *   3. DMA possible pour gros blocs
 * 
 * EXEMPLES D'UTILISATION :
 * 
 *   // CRC16 CCITT d'un paquet
 *   uint16_t crc = CRC16_Calculate(data, length, CRC16_CCITT);
 * 
 *   // CRC32 matériel (très rapide)
 *   uint32_t crc = CRC32_Hardware(data, length);
 * 
 *   // Vérification rapide
 *   bool ok = CRC16_Check(data, length, expected_crc, CRC16_CCITT);
 */

#ifndef CRC_UTILS_H
#define CRC_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ======================================================================== */
/*                     TYPES DE CRC                                          */
/* ======================================================================== */

/**
 * @brief Configuration d'un CRC
 */
typedef struct {
    uint32_t    polynomial;         /**< Polynôme générateur                  */
    uint32_t    initial_value;      /**< Valeur initiale                      */
    uint32_t    final_xor;          /**< XOR final                            */
    bool        reflect_input;      /**< Inverser les bits en entrée          */
    bool        reflect_output;     /**< Inverser les bits en sortie          */
    uint8_t     width;              /**< Largeur (8, 16, 32)                  */
} CRCConfig_t;

/* ======================================================================== */
/*              CONFIGURATIONS STANDARDS                                     */
/* ======================================================================== */

/** CRC8 DALLAS/Maxim (1-Wire, DS18B20) */
#define CRC8_DALLAS_POLY                    0x31
#define CRC8_DALLAS_INIT                    0x00

/** CRC8 AUTOSAR */
#define CRC8_AUTOSAR_POLY                   0x2F
#define CRC8_AUTOSAR_INIT                   0xFF

/** CRC8 Bluetooth */
#define CRC8_BLUETOOTH_POLY                 0xA7
#define CRC8_BLUETOOTH_INIT                 0x00

/** CRC16 CCITT (XMODEM, X.25, Bluetooth) */
#define CRC16_CCITT_POLY                    0x1021
#define CRC16_CCITT_INIT                    0x0000

/** CRC16 MODBUS */
#define CRC16_MODBUS_POLY                   0x8005
#define CRC16_MODBUS_INIT                   0xFFFF

/** CRC16 IBM (USB, ANSI) */
#define CRC16_IBM_POLY                      0x8005
#define CRC16_IBM_INIT                      0x0000

/** CRC32 Ethernet (ZIP, PNG, gzip) */
#define CRC32_ETHERNET_POLY                 0x04C11DB7
#define CRC32_ETHERNET_INIT                 0xFFFFFFFF

/** CRC32 MPEG2 */
#define CRC32_MPEG2_POLY                    0x04C11DB7
#define CRC32_MPEG2_INIT                    0xFFFFFFFF

/* ======================================================================== */
/*              CONFIGURATIONS PRÉDÉFINIES                                  */
/* ======================================================================== */

/** Configuration CRC8 DALLAS */
extern const CRCConfig_t CRC8_DALLAS;

/** Configuration CRC8 AUTOSAR */
extern const CRCConfig_t CRC8_AUTOSAR;

/** Configuration CRC16 CCITT */
extern const CRCConfig_t CRC16_CCITT;

/** Configuration CRC16 MODBUS */
extern const CRCConfig_t CRC16_MODBUS;

/** Configuration CRC32 Ethernet */
extern const CRCConfig_t CRC32_ETHERNET;

/** Configuration CRC32 MPEG2 */
extern const CRCConfig_t CRC32_MPEG2;

/* ======================================================================== */
/*              PROTOTYPES - CRC8                                           */
/* ======================================================================== */

/**
 * @brief Calcule le CRC8 d'un buffer
 * 
 * @param data          Données
 * @param length        Longueur
 * @param polynomial    Polynôme (ex: CRC8_DALLAS_POLY)
 * @param initial       Valeur initiale
 * @return              CRC 8 bits
 */
uint8_t CRC8_Calculate(const uint8_t* data, size_t length,
                       uint8_t polynomial, uint8_t initial);

/**
 * @brief Calcule le CRC8 avec une configuration complète
 * 
 * @param data          Données
 * @param length        Longueur
 * @param config        Configuration
 * @return              CRC 8 bits
 */
uint8_t CRC8_CalculateEx(const uint8_t* data, size_t length,
                         const CRCConfig_t* config);

/**
 * @brief Vérifie un CRC8
 * 
 * @param data          Données
 * @param length        Longueur (incluant le CRC)
 * @param polynomial    Polynôme
 * @param initial       Valeur initiale
 * @return              true si CRC correct
 */
bool CRC8_Check(const uint8_t* data, size_t length,
                uint8_t polynomial, uint8_t initial);

/**
 * @brief CRC8 DALLAS (1-Wire)
 * 
 * Compatible avec DS18B20, capteurs 1-Wire.
 * 
 * @param data          Données
 * @param length        Longueur
 * @return              CRC8 DALLAS
 */
uint8_t CRC8_Dallas(const uint8_t* data, size_t length);

/* ======================================================================== */
/*              PROTOTYPES - CRC16                                          */
/* ======================================================================== */

/**
 * @brief Calcule le CRC16 d'un buffer
 * 
 * @param data          Données
 * @param length        Longueur
 * @param polynomial    Polynôme (ex: CRC16_CCITT_POLY)
 * @param initial       Valeur initiale
 * @return              CRC 16 bits
 */
uint16_t CRC16_Calculate(const uint8_t* data, size_t length,
                         uint16_t polynomial, uint16_t initial);

/**
 * @brief Calcule le CRC16 avec configuration complète
 * 
 * @param data          Données
 * @param length        Longueur
 * @param config        Configuration
 * @return              CRC 16 bits
 */
uint16_t CRC16_CalculateEx(const uint8_t* data, size_t length,
                           const CRCConfig_t* config);

/**
 * @brief Vérifie un CRC16
 * 
 * @param data          Données
 * @param length        Longueur (incluant le CRC)
 * @param polynomial    Polynôme
 * @param initial       Valeur initiale
 * @return              true si CRC correct
 */
bool CRC16_Check(const uint8_t* data, size_t length,
                 uint16_t polynomial, uint16_t initial);

/**
 * @brief CRC16 CCITT (recommandé pour communications)
 * 
 * Utilisé dans : XMODEM, X.25, Bluetooth, SD cards
 * 
 * @param data          Données
 * @param length        Longueur
 * @return              CRC16 CCITT
 */
uint16_t CRC16_CCITT(const uint8_t* data, size_t length);

/**
 * @brief CRC16 MODBUS
 * 
 * Utilisé dans : MODBUS RTU, industrie
 * 
 * @param data          Données
 * @param length        Longueur
 * @return              CRC16 MODBUS
 */
uint16_t CRC16_Modbus(const uint8_t* data, size_t length);

/**
 * @brief CRC16 avec table précalculée (rapide)
 * 
 * @param data          Données
 * @param length        Longueur
 * @param table         Table précalculée (256 entrées uint16_t)
 * @return              CRC 16 bits
 */
uint16_t CRC16_TableLookup(const uint8_t* data, size_t length,
                           const uint16_t* table);

/**
 * @brief Génère une table CRC16
 * 
 * @param table         [out] Table (256 uint16_t)
 * @param polynomial    Polynôme
 */
void CRC16_GenerateTable(uint16_t* table, uint16_t polynomial);

/* ======================================================================== */
/*              PROTOTYPES - CRC32                                          */
/* ======================================================================== */

/**
 * @brief Calcule le CRC32 d'un buffer (logiciel)
 * 
 * @param data          Données
 * @param length        Longueur
 * @param polynomial    Polynôme
 * @param initial       Valeur initiale
 * @return              CRC 32 bits
 */
uint32_t CRC32_Calculate(const uint8_t* data, size_t length,
                         uint32_t polynomial, uint32_t initial);

/**
 * @brief Calcule le CRC32 avec configuration complète
 * 
 * @param data          Données
 * @param length        Longueur
 * @param config        Configuration
 * @return              CRC 32 bits
 */
uint32_t CRC32_CalculateEx(const uint8_t* data, size_t length,
                           const CRCConfig_t* config);

/**
 * @brief Vérifie un CRC32
 * 
 * @param data          Données
 * @param length        Longueur (incluant le CRC)
 * @param polynomial    Polynôme
 * @param initial       Valeur initiale
 * @return              true si CRC correct
 */
bool CRC32_Check(const uint8_t* data, size_t length,
                 uint32_t polynomial, uint32_t initial);

/**
 * @brief CRC32 Ethernet (le plus courant)
 * 
 * @param data          Données
 * @param length        Longueur
 * @return              CRC32 Ethernet
 */
uint32_t CRC32_Ethernet(const uint8_t* data, size_t length);

/**
 * @brief CRC32 matériel (utilise le CRC unit du STM32F4)
 * 
 * ⚠️ TRÈS RAPIDE : ~1 cycle/mot en 32 bits
 * ⚠️ Attention au padding et à l'alignement
 * 
 * @param data          Données (alignées 32 bits recommandé)
 * @param length        Longueur en octets
 * @return              CRC32 matériel
 */
uint32_t CRC32_Hardware(const uint32_t* data, size_t length);

/**
 * @brief Initialise l'unité CRC matérielle
 */
void CRC32_HardwareInit(void);

/**
 * @brief Calcule le CRC32 matériel d'un seul mot
 * 
 * @param word          Mot 32 bits
 * @return              CRC accumulé
 */
uint32_t CRC32_HardwareWord(uint32_t word);

/**
 * @brief Réinitialise l'accumulateur CRC matériel
 */
void CRC32_HardwareReset(void);

/**
 * @brief CRC32 avec table précalculée (rapide logiciel)
 * 
 * @param data          Données
 * @param length        Longueur
 * @param table         Table (256 uint32_t)
 * @return              CRC 32 bits
 */
uint32_t CRC32_TableLookup(const uint8_t* data, size_t length,
                           const uint32_t* table);

/**
 * @brief Génère une table CRC32
 * 
 * @param table         [out] Table (256 uint32_t)
 * @param polynomial    Polynôme
 */
void CRC32_GenerateTable(uint32_t* table, uint32_t polynomial);

/* ======================================================================== */
/*              PROTOTYPES - UTILITAIRES                                    */
/* ======================================================================== */

/**
 * @brief Ajoute un CRC16 à la fin d'un buffer
 * 
 * @param data          Buffer (doit avoir assez de place pour +2 octets)
 * @param length        Longueur des données (sera incrémentée de 2)
 * @param polynomial    Polynôme
 * @param initial       Valeur initiale
 */
void CRC16_Append(uint8_t* data, size_t* length,
                  uint16_t polynomial, uint16_t initial);

/**
 * @brief Extrait le CRC16 de la fin d'un buffer
 * 
 * @param data          Buffer
 * @param length        Longueur totale (données + CRC)
 * @return              CRC extrait
 */
uint16_t CRC16_Extract(const uint8_t* data, size_t length);

/**
 * @brief Ajoute un CRC32 à la fin d'un buffer
 * 
 * @param data          Buffer (+4 octets)
 * @param length        Longueur (incrémentée de 4)
 * @param polynomial    Polynôme
 * @param initial       Valeur initiale
 */
void CRC32_Append(uint8_t* data, size_t* length,
                  uint32_t polynomial, uint32_t initial);

/**
 * @brief Extrait le CRC32 de la fin d'un buffer
 * 
 * @param data          Buffer
 * @param length        Longueur totale
 * @return              CRC extrait
 */
uint32_t CRC32_Extract(const uint8_t* data, size_t length);

/**
 * @brief Formate un CRC en chaîne hexadécimale
 * 
 * @param dst           Buffer destination (assez grand)
 * @param crc           Valeur CRC
 * @param width         Largeur (8, 16, 32)
 * @return              Pointeur vers dst
 */
char* CRC_FormatHex(char* dst, uint32_t crc, uint8_t width);

/* ======================================================================== */
/*              MACROS UTILITAIRES                                           */
/* ======================================================================== */

/**
 * @brief Calcule le CRC16 CCITT d'un buffer (macro pratique)
 */
#define CRC16_CCITT_BUFFER(data, len)       CRC16_CCITT((data), (len))

/**
 * @brief Calcule le CRC32 Ethernet (macro pratique)
 */
#define CRC32_ETHERNET_BUFFER(data, len)    CRC32_Ethernet((data), (len))

/**
 * @brief Vérification rapide CRC16 CCITT
 */
#define CRC16_CHECK(data, len, expected) \
    CRC16_Check((data), (len), (expected), CRC16_CCITT_POLY, CRC16_CCITT_INIT)

/* ======================================================================== */
/*              TABLEAUX PRÉCALCULÉS (si espace disponible)                  */
/* ======================================================================== */

/**
 * @brief Table CRC16 CCITT précalculée (512 octets)
 * 
 * Définir CRC_USE_PRECALCULATED_TABLES dans project_config.h
 * pour utiliser les tables précalculées.
 */
#ifdef CRC_USE_PRECALCULATED_TABLES
    extern const uint16_t CRC16_CCITT_TABLE[256];
    extern const uint32_t CRC32_ETHERNET_TABLE[256];
#endif

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */