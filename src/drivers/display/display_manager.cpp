/**
 * @file display_manager.h
 * @brief Gestionnaire d'affichage haut niveau
 * 
 * Ce fichier unifie tous les drivers d'affichage (ILI9488, LTDC, DMA2D, Buffers)
 * en une API simple et cohérente pour l'interface utilisateur.
 * 
 * Il fournit :
 * - L'initialisation complète du système d'affichage
 * - Des fonctions de dessin de primitives graphiques
 * - La gestion des couleurs et des polices
 * - Des widgets simples (boutons, cadres, barres)
 * - La gestion du double buffering et du VSYNC
 * 
 * Cette couche isole le reste de l'application des détails
 * matériels (LTDC, DMA2D, ILI9488).
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "ili9488_defs.h"
#include "ltdc_config.h"
#include "display_buffer.h"
#include "../../config.h"
#include "../../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du gestionnaire d'affichage */
#define DISPLAY_MANAGER_VERSION         "1.0.0"

/** @brief Largeur de l'écran */
#define DISPLAY_WIDTH                   LTDC_WIDTH

/** @brief Hauteur de l'écran */
#define DISPLAY_HEIGHT                  LTDC_HEIGHT

/** @brief Nombre total de pixels */
#define DISPLAY_TOTAL_PIXELS            (DISPLAY_WIDTH * DISPLAY_HEIGHT)

/** @brief Taille d'un pixel en octets */
#define DISPLAY_BYTES_PER_PIXEL         2

/** @brief Nombre maximal de polices chargées */
#define DISPLAY_MAX_FONTS               5

/** @brief Taille maximale du buffer de texte */
#define DISPLAY_TEXT_BUFFER_SIZE        256

// ============================================================
// SECTION 2 : ALIGNEMENT ET POSITIONNEMENT
// ============================================================

/**
 * @brief Alignement horizontal pour le texte
 */
typedef enum {
    DISPLAY_ALIGN_LEFT      = 0,    // Aligné à gauche
    DISPLAY_ALIGN_CENTER    = 1,    // Centré horizontalement
    DISPLAY_ALIGN_RIGHT     = 2     // Aligné à droite
} DisplayAlign;

/**
 * @brief Alignement vertical pour le texte
 */
typedef enum {
    DISPLAY_VALIGN_TOP      = 0,    // Aligné en haut
    DISPLAY_VALIGN_MIDDLE   = 1,    // Centré verticalement
    DISPLAY_VALIGN_BOTTOM   = 2     // Aligné en bas
} DisplayVAlign;

// ============================================================
// SECTION 3 : STRUCTURES DE POLICES
// ============================================================

/**
 * @brief Structure décrivant une police de caractères
 * 
 * Supporte les polices bitmap de taille fixe.
 */
typedef struct {
    const uint8_t* data;            // Données de la police
    uint8_t charWidth;              // Largeur d'un caractère en pixels
    uint8_t charHeight;             // Hauteur d'un caractère en pixels
    uint8_t bytesPerChar;           // Octets par caractère
    char firstChar;                 // Premier caractère de la table (généralement ' ')
    char lastChar;                  // Dernier caractère de la table (généralement '~')
    const char* name;               // Nom de la police
} DisplayFont;

/**
 * @brief Polices intégrées disponibles
 */
extern const DisplayFont font_5x7;      // Police 5×7 (compacte)
extern const DisplayFont font_8x16;     // Police 8×16 (standard)
extern const DisplayFont font_12x24;    // Police 12×24 (grande)
extern const DisplayFont font_16x32;    // Police 16×32 (très grande)

// ============================================================
// SECTION 4 : STRUCTURES DE WIDGETS SIMPLES
// ============================================================

/**
 * @brief Structure d'un bouton
 */
typedef struct {
    uint16_t x, y;                  // Position
    uint16_t width, height;         // Dimensions
    const char* text;               // Texte du bouton
    uint16_t bgColor;               // Couleur de fond
    uint16_t textColor;             // Couleur du texte
    uint16_t borderColor;           // Couleur de la bordure
    uint8_t cornerRadius;           // Rayon des coins (0 = carré)
    bool pressed;                   // État pressé
    bool enabled;                   // Activé
} DisplayButton;

/**
 * @brief Structure d'une barre de progression
 */
typedef struct {
    uint16_t x, y;                  // Position
    uint16_t width, height;         // Dimensions
    uint16_t bgColor;               // Couleur de fond
    uint16_t fillColor;             // Couleur de remplissage
    uint16_t borderColor;           // Couleur de la bordure
    uint8_t progress;               // Progression (0-100)
} DisplayProgressBar;

/**
 * @brief Structure d'un panneau/cadre
 */
typedef struct {
    uint16_t x, y;                  // Position
    uint16_t width, height;         // Dimensions
    uint16_t bgColor;               // Couleur de fond
    uint16_t borderColor;           // Couleur de la bordure
    uint8_t cornerRadius;           // Rayon des coins
    const char* title;              // Titre (optionnel)
} DisplayPanel;

// ============================================================
// SECTION 5 : ÉTAT DU GESTIONNAIRE
// ============================================================

/**
 * @brief État global du gestionnaire d'affichage
 */
typedef struct {
    bool initialized;               // Système initialisé
    bool displayOn;                 // Écran allumé
    uint16_t cursorX;               // Curseur X pour le texte
    uint16_t cursorY;               // Curseur Y pour le texte
    uint16_t textColor;             // Couleur du texte courant
    uint16_t bgColor;               // Couleur de fond courante
    const DisplayFont* currentFont; // Police courante
    uint8_t textSize;               // Taille du texte (multiplicateur)
    bool textWrap;                  // Retour à la ligne automatique
    ILI9488_Rotation rotation;      // Rotation actuelle
    uint32_t frameCount;            // Compteur de trames
} DisplayState;

// ============================================================
// SECTION 6 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise le système d'affichage complet
 * 
 * Ordre d'initialisation :
 * 1. GPIO (broches de contrôle)
 * 2. ILI9488 (écran)
 * 3. LTDC (contrôleur LCD)
 * 4. DMA2D (accélérateur graphique)
 * 5. Buffers (double buffering)
 * 
 * @return true si tout est initialisé correctement
 */
bool display_init(void);

/**
 * @brief Désinitialise le système d'affichage
 */
void display_deinit(void);

/**
 * @brief Vérifie si l'affichage est prêt
 * @return true si prêt
 */
bool display_is_ready(void);

/**
 * @brief Récupère l'état du gestionnaire
 * @return Pointeur vers l'état
 */
DisplayState* display_get_state(void);

// ============================================================
// SECTION 7 : FONCTIONS DE CONTRÔLE GLOBAL
// ============================================================

/**
 * @brief Allume l'écran
 */
void display_on(void);

/**
 * @brief Éteint l'écran
 */
void display_off(void);

/**
 * @brief Met l'écran en veille
 */
void display_sleep(void);

/**
 * @brief Réveille l'écran
 */
void display_wakeup(void);

/**
 * @brief Définit la luminosité
 * @param brightness Luminosité (0-255)
 */
void display_set_brightness(uint8_t brightness);

/**
 * @brief Récupère la luminosité
 * @return Luminosité (0-255)
 */
uint8_t display_get_brightness(void);

/**
 * @brief Définit la rotation de l'écran
 * @param rotation Rotation souhaitée
 */
void display_set_rotation(ILI9488_Rotation rotation);

/**
 * @brief Récupère la rotation actuelle
 * @return Rotation
 */
ILI9488_Rotation display_get_rotation(void);

// ============================================================
// SECTION 8 : FONCTIONS DE DESSIN DE BASE
// ============================================================

/**
 * @brief Efface tout l'écran avec une couleur
 * @param color Couleur RGB565
 */
void display_clear(uint16_t color);

/**
 * @brief Dessine un pixel
 * @param x Colonne
 * @param y Ligne
 * @param color Couleur RGB565
 */
void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Dessine une ligne
 * @param x1 Début X
 * @param y1 Début Y
 * @param x2 Fin X
 * @param y2 Fin Y
 * @param color Couleur RGB565
 */
void display_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/**
 * @brief Dessine une ligne horizontale (optimisé DMA2D)
 * @param x1 Début X
 * @param y Ligne Y
 * @param x2 Fin X
 * @param color Couleur RGB565
 */
void display_draw_hline(uint16_t x1, uint16_t y, uint16_t x2, uint16_t color);

/**
 * @brief Dessine une ligne verticale (optimisé DMA2D)
 * @param x Colonne X
 * @param y1 Début Y
 * @param y2 Fin Y
 * @param color Couleur RGB565
 */
void display_draw_vline(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color);

/**
 * @brief Dessine un rectangle (contour)
 * @param x1 Début X
 * @param y1 Début Y
 * @param x2 Fin X
 * @param y2 Fin Y
 * @param color Couleur RGB565
 */
void display_draw_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/**
 * @brief Remplit un rectangle (plein)
 * @param x1 Début X
 * @param y1 Début Y
 * @param x2 Fin X
 * @param y2 Fin Y
 * @param color Couleur RGB565
 */
void display_fill_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/**
 * @brief Dessine un rectangle arrondi (contour)
 * @param x1 Début X
 * @param y1 Début Y
 * @param x2 Fin X
 * @param y2 Fin Y
 * @param radius Rayon des coins
 * @param color Couleur RGB565
 */
void display_draw_round_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                              uint16_t radius, uint16_t color);

/**
 * @brief Remplit un rectangle arrondi (plein)
 * @param x1 Début X
 * @param y1 Début Y
 * @param x2 Fin X
 * @param y2 Fin Y
 * @param radius Rayon des coins
 * @param color Couleur RGB565
 */
void display_fill_round_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                              uint16_t radius, uint16_t color);

/**
 * @brief Dessine un cercle (contour)
 * @param x0 Centre X
 * @param y0 Centre Y
 * @param radius Rayon
 * @param color Couleur RGB565
 */
void display_draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);

/**
 * @brief Remplit un cercle (plein)
 * @param x0 Centre X
 * @param y0 Centre Y
 * @param radius Rayon
 * @param color Couleur RGB565
 */
void display_fill_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);

// ============================================================
// SECTION 9 : FONCTIONS DE TEXTE
// ============================================================

/**
 * @brief Définit la police courante
 * @param font Pointeur vers la police
 */
void display_set_font(const DisplayFont* font);

/**
 * @brief Récupère la police courante
 * @return Pointeur vers la police
 */
const DisplayFont* display_get_font(void);

/**
 * @brief Définit la taille du texte
 * @param size Multiplicateur (1, 2, 3...)
 */
void display_set_text_size(uint8_t size);

/**
 * @brief Définit la couleur du texte
 * @param color Couleur RGB565
 */
void display_set_text_color(uint16_t color);

/**
 * @brief Définit la couleur de fond du texte
 * @param color Couleur RGB565
 */
void display_set_text_bg_color(uint16_t color);

/**
 * @brief Positionne le curseur texte
 * @param x Colonne
 * @param y Ligne
 */
void display_set_cursor(uint16_t x, uint16_t y);

/**
 * @brief Active/désactive le retour à la ligne
 * @param wrap true pour activer
 */
void display_set_text_wrap(bool wrap);

/**
 * @brief Dessine un caractère
 * @param x Colonne
 * @param y Ligne
 * @param c Caractère
 * @param color Couleur RGB565
 * @param bgColor Couleur de fond (si identique à color, transparent)
 * @param size Multiplicateur de taille
 */
void display_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, 
                        uint16_t bgColor, uint8_t size);

/**
 * @brief Écrit un caractère à la position du curseur
 * @param c Caractère
 */
void display_write_char(char c);

/**
 * @brief Écrit une chaîne de caractères
 * @param x Colonne
 * @param y Ligne
 * @param text Texte à afficher
 * @param color Couleur RGB565
 * @param size Multiplicateur de taille
 */
void display_draw_text(uint16_t x, uint16_t y, const char* text, 
                        uint16_t color, uint8_t size);

/**
 * @brief Écrit une chaîne à la position du curseur
 * @param text Texte à afficher
 */
void display_write_text(const char* text);

/**
 * @brief Écrit une chaîne centrée verticalement
 * @param y Ligne
 * @param text Texte à afficher
 * @param color Couleur RGB565
 * @param size Multiplicateur de taille
 */
void display_draw_text_center(uint16_t y, const char* text, uint16_t color, uint8_t size);

/**
 * @brief Écrit une chaîne alignée
 * @param x1 Début X
 * @param y1 Début Y
 * @param x2 Fin X
 * @param y2 Fin Y
 * @param text Texte
 * @param color Couleur RGB565
 * @param size Taille
 * @param align Alignement horizontal
 * @param valign Alignement vertical
 */
void display_draw_text_aligned(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                const char* text, uint16_t color, uint8_t size,
                                DisplayAlign align, DisplayVAlign valign);

/**
 * @brief Mesure la largeur d'un texte en pixels
 * @param text Texte à mesurer
 * @param size Taille du texte
 * @return Largeur en pixels
 */
uint16_t display_text_width(const char* text, uint8_t size);

/**
 * @brief Mesure la hauteur d'un texte en pixels
 * @param size Taille du texte
 * @return Hauteur en pixels
 */
uint16_t display_text_height(uint8_t size);

// ============================================================
// SECTION 10 : FONCTIONS DE WIDGETS
// ============================================================

/**
 * @brief Dessine un bouton
 * @param button Configuration du bouton
 */
void display_draw_button(const DisplayButton* button);

/**
 * @brief Vérifie si des coordonnées touchent un bouton
 * @param button Bouton
 * @param tx Coordonnée X tactile
 * @param ty Coordonnée Y tactile
 * @return true si le bouton est touché
 */
bool display_button_touched(const DisplayButton* button, uint16_t tx, uint16_t ty);

/**
 * @brief Dessine une barre de progression
 * @param bar Configuration de la barre
 */
void display_draw_progress_bar(const DisplayProgressBar* bar);

/**
 * @brief Met à jour une barre de progression
 * @param bar Barre à mettre à jour
 * @param progress Nouvelle progression (0-100)
 */
void display_update_progress_bar(DisplayProgressBar* bar, uint8_t progress);

/**
 * @brief Dessine un panneau/cadre
 * @param panel Configuration du panneau
 */
void display_draw_panel(const DisplayPanel* panel);

/**
 * @brief Dessine une icône (bitmap RGB565)
 * @param x Position X
 * @param y Position Y
 * @param iconData Données de l'icône
 * @param width Largeur
 * @param height Hauteur
 */
void display_draw_icon(uint16_t x, uint16_t y, const uint16_t* iconData,
                        uint16_t width, uint16_t height);

// ============================================================
// SECTION 11 : FONCTIONS DE DOUBLE BUFFERING
// ============================================================

/**
 * @brief Active le double buffering
 */
void display_double_buffer_enable(void);

/**
 * @brief Désactive le double buffering
 */
void display_double_buffer_disable(void);

/**
 * @brief Échange les buffers (swap) et met à jour l'écran
 */
void display_swap_buffers(void);

/**
 * @brief Récupère le back buffer pour dessin direct
 * @return Pointeur vers le back buffer (uint16_t*)
 */
uint16_t* display_get_back_buffer(void);

/**
 * @brief Récupère le front buffer (affiché)
 * @return Pointeur vers le front buffer (uint16_t*)
 */
uint16_t* display_get_front_buffer(void);

// ============================================================
// SECTION 12 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche les informations du système d'affichage
 */
void display_print_info(void);

/**
 * @brief Affiche un motif de test
 */
void display_test_pattern(void);

/**
 * @brief Affiche les statistiques de performance
 */
void display_print_statistics(void);

/**
 * @brief Vérifie l'intégrité du système d'affichage
 * @return true si tout est OK
 */
bool display_self_test(void);

// ============================================================
// SECTION 13 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define DISPLAY_DEBUG(fmt, ...)     printf("[DISPLAY] " fmt, ##__VA_ARGS__)
#else
    #define DISPLAY_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 14 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_MANAGER_H