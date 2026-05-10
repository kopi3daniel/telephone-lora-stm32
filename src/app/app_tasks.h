/**
 * @file    app_tasks.h
 * @brief   Gestionnaire de tâches - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Gère les tâches concurrentes de l'application.
 * 
 * DEUX MODES DE FONCTIONNEMENT :
 * 
 * 1. MODE SUPER-LOOP (sans FreeRTOS) :
 *    - Toutes les "tâches" sont exécutées séquentiellement
 *    - Dans la boucle principale de PhoneApp_Run()
 *    - Pas de préemption, coopératif uniquement
 *    - Adapté aux applications simples, économise la RAM
 * 
 * 2. MODE FreeRTOS (avec OS) :
 *    - Chaque tâche a sa propre pile et priorité
 *    - Préemption basée sur les priorités FreeRTOS
 *    - Communication par files de messages et sémaphores
 *    - Adapté aux applications complexes
 * 
 * ARCHITECTURE DES TÂCHES (Mode Super-Loop) :
 * 
 *   Boucle principale (10ms)
 *   │
 *   ├── Task_UI()           → Rafraîchir l'écran actif
 *   ├── Task_Touch()        → Scanner l'écran tactile
 *   ├── Task_Keys()         → Scanner le clavier matriciel
 *   ├── Task_LoRa()         → Vérifier les paquets reçus
 *   ├── Task_Audio()        → Traiter les buffers audio
 *   ├── Task_Phone()        → Gérer les appels en cours
 *   ├── Task_Battery()      → Surveiller la batterie
 *   └── Task_Watchdog()     → Réarmer le chien de garde
 * 
 * ARCHITECTURE DES TÂCHES (Mode FreeRTOS) :
 * 
 *   Task_UI          Priorité 2  Stack 4096  Core 0
 *   Task_Touch       Priorité 3  Stack 1024  Core 0
 *   Task_Keys        Priorité 3  Stack 1024  Core 0
 *   Task_LoRa        Priorité 4  Stack 2048  Core 1
 *   Task_Audio       Priorité 4  Stack 2048  Core 1
 *   Task_Phone       Priorité 3  Stack 1536  Core 0
 *   Task_Battery     Priorité 1  Stack 512   Core 0
 *   Task_Watchdog    Priorité 5  Stack 512   Core 0
 *   Task_Idle        Priorité 0  Stack 256   Core 0/1
 * 
 * STATISTIQUES DES TÂCHES :
 * 
 *   Chaque tâche collecte :
 *   - Temps d'exécution total
 *   - Temps d'exécution max (pire cas)
 *   - Nombre d'exécutions
 *   - Charge CPU moyenne (%)
 * 
 *   Utile pour l'optimisation et le débogage.
 * 
 * EXEMPLE D'UTILISATION :
 * 
 *   // En mode super-loop (sans FreeRTOS)
 *   AppTasks_Init(NULL);
 *   while (1) {
 *       AppTasks_RunAll();
 *       HAL_Delay(10);
 *   }
 * 
 *   // En mode FreeRTOS
 *   AppTasks_Init(&g_app);
 *   AppTasks_CreateAll();
 *   vTaskStartScheduler();  // Ne retourne jamais
 */

#ifndef APP_TASKS_H
#define APP_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include <stdint.h>
#include <stdbool.h>

/* Forward declaration */
struct PhoneApp_t;

/* Si FreeRTOS est disponible */
#ifdef INC_FREERTOS_H
    #include "FreeRTOS.h"
    #include "task.h"
    #include "queue.h"
    #include "semphr.h"
    #define APP_TASKS_USE_FREERTOS      1
#else
    #define APP_TASKS_USE_FREERTOS      0
#endif

/* ======================================================================== */
/*                     CONSTANTES SYMBOLIQUES                                */
/* ======================================================================== */

/**
 * @brief Nombre maximum de tâches gérées
 */
#define APP_TASKS_MAX_COUNT                     16

/**
 * @brief Nombre maximum de caractères dans un nom de tâche
 */
#define APP_TASK_NAME_MAX_LENGTH                16

/**
 * @brief Période par défaut des tâches en mode super-loop (ms)
 */
#define APP_TASKS_DEFAULT_PERIOD_MS             10

/* ======================================================================== */
/*                     TYPES                                                 */
/* ======================================================================== */

/**
 * @brief Identifiants des tâches
 */
typedef enum {
    APP_TASK_ID_UI = 0,                     /**< Interface utilisateur (écran)  */
    APP_TASK_ID_TOUCH,                      /**< Scanner tactile                */
    APP_TASK_ID_KEYS,                       /**< Scanner clavier matriciel      */
    APP_TASK_ID_LORA_RX,                    /**< Réception LoRa                 */
    APP_TASK_ID_LORA_TX,                    /**< Transmission LoRa              */
    APP_TASK_ID_AUDIO_IN,                   /**< Capture audio (micro)          */
    APP_TASK_ID_AUDIO_OUT,                  /**< Lecture audio (HP)             */
    APP_TASK_ID_PHONE,                      /**< Gestion téléphonie             */
    APP_TASK_ID_SMS,                        /**< Gestion messages               */
    APP_TASK_ID_BATTERY,                    /**< Surveillance batterie          */
    APP_TASK_ID_WATCHDOG,                   /**< Chien de garde                 */
    APP_TASK_ID_BACKGROUND,                 /**< Tâches de fond (nettoyage)     */
    APP_TASK_ID_CUSTOM_0,                   /**< Tâche personnalisée 0          */
    APP_TASK_ID_CUSTOM_1,                   /**< Tâche personnalisée 1          */
    APP_TASK_ID_CUSTOM_2,                   /**< Tâche personnalisée 2          */
    APP_TASK_ID_COUNT                       /**< Nombre total de tâches         */
} AppTaskId_t;

/**
 * @brief État d'une tâche
 */
typedef enum {
    APP_TASK_STATE_STOPPED = 0,             /**< Arrêtée                        */
    APP_TASK_STATE_RUNNING,                 /**< En cours d'exécution           */
    APP_TASK_STATE_PAUSED,                  /**< En pause                       */
    APP_TASK_STATE_ERROR,                   /**< Erreur                         */
} AppTaskState_t;

/**
 * @brief Priorité d'une tâche (mode super-loop)
 */
typedef enum {
    APP_TASK_PRIORITY_IDLE = 0,             /**< Exécutée si rien d'autre       */
    APP_TASK_PRIORITY_LOW,                  /**< Basse priorité                 */
    APP_TASK_PRIORITY_NORMAL,               /**< Priorité normale               */
    APP_TASK_PRIORITY_HIGH,                 /**< Haute priorité                 */
    APP_TASK_PRIORITY_CRITICAL,             /**< Critique (watchdog)            */
    APP_TASK_PRIORITY_COUNT
} AppTaskPriority_t;

/**
 * @brief Fonction exécutée par une tâche
 * 
 * @param context   Contexte (généralement PhoneApp_t*)
 */
typedef void (*AppTaskFunction_t)(void* context);

/**
 * @brief Statistiques d'une tâche
 */
typedef struct {
    uint32_t    total_executions;           /**< Nombre total d'exécutions      */
    uint32_t    total_time_us;              /**< Temps total en microsecondes   */
    uint32_t    max_time_us;               /**< Temps max d'une exécution      */
    uint32_t    min_time_us;               /**< Temps min d'une exécution      */
    uint32_t    last_execution_ms;          /**< Timestamp dernière exécution   */
    uint32_t    missed_deadlines;           /**< Échéances manquées             */
    float       cpu_load_percent;           /**< Charge CPU estimée (%)         */
    uint32_t    error_count;                /**< Nombre d'erreurs               */
} AppTaskStats_t;

/* ======================================================================== */
/*                     STRUCTURE DE TÂCHE                                   */
/* ======================================================================== */

/**
 * @brief Définition d'une tâche
 */
typedef struct {
    /* ---- Identification ---- */
    AppTaskId_t         id;                 /**< Identifiant unique             */
    char                name[APP_TASK_NAME_MAX_LENGTH]; /**< Nom lisible       */
    AppTaskFunction_t   function;           /**< Fonction à exécuter            */
    void*               context;            /**< Contexte passé à la fonction   */

    /* ---- Configuration ---- */
    AppTaskPriority_t   priority;           /**< Priorité d'exécution           */
    uint32_t            period_ms;          /**< Période en ms (0 = exécution continue) */
    bool                enabled;            /**< Tâche activée                  */
    bool                run_once;           /**< Exécuter une seule fois        */

    /* ---- État ---- */
    AppTaskState_t      state;              /**< État actuel                    */
    uint32_t            next_run_ms;        /**< Prochaine exécution (timestamp) */
    uint32_t            last_run_ms;        /**< Dernière exécution             */
    uint32_t            last_duration_us;   /**< Durée dernière exécution       */

    /* ---- Statistiques ---- */
    AppTaskStats_t      stats;              /**< Statistiques                   */

    /* ---- Mode FreeRTOS (si disponible) ---- */
    #if APP_TASKS_USE_FREERTOS
        TaskHandle_t    rtos_handle;        /**< Handle FreeRTOS                */
        StaticTask_t    rtos_buffer;        /**< Buffer statique                */
        StackType_t*    rtos_stack;         /**< Pile de la tâche               */
        uint32_t        rtos_stack_size;    /**< Taille de la pile              */
        UBaseType_t     rtos_priority;      /**< Priorité FreeRTOS              */
        BaseType_t      rtos_core;          /**< Core assigné (0 ou 1)          */
    #endif

} AppTask_t;

/* ======================================================================== */
/*                     STRUCTURE PRINCIPALE                                  */
/* ======================================================================== */

/**
 * @brief Gestionnaire de tâches
 * 
 * Taille approximative : ~1 Ko + tâches
 */
typedef struct {
    /* ---- Tâches ---- */
    AppTask_t           tasks[APP_TASKS_MAX_COUNT]; /**< Tableau des tâches     */
    uint8_t             task_count;         /**< Nombre de tâches enregistrées  */

    /* ---- État ---- */
    bool                is_running;         /**< Gestionnaire actif             */
    uint32_t            total_cycles;       /**< Nombre de cycles exécutés      */
    uint32_t            cycle_start_ms;     /**< Timestamp début cycle          */
    uint32_t            cycle_duration_us;  /**< Durée du dernier cycle         */
    uint32_t            max_cycle_us;       /**< Pire durée de cycle            */

    /* ---- Callback ---- */
    void (*on_task_error)(AppTaskId_t task_id, uint32_t error_code);

    /* ---- Mode FreeRTOS ---- */
    #if APP_TASKS_USE_FREERTOS
        bool            use_freertos;       /**< Utiliser FreeRTOS              */
    #endif

} AppTaskManager_t;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/**
 * @brief Initialise le gestionnaire de tâches
 * 
 * Enregistre les tâches par défaut (UI, Touch, Keys, LoRa, etc.)
 * avec leurs périodes et priorités.
 * 
 * @param manager   Gestionnaire à initialiser
 * @param app       Contexte applicatif (PhoneApp_t*)
 */
void AppTasks_Init(AppTaskManager_t* manager, struct PhoneApp_t* app);

/**
 * @brief Enregistre une nouvelle tâche
 * 
 * @param manager   Gestionnaire
 * @param id        Identifiant de la tâche
 * @param name      Nom lisible
 * @param function  Fonction à exécuter
 * @param context   Contexte
 * @param period_ms Période en ms (0 = exécution continue)
 * @param priority  Priorité
 * @return          true si enregistrée avec succès
 */
bool AppTasks_Register(AppTaskManager_t* manager,
                       AppTaskId_t id,
                       const char* name,
                       AppTaskFunction_t function,
                       void* context,
                       uint32_t period_ms,
                       AppTaskPriority_t priority);

/**
 * @brief Exécute toutes les tâches éligibles (mode super-loop)
 * 
 * Parcourt les tâches par ordre de priorité et exécute
 * celles dont la période est écoulée.
 * 
 * @param manager   Gestionnaire
 */
void AppTasks_RunAll(AppTaskManager_t* manager);

/**
 * @brief Exécute une tâche spécifique
 * 
 * @param manager   Gestionnaire
 * @param id        Identifiant de la tâche
 */
void AppTasks_RunOne(AppTaskManager_t* manager, AppTaskId_t id);

/**
 * @brief Démarre une tâche
 * @param manager   Gestionnaire
 * @param id        Identifiant
 */
void AppTasks_Start(AppTaskManager_t* manager, AppTaskId_t id);

/**
 * @brief Arrête une tâche
 * @param manager   Gestionnaire
 * @param id        Identifiant
 */
void AppTasks_Stop(AppTaskManager_t* manager, AppTaskId_t id);

/**
 * @brief Met une tâche en pause
 * @param manager   Gestionnaire
 * @param id        Identifiant
 */
void AppTasks_Pause(AppTaskManager_t* manager, AppTaskId_t id);

/**
 * @brief Reprend une tâche en pause
 * @param manager   Gestionnaire
 * @param id        Identifiant
 */
void AppTasks_Resume(AppTaskManager_t* manager, AppTaskId_t id);

/**
 * @brief Active/désactive une tâche
 * @param manager   Gestionnaire
 * @param id        Identifiant
 * @param enabled   true = activer
 */
void AppTasks_SetEnabled(AppTaskManager_t* manager, AppTaskId_t id, bool enabled);

/**
 * @brief Modifie la période d'une tâche
 * @param manager   Gestionnaire
 * @param id        Identifiant
 * @param period_ms Nouvelle période en ms
 */
void AppTasks_SetPeriod(AppTaskManager_t* manager, AppTaskId_t id, uint32_t period_ms);

/**
 * @brief Modifie la priorité d'une tâche
 * @param manager   Gestionnaire
 * @param id        Identifiant
 * @param priority  Nouvelle priorité
 */
void AppTasks_SetPriority(AppTaskManager_t* manager, AppTaskId_t id, AppTaskPriority_t priority);

/**
 * @brief Récupère une tâche par son ID
 * @param manager   Gestionnaire
 * @param id        Identifiant
 * @return          Pointeur vers la tâche, NULL si non trouvée
 */
AppTask_t* AppTasks_GetTask(AppTaskManager_t* manager, AppTaskId_t id);

/**
 * @brief Récupère l'état d'une tâche
 * @param manager   Gestionnaire
 * @param id        Identifiant
 * @return          État de la tâche
 */
AppTaskState_t AppTasks_GetState(AppTaskManager_t* manager, AppTaskId_t id);

/**
 * @brief Récupère les statistiques d'une tâche
 * @param manager   Gestionnaire
 * @param id        Identifiant
 * @param stats     [out] Statistiques
 * @return          true si trouvée
 */
bool AppTasks_GetStats(AppTaskManager_t* manager, AppTaskId_t id, AppTaskStats_t* stats);

/**
 * @brief Réinitialise les statistiques de toutes les tâches
 * @param manager   Gestionnaire
 */
void AppTasks_ResetStats(AppTaskManager_t* manager);

/**
 * @brief Calcule la charge CPU totale
 * @param manager   Gestionnaire
 * @return          Pourcentage de charge CPU (0-100)
 */
float AppTasks_GetCPULoad(AppTaskManager_t* manager);

/**
 * @brief Affiche un rapport des tâches (console debug)
 * @param manager   Gestionnaire
 */
void AppTasks_PrintReport(AppTaskManager_t* manager);

/**
 * @brief Définit le callback d'erreur
 * @param manager   Gestionnaire
 * @param callback  Fonction à appeler en cas d'erreur
 */
void AppTasks_SetErrorCallback(AppTaskManager_t* manager,
                               void (*callback)(AppTaskId_t, uint32_t));

/* ======================================================================== */
/*              MODE FreeRTOS (si disponible)                               */
/* ======================================================================== */

#if APP_TASKS_USE_FREERTOS

/**
 * @brief Crée toutes les tâches FreeRTOS
 * 
 * Doit être appelé avant vTaskStartScheduler().
 * 
 * @param manager   Gestionnaire
 */
void AppTasks_CreateAll_FreeRTOS(AppTaskManager_t* manager);

/**
 * @brief Crée une tâche FreeRTOS
 * 
 * @param manager   Gestionnaire
 * @param id        Identifiant de la tâche
 * @return          true si créée avec succès
 */
bool AppTasks_Create_FreeRTOS(AppTaskManager_t* manager, AppTaskId_t id);

#endif /* APP_TASKS_USE_FREERTOS */

/* ======================================================================== */
/*              MACROS UTILITAIRES                                           */
/* ======================================================================== */

/**
 * @brief Mesure le temps d'exécution d'une tâche
 * 
 * Utilisation :
 *   APP_TASK_TIMING_START();
 *   // ... code de la tâche ...
 *   APP_TASK_TIMING_END(task);
 */
#define APP_TASK_TIMING_START()             uint32_t _task_start_us = HAL_GetTick() * 1000
#define APP_TASK_TIMING_END(task_ptr)       \
    do { \
        uint32_t _elapsed = (HAL_GetTick() * 1000) - _task_start_us; \
        (task_ptr)->last_duration_us = _elapsed; \
        (task_ptr)->stats.total_time_us += _elapsed; \
        if (_elapsed > (task_ptr)->stats.max_time_us) \
            (task_ptr)->stats.max_time_us = _elapsed; \
        if (_elapsed < (task_ptr)->stats.min_time_us || (task_ptr)->stats.min_time_us == 0) \
            (task_ptr)->stats.min_time_us = _elapsed; \
        (task_ptr)->stats.total_executions++; \
    } while (0)

/**
 * @brief Vérifie si une tâche est en retard
 */
#define APP_TASK_IS_LATE(task_ptr)          \
    ((task_ptr)->last_duration_us > (task_ptr)->period_ms * 1000)

/**
 * @brief Calcule le taux d'utilisation CPU d'une tâche
 */
#define APP_TASK_CPU_USAGE(task_ptr)        \
    ((float)(task_ptr)->stats.total_time_us / (float)(task_ptr)->stats.total_executions / \
     (float)((task_ptr)->period_ms * 1000) * 100.0f)

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */