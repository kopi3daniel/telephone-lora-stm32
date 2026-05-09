/**
 * @file keypad_multitap.cpp
 * @brief Implémentation du système de saisie Multitap et T9
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans keypad_multitap.h.
 * 
 * Il gère :
 * - La saisie Multitap (ABC, abc, 123)
 * - La saisie T9 prédictive
 * - La saisie de symboles
 * - Le dictionnaire T9 intégré
 * - Le buffer de texte avec curseur
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "keypad_multitap.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État global de la saisie Multitap */
static MultitapState multitap_state;

/** @brief Dictionnaire T9 intégré (mots français courants) */
static T9DictionaryEntry t9_dictionary[MULTITAP_DICTIONARY_SIZE] = {
    // Mots de base préchargés
    {"bonjour", 1},
    {"salut", 1},
    {"merci", 1},
    {"oui", 2},
    {"non", 2},
    {"allo", 1},
    {"comment", 3},
    {"ca", 2},
    {"va", 2},
    {"bien", 3},
    {"appel", 1},
    {"message", 2},
    {"telephone", 1},
    {"urgence", 1},
    {"aide", 2},
    {"test", 3},
    {"ok", 1},
    {"rdv", 2},
    {"maison", 3},
    {"travail", 3},
    // ... plus de mots seront ajoutés
    {"", 0}  // Terminateur
};

/** @brief Nombre de mots dans le dictionnaire */
static uint16_t t9_dictionary_count = 20;  // Nombre de mots préchargés

/** @brief Timeout Multitap configuré */
static uint16_t multitap_timeout_ms = MULTITAP_DEFAULT_TIMEOUT_MS;

/** @brief Dernier timestamp d'activité */
static uint32_t last_activity_time = 0;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le module Multitap
 */
void multitap_init(void)
{
    MULTITAP_DEBUG("Initialisation du module Multitap...\n");
    
    // Réinitialiser l'état
    memset(&multitap_state, 0, sizeof(MultitapState));
    
    // Configuration par défaut
    multitap_state.mode = MULTITAP_MODE_abc;
    multitap_state.autoCapitalize = true;
    multitap_state.t9Enabled = false;
    multitap_state.symbolPage = 0;
    
    // Charger le dictionnaire si disponible
    if (!multitap_t9_load_dictionary())
    {
        MULTITAP_DEBUG("Dictionnaire par défaut utilisé\n");
    }
    
    MULTITAP_DEBUG("Module Multitap initialisé\n");
}

/**
 * @brief Réinitialise l'état
 */
void multitap_reset(void)
{
    multitap_clear_buffer();
    multitap_state.currentKey = KEY_NONE;
    multitap_state.pressCount = 0;
    multitap_state.previewChar = 0;
    multitap_state.isEditing = false;
    multitap_state.t9SequenceLength = 0;
    multitap_state.t9SuggestionCount = 0;
    multitap_state.symbolPage = 0;
}

/**
 * @brief Efface le buffer de texte
 */
void multitap_clear_buffer(void)
{
    memset(multitap_state.textBuffer, 0, MULTITAP_BUFFER_SIZE);
    multitap_state.bufferLength = 0;
    multitap_state.cursorPosition = 0;
}

// ============================================================
// SECTION 2 : TRAITEMENT DES TOUCHES
// ============================================================

/**
 * @brief Traite l'appui sur une touche
 */
char multitap_process_key(KeyCode key)
{
    if (!MULTITAP_IS_INPUT_KEY(key)) return 0;
    
    last_activity_time = HAL_GetTick();
    
    // Vérifier le timeout Multitap
    if (multitap_state.isEditing)
    {
        uint32_t elapsed = HAL_GetTick() - multitap_state.lastPressTime;
        
        if (elapsed > multitap_timeout_ms || key != multitap_state.currentKey)
        {
            // Timeout ou touche différente → valider le caractère en cours
            multitap_commit_char();
        }
    }
    
    char result = 0;
    
    // Traiter selon le mode
    if (multitap_state.mode >= MULTITAP_MODE_T9_ABC)
    {
        // Mode T9
        multitap_process_t9(key);
    }
    else if (multitap_state.mode == MULTITAP_MODE_123)
    {
        // Mode chiffres
        result = multitap_process_numbers(key);
    }
    else if (multitap_state.mode == MULTITAP_MODE_SYMBOLS)
    {
        // Mode symboles
        result = multitap_process_symbols(key);
    }
    else
    {
        // Mode ABC/abc
        result = multitap_process_abc(key);
    }
    
    multitap_state.totalCharacters++;
    
    return result;  // Retourne le caractère si validé, 0 si prévisualisation
}

/**
 * @brief Traite une touche en mode ABC/abc
 */
char multitap_process_abc(KeyCode key)
{
    uint8_t digit = KEYPAD_KEY_TO_DIGIT(key);
    uint8_t pressCount;
    
    if (key == multitap_state.currentKey)
    {
        // Même touche → incrémenter le compteur
        multitap_state.pressCount++;
        if (multitap_state.pressCount > MULTITAP_CHAR_COUNT[digit])
        {
            multitap_state.pressCount = 1;  // Reboucler
        }
    }
    else
    {
        // Nouvelle touche
        multitap_state.currentKey = key;
        multitap_state.pressCount = 1;
    }
    
    pressCount = multitap_state.pressCount - 1;  // 0-based
    multitap_state.lastPressTime = HAL_GetTick();
    multitap_state.isEditing = true;
    
    // Obtenir le caractère selon le mode
    char c = 0;
    
    if (multitap_state.mode == MULTITAP_MODE_ABC)
    {
        c = MULTITAP_UPPER[digit][pressCount];
    }
    else if (multitap_state.mode == MULTITAP_MODE_abc)
    {
        c = MULTITAP_LOWER[digit][pressCount];
    }
    else if (multitap_state.mode == MULTITAP_MODE_ABC_LOCK)
    {
        c = MULTITAP_UPPER[digit][pressCount];
    }
    
    multitap_state.previewChar = c;
    
    // Notifier la prévisualisation
    MULTITAP_DEBUG("Preview: '%c' (digit=%d, press=%d)\n", c, digit, pressCount + 1);
    
    return 0;  // Pas encore validé
}

/**
 * @brief Traite une touche en mode 123
 */
char multitap_process_numbers(KeyCode key)
{
    uint8_t digit = KEYPAD_KEY_TO_DIGIT(key);
    
    // En mode chiffres, valider immédiatement
    char c = '0' + digit;
    multitap_insert_char(c);
    multitap_state.totalWords++;
    
    MULTITAP_DEBUG("Number: '%c'\n", c);
    
    return c;
}

/**
 * @brief Traite une touche en mode T9
 */
bool multitap_process_t9(KeyCode key)
{
    uint8_t digit = KEYPAD_KEY_TO_DIGIT(key);
    
    // Ajouter le chiffre à la séquence T9
    if (multitap_state.t9SequenceLength < 15)
    {
        multitap_state.t9Sequence[multitap_state.t9SequenceLength] = '0' + digit;
        multitap_state.t9SequenceLength++;
        multitap_state.t9Sequence[multitap_state.t9SequenceLength] = '\0';
    }
    
    // Rechercher des suggestions
    multitap_state.t9SuggestionCount = multitap_t9_find_suggestions(
        multitap_state.t9Sequence,
        multitap_state.t9Suggestions,
        5
    );
    
    multitap_state.t9SelectedSuggestion = 0;
    
    if (multitap_state.t9SuggestionCount > 0)
    {
        // Afficher la première suggestion
        multitap_state.previewChar = multitap_state.t9Suggestions[0][0];
        MULTITAP_DEBUG("T9: %d suggestions pour '%s'\n", 
                      multitap_state.t9SuggestionCount,
                      multitap_state.t9Sequence);
    }
    
    return (multitap_state.t9SuggestionCount > 0);
}

/**
 * @brief Traite une touche en mode Symboles
 */
char multitap_process_symbols(KeyCode key)
{
    uint8_t digit = KEYPAD_KEY_TO_DIGIT(key);
    
    // Obtenir la page de symboles actuelle
    const char* page = SYMBOL_PAGES[multitap_state.symbolPage];
    uint8_t pageLength = strlen(page);
    
    char c = 0;
    
    if (digit < pageLength)
    {
        c = page[digit];
        multitap_insert_char(c);
    }
    
    MULTITAP_DEBUG("Symbol: '%c' (page=%d, digit=%d)\n", c, multitap_state.symbolPage, digit);
    
    return c;
}

// ============================================================
// SECTION 3 : FONCTIONS DE SAISIE
// ============================================================

/**
 * @brief Insère un caractère dans le buffer
 */
void multitap_insert_char(char c)
{
    if (MULTITAP_BUFFER_FULL()) return;
    if (c == 0) return;
    
    // Appliquer l'auto-capitalisation
    if (multitap_state.autoCapitalize && multitap_state.bufferLength == 0)
    {
        if (c >= 'a' && c <= 'z')
        {
            c = c - 'a' + 'A';  // Convertir en majuscule
        }
    }
    
    // Décaler les caractères après le curseur
    if (multitap_state.cursorPosition < multitap_state.bufferLength)
    {
        memmove(&multitap_state.textBuffer[multitap_state.cursorPosition + 1],
                &multitap_state.textBuffer[multitap_state.cursorPosition],
                multitap_state.bufferLength - multitap_state.cursorPosition);
    }
    
    // Insérer le caractère
    multitap_state.textBuffer[multitap_state.cursorPosition] = c;
    multitap_state.bufferLength++;
    multitap_state.cursorPosition++;
    multitap_state.textBuffer[multitap_state.bufferLength] = '\0';
    
    // Réinitialiser l'état d'édition
    multitap_state.currentKey = KEY_NONE;
    multitap_state.pressCount = 0;
    multitap_state.previewChar = 0;
    multitap_state.isEditing = false;
    
    MULTITAP_DEBUG("Inserted '%c' at position %d\n", c, multitap_state.cursorPosition - 1);
}

/**
 * @brief Supprime le caractère avant le curseur
 */
void multitap_delete_char(void)
{
    if (multitap_state.cursorPosition > 0)
    {
        // Décaler les caractères
        memmove(&multitap_state.textBuffer[multitap_state.cursorPosition - 1],
                &multitap_state.textBuffer[multitap_state.cursorPosition],
                multitap_state.bufferLength - multitap_state.cursorPosition);
        
        multitap_state.bufferLength--;
        multitap_state.cursorPosition--;
        multitap_state.textBuffer[multitap_state.bufferLength] = '\0';
        
        MULTITAP_DEBUG("Deleted char at position %d\n", multitap_state.cursorPosition);
    }
}

/**
 * @brief Supprime tout le texte
 */
void multitap_delete_all(void)
{
    multitap_clear_buffer();
}

/**
 * @brief Déplace le curseur à gauche
 */
void multitap_cursor_left(void)
{
    if (multitap_state.cursorPosition > 0)
    {
        multitap_state.cursorPosition--;
    }
}

/**
 * @brief Déplace le curseur à droite
 */
void multitap_cursor_right(void)
{
    if (multitap_state.cursorPosition < multitap_state.bufferLength)
    {
        multitap_state.cursorPosition++;
    }
}

/**
 * @brief Valide le caractère en cours d'édition
 */
void multitap_commit_char(void)
{
    if (multitap_state.isEditing && multitap_state.previewChar != 0)
    {
        multitap_insert_char(multitap_state.previewChar);
        multitap_state.isEditing = false;
        multitap_state.previewChar = 0;
    }
}

/**
 * @brief Insère un espace
 */
void multitap_insert_space(void)
{
    // Valider d'abord le caractère en cours
    multitap_commit_char();
    
    // Insérer un espace
    multitap_insert_char(' ');
    multitap_state.totalWords++;
}

/**
 * @brief Insère un saut de ligne
 */
void multitap_new_line(void)
{
    multitap_commit_char();
    multitap_insert_char('\n');
}

// ============================================================
// SECTION 4 : GESTION DES MODES
// ============================================================

/**
 * @brief Passe au mode suivant
 */
void multitap_next_mode(void)
{
    uint8_t nextMode = (multitap_state.mode + 1) % MULTITAP_MODE_COUNT;
    multitap_set_mode((MultitapMode)nextMode);
}

/**
 * @brief Passe au mode précédent
 */
void multitap_prev_mode(void)
{
    uint8_t prevMode = (multitap_state.mode == 0) ? 
                       MULTITAP_MODE_COUNT - 1 : 
                       multitap_state.mode - 1;
    multitap_set_mode((MultitapMode)prevMode);
}

/**
 * @brief Définit le mode de saisie
 */
void multitap_set_mode(MultitapMode mode)
{
    multitap_state.mode = mode;
    
    // Activer/désactiver T9 selon le mode
    multitap_state.t9Enabled = (mode >= MULTITAP_MODE_T9_ABC);
    
    // Réinitialiser la séquence T9
    if (!multitap_state.t9Enabled)
    {
        multitap_state.t9SequenceLength = 0;
        multitap_state.t9SuggestionCount = 0;
    }
    
    MULTITAP_DEBUG("Mode: %s\n", MULTITAP_MODE_NAMES[mode]);
}

/**
 * @brief Récupère le mode actuel
 */
MultitapMode multitap_get_mode(void)
{
    return multitap_state.mode;
}

/**
 * @brief Bascule le verrouillage majuscule
 */
void multitap_toggle_shift_lock(void)
{
    if (multitap_state.mode == MULTITAP_MODE_ABC_LOCK)
    {
        multitap_set_mode(MULTITAP_MODE_abc);
    }
    else if (multitap_state.mode == MULTITAP_MODE_abc)
    {
        multitap_set_mode(MULTITAP_MODE_ABC_LOCK);
    }
    else if (multitap_state.mode == MULTITAP_MODE_ABC)
    {
        multitap_set_mode(MULTITAP_MODE_ABC_LOCK);
    }
}

/**
 * @brief Bascule le mode T9
 */
void multitap_toggle_t9(void)
{
    if (multitap_state.t9Enabled)
    {
        multitap_set_mode(MULTITAP_MODE_abc);
    }
    else
    {
        multitap_set_mode(MULTITAP_MODE_T9_abc);
    }
}

// ============================================================
// SECTION 5 : DICTIONNAIRE T9
// ============================================================

/**
 * @brief Ajoute un mot au dictionnaire
 */
void multitap_t9_add_word(const char* word)
{
    if (word == NULL || strlen(word) == 0) return;
    if (t9_dictionary_count >= MULTITAP_DICTIONARY_SIZE - 1) return;
    
    // Vérifier si le mot existe déjà
    for (uint16_t i = 0; i < t9_dictionary_count; i++)
    {
        if (strcasecmp(t9_dictionary[i].word, word) == 0)
        {
            // Augmenter la priorité
            if (t9_dictionary[i].priority > 0)
            {
                t9_dictionary[i].priority--;
            }
            return;
        }
    }
    
    // Ajouter le nouveau mot
    strncpy(t9_dictionary[t9_dictionary_count].word, word, MULTITAP_MAX_WORD_LENGTH - 1);
    t9_dictionary[t9_dictionary_count].priority = 5;  // Priorité moyenne
    t9_dictionary_count++;
    
    MULTITAP_DEBUG("Mot ajouté au dictionnaire: '%s'\n", word);
}

/**
 * @brief Convertit un mot en séquence T9
 */
static void word_to_t9_sequence(const char* word, char* sequence)
{
    uint8_t len = strlen(word);
    
    for (uint8_t i = 0; i < len; i++)
    {
        char c = word[i];
        
        if (c >= 'a' && c <= 'z')
        {
            sequence[i] = T9_MAP[c - 'a'];
        }
        else if (c >= 'A' && c <= 'Z')
        {
            sequence[i] = T9_MAP[c - 'A'];
        }
        else
        {
            sequence[i] = '0';  // Caractère non mappé
        }
    }
    sequence[len] = '\0';
}

/**
 * @brief Recherche des suggestions T9
 */
uint8_t multitap_t9_find_suggestions(const char* sequence,
                                      char suggestions[][MULTITAP_MAX_WORD_LENGTH],
                                      uint8_t maxCount)
{
    if (sequence == NULL || suggestions == NULL) return 0;
    if (strlen(sequence) == 0) return 0;
    
    uint8_t count = 0;
    
    // Parcourir le dictionnaire
    for (uint16_t i = 0; i < t9_dictionary_count && count < maxCount; i++)
    {
        char wordSequence[MULTITAP_MAX_WORD_LENGTH];
        word_to_t9_sequence(t9_dictionary[i].word, wordSequence);
        
        // Comparer la séquence
        if (strncmp(sequence, wordSequence, strlen(sequence)) == 0)
        {
            // Vérifier la longueur
            if (strlen(wordSequence) >= strlen(sequence))
            {
                strncpy(suggestions[count], t9_dictionary[i].word, MULTITAP_MAX_WORD_LENGTH - 1);
                suggestions[count][MULTITAP_MAX_WORD_LENGTH - 1] = '\0';
                count++;
            }
        }
    }
    
    return count;
}

/**
 * @brief Sélectionne la suggestion suivante
 */
void multitap_t9_next_suggestion(void)
{
    if (multitap_state.t9SuggestionCount > 0)
    {
        multitap_state.t9SelectedSuggestion = 
            (multitap_state.t9SelectedSuggestion + 1) % multitap_state.t9SuggestionCount;
    }
}

/**
 * @brief Sélectionne la suggestion précédente
 */
void multitap_t9_prev_suggestion(void)
{
    if (multitap_state.t9SuggestionCount > 0)
    {
        multitap_state.t9SelectedSuggestion = 
            (multitap_state.t9SelectedSuggestion == 0) ? 
            multitap_state.t9SuggestionCount - 1 : 
            multitap_state.t9SelectedSuggestion - 1;
    }
}

/**
 * @brief Valide la suggestion sélectionnée
 */
void multitap_t9_accept_suggestion(void)
{
    if (multitap_state.t9SuggestionCount == 0) return;
    
    const char* word = multitap_state.t9Suggestions[multitap_state.t9SelectedSuggestion];
    
    // Supprimer la séquence en cours (remplacer par le mot)
    // D'abord, revenir au début de la séquence
    for (uint8_t i = 0; i < multitap_state.t9SequenceLength; i++)
    {
        multitap_delete_char();
    }
    
    // Insérer le mot complet
    uint8_t wordLen = strlen(word);
    for (uint8_t i = 0; i < wordLen; i++)
    {
        multitap_insert_char(word[i]);
    }
    
    // Réinitialiser la séquence T9
    multitap_state.t9SequenceLength = 0;
    multitap_state.t9SuggestionCount = 0;
    multitap_state.isEditing = false;
    
    MULTITAP_DEBUG("T9 accepté: '%s'\n", word);
}

/**
 * @brief Charge le dictionnaire (simulé)
 */
bool multitap_t9_load_dictionary(void)
{
    MULTITAP_DEBUG("Chargement dictionnaire T9...\n");
    // TODO : Charger depuis la Flash
    return false;  // Pas encore implémenté
}

/**
 * @brief Sauvegarde le dictionnaire (simulé)
 */
bool multitap_t9_save_dictionary(void)
{
    MULTITAP_DEBUG("Sauvegarde dictionnaire T9...\n");
    // TODO : Sauvegarder en Flash
    return false;  // Pas encore implémenté
}

// ============================================================
// SECTION 6 : FONCTIONS DE LECTURE
// ============================================================

/**
 * @brief Récupère le texte saisi
 */
const char* multitap_get_text(void)
{
    return multitap_state.textBuffer;
}

/**
 * @brief Récupère la longueur du texte
 */
uint16_t multitap_get_length(void)
{
    return multitap_state.bufferLength;
}

/**
 * @brief Récupère la position du curseur
 */
uint16_t multitap_get_cursor(void)
{
    return multitap_state.cursorPosition;
}

/**
 * @brief Récupère le caractère en prévisualisation
 */
char multitap_get_preview_char(void)
{
    return multitap_state.previewChar;
}

/**
 * @brief Récupère le nombre de suggestions T9
 */
uint8_t multitap_t9_get_suggestion_count(void)
{
    return multitap_state.t9SuggestionCount;
}

/**
 * @brief Récupère une suggestion T9
 */
const char* multitap_t9_get_suggestion(uint8_t index)
{
    if (index >= multitap_state.t9SuggestionCount) return NULL;
    return multitap_state.t9Suggestions[index];
}

/**
 * @brief Vérifie si en cours d'édition
 */
bool multitap_is_editing(void)
{
    return multitap_state.isEditing;
}

// ============================================================
// SECTION 7 : CONFIGURATION
// ============================================================

/**
 * @brief Définit le timeout Multitap
 */
void multitap_set_timeout(uint16_t timeoutMs)
{
    if (timeoutMs < MULTITAP_MIN_TIMEOUT_MS) timeoutMs = MULTITAP_MIN_TIMEOUT_MS;
    if (timeoutMs > MULTITAP_MAX_TIMEOUT_MS) timeoutMs = MULTITAP_MAX_TIMEOUT_MS;
    multitap_timeout_ms = timeoutMs;
}

/**
 * @brief Vérifie et applique le timeout
 */
void multitap_check_timeout(void)
{
    if (multitap_state.isEditing)
    {
        uint32_t elapsed = HAL_GetTick() - multitap_state.lastPressTime;
        
        if (elapsed > multitap_timeout_ms)
        {
            multitap_commit_char();
            MULTITAP_DEBUG("Timeout Multitap, caractère validé\n");
        }
    }
}

// ============================================================
// SECTION 8 : DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état
 */
void multitap_print_state(void)
{
    printf("\n═══ ÉTAT MULTITAP ═══\n");
    printf("Mode        : %s\n", MULTITAP_MODE_NAMES[multitap_state.mode]);
    printf("T9          : %s\n", multitap_state.t9Enabled ? "Activé" : "Désactivé");
    printf("Édition     : %s\n", multitap_state.isEditing ? "Oui" : "Non");
    printf("Buffer      : \"%s\"\n", multitap_state.textBuffer);
    printf("Longueur    : %d\n", multitap_state.bufferLength);
    printf("Curseur     : %d\n", multitap_state.cursorPosition);
    printf("Preview     : '%c'\n", multitap_state.previewChar ? multitap_state.previewChar : ' ');
    printf("Caractères  : %lu\n", (unsigned long)multitap_state.totalCharacters);
    printf("Mots        : %lu\n", (unsigned long)multitap_state.totalWords);
    printf("══════════════════════\n\n");
}

/**
 * @brief Affiche le buffer
 */
void multitap_print_buffer(void)
{
    printf("Texte : \"%s\"\n", multitap_state.textBuffer);
    printf("        ");
    for (uint16_t i = 0; i < multitap_state.cursorPosition; i++) printf(" ");
    printf("^\n");
}

/**
 * @brief Affiche le dictionnaire T9
 */
void multitap_t9_print_dictionary(void)
{
    printf("\n═══ DICTIONNAIRE T9 (%d mots) ═══\n", t9_dictionary_count);
    
    for (uint16_t i = 0; i < t9_dictionary_count && i < 50; i++)
    {
        printf("  %-20s (priorité: %d)\n", 
               t9_dictionary[i].word, 
               t9_dictionary[i].priority);
    }
    
    if (t9_dictionary_count > 50)
    {
        printf("  ... et %d autres mots\n", t9_dictionary_count - 50);
    }
    printf("══════════════════════════════\n\n");
}

/**
 * @brief Affiche les statistiques
 */
void multitap_print_statistics(void)
{
    printf("\n═══ STATISTIQUES SAISIE ═══\n");
    printf("Caractères  : %lu\n", (unsigned long)multitap_state.totalCharacters);
    printf("Mots        : %lu\n", (unsigned long)multitap_state.totalWords);
    printf("Dictionnaire: %d mots\n", t9_dictionary_count);
    printf("════════════════════════════\n\n");
}

/**
 * @brief Auto-test
 */
bool multitap_self_test(void)
{
    MULTITAP_DEBUG("Auto-test...\n");
    
    // Réinitialiser
    multitap_reset();
    
    // Test 1 : Saisie "abc" sur la touche 2
    multitap_set_mode(MULTITAP_MODE_abc);
    multitap_process_key(KEY_2);  // a
    multitap_process_key(KEY_2);  // b
    multitap_process_key(KEY_2);  // c
    
    if (multitap_state.previewChar != 'c')
    {
        MULTITAP_DEBUG("Échec test 1\n");
        return false;
    }
    
    // Test 2 : Mode chiffres
    multitap_set_mode(MULTITAP_MODE_123);
    multitap_process_key(KEY_5);
    
    if (multitap_state.textBuffer[0] != '5')
    {
        MULTITAP_DEBUG("Échec test 2\n");
        return false;
    }
    
    // Réinitialiser
    multitap_reset();
    
    MULTITAP_DEBUG("Auto-test OK\n");
    return true;
}