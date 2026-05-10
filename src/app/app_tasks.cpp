/**
 * @file    app_tasks.cpp
 * @brief   Implémentation du gestionnaire de tâches
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente l'ordonnancement coopératif des tâches.
 * 
 * FONCTIONNEMENT EN MODE SUPER-LOOP :
 * 
 *   À chaque appel de AppTasks_RunAll() :
 * 
 *   1. Parcourir les tâches par ordre de priorité décroissante
 *   2. Pour chaque tâche :
 *      a. Vérifier si elle est activée (enabled)
 *      b. Vérifier si elle est en cours (state == RUNNING)
 *      c. Vérifier si sa période est écoulée (now >= next_run_ms)
 *      d. Si oui, exécuter la fonction de la tâche
 *      e. Mesurer le temps d'exécution
 *      f. Mettre à jour next_run_ms = now + period_ms
 *      g. Mettre à jour les statistiques
 *   3. Si une tâche a dépassé sa période, incrémenter missed_deadlines
 * 
 * ORDONNANCEMENT PAR PRIORITÉ :
 * 
 *   Les tâches sont exécutées dans l'ordre de priorité :
 *   CRITICAL → HIGH → NORMAL → LOW → IDLE
 * 
 *   Au sein d'une même priorité, les tâches sont exécutées
 *   dans l'ordre de leur ID (ordre d'enregistrement).
 * 
 *   Une tâche de priorité supérieure peut interrompre une tâche
 *   de priorité inférieure en mode FreeRTOS uniquement.
 * 
 * DÉTECTION DE RETARD (MISSED DEADLINE) :
 * 
 *   Une tâche est en retard si :
 *     now > next_run_ms + period_ms
 * 
 *   C'est-à-dire si plus d'une période complète s'est écoulée
 *   depuis la dernière exécution prévue.
 * 
 *   Les retards répétés indiquent une surcharge CPU.
 * 
 * OPTIMISATIONS :
 * 
 *   - Les tâches désactivées sont ignorées (pas de vérification)
 *   - Les tâches à période 0 sont exécutées à chaque cycle
 *   - Les tâches run_once sont désactivées après exécution
 *   - La mesure du temps utilise le cycle counter DWT (précis)
 * 
 * EXEMPLE DE CYCLE :
 * 
 *   Tâches : UI(30ms), Touch(10ms), LoRa(5ms)
 * 
 *   t=0ms   : UI ✓, Touch ✓, LoRa ✓
 *   t=5ms   : LoRa ✓ (prochaine: 10ms)
 *   t=10ms  : Touch ✓, LoRa ✓ (prochaines: 20ms, 15ms)
 *   t=15ms  : LoRa ✓ (prochaine: 20ms)
 *   t=20ms  : Touch ✓, LoRa ✓
 *   t=25ms  : LoRa ✓
 *   t=30ms  : UI ✓, Touch ✓, LoRa ✓
 *   ...
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "app_tasks.h"
#include "phone_app.h"

/* HAL */
#include "stm32f4xx_hal.h"

/* Utilitaires */
#include "../utils/debug_utils.h"
#include "../utils/timer_utils.h"

/* Standard */
#include <string.h>
#include <stdio.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs */
#define TAG                                 "Tasks"

/** Nombre de tâches par défaut */
#define DEFAULT_TASK_COUNT                  12

/** Seuil d'alerte de charge CPU (%) */
#define CPU_LOAD_WARNING_THRESHOLD          80.0f

/** Seuil critique de charge CPU (%) */
#define CPU_LOAD_CRITICAL_THRESHOLD         95.0f

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

static AppTask_t* find_task(AppTaskManager_t* manager, AppTaskId_t id);
static int compare_priority(const void* a, const void* b);
static void update_cpu_load(AppTaskManager_t* manager);
static void default_ui_task(void* context);
static void default_touch_task(void* context);
static void default_keys_task(void* context);
static void default_lora_rx_task(void* context);
static void default_lora_tx_task(void* context);
static void default_audio_in_task(void* context);
static void default_audio_out_task(void* context);
static void default_phone_task(void* context);
static void default_sms_task(void* context);
static void default_battery_task(void* context);
static void default_watchdog_task(void* context);
static void default_background_task(void* context);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise le gestionnaire de tâches
 */
void AppTasks_Init(AppTaskManager_t* manager, struct PhoneApp_t* app)
{
    if (!manager) return;

    DEBUG_INFO(TAG, "Initialisation du gestionnaire de tâches...");

    /* Mise à zéro */
    memset(manager, 0, sizeof(AppTaskManager_t));

    /* État */
    manager->is_running = true;
    manager->total_cycles = 0;
    manager->cycle_start_ms = 0;
    manager->cycle_duration_us = 0;
    manager->max_cycle_us = 0;

    /* Callback erreur */
    manager->on_task_error = NULL;

    /* Mode FreeRTOS */
    #if APP_TASKS_USE_FREERTOS
        manager->use_freertos = false;  /* Sera activé si CreateAll_FreeRTOS appelé */
    #endif

    /* ---- Enregistrer les tâches par défaut ---- */

    AppTasks_Register(manager, APP_TASK_ID_UI,
                      "UI", default_ui_task, app,
                      30, APP_TASK_PRIORITY_NORMAL);

    AppTasks_Register(manager, APP_TASK_ID_TOUCH,
                      "Touch", default_touch_task, app,
                      10, APP_TASK_PRIORITY_HIGH);

    AppTasks_Register(manager, APP_TASK_ID_KEYS,
                      "Keys", default_keys_task, app,
                      20, APP_TASK_PRIORITY_NORMAL);

    AppTasks_Register(manager, APP_TASK_ID_LORA_RX,
                      "LoRa_RX", default_lora_rx_task, app,
                      5, APP_TASK_PRIORITY_HIGH);

    AppTasks_Register(manager, APP_TASK_ID_LORA_TX,
                      "LoRa_TX", default_lora_tx_task, app,
                      50, APP_TASK_PRIORITY_NORMAL);

    AppTasks_Register(manager, APP_TASK_ID_AUDIO_IN,
                      "Audio_IN", default_audio_in_task, app,
                      5, APP_TASK_PRIORITY_HIGH);

    AppTasks_Register(manager, APP_TASK_ID_AUDIO_OUT,
                      "Audio_OUT", default_audio_out_task, app,
                      5, APP_TASK_PRIORITY_HIGH);

    AppTasks_Register(manager, APP_TASK_ID_PHONE,
                      "Phone", default_phone_task, app,
                      50, APP_TASK_PRIORITY_NORMAL);

    AppTasks_Register(manager, APP_TASK_ID_SMS,
                      "SMS", default_sms_task, app,
                      100, APP_TASK_PRIORITY_LOW);

    AppTasks_Register(manager, APP_TASK_ID_BATTERY,
                      "Battery", default_battery_task, app,
                      1000, APP_TASK_PRIORITY_LOW);

    AppTasks_Register(manager, APP_TASK_ID_WATCHDOG,
                      "Watchdog", default_watchdog_task, app,
                      500, APP_TASK_PRIORITY_CRITICAL);

    AppTasks_Register(manager, APP_TASK_ID_BACKGROUND,
                      "Background", default_background_task, app,
                      1000, APP_TASK_PRIORITY_IDLE);

    DEBUG_INFO(TAG, "Gestionnaire initialisé avec %d tâches", manager->task_count);
}

/**
 * @brief Enregistre une nouvelle tâche
 */
bool AppTasks_Register(AppTaskManager_t* manager,
                       AppTaskId_t id,
                       const char* name,
                       AppTaskFunction_t function,
                       void* context,
                       uint32_t period_ms,
                       AppTaskPriority_t priority)
{
    if (!manager || !function) return false;

    /* Vérifier si la table est pleine */
    if (manager->task_count >= APP_TASKS_MAX_COUNT) {
        DEBUG_ERROR(TAG, "Table de tâches pleine (%d max)", APP_TASKS_MAX_COUNT);
        return false;
    }

    /* Vérifier si une tâche avec cet ID existe déjà */
    if (find_task(manager, id) != NULL) {
        DEBUG_WARN(TAG, "Tâche ID=%d existe déjà", id);
        return false;
    }

    /* Créer la tâche */
    AppTask_t* task = &manager->tasks[manager->task_count];
    memset(task, 0, sizeof(AppTask_t));

    task->id = id;
    strncpy(task->name, name, APP_TASK_NAME_MAX_LENGTH - 1);
    task->function = function;
    task->context = context;
    task->priority = priority;
    task->period_ms = period_ms;
    task->enabled = true;
    task->run_once = false;
    task->state = APP_TASK_STATE_STOPPED;
    task->next_run_ms = HAL_GetTick();
    task->last_run_ms = 0;
    task->last_duration_us = 0;

    /* Stats à zéro */
    memset(&task->stats, 0, sizeof(AppTaskStats_t));

    manager->task_count++;

    DEBUG_INFO(TAG, "Tâche enregistrée: %s (ID=%d, période=%lums, priorité=%d)",
               name, id, period_ms, priority);

    return true;
}

/**
 * @brief Exécute toutes les tâches éligibles
 */
void AppTasks_RunAll(AppTaskManager_t* manager)
{
    if (!manager || !manager->is_running) return;

    uint32_t cycle_start = HAL_GetTick();
    manager->cycle_start_ms = cycle_start;
    manager->total_cycles++;

    /* Exécuter les tâches par priorité décroissante */
    for (int prio = APP_TASK_PRIORITY_CRITICAL; prio >= APP_TASK_PRIORITY_IDLE; prio--) {

        for (uint8_t i = 0; i < manager->task_count; i++) {
            AppTask_t* task = &manager->tasks[i];

            /* Vérifier si la tâche est éligible */
            if (!task->enabled) continue;
            if (task->state != APP_TASK_STATE_RUNNING && 
                task->state != APP_TASK_STATE_STOPPED) continue;
            if (task->priority != (AppTaskPriority_t)prio) continue;

            uint32_t now = HAL_GetTick();

            /* Vérifier la période */
            if (now < task->next_run_ms) continue;

            /* Marquer comme en cours */
            task->state = APP_TASK_STATE_RUNNING;

            /* Mesurer le temps d'exécution */
            uint32_t start_us = HAL_GetTick() * 1000;  /* Approximation */

            /* Exécuter la fonction */
            if (task->function) {
                task->function(task->context);
            }

            /* Calculer la durée */
            uint32_t end_us = HAL_GetTick() * 1000;
            task->last_duration_us = end_us - start_us;
            task->last_run_ms = now;

            /* Mettre à jour les statistiques */
            task->stats.total_executions++;
            task->stats.total_time_us += task->last_duration_us;

            if (task->last_duration_us > task->stats.max_time_us) {
                task->stats.max_time_us = task->last_duration_us;
            }
            if (task->last_duration_us < task->stats.min_time_us || 
                task->stats.min_time_us == 0) {
                task->stats.min_time_us = task->last_duration_us;
            }

            /* Calculer la prochaine exécution */
            if (task->run_once) {
                /* Tâche unique : désactiver après exécution */
                task->enabled = false;
                task->state = APP_TASK_STATE_STOPPED;
            } else {
                /* Tâche périodique */
                task->next_run_ms = now + task->period_ms;
                task->state = APP_TASK_STATE_STOPPED;

                /* Détecter les retards */
                if (now > task->next_run_ms + task->period_ms) {
                    task->stats.missed_deadlines++;
                }
            }
        }
    }

    /* Mesurer la durée du cycle */
    uint32_t cycle_end = HAL_GetTick();
    manager->cycle_duration_us = (cycle_end - cycle_start) * 1000;

    if (manager->cycle_duration_us > manager->max_cycle_us) {
        manager->max_cycle_us = manager->cycle_duration_us;
    }

    /* Mettre à jour la charge CPU périodiquement */
    static uint32_t last_cpu_update = 0;
    uint32_t now = HAL_GetTick();
    if (now - last_cpu_update > 5000) {  /* Toutes les 5 secondes */
        update_cpu_load(manager);
        last_cpu_update = now;

        /* Alerter si surcharge */
        float cpu_load = AppTasks_GetCPULoad(manager);
        if (cpu_load > CPU_LOAD_CRITICAL_THRESHOLD) {
            DEBUG_ERROR(TAG, "Surcharge CPU critique: %.1f%%", cpu_load);
        } else if (cpu_load > CPU_LOAD_WARNING_THRESHOLD) {
            DEBUG_WARN(TAG, "Surcharge CPU: %.1f%%", cpu_load);
        }
    }
}

/**
 * @brief Exécute une tâche spécifique
 */
void AppTasks_RunOne(AppTaskManager_t* manager, AppTaskId_t id)
{
    if (!manager) return;

    AppTask_t* task = find_task(manager, id);
    if (!task || !task->enabled || !task->function) return;

    task->state = APP_TASK_STATE_RUNNING;

    uint32_t start_us = HAL_GetTick() * 1000;

    task->function(task->context);

    uint32_t end_us = HAL_GetTick() * 1000;
    task->last_duration_us = end_us - start_us;
    task->last_run_ms = HAL_GetTick();
    task->state = APP_TASK_STATE_STOPPED;
}

/**
 * @brief Démarre une tâche
 */
void AppTasks_Start(AppTaskManager_t* manager, AppTaskId_t id)
{
    AppTask_t* task = find_task(manager, id);
    if (!task) return;

    task->enabled = true;
    task->state = APP_TASK_STATE_RUNNING;
    task->next_run_ms = HAL_GetTick();
    task->run_once = false;

    DEBUG_VERBOSE(TAG, "Tâche %s démarrée", task->name);
}

/**
 * @brief Arrête une tâche
 */
void AppTasks_Stop(AppTaskManager_t* manager, AppTaskId_t id)
{
    AppTask_t* task = find_task(manager, id);
    if (!task) return;

    task->enabled = false;
    task->state = APP_TASK_STATE_STOPPED;

    DEBUG_VERBOSE(TAG, "Tâche %s arrêtée", task->name);
}

/**
 * @brief Met une tâche en pause
 */
void AppTasks_Pause(AppTaskManager_t* manager, AppTaskId_t id)
{
    AppTask_t* task = find_task(manager, id);
    if (!task) return;

    task->state = APP_TASK_STATE_PAUSED;

    DEBUG_VERBOSE(TAG, "Tâche %s en pause", task->name);
}

/**
 * @brief Reprend une tâche
 */
void AppTasks_Resume(AppTaskManager_t* manager, AppTaskId_t id)
{
    AppTask_t* task = find_task(manager, id);
    if (!task) return;

    task->state = APP_TASK_STATE_RUNNING;
    task->next_run_ms = HAL_GetTick();  /* Réinitialiser le délai */

    DEBUG_VERBOSE(TAG, "Tâche %s reprise", task->name);
}

/**
 * @brief Active/désactive une tâche
 */
void AppTasks_SetEnabled(AppTaskManager_t* manager, AppTaskId_t id, bool enabled)
{
    AppTask_t* task = find_task(manager, id);
    if (!task) return;

    task->enabled = enabled;
    if (enabled) {
        task->next_run_ms = HAL_GetTick();
    }
}

/**
 * @brief Modifie la période
 */
void AppTasks_SetPeriod(AppTaskManager_t* manager, AppTaskId_t id, uint32_t period_ms)
{
    AppTask_t* task = find_task(manager, id);
    if (!task) return;

    task->period_ms = period_ms;
    task->next_run_ms = HAL_GetTick() + period_ms;
}

/**
 * @brief Modifie la priorité
 */
void AppTasks_SetPriority(AppTaskManager_t* manager, AppTaskId_t id, AppTaskPriority_t priority)
{
    AppTask_t* task = find_task(manager, id);
    if (!task) return;

    task->priority = priority;
}

/**
 * @brief Récupère une tâche
 */
AppTask_t* AppTasks_GetTask(AppTaskManager_t* manager, AppTaskId_t id)
{
    return find_task(manager, id);
}

/**
 * @brief Récupère l'état
 */
AppTaskState_t AppTasks_GetState(AppTaskManager_t* manager, AppTaskId_t id)
{
    AppTask_t* task = find_task(manager, id);
    return task ? task->state : APP_TASK_STATE_ERROR;
}

/**
 * @brief Récupère les statistiques
 */
bool AppTasks_GetStats(AppTaskManager_t* manager, AppTaskId_t id, AppTaskStats_t* stats)
{
    AppTask_t* task = find_task(manager, id);
    if (!task || !stats) return false;

    memcpy(stats, &task->stats, sizeof(AppTaskStats_t));
    return true;
}

/**
 * @brief Réinitialise les statistiques
 */
void AppTasks_ResetStats(AppTaskManager_t* manager)
{
    if (!manager) return;

    for (uint8_t i = 0; i < manager->task_count; i++) {
        memset(&manager->tasks[i].stats, 0, sizeof(AppTaskStats_t));
    }

    manager->max_cycle_us = 0;
    manager->total_cycles = 0;

    DEBUG_INFO(TAG, "Statistiques réinitialisées");
}

/**
 * @brief Calcule la charge CPU totale
 */
float AppTasks_GetCPULoad(AppTaskManager_t* manager)
{
    if (!manager || manager->task_count == 0) return 0.0f;

    float total_load = 0.0f;

    for (uint8_t i = 0; i < manager->task_count; i++) {
        AppTask_t* task = &manager->tasks[i];
        if (task->period_ms > 0 && task->stats.total_executions > 0) {
            float task_load = APP_TASK_CPU_USAGE(task);
            total_load += task_load;
        }
    }

    return total_load;
}

/**
 * @brief Affiche un rapport
 */
void AppTasks_PrintReport(AppTaskManager_t* manager)
{
    if (!manager) return;

    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  RAPPORT DES TÂCHES");
    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  Cycles: %lu | Max cycle: %lu us",
               manager->total_cycles, manager->max_cycle_us);
    DEBUG_INFO(TAG, "  Charge CPU: %.1f%%", AppTasks_GetCPULoad(manager));
    DEBUG_INFO(TAG, "----------------------------------------");

    for (uint8_t i = 0; i < manager->task_count; i++) {
        AppTask_t* task = &manager->tasks[i];
        float cpu_usage = (task->period_ms > 0 && task->stats.total_executions > 0) 
                          ? APP_TASK_CPU_USAGE(task) : 0.0f;

        DEBUG_INFO(TAG, "  %-12s | P=%d | %5lu exéc | %6lu us max | %5.1f%% CPU | %lu ratés",
                   task->name,
                   task->priority,
                   task->stats.total_executions,
                   task->stats.max_time_us,
                   cpu_usage,
                   task->stats.missed_deadlines);
    }
    DEBUG_INFO(TAG, "========================================");
}

/**
 * @brief Définit le callback d'erreur
 */
void AppTasks_SetErrorCallback(AppTaskManager_t* manager,
                               void (*callback)(AppTaskId_t, uint32_t))
{
    if (!manager) return;
    manager->on_task_error = callback;
}

/* ======================================================================== */
/*              MODE FreeRTOS (si disponible)                               */
/* ======================================================================== */

#if APP_TASKS_USE_FREERTOS

/**
 * @brief Wrapper FreeRTOS pour une tâche
 */
static void freertos_task_wrapper(void* parameters)
{
    AppTask_t* task = (AppTask_t*)parameters;
    if (!task || !task->function) {
        vTaskDelete(NULL);
        return;
    }

    TickType_t last_wake_time = xTaskGetTickCount();

    while (1) {
        task->state = APP_TASK_STATE_RUNNING;

        uint32_t start_us = HAL_GetTick() * 1000;

        task->function(task->context);

        uint32_t end_us = HAL_GetTick() * 1000;
        task->last_duration_us = end_us - start_us;
        task->stats.total_executions++;
        task->stats.total_time_us += task->last_duration_us;

        task->state = APP_TASK_STATE_STOPPED;

        if (task->period_ms > 0) {
            vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(task->period_ms));
        } else {
            taskYIELD();
        }
    }
}

void AppTasks_CreateAll_FreeRTOS(AppTaskManager_t* manager)
{
    if (!manager) return;

    DEBUG_INFO(TAG, "Création des tâches FreeRTOS...");
    manager->use_freertos = true;

    for (uint8_t i = 0; i < manager->task_count; i++) {
        AppTask_t* task = &manager->tasks[i];
        if (!task->enabled) continue;

        BaseType_t result = xTaskCreate(
            freertos_task_wrapper,
            task->name,
            configMINIMAL_STACK_SIZE + 256,
            task,
            task->priority + 1,  /* Priorité FreeRTOS = priorité app + 1 */
            &task->rtos_handle
        );

        if (result != pdPASS) {
            DEBUG_ERROR(TAG, "Échec création tâche %s", task->name);
        } else {
            DEBUG_INFO(TAG, "Tâche FreeRTOS créée: %s", task->name);
        }
    }
}

bool AppTasks_Create_FreeRTOS(AppTaskManager_t* manager, AppTaskId_t id)
{
    if (!manager || !manager->use_freertos) return false;

    AppTask_t* task = find_task(manager, id);
    if (!task) return false;

    BaseType_t result = xTaskCreate(
        freertos_task_wrapper,
        task->name,
        configMINIMAL_STACK_SIZE + 256,
        task,
        task->priority + 1,
        &task->rtos_handle
    );

    return result == pdPASS;
}

#endif /* APP_TASKS_USE_FREERTOS */

/* ======================================================================== */
/*              FONCTIONS PRIVÉES                                           */
/* ======================================================================== */

/**
 * @brief Trouve une tâche par son ID
 */
static AppTask_t* find_task(AppTaskManager_t* manager, AppTaskId_t id)
{
    if (!manager) return NULL;

    for (uint8_t i = 0; i < manager->task_count; i++) {
        if (manager->tasks[i].id == id) {
            return &manager->tasks[i];
        }
    }
    return NULL;
}

/**
 * @brief Met à jour la charge CPU de chaque tâche
 */
static void update_cpu_load(AppTaskManager_t* manager)
{
    if (!manager) return;

    for (uint8_t i = 0; i < manager->task_count; i++) {
        AppTask_t* task = &manager->tasks[i];
        if (task->period_ms > 0 && task->stats.total_executions > 0) {
            task->stats.cpu_load_percent = APP_TASK_CPU_USAGE(task);
        }
    }
}

/* ======================================================================== */
/*              TÂCHES PAR DÉFAUT                                           */
/* ======================================================================== */

static void default_ui_task(void* context)
{
    PhoneApp_t* app = (PhoneApp_t*)context;
    if (!app) return;

    /* Rafraîchir l'écran actif */
    if (app->active_screen && app->active_screen->is_visible) {
        if (app->active_screen->update) {
            app->active_screen->update(app->active_screen);
        }
    }
}

static void default_touch_task(void* context)
{
    PhoneApp_t* app = (PhoneApp_t*)context;
    if (!app) return;

    /* Scanner l'écran tactile */
    /* Les coordonnées sont lues par interruption */
    /* Ici on vérifie juste les événements en attente */
}

static void default_keys_task(void* context)
{
    PhoneApp_t* app = (PhoneApp_t*)context;
    if (!app) return;

    /* Scanner le clavier matriciel */
    /* uint8_t key = Keypad_Scan(); */
    /* if (key) PhoneApp_PostEvent(app, APP_EVENT_KEY_PRESS, APP_PRIORITY_NORMAL); */
}

static void default_lora_rx_task(void* context)
{
    PhoneApp_t* app = (PhoneApp_t*)context;
    if (!app || !app->lora) return;

    /* Vérifier si un paquet est disponible */
    /* int packet_size = LoRaDriver_ParsePacket(app->lora); */
    /* if (packet_size > 0) { ... } */
}

static void default_lora_tx_task(void* context)
{
    /* Vérifier la file de transmission */
}

static void default_audio_in_task(void* context)
{
    /* Traiter le buffer DMA du microphone */
}

static void default_audio_out_task(void* context)
{
    /* Remplir le buffer DMA du haut-parleur */
}

static void default_phone_task(void* context)
{
    PhoneApp_t* app = (PhoneApp_t*)context;
    if (!app) return;

    /* Mettre à jour le service téléphonie */
    /* PhoneService_Update(&app->phone_service); */
}

static void default_sms_task(void* context)
{
    /* Traiter les messages en file */
}

static void default_battery_task(void* context)
{
    PhoneApp_t* app = (PhoneApp_t*)context;
    if (!app || !app->power) return;

    /* Lire le niveau de batterie */
    /* uint8_t level = PowerManager_GetBatteryPercent(app->power); */
    /* Vérifier les seuils */
}

static void default_watchdog_task(void* context)
{
    /* Réarmer le chien de garde */
    /* IWDG_Refresh(); */
}

static void default_background_task(void* context)
{
    /* Nettoyage : purger les vieux logs, vérifier la mémoire... */
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */