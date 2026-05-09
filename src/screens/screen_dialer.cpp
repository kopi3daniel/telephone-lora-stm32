/**
 * @file screen_dialer.cpp
 * @brief Implémentation de l'écran de numérotation
 * 
 * Fonctionnalités :
 * - Clavier numérique 3×4 avec retour visuel
 * - Affichage du numéro composé avec formatage
 * - Suggestions de contacts (recherche prédictive)
 * - Bouton Appeler avec validation
 * - Bouton Effacer (backspace)
 * - Accès rapide aux contacts
 * - Gestion des touches physiques du clavier
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "screen_dialer.h"
#include "screen_call.h"
#include "screen_contacts.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_icons.h"
#include "../ui/ui_fonts.h"
#include "../ui/ui_animations.h"
#include "../services/phone_service.h"
#include "../services/contact_service.h"
#include "../services/call_log_service.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// ============================================================
// VARIABLES
// ============================================================

/** @brief État du composeur */
static DialerScreenState dialerState;

/** @brief Son DTMF activé */
static bool dtmfEnabled = true;

// ============================================================
// CALLBACKS
// ============================================================

/**
 * @brief Callback de création de l'écran
 */
static void dialer_on_create(ScreenBase* screen)
{
    DIALER_DEBUG("Création du composeur\n");
    
    memset(&dialerState, 0, sizeof(DialerScreenState));
    dialerState.showSuggestions = true;
    
    screen_dialer_init_widgets(screen);
}

/**
 * @brief Callback d'entrée dans l'écran
 */
static void dialer_on_enter(ScreenBase* screen)
{
    DIALER_DEBUG("Entrée dans le composeur\n");
    
    // Restaurer le numéro précédent si existant
    if (dialerState.digitCount == 0)
    {
        screen_dialer_clear(screen);
    }
}

/**
 * @brief Callback de pression sur une touche
 */
static void dialer_on_key(ScreenBase* screen, KeyCode key, KeyEvent event)
{
    if (event != KEY_EVENT_PRESS && event != KEY_EVENT_REPEAT) return;
    
    switch (key)
    {
        case KEY_0: case KEY_1: case KEY_2: case KEY_3: case KEY_4:
        case KEY_5: case KEY_6: case KEY_7: case KEY_8: case KEY_9:
            screen_dialer_add_digit(screen, '0' + (key - KEY_0));
            break;
            
        case KEY_STAR:
            screen_dialer_add_digit(screen, '*');
            break;
            
        case KEY_HASH:
            screen_dialer_add_digit(screen, '#');
            break;
            
        case KEY_CALL:
            screen_dialer_make_call(screen);
            break;
            
        case KEY_BACK:
            screen_dialer_delete_digit(screen);
            break;
            
        case KEY_UP:
            // Sélectionner la suggestion précédente
            if (dialerState.suggestionCount > 0)
            {
                // TODO: navigation dans les suggestions
            }
            break;
            
        case KEY_DOWN:
            // Sélectionner la suggestion suivante
            break;
            
        default:
            break;
    }
}

// ============================================================
// CRÉATION DE L'ÉCRAN
// ============================================================

ScreenBase* screen_dialer_create(void)
{
    ScreenBase* screen = screen_create(SCREEN_DIALER_NAME);
    if (screen == NULL) return NULL;
    
    screen->onCreate = dialer_on_create;
    screen->onEnter = dialer_on_enter;
    screen->onKeyPress = dialer_on_key;
    
    strncpy(screen->title, "Composeur", 63);
    
    screen_dialer_init_widgets(screen);
    
    return screen;
}

// ============================================================
// INITIALISATION DES WIDGETS
// ============================================================

void screen_dialer_init_widgets(ScreenBase* screen)
{
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    // --- Affichage du numéro ---
    dialerState.numberLabel = ui_label_create("number", "",
                                               UI_RECT(20, 60, 280, 50));
    ui_label_set_style(dialerState.numberLabel, LABEL_STYLE_TITLE);
    ui_label_set_alignment(dialerState.numberLabel, UI_ALIGN_CENTER);
    dialerState.numberLabel->base.rect.height = 50;
    screen_add_widget(screen, (UIWidget*)dialerState.numberLabel);
    
    // --- Clavier numérique (3×4) ---
    for (uint8_t i = 0; i < DIALER_KEY_COUNT; i++)
    {
        const DialerKey* keyDef = &DIALER_KEYS[i];
        
        char btnName[16];
        snprintf(btnName, sizeof(btnName), "key_%c", keyDef->digit);
        
        UIButton* key = ui_button_create(btnName, keyDef->label,
                                          UI_RECT(keyDef->x, keyDef->y, keyDef->w, keyDef->h));
        ui_button_set_style(key, BUTTON_STYLE_SECONDARY);
        key->base.rect.width = keyDef->w;
        key->base.rect.height = keyDef->h;
        
        // Callback pour chaque touche
        char digit = keyDef->digit;
        key->onClick = [digit](UIButton* btn) {
            // Trouver l'écran parent
            ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
            if (parent)
            {
                screen_dialer_add_digit(parent, digit);
            }
            
            // Jouer le son DTMF
            if (dtmfEnabled && digit >= '0' && digit <= '9')
            {
                audio_manager_play_dtmf(digit, 100);
            }
        };
        
        screen_add_widget(screen, (UIWidget*)key);
        dialerState.digitKeys[i] = key;
    }
    
    // --- Bouton Appeler ---
    dialerState.btnCall = ui_button_create("btnCall", "📞 Appeler",
                                            UI_RECT(170, 410, 130, 50));
    ui_button_set_style(dialerState.btnCall, BUTTON_STYLE_CALL);
    dialerState.btnCall->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_dialer_make_call(parent);
    };
    screen_add_widget(screen, (UIWidget*)dialerState.btnCall);
    
    // --- Bouton Effacer ---
    dialerState.btnDelete = ui_button_create("btnDelete", "⌫",
                                              UI_RECT(20, 410, 60, 50));
    ui_button_set_style(dialerState.btnDelete, BUTTON_STYLE_OUTLINE);
    dialerState.btnDelete->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_dialer_delete_digit(parent);
    };
    screen_add_widget(screen, (UIWidget*)dialerState.btnDelete);
    
    // --- Bouton Contacts ---
    dialerState.btnContacts = ui_button_create("btnContacts", "👤",
                                                UI_RECT(90, 410, 70, 50));
    ui_button_set_style(dialerState.btnContacts, BUTTON_STYLE_SECONDARY);
    dialerState.btnContacts->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent)
        {
            // Ouvrir les contacts en mode sélection
            ScreenBase* contacts = screen_contacts_create();
            if (contacts)
            {
                contacts->result = SCREEN_RESULT_CUSTOM + 1;  // Mode sélection
                ui_push_screen((UIScreen*)contacts);
            }
        }
    };
    screen_add_widget(screen, (UIWidget*)dialerState.btnContacts);
    
    // --- Labels de suggestions (initialement cachés) ---
    for (uint8_t i = 0; i < DIALER_MAX_SUGGESTIONS; i++)
    {
        char name[16];
        snprintf(name, sizeof(name), "sugg_%d", i);
        
        UILabel* sugg = ui_label_create(name, "",
                                         UI_RECT(20, 115 + i * 22, 280, 20));
        ui_label_set_style(sugg, LABEL_STYLE_BODY);
        sugg->base.visible = false;
        
        screen_add_widget(screen, (UIWidget*)sugg);
        dialerState.suggestionLabels[i] = sugg;
    }
    
    DIALER_DEBUG("Widgets du composeur initialisés (%d widgets)\n", screen->widgetCount);
}

// ============================================================
// GESTION DU NUMÉRO
// ============================================================

void screen_dialer_add_digit(ScreenBase* screen, char digit)
{
    if (screen == NULL) return;
    if (dialerState.digitCount >= DIALER_MAX_DIGITS) return;
    
    // Ajouter le chiffre
    dialerState.phoneNumber[dialerState.digitCount++] = digit;
    dialerState.phoneNumber[dialerState.digitCount] = '\0';
    
    // Formater l'affichage (espaces tous les 2 chiffres)
    char formatted[32];
    format_phone_number(dialerState.phoneNumber, formatted, sizeof(formatted));
    
    // Mettre à jour l'affichage
    if (dialerState.numberLabel)
    {
        ui_label_set_text(dialerState.numberLabel, formatted);
    }
    
    // Mettre à jour les suggestions
    screen_dialer_update_suggestions(screen);
    
    // Activer le bouton Appeler
    if (dialerState.btnCall && dialerState.digitCount > 0)
    {
        ui_button_set_enabled(dialerState.btnCall, true);
    }
    
    DIALER_DEBUG("Chiffre ajouté: '%c' → %s\n", digit, dialerState.phoneNumber);
}

void screen_dialer_delete_digit(ScreenBase* screen)
{
    if (screen == NULL) return;
    if (dialerState.digitCount == 0) return;
    
    // Supprimer le dernier chiffre
    dialerState.digitCount--;
    dialerState.phoneNumber[dialerState.digitCount] = '\0';
    
    // Mettre à jour l'affichage
    char formatted[32];
    if (dialerState.digitCount > 0)
    {
        format_phone_number(dialerState.phoneNumber, formatted, sizeof(formatted));
    }
    else
    {
        formatted[0] = '\0';
    }
    
    if (dialerState.numberLabel)
    {
        ui_label_set_text(dialerState.numberLabel, formatted);
    }
    
    // Mettre à jour les suggestions
    screen_dialer_update_suggestions(screen);
    
    // Désactiver le bouton Appeler si vide
    if (dialerState.btnCall && dialerState.digitCount == 0)
    {
        ui_button_set_enabled(dialerState.btnCall, false);
    }
}

void screen_dialer_clear(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    dialerState.digitCount = 0;
    memset(dialerState.phoneNumber, 0, sizeof(dialerState.phoneNumber));
    
    if (dialerState.numberLabel)
    {
        ui_label_set_text(dialerState.numberLabel, "");
    }
    
    // Cacher les suggestions
    for (uint8_t i = 0; i < DIALER_MAX_SUGGESTIONS; i++)
    {
        if (dialerState.suggestionLabels[i])
        {
            dialerState.suggestionLabels[i]->base.visible = false;
        }
    }
    dialerState.suggestionCount = 0;
    
    if (dialerState.btnCall)
    {
        ui_button_set_enabled(dialerState.btnCall, false);
    }
}

void screen_dialer_make_call(ScreenBase* screen)
{
    if (screen == NULL) return;
    if (dialerState.digitCount == 0) return;
    
    DIALER_DEBUG("Appel du numéro : %s\n", dialerState.phoneNumber);
    
    // Lancer l'appel
    if (phone_service_dial(dialerState.phoneNumber))
    {
        // Ouvrir l'écran d'appel
        ScreenBase* callScreen = screen_call_create(dialerState.phoneNumber,
                                                     dialerState.phoneNumber);  // Nom = numéro
        if (callScreen)
        {
            screen_set_transition(callScreen, SCREEN_TRANSITION_SLIDE_UP,
                                  SCREEN_TRANSITION_SLIDE_DOWN, 250);
            ui_push_screen((UIScreen*)callScreen);
        }
        
        // Effacer le numéro pour le prochain appel
        screen_dialer_clear(screen);
    }
    else
    {
        DIALER_DEBUG("Échec de l'appel\n");
        // Afficher une erreur ?
    }
}

// ============================================================
// SUGGESTIONS
// ============================================================

void screen_dialer_update_suggestions(ScreenBase* screen)
{
    if (screen == NULL || !dialerState.showSuggestions) return;
    if (dialerState.digitCount == 0)
    {
        // Afficher les derniers appels si aucun chiffre
        show_recent_calls(screen);
        return;
    }
    
    // Rechercher les contacts correspondants
    uint16_t foundIndices[DIALER_MAX_SUGGESTIONS];
    uint16_t found = contact_service_search(dialerState.phoneNumber, foundIndices, DIALER_MAX_SUGGESTIONS);
    
    dialerState.suggestionCount = found;
    
    for (uint8_t i = 0; i < DIALER_MAX_SUGGESTIONS; i++)
    {
        if (dialerState.suggestionLabels[i])
        {
            if (i < found)
            {
                Contact* contact = contact_service_get(foundIndices[i]);
                if (contact)
                {
                    char suggText[64];
                    snprintf(suggText, sizeof(suggText), "%s  →  %s", contact->name, contact->number);
                    ui_label_set_text(dialerState.suggestionLabels[i], suggText);
                }
                dialerState.suggestionLabels[i]->base.visible = true;
            }
            else
            {
                dialerState.suggestionLabels[i]->base.visible = false;
            }
        }
    }
}

void screen_dialer_select_suggestion(ScreenBase* screen, uint8_t index)
{
    if (screen == NULL) return;
    if (index >= dialerState.suggestionCount) return;
    
    // Récupérer le contact suggéré
    uint16_t foundIndices[DIALER_MAX_SUGGESTIONS];
    uint16_t found = contact_service_search(dialerState.phoneNumber, foundIndices, DIALER_MAX_SUGGESTIONS);
    
    if (index < found)
    {
        Contact* contact = contact_service_get(foundIndices[index]);
        if (contact)
        {
            // Remplacer le numéro par celui du contact
            strncpy(dialerState.phoneNumber, contact->number, DIALER_MAX_DIGITS);
            dialerState.digitCount = strlen(dialerState.phoneNumber);
            
            char formatted[32];
            format_phone_number(dialerState.phoneNumber, formatted, sizeof(formatted));
            ui_label_set_text(dialerState.numberLabel, formatted);
            
            // Lancer l'appel directement
            screen_dialer_make_call(screen);
        }
    }
}

// ============================================================
// FONCTIONS UTILITAIRES
// ============================================================

/**
 * @brief Formate un numéro de téléphone avec des espaces
 */
static void format_phone_number(const char* raw, char* formatted, uint16_t maxLen)
{
    if (raw == NULL || formatted == NULL) return;
    
    uint16_t len = strlen(raw);
    uint16_t outPos = 0;
    
    for (uint16_t i = 0; i < len && outPos < maxLen - 1; i++)
    {
        formatted[outPos++] = raw[i];
        
        // Ajouter un espace tous les 2 chiffres
        if ((i + 1) % 2 == 0 && i < len - 1 && outPos < maxLen - 1)
        {
            formatted[outPos++] = ' ';
        }
    }
    formatted[outPos] = '\0';
}

/**
 * @brief Affiche les derniers appels (quand aucun chiffre n'est saisi)
 */
static void show_recent_calls(ScreenBase* screen)
{
    CallLogEntry recentCalls[DIALER_MAX_SUGGESTIONS];
    uint16_t count = call_log_get_entries(CALL_LOG_TYPE_ALL, recentCalls, DIALER_MAX_SUGGESTIONS);
    
    dialerState.suggestionCount = count;
    
    for (uint8_t i = 0; i < DIALER_MAX_SUGGESTIONS; i++)
    {
        if (dialerState.suggestionLabels[i])
        {
            if (i < count)
            {
                const char* typeStr = (recentCalls[i].type == CALL_LOG_TYPE_INCOMING) ? "←" : "→";
                char suggText[64];
                snprintf(suggText, sizeof(suggText), "%s  %s  %s",
                         typeStr, recentCalls[i].contactName, recentCalls[i].number);
                ui_label_set_text(dialerState.suggestionLabels[i], suggText);
                dialerState.suggestionLabels[i]->base.visible = true;
            }
            else
            {
                dialerState.suggestionLabels[i]->base.visible = false;
            }
        }
    }
}