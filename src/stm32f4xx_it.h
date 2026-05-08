/**
 * @file stm32f4xx_it.h
 * @brief Gestionnaires d'interruptions - Déclarations
 * 
 * Ce fichier contient les prototypes de TOUS les gestionnaires
 * d'interruptions du projet.
 * 
 * Les interruptions sont générées par les périphériques pour
 * signaler des événements (fin de transmission, réception de
 * données, overflow, erreurs...).
 * 
 * Chaque interruption est traitée dans stm32f4xx_it.c
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef STM32F4xx_IT_H
#define STM32F4xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// SECTION 1 : INCLUDES NÉCESSAIRES
// ============================================================

#include "stm32f4xx_hal.h"
#include "config.h"

// ============================================================
// SECTION 2 : INTERRUPTIONS SYSTÈME (Cortex-M4)
// ============================================================

/**
 * @brief Gestionnaire d'interruption Non-Maskable (NMI)
 * 
 * L'interruption NMI est déclenchée par :
 * - Erreur ECC dans la Flash
 * - Erreur de parité dans la RAM
 * - Signal externe sur le pin NMI
 * 
 * ⚠️ Cette interruption ne peut PAS être masquée.
 */
void NMI_Handler(void);

/**
 * @brief Gestionnaire d'interruption HardFault
 * 
 * Déclenchée par :
 * - Accès mémoire invalide
 * - Division par zéro
 * - Instruction non définie
 * - Violation MPU
 * 
 * En cas de HardFault, le programme est en erreur critique.
 * Le gestionnaire enregistre les informations de debug.
 */
void HardFault_Handler(void);

/**
 * @brief Gestionnaire d'interruption MemManage (MPU)
 * 
 * Déclenchée par une violation de la Memory Protection Unit :
 * - Accès à une zone mémoire non autorisée
 * - Exécution de code dans une zone non exécutable
 */
void MemManage_Handler(void);

/**
 * @brief Gestionnaire d'interruption BusFault
 * 
 * Déclenchée par une erreur sur le bus :
 * - Accès à une adresse inexistante
 * - Erreur de préfetch
 * - Accès non aligné (si activé)
 */
void BusFault_Handler(void);

/**
 * @brief Gestionnaire d'interruption UsageFault
 * 
 * Déclenchée par une erreur d'utilisation du CPU :
 * - Instruction non définie
 * - Division par zéro
 * - État invalide du coprocesseur
 * - Alignement invalide
 */
void UsageFault_Handler(void);

/**
 * @brief Gestionnaire d'interruption SVCall (Supervisor Call)
 * 
 * Déclenchée par l'instruction SVC.
 * Utilisée par FreeRTOS pour les appels système.
 */
void SVC_Handler(void);

/**
 * @brief Gestionnaire d'interruption Debug Monitor
 * 
 * Utilisée par le débogueur pour les points d'arrêt
 * et le monitoring en temps réel.
 */
void DebugMon_Handler(void);

/**
 * @brief Gestionnaire d'interruption PendSV
 * 
 * Interruption pendable. Utilisée par FreeRTOS pour
 * le changement de contexte (context switch).
 */
void PendSV_Handler(void);

/**
 * @brief Gestionnaire d'interruption SysTick
 * 
 * Timer système (1 ms par défaut).
 * Incrémente le compteur HAL_GetTick().
 * 
 * Fréquence : HCLK / 1000 = 180 000 ticks/seconde
 */
void SysTick_Handler(void);

// ============================================================
// SECTION 3 : INTERRUPTIONS PÉRIPHÉRIQUES
// ============================================================

// --- Périphériques de communication ---

/**
 * @brief Interruption USART1 (Debug série)
 * 
 * Déclenchée par :
 * - Réception d'un caractère (RXNE)
 * - Fin de transmission (TC)
 * - Erreur de réception (ORE, FE, NE)
 */
void USART1_IRQHandler(void);

/**
 * @brief Interruption USART2 (réservé futur usage)
 */
void USART2_IRQHandler(void);

/**
 * @brief Interruption USART3 (réservé futur usage)
 */
void USART3_IRQHandler(void);

/**
 * @brief Interruption SPI1 (réservé futur usage)
 */
void SPI1_IRQHandler(void);

/**
 * @brief Interruption SPI2 (Module LoRa RA-02)
 * 
 * Déclenchée par :
 * - Fin de transmission DMA
 * - Fin de réception DMA
 * - Erreur SPI
 * 
 * Utilisée pour le module LoRa en mode DMA.
 */
void SPI2_IRQHandler(void);

/**
 * @brief Interruption SPI3 (réservé futur usage)
 */
void SPI3_IRQHandler(void);

/**
 * @brief Interruption I2C1 (Écran tactile XPT2046)
 * 
 * Déclenchée par :
 * - Fin de transmission
 * - Fin de réception
 * - Erreur I2C
 */
void I2C1_EV_IRQHandler(void);    // Événements I2C
void I2C1_ER_IRQHandler(void);    // Erreurs I2C

// --- Périphériques audio ---

/**
 * @brief Interruption ADC1 (Microphone)
 * 
 * Déclenchée par :
 * - Fin de conversion
 * - Fin de séquence
 * - Watchdog analogique
 * 
 * Utilisée pour la capture audio en continu.
 */
void ADC_IRQHandler(void);

/**
 * @brief Interruption DMA2 Stream0 (ADC1 - Microphone)
 * 
 * Transfert DMA terminé pour la capture audio.
 */
void DMA2_Stream0_IRQHandler(void);

/**
 * @brief Interruption DMA1 Stream5 (DAC1 - Haut-parleur)
 * 
 * Transfert DMA terminé pour la lecture audio.
 */
void DMA1_Stream5_IRQHandler(void);

// --- Timers ---

/**
 * @brief Interruption TIM2 (réservé futur usage)
 */
void TIM2_IRQHandler(void);

/**
 * @brief Interruption TIM3 (réservé futur usage)
 */
void TIM3_IRQHandler(void);

/**
 * @brief Interruption TIM4 (PWM - Backlight clavier)
 * 
 * Déclenchée à chaque période PWM.
 * Utilisée pour le rétroéclairage du clavier.
 */
void TIM4_IRQHandler(void);

/**
 * @brief Interruption TIM5 (réservé futur usage)
 */
void TIM5_IRQHandler(void);

/**
 * @brief Interruption TIM6 (Base de temps audio)
 * 
 * Déclenchée à 8 kHz pour l'échantillonnage audio.
 * Fréquence : 180 MHz / (ARR+1) = 8000 Hz
 */
void TIM6_DAC_IRQHandler(void);

/**
 * @brief Interruption TIM7 (Base de temps secondaire)
 */
void TIM7_IRQHandler(void);

/**
 * @brief Interruption TIM9 (PWM - Lampe torche + Backlight écran)
 * 
 * Déclenchée à chaque période PWM.
 */
void TIM1_BRK_TIM9_IRQHandler(void);

// --- Contrôleurs d'affichage ---

/**
 * @brief Interruption LTDC (Écran TFT)
 * 
 * Déclenchée par :
 * - Fin de ligne (Line Interrupt)
 * - Fin de trame (Frame Interrupt)
 * - Erreur de synchronisation
 * 
 * Utilisée pour le double buffering et
 * la synchronisation verticale.
 */
void LTDC_IRQHandler(void);

/**
 * @brief Interruption DMA2D (Accélérateur graphique)
 * 
 * Déclenchée par :
 * - Fin de transfert DMA2D
 * - Erreur de transfert
 * 
 * Permet de lancer le transfert suivant
 * sans bloquer le CPU.
 */
void DMA2D_IRQHandler(void);

// --- GPIO ---

/**
 * @brief Interruption EXTI ligne 0 (Bouton utilisateur)
 */
void EXTI0_IRQHandler(void);

/**
 * @brief Interruption EXTI ligne 1 (Tactile IRQ)
 */
void EXTI1_IRQHandler(void);

/**
 * @brief Interruption EXTI ligne 2 (LoRa DIO0)
 * 
 * Interruption du module LoRa :
 * - Paquet reçu
 * - Transmission terminée
 * - Timeout réception
 */
void EXTI2_IRQHandler(void);

/**
 * @brief Interruption EXTI ligne 3 (LoRa DIO1)
 */
void EXTI3_IRQHandler(void);

/**
 * @brief Interruption EXTI ligne 4 (LoRa DIO2 - optionnel)
 */
void EXTI4_IRQHandler(void);

/**
 * @brief Interruption EXTI ligne 9_5 (Touches clavier)
 * 
 * Regroupe les interruptions sur les lignes 5 à 9.
 */
void EXTI9_5_IRQHandler(void);

/**
 * @brief Interruption EXTI ligne 15_10
 * 
 * Regroupe les interruptions sur les lignes 10 à 15.
 */
void EXTI15_10_IRQHandler(void);

// ============================================================
// SECTION 4 : FONCTIONS DE RAPPEL (CALLBACKS) DU HAL
// ============================================================

/**
 * @brief Callback de fin de transmission SPI
 * @param hspi Handle SPI
 * 
 * Appelée automatiquement par le HAL après une transmission SPI.
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);

/**
 * @brief Callback de fin de réception SPI
 * @param hspi Handle SPI
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi);

/**
 * @brief Callback de fin de transmission/réception SPI
 * @param hspi Handle SPI
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi);

/**
 * @brief Callback d'erreur SPI
 * @param hspi Handle SPI
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);

/**
 * @brief Callback de fin de conversion ADC
 * @param hadc Handle ADC
 * 
 * Appelée quand l'ADC a terminé une conversion.
 * Utilisée pour la capture audio.
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc);

/**
 * @brief Callback de fin de conversion DMA ADC
 * @param hadc Handle ADC
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc);

/**
 * @brief Callback de fin de conversion DAC
 * @param hdac Handle DAC
 */
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef* hdac);

/**
 * @brief Callback de période PWM (Timer)
 * @param htim Handle Timer
 * 
 * Appelée à chaque période du timer configuré.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

/**
 * @brief Callback de fin de transfert DMA
 * @param hdma Handle DMA
 */
void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma);

/**
 * @brief Callback d'erreur DMA
 * @param hdma Handle DMA
 */
void HAL_DMA_ErrorCallback(DMA_HandleTypeDef *hdma);

/**
 * @brief Callback de réception UART
 * @param huart Handle UART
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

/**
 * @brief Callback de transmission UART terminée
 * @param huart Handle UART
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);

/**
 * @brief Callback d'erreur UART
 * @param huart Handle UART
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);

/**
 * @brief Callback interruption GPIO (EXTI)
 * @param GPIO_Pin Pin ayant déclenché l'interruption
 * 
 * Appelée quand une interruption EXTI est détectée.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

/**
 * @brief Callback fin de trame LTDC
 * @param hltdc Handle LTDC
 */
void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *hltdc);

/**
 * @brief Callback de transfert DMA2D terminé
 * @param hdma2d Handle DMA2D
 */
void HAL_DMA2D_TransferCpltCallback(DMA2D_HandleTypeDef *hdma2d);

// ============================================================
// SECTION 5 : CONSTANTES ET TYPES
// ============================================================

/**
 * @def MAX_HANDLERS
 * @brief Nombre maximum de callbacks par périphérique
 */
#define MAX_CALLBACKS_PER_PERIPHERAL    4

/**
 * @enum InterruptPriority
 * @brief Priorités des interruptions (0 = plus haute priorité)
 * 
 * Sur Cortex-M4 : 16 niveaux de priorité (0-15)
 * Plus le chiffre est petit, plus la priorité est haute.
 */
typedef enum {
    IRQ_PRIORITY_CRITICAL   = 0,    // Critique (HardFault, NMI)
    IRQ_PRIORITY_HIGHEST    = 1,    // Très haute (audio, LoRa DIO0)
    IRQ_PRIORITY_HIGH       = 2,    // Haute (DMA, timers critiques)
    IRQ_PRIORITY_MEDIUM     = 3,    // Moyenne (SPI, I2C, UART)
    IRQ_PRIORITY_LOW        = 4,    // Basse (GPIO EXTI)
    IRQ_PRIORITY_LOWEST     = 5,    // Très basse (tâches de fond)
    IRQ_PRIORITY_NONE       = 15    // Désactivée
} InterruptPriority;

/**
 * @def PREEMPTION_PRIORITY_BITS
 * @brief Nombre de bits pour la priorité de préemption
 * 
 * 4 bits = 16 niveaux de priorité
 */
#define PREEMPTION_PRIORITY_BITS    4
#define SUB_PRIORITY_BITS           0

// ============================================================
// SECTION 6 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Désactiver toutes les interruptions
 */
#define DISABLE_ALL_INTERRUPTS()    __disable_irq()

/**
 * @brief Activer toutes les interruptions
 */
#define ENABLE_ALL_INTERRUPTS()     __enable_irq()

/**
 * @brief Désactiver une interruption spécifique
 * @param irq Numéro de l'interruption
 */
#define DISABLE_IRQ(irq)            HAL_NVIC_DisableIRQ(irq)

/**
 * @brief Activer une interruption spécifique
 * @param irq Numéro de l'interruption
 */
#define ENABLE_IRQ(irq)             HAL_NVIC_EnableIRQ(irq)

/**
 * @brief Définir la priorité d'une interruption
 * @param irq Numéro de l'interruption
 * @param preempt Priorité de préemption (0-15)
 * @param sub Sous-priorité (0)
 */
#define SET_IRQ_PRIORITY(irq, preempt, sub) \
    HAL_NVIC_SetPriority(irq, preempt, sub)

/**
 * @brief Vérifier si on est dans une interruption
 * @return 1 si dans une IRQ, 0 sinon
 */
#define IS_IN_INTERRUPT()           (__get_IPSR() != 0)

// ============================================================
// SECTION 7 : VARIABLES GLOBALES DE DÉBOGAGE
// ============================================================

#ifdef DEBUG
    /**
     * @brief Compteur d'interruptions pour le débogage
     */
    extern volatile uint32_t irq_counter_total;
    extern volatile uint32_t irq_counter_systick;
    extern volatile uint32_t irq_counter_lora;
    extern volatile uint32_t irq_counter_audio;
    extern volatile uint32_t irq_counter_display;
    extern volatile uint32_t irq_counter_uart;
    
    /**
     * @brief Dernière interruption reçue (pour debug)
     */
    extern volatile int32_t last_irq_type;
    
    /**
     * @brief Horodatage de la dernière HardFault
     */
    extern volatile uint32_t hardfault_timestamp;
#endif

// ============================================================
// SECTION 8 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // STM32F4xx_IT_H