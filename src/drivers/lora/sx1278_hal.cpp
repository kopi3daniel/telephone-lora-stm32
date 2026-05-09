/**
 * @file sx1278_hal.cpp
 * @brief Implémentation de la couche d'abstraction matérielle du SX1278
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans sx1278_hal.h.
 * 
 * Il gère :
 * - La communication SPI avec le module LoRa
 * - Le contrôle des broches (CS, Reset, DIO)
 * - La lecture/écriture des registres en mode simple et burst
 * - La configuration de tous les paramètres (fréquence, puissance, etc.)
 * - La transmission et la réception des paquets
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "sx1278_hal.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/**
 * @brief Configuration actuelle du module SX1278
 */
SX1278_Config sx1278_config = {
    .frequency = 868000000,         // 868 MHz (Europe)
    .spreadingFactor = 7,           // SF7
    .bandwidth = 125000,            // 125 kHz
    .codingRate = 5,                // 4/5
    .txPower = 17,                  // 17 dBm
    .preambleLength = 8,            // 8 symboles
    .syncWord = 0x34,               // Public
    .crcEnabled = true,             // CRC activé
    .implicitHeader = false,        // Header explicite
    .lowDataRateOptimize = false,   // Pas d'optimisation bas débit
    .agcAutoOn = true,              // AGC automatique
    .lnaGain = LNA_GAIN_G1,         // Gain LNA maximum
    .lnaBoostHf = true,             // Boost LNA HF
    .paBoost = true                 // PA Boost activé
};

/**
 * @brief État actuel du module SX1278
 */
SX1278_State sx1278_state = {
    .initialized = false,
    .transmitting = false,
    .receiving = false,
    .currentMode = MODE_SLEEP,
    .lastFrequency = 0,
    .lastRSSI = 0,
    .lastSNR = 0,
    .packetsSent = 0,
    .packetsReceived = 0,
    .packetsError = 0,
    .txTimeout = SX1278_TX_TIMEOUT_MS,
    .rxTimeout = SX1278_RX_TIMEOUT_MS
};

/** @brief Flag : paquet reçu (positionné par l'IRQ) */
volatile bool sx1278_packet_received = false;

/** @brief Flag : transmission terminée (positionné par l'IRQ) */
volatile bool sx1278_transmit_done = false;

/** @brief Flag : timeout réception */
volatile bool sx1278_rx_timeout = false;

/** @brief Flag : erreur CRC */
volatile bool sx1278_crc_error = false;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise la couche HAL du SX1278
 */
SX1278_Error sx1278_hal_init(void)
{
    SX1278_DEBUG("Initialisation HAL SX1278\n");
    
    // --- Configuration des broches GPIO ---
    
    // CS (Chip Select) - Sortie, désélectionné par défaut
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = LORA_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LORA_CS_PORT, &GPIO_InitStruct);
    sx1278_hal_cs_high();  // Désélectionné
    
    // Reset - Sortie, pas en reset
    GPIO_InitStruct.Pin = LORA_RST_PIN;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LORA_RST_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LORA_RST_PORT, LORA_RST_PIN, GPIO_PIN_SET);
    
    // DIO0 - Entrée avec interruption
    GPIO_InitStruct.Pin = LORA_DIO0_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(LORA_DIO0_PORT, &GPIO_InitStruct);
    
    // DIO1 - Entrée avec interruption
    GPIO_InitStruct.Pin = LORA_DIO1_PIN;
    HAL_GPIO_Init(LORA_DIO1_PORT, &GPIO_InitStruct);
    
    // --- Configuration des interruptions NVIC ---
    HAL_NVIC_SetPriority(EXTI2_IRQn, 2, 0);   // DIO0 priorité haute
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
    
    HAL_NVIC_SetPriority(EXTI3_IRQn, 3, 0);   // DIO1 priorité moyenne
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
    
    // --- Réinitialiser le module ---
    sx1278_hal_reset();
    
    // --- Vérifier la présence du module ---
    if (!sx1278_hal_is_present())
    {
        SX1278_DEBUG("Module SX1278 non détecté !\n");
        return SX1278_ERROR_VERSION;
    }
    
    // --- Configuration par défaut du mode LoRa ---
    // Mettre en mode Sleep pour configurer
    sx1278_hal_set_mode(MODE_LONG_RANGE_MODE | MODE_SLEEP);
    
    // Configurer la fréquence
    sx1278_hal_set_frequency(sx1278_config.frequency);
    
    // Configurer la FIFO
    sx1278_hal_write_register(REG_FIFO_TX_BASE_ADDR, 0x00);
    sx1278_hal_write_register(REG_FIFO_RX_BASE_ADDR, 0x00);
    
    // Configurer le LNA
    uint8_t lna_value = sx1278_config.lnaGain;
    if (sx1278_config.lnaBoostHf) {
        lna_value |= LNA_BOOST_HF_ON;
    }
    sx1278_hal_write_register(REG_LNA, lna_value);
    
    // Configurer le PA
    sx1278_hal_set_tx_power(sx1278_config.txPower);
    
    // Configurer la rampe PA (par défaut 40 µs)
    sx1278_hal_write_register(REG_PA_RAMP, 0x09);
    
    // Configurer l'OCP (limite de courant)
    sx1278_hal_write_register(REG_OCP, 0x0B);  // 100 mA
    
    // Configurer le modem
    sx1278_hal_set_spreading_factor(sx1278_config.spreadingFactor);
    sx1278_hal_set_bandwidth(sx1278_config.bandwidth);
    sx1278_hal_set_coding_rate(sx1278_config.codingRate);
    
    // Activer le CRC si configuré
    uint8_t modem_config2 = sx1278_hal_read_register(REG_MODEM_CONFIG2);
    if (sx1278_config.crcEnabled) {
        modem_config2 |= RX_CRC_ENABLE;
    } else {
        modem_config2 &= ~RX_CRC_ENABLE;
    }
    sx1278_hal_write_register(REG_MODEM_CONFIG2, modem_config2);
    
    // Configurer le modem 3 (AGC auto + correction fréquence)
    uint8_t modem_config3 = 0x04;  // AGC auto
    if (sx1278_config.lowDataRateOptimize) {
        modem_config3 |= LOW_DATA_RATE_OPTIMIZE_ON;
    }
    sx1278_hal_write_register(REG_MODEM_CONFIG3, modem_config3);
    
    // Configurer le préambule
    sx1278_hal_set_preamble_length(sx1278_config.preambleLength);
    
    // Configurer le mot de synchronisation
    sx1278_hal_set_sync_word(sx1278_config.syncWord);
    
    // Configurer le mapping DIO
    sx1278_hal_enable_interrupts(DIO0_RX_DONE | DIO0_TX_DONE, 
                                  DIO1_RX_TIMEOUT);
    
    // Configurer le timeout de réception
    sx1278_hal_write_register(REG_SYMB_TIMEOUT_LSB, 0xFF);  // ~655 ms max
    
    // Passer en mode Standby
    sx1278_hal_set_mode(MODE_LONG_RANGE_MODE | MODE_STDBY);
    HAL_Delay(10);
    
    sx1278_state.initialized = true;
    sx1278_state.currentMode = MODE_STDBY;
    
    SX1278_DEBUG("Initialisation réussie\n");
    return SX1278_OK;
}

/**
 * @brief Réinitialise le module SX1278
 */
void sx1278_hal_reset(void)
{
    SX1278_DEBUG("Reset du module\n");
    
    // Séquence de reset : LOW → 10ms → HIGH → 10ms
    HAL_GPIO_WritePin(LORA_RST_PORT, LORA_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(SX1278_RESET_DELAY_MS);
    HAL_GPIO_WritePin(LORA_RST_PORT, LORA_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(SX1278_RESET_DELAY_MS);
    
    sx1278_state.initialized = false;
    sx1278_state.currentMode = MODE_STDBY;
}

/**
 * @brief Vérifie si le module est présent
 */
bool sx1278_hal_is_present(void)
{
    uint8_t version = sx1278_hal_read_register(REG_VERSION);
    SX1278_DEBUG("Version silicium: 0x%02X (attendue: 0x%02X)\n", 
                 version, SX1278_EXPECTED_VERSION);
    return (version == SX1278_EXPECTED_VERSION);
}

// ============================================================
// SECTION 2 : COMMUNICATION SPI
// ============================================================

/**
 * @brief Écrit une valeur dans un registre
 */
void sx1278_hal_write_register(uint8_t reg, uint8_t value)
{
    uint8_t tx_buffer[2];
    
    // Format : adresse | 0x80 (bit WNR = 1 pour écriture)
    tx_buffer[0] = reg | 0x80;
    tx_buffer[1] = value;
    
    sx1278_hal_cs_low();
    HAL_SPI_Transmit(&hspi2, tx_buffer, 2, HAL_MAX_DELAY);
    sx1278_hal_cs_high();
}

/**
 * @brief Lit la valeur d'un registre
 */
uint8_t sx1278_hal_read_register(uint8_t reg)
{
    uint8_t tx_buffer[2];
    uint8_t rx_buffer[2];
    
    // Format : adresse & 0x7F (bit WNR = 0 pour lecture)
    tx_buffer[0] = reg & 0x7F;
    tx_buffer[1] = 0x00;  // Donnée factice pour générer l'horloge
    
    sx1278_hal_cs_low();
    HAL_SPI_TransmitReceive(&hspi2, tx_buffer, rx_buffer, 2, HAL_MAX_DELAY);
    sx1278_hal_cs_high();
    
    return rx_buffer[1];  // Le deuxième octet reçu est la valeur
}

/**
 * @brief Écrit plusieurs octets en mode burst
 */
void sx1278_hal_write_burst(uint8_t reg, uint8_t* data, uint16_t length)
{
    uint8_t tx_buffer[257];  // 1 adresse + max 256 données
    
    if (length > 256) return;
    
    // Format : adresse | 0x80
    tx_buffer[0] = reg | 0x80;
    memcpy(&tx_buffer[1], data, length);
    
    sx1278_hal_cs_low();
    HAL_SPI_Transmit(&hspi2, tx_buffer, length + 1, HAL_MAX_DELAY);
    sx1278_hal_cs_high();
}

/**
 * @brief Lit plusieurs octets en mode burst
 */
void sx1278_hal_read_burst(uint8_t reg, uint8_t* data, uint16_t length)
{
    uint8_t tx_buffer[257];
    
    if (length > 256) return;
    
    // Format : adresse & 0x7F (lecture)
    tx_buffer[0] = reg & 0x7F;
    memset(&tx_buffer[1], 0x00, length);
    
    sx1278_hal_cs_low();
    // Envoyer l'adresse puis recevoir les données
    HAL_SPI_Transmit(&hspi2, tx_buffer, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi2, data, length, HAL_MAX_DELAY);
    sx1278_hal_cs_high();
}

// ============================================================
// SECTION 3 : GESTION DE LA FIFO
// ============================================================

/**
 * @brief Écrit des données dans la FIFO de transmission
 */
void sx1278_hal_write_fifo(uint8_t* data, uint16_t length)
{
    if (length > SX1278_FIFO_SIZE) {
        length = SX1278_FIFO_SIZE;
    }
    
    // Positionner le pointeur FIFO au début de la zone TX
    sx1278_hal_write_register(REG_FIFO_ADDR_PTR, 
                              sx1278_hal_read_register(REG_FIFO_TX_BASE_ADDR));
    
    // Écrire les données en burst
    sx1278_hal_write_burst(REG_FIFO, data, length);
}

/**
 * @brief Lit les données de la FIFO de réception
 */
void sx1278_hal_read_fifo(uint8_t* data, uint16_t length)
{
    if (length > SX1278_FIFO_SIZE) {
        length = SX1278_FIFO_SIZE;
    }
    
    // Positionner le pointeur FIFO à l'adresse courante de réception
    sx1278_hal_write_register(REG_FIFO_ADDR_PTR, 
                              sx1278_hal_read_register(REG_FIFO_RX_CURRENT_ADDR));
    
    // Lire les données en burst
    sx1278_hal_read_burst(REG_FIFO, data, length);
}

/**
 * @brief Vide la FIFO de réception
 */
void sx1278_hal_flush_fifo(void)
{
    // Réinitialiser le pointeur FIFO à l'adresse de base RX
    sx1278_hal_write_register(REG_FIFO_ADDR_PTR, 
                              sx1278_hal_read_register(REG_FIFO_RX_BASE_ADDR));
}

// ============================================================
// SECTION 4 : GESTION DU MODE
// ============================================================

/**
 * @brief Change le mode de fonctionnement
 */
void sx1278_hal_set_mode(uint8_t mode)
{
    uint8_t current_mode = sx1278_hal_read_register(REG_OP_MODE);
    
    // Conserver le bit Long Range Mode
    uint8_t new_mode = (current_mode & MODE_LONG_RANGE_MODE) | (mode & ~MODE_LONG_RANGE_MODE);
    
    sx1278_hal_write_register(REG_OP_MODE, new_mode);
    
    // Attendre que le mode soit effectif
    HAL_Delay(SX1278_MODE_CHANGE_DELAY_MS);
    
    sx1278_state.currentMode = mode & MODE_MASK;
}

/**
 * @brief Lit le mode de fonctionnement actuel
 */
uint8_t sx1278_hal_get_mode(void)
{
    return sx1278_hal_read_register(REG_OP_MODE) & MODE_MASK;
}

/**
 * @brief Attend un flag d'interruption avec timeout
 */
bool sx1278_hal_wait_for_irq(uint8_t irq_mask, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        uint8_t irq_flags = sx1278_hal_read_register(REG_IRQ_FLAGS);
        
        if (irq_flags & irq_mask)
        {
            return true;
        }
        
        HAL_Delay(1);  // Petite pause
    }
    
    return false;  // Timeout
}

// ============================================================
// SECTION 5 : GESTION DES INTERRUPTIONS
// ============================================================

/**
 * @brief Active les interruptions du SX1278
 */
void sx1278_hal_enable_interrupts(uint8_t dio0_mode, uint8_t dio1_mode)
{
    // Configurer le mapping DIO0
    uint8_t dio_mapping1 = sx1278_hal_read_register(REG_DIO_MAPPING_1);
    dio_mapping1 = (dio_mapping1 & ~DIO0_MASK) | (dio0_mode & DIO0_MASK);
    dio_mapping1 = (dio_mapping1 & ~DIO1_MASK) | (dio1_mode & DIO1_MASK);
    sx1278_hal_write_register(REG_DIO_MAPPING_1, dio_mapping1);
    
    // Activer les interruptions souhaitées
    sx1278_hal_write_register(REG_IRQ_FLAGS_MASK, 
                              ~(IRQ_RX_DONE_MASK | IRQ_TX_DONE_MASK | 
                                IRQ_RX_TIMEOUT_MASK | IRQ_CRC_ERROR_MASK));
}

/**
 * @brief Désactive toutes les interruptions
 */
void sx1278_hal_disable_interrupts(void)
{
    sx1278_hal_write_register(REG_IRQ_FLAGS_MASK, 0xFF);  // Masquer tout
}

/**
 * @brief Lit les flags d'interruption
 */
uint8_t sx1278_hal_get_irq_flags(void)
{
    return sx1278_hal_read_register(REG_IRQ_FLAGS);
}

/**
 * @brief Efface un flag d'interruption
 */
void sx1278_hal_clear_irq_flag(uint8_t irq_mask)
{
    sx1278_hal_write_register(REG_IRQ_FLAGS, irq_mask);
}

/**
 * @brief Callback DIO0 (appelé depuis l'IRQ EXTI)
 */
void sx1278_hal_dio0_callback(void)
{
    uint8_t irq_flags = sx1278_hal_read_register(REG_IRQ_FLAGS);
    
    if (irq_flags & IRQ_RX_DONE_MASK)
    {
        sx1278_packet_received = true;
        sx1278_state.packetsReceived++;
    }
    
    if (irq_flags & IRQ_TX_DONE_MASK)
    {
        sx1278_transmit_done = true;
        sx1278_state.packetsSent++;
    }
    
    if (irq_flags & IRQ_CRC_ERROR_MASK)
    {
        sx1278_crc_error = true;
        sx1278_state.packetsError++;
    }
    
    // Effacer les flags
    sx1278_hal_clear_irq_flag(IRQ_CLEAR_ALL_MASK);
}

/**
 * @brief Callback DIO1 (appelé depuis l'IRQ EXTI)
 */
void sx1278_hal_dio1_callback(void)
{
    uint8_t irq_flags = sx1278_hal_read_register(REG_IRQ_FLAGS);
    
    if (irq_flags & IRQ_RX_TIMEOUT_MASK)
    {
        sx1278_rx_timeout = true;
    }
    
    sx1278_hal_clear_irq_flag(IRQ_CLEAR_ALL_MASK);
}

// ============================================================
// SECTION 6 : CONFIGURATION
// ============================================================

/**
 * @brief Configure la fréquence
 */
SX1278_Error sx1278_hal_set_frequency(uint32_t freq_hz)
{
    // Vérifier les limites
    if (freq_hz < SX1278_FREQ_MIN || freq_hz > SX1278_FREQ_MAX)
    {
        return SX1278_ERROR_FREQUENCY;
    }
    
    // Calculer la valeur FRF (24 bits)
    uint32_t frf = SX1278_CALC_FRF(freq_hz);
    
    // Sauvegarder le mode actuel
    uint8_t current_mode = sx1278_hal_read_register(REG_OP_MODE);
    
    // Passer en mode Sleep pour configurer
    sx1278_hal_set_mode(MODE_LONG_RANGE_MODE | MODE_SLEEP);
    
    // Écrire les 3 octets FRF
    sx1278_hal_write_register(REG_FRF_MSB, (frf >> 16) & 0xFF);
    sx1278_hal_write_register(REG_FRF_MID, (frf >> 8) & 0xFF);
    sx1278_hal_write_register(REG_FRF_LSB, frf & 0xFF);
    
    // Restaurer le mode précédent
    sx1278_hal_write_register(REG_OP_MODE, current_mode);
    
    sx1278_config.frequency = freq_hz;
    sx1278_state.lastFrequency = freq_hz;
    
    SX1278_DEBUG("Fréquence: %lu Hz (FRF: 0x%06lX)\n", (unsigned long)freq_hz, (unsigned long)frf);
    return SX1278_OK;
}

/**
 * @brief Configure la puissance d'émission
 */
SX1278_Error sx1278_hal_set_tx_power(int8_t power_dbm)
{
    // Vérifier les limites
    if (power_dbm < 2 || power_dbm > 20)
    {
        return SX1278_ERROR_POWER;
    }
    
    uint8_t pa_config = 0;
    
    if (sx1278_config.paBoost)
    {
        // Mode PA Boost (14-20 dBm)
        pa_config = PA_BOOST_ON;
        
        if (power_dbm >= 20) {
            pa_config |= 0x0F;  // Max power
            // Configurer PA_DAC pour +20 dBm
            sx1278_hal_write_register(REG_PA_DAC, PA_DAC_HIGH_POWER);
        } else if (power_dbm >= 17) {
            pa_config |= (power_dbm - 11);  // PA_BOOST: Pout = 17 - (15 - OutputPower)
        } else {
            pa_config |= (power_dbm - 2);   // PA_BOOST: Pout = 2 + OutputPower
        }
    }
    else
    {
        // Mode normal (2-14 dBm)
        pa_config = PA_BOOST_OFF;
        pa_config |= (power_dbm - 2);
    }
    
    sx1278_hal_write_register(REG_PA_CONFIG, pa_config);
    sx1278_config.txPower = power_dbm;
    
    SX1278_DEBUG("Puissance TX: %d dBm (PA_CONFIG: 0x%02X)\n", power_dbm, pa_config);
    return SX1278_OK;
}

/**
 * @brief Configure le Spreading Factor
 */
void sx1278_hal_set_spreading_factor(uint8_t sf)
{
    if (sf < 6) sf = 6;
    if (sf > 12) sf = 12;
    
    uint8_t modem_config2 = sx1278_hal_read_register(REG_MODEM_CONFIG2);
    modem_config2 = (modem_config2 & ~SF_MASK) | (SF_TO_REGISTER[sf - 6]);
    sx1278_hal_write_register(REG_MODEM_CONFIG2, modem_config2);
    
    // Si SF11 ou SF12, activer Low Data Rate Optimize
    if (sf >= 11)
    {
        uint8_t modem_config3 = sx1278_hal_read_register(REG_MODEM_CONFIG3);
        modem_config3 |= LOW_DATA_RATE_OPTIMIZE_ON;
        sx1278_hal_write_register(REG_MODEM_CONFIG3, modem_config3);
    }
    
    sx1278_config.spreadingFactor = sf;
}

/**
 * @brief Configure la bande passante
 */
void sx1278_hal_set_bandwidth(uint32_t bw_hz)
{
    uint8_t bw_reg = BW_125_KHZ;  // Par défaut
    
    // Chercher la valeur la plus proche
    for (int i = 0; i < BW_TABLE_SIZE; i++)
    {
        if (bw_hz <= BW_VALUES[i])
        {
            bw_reg = BW_TO_REGISTER[i];
            sx1278_config.bandwidth = BW_VALUES[i];
            break;
        }
    }
    
    uint8_t modem_config1 = sx1278_hal_read_register(REG_MODEM_CONFIG1);
    modem_config1 = (modem_config1 & ~BW_MASK) | bw_reg;
    sx1278_hal_write_register(REG_MODEM_CONFIG1, modem_config1);
}

/**
 * @brief Configure le Coding Rate
 */
void sx1278_hal_set_coding_rate(uint8_t cr)
{
    if (cr < 5) cr = 5;
    if (cr > 8) cr = 8;
    
    uint8_t cr_reg = CR_TO_REGISTER[cr - 5];
    
    uint8_t modem_config1 = sx1278_hal_read_register(REG_MODEM_CONFIG1);
    modem_config1 = (modem_config1 & ~CR_MASK) | cr_reg;
    sx1278_hal_write_register(REG_MODEM_CONFIG1, modem_config1);
    
    sx1278_config.codingRate = cr;
}

/**
 * @brief Configure la longueur du préambule
 */
void sx1278_hal_set_preamble_length(uint16_t length)
{
    sx1278_hal_write_register(REG_PREAMBLE_MSB, (length >> 8) & 0xFF);
    sx1278_hal_write_register(REG_PREAMBLE_LSB, length & 0xFF);
    sx1278_config.preambleLength = length;
}

/**
 * @brief Configure le mot de synchronisation
 */
void sx1278_hal_set_sync_word(uint8_t sync_word)
{
    sx1278_hal_write_register(REG_SYNC_WORD, sync_word);
    sx1278_config.syncWord = sync_word;
}

/**
 * @brief Active/désactive le CRC
 */
void sx1278_hal_set_crc(bool enabled)
{
    uint8_t modem_config2 = sx1278_hal_read_register(REG_MODEM_CONFIG2);
    
    if (enabled)
        modem_config2 |= RX_CRC_ENABLE;
    else
        modem_config2 &= ~RX_CRC_ENABLE;
    
    sx1278_hal_write_register(REG_MODEM_CONFIG2, modem_config2);
    sx1278_config.crcEnabled = enabled;
}

// ============================================================
// SECTION 7 : TRANSMISSION / RÉCEPTION
// ============================================================

/**
 * @brief Envoie un paquet de données
 */
SX1278_Error sx1278_hal_transmit(uint8_t* data, uint16_t length)
{
    if (!sx1278_state.initialized)
        return SX1278_ERROR_NOT_INITIALIZED;
    
    if (length > SX1278_MAX_PACKET_SIZE)
        return SX1278_ERROR_FIFO;
    
    SX1278_DEBUG("Transmission de %d octets\n", length);
    
    // Passer en mode Standby
    sx1278_hal_set_mode(MODE_LONG_RANGE_MODE | MODE_STDBY);
    
    // Écrire les données dans la FIFO
    sx1278_hal_write_fifo(data, length);
    
    // Définir la longueur du payload
    sx1278_hal_write_register(REG_PAYLOAD_LENGTH, length);
    
    // Réinitialiser les flags
    sx1278_transmit_done = false;
    sx1278_hal_clear_irq_flag(IRQ_CLEAR_ALL_MASK);
    
    // Passer en mode TX
    sx1278_state.transmitting = true;
    sx1278_hal_set_mode(MODE_LONG_RANGE_MODE | MODE_TX);
    
    // Attendre la fin de transmission
    if (!sx1278_hal_wait_for_irq(IRQ_TX_DONE_MASK, sx1278_state.txTimeout))
    {
        sx1278_state.transmitting = false;
        SX1278_DEBUG("Timeout transmission !\n");
        return SX1278_ERROR_TIMEOUT;
    }
    
    sx1278_state.transmitting = false;
    
    // Effacer les flags
    sx1278_hal_clear_irq_flag(IRQ_CLEAR_ALL_MASK);
    
    // Retourner en Standby
    sx1278_hal_set_mode(MODE_LONG_RANGE_MODE | MODE_STDBY);
    
    SX1278_DEBUG("Transmission terminée\n");
    return SX1278_OK;
}

/**
 * @brief Passe en mode réception continue
 */
void sx1278_hal_start_receive(void)
{
    if (!sx1278_state.initialized) return;
    
    // Réinitialiser les flags
    sx1278_packet_received = false;
    sx1278_rx_timeout = false;
    sx1278_crc_error = false;
    sx1278_hal_clear_irq_flag(IRQ_CLEAR_ALL_MASK);
    
    // Passer en mode RX continu
    sx1278_state.receiving = true;
    sx1278_hal_set_mode(MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
    
    SX1278_DEBUG("Réception continue activée\n");
}

/**
 * @brief Vérifie si un paquet a été reçu
 */
bool sx1278_hal_is_packet_received(void)
{
    if (sx1278_packet_received)
    {
        sx1278_packet_received = false;
        return true;
    }
    return false;
}

/**
 * @brief Lit le dernier paquet reçu
 */
SX1278_Error sx1278_hal_read_packet(uint8_t* data, uint16_t* length)
{
    if (!sx1278_state.initialized)
        return SX1278_ERROR_NOT_INITIALIZED;
    
    // Lire la longueur du paquet reçu
    uint8_t rx_bytes = sx1278_hal_read_register(REG_RX_NB_BYTES);
    
    if (rx_bytes == 0)
        return SX1278_ERROR_FIFO;
    
    // Lire les données de la FIFO
    sx1278_hal_read_fifo(data, rx_bytes);
    *length = rx_bytes;
    
    // Mettre à jour les mesures
    sx1278_state.lastRSSI = sx1278_hal_get_packet_rssi();
    sx1278_state.lastSNR = sx1278_hal_get_packet_snr();
    
    // Effacer les flags
    sx1278_hal_clear_irq_flag(IRQ_CLEAR_ALL_MASK);
    
    SX1278_DEBUG("Paquet reçu: %d octets, RSSI: %d dBm, SNR: %d dB\n",
                 rx_bytes, sx1278_state.lastRSSI, sx1278_state.lastSNR);
    
    return SX1278_OK;
}

/**
 * @brief Vérifie si la transmission est terminée
 */
bool sx1278_hal_is_transmit_done(void)
{
    if (sx1278_transmit_done)
    {
        sx1278_transmit_done = false;
        return true;
    }
    return false;
}

// ============================================================
// SECTION 8 : MESURES
// ============================================================

/**
 * @brief Mesure le RSSI instantané
 */
int16_t sx1278_hal_get_rssi(void)
{
    int16_t rssi = (int16_t)sx1278_hal_read_register(REG_RSSI_VALUE);
    return -137 + rssi;  // Formule pour HF
}

/**
 * @brief Mesure le SNR
 */
int8_t sx1278_hal_get_snr(void)
{
    return (int8_t)sx1278_hal_read_register(REG_PKT_SNR_VALUE) / 4;
}

/**
 * @brief Mesure le RSSI du dernier paquet
 */
int16_t sx1278_hal_get_packet_rssi(void)
{
    int16_t rssi = (int16_t)sx1278_hal_read_register(REG_PKT_RSSI_VALUE);
    
    // Ajustement selon le SNR
    int8_t snr = sx1278_hal_get_packet_snr();
    if (snr < 0)
        rssi += snr;
    
    return -137 + rssi;
}

/**
 * @brief Mesure le SNR du dernier paquet
 */
int8_t sx1278_hal_get_packet_snr(void)
{
    return (int8_t)sx1278_hal_read_register(REG_PKT_SNR_VALUE) / 4;
}

/**
 * @brief Estime l'erreur de fréquence
 */
int32_t sx1278_hal_get_frequency_error(void)
{
    int32_t fei = ((int32_t)sx1278_hal_read_register(REG_FEI_MSB) << 16) |
                  ((int32_t)sx1278_hal_read_register(REG_FEI_MID) << 8)  |
                  ((int32_t)sx1278_hal_read_register(REG_FEI_LSB));
    
    // Convertir en Hz
    return (int32_t)((float)fei * SX1278_FREQ_STEP);
}

// ============================================================
// SECTION 9 : GESTION D'ÉNERGIE
// ============================================================

/**
 * @brief Met le module en veille
 */
void sx1278_hal_sleep(void)
{
    sx1278_hal_set_mode(MODE_LONG_RANGE_MODE | MODE_SLEEP);
    SX1278_DEBUG("Module en veille (Sleep)\n");
}

/**
 * @brief Réveille le module
 */
void sx1278_hal_wakeup(void)
{
    sx1278_hal_set_mode(MODE_LONG_RANGE_MODE | MODE_STDBY);
    SX1278_DEBUG("Module réveillé (Standby)\n");
}

/**
 * @brief Vérifie si le module est en veille
 */
bool sx1278_hal_is_sleeping(void)
{
    return (sx1278_hal_get_mode() == MODE_SLEEP);
}

//