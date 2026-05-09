/**
 * @file ili9488_driver.cpp
 * @brief Implémentation du driver bas niveau ILI9488
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans ili9488_driver.h.
 * 
 * Il gère :
 * - L'initialisation de l'écran
 * - La communication via le bus parallèle 16-bit
 * - L'envoi de commandes et de données
 * - Les fonctions de dessin bas niveau
 * - La gestion de la luminosité et du mode veille
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ili9488_driver.h"
#include "ili9488_defs.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Informations du contrôleur */
static ILI9488_Info ili9488_info = {
    .controllerType = ILI9488_CONTROLLER_UNKNOWN,
    .deviceId = 0,
    .madctl = ILI9488_ROTATION_PORTRAIT,
    .pixelFormat = ILI9488_DPI_16BPP,
    .activeWidth = ILI9488_WIDTH,
    .activeHeight = ILI9488_HEIGHT,
    .initialized = false,
    .displayOn = false,
    .sleeping = true,
    .brightness = 255,
    .rotation = ILI9488_ROTATION_PORTRAIT,
    .frameCount = 0
};

/** @brief Luminosité actuelle */
static uint8_t current_brightness = 255;

/** @brief Buffer temporaire pour les données */
static uint16_t data_buffer[ILI9488_WIDTH];  // Une ligne complète

// ============================================================
// SECTION 1 : SÉQUENCE D'INITIALISATION
// ============================================================

/**
 * @brief Données de la séquence d'initialisation
 * 
 * Cette séquence est basée sur les recommandations du fabricant
 * pour un écran ILI9488 en mode 16-bit parallèle.
 */
static const uint8_t init_data_1[] = { 0x03, 0x03, 0x03 };
static const uint8_t init_data_2[] = { 0x0D, 0x0D };
static const uint8_t init_data_3[] = { 0x19 };
static const uint8_t init_data_4[] = { 0x41 };
static const uint8_t init_data_5[] = { 0x00, 0x1A };
static const uint8_t init_data_6[] = { 0x00, 0x26, 0x09 };
static const uint8_t init_data_7[] = { 0x2F };
static const uint8_t init_data_8[] = { 0x11 };
static const uint8_t init_data_9[] = { 0x55 };
static const uint8_t init_data_10[] = { 0x00, 0x18 };
static const uint8_t init_data_11[] = { 0x0A };
static const uint8_t init_data_12[] = { 0x01 };
static const uint8_t init_data_13[] = { 0x15, 0x02 };

// Gamma positif
static const uint8_t gamma_positive[] = {
    0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
    0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F
};

// Gamma négatif
static const uint8_t gamma_negative[] = {
    0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
    0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F
};

/**
 * @brief Séquence d'initialisation complète
 */
const ILI9488_InitCommand ili9488_init_sequence[] = {
    // { Commande, Longueur données, Données, Délai ms }
    
    // 1. Reset logiciel
    { ILI9488_SOFT_RESET, 0, NULL, 120 },
    
    // 2. Configuration puissance 1
    { ILI9488_POWER_CONTROL_1, 3, init_data_1, 0 },
    
    // 3. Configuration puissance 2
    { ILI9488_POWER_CONTROL_2, 2, init_data_2, 0 },
    
    // 4. Configuration VCOM 1
    { ILI9488_VCOM_CONTROL_1, 1, init_data_3, 0 },
    
    // 5. Configuration VCOM 2
    { ILI9488_VCOM_CONTROL_2, 1, init_data_4, 0 },
    
    // 6. Configuration interface
    { ILI9488_INTERFACE_CONTROL, 2, init_data_5, 0 },
    
    // 7. Configuration timing driver
    { ILI9488_DRIVER_TIMING_CONTROL, 3, init_data_6, 0 },
    
    // 8. Configuration ratio pompe
    { ILI9488_PUMP_RATIO_CONTROL, 1, init_data_7, 0 },
    
    // 9. Configuration sortie driver
    { ILI9488_DRIVER_OUTPUT_CONTROL, 1, init_data_8, 0 },
    
    // 10. Format pixel : 16 bpp
    { ILI9488_PIXEL_FORMAT_SET, 1, init_data_9, 0 },
    
    // 11. Configuration mémoire
    { ILI9488_MEMORY_ACCESS_CONTROL, 2, init_data_10, 0 },
    
    // 12. Sortie de veille
    { ILI9488_SLEEP_OUT, 0, NULL, 120 },
    
    // 13. Gamma positif
    { ILI9488_POSITIVE_GAMMA_CORRECT, 15, gamma_positive, 0 },
    
    // 14. Gamma négatif
    { ILI9488_NEGATIVE_GAMMA_CORRECT, 15, gamma_negative, 0 },
    
    // 15. Contrôle affichage
    { ILI9488_WRITE_CTRL_DISPLAY, 1, init_data_11, 0 },
    
    // 16. Contrôle adaptatif
    { ILI9488_WRITE_CONTENT_ADAPTIVE, 1, init_data_12, 0 },
    
    // 17. Luminosité min CABC
    { ILI9488_WRITE_CABC_MIN_BRIGHTNESS, 2, init_data_13, 0 },
    
    // 18. Allumer l'écran
    { ILI9488_DISPLAY_ON, 0, NULL, 50 },
};

/** @brief Taille de la séquence */
const uint8_t ili9488_init_sequence_size = sizeof(ili9488_init_sequence) / sizeof(ILI9488_InitCommand);

// ============================================================
// SECTION 2 : FONCTIONS DE CONTRÔLE BAS NIVEAU (BUS PARALLÈLE)
// ============================================================

/**
 * @brief Configure les broches du bus de données en sortie
 */
static void ili9488_data_bus_output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    
    // Configurer toutes les broches de données en sortie
    GPIO_InitStruct.Pin = TFT_D0_PIN;
    HAL_GPIO_Init(TFT_D0_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D1_PIN;
    HAL_GPIO_Init(TFT_D1_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D2_PIN;
    HAL_GPIO_Init(TFT_D2_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D3_PIN;
    HAL_GPIO_Init(TFT_D3_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D4_PIN;
    HAL_GPIO_Init(TFT_D4_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D5_PIN;
    HAL_GPIO_Init(TFT_D5_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D6_PIN;
    HAL_GPIO_Init(TFT_D6_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D7_PIN;
    HAL_GPIO_Init(TFT_D7_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D8_PIN;
    HAL_GPIO_Init(TFT_D8_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D9_PIN;
    HAL_GPIO_Init(TFT_D9_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D10_PIN;
    HAL_GPIO_Init(TFT_D10_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D11_PIN;
    HAL_GPIO_Init(TFT_D11_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D12_PIN;
    HAL_GPIO_Init(TFT_D12_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D13_PIN;
    HAL_GPIO_Init(TFT_D13_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D14_PIN;
    HAL_GPIO_Init(TFT_D14_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D15_PIN;
    HAL_GPIO_Init(TFT_D15_PORT, &GPIO_InitStruct);
}

/**
 * @brief Configure les broches du bus de données en entrée
 */
static void ili9488_data_bus_input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    
    GPIO_InitStruct.Pin = TFT_D0_PIN;
    HAL_GPIO_Init(TFT_D0_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D1_PIN;
    HAL_GPIO_Init(TFT_D1_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D2_PIN;
    HAL_GPIO_Init(TFT_D2_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D3_PIN;
    HAL_GPIO_Init(TFT_D3_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D4_PIN;
    HAL_GPIO_Init(TFT_D4_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D5_PIN;
    HAL_GPIO_Init(TFT_D5_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D6_PIN;
    HAL_GPIO_Init(TFT_D6_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D7_PIN;
    HAL_GPIO_Init(TFT_D7_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D8_PIN;
    HAL_GPIO_Init(TFT_D8_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D9_PIN;
    HAL_GPIO_Init(TFT_D9_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D10_PIN;
    HAL_GPIO_Init(TFT_D10_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D11_PIN;
    HAL_GPIO_Init(TFT_D11_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D12_PIN;
    HAL_GPIO_Init(TFT_D12_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D13_PIN;
    HAL_GPIO_Init(TFT_D13_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D14_PIN;
    HAL_GPIO_Init(TFT_D14_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = TFT_D15_PIN;
    HAL_GPIO_Init(TFT_D15_PORT, &GPIO_InitStruct);
}

/**
 * @brief Écrit une valeur 16 bits sur le bus de données
 * @param value Valeur 16 bits à écrire
 */
static void ili9488_write_bus(uint16_t value)
{
    // Écrire les 16 bits sur les broches D0-D15
    // Cette fonction doit être optimisée pour la vitesse
    
    // Méthode directe par registre (plus rapide que HAL_GPIO_WritePin)
    // On écrit sur les ports 8 bits par 8 bits
    
    // Octet de poids faible (D0-D7)
    uint8_t low = value & 0xFF;
    // Octet de poids fort (D8-D15)
    uint8_t high = (value >> 8) & 0xFF;
    
    // Écriture sur les ports (selon le câblage réel)
    // Exemple : D0-D7 sur GPIOD, D8-D15 sur GPIOE
    GPIOD->ODR = (GPIOD->ODR & 0x00FF) | (low << 8);   // D0-D7 sur PD8-PD15
    GPIOE->ODR = (GPIOE->ODR & 0x00FF) | (high << 8);   // D8-D15 sur PE8-PE15
}

/**
 * @brief Lit une valeur 16 bits du bus de données
 * @return uint16_t Valeur lue
 */
static uint16_t ili9488_read_bus(void)
{
    uint16_t value = 0;
    
    // Lire les 16 bits depuis les broches D0-D15
    uint8_t low = (GPIOD->IDR >> 8) & 0xFF;   // D0-D7 sur PD8-PD15
    uint8_t high = (GPIOE->IDR >> 8) & 0xFF;   // D8-D15 sur PE8-PE15
    
    value = ((uint16_t)high << 8) | low;
    
    return value;
}

// ============================================================
// SECTION 3 : FONCTIONS DE COMMANDES/DONNÉES
// ============================================================

/**
 * @brief Émet une commande vers l'écran
 */
void ili9488_write_command(uint8_t cmd)
{
    ILI9488_DEBUG_CMD(cmd);
    
    ILI9488_CS_LOW();           // Sélectionner l'écran
    ILI9488_RS_COMMAND();       // Mode commande (RS = LOW)
    
    ili9488_data_bus_output();   // Bus en sortie
    ili9488_write_bus(cmd);      // Écrire la commande
    ILI9488_WR_PULSE();          // Strobe Write
    
    ILI9488_CS_HIGH();           // Désélectionner
}

/**
 * @brief Émet une donnée 8 bits vers l'écran
 */
void ili9488_write_data(uint8_t data)
{
    ILI9488_CS_LOW();           // Sélectionner
    ILI9488_RS_DATA();          // Mode données (RS = HIGH)
    
    ili9488_data_bus_output();   // Bus en sortie
    ili9488_write_bus(data);     // Écrire la donnée (poids faible)
    ILI9488_WR_PULSE();          // Strobe Write
    
    ILI9488_CS_HIGH();           // Désélectionner
}

/**
 * @brief Émet une donnée 16 bits vers l'écran
 */
void ili9488_write_data16(uint16_t data)
{
    ILI9488_CS_LOW();
    ILI9488_RS_DATA();
    
    ili9488_data_bus_output();
    ili9488_write_bus(data);
    ILI9488_WR_PULSE();
    
    ILI9488_CS_HIGH();
}

/**
 * @brief Émet un pixel (couleur RGB565)
 */
void ili9488_write_pixel(uint16_t color)
{
    ili9488_write_data16(color);
}

/**
 * @brief Émet plusieurs pixels en mode burst
 */
void ili9488_write_pixels(uint16_t* colors, uint32_t count)
{
    ILI9488_CS_LOW();
    ILI9488_RS_DATA();
    ili9488_data_bus_output();
    
    for (uint32_t i = 0; i < count; i++)
    {
        ili9488_write_bus(colors[i]);
        ILI9488_WR_PULSE();
    }
    
    ILI9488_CS_HIGH();
}

/**
 * @brief Lit l'identifiant du contrôleur
 */
uint32_t ili9488_read_id(void)
{
    uint32_t id = 0;
    
    ILI9488_CS_LOW();
    ILI9488_RS_COMMAND();
    ili9488_data_bus_output();
    ili9488_write_bus(ILI9488_READ_ID);
    ILI9488_WR_PULSE();
    
    // Passer en lecture
    ili9488_data_bus_input();
    ILI9488_RS_DATA();
    
    // Lire 3 octets d'ID
    for (int i = 0; i < 3; i++)
    {
        ILI9488_RD_LOW();
        __NOP();
        id = (id << 8) | (ili9488_read_bus() & 0xFF);
        ILI9488_RD_HIGH();
    }
    
    ILI9488_CS_HIGH();
    
    ILI9488_DEBUG("ID lu: 0x%08lX\n", (unsigned long)id);
    
    return id;
}

/**
 * @brief Lit un registre de paramètre
 */
uint8_t ili9488_read_register(uint8_t cmd)
{
    uint8_t value = 0;
    
    ILI9488_CS_LOW();
    ILI9488_RS_COMMAND();
    ili9488_data_bus_output();
    ili9488_write_bus(cmd);
    ILI9488_WR_PULSE();
    
    ili9488_data_bus_input();
    ILI9488_RS_DATA();
    
    ILI9488_RD_LOW();
    __NOP();
    value = ili9488_read_bus() & 0xFF;
    ILI9488_RD_HIGH();
    
    ILI9488_CS_HIGH();
    
    return value;
}

// ============================================================
// SECTION 4 : INITIALISATION
// ============================================================

/**
 * @brief Configure les broches de contrôle
 */
static void ili9488_control_pins_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Activer les horloges des ports
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    // Configuration commune pour les broches de contrôle
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    
    // CS (Chip Select)
    GPIO_InitStruct.Pin = TFT_CS_PIN;
    HAL_GPIO_Init(TFT_CS_PORT, &GPIO_InitStruct);
    
    // RS (Register Select / D/C)
    GPIO_InitStruct.Pin = TFT_RS_PIN;
    HAL_GPIO_Init(TFT_RS_PORT, &GPIO_InitStruct);
    
    // WR (Write Strobe)
    GPIO_InitStruct.Pin = TFT_WR_PIN;
    HAL_GPIO_Init(TFT_WR_PORT, &GPIO_InitStruct);
    
    // RD (Read Strobe)
    GPIO_InitStruct.Pin = TFT_RD_PIN;
    HAL_GPIO_Init(TFT_RD_PORT, &GPIO_InitStruct);
    
    // Reset
    GPIO_InitStruct.Pin = TFT_RST_PIN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TFT_RST_PORT, &GPIO_InitStruct);
    
    // Backlight (PWM)
    GPIO_InitStruct.Pin = TFT_BL_PIN;
    HAL_GPIO_Init(TFT_BL_PORT, &GPIO_InitStruct);
    
    // État initial
    ILI9488_CS_HIGH();
    ILI9488_WR_HIGH();
    ILI9488_RD_HIGH();
    ILI9488_RESET_HIGH();
    ILI9488_BL_OFF();
}

/**
 * @brief Réinitialise l'écran (matériel)
 */
void ili9488_reset(void)
{
    ILI9488_DEBUG("Reset matériel\n");
    
    ILI9488_RESET_LOW();
    HAL_Delay(ILI9488_RESET_DELAY_MS);
    ILI9488_RESET_HIGH();
    HAL_Delay(ILI9488_RESET_DELAY_MS);
    
    ili9488_info.initialized = false;
    ili9488_info.displayOn = false;
}

/**
 * @brief Initialise l'écran ILI9488
 */
bool ili9488_init(void)
{
    ILI9488_DEBUG("Initialisation ILI9488...\n");
    
    // 1. Initialiser les broches de contrôle
    ili9488_control_pins_init();
    
    // 2. Configurer le bus de données en sortie
    ili9488_data_bus_output();
    
    // 3. Reset matériel
    ili9488_reset();
    
    // 4. Lire l'ID du contrôleur
    uint32_t id = 0;
    for (int i = 0; i < ILI9488_ID_READ_RETRIES; i++)
    {
        id = ili9488_read_id();
        if (id == ILI9488_ID_EXPECTED || id == ILI9486_ID_EXPECTED || id == ILI9481_ID_EXPECTED)
        {
            break;
        }
        HAL_Delay(10);
    }
    
    // Déterminer le type de contrôleur
    if (id == ILI9488_ID_EXPECTED)
        ili9488_info.controllerType = ILI9488_CONTROLLER_9488;
    else if (id == ILI9486_ID_EXPECTED)
        ili9488_info.controllerType = ILI9488_CONTROLLER_9486;
    else if (id == ILI9481_ID_EXPECTED)
        ili9488_info.controllerType = ILI9488_CONTROLLER_9481;
    else
    {
        ILI9488_DEBUG("ID contrôleur inconnu: 0x%08lX\n", (unsigned long)id);
        // Continuer quand même, certains écrans ont des ID différents
        ili9488_info.controllerType = ILI9488_CONTROLLER_9488;  // Supposer ILI9488
    }
    
    ili9488_info.deviceId = id;
    
    // 5. Exécuter la séquence d'initialisation
    ILI9488_DEBUG("Exécution séquence d'init (%d commandes)\n", ili9488_init_sequence_size);
    
    for (uint8_t i = 0; i < ili9488_init_sequence_size; i++)
    {
        const ILI9488_InitCommand* cmd = &ili9488_init_sequence[i];
        
        // Envoyer la commande
        ili9488_write_command(cmd->command);
        
        // Envoyer les données si nécessaire
        if (cmd->dataLength > 0 && cmd->data != NULL)
        {
            for (uint8_t d = 0; d < cmd->dataLength; d++)
            {
                ili9488_write_data(cmd->data[d]);
            }
        }
        
        // Attendre le délai si nécessaire
        if (cmd->delayMs > 0)
        {
            HAL_Delay(cmd->delayMs);
        }
    }
    
    // 6. Configuration par défaut
    ili9488_set_rotation(ILI9488_ROTATION_PORTRAIT);
    ili9488_set_brightness(255);
    ili9488_info.initialized = true;
    ili9488_info.displayOn = true;
    
    ILI9488_DEBUG("Initialisation terminée\n");
    
    return true;
}

/**
 * @brief Vérifie si l'écran est prêt
 */
bool ili9488_is_ready(void)
{
    return ili9488_info.initialized;
}

/**
 * @brief Récupère les informations du contrôleur
 */
ILI9488_Info* ili9488_get_info(void)
{
    return &ili9488_info;
}

// ============================================================
// SECTION 5 : FONCTIONS DE CONTRÔLE D'AFFICHAGE
// ============================================================

/**
 * @brief Allume l'écran
 */
void ili9488_display_on(void)
{
    ili9488_write_command(ILI9488_DISPLAY_ON);
    ili9488_info.displayOn = true;
    ILI9488_BL_ON();
}

/**
 * @brief Éteint l'écran
 */
void ili9488_display_off(void)
{
    ili9488_write_command(ILI9488_DISPLAY_OFF);
    ili9488_info.displayOn = false;
    ILI9488_BL_OFF();
}

/**
 * @brief Met l'écran en veille
 */
void ili9488_sleep_in(void)
{
    ili9488_write_command(ILI9488_ENTER_SLEEP);
    ili9488_info.sleeping = true;
    HAL_Delay(5);
}

/**
 * @brief Sort l'écran de veille
 */
void ili9488_sleep_out(void)
{
    ili9488_write_command(ILI9488_SLEEP_OUT);
    ili9488_info.sleeping = false;
    HAL_Delay(ILI9488_SLEEP_OUT_DELAY_MS);
}

/**
 * @brief Définit la rotation de l'écran
 */
void ili9488_set_rotation(ILI9488_Rotation rotation)
{
    ili9488_write_command(ILI9488_MEMORY_ACCESS_CONTROL);
    ili9488_write_data(rotation);
    
    ili9488_info.madctl = rotation;
    ili9488_info.rotation = rotation;
    
    // Mettre à jour largeur/hauteur selon la rotation
    if (rotation == ILI9488_ROTATION_PORTRAIT || 
        rotation == ILI9488_ROTATION_PORTRAIT_INVERTED)
    {
        ili9488_info.activeWidth = ILI9488_WIDTH;
        ili9488_info.activeHeight = ILI9488_HEIGHT;
    }
    else
    {
        ili9488_info.activeWidth = ILI9488_HEIGHT;
        ili9488_info.activeHeight = ILI9488_WIDTH;
    }
}

/**
 * @brief Récupère la rotation actuelle
 */
ILI9488_Rotation ili9488_get_rotation(void)
{
    return ili9488_info.rotation;
}

/**
 * @brief Définit la luminosité
 */
void ili9488_set_brightness(uint8_t brightness)
{
    current_brightness = brightness;
    ili9488_info.brightness = brightness;
    
    // Utiliser le PWM pour le rétroéclairage
    __HAL_TIM_SET_COMPARE(&htim9, TFT_BL_TIMER_CHANNEL, brightness);
}

/**
 * @brief Récupère la luminosité
 */
uint8_t ili9488_get_brightness(void)
{
    return current_brightness;
}

/**
 * @brief Inverse les couleurs
 */
void ili9488_set_invert(bool invert)
{
    ili9488_write_command(invert ? ILI9488_DISPLAY_INVERSION_ON : ILI9488_DISPLAY_INVERSION_OFF);
}

// ============================================================
// SECTION 6 : FONCTIONS DE DESSIN BAS NIVEAU
// ============================================================

/**
 * @brief Définit la zone de dessin (fenêtre)
 */
void ili9488_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    // Limiter aux dimensions de l'écran
    if (x1 >= ILI9488_WIDTH) x1 = ILI9488_WIDTH - 1;
    if (x2 >= ILI9488_WIDTH) x2 = ILI9488_WIDTH - 1;
    if (y1 >= ILI9488_HEIGHT) y1 = ILI9488_HEIGHT - 1;
    if (y2 >= ILI9488_HEIGHT) y2 = ILI9488_HEIGHT - 1;
    
    // Définir la plage de colonnes (X)
    ili9488_write_command(ILI9488_COLUMN_ADDRESS_SET);
    ili9488_write_data16(x1);
    ili9488_write_data16(x2);
    
    // Définir la plage de lignes (Y)
    ili9488_write_command(ILI9488_PAGE_ADDRESS_SET);
    ili9488_write_data16(y1);
    ili9488_write_data16(y2);
    
    // Prêt pour l'écriture mémoire
    ili9488_write_command(ILI9488_MEMORY_WRITE);
}

/**
 * @brief Remplit une zone rectangulaire
 */
void ili9488_fill_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint32_t pixel_count = (uint32_t)(x2 - x1 + 1) * (y2 - y1 + 1);
    
    ili9488_set_window(x1, y1, x2, y2);
    
    ILI9488_CS_LOW();
    ILI9488_RS_DATA();
    ili9488_data_bus_output();
    
    // Remplir avec la couleur (mode burst)
    for (uint32_t i = 0; i < pixel_count; i++)
    {
        ili9488_write_bus(color);
        ILI9488_WR_PULSE();
    }
    
    ILI9488_CS_HIGH();
}

/**
 * @brief Remplit tout l'écran
 */
void ili9488_fill_screen(uint16_t color)
{
    ili9488_fill_rect(0, 0, ILI9488_WIDTH - 1, ILI9488_HEIGHT - 1, color);
}

/**
 * @brief Dessine un pixel
 */
void ili9488_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= ILI9488_WIDTH || y >= ILI9488_HEIGHT) return;
    
    ili9488_set_window(x, y, x, y);
    ili9488_write_data16(color);
}

/**
 * @brief Lit un pixel
 */
uint16_t ili9488_read_pixel(uint16_t x, uint16_t y)
{
    uint16_t color = 0;
    
    if (x >= ILI9488_WIDTH || y >= ILI9488_HEIGHT) return 0;
    
    ili9488_set_window(x, y, x, y);
    
    // Envoyer la commande de lecture
    ili9488_write_command(ILI9488_MEMORY_READ);
    
    // Premier octet factice (dummy read)
    ILI9488_CS_LOW();
    ILI9488_RS_DATA();
    ili9488_data_bus_input();
    ILI9488_RD_LOW();
    __NOP();
    ili9488_read_bus();  // Dummy
    ILI9488_RD_HIGH();
    
    // Lire la vraie valeur
    ILI9488_RD_LOW();
    __NOP();
    color = ili9488_read_bus();
    ILI9488_RD_HIGH();
    
    ILI9488_CS_HIGH();
    
    return color;
}

/**
 * @brief Écrit directement dans la mémoire vidéo
 */
void ili9488_write_memory(uint16_t* data, uint32_t length)
{
    ILI9488_CS_LOW();
    ILI9488_RS_DATA();
    ili9488_data_bus_output();
    
    for (uint32_t i = 0; i < length; i++)
    {
        ili9488_write_bus(data[i]);
        ILI9488_WR_PULSE();
    }
    
    ILI9488_CS_HIGH();
}

/**
 * @brief Lit directement la mémoire vidéo
 */
void ili9488_read_memory(uint16_t* data, uint32_t length)
{
    ILI9488_CS_LOW();
    ILI9488_RS_DATA();
    ili9488_data_bus_input();
    
    // Premier octet factice
    ILI9488_RD_LOW();
    __NOP();
    ili9488_read_bus();
    ILI9488_RD_HIGH();
    
    for (uint32_t i = 0; i < length; i++)
    {
        ILI9488_RD_LOW();
        __NOP();
        data[i] = ili9488_read_bus();
        ILI9488_RD_HIGH();
    }
    
    ILI9488_CS_HIGH();
}

// ============================================================
// SECTION 7 : SCROLLING
// ============================================================

/**
 * @brief Définit la zone de scrolling
 */
void ili9488_set_scroll_area(uint16_t top, uint16_t scroll, uint16_t bottom)
{
    ili9488_write_command(ILI9488_VERT_SCROLL_DEFINITION);
    ili9488_write_data16(top);
    ili9488_write_data16(scroll);
    ili9488_write_data16(bottom);
}

/**
 * @brief Définit le décalage de scrolling
 */
void ili9488_scroll(uint16_t offset)
{
    ili9488_write_command(ILI9488_VERT_SCROLL_START_ADDR);
    ili9488_write_data16(offset);
}

// ============================================================
// SECTION 8 : DIAGNOSTIC
// ============================================================

/**
 * @brief Vérifie l'état de l'écran
 */
uint32_t ili9488_get_status(void)
{
    return ili9488_read_register(ILI9488_READ_STATUS);
}

/**
 * @brief Test d'auto-diagnostic
 */
bool ili9488_self_test(void)
{
    uint32_t status = ili9488_get_status();
    ILI9488_DEBUG("Statut: 0x%08lX\n", (unsigned long)status);
    return (status != 0);
}

/**
 * @brief Affiche les informations du contrôleur
 */
void ili9488_print_info(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║       INFORMATIONS ÉCRAN ILI9488         ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Driver Version : %-23s ║\n", ILI9488_DRIVER_VERSION);
    printf("║ Contrôleur     : ");
    switch (ili9488_info.controllerType)
    {
        case ILI9488_CONTROLLER_9488: printf("ILI9488                  ║\n"); break;
        case ILI9488_CONTROLLER_9486: printf("ILI9486                  ║\n"); break;
        case ILI9488_CONTROLLER_9481: printf("ILI9481                  ║\n"); break;
        default:                      printf("Inconnu                  ║\n"); break;
    }
    printf("║ Device ID      : 0x%04lX                    ║\n", (unsigned long)ili9488_info.deviceId);
    printf("║ Résolution     : %d × %d               ║\n", ili9488_info.activeWidth, ili9488_info.activeHeight);
    printf("║ Format Pixel   : %d bpp                   ║\n", ili9488_info.pixelFormat == ILI9488_DPI_16BPP ? 16 : 18);
    printf("║ Rotation       : %d°                     ║\n", ili9488_info.rotation == ILI9488_ROTATION_PORTRAIT ? 0 : 90);
    printf("║ Luminosité     : %d/255                  ║\n", ili9488_info.brightness);
    printf("║ État           : %s                      ║\n", ili9488_info.displayOn ? "Allumé" : "Éteint");
    printf("║ Veille         : %s                      ║\n", ili9488_info.sleeping ? "Oui" : "Non");
    printf("║ Initialisé     : %s                      ║\n", ili9488_info.initialized ? "Oui" : "Non");
    printf("╚══════════════════════════════════════════╝\n\n");
}

/**
 * @brief Affiche un motif de test
 */
void ili9488_test_pattern(void)
{
    ILI9488_DEBUG("Affichage motif de test\n");
    
    // Bandes de couleurs horizontales
    uint16_t colors[] = {
        ILI9488_RED, ILI9488_GREEN, ILI9488_BLUE,
        ILI9488_YELLOW, ILI9488_CYAN, ILI9488_MAGENTA,
        ILI9488_WHITE, ILI9488_BLACK
    };
    
    uint16_t band_height = ILI9488_HEIGHT / 8;
    
    for (int i = 0; i < 8; i++)
    {
        ili9488_fill_rect(0, i * band_height, 
                          ILI9488_WIDTH - 1, (i + 1) * band_height - 1, 
                          colors[i]);
    }
    
    // Rectangle blanc au centre
    ili9488_fill_rect(60, 140, 260, 340, ILI9488_WHITE);
    
    // Texte de test centré
    // (Nécessite les fonctions de texte pour afficher)
}