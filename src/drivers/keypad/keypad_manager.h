/**
 * @file keypad_manager.h
 * @brief Gestionnaire haut niveau du clavier
 * 
 * Ce fichier unifie tous les modules du clavier :
 * - keypad_matrix : scan matériel de la matrice
 * - keypad_debounce : anti-rebond des touches
 * - keypad_multitap : saisie de texte
 * 
 * Il fournit une API simple pour l'application :
 * - Initialisation automatique de tous les sous-systèmes
 * - Callbacks pour les événements clavier
 * - Gestion des touches spéciales (Appel, Raccrocher, Lampe...)
 * - Modes de saisie et traitement du texte
 * - Contrôle du rétroéclairage
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef KEYPAD_MANAGER_H
#define KEYPAD_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "keypad_matrix.h"
#include "keypad_debounce.h"
#include "keypad_multitap.h"
#include "../../config.h"
#include "../../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du gestionnaire de clavier */
#define KEYPAD_MANAGER_VERSION          "1.0.0"

/** @brief Intervalle de scan par défaut (ms) */
#define KEYPAD_SCAN_INTERVAL_MS         10

/** @brief Timeout d'inactivité du rétroéclairage (secondes) */
#define KEYPAD_BACKLIGHT_TIMEOUT_S      10

/** @brief Nombre maximal de callbacks */
#define KEYPAD_MAX_CALLBACKS            5

// ============================================================
// SECTION 2 : TYPES DE CALLBACKS
// ============================================================

/**
 * @brief Type de callback pour les événements clavier
 * @param key Touche concernée
 * @param event Type d'événement
 */
typedef void (*KeypadCallback)(KeyCode key, KeyEvent event);

/**
 * @brief Type de callback pour la saisie de texte
 * @param text Texte saisi
 * @param length Longueur du texte
 */
typedef void (*KeypadTextCallback)(const char* text, uint16_t length);

/**
 * @brief Type de callback pour le changement de mode
 * @param mode Nouveau mode
 */
typedef void (*KeypadModeCallback)(MultitapMode mode);

// ============================================================
// SECTION 3 : CONFIGURATION DU GESTIONNAIRE
// ============================================================

/**
 * @brief Configuration du gestionnaire de clavier
 */
typedef struct {
    // Scan
    uint8_t scanIntervalMs;             // Intervalle de scan
    
    // Anti-rebond
    DebounceMethod debounceMethod;       // Méthode d'anti-rebond
    uint16_t debounceDelayMs;           // Délai d'anti-rebond
    
    // Saisie
    MultitapMode defaultInputMode;      // Mode de saisie par défaut
    uint16_t multitapTimeoutMs;         // Timeout Multitap
    
    // Rétroéclairage
    bool backlightEnabled;              // Rétroéclairage activé
    uint8_t backlightBrightness;        // Luminosité (0-255)
    uint8_t backlightTimeoutS;          // Extinction après (secondes)
    
    // Touches spéciales
    bool enableLongPress;               // Appuis longs
    uint16_t longPressMs;               // Seuil appui long
    
    // Callbacks
    KeypadCallback onKeyEvent;          // Événement clavier
    KeypadTextCallback onTextChanged;   // Texte modifié
    KeypadModeCallback onModeChanged;   // Mode changé
} KeypadManager_Config;

// ============================================================
// SECTION 4 : ÉTAT DU GESTIONNAIRE
// ============================================================

/**
 * @brief État du gestionnaire de clavier
 */
typedef struct {
    bool initialized;                   // Initialisé
    bool scanEnabled;                   // Scan actif
    KeyCode lastKey;                    // Dernière touche
    KeyEvent lastEvent;                 // Dernier événement
    uint32_t totalKeyPresses;           // Nombre total d'appuis
    uint32_t lastActivityTime;          // Dernière activité
    bool backlightOn;                   // Rétroéclairage allumé
    bool lampOn;                        // Lampe torche allumée
    bool muted;                         // Mode muet
    uint8_t volume;                     // Volume (0-100)
} KeypadManager_State;

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise le gestionnaire de clavier
 * 
 * Initialise automatiquement :
 * - La matrice du clavier (GPIO)
 * - L'anti-rebond
 * - Le module Multitap
 * 
 * @param config Configuration (NULL = défaut)
 * @return true si succès
 */
bool keypad_manager_init(const KeypadManager_Config* config);

/**
 * @brief Désinitialise le gestionnaire
 */
void keypad_manager_deinit(void);

/**
 * @brief Vérifie si le gestionnaire est prêt
 * @return true si initialisé
 */
bool keypad_manager_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS DE TRAITEMENT
// ============================================================

/**
 * @brief Traitement périodique (à appeler dans la boucle principale)
 * 
 * Scanne le clavier, applique l'anti-rebond,
 * traite la saisie et vérifie les timeouts.
 */
void keypad_manager_process(void);

/**
 * @brief Active/désactive le scan du clavier
 * @param enable true pour activer
 */
void keypad_manager_scan_enable(bool enable);

/**
 * @brief Force un scan immédiat
 */
void keypad_manager_scan_now(void);

// ============================================================
// SECTION 7 : FONCTIONS DE LECTURE DES TOUCHES
// ============================================================

/**
 * @brief Vérifie si une touche est pressée
 * @param key Code de la touche
 * @return true si pressée
 */
bool keypad_manager_is_pressed(KeyCode key);

/**
 * @brief Vérifie si une touche vient d'être pressée
 * @param key Code de la touche
 * @return true si front montant
 */
bool keypad_manager_just_pressed(KeyCode key);

/**
 * @brief Vérifie si une touche vient d'être relâchée
 * @param key Code de la touche
 * @return true si front descendant
 */
bool keypad_manager_just_released(KeyCode key);

/**
 * @brief Vérifie si une touche est maintenue
 * @param key Code de la touche
 * @return true si appui long
 */
bool keypad_manager_is_held(KeyCode key);

/**
 * @brief Récupère la dernière touche pressée
 * @return Code de la touche
 */
KeyCode keypad_manager_get_last_key(void);

// ============================================================
// SECTION 8 : FONCTIONS DE SAISIE DE TEXTE
// ============================================================

/**
 * @brief Récupère le texte saisi
 * @return Pointeur vers le buffer de texte
 */
const char* keypad_manager_get_text(void);

/**
 * @brief Récupère la longueur du texte
 * @return Nombre de caractères
 */
uint16_t keypad_manager_get_text_length(void);

/**
 * @brief Efface le texte saisi
 */
void keypad_manager_clear_text(void);

/**
 * @brief Définit le mode de saisie
 * @param mode Mode souhaité
 */
void keypad_manager_set_input_mode(MultitapMode mode);

/**
 * @brief Passe au mode de saisie suivant
 */
void keypad_manager_next_input_mode(void);

/**
 * @brief Récupère le mode de saisie actuel
 * @return Mode actuel
 */
MultitapMode keypad_manager_get_input_mode(void);

/**
 * @brief Récupère le caractère en prévisualisation
 * @return Caractère (0 si aucun)
 */
char keypad_manager_get_preview_char(void);

/**
 * @brief Active/désactive le mode T9
 * @param enable true pour activer
 */
void keypad_manager_t9_enable(bool enable);

/**
 * @brief Vérifie si le mode T9 est actif
 * @return true si T9 actif
 */
bool keypad_manager_t9_is_enabled(void);

// ============================================================
// SECTION 9 : FONCTIONS DE CONTRÔLE (TOUCHES SPÉCIALES)
// ============================================================

/**
 * @brief Active/désactive la lampe torche
 */
void keypad_manager_toggle_lamp(void);

/**
 * @brief Vérifie si la lampe est allumée
 * @return true si allumée
 */
bool keypad_manager_is_lamp_on(void);

/**
 * @brief Active/désactive le mode muet
 */
void keypad_manager_toggle_mute(void);

/**
 * @brief Vérifie si le mode muet est actif
 * @return true si muet
 */
bool keypad_manager_is_muted(void);

/**
 * @brief Augmente le volume
 */
void keypad_manager_volume_up(void);

/**
 * @brief Diminue le volume
 */
void keypad_manager_volume_down(void);

/**
 * @brief Récupère le volume actuel
 * @return Volume (0-100)
 */
uint8_t keypad_manager_get_volume(void);

// ============================================================
// SECTION 10 : FONCTIONS DE RÉTROÉCLAIRAGE
// ============================================================

/**
 * @brief Allume le rétroéclairage
 */
void keypad_manager_backlight_on(void);

/**
 * @brief Éteint le rétroéclairage
 */
void keypad_manager_backlight_off(void);

/**
 * @brief Bascule le rétroéclairage
 */
void keypad_manager_backlight_toggle(void);

/**
 * @brief Définit la luminosité
 * @param brightness Luminosité (0-255)
 */
void keypad_manager_backlight_set(uint8_t brightness);

/**
 * @brief Vérifie si le rétroéclairage est allumé
 * @return true si allumé
 */
bool keypad_manager_backlight_is_on(void);

// ============================================================
// SECTION 11 : FONCTIONS DE CALLBACKS
// ============================================================

/**
 * @brief Enregistre un callback pour les événements clavier
 * @param callback Fonction à appeler
 */
void keypad_manager_set_key_callback(KeypadCallback callback);

/**
 * @brief Enregistre un callback pour les changements de texte
 * @param callback Fonction à appeler
 */
void keypad_manager_set_text_callback(KeypadTextCallback callback);

/**
 * @brief Enregistre un callback pour les changements de mode
 * @param callback Fonction à appeler
 */
void keypad_manager_set_mode_callback(KeypadModeCallback callback);

// ============================================================
// SECTION 12 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état complet du gestionnaire
 */
void keypad_manager_print_state(void);

/**
 * @brief Affiche la matrice visuelle du clavier
 */
void keypad_manager_print_matrix(void);

/**
 * @brief Affiche les statistiques
 */
void keypad_manager_print_statistics(void);

/**
 * @brief Test de fonctionnement
 * @return true si OK
 */
bool keypad_manager_self_test(void);

// ============================================================
// SECTION 13 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Vérifie si une touche d'appel est pressée
 */
#define KEYPAD_CALL_PRESSED()       keypad_manager_just_pressed(KEY_CALL)

/**
 * @brief Vérifie si la touche raccrocher est pressée
 */
#define KEYPAD_END_PRESSED()        keypad_manager_just_pressed(KEY_END)

/**
 * @brief Vérifie si la touche lampe est pressée
 */
#define KEYPAD_LAMP_PRESSED()       keypad_manager_just_pressed(KEY_LAMP)

/**
 * @brief Vérifie si la touche PTT est pressée
 */
#define KEYPAD_PTT_PRESSED()        keypad_manager_is_pressed(KEY_PTT)

// ============================================================
// SECTION 14 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define KEYPAD_MGR_DEBUG(fmt, ...)  printf("[KEYPAD_MGR] " fmt, ##__VA_ARGS__)
#else
    #define KEYPAD_MGR_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // KEYPAD_MANAGER_H