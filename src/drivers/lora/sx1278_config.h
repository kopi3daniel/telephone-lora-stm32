/**
 * @file sx1278_config.h
 * @brief Configurations prédéfinies pour le module SX1278
 * 
 * Ce fichier contient des profils de configuration prêts à l'emploi
 * pour différents cas d'usage :
 * - Audio temps réel (faible latence)
 * - Longue portée (SF12, portée maximale)
 * - Équilibré (bon compromis)
 * - Économie d'énergie (basse consommation)
 * - Haut débit (transmission rapide)
 * 
 * Chaque profil est optimisé pour un usage spécifique.
 * Il suffit d'appeler sx1278_hal_configure(&profile) pour l'appliquer.
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SX1278_CONFIG_H
#define SX1278_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "sx1278_defs.h"
#include "sx1278_hal.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================
// SECTION 1 : TYPES DE PROFILS
// ============================================================

/**
 * @brief Type de profil de configuration
 */
typedef enum {
    PROFILE_AUDIO,          // Audio temps réel (faible latence)
    PROFILE_LONG_RANGE,     // Longue portée (max distance)
    PROFILE_BALANCED,       // Équilibré (bon compromis)
    PROFILE_LOW_POWER,      // Économie d'énergie
    PROFILE_HIGH_SPEED,     // Haut débit
    PROFILE_SMS,            // Messages texte
    PROFILE_CUSTOM          // Personnalisé
} SX1278_ProfileType;

// ============================================================
// SECTION 2 : STRUCTURE DE PROFIL ÉTENDU
// ============================================================

/**
 * @brief Structure complète d'un profil de configuration
 */
typedef struct {
    SX1278_ProfileType type;        // Type de profil
    const char* name;               // Nom du profil
    const char* description;        // Description
    
    // Paramètres de base
    uint32_t frequency;             // Fréquence en Hz
    uint8_t spreadingFactor;        // Spreading Factor (6-12)
    uint32_t bandwidth;             // Bande passante en Hz
    uint8_t codingRate;             // Coding Rate (5-8)
    uint8_t txPower;                // Puissance d'émission (dBm)
    
    // Paramètres avancés
    uint16_t preambleLength;        // Longueur préambule (symboles)
    uint8_t syncWord;               // Mot de synchronisation
    bool crcEnabled;                // CRC activé
    bool implicitHeader;            // Header implicite
    bool lowDataRateOptimize;       // Optimisation bas débit
    
    // Paramètres RF
    uint8_t lnaGain;                // Gain LNA
    bool lnaBoostHf;                // Boost LNA HF
    bool paBoost;                   // PA Boost
    uint8_t paDac;                  // Configuration PA DAC
    
    // Timings
    uint32_t txTimeoutMs;           // Timeout transmission (ms)
    uint32_t rxTimeoutMs;           // Timeout réception (ms)
    uint32_t symbolTimeout;         // Timeout symbole (nombre de symboles)
    
    // Performances estimées
    uint32_t estimatedBitrate;      // Débit estimé (bps)
    uint32_t estimatedRange;        // Portée estimée (mètres)
    uint32_t estimatedTxTime;       // Temps TX estimé (ms pour 64 octets)
    float sensitivity;              // Sensibilité estimée (dBm)
    
} SX1278_Profile;

// ============================================================
// SECTION 3 : PROFILS PRÉDÉFINIS
// ============================================================

/**
 * @brief Profil Audio - Optimisé pour la voix en temps réel
 * 
 * Caractéristiques :
 * - SF7, BW 125 kHz → Faible latence (~100 ms)
 * - Débit : 5470 bps
 * - Portée : ~2 km en ville, ~8 km en campagne
 * - Idéal pour les appels vocaux
 */
static const SX1278_Profile PROFILE_AUDIO_CONFIG = {
    .type = PROFILE_AUDIO,
    .name = "Audio temps réel",
    .description = "Optimisé pour la voix, faible latence",
    
    .frequency = 868000000,
    .spreadingFactor = 7,
    .bandwidth = 125000,
    .codingRate = 5,            // 4/5 (protection minimale, débit max)
    .txPower = 17,
    
    .preambleLength = 8,
    .syncWord = 0x34,           // Public
    .crcEnabled = false,        // Pas de CRC pour l'audio (latence)
    .implicitHeader = false,
    .lowDataRateOptimize = false,
    
    .lnaGain = LNA_GAIN_G1,     // Gain maximum
    .lnaBoostHf = true,
    .paBoost = true,
    .paDac = PA_DAC_HIGH_POWER,
    
    .txTimeoutMs = 2000,
    .rxTimeoutMs = 5000,
    .symbolTimeout = 0x3FF,     // Maximum
    
    .estimatedBitrate = 5470,
    .estimatedRange = 2000,
    .estimatedTxTime = 82,      // 82 ms pour 64 octets
    .sensitivity = -123.0f
};

/**
 * @brief Profil Longue Portée - Portée maximale
 * 
 * Caractéristiques :
 * - SF12, BW 125 kHz → Portée maximale
 * - Débit : 293 bps
 * - Portée : ~5 km en ville, ~15 km en campagne
 * - Idéal pour les SMS longue distance
 */
static const SX1278_Profile PROFILE_LONG_RANGE_CONFIG = {
    .type = PROFILE_LONG_RANGE,
    .name = "Longue portée",
    .description = "Portée maximale, débit très faible",
    
    .frequency = 868000000,
    .spreadingFactor = 12,
    .bandwidth = 125000,
    .codingRate = 8,            // 4/8 (protection maximale)
    .txPower = 20,              // Puissance max
    
    .preambleLength = 12,       // Préambule plus long
    .syncWord = 0x34,
    .crcEnabled = true,
    .implicitHeader = false,
    .lowDataRateOptimize = true, // Important pour SF12 !
    
    .lnaGain = LNA_GAIN_G1,
    .lnaBoostHf = true,
    .paBoost = true,
    .paDac = PA_DAC_HIGH_POWER,
    
    .txTimeoutMs = 10000,       // Timeout plus long
    .rxTimeoutMs = 30000,
    .symbolTimeout = 0x3FF,
    
    .estimatedBitrate = 293,
    .estimatedRange = 15000,
    .estimatedTxTime = 1582,    // ~1.6 secondes pour 64 octets
    .sensitivity = -137.0f
};

/**
 * @brief Profil Équilibré - Bon compromis
 * 
 * Caractéristiques :
 * - SF9, BW 125 kHz → Bon équilibre portée/débit
 * - Débit : 1757 bps
 * - Portée : ~3.5 km en ville, ~10 km en campagne
 * - Usage général
 */
static const SX1278_Profile PROFILE_BALANCED_CONFIG = {
    .type = PROFILE_BALANCED,
    .name = "Équilibré",
    .description = "Bon compromis portée/débit",
    
    .frequency = 868000000,
    .spreadingFactor = 9,
    .bandwidth = 125000,
    .codingRate = 6,            // 4/6
    .txPower = 17,
    
    .preambleLength = 8,
    .syncWord = 0x34,
    .crcEnabled = true,
    .implicitHeader = false,
    .lowDataRateOptimize = false,
    
    .lnaGain = LNA_GAIN_G1,
    .lnaBoostHf = true,
    .paBoost = true,
    .paDac = PA_DAC_HIGH_POWER,
    
    .txTimeoutMs = 5000,
    .rxTimeoutMs = 10000,
    .symbolTimeout = 0x3FF,
    
    .estimatedBitrate = 1757,
    .estimatedRange = 3500,
    .estimatedTxTime = 247,
    .sensitivity = -129.0f
};

/**
 * @brief Profil Économie d'énergie
 * 
 * Caractéristiques :
 * - SF7, BW 250 kHz → Temps d'émission court
 * - Faible puissance (10 dBm)
 * - Consommation réduite
 */
static const SX1278_Profile PROFILE_LOW_POWER_CONFIG = {
    .type = PROFILE_LOW_POWER,
    .name = "Économie d'énergie",
    .description = "Basse consommation, courte portée",
    
    .frequency = 868000000,
    .spreadingFactor = 7,
    .bandwidth = 250000,
    .codingRate = 5,
    .txPower = 10,              // Puissance réduite
    
    .preambleLength = 6,
    .syncWord = 0x34,
    .crcEnabled = true,
    .implicitHeader = false,
    .lowDataRateOptimize = false,
    
    .lnaGain = LNA_GAIN_G1,
    .lnaBoostHf = false,
    .paBoost = false,
    .paDac = PA_DAC_LOW_POWER,
    
    .txTimeoutMs = 1000,
    .rxTimeoutMs = 5000,
    .symbolTimeout = 0x1FF,
    
    .estimatedBitrate = 10940,
    .estimatedRange = 500,
    .estimatedTxTime = 41,
    .sensitivity = -117.0f
};

/**
 * @brief Profil Haut Débit
 * 
 * Caractéristiques :
 * - SF6, BW 500 kHz → Débit maximal
 * - Débit : 37500 bps
 * - Portée : ~500 m
 * - Usage : Transfert de fichiers, mise à jour firmware
 */
static const SX1278_Profile PROFILE_HIGH_SPEED_CONFIG = {
    .type = PROFILE_HIGH_SPEED,
    .name = "Haut débit",
    .description = "Débit maximal, très courte portée",
    
    .frequency = 868000000,
    .spreadingFactor = 6,       // SF6 = débit max
    .bandwidth = 500000,        // Bande passante max
    .codingRate = 5,
    .txPower = 17,
    
    .preambleLength = 6,
    .syncWord = 0x34,
    .crcEnabled = true,
    .implicitHeader = true,     // Header implicite pour SF6
    .lowDataRateOptimize = false,
    
    .lnaGain = LNA_GAIN_G1,
    .lnaBoostHf = true,
    .paBoost = true,
    .paDac = PA_DAC_HIGH_POWER,
    
    .txTimeoutMs = 2000,
    .rxTimeoutMs = 5000,
    .symbolTimeout = 0x0FF,
    
    .estimatedBitrate = 37500,
    .estimatedRange = 500,
    .estimatedTxTime = 14,      // 14 ms pour 64 octets
    .sensitivity = -111.0f
};

/**
 * @brief Profil SMS - Optimisé pour les messages texte
 * 
 * Caractéristiques :
 * - SF10, BW 125 kHz → Bonne portée, débit correct
 * - Débit : 976 bps
 * - Portée : ~4 km en ville
 */
static const SX1278_Profile PROFILE_SMS_CONFIG = {
    .type = PROFILE_SMS,
    .name = "Messagerie SMS",
    .description = "Optimisé pour les messages texte",
    
    .frequency = 868000000,
    .spreadingFactor = 10,
    .bandwidth = 125000,
    .codingRate = 6,
    .txPower = 17,
    
    .preambleLength = 8,
    .syncWord = 0x34,
    .crcEnabled = true,         // CRC important pour les messages
    .implicitHeader = false,
    .lowDataRateOptimize = false,
    
    .lnaGain = LNA_GAIN_G1,
    .lnaBoostHf = true,
    .paBoost = true,
    .paDac = PA_DAC_HIGH_POWER,
    
    .txTimeoutMs = 5000,
    .rxTimeoutMs = 15000,
    .symbolTimeout = 0x3FF,
    
    .estimatedBitrate = 976,
    .estimatedRange = 4000,
    .estimatedTxTime = 495,
    .sensitivity = -132.0f
};

// ============================================================
// SECTION 4 : TABLEAU DES PROFILS
// ============================================================

/**
 * @brief Tableau de tous les profils disponibles
 * 
 * Indexé par SX1278_ProfileType
 */
static const SX1278_Profile* const PROFILES[] = {
    [PROFILE_AUDIO]      = &PROFILE_AUDIO_CONFIG,
    [PROFILE_LONG_RANGE] = &PROFILE_LONG_RANGE_CONFIG,
    [PROFILE_BALANCED]   = &PROFILE_BALANCED_CONFIG,
    [PROFILE_LOW_POWER]  = &PROFILE_LOW_POWER_CONFIG,
    [PROFILE_HIGH_SPEED] = &PROFILE_HIGH_SPEED_CONFIG,
    [PROFILE_SMS]        = &PROFILE_SMS_CONFIG,
};

/** @brief Nombre de profils disponibles */
#define PROFILE_COUNT   (sizeof(PROFILES) / sizeof(PROFILES[0]))

// ============================================================
// SECTION 5 : FONCTIONS DE GESTION DES PROFILS
// ============================================================

/**
 * @brief Applique un profil de configuration au module SX1278
 * 
 * Configure tous les paramètres du module selon le profil choisi.
 * 
 * @param profile Profil à appliquer
 * @return SX1278_OK si succès, code d'erreur sinon
 */
SX1278_Error sx1278_config_apply_profile(const SX1278_Profile* profile);

/**
 * @brief Récupère un profil par son type
 * 
 * @param type Type de profil
 * @return Pointeur vers le profil, NULL si invalide
 */
const SX1278_Profile* sx1278_config_get_profile(SX1278_ProfileType type);

/**
 * @brief Récupère le profil actuellement actif
 * 
 * @return Pointeur vers le profil actif
 */
const SX1278_Profile* sx1278_config_get_active_profile(void);

/**
 * @brief Crée un profil personnalisé
 * 
 * @param profile Structure à remplir
 * @param sf Spreading Factor
 * @param bw Bande passante
 * @param cr Coding Rate
 * @param power Puissance
 */
void sx1278_config_create_custom(SX1278_Profile* profile,
                                  uint8_t sf, uint32_t bw, 
                                  uint8_t cr, uint8_t power);

/**
 * @brief Affiche les détails d'un profil
 * 
 * @param profile Profil à afficher
 */
void sx1278_config_print_profile(const SX1278_Profile* profile);

/**
 * @brief Affiche tous les profils disponibles
 */
void sx1278_config_print_all_profiles(void);

/**
 * @brief Compare deux profils
 * 
 * @param a Premier profil
 * @param b Second profil
 * @return 0 si identiques, non-zéro sinon
 */
int sx1278_config_compare_profiles(const SX1278_Profile* a, 
                                    const SX1278_Profile* b);

/**
 * @brief Estime le temps d'occupation de l'air
 * 
 * @param profile Profil utilisé
 * @param payloadLength Longueur du payload en octets
 * @return Temps en millisecondes
 */
uint32_t sx1278_config_estimate_time_on_air(const SX1278_Profile* profile,
                                              uint16_t payloadLength);

/**
 * @brief Calcule le débit effectif
 * 
 * @param profile Profil utilisé
 * @return Débit en bits par seconde
 */
uint32_t sx1278_config_calculate_bitrate(const SX1278_Profile* profile);

// ============================================================
// SECTION 6 : MACROS DE CHANGEMENT RAPIDE DE PROFIL
// ============================================================

/**
 * @brief Passe en mode Audio (appel vocal)
 */
#define SX1278_SWITCH_TO_AUDIO() \
    sx1278_config_apply_profile(&PROFILE_AUDIO_CONFIG)

/**
 * @brief Passe en mode Longue Portée
 */
#define SX1278_SWITCH_TO_LONG_RANGE() \
    sx1278_config_apply_profile(&PROFILE_LONG_RANGE_CONFIG)

/**
 * @brief Passe en mode Équilibré
 */
#define SX1278_SWITCH_TO_BALANCED() \
    sx1278_config_apply_profile(&PROFILE_BALANCED_CONFIG)

/**
 * @brief Passe en mode Économie d'énergie
 */
#define SX1278_SWITCH_TO_LOW_POWER() \
    sx1278_config_apply_profile(&PROFILE_LOW_POWER_CONFIG)

/**
 * @brief Passe en mode SMS
 */
#define SX1278_SWITCH_TO_SMS() \
    sx1278_config_apply_profile(&PROFILE_SMS_CONFIG)

// ============================================================
// SECTION 7 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SX1278_CONFIG_H