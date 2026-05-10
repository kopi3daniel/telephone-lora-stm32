/**
 * @file    app_state_machine.h
 * @brief   Machine d'états de l'application - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Gère les transitions entre les états globaux de l'application.
 * 
 * DIAGRAMME COMPLET DE LA MACHINE D'ÉTATS :
 * 
 *                         ┌─────────────┐
 *                         │    INIT     │ (démarrage)
 *                         └──────┬──────┘
 *                                │ init terminée
 *                                ▼
 *                         ┌─────────────┐
 *                         │   SPLASH    │ (écran démarrage)
 *                         └──────┬──────┘
 *                                │ splash terminé
 *                    ┌───────────┴───────────┐
 *                    │ PIN configuré ?        │
 *                    └───────────┬───────────┘
 *                      OUI ┌─────┴─────┐ NON
 *                          ▼             ▼
 *                   ┌──────────┐   ┌──────────┐
 *                   │  LOCKED  │   │   IDLE   │ (accueil)
 *                   └────┬─────┘   └────┬─────┘
 *                        │ déverrouillé  │
 *                        └──────┬────────┘
 *                               ▼
 *                        ┌──────────┐
 *                        │   IDLE   │◄──────────────────────────┐
 *                        └────┬─────┘                           │
 *           ┌─────────────────┼──────────────────┐              │
 *           │                 │                   │              │
 *           ▼                 ▼                    ▼              │
 *    ┌──────────┐    ┌──────────────┐    ┌──────────────┐       │
 *    │ DIALING  │    │  SETTINGS    │    │  MESSAGING   │       │
 *    └────┬─────┘    └──────┬───────┘    └──────┬───────┘       │
 *         │ appel lancé      │                   │               │
 *         ▼                  │                   │               │
 *    ┌──────────┐            │                   │               │
 *    │ IN_CALL  │            │                   │               │
 *    └────┬─────┘            │                   │               │
 *         │ fin d'appel       │                   │               │
 *         └───────────────────┴───────────────────┴───────────────┘
 *                              │ retour
 *                              ▼
 *                        ┌──────────┐
 *                        │   IDLE   │
 *                        └──────────┘
 * 
 *    ┌──────────────────────────────────────────────────────────┐
 *    │               ÉTAT SPÉCIAL : INCOMING_CALL               │
 *    │  Peut interrompre n'importe quel état sauf IN_CALL       │
 *    │  ┌──────────────────────────────────────────────────┐    │
 *    │  │  IDLE/DIALING/SETTINGS/MESSAGING → INCOMING_CALL │    │
 *    │  └──────────────────────────────────────────────────┘    │
 *    │  Accepté → IN_CALL                                       │
 *    │  Refusé  → État précédent                                │
 *    └──────────────────────────────────────────────────────────┘
 * 
 *    ┌──────────────────────────────────────────────────────────┐
 *    │              ÉTAT SPÉCIAL : ERROR / SHUTDOWN             │
 *    │  Peut interrompre TOUS les états                         │
 *    │  ERROR    → Redémarrage (si récupérable)                 │
 *    │  SHUTDOWN → Extinction complète                          │
 *    └──────────────────────────────────────────────────────────┘
 * 
 * RÈGLES DE TRANSITION :
 * 
 * 1. Une transition n'est valide que si elle est définie dans
 *    la table de transitions (state_transitions[][]).
 * 
 * 2. Chaque transition a :
 *    - Un état source
 *    - Un état destination
 *    - Une condition (guard) optionnelle
 *    - Une action à exécuter pendant la transition
 * 
 * 3. Les transitions sont atomiques : l'état ne change qu'après
 *    l'exécution complète de l'action de transition.
 * 
 * 4. État verrouillé (LOCKED) :
 *    - Seules les transitions vers UNLOCKED ou INCOMING_CALL sont autorisées
 *    - Tout autre événement est ignoré
 * 
 * 5. État en appel (IN_CALL) :
 *    - Les transitions sont limitées à END_CALL ou ERROR
 *    - L'appel entrant est ignoré (signal occupé)
 * 
 * 6. État erreur (ERROR) :
 *    - Seule la transition vers SHUTDOWN est autorisée
 *    - L'utilisateur doit redémarrer manuellement
 * 
 * EXEMPLE D'UTILISATION :
 * 
 *   // Transition simple
 *   AppStateMachine_Transition(APP_STATE_IDLE, APP_STATE_DIALING);
 * 
 *   // Transition avec condition
 *   if (AppStateMachine_CanTransition(APP_STATE_IDLE, APP_STATE_SETTINGS)) {
 *       AppStateMachine_Transition(APP_STATE_IDLE, APP_STATE_SETTINGS);
 *   }
 * 
 *   // Transition avec callback
 *   AppStateMachine_SetOnTransition(APP_STATE_DIALING, APP_STATE_IN_CALL, 
 *                                    on_call_started);
 */

#ifndef APP_STATE_MACHINE_H
#define APP_STATE_MACHINE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include <stdint.h>
#include <stdbool.h>

/* Type état (défini dans phone_app.h, redéfini ici pour indépendance) */
#ifndef APP_STATE_T_DEFINED
    #define APP_STATE_T_DEFINED
    typedef enum {
        APP_STATE_INIT = 0,
        APP_STATE_SPLASH,
        APP_STATE_LOCKED,
        APP_STATE_IDLE,
        APP_STATE_ACTIVE,
        APP_STATE_IN_CALL,
        APP_STATE_INCOMING_CALL,
        APP_STATE_DIALING,
        APP_STATE_MESSAGING,
        APP_STATE_SETTINGS,
        APP_STATE_ERROR,
        APP_STATE_SHUTDOWN,
        APP_STATE_COUNT          /**< Nombre total d'états (toujours en dernier) */
    } AppState_t;
#endif

/* ======================================================================== */
/*                     CONSTANTES SYMBOLIQUES                                */
/* ======================================================================== */

/**
 * @brief Nombre maximum de transitions définies
 */
#define APP_STATE_MAX_TRANSITIONS           32

/**
 * @brief Longueur maximale du nom d'un état (pour logs)
 */
#define APP_STATE_NAME_MAX_LENGTH           20

/* ======================================================================== */
/*                     TYPES                                                 */
/* ======================================================================== */

/**
 * @brief Action exécutée lors d'une transition
 * 
 * @param from_state    État source
 * @param to_state      État destination
 * @param context       Contexte utilisateur (optionnel)
 */
typedef void (*AppStateAction_t)(AppState_t from_state,
                                 AppState_t to_state,
                                 void* context);

/**
 * @brief Condition (guard) pour une transition
 * 
 * @param from_state    État source
 * @param to_state      État destination
 * @param context       Contexte utilisateur
 * @return              true si la transition est autorisée
 */
typedef bool (*AppStateGuard_t)(AppState_t from_state,
                                AppState_t to_state,
                                void* context);

/**
 * @brief Callback appelé après une transition réussie
 * 
 * @param from_state    Ancien état
 * @param new_state     Nouvel état
 * @param context       Contexte utilisateur
 */
typedef void (*AppStateCallback_t)(AppState_t from_state,
                                   AppState_t new_state,
                                   void* context);

/**
 * @brief Définition d'une transition d'état
 */
typedef struct {
    AppState_t          from_state;     /**< État source                     */
    AppState_t          to_state;       /**< État destination                */
    AppStateGuard_t     guard;          /**< Condition (NULL = toujours ok)  */
    AppStateAction_t    action;         /**< Action à exécuter (NULL = rien) */
    const char*         description;    /**< Description pour logs            */
} AppStateTransition_t;

/**
 * @brief Statistiques de la machine d'états
 */
typedef struct {
    uint32_t            transition_count;       /**< Nombre total de transitions */
    uint32_t            state_time_ms[APP_STATE_COUNT]; /**< Temps passé par état */
    uint32_t            state_entry_count[APP_STATE_COUNT]; /**< Entrées par état */
    AppState_t          last_state;             /**< Dernier état visité         */
    uint32_t            last_transition_ms;     /**< Timestamp dernière transition */
    uint32_t            error_transition_count; /**< Transitions refusées        */
} AppStateMachineStats_t;

/* ======================================================================== */
/*                     STRUCTURE PRINCIPALE                                  */
/* ======================================================================== */

/**
 * @brief Machine d'états de l'application
 * 
 * Maintient l'état courant et gère les transitions valides.
 * 
 * Taille approximative : ~500 octets + transitions
 */
typedef struct {
    /* ---- État courant ---- */
    AppState_t          current_state;          /**< État actuel                    */
    AppState_t          previous_state;         /**< État précédent                 */
    uint32_t            state_entered_ms;       /**< Timestamp entrée dans l'état   */

    /* ---- Table de transitions ---- */
    AppStateTransition_t transitions[APP_STATE_MAX_TRANSITIONS]; /**< Transitions   */
    uint8_t             transition_count;       /**< Nombre de transitions définies */

    /* ---- Callbacks ---- */
    AppStateCallback_t  on_transition;          /**< Appelé après chaque transition */
    void*               callback_context;       /**< Contexte pour les callbacks    */

    /* ---- Statistiques ---- */
    AppStateMachineStats_t stats;               /**< Statistiques d'utilisation     */

    /* ---- Verrouillage ---- */
    bool                is_locked;              /**< Machine verrouillée (aucune transition) */
    bool                log_transitions;        /**< Activer les logs de transition */

} AppStateMachine_t;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/**
 * @brief Initialise la machine d'états
 * 
 * Configure l'état initial (APP_STATE_INIT) et définit
 * toutes les transitions valides.
 * 
 * @param sm    Machine d'états à initialiser
 */
void AppStateMachine_Init(AppStateMachine_t* sm);

/**
 * @brief Réinitialise la machine d'états
 * 
 * Retourne à l'état INIT et efface les statistiques.
 * 
 * @param sm    Machine d'états
 */
void AppStateMachine_Reset(AppStateMachine_t* sm);

/**
 * @brief Tente une transition vers un nouvel état
 * 
 * Vérifie si la transition est valide, exécute la guard,
 * l'action de transition, puis met à jour l'état.
 * 
 * @param sm        Machine d'états
 * @param new_state État destination souhaité
 * @return          true si la transition a réussi
 */
bool AppStateMachine_Transition(AppStateMachine_t* sm,
                                AppState_t new_state);

/**
 * @brief Vérifie si une transition est possible
 * 
 * Vérifie sans effectuer la transition.
 * Utile pour activer/désactiver des UI.
 * 
 * @param sm        Machine d'états
 * @param new_state État destination
 * @return          true si la transition est autorisée
 */
bool AppStateMachine_CanTransition(AppStateMachine_t* sm,
                                   AppState_t new_state);

/**
 * @brief Force une transition (sans vérification)
 * 
 * ⚠️ DANGEREUX : Ignore les guards et les actions.
 * Réservé aux cas d'urgence (erreur fatale, shutdown).
 * 
 * @param sm        Machine d'états
 * @param new_state État destination forcé
 */
void AppStateMachine_ForceTransition(AppStateMachine_t* sm,
                                     AppState_t new_state);

/**
 * @brief Retourne à l'état précédent
 * 
 * Équivalent à Transition(previous_state).
 * 
 * @param sm    Machine d'états
 * @return      true si le retour a réussi
 */
bool AppStateMachine_GoBack(AppStateMachine_t* sm);

/**
 * @brief Ajoute une transition personnalisée
 * 
 * Permet d'étendre la table de transitions par défaut.
 * 
 * @param sm            Machine d'états
 * @param from_state    État source
 * @param to_state      État destination
 * @param guard         Condition (NULL = toujours)
 * @param action        Action (NULL = rien)
 * @param description   Description pour logs
 * @return              true si ajoutée
 */
bool AppStateMachine_AddTransition(AppStateMachine_t* sm,
                                   AppState_t from_state,
                                   AppState_t to_state,
                                   AppStateGuard_t guard,
                                   AppStateAction_t action,
                                   const char* description);

/**
 * @brief Définit le callback de transition globale
 * 
 * Appelé après chaque transition réussie.
 * 
 * @param sm        Machine d'états
 * @param callback  Fonction à appeler
 * @param context   Contexte utilisateur
 */
void AppStateMachine_SetCallback(AppStateMachine_t* sm,
                                 AppStateCallback_t callback,
                                 void* context);

/**
 * @brief Verrouille/déverrouille la machine d'états
 * 
 * Quand verrouillée, aucune transition n'est autorisée
 * sauf via ForceTransition.
 * 
 * @param sm        Machine d'états
 * @param locked    true = verrouiller
 */
void AppStateMachine_Lock(AppStateMachine_t* sm, bool locked);

/**
 * @brief Active/désactive les logs de transition
 * 
 * @param sm        Machine d'états
 * @param enable    true = activer
 */
void AppStateMachine_SetLogging(AppStateMachine_t* sm, bool enable);

/**
 * @brief Récupère l'état courant
 * @param sm    Machine d'états
 * @return      État actuel
 */
AppState_t AppStateMachine_GetState(AppStateMachine_t* sm);

/**
 * @brief Récupère l'état précédent
 * @param sm    Machine d'états
 * @return      État précédent
 */
AppState_t AppStateMachine_GetPreviousState(AppStateMachine_t* sm);

/**
 * @brief Retourne le nom lisible d'un état
 * 
 * @param state État
 * @return      Chaîne statique (ne pas libérer)
 */
const char* AppStateMachine_GetStateName(AppState_t state);

/**
 * @brief Calcule le temps passé dans l'état courant
 * 
 * @param sm    Machine d'états
 * @return      Temps en millisecondes
 */
uint32_t AppStateMachine_GetCurrentStateTime(AppStateMachine_t* sm);

/**
 * @brief Récupère les statistiques
 * 
 * @param sm    Machine d'états
 * @param stats [out] Statistiques copiées
 */
void AppStateMachine_GetStats(AppStateMachine_t* sm,
                              AppStateMachineStats_t* stats);

/**
 * @brief Réinitialise les statistiques
 * @param sm    Machine d'états
 */
void AppStateMachine_ResetStats(AppStateMachine_t* sm);

/**
 * @brief Vérifie si la machine est dans un état donné
 * 
 * @param sm    Machine d'états
 * @param state État à vérifier
 * @return      true si l'état courant correspond
 */
bool AppStateMachine_IsInState(AppStateMachine_t* sm, AppState_t state);

/**
 * @brief Vérifie si la machine est dans un des états donnés
 * 
 * @param sm        Machine d'états
 * @param states    Tableau d'états
 * @param count     Nombre d'états dans le tableau
 * @return          true si l'état courant est dans le tableau
 */
bool AppStateMachine_IsInAnyState(AppStateMachine_t* sm,
                                  const AppState_t* states,
                                  uint8_t count);

/* ======================================================================== */
/*              MACROS UTILITAIRES                                           */
/* ======================================================================== */

/**
 * @brief Vérifie si l'état courant est l'un des états donnés
 * 
 * Utilisation :
 *   if (APP_STATE_IS(APP_STATE_IDLE) || APP_STATE_IS(APP_STATE_ACTIVE)) { ... }
 * 
 * @note Nécessite une variable 'sm' dans le scope
 */
#define APP_STATE_IS(state)             AppStateMachine_IsInState(sm, state)

/**
 * @brief Vérifie si l'état courant est dans une liste
 * 
 * Utilisation :
 *   AppState_t idle_states[] = {APP_STATE_IDLE, APP_STATE_ACTIVE};
 *   if (APP_STATE_IS_ANY(idle_states, 2)) { ... }
 */
#define APP_STATE_IS_ANY(states, n)     AppStateMachine_IsInAnyState(sm, states, n)

/**
 * @brief Effectue une transition avec log
 */
#define APP_STATE_TRANSITION(new_state) \
    do { \
        DEBUG_INFO("SM", "Transition: %s → %s", \
                   AppStateMachine_GetStateName(sm->current_state), \
                   AppStateMachine_GetStateName(new_state)); \
        AppStateMachine_Transition(sm, new_state); \
    } while (0)

/* ======================================================================== */
/*              TABLE DES TRANSITIONS PAR DÉFAUT                             */
/* ======================================================================== */

/*
 * Transitions définies à l'initialisation :
 * 
 * INIT      → SPLASH               (init terminée)
 * SPLASH    → LOCKED               (PIN configuré)
 * SPLASH    → IDLE                 (pas de PIN)
 * LOCKED    → IDLE                 (déverrouillé)
 * LOCKED    → INCOMING_CALL        (appel entrant accepté)
 * IDLE      → DIALING              (lancer composeur)
 * IDLE      → SETTINGS             (ouvrir paramètres)
 * IDLE      → MESSAGING            (ouvrir messages)
 * IDLE      → ACTIVE               (interaction)
 * IDLE      → INCOMING_CALL        (appel entrant)
 * IDLE      → LOCKED               (timeout inactivité)
 * ACTIVE    → IDLE                 (retour accueil)
 * ACTIVE    → DIALING              (composer)
 * ACTIVE    → SETTINGS             (paramètres)
 * ACTIVE    → INCOMING_CALL        (appel entrant)
 * DIALING   → IN_CALL              (appel connecté)
 * DIALING   → IDLE                 (annulation)
 * IN_CALL   → IDLE                 (fin d'appel)
 * INCOMING_CALL → IN_CALL          (accepté)
 * INCOMING_CALL → IDLE             (refusé)
 * INCOMING_CALL → LOCKED           (refusé, était verrouillé)
 * SETTINGS  → IDLE                 (retour)
 * MESSAGING → IDLE                 (retour)
 * ANY       → ERROR               (erreur critique)
 * ANY       → SHUTDOWN             (extinction)
 * ERROR     → SHUTDOWN             (extinction après erreur)
 * ERROR     → INIT                 (redémarrage après erreur)
 */

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */