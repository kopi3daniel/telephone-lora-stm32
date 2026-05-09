/**
 * @file keypad_debounce.h
 * @brief Gestion de l'anti-rebond pour le clavier matriciel
 * 
 * Ce fichier implémente plusieurs algorithmes d'anti-rebond
 * pour éliminer les rebonds mécaniques des touches.
 * 
 * Méthodes disponibles :
 * - Debounce par délai (Delay) : simple, attend un délai fixe
 * - Debounce par intégration (Integrate) : compte les échantillons stables
 * - Debounce par verrouillage (Lock) : ignore les changements pendant un délai
 * 
 * Principe du rebond :
 * 
 * Appui réel :     ──┐
 *                   └──────────────────────────
 * 
 * Signal avec rebond :
 *                   ──┐┌┐┌┐┌┐┌────────────────
 *                     └┘└┘└┘└┘└┘
 *                     ▲▲▲▲▲▲▲▲▲ (transitions parasites)
 * 
 * Après anti-rebond :
 *                   ──┐
 *                     └──────────────────────────
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef KEYPAD_DEBOUNCE_H
#define KEYPAD_DEBOUNCE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define DEBOUNCE_VERSION                "1.0.0"

/** @brief Délai d'anti-rebond par défaut (ms) */
#define DEBOUNCE_DEFAULT_DELAY_MS       20

/** @brief Délai minimum (ms) */
#define DEBOUNCE_MIN_DELAY_MS           5

/** @brief Délai maximum (ms) */
#define DEBOUNCE_MAX_DELAY_MS           100

/** @brief Nombre d'échantillons pour l'intégration */
#define DEBOUNCE_DEFAULT_SAMPLES        5

/** @brief Nombre maximum de touches à gérer simultanément */
#define DEBOUNCE_MAX_KEYS               24

/** @brief Taille du buffer d'historique */
#define DEBOUNCE_HISTORY_SIZE           8

// ============================================================
// SECTION 2 : MÉTHODES D'ANTI-REBOND
// ============================================================

/**
 * @brief Méthodes d'anti-rebond disponibles
 */
typedef enum {
    DEBOUNCE_METHOD_DELAY       = 0,    // Délai simple (attendre X ms)
    DEBOUNCE_METHOD_INTEGRATE   = 1,    // Intégration (N échantillons stables)
    DEBOUNCE_METHOD_LOCK        = 2,    // Verrouillage (ignorer pendant X ms)
    DEBOUNCE_METHOD_ADAPTIVE    = 3     // Adaptatif (ajuste le délai automatiquement)
} DebounceMethod;

// ============================================================
// SECTION 3 : ÉTAT D'UNE TOUCHE
// ============================================================

/**
 * @brief État d'anti-rebond pour une touche
 */
typedef struct {
    // État actuel
    bool rawState;                  // État brut (avant anti-rebond)
    bool debouncedState;            // État après anti-rebond
    bool stableState;               // État stable confirmé
    bool changed;                   // Changement détecté
    
    // Timers
    uint32_t lastChangeTime;        // Dernier changement d'état brut
    uint32_t lastStableTime;        // Dernière confirmation d'état stable
    uint32_t lockUntilTime;         // Verrouillage jusqu'à (méthode LOCK)
    
    // Compteurs
    uint8_t stableCount;            // Compteur d'échantillons stables (méthode INTEGRATE)
    uint8_t unstableCount;          // Compteur d'échantillons instables
    
    // Historique (pour la méthode adaptative)
    uint32_t changeHistory[DEBOUNCE_HISTORY_SIZE];  // Horodatages des changements
    uint8_t historyIndex;                           // Index dans l'historique
    uint8_t historyCount;                           // Nombre d'entrées valides
    
    // Métriques
    uint32_t totalChanges;          // Nombre total de changements détectés
    uint32_t totalDebounceTime;     // Temps total passé en anti-rebond (µs)
    uint32_t maxBounceDuration;     // Durée maximale d'un rebond (µs)
} DebounceState;

// ============================================================
// SECTION 4 : CONFIGURATION GLOBALE
// ============================================================

/**
 * @brief Configuration de l'anti-rebond
 */
typedef struct {
    DebounceMethod method;          // Méthode choisie
    uint16_t delayMs;               // Délai (pour méthodes DELAY et LOCK)
    uint8_t sampleCount;            // Nombre d'échantillons (pour méthode INTEGRATE)
    uint16_t adaptiveMinMs;         // Délai minimum adaptatif
    uint16_t adaptiveMaxMs;         // Délai maximum adaptatif
    bool enableMetrics;             // Activer les métriques
} DebounceConfig;

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise le module d'anti-rebond
 * @param config Configuration (NULL = défaut)
 */
void debounce_init(const DebounceConfig* config);

/**
 * @brief Réinitialise l'état d'une touche
 * @param state État à réinitialiser
 */
void debounce_reset_key(DebounceState* state);

/**
 * @brief Réinitialise tous les états
 */
void debounce_reset_all(void);

// ============================================================
// SECTION 6 : FONCTIONS DE TRAITEMENT
// ============================================================

/**
 * @brief Traite l'anti-rebond pour une touche
 * 
 * À appeler à chaque scan du clavier avec l'état brut de la touche.
 * 
 * @param state État de la touche
 * @param rawState État brut actuel (true = pressée)
 * @param timestampMs Horodatage en ms
 * @return true si l'état stable a changé
 */
bool debounce_process(DebounceState* state, bool rawState, uint32_t timestampMs);

/**
 * @brief Traitement par délai simple
 */
bool debounce_process_delay(DebounceState* state, bool rawState, uint32_t timestampMs);

/**
 * @brief Traitement par intégration
 */
bool debounce_process_integrate(DebounceState* state, bool rawState, uint32_t timestampMs);

/**
 * @brief Traitement par verrouillage
 */
bool debounce_process_lock(DebounceState* state, bool rawState, uint32_t timestampMs);

/**
 * @brief Traitement adaptatif
 */
bool debounce_process_adaptive(DebounceState* state, bool rawState, uint32_t timestampMs);

// ============================================================
// SECTION 7 : FONCTIONS DE LECTURE
// ============================================================

/**
 * @brief Récupère l'état stable d'une touche (après anti-rebond)
 * @param state État de la touche
 * @return true si la touche est considérée comme pressée
 */
bool debounce_is_pressed(const DebounceState* state);

/**
 * @brief Vérifie si la touche vient d'être pressée (front montant)
 * @param state État de la touche
 * @return true si front montant détecté
 */
bool debounce_just_pressed(DebounceState* state);

/**
 * @brief Vérifie si la touche vient d'être relâchée (front descendant)
 * @param state État de la touche
 * @return true si front descendant détecté
 */
bool debounce_just_released(DebounceState* state);

/**
 * @brief Récupère la durée depuis le dernier changement stable
 * @param state État de la touche
 * @return Durée en ms
 */
uint32_t debounce_get_stable_duration(const DebounceState* state);

// ============================================================
// SECTION 8 : FONCTIONS DE CONFIGURATION
// ============================================================

/**
 * @brief Change la méthode d'anti-rebond
 * @param method Nouvelle méthode
 */
void debounce_set_method(DebounceMethod method);

/**
 * @brief Définit le délai d'anti-rebond
 * @param delayMs Délai en ms
 */
void debounce_set_delay(uint16_t delayMs);

/**
 * @brief Définit le nombre d'échantillons pour l'intégration
 * @param count Nombre d'échantillons
 */
void debounce_set_samples(uint8_t count);

/**
 * @brief Active/désactive les métriques
 * @param enable true pour activer
 */
void debounce_enable_metrics(bool enable);

// ============================================================
// SECTION 9 : FONCTIONS DE MÉTRIQUES
// ============================================================

/**
 * @brief Récupère le nombre total de changements
 * @param state État de la touche
 * @return Nombre de changements
 */
uint32_t debounce_get_total_changes(const DebounceState* state);

/**
 * @brief Récupère la durée maximale de rebond
 * @param state État de la touche
 * @return Durée en µs
 */
uint32_t debounce_get_max_bounce(const DebounceState* state);

/**
 * @brief Calcule la durée moyenne de rebond
 * @param state État de la touche
 * @return Durée moyenne en µs
 */
float debounce_get_avg_bounce(const DebounceState* state);

/**
 * @brief Estime la qualité du contact (0-100%)
 * @param state État de la touche
 * @return Qualité estimée (100 = parfait)
 */
uint8_t debounce_get_quality(const DebounceState* state);

// ============================================================
// SECTION 10 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état d'anti-rebond d'une touche
 * @param state État de la touche
 * @param name Nom de la touche
 */
void debounce_print_key(const DebounceState* state, const char* name);

/**
 * @brief Affiche la configuration actuelle
 */
void debounce_print_config(void);

/**
 * @brief Affiche les métriques globales
 */
void debounce_print_metrics(void);

/**
 * @brief Test de performance de l'anti-rebond
 * @return true si les tests passent
 */
bool debounce_self_test(void);

// ============================================================
// SECTION 11 : FONCTIONS AVANCÉES
// ============================================================

/**
 * @brief Détecte un appui double (deux appuis rapides)
 * @param state État de la touche
 * @param maxIntervalMs Intervalle maximum entre les deux appuis
 * @return true si double appui détecté
 */
bool debounce_detect_double_press(DebounceState* state, uint16_t maxIntervalMs);

/**
 * @brief Détecte un appui long
 * @param state État de la touche
 * @param minDurationMs Durée minimum en ms
 * @return true si appui long détecté
 */
bool debounce_detect_long_press(DebounceState* state, uint16_t minDurationMs);

/**
 * @brief Filtre les appuis trop courts (fausses détections)
 * @param state État de la touche
 * @param minDurationMs Durée minimum valide
 * @return true si l'appui est valide
 */
bool debounce_filter_short_press(DebounceState* state, uint16_t minDurationMs);

/**
 * @brief Calcule le délai d'anti-rebond optimal
 * 
 * Analyse l'historique des rebonds pour déterminer
 * le délai optimal pour cette touche.
 * 
 * @param state État de la touche
 * @return Délai optimal en ms
 */
uint16_t debounce_calculate_optimal_delay(const DebounceState* state);

// ============================================================
// SECTION 12 : MACROS UTILITAIRES
// ============================================================

/**
 * @brief Vérifie si un état est stable
 */
#define DEBOUNCE_IS_STABLE(state)       ((state)->stableCount >= debounce_config.sampleCount)

/**
 * @brief Vérifie si le délai de verrouillage est écoulé
 */
#define DEBOUNCE_IS_UNLOCKED(state, ts)  ((ts) >= (state)->lockUntilTime)

/**
 * @brief Réinitialise les compteurs d'une touche
 */
#define DEBOUNCE_RESET_COUNTERS(state)   do { \
    (state)->stableCount = 0; \
    (state)->unstableCount = 0; \
} while(0)

// ============================================================
// SECTION 13 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define DEBOUNCE_DEBUG(fmt, ...)    printf("[DEBOUNCE] " fmt, ##__VA_ARGS__)
#else
    #define DEBOUNCE_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 14 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // KEYPAD_DEBOUNCE_H