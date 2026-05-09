/**
 * @file xpt2046_driver.h
 * @brief Driver pour le contrôleur tactile XPT2046
 * 
 * Ce fichier déclare l'interface du driver pour le contrôleur
 * tactile résistif XPT2046.
 * 
 * Fonctionnalités :
 * - Communication SPI/I2C
 * - Lecture des coordonnées brutes (ADC 12 bits)
 * - Calibration 3 points
 * - Conversion coordonnées ADC → pixels
 * - Filtrage (médian, moyenne)
 * - Détection des événements (press, move, release)
 * - Reconnaissance de gestes simples
 * - Gestion des interruptions (IRQ)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef XPT2046_DRIVER_H
#define XPT2046_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "xpt2046_defs.h"
#include "../../config.h"
#include "../../../include/project_config.h"

// ============================================================
// SECTION 1 : HANDLES EXTERNES
// ============================================================

/** @brief Handle SPI utilisé pour la communication (mode SPI) */
extern SPI_HandleTypeDef hspi2;

/** @brief Handle I2C utilisé pour la communication (mode I2C) */
extern I2C_HandleTypeDef hi2c1;

// ============================================================
// SECTION 2 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise le contrôleur tactile XPT2046
 * 
 * Configure :
 * - Les broches GPIO (CS, IRQ)
 * - L'interface de communication (SPI ou I2C)
 * - Les paramètres par défaut
 * 
 * @param config Configuration souhaitée (NULL = défaut)
 * @return XPT2046_OK si succès
 */
XPT2046_Error xpt2046_init(const XPT2046_Config* config);

/**
 * @brief Désinitialise le contrôleur tactile
 */
void xpt2046_deinit(void);

/**
 * @brief Vérifie si le contrôleur est initialisé
 * @return true si prêt
 */
bool xpt2046_is_ready(void);

/**
 * @brief Vérifie si le contrôleur répond
 * @return true si présent
 */
bool xpt2046_is_present(void);

// ============================================================
// SECTION 3 : FONCTIONS DE LECTURE BRUTE
// ============================================================

/**
 * @brief Lit un canal ADC du XPT2046
 * @param command Commande de conversion
 * @return Valeur ADC 12 bits (0-4095)
 */
uint16_t xpt2046_read_channel(uint8_t command);

/**
 * @brief Lit la position X brute
 * @return Valeur ADC 12 bits
 */
uint16_t xpt2046_read_raw_x(void);

/**
 * @brief Lit la position Y brute
 * @return Valeur ADC 12 bits
 */
uint16_t xpt2046_read_raw_y(void);

/**
 * @brief Lit la pression (Z)
 * @return Valeur ADC 12 bits (0 = pas de toucher)
 */
uint16_t xpt2046_read_raw_z(void);

/**
 * @brief Lit un point complet (X, Y, Z) avec moyenne
 * @param point Structure à remplir
 * @return XPT2046_OK si succès
 */
XPT2046_Error xpt2046_read_raw_point(XPT2046_RawPoint* point);

/**
 * @brief Lit plusieurs échantillons et applique les filtres
 * @param point Point filtré (sortie)
 * @return XPT2046_OK si succès
 */
XPT2046_Error xpt2046_read_filtered_point(XPT2046_RawPoint* point);

// ============================================================
// SECTION 4 : FONCTIONS DE CALIBRATION
// ============================================================

/**
 * @brief Définit un point de calibration
 * 
 * Appelée 3 fois pour les 3 points (A, B, C).
 * L'utilisateur doit toucher l'écran aux positions demandées.
 * 
 * @param pointIndex Index du point (0=A, 1=B, 2=C)
 * @param screenX Position X écran
 * @param screenY Position Y écran
 * @return true si le point a été lu correctement
 */
bool xpt2046_calibration_set_point(uint8_t pointIndex, uint16_t screenX, uint16_t screenY);

/**
 * @brief Calcule les coefficients de calibration
 * 
 * À appeler après avoir défini les 3 points.
 * 
 * @return true si le calcul a réussi
 */
bool xpt2046_calibration_calculate(void);

/**
 * @brief Convertit un point brut en point écran (pixels)
 * @param raw Point brut
 * @param pixel Point calibré (sortie)
 */
void xpt2046_calibrate_point(const XPT2046_RawPoint* raw, XPT2046_PixelPoint* pixel);

/**
 * @brief Vérifie si la calibration est valide
 * @return true si calibré
 */
bool xpt2046_is_calibrated(void);

/**
 * @brief Sauvegarde la calibration en Flash
 * @return true si sauvegardé
 */
bool xpt2046_calibration_save(void);

/**
 * @brief Charge la calibration depuis la Flash
 * @return true si chargé
 */
bool xpt2046_calibration_load(void);

/**
 * @brief Réinitialise la calibration (valeurs par défaut)
 */
void xpt2046_calibration_reset(void);

// ============================================================
// SECTION 5 : FONCTIONS DE SCAN ET ÉVÉNEMENTS
// ============================================================

/**
 * @brief Scanne l'écran tactile (à appeler périodiquement)
 * 
 * Vérifie si l'écran est touché et met à jour l'état.
 * Doit être appelée toutes les ~20 ms.
 */
void xpt2046_scan(void);

/**
 * @brief Vérifie si l'écran est actuellement touché
 * @return true si touché
 */
bool xpt2046_is_touched(void);

/**
 * @brief Récupère la position actuelle (en pixels)
 * @param x Position X (sortie)
 * @param y Position Y (sortie)
 * @return true si la position est valide
 */
bool xpt2046_get_position(uint16_t* x, uint16_t* y);

/**
 * @brief Récupère le dernier événement tactile
 * @return Type d'événement
 */
XPT2046_TouchEvent xpt2046_get_event(void);

/**
 * @brief Récupère l'état actuel du tactile
 * @return État
 */
XPT2046_TouchState xpt2046_get_state(void);

/**
 * @brief Attend un événement tactile
 * @param timeoutMs Timeout en ms
 * @return true si un événement s'est produit
 */
bool xpt2046_wait_for_event(uint32_t timeoutMs);

// ============================================================
// SECTION 6 : FONCTIONS DE FILTRAGE
// ============================================================

/**
 * @brief Active/désactive le filtre médian
 * @param enable true pour activer
 */
void xpt2046_filter_median_enable(bool enable);

/**
 * @brief Active/désactive le filtre de moyenne
 * @param enable true pour activer
 */
void xpt2046_filter_average_enable(bool enable);

/**
 * @brief Définit le nombre d'échantillons pour les filtres
 * @param count Nombre d'échantillons (1-20)
 */
void xpt2046_set_samples(uint8_t count);

/**
 * @brief Applique un filtre médian sur un tableau
 * @param data Tableau de valeurs
 * @param count Nombre d'éléments
 * @return Valeur médiane
 */
uint16_t xpt2046_median_filter(uint16_t* data, uint8_t count);

/**
 * @brief Applique un filtre de moyenne
 * @param data Tableau de valeurs
 * @param count Nombre d'éléments
 * @return Valeur moyenne
 */
uint16_t xpt2046_average_filter(uint16_t* data, uint8_t count);

// ============================================================
// SECTION 7 : FONCTIONS DE GESTES
// ============================================================

/**
 * @brief Active la détection de gestes
 * @param config Configuration des gestes (NULL = défaut)
 */
void xpt2046_gesture_enable(const XPT2046_GestureConfig* config);

/**
 * @brief Désactive la détection de gestes
 */
void xpt2046_gesture_disable(void);

/**
 * @brief Récupère le dernier geste détecté
 * @return Type de geste
 */
XPT2046_Gesture xpt2046_get_gesture(void);

/**
 * @brief Vérifie si un geste spécifique a été détecté
 * @param gesture Type de geste
 * @return true si détecté
 */
bool xpt2046_is_gesture(XPT2046_Gesture gesture);

// ============================================================
// SECTION 8 : FONCTIONS DE CONFIGURATION
// ============================================================

/**
 * @brief Définit l'orientation du tactile
 * @param swapXY Échanger X et Y
 * @param invertX Inverser X
 * @param invertY Inverser Y
 */
void xpt2046_set_orientation(bool swapXY, bool invertX, bool invertY);

/**
 * @brief Définit le seuil de détection de toucher
 * @param threshold Seuil (0-4095)
 */
void xpt2046_set_threshold(uint16_t threshold);

/**
 * @brief Définit l'intervalle de scan
 * @param intervalMs Intervalle en ms
 */
void xpt2046_set_scan_interval(uint8_t intervalMs);

// ============================================================
// SECTION 9 : FONCTIONS DE CALLBACKS
// ============================================================

/**
 * @brief Type de fonction callback pour les événements tactiles
 * @param event Événement
 * @param x Position X (pixels)
 * @param y Position Y (pixels)
 */
typedef void (*XPT2046_Callback)(XPT2046_TouchEvent event, uint16_t x, uint16_t y);

/**
 * @brief Enregistre un callback pour les événements tactiles
 * @param callback Fonction à appeler
 */
void xpt2046_set_callback(XPT2046_Callback callback);

/**
 * @brief Enregistre un callback spécifique pour les gestes
 * @param callback Fonction à appeler
 */
void xpt2046_set_gesture_callback(void (*callback)(XPT2046_Gesture gesture));

// ============================================================
// SECTION 10 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche les informations du contrôleur
 */
void xpt2046_print_info(void);

/**
 * @brief Affiche l'état actuel
 */
void xpt2046_print_state(void);

/**
 * @brief Affiche les données de calibration
 */
void xpt2046_print_calibration(void);

/**
 * @brief Test de fonctionnement du tactile
 * @return true si le test réussit
 */
bool xpt2046_self_test(void);

// ============================================================
// SECTION 11 : FONCTIONS DE GESTION D'ÉNERGIE
// ============================================================

/**
 * @brief Met le contrôleur en veille
 */
void xpt2046_sleep(void);

/**
 * @brief Réveille le contrôleur
 */
void xpt2046_wakeup(void);

/**
 * @brief Vérifie si le contrôleur est en veille
 * @return true si en veille
 */
bool xpt2046_is_sleeping(void);

// ============================================================
// SECTION 12 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define XPT2046_DEBUG(fmt, ...)     printf("[XPT2046] " fmt, ##__VA_ARGS__)
#else
    #define XPT2046_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 13 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // XPT2046_DRIVER_H