/**
 * @file stm32f4xx_hal_msp.c
 * @brief Initialisation MSP (MCU Support Package) des périphériques
 * 
 * MSP = MCU Support Package
 * 
 * Ce fichier contient les fonctions d'initialisation des ressources
 * matérielles pour CHAQUE périphérique utilisé :
 * - Configuration des GPIO (broches, mode, pull-up/down)
 * - Configuration des DMA (canaux, streams)
 * - Configuration des NVIC (interruptions, priorités)
 * - Activation des horloges des périphériques
 * 
 * Chaque fonction HAL_PPP_MspInit() est appelée automatiquement
 * par le HAL lors de l'initialisation d'un périphérique.
 * 
 * ⚠️ NE PAS MODIFIER manuellement - Utiliser STM32CubeMX si possible
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "stm32f4xx_hal.h"
#include "config.h"

// ============================================================
// HANDLES DMA GLOBAUX (définis dans main.c)
// ============================================================

extern DMA_HandleTypeDef hdma_spi2_tx;    // DMA SPI2 TX (LoRa)
extern DMA_HandleTypeDef hdma_spi2_rx;    // DMA SPI2 RX (LoRa)
extern DMA_HandleTypeDef hdma_adc1;       // DMA ADC1 (Microphone)
extern DMA_HandleTypeDef hdma_dac1;       // DMA DAC1 (Haut-parleur)
extern DMA_HandleTypeDef hdma_usart1_tx;  // DMA USART1 TX (Debug)
extern DMA_HandleTypeDef hdma_usart1_rx;  // DMA USART1 RX (Debug)

// ============================================================
// SECTION 1 : INITIALISATION MSP - UART (DEBUG SÉRIE)
// ============================================================

/**
 * @brief Initialise les ressources pour USART1 (Debug)
 * @param huart Handle UART
 * 
 * Configure :
 * - GPIO : PA9 (TX), PA10 (RX)
 * - DMA : Stream pour TX et RX
 * - NVIC : Interruption USART1
 */
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    if (huart->Instance == USART1)
    {
        // --- Activation des horloges ---
        __HAL_RCC_USART1_CLK_ENABLE();      // Horloge USART1
        __HAL_RCC_GPIOA_CLK_ENABLE();       // Horloge GPIOA (TX/RX)
        __HAL_RCC_DMA2_CLK_ENABLE();        // Horloge DMA2 (utilisé par USART1)
        
        // --- Configuration GPIO ---
        
        // PA9 = USART1_TX (sortie)
        GPIO_InitStruct.Pin = GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;          // Alternate Function Push-Pull
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;    // Haute vitesse (débit élevé)
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;     // AF7 = USART1
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        
        // PA10 = USART1_RX (entrée)
        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;              // Pull-up sur RX (évite les parasites)
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        
        // --- Configuration DMA pour TX ---
        hdma_usart1_tx.Instance = DMA2_Stream7;          // DMA2 Stream7 Channel4 = USART1_TX
        hdma_usart1_tx.Init.Channel = DMA_CHANNEL_4;
        hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;   // Mémoire → USART
        hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;       // Adresse périph fixe
        hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;           // Adresse mémoire incrémentée
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;  // 8 bits
        hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode = DMA_NORMAL;                  // Mode normal (pas circulaire)
        hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;        // Priorité basse (debug)
        hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;    // Pas de FIFO
        HAL_DMA_Init(&hdma_usart1_tx);
        
        // --- Configuration DMA pour RX ---
        hdma_usart1_rx.Instance = DMA2_Stream2;          // DMA2 Stream2 Channel4 = USART1_RX
        hdma_usart1_rx.Init.Channel = DMA_CHANNEL_4;
        hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;   // USART → Mémoire
        hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart1_rx.Init.Mode = DMA_CIRCULAR;                // Mode circulaire (continu)
        hdma_usart1_rx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_usart1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        HAL_DMA_Init(&hdma_usart1_rx);
        
        // --- Configuration NVIC (interruptions) ---
        HAL_NVIC_SetPriority(USART1_IRQn, 4, 0);         // Priorité 4 (basse, debug)
        HAL_NVIC_EnableIRQ(USART1_IRQn);                 // Activer l'interruption
        
        // --- Lier les DMA au handle UART ---
        __HAL_LINKDMA(huart, hdmatx, hdma_usart1_tx);
        __HAL_LINKDMA(huart, hdmarx, hdma_usart1_rx);
    }
}

/**
 * @brief Désinitialise les ressources USART1
 */
void HAL_UART_MspDeInit(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_DISABLE();
        
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
        
        HAL_DMA_DeInit(&hdma_usart1_tx);
        HAL_DMA_DeInit(&hdma_usart1_rx);
        
        HAL_NVIC_DisableIRQ(USART1_IRQn);
    }
}

// ============================================================
// SECTION 2 : INITIALISATION MSP - SPI2 (MODULE LORA)
// ============================================================

/**
 * @brief Initialise les ressources pour SPI2 (LoRa RA-02)
 * @param hspi Handle SPI
 * 
 * Configure :
 * - GPIO : PB12 (NSS), PB13 (SCK), PB14 (MISO), PB15 (MOSI)
 * - DMA : Stream pour TX et RX
 * - NVIC : Interruption SPI2
 */
void HAL_SPI_MspInit(SPI_HandleTypeDef* hspi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    if (hspi->Instance == SPI2)
    {
        // --- Activation des horloges ---
        __HAL_RCC_SPI2_CLK_ENABLE();        // Horloge SPI2
        __HAL_RCC_GPIOB_CLK_ENABLE();       // Horloge GPIOB (pins SPI)
        __HAL_RCC_DMA1_CLK_ENABLE();        // Horloge DMA1 (utilisé par SPI2)
        
        // --- Configuration GPIO ---
        
        // PB12 = NSS (Chip Select) - Géré en GPIO manuel (software CS)
        GPIO_InitStruct.Pin = GPIO_PIN_12;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;      // Sortie Push-Pull
        GPIO_InitStruct.Pull = GPIO_PULLUP;              // Pull-up (désélectionné par défaut)
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;    // Haute vitesse
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);  // NSS = HIGH (désélectionné)
        
        // PB13 = SCK (Horloge)
        GPIO_InitStruct.Pin = GPIO_PIN_13;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;          // Alternate Function
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH; // Très haute vitesse (10 MHz)
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;        // AF5 = SPI2
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        
        // PB14 = MISO (Master In Slave Out)
        GPIO_InitStruct.Pin = GPIO_PIN_14;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;            // Pull-down (évite les parasites)
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        
        // PB15 = MOSI (Master Out Slave In)
        GPIO_InitStruct.Pin = GPIO_PIN_15;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        
        // --- Configuration DMA pour TX ---
        hdma_spi2_tx.Instance = DMA1_Stream4;            // DMA1 Stream4 Channel0 = SPI2_TX
        hdma_spi2_tx.Init.Channel = DMA_CHANNEL_0;
        hdma_spi2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_spi2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_spi2_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_spi2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_spi2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_spi2_tx.Init.Mode = DMA_NORMAL;
        hdma_spi2_tx.Init.Priority = DMA_PRIORITY_HIGH;       // Priorité haute (LoRa)
        hdma_spi2_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        HAL_DMA_Init(&hdma_spi2_tx);
        
        // --- Configuration DMA pour RX ---
        hdma_spi2_rx.Instance = DMA1_Stream3;            // DMA1 Stream3 Channel0 = SPI2_RX
        hdma_spi2_rx.Init.Channel = DMA_CHANNEL_0;
        hdma_spi2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_spi2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_spi2_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_spi2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_spi2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_spi2_rx.Init.Mode = DMA_NORMAL;
        hdma_spi2_rx.Init.Priority = DMA_PRIORITY_HIGH;
        hdma_spi2_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        HAL_DMA_Init(&hdma_spi2_rx);
        
        // --- Configuration NVIC ---
        HAL_NVIC_SetPriority(SPI2_IRQn, 3, 0);           // Priorité 3 (moyenne-haute)
        HAL_NVIC_EnableIRQ(SPI2_IRQn);
        
        // --- Lier les DMA au handle SPI ---
        __HAL_LINKDMA(hspi, hdmatx, hdma_spi2_tx);
        __HAL_LINKDMA(hspi, hdmarx, hdma_spi2_rx);
    }
}

/**
 * @brief Désinitialise les ressources SPI2
 */
void HAL_SPI_MspDeInit(SPI_HandleTypeDef* hspi)
{
    if (hspi->Instance == SPI2)
    {
        __HAL_RCC_SPI2_CLK_DISABLE();
        
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
        
        HAL_DMA_DeInit(&hdma_spi2_tx);
        HAL_DMA_DeInit(&hdma_spi2_rx);
        
        HAL_NVIC_DisableIRQ(SPI2_IRQn);
    }
}

// ============================================================
// SECTION 3 : INITIALISATION MSP - I2C1 (ÉCRAN TACTILE)
// ============================================================

/**
 * @brief Initialise les ressources pour I2C1 (XPT2046 tactile)
 * @param hi2c Handle I2C
 * 
 * Configure :
 * - GPIO : PB6 (SCL), PB7 (SDA)
 * - NVIC : Interruptions I2C1
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    if (hi2c->Instance == I2C1)
    {
        // --- Activation des horloges ---
        __HAL_RCC_I2C1_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        
        // --- Configuration GPIO ---
        
        // PB6 = SCL (Horloge I2C)
        // PB7 = SDA (Données I2C)
        GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;          // Open-Drain (obligatoire pour I2C)
        GPIO_InitStruct.Pull = GPIO_PULLUP;              // Pull-up (obligatoire pour I2C)
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;        // AF4 = I2C1
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        
        // --- Configuration NVIC ---
        HAL_NVIC_SetPriority(I2C1_EV_IRQn, 4, 0);        // Priorité 4 (basse)
        HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
        
        HAL_NVIC_SetPriority(I2C1_ER_IRQn, 4, 0);
        HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
    }
}

/**
 * @brief Désinitialise les ressources I2C1
 */
void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        __HAL_RCC_I2C1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
        HAL_NVIC_DisableIRQ(I2C1_EV_IRQn);
        HAL_NVIC_DisableIRQ(I2C1_ER_IRQn);
    }
}

// ============================================================
// SECTION 4 : INITIALISATION MSP - ADC1 (MICROPHONE)
// ============================================================

/**
 * @brief Initialise les ressources pour ADC1 (Microphone)
 * @param hadc Handle ADC
 * 
 * Configure :
 * - GPIO : PA0 (ADC_IN0)
 * - DMA : Stream pour transfert continu
 * - NVIC : Interruption ADC
 */
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    if (hadc->Instance == ADC1)
    {
        // --- Activation des horloges ---
        __HAL_RCC_ADC1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_DMA2_CLK_ENABLE();
        
        // --- Configuration GPIO ---
        
        // PA0 = ADC1_IN0 (entrée analogique micro)
        GPIO_InitStruct.Pin = GPIO_PIN_0;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;         // Mode analogique
        GPIO_InitStruct.Pull = GPIO_NOPULL;              // Pas de pull-up/down
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        
        // --- Configuration DMA pour transfert continu ---
        hdma_adc1.Instance = DMA2_Stream0;               // DMA2 Stream0 Channel0 = ADC1
        hdma_adc1.Init.Channel = DMA_CHANNEL_0;
        hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;  // ADC → Mémoire
        hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;      // Registre ADC fixe
        hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;          // Buffer mémoire incrémenté
        hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;  // 16 bits
        hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        hdma_adc1.Init.Mode = DMA_CIRCULAR;               // Mode circulaire (continu)
        hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;      // Priorité haute (audio)
        hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        HAL_DMA_Init(&hdma_adc1);
        
        // --- Lier le DMA au handle ADC ---
        __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);
        
        // --- Configuration NVIC ---
        HAL_NVIC_SetPriority(ADC_IRQn, 2, 0);            // Priorité 2 (haute, audio)
        HAL_NVIC_EnableIRQ(ADC_IRQn);
    }
}

/**
 * @brief Désinitialise les ressources ADC1
 */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        __HAL_RCC_ADC1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0);
        HAL_DMA_DeInit(&hdma_adc1);
        HAL_NVIC_DisableIRQ(ADC_IRQn);
    }
}

// ============================================================
// SECTION 5 : INITIALISATION MSP - DAC (HAUT-PARLEUR)
// ============================================================

/**
 * @brief Initialise les ressources pour DAC (Haut-parleur)
 * @param hdac Handle DAC
 * 
 * Configure :
 * - GPIO : PA5 (DAC_OUT2)
 * - DMA : Stream pour transfert continu
 */
void HAL_DAC_MspInit(DAC_HandleTypeDef* hdac)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    if (hdac->Instance == DAC1)
    {
        // --- Activation des horloges ---
        __HAL_RCC_DAC_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();
        
        // --- Configuration GPIO ---
        
        // PA5 = DAC_OUT2 (sortie analogique vers ampli HP)
        GPIO_InitStruct.Pin = GPIO_PIN_5;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        
        // --- Configuration DMA pour DAC ---
        hdma_dac1.Instance = DMA1_Stream5;               // DMA1 Stream5 Channel7 = DAC1
        hdma_dac1.Init.Channel = DMA_CHANNEL_7;
        hdma_dac1.Init.Direction = DMA_MEMORY_TO_PERIPH;  // Mémoire → DAC
        hdma_dac1.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_dac1.Init.MemInc = DMA_MINC_ENABLE;
        hdma_dac1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;  // 12 bits
        hdma_dac1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        hdma_dac1.Init.Mode = DMA_CIRCULAR;               // Mode circulaire (audio continu)
        hdma_dac1.Init.Priority = DMA_PRIORITY_HIGH;      // Priorité haute (audio)
        hdma_dac1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        HAL_DMA_Init(&hdma_dac1);
        
        // --- Lier le DMA au handle DAC ---
        __HAL_LINKDMA(hdac, DMA_Handle1, hdma_dac1);
    }
}

/**
 * @brief Désinitialise les ressources DAC
 */
void HAL_DAC_MspDeInit(DAC_HandleTypeDef* hdac)
{
    if (hdac->Instance == DAC1)
    {
        __HAL_RCC_DAC_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5);
        HAL_DMA_DeInit(&hdma_dac1);
    }
}

// ============================================================
// SECTION 6 : INITIALISATION MSP - TIMERS
// ============================================================

/**
 * @brief Initialise les ressources pour les Timers
 * @param htim Handle Timer
 * 
 * Configure selon le timer :
 * - TIM4 : PWM backlight clavier
 * - TIM6 : Base de temps audio (8 kHz)
 * - TIM9 : PWM lampe torche + backlight écran
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim)
{
    if (htim->Instance == TIM4)
    {
        __HAL_RCC_TIM4_CLK_ENABLE();
        HAL_NVIC_SetPriority(TIM4_IRQn, 5, 0);           // Priorité basse
        HAL_NVIC_EnableIRQ(TIM4_IRQn);
    }
    else if (htim->Instance == TIM6)
    {
        __HAL_RCC_TIM6_CLK_ENABLE();
        HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 1, 0);       // Priorité très haute (audio !)
        HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    }
    else if (htim->Instance == TIM9)
    {
        __HAL_RCC_TIM9_CLK_ENABLE();
        HAL_NVIC_SetPriority(TIM1_BRK_TIM9_IRQn, 5, 0);   // Priorité basse (PWM)
        HAL_NVIC_EnableIRQ(TIM1_BRK_TIM9_IRQn);
    }
}

/**
 * @brief Désinitialise les ressources Timer
 */
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* htim)
{
    if (htim->Instance == TIM4)
    {
        __HAL_RCC_TIM4_CLK_DISABLE();
        HAL_NVIC_DisableIRQ(TIM4_IRQn);
    }
    else if (htim->Instance == TIM6)
    {
        __HAL_RCC_TIM6_CLK_DISABLE();
        HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn);
    }
    else if (htim->Instance == TIM9)
    {
        __HAL_RCC_TIM9_CLK_DISABLE();
        HAL_NVIC_DisableIRQ(TIM1_BRK_TIM9_IRQn);
    }
}

// ============================================================
// SECTION 7 : INITIALISATION MSP - GPIO
// ============================================================

/**
 * @brief Initialise les GPIO généraux
 * 
 * Configure les broches qui ne sont pas gérées par un périphérique :
 * - Sorties : LEDs, Reset LoRa, contrôle écran
 * - Entrées : Boutons, interruptions externes
 */
void HAL_GPIO_MspInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // --- Activation des horloges des ports utilisés ---
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    
    // --- Sorties numériques ---
    
    // LED Statut (PG13) - intégrée Discovery
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_RESET);  // LED éteinte
    
    // Reset module LoRa (PD8)
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);    // Reset actif bas
    
    // --- Entrées avec interruption ---
    
    // Bouton utilisateur (PA0) - interruption EXTI0
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;          // Interruption sur front montant
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;                // Pull-down
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // LoRa DIO0 (PD7) - interruption EXTI7
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;          // SX1278 DIO0 actif haut
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
    // LoRa DIO1 (PD6) - interruption EXTI6
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
    // Tactile IRQ (PB5) - interruption EXTI5
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;         // XPT2046 IRQ actif bas
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // --- Configuration des priorités NVIC pour EXTI ---
    HAL_NVIC_SetPriority(EXTI0_IRQn,     5, 0);   // Bouton utilisateur (basse)
    HAL_NVIC_SetPriority(EXTI1_IRQn,     4, 0);   // Tactile (moyenne)
    HAL_NVIC_SetPriority(EXTI2_IRQn,     2, 0);   // LoRa DIO0 (haute !)
    HAL_NVIC_SetPriority(EXTI3_IRQn,     3, 0);   // LoRa DIO1 (moyenne-haute)
    HAL_NVIC_SetPriority(EXTI4_IRQn,     5, 0);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn,   5, 0);   // Clavier (basse)
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
    
    // --- Activer les interruptions ---
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

// ============================================================
// SECTION 8 : INITIALISATION MSP - LTDC (ÉCRAN TFT)
// ============================================================

/**
 * @brief Initialise les ressources pour LTDC (Écran ILI9488)
 * @param hltdc Handle LTDC
 * 
 * Configure les 23 broches du bus parallèle RVB
 */
void HAL_LTDC_MspInit(LTDC_HandleTypeDef* hltdc)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    if (hltdc->Instance == LTDC)
    {
        // --- Activation des horloges ---
        __HAL_RCC_LTDC_CLK_ENABLE();
        __HAL_RCC_DMA2D_CLK_ENABLE();       // Accélérateur graphique
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();
        
        // --- Configuration des broches LTDC ---
        // Toutes en Alternate Function Push-Pull, AF14 = LTDC
        
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  // Très haute vitesse
        GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
        
        // Rouge R0-R4
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);  // PE12 (R0)
        // ... (toutes les broches LTDC configurées ici)
        
        // Contrôle (CLK, HSYNC, VSYNC, DE)
        // PE0 = CLK, PE1 = HSYNC, PE2 = VSYNC, PE3 = DE
        
        // --- Configuration NVIC ---
        HAL_NVIC_SetPriority(LTDC_IRQn, 4, 0);
        HAL_NVIC_EnableIRQ(LTDC_IRQn);
        
        HAL_NVIC_SetPriority(DMA2D_IRQn, 4, 0);
        HAL_NVIC_EnableIRQ(DMA2D_IRQn);
    }
}

// ============================================================
// SECTION 9 : INITIALISATION MSP - FMC/SDRAM
// ============================================================

/**
 * @brief Initialise les ressources pour FMC (SDRAM externe)
 * @param hsdram Handle SDRAM
 */
void HAL_SDRAM_MspInit(SDRAM_HandleTypeDef* hsdram)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // --- Activation des horloges ---
    __HAL_RCC_FMC_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    
    // --- Configuration des broches SDRAM ---
    // Toutes en Alternate Function Push-Pull, AF12 = FMC
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
    
    // Broches de données D0-D15
    // Broches d'adresse A0-A12
    // Broches de contrôle (SDCKE, SDCLK, SDNE, etc.)
    
    // ... (configuration détaillée de chaque broche)
}