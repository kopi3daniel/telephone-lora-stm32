/**
 * @file ili9488_driver.h
 * @brief Driver bas niveau pour l'écran TFT ILI9488
 * 
 * Ce fichier contient :
 * - Les commandes et registres du contrôleur ILI9488
 * - Les constantes de configuration (timings, dimensions)
 * - Les fonctions d'initialisation et de contrôle bas niveau
 * - La gestion du bus parallèle 16-bit
 * 
 * Caractéristiques de l'écran :
 * - Résolution : 320 × 480 pixels
 * - Format : RGB565 (16 bits par pixel)
 * - Interface : Parallèle 16-bit 8080
 * - Driver : ILI9488 (ou compatible ILI9486/ILI9481)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef ILI9488_DRIVER_H
#define ILI9488_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "../../config.h"
#include "../../../include/project_config.h"

// ============================================================
// SECTION 1 : DIMENSIONS DE L'ÉCRAN
// ============================================================

/** @brief Largeur de l'écran en pixels */
#define ILI9488_WIDTH                   320

/** @brief Hauteur de l'écran en pixels */
#define ILI9488_HEIGHT                  480

/** @brief Nombre total de pixels */
#define ILI9488_TOTAL_PIXELS            (ILI9488_WIDTH * ILI9488_HEIGHT)

/** @brief Taille d'un pixel en octets (RGB565 = 2 octets) */
#define ILI9488_BYTES_PER_PIXEL         2

/** @brief Taille du framebuffer en octets */
#define ILI9488_FRAMEBUFFER_SIZE        (ILI9488_TOTAL_PIXELS * ILI9488_BYTES_PER_PIXEL)

// ============================================================
// SECTION 2 : FORMATS DE COULEURS
// ============================================================

/**
 * @brief Conversion RGB888 → RGB565
 * @param r Rouge (0-255)
 * @param g Vert (0-255)
 * @param b Bleu (0-255)
 * @return Couleur 16 bits RGB565
 */
#define ILI9488_RGB565(r, g, b)         ((((r) & 0xF8) << 8) | \
                                         (((g) & 0xFC) << 3) | \
                                         (((b) & 0xF8) >> 3))

/**
 * @brief Extraction des composantes d'une couleur RGB565
 */
#define ILI9488_RGB565_RED(color)       (((color) >> 11) & 0x1F)
#define ILI9488_RGB565_GREEN(color)     (((color) >> 5) & 0x3F)
#define ILI9488_RGB565_BLUE(color)      ((color) & 0x1F)

/**
 * @brief Couleurs prédéfinies (RGB565)
 */
#define ILI9488_COLOR_BLACK             0x0000
#define ILI9488_COLOR_WHITE             0xFFFF
#define ILI9488_COLOR_RED               0xF800
#define ILI9488_COLOR_GREEN             0x07E0
#define ILI9488_COLOR_BLUE              0x001F
#define ILI9488_COLOR_YELLOW            0xFFE0
#define ILI9488_COLOR_CYAN              0x07FF
#define ILI9488_COLOR_MAGENTA           0xF81F
#define ILI9488_COLOR_GRAY              0x8410
#define ILI9488_COLOR_DARK_GRAY         0x4208
#define ILI9488_COLOR_LIGHT_GRAY        0xC618
#define ILI9488_COLOR_ORANGE            0xFD20
#define ILI9488_COLOR_NAVY              0x0010
#define ILI9488_COLOR_DARK_GREEN        0x03E0
#define ILI9488_COLOR_MAROON            0x7800
#define ILI9488_COLOR_PURPLE            0x780F
#define ILI9488_COLOR_OLIVE             0x7BE0
#define ILI9488_COLOR_SILVER            0xC618

// ============================================================
// SECTION 3 : COMMANDES DU ILI9488
// ============================================================

/**
 * @name Commandes système
 * @{
 */
#define ILI9488_CMD_NOP                 0x00    // Pas d'opération
#define ILI9488_CMD_SOFT_RESET          0x01    // Reset logiciel
#define ILI9488_CMD_READ_DISPLAY_ID     0x04    // Lire identifiant
#define ILI9488_CMD_READ_DISPLAY_STATUS 0x09    // Lire statut
#define ILI9488_CMD_READ_DISPLAY_POWER  0x0A    // Lire mode puissance
#define ILI9488_CMD_READ_DISPLAY_MADCTL 0x0B    // Lire MADCTL
#define ILI9488_CMD_READ_DISPLAY_PIXFMT 0x0C    // Lire format pixel
#define ILI9488_CMD_READ_DISPLAY_IMG    0x0D    // Lire format image
#define ILI9488_CMD_READ_DISPLAY_SIGNAL 0x0E    // Lire mode signal
#define ILI9488_CMD_READ_DISPLAY_DIAG   0x0F    // Lire diagnostic
/** @} */

/**
 * @name Commandes de configuration d'affichage
 * @{
 */
#define ILI9488_CMD_SLEEP_IN            0x10    // Entrer en veille
#define ILI9488_CMD_SLEEP_OUT           0x11    // Sortir de veille
#define ILI9488_CMD_PARTIAL_MODE_ON     0x12    // Mode partiel ON
#define ILI9488_CMD_NORMAL_MODE_ON      0x13    // Mode normal ON
#define ILI9488_CMD_DISPLAY_INVERSION   0x20    // Inversion affichage
#define ILI9488_CMD_DISPLAY_ON          0x29    // Allumer l'écran
#define ILI9488_CMD_DISPLAY_OFF         0x28    // Éteindre l'écran
/** @} */

/**
 * @name Commandes de fenêtre d'affichage
 * @{
 */
#define ILI9488_CMD_COLUMN_ADDR_SET     0x2A    // Définir plage colonnes
#define ILI9488_CMD_PAGE_ADDR_SET       0x2B    // Définir plage lignes
#define ILI9488_CMD_MEMORY_WRITE        0x2C    // Écriture mémoire
#define ILI9488_CMD_MEMORY_READ         0x2E    // Lecture mémoire
#define ILI9488_CMD_MEMORY_ACCESS_CTRL  0x36    // Contrôle accès mémoire
#define ILI9488_CMD_PIXEL_FORMAT        0x3A    // Format pixel
/** @} */

/**
 * @name Commandes de couleurs
 * @{
 */
#define ILI9488_CMD_BRIGHTNESS          0x51    // Luminosité
#define ILI9488_CMD_CTRL_DISPLAY        0x53    // Contrôle affichage
#define ILI9488_CMD_CONTENT_ADAPTIVE    0x55    // Contrôle adaptatif
#define ILI9488_CMD_CABC_MIN_BRIGHTNESS 0x5E    // Luminosité min CABC
/** @} */

/**
 * @name Commandes de gamma
 * @{
 */
#define ILI9488_CMD_GAMMA_SET           0x26    // Configuration gamma
#define ILI9488_CMD_POS_GAMMA_CORRECT   0xE0    // Correction gamma positive
#define ILI9488_CMD_NEG_GAMMA_CORRECT   0xE1    // Correction gamma négative
/** @} */

/**
 * @name Commandes de contrôle d'alimentation
 * @{
 */
#define ILI9488_CMD_POWER_CTRL1         0xC0    // Contrôle puissance 1
#define ILI9488_CMD_POWER_CTRL2         0xC1    // Contrôle puissance 2
#define ILI9488_CMD_VCOM_CTRL1          0xC5    // Contrôle VCOM 1
#define ILI9488_CMD_VCOM_CTRL2          0xC7    // Contrôle VCOM 2
#define ILI9488_CMD_POWER_ON_SEQUENCE   0xED    // Séquence power-on
/** @} */

/**
 * @name Commandes de rotation et orientation
 * @{
 */
#define ILI9488_MADCTL_MY               0x80    // Mirror Y (lignes)
#define ILI9488_MADCTL_MX               0x40    // Mirror X (colonnes)
#define ILI9488_MADCTL_MV               0x20    // Échange X/Y
#define ILI9488_MADCTL_ML               0x10    // Mirror L
#define ILI9488_MADCTL_BGR              0x08    // Ordre BGR (sinon RGB)
#define ILI9488_MADCTL_MH               0x04    // Mirror H

/**
 * @brief Rotations prédéfinies (MADCTL)
 */
typedef enum {
    ILI9488_ROTATION_0      = (ILI9488_MADCTL_MX | ILI9488_MADCTL_BGR),            // Portrait
    ILI9488_ROTATION_90     = (ILI9488_MADCTL_MV | ILI9488_MADCTL_BGR),            // Paysage
    ILI9488_ROTATION_180    = (ILI9488_MADCTL_MY | ILI9488_MADCTL_BGR),            // Portrait inversé
    ILI9488_ROTATION_270    = (ILI9488_MADCTL_MX | ILI9488_MADCTL_MY | 
                               ILI9488_MADCTL_MV | ILI9488_MADCTL_BGR)             // Paysage inversé
} ILI9488_Rotation;
/** @} */

/**
 * @name Formats de pixel
 * @{
 */
#define ILI9488_PIXFMT_16BPP            0x55    // 16 bits par pixel (RGB565)
#define ILI9488_PIXFMT_18BPP            0x66    // 18 bits par pixel (RGB666)
#define ILI9488_PIXFMT_24BPP            0x77    // 24 bits par pixel (RGB888)
/** @} */

// ============================================================
// SECTION 4 : IDENTIFICATION DU DRIVER
// ============================================================

/**
 * @brief Identifiants possibles du contrôleur
 */
#define ILI9488_EXPECTED_ID             0x9488
#define ILI9486_EXPECTED_ID             0x9486
#define ILI9481_EXPECTED_ID             0x9481

/**
 * @brief Types de contrôleurs supportés
 */
typedef enum {
    ILI9488_CONTROLLER_9488 = 0,
    ILI9488_CONTROLLER_9486 = 1,
    ILI9488_CONTROLLER_9481 = 2,
    ILI9488_CONTROLLER_UNKNOWN = 0xFF
} ILI9488_ControllerType;

/**
 * @brief Structure d'informations du contrôleur
 */
typedef struct {
    ILI9488_ControllerType type;    // Type de contrôleur
    uint16_t id;                    // ID lu
    uint8_t madctl;                 // Configuration MADCTL
    uint8_t pixelFormat;            // Format de pixel
    uint16_t width;                 // Largeur active
    uint16_t height;                // Hauteur active
    bool initialized;               // Initialisé
} ILI9488_Info;

// ============================================================
// SECTION 5 : FONCTIONS DE CONTRÔLE BAS NIVEAU
// ============================================================

/**
 * @brief Émet une commande vers l'écran
 * @param cmd Code de la commande
 */
void ili9488_write_command(uint8_t cmd);

/**
 * @brief Émet une donnée vers l'écran
 * @param data Donnée 8 bits
 */
void ili9488_write_data(uint8_t data);

/**
 * @brief Émet une donnée 16 bits vers l'écran
 * @param data Donnée 16 bits
 */
void ili9488_write_data16(uint16_t data);

/**
 * @brief Émet un pixel (couleur 16 bits)
 * @param color Couleur RGB565
 */
void ili9488_write_pixel(uint16_t color);

/**
 * @brief Émet plusieurs pixels (burst)
 * @param colors Tableau de couleurs
 * @param count Nombre de pixels
 */
void ili9488_write_pixels(uint16_t* colors, uint32_t count);

/**
 * @brief Lit l'identifiant du contrôleur
 * @return uint32_t Identifiant (0x9488, 0x9486, etc.)
 */
uint32_t ili9488_read_id(void);

/**
 * @brief Lit un registre de paramètre
 * @param cmd Commande à lire
 * @return uint8_t Valeur lue
 */
uint8_t ili9488_read_register(uint8_t cmd);

// ============================================================
// SECTION 6 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise l'écran ILI9488
 * 
 * Séquence complète d'initialisation :
 * 1. Reset matériel
 * 2. Sortie du mode veille
 * 3. Configuration du format pixel (16 bpp)
 * 4. Configuration de la mémoire
 * 5. Configuration des tensions
 * 6. Configuration du gamma
 * 7. Allumage de l'écran
 * 
 * @return true si l'initialisation réussit
 */
bool ili9488_init(void);

/**
 * @brief Réinitialise l'écran
 */
void ili9488_reset(void);

/**
 * @brief Vérifie si l'écran est correctement initialisé
 * @return true si prêt
 */
bool ili9488_is_ready(void);

/**
 * @brief Récupère les informations du contrôleur
 * @return Pointeur vers la structure d'infos
 */
ILI9488_Info* ili9488_get_info(void);

// ============================================================
// SECTION 7 : FONCTIONS DE CONTRÔLE D'AFFICHAGE
// ============================================================

/**
 * @brief Allume l'écran
 */
void ili9488_display_on(void);

/**
 * @brief Éteint l'écran
 */
void ili9488_display_off(void);

/**
 * @brief Met l'écran en veille
 */
void ili9488_sleep_in(void);

/**
 * @brief Sort l'écran de veille
 */
void ili9488_sleep_out(void);

/**
 * @brief Définit la rotation de l'écran
 * @param rotation Rotation souhaitée
 */
void ili9488_set_rotation(ILI9488_Rotation rotation);

/**
 * @brief Récupère la rotation actuelle
 * @return ILI9488_Rotation
 */
ILI9488_Rotation ili9488_get_rotation(void);

/**
 * @brief Définit la luminosité (via PWM backlight)
 * @param brightness Luminosité (0-255)
 */
void ili9488_set_brightness(uint8_t brightness);

/**
 * @brief Récupère la luminosité actuelle
 * @return uint8_t Luminosité (0-255)
 */
uint8_t ili9488_get_brightness(void);

/**
 * @brief Inverse les couleurs
 * @param invert true pour inverser
 */
void ili9488_set_invert(bool invert);

// ============================================================
// SECTION 8 : FONCTIONS DE DESSIN BAS NIVEAU
// ============================================================

/**
 * @brief Définit la zone de dessin (fenêtre)
 * @param x1 Colonne de départ
 * @param y1 Ligne de départ
 * @param x2 Colonne de fin
 * @param y2 Ligne de fin
 */
void ili9488_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/**
 * @brief Remplit une zone rectangulaire avec une couleur
 * @param x1 Colonne de départ
 * @param y1 Ligne de départ
 * @param x2 Colonne de fin
 * @param y2 Ligne de fin
 * @param color Couleur RGB565
 */
void ili9488_fill_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/**
 * @brief Remplit tout l'écran avec une couleur
 * @param color Couleur RGB565
 */
void ili9488_fill_screen(uint16_t color);

/**
 * @brief Écrit un pixel à une position donnée
 * @param x Colonne
 * @param y Ligne
 * @param color Couleur RGB565
 */
void ili9488_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Lit la couleur d'un pixel
 * @param x Colonne
 * @param y Ligne
 * @return uint16_t Couleur RGB565
 */
uint16_t ili9488_read_pixel(uint16_t x, uint16_t y);

/**
 * @brief Écrit directement dans la mémoire vidéo (mode burst)
 * @param data Données pixels
 * @param length Nombre de pixels
 */
void ili9488_write_memory(uint16_t* data, uint32_t length);

/**
 * @brief Lit directement la mémoire vidéo (mode burst)
 * @param data Buffer de réception
 * @param length Nombre de pixels
 */
void ili9488_read_memory(uint16_t* data, uint32_t length);

// ============================================================
// SECTION 9 : FONCTIONS DE SCROLLING
// ============================================================

/**
 * @brief Définit la zone de scrolling vertical
 * @param top Ligne de début de la zone fixe supérieure
 * @param scroll Ligne de début de la zone de scrolling
 * @param bottom Ligne de fin de la zone fixe inférieure
 */
void ili9488_set_scroll_area(uint16_t top, uint16_t scroll, uint16_t bottom);

/**
 * @brief Définit le décalage de scrolling
 * @param offset Nombre de lignes de décalage
 */
void ili9488_scroll(uint16_t offset);

// ============================================================
// SECTION 10 : FONCTIONS DE DIAGNOSTIC
// ============================================================

/**
 * @brief Vérifie l'état de l'écran
 * @return uint32_t Registre de statut
 */
uint32_t ili9488_get_status(void);

/**
 * @brief Test d'auto-diagnostic
 * @return true si le test réussit
 */
bool ili9488_self_test(void);

/**
 * @brief Affiche les informations du contrôleur (debug)
 */
void ili9488_print_info(void);

/**
 * @brief Affiche un motif de test (couleurs)
 */
void ili9488_test_pattern(void);

// ============================================================
// SECTION 11 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // ILI9488_DRIVER_H