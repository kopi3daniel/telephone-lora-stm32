/**
 * @file    screen_splash.h
 * @brief   Écran de démarrage (splash screen) - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Cet écran s'affiche brièvement au démarrage du téléphone.
 * Il présente le logo, le nom du projet et une barre de progression
 * pendant l'initialisation du système.
 * 
 * ORGANISATION DE L'ÉCRAN :
 * ┌──────────────────────────────────────────────────────────┐
 * │                                                          │
 * │                                                          │
 * │                     ╔══════════════╗                     │
 * │                     ║   📡 📱 🔒   ║                     │ ← Logo/icône
 * │                     ╚══════════════╝                     │    (bitmap 80x80)
 * │                                                          │
 * │                   LoRa Phone v1.0.0                      │ ← Nom + version
 * │                                                          │
 * │              ┌──────────────────────────┐                │
 * │              │  ████████████░░░░░░░░░░  │  65%           │ ← Barre progression
 * │              └──────────────────────────┘                │
 * │                                                          │
 * │              Initialisation du module LoRa...            │ ← Message statut
 * │                                                          │
 * │                                                          │
 * │         ⚡ STM32F429 @ 180 MHz  |  SX1278 LoRa           │ ← Infos techniques
 * │                                                          │
 * │                                                          │
 * │              © 2026 Votre Nom / Licence MIT              │ ← Copyright
 * │                                                          │
 * └──────────────────────────────────────────────────────────┘
 * 
 * ÉTAPES D'INITIALISATION :
 * 
 * La barre de progression avance à chaque étape complétée :
 * 
 *   [████░░░░░░░░░░░░░░░░]   5%  ⏳ Démarrage du processeur...
 *   [████████░░░░░░░░░░░░]  15%  ⏳ Initialisation HAL...
 *   [████████████░░░░░░░░]  25%  ⏳ Configuration horloge 180 MHz...
 *   [████████████████░░░░]  35%  ⏳ Initialisation SDRAM...
 *   [██████████████████░░]  45%  ⏳ Initialisation LTDC / Écran...
 *   [████████████████████]  55%  ⏳ Initialisation DMA2D...
 *   [████████████████████]  65%  ⏳ Initialisation SPI / LoRa...
 *   [████████████████████]  75%  ⏳ Configuration radio SX1278...
 *   [████████████████████]  85%  ⏳ Initialisation Audio (ADC/DAC)...
 *   [████████████████████]  95%  ⏳ Chargement des paramètres...
 *   [████████████████████] 100%  ✅ Système prêt !
 * 
 * ANIMATIONS :
 *   - Logo : Fondu entrant (fade in) sur 500ms
 *   - Texte : Apparition séquentielle (slide up)
 *   - Barre progression : Remplissage fluide avec DMA2D
 *   - Points de chargement : Animation "..." clignotante
 *   - Transition sortie : Fondu sortant (fade out) sur 300ms
 * 
 * PERSONNALISATION :
 *   - Logo : Remplacer l'image bitmap dans ui_icons.h
 *   - Couleurs : Définies dans le thème (THEME_SPLASH_BG, etc.)
 *   - Durée minimale : SPLASH_MIN_DURATION_MS (évite un flash)
 *   - Version : Définie dans version.h (FIRMWARE_VERSION)
 */

#ifndef SCREEN_SPLASH_H
#define SCREEN_SPLASH_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "screen_base.h"                        // Classe de base écran
#include "../ui/ui_core.h"                      // Noyau UI
#include "../ui/ui_theme.h"                     // Thème couleurs
#include "../ui/ui_widgets.h"                   // Widgets communs
#include "../ui/ui_label.h"                     // Texte
#include "../ui/ui_progress_bar.h"              // Barre de progression
#include "../ui/ui_icons.h"                     // Logo / icônes
#include "../ui/ui_animations.h"                // Animations (fade in/out)
#include "../ui/ui_draw_primitives.h"           // Primitives dessin
#include "../drivers/display/display_manager.h"  // Gestionnaire affichage
#include "../drivers/display/dma2d_driver.h"    // Accélérateur DMA2D
#include "../utils/timer_utils.h"               // Timers
#include "../version.h"                         // FIRMWARE_VERSION

/* ======================================================================== */
/*                     CONSTANTES SYMBOLIQUES                                */
/* ======================================================================== */

/**
 * @brief Durée minimale d'affichage du splash screen (ms)
 * 
 * Évite que l'écran ne disparaisse trop vite si l'initialisation
 * est très rapide (ce qui provoquerait un flash désagréable).
 */
#define SPLASH_MIN_DURATION_MS              1500

/**
 * @brief Durée de l'animation de fondu entrant (ms)
 */
#define SPLASH_FADE_IN_MS                   500

/**
 * @brief Durée de l'animation de fondu sortant (ms)
 */
#define SPLASH_FADE_OUT_MS                  300

/**
 * @brief Nombre maximal d'étapes d'initialisation
 * 
 * Chaque étape correspond à un pourcentage d'avancement.
 * Le tableau steps[] définit le message et le pourcentage.
 */
#define SPLASH_MAX_STEPS                    12

/**
 * @brief Nombre de points dans l'animation "loading..."
 */
#define SPLASH_LOADING_DOTS                 3

/**
 * @brief Intervalle d'animation des points (ms)
 */
#define SPLASH_DOTS_ANIM_MS                 400

/* ======================================================================== */
/*                     STRUCTURES DE DONNÉES                                */
/* ======================================================================== */

/**
 * @brief Une étape d'initialisation
 * 
 * Chaque étape a un message descriptif et un pourcentage
 * cible que la barre de progression doit atteindre.
 */
typedef struct {
    const char* message;                    /**< Message affiché              */
    uint8_t     progress_percent;           /**< Pourcentage cible (0-100)    */
    bool        is_completed;               /**< Étape terminée               */
    bool        is_error;                   /**< Étape en erreur              */
} SplashStep_t;

/**
 * @brief États du splash screen
 */
typedef enum {
    SPLASH_STATE_FADE_IN,                   /**< Animation d'entrée           */
    SPLASH_STATE_LOADING,                   /**< Chargement en cours          */
    SPLASH_STATE_COMPLETE,                  /**< Initialisation terminée      */
    SPLASH_STATE_FADE_OUT,                  /**< Animation de sortie          */
    SPLASH_STATE_ERROR,                     /**< Erreur d'initialisation      */
    SPLASH_STATE_FINISHED,                  /**< Écran terminé (caché)        */
} SplashScreenState_t;

/* ======================================================================== */
/*                     STRUCTURE PRINCIPALE                                  */
/* ======================================================================== */

/**
 * @brief Structure de l'écran de démarrage
 * 
 * Taille approximative : ~2 Ko
 */
typedef struct {
    /* ---- Héritage de ScreenBase ---- */
    ScreenBase_t base;                          /**< Classe de base           */

    /* ---- État ---- */
    SplashScreenState_t state;                  /**< État actuel              */

    /* ---- Widgets UI ---- */
    UILabel_t*          title_label;            /**< "LoRa Phone"             */
    UILabel_t*          version_label;          /**< "v1.0.0"                 */
    UILabel_t*          status_label;           /**< Message étape en cours   */
    UILabel_t*          copyright_label;        /**< Copyright                */
    UILabel_t*          info_label;             /**< Infos techniques         */
    UIProgressBar_t*    progress_bar;           /**< Barre de progression     */

    /* Logo */
    UIIcon_t            logo_icon;              /**< Icône du logo            */
    int16_t             logo_x;                 /**< Position X logo          */
    int16_t             logo_y;                 /**< Position Y logo          */

    /* ---- Étapes d'initialisation ---- */
    SplashStep_t        steps[SPLASH_MAX_STEPS]; /**< Tableau des étapes      */
    uint8_t             current_step;           /**< Étape en cours           */
    uint8_t             total_steps;            /**< Nombre total d'étapes    */

    /* ---- Progression ---- */
    uint8_t             target_progress;        /**< Pourcentage cible        */
    uint8_t             current_progress;       /**< Pourcentage actuel       */
    uint8_t             progress_increment;     /**< Incrément par frame      */

    /* ---- Animation points ---- */
    uint8_t             loading_dots;           /**< Nombre de points (0-3)   */
    
    /* ---- Callback ---- */
    void (*on_finished)(void);                  /**< Appelé à la fin          */
    void (*on_error)(const char* error_msg);    /**< Appelé en cas d'erreur   */

    /* ---- Timers ---- */
    TimerHandle_t       fade_timer;             /**< Timer animation fondu    */
    TimerHandle_t       dots_timer;             /**< Timer points chargement  */
    TimerHandle_t       min_duration_timer;     /**< Timer durée minimale     */
    TimerHandle_t       progress_timer;         /**< Timer progression fluide */

    /* ---- État erreur ---- */
    char                error_message[128];     /**< Message d'erreur         */
    bool                min_duration_elapsed;   /**< Durée min écoulée        */
    bool                init_complete;          /**< Initialisation finie     */

    /* ---- Timestamp début ---- */
    uint32_t            start_timestamp_ms;     /**< Pour durée minimale      */

} ScreenSplash_t;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/**
 * @brief Initialise l'écran de démarrage
 * 
 * Prépare les widgets, définit les étapes d'initialisation,
 * mais n'affiche rien encore (attendre Show).
 * 
 * @param screen    Structure à initialiser
 * @return          true si succès
 */
bool ScreenSplash_Init(ScreenSplash_t* screen);

/**
 * @brief Affiche l'écran de démarrage
 * 
 * Lance l'animation de fondu entrant puis commence
 * à afficher les étapes de chargement.
 * 
 * @param screen    Écran à afficher
 */
void ScreenSplash_Show(ScreenSplash_t* screen);

/**
 * @brief Masque l'écran de démarrage
 * 
 * Lance l'animation de fondu sortant avant de masquer.
 * 
 * @param screen    Écran à masquer
 */
void ScreenSplash_Hide(ScreenSplash_t* screen);

/**
 * @brief Mise à jour périodique
 * 
 * Gère l'animation fluide de la barre de progression
 * et les transitions d'état.
 * 
 * @param screen    Écran à mettre à jour
 */
void ScreenSplash_Update(ScreenSplash_t* screen);

/**
 * @brief Passe à l'étape d'initialisation suivante
 * 
 * Met à jour le message, le pourcentage cible,
 * et lance l'animation de la barre de progression.
 * 
 * @param screen    Écran
 * @param message   Message décrivant l'étape (ex: "Initialisation LoRa...")
 * @param progress  Pourcentage cible (0-100)
 */
void ScreenSplash_SetStep(ScreenSplash_t* screen,
                          const char* message,
                          uint8_t progress);

/**
 * @brief Définit le pourcentage de progression directement
 * 
 * Utile pour les étapes dont la durée est inconnue.
 * 
 * @param screen    Écran
 * @param progress  Pourcentage (0-100)
 */
void ScreenSplash_SetProgress(ScreenSplash_t* screen,
                              uint8_t progress);

/**
 * @brief Signale que l'initialisation est terminée
 * 
 * La barre atteint 100%, puis après la durée minimale,
 * l'écran lance le fondu sortant et appelle on_finished.
 * 
 * @param screen    Écran
 */
void ScreenSplash_Complete(ScreenSplash_t* screen);

/**
 * @brief Signale une erreur d'initialisation
 * 
 * Affiche le message d'erreur en rouge et arrête la progression.
 * L'utilisateur peut redémarrer ou l'écran reste figé.
 * 
 * @param screen        Écran
 * @param error_msg     Message d'erreur descriptif
 */
void ScreenSplash_SetError(ScreenSplash_t* screen,
                           const char* error_msg);

/**
 * @brief Vérifie si le splash screen est encore visible
 * @param screen    Écran
 * @return          true si visible
 */
bool ScreenSplash_IsVisible(ScreenSplash_t* screen);

/**
 * @brief Vérifie si le splash screen est terminé
 * @param screen    Écran
 * @return          true si terminé (peut passer à l'écran suivant)
 */
bool ScreenSplash_IsFinished(ScreenSplash_t* screen);

/**
 * @brief Libère les ressources
 * @param screen    Écran
 */
void ScreenSplash_Deinit(ScreenSplash_t* screen);

/* ======================================================================== */
/*              EXEMPLE D'UTILISATION (DANS main.cpp)                        */
/* ======================================================================== */

/*
 * // === INITIALISATION ===
 * 
 * static ScreenSplash_t g_splash_screen;
 * 
 * void App_Init(void) {
 *     ScreenSplash_Init(&g_splash_screen);
 *     g_splash_screen.on_finished = App_OnSplashFinished;
 *     g_splash_screen.on_error = App_OnSplashError;
 *     
 *     ScreenSplash_Show(&g_splash_screen);
 *     
 *     // === ÉTAPES D'INITIALISATION ===
 *     
 *     ScreenSplash_SetStep(&g_splash_screen, "Demarrage du processeur...", 5);
 *     HAL_Delay(100);  // Simuler initialisation
 *     
 *     ScreenSplash_SetStep(&g_splash_screen, "Initialisation HAL...", 15);
 *     HAL_Init();
 *     SystemClock_Config();
 *     
 *     ScreenSplash_SetStep(&g_splash_screen, "Configuration horloge 180 MHz...", 25);
 *     
 *     ScreenSplash_SetStep(&g_splash_screen, "Initialisation SDRAM...", 35);
 *     SDRAM_Init();
 *     
 *     ScreenSplash_SetStep(&g_splash_screen, "Initialisation LTDC / Ecran...", 45);
 *     Display_Init();
 *     
 *     ScreenSplash_SetStep(&g_splash_screen, "Initialisation DMA2D...", 55);
 *     DMA2D_Init();
 *     
 *     ScreenSplash_SetStep(&g_splash_screen, "Initialisation SPI / LoRa...", 65);
 *     LoRa_Init();
 *     
 *     ScreenSplash_SetStep(&g_splash_screen, "Configuration radio SX1278...", 75);
 *     
 *     ScreenSplash_SetStep(&g_splash_screen, "Initialisation Audio...", 85);
 *     Audio_Init();
 *     
 *     ScreenSplash_SetStep(&g_splash_screen, "Chargement des parametres...", 95);
 *     Settings_Load();
 *     
 *     ScreenSplash_SetStep(&g_splash_screen, "Systeme pret !", 100);
 *     ScreenSplash_Complete(&g_splash_screen);
 * }
 * 
 * void App_OnSplashFinished(void) {
 *     // Passer à l'écran de verrouillage ou d'accueil
 *     if (ScreenLock_IsPinConfigured(&g_lock_screen)) {
 *         ScreenLock_Show(&g_lock_screen);
 *     } else {
 *         ScreenHome_Show(&g_home_screen);
 *     }
 * }
 * 
 * void App_OnSplashError(const char* error_msg) {
 *     // Afficher l'erreur et attendre
 *     // L'utilisateur peut redémarrer
 * }
 */

/* ======================================================================== */
/*              DÉPENDANCES MATÉRIELLES                                      */
/* ======================================================================== */

/*
 * Le splash screen n'a pas de dépendances matérielles spécifiques
 * au-delà de l'affichage LTDC déjà initialisé.
 * 
 * Il utilise :
 *   - LTDC Layer 0 : Fond d'écran
 *   - LTDC Layer 1 : Logo, texte, barre de progression
 *   - DMA2D        : Animations de fondu et remplissage barre
 *   - SDRAM        : Framebuffer pour double buffering
 * 
 * Performances :
 *   - Rendu initial : ~10 ms (DMA2D)
 *   - Mise à jour barre : ~2 ms (DMA2D FillRect)
 *   - Animation fondu : ~5 ms par frame (DMA2D blend)
 */

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */