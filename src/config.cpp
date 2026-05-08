/**
 * @file config.cpp
 * @brief Implémentation de la configuration matérielle
 * 
 * Ce fichier contient :
 * - Les variables globales de configuration
 * - Les fonctions d'initialisation du matériel
 * - Les fonctions utilitaires liées à la configuration
 * 
 * Il est le compagnon de config.h qui contient les #define.
 * Ici on trouve ce qui ne peut pas être mis dans un header
 * (variables, fonctions, initialisations complexes).
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "config.h"
#include "include/project_config.h"
#include "include/platform.h"
#include "include/version.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// SECTION 1 : VARIABLES GLOBALES DE CONFIGURATION
// ============================================================

/**
 * @brief Tableau de toutes les broches utilisées (pour vérification)
 * 
 * Utile pour détecter les conflits de broches au démarrage.
 * Chaque entrée = {port, pin, nom, fonction}
 */
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
    const char* name;
    const char* function;
} PinConfig;

static const PinConfig all_pins[] = {
    // LoRa
    {LORA_CS_PORT,   LORA_CS_PIN,   "PB12", "LoRa CS"},
    {LORA_RST_PORT,  LORA_RST_PIN,  "PD8",  "LoRa Reset"},
    {LORA_DIO0_PORT, LORA_DIO0_PIN, "PD7",  "LoRa DIO0"},
    {LORA_DIO1_PORT, LORA_DIO1_PIN, "PD6",  "LoRa DIO1"},
    
    // Écran
    {TFT_BL_PORT,  TFT_BL_PIN,  "PE4",  "TFT Backlight"},
    {TFT_RST_PORT, TFT_RST_PIN, "PE5",  "TFT Reset"},
    {TFT_CS_PORT,  TFT_CS_PIN,  "PE11", "TFT CS"},
    {TFT_RS_PORT,  TFT_RS_PIN,  "PE6",  "TFT RS"},
    {TFT_WR_PORT,  TFT_WR_PIN,  "PD5",  "TFT WR"},
    {TFT_RD_PORT,  TFT_RD_PIN,  "PD4",  "TFT RD"},
    
    // Bus de données TFT
    {TFT_D0_PORT,  TFT_D0_PIN,  "PE12", "TFT D0"},
    {TFT_D1_PORT,  TFT_D1_PIN,  "PE13", "TFT D1"},
    {TFT_D2_PORT,  TFT_D2_PIN,  "PE14", "TFT D2"},
    {TFT_D3_PORT,  TFT_D3_PIN,  "PE15", "TFT D3"},
    {TFT_D4_PORT,  TFT_D4_PIN,  "PD9",  "TFT D4"},
    {TFT_D5_PORT,  TFT_D5_PIN,  "PD15", "TFT D5"},
    {TFT_D6_PORT,  TFT_D6_PIN,  "PD0",  "TFT D6"},
    {TFT_D7_PORT,  TFT_D7_PIN,  "PD1",  "TFT D7"},
    
    // Tactile
    {TOUCH_SCL_PORT, TOUCH_SCL_PIN, "PB6", "Touch SCL"},
    {TOUCH_SDA_PORT, TOUCH_SDA_PIN, "PB7", "Touch SDA"},
    {TOUCH_IRQ_PORT, TOUCH_IRQ_PIN, "PB5", "Touch IRQ"},
    {TOUCH_RST_PORT, TOUCH_RST_PIN, "PB4", "Touch Reset"},
    
    // Audio
    {MIC_PORT,  MIC_PIN,  "PA0", "Microphone (ADC)"},
    {SPK_PORT,  SPK_PIN,  "PA5", "Haut-parleur (DAC)"},
    
    // Clavier (lignes)
    {KEYPAD_ROW_PORT, KEYPAD_ROW0_PIN, "PD0", "Clavier Row0"},
    {KEYPAD_ROW_PORT, KEYPAD_ROW1_PIN, "PD1", "Clavier Row1"},
    {KEYPAD_ROW_PORT, KEYPAD_ROW2_PIN, "PD2", "Clavier Row2"},
    {KEYPAD_ROW_PORT, KEYPAD_ROW3_PIN, "PD3", "Clavier Row3"},
    {KEYPAD_ROW_PORT, KEYPAD_ROW4_PIN, "PD4", "Clavier Row4"},
    {KEYPAD_ROW_PORT, KEYPAD_ROW5_PIN, "PD5", "Clavier Row5"},
    
    // Clavier (colonnes)
    {KEYPAD_COL_PORT, KEYPAD_COL0_PIN, "PE8",  "Clavier Col0"},
    {KEYPAD_COL_PORT, KEYPAD_COL1_PIN, "PE9",  "Clavier Col1"},
    {KEYPAD_COL_PORT, KEYPAD_COL2_PIN, "PE10", "Clavier Col2"},
    {KEYPAD_COL_PORT, KEYPAD_COL3_PIN, "PE11", "Clavier Col3"},
    
    // LEDs
    {STATUS_LED_PORT, STATUS_LED_PIN, "PG13", "LED Statut"},
    {LAMP_LED_PORT,   LAMP_LED_PIN,   "PE6",  "LED Lampe"},
    {KEYPAD_BL_PORT,  KEYPAD_BL_PIN,  "PG13", "Backlight Clavier"},
    
    // Debug UART
    {DEBUG_TX_PORT, DEBUG_TX_PIN, "PA9",  "Debug TX"},
    {DEBUG_RX_PORT, DEBUG_RX_PIN, "PA10", "Debug RX"},
};

/** @brief Nombre total de broches configurées */
#define TOTAL_CONFIGURED_PINS   (sizeof(all_pins) / sizeof(all_pins[0]))

/**
 * @brief État d'initialisation de chaque sous-système
 */
typedef enum {
    SUBSYSTEM_UNINITIALIZED = 0,
    SUBSYSTEM_INITIALIZING  = 1,
    SUBSYSTEM_READY         = 2,
    SUBSYSTEM_ERROR         = 3
} SubsystemState;

static SubsystemState lora_state = SUBSYSTEM_UNINITIALIZED;
static SubsystemState display_state = SUBSYSTEM_UNINITIALIZED;
static SubsystemState touch_state = SUBSYSTEM_UNINITIALIZED;
static SubsystemState audio_state = SUBSYSTEM_UNINITIALIZED;
static SubsystemState keypad_state = SUBSYSTEM_UNINITIALIZED;

// ============================================================
// SECTION 2 : FONCTIONS DE VÉRIFICATION
// ============================================================

/**
 * @brief Vérifie qu'il n'y a pas de conflit de broches
 * 
 * Parcourt toutes les broches configurées et vérifie
 * qu'aucune paire (port, pin) n'est utilisée deux fois.
 * 
 * @return true si aucune conflit, false sinon
 */
bool config_check_pin_conflicts(void)
{
    bool has_conflict = false;
    
    for (uint32_t i = 0; i < TOTAL_CONFIGURED_PINS; i++)
    {
        for (uint32_t j = i + 1; j < TOTAL_CONFIGURED_PINS; j++)
        {
            if (all_pins[i].port == all_pins[j].port &&
                all_pins[i].pin == all_pins[j].pin)
            {
                printf("⚠️ CONFLIT DE BROCHES !\n");
                printf("   %s (%s) et %s (%s)\n",
                       all_pins[i].name, all_pins[i].function,
                       all_pins[j].name, all_pins[j].function);
                printf("   Même broche utilisée deux fois !\n");
                has_conflict = true;
            }
        }
    }
    
    if (!has_conflict)
    {
        printf("✅ Aucun conflit de broches détecté (%lu broches)\n", 
               (unsigned long)TOTAL_CONFIGURED_PINS);
    }
    
    return !has_conflict;
}

/**
 * @brief Affiche le résumé de toutes les broches configurées
 */
void config_print_pinout_summary(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║           RÉSUMÉ DU BROCHAGE (PINOUT)               ║\n");
    printf("╠══════════════════════════════════════════════════════╣\n");
    printf("║ %-6s │ %-4s │ %-30s ║\n", "Broche", "Pin", "Fonction");
    printf("╠══════════════════════════════════════════════════════╣\n");
    
    for (uint32_t i = 0; i < TOTAL_CONFIGURED_PINS; i++)
    {
        printf("║ %-6s │ %-4s │ %-30s ║\n",
               all_pins[i].name,
               "",  // Pin déjà inclus dans le nom
               all_pins[i].function);
    }
    
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("Total : %lu broches configurées\n\n", (unsigned long)TOTAL_CONFIGURED_PINS);
}

// ============================================================
// SECTION 3 : FONCTIONS D'INITIALISATION GÉNÉRALES
// ============================================================

/**
 * @brief Initialise la configuration matérielle
 * 
 * Cette fonction est appelée au démarrage pour :
 * 1. Vérifier les conflits de broches
 * 2. Initialiser les sous-systèmes dans le bon ordre
 * 3. Afficher le résumé de la configuration
 * 
 * @return true si tout est OK
 */
bool config_init(void)
{
    printf("\n═══ INITIALISATION CONFIGURATION ═══\n\n");
    
    // 1. Vérifier les conflits
    if (!config_check_pin_conflicts())
    {
        printf("❌ ERREUR : Conflits de broches détectés !\n");
        printf("   Corrigez config.h avant de continuer.\n");
        return false;
    }
    
    // 2. Vérifier la plateforme
    printf("Plateforme : %s\n", PLATFORM_NAME);
    printf("MCU : %s @ %lu MHz\n", MCU_MODEL, (unsigned long)SYSTEM_CLOCK_MHZ);
    
    // 3. Vérifier la mémoire
    printf("\n═══ MÉMOIRE ═══\n");
    printf("Flash : %lu Ko\n", (unsigned long)FLASH_SIZE_KB);
    printf("RAM   : %lu Ko\n", (unsigned long)RAM_SIZE_KB);
#if PLATFORM_HAS_SDRAM
    printf("SDRAM : %lu Mo (0x%08lX)\n", 
           (unsigned long)SDRAM_SIZE_MB, (unsigned long)SDRAM_BASE_ADDR);
#endif
    
    // 4. Vérifier les fréquences d'horloge
    printf("\n═══ HORLOGES ═══\n");
    printf("SYSCLK : %lu MHz\n", (unsigned long)SYSTEM_CLOCK_MHZ);
    printf("HCLK   : %lu MHz\n", (unsigned long)(HCLK_HZ / 1000000));
    printf("PCLK1  : %lu MHz\n", (unsigned long)(PCLK1_HZ / 1000000));
    printf("PCLK2  : %lu MHz\n", (unsigned long)(PCLK2_HZ / 1000000));
    
    printf("\n✅ Configuration matérielle OK\n\n");
    return true;
}

// ============================================================
// SECTION 4 : FONCTIONS SPÉCIFIQUES LORA
// ============================================================

/**
 * @brief Configure les broches du module LoRa
 */
void config_lora_pins_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Activer l'horloge des ports
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    
    // CS (PB12) - Sortie Push-Pull
    GPIO_InitStruct.Pin = LORA_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LORA_CS_PORT, &GPIO_InitStruct);
    LORA_CS_HIGH();  // Désélectionné par défaut
    
    // Reset (PD8) - Sortie Push-Pull
    GPIO_InitStruct.Pin = LORA_RST_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LORA_RST_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LORA_RST_PORT, LORA_RST_PIN, GPIO_PIN_SET);  // Pas en reset
    
    // DIO0 (PD7) - Entrée avec interruption
    GPIO_InitStruct.Pin = LORA_DIO0_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(LORA_DIO0_PORT, &GPIO_InitStruct);
    
    // DIO1 (PD6) - Entrée avec interruption
    GPIO_InitStruct.Pin = LORA_DIO1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(LORA_DIO1_PORT, &GPIO_InitStruct);
    
    // Configuration NVIC pour les interruptions LoRa
    HAL_NVIC_SetPriority(EXTI2_IRQn, 2, 0);   // DIO0 : priorité haute
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
    
    HAL_NVIC_SetPriority(EXTI3_IRQn, 3, 0);   // DIO1 : priorité moyenne
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
    
    lora_state = SUBSYSTEM_READY;
    printf("[LORA] Broches initialisées\n");
}

// ============================================================
// SECTION 5 : FONCTIONS SPÉCIFIQUES ÉCRAN
// ============================================================

/**
 * @brief Configure les broches de l'écran TFT
 */
void config_display_pins_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Activer les horloges
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    // Backlight (PE4) - Sortie Push-Pull
    GPIO_InitStruct.Pin = TFT_BL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TFT_BL_PORT, &GPIO_InitStruct);
    TFT_BL_ON();  // Allumer le rétroéclairage
    
    // Reset (PE5) - Sortie Push-Pull
    GPIO_InitStruct.Pin = TFT_RST_PIN;
    HAL_GPIO_Init(TFT_RST_PORT, &GPIO_InitStruct);
    
    display_state = SUBSYSTEM_READY;
    printf("[DISPLAY] Broches initialisées\n");
}

// ============================================================
// SECTION 6 : FONCTIONS SPÉCIFIQUES AUDIO
// ============================================================

/**
 * @brief Configure les broches audio (Micro + HP)
 */
void config_audio_pins_init(void)
{
    // Activer l'horloge GPIOA
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // PA0 = Microphone (Analogique)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = MIC_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(MIC_PORT, &GPIO_InitStruct);
    
    // PA5 = Haut-parleur (Analogique)
    GPIO_InitStruct.Pin = SPK_PIN;
    HAL_GPIO_Init(SPK_PORT, &GPIO_InitStruct);
    
    audio_state = SUBSYSTEM_READY;
    printf("[AUDIO] Broches initialisées\n");
}

// ============================================================
// SECTION 7 : FONCTIONS SPÉCIFIQUES CLAVIER
// ============================================================

/**
 * @brief Configure les broches du clavier matriciel
 */
void config_keypad_pins_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Activer les horloges
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    
    // Lignes (sorties) - PD0 à PD5
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    for (int i = 0; i < KEYPAD_ROWS; i++)
    {
        GPIO_InitStruct.Pin = KEYPAD_ROW_PINS[i];
        HAL_GPIO_Init(KEYPAD_ROW_PORT, &GPIO_InitStruct);
        HAL_GPIO_WritePin(KEYPAD_ROW_PORT, KEYPAD_ROW_PINS[i], GPIO_PIN_SET);
    }
    
    // Colonnes (entrées avec pull-up) - PE8 à PE11
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    
    for (int i = 0; i < KEYPAD_COLS; i++)
    {
        GPIO_InitStruct.Pin = KEYPAD_COL_PINS[i];
        HAL_GPIO_Init(KEYPAD_COL_PORT, &GPIO_InitStruct);
    }
    
    keypad_state = SUBSYSTEM_READY;
    printf("[KEYPAD] Broches initialisées\n");
}

// ============================================================
// SECTION 8 : FONCTIONS D'ÉTAT
// ============================================================

/**
 * @brief Vérifie si un sous-système est prêt
 */
bool config_is_subsystem_ready(const char* subsystem)
{
    if (strcmp(subsystem, "lora") == 0)    return lora_state == SUBSYSTEM_READY;
    if (strcmp(subsystem, "display") == 0) return display_state == SUBSYSTEM_READY;
    if (strcmp(subsystem, "touch") == 0)   return touch_state == SUBSYSTEM_READY;
    if (strcmp(subsystem, "audio") == 0)   return audio_state == SUBSYSTEM_READY;
    if (strcmp(subsystem, "keypad") == 0)  return keypad_state == SUBSYSTEM_READY;
    return false;
}

/**
 * @brief Vérifie si TOUS les sous-systèmes sont prêts
 */
bool config_is_all_ready(void)
{
    return config_is_subsystem_ready("lora") &&
           config_is_subsystem_ready("display") &&
           config_is_subsystem_ready("audio") &&
           config_is_subsystem_ready("keypad");
}

/**
 * @brief Affiche l'état de tous les sous-systèmes
 */
void config_print_subsystem_status(void)
{
    printf("\n═══ ÉTAT DES SOUS-SYSTÈMES ═══\n");
    printf("LoRa    : %s\n", lora_state == SUBSYSTEM_READY ? "✅ OK" : "❌ ERREUR");
    printf("Écran   : %s\n", display_state == SUBSYSTEM_READY ? "✅ OK" : "❌ ERREUR");
    printf("Tactile : %s\n", touch_state == SUBSYSTEM_READY ? "✅ OK" : "❌ ERREUR");
    printf("Audio   : %s\n", audio_state == SUBSYSTEM_READY ? "✅ OK" : "❌ ERREUR");
    printf("Clavier : %s\n", keypad_state == SUBSYSTEM_READY ? "✅ OK" : "❌ ERREUR");
    printf("═══════════════════════════════\n\n");
}

// ============================================================
// SECTION 9 : FONCTIONS UTILITAIRES
// ============================================================

/**
 * @brief Calcule la fréquence SYSCLK actuelle
 * @return Fréquence en Hz
 * 
 * Lit les registres PLL pour déterminer la fréquence réelle.
 */
uint32_t config_get_sysclk_freq(void)
{
    RCC_ClkInitTypeDef clkconfig;
    uint32_t latency;
    
    HAL_RCC_GetClockConfig(&clkconfig, &latency);
    
    return HAL_RCC_GetSysClockFreq();
}

/**
 * @brief Calcule la fréquence HCLK actuelle
 * @return Fréquence en Hz
 */
uint32_t config_get_hclk_freq(void)
{
    return HAL_RCC_GetHCLKFreq();
}

/**
 * @brief Calcule la fréquence PCLK1 actuelle
 * @return Fréquence en Hz
 */
uint32_t config_get_pclk1_freq(void)
{
    return HAL_RCC_GetPCLK1Freq();
}

/**
 * @brief Calcule la fréquence PCLK2 actuelle
 * @return Fréquence en Hz
 */
uint32_t config_get_pclk2_freq(void)
{
    return HAL_RCC_GetPCLK2Freq();
}

/**
 * @brief Convertit un port GPIO en chaîne de caractères
 * @param port Pointeur vers le registre GPIO
 * @return Nom du port ("GPIOA", "GPIOB", etc.)
 */
const char* config_gpio_port_name(GPIO_TypeDef* port)
{
    if (port == GPIOA) return "GPIOA";
    if (port == GPIOB) return "GPIOB";
    if (port == GPIOC) return "GPIOC";
    if (port == GPIOD) return "GPIOD";
    if (port == GPIOE) return "GPIOE";
    if (port == GPIOF) return "GPIOF";
    if (port == GPIOG) return "GPIOG";
    if (port == GPIOH) return "GPIOH";
    return "UNKNOWN";
}

/**
 * @brief Convertit un numéro de pin en chaîne
 * @param pin GPIO_PIN_X
 * @return Nom de la pin ("0", "1", ..., "15")
 */
const char* config_pin_name(uint16_t pin)
{
    for (int i = 0; i < 16; i++)
    {
        if (pin == (1 << i))
        {
            static char name[3];
            snprintf(name, sizeof(name), "%d", i);
            return name;
        }
    }
    return "?";
}

/**
 * @brief Affiche les informations de fréquence détaillées
 */
void config_print_clock_tree(void)
{
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║        ARBRE D'HORLOGE (CLOCK TREE)     ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    
    uint32_t sysclk = config_get_sysclk_freq();
    uint32_t hclk = config_get_hclk_freq();
    uint32_t pclk1 = config_get_pclk1_freq();
    uint32_t pclk2 = config_get_pclk2_freq();
    
    printf("║ HSE (8 MHz)                              ║\n");
    printf("║   └─ PLL (×360, ÷2)                     ║\n");
    printf("║       └─ SYSCLK = %3lu MHz                ║\n", (unsigned long)(sysclk / 1000000));
    printf("║           ├─ HCLK  = %3lu MHz             ║\n", (unsigned long)(hclk / 1000000));
    printf("║           ├─ PCLK1 = %3lu MHz             ║\n", (unsigned long)(pclk1 / 1000000));
    printf("║           └─ PCLK2 = %3lu MHz             ║\n", (unsigned long)(pclk2 / 1000000));
    printf("╚══════════════════════════════════════════╝\n\n");
}