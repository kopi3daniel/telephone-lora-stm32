/**
 * @file screen_dialer.h
 * @brief Écran de numérotation (composeur téléphonique)
 * 
 * Cet écran permet de composer un numéro de téléphone :
 * - Clavier numérique (0-9, *, #)
 * - Zone d'affichage du numéro composé
 * - Bouton Appeler (📞)
 * - Bouton Effacer (⌫)
 * - Bouton Contacts (accès rapide)
 * - Suggestions de contacts (recherche prédictive)
 * 
 * Disposition :
 * ┌─────────────────────────────────────────────────────────────┐
 * │ ← Composeur                                                │
 * ├─────────────────────────────────────────────────────────────┤
 * │                                                             │
 * │              ┌──────────────────────────┐                   │
 * │              │     06 12 34 56 78       │                   │
 * │              └──────────────────────────┘                   │
 * │                                                             │
 * │         ┌──────┐  ┌──────┐  ┌──────┐                       │
 * │         │   1  │  │   2  │  │   3  │                       │
 * │         │      │  │ ABC  │  │ DEF  │                       │
 * │         └──────┘  └──────┘  └──────┘                       │
 * │         ┌──────┐  ┌──────┐  ┌──────┐                       │
 * │         │   4  │  │   5  │  │   6  │                       │
 * │         │ GHI  │  │ JKL  │  │ MNO  │                       │
 * │         └──────┘  └──────┘  └──────┘                       │
 * │         ┌──────┐  ┌──────┐  ┌──────┐                       │
 * │         │   7  │  │   8  │  │   9  │                       │
 * │         │ PQRS │  │ TUV  │  │ WXYZ │                       │
 * │         └──────┘  └──────┘  └──────┘                       │
 * │         ┌──────┐  ┌──────┐  ┌──────┐                       │
 * │         │   *  │  │   0  │  │   #  │                       │
 * │         │      │  │  +   │  │      │                       │
 * │         └──────┘  └──────┘  └──────┘                       │
 * │                                                             │
 * │         ┌─────────────┐    ┌─────────────┐                  │
 * │         │  👤 Contacts │    │  📞 Appeler  │                  │
 * │         └─────────────┘    └─────────────┘                  │
 * │                                                             │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SCREEN_DIALER_H
#define SCREEN_DIALER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "screen_base.h"
#include "../ui/ui_widgets.h"
#include "../services/phone_service.h"
#include "../services/contact_service.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Nom de l'écran */
#define SCREEN_DIALER_NAME              "DialerScreen"

/** @brief Longueur maximale d'un numéro */
#define DIALER_MAX_DIGITS               20

/** @brief Nombre de touches du clavier */
#define DIALER_KEY_COUNT                12

/** @brief Nombre de suggestions de contacts */
#define DIALER_MAX_SUGGESTIONS          5

// ============================================================
// SECTION 2 : ÉTAT DU COMPOSEUR
// ============================================================

/**
 * @brief État spécifique au composeur
 */
typedef struct {
    // --- Numéro en cours ---
    char phoneNumber[DIALER_MAX_DIGITS + 1];  // Numéro composé
    uint8_t digitCount;                       // Nombre de chiffres
    
    // --- Widgets ---
    UILabel* numberLabel;               // Affichage du numéro
    UIButton* digitKeys[DIALER_KEY_COUNT];  // Touches 0-9, *, #
    UIButton* btnCall;                  // Bouton Appeler
    UIButton* btnDelete;                // Bouton Effacer
    UIButton* btnContacts;              // Bouton Contacts
    UILabel* suggestionLabels[DIALER_MAX_SUGGESTIONS];  // Suggestions
    
    // --- État ---
    bool showSuggestions;               // Afficher les suggestions
    Contact* suggestedContacts[DIALER_MAX_SUGGESTIONS];
    uint8_t suggestionCount;
    
} DialerScreenState;

// ============================================================
// SECTION 3 : DÉFINITION DES TOUCHES
// ============================================================

/**
 * @brief Structure d'une touche du composeur
 */
typedef struct {
    char digit;                     // Chiffre (0-9, *, #)
    const char* label;              // Texte principal
    const char* sublabel;           // Sous-texte (lettres)
    uint16_t x, y;                  // Position
    uint16_t w, h;                  // Taille
} DialerKey;

/** @brief Disposition des touches du composeur */
static const DialerKey DIALER_KEYS[DIALER_KEY_COUNT] = {
    {'1', "1", "",       20, 120, 85, 60},
    {'2', "2", "ABC",   110, 120, 85, 60},
    {'3', "3", "DEF",   200, 120, 85, 60},
    {'4', "4", "GHI",    20, 190, 85, 60},
    {'5', "5", "JKL",   110, 190, 85, 60},
    {'6', "6", "MNO",   200, 190, 85, 60},
    {'7', "7", "PQRS",   20, 260, 85, 60},
    {'8', "8", "TUV",   110, 260, 85, 60},
    {'9', "9", "WXYZ",  200, 260, 85, 60},
    {'*', "*", "",       20, 330, 85, 60},
    {'0', "0", "+",     110, 330, 85, 60},
    {'#', "#", "",      200, 330, 85, 60},
};

// ============================================================
// SECTION 4 : FONCTIONS DE CRÉATION
// ============================================================

ScreenBase* screen_dialer_create(void);
void screen_dialer_init_widgets(ScreenBase* screen);
void screen_dialer_add_digit(ScreenBase* screen, char digit);
void screen_dialer_delete_digit(ScreenBase* screen);
void screen_dialer_clear(ScreenBase* screen);
void screen_dialer_make_call(ScreenBase* screen);

// ============================================================
// SECTION 5 : FONCTIONS DE SUGGESTIONS
// ============================================================

void screen_dialer_update_suggestions(ScreenBase* screen);
void screen_dialer_select_suggestion(ScreenBase* screen, uint8_t index);

// ============================================================
// SECTION 6 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define DIALER_DEBUG(fmt, ...)      printf("[DIALER] " fmt, ##__VA_ARGS__)
#else
    #define DIALER_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 7 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SCREEN_DIALER_H