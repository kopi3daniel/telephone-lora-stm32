/**
 * @file sx1278_defs.h
 * @brief Définitions des registres et constantes du SX1278
 * 
 * Ce fichier contient TOUTES les définitions nécessaires pour
 * communiquer avec le module LoRa SX1278 (RA-02).
 * 
 * Référence : Datasheet SX1276/77/78/79 - Rev. 7 - Mai 2020
 * 
 * Le SX1278 est un émetteur-récepteur LoRa longue portée
 * fonctionnant dans les bandes 137-525 MHz et 862-1020 MHz.
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SX1278_DEFS_H
#define SX1278_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// SECTION 1 : ADRESSES DES REGISTRES
// ============================================================

/**
 * @name Registres communs (mode LoRa et FSK)
 * @{
 */

/** @brief Registre FIFO (lecture/écriture des données) */
#define REG_FIFO                        0x00

/** @brief Registre de mode de fonctionnement */
#define REG_OP_MODE                     0x01

/** @brief Registre de fréquence (MSB) */
#define REG_FRF_MSB                     0x06
/** @brief Registre de fréquence (MID) */
#define REG_FRF_MID                     0x07
/** @brief Registre de fréquence (LSB) */
#define REG_FRF_LSB                     0x08

/** @brief Configuration de l'amplificateur de puissance */
#define REG_PA_CONFIG                   0x09

/** @brief Configuration de la rampe PA */
#define REG_PA_RAMP                     0x0A

/** @brief Protection contre les surintensités (OCP) */
#define REG_OCP                         0x0B

/** @brief Configuration de l'amplificateur faible bruit (LNA) */
#define REG_LNA                         0x0C

/** @} */ // Fin registres communs

/**
 * @name Registres spécifiques au mode LoRa
 * @{
 */

/** @brief Pointeur d'adresse FIFO */
#define REG_FIFO_ADDR_PTR               0x0D

/** @brief Adresse de base TX dans la FIFO */
#define REG_FIFO_TX_BASE_ADDR           0x0E

/** @brief Adresse de base RX dans la FIFO */
#define REG_FIFO_RX_BASE_ADDR           0x0F

/** @brief Adresse courante RX dans la FIFO */
#define REG_FIFO_RX_CURRENT_ADDR        0x10

/** @brief Masque des flags d'interruption */
#define REG_IRQ_FLAGS_MASK              0x11

/** @brief Flags d'interruption */
#define REG_IRQ_FLAGS                   0x12

/** @brief Nombre d'octets reçus */
#define REG_RX_NB_BYTES                 0x13

/** @brief Compteur d'en-têtes valides reçus (MSB) */
#define REG_RX_HEADER_CNT_VALUE_MSB     0x14
/** @brief Compteur d'en-têtes valides reçus (LSB) */
#define REG_RX_HEADER_CNT_VALUE_LSB     0x15

/** @brief Compteur de paquets valides reçus (MSB) */
#define REG_RX_PACKET_CNT_VALUE_MSB     0x16
/** @brief Compteur de paquets valides reçus (LSB) */
#define REG_RX_PACKET_CNT_VALUE_LSB     0x17

/** @brief État du modem */
#define REG_MODEM_STAT                  0x18

/** @brief SNR du dernier paquet reçu */
#define REG_PKT_SNR_VALUE               0x19

/** @brief RSSI du dernier paquet reçu */
#define REG_PKT_RSSI_VALUE              0x1A

/** @brief RSSI instantané */
#define REG_RSSI_VALUE                  0x1B

/** @brief Canal de saut de fréquence */
#define REG_HOP_CHANNEL                 0x1C

/** @brief Configuration du modem 1 */
#define REG_MODEM_CONFIG1               0x1D

/** @brief Configuration du modem 2 */
#define REG_MODEM_CONFIG2               0x1E

/** @brief Timeout des symboles (LSB) */
#define REG_SYMB_TIMEOUT_LSB            0x1F

/** @brief Longueur du préambule (MSB) */
#define REG_PREAMBLE_MSB                0x20
/** @brief Longueur du préambule (LSB) */
#define REG_PREAMBLE_LSB                0x21

/** @brief Longueur du payload */
#define REG_PAYLOAD_LENGTH              0x22

/** @brief Longueur maximale du payload */
#define REG_MAX_PAYLOAD_LENGTH          0x23

/** @brief Période de saut de fréquence */
#define REG_HOP_PERIOD                  0x24

/** @brief Adresse de lecture du dernier octet FIFO */
#define REG_FIFO_RX_BYTE_ADDR           0x25

/** @brief Configuration du modem 3 */
#define REG_MODEM_CONFIG3               0x26

/** @brief Estimation de l'erreur de fréquence (MSB) */
#define REG_FEI_MSB                     0x28
/** @brief Estimation de l'erreur de fréquence (MID) */
#define REG_FEI_MID                     0x29
/** @brief Estimation de l'erreur de fréquence (LSB) */
#define REG_FEI_LSB                     0x2A

/** @brief RSSI large bande */
#define REG_RSSI_WIDEBAND               0x2C

/** @brief Optimisation de détection */
#define REG_DETECT_OPTIMIZE             0x31

/** @brief Inversion IQ */
#define REG_INVERTIQ                    0x33

/** @brief Seuil de détection */
#define REG_DETECTION_THRESHOLD         0x37

/** @brief Mot de synchronisation */
#define REG_SYNC_WORD                   0x39

/** @brief Inversion IQ 2 */
#define REG_INVERTIQ2                   0x3B

/** @brief Mapping DIO */
#define REG_DIO_MAPPING_1               0x40
#define REG_DIO_MAPPING_2               0x41

/** @brief Version du silicium */
#define REG_VERSION                     0x42

/** @brief Configuration PA Boost (pour puissance > 17 dBm) */
#define REG_PA_DAC                      0x4D

/** @} */ // Fin registres LoRa

// ============================================================
// SECTION 2 : MODES DE FONCTIONNEMENT (REG_OP_MODE)
// ============================================================

/**
 * @name Bits du registre OP_MODE (0x01)
 * @{
 */

/** @brief Mode Long Range (LoRa) - doit être activé en premier */
#define MODE_LONG_RANGE_MODE            0x80

/** @brief Mode modulation FSK/OOK (non utilisé) */
#define MODE_FSK_OOK                    0x00

/** @brief Accès aux registres FSK en mode LoRa */
#define MODE_ACCESS_SHARED_REG          0x40

/** @brief Masque des bits de mode */
#define MODE_MASK                       0x07

/** @brief Mode Sleep (consommation minimale) */
#define MODE_SLEEP                      0x00
/** @brief Mode Standby (oscillateur actif) */
#define MODE_STDBY                      0x01
/** @brief Mode Transmission Fréquence Synthétisée (FS) */
#define MODE_FSTX                       0x02
/** @brief Mode Transmission */
#define MODE_TX                         0x03
/** @brief Mode Réception Fréquence Synthétisée (FS) */
#define MODE_FSRX                       0x04
/** @brief Mode Réception Continue */
#define MODE_RX_CONTINUOUS              0x05
/** @brief Mode Réception Simple (une seule trame) */
#define MODE_RX_SINGLE                  0x06
/** @brief Mode Cadencement (Channel Activity Detection) */
#define MODE_CAD                        0x07

/** @} */ // Fin modes OP_MODE

// ============================================================
// SECTION 3 : FLAGS D'INTERRUPTION (REG_IRQ_FLAGS)
// ============================================================

/**
 * @name Bits du registre IRQ_FLAGS (0x12)
 * @{
 */

/** @brief Mode CAD détecté */
#define IRQ_CAD_DETECTED_MASK           0x01
/** @brief Signal FHSS changé */
#define IRQ_FHSS_CHANGE_CHANNEL_MASK    0x02
/** @brief Détection de débit de symboles CAD */
#define IRQ_CAD_DONE_MASK               0x04
/** @brief Transmission terminée */
#define IRQ_TX_DONE_MASK                0x08
/** @brief Timeout de réception */
#define IRQ_RX_TIMEOUT_MASK             0x80
/** @brief Paquet reçu avec succès */
#define IRQ_RX_DONE_MASK                0x40
/** @brief Erreur CRC dans le paquet reçu */
#define IRQ_CRC_ERROR_MASK              0x20
/** @brief Erreur de débit valide */
#define IRQ_VALID_HEADER_MASK           0x10

/** @brief Masque combiné pour les erreurs */
#define IRQ_ERROR_MASK                  (IRQ_CRC_ERROR_MASK | \
                                         IRQ_RX_TIMEOUT_MASK)

/** @brief Masque pour effacer tous les flags */
#define IRQ_CLEAR_ALL_MASK              0xFF

/** @} */ // Fin flags IRQ

// ============================================================
// SECTION 4 : CONFIGURATION DU MODEM (REG_MODEM_CONFIG1-3)
// ============================================================

/**
 * @name Configuration de la bande passante (REG_MODEM_CONFIG1)
 * @{
 */

/** @brief Bande passante = 7.8 kHz */
#define BW_7_8_KHZ                      0x00
/** @brief Bande passante = 10.4 kHz */
#define BW_10_4_KHZ                     0x10
/** @brief Bande passante = 15.6 kHz */
#define BW_15_6_KHZ                     0x20
/** @brief Bande passante = 20.8 kHz */
#define BW_20_8_KHZ                     0x30
/** @brief Bande passante = 31.25 kHz */
#define BW_31_25_KHZ                    0x40
/** @brief Bande passante = 41.7 kHz */
#define BW_41_7_KHZ                     0x50
/** @brief Bande passante = 62.5 kHz */
#define BW_62_5_KHZ                     0x60
/** @brief Bande passante = 125 kHz */
#define BW_125_KHZ                      0x70
/** @brief Bande passante = 250 kHz */
#define BW_250_KHZ                      0x80
/** @brief Bande passante = 500 kHz */
#define BW_500_KHZ                      0x90

/** @brief Masque pour extraire la bande passante */
#define BW_MASK                         0xF0

/** @brief Coding Rate 4/5 */
#define CR_4_5                          0x02
/** @brief Coding Rate 4/6 */
#define CR_4_6                          0x04
/** @brief Coding Rate 4/7 */
#define CR_4_7                          0x06
/** @brief Coding Rate 4/8 */
#define CR_4_8                          0x08

/** @brief Masque pour extraire le coding rate */
#define CR_MASK                         0x0E

/** @brief Mode header implicite */
#define HEADER_IMPLICIT_MODE            0x01
/** @brief Mode header explicite */
#define HEADER_EXPLICIT_MODE            0x00

/** @} */ // Fin MODEM_CONFIG1

/**
 * @name Configuration du Spreading Factor (REG_MODEM_CONFIG2)
 * @{
 */

/** @brief Spreading Factor 6 (le plus rapide, 64 chips/symbole) */
#define SF_6                            0x60
/** @brief Spreading Factor 7 (128 chips/symbole) */
#define SF_7                            0x70
/** @brief Spreading Factor 8 (256 chips/symbole) */
#define SF_8                            0x80
/** @brief Spreading Factor 9 (512 chips/symbole) */
#define SF_9                            0x90
/** @brief Spreading Factor 10 (1024 chips/symbole) */
#define SF_10                           0xA0
/** @brief Spreading Factor 11 (2048 chips/symbole) */
#define SF_11                           0xB0
/** @brief Spreading Factor 12 (4096 chips/symbole, plus longue portée) */
#define SF_12                           0xC0

/** @brief Masque pour extraire le Spreading Factor */
#define SF_MASK                         0xF0

/** @brief Mode CRC activé */
#define TX_CONTINUOUS_MODE              0x08
/** @brief Mode paquet unique */
#define TX_SINGLE_PACKET                0x00

/** @brief Activation du CRC */
#define RX_CRC_ENABLE                   0x04
#define RX_CRC_DISABLE                  0x00

/** @brief Timeout de réception (nombre de symboles) */
#define RX_TIMEOUT_MSB_MASK             0x03

/** @} */ // Fin MODEM_CONFIG2

/**
 * @name Configuration avancée (REG_MODEM_CONFIG3)
 * @{
 */

/** @brief Activation de l'AGC automatique */
#define AGC_AUTO_ON                     0x04
#define AGC_AUTO_OFF                    0x00

/** @brief Activation de la correction de fréquence */
#define FREQ_CORRECTION_ON              0x08

/** @brief Mode Low Data Rate Optimize (pour SF11-SF12) */
#define LOW_DATA_RATE_OPTIMIZE_ON       0x08
#define LOW_DATA_RATE_OPTIMIZE_OFF      0x00

