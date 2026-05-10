/**
 * @file    screen_settings_display.cpp
 * @brief   Implémentation de l'écran des réglages d'affichage
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente la configuration complète de l'affichage TFT ILI9488.
 * 
 * FONCTIONNEMENT :
 * 
 * 1. LUMINOSITÉ (PWM TIM1) :
 *    - Le slider ajuste le rapport cyclique du PWM en temps réel
 *    - 5 presets rapides (Min 10%, Bas 25%, Moyen 50%, Élevé 75%, Max 100%)
 *    - La valeur PWM est limitée à 10% minimum (écran jamais complètement noir)
 *    - Application immédiate via BacklightControl
 * 
 * 2. TIMEOUT ÉCRAN :
 *    - Définit le délai avant extinction automatique
 *    - Options : 15s, 30s, 1min, 2min, 5min, Jamais
 *    - Géré par PowerManager (minuterie d'inactivité)
 * 
 * 3. ROTATION ÉCRAN :
 *    - Modifie la configuration LTDC (timings, dimensions)
 *    - Reconfigure le framebuffer (swap largeur/hauteur)
 *    - ⚠️ Provoque un réaffichage complet
 * 
 * 4. THÈME DE COULEURS :
 *    - 3 thèmes : Sombre, Clair, OLED Noir
 *    - Change la palette de couleurs globale (ui_theme)
 *    - Aperçu temporaire avec retour automatique
 *    - Appliqué immédiatement pour preview
 * 
 * 5. TAILLE DE POLICE :
 *    - 3 tailles : Petite (5x7), Normale (8x16), Grande (16x24)
 *    - Texte de démonstration mis à jour en direct
 * 
 * 6. ÉCONOMIE D'ÉNERGIE :
 *    - Active/désactive le mode économie
 *    - Réduit automatiquement la luminosité sous un seuil batterie
 *    - Seuil configurable (5-50%)
 * 
 * 7. ANIMATIONS :
 *    - Active/désactive les animations DMA2D
 *    - Économise CPU et batterie si désactivées
 * 
 * ARCHITECTURE LOGICIELLE :
 * 
 *   ScreenSettingsDisplay (UI)
 *        │
 *        ├──→ BacklightControl (PWM TIM1)
 *        │        └──→ TIM1_CH1 (PE9) → Driver LED
 *        │
 *        ├──→ DisplayManager (LTDC)
 *        │        ├──→ LTDC Layer 0/1
 *        │        ├──→ Framebuffer SDRAM
 *        │        └──→ DMA2D (Chrom-ART)
 *        │
 *        ├──→ UITheme (palette couleurs)
 *        │        └──→ Variables globales THEME_*
 *        │
 *        └──→ SettingsService (persistance flash)
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "screen_settings_display.h"

/* UI */
#include "../ui/ui_core.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_widgets.h"
#include "../ui/ui_label.h"
#include "../ui/ui_button.h"
#include "../ui/ui_slider.h"
#include "../ui/ui_switch.h"
#include "../ui/ui_radio_group.h"
#include "../ui/ui_color_preview.h"
#include "../ui/ui_dialog.h"
#include "../ui/ui_statusbar.h"
#include "../ui/ui_navigation.h"
#include "../ui/ui_icons.h"
#include "../ui/ui_animations.h"
#include "../ui/ui_draw_primitives.h"
#include "../ui/ui_fonts.h"

/* Services */
#include "../services/settings_service.h"

/* Drivers */
#include "../drivers/display/display_manager.h"
#include "../drivers/display/display_buffer.h"
#include "../drivers/display/ltdc_config.h"
#include "../drivers/display/dma2d_driver.h"
#include "../drivers/power/backlight_control.h"
#include "../drivers/power/power_manager.h"

/* Utilitaires */
#include "../utils/string_utils.h"
#include "../utils/timer_utils.h"
#include "../utils/debug_utils.h"
#include "../utils/math_utils.h"

/* Standard */
#include <string.h>
#include <stdio.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs de debug */
#define TAG                                 "ScreenSettingsDisplay"

/** Dimensions de l'écran */
#define SCREEN_WIDTH                        320
#define SCREEN_HEIGHT                       480

/** Zone de contenu */
#define CONTENT_Y_START                     60
#define CONTENT_X_MARGIN                    15
#define CONTENT_WIDTH                       (SCREEN_WIDTH - 2 * CONTENT_X_MARGIN)

/** Hauteur d'une ligne de paramètre */
#define PARAM_ROW_HEIGHT                    52
#define PARAM_ROW_SPACING                   8

/** Positions Y des paramètres */
#define PARAM_BRIGHTNESS_Y                  (CONTENT_Y_START + 5)
#define PARAM_TIMEOUT_Y                     (PARAM_BRIGHTNESS_Y + PARAM_ROW_HEIGHT + 20)
#define PARAM_ROTATION_Y                    (PARAM_TIMEOUT_Y + PARAM_ROW_HEIGHT + 10)
#define PARAM_THEME_Y                       (PARAM_ROTATION_Y + PARAM_ROW_HEIGHT + 10)
#define PARAM_FONT_Y                        (PARAM_THEME_Y + PARAM_ROW_HEIGHT + 10)
#define PARAM_POWER_SAVING_Y                (PARAM_FONT_Y + PARAM_ROW_HEIGHT + 10)
#define PARAM_ANIMATIONS_Y                  (PARAM_POWER_SAVING_Y + PARAM_ROW_HEIGHT + 5)

/** Positions des boutons du bas */
#define BOTTOM_BUTTONS_Y                    425
#define APPLY_BUTTON_X                      15
#define APPLY_BUTTON_WIDTH                  140
#define DEFAULTS_BUTTON_X                   165
#define DEFAULTS_BUTTON_WIDTH               140

/** Dimensions sliders */
#define SLIDER_X                            90
#define SLIDER_WIDTH                        160
#define SLIDER_HEIGHT                       18

/** Presets luminosité */
#define PRESET_BUTTONS_Y_OFFSET             42
#define PRESET_BUTTON_WIDTH                 48
#define PRESET_BUTTON_HEIGHT                24
#define PRESET_BUTTON_SPACING               6

/** Valeurs des presets */
#define BRIGHTNESS_PRESET_VALUES            {10, 25, 50, 75, 100}

/** Seuil luminosité minimale (jamais 0%) */
#define BRIGHTNESS_MIN_PERCENT              10
#define BRIGHTNESS_MAX_PERCENT              100

/** PWM 12-bit */
#define PWM_MIN                             409     /* 10% */
#define PWM_MAX                             4095    /* 100% */

/** Timeout aperçu (ms) */
#define PREVIEW_TIMEOUT_MS                  3000

/** Timeout message statut (ms) */
#define STATUS_TIMEOUT_MS                   2500

/** Texte de démonstration pour la taille de police */
#define FONT_DEMO_TEXT                      "Bonjour le monde!"

/* ======================================================================== */
/*                VARIABLES STATIQUES (OPTIONS)                             */
/* ======================================================================== */

/** Labels des timeouts écran */
static const char* TIMEOUT_OPTIONS[] = {
    "15s", "30s", "1min", "2min", "5min", "Jamais"
};
static const uint16_t TIMEOUT_VALUES[] = {
    15, 30, 60, 120, 300, 0  /* 0 = jamais */
};
#define TIMEOUT_OPTIONS_COUNT   6

/** Labels des rotations */
static const char* ROTATION_OPTIONS[] = {
    "Portrait", "Paysage", "Portrait inv.", "Paysage inv."
};

/** Labels des thèmes */
static const char* THEME_OPTIONS[] = {
    "Sombre", "Clair", "OLED Noir"
};

/** Labels des tailles de police */
static const char* FONT_SIZE_OPTIONS[] = {
    "Petite", "Normale", "Grande"
};

/** Labels des presets luminosité */
static const char* BRIGHTNESS_PRESET_LABELS[] = {
    "Min", "Bas", "Moyen", "Eleve", "Max"
};

/** Valeurs des presets en pourcentage */
static const uint8_t BRIGHTNESS_PRESET_PERCENT[] = {
    10, 25, 50, 75, 100
};

/** Polices correspondant aux tailles */
static const UIFont_t* FONT_SIZE_MAP[] = {
    &font_small,        /* Petite  (5x7)  */
    &font_medium,       /* Normale (8x16) */
    &font_large,        /* Grande  (16x24) */
};

/** Couleurs d'aperçu par thème */
static const uint16_t THEME_PREVIEW_COLORS[DISPLAY_THEME_COUNT][5] = {
    /* Fond, Surface, Texte primaire, Texte secondaire, Accent */
    [DISPLAY_THEME_DARK]  = {0x1082, 0x18E3, 0xFFFF, 0xBDF7, 0x4DF9},  /* Sombre */
    [DISPLAY_THEME_LIGHT] = {0xFFBE, 0xFFFF, 0x2104, 0x7BEF, 0x1CF6},  /* Clair  */
    [DISPLAY_THEME_OLED]  = {0x0000, 0x0841, 0xFFFF, 0x8C71, 0xFD80},  /* OLED   */
};

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

/* --- Initialisation --- */
static void init_params(ScreenSettingsDisplay_t* screen);
static void load_params_from_service(ScreenSettingsDisplay_t* screen);
static void save_params_to_service(ScreenSettingsDisplay_t* screen);

/* --- Création widgets --- */
static void create_brightness_widgets(ScreenSettingsDisplay_t* screen);
static void create_timeout_widgets(ScreenSettingsDisplay_t* screen);
static void create_rotation_widgets(ScreenSettingsDisplay_t* screen);
static void create_theme_widgets(ScreenSettingsDisplay_t* screen);
static void create_font_size_widgets(ScreenSettingsDisplay_t* screen);
static void create_power_saving_widgets(ScreenSettingsDisplay_t* screen);
static void create_animations_widgets(ScreenSettingsDisplay_t* screen);

/* --- Callbacks widgets --- */
static void on_brightness_changed(void* context, int16_t value);
static void on_brightness_preset_clicked(void* context);
static void on_timeout_selected(void* context, uint8_t index);
static void on_rotation_selected(void* context, uint8_t index);
static void on_theme_selected(void* context, uint8_t index);
static void on_font_size_selected(void* context, uint8_t index);
static void on_power_saving_changed(void* context, bool enabled);
static void on_low_battery_changed(void* context, int16_t value);
static void on_animations_changed(void* context, bool enabled);
static void on_apply_clicked(void* context);
static void on_defaults_clicked(void* context);
static void on_back_clicked(void* context);

/* --- Application --- */
static void apply_brightness(ScreenSettingsDisplay_t* screen);
static void apply_screen_timeout(ScreenSettingsDisplay_t* screen);
static void apply_rotation(ScreenSettingsDisplay_t* screen);
static void apply_theme(ScreenSettingsDisplay_t* screen);
static void apply_font_size(ScreenSettingsDisplay_t* screen);
static void apply_power_saving(ScreenSettingsDisplay_t* screen);
static void apply_animations(ScreenSettingsDisplay_t* screen);

/* --- Rendu --- */
static void draw_param_row_background(int16_t y, bool selected);
static void draw_param_label(const char* label, int16_t y);
static void update_brightness_display(ScreenSettingsDisplay_t* screen);
static void update_font_demo(ScreenSettingsDisplay_t* screen);
static void update_theme_preview(ScreenSettingsDisplay_t* screen);
static void update_status_message(ScreenSettingsDisplay_t* screen, const char* msg);

/* --- Timers --- */
static void preview_timer_callback(TimerHandle_t timer);
static void status_timer_callback(TimerHandle_t timer);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise l'écran des réglages d'affichage
 */
bool ScreenSettingsDisplay_Init(ScreenSettingsDisplay_t* screen,
                                SettingsService_t* settings_service,
                                BacklightControl_t* backlight)
{
    if (!screen || !settings_service || !backlight) {
        DEBUG_ERROR(TAG, "Paramètres invalides");
        return false;
    }

    DEBUG_INFO(TAG, "Initialisation de l'écran affichage...");

    /* Mise à zéro */
    memset(screen, 0, sizeof(ScreenSettingsDisplay_t));

    /* Classe de base */
    ScreenBase_Init(&screen->base, SCREEN_ID_SETTINGS_DISPLAY, "Affichage");

    /* Services */
    screen->settings_service = settings_service;
    screen->backlight = backlight;
    screen->display_manager = DisplayManager_GetInstance();

    /* État initial */
    screen->state = DISPLAY_STATE_IDLE;
    screen->selected_row = -1;

    /* ---- Widgets de base ---- */

    /* Barre de statut */
    UIStatusBar_Init(&screen->status_bar);

    /* Titre */
    screen->title_label = UILabel_Create();
    UILabel_SetText(screen->title_label, "Affichage");
    UILabel_SetFont(screen->title_label, &font_large_bold);
    UILabel_SetColor(screen->title_label, THEME_TEXT_PRIMARY);
    UILabel_SetPosition(screen->title_label, CONTENT_X_MARGIN, 8);

    /* Bouton Retour */
    screen->back_button = UIButton_Create();
    UIButton_SetText(screen->back_button, "← Retour");
    UIButton_SetFont(screen->back_button, &font_small);
    UIButton_SetSize(screen->back_button, 80, 30);
    UIButton_SetPosition(screen->back_button, SCREEN_WIDTH - 95, 10);
    UIButton_SetOnClick(screen->back_button, on_back_clicked, screen);
    UIButton_SetCornerRadius(screen->back_button, 6);
    UIButton_SetColor(screen->back_button, THEME_BUTTON_SECONDARY);

    /* Label de statut */
    screen->status_label = UILabel_Create();
    UILabel_SetText(screen->status_label, "");
    UILabel_SetFont(screen->status_label, &font_small);
    UILabel_SetColor(screen->status_label, THEME_SUCCESS);
    UILabel_SetPosition(screen->status_label, CONTENT_X_MARGIN, SCREEN_HEIGHT - 55);

    /* Créer les widgets pour chaque paramètre */
    create_brightness_widgets(screen);
    create_timeout_widgets(screen);
    create_rotation_widgets(screen);
    create_theme_widgets(screen);
    create_font_size_widgets(screen);
    create_power_saving_widgets(screen);
    create_animations_widgets(screen);

    /* Boutons du bas */
    screen->apply_button = UIButton_Create();
    UIButton_SetText(screen->apply_button, "Appliquer");
    UIButton_SetFont(screen->apply_button, &font_medium);
    UIButton_SetSize(screen->apply_button, APPLY_BUTTON_WIDTH, 40);
    UIButton_SetPosition(screen->apply_button, APPLY_BUTTON_X, BOTTOM_BUTTONS_Y);
    UIButton_SetOnClick(screen->apply_button, on_apply_clicked, screen);
    UIButton_SetCornerRadius(screen->apply_button, 8);
    UIButton_SetColor(screen->apply_button, THEME_ACCENT);

    screen->defaults_button = UIButton_Create();
    UIButton_SetText(screen->defaults_button, "Defauts");
    UIButton_SetFont(screen->defaults_button, &font_medium);
    UIButton_SetSize(screen->defaults_button, DEFAULTS_BUTTON_WIDTH, 40);
    UIButton_SetPosition(screen->defaults_button, DEFAULTS_BUTTON_X, BOTTOM_BUTTONS_Y);
    UIButton_SetOnClick(screen->defaults_button, on_defaults_clicked, screen);
    UIButton_SetCornerRadius(screen->defaults_button, 8);
    UIButton_SetColor(screen->defaults_button, THEME_BUTTON_SECONDARY);

    /* Dialogue de confirmation */
    screen->confirm_dialog = UIDialog_Create();
    UIDialog_SetVisible(screen->confirm_dialog, false);

    /* Initialiser les paramètres */
    init_params(screen);
    load_params_from_service(screen);

    /* Sauvegarde pour aperçu */
    screen->saved_brightness = screen->params[DISPLAY_PARAM_BRIGHTNESS].value.brightness;
    screen->saved_theme = DISPLAY_THEME_DARK;

    /* Timers */
    screen->brightness_preview_timer = Timer_Create("BrightnessPreview",
                                                     PREVIEW_TIMEOUT_MS,
                                                     false,
                                                     preview_timer_callback,
                                                     screen);
    screen->theme_preview_timer = Timer_Create("ThemePreview",
                                                PREVIEW_TIMEOUT_MS,
                                                false,
                                                preview_timer_callback,
                                                screen);

    DEBUG_INFO(TAG, "Initialisation terminée");

    return true;
}

/**
 * @brief Affiche l'écran d'affichage
 */
void ScreenSettingsDisplay_Show(ScreenSettingsDisplay_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Affichage de l'écran affichage");

    screen->state = DISPLAY_STATE_IDLE;
    screen->selected_row = -1;

    /* Recharger depuis le service */
    load_params_from_service(screen);

    /* Fond d'écran */
    Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);

    /* Barre de statut */
    UIStatusBar_Draw(&screen->status_bar);

    /* Barre de titre */
    Display_FillRect(0, 25, SCREEN_WIDTH, 38, THEME_BG_SURFACE);
    UILabel_Draw(screen->title_label);
    UIButton_Draw(screen->back_button);
    Display_DrawHLine(0, 62, SCREEN_WIDTH, THEME_DIVIDER);

    /* ---- Dessiner chaque paramètre ---- */

    /* 1. Luminosité */
    draw_param_row_background(PARAM_BRIGHTNESS_Y, false);
    draw_param_label("LUMINOSITE", PARAM_BRIGHTNESS_Y);
    UISlider_Draw(screen->brightness_slider);
    update_brightness_display(screen);
    /* Presets */
    for (int i = 0; i < BRIGHTNESS_PRESET_COUNT; i++) {
        UIButton_Draw(screen->brightness_presets[i]);
    }

    /* 2. Timeout écran */
    draw_param_row_background(PARAM_TIMEOUT_Y, false);
    draw_param_label("TIMEOUT ECRAN", PARAM_TIMEOUT_Y);
    UIRadioGroup_Draw(screen->timeout_radio);

    /* 3. Rotation */
    draw_param_row_background(PARAM_ROTATION_Y, false);
    draw_param_label("ROTATION", PARAM_ROTATION_Y);
    UIRadioGroup_Draw(screen->rotation_radio);

    /* 4. Thème */
    draw_param_row_background(PARAM_THEME_Y, false);
    draw_param_label("THEME COULEURS", PARAM_THEME_Y);
    UIRadioGroup_Draw(screen->theme_radio);
    update_theme_preview(screen);

    /* 5. Taille de police */
    draw_param_row_background(PARAM_FONT_Y, false);
    draw_param_label("TAILLE POLICE", PARAM_FONT_Y);
    UIRadioGroup_Draw(screen->font_size_radio);
    update_font_demo(screen);

    /* 6. Économie d'énergie */
    draw_param_row_background(PARAM_POWER_SAVING_Y, false);
    draw_param_label("ECONOMIE ENERGIE", PARAM_POWER_SAVING_Y);
    UISwitch_Draw(screen->power_saving_switch);
    if (screen->params[DISPLAY_PARAM_POWER_SAVING].value.boolean) {
        UISlider_Draw(screen->low_battery_slider);
        UILabel_Draw(screen->low_battery_label);
    }

    /* 7. Animations */
    draw_param_row_background(PARAM_ANIMATIONS_Y, false);
    draw_param_label("ANIMATIONS", PARAM_ANIMATIONS_Y);
    UISwitch_Draw(screen->animations_switch);

    /* Boutons du bas */
    Display_DrawHLine(0, BOTTOM_BUTTONS_Y - 8, SCREEN_WIDTH, THEME_DIVIDER);
    UIButton_Draw(screen->apply_button);
    UIButton_Draw(screen->defaults_button);

    /* Label statut */
    UILabel_Draw(screen->status_label);

    screen->base.is_visible = true;
}

/**
 * @brief Masque l'écran
 */
void ScreenSettingsDisplay_Hide(ScreenSettingsDisplay_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Masquage de l'écran affichage");

    Timer_Stop(screen->brightness_preview_timer);
    Timer_Stop(screen->theme_preview_timer);

    screen->base.is_visible = false;
}

/**
 * @brief Mise à jour périodique
 */
void ScreenSettingsDisplay_Update(ScreenSettingsDisplay_t* screen)
{
    if (!screen || !screen->base.is_visible) return;

    UIStatusBar_Update(&screen->status_bar);
}

/**
 * @brief Gère les événements tactiles
 */
bool ScreenSettingsDisplay_HandleTouch(ScreenSettingsDisplay_t* screen,
                                       const TouchEvent_t* event)
{
    if (!screen || !event) return false;

    /* Dialogue de confirmation */
    if (UIDialog_IsVisible(screen->confirm_dialog)) {
        return UIDialog_HandleTouch(screen->confirm_dialog, event);
    }

    /* Boutons du bas */
    if (UIButton_HitTest(screen->apply_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->apply_button);
        }
        return true;
    }
    if (UIButton_HitTest(screen->defaults_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->defaults_button);
        }
        return true;
    }
    if (UIButton_HitTest(screen->back_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->back_button);
        }
        return true;
    }

    /* Presets luminosité */
    for (int i = 0; i < BRIGHTNESS_PRESET_COUNT; i++) {
        if (UIButton_HitTest(screen->brightness_presets[i], event->x, event->y)) {
            if (event->type == TOUCH_EVENT_TAP) {
                UIButton_TriggerClick(screen->brightness_presets[i]);
            }
            return true;
        }
    }

    /* Sliders */
    if (UISlider_HitTest(screen->brightness_slider, event->x, event->y)) {
        return UISlider_HandleTouch(screen->brightness_slider, event);
    }
    if (UISlider_HitTest(screen->low_battery_slider, event->x, event->y)) {
        return UISlider_HandleTouch(screen->low_battery_slider, event);
    }

    /* Switches */
    if (UISwitch_HitTest(screen->power_saving_switch, event->x, event->y)) {
        return UISwitch_HandleTouch(screen->power_saving_switch, event);
    }
    if (UISwitch_HitTest(screen->animations_switch, event->x, event->y)) {
        return UISwitch_HandleTouch(screen->animations_switch, event);
    }

    /* Radio groups */
    if (UIRadioGroup_HitTest(screen->timeout_radio, event->x, event->y)) {
        return UIRadioGroup_HandleTouch(screen->timeout_radio, event);
    }
    if (UIRadioGroup_HitTest(screen->rotation_radio, event->x, event->y)) {
        return UIRadioGroup_HandleTouch(screen->rotation_radio, event);
    }
    if (UIRadioGroup_HitTest(screen->theme_radio, event->x, event->y)) {
        return UIRadioGroup_HandleTouch(screen->theme_radio, event);
    }
    if (UIRadioGroup_HitTest(screen->font_size_radio, event->x, event->y)) {
        return UIRadioGroup_HandleTouch(screen->font_size_radio, event);
    }

    return false;
}

/**
 * @brief Gère les touches physiques
 */
bool ScreenSettingsDisplay_HandleKey(ScreenSettingsDisplay_t* screen,
                                     KeyCode_t key)
{
    if (!screen) return false;

    switch (key) {
        case KEY_UP:
            if (screen->selected_row > 0) screen->selected_row--;
            return true;

        case KEY_DOWN:
            if (screen->selected_row < DISPLAY_PARAM_COUNT - 1) screen->selected_row++;
            return true;

        case KEY_LEFT:
            /* Décrémenter la valeur */
            if (screen->selected_row == 0) {
                uint8_t brt = screen->params[DISPLAY_PARAM_BRIGHTNESS].value.brightness;
                if (brt > BRIGHTNESS_MIN_PERCENT) {
                    ScreenSettingsDisplay_SetBrightness(screen, brt - 5);
                }
            }
            return true;

        case KEY_RIGHT:
            /* Incrémenter la valeur */
            if (screen->selected_row == 0) {
                uint8_t brt = screen->params[DISPLAY_PARAM_BRIGHTNESS].value.brightness;
                if (brt < BRIGHTNESS_MAX_PERCENT) {
                    ScreenSettingsDisplay_SetBrightness(screen, brt + 5);
                }
            }
            return true;

        case KEY_BACK:
        case KEY_CANCEL:
            on_back_clicked(screen);
            return true;

        default:
            break;
    }

    return false;
}

/**
 * @brief Applique tous les paramètres modifiés
 */
uint8_t ScreenSettingsDisplay_ApplyParams(ScreenSettingsDisplay_t* screen)
{
    if (!screen) return 0;

    DEBUG_INFO(TAG, "Application des paramètres affichage...");

    screen->state = DISPLAY_STATE_APPLYING;
    uint8_t count = 0;

    for (int i = 0; i < DISPLAY_PARAM_COUNT; i++) {
        if (!screen->params[i].is_modified) continue;

        switch (screen->params[i].id) {
            case DISPLAY_PARAM_BRIGHTNESS:
                apply_brightness(screen);
                break;
            case DISPLAY_PARAM_SCREEN_TIMEOUT:
                apply_screen_timeout(screen);
                break;
            case DISPLAY_PARAM_ROTATION:
                apply_rotation(screen);
                break;
            case DISPLAY_PARAM_THEME:
                apply_theme(screen);
                break;
            case DISPLAY_PARAM_FONT_SIZE:
                apply_font_size(screen);
                break;
            case DISPLAY_PARAM_POWER_SAVING:
                apply_power_saving(screen);
                break;
            case DISPLAY_PARAM_ANIMATIONS:
                apply_animations(screen);
                break;
            default:
                continue;
        }

        screen->params[i].is_modified = false;
        count++;
    }

    /* Sauvegarder en flash */
    save_params_to_service(screen);

    screen->state = DISPLAY_STATE_IDLE;

    if (count > 0) {
        update_status_message(screen, "Parametres appliques");
    } else {
        update_status_message(screen, "Aucune modification");
    }

    DEBUG_INFO(TAG, "%d paramètres appliqués", count);
    return count;
}

/**
 * @brief Restaure les valeurs par défaut
 */
void ScreenSettingsDisplay_RestoreDefaults(ScreenSettingsDisplay_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Restauration des valeurs par défaut affichage");

    for (int i = 0; i < DISPLAY_PARAM_COUNT; i++) {
        screen->params[i].value.brightness = screen->params[i].default_value;
        screen->params[i].is_modified = true;
    }

    /* Cas spéciaux */
    screen->params[DISPLAY_PARAM_BRIGHTNESS].value.brightness = 70;
    screen->params[DISPLAY_PARAM_TIMEOUT].value.timeout_sec = 30;
    screen->params[DISPLAY_PARAM_ROTATION].value.rotation = DISPLAY_ROTATION_PORTRAIT;
    screen->params[DISPLAY_PARAM_THEME].value.theme = DISPLAY_THEME_DARK;
    screen->params[DISPLAY_PARAM_FONT_SIZE].value.font_size = DISPLAY_FONT_NORMAL;
    screen->params[DISPLAY_PARAM_POWER_SAVING].value.boolean = false;
    screen->params[DISPLAY_PARAM_ANIMATIONS].value.boolean = true;

    ScreenSettingsDisplay_ApplyParams(screen);

    if (screen->base.is_visible) {
        /* Mettre à jour widgets */
        UISlider_SetValue(screen->brightness_slider, 70);
        UIRadioGroup_SetSelected(screen->timeout_radio, 1);  /* 30s */
        UIRadioGroup_SetSelected(screen->rotation_radio, DISPLAY_ROTATION_PORTRAIT);
        UIRadioGroup_SetSelected(screen->theme_radio, DISPLAY_THEME_DARK);
        UIRadioGroup_SetSelected(screen->font_size_radio, DISPLAY_FONT_NORMAL);
        UISwitch_SetState(screen->power_saving_switch, false);
        UISwitch_SetState(screen->animations_switch, true);

        /* Redessiner */
        ScreenSettingsDisplay_Show(screen);
        update_status_message(screen, "Valeurs par defaut restaurees");
    }
}

/**
 * @brief Définit la luminosité et l'applique immédiatement
 */
void ScreenSettingsDisplay_SetBrightness(ScreenSettingsDisplay_t* screen,
                                         uint8_t brightness)
{
    if (!screen) return;

    brightness = CLAMP(brightness, BRIGHTNESS_MIN_PERCENT, BRIGHTNESS_MAX_PERCENT);
    screen->params[DISPLAY_PARAM_BRIGHTNESS].value.brightness = brightness;
    screen->params[DISPLAY_PARAM_BRIGHTNESS].is_modified = true;

    /* Appliquer immédiatement */
    apply_brightness(screen);

    if (screen->base.is_visible) {
        UISlider_SetValue(screen->brightness_slider, brightness);
        update_brightness_display(screen);
    }

    DEBUG_VERBOSE(TAG, "Luminosité: %d%%", brightness);
}

/**
 * @brief Applique un preset de luminosité
 */
void ScreenSettingsDisplay_ApplyBrightnessPreset(ScreenSettingsDisplay_t* screen,
                                                  BrightnessPreset_t preset)
{
    if (!screen || preset >= BRIGHTNESS_PRESET_COUNT) return;

    uint8_t value = BRIGHTNESS_PRESET_PERCENT[preset];
    ScreenSettingsDisplay_SetBrightness(screen, value);

    update_status_message(screen, "Luminosite predefinie appliquee");
}

/**
 * @brief Définit le timeout d'extinction
 */
void ScreenSettingsDisplay_SetScreenTimeout(ScreenSettingsDisplay_t* screen,
                                            uint16_t timeout_sec)
{
    if (!screen) return;

    screen->params[DISPLAY_PARAM_SCREEN_TIMEOUT].value.timeout_sec = timeout_sec;
    screen->params[DISPLAY_PARAM_SCREEN_TIMEOUT].is_modified = true;

    /* Trouver l'index correspondant dans les options */
    for (uint8_t i = 0; i < TIMEOUT_OPTIONS_COUNT; i++) {
        if (TIMEOUT_VALUES[i] == timeout_sec) {
            UIRadioGroup_SetSelected(screen->timeout_radio, i);
            break;
        }
    }
}

/**
 * @brief Définit la rotation de l'écran
 */
void ScreenSettingsDisplay_SetRotation(ScreenSettingsDisplay_t* screen,
                                       DisplayRotation_t rotation)
{
    if (!screen || rotation >= DISPLAY_ROTATION_COUNT) return;

    screen->params[DISPLAY_PARAM_ROTATION].value.rotation = (uint8_t)rotation;
    screen->params[DISPLAY_PARAM_ROTATION].is_modified = true;

    if (screen->base.is_visible) {
        UIRadioGroup_SetSelected(screen->rotation_radio, rotation);
        UIRadioGroup_Draw(screen->rotation_radio);
    }
}

/**
 * @brief Définit le thème de couleurs
 */
void ScreenSettingsDisplay_SetTheme(ScreenSettingsDisplay_t* screen,
                                    DisplayTheme_t theme)
{
    if (!screen || theme >= DISPLAY_THEME_COUNT) return;

    screen->params[DISPLAY_PARAM_THEME].value.theme = (uint8_t)theme;
    screen->params[DISPLAY_PARAM_THEME].is_modified = true;

    if (screen->base.is_visible) {
        UIRadioGroup_SetSelected(screen->theme_radio, theme);
        UIRadioGroup_Draw(screen->theme_radio);
        update_theme_preview(screen);
    }
}

/**
 * @brief Définit la taille de police
 */
void ScreenSettingsDisplay_SetFontSize(ScreenSettingsDisplay_t* screen,
                                       DisplayFontSize_t font_size)
{
    if (!screen || font_size >= DISPLAY_FONT_COUNT) return;

    screen->params[DISPLAY_PARAM_FONT_SIZE].value.font_size = (uint8_t)font_size;
    screen->params[DISPLAY_PARAM_FONT_SIZE].is_modified = true;

    if (screen->base.is_visible) {
        UIRadioGroup_SetSelected(screen->font_size_radio, font_size);
        UIRadioGroup_Draw(screen->font_size_radio);
        update_font_demo(screen);
    }
}

/**
 * @brief Active/désactive l'économie d'énergie
 */
void ScreenSettingsDisplay_SetPowerSaving(ScreenSettingsDisplay_t* screen,
                                          bool enabled)
{
    if (!screen) return;

    screen->params[DISPLAY_PARAM_POWER_SAVING].value.boolean = enabled;
    screen->params[DISPLAY_PARAM_POWER_SAVING].is_modified = true;

    if (screen->base.is_visible) {
        UISwitch_SetState(screen->power_saving_switch, enabled);
        UISwitch_Draw(screen->power_saving_switch);

        /* Afficher/masquer le slider de seuil */
        if (enabled) {
            UISlider_Draw(screen->low_battery_slider);
            UILabel_Draw(screen->low_battery_label);
        }
    }
}

/**
 * @brief Définit le seuil de batterie faible
 */
void ScreenSettingsDisplay_SetLowBatteryThreshold(ScreenSettingsDisplay_t* screen,
                                                   uint8_t threshold)
{
    if (!screen) return;

    threshold = CLAMP(threshold, 5, 50);
    /* Stocké dans le même paramètre que power_saving */
    /* Utiliser le service directement */
    SettingsService_SetLowBatteryThreshold(screen->settings_service, threshold);
    SettingsService_Save(screen->settings_service);

    if (screen->base.is_visible) {
        UISlider_SetValue(screen->low_battery_slider, threshold);
        char buf[32];
        snprintf(buf, sizeof(buf), "Seuil batterie: %d%%", threshold);
        UILabel_SetText(screen->low_battery_label, buf);
        UILabel_Draw(screen->low_battery_label);
    }
}

/**
 * @brief Active/désactive les animations
 */
void ScreenSettingsDisplay_SetAnimations(ScreenSettingsDisplay_t* screen,
                                         bool enabled)
{
    if (!screen) return;

    screen->params[DISPLAY_PARAM_ANIMATIONS].value.boolean = enabled;
    screen->params[DISPLAY_PARAM_ANIMATIONS].is_modified = true;

    if (screen->base.is_visible) {
        UISwitch_SetState(screen->animations_switch, enabled);
        UISwitch_Draw(screen->animations_switch);
    }
}

/**
 * @brief Affiche un aperçu temporaire du thème
 */
void ScreenSettingsDisplay_PreviewTheme(ScreenSettingsDisplay_t* screen,
                                        DisplayTheme_t theme)
{
    if (!screen || theme >= DISPLAY_THEME_COUNT) return;

    /* Sauvegarder le thème actuel */
    screen->saved_theme = (DisplayTheme_t)screen->params[DISPLAY_PARAM_THEME].value.theme;

    /* Appliquer temporairement */
    screen->params[DISPLAY_PARAM_THEME].value.theme = (uint8_t)theme;
    apply_theme(screen);

    /* Lancer le timer de retour */
    Timer_Start(screen->theme_preview_timer);

    update_status_message(screen, "Apercu du theme - Retour automatique");
}

/**
 * @brief Libère les ressources
 */
void ScreenSettingsDisplay_Deinit(ScreenSettingsDisplay_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Libération des ressources affichage");

    Timer_Delete(screen->brightness_preview_timer);
    Timer_Delete(screen->theme_preview_timer);

    UILabel_Destroy(screen->title_label);
    UILabel_Destroy(screen->status_label);
    UIButton_Destroy(screen->back_button);
    UIButton_Destroy(screen->apply_button);
    UIButton_Destroy(screen->defaults_button);

    UISlider_Destroy(screen->brightness_slider);
    UISlider_Destroy(screen->low_battery_slider);
    UILabel_Destroy(screen->low_battery_label);

    for (int i = 0; i < BRIGHTNESS_PRESET_COUNT; i++) {
        UIButton_Destroy(screen->brightness_presets[i]);
    }

    UIRadioGroup_Destroy(screen->timeout_radio);
    UIRadioGroup_Destroy(screen->rotation_radio);
    UIRadioGroup_Destroy(screen->theme_radio);
    UIRadioGroup_Destroy(screen->font_size_radio);

    UISwitch_Destroy(screen->power_saving_switch);
    UISwitch_Destroy(screen->animations_switch);

    if (screen->theme_preview) {
        UIColorPreview_Destroy(screen->theme_preview);
    }
    UILabel_Destroy(screen->font_demo_label);

    UIDialog_Destroy(screen->confirm_dialog);

    for (int i = 0; i < DISPLAY_PARAM_COUNT; i++) {
        UILabel_Destroy(screen->param_labels[i]);
        UILabel_Destroy(screen->value_labels[i]);
    }

    memset(screen, 0, sizeof(ScreenSettingsDisplay_t));
}

/* ======================================================================== */
/*              CONVERSIONS PWM                                             */
/* ======================================================================== */

uint16_t DisplaySettings_BrightnessToPWM(uint8_t percentage)
{
    percentage = CLAMP(percentage, BRIGHTNESS_MIN_PERCENT, BRIGHTNESS_MAX_PERCENT);
    /* PWM = PWM_MIN + (percentage - 10) * (PWM_MAX - PWM_MIN) / 90 */
    return PWM_MIN + ((uint16_t)(percentage - BRIGHTNESS_MIN_PERCENT) * (PWM_MAX - PWM_MIN)) / 90;
}

uint8_t DisplaySettings_PWMToBrightness(uint16_t pwm_value)
{
    if (pwm_value <= PWM_MIN) return BRIGHTNESS_MIN_PERCENT;
    if (pwm_value >= PWM_MAX) return BRIGHTNESS_MAX_PERCENT;
    /* % = 10 + (pwm - PWM_MIN) * 90 / (PWM_MAX - PWM_MIN) */
    return BRIGHTNESS_MIN_PERCENT + ((pwm_value - PWM_MIN) * 90) / (PWM_MAX - PWM_MIN);
}

uint8_t DisplaySettings_GetPresetValue(BrightnessPreset_t preset)
{
    if (preset >= BRIGHTNESS_PRESET_COUNT) return 70;
    return BRIGHTNESS_PRESET_PERCENT[preset];
}

/* ======================================================================== */
/*              INITIALISATION DES PARAMÈTRES                               */
/* ======================================================================== */

static void init_params(ScreenSettingsDisplay_t* screen)
{
    if (!screen) return;

    /* Paramètre 0 : Luminosité */
    screen->params[0].id = DISPLAY_PARAM_BRIGHTNESS;
    screen->params[0].name = "Luminosite";
    screen->params[0].unit = "%";
    screen->params[0].min_value = BRIGHTNESS_MIN_PERCENT;
    screen->params[0].max_value = BRIGHTNESS_MAX_PERCENT;
    screen->params[0].default_value = 70;

    /* Paramètre 1 : Timeout écran */
    screen->params[1].id = DISPLAY_PARAM_SCREEN_TIMEOUT;
    screen->params[1].name = "Timeout ecran";
    screen->params[1].unit = "sec";
    screen->params[1].options = TIMEOUT_OPTIONS;
    screen->params[1].option_count = TIMEOUT_OPTIONS_COUNT;
    screen->params[1].default_value = 2;  /* Index 2 = 30s */

    /* Paramètre 2 : Rotation */
    screen->params[2].id = DISPLAY_PARAM_ROTATION;
    screen->params[2].name = "Rotation";
    screen->params[2].unit = "";
    screen->params[2].options = ROTATION_OPTIONS;
    screen->params[2].option_count = DISPLAY_ROTATION_COUNT;
    screen->params[2].default_value = DISPLAY_ROTATION_PORTRAIT;

    /* Paramètre 3 : Thème */
    screen->params[3].id = DISPLAY_PARAM_THEME;
    screen->params[3].name = "Theme";
    screen->params[3].unit = "";
    screen->params[3].options = THEME_OPTIONS;
    screen->params[3].option_count = DISPLAY_THEME_COUNT;
    screen->params[3].default_value = DISPLAY_THEME_DARK;

    /* Paramètre 4 : Taille police */
    screen->params[4].id = DISPLAY_PARAM_FONT_SIZE;
    screen->params[4].name = "Police";
    screen->params[4].unit = "";
    screen->params[4].options = FONT_SIZE_OPTIONS;
    screen->params[4].option_count = DISPLAY_FONT_COUNT;
    screen->params[4].default_value = DISPLAY_FONT_NORMAL;

    /* Paramètre 5 : Économie énergie */
    screen->params[5].id = DISPLAY_PARAM_POWER_SAVING;
    screen->params[5].name = "Eco energie";
    screen->params[5].unit = "";
    screen->params[5].default_value = 0;  /* OFF */

    /* Paramètre 6 : Animations */
    screen->params[6].id = DISPLAY_PARAM_ANIMATIONS;
    screen->params[6].name = "Animations";
    screen->params[6].unit = "";
    screen->params[6].default_value = 1;  /* ON */

    /* Flags */
    for (int i = 0; i < DISPLAY_PARAM_COUNT; i++) {
        screen->params[i].is_modified = false;
        screen->params[i].widget = NULL;
    }
}

static void load_params_from_service(ScreenSettingsDisplay_t* screen)
{
    if (!screen || !screen->settings_service) return;

    SettingsService_t* svc = screen->settings_service;

    uint8_t val8;
    uint16_t val16;
    bool flag;

    if (SettingsService_GetBrightness(svc, &val8))
        screen->params[DISPLAY_PARAM_BRIGHTNESS].value.brightness = val8;
    else
        screen->params[DISPLAY_PARAM_BRIGHTNESS].value.brightness = 70;

    if (SettingsService_GetScreenTimeout(svc, &val16))
        screen->params[DISPLAY_PARAM_SCREEN_TIMEOUT].value.timeout_sec = val16;
    else
        screen->params[DISPLAY_PARAM_SCREEN_TIMEOUT].value.timeout_sec = 30;

    if (SettingsService_GetRotation(svc, &val8))
        screen->params[DISPLAY_PARAM_ROTATION].value.rotation = val8;
    else
        screen->params[DISPLAY_PARAM_ROTATION].value.rotation = DISPLAY_ROTATION_PORTRAIT;

    if (SettingsService_GetTheme(svc, &val8))
        screen->params[DISPLAY_PARAM_THEME].value.theme = val8;
    else
        screen->params[DISPLAY_PARAM_THEME].value.theme = DISPLAY_THEME_DARK;

    if (SettingsService_GetFontSize(svc, &val8))
        screen->params[DISPLAY_PARAM_FONT_SIZE].value.font_size = val8;
    else
        screen->params[DISPLAY_PARAM_FONT_SIZE].value.font_size = DISPLAY_FONT_NORMAL;

    if (SettingsService_GetPowerSaving(svc, &flag))
        screen->params[DISPLAY_PARAM_POWER_SAVING].value.boolean = flag;
    else
        screen->params[DISPLAY_PARAM_POWER_SAVING].value.boolean = false;

    if (SettingsService_GetAnimations(svc, &flag))
        screen->params[DISPLAY_PARAM_ANIMATIONS].value.boolean = flag;
    else
        screen->params[DISPLAY_PARAM_ANIMATIONS].value.boolean = true;

    /* Mettre à jour les widgets */
    if (screen->brightness_slider)
        UISlider_SetValue(screen->brightness_slider, screen->params[DISPLAY_PARAM_BRIGHTNESS].value.brightness);

    /* Trouver l'index du timeout */
    for (uint8_t i = 0; i < TIMEOUT_OPTIONS_COUNT; i++) {
        if (TIMEOUT_VALUES[i] == screen->params[DISPLAY_PARAM_SCREEN_TIMEOUT].value.timeout_sec) {
            if (screen->timeout_radio) UIRadioGroup_SetSelected(screen->timeout_radio, i);
            break;
        }
    }

    if (screen->rotation_radio)
        UIRadioGroup_SetSelected(screen->rotation_radio, screen->params[DISPLAY_PARAM_ROTATION].value.rotation);
    if (screen->theme_radio)
        UIRadioGroup_SetSelected(screen->theme_radio, screen->params[DISPLAY_PARAM_THEME].value.theme);
    if (screen->font_size_radio)
        UIRadioGroup_SetSelected(screen->font_size_radio, screen->params[DISPLAY_PARAM_FONT_SIZE].value.font_size);
    if (screen->power_saving_switch)
        UISwitch_SetState(screen->power_saving_switch, screen->params[DISPLAY_PARAM_POWER_SAVING].value.boolean);
    if (screen->animations_switch)
        UISwitch_SetState(screen->animations_switch, screen->params[DISPLAY_PARAM_ANIMATIONS].value.boolean);

    /* Charger le seuil batterie */
    uint8_t threshold = 15;
    SettingsService_GetLowBatteryThreshold(svc, &threshold);
    if (screen->low_battery_slider)
        UISlider_SetValue(screen->low_battery_slider, threshold);
}

static void save_params_to_service(ScreenSettingsDisplay_t* screen)
{
    if (!screen || !screen->settings_service) return;

    SettingsService_t* svc = screen->settings_service;

    SettingsService_SetBrightness(svc, screen->params[DISPLAY_PARAM_BRIGHTNESS].value.brightness);
    SettingsService_SetScreenTimeout(svc, screen->params[DISPLAY_PARAM_SCREEN_TIMEOUT].value.timeout_sec);
    SettingsService_SetRotation(svc, screen->params[DISPLAY_PARAM_ROTATION].value.rotation);
    SettingsService_SetTheme(svc, screen->params[DISPLAY_PARAM_THEME].value.theme);
    SettingsService_SetFontSize(svc, screen->params[DISPLAY_PARAM_FONT_SIZE].value.font_size);
    SettingsService_SetPowerSaving(svc, screen->params[DISPLAY_PARAM_POWER_SAVING].value.boolean);
    SettingsService_SetAnimations(svc, screen->params[DISPLAY_PARAM_ANIMATIONS].value.boolean);

    SettingsService_Save(svc);
}

/* ======================================================================== */
/*              CRÉATION DES WIDGETS                                        */
/* ======================================================================== */

static void create_brightness_widgets(ScreenSettingsDisplay_t* screen)
{
    /* Slider */
    screen->brightness_slider = UISlider_Create();
    UISlider_SetPosition(screen->brightness_slider, SLIDER_X, PARAM_BRIGHTNESS_Y + 18);
    UISlider_SetSize(screen->brightness_slider, SLIDER_WIDTH, SLIDER_HEIGHT);
    UISlider_SetRange(screen->brightness_slider, BRIGHTNESS_MIN_PERCENT, BRIGHTNESS_MAX_PERCENT);
    UISlider_SetOnChanged(screen->brightness_slider, on_brightness_changed, screen);
    UISlider_SetColor(screen->brightness_slider, THEME_WARNING);

    /* Label valeur */
    screen->value_labels[DISPLAY_PARAM_BRIGHTNESS] = UILabel_Create();
    UILabel_SetFont(screen->value_labels[DISPLAY_PARAM_BRIGHTNESS], &font_medium_bold);
    UILabel_SetColor(screen->value_labels[DISPLAY_PARAM_BRIGHTNESS], THEME_TEXT_PRIMARY);
    UILabel_SetPosition(screen->value_labels[DISPLAY_PARAM_BRIGHTNESS],
                        SLIDER_X + SLIDER_WIDTH + 8, PARAM_BRIGHTNESS_Y + 16);

    /* Presets */
    const char* preset_labels[] = {"☀️", "🌤️", "⛅", "🌥️", "☀️+"};
    uint16_t preset_x = CONTENT_X_MARGIN + 5;
    for (int i = 0; i < BRIGHTNESS_PRESET_COUNT; i++) {
        screen->brightness_presets[i] = UIButton_Create();
        UIButton_SetText(screen->brightness_presets[i], BRIGHTNESS_PRESET_LABELS[i]);
        UIButton_SetFont(screen->brightness_presets[i], &font_small);
        UIButton_SetSize(screen->brightness_presets[i], PRESET_BUTTON_WIDTH, PRESET_BUTTON_HEIGHT);
        UIButton_SetPosition(screen->brightness_presets[i], preset_x,
                             PARAM_BRIGHTNESS_Y + PRESET_BUTTONS_Y_OFFSET);
        UIButton_SetOnClick(screen->brightness_presets[i], on_brightness_preset_clicked, screen);
        UIButton_SetUserData(screen->brightness_presets[i], (void*)(uintptr_t)i);
        UIButton_SetCornerRadius(screen->brightness_presets[i], 5);

        /* Colorer selon le preset */
        uint16_t preset_color;
        switch (i) {
            case BRIGHTNESS_PRESET_MIN:  preset_color = 0x4208; break;  /* Très sombre */
            case BRIGHTNESS_PRESET_LOW:  preset_color = 0x6B4D; break;  /* Sombre */
            case BRIGHTNESS_PRESET_MEDIUM: preset_color = 0x9CD3; break; /* Moyen */
            case BRIGHTNESS_PRESET_HIGH: preset_color = 0xC618; break;  /* Clair */
            case BRIGHTNESS_PRESET_MAX:  preset_color = 0xFFE0; break;  /* Très clair */
            default: preset_color = THEME_BUTTON_NEUTRAL; break;
        }
        UIButton_SetColor(screen->brightness_presets[i], preset_color);

        preset_x += PRESET_BUTTON_WIDTH + PRESET_BUTTON_SPACING;
    }
}

static void create_timeout_widgets(ScreenSettingsDisplay_t* screen)
{
    screen->timeout_radio = UIRadioGroup_Create();
    UIRadioGroup_SetPosition(screen->timeout_radio, CONTENT_X_MARGIN + 5, PARAM_TIMEOUT_Y + 20);
    UIRadioGroup_SetOptions(screen->timeout_radio, TIMEOUT_OPTIONS, TIMEOUT_OPTIONS_COUNT);
    UIRadioGroup_SetOrientation(screen->timeout_radio, RADIO_HORIZONTAL);
    UIRadioGroup_SetSpacing(screen->timeout_radio, 4);
    UIRadioGroup_SetOnSelected(screen->timeout_radio, on_timeout_selected, screen);
}

static void create_rotation_widgets(ScreenSettingsDisplay_t* screen)
{
    screen->rotation_radio = UIRadioGroup_Create();
    UIRadioGroup_SetPosition(screen->rotation_radio, CONTENT_X_MARGIN + 5, PARAM_ROTATION_Y + 20);
    UIRadioGroup_SetOptions(screen->rotation_radio, ROTATION_OPTIONS, DISPLAY_ROTATION_COUNT);
    UIRadioGroup_SetOrientation(screen->rotation_radio, RADIO_HORIZONTAL);
    UIRadioGroup_SetSpacing(screen->rotation_radio, 4);
    UIRadioGroup_SetOnSelected(screen->rotation_radio, on_rotation_selected, screen);
}

static void create_theme_widgets(ScreenSettingsDisplay_t* screen)
{
    screen->theme_radio = UIRadioGroup_Create();
    UIRadioGroup_SetPosition(screen->theme_radio, CONTENT_X_MARGIN + 5, PARAM_THEME_Y + 20);
    UIRadioGroup_SetOptions(screen->theme_radio, THEME_OPTIONS, DISPLAY_THEME_COUNT);
    UIRadioGroup_SetOrientation(screen->theme_radio, RADIO_HORIZONTAL);
    UIRadioGroup_SetSpacing(screen->theme_radio, 8);
    UIRadioGroup_SetOnSelected(screen->theme_radio, on_theme_selected, screen);

    /* Aperçu couleurs */
    screen->theme_preview = UIColorPreview_Create();
    UIColorPreview_SetPosition(screen->theme_preview, CONTENT_X_MARGIN + 10, PARAM_THEME_Y + 50);
    UIColorPreview_SetSize(screen->theme_preview, CONTENT_WIDTH - 20, 24);
    UIColorPreview_SetColors(screen->theme_preview,
                             THEME_PREVIEW_COLORS[DISPLAY_THEME_DARK], 5);
}

static void create_font_size_widgets(ScreenSettingsDisplay_t* screen)
{
    screen->font_size_radio = UIRadioGroup_Create();
    UIRadioGroup_SetPosition(screen->font_size_radio, CONTENT_X_MARGIN + 5, PARAM_FONT_Y + 20);
    UIRadioGroup_SetOptions(screen->font_size_radio, FONT_SIZE_OPTIONS, DISPLAY_FONT_COUNT);
    UIRadioGroup_SetOrientation(screen->font_size_radio, RADIO_HORIZONTAL);
    UIRadioGroup_SetSpacing(screen->font_size_radio, 10);
    UIRadioGroup_SetOnSelected(screen->font_size_radio, on_font_size_selected, screen);

    /* Texte de démonstration */
    screen->font_demo_label = UILabel_Create();
    UILabel_SetText(screen->font_demo_label, FONT_DEMO_TEXT);
    UILabel_SetFont(screen->font_demo_label, &font_medium);
    UILabel_SetColor(screen->font_demo_label, THEME_ACCENT);
    UILabel_SetPosition(screen->font_demo_label, CONTENT_X_MARGIN + 10, PARAM_FONT_Y + 48);
}

static void create_power_saving_widgets(ScreenSettingsDisplay_t* screen)
{
    screen->power_saving_switch = UISwitch_Create();
    UISwitch_SetPosition(screen->power_saving_switch, SLIDER_X, PARAM_POWER_SAVING_Y + 16);
    UISwitch_SetOnChanged(screen->power_saving_switch, on_power_saving_changed, screen);
    UISwitch_SetOnColor(screen->power_saving_switch, THEME_SUCCESS);

    /* Slider seuil batterie */
    screen->low_battery_slider = UISlider_Create();
    UISlider_SetPosition(screen->low_battery_slider, SLIDER_X, PARAM_POWER_SAVING_Y + 42);
    UISlider_SetSize(screen->low_battery_slider, SLIDER_WIDTH, SLIDER_HEIGHT);
    UISlider_SetRange(screen->low_battery_slider, 5, 50);
    UISlider_SetOnChanged(screen->low_battery_slider, on_low_battery_changed, screen);
    UISlider_SetColor(screen->low_battery_slider, THEME_WARNING);

    screen->low_battery_label = UILabel_Create();
    UILabel_SetFont(screen->low_battery_label, &font_small);
    UILabel_SetColor(screen->low_battery_label, THEME_TEXT_SECONDARY);
    UILabel_SetPosition(screen->low_battery_label, SLIDER_X, PARAM_POWER_SAVING_Y + 62);
}

static void create_animations_widgets(ScreenSettingsDisplay_t* screen)
{
    screen->animations_switch = UISwitch_Create();
    UISwitch_SetPosition(screen->animations_switch, SLIDER_X, PARAM_ANIMATIONS_Y + 16);
    UISwitch_SetOnChanged(screen->animations_switch, on_animations_changed, screen);
}

/* ======================================================================== */
/*              CALLBACKS WIDGETS                                           */
/* ======================================================================== */

static void on_brightness_changed(void* context, int16_t value)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen) return;

    ScreenSettingsDisplay_SetBrightness(screen, (uint8_t)value);
}

static void on_brightness_preset_clicked(void* context)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen) return;

    /* Récupérer l'index du preset depuis le user data */
    /* (Le callback UIButton devrait fournir le contexte + user data) */
    /* Pour l'exemple, on utilise le premier preset */
    /* TODO: Améliorer la récupération de l'index */
}

static void on_timeout_selected(void* context, uint8_t index)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen || index >= TIMEOUT_OPTIONS_COUNT) return;

    screen->params[DISPLAY_PARAM_SCREEN_TIMEOUT].value.timeout_sec = TIMEOUT_VALUES[index];
    screen->params[DISPLAY_PARAM_SCREEN_TIMEOUT].is_modified = true;

    DEBUG_VERBOSE(TAG, "Timeout écran: %s", TIMEOUT_OPTIONS[index]);
}

static void on_rotation_selected(void* context, uint8_t index)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen || index >= DISPLAY_ROTATION_COUNT) return;

    screen->params[DISPLAY_PARAM_ROTATION].value.rotation = index;
    screen->params[DISPLAY_PARAM_ROTATION].is_modified = true;

    /* Appliquer immédiatement pour aperçu */
    apply_rotation(screen);

    DEBUG_VERBOSE(TAG, "Rotation: %s", ROTATION_OPTIONS[index]);
}

static void on_theme_selected(void* context, uint8_t index)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen || index >= DISPLAY_THEME_COUNT) return;

    /* Aperçu temporaire du thème */
    ScreenSettingsDisplay_PreviewTheme(screen, (DisplayTheme_t)index);

    DEBUG_VERBOSE(TAG, "Thème: %s", THEME_OPTIONS[index]);
}

static void on_font_size_selected(void* context, uint8_t index)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen || index >= DISPLAY_FONT_COUNT) return;

    screen->params[DISPLAY_PARAM_FONT_SIZE].value.font_size = index;
    screen->params[DISPLAY_PARAM_FONT_SIZE].is_modified = true;

    /* Mettre à jour le texte de démo immédiatement */
    update_font_demo(screen);

    DEBUG_VERBOSE(TAG, "Police: %s", FONT_SIZE_OPTIONS[index]);
}

static void on_power_saving_changed(void* context, bool enabled)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen) return;

    ScreenSettingsDisplay_SetPowerSaving(screen, enabled);

    DEBUG_VERBOSE(TAG, "Économie énergie: %s", enabled ? "ON" : "OFF");
}

static void on_low_battery_changed(void* context, int16_t value)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen) return;

    ScreenSettingsDisplay_SetLowBatteryThreshold(screen, (uint8_t)value);

    DEBUG_VERBOSE(TAG, "Seuil batterie: %d%%", value);
}

static void on_animations_changed(void* context, bool enabled)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen) return;

    screen->params[DISPLAY_PARAM_ANIMATIONS].value.boolean = enabled;
    screen->params[DISPLAY_PARAM_ANIMATIONS].is_modified = true;

    DEBUG_VERBOSE(TAG, "Animations: %s", enabled ? "ON" : "OFF");
}

static void on_apply_clicked(void* context)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen) return;

    bool has_modifications = false;
    for (int i = 0; i < DISPLAY_PARAM_COUNT; i++) {
        if (screen->params[i].is_modified) {
            has_modifications = true;
            break;
        }
    }

    if (!has_modifications) {
        update_status_message(screen, "Aucune modification a appliquer");
        return;
    }

    ScreenSettingsDisplay_ApplyParams(screen);
}

static void on_defaults_clicked(void* context)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen) return;

    UIDialog_SetTitle(screen->confirm_dialog, "Valeurs par defaut");
    UIDialog_SetMessage(screen->confirm_dialog,
                        "Remettre tous les parametres\n"
                        "d'affichage aux valeurs d'usine ?\n\n"
                        "Luminosite : 70%%\n"
                        "Timeout    : 30s\n"
                        "Theme      : Sombre\n"
                        "Police     : Normale");
    UIDialog_SetOnResult(screen->confirm_dialog,
                        (void(*)(void*, bool))on_defaults_confirmed,
                        screen);
    UIDialog_SetVisible(screen->confirm_dialog, true);

    if (screen->base.is_visible) {
        UIDialog_Draw(screen->confirm_dialog);
    }
}

static void on_defaults_confirmed(void* context, bool confirmed)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen) return;

    UIDialog_SetVisible(screen->confirm_dialog, false);

    if (screen->base.is_visible) {
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
        ScreenSettingsDisplay_Show(screen);
    }

    if (confirmed) {
        ScreenSettingsDisplay_RestoreDefaults(screen);
    }
}

static void on_back_clicked(void* context)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)context;
    if (!screen) return;

    /* Restaurer le thème précédent si aperçu en cours */
    if (screen->state == DISPLAY_STATE_PREVIEW_THEME) {
        screen->params[DISPLAY_PARAM_THEME].value.theme = (uint8_t)screen->saved_theme;
        apply_theme(screen);
    }

    if (screen->on_back_pressed) {
        screen->on_back_pressed();
    } else {
        UINavigation_GoBack();
    }
}

/* ======================================================================== */
/*              APPLICATION AU MATÉRIEL                                     */
/* ======================================================================== */

static void apply_brightness(ScreenSettingsDisplay_t* screen)
{
    if (!screen || !screen->backlight) return;

    uint8_t percent = screen->params[DISPLAY_PARAM_BRIGHTNESS].value.brightness;
    uint16_t pwm = DisplaySettings_BrightnessToPWM(percent);

    BacklightControl_SetPWM(screen->backlight, pwm);

    DEBUG_VERBOSE(TAG, "Luminosité appliquée: %d%% (PWM=%d)", percent, pwm);
}

static void apply_screen_timeout(ScreenSettingsDisplay_t* screen)
{
    if (!screen) return;

    uint16_t timeout = screen->params[DISPLAY_PARAM_SCREEN_TIMEOUT].value.timeout_sec;

    /* Configurer le PowerManager */
    PowerManager_SetScreenTimeout(timeout);

    DEBUG_VERBOSE(TAG, "Timeout appliqué: %d secondes", timeout);
}

static void apply_rotation(ScreenSettingsDisplay_t* screen)
{
    if (!screen || !screen->display_manager) return;

    DisplayRotation_t rot = (DisplayRotation_t)screen->params[DISPLAY_PARAM_ROTATION].value.rotation;

    /* Reconfigurer le LTDC pour la rotation */
    DisplayManager_SetRotation(screen->display_manager, rot);

    DEBUG_VERBOSE(TAG, "Rotation appliquée: %s", ROTATION_OPTIONS[rot]);

    /* Forcer un réaffichage complet */
    Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
    ScreenSettingsDisplay_Show(screen);
}

static void apply_theme(ScreenSettingsDisplay_t* screen)
{
    if (!screen) return;

    DisplayTheme_t theme = (DisplayTheme_t)screen->params[DISPLAY_PARAM_THEME].value.theme;

    /* Mettre à jour les variables globales du thème */
    const uint16_t* colors = THEME_PREVIEW_COLORS[theme];
    UITheme_Apply(colors[0], colors[1], colors[2], colors[3], colors[4]);

    DEBUG_VERBOSE(TAG, "Thème appliqué: %s", THEME_OPTIONS[theme]);

    /* Redessiner si visible */
    if (screen->base.is_visible) {
        update_theme_preview(screen);
    }
}

static void apply_font_size(ScreenSettingsDisplay_t* screen)
{
    if (!screen) return;

    DisplayFontSize_t size = (DisplayFontSize_t)screen->params[DISPLAY_PARAM_FONT_SIZE].value.font_size;

    /* Mettre à jour la police globale */
    UITheme_SetDefaultFont(FONT_SIZE_MAP[size]);

    DEBUG_VERBOSE(TAG, "Police appliquée: %s", FONT_SIZE_OPTIONS[size]);
}

static void apply_power_saving(ScreenSettingsDisplay_t* screen)
{
    if (!screen) return;

    bool enabled = screen->params[DISPLAY_PARAM_POWER_SAVING].value.boolean;

    /* Configurer le PowerManager */
    PowerManager_SetPowerSaving(enabled);

    /* Si activé, appliquer le seuil */
    if (enabled) {
        uint8_t threshold = 15;
        SettingsService_GetLowBatteryThreshold(screen->settings_service, &threshold);
        PowerManager_SetLowBatteryThreshold(threshold);
    }

    DEBUG_VERBOSE(TAG, "Économie énergie: %s", enabled ? "ON" : "OFF");
}

static void apply_animations(ScreenSettingsDisplay_t* screen)
{
    if (!screen) return;

    bool enabled = screen->params[DISPLAY_PARAM_ANIMATIONS].value.boolean;

    /* Activer/désactiver les animations globales */
    UIAnimations_SetEnabled(enabled);

    DEBUG_VERBOSE(TAG, "Animations: %s", enabled ? "ON" : "OFF");
}

/* ======================================================================== */
/*              RENDU                                                       */
/* ======================================================================== */

static void draw_param_row_background(int16_t y, bool selected)
{
    uint16_t bg = selected ? THEME_LIST_SELECTED : THEME_BG_MAIN;
    Display_FillRect(CONTENT_X_MARGIN, y, CONTENT_WIDTH, PARAM_ROW_HEIGHT - 2, bg);
}

static void draw_param_label(const char* label, int16_t y)
{
    Display_DrawText(CONTENT_X_MARGIN + 5, y + 4,
                     label, &font_small_bold,
                     THEME_TEXT_TERTIARY, THEME_BG_MAIN);
}

static void update_brightness_display(ScreenSettingsDisplay_t* screen)
{
    if (!screen) return;

    uint8_t brt = screen->params[DISPLAY_PARAM_BRIGHTNESS].value.brightness;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", brt);

    UILabel_SetText(screen->value_labels[DISPLAY_PARAM_BRIGHTNESS], buf);
    if (screen->base.is_visible) {
        UILabel_Draw(screen->value_labels[DISPLAY_PARAM_BRIGHTNESS]);
    }
}

static void update_font_demo(ScreenSettingsDisplay_t* screen)
{
    if (!screen || !screen->font_demo_label) return;

    DisplayFontSize_t size = (DisplayFontSize_t)screen->params[DISPLAY_PARAM_FONT_SIZE].value.font_size;
    UILabel_SetFont(screen->font_demo_label, FONT_SIZE_MAP[size]);

    if (screen->base.is_visible) {
        /* Effacer et redessiner */
        Display_FillRect(CONTENT_X_MARGIN, PARAM_FONT_Y + 44,
                         CONTENT_WIDTH, 30, THEME_BG_MAIN);
        UILabel_Draw(screen->font_demo_label);
    }
}

static void update_theme_preview(ScreenSettingsDisplay_t* screen)
{
    if (!screen || !screen->theme_preview) return;

    DisplayTheme_t theme = (DisplayTheme_t)screen->params[DISPLAY_PARAM_THEME].value.theme;
    UIColorPreview_SetColors(screen->theme_preview,
                             THEME_PREVIEW_COLORS[theme], 5);

    if (screen->base.is_visible) {
        UIColorPreview_Draw(screen->theme_preview);
    }
}

static void update_status_message(ScreenSettingsDisplay_t* screen, const char* msg)
{
    if (!screen) return;

    UILabel_SetText(screen->status_label, msg);

    if (screen->base.is_visible) {
        UILabel_Draw(screen->status_label);
    }

    /* Timer pour effacer le message */
    static TimerHandle_t status_timer = NULL;
    if (!status_timer) {
        status_timer = Timer_Create("DisplayStatus", STATUS_TIMEOUT_MS, false,
                                    status_timer_callback, screen);
    }
    Timer_Start(status_timer);
}

/* ======================================================================== */
/*              TIMERS                                                      */
/* ======================================================================== */

static void preview_timer_callback(TimerHandle_t timer)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)Timer_GetContext(timer);
    if (!screen) return;

    DEBUG_VERBOSE(TAG, "Fin de l'aperçu, retour au thème précédent");

    /* Restaurer le thème précédent */
    screen->params[DISPLAY_PARAM_THEME].value.theme = (uint8_t)screen->saved_theme;
    apply_theme(screen);

    /* Mettre à jour l'affichage */
    if (screen->base.is_visible) {
        UIRadioGroup_SetSelected(screen->theme_radio, screen->saved_theme);
        UIRadioGroup_Draw(screen->theme_radio);
        update_theme_preview(screen);
    }

    screen->state = DISPLAY_STATE_IDLE;
    update_status_message(screen, "Theme precedent restaure");
}

static void status_timer_callback(TimerHandle_t timer)
{
    ScreenSettingsDisplay_t* screen = (ScreenSettingsDisplay_t*)Timer_GetContext(timer);
    if (!screen) return;

    UILabel_SetText(screen->status_label, "");
    if (screen->base.is_visible) {
        UILabel_Draw(screen->status_label);
    }
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */