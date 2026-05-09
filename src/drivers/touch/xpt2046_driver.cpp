/**
 * @file xpt2046_driver.cpp
 * @brief Implémentation du driver pour le contrôleur tactile XPT2046
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans xpt2046_driver.h.
 * 
 * Il gère :
 * - La communication SPI avec le contrôleur
 * - La lecture des coordonnées brutes (ADC 12 bits)
 * - La calibration et la conversion en pixels
 * - Le filtrage des mesures (médian, moyenne)
 * - La détection des événements (press, move, release)
 * - La reconnaissance de gestes simples
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "xpt2046_driver.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Configuration actuelle */
static XPT2046_Config xpt2046_config = {
    .useI2C = false,
    .i2cAddress = XPT2046_I2C_ADDRESS,
    .samples = XPT2046_SAMPLES_DEFAULT,
    .touchThreshold = XPT2046_TOUCH_THRESHOLD,
    .stabilityThreshold = XPT2046_STABILITY_THRESHOLD,
    .readDelayMs = XPT2046_READ_DELAY_MS,
    .scanIntervalMs = XPT2046_SCAN_INTERVAL_MS,
    .enableMedianFilter = true,
    .enableAverageFilter = true,
    .filterWindow = 5,
    .swapXY = false,
    .invertX = false,
    .invertY = false
};

/** @brief État actuel */
static XPT2046_State xpt2046_state = {
    .state = XPT2046_STATE_IDLE,
    .lastEvent = XPT2046_EVENT_NONE,
    .touched = false,
    .touchCount = 0,
    .errorCount = 0
};

/** @brief Configuration des gestes */
static XPT2046_GestureConfig gesture_config = {
    .enableGestures = false,
    .tapTimeout = 200,
    .doubleTapTimeout = 300,
    .longPressTimeout = 500,
    .swipeThreshold = 30,
    .pinchThreshold = 50
};

/** @brief Historique des positions (pour les gestes) */
#define GESTURE_HISTORY_SIZE    10
static XPT2046_PixelPoint gesture_history[GESTURE_HISTORY_SIZE];
static uint8_t gesture_history_index = 0;

/** @brief Callbacks */
static XPT2046_Callback touch_callback = NULL;
static void (*gesture_callback)(XPT2046_Gesture) = NULL;

/** @brief Dernier geste détecté */
static XPT2046_Gesture last_gesture = XPT2046_GESTURE_NONE;

/** @brief Flags */
static bool initialized = false;
static bool calibration_valid = false;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le contrôleur tactile
 */
XPT2046_Error xpt2046_init(const XPT2046_Config* config)
{
    XPT2046_DEBUG("Initialisation du contrôleur tactile...\n");
    
    // Sauvegarder la configuration
    if (config != NULL)
    {
        memcpy(&xpt2046_config, config, sizeof(XPT2046_Config));
    }
    
    // Initialiser les broches GPIO
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Activer l'horloge GPIO
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    // CS (Chip Select) - Sortie
    GPIO_InitStruct.Pin = TOUCH_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(TOUCH_CS_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_SET);  // Désélectionné
    
    // IRQ (Interruption) - Entrée
    GPIO_InitStruct.Pin = TOUCH_IRQ_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(TOUCH_IRQ_PORT, &GPIO_InitStruct);
    
    // Reset - Sortie
    GPIO_InitStruct.Pin = TOUCH_RST_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TOUCH_RST_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(TOUCH_RST_PORT, TOUCH_RST_PIN, GPIO_PIN_SET);
    
    // Activer l'interruption IRQ
    HAL_NVIC_SetPriority(EXTI1_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    
    // Réinitialiser l'état
    memset(&xpt2046_state, 0, sizeof(XPT2046_State));
    xpt2046_state.state = XPT2046_STATE_IDLE;
    
    // Charger la calibration si disponible
    if (!xpt2046_calibration_load())
    {
        XPT2046_DEBUG("Aucune calibration sauvegardée\n");
        // Initialiser avec des valeurs par défaut
        xpt2046_calibration_reset();
    }
    
    initialized = true;
    
    XPT2046_DEBUG("Initialisation réussie\n");
    return XPT2046_OK;
}

/**
 * @brief Désinitialise le contrôleur
 */
void xpt2046_deinit(void)
{
    HAL_NVIC_DisableIRQ(EXTI1_IRQn);
    initialized = false;
}

/**
 * @brief Vérifie si le contrôleur est prêt
 */
bool xpt2046_is_ready(void)
{
    return initialized;
}

/**
 * @brief Vérifie si le contrôleur répond
 */
bool xpt2046_is_present(void)
{
    // Tenter de lire la position X
    uint16_t x = xpt2046_read_channel(XPT2046_CMD_READ_X);
    uint16_t y = xpt2046_read_channel(XPT2046_CMD_READ_Y);
    
    // Vérifier que les valeurs sont dans une plage raisonnable
    return (x > 0 && x < 4095 && y > 0 && y < 4095);
}

// ============================================================
// SECTION 2 : LECTURE BRUTE
// ============================================================

/**
 * @brief Lit un canal ADC du XPT2046
 */
uint16_t xpt2046_read_channel(uint8_t command)
{
    uint8_t tx_data[3] = {0};
    uint8_t rx_data[3] = {0};
    uint16_t result;
    
    // Sélectionner le contrôleur (CS LOW)
    HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_RESET);
    
    // Envoyer la commande et recevoir la réponse
    tx_data[0] = command;
    HAL_SPI_TransmitReceive(&hspi2, tx_data, rx_data, 3, XPT2046_SPI_TIMEOUT_MS);
    
    // Désélectionner (CS HIGH)
    HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_SET);
    
    // Assembler le résultat 12 bits
    result = ((uint16_t)rx_data[1] << 8) | rx_data[2];
    result >>= 3;  // Décaler pour obtenir 12 bits
    
    return result & 0x0FFF;  // Masquer à 12 bits
}

/**
 * @brief Lit la position X brute
 */
uint16_t xpt2046_read_raw_x(void)
{
    return xpt2046_read_channel(XPT2046_CMD_READ_X);
}

/**
 * @brief Lit la position Y brute
 */
uint16_t xpt2046_read_raw_y(void)
{
    return xpt2046_read_channel(XPT2046_CMD_READ_Y);
}

/**
 * @brief Lit la pression (Z)
 */
uint16_t xpt2046_read_raw_z(void)
{
    uint16_t z1 = xpt2046_read_channel(XPT2046_CMD_READ_Z1);
    uint16_t z2 = xpt2046_read_channel(XPT2046_CMD_READ_Z2);
    
    // La pression est proportionnelle à Z2 - Z1
    return (z2 > z1) ? (z2 - z1) : 0;
}

/**
 * @brief Lit un point complet avec plusieurs échantillons
 */
XPT2046_Error xpt2046_read_raw_point(XPT2046_RawPoint* point)
{
    if (point == NULL) return XPT2046_ERROR_PARAM;
    if (!initialized) return XPT2046_ERROR_INIT;
    
    // Tableaux pour les échantillons
    uint16_t x_samples[XPT2046_SAMPLES_MAX];
    uint16_t y_samples[XPT2046_SAMPLES_MAX];
    
    // Lire plusieurs échantillons
    for (uint8_t i = 0; i < xpt2046_config.samples; i++)
    {
        x_samples[i] = xpt2046_read_channel(XPT2046_CMD_READ_X);
        y_samples[i] = xpt2046_read_channel(XPT2046_CMD_READ_Y);
        
        if (xpt2046_config.readDelayMs > 0)
        {
            HAL_Delay(xpt2046_config.readDelayMs);
        }
    }
    
    // Appliquer les filtres
    if (xpt2046_config.enableMedianFilter)
    {
        point->x = xpt2046_median_filter(x_samples, xpt2046_config.samples);
        point->y = xpt2046_median_filter(y_samples, xpt2046_config.samples);
    }
    else if (xpt2046_config.enableAverageFilter)
    {
        point->x = xpt2046_average_filter(x_samples, xpt2046_config.samples);
        point->y = xpt2046_average_filter(y_samples, xpt2046_config.samples);
    }
    else
    {
        point->x = x_samples[0];
        point->y = y_samples[0];
    }
    
    // Lire la pression
    point->z = xpt2046_read_raw_z();
    
    return XPT2046_OK;
}

/**
 * @brief Lit un point filtré (pour compatibilité)
 */
XPT2046_Error xpt2046_read_filtered_point(XPT2046_RawPoint* point)
{
    return xpt2046_read_raw_point(point);
}

// ============================================================
// SECTION 3 : CALIBRATION
// ============================================================

/**
 * @brief Définit un point de calibration
 */
bool xpt2046_calibration_set_point(uint8_t pointIndex, uint16_t screenX, uint16_t screenY)
{
    if (pointIndex > 2) return false;
    
    XPT2046_DEBUG("Calibration point %d : touchez (%d, %d)\n", pointIndex, screenX, screenY);
    
    // Attendre que l'utilisateur touche l'écran
    uint32_t timeout = HAL_GetTick() + 5000;  // 5 secondes timeout
    
    while (!xpt2046_is_touched())
    {
        xpt2046_scan();
        if (HAL_GetTick() > timeout)
        {
            XPT2046_DEBUG("Timeout calibration\n");
            return false;
        }
        HAL_Delay(10);
    }
    
    // Lire les valeurs ADC
    XPT2046_RawPoint raw;
    xpt2046_read_raw_point(&raw);
    
    // Stocker les valeurs
    switch (pointIndex)
    {
        case 0:
            xpt2046_config.calibration.screenAx = screenX;
            xpt2046_config.calibration.screenAy = screenY;
            xpt2046_config.calibration.adcAx = raw.x;
            xpt2046_config.calibration.adcAy = raw.y;
            break;
            
        case 1:
            xpt2046_config.calibration.screenBx = screenX;
            xpt2046_config.calibration.screenBy = screenY;
            xpt2046_config.calibration.adcBx = raw.x;
            xpt2046_config.calibration.adcBy = raw.y;
            break;
            
        case 2:
            xpt2046_config.calibration.screenCx = screenX;
            xpt2046_config.calibration.screenCy = screenY;
            xpt2046_config.calibration.adcCx = raw.x;
            xpt2046_config.calibration.adcCy = raw.y;
            break;
    }
    
    XPT2046_DEBUG("Point %d : ADC(%d, %d)\n", pointIndex, raw.x, raw.y);
    
    // Attendre que l'utilisateur relâche
    while (xpt2046_is_touched())
    {
        xpt2046_scan();
        HAL_Delay(10);
    }
    
    return true;
}

/**
 * @brief Calcule les coefficients de calibration
 */
bool xpt2046_calibration_calculate(void)
{
    XPT2046_Calibration* cal = &xpt2046_config.calibration;
    
    // Récupérer les valeurs
    float adcAx = cal->adcAx, adcAy = cal->adcAy;
    float adcBx = cal->adcBx, adcBy = cal->adcBy;
    float adcCx = cal->adcCx, adcCy = cal->adcCy;
    
    float screenAx = cal->screenAx, screenAy = cal->screenAy;
    float screenBx = cal->screenBx, screenBy = cal->screenBy;
    float screenCx = cal->screenCx, screenCy = cal->screenCy;
    
    // Résoudre le système d'équations pour X
    float detX = (adcAx - adcCx) * (adcBy - adcCy) - (adcBx - adcCx) * (adcAy - adcCy);
    
    if (fabsf(detX) < 0.001f)
    {
        XPT2046_DEBUG("Erreur : déterminant X nul\n");
        return false;
    }
    
    cal->alphaX = ((screenAx - screenCx) * (adcBy - adcCy) - (screenBx - screenCx) * (adcAy - adcCy)) / detX;
    cal->betaX  = ((adcAx - adcCx) * (screenBx - screenCx) - (adcBx - adcCx) * (screenAx - screenCx)) / detX;
    cal->deltaX = screenAx - cal->alphaX * adcAx - cal->betaX * adcAy;
    
    // Résoudre le système d'équations pour Y
    float detY = (adcAx - adcCx) * (adcBy - adcCy) - (adcBx - adcCx) * (adcAy - adcCy);
    
    if (fabsf(detY) < 0.001f)
    {
        XPT2046_DEBUG("Erreur : déterminant Y nul\n");
        return false;
    }
    
    cal->alphaY = ((screenAy - screenCy) * (adcBy - adcCy) - (screenBy - screenCy) * (adcAy - adcCy)) / detY;
    cal->betaY  = ((adcAx - adcCx) * (screenBy - screenCy) - (adcBx - adcCx) * (screenAy - screenCy)) / detY;
    cal->deltaY = screenAy - cal->alphaY * adcAx - cal->betaY * adcAy;
    
    cal->calibrated = true;
    calibration_valid = true;
    
    XPT2046_DEBUG("Calibration calculée:\n");
    XPT2046_DEBUG("  X = %.4f * adcX + %.4f * adcY + %.4f\n", cal->alphaX, cal->betaX, cal->deltaX);
    XPT2046_DEBUG("  Y = %.4f * adcX + %.4f * adcY + %.4f\n", cal->alphaY, cal->betaY, cal->deltaY);
    
    return true;
}

/**
 * @brief Convertit un point brut en pixels
 */
void xpt2046_calibrate_point(const XPT2046_RawPoint* raw, XPT2046_PixelPoint* pixel)
{
    if (raw == NULL || pixel == NULL) return;
    
    XPT2046_Calibration* cal = &xpt2046_config.calibration;
    
    if (cal->calibrated)
    {
        // Appliquer la transformation calibrée
        float x = cal->alphaX * raw->x + cal->betaX * raw->y + cal->deltaX;
        float y = cal->alphaY * raw->x + cal->betaY * raw->y + cal->deltaY;
        
        pixel->x = (uint16_t)roundf(x);
        pixel->y = (uint16_t)roundf(y);
    }
    else
    {
        // Transformation par défaut (approximative)
        pixel->x = (uint16_t)((uint32_t)raw->x * DISPLAY_WIDTH / XPT2046_ADC_MAX);
        pixel->y = (uint16_t)((uint32_t)raw->y * DISPLAY_HEIGHT / XPT2046_ADC_MAX);
    }
    
    // Appliquer les inversions
    if (xpt2046_config.swapXY)
    {
        uint16_t temp = pixel->x;
        pixel->x = pixel->y;
        pixel->y = temp;
    }
    
    if (xpt2046_config.invertX)
    {
        pixel->x = DISPLAY_WIDTH - 1 - pixel->x;
    }
    
    if (xpt2046_config.invertY)
    {
        pixel->y = DISPLAY_HEIGHT - 1 - pixel->y;
    }
    
    // Limiter aux dimensions de l'écran
    if (pixel->x >= DISPLAY_WIDTH) pixel->x = DISPLAY_WIDTH - 1;
    if (pixel->y >= DISPLAY_HEIGHT) pixel->y = DISPLAY_HEIGHT - 1;
    
    pixel->valid = true;
}

/**
 * @brief Vérifie si la calibration est valide
 */
bool xpt2046_is_calibrated(void)
{
    return calibration_valid && xpt2046_config.calibration.calibrated;
}

/**
 * @brief Sauvegarde la calibration
 */
bool xpt2046_calibration_save(void)
{
    // À implémenter : sauvegarde en Flash ou EEPROM
    XPT2046_DEBUG("Calibration sauvegardée (simulé)\n");
    return true;
}

/**
 * @brief Charge la calibration
 */
bool xpt2046_calibration_load(void)
{
    // À implémenter : chargement depuis Flash ou EEPROM
    XPT2046_DEBUG("Calibration chargée (simulé)\n");
    return false;  // Pas encore de sauvegarde
}

/**
 * @brief Réinitialise la calibration
 */
void xpt2046_calibration_reset(void)
{
    memset(&xpt2046_config.calibration, 0, sizeof(XPT2046_Calibration));
    calibration_valid = false;
    
    // Valeurs par défaut approximatives
    xpt2046_config.calibration.alphaX = (float)DISPLAY_WIDTH / XPT2046_ADC_MAX;
    xpt2046_config.calibration.betaX = 0;
    xpt2046_config.calibration.deltaX = 0;
    
    xpt2046_config.calibration.alphaY = (float)DISPLAY_HEIGHT / XPT2046_ADC_MAX;
    xpt2046_config.calibration.betaY = 0;
    xpt2046_config.calibration.deltaY = 0;
}

// ============================================================
// SECTION 4 : SCAN ET ÉVÉNEMENTS
// ============================================================

/**
 * @brief Scanne l'écran tactile
 */
void xpt2046_scan(void)
{
    if (!initialized) return;
    
    static uint32_t lastScan = 0;
    uint32_t now = HAL_GetTick();
    
    // Respecter l'intervalle de scan
    if ((now - lastScan) < xpt2046_config.scanIntervalMs) return;
    lastScan = now;
    
    // Lire les coordonnées brutes
    XPT2046_RawPoint raw;
    if (xpt2046_read_raw_point(&raw) != XPT2046_OK)
    {
        xpt2046_state.errorCount++;
        return;
    }
    
    // Vérifier si l'écran est touché
    bool currentlyTouched = (raw.z > xpt2046_config.touchThreshold);
    
    if (currentlyTouched && !xpt2046_state.touched)
    {
        // Nouvel appui
        xpt2046_state.state = XPT2046_STATE_PRESSED;
        xpt2046_state.lastEvent = XPT2046_EVENT_PRESS;
        xpt2046_state.touched = true;
        xpt2046_state.pressTime = now;
        xpt2046_state.touchCount++;
        
        // Calibrer le point
        xpt2046_calibrate_point(&raw, &xpt2046_state.pixelPoint);
        xpt2046_state.lastPoint = xpt2046_state.pixelPoint;
        
        // Ajouter à l'historique des gestes
        gesture_history[gesture_history_index % GESTURE_HISTORY_SIZE] = xpt2046_state.pixelPoint;
        gesture_history_index++;
        
        // Appeler le callback
        if (touch_callback)
        {
            touch_callback(XPT2046_EVENT_PRESS, 
                          xpt2046_state.pixelPoint.x, 
                          xpt2046_state.pixelPoint.y);
        }
    }
    else if (currentlyTouched && xpt2046_state.touched)
    {
        // Toucher maintenu (peut-être déplacé)
        xpt2046_state.state = XPT2046_STATE_HELD;
        xpt2046_state.holdDuration = now - xpt2046_state.pressTime;
        
        // Calibrer le nouveau point
        XPT2046_PixelPoint newPoint;
        xpt2046_calibrate_point(&raw, &newPoint);
        
        // Vérifier si la position a changé significativement
        int16_t dx = abs((int16_t)newPoint.x - (int16_t)xpt2046_state.lastPoint.x);
        int16_t dy = abs((int16_t)newPoint.y - (int16_t)xpt2046_state.lastPoint.y);
        
        if (dx > 2 || dy > 2)  // Seuil de mouvement
        {
            xpt2046_state.lastEvent = XPT2046_EVENT_MOVE;
            xpt2046_state.lastPoint = newPoint;
            xpt2046_state.pixelPoint = newPoint;
            
            // Ajouter à l'historique
            gesture_history[gesture_history_index % GESTURE_HISTORY_SIZE] = newPoint;
            gesture_history_index++;
            
            if (touch_callback)
            {
                touch_callback(XPT2046_EVENT_MOVE, newPoint.x, newPoint.y);
            }
        }
        
        // Vérifier l'appui long
        if (xpt2046_state.holdDuration > gesture_config.longPressTimeout)
        {
            if (gesture_config.enableGestures)
            {
                last_gesture = XPT2046_GESTURE_LONG_PRESS;
                if (gesture_callback) gesture_callback(last_gesture);
            }
        }
    }
    else if (!currentlyTouched && xpt2046_state.touched)
    {
        // Relâchement
        xpt2046_state.state = XPT2046_STATE_RELEASED;
        xpt2046_state.lastEvent = XPT2046_EVENT_RELEASE;
        xpt2046_state.touched = false;
        xpt2046_state.releaseTime = now;
        
        uint32_t tapDuration = xpt2046_state.releaseTime - xpt2046_state.pressTime;
        
        // Détecter les gestes
        if (gesture_config.enableGestures)
        {
            if (tapDuration < gesture_config.tapTimeout)
            {
                last_gesture = XPT2046_GESTURE_TAP;
            }
            else
            {
                // Analyser le mouvement pour les swipes
                detect_swipe_gesture();
            }
            
            if (gesture_callback && last_gesture != XPT2046_GESTURE_NONE)
            {
                gesture_callback(last_gesture);
            }
        }
        
        if (touch_callback)
        {
            touch_callback(XPT2046_EVENT_RELEASE,
                          xpt2046_state.pixelPoint.x,
                          xpt2046_state.pixelPoint.y);
        }
        
        xpt2046_state.holdDuration = 0;
    }
    else
    {
        // Pas de toucher
        xpt2046_state.state = XPT2046_STATE_IDLE;
        xpt2046_state.lastEvent = XPT2046_EVENT_NONE;
    }
    
    // Stocker le point brut
    xpt2046_state.rawPoint = raw;
}

/**
 * @brief Détecte un geste de glissement (swipe)
 */
static void detect_swipe_gesture(void)
{
    if (gesture_history_index < 3) return;
    
    // Récupérer le premier et le dernier point
    uint8_t firstIndex = (gesture_history_index - gesture_history_index % GESTURE_HISTORY_SIZE) % GESTURE_HISTORY_SIZE;
    uint8_t lastIndex = (gesture_history_index - 1) % GESTURE_HISTORY_SIZE;
    
    if (firstIndex == lastIndex) return;
    
    XPT2046_PixelPoint first = gesture_history[firstIndex];
    XPT2046_PixelPoint last = gesture_history[lastIndex];
    
    int16_t dx = (int16_t)last.x - (int16_t)first.x;
    int16_t dy = (int16_t)last.y - (int16_t)first.y;
    
    // Vérifier si le déplacement dépasse le seuil
    if (abs(dx) > gesture_config.swipeThreshold || abs(dy) > gesture_config.swipeThreshold)
    {
        if (abs(dx) > abs(dy))
        {
            // Swipe horizontal
            last_gesture = (dx > 0) ? XPT2046_GESTURE_SWIPE_RIGHT : XPT2046_GESTURE_SWIPE_LEFT;
        }
        else
        {
            // Swipe vertical
            last_gesture = (dy > 0) ? XPT2046_GESTURE_SWIPE_DOWN : XPT2046_GESTURE_SWIPE_UP;
        }
    }
    else
    {
        last_gesture = XPT2046_GESTURE_TAP;
    }
}

/**
 * @brief Vérifie si l'écran est touché
 */
bool xpt2046_is_touched(void)
{
    return xpt2046_state.touched;
}

/**
 * @brief Récupère la position actuelle
 */
bool xpt2046_get_position(uint16_t* x, uint16_t* y)
{
    if (!xpt2046_state.touched) return false;
    
    if (x) *x = xpt2046_state.pixelPoint.x;
    if (y) *y = xpt2046_state.pixelPoint.y;
    
    return true;
}

/**
 * @brief Récupère le dernier événement
 */
XPT2046_TouchEvent xpt2046_get_event(void)
{
    return xpt2046_state.lastEvent;
}

/**
 * @brief Récupère l'état
 */
XPT2046_TouchState xpt2046_get_state(void)
{
    return xpt2046_state.state;
}

/**
 * @brief Attend un événement
 */
bool xpt2046_wait_for_event(uint32_t timeoutMs)
{
    uint32_t start = HAL_GetTick();
    
    while ((HAL_GetTick() - start) < timeoutMs)
    {
        xpt2046_scan();
        
        if (xpt2046_state.lastEvent != XPT2046_EVENT_NONE)
        {
            return true;
        }
        
        HAL_Delay(10);
    }
    
    return false;
}

// ============================================================
// SECTION 5 : FILTRAGE
// ============================================================

/**
 * @brief Active/désactive le filtre médian
 */
void xpt2046_filter_median_enable(bool enable)
{
    xpt2046_config.enableMedianFilter = enable;
}

/**
 * @brief Active/désactive le filtre de moyenne
 */
void xpt2046_filter_average_enable(bool enable)
{
    xpt2046_config.enableAverageFilter = enable;
}

/**
 * @brief Définit le nombre d'échantillons
 */
void xpt2046_set_samples(uint8_t count)
{
    if (count < XPT2046_SAMPLES_MIN) count = XPT2046_SAMPLES_MIN;
    if (count > XPT2046_SAMPLES_MAX) count = XPT2046_SAMPLES_MAX;
    xpt2046_config.samples = count;
}

/**
 * @brief Comparateur pour qsort (ordre croissant)
 */
static int compare_uint16(const void* a, const void* b)
{
    return (*(uint16_t*)a - *(uint16_t*)b);
}

/**
 * @brief Applique un filtre médian
 */
uint16_t xpt2046_median_filter(uint16_t* data, uint8_t count)
{
    if (count == 0) return 0;
    if (count == 1) return data[0];
    
    // Trier le tableau
    uint16_t sorted[XPT2046_SAMPLES_MAX];
    memcpy(sorted, data, count * sizeof(uint16_t));
    qsort(sorted, count, sizeof(uint16_t), compare_uint16);
    
    // Retourner la valeur médiane
    if (count % 2 == 1)
    {
        return sorted[count / 2];
    }
    else
    {
        return (sorted[count / 2 - 1] + sorted[count / 2]) / 2;
    }
}

/**
 * @brief Applique un filtre de moyenne
 */
uint16_t xpt2046_average_filter(uint16_t* data, uint8_t count)
{
    if (count == 0) return 0;
    
    uint32_t sum = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        sum += data[i];
    }
    
    return (uint16_t)(sum / count);
}

// ============================================================
// SECTION 6 : GESTES
// ============================================================

/**
 * @brief Active la détection de gestes
 */
void xpt2046_gesture_enable(const XPT2046_GestureConfig* config)
{
    if (config != NULL)
    {
        memcpy(&gesture_config, config, sizeof(XPT2046_GestureConfig));
    }
    gesture_config.enableGestures = true;
    gesture_history_index = 0;
}

/**
 * @brief Désactive la détection de gestes
 */
void xpt2046_gesture_disable(void)
{
    gesture_config.enableGestures = false;
}

/**
 * @brief Récupère le dernier geste
 */
XPT2046_Gesture xpt2046_get_gesture(void)
{
    XPT2046_Gesture gesture = last_gesture;
    last_gesture = XPT2046_GESTURE_NONE;  // Réinitialiser
    return gesture;
}

/**
 * @brief Vérifie un geste spécifique
 */
bool xpt2046_is_gesture(XPT2046_Gesture gesture)
{
    return (last_gesture == gesture);
}

// ============================================================
// SECTION 7 : CONFIGURATION
// ============================================================

/**
 * @brief Définit l'orientation
 */
void xpt2046_set_orientation(bool swapXY, bool invertX, bool invertY)
{
    xpt2046_config.swapXY = swapXY;
    xpt2046_config.invertX = invertX;
    xpt2046_config.invertY = invertY;
}

/**
 * @brief Définit le seuil de détection
 */
void xpt2046_set_threshold(uint16_t threshold)
{
    xpt2046_config.touchThreshold = threshold;
}

/**
 * @brief Définit l'intervalle de scan
 */
void xpt2046_set_scan_interval(uint8_t intervalMs)
{
    xpt2046_config.scanIntervalMs = intervalMs;
}

// ============================================================
// SECTION 8 : CALLBACKS
// ============================================================

/**
 * @brief Enregistre un callback tactile
 */
void xpt2046_set_callback(XPT2046_Callback callback)
{
    touch_callback = callback;
}

/**
 * @brief Enregistre un callback de gestes
 */
void xpt2046_set_gesture_callback(void (*callback)(XPT2046_Gesture))
{
    gesture_callback = callback;
}

// ============================================================
// SECTION 9 : DÉBOGAGE
// ============================================================

/**
 * @brief Affiche les informations
 */
void xpt2046_print_info(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║     INFORMATIONS CONTRÔLEUR TACTILE          ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Driver      : %-31s ║\n", XPT2046_DRIVER_VERSION);
    printf("║ Interface   : %-31s ║\n", xpt2046_config.useI2C ? "I2C" : "SPI");
    printf("║ Échantillons: %-31d ║\n", xpt2046_config.samples);
    printf("║ Seuil       : %-31d ║\n", xpt2046_config.touchThreshold);
    printf("║ Calibré     : %-31s ║\n", calibration_valid ? "Oui" : "Non");
    printf("║ Touchers    : %-31lu ║\n", (unsigned long)xpt2046_state.touchCount);
    printf("║ Erreurs     : %-31lu ║\n", (unsigned long)xpt2046_state.errorCount);
    printf("║ Gestes      : %-31s ║\n", gesture_config.enableGestures ? "Activés" : "Désactivés");
    printf("╚══════════════════════════════════════════════╝\n\n");
}

/**
 * @brief Affiche l'état
 */
void xpt2046_print_state(void)
{
    const char* stateStr = "Inconnu";
    switch (xpt2046_state.state)
    {
        case XPT2046_STATE_IDLE:     stateStr = "En attente"; break;
        case XPT2046_STATE_PRESSED:  stateStr = "Appuyé"; break;
        case XPT2046_STATE_HELD:     stateStr = "Maintenu"; break;
        case XPT2046_STATE_RELEASED: stateStr = "Relâché"; break;
        case XPT2046_STATE_ERROR:    stateStr = "Erreur"; break;
    }
    
    printf("[TACTILE] État: %s | Touché: %s | Position: (%d,%d) | Pression: %d\n",
           stateStr,
           xpt2046_state.touched ? "Oui" : "Non",
           xpt2046_state.pixelPoint.x,
           xpt2046_state.pixelPoint.y,
           xpt2046_state.rawPoint.z);
}

/**
 * @brief Affiche la calibration
 */
void xpt2046_print_calibration(void)
{
    XPT2046_Calibration* cal = &xpt2046_config.calibration;
    
    printf("\n═══ DONNÉES DE CALIBRATION ═══\n");
    printf("Calibré : %s\n", cal->calibrated ? "Oui" : "Non");
    printf("Point A : Écran(%d,%d) ADC(%d,%d)\n", cal->screenAx, cal->screenAy, cal->adcAx, cal->adcAy);
    printf("Point B : Écran(%d,%d) ADC(%d,%d)\n", cal->screenBx, cal->screenBy, cal->adcBx, cal->adcBy);
    printf("Point C : Écran(%d,%d) ADC(%d,%d)\n", cal->screenCx, cal->screenCy, cal->adcCx, cal->adcCy);
    printf("X = %.4f*adcX + %.4f*adcY + %.4f\n", cal->alphaX, cal->betaX, cal->deltaX);
    printf("Y = %.4f*adcX + %.4f*adcY + %.4f\n", cal->alphaY, cal->betaY, cal->deltaY);
    printf("══════════════════════════════\n\n");
}

/**
 * @brief Test de fonctionnement
 */
bool xpt2046_self_test(void)
{
    if (!initialized) return false;
    
    XPT2046_DEBUG("Auto-test...\n");
    
    // Tester la communication
    uint16_t x = xpt2046_read_raw_x();
    uint16_t y = xpt2046_read_raw_y();
    
    if (x == 0 || y == 0 || x > 4095 || y > 4095)
    {
        XPT2046_DEBUG("Échec : valeurs hors limites\n");
        return false;
    }
    
    XPT2046_DEBUG("Auto-test OK (X=%d, Y=%d)\n", x, y);
    return true;
}

// ============================================================
// SECTION 10 : GESTION D'ÉNERGIE
// ============================================================

/**
 * @brief Met en veille
 */
void xpt2046_sleep(void)
{
    // Envoyer commande POWER_DOWN
    xpt2046_read_channel(XPT2046_START | XPT2046_CHANNEL_X | 
                         XPT2046_RESOLUTION_12BIT | XPT2046_DIFFERENTIAL | 
                         XPT2046_POWER_DOWN);
}

/**
 * @brief Réveille
 */
void xpt2046_wakeup(void)
{
    // Une simple lecture réveille le contrôleur
    xpt2046_read_raw_x();
}

/**
 * @brief Vérifie si en veille
 */
bool xpt2046_is_sleeping(void)
{
    return false;  // Le XPT2046 se réveille automatiquement
}

// ============================================================
// SECTION 11 : HANDLER INTERRUPTION
// ============================================================

/**
 * @brief Handler d'interruption EXTI (tactile)
 * 
 * Appelé quand l'écran est touché (front descendant sur IRQ).
 */
void EXTI1_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(TOUCH_IRQ_PIN) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(TOUCH_IRQ_PIN);
        
        // Forcer un scan immédiat
        // (normalement géré par le scan périodique)
    }
    
    HAL_GPIO_EXTI_IRQHandler(TOUCH_IRQ_PIN);
}