/**
 * @file keypad_manager.cpp
 * @brief Implémentation du gestionnaire haut niveau du clavier
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans keypad_manager.h.
 * 
 * Il unifie les modules :
 * - keypad_matrix : scan matériel
 * - keypad_debounce : anti-rebond
 * - keypad_multitap : saisie de texte
 * 
 * Et ajoute la logique métier :
 * - Interprétation des touches spéciales
 * - Gestion du rétroéclairage
 * - Gestion de la lampe torche
 * - Gestion du volume et du mode muet
 * - Callbacks utilisateur
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "keypad_manager.h"
#include "../display/display_manager.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Configuration du gestionnaire */
static KeypadManager_Config keypad_config = {
    .scanIntervalMs = KEYPAD_SCAN_INTERVAL_MS,
    .debounceMethod = DEBOUNCE_METHOD_DELAY,
    .debounceDelayMs = DEBOUNCE_DEFAULT_DELAY_MS,
    .defaultInputMode = MULTITAP_MODE_abc,
    .multitapTimeoutMs = MULTITAP_DEFAULT_TIMEOUT_MS,
    .backlightEnabled = true,
    .backlightBrightness = 128,
    .backlightTimeoutS = KEYPAD_BACKLIGHT_TIMEOUT_S,
    .enableLongPress = true,
    .longPressMs = 500,
    .onKeyEvent = NULL,
    .onTextChanged = NULL,
    .onModeChanged = NULL
};

/** @brief État du gestionnaire */
static KeypadManager_State keypad_state = {
    .initialized = false,
    .scanEnabled = true,
    .lastKey = KEY_NONE,
    .lastEvent = KEY_EVENT_NONE,
    .totalKeyPresses = 0,
    .lastActivityTime = 0,
    .backlightOn = false,
    .lampOn = false,
    .muted = false,
    .volume = 80
};

/** @brief Callbacks enregistrés */
static KeypadCallback key_callbacks[KEYPAD_MAX_CALLBACKS];
static uint8_t key_callback_count = 0;

static KeypadTextCallback text_callback = NULL;
static KeypadModeCallback mode_callback = NULL;

/** @brief Compteur pour le rétroéclairage */
static uint32_t backlight_timer = 0;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le gestionnaire de clavier
 */
bool keypad_manager_init(const KeypadManager_Config* config)
{
    KEYPAD_MGR_DEBUG("Initialisation du gestionnaire de clavier...\n");
    
    // Sauvegarder la configuration
    if (config != NULL)
    {
        memcpy(&keypad_config, config, sizeof(KeypadManager_Config));
        KEYPAD_MGR_DEBUG("Configuration personnalisée appliquée\n");
    }
    
    // 1. Initialiser la matrice du clavier
    keypad_matrix_init();
    keypad_matrix_set_scan_interval(keypad_config.scanIntervalMs);
    keypad_matrix_set_debounce(keypad_config.debounceDelayMs);
    keypad_matrix_set_long_press(keypad_config.longPressMs);
    keypad_matrix_set_repeat(true, 100);  // Répétition toutes les 100ms
    
    // 2. Initialiser l'anti-rebond
    DebounceConfig debounceConfig = {
        .method = keypad_config.debounceMethod,
        .delayMs = keypad_config.debounceDelayMs,
        .sampleCount = DEBOUNCE_DEFAULT_SAMPLES,
        .enableMetrics = false
    };
    debounce_init(&debounceConfig);
    
    // 3. Initialiser le module Multitap
    multitap_init();
    multitap_set_mode(keypad_config.defaultInputMode);
    multitap_set_timeout(keypad_config.multitapTimeoutMs);
    
    // 4. Configurer le callback interne de la matrice
    keypad_matrix_set_callback(keypad_internal_event_handler);
    
    // 5. Initialiser le rétroéclairage
    if (keypad_config.backlightEnabled)
    {
        keypad_manager_backlight_on();
        keypad_manager_backlight_set(keypad_config.backlightBrightness);
    }
    
    // 6. Initialiser l'état
    keypad_state.initialized = true;
    keypad_state.scanEnabled = true;
    keypad_state.lastActivityTime = HAL_GetTick();
    keypad_state.lampOn = false;
    keypad_state.muted = false;
    
    // 7. Initialiser les callbacks
    key_callback_count = 0;
    if (keypad_config.onKeyEvent != NULL)
    {
        keypad_manager_set_key_callback(keypad_config.onKeyEvent);
    }
    if (keypad_config.onTextChanged != NULL)
    {
        keypad_manager_set_text_callback(keypad_config.onTextChanged);
    }
    if (keypad_config.onModeChanged != NULL)
    {
        keypad_manager_set_mode_callback(keypad_config.onModeChanged);
    }
    
    KEYPAD_MGR_DEBUG("Gestionnaire de clavier initialisé\n");
    
    return true;
}

/**
 * @brief Désinitialise le gestionnaire
 */
void keypad_manager_deinit(void)
{
    keypad_manager_backlight_off();
    keypad_state.initialized = false;
    keypad_state.scanEnabled = false;
}

/**
 * @brief Vérifie si le gestionnaire est prêt
 */
bool keypad_manager_is_ready(void)
{
    return keypad_state.initialized;
}

// ============================================================
// SECTION 2 : TRAITEMENT PÉRIODIQUE
// ============================================================

/**
 * @brief Traitement périodique (appelé dans la boucle principale)
 */
void keypad_manager_process(void)
{
    if (!keypad_state.initialized) return;
    if (!keypad_state.scanEnabled) return;
    
    // Scanner la matrice
    keypad_matrix_scan();
    
    // Vérifier le timeout Multitap
    multitap_check_timeout();
    
    // Vérifier le timeout du rétroéclairage
    if (keypad_config.backlightEnabled && keypad_state.backlightOn)
    {
        uint32_t elapsed = HAL_GetTick() - keypad_state.lastActivityTime;
        
        if (elapsed > (keypad_config.backlightTimeoutS * 1000))
        {
            keypad_manager_backlight_off();
            KEYPAD_MGR_DEBUG("Rétroéclairage éteint (timeout)\n");
        }
    }
    
    // Traiter les événements en attente
    KeypadEvent event;
    while (keypad_get_event(&event))
    {
        process_keypad_event(&event);
    }
}

/**
 * @brief Active/désactive le scan
 */
void keypad_manager_scan_enable(bool enable)
{
    keypad_state.scanEnabled = enable;
}

/**
 * @brief Force un scan immédiat
 */
void keypad_manager_scan_now(void)
{
    keypad_matrix_scan();
}

// ============================================================
// SECTION 3 : GESTIONNAIRE D'ÉVÉNEMENTS INTERNE
// ============================================================

/**
 * @brief Callback interne appelé par la matrice lors d'un événement
 */
static void keypad_internal_event_handler(const KeypadEvent* event)
{
    if (event == NULL) return;
    
    // Mettre à jour l'activité
    keypad_state.lastActivityTime = HAL_GetTick();
    keypad_state.lastKey = event->key;
    keypad_state.lastEvent = event->event;
    
    // Réactiver le rétroéclairage si nécessaire
    if (keypad_config.backlightEnabled && !keypad_state.backlightOn)
    {
        keypad_manager_backlight_on();
    }
    
    // Incrémenter le compteur
    if (event->event == KEY_EVENT_PRESS)
    {
        keypad_state.totalKeyPresses++;
    }
    
    // Traiter l'événement
    process_keypad_event(event);
}

/**
 * @brief Traite un événement clavier
 */
static void process_keypad_event(const KeypadEvent* event)
{
    if (event == NULL) return;
    
    // Traiter les touches spéciales d'abord
    if (handle_special_key(event))
    {
        return;  // Touche spéciale traitée
    }
    
    // Traiter les touches de saisie
    if (event->event == KEY_EVENT_PRESS || event->event == KEY_EVENT_REPEAT)
    {
        if (MULTITAP_IS_INPUT_KEY(event->key))
        {
            // Touche de saisie → traiter avec Multitap
            char c = multitap_process_key(event->key);
            
            if (c != 0)
            {
                // Caractère validé
                KEYPAD_MGR_DEBUG("Caractère saisi: '%c'\n", c);
                
                // Notifier le changement de texte
                if (text_callback)
                {
                    text_callback(multitap_get_text(), multitap_get_length());
                }
            }
            
            // Notifier l'événement clavier
            notify_key_callbacks(event->key, event->event);
            return;
        }
    }
    
    // Notifier les autres touches
    notify_key_callbacks(event->key, event->event);
}

/**
 * @brief Traite les touches spéciales
 * @return true si la touche a été traitée
 */
static bool handle_special_key(const KeypadEvent* event)
{
    if (event->event != KEY_EVENT_PRESS && event->event != KEY_EVENT_REPEAT)
    {
        return false;
    }
    
    switch (event->key)
    {
        case KEY_LAMP:
            keypad_manager_toggle_lamp();
            return true;
            
        case KEY_MUTE:
            keypad_manager_toggle_mute();
            return true;
            
        case KEY_VOL:
            // Bascule entre volume up et down
            if (keypad_state.volume < 50)
                keypad_manager_volume_up();
            else
                keypad_manager_volume_down();
            return true;
            
        case KEY_SHIFT:
            keypad_manager_next_input_mode();
            return true;
            
        case KEY_BACK:
            // En mode saisie : effacer un caractère
            multitap_commit_char();  // Valider d'abord le caractère en cours
            multitap_delete_char();
            if (text_callback)
            {
                text_callback(multitap_get_text(), multitap_get_length());
            }
            return true;
            
        case KEY_UP:
            // Navigation T9 : suggestion suivante
            if (keypad_manager_t9_is_enabled())
            {
                multitap_t9_next_suggestion();
            }
            return true;
            
        case KEY_DOWN:
            // Navigation T9 : suggestion précédente
            if (keypad_manager_t9_is_enabled())
            {
                multitap_t9_prev_suggestion();
            }
            return true;
            
        case KEY_OK:
            // Valider la suggestion T9
            if (keypad_manager_t9_is_enabled())
            {
                multitap_t9_accept_suggestion();
                if (text_callback)
                {
                    text_callback(multitap_get_text(), multitap_get_length());
                }
            }
            return true;
            
        case KEY_0:
            // Espace (en mode saisie)
            if (event->event == KEY_EVENT_PRESS)
            {
                multitap_insert_space();
                if (text_callback)
                {
                    text_callback(multitap_get_text(), multitap_get_length());
                }
            }
            return true;
            
        default:
            return false;  // Pas une touche spéciale
    }
}

/**
 * @brief Notifie tous les callbacks enregistrés
 */
static void notify_key_callbacks(KeyCode key, KeyEvent event)
{
    for (uint8_t i = 0; i < key_callback_count; i++)
    {
        if (key_callbacks[i] != NULL)
        {
            key_callbacks[i](key, event);
        }
    }
}

// ============================================================
// SECTION 4 : FONCTIONS DE LECTURE DES TOUCHES
// ============================================================

/**
 * @brief Vérifie si une touche est pressée
 */
bool keypad_manager_is_pressed(KeyCode key)
{
    return keypad_is_pressed(key);
}

/**
 * @brief Vérifie si une touche vient d'être pressée
 */
bool keypad_manager_just_pressed(KeyCode key)
{
    return keypad_was_just_pressed(key);
}

/**
 * @brief Vérifie si une touche vient d'être relâchée
 */
bool keypad_manager_just_released(KeyCode key)
{
    return keypad_was_just_released(key);
}

/**
 * @brief Vérifie si une touche est maintenue
 */
bool keypad_manager_is_held(KeyCode key)
{
    return keypad_is_held(key);
}

/**
 * @brief Récupère la dernière touche pressée
 */
KeyCode keypad_manager_get_last_key(void)
{
    return keypad_state.lastKey;
}

// ============================================================
// SECTION 5 : FONCTIONS DE SAISIE DE TEXTE
// ============================================================

/**
 * @brief Récupère le texte saisi
 */
const char* keypad_manager_get_text(void)
{
    return multitap_get_text();
}

/**
 * @brief Récupère la longueur du texte
 */
uint16_t keypad_manager_get_text_length(void)
{
    return multitap_get_length();
}

/**
 * @brief Efface le texte
 */
void keypad_manager_clear_text(void)
{
    multitap_clear_buffer();
}

/**
 * @brief Définit le mode de saisie
 */
void keypad_manager_set_input_mode(MultitapMode mode)
{
    multitap_set_mode(mode);
    
    if (mode_callback)
    {
        mode_callback(mode);
    }
}

/**
 * @brief Passe au mode suivant
 */
void keypad_manager_next_input_mode(void)
{
    multitap_next_mode();
    
    if (mode_callback)
    {
        mode_callback(multitap_get_mode());
    }
}

/**
 * @brief Récupère le mode actuel
 */
MultitapMode keypad_manager_get_input_mode(void)
{
    return multitap_get_mode();
}

/**
 * @brief Récupère le caractère en prévisualisation
 */
char keypad_manager_get_preview_char(void)
{
    return multitap_get_preview_char();
}

/**
 * @brief Active/désactive le T9
 */
void keypad_manager_t9_enable(bool enable)
{
    if (enable)
        multitap_set_mode(MULTITAP_MODE_T9_abc);
    else
        multitap_set_mode(MULTITAP_MODE_abc);
}

/**
 * @brief Vérifie si le T9 est actif
 */
bool keypad_manager_t9_is_enabled(void)
{
    MultitapMode mode = multitap_get_mode();
    return (mode == MULTITAP_MODE_T9_ABC || mode == MULTITAP_MODE_T9_abc);
}

// ============================================================
// SECTION 6 : CONTRÔLE (LAMPE, MUET, VOLUME)
// ============================================================

/**
 * @brief Bascule la lampe torche
 */
void keypad_manager_toggle_lamp(void)
{
    keypad_state.lampOn = !keypad_state.lampOn;
    
    if (keypad_state.lampOn)
    {
        HAL_GPIO_WritePin(LAMP_PORT, LAMP_PIN, GPIO_PIN_SET);
        KEYPAD_MGR_DEBUG("Lampe allumée\n");
    }
    else
    {
        HAL_GPIO_WritePin(LAMP_PORT, LAMP_PIN, GPIO_PIN_RESET);
        KEYPAD_MGR_DEBUG("Lampe éteinte\n");
    }
}

/**
 * @brief Vérifie si la lampe est allumée
 */
bool keypad_manager_is_lamp_on(void)
{
    return keypad_state.lampOn;
}

/**
 * @brief Bascule le mode muet
 */
void keypad_manager_toggle_mute(void)
{
    keypad_state.muted = !keypad_state.muted;
    KEYPAD_MGR_DEBUG("Mode muet: %s\n", keypad_state.muted ? "ON" : "OFF");
}

/**
 * @brief Vérifie si le mode muet est actif
 */
bool keypad_manager_is_muted(void)
{
    return keypad_state.muted;
}

/**
 * @brief Augmente le volume
 */
void keypad_manager_volume_up(void)
{
    if (keypad_state.volume < 100)
    {
        keypad_state.volume += 10;
        if (keypad_state.volume > 100) keypad_state.volume = 100;
        KEYPAD_MGR_DEBUG("Volume: %d%%\n", keypad_state.volume);
    }
}

/**
 * @brief Diminue le volume
 */
void keypad_manager_volume_down(void)
{
    if (keypad_state.volume > 0)
    {
        keypad_state.volume -= 10;
        if (keypad_state.volume < 0) keypad_state.volume = 0;
        KEYPAD_MGR_DEBUG("Volume: %d%%\n", keypad_state.volume);
    }
}

/**
 * @brief Récupère le volume
 */
uint8_t keypad_manager_get_volume(void)
{
    return keypad_state.volume;
}

// ============================================================
// SECTION 7 : RÉTROÉCLAIRAGE
// ============================================================

/**
 * @brief Allume le rétroéclairage
 */
void keypad_manager_backlight_on(void)
{
    if (!keypad_config.backlightEnabled) return;
    
    keypad_state.backlightOn = true;
    keypad_state.lastActivityTime = HAL_GetTick();
    
    // Activer le PWM pour le rétroéclairage
    __HAL_TIM_SET_COMPARE(&htim4, KEYPAD_BL_TIMER_CHANNEL, keypad_config.backlightBrightness);
    
    KEYPAD_MGR_DEBUG("Rétroéclairage allumé\n");
}

/**
 * @brief Éteint le rétroéclairage
 */
void keypad_manager_backlight_off(void)
{
    keypad_state.backlightOn = false;
    
    // Désactiver le PWM
    __HAL_TIM_SET_COMPARE(&htim4, KEYPAD_BL_TIMER_CHANNEL, 0);
    
    KEYPAD_MGR_DEBUG("Rétroéclairage éteint\n");
}

/**
 * @brief Bascule le rétroéclairage
 */
void keypad_manager_backlight_toggle(void)
{
    if (keypad_state.backlightOn)
        keypad_manager_backlight_off();
    else
        keypad_manager_backlight_on();
}

/**
 * @brief Définit la luminosité
 */
void keypad_manager_backlight_set(uint8_t brightness)
{
    keypad_config.backlightBrightness = brightness;
    
    if (keypad_state.backlightOn)
    {
        __HAL_TIM_SET_COMPARE(&htim4, KEYPAD_BL_TIMER_CHANNEL, brightness);
    }
}

/**
 * @brief Vérifie si le rétroéclairage est allumé
 */
bool keypad_manager_backlight_is_on(void)
{
    return keypad_state.backlightOn;
}

// ============================================================
// SECTION 8 : CALLBACKS
// ============================================================

/**
 * @brief Enregistre un callback clavier
 */
void keypad_manager_set_key_callback(KeypadCallback callback)
{
    if (key_callback_count < KEYPAD_MAX_CALLBACKS && callback != NULL)
    {
        key_callbacks[key_callback_count++] = callback;
    }
}

/**
 * @brief Enregistre un callback texte
 */
void keypad_manager_set_text_callback(KeypadTextCallback callback)
{
    text_callback = callback;
}

/**
 * @brief Enregistre un callback mode
 */
void keypad_manager_set_mode_callback(KeypadModeCallback callback)
{
    mode_callback = callback;
}

// ============================================================
// SECTION 9 : DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état complet
 */
void keypad_manager_print_state(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║     ÉTAT DU GESTIONNAIRE DE CLAVIER           ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Initialisé    : %-31s ║\n", keypad_state.initialized ? "Oui" : "Non");
    printf("║ Scan actif    : %-31s ║\n", keypad_state.scanEnabled ? "Oui" : "Non");
    printf("║ Dernière touche: %-29s ║\n", keypad_key_name(keypad_state.lastKey));
    printf("║ Total appuis  : %-31lu ║\n", (unsigned long)keypad_state.totalKeyPresses);
    printf("║ Mode saisie   : %-31s ║\n", MULTITAP_MODE_NAMES[multitap_get_mode()]);
    printf("║ T9            : %-31s ║\n", keypad_manager_t9_is_enabled() ? "Activé" : "Désactivé");
    printf("║ Texte         : \"%s\"\n", multitap_get_text());
    printf("║ Longueur texte: %-31d ║\n", multitap_get_length());
    printf("║ Lampe         : %-31s ║\n", keypad_state.lampOn ? "Allumée" : "Éteinte");
    printf("║ Muet          : %-31s ║\n", keypad_state.muted ? "Oui" : "Non");
    printf("║ Volume        : %-31d ║\n", keypad_state.volume);
    printf("║ Rétroéclairage: %-31s ║\n", keypad_state.backlightOn ? "Allumé" : "Éteint");
    printf("║ Luminosité    : %-31d ║\n", keypad_config.backlightBrightness);
    printf("║ Callbacks     : %-31d ║\n", key_callback_count);
    printf("╚══════════════════════════════════════════════╝\n\n");
}

/**
 * @brief Affiche la matrice visuelle
 */
void keypad_manager_print_matrix(void)
{
    keypad_matrix_print_visual();
}

/**
 * @brief Affiche les statistiques
 */
void keypad_manager_print_statistics(void)
{
    printf("\n═══ STATISTIQUES CLAVIER ═══\n");
    printf("Appuis totaux    : %lu\n", (unsigned long)keypad_state.totalKeyPresses);
    printf("Caractères saisis: %lu\n", (unsigned long)multitap_state.totalCharacters);
    printf("Mots saisis      : %lu\n", (unsigned long)multitap_state.totalWords);
    printf("Mode actuel      : %s\n", MULTITAP_MODE_NAMES[multitap_get_mode()]);
    printf("══════════════════════════════\n\n");
}

/**
 * @brief Test de fonctionnement
 */
bool keypad_manager_self_test(void)
{
    KEYPAD_MGR_DEBUG("Auto-test du gestionnaire...\n");
    
    if (!keypad_state.initialized)
    {
        KEYPAD_MGR_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Tester le rétroéclairage
    keypad_manager_backlight_on();
    HAL_Delay(200);
    keypad_manager_backlight_off();
    HAL_Delay(200);
    keypad_manager_backlight_on();
    
    // Tester la lampe
    keypad_manager_toggle_lamp();
    HAL_Delay(200);
    keypad_manager_toggle_lamp();
    
    // Tester le mode muet
    keypad_manager_toggle_mute();
    HAL_Delay(100);
    keypad_manager_toggle_mute();
    
    // Tester les modes de saisie
    keypad_manager_set_input_mode(MULTITAP_MODE_abc);
    keypad_manager_set_input_mode(MULTITAP_MODE_123);
    keypad_manager_set_input_mode(MULTITAP_MODE_abc);
    
    KEYPAD_MGR_DEBUG("Auto-test OK\n");
    return true;
}