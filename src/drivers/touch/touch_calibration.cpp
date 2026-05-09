/**
 * @file touch_calibration.cpp
 * @brief Implémentation du système de calibration tactile
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans touch_calibration.h.
 * 
 * Il gère :
 * - La procédure de calibration interactive
 * - Le calcul de la matrice de transformation
 * - La sauvegarde/restauration en Flash
 * - La validation de la qualité
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "touch_calibration.h"
#include "xpt2046_driver.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Données de calibration actuelles */
static CalibrationData calib_data;

/** @brief État de la procédure */
static CalibrationState calib_state = CALIB_STATE_IDLE;

/** @brief Interface utilisateur */
static CalibrationUI calib_ui = {0};

/** @brief Méthode de calibration active */
static CalibrationMethod calib_method = CALIB_METHOD_3_POINTS;

/** @brief Index du point en cours */
static uint8_t current_point_index = 0;

/** @brief Compteur d'échantillons pour le point actuel */
static uint8_t current_sample_count = 0;

/** @brief Échantillons temporaires */
static uint16_t temp_samples_x[CALIBRATION_SAMPLES_PER_POINT];
static uint16_t temp_samples_y[CALIBRATION_SAMPLES_PER_POINT];

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le module de calibration
 */
void calibration_init(void)
{
    CALIB_DEBUG("Initialisation du module de calibration\n");
    
    // Réinitialiser les données
    memset(&calib_data, 0, sizeof(CalibrationData));
    
    // Définir la signature magique
    uint8_t magic[] = CALIBRATION_MAGIC;
    memcpy(calib_data.magic, magic, CALIBRATION_MAGIC_SIZE);
    
    calib_data.version = CALIBRATION_VERSION;
    calib_data.displayWidth = DISPLAY_WIDTH;
    calib_data.displayHeight = DISPLAY_HEIGHT;
    calib_data.valid = false;
    calib_data.saved = false;
    
    calib_state = CALIB_STATE_IDLE;
    
    // Tenter de charger une calibration existante
    if (calibration_load())
    {
        CALIB_DEBUG("Calibration chargée depuis la Flash\n");
    }
    else
    {
        CALIB_DEBUG("Aucune calibration trouvée\n");
    }
}

// ============================================================
// SECTION 2 : PROCÉDURE DE CALIBRATION
// ============================================================

/**
 * @brief Démarre une procédure de calibration
 */
bool calibration_start(CalibrationMethod method, const CalibrationUI* ui)
{
    if (calib_state != CALIB_STATE_IDLE && calib_state != CALIB_STATE_COMPLETE)
    {
        CALIB_DEBUG("Calibration déjà en cours\n");
        return false;
    }
    
    CALIB_DEBUG("Démarrage calibration %d points\n", method);
    
    // Sauvegarder l'interface utilisateur
    if (ui != NULL)
    {
        memcpy(&calib_ui, ui, sizeof(CalibrationUI));
    }
    else
    {
        memset(&calib_ui, 0, sizeof(CalibrationUI));
    }
    
    // Initialiser les paramètres
    calib_method = method;
    calib_data.method = method;
    calib_data.pointCount = method;
    current_point_index = 0;
    calib_state = CALIB_STATE_WAITING;
    
    // Effacer l'écran
    if (calib_ui.clearScreen)
    {
        calib_ui.clearScreen();
    }
    
    // Afficher les instructions
    if (calib_ui.drawInstruction)
    {
        calib_ui.drawInstruction("Calibration - Touchez le point");
    }
    
    // Afficher le premier point
    uint16_t px, py;
    calibration_get_point_position(&px, &py);
    
    if (calib_ui.drawPoint)
    {
        calib_ui.drawPoint(px, py, CALIBRATION_POINT_RADIUS, ILI9488_RED);
    }
    
    return true;
}

/**
 * @brief Annule la procédure
 */
void calibration_cancel(void)
{
    CALIB_DEBUG("Calibration annulée\n");
    calib_state = CALIB_STATE_IDLE;
    current_point_index = 0;
    current_sample_count = 0;
}

/**
 * @brief Récupère l'état
 */
CalibrationState calibration_get_state(void)
{
    return calib_state;
}

/**
 * @brief Récupère l'index du point actuel
 */
uint8_t calibration_get_current_point(void)
{
    return current_point_index;
}

/**
 * @brief Récupère la position du point actuel
 */
void calibration_get_point_position(uint16_t* x, uint16_t* y)
{
    if (x == NULL || y == NULL) return;
    
    if (calib_method == CALIB_METHOD_3_POINTS)
    {
        switch (current_point_index)
        {
            case 0: *x = CALIB_3P_A_X; *y = CALIB_3P_A_Y; break;
            case 1: *x = CALIB_3P_B_X; *y = CALIB_3P_B_Y; break;
            case 2: *x = CALIB_3P_C_X; *y = CALIB_3P_C_Y; break;
            default: *x = 0; *y = 0; break;
        }
    }
    else if (calib_method == CALIB_METHOD_5_POINTS)
    {
        if (current_point_index < 5)
        {
            *x = CALIB_5P_POSITIONS[current_point_index][0];
            *y = CALIB_5P_POSITIONS[current_point_index][1];
        }
    }
    else
    {
        *x = 0;
        *y = 0;
    }
}

/**
 * @brief Passe au point suivant
 */
bool calibration_next_point(void)
{
    current_point_index++;
    
    if (current_point_index >= calib_data.pointCount)
    {
        // Tous les points sont faits
        CALIB_DEBUG("Tous les points échantillonnés\n");
        calib_state = CALIB_STATE_CALCULATING;
        return false;  // Plus de points
    }
    
    // Afficher le point suivant
    uint16_t px, py;
    calibration_get_point_position(&px, &py);
    
    if (calib_ui.drawPoint)
    {
        calib_ui.drawPoint(px, py, CALIBRATION_POINT_RADIUS, ILI9488_RED);
    }
    
    calib_state = CALIB_STATE_WAITING;
    current_sample_count = 0;
    
    CALIB_DEBUG("Point %d/%d : (%d, %d)\n", 
               current_point_index + 1, calib_data.pointCount, px, py);
    
    return true;
}

/**
 * @brief Ajoute un échantillon pour le point actuel
 */
bool calibration_add_sample(uint16_t adcX, uint16_t adcY)
{
    if (calib_state != CALIB_STATE_WAITING && calib_state != CALIB_STATE_SAMPLING)
    {
        return false;
    }
    
    calib_state = CALIB_STATE_SAMPLING;
    
    // Stocker l'échantillon
    if (current_sample_count < CALIBRATION_SAMPLES_PER_POINT)
    {
        temp_samples_x[current_sample_count] = adcX;
        temp_samples_y[current_sample_count] = adcY;
        current_sample_count++;
        
        if (current_sample_count >= CALIBRATION_SAMPLES_PER_POINT)
        {
            // Assez d'échantillons, calculer la moyenne
            float meanX = calibration_average(temp_samples_x, CALIBRATION_SAMPLES_PER_POINT);
            float meanY = calibration_average(temp_samples_y, CALIBRATION_SAMPLES_PER_POINT);
            
            // Vérifier la stabilité (écart-type)
            float stdX = calibration_stddev(temp_samples_x, CALIBRATION_SAMPLES_PER_POINT, meanX);
            float stdY = calibration_stddev(temp_samples_y, CALIBRATION_SAMPLES_PER_POINT, meanY);
            
            if (stdX > 50 || stdY > 50)  // Trop de variation
            {
                CALIB_DEBUG("Point instable (stdX=%.1f, stdY=%.1f), recommencer\n", stdX, stdY);
                current_sample_count = 0;
                return false;
            }
            
            // Stocker le point
            CalibrationPoint* point = &calib_data.points[current_point_index];
            point->adcX = (uint16_t)roundf(meanX);
            point->adcY = (uint16_t)roundf(meanY);
            point->valid = true;
            
            // Récupérer la position écran
            calibration_get_point_position(&point->screenX, &point->screenY);
            
            CALIB_DEBUG("Point %d validé : ADC(%d,%d) -> Écran(%d,%d)\n",
                       current_point_index,
                       point->adcX, point->adcY,
                       point->screenX, point->screenY);
            
            return true;  // Point validé
        }
    }
    
    return false;  // Pas encore assez d'échantillons
}

/**
 * @brief Finalise la calibration
 */
bool calibration_finalize(void)
{
    CALIB_DEBUG("Finalisation de la calibration...\n");
    
    // Vérifier que tous les points sont valides
    for (uint8_t i = 0; i < calib_data.pointCount; i++)
    {
        if (!calib_data.points[i].valid)
        {
            CALIB_DEBUG("Point %d invalide\n", i);
            calib_state = CALIB_STATE_FAILED;
            return false;
        }
    }
    
    // Construire les matrices pour la résolution
    float A[9];  // 3x3
    float bx[3]; // Second membre X
    float by[3]; // Second membre Y
    
    // Remplir la matrice A (3x3)
    // ┌                ┐
    // │ adcX1 adcY1 1  │
    // │ adcX2 adcY2 1  │
    // │ adcX3 adcY3 1  │
    // └                ┘
    
    if (calib_data.pointCount >= 3)
    {
        // Utiliser les 3 meilleurs points (les plus espacés)
        uint8_t p0 = 0;  // Premier point
        uint8_t p1 = 1;  // Deuxième point
        uint8_t p2 = 2;  // Troisième point
        
        // Pour plus de 3 points, choisir les plus éloignés
        if (calib_data.pointCount > 3)
        {
            float maxDist = 0;
            for (uint8_t i = 0; i < calib_data.pointCount; i++)
            {
                for (uint8_t j = i + 1; j < calib_data.pointCount; j++)
                {
                    float dx = (float)calib_data.points[i].adcX - (float)calib_data.points[j].adcX;
                    float dy = (float)calib_data.points[i].adcY - (float)calib_data.points[j].adcY;
                    float dist = sqrtf(dx * dx + dy * dy);
                    
                    if (dist > maxDist)
                    {
                        maxDist = dist;
                        p0 = i;
                        p1 = j;
                        // Trouver un 3ème point éloigné des deux premiers
                        for (uint8_t k = 0; k < calib_data.pointCount; k++)
                        {
                            if (k != p0 && k != p1)
                            {
                                p2 = k;
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        // Remplir A
        A[0] = calib_data.points[p0].adcX; A[1] = calib_data.points[p0].adcY; A[2] = 1;
        A[3] = calib_data.points[p1].adcX; A[4] = calib_data.points[p1].adcY; A[5] = 1;
        A[6] = calib_data.points[p2].adcX; A[7] = calib_data.points[p2].adcY; A[8] = 1;
        
        // Remplir Bx et By
        bx[0] = calib_data.points[p0].screenX;
        bx[1] = calib_data.points[p1].screenX;
        bx[2] = calib_data.points[p2].screenX;
        
        by[0] = calib_data.points[p0].screenY;
        by[1] = calib_data.points[p1].screenY;
        by[2] = calib_data.points[p2].screenY;
    }
    else
    {
        CALIB_DEBUG("Pas assez de points\n");
        calib_state = CALIB_STATE_FAILED;
        return false;
    }
    
    // Résoudre pour X : A * (a,b,c) = bx
    float cx[3];  // (a, b, c)
    if (!calibration_solve3x3(A, bx, cx))
    {
        CALIB_DEBUG("Impossible de résoudre pour X\n");
        calib_state = CALIB_STATE_FAILED;
        return false;
    }
    
    // Résoudre pour Y : A * (d,e,f) = by
    float cy[3];  // (d, e, f)
    if (!calibration_solve3x3(A, by, cy))
    {
        CALIB_DEBUG("Impossible de résoudre pour Y\n");
        calib_state = CALIB_STATE_FAILED;
        return false;
    }
    
    // Stocker les coefficients
    calib_data.matrix.a = cx[0];
    calib_data.matrix.b = cx[1];
    calib_data.matrix.c = cx[2];
    
    calib_data.matrix.d = cy[0];
    calib_data.matrix.e = cy[1];
    calib_data.matrix.f = cy[2];
    
    calib_data.matrix.useQuadratic = false;
    
    // Calculer les erreurs
    calib_data.matrix.maxError = 0;
    calib_data.matrix.avgError = 0;
    float totalError = 0;
    
    for (uint8_t i = 0; i < calib_data.pointCount; i++)
    {
        float predX = calib_data.matrix.a * calib_data.points[i].adcX + 
                      calib_data.matrix.b * calib_data.points[i].adcY + 
                      calib_data.matrix.c;
        float predY = calib_data.matrix.d * calib_data.points[i].adcX + 
                      calib_data.matrix.e * calib_data.points[i].adcY + 
                      calib_data.matrix.f;
        
        float errorX = fabsf(predX - calib_data.points[i].screenX);
        float errorY = fabsf(predY - calib_data.points[i].screenY);
        
        calib_data.points[i].errorX = errorX;
        calib_data.points[i].errorY = errorY;
        
        float error = sqrtf(errorX * errorX + errorY * errorY);
        
        if (error > calib_data.matrix.maxError)
            calib_data.matrix.maxError = error;
        
        totalError += error;
    }
    
    calib_data.matrix.avgError = totalError / calib_data.pointCount;
    calib_data.matrix.rmsError = sqrtf(totalError * totalError / calib_data.pointCount);
    
    // Marquer comme valide
    calib_data.valid = true;
    calib_data.timestamp = HAL_GetTick();
    
    // Calculer le checksum
    calib_data.checksum = calibration_calculate_checksum();
    
    calib_state = CALIB_STATE_COMPLETE;
    
    CALIB_DEBUG("Calibration terminée\n");
    CALIB_DEBUG("  Max Error: %.2f px\n", calib_data.matrix.maxError);
    CALIB_DEBUG("  Avg Error: %.2f px\n", calib_data.matrix.avgError);
    
    // Afficher les résultats
    if (calib_ui.showResults)
    {
        calib_ui.showResults(&calib_data);
    }
    
    // Proposer la sauvegarde
    if (calib_ui.confirmCalibration)
    {
        if (calib_ui.confirmCalibration())
        {
            calibration_save();
        }
    }
    else
    {
        calibration_save();  // Sauvegarder automatiquement
    }
    
    return true;
}

// ============================================================
// SECTION 3 : TRANSFORMATION
// ============================================================

/**
 * @brief Transforme des coordonnées ADC en pixels
 */
bool calibration_transform(uint16_t adcX, uint16_t adcY,
                           uint16_t* pixelX, uint16_t* pixelY)
{
    if (!calib_data.valid) return false;
    if (pixelX == NULL || pixelY == NULL) return false;
    
    // Appliquer la transformation affine
    float x = calib_data.matrix.a * adcX + 
              calib_data.matrix.b * adcY + 
              calib_data.matrix.c;
    float y = calib_data.matrix.d * adcX + 
              calib_data.matrix.e * adcY + 
              calib_data.matrix.f;
    
    // Arrondir et limiter aux dimensions de l'écran
    int16_t px = (int16_t)roundf(x);
    int16_t py = (int16_t)roundf(y);
    
    if (px < 0) px = 0;
    if (px >= (int16_t)DISPLAY_WIDTH) px = DISPLAY_WIDTH - 1;
    if (py < 0) py = 0;
    if (py >= (int16_t)DISPLAY_HEIGHT) py = DISPLAY_HEIGHT - 1;
    
    *pixelX = (uint16_t)px;
    *pixelY = (uint16_t)py;
    
    return true;
}

/**
 * @brief Transforme un point brut en point écran
 */
void calibration_transform_point(const XPT2046_RawPoint* raw,
                                  XPT2046_PixelPoint* pixel)
{
    if (raw == NULL || pixel == NULL) return;
    
    pixel->valid = calibration_transform(raw->x, raw->y, &pixel->x, &pixel->y);
}

/**
 * @brief Vérifie si des coordonnées sont valides
 */
bool calibration_is_valid_pixel(uint16_t pixelX, uint16_t pixelY)
{
    return (pixelX < DISPLAY_WIDTH && pixelY < DISPLAY_HEIGHT);
}

// ============================================================
// SECTION 4 : PERSISTANCE
// ============================================================

/**
 * @brief Calcule le checksum des données
 */
static uint16_t calibration_calculate_checksum(void)
{
    uint16_t checksum = 0;
    uint8_t* data = (uint8_t*)&calib_data;
    
    // Exclure le champ checksum lui-même
    for (uint32_t i = CALIBRATION_MAGIC_SIZE + 1; i < sizeof(CalibrationData); i++)
    {
        checksum += data[i];
    }
    
    return checksum;
}

/**
 * @brief Sauvegarde la calibration en Flash
 */
bool calibration_save(void)
{
    if (!calib_data.valid) return false;
    
    // Mettre à jour le checksum
    calib_data.checksum = calibration_calculate_checksum();
    
    // TODO : Implémenter la sauvegarde en Flash
    // Pour l'instant, on simule
    CALIB_DEBUG("Calibration sauvegardée en Flash (simulé)\n");
    calib_data.saved = true;
    
    return true;
}

/**
 * @brief Charge la calibration depuis la Flash
 */
bool calibration_load(void)
{
    // TODO : Implémenter le chargement depuis la Flash
    // Pour l'instant, on simule
    CALIB_DEBUG("Tentative de chargement (simulé)\n");
    
    // Vérifier la magique
    uint8_t expected_magic[] = CALIBRATION_MAGIC;
    if (memcmp(calib_data.magic, expected_magic, CALIBRATION_MAGIC_SIZE) == 0 &&
        calib_data.valid)
    {
        CALIB_DEBUG("Calibration valide trouvée\n");
        return true;
    }
    
    return false;
}

/**
 * @brief Vérifie si une calibration est sauvegardée
 */
bool calibration_is_saved(void)
{
    return calib_data.saved;
}

/**
 * @brief Efface la calibration
 */
void calibration_erase(void)
{
    memset(&calib_data, 0, sizeof(CalibrationData));
    calib_data.valid = false;
    calib_data.saved = false;
    CALIB_DEBUG("Calibration effacée\n");
}

/**
 * @brief Exporte la calibration en chaîne
 */
int calibration_export(char* buffer, uint16_t size)
{
    if (buffer == NULL || size < 100) return 0;
    
    return snprintf(buffer, size,
        "CALIB:%d:%d:%.4f:%.4f:%.4f:%.4f:%.4f:%.4f:%d:%d",
        calib_data.method,
        calib_data.pointCount,
        calib_data.matrix.a, calib_data.matrix.b, calib_data.matrix.c,
        calib_data.matrix.d, calib_data.matrix.e, calib_data.matrix.f,
        calib_data.displayWidth, calib_data.displayHeight);
}

/**
 * @brief Importe une calibration depuis une chaîne
 */
bool calibration_import(const char* data)
{
    if (data == NULL) return false;
    
    int method, pointCount, width, height;
    float a, b, c, d, e, f;
    
    if (sscanf(data, "CALIB:%d:%d:%f:%f:%f:%f:%f:%f:%d:%d",
               &method, &pointCount, &a, &b, &c, &d, &e, &f, &width, &height) == 10)
    {
        calib_data.method = (CalibrationMethod)method;
        calib_data.pointCount = pointCount;
        calib_data.matrix.a = a;
        calib_data.matrix.b = b;
        calib_data.matrix.c = c;
        calib_data.matrix.d = d;
        calib_data.matrix.e = e;
        calib_data.matrix.f = f;
        calib_data.displayWidth = width;
        calib_data.displayHeight = height;
        calib_data.valid = true;
        
        CALIB_DEBUG("Calibration importée\n");
        return true;
    }
    
    return false;
}

// ============================================================
// SECTION 5 : QUALITÉ
// ============================================================

/**
 * @brief Vérifie la qualité de la calibration
 */
bool calibration_check_quality(void)
{
    if (!calib_data.valid) return false;
    
    // Vérifier l'erreur maximale
    if (calib_data.matrix.maxError > CALIBRATION_MAX_ERROR)
    {
        CALIB_DEBUG("Erreur trop grande : %.2f > %d\n", 
                   calib_data.matrix.maxError, CALIBRATION_MAX_ERROR);
        return false;
    }
    
    return true;
}

/**
 * @brief Calcule l'erreur pour un point
 */
void calibration_calculate_error(uint16_t screenX, uint16_t screenY,
                                  uint16_t adcX, uint16_t adcY,
                                  float* errorX, float* errorY)
{
    uint16_t predX, predY;
    calibration_transform(adcX, adcY, &predX, &predY);
    
    if (errorX) *errorX = fabsf((float)predX - (float)screenX);
    if (errorY) *errorY = fabsf((float)predY - (float)screenY);
}

/**
 * @brief Récupère l'erreur maximale
 */
float calibration_get_max_error(void)
{
    return calib_data.valid ? calib_data.matrix.maxError : 999.0f;
}

/**
 * @brief Récupère l'erreur moyenne
 */
float calibration_get_avg_error(void)
{
    return calib_data.valid ? calib_data.matrix.avgError : 999.0f;
}

/**
 * @brief Vérifie si une recalibration est nécessaire
 */
bool calibration_needs_recalibration(void)
{
    if (!calib_data.valid) return true;
    
    // Vérifier l'erreur
    if (calib_data.matrix.maxError > CALIBRATION_MAX_ERROR * 2)
    {
        return true;
    }
    
    // Vérifier l'âge de la calibration (plus de 30 jours)
    uint32_t now = HAL_GetTick() / 1000;
    uint32_t calibAge = now - calib_data.timestamp;
    
    if (calibAge > 30 * 24 * 3600)  // 30 jours
    {
        CALIB_DEBUG("Calibration trop ancienne\n");
        return true;
    }
    
    return false;
}

// ============================================================
// SECTION 6 : FONCTIONS MATHÉMATIQUES
// ============================================================

/**
 * @brief Calcule le déterminant d'une matrice 3x3
 */
float calibration_det3x3(const float m[9])
{
    return m[0] * (m[4] * m[8] - m[5] * m[7]) -
           m[1] * (m[3] * m[8] - m[5] * m[6]) +
           m[2] * (m[3] * m[7] - m[4] * m[6]);
}

/**
 * @brief Résout un système linéaire 3x3 (règle de Cramer)
 */
bool calibration_solve3x3(const float A[9], const float b[3], float x[3])
{
    float detA = calibration_det3x3(A);
    
    if (fabsf(detA) < 0.0001f)
    {
        // Matrice singulière
        return false;
    }
    
    // Matrice pour x[0] (a) : remplacer colonne 0 par b
    float A0[9] = {
        b[0], A[1], A[2],
        b[1], A[4], A[5],
        b[2], A[7], A[8]
    };
    
    // Matrice pour x[1] (b) : remplacer colonne 1 par b
    float A1[9] = {
        A[0], b[0], A[2],
        A[3], b[1], A[5],
        A[6], b[2], A[8]
    };
    
    // Matrice pour x[2] (c) : remplacer colonne 2 par b
    float A2[9] = {
        A[0], A[1], b[0],
        A[3], A[4], b[1],
        A[6], A[7], b[2]
    };
    
    x[0] = calibration_det3x3(A0) / detA;
    x[1] = calibration_det3x3(A1) / detA;
    x[2] = calibration_det3x3(A2) / detA;
    
    return true;
}

/**
 * @brief Calcule la moyenne
 */
float calibration_average(const uint16_t* data, uint8_t count)
{
    if (count == 0) return 0;
    
    float sum = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        sum += data[i];
    }
    
    return sum / count;
}

/**
 * @brief Calcule l'écart-type
 */
float calibration_stddev(const uint16_t* data, uint8_t count, float mean)
{
    if (count < 2) return 0;
    
    float sum = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        float diff = data[i] - mean;
        sum += diff * diff;
    }
    
    return sqrtf(sum / (count - 1));
}

// ============================================================
// SECTION 7 : DÉBOGAGE
// ============================================================

/**
 * @brief Affiche les données de calibration
 */
void calibration_print_data(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║       DONNÉES DE CALIBRATION                 ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Version    : %d                              ║\n", calib_data.version);
    printf("║ Méthode    : %d points                       ║\n", calib_data.pointCount);
    printf("║ Valide     : %s                              ║\n", calib_data.valid ? "Oui" : "Non");
    printf("║ Sauvegardée: %s                              ║\n", calib_data.saved ? "Oui" : "Non");
    printf("╠══════════════════════════════════════════════╣\n");
    
    for (uint8_t i = 0; i < calib_data.pointCount; i++)
    {
        CalibrationPoint* p = &calib_data.points[i];
        printf("║ Point %d : Écran(%3d,%3d) ADC(%4d,%4d) Err(%.1f,%.1f) ║\n",
               i, p->screenX, p->screenY, p->adcX, p->adcY, p->errorX, p->errorY);
    }
    
    printf("╚══════════════════════════════════════════════╝\n\n");
}

/**
 * @brief Affiche la matrice
 */
void calibration_print_matrix(void)
{
    printf("\n═══ MATRICE DE CALIBRATION ═══\n");
    printf("X_ecran = %.4f * X_adc + %.4f * Y_adc + %.4f\n",
           calib_data.matrix.a, calib_data.matrix.b, calib_data.matrix.c);
    printf("Y_ecran = %.4f * X_adc + %.4f * Y_adc + %.4f\n",
           calib_data.matrix.d, calib_data.matrix.e, calib_data.matrix.f);
    printf("Max Error : %.2f px\n", calib_data.matrix.maxError);
    printf("Avg Error : %.2f px\n", calib_data.matrix.avgError);
    printf("══════════════════════════════\n\n");
}

/**
 * @brief Affiche les erreurs
 */
void calibration_print_errors(void)
{
    printf("\n═══ ERREURS PAR POINT ═══\n");
    
    for (uint8_t i = 0; i < calib_data.pointCount; i++)
    {
        CalibrationPoint* p = &calib_data.points[i];
        printf("Point %d : ΔX=%.2f ΔY=%.2f Total=%.2f px\n",
               i, p->errorX, p->errorY, sqrtf(p->errorX*p->errorX + p->errorY*p->errorY));
    }
    printf("══════════════════════════\n\n");
}

/**
 * @brief Test de validation
 */
bool calibration_validate(void)
{
    if (!calib_data.valid) return false;
    
    // Vérifier que les coefficients sont raisonnables
    if (fabsf(calib_data.matrix.a) > 10 || fabsf(calib_data.matrix.d) > 10)
    {
        CALIB_DEBUG("Coefficients suspects\n");
        return false;
    }
    
    return true;
}

/**
 * @brief Réinitialise les données
 */
void calibration_reset(void)
{
    calibration_erase();
    calibration_init();
}