/**
 * @file sx1278_hal.h
 * @brief Couche d'abstraction matérielle (HAL) pour le SX1278
 * 
 * Ce fichier déclare les fonctions de bas niveau pour :
 * - La communication SPI avec le module
 * - La gestion des broches de contrôle (CS, Reset, DIO)
 * - La lecture/écriture des registres
 * - La gestion des interruptions
 * 
 * Cette couche isole le matériel : si vous changez de MCU,
 * seul ce fichier (et son .c) doivent être modifiés.
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SX1278_HAL_H
#define SX1278_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "sx1278_defs.h"
#include "../../config.h"

// ============================================================
// SECTION 1 : HANDLES ET VARIABLES GLOBALES
// ============================================================

/**
 * @brief Handle SPI utilisé pour la communication avec le SX1278
 * 
 * Défini dans main.c, initialisé par MX_SPI2_Init()
 */
extern SPI_HandleTypeDef hspi2;

/**
 * @brief Configuration actuelle du module
 */
extern SX1278_Config sx1278_config;

/**
 * @brief État actuel du module
 */
extern SX1278_State sx1278_state;

// ============================================================
// SECTION 2 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise la couche HAL du SX1278
 * 
 * Configure :
 * - Les broches GPIO (CS, Reset, DIO0, DIO1)
 * - L'interface SPI (via le handle hspi2)
 * - Les interruptions externes (DIO0)
 * 
 * @return SX1278_OK si succès, code d'erreur sinon
 */
SX1278_Error sx1278_hal_init(void);

/**
 * @brief Réinitialise le module SX1278
 * 
 * Effectue une séquence de reset hardware :
 * 1. Mise à LOW du pin Reset
 * 2. Attente 10 ms
 * 3. Remise à HIGH du pin Reset
 * 4. Attente 10 ms (stabilisation)
 */
void sx1278_hal_reset(void);

/**
 * @brief Vérifie si le module SX1278 est présent
 * 
 * Lit le registre REG_VERSION (0x42) et compare
 * avec la valeur attendue (0x12).
 * 
 * @return true si le module répond correctement
 */
bool sx1278_hal_is_present(void);

// ============================================================
// SECTION 3 : FONCTIONS DE COMMUNICATION SPI
// ============================================================

/**
 * @brief Écrit une valeur dans un registre du SX1278
 * 
 * Format SPI :
 * - Octet 1 : adresse du registre | 0x80 (bit WNR = 1 pour écriture)
 * - Octet 2 : valeur à écrire
 * 
 * @param reg Adresse du registre (0x00 à 0x4D)
 * @param value Valeur à écrire
 */
void sx1278_hal_write_register(uint8_t reg, uint8_t value);

/**
 * @brief Lit la valeur d'un registre du SX1278
 * 
 * Format SPI :
 * - Octet 1 : adresse du registre & 0x7F (bit WNR = 0 pour lecture)
 * - Octet 2 : valeur lue (retournée)
 * 
 * @param reg Adresse du registre (0x00 à 0x4D)
 * @return uint8_t Valeur du registre
 */
uint8_t sx1278_hal_read_register(uint8_t reg);

/**
 * @brief Écrit plusieurs octets dans les registres du SX1278
 * 
 * Utilise le mode burst du SX1278 :
 * - Octet 1 : adresse de base | 0x80
 * - Octets suivants : données à écrire séquentiellement
 * 
 * @param reg Adresse du premier registre
 * @param data Pointeur vers les données
 * @param length Nombre d'octets à écrire
 */
void sx1278_hal_write_burst(uint8_t reg, uint8_t* data, uint16_t length);

/**
 * @brief Lit plusieurs octets des registres du SX1278
 * 
 * Utilise le mode burst du SX1278 :
 * - Octet 1 : adresse de base (bit WNR = 0)
 * - Octets suivants : données lues séquentiellement
 * 
 * @param reg Adresse du premier registre
 * @param data Buffer de réception
 * @param length Nombre d'octets à lire
 */
void sx1278_hal_read_burst(uint8_t reg, uint8_t* data, uint16_t length);

// ============================================================
// SECTION 4 : FONCTIONS DE GESTION DE LA FIFO
// ============================================================

/**
 * @brief Écrit des données dans la FIFO de transmission
 * 
 * La FIFO fait 256 octets maximum.
 * Les données sont écrites à partir de l'adresse FIFO_TX_BASE.
 * 
 * @param data Données à envoyer
 * @param length Nombre d'octets
 */
void sx1278_hal_write_fifo(uint8_t* data, uint16_t length);

/**
 * @brief Lit les données de la FIFO de réception
 * 
 * Lit à partir de l'adresse pointée par REG_FIFO_RX_CURRENT_ADDR.
 * 
 * @param data Buffer de réception
 * @param length Nombre d'octets à lire
 */
void sx1278_hal_read_fifo(uint8_t* data, uint16_t length);

/**
 * @brief Vide la FIFO de réception
 * 
 * Réinitialise le pointeur FIFO à l'adresse de base RX.
 */
void sx1278_hal_flush_fifo(void);

// ============================================================
// SECTION 5 : FONCTIONS DE GESTION DU MODE
// ============================================================

/**
 * @brief Change le mode de fonctionnement du SX1278
 * 
 * Modes disponibles :
 * - MODE_SLEEP : consommation minimale
 * - MODE_STDBY : oscillateur actif, prêt à émettre/recevoir
 * - MODE_TX : transmission en cours
 * - MODE_RX_CONTINUOUS : réception continue
 * - MODE_RX_SINGLE : réception d'un seul paquet
 * - MODE_CAD : détection d'activité du canal
 * 
 * @param mode Mode souhaité (combinaison de MODE_LONG_RANGE_MODE | MODE_xxx)
 */
void sx1278_hal_set_mode(uint8_t mode);

/**
 * @brief Lit le mode de fonctionnement actuel
 * 
 * @return uint8_t Mode actuel (bits 2:0 de REG_OP_MODE)
 */
uint8_t sx1278_hal_get_mode(void);

/**
 * @brief Attend que le module ait fini son opération en cours
 * 
 * Vérifie périodiquement les flags d'interruption.
 * 
 * @param irq_mask Masque du flag à attendre (ex: IRQ_TX_DONE_MASK)
 * @param timeout_ms Timeout en millisecondes
 * @return true si le flag est levé avant le timeout
 */
bool sx1278_hal_wait_for_irq(uint8_t irq_mask, uint32_t timeout_ms);

// ============================================================
// SECTION 6 : FONCTIONS DE GESTION DES INTERRUPTIONS
// ============================================================

/**
 * @brief Active les interruptions du SX1278
 * 
 * Configure les broches DIO en entrée avec interruption.
 * 
 * @param dio0_mode Mode d'interruption pour DIO0 (DIO0_RX_DONE, DIO0_TX_DONE, etc.)
 * @param dio1_mode Mode d'interruption pour DIO1
 */
void sx1278_hal_enable_interrupts(uint8_t dio0_mode, uint8_t dio1_mode);

/**
 * @brief Désactive toutes les interruptions
 */
void sx1278_hal_disable_interrupts(void);

/**
 * @brief Lit et efface les flags d'interruption
 * 
 * @return uint8_t Valeur du registre IRQ_FLAGS
 */
uint8_t sx1278_hal_get_irq_flags(void);

/**
 * @brief Efface un flag d'interruption spécifique
 * 
 * @param irq_mask Masque du flag à effacer
 */
void sx1278_hal_clear_irq_flag(uint8_t irq_mask);

/**
 * @brief Callback appelé quand DIO0 est déclenché (paquet reçu/envoyé)
 * 
 * Cette fonction est appelée depuis l'IRQ EXTI.
 * Elle ne fait que positionner un flag, le traitement
 * est fait dans la boucle principale.
 */
void sx1278_hal_dio0_callback(void);

/**
 * @brief Callback appelé quand DIO1 est déclenché
 */
void sx1278_hal_dio1_callback(void);

// ============================================================
// SECTION 7 : FONCTIONS DE CONTRÔLE DES BROCHES
// ============================================================

/**
 * @brief Active le Chip Select (CS = LOW)
 * 
 * Appelée avant chaque transaction SPI.
 */
static inline void sx1278_hal_cs_low(void)
{
    HAL_GPIO_WritePin(LORA_CS_PORT, LORA_CS_PIN, GPIO_PIN_RESET);
}

/**
 * @brief Désactive le Chip Select (CS = HIGH)
 * 
 * Appelée après chaque transaction SPI.
 */
static inline void sx1278_hal_cs_high(void)
{
    HAL_GPIO_WritePin(LORA_CS_PORT, LORA_CS_PIN, GPIO_PIN_SET);
}

/**
 * @brief Lit l'état de la broche DIO0
 * 
 * @return GPIO_PIN_SET ou GPIO_PIN_RESET
 */
static inline GPIO_PinState sx1278_hal_read_dio0(void)
{
    return HAL_GPIO_ReadPin(LORA_DIO0_PORT, LORA_DIO0_PIN);
}

/**
 * @brief Lit l'état de la broche DIO1
 * 
 * @return GPIO_PIN_SET ou GPIO_PIN_RESET
 */
static inline GPIO_PinState sx1278_hal_read_dio1(void)
{
    return HAL_GPIO_ReadPin(LORA_DIO1_PORT, LORA_DIO1_PIN);
}

// ============================================================
// SECTION 8 : FONCTIONS DE CONFIGURATION
// ============================================================

/**
 * @brief Configure la fréquence de fonctionnement
 * 
 * @param freq_hz Fréquence en Hz (ex: 868000000 pour 868 MHz)
 * @return SX1278_ERROR_FREQUENCY si hors limites
 */
SX1278_Error sx1278_hal_set_frequency(uint32_t freq_hz);

/**
 * @brief Configure la puissance d'émission
 * 
 * @param power_dbm Puissance en dBm (2 à 20)
 * @return SX1278_ERROR_POWER si hors limites
 */
SX1278_Error sx1278_hal_set_tx_power(int8_t power_dbm);

/**
 * @brief Configure le Spreading Factor
 * 
 * @param sf Spreading Factor (6 à 12)
 */
void sx1278_hal_set_spreading_factor(uint8_t sf);

/**
 * @brief Configure la bande passante
 * 
 * @param bw_hz Bande passante en Hz (7800 à 500000)
 */
void sx1278_hal_set_bandwidth(uint32_t bw_hz);

/**
 * @brief Configure le Coding Rate
 * 
 * @param cr Coding Rate (5=4/5, 6=4/6, 7=4/7, 8=4/8)
 */
void sx1278_hal_set_coding_rate(uint8_t cr);

/**
 * @brief Configure la longueur du préambule
 * 
 * @param length Nombre de symboles (généralement 8)
 */
void sx1278_hal_set_preamble_length(uint16_t length);

/**
 * @brief Configure le mot de synchronisation
 * 
 * @param sync_word Mot de synchronisation (0x34 = public, 0x12 = privé)
 */
void sx1278_hal_set_sync_word(uint8_t sync_word);

/**
 * @brief Active/désactive le CRC
 * 
 * @param enabled true pour activer
 */
void sx1278_hal_set_crc(bool enabled);

// ============================================================
// SECTION 9 : FONCTIONS DE TRANSMISSION/RÉCEPTION
// ============================================================

/**
 * @brief Envoie un paquet de données
 * 
 * 1. Configure le mode STDBY
 * 2. Écrit les données dans la FIFO
 * 3. Configure la longueur du payload
 * 4. Passe en mode TX
 * 5. Attend la fin de transmission (ou timeout)
 * 
 * @param data Données à envoyer
 * @param length Nombre d'octets (max 255)
 * @return SX1278_OK si succès
 */
SX1278_Error sx1278_hal_transmit(uint8_t* data, uint16_t length);

/**
 * @brief Passe en mode réception continue
 * 
 * Le module restera en réception jusqu'à ce qu'un paquet
 * soit reçu ou qu'on change de mode.
 */
void sx1278_hal_start_receive(void);

/**
 * @brief Passe en mode réception simple (un seul paquet)
 */
void sx1278_hal_start_receive_single(void);

/**
 * @brief Vérifie si un paquet a été reçu
 * 
 * @return true si IRQ_RX_DONE est levé
 */
bool sx1278_hal_is_packet_received(void);

/**
 * @brief Lit le dernier paquet reçu
 * 
 * @param data Buffer de réception
 * @param length Pointeur vers la variable recevant la longueur
 * @return SX1278_OK si succès
 */
SX1278_Error sx1278_hal_read_packet(uint8_t* data, uint16_t* length);

/**
 * @brief Vérifie si la transmission est terminée
 * 
 * @return true si IRQ_TX_DONE est levé
 */
bool sx1278_hal_is_transmit_done(void);

// ============================================================
// SECTION 10 : FONCTIONS DE MESURE
// ============================================================

/**
 * @brief Mesure le RSSI (Received Signal Strength Indicator)
 * 
 * @return int16_t RSSI en dBm (valeur négative, ex: -80)
 */
int16_t sx1278_hal_get_rssi(void);

/**
 * @brief Mesure le SNR (Signal-to-Noise Ratio)
 * 
 * @return int8_t SNR en dB (ex: 8 pour 8 dB au-dessus du bruit)
 */
int8_t sx1278_hal_get_snr(void);

/**
 * @brief Mesure le RSSI du dernier paquet reçu
 * 
 * @return int16_t RSSI en dBm
 */
int16_t sx1278_hal_get_packet_rssi(void);

/**
 * @brief Mesure le SNR du dernier paquet reçu
 * 
 * @return int8_t SNR en dB
 */
int8_t sx1278_hal_get_packet_snr(void);

/**
 * @brief Estime l'erreur de fréquence
 * 
 * Utile pour calibrer l'oscillateur.
 * 
 * @return int32_t Erreur de fréquence en Hz
 */
int32_t sx1278_hal_get_frequency_error(void);

// ============================================================
// SECTION 11 : FONCTIONS DE GESTION D'ÉNERGIE
// ============================================================

/**
 * @brief Met le module en veille (Sleep)
 * 
 * Consommation : ~0.2 µA
 * Seul le registre REG_OP_MODE reste accessible.
 */
void sx1278_hal_sleep(void);

/**
 * @brief Réveille le module (Standby)
 * 
 * Consommation : ~1.6 mA
 * L'oscillateur est actif, prêt à émettre/recevoir.
 */
void sx1278_hal_wakeup(void);

/**
 * @brief Vérifie si le module est en veille
 * 
 * @return true si le mode est MODE_SLEEP
 */
bool sx1278_hal_is_sleeping(void);

// ============================================================
// SECTION 12 : FONCTIONS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
/**
 * @brief Affiche le contenu de tous les registres
 * 
 * Utile pour le débogage.
 */
void sx1278_hal_dump_registers(void);

/**
 * @brief Affiche l'état du module
 */
void sx1278_hal_print_state(void);
#endif

// ============================================================
// SECTION 13 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define SX1278_DEBUG(fmt, ...)  printf("[SX1278] " fmt, ##__VA_ARGS__)
#else
    #define SX1278_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 14 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SX1278_HAL_H