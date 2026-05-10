/**
 * @file    app_watchdog.cpp
 * @brief   Implémentation du chien de garde (Watchdog)
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente la surveillance du système via IWDG et WWDG.
 * 
 * FONCTIONNEMENT DÉTAILLÉ :
 * 
 * 1. INITIALISATION :
 *    - Configuration IWDG : LSI 32 kHz, prescaler 256 → 125 Hz
 *    - Configuration WWDG : APB1 45 MHz, prescaler 8 → 1373 Hz
 *    - Activation des registres backup (BKP) pour diagnostic
 *    - Vérification de la cause du dernier reset
 * 
 * 2. RAFRAÎCHISSEMENT :
 *    - Appelé toutes les 5 secondes par la tâche Watchdog
 *    - IWDG : simple écriture 0xAAAA dans KR
 *    - WWDG : écriture du compteur dans CR, doit être dans [window, 0x7F]
 *    - Incrémentation du compteur de rafraîchissements
 * 
 * 3. DIAGNOSTIC POST-MORTEM :
 *    - Les registres backup (BKP_DR0-4) stockent :
 *      - Cause du reset
 *      - Dernier flag de diagnostic
 *      - Timestamp
 *      - Compteur de resets
 *      - Checksum de validation
 *    - Ces données survivent au reset (mais pas au power-off)
 *    - Analysées au démarrage pour comprendre la cause du crash
 * 
 * 4. SAUVEGARDE PRÉ-RESET :
 *    - Quand le WWDG Early Wakeup Interrupt (EWI) se déclenche :
 *      1. Sauvegarde des paramètres en flash
 *      2. Écriture du contexte dans les registres backup
 *      3. Log de l'erreur
 *      4. Attente du reset
 * 
 * 5. STRATÉGIE ANTI-BLOCAGE :
 *    - Chaque tâche critique définit un flag de diagnostic
 *    - Si le flag n'est pas mis à jour dans le timeout → reset
 *    - Permet de détecter les blocages dans des sections spécifiques
 * 
 * REGISTRES UTILISÉS :
 * 
 *   IWDG_KR   : 0x40003000  (Key Register)
 *   IWDG_PR   : 0x40003004  (Prescaler)
 *   IWDG_RLR  : 0x40003008  (Reload)
 *   IWDG_SR   : 0x4000300C  (Status)
 * 
 *   WWDG_CR   : 0x40002C00  (Control)
 *   WWDG_CFR  : 0x40002C04  (Configuration)
 *   WWDG_SR   : 0x40002C08  (Status)
 * 
 *   BKP_DR0   : 0x40002850  (Backup Register 0)
 *   ...       : ...
 *   BKP_DR4   : 0x40002860  (Backup Register 4)
 * 
 *   RCC_CSR   : 0x40023874  (Control/Status Register - flags reset)
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "app_watchdog.h"

/* HAL */
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_iwdg.h"
#include "stm32f4xx_hal_wwdg.h"
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_pwr.h"

/* Utilitaires */
#include "../utils/debug_utils.h"

/* Standard */
#include <string.h>
#include <stdio.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs */
#define TAG                                 "Watchdog"

/** Adresses des registres backup */
#define BKP_BASE                            0x40002850
#define BKP_DR0                             (*(__IO uint32_t*)(BKP_BASE + 0x00))
#define BKP_DR1                             (*(__IO uint32_t*)(BKP_BASE + 0x04))
#define BKP_DR2                             (*(__IO uint32_t*)(BKP_BASE + 0x08))
#define BKP_DR3                             (*(__IO uint32_t*)(BKP_BASE + 0x0C))
#define BKP_DR4                             (*(__IO uint32_t*)(BKP_BASE + 0x10))

/** Magic number pour validation des registres backup */
#define BKP_VALID_MAGIC                     0xDEADBEEF

/** Checksum simple pour validation */
#define BKP_CHECKSUM(data)                  ((data) ^ 0xAAAAAAAA)

/** Flags de reset dans RCC_CSR */
#define RCC_CSR_IWDGRSTF                    (1 << 29)  /**< IWDG reset flag    */
#define RCC_CSR_WWDGRSTF                    (1 << 30)  /**< WWDG reset flag    */
#define RCC_CSR_LPWRRSTF                    (1 << 26)  /**< Low-power reset    */
#define RCC_CSR_PINRSTF                     (1 << 25)  /**< Pin reset flag     */
#define RCC_CSR_PORRSTF                     (1 << 24)  /**< Power-on reset     */
#define RCC_CSR_SFTRSTF                     (1 << 23)  /**< Software reset     */

/* ======================================================================== */
/*                VARIABLE GLOBALE                                          */
/* ======================================================================== */

/**
 * @brief Instance globale du chien de garde
 */
AppWatchdog_t g_watchdog;

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

static void detect_reset_cause(AppWatchdog_t* wd);
static void save_to_backup_registers(AppWatchdog_t* wd);
static void load_from_backup_registers(AppWatchdog_t* wd);
static bool validate_backup_registers(void);
static void enable_backup_domain(void);
static uint32_t calculate_iwdg_reload(uint32_t timeout_sec);
static uint8_t calculate_iwdg_prescaler(uint32_t timeout_sec);
static uint8_t calculate_wwdg_prescaler(uint32_t timeout_ms);
static uint8_t calculate_wwdg_window(uint32_t window_ms);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise le chien de garde
 */
void AppWatchdog_Init(uint32_t timeout_sec)
{
    DEBUG_INFO(TAG, "Initialisation du chien de garde...");

    /* Mise à zéro */
    memset(&g_watchdog, 0, sizeof(AppWatchdog_t));

    /* Timeout par défaut */
    if (timeout_sec == 0) {
        timeout_sec = APP_WATCHDOG_DEFAULT_IWDG_TIMEOUT_SEC;
    }

    /* Activer le domaine backup (pour registres BKP) */
    enable_backup_domain();

    /* Détecter la cause du dernier reset */
    detect_reset_cause(&g_watchdog);

    /* Charger les données de diagnostic post-mortem */
    if (validate_backup_registers()) {
        load_from_backup_registers(&g_watchdog);

        if (g_watchdog.last_reset_cause == APP_WATCHDOG_RESET_IWDG ||
            g_watchdog.last_reset_cause == APP_WATCHDOG_RESET_WWDG) {
            DEBUG_WARN(TAG, "Reset détecté: %s (flag=%s)",
                       AppWatchdog_GetResetCauseName(g_watchdog.last_reset_cause),
                       AppWatchdog_GetDiagFlagName(g_watchdog.diag_flag));

            /* Appeler le callback post-reset */
            if (g_watchdog.on_reset_by_watchdog) {
                g_watchdog.on_reset_by_watchdog(g_watchdog.last_reset_cause);
            }
        }
    }

    /* Effacer les flags de reset RCC */
    __HAL_RCC_CLEAR_RESET_FLAGS();

    /* Effacer les registres backup */
    AppWatchdog_ClearDiagFlags();

    /* Configuration par défaut */
    g_watchdog.iwdg_enabled = false;
    g_watchdog.wwdg_enabled = false;
    g_watchdog.state = APP_WATCHDOG_STATE_STOPPED;
    g_watchdog.last_refresh_ms = 0;
    g_watchdog.refresh_count = 0;

    /* Configurer IWDG */
    AppWatchdog_ConfigureIWDG(timeout_sec);

    /* Configurer WWDG */
    AppWatchdog_ConfigureWWDG(APP_WATCHDOG_DEFAULT_WWDG_WINDOW_MS,
                              APP_WATCHDOG_DEFAULT_WWDG_TIMEOUT_MS);

    DEBUG_INFO(TAG, "Chien de garde initialisé (IWDG=%lus, WWDG=%lums/%lums)",
               timeout_sec,
               APP_WATCHDOG_DEFAULT_WWDG_WINDOW_MS,
               APP_WATCHDOG_DEFAULT_WWDG_TIMEOUT_MS);
}

/**
 * @brief Configure l'IWDG
 */
bool AppWatchdog_ConfigureIWDG(uint32_t timeout_sec)
{
    /* Limiter le timeout */
    if (timeout_sec < 1) timeout_sec = 1;
    if (timeout_sec > 32) timeout_sec = 32;

    /* Calculer prescaler et reload */
    uint8_t prescaler = calculate_iwdg_prescaler(timeout_sec);
    uint16_t reload = calculate_iwdg_reload(timeout_sec);

    /* Configurer le handle HAL */
    g_watchdog.iwdg_handle.Instance = IWDG;
    g_watchdog.iwdg_handle.Init.Prescaler = prescaler;
    g_watchdog.iwdg_handle.Init.Reload = reload;

    g_watchdog.iwdg_timeout_sec = timeout_sec;

    DEBUG_INFO(TAG, "IWDG configuré: timeout=%lus, prescaler=%d, reload=%d",
               timeout_sec, prescaler, reload);

    return true;
}

/**
 * @brief Configure le WWDG
 */
bool AppWatchdog_ConfigureWWDG(uint32_t window_ms, uint32_t timeout_ms)
{
    if (window_ms >= timeout_ms) {
        DEBUG_ERROR(TAG, "WWDG: fenêtre (%lums) doit être < timeout (%lums)",
                    window_ms, timeout_ms);
        return false;
    }

    /* Limiter */
    if (timeout_ms < 1) timeout_ms = 1;
    if (timeout_ms > 100) timeout_ms = 100;
    if (window_ms < 1) window_ms = 1;

    /* Calculer le prescaler */
    uint8_t prescaler = calculate_wwdg_prescaler(timeout_ms);

    /* Calculer la valeur de reload (compteur 7 bits : 0x40-0x7F) */
    uint8_t reload = 0x7F;  /* Valeur maximale */
    uint8_t window = calculate_wwdg_window(window_ms);

    /* Configurer le handle HAL */
    g_watchdog.wwdg_handle.Instance = WWDG;
    g_watchdog.wwdg_handle.Init.Prescaler = prescaler;
    g_watchdog.wwdg_handle.Init.Window = window;
    g_watchdog.wwdg_handle.Init.Counter = reload;
    g_watchdog.wwdg_handle.Init.EWIMode = WWDG_EWI_ENABLE;  /* Early Wakeup */

    g_watchdog.wwdg_timeout_ms = timeout_ms;
    g_watchdog.wwdg_window_ms = window_ms;

    DEBUG_INFO(TAG, "WWDG configuré: timeout=%lums, fenêtre=%lums, prescaler=%d",
               timeout_ms, window_ms, prescaler);

    return true;
}

/**
 * @brief Démarre le chien de garde
 */
void AppWatchdog_Start(AppWatchdogType_t type)
{
    DEBUG_INFO(TAG, "Démarrage du chien de garde (type=%d)...", type);

    /* Démarrer IWDG */
    if (type == APP_WATCHDOG_TYPE_IWDG || type == APP_WATCHDOG_TYPE_BOTH) {
        if (!g_watchdog.iwdg_enabled) {
            HAL_IWDG_Init(&g_watchdog.iwdg_handle);
            g_watchdog.iwdg_enabled = true;
            DEBUG_INFO(TAG, "IWDG démarré (timeout=%lus, IRRÉVERSIBLE)",
                       g_watchdog.iwdg_timeout_sec);
        }
    }

    /* Démarrer WWDG */
    if (type == APP_WATCHDOG_TYPE_WWDG || type == APP_WATCHDOG_TYPE_BOTH) {
        if (!g_watchdog.wwdg_enabled) {
            HAL_WWDG_Init(&g_watchdog.wwdg_handle);
            g_watchdog.wwdg_enabled = true;
            DEBUG_INFO(TAG, "WWDG démarré (timeout=%lums, fenêtre=%lums)",
                       g_watchdog.wwdg_timeout_ms, g_watchdog.wwdg_window_ms);
        }
    }

    g_watchdog.state = APP_WATCHDOG_STATE_RUNNING;
    g_watchdog.last_refresh_ms = HAL_GetTick();
}

/**
 * @brief Rafraîchit le chien de garde
 */
bool AppWatchdog_Refresh(void)
{
    bool success = true;

    /* Rafraîchir IWDG */
    if (g_watchdog.iwdg_enabled) {
        success &= AppWatchdog_RefreshIWDG();
    }

    /* Rafraîchir WWDG */
    if (g_watchdog.wwdg_enabled) {
        success &= AppWatchdog_RefreshWWDG();
    }

    if (success) {
        g_watchdog.last_refresh_ms = HAL_GetTick();
        g_watchdog.refresh_count++;
    }

    return success;
}

/**
 * @brief Rafraîchit uniquement l'IWDG
 */
bool AppWatchdog_RefreshIWDG(void)
{
    if (!g_watchdog.iwdg_enabled) return true;

    /* Réarmer le compteur IWDG (écrire 0xAAAA dans KR) */
    HAL_IWDG_Refresh(&g_watchdog.iwdg_handle);

    return true;
}

/**
 * @brief Rafraîchit uniquement le WWDG
 */
bool AppWatchdog_RefreshWWDG(void)
{
    if (!g_watchdog.wwdg_enabled) return true;

    /* Vérifier si on est dans la fenêtre */
    uint8_t current_counter = (uint8_t)(WWDG->CR & 0x7F);
    uint8_t window = g_watchdog.wwdg_handle.Init.Window;

    if (current_counter > window) {
        /* Trop tôt ! On est avant la fenêtre */
        DEBUG_WARN(TAG, "WWDG rafraîchi trop tôt! (compteur=0x%02X, fenêtre=0x%02X)",
                   current_counter, window);
        return false;
    }

    /* Réarmer le compteur */
    HAL_WWDG_Refresh(&g_watchdog.wwdg_handle);

    return true;
}

/**
 * @brief Vérifie la cause du dernier reset
 */
AppWatchdogResetCause_t AppWatchdog_GetResetCause(void)
{
    return g_watchdog.last_reset_cause;
}

/**
 * @brief Vérifie si reset par watchdog
 */
bool AppWatchdog_WasResetByWatchdog(void)
{
    return g_watchdog.last_reset_cause == APP_WATCHDOG_RESET_IWDG ||
           g_watchdog.last_reset_cause == APP_WATCHDOG_RESET_WWDG;
}

/**
 * @brief Définit le flag de diagnostic
 */
void AppWatchdog_SetDiagFlag(AppWatchdogDiagFlag_t flag)
{
    g_watchdog.diag_flag = flag;
    g_watchdog.diag_timestamp = HAL_GetTick();

    /* Sauvegarder dans les registres backup immédiatement */
    BKP_DR1 = (uint32_t)flag;
    BKP_DR2 = g_watchdog.diag_timestamp;
    BKP_DR4 = BKP_CHECKSUM(BKP_DR0 ^ BKP_DR1 ^ BKP_DR2 ^ BKP_DR3);
}

/**
 * @brief Récupère le dernier flag de diagnostic
 */
AppWatchdogDiagFlag_t AppWatchdog_GetDiagFlag(void)
{
    return g_watchdog.diag_flag;
}

/**
 * @brief Efface les flags de diagnostic
 */
void AppWatchdog_ClearDiagFlags(void)
{
    BKP_DR0 = 0;
    BKP_DR1 = 0;
    BKP_DR2 = 0;
    BKP_DR3 = 0;
    BKP_DR4 = 0;

    g_watchdog.diag_flag = APP_WATCHDOG_DIAG_IDLE;
    g_watchdog.diag_timestamp = 0;
    g_watchdog.reset_count = 0;
}

/**
 * @brief Sauvegarde le contexte avant reset
 */
void AppWatchdog_SaveContext(void)
{
    DEBUG_WARN(TAG, "Sauvegarde du contexte pré-reset...");

    /* Sauvegarder dans les registres backup */
    save_to_backup_registers(&g_watchdog);

    /* Journaliser le contexte */
    snprintf(g_watchdog.diag_buffer, sizeof(g_watchdog.diag_buffer),
             "WDTOUT: cause=%s, flag=%s, refresh=%lu, uptime=%lums",
             AppWatchdog_GetResetCauseName(g_watchdog.last_reset_cause),
             AppWatchdog_GetDiagFlagName(g_watchdog.diag_flag),
             g_watchdog.refresh_count,
             HAL_GetTick());

    DEBUG_ERROR(TAG, "%s", g_watchdog.diag_buffer);
}

/**
 * @brief Récupère le contexte sauvegardé
 */
const char* AppWatchdog_GetSavedContext(void)
{
    if (strlen(g_watchdog.diag_buffer) == 0) {
        return "Aucun contexte sauvegardé";
    }
    return g_watchdog.diag_buffer;
}

/**
 * @brief Définit le callback d'avertissement
 */
void AppWatchdog_SetWarningCallback(void (*callback)(uint32_t remaining_ms))
{
    g_watchdog.on_warning = callback;
}

/**
 * @brief Définit le callback de reset imminent
 */
void AppWatchdog_SetResetCallback(void (*callback)(void))
{
    g_watchdog.on_reset_imminent = callback;
}

/**
 * @brief Définit le callback post-reset
 */
void AppWatchdog_SetPostResetCallback(void (*callback)(AppWatchdogResetCause_t cause))
{
    g_watchdog.on_reset_by_watchdog = callback;
}

/**
 * @brief Récupère le temps restant IWDG
 */
uint32_t AppWatchdog_GetIWDGRemainingTime(void)
{
    if (!g_watchdog.iwdg_enabled) return 0;

    /* Lire le compteur actuel */
    uint16_t current_counter = (uint16_t)(IWDG->RLR);  /* Approximation */

    /* Calculer le temps restant */
    uint32_t prescaler_div;
    switch (g_watchdog.iwdg_handle.Init.Prescaler) {
        case IWDG_PRESCALER_4:   prescaler_div = 4;   break;
        case IWDG_PRESCALER_8:   prescaler_div = 8;   break;
        case IWDG_PRESCALER_16:  prescaler_div = 16;  break;
        case IWDG_PRESCALER_32:  prescaler_div = 32;  break;
        case IWDG_PRESCALER_64:  prescaler_div = 64;  break;
        case IWDG_PRESCALER_128: prescaler_div = 128; break;
        case IWDG_PRESCALER_256: prescaler_div = 256; break;
        default: prescaler_div = 256; break;
    }

    /* Fréquence LSI = 32000 Hz */
    uint32_t freq = 32000 / prescaler_div;
    uint32_t remaining_ms = (current_counter * 1000) / freq;

    return remaining_ms;
}

/**
 * @brief Récupère le temps restant WWDG
 */
uint32_t AppWatchdog_GetWWDGRemainingTime(void)
{
    if (!g_watchdog.wwdg_enabled) return 0;

    uint8_t counter = (uint8_t)(WWDG->CR & 0x7F);

    /* APB1 = 45 MHz, prescaler = 4096 * 2^prescaler */
    uint32_t prescaler_div = 4096 * (1 << g_watchdog.wwdg_handle.Init.Prescaler);
    uint32_t freq = 45000000 / prescaler_div;
    uint32_t remaining_ms = ((counter - 0x3F) * 1000) / freq;

    return remaining_ms;
}

/**
 * @brief Récupère l'état
 */
AppWatchdogState_t AppWatchdog_GetState(void)
{
    return g_watchdog.state;
}

/**
 * @brief Récupère le nombre de rafraîchissements
 */
uint32_t AppWatchdog_GetRefreshCount(void)
{
    return g_watchdog.refresh_count;
}

/**
 * @brief Teste le watchdog (reset volontaire)
 */
void AppWatchdog_TestReset(void)
{
    DEBUG_WARN(TAG, "TEST RESET WATCHDOG - Reset dans 1 seconde...");

    /* Sauvegarder le contexte */
    AppWatchdog_SaveContext();

    /* Désactiver les interruptions */
    __disable_irq();

    /* Attendre le reset IWDG (ne pas rafraîchir) */
    while (1) {
        /* Boucle infinie → IWDG timeout → RESET */
    }
}

/**
 * @brief Désactive le watchdog (WWDG seulement)
 */
void AppWatchdog_Deinit(void)
{
    /* L'IWDG ne peut pas être désactivé */

    if (g_watchdog.wwdg_enabled) {
        HAL_WWDG_DeInit(&g_watchdog.wwdg_handle);
        g_watchdog.wwdg_enabled = false;
    }

    g_watchdog.state = APP_WATCHDOG_STATE_STOPPED;
}

/* ======================================================================== */
/*              DIAGNOSTIC POST-MORTEM                                      */
/* ======================================================================== */

/**
 * @brief Imprime un rapport de diagnostic
 */
void AppWatchdog_PrintDiagnostic(void)
{
    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  DIAGNOSTIC WATCHDOG");
    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  Cause reset : %s",
               AppWatchdog_GetResetCauseName(g_watchdog.last_reset_cause));
    DEBUG_INFO(TAG, "  Flag diag   : %s",
               AppWatchdog_GetDiagFlagName(g_watchdog.diag_flag));
    DEBUG_INFO(TAG, "  Timestamp   : %lu ms", g_watchdog.diag_timestamp);
    DEBUG_INFO(TAG, "  Nb resets   : %lu", g_watchdog.reset_count);
    DEBUG_INFO(TAG, "  Refresh cnt : %lu", g_watchdog.refresh_count);
    DEBUG_INFO(TAG, "  IWDG actif  : %s", g_watchdog.iwdg_enabled ? "OUI" : "NON");
    DEBUG_INFO(TAG, "  WWDG actif  : %s", g_watchdog.wwdg_enabled ? "OUI" : "NON");

    if (strlen(g_watchdog.diag_buffer) > 0) {
        DEBUG_INFO(TAG, "  Contexte    : %s", g_watchdog.diag_buffer);
    }
    DEBUG_INFO(TAG, "========================================");
}

/**
 * @brief Nom lisible d'une cause de reset
 */
const char* AppWatchdog_GetResetCauseName(AppWatchdogResetCause_t cause)
{
    switch (cause) {
        case APP_WATCHDOG_RESET_NONE:     return "Aucun";
        case APP_WATCHDOG_RESET_IWDG:     return "IWDG (Independant)";
        case APP_WATCHDOG_RESET_WWDG:     return "WWDG (Fenetre)";
        case APP_WATCHDOG_RESET_SOFTWARE: return "Logiciel (NVIC)";
        case APP_WATCHDOG_RESET_POWER:    return "Mise sous tension";
        case APP_WATCHDOG_RESET_PIN:      return "Broche NRST";
        case APP_WATCHDOG_RESET_UNKNOWN:  return "Inconnu";
        default:                          return "???";
    }
}

/**
 * @brief Nom lisible d'un flag de diagnostic
 */
const char* AppWatchdog_GetDiagFlagName(AppWatchdogDiagFlag_t flag)
{
    switch (flag) {
        case APP_WATCHDOG_DIAG_IDLE:            return "IDLE";
        case APP_WATCHDOG_DIAG_MAIN_LOOP:       return "MAIN_LOOP";
        case APP_WATCHDOG_DIAG_UI_TASK:         return "UI_TASK";
        case APP_WATCHDOG_DIAG_LORA_RX:         return "LORA_RX";
        case APP_WATCHDOG_DIAG_LORA_TX:         return "LORA_TX";
        case APP_WATCHDOG_DIAG_AUDIO_IN:        return "AUDIO_IN";
        case APP_WATCHDOG_DIAG_AUDIO_OUT:       return "AUDIO_OUT";
        case APP_WATCHDOG_DIAG_TOUCH_ISR:       return "TOUCH_ISR";
        case APP_WATCHDOG_DIAG_LORA_ISR:        return "LORA_ISR";
        case APP_WATCHDOG_DIAG_FLASH_WRITE:     return "FLASH_WRITE";
        case APP_WATCHDOG_DIAG_SETTINGS_SAVE:   return "SETTINGS_SAVE";
        case APP_WATCHDOG_DIAG_CALL_HANDLER:    return "CALL_HANDLER";
        case APP_WATCHDOG_DIAG_SMS_HANDLER:     return "SMS_HANDLER";
        case APP_WATCHDOG_DIAG_DMA_TRANSFER:    return "DMA_TRANSFER";
        case APP_WATCHDOG_DIAG_SPI_TRANSFER:    return "SPI_TRANSFER";
        case APP_WATCHDOG_DIAG_UNKNOWN:         return "UNKNOWN";
        default:                                return "???";
    }
}

/* ======================================================================== */
/*              FONCTIONS PRIVÉES                                           */
/* ======================================================================== */

/**
 * @brief Détecte la cause du dernier reset
 * 
 * Lit les flags dans RCC_CSR pour déterminer la source du reset.
 */
static void detect_reset_cause(AppWatchdog_t* wd)
{
    if (!wd) return;

    uint32_t csr = RCC->CSR;

    if (csr & RCC_CSR_IWDGRSTF) {
        wd->last_reset_cause = APP_WATCHDOG_RESET_IWDG;
    } else if (csr & RCC_CSR_WWDGRSTF) {
        wd->last_reset_cause = APP_WATCHDOG_RESET_WWDG;
    } else if (csr & RCC_CSR_SFTRSTF) {
        wd->last_reset_cause = APP_WATCHDOG_RESET_SOFTWARE;
    } else if (csr & RCC_CSR_PORRSTF) {
        wd->last_reset_cause = APP_WATCHDOG_RESET_POWER;
    } else if (csr & RCC_CSR_PINRSTF) {
        wd->last_reset_cause = APP_WATCHDOG_RESET_PIN;
    } else {
        wd->last_reset_cause = APP_WATCHDOG_RESET_UNKNOWN;
    }

    DEBUG_INFO(TAG, "Cause reset: %s", AppWatchdog_GetResetCauseName(wd->last_reset_cause));
}

/**
 * @brief Sauvegarde dans les registres backup
 */
static void save_to_backup_registers(AppWatchdog_t* wd)
{
    if (!wd) return;

    BKP_DR0 = (uint32_t)wd->last_reset_cause;
    BKP_DR1 = (uint32_t)wd->diag_flag;
    BKP_DR2 = wd->diag_timestamp;
    BKP_DR3 = wd->reset_count;
    BKP_DR4 = BKP_CHECKSUM(BKP_DR0 ^ BKP_DR1 ^ BKP_DR2 ^ BKP_DR3);
}

/**
 * @brief Charge depuis les registres backup
 */
static void load_from_backup_registers(AppWatchdog_t* wd)
{
    if (!wd) return;

    wd->last_reset_cause = (AppWatchdogResetCause_t)BKP_DR0;
    wd->diag_flag = (AppWatchdogDiagFlag_t)BKP_DR1;
    wd->diag_timestamp = BKP_DR2;
    wd->reset_count = BKP_DR3;
}

/**
 * @brief Valide les registres backup
 */
static bool validate_backup_registers(void)
{
    uint32_t checksum = BKP_CHECKSUM(BKP_DR0 ^ BKP_DR1 ^ BKP_DR2 ^ BKP_DR3);
    return checksum == BKP_DR4;
}

/**
 * @brief Active le domaine backup
 */
static void enable_backup_domain(void)
{
    /* Autoriser l'accès aux registres backup et RTC */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
}

/**
 * @brief Calcule la valeur de reload IWDG
 */
static uint16_t calculate_iwdg_reload(uint32_t timeout_sec)
{
    /* Fréquence LSI = 32 kHz */
    /* Prescaler = 256 → 32000/256 = 125 Hz */
    /* Reload = timeout * 125 */
    return (uint16_t)(timeout_sec * 125);

    /* Limiter à 12 bits (0xFFF) */
    /* return MIN(reload, 0x0FFF); */
}

/**
 * @brief Calcule le prescaler IWDG optimal
 */
static uint8_t calculate_iwdg_prescaler(uint32_t timeout_sec)
{
    /* Pour timeout ≤ 32s, prescaler 256 est optimal */
    if (timeout_sec <= 32) {
        return IWDG_PRESCALER_256;
    }

    /* Jamais atteint car limité à 32s */
    return IWDG_PRESCALER_256;
}

/**
 * @brief Calcule le prescaler WWDG
 */
static uint8_t calculate_wwdg_prescaler(uint32_t timeout_ms)
{
    /* WWDG_PRESCALER_1 = div 1, _2 = div 2, _4 = div 4, _8 = div 8 */
    if (timeout_ms <= 25)  return WWDG_PRESCALER_1;
    if (timeout_ms <= 50)  return WWDG_PRESCALER_2;
    if (timeout_ms <= 75)  return WWDG_PRESCALER_4;
    return WWDG_PRESCALER_8;
}

/**
 * @brief Calcule la fenêtre WWDG
 */
static uint8_t calculate_wwdg_window(uint32_t window_ms)
{
    /* Convertir ms en valeur de compteur (0x40-0x7F) */
    uint32_t prescaler_div = 4096 * (1 << g_watchdog.wwdg_handle.Init.Prescaler);
    uint32_t freq = 45000000 / prescaler_div;

    uint8_t window = (uint8_t)(0x7F - (window_ms * freq / 1000));

    if (window < 0x40) window = 0x40;
    if (window > 0x7F) window = 0x7F;

    return window;
}

/* ======================================================================== */
/*              HANDLER INTERRUPTION WWDG EARLY WAKEUP                       */
/* ======================================================================== */

/**
 * @brief Handler interruption WWDG Early Wakeup
 * 
 * Appelée juste avant un reset WWDG.
 * Dernière chance de sauvegarder les données critiques.
 */
void WWDG_IRQHandler(void)
{
    /* Acquitter l'interruption */
    if (__HAL_WWDG_GET_FLAG(&g_watchdog.wwdg_handle, WWDG_FLAG_EWIF)) {
        __HAL_WWDG_CLEAR_FLAG(&g_watchdog.wwdg_handle, WWDG_FLAG_EWIF);

        DEBUG_ERROR(TAG, "WWDG EARLY WAKEUP - Reset imminent!");

        /* Sauvegarder le contexte */
        AppWatchdog_SaveContext();

        /* Callback avertissement */
        if (g_watchdog.on_warning) {
            uint32_t remaining = AppWatchdog_GetWWDGRemainingTime();
            g_watchdog.on_warning(remaining);
        }

        /* Callback reset imminent */
        if (g_watchdog.on_reset_imminent) {
            g_watchdog.on_reset_imminent();
        }

        /* Sauvegarder les paramètres en flash (dernière action) */
        /* SettingsService_Save(&g_app.settings_service); */
    }
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */