/**
 * @file    screen_settings_display.h
 * @brief   Écran des réglages d'affichage - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Cet écran permet de configurer tous les paramètres d'affichage
 * de l'écran TFT ILI9488 3.5" 320x480.
 * Il est accessible depuis l'écran Paramètres → Affichage.
 * 
 * ORGANISATION DE L'ÉCRAN :
 * ┌──────────────────────────────────────────────────────────┐
 * │  ← Affichage                                   12:45 ███ │ ← Barre statut
 * │──────────────────────────────────────────────────────────│
 * │                                                          │
 * │  LUMINOSITÉ                                              │
 * │  [████████████████░░░░]  70%                             │ ← Slider + %
 * │  ─────────────────────────●─────────────────────────      │
 * │  ☀️  Actuel  ○  Min  ○  Moyen  ○  Max                   │ ← Presets rapides
 * │                                                          │
 * │  TIMEOUT ÉCRAN                                           │
 * │  Après 30 secondes d'inactivité                          │
 * │  ◉ 15s  ○ 30s  ○ 1min  ○ 2min  ○ 5min  ○ Jamais       │ ← Radio buttons
 * │                                                          │
 * │  ROTATION ÉCRAN                                          │
 * │  ◉ Portrait  ○  Paysage  ○  Portrait inversé            │ ← Radio buttons
 * │  ○  Paysage inversé                                      │
 * │                                                          │
 * │  THÈME DE COULEURS                                       │
 * │  ◉ Sombre  ○  Clair  ○  OLED Noir                       │ ← Radio buttons
 * │  [████████] [████████] [████████]  ← Aperçu couleurs     │
 * │                                                          │
 * │  TAILLE DE POLICE                                        │
 * │  ○  Petite  ◉  Normale  ○  Grande                       │ ← Radio buttons
 * │  Exemple: "Bonjour le monde!"                            │ ← Texte démo
 * │                                                          │
 * │  ÉCONOMIE D'ÉNERGIE                                      │
 * │  [●] Activée       Niveau batterie faible: 15%           │ ← Switch + slider
 * │                                                          │
 * │  ANIMATIONS                                              │
 * │  [●] Activées                                            │ ← Switch ON/OFF
 * │                                                          │
 * │──────────────────────────────────────────────────────────│
 * │  [Appliquer]              [Valeurs par défaut]            │
 * └──────────────────────────────────────────────────────────┘
 * 
 * ARCHITECTURE AFFICHAGE STM32F429 :
 * 
 * Contrôleur LTDC intégré :
 * ┌─────────────────────────────────────────────────────────┐
 * │                   STM32F429                             │
 * │  ┌─────────┐    ┌──────────┐    ┌──────────────────┐   │
 * │  │ Cortex  │───→│ DMA2D    │───→│ LTDC Layer 1     │   │
 * │  │ M4      │    │ Chrom-ART│    │ (Framebuffer 1)  │   │
 * │  │ 180 MHz │    │ Accel.   │    │ 320x480x16bit    │   │
 * │  └─────────┘    └──────────┘    └────────┬─────────┘   │
 * │                                          │              │
 * │  ┌─────────┐    ┌──────────┐    ┌────────▼─────────┐   │
 * │  │ SDRAM   │◄──→│ DMA2D    │    │ LTDC Layer 0     │   │
 * │  │ 8 Mo    │    │ PxP      │    │ (Framebuffer 0)  │   │
 * │  │ FMC     │    │          │    │ 320x480x16bit    │   │
 * │  └─────────┘    └──────────┘    └────────┬─────────┘   │
 * │                                          │              │
 * │                                 ┌────────▼─────────┐   │
 * │                                 │ ILI9488 Display  │   │
 * │                                 │ 320x480 16-bit   │   │
 * │                                 │ Interface parall. │   │
 * │                                 └──────────────────┘   │
 * └─────────────────────────────────────────────────────────┘
 * 
 * RÉTROÉCLAIRAGE :
 *   TIM1_CH1 (PE9) → PWM 25 kHz → Driver LED → Rétroéclairage
 *   Résolution : 12 bits (0-4095)
 *   Plage : 10% (409) à 100% (4095)
 * 
 * FRAMEBUFFER :
 *   - Double buffering en SDRAM (si disponible) : zéro tearing
 *   - Simple buffering en SRAM interne : économie énergie
 *   - Format RGB565 (16 bits/pixel) : 300 Ko par buffer
 * 
 * ANIMATIONS DMA2D :
 *   - Fondu ouverture/fermeture écrans
 *   - Défilement liste avec blending alpha
 *   - Transition entre écrans (slide left/right)
 */

#ifndef SCREEN_SETTINGS_DISPLAY_H
#define SCREEN_SETTINGS_DISPLAY_H

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
#include "../ui/ui_button.h"                    // Boutons
#include "../ui/ui_slider.h"                    // Sliders
#include "../ui/ui_switch.h"                    // Interrupteurs ON/OFF
#include "../ui/ui_radio_group.h"               // Groupes de boutons radio
#include "../ui/ui_color_preview.h"             // Aperçu de couleurs
#include "../ui/ui_statusbar.h"                 // Barre de statut
#include "../ui/ui_navigation.h"                // Navigation
#include "../ui/ui_icons.h"                     // Icônes 🖥️☀️🎨
#include "../services/settings_service.h"       // Persistance paramètres
#include "../drivers/display/display_manager.h"  // Gestionnaire affichage
#include "../drivers/display/display_buffer.h"   // Framebuffer
#include "../drivers/display/ltdc_config.h"      // Configuration LTDC
#include "../drivers/display/dma2d_driver.h"     // Accélérateur DMA2D
#include "../drivers/power/backlight_control.h"  // Contrôle rétroéclairage
#include "../app/app_events.h"                  // Événements
#include "../utils/timer_utils.h"               // Timers

/* ======================================================================== */
/*                     CONSTANTES SYMBOLIQUES                                */
/* ======================================================================== */

/**
 * @brief Nombre de paramètres d'affichage configurables
 */
#define DISPLAY_PARAM_COUNT                 7

/**
 * @brief Identifiants des paramètres d'affichage
 */
typedef enum {
    DISPLAY_PARAM_BRIGHTNESS = 0,       /**< Luminosité (10-100%)             */
    DISPLAY_PARAM_SCREEN_TIMEOUT,       /**< Timeout écran (secondes)         */
    DISPLAY_PARAM_ROTATION,             /**< Rotation écran (0/90/180/270)    */
    DISPLAY_PARAM_THEME,                /**< Thème couleurs (sombre/clair)    */
    DISPLAY_PARAM_FONT_SIZE,            /**< Taille police (petite/normale/grande) */
    DISPLAY_PARAM_POWER_SAVING,         /**< Économie d'énergie (ON/OFF)      */
    DISPLAY_PARAM_ANIMATIONS,           /**< Animations (ON/OFF)              */
} DisplayParamId_t;

/**
 * @brief États de l'écran d'affichage
 */
typedef enum {
    DISPLAY_STATE_IDLE,                 /**< Affichage normal                 */
    DISPLAY_STATE_ADJUSTING_BRIGHTNESS, /**< Réglage luminosité en cours      */
    DISPLAY_STATE_PREVIEW_THEME,        /**< Aperçu thème actif               */
    DISPLAY_STATE_APPLYING,             /**< Application des changements      */
} DisplayScreenState_t;

/**
 * @brief Rotations d'écran possibles
 */
typedef enum {
    DISPLAY_ROTATION_PORTRAIT = 0,      /**< Portrait (0°)                    */
    DISPLAY_ROTATION_LANDSCAPE,         /**< Paysage (90°)                    */
    DISPLAY_ROTATION_PORTRAIT_INV,      /**< Portrait inversé (180°)          */
    DISPLAY_ROTATION_LANDSCAPE_INV,     /**< Paysage inversé (270°)           */
    DISPLAY_ROTATION_COUNT
} DisplayRotation_t;

/**
 * @brief Thèmes de couleurs disponibles
 */
typedef enum {
    DISPLAY_THEME_DARK = 0,             /**< Thème sombre (défaut)            */
    DISPLAY_THEME_LIGHT,                /**< Thème clair                      */
    DISPLAY_THEME_OLED,                 /**< Thème noir OLED (fond vrai noir) */
    DISPLAY_THEME_COUNT
} DisplayTheme_t;

/**
 * @brief Tailles de police disponibles
 */
typedef enum {
    DISPLAY_FONT_SMALL = 0,             /**< Petite police (5x7)              */
    DISPLAY_FONT_NORMAL,                /**< Police normale (8x16)            */
    DISPLAY_FONT_LARGE,                 /**< Grande police (16x24)            */
    DISPLAY_FONT_COUNT
} DisplayFontSize_t;

/**
 * @brief Presets de luminosité
 */
typedef enum {
    BRIGHTNESS_PRESET_MIN = 0,          /**< Minimum (10%)                    */
    BRIGHTNESS_PRESET_LOW,              /**< Bas (25%)                        */
    BRIGHTNESS_PRESET_MEDIUM,           /**< Moyen (50%)                      */
    BRIGHTNESS_PRESET_HIGH,             /**< Élevé (75%)                      */
    BRIGHTNESS_PRESET_MAX,              /**< Maximum (100%)                   */
    BRIGHTNESS_PRESET_COUNT
} BrightnessPreset_t;

/* ======================================================================== */
/*              STRUCTURES DE DONNÉES                                        */
/* ======================================================================== */

/**
 * @brief Structure décrivant un paramètre d'affichage
 */
typedef struct {
    DisplayParamId_t    id;             /**< Identifiant du paramètre         */
    const char*         name;           /**< Nom affiché                      */
    const char*         unit;           /**< Unité ("%", "sec", "")           */

    /* Valeur courante */
    union {
        uint8_t     brightness;         /**< Luminosité (10-100)              */
        uint16_t    timeout_sec;        /**< Timeout écran (10-300)           */
        uint8_t     rotation;           /**< Rotation (0-3)                   */
        uint8_t     theme;             /**< Index thème (0-2)                 */
        uint8_t     font_size;          /**< Index taille police (0-2)        */
        bool        boolean;            /**< ON/OFF                          */
    } value;

    /* Limites */
    uint8_t             min_value;      /**< Valeur minimale                  */
    uint8_t             max_value;      /**< Valeur maximale                  */
    uint8_t             default_value;  /**< Valeur par défaut                */

    /* Options pour les choix discrets */
    const char**        options;        /**< Labels des options               */
    uint8_t             option_count;   /**< Nombre d'options                 */

    /* État */
    bool                is_modified;    /**< true si modifié                  */

    /* Widget associé */
    void*               widget;         /**< Pointeur générique vers le widget */

} DisplayParam_t;

/* ======================================================================== */
/*                     STRUCTURE PRINCIPALE                                  */
/* ======================================================================== */

/**
 * @brief Structure de l'écran des réglages d'affichage
 * 
 * Taille approximative : ~3 Ko
 */
typedef struct {
    /* ---- Héritage de ScreenBase ---- */
    ScreenBase_t base;                          /**< Classe de base           */

    /* ---- État actuel ---- */
    DisplayScreenState_t state;                 /**< État machine d'états     */
    int16_t selected_row;                       /**< Ligne sélectionnée       */

    /* ---- Paramètres d'affichage ---- */
    DisplayParam_t params[DISPLAY_PARAM_COUNT]; /**< Tableau des paramètres   */

    /* ---- Widgets UI ---- */
    UIStatusBar_t   status_bar;                 /**< Barre de statut          */
    UILabel_t*      title_label;                /**< Titre "Affichage"        */
    UIButton_t*     back_button;                /**< Bouton retour            */
    UIButton_t*     apply_button;               /**< Bouton Appliquer         */
    UIButton_t*     defaults_button;            /**< Bouton Valeurs défaut    */

    /* ---- Widgets par paramètre ---- */
    UILabel_t*      param_labels[DISPLAY_PARAM_COUNT];   /**< Noms paramètres */
    UILabel_t*      value_labels[DISPLAY_PARAM_COUNT];   /**< Valeurs         */

    /* Luminosité */
    UISlider_t*     brightness_slider;          /**< Slider luminosité        */
    UIButton_t*     brightness_presets[BRIGHTNESS_PRESET_COUNT]; /**< Presets */

    /* Timeout écran */
    UIRadioGroup_t* timeout_radio;              /**< Radio timeout            */

    /* Rotation */
    UIRadioGroup_t* rotation_radio;             /**< Radio rotation           */

    /* Thème */
    UIRadioGroup_t* theme_radio;                /**< Radio thème              */
    UIColorPreview_t* theme_preview;            /**< Aperçu couleurs thème    */

    /* Taille police */
    UIRadioGroup_t* font_size_radio;            /**< Radio taille police      */
    UILabel_t*      font_demo_label;            /**< Texte démonstration      */

    /* Économie énergie */
    UISwitch_t*     power_saving_switch;        /**< Switch économie énergie  */
    UISlider_t*     low_battery_slider;         /**< Seuil batterie faible    */
    UILabel_t*      low_battery_label;          /**< Label seuil              */

    /* Animations */
    UISwitch_t*     animations_switch;          /**< Switch animations        */

    /* ---- Label de statut ---- */
    UILabel_t*      status_label;               /**< Message statut           */

    /* ---- Services ---- */
    SettingsService_t*  settings_service;       /**< Service persistance      */
    DisplayManager_t*   display_manager;        /**< Gestionnaire affichage   */
    BacklightControl_t* backlight;              /**< Contrôle rétroéclairage  */

    /* ---- Dialogue de confirmation ---- */
    UIDialog_t*     confirm_dialog;             /**< Dialogue confirmation    */

    /* ---- Callbacks ---- */
    void (*on_back_pressed)(void);              /**< Callback retour          */

    /* ---- Timers ---- */
    TimerHandle_t   brightness_preview_timer;   /**< Timeout aperçu luminosité */
    TimerHandle_t   theme_preview_timer;        /**< Timeout aperçu thème     */

    /* ---- Sauvegarde pour aperçu ---- */
    uint8_t         saved_brightness;           /**< Luminosité avant aperçu  */
    DisplayTheme_t  saved_theme;                /**< Thème avant aperçu       */

} ScreenSettingsDisplay_t;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/**
 * @brief Initialise l'écran des réglages d'affichage
 * 
 * @param screen            Structure à initialiser
 * @param settings_service  Service de paramètres persistants
 * @param backlight         Contrôleur de rétroéclairage (pour aperçu temps réel)
 * @return                  true si succès
 */
bool ScreenSettingsDisplay_Init(ScreenSettingsDisplay_t* screen,
                                SettingsService_t* settings_service,
                                BacklightControl_t* backlight);

/**
 * @brief Affiche l'écran d'affichage
 * @param screen    Écran à afficher
 */
void ScreenSettingsDisplay_Show(ScreenSettingsDisplay_t* screen);

/**
 * @brief Masque l'écran d'affichage
 * @param screen    Écran à masquer
 */
void ScreenSettingsDisplay_Hide(ScreenSettingsDisplay_t* screen);

/**
 * @brief Mise à jour périodique
 * @param screen    Écran à mettre à jour
 */
void ScreenSettingsDisplay_Update(ScreenSettingsDisplay_t* screen);

/**
 * @brief Gère les événements tactiles
 * @param screen    Écran
 * @param event     Événement tactile
 * @return          true si consommé
 */
bool ScreenSettingsDisplay_HandleTouch(ScreenSettingsDisplay_t* screen,
                                       const TouchEvent_t* event);

/**
 * @brief Gère les touches physiques
 * @param screen    Écran
 * @param key       Code touche
 * @return          true si consommé
 */
bool ScreenSettingsDisplay_HandleKey(ScreenSettingsDisplay_t* screen,
                                     KeyCode_t key);

/**
 * @brief Applique tous les paramètres modifiés
 * @param screen    Écran
 * @return          Nombre de paramètres appliqués
 */
uint8_t ScreenSettingsDisplay_ApplyParams(ScreenSettingsDisplay_t* screen);

/**
 * @brief Restaure les valeurs par défaut
 * @param screen    Écran
 */
void ScreenSettingsDisplay_RestoreDefaults(ScreenSettingsDisplay_t* screen);

/**
 * @brief Définit la luminosité et l'applique immédiatement
 * 
 * Modifie le rapport cyclique PWM du rétroéclairage.
 * La luminosité minimale est 10% (PWM=409) pour éviter
 * l'extinction complète qui rendrait l'écran inutilisable.
 * 
 * @param screen        Écran
 * @param brightness    Luminosité en pourcentage (10-100)
 */
void ScreenSettingsDisplay_SetBrightness(ScreenSettingsDisplay_t* screen,
                                         uint8_t brightness);

/**
 * @brief Applique un preset de luminosité
 * @param screen    Écran
 * @param preset    Preset (MIN/LOW/MEDIUM/HIGH/MAX)
 */
void ScreenSettingsDisplay_ApplyBrightnessPreset(ScreenSettingsDisplay_t* screen,
                                                  BrightnessPreset_t preset);

/**
 * @brief Définit le timeout d'extinction de l'écran
 * @param screen        Écran
 * @param timeout_sec   Délai en secondes (0 = jamais)
 */
void ScreenSettingsDisplay_SetScreenTimeout(ScreenSettingsDisplay_t* screen,
                                            uint16_t timeout_sec);

/**
 * @brief Définit la rotation de l'écran
 * 
 * Modifie la configuration LTDC et recalcule les dimensions.
 * ⚠️ Provoque un réaffichage complet de l'interface.
 * 
 * @param screen    Écran
 * @param rotation  Rotation souhaitée
 */
void ScreenSettingsDisplay_SetRotation(ScreenSettingsDisplay_t* screen,
                                       DisplayRotation_t rotation);

/**
 * @brief Définit le thème de couleurs
 * 
 * Change la palette de couleurs de l'interface.
 * Appliqué immédiatement pour un aperçu en direct.
 * 
 * @param screen    Écran
 * @param theme     Thème souhaité
 */
void ScreenSettingsDisplay_SetTheme(ScreenSettingsDisplay_t* screen,
                                    DisplayTheme_t theme);

/**
 * @brief Définit la taille de police globale
 * @param screen    Écran
 * @param font_size Taille de police
 */
void ScreenSettingsDisplay_SetFontSize(ScreenSettingsDisplay_t* screen,
                                       DisplayFontSize_t font_size);

/**
 * @brief Active/désactive l'économie d'énergie
 * @param screen    Écran
 * @param enabled   true = activé
 */
void ScreenSettingsDisplay_SetPowerSaving(ScreenSettingsDisplay_t* screen,
                                          bool enabled);

/**
 * @brief Définit le seuil de batterie faible pour l'économie d'énergie
 * @param screen        Écran
 * @param threshold     Seuil en pourcentage (5-50)
 */
void ScreenSettingsDisplay_SetLowBatteryThreshold(ScreenSettingsDisplay_t* screen,
                                                   uint8_t threshold);

/**
 * @brief Active/désactive les animations
 * @param screen    Écran
 * @param enabled   true = animations activées
 */
void ScreenSettingsDisplay_SetAnimations(ScreenSettingsDisplay_t* screen,
                                         bool enabled);

/**
 * @brief Affiche un aperçu temporaire du thème sélectionné
 * 
 * Applique le thème pendant 3 secondes puis revient
 * au thème précédent si l'utilisateur n'a pas validé.
 * 
 * @param screen    Écran
 * @param theme     Thème à prévisualiser
 */
void ScreenSettingsDisplay_PreviewTheme(ScreenSettingsDisplay_t* screen,
                                        DisplayTheme_t theme);

/**
 * @brief Libère les ressources
 * @param screen    Écran
 */
void ScreenSettingsDisplay_Deinit(ScreenSettingsDisplay_t* screen);

/* ======================================================================== */
/*              CONVERSION LUMINOSITÉ                                        */
/* ======================================================================== */

/**
 * @brief Convertit un pourcentage en valeur PWM (12-bit)
 * 
 * La luminosité est limitée à 10% minimum pour éviter
 * un écran complètement noir.
 * 
 * @param percentage    Pourcentage (10-100)
 * @return              Valeur PWM (409-4095)
 */
uint16_t DisplaySettings_BrightnessToPWM(uint8_t percentage);

/**
 * @brief Convertit une valeur PWM en pourcentage
 * @param pwm_value     Valeur PWM (0-4095)
 * @return              Pourcentage (10-100)
 */
uint8_t DisplaySettings_PWMToBrightness(uint16_t pwm_value);

/**
 * @brief Retourne la valeur de luminosité pour un preset
 * @param preset    Preset
 * @return          Pourcentage (10, 25, 50, 75, 100)
 */
uint8_t DisplaySettings_GetPresetValue(BrightnessPreset_t preset);

/* ======================================================================== */
/*              VALEURS PAR DÉFAUT                                          */
/* ======================================================================== */

/*
 * | Paramètre              | Défaut | Plage        |
 * |------------------------|--------|--------------|
 * | Luminosité             | 70%    | 10-100%      |
 * | Timeout écran          | 30s    | 10-300s / ∞  |
 * | Rotation               | 0°     | 0/90/180/270 |
 * | Thème                  | Sombre | S/C/OLED     |
 * | Taille police          | Normale| S/M/L        |
 * | Économie énergie       | OFF    | ON/OFF       |
 * | Seuil batterie faible  | 15%    | 5-50%        |
 * | Animations             | ON     | ON/OFF       |
 */

/* ======================================================================== */
/*              THÈMES DE COULEURS                                           */
/* ======================================================================== */

/*
 * Définition des palettes de couleurs pour chaque thème :
 * 
 * THÈME SOMBRE (défaut) :
 *   Fond principal  : #121212 (gris très foncé)
 *   Fond surface    : #1E1E1E
 *   Texte primaire  : #FFFFFF
 *   Texte secondaire: #B0B0B0
 *   Accent          : #4FC3F7 (bleu clair)
 *   Highlight       : #03DAC6 (cyan)
 * 
 * THÈME CLAIR :
 *   Fond principal  : #FAFAFA (blanc cassé)
 *   Fond surface    : #FFFFFF
 *   Texte primaire  : #212121
 *   Texte secondaire: #757575
 *   Accent          : #1976D2 (bleu)
 *   Highlight       : #00897B (teal)
 * 
 * THÈME OLED NOIR :
 *   Fond principal  : #000000 (vrai noir)
 *   Fond surface    : #0A0A0A
 *   Texte primaire  : #FFFFFF
 *   Texte secondaire: #888888
 *   Accent          : #FF6D00 (orange)
 *   Highlight       : #FFAB00 (ambre)
 */

/* ======================================================================== */
/*              DÉPENDANCES MATÉRIELLES                                      */
/* ======================================================================== */

/*
 * Connexions affichage STM32F429 → ILI9488 :
 * 
 * Interface parallèle 16-bit (LTDC) :
 *   PD14 (D0)  → DB8   PD15 (D1)  → DB9
 *   PD0  (D2)  → DB10  PD1  (D3)  → DB11
 *   PE7  (D4)  → DB12  PE8  (D5)  → DB13
 *   PE9  (D6)  → DB14  PE10 (D7)  → DB15
 *   PE11 (D8)  → DB16  PE12 (D9)  → DB17
 *   PE13 (D10) → DB0   PE14 (D11) → DB1
 *   PE15 (D12) → DB2   PD8  (D13) → DB3
 *   PD9  (D14) → DB4   PD10 (D15) → DB5
 * 
 * Signaux de contrôle :
 *   PB0  → DCX (Data/Command)  [RS]
 *   PB1  → WRX (Write strobe)  [WR]
 *   PB5  → RDX (Read strobe)   [RD]
 *   PC6  → CSX (Chip Select)   [CS]
 *   PC7  → RESX (Reset)        [RST]
 * 
 * Rétroéclairage (PWM) :
 *   PE9  → TIM1_CH1 → Backlight LED (via driver)
 * 
 * ⚠️ Toutes les broches sont configurables dans ltdc_config.h
 */

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_SETTINGS_DISPLAY_H */

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */