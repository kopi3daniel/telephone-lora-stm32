/**
 * @file keypad_multitap.h
 * @brief Gestion de la saisie Multitap et T9
 * 
 * Ce fichier implémente le système de saisie de texte par
 * appuis multiples sur les touches numériques.
 * 
 * Modes de saisie :
 * - ABC : Majuscules (première lettre en majuscule)
 * - abc : Minuscules
 * - 123 : Chiffres uniquement
 * - T9  : Texte prédictif (dictionnaire intégré)
 * - #+= : Symboles spéciaux
 * 
 * Fonctionnement Multitap :
 * - Appui 1 sur '2' → 'a' (ou 'A' en mode ABC)
 * - Appui 2 sur '2' → 'b'
 * - Appui 3 sur '2' → 'c'
 * - Appui 4 sur '2' → '2' (reboucle)
 * - Timeout 1s → le caractère est validé
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef KEYPAD_MULTITAP_H
#define KEYPAD_MULTITAP_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "keypad_matrix.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module Multitap */
#define MULTITAP_VERSION                "1.0.0"

/** @brief Timeout Multitap par défaut (ms) */
#define MULTITAP_DEFAULT_TIMEOUT_MS     1000

/** @brief Timeout minimum (ms) */
#define MULTITAP_MIN_TIMEOUT_MS         500

/** @brief Timeout maximum (ms) */
#define MULTITAP_MAX_TIMEOUT_MS         3000

/** @brief Nombre maximum de caractères dans le buffer */
#define MULTITAP_BUFFER_SIZE            256

/** @brief Nombre maximum d'appuis par touche */
#define MULTITAP_MAX_PRESSES            5

/** @brief Taille du dictionnaire T9 */
#define MULTITAP_DICTIONARY_SIZE        500

/** @brief Longueur maximale d'un mot dans le dictionnaire */
#define MULTITAP_MAX_WORD_LENGTH        32

// ============================================================
// SECTION 2 : MODES DE SAISIE ÉTENDUS
// ============================================================

/**
 * @brief Modes de saisie disponibles
 */
typedef enum {
    MULTITAP_MODE_ABC       = 0,    // Majuscules (première lettre en majuscule)
    MULTITAP_MODE_abc       = 1,    // Minuscules
    MULTITAP_MODE_ABC_LOCK  = 2,    // Verrouillage majuscules (CAPS LOCK)
    MULTITAP_MODE_123       = 3,    // Chiffres uniquement
    MULTITAP_MODE_T9_ABC    = 4,    // T9 avec majuscules
    MULTITAP_MODE_T9_abc    = 5,    // T9 avec minuscules
    MULTITAP_MODE_SYMBOLS   = 6,    // Symboles spéciaux
    MULTITAP_MODE_COUNT      = 7     // Nombre total de modes
} MultitapMode;

/**
 * @brief Noms des modes pour affichage
 */
static const char* MULTITAP_MODE_NAMES[] = {
    "ABC",      // Majuscules
    "abc",      // Minuscules
    "ABC",      // CAPS LOCK
    "123",      // Chiffres
    "T9 Abc",   // T9 majuscules
    "T9 abc",   // T9 minuscules
    "#+="       // Symboles
};

// ============================================================
// SECTION 3 : STRUCTURES DE DONNÉES
// ============================================================

/**
 * @brief État de la saisie Multitap
 */
typedef struct {
    // Buffer de texte
    char textBuffer[MULTITAP_BUFFER_SIZE];  // Texte saisi
    uint16_t bufferLength;                  // Longueur du texte
    uint16_t cursorPosition;                // Position du curseur
    
    // État de la touche en cours
    KeyCode currentKey;                     // Touche en cours d'édition
    uint8_t pressCount;                     // Nombre d'appuis sur la touche
    uint32_t lastPressTime;                 // Dernier appui
    char previewChar;                       // Caractère prévisualisé
    bool isEditing;                         // En mode édition
    
    // Mode de saisie
    MultitapMode mode;                      // Mode actuel
    bool shiftLock;                         // Verrouillage majuscule
    bool autoCapitalize;                    // Majuscule automatique
    
    // T9
    bool t9Enabled;                         // T9 activé
    uint8_t t9CurrentDigit;                // Séquence T9 en cours
    char t9Sequence[16];                   // Séquence de chiffres T9
    uint8_t t9SequenceLength;              // Longueur de la séquence
    char t9Suggestions[5][MULTITAP_MAX_WORD_LENGTH];  // Suggestions
    uint8_t t9SuggestionCount;             // Nombre de suggestions
    uint8_t t9SelectedSuggestion;          // Suggestion sélectionnée
    
    // Symboles
    uint8_t symbolPage;                    // Page de symboles (0-2)
    
    // Statistiques
    uint32_t totalCharacters;              // Caractères saisis
    uint32_t totalWords;                   // Mots saisis
} MultitapState;

/**
 * @brief Entrée du dictionnaire T9
 */
typedef struct {
    char word[MULTITAP_MAX_WORD_LENGTH];   // Mot
    uint8_t priority;                       // Priorité (0 = haute)
} T9DictionaryEntry;

// ============================================================
// SECTION 4 : MAPPING DES TOUCHES (ÉTENDU)
// ============================================================

/**
 * @brief Nombre de caractères par touche numérique
 */
static const uint8_t MULTITAP_CHAR_COUNT[10] = {
    1,  // Touche 0 : Espace
    5,  // Touche 1 : . , ? ! @
    3,  // Touche 2 : a b c
    3,  // Touche 3 : d e f
    3,  // Touche 4 : g h i
    3,  // Touche 5 : j k l
    3,  // Touche 6 : m n o
    4,  // Touche 7 : p q r s
    3,  // Touche 8 : t u v
    4   // Touche 9 : w x y z
};

/**
 * @brief Mapping T9 (letters to digits)
 */
static const char T9_MAP[26] = {
    '2', '2', '2',  // A B C
    '3', '3', '3',  // D E F
    '4', '4', '4',  // G H I
    '5', '5', '5',  // J K L
    '6', '6', '6',  // M N O
    '7', '7', '7', '7',  // P Q R S
    '8', '8', '8',  // T U V
    '9', '9', '9', '9'   // W X Y Z
};

/**
 * @brief Pages de symboles
 */
static const char* SYMBOL_PAGES[] = {
    ".,?!@#'\"-_:;()",       // Page 0 : Ponctuation
    "&%$€£¥+×÷=<>~\",       // Page 1 : Monnaie et maths
    "[]{}|/\\^`*§©®™°"      // Page 2 : Divers
};

/** @brief Nombre de pages de symboles */
#define SYMBOL_PAGE_COUNT               3

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise le module Multitap
 */
void multitap_init(void);

/**
 * @brief Réinitialise l'état de la saisie
 */
void multitap_reset(void);

/**
 * @brief Efface le buffer de texte
 */
void multitap_clear_buffer(void);

// ============================================================
// SECTION 6 : FONCTIONS DE TRAITEMENT DES TOUCHES
// ============================================================

/**
 * @brief Traite l'appui sur une touche
 * @param key Touche pressée
 * @return Caractère produit (0 si pas de caractère)
 */
char multitap_process_key(KeyCode key);

/**
 * @brief Traite une touche en mode ABC/abc
 * @param key Touche pressée
 * @return Caractère produit
 */
char multitap_process_abc(KeyCode key);

/**
 * @brief Traite une touche en mode 123
 * @param key Touche pressée
 * @return Caractère produit
 */
char multitap_process_numbers(KeyCode key);

/**
 * @brief Traite une touche en mode T9
 * @param key Touche pressée
 * @return true si une suggestion est sélectionnée
 */
bool multitap_process_t9(KeyCode key);

/**
 * @brief Traite une touche en mode Symboles
 * @param key Touche pressée
 * @return Caractère produit
 */
char multitap_process_symbols(KeyCode key);

// ============================================================
// SECTION 7 : FONCTIONS DE SAISIE
// ============================================================

/**
 * @brief Insère un caractère dans le buffer
 * @param c Caractère à insérer
 */
void multitap_insert_char(char c);

/**
 * @brief Supprime le caractère avant le curseur
 */
void multitap_delete_char(void);

/**
 * @brief Supprime tout le texte
 */
void multitap_delete_all(void);

/**
 * @brief Déplace le curseur à gauche
 */
void multitap_cursor_left(void);

/**
 * @brief Déplace le curseur à droite
 */
void multitap_cursor_right(void);

/**
 * @brief Valide le caractère en cours d'édition
 */
void multitap_commit_char(void);

/**
 * @brief Insère un espace
 */
void multitap_insert_space(void);

/**
 * @brief Insère un saut de ligne
 */
void multitap_new_line(void);

// ============================================================
// SECTION 8 : FONCTIONS DE MODE
// ============================================================

/**
 * @brief Passe au mode de saisie suivant
 */
void multitap_next_mode(void);

/**
 * @brief Passe au mode de saisie précédent
 */
void multitap_prev_mode(void);

/**
 * @brief Définit le mode de saisie
 * @param mode Mode souhaité
 */
void multitap_set_mode(MultitapMode mode);

/**
 * @brief Récupère le mode de saisie actuel
 * @return Mode actuel
 */
MultitapMode multitap_get_mode(void);

/**
 * @brief Active/désactive le verrouillage majuscule
 */
void multitap_toggle_shift_lock(void);

/**
 * @brief Active/désactive le mode T9
 */
void multitap_toggle_t9(void);

// ============================================================
// SECTION 9 : FONCTIONS DU DICTIONNAIRE T9
// ============================================================

/**
 * @brief Ajoute un mot au dictionnaire T9
 * @param word Mot à ajouter
 */
void multitap_t9_add_word(const char* word);

/**
 * @brief Recherche des suggestions T9
 * @param sequence Séquence de chiffres
 * @param suggestions Tableau de suggestions (sortie)
 * @param maxCount Nombre maximum de suggestions
 * @return Nombre de suggestions trouvées
 */
uint8_t multitap_t9_find_suggestions(const char* sequence, 
                                      char suggestions[][MULTITAP_MAX_WORD_LENGTH],
                                      uint8_t maxCount);

/**
 * @brief Sélectionne la suggestion suivante
 */
void multitap_t9_next_suggestion(void);

/**
 * @brief Sélectionne la suggestion précédente
 */
void multitap_t9_prev_suggestion(void);

/**
 * @brief Valide la suggestion sélectionnée
 */
void multitap_t9_accept_suggestion(void);

/**
 * @brief Charge le dictionnaire depuis la Flash
 * @return true si chargé
 */
bool multitap_t9_load_dictionary(void);

/**
 * @brief Sauvegarde le dictionnaire en Flash
 * @return true si sauvegardé
 */
bool multitap_t9_save_dictionary(void);

// ============================================================
// SECTION 10 : FONCTIONS DE LECTURE
// ============================================================

/**
 * @brief Récupère le texte saisi
 * @return Pointeur vers le buffer de texte
 */
const char* multitap_get_text(void);

/**
 * @brief Récupère la longueur du texte
 * @return Nombre de caractères
 */
uint16_t multitap_get_length(void);

/**
 * @brief Récupère la position du curseur
 * @return Position du curseur
 */
uint16_t multitap_get_cursor(void);

/**
 * @brief Récupère le caractère en cours de prévisualisation
 * @return Caractère prévisualisé (0 si aucun)
 */
char multitap_get_preview_char(void);

/**
 * @brief Récupère le nombre de suggestions T9
 * @return Nombre de suggestions
 */
uint8_t multitap_t9_get_suggestion_count(void);

/**
 * @brief Récupère une suggestion T9
 * @param index Index de la suggestion
 * @return Pointeur vers la suggestion
 */
const char* multitap_t9_get_suggestion(uint8_t index);

/**
 * @brief Vérifie si le module est en cours d'édition
 * @return true si en édition
 */
bool multitap_is_editing(void);

// ============================================================
// SECTION 11 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état de la saisie Multitap
 */
void multitap_print_state(void);

/**
 * @brief Affiche le contenu du buffer
 */
void multitap_print_buffer(void);

/**
 * @brief Affiche le dictionnaire T9
 */
void multitap_t9_print_dictionary(void);

/**
 * @brief Affiche les statistiques de saisie
 */
void multitap_print_statistics(void);

/**
 * @brief Auto-test du module Multitap
 * @return true si les tests passent
 */
bool multitap_self_test(void);

// ============================================================
// SECTION 12 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Vérifie si une touche est une touche de saisie
 */
#define MULTITAP_IS_INPUT_KEY(key)  ((key) >= KEY_0 && (key) <= KEY_9)

/**
 * @brief Vérifie si le buffer est plein
 */
#define MULTITAP_BUFFER_FULL()      (multitap_state.bufferLength >= MULTITAP_BUFFER_SIZE - 1)

/**
 * @brief Vérifie si le buffer est vide
 */
#define MULTITAP_BUFFER_EMPTY()     (multitap_state.bufferLength == 0)

// ============================================================
// SECTION 13 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define MULTITAP_DEBUG(fmt, ...)    printf("[MULTITAP] " fmt, ##__VA_ARGS__)
#else
    #define MULTITAP_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 14 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // KEYPAD_MULTITAP_H