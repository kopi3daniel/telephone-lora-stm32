/**
 * @file audio_codec_adpcm.cpp
 * @brief Implémentation du codec ADPCM
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans audio_codec_adpcm.h.
 * 
 * Il gère :
 * - L'encodage ADPCM (16→8, 16→4, 16→2 bits)
 * - Le décodage ADPCM (8→16, 4→16, 2→16 bits)
 * - Les fonctions optimisées pour LoRa (8→4, 4→8 bits)
 * - Les mesures de qualité (SNR, ratio de compression)
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "audio_codec_adpcm.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise l'encodeur ADPCM
 */
void adpcm_encoder_init(ADPCM_EncoderState* state, ADPCM_Mode mode)
{
    if (state == NULL) return;
    
    memset(state, 0, sizeof(ADPCM_EncoderState));
    
    state->predictedSample = 0;
    state->stepIndex = 0;
    state->stepSize = ADPCM_STEP_TABLE[0];
    state->mode = mode;
    state->totalSamplesEncoded = 0;
    state->totalBytesEncoded = 0;
    state->compressionRatio = 1.0f;
    
    ADPCM_DEBUG("Encodeur initialisé (mode %d bits)\n", 
                (mode == ADPCM_MODE_8BIT) ? 8 : 
                (mode == ADPCM_MODE_4BIT) ? 4 : 2);
}

/**
 * @brief Initialise le décodeur ADPCM
 */
void adpcm_decoder_init(ADPCM_DecoderState* state, ADPCM_Mode mode)
{
    if (state == NULL) return;
    
    memset(state, 0, sizeof(ADPCM_DecoderState));
    
    state->predictedSample = 0;
    state->stepIndex = 0;
    state->stepSize = ADPCM_STEP_TABLE[0];
    state->mode = mode;
    state->totalSamplesDecoded = 0;
    state->totalBytesDecoded = 0;
    
    ADPCM_DEBUG("Décodeur initialisé (mode %d bits)\n",
                (mode == ADPCM_MODE_8BIT) ? 8 :
                (mode == ADPCM_MODE_4BIT) ? 4 : 2);
}

/**
 * @brief Réinitialise l'encodeur
 */
void adpcm_encoder_reset(ADPCM_EncoderState* state)
{
    if (state == NULL) return;
    adpcm_encoder_init(state, state->mode);
}

/**
 * @brief Réinitialise le décodeur
 */
void adpcm_decoder_reset(ADPCM_DecoderState* state)
{
    if (state == NULL) return;
    adpcm_decoder_init(state, state->mode);
}

// ============================================================
// SECTION 2 : ENCODAGE
// ============================================================

/**
 * @brief Encode UN échantillon 16 bits en ADPCM
 */
uint8_t adpcm_encode_sample(ADPCM_EncoderState* state, int16_t sample)
{
    if (state == NULL) return 0;
    
    // 1. Calculer la différence entre l'échantillon et la prédiction
    int32_t diff = (int32_t)sample - (int32_t)state->predictedSample;
    
    // 2. Déterminer le signe
    uint8_t signBit = 0;
    if (diff < 0)
    {
        signBit = 1;
        diff = -diff;
    }
    
    // 3. Quantifier la différence
    uint8_t quantizedCode = 0;
    int32_t stepSize = state->stepSize;
    
    // Recherche du niveau de quantification
    if (diff >= stepSize)
    {
        quantizedCode = 1;
        if (diff >= stepSize * 2) { quantizedCode = 2; }
        if (diff >= stepSize * 3) { quantizedCode = 3; }
        if (diff >= stepSize * 4) { quantizedCode = 4; }
        if (diff >= stepSize * 5) { quantizedCode = 5; }
        if (diff >= stepSize * 6) { quantizedCode = 6; }
        if (diff >= stepSize * 7) { quantizedCode = 7; }
    }
    
    // 4. Quantifier la valeur de la différence
    int32_t quantizedDiff = stepSize * quantizedCode + (stepSize / 2);
    if (signBit) quantizedDiff = -quantizedDiff;
    
    // 5. Mettre à jour la prédiction
    state->predictedSample += quantizedDiff;
    
    // Limiter à [-32768, 32767]
    if (state->predictedSample > 32767) state->predictedSample = 32767;
    if (state->predictedSample < -32768) state->predictedSample = -32768;
    
    // 6. Adapter l'index du pas
    state->stepIndex += ADPCM_INDEX_TABLE[quantizedCode];
    
    if (state->stepIndex < 0) state->stepIndex = 0;
    if (state->stepIndex >= ADPCM_STEP_TABLE_SIZE) state->stepIndex = ADPCM_STEP_TABLE_SIZE - 1;
    
    state->stepSize = ADPCM_STEP_TABLE[state->stepIndex];
    
    // 7. Construire le code de sortie (4 bits)
    uint8_t code = (uint8_t)(quantizedCode & 0x0F);
    if (signBit) code |= 0x08;
    
    state->totalSamplesEncoded++;
    
    return code;
}

/**
 * @brief Encode un buffer complet 16 bits → ADPCM
 */
void adpcm_encode_buffer(ADPCM_EncoderState* state,
                         const int16_t* input, uint16_t inputSize,
                         uint8_t* output, uint16_t* outputSize)
{
    if (state == NULL || input == NULL || output == NULL || outputSize == NULL) return;
    
    uint16_t outIndex = 0;
    
    if (state->mode == ADPCM_MODE_8BIT)
    {
        // Mode 8 bits : un octet par échantillon
        for (uint16_t i = 0; i < inputSize; i++)
        {
            output[outIndex++] = adpcm_encode_sample(state, input[i]);
        }
    }
    else if (state->mode == ADPCM_MODE_4BIT)
    {
        // Mode 4 bits : deux échantillons par octet (packés en nibbles)
        for (uint16_t i = 0; i < inputSize; i += 2)
        {
            uint8_t highNibble = adpcm_encode_sample(state, input[i]);
            uint8_t lowNibble = 0;
            
            if (i + 1 < inputSize)
            {
                lowNibble = adpcm_encode_sample(state, input[i + 1]);
            }
            
            output[outIndex++] = ADPCM_COMBINE_NIBBLES(highNibble, lowNibble);
        }
    }
    else if (state->mode == ADPCM_MODE_2BIT)
    {
        // Mode 2 bits : quatre échantillons par octet
        for (uint16_t i = 0; i < inputSize; i += 4)
        {
            uint8_t byteVal = 0;
            
            for (uint8_t j = 0; j < 4; j++)
            {
                uint8_t code = 0;
                if (i + j < inputSize)
                {
                    code = adpcm_encode_sample(state, input[i + j]) & 0x03;
                }
                byteVal |= (code << (6 - j * 2));
            }
            
            output[outIndex++] = byteVal;
        }
    }
    
    *outputSize = outIndex;
    state->totalBytesEncoded += outIndex;
    
    // Calculer le ratio de compression
    if (outIndex > 0)
    {
        state->compressionRatio = (float)(inputSize * sizeof(int16_t)) / (float)outIndex;
    }
}

/**
 * @brief Version optimisée : 8 bits → ADPCM 4 bits
 */
uint16_t adpcm_encode_8to4(const uint8_t* input, uint16_t inputSize, uint8_t* output)
{
    if (input == NULL || output == NULL) return 0;
    
    ADPCM_EncoderState state;
    adpcm_encoder_init(&state, ADPCM_MODE_4BIT);
    
    uint16_t outIndex = 0;
    
    for (uint16_t i = 0; i < inputSize; i += 2)
    {
        // Convertir 8 bits non signé → 16 bits signé
        int16_t sample1 = ((int16_t)input[i] - 128) << 8;
        int16_t sample2 = ((int16_t)input[i + 1] - 128) << 8;
        
        uint8_t highNibble = adpcm_encode_sample(&state, sample1);
        uint8_t lowNibble = adpcm_encode_sample(&state, sample2);
        
        output[outIndex++] = ADPCM_COMBINE_NIBBLES(highNibble, lowNibble);
    }
    
    return outIndex;
}

// ============================================================
// SECTION 3 : DÉCODAGE
// ============================================================

/**
 * @brief Décode UNE valeur ADPCM en échantillon 16 bits
 */
int16_t adpcm_decode_sample(ADPCM_DecoderState* state, uint8_t code)
{
    if (state == NULL) return 0;
    
    // 1. Extraire le signe et la valeur quantifiée
    uint8_t signBit = (code >> 3) & 0x01;
    uint8_t quantizedCode = code & 0x07;
    
    // 2. Calculer la différence quantifiée
    int32_t stepSize = state->stepSize;
    int32_t diff = stepSize * quantizedCode + (stepSize / 2);
    if (signBit) diff = -diff;
    
    // 3. Reconstruire l'échantillon
    state->predictedSample += diff;
    
    // Limiter
    if (state->predictedSample > 32767) state->predictedSample = 32767;
    if (state->predictedSample < -32768) state->predictedSample = -32768;
    
    // 4. Adapter l'index du pas
    state->stepIndex += ADPCM_INDEX_TABLE[quantizedCode];
    
    if (state->stepIndex < 0) state->stepIndex = 0;
    if (state->stepIndex >= ADPCM_STEP_TABLE_SIZE) state->stepIndex = ADPCM_STEP_TABLE_SIZE - 1;
    
    state->stepSize = ADPCM_STEP_TABLE[state->stepIndex];
    
    state->totalSamplesDecoded++;
    
    return state->predictedSample;
}

/**
 * @brief Décode un buffer ADPCM → 16 bits
 */
void adpcm_decode_buffer(ADPCM_DecoderState* state,
                         const uint8_t* input, uint16_t inputSize,
                         int16_t* output, uint16_t* outputSize)
{
    if (state == NULL || input == NULL || output == NULL || outputSize == NULL) return;
    
    uint16_t outIndex = 0;
    
    if (state->mode == ADPCM_MODE_8BIT)
    {
        // Mode 8 bits : un octet → un échantillon
        for (uint16_t i = 0; i < inputSize; i++)
        {
            output[outIndex++] = adpcm_decode_sample(state, input[i]);
        }
    }
    else if (state->mode == ADPCM_MODE_4BIT)
    {
        // Mode 4 bits : un octet → deux échantillons
        for (uint16_t i = 0; i < inputSize; i++)
        {
            uint8_t highNibble = ADPCM_HIGH_NIBBLE(input[i]);
            uint8_t lowNibble = ADPCM_LOW_NIBBLE(input[i]);
            
            output[outIndex++] = adpcm_decode_sample(state, highNibble);
            output[outIndex++] = adpcm_decode_sample(state, lowNibble);
        }
    }
    else if (state->mode == ADPCM_MODE_2BIT)
    {
        // Mode 2 bits : un octet → quatre échantillons
        for (uint16_t i = 0; i < inputSize; i++)
        {
            for (int8_t j = 3; j >= 0; j--)
            {
                uint8_t code = (input[i] >> (j * 2)) & 0x03;
                output[outIndex++] = adpcm_decode_sample(state, code);
            }
        }
    }
    
    *outputSize = outIndex;
    state->totalBytesDecoded += inputSize;
}

/**
 * @brief Version optimisée : ADPCM 4 bits → 8 bits
 */
uint16_t adpcm_decode_4to8(const uint8_t* input, uint16_t inputSize, uint8_t* output)
{
    if (input == NULL || output == NULL) return 0;
    
    ADPCM_DecoderState state;
    adpcm_decoder_init(&state, ADPCM_MODE_4BIT);
    
    uint16_t outIndex = 0;
    
    for (uint16_t i = 0; i < inputSize; i++)
    {
        uint8_t highNibble = ADPCM_HIGH_NIBBLE(input[i]);
        uint8_t lowNibble = ADPCM_LOW_NIBBLE(input[i]);
        
        // Décoder et convertir 16 bits signé → 8 bits non signé
        int16_t sample1 = adpcm_decode_sample(&state, highNibble);
        int16_t sample2 = adpcm_decode_sample(&state, lowNibble);
        
        output[outIndex++] = (uint8_t)((sample1 >> 8) + 128);
        output[outIndex++] = (uint8_t)((sample2 >> 8) + 128);
    }
    
    return outIndex;
}

// ============================================================
// SECTION 4 : MESURES
// ============================================================

/**
 * @brief Calcule le ratio de compression
 */
float adpcm_get_compression_ratio(uint16_t inputSize, uint16_t outputSize)
{
    if (outputSize == 0) return 1.0f;
    return (float)inputSize / (float)outputSize;
}

/**
 * @brief Estime la taille compressée
 */
uint16_t adpcm_estimate_output_size(uint16_t inputSamples, ADPCM_Mode mode)
{
    return ADPCM_CALC_OUTPUT_SIZE(inputSamples, mode);
}

/**
 * @brief Estime le gain de bande passante
 */
uint8_t adpcm_get_bandwidth_savings(uint16_t inputSamples, ADPCM_Mode mode)
{
    uint16_t originalSize = inputSamples * 2;  // 16 bits = 2 octets
    uint16_t compressedSize = adpcm_estimate_output_size(inputSamples, mode);
    
    if (originalSize == 0) return 0;
    
    return (uint8_t)(100 - (compressedSize * 100 / originalSize));
}

// ============================================================
// SECTION 5 : TESTS
// ============================================================

/**
 * @brief Test aller-retour (encode → decode)
 */
int16_t adpcm_roundtrip_test(int16_t input)
{
    ADPCM_EncoderState encoder;
    ADPCM_DecoderState decoder;
    
    adpcm_encoder_init(&encoder, ADPCM_MODE_4BIT);
    adpcm_decoder_init(&decoder, ADPCM_MODE_4BIT);
    
    uint8_t code = adpcm_encode_sample(&encoder, input);
    int16_t output = adpcm_decode_sample(&decoder, code);
    
    return output;
}

/**
 * @brief Calcule le SNR (Signal-to-Noise Ratio)
 */
float adpcm_calculate_snr(const int16_t* original, const int16_t* decoded, uint16_t count)
{
    if (original == NULL || decoded == NULL || count == 0) return 0.0f;
    
    float signalPower = 0.0f;
    float noisePower = 0.0f;
    
    for (uint16_t i = 0; i < count; i++)
    {
        float orig = (float)original[i];
        float diff = orig - (float)decoded[i];
        
        signalPower += orig * orig;
        noisePower += diff * diff;
    }
    
    signalPower /= count;
    noisePower /= count;
    
    if (noisePower < 0.0001f) return 99.0f;  // Pas de bruit
    
    float snr = 10.0f * log10f(signalPower / noisePower);
    return snr;
}

/**
 * @brief Auto-test complet
 */
bool adpcm_self_test(void)
{
    ADPCM_DEBUG("Auto-test ADPCM...\n");
    
    // Test 1 : Encodage/Décodage d'un échantillon
    int16_t testSample = 1000;
    int16_t decoded = adpcm_roundtrip_test(testSample);
    int16_t error = abs(testSample - decoded);
    
    ADPCM_DEBUG("Test 1 : %d → %d (erreur: %d)\n", testSample, decoded, error);
    
    if (error > 500)  // Erreur max acceptable
    {
        ADPCM_DEBUG("Échec test 1 : erreur trop grande\n");
        return false;
    }
    
    // Test 2 : Compression d'un buffer
    int16_t testBuffer[128];
    uint8_t compressed[64];
    uint16_t compressedSize;
    
    // Remplir avec une sinusoïde
    for (int i = 0; i < 128; i++)
    {
        testBuffer[i] = (int16_t)(sinf(2.0f * M_PI * 440 * i / 8000) * 10000);
    }
    
    ADPCM_EncoderState encoder;
    adpcm_encoder_init(&encoder, ADPCM_MODE_4BIT);
    adpcm_encode_buffer(&encoder, testBuffer, 128, compressed, &compressedSize);
    
    ADPCM_DEBUG("Test 2 : 128 éch. → %d octets (ratio %.1f:1)\n",
               compressedSize, encoder.compressionRatio);
    
    if (compressedSize != 64)
    {
        ADPCM_DEBUG("Échec test 2 : taille incorrecte\n");
        return false;
    }
    
    // Test 3 : SNR
    int16_t decodedBuffer[128];
    uint16_t decodedSize;
    
    ADPCM_DecoderState decoder;
    adpcm_decoder_init(&decoder, ADPCM_MODE_4BIT);
    adpcm_decode_buffer(&decoder, compressed, compressedSize, decodedBuffer, &decodedSize);
    
    float snr = adpcm_calculate_snr(testBuffer, decodedBuffer, 128);
    ADPCM_DEBUG("Test 3 : SNR = %.1f dB\n", snr);
    
    if (snr < 20.0f)
    {
        ADPCM_DEBUG("Échec test 3 : SNR trop faible\n");
        return false;
    }
    
    ADPCM_DEBUG("Auto-test OK\n");
    return true;
}

// ============================================================
// SECTION 6 : DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état de l'encodeur
 */
void adpcm_encoder_print_state(const ADPCM_EncoderState* state)
{
    if (state == NULL) return;
    
    printf("\n═══ ÉTAT ENCODEUR ADPCM ═══\n");
    printf("Mode           : %d bits\n", 
           (state->mode == ADPCM_MODE_8BIT) ? 8 :
           (state->mode == ADPCM_MODE_4BIT) ? 4 : 2);
    printf("Prédiction     : %d\n", state->predictedSample);
    printf("Index pas      : %d\n", state->stepIndex);
    printf("Taille pas     : %d\n", state->stepSize);
    printf("Éch. encodés   : %lu\n", (unsigned long)state->totalSamplesEncoded);
    printf("Octets sortie  : %lu\n", (unsigned long)state->totalBytesEncoded);
    printf("Ratio compr.   : %.2f:1\n", state->compressionRatio);
    printf("══════════════════════════\n\n");
}

/**
 * @brief Affiche l'état du décodeur
 */
void adpcm_decoder_print_state(const ADPCM_DecoderState* state)
{
    if (state == NULL) return;
    
    printf("\n═══ ÉTAT DÉCODEUR ADPCM ═══\n");
    printf("Mode           : %d bits\n",
           (state->mode == ADPCM_MODE_8BIT) ? 8 :
           (state->mode == ADPCM_MODE_4BIT) ? 4 : 2);
    printf("Prédiction     : %d\n", state->predictedSample);
    printf("Index pas      : %d\n", state->stepIndex);
    printf("Taille pas     : %d\n", state->stepSize);
    printf("Éch. décodés   : %lu\n", (unsigned long)state->totalSamplesDecoded);
    printf("Octets entrée  : %lu\n", (unsigned long)state->totalBytesDecoded);
    printf("══════════════════════════\n\n");
}

/**
 * @brief Affiche les statistiques de compression
 */
void adpcm_print_compression_stats(uint16_t inputSize, uint16_t outputSize)
{
    float ratio = adpcm_get_compression_ratio(inputSize, outputSize);
    uint8_t savings = (uint8_t)(100 - (outputSize * 100 / (inputSize > 0 ? inputSize : 1)));
    
    printf("\n═══ STATISTIQUES COMPRESSION ADPCM ═══\n");
    printf("Taille originale  : %d octets\n", inputSize);
    printf("Taille compressée : %d octets\n", outputSize);
    printf("Ratio compression : %.1f:1\n", ratio);
    printf("Bande passante    : %d%% économisée\n", savings);
    printf("═══════════════════════════════════\n\n");
}