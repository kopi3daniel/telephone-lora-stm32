/**
 * @file ltdc_layer.h
 * @brief Gestion des couches (layers) du contrôleur LTDC
 * 
 * Le LTDC du STM32F429 supporte 2 couches matérielles :
 * - Couche 1 (Layer 0) : Couche de fond (background)
 * - Couche 2 (Layer 1) : Couche de superposition (foreground)
 * 
 * Chaque couche a son propre framebuffer, sa position,
 * son format de pixel et sa transparence.
 * 
 * Fonctionnalités :
 * - Double buffering par couche
 * - Alpha blending (transparence)
 * - Fenêtrage (portion d'écran)
 * - Changement de framebuffer dynamique
 * - Synchronisation verticale (pas de tearing)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef LTDC_LAYER_H
#define LTDC_LAYER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "ltdc_config.h"
#include "../../config.h"

// ============================================================
// SECTION 1 : CONSTANTES DES COUCHES
// ============================================================

/** @brief Nombre de couches matérielles disponibles */
#define LTDC_MAX_LAYERS                 2

/** @brief Index de la couche de fond (principale) */
#define LTDC_LAYER_BACKGROUND           0

/** @brief Index de la couche de superposition */
#define LTDC_LAYER_FOREGROUND           1

/** @brief Taille maximale d'une couche en pixels */
#define LTDC_LAYER_MAX_WIDTH            LTDC_WIDTH
#define LTDC_LAYER_MAX_HEIGHT           LTDC_HEIGHT

/** @brief Taille d'une couche plein écran en octets */
#define LTDC_LAYER_FULLSCREEN_SIZE      (LTDC_WIDTH * LTDC_HEIGHT * LTDC_BYTES_PER_PIXEL)

/** @brief Nombre de buffers par couche (double buffering) */
#define LTDC_LAYER_BUFFER_COUNT         2

// ============================================================
// SECTION 2 : FORMATS DE PIXEL SUPPORTÉS
// ============================================================

/**
 * @brief Formats de pixel supportés par les couches LTDC
 */
typedef enum {
    LTDC_LAYER_FORMAT_ARGB8888 = 0,     // 32 bits : Alpha + RGB
    LTDC_LAYER_FORMAT_RGB888   = 1,     // 24 bits : RGB
    LTDC_LAYER_FORMAT_RGB565   = 2,     // 16 bits : RGB565 (utilisé)
    LTDC_LAYER_FORMAT_ARGB1555 = 3,     // 16 bits : Alpha + RGB555
    LTDC_LAYER_FORMAT_ARGB4444 = 4,     // 16 bits : Alpha + RGB444
    LTDC_LAYER_FORMAT_L8       = 5,     // 8 bits : Luminance (CLUT)
    LTDC_LAYER_FORMAT_AL44     = 6,     // 8 bits : Alpha + Luminance
    LTDC_LAYER_FORMAT_AL88     = 7      // 16 bits : Alpha + Luminance
} LTDC_LayerPixelFormat;

// ============================================================
// SECTION 3 : MODES DE BLENDING
// ============================================================

/**
 * @brief Modes de blending entre les couches
 */
typedef enum {
    LTDC_BLEND_NONE = 0,            // Pas de blending (la couche 1 remplace)
    LTDC_BLEND_ALPHA_CONSTANT,      // Alpha constant (valeur fixe)
    LTDC_BLEND_ALPHA_PIXEL,         // Alpha par pixel (du framebuffer)
    LTDC_BLEND_ALPHA_COMBINED       // Alpha constant × Alpha pixel
} LTDC_BlendMode;

/**
 * @brief Facteurs de blending pour le LTDC
 */
#define LTDC_BLEND_FACTOR_CONSTANT      LTDC_BLENDING_FACTOR1_CA
#define LTDC_BLEND_FACTOR_PIXEL         LTDC_BLENDING_FACTOR1_PAxCA

// ============================================================
// SECTION 4 : STRUCTURE DE CONFIGURATION D'UNE COUCHE
// ============================================================

/**
 * @brief Configuration complète d'une couche LTDC
 */
typedef struct {
    // --- Identité ---
    uint8_t layerIndex;                     // 0 = fond, 1 = superposition
    bool enabled;                           // Couche active
    
    // --- Fenêtre ---
    uint16_t windowX1;                      // Colonne de début
    uint16_t windowY1;                      // Ligne de début
    uint16_t windowX2;                      // Colonne de fin
    uint16_t windowY2;                      // Ligne de fin
    
    // --- Framebuffer ---
    uint32_t framebufferAddr;               // Adresse dans la SDRAM
    uint32_t framebufferSize;               // Taille en octets
    uint16_t imageWidth;                    // Largeur de l'image source
    uint16_t imageHeight;                   // Hauteur de l'image source
    
    // --- Format ---
    LTDC_LayerPixelFormat pixelFormat;      // Format des pixels
    uint16_t linePitch;                     // Pas de ligne (octets)
    uint8_t bytesPerPixel;                  // Octets par pixel
    
    // --- Apparence ---
    uint8_t alpha;                          // Transparence (0-255)
    uint32_t defaultColor;                  // Couleur par défaut (si alpha < 255)
    LTDC_BlendMode blendMode;               // Mode de blending
    
    // --- Double buffering ---
    bool doubleBuffer;                      // Double buffering activé
    uint32_t backBufferAddr;                // Adresse du back buffer
    uint32_t frontBufferAddr;               // Adresse du front buffer
    
} LTDC_LayerConfig;

// ============================================================
// SECTION 5 : STRUCTURE D'ÉTAT D'UNE COUCHE
// ============================================================

/**
 * @brief État d'une couche LTDC
 */
typedef struct {
    LTDC_LayerConfig config;                // Configuration actuelle
    bool initialized;                       // Couche initialisée
    bool needsUpdate;                       // Mise à jour nécessaire
    uint32_t frameCount;                    // Compteur de trames
    uint32_t lastSwapTime;                  // Timestamp dernier swap
    uint32_t updateCount;                   // Nombre de mises à jour
} LTDC_LayerState;

// ============================================================
// SECTION 6 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise une couche LTDC
 * @param layerIndex Index de la couche (0 ou 1)
 * @param config Configuration de la couche
 * @return true si succès
 */
bool ltdc_layer_init(uint8_t layerIndex, const LTDC_LayerConfig* config);

/**
 * @brief Désinitialise une couche LTDC
 * @param layerIndex Index de la couche
 */
void ltdc_layer_deinit(uint8_t layerIndex);

/**
 * @brief Vérifie si une couche est initialisée
 * @param layerIndex Index de la couche
 * @return true si prête
 */
bool ltdc_layer_is_ready(uint8_t layerIndex);

/**
 * @brief Récupère l'état d'une couche
 * @param layerIndex Index de la couche
 * @return Pointeur vers l'état
 */
LTDC_LayerState* ltdc_layer_get_state(uint8_t layerIndex);

// ============================================================
// SECTION 7 : FONCTIONS DE CONTRÔLE
// ============================================================

/**
 * @brief Active une couche
 * @param layerIndex Index de la couche
 */
void ltdc_layer_enable(uint8_t layerIndex);

/**
 * @brief Désactive une couche
 * @param layerIndex Index de la couche
 */
void ltdc_layer_disable(uint8_t layerIndex);

/**
 * @brief Vérifie si une couche est active
 * @param layerIndex Index de la couche
 * @return true si active
 */
bool ltdc_layer_is_enabled(uint8_t layerIndex);

/**
 * @brief Définit la visibilité d'une couche
 * @param layerIndex Index de la couche
 * @param visible true pour visible
 */
void ltdc_layer_set_visible(uint8_t layerIndex, bool visible);

/**
 * @brief Bascule la visibilité d'une couche
 * @param layerIndex Index de la couche
 */
void ltdc_layer_toggle_visible(uint8_t layerIndex);

// ============================================================
// SECTION 8 : FONCTIONS DE POSITIONNEMENT
// ============================================================

/**
 * @brief Définit la position d'une couche
 * @param layerIndex Index de la couche
 * @param x1 Colonne de début
 * @param y1 Ligne de début
 * @param x2 Colonne de fin
 * @param y2 Ligne de fin
 */
void ltdc_layer_set_position(uint8_t layerIndex, 
                              uint16_t x1, uint16_t y1,
                              uint16_t x2, uint16_t y2);

/**
 * @brief Centre une couche sur l'écran
 * @param layerIndex Index de la couche
 * @param width Largeur de la couche
 * @param height Hauteur de la couche
 */
void ltdc_layer_center(uint8_t layerIndex, uint16_t width, uint16_t height);

/**
 * @brief Met une couche en plein écran
 * @param layerIndex Index de la couche
 */
void ltdc_layer_fullscreen(uint8_t layerIndex);

/**
 * @brief Déplace une couche
 * @param layerIndex Index de la couche
 * @param dx Décalage horizontal
 * @param dy Décalage vertical
 */
void ltdc_layer_move(uint8_t layerIndex, int16_t dx, int16_t dy);

/**
 * @brief Récupère la position d'une couche
 * @param layerIndex Index de la couche
 * @param x1 Colonne début (sortie)
 * @param y1 Ligne début (sortie)
 * @param x2 Colonne fin (sortie)
 * @param y2 Ligne fin (sortie)
 */
void ltdc_layer_get_position(uint8_t layerIndex,
                              uint16_t* x1, uint16_t* y1,
                              uint16_t* x2, uint16_t* y2);

// ============================================================
// SECTION 9 : FONCTIONS DE FRAMEBUFFER
// ============================================================

/**
 * @brief Définit l'adresse du framebuffer d'une couche
 * @param layerIndex Index de la couche
 * @param addr Adresse du framebuffer (dans la SDRAM)
 */
void ltdc_layer_set_framebuffer(uint8_t layerIndex, uint32_t addr);

/**
 * @brief Récupère l'adresse du framebuffer d'une couche
 * @param layerIndex Index de la couche
 * @return Adresse du framebuffer
 */
uint32_t ltdc_layer_get_framebuffer(uint8_t layerIndex);

/**
 * @brief Efface le framebuffer d'une couche (remplit avec une couleur)
 * @param layerIndex Index de la couche
 * @param color Couleur RGB565
 */
void ltdc_layer_clear(uint8_t layerIndex, uint16_t color);

/**
 * @brief Remplit une zone du framebuffer d'une couche
 * @param layerIndex Index de la couche
 * @param x1 Colonne début
 * @param y1 Ligne début
 * @param x2 Colonne fin
 * @param y2 Ligne fin
 * @param color Couleur RGB565
 */
void ltdc_layer_fill_rect(uint8_t layerIndex,
                           uint16_t x1, uint16_t y1,
                           uint16_t x2, uint16_t y2,
                           uint16_t color);

// ============================================================
// SECTION 10 : FONCTIONS DE DOUBLE BUFFERING
// ============================================================

/**
 * @brief Active le double buffering pour une couche
 * @param layerIndex Index de la couche
 */
void ltdc_layer_double_buffer_enable(uint8_t layerIndex);

/**
 * @brief Désactive le double buffering
 * @param layerIndex Index de la couche
 */
void ltdc_layer_double_buffer_disable(uint8_t layerIndex);

/**
 * @brief Échange les buffers (swap) d'une couche
 * @param layerIndex Index de la couche
 * 
 * Échange le front buffer et le back buffer.
 * Doit être appelé pendant le blanking vertical.
 */
void ltdc_layer_swap_buffers(uint8_t layerIndex);

/**
 * @brief Récupère l'adresse du back buffer
 * @param layerIndex Index de la couche
 * @return Adresse du back buffer (pour dessiner)
 */
uint32_t ltdc_layer_get_back_buffer(uint8_t layerIndex);

/**
 * @brief Récupère l'adresse du front buffer
 * @param layerIndex Index de la couche
 * @return Adresse du front buffer (affiché)
 */
uint32_t ltdc_layer_get_front_buffer(uint8_t layerIndex);

// ============================================================
// SECTION 11 : FONCTIONS DE TRANSPARENCE
// ============================================================

/**
 * @brief Définit la transparence d'une couche
 * @param layerIndex Index de la couche
 * @param alpha Valeur alpha (0=transparent, 255=opaque)
 */
void ltdc_layer_set_alpha(uint8_t layerIndex, uint8_t alpha);

/**
 * @brief Récupère la transparence d'une couche
 * @param layerIndex Index de la couche
 * @return Valeur alpha
 */
uint8_t ltdc_layer_get_alpha(uint8_t layerIndex);

/**
 * @brief Effet de fondu entrant (fade in)
 * @param layerIndex Index de la couche
 * @param durationMs Durée en ms
 */
void ltdc_layer_fade_in(uint8_t layerIndex, uint32_t durationMs);

/**
 * @brief Effet de fondu sortant (fade out)
 * @param layerIndex Index de la couche
 * @param durationMs Durée en ms
 */
void ltdc_layer_fade_out(uint8_t layerIndex, uint32_t durationMs);

/**
 * @brief Définit le mode de blending
 * @param layerIndex Index de la couche
 * @param mode Mode de blending
 */
void ltdc_layer_set_blend_mode(uint8_t layerIndex, LTDC_BlendMode mode);

// ============================================================
// SECTION 12 : FONCTIONS D'ANIMATION
// ============================================================

/**
 * @brief Animation de slide horizontal
 * @param layerIndex Index de la couche
 * @param fromX Position X de départ
 * @param toX Position X d'arrivée
 * @param durationMs Durée en ms
 */
void ltdc_layer_slide_x(uint8_t layerIndex, int16_t fromX, int16_t toX, uint32_t durationMs);

/**
 * @brief Animation de slide vertical
 * @param layerIndex Index de la couche
 * @param fromY Position Y de départ
 * @param toY Position Y d'arrivée
 * @param durationMs Durée en ms
 */
void ltdc_layer_slide_y(uint8_t layerIndex, int16_t fromY, int16_t toY, uint32_t durationMs);

/**
 * @brief Animation de zoom
 * @param layerIndex Index de la couche
 * @param fromScale Échelle de départ (100 = normal)
 * @param toScale Échelle d'arrivée
 * @param durationMs Durée en ms
 */
void ltdc_layer_zoom(uint8_t layerIndex, uint8_t fromScale, uint8_t toScale, uint32_t durationMs);

// ============================================================
// SECTION 13 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche les informations d'une couche
 * @param layerIndex Index de la couche
 */
void ltdc_layer_print_info(uint8_t layerIndex);

/**
 * @brief Affiche toutes les couches
 */
void ltdc_layer_print_all(void);

/**
 * @brief Vérifie la validité d'une configuration
 * @param config Configuration à vérifier
 * @return true si valide
 */
bool ltdc_layer_validate_config(const LTDC_LayerConfig* config);

/**
 * @brief Crée une configuration par défaut pour une couche
 * @param layerIndex Index de la couche
 * @param config Configuration à remplir (sortie)
 */
void ltdc_layer_get_default_config(uint8_t layerIndex, LTDC_LayerConfig* config);

// ============================================================
// SECTION 14 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Calcule l'adresse d'un pixel dans le framebuffer
 * @param x Colonne
 * @param y Ligne
 * @return Offset en pixels
 */
#define LTDC_PIXEL_OFFSET(x, y)         ((y) * LTDC_WIDTH + (x))

/**
 * @brief Calcule l'adresse d'un pixel en octets
 * @param base Base du framebuffer
 * @param x Colonne
 * @param y Ligne
 * @return Adresse absolue
 */
#define LTDC_PIXEL_ADDR(base, x, y)     ((base) + LTDC_PIXEL_OFFSET(x, y) * LTDC_BYTES_PER_PIXEL)

/**
 * @brief Vérifie si des coordonnées sont dans la couche
 * @param layerIndex Index de la couche
 * @param x Colonne
 * @param y Ligne
 * @return true si dans la couche
 */
#define LTDC_LAYER_CONTAINS(layer, x, y) \
    ((x) >= (layer)->config.windowX1 && (x) <= (layer)->config.windowX2 && \
     (y) >= (layer)->config.windowY1 && (y) <= (layer)->config.windowY2)

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // LTDC_LAYER_H