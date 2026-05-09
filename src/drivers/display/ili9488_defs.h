/**
 * @file ili9488_defs.h
 * @brief Définitions complètes pour le contrôleur ILI9488
 * 
 * Ce fichier contient TOUTES les définitions nécessaires pour
 * contrôler l'écran TFT ILI9488 (et compatibles ILI9486/ILI9481) :
 * - Registres et commandes
 * - Paramètres de configuration
 * - Structures de données
 * - Constantes de timing
 * - Séquences d'initialisation
 * 
 * Référence : Datasheet ILI9488 - Rev. 1.0 - 2014
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef ILI9488_DEFS_H
#define ILI9488_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// SECTION 1 : IDENTIFICATION DU DRIVER
// ============================================================

/** @brief Version du driver */
#define ILI9488_DRIVER_VERSION          "1.0.0"

/** @brief Identifiants possibles du contrôleur */
#define ILI9488_ID_EXPECTED             0x9488
#define ILI9486_ID_EXPECTED             0x9486
#define ILI9481_ID_EXPECTED             0x9481

/** @brief Nombre max de tentatives de lecture d'ID */
#define ILI9488_ID_READ_RETRIES         3

// ============================================================
// SECTION 2 : DIMENSIONS DE L'ÉCRAN
// ============================================================

/** @brief Largeur native en pixels */
#define ILI9488_WIDTH                   320

/** @brief Hauteur native en pixels */
#define ILI9488_HEIGHT                  480

/** @brief Nombre total de pixels */
#define ILI9488_PIXEL_COUNT             (ILI9488_WIDTH * ILI9488_HEIGHT)

/** @brief Taille d'un pixel en octets (RGB565) */
#define ILI9488_PIXEL_SIZE              2

/** @brief Taille du framebuffer en octets */
#define ILI9488_FRAMEBUFFER_SIZE        (ILI9488_PIXEL_COUNT * ILI9488_PIXEL_SIZE)

/** @brief Taille d'une ligne en octets */
#define ILI9488_LINE_SIZE               (ILI9488_WIDTH * ILI9488_PIXEL_SIZE)

// ============================================================
// SECTION 3 : FORMATS DE COULEURS (RGB565)
// ============================================================

/**
 * @brief Macro de conversion RGB888 → RGB565
 * @param r Rouge (0-255)
 * @param g Vert (0-255)  
 * @param b Bleu (0-255)
 * @return Valeur 16 bits RGB565
 */
#define ILI9488_RGB(r, g, b)            ((((uint16_t)(r) & 0xF8) << 8) | \
                                         (((uint16_t)(g) & 0xFC) << 3) | \
                                         (((uint16_t)(b) & 0xF8) >> 3))

/** @brief Extraction composante rouge (0-31) */
#define ILI9488_RED(color)              (((color) >> 11) & 0x1F)

/** @brief Extraction composante verte (0-63) */
#define ILI9488_GREEN(color)            (((color) >> 5) & 0x3F)

/** @brief Extraction composante bleue (0-31) */
#define ILI9488_BLUE(color)             ((color) & 0x1F)

/** @brief Conversion RGB565 → RGB888 (rouge) */
#define ILI9488_RED888(color)           (((color) >> 8) & 0xF8)

/** @brief Conversion RGB565 → RGB888 (vert) */
#define ILI9488_GREEN888(color)         (((color) >> 3) & 0xFC)

/** @brief Conversion RGB565 → RGB888 (bleu) */
#define ILI9488_BLUE888(color)          (((color) << 3) & 0xF8)

// ============================================================
// SECTION 4 : PALETTE DE COULEURS PRÉDÉFINIES
// ============================================================

/** @name Couleurs de base */
/** @{ */
#define ILI9488_BLACK                   0x0000
#define ILI9488_WHITE                   0xFFFF
#define ILI9488_RED                     0xF800
#define ILI9488_GREEN                   0x07E0
#define ILI9488_BLUE                    0x001F
/** @} */

/** @name Couleurs secondaires */
/** @{ */
#define ILI9488_YELLOW                  0xFFE0
#define ILI9488_CYAN                    0x07FF
#define ILI9488_MAGENTA                 0xF81F
#define ILI9488_ORANGE                  0xFD20
#define ILI9488_PURPLE                  0x780F
#define ILI9488_PINK                    0xF81F
/** @} */

/** @name Nuances de gris */
/** @{ */
#define ILI9488_GRAY_10                 0x18E3
#define ILI9488_GRAY_20                 0x3186
#define ILI9488_GRAY_30                 0x4A49
#define ILI9488_GRAY_40                 0x632C
#define ILI9488_GRAY_50                 0x7BEF
#define ILI9488_GRAY_60                 0x94B2
#define ILI9488_GRAY_70                 0xAD75
#define ILI9488_GRAY_80                 0xC638
#define ILI9488_GRAY_90                 0xDEDB
#define ILI9488_GRAY                    0x8410
#define ILI9488_DARK_GRAY               0x4208
#define ILI9488_LIGHT_GRAY              0xC618
/** @} */

/** @name Couleurs UI */
/** @{ */
#define ILI9488_NAVY                    0x0010
#define ILI9488_DARK_GREEN              0x03E0
#define ILI9488_DARK_CYAN               0x03EF
#define ILI9488_MAROON                  0x7800
#define ILI9488_OLIVE                   0x7BE0
#define ILI9488_SILVER                  0xC618
#define ILI9488_TEAL                    0x0410
#define ILI9488_BROWN                   0xA145
#define ILI9488_GOLD                    0xFEA0
#define ILI9488_SKY_BLUE                0x867D
#define ILI9488_LIME                    0x07E0
#define ILI9488_INDIGO                  0x4810
#define ILI9488_VIOLET                  0x801F
#define ILI9488_CORAL                   0xFBEA
#define ILI9488_TURQUOISE               0x471A
#define ILI9488_SALMON                  0xFC0E
/** @} */

/** @name Couleurs thème téléphone */
/** @{ */
#define ILI9488_BG_DARK                 0x18E3      // Fond sombre
#define ILI9488_BG_LIGHT                0xDEDB      // Fond clair
#define ILI9488_PRIMARY                 0x07E0      // Couleur principale (vert)
#define ILI9488_SECONDARY               0x5AEB      // Couleur secondaire
#define ILI9488_ACCENT                  0x001F      // Accentuation (bleu)
#define ILI9488_TEXT_PRIMARY            0xFFFF      // Texte principal (blanc)
#define ILI9488_TEXT_SECONDARY          0x8410      // Texte secondaire (gris)
#define ILI9488_BUTTON_CALL             0x07E0      // Bouton appeler (vert)
#define ILI9488_BUTTON_END              0xF800      // Bouton raccrocher (rouge)
#define ILI9488_BUTTON_MUTE             0x5B8D      // Bouton muet
#define ILI9488_STATUS_BAR              0x2104      // Barre de statut
#define ILI9488_NAV_BAR                 0x2104      // Barre de navigation
/** @} */

// ============================================================
// SECTION 5 : COMMANDES DU ILI9488 (COMPLET)
// ============================================================

/**
 * @name Commandes système (0x00-0x0F)
 * @{
 */
#define ILI9488_NOP                     0x00
#define ILI9488_SOFT_RESET              0x01
#define ILI9488_READ_ID                 0x04
#define ILI9488_READ_STATUS             0x09
#define ILI9488_READ_POWER_MODE         0x0A
#define ILI9488_READ_MADCTL             0x0B
#define ILI9488_READ_PIXEL_FORMAT       0x0C
#define ILI9488_READ_IMAGE_FORMAT       0x0D
#define ILI9488_READ_SIGNAL_MODE        0x0E
#define ILI9488_READ_SELF_DIAG          0x0F
/** @} */

/**
 * @name Commandes de power management (0x10-0x1F)
 * @{
 */
#define ILI9488_ENTER_SLEEP             0x10
#define ILI9488_SLEEP_OUT               0x11
#define ILI9488_PARTIAL_MODE_ON         0x12
#define ILI9488_NORMAL_MODE_ON          0x13
#define ILI9488_DISPLAY_INVERSION_OFF   0x20
#define ILI9488_DISPLAY_INVERSION_ON    0x21
/** @} */

/**
 * @name Commandes d'affichage (0x20-0x2F)
 * @{
 */
#define ILI9488_DISPLAY_OFF             0x28
#define ILI9488_DISPLAY_ON              0x29
#define ILI9488_COLUMN_ADDRESS_SET      0x2A
#define ILI9488_PAGE_ADDRESS_SET        0x2B
#define ILI9488_MEMORY_WRITE            0x2C
#define ILI9488_MEMORY_READ             0x2E
/** @} */

/**
 * @name Commandes de configuration mémoire (0x30-0x3F)
 * @{
 */
#define ILI9488_PARTIAL_AREA            0x30
#define ILI9488_VERT_SCROLL_DEFINITION  0x33
#define ILI9488_SOFTWARE_RESET          0x01
#define ILI9488_TEARING_EFFECT_OFF      0x34
#define ILI9488_TEARING_EFFECT_ON       0x35
#define ILI9488_MEMORY_ACCESS_CONTROL   0x36
#define ILI9488_VERT_SCROLL_START_ADDR  0x37
#define ILI9488_IDLE_MODE_OFF           0x38
#define ILI9488_IDLE_MODE_ON            0x39
#define ILI9488_PIXEL_FORMAT_SET        0x3A
/** @} */

/**
 * @name Commandes de contrôle avancé (0x50-0x5F)
 * @{
 */
#define ILI9488_WRITE_DISPLAY_BRIGHTNESS   0x51
#define ILI9488_READ_DISPLAY_BRIGHTNESS    0x52
#define ILI9488_WRITE_CTRL_DISPLAY         0x53
#define ILI9488_READ_CTRL_DISPLAY          0x54
#define ILI9488_WRITE_CONTENT_ADAPTIVE     0x55
#define ILI9488_READ_CONTENT_ADAPTIVE      0x56
#define ILI9488_WRITE_CABC_MIN_BRIGHTNESS  0x5E
#define ILI9488_READ_CABC_MIN_BRIGHTNESS   0x5F
/** @} */

/**
 * @name Commandes de gamma (0xE0-0xEF)
 * @{
 */
#define ILI9488_GAMMA_SET               0x26
#define ILI9488_POSITIVE_GAMMA_CORRECT  0xE0
#define ILI9488_NEGATIVE_GAMMA_CORRECT  0xE1
#define ILI9488_DIGITAL_GAMMA_CONTROL1  0xE2
#define ILI9488_DIGITAL_GAMMA_CONTROL2  0xE3
/** @} */

/**
 * @name Commandes d'alimentation (0xC0-0xCF)
 * @{
 */
#define ILI9488_POWER_CONTROL_1         0xC0
#define ILI9488_POWER_CONTROL_2         0xC1
#define ILI9488_POWER_CONTROL_3         0xC2
#define ILI9488_POWER_CONTROL_4         0xC3
#define ILI9488_VCOM_CONTROL_1          0xC5
#define ILI9488_VCOM_CONTROL_2          0xC7
/** @} */

/**
 * @name Commandes diverses
 * @{
 */
#define ILI9488_POWER_ON_SEQUENCE       0xED
#define ILI9488_DRIVER_TIMING_CONTROL   0xE8
#define ILI9488_DRIVER_OUTPUT_CONTROL   0xF2
#define ILI9488_INTERFACE_CONTROL       0xF6
#define ILI9488_PUMP_RATIO_CONTROL      0xF7
/** @} */

// ============================================================
// SECTION 6 : BITS DE CONTRÔLE MADCTL (REGISTRE 0x36)
// ============================================================

/**
 * @name Bits du registre Memory Access Control
 * @{
 */
#define MADCTL_MY                       0x80    // Mirror Y (inversion verticale)
#define MADCTL_MX                       0x40    // Mirror X (inversion horizontale)
#define MADCTL_MV                       0x20    // Échange X/Y (rotation)
#define MADCTL_ML                       0x10    // Mirror L
#define MADCTL_BGR                      0x08    // Ordre BGR (sinon RGB)
#define MADCTL_MH                       0x04    // Mirror H
#define MADCTL_SS                       0x02    // Sélection horizontale
#define MADCTL_GS                       0x01    // Sélection verticale
/** @} */

/**
 * @brief Rotations prédéfinies (combinaisons MADCTL)
 */
typedef enum {
    ILI9488_ROTATION_PORTRAIT          = (MADCTL_MX | MADCTL_BGR),
    ILI9488_ROTATION_LANDSCAPE         = (MADCTL_MV | MADCTL_BGR),
    ILI9488_ROTATION_PORTRAIT_INVERTED = (MADCTL_MY | MADCTL_BGR),
    ILI9488_ROTATION_LANDSCAPE_INVERTED= (MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR)
} ILI9488_Rotation;

// ============================================================
// SECTION 7 : FORMATS DE PIXELS (REGISTRE 0x3A)
// ============================================================

/**
 * @name Formats de pixel supportés
 * @{
 */
#define ILI9488_DPI_16BPP               0x55    // 16 bits/pixel (RGB565)
#define ILI9488_DPI_18BPP               0x66    // 18 bits/pixel (RGB666)
#define ILI9488_DPI_24BPP               0x77    // 24 bits/pixel (RGB888)
#define ILI9488_DPI_3BPP                0x01    // 3 bits/pixel (8 couleurs)
#define ILI9488_DPI_6BPP                0x06    // 6 bits/pixel (64 couleurs)
#define ILI9488_DPI_12BPP               0x03    // 12 bits/pixel (4096 couleurs)
/** @} */

// ============================================================
// SECTION 8 : PARAMÈTRES DE CONTRÔLE D'AFFICHAGE (REGISTRE 0x53)
// ============================================================

/**
 * @name Bits du registre CTRL_DISPLAY
 * @{
 */
#define CTRL_BRIGHTNESS_CTRL_ON         (1 << 5)    // Contrôle luminosité activé
#define CTRL_DISPLAY_DIMMING_ON         (1 << 4)    // Dimming activé
#define CTRL_BRIGHTNESS_CTRL_OFF        (0 << 5)
#define CTRL_DISPLAY_DIMMING_OFF        (0 << 4)
/** @} */

// ============================================================
// SECTION 9 : STRUCTURES DE DONNÉES
// ============================================================

/**
 * @brief Types de contrôleurs supportés
 */
typedef enum {
    ILI9488_CONTROLLER_UNKNOWN  = 0,
    ILI9488_CONTROLLER_9488     = 1,
    ILI9488_CONTROLLER_9486     = 2,
    ILI9488_CONTROLLER_9481     = 3
} ILI9488_ControllerType;

/**
 * @brief Structure d'information du contrôleur
 */
typedef struct {
    ILI9488_ControllerType controllerType;  // Type détecté
    uint32_t deviceId;                      // ID du dispositif
    uint8_t madctl;                         // Configuration MADCTL actuelle
    uint8_t pixelFormat;                    // Format pixel actuel
    uint16_t activeWidth;                   // Largeur active
    uint16_t activeHeight;                  // Hauteur active
    bool initialized;                       // Module initialisé
    bool displayOn;                         // Écran allumé
    bool sleeping;                          // En veille
    uint8_t brightness;                     // Luminosité (0-255)
    ILI9488_Rotation rotation;              // Rotation actuelle
    uint32_t frameCount;                    // Compteur de trames
} ILI9488_Info;

/**
 * @brief Structure de calibration tactile (si tactile intégré)
 */
typedef struct {
    uint16_t x_min;         // X minimum (valeur ADC)
    uint16_t x_max;         // X maximum
    uint16_t y_min;         // Y minimum
    uint16_t y_max;         // Y maximum
    uint16_t width;         // Largeur écran
    uint16_t height;        // Hauteur écran
    bool calibrated;        // Calibration effectuée
} ILI9488_TouchCalibration;

// ============================================================
// SECTION 10 : SÉQUENCE D'INITIALISATION
// ============================================================

/**
 * @brief Nombre de commandes dans la séquence d'initialisation
 */
#define ILI9488_INIT_SEQUENCE_SIZE       30

/**
 * @brief Structure d'une commande d'initialisation
 */
typedef struct {
    uint8_t command;            // Code de la commande
    uint8_t dataLength;         // Nombre d'octets de données
    const uint8_t* data;        // Pointeur vers les données (NULL si pas de données)
    uint32_t delayMs;           // Délai après la commande (ms)
} ILI9488_InitCommand;

/**
 * @brief Tableau de la séquence d'initialisation par défaut
 * 
 * Cette séquence est basée sur les recommandations du fabricant.
 * Elle configure les tensions, le gamma, et l'affichage.
 */
extern const ILI9488_InitCommand ili9488_init_sequence[];

/**
 * @brief Taille de la séquence d'initialisation
 */
extern const uint8_t ili9488_init_sequence_size;

// ============================================================
// SECTION 11 : PARAMÈTRES DE TIMING
// ============================================================

/** @brief Délai après reset matériel (ms) */
#define ILI9488_RESET_DELAY_MS          120

/** @brief Délai après reset logiciel (ms) */
#define ILI9488_SOFT_RESET_DELAY_MS     120

/** @brief Délai après sortie de veille (ms) */
#define ILI9488_SLEEP_OUT_DELAY_MS      120

/** @brief Délai après changement de mode (ms) */
#define ILI9488_MODE_CHANGE_DELAY_MS    20

/** @brief Timeout par défaut (ms) */
#define ILI9488_DEFAULT_TIMEOUT_MS      5000

/** @brief Nombre maximum de tentatives d'initialisation */
#define ILI9488_MAX_INIT_RETRIES        3

// ============================================================
// SECTION 12 : MACROS DE CONTRÔLE DES BROCHES
// ============================================================

/**
 * @name Macros de contrôle du bus parallèle
 * @{
 */

/** @brief Chip Select - Actif bas */
#define ILI9488_CS_LOW()        HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_RESET)
#define ILI9488_CS_HIGH()       HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET)

/** @brief Register Select (D/C) - LOW=Commande, HIGH=Donnée */
#define ILI9488_RS_COMMAND()    HAL_GPIO_WritePin(TFT_RS_PORT, TFT_RS_PIN, GPIO_PIN_RESET)
#define ILI9488_RS_DATA()       HAL_GPIO_WritePin(TFT_RS_PORT, TFT_RS_PIN, GPIO_PIN_SET)

/** @brief Write Strobe - Front descendant pour écrire */
#define ILI9488_WR_LOW()        HAL_GPIO_WritePin(TFT_WR_PORT, TFT_WR_PIN, GPIO_PIN_RESET)
#define ILI9488_WR_HIGH()       HAL_GPIO_WritePin(TFT_WR_PORT, TFT_WR_PIN, GPIO_PIN_SET)
#define ILI9488_WR_PULSE()      do { ILI9488_WR_LOW(); __NOP(); ILI9488_WR_HIGH(); } while(0)

/** @brief Read Strobe - Front montant pour lire */
#define ILI9488_RD_LOW()        HAL_GPIO_WritePin(TFT_RD_PORT, TFT_RD_PIN, GPIO_PIN_RESET)
#define ILI9488_RD_HIGH()       HAL_GPIO_WritePin(TFT_RD_PORT, TFT_RD_PIN, GPIO_PIN_SET)

/** @brief Reset */
#define ILI9488_RESET_LOW()     HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_RESET)
#define ILI9488_RESET_HIGH()    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET)

/** @brief Backlight */
#define ILI9488_BL_ON()         HAL_GPIO_WritePin(TFT_BL_PORT, TFT_BL_PIN, GPIO_PIN_SET)
#define ILI9488_BL_OFF()        HAL_GPIO_WritePin(TFT_BL_PORT, TFT_BL_PIN, GPIO_PIN_RESET)

/** @} */

// ============================================================
// SECTION 13 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define ILI9488_DEBUG(fmt, ...)     printf("[ILI9488] " fmt, ##__VA_ARGS__)
    #define ILI9488_DEBUG_CMD(cmd)      printf("[ILI9488] CMD: 0x%02X\n", cmd)
    #define ILI9488_DEBUG_DATA(data)    printf("[ILI9488] DATA: 0x%02X\n", data)
#else
    #define ILI9488_DEBUG(fmt, ...)
    #define ILI9488_DEBUG_CMD(cmd)
    #define ILI9488_DEBUG_DATA(data)
#endif

// ============================================================
// SECTION 14 : CODES D'ERREUR
// ============================================================

/**
 * @brief Codes d'erreur du driver ILI9488
 */
typedef enum {
    ILI9488_OK                  = 0,    // Succès
    ILI9488_ERROR_INIT          = -1,   // Échec initialisation
    ILI9488_ERROR_ID            = -2,   // ID incorrect
    ILI9488_ERROR_TIMEOUT       = -3,   // Timeout
    ILI9488_ERROR_PARAM         = -4,   // Paramètre invalide
    ILI9488_ERROR_BUS           = -5,   // Erreur de bus
    ILI9488_ERROR_NOT_READY     = -6,   // Écran pas prêt
    ILI9488_ERROR_MEMORY        = -7    // Erreur mémoire
} ILI9488_Error;

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // ILI9488_DEFS_H