/**
 * @file    app_state_machine.cpp
 * @brief   Implémentation de la machine d'états de l'application
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente la gestion des transitions d'états de l'application.
 * 
 * PRINCIPE DE FONCTIONNEMENT :
 * 
 * 1. TABLE DE TRANSITIONS
 *    Chaque transition est définie explicitement dans un tableau.
 *    Une transition n'est valide QUE si elle est dans ce tableau.
 *    Format : { état_source, état_destination, guard, action, description }
 * 
 * 2. VÉRIFICATION (GUARD)
 *    Avant chaque transition, la fonction guard (si définie) est appelée.
 *    Si elle retourne false, la transition est refusée.
 *    Exemple : on ne peut passer en IN_CALL que si le module LoRa est prêt.
 * 
 * 3. ACTION DE TRANSITION
 *    Pendant la transition, une action peut être exécutée :
 *    - Sauvegarde de l'état précédent
 *    - Nettoyage des ressources de l'ancien état
 *    - Initialisation du nouvel état
 *    - Notification des observateurs
 * 
 * 4. CALLBACK GLOBAL
 *    Après chaque transition réussie, un callback est appelé.
 *    Il permet de :
 *    - Logger la transition
 *    - Mettre à jour l'UI globale (barre de statut)
 *    - Gérer les timers spécifiques à l'état
 * 
 * 5. STATISTIQUES
 *    Chaque transition est comptabilisée.
 *    Le temps passé dans chaque état est mesuré.
 *    Utile pour le débogage et l'optimisation.
 * 
 * EXEMPLE DE FLUX COMPLET :
 * 
 *   AppStateMachine_Transition(sm, APP_STATE_DIALING);
 *        │
 *        ├── 1. Vérifier si la transition est dans la table
 *        │      → Oui : IDLE → DIALING existe
 *        │
 *        ├── 2. Appeler le guard (si défini)
 *        │      → Guard vérifie que le module LoRa est prêt
 *        │      → Retourne true
 *        │
 *        ├── 3. Exécuter l'action de transition
 *        │      → Sauvegarder l'écran précédent
 *        │      → Initialiser le composeur
 *        │
 *        ├── 4. Mettre à jour l'état
 *        │      → previous_state = IDLE
 *        │      → current_state = DIALING
 *        │      → state_entered_ms = maintenant
 *        │
 *        ├── 5. Mettre à jour les statistiques
 *        │      → transition_count++
 *        │      → state_entry_count[DIALING]++
 *        │
 *        └── 6. Appeler le callback global
 *               → on_transition(IDLE, DIALING, context)
 *               → L'UI se met à jour pour refléter le nouvel état
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "app_state_machine.h"

/* Utilitaires */
#include "../utils/debug_utils.h"
#include "../utils/timer_utils.h"

/* HAL */
#include "stm32f4xx_hal.h"

/* Standard */
#include <string.h>
#include <stdio.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs */
#define TAG                                 "StateMachine"

/** Version de la table de transitions (pour compatibilité) */
#define STATE_TRANSITION_TABLE_VERSION      1

/* ======================================================================== */
/*                VARIABLES STATIQUES                                       */
/* ======================================================================== */

/** Noms lisibles des états (pour logs et debug) */
static const char* STATE_NAMES[APP_STATE_COUNT] = {
    [APP_STATE_INIT]            = "INIT",
    [APP_STATE_SPLASH]          = "SPLASH",
    [APP_STATE_LOCKED]          = "LOCKED",
    [APP_STATE_IDLE]            = "IDLE",
    [APP_STATE_ACTIVE]          = "ACTIVE",
    [APP_STATE_IN_CALL]         = "IN_CALL",
    [APP_STATE_INCOMING_CALL]   = "INCOMING_CALL",
    [APP_STATE_DIALING]         = "DIALING",
    [APP_STATE_MESSAGING]       = "MESSAGING",
    [APP_STATE_SETTINGS]        = "SETTINGS",
    [APP_STATE_ERROR]           = "ERROR",
    [APP_STATE_SHUTDOWN]        = "SHUTDOWN",
};

/* ======================================================================== */
/*                TRANSITIONS PAR DÉFAUT                                     */
/* ======================================================================== */

/**
 * @brief Table des transitions valides
 * 
 * DÉFINIT LE COMPORTEMENT DE LA MACHINE D'ÉTATS.
 * Toute transition non listée ici sera REFUSÉE.
 * 
 * Pour ajouter une transition, utiliser AppStateMachine_AddTransition()
 * ou modifier cette table.
 * 
 * ORDRE : Les transitions sont vérifiées dans l'ordre de la table.
 * La première correspondance trouvée est utilisée.
 */
static const AppStateTransition_t DEFAULT_TRANSITIONS[] = {
    /* ================================================================ */
    /*  INIT → SPLASH (démarrage normal)                                */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_INIT,
        .to_state     = APP_STATE_SPLASH,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Demarrage → Splash screen"
    },

    /* ================================================================ */
    /*  SPLASH → LOCKED ou IDLE (fin du splash)                         */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_SPLASH,
        .to_state     = APP_STATE_LOCKED,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Splash → Verrouillage (PIN configuré)"
    },
    {
        .from_state   = APP_STATE_SPLASH,
        .to_state     = APP_STATE_IDLE,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Splash → Accueil (pas de PIN)"
    },

    /* ================================================================ */
    /*  LOCKED → IDLE (déverrouillage réussi)                           */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_LOCKED,
        .to_state     = APP_STATE_IDLE,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Verrouillage → Accueil"
    },

    /* ================================================================ */
    /*  LOCKED → INCOMING_CALL (appel entrant même verrouillé)          */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_LOCKED,
        .to_state     = APP_STATE_INCOMING_CALL,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Verrouillage → Appel entrant"
    },

    /* ================================================================ */
    /*  IDLE → Divers (navigation depuis l'accueil)                     */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_IDLE,
        .to_state     = APP_STATE_ACTIVE,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Accueil → Actif"
    },
    {
        .from_state   = APP_STATE_IDLE,
        .to_state     = APP_STATE_DIALING,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Accueil → Composeur"
    },
    {
        .from_state   = APP_STATE_IDLE,
        .to_state     = APP_STATE_SETTINGS,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Accueil → Paramètres"
    },
    {
        .from_state   = APP_STATE_IDLE,
        .to_state     = APP_STATE_MESSAGING,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Accueil → Messages"
    },
    {
        .from_state   = APP_STATE_IDLE,
        .to_state     = APP_STATE_INCOMING_CALL,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Accueil → Appel entrant"
    },
    {
        .from_state   = APP_STATE_IDLE,
        .to_state     = APP_STATE_LOCKED,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Accueil → Verrouillage (timeout)"
    },

    /* ================================================================ */
    /*  ACTIVE → Retours et navigation                                  */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_ACTIVE,
        .to_state     = APP_STATE_IDLE,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Actif → Accueil"
    },
    {
        .from_state   = APP_STATE_ACTIVE,
        .to_state     = APP_STATE_DIALING,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Actif → Composeur"
    },
    {
        .from_state   = APP_STATE_ACTIVE,
        .to_state     = APP_STATE_SETTINGS,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Actif → Paramètres"
    },
    {
        .from_state   = APP_STATE_ACTIVE,
        .to_state     = APP_STATE_INCOMING_CALL,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Actif → Appel entrant"
    },

    /* ================================================================ */
    /*  DIALING → IN_CALL ou IDLE                                       */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_DIALING,
        .to_state     = APP_STATE_IN_CALL,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Numérotation → Appel en cours"
    },
    {
        .from_state   = APP_STATE_DIALING,
        .to_state     = APP_STATE_IDLE,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Numérotation → Annulation"
    },

    /* ================================================================ */
    /*  IN_CALL → IDLE (fin d'appel)                                    */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_IN_CALL,
        .to_state     = APP_STATE_IDLE,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Appel → Fin d'appel"
    },

    /* ================================================================ */
    /*  INCOMING_CALL → IN_CALL ou IDLE ou LOCKED                       */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_INCOMING_CALL,
        .to_state     = APP_STATE_IN_CALL,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Appel entrant → Accepté"
    },
    {
        .from_state   = APP_STATE_INCOMING_CALL,
        .to_state     = APP_STATE_IDLE,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Appel entrant → Refusé"
    },
    {
        .from_state   = APP_STATE_INCOMING_CALL,
        .to_state     = APP_STATE_LOCKED,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Appel entrant → Refusé (retour verrouillage)"
    },

    /* ================================================================ */
    /*  SETTINGS → IDLE (retour)                                        */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_SETTINGS,
        .to_state     = APP_STATE_IDLE,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Paramètres → Retour"
    },

    /* ================================================================ */
    /*  MESSAGING → IDLE (retour)                                       */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_MESSAGING,
        .to_state     = APP_STATE_IDLE,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Messages → Retour"
    },

    /* ================================================================ */
    /*  ANY → ERROR (exception critique)                                */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_COUNT,   /* COUNT = ANY */
        .to_state     = APP_STATE_ERROR,
        .guard        = NULL,
        .action       = NULL,
        .description  = "QUELCONQUE → Erreur critique"
    },

    /* ================================================================ */
    /*  ANY → SHUTDOWN (extinction)                                     */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_COUNT,   /* COUNT = ANY */
        .to_state     = APP_STATE_SHUTDOWN,
        .guard        = NULL,
        .action       = NULL,
        .description  = "QUELCONQUE → Extinction"
    },

    /* ================================================================ */
    /*  ERROR → SHUTDOWN ou INIT                                        */
    /* ================================================================ */
    {
        .from_state   = APP_STATE_ERROR,
        .to_state     = APP_STATE_SHUTDOWN,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Erreur → Extinction"
    },
    {
        .from_state   = APP_STATE_ERROR,
        .to_state     = APP_STATE_INIT,
        .guard        = NULL,
        .action       = NULL,
        .description  = "Erreur → Redémarrage"
    },
};

/** Nombre de transitions dans la table par défaut */
#define DEFAULT_TRANSITION_COUNT    (sizeof(DEFAULT_TRANSITIONS) / sizeof(DEFAULT_TRANSITIONS[0]))

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

static const AppStateTransition_t* find_transition(AppStateMachine_t* sm,
                                                    AppState_t from,
                                                    AppState_t to);

static bool execute_transition(AppStateMachine_t* sm,
                               const AppStateTransition_t* transition);

static void update_stats(AppStateMachine_t* sm,
                         AppState_t from,
                         AppState_t to);

static void log_transition(const char* from_name,
                           const char* to_name,
                           bool success,
                           const char* reason);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise la machine d'états
 */
void AppStateMachine_Init(AppStateMachine_t* sm)
{
    if (!sm) return;

    DEBUG_INFO(TAG, "Initialisation de la machine d'états...");

    /* Mise à zéro */
    memset(sm, 0, sizeof(AppStateMachine_t));

    /* État initial */
    sm->current_state = APP_STATE_INIT;
    sm->previous_state = APP_STATE_INIT;
    sm->state_entered_ms = HAL_GetTick();

    /* Copier les transitions par défaut */
    sm->transition_count = 0;
    for (uint32_t i = 0; i < DEFAULT_TRANSITION_COUNT && 
         sm->transition_count < APP_STATE_MAX_TRANSITIONS; i++) {
        memcpy(&sm->transitions[sm->transition_count],
               &DEFAULT_TRANSITIONS[i],
               sizeof(AppStateTransition_t));
        sm->transition_count++;
    }

    /* Callbacks */
    sm->on_transition = NULL;
    sm->callback_context = NULL;

    /* Stats */
    memset(&sm->stats, 0, sizeof(sm->stats));

    /* Configuration */
    sm->is_locked = false;
    sm->log_transitions = true;

    DEBUG_INFO(TAG, "Machine d'états initialisée (%d états, %d transitions)",
               APP_STATE_COUNT, sm->transition_count);
}

/**
 * @brief Réinitialise la machine d'états
 */
void AppStateMachine_Reset(AppStateMachine_t* sm)
{
    if (!sm) return;

    DEBUG_INFO(TAG, "Réinitialisation de la machine d'états");

    sm->current_state = APP_STATE_INIT;
    sm->previous_state = APP_STATE_INIT;
    sm->state_entered_ms = HAL_GetTick();
    sm->is_locked = false;

    memset(&sm->stats, 0, sizeof(sm->stats));
}

/**
 * @brief Tente une transition vers un nouvel état
 */
bool AppStateMachine_Transition(AppStateMachine_t* sm, AppState_t new_state)
{
    if (!sm) return false;

    /* Vérifier si la machine est verrouillée */
    if (sm->is_locked) {
        log_transition(STATE_NAMES[sm->current_state],
                       STATE_NAMES[new_state],
                       false, "Machine verrouillée");
        sm->stats.error_transition_count++;
        return false;
    }

    /* Vérifier si c'est le même état */
    if (sm->current_state == new_state) {
        /* Même état : pas d'erreur mais rien à faire */
        return true;
    }

    /* Vérifier si la transition est autorisée */
    if (!AppStateMachine_CanTransition(sm, new_state)) {
        log_transition(STATE_NAMES[sm->current_state],
                       STATE_NAMES[new_state],
                       false, "Transition non autorisée");
        sm->stats.error_transition_count++;
        return false;
    }

    /* Trouver la définition de la transition */
    const AppStateTransition_t* transition = find_transition(sm,
                                                              sm->current_state,
                                                              new_state);
    if (!transition) {
        log_transition(STATE_NAMES[sm->current_state],
                       STATE_NAMES[new_state],
                       false, "Transition introuvable");
        sm->stats.error_transition_count++;
        return false;
    }

    /* Vérifier la guard (condition) */
    if (transition->guard) {
        if (!transition->guard(sm->current_state, new_state, sm->callback_context)) {
            log_transition(STATE_NAMES[sm->current_state],
                           STATE_NAMES[new_state],
                           false, "Guard refusée");
            sm->stats.error_transition_count++;
            return false;
        }
    }

    /* Exécuter la transition */
    return execute_transition(sm, transition);
}

/**
 * @brief Vérifie si une transition est possible
 */
bool AppStateMachine_CanTransition(AppStateMachine_t* sm, AppState_t new_state)
{
    if (!sm) return false;

    /* Vérifier si la machine est verrouillée */
    if (sm->is_locked) return false;

    /* Vérifier si la transition existe */
    const AppStateTransition_t* transition = find_transition(sm,
                                                              sm->current_state,
                                                              new_state);
    if (!transition) return false;

    /* Vérifier la guard */
    if (transition->guard) {
        if (!transition->guard(sm->current_state, new_state, sm->callback_context)) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Force une transition sans vérification
 */
void AppStateMachine_ForceTransition(AppStateMachine_t* sm, AppState_t new_state)
{
    if (!sm) return;

    DEBUG_WARN(TAG, "Transition FORCÉE: %s → %s",
               STATE_NAMES[sm->current_state],
               STATE_NAMES[new_state]);

    /* Mettre à jour les stats */
    update_stats(sm, sm->current_state, new_state);

    /* Changer l'état directement */
    sm->previous_state = sm->current_state;
    sm->current_state = new_state;
    sm->state_entered_ms = HAL_GetTick();

    /* Callback */
    if (sm->on_transition) {
        sm->on_transition(sm->previous_state, new_state, sm->callback_context);
    }
}

/**
 * @brief Retourne à l'état précédent
 */
bool AppStateMachine_GoBack(AppStateMachine_t* sm)
{
    if (!sm) return false;

    AppState_t prev = sm->previous_state;

    /* Ne pas boucler si même état */
    if (prev == sm->current_state) {
        /* Pas d'historique, retour à IDLE */
        prev = APP_STATE_IDLE;
    }

    DEBUG_INFO(TAG, "Retour: %s → %s",
               STATE_NAMES[sm->current_state],
               STATE_NAMES[prev]);

    return AppStateMachine_Transition(sm, prev);
}

/**
 * @brief Ajoute une transition personnalisée
 */
bool AppStateMachine_AddTransition(AppStateMachine_t* sm,
                                   AppState_t from_state,
                                   AppState_t to_state,
                                   AppStateGuard_t guard,
                                   AppStateAction_t action,
                                   const char* description)
{
    if (!sm) return false;

    /* Vérifier si la table est pleine */
    if (sm->transition_count >= APP_STATE_MAX_TRANSITIONS) {
        DEBUG_ERROR(TAG, "Table de transitions pleine (%d max)", APP_STATE_MAX_TRANSITIONS);
        return false;
    }

    /* Vérifier si elle n'existe pas déjà */
    if (find_transition(sm, from_state, to_state)) {
        DEBUG_WARN(TAG, "Transition %s→%s existe déjà",
                   STATE_NAMES[from_state], STATE_NAMES[to_state]);
        return false;
    }

    /* Ajouter */
    AppStateTransition_t* t = &sm->transitions[sm->transition_count];
    t->from_state = from_state;
    t->to_state = to_state;
    t->guard = guard;
    t->action = action;
    t->description = description;

    sm->transition_count++;

    DEBUG_INFO(TAG, "Transition ajoutée: %s → %s (%s)",
               STATE_NAMES[from_state], STATE_NAMES[to_state],
               description ? description : "sans description");

    return true;
}

/**
 * @brief Définit le callback de transition
 */
void AppStateMachine_SetCallback(AppStateMachine_t* sm,
                                 AppStateCallback_t callback,
                                 void* context)
{
    if (!sm) return;

    sm->on_transition = callback;
    sm->callback_context = context;
}

/**
 * @brief Verrouille/déverrouille la machine
 */
void AppStateMachine_Lock(AppStateMachine_t* sm, bool locked)
{
    if (!sm) return;

    sm->is_locked = locked;
    DEBUG_INFO(TAG, "Machine %s", locked ? "VERROUILLÉE" : "DÉVERROUILLÉE");
}

/**
 * @brief Active/désactive les logs
 */
void AppStateMachine_SetLogging(AppStateMachine_t* sm, bool enable)
{
    if (!sm) return;
    sm->log_transitions = enable;
}

/**
 * @brief Récupère l'état courant
 */
AppState_t AppStateMachine_GetState(AppStateMachine_t* sm)
{
    return sm ? sm->current_state : APP_STATE_ERROR;
}

/**
 * @brief Récupère l'état précédent
 */
AppState_t AppStateMachine_GetPreviousState(AppStateMachine_t* sm)
{
    return sm ? sm->previous_state : APP_STATE_ERROR;
}

/**
 * @brief Retourne le nom lisible d'un état
 */
const char* AppStateMachine_GetStateName(AppState_t state)
{
    if (state >= APP_STATE_COUNT) return "INCONNU";
    return STATE_NAMES[state];
}

/**
 * @brief Calcule le temps passé dans l'état courant
 */
uint32_t AppStateMachine_GetCurrentStateTime(AppStateMachine_t* sm)
{
    if (!sm) return 0;
    return HAL_GetTick() - sm->state_entered_ms;
}

/**
 * @brief Récupère les statistiques
 */
void AppStateMachine_GetStats(AppStateMachine_t* sm,
                              AppStateMachineStats_t* stats)
{
    if (!sm || !stats) return;

    memcpy(stats, &sm->stats, sizeof(AppStateMachineStats_t));
}

/**
 * @brief Réinitialise les statistiques
 */
void AppStateMachine_ResetStats(AppStateMachine_t* sm)
{
    if (!sm) return;
    memset(&sm->stats, 0, sizeof(sm->stats));
}

/**
 * @brief Vérifie si la machine est dans un état donné
 */
bool AppStateMachine_IsInState(AppStateMachine_t* sm, AppState_t state)
{
    if (!sm) return false;
    return sm->current_state == state;
}

/**
 * @brief Vérifie si la machine est dans un des états donnés
 */
bool AppStateMachine_IsInAnyState(AppStateMachine_t* sm,
                                  const AppState_t* states,
                                  uint8_t count)
{
    if (!sm || !states) return false;

    for (uint8_t i = 0; i < count; i++) {
        if (sm->current_state == states[i]) {
            return true;
        }
    }
    return false;
}

/* ======================================================================== */
/*              FONCTIONS PRIVÉES                                           */
/* ======================================================================== */

/**
 * @brief Trouve la définition d'une transition dans la table
 * 
 * Cherche d'abord une correspondance exacte (from→to),
 * puis cherche une correspondance générique (ANY→to).
 * 
 * @param sm    Machine d'états
 * @param from  État source
 * @param to    État destination
 * @return      Pointeur vers la transition, ou NULL si non trouvée
 */
static const AppStateTransition_t* find_transition(AppStateMachine_t* sm,
                                                    AppState_t from,
                                                    AppState_t to)
{
    if (!sm) return NULL;

    const AppStateTransition_t* any_match = NULL;

    for (uint8_t i = 0; i < sm->transition_count; i++) {
        const AppStateTransition_t* t = &sm->transitions[i];

        /* Correspondance exacte ? */
        if (t->from_state == from && t->to_state == to) {
            return t;
        }

        /* Transition générique (ANY) ? */
        if (t->from_state == APP_STATE_COUNT && t->to_state == to) {
            any_match = t;  /* Mémoriser pour plus tard */
        }
    }

    /* Retourner la correspondance ANY si trouvée */
    return any_match;
}

/**
 * @brief Exécute une transition complète
 * 
 * Étapes :
 * 1. Exécuter l'action de transition (si définie)
 * 2. Mettre à jour les statistiques
 * 3. Changer l'état
 * 4. Appeler le callback global
 * 5. Logger la transition
 * 
 * @param sm            Machine d'états
 * @param transition    Transition à exécuter
 * @return              true si succès
 */
static bool execute_transition(AppStateMachine_t* sm,
                               const AppStateTransition_t* transition)
{
    if (!sm || !transition) return false;

    AppState_t from = sm->current_state;
    AppState_t to = transition->to_state;

    /* 1. Exécuter l'action de transition */
    if (transition->action) {
        transition->action(from, to, sm->callback_context);
    }

    /* 2. Mettre à jour les statistiques */
    update_stats(sm, from, to);

    /* 3. Changer l'état */
    sm->previous_state = from;
    sm->current_state = to;
    sm->state_entered_ms = HAL_GetTick();

    /* 4. Callback global */
    if (sm->on_transition) {
        sm->on_transition(from, to, sm->callback_context);
    }

    /* 5. Logger */
    log_transition(STATE_NAMES[from],
                   STATE_NAMES[to],
                   true,
                   transition->description);

    return true;
}

/**
 * @brief Met à jour les statistiques de la machine
 * 
 * Incrémente les compteurs et calcule le temps passé
 * dans l'état précédent.
 * 
 * @param sm    Machine d'états
 * @param from  État source
 * @param to    État destination
 */
static void update_stats(AppStateMachine_t* sm,
                         AppState_t from,
                         AppState_t to)
{
    if (!sm) return;

    /* Incrémenter le compteur de transitions */
    sm->stats.transition_count++;

    /* Calculer le temps passé dans l'état précédent */
    uint32_t time_in_state = HAL_GetTick() - sm->state_entered_ms;
    sm->stats.state_time_ms[from] += time_in_state;

    /* Incrémenter le compteur d'entrée dans le nouvel état */
    sm->stats.state_entry_count[to]++;

    /* Mémoriser le dernier état */
    sm->stats.last_state = from;
    sm->stats.last_transition_ms = HAL_GetTick();
}

/**
 * @brief Log une transition
 * 
 * Affiche un message de log avec le niveau approprié
 * selon le succès ou l'échec de la transition.
 * 
 * @param from_name Nom de l'état source
 * @param to_name   Nom de l'état destination
 * @param success   true si la transition a réussi
 * @param reason    Raison (description ou raison d'échec)
 */
static void log_transition(const char* from_name,
                           const char* to_name,
                           bool success,
                           const char* reason)
{
    if (success) {
        DEBUG_INFO(TAG, "%-12s → %-12s [%s]",
                   from_name, to_name, reason ? reason : "OK");
    } else {
        DEBUG_WARN(TAG, "%-12s → %-12s REFUSÉE [%s]",
                   from_name, to_name, reason ? reason : "raison inconnue");
    }
}

/* ======================================================================== */
/*              EXEMPLE DE GUARDS PERSONNALISÉES                            */
/* ======================================================================== */

#if 0  /* Exemples - Non compilés */

/**
 * @brief Guard : Vérifie que le module LoRa est prêt avant un appel
 */
static bool guard_lora_ready(AppState_t from, AppState_t to, void* ctx)
{
    /* Vérifier que le driver LoRa est initialisé et prêt */
    /* return LoRaDriver_IsReady(); */
    return true;
}

/**
 * @brief Guard : Vérifie que l'écran n'est pas en timeout
 */
static bool guard_screen_active(AppState_t from, AppState_t to, void* ctx)
{
    /* Vérifier que l'écran n'est pas en veille */
    /* return DisplayManager_IsActive(); */
    return true;
}

/**
 * @brief Guard : Vérifie le niveau de batterie avant une action longue
 */
static bool guard_battery_sufficient(AppState_t from, AppState_t to, void* ctx)
{
    /* Vérifier que la batterie est > 10% */
    /* return PowerManager_GetBatteryPercent() > 10; */
    return true;
}

#endif

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */