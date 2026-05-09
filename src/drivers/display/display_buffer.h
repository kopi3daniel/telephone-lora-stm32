/**
 * @file display_buffer.h
 * @brief Gestion des buffers d'affichage (framebuffers)
 * 
 * Ce fichier gère l'allocation, la gestion et l'échange
 * des buffers d'affichage (framebuffers) pour un rendu fluide.
 * 
 * Fonctionnalités :
 * - Allocation de buffers en SDRAM
 * - Double buffering (front/back buffer)
 * - Triple buffering (optionnel)
 * - Swap synchronisé avec le VSYNC (pas de tearing)
 * - Gestion de plusieurs buffers pour différentes couches
 * - API de dessin directe dans les buffers
 * 
 * Architecture mémoire (SDRAM) :
 * ┌──────────────────────────────────────────────┐
 * │ 0xC0000000 : Framebuffer Principal (300 Ko)  │
 * │ 0xC004B000 : Framebuffer Secondaire (300 Ko) │
 * │ 0xC0096000 : Buffer Couche 2 (300 Ko)        │
 * │ 0xC00E1000 : Libre (~6.9 Mo)                 │
 * └──────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef DISPLAY_BUFFER_H
#define DISPLAY_BUFFER_H

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
#include "ltdc_config.h"
#include "../../config.h"
#include "../../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Nombre de buffers par couche (2 = double buffering) */
#define DISPLAY_BUFFER_COUNT            2

/** @brief Taille d'un buffer en octets */
#define DISPLAY_BUFFER_SIZE             LTDC_FRAMEBUFFER_SIZE

/** @brief Nombre total de pixels par buffer */
#define DISPLAY_BUFFER_PIXELS           (LTDC_WIDTH * LTDC_HEIGHT)

/** @brief Adresse de base des buffers dans la SDRAM */
#define DISPLAY_BUFFER_BASE_ADDR        SDRAM_BASE_ADDR

/** @brief Offset entre les buffers */
#define DISPLAY_BUFFER_OFFSET           DISPLAY_BUFFER_SIZE

/** @brief Nombre maximal de buffers gérables */
#define DISPLAY_MAX_BUFFERS             4

// ============================================================
// SECTION 2 : TYPES DE BUFFERS
// ============================================================

/**
 * @brief Types de buffers d'affichage
 */
typedef enum {
    DISPLAY_BUFFER_FRONT    = 0,    // Buffer affiché à l'écran (front buffer)
    DISPLAY_BUFFER_BACK     = 1,    // Buffer de dessin (back buffer)
    DISPLAY_BUFFER_EXTRA    = 2,    // Buffer supplémentaire (triple buffering)
    DISPLAY_BUFFER_OVERLAY  = 3     // Buffer pour la couche de superposition
} DisplayBufferType;

/**
 * @brief États d'un buffer
 */
typedef enum {
    BUFFER_STATE_FREE       = 0,    // Libre (prêt pour le dessin)
    BUFFER_STATE_DRAWING    = 1,    // En cours de dessin
    BUFFER_STATE_READY      = 2,    // Prêt à être affiché
    BUFFER_STATE_DISPLAYED  = 3     // Actuellement affiché
} BufferState;

// ============================================================
// SECTION 3 : STRUCTURES DE DONNÉES
// ============================================================

/**
 * @brief Structure décrivant un buffer d'affichage
 */
typedef struct {
    uint32_t address;               // Adresse physique dans la SDRAM
    uint32_t size;                  // Taille en octets
    uint16_t width;                 // Largeur en pixels
    uint16_t height;                // Hauteur en pixels
    uint8_t bytesPerPixel;          // Octets par pixel (2 pour RGB565)
    BufferState state;              // État actuel du buffer
    DisplayBufferType type;         // Type de buffer
    uint32_t frameCount;            // Compteur de trames
    uint32_t lastSwapTime;          // Timestamp du dernier swap
    bool dirty;                     // Buffer modifié depuis le dernier affichage
} DisplayBuffer;

/**
 * @brief Configuration globale des buffers d'affichage
 */
typedef struct {
    DisplayBuffer buffers[DISPLAY_MAX_BUFFERS];  // Tableau des buffers
    uint8_t bufferCount;                         // Nombre de buffers alloués
    uint8_t activeFrontBuffer;                   // Index du front buffer actif
    uint8_t activeBackBuffer;                    // Index du back buffer actif
    bool doubleBufferingEnabled;                 // Double buffering actif
    bool tripleBufferingEnabled;                 // Triple buffering actif
    bool vsyncEnabled;                           // Synchronisation VSYNC
    uint32_t totalSwaps;                         // Nombre total d'échanges
    uint32_t totalFrames;                        // Nombre total de trames
    uint32_t tearCount;                          // Nombre de déchirements détectés
} DisplayBufferConfig;

// ============================================================
// SECTION 4 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise le système de buffers d'affichage
 * @return true si succès
 */
bool display_buffer_init(void);

/**
 * @brief Alloue un nouveau buffer en SDRAM
 * @param type Type de buffer souhaité
 * @param width Largeur en pixels
 * @param height Hauteur en pixels
 * @return Index du buffer alloué, -1 si échec
 */
int8_t display_buffer_allocate(DisplayBufferType type, uint16_t width, uint16_t height);

/**
 * @brief Libère un buffer
 * @param bufferIndex Index du buffer à libérer
 */
void display_buffer_free(uint8_t bufferIndex);

/**
 * @brief Réinitialise tous les buffers
 */
void display_buffer_reset(void);

/**
 * @brief Vérifie si les buffers sont initialisés
 * @return true si prêt
 */
bool display_buffer_is_ready(void);

// ============================================================
// SECTION 5 : FONCTIONS D'ACCÈS AUX BUFFERS
// ============================================================

/**
 * @brief Récupère le front buffer actif (affiché)
 * @return Pointeur vers le buffer de premier plan (uint16_t*)
 */
uint16_t* display_buffer_get_front(void);

/**
 * @brief Récupère le back buffer (pour dessiner)
 * @return Pointeur vers le buffer d'arrière-plan (uint16_t*)
 */
uint16_t* display_buffer_get_back(void);

/**
 * @brief Récupère un buffer par son type
 * @param type Type de buffer
 * @return Pointeur vers le buffer
 */
uint16_t* display_buffer_get_by_type(DisplayBufferType type);

/**
 * @brief Récupère un buffer par son index
 * @param index Index du buffer
 * @return Pointeur vers le buffer
 */
uint16_t* display_buffer_get_by_index(uint8_t index);

/**
 * @brief Récupère l'adresse physique d'un buffer
 * @param index Index du buffer
 * @return Adresse dans la SDRAM
 */
uint32_t display_buffer_get_address(uint8_t index);

/**
 * @brief Récupère la taille d'un buffer
 * @param index Index du buffer
 * @return Taille en octets
 */
uint32_t display_buffer_get_size(uint8_t index);

// ============================================================
// SECTION 6 : FONCTIONS DE DOUBLE BUFFERING
// ============================================================

/**
 * @brief Active le double buffering
 * 
 * Alloue deux buffers et les initialise.
 * Le front buffer est affiché, le back buffer est prêt pour le dessin.
 */
void display_buffer_double_enable(void);

/**
 * @brief Désactive le double buffering
 * 
 * Revenir à un seul buffer (pas d'échange).
 */
void display_buffer_double_disable(void);

/**
 * @brief Active le triple buffering (plus fluide, mais plus de mémoire)
 */
void display_buffer_triple_enable(void);

/**
 * @brief Échange les buffers (swap)
 * 
 * Le back buffer devient le front buffer (affiché)
 * L'ancien front buffer devient le nouveau back buffer (pour dessiner)
 * 
 * @param waitVsync Attendre le VSYNC pour éviter le tearing
 */
void display_buffer_swap(bool waitVsync);

/**
 * @brief Force l'échange immédiat (sans VSYNC)
 * 
 * ⚠️ Peut causer du tearing (déchirement d'image)
 */
void display_buffer_swap_immediate(void);

/**
 * @brief Vérifie si un swap est en attente
 * @return true si un swap est programmé
 */
bool display_buffer_swap_pending(void);

// ============================================================
// SECTION 7 : FONCTIONS DE SYNCHRONISATION
// ============================================================

/**
 * @brief Active la synchronisation verticale (VSYNC)
 * 
 * Les swaps seront synchronisés avec le rafraîchissement de l'écran,
 * éliminant le tearing.
 */
void display_buffer_vsync_enable(void);

/**
 * @brief Désactive la synchronisation VSYNC
 */
void display_buffer_vsync_disable(void);

/**
 * @brief Vérifie si on est dans la période de blanking vertical
 * @return true si on peut échanger sans tearing
 */
bool display_buffer_is_vblank(void);

/**
 * @brief Attend la prochaine période de blanking vertical
 * @param timeoutMs Timeout en ms
 * @return true si le blanking est détecté avant le timeout
 */
bool display_buffer_wait_vblank(uint32_t timeoutMs);

/**
 * @brief Callback appelé à chaque VSYNC (fin de trame)
 * 
 * Cette fonction est appelée automatiquement par l'interruption LTDC.
 */
void display_buffer_vsync_callback(void);

// ============================================================
// SECTION 8 : FONCTIONS DE DESSIN DIRECT
// ============================================================

/**
 * @brief Efface le back buffer avec une couleur
 * @param color Couleur RGB565
 */
void display_buffer_clear_back(uint16_t color);

/**
 * @brief Efface le front buffer avec une couleur
 * @param color Couleur RGB565
 */
void display_buffer_clear_front(uint16_t color);

/**
 * @brief Efface un buffer spécifique
 * @param bufferIndex Index du buffer
 * @param color Couleur RGB565
 */
void display_buffer_clear(uint8_t bufferIndex, uint16_t color);

/**
 * @brief Remplit une zone rectangulaire dans le back buffer
 * @param x1 Colonne début
 * @param y1 Ligne début
 * @param x2 Colonne fin
 * @param y2 Ligne fin
 * @param color Couleur RGB565
 */
void display_buffer_fill_rect_back(uint16_t x1, uint16_t y1,
                                    uint16_t x2, uint16_t y2,
                                    uint16_t color);

/**
 * @brief Définit un pixel dans le back buffer
 * @param x Colonne
 * @param y Ligne
 * @param color Couleur RGB565
 */
void display_buffer_set_pixel_back(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Lit un pixel du front buffer
 * @param x Colonne
 * @param y Ligne
 * @return Couleur RGB565
 */
uint16_t display_buffer_get_pixel_front(uint16_t x, uint16_t y);

// ============================================================
// SECTION 9 : FONCTIONS DE COPIE ENTRE BUFFERS
// ============================================================

/**
 * @brief Copie le front buffer vers le back buffer
 * 
 * Utile pour les rendus incrémentaux (on part de l'image affichée).
 */
void display_buffer_copy_front_to_back(void);

/**
 * @brief Copie le back buffer vers le front buffer (sans swap)
 */
void display_buffer_copy_back_to_front(void);

/**
 * @brief Copie une zone entre deux buffers
 * @param srcIndex Buffer source
 * @param dstIndex Buffer destination
 * @param x1 Colonne début
 * @param y1 Ligne début
 * @param x2 Colonne fin
 * @param y2 Ligne fin
 */
void display_buffer_copy_rect(uint8_t srcIndex, uint8_t dstIndex,
                               uint16_t x1, uint16_t y1,
                               uint16_t x2, uint16_t y2);

// ============================================================
// SECTION 10 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche les informations des buffers
 */
void display_buffer_print_info(void);

/**
 * @brief Affiche l'état de tous les buffers
 */
void display_buffer_print_all(void);

/**
 * @brief Vérifie l'intégrité d'un buffer
 * @param bufferIndex Index du buffer
 * @return true si le buffer est valide
 */
bool display_buffer_check_integrity(uint8_t bufferIndex);

/**
 * @brief Remplit un buffer avec un motif de test
 * @param bufferIndex Index du buffer
 */
void display_buffer_test_pattern(uint8_t bufferIndex);

/**
 * @brief Récupère les statistiques d'utilisation
 * @param totalSwaps Nombre total d'échanges (sortie)
 * @param totalFrames Nombre total de trames (sortie)
 * @param tearCount Nombre de déchirements (sortie)
 */
void display_buffer_get_stats(uint32_t* totalSwaps, 
                               uint32_t* totalFrames,
                               uint32_t* tearCount);

// ============================================================
// SECTION 11 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Calcule l'offset d'un pixel dans le buffer
 * @param x Colonne
 * @param y Ligne
 * @return Offset en pixels
 */
#define BUFFER_PIXEL_OFFSET(x, y)       ((y) * LTDC_WIDTH + (x))

/**
 * @brief Calcule l'adresse d'un pixel dans un buffer
 * @param buffer Pointeur vers le buffer
 * @param x Colonne
 * @param y Ligne
 * @return Adresse du pixel
 */
#define BUFFER_PIXEL_ADDR(buffer, x, y)  (&(buffer)[BUFFER_PIXEL_OFFSET(x, y)])

/**
 * @brief Vérifie si des coordonnées sont dans les limites
 * @param x Colonne
 * @param y Ligne
 * @return true si valide
 */
#define BUFFER_IS_VALID_POS(x, y)       ((x) < LTDC_WIDTH && (y) < LTDC_HEIGHT)

// ============================================================
// SECTION 12 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define BUFFER_DEBUG(fmt, ...)      printf("[BUFFER] " fmt, ##__VA_ARGS__)
#else
    #define BUFFER_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 13 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_BUFFER_H