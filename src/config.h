/**
 * @file config.h
 * @brief Configuration matérielle - Broches, adresses et constantes physiques
 *
 * Ce fichier contient TOUTES les définitions de broches (pinout),
 * les adresses mémoire, et les constantes liées au matériel.
 *
 * C'est LE fichier de référence pour le câblage.
 * Toute modification de câblage DOIT être répercutée ici.
 *
 * Contrairement à project_config.h qui est générique,
 * ce fichier est SPÉCIFIQUE à votre montage.
 *
 * @author Votre Nom
 * @date 2024
 */

#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "stm32f4xx_hal.h"
#include "include/project_config.h"
#include "include/platform.h"

// ============================================================
// SECTION 1 : FRÉQUENCE LORA (SELON RÉGION)
// ============================================================

/**
 * @def LORA_FREQUENCY
 * @brief Fréquence LoRa active
 *
 * Choisir selon votre région :
 * - Europe/Afrique : 868 MHz
 * - Amérique du Nord : 915 MHz
 * - Asie : 433 MHz
 *
 * Modifier dans project_config.h ou ici
 */
#ifndef LORA_FREQUENCY
    #define LORA_FREQUENCY      LORA_FREQ_EUROPE
#endif

// ============================================================
// SECTION 2 : BROCHES MODULE LORA RA-02 (SX1278)
// ============================================================

/**
 * @name Interface SPI2 - Module LoRa
 * @{
 */

/** @brief Instance SPI utilisée pour le module LoRa */
#define LORA_SPI_INSTANCE       SPI2

/** @brief Handle SPI correspondant */
#define LORA_SPI_HANDLE         hspi2

/** @brief Port GPIO pour le Chip Select (NSS) */
#define LORA_CS_PORT            GPIOB
/** @brief Pin GPIO pour le Chip Select (NSS) */
#define LORA_CS_PIN             GPIO_PIN_12

/** @brief Port GPIO pour le Reset */
#define LORA_RST_PORT           GPIOD
/** @brief Pin GPIO pour le Reset (actif bas) */
#define LORA_RST_PIN            GPIO_PIN_8

/** @brief Port GPIO pour DIO0 (interruption principale) */
#define LORA_DIO0_PORT          GPIOD
/** @brief Pin GPIO pour DIO0 */
#define LORA_DIO0_PIN           GPIO_PIN_7

/** @brief Port GPIO pour DIO1 (interruption secondaire) */
#define LORA_DIO1_PORT          GPIOD
/** @brief Pin GPIO pour DIO1 */
#define LORA_DIO1_PIN           GPIO_PIN_6

/** @brief Port GPIO pour DIO2 (optionnel) */
#define LORA_DIO2_PORT          GPIOD
/** @brief Pin GPIO pour DIO2 */
#define LORA_DIO2_PIN           GPIO_PIN_5

/** @} */ // Fin interface LoRa

// ============================================================
// SECTION 3 : BROCHES ÉCRAN TFT ILI9488
// ============================================================

/**
 * @name Interface parallèle 16-bit - Écran ILI9488
 * @{
 */

/** @brief Dimensions de l'écran en pixels */
#define TFT_WIDTH               320
#define TFT_HEIGHT              480

/** @brief Nombre de bits par pixel (RGB565) */
#define TFT_BITS_PER_PIXEL      16

/** @brief Format des couleurs */
#define TFT_PIXEL_FORMAT        RGB565

// --- Broches de contrôle ---

/** @brief Backlight (PWM) */
#define TFT_BL_PORT             GPIOE
#define TFT_BL_PIN              GPIO_PIN_4
#define TFT_BL_TIMER            TIM9
#define TFT_BL_TIMER_CHANNEL    TIM_CHANNEL_4

/** @brief Reset écran */
#define TFT_RST_PORT            GPIOE
#define TFT_RST_PIN             GPIO_PIN_5

/** @brief Chip Select */
#define TFT_CS_PORT             GPIOE
#define TFT_CS_PIN              GPIO_PIN_11

/** @brief Register/Data Select (D/C) */
#define TFT_RS_PORT             GPIOE
#define TFT_RS_PIN              GPIO_PIN_6

/** @brief Write strobe */
#define TFT_WR_PORT             GPIOD
#define TFT_WR_PIN              GPIO_PIN_5

/** @brief Read strobe */
#define TFT_RD_PORT             GPIOD
#define TFT_RD_PIN              GPIO_PIN_4

// --- Bus de données 16 bits (RGB565) ---

/** @brief Data bit 0 (Rouge LSB) */
#define TFT_D0_PORT             GPIOE
#define TFT_D0_PIN              GPIO_PIN_12

/** @brief Data bit 1 */
#define TFT_D1_PORT             GPIOE
#define TFT_D1_PIN              GPIO_PIN_13

/** @brief Data bit 2 */
#define TFT_D2_PORT             GPIOE
#define TFT_D2_PIN              GPIO_PIN_14

/** @brief Data bit 3 */
#define TFT_D3_PORT             GPIOE
#define TFT_D3_PIN              GPIO_PIN_15

/** @brief Data bit 4 (Rouge MSB) */
#define TFT_D4_PORT             GPIOD
#define TFT_D4_PIN              GPIO_PIN_9

/** @brief Data bit 5 (Vert LSB) */
#define TFT_D5_PORT             GPIOD
#define TFT_D5_PIN              GPIO_PIN_15

/** @brief Data bit 6 */
#define TFT_D6_PORT             GPIOD
#define TFT_D6_PIN              GPIO_PIN_0

/** @brief Data bit 7 */
#define TFT_D7_PORT             GPIOD
#define TFT_D7_PIN              GPIO_PIN_1

/** @brief Data bit 8 */
#define TFT_D8_PORT             GPIOE
#define TFT_D8_PIN              GPIO_PIN_7

/** @brief Data bit 9 */
#define TFT_D9_PORT             GPIOE
#define TFT_D9_PIN              GPIO_PIN_8

/** @brief Data bit 10 (Vert MSB) */
#define TFT_D10_PORT            GPIOE
#define TFT_D10_PIN             GPIO_PIN_9

/** @brief Data bit 11 (Bleu LSB) */
#define TFT_D11_PORT            GPIOE
#define TFT_D11_PIN             GPIO_PIN_10

/** @brief Data bit 12 */
#define TFT_D12_PORT            GPIOG
#define TFT_D12_PIN             GPIO_PIN_10

/** @brief Data bit 13 */
#define TFT_D13_PORT            GPIOG
#define TFT_D13_PIN             GPIO_PIN_11

/** @brief Data bit 14 */
#define TFT_D14_PORT            GPIOG
#define TFT_D14_PIN             GPIO_PIN_12

/** @brief Data bit 15 (Bleu MSB) */
#define TFT_D15_PORT            GPIOC
#define TFT_D15_PIN             GPIO_PIN_8

/** @} */ // Fin interface écran

// ============================================================
// SECTION 4 : BROCHES ÉCRAN TACTILE XPT2046
// ============================================================

/**
 * @name Interface I2C1 - Contrôleur tactile XPT2046
 * @{
 */

/** @brief Instance I2C utilisée */
#define TOUCH_I2C_INSTANCE      I2C1
#define TOUCH_I2C_HANDLE        hi2c1

/** @brief Adresse I2C du XPT2046 (7 bits) */
#define TOUCH_I2C_ADDRESS       0x38

/** @brief Horloge I2C (SCL) */
#define TOUCH_SCL_PORT          GPIOB
#define TOUCH_SCL_PIN           GPIO_PIN_6

/** @brief Données I2C (SDA) */
#define TOUCH_SDA_PORT          GPIOB
#define TOUCH_SDA_PIN           GPIO_PIN_7

/** @brief Interruption (tactile détecté) */
#define TOUCH_IRQ_PORT          GPIOB
#define TOUCH_IRQ_PIN           GPIO_PIN_5

/** @brief Reset du contrôleur tactile */
#define TOUCH_RST_PORT          GPIOB
#define TOUCH_RST_PIN           GPIO_PIN_4

/** @} */ // Fin interface tactile

// ============================================================
// SECTION 5 : BROCHES AUDIO (MICRO + HAUT-PARLEUR)
// ============================================================

/**
 * @name Interface audio
 * @{
 */

// --- Microphone ---

/** @brief Instance ADC */
#define MIC_ADC_INSTANCE        ADC1
#define MIC_ADC_HANDLE          hadc1

/** @brief Canal ADC (PA0 = ADC1_IN0) */
#define MIC_ADC_CHANNEL         ADC_CHANNEL_0

/** @brief Port et pin du microphone */
#define MIC_PORT                GPIOA
#define MIC_PIN                 GPIO_PIN_0

/** @brief Fréquence d'échantillonnage audio */
#define AUDIO_SAMPLE_RATE       8000

/** @brief Taille du buffer audio (échantillons) */
#define AUDIO_BUFFER_SIZE       256

// --- Haut-parleur ---

/** @brief Instance DAC */
#define SPK_DAC_INSTANCE        DAC1
#define SPK_DAC_HANDLE          hdac

/** @brief Canal DAC (PA5 = DAC_OUT2) */
#define SPK_DAC_CHANNEL         DAC_CHANNEL_2

/** @brief Port et pin du haut-parleur */
#define SPK_PORT                GPIOA
#define SPK_PIN                 GPIO_PIN_5

/** @brief Timer déclenchant la lecture audio */
#define AUDIO_TIMER_INSTANCE    TIM6
#define AUDIO_TIMER_HANDLE      htim6

/** @} */ // Fin interface audio

// ============================================================
// SECTION 6 : BROCHES CLAVIER MATRICIEL 6x4
// ============================================================

/**
 * @name Clavier matriciel 24 touches
 * @{
 */

/** @brief Nombre de lignes et colonnes */
#define KEYPAD_ROWS             6
#define KEYPAD_COLS             4

/** @brief Port pour toutes les LIGNES (sorties) */
#define KEYPAD_ROW_PORT         GPIOD

/** @brief Ligne 0 - Touches : Menu, Haut, OK, Bas */
#define KEYPAD_ROW0_PIN         GPIO_PIN_0
/** @brief Ligne 1 - Touches : 1, 2, 3, Retour */
#define KEYPAD_ROW1_PIN         GPIO_PIN_1
/** @brief Ligne 2 - Touches : 4, 5, 6, Appel */
#define KEYPAD_ROW2_PIN         GPIO_PIN_2
/** @brief Ligne 3 - Touches : 7, 8, 9, Raccrocher */
#define KEYPAD_ROW3_PIN         GPIO_PIN_3
/** @brief Ligne 4 - Touches : *, 0, #, MAJ */
#define KEYPAD_ROW4_PIN         GPIO_PIN_4
/** @brief Ligne 5 - Touches : Lampe, PTT, Mute, Vol */
#define KEYPAD_ROW5_PIN         GPIO_PIN_5

/** @brief Port pour toutes les COLONNES (entrées) */
#define KEYPAD_COL_PORT         GPIOE

/** @brief Colonne 0 */
#define KEYPAD_COL0_PIN         GPIO_PIN_8
/** @brief Colonne 1 */
#define KEYPAD_COL1_PIN         GPIO_PIN_9
/** @brief Colonne 2 */
#define KEYPAD_COL2_PIN         GPIO_PIN_10
/** @brief Colonne 3 */
#define KEYPAD_COL3_PIN         GPIO_PIN_11

/** @brief Tableau des broches de lignes */
static const uint16_t KEYPAD_ROW_PINS[KEYPAD_ROWS] = {
    KEYPAD_ROW0_PIN,
    KEYPAD_ROW1_PIN,
    KEYPAD_ROW2_PIN,
    KEYPAD_ROW3_PIN,
    KEYPAD_ROW4_PIN,
    KEYPAD_ROW5_PIN
};

/** @brief Tableau des broches de colonnes */
static const uint16_t KEYPAD_COL_PINS[KEYPAD_COLS] = {
    KEYPAD_COL0_PIN,
    KEYPAD_COL1_PIN,
    KEYPAD_COL2_PIN,
    KEYPAD_COL3_PIN
};

/**
 * @brief Mapping des touches (matrice 6x4)
 *
 *        Col0    Col1    Col2    Col3
 * Row0   MENU    HAUT    OK      BAS
 * Row1   1       2       3       RETOUR
 * Row2   4       5       6       APPEL
 * Row3   7       8       9       RACC
 * Row4   *       0       #       MAJ
 * Row5   LAMPE   PTT     MUET    VOL
 */
#define KEYPAD_MAP_ROWS         6
#define KEYPAD_MAP_COLS         4

/** @} */ // Fin clavier

// ============================================================
// SECTION 7 : BROCHES LED ET RÉTROÉCLAIRAGE
// ============================================================

/**
 * @name LEDs et éclairage
 * @{
 */

/** @brief LED statut (intégrée Discovery) */
#define STATUS_LED_PORT         GPIOG
#define STATUS_LED_PIN          GPIO_PIN_13

/** @brief LED lampe torche */
#define LAMP_LED_PORT           GPIOE
#define LAMP_LED_PIN            GPIO_PIN_6
#define LAMP_TIMER              TIM9
#define LAMP_TIMER_CHANNEL      TIM_CHANNEL_1

/** @brief Rétroéclairage clavier */
#define KEYPAD_BL_PORT          GPIOG
#define KEYPAD_BL_PIN           GPIO_PIN_13
#define KEYPAD_BL_TIMER         TIM4
#define KEYPAD_BL_TIMER_CHANNEL TIM_CHANNEL_3

/** @} */ // Fin LEDs

// ============================================================
// SECTION 8 : BROCHES UART DEBUG
// ============================================================

/**
 * @name Interface UART Debug (USART1)
 * @{
 */

/** @brief Instance UART */
#define DEBUG_UART_INSTANCE     USART1
#define DEBUG_UART_HANDLE       huart1
#define DEBUG_UART_BAUD         115200

/** @brief TX (connecté à ST-LINK RX) */
#define DEBUG_TX_PORT           GPIOA
#define DEBUG_TX_PIN            GPIO_PIN_9

/** @brief RX (connecté à ST-LINK TX) */
#define DEBUG_RX_PORT           GPIOA
#define DEBUG_RX_PIN            GPIO_PIN_10

/** @} */ // Fin UART debug

// ============================================================
// SECTION 9 : ADRESSES MÉMOIRE
// ============================================================

/**
 * @name Mapping mémoire
 * @{
 */

#if PLATFORM_HAS_SDRAM
    /** @brief Adresse de base de la SDRAM */
    #define SDRAM_BASE_ADDR         0xC0000000UL
    /** @brief Taille de la SDRAM (8 Mo) */
    #define SDRAM_SIZE              0x800000UL

    /** @brief Adresse du framebuffer principal */
    #define FRAMEBUFFER_ADDR        SDRAM_BASE_ADDR
    /** @brief Adresse du second framebuffer (double buffering) */
    #define FRAMEBUFFER2_ADDR       (SDRAM_BASE_ADDR + (TFT_WIDTH * TFT_HEIGHT * 2))
#endif

/** @brief Adresse de base de la Flash interne */
#define FLASH_BASE_ADDR         0x08000000UL
/** @brief Taille de la Flash (2 Mo) */
#define FLASH_SIZE              0x200000UL

/** @brief Adresse de base de la RAM interne */
#define RAM_BASE_ADDR           0x20000000UL
/** @brief Taille de la RAM (256 Ko) */
#define RAM_SIZE                0x40000UL

/** @brief Adresse de base de la CCM RAM */
#define CCM_RAM_BASE_ADDR       0x10000000UL
/** @brief Taille de la CCM RAM (64 Ko) */
#define CCM_RAM_SIZE            0x10000UL

/** @brief Secteur Flash réservé aux paramètres */
#define SETTINGS_FLASH_ADDR     0x080E0000UL     // Secteur 11 (dernier secteur 128 Ko)
#define SETTINGS_FLASH_SIZE     0x20000UL        // 128 Ko

/** @} */ // Fin mapping mémoire

// ============================================================
// SECTION 10 : CONSTANTES PHYSIQUES
// ============================================================

/**
 * @name Constantes diverses
 * @{
 */

/** @brief Fréquence du quartz HSE */
#define HSE_FREQ_HZ             8000000UL

/** @brief Fréquence du quartz LSE */
#define LSE_FREQ_HZ             32768UL

/** @brief Tension de référence ADC (V) */
#define ADC_REF_VOLTAGE         3.3f

/** @brief Résolution ADC (bits) */
#define ADC_RESOLUTION_BITS     12
#define ADC_MAX_VALUE           ((1 << ADC_RESOLUTION_BITS) - 1)  // 4095

/** @brief Résolution DAC (bits) */
#define DAC_RESOLUTION_BITS     12
#define DAC_MAX_VALUE           ((1 << DAC_RESOLUTION_BITS) - 1)  // 4095

/** @} */ // Fin constantes

// ============================================================
// SECTION 11 : MACROS DE CONTRÔLE RAPIDE
// ============================================================

/**
 * @name Macros de manipulation des broches
 * @{
 */

// --- Module LoRa ---
#define LORA_CS_LOW()           HAL_GPIO_WritePin(LORA_CS_PORT, LORA_CS_PIN, GPIO_PIN_RESET)
#define LORA_CS_HIGH()          HAL_GPIO_WritePin(LORA_CS_PORT, LORA_CS_PIN, GPIO_PIN_SET)
#define LORA_RESET()            do { \
                                    HAL_GPIO_WritePin(LORA_RST_PORT, LORA_RST_PIN, GPIO_PIN_RESET); \
                                    HAL_Delay(10); \
                                    HAL_GPIO_WritePin(LORA_RST_PORT, LORA_RST_PIN, GPIO_PIN_SET); \
                                    HAL_Delay(10); \
                                } while(0)
#define LORA_DIO0_READ()        HAL_GPIO_ReadPin(LORA_DIO0_PORT, LORA_DIO0_PIN)

// --- Écran TFT ---
#define TFT_BL_ON()             HAL_GPIO_WritePin(TFT_BL_PORT, TFT_BL_PIN, GPIO_PIN_SET)
#define TFT_BL_OFF()            HAL_GPIO_WritePin(TFT_BL_PORT, TFT_BL_PIN, GPIO_PIN_RESET)
#define TFT_RESET()             do { \
                                    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_RESET); \
                                    HAL_Delay(10); \
                                    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET); \
                                    HAL_Delay(120); \
                                } while(0)
#define TFT_CS_LOW()            HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_RESET)
#define TFT_CS_HIGH()           HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET)
#define TFT_RS_LOW()            HAL_GPIO_WritePin(TFT_RS_PORT, TFT_RS_PIN, GPIO_PIN_RESET)  // Commande
#define TFT_RS_HIGH()           HAL_GPIO_WritePin(TFT_RS_PORT, TFT_RS_PIN, GPIO_PIN_SET)    // Data
#define TFT_WR_LOW()            HAL_GPIO_WritePin(TFT_WR_PORT, TFT_WR_PIN, GPIO_PIN_RESET)
#define TFT_WR_HIGH()           HAL_GPIO_WritePin(TFT_WR_PORT, TFT_WR_PIN, GPIO_PIN_SET)
#define TFT_WR_PULSE()          do { TFT_WR_LOW(); __NOP(); TFT_WR_HIGH(); } while(0)

// --- LED Statut ---
#define STATUS_LED_ON()         HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN, GPIO_PIN_SET)
#define STATUS_LED_OFF()        HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN, GPIO_PIN_RESET)
#define STATUS_LED_TOGGLE()     HAL_GPIO_TogglePin(STATUS_LED_PORT, STATUS_LED_PIN)

// --- Lampe torche ---
#define LAMP_ON()               HAL_GPIO_WritePin(LAMP_LED_PORT, LAMP_LED_PIN, GPIO_PIN_SET)
#define LAMP_OFF()              HAL_GPIO_WritePin(LAMP_LED_PORT, LAMP_LED_PIN, GPIO_PIN_RESET)
#define LAMP_TOGGLE()           HAL_GPIO_TogglePin(LAMP_LED_PORT, LAMP_LED_PIN)

/** @} */ // Fin macros contrôle

// ============================================================
// SECTION 12 : MACROS UTILITAIRES
// ============================================================

/**
 * @name Macros pratiques
 * @{
 */

/** @brief Taille d'un tableau statique */
#define ARRAY_SIZE(arr)         (sizeof(arr) / sizeof((arr)[0]))

/** @brief Minimum de deux valeurs */
#define MIN(a, b)               ((a) < (b) ? (a) : (b))

/** @brief Maximum de deux valeurs */
#define MAX(a, b)               ((a) > (b) ? (a) : (b))

/** @brief Contraindre une valeur entre min et max */
#define CLAMP(x, min, max)      MIN(MAX((x), (min)), (max))

/** @brief Valeur absolue */
#define ABS(x)                  ((x) < 0 ? -(x) : (x))

/** @brief Convertir degrés en radians */
#define DEG_TO_RAD(deg)         ((deg) * 3.14159265359f / 180.0f)

/** @brief Convertir radians en degrés */
#define RAD_TO_DEG(rad)         ((rad) * 180.0f / 3.14159265359f)

/** @brief Mapper une valeur d'une plage à une autre */
#define MAP(val, in_min, in_max, out_min, out_max) \
    (((val) - (in_min)) * ((out_max) - (out_min)) / ((in_max) - (in_min)) + (out_min))

/** @brief Supprimer l'avertissement "unused variable" */
#define UNUSED(x)               ((void)(x))

/** @brief Délai en microsecondes (approximatif, basé sur CPU 180 MHz) */
#define DELAY_US(us)            for(volatile uint32_t _d=0; _d<((us)*36); _d++)

/** @} */ // Fin macros utilitaires

// ============================================================
// SECTION 13 : VÉRIFICATIONS DE CONFIGURATION
// ============================================================

#if !defined(LORA_CS_PORT) || !defined(LORA_CS_PIN)
    #error "Broches LoRa non définies ! Vérifiez config.h"
#endif

#if !defined(TFT_BL_PORT) || !defined(TFT_BL_PIN)
    #error "Broches écran non définies ! Vérifiez config.h"
#endif

#if !defined(MIC_PORT) || !defined(MIC_PIN)
    #error "Broches microphone non définies ! Vérifiez config.h"
#endif

#if !defined(KEYPAD_ROW_PORT) || !defined(KEYPAD_COL_PORT)
    #error "Broches clavier non définies ! Vérifiez config.h"
#endif

// ============================================================
// SECTION 14 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H