/** @} */ // Fin MODEM_CONFIG3

// ============================================================
// SECTION 5 : CONFIGURATION PA (REG_PA_CONFIG)
// ============================================================

/**
 * @name Configuration de l'amplificateur de puissance
 * @{
 */

/** @brief Activation du PA Boost (pour puissance > 14 dBm) */
#define PA_BOOST_ON                     0x80
#define PA_BOOST_OFF                    0x00

/** @brief Masque de la puissance de sortie */
#define OUTPUT_POWER_MASK               0x0F

/** @brief Puissance maximale avec PA Boost */
#define PA_MAX_POWER                    0x70

/** @brief Configuration PA DAC pour haute puissance */
#define PA_DAC_LOW_POWER                0x84    // Puissance normale
#define PA_DAC_HIGH_POWER               0x87    // +20 dBm

/** @} */ // Fin configuration PA

// ============================================================
// SECTION 6 : CONFIGURATION LNA (REG_LNA)
// ============================================================

/**
 * @name Configuration du LNA (Low Noise Amplifier)
 * @{
 */

/** @brief Gain LNA automatique (recommandé) */
#define LNA_GAIN_AUTO                   0x20

/** @brief Gain LNA maximum (G1) */
#define LNA_GAIN_G1                     0x20
/** @brief Gain LNA maximum (G2) */
#define LNA_GAIN_G2                     0x40
/** @brief Gain LNA maximum (G3) */
#define LNA_GAIN_G3                     0x60
/** @brief Gain LNA maximum (G4) */
#define LNA_GAIN_G4                     0x80
/** @brief Gain LNA maximum (G5) */
#define LNA_GAIN_G5                     0xA0
/** @brief Gain LNA maximum (G6) */
#define LNA_GAIN_G6                     0xC0

/** @brief Masque du gain LNA */
#define LNA_GAIN_MASK                   0xE0

/** @brief Boost LNA HF activé */
#define LNA_BOOST_HF_ON                 0x03
#define LNA_BOOST_HF_OFF                0x00

/** @} */ // Fin configuration LNA

// ============================================================
// SECTION 7 : CONFIGURATION DES BROCHES DIO
// ============================================================

/**
 * @name Mapping des interruptions DIO (REG_DIO_MAPPING_1)
 * @{
 */

/** @brief DIO0 = RxDone */
#define DIO0_RX_DONE                    0x00
/** @brief DIO0 = TxDone */
#define DIO0_TX_DONE                    0x40
/** @brief DIO0 = CadDone */
#define DIO0_CAD_DONE                   0x80

/** @brief Masque DIO0 */
#define DIO0_MASK                       0xC0

/** @brief DIO1 = RxTimeout */
#define DIO1_RX_TIMEOUT                 0x00
/** @brief DIO1 = FhssChangeChannel */
#define DIO1_FHSS_CHANGE                0x10
/** @brief DIO1 = CadDetected */
#define DIO1_CAD_DETECTED               0x20

/** @brief Masque DIO1 */
#define DIO1_MASK                       0x30

/** @brief DIO2 = FhssChangeChannel */
#define DIO2_FHSS_CHANGE                0x00
/** @brief DIO2 = FhssChangeChannel (inversé) */
#define DIO2_FHSS_CHANGE_INV            0x04

/** @brief Masque DIO2 */
#define DIO2_MASK                       0x0C

/** @brief DIO3 = ValidHeader */
#define DIO3_VALID_HEADER               0x00
/** @brief DIO3 = RxDone (comme DIO0) */
#define DIO3_RX_DONE                    0x01

/** @brief Masque DIO3 */
#define DIO3_MASK                       0x03

/** @} */ // Fin mapping DIO

// ============================================================
// SECTION 8 : IDENTIFICATION ET VERSION
// ============================================================

/**
 * @name Version du silicium
 * @{
 */

/** @brief Version attendue du SX1278 */
#define SX1278_EXPECTED_VERSION         0x12

/** @brief Version du SX1276 */
#define SX1276_EXPECTED_VERSION         0x12

/** @brief Version du SX1279 */
#define SX1279_EXPECTED_VERSION         0x12

/** @} */ // Fin version

// ============================================================
// SECTION 9 : CONSTANTES DE TIMING
// ============================================================

/**
 * @name Timeouts par défaut
 * @{
 */

/** @brief Timeout transmission (ms) */
#define SX1278_TX_TIMEOUT_MS            5000

/** @brief Timeout réception (ms) */
#define SX1278_RX_TIMEOUT_MS            10000

/** @brief Délai après reset (ms) */
#define SX1278_RESET_DELAY_MS           10

/** @brief Délai après changement de mode (ms) */
#define SX1278_MODE_CHANGE_DELAY_MS     5

/** @brief Délai stabilisation PLL (ms) */
#define SX1278_PLL_LOCK_DELAY_MS        2

/** @} */ // Fin timeouts

// ============================================================
// SECTION 10 : FREQUENCES STANDARD
// ============================================================

/**
 * @name Fréquences de fonctionnement
 * @{
 */

/** @brief Fréquence minimale (Hz) */
#define SX1278_FREQ_MIN                 137000000UL

/** @brief Fréquence maximale (Hz) */
#define SX1278_FREQ_MAX                 1020000000UL

/** @brief Pas de fréquence (Hz) - F(XOSC) / 2^19 */
#define SX1278_FREQ_STEP                61.03515625f

/** @brief Fréquence de l'oscillateur (Hz) */
#define SX1278_XOSC_FREQ                32000000UL

/** @brief Résolution du synthétiseur (2^19) */
#define SX1278_FREQ_RESOLUTION          524288UL

/** @} */ // Fin fréquences

// ============================================================
// SECTION 11 : TAILLE DE LA FIFO
// ============================================================

/**
 * @name Configuration de la FIFO
 * @{
 */

/** @brief Taille maximale de la FIFO (octets) */
#define SX1278_FIFO_SIZE                256

/** @brief Taille maximale d'un paquet (octets) */
#define SX1278_MAX_PACKET_SIZE          255

/** @brief Taille minimale d'un paquet (octets) */
#define SX1278_MIN_PACKET_SIZE          1

/** @} */ // Fin FIFO

// ============================================================
// SECTION 12 : STRUCTURES DE DONNÉES
// ============================================================

/**
 * @brief Structure de configuration du SX1278
 */
typedef struct {
    uint32_t frequency;             // Fréquence en Hz
    uint8_t spreadingFactor;        // Spreading Factor (6-12)
    uint32_t bandwidth;             // Bande passante en Hz
    uint8_t codingRate;             // Coding Rate (5-8, correspond à 4/5 - 4/8)
    uint8_t txPower;                // Puissance d'émission (2-20 dBm)
    uint8_t preambleLength;         // Longueur du préambule (symboles)
    uint8_t syncWord;               // Mot de synchronisation
    bool crcEnabled;                // CRC activé/désactivé
    bool implicitHeader;            // Header implicite/explicite
    bool lowDataRateOptimize;       // Optimisation bas débit
    bool agcAutoOn;                 // AGC automatique
    uint8_t lnaGain;                // Gain LNA
    bool lnaBoostHf;                // Boost LNA HF
    bool paBoost;                   // PA Boost activé
} SX1278_Config;

/**
 * @brief Structure d'état du SX1278
 */
typedef struct {
    bool initialized;               // Module initialisé
    bool transmitting;              // En cours de transmission
    bool receiving;                 // En cours de réception
    uint8_t currentMode;            // Mode actuel
    uint32_t lastFrequency;         // Dernière fréquence configurée
    int16_t lastRSSI;               // Dernier RSSI mesuré
    int8_t lastSNR;                 // Dernier SNR mesuré
    uint32_t packetsSent;           // Compteur paquets envoyés
    uint32_t packetsReceived;       // Compteur paquets reçus
    uint32_t packetsError;          // Compteur paquets en erreur
    uint32_t txTimeout;             // Timeout transmission
    uint32_t rxTimeout;             // Timeout réception
} SX1278_State;

/**
 * @brief Énumération des erreurs possibles
 */
typedef enum {
    SX1278_OK = 0,                      // Succès
    SX1278_ERROR_INIT = -1,             // Échec initialisation
    SX1278_ERROR_VERSION = -2,          // Version incorrecte
    SX1278_ERROR_TIMEOUT = -3,          // Timeout
    SX1278_ERROR_CRC = -4,              // Erreur CRC
    SX1278_ERROR_BUSY = -5,             // Module occupé
    SX1278_ERROR_PARAM = -6,            // Paramètre invalide
    SX1278_ERROR_SPI = -7,              // Erreur SPI
    SX1278_ERROR_FREQUENCY = -8,        // Fréquence invalide
    SX1278_ERROR_POWER = -9,            // Puissance invalide
    SX1278_ERROR_FIFO = -10,            // Erreur FIFO
    SX1278_ERROR_NOT_INITIALIZED = -11  // Non initialisé
} SX1278_Error;

// ============================================================
// SECTION 13 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Calcule la valeur du registre FRF à partir d'une fréquence
 * @param freq Fréquence en Hz
 * @return Valeur 24 bits pour les registres FRF_MSB, FRF_MID, FRF_LSB
 */
#define SX1278_CALC_FRF(freq)           ((uint32_t)(((uint64_t)(freq) << 19) / SX1278_XOSC_FREQ))

/**
 * @brief Calcule la fréquence à partir d'une valeur FRF
 * @param frf Valeur 24 bits des registres FRF
 * @return Fréquence en Hz
 */
#define SX1278_CALC_FREQ(frf)           ((uint32_t)(((uint64_t)(frf) * SX1278_XOSC_FREQ) >> 19))

/**
 * @brief Calcule le temps d'occupation de l'air (Time on Air)
 * @param sf Spreading Factor
 * @param bw Bande passante en Hz
 * @param payloadLength Longueur du payload en octets
 * @param cr Coding Rate (1-4 correspondant à 4/5 - 4/8)
 * @param preambleLength Longueur du préambule
 * @param headerEnabled Header explicite activé
 * @return Temps en millisecondes
 */
#define SX1278_CALC_TIME_ON_AIR(sf, bw, payloadLength, cr, preambleLength, headerEnabled) \
    ((sf >= 7) ? \
        (/* Temps préambule */ \
         (preambleLength + 4.25f) * (1000.0f / bw) * (1 << sf) + \
         /* Temps payload */ \
         (8.0f + MAX(ceil((8.0f * payloadLength - 4.0f * sf + 28.0f + 16.0f * (headerEnabled ? 1 : 0) - 20.0f * (headerEnabled ? 0 : 1)) / \
         (4.0f * (sf - 2.0f * 1.0f))) * (cr + 4.0f), 0.0f) * (1000.0f / bw) * (1 << sf)) \
     : 0)

// ============================================================
// SECTION 14 : TABLEAUX DE CONVERSION
// ============================================================

/**
 * @brief Table de conversion Spreading Factor → valeur registre
 */
static const uint8_t SF_TO_REGISTER[] = {
    0x60,   // SF6
    0x70,   // SF7
    0x80,   // SF8
    0x90,   // SF9
    0xA0,   // SF10
    0xB0,   // SF11
    0xC0    // SF12
};

/**
 * @brief Table de conversion Bande Passante → valeur registre
 */
static const uint8_t BW_TO_REGISTER[] = {
    0x00,   // 7.8 kHz
    0x10,   // 10.4 kHz
    0x20,   // 15.6 kHz
    0x30,   // 20.8 kHz
    0x40,   // 31.25 kHz
    0x50,   // 41.7 kHz
    0x60,   // 62.5 kHz
    0x70,   // 125 kHz
    0x80,   // 250 kHz
    0x90    // 500 kHz
};

/**
 * @brief Valeurs de bande passante en Hz
 */
static const uint32_t BW_VALUES[] = {
    7800,       // 7.8 kHz
    10400,      // 10.4 kHz
    15600,      // 15.6 kHz
    20800,      // 20.8 kHz
    31250,      // 31.25 kHz
    41700,      // 41.7 kHz
    62500,      // 62.5 kHz
    125000,     // 125 kHz
    250000,     // 250 kHz
    500000      // 500 kHz
};

/** @brief Nombre d'entrées dans les tables */
#define BW_TABLE_SIZE                   10

/**
 * @brief Table de conversion Coding Rate → valeur registre
 */
static const uint8_t CR_TO_REGISTER[] = {
    0x02,   // 4/5
    0x04,   // 4/6
    0x06,   // 4/7
    0x08    // 4/8
};

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SX1278_DEFS_H