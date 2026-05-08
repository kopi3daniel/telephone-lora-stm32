/**
 * @file stm32f4xx_it.c
 * @brief Gestionnaires d'interruptions - Implémentations
 * 
 * Ce fichier contient l'implémentation de TOUS les gestionnaires
 * d'interruptions du projet.
 * 
 * ⚠️ RÈGLES IMPORTANTES :
 * 1. Les handlers d'interruption doivent être RAPIDES
 * 2. Pas de printf() dans une IRQ (trop lent)
 * 3. Pas de HAL_Delay() dans une IRQ (bloquant)
 * 4. Utiliser des flags volatils pour communiquer avec le main
 * 5. Priorité audio > LoRa > UI
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "stm32f4xx_it.h"
#include "config.h"
#include "include/project_config.h"

// ============================================================
// VARIABLES GLOBALES DE DÉBOGAGE
// ============================================================

#ifdef DEBUG
    volatile uint32_t irq_counter_total = 0;      // Compteur total d'IRQ
    volatile uint32_t irq_counter_systick = 0;    // Compteur SysTick
    volatile uint32_t irq_counter_lora = 0;       // Compteur LoRa
    volatile uint32_t irq_counter_audio = 0;      // Compteur audio
    volatile uint32_t irq_counter_display = 0;    // Compteur affichage
    volatile uint32_t irq_counter_uart = 0;       // Compteur UART
    volatile int32_t last_irq_type = -1;          // Dernière IRQ
    volatile uint32_t hardfault_timestamp = 0;    // Timestamp HardFault
#endif

// ============================================================
// FLAGS VOLATILS (communication IRQ → main loop)
// ============================================================

/**
 * @brief Flag : paquet LoRa reçu
 * 
 * Positionné par EXTI2_IRQHandler (DIO0 du SX1278)
 * Traité par la boucle principale
 */
volatile bool lora_packet_received = false;

/**
 * @brief Flag : transmission LoRa terminée
 */
volatile bool lora_transmission_done = false;

/**
 * @brief Flag : buffer audio plein (prêt à envoyer)
 */
volatile bool audio_buffer_ready = false;

/**
 * @brief Flag : buffer audio vide (prêt à recevoir)
 */
volatile bool audio_buffer_played = false;

/**
 * @brief Flag : écran tactile touché
 */
volatile bool touch_detected = false;

/**
 * @brief Flag : bouton utilisateur pressé
 */
volatile bool user_button_pressed = false;

/**
 * @brief Flag : transfert DMA2D terminé
 */
volatile bool dma2d_transfer_done = true;

/**
 * @brief Flag : trame écran terminée (VSYNC)
 */
volatile bool ltdc_frame_done = false;

/**
 * @brief Flag : donnée UART reçue
 */
volatile bool uart_data_received = false;

/**
 * @brief Dernier caractère reçu par UART
 */
volatile uint8_t uart_rx_char = 0;

// ============================================================
// SECTION 1 : INTERRUPTIONS SYSTÈME (Cortex-M4)
// ============================================================

/**
 * @brief Gestionnaire NMI (Non-Maskable Interrupt)
 * 
 * L'interruption NMI ne peut PAS être désactivée.
 * Elle est déclenchée par des erreurs hardware critiques.
 * 
 * ⚠️ Cette fonction ne doit JAMAIS retourner en cas d'erreur.
 */
void NMI_Handler(void)
{
    // Enregistrer l'erreur
    hardfault_timestamp = HAL_GetTick();
    
    // Tenter de logger l'erreur avant de bloquer
    // (écriture directe dans un registre pour éviter les appels complexes)
    
    // Boucle infinie - le système est dans un état instable
    while (1)
    {
        // Faire clignoter la LED très rapidement (SOS hardware)
        HAL_GPIO_TogglePin(STATUS_LED_PORT, STATUS_LED_PIN);
        for (volatile uint32_t i = 0; i < 100000; i++);
    }
}

/**
 * @brief Gestionnaire HardFault
 * 
 * Appelé en cas d'erreur fatale du CPU :
 * - Accès mémoire invalide
 * - Division par zéro
 * - Instruction non définie
 * 
 * Cette fonction capture les informations de debug
 * avant de bloquer le système.
 */
void HardFault_Handler(void)
{
    // Sauvegarder le timestamp
    hardfault_timestamp = HAL_GetTick();
    
    // Récupérer les registres empilés automatiquement
    // par le CPU lors de l'exception
    uint32_t *stack_pointer = (uint32_t *)__get_MSP();
    
    // Registres sauvegardés sur la stack :
    // SP[0] = R0
    // SP[1] = R1
    // SP[2] = R2
    // SP[3] = R3
    // SP[4] = R12
    // SP[5] = LR (Link Register)
    // SP[6] = PC (Program Counter) ← Adresse qui a causé la faute
    // SP[7] = xPSR (Program Status Register)
    
    volatile uint32_t stacked_r0  = stack_pointer[0];
    volatile uint32_t stacked_r1  = stack_pointer[1];
    volatile uint32_t stacked_r2  = stack_pointer[2];
    volatile uint32_t stacked_r3  = stack_pointer[3];
    volatile uint32_t stacked_r12 = stack_pointer[4];
    volatile uint32_t stacked_lr  = stack_pointer[5];
    volatile uint32_t stacked_pc  = stack_pointer[6];
    volatile uint32_t stacked_psr = stack_pointer[7];
    
    // Lire les registres de statut de faute
    volatile uint32_t cfsr = SCB->CFSR;  // Configurable Fault Status Register
    volatile uint32_t hfsr = SCB->HFSR;  // HardFault Status Register
    volatile uint32_t mmfar = SCB->MMFAR; // MemManage Fault Address Register
    volatile uint32_t bfar = SCB->BFAR;   // BusFault Address Register
    
    // Éviter les warnings "unused variable"
    (void)stacked_r0;
    (void)stacked_r1;
    (void)stacked_r2;
    (void)stacked_r3;
    (void)stacked_r12;
    (void)stacked_lr;
    (void)stacked_pc;
    (void)stacked_psr;
    (void)cfsr;
    (void)hfsr;
    (void)mmfar;
    (void)bfar;
    
    // Boucle infinie avec LED
    while (1)
    {
        // Pattern : 3 clignotements rapides = HardFault
        for (int i = 0; i < 3; i++)
        {
            HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN, GPIO_PIN_SET);
            for (volatile uint32_t d = 0; d < 500000; d++);
            HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN, GPIO_PIN_RESET);
            for (volatile uint32_t d = 0; d < 500000; d++);
        }
        // Pause longue
        for (volatile uint32_t d = 0; d < 5000000; d++);
    }
}

/**
 * @brief Gestionnaire MemManage (MPU Fault)
 */
void MemManage_Handler(void)
{
    // Erreur de protection mémoire
    // Rediriger vers HardFault pour diagnostic
    HardFault_Handler();
}

/**
 * @brief Gestionnaire BusFault
 */
void BusFault_Handler(void)
{
    // Erreur de bus (accès mémoire invalide)
    HardFault_Handler();
}

/**
 * @brief Gestionnaire UsageFault
 */
void UsageFault_Handler(void)
{
    // Erreur d'utilisation du CPU
    HardFault_Handler();
}

/**
 * @brief Gestionnaire SVCall (Supervisor Call)
 * 
 * Utilisé par FreeRTOS pour les appels système.
 * Si vous n'utilisez pas FreeRTOS, cette fonction
 * ne devrait jamais être appelée.
 */
void SVC_Handler(void)
{
    // FreeRTOS gère cette interruption
    // Si pas de FreeRTOS : ne rien faire
}

/**
 * @brief Gestionnaire Debug Monitor
 */
void DebugMon_Handler(void)
{
    // Utilisé par le débogueur
    // Ne rien faire en mode normal
}

/**
 * @brief Gestionnaire PendSV
 * 
 * Utilisé par FreeRTOS pour le changement de contexte.
 */
void PendSV_Handler(void)
{
    // FreeRTOS gère cette interruption
}

/**
 * @brief Gestionnaire SysTick (Timer système)
 * 
 * Appelé toutes les 1 ms (1000 Hz).
 * C'est le battement de cœur du système.
 * 
 * ⚠️ Cette fonction doit être RAPIDE (< 10 µs)
 */
void SysTick_Handler(void)
{
    // Incrémenter le compteur de ticks du HAL
    // Cela permet à HAL_Delay() et HAL_GetTick() de fonctionner
    HAL_IncTick();
    
#ifdef DEBUG
    irq_counter_total++;
    irq_counter_systick++;
#endif
}

// ============================================================
// SECTION 2 : INTERRUPTIONS PÉRIPHÉRIQUES
// ============================================================

/**
 * @brief Gestionnaire USART1 (Debug série)
 * 
 * Appelé quand un caractère est reçu ou transmis.
 */
void USART1_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
    irq_counter_uart++;
#endif
    
    // Vérifier si c'est une réception
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE))
    {
        // Lire le caractère reçu
        uint8_t ch = (uint8_t)(huart1.Instance->DR & 0xFF);
        
        // Stocker pour traitement par la boucle principale
        uart_rx_char = ch;
        uart_data_received = true;
        
        // Effacer le flag
        __HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_RXNE);
    }
    
    // Vérifier les erreurs
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE))
    {
        __HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_ORE);
    }
    
    // Appeler le handler HAL standard
    HAL_UART_IRQHandler(&huart1);
}

/**
 * @brief Gestionnaire SPI2 (Module LoRa)
 * 
 * Appelé par les événements SPI (DMA terminé, erreur).
 */
void SPI2_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
    irq_counter_lora++;
#endif
    
    // Appeler le handler HAL standard
    HAL_SPI_IRQHandler(&hspi2);
}

/**
 * @brief Gestionnaire I2C1 événements (Tactile)
 */
void I2C1_EV_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
#endif
    
    // Appeler le handler HAL standard
    HAL_I2C_EV_IRQHandler(&hi2c1);
}

/**
 * @brief Gestionnaire I2C1 erreurs (Tactile)
 */
void I2C1_ER_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
#endif
    
    HAL_I2C_ER_IRQHandler(&hi2c1);
}

// ============================================================
// SECTION 3 : INTERRUPTIONS AUDIO
// ============================================================

/**
 * @brief Gestionnaire ADC (Microphone)
 * 
 * Appelé quand l'ADC a terminé une conversion.
 * Utilisé pour la capture audio en continu.
 */
void ADC_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
    irq_counter_audio++;
#endif
    
    // Appeler le handler HAL
    HAL_ADC_IRQHandler(&hadc1);
}

/**
 * @brief Gestionnaire Timer 6 (Base de temps audio)
 * 
 * Appelé à 8 kHz pour l'échantillonnage audio.
 * Fréquence : 180 MHz / (ARR+1) = 8000 Hz
 */
void TIM6_DAC_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
    irq_counter_audio++;
#endif
    
    // Effacer le flag d'interruption
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    
    // Signaler à la boucle principale qu'un échantillon audio
    // doit être traité
    audio_buffer_ready = true;
}

/**
 * @brief Gestionnaire DMA2 Stream0 (ADC → Mémoire)
 * 
 * Appelé quand le buffer audio est plein.
 */
void DMA2_Stream0_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
    irq_counter_audio++;
#endif
    
    // Vérifier si transfert complet
    if (__HAL_DMA_GET_FLAG(&hdma_adc1, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_adc1)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_adc1, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_adc1));
        
        // Buffer audio plein, prêt à être envoyé via LoRa
        audio_buffer_ready = true;
    }
    
    // Vérifier si demi-transfert (double buffering)
    if (__HAL_DMA_GET_FLAG(&hdma_adc1, __HAL_DMA_GET_HT_FLAG_INDEX(&hdma_adc1)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_adc1, __HAL_DMA_GET_HT_FLAG_INDEX(&hdma_adc1));
        // Première moitié du buffer prête
    }
}

/**
 * @brief Gestionnaire DMA1 Stream5 (Mémoire → DAC)
 * 
 * Appelé quand le buffer audio a été joué.
 */
void DMA1_Stream5_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
    irq_counter_audio++;
#endif
    
    if (__HAL_DMA_GET_FLAG(&hdma_dac1, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_dac1)))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_dac1, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_dac1));
        audio_buffer_played = true;
    }
}

// ============================================================
// SECTION 4 : INTERRUPTIONS AFFICHAGE
// ============================================================

/**
 * @brief Gestionnaire LTDC (Écran TFT)
 * 
 * Appelé à chaque fin de ligne ou fin de trame.
 */
void LTDC_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
    irq_counter_display++;
#endif
    
    // Vérifier si interruption de ligne
    if (__HAL_LTDC_GET_FLAG(&hltdc, LTDC_FLAG_LI))
    {
        __HAL_LTDC_CLEAR_FLAG(&hltdc, LTDC_FLAG_LI);
        // Interruption de ligne (peut être utilisée pour le tear-free)
    }
    
    // Vérifier si interruption de trame (VSYNC)
    if (__HAL_LTDC_GET_FLAG(&hltdc, LTDC_FLAG_FU))
    {
        __HAL_LTDC_CLEAR_FLAG(&hltdc, LTDC_FLAG_FU);
        ltdc_frame_done = true;
    }
    
    // Appeler le handler HAL
    HAL_LTDC_IRQHandler(&hltdc);
}

/**
 * @brief Gestionnaire DMA2D (Accélérateur graphique)
 * 
 * Appelé quand un transfert DMA2D est terminé.
 */
void DMA2D_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
    irq_counter_display++;
#endif
    
    // Vérifier si transfert terminé
    if (__HAL_DMA2D_GET_FLAG(&hdma2d, DMA2D_FLAG_TC))
    {
        __HAL_DMA2D_CLEAR_FLAG(&hdma2d, DMA2D_FLAG_TC);
        dma2d_transfer_done = true;
    }
    
    // Vérifier erreurs
    if (__HAL_DMA2D_GET_FLAG(&hdma2d, DMA2D_FLAG_TE))
    {
        __HAL_DMA2D_CLEAR_FLAG(&hdma2d, DMA2D_FLAG_TE);
    }
    
    HAL_DMA2D_IRQHandler(&hdma2d);
}

// ============================================================
// SECTION 5 : INTERRUPTIONS GPIO (EXTI)
// ============================================================

/**
 * @brief Gestionnaire EXTI0 (Bouton utilisateur PA0)
 */
void EXTI0_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
#endif
    
    // Vérifier si c'est bien le bouton utilisateur
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_0) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
        
        // Anti-rebond simple (logiciel)
        static uint32_t last_press = 0;
        uint32_t now = HAL_GetTick();
        
        if (now - last_press > 50)  // 50 ms anti-rebond
        {
            last_press = now;
            user_button_pressed = true;
        }
    }
    
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

/**
 * @brief Gestionnaire EXTI1 (Tactile IRQ PB1)
 */
void EXTI1_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
#endif
    
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_1) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);
        touch_detected = true;
    }
    
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}

/**
 * @brief Gestionnaire EXTI2 (LoRa DIO0 PD2)
 * 
 * C'est l'interruption la plus importante pour LoRa.
 * DIO0 signale : paquet reçu, transmission terminée, timeout.
 */
void EXTI2_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
    irq_counter_lora++;
#endif
    
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_2) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2);
        
        // Lire les flags d'interruption du SX1278
        // pour savoir quel événement s'est produit
        uint8_t irq_flags = lora_read_register(REG_IRQ_FLAGS);
        
        if (irq_flags & IRQ_RX_DONE_MASK)
        {
            // Paquet reçu !
            lora_packet_received = true;
        }
        
        if (irq_flags & IRQ_TX_DONE_MASK)
        {
            // Transmission terminée
            lora_transmission_done = true;
        }
        
        // Effacer les flags du SX1278
        lora_write_register(REG_IRQ_FLAGS, 0xFF);
    }
    
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2);
}

/**
 * @brief Gestionnaire EXTI3 (LoRa DIO1 PD3 - optionnel)
 */
void EXTI3_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
    irq_counter_lora++;
#endif
    
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_3) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);
        // DIO1 : utilisé pour d'autres événements LoRa
    }
    
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
}

/**
 * @brief Gestionnaire EXTI4 (LoRa DIO2 PD4 - optionnel)
 */
void EXTI4_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
#endif
    
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_4) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);
    }
    
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);
}

/**
 * @brief Gestionnaire EXTI9_5 (Touches clavier, etc.)
 */
void EXTI9_5_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
#endif
    
    // Vérifier chaque pin de cette plage
    for (uint8_t pin = 5; pin <= 9; pin++)
    {
        if (__HAL_GPIO_EXTI_GET_IT(1 << pin) != RESET)
        {
            __HAL_GPIO_EXTI_CLEAR_IT(1 << pin);
            // Traitement selon le pin
        }
    }
    
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
}

/**
 * @brief Gestionnaire EXTI15_10
 */
void EXTI15_10_IRQHandler(void)
{
#ifdef DEBUG
    irq_counter_total++;
#endif
    
    for (uint8_t pin = 10; pin <= 15; pin++)
    {
        if (__HAL_GPIO_EXTI_GET_IT(1 << pin) != RESET)
        {
            __HAL_GPIO_EXTI_CLEAR_IT(1 << pin);
        }
    }
    
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | 
                             GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
}

// ============================================================
// SECTION 6 : FONCTIONS DE RAPPEL (CALLBACKS) DU HAL
// ============================================================

/**
 * @brief Callback de fin de transmission SPI
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI2)
    {
        // Transmission SPI2 (LoRa) terminée
        lora_transmission_done = true;
    }
}

/**
 * @brief Callback de fin de réception SPI
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI2)
    {
        // Réception SPI2 (LoRa) terminée
        lora_packet_received = true;
    }
}

/**
 * @brief Callback d'erreur SPI
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    // Erreur de communication SPI
    // Logger l'erreur et réinitialiser si nécessaire
    if (hspi->Instance == SPI2)
    {
        // Erreur LoRa - tenter de réinitialiser
        HAL_SPI_Abort(hspi);
    }
}

/**
 * @brief Callback de fin de conversion ADC (Micro)
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        // Une conversion ADC est terminée
        // Utilisé pour la capture audio
    }
}

/**
 * @brief Callback période Timer écoulée
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        // Timer audio (8 kHz)
        audio_buffer_ready = true;
    }
    else if (htim->Instance == TIM4)
    {
        // Timer backlight clavier
    }
}

/**
 * @brief Callback interruption GPIO (EXTI)
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    switch (GPIO_Pin)
    {
        case GPIO_PIN_0:
            user_button_pressed = true;
            break;
            
        case GPIO_PIN_1:
            touch_detected = true;
            break;
            
        case GPIO_PIN_2:
            // LoRa DIO0
            break;
            
        default:
            break;
    }
}

/**
 * @brief Callback fin de trame LTDC
 */
void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *hltdc)
{
    ltdc_frame_done = true;
}

/**
 * @brief Callback transfert DMA2D terminé
 */
void HAL_DMA2D_TransferCpltCallback(DMA2D_HandleTypeDef *hdma2d)
{
    dma2d_transfer_done = true;
}

/**
 * @brief Callback réception UART
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uart_data_received = true;
    }
}

/**
 * @brief Callback erreur UART
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    // Erreur de communication UART
    if (huart->Instance == USART1)
    {
        // Réactiver la réception
        HAL_UART_Receive_IT(huart, (uint8_t*)&uart_rx_char, 1);
    }
}

// ============================================================
// SECTION 7 : TRAITEMENT DIFFÉRÉ (DEFERRED PROCESSING)
// ============================================================

/**
 * @brief Traite les flags positionnés par les IRQ
 * 
 * Cette fonction est appelée depuis la boucle principale.
 * Elle lit les flags volatils et appelle les fonctions
 * de traitement appropriées.
 * 
 * ⚠️ Ne PAS appeler depuis une IRQ !
 */
void process_interrupt_flags(void)
{
    // --- LoRa ---
    if (lora_packet_received)
    {
        lora_packet_received = false;
        // Traiter le paquet reçu (appeler le gestionnaire LoRa)
        extern void lora_handle_received_packet(void);
        lora_handle_received_packet();
    }
    
    if (lora_transmission_done)
    {
        lora_transmission_done = false;
        // Transmission terminée
        extern void lora_handle_transmission_done(void);
        lora_handle_transmission_done();
    }
    
    // --- Audio ---
    if (audio_buffer_ready)
    {
        audio_buffer_ready = false;
        // Envoyer le buffer audio via LoRa
        extern void audio_handle_buffer_ready(void);
        audio_handle_buffer_ready();
    }
    
    if (audio_buffer_played)
    {
        audio_buffer_played = false;
        // Préparer le prochain buffer audio
        extern void audio_handle_buffer_played(void);
        audio_handle_buffer_played();
    }
    
    // --- Tactile ---
    if (touch_detected)
    {
        touch_detected = false;
        // Lire les coordonnées tactiles
        extern void touch_handle_detection(void);
        touch_handle_detection();
    }
    
    // --- Bouton utilisateur ---
    if (user_button_pressed)
    {
        user_button_pressed = false;
        // Traiter l'appui bouton
        extern void button_handle_press(void);
        button_handle_press();
    }
    
    // --- Affichage ---
    if (dma2d_transfer_done)
    {
        dma2d_transfer_done = true;  // Reste à true jusqu'à nouvelle demande
        // Le DMA2D est prêt pour un nouveau transfert
    }
    
    // --- UART ---
    if (uart_data_received)
    {
        uart_data_received = false;
        // Traiter le caractère reçu (console série)
        extern void uart_handle_char(uint8_t ch);
        uart_handle_char(uart_rx_char);
    }
}

/**
 * @brief Réinitialise tous les compteurs de debug
 */
#ifdef DEBUG
void reset_irq_counters(void)
{
    irq_counter_total = 0;
    irq_counter_systick = 0;
    irq_counter_lora = 0;
    irq_counter_audio = 0;
    irq_counter_display = 0;
    irq_counter_uart = 0;
}

/**
 * @brief Affiche les statistiques d'interruptions
 */
void print_irq_statistics(void)
{
    printf("═══ STATISTIQUES INTERRUPTIONS ═══\n");
    printf("Total IRQ:     %lu\n", irq_counter_total);
    printf("SysTick:       %lu (%.1f%%)\n", 
           irq_counter_systick,
           100.0f * irq_counter_systick / irq_counter_total);
    printf("LoRa:          %lu (%.1f%%)\n",
           irq_counter_lora,
           100.0f * irq_counter_lora / irq_counter_total);
    printf("Audio:         %lu (%.1f%%)\n",
           irq_counter_audio,
           100.0f * irq_counter_audio / irq_counter_total);
    printf("Affichage:     %lu (%.1f%%)\n",
           irq_counter_display,
           100.0f * irq_counter_display / irq_counter_total);
    printf("UART:          %lu (%.1f%%)\n",
           irq_counter_uart,
           100.0f * irq_counter_uart / irq_counter_total);
    printf("══════════════════════════════════\n");
}
#endif