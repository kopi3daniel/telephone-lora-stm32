/**
 * @file keypad_matrix.cpp
 * @brief Implémentation du driver de la matrice du clavier 6x4
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans keypad_matrix.h.
 * 
 * Il gère :
 * - L'initialisation des broches GPIO
 * - Le scan de la matrice (lecture des lignes/colonnes)
 * - L'anti-rebond (debounce)
 * - La détection des appuis longs
 * - La gestion du mode Multitap
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "keypad_matrix.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État complet du clavier */
static KeypadState keypad_state;

/** @brief Configuration du clavier */
static struct {
    uint8_t scanIntervalMs;         // Intervalle de scan (défaut: 10ms)
    uint16_t debounceMs;            // Temps anti-rebond (défaut: 20ms)
    uint16_t longPressMs;           // Seuil appui long (défaut: 500ms)
    uint16_t repeatMs;              // Intervalle répétition (défaut: 100ms)
    uint16_t multitapTimeoutMs;     // Timeout Multitap (défaut: 1000ms)
    bool enableRepeat;              // Activer la répétition automatique
} keypad_config = {
    .scanIntervalMs = 10,
    .debounceMs = 20,
    .longPressMs = 500,
    .repeatMs = 100,
    .multitapTimeoutMs = 1000,
    .enableRepeat = true
};

/** @brief Callback pour les événements clavier */
static void (*keypad_event_callback)(const KeypadEvent* event) = NULL;

/** @brief Buffer d'événements */
#define KEYPAD_EVENT_BUFFER_SIZE    16
static KeypadEvent event_buffer[KEYPAD_EVENT_BUFFER_SIZE];
static uint8_t event_buffer_head = 0;
static uint8_t event_buffer_tail = 0;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le clavier matriciel
 */
void keypad_matrix_init(void)
{
    KEYPAD_DEBUG("Initialisation du clavier matriciel...\n");
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Activer les horloges des ports
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    
    // --- Configuration des LIGNES (sorties) ---
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    GPIO_InitStruct.Pin = KEYPAD_ROW0_PIN | KEYPAD_ROW1_PIN | KEYPAD_ROW2_PIN |
                          KEYPAD_ROW3_PIN | KEYPAD_ROW4_PIN | KEYPAD_ROW5_PIN;
    HAL_GPIO_Init(KEYPAD_ROW_PORT, &GPIO_InitStruct);
    
    // Mettre toutes les lignes à HIGH au départ
    HAL_GPIO_WritePin(KEYPAD_ROW_PORT, 
                      KEYPAD_ROW0_PIN | KEYPAD_ROW1_PIN | KEYPAD_ROW2_PIN |
                      KEYPAD_ROW3_PIN | KEYPAD_ROW4_PIN | KEYPAD_ROW5_PIN, 
                      GPIO_PIN_SET);
    
    // --- Configuration des COLONNES (entrées avec pull-up) ---
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    
    GPIO_InitStruct.Pin = KEYPAD_COL0_PIN | KEYPAD_COL1_PIN | 
                          KEYPAD_COL2_PIN | KEYPAD_COL3_PIN;
    HAL_GPIO_Init(KEYPAD_COL_PORT, &GPIO_InitStruct);
    
    // Réinitialiser l'état
    memset(&keypad_state, 0, sizeof(KeypadState));
    keypad_state.inputMode = INPUT_MODE_abc;
    keypad_state.shiftLock = false;
    
    // Initialiser les états des touches
    for (int i = 0; i < KEYPAD_TOTAL_KEYS; i++)
    {
        uint8_t row = i / KEYPAD_COLS;
        uint8_t col = i % KEYPAD_COLS;
        keypad_state.keys[i].code = keypad_make_key(row, col);
    }
    
    // Vider le buffer d'événements
    memset(event_buffer, 0, sizeof(event_buffer));
    event_buffer_head = 0;
    event_buffer_tail = 0;
    
    KEYPAD_DEBUG("Clavier initialisé (%d touches)\n", KEYPAD_TOTAL_KEYS);
}

/**
 * @brief Enregistre un callback pour les événements
 */
void keypad_matrix_set_callback(void (*callback)(const KeypadEvent* event))
{
    keypad_event_callback = callback;
}

// ============================================================
// SECTION 2 : SCAN DE LA MATRICE
// ============================================================

/**
 * @brief Scanne la matrice du clavier
 * 
 * Principe :
 * 1. Mettre une ligne à LOW
 * 2. Lire les colonnes
 * 3. Si une colonne est LOW → la touche à l'intersection est pressée
 * 4. Remettre la ligne à HIGH
 * 5. Passer à la ligne suivante
 */
void keypad_matrix_scan(void)
{
    static uint32_t lastScan = 0;
    uint32_t now = HAL_GetTick();
    
    // Respecter l'intervalle de scan
    if ((now - lastScan) < keypad_config.scanIntervalMs)
    {
        return;
    }
    lastScan = now;
    
    // Pour chaque ligne
    for (uint8_t row = 0; row < KEYPAD_ROWS; row++)
    {
        // Mettre la ligne courante à LOW
        HAL_GPIO_WritePin(KEYPAD_ROW_PORT, KEYPAD_ROW_PINS[row], GPIO_PIN_RESET);
        
        // Petit délai de stabilisation
        for (volatile uint8_t d = 0; d < 10; d++) { __NOP(); }
        
        // Lire les colonnes
        for (uint8_t col = 0; col < KEYPAD_COLS; col++)
        {
            uint8_t index = row * KEYPAD_COLS + col;
            KeyState* key = &keypad_state.keys[index];
            
            // Lire l'état de la colonne (LOW = pressée à cause du pull-up)
            bool currentlyPressed = (HAL_GPIO_ReadPin(KEYPAD_COL_PORT, KEYPAD_COL_PINS[col]) == GPIO_PIN_RESET);
            
            // Traiter le changement d'état
            process_key_state(key, currentlyPressed, now);
        }
        
        // Remettre la ligne à HIGH
        HAL_GPIO_WritePin(KEYPAD_ROW_PORT, KEYPAD_ROW_PINS[row], GPIO_PIN_SET);
    }
    
    keypad_state.lastScanTime = now;
}

/**
 * @brief Traite le changement d'état d'une touche
 */
static void process_key_state(KeyState* key, bool currentlyPressed, uint32_t now)
{
    // Sauvegarder l'état précédent
    bool wasPressed = key->pressed;
    
    // Anti-rebond : ignorer les changements trop rapides
    if (currentlyPressed != wasPressed)
    {
        uint32_t timeSinceChange = now - key->lastPressTime;
        
        if (timeSinceChange < keypad_config.debounceMs)
        {
            return;  // Ignorer (rebond)
        }
    }
    
    // Mettre à jour l'état
    key->lastState = key->pressed;
    key->pressed = currentlyPressed;
    key->changed = (key->pressed != key->lastState);
    
    if (key->changed)
    {
        if (key->pressed)
        {
            // --- TOUCHE PRESSÉE ---
            key->pressTime = now;
            key->pressCount++;
            
            // Vérifier si c'est un nouvel appui ou une répétition Multitap
            uint32_t timeSinceLastPress = now - key->lastPressTime;
            
            if (timeSinceLastPress > keypad_config.multitapTimeoutMs)
            {
                key->pressCount = 1;  // Nouveau cycle Multitap
            }
            
            key->lastPressTime = now;
            keypad_state.activeKeyCount++;
            keypad_state.lastKey = key->code;
            keypad_state.totalKeyPresses++;
            
            // Créer un événement PRESS
            KeypadEvent event = {
                .key = key->code,
                .event = KEY_EVENT_PRESS,
                .timestamp = now,
                .row = keypad_get_row(key->code),
                .col = keypad_get_col(key->code),
                .character = keypad_get_character(key->code)
            };
            
            push_event(&event);
            
            KEYPAD_DEBUG("Key 0x%02X pressed (count=%d)\n", key->code, key->pressCount);
        }
        else
        {
            // --- TOUCHE RELÂCHÉE ---
            key->releaseTime = now;
            key->holdDuration = now - key->pressTime;
            
            if (keypad_state.activeKeyCount > 0)
            {
                keypad_state.activeKeyCount--;
            }
            
            // Créer un événement RELEASE
            KeypadEvent event = {
                .key = key->code,
                .event = KEY_EVENT_RELEASE,
                .timestamp = now,
                .row = keypad_get_row(key->code),
                .col = keypad_get_col(key->code),
                .character = 0
            };
            
            push_event(&event);
            
            KEYPAD_DEBUG("Key 0x%02X released (duration=%lu ms)\n", 
                        key->code, (unsigned long)key->holdDuration);
        }
    }
    
    // Vérifier l'appui long
    if (key->pressed)
    {
        uint32_t holdDuration = now - key->pressTime;
        
        if (holdDuration >= keypad_config.longPressMs && 
            (holdDuration - keypad_config.scanIntervalMs) < keypad_config.longPressMs)
        {
            // Appui long détecté
            KeypadEvent event = {
                .key = key->code,
                .event = KEY_EVENT_HOLD,
                .timestamp = now,
                .row = keypad_get_row(key->code),
                .col = keypad_get_col(key->code),
                .character = 0
            };
            
            push_event(&event);
            
            KEYPAD_DEBUG("Key 0x%02X held\n", key->code);
        }
        
        // Répétition automatique
        if (keypad_config.enableRepeat && holdDuration >= keypad_config.longPressMs)
        {
            uint32_t repeatTime = holdDuration - keypad_config.longPressMs;
            
            if (repeatTime > 0 && (repeatTime % keypad_config.repeatMs) < keypad_config.scanIntervalMs)
            {
                KeypadEvent event = {
                    .key = key->code,
                    .event = KEY_EVENT_REPEAT,
                    .timestamp = now,
                    .row = keypad_get_row(key->code),
                    .col = keypad_get_col(key->code),
                    .character = keypad_get_character(key->code)
                };
                
                push_event(&event);
            }
        }
    }
}

// ============================================================
// SECTION 3 : GESTION DU MULTITAP
// ============================================================

/**
 * @brief Récupère le caractère correspondant à une touche
 */
char keypad_get_character(KeyCode key)
{
    if (!KEYPAD_IS_CHAR_KEY(key)) return 0;
    
    uint8_t digit = KEYPAD_KEY_TO_DIGIT(key);
    uint8_t pressCount = keypad_state.keys[key].pressCount - 1;  // 0-based
    
    if (pressCount >= KEYPAD_MAX_MULTITAP)
    {
        pressCount = 0;  // Reboucler
    }
    
    switch (keypad_state.inputMode)
    {
        case INPUT_MODE_ABC:
            return MULTITAP_UPPER[digit][pressCount];
            
        case INPUT_MODE_abc:
            return MULTITAP_LOWER[digit][pressCount];
            
        case INPUT_MODE_123:
            return MULTITAP_NUMBERS[digit][pressCount];
            
        case INPUT_MODE_T9:
            // Mode T9 : retourner le premier caractère (la prédiction est gérée ailleurs)
            return MULTITAP_LOWER[digit][0];
            
        case INPUT_MODE_SYMBOLS:
            // Mode symboles : caractères spéciaux
            {
                static const char symbols[] = ".,?!@#$%^&*()-_=+[]{}|;:'\"<>/\\`~";
                if (digit < (sizeof(symbols) - 1))
                {
                    return symbols[digit];
                }
            }
            return 0;
            
        default:
            return 0;
    }
}

/**
 * @brief Change le mode de saisie
 */
void keypad_matrix_set_input_mode(InputMode mode)
{
    if (mode < INPUT_MODE_COUNT)
    {
        keypad_state.inputMode = mode;
        KEYPAD_DEBUG("Mode saisie: %s\n", INPUT_MODE_NAMES[mode]);
    }
}

/**
 * @brief Passe au mode de saisie suivant
 */
void keypad_matrix_next_input_mode(void)
{
    uint8_t nextMode = (keypad_state.inputMode + 1) % INPUT_MODE_COUNT;
    keypad_matrix_set_input_mode((InputMode)nextMode);
}

/**
 * @brief Récupère le mode de saisie actuel
 */
InputMode keypad_matrix_get_input_mode(void)
{
    return keypad_state.inputMode;
}

// ============================================================
// SECTION 4 : API PUBLIQUE
// ============================================================

/**
 * @brief Vérifie si une touche est pressée
 */
bool keypad_is_pressed(KeyCode key)
{
    if (!keypad_is_valid_key(key)) return false;
    
    uint8_t index = keypad_get_row(key) * KEYPAD_COLS + keypad_get_col(key);
    return keypad_state.keys[index].pressed;
}

/**
 * @brief Vérifie si une touche vient d'être pressée
 */
bool keypad_was_just_pressed(KeyCode key)
{
    if (!keypad_is_valid_key(key)) return false;
    
    uint8_t index = keypad_get_row(key) * KEYPAD_COLS + keypad_get_col(key);
    KeyState* ks = &keypad_state.keys[index];
    
    bool justPressed = ks->pressed && ks->changed;
    if (justPressed)
    {
        ks->changed = false;  // Consommer l'événement
    }
    return justPressed;
}

/**
 * @brief Vérifie si une touche vient d'être relâchée
 */
bool keypad_was_just_released(KeyCode key)
{
    if (!keypad_is_valid_key(key)) return false;
    
    uint8_t index = keypad_get_row(key) * KEYPAD_COLS + keypad_get_col(key);
    KeyState* ks = &keypad_state.keys[index];
    
    bool justReleased = !ks->pressed && ks->changed;
    if (justReleased)
    {
        ks->changed = false;
    }
    return justReleased;
}

/**
 * @brief Vérifie si une touche est maintenue (appui long)
 */
bool keypad_is_held(KeyCode key)
{
    if (!keypad_is_valid_key(key)) return false;
    
    uint8_t index = keypad_get_row(key) * KEYPAD_COLS + keypad_get_col(key);
    KeyState* ks = &keypad_state.keys[index];
    
    if (!ks->pressed) return false;
    
    return (HAL_GetTick() - ks->pressTime) >= keypad_config.longPressMs;
}

/**
 * @brief Récupère la dernière touche pressée
 */
KeyCode keypad_get_last_key(void)
{
    return keypad_state.lastKey;
}

/**
 * @brief Attend qu'une touche soit pressée
 */
KeyCode keypad_wait_for_key(uint32_t timeoutMs)
{
    uint32_t start = HAL_GetTick();
    
    while ((HAL_GetTick() - start) < timeoutMs)
    {
        keypad_matrix_scan();
        
        if (keypad_state.lastKey != KEY_NONE)
        {
            KeyCode key = keypad_state.lastKey;
            keypad_state.lastKey = KEY_NONE;
            return key;
        }
        
        HAL_Delay(1);
    }
    
    return KEY_NONE;
}

// ============================================================
// SECTION 5 : GESTION DES ÉVÉNEMENTS
// ============================================================

/**
 * @brief Ajoute un événement dans le buffer circulaire
 */
static void push_event(const KeypadEvent* event)
{
    if (event == NULL) return;
    
    event_buffer[event_buffer_head] = *event;
    event_buffer_head = (event_buffer_head + 1) % KEYPAD_EVENT_BUFFER_SIZE;
    
    // Si le buffer est plein, écraser le plus ancien
    if (event_buffer_head == event_buffer_tail)
    {
        event_buffer_tail = (event_buffer_tail + 1) % KEYPAD_EVENT_BUFFER_SIZE;
    }
    
    // Notifier le callback
    if (keypad_event_callback)
    {
        keypad_event_callback(event);
    }
}

/**
 * @brief Récupère le prochain événement (FIFO)
 */
bool keypad_get_event(KeypadEvent* event)
{
    if (event == NULL) return false;
    if (event_buffer_head == event_buffer_tail) return false;  // Buffer vide
    
    *event = event_buffer[event_buffer_tail];
    event_buffer_tail = (event_buffer_tail + 1) % KEYPAD_EVENT_BUFFER_SIZE;
    
    return true;
}

/**
 * @brief Vérifie si des événements sont disponibles
 */
bool keypad_has_events(void)
{
    return (event_buffer_head != event_buffer_tail);
}

/**
 * @brief Vide le buffer d'événements
 */
void keypad_flush_events(void)
{
    event_buffer_head = 0;
    event_buffer_tail = 0;
    memset(event_buffer, 0, sizeof(event_buffer));
}

// ============================================================
// SECTION 6 : CONFIGURATION
// ============================================================

/**
 * @brief Définit l'intervalle de scan
 */
void keypad_matrix_set_scan_interval(uint8_t ms)
{
    keypad_config.scanIntervalMs = ms;
}

/**
 * @brief Définit le temps d'anti-rebond
 */
void keypad_matrix_set_debounce(uint16_t ms)
{
    keypad_config.debounceMs = ms;
}

/**
 * @brief Définit le seuil d'appui long
 */
void keypad_matrix_set_long_press(uint16_t ms)
{
    keypad_config.longPressMs = ms;
}

/**
 * @brief Active/désactive la répétition automatique
 */
void keypad_matrix_set_repeat(bool enable, uint16_t intervalMs)
{
    keypad_config.enableRepeat = enable;
    keypad_config.repeatMs = intervalMs;
}

// ============================================================
// SECTION 7 : FONCTIONS UTILITAIRES
// ============================================================

/**
 * @brief Convertit un code de touche en nom lisible
 */
const char* keypad_key_name(KeyCode key)
{
    switch (key)
    {
        case KEY_MENU:   return "MENU";
        case KEY_UP:     return "HAUT";
        case KEY_OK:     return "OK";
        case KEY_DOWN:   return "BAS";
        case KEY_1:      return "1";
        case KEY_2:      return "2";
        case KEY_3:      return "3";
        case KEY_BACK:   return "RETOUR";
        case KEY_4:      return "4";
        case KEY_5:      return "5";
        case KEY_6:      return "6";
        case KEY_CALL:   return "APPEL";
        case KEY_7:      return "7";
        case KEY_8:      return "8";
        case KEY_9:      return "9";
        case KEY_END:    return "RACCROCHER";
        case KEY_STAR:   return "*";
        case KEY_0:      return "0";
        case KEY_HASH:   return "#";
        case KEY_SHIFT:  return "MAJ";
        case KEY_LAMP:   return "LAMPE";
        case KEY_PTT:    return "PTT";
        case KEY_MUTE:   return "MUET";
        case KEY_VOL:    return "VOLUME";
        case KEY_NONE:   return "AUCUNE";
        default:         return "INCONNUE";
    }
}

/**
 * @brief Récupère le nombre total d'appuis
 */
uint32_t keypad_get_total_presses(void)
{
    return keypad_state.totalKeyPresses;
}

/**
 * @brief Réinitialise les statistiques
 */
void keypad_reset_stats(void)
{
    keypad_state.totalKeyPresses = 0;
    for (int i = 0; i < KEYPAD_TOTAL_KEYS; i++)
    {
        keypad_state.keys[i].pressCount = 0;
    }
}

// ============================================================
// SECTION 8 : DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état du clavier
 */
void keypad_matrix_print_state(void)
{
    printf("\n═══ ÉTAT DU CLAVIER ═══\n");
    printf("Mode       : %s\n", INPUT_MODE_NAMES[keypad_state.inputMode]);
    printf("Shift Lock : %s\n", keypad_state.shiftLock ? "Oui" : "Non");
    printf("Touches actives : %d\n", keypad_state.activeKeyCount);
    printf("Total appuis    : %lu\n", (unsigned long)keypad_state.totalKeyPresses);
    
    printf("\nTouches pressées :\n");
    for (int i = 0; i < KEYPAD_TOTAL_KEYS; i++)
    {
        if (keypad_state.keys[i].pressed)
        {
            printf("  %-10s (0x%02X) - %lu ms\n", 
                   keypad_key_name(keypad_state.keys[i].code),
                   keypad_state.keys[i].code,
                   (unsigned long)(HAL_GetTick() - keypad_state.keys[i].pressTime));
        }
    }
    printf("═══════════════════════\n\n");
}

/**
 * @brief Affiche la matrice visuelle
 */
void keypad_matrix_print_visual(void)
{
    printf("\n┌───────┬───────┬───────┬───────┐\n");
    
    for (int row = 0; row < KEYPAD_ROWS; row++)
    {
        printf("│");
        for (int col = 0; col < KEYPAD_COLS; col++)
        {
            KeyCode key = keypad_make_key(row, col);
            bool pressed = keypad_is_pressed(key);
            
            if (pressed)
            {
                printf(" [%-3s] │", keypad_key_name(key));
            }
            else
            {
                printf("  %-3s  │", keypad_key_name(key));
            }
        }
        printf("\n");
        
        if (row < KEYPAD_ROWS - 1)
        {
            printf("├───────┼───────┼───────┼───────┤\n");
        }
    }
    
    printf("└───────┴───────┴───────┴───────┘\n\n");
}