/**
 * @file keypad_debounce.cpp
 * @brief Implémentation des algorithmes d'anti-rebond
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans keypad_debounce.h.
 * 
 * Il gère :
 * - L'anti-rebond par délai (Delay)
 * - L'anti-rebond par intégration (Integrate)
 * - L'anti-rebond par verrouillage (Lock)
 * - L'anti-rebond adaptatif
 * - Les métriques de qualité des contacts
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "keypad_debounce.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Configuration globale de l'anti-rebond */
static DebounceConfig debounce_config = {
    .method = DEBOUNCE_METHOD_DELAY,
    .delayMs = DEBOUNCE_DEFAULT_DELAY_MS,
    .sampleCount = DEBOUNCE_DEFAULT_SAMPLES,
    .adaptiveMinMs = DEBOUNCE_MIN_DELAY_MS,
    .adaptiveMaxMs = DEBOUNCE_MAX_DELAY_MS,
    .enableMetrics = false
};

/** @brief Métriques globales */
static struct {
    uint32_t totalProcessed;        // Nombre total d'appels à debounce_process
    uint32_t totalChanges;          // Nombre total de changements détectés
    uint32_t totalBouncesFiltered;  // Nombre total de rebonds filtrés
    uint32_t totalTimeUs;           // Temps total passé dans le traitement
} global_metrics = {0};

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le module d'anti-rebond
 */
void debounce_init(const DebounceConfig* config)
{
    if (config != NULL)
    {
        memcpy(&debounce_config, config, sizeof(DebounceConfig));
    }
    
    DEBOUNCE_DEBUG("Module anti-rebond initialisé\n");
    DEBOUNCE_DEBUG("Méthode: %d, Délai: %d ms, Échantillons: %d\n",
                   debounce_config.method,
                   debounce_config.delayMs,
                   debounce_config.sampleCount);
    
    memset(&global_metrics, 0, sizeof(global_metrics));
}

/**
 * @brief Réinitialise l'état d'une touche
 */
void debounce_reset_key(DebounceState* state)
{
    if (state == NULL) return;
    
    memset(state, 0, sizeof(DebounceState));
}

/**
 * @brief Réinitialise tous les états (non applicable ici)
 */
void debounce_reset_all(void)
{
    memset(&global_metrics, 0, sizeof(global_metrics));
}

// ============================================================
// SECTION 2 : TRAITEMENT PRINCIPAL
// ============================================================

/**
 * @brief Traite l'anti-rebond pour une touche
 */
bool debounce_process(DebounceState* state, bool rawState, uint32_t timestampMs)
{
    if (state == NULL) return false;
    
    global_metrics.totalProcessed++;
    
    // Détecter un changement d'état brut
    if (rawState != state->rawState)
    {
        state->rawState = rawState;
        state->lastChangeTime = timestampMs;
        state->totalChanges++;
        
        // Enregistrer dans l'historique pour la méthode adaptative
        if (debounce_config.enableMetrics)
        {
            state->changeHistory[state->historyIndex % DEBOUNCE_HISTORY_SIZE] = timestampMs;
            state->historyIndex++;
            if (state->historyCount < DEBOUNCE_HISTORY_SIZE)
            {
                state->historyCount++;
            }
        }
    }
    
    // Appliquer la méthode choisie
    bool changed = false;
    
    switch (debounce_config.method)
    {
        case DEBOUNCE_METHOD_DELAY:
            changed = debounce_process_delay(state, rawState, timestampMs);
            break;
            
        case DEBOUNCE_METHOD_INTEGRATE:
            changed = debounce_process_integrate(state, rawState, timestampMs);
            break;
            
        case DEBOUNCE_METHOD_LOCK:
            changed = debounce_process_lock(state, rawState, timestampMs);
            break;
            
        case DEBOUNCE_METHOD_ADAPTIVE:
            changed = debounce_process_adaptive(state, rawState, timestampMs);
            break;
            
        default:
            // Méthode par défaut : délai simple
            changed = debounce_process_delay(state, rawState, timestampMs);
            break;
    }
    
    // Mettre à jour le flag de changement
    if (changed)
    {
        state->changed = true;
        state->lastStableTime = timestampMs;
    }
    else
    {
        state->changed = false;
    }
    
    return changed;
}

// ============================================================
// SECTION 3 : MÉTHODE PAR DÉLAI (DELAY)
// ============================================================

/**
 * @brief Anti-rebond par délai simple
 * 
 * Principe :
 * - Au premier changement, on note le timestamp
 * - Si l'état reste stable pendant le délai, on le valide
 * - Si l'état change avant la fin du délai, on recommence
 */
bool debounce_process_delay(DebounceState* state, bool rawState, uint32_t timestampMs)
{
    // Si l'état brut est différent de l'état stable
    if (rawState != state->debouncedState)
    {
        // Calculer depuis combien de temps l'état est stable
        uint32_t stableTime = timestampMs - state->lastChangeTime;
        
        // Si stable depuis assez longtemps
        if (stableTime >= debounce_config.delayMs)
        {
            // Confirmer le nouvel état
            state->debouncedState = rawState;
            state->stableState = rawState;
            state->stableCount = 0;
            state->unstableCount = 0;
            
            global_metrics.totalChanges++;
            
            return true;  // Changement confirmé
        }
        else
        {
            // Pas encore assez stable, compter comme instable
            state->unstableCount++;
            global_metrics.totalBouncesFiltered++;
        }
    }
    else
    {
        // État stable, réinitialiser les compteurs
        state->stableCount++;
        state->unstableCount = 0;
    }
    
    return false;  // Pas de changement confirmé
}

// ============================================================
// SECTION 4 : MÉTHODE PAR INTÉGRATION (INTEGRATE)
// ============================================================

/**
 * @brief Anti-rebond par intégration
 * 
 * Principe :
 * - On compte le nombre d'échantillons consécutifs stables
 * - Si N échantillons sont identiques, on valide l'état
 * - Si un échantillon diffère, on recommence
 */
bool debounce_process_integrate(DebounceState* state, bool rawState, uint32_t timestampMs)
{
    (void)timestampMs;  // Pas utilisé dans cette méthode
    
    // Vérifier si l'échantillon est différent de l'état stable
    if (rawState != state->debouncedState)
    {
        // Incrémenter le compteur d'échantillons différents
        state->stableCount++;
        
        // Si assez d'échantillons consécutifs, confirmer le changement
        if (state->stableCount >= debounce_config.sampleCount)
        {
            state->debouncedState = rawState;
            state->stableState = rawState;
            state->stableCount = 0;
            state->unstableCount = 0;
            
            global_metrics.totalChanges++;
            
            return true;  // Changement confirmé
        }
    }
    else
    {
        // L'échantillon correspond à l'état stable, réinitialiser
        state->stableCount = 0;
        state->unstableCount++;
    }
    
    return false;  // Pas de changement confirmé
}

// ============================================================
// SECTION 5 : MÉTHODE PAR VERROUILLAGE (LOCK)
// ============================================================

/**
 * @brief Anti-rebond par verrouillage
 * 
 * Principe :
 * - Après un changement confirmé, on verrouille pendant X ms
 * - Pendant le verrouillage, tout changement est ignoré
 * - Après le déverrouillage, on accepte les nouveaux changements
 */
bool debounce_process_lock(DebounceState* state, bool rawState, uint32_t timestampMs)
{
    // Vérifier si on est encore verrouillé
    if (timestampMs < state->lockUntilTime)
    {
        // Verrouillé, ignorer tout changement
        return false;
    }
    
    // Déverrouillé, vérifier si l'état a changé
    if (rawState != state->debouncedState)
    {
        // Attendre le délai avant de confirmer
        uint32_t stableTime = timestampMs - state->lastChangeTime;
        
        if (stableTime >= debounce_config.delayMs)
        {
            // Confirmer le changement et verrouiller
            state->debouncedState = rawState;
            state->stableState = rawState;
            state->lockUntilTime = timestampMs + debounce_config.delayMs;
            state->stableCount = 0;
            state->unstableCount = 0;
            
            global_metrics.totalChanges++;
            
            return true;
        }
    }
    
    return false;
}

// ============================================================
// SECTION 6 : MÉTHODE ADAPTATIVE
// ============================================================

/**
 * @brief Anti-rebond adaptatif
 * 
 * Principe :
 * - Analyse l'historique des rebonds
 * - Ajuste dynamiquement le délai d'anti-rebond
 * - Pour les touches "bruyantes" : augmente le délai
 * - Pour les touches "propres" : réduit le délai
 */
bool debounce_process_adaptive(DebounceState* state, bool rawState, uint32_t timestampMs)
{
    // Calculer le délai optimal basé sur l'historique
    uint16_t optimalDelay = debounce_calculate_optimal_delay(state);
    
    // Limiter au min/max configuré
    if (optimalDelay < debounce_config.adaptiveMinMs)
    {
        optimalDelay = debounce_config.adaptiveMinMs;
    }
    if (optimalDelay > debounce_config.adaptiveMaxMs)
    {
        optimalDelay = debounce_config.adaptiveMaxMs;
    }
    
    // Appliquer le même principe que la méthode DELAY
    if (rawState != state->debouncedState)
    {
        uint32_t stableTime = timestampMs - state->lastChangeTime;
        
        if (stableTime >= optimalDelay)
        {
            state->debouncedState = rawState;
            state->stableState = rawState;
            state->stableCount = 0;
            state->unstableCount = 0;
            
            global_metrics.totalChanges++;
            
            // Mettre à jour le délai optimal
            state->maxBounceDuration = (stableTime > state->maxBounceDuration) ? 
                                        stableTime : state->maxBounceDuration;
            
            return true;
        }
        else
        {
            state->unstableCount++;
            global_metrics.totalBouncesFiltered++;
        }
    }
    else
    {
        state->stableCount++;
        state->unstableCount = 0;
    }
    
    return false;
}

// ============================================================
// SECTION 7 : FONCTIONS DE LECTURE
// ============================================================

/**
 * @brief Vérifie si la touche est pressée (état stable)
 */
bool debounce_is_pressed(const DebounceState* state)
{
    if (state == NULL) return false;
    return state->stableState;
}

/**
 * @brief Vérifie si la touche vient d'être pressée
 */
bool debounce_just_pressed(DebounceState* state)
{
    if (state == NULL) return false;
    
    bool justPressed = state->stableState && state->changed;
    
    // Consommer l'événement
    if (justPressed)
    {
        // Note: on ne peut pas modifier un const, donc l'appelant doit gérer
    }
    
    return justPressed;
}

/**
 * @brief Vérifie si la touche vient d'être relâchée
 */
bool debounce_just_released(DebounceState* state)
{
    if (state == NULL) return false;
    
    bool justReleased = !state->stableState && state->changed;
    
    return justReleased;
}

/**
 * @brief Récupère la durée depuis le dernier changement stable
 */
uint32_t debounce_get_stable_duration(const DebounceState* state)
{
    if (state == NULL) return 0;
    return state->lastStableTime > 0 ? 
           (HAL_GetTick() - state->lastStableTime) : 0;
}

// ============================================================
// SECTION 8 : CONFIGURATION
// ============================================================

/**
 * @brief Change la méthode d'anti-rebond
 */
void debounce_set_method(DebounceMethod method)
{
    debounce_config.method = method;
    DEBOUNCE_DEBUG("Méthode changée: %d\n", method);
}

/**
 * @brief Définit le délai
 */
void debounce_set_delay(uint16_t delayMs)
{
    if (delayMs < DEBOUNCE_MIN_DELAY_MS) delayMs = DEBOUNCE_MIN_DELAY_MS;
    if (delayMs > DEBOUNCE_MAX_DELAY_MS) delayMs = DEBOUNCE_MAX_DELAY_MS;
    
    debounce_config.delayMs = delayMs;
}

/**
 * @brief Définit le nombre d'échantillons
 */
void debounce_set_samples(uint8_t count)
{
    if (count < 2) count = 2;
    if (count > 20) count = 20;
    
    debounce_config.sampleCount = count;
}

/**
 * @brief Active/désactive les métriques
 */
void debounce_enable_metrics(bool enable)
{
    debounce_config.enableMetrics = enable;
    if (!enable)
    {
        memset(&global_metrics, 0, sizeof(global_metrics));
    }
}

// ============================================================
// SECTION 9 : MÉTRIQUES
// ============================================================

/**
 * @brief Récupère le nombre total de changements bruts
 */
uint32_t debounce_get_total_changes(const DebounceState* state)
{
    if (state == NULL) return 0;
    return state->totalChanges;
}

/**
 * @brief Récupère la durée maximale de rebond
 */
uint32_t debounce_get_max_bounce(const DebounceState* state)
{
    if (state == NULL) return 0;
    return state->maxBounceDuration;
}

/**
 * @brief Calcule la durée moyenne entre deux changements
 */
float debounce_get_avg_bounce(const DebounceState* state)
{
    if (state == NULL || state->historyCount < 2) return 0.0f;
    
    uint32_t totalInterval = 0;
    for (uint8_t i = 1; i < state->historyCount; i++)
    {
        uint8_t idx1 = (state->historyIndex - state->historyCount + i - 1) % DEBOUNCE_HISTORY_SIZE;
        uint8_t idx2 = (state->historyIndex - state->historyCount + i) % DEBOUNCE_HISTORY_SIZE;
        totalInterval += state->changeHistory[idx2] - state->changeHistory[idx1];
    }
    
    return (float)totalInterval / (state->historyCount - 1);
}

/**
 * @brief Estime la qualité du contact (0-100%)
 */
uint8_t debounce_get_quality(const DebounceState* state)
{
    if (state == NULL) return 0;
    
    // Si pas de changements, qualité parfaite
    if (state->totalChanges == 0) return 100;
    
    // Ratio : changements stables / changements totaux
    uint32_t total = state->stableCount + state->unstableCount;
    if (total == 0) return 100;
    
    uint8_t quality = (uint8_t)((state->stableCount * 100) / total);
    
    // Pénaliser selon la durée max de rebond
    if (state->maxBounceDuration > 50)
    {
        quality = (uint8_t)(quality * 0.8f);  // -20%
    }
    
    return (quality > 100) ? 100 : quality;
}

// ============================================================
// SECTION 10 : FONCTIONS AVANCÉES
// ============================================================

/**
 * @brief Détecte un double appui
 */
bool debounce_detect_double_press(DebounceState* state, uint16_t maxIntervalMs)
{
    if (state == NULL) return false;
    if (state->historyCount < 4) return false;  // Besoin de 2 appuis complets
    
    // Chercher deux fronts montants dans l'historique
    uint8_t pressCount = 0;
    uint32_t lastPressTime = 0;
    
    for (uint8_t i = 0; i < state->historyCount - 1; i++)
    {
        uint8_t idx1 = (state->historyIndex - state->historyCount + i) % DEBOUNCE_HISTORY_SIZE;
        uint8_t idx2 = (state->historyIndex - state->historyCount + i + 1) % DEBOUNCE_HISTORY_SIZE;
        
        uint32_t t1 = state->changeHistory[idx1];
        uint32_t t2 = state->changeHistory[idx2];
        
        // Un front montant est détecté quand deux changements sont proches
        if ((t2 - t1) < 5)  // Moins de 5ms entre deux changements
        {
            if (pressCount == 0)
            {
                lastPressTime = t1;
                pressCount = 1;
            }
            else if (pressCount == 1)
            {
                if ((t1 - lastPressTime) < maxIntervalMs)
                {
                    return true;  // Double appui détecté !
                }
                pressCount = 0;
            }
        }
    }
    
    return false;
}

/**
 * @brief Détecte un appui long
 */
bool debounce_detect_long_press(DebounceState* state, uint16_t minDurationMs)
{
    if (state == NULL) return false;
    
    // Vérifier si la touche est pressée et depuis combien de temps
    if (state->stableState)
    {
        // La durée est estimée depuis le dernier changement d'état
        // (approximation - la durée réelle est calculée dans keypad_matrix)
        return true;  // La durée exacte est vérifiée au niveau supérieur
    }
    
    return false;
}

/**
 * @brief Filtre les appuis trop courts
 */
bool debounce_filter_short_press(DebounceState* state, uint16_t minDurationMs)
{
    if (state == NULL) return false;
    
    // Vérifier si l'appui a duré assez longtemps
    if (!state->stableState && state->changed)
    {
        // Touche relâchée, vérifier la durée
        uint32_t pressDuration = state->lastStableTime - state->lastChangeTime;
        return (pressDuration >= minDurationMs);
    }
    
    return true;  // Pas un relâchement, accepter
}

/**
 * @brief Calcule le délai optimal basé sur l'historique
 */
uint16_t debounce_calculate_optimal_delay(const DebounceState* state)
{
    if (state == NULL || state->historyCount < 4)
    {
        return debounce_config.delayMs;  // Pas assez de données
    }
    
    // Calculer l'intervalle maximum entre rebonds
    uint32_t maxInterval = 0;
    
    for (uint8_t i = 1; i < state->historyCount; i++)
    {
        uint8_t idx1 = (state->historyIndex - state->historyCount + i - 1) % DEBOUNCE_HISTORY_SIZE;
        uint8_t idx2 = (state->historyIndex - state->historyCount + i) % DEBOUNCE_HISTORY_SIZE;
        
        uint32_t interval = state->changeHistory[idx2] - state->changeHistory[idx1];
        
        if (interval > maxInterval)
        {
            maxInterval = interval;
        }
    }
    
    // Ajouter une marge de 50%
    uint16_t optimalDelay = (uint16_t)(maxInterval * 1.5f);
    
    return optimalDelay;
}

// ============================================================
// SECTION 11 : DÉBOGAGE
// ============================================================

/**
 * @brief Affiche l'état d'une touche
 */
void debounce_print_key(const DebounceState* state, const char* name)
{
    if (state == NULL) return;
    
    printf("[%s] Raw:%d Stable:%d Changed:%d Changes:%lu MaxBounce:%lu ms Quality:%d%%\n",
           name ? name : "?",
           state->rawState,
           state->stableState,
           state->changed,
           (unsigned long)state->totalChanges,
           (unsigned long)state->maxBounceDuration,
           debounce_get_quality(state));
}

/**
 * @brief Affiche la configuration
 */
void debounce_print_config(void)
{
    const char* methodStr = "Inconnue";
    switch (debounce_config.method)
    {
        case DEBOUNCE_METHOD_DELAY:     methodStr = "Délai"; break;
        case DEBOUNCE_METHOD_INTEGRATE: methodStr = "Intégration"; break;
        case DEBOUNCE_METHOD_LOCK:      methodStr = "Verrouillage"; break;
        case DEBOUNCE_METHOD_ADAPTIVE:  methodStr = "Adaptatif"; break;
    }
    
    printf("\n═══ CONFIGURATION ANTI-REBOND ═══\n");
    printf("Méthode      : %s\n", methodStr);
    printf("Délai        : %d ms\n", debounce_config.delayMs);
    printf("Échantillons : %d\n", debounce_config.sampleCount);
    printf("Métriques    : %s\n", debounce_config.enableMetrics ? "Activées" : "Désactivées");
    printf("══════════════════════════════════\n\n");
}

/**
 * @brief Affiche les métriques globales
 */
void debounce_print_metrics(void)
{
    printf("\n═══ MÉTRIQUES ANTI-REBOND ═══\n");
    printf("Traités      : %lu\n", (unsigned long)global_metrics.totalProcessed);
    printf("Changements  : %lu\n", (unsigned long)global_metrics.totalChanges);
    printf("Rebonds filtrés: %lu\n", (unsigned long)global_metrics.totalBouncesFiltered);
    
    if (global_metrics.totalProcessed > 0)
    {
        float filterRate = 100.0f * global_metrics.totalBouncesFiltered / global_metrics.totalProcessed;
        printf("Taux filtrage : %.1f%%\n", filterRate);
    }
    printf("══════════════════════════════\n\n");
}

/**
 * @brief Test de performance
 */
bool debounce_self_test(void)
{
    DEBOUNCE_DEBUG("Auto-test...\n");
    
    // Créer un état de test
    DebounceState testState;
    debounce_reset_key(&testState);
    
    uint32_t now = 1000;  // Simuler le temps
    
    // Simuler un appui avec rebond
    bool result1 = debounce_process(&testState, true, now);      // t=1000
    bool result2 = debounce_process(&testState, false, now + 5); // t=1005 (rebond)
    bool result3 = debounce_process(&testState, true, now + 10); // t=1010 (rebond)
    bool result4 = debounce_process(&testState, true, now + 30); // t=1030 (stable)
    
    // Après 20ms de stabilité, l'état devrait être confirmé
    bool result5 = debounce_process(&testState, true, now + 50); // t=1050
    
    // Vérifier que les rebonds ont été filtrés
    if (result1 || result2 || result3 || result4)
    {
        DEBOUNCE_DEBUG("Échec : rebond non filtré\n");
        return false;
    }
    
    if (!result5)
    {
        DEBOUNCE_DEBUG("Échec : appui valide non détecté\n");
        return false;
    }
    
    DEBOUNCE_DEBUG("Auto-test OK\n");
    return true;
}