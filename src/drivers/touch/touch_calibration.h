/**
 * @file touch_calibration.h
 * @brief Système de calibration de l'écran tactile
 * 
 * Ce fichier gère la calibration de l'écran tactile résistif.
 * La calibration permet de faire correspondre les coordonnées
 * ADC brutes avec les coordonnées pixels de l'écran.
 * 
 * Méthodes de calibration supportées :
 * - 3 points (recommandée) : précise, rapide
 * - 5 points : très précise, pour les grands écrans
 * - 9 points : calibration professionnelle
 * 
 * Les données de calibration sont sauvegardées en Flash
 * et restaurées automatiquement au démarrage.
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef TOUCH_CALIBRATION_H
#define TOUCH_CALIBRATION_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "xpt2046_defs.h"
#include "../../config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module de calibration */
#define CALIBRATION_VERSION             1

/** @brief Nombre de points pour la calibration 3 points */
#define CALIBRATION_POINTS_3            3

/** @brief Nombre de points pour la calibration 5 points */
#define CALIBRATION_POINTS_5            5

/** @brief Nombre de points pour la calibration 9 points */
#define CALIBRATION_POINTS_9            9

/** @brief Nombre maximal de points de calibration */
#define CALIBRATION_MAX_POINTS          9

/** @brief Taille de la marge (pixels) pour les points de calibration */
#define CALIBRATION_MARGIN              30

/** @brief Rayon du point de calibration (pixels) */
#define CALIBRATION_POINT_RADIUS        8

/** @brief Nombre d'échantillons par point */
#define CALIBRATION_SAMPLES_PER_POINT   20

/** @brief Erreur maximale acceptable (pixels) */
#define CALIBRATION_MAX_ERROR           5

/** @brief Taille de la signature magique */
#define CALIBRATION_MAGIC_SIZE          4

/** @brief Signature magique pour validation */
#define CALIBRATION_MAGIC               {0xCA, 0xL1, 0xBR, 0x01}

// ============================================================
// SECTION 2 : MÉTHODES DE CALIBRATION
// ============================================================

/**
 * @brief Méthodes de calibration disponibles
 */
typedef enum {
    CALIB_METHOD_3_POINTS  = 3,     // 3 points (recommandée)
    CALIB_METHOD_5_POINTS  = 5,     // 5 points (plus précise)
    CALIB_METHOD_9_POINTS  = 9      // 9 points (professionnelle)
} CalibrationMethod;

/**
 * @brief État de la procédure de calibration
 */
typedef enum {
    CALIB_STATE_IDLE        = 0,    // En attente
    CALIB_STATE_WAITING     = 1,    // En attente du toucher
    CALIB_STATE_SAMPLING    = 2,    // Échantillonnage en cours
    CALIB_STATE_CALCULATING = 3,    // Calcul en cours
    CALIB_STATE_COMPLETE    = 4,    // Terminé avec succès
    CALIB_STATE_FAILED      = 5     // Échec
} CalibrationState;

// ============================================================
// SECTION 3 : POINTS DE CALIBRATION
// ============================================================

/**
 * @brief Point de calibration
 */
typedef struct {
    uint16_t screenX;               // Position X écran (pixels)
    uint16_t screenY;               // Position Y écran (pixels)
    uint16_t adcX;                  // Valeur ADC X mesurée
    uint16_t adcY;                  // Valeur ADC Y mesurée
    uint16_t adcZ;                  // Pression mesurée
    bool valid;                     // Point valide
    float errorX;                   // Erreur résiduelle X
    float errorY;                   // Erreur résiduelle Y
} CalibrationPoint;

/**
 * @brief Positions prédéfinies pour la calibration 3 points
 */
#define CALIB_3P_A_X    CALIBRATION_MARGIN
#define CALIB_3P_A_Y    CALIBRATION_MARGIN
#define CALIB_3P_B_X    (DISPLAY_WIDTH - CALIBRATION_MARGIN)
#define CALIB_3P_B_Y    CALIBRATION_MARGIN
#define CALIB_3P_C_X    CALIBRATION_MARGIN
#define CALIB_3P_C_Y    (DISPLAY_HEIGHT - CALIBRATION_MARGIN)

/**
 * @brief Positions prédéfinies pour la calibration 5 points
 * 
 * Points : 4 coins + centre
 */
static const uint16_t CALIB_5P_POSITIONS[5][2] = {
    {CALIBRATION_MARGIN, CALIBRATION_MARGIN},                           // Coin HG
    {DISPLAY_WIDTH - CALIBRATION_MARGIN, CALIBRATION_MARGIN},           // Coin HD
    {DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2},                            // Centre
    {CALIBRATION_MARGIN, DISPLAY_HEIGHT - CALIBRATION_MARGIN},          // Coin BG
    {DISPLAY_WIDTH - CALIBRATION_MARGIN, DISPLAY_HEIGHT - CALIBRATION_MARGIN}  // Coin BD
};

// ============================================================
// SECTION 4 : MATRICE DE CALIBRATION
// ============================================================

/**
 * @brief Coefficients de la matrice de calibration
 * 
 * Transformation affine complète :
 * X_ecran = a * X_adc + b * Y_adc + c
 * Y_ecran = d * X_adc + e * Y_adc + f
 * 
 * Version étendue (avec termes quadratiques) :
 * X_ecran = a*X² + b*Y² + c*X*Y + d*X + e*Y + f
 * Y_ecran = g*X² + h*Y² + i*X*Y + j*X + k*Y + l
 */
typedef struct {
    // Coefficients affines (6 paramètres)
    float a, b, c;                  // Pour X : a*X_adc + b*Y_adc + c
    float d, e, f;                  // Pour Y : d*X_adc + e*Y_adc + f
    
    // Coefficients quadratiques (optionnels, 12 paramètres)
    float qa, qb, qc, qd, qe, qf;  // Quadratiques X
    float qg, qh, qi, qj, qk, ql;  // Quadratiques Y
    bool useQuadratic;              // Utiliser la version quadratique
    
    // Métriques de qualité
    float maxError;                 // Erreur maximale (pixels)
    float avgError;                 // Erreur moyenne (pixels)
    float rmsError;                 // Erreur RMS (pixels)
} CalibrationMatrix;

// ============================================================
// SECTION 5 : DONNÉES DE CALIBRATION COMPLÈTES
// ============================================================

/**
 * @brief Structure complète de calibration
 */
typedef struct {
    // Signature
    uint8_t magic[CALIBRATION_MAGIC_SIZE];  // Magique pour validation
    uint8_t version;                        // Version de la structure
    uint16_t checksum;                      // Somme de contrôle
    
    // Paramètres
    CalibrationMethod method;               // Méthode utilisée
    uint8_t pointCount;                     // Nombre de points
    CalibrationPoint points[CALIBRATION_MAX_POINTS];  // Points mesurés
    
    // Matrice calculée
    CalibrationMatrix matrix;               // Coefficients
    
    // Métadonnées
    uint32_t timestamp;                     // Date de calibration
    uint32_t displayWidth;                  // Largeur écran
    uint32_t displayHeight;                 // Hauteur écran
    bool valid;                             // Calibration valide
    bool saved;                             // Sauvegardée en Flash
    
    // Erreurs
    float maxErrorX;                        // Erreur max X
    float maxErrorY;                        // Erreur max Y
    float avgErrorX;                        // Erreur moyenne X
    float avgErrorY;                        // Erreur moyenne Y
    
} CalibrationData;

// ============================================================
// SECTION 6 : INTERFACE DE CALIBRATION
// ============================================================

/**
 * @brief Interface utilisateur pour la calibration
 */
typedef struct {
    // Callback pour dessiner le point de calibration
    void (*drawPoint)(uint16_t x, uint16_t y, uint16_t radius, uint16_t color);
    
    // Callback pour dessiner une instruction
    void (*drawInstruction)(const char* text);
    
    // Callback pour effacer l'écran
    void (*clearScreen)(void);
    
    // Callback pour demander confirmation
    bool (*confirmCalibration)(void);
    
    // Callback pour afficher les résultats
    void (*showResults)(CalibrationData* data);
    
} CalibrationUI;

// ============================================================
// SECTION 7 : FONCTIONS DE CALIBRATION
// ============================================================

/**
 * @brief Initialise le module de calibration
 */
void calibration_init(void);

/**
 * @brief Démarre une procédure de calibration
 * @param method Méthode souhaitée
 * @param ui Interface utilisateur (NULL = pas d'UI)
 * @return true si la procédure démarre
 */
bool calibration_start(CalibrationMethod method, const CalibrationUI* ui);

/**
 * @brief Annule la procédure de calibration en cours
 */
void calibration_cancel(void);

/**
 * @brief Récupère l'état de la calibration
 * @return État actuel
 */
CalibrationState calibration_get_state(void);

/**
 * @brief Récupère le point de calibration en cours
 * @return Index du point (0-based)
 */
uint8_t calibration_get_current_point(void);

/**
 * @brief Passe au point de calibration suivant
 * 
 * Appelée après avoir échantillonné le point actuel.
 * @return true s'il reste des points
 */
bool calibration_next_point(void);

/**
 * @brief Récupère la position du point actuel
 * @param x Position X (sortie)
 * @param y Position Y (sortie)
 */
void calibration_get_point_position(uint16_t* x, uint16_t* y);

/**
 * @brief Ajoute un échantillon pour le point actuel
 * @param adcX Valeur ADC X
 * @param adcY Valeur ADC Y
 * @return true si l'échantillon est valide
 */
bool calibration_add_sample(uint16_t adcX, uint16_t adcY);

/**
 * @brief Finalise la calibration (calcule la matrice)
 * @return true si le calcul réussit
 */
bool calibration_finalize(void);

// ============================================================
// SECTION 8 : FONCTIONS DE TRANSFORMATION
// ============================================================

/**
 * @brief Transforme des coordonnées ADC en pixels
 * @param adcX Valeur ADC X
 * @param adcY Valeur ADC Y
 * @param pixelX Pixel X (sortie)
 * @param pixelY Pixel Y (sortie)
 * @return true si la transformation est valide
 */
bool calibration_transform(uint16_t adcX, uint16_t adcY, 
                           uint16_t* pixelX, uint16_t* pixelY);

/**
 * @brief Transforme un point brut en point écran
 * @param raw Point brut
 * @param pixel Point calibré (sortie)
 */
void calibration_transform_point(const XPT2046_RawPoint* raw, 
                                  XPT2046_PixelPoint* pixel);

/**
 * @brief Vérifie si des coordonnées sont valides après calibration
 * @param pixelX Pixel X
 * @param pixelY Pixel Y
 * @return true si dans les limites de l'écran
 */
bool calibration_is_valid_pixel(uint16_t pixelX, uint16_t pixelY);

// ============================================================
// SECTION 9 : FONCTIONS DE PERSISTANCE
// ============================================================

/**
 * @brief Sauvegarde la calibration en Flash
 * @return true si sauvegardé
 */
bool calibration_save(void);

/**
 * @brief Charge la calibration depuis la Flash
 * @return true si chargé avec succès
 */
bool calibration_load(void);

/**
 * @brief Vérifie si une calibration est sauvegardée
 * @return true si une calibration existe en Flash
 */
bool calibration_is_saved(void);

/**
 * @brief Efface la calibration sauvegardée
 */
void calibration_erase(void);

/**
 * @brief Exporte la calibration sous forme de chaîne
 * @param buffer Buffer de sortie
 * @param size Taille du buffer
 * @return Nombre de caractères écrits
 */
int calibration_export(char* buffer, uint16_t size);

/**
 * @brief Importe une calibration depuis une chaîne
 * @param data Chaîne de données
 * @return true si importé avec succès
 */
bool calibration_import(const char* data);

// ============================================================
// SECTION 10 : FONCTIONS DE QUALITÉ
// ============================================================

/**
 * @brief Vérifie la qualité de la calibration
 * @return true si la qualité est acceptable
 */
bool calibration_check_quality(void);

/**
 * @brief Calcule l'erreur pour un point donné
 * @param screenX Position X écran attendue
 * @param screenY Position Y écran attendue
 * @param adcX Valeur ADC X mesurée
 * @param adcY Valeur ADC Y mesurée
 * @param errorX Erreur X (sortie)
 * @param errorY Erreur Y (sortie)
 */
void calibration_calculate_error(uint16_t screenX, uint16_t screenY,
                                  uint16_t adcX, uint16_t adcY,
                                  float* errorX, float* errorY);

/**
 * @brief Récupère l'erreur maximale
 * @return Erreur maximale en pixels
 */
float calibration_get_max_error(void);

/**
 * @brief Récupère l'erreur moyenne
 * @return Erreur moyenne en pixels
 */
float calibration_get_avg_error(void);

/**
 * @brief Vérifie si la calibration doit être refaite
 * @return true si une recalibration est recommandée
 */
bool calibration_needs_recalibration(void);

// ============================================================
// SECTION 11 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche les données de calibration
 */
void calibration_print_data(void);

/**
 * @brief Affiche la matrice de calibration
 */
void calibration_print_matrix(void);

/**
 * @brief Affiche les erreurs par point
 */
void calibration_print_errors(void);

/**
 * @brief Test de validation de la calibration
 * @return true si le test réussit
 */
bool calibration_validate(void);

/**
 * @brief Réinitialise les données de calibration
 */
void calibration_reset(void);

// ============================================================
// SECTION 12 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define CALIB_DEBUG(fmt, ...)       printf("[CALIB] " fmt, ##__VA_ARGS__)
#else
    #define CALIB_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 13 : FONCTIONS UTILITAIRES
// ============================================================

/**
 * @brief Calcule le déterminant d'une matrice 3x3
 * @param m Matrice 3x3 (tableau de 9 floats)
 * @return Déterminant
 */
float calibration_det3x3(const float m[9]);

/**
 * @brief Résout un système linéaire 3x3
 * @param A Matrice des coefficients
 * @param b Vecteur second membre
 * @param x Vecteur solution (sortie)
 * @return true si résolu
 */
bool calibration_solve3x3(const float A[9], const float b[3], float x[3]);

/**
 * @brief Calcule la moyenne d'un tableau
 * @param data Tableau de valeurs
 * @param count Nombre d'éléments
 * @return Moyenne
 */
float calibration_average(const uint16_t* data, uint8_t count);

/**
 * @brief Calcule l'écart-type d'un tableau
 * @param data Tableau de valeurs
 * @param count Nombre d'éléments
 * @param mean Moyenne (précalculée)
 * @return Écart-type
 */
float calibration_stddev(const uint16_t* data, uint8_t count, float mean);

// ============================================================
// SECTION 14 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // TOUCH_CALIBRATION_H