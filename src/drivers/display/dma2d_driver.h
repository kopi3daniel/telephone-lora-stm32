/**
 * @file dma2d_driver.h
 * @brief Driver pour l'accélérateur graphique DMA2D (Chrom-ART™)
 * 
 * Le DMA2D (Chrom-ART Accelerator™) est un accélérateur graphique
 * matériel intégré au STM32F429. Il permet d'effectuer des opérations
 * graphiques SANS utiliser le CPU :
 * 
 * - Remplissage de rectangles (Fill)
 * - Copie de zones mémoire (Copy)
 * - Conversion de formats de couleurs (PFC - Pixel Format Converter)
 * - Alpha blending (transparence)
 * - Mélange d'images (Blending)
 * 
 * Avantages par rapport au CPU :
 * - Jusqu'à 100x plus rapide pour les opérations graphiques
 * - Libère le CPU pour d'autres tâches (audio, LoRa)
 * - Consommation énergétique réduite
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef DMA2D_DRIVER_H
#define DMA2D_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "ili9488_defs.h"
#include "../../config.h"
#include "../../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Largeur maximale supportée par le DMA2D */
#define DMA2D_MAX_WIDTH                 16384

/** @brief Hauteur maximale supportée par le DMA2D */
#define DMA2D_MAX_HEIGHT                16384

/** @brief Timeout par défaut (ms) */
#define DMA2D_DEFAULT_TIMEOUT_MS        1000

/** @brief Version du driver */
#define DMA2D_DRIVER_VERSION           "1.0.0"

// ============================================================
// SECTION 2 : MODES DE FONCTIONNEMENT
// ============================================================

/**
 * @brief Modes de fonctionnement du DMA2D
 */
typedef enum {
    DMA2D_MODE_FILL        = 0,    // Remplissage (Register-to-Memory)
    DMA2D_MODE_COPY        = 1,    // Copie (Memory-to-Memory)
    DMA2D_MODE_COPY_PFC    = 2,    // Copie avec conversion de format
    DMA2D_MODE_BLEND       = 3,    // Mélange avec alpha blending
    DMA2D_MODE_BLEND_PFC   = 4     // Mélange avec conversion + blending
} DMA2D_Mode;

// ============================================================
// SECTION 3 : FORMATS DE PIXELS SUPPORTÉS
// ============================================================

/**
 * @brief Formats de pixels supportés par le DMA2D
 */
typedef enum {
    DMA2D_FORMAT_ARGB8888   = 0,    // 32 bits : Alpha + RGB (8 bits chacun)
    DMA2D_FORMAT_RGB888     = 1,    // 24 bits : RGB (8 bits chacun)
    DMA2D_FORMAT_RGB565     = 2,    // 16 bits : RGB565 (utilisé pour l'écran)
    DMA2D_FORMAT_ARGB1555   = 3,    // 16 bits : Alpha 1 bit + RGB555
    DMA2D_FORMAT_ARGB4444   = 4,    // 16 bits : Alpha 4 bits + RGB444
    DMA2D_FORMAT_L8         = 5,    // 8 bits : Luminance (niveaux de gris)
    DMA2D_FORMAT_AL44       = 6,    // 8 bits : Alpha 4 bits + Luminance 4 bits
    DMA2D_FORMAT_AL88       = 7,    // 16 bits : Alpha 8 bits + Luminance 8 bits
    DMA2D_FORMAT_A8         = 8,    // 8 bits : Alpha seulement
    DMA2D_FORMAT_A4         = 9     // 4 bits : Alpha seulement
} DMA2D_PixelFormat;

// ============================================================
// SECTION 4 : STRUCTURES DE CONFIGURATION
// ============================================================

/**
 * @brief Configuration d'une opération de remplissage
 */
typedef struct {
    uint32_t dstAddress;                // Adresse de destination
    uint16_t dstWidth;                  // Largeur de la zone
    uint16_t dstHeight;                 // Hauteur de la zone
    uint16_t dstOffset;                 // Décalage entre les lignes (pixels)
    uint32_t fillColor;                 // Couleur de remplissage (format dépend du mode)
    DMA2D_PixelFormat dstFormat;        // Format de destination
} DMA2D_FillConfig;

/**
 * @brief Configuration d'une opération de copie
 */
typedef struct {
    uint32_t srcAddress;                // Adresse source
    uint32_t dstAddress;                // Adresse destination
    uint16_t width;                     // Largeur à copier
    uint16_t height;                    // Hauteur à copier
    uint16_t srcOffset;                 // Décalage lignes source (pixels)
    uint16_t dstOffset;                 // Décalage lignes destination (pixels)
    DMA2D_PixelFormat srcFormat;        // Format source
    DMA2D_PixelFormat dstFormat;        // Format destination
    bool convertFormat;                 // Activer la conversion de format
} DMA2D_CopyConfig;

/**
 * @brief Configuration d'une opération de blending
 */
typedef struct {
    // Couche de premier plan (foreground)
    uint32_t fgAddress;                 // Adresse foreground
    uint16_t fgOffset;                  // Décalage lignes foreground
    DMA2D_PixelFormat fgFormat;         // Format foreground
    
    // Couche d'arrière-plan (background)
    uint32_t bgAddress;                 // Adresse background
    uint16_t bgOffset;                  // Décalage lignes background
    DMA2D_PixelFormat bgFormat;         // Format background
    
    // Destination
    uint32_t dstAddress;                // Adresse destination
    uint16_t dstOffset;                 // Décalage lignes destination
    DMA2D_PixelFormat dstFormat;        // Format destination
    
    // Dimensions
    uint16_t width;                     // Largeur
    uint16_t height;                    // Hauteur
    
    // Alpha
    uint8_t alphaConst;                 // Alpha constant (0-255)
    bool alphaPerPixel;                 // Utiliser l'alpha par pixel
} DMA2D_BlendConfig;

/**
 * @brief Configuration d'une opération de transfert d'image
 */
typedef struct {
    const uint8_t* imageData;           // Données de l'image (compressée ou non)
    uint32_t dstAddress;                // Adresse destination (framebuffer)
    uint16_t imageWidth;                // Largeur de l'image source
    uint16_t imageHeight;               // Hauteur de l'image source
    uint16_t dstX;                      // Position X destination
    uint16_t dstY;                      // Position Y destination
    uint16_t dstScreenWidth;            // Largeur de l'écran (pour le pitch)
    DMA2D_PixelFormat imageFormat;      // Format de l'image source
    bool useAlpha;                      // Utiliser le canal alpha
} DMA2D_ImageConfig;

// ============================================================
// SECTION 5 : STATISTIQUES
// ============================================================

/**
 * @brief Statistiques d'utilisation du DMA2D
 */
typedef struct {
    uint32_t fillOperations;            // Nombre d'opérations de remplissage
    uint32_t copyOperations;            // Nombre d'opérations de copie
    uint32_t blendOperations;           // Nombre d'opérations de blending
    uint32_t totalPixels;               // Total pixels traités
    uint32_t totalBytes;                // Total octets transférés
    uint32_t errors;                    // Nombre d'erreurs
    uint32_t lastOperationTimeUs;       // Durée dernière opération (µs)
    uint32_t totalTimeUs;               // Temps total (µs)
} DMA2D_Statistics;

// ============================================================
// SECTION 6 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise le driver DMA2D
 * @return true si succès
 */
bool dma2d_init(void);

/**
 * @brief Désinitialise le driver DMA2D
 */
void dma2d_deinit(void);

/**
 * @brief Vérifie si le DMA2D est prêt
 * @return true si initialisé
 */
bool dma2d_is_ready(void);

/**
 * @brief Récupère les statistiques du DMA2D
 * @param stats Structure à remplir
 */
void dma2d_get_statistics(DMA2D_Statistics* stats);

/**
 * @brief Réinitialise les statistiques
 */
void dma2d_reset_statistics(void);

// ============================================================
// SECTION 7 : OPÉRATIONS DE REMPLISSAGE (FILL)
// ============================================================

/**
 * @brief Remplit une zone rectangulaire avec une couleur
 * @param config Configuration de l'opération
 * @return true si succès
 */
bool dma2d_fill_rect(const DMA2D_FillConfig* config);

/**
 * @brief Remplit tout l'écran avec une couleur
 * @param color Couleur RGB565
 * @return true si succès
 */
bool dma2d_fill_screen(uint16_t color);

/**
 * @brief Remplit une zone du framebuffer
 * @param x1 Colonne de début
 * @param y1 Ligne de début
 * @param x2 Colonne de fin
 * @param y2 Ligne de fin
 * @param color Couleur RGB565
 * @return true si succès
 */
bool dma2d_fill_rect_xy(uint16_t x1, uint16_t y1, 
                         uint16_t x2, uint16_t y2, 
                         uint16_t color);

/**
 * @brief Efface le framebuffer (remplissage noir)
 * @param framebufferAddr Adresse du framebuffer
 */
void dma2d_clear_framebuffer(uint32_t framebufferAddr);

// ============================================================
// SECTION 8 : OPÉRATIONS DE COPIE (COPY)
// ============================================================

/**
 * @brief Copie une zone mémoire avec ou sans conversion de format
 * @param config Configuration de la copie
 * @return true si succès
 */
bool dma2d_copy(const DMA2D_CopyConfig* config);

/**
 * @brief Copie une zone rectangulaire du framebuffer
 * @param srcX Source X
 * @param srcY Source Y
 * @param dstX Destination X
 * @param dstY Destination Y
 * @param width Largeur
 * @param height Hauteur
 * @return true si succès
 */
bool dma2d_copy_rect(uint16_t srcX, uint16_t srcY,
                      uint16_t dstX, uint16_t dstY,
                      uint16_t width, uint16_t height);

/**
 * @brief Copie tout le framebuffer
 * @param srcAddr Adresse source
 * @param dstAddr Adresse destination
 */
void dma2d_copy_framebuffer(uint32_t srcAddr, uint32_t dstAddr);

// ============================================================
// SECTION 9 : OPÉRATIONS DE BLENDING
// ============================================================

/**
 * @brief Effectue un alpha blending entre deux images
 * @param config Configuration du blending
 * @return true si succès
 */
bool dma2d_blend(const DMA2D_BlendConfig* config);

/**
 * @brief Affiche une image avec transparence
 * @param fgAddr Adresse de l'image foreground
 * @param fgWidth Largeur foreground
 * @param fgHeight Hauteur foreground
 * @param dstX Position X destination
 * @param dstY Position Y destination
 * @param alpha Transparence (0-255)
 * @return true si succès
 */
bool dma2d_blend_image(uint32_t fgAddr, uint16_t fgWidth, uint16_t fgHeight,
                        uint16_t dstX, uint16_t dstY, uint8_t alpha);

// ============================================================
// SECTION 10 : OPÉRATIONS D'IMAGES
// ============================================================

/**
 * @brief Affiche une image (bitmap) dans le framebuffer
 * @param config Configuration de l'image
 * @return true si succès
 */
bool dma2d_draw_image(const DMA2D_ImageConfig* config);

/**
 * @brief Affiche une icône (petite image)
 * @param iconData Données de l'icône (RGB565)
 * @param iconWidth Largeur icône
 * @param iconHeight Hauteur icône
 * @param x Position X
 * @param y Position Y
 * @return true si succès
 */
bool dma2d_draw_icon(const uint16_t* iconData, uint16_t iconWidth, uint16_t iconHeight,
                      uint16_t x, uint16_t y);

/**
 * @brief Affiche une image avec mise à l'échelle
 * @param srcAddr Adresse source
 * @param srcWidth Largeur source
 * @param srcHeight Hauteur source
 * @param dstX Position X destination
 * @param dstY Position Y destination
 * @param dstWidth Largeur destination
 * @param dstHeight Hauteur destination
 * @return true si succès
 * 
 * ⚠️ Le DMA2D ne supporte PAS le redimensionnement matériel.
 * Cette fonction utilise un algorithme logiciel simplifié.
 */
bool dma2d_draw_scaled_image(uint32_t srcAddr, 
                              uint16_t srcWidth, uint16_t srcHeight,
                              uint16_t dstX, uint16_t dstY,
                              uint16_t dstWidth, uint16_t dstHeight);

// ============================================================
// SECTION 11 : OPÉRATIONS DE CONVERSION DE FORMAT
// ============================================================

/**
 * @brief Convertit un buffer d'un format à un autre
 * @param srcAddr Adresse source
 * @param dstAddr Adresse destination
 * @param width Largeur
 * @param height Hauteur
 * @param srcFormat Format source
 * @param dstFormat Format destination
 * @return true si succès
 */
bool dma2d_convert_format(uint32_t srcAddr, uint32_t dstAddr,
                           uint16_t width, uint16_t height,
                           DMA2D_PixelFormat srcFormat,
                           DMA2D_PixelFormat dstFormat);

/**
 * @brief Convertit RGB888 en RGB565
 * @param src888 Buffer source RGB888
 * @param dst565 Buffer destination RGB565
 * @param pixelCount Nombre de pixels
 */
void dma2d_rgb888_to_rgb565(const uint8_t* src888, uint16_t* dst565, uint32_t pixelCount);

/**
 * @brief Convertit RGB565 en RGB888
 * @param src565 Buffer source RGB565
 * @param dst888 Buffer destination RGB888
 * @param pixelCount Nombre de pixels
 */
void dma2d_rgb565_to_rgb888(const uint16_t* src565, uint8_t* dst888, uint32_t pixelCount);

// ============================================================
// SECTION 12 : FONCTIONS UTILITAIRES
// ============================================================

/**
 * @brief Attend que l'opération DMA2D en cours se termine
 * @param timeoutMs Timeout en ms
 * @return true si terminé avant le timeout
 */
bool dma2d_wait_for_completion(uint32_t timeoutMs);

/**
 * @brief Vérifie si le DMA2D est occupé
 * @return true si une opération est en cours
 */
bool dma2d_is_busy(void);

/**
 * @brief Annule l'opération DMA2D en cours
 */
void dma2d_abort(void);

/**
 * @brief Récupère l'adresse du framebuffer principal
 * @return Adresse du framebuffer
 */
uint32_t dma2d_get_framebuffer_addr(void);

/**
 * @brief Définit l'adresse du framebuffer principal
 * @param addr Adresse (dans la SDRAM)
 */
void dma2d_set_framebuffer_addr(uint32_t addr);

// ============================================================
// SECTION 13 : FONCTIONS AVANCÉES
// ============================================================

/**
 * @brief Dessine une ligne horizontale (optimisé DMA2D)
 * @param x1 Début X
 * @param y Ligne Y
 * @param x2 Fin X
 * @param color Couleur RGB565
 */
void dma2d_draw_hline(uint16_t x1, uint16_t y, uint16_t x2, uint16_t color);

/**
 * @brief Dessine une ligne verticale (optimisé DMA2D)
 * @param x Colonne X
 * @param y1 Début Y
 * @param y2 Fin Y
 * @param color Couleur RGB565
 */
void dma2d_draw_vline(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color);

/**
 * @brief Remplit un rectangle arrondi (coins ronds)
 * @param x1 Début X
 * @param y1 Début Y
 * @param x2 Fin X
 * @param y2 Fin Y
 * @param radius Rayon des coins
 * @param color Couleur RGB565
 */
void dma2d_fill_round_rect(uint16_t x1, uint16_t y1,
                            uint16_t x2, uint16_t y2,
                            uint16_t radius, uint16_t color);

// ============================================================
// SECTION 14 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define DMA2D_DEBUG(fmt, ...)       printf("[DMA2D] " fmt, ##__VA_ARGS__)
#else
    #define DMA2D_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // DMA2D_DRIVER_H