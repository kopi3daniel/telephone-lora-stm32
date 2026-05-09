/**
 * @file audio_codec_adpcm.h
 * @brief Codec de compression audio ADPCM (Adaptive Differential PCM)
 * 
 * Ce fichier implémente la compression/décompression audio ADPCM
 * pour réduire la bande passante nécessaire à la transmission LoRa.
 * 
 * L'ADPCM compresse l'audio en utilisant la différence entre
 * les échantillons successifs plutôt que les valeurs absolues.
 * 
 * Performances :
 * - Compression 2:1 (16 bits → 8 bits) avec pertes minimales
 * - Compression 4:1 (16 bits → 4 bits) pour l'audio LoRa
 * - Compression 8:1 (16 bits → 2 bits) pour les messages vocaux
 * - Qualité MOS : 3.5-4.0 (bonne qualité vocale)
 * 
 * Algorithmes supportés :
 * - ADPCM standard (ITU-T G.726)
 * - ADPCM simplifié (plus rapide, pour microcontrôleur)
 * - ADPCM 4 bits (recommandé pour LoRa)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef AUDIO_CODEC_ADPCM_H
#define AUDIO_CODEC_ADPCM_H

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

/** @brief Version du codec */
#define ADPCM_VERSION                   "1.0.0"

/** @brief Modes de compression */
typedef enum {
    ADPCM_MODE_8BIT     = 0,        // 16→8 bits (ratio 2:1)
    ADPCM_MODE_4BIT     = 1,        // 16→4 bits (ratio 4:1, recommandé)
    ADPCM_MODE_2BIT     = 2         // 16→2 bits (ratio 8:1, qualité réduite)
} ADPCM_Mode;

/** @brief Taille du buffer d'entrée (échantillons 16 bits) */
#define ADPCM_INPUT_BUFFER_SIZE         256

/** @brief Taille du buffer de sortie compressé (octets) */
#define ADPCM_OUTPUT_BUFFER_SIZE        128

/** @brief Nombre de niveaux de quantification */
#define ADPCM_QUANTIZATION_LEVELS       16

/** @brief Facteur d'adaptation du pas */
#define ADPCM_ADAPTATION_FACTOR         0.875f

/** @brief Pas initial */
#define ADPCM_INITIAL_STEP              16

/** @brief Pas minimum */
#define ADPCM_MIN_STEP                  1

/** @brief Pas maximum */
#define ADPCM_MAX_STEP                  2047

// ============================================================
// SECTION 2 : TABLES DE QUANTIFICATION
// ============================================================

/**
 * @brief Table des pas de quantification (index → step size)
 * 
 * Basée sur la table ITU-T G.726
 */
static const int16_t ADPCM_STEP_TABLE[] = {
    16, 17, 19, 21, 23, 25, 28, 31, 34, 37,
    41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253,
    279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707
};

/** @brief Nombre d'entrées dans la table */
#define ADPCM_STEP_TABLE_SIZE           50

/**
 * @brief Table d'adaptation de l'index (valeur quantifiée → delta index)
 */
static const int8_t ADPCM_INDEX_TABLE[] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

// ============================================================
// SECTION 3 : ÉTAT DU CODEC
// ============================================================

/**
 * @brief État de l'encodeur ADPCM
 */
typedef struct {
    int16_t predictedSample;        // Échantillon prédit
    uint8_t stepIndex;              // Index dans la table de pas
    int16_t stepSize;               // Taille du pas actuel
    ADPCM_Mode mode;                // Mode de compression
    uint32_t totalSamplesEncoded;   // Nombre d'échantillons encodés
    uint32_t totalBytesEncoded;     // Nombre d'octets en sortie
    float compressionRatio;         // Ratio de compression réel
} ADPCM_EncoderState;

/**
 * @brief État du décodeur ADPCM
 */
typedef struct {
    int16_t predictedSample;        // Échantillon prédit
    uint8_t stepIndex;              // Index dans la table de pas
    int16_t stepSize;               // Taille du pas actuel
    ADPCM_Mode mode;                // Mode de décompression
    uint32_t totalSamplesDecoded;   // Nombre d'échantillons décodés
    uint32_t totalBytesDecoded;     // Nombre d'octets en entrée
} ADPCM_DecoderState;

// ============================================================
// SECTION 4 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise l'encodeur ADPCM
 * @param state État à initialiser
 * @param mode Mode de compression
 */
void adpcm_encoder_init(ADPCM_EncoderState* state, ADPCM_Mode mode);

/**
 * @brief Initialise le décodeur ADPCM
 * @param state État à initialiser
 * @param mode Mode de compression
 */
void adpcm_decoder_init(ADPCM_DecoderState* state, ADPCM_Mode mode);

/**
 * @brief Réinitialise l'encodeur
 */
void adpcm_encoder_reset(ADPCM_EncoderState* state);

/**
 * @brief Réinitialise le décodeur
 */
void adpcm_decoder_reset(ADPCM_DecoderState* state);

// ============================================================
// SECTION 5 : FONCTIONS D'ENCODAGE
// ============================================================

/**
 * @brief Encode UN échantillon 16 bits en ADPCM
 * 
 * @param state État de l'encodeur
 * @param sample Échantillon 16 bits à encoder
 * @return Valeur ADPCM (4 bits si mode 4BIT, 2 bits si mode 2BIT)
 */
uint8_t adpcm_encode_sample(ADPCM_EncoderState* state, int16_t sample);

/**
 * @brief Encode un buffer complet de 16 bits → ADPCM
 * 
 * @param state État de l'encodeur
 * @param input Buffer d'entrée (échantillons 16 bits)
 * @param inputSize Nombre d'échantillons
 * @param output Buffer de sortie (données ADPCM)
 * @param outputSize Nombre d'octets en sortie (mis à jour)
 */
void adpcm_encode_buffer(ADPCM_EncoderState* state,
                         const int16_t* input, uint16_t inputSize,
                         uint8_t* output, uint16_t* outputSize);

/**
 * @brief Encode un buffer 8 bits → ADPCM 4 bits (optimisé LoRa)
 * 
 * Version simplifiée pour les données audio 8 bits.
 * 
 * @param input Buffer d'entrée (échantillons 8 bits non signés)
 * @param inputSize Nombre d'échantillons
 * @param output Buffer de sortie (données ADPCM 4 bits packées)
 * @return Nombre d'octets en sortie
 */
uint16_t adpcm_encode_8to4(const uint8_t* input, uint16_t inputSize, uint8_t* output);

// ============================================================
// SECTION 6 : FONCTIONS DE DÉCODAGE
// ============================================================

/**
 * @brief Décode UNE valeur ADPCM en échantillon 16 bits
 * 
 * @param state État du décodeur
 * @param code Valeur ADPCM (4 bits si mode 4BIT, 2 bits si mode 2BIT)
 * @return Échantillon 16 bits reconstruit
 */
int16_t adpcm_decode_sample(ADPCM_DecoderState* state, uint8_t code);

/**
 * @brief Décode un buffer ADPCM → 16 bits
 * 
 * @param state État du décodeur
 * @param input Buffer d'entrée (données ADPCM)
 * @param inputSize Nombre d'octets en entrée
 * @param output Buffer de sortie (échantillons 16 bits)
 * @param outputSize Nombre d'échantillons en sortie (mis à jour)
 */
void adpcm_decode_buffer(ADPCM_DecoderState* state,
                         const uint8_t* input, uint16_t inputSize,
                         int16_t* output, uint16_t* outputSize);

/**
 * @brief Décode ADPCM 4 bits → 8 bits (optimisé LoRa)
 * 
 * Version simplifiée pour restituer l'audio 8 bits.
 * 
 * @param input Buffer d'entrée (données ADPCM 4 bits packées)
 * @param inputSize Nombre d'octets en entrée
 * @param output Buffer de sortie (échantillons 8 bits)
 * @return Nombre d'échantillons en sortie
 */
uint16_t adpcm_decode_4to8(const uint8_t* input, uint16_t inputSize, uint8_t* output);

// ============================================================
// SECTION 7 : FONCTIONS DE MESURE
// ============================================================

/**
 * @brief Calcule le ratio de compression
 * @param inputSize Taille d'entrée (octets)
 * @param outputSize Taille de sortie (octets)
 * @return Ratio (ex: 2.0 pour 2:1)
 */
float adpcm_get_compression_ratio(uint16_t inputSize, uint16_t outputSize);

/**
 * @brief Estime la taille compressée
 * @param inputSamples Nombre d'échantillons 16 bits
 * @param mode Mode de compression
 * @return Taille estimée en octets
 */
uint16_t adpcm_estimate_output_size(uint16_t inputSamples, ADPCM_Mode mode);

/**
 * @brief Estime le gain de bande passante LoRa
 * @param inputSamples Nombre d'échantillons
 * @param mode Mode de compression
 * @return Gain en pourcentage
 */
uint8_t adpcm_get_bandwidth_savings(uint16_t inputSamples, ADPCM_Mode mode);

// ============================================================
// SECTION 8 : FONCTIONS DE TEST
// ============================================================

/**
 * @brief Test de l'encodeur/décodeur (boucle complète)
 * 
 * Encode puis décode pour vérifier la qualité.
 * 
 * @param input Échantillon d'entrée
 * @return Échantillon après encode/décode
 */
int16_t adpcm_roundtrip_test(int16_t input);

/**
 * @brief Calcule le SNR (Signal-to-Noise Ratio)
 * @param original Buffer original
 * @param decoded Buffer décodé
 * @param count Nombre d'échantillons
 * @return SNR en dB
 */
float adpcm_calculate_snr(const int16_t* original, const int16_t* decoded, uint16_t count);

/**
 * @brief Auto-test complet du codec
 * @return true si OK
 */
bool adpcm_self_test(void);

// ============================================================
// SECTION 9 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état de l'encodeur
 */
void adpcm_encoder_print_state(const ADPCM_EncoderState* state);

/**
 * @brief Affiche l'état du décodeur
 */
void adpcm_decoder_print_state(const ADPCM_DecoderState* state);

/**
 * @brief Affiche les statistiques de compression
 */
void adpcm_print_compression_stats(uint16_t inputSize, uint16_t outputSize);

// ============================================================
// SECTION 10 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Calcule la taille compressée pour N échantillons
 */
#define ADPCM_CALC_OUTPUT_SIZE(samples, mode) \
    (((mode) == ADPCM_MODE_8BIT) ? (samples) : \
     ((mode) == ADPCM_MODE_4BIT) ? ((samples) / 2) : \
     ((samples) / 4))

/**
 * @brief Calcule le nombre d'échantillons décompressés
 */
#define ADPCM_CALC_SAMPLES(bytes, mode) \
    (((mode) == ADPCM_MODE_8BIT) ? (bytes) : \
     ((mode) == ADPCM_MODE_4BIT) ? ((bytes) * 2) : \
     ((bytes) * 4))

/**
 * @brief Vérifie si le mode est valide
 */
#define ADPCM_IS_VALID_MODE(mode)   ((mode) <= ADPCM_MODE_2BIT)

/**
 * @brief Extrait le nibble haut d'un octet
 */
#define ADPCM_HIGH_NIBBLE(byte)     (((byte) >> 4) & 0x0F)

/**
 * @brief Extrait le nibble bas d'un octet
 */
#define ADPCM_LOW_NIBBLE(byte)      ((byte) & 0x0F)

/**
 * @brief Combine deux nibbles en un octet
 */
#define ADPCM_COMBINE_NIBBLES(high, low)  (((high) << 4) | ((low) & 0x0F))

// ============================================================
// SECTION 11 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define ADPCM_DEBUG(fmt, ...)       printf("[ADPCM] " fmt, ##__VA_ARGS__)
#else
    #define ADPCM_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 12 : COMPARAISON DES MODES
// ============================================================

/**
 * @brief Caractéristiques des modes de compression
 * 
 * ┌──────────┬──────────┬──────────┬──────────┬──────────┐
 * │ MODE     │ RATIO    │ QUALITÉ  │ SNR (dB) │ USAGE    │
 * ├──────────┼──────────┼──────────┼──────────┼──────────┤
 * │ 8 bits   │ 2:1      │ Excell.  │ ~45 dB   │ Audio HD │
 * │ 4 bits   │ 4:1      │ Bonne    │ ~30 dB   │ Voix     │
 * │ 2 bits   │ 8:1      │ Accept.  │ ~15 dB   │ Messages │
 * └──────────┴──────────┴──────────┴──────────┴──────────┘
 */

// ============================================================
// SECTION 13 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // AUDIO_CODEC_ADPCM_H