/**
 * @file ltdc_config.h
 * @brief Configuration du contrôleur LTDC du STM32F429
 * 
 * Le LTDC (LCD-TFT Display Controller) est un contrôleur intégré
 * qui gère l'affichage sur un écran TFT en mode parallèle RGB.
 * 
 * Il génère automatiquement :
 * - Les signaux de synchronisation (HSYNC, VSYNC, DE)
 * - L'horloge pixel (PCLK)
 * - Le rafraîchissement continu de l'écran depuis le framebuffer
 * 
 * Cela libère complètement le CPU de la gestion de l'affichage.
 * 
 * Configuration pour ILI9488 en mode 16-bit RGB565 :
 * - Résolution : 320 × 480
 * - Format : RGB565 (16 bpp)
 * - Pixel Clock : 10 MHz
 * - Fréquence de rafraîchissement : ~57 Hz
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef LTDC_CONFIG_H
#define LTDC_CONFIG_H

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
#include "../../../include/platform.h"

// ============================================================
// SECTION 1 : CONSTANTES DE L'ÉCRAN
// ============================================================

/** @brief Largeur visible de l'écran */
#define LTDC_WIDTH                      320

/** @brief Hauteur visible de l'écran */
#define LTDC_HEIGHT                     480

/** @brief Format pixel : RGB565 (16 bits) */
#define LTDC_PIXEL_FORMAT               LTDC_PIXFORMAT_RGB565

/** @brief Nombre de bits par pixel */
#define LTDC_BITS_PER_PIXEL             16

/** @brief Taille d'un pixel en octets */
#define LTDC_BYTES_PER_PIXEL            2

// ============================================================
// SECTION 2 : PARAMÈTRES DE SYNCHRONISATION
// ============================================================

/**
 * @brief Paramètres de synchronisation horizontale
 * 
 * Structure d'une ligne :
 * ┌────────┬──────────┬──────────────────────┬───────────┐
 * │  HSYNC │ Back     │     Zone active      │  Front    │
 * │  pulse │ Porch    │     (320 pixels)     │  Porch    │
 * │ 10 px  │ 10 px    │                      │  20 px    │
 * └────────┴──────────┴──────────────────────┴───────────┘
 * ◄────────────────── 360 pixels total ──────────────────►
 */

/** @brief Largeur de l'impulsion HSYNC (en pixels) */
#define LTDC_HSYNC_WIDTH                10

/** @brief Back Porch horizontal (en pixels) */
#define LTDC_HORIZONTAL_BACK_PORCH      10

/** @brief Zone active horizontale (largeur visible) */
#define LTDC_ACTIVE_WIDTH               LTDC_WIDTH

/** @brief Front Porch horizontal (en pixels) */
#define LTDC_HORIZONTAL_FRONT_PORCH     20

/** @brief Total horizontal = HSYNC + BP + Active + FP */
#define LTDC_TOTAL_WIDTH                (LTDC_HSYNC_WIDTH + \
                                         LTDC_HORIZONTAL_BACK_PORCH + \
                                         LTDC_ACTIVE_WIDTH + \
                                         LTDC_HORIZONTAL_FRONT_PORCH)

/**
 * @brief Paramètres de synchronisation verticale
 * 
 * Structure d'une trame :
 * ┌────────┬──────────┬──────────────────────┬───────────┐
 * │  VSYNC │ Back     │     Zone active      │  Front    │
 * │  pulse │ Porch    │     (480 lignes)     │  Porch    │
 * │ 2 lgns │ 2 lgns   │                      │  1 ligne  │
 * └────────┴──────────┴──────────────────────┴───────────┘
 * ◄────────────────── 485 lignes total ───────────────────►
 */

/** @brief Hauteur de l'impulsion VSYNC (en lignes) */
#define LTDC_VSYNC_HEIGHT               2

/** @brief Back Porch vertical (en lignes) */
#define LTDC_VERTICAL_BACK_PORCH        2

/** @brief Zone active verticale (hauteur visible) */
#define LTDC_ACTIVE_HEIGHT              LTDC_HEIGHT

/** @brief Front Porch vertical (en lignes) */
#define LTDC_VERTICAL_FRONT_PORCH       1

/** @brief Total vertical = VSYNC + BP + Active + FP */
#define LTDC_TOTAL_HEIGHT               (LTDC_VSYNC_HEIGHT + \
                                         LTDC_VERTICAL_BACK_PORCH + \
                                         LTDC_ACTIVE_HEIGHT + \
                                         LTDC_VERTICAL_FRONT_PORCH)

// ============================================================
// SECTION 3 : CALCUL DE LA FRÉQUENCE PIXEL
// ============================================================

/**
 * @brief Fréquence pixel souhaitée (Hz)
 * 
 * Pour un rafraîchissement à ~60 Hz :
 * PCLK = Total_Width × Total_Height × Fréquence
 * PCLK = 360 × 485 × 60 = 10 476 000 Hz ≈ 10 MHz
 */
#define LTDC_PIXEL_CLOCK_TARGET         10000000    // 10 MHz

/**
 * @brief Fréquence de rafraîchissement résultante
 * 
 * Fréquence = PCLK / (Total_Width × Total_Height)
 * Fréquence = 10 000 000 / (360 × 485) = 57.2 Hz
 */
#define LTDC_REFRESH_RATE               57          // ~57 Hz

// ============================================================
// SECTION 4 : CONFIGURATION DE LA COUCHE (LAYER)
// ============================================================

/**
 * @brief Nombre de couches utilisées
 * 
 * Le LTDC supporte 2 couches matérielles.
 * On utilise la couche 1 pour le framebuffer principal.
 * La couche 2 peut être utilisée pour des overlays.
 */
#define LTDC_LAYER_COUNT                1

/**
 * @brief Couche utilisée pour l'interface principale
 */
#define LTDC_MAIN_LAYER                 0

/**
 * @brief Adresse du framebuffer dans la SDRAM
 * 
 * Le framebuffer est placé au début de la SDRAM
 * pour des performances optimales.
 */
#define LTDC_FRAMEBUFFER_ADDR           SDRAM_BASE_ADDR

/**
 * @brief Adresse du second framebuffer (double buffering)
 */
#define LTDC_FRAMEBUFFER2_ADDR          (SDRAM_BASE_ADDR + (LTDC_WIDTH * LTDC_HEIGHT * LTDC_BYTES_PER_PIXEL))

/**
 * @brief Taille du framebuffer en octets
 */
#define LTDC_FRAMEBUFFER_SIZE           (LTDC_WIDTH * LTDC_HEIGHT * LTDC_BYTES_PER_PIXEL)

/**
 * @brief Configuration de la fenêtre de la couche
 * 
 * La couche couvre tout l'écran.
 */
#define LTDC_LAYER_WINDOW_X1            0
#define LTDC_LAYER_WINDOW_Y1            0
#define LTDC_LAYER_WINDOW_X2            (LTDC_WIDTH - 1)
#define LTDC_LAYER_WINDOW_Y2            (LTDC_HEIGHT - 1)

/**
 * @brief Pas de ligne (Line Pitch) en octets
 * 
 * Pour RGB565 : 320 pixels × 2 octets = 640 octets par ligne
 */
#define LTDC_LINE_PITCH                 (LTDC_WIDTH * LTDC_BYTES_PER_PIXEL)

/**
 * @brief Format de couleur de la couche
 */
#define LTDC_LAYER_PIXEL_FORMAT         LTDC_PIXFORMAT_RGB565

/**
 * @brief Couleur par défaut de la couche (noir transparent)
 */
#define LTDC_LAYER_DEFAULT_COLOR        0x00000000

/**
 * @brief Constante alpha par défaut (opaque)
 */
#define LTDC_LAYER_DEFAULT_ALPHA        0xFF

/**
 * @brief Fond d'écran par défaut (noir)
 */
#define LTDC_DEFAULT_BACKGROUND         0x00000000

// ============================================================
// SECTION 5 : POLARITÉS DES SIGNAUX
// ============================================================

/**
 * @brief Polarité de l'horloge pixel
 * 
 * IPC = Input Pixel Clock
 * - LTDC_POLARITY_IPC : Rising edge (front montant)
 * - LTDC_POLARITY_IIPC : Falling edge (front descendant)
 */
#define LTDC_PCLK_POLARITY              LTDC_POLARITY_IPC

/**
 * @brief Polarité du signal HSYNC
 * 
 * AL = Active Low (actif bas)
 * AH = Active High (actif haut)
 */
#define LTDC_HSYNC_POLARITY             LTDC_HSPOLARITY_AL

/**
 * @brief Polarité du signal VSYNC
 */
#define LTDC_VSYNC_POLARITY             LTDC_VSPOLARITY_AL

/**
 * @brief Polarité du signal Data Enable (DE)
 */
#define LTDC_DE_POLARITY                LTDC_DEPOLARITY_AL

// ============================================================
// SECTION 6 : BROCHES LTDC (MAPPING GPIO)
// ============================================================

/**
 * @name Broches du bus de données LTDC
 * 
 * Le STM32F429 utilise l'Alternate Function AF14 pour le LTDC.
 * Configuration RGB565 (16 bits) :
 * - Rouge : 5 bits (R4-R0)
 * - Vert  : 6 bits (G5-G0)
 * - Bleu  : 5 bits (B4-B0)
 * @{
 */

/** @brief Broches Rouge (5 bits : R4..R0) */
#define LTDC_R4_PORT                    GPIOE
#define LTDC_R4_PIN                     GPIO_PIN_12
#define LTDC_R3_PORT                    GPIOE
#define LTDC_R3_PIN                     GPIO_PIN_13
#define LTDC_R2_PORT                    GPIOE
#define LTDC_R2_PIN                     GPIO_PIN_14
#define LTDC_R1_PORT                    GPIOE
#define LTDC_R1_PIN                     GPIO_PIN_15
#define LTDC_R0_PORT                    GPIOD
#define LTDC_R0_PIN                     GPIO_PIN_9

/** @brief Broches Vert (6 bits : G5..G0) */
#define LTDC_G5_PORT                    GPIOE
#define LTDC_G5_PIN                     GPIO_PIN_9
#define LTDC_G4_PORT                    GPIOE
#define LTDC_G4_PIN                     GPIO_PIN_8
#define LTDC_G3_PORT                    GPIOE
#define LTDC_G3_PIN                     GPIO_PIN_7
#define LTDC_G2_PORT                    GPIOD
#define LTDC_G2_PIN                     GPIO_PIN_1
#define LTDC_G1_PORT                    GPIOD
#define LTDC_G1_PIN                     GPIO_PIN_0
#define LTDC_G0_PORT                    GPIOD
#define LTDC_G0_PIN                     GPIO_PIN_15

/** @brief Broches Bleu (5 bits : B4..B0) */
#define LTDC_B4_PORT                    GPIOG
#define LTDC_B4_PIN                     GPIO_PIN_12
#define LTDC_B3_PORT                    GPIOG
#define LTDC_B3_PIN                     GPIO_PIN_11
#define LTDC_B2_PORT                    GPIOG
#define LTDC_B2_PIN                     GPIO_PIN_10
#define LTDC_B1_PORT                    GPIOB
#define LTDC_B1_PIN                     GPIO_PIN_9
#define LTDC_B0_PORT                    GPIOB
#define LTDC_B0_PIN                     GPIO_PIN_8

/** @brief Broches de contrôle */
#define LTDC_CLK_PORT                   GPIOE
#define LTDC_CLK_PIN                    GPIO_PIN_0
#define LTDC_HSYNC_PORT                 GPIOE
#define LTDC_HSYNC_PIN                  GPIO_PIN_1
#define LTDC_VSYNC_PORT                 GPIOE
#define LTDC_VSYNC_PIN                  GPIO_PIN_2
#define LTDC_DE_PORT                    GPIOE
#define LTDC_DE_PIN                     GPIO_PIN_3

/** @} */ // Fin broches LTDC

// ============================================================
// SECTION 7 : STRUCTURES DE CONFIGURATION
// ============================================================

/**
 * @brief Structure de configuration complète du LTDC
 */
typedef struct {
    // Paramètres de synchronisation
    uint16_t hsyncWidth;
    uint16_t hBackPorch;
    uint16_t hFrontPorch;
    uint16_t vsyncHeight;
    uint16_t vBackPorch;
    uint16_t vFrontPorch;
    
    // Zone active
    uint16_t activeWidth;
    uint16_t activeHeight;
    uint16_t totalWidth;
    uint16_t totalHeight;
    
    // Polarités
    uint32_t hsyncPolarity;
    uint32_t vsyncPolarity;
    uint32_t dePolarity;
    uint32_t pclkPolarity;
    
    // Fond d'écran
    uint32_t backgroundColor;
    
    // Couche 1
    uint32_t layer1Framebuffer;
    uint16_t layer1WindowX1;
    uint16_t layer1WindowY1;
    uint16_t layer1WindowX2;
    uint16_t layer1WindowY2;
    uint32_t layer1PixelFormat;
    uint32_t layer1DefaultColor;
    uint8_t  layer1Alpha;
    uint32_t layer1LinePitch;
    
    // Fréquence
    uint32_t pixelClockTarget;
    uint32_t refreshRate;
    
} LTDC_Config;

/**
 * @brief Configuration LTDC par défaut pour ILI9488
 */
extern const LTDC_Config ltdc_default_config;

/**
 * @brief Handle LTDC (utilisé par le HAL)
 */
extern LTDC_HandleTypeDef hltdc;

// ============================================================
// SECTION 8 : FONCTIONS DE CONFIGURATION
// ============================================================

/**
 * @brief Initialise le contrôleur LTDC
 * 
 * Configure :
 * - Les broches GPIO en Alternate Function AF14
 * - Les timings de synchronisation
 * - La couche 1 avec le framebuffer
 * - L'horloge pixel
 * 
 * @return true si l'initialisation réussit
 */
bool ltdc_init(void);

/**
 * @brief Applique une configuration LTDC
 * @param config Pointeur vers la configuration
 * @return true si succès
 */
bool ltdc_configure(const LTDC_Config* config);

/**
 * @brief Réinitialise le LTDC avec la configuration par défaut
 */
void ltdc_reset(void);

/**
 * @brief Définit l'adresse du framebuffer
 * @param addr Adresse du framebuffer (dans la SDRAM)
 */
void ltdc_set_framebuffer(uint32_t addr);

/**
 * @brief Bascule vers le second framebuffer (double buffering)
 * 
 * Change l'adresse du framebuffer actif pendant
 * la période de blanking vertical pour éviter le tearing.
 */
void ltdc_swap_framebuffer(void);

/**
 * @brief Active/désactive une couche
 * @param layer Numéro de couche (0 ou 1)
 * @param enable true pour activer
 */
void ltdc_enable_layer(uint8_t layer, bool enable);

/**
 * @brief Définit la transparence d'une couche
 * @param layer Numéro de couche
 * @param alpha Valeur alpha (0=transparent, 255=opaque)
 */
void ltdc_set_layer_alpha(uint8_t layer, uint8_t alpha);

/**
 * @brief Définit la position d'une couche
 * @param layer Numéro de couche
 * @param x1 Colonne de début
 * @param y1 Ligne de début
 * @param x2 Colonne de fin
 * @param y2 Ligne de fin
 */
void ltdc_set_layer_window(uint8_t layer, uint16_t x1, uint16_t y1, 
                            uint16_t x2, uint16_t y2);

/**
 * @brief Active/désactive le LTDC
 * @param enable true pour activer
 */
void ltdc_enable(bool enable);

/**
 * @brief Vérifie si le LTDC est initialisé
 * @return true si prêt
 */
bool ltdc_is_ready(void);

// ============================================================
// SECTION 9 : FONCTIONS DE DOUBLE BUFFERING
// ============================================================

/**
 * @brief Active le double buffering
 * 
 * Utilise deux framebuffers :
 * - Framebuffer 1 : affiché à l'écran
 * - Framebuffer 2 : dessin en cours
 * 
 * L'échange se fait pendant le blanking vertical.
 */
void ltdc_double_buffering_enable(void);

/**
 * @brief Désactive le double buffering
 */
void ltdc_double_buffering_disable(void);

/**
 * @brief Récupère l'adresse du framebuffer de dessin
 * 
 * Retourne l'adresse du framebuffer qui n'est PAS
 * actuellement affiché.
 * 
 * @return uint32_t Adresse du back buffer
 */
uint32_t ltdc_get_back_buffer(void);

/**
 * @brief Récupère l'adresse du framebuffer affiché
 * @return uint32_t Adresse du front buffer
 */
uint32_t ltdc_get_front_buffer(void);

// ============================================================
// SECTION 10 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche la configuration LTDC actuelle
 */
void ltdc_print_config(void);

/**
 * @brief Affiche les statistiques LTDC
 */
void ltdc_print_statistics(void);

/**
 * @brief Vérifie la présence d'un écran connecté
 * @return true si un écran est détecté
 */
bool ltdc_is_display_connected(void);

// ============================================================
// SECTION 11 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define LTDC_DEBUG(fmt, ...)        printf("[LTDC] " fmt, ##__VA_ARGS__)
#else
    #define LTDC_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 12 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // LTDC_CONFIG_H