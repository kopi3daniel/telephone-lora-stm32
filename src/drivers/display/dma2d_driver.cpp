/**
 * @file dma2d_driver.cpp
 * @brief Implémentation du driver DMA2D (Chrom-ART Accelerator™)
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans dma2d_driver.h.
 * 
 * Le DMA2D fonctionne de manière ASYNCHRONE :
 * - On configure l'opération
 * - On démarre le transfert
 * - Le DMA2D travaille en arrière-plan
 * - On peut attendre la fin ou faire autre chose
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "dma2d_driver.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================
// HANDLES EXTERNES
// ============================================================

/** @brief Handle DMA2D */
extern DMA2D_HandleTypeDef hdma2d;

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État d'initialisation */
static bool dma2d_initialized = false;

/** @brief Adresse du framebuffer principal */
static uint32_t framebuffer_addr = LTDC_FRAMEBUFFER_ADDR;

/** @brief Statistiques */
static DMA2D_Statistics stats = {0};

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le driver DMA2D
 */
bool dma2d_init(void)
{
    DMA2D_DEBUG("Initialisation DMA2D...\n");
    
    // Activer l'horloge DMA2D
    __HAL_RCC_DMA2D_CLK_ENABLE();
    
    // Initialiser le handle HAL
    hdma2d.Instance = DMA2D;
    
    // Réinitialiser le DMA2D
    if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
    {
        DMA2D_DEBUG("Échec initialisation\n");
        return false;
    }
    
    // Configurer les interruptions (optionnel)
    HAL_NVIC_SetPriority(DMA2D_IRQn, 4, 0);
    HAL_NVIC_EnableIRQ(DMA2D_IRQn);
    
    dma2d_initialized = true;
    DMA2D_DEBUG("Initialisation réussie\n");
    
    return true;
}

/**
 * @brief Désinitialise le driver
 */
void dma2d_deinit(void)
{
    HAL_DMA2D_DeInit(&hdma2d);
    __HAL_RCC_DMA2D_CLK_DISABLE();
    dma2d_initialized = false;
}

/**
 * @brief Vérifie si le DMA2D est prêt
 */
bool dma2d_is_ready(void)
{
    return dma2d_initialized;
}

// ============================================================
// SECTION 2 : CONVERSION DES FORMATS DE PIXELS
// ============================================================

/**
 * @brief Convertit le format DMA2D vers le format HAL
 */
static uint32_t dma2d_get_hal_format(DMA2D_PixelFormat format)
{
    switch (format)
    {
        case DMA2D_FORMAT_ARGB8888: return DMA2D_OUTPUT_ARGB8888;
        case DMA2D_FORMAT_RGB888:   return DMA2D_OUTPUT_RGB888;
        case DMA2D_FORMAT_RGB565:   return DMA2D_OUTPUT_RGB565;
        case DMA2D_FORMAT_ARGB1555: return DMA2D_OUTPUT_ARGB1555;
        case DMA2D_FORMAT_ARGB4444: return DMA2D_OUTPUT_ARGB4444;
        default:                    return DMA2D_OUTPUT_RGB565;
    }
}

/**
 * @brief Récupère le nombre d'octets par pixel pour un format
 */
static uint8_t dma2d_get_bytes_per_pixel(DMA2D_PixelFormat format)
{
    switch (format)
    {
        case DMA2D_FORMAT_ARGB8888: return 4;
        case DMA2D_FORMAT_RGB888:   return 3;
        case DMA2D_FORMAT_RGB565:   return 2;
        case DMA2D_FORMAT_ARGB1555: return 2;
        case DMA2D_FORMAT_ARGB4444: return 2;
        case DMA2D_FORMAT_L8:       return 1;
        case DMA2D_FORMAT_AL44:     return 1;
        case DMA2D_FORMAT_AL88:     return 2;
        case DMA2D_FORMAT_A8:       return 1;
        case DMA2D_FORMAT_A4:       return 1;  // 2 pixels par octet
        default:                    return 2;
    }
}

// ============================================================
// SECTION 3 : OPÉRATIONS DE REMPLISSAGE (FILL)
// ============================================================

/**
 * @brief Remplit une zone rectangulaire
 */
bool dma2d_fill_rect(const DMA2D_FillConfig* config)
{
    if (!dma2d_initialized || config == NULL) return false;
    
    uint32_t startTime = HAL_GetTick();
    
    // Attendre que le DMA2D soit libre
    if (!dma2d_wait_for_completion(DMA2D_DEFAULT_TIMEOUT_MS))
    {
        stats.errors++;
        return false;
    }
    
    // Configurer le mode Register-to-Memory (remplissage)
    DMA2D->CR = DMA2D_R2M;
    
    // Format de sortie
    DMA2D->OPFCCR = dma2d_get_hal_format(config->dstFormat);
    
    // Adresse de destination
    DMA2D->OMAR = config->dstAddress;
    
    // Offset (décalage entre les lignes en pixels)
    DMA2D->OOR = config->dstOffset;
    
    // Couleur de remplissage
    DMA2D->OCOLR = config->fillColor;
    
    // Nombre de pixels : Largeur (bits 15:0) | Hauteur (bits 31:16)
    DMA2D->NLR = ((uint32_t)config->dstWidth << 16) | config->dstHeight;
    
    // Démarrer le transfert
    DMA2D->CR |= DMA2D_CR_START;
    
    // Attendre la fin
    if (!dma2d_wait_for_completion(DMA2D_DEFAULT_TIMEOUT_MS))
    {
        stats.errors++;
        return false;
    }
    
    // Mettre à jour les statistiques
    stats.fillOperations++;
    stats.totalPixels += (uint32_t)config->dstWidth * config->dstHeight;
    stats.totalBytes += (uint32_t)config->dstWidth * config->dstHeight * 
                        dma2d_get_bytes_per_pixel(config->dstFormat);
    stats.lastOperationTimeUs = (HAL_GetTick() - startTime) * 1000;
    stats.totalTimeUs += stats.lastOperationTimeUs;
    
    return true;
}

/**
 * @brief Remplit tout l'écran avec une couleur
 */
bool dma2d_fill_screen(uint16_t color)
{
    DMA2D_FillConfig config = {
        .dstAddress = framebuffer_addr,
        .dstWidth = LTDC_WIDTH,
        .dstHeight = LTDC_HEIGHT,
        .dstOffset = 0,
        .fillColor = color,
        .dstFormat = DMA2D_FORMAT_RGB565
    };
    
    return dma2d_fill_rect(&config);
}

/**
 * @brief Remplit une zone avec coordonnées XY
 */
bool dma2d_fill_rect_xy(uint16_t x1, uint16_t y1, 
                         uint16_t x2, uint16_t y2, 
                         uint16_t color)
{
    if (x1 > x2) { uint16_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    
    uint16_t width = x2 - x1 + 1;
    uint16_t height = y2 - y1 + 1;
    
    DMA2D_FillConfig config = {
        .dstAddress = framebuffer_addr + (y1 * LTDC_WIDTH + x1) * 2,
        .dstWidth = width,
        .dstHeight = height,
        .dstOffset = LTDC_WIDTH - width,  // Saut de ligne
        .fillColor = color,
        .dstFormat = DMA2D_FORMAT_RGB565
    };
    
    return dma2d_fill_rect(&config);
}

/**
 * @brief Efface le framebuffer
 */
void dma2d_clear_framebuffer(uint32_t addr)
{
    if (!dma2d_initialized) return;
    
    dma2d_wait_for_completion(DMA2D_DEFAULT_TIMEOUT_MS);
    
    DMA2D->CR = DMA2D_R2M;
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
    DMA2D->OMAR = addr;
    DMA2D->OOR = 0;
    DMA2D->OCOLR = 0x0000;  // Noir
    DMA2D->NLR = (LTDC_WIDTH << 16) | LTDC_HEIGHT;
    DMA2D->CR |= DMA2D_CR_START;
    
    dma2d_wait_for_completion(DMA2D_DEFAULT_TIMEOUT_MS);
}

// ============================================================
// SECTION 4 : OPÉRATIONS DE COPIE (COPY)
// ============================================================

/**
 * @brief Copie une zone mémoire
 */
bool dma2d_copy(const DMA2D_CopyConfig* config)
{
    if (!dma2d_initialized || config == NULL) return false;
    
    uint32_t startTime = HAL_GetTick();
    
    if (!dma2d_wait_for_completion(DMA2D_DEFAULT_TIMEOUT_MS))
    {
        stats.errors++;
        return false;
    }
    
    if (config->convertFormat)
    {
        // Mode Memory-to-Memory avec conversion de format (PFC)
        DMA2D->CR = DMA2D_M2M_PFC;
        
        // Format source
        DMA2D->FGPFCCR = dma2d_get_hal_format(config->srcFormat);
        
        // Format destination
        DMA2D->OPFCCR = dma2d_get_hal_format(config->dstFormat);
    }
    else
    {
        // Mode Memory-to-Memory simple
        DMA2D->CR = DMA2D_M2M;
    }
    
    // Adresse source (Foreground pour M2M_PFC)
    DMA2D->FGMAR = config->srcAddress;
    DMA2D->FGOR = config->srcOffset;
    
    // Adresse destination
    DMA2D->OMAR = config->dstAddress;
    DMA2D->OOR = config->dstOffset;
    
    // Dimensions
    DMA2D->NLR = ((uint32_t)config->width << 16) | config->height;
    
    // Démarrer
    DMA2D->CR |= DMA2D_CR_START;
    
    if (!dma2d_wait_for_completion(DMA2D_DEFAULT_TIMEOUT_MS))
    {
        stats.errors++;
        return false;
    }
    
    // Statistiques
    stats.copyOperations++;
    stats.totalPixels += (uint32_t)config->width * config->height;
    stats.lastOperationTimeUs = (HAL_GetTick() - startTime) * 1000;
    stats.totalTimeUs += stats.lastOperationTimeUs;
    
    return true;
}

/**
 * @brief Copie une zone rectangulaire du framebuffer
 */
bool dma2d_copy_rect(uint16_t srcX, uint16_t srcY,
                      uint16_t dstX, uint16_t dstY,
                      uint16_t width, uint16_t height)
{
    DMA2D_CopyConfig config = {
        .srcAddress = framebuffer_addr + (srcY * LTDC_WIDTH + srcX) * 2,
        .dstAddress = framebuffer_addr + (dstY * LTDC_WIDTH + dstX) * 2,
        .width = width,
        .height = height,
        .srcOffset = LTDC_WIDTH - width,
        .dstOffset = LTDC_WIDTH - width,
        .srcFormat = DMA2D_FORMAT_RGB565,
        .dstFormat = DMA2D_FORMAT_RGB565,
        .convertFormat = false
    };
    
    return dma2d_copy(&config);
}

/**
 * @brief Copie tout le framebuffer
 */
void dma2d_copy_framebuffer(uint32_t srcAddr, uint32_t dstAddr)
{
    DMA2D_CopyConfig config = {
        .srcAddress = srcAddr,
        .dstAddress = dstAddr,
        .width = LTDC_WIDTH,
        .height = LTDC_HEIGHT,
        .srcOffset = 0,
        .dstOffset = 0,
        .srcFormat = DMA2D_FORMAT_RGB565,
        .dstFormat = DMA2D_FORMAT_RGB565,
        .convertFormat = false
    };
    
    dma2d_copy(&config);
}

// ============================================================
// SECTION 5 : OPÉRATIONS DE BLENDING
// ============================================================

/**
 * @brief Effectue un alpha blending
 */
bool dma2d_blend(const DMA2D_BlendConfig* config)
{
    if (!dma2d_initialized || config == NULL) return false;
    
    uint32_t startTime = HAL_GetTick();
    
    if (!dma2d_wait_for_completion(DMA2D_DEFAULT_TIMEOUT_MS))
    {
        stats.errors++;
        return false;
    }
    
    // Mode Memory-to-Memory avec blending
    DMA2D->CR = DMA2D_M2M_BLEND;
    
    // Format foreground
    DMA2D->FGPFCCR = dma2d_get_hal_format(config->fgFormat);
    if (config->alphaPerPixel)
    {
        DMA2D->FGPFCCR |= DMA2D_FGPFCCR_AM;  // Alpha mode
    }
    DMA2D->FGPFCCR |= ((uint32_t)config->alphaConst << 24);  // Alpha constant
    
    // Format background
    DMA2D->BGPFCCR = dma2d_get_hal_format(config->bgFormat);
    
    // Format destination
    DMA2D->OPFCCR = dma2d_get_hal_format(config->dstFormat);
    
    // Adresses
    DMA2D->FGMAR = config->fgAddress;   // Foreground
    DMA2D->BGMAR = config->bgAddress;   // Background
    DMA2D->OMAR = config->dstAddress;    // Destination
    
    // Offsets
    DMA2D->FGOR = config->fgOffset;
    DMA2D->BGOR = config->bgOffset;
    DMA2D->OOR = config->dstOffset;
    
    // Dimensions
    DMA2D->NLR = ((uint32_t)config->width << 16) | config->height;
    
    // Démarrer
    DMA2D->CR |= DMA2D_CR_START;
    
    if (!dma2d_wait_for_completion(DMA2D_DEFAULT_TIMEOUT_MS))
    {
        stats.errors++;
        return false;
    }
    
    // Statistiques
    stats.blendOperations++;
    stats.totalPixels += (uint32_t)config->width * config->height;
    stats.lastOperationTimeUs = (HAL_GetTick() - startTime) * 1000;
    stats.totalTimeUs += stats.lastOperationTimeUs;
    
    return true;
}

/**
 * @brief Affiche une image avec transparence
 */
bool dma2d_blend_image(uint32_t fgAddr, uint16_t fgWidth, uint16_t fgHeight,
                        uint16_t dstX, uint16_t dstY, uint8_t alpha)
{
    DMA2D_BlendConfig config = {
        .fgAddress = fgAddr,
        .fgOffset = 0,
        .fgFormat = DMA2D_FORMAT_RGB565,
        
        .bgAddress = framebuffer_addr + (dstY * LTDC_WIDTH + dstX) * 2,
        .bgOffset = LTDC_WIDTH - fgWidth,
        .bgFormat = DMA2D_FORMAT_RGB565,
        
        .dstAddress = framebuffer_addr + (dstY * LTDC_WIDTH + dstX) * 2,
        .dstOffset = LTDC_WIDTH - fgWidth,
        .dstFormat = DMA2D_FORMAT_RGB565,
        
        .width = fgWidth,
        .height = fgHeight,
        
        .alphaConst = alpha,
        .alphaPerPixel = false
    };
    
    return dma2d_blend(&config);
}

// ============================================================
// SECTION 6 : OPÉRATIONS D'IMAGES
// ============================================================

/**
 * @brief Affiche une image (bitmap) dans le framebuffer
 */
bool dma2d_draw_image(const DMA2D_ImageConfig* config)
{
    if (!dma2d_initialized || config == NULL || config->imageData == NULL) return false;
    
    uint32_t dstAddr = framebuffer_addr + 
                       (config->dstY * config->dstScreenWidth + config->dstX) * 2;
    
    DMA2D_CopyConfig copyConfig = {
        .srcAddress = (uint32_t)config->imageData,
        .dstAddress = dstAddr,
        .width = config->imageWidth,
        .height = config->imageHeight,
        .srcOffset = 0,
        .dstOffset = config->dstScreenWidth - config->imageWidth,
        .srcFormat = config->imageFormat,
        .dstFormat = DMA2D_FORMAT_RGB565,
        .convertFormat = (config->imageFormat != DMA2D_FORMAT_RGB565)
    };
    
    return dma2d_copy(&copyConfig);
}

/**
 * @brief Affiche une icône
 */
bool dma2d_draw_icon(const uint16_t* iconData, uint16_t iconWidth, uint16_t iconHeight,
                      uint16_t x, uint16_t y)
{
    DMA2D_ImageConfig config = {
        .imageData = (const uint8_t*)iconData,
        .dstAddress = 0,  // Sera calculé dans draw_image
        .imageWidth = iconWidth,
        .imageHeight = iconHeight,
        .dstX = x,
        .dstY = y,
        .dstScreenWidth = LTDC_WIDTH,
        .imageFormat = DMA2D_FORMAT_RGB565,
        .useAlpha = false
    };
    
    return dma2d_draw_image(&config);
}

/**
 * @brief Affiche une image avec mise à l'échelle (logicielle)
 */
bool dma2d_draw_scaled_image(uint32_t srcAddr,
                              uint16_t srcWidth, uint16_t srcHeight,
                              uint16_t dstX, uint16_t dstY,
                              uint16_t dstWidth, uint16_t dstHeight)
{
    if (!dma2d_initialized) return false;
    
    // Le DMA2D ne supporte pas le redimensionnement matériel.
    // On utilise un algorithme simplifié "plus proche voisin".
    // Pour une version plus rapide, utiliser le CPU ou pré-calculer.
    
    uint16_t* src = (uint16_t*)srcAddr;
    uint16_t* dst = (uint16_t*)(framebuffer_addr + (dstY * LTDC_WIDTH + dstX) * 2);
    
    for (uint16_t dy = 0; dy < dstHeight; dy++)
    {
        uint16_t sy = (dy * srcHeight) / dstHeight;
        
        for (uint16_t dx = 0; dx < dstWidth; dx++)
        {
            uint16_t sx = (dx * srcWidth) / dstWidth;
            dst[dy * LTDC_WIDTH + dx] = src[sy * srcWidth + sx];
        }
    }
    
    return true;
}

// ============================================================
// SECTION 7 : CONVERSION DE FORMATS
// ============================================================

/**
 * @brief Convertit un buffer d'un format à un autre
 */
bool dma2d_convert_format(uint32_t srcAddr, uint32_t dstAddr,
                           uint16_t width, uint16_t height,
                           DMA2D_PixelFormat srcFormat,
                           DMA2D_PixelFormat dstFormat)
{
    DMA2D_CopyConfig config = {
        .srcAddress = srcAddr,
        .dstAddress = dstAddr,
        .width = width,
        .height = height,
        .srcOffset = 0,
        .dstOffset = 0,
        .srcFormat = srcFormat,
        .dstFormat = dstFormat,
        .convertFormat = true
    };
    
    return dma2d_copy(&config);
}

/**
 * @brief Convertit RGB888 en RGB565 (logiciel)
 */
void dma2d_rgb888_to_rgb565(const uint8_t* src888, uint16_t* dst565, uint32_t pixelCount)
{
    for (uint32_t i = 0; i < pixelCount; i++)
    {
        uint8_t r = src888[i * 3 + 0];
        uint8_t g = src888[i * 3 + 1];
        uint8_t b = src888[i * 3 + 2];
        
        dst565[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
}

/**
 * @brief Convertit RGB565 en RGB888 (logiciel)
 */
void dma2d_rgb565_to_rgb888(const uint16_t* src565, uint8_t* dst888, uint32_t pixelCount)
{
    for (uint32_t i = 0; i < pixelCount; i++)
    {
        uint16_t color = src565[i];
        
        dst888[i * 3 + 0] = (color >> 8) & 0xF8;  // Rouge
        dst888[i * 3 + 1] = (color >> 3) & 0xFC;  // Vert
        dst888[i * 3 + 2] = (color << 3) & 0xF8;  // Bleu
    }
}

// ============================================================
// SECTION 8 : FONCTIONS UTILITAIRES
// ============================================================

/**
 * @brief Attend la fin de l'opération DMA2D
 */
bool dma2d_wait_for_completion(uint32_t timeoutMs)
{
    uint32_t start = HAL_GetTick();
    
    // Attendre que le bit START passe à 0 (opération terminée)
    while (DMA2D->CR & DMA2D_CR_START)
    {
        if ((HAL_GetTick() - start) > timeoutMs)
        {
            DMA2D_DEBUG("Timeout DMA2D !\n");
            return false;
        }
        // Petite pause pour éviter de saturer le bus
        __NOP();
    }
    
    return true;
}

/**
 * @brief Vérifie si le DMA2D est occupé
 */
bool dma2d_is_busy(void)
{
    return (DMA2D->CR & DMA2D_CR_START) != 0;
}

/**
 * @brief Annule l'opération en cours
 */
void dma2d_abort(void)
{
    DMA2D->CR &= ~DMA2D_CR_START;
    dma2d_wait_for_completion(DMA2D_DEFAULT_TIMEOUT_MS);
}

/**
 * @brief Récupère l'adresse du framebuffer
 */
uint32_t dma2d_get_framebuffer_addr(void)
{
    return framebuffer_addr;
}

/**
 * @brief Définit l'adresse du framebuffer
 */
void dma2d_set_framebuffer_addr(uint32_t addr)
{
    framebuffer_addr = addr;
}

// ============================================================
// SECTION 9 : FONCTIONS AVANCÉES
// ============================================================

/**
 * @brief Dessine une ligne horizontale (optimisé DMA2D)
 */
void dma2d_draw_hline(uint16_t x1, uint16_t y, uint16_t x2, uint16_t color)
{
    if (x1 > x2) { uint16_t t = x1; x1 = x2; x2 = t; }
    
    dma2d_fill_rect_xy(x1, y, x2, y, color);
}

/**
 * @brief Dessine une ligne verticale (optimisé DMA2D)
 */
void dma2d_draw_vline(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color)
{
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    
    dma2d_fill_rect_xy(x, y1, x, y2, color);
}

/**
 * @brief Remplit un rectangle arrondi
 */
void dma2d_fill_round_rect(uint16_t x1, uint16_t y1,
                            uint16_t x2, uint16_t y2,
                            uint16_t radius, uint16_t color)
{
    // Partie centrale (rectangle plein)
    dma2d_fill_rect_xy(x1 + radius, y1, x2 - radius, y2, color);
    dma2d_fill_rect_xy(x1, y1 + radius, x2, y2 - radius, color);
    
    // Les coins arrondis sont dessinés par le CPU (le DMA2D ne gère pas les cercles)
    // Pour une version plus optimisée, pré-calculer les coins
}

// ============================================================
// SECTION 10 : STATISTIQUES
// ============================================================

/**
 * @brief Récupère les statistiques
 */
void dma2d_get_statistics(DMA2D_Statistics* outStats)
{
    if (outStats != NULL)
    {
        memcpy(outStats, &stats, sizeof(DMA2D_Statistics));
    }
}

/**
 * @brief Réinitialise les statistiques
 */
void dma2d_reset_statistics(void)
{
    memset(&stats, 0, sizeof(DMA2D_Statistics));
}

/**
 * @brief Affiche les statistiques (debug)
 */
void dma2d_print_statistics(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║       STATISTIQUES DMA2D                 ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Remplissages : %8lu                ║\n", (unsigned long)stats.fillOperations);
    printf("║ Copies       : %8lu                ║\n", (unsigned long)stats.copyOperations);
    printf("║ Blendings    : %8lu                ║\n", (unsigned long)stats.blendOperations);
    printf("║ Total pixels : %8lu                ║\n", (unsigned long)stats.totalPixels);
    printf("║ Total octets : %8lu                ║\n", (unsigned long)stats.totalBytes);
    printf("║ Erreurs      : %8lu                ║\n", (unsigned long)stats.errors);
    printf("║ Temps total  : %8lu µs            ║\n", (unsigned long)stats.totalTimeUs);
    printf("╚══════════════════════════════════════════╝\n\n");
}

// ============================================================
// SECTION 11 : IRQ HANDLER (optionnel)
// ============================================================

/**
 * @brief Handler d'interruption DMA2D
 * 
 * Appelé quand une opération DMA2D se termine.
 * Peut être utilisé pour enchaîner des opérations.
 */
void DMA2D_IRQHandler(void)
{
    // Vérifier si transfert terminé
    if (__HAL_DMA2D_GET_FLAG(&hdma2d, DMA2D_FLAG_TC))
    {
        __HAL_DMA2D_CLEAR_FLAG(&hdma2d, DMA2D_FLAG_TC);
        // Opération terminée
    }
    
    // Vérifier erreur de transfert
    if (__HAL_DMA2D_GET_FLAG(&hdma2d, DMA2D_FLAG_TE))
    {
        __HAL_DMA2D_CLEAR_FLAG(&hdma2d, DMA2D_FLAG_TE);
        stats.errors++;
    }
    
    // Vérifier erreur de configuration
    if (__HAL_DMA2D_GET_FLAG(&hdma2d, DMA2D_FLAG_CE))
    {
        __HAL_DMA2D_CLEAR_FLAG(&hdma2d, DMA2D_FLAG_CE);
        stats.errors++;
    }
    
    HAL_DMA2D_IRQHandler(&hdma2d);
}