/**
 * @file ui_animations.cpp
 * @brief Implémentation du module d'animations
 * 
 * Fonctionnalités :
 * - 11 types d'animations (fade, slide, zoom, rotate, pulse, shake, bounce, color)
 * - 8 courbes d'accélération (easing functions)
 * - Gestion de la progression et des répétitions
 * - Callbacks (onStart, onUpdate, onComplete)
 * - Transitions entre écrans
 * - Interpolation de couleurs
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "ui_animations.h"
#include "../drivers/display/display_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du module */
static UIAnimationsState anim_state;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

bool ui_animations_init(void)
{
    ANIM_DEBUG("Initialisation du module d'animations...\n");
    
    memset(&anim_state, 0, sizeof(UIAnimationsState));
    anim_state.initialized = true;
    
    ANIM_DEBUG("Module initialisé\n");
    return true;
}

void ui_animations_deinit(void)
{
    ui_anim_stop_all();
    anim_state.initialized = false;
}

bool ui_animations_is_ready(void)
{
    return anim_state.initialized;
}

// ============================================================
// SECTION 2 : CRÉATION D'ANIMATIONS
// ============================================================

UIAnimation* ui_anim_create(AnimationType type, uint32_t durationMs)
{
    if (anim_state.activeCount >= UI_ANIMATIONS_MAX_ACTIVE)
    {
        ANIM_DEBUG("Plus de place pour une nouvelle animation\n");
        return NULL;
    }
    
    UIAnimation* anim = (UIAnimation*)calloc(1, sizeof(UIAnimation));
    if (anim == NULL) return NULL;
    
    anim->id = ++anim_state.nextId;
    anim->type = type;
    anim->state = ANIM_STATE_IDLE;
    anim->durationMs = durationMs;
    anim->easing = EASING_EASE_OUT;
    anim->direction = ANIM_DIR_FORWARD;
    anim->repeatCount = 1;
    anim->opacity.from = 1.0f;
    anim->opacity.to = 1.0f;
    anim->scaleX.from = 1.0f;
    anim->scaleX.to = 1.0f;
    anim->scaleY.from = 1.0f;
    anim->scaleY.to = 1.0f;
    
    // Configurer les valeurs selon le type
    switch (type)
    {
        case ANIM_TYPE_FADE:
            anim->opacity.from = 0.0f;
            anim->opacity.to = 1.0f;
            break;
        case ANIM_TYPE_SLIDE_LEFT:
            anim->positionX.from = (float)DISPLAY_WIDTH;
            anim->positionX.to = 0.0f;
            break;
        case ANIM_TYPE_SLIDE_RIGHT:
            anim->positionX.from = -(float)DISPLAY_WIDTH;
            anim->positionX.to = 0.0f;
            break;
        case ANIM_TYPE_SLIDE_UP:
            anim->positionY.from = (float)DISPLAY_HEIGHT;
            anim->positionY.to = 0.0f;
            break;
        case ANIM_TYPE_SLIDE_DOWN:
            anim->positionY.from = -(float)DISPLAY_HEIGHT;
            anim->positionY.to = 0.0f;
            break;
        case ANIM_TYPE_ZOOM_IN:
            anim->scaleX.from = 0.0f;
            anim->scaleX.to = 1.0f;
            anim->scaleY.from = 0.0f;
            anim->scaleY.to = 1.0f;
            break;
        case ANIM_TYPE_ZOOM_OUT:
            anim->scaleX.from = 1.5f;
            anim->scaleX.to = 1.0f;
            anim->scaleY.from = 1.5f;
            anim->scaleY.to = 1.0f;
            break;
        default:
            break;
    }
    
    return anim;
}

UIAnimation* ui_anim_create_fade(bool fadeIn, uint32_t durationMs)
{
    UIAnimation* anim = ui_anim_create(ANIM_TYPE_FADE, durationMs);
    if (anim)
    {
        if (fadeIn)
        {
            anim->opacity.from = 0.0f;
            anim->opacity.to = 1.0f;
        }
        else
        {
            anim->opacity.from = 1.0f;
            anim->opacity.to = 0.0f;
        }
    }
    return anim;
}

UIAnimation* ui_anim_create_slide_left(bool slideIn, uint32_t durationMs)
{
    UIAnimation* anim = ui_anim_create(ANIM_TYPE_SLIDE_LEFT, durationMs);
    if (anim)
    {
        if (slideIn)
        {
            anim->positionX.from = (float)DISPLAY_WIDTH;
            anim->positionX.to = 0.0f;
        }
        else
        {
            anim->positionX.from = 0.0f;
            anim->positionX.to = -(float)DISPLAY_WIDTH;
        }
    }
    return anim;
}

UIAnimation* ui_anim_create_slide_right(bool slideIn, uint32_t durationMs)
{
    UIAnimation* anim = ui_anim_create(ANIM_TYPE_SLIDE_RIGHT, durationMs);
    if (anim)
    {
        if (slideIn)
        {
            anim->positionX.from = -(float)DISPLAY_WIDTH;
            anim->positionX.to = 0.0f;
        }
        else
        {
            anim->positionX.from = 0.0f;
            anim->positionX.to = (float)DISPLAY_WIDTH;
        }
    }
    return anim;
}

UIAnimation* ui_anim_create_slide_up(bool slideIn, uint32_t durationMs)
{
    UIAnimation* anim = ui_anim_create(ANIM_TYPE_SLIDE_UP, durationMs);
    if (anim)
    {
        if (slideIn)
        {
            anim->positionY.from = (float)DISPLAY_HEIGHT;
            anim->positionY.to = 0.0f;
        }
        else
        {
            anim->positionY.from = 0.0f;
            anim->positionY.to = -(float)DISPLAY_HEIGHT;
        }
    }
    return anim;
}

UIAnimation* ui_anim_create_slide_down(bool slideIn, uint32_t durationMs)
{
    UIAnimation* anim = ui_anim_create(ANIM_TYPE_SLIDE_DOWN, durationMs);
    if (anim)
    {
        if (slideIn)
        {
            anim->positionY.from = -(float)DISPLAY_HEIGHT;
            anim->positionY.to = 0.0f;
        }
        else
        {
            anim->positionY.from = 0.0f;
            anim->positionY.to = (float)DISPLAY_HEIGHT;
        }
    }
    return anim;
}

UIAnimation* ui_anim_create_zoom(bool zoomIn, uint32_t durationMs)
{
    UIAnimation* anim = ui_anim_create(zoomIn ? ANIM_TYPE_ZOOM_IN : ANIM_TYPE_ZOOM_OUT, durationMs);
    return anim;
}

UIAnimation* ui_anim_create_pulse(uint32_t durationMs)
{
    UIAnimation* anim = ui_anim_create(ANIM_TYPE_PULSE, durationMs);
    if (anim)
    {
        anim->scaleX.from = 1.0f;
        anim->scaleX.to = 1.2f;
        anim->scaleY.from = 1.0f;
        anim->scaleY.to = 1.2f;
    }
    return anim;
}

UIAnimation* ui_anim_create_shake(uint32_t durationMs)
{
    UIAnimation* anim = ui_anim_create(ANIM_TYPE_SHAKE, durationMs);
    if (anim)
    {
        anim->positionX.from = -10.0f;
        anim->positionX.to = 10.0f;
    }
    return anim;
}

// ============================================================
// SECTION 3 : CONFIGURATION
// ============================================================

void ui_anim_set_target(UIAnimation* anim, UIWidget* target)
{
    if (anim == NULL) return;
    anim->target = target;
    if (target)
    {
        memcpy(&anim->originalRect, &target->rect, sizeof(UIRect));
    }
}

void ui_anim_set_easing(UIAnimation* anim, EasingType easing)
{
    if (anim) anim->easing = easing;
}

void ui_anim_set_delay(UIAnimation* anim, uint32_t delayMs)
{
    if (anim) anim->delayMs = delayMs;
}

void ui_anim_set_repeat(UIAnimation* anim, uint8_t count)
{
    if (anim) anim->repeatCount = count;
}

void ui_anim_set_direction(UIAnimation* anim, AnimationDirection direction)
{
    if (anim) anim->direction = direction;
}

void ui_anim_set_opacity(UIAnimation* anim, float from, float to)
{
    if (anim)
    {
        anim->opacity.from = from;
        anim->opacity.to = to;
    }
}

void ui_anim_set_scale(UIAnimation* anim, float fromX, float toX, float fromY, float toY)
{
    if (anim)
    {
        anim->scaleX.from = fromX; anim->scaleX.to = toX;
        anim->scaleY.from = fromY; anim->scaleY.to = toY;
    }
}

void ui_anim_set_position(UIAnimation* anim, float fromX, float toX, float fromY, float toY)
{
    if (anim)
    {
        anim->positionX.from = fromX; anim->positionX.to = toX;
        anim->positionY.from = fromY; anim->positionY.to = toY;
    }
}

// ============================================================
// SECTION 4 : CONTRÔLE
// ============================================================

bool ui_anim_start(UIAnimation* anim)
{
    if (anim == NULL) return false;
    if (anim_state.activeCount >= UI_ANIMATIONS_MAX_ACTIVE) return false;
    
    anim->state = ANIM_STATE_RUNNING;
    anim->startTime = HAL_GetTick() + anim->delayMs;
    anim->progress = 0.0f;
    anim->repeatIndex = 0;
    
    // Ajouter à la liste des animations actives
    anim_state.activeAnimations[anim_state.activeCount++] = anim;
    
    ANIM_DEBUG("Animation #%lu démarrée (type=%d, durée=%lu ms)\n",
              (unsigned long)anim->id, anim->type, (unsigned long)anim->durationMs);
    
    if (anim->onStart) anim->onStart(anim);
    
    return true;
}

void ui_anim_stop(UIAnimation* anim)
{
    if (anim == NULL) return;
    
    // Terminer l'animation à sa valeur finale
    anim->progress = 1.0f;
    apply_animation_values(anim);
    anim->state = ANIM_STATE_COMPLETED;
    
    // Retirer de la liste active
    remove_animation_from_active(anim);
    
    if (anim->onComplete) anim->onComplete(anim);
}

void ui_anim_pause(UIAnimation* anim)
{
    if (anim && anim->state == ANIM_STATE_RUNNING)
        anim->state = ANIM_STATE_PAUSED;
}

void ui_anim_resume(UIAnimation* anim)
{
    if (anim && anim->state == ANIM_STATE_PAUSED)
    {
        anim->state = ANIM_STATE_RUNNING;
        // Ajuster le temps de début pour compenser la pause
        anim->startTime = HAL_GetTick() - (uint32_t)(anim->progress * anim->durationMs);
    }
}

void ui_anim_cancel(UIAnimation* anim)
{
    if (anim == NULL) return;
    
    anim->state = ANIM_STATE_CANCELLED;
    remove_animation_from_active(anim);
    
    if (anim->onCancel) anim->onCancel(anim);
}

void ui_anim_stop_all(void)
{
    for (uint8_t i = 0; i < anim_state.activeCount; i++)
    {
        UIAnimation* anim = anim_state.activeAnimations[i];
        if (anim)
        {
            anim->progress = 1.0f;
            apply_animation_values(anim);
            anim->state = ANIM_STATE_COMPLETED;
            if (anim->onComplete) anim->onComplete(anim);
        }
    }
    anim_state.activeCount = 0;
}

void ui_anim_pause_all(void)
{
    anim_state.globalPause = true;
}

void ui_anim_resume_all(void)
{
    anim_state.globalPause = false;
}

// ============================================================
// SECTION 5 : TRANSITIONS D'ÉCRAN
// ============================================================

void ui_anim_transition_push(UIScreen* from, UIScreen* to, AnimationType type, uint32_t durationMs)
{
    UIAnimation* anim = ui_anim_create(type, durationMs);
    if (anim == NULL) return;
    
    anim->onComplete = [](UIAnimation* a) {
        // L'écran de destination est maintenant actif
        ui_request_redraw();
    };
    
    // Configurer la transition selon le type
    switch (type)
    {
        case ANIM_TYPE_SLIDE_LEFT:
            // L'ancien écran glisse vers la gauche, le nouveau arrive de la droite
            break;
        case ANIM_TYPE_FADE:
            // L'ancien écran s'estompe, le nouveau apparaît
            break;
        default:
            break;
    }
    
    ui_anim_start(anim);
}

void ui_anim_transition_pop(UIScreen* from, UIScreen* to, AnimationType type, uint32_t durationMs)
{
    // Similaire à push mais en sens inverse
    ui_anim_transition_push(from, to, type, durationMs);
}

// ============================================================
// SECTION 6 : TRAITEMENT PÉRIODIQUE
// ============================================================

void ui_animations_process(void)
{
    if (!anim_state.initialized || anim_state.globalPause) return;
    
    uint32_t now = HAL_GetTick();
    
    for (uint8_t i = 0; i < anim_state.activeCount; i++)
    {
        UIAnimation* anim = anim_state.activeAnimations[i];
        if (anim == NULL || anim->state != ANIM_STATE_RUNNING) continue;
        
        // Vérifier le délai
        if (now < anim->startTime) continue;
        
        // Calculer la progression
        uint32_t elapsed = now - anim->startTime;
        float rawProgress = (float)elapsed / anim->durationMs;
        
        if (rawProgress >= 1.0f)
        {
            rawProgress = 1.0f;
            
            // Vérifier les répétitions
            anim->repeatIndex++;
            if (anim->repeatCount == 0 || anim->repeatIndex < anim->repeatCount)
            {
                // Redémarrer
                anim->startTime = now;
                anim->progress = 0.0f;
                rawProgress = 0.0f;
                
                // Inverser si mode alterné
                if (anim->direction == ANIM_DIR_ALTERNATE)
                {
                    swap_animation_values(anim);
                }
            }
            else
            {
                // Animation terminée
                anim->progress = 1.0f;
                apply_animation_values(anim);
                anim->state = ANIM_STATE_COMPLETED;
                
                if (anim->onComplete) anim->onComplete(anim);
                remove_animation_from_active(anim);
                i--;  // Ajuster l'index
                continue;
            }
        }
        
        // Appliquer l'easing
        float easedProgress = ui_anim_easing_calculate(anim->easing, rawProgress);
        anim->progress = easedProgress;
        
        // Appliquer les valeurs au widget cible
        apply_animation_values(anim);
        
        if (anim->onUpdate) anim->onUpdate(anim, easedProgress);
    }
}

// ============================================================
// SECTION 7 : FONCTIONS UTILITAIRES
// ============================================================

/**
 * @brief Calcule la valeur avec easing
 */
float ui_anim_easing_calculate(EasingType easing, float t)
{
    switch (easing)
    {
        case EASING_LINEAR:
            return t;
            
        case EASING_EASE_IN:
            return t * t;
            
        case EASING_EASE_OUT:
            return 1.0f - (1.0f - t) * (1.0f - t);
            
        case EASING_EASE_IN_OUT:
            if (t < 0.5f)
                return 2.0f * t * t;
            else
                return 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
            
        case EASING_BOUNCE:
        {
            float n1 = 7.5625f;
            float d1 = 2.75f;
            if (t < 1.0f / d1)
                return n1 * t * t;
            else if (t < 2.0f / d1)
                return n1 * (t -= 1.5f / d1) * t + 0.75f;
            else if (t < 2.5f / d1)
                return n1 * (t -= 2.25f / d1) * t + 0.9375f;
            else
                return n1 * (t -= 2.625f / d1) * t + 0.984375f;
        }
        
        case EASING_ELASTIC:
            if (t == 0.0f || t == 1.0f) return t;
            return -powf(2.0f, 10.0f * (t - 1.0f)) * sinf((t - 1.0f) * 20.0f * M_PI / 3.0f);
            
        case EASING_BACK_IN:
            return 2.70158f * t * t * t - 1.70158f * t * t;
            
        case EASING_BACK_OUT:
            return 1.0f + 2.70158f * powf(t - 1.0f, 3.0f) + 1.70158f * powf(t - 1.0f, 2.0f);
            
        default:
            return t;
    }
}

/**
 * @brief Interpole entre deux couleurs RGB565
 */
uint16_t ui_anim_interpolate_color(uint16_t from, uint16_t to, float progress)
{
    uint8_t r1 = (from >> 11) & 0x1F;
    uint8_t g1 = (from >> 5) & 0x3F;
    uint8_t b1 = from & 0x1F;
    
    uint8_t r2 = (to >> 11) & 0x1F;
    uint8_t g2 = (to >> 5) & 0x3F;
    uint8_t b2 = to & 0x1F;
    
    uint8_t r = (uint8_t)(r1 + (r2 - r1) * progress);
    uint8_t g = (uint8_t)(g1 + (g2 - g1) * progress);
    uint8_t b = (uint8_t)(b1 + (b2 - b1) * progress);
    
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
}

// ============================================================
// SECTION 8 : FONCTIONS INTERNES
// ============================================================

static void apply_animation_values(UIAnimation* anim)
{
    if (anim == NULL || anim->target == NULL) return;
    
    float t = anim->progress;
    
    // Interpoler les valeurs
    anim->opacity.current = anim->opacity.from + (anim->opacity.to - anim->opacity.from) * t;
    anim->scaleX.current = anim->scaleX.from + (anim->scaleX.to - anim->scaleX.from) * t;
    anim->scaleY.current = anim->scaleY.from + (anim->scaleY.to - anim->scaleY.from) * t;
    anim->positionX.current = anim->positionX.from + (anim->positionX.to - anim->positionX.from) * t;
    anim->positionY.current = anim->positionY.from + (anim->positionY.to - anim->positionY.from) * t;
    
    // Appliquer au widget cible
    UIRect* r = &anim->target->rect;
    
    r->x = anim->originalRect.x + (int16_t)anim->positionX.current;
    r->y = anim->originalRect.y + (int16_t)anim->positionY.current;
    r->width = (uint16_t)(anim->originalRect.width * anim->scaleX.current);
    r->height = (uint16_t)(anim->originalRect.height * anim->scaleY.current);
    
    // Réduire l'opacité si < 1.0 (simulé en assombrissant)
    if (anim->opacity.current < 1.0f)
    {
        // Note: l'opacité réelle n'est pas supportée par le matériel
        // On peut simuler en dessinant un overlay semi-transparent
    }
    
    anim->target->needsRedraw = true;
}

static void swap_animation_values(UIAnimation* anim)
{
    float temp;
    
    temp = anim->opacity.from; anim->opacity.from = anim->opacity.to; anim->opacity.to = temp;
    temp = anim->scaleX.from; anim->scaleX.from = anim->scaleX.to; anim->scaleX.to = temp;
    temp = anim->scaleY.from; anim->scaleY.from = anim->scaleY.to; anim->scaleY.to = temp;
    temp = anim->positionX.from; anim->positionX.from = anim->positionX.to; anim->positionX.to = temp;
    temp = anim->positionY.from; anim->positionY.from = anim->positionY.to; anim->positionY.to = temp;
}

static void remove_animation_from_active(UIAnimation* anim)
{
    for (uint8_t i = 0; i < anim_state.activeCount; i++)
    {
        if (anim_state.activeAnimations[i] == anim)
        {
            if (i < anim_state.activeCount - 1)
            {
                memmove(&anim_state.activeAnimations[i], &anim_state.activeAnimations[i + 1],
                        (anim_state.activeCount - i - 1) * sizeof(UIAnimation*));
            }
            anim_state.activeCount--;
            return;
        }
    }
}

// ============================================================
// SECTION 9 : DÉBOGAGE
// ============================================================

void ui_animations_print_state(void)
{
    printf("\n═══ ÉTAT ANIMATIONS ═══\n");
    printf("Actives    : %d/%d\n", anim_state.activeCount, UI_ANIMATIONS_MAX_ACTIVE);
    printf("Pause      : %s\n", anim_state.globalPause ? "Oui" : "Non");
    printf("══════════════════════\n\n");
}

void ui_animations_print_active(void)
{
    printf("\n═══ ANIMATIONS ACTIVES (%d) ═══\n", anim_state.activeCount);
    
    for (uint8_t i = 0; i < anim_state.activeCount; i++)
    {
        UIAnimation* anim = anim_state.activeAnimations[i];
        if (anim)
        {
            printf("[%d] #%lu type=%d progress=%.2f duration=%lu ms\n",
                   i, (unsigned long)anim->id, anim->type,
                   anim->progress, (unsigned long)anim->durationMs);
        }
    }
    printf("══════════════════════════════\n\n");
}

bool ui_animations_self_test(void)
{
    ANIM_DEBUG("Auto-test...\n");
    
    if (!anim_state.initialized)
    {
        ANIM_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : créer et détruire une animation
    UIAnimation* anim = ui_anim_create_fade(true, 100);
    if (anim == NULL)
    {
        ANIM_DEBUG("Échec : création animation\n");
        return false;
    }
    
    // Test easing
    float eased = ui_anim_easing_calculate(EASING_EASE_OUT, 0.5f);
    if (eased < 0.0f || eased > 1.0f)
    {
        ANIM_DEBUG("Échec : easing invalide\n");
        free(anim);
        return false;
    }
    
    free(anim);
    
    ANIM_DEBUG("Auto-test OK\n");
    return true;
}