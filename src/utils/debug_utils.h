/**
 * @file    debug_utils.h
 * @brief   Utilitaires de débogage - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 *
 * Fournit un système de logging et de débogage pour le développement.
 *
 * FONCTIONNALITÉS :
 *
 * 1. LOGGING MULTI-NIVEAUX :
 *    - ERROR   : Erreurs critiques (toujours affichées)
 *    - WARN    : Avertissements
 *    - INFO    : Informations générales
 *    - DEBUG   : Débogage détaillé
 *    - VERBOSE : Très détaillé (peut spammer)
 *
 * 2. SORTIES MULTIPLES :
 *    - UART (console série)
 *    - SWO (Single Wire Output, via debugger)
 *    - Écran (affichage direct)
 *    - Flash (log persistant)
 *
 * 3. ASSERTIONS :
 *    - DEBUG_ASSERT : Vérification en debug, désactivé en release
 *    - CHECK : Vérification toujours active
 *    - FATAL : Erreur fatale avec message
 *
 * 4. TRACES :
 *    - Fonction entrée/sortie
 *    - Valeurs de variables
 *    - Horodatage automatique
 *
 * 5. DUMPS :
 *    - Hexdump mémoire
 *    - Registres CPU
 *    - Stack trace (adresses)
 *
 * FORMAT DES LOGS :
 *
 *   [ERROR]   FileSystem: Flash erase failed at 0x08020000
 *   [WARN]    LoRa: RSSI low (-120 dBm)
 *   [INFO]    PhoneApp: Démarrage v1.0.0
 *   [DEBUG]   Audio: Buffer level 512/1024
 *   [VERBOSE] Touch: X=150 Y=320 P=2048
 *
 * UTILISATION :
 *
 *   DEBUG_ERROR("LoRa", "Échec initialisation");
 *   DEBUG_WARN("Battery", "Niveau faible: %d%%", percent);
 *   DEBUG_INFO("App", "Version %s", VERSION);
 *   DEBUG_VERBOSE("Sensor", "Valeur=%d", value);
 *
 * CONFIGURATION (dans project_config.h) :
 *
 *   #define DEBUG_LEVEL    DEBUG_LEVEL_INFO    // Niveau max
 *   #define DEBUG_UART     1                   // Sortie UART
 *   #define DEBUG_SWO      0                   // Sortie SWO
 *   #define DEBUG_TIMESTAMP 1                  // Horodatage
 *   #define DEBUG_COLOR    1                   // Couleurs ANSI
 */

#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* ======================================================================== */
/*              CONFIGURATION (override dans project_config.h)               */
/* ======================================================================== */

/**
 * @brief Niveaux de debug
 *
 * Les messages de niveau ≤ DEBUG_LEVEL seront affichés.
 * En release, définir DEBUG_LEVEL = 0 pour désactiver.
 */
#ifndef DEBUG_LEVEL
    #define DEBUG_LEVEL                         DEBUG_LEVEL_INFO
#endif

#define DEBUG_LEVEL_NONE                        0   /**< Aucun log           */
#define DEBUG_LEVEL_ERROR                       1   /**< Erreurs seulement   */
#define DEBUG_LEVEL_WARN                        2   /**< + Warnings         */
#define DEBUG_LEVEL_INFO                        3   /**< + Infos            */
#define DEBUG_LEVEL_DEBUG                       4   /**< + Debug            */
#define DEBUG_LEVEL_VERBOSE                     5   /**< + Verbose          */

/**
 * @brief Sorties activées
 */
#ifndef DEBUG_UART_ENABLED
    #define DEBUG_UART_ENABLED                  1   /**< Sortie UART         */
#endif

#ifndef DEBUG_SWO_ENABLED
    #define DEBUG_SWO_ENABLED                   0   /**< Sortie SWO          */
#endif

#ifndef DEBUG_DISPLAY_ENABLED
    #define DEBUG_DISPLAY_ENABLED               0   /**< Sortie écran        */
#endif

#ifndef DEBUG_FLASH_ENABLED
    #define DEBUG_FLASH_ENABLED                 0   /**< Log persistant      */
#endif

/**
 * @brief Options d'affichage
 */
#ifndef DEBUG_TIMESTAMP
    #define DEBUG_TIMESTAMP                     1   /**< Horodatage           */
#endif

#ifndef DEBUG_COLOR
    #define DEBUG_COLOR                         1   /**< Couleurs ANSI        */
#endif

#ifndef DEBUG_FUNCTION_NAME
    #define DEBUG_FUNCTION_NAME                 0   /**< Nom fonction         */
#endif

#ifndef DEBUG_FILE_LINE
    #define DEBUG_FILE_LINE                     0   /**< Fichier/ligne       */
#endif

/* ======================================================================== */
/*              MACROS DE LOGGING                                           */
/* ======================================================================== */

/**
 * @brief Log un message avec niveau et tag
 *
 * Utilisation :
 *   DEBUG_LOG(DEBUG_LEVEL_ERROR, "TAG", "Message %d", value);
 */
#define DEBUG_LOG(level, tag, ...) \
    do { \
        if ((level) <= DEBUG_LEVEL) { \
            DebugUtils_Log(level, tag, __VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief Raccourcis par niveau
 */
#if DEBUG_LEVEL >= DEBUG_LEVEL_ERROR
    #define DEBUG_ERROR(tag, ...)           DEBUG_LOG(DEBUG_LEVEL_ERROR, tag, __VA_ARGS__)
#else
    #define DEBUG_ERROR(tag, ...)           ((void)0)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_WARN
    #define DEBUG_WARN(tag, ...)            DEBUG_LOG(DEBUG_LEVEL_WARN, tag, __VA_ARGS__)
#else
    #define DEBUG_WARN(tag, ...)            ((void)0)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_INFO
    #define DEBUG_INFO(tag, ...)            DEBUG_LOG(DEBUG_LEVEL_INFO, tag, __VA_ARGS__)
#else
    #define DEBUG_INFO(tag, ...)            ((void)0)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_DEBUG
    #define DEBUG_DEBUG(tag, ...)           DEBUG_LOG(DEBUG_LEVEL_DEBUG, tag, __VA_ARGS__)
#else
    #define DEBUG_DEBUG(tag, ...)           ((void)0)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_VERBOSE
    #define DEBUG_VERBOSE(tag, ...)         DEBUG_LOG(DEBUG_LEVEL_VERBOSE, tag, __VA_ARGS__)
#else
    #define DEBUG_VERBOSE(tag, ...)         ((void)0)
#endif

/**
 * @brief Trace d'entrée/sortie de fonction
 */
#if DEBUG_LEVEL >= DEBUG_LEVEL_DEBUG
    #define DEBUG_ENTER()                   DebugUtils_TraceEnter(__func__)
    #define DEBUG_EXIT()                    DebugUtils_TraceExit(__func__)
    #define DEBUG_TRACE()                   DebugUtils_Trace(__func__, __LINE__)
#else
    #define DEBUG_ENTER()                   ((void)0)
    #define DEBUG_EXIT()                    ((void)0)
    #define DEBUG_TRACE()                   ((void)0)
#endif

/* ======================================================================== */
/*              MACROS D'ASSERTION                                          */
/* ======================================================================== */

/**
 * @brief Assertion en mode debug
 *
 * En debug : vérifie la condition, log + breakpoint si échec.
 * En release : désactivé.
 */
#if DEBUG_LEVEL >= DEBUG_LEVEL_ERROR
    #define DEBUG_ASSERT(cond) \
        do { \
            if (!(cond)) { \
                DebugUtils_AssertFailed(#cond, __FILE__, __LINE__); \
            } \
        } while(0)
#else
    #define DEBUG_ASSERT(cond)              ((void)0)
#endif

/**
 * @brief Vérification toujours active (même en release)
 */
#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            DebugUtils_CheckFailed(#cond, __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief Erreur fatale (log + boucle infinie)
 */
#define FATAL(...) \
    do { \
        DebugUtils_Fatal(__FILE__, __LINE__, __VA_ARGS__); \
    } while(0)

/**
 * @brief Point d'arrêt (breakpoint)
 */
#define DEBUG_BREAK() \
    do { \
        if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) { \
            __BKPT(0); \
        } \
    } while(0)

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS                                    */
/* ======================================================================== */

/**
 * @brief Initialise le système de debug
 *
 * Configure UART, SWO selon la configuration.
 */
void DebugUtils_Init(void);

/**
 * @brief Log un message formaté
 *
 * @param level     Niveau (DEBUG_LEVEL_ERROR...)
 * @param tag       Tag (nom du module)
 * @param format    Format printf
 * @param ...       Arguments
 */
void DebugUtils_Log(uint8_t level, const char* tag, const char* format, ...);

/**
 * @brief Log brut (sans formatage niveau/tag)
 *
 * @param format    Format
 * @param ...       Arguments
 */
void DebugUtils_Raw(const char* format, ...);

/**
 * @brief Trace d'entrée de fonction
 * @param func      Nom de la fonction
 */
void DebugUtils_TraceEnter(const char* func);

/**
 * @brief Trace de sortie de fonction
 * @param func      Nom de la fonction
 */
void DebugUtils_TraceExit(const char* func);

/**
 * @brief Trace simple
 * @param func      Nom de la fonction
 * @param line      Ligne
 */
void DebugUtils_Trace(const char* func, uint32_t line);

/**
 * @brief Assertion échouée
 *
 * @param expr      Expression
 * @param file      Fichier
 * @param line      Ligne
 */
void DebugUtils_AssertFailed(const char* expr, const char* file, uint32_t line);

/**
 * @brief Vérification échouée
 */
void DebugUtils_CheckFailed(const char* expr, const char* file, uint32_t line);

/**
 * @brief Erreur fatale
 *
 * @param file      Fichier
 * @param line      Ligne
 * @param format    Message
 * @param ...       Arguments
 */
void DebugUtils_Fatal(const char* file, uint32_t line, const char* format, ...);

/**
 * @brief Affiche un hexdump
 *
 * @param data      Données
 * @param length    Longueur
 * @param address   Adresse de départ (pour affichage)
 */
void DebugUtils_Hexdump(const uint8_t* data, size_t length, uint32_t address);

/**
 * @brief Affiche les registres du CPU
 */
void DebugUtils_DumpRegisters(void);

/**
 * @brief Affiche la pile d'appel (adresses)
 *
 * @param max_depth Profondeur max
 */
void DebugUtils_StackTrace(uint8_t max_depth);

/**
 * @brief Affiche l'utilisation mémoire
 */
void DebugUtils_MemoryInfo(void);

/**
 * @brief Active/désactive un niveau de log
 *
 * @param level     Niveau
 * @param enabled   true = activé
 */
void DebugUtils_SetLevelEnabled(uint8_t level, bool enabled);

/**
 * @brief Définit la fonction de sortie personnalisée
 *
 * @param callback  Fonction appelée pour chaque caractère
 */
void DebugUtils_SetOutput(void (*callback)(char c));

/**
 * @brief Vide les buffers de sortie
 */
void DebugUtils_Flush(void);

/**
 * @brief Retourne le nombre de messages loggés
 * @return          Compteur total
 */
uint32_t DebugUtils_GetLogCount(void);

/**
 * @brief Réinitialise le compteur de logs
 */
void DebugUtils_ResetLogCount(void);

/**
 * @brief Assigne un nom au thread courant (pour logs)
 * @param name      Nom du thread
 */
void DebugUtils_SetThreadName(const char* name);

/* ======================================================================== */
/*              COULEURS ANSI (si DEBUG_COLOR activé)                        */
/* ======================================================================== */

#if DEBUG_COLOR

    #define ANSI_RESET              "\033[0m"
    #define ANSI_BOLD               "\033[1m"
    #define ANSI_DIM                "\033[2m"
    #define ANSI_ITALIC             "\033[3m"
    #define ANSI_UNDERLINE          "\033[4m"

    #define ANSI_BLACK              "\033[30m"
    #define ANSI_RED                "\033[31m"
    #define ANSI_GREEN              "\033[32m"
    #define ANSI_YELLOW             "\033[33m"
    #define ANSI_BLUE               "\033[34m"
    #define ANSI_MAGENTA            "\033[35m"
    #define ANSI_CYAN               "\033[36m"
    #define ANSI_WHITE              "\033[37m"

    #define ANSI_BG_BLACK           "\033[40m"
    #define ANSI_BG_RED             "\033[41m"
    #define ANSI_BG_GREEN           "\033[42m"
    #define ANSI_BG_YELLOW          "\033[43m"
    #define ANSI_BG_BLUE            "\033[44m"
    #define ANSI_BG_MAGENTA         "\033[45m"
    #define ANSI_BG_CYAN            "\033[46m"
    #define ANSI_BG_WHITE           "\033[47m"

    /** Couleurs par niveau */
    #define ANSI_ERROR              ANSI_RED ANSI_BOLD
    #define ANSI_WARN               ANSI_YELLOW
    #define ANSI_INFO               ANSI_GREEN
    #define ANSI_DEBUG              ANSI_CYAN
    #define ANSI_VERBOSE            ANSI_DIM ANSI_WHITE

#else

    #define ANSI_RESET              ""
    #define ANSI_BOLD               ""
    #define ANSI_ERROR              ""
    #define ANSI_WARN               ""
    #define ANSI_INFO               ""
    #define ANSI_DEBUG              ""
    #define ANSI_VERBOSE            ""

#endif /* DEBUG_COLOR */

/* ======================================================================== */
/*              MACROS DE MESURE DE PERFORMANCE                              */
/* ======================================================================== */

/**
 * @brief Mesure le temps d'exécution d'un bloc
 *
 * Utilisation :
 *   DEBUG_TIME_START();
 *   // ... code à mesurer ...
 *   DEBUG_TIME_END("MonTraitement");
 */
#if DEBUG_LEVEL >= DEBUG_LEVEL_DEBUG

    #define DEBUG_TIME_START() \
        uint32_t _debug_time_start = DWT->CYCCNT

    #define DEBUG_TIME_END(label) \
        do { \
            uint32_t _elapsed = DWT->CYCCNT - _debug_time_start; \
            float _us = (float)_elapsed / (SystemCoreClock / 1000000.0f); \
            DEBUG_DEBUG("PERF", "%s: %.2f µs (%lu cycles)", label, _us, _elapsed); \
        } while(0)

#else

    #define DEBUG_TIME_START()              ((void)0)
    #define DEBUG_TIME_END(label)           ((void)0)

#endif

/**
 * @brief Compteur de performance simple
 *
 * Incrémente un compteur à chaque appel. Utile pour
 * compter les appels de fonction, interruptions, etc.
 */
#if DEBUG_LEVEL >= DEBUG_LEVEL_DEBUG

    #define DEBUG_COUNTER_INC(name) \
        do { \
            static uint32_t _counter_##name = 0; \
            _counter_##name++; \
        } while(0)

    #define DEBUG_COUNTER_GET(name)         (_counter_##name)

#else

    #define DEBUG_COUNTER_INC(name)         ((void)0)
    #define DEBUG_COUNTER_GET(name)         0

#endif

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */