/**
 * @file platform.h
 * @brief Détection et configuration de la plateforme matérielle
 * 
 * Ce fichier détecte automatiquement la carte utilisée et
 * définit les constantes spécifiques à chaque plateforme.
 * 
 * Plateformes supportées :
 * - STM32F429I-DISCOVERY (disco_f429zi)
 * - NUCLEO-F429ZI (nucleo_f429zi)
 * - Carte personnalisée (genericSTM32F429ZI)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// SECTION 1 : DÉTECTION AUTOMATIQUE DE LA PLATEFORME
// ============================================================

/**
 * @def PLATFORM_DISCOVERY
 * @brief Défini si la carte est une STM32F429I-DISCOVERY
 * 
 * Détecté automatiquement via les flags de compilation (-D)
 * ou les définitions du HAL STM32.
 */
#if defined(STM32F429I_DISCO) || defined(PLATFORM_DISCOVERY)
    #define PLATFORM_IS_DISCOVERY       1
    #define PLATFORM_NAME               "STM32F429I-DISCOVERY"
    #define PLATFORM_HAS_SDRAM           1
    #define PLATFORM_HAS_LTDC            1
    #define PLATFORM_HAS_DMA2D           1
    #define PLATFORM_HAS_TOUCHSCREEN     1
    #define PLATFORM_HAS_LED_USER        1
    #define PLATFORM_HAS_BUTTON_USER     1
    #define PLATFORM_HAS_STLINK          1
    #define PLATFORM_HAS_USB_OTG         1

/**
 * @def PLATFORM_NUCLEO
 * @brief Défini si la carte est une NUCLEO-F429ZI
 */
#elif defined(STM32F429I_NUCLEO) || defined(NUCLEO_BOARD)
    #define PLATFORM_IS_NUCLEO          1
    #define PLATFORM_NAME               "NUCLEO-F429ZI"
    #define PLATFORM_HAS_SDRAM           0
    #define PLATFORM_HAS_LTDC            0
    #define PLATFORM_HAS_DMA2D           0
    #define PLATFORM_HAS_TOUCHSCREEN     0
    #define PLATFORM_HAS_LED_USER        1
    #define PLATFORM_HAS_BUTTON_USER     1
    #define PLATFORM_HAS_STLINK          1
    #define PLATFORM_HAS_USB_OTG         1

/**
 * @def PLATFORM_GENERIC
 * @brief Défini si la carte est une carte STM32F429 générique
 */
#else
    #define PLATFORM_IS_GENERIC         1
    #define PLATFORM_NAME               "Generic STM32F429"
    #define PLATFORM_HAS_SDRAM           0
    #define PLATFORM_HAS_LTDC            0
    #define PLATFORM_HAS_DMA2D           0
    #define PLATFORM_HAS_TOUCHSCREEN     0
    #define PLATFORM_HAS_LED_USER        0
    #define PLATFORM_HAS_BUTTON_USER     0
    #define PLATFORM_HAS_STLINK          0
    #define PLATFORM_HAS_USB_OTG         0
    
    #warning "Plateforme non reconnue - Configuration générique utilisée"
#endif

// ============================================================
// SECTION 2 : INFORMATIONS DU MICROCONTRÔLEUR
// ============================================================

/**
 * @def MCU_FAMILY
 * @brief Famille du microcontrôleur
 */
#define MCU_FAMILY                  "STM32F4"

/**
 * @def MCU_MODEL
 * @brief Modèle exact du microcontrôleur
 */
#define MCU_MODEL                   "STM32F429ZIT6"

/**
 * @def MCU_CORE
 * @brief Cœur du processeur
 */
#define MCU_CORE                    "ARM Cortex-M4"

/**
 * @def MCU_CORE_VERSION
 * @brief Version du cœur (r0p1)
 */
#define MCU_CORE_VERSION            "r0p1"

/**
 * @def MCU_REVISION
 * @brief Révision du silicium
 */
#define MCU_REVISION                0x1001          // Rev Y (1)

// ============================================================
// SECTION 3 : FRÉQUENCES D'HORLOGE
// ============================================================

/**
 * @def SYSTEM_CLOCK_HZ
 * @brief Fréquence de l'horloge système (SYSCLK)
 * 
 * 180 MHz = 180 000 000 Hz
 */
#define SYSTEM_CLOCK_HZ             180000000UL

/**
 * @def SYSTEM_CLOCK_MHZ
 * @brief Fréquence SYSCLK en MHz
 */
#define SYSTEM_CLOCK_MHZ            180

/**
 * @def HCLK_HZ
 * @brief Fréquence du bus AHB (HCLK)
 * 
 * Sur STM32F429 : HCLK = SYSCLK = 180 MHz
 */
#define HCLK_HZ                     180000000UL

/**
 * @def PCLK1_HZ
 * @brief Fréquence du bus APB1 (PCLK1)
 * 
 * APB1 max = 45 MHz, prescaler = /4
 */
#define PCLK1_HZ                    45000000UL

/**
 * @def PCLK2_HZ
 * @brief Fréquence du bus APB2 (PCLK2)
 * 
 * APB2 max = 90 MHz, prescaler = /2
 */
#define PCLK2_HZ                    90000000UL

/**
 * @def HSE_VALUE
 * @brief Fréquence du quartz externe (HSE)
 */
#define HSE_FREQ_HZ                 8000000UL

/**
 * @def LSE_VALUE
 * @brief Fréquence du quartz 32.768 kHz (LSE)
 */
#define LSE_FREQ_HZ                 32768UL

/**
 * @def HSI_VALUE
 * @brief Fréquence de l'oscillateur interne (HSI)
 */
#define HSI_FREQ_HZ                 16000000UL

// ============================================================
// SECTION 4 : MÉMOIRE
// ============================================================

/**
 * @def FLASH_SIZE_BYTES
 * @brief Taille de la mémoire Flash interne
 * 
 * STM32F429ZI : 2 Mo
 */
#define FLASH_SIZE_BYTES            0x200000        // 2 097 152 octets
#define FLASH_SIZE_KB               2048
#define FLASH_SIZE_MB               2

/**
 * @def FLASH_BASE
 * @brief Adresse de base de la Flash
 */
#define FLASH_BASE_ADDR             0x08000000UL

/**
 * @def FLASH_SECTOR_SIZE
 * @brief Taille d'un secteur Flash
 */
#define FLASH_SECTOR_SIZE           0x4000          // 16 Ko (secteurs 0-11)
#define FLASH_SECTOR_COUNT          24

/**
 * @def RAM_SIZE_BYTES
 * @brief Taille de la RAM interne
 * 
 * STM32F429ZI : 256 Ko (192 Ko SRAM1 + 64 Ko SRAM2)
 */
#define RAM_SIZE_BYTES              0x40000         // 262 144 octets
#define RAM_SIZE_KB                 256
#define RAM_BASE_ADDR               0x20000000UL

/**
 * @def CCM_RAM_SIZE
 * @brief Taille de la CCM RAM (Core Coupled Memory)
 * 
 * 64 Ko de RAM rapide directement connectée au CPU
 */
#define CCM_RAM_SIZE_BYTES          0x10000         // 65 536 octets
#define CCM_RAM_SIZE_KB             64
#define CCM_RAM_BASE_ADDR           0x10000000UL

#if PLATFORM_HAS_SDRAM
    /**
     * @def SDRAM_SIZE
     * @brief Taille de la SDRAM externe
     * 
     * Discovery : 64 Mbits = 8 Mo
     */
    #define SDRAM_SIZE_BYTES        0x800000        // 8 388 608 octets
    #define SDRAM_SIZE_KB           8192
    #define SDRAM_SIZE_MB           8
    #define SDRAM_BASE_ADDR         0xC0000000UL
    #define SDRAM_CLOCK_HZ          90000000UL      // 90 MHz (HCLK/2)
#endif

// ============================================================
// SECTION 5 : PÉRIPHÉRIQUES DISPONIBLES
// ============================================================

/**
 * @def HAS_FPU
 * @brief FPU (Floating Point Unit) disponible
 * 
 * Le Cortex-M4 intègre un FPU simple précision
 */
#define HAS_FPU                     1
#define FPU_TYPE                    "fpv4-sp-d16"

/**
 * @def HAS_DSP
 * @brief Instructions DSP disponibles
 * 
 * Le Cortex-M4 intègre des instructions DSP
 */
#define HAS_DSP                     1

/**
 * @def HAS_MPU
 * @brief MPU (Memory Protection Unit) disponible
 */
#define HAS_MPU                     1

/**
 * @def HAS_ART_ACCELERATOR
 * @brief Accélérateur ART (Adaptive Real-Time)
 * 
 * Accélère les accès à la Flash interne
 */
#define HAS_ART_ACCELERATOR         1

/**
 * @def HAS_ITM
 * @brief ITM (Instrumentation Trace Macrocell) disponible
 * 
 * Permet le tracing et les printf en debug
 */
#define HAS_ITM                     1

// ============================================================
// SECTION 6 : BROCHAGE SPÉCIFIQUE À LA PLATEFORME
// ============================================================

/**
 * @name Pins communes à toutes les plateformes
 * @{
 */
#define LED_BUILTIN_PORT            GPIOG
#define LED_BUILTIN_PIN             GPIO_PIN_13
#define LED_BUILTIN_ACTIVE_STATE    GPIO_PIN_SET     // HIGH = allumée
/** @} */

#if PLATFORM_IS_DISCOVERY
    /**
     * @name Pins spécifiques à la Discovery
     * @{
     */
    
    // --- LED utilisateur ---
    #define USER_LED_PORT           GPIOG
    #define USER_LED_PIN            GPIO_PIN_13      // LED orange
    #define USER_LED2_PORT          GPIOG
    #define USER_LED2_PIN           GPIO_PIN_14      // LED rouge
    
    // --- Bouton utilisateur ---
    #define USER_BUTTON_PORT        GPIOA
    #define USER_BUTTON_PIN         GPIO_PIN_0
    #define USER_BUTTON_ACTIVE      GPIO_PIN_RESET    // Appui = LOW
    
    // --- Écran intégré (debug) ---
    #define DISCOVERY_LCD_WIDTH     240
    #define DISCOVERY_LCD_HEIGHT    320
    #define DISCOVERY_LCD_SPI       SPI5
    
    // --- ST-LINK ---
    #define STLINK_SWCLK_PORT       GPIOA
    #define STLINK_SWCLK_PIN        GPIO_PIN_14
    #define STLINK_SWDIO_PORT       GPIOA
    #define STLINK_SWDIO_PIN        GPIO_PIN_13
    #define STLINK_UART_TX_PORT     GPIOD
    #define STLINK_UART_TX_PIN      GPIO_PIN_8
    #define STLINK_UART_RX_PORT     GPIOD
    #define STLINK_UART_RX_PIN      GPIO_PIN_9
    
    // --- Gyroscope L3GD20 (intégré) ---
    #define HAS_GYROSCOPE           1
    #define GYRO_SPI                SPI5
    #define GYRO_CS_PORT            GPIOC
    #define GYRO_CS_PIN             GPIO_PIN_1
    #define GYRO_INT1_PORT          GPIOA
    #define GYRO_INT1_PIN           GPIO_PIN_1
    #define GYRO_INT2_PORT          GPIOA
    #define GYRO_INT2_PIN           GPIO_PIN_2
    
    /** @} */

#elif PLATFORM_IS_NUCLEO
    /**
     * @name Pins spécifiques à la NUCLEO
     * @{
     */
    
    // --- LED utilisateur ---
    #define USER_LED_PORT           GPIOB
    #define USER_LED_PIN            GPIO_PIN_0       // LED verte (LD1)
    #define USER_LED2_PORT          GPIOB
    #define USER_LED2_PIN           GPIO_PIN_7       // LED bleue (LD2)
    #define USER_LED3_PORT          GPIOB
    #define USER_LED3_PIN           GPIO_PIN_14      // LED rouge (LD3)
    
    // --- Bouton utilisateur ---
    #define USER_BUTTON_PORT        GPIOC
    #define USER_BUTTON_PIN         GPIO_PIN_13
    #define USER_BUTTON_ACTIVE      GPIO_PIN_RESET
    
    // --- Connecteurs Arduino Uno V3 ---
    #define ARDUINO_D0_PORT         GPIOD
    #define ARDUINO_D0_PIN          GPIO_PIN_0       // RX
    #define ARDUINO_D1_PORT         GPIOD
    #define ARDUINO_D1_PIN          GPIO_PIN_1       // TX
    #define ARDUINO_D2_PORT         GPIOD
    #define ARDUINO_D2_PIN          GPIO_PIN_4
    #define ARDUINO_D3_PORT         GPIOD
    #define ARDUINO_D3_PIN          GPIO_PIN_6
    #define ARDUINO_D4_PORT         GPIOD
    #define ARDUINO_D4_PIN          GPIO_PIN_3
    #define ARDUINO_D5_PORT         GPIOD
    #define ARDUINO_D5_PIN          GPIO_PIN_7
    #define ARDUINO_D6_PORT         GPIOB
    #define ARDUINO_D6_PIN          GPIO_PIN_4
    #define ARDUINO_D7_PORT         GPIOB
    #define ARDUINO_D7_PIN          GPIO_PIN_5
    #define ARDUINO_A0_PORT         GPIOA
    #define ARDUINO_A0_PIN          GPIO_PIN_0
    #define ARDUINO_A1_PORT         GPIOA
    #define ARDUINO_A1_PIN          GPIO_PIN_1
    
    // --- ST morpho connector pins ---
    #define MORPHO_CN10_PIN1_PORT   GPIOA
    #define MORPHO_CN10_PIN1_PIN    GPIO_PIN_9
    
    /** @} */

#else
    /**
     * @name Pins pour carte générique
     * @{
     */
    
    // À définir par l'utilisateur selon sa carte
    #ifndef USER_LED_PORT
        #define USER_LED_PORT       GPIOC
        #define USER_LED_PIN        GPIO_PIN_13
    #endif
    
    #ifndef USER_BUTTON_PORT
        #define USER_BUTTON_PORT    GPIOA
        #define USER_BUTTON_PIN     GPIO_PIN_0
    #endif
    
    /** @} */
#endif

// ============================================================
// SECTION 7 : COMPATIBILITÉ DES FONCTIONNALITÉS
// ============================================================

/**
 * @brief Vérifications de compatibilité à la compilation
 * 
 * Ces vérifications empêchent de compiler avec une configuration
 * incompatible avec la plateforme détectée.
 */

// L'écran parallèle (LTDC) nécessite la SDRAM
#if defined(ENABLE_DISPLAY) && defined(USE_LTDC)
    #if !PLATFORM_HAS_SDRAM && !PLATFORM_HAS_LTDC
        #error "LTDC et SDRAM requis pour l'écran parallèle. Utilisez SPI ou une carte compatible."
    #endif
#endif

// DMA2D nécessite le LTDC
#if defined(ENABLE_DISPLAY) && defined(USE_DMA2D)
    #if !PLATFORM_HAS_DMA2D
        #error "DMA2D non disponible sur cette plateforme."
    #endif
#endif

// ============================================================
// SECTION 8 : FONCTIONS UTILITAIRES
// ============================================================

/**
 * @brief Obtient le nom de la plateforme
 * @return Chaîne statique contenant le nom
 */
static inline const char* platform_get_name(void) {
    return PLATFORM_NAME;
}

/**
 * @brief Vérifie si la plateforme supporte une fonctionnalité
 * @param feature Nom de la fonctionnalité
 * @return true si supportée
 */
static inline bool platform_has_feature(const char* feature) {
    if (strcmp(feature, "sdram") == 0) return PLATFORM_HAS_SDRAM;
    if (strcmp(feature, "ltdc") == 0)  return PLATFORM_HAS_LTDC;
    if (strcmp(feature, "dma2d") == 0) return PLATFORM_HAS_DMA2D;
    if (strcmp(feature, "touch") == 0) return PLATFORM_HAS_TOUCHSCREEN;
    if (strcmp(feature, "stlink") == 0) return PLATFORM_HAS_STLINK;
    return false;
}

/**
 * @brief Affiche les informations de la plateforme
 */
static inline void platform_print_info(void) {
    printf("╔══════════════════════════════════════════╗\n");
    printf("║       INFORMATIONS PLATEFORME            ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Carte:      %-29s ║\n", PLATFORM_NAME);
    printf("║ MCU:        %-29s ║\n", MCU_MODEL);
    printf("║ Cœur:       %-29s ║\n", MCU_CORE);
    printf("║ Fréquence:  %-3lu MHz                     ║\n", SYSTEM_CLOCK_MHZ);
    printf("║ Flash:      %-4lu Ko                     ║\n", FLASH_SIZE_KB);
    printf("║ RAM:        %-4lu Ko                     ║\n", RAM_SIZE_KB);
#if PLATFORM_HAS_SDRAM
    printf("║ SDRAM:      %-4lu Mo                     ║\n", SDRAM_SIZE_MB);
#endif
    printf("║ FPU:        %-29s ║\n", HAS_FPU ? "Oui (fpv4-sp-d16)" : "Non");
    printf("║ DSP:        %-29s ║\n", HAS_DSP ? "Oui" : "Non");
    printf("║ ST-LINK:    %-29s ║\n", PLATFORM_HAS_STLINK ? "Intégré" : "Externe");
    printf("╚══════════════════════════════════════════╝\n");
}

// ============================================================
// SECTION 9 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_H