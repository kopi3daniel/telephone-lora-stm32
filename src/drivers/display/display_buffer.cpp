/**
 * @file display_buffer.cpp
 * @brief Implémentation de la gestion des buffers d'affichage
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans display_buffer.h.
 * 
 * Il gère :
 * - L'allocation des buffers en SDRAM
 * - Le double/triple buffering
 * - La synchronisation VSYNC
 * - Les opérations de copie entre buffers
 * - Le dessin direct dans les buffers
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "display_buffer.h"
#include "dma2d_driver.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// HANDLES EXTERNES
// ============================================================

/** @brief Handle DMA2D pour les copies accélérées */
extern DMA2D_HandleTypeDef hdma2d;

/** @brief Handle LTDC */
extern LTDC_HandleTypeDef hltdc;

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Configuration globale des buffers */
static DisplayBufferConfig buffer_config;

/** @brief Flag VSYNC (positionné par l'IRQ LTDC) */
static volatile bool vsync_flag = false;

/** @brief Compteur VSYNC */
static volatile uint32_t vsync_counter = 0;

/** @brief Flag swap en attente */
static volatile bool swap_pending = false;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le système de buffers
 */
bool display_buffer_init(void)
{
    BUFFER_DEBUG("Initialisation des buffers...\n");
    
    // Réinitialiser la configuration
    memset(&buffer_config, 0, sizeof(DisplayBufferConfig));
    
    // Calculer la taille d'un buffer
    uint32_t bufferSize = LTDC_WIDTH * LTDC_HEIGHT * LTDC_BYTES_PER_PIXEL;
    
    // Allouer les buffers en SDRAM
    uint32_t baseAddr = DISPLAY_BUFFER_BASE_ADDR;
    
    // Buffer 0 : Front Buffer (affiché)
    buffer_config.buffers[0].address = baseAddr;
    buffer_config.buffers[0].size = bufferSize;
    buffer_config.buffers[0].width = LTDC_WIDTH;
    buffer_config.buffers[0].height = LTDC_HEIGHT;
    buffer_config.buffers[0].bytesPerPixel = LTDC_BYTES_PER_PIXEL;
    buffer_config.buffers[0].state = BUFFER_STATE_DISPLAYED;
    buffer_config.buffers[0].type = DISPLAY_BUFFER_FRONT;
    buffer_config.buffers[0].frameCount = 0;
    buffer_config.buffers[0].dirty = false;
    
    // Buffer 1 : Back Buffer (dessin)
    buffer_config.buffers[1].address = baseAddr + bufferSize;
    buffer_config.buffers[1].size = bufferSize;
    buffer_config.buffers[1].width = LTDC_WIDTH;
    buffer_config.buffers[1].height = LTDC_HEIGHT;
    buffer_config.buffers[1].bytesPerPixel = LTDC_BYTES_PER_PIXEL;
    buffer_config.buffers[1].state = BUFFER_STATE_FREE;
    buffer_config.buffers[1].type = DISPLAY_BUFFER_BACK;
    buffer_config.buffers[1].frameCount = 0;
    buffer_config.buffers[1].dirty = false;
    
    buffer_config.bufferCount = 2;
    buffer_config.activeFrontBuffer = 0;
    buffer_config.activeBackBuffer = 1;
    buffer_config.doubleBufferingEnabled = false;
    buffer_config.vsyncEnabled = false;
    
    // Effacer les deux buffers
    dma2d_clear_framebuffer(buffer_config.buffers[0].address);
    dma2d_clear_framebuffer(buffer_config.buffers[1].address);
    
    // Configurer le LTDC pour utiliser le front buffer
    ltdc_set_framebuffer(buffer_config.buffers[0].address);
    
    BUFFER_DEBUG("Buffers initialisés\n");
    BUFFER_DEBUG("Front: 0x%08lX, Back: 0x%08lX\n",
                 (unsigned long)buffer_config.buffers[0].address,
                 (unsigned long)buffer_config.buffers[1].address);
    
    return true;
}

/**
 * @brief Alloue un nouveau buffer
 */
int8_t display_buffer_allocate(DisplayBufferType type, uint16_t width, uint16_t height)
{
    if (buffer_config.bufferCount >= DISPLAY_MAX_BUFFERS)
    {
        BUFFER_DEBUG("Plus de place pour allouer un buffer\n");
        return -1;
    }
    
    uint8_t index = buffer_config.bufferCount;
    uint32_t bufferSize = width * height * LTDC_BYTES_PER_PIXEL;
    uint32_t addr;
    
    if (index == 0)
        addr = DISPLAY_BUFFER_BASE_ADDR;
    else
        addr = buffer_config.buffers[index - 1].address + buffer_config.buffers[index - 1].size;
    
    // Vérifier qu'on ne dépasse pas la SDRAM
    if (addr + bufferSize > SDRAM_BASE_ADDR + SDRAM_SIZE)
    {
        BUFFER_DEBUG("Dépassement SDRAM\n");
        return -1;
    }
    
    // Configurer le buffer
    buffer_config.buffers[index].address = addr;
    buffer_config.buffers[index].size = bufferSize;
    buffer_config.buffers[index].width = width;
    buffer_config.buffers[index].height = height;
    buffer_config.buffers[index].bytesPerPixel = LTDC_BYTES_PER_PIXEL;
    buffer_config.buffers[index].state = BUFFER_STATE_FREE;
    buffer_config.buffers[index].type = type;
    buffer_config.buffers[index].frameCount = 0;
    buffer_config.buffers[index].dirty = false;
    
    buffer_config.bufferCount++;
    
    // Effacer le nouveau buffer
    dma2d_clear_framebuffer(addr);
    
    BUFFER_DEBUG("Buffer %d alloué: 0x%08lX (%lu octets)\n",
                 index, (unsigned long)addr, (unsigned long)bufferSize);
    
    return index;
}

/**
 * @brief Libère un buffer
 */
void display_buffer_free(uint8_t bufferIndex)
{
    if (bufferIndex >= buffer_config.bufferCount) return;
    
    // Effacer le buffer
    dma2d_clear_framebuffer(buffer_config.buffers[bufferIndex].address);
    
    // Marquer comme libre
    buffer_config.buffers[bufferIndex].state = BUFFER_STATE_FREE;
    buffer_config.buffers[bufferIndex].type = (DisplayBufferType)0;
}

/**
 * @brief Réinitialise tous les buffers
 */
void display_buffer_reset(void)
{
    for (uint8_t i = 0; i < buffer_config.bufferCount; i++)
    {
        dma2d_clear_framebuffer(buffer_config.buffers[i].address);
        buffer_config.buffers[i].state = BUFFER_STATE_FREE;
        buffer_config.buffers[i].dirty = false;
        buffer_config.buffers[i].frameCount = 0;
    }
    
    buffer_config.activeFrontBuffer = 0;
    buffer_config.activeBackBuffer = 1;
    buffer_config.totalSwaps = 0;
    buffer_config.totalFrames = 0;
    buffer_config.tearCount = 0;
    
    ltdc_set_framebuffer(buffer_config.buffers[0].address);
}

/**
 * @brief Vérifie si les buffers sont prêts
 */
bool display_buffer_is_ready(void)
{
    return (buffer_config.bufferCount >= 2);
}

// ============================================================
// SECTION 2 : ACCÈS AUX BUFFERS
// ============================================================

/**
 * @brief Récupère le front buffer (affiché)
 */
uint16_t* display_buffer_get_front(void)
{
    return (uint16_t*)buffer_config.buffers[buffer_config.activeFrontBuffer].address;
}

/**
 * @brief Récupère le back buffer (dessin)
 */
uint16_t* display_buffer_get_back(void)
{
    return (uint16_t*)buffer_config.buffers[buffer_config.activeBackBuffer].address;
}

/**
 * @brief Récupère un buffer par type
 */
uint16_t* display_buffer_get_by_type(DisplayBufferType type)
{
    for (uint8_t i = 0; i < buffer_config.bufferCount; i++)
    {
        if (buffer_config.buffers[i].type == type)
        {
            return (uint16_t*)buffer_config.buffers[i].address;
        }
    }
    return NULL;
}

/**
 * @brief Récupère un buffer par index
 */
uint16_t* display_buffer_get_by_index(uint8_t index)
{
    if (index >= buffer_config.bufferCount) return NULL;
    return (uint16_t*)buffer_config.buffers[index].address;
}

/**
 * @brief Récupère l'adresse physique d'un buffer
 */
uint32_t display_buffer_get_address(uint8_t index)
{
    if (index >= buffer_config.bufferCount) return 0;
    return buffer_config.buffers[index].address;
}

/**
 * @brief Récupère la taille d'un buffer
 */
uint32_t display_buffer_get_size(uint8_t index)
{
    if (index >= buffer_config.bufferCount) return 0;
    return buffer_config.buffers[index].size;
}

// ============================================================
// SECTION 3 : DOUBLE BUFFERING
// ============================================================

/**
 * @brief Active le double buffering
 */
void display_buffer_double_enable(void)
{
    if (buffer_config.bufferCount < 2)
    {
        BUFFER_DEBUG("Pas assez de buffers pour le double buffering\n");
        return;
    }
    
    buffer_config.doubleBufferingEnabled = true;
    
    // Marquer les buffers
    buffer_config.buffers[0].state = BUFFER_STATE_DISPLAYED;
    buffer_config.buffers[1].state = BUFFER_STATE_FREE;
    
    BUFFER_DEBUG("Double buffering activé\n");
}

/**
 * @brief Désactive le double buffering
 */
void display_buffer_double_disable(void)
{
    buffer_config.doubleBufferingEnabled = false;
    BUFFER_DEBUG("Double buffering désactivé\n");
}

/**
 * @brief Active le triple buffering
 */
void display_buffer_triple_enable(void)
{
    if (buffer_config.bufferCount < 3)
    {
        // Allouer un troisième buffer
        if (display_buffer_allocate(DISPLAY_BUFFER_EXTRA, LTDC_WIDTH, LTDC_HEIGHT) < 0)
        {
            BUFFER_DEBUG("Impossible d'allouer le 3ème buffer\n");
            return;
        }
    }
    
    buffer_config.tripleBufferingEnabled = true;
    BUFFER_DEBUG("Triple buffering activé\n");
}

/**
 * @brief Échange les buffers (swap)
 */
void display_buffer_swap(bool waitVsync)
{
    if (!buffer_config.doubleBufferingEnabled) return;
    
    if (waitVsync)
    {
        // Attendre le prochain VSYNC
        display_buffer_wait_vblank(100);  // 100 ms timeout
    }
    
    // Échanger les index
    uint8_t temp = buffer_config.activeFrontBuffer;
    buffer_config.activeFrontBuffer = buffer_config.activeBackBuffer;
    buffer_config.activeBackBuffer = temp;
    
    // Mettre à jour les états
    buffer_config.buffers[buffer_config.activeFrontBuffer].state = BUFFER_STATE_DISPLAYED;
    buffer_config.buffers[buffer_config.activeFrontBuffer].frameCount++;
    buffer_config.buffers[buffer_config.activeFrontBuffer].dirty = false;
    
    buffer_config.buffers[buffer_config.activeBackBuffer].state = BUFFER_STATE_FREE;
    
    // Mettre à jour l'adresse du framebuffer LTDC
    ltdc_set_framebuffer(buffer_config.buffers[buffer_config.activeFrontBuffer].address);
    
    buffer_config.totalSwaps++;
    buffer_config.totalFrames++;
    
    BUFFER_DEBUG("Swap #%lu\n", (unsigned long)buffer_config.totalSwaps);
}

/**
 * @brief Force l'échange immédiat (sans VSYNC)
 */
void display_buffer_swap_immediate(void)
{
    display_buffer_swap(false);
}

/**
 * @brief Vérifie si un swap est en attente
 */
bool display_buffer_swap_pending(void)
{
    return swap_pending;
}

// ============================================================
// SECTION 4 : SYNCHRONISATION VSYNC
// ============================================================

/**
 * @brief Active la synchronisation VSYNC
 */
void display_buffer_vsync_enable(void)
{
    buffer_config.vsyncEnabled = true;
    BUFFER_DEBUG("VSYNC activé\n");
}

/**
 * @brief Désactive la synchronisation VSYNC
 */
void display_buffer_vsync_disable(void)
{
    buffer_config.vsyncEnabled = false;
    BUFFER_DEBUG("VSYNC désactivé\n");
}

/**
 * @brief Vérifie si on est en période de blanking vertical
 */
bool display_buffer_is_vblank(void)
{
    // Le flag est positionné par l'IRQ LTDC (fin de trame)
    if (vsync_flag)
    {
        vsync_flag = false;
        return true;
    }
    return false;
}

/**
 * @brief Attend la prochaine période de blanking vertical
 */
bool display_buffer_wait_vblank(uint32_t timeoutMs)
{
    uint32_t start = HAL_GetTick();
    
    vsync_flag = false;
    
    while (!vsync_flag)
    {
        if ((HAL_GetTick() - start) > timeoutMs)
        {
            BUFFER_DEBUG("Timeout attente VSYNC\n");
            return false;
        }
        
        // Petite pause pour éviter de saturer le CPU
        __WFI();  // Wait For Interrupt
    }
    
    vsync_flag = false;
    return true;
}

/**
 * @brief Callback appelé à chaque VSYNC
 */
void display_buffer_vsync_callback(void)
{
    vsync_flag = true;
    vsync_counter++;
    
    // Si un swap est en attente, l'exécuter
    if (swap_pending && buffer_config.doubleBufferingEnabled)
    {
        display_buffer_swap(false);
        swap_pending = false;
    }
}

// ============================================================
// SECTION 5 : DESSIN DIRECT DANS LES BUFFERS
// ============================================================

/**
 * @brief Efface le back buffer
 */
void display_buffer_clear_back(uint16_t color)
{
    uint32_t addr = buffer_config.buffers[buffer_config.activeBackBuffer].address;
    dma2d_clear_framebuffer(addr);
    
    // Alternative avec DMA2D pour une couleur spécifique :
    // dma2d_fill_screen ne peut être utilisée que si l'adresse est la bonne
    // On refait un clear avec la couleur
    DMA2D_FillConfig config = {
        .dstAddress = addr,
        .dstWidth = LTDC_WIDTH,
        .dstHeight = LTDC_HEIGHT,
        .dstOffset = 0,
        .fillColor = color,
        .dstFormat = DMA2D_FORMAT_RGB565
    };
    dma2d_fill_rect(&config);
}

/**
 * @brief Efface le front buffer
 */
void display_buffer_clear_front(uint16_t color)
{
    uint32_t addr = buffer_config.buffers[buffer_config.activeFrontBuffer].address;
    
    DMA2D_FillConfig config = {
        .dstAddress = addr,
        .dstWidth = LTDC_WIDTH,
        .dstHeight = LTDC_HEIGHT,
        .dstOffset = 0,
        .fillColor = color,
        .dstFormat = DMA2D_FORMAT_RGB565
    };
    dma2d_fill_rect(&config);
}

/**
 * @brief Efface un buffer spécifique
 */
void display_buffer_clear(uint8_t bufferIndex, uint16_t color)
{
    if (bufferIndex >= buffer_config.bufferCount) return;
    
    uint32_t addr = buffer_config.buffers[bufferIndex].address;
    
    DMA2D_FillConfig config = {
        .dstAddress = addr,
        .dstWidth = buffer_config.buffers[bufferIndex].width,
        .dstHeight = buffer_config.buffers[bufferIndex].height,
        .dstOffset = 0,
        .fillColor = color,
        .dstFormat = DMA2D_FORMAT_RGB565
    };
    dma2d_fill_rect(&config);
}

/**
 * @brief Remplit un rectangle dans le back buffer
 */
void display_buffer_fill_rect_back(uint16_t x1, uint16_t y1,
                                    uint16_t x2, uint16_t y2,
                                    uint16_t color)
{
    if (x1 > x2) { uint16_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    
    uint32_t addr = buffer_config.buffers[buffer_config.activeBackBuffer].address;
    uint16_t width = x2 - x1 + 1;
    uint16_t height = y2 - y1 + 1;
    
    DMA2D_FillConfig config = {
        .dstAddress = addr + (y1 * LTDC_WIDTH + x1) * LTDC_BYTES_PER_PIXEL,
        .dstWidth = width,
        .dstHeight = height,
        .dstOffset = LTDC_WIDTH - width,
        .fillColor = color,
        .dstFormat = DMA2D_FORMAT_RGB565
    };
    dma2d_fill_rect(&config);
}

/**
 * @brief Définit un pixel dans le back buffer
 */
void display_buffer_set_pixel_back(uint16_t x, uint16_t y, uint16_t color)
{
    if (!BUFFER_IS_VALID_POS(x, y)) return;
    
    uint16_t* backBuf = display_buffer_get_back();
    backBuf[y * LTDC_WIDTH + x] = color;
}

/**
 * @brief Lit un pixel du front buffer
 */
uint16_t display_buffer_get_pixel_front(uint16_t x, uint16_t y)
{
    if (!BUFFER_IS_VALID_POS(x, y)) return 0;
    
    uint16_t* frontBuf = display_buffer_get_front();
    return frontBuf[y * LTDC_WIDTH + x];
}

// ============================================================
// SECTION 6 : COPIE ENTRE BUFFERS
// ============================================================

/**
 * @brief Copie le front buffer vers le back buffer
 */
void display_buffer_copy_front_to_back(void)
{
    uint32_t srcAddr = buffer_config.buffers[buffer_config.activeFrontBuffer].address;
    uint32_t dstAddr = buffer_config.buffers[buffer_config.activeBackBuffer].address;
    
    dma2d_copy_framebuffer(srcAddr, dstAddr);
}

/**
 * @brief Copie le back buffer vers le front buffer
 */
void display_buffer_copy_back_to_front(void)
{
    uint32_t srcAddr = buffer_config.buffers[buffer_config.activeBackBuffer].address;
    uint32_t dstAddr = buffer_config.buffers[buffer_config.activeFrontBuffer].address;
    
    dma2d_copy_framebuffer(srcAddr, dstAddr);
}

/**
 * @brief Copie une zone entre deux buffers
 */
void display_buffer_copy_rect(uint8_t srcIndex, uint8_t dstIndex,
                               uint16_t x1, uint16_t y1,
                               uint16_t x2, uint16_t y2)
{
    if (srcIndex >= buffer_config.bufferCount || dstIndex >= buffer_config.bufferCount) return;
    if (x1 > x2) { uint16_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    
    uint16_t width = x2 - x1 + 1;
    uint16_t height = y2 - y1 + 1;
    
    DMA2D_CopyConfig config = {
        .srcAddress = buffer_config.buffers[srcIndex].address + (y1 * LTDC_WIDTH + x1) * 2,
        .dstAddress = buffer_config.buffers[dstIndex].address + (y1 * LTDC_WIDTH + x1) * 2,
        .width = width,
        .height = height,
        .srcOffset = LTDC_WIDTH - width,
        .dstOffset = LTDC_WIDTH - width,
        .srcFormat = DMA2D_FORMAT_RGB565,
        .dstFormat = DMA2D_FORMAT_RGB565,
        .convertFormat = false
    };
    
    dma2d_copy(&config);
}

// ============================================================
// SECTION 7 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche les informations des buffers
 */
void display_buffer_print_info(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║       INFORMATIONS BUFFERS D'AFFICHAGE       ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Nombre de buffers : %d                        ║\n", buffer_config.bufferCount);
    printf("║ Double buffering  : %s                        ║\n", 
           buffer_config.doubleBufferingEnabled ? "Activé" : "Désactivé");
    printf("║ Triple buffering  : %s                        ║\n",
           buffer_config.tripleBufferingEnabled ? "Activé" : "Désactivé");
    printf("║ VSYNC             : %s                        ║\n",
           buffer_config.vsyncEnabled ? "Activé" : "Désactivé");
    printf("║ Front Buffer      : %d (0x%08lX)             ║\n", 
           buffer_config.activeFrontBuffer,
           (unsigned long)buffer_config.buffers[buffer_config.activeFrontBuffer].address);
    printf("║ Back Buffer       : %d (0x%08lX)             ║\n",
           buffer_config.activeBackBuffer,
           (unsigned long)buffer_config.buffers[buffer_config.activeBackBuffer].address);
    printf("║ Total swaps       : %lu                       ║\n", (unsigned long)buffer_config.totalSwaps);
    printf("║ Total frames      : %lu                       ║\n", (unsigned long)buffer_config.totalFrames);
    printf("║ Tearing détecté   : %lu                       ║\n", (unsigned long)buffer_config.tearCount);
    printf("║ VSYNC counter     : %lu                       ║\n", (unsigned long)vsync_counter);
    printf("╚══════════════════════════════════════════════╝\n\n");
}

/**
 * @brief Affiche l'état de tous les buffers
 */
void display_buffer_print_all(void)
{
    printf("\n═══ ÉTAT DES BUFFERS ═══\n");
    
    for (uint8_t i = 0; i < buffer_config.bufferCount; i++)
    {
        DisplayBuffer* buf = &buffer_config.buffers[i];
        
        const char* stateStr = "Inconnu";
        switch (buf->state)
        {
            case BUFFER_STATE_FREE:      stateStr = "Libre"; break;
            case BUFFER_STATE_DRAWING:   stateStr = "Dessin"; break;
            case BUFFER_STATE_READY:     stateStr = "Prêt"; break;
            case BUFFER_STATE_DISPLAYED: stateStr = "Affiché"; break;
        }
        
        const char* typeStr = "Inconnu";
        switch (buf->type)
        {
            case DISPLAY_BUFFER_FRONT:   typeStr = "Front"; break;
            case DISPLAY_BUFFER_BACK:    typeStr = "Back"; break;
            case DISPLAY_BUFFER_EXTRA:   typeStr = "Extra"; break;
            case DISPLAY_BUFFER_OVERLAY: typeStr = "Overlay"; break;
        }
        
        printf("Buffer %d : [%s] [%s] 0x%08lX (%d×%d) Frames:%lu %s\n",
               i, typeStr, stateStr,
               (unsigned long)buf->address,
               buf->width, buf->height,
               (unsigned long)buf->frameCount,
               buf->dirty ? "(modifié)" : "");
    }
    printf("═══════════════════════\n\n");
}

/**
 * @brief Vérifie l'intégrité d'un buffer
 */
bool display_buffer_check_integrity(uint8_t bufferIndex)
{
    if (bufferIndex >= buffer_config.bufferCount) return false;
    
    DisplayBuffer* buf = &buffer_config.buffers[bufferIndex];
    
    // Vérifier que l'adresse est dans la SDRAM
    if (buf->address < SDRAM_BASE_ADDR || 
        buf->address >= SDRAM_BASE_ADDR + SDRAM_SIZE)
    {
        BUFFER_DEBUG("Buffer %d : adresse hors SDRAM\n", bufferIndex);
        return false;
    }
    
    // Vérifier la taille
    if (buf->size == 0 || buf->size > SDRAM_SIZE)
    {
        BUFFER_DEBUG("Buffer %d : taille invalide\n", bufferIndex);
        return false;
    }
    
    return true;
}

/**
 * @brief Remplit un buffer avec un motif de test
 */
void display_buffer_test_pattern(uint8_t bufferIndex)
{
    if (bufferIndex >= buffer_config.bufferCount) return;
    
    uint16_t* buf = (uint16_t*)buffer_config.buffers[bufferIndex].address;
    uint16_t w = buffer_config.buffers[bufferIndex].width;
    uint16_t h = buffer_config.buffers[bufferIndex].height;
    
    // Motif : barres de couleurs
    uint16_t colors[] = {
        ILI9488_RED, ILI9488_GREEN, ILI9488_BLUE,
        ILI9488_YELLOW, ILI9488_CYAN, ILI9488_MAGENTA,
        ILI9488_WHITE, ILI9488_BLACK
    };
    
    uint16_t barWidth = w / 8;
    
    for (uint16_t y = 0; y < h; y++)
    {
        for (uint16_t x = 0; x < w; x++)
        {
            uint8_t colorIndex = x / barWidth;
            if (colorIndex >= 8) colorIndex = 7;
            buf[y * w + x] = colors[colorIndex];
        }
    }
    
    buffer_config.buffers[bufferIndex].dirty = true;
    BUFFER_DEBUG("Motif de test appliqué au buffer %d\n", bufferIndex);
}

/**
 * @brief Récupère les statistiques
 */
void display_buffer_get_stats(uint32_t* totalSwaps,
                               uint32_t* totalFrames,
                               uint32_t* tearCount)
{
    if (totalSwaps) *totalSwaps = buffer_config.totalSwaps;
    if (totalFrames) *totalFrames = buffer_config.totalFrames;
    if (tearCount) *tearCount = buffer_config.tearCount;
}