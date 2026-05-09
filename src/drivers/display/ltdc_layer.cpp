/**
 * @file ltdc_layer.cpp
 * @brief Implémentation de la gestion des couches LTDC
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans ltdc_layer.h.
 * 
 * Il gère :
 * - L'initialisation et la configuration des couches
 * - Le positionnement et le dimensionnement
 * - Le double buffering par couche
 * - La transparence et le blending
 * - Les animations (slide, zoom, fade)
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ltdc_layer.h"
#include "ili9488_defs.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================
// HANDLES EXTERNES
// ============================================================

/** @brief Handle LTDC */
extern LTDC_HandleTypeDef hltdc;

/** @brief Handle DMA2D pour les accélérations graphiques */
extern DMA2D_HandleTypeDef hdma2d;

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief États des couches */
static LTDC_LayerState layer_states[LTDC_MAX_LAYERS];

/** @brief État d'occupation des couches */
static bool layer_allocated[LTDC_MAX_LAYERS] = {false, false};

// ============================================================
// SECTION 1 : CONVERSION DE FORMATS
// ============================================================

/**
 * @brief Convertit un format de pixel en nombre d'octets par pixel
 */
static uint8_t get_bytes_per_pixel(LTDC_LayerPixelFormat format)
{
    switch (format)
    {
        case LTDC_LAYER_FORMAT_ARGB8888: return 4;
        case LTDC_LAYER_FORMAT_RGB888:   return 3;
        case LTDC_LAYER_FORMAT_RGB565:   return 2;
        case LTDC_LAYER_FORMAT_ARGB1555: return 2;
        case LTDC_LAYER_FORMAT_ARGB4444: return 2;
        case LTDC_LAYER_FORMAT_L8:       return 1;
        case LTDC_LAYER_FORMAT_AL44:     return 1;
        case LTDC_LAYER_FORMAT_AL88:     return 2;
        default:                         return 2;
    }
}

/**
 * @brief Convertit le format vers la constante HAL LTDC
 */
static uint32_t get_hal_pixel_format(LTDC_LayerPixelFormat format)
{
    switch (format)
    {
        case LTDC_LAYER_FORMAT_ARGB8888: return LTDC_PIXFORMAT_ARGB8888;
        case LTDC_LAYER_FORMAT_RGB888:   return LTDC_PIXFORMAT_RGB888;
        case LTDC_LAYER_FORMAT_RGB565:   return LTDC_PIXFORMAT_RGB565;
        case LTDC_LAYER_FORMAT_ARGB1555: return LTDC_PIXFORMAT_ARGB1555;
        case LTDC_LAYER_FORMAT_ARGB4444: return LTDC_PIXFORMAT_ARGB4444;
        case LTDC_LAYER_FORMAT_L8:       return LTDC_PIXFORMAT_L8;
        case LTDC_LAYER_FORMAT_AL44:     return LTDC_PIXFORMAT_AL44;
        case LTDC_LAYER_FORMAT_AL88:     return LTDC_PIXFORMAT_AL88;
        default:                         return LTDC_PIXFORMAT_RGB565;
    }
}

// ============================================================
// SECTION 2 : VALIDATION
// ============================================================

/**
 * @brief Vérifie la validité d'une configuration de couche
 */
bool ltdc_layer_validate_config(const LTDC_LayerConfig* config)
{
    if (config == NULL) return false;
    
    // Vérifier l'index
    if (config->layerIndex >= LTDC_MAX_LAYERS) return false;
    
    // Vérifier la fenêtre
    if (config->windowX1 > config->windowX2) return false;
    if (config->windowY1 > config->windowY2) return false;
    if (config->windowX2 >= LTDC_WIDTH) return false;
    if (config->windowY2 >= LTDC_HEIGHT) return false;
    
    // Vérifier le framebuffer
    if (config->framebufferAddr == 0) return false;
    if (config->framebufferAddr < SDRAM_BASE_ADDR) return false;
    
    // Vérifier l'alpha
    if (config->alpha > 255) return false;
    
    return true;
}

/**
 * @brief Crée une configuration par défaut pour une couche
 */
void ltdc_layer_get_default_config(uint8_t layerIndex, LTDC_LayerConfig* config)
{
    if (config == NULL) return;
    
    memset(config, 0, sizeof(LTDC_LayerConfig));
    
    config->layerIndex = layerIndex;
    config->enabled = (layerIndex == LTDC_LAYER_BACKGROUND);  // Fond actif par défaut
    
    // Plein écran
    config->windowX1 = 0;
    config->windowY1 = 0;
    config->windowX2 = LTDC_WIDTH - 1;
    config->windowY2 = LTDC_HEIGHT - 1;
    
    // Format standard
    config->pixelFormat = LTDC_LAYER_FORMAT_RGB565;
    config->bytesPerPixel = 2;
    config->linePitch = LTDC_WIDTH * 2;
    
    // Framebuffer (différent pour chaque couche)
    config->framebufferSize = LTDC_LAYER_FULLSCREEN_SIZE;
    
    if (layerIndex == LTDC_LAYER_BACKGROUND)
    {
        config->framebufferAddr = LTDC_FRAMEBUFFER_ADDR;
        config->backBufferAddr = LTDC_FRAMEBUFFER2_ADDR;
    }
    else
    {
        // Couche 1 : après les buffers de la couche 0
        config->framebufferAddr = LTDC_FRAMEBUFFER2_ADDR + LTDC_LAYER_FULLSCREEN_SIZE;
        config->backBufferAddr = config->framebufferAddr + LTDC_LAYER_FULLSCREEN_SIZE;
    }
    
    config->frontBufferAddr = config->framebufferAddr;
    
    // Apparence
    config->alpha = 255;  // Opaque
    config->defaultColor = 0x00000000;  // Noir transparent
    config->blendMode = LTDC_BLEND_ALPHA_CONSTANT;
    
    // Double buffering désactivé par défaut
    config->doubleBuffer = false;
    
    // Dimensions de l'image = dimensions de la fenêtre
    config->imageWidth = LTDC_WIDTH;
    config->imageHeight = LTDC_HEIGHT;
}

// ============================================================
// SECTION 3 : INITIALISATION
// ============================================================

/**
 * @brief Initialise une couche LTDC
 */
bool ltdc_layer_init(uint8_t layerIndex, const LTDC_LayerConfig* config)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return false;
    if (config == NULL) return false;
    if (!ltdc_layer_validate_config(config)) return false;
    
    LTDC_LayerState* state = &layer_states[layerIndex];
    
    // Sauvegarder la configuration
    memcpy(&state->config, config, sizeof(LTDC_LayerConfig));
    state->initialized = true;
    state->needsUpdate = true;
    state->frameCount = 0;
    state->updateCount = 0;
    
    // Appliquer la configuration au matériel
    LTDC_LayerCfgTypeDef halConfig = {0};
    
    // Fenêtre
    halConfig.WindowX0 = config->windowX1;
    halConfig.WindowX1 = config->windowX2;
    halConfig.WindowY0 = config->windowY1;
    halConfig.WindowY1 = config->windowY2;
    
    // Format
    halConfig.PixelFormat = get_hal_pixel_format(config->pixelFormat);
    
    // Framebuffer
    halConfig.FBStartAdress = config->framebufferAddr;
    
    // Alpha
    halConfig.Alpha = config->alpha;
    halConfig.Alpha0 = 0;
    
    // Couleur par défaut
    halConfig.Backcolor.Red   = (config->defaultColor >> 16) & 0xFF;
    halConfig.Backcolor.Green = (config->defaultColor >> 8) & 0xFF;
    halConfig.Backcolor.Blue  = config->defaultColor & 0xFF;
    
    // Dimensions de l'image source
    halConfig.ImageWidth  = config->imageWidth;
    halConfig.ImageHeight = config->imageHeight;
    
    // Blending
    halConfig.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    halConfig.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    
    if (HAL_LTDC_ConfigLayer(&hltdc, &halConfig, layerIndex) != HAL_OK)
    {
        state->initialized = false;
        return false;
    }
    
    // Activer la couche si nécessaire
    if (config->enabled)
    {
        __HAL_LTDC_LAYER_ENABLE(&hltdc, layerIndex);
    }
    
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
    
    layer_allocated[layerIndex] = true;
    
    return true;
}

/**
 * @brief Désinitialise une couche
 */
void ltdc_layer_deinit(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    __HAL_LTDC_LAYER_DISABLE(&hltdc, layerIndex);
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
    
    memset(&layer_states[layerIndex], 0, sizeof(LTDC_LayerState));
    layer_allocated[layerIndex] = false;
}

/**
 * @brief Vérifie si une couche est prête
 */
bool ltdc_layer_is_ready(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return false;
    return layer_states[layerIndex].initialized;
}

/**
 * @brief Récupère l'état d'une couche
 */
LTDC_LayerState* ltdc_layer_get_state(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return NULL;
    return &layer_states[layerIndex];
}

// ============================================================
// SECTION 4 : CONTRÔLE DES COUCHES
// ============================================================

/**
 * @brief Active une couche
 */
void ltdc_layer_enable(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    if (!layer_states[layerIndex].initialized) return;
    
    __HAL_LTDC_LAYER_ENABLE(&hltdc, layerIndex);
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
    layer_states[layerIndex].config.enabled = true;
}

/**
 * @brief Désactive une couche
 */
void ltdc_layer_disable(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    __HAL_LTDC_LAYER_DISABLE(&hltdc, layerIndex);
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
    layer_states[layerIndex].config.enabled = false;
}

/**
 * @brief Vérifie si une couche est active
 */
bool ltdc_layer_is_enabled(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return false;
    return layer_states[layerIndex].config.enabled;
}

/**
 * @brief Définit la visibilité d'une couche
 */
void ltdc_layer_set_visible(uint8_t layerIndex, bool visible)
{
    if (visible)
        ltdc_layer_enable(layerIndex);
    else
        ltdc_layer_disable(layerIndex);
}

/**
 * @brief Bascule la visibilité
 */
void ltdc_layer_toggle_visible(uint8_t layerIndex)
{
    if (ltdc_layer_is_enabled(layerIndex))
        ltdc_layer_disable(layerIndex);
    else
        ltdc_layer_enable(layerIndex);
}

// ============================================================
// SECTION 5 : POSITIONNEMENT
// ============================================================

/**
 * @brief Définit la position d'une couche
 */
void ltdc_layer_set_position(uint8_t layerIndex,
                              uint16_t x1, uint16_t y1,
                              uint16_t x2, uint16_t y2)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    // Limiter à l'écran
    if (x2 >= LTDC_WIDTH)  x2 = LTDC_WIDTH - 1;
    if (y2 >= LTDC_HEIGHT) y2 = LTDC_HEIGHT - 1;
    if (x1 > x2) x1 = x2;
    if (y1 > y2) y1 = y2;
    
    LTDC_LayerState* state = &layer_states[layerIndex];
    
    state->config.windowX1 = x1;
    state->config.windowY1 = y1;
    state->config.windowX2 = x2;
    state->config.windowY2 = y2;
    
    // Appliquer au matériel
    hltdc.LayerCfg[layerIndex].WindowX0 = x1;
    hltdc.LayerCfg[layerIndex].WindowX1 = x2;
    hltdc.LayerCfg[layerIndex].WindowY0 = y1;
    hltdc.LayerCfg[layerIndex].WindowY1 = y2;
    
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
}

/**
 * @brief Centre une couche sur l'écran
 */
void ltdc_layer_center(uint8_t layerIndex, uint16_t width, uint16_t height)
{
    uint16_t x1 = (LTDC_WIDTH - width) / 2;
    uint16_t y1 = (LTDC_HEIGHT - height) / 2;
    uint16_t x2 = x1 + width - 1;
    uint16_t y2 = y1 + height - 1;
    
    ltdc_layer_set_position(layerIndex, x1, y1, x2, y2);
}

/**
 * @brief Met une couche en plein écran
 */
void ltdc_layer_fullscreen(uint8_t layerIndex)
{
    ltdc_layer_set_position(layerIndex, 0, 0, LTDC_WIDTH - 1, LTDC_HEIGHT - 1);
}

/**
 * @brief Déplace une couche
 */
void ltdc_layer_move(uint8_t layerIndex, int16_t dx, int16_t dy)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    LTDC_LayerState* state = &layer_states[layerIndex];
    
    int32_t new_x1 = (int32_t)state->config.windowX1 + dx;
    int32_t new_y1 = (int32_t)state->config.windowY1 + dy;
    uint16_t width  = state->config.windowX2 - state->config.windowX1;
    uint16_t height = state->config.windowY2 - state->config.windowY1;
    
    // Limiter à l'écran
    if (new_x1 < 0) new_x1 = 0;
    if (new_y1 < 0) new_y1 = 0;
    if (new_x1 + width >= LTDC_WIDTH)  new_x1 = LTDC_WIDTH - width - 1;
    if (new_y1 + height >= LTDC_HEIGHT) new_y1 = LTDC_HEIGHT - height - 1;
    
    ltdc_layer_set_position(layerIndex, 
                             (uint16_t)new_x1, (uint16_t)new_y1,
                             (uint16_t)(new_x1 + width), (uint16_t)(new_y1 + height));
}

/**
 * @brief Récupère la position d'une couche
 */
void ltdc_layer_get_position(uint8_t layerIndex,
                              uint16_t* x1, uint16_t* y1,
                              uint16_t* x2, uint16_t* y2)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    LTDC_LayerState* state = &layer_states[layerIndex];
    
    if (x1) *x1 = state->config.windowX1;
    if (y1) *y1 = state->config.windowY1;
    if (x2) *x2 = state->config.windowX2;
    if (y2) *y2 = state->config.windowY2;
}

// ============================================================
// SECTION 6 : GESTION DU FRAMEBUFFER
// ============================================================

/**
 * @brief Définit l'adresse du framebuffer
 */
void ltdc_layer_set_framebuffer(uint8_t layerIndex, uint32_t addr)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    layer_states[layerIndex].config.framebufferAddr = addr;
    hltdc.LayerCfg[layerIndex].FBStartAdress = addr;
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
}

/**
 * @brief Récupère l'adresse du framebuffer
 */
uint32_t ltdc_layer_get_framebuffer(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return 0;
    return layer_states[layerIndex].config.framebufferAddr;
}

/**
 * @brief Efface le framebuffer d'une couche
 */
void ltdc_layer_clear(uint8_t layerIndex, uint16_t color)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    uint32_t addr = ltdc_layer_get_framebuffer(layerIndex);
    uint32_t size = LTDC_LAYER_FULLSCREEN_SIZE;
    
    // Utiliser DMA2D pour un remplissage rapide (matériel)
    hdma2d.Instance = DMA2D;
    
    // Attendre que le DMA2D soit libre
    while (DMA2D->CR & DMA2D_CR_START);
    
    DMA2D->CR = DMA2D_R2M;                    // Register to Memory
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;       // Format sortie
    DMA2D->OMAR = addr;                        // Adresse destination
    DMA2D->OOR = 0;                            // Offset de ligne (0 = pas de saut)
    DMA2D->OCOLR = color;                      // Couleur de remplissage
    DMA2D->NLR = (LTDC_WIDTH << 16) | LTDC_HEIGHT;  // Largeur/Hauteur
    DMA2D->CR |= DMA2D_CR_START;               // Démarrer
    
    // Attendre la fin
    while (DMA2D->CR & DMA2D_CR_START);
}

/**
 * @brief Remplit une zone rectangulaire
 */
void ltdc_layer_fill_rect(uint8_t layerIndex,
                           uint16_t x1, uint16_t y1,
                           uint16_t x2, uint16_t y2,
                           uint16_t color)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    // Limiter aux dimensions de la couche
    if (x1 > x2) { uint16_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    if (x2 >= LTDC_WIDTH)  x2 = LTDC_WIDTH - 1;
    if (y2 >= LTDC_HEIGHT) y2 = LTDC_HEIGHT - 1;
    
    uint16_t width  = x2 - x1 + 1;
    uint16_t height = y2 - y1 + 1;
    
    uint32_t addr = ltdc_layer_get_framebuffer(layerIndex);
    addr += (y1 * LTDC_WIDTH + x1) * LTDC_BYTES_PER_PIXEL;
    
    // Utiliser DMA2D pour le remplissage
    while (DMA2D->CR & DMA2D_CR_START);
    
    DMA2D->CR = DMA2D_R2M;
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
    DMA2D->OMAR = addr;
    DMA2D->OOR = (LTDC_WIDTH - width);         // Saut de ligne
    DMA2D->OCOLR = color;
    DMA2D->NLR = (width << 16) | height;
    DMA2D->CR |= DMA2D_CR_START;
    
    while (DMA2D->CR & DMA2D_CR_START);
}

// ============================================================
// SECTION 7 : DOUBLE BUFFERING
// ============================================================

/**
 * @brief Active le double buffering
 */
void ltdc_layer_double_buffer_enable(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    LTDC_LayerState* state = &layer_states[layerIndex];
    state->config.doubleBuffer = true;
    
    // Initialiser les deux buffers
    state->config.frontBufferAddr = state->config.framebufferAddr;
    state->config.backBufferAddr = state->config.framebufferAddr + LTDC_LAYER_FULLSCREEN_SIZE;
    
    // Effacer les deux buffers
    uint16_t black = ILI9488_BLACK;
    ltdc_layer_clear(layerIndex, black);
    
    // Copier dans le back buffer aussi
    uint32_t front = state->config.frontBufferAddr;
    uint32_t back  = state->config.backBufferAddr;
    memcpy((void*)back, (void*)front, LTDC_LAYER_FULLSCREEN_SIZE);
}

/**
 * @brief Désactive le double buffering
 */
void ltdc_layer_double_buffer_disable(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    LTDC_LayerState* state = &layer_states[layerIndex];
    state->config.doubleBuffer = false;
    
    // Revenir au framebuffer principal
    ltdc_layer_set_framebuffer(layerIndex, state->config.frontBufferAddr);
}

/**
 * @brief Échange les buffers (swap)
 */
void ltdc_layer_swap_buffers(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    LTDC_LayerState* state = &layer_states[layerIndex];
    
    if (!state->config.doubleBuffer) return;
    
    // Échanger les adresses
    uint32_t temp = state->config.frontBufferAddr;
    state->config.frontBufferAddr = state->config.backBufferAddr;
    state->config.backBufferAddr = temp;
    
    // Mettre à jour l'adresse affichée
    ltdc_layer_set_framebuffer(layerIndex, state->config.frontBufferAddr);
    
    state->lastSwapTime = HAL_GetTick();
    state->updateCount++;
}

/**
 * @brief Récupère l'adresse du back buffer
 */
uint32_t ltdc_layer_get_back_buffer(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return 0;
    
    LTDC_LayerState* state = &layer_states[layerIndex];
    
    if (state->config.doubleBuffer)
        return state->config.backBufferAddr;
    else
        return state->config.framebufferAddr;
}

/**
 * @brief Récupère l'adresse du front buffer
 */
uint32_t ltdc_layer_get_front_buffer(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return 0;
    
    LTDC_LayerState* state = &layer_states[layerIndex];
    return state->config.frontBufferAddr;
}

// ============================================================
// SECTION 8 : TRANSPARENCE
// ============================================================

/**
 * @brief Définit la transparence d'une couche
 */
void ltdc_layer_set_alpha(uint8_t layerIndex, uint8_t alpha)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    layer_states[layerIndex].config.alpha = alpha;
    hltdc.LayerCfg[layerIndex].Alpha = alpha;
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
}

/**
 * @brief Récupère la transparence
 */
uint8_t ltdc_layer_get_alpha(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return 255;
    return layer_states[layerIndex].config.alpha;
}

/**
 * @brief Effet de fondu entrant (fade in)
 */
void ltdc_layer_fade_in(uint8_t layerIndex, uint32_t durationMs)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    uint32_t start = HAL_GetTick();
    uint8_t alpha = 0;
    
    ltdc_layer_enable(layerIndex);
    
    while (alpha < 255)
    {
        uint32_t elapsed = HAL_GetTick() - start;
        alpha = (uint8_t)((elapsed * 255) / durationMs);
        if (alpha > 255) alpha = 255;
        
        ltdc_layer_set_alpha(layerIndex, alpha);
        HAL_Delay(10);
    }
    
    ltdc_layer_set_alpha(layerIndex, 255);
}

/**
 * @brief Effet de fondu sortant (fade out)
 */
void ltdc_layer_fade_out(uint8_t layerIndex, uint32_t durationMs)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    uint32_t start = HAL_GetTick();
    uint8_t alpha = 255;
    
    while (alpha > 0)
    {
        uint32_t elapsed = HAL_GetTick() - start;
        alpha = 255 - (uint8_t)((elapsed * 255) / durationMs);
        if (alpha > 255) alpha = 0;  // Underflow guard
        
        ltdc_layer_set_alpha(layerIndex, alpha);
        HAL_Delay(10);
    }
    
    ltdc_layer_disable(layerIndex);
}

/**
 * @brief Définit le mode de blending
 */
void ltdc_layer_set_blend_mode(uint8_t layerIndex, LTDC_BlendMode mode)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    layer_states[layerIndex].config.blendMode = mode;
    
    // Appliquer au matériel
    switch (mode)
    {
        case LTDC_BLEND_NONE:
            hltdc.LayerCfg[layerIndex].BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
            hltdc.LayerCfg[layerIndex].BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
            break;
            
        case LTDC_BLEND_ALPHA_CONSTANT:
            hltdc.LayerCfg[layerIndex].BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
            hltdc.LayerCfg[layerIndex].BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
            break;
            
        case LTDC_BLEND_ALPHA_PIXEL:
            hltdc.LayerCfg[layerIndex].BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
            hltdc.LayerCfg[layerIndex].BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
            break;
            
        case LTDC_BLEND_ALPHA_COMBINED:
            hltdc.LayerCfg[layerIndex].BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
            hltdc.LayerCfg[layerIndex].BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
            break;
    }
    
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
}

// ============================================================
// SECTION 9 : ANIMATIONS
// ============================================================

/**
 * @brief Animation de slide horizontal
 */
void ltdc_layer_slide_x(uint8_t layerIndex, int16_t fromX, int16_t toX, uint32_t durationMs)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    LTDC_LayerState* state = &layer_states[layerIndex];
    uint16_t width = state->config.windowX2 - state->config.windowX1;
    uint16_t y1 = state->config.windowY1;
    uint16_t y2 = state->config.windowY2;
    
    uint32_t start = HAL_GetTick();
    
    while (1)
    {
        uint32_t elapsed = HAL_GetTick() - start;
        if (elapsed >= durationMs) break;
        
        // Interpolation linéaire
        float progress = (float)elapsed / durationMs;
        int16_t currentX = fromX + (int16_t)((toX - fromX) * progress);
        
        ltdc_layer_set_position(layerIndex, currentX, y1, currentX + width, y2);
        
        HAL_Delay(16);  // ~60 FPS
    }
    
    // Position finale
    ltdc_layer_set_position(layerIndex, toX, y1, toX + width, y2);
}

/**
 * @brief Animation de slide vertical
 */
void ltdc_layer_slide_y(uint8_t layerIndex, int16_t fromY, int16_t toY, uint32_t durationMs)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    LTDC_LayerState* state = &layer_states[layerIndex];
    uint16_t height = state->config.windowY2 - state->config.windowY1;
    uint16_t x1 = state->config.windowX1;
    uint16_t x2 = state->config.windowX2;
    
    uint32_t start = HAL_GetTick();
    
    while (1)
    {
        uint32_t elapsed = HAL_GetTick() - start;
        if (elapsed >= durationMs) break;
        
        float progress = (float)elapsed / durationMs;
        int16_t currentY = fromY + (int16_t)((toY - fromY) * progress);
        
        ltdc_layer_set_position(layerIndex, x1, currentY, x2, currentY + height);
        
        HAL_Delay(16);
    }
    
    ltdc_layer_set_position(layerIndex, x1, toY, x2, toY + height);
}

/**
 * @brief Animation de zoom (simulé par redimensionnement de la fenêtre)
 */
void ltdc_layer_zoom(uint8_t layerIndex, uint8_t fromScale, uint8_t toScale, uint32_t durationMs)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    if (fromScale == 0) fromScale = 10;
    if (toScale == 0) toScale = 10;
    
    uint16_t origWidth  = LTDC_WIDTH;
    uint16_t origHeight = LTDC_HEIGHT;
    
    uint32_t start = HAL_GetTick();
    
    while (1)
    {
        uint32_t elapsed = HAL_GetTick() - start;
        if (elapsed >= durationMs) break;
        
        float progress = (float)elapsed / durationMs;
        uint8_t currentScale = fromScale + (uint8_t)((toScale - fromScale) * progress);
        
        uint16_t w = (origWidth * currentScale) / 100;
        uint16_t h = (origHeight * currentScale) / 100;
        
        ltdc_layer_center(layerIndex, w, h);
        
        HAL_Delay(16);
    }
    
    uint16_t finalW = (origWidth * toScale) / 100;
    uint16_t finalH = (origHeight * toScale) / 100;
    ltdc_layer_center(layerIndex, finalW, finalH);
}

// ============================================================
// SECTION 10 : DÉBOGAGE
// ============================================================

/**
 * @brief Affiche les informations d'une couche
 */
void ltdc_layer_print_info(uint8_t layerIndex)
{
    if (layerIndex >= LTDC_MAX_LAYERS) return;
    
    LTDC_LayerState* state = &layer_states[layerIndex];
    
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║         COUCHE LTDC %d                         ║\n", layerIndex);
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Initialisée : %s                             ║\n", state->initialized ? "Oui" : "Non");
    printf("║ Active      : %s                             ║\n", state->config.enabled ? "Oui" : "Non");
    printf("║ Alpha       : %d                             ║\n", state->config.alpha);
    printf("║ Position    : (%d,%d) → (%d,%d)              ║\n", 
           state->config.windowX1, state->config.windowY1,
           state->config.windowX2, state->config.windowY2);
    printf("║ Taille      : %d×%d                          ║\n",
           state->config.windowX2 - state->config.windowX1 + 1,
           state->config.windowY2 - state->config.windowY1 + 1);
    printf("║ Framebuffer : 0x%08lX                        ║\n", (unsigned long)state->config.framebufferAddr);
    printf("║ Double Buff : %s                             ║\n", state->config.doubleBuffer ? "Oui" : "Non");
    printf("║ Format      : %d bpp                         ║\n", state->config.bytesPerPixel * 8);
    printf("║ MàJ         : %lu                            ║\n", (unsigned long)state->updateCount);
    printf("╚══════════════════════════════════════════════╝\n\n");
}

/**
 * @brief Affiche toutes les couches
 */
void ltdc_layer_print_all(void)
{
    for (int i = 0; i < LTDC_MAX_LAYERS; i++)
    {
        ltdc_layer_print_info(i);
    }
}