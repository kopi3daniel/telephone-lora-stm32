/**
 * @file    debug_utils.cpp
 * @brief   Implémentation des utilitaires de débogage
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente le système de logging et de débogage.
 * 
 * SORTIES SUPPORTÉES :
 * 
 * 1. UART (ITM ou USART) :
 *    - USART1 (PA9 TX, PA10 RX) à 115200 bauds
 *    - Utilisé pour la console série standard
 * 
 * 2. SWO (Single Wire Output) :
 *    - Via le debugger ST-LINK
 *    - Affichage dans le terminal SWO de l'IDE
 *    - Plus rapide que UART, ne consomme pas d'USART
 * 
 * 3. ÉCRAN :
 *    - Affichage sur l'écran TFT en overlay
 *    - Utile quand pas de connexion debug
 * 
 * FORMAT DES LOGS :
 * 
 *   [LEVEL] [TAG] [HH:MM:SS.ms] Message
 * 
 *   Exemple avec couleurs :
 *   [ERROR] [LoRa] [14:32:15.123] Échec initialisation
 *   [WARN]  [Bat]  [14:32:20.456] Niveau faible: 15%
 *   [INFO]  [App]  [14:32:25.789] Démarrage v1.0.0
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "debug_utils.h"
#include "timer_utils.h"

/* HAL */
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"

/* Standard */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs du module debug */
#define TAG                                 "Debug"

/** Taille du buffer de formatage */
#define DEBUG_BUFFER_SIZE                   256

/** Taille du buffer circulaire de logs persistants */
#define DEBUG_LOG_HISTORY_SIZE              64
#define DEBUG_LOG_ENTRY_MAX_LENGTH          128

/** Timeout UART (ms) */
#define DEBUG_UART_TIMEOUT                  10

/** Adresse de base ITM (Instrumentation Trace Macrocell) */
#define ITM_BASE                            (0xE0000000UL)
#define ITM_STIMULUS_PORT0                  (*(volatile uint32_t*)(ITM_BASE + 0x00))
#define ITM_TRACE_ENABLE                    (*(volatile uint32_t*)(ITM_BASE + 0xE00))
#define ITM_TRACE_PRIVILEGED                (*(volatile uint32_t*)(ITM_BASE + 0xE40))

/* ======================================================================== */
/*                VARIABLES STATIQUES                                       */
/* ======================================================================== */

/** Handle UART pour debug */
static UART_HandleTypeDef g_debug_uart;

/** Buffer de formatage */
static char g_format_buffer[DEBUG_BUFFER_SIZE];

/** Compteur de logs */
static uint32_t g_log_count = 0;

/** Niveaux activés (bitmask) */
static uint8_t g_enabled_levels = 0xFF;  /* Tous activés */

/** Callback de sortie personnalisé */
static void (*g_output_callback)(char c) = NULL;

/** Historique circulaire */
typedef struct {
    char    entry[DEBUG_LOG_ENTRY_MAX_LENGTH];
    uint8_t level;
} DebugLogEntry_t;

static DebugLogEntry_t g_log_history[DEBUG_LOG_HISTORY_SIZE];
static uint8_t g_log_history_index = 0;

/** Nom du thread courant */
static char g_thread_name[16] = "MAIN";

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

static void init_uart(void);
static void init_swo(void);
static void output_char(char c);
static void output_string(const char* str);
static const char* level_to_string(uint8_t level);
static const char* level_to_ansi(uint8_t level);
static void add_to_history(uint8_t level, const char* message);
static uint32_t get_timestamp_ms(void);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise le système de debug
 */
void DebugUtils_Init(void)
{
    /* UART */
#if DEBUG_UART_ENABLED
    init_uart();
#endif

    /* SWO */
#if DEBUG_SWO_ENABLED
    init_swo();
#endif

    /* Historique */
    memset(g_log_history, 0, sizeof(g_log_history));
    g_log_history_index = 0;

    g_log_count = 0;

    DEBUG_INFO(TAG, "Système debug initialisé (niveau=%d)", DEBUG_LEVEL);
}

/**
 * @brief Log un message formaté
 */
void DebugUtils_Log(uint8_t level, const char* tag, const char* format, ...)
{
    /* Vérifier si le niveau est activé */
    if (!(g_enabled_levels & (1 << level))) {
        return;
    }

    /* Horodatage */
    uint32_t timestamp = get_timestamp_ms();
    uint32_t ms = timestamp % 1000;
    uint32_t sec = (timestamp / 1000) % 60;
    uint32_t min = (timestamp / 60000) % 60;
    uint32_t hour = (timestamp / 3600000) % 24;

    /* Formater le message utilisateur */
    char user_msg[DEBUG_BUFFER_SIZE - 64];
    va_list args;
    va_start(args, format);
    vsnprintf(user_msg, sizeof(user_msg), format, args);
    va_end(args);

    /* Construire la ligne complète */
    int pos = 0;

    /* Niveau avec couleur */
#if DEBUG_COLOR
    pos += snprintf(g_format_buffer + pos, DEBUG_BUFFER_SIZE - pos,
                    "%s[%-5s]%s ",
                    level_to_ansi(level), level_to_string(level), ANSI_RESET);
#else
    pos += snprintf(g_format_buffer + pos, DEBUG_BUFFER_SIZE - pos,
                    "[%-5s] ", level_to_string(level));
#endif

    /* Tag */
    pos += snprintf(g_format_buffer + pos, DEBUG_BUFFER_SIZE - pos,
                    "[%-8s] ", tag ? tag : "?");

    /* Timestamp */
#if DEBUG_TIMESTAMP
    pos += snprintf(g_format_buffer + pos, DEBUG_BUFFER_SIZE - pos,
                    "[%02lu:%02lu:%02lu.%03lu] ",
                    hour, min, sec, ms);
#endif

    /* Nom fonction */
#if DEBUG_FUNCTION_NAME
    pos += snprintf(g_format_buffer + pos, DEBUG_BUFFER_SIZE - pos,
                    "[%s] ", g_thread_name);
#endif

    /* Message */
    pos += snprintf(g_format_buffer + pos, DEBUG_BUFFER_SIZE - pos,
                    "%s", user_msg);

    /* Nouvelle ligne */
    if (pos < (int)(DEBUG_BUFFER_SIZE - 2)) {
        g_format_buffer[pos++] = '\r';
        g_format_buffer[pos++] = '\n';
    }
    g_format_buffer[pos] = '\0';

    /* Sortie */
    output_string(g_format_buffer);

    /* Historique */
    add_to_history(level, user_msg);

    g_log_count++;
}

/**
 * @brief Log brut sans formatage
 */
void DebugUtils_Raw(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_format_buffer, DEBUG_BUFFER_SIZE, format, args);
    va_end(args);

    output_string(g_format_buffer);
}

/**
 * @brief Trace d'entrée de fonction
 */
void DebugUtils_TraceEnter(const char* func)
{
    DEBUG_DEBUG("TRACE", "→ %s()", func ? func : "?");
}

/**
 * @brief Trace de sortie de fonction
 */
void DebugUtils_TraceExit(const char* func)
{
    DEBUG_DEBUG("TRACE", "← %s()", func ? func : "?");
}

/**
 * @brief Trace simple
 */
void DebugUtils_Trace(const char* func, uint32_t line)
{
    DEBUG_DEBUG("TRACE", "● %s:%lu", func ? func : "?", line);
}

/**
 * @brief Assertion échouée
 */
void DebugUtils_AssertFailed(const char* expr, const char* file, uint32_t line)
{
    DEBUG_ERROR("ASSERT", "ÉCHEC: %s", expr ? expr : "?");
    DEBUG_ERROR("ASSERT", "  Fichier: %s", file ? file : "?");
    DEBUG_ERROR("ASSERT", "  Ligne  : %lu", line);

    /* Point d'arrêt si debugger connecté */
    DEBUG_BREAK();

    /* Boucle infinie (bloque le système) */
    while (1) {
        __NOP();
    }
}

/**
 * @brief Vérification échouée
 */
void DebugUtils_CheckFailed(const char* expr, const char* file, uint32_t line)
{
    DEBUG_ERROR("CHECK", "ÉCHEC: %s", expr ? expr : "?");
    DEBUG_ERROR("CHECK", "  Fichier: %s", file ? file : "?");
    DEBUG_ERROR("CHECK", "  Ligne  : %lu", line);
}

/**
 * @brief Erreur fatale
 */
void DebugUtils_Fatal(const char* file, uint32_t line, const char* format, ...)
{
    char msg[DEBUG_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    DEBUG_ERROR("FATAL", "%s", msg);
    DEBUG_ERROR("FATAL", "  Fichier: %s:%lu", file ? file : "?", line);

    /* Sauvegarder les logs */
    /* (à implémenter si DEBUG_FLASH_ENABLED) */

    DEBUG_BREAK();

    while (1) {
        __NOP();
    }
}

/**
 * @brief Affiche un hexdump
 */
void DebugUtils_Hexdump(const uint8_t* data, size_t length, uint32_t address)
{
    if (!data || length == 0) return;

    /* Activer temporairement le niveau DEBUG si nécessaire */
    uint8_t saved_levels = g_enabled_levels;
    g_enabled_levels |= (1 << DEBUG_LEVEL_DEBUG);

    DEBUG_DEBUG("HEX", "Dump de %u octets à 0x%08lX:", (unsigned int)length, address);

    for (size_t i = 0; i < length; i += 16) {
        char line[80];
        int pos = 0;

        /* Adresse */
        pos += snprintf(line + pos, sizeof(line) - pos, "  %08lX: ", (uint32_t)(address + i));

        /* Hex */
        for (size_t j = 0; j < 16; j++) {
            if (i + j < length) {
                pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", data[i + j]);
            } else {
                pos += snprintf(line + pos, sizeof(line) - pos, "   ");
            }
        }

        /* ASCII */
        pos += snprintf(line + pos, sizeof(line) - pos, " |");
        for (size_t j = 0; j < 16 && (i + j) < length; j++) {
            char c = data[i + j];
            pos += snprintf(line + pos, sizeof(line) - pos, "%c",
                           (c >= 32 && c < 127) ? c : '.');
        }
        pos += snprintf(line + pos, sizeof(line) - pos, "|");

        DebugUtils_Raw("%s\r\n", line);
    }

    g_enabled_levels = saved_levels;
}

/**
 * @brief Affiche les registres du CPU
 */
void DebugUtils_DumpRegisters(void)
{
    DEBUG_INFO("CPU", "========================================");
    DEBUG_INFO("CPU", "  REGISTRES CPU (Cortex-M4)");
    DEBUG_INFO("CPU", "========================================");

    uint32_t msp, psp, control, ipsr, primask, faultmask, basepri;

    __asm volatile("MRS %0, MSP" : "=r"(msp));
    __asm volatile("MRS %0, PSP" : "=r"(psp));
    __asm volatile("MRS %0, CONTROL" : "=r"(control));
    __asm volatile("MRS %0, IPSR" : "=r"(ipsr));
    __asm volatile("MRS %0, PRIMASK" : "=r"(primask));
    __asm volatile("MRS %0, FAULTMASK" : "=r"(faultmask));
    __asm volatile("MRS %0, BASEPRI" : "=r"(basepri));

    DEBUG_INFO("CPU", "  MSP      = 0x%08lX", msp);
    DEBUG_INFO("CPU", "  PSP      = 0x%08lX", psp);
    DEBUG_INFO("CPU", "  CONTROL  = 0x%08lX", control);
    DEBUG_INFO("CPU", "  IPSR     = %lu", ipsr);
    DEBUG_INFO("CPU", "  PRIMASK  = %lu", primask);
    DEBUG_INFO("CPU", "  FAULTMASK= %lu", faultmask);
    DEBUG_INFO("CPU", "  BASEPRI  = %lu", basepri);
    DEBUG_INFO("CPU", "  SP main  = 0x%08lX", (uint32_t)&__initial_sp);
    DEBUG_INFO("CPU", "========================================");
}

/**
 * @brief Affiche la pile d'appel
 */
void DebugUtils_StackTrace(uint8_t max_depth)
{
    DEBUG_INFO("STACK", "========================================");
    DEBUG_INFO("STACK", "  TRACE DE PILE (max=%d)", max_depth);
    DEBUG_INFO("STACK", "========================================");

    /* Récupérer le pointeur de pile courant */
    uint32_t sp;
    __asm volatile("MOV %0, SP" : "=r"(sp));

    /* Aligner */
    sp &= ~0x03;

    DEBUG_INFO("STACK", "  SP = 0x%08lX", sp);

    /* Parcourir la pile (approximatif) */
    uint32_t* stack_ptr = (uint32_t*)sp;
    uint32_t* stack_top = (uint32_t*)((uint32_t)&__initial_sp);

    uint8_t depth = 0;
    for (uint32_t* ptr = stack_ptr; ptr < stack_top && depth < max_depth; ptr++) {
        uint32_t value = *ptr;

        /* Vérifier si ça ressemble à une adresse flash (0x0800xxxx) */
        if (value >= 0x08000000 && value < 0x08200000) {
            /* Bit 0 = 1 pour adresse Thumb */
            if (value & 1) {
                DEBUG_INFO("STACK", "  [%d] 0x%08lX (PC probable)", depth, value & ~1);
                depth++;
            }
        }
    }

    if (depth == 0) {
        DEBUG_INFO("STACK", "  Aucune adresse trouvée");
    }
    DEBUG_INFO("STACK", "========================================");
}

/**
 * @brief Affiche l'utilisation mémoire
 */
void DebugUtils_MemoryInfo(void)
{
    DEBUG_INFO("MEM", "========================================");
    DEBUG_INFO("MEM", "  UTILISATION MÉMOIRE");
    DEBUG_INFO("MEM", "========================================");

    /* Pointeur de pile */
    extern uint32_t __initial_sp;
    extern uint32_t __heap_start;
    extern uint32_t __heap_end;

    uint32_t sp;
    __asm volatile("MOV %0, SP" : "=r"(sp));

    uint32_t stack_total = (uint32_t)&__initial_sp - 0x20000000;
    uint32_t stack_used = (uint32_t)&__initial_sp - sp;
    uint32_t stack_free = sp - 0x20000000;

    DEBUG_INFO("MEM", "  Stack total : %lu octets", stack_total);
    DEBUG_INFO("MEM", "  Stack utilisé: %lu octets", stack_used);
    DEBUG_INFO("MEM", "  Stack libre : %lu octets", stack_free);
    DEBUG_INFO("MEM", "  Stack SP    : 0x%08lX", sp);

    uint32_t heap_total = (uint32_t)&__heap_end - (uint32_t)&__heap_start;
    DEBUG_INFO("MEM", "  Heap total  : %lu octets", heap_total);

    /* RAM totale (STM32F429 = 256 Ko, dont 64 Ko CCM) */
    DEBUG_INFO("MEM", "  RAM totale  : 256 Ko (192+64 CCM)");
    DEBUG_INFO("MEM", "========================================");
}

/**
 * @brief Active/désactive un niveau
 */
void DebugUtils_SetLevelEnabled(uint8_t level, bool enabled)
{
    if (level > 7) return;

    if (enabled) {
        g_enabled_levels |= (1 << level);
    } else {
        g_enabled_levels &= ~(1 << level);
    }
}

/**
 * @brief Définit le callback de sortie
 */
void DebugUtils_SetOutput(void (*callback)(char c))
{
    g_output_callback = callback;
}

/**
 * @brief Vide les buffers
 */
void DebugUtils_Flush(void)
{
    /* Rien à faire pour UART bloquant */
}

/**
 * @brief Compteur de logs
 */
uint32_t DebugUtils_GetLogCount(void)
{
    return g_log_count;
}

/**
 * @brief Réinitialise le compteur
 */
void DebugUtils_ResetLogCount(void)
{
    g_log_count = 0;
}

/**
 * @brief Nom du thread
 */
void DebugUtils_SetThreadName(const char* name)
{
    if (name) {
        strncpy(g_thread_name, name, sizeof(g_thread_name) - 1);
        g_thread_name[sizeof(g_thread_name) - 1] = '\0';
    }
}

/* ======================================================================== */
/*              FONCTIONS PRIVÉES                                           */
/* ======================================================================== */

/**
 * @brief Initialise l'UART de debug (USART1)
 */
static void init_uart(void)
{
    /* Configurer USART1 : PA9=TX, PA10=RX, 115200 bauds */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA9 = TX */
    GPIO_InitTypeDef gpio = {
        .Pin = GPIO_PIN_9,
        .Mode = GPIO_MODE_AF_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_HIGH,
        .Alternate = GPIO_AF7_USART1
    };
    HAL_GPIO_Init(GPIOA, &gpio);

    /* PA10 = RX (optionnel) */
    gpio.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* UART */
    g_debug_uart.Instance = USART1;
    g_debug_uart.Init.BaudRate = 115200;
    g_debug_uart.Init.WordLength = UART_WORDLENGTH_8B;
    g_debug_uart.Init.StopBits = UART_STOPBITS_1;
    g_debug_uart.Init.Parity = UART_PARITY_NONE;
    g_debug_uart.Init.Mode = UART_MODE_TX_RX;
    g_debug_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_debug_uart.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&g_debug_uart);
}

/**
 * @brief Initialise la sortie SWO (ITM)
 */
static void init_swo(void)
{
    /* Activer le trace ITM */
    ITM_TRACE_ENABLE = 0xFFFFFFFF;
    ITM_TRACE_PRIVILEGED = 1;
}

/**
 * @brief Émet un caractère sur toutes les sorties actives
 */
static void output_char(char c)
{
    /* UART */
#if DEBUG_UART_ENABLED
    HAL_UART_Transmit(&g_debug_uart, (uint8_t*)&c, 1, DEBUG_UART_TIMEOUT);
#endif

    /* SWO (ITM) */
#if DEBUG_SWO_ENABLED
    ITM_STIMULUS_PORT0 = c;
#endif

    /* Callback personnalisé */
    if (g_output_callback) {
        g_output_callback(c);
    }
}

/**
 * @brief Émet une chaîne
 */
static void output_string(const char* str)
{
    if (!str) return;

    while (*str) {
        /* Remplacer \n seul par \r\n */
        if (*str == '\n') {
            output_char('\r');
            output_char('\n');
        } else {
            output_char(*str);
        }
        str++;
    }
}

/**
 * @brief Convertit un niveau en chaîne
 */
static const char* level_to_string(uint8_t level)
{
    switch (level) {
        case DEBUG_LEVEL_ERROR:     return "ERROR";
        case DEBUG_LEVEL_WARN:      return "WARN";
        case DEBUG_LEVEL_INFO:      return "INFO";
        case DEBUG_LEVEL_DEBUG:     return "DEBUG";
        case DEBUG_LEVEL_VERBOSE:   return "VERB";
        default:                    return "?????";
    }
}

/**
 * @brief Code couleur ANSI pour un niveau
 */
static const char* level_to_ansi(uint8_t level)
{
    switch (level) {
        case DEBUG_LEVEL_ERROR:     return ANSI_ERROR;
        case DEBUG_LEVEL_WARN:      return ANSI_WARN;
        case DEBUG_LEVEL_INFO:      return ANSI_INFO;
        case DEBUG_LEVEL_DEBUG:     return ANSI_DEBUG;
        case DEBUG_LEVEL_VERBOSE:   return ANSI_VERBOSE;
        default:                    return "";
    }
}

/**
 * @brief Ajoute un message à l'historique circulaire
 */
static void add_to_history(uint8_t level, const char* message)
{
    if (!message) return;

    DebugLogEntry_t* entry = &g_log_history[g_log_history_index];
    entry->level = level;
    strncpy(entry->entry, message, DEBUG_LOG_ENTRY_MAX_LENGTH - 1);
    entry->entry[DEBUG_LOG_ENTRY_MAX_LENGTH - 1] = '\0';

    g_log_history_index = (g_log_history_index + 1) % DEBUG_LOG_HISTORY_SIZE;
}

/**
 * @brief Timestamp millisecondes
 */
static uint32_t get_timestamp_ms(void)
{
    return HAL_GetTick();
}

/* ======================================================================== */
/*              FONCTIONS DE REDIRECTION                                    */
/* ======================================================================== */

/**
 * @brief Redirection de printf vers le système de debug
 *
 * Active avec : setvbuf(stdout, NULL, _IONBF, 0);
 * Puis printf() utilisera automatiquement la sortie debug.
 */
int _write(int file, char* ptr, int len)
{
    (void)file;

    for (int i = 0; i < len; i++) {
        output_char(ptr[i]);
    }

    return len;
}

/* ======================================================================== */
/*              EXEMPLE D'UTILISATION                                       */
/* ======================================================================== */

#if 0  /* Exemples - Non compilés */

void debug_examples(void)
{
    /* Logging basique */
    DEBUG_ERROR("Test", "Ceci est une erreur");
    DEBUG_WARN("Test", "Ceci est un avertissement");
    DEBUG_INFO("Test", "Ceci est une information");
    DEBUG_DEBUG("Test", "Ceci est un debug");
    DEBUG_VERBOSE("Test", "Ceci est un verbose");

    /* Hexdump */
    uint8_t data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F,
                      0x72, 0x6C, 0x64, 0x21, 0x00, 0xFF, 0xAA, 0x55};
    DebugUtils_Hexdump(data, sizeof(data), 0x20000000);

    /* Assertion */
    int* ptr = NULL;
    DEBUG_ASSERT(ptr != NULL);  /* Va échouer et logger */

    /* Trace fonction */
    void ma_fonction(void) {
        DEBUG_ENTER();
        // ...
        DEBUG_EXIT();
    }

    /* Registres */
    DebugUtils_DumpRegisters();

    /* Mémoire */
    DebugUtils_MemoryInfo();

    /* Stack trace (après crash) */
    DebugUtils_StackTrace(10);
}

#endif

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */