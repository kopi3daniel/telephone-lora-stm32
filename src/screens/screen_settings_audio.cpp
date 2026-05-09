/**
 * @file    screen_settings_audio.cpp
 * @brief   Implémentation de l'écran des réglages audio
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente la configuration audio complète du téléphone LoRa.
 * 
 * FONCTIONNEMENT :
 * 
 * 1. MODIFICATION D'UN PARAMÈTRE :
 *    - L'utilisateur ajuste un slider ou bascule un switch
 *    - Le callback associé met à jour params[x].value
 *    - params[x].is_modified = true
 *    - Si le paramètre le permet, application immédiate (volume HP)
 *    - Sinon, attente du bouton [Appliquer]
 * 
 * 2. TEST AUDIO :
 *    - Test HP : génère une sinusoïde 1 kHz pendant 1 seconde
 *    - Test sonnerie : joue la mélodie sélectionnée
 *    - Test vibreur : active le PWM du moteur vibrant
 *    - VU-mètre : affiche le niveau micro en temps réel
 * 
 * 3. APPLICATION :
 *    - Appel à AudioManager pour chaque paramètre modifié
 *    - Sauvegarde dans SettingsService (flash)
 *    - Feedback visuel (message de statut)
 * 
 * ARCHITECTURE LOGICIELLE :
 * 
 *   ScreenSettingsAudio (UI)
 *        │
 *        ├──→ AudioManager (abstraction haut niveau)
 *        │        │
 *        │        ├──→ AudioADC    (capture micro, DMA)
 *        │        ├──→ AudioDAC    (lecture HP, DMA)
 *        │        ├──→ AudioMixer  (volumes, gain)
 *        │        └──→ AudioCodec  (ADPCM, filtres)
 *        │
 *        └──→ SettingsService (persistance flash)
 * 
 * GÉNÉRATION DE TONALITÉ DE TEST :
 * 
 * La sinusoïde de test est générée par une table précalculée
 * de 256 échantillons (1 cycle complet). La fréquence est
 * déterminée par le pas de lecture dans la table.
 * 
 *   f_out = (f_echantillonnage * pas) / taille_table
 * 
 *   Pour 1 kHz @ 8 kHz : pas = (1000 * 256) / 8000 = 32
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "screen_settings_audio.h"

/* UI */
#include "../ui/ui_core.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_widgets.h"
#include "../ui/ui_label.h"
#include "../ui/ui_button.h"
#include "../ui/ui_slider.h"
#include "../ui/ui_switch.h"
#include "../ui/ui_radio_group.h"
#include "../ui/ui_vu_meter.h"
#include "../ui/ui_dialog.h"
#include "../ui/ui_statusbar.h"
#include "../ui/ui_navigation.h"
#include "../ui/ui_icons.h"
#include "../ui/ui_animations.h"
#include "../ui/ui_draw_primitives.h"
#include "../ui/ui_fonts.h"

/* Services */
#include "../services/settings_service.h"

/* Drivers audio */
#include "../drivers/audio/audio_manager.h"
#include "../drivers/audio/audio_adc.h"
#include "../drivers/audio/audio_dac.h"
#include "../drivers/audio/audio_mixer.h"
#include "../drivers/audio/audio_codec_adpcm.h"

/* Drivers affichage */
#include "../drivers/display/display_manager.h"
#include "../drivers/display/dma2d_driver.h"

/* Utilitaires */
#include "../utils/string_utils.h"
#include "../utils/timer_utils.h"
#include "../utils/debug_utils.h"
#include "../utils/math_utils.h"

/* Standard */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs de debug */
#define TAG                                 "ScreenSettingsAudio"

/** Dimensions de l'écran */
#define SCREEN_WIDTH                        320
#define SCREEN_HEIGHT                       480

/** Zone de contenu */
#define CONTENT_Y_START                     60
#define CONTENT_X_MARGIN                    15
#define CONTENT_WIDTH                       (SCREEN_WIDTH - 2 * CONTENT_X_MARGIN)

/** Hauteur d'une ligne de paramètre */
#define PARAM_ROW_HEIGHT                    52

/** Positions Y des paramètres */
#define PARAM_MIC_VOL_Y                     (CONTENT_Y_START)
#define PARAM_SPEAKER_VOL_Y                 (PARAM_MIC_VOL_Y + PARAM_ROW_HEIGHT + 10)
#define PARAM_RINGTONE_VOL_Y                (PARAM_SPEAKER_VOL_Y + PARAM_ROW_HEIGHT + 10)
#define PARAM_SILENT_Y                      (PARAM_RINGTONE_VOL_Y + PARAM_ROW_HEIGHT + 10)
#define PARAM_VIBRATOR_Y                    (PARAM_SILENT_Y + 38)
#define PARAM_AUDIO_MODE_Y                  (PARAM_VIBRATOR_Y + 38)
#define PARAM_NR_Y                          (PARAM_AUDIO_MODE_Y + PARAM_ROW_HEIGHT + 5)
#define PARAM_MIC_GAIN_Y                    (PARAM_NR_Y + PARAM_ROW_HEIGHT)

/** Positions des boutons du bas */
#define BOTTOM_BUTTONS_Y                    425
#define APPLY_BUTTON_X                      15
#define APPLY_BUTTON_WIDTH                  140
#define DEFAULTS_BUTTON_X                   165
#define DEFAULTS_BUTTON_WIDTH               140

/** Dimensions des sliders */
#define SLIDER_X                            80
#define SLIDER_WIDTH                        175
#define SLIDER_HEIGHT                       18

/** Paramètres de test audio */
#define TEST_FREQUENCY_HZ                   1000    /* 1 kHz */
#define TEST_DURATION_MS                    1000    /* 1 seconde */
#define SAMPLE_RATE_HZ                      8000    /* 8 kHz */
#define SINE_TABLE_SIZE                     256     /* Échantillons par cycle */

/** Timeout messages statut */
#define STATUS_TIMEOUT_MS                   3000

/** Intervalle rafraîchissement VU-mètre */
#define VU_METER_REFRESH_MS                 50      /* 20 FPS */

/* ======================================================================== */
/*                VARIABLES STATIQUES (OPTIONS)                             */
/* ======================================================================== */

/** Labels des modes audio */
static const char* AUDIO_MODE_OPTIONS[] = {
    "Haut-parleur",
    "Ecouteur",
    "Casque"
};

/** Labels des niveaux de réduction de bruit */
static const char* NOISE_REDUCTION_OPTIONS[] = {
    "Desactivee",
    "Faible",
    "Moyen",
    "Fort"
};

/** Table sinusoïdale précalculée pour les tests */
static const int16_t SINE_TABLE[SINE_TABLE_SIZE] = {
    /* Générée par le script tools/generate_sine_table.py */
    /* Amplitude : 2047 (12-bit DAC) */
    /* Fréquence : 1 cycle complet sur 256 échantillons */
    #include "../drivers/audio/sine_table_256.h"
};

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

/* --- Initialisation --- */
static void init_params(ScreenSettingsAudio_t* screen);
static void load_params_from_service(ScreenSettingsAudio_t* screen);
static void save_params_to_service(ScreenSettingsAudio_t* screen);

/* --- Création widgets --- */
static void create_mic_volume_widgets(ScreenSettingsAudio_t* screen);
static void create_speaker_volume_widgets(ScreenSettingsAudio_t* screen);
static void create_ringtone_volume_widgets(ScreenSettingsAudio_t* screen);
static void create_silent_mode_widgets(ScreenSettingsAudio_t* screen);
static void create_vibrator_widgets(ScreenSettingsAudio_t* screen);
static void create_audio_mode_widgets(ScreenSettingsAudio_t* screen);
static void create_noise_reduction_widgets(ScreenSettingsAudio_t* screen);
static void create_mic_gain_widgets(ScreenSettingsAudio_t* screen);

/* --- Callbacks widgets --- */
static void on_mic_volume_changed(void* context, int16_t value);
static void on_speaker_volume_changed(void* context, int16_t value);
static void on_ringtone_volume_changed(void* context, int16_t value);
static void on_silent_mode_changed(void* context, bool enabled);
static void on_vibrator_changed(void* context, bool enabled);
static void on_audio_mode_selected(void* context, uint8_t index);
static void on_noise_reduction_selected(void* context, uint8_t index);
static void on_mic_gain_changed(void* context, int16_t value);
static void on_test_speaker_clicked(void* context);
static void on_test_ringtone_clicked(void* context);
static void on_test_vibrator_clicked(void* context);
static void on_apply_clicked(void* context);
static void on_defaults_clicked(void* context);
static void on_back_clicked(void* context);

/* --- Application --- */
static void apply_mic_volume(ScreenSettingsAudio_t* screen);
static void apply_speaker_volume(ScreenSettingsAudio_t* screen);
static void apply_ringtone_volume(ScreenSettingsAudio_t* screen);
static void apply_silent_mode(ScreenSettingsAudio_t* screen);
static void apply_vibrator(ScreenSettingsAudio_t* screen);
static void apply_audio_mode(ScreenSettingsAudio_t* screen);
static void apply_noise_reduction(ScreenSettingsAudio_t* screen);
static void apply_mic_gain(ScreenSettingsAudio_t* screen);

/* --- Tests audio --- */
static void generate_test_tone(int16_t* buffer, uint16_t samples);
static void play_test_tone(ScreenSettingsAudio_t* screen);
static void stop_test_tone(ScreenSettingsAudio_t* screen);

/* --- Rendu --- */
static void draw_param_row(int16_t y, const char* label, bool selected);
static void update_vu_meter(ScreenSettingsAudio_t* screen);
static void update_status_message(ScreenSettingsAudio_t* screen, const char* msg);

/* --- Timers --- */
static void vu_meter_timer_callback(TimerHandle_t timer);
static void test_timer_callback(TimerHandle_t timer);
static void status_timer_callback(TimerHandle_t timer);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise l'écran des réglages audio
 */
bool ScreenSettingsAudio_Init(ScreenSettingsAudio_t* screen,
                              SettingsService_t* settings_service)
{
    if (!screen || !settings_service) {
        DEBUG_ERROR(TAG, "Paramètres invalides");
        return false;
    }

    DEBUG_INFO(TAG, "Initialisation de l'écran audio...");

    /* Mise à zéro */
    memset(screen, 0, sizeof(ScreenSettingsAudio_t));

    /* Classe de base */
    ScreenBase_Init(&screen->base, SCREEN_ID_SETTINGS_AUDIO, "Audio");

    /* Services */
    screen->settings_service = settings_service;
    screen->audio_manager = AudioManager_GetInstance();
    if (!screen->audio_manager) {
        DEBUG_ERROR(TAG, "AudioManager non disponible !");
        return false;
    }

    /* État initial */
    screen->state = AUDIO_STATE_IDLE;
    screen->selected_row = -1;

    /* ---- Widgets de base ---- */

    /* Barre de statut */
    UIStatusBar_Init(&screen->status_bar);

    /* Titre */
    screen->title_label = UILabel_Create();
    UILabel_SetText(screen->title_label, "Audio");
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
    create_mic_volume_widgets(screen);
    create_speaker_volume_widgets(screen);
    create_ringtone_volume_widgets(screen);
    create_silent_mode_widgets(screen);
    create_vibrator_widgets(screen);
    create_audio_mode_widgets(screen);
    create_noise_reduction_widgets(screen);
    create_mic_gain_widgets(screen);

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
    screen->vu_meter_timer = Timer_Create("VuMeter",
                                          VU_METER_REFRESH_MS,
                                          true,  /* auto-reload */
                                          vu_meter_timer_callback,
                                          screen);
    screen->test_timer = Timer_Create("AudioTest",
                                      TEST_DURATION_MS,
                                      false,  /* one-shot */
                                      test_timer_callback,
                                      screen);

    DEBUG_INFO(TAG, "Initialisation terminée");

    return true;
}

/**
 * @brief Affiche l'écran audio
 */
void ScreenSettingsAudio_Show(ScreenSettingsAudio_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Affichage de l'écran audio");

    screen->state = AUDIO_STATE_IDLE;
    screen->selected_row = -1;

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

    /* 1. Volume microphone + VU-mètre */
    draw_param_row(PARAM_MIC_VOL_Y, "VOLUME MICROPHONE", false);
    UISlider_Draw(screen->mic_volume_slider);
    UILabel_Draw(screen->value_labels[AUDIO_PARAM_MIC_VOLUME]);
    if (screen->mic_vu_meter) {
        UIVuMeter_Draw(screen->mic_vu_meter);
    }

    /* Activation du VU-mètre */
    ScreenSettingsAudio_SetVuMeterActive(screen, true);

    /* 2. Volume haut-parleur */
    draw_param_row(PARAM_SPEAKER_VOL_Y, "VOLUME HAUT-PARLEUR", false);
    UISlider_Draw(screen->speaker_volume_slider);
    UILabel_Draw(screen->value_labels[AUDIO_PARAM_SPEAKER_VOLUME]);
    UIButton_Draw(screen->test_speaker_button);

    /* 3. Volume sonnerie */
    draw_param_row(PARAM_RINGTONE_VOL_Y, "VOLUME SONNERIE", false);
    UISlider_Draw(screen->ringtone_volume_slider);
    UILabel_Draw(screen->value_labels[AUDIO_PARAM_RINGTONE_VOLUME]);
    UIButton_Draw(screen->test_ringtone_button);

    /* 4. Mode silence */
    draw_param_row(PARAM_SILENT_Y, "MODE SILENCE", false);
    UISwitch_Draw(screen->silent_mode_switch);

    /* 5. Vibreur */
    draw_param_row(PARAM_VIBRATOR_Y, "VIBREUR", false);
    UISwitch_Draw(screen->vibrator_switch);
    UIButton_Draw(screen->test_vibrator_button);

    /* 6. Mode audio */
    draw_param_row(PARAM_AUDIO_MODE_Y, "MODE AUDIO", false);
    UIRadioGroup_Draw(screen->audio_mode_radio);

    /* 7. Réduction de bruit */
    draw_param_row(PARAM_NR_Y, "REDUCTION DE BRUIT", false);
    UIRadioGroup_Draw(screen->noise_reduction_radio);

    /* 8. Gain micro */
    draw_param_row(PARAM_MIC_GAIN_Y, "GAIN MICRO", false);
    UISlider_Draw(screen->mic_gain_slider);
    UILabel_Draw(screen->value_labels[AUDIO_PARAM_MIC_GAIN]);

    /* Boutons du bas */
    Display_DrawHLine(0, BOTTOM_BUTTONS_Y - 8, SCREEN_WIDTH, THEME_DIVIDER);
    UIButton_Draw(screen->apply_button);
    UIButton_Draw(screen->defaults_button);

    /* Label statut */
    UILabel_Draw(screen->status_label);

    screen->base.is_visible = true;
}

/**
 * @brief Masque l'écran audio
 */
void ScreenSettingsAudio_Hide(ScreenSettingsAudio_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Masquage de l'écran audio");

    /* Arrêter le VU-mètre */
    ScreenSettingsAudio_SetVuMeterActive(screen, false);

    /* Arrêter tout test en cours */
    if (screen->state == AUDIO_STATE_TESTING_SPEAKER ||
        screen->state == AUDIO_STATE_TESTING_RINGTONE) {
        stop_test_tone(screen);
    }

    Timer_Stop(screen->vu_meter_timer);
    Timer_Stop(screen->test_timer);

    screen->base.is_visible = false;
}

/**
 * @brief Mise à jour périodique
 */
void ScreenSettingsAudio_Update(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->base.is_visible) return;

    UIStatusBar_Update(&screen->status_bar);
}

/**
 * @brief Gère les événements tactiles
 */
bool ScreenSettingsAudio_HandleTouch(ScreenSettingsAudio_t* screen,
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

    /* Sliders */
    if (UISlider_HitTest(screen->mic_volume_slider, event->x, event->y)) {
        return UISlider_HandleTouch(screen->mic_volume_slider, event);
    }
    if (UISlider_HitTest(screen->speaker_volume_slider, event->x, event->y)) {
        return UISlider_HandleTouch(screen->speaker_volume_slider, event);
    }
    if (UISlider_HitTest(screen->ringtone_volume_slider, event->x, event->y)) {
        return UISlider_HandleTouch(screen->ringtone_volume_slider, event);
    }
    if (UISlider_HitTest(screen->mic_gain_slider, event->x, event->y)) {
        return UISlider_HandleTouch(screen->mic_gain_slider, event);
    }

    /* Switches */
    if (UISwitch_HitTest(screen->silent_mode_switch, event->x, event->y)) {
        return UISwitch_HandleTouch(screen->silent_mode_switch, event);
    }
    if (UISwitch_HitTest(screen->vibrator_switch, event->x, event->y)) {
        return UISwitch_HandleTouch(screen->vibrator_switch, event);
    }

    /* Radio groups */
    if (UIRadioGroup_HitTest(screen->audio_mode_radio, event->x, event->y)) {
        return UIRadioGroup_HandleTouch(screen->audio_mode_radio, event);
    }
    if (UIRadioGroup_HitTest(screen->noise_reduction_radio, event->x, event->y)) {
        return UIRadioGroup_HandleTouch(screen->noise_reduction_radio, event);
    }

    /* Boutons de test */
    if (UIButton_HitTest(screen->test_speaker_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->test_speaker_button);
        }
        return true;
    }
    if (UIButton_HitTest(screen->test_ringtone_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->test_ringtone_button);
        }
        return true;
    }
    if (UIButton_HitTest(screen->test_vibrator_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->test_vibrator_button);
        }
        return true;
    }

    return false;
}

/**
 * @brief Gère les touches physiques
 */
bool ScreenSettingsAudio_HandleKey(ScreenSettingsAudio_t* screen,
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
            if (screen->selected_row < AUDIO_PARAM_COUNT - 1) {
                screen->selected_row++;
            }
            return true;

        case KEY_LEFT:
            /* Décrémenter le paramètre sélectionné */
            return true;

        case KEY_RIGHT:
            /* Incrémenter le paramètre sélectionné */
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
uint8_t ScreenSettingsAudio_ApplyParams(ScreenSettingsAudio_t* screen)
{
    if (!screen) return 0;

    DEBUG_INFO(TAG, "Application des paramètres audio...");

    screen->state = AUDIO_STATE_APPLYING;
    uint8_t count = 0;

    /* Appliquer chaque paramètre modifié */
    for (int i = 0; i < AUDIO_PARAM_COUNT; i++) {
        if (!screen->params[i].is_modified) {
            continue;
        }

        switch (screen->params[i].id) {
            case AUDIO_PARAM_MIC_VOLUME:
                apply_mic_volume(screen);
                break;
            case AUDIO_PARAM_SPEAKER_VOLUME:
                apply_speaker_volume(screen);
                break;
            case AUDIO_PARAM_RINGTONE_VOLUME:
                apply_ringtone_volume(screen);
                break;
            case AUDIO_PARAM_SILENT_MODE:
                apply_silent_mode(screen);
                break;
            case AUDIO_PARAM_VIBRATOR:
                apply_vibrator(screen);
                break;
            case AUDIO_PARAM_AUDIO_MODE:
                apply_audio_mode(screen);
                break;
            case AUDIO_PARAM_NOISE_REDUCTION:
                apply_noise_reduction(screen);
                break;
            case AUDIO_PARAM_MIC_GAIN:
                apply_mic_gain(screen);
                break;
            default:
                continue;
        }

        screen->params[i].is_modified = false;
        count++;
    }

    /* Sauvegarder en flash */
    save_params_to_service(screen);

    screen->state = AUDIO_STATE_IDLE;

    if (count > 0) {
        update_status_message(screen, "Parametres audio appliques");
    } else {
        update_status_message(screen, "Aucune modification");
    }

    DEBUG_INFO(TAG, "%d paramètres appliqués", count);

    return count;
}

/**
 * @brief Restaure les valeurs par défaut
 */
void ScreenSettingsAudio_RestoreDefaults(ScreenSettingsAudio_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Restauration des valeurs par défaut audio");

    /* Appliquer les défauts */
    for (int i = 0; i < AUDIO_PARAM_COUNT; i++) {
        screen->params[i].value.percentage = screen->params[i].default_value;
        screen->params[i].is_modified = true;
    }

    /* Cas spéciaux */
    screen->params[AUDIO_PARAM_SILENT_MODE].value.boolean = false;
    screen->params[AUDIO_PARAM_VIBRATOR].value.boolean = true;
    screen->params[AUDIO_PARAM_AUDIO_MODE].value.mode_index = AUDIO_MODE_SPEAKER;
    screen->params[AUDIO_PARAM_NOISE_REDUCTION].value.nr_level = NOISE_REDUCTION_OFF;

    /* Appliquer */
    ScreenSettingsAudio_ApplyParams(screen);

    /* Mettre à jour l'affichage */
    if (screen->base.is_visible) {
        UISlider_SetValue(screen->mic_volume_slider, 80);
        UISlider_SetValue(screen->speaker_volume_slider, 75);
        UISlider_SetValue(screen->ringtone_volume_slider, 85);
        UISlider_SetValue(screen->mic_gain_slider, 60);
        UISwitch_SetState(screen->silent_mode_switch, false);
        UISwitch_SetState(screen->vibrator_switch, true);
        UIRadioGroup_SetSelected(screen->audio_mode_radio, AUDIO_MODE_SPEAKER);
        UIRadioGroup_SetSelected(screen->noise_reduction_radio, NOISE_REDUCTION_OFF);

        /* Redessiner */
        ScreenSettingsAudio_Show(screen);
        update_status_message(screen, "Valeurs par defaut restaurees");
    }
}

/**
 * @brief Joue un son de test dans le haut-parleur
 */
void ScreenSettingsAudio_TestSpeaker(ScreenSettingsAudio_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Test haut-parleur démarré");

    screen->state = AUDIO_STATE_TESTING_SPEAKER;
    play_test_tone(screen);
    Timer_Start(screen->test_timer);

    update_status_message(screen, "Test HP en cours...");
}

/**
 * @brief Joue la sonnerie sélectionnée
 */
void ScreenSettingsAudio_TestRingtone(ScreenSettingsAudio_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Test sonnerie démarré");

    screen->state = AUDIO_STATE_TESTING_RINGTONE;

    /* Utiliser la sonnerie configurée */
    AudioManager_PlayRingtone(screen->audio_manager);
    Timer_Start(screen->test_timer);

    update_status_message(screen, "Ecoute de la melodie...");
}

/**
 * @brief Active le vibreur pour test
 */
void ScreenSettingsAudio_TestVibrator(ScreenSettingsAudio_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Test vibreur démarré");

    screen->state = AUDIO_STATE_TESTING_VIBRATOR;

    /* Activer le vibreur */
    AudioManager_SetVibrator(screen->audio_manager, true);
    Timer_Start(screen->test_timer);

    update_status_message(screen, "Test vibreur en cours...");
}

/**
 * @brief Active/désactive le VU-mètre
 */
void ScreenSettingsAudio_SetVuMeterActive(ScreenSettingsAudio_t* screen,
                                          bool active)
{
    if (!screen) return;

    if (active) {
        screen->state = AUDIO_STATE_VU_METER_ACTIVE;
        Timer_Start(screen->vu_meter_timer);
        DEBUG_VERBOSE(TAG, "VU-mètre activé");
    } else {
        if (screen->state == AUDIO_STATE_VU_METER_ACTIVE) {
            screen->state = AUDIO_STATE_IDLE;
        }
        Timer_Stop(screen->vu_meter_timer);
        DEBUG_VERBOSE(TAG, "VU-mètre désactivé");
    }
}

/**
 * @brief Setters individuels
 */
void ScreenSettingsAudio_SetMicVolume(ScreenSettingsAudio_t* screen, uint8_t volume)
{
    if (!screen) return;
    volume = CLAMP(volume, 0, 100);
    screen->params[AUDIO_PARAM_MIC_VOLUME].value.percentage = volume;
    screen->params[AUDIO_PARAM_MIC_VOLUME].is_modified = true;
    if (screen->base.is_visible) {
        UISlider_SetValue(screen->mic_volume_slider, volume);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", volume);
        UILabel_SetText(screen->value_labels[AUDIO_PARAM_MIC_VOLUME], buf);
        UILabel_Draw(screen->value_labels[AUDIO_PARAM_MIC_VOLUME]);
    }
    /* Application immédiate */
    apply_mic_volume(screen);
}

void ScreenSettingsAudio_SetSpeakerVolume(ScreenSettingsAudio_t* screen, uint8_t volume)
{
    if (!screen) return;
    volume = CLAMP(volume, 0, 100);
    screen->params[AUDIO_PARAM_SPEAKER_VOLUME].value.percentage = volume;
    screen->params[AUDIO_PARAM_SPEAKER_VOLUME].is_modified = true;
    if (screen->base.is_visible) {
        UISlider_SetValue(screen->speaker_volume_slider, volume);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", volume);
        UILabel_SetText(screen->value_labels[AUDIO_PARAM_SPEAKER_VOLUME], buf);
        UILabel_Draw(screen->value_labels[AUDIO_PARAM_SPEAKER_VOLUME]);
    }
    /* Application immédiate */
    apply_speaker_volume(screen);
}

void ScreenSettingsAudio_SetRingtoneVolume(ScreenSettingsAudio_t* screen, uint8_t volume)
{
    if (!screen) return;
    volume = CLAMP(volume, 0, 100);
    screen->params[AUDIO_PARAM_RINGTONE_VOLUME].value.percentage = volume;
    screen->params[AUDIO_PARAM_RINGTONE_VOLUME].is_modified = true;
    if (screen->base.is_visible) {
        UISlider_SetValue(screen->ringtone_volume_slider, volume);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", volume);
        UILabel_SetText(screen->value_labels[AUDIO_PARAM_RINGTONE_VOLUME], buf);
        UILabel_Draw(screen->value_labels[AUDIO_PARAM_RINGTONE_VOLUME]);
    }
}

void ScreenSettingsAudio_SetSilentMode(ScreenSettingsAudio_t* screen, bool silent)
{
    if (!screen) return;
    screen->params[AUDIO_PARAM_SILENT_MODE].value.boolean = silent;
    screen->params[AUDIO_PARAM_SILENT_MODE].is_modified = true;
    if (screen->base.is_visible) {
        UISwitch_SetState(screen->silent_mode_switch, silent);
        UISwitch_Draw(screen->silent_mode_switch);
    }
    /* Application immédiate */
    apply_silent_mode(screen);
}

void ScreenSettingsAudio_SetVibrator(ScreenSettingsAudio_t* screen, bool enabled)
{
    if (!screen) return;
    screen->params[AUDIO_PARAM_VIBRATOR].value.boolean = enabled;
    screen->params[AUDIO_PARAM_VIBRATOR].is_modified = true;
    if (screen->base.is_visible) {
        UISwitch_SetState(screen->vibrator_switch, enabled);
        UISwitch_Draw(screen->vibrator_switch);
    }
}

void ScreenSettingsAudio_SetAudioMode(ScreenSettingsAudio_t* screen, AudioOutputMode_t mode)
{
    if (!screen || mode >= AUDIO_MODE_COUNT) return;
    screen->params[AUDIO_PARAM_AUDIO_MODE].value.mode_index = (uint8_t)mode;
    screen->params[AUDIO_PARAM_AUDIO_MODE].is_modified = true;
    if (screen->base.is_visible) {
        UIRadioGroup_SetSelected(screen->audio_mode_radio, mode);
        UIRadioGroup_Draw(screen->audio_mode_radio);
    }
}

void ScreenSettingsAudio_SetNoiseReduction(ScreenSettingsAudio_t* screen, NoiseReductionLevel_t level)
{
    if (!screen || level >= NOISE_REDUCTION_COUNT) return;
    screen->params[AUDIO_PARAM_NOISE_REDUCTION].value.nr_level = (uint8_t)level;
    screen->params[AUDIO_PARAM_NOISE_REDUCTION].is_modified = true;
    if (screen->base.is_visible) {
        UIRadioGroup_SetSelected(screen->noise_reduction_radio, level);
        UIRadioGroup_Draw(screen->noise_reduction_radio);
    }
}

void ScreenSettingsAudio_SetMicGain(ScreenSettingsAudio_t* screen, uint8_t gain)
{
    if (!screen) return;
    gain = CLAMP(gain, 0, 100);
    screen->params[AUDIO_PARAM_MIC_GAIN].value.percentage = gain;
    screen->params[AUDIO_PARAM_MIC_GAIN].is_modified = true;
    if (screen->base.is_visible) {
        UISlider_SetValue(screen->mic_gain_slider, gain);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", gain);
        UILabel_SetText(screen->value_labels[AUDIO_PARAM_MIC_GAIN], buf);
        UILabel_Draw(screen->value_labels[AUDIO_PARAM_MIC_GAIN]);
    }
}

/**
 * @brief Libère les ressources
 */
void ScreenSettingsAudio_Deinit(ScreenSettingsAudio_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Libération des ressources audio");

    Timer_Delete(screen->vu_meter_timer);
    Timer_Delete(screen->test_timer);

    UILabel_Destroy(screen->title_label);
    UILabel_Destroy(screen->status_label);
    UIButton_Destroy(screen->back_button);
    UIButton_Destroy(screen->apply_button);
    UIButton_Destroy(screen->defaults_button);
    UIButton_Destroy(screen->test_speaker_button);
    UIButton_Destroy(screen->test_ringtone_button);
    UIButton_Destroy(screen->test_vibrator_button);

    UISlider_Destroy(screen->mic_volume_slider);
    UISlider_Destroy(screen->speaker_volume_slider);
    UISlider_Destroy(screen->ringtone_volume_slider);
    UISlider_Destroy(screen->mic_gain_slider);

    if (screen->mic_vu_meter) {
        UIVuMeter_Destroy(screen->mic_vu_meter);
    }

    UISwitch_Destroy(screen->silent_mode_switch);
    UISwitch_Destroy(screen->vibrator_switch);

    UIRadioGroup_Destroy(screen->audio_mode_radio);
    UIRadioGroup_Destroy(screen->noise_reduction_radio);

    UIDialog_Destroy(screen->confirm_dialog);

    for (int i = 0; i < AUDIO_PARAM_COUNT; i++) {
        UILabel_Destroy(screen->param_labels[i]);
        UILabel_Destroy(screen->value_labels[i]);
    }

    memset(screen, 0, sizeof(ScreenSettingsAudio_t));
}

/* ======================================================================== */
/*              INITIALISATION DES PARAMÈTRES                               */
/* ======================================================================== */

static void init_params(ScreenSettingsAudio_t* screen)
{
    if (!screen) return;

    /* Paramètre 0 : Volume micro */
    screen->params[0].id = AUDIO_PARAM_MIC_VOLUME;
    screen->params[0].name = "Volume micro";
    screen->params[0].unit = "%";
    screen->params[0].min_value = 0;
    screen->params[0].max_value = 100;
    screen->params[0].default_value = 80;

    /* Paramètre 1 : Volume haut-parleur */
    screen->params[1].id = AUDIO_PARAM_SPEAKER_VOLUME;
    screen->params[1].name = "Volume HP";
    screen->params[1].unit = "%";
    screen->params[1].min_value = 0;
    screen->params[1].max_value = 100;
    screen->params[1].default_value = 75;

    /* Paramètre 2 : Volume sonnerie */
    screen->params[2].id = AUDIO_PARAM_RINGTONE_VOLUME;
    screen->params[2].name = "Volume sonnerie";
    screen->params[2].unit = "%";
    screen->params[2].min_value = 0;
    screen->params[2].max_value = 100;
    screen->params[2].default_value = 85;

    /* Paramètre 3 : Mode silence */
    screen->params[3].id = AUDIO_PARAM_SILENT_MODE;
    screen->params[3].name = "Mode silence";
    screen->params[3].unit = "";
    screen->params[3].default_value = 0;  /* OFF */

    /* Paramètre 4 : Vibreur */
    screen->params[4].id = AUDIO_PARAM_VIBRATOR;
    screen->params[4].name = "Vibreur";
    screen->params[4].unit = "";
    screen->params[4].default_value = 1;  /* ON */

    /* Paramètre 5 : Mode audio */
    screen->params[5].id = AUDIO_PARAM_AUDIO_MODE;
    screen->params[5].name = "Mode audio";
    screen->params[5].unit = "";
    screen->params[5].options = AUDIO_MODE_OPTIONS;
    screen->params[5].option_count = AUDIO_MODE_COUNT;
    screen->params[5].default_value = AUDIO_MODE_SPEAKER;

    /* Paramètre 6 : Réduction de bruit */
    screen->params[6].id = AUDIO_PARAM_NOISE_REDUCTION;
    screen->params[6].name = "Réduction bruit";
    screen->params[6].unit = "";
    screen->params[6].options = NOISE_REDUCTION_OPTIONS;
    screen->params[6].option_count = NOISE_REDUCTION_COUNT;
    screen->params[6].default_value = NOISE_REDUCTION_OFF;

    /* Paramètre 7 : Gain micro */
    screen->params[7].id = AUDIO_PARAM_MIC_GAIN;
    screen->params[7].name = "Gain micro";
    screen->params[7].unit = "%";
    screen->params[7].min_value = 0;
    screen->params[7].max_value = 100;
    screen->params[7].default_value = 60;

    /* Initialiser les flags */
    for (int i = 0; i < AUDIO_PARAM_COUNT; i++) {
        screen->params[i].is_modified = false;
        screen->params[i].widget = NULL;
    }
}

static void load_params_from_service(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->settings_service) return;

    SettingsService_t* svc = screen->settings_service;

    uint8_t vol;
    if (SettingsService_GetMicVolume(svc, &vol))
        screen->params[AUDIO_PARAM_MIC_VOLUME].value.percentage = vol;
    else
        screen->params[AUDIO_PARAM_MIC_VOLUME].value.percentage = 80;

    if (SettingsService_GetSpeakerVolume(svc, &vol))
        screen->params[AUDIO_PARAM_SPEAKER_VOLUME].value.percentage = vol;
    else
        screen->params[AUDIO_PARAM_SPEAKER_VOLUME].value.percentage = 75;

    if (SettingsService_GetRingtoneVolume(svc, &vol))
        screen->params[AUDIO_PARAM_RINGTONE_VOLUME].value.percentage = vol;
    else
        screen->params[AUDIO_PARAM_RINGTONE_VOLUME].value.percentage = 85;

    bool flag;
    if (SettingsService_GetSilentMode(svc, &flag))
        screen->params[AUDIO_PARAM_SILENT_MODE].value.boolean = flag;
    else
        screen->params[AUDIO_PARAM_SILENT_MODE].value.boolean = false;

    if (SettingsService_GetVibratorEnabled(svc, &flag))
        screen->params[AUDIO_PARAM_VIBRATOR].value.boolean = flag;
    else
        screen->params[AUDIO_PARAM_VIBRATOR].value.boolean = true;

    uint8_t mode;
    if (SettingsService_GetAudioMode(svc, &mode))
        screen->params[AUDIO_PARAM_AUDIO_MODE].value.mode_index = mode;
    else
        screen->params[AUDIO_PARAM_AUDIO_MODE].value.mode_index = AUDIO_MODE_SPEAKER;

    uint8_t nr;
    if (SettingsService_GetNoiseReduction(svc, &nr))
        screen->params[AUDIO_PARAM_NOISE_REDUCTION].value.nr_level = nr;
    else
        screen->params[AUDIO_PARAM_NOISE_REDUCTION].value.nr_level = NOISE_REDUCTION_OFF;

    uint8_t gain;
    if (SettingsService_GetMicGain(svc, &gain))
        screen->params[AUDIO_PARAM_MIC_GAIN].value.percentage = gain;
    else
        screen->params[AUDIO_PARAM_MIC_GAIN].value.percentage = 60;

    /* Mettre à jour les widgets */
    if (screen->mic_volume_slider)
        UISlider_SetValue(screen->mic_volume_slider, screen->params[AUDIO_PARAM_MIC_VOLUME].value.percentage);
    if (screen->speaker_volume_slider)
        UISlider_SetValue(screen->speaker_volume_slider, screen->params[AUDIO_PARAM_SPEAKER_VOLUME].value.percentage);
    if (screen->ringtone_volume_slider)
        UISlider_SetValue(screen->ringtone_volume_slider, screen->params[AUDIO_PARAM_RINGTONE_VOLUME].value.percentage);
    if (screen->silent_mode_switch)
        UISwitch_SetState(screen->silent_mode_switch, screen->params[AUDIO_PARAM_SILENT_MODE].value.boolean);
    if (screen->vibrator_switch)
        UISwitch_SetState(screen->vibrator_switch, screen->params[AUDIO_PARAM_VIBRATOR].value.boolean);
    if (screen->audio_mode_radio)
        UIRadioGroup_SetSelected(screen->audio_mode_radio, screen->params[AUDIO_PARAM_AUDIO_MODE].value.mode_index);
    if (screen->noise_reduction_radio)
        UIRadioGroup_SetSelected(screen->noise_reduction_radio, screen->params[AUDIO_PARAM_NOISE_REDUCTION].value.nr_level);
    if (screen->mic_gain_slider)
        UISlider_SetValue(screen->mic_gain_slider, screen->params[AUDIO_PARAM_MIC_GAIN].value.percentage);
}

static void save_params_to_service(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->settings_service) return;

    SettingsService_t* svc = screen->settings_service;

    SettingsService_SetMicVolume(svc, screen->params[AUDIO_PARAM_MIC_VOLUME].value.percentage);
    SettingsService_SetSpeakerVolume(svc, screen->params[AUDIO_PARAM_SPEAKER_VOLUME].value.percentage);
    SettingsService_SetRingtoneVolume(svc, screen->params[AUDIO_PARAM_RINGTONE_VOLUME].value.percentage);
    SettingsService_SetSilentMode(svc, screen->params[AUDIO_PARAM_SILENT_MODE].value.boolean);
    SettingsService_SetVibratorEnabled(svc, screen->params[AUDIO_PARAM_VIBRATOR].value.boolean);
    SettingsService_SetAudioMode(svc, screen->params[AUDIO_PARAM_AUDIO_MODE].value.mode_index);
    SettingsService_SetNoiseReduction(svc, screen->params[AUDIO_PARAM_NOISE_REDUCTION].value.nr_level);
    SettingsService_SetMicGain(svc, screen->params[AUDIO_PARAM_MIC_GAIN].value.percentage);

    SettingsService_Save(svc);
}

/* ======================================================================== */
/*              CRÉATION DES WIDGETS                                        */
/* ======================================================================== */

static void create_mic_volume_widgets(ScreenSettingsAudio_t* screen)
{
    /* Slider */
    screen->mic_volume_slider = UISlider_Create();
    UISlider_SetPosition(screen->mic_volume_slider, SLIDER_X, PARAM_MIC_VOL_Y + 20);
    UISlider_SetSize(screen->mic_volume_slider, SLIDER_WIDTH, SLIDER_HEIGHT);
    UISlider_SetRange(screen->mic_volume_slider, 0, 100);
    UISlider_SetOnChanged(screen->mic_volume_slider, on_mic_volume_changed, screen);
    UISlider_SetColor(screen->mic_volume_slider, THEME_ACCENT);

    /* Label valeur */
    screen->value_labels[AUDIO_PARAM_MIC_VOLUME] = UILabel_Create();
    UILabel_SetFont(screen->value_labels[AUDIO_PARAM_MIC_VOLUME], &font_medium_bold);
    UILabel_SetColor(screen->value_labels[AUDIO_PARAM_MIC_VOLUME], THEME_TEXT_PRIMARY);
    UILabel_SetPosition(screen->value_labels[AUDIO_PARAM_MIC_VOLUME],
                        SLIDER_X + SLIDER_WIDTH + 8, PARAM_MIC_VOL_Y + 18);

    /* VU-mètre */
    screen->mic_vu_meter = UIVuMeter_Create();
    UIVuMeter_SetPosition(screen->mic_vu_meter, SLIDER_X, PARAM_MIC_VOL_Y + 42);
    UIVuMeter_SetSize(screen->mic_vu_meter, SLIDER_WIDTH, 8);
    UIVuMeter_SetColor(screen->mic_vu_meter, THEME_SUCCESS);
    UIVuMeter_SetPeakColor(screen->mic_vu_meter, THEME_WARNING);
    UIVuMeter_SetClipColor(screen->mic_vu_meter, THEME_DANGER);
}

static void create_speaker_volume_widgets(ScreenSettingsAudio_t* screen)
{
    screen->speaker_volume_slider = UISlider_Create();
    UISlider_SetPosition(screen->speaker_volume_slider, SLIDER_X, PARAM_SPEAKER_VOL_Y + 20);
    UISlider_SetSize(screen->speaker_volume_slider, SLIDER_WIDTH - 60, SLIDER_HEIGHT);
    UISlider_SetRange(screen->speaker_volume_slider, 0, 100);
    UISlider_SetOnChanged(screen->speaker_volume_slider, on_speaker_volume_changed, screen);
    UISlider_SetColor(screen->speaker_volume_slider, THEME_INFO);

    screen->value_labels[AUDIO_PARAM_SPEAKER_VOLUME] = UILabel_Create();
    UILabel_SetFont(screen->value_labels[AUDIO_PARAM_SPEAKER_VOLUME], &font_medium_bold);
    UILabel_SetColor(screen->value_labels[AUDIO_PARAM_SPEAKER_VOLUME], THEME_TEXT_PRIMARY);
    UILabel_SetPosition(screen->value_labels[AUDIO_PARAM_SPEAKER_VOLUME],
                        SLIDER_X + SLIDER_WIDTH - 50, PARAM_SPEAKER_VOL_Y + 18);

    /* Bouton test */
    screen->test_speaker_button = UIButton_Create();
    UIButton_SetText(screen->test_speaker_button, "▶");
    UIButton_SetFont(screen->test_speaker_button, &font_medium);
    UIButton_SetSize(screen->test_speaker_button, 40, 28);
    UIButton_SetPosition(screen->test_speaker_button, SCREEN_WIDTH - 55, PARAM_SPEAKER_VOL_Y + 16);
    UIButton_SetOnClick(screen->test_speaker_button, on_test_speaker_clicked, screen);
    UIButton_SetCornerRadius(screen->test_speaker_button, 5);
    UIButton_SetColor(screen->test_speaker_button, THEME_SUCCESS);
}

static void create_ringtone_volume_widgets(ScreenSettingsAudio_t* screen)
{
    screen->ringtone_volume_slider = UISlider_Create();
    UISlider_SetPosition(screen->ringtone_volume_slider, SLIDER_X, PARAM_RINGTONE_VOL_Y + 20);
    UISlider_SetSize(screen->ringtone_volume_slider, SLIDER_WIDTH - 60, SLIDER_HEIGHT);
    UISlider_SetRange(screen->ringtone_volume_slider, 0, 100);
    UISlider_SetOnChanged(screen->ringtone_volume_slider, on_ringtone_volume_changed, screen);
    UISlider_SetColor(screen->ringtone_volume_slider, THEME_ACCENT);

    screen->value_labels[AUDIO_PARAM_RINGTONE_VOLUME] = UILabel_Create();
    UILabel_SetFont(screen->value_labels[AUDIO_PARAM_RINGTONE_VOLUME], &font_medium_bold);
    UILabel_SetColor(screen->value_labels[AUDIO_PARAM_RINGTONE_VOLUME], THEME_TEXT_PRIMARY);
    UILabel_SetPosition(screen->value_labels[AUDIO_PARAM_RINGTONE_VOLUME],
                        SLIDER_X + SLIDER_WIDTH - 50, PARAM_RINGTONE_VOL_Y + 18);

    /* Bouton test */
    screen->test_ringtone_button = UIButton_Create();
    UIButton_SetText(screen->test_ringtone_button, "♪");
    UIButton_SetFont(screen->test_ringtone_button, &font_medium);
    UIButton_SetSize(screen->test_ringtone_button, 40, 28);
    UIButton_SetPosition(screen->test_ringtone_button, SCREEN_WIDTH - 55, PARAM_RINGTONE_VOL_Y + 16);
    UIButton_SetOnClick(screen->test_ringtone_button, on_test_ringtone_clicked, screen);
    UIButton_SetCornerRadius(screen->test_ringtone_button, 5);
    UIButton_SetColor(screen->test_ringtone_button, THEME_ACCENT);
}

static void create_silent_mode_widgets(ScreenSettingsAudio_t* screen)
{
    screen->silent_mode_switch = UISwitch_Create();
    UISwitch_SetPosition(screen->silent_mode_switch, SLIDER_X, PARAM_SILENT_Y + 16);
    UISwitch_SetOnChanged(screen->silent_mode_switch, on_silent_mode_changed, screen);
    UISwitch_SetOnColor(screen->silent_mode_switch, THEME_DANGER);

    /* Labels ON/OFF à côté du switch */
    screen->value_labels[AUDIO_PARAM_SILENT_MODE] = UILabel_Create();
    UILabel_SetFont(screen->value_labels[AUDIO_PARAM_SILENT_MODE], &font_small);
    UILabel_SetColor(screen->value_labels[AUDIO_PARAM_SILENT_MODE], THEME_TEXT_SECONDARY);
    UILabel_SetPosition(screen->value_labels[AUDIO_PARAM_SILENT_MODE],
                        SLIDER_X + 60, PARAM_SILENT_Y + 20);
}

static void create_vibrator_widgets(ScreenSettingsAudio_t* screen)
{
    screen->vibrator_switch = UISwitch_Create();
    UISwitch_SetPosition(screen->vibrator_switch, SLIDER_X, PARAM_VIBRATOR_Y + 16);
    UISwitch_SetOnChanged(screen->vibrator_switch, on_vibrator_changed, screen);

    screen->value_labels[AUDIO_PARAM_VIBRATOR] = UILabel_Create();
    UILabel_SetFont(screen->value_labels[AUDIO_PARAM_VIBRATOR], &font_small);
    UILabel_SetColor(screen->value_labels[AUDIO_PARAM_VIBRATOR], THEME_TEXT_SECONDARY);
    UILabel_SetPosition(screen->value_labels[AUDIO_PARAM_VIBRATOR],
                        SLIDER_X + 60, PARAM_VIBRATOR_Y + 20);

    /* Bouton test */
    screen->test_vibrator_button = UIButton_Create();
    UIButton_SetText(screen->test_vibrator_button, "Test");
    UIButton_SetFont(screen->test_vibrator_button, &font_small);
    UIButton_SetSize(screen->test_vibrator_button, 50, 24);
    UIButton_SetPosition(screen->test_vibrator_button, SCREEN_WIDTH - 65, PARAM_VIBRATOR_Y + 16);
    UIButton_SetOnClick(screen->test_vibrator_button, on_test_vibrator_clicked, screen);
    UIButton_SetCornerRadius(screen->test_vibrator_button, 5);
}

static void create_audio_mode_widgets(ScreenSettingsAudio_t* screen)
{
    screen->audio_mode_radio = UIRadioGroup_Create();
    UIRadioGroup_SetPosition(screen->audio_mode_radio, CONTENT_X_MARGIN + 5, PARAM_AUDIO_MODE_Y + 20);
    UIRadioGroup_SetOptions(screen->audio_mode_radio, AUDIO_MODE_OPTIONS, AUDIO_MODE_COUNT);
    UIRadioGroup_SetOrientation(screen->audio_mode_radio, RADIO_HORIZONTAL);
    UIRadioGroup_SetSpacing(screen->audio_mode_radio, 6);
    UIRadioGroup_SetOnSelected(screen->audio_mode_radio, on_audio_mode_selected, screen);
}

static void create_noise_reduction_widgets(ScreenSettingsAudio_t* screen)
{
    screen->noise_reduction_radio = UIRadioGroup_Create();
    UIRadioGroup_SetPosition(screen->noise_reduction_radio, CONTENT_X_MARGIN + 5, PARAM_NR_Y + 20);
    UIRadioGroup_SetOptions(screen->noise_reduction_radio, NOISE_REDUCTION_OPTIONS, NOISE_REDUCTION_COUNT);
    UIRadioGroup_SetOrientation(screen->noise_reduction_radio, RADIO_HORIZONTAL);
    UIRadioGroup_SetSpacing(screen->noise_reduction_radio, 4);
    UIRadioGroup_SetOnSelected(screen->noise_reduction_radio, on_noise_reduction_selected, screen);
    UIRadioGroup_SetSelectedColor(screen->noise_reduction_radio, THEME_ACCENT);
}

static void create_mic_gain_widgets(ScreenSettingsAudio_t* screen)
{
    screen->mic_gain_slider = UISlider_Create();
    UISlider_SetPosition(screen->mic_gain_slider, SLIDER_X, PARAM_MIC_GAIN_Y + 20);
    UISlider_SetSize(screen->mic_gain_slider, SLIDER_WIDTH, SLIDER_HEIGHT);
    UISlider_SetRange(screen->mic_gain_slider, 0, 100);
    UISlider_SetOnChanged(screen->mic_gain_slider, on_mic_gain_changed, screen);
    UISlider_SetColor(screen->mic_gain_slider, THEME_WARNING);

    screen->value_labels[AUDIO_PARAM_MIC_GAIN] = UILabel_Create();
    UILabel_SetFont(screen->value_labels[AUDIO_PARAM_MIC_GAIN], &font_medium_bold);
    UILabel_SetColor(screen->value_labels[AUDIO_PARAM_MIC_GAIN], THEME_TEXT_PRIMARY);
    UILabel_SetPosition(screen->value_labels[AUDIO_PARAM_MIC_GAIN],
                        SLIDER_X + SLIDER_WIDTH + 8, PARAM_MIC_GAIN_Y + 18);
}

/* ======================================================================== */
/*              CALLBACKS WIDGETS                                           */
/* ======================================================================== */

static void on_mic_volume_changed(void* context, int16_t value)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;

    screen->params[AUDIO_PARAM_MIC_VOLUME].value.percentage = (uint8_t)value;
    screen->params[AUDIO_PARAM_MIC_VOLUME].is_modified = true;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    UILabel_SetText(screen->value_labels[AUDIO_PARAM_MIC_VOLUME], buf);
    UILabel_Draw(screen->value_labels[AUDIO_PARAM_MIC_VOLUME]);

    /* Application immédiate */
    apply_mic_volume(screen);
}

static void on_speaker_volume_changed(void* context, int16_t value)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;

    screen->params[AUDIO_PARAM_SPEAKER_VOLUME].value.percentage = (uint8_t)value;
    screen->params[AUDIO_PARAM_SPEAKER_VOLUME].is_modified = true;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    UILabel_SetText(screen->value_labels[AUDIO_PARAM_SPEAKER_VOLUME], buf);
    UILabel_Draw(screen->value_labels[AUDIO_PARAM_SPEAKER_VOLUME]);

    /* Application immédiate */
    apply_speaker_volume(screen);
}

static void on_ringtone_volume_changed(void* context, int16_t value)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;

    screen->params[AUDIO_PARAM_RINGTONE_VOLUME].value.percentage = (uint8_t)value;
    screen->params[AUDIO_PARAM_RINGTONE_VOLUME].is_modified = true;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    UILabel_SetText(screen->value_labels[AUDIO_PARAM_RINGTONE_VOLUME], buf);
    UILabel_Draw(screen->value_labels[AUDIO_PARAM_RINGTONE_VOLUME]);
}

static void on_silent_mode_changed(void* context, bool enabled)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;

    screen->params[AUDIO_PARAM_SILENT_MODE].value.boolean = enabled;
    screen->params[AUDIO_PARAM_SILENT_MODE].is_modified = true;

    /* Mettre à jour le label */
    UILabel_SetText(screen->value_labels[AUDIO_PARAM_SILENT_MODE],
                    enabled ? "Silencieux" : "Normal");
    UILabel_Draw(screen->value_labels[AUDIO_PARAM_SILENT_MODE]);

    /* Application immédiate */
    apply_silent_mode(screen);

    update_status_message(screen, enabled ? "Mode silence active" : "Mode normal");
}

static void on_vibrator_changed(void* context, bool enabled)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;

    screen->params[AUDIO_PARAM_VIBRATOR].value.boolean = enabled;
    screen->params[AUDIO_PARAM_VIBRATOR].is_modified = true;

    UILabel_SetText(screen->value_labels[AUDIO_PARAM_VIBRATOR],
                    enabled ? "Active" : "Desactive");
    UILabel_Draw(screen->value_labels[AUDIO_PARAM_VIBRATOR]);
}

static void on_audio_mode_selected(void* context, uint8_t index)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;

    screen->params[AUDIO_PARAM_AUDIO_MODE].value.mode_index = index;
    screen->params[AUDIO_PARAM_AUDIO_MODE].is_modified = true;

    update_status_message(screen, "Mode audio change");
}

static void on_noise_reduction_selected(void* context, uint8_t index)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;

    screen->params[AUDIO_PARAM_NOISE_REDUCTION].value.nr_level = index;
    screen->params[AUDIO_PARAM_NOISE_REDUCTION].is_modified = true;
}

static void on_mic_gain_changed(void* context, int16_t value)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;

    screen->params[AUDIO_PARAM_MIC_GAIN].value.percentage = (uint8_t)value;
    screen->params[AUDIO_PARAM_MIC_GAIN].is_modified = true;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    UILabel_SetText(screen->value_labels[AUDIO_PARAM_MIC_GAIN], buf);
    UILabel_Draw(screen->value_labels[AUDIO_PARAM_MIC_GAIN]);
}

static void on_test_speaker_clicked(void* context)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;
    ScreenSettingsAudio_TestSpeaker(screen);
}

static void on_test_ringtone_clicked(void* context)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;
    ScreenSettingsAudio_TestRingtone(screen);
}

static void on_test_vibrator_clicked(void* context)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;
    ScreenSettingsAudio_TestVibrator(screen);
}

static void on_apply_clicked(void* context)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;

    bool has_modifications = false;
    for (int i = 0; i < AUDIO_PARAM_COUNT; i++) {
        if (screen->params[i].is_modified) {
            has_modifications = true;
            break;
        }
    }

    if (!has_modifications) {
        update_status_message(screen, "Aucune modification a appliquer");
        return;
    }

    ScreenSettingsAudio_ApplyParams(screen);
}

static void on_defaults_clicked(void* context)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;

    UIDialog_SetTitle(screen->confirm_dialog, "Valeurs par defaut");
    UIDialog_SetMessage(screen->confirm_dialog,
                        "Remettre tous les parametres\n"
                        "audio aux valeurs d'usine ?\n\n"
                        "Micro : 80%%\n"
                        "HP    : 75%%\n"
                        "Sonnerie: 85%%\n"
                        "Silence : OFF\n"
                        "Vibreur : ON");
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
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;

    UIDialog_SetVisible(screen->confirm_dialog, false);

    if (screen->base.is_visible) {
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
        ScreenSettingsAudio_Show(screen);
    }

    if (confirmed) {
        ScreenSettingsAudio_RestoreDefaults(screen);
    }
}

static void on_back_clicked(void* context)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)context;
    if (!screen) return;

    /* Désactiver le VU-mètre avant de quitter */
    ScreenSettingsAudio_SetVuMeterActive(screen, false);

    if (screen->on_back_pressed) {
        screen->on_back_pressed();
    } else {
        UINavigation_GoBack();
    }
}

/* ======================================================================== */
/*              APPLICATION AU MATÉRIEL                                     */
/* ======================================================================== */

static void apply_mic_volume(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->audio_manager) return;

    uint8_t vol = screen->params[AUDIO_PARAM_MIC_VOLUME].value.percentage;
    AudioManager_SetMicVolume(screen->audio_manager, vol);

    DEBUG_VERBOSE(TAG, "Volume micro appliqué: %d%%", vol);
}

static void apply_speaker_volume(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->audio_manager) return;

    uint8_t vol = screen->params[AUDIO_PARAM_SPEAKER_VOLUME].value.percentage;
    AudioManager_SetSpeakerVolume(screen->audio_manager, vol);

    DEBUG_VERBOSE(TAG, "Volume HP appliqué: %d%%", vol);
}

static void apply_ringtone_volume(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->audio_manager) return;

    uint8_t vol = screen->params[AUDIO_PARAM_RINGTONE_VOLUME].value.percentage;
    AudioManager_SetRingtoneVolume(screen->audio_manager, vol);

    DEBUG_VERBOSE(TAG, "Volume sonnerie appliqué: %d%%", vol);
}

static void apply_silent_mode(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->audio_manager) return;

    bool silent = screen->params[AUDIO_PARAM_SILENT_MODE].value.boolean;
    AudioManager_SetSilentMode(screen->audio_manager, silent);

    DEBUG_VERBOSE(TAG, "Mode silence: %s", silent ? "ON" : "OFF");
}

static void apply_vibrator(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->audio_manager) return;

    bool enabled = screen->params[AUDIO_PARAM_VIBRATOR].value.boolean;
    AudioManager_SetVibrator(screen->audio_manager, enabled);

    DEBUG_VERBOSE(TAG, "Vibreur: %s", enabled ? "ON" : "OFF");
}

static void apply_audio_mode(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->audio_manager) return;

    uint8_t mode = screen->params[AUDIO_PARAM_AUDIO_MODE].value.mode_index;
    AudioManager_SetOutputMode(screen->audio_manager, (AudioOutputMode_t)mode);

    DEBUG_VERBOSE(TAG, "Mode audio: %s", AUDIO_MODE_OPTIONS[mode]);
}

static void apply_noise_reduction(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->audio_manager) return;

    uint8_t level = screen->params[AUDIO_PARAM_NOISE_REDUCTION].value.nr_level;
    AudioManager_SetNoiseReduction(screen->audio_manager, (NoiseReductionLevel_t)level);

    DEBUG_VERBOSE(TAG, "Réduction bruit: %s", NOISE_REDUCTION_OPTIONS[level]);
}

static void apply_mic_gain(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->audio_manager) return;

    uint8_t gain = screen->params[AUDIO_PARAM_MIC_GAIN].value.percentage;
    AudioManager_SetMicGain(screen->audio_manager, gain);

    DEBUG_VERBOSE(TAG, "Gain micro appliqué: %d%%", gain);
}

/* ======================================================================== */
/*              GÉNÉRATION AUDIO DE TEST                                    */
/* ======================================================================== */

/**
 * @brief Génère une tonalité sinusoïdale dans un buffer
 * 
 * f_out = (SAMPLE_RATE * step) / SINE_TABLE_SIZE
 * Pour 1 kHz @ 8 kHz : step = 1000 * 256 / 8000 = 32
 */
static void generate_test_tone(int16_t* buffer, uint16_t samples)
{
    /* Pas de lecture dans la table pour obtenir 1 kHz */
    uint16_t step = (TEST_FREQUENCY_HZ * SINE_TABLE_SIZE) / SAMPLE_RATE_HZ;
    uint16_t index = 0;

    for (uint16_t i = 0; i < samples; i++) {
        buffer[i] = SINE_TABLE[index];
        index = (index + step) % SINE_TABLE_SIZE;
    }
}

/**
 * @brief Joue la tonalité de test
 * 
 * Alloue un buffer DMA, le remplit avec la sinusoïde,
 * et lance la lecture via AudioManager.
 */
static void play_test_tone(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->audio_manager) return;

    /* Calculer le nombre d'échantillons pour la durée souhaitée */
    uint16_t sample_count = (SAMPLE_RATE_HZ * TEST_DURATION_MS) / 1000;

    /* Allouer le buffer (libéré par le DMA à la fin) */
    int16_t* buffer = (int16_t*)malloc(sample_count * sizeof(int16_t));
    if (!buffer) {
        DEBUG_ERROR(TAG, "Échec allocation buffer test audio");
        return;
    }

    /* Générer la sinusoïde */
    generate_test_tone(buffer, sample_count);

    /* Ajuster l'amplitude selon le volume HP */
    uint8_t volume = screen->params[AUDIO_PARAM_SPEAKER_VOLUME].value.percentage;
    float gain = volume / 100.0f;
    for (uint16_t i = 0; i < sample_count; i++) {
        buffer[i] = (int16_t)(buffer[i] * gain);
    }

    /* Jouer via l'AudioManager */
    AudioManager_PlayBuffer(screen->audio_manager, buffer, sample_count, true);

    DEBUG_VERBOSE(TAG, "Test audio démarré: %d échantillons", sample_count);
}

/**
 * @brief Arrête la tonalité de test
 */
static void stop_test_tone(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->audio_manager) return;

    AudioManager_StopPlayback(screen->audio_manager);
    DEBUG_VERBOSE(TAG, "Test audio arrêté");
}

/* ======================================================================== */
/*              RENDU                                                       */
/* ======================================================================== */

static void draw_param_row(int16_t y, const char* label, bool selected)
{
    uint16_t bg = selected ? THEME_LIST_SELECTED : THEME_BG_MAIN;
    Display_FillRect(CONTENT_X_MARGIN, y, CONTENT_WIDTH, PARAM_ROW_HEIGHT - 2, bg);
    Display_DrawText(CONTENT_X_MARGIN + 5, y + 4,
                     label, &font_small_bold,
                     THEME_TEXT_TERTIARY, bg);
}

static void update_vu_meter(ScreenSettingsAudio_t* screen)
{
    if (!screen || !screen->audio_manager || !screen->mic_vu_meter) return;

    /* Lire le niveau audio actuel */
    uint16_t level = AudioManager_GetMicLevel(screen->audio_manager);

    /* Convertir en pourcentage (0-4095 → 0-100) */
    uint8_t percentage = (uint8_t)((level * 100) / 4095);

    UIVuMeter_SetValue(screen->mic_vu_meter, percentage);

    if (screen->base.is_visible && screen->state == AUDIO_STATE_VU_METER_ACTIVE) {
        UIVuMeter_Draw(screen->mic_vu_meter);
    }
}

static void update_status_message(ScreenSettingsAudio_t* screen, const char* msg)
{
    if (!screen) return;

    UILabel_SetText(screen->status_label, msg);
    UILabel_SetColor(screen->status_label, THEME_SUCCESS);

    if (screen->base.is_visible) {
        UILabel_Draw(screen->status_label);
    }

    /* Démarrer un timer pour effacer le message */
    static TimerHandle_t status_timer = NULL;
    if (!status_timer) {
        status_timer = Timer_Create("AudioStatus", STATUS_TIMEOUT_MS, false,
                                    status_timer_callback, screen);
    }
    Timer_Start(status_timer);
}

/* ======================================================================== */
/*              TIMERS                                                      */
/* ======================================================================== */

static void vu_meter_timer_callback(TimerHandle_t timer)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)Timer_GetContext(timer);
    if (!screen) return;
    update_vu_meter(screen);
}

static void test_timer_callback(TimerHandle_t timer)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)Timer_GetContext(timer);
    if (!screen) return;

    DEBUG_VERBOSE(TAG, "Fin du test audio");

    /* Arrêter le test en cours */
    switch (screen->state) {
        case AUDIO_STATE_TESTING_SPEAKER:
        case AUDIO_STATE_TESTING_RINGTONE:
            stop_test_tone(screen);
            break;
        case AUDIO_STATE_TESTING_VIBRATOR:
            AudioManager_SetVibrator(screen->audio_manager, false);
            break;
        default:
            break;
    }

    screen->state = AUDIO_STATE_IDLE;
    update_status_message(screen, "Test termine");
}

static void status_timer_callback(TimerHandle_t timer)
{
    ScreenSettingsAudio_t* screen = (ScreenSettingsAudio_t*)Timer_GetContext(timer);
    if (!screen) return;

    UILabel_SetText(screen->status_label, "");
    if (screen->base.is_visible) {
        UILabel_Draw(screen->status_label);
    }
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */