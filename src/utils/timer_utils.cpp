/**
 * @file    timer_utils.cpp
 * @brief   Implémentation du gestionnaire de timers logiciels
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente un pool de timers logiciels basé sur SysTick.
 * 
 * FONCTIONNEMENT DÉTAILLÉ :
 * 
 * 1. POOL DE TIMERS :
 *    - Tableau statique de TIMER_MAX_COUNT structures
 *    - Liste chaînée des timers actifs pour parcours rapide
 *    - Allocation par recherche du premier slot libre
 * 
 * 2. SYSTICK (1ms) :
 *    - SysTick_Handler appelle Timer_ProcessExpired()
 *    - Parcourt la liste des timers actifs
 *    - Décrémente remaining_ms
 *    - Si remaining_ms == 0 : appelle le callback OU marque expiré
 *    - Si auto_reload : réinitialise remaining_ms = period_ms
 * 
 * 3. CALLBACK :
 *    - Exécuté dans le contexte SysTick (priorité élevée)
 *    - DOIT être court (< 1ms) et non-bloquant
 *    - Ne pas appeler HAL_Delay ou fonctions bloquantes
 * 
 * 4. DWT DELAY (µs) :
 *    - Utilise le compteur de cycle DWT_CYCCNT
 *    - Boucle active jusqu'à ce que le nombre de cycles soit écoulé
 *    - À 180 MHz : 1 µs = 180 cycles
 *    - Précision : ~0.005 µs
 * 
 * OPTIMISATIONS :
 *    - Liste chaînée simple (pas de tableau à parcourir)
 *    - Pas de division/modulo (les timers comptent en ms)
 *    - DWT pour microsecondes (pas de timer matériel)
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "timer_utils.h"
#include "debug_utils.h"

/* HAL */
#include "stm32f4xx_hal.h"

/* Standard */
#include <string.h>
#include <stdio.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs */
#define TAG                                 "Timer"

/** Fréquence CPU pour DWT (Hz) */
#define SYSTEM_CLOCK_HZ                     180000000

/** Cycles par microseconde */
#define CYCLES_PER_US                       (SYSTEM_CLOCK_HZ / 1000000)

/** DWT base address */
#define DWT_BASE                            (0xE0001000UL)
#define DWT_CYCCNT                          (*(volatile uint32_t*)(DWT_BASE + 0x04))
#define DWT_CTRL                            (*(volatile uint32_t*)(DWT_BASE + 0x00))
#define DWT_CTRL_CYCCNTENA                  (1 << 0)

/* ======================================================================== */
/*                VARIABLES STATIQUES                                       */
/* ======================================================================== */

/** Pool de timers */
static TimerHandle_s g_timer_pool[TIMER_MAX_COUNT];

/** Tête de liste chaînée des timers actifs */
static TimerHandle_t g_active_head = NULL;

/** Compteur de timestamp (incrémenté dans SysTick) */
static volatile uint32_t g_system_tick_ms = 0;

/** Compteur global d'expirations */
static volatile uint32_t g_total_expired = 0;

/** Compteur de dépassements (callback trop long) */
static volatile uint32_t g_overrun_count = 0;

/** Nombre maximum de timers actifs simultanés */
static uint32_t g_max_active = 0;

/** Flag d'initialisation */
static bool g_initialized = false;

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

static TimerHandle_t find_free_slot(void);
static void add_to_active_list(TimerHandle_t timer);
static void remove_from_active_list(TimerHandle_t timer);
static void init_dwt(void);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise le gestionnaire de timers
 */
void Timer_Init(void)
{
    if (g_initialized) {
        DEBUG_WARN(TAG, "Déjà initialisé");
        return;
    }

    DEBUG_INFO(TAG, "Initialisation du gestionnaire de timers...");

    /* Initialiser le pool */
    memset(g_timer_pool, 0, sizeof(g_timer_pool));
    for (int i = 0; i < TIMER_MAX_COUNT; i++) {
        g_timer_pool[i].state = TIMER_STATE_FREE;
        g_timer_pool[i].id = i;
    }

    /* Liste vide */
    g_active_head = NULL;

    /* Compteurs */
    g_system_tick_ms = 0;
    g_total_expired = 0;
    g_overrun_count = 0;
    g_max_active = 0;

    /* Initialiser DWT pour DelayUs */
    init_dwt();

    g_initialized = true;

    DEBUG_INFO(TAG, "Gestionnaire initialisé (%d timers max)", TIMER_MAX_COUNT);
}

/**
 * @brief Crée un nouveau timer
 */
TimerHandle_t Timer_Create(const char* name,
                           uint32_t delay_ms,
                           bool auto_reload,
                           TimerCallback_t callback,
                           void* user_data)
{
    if (!g_initialized) {
        DEBUG_ERROR(TAG, "Gestionnaire non initialisé");
        return NULL;
    }

    /* Trouver un slot libre */
    TimerHandle_t timer = find_free_slot();
    if (!timer) {
        DEBUG_ERROR(TAG, "Plus de timers disponibles (max=%d)", TIMER_MAX_COUNT);
        return NULL;
    }

    /* Limiter le délai */
    if (delay_ms < TIMER_MIN_DELAY_MS) {
        delay_ms = TIMER_MIN_DELAY_MS;
    }

    /* Configurer */
    if (name) {
        strncpy(timer->name, name, TIMER_MAX_NAME_LENGTH - 1);
        timer->name[TIMER_MAX_NAME_LENGTH - 1] = '\0';
    } else {
        snprintf(timer->name, sizeof(timer->name), "Timer_%d", timer->id);
    }

    timer->delay_ms = delay_ms;
    timer->period_ms = delay_ms;
    timer->auto_reload = auto_reload;
    timer->callback = callback;
    timer->user_data = user_data;
    timer->state = TIMER_STATE_STOPPED;
    timer->remaining_ms = 0;
    timer->elapsed_ms = 0;
    timer->expiry_count = 0;
    timer->next = NULL;

    DEBUG_VERBOSE(TAG, "Timer '%s' créé (id=%d, délai=%lums, reload=%s)",
                  timer->name, timer->id, delay_ms,
                  auto_reload ? "oui" : "non");

    return timer;
}

/**
 * @brief Détruit un timer
 */
void Timer_Delete(TimerHandle_t timer)
{
    if (!timer) return;

    DEBUG_VERBOSE(TAG, "Suppression timer '%s' (id=%d)", timer->name, timer->id);

    /* Retirer de la liste active si nécessaire */
    if (timer->state == TIMER_STATE_RUNNING) {
        remove_from_active_list(timer);
    }

    /* Libérer le slot */
    memset(timer, 0, sizeof(TimerHandle_s));
    timer->state = TIMER_STATE_FREE;
}

/**
 * @brief Démarre un timer
 */
bool Timer_Start(TimerHandle_t timer)
{
    if (!timer) return false;
    if (timer->state == TIMER_STATE_FREE) return false;

    /* Si déjà en cours, redémarrer */
    if (timer->state == TIMER_STATE_RUNNING) {
        remove_from_active_list(timer);
    }

    /* Configurer */
    timer->remaining_ms = timer->delay_ms;
    timer->elapsed_ms = 0;
    timer->state = TIMER_STATE_RUNNING;

    /* Ajouter à la liste active */
    add_to_active_list(timer);

    return true;
}

/**
 * @brief Démarre avec un nouveau délai
 */
bool Timer_StartWithDelay(TimerHandle_t timer, uint32_t new_delay_ms)
{
    if (!timer) return false;

    if (new_delay_ms < TIMER_MIN_DELAY_MS) {
        new_delay_ms = TIMER_MIN_DELAY_MS;
    }

    timer->delay_ms = new_delay_ms;
    timer->period_ms = new_delay_ms;

    return Timer_Start(timer);
}

/**
 * @brief Arrête un timer
 */
bool Timer_Stop(TimerHandle_t timer)
{
    if (!timer) return false;

    if (timer->state == TIMER_STATE_RUNNING) {
        remove_from_active_list(timer);
        timer->state = TIMER_STATE_STOPPED;
        timer->remaining_ms = 0;
    }

    return true;
}

/**
 * @brief Réinitialise le compteur
 */
bool Timer_Reset(TimerHandle_t timer)
{
    if (!timer) return false;

    if (timer->state == TIMER_STATE_RUNNING) {
        /* Juste remettre le compteur */
        timer->remaining_ms = timer->delay_ms;
        timer->elapsed_ms = 0;
    }

    return true;
}

/**
 * @brief Redémarre (stop + start)
 */
bool Timer_Restart(TimerHandle_t timer)
{
    if (!timer) return false;

    Timer_Stop(timer);
    return Timer_Start(timer);
}

/**
 * @brief Vérifie si expiré
 */
bool Timer_IsExpired(TimerHandle_t timer)
{
    if (!timer) return false;

    /* Pour les timers en cours, vérifier remaining_ms == 0 */
    /* (peut être lu depuis une ISR, donc volatile) */

    if (timer->state == TIMER_STATE_RUNNING) {
        return timer->remaining_ms == 0;
    }

    return timer->state == TIMER_STATE_EXPIRED;
}

/**
 * @brief Vérifie si en cours
 */
bool Timer_IsRunning(TimerHandle_t timer)
{
    return timer ? timer->state == TIMER_STATE_RUNNING : false;
}

/**
 * @brief Temps restant
 */
uint32_t Timer_GetRemaining(TimerHandle_t timer)
{
    if (!timer) return 0;
    return timer->remaining_ms;
}

/**
 * @brief Temps écoulé
 */
uint32_t Timer_GetElapsed(TimerHandle_t timer)
{
    if (!timer) return 0;

    if (timer->state == TIMER_STATE_RUNNING) {
        return timer->delay_ms - timer->remaining_ms;
    }

    return timer->elapsed_ms;
}

/**
 * @brief Nombre d'expirations
 */
uint32_t Timer_GetExpiryCount(TimerHandle_t timer)
{
    return timer ? timer->expiry_count : 0;
}

/**
 * @brief État
 */
TimerState_t Timer_GetState(TimerHandle_t timer)
{
    return timer ? timer->state : TIMER_STATE_FREE;
}

/**
 * @brief Contexte utilisateur
 */
void* Timer_GetContext(TimerHandle_t timer)
{
    return timer ? timer->user_data : NULL;
}

/**
 * @brief Modifie le callback
 */
void Timer_SetCallback(TimerHandle_t timer, TimerCallback_t callback)
{
    if (!timer) return;
    timer->callback = callback;
}

/**
 * @brief Modifie la période
 */
void Timer_SetPeriod(TimerHandle_t timer, uint32_t period_ms)
{
    if (!timer) return;
    if (period_ms < TIMER_MIN_DELAY_MS) period_ms = TIMER_MIN_DELAY_MS;

    timer->delay_ms = period_ms;
    timer->period_ms = period_ms;
}

/**
 * @brief Traite les timers expirés (appelé depuis SysTick)
 */
void Timer_ProcessExpired(void)
{
    if (!g_initialized) return;

    /* Incrémenter le timestamp global */
    g_system_tick_ms++;

    /* Parcourir la liste des timers actifs */
    TimerHandle_t current = g_active_head;
    TimerHandle_t prev = NULL;

    while (current != NULL) {
        /* Décrémenter le compteur */
        if (current->remaining_ms > 0) {
            current->remaining_ms--;
            current->elapsed_ms++;
        }

        /* Vérifier expiration */
        if (current->remaining_ms == 0 && current->state == TIMER_STATE_RUNNING) {
            current->expiry_count++;
            g_total_expired++;

            /* Appeler le callback si défini */
            if (current->callback) {
                current->callback(current);
            }

            if (current->auto_reload) {
                /* Réarmer pour la prochaine période */
                current->remaining_ms = current->period_ms;
                current->elapsed_ms = 0;
            } else {
                /* One-shot : marquer comme expiré, retirer de la liste */
                current->state = TIMER_STATE_EXPIRED;

                /* Retirer de la liste chaînée */
                if (prev) {
                    prev->next = current->next;
                } else {
                    g_active_head = current->next;
                }
                current->next = NULL;
            }
        }

        prev = current;
        current = current->next;
    }
}

/**
 * @brief Récupère les statistiques
 */
void Timer_GetStats(TimerStats_t* stats)
{
    if (!stats) return;

    memset(stats, 0, sizeof(*stats));

    /* Compter les actifs */
    TimerHandle_t current = g_active_head;
    while (current) {
        stats->active_count++;
        current = current->next;
    }

    /* Compter les créés (non FREE) */
    for (int i = 0; i < TIMER_MAX_COUNT; i++) {
        if (g_timer_pool[i].state != TIMER_STATE_FREE) {
            stats->total_created++;
        }
    }

    stats->total_expired = g_total_expired;
    stats->max_active = g_max_active;
    stats->overrun_count = g_overrun_count;
}

/**
 * @brief Réinitialise les statistiques
 */
void Timer_ResetStats(void)
{
    g_total_expired = 0;
    g_overrun_count = 0;
    g_max_active = 0;
}

/**
 * @brief Affiche un rapport
 */
void Timer_PrintReport(void)
{
    TimerStats_t stats;
    Timer_GetStats(&stats);

    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  RAPPORT DES TIMERS");
    DEBUG_INFO(TAG, "========================================");
    DEBUG_INFO(TAG, "  Actifs      : %lu", stats.active_count);
    DEBUG_INFO(TAG, "  Créés       : %lu", stats.total_created);
    DEBUG_INFO(TAG, "  Expirations : %lu", stats.total_expired);
    DEBUG_INFO(TAG, "  Max actifs  : %lu", stats.max_active);
    DEBUG_INFO(TAG, "  Overruns    : %lu", stats.overrun_count);
    DEBUG_INFO(TAG, "----------------------------------------");

    /* Lister les timers actifs */
    TimerHandle_t current = g_active_head;
    int index = 0;
    while (current && index < 10) {
        DEBUG_INFO(TAG, "  [%d] %-14s rem=%lums count=%lu %s",
                   current->id,
                   current->name,
                   current->remaining_ms,
                   current->expiry_count,
                   current->auto_reload ? "(P)" : "(1)");
        current = current->next;
        index++;
    }
    DEBUG_INFO(TAG, "========================================");
}

/* ---- Fonctions de délai ---- */

/**
 * @brief Attente en millisecondes
 */
void Timer_Delay(uint32_t ms)
{
    uint32_t start = g_system_tick_ms;
    while ((g_system_tick_ms - start) < ms) {
        /* Attente active, yield au CPU */
        __NOP();
    }
}

/**
 * @brief Attente en microsecondes (DWT)
 */
void Timer_DelayUs(uint32_t us)
{
    if (us == 0) return;

    uint32_t start = DWT_CYCCNT;
    uint32_t cycles = us * CYCLES_PER_US;

    while ((DWT_CYCCNT - start) < cycles) {
        __NOP();
    }
}

/**
 * @brief Timestamp millisecondes
 */
uint32_t Timer_GetTimestamp(void)
{
    return g_system_tick_ms;
}

/**
 * @brief Timestamp microsecondes
 */
uint32_t Timer_GetTimestampUs(void)
{
    return DWT_CYCCNT / CYCLES_PER_US;
}

/**
 * @brief Différence entre deux timestamps (gère wrap-around)
 */
uint32_t Timer_GetElapsedTime(uint32_t t1, uint32_t t2)
{
    /* Gérer le wrap-around du compteur 32 bits */
    if (t2 >= t1) {
        return t2 - t1;
    } else {
        return (0xFFFFFFFF - t1) + t2 + 1;
    }
}

/**
 * @brief Temps écoulé depuis un timestamp
 */
uint32_t Timer_GetTimeSince(uint32_t since)
{
    return Timer_GetElapsedTime(since, g_system_tick_ms);
}

/**
 * @brief Vérifie un timeout
 */
bool Timer_IsTimeout(uint32_t since, uint32_t timeout_ms)
{
    return Timer_GetTimeSince(since) >= timeout_ms;
}

/* ======================================================================== */
/*              FONCTIONS PRIVÉES                                           */
/* ======================================================================== */

/**
 * @brief Trouve le premier slot libre dans le pool
 */
static TimerHandle_t find_free_slot(void)
{
    for (int i = 0; i < TIMER_MAX_COUNT; i++) {
        if (g_timer_pool[i].state == TIMER_STATE_FREE) {
            return &g_timer_pool[i];
        }
    }
    return NULL;
}

/**
 * @brief Ajoute un timer à la liste active (en tête)
 */
static void add_to_active_list(TimerHandle_t timer)
{
    if (!timer) return;

    /* Ajouter en tête de liste */
    timer->next = g_active_head;
    g_active_head = timer;

    /* Mettre à jour le compteur max */
    uint32_t active = 0;
    TimerHandle_t current = g_active_head;
    while (current) {
        active++;
        current = current->next;
    }
    if (active > g_max_active) {
        g_max_active = active;
    }
}

/**
 * @brief Retire un timer de la liste active
 */
static void remove_from_active_list(TimerHandle_t timer)
{
    if (!timer || !g_active_head) return;

    /* Chercher et retirer */
    if (g_active_head == timer) {
        /* En tête de liste */
        g_active_head = timer->next;
        timer->next = NULL;
        return;
    }

    /* Chercher dans la liste */
    TimerHandle_t current = g_active_head;
    while (current->next) {
        if (current->next == timer) {
            current->next = timer->next;
            timer->next = NULL;
            return;
        }
        current = current->next;
    }
}

/**
 * @brief Initialise le compteur de cycle DWT
 */
static void init_dwt(void)
{
    /* Activer le compteur de cycle DWT */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT_CYCCNT = 0;
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;
}

/* ======================================================================== */
/*              HANDLER SYSTICK                                              */
/* ======================================================================== */

/**
 * @brief Handler d'interruption SysTick
 *
 * Appelée toutes les 1ms par le timer système.
 * Incrémente le tick HAL et traite les timers logiciels.
 */
void SysTick_Handler(void)
{
    /* Incrémenter le tick HAL (nécessaire pour HAL_Delay) */
    HAL_IncTick();

    /* Traiter les timers logiciels */
    Timer_ProcessExpired();
}

/* ======================================================================== */
/*              EXEMPLE D'UTILISATION                                       */
/* ======================================================================== */

#if 0  /* Exemples - Non compilés */

/* --- Exemple 1 : Timer one-shot avec callback --- */

static TimerHandle_t led_off_timer;

void led_on_for_500ms(void) {
    LED_On();
    Timer_StartWithDelay(led_off_timer, 500);
}

static void led_off_callback(TimerHandle_t timer) {
    LED_Off();
}

void setup_timer_example_1(void) {
    led_off_timer = Timer_Create("LED_Off", 500, false, led_off_callback, NULL);
}

/* --- Exemple 2 : Timer périodique avec polling --- */

static TimerHandle_t sensor_timer;
static TimerHandle_t display_timer;

void setup_timer_example_2(void) {
    sensor_timer = Timer_Create("Sensor", 100, true, NULL, NULL);
    display_timer = Timer_Create("Display", 1000, true, NULL, NULL);
    
    Timer_Start(sensor_timer);
    Timer_Start(display_timer);
}

void loop_timer_example_2(void) {
    if (TIMER_CHECK_AND_RESET(sensor_timer)) {
        read_temperature();
    }
    
    if (TIMER_CHECK_AND_RESET(display_timer)) {
        update_screen();
    }
}

/* --- Exemple 3 : Timeout avec timestamp --- */

bool wait_for_response(uint32_t timeout_ms) {
    uint32_t start = TIMER_NOW();
    
    while (!TIMER_TIMEOUT(start, timeout_ms)) {
        if (data_received()) {
            return true;
        }
        __NOP();
    }
    
    return false;  /* Timeout */
}

/* --- Exemple 4 : Délai microsecondes pour signal précis --- */

void generate_pulse(void) {
    GPIO_SetHigh();
    Timer_DelayUs(10);   /* 10 µs high */
    GPIO_SetLow();
    Timer_DelayUs(90);   /* 90 µs low */
}

#endif

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */