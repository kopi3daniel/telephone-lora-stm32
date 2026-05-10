/**
 * @file    app_watchdog.h
 * @brief   Chien de garde (Watchdog) - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Gère le chien de garde indépendant (IWDG) et le chien de garde
 * fenêtré (WWDG) du STM32F429 pour assurer la fiabilité du système.
 * 
 * DEUX TYPES DE WATCHDOG SUR STM32F429 :
 * 
 * 1. IWDG (Independent Watchdog) :
 *    - Horloge indépendante (oscillateur LSI 32 kHz)
 *    - Fonctionne même si l'horloge système est défaillante
 *    - Timeout configurable : 4 ms à 32 secondes
 *    - Reset matériel complet du MCU
 *    - Registres protégés par clé (0x5555, 0xAAAA, 0xCCCC)
 *    - Une fois activé, NE PEUT PAS être désactivé (sécurité maximale)
 * 
 * 2. WWDG (Window Watchdog) :
 *    - Horloge système (APB1)
 *    - Fenêtre de rafraîchissement : le compteur doit être
 *      réarmé dans une plage précise (ni trop tôt, ni trop tard)
 *    - Interruption Early Wakeup (EWI) avant le reset
 *    - Permet de sauvegarder des données avant le reset
 * 
 * STRATÉGIE DE SURVEILLANCE :
 * 
 * ┌─────────────────────────────────────────────────────────────┐
 * │                     SURVEILLANCE                            │
 * │                                                             │
 * │  ┌──────────┐     ┌──────────┐     ┌──────────┐            │
 * │  │  IWDG    │     │  WWDG    │     │  SOFT    │            │
 * │  │ (Matériel)│    │ (Fenêtré)│     │ (Logiciel)│           │
 * │  └─────┬─────┘     └─────┬─────┘     └─────┬─────┘            │
 * │        │                 │                 │                  │
 * │        ▼                 ▼                 ▼                  │
 * │  ┌─────────────────────────────────────────────────────┐    │
 * │  │  TÂCHE WATCHDOG (app_tasks)                         │    │
 * │  │  Exécutée toutes les 500ms                         │    │
 * │  │                                                     │    │
 * │  │  1. Vérifier boucle principale (super-loop alive)   │    │
 * │  │  2. Vérifier tâches critiques (UI, LoRa, Audio)     │    │
 * │  │  3. Vérifier niveau batterie                        │    │
 * │  │  4. Vérifier stack overflow (si FreeRTOS)           │    │
 * │  │  5. Réarmer IWDG                                    │    │
 * │  │  6. Réarmer WWDG (dans la fenêtre)                  │    │
 * │  └─────────────────────────────────────────────────────┘    │
 * │                                                             │
 * │  SI ANOMALIE DÉTECTÉE :                                     │
 * │    1. Logger l'erreur en flash (pour analyse post-mortem)   │
 * │    2. Sauvegarder les paramètres                            │
 * │    3. Tenter un soft reset                                  │
 * │    4. Si échec → IWDG reset matériel                       │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * CONFIGURATION RECOMMANDÉE :
 * 
 *   IWDG :
 *     Prescaler : 256 (LSI 32 kHz / 256 = 125 Hz)
 *     Reload    : 1250 (1250 / 125 Hz = 10 secondes)
 *     Timeout   : 10 secondes
 * 
 *   WWDG :
 *     Prescaler : 8 (APB1 45 MHz / 4096 / 8 = 1373 Hz)
 *     Window    : 100 (73 ms)
 *     Reload    : 127 (92 ms max)
 *     Timeout   : ~92 ms
 *     Fenêtre   : 73 ms à 92 ms
 * 
 * REGISTRES IWDG (STM32F429) :
 * 
 *   IWDG_KR  (Key Register)        : 0x40003000
 *     - 0x5555 : Déverrouiller PR et RLR
 *     - 0xAAAA : Réarmer (reset counter)
 *     - 0xCCCC : Démarrer le watchdog
 *   IWDG_PR  (Prescaler Register)   : 0x40003004
 *   IWDG_RLR (Reload Register)      : 0x40003008
 *   IWDG_SR  (Status Register)      : 0x4000300C
 * 
 * REGISTRES WWDG (STM32F429) :
 * 
 *   WWDG_CR  (Control Register)     : 0x40002C00
 *   WWDG_CFR (Configuration Register): 0x40002C04
 *   WWDG_SR  (Status Register)      : 0x40002C08
 * 
 * EXEMPLE D'UTILISATION :
 * 
 *   // Initialisation au démarrage
 *   AppWatchdog_Init();
 *   AppWatchdog_ConfigureIWDG(10);  // Timeout 10 secondes
 *   AppWatchdog_ConfigureWWDG(80, 100);  // Fenêtre 80-100ms
 *   AppWatchdog_Start();
 * 
 *   // Dans la boucle principale ou tâche watchdog
 *   AppWatchdog_Refresh();
 * 
 *   // Vérifier si un reset watchdog a eu lieu
 *   if (AppWatchdog_WasResetByWatchdog()) {
 *       DEBUG_ERROR("System was reset by watchdog!");
 *   }
 */

#ifndef APP_WATCHDOG_H
#define APP_WATCHDOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include <stdint.h>
#include <stdbool.h>

/* HAL */
#include "stm32f4xx_hal.h"

/* ======================================================================== */
/*                     CONSTANTES SYMBOLIQUES                                */
/* ======================================================================== */

/**
 * @brief Timeout IWDG par défaut (secondes)
 */
#define APP_WATCHDOG_DEFAULT_IWDG_TIMEOUT_SEC    10

/**
 * @brief Timeout WWDG par défaut (millisecondes)
 */
#define APP_WATCHDOG_DEFAULT_WWDG_TIMEOUT_MS     100

/**
 * @brief Fenêtre WWDG par défaut (millisecondes)
 */
#define APP_WATCHDOG_DEFAULT_WWDG_WINDOW_MS      70

/**
 * @brief Période de rafraîchissement recommandée (ms)
 * 
 * Doit être inférieure au timeout pour éviter un reset.
 * Par sécurité, on rafraîchit à 50% du timeout.
 */
#define APP_WATCHDOG_REFRESH_PERIOD_MS           5000

/**
 * @brief Nombre maximum de flags de diagnostic
 */
#define APP_WATCHDOG_MAX_DIAG_FLAGS              16

/**
 * @brief Taille du buffer pour le diagnostic post-mortem
 */
#define APP_WATCHDOG_DIAG_BUFFER_SIZE             256

/* ======================================================================== */
/*                     TYPES                                                 */
/* ======================================================================== */

/**
 * @brief Type de chien de garde
 */
typedef enum {
    APP_WATCHDOG_TYPE_IWDG = 0,         /**< Independent Watchdog             */
    APP_WATCHDOG_TYPE_WWDG,             /**< Window Watchdog                  */
    APP_WATCHDOG_TYPE_BOTH,             /**< Les deux                         */
} AppWatchdogType_t;

/**
 * @brief État du chien de garde
 */
typedef enum {
    APP_WATCHDOG_STATE_STOPPED = 0,     /**< Arrêté (seulement WWDG)          */
    APP_WATCHDOG_STATE_RUNNING,         /**< En cours d'exécution             */
    APP_WATCHDOG_STATE_WARNING,         /**< Avertissement (proche timeout)   */
    APP_WATCHDOG_STATE_RESET,           /**< Reset imminent                   */
} AppWatchdogState_t;

/**
 * @brief Cause de reset
 */
typedef enum {
    APP_WATCHDOG_RESET_NONE = 0,        /**< Pas de reset watchdog            */
    APP_WATCHDOG_RESET_IWDG,            /**< Reset par IWDG                   */
    APP_WATCHDOG_RESET_WWDG,            /**< Reset par WWDG                   */
    APP_WATCHDOG_RESET_SOFTWARE,        /**< Reset logiciel                   */
    APP_WATCHDOG_RESET_POWER,           /**< Reset mise sous tension          */
    APP_WATCHDOG_RESET_PIN,             /**< Reset par broche NRST            */
    APP_WATCHDOG_RESET_UNKNOWN,         /**< Cause inconnue                   */
} AppWatchdogResetCause_t;

/**
 * @brief Flag de diagnostic
 * 
 * Permet de savoir où le système était avant un reset watchdog.
 * Chaque section critique du code définit un flag différent.
 */
typedef enum {
    APP_WATCHDOG_DIAG_IDLE = 0,         /**< Inactif (pas de diagnostic)      */
    APP_WATCHDOG_DIAG_MAIN_LOOP,        /**< Boucle principale                */
    APP_WATCHDOG_DIAG_UI_TASK,          /**< Tâche UI                        */
    APP_WATCHDOG_DIAG_LORA_RX,          /**< Réception LoRa                   */
    APP_WATCHDOG_DIAG_LORA_TX,          /**< Transmission LoRa                */
    APP_WATCHDOG_DIAG_AUDIO_IN,         /**< Capture audio                    */
    APP_WATCHDOG_DIAG_AUDIO_OUT,        /**< Lecture audio                    */
    APP_WATCHDOG_DIAG_TOUCH_ISR,        /**< ISR tactile                     */
    APP_WATCHDOG_DIAG_LORA_ISR,         /**< ISR LoRa                        */
    APP_WATCHDOG_DIAG_FLASH_WRITE,      /**< Écriture flash                   */
    APP_WATCHDOG_DIAG_SETTINGS_SAVE,    /**< Sauvegarde paramètres            */
    APP_WATCHDOG_DIAG_CALL_HANDLER,     /**< Gestionnaire d'appel             */
    APP_WATCHDOG_DIAG_SMS_HANDLER,      /**< Gestionnaire SMS                 */
    APP_WATCHDOG_DIAG_DMA_TRANSFER,     /**< Transfert DMA                    */
    APP_WATCHDOG_DIAG_SPI_TRANSFER,     /**< Transfert SPI                    */
    APP_WATCHDOG_DIAG_UNKNOWN,          /**< Inconnu                          */
} AppWatchdogDiagFlag_t;

/* ======================================================================== */
/*                     STRUCTURES                                            */
/* ======================================================================== */

/**
 * @brief Configuration du chien de garde
 */
typedef struct {
    /* ---- IWDG ---- */
    bool                iwdg_enabled;       /**< IWDG activé                   */
    uint32_t            iwdg_timeout_sec;   /**< Timeout IWDG (secondes)       */
    IWDG_HandleTypeDef  iwdg_handle;        /**< Handle HAL IWDG               */

    /* ---- WWDG ---- */
    bool                wwdg_enabled;       /**< WWDG activé                   */
    uint32_t            wwdg_timeout_ms;    /**< Timeout WWDG (ms)             */
    uint32_t            wwdg_window_ms;     /**< Fenêtre WWDG (ms)             */
    WWDG_HandleTypeDef  wwdg_handle;        /**< Handle HAL WWDG               */

    /* ---- État ---- */
    AppWatchdogState_t  state;              /**< État actuel                   */
    uint32_t            last_refresh_ms;    /**< Dernier rafraîchissement      */
    uint32_t            refresh_count;      /**< Nombre de rafraîchissements   */

    /* ---- Diagnostic ---- */
    AppWatchdogDiagFlag_t diag_flag;        /**< Flag de diagnostic courant    */
    uint32_t            diag_timestamp;     /**< Timestamp du flag             */
    uint32_t            reset_count;        /**< Nombre de resets watchdog     */
    AppWatchdogResetCause_t last_reset_cause;/**< Dernière cause de reset      */
    char                diag_buffer[APP_WATCHDOG_DIAG_BUFFER_SIZE]; /**< Buffer */

    /* ---- Callbacks ---- */
    void (*on_warning)(uint32_t remaining_ms);  /**< Avertissement             */
    void (*on_reset_imminent)(void);             /**< Reset imminent           */
    void (*on_reset_by_watchdog)(AppWatchdogResetCause_t cause); /**< Post-reset */

} AppWatchdog_t;

/* ======================================================================== */
/*              VARIABLE GLOBALE                                             */
/* ======================================================================== */

/**
 * @brief Instance globale du chien de garde
 */
extern AppWatchdog_t g_watchdog;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/**
 * @brief Initialise le chien de garde
 * 
 * Configure les deux watchdogs (IWDG et WWDG) avec les
 * paramètres par défaut mais ne les démarre pas encore.
 * 
 * @param timeout_sec   Timeout IWDG en secondes (0 = défaut 10s)
 */
void AppWatchdog_Init(uint32_t timeout_sec);

/**
 * @brief Configure l'IWDG
 * 
 * @param timeout_sec   Timeout en secondes (4 ms à 32 s)
 * @return              true si configuration réussie
 */
bool AppWatchdog_ConfigureIWDG(uint32_t timeout_sec);

/**
 * @brief Configure le WWDG
 * 
 * @param window_ms     Fenêtre minimale en ms
 * @param timeout_ms    Timeout maximal en ms
 * @return              true si configuration réussie
 */
bool AppWatchdog_ConfigureWWDG(uint32_t window_ms, uint32_t timeout_ms);

/**
 * @brief Démarre le chien de garde
 * 
 * Active l'IWDG (irréversible) et/ou le WWDG.
 * Une fois l'IWDG démarré, il ne peut plus être arrêté.
 * 
 * @param type      Type(s) à démarrer
 */
void AppWatchdog_Start(AppWatchdogType_t type);

/**
 * @brief Rafraîchit le chien de garde (réarme le compteur)
 * 
 * DOIT être appelé régulièrement (période < timeout).
 * Pour l'IWDG : simple réarmement.
 * Pour le WWDG : doit être dans la fenêtre de temps.
 * 
 * @return          true si rafraîchi avec succès
 */
bool AppWatchdog_Refresh(void);

/**
 * @brief Rafraîchit uniquement l'IWDG
 * @return          true si succès
 */
bool AppWatchdog_RefreshIWDG(void);

/**
 * @brief Rafraîchit uniquement le WWDG
 * @return          true si dans la fenêtre, false sinon
 */
bool AppWatchdog_RefreshWWDG(void);

/**
 * @brief Vérifie si le dernier reset a été causé par le watchdog
 * @return          Cause du reset
 */
AppWatchdogResetCause_t AppWatchdog_GetResetCause(void);

/**
 * @brief Vérifie si le dernier reset était dû au watchdog
 * @return          true si reset par IWDG ou WWDG
 */
bool AppWatchdog_WasResetByWatchdog(void);

/**
 * @brief Définit le flag de diagnostic courant
 * 
 * Permet de tracer où le système se trouvait avant un reset.
 * Le flag est stocké en mémoire backup (survit au reset).
 * 
 * @param flag      Flag de diagnostic
 */
void AppWatchdog_SetDiagFlag(AppWatchdogDiagFlag_t flag);

/**
 * @brief Récupère le dernier flag de diagnostic
 * @return          Dernier flag avant reset
 */
AppWatchdogDiagFlag_t AppWatchdog_GetDiagFlag(void);

/**
 * @brief Efface les flags de diagnostic
 */
void AppWatchdog_ClearDiagFlags(void);

/**
 * @brief Sauvegarde le contexte avant un reset
 * 
 * Stocke les informations critiques en mémoire backup
 * pour analyse post-mortem.
 */
void AppWatchdog_SaveContext(void);

/**
 * @brief Récupère le contexte sauvegardé
 * @return          Chaîne descriptive du dernier contexte
 */
const char* AppWatchdog_GetSavedContext(void);

/**
 * @brief Définit le callback d'avertissement
 * 
 * Appelé quand le temps restant avant reset est critique.
 * 
 * @param callback  Fonction à appeler
 */
void AppWatchdog_SetWarningCallback(void (*callback)(uint32_t remaining_ms));

/**
 * @brief Définit le callback de reset imminent
 * @param callback  Fonction à appeler juste avant le reset
 */
void AppWatchdog_SetResetCallback(void (*callback)(void));

/**
 * @brief Définit le callback post-reset
 * @param callback  Fonction à appeler après un reset watchdog
 */
void AppWatchdog_SetPostResetCallback(void (*callback)(AppWatchdogResetCause_t cause));

/**
 * @brief Récupère le temps restant avant timeout IWDG
 * @return          Temps restant en millisecondes
 */
uint32_t AppWatchdog_GetIWDGRemainingTime(void);

/**
 * @brief Récupère le temps restant avant timeout WWDG
 * @return          Temps restant en millisecondes
 */
uint32_t AppWatchdog_GetWWDGRemainingTime(void);

/**
 * @brief Récupère l'état actuel
 * @return          État du watchdog
 */
AppWatchdogState_t AppWatchdog_GetState(void);

/**
 * @brief Récupère le nombre de rafraîchissements
 * @return          Compteur de rafraîchissements
 */
uint32_t AppWatchdog_GetRefreshCount(void);

/**
 * @brief Teste le watchdog (provoque un reset volontaire)
 * 
 * ⚠️ DANGEREUX : Provoque un reset du système.
 * Utilisé uniquement pour les tests.
 */
void AppWatchdog_TestReset(void);

/**
 * @brief Désactive le WWDG (uniquement possible avant Start)
 * 
 * Note : L'IWDG NE PEUT PAS être désactivé une fois démarré.
 */
void AppWatchdog_Deinit(void);

/* ======================================================================== */
/*              MACROS UTILITAIRES                                           */
/* ======================================================================== */

/**
 * @brief Définit le flag de diagnostic (macro pratique)
 * 
 * Utilisation :
 *   WATCHDOG_DIAG(APP_WATCHDOG_DIAG_MAIN_LOOP);
 *   // ... code critique ...
 *   WATCHDOG_DIAG(APP_WATCHDOG_DIAG_IDLE);
 */
#define WATCHDOG_DIAG(flag)                 AppWatchdog_SetDiagFlag(flag)

/**
 * @brief Rafraîchit le watchdog (macro pratique)
 */
#define WATCHDOG_REFRESH()                  AppWatchdog_Refresh()

/**
 * @brief Vérifie si le reset était dû au watchdog
 */
#define WATCHDOG_WAS_RESET()                AppWatchdog_WasResetByWatchdog()

/* ======================================================================== */
/*              FONCTIONS DE DIAGNOSTIC POST-MORTEM                          */
/* ======================================================================== */

/**
 * @brief Imprime un rapport de diagnostic sur la console
 * 
 * Affiche la cause du dernier reset, le flag de diagnostic,
 * et les informations de contexte sauvegardées.
 */
void AppWatchdog_PrintDiagnostic(void);

/**
 * @brief Récupère le nom lisible d'une cause de reset
 * @param cause     Cause
 * @return          Chaîne statique
 */
const char* AppWatchdog_GetResetCauseName(AppWatchdogResetCause_t cause);

/**
 * @brief Récupère le nom lisible d'un flag de diagnostic
 * @param flag      Flag
 * @return          Chaîne statique
 */
const char* AppWatchdog_GetDiagFlagName(AppWatchdogDiagFlag_t flag);

/* ======================================================================== */
/*              REGISTRES BACKUP (sauvegarde post-reset)                     */
/* ======================================================================== */

/*
 * Les registres de backup (BKP) du STM32F429 sont préservés
 * pendant un reset (mais pas pendant une coupure d'alimentation).
 * 
 * Registres utilisés :
 *   BKP_DR0  : Cause du reset (AppWatchdogResetCause_t)
 *   BKP_DR1  : Flag de diagnostic (AppWatchdogDiagFlag_t)
 *   BKP_DR2  : Timestamp du flag
 *   BKP_DR3  : Compteur de resets watchdog
 *   BKP_DR4  : Checksum pour validation
 * 
 * Adresses :
 *   BKP_BASE : 0x40002800
 *   BKP_DR0  : BKP_BASE + 0x00
 *   BKP_DR1  : BKP_BASE + 0x04
 *   BKP_DR2  : BKP_BASE + 0x08
 *   BKP_DR3  : BKP_BASE + 0x0C
 *   BKP_DR4  : BKP_BASE + 0x10
 */

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */