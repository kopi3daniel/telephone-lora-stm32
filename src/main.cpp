/**
 * @file main.cpp
 * @brief Point d'entrée principal du Téléphone LoRa STM32F429
 *
 * Ce fichier contient la fonction main() et la boucle principale.
 * Il orchestre l'initialisation de tous les sous-systèmes et
 * gère le cycle de vie de l'application.
 *
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// HAL STM32
#include "stm32f4xx_hal.h"

// Configuration
#include "config.h"
#include "include/project_config.h"
#include "include/version.h"
#include "include/platform.h"

// Initialisation système
#include "system_init.h"
#include "stm32f4xx_it.h"

// Drivers (à implémenter dans les prochaines étapes)
// #include "drivers/lora/lora_driver.h"
// #include "drivers/display/display_manager.h"
// #include "drivers/keypad/keypad_manager.h"
// #include "drivers/audio/audio_manager.h"

// Application (à implémenter dans les prochaines étapes)
// #include "app/phone_app.h"

// ============================================================
// HANDLES GLOBAUX DES PÉRIPHÉRIQUES
// ============================================================

// Ces handles sont utilisés par tout le projet.
// Ils sont déclarés "extern" dans system_init.h

UART_HandleTypeDef  huart1;      // Debug série
SPI_HandleTypeDef   hspi2;       // LoRa
I2C_HandleTypeDef   hi2c1;       // Tactile
ADC_HandleTypeDef   hadc1;       // Microphone
DAC_HandleTypeDef   hdac;        // Haut-parleur
TIM_HandleTypeDef   htim6;       // Timer audio 8 kHz
TIM_HandleTypeDef   htim4;       // PWM backlight clavier
TIM_HandleTypeDef   htim9;       // PWM lampe + écran

#if PLATFORM_HAS_LTDC
LTDC_HandleTypeDef  hltdc;       // Contrôleur écran TFT
DMA2D_HandleTypeDef hdma2d;      // Accélérateur graphique
#endif

#if PLATFORM_HAS_SDRAM
SDRAM_HandleTypeDef hsdram1;     // SDRAM externe
#endif

// Handles DMA
DMA_HandleTypeDef hdma_spi2_tx;    // DMA SPI2 TX (LoRa)
DMA_HandleTypeDef hdma_spi2_rx;    // DMA SPI2 RX (LoRa)
DMA_HandleTypeDef hdma_adc1;       // DMA ADC1 (Microphone)
DMA_HandleTypeDef hdma_dac1;       // DMA DAC1 (Haut-parleur)
DMA_HandleTypeDef hdma_usart1_tx;  // DMA USART1 TX (Debug)
DMA_HandleTypeDef hdma_usart1_rx;  // DMA USART1 RX (Debug)

// ============================================================
// VARIABLES GLOBALES DE L'APPLICATION
// ============================================================

/**
 * @brief Compteur de ticks pour le heartbeat
 */
static uint32_t system_tick_count = 0;

/**
 * @brief Timestamp du démarrage
 */
static uint32_t boot_timestamp = 0;

/**
 * @brief État global du système
 */
typedef enum {
    SYSTEM_STATE_BOOTING,       // Démarrage en cours
    SYSTEM_STATE_INITIALIZING,  // Initialisation
    SYSTEM_STATE_READY,         // Prêt
    SYSTEM_STATE_RUNNING,       // En fonctionnement
    SYSTEM_STATE_ERROR,         // Erreur
    SYSTEM_STATE_SLEEP          // Veille
} SystemState;

static SystemState system_state = SYSTEM_STATE_BOOTING;

// ============================================================
// PROTOTYPES DES FONCTIONS LOCALES
// ============================================================

static void SystemClock_Config(void);
static void System_Init(void);
static void System_PrintBanner(void);
static void System_Heartbeat(void);
static void System_ErrorHandler(void);

// ============================================================
// SECTION 1 : FONCTION PRINCIPALE
// ============================================================

/**
 * @brief Point d'entrée du programme
 *
 * Flux d'exécution :
 * 1. Initialisation du HAL
 * 2. Configuration de l'horloge système (180 MHz)
 * 3. Initialisation des périphériques (GPIO, SPI, I2C, etc.)
 * 4. Initialisation des drivers (LoRa, Écran, Audio, Clavier)
 * 5. Initialisation de l'application (PhoneApp)
 * 6. Boucle principale
 *
 * @return int Code de retour (jamais atteint normalement)
 */
int main(void)
{
    // =====================================================
    // ÉTAPE 1 : INITIALISATION DU HAL
    // =====================================================
    HAL_Init();
    system_state = SYSTEM_STATE_BOOTING;

    // =====================================================
    // ÉTAPE 2 : CONFIGURATION DE L'HORLOGE
    // =====================================================
    SystemClock_Config();
    boot_timestamp = HAL_GetTick();

    // =====================================================
    // ÉTAPE 3 : INITIALISATION DES PÉRIPHÉRIQUES
    // =====================================================
    system_state = SYSTEM_STATE_INITIALIZING;

    MX_GPIO_Init();          // GPIO en premier
    MX_DMA_Init();           // DMA ensuite
    MX_NVIC_Init();          // Interruptions
    MX_USART1_UART_Init();   // Debug série (pour les printf)

    // Afficher la bannière de démarrage
    System_PrintBanner();

    printf("[INIT] Démarrage du système...\n");
    printf("[INIT] Horloge : %lu MHz\n", (unsigned long)(HAL_RCC_GetSysClockFreq() / 1000000));

    // Initialiser la SDRAM si disponible (avant l'écran !)
#if PLATFORM_HAS_SDRAM
    printf("[INIT] SDRAM...");
    MX_FMC_SDRAM_Init();
    printf(" OK (8 Mo @ 0x%08lX)\n", (unsigned long)SDRAM_BASE_ADDR);
#endif

    // Initialiser l'écran si disponible
#if PLATFORM_HAS_LTDC
    printf("[INIT] LTDC + DMA2D...");
    MX_LTDC_Init();
    MX_DMA2D_Init();
    printf(" OK\n");
#endif

    // Initialiser les autres périphériques
    printf("[INIT] SPI2 (LoRa)...");
    MX_SPI2_Init();
    printf(" OK\n");

    printf("[INIT] I2C1 (Tactile)...");
    MX_I2C1_Init();
    printf(" OK\n");

    printf("[INIT] ADC1 (Micro)...");
    MX_ADC1_Init();
    printf(" OK\n");

    printf("[INIT] DAC (Haut-parleur)...");
    MX_DAC_Init();
    printf(" OK\n");

    printf("[INIT] TIM6 (Audio 8 kHz)...");
    MX_TIM6_Init();
    printf(" OK\n");

    printf("[INIT] TIM4 (Backlight)...");
    MX_TIM4_Init();
    printf(" OK\n");

    printf("[INIT] TIM9 (PWM)...");
    MX_TIM9_Init();
    printf(" OK\n");

    // =====================================================
    // ÉTAPE 4 : INITIALISATION DE LA CONFIGURATION
    // =====================================================
    printf("\n[INIT] Vérification de la configuration matérielle...\n");

    if (!config_init())
    {
        printf("[ERREUR] Configuration matérielle invalide !\n");
        System_ErrorHandler();
    }

    // Initialiser les broches spécifiques
    config_lora_pins_init();
    config_display_pins_init();
    config_audio_pins_init();
    config_keypad_pins_init();

    // Vérifier l'état des sous-systèmes
    config_print_subsystem_status();

    if (!config_is_all_ready())
    {
        printf("[ERREUR] Certains sous-systèmes ne sont pas prêts !\n");
        System_ErrorHandler();
    }

    // Afficher le résumé du brochage
    config_print_pinout_summary();
    config_print_clock_tree();

    // =====================================================
    // ÉTAPE 5 : INITIALISATION DES DRIVERS
    // (À décommenter quand les drivers seront créés)
    // =====================================================
    printf("[INIT] Initialisation des drivers...\n");

    // LoRa
    // if (!lora_init())
    // {
    //     printf("[ERREUR] Module LoRa non détecté !\n");
    //     System_ErrorHandler();
    // }

    // Écran
    // display_init();
    // display_clear(COLOR_BLACK);
    // display_draw_splash_screen();

    // Audio
    // audio_init();

    // Clavier
    // keypad_init();

    printf("[INIT] Drivers... OK (simulé)\n");

    // =====================================================
    // ÉTAPE 6 : INITIALISATION DE L'APPLICATION
    // (À décommenter quand l'application sera créée)
    // =====================================================
    printf("[INIT] Démarrage de l'application...\n");

    // phone_app_init();
    // phone_app_show_home_screen();

    printf("[INIT] Application... OK (simulé)\n");

    // =====================================================
    // ÉTAPE 7 : SYSTÈME PRÊT
    // =====================================================
    system_state = SYSTEM_STATE_READY;

    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║         ✅ SYSTÈME PRÊT                  ║\n");
    printf("║         Téléphone LoRa opérationnel      ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    STATUS_LED_ON();  // Allumer la LED statut

    // =====================================================
    // ÉTAPE 8 : BOUCLE PRINCIPALE
    // =====================================================
    system_state = SYSTEM_STATE_RUNNING;

    uint32_t last_heartbeat = 0;
    uint32_t last_keypad_scan = 0;
    uint32_t last_lora_check = 0;
    uint32_t last_display_update = 0;
    uint32_t last_stats_update = 0;

    printf("[MAIN] Entrée dans la boucle principale\n\n");

    while (1)
    {
        uint32_t now = HAL_GetTick();

        // --- Heartbeat (toutes les 5 secondes) ---
        if (now - last_heartbeat >= 5000)
        {
            System_Heartbeat();
            last_heartbeat = now;
        }

        // --- Scan clavier (toutes les 10 ms) ---
        if (now - last_keypad_scan >= 10)
        {
            // keypad_scan();
            last_keypad_scan = now;
        }

        // --- Vérification LoRa (toutes les 50 ms) ---
        if (now - last_lora_check >= 50)
        {
            // lora_process();
            last_lora_check = now;
        }

        // --- Mise à jour affichage (toutes les 100 ms) ---
        if (now - last_display_update >= 100)
        {
            // display_update();
            last_display_update = now;
        }

        // --- Traitement des flags d'interruption ---
        process_interrupt_flags();

        // --- Mise à jour des statistiques (toutes les 60 secondes) ---
        if (now - last_stats_update >= 60000)
        {
            // system_print_stats();
            last_stats_update = now;
        }

        // --- Gestion de l'énergie (mode veille) ---
        // power_manage();

        // Petite pause pour éviter de saturer le CPU
        HAL_Delay(1);
    }

    // Normalement, on ne sort jamais de la boucle
    return 0;
}

// ============================================================
// SECTION 2 : INITIALISATION DE L'HORLOGE SYSTÈME
// ============================================================

/**
 * @brief Configure l'horloge système à 180 MHz
 *
 * HSE (8 MHz) → PLL (×360, ÷2) → SYSCLK = 180 MHz
 *
 * Bus clocks :
 * - HCLK  = 180 MHz (AHB)
 * - PCLK1 = 45 MHz  (APB1, /4)
 * - PCLK2 = 90 MHz  (APB2, /2)
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // Activer l'horloge du régulateur de tension
    __HAL_RCC_PWR_CLK_ENABLE();

    // Configurer le régulateur pour 180 MHz (Scale 1)
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    // --- Configuration de l'oscillateur ---
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;                    // HSE / 8 = 1 MHz
    RCC_OscInitStruct.PLL.PLLN = 360;                  // 1 MHz × 360 = 360 MHz VCO
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;        // 360 / 2 = 180 MHz SYSCLK
    RCC_OscInitStruct.PLL.PLLQ = 7;                    // 360 / 7 ≈ 51.4 MHz (USB)

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        System_ErrorHandler();
    }

    // Activer l'OverDrive (obligatoire pour 180 MHz)
    HAL_PWREx_EnableOverDrive();

    // --- Configuration des horloges bus ---
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                   RCC_CLOCKTYPE_SYSCLK |
                                   RCC_CLOCKTYPE_PCLK1 |
                                   RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;     // HCLK = SYSCLK = 180 MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;      // PCLK1 = 45 MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;      // PCLK2 = 90 MHz

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        System_ErrorHandler();
    }
}

// ============================================================
// SECTION 3 : FONCTIONS SYSTÈME
// ============================================================

/**
 * @brief Affiche la bannière de démarrage
 */
static void System_PrintBanner(void)
{
    printf("\n\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║                                                  ║\n");
    printf("║     📱 TÉLÉPHONE LORA STM32F429                 ║\n");
    printf("║                                                  ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  Version  : %-36s ║\n", VERSION_STRING);
    printf("║  Build    : %-36s ║\n", BUILD_TIMESTAMP);
    printf("║  Plateforme: %-34s ║\n", PLATFORM_NAME);
    printf("║  MCU      : %-36s ║\n", MCU_MODEL);
    printf("║  Fréquence: %-3lu MHz                           ║\n", (unsigned long)SYSTEM_CLOCK_MHZ);
    printf("║  Auteur   : %-36s ║\n", PROJECT_AUTHOR);
    printf("║  Licence  : %-36s ║\n", PROJECT_LICENSE);
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\n");
}

/**
 * @brief Fonction heartbeat (appelée périodiquement)
 *
 * Affiche un message de vie et fait clignoter la LED.
 */
static void System_Heartbeat(void)
{
    system_tick_count++;

    // Faire clignoter la LED statut
    STATUS_LED_TOGGLE();

    // Afficher l'uptime
    uint32_t uptime = HAL_GetTick() / 1000;
    uint32_t hours = uptime / 3600;
    uint32_t minutes = (uptime % 3600) / 60;
    uint32_t seconds = uptime % 60;

    printf("[HEARTBEAT] Uptime: %luh %lum %lus | Ticks: %lu\n",
           (unsigned long)hours,
           (unsigned long)minutes,
           (unsigned long)seconds,
           (unsigned long)system_tick_count);
}

/**
 * @brief Gestionnaire d'erreur système
 *
 * Appelé en cas d'erreur fatale.
 * Bloque le système et fait clignoter la LED en SOS.
 */
static void System_ErrorHandler(void)
{
    system_state = SYSTEM_STATE_ERROR;

    // Désactiver toutes les interruptions
    __disable_irq();

    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║     ❌ ERREUR SYSTÈME FATALE             ║\n");
    printf("║     Le système va redémarrer...          ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    // Attendre un peu que le message soit transmis
    HAL_Delay(1000);

    // Boucle infinie avec clignotement SOS
    while (1)
    {
        // Pattern SOS : ... --- ...
        // 3 courts
        for (int i = 0; i < 3; i++)
        {
            STATUS_LED_ON();
            HAL_Delay(150);
            STATUS_LED_OFF();
            HAL_Delay(150);
        }
        HAL_Delay(300);

        // 3 longs
        for (int i = 0; i < 3; i++)
        {
            STATUS_LED_ON();
            HAL_Delay(600);
            STATUS_LED_OFF();
            HAL_Delay(150);
        }
        HAL_Delay(300);

        // 3 courts
        for (int i = 0; i < 3; i++)
        {
            STATUS_LED_ON();
            HAL_Delay(150);
            STATUS_LED_OFF();
            HAL_Delay(150);
        }

        // Pause longue avant de répéter
        HAL_Delay(2000);
    }
}

// ============================================================
// SECTION 4 : ASSERTION DE DÉBOGAGE
// ============================================================

#ifdef DEBUG

/**
 * @brief Fonction appelée quand une assertion échoue
 *
 * @param file Fichier source
 * @param line Ligne de l'assertion
 */
void assert_failed(const char* file, uint32_t line)
{
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║     ❌ ASSERTION ÉCHOUÉE                 ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Fichier : %-30s ║\n", file);
    printf("║  Ligne   : %-30lu ║\n", (unsigned long)line);
    printf("╚══════════════════════════════════════════╝\n\n");

    System_ErrorHandler();
}

#endif // DEBUG

// ============================================================
// SECTION 5 : REDIRECTION PRINTF ( Debug Série)
// ============================================================

/**
 * @brief Redirection du printf vers l'UART
 *
 * Permet d'utiliser printf() pour afficher sur la console série.
 *
 * @param ch Caractère à envoyer
 * @return int Caractère envoyé
 */
#ifdef __GNUC__
    #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
    #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
    // Envoyer le caractère sur l'UART de debug
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

// ============================================================
// SECTION 6 : FONCTIONS D'INTERRUPTION (FAIBLES)
// ============================================================

/**
 * @brief Handler par défaut pour les interruptions non utilisées
 *
 * Ces fonctions "faibles" (weak) peuvent être surchargées
 * dans stm32f4xx_it.c si nécessaire.
 */

__weak void NMI_Handler(void)           { while(1); }
__weak void HardFault_Handler(void)     { System_ErrorHandler(); }
__weak void MemManage_Handler(void)     { System_ErrorHandler(); }
__weak void BusFault_Handler(void)      { System_ErrorHandler(); }
__weak void UsageFault_Handler(void)    { System_ErrorHandler(); }
__weak void SVC_Handler(void)           {}
__weak void DebugMon_Handler(void)      {}
__weak void PendSV_Handler(void)        {}
__weak void SysTick_Handler(void)       { HAL_IncTick(); }

// ============================================================
// SECTION 7 : FONCTIONS DE COMPATIBILITÉ C
// ============================================================

/**
 * @brief Fonctions d'initialisation déclarées dans system_init.h
 *
 * Ces fonctions sont normalement générées par CubeMX.
 * Elles sont implémentées dans stm32f4xx_hal_msp.c
 */

extern "C" {

void MX_GPIO_Init(void);
void MX_DMA_Init(void);
void MX_NVIC_Init(void);
void MX_USART1_UART_Init(void);
void MX_SPI2_Init(void);
void MX_I2C1_Init(void);
void MX_ADC1_Init(void);
void MX_DAC_Init(void);
void MX_TIM6_Init(void);
void MX_TIM4_Init(void);
void MX_TIM9_Init(void);

#if PLATFORM_HAS_LTDC
void MX_LTDC_Init(void);
void MX_DMA2D_Init(void);
#endif

#if PLATFORM_HAS_SDRAM
void MX_FMC_SDRAM_Init(void);
#endif

} // extern "C"

// ============================================================
// FIN DU FICHIER
// ============================================================

/**
 * @mainpage Téléphone LoRa STM32F429
 *
 * @section intro Introduction
 * Projet de téléphone portable utilisant la technologie LoRa
 * pour les communications longue distance (2-15 km).
 *
 * @section hardware Matériel
 * - STM32F429ZIT6 (carte Discovery)
 * - Module LoRa RA-02 (SX1278)
 * - Écran tactile ILI9488 3.5"
 * - Clavier matriciel 24 touches
 * - Microphone MEMS + Haut-parleur 3W
 *
 * @section software Logiciel
 * - Système bare-metal (pas de RTOS)
 * - HAL STM32 pour les drivers
 * - Architecture en couches (Drivers → UI → Application)
 * - Protocole d'appel personnalisé sur LoRa
 *
 * @section author Auteur
 * Votre Nom - 2024
 */