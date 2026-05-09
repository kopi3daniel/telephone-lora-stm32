/**
 * @file keypad_matrix.h
 * @brief Définition de la matrice du clavier physique 6x4 (24 touches)
 * 
 * Ce fichier contient toutes les définitions pour le clavier
 * matriciel 6 lignes × 4 colonnes = 24 touches.
 * 
 * Disposition physique des touches :
 * 
 *        Col0    Col1    Col2    Col3
 *        PE8     PE9     PE10    PE11
 * Row0 ┌───────┬───────┬───────┬───────┐
 * PD0  │ MENU  │ HAUT  │  OK   │ BAS   │
 *      ├───────┼───────┼───────┼───────┤
 * Row1 │   1   │   2   │   3   │RETOUR │
 * PD1  │       │ (abc) │ (def) │       │
 *      ├───────┼───────┼───────┼───────┤
 * Row2 │   4   │   5   │   6   │ APPEL │
 * PD2  │ (ghi) │ (jkl) │ (mno) │  📞   │
 *      ├───────┼───────┼───────┼───────┤
 * Row3 │   7   │   8   │   9   │ RACC  │
 * PD3  │ (pqrs)│ (tuv) │ (wxyz)│  ⏹   │
 *      ├───────┼───────┼───────┼───────┤
 * Row4 │   *   │   0   │   #   │  MAJ  │
 * PD4  │       │ (esp) │       │  🔤   │
 *      ├───────┼───────┼───────┼───────┤
 * Row5 │ LAMPE │  PTT  │ MUET  │ VOL   │
 * PD5  │  🔦   │  🎤   │  🔇   │ 🔊   │
 *      └───────┴───────┴───────┴───────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef KEYPAD_MATRIX_H
#define KEYPAD_MATRIX_H

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
// SECTION 1 : DIMENSIONS DE LA MATRICE
// ============================================================

/** @brief Nombre de lignes (sorties) */
#define KEYPAD_ROWS                     6

/** @brief Nombre de colonnes (entrées) */
#define KEYPAD_COLS                     4

/** @brief Nombre total de touches */
#define KEYPAD_TOTAL_KEYS               (KEYPAD_ROWS * KEYPAD_COLS)

/** @brief Nombre maximal de touches pressées simultanément */
#define KEYPAD_MAX_SIMULTANEOUS_KEYS    3

// ============================================================
// SECTION 2 : BROCHES DE LA MATRICE
// ============================================================

/**
 * @name Broches des LIGNES (sorties)
 * @{
 */
#define KEYPAD_ROW_PORT                 GPIOD
#define KEYPAD_ROW0_PIN                 GPIO_PIN_0
#define KEYPAD_ROW1_PIN                 GPIO_PIN_1
#define KEYPAD_ROW2_PIN                 GPIO_PIN_2
#define KEYPAD_ROW3_PIN                 GPIO_PIN_3
#define KEYPAD_ROW4_PIN                 GPIO_PIN_4
#define KEYPAD_ROW5_PIN                 GPIO_PIN_5
/** @} */

/**
 * @name Broches des COLONNES (entrées)
 * @{
 */
#define KEYPAD_COL_PORT                 GPIOE
#define KEYPAD_COL0_PIN                 GPIO_PIN_8
#define KEYPAD_COL1_PIN                 GPIO_PIN_9
#define KEYPAD_COL2_PIN                 GPIO_PIN_10
#define KEYPAD_COL3_PIN                 GPIO_PIN_11
/** @} */

/** @brief Tableau des broches de lignes (pour l'itération) */
static const uint16_t KEYPAD_ROW_PINS[KEYPAD_ROWS] = {
    KEYPAD_ROW0_PIN,
    KEYPAD_ROW1_PIN,
    KEYPAD_ROW2_PIN,
    KEYPAD_ROW3_PIN,
    KEYPAD_ROW4_PIN,
    KEYPAD_ROW5_PIN
};

/** @brief Tableau des broches de colonnes (pour l'itération) */
static const uint16_t KEYPAD_COL_PINS[KEYPAD_COLS] = {
    KEYPAD_COL0_PIN,
    KEYPAD_COL1_PIN,
    KEYPAD_COL2_PIN,
    KEYPAD_COL3_PIN
};

// ============================================================
// SECTION 3 : CODES DES TOUCHES
// ============================================================

/**
 * @brief Codes identifiant chaque touche du clavier
 * 
 * Format du code : 0xRC où R = ligne (0-5) et C = colonne (0-3)
 * Exemple : la touche '5' est en ligne 2, colonne 1 → code 0x21
 */
typedef enum {
    // --- Row 0 : Touches de navigation ---
    KEY_MENU    = 0x00,     // Menu (col 0)
    KEY_UP      = 0x01,     // Haut (col 1)
    KEY_OK      = 0x02,     // OK/Valider (col 2)
    KEY_DOWN    = 0x03,     // Bas (col 3)
    
    // --- Row 1 : Touches 1-3 + Retour ---
    KEY_1       = 0x10,     // Touche 1 (col 0)
    KEY_2       = 0x11,     // Touche 2/ABC (col 1)
    KEY_3       = 0x12,     // Touche 3/DEF (col 2)
    KEY_BACK    = 0x13,     // Retour/Effacer (col 3)
    
    // --- Row 2 : Touches 4-6 + Appel ---
    KEY_4       = 0x20,     // Touche 4/GHI (col 0)
    KEY_5       = 0x21,     // Touche 5/JKL (col 1)
    KEY_6       = 0x22,     // Touche 6/MNO (col 2)
    KEY_CALL    = 0x23,     // Appeler/Décrocher (col 3)
    
    // --- Row 3 : Touches 7-9 + Raccrocher ---
    KEY_7       = 0x30,     // Touche 7/PQRS (col 0)
    KEY_8       = 0x31,     // Touche 8/TUV (col 1)
    KEY_9       = 0x32,     // Touche 9/WXYZ (col 2)
    KEY_END     = 0x33,     // Raccrocher/Fin (col 3)
    
    // --- Row 4 : Touches *, 0, # + MAJ ---
    KEY_STAR    = 0x40,     // Touche * (col 0)
    KEY_0       = 0x41,     // Touche 0/Espace (col 1)
    KEY_HASH    = 0x42,     // Touche # (col 2)
    KEY_SHIFT   = 0x43,     // Majuscule/Mode (col 3)
    
    // --- Row 5 : Touches fonction ---
    KEY_LAMP    = 0x50,     // Lampe torche (col 0)
    KEY_PTT     = 0x51,     // Push-To-Talk (col 1)
    KEY_MUTE    = 0x52,     // Muet (col 2)
    KEY_VOL     = 0x53,     // Volume (col 3)
    
    // --- Touches spéciales ---
    KEY_NONE    = 0xFF      // Aucune touche
} KeyCode;

// ============================================================
// SECTION 4 : MAPPING DES CARACTÈRES (MULTITAP)
// ============================================================

/**
 * @brief Nombre maximum d'appuis pour une touche
 */
#define KEYPAD_MAX_MULTITAP             5

/**
 * @brief Mapping Multitap - Mode minuscules (abc)
 * 
 * Chaque ligne correspond à une touche (0-9).
 * Chaque colonne correspond au nombre d'appuis (1-5).
 */
static const char MULTITAP_LOWER[10][KEYPAD_MAX_MULTITAP] = {
    {' ', ' ', ' ', ' ', ' '},      // Touche 0 : Espace
    {'.', ',', '?', '!', '@'},      // Touche 1 : Ponctuation
    {'a', 'b', 'c', '2', 'A'},      // Touche 2
    {'d', 'e', 'f', '3', 'D'},      // Touche 3
    {'g', 'h', 'i', '4', 'G'},      // Touche 4
    {'j', 'k', 'l', '5', 'J'},      // Touche 5
    {'m', 'n', 'o', '6', 'M'},      // Touche 6
    {'p', 'q', 'r', 's', '7'},      // Touche 7
    {'t', 'u', 'v', '8', 'T'},      // Touche 8
    {'w', 'x', 'y', 'z', '9'}       // Touche 9
};

/**
 * @brief Mapping Multitap - Mode majuscules (ABC)
 */
static const char MULTITAP_UPPER[10][KEYPAD_MAX_MULTITAP] = {
    {' ', ' ', ' ', ' ', ' '},      // Touche 0
    {'.', ',', '?', '!', '@'},      // Touche 1
    {'A', 'B', 'C', '2', 'a'},      // Touche 2
    {'D', 'E', 'F', '3', 'd'},      // Touche 3
    {'G', 'H', 'I', '4', 'g'},      // Touche 4
    {'J', 'K', 'L', '5', 'j'},      // Touche 5
    {'M', 'N', 'O', '6', 'm'},      // Touche 6
    {'P', 'Q', 'R', 'S', '7'},      // Touche 7
    {'T', 'U', 'V', '8', 't'},      // Touche 8
    {'W', 'X', 'Y', 'Z', '9'}       // Touche 9
};

/**
 * @brief Mapping Multitap - Mode chiffres (123)
 */
static const char MULTITAP_NUMBERS[10][KEYPAD_MAX_MULTITAP] = {
    {'0', ' ', ' ', ' ', ' '},      // Touche 0
    {'1', '.', ',', '?', '!'},      // Touche 1
    {'2', 'a', 'b', 'c', 'A'},      // Touche 2
    {'3', 'd', 'e', 'f', 'D'},      // Touche 3
    {'4', 'g', 'h', 'i', 'G'},      // Touche 4
    {'5', 'j', 'k', 'l', 'J'},      // Touche 5
    {'6', 'm', 'n', 'o', 'M'},      // Touche 6
    {'7', 'p', 'q', 'r', 's'},      // Touche 7
    {'8', 't', 'u', 'v', 'T'},      // Touche 8
    {'9', 'w', 'x', 'y', 'z'}       // Touche 9
};

// ============================================================
// SECTION 5 : MODES DE SAISIE
// ============================================================

/**
 * @brief Modes de saisie du clavier
 */
typedef enum {
    INPUT_MODE_ABC,             // Majuscules (première lettre en majuscule)
    INPUT_MODE_abc,             // Minuscules
    INPUT_MODE_123,             // Chiffres uniquement
    INPUT_MODE_T9,              // Texte prédictif (T9)
    INPUT_MODE_SYMBOLS          // Symboles spéciaux
} InputMode;

/**
 * @brief Nombre de modes de saisie
 */
#define INPUT_MODE_COUNT                5

/**
 * @brief Noms des modes (pour affichage)
 */
static const char* INPUT_MODE_NAMES[] = {
    "ABC",      // Majuscules
    "abc",      // Minuscules
    "123",      // Chiffres
    "T9",       // Prédictif
    "#+="       // Symboles
};

// ============================================================
// SECTION 6 : STRUCTURES DE DONNÉES
// ============================================================

/**
 * @brief Structure décrivant l'état d'une touche
 */
typedef struct {
    KeyCode code;                   // Code de la touche
    bool pressed;                   // Actuellement pressée ?
    bool lastState;                 // État précédent
    bool changed;                   // Changement d'état
    uint32_t pressTime;             // Timestamp de l'appui
    uint32_t releaseTime;           // Timestamp du relâchement
    uint32_t holdDuration;          // Durée de maintien
    uint8_t pressCount;             // Nombre d'appuis (multitap)
    uint32_t lastPressTime;         // Dernier appui (multitap)
} KeyState;

/**
 * @brief État complet du clavier
 */
typedef struct {
    KeyState keys[KEYPAD_TOTAL_KEYS];   // État de chaque touche
    InputMode inputMode;                // Mode de saisie actuel
    bool shiftLock;                     // Verrouillage majuscule
    uint8_t activeKeyCount;             // Nombre de touches actives
    KeyCode lastKey;                    // Dernière touche pressée
    uint32_t lastScanTime;             // Timestamp du dernier scan
    uint32_t totalKeyPresses;           // Nombre total d'appuis
} KeypadState;

/**
 * @brief Événements du clavier
 */
typedef enum {
    KEY_EVENT_NONE      = 0,        // Aucun événement
    KEY_EVENT_PRESS     = 1,        // Touche pressée
    KEY_EVENT_RELEASE   = 2,        // Touche relâchée
    KEY_EVENT_HOLD      = 3,        // Touche maintenue (> 500ms)
    KEY_EVENT_REPEAT    = 4         // Répétition automatique
} KeyEvent;

/**
 * @brief Structure d'un événement clavier
 */
typedef struct {
    KeyCode key;                    // Touche concernée
    KeyEvent event;                 // Type d'événement
    uint32_t timestamp;             // Horodatage
    uint8_t row;                    // Ligne (0-5)
    uint8_t col;                    // Colonne (0-3)
    char character;                 // Caractère résultant (si saisie texte)
} KeypadEvent;

// ============================================================
// SECTION 7 : FONCTIONS DE CONVERSION
// ============================================================

/**
 * @brief Convertit un code de touche en ligne
 * @param key Code de la touche
 * @return Ligne (0-5)
 */
static inline uint8_t keypad_get_row(KeyCode key)
{
    return (key >> 4) & 0x0F;
}

/**
 * @brief Convertit un code de touche en colonne
 * @param key Code de la touche
 * @return Colonne (0-3)
 */
static inline uint8_t keypad_get_col(KeyCode key)
{
    return key & 0x0F;
}

/**
 * @brief Construit un code de touche à partir de ligne/colonne
 * @param row Ligne (0-5)
 * @param col Colonne (0-3)
 * @return Code de la touche
 */
static inline KeyCode keypad_make_key(uint8_t row, uint8_t col)
{
    return (KeyCode)((row << 4) | col);
}

/**
 * @brief Vérifie si un code de touche est valide
 * @param key Code à vérifier
 * @return true si valide
 */
static inline bool keypad_is_valid_key(KeyCode key)
{
    uint8_t row = keypad_get_row(key);
    uint8_t col = keypad_get_col(key);
    return (row < KEYPAD_ROWS && col < KEYPAD_COLS);
}

// ============================================================
// SECTION 8 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Vérifie si une touche est une touche numérique
 */
#define KEYPAD_IS_NUMERIC(key)      ((key) >= KEY_0 && (key) <= KEY_9)

/**
 * @brief Vérifie si une touche est une touche de fonction
 */
#define KEYPAD_IS_FUNCTION(key)     ((key) >= KEY_MENU && (key) <= KEY_DOWN)

/**
 * @brief Vérifie si une touche est une touche d'appel
 */
#define KEYPAD_IS_CALL_KEY(key)     ((key) == KEY_CALL || (key) == KEY_END)

/**
 * @brief Vérifie si une touche produit un caractère
 */
#define KEYPAD_IS_CHAR_KEY(key)     ((key) >= KEY_1 && (key) <= KEY_9) || \
                                     (key) == KEY_0 || (key) == KEY_STAR || \
                                     (key) == KEY_HASH

/**
 * @brief Extrait le chiffre d'une touche numérique
 */
#define KEYPAD_KEY_TO_DIGIT(key)    ((key) - KEY_0)

// ============================================================
// SECTION 9 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define KEYPAD_DEBUG(fmt, ...)      printf("[KEYPAD] " fmt, ##__VA_ARGS__)
#else
    #define KEYPAD_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 10 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // KEYPAD_MATRIX_H