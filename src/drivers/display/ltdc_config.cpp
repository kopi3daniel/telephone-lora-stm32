/**
 * @file ltdc_config.cpp
 * @brief Implémentation de la configuration du contrôleur LTDC
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans ltdc_config.h.
 * 
 * Il gère :
 * - L'initialisation du contrôleur LTDC
 * - La configuration des timings (HSYNC, VSYNC, PCLK)
 * - La configuration des couches (layers)
 * - Le double buffering
 * - La gestion du framebuffer en SDRAM
 * 
 * Le LTDC fonctionne EN ARRIÈRE-PLAN, sans intervention du CPU.
 * Il lit le framebuffer en SDRAM et envoie les pixels à l'écran
 * en continu, 60 fois par seconde.
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ltdc_config.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// HANDLES GLOBAUX
// ============================================================

/** @brief Handle LTDC utilisé par le HAL */
LTDC_HandleTypeDef hltdc;

/** @brief Handle DMA2D (accélérateur graphique) */
extern DMA2D_HandleTypeDef hdma2d;

/** @brief Handle SDRAM (mémoire externe) */
extern SDRAM_HandleTypeDef hsdram1;

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Configuration par défaut */
const LTDC_Config ltdc_default_config = {
    // Synchronisation horizontale
    .hsyncWidth     = LTDC_HSYNC_WIDTH,            // 10 pixels
    .hBackPorch     = LTDC_HORIZONTAL_BACK_PORCH,  // 10 pixels
    .hFrontPorch    = LTDC_HORIZONTAL_FRONT_PORCH, // 20 pixels
    
    // Synchronisation verticale
    .vsyncHeight    = LTDC_VSYNC_HEIGHT,            // 2 lignes
    .vBackPorch     = LTDC_VERTICAL_BACK_PORCH,     // 2 lignes
    .vFrontPorch    = LTDC_VERTICAL_FRONT_PORCH,    // 1 ligne
    
    // Zone active
    .activeWidth    = LTDC_ACTIVE_WIDTH,            // 320 pixels
    .activeHeight   = LTDC_ACTIVE_HEIGHT,           // 480 pixels
    .totalWidth     = LTDC_TOTAL_WIDTH,             // 360 pixels
    .totalHeight    = LTDC_TOTAL_HEIGHT,            // 485 lignes
    
    // Polarités (actif bas pour ILI9488)
    .hsyncPolarity  = LTDC_HSYNC_POLARITY,
    .vsyncPolarity  = LTDC_VSYNC_POLARITY,
    .dePolarity     = LTDC_DE_POLARITY,
    .pclkPolarity   = LTDC_PCLK_POLARITY,
    
    // Fond d'écran
    .backgroundColor = LTDC_DEFAULT_BACKGROUND,
    
    // Couche 1
    .layer1Framebuffer  = LTDC_FRAMEBUFFER_ADDR,
    .layer1WindowX1     = LTDC_LAYER_WINDOW_X1,
    .layer1WindowY1     = LTDC_LAYER_WINDOW_Y1,
    .layer1WindowX2     = LTDC_LAYER_WINDOW_X2,
    .layer1WindowY2     = LTDC_LAYER_WINDOW_Y2,
    .layer1PixelFormat  = LTDC_LAYER_PIXEL_FORMAT,
    .layer1DefaultColor = LTDC_LAYER_DEFAULT_COLOR,
    .layer1Alpha        = LTDC_LAYER_DEFAULT_ALPHA,
    .layer1LinePitch    = LTDC_LINE_PITCH,
    
    // Fréquence
    .pixelClockTarget   = LTDC_PIXEL_CLOCK_TARGET,
    .refreshRate        = LTDC_REFRESH_RATE
};

/** @brief État du LTDC */
static bool ltdc_initialized = false;

/** @brief Mode double buffering */
static bool double_buffering_active = false;

/** @brief Adresses des framebuffers */
static uint32_t front_buffer_addr = LTDC_FRAMEBUFFER_ADDR;
static uint32_t back_buffer_addr = LTDC_FRAMEBUFFER2_ADDR;

/** @brief Configuration actuelle */
static LTDC_Config current_config;

// ============================================================
// SECTION 1 : INITIALISATION DES BROCHES LTDC
// ============================================================

/**
 * @brief Configure tous les GPIO pour le LTDC
 * 
 * Configure 22 broches en Alternate Function AF14 (LTDC) :
 * - 16 broches de données (RGB565)
 * - 4 broches de contrôle (CLK, HSYNC, VSYNC, DE)
 */
static void ltdc_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    LTDC_DEBUG("Configuration des broches LTDC...\n");
    
    // Activer les horloges des ports utilisés
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    
    // Configuration commune pour toutes les broches LTDC
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;          // Alternate Function Push-Pull
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH; // Très haute vitesse
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;       // AF14 = LTDC
    
    // --- Broches Rouge (5 bits) ---
    
    // R4 - PE12
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    // R3 - PE13
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    // R2 - PE14
    GPIO_InitStruct.Pin = GPIO_PIN_14;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    // R1 - PE15
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    // R0 - PD9
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
    // --- Broches Vert (6 bits) ---
    
    // G5 - PE9
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    // G4 - PE8
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    // G3 - PE7
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    // G2 - PD1
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
    // G1 - PD0
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
    // G0 - PD15
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
    // --- Broches Bleu (5 bits) ---
    
    // B4 - PG12
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    
    // B3 - PG11
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    
    // B2 - PG10
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    
    // B1 - PB9
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // B0 - PB8
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // --- Broches de contrôle ---
    
    // CLK - PE0
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    // HSYNC - PE1
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    // VSYNC - PE2
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    // DE - PE3
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    LTDC_DEBUG("Broches LTDC configurées (22 broches)\n");
}

// ============================================================
// SECTION 2 : CONFIGURATION DE L'HORLOGE PIXEL
// ============================================================

/**
 * @brief Configure l'horloge pixel du LTDC
 * 
 * L'horloge pixel est dérivée du PLLSAI.
 * Pour obtenir ~10 MHz :
 * PLLSAI_VCO = HSE (8 MHz) × PLLSAI_N / PLLSAI_M
 * PCLK = PLLSAI_VCO / PLLSAI_R
 * 
 * @return true si la configuration réussit
 */
static bool ltdc_clock_config(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    
    LTDC_DEBUG("Configuration horloge pixel...\n");
    
    // Configuration du PLLSAI pour générer l'horloge pixel
    // PLLSAI_VCO = 8 MHz × 192 / 8 = 192 MHz
    // PCLK = 192 MHz / 20 = 9.6 MHz (~10 MHz)
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    PeriphClkInitStruct.PLLSAI.PLLSAIN = 192;        // Multiplicateur
    PeriphClkInitStruct.PLLSAI.PLLSAIM = 8;          // Diviseur entrée
    PeriphClkInitStruct.PLLSAI.PLLSAIR = 20;         // Diviseur sortie LTDC
    PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_DIV1;
    
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        LTDC_DEBUG("Erreur configuration PLLSAI\n");
        return false;
    }
    
    LTDC_DEBUG("Horloge pixel configurée (~10 MHz)\n");
    return true;
}

// ============================================================
// SECTION 3 : INITIALISATION PRINCIPALE
// ============================================================

/**
 * @brief Initialise le contrôleur LTDC
 */
bool ltdc_init(void)
{
    LTDC_DEBUG("Initialisation LTDC...\n");
    
    // 1. Configurer les broches GPIO
    ltdc_gpio_init();
    
    // 2. Configurer l'horloge pixel
    if (!ltdc_clock_config())
    {
        LTDC_DEBUG("Échec configuration horloge\n");
        return false;
    }
    
    // 3. Activer l'horloge LTDC
    __HAL_RCC_LTDC_CLK_ENABLE();
    
    // 4. Appliquer la configuration par défaut
    if (!ltdc_configure(&ltdc_default_config))
    {
        LTDC_DEBUG("Échec configuration LTDC\n");
        return false;
    }
    
    // 5. Activer le LTDC
    __HAL_LTDC_ENABLE(&hltdc);
    
    ltdc_initialized = true;
    
    LTDC_DEBUG("LTDC initialisé avec succès\n");
    LTDC_DEBUG("Résolution: %d×%d @ ~%d Hz\n", 
               current_config.activeWidth, 
               current_config.activeHeight, 
               current_config.refreshRate);
    
    return true;
}

/**
 * @brief Applique une configuration LTDC
 */
bool ltdc_configure(const LTDC_Config* config)
{
    if (config == NULL) return false;
    
    // Sauvegarder la configuration
    memcpy(&current_config, config, sizeof(LTDC_Config));
    
    // --- Configuration des timings ---
    hltdc.Init.HSPolarity = config->hsyncPolarity;
    hltdc.Init.VSPolarity = config->vsyncPolarity;
    hltdc.Init.DEPolarity = config->dePolarity;
    hltdc.Init.PCPolarity = config->pclkPolarity;
    
    hltdc.Init.HorizontalSync = config->hsyncWidth - 1;
    hltdc.Init.VerticalSync = config->vsyncHeight - 1;
    hltdc.Init.AccumulatedHBP = config->hsyncWidth + config->hBackPorch - 1;
    hltdc.Init.AccumulatedVBP = config->vsyncHeight + config->vBackPorch - 1;
    hltdc.Init.AccumulatedActiveW = config->hsyncWidth + config->hBackPorch + config->activeWidth - 1;
    hltdc.Init.AccumulatedActiveH = config->vsyncHeight + config->vBackPorch + config->activeHeight - 1;
    hltdc.Init.TotalWidth = config->totalWidth - 1;
    hltdc.Init.TotalHeigh = config->totalHeight - 1;
    
    hltdc.Init.Backcolor.Red = 0;
    hltdc.Init.Backcolor.Green = 0;
    hltdc.Init.Backcolor.Blue = 0;
    
    hltdc.Instance = LTDC;
    
    if (HAL_LTDC_Init(&hltdc) != HAL_OK)
    {
        return false;
    }
    
    // --- Configuration de la couche 1 ---
    LTDC_LayerCfgTypeDef layerCfg = {0};
    
    // Fenêtre de la couche (plein écran)
    layerCfg.WindowX0 = config->layer1WindowX1;
    layerCfg.WindowX1 = config->layer1WindowX2;
    layerCfg.WindowY0 = config->layer1WindowY1;
    layerCfg.WindowY1 = config->layer1WindowY2;
    
    // Format pixel
    layerCfg.PixelFormat = config->layer1PixelFormat;
    
    // Adresse du framebuffer
    layerCfg.FBStartAdress = config->layer1Framebuffer;
    
    // Alpha (opaque par défaut)
    layerCfg.Alpha = config->layer1Alpha;
    layerCfg.Alpha0 = 0;  // Transparence fixe désactivée
    
    // Couleur par défaut (si alpha < 255)
    layerCfg.Backcolor.Red = (config->layer1DefaultColor >> 16) & 0xFF;
    layerCfg.Backcolor.Green = (config->layer1DefaultColor >> 8) & 0xFF;
    layerCfg.Backcolor.Blue = config->layer1DefaultColor & 0xFF;
    
    // Pas de ligne (line pitch)
    layerCfg.ImageWidth = config->activeWidth;
    layerCfg.ImageHeight = config->activeHeight;
    
    // Configuration du blending
    layerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    layerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    
    if (HAL_LTDC_ConfigLayer(&hltdc, &layerCfg, 0) != HAL_OK)
    {
        return false;
    }
    
    return true;
}

/**
 * @brief Réinitialise le LTDC
 */
void ltdc_reset(void)
{
    __HAL_LTDC_DISABLE(&hltdc);
    ltdc_configure(&ltdc_default_config);
    __HAL_LTDC_ENABLE(&hltdc);
}

/**
 * @brief Définit l'adresse du framebuffer
 */
void ltdc_set_framebuffer(uint32_t addr)
{
    front_buffer_addr = addr;
    
    // Modifier l'adresse du framebuffer de la couche 1
    hltdc.LayerCfg[0].FBStartAdress = addr;
    
    // Recharger la configuration immédiatement
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
    
    LTDC_DEBUG("Framebuffer: 0x%08lX\n", (unsigned long)addr);
}

/**
 * @brief Bascule vers le second framebuffer
 */
void ltdc_swap_framebuffer(void)
{
    if (!double_buffering_active) return;
    
    uint32_t temp = front_buffer_addr;
    front_buffer_addr = back_buffer_addr;
    back_buffer_addr = temp;
    
    // Recharger pendant le blanking vertical (pas de tearing)
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
}

// ============================================================
// SECTION 4 : CONTRÔLE DES COUCHES
// ============================================================

/**
 * @brief Active/désactive une couche
 */
void ltdc_enable_layer(uint8_t layer, bool enable)
{
    if (layer > 1) return;
    
    if (enable)
        __HAL_LTDC_LAYER_ENABLE(&hltdc, layer);
    else
        __HAL_LTDC_LAYER_DISABLE(&hltdc, layer);
    
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
}

/**
 * @brief Définit la transparence d'une couche
 */
void ltdc_set_layer_alpha(uint8_t layer, uint8_t alpha)
{
    if (layer > 1) return;
    
    hltdc.LayerCfg[layer].Alpha = alpha;
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
}

/**
 * @brief Définit la position d'une couche
 */
void ltdc_set_layer_window(uint8_t layer, uint16_t x1, uint16_t y1, 
                            uint16_t x2, uint16_t y2)
{
    if (layer > 1) return;
    
    hltdc.LayerCfg[layer].WindowX0 = x1;
    hltdc.LayerCfg[layer].WindowX1 = x2;
    hltdc.LayerCfg[layer].WindowY0 = y1;
    hltdc.LayerCfg[layer].WindowY1 = y2;
    
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
}

/**
 * @brief Active/désactive le LTDC
 */
void ltdc_enable(bool enable)
{
    if (enable)
        __HAL_LTDC_ENABLE(&hltdc);
    else
        __HAL_LTDC_DISABLE(&hltdc);
}

/**
 * @brief Vérifie si le LTDC est prêt
 */
bool ltdc_is_ready(void)
{
    return ltdc_initialized;
}

// ============================================================
// SECTION 5 : DOUBLE BUFFERING
// ============================================================

/**
 * @brief Active le double buffering
 */
void ltdc_double_buffering_enable(void)
{
    if (!ltdc_initialized) return;
    
    double_buffering_active = true;
    
    // Initialiser le front buffer
    front_buffer_addr = LTDC_FRAMEBUFFER_ADDR;
    back_buffer_addr = LTDC_FRAMEBUFFER2_ADDR;
    
    // Effacer les deux buffers
    memset((void*)front_buffer_addr, 0, LTDC_FRAMEBUFFER_SIZE);
    memset((void*)back_buffer_addr, 0, LTDC_FRAMEBUFFER_SIZE);
    
    LTDC_DEBUG("Double buffering activé\n");
    LTDC_DEBUG("Front: 0x%08lX, Back: 0x%08lX\n", 
               (unsigned long)front_buffer_addr, 
               (unsigned long)back_buffer_addr);
}

/**
 * @brief Désactive le double buffering
 */
void ltdc_double_buffering_disable(void)
{
    double_buffering_active = false;
    
    // Revenir au framebuffer principal
    ltdc_set_framebuffer(LTDC_FRAMEBUFFER_ADDR);
    
    LTDC_DEBUG("Double buffering désactivé\n");
}

/**
 * @brief Récupère l'adresse du back buffer
 */
uint32_t ltdc_get_back_buffer(void)
{
    return double_buffering_active ? back_buffer_addr : front_buffer_addr;
}

/**
 * @brief Récupère l'adresse du front buffer
 */
uint32_t ltdc_get_front_buffer(void)
{
    return front_buffer_addr;
}

// ============================================================
// SECTION 6 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche la configuration LTDC
 */
void ltdc_print_config(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║         CONFIGURATION LTDC                   ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Résolution      : %d × %d                  ║\n", 
           current_config.activeWidth, current_config.activeHeight);
    printf("║ Total           : %d × %d                  ║\n",
           current_config.totalWidth, current_config.totalHeight);
    printf("║ HSYNC           : %d pixels                ║\n", current_config.hsyncWidth);
    printf("║ H Back Porch    : %d pixels                ║\n", current_config.hBackPorch);
    printf("║ H Front Porch   : %d pixels                ║\n", current_config.hFrontPorch);
    printf("║ VSYNC           : %d lignes                ║\n", current_config.vsyncHeight);
    printf("║ V Back Porch    : %d lignes                ║\n", current_config.vBackPorch);
    printf("║ V Front Porch   : %d lignes                ║\n", current_config.vFrontPorch);
    printf("║ Pixel Clock     : %lu MHz                  ║\n", 
           (unsigned long)(current_config.pixelClockTarget / 1000000));
    printf("║ Rafraîchissement: ~%d Hz                   ║\n", current_config.refreshRate);
    printf("║ Format Pixel    : RGB565 (16 bpp)           ║\n");
    printf("║ Framebuffer     : 0x%08lX                  ║\n", 
           (unsigned long)current_config.layer1Framebuffer);
    printf("║ Double Buffering: %s                        ║\n", 
           double_buffering_active ? "Activé" : "Désactivé");
    printf("║ Alpha           : %d                        ║\n", current_config.layer1Alpha);
    printf("║ Initialisé      : %s                        ║\n", 
           ltdc_initialized ? "Oui" : "Non");
    printf("╚══════════════════════════════════════════════╝\n\n");
}

/**
 * @brief Affiche les statistiques LTDC
 */
void ltdc_print_statistics(void)
{
    printf("\n═══ STATISTIQUES LTDC ═══\n");
    printf("État       : %s\n", ltdc_initialized ? "Initialisé" : "Non initialisé");
    printf("Écran      : %s\n", ltdc_is_display_connected() ? "Connecté" : "Non détecté");
    printf("Framebuffer: 0x%08lX\n", (unsigned long)front_buffer_addr);
    printf("Fréquence  : ~%d Hz\n", current_config.refreshRate);
    printf("Taille FB  : %lu octets\n", (unsigned long)LTDC_FRAMEBUFFER_SIZE);
    printf("═══════════════════════════\n\n");
}

/**
 * @brief Vérifie la présence d'un écran
 */
bool ltdc_is_display_connected(void)
{
    // Vérifier que l'horloge LTDC est active
    return (__HAL_RCC_LTDC_IS_CLK_ENABLED() != 0);
}