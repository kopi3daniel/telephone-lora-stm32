/**
 * @file system_init.h
 * @brief Fonctions d'initialisation du système
 * 
 * Ce fichier déclare les prototypes de toutes les fonctions
 * d'initialisation du microcontrôleur et de ses périphériques.
 * 
 * Il est le pendant de "main.h" généré par CubeMX, mais avec
 * une organisation plus claire et des commentaires détaillés.
 * 
 * Les fonctions sont implémentées dans system_init.c
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES NÉCESSAIRES
// ============================================================

#include "stm32f4xx_hal.h"
#include "config.h"
#include "include/project_config.h"

// ============================================================
// SECTION 1 : INITIALISATION PRINCIPALE DU SYSTÈME
// ============================================================

/**
 * @brief Initialise l'horloge système (SYSCLK)
 * 
 * Configure le PLL pour générer 180 MHz à partir du HSE 8 MHz.
 * 
 * Configuration :
 * - HSE = 8 MHz (quartz externe)
 * - PLLM = 8 (HSE / 8 = 1 MHz)
 * - PLLN = 360 (1 MHz × 360 = 360 MHz VCO)
 * - PLLP = 2 (360 MHz / 2 = 180 MHz SYSCLK)
 * - PLLQ = 7 (360 MHz / 7 ≈ 51.4 MHz pour USB OTG)
 * 
 * Résultat :
 * - SYSCLK = 180 MHz
 * - HCLK   = 180 MHz
 * - PCLK1  = 45 MHz (APB1, /4)
 * - PCLK2  = 90 MHz (APB2, /2)
 */
void MX_SystemClock_Config(void);

/**
 * @brief Initialise tous les GPIO du projet
 * 
 * Configure :
 * - Sorties : LEDs, Reset LoRa, contrôle écran
 * - Entrées : Boutons, interruptions externes
 * - Analogique : Microphone, Haut-parleur
 * - Alternate Functions : SPI, I2C, UART, LTDC, FMC
 */
void MX_GPIO_Init(void);

/**
 * @brief Configure le contrôleur d'interruptions (NVIC)
 * 
 * Définit :
 * - Le nombre de bits de priorité (4 bits = 16 niveaux)
 * - La priorité de chaque interruption
 * - L'activation des interruptions utilisées
 */
void MX_NVIC_Init(void);

/**
 * @brief Configure le contrôleur DMA
 * 
 * Initialise les canaux DMA pour :
 * - SPI2 TX/RX (LoRa)
 * - ADC1 (Microphone)
 * - DAC1 (Haut-parleur)
 * - USART1 TX/RX (Debug)
 */
void MX_DMA_Init(void);

// ============================================================
// SECTION 2 : INITIALISATION DES PÉRIPHÉRIQUES
// ============================================================

/**
 * @brief Initialise l'UART de debug (USART1)
 * 
 * Configuration :
 * - Baudrate : 115200
 * - Data bits : 8
 * - Stop bits : 1
 * - Parité : Aucune
 * - Flow control : Aucun
 */
void MX_USART1_UART_Init(void);

/**
 * @brief Initialise le SPI pour le module LoRa (SPI2)
 * 
 * Configuration :
 * - Mode : Master
 * - Data size : 8 bits
 * - Clock polarity : Low (CPOL=0)
 * - Clock phase : 1 Edge (CPHA=0)
 * - Baudrate : PCLK1/4 = 11.25 MHz
 * - NSS : Software (géré manuellement)
 */
void MX_SPI2_Init(void);

/**
 * @brief Initialise l'I2C pour l'écran tactile (I2C1)
 * 
 * Configuration :
 * - Mode : Master
 * - Speed : 400 kHz (Fast Mode)
 * - Addressing : 7 bits
 */
void MX_I2C1_Init(void);

/**
 * @brief Initialise l'ADC pour le microphone (ADC1)
 * 
 * Configuration :
 * - Résolution : 12 bits
 * - Mode : Conversion simple (déclenchée par timer)
 * - Canal : IN0 (PA0)
 * - DMA : Mode circulaire
 */
void MX_ADC1_Init(void);

/**
 * @brief Initialise le DAC pour le haut-parleur
 * 
 * Configuration :
 * - Résolution : 12 bits
 * - Canal : OUT2 (PA5)
 * - DMA : Mode circulaire
 * - Trigger : Timer 6 (8 kHz)
 */
void MX_DAC_Init(void);

/**
 * @brief Initialise le Timer 6 (Base de temps audio)
 * 
 * Configuration :
 * - Fréquence : 8 kHz (125 µs)
 * - Prescaler : 0
 * - Period : 22499 (180MHz / 22500 = 8000 Hz)
 * - Mode : Up counter
 */
void MX_TIM6_Init(void);

/**
 * @brief Initialise le Timer 4 (PWM Backlight clavier)
 * 
 * Configuration :
 * - Fréquence PWM : 5 kHz
 * - Résolution : 8 bits (0-255)
 * - Canal : CH3 (PG13)
 */
void MX_TIM4_Init(void);

/**
 * @brief Initialise le Timer 9 (PWM Lampe + Backlight écran)
 * 
 * Configuration :
 * - Fréquence PWM : 5 kHz
 * - Résolution : 8 bits (0-255)
 * - Canal 1 : Lampe torche (PE6)
 * - Canal 4 : Backlight écran (PE4)
 */
void MX_TIM9_Init(void);

// ============================================================
// SECTION 3 : INITIALISATION DES PÉRIPHÉRIQUES AVANCÉS
// ============================================================

#if PLATFORM_HAS_LTDC
/**
 * @brief Initialise le contrôleur LTDC (Écran TFT ILI9488)
 * 
 * Configuration :
 * - Résolution : 320 × 480
 * - Format pixel : RGB565 (16 bits)
 * - Pixel Clock : 10 MHz
 * - HSYNC : 10, HBP : 10, HFP : 20
 * - VSYNC : 2, VBP : 2, VFP : 1
 * - Couche 1 : Framebuffer en SDRAM
 * 
 * ⚠️ Nécessite SDRAM initialisée
 */
void MX_LTDC_Init(void);

/**
 * @brief Initialise l'accélérateur graphique DMA2D
 * 
 * Configuration :
 * - Mode : Register-to-Memory (remplissage)
 * - Format sortie : RGB565
 * 
 * Utilisé pour :
 * - Remplissage rapide de rectangles
 * - Copie d'images avec conversion de format
 * - Alpha blending
 */
void MX_DMA2D_Init(void);
#endif

#if PLATFORM_HAS_SDRAM
/**
 * @brief Initialise la SDRAM externe (8 Mo sur Discovery)
 * 
 * Configuration :
 * - Taille : 64 Mbits (8 Mo)
 * - Bus : 16 bits
 * - Horloge : 90 MHz (HCLK/2)
 * - CAS Latency : 2 cycles
 * - Bank : 4 banks internes
 * - Adresse de base : 0xC0000000
 * 
 * ⚠️ Doit être appelée avant MX_LTDC_Init()
 */
void MX_FMC_SDRAM_Init(void);
#endif

// ============================================================
// SECTION 4 : FONCTIONS UTILITAIRES D'INITIALISATION
// ============================================================

/**
 * @brief Initialise tous les périphériques dans l'ordre correct
 * 
 * Ordre d'initialisation :
 * 1. HAL_Init()
 * 2. SystemClock_Config()
 * 3. MX_GPIO_Init()
 * 4. MX_DMA_Init()
 * 5. MX_NVIC_Init()
 * 6. MX_FMC_SDRAM_Init() (si disponible)
 * 7. MX_LTDC_Init() + MX_DMA2D_Init() (si disponible)
 * 8. MX_USART1_UART_Init()
 * 9. MX_SPI2_Init()
 * 10. MX_I2C1_Init()
 * 11. MX_ADC1_Init()
 * 12. MX_DAC_Init()
 * 13. MX_TIM6_Init()
 * 14. MX_TIM4_Init()
 * 15. MX_TIM9_Init()
 */
void MX_InitAll(void);

/**
 * @brief Vérifie que tous les périphériques sont correctement initialisés
 * @return true si tout est OK, false si une erreur est détectée
 */
bool MX_CheckInit(void);

/**
 * @brief Affiche le résumé de l'initialisation sur la console debug
 */
void MX_PrintInitSummary(void);

/**
 * @brief Réinitialise un périphérique spécifique
 * @param peripheral Nom du périphérique ("SPI2", "ADC1", etc.)
 * @return true si réinitialisé avec succès
 */
bool MX_ResetPeripheral(const char* peripheral);

// ============================================================
// SECTION 5 : HANDLES GLOBAUX DES PÉRIPHÉRIQUES
// ============================================================

/**
 * @name Handles des périphériques
 * 
 * Ces handles sont déclarés "extern" car ils sont définis
 * dans system_init.c et utilisés par tous les autres modules.
 * @{
 */

/** @brief Handle UART debug (USART1) */
extern UART_HandleTypeDef huart1;

/** @brief Handle SPI LoRa (SPI2) */
extern SPI_HandleTypeDef hspi2;

/** @brief Handle I2C tactile (I2C1) */
extern I2C_HandleTypeDef hi2c1;

/** @brief Handle ADC microphone (ADC1) */
extern ADC_HandleTypeDef hadc1;

/** @brief Handle DAC haut-parleur */
extern DAC_HandleTypeDef hdac;

/** @brief Handle Timer audio (TIM6) */
extern TIM_HandleTypeDef htim6;

/** @brief Handle Timer backlight (TIM4) */
extern TIM_HandleTypeDef htim4;

/** @brief Handle Timer PWM (TIM9) */
extern TIM_HandleTypeDef htim9;

#if PLATFORM_HAS_LTDC
    /** @brief Handle contrôleur écran */
    extern LTDC_HandleTypeDef hltdc;
    
    /** @brief Handle accélérateur graphique */
    extern DMA2D_HandleTypeDef hdma2d;
#endif

#if PLATFORM_HAS_SDRAM
    /** @brief Handle SDRAM */
    extern SDRAM_HandleTypeDef hsdram1;
#endif

/**
 * @name Handles DMA
 * @{
 */
extern DMA_HandleTypeDef hdma_spi2_tx;      // DMA SPI2 TX (LoRa)
extern DMA_HandleTypeDef hdma_spi2_rx;      // DMA SPI2 RX (LoRa)
extern DMA_HandleTypeDef hdma_adc1;         // DMA ADC1 (Microphone)
extern DMA_HandleTypeDef hdma_dac1;         // DMA DAC1 (Haut-parleur)
extern DMA_HandleTypeDef hdma_usart1_tx;    // DMA USART1 TX (Debug)
extern DMA_HandleTypeDef hdma_usart1_rx;    // DMA USART1 RX (Debug)
/** @} */

/** @} */ // Fin du groupe handles

// ============================================================
// SECTION 6 : CONSTANTES D'INITIALISATION
// ============================================================

/**
 * @def INIT_TIMEOUT_MS
 * @brief Timeout pour les opérations d'initialisation (ms)
 */
#define INIT_TIMEOUT_MS             1000

/**
 * @def INIT_RETRY_COUNT
 * @brief Nombre de tentatives en cas d'échec d'initialisation
 */
#define INIT_RETRY_COUNT            3

/**
 * @enum InitStatus
 * @brief État d'initialisation d'un périphérique
 */
typedef enum {
    INIT_NOT_STARTED    = 0,    // Pas encore initialisé
    INIT_IN_PROGRESS    = 1,    // Initialisation en cours
    INIT_SUCCESS        = 2,    // Initialisation réussie
    INIT_FAILED         = 3,    // Échec de l'initialisation
    INIT_TIMEOUT        = 4     // Timeout pendant l'initialisation
} InitStatus;

/**
 * @struct InitReport
 * @brief Rapport d'initialisation d'un périphérique
 */
typedef struct {
    const char* name;           // Nom du périphérique
    InitStatus status;          // État
    uint32_t duration_ms;       // Durée de l'initialisation
    uint32_t error_code;        // Code d'erreur si échec
} InitReport;

/**
 * @def MAX_INIT_REPORTS
 * @brief Nombre maximum de rapports d'initialisation
 */
#define MAX_INIT_REPORTS            20

/**
 * @brief Tableau des rapports d'initialisation
 */
extern InitReport init_reports[MAX_INIT_REPORTS];
extern uint8_t init_report_count;

// ============================================================
// SECTION 7 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_INIT_H