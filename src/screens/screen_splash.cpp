/**
 * @file    screen_splash.cpp
 * @brief   Implémentation de l'écran de démarrage (splash screen)
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente l'écran de démarrage avec :
 * - Logo animé (fade in)
 * - Barre de progression fluide
 * - Messages d'étape
 * - Gestion des erreurs
 * 
 * ANIMATIONS :
 * 
 * 1. FONDU ENTRANT (fade in) :
 *    - Tous les éléments apparaissent progressivement
 *    - Utilise DMA2D blending (alpha progressif)
 *    - Durée : SPLASH_FADE_IN_MS (500ms)
 * 
 * 2. POINTS DE CHARGEMENT :
 *    - Les points "..." après le message clignotent
 *    - Cycle : . → .. → ... → . (toutes les 400ms)
 * 
 * 3. BARRE DE PROGRESSION :
 *    - Remplissage fluide via DMA2D FillRect
 *    - La barre "poursuit" la cible par incréments
 *    - Vitesse adaptative : plus rapide si loin de la cible
 * 
 * 4. FONDU SORTANT (fade out) :
 *    - Tous les éléments disparaissent progressivement
 *    - Durée : SPLASH_FADE_OUT_MS (300ms)
 *    - Puis callback on_finished
 * 
 * TRANSITIONS D'ÉTAT :
 * 
 *   FADE_IN ──(500ms)──→ LOADING ──(100%)──→ COMPLETE
 *                            │                    │
 *                            │ (erreur)           │ (durée min écoulée)
 *                            ▼                    ▼
 *                          ERROR              FADE_OUT ──(300ms)──→ FINISHED
 *                                                 │
 *                                                 ▼
 *                                          on_finished()
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "screen_splash.h"

/* UI */
#include "../ui/ui_core.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_widgets.h"
#include "../ui/ui_label.h"
#include "../ui/ui_progress_bar.h"
#include "../ui/ui_icons.h"
#include "../ui/ui_animations.h"
#include "../ui/ui_draw_primitives.h"
#include "../ui/ui_fonts.h"

/* Drivers */
#include "../drivers/display/display_manager.h"
#include "../drivers/display/display_buffer.h"
#include "../drivers/display/dma2d_driver.h"

/* Utilitaires */
#include "../utils/string_utils.h"
#include "../utils/timer_utils.h"
#include "../utils/debug_utils.h"
#include "../utils/math_utils.h"

/* HAL */
#include "stm32f4xx_hal.h"

/* Version */
#include "../version.h"

/* Standard */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs de debug */
#define TAG                                 "ScreenSplash"

/** Dimensions de l'écran */
#define SCREEN_WIDTH                        320
#define SCREEN_HEIGHT                       480
#define CENTER_X                            (SCREEN_WIDTH / 2)
#define CENTER_Y                            (SCREEN_HEIGHT / 2)

/** Positions des éléments */
#define LOGO_SIZE                           80
#define LOGO_X                              (CENTER_X - LOGO_SIZE / 2)
#define LOGO_Y                              100

#define TITLE_Y                             200
#define VERSION_Y                           228
#define STATUS_Y                            270
#define PROGRESS_BAR_Y                      310
#define PROGRESS_BAR_WIDTH                  240
#define PROGRESS_BAR_HEIGHT                 18
#define PROGRESS_BAR_X                      (CENTER_X - PROGRESS_BAR_WIDTH / 2)
#define PROGRESS_TEXT_Y                     335
#define INFO_Y                              380
#define COPYRIGHT_Y                         450

/** Pas d'incrémentation de la barre de progression */
#define PROGRESS_STEP_NORMAL                1
#define PROGRESS_STEP_FAST                  3
#define PROGRESS_STEP_SLOW                  1

/** Seuil pour l'incrémentation rapide (écart > 10%) */
#define PROGRESS_FAST_THRESHOLD             10

/* ======================================================================== */
/*                VARIABLES STATIQUES                                       */
/* ======================================================================== */

/** Couleurs spécifiques au splash screen */
#define SPLASH_BG_COLOR                     0x0000  /**< Fond noir           */
#define SPLASH_TITLE_COLOR                  0xFFFF  /**< Blanc              */
#define SPLASH_VERSION_COLOR                0x8410  /**< Gris clair         */
#define SPLASH_STATUS_COLOR                 0xC618  /**< Gris moyen         */
#define SPLASH_INFO_COLOR                   0x630C  /**< Gris foncé         */
#define SPLASH_COPYRIGHT_COLOR              0x4208  /**< Gris très foncé    */
#define SPLASH_PROGRESS_BG                  0x18E3  /**< Fond barre         */
#define SPLASH_PROGRESS_FILL                0x07E0  /**< Vert remplissage   */
#define SPLASH_PROGRESS_BORDER              0x8410  /**< Bordure barre      */
#define SPLASH_ERROR_COLOR                  0xF800  /**< Rouge erreur       */

/** Messages d'information technique */
#define SPLASH_INFO_TEXT                    "STM32F429 @ 180 MHz | SX1278 LoRa"
#define SPLASH_COPYRIGHT_TEXT               "© 2026 - Licence MIT"

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

/* --- États --- */
static void handle_fade_in(ScreenSplash_t* screen);
static void handle_loading(ScreenSplash_t* screen);
static void handle_complete(ScreenSplash_t* screen);
static void handle_fade_out(ScreenSplash_t* screen);
static void handle_error(ScreenSplash_t* screen);

/* --- UI --- */
static void create_ui_elements(ScreenSplash_t* screen);
static void draw_all_static_elements(ScreenSplash_t* screen);
static void update_progress_bar(ScreenSplash_t* screen);
static void update_status_message(ScreenSplash_t* screen);
static void animate_loading_dots(ScreenSplash_t* screen);

/* --- Animation --- */
static void update_fade_alpha(ScreenSplash_t* screen, uint8_t alpha);
static void smooth_progress_update(ScreenSplash_t* screen);

/* --- Timers --- */
static void fade_timer_callback(TimerHandle_t timer);
static void dots_timer_callback(TimerHandle_t timer);
static void min_duration_timer_callback(TimerHandle_t timer);
static void progress_timer_callback(TimerHandle_t timer);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise l'écran de démarrage
 */
bool ScreenSplash_Init(ScreenSplash_t* screen)
{
    if (!screen) {
        DEBUG_ERROR(TAG, "Paramètre invalide");
        return false;
    }

    DEBUG_INFO(TAG, "Initialisation de l'écran de démarrage...");

    /* Mise à zéro */
    memset(screen, 0, sizeof(ScreenSplash_t));

    /* Classe de base */
    ScreenBase_Init(&screen->base, SCREEN_ID_SPLASH, "Splash");

    /* État initial */
    screen->state = SPLASH_STATE_FADE_IN;
    screen->current_step = 0;
    screen->total_steps = 0;
    screen->target_progress = 0;
    screen->current_progress = 0;
    screen->progress_increment = PROGRESS_STEP_NORMAL;
    screen->loading_dots = 0;
    screen->min_duration_elapsed = false;
    screen->init_complete = false;
    screen->start_timestamp_ms = 0;

    /* Logo */
    screen->logo_icon = ICON_LOGO_LARGE;
    screen->logo_x = LOGO_X;
    screen->logo_y = LOGO_Y;

    /* Créer les éléments UI */
    create_ui_elements(screen);

    /* Timers */
    screen->fade_timer = Timer_Create("SplashFade",
                                      20,     /* 50 FPS pour animation fluide */
                                      true,   /* auto-reload */
                                      fade_timer_callback,
                                      screen);

    screen->dots_timer = Timer_Create("SplashDots",
                                      SPLASH_DOTS_ANIM_MS,
                                      true,   /* auto-reload */
                                      dots_timer_callback,
                                      screen);

    screen->min_duration_timer = Timer_Create("SplashMinDuration",
                                              SPLASH_MIN_DURATION_MS,
                                              false,  /* one-shot */
                                              min_duration_timer_callback,
                                              screen);

    screen->progress_timer = Timer_Create("SplashProgress",
                                          30,     /* ~33 FPS */
                                          true,   /* auto-reload */
                                          progress_timer_callback,
                                          screen);

    DEBUG_INFO(TAG, "Initialisation terminée");

    return true;
}

/**
 * @brief Affiche l'écran de démarrage
 */
void ScreenSplash_Show(ScreenSplash_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Affichage du splash screen");

    /* Enregistrer le timestamp de début */
    screen->start_timestamp_ms = HAL_GetTick();

    /* Réinitialiser l'état */
    screen->state = SPLASH_STATE_FADE_IN;
    screen->current_progress = 0;
    screen->target_progress = 0;
    screen->current_step = 0;
    screen->min_duration_elapsed = false;
    screen->init_complete = false;
    screen->error_message[0] = '\0';

    /* Fond noir */
    Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SPLASH_BG_COLOR);

    /* Dessiner les éléments statiques */
    draw_all_static_elements(screen);

    /* Démarrer le fondu entrant */
    Timer_Start(screen->fade_timer);

    /* Démarrer la durée minimale */
    Timer_Start(screen->min_duration_timer);

    /* Démarrer l'animation des points */
    Timer_Start(screen->dots_timer);

    /* Démarrer la progression fluide */
    Timer_Start(screen->progress_timer);

    screen->base.is_visible = true;

    DEBUG_INFO(TAG, "Splash screen affiché");
}

/**
 * @brief Masque l'écran de démarrage
 */
void ScreenSplash_Hide(ScreenSplash_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Masquage du splash screen");

    Timer_Stop(screen->fade_timer);
    Timer_Stop(screen->dots_timer);
    Timer_Stop(screen->min_duration_timer);
    Timer_Stop(screen->progress_timer);

    screen->base.is_visible = false;
}

/**
 * @brief Mise à jour périodique
 */
void ScreenSplash_Update(ScreenSplash_t* screen)
{
    if (!screen || !screen->base.is_visible) return;

    switch (screen->state) {
        case SPLASH_STATE_FADE_IN:
            /* Géré par le timer */
            break;

        case SPLASH_STATE_LOADING:
            /* Progression fluide gérée par le timer */
            smooth_progress_update(screen);
            update_progress_bar(screen);
            break;

        case SPLASH_STATE_COMPLETE:
            /* Vérifier si on peut passer au fade out */
            if (screen->min_duration_elapsed) {
                handle_fade_out(screen);
            }
            break;

        case SPLASH_STATE_ERROR:
            /* L'écran reste figé sur l'erreur */
            break;

        case SPLASH_STATE_FADE_OUT:
            /* Géré par le timer */
            break;

        case SPLASH_STATE_FINISHED:
            /* Ne rien faire */
            break;
    }
}

/**
 * @brief Passe à l'étape suivante
 */
void ScreenSplash_SetStep(ScreenSplash_t* screen,
                          const char* message,
                          uint8_t progress)
{
    if (!screen) return;

    /* Limiter le progrès */
    if (progress > 100) progress = 100;
    if (progress < screen->target_progress) progress = screen->target_progress;

    DEBUG_INFO(TAG, "Étape %d: %s (%d%%)", 
               screen->current_step, message, progress);

    /* Enregistrer l'étape si tableau non plein */
    if (screen->total_steps < SPLASH_MAX_STEPS) {
        SplashStep_t* step = &screen->steps[screen->total_steps];
        step->message = message;
        step->progress_percent = progress;
        step->is_completed = false;
        step->is_error = false;
        screen->total_steps++;
    }

    /* Mettre à jour l'étape courante */
    screen->current_step = screen->total_steps;
    screen->target_progress = progress;

    /* Mettre à jour le message */
    update_status_message(screen);

    /* Déterminer la vitesse d'incrémentation */
    if (progress == 100) {
        screen->progress_increment = PROGRESS_STEP_FAST;
    } else if (progress - screen->current_progress > PROGRESS_FAST_THRESHOLD) {
        screen->progress_increment = PROGRESS_STEP_FAST;
    } else {
        screen->progress_increment = PROGRESS_STEP_NORMAL;
    }

    /* Si 100%, marquer comme complet */
    if (progress >= 100) {
        screen->current_progress = 100;
        screen->init_complete = true;
        update_progress_bar(screen);

        /* Passer à l'état complet */
        if (screen->state == SPLASH_STATE_LOADING) {
            screen->state = SPLASH_STATE_COMPLETE;
        }
    }
}

/**
 * @brief Définit le pourcentage directement
 */
void ScreenSplash_SetProgress(ScreenSplash_t* screen, uint8_t progress)
{
    if (!screen) return;

    screen->target_progress = CLAMP(progress, 0, 100);
    screen->progress_increment = (progress >= 100) ? 
                                  PROGRESS_STEP_FAST : PROGRESS_STEP_NORMAL;
}

/**
 * @brief Signale que tout est prêt
 */
void ScreenSplash_Complete(ScreenSplash_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Initialisation terminée !");

    screen->target_progress = 100;
    screen->init_complete = true;
    screen->state = SPLASH_STATE_COMPLETE;

    /* Mettre à jour le message */
    UILabel_SetText(screen->status_label, "Systeme pret !");
    UILabel_SetColor(screen->status_label, SPLASH_PROGRESS_FILL);

    if (screen->base.is_visible) {
        UILabel_Draw(screen->status_label);
    }

    /* Si la durée minimale est déjà écoulée, lancer le fade out */
    if (screen->min_duration_elapsed) {
        handle_fade_out(screen);
    }
}

/**
 * @brief Signale une erreur
 */
void ScreenSplash_SetError(ScreenSplash_t* screen, const char* error_msg)
{
    if (!screen || !error_msg) return;

    DEBUG_ERROR(TAG, "ERREUR: %s", error_msg);

    screen->state = SPLASH_STATE_ERROR;
    strncpy(screen->error_message, error_msg, sizeof(screen->error_message) - 1);
    screen->error_message[sizeof(screen->error_message) - 1] = '\0';

    /* Mettre à jour l'affichage */
    if (screen->base.is_visible) {
        /* Fond rouge foncé */
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x3000);

        /* Message d'erreur */
        UILabel_SetText(screen->status_label, error_msg);
        UILabel_SetColor(screen->status_label, SPLASH_ERROR_COLOR);

        /* Barre de progression en rouge */
        UIProgressBar_SetColor(screen->progress_bar, SPLASH_ERROR_COLOR);

        /* Redessiner */
        draw_all_static_elements(screen);
    }

    /* Arrêter les animations */
    Timer_Stop(screen->dots_timer);
    Timer_Stop(screen->progress_timer);

    /* Notifier l'application */
    if (screen->on_error) {
        screen->on_error(error_msg);
    }
}

/**
 * @brief Vérifie si le splash est visible
 */
bool ScreenSplash_IsVisible(ScreenSplash_t* screen)
{
    if (!screen) return false;
    return screen->base.is_visible;
}

/**
 * @brief Vérifie si le splash est terminé
 */
bool ScreenSplash_IsFinished(ScreenSplash_t* screen)
{
    if (!screen) return true;
    return screen->state == SPLASH_STATE_FINISHED;
}

/**
 * @brief Libère les ressources
 */
void ScreenSplash_Deinit(ScreenSplash_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Libération des ressources splash");

    Timer_Delete(screen->fade_timer);
    Timer_Delete(screen->dots_timer);
    Timer_Delete(screen->min_duration_timer);
    Timer_Delete(screen->progress_timer);

    UILabel_Destroy(screen->title_label);
    UILabel_Destroy(screen->version_label);
    UILabel_Destroy(screen->status_label);
    UILabel_Destroy(screen->copyright_label);
    UILabel_Destroy(screen->info_label);
    UIProgressBar_Destroy(screen->progress_bar);

    memset(screen, 0, sizeof(ScreenSplash_t));
}

/* ======================================================================== */
/*              GESTION DES ÉTATS                                           */
/* ======================================================================== */

static void handle_fade_in(ScreenSplash_t* screen)
{
    if (!screen) return;

    /* Calculer l'alpha basé sur le temps écoulé */
    uint32_t elapsed = HAL_GetTick() - screen->start_timestamp_ms;
    uint8_t alpha;

    if (elapsed >= SPLASH_FADE_IN_MS) {
        /* Fondu terminé */
        alpha = 255;
        screen->state = SPLASH_STATE_LOADING;
        Timer_Stop(screen->fade_timer);

        DEBUG_VERBOSE(TAG, "Fondu entrant terminé");
    } else {
        /* Progression linéaire */
        alpha = (uint8_t)((elapsed * 255) / SPLASH_FADE_IN_MS);
    }

    /* Appliquer l'alpha */
    update_fade_alpha(screen, alpha);
}

static void handle_loading(ScreenSplash_t* screen)
{
    if (!screen) return;

    /* La progression est gérée par le timer */
}

static void handle_complete(ScreenSplash_t* screen)
{
    if (!screen) return;

    /* La transition vers fade out est gérée dans Update() */
}

static void handle_fade_out(ScreenSplash_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Démarrage fondu sortant");

    screen->state = SPLASH_STATE_FADE_OUT;
    screen->start_timestamp_ms = HAL_GetTick();

    /* Redémarrer le timer fade pour le fondu sortant */
    Timer_Start(screen->fade_timer);
}

static void handle_fade_out_update(ScreenSplash_t* screen)
{
    if (!screen) return;

    uint32_t elapsed = HAL_GetTick() - screen->start_timestamp_ms;
    uint8_t alpha;

    if (elapsed >= SPLASH_FADE_OUT_MS) {
        /* Fondu terminé - écran noir complet */
        alpha = 0;
        screen->state = SPLASH_STATE_FINISHED;
        Timer_Stop(screen->fade_timer);
        Timer_Stop(screen->dots_timer);
        Timer_Stop(screen->progress_timer);

        /* Fond noir */
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x0000);

        DEBUG_INFO(TAG, "Splash screen terminé");

        /* Notifier l'application */
        if (screen->on_finished) {
            screen->on_finished();
        }
    } else {
        /* Alpha décroissant */
        alpha = 255 - (uint8_t)((elapsed * 255) / SPLASH_FADE_OUT_MS);
    }

    /* Appliquer l'alpha (fondu vers noir) */
    update_fade_alpha(screen, alpha);
}

static void handle_error(ScreenSplash_t* screen)
{
    if (!screen) return;

    /* L'erreur est déjà affichée, rien de plus à faire */
    /* L'écran reste figé jusqu'au redémarrage */
}

/* ======================================================================== */
/*              CRÉATION DE L'INTERFACE                                     */
/* ======================================================================== */

static void create_ui_elements(ScreenSplash_t* screen)
{
    if (!screen) return;

    /* 1. Titre "LoRa Phone" */
    screen->title_label = UILabel_Create();
    UILabel_SetText(screen->title_label, "LoRa Phone");
    UILabel_SetFont(screen->title_label, &font_huge_bold);
    UILabel_SetColor(screen->title_label, SPLASH_TITLE_COLOR);
    UILabel_SetPosition(screen->title_label, CENTER_X - 70, TITLE_Y);

    /* 2. Version */
    screen->version_label = UILabel_Create();
    UILabel_SetText(screen->version_label, "v" FIRMWARE_VERSION);
    UILabel_SetFont(screen->version_label, &font_medium);
    UILabel_SetColor(screen->version_label, SPLASH_VERSION_COLOR);
    UILabel_SetPosition(screen->version_label, CENTER_X - 20, VERSION_Y);

    /* 3. Message de statut */
    screen->status_label = UILabel_Create();
    UILabel_SetText(screen->status_label, "Demarrage...");
    UILabel_SetFont(screen->status_label, &font_small);
    UILabel_SetColor(screen->status_label, SPLASH_STATUS_COLOR);
    UILabel_SetPosition(screen->status_label, CENTER_X - 80, STATUS_Y);

    /* 4. Barre de progression */
    screen->progress_bar = UIProgressBar_Create();
    UIProgressBar_SetPosition(screen->progress_bar, PROGRESS_BAR_X, PROGRESS_BAR_Y);
    UIProgressBar_SetSize(screen->progress_bar, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT);
    UIProgressBar_SetProgress(screen->progress_bar, 0);
    UIProgressBar_SetFillColor(screen->progress_bar, SPLASH_PROGRESS_FILL);
    UIProgressBar_SetBgColor(screen->progress_bar, SPLASH_PROGRESS_BG);
    UIProgressBar_SetBorderColor(screen->progress_bar, SPLASH_PROGRESS_BORDER);
    UIProgressBar_SetBorderWidth(screen->progress_bar, 1);
    UIProgressBar_SetCornerRadius(screen->progress_bar, 3);
    UIProgressBar_SetShowPercentage(screen->progress_bar, false);

    /* 5. Informations techniques */
    screen->info_label = UILabel_Create();
    UILabel_SetText(screen->info_label, SPLASH_INFO_TEXT);
    UILabel_SetFont(screen->info_label, &font_tiny);
    UILabel_SetColor(screen->info_label, SPLASH_INFO_COLOR);
    UILabel_SetPosition(screen->info_label, CENTER_X - 90, INFO_Y);

    /* 6. Copyright */
    screen->copyright_label = UILabel_Create();
    UILabel_SetText(screen->copyright_label, SPLASH_COPYRIGHT_TEXT);
    UILabel_SetFont(screen->copyright_label, &font_tiny);
    UILabel_SetColor(screen->copyright_label, SPLASH_COPYRIGHT_COLOR);
    UILabel_SetPosition(screen->copyright_label, CENTER_X - 60, COPYRIGHT_Y);
}

/* ======================================================================== */
/*              RENDU                                                       */
/* ======================================================================== */

static void draw_all_static_elements(ScreenSplash_t* screen)
{
    if (!screen) return;

    /* Fond */
    Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SPLASH_BG_COLOR);

    /* Logo */
    UIIcons_Draw(screen->logo_icon, screen->logo_x, screen->logo_y, SPLASH_TITLE_COLOR);

    /* Titre */
    UILabel_Draw(screen->title_label);

    /* Version */
    UILabel_Draw(screen->version_label);

    /* Message statut */
    UILabel_Draw(screen->status_label);

    /* Barre de progression */
    UIProgressBar_Draw(screen->progress_bar);

    /* Infos techniques */
    UILabel_Draw(screen->info_label);

    /* Copyright */
    UILabel_Draw(screen->copyright_label);
}

static void update_progress_bar(ScreenSplash_t* screen)
{
    if (!screen || !screen->progress_bar) return;

    UIProgressBar_SetProgress(screen->progress_bar, screen->current_progress);

    if (screen->base.is_visible) {
        /* Redessiner uniquement la zone de la barre */
        Display_FillRect(PROGRESS_BAR_X - 2, PROGRESS_BAR_Y - 2,
                         PROGRESS_BAR_WIDTH + 4, PROGRESS_BAR_HEIGHT + 4,
                         SPLASH_BG_COLOR);
        UIProgressBar_Draw(screen->progress_bar);

        /* Afficher le pourcentage sous la barre */
        char pct_text[8];
        snprintf(pct_text, sizeof(pct_text), "%d%%", screen->current_progress);
        Display_DrawText(CENTER_X - 12, PROGRESS_TEXT_Y,
                         pct_text, &font_small,
                         SPLASH_VERSION_COLOR, SPLASH_BG_COLOR);
    }
}

static void update_status_message(ScreenSplash_t* screen)
{
    if (!screen || !screen->status_label) return;

    /* Construire le message avec les points */
    char message[128];
    const char* current_msg = "Initialisation...";

    /* Récupérer le message de l'étape courante */
    if (screen->total_steps > 0 && screen->current_step > 0) {
        current_msg = screen->steps[screen->current_step - 1].message;
    }

    /* Ajouter les points de chargement */
    snprintf(message, sizeof(message), "%s", current_msg);

    /* Ajouter les points animés */
    size_t len = strlen(message);
    for (int i = 0; i < screen->loading_dots; i++) {
        if (len < sizeof(message) - 1) {
            message[len++] = '.';
            message[len] = '\0';
        }
    }

    UILabel_SetText(screen->status_label, message);

    if (screen->base.is_visible) {
        /* Effacer et redessiner la zone du texte */
        Display_FillRect(0, STATUS_Y, SCREEN_WIDTH, 20, SPLASH_BG_COLOR);
        UILabel_Draw(screen->status_label);
    }
}

static void animate_loading_dots(ScreenSplash_t* screen)
{
    if (!screen) return;

    /* Cycle 0 → 1 → 2 → 3 → 0 */
    screen->loading_dots = (screen->loading_dots + 1) % (SPLASH_LOADING_DOTS + 1);

    if (screen->state == SPLASH_STATE_LOADING || 
        screen->state == SPLASH_STATE_FADE_IN) {
        update_status_message(screen);
    }
}

/* ======================================================================== */
/*              ANIMATIONS                                                  */
/* ======================================================================== */

static void update_fade_alpha(ScreenSplash_t* screen, uint8_t alpha)
{
    if (!screen || !screen->base.is_visible) return;

    /* Utiliser DMA2D pour appliquer un blend global */
    /* La couche 0 est noire, la couche 1 contient le splash */
    /* Alpha 0 = couche 0 visible (noir), Alpha 255 = couche 1 visible */

    /* Simplification : redessiner avec des couleurs ajustées */
    uint16_t title_color = DMA2D_BlendColor(SPLASH_TITLE_COLOR, SPLASH_BG_COLOR, alpha);
    uint16_t version_color = DMA2D_BlendColor(SPLASH_VERSION_COLOR, SPLASH_BG_COLOR, alpha);
    uint16_t status_color = DMA2D_BlendColor(SPLASH_STATUS_COLOR, SPLASH_BG_COLOR, alpha);
    uint16_t info_color = DMA2D_BlendColor(SPLASH_INFO_COLOR, SPLASH_BG_COLOR, alpha);
    uint16_t copyright_color = DMA2D_BlendColor(SPLASH_COPYRIGHT_COLOR, SPLASH_BG_COLOR, alpha);
    uint16_t logo_color = DMA2D_BlendColor(SPLASH_TITLE_COLOR, SPLASH_BG_COLOR, alpha);
    uint16_t progress_fill = DMA2D_BlendColor(SPLASH_PROGRESS_FILL, SPLASH_BG_COLOR, alpha);
    uint16_t progress_bg = DMA2D_BlendColor(SPLASH_PROGRESS_BG, SPLASH_BG_COLOR, alpha);
    uint16_t progress_border = DMA2D_BlendColor(SPLASH_PROGRESS_BORDER, SPLASH_BG_COLOR, alpha);

    /* Redessiner avec les couleurs ajustées */
    Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SPLASH_BG_COLOR);

    /* Logo */
    UIIcons_Draw(screen->logo_icon, screen->logo_x, screen->logo_y, logo_color);

    /* Titre */
    UILabel_SetColor(screen->title_label, title_color);
    UILabel_Draw(screen->title_label);

    /* Version */
    UILabel_SetColor(screen->version_label, version_color);
    UILabel_Draw(screen->version_label);

    /* Statut */
    UILabel_SetColor(screen->status_label, status_color);
    UILabel_Draw(screen->status_label);

    /* Barre */
    UIProgressBar_SetFillColor(screen->progress_bar, progress_fill);
    UIProgressBar_SetBgColor(screen->progress_bar, progress_bg);
    UIProgressBar_SetBorderColor(screen->progress_bar, progress_border);
    UIProgressBar_Draw(screen->progress_bar);

    /* Pourcentage */
    char pct_text[8];
    snprintf(pct_text, sizeof(pct_text), "%d%%", screen->current_progress);
    Display_DrawText(CENTER_X - 12, PROGRESS_TEXT_Y,
                     pct_text, &font_small,
                     version_color, SPLASH_BG_COLOR);

    /* Infos */
    UILabel_SetColor(screen->info_label, info_color);
    UILabel_Draw(screen->info_label);

    /* Copyright */
    UILabel_SetColor(screen->copyright_label, copyright_color);
    UILabel_Draw(screen->copyright_label);
}

static void smooth_progress_update(ScreenSplash_t* screen)
{
    if (!screen) return;

    if (screen->current_progress < screen->target_progress) {
        /* Incrémenter vers la cible */
        uint8_t diff = screen->target_progress - screen->current_progress;

        if (diff > PROGRESS_FAST_THRESHOLD) {
            screen->progress_increment = PROGRESS_STEP_FAST;
        } else {
            screen->progress_increment = PROGRESS_STEP_SLOW;
        }

        screen->current_progress += screen->progress_increment;

        if (screen->current_progress > screen->target_progress) {
            screen->current_progress = screen->target_progress;
        }

        /* Mettre à jour la barre */
        update_progress_bar(screen);
    }

    /* Si 100% atteint, marquer comme complet */
    if (screen->current_progress >= 100 && screen->init_complete) {
        if (screen->state == SPLASH_STATE_LOADING) {
            screen->state = SPLASH_STATE_COMPLETE;
        }
    }
}

/* ======================================================================== */
/*              TIMERS                                                      */
/* ======================================================================== */

static void fade_timer_callback(TimerHandle_t timer)
{
    ScreenSplash_t* screen = (ScreenSplash_t*)Timer_GetContext(timer);
    if (!screen || !screen->base.is_visible) return;

    if (screen->state == SPLASH_STATE_FADE_IN) {
        handle_fade_in(screen);
    } else if (screen->state == SPLASH_STATE_FADE_OUT) {
        handle_fade_out_update(screen);
    }
}

static void dots_timer_callback(TimerHandle_t timer)
{
    ScreenSplash_t* screen = (ScreenSplash_t*)Timer_GetContext(timer);
    if (!screen) return;

    animate_loading_dots(screen);
}

static void min_duration_timer_callback(TimerHandle_t timer)
{
    ScreenSplash_t* screen = (ScreenSplash_t*)Timer_GetContext(timer);
    if (!screen) return;

    DEBUG_VERBOSE(TAG, "Durée minimale écoulée");

    screen->min_duration_elapsed = true;

    /* Si l'init est déjà terminée, lancer le fade out */
    if (screen->init_complete && 
        screen->state == SPLASH_STATE_COMPLETE) {
        handle_fade_out(screen);
    }
}

static void progress_timer_callback(TimerHandle_t timer)
{
    ScreenSplash_t* screen = (ScreenSplash_t*)Timer_GetContext(timer);
    if (!screen || !screen->base.is_visible) return;

    if (screen->state == SPLASH_STATE_LOADING) {
        smooth_progress_update(screen);
    }
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */