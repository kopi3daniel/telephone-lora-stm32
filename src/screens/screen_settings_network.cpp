/**
 * @file    screen_settings_network.cpp
 * @brief   Implémentation de l'écran des réglages réseau LoRa
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente la configuration complète de la radio LoRa SX1278.
 * 
 * ARCHITECTURE DE L'ÉCRAN :
 * ┌──────────────────────────────────────────────────────────┐
 * │  ← Réseau LoRa                                 12:45 ███ │ ← Barre statut
 * │──────────────────────────────────────────────────────────│
 * │                                                          │
 * │  FRÉQUENCE                                       ✓       │ ← Indicateur
 * │  [868.000] MHz          [-] [+]                          │ ← Slider + boutons
 * │  ───────────────────────●─────────────────────────       │
 * │                                                          │
 * │  PUISSANCE                                        ✓      │
 * │  [20 dBm]                                                │
 * │  ───────────────────────────────────────●──────────      │
 * │                                                          │
 * │  SPREADING FACTOR                                 ✓      │
 * │  ◉ SF7  ○ SF8  ○ SF9  ○ SF10  ○ SF11  ○ SF12          │ ← Radio buttons
 * │                                                          │
 * │  BANDE PASSANTE                                   ✓      │
 * │  ◉ 125 kHz  ○ 250 kHz  ○ 500 kHz                        │ ← Radio buttons
 * │                                                          │
 * │  CODING RATE                                      ✓      │
 * │  ◉ 4/5  ○ 4/6  ○ 4/7  ○ 4/8                            │ ← Radio buttons
 * │                                                          │
 * │  SYNC WORD                                        ✓      │
 * │  0x12                                    [Modifier]      │ ← Bouton édition
 * │                                                          │
 * │  ──── Infos ─────────────────────────────────────       │
 * │  Portée estimée : 3.2 km                                 │
 * │  Débit estimé   : 5470 bps                               │
 * │  Temps symbole  : 5.1 ms                                 │
 * │                                                          │
 * │──────────────────────────────────────────────────────────│
 * │  [Appliquer]              [Valeurs par défaut]            │
 * └──────────────────────────────────────────────────────────┘
 * 
 * FLUX DE MODIFICATION D'UN PARAMÈTRE :
 * 
 * 1. L'utilisateur modifie un slider ou sélectionne un bouton radio
 * 2. Le callback on_param_changed() est appelé
 * 3. La valeur est mise à jour dans params[x].value
 * 4. params[x].is_modified = true
 * 5. params[x].apply_status = APPLY_STATUS_PENDING
 * 6. Si mode "application immédiate" :
 *    → ScreenSettingsNetwork_ApplySingleParam() est appelé
 *    → La commande SPI est envoyée au SX1278
 *    → Le statut passe à SUCCESS ou FAILED
 * 7. Si mode "application différée" :
 *    → L'utilisateur doit appuyer sur [Appliquer]
 *    → ScreenSettingsNetwork_ApplyParams() applique tout
 * 
 * PARTICULARITÉS TECHNIQUES STM32F429 :
 * - SPI1 @ 10 MHz pour communication avec SX1278
 * - DMA2D pour rendu accéléré des sliders (dégradés)
 * - GPIO interrupts pour DIO0 (TxDone/RxDone)
 * - Timers pour debounce des boutons +/-
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "screen_settings_network.h"

/* UI */
#include "../ui/ui_core.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_widgets.h"
#include "../ui/ui_label.h"
#include "../ui/ui_button.h"
#include "../ui/ui_slider.h"
#include "../ui/ui_radio_group.h"
#include "../ui/ui_switch.h"
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
#include "../drivers/display/dma2d_driver.h"
#include "../drivers/lora/lora_driver.h"
#include "../drivers/lora/sx1278_defs.h"
#include "../drivers/lora/sx1278_hal.h"

/* Utilitaires */
#include "../utils/string_utils.h"
#include "../utils/timer_utils.h"
#include "../utils/debug_utils.h"
#include "../utils/math_utils.h"

/* Standard */
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs de debug */
#define TAG                                 "ScreenSettingsNetwork"

/** Dimensions de l'écran */
#define SCREEN_WIDTH                        320
#define SCREEN_HEIGHT                       480

/** Zone de contenu (sous la barre de titre) */
#define CONTENT_Y_START                     70
#define CONTENT_X_MARGIN                    15
#define CONTENT_WIDTH                       (SCREEN_WIDTH - 2 * CONTENT_X_MARGIN)

/** Hauteur d'une ligne de paramètre */
#define PARAM_ROW_HEIGHT                    62

/** Position Y de chaque paramètre */
#define PARAM_FREQ_Y                        (CONTENT_Y_START + 5)
#define PARAM_POWER_Y                       (PARAM_FREQ_Y + PARAM_ROW_HEIGHT)
#define PARAM_SF_Y                          (PARAM_POWER_Y + PARAM_ROW_HEIGHT)
#define PARAM_BW_Y                          (PARAM_SF_Y + PARAM_ROW_HEIGHT)
#define PARAM_CR_Y                          (PARAM_BW_Y + PARAM_ROW_HEIGHT)
#define PARAM_SYNC_Y                        (PARAM_CR_Y + PARAM_ROW_HEIGHT)

/** Zone d'infos (portée, débit) */
#define INFO_AREA_Y                         (PARAM_SYNC_Y + PARAM_ROW_HEIGHT)
#define INFO_AREA_HEIGHT                    70

/** Position des boutons du bas */
#define BOTTOM_BUTTONS_Y                    420
#define APPLY_BUTTON_X                      15
#define APPLY_BUTTON_WIDTH                  140
#define DEFAULTS_BUTTON_X                   165
#define DEFAULTS_BUTTON_WIDTH               140

/** Dimensions des sliders */
#define SLIDER_WIDTH                        200
#define SLIDER_HEIGHT                       20
#define SLIDER_X                            70

/** Valeurs min/max/défaut pour la fréquence */
#define FREQ_MIN_MHZ                        863.0f
#define FREQ_MAX_MHZ                        870.0f
#define FREQ_DEFAULT_MHZ                    868.0f
#define FREQ_STEP_MHZ                       0.1f
#define FREQ_SLIDER_STEPS                   ((int)((FREQ_MAX_MHZ - FREQ_MIN_MHZ) / FREQ_STEP_MHZ))

/** Valeurs min/max/défaut pour la puissance */
#define POWER_MIN_DBM                       2
#define POWER_MAX_DBM                       20
#define POWER_DEFAULT_DBM                   20

/** Options fixes pour les paramètres discrets */
#define SF_OPTIONS_COUNT                    7   /* SF6 à SF12 */
#define BW_OPTIONS_COUNT                    3   /* 125, 250, 500 kHz */
#define CR_OPTIONS_COUNT                    4   /* 4/5, 4/6, 4/7, 4/8 */

/** Timeout affichage statut (ms) */
#define APPLY_STATUS_TIMEOUT_MS             2500

/** Debounce boutons +/- (ms) */
#define DEBOUNCE_INTERVAL_MS                150

/** Nombre de pressions longues avant répétition rapide */
#define LONG_PRESS_THRESHOLD_MS             500
#define LONG_PRESS_REPEAT_MS                50

/* ======================================================================== */
/*                VARIABLES STATIQUES (OPTIONS)                             */
/* ======================================================================== */

/** Labels pour les Spreading Factors */
static const char* SF_OPTIONS[SF_OPTIONS_COUNT] = {
    "SF6", "SF7", "SF8", "SF9", "SF10", "SF11", "SF12"
};

/** Labels pour les bandes passantes */
static const char* BW_OPTIONS[BW_OPTIONS_COUNT] = {
    "125 kHz", "250 kHz", "500 kHz"
};

/** Labels pour les Coding Rates */
static const char* CR_OPTIONS[CR_OPTIONS_COUNT] = {
    "4/5", "4/6", "4/7", "4/8"
};

/** Bandes passantes en Hz (correspondant à BW_OPTIONS) */
static const uint32_t BW_VALUES_HZ[BW_OPTIONS_COUNT] = {
    125000, 250000, 500000
};

/** Coding Rates numériques (correspondant à CR_OPTIONS, valeur registre) */
static const uint8_t CR_VALUES[CR_OPTIONS_COUNT] = {
    5, 6, 7, 8  /* 5=4/5, 6=4/6, 7=4/7, 8=4/8 */
};

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

/* --- Initialisation des paramètres --- */
static void init_params(ScreenSettingsNetwork_t* screen);
static void load_params_from_service(ScreenSettingsNetwork_t* screen);
static void save_params_to_service(ScreenSettingsNetwork_t* screen);

/* --- Construction UI --- */
static void create_frequency_widgets(ScreenSettingsNetwork_t* screen);
static void create_power_widgets(ScreenSettingsNetwork_t* screen);
static void create_sf_widgets(ScreenSettingsNetwork_t* screen);
static void create_bw_widgets(ScreenSettingsNetwork_t* screen);
static void create_cr_widgets(ScreenSettingsNetwork_t* screen);
static void create_sync_widgets(ScreenSettingsNetwork_t* screen);
static void create_info_area(ScreenSettingsNetwork_t* screen);

/* --- Callbacks widgets --- */
static void on_freq_slider_changed(void* context, int16_t value);
static void on_power_slider_changed(void* context, int16_t value);
static void on_sf_selected(void* context, uint8_t index);
static void on_bw_selected(void* context, uint8_t index);
static void on_cr_selected(void* context, uint8_t index);
static void on_freq_plus_clicked(void* context);
static void on_freq_minus_clicked(void* context);
static void on_apply_clicked(void* context);
static void on_defaults_clicked(void* context);
static void on_back_clicked(void* context);
static void on_sync_edit_clicked(void* context);

/* --- Application au module LoRa --- */
static ApplyStatus_t apply_frequency(ScreenSettingsNetwork_t* screen);
static ApplyStatus_t apply_power(ScreenSettingsNetwork_t* screen);
static ApplyStatus_t apply_spreading_factor(ScreenSettingsNetwork_t* screen);
static ApplyStatus_t apply_bandwidth(ScreenSettingsNetwork_t* screen);
static ApplyStatus_t apply_coding_rate(ScreenSettingsNetwork_t* screen);
static ApplyStatus_t apply_sync_word(ScreenSettingsNetwork_t* screen);

/* --- Rendu --- */
static void draw_param_row_background(int16_t y, bool is_selected);
static void draw_param_label(const char* label, int16_t y);
static void draw_apply_status(ApplyStatus_t status, int16_t x, int16_t y);
static void update_info_area(ScreenSettingsNetwork_t* screen);
static void update_freq_display(ScreenSettingsNetwork_t* screen);
static void update_power_display(ScreenSettingsNetwork_t* screen);
static void update_sync_display(ScreenSettingsNetwork_t* screen);

/* --- Timers --- */
static void apply_status_timer_callback(TimerHandle_t timer);
static void debounce_timer_callback(TimerHandle_t timer);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise l'écran des réglages réseau
 */
bool ScreenSettingsNetwork_Init(ScreenSettingsNetwork_t* screen,
                                SettingsService_t* settings_service)
{
    if (!screen || !settings_service) {
        DEBUG_ERROR(TAG, "Paramètres invalides");
        return false;
    }

    DEBUG_INFO(TAG, "Initialisation de l'écran réseau LoRa...");

    /* Mise à zéro complète */
    memset(screen, 0, sizeof(ScreenSettingsNetwork_t));

    /* Classe de base */
    ScreenBase_Init(&screen->base, SCREEN_ID_SETTINGS_NETWORK, "Réseau LoRa");

    /* Services */
    screen->settings_service = settings_service;

    /* Récupérer le driver LoRa depuis le gestionnaire global */
    screen->lora_driver = LoRaDriver_GetInstance();
    if (!screen->lora_driver) {
        DEBUG_ERROR(TAG, "Driver LoRa non disponible !");
        return false;
    }

    /* État initial */
    screen->state = NETWORK_STATE_IDLE;
    screen->active_param = NETWORK_PARAM_FREQUENCY;
    screen->selected_row = 0;

    /* ---- Création des widgets ---- */

    /* Barre de statut */
    UIStatusBar_Init(&screen->status_bar);

    /* Titre */
    screen->title_label = UILabel_Create();
    UILabel_SetText(screen->title_label, "Reseau LoRa");
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

    /* Label de statut global */
    screen->status_label = UILabel_Create();
    UILabel_SetText(screen->status_label, "");
    UILabel_SetFont(screen->status_label, &font_small);
    UILabel_SetColor(screen->status_label, THEME_TEXT_TERTIARY);
    UILabel_SetPosition(screen->status_label, CONTENT_X_MARGIN, SCREEN_HEIGHT - 40);

    /* Créer les widgets pour chaque paramètre */
    create_frequency_widgets(screen);
    create_power_widgets(screen);
    create_sf_widgets(screen);
    create_bw_widgets(screen);
    create_cr_widgets(screen);
    create_sync_widgets(screen);
    create_info_area(screen);

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

    /* Timers */
    screen->apply_status_timer = Timer_Create("ApplyStatus",
                                              APPLY_STATUS_TIMEOUT_MS,
                                              false,
                                              apply_status_timer_callback,
                                              screen);
    screen->debounce_timer = Timer_Create("Debounce",
                                          DEBOUNCE_INTERVAL_MS,
                                          false,
                                          debounce_timer_callback,
                                          screen);

    DEBUG_INFO(TAG, "Initialisation terminée");

    return true;
}

/**
 * @brief Affiche l'écran
 */
void ScreenSettingsNetwork_Show(ScreenSettingsNetwork_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Affichage de l'écran réseau");

    screen->state = NETWORK_STATE_IDLE;
    screen->selected_row = -1;

    /* Recharger depuis le module pour synchronisation */
    ScreenSettingsNetwork_ReadFromModule(screen);

    /* Fond d'écran */
    Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);

    /* Barre de statut */
    UIStatusBar_Draw(&screen->status_bar);

    /* Barre de titre */
    Display_FillRect(0, 25, SCREEN_WIDTH, 44, THEME_BG_SURFACE);
    UILabel_Draw(screen->title_label);
    UIButton_Draw(screen->back_button);
    Display_DrawHLine(0, 68, SCREEN_WIDTH, THEME_DIVIDER);

    /* Dessiner chaque ligne de paramètre */
    /* (Les widgets seront dessinés individuellement) */

    /* Fréquence */
    draw_param_row_background(PARAM_FREQ_Y, false);
    draw_param_label("FREQUENCE", PARAM_FREQ_Y);
    update_freq_display(screen);

    /* Puissance */
    draw_param_row_background(PARAM_POWER_Y, false);
    draw_param_label("PUISSANCE", PARAM_POWER_Y);
    update_power_display(screen);
    UISlider_Draw(screen->power_slider);

    /* Spreading Factor */
    draw_param_row_background(PARAM_SF_Y, false);
    draw_param_label("SPREADING FACTOR", PARAM_SF_Y);
    UIRadioGroup_Draw(screen->sf_radio_group);

    /* Bande passante */
    draw_param_row_background(PARAM_BW_Y, false);
    draw_param_label("BANDE PASSANTE", PARAM_BW_Y);
    UIRadioGroup_Draw(screen->bw_radio_group);

    /* Coding Rate */
    draw_param_row_background(PARAM_CR_Y, false);
    draw_param_label("CODING RATE", PARAM_CR_Y);
    UIRadioGroup_Draw(screen->cr_radio_group);

    /* Sync Word */
    draw_param_row_background(PARAM_SYNC_Y, false);
    draw_param_label("SYNC WORD", PARAM_SYNC_Y);
    update_sync_display(screen);

    /* Zone d'infos */
    update_info_area(screen);

    /* Boutons du bas */
    Display_DrawHLine(0, BOTTOM_BUTTONS_Y - 10, SCREEN_WIDTH, THEME_DIVIDER);
    UIButton_Draw(screen->apply_button);
    UIButton_Draw(screen->defaults_button);

    screen->base.is_visible = true;
}

/**
 * @brief Masque l'écran
 */
void ScreenSettingsNetwork_Hide(ScreenSettingsNetwork_t* screen)
{
    if (!screen) return;

    Timer_Stop(screen->apply_status_timer);
    Timer_Stop(screen->debounce_timer);
    screen->base.is_visible = false;
}

/**
 * @brief Mise à jour périodique
 */
void ScreenSettingsNetwork_Update(ScreenSettingsNetwork_t* screen)
{
    if (!screen || !screen->base.is_visible) return;

    UIStatusBar_Update(&screen->status_bar);
}

/**
 * @brief Gère les événements tactiles
 */
bool ScreenSettingsNetwork_HandleTouch(ScreenSettingsNetwork_t* screen,
                                       const TouchEvent_t* event)
{
    if (!screen || !event) return false;

    /* Dialogue de confirmation */
    if (UIDialog_IsVisible(screen->confirm_dialog)) {
        return UIDialog_HandleTouch(screen->confirm_dialog, event);
    }

    /* Si édition sync word avec numpad */
    if (screen->state == NETWORK_STATE_EDITING_SYNC && screen->numpad) {
        return UINumpad_HandleTouch(screen->numpad, event);
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

    /* Bouton Retour */
    if (UIButton_HitTest(screen->back_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->back_button);
        }
        return true;
    }

    /* Slider fréquence */
    if (UISlider_HitTest(screen->freq_slider, event->x, event->y)) {
        return UISlider_HandleTouch(screen->freq_slider, event);
    }

    /* Slider puissance */
    if (UISlider_HitTest(screen->power_slider, event->x, event->y)) {
        return UISlider_HandleTouch(screen->power_slider, event);
    }

    /* Boutons +/- fréquence */
    if (UIButton_HitTest(screen->freq_plus_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->freq_plus_button);
        }
        return true;
    }
    if (UIButton_HitTest(screen->freq_minus_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->freq_minus_button);
        }
        return true;
    }

    /* Radio groups */
    if (UIRadioGroup_HitTest(screen->sf_radio_group, event->x, event->y)) {
        return UIRadioGroup_HandleTouch(screen->sf_radio_group, event);
    }
    if (UIRadioGroup_HitTest(screen->bw_radio_group, event->x, event->y)) {
        return UIRadioGroup_HandleTouch(screen->bw_radio_group, event);
    }
    if (UIRadioGroup_HitTest(screen->cr_radio_group, event->x, event->y)) {
        return UIRadioGroup_HandleTouch(screen->cr_radio_group, event);
    }

    /* Bouton édition sync word */
    if (UIButton_HitTest(screen->sync_edit_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->sync_edit_button);
        }
        return true;
    }

    return false;
}

/**
 * @brief Gère les touches physiques
 */
bool ScreenSettingsNetwork_HandleKey(ScreenSettingsNetwork_t* screen,
                                     KeyCode_t key)
{
    if (!screen) return false;

    switch (key) {
        case KEY_UP:
            if (screen->selected_row > 0) {
                screen->selected_row--;
            }
            return true;

        case KEY_DOWN:
            if (screen->selected_row < 5) {  /* 6 paramètres (0-5) */
                screen->selected_row++;
            }
            return true;

        case KEY_LEFT:
            /* Décrémenter la valeur du paramètre sélectionné */
            if (screen->selected_row == 0) {
                on_freq_minus_clicked(screen);
            }
            return true;

        case KEY_RIGHT:
            /* Incrémenter la valeur du paramètre sélectionné */
            if (screen->selected_row == 0) {
                on_freq_plus_clicked(screen);
            }
            return true;

        case KEY_BACK:
        case KEY_CANCEL:
            on_back_clicked(screen);
            return true;

        case KEY_OK:
        case KEY_SELECT:
            on_apply_clicked(screen);
            return true;

        default:
            break;
    }

    return false;
}

/**
 * @brief Applique tous les paramètres modifiés
 */
uint8_t ScreenSettingsNetwork_ApplyParams(ScreenSettingsNetwork_t* screen)
{
    if (!screen) return 0;

    DEBUG_INFO(TAG, "Application de tous les paramètres...");

    screen->state = NETWORK_STATE_APPLYING;
    uint8_t success_count = 0;

    /* Mettre le module en sleep pour configuration */
    LoRaDriver_SetMode(screen->lora_driver, LORA_MODE_SLEEP);
    HAL_Delay(5);

    /* Appliquer chaque paramètre modifié */
    for (int i = 0; i < NETWORK_PARAM_COUNT; i++) {
        NetworkParam_t* param = &screen->params[i];

        if (!param->is_modified) {
            continue;
        }

        ApplyStatus_t status = APPLY_STATUS_FAILED;
        
        switch (param->id) {
            case NETWORK_PARAM_FREQUENCY:
                status = apply_frequency(screen);
                break;
            case NETWORK_PARAM_POWER:
                status = apply_power(screen);
                break;
            case NETWORK_PARAM_SPREADING_FACTOR:
                status = apply_spreading_factor(screen);
                break;
            case NETWORK_PARAM_BANDWIDTH:
                status = apply_bandwidth(screen);
                break;
            case NETWORK_PARAM_CODING_RATE:
                status = apply_coding_rate(screen);
                break;
            case NETWORK_PARAM_SYNC_WORD:
                status = apply_sync_word(screen);
                break;
            default:
                break;
        }

        param->apply_status = status;
        if (status == APPLY_STATUS_SUCCESS) {
            param->is_modified = false;
            success_count++;
        }
    }

    /* Remettre en mode standby */
    LoRaDriver_SetMode(screen->lora_driver, LORA_MODE_STANDBY);

    /* Sauvegarder en flash */
    save_params_to_service(screen);

    /* Mettre à jour la zone d'infos */
    update_info_area(screen);

    /* Afficher le résultat */
    if (screen->base.is_visible) {
        char status_str[64];
        snprintf(status_str, sizeof(status_str),
                 "%d parametre(s) applique(s) avec succes", success_count);
        UILabel_SetText(screen->status_label, status_str);

        /* Redessiner les indicateurs de statut */
        update_freq_display(screen);
        update_power_display(screen);
        update_sync_display(screen);
        UIRadioGroup_Draw(screen->sf_radio_group);
        UIRadioGroup_Draw(screen->bw_radio_group);
        UIRadioGroup_Draw(screen->cr_radio_group);
    }

    screen->state = NETWORK_STATE_IDLE;

    DEBUG_INFO(TAG, "%d paramètres appliqués avec succès", success_count);

    /* Démarrer le timer pour effacer le message */
    Timer_Start(screen->apply_status_timer);

    return success_count;
}

/**
 * @brief Applique un seul paramètre
 */
ApplyStatus_t ScreenSettingsNetwork_ApplySingleParam(ScreenSettingsNetwork_t* screen,
                                                      NetworkParamId_t param_id)
{
    if (!screen) return APPLY_STATUS_FAILED;

    NetworkParam_t* param = NULL;

    /* Trouver le paramètre */
    for (int i = 0; i < NETWORK_PARAM_COUNT; i++) {
        if (screen->params[i].id == param_id) {
            param = &screen->params[i];
            break;
        }
    }

    if (!param || !param->is_modified) {
        return APPLY_STATUS_IDLE;
    }

    ApplyStatus_t status = APPLY_STATUS_FAILED;

    switch (param_id) {
        case NETWORK_PARAM_FREQUENCY:
            status = apply_frequency(screen);
            break;
        case NETWORK_PARAM_POWER:
            status = apply_power(screen);
            break;
        case NETWORK_PARAM_SPREADING_FACTOR:
            status = apply_spreading_factor(screen);
            break;
        case NETWORK_PARAM_BANDWIDTH:
            status = apply_bandwidth(screen);
            break;
        case NETWORK_PARAM_CODING_RATE:
            status = apply_coding_rate(screen);
            break;
        case NETWORK_PARAM_SYNC_WORD:
            status = apply_sync_word(screen);
            break;
        default:
            break;
    }

    param->apply_status = status;
    if (status == APPLY_STATUS_SUCCESS) {
        param->is_modified = false;
    }

    return status;
}

/**
 * @brief Restaure les valeurs par défaut
 */
void ScreenSettingsNetwork_RestoreDefaults(ScreenSettingsNetwork_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Restauration des valeurs par défaut");

    /* Appliquer les valeurs par défaut */
    screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz = FREQ_DEFAULT_MHZ;
    screen->params[NETWORK_PARAM_POWER].value.power_dbm = POWER_DEFAULT_DBM;
    screen->params[NETWORK_PARAM_SPREADING_FACTOR].value.sf_value = 7;
    screen->params[NETWORK_PARAM_BANDWIDTH].value.bw_index = 1;  /* 250 kHz */
    screen->params[NETWORK_PARAM_CODING_RATE].value.cr_index = 0;  /* 4/5 */
    screen->params[NETWORK_PARAM_SYNC_WORD].value.sync_byte = 0x12;

    /* Marquer tous comme modifiés */
    for (int i = 0; i < NETWORK_PARAM_COUNT; i++) {
        screen->params[i].is_modified = true;
    }

    /* Appliquer au module */
    ScreenSettingsNetwork_ApplyParams(screen);

    /* Mettre à jour l'affichage */
    if (screen->base.is_visible) {
        UISlider_SetValue(screen->freq_slider, 
                         (int16_t)((FREQ_DEFAULT_MHZ - FREQ_MIN_MHZ) / FREQ_STEP_MHZ));
        UISlider_SetValue(screen->power_slider, POWER_DEFAULT_DBM);
        UIRadioGroup_SetSelected(screen->sf_radio_group, 1);  /* SF7 = index 1 */
        UIRadioGroup_SetSelected(screen->bw_radio_group, 1);  /* 250 kHz */
        UIRadioGroup_SetSelected(screen->cr_radio_group, 0);  /* 4/5 */
        
        update_freq_display(screen);
        update_power_display(screen);
        update_sync_display(screen);
        update_info_area(screen);

        /* Redessiner */
        UISlider_Draw(screen->freq_slider);
        UISlider_Draw(screen->power_slider);
        UIRadioGroup_Draw(screen->sf_radio_group);
        UIRadioGroup_Draw(screen->bw_radio_group);
        UIRadioGroup_Draw(screen->cr_radio_group);

        UILabel_SetText(screen->status_label, "Valeurs par defaut restaurees");
        Timer_Start(screen->apply_status_timer);
    }
}

/**
 * @brief Lit les paramètres depuis le module LoRa
 */
void ScreenSettingsNetwork_ReadFromModule(ScreenSettingsNetwork_t* screen)
{
    if (!screen || !screen->lora_driver) return;

    DEBUG_VERBOSE(TAG, "Lecture des paramètres depuis le SX1278");

    /* Lire la fréquence */
    uint32_t freq_reg = SX1278_ReadRegister(screen->lora_driver, REG_FRF_MSB) << 16;
    freq_reg |= SX1278_ReadRegister(screen->lora_driver, REG_FRF_MID) << 8;
    freq_reg |= SX1278_ReadRegister(screen->lora_driver, REG_FRF_LSB);
    screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz = 
        (float)freq_reg * 61.03515625f / 1000000.0f;  /* FSTEP = 61.035 Hz */

    /* Lire la puissance */
    uint8_t pa_config = SX1278_ReadRegister(screen->lora_driver, REG_PA_CONFIG);
    screen->params[NETWORK_PARAM_POWER].value.power_dbm = 
        (int8_t)(pa_config & 0x0F) + 2;  /* PA output power = Pout + 2 */

    /* Lire le modem config */
    uint8_t modem1 = SX1278_ReadRegister(screen->lora_driver, REG_MODEM_CONFIG_1);
    uint8_t modem2 = SX1278_ReadRegister(screen->lora_driver, REG_MODEM_CONFIG_2);

    /* Bande passante (bits 7-4 de ModemConfig1) */
    uint8_t bw_raw = (modem1 >> 4) & 0x0F;
    screen->params[NETWORK_PARAM_BANDWIDTH].value.bw_index = bw_raw - 7;  /* 7=125k, 8=250k, 9=500k */

    /* Coding Rate (bits 3-1 de ModemConfig1) */
    uint8_t cr_raw = (modem1 >> 1) & 0x07;
    screen->params[NETWORK_PARAM_CODING_RATE].value.cr_index = cr_raw - 1;  /* 1=4/5, 2=4/6... */

    /* Spreading Factor (bits 7-4 de ModemConfig2) */
    uint8_t sf_raw = (modem2 >> 4) & 0x0F;
    screen->params[NETWORK_PARAM_SPREADING_FACTOR].value.sf_value = sf_raw;

    /* Sync Word */
    uint8_t sync = SX1278_ReadRegister(screen->lora_driver, REG_SYNC_CONFIG);
    screen->params[NETWORK_PARAM_SYNC_WORD].value.sync_byte = sync;

    /* Mettre à jour les widgets */
    if (screen->base.is_visible) {
        UISlider_SetValue(screen->freq_slider,
                         (int16_t)((screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz 
                                    - FREQ_MIN_MHZ) / FREQ_STEP_MHZ));
        UISlider_SetValue(screen->power_slider,
                         screen->params[NETWORK_PARAM_POWER].value.power_dbm);
        UIRadioGroup_SetSelected(screen->sf_radio_group,
                                 screen->params[NETWORK_PARAM_SPREADING_FACTOR].value.sf_value - 6);
        UIRadioGroup_SetSelected(screen->bw_radio_group,
                                 screen->params[NETWORK_PARAM_BANDWIDTH].value.bw_index);
        UIRadioGroup_SetSelected(screen->cr_radio_group,
                                 screen->params[NETWORK_PARAM_CODING_RATE].value.cr_index);
        update_freq_display(screen);
        update_power_display(screen);
        update_sync_display(screen);
        update_info_area(screen);
    }

    /* Réinitialiser les flags de modification */
    for (int i = 0; i < NETWORK_PARAM_COUNT; i++) {
        screen->params[i].is_modified = false;
        screen->params[i].apply_status = APPLY_STATUS_IDLE;
    }
}

/**
 * @brief Setters individuels
 */
void ScreenSettingsNetwork_SetFrequency(ScreenSettingsNetwork_t* screen, float freq_mhz)
{
    if (!screen) return;
    freq_mhz = CLAMP(freq_mhz, FREQ_MIN_MHZ, FREQ_MAX_MHZ);
    screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz = freq_mhz;
    screen->params[NETWORK_PARAM_FREQUENCY].is_modified = true;
    if (screen->base.is_visible) {
        UISlider_SetValue(screen->freq_slider,
                         (int16_t)((freq_mhz - FREQ_MIN_MHZ) / FREQ_STEP_MHZ));
        update_freq_display(screen);
    }
}

void ScreenSettingsNetwork_SetPower(ScreenSettingsNetwork_t* screen, int8_t power_dbm)
{
    if (!screen) return;
    power_dbm = CLAMP(power_dbm, POWER_MIN_DBM, POWER_MAX_DBM);
    screen->params[NETWORK_PARAM_POWER].value.power_dbm = power_dbm;
    screen->params[NETWORK_PARAM_POWER].is_modified = true;
    if (screen->base.is_visible) {
        UISlider_SetValue(screen->power_slider, power_dbm);
        update_power_display(screen);
    }
}

void ScreenSettingsNetwork_SetSpreadingFactor(ScreenSettingsNetwork_t* screen, uint8_t sf)
{
    if (!screen) return;
    sf = CLAMP(sf, 6, 12);
    screen->params[NETWORK_PARAM_SPREADING_FACTOR].value.sf_value = sf;
    screen->params[NETWORK_PARAM_SPREADING_FACTOR].is_modified = true;
    if (screen->base.is_visible) {
        UIRadioGroup_SetSelected(screen->sf_radio_group, sf - 6);
        UIRadioGroup_Draw(screen->sf_radio_group);
        update_info_area(screen);
    }
}

void ScreenSettingsNetwork_SetBandwidth(ScreenSettingsNetwork_t* screen, uint16_t bw_khz)
{
    if (!screen) return;
    uint8_t index = 0;
    if (bw_khz <= 125) index = 0;
    else if (bw_khz <= 250) index = 1;
    else index = 2;
    screen->params[NETWORK_PARAM_BANDWIDTH].value.bw_index = index;
    screen->params[NETWORK_PARAM_BANDWIDTH].is_modified = true;
    if (screen->base.is_visible) {
        UIRadioGroup_SetSelected(screen->bw_radio_group, index);
        UIRadioGroup_Draw(screen->bw_radio_group);
        update_info_area(screen);
    }
}

void ScreenSettingsNetwork_SetCodingRate(ScreenSettingsNetwork_t* screen, uint8_t cr)
{
    if (!screen) return;
    uint8_t index = CLAMP(cr, 5, 8) - 5;
    screen->params[NETWORK_PARAM_CODING_RATE].value.cr_index = index;
    screen->params[NETWORK_PARAM_CODING_RATE].is_modified = true;
    if (screen->base.is_visible) {
        UIRadioGroup_SetSelected(screen->cr_radio_group, index);
        UIRadioGroup_Draw(screen->cr_radio_group);
        update_info_area(screen);
    }
}

void ScreenSettingsNetwork_SetSyncWord(ScreenSettingsNetwork_t* screen, uint8_t sync_byte)
{
    if (!screen) return;
    screen->params[NETWORK_PARAM_SYNC_WORD].value.sync_byte = sync_byte;
    screen->params[NETWORK_PARAM_SYNC_WORD].is_modified = true;
    if (screen->base.is_visible) {
        update_sync_display(screen);
    }
}

/**
 * @brief Libère les ressources
 */
void ScreenSettingsNetwork_Deinit(ScreenSettingsNetwork_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Libération des ressources réseau");

    Timer_Delete(screen->apply_status_timer);
    Timer_Delete(screen->debounce_timer);

    UILabel_Destroy(screen->title_label);
    UILabel_Destroy(screen->status_label);
    UIButton_Destroy(screen->back_button);
    UIButton_Destroy(screen->apply_button);
    UIButton_Destroy(screen->defaults_button);
    UIButton_Destroy(screen->freq_plus_button);
    UIButton_Destroy(screen->freq_minus_button);
    UIButton_Destroy(screen->sync_edit_button);
    UISlider_Destroy(screen->freq_slider);
    UISlider_Destroy(screen->power_slider);
    UIRadioGroup_Destroy(screen->sf_radio_group);
    UIRadioGroup_Destroy(screen->bw_radio_group);
    UIRadioGroup_Destroy(screen->cr_radio_group);
    UIDialog_Destroy(screen->confirm_dialog);

    /* Libérer les labels des paramètres */
    for (int i = 0; i < NETWORK_PARAM_COUNT; i++) {
        UILabel_Destroy(screen->param_labels[i]);
        UILabel_Destroy(screen->value_labels[i]);
    }

    if (screen->numpad) {
        UINumpad_Destroy(screen->numpad);
    }

    memset(screen, 0, sizeof(ScreenSettingsNetwork_t));
}

/* ======================================================================== */
/*              FONCTIONS DE CONVERSION                                     */
/* ======================================================================== */

float NetworkSettings_SFToSymbolTime(uint8_t sf, uint32_t bw_hz)
{
    /* Ts = 2^SF / BW */
    return (float)(1 << sf) / (float)bw_hz * 1000.0f;
}

uint32_t NetworkSettings_GetBitRate(uint8_t sf, uint32_t bw_hz, uint8_t cr)
{
    /* Rb = SF * BW * CR / (2^SF * 4) */
    /* CR = coding_rate / 4 (ex: 4/5 → cr_num=4, cr_den=5) */
    float cr_ratio = 4.0f / (float)cr;
    return (uint32_t)((float)sf * (float)bw_hz * cr_ratio / (float)(1 << sf) / 4.0f);
}

float NetworkSettings_EstimateRange(int8_t power_dbm, uint8_t sf,
                                    uint32_t bw_hz, float freq_mhz)
{
    /* Bilan de liaison simplifié */
    /* Sensibilité approximative du SX1278 pour SF donné */
    float sensitivity;
    switch (sf) {
        case 6:  sensitivity = -121.0f; break;
        case 7:  sensitivity = -124.0f; break;
        case 8:  sensitivity = -127.0f; break;
        case 9:  sensitivity = -130.0f; break;
        case 10: sensitivity = -133.0f; break;
        case 11: sensitivity = -135.0f; break;
        case 12: sensitivity = -137.0f; break;
        default: sensitivity = -124.0f; break;
    }

    /* Bilan de liaison */
    float link_budget = (float)power_dbm - sensitivity;

    /* Affaiblissement de parcours en espace libre */
    /* Path Loss = 32.44 + 20*log10(d) + 20*log10(f) */
    /* → d = 10^((Pl - 32.44 - 20*log10(f)) / 20) */
    float log_freq = log10f(freq_mhz);
    float distance_km = powf(10.0f, (link_budget - 32.44f - 20.0f * log_freq) / 20.0f);

    /* Appliquer une marge réaliste (zone urbaine = 0.4 * espace libre) */
    return distance_km * 0.4f;
}

/* ======================================================================== */
/*              INITIALISATION DES PARAMÈTRES                               */
/* ======================================================================== */

static void init_params(ScreenSettingsNetwork_t* screen)
{
    if (!screen) return;

    /* Paramètre 0 : Fréquence */
    screen->params[0].id = NETWORK_PARAM_FREQUENCY;
    screen->params[0].name = "FREQUENCE";
    screen->params[0].unit = "MHz";
    screen->params[0].limits.freq_range.min = FREQ_MIN_MHZ;
    screen->params[0].limits.freq_range.max = FREQ_MAX_MHZ;
    screen->params[0].limits.freq_range.step = FREQ_STEP_MHZ;

    /* Paramètre 1 : Puissance */
    screen->params[1].id = NETWORK_PARAM_POWER;
    screen->params[1].name = "PUISSANCE";
    screen->params[1].unit = "dBm";
    screen->params[1].limits.power_range.min = POWER_MIN_DBM;
    screen->params[1].limits.power_range.max = POWER_MAX_DBM;

    /* Paramètre 2 : Spreading Factor */
    screen->params[2].id = NETWORK_PARAM_SPREADING_FACTOR;
    screen->params[2].name = "SPREADING FACTOR";
    screen->params[2].unit = "";
    screen->params[2].options = SF_OPTIONS;
    screen->params[2].option_count = SF_OPTIONS_COUNT;
    screen->params[2].limits.sf_range.min = 6;
    screen->params[2].limits.sf_range.max = 12;

    /* Paramètre 3 : Bande passante */
    screen->params[3].id = NETWORK_PARAM_BANDWIDTH;
    screen->params[3].name = "BANDE PASSANTE";
    screen->params[3].unit = "";
    screen->params[3].options = BW_OPTIONS;
    screen->params[3].option_count = BW_OPTIONS_COUNT;

    /* Paramètre 4 : Coding Rate */
    screen->params[4].id = NETWORK_PARAM_CODING_RATE;
    screen->params[4].name = "CODING RATE";
    screen->params[4].unit = "";
    screen->params[4].options = CR_OPTIONS;
    screen->params[4].option_count = CR_OPTIONS_COUNT;

    /* Paramètre 5 : Sync Word */
    screen->params[5].id = NETWORK_PARAM_SYNC_WORD;
    screen->params[5].name = "SYNC WORD";
    screen->params[5].unit = "";

    /* Initialiser les statuts */
    for (int i = 0; i < NETWORK_PARAM_COUNT; i++) {
        screen->params[i].apply_status = APPLY_STATUS_IDLE;
        screen->params[i].is_modified = false;
        screen->params[i].widget = NULL;
    }
}

static void load_params_from_service(ScreenSettingsNetwork_t* screen)
{
    if (!screen || !screen->settings_service) return;

    SettingsService_t* svc = screen->settings_service;

    /* Charger depuis le service persistant */
    uint32_t freq_hz;
    if (SettingsService_GetLoRaFrequency(svc, &freq_hz)) {
        screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz = freq_hz / 1000000.0f;
    } else {
        screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz = FREQ_DEFAULT_MHZ;
    }

    int8_t power;
    if (SettingsService_GetLoRaPower(svc, &power)) {
        screen->params[NETWORK_PARAM_POWER].value.power_dbm = power;
    } else {
        screen->params[NETWORK_PARAM_POWER].value.power_dbm = POWER_DEFAULT_DBM;
    }

    uint8_t sf;
    if (SettingsService_GetLoRaSF(svc, &sf)) {
        screen->params[NETWORK_PARAM_SPREADING_FACTOR].value.sf_value = sf;
    } else {
        screen->params[NETWORK_PARAM_SPREADING_FACTOR].value.sf_value = 7;
    }

    uint8_t bw;
    if (SettingsService_GetLoRaBandwidth(svc, &bw)) {
        screen->params[NETWORK_PARAM_BANDWIDTH].value.bw_index = bw;
    } else {
        screen->params[NETWORK_PARAM_BANDWIDTH].value.bw_index = 1;  /* 250 kHz */
    }

    uint8_t cr;
    if (SettingsService_GetLoRaCodingRate(svc, &cr)) {
        screen->params[NETWORK_PARAM_CODING_RATE].value.cr_index = cr;
    } else {
        screen->params[NETWORK_PARAM_CODING_RATE].value.cr_index = 0;  /* 4/5 */
    }

    uint8_t sync;
    if (SettingsService_GetLoRaSyncWord(svc, &sync)) {
        screen->params[NETWORK_PARAM_SYNC_WORD].value.sync_byte = sync;
    } else {
        screen->params[NETWORK_PARAM_SYNC_WORD].value.sync_byte = 0x12;
    }

    /* Mettre à jour les sliders */
    if (screen->freq_slider) {
        UISlider_SetValue(screen->freq_slider,
                         (int16_t)((screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz
                                    - FREQ_MIN_MHZ) / FREQ_STEP_MHZ));
    }
    if (screen->power_slider) {
        UISlider_SetValue(screen->power_slider,
                         screen->params[NETWORK_PARAM_POWER].value.power_dbm);
    }
    if (screen->sf_radio_group) {
        UIRadioGroup_SetSelected(screen->sf_radio_group,
                                 screen->params[NETWORK_PARAM_SPREADING_FACTOR].value.sf_value - 6);
    }
    if (screen->bw_radio_group) {
        UIRadioGroup_SetSelected(screen->bw_radio_group,
                                 screen->params[NETWORK_PARAM_BANDWIDTH].value.bw_index);
    }
    if (screen->cr_radio_group) {
        UIRadioGroup_SetSelected(screen->cr_radio_group,
                                 screen->params[NETWORK_PARAM_CODING_RATE].value.cr_index);
    }
}

static void save_params_to_service(ScreenSettingsNetwork_t* screen)
{
    if (!screen || !screen->settings_service) return;

    SettingsService_t* svc = screen->settings_service;

    SettingsService_SetLoRaFrequency(svc, 
        (uint32_t)(screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz * 1000000.0f));
    SettingsService_SetLoRaPower(svc, 
        screen->params[NETWORK_PARAM_POWER].value.power_dbm);
    SettingsService_SetLoRaSF(svc, 
        screen->params[NETWORK_PARAM_SPREADING_FACTOR].value.sf_value);
    SettingsService_SetLoRaBandwidth(svc, 
        screen->params[NETWORK_PARAM_BANDWIDTH].value.bw_index);
    SettingsService_SetLoRaCodingRate(svc, 
        screen->params[NETWORK_PARAM_CODING_RATE].value.cr_index);
    SettingsService_SetLoRaSyncWord(svc, 
        screen->params[NETWORK_PARAM_SYNC_WORD].value.sync_byte);

    SettingsService_Save(svc);
}

/* ======================================================================== */
/*              CRÉATION DES WIDGETS                                        */
/* ======================================================================== */

static void create_frequency_widgets(ScreenSettingsNetwork_t* screen)
{
    /* Slider fréquence */
    screen->freq_slider = UISlider_Create();
    UISlider_SetPosition(screen->freq_slider, SLIDER_X, PARAM_FREQ_Y + 22);
    UISlider_SetSize(screen->freq_slider, SLIDER_WIDTH, SLIDER_HEIGHT);
    UISlider_SetRange(screen->freq_slider, 0, FREQ_SLIDER_STEPS);
    UISlider_SetOnChanged(screen->freq_slider, on_freq_slider_changed, screen);
    UISlider_SetColor(screen->freq_slider, THEME_ACCENT);

    /* Label valeur */
    screen->value_labels[NETWORK_PARAM_FREQUENCY] = UILabel_Create();
    UILabel_SetPosition(screen->value_labels[NETWORK_PARAM_FREQUENCY], 
                        SLIDER_X + SLIDER_WIDTH + 10, PARAM_FREQ_Y + 20);
    UILabel_SetFont(screen->value_labels[NETWORK_PARAM_FREQUENCY], &font_medium_bold);
    UILabel_SetColor(screen->value_labels[NETWORK_PARAM_FREQUENCY], THEME_TEXT_PRIMARY);

    /* Boutons +/- */
    screen->freq_plus_button = UIButton_Create();
    UIButton_SetText(screen->freq_plus_button, "+");
    UIButton_SetFont(screen->freq_plus_button, &font_medium);
    UIButton_SetSize(screen->freq_plus_button, 35, 28);
    UIButton_SetPosition(screen->freq_plus_button, SCREEN_WIDTH - 80, PARAM_FREQ_Y + 18);
    UIButton_SetOnClick(screen->freq_plus_button, on_freq_plus_clicked, screen);
    UIButton_SetCornerRadius(screen->freq_plus_button, 5);

    screen->freq_minus_button = UIButton_Create();
    UIButton_SetText(screen->freq_minus_button, "-");
    UIButton_SetFont(screen->freq_minus_button, &font_medium);
    UIButton_SetSize(screen->freq_minus_button, 35, 28);
    UIButton_SetPosition(screen->freq_minus_button, SCREEN_WIDTH - 40, PARAM_FREQ_Y + 18);
    UIButton_SetOnClick(screen->freq_minus_button, on_freq_minus_clicked, screen);
    UIButton_SetCornerRadius(screen->freq_minus_button, 5);
}

static void create_power_widgets(ScreenSettingsNetwork_t* screen)
{
    screen->power_slider = UISlider_Create();
    UISlider_SetPosition(screen->power_slider, SLIDER_X, PARAM_POWER_Y + 22);
    UISlider_SetSize(screen->power_slider, SLIDER_WIDTH, SLIDER_HEIGHT);
    UISlider_SetRange(screen->power_slider, POWER_MIN_DBM, POWER_MAX_DBM);
    UISlider_SetOnChanged(screen->power_slider, on_power_slider_changed, screen);
    UISlider_SetColor(screen->power_slider, THEME_WARNING);

    screen->value_labels[NETWORK_PARAM_POWER] = UILabel_Create();
    UILabel_SetPosition(screen->value_labels[NETWORK_PARAM_POWER],
                        SLIDER_X + SLIDER_WIDTH + 10, PARAM_POWER_Y + 20);
    UILabel_SetFont(screen->value_labels[NETWORK_PARAM_POWER], &font_medium_bold);
    UILabel_SetColor(screen->value_labels[NETWORK_PARAM_POWER], THEME_TEXT_PRIMARY);
}

static void create_sf_widgets(ScreenSettingsNetwork_t* screen)
{
    screen->sf_radio_group = UIRadioGroup_Create();
    UIRadioGroup_SetPosition(screen->sf_radio_group, CONTENT_X_MARGIN + 5, PARAM_SF_Y + 22);
    UIRadioGroup_SetOptions(screen->sf_radio_group, SF_OPTIONS, SF_OPTIONS_COUNT);
    UIRadioGroup_SetOrientation(screen->sf_radio_group, RADIO_HORIZONTAL);
    UIRadioGroup_SetSpacing(screen->sf_radio_group, 4);
    UIRadioGroup_SetOnSelected(screen->sf_radio_group, on_sf_selected, screen);
    UIRadioGroup_SetButtonColor(screen->sf_radio_group, THEME_BUTTON_NEUTRAL);
    UIRadioGroup_SetSelectedColor(screen->sf_radio_group, THEME_ACCENT);
}

static void create_bw_widgets(ScreenSettingsNetwork_t* screen)
{
    screen->bw_radio_group = UIRadioGroup_Create();
    UIRadioGroup_SetPosition(screen->bw_radio_group, CONTENT_X_MARGIN + 5, PARAM_BW_Y + 22);
    UIRadioGroup_SetOptions(screen->bw_radio_group, BW_OPTIONS, BW_OPTIONS_COUNT);
    UIRadioGroup_SetOrientation(screen->bw_radio_group, RADIO_HORIZONTAL);
    UIRadioGroup_SetSpacing(screen->bw_radio_group, 8);
    UIRadioGroup_SetOnSelected(screen->bw_radio_group, on_bw_selected, screen);
    UIRadioGroup_SetButtonColor(screen->bw_radio_group, THEME_BUTTON_NEUTRAL);
    UIRadioGroup_SetSelectedColor(screen->bw_radio_group, THEME_INFO);
}

static void create_cr_widgets(ScreenSettingsNetwork_t* screen)
{
    screen->cr_radio_group = UIRadioGroup_Create();
    UIRadioGroup_SetPosition(screen->cr_radio_group, CONTENT_X_MARGIN + 5, PARAM_CR_Y + 22);
    UIRadioGroup_SetOptions(screen->cr_radio_group, CR_OPTIONS, CR_OPTIONS_COUNT);
    UIRadioGroup_SetOrientation(screen->cr_radio_group, RADIO_HORIZONTAL);
    UIRadioGroup_SetSpacing(screen->cr_radio_group, 12);
    UIRadioGroup_SetOnSelected(screen->cr_radio_group, on_cr_selected, screen);
    UIRadioGroup_SetButtonColor(screen->cr_radio_group, THEME_BUTTON_NEUTRAL);
    UIRadioGroup_SetSelectedColor(screen->cr_radio_group, THEME_SUCCESS);
}

static void create_sync_widgets(ScreenSettingsNetwork_t* screen)
{
    /* Label valeur sync word */
    screen->value_labels[NETWORK_PARAM_SYNC_WORD] = UILabel_Create();
    UILabel_SetPosition(screen->value_labels[NETWORK_PARAM_SYNC_WORD],
                        CONTENT_X_MARGIN + 10, PARAM_SYNC_Y + 22);
    UILabel_SetFont(screen->value_labels[NETWORK_PARAM_SYNC_WORD], &font_medium_bold);
    UILabel_SetColor(screen->value_labels[NETWORK_PARAM_SYNC_WORD], THEME_ACCENT);

    /* Bouton Modifier */
    screen->sync_edit_button = UIButton_Create();
    UIButton_SetText(screen->sync_edit_button, "Modifier");
    UIButton_SetFont(screen->sync_edit_button, &font_small);
    UIButton_SetSize(screen->sync_edit_button, 80, 28);
    UIButton_SetPosition(screen->sync_edit_button, SCREEN_WIDTH - 100, PARAM_SYNC_Y + 18);
    UIButton_SetOnClick(screen->sync_edit_button, on_sync_edit_clicked, screen);
    UIButton_SetCornerRadius(screen->sync_edit_button, 5);
}

static void create_info_area(ScreenSettingsNetwork_t* screen)
{
    /* Les infos sont redessinées dynamiquement dans update_info_area() */
    /* Pas de widgets persistants, juste du texte rendu directement */
}

/* ======================================================================== */
/*              CALLBACKS WIDGETS                                           */
/* ======================================================================== */

static void on_freq_slider_changed(void* context, int16_t value)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    float freq = FREQ_MIN_MHZ + (float)value * FREQ_STEP_MHZ;
    freq = roundf(freq * 10.0f) / 10.0f;  /* Arrondir à 0.1 MHz */

    screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz = freq;
    screen->params[NETWORK_PARAM_FREQUENCY].is_modified = true;
    screen->params[NETWORK_PARAM_FREQUENCY].apply_status = APPLY_STATUS_PENDING;

    update_freq_display(screen);
    update_info_area(screen);

    /* Application immédiate pour la fréquence */
    ScreenSettingsNetwork_ApplySingleParam(screen, NETWORK_PARAM_FREQUENCY);
}

static void on_power_slider_changed(void* context, int16_t value)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    screen->params[NETWORK_PARAM_POWER].value.power_dbm = (int8_t)value;
    screen->params[NETWORK_PARAM_POWER].is_modified = true;
    screen->params[NETWORK_PARAM_POWER].apply_status = APPLY_STATUS_PENDING;

    update_power_display(screen);
    update_info_area(screen);
}

static void on_sf_selected(void* context, uint8_t index)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    screen->params[NETWORK_PARAM_SPREADING_FACTOR].value.sf_value = index + 6;
    screen->params[NETWORK_PARAM_SPREADING_FACTOR].is_modified = true;
    screen->params[NETWORK_PARAM_SPREADING_FACTOR].apply_status = APPLY_STATUS_PENDING;

    update_info_area(screen);
}

static void on_bw_selected(void* context, uint8_t index)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    screen->params[NETWORK_PARAM_BANDWIDTH].value.bw_index = index;
    screen->params[NETWORK_PARAM_BANDWIDTH].is_modified = true;
    screen->params[NETWORK_PARAM_BANDWIDTH].apply_status = APPLY_STATUS_PENDING;

    update_info_area(screen);
}

static void on_cr_selected(void* context, uint8_t index)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    screen->params[NETWORK_PARAM_CODING_RATE].value.cr_index = index;
    screen->params[NETWORK_PARAM_CODING_RATE].is_modified = true;
    screen->params[NETWORK_PARAM_CODING_RATE].apply_status = APPLY_STATUS_PENDING;

    update_info_area(screen);
}

static void on_freq_plus_clicked(void* context)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    float freq = screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz;
    freq += FREQ_STEP_MHZ;
    if (freq <= FREQ_MAX_MHZ) {
        ScreenSettingsNetwork_SetFrequency(screen, freq);
        ScreenSettingsNetwork_ApplySingleParam(screen, NETWORK_PARAM_FREQUENCY);
    }
}

static void on_freq_minus_clicked(void* context)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    float freq = screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz;
    freq -= FREQ_STEP_MHZ;
    if (freq >= FREQ_MIN_MHZ) {
        ScreenSettingsNetwork_SetFrequency(screen, freq);
        ScreenSettingsNetwork_ApplySingleParam(screen, NETWORK_PARAM_FREQUENCY);
    }
}

static void on_apply_clicked(void* context)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    /* Vérifier si des modifications sont en attente */
    bool has_modifications = false;
    for (int i = 0; i < NETWORK_PARAM_COUNT; i++) {
        if (screen->params[i].is_modified) {
            has_modifications = true;
            break;
        }
    }

    if (!has_modifications) {
        UILabel_SetText(screen->status_label, "Aucune modification a appliquer");
        Timer_Start(screen->apply_status_timer);
        return;
    }

    /* Appliquer tout */
    uint8_t count = ScreenSettingsNetwork_ApplyParams(screen);
    DEBUG_INFO(TAG, "%d paramètres appliqués", count);
}

static void on_defaults_clicked(void* context)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    /* Demander confirmation */
    UIDialog_SetTitle(screen->confirm_dialog, "Valeurs par defaut");
    UIDialog_SetMessage(screen->confirm_dialog,
                        "Remettre tous les parametres\n"
                        "reseau aux valeurs d'usine ?\n\n"
                        "Frequence : 868.0 MHz\n"
                        "Puissance : 20 dBm\n"
                        "SF        : 7\n"
                        "Bande     : 250 kHz\n"
                        "CR        : 4/5\n"
                        "Sync      : 0x12");
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
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    UIDialog_SetVisible(screen->confirm_dialog, false);

    if (screen->base.is_visible) {
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
        ScreenSettingsNetwork_Show(screen);
    }

    if (confirmed) {
        ScreenSettingsNetwork_RestoreDefaults(screen);
    }
}

static void on_back_clicked(void* context)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    /* Vérifier s'il y a des modifications non appliquées */
    bool has_pending = false;
    for (int i = 0; i < NETWORK_PARAM_COUNT; i++) {
        if (screen->params[i].is_modified) {
            has_pending = true;
            break;
        }
    }

    if (has_pending) {
        UIDialog_SetTitle(screen->confirm_dialog, "Modifications non appliquees");
        UIDialog_SetMessage(screen->confirm_dialog,
                            "Vous avez des modifications\n"
                            "non appliquees.\n\n"
                            "Voulez-vous les appliquer\n"
                            "avant de quitter ?");
        UIDialog_SetOnResult(screen->confirm_dialog,
                            (void(*)(void*, bool))on_quit_confirmed,
                            screen);
        UIDialog_SetVisible(screen->confirm_dialog, true);

        if (screen->base.is_visible) {
            UIDialog_Draw(screen->confirm_dialog);
        }
        return;
    }

    /* Pas de modifications, retour direct */
    if (screen->on_back_pressed) {
        screen->on_back_pressed();
    } else {
        UINavigation_GoBack();
    }
}

static void on_quit_confirmed(void* context, bool confirmed)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    UIDialog_SetVisible(screen->confirm_dialog, false);

    if (confirmed) {
        /* Appliquer puis quitter */
        ScreenSettingsNetwork_ApplyParams(screen);
    }

    if (screen->on_back_pressed) {
        screen->on_back_pressed();
    } else {
        UINavigation_GoBack();
    }
}

static void on_sync_edit_clicked(void* context)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    /* Ouvrir le pavé numérique pour saisir la valeur hex */
    screen->state = NETWORK_STATE_EDITING_SYNC;

    if (!screen->numpad) {
        screen->numpad = UINumpad_Create();
        UINumpad_SetPosition(screen->numpad, 20, 180);
        UINumpad_SetMode(screen->numpad, NUMPAD_MODE_HEX);
        UINumpad_SetMaxDigits(screen->numpad, 2);
        UINumpad_SetOnComplete(screen->numpad, on_sync_numpad_complete, screen);
    }

    UINumpad_SetVisible(screen->numpad, true);
    UINumpad_SetValueHex(screen->numpad, 
                         screen->params[NETWORK_PARAM_SYNC_WORD].value.sync_byte);

    if (screen->base.is_visible) {
        UINumpad_Draw(screen->numpad);
    }
}

static void on_sync_numpad_complete(void* context, uint32_t value)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)context;
    if (!screen) return;

    screen->state = NETWORK_STATE_IDLE;
    UINumpad_SetVisible(screen->numpad, false);

    ScreenSettingsNetwork_SetSyncWord(screen, (uint8_t)(value & 0xFF));
    ScreenSettingsNetwork_ApplySingleParam(screen, NETWORK_PARAM_SYNC_WORD);

    if (screen->base.is_visible) {
        /* Effacer le numpad et redessiner */
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
        ScreenSettingsNetwork_Show(screen);
    }
}

/* ======================================================================== */
/*              APPLICATION AU MODULE LoRa                                  */
/* ======================================================================== */

static ApplyStatus_t apply_frequency(ScreenSettingsNetwork_t* screen)
{
    if (!screen || !screen->lora_driver) return APPLY_STATUS_FAILED;

    float freq_mhz = screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz;

    /* Convertir en valeur registre 24 bits */
    /* Freg = Freq / FSTEP, où FSTEP = 61.03515625 Hz */
    uint32_t freq_val = (uint32_t)(freq_mhz * 1000000.0f / 61.03515625f);

    /* Écrire les 3 registres */
    bool ok = true;
    ok &= SX1278_WriteRegister(screen->lora_driver, REG_FRF_MSB, (freq_val >> 16) & 0xFF);
    ok &= SX1278_WriteRegister(screen->lora_driver, REG_FRF_MID, (freq_val >> 8) & 0xFF);
    ok &= SX1278_WriteRegister(screen->lora_driver, REG_FRF_LSB, freq_val & 0xFF);

    /* Vérifier en relisant */
    uint32_t readback = SX1278_ReadRegister(screen->lora_driver, REG_FRF_MSB) << 16;
    readback |= SX1278_ReadRegister(screen->lora_driver, REG_FRF_MID) << 8;
    readback |= SX1278_ReadRegister(screen->lora_driver, REG_FRF_LSB);

    if (ok && readback == freq_val) {
        DEBUG_VERBOSE(TAG, "Fréquence appliquée: %.1f MHz", freq_mhz);
        return APPLY_STATUS_SUCCESS;
    } else {
        DEBUG_ERROR(TAG, "Échec application fréquence (attendu: 0x%06lX, lu: 0x%06lX)",
                    freq_val, readback);
        return APPLY_STATUS_FAILED;
    }
}

static ApplyStatus_t apply_power(ScreenSettingsNetwork_t* screen)
{
    if (!screen || !screen->lora_driver) return APPLY_STATUS_FAILED;

    int8_t power = screen->params[NETWORK_PARAM_POWER].value.power_dbm;

    /* Configurer PA_BOOST et puissance */
    uint8_t pa_config = 0x80;  /* PA_BOOST activé */
    pa_config |= ((uint8_t)(power - 2)) & 0x0F;  /* Pout = power - 2 */

    bool ok = SX1278_WriteRegister(screen->lora_driver, REG_PA_CONFIG, pa_config);
    uint8_t readback = SX1278_ReadRegister(screen->lora_driver, REG_PA_CONFIG);

    if (ok && (readback & 0x8F) == pa_config) {
        DEBUG_VERBOSE(TAG, "Puissance appliquée: %d dBm", power);
        return APPLY_STATUS_SUCCESS;
    } else {
        DEBUG_ERROR(TAG, "Échec application puissance");
        return APPLY_STATUS_FAILED;
    }
}

static ApplyStatus_t apply_spreading_factor(ScreenSettingsNetwork_t* screen)
{
    if (!screen || !screen->lora_driver) return APPLY_STATUS_FAILED;

    uint8_t sf = screen->params[NETWORK_PARAM_SPREADING_FACTOR].value.sf_value;

    /* Lire la config actuelle */
    uint8_t modem2 = SX1278_ReadRegister(screen->lora_driver, REG_MODEM_CONFIG_2);

    /* Modifier SF (bits 7-4) */
    modem2 &= ~0xF0;
    modem2 |= (sf << 4);

    bool ok = SX1278_WriteRegister(screen->lora_driver, REG_MODEM_CONFIG_2, modem2);
    uint8_t readback = SX1278_ReadRegister(screen->lora_driver, REG_MODEM_CONFIG_2);

    if (ok && ((readback >> 4) & 0x0F) == sf) {
        DEBUG_VERBOSE(TAG, "SF appliqué: %d", sf);
        return APPLY_STATUS_SUCCESS;
    } else {
        DEBUG_ERROR(TAG, "Échec application SF");
        return APPLY_STATUS_FAILED;
    }
}

static ApplyStatus_t apply_bandwidth(ScreenSettingsNetwork_t* screen)
{
    if (!screen || !screen->lora_driver) return APPLY_STATUS_FAILED;

    uint8_t bw_index = screen->params[NETWORK_PARAM_BANDWIDTH].value.bw_index;
    uint8_t bw_reg_value = bw_index + 7;  /* 7=125k, 8=250k, 9=500k */

    uint8_t modem1 = SX1278_ReadRegister(screen->lora_driver, REG_MODEM_CONFIG_1);
    modem1 &= ~0xF0;  /* Clear BW bits */
    modem1 |= (bw_reg_value << 4);

    bool ok = SX1278_WriteRegister(screen->lora_driver, REG_MODEM_CONFIG_1, modem1);
    uint8_t readback = SX1278_ReadRegister(screen->lora_driver, REG_MODEM_CONFIG_1);

    if (ok && ((readback >> 4) & 0x0F) == bw_reg_value) {
        DEBUG_VERBOSE(TAG, "Bande passante appliquée: %s", BW_OPTIONS[bw_index]);
        return APPLY_STATUS_SUCCESS;
    } else {
        DEBUG_ERROR(TAG, "Échec application bande passante");
        return APPLY_STATUS_FAILED;
    }
}

static ApplyStatus_t apply_coding_rate(ScreenSettingsNetwork_t* screen)
{
    if (!screen || !screen->lora_driver) return APPLY_STATUS_FAILED;

    uint8_t cr_index = screen->params[NETWORK_PARAM_CODING_RATE].value.cr_index;
    uint8_t cr_reg_value = cr_index + 1;  /* 1=4/5, 2=4/6, 3=4/7, 4=4/8 */

    uint8_t modem1 = SX1278_ReadRegister(screen->lora_driver, REG_MODEM_CONFIG_1);
    modem1 &= ~0x0E;  /* Clear CR bits */
    modem1 |= (cr_reg_value << 1);

    bool ok = SX1278_WriteRegister(screen->lora_driver, REG_MODEM_CONFIG_1, modem1);
    uint8_t readback = SX1278_ReadRegister(screen->lora_driver, REG_MODEM_CONFIG_1);

    if (ok && ((readback >> 1) & 0x07) == cr_reg_value) {
        DEBUG_VERBOSE(TAG, "Coding rate appliqué: %s", CR_OPTIONS[cr_index]);
        return APPLY_STATUS_SUCCESS;
    } else {
        DEBUG_ERROR(TAG, "Échec application coding rate");
        return APPLY_STATUS_FAILED;
    }
}

static ApplyStatus_t apply_sync_word(ScreenSettingsNetwork_t* screen)
{
    if (!screen || !screen->lora_driver) return APPLY_STATUS_FAILED;

    uint8_t sync = screen->params[NETWORK_PARAM_SYNC_WORD].value.sync_byte;

    bool ok = SX1278_WriteRegister(screen->lora_driver, REG_SYNC_CONFIG, sync);
    uint8_t readback = SX1278_ReadRegister(screen->lora_driver, REG_SYNC_CONFIG);

    if (ok && readback == sync) {
        DEBUG_VERBOSE(TAG, "Sync word appliqué: 0x%02X", sync);
        return APPLY_STATUS_SUCCESS;
    } else {
        DEBUG_ERROR(TAG, "Échec application sync word");
        return APPLY_STATUS_FAILED;
    }
}

/* ======================================================================== */
/*              RENDU GRAPHIQUE                                             */
/* ======================================================================== */

static void draw_param_row_background(int16_t y, bool is_selected)
{
    uint16_t bg = is_selected ? THEME_LIST_SELECTED : THEME_BG_MAIN;
    Display_FillRect(CONTENT_X_MARGIN, y, CONTENT_WIDTH, PARAM_ROW_HEIGHT - 2, bg);
}

static void draw_param_label(const char* label, int16_t y)
{
    Display_DrawText(CONTENT_X_MARGIN + 5, y + 4,
                     label, &font_small_bold,
                     THEME_TEXT_TERTIARY, THEME_BG_MAIN);
}

static void draw_apply_status(ApplyStatus_t status, int16_t x, int16_t y)
{
    const char* symbol;
    uint16_t color;

    switch (status) {
        case APPLY_STATUS_SUCCESS:
            symbol = "✓";
            color = THEME_SUCCESS;
            break;
        case APPLY_STATUS_PENDING:
            symbol = "⏳";
            color = THEME_WARNING;
            break;
        case APPLY_STATUS_FAILED:
            symbol = "✗";
            color = THEME_DANGER;
            break;
        default:
            return;  /* IDLE : ne rien afficher */
    }

    Display_DrawText(x, y, symbol, &font_small, color, THEME_BG_MAIN);
}

static void update_info_area(ScreenSettingsNetwork_t* screen)
{
    if (!screen) return;

    /* Effacer la zone */
    Display_FillRect(CONTENT_X_MARGIN, INFO_AREA_Y,
                     CONTENT_WIDTH, INFO_AREA_HEIGHT, THEME_BG_SURFACE);
    Display_DrawRect(CONTENT_X_MARGIN, INFO_AREA_Y,
                     CONTENT_WIDTH, INFO_AREA_HEIGHT, THEME_DIVIDER);

    /* Récupérer les valeurs actuelles */
    float freq_mhz = screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz;
    int8_t power_dbm = screen->params[NETWORK_PARAM_POWER].value.power_dbm;
    uint8_t sf = screen->params[NETWORK_PARAM_SPREADING_FACTOR].value.sf_value;
    uint8_t bw_index = screen->params[NETWORK_PARAM_BANDWIDTH].value.bw_index;
    uint8_t cr_index = screen->params[NETWORK_PARAM_CODING_RATE].value.cr_index;
    uint32_t bw_hz = BW_VALUES_HZ[bw_index];
    uint8_t cr = CR_VALUES[cr_index];

    /* Calculer les estimations */
    float sym_time = NetworkSettings_SFToSymbolTime(sf, bw_hz);
    uint32_t bitrate = NetworkSettings_GetBitRate(sf, bw_hz, cr);
    float range = NetworkSettings_EstimateRange(power_dbm, sf, bw_hz, freq_mhz);

    /* Afficher les infos */
    char info_line[64];
    int16_t y = INFO_AREA_Y + 8;

    snprintf(info_line, sizeof(info_line), 
             "Portee estimee  : %.1f km", range);
    Display_DrawText(CONTENT_X_MARGIN + 10, y,
                     info_line, &font_small, THEME_TEXT_PRIMARY, THEME_BG_SURFACE);

    y += 18;
    snprintf(info_line, sizeof(info_line),
             "Debit estime    : %lu bps", bitrate);
    Display_DrawText(CONTENT_X_MARGIN + 10, y,
                     info_line, &font_small, THEME_TEXT_PRIMARY, THEME_BG_SURFACE);

    y += 18;
    snprintf(info_line, sizeof(info_line),
             "Temps symbole   : %.1f ms", sym_time);
    Display_DrawText(CONTENT_X_MARGIN + 10, y,
                     info_line, &font_small, THEME_TEXT_PRIMARY, THEME_BG_SURFACE);
}

static void update_freq_display(ScreenSettingsNetwork_t* screen)
{
    if (!screen) return;

    float freq = screen->params[NETWORK_PARAM_FREQUENCY].value.freq_mhz;
    char buffer[VALUE_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%.3f MHz", freq);

    UILabel_SetText(screen->value_labels[NETWORK_PARAM_FREQUENCY], buffer);
    UILabel_Draw(screen->value_labels[NETWORK_PARAM_FREQUENCY]);

    /* Indicateur de statut */
    draw_apply_status(screen->params[NETWORK_PARAM_FREQUENCY].apply_status,
                      SCREEN_WIDTH - 25, PARAM_FREQ_Y + 4);
}

static void update_power_display(ScreenSettingsNetwork_t* screen)
{
    if (!screen) return;

    int8_t power = screen->params[NETWORK_PARAM_POWER].value.power_dbm;
    char buffer[VALUE_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%d dBm", power);

    UILabel_SetText(screen->value_labels[NETWORK_PARAM_POWER], buffer);
    UILabel_Draw(screen->value_labels[NETWORK_PARAM_POWER]);

    draw_apply_status(screen->params[NETWORK_PARAM_POWER].apply_status,
                      SCREEN_WIDTH - 25, PARAM_POWER_Y + 4);
}

static void update_sync_display(ScreenSettingsNetwork_t* screen)
{
    if (!screen) return;

    uint8_t sync = screen->params[NETWORK_PARAM_SYNC_WORD].value.sync_byte;
    char buffer[VALUE_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "0x%02X", sync);

    UILabel_SetText(screen->value_labels[NETWORK_PARAM_SYNC_WORD], buffer);
    UILabel_Draw(screen->value_labels[NETWORK_PARAM_SYNC_WORD]);
    UIButton_Draw(screen->sync_edit_button);

    draw_apply_status(screen->params[NETWORK_PARAM_SYNC_WORD].apply_status,
                      SCREEN_WIDTH - 25, PARAM_SYNC_Y + 4);
}

/* ======================================================================== */
/*              TIMERS                                                      */
/* ======================================================================== */

static void apply_status_timer_callback(TimerHandle_t timer)
{
    ScreenSettingsNetwork_t* screen = (ScreenSettingsNetwork_t*)Timer_GetContext(timer);
    if (!screen) return;

    /* Effacer le message de statut */
    UILabel_SetText(screen->status_label, "");

    /* Réinitialiser les statuts d'application */
    for (int i = 0; i < NETWORK_PARAM_COUNT; i++) {
        if (screen->params[i].apply_status == APPLY_STATUS_SUCCESS ||
            screen->params[i].apply_status == APPLY_STATUS_FAILED) {
            screen->params[i].apply_status = APPLY_STATUS_IDLE;
        }
    }

    if (screen->base.is_visible) {
        UILabel_Draw(screen->status_label);
        update_freq_display(screen);
        update_power_display(screen);
        update_sync_display(screen);
    }
}

static void debounce_timer_callback(TimerHandle_t timer)
{
    /* Rien à faire, le timer sert juste de délai anti-rebond */
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */