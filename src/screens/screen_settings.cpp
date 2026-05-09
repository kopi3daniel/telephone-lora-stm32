/**
 * @file    screen_settings.cpp
 * @brief   Implémentation de l'écran des paramètres
 * @author  Votre Nom
 * @date    2026
 * 
 * Cet écran centralise tous les réglages du téléphone LoRa.
 * 
 * FONCTIONNEMENT :
 * ┌─────────────────────────────────────────────────────────┐
 * │  PARAMÈTRES                                  12:45  ███ │
 * │─────────────────────────────────────────────────────────│
 * │                                                         │
 * │  📡 Réseau LoRa ........................ 868.0 MHz      │
 * │  🔊 Audio .............................. Vol: 75%       │
 * │  🖥️ Affichage .......................... Lum: 60%       │
 * │  🔔 Sonneries .......................... Mélodie 1      │
 * │  🔒 Sécurité ........................... PIN: Activé    │
 * │  💾 Système ............................ v1.2.0         │
 * │  👤 Profil ............................. Mon Téléphone  │
 * │                                                         │
 * │─────────────────────────────────────────────────────────│
 * │  [Retour]                                               │
 * └─────────────────────────────────────────────────────────┘
 * 
 * Chaque ligne est cliquable et ouvre un sous-écran spécialisé.
 * Les valeurs affichées à droite sont lues en temps réel depuis
 * le SettingsService (persistant en flash).
 * 
 * PARTICULARITÉS STM32F429 :
 * - DMA2D utilisé pour le rendu accéléré des icônes et du fond
 * - LTDC couche 1 pour l'affichage principal
 * - Flash émulée EEPROM pour la persistance
 * - PWM TIM1 pour l'aperçu de luminosité en direct
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "screen_settings.h"

/* Sous-écrans spécialisés */
#include "screen_settings_network.h"
#include "screen_settings_audio.h"
#include "screen_settings_display.h"

/* UI */
#include "../ui/ui_core.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_widgets.h"
#include "../ui/ui_list.h"
#include "../ui/ui_label.h"
#include "../ui/ui_button.h"
#include "../ui/ui_slider.h"
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
#include "../drivers/power/backlight_control.h"

/* Utilitaires */
#include "../utils/string_utils.h"
#include "../utils/timer_utils.h"
#include "../utils/debug_utils.h"

/* Standard */
#include <string.h>
#include <stdio.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs de debug (apparaît dans la console série) */
#define TAG                         "ScreenSettings"

/** Dimensions de l'écran (définies aussi dans ltdc_config.h) */
#define SCREEN_WIDTH                320
#define SCREEN_HEIGHT               480

/** Zone de la liste des catégories */
#define LIST_AREA_X                 0
#define LIST_AREA_Y                 68          /* Sous la barre de titre */
#define LIST_AREA_WIDTH             320
#define LIST_AREA_HEIGHT            360         /* Jusqu'aux boutons du bas */

/** Hauteur d'une ligne de catégorie */
#define CATEGORY_ROW_HEIGHT         52

/** Position du bouton Retour */
#define BACK_BUTTON_X               10
#define BACK_BUTTON_Y               430
#define BACK_BUTTON_WIDTH           140
#define BACK_BUTTON_HEIGHT          40

/** Temps de rafraîchissement automatique (ms) */
#define REFRESH_INTERVAL_MS         2000

/** Taille du buffer pour le formatage des valeurs */
#define VALUE_BUFFER_SIZE           32

/** Durée de l'animation de transition (ms) */
#define TRANSITION_DURATION_MS      180

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

/* --- Construction des catégories --- */
static void build_categories(ScreenSettings_t* screen);
static void update_category_value(ScreenSettings_t* screen, 
                                  SettingsCategory_t category);

/* --- Callbacks UI --- */
static void on_category_selected(void* context, int16_t index);
static void on_back_clicked(void* context);
static void on_reset_confirmed(void* context, bool confirmed);

/* --- Rendu --- */
static void draw_category_row(void* context, 
                              int16_t index, 
                              int16_t y_position);
static void draw_title_bar(ScreenSettings_t* screen);
static void draw_bottom_bar(ScreenSettings_t* screen);

/* --- Rafraîchissement --- */
static void refresh_timer_callback(TimerHandle_t timer);
static void refresh_all_values(ScreenSettings_t* screen);

/* --- Sous-écrans --- */
static void open_network_settings(ScreenSettings_t* screen);
static void open_audio_settings(ScreenSettings_t* screen);
static void open_display_settings(ScreenSettings_t* screen);
static void open_ringtones_settings(ScreenSettings_t* screen);
static void open_security_settings(ScreenSettings_t* screen);
static void open_system_settings(ScreenSettings_t* screen);
static void open_profile_settings(ScreenSettings_t* screen);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise l'écran des paramètres
 * 
 * Étapes d'initialisation :
 * 1. Mise à zéro de la structure
 * 2. Initialisation de la classe de base ScreenBase
 * 3. Sauvegarde des pointeurs de services
 * 4. Création des widgets UI (liste, labels, boutons)
 * 5. Construction du tableau des catégories
 * 6. Création des timers de rafraîchissement
 * 
 * @note Les sous-écrans NE SONT PAS initialisés ici mais à la demande
 *       (lazy initialization) pour économiser la RAM.
 */
bool ScreenSettings_Init(ScreenSettings_t* screen,
                         SettingsService_t* settings_service,
                         BacklightControl_t* backlight)
{
    if (!screen || !settings_service) {
        DEBUG_ERROR(TAG, "Paramètres invalides (screen=%p, service=%p)",
                    (void*)screen, (void*)settings_service);
        return false;
    }

    DEBUG_INFO(TAG, "Initialisation de l'écran paramètres...");

    /* ---- 1. Mise à zéro complète ---- */
    memset(screen, 0, sizeof(ScreenSettings_t));

    /* ---- 2. Classe de base ---- */
    ScreenBase_Init(&screen->base, SCREEN_ID_SETTINGS, "Paramètres");

    /* ---- 3. Services ---- */
    screen->settings_service = settings_service;
    screen->backlight = backlight;

    /* ---- 4. État initial ---- */
    screen->state = SETTINGS_STATE_MAIN_LIST;
    screen->selected_index = -1;
    screen->scroll_offset = 0;

    /* ---- 5. Widgets UI ---- */

    /* 5a. Label titre */
    screen->title_label = UILabel_Create();
    UILabel_SetText(screen->title_label, "Parametres");
    UILabel_SetFont(screen->title_label, &font_large_bold);
    UILabel_SetColor(screen->title_label, THEME_TEXT_PRIMARY);
    UILabel_SetPosition(screen->title_label, 15, 8);

    /* 5b. Liste des catégories */
    screen->list_widget = UIList_Create();
    UIList_SetPosition(screen->list_widget, LIST_AREA_X, LIST_AREA_Y);
    UIList_SetSize(screen->list_widget, LIST_AREA_WIDTH, LIST_AREA_HEIGHT);
    UIList_SetRowHeight(screen->list_widget, CATEGORY_ROW_HEIGHT);
    UIList_SetRowCount(screen->list_widget, 0);  /* Rempli plus tard */
    UIList_SetDrawRowCallback(screen->list_widget, draw_category_row, screen);
    UIList_SetOnSelectCallback(screen->list_widget, on_category_selected, screen);
    UIList_SetScrollBarVisible(screen->list_widget, true);
    UIList_SetScrollBarColor(screen->list_widget, THEME_SCROLLBAR);
    UIList_SetScrollBarWidth(screen->list_widget, 4);

    /* 5c. Bouton Retour */
    screen->back_button = UIButton_Create();
    UIButton_SetText(screen->back_button, "Retour");
    UIButton_SetFont(screen->back_button, &font_medium);
    UIButton_SetSize(screen->back_button, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT);
    UIButton_SetPosition(screen->back_button, BACK_BUTTON_X, BACK_BUTTON_Y);
    UIButton_SetOnClick(screen->back_button, on_back_clicked, screen);
    UIButton_SetCornerRadius(screen->back_button, 8);
    UIButton_SetColor(screen->back_button, THEME_BUTTON_SECONDARY);

    /* 5d. Dialogue de confirmation (reset usine) */
    screen->confirm_dialog = UIDialog_Create();
    UIDialog_SetTitle(screen->confirm_dialog, "Reset usine");
    UIDialog_SetMessage(screen->confirm_dialog,
                        "Remettre tous les parametres\n"
                        "aux valeurs d'usine ?\n\n"
                        "Cette action est irreversible.");
    UIDialog_SetOnResult(screen->confirm_dialog, on_reset_confirmed, screen);
    UIDialog_SetVisible(screen->confirm_dialog, false);

    /* 5e. Barre de statut */
    UIStatusBar_Init(&screen->status_bar);

    /* ---- 6. Sous-écrans (initialisation lazy) ---- */
    screen->network_screen = NULL;
    screen->audio_screen = NULL;
    screen->display_screen = NULL;

    /* ---- 7. Construction des catégories ---- */
    build_categories(screen);

    /* ---- 8. Timers ---- */
    screen->refresh_timer = Timer_Create("SettingsRefresh",
                                         REFRESH_INTERVAL_MS,
                                         true,  /* auto-reload */
                                         refresh_timer_callback,
                                         screen);

    screen->backlight_preview = Timer_Create("BacklightPreview",
                                              3000,  /* 3 secondes */
                                              false,  /* one-shot */
                                              NULL,
                                              screen);

    DEBUG_INFO(TAG, "Initialisation terminée (%d catégories)",
               screen->category_count);

    return true;
}

/**
 * @brief Affiche l'écran des paramètres
 * 
 * Rendu complet :
 * 1. Efface l'écran avec la couleur de fond du thème
 * 2. Dessine la barre de titre
 * 3. Dessine la liste des catégories
 * 4. Dessine la barre du bas avec le bouton Retour
 * 5. Lance le timer de rafraîchissement
 */
void ScreenSettings_Show(ScreenSettings_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Affichage de l'écran paramètres");

    /* Réinitialiser l'état */
    screen->state = SETTINGS_STATE_MAIN_LIST;
    screen->selected_index = -1;

    /* Rafraîchir les valeurs affichées */
    refresh_all_values(screen);

    /* ---- Rendu complet ---- */

    /* Fond d'écran */
    Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);

    /* Barre de statut (heure, batterie, signal) */
    UIStatusBar_Draw(&screen->status_bar);

    /* Barre de titre */
    draw_title_bar(screen);

    /* Liste des catégories */
    UIList_SetRowCount(screen->list_widget, screen->category_count);
    UIList_SetVisible(screen->list_widget, true);
    UIList_Draw(screen->list_widget);

    /* Barre du bas */
    draw_bottom_bar(screen);

    /* Bouton Retour */
    UIButton_Draw(screen->back_button);

    /* Timer de rafraîchissement */
    Timer_Start(screen->refresh_timer);

    screen->base.is_visible = true;

    DEBUG_INFO(TAG, "Écran paramètres affiché");
}

/**
 * @brief Masque l'écran des paramètres
 */
void ScreenSettings_Hide(ScreenSettings_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Masquage de l'écran paramètres");

    Timer_Stop(screen->refresh_timer);

    /* Fermer tout sous-écran ouvert */
    if (screen->state == SETTINGS_STATE_SUBSCREEN_OPEN) {
        ScreenSettings_CloseSubScreen(screen);
    }

    screen->base.is_visible = false;
}

/**
 * @brief Mise à jour périodique
 */
void ScreenSettings_Update(ScreenSettings_t* screen)
{
    if (!screen || !screen->base.is_visible) return;

    /* Mise à jour de l'heure dans la barre de statut */
    UIStatusBar_Update(&screen->status_bar);

    /* Si un sous-écran est ouvert, lui déléguer la mise à jour */
    if (screen->state == SETTINGS_STATE_SUBSCREEN_OPEN) {
        /* Les sous-écrans gèrent leur propre Update */
        return;
    }
}

/**
 * @brief Gère les événements tactiles
 * 
 * Priorité de traitement :
 * 1. Dialogue de confirmation (s'il est visible)
 * 2. Sous-écran actif (s'il est ouvert)
 * 3. Bouton Retour
 * 4. Liste des catégories
 */
bool ScreenSettings_HandleTouch(ScreenSettings_t* screen,
                                const TouchEvent_t* event)
{
    if (!screen || !event) return false;

    /* Priorité 1 : Dialogue de confirmation */
    if (UIDialog_IsVisible(screen->confirm_dialog)) {
        return UIDialog_HandleTouch(screen->confirm_dialog, event);
    }

    /* Priorité 2 : Sous-écran actif */
    if (screen->state == SETTINGS_STATE_SUBSCREEN_OPEN) {
        /* Déléguer au sous-écran (via son propre handler) */
        return false;  /* Le sous-écran est géré par le navigateur */
    }

    /* Priorité 3 : Bouton Retour */
    if (UIButton_HitTest(screen->back_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->back_button);
        }
        return true;
    }

    /* Priorité 4 : Liste des catégories */
    if (UIList_HitTest(screen->list_widget, event->x, event->y)) {
        return UIList_HandleTouch(screen->list_widget, event);
    }

    return false;
}

/**
 * @brief Gère les touches du clavier physique
 */
bool ScreenSettings_HandleKey(ScreenSettings_t* screen,
                              KeyCode_t key)
{
    if (!screen) return false;

    /* Si dialogue visible, priorité au dialogue */
    if (UIDialog_IsVisible(screen->confirm_dialog)) {
        if (key == KEY_OK || key == KEY_SELECT) {
            /* Confirmer */
            on_reset_confirmed(screen, true);
            return true;
        }
        if (key == KEY_BACK || key == KEY_CANCEL) {
            /* Annuler */
            on_reset_confirmed(screen, false);
            return true;
        }
        return false;
    }

    switch (key) {
        case KEY_UP:
            if (screen->selected_index > 0) {
                screen->selected_index--;
                UIList_ScrollTo(screen->list_widget, screen->selected_index);
                UIList_RedrawAll(screen->list_widget);
            }
            return true;

        case KEY_DOWN:
            if (screen->selected_index < (int16_t)(screen->category_count - 1)) {
                screen->selected_index++;
                UIList_ScrollTo(screen->list_widget, screen->selected_index);
                UIList_RedrawAll(screen->list_widget);
            }
            return true;

        case KEY_OK:
        case KEY_SELECT:
            if (screen->selected_index >= 0) {
                on_category_selected(screen, screen->selected_index);
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
 * @brief Rafraîchit toutes les valeurs affichées
 */
void ScreenSettings_RefreshValues(ScreenSettings_t* screen)
{
    if (!screen) return;
    refresh_all_values(screen);

    if (screen->base.is_visible) {
        UIList_RedrawAll(screen->list_widget);
    }
}

/**
 * @brief Ouvre un sous-écran de paramètres spécifique
 * 
 * Appelée quand l'utilisateur sélectionne une catégorie.
 * Le sous-écran est créé à la demande (lazy init) puis affiché.
 */
void ScreenSettings_OpenCategory(ScreenSettings_t* screen,
                                 SettingsCategory_t category)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Ouverture de la catégorie: %d", category);

    screen->state = SETTINGS_STATE_SUBSCREEN_OPEN;

    /* Appeler le callback spécifique à la catégorie */
    for (int i = 0; i < screen->category_count; i++) {
        if (screen->categories[i].id == category) {
            if (screen->categories[i].on_selected) {
                screen->categories[i].on_selected(screen, category);
            }
            break;
        }
    }

    /* Notifier l'application */
    if (screen->on_category_opened) {
        screen->on_category_opened(category);
    }
}

/**
 * @brief Ferme le sous-écran actif
 */
void ScreenSettings_CloseSubScreen(ScreenSettings_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Fermeture du sous-écran");

    screen->state = SETTINGS_STATE_MAIN_LIST;

    /* Redessiner l'écran principal */
    if (screen->base.is_visible) {
        /* Animation de retour */
        UIAnimation_SlideFromLeft(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                                  TRANSITION_DURATION_MS);
        ScreenSettings_Show(screen);
    }
}

/**
 * @brief Ferme le dialogue de confirmation
 */
void ScreenSettings_CloseDialog(ScreenSettings_t* screen)
{
    if (!screen) return;

    UIDialog_SetVisible(screen->confirm_dialog, false);
    screen->state = SETTINGS_STATE_MAIN_LIST;

    if (screen->base.is_visible) {
        /* Redessiner la zone du dialogue */
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
        ScreenSettings_Show(screen);
    }
}

/**
 * @brief Libère toutes les ressources
 */
void ScreenSettings_Deinit(ScreenSettings_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Libération des ressources");

    /* Arrêter et supprimer les timers */
    if (screen->refresh_timer) {
        Timer_Delete(screen->refresh_timer);
        screen->refresh_timer = NULL;
    }
    if (screen->backlight_preview) {
        Timer_Delete(screen->backlight_preview);
        screen->backlight_preview = NULL;
    }

    /* Détruire les widgets */
    if (screen->title_label) {
        UILabel_Destroy(screen->title_label);
        screen->title_label = NULL;
    }
    if (screen->list_widget) {
        UIList_Destroy(screen->list_widget);
        screen->list_widget = NULL;
    }
    if (screen->back_button) {
        UIButton_Destroy(screen->back_button);
        screen->back_button = NULL;
    }
    if (screen->confirm_dialog) {
        UIDialog_Destroy(screen->confirm_dialog);
        screen->confirm_dialog = NULL;
    }

    /* Libérer les sous-écrans (s'ils ont été créés) */
    if (screen->network_screen) {
        ScreenSettingsNetwork_Deinit(screen->network_screen);
        free(screen->network_screen);
        screen->network_screen = NULL;
    }
    if (screen->audio_screen) {
        ScreenSettingsAudio_Deinit(screen->audio_screen);
        free(screen->audio_screen);
        screen->audio_screen = NULL;
    }
    if (screen->display_screen) {
        ScreenSettingsDisplay_Deinit(screen->display_screen);
        free(screen->display_screen);
        screen->display_screen = NULL;
    }

    memset(screen, 0, sizeof(ScreenSettings_t));

    DEBUG_INFO(TAG, "Ressources libérées");
}

/* ======================================================================== */
/*              CONSTRUCTION DES CATÉGORIES                                 */
/* ======================================================================== */

/**
 * @brief Construit le tableau des catégories avec leurs propriétés
 * 
 * Chaque catégorie est définie avec :
 * - Un identifiant unique (enum SettingsCategory_t)
 * - Un titre affiché
 * - Un sous-titre (description)
 * - Une icône et sa couleur
 * - Un callback qui sera appelé à la sélection
 * 
 * Les valeurs actuelles sont lues depuis le SettingsService
 * via update_category_value().
 */
static void build_categories(ScreenSettings_t* screen)
{
    if (!screen) return;

    screen->category_count = SETTINGS_CATEGORY_COUNT;

    /* Catégorie 0 : Réseau LoRa */
    screen->categories[0].id = SETTINGS_CAT_NETWORK;
    screen->categories[0].title = "Reseau LoRa";
    screen->categories[0].subtitle = "Frequence, puissance, parametres";
    screen->categories[0].icon = ICON_SETTINGS_NETWORK;
    screen->categories[0].icon_color = THEME_ICON_NETWORK;
    screen->categories[0].on_selected = (void(*)(void*, SettingsCategory_t))open_network_settings;

    /* Catégorie 1 : Audio */
    screen->categories[1].id = SETTINGS_CAT_AUDIO;
    screen->categories[1].title = "Audio";
    screen->categories[1].subtitle = "Volume micro, haut-parleur, silence";
    screen->categories[1].icon = ICON_SETTINGS_AUDIO;
    screen->categories[1].icon_color = THEME_ICON_AUDIO;
    screen->categories[1].on_selected = (void(*)(void*, SettingsCategory_t))open_audio_settings;

    /* Catégorie 2 : Affichage */
    screen->categories[2].id = SETTINGS_CAT_DISPLAY;
    screen->categories[2].title = "Affichage";
    screen->categories[2].subtitle = "Luminosite, timeout ecran";
    screen->categories[2].icon = ICON_SETTINGS_DISPLAY;
    screen->categories[2].icon_color = THEME_ICON_DISPLAY;
    screen->categories[2].on_selected = (void(*)(void*, SettingsCategory_t))open_display_settings;

    /* Catégorie 3 : Sonneries */
    screen->categories[3].id = SETTINGS_CAT_RINGTONES;
    screen->categories[3].title = "Sonneries";
    screen->categories[3].subtitle = "Melodie, vibreur, volume";
    screen->categories[3].icon = ICON_SETTINGS_RINGTONE;
    screen->categories[3].icon_color = THEME_ICON_RINGTONE;
    screen->categories[3].on_selected = (void(*)(void*, SettingsCategory_t))open_ringtones_settings;

    /* Catégorie 4 : Sécurité */
    screen->categories[4].id = SETTINGS_CAT_SECURITY;
    screen->categories[4].title = "Securite";
    screen->categories[4].subtitle = "Code PIN, verrouillage";
    screen->categories[4].icon = ICON_SETTINGS_SECURITY;
    screen->categories[4].icon_color = THEME_ICON_SECURITY;
    screen->categories[4].on_selected = (void(*)(void*, SettingsCategory_t))open_security_settings;

    /* Catégorie 5 : Système */
    screen->categories[5].id = SETTINGS_CAT_SYSTEM;
    screen->categories[5].title = "Systeme";
    screen->categories[5].subtitle = "Stockage, version, reset";
    screen->categories[5].icon = ICON_SETTINGS_SYSTEM;
    screen->categories[5].icon_color = THEME_ICON_SYSTEM;
    screen->categories[5].on_selected = (void(*)(void*, SettingsCategory_t))open_system_settings;

    /* Catégorie 6 : Profil */
    screen->categories[6].id = SETTINGS_CAT_PROFILE;
    screen->categories[6].title = "Profil";
    screen->categories[6].subtitle = "Nom appareil, indicatif, ID reseau";
    screen->categories[6].icon = ICON_SETTINGS_PROFILE;
    screen->categories[6].icon_color = THEME_ICON_PROFILE;
    screen->categories[6].on_selected = (void(*)(void*, SettingsCategory_t))open_profile_settings;

    /* Lire les valeurs actuelles depuis le service */
    refresh_all_values(screen);

    DEBUG_VERBOSE(TAG, "Catégories construites: %d", screen->category_count);
}

/**
 * @brief Met à jour la valeur affichée pour une catégorie spécifique
 * 
 * Lit la valeur depuis le SettingsService et la formate en chaîne
 * lisible dans current_value.
 * 
 * Exemples de formatage :
 *   Réseau  → "868.0 MHz"
 *   Audio   → "Vol: 75%"
 *   Système → "v1.2.0"
 */
static void update_category_value(ScreenSettings_t* screen,
                                  SettingsCategory_t category)
{
    if (!screen || !screen->settings_service) return;

    SettingsService_t* svc = screen->settings_service;
    char* buffer = NULL;

    /* Trouver la catégorie dans le tableau */
    for (int i = 0; i < screen->category_count; i++) {
        if (screen->categories[i].id == category) {
            buffer = screen->categories[i].current_value;
            break;
        }
    }

    if (!buffer) return;

    switch (category) {
        case SETTINGS_CAT_NETWORK: {
            /* Fréquence actuelle */
            uint32_t freq_hz;
            SettingsService_GetLoRaFrequency(svc, &freq_hz);
            float freq_mhz = freq_hz / 1000000.0f;
            snprintf(buffer, VALUE_BUFFER_SIZE, "%.1f MHz", freq_mhz);
            break;
        }

        case SETTINGS_CAT_AUDIO: {
            /* Volume haut-parleur */
            uint8_t volume;
            SettingsService_GetSpeakerVolume(svc, &volume);
            snprintf(buffer, VALUE_BUFFER_SIZE, "Vol: %d%%", volume);
            break;
        }

        case SETTINGS_CAT_DISPLAY: {
            /* Luminosité */
            uint8_t brightness;
            SettingsService_GetBrightness(svc, &brightness);
            snprintf(buffer, VALUE_BUFFER_SIZE, "Lum: %d%%", brightness);
            break;
        }

        case SETTINGS_CAT_RINGTONES: {
            /* Mélodie active */
            uint8_t melody;
            SettingsService_GetRingtone(svc, &melody);
            snprintf(buffer, VALUE_BUFFER_SIZE, "Melodie %d", melody + 1);
            break;
        }

        case SETTINGS_CAT_SECURITY: {
            /* État du PIN */
            bool pin_enabled;
            SettingsService_GetPinEnabled(svc, &pin_enabled);
            snprintf(buffer, VALUE_BUFFER_SIZE, "PIN: %s",
                     pin_enabled ? "Active" : "Desactive");
            break;
        }

        case SETTINGS_CAT_SYSTEM: {
            /* Version du firmware */
            snprintf(buffer, VALUE_BUFFER_SIZE, "v%s", FIRMWARE_VERSION);
            break;
        }

        case SETTINGS_CAT_PROFILE: {
            /* Nom de l'appareil */
            char device_name[17];
            SettingsService_GetDeviceName(svc, device_name, sizeof(device_name));
            /* Tronquer si trop long */
            if (strlen(device_name) > 14) {
                device_name[14] = '.';
                device_name[15] = '.';
                device_name[16] = '\0';
            }
            snprintf(buffer, VALUE_BUFFER_SIZE, "%s", device_name);
            break;
        }

        default:
            buffer[0] = '\0';
            break;
    }
}

/* ======================================================================== */
/*              CALLBACKS UI                                                */
/* ======================================================================== */

/**
 * @brief Callback appelé quand une catégorie est sélectionnée
 * 
 * Joue un retour haptique (si disponible) puis ouvre le sous-écran
 * correspondant à la catégorie.
 */
static void on_category_selected(void* context, int16_t index)
{
    ScreenSettings_t* screen = (ScreenSettings_t*)context;
    if (!screen || index < 0 || index >= screen->category_count) return;

    DEBUG_INFO(TAG, "Catégorie sélectionnée: %d (%s)",
               index, screen->categories[index].title);

    /* Mettre à jour la sélection */
    screen->selected_index = index;

    /* Ouvrir la catégorie (le sous-écran correspondant) */
    SettingsCategory_t cat_id = screen->categories[index].id;
    ScreenSettings_OpenCategory(screen, cat_id);
}

/**
 * @brief Callback du bouton Retour
 * 
 * Appelle le callback on_back_pressed s'il est défini,
 * sinon utilise le comportement par défaut (retour à l'écran précédent).
 */
static void on_back_clicked(void* context)
{
    ScreenSettings_t* screen = (ScreenSettings_t*)context;
    if (!screen) return;

    DEBUG_INFO(TAG, "Bouton Retour cliqué");

    if (screen->on_back_pressed) {
        screen->on_back_pressed();
    } else {
        /* Comportement par défaut : navigation arrière */
        UINavigation_GoBack();
    }
}

/**
 * @brief Callback du dialogue de confirmation (reset usine)
 * 
 * Si l'utilisateur confirme :
 * 1. Appelle SettingsService_FactoryReset() pour effacer la flash
 * 2. Redémarre l'appareil
 * 
 * Si l'utilisateur annule :
 * 1. Ferme simplement le dialogue
 */
static void on_reset_confirmed(void* context, bool confirmed)
{
    ScreenSettings_t* screen = (ScreenSettings_t*)context;
    if (!screen) return;

    DEBUG_INFO(TAG, "Confirmation reset: %s", confirmed ? "OUI" : "NON");

    /* Masquer le dialogue */
    UIDialog_SetVisible(screen->confirm_dialog, false);
    screen->state = SETTINGS_STATE_MAIN_LIST;

    if (confirmed) {
        DEBUG_WARN(TAG, "RESET USINE - Redémarrage imminent...");

        /* Afficher un message */
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
        Display_DrawText(SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2 - 10,
                         "Reset en cours...",
                         &font_large, THEME_TEXT_PRIMARY, THEME_BG_MAIN);

        /* Forcer l'affichage */
        Display_SwapBuffers();

        /* Effacer les paramètres en flash */
        SettingsService_FactoryReset(screen->settings_service);

        /* Attendre un peu pour que l'utilisateur voie le message */
        HAL_Delay(1500);

        /* Redémarrer le système */
        HAL_NVIC_SystemReset();

        /* Ne devrait jamais arriver ici */
        while (1) {}
    }

    /* Redessiner l'écran */
    if (screen->base.is_visible) {
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
        ScreenSettings_Show(screen);
    }
}

/* ======================================================================== */
/*              RENDU GRAPHIQUE                                             */
/* ======================================================================== */

/**
 * @brief Dessine une ligne de catégorie dans la liste
 * 
 * Format d'une ligne :
 * ┌──────────────────────────────────────────────────┐
 * │ [📡]  Réseau LoRa .................. 868.0 MHz   │
 * │       Fréquence, puissance, paramètres           │
 * └──────────────────────────────────────────────────┘
 * 
 * Utilise DMA2D pour le rendu accéléré du fond et de l'icône.
 */
static void draw_category_row(void* context,
                              int16_t index,
                              int16_t y_position)
{
    ScreenSettings_t* screen = (ScreenSettings_t*)context;
    if (!screen || index < 0 || index >= screen->category_count) return;

    SettingsCategory_t* cat = &screen->categories[index];

    /* Coordonnées de la ligne */
    int16_t row_x = LIST_AREA_X;
    int16_t row_y = LIST_AREA_Y + y_position;
    int16_t row_w = LIST_AREA_WIDTH;
    int16_t row_h = CATEGORY_ROW_HEIGHT - 1;

    /* Fond de la ligne */
    uint16_t bg_color;
    if (index == screen->selected_index) {
        bg_color = THEME_LIST_SELECTED;
    } else if (index % 2 == 0) {
        bg_color = THEME_LIST_EVEN;
    } else {
        bg_color = THEME_LIST_ODD;
    }

    /* Remplir le fond avec DMA2D (accéléré) */
    DMA2D_FillRect(row_x, row_y, row_w, row_h, bg_color);

    /* ---- Icône à gauche ---- */
    int16_t icon_x = row_x + 10;
    int16_t icon_y = row_y + (row_h - 28) / 2;

    UIIcons_Draw(cat->icon, icon_x, icon_y, cat->icon_color);

    /* ---- Texte principal (titre) ---- */
    int16_t text_x = icon_x + 36;  /* À droite de l'icône */
    int16_t text_y = row_y + 6;

    Display_DrawText(text_x, text_y,
                     cat->title,
                     &font_medium_bold,
                     THEME_TEXT_PRIMARY,
                     bg_color);

    /* ---- Sous-titre ---- */
    Display_DrawText(text_x, text_y + 22,
                     cat->subtitle,
                     &font_small,
                     THEME_TEXT_TERTIARY,
                     bg_color);

    /* ---- Valeur actuelle (alignée à droite) ---- */
    if (strlen(cat->current_value) > 0) {
        /* Calculer la position pour aligner à droite */
        int16_t value_width = strlen(cat->current_value) * 7;  /* ~7px par char */
        int16_t value_x = row_x + row_w - value_width - 12;
        int16_t value_y = row_y + (row_h - 16) / 2;

        Display_DrawText(value_x, value_y,
                         cat->current_value,
                         &font_small,
                         THEME_ACCENT,
                         bg_color);
    }

    /* ---- Ligne séparatrice en bas ---- */
    Display_DrawHLine(row_x, row_y + row_h, row_w, THEME_LIST_SEPARATOR);

    /* ---- Flèche ">" à droite pour indiquer que c'est cliquable ---- */
    int16_t arrow_x = row_x + row_w - 18;
    int16_t arrow_y = row_y + (row_h - 18) / 2;

    Display_DrawText(arrow_x, arrow_y,
                     ">",
                     &font_medium,
                     THEME_TEXT_TERTIARY,
                     bg_color);
}

/**
 * @brief Dessine la barre de titre
 * 
 * Contient le titre "Paramètres" et une ligne de séparation.
 */
static void draw_title_bar(ScreenSettings_t* screen)
{
    if (!screen) return;

    /* Fond de la barre de titre */
    Display_FillRect(0, 25, SCREEN_WIDTH, 42, THEME_BG_MAIN);

    /* Titre */
    UILabel_Draw(screen->title_label);

    /* Ligne de séparation */
    Display_DrawHLine(0, 66, SCREEN_WIDTH, THEME_DIVIDER);
}

/**
 * @brief Dessine la barre du bas
 * 
 * Contient une ligne de séparation au-dessus du bouton Retour.
 */
static void draw_bottom_bar(ScreenSettings_t* screen)
{
    if (!screen) return;

    /* Ligne de séparation */
    Display_DrawHLine(0, BACK_BUTTON_Y - 8, SCREEN_WIDTH, THEME_DIVIDER);
}

/* ======================================================================== */
/*              RAFRAÎCHISSEMENT                                            */
/* ======================================================================== */

/**
 * @brief Callback du timer de rafraîchissement
 * 
 * Appelé toutes les REFRESH_INTERVAL_MS millisecondes.
 * Met à jour les valeurs affichées (ex: changement de volume
 * par un bouton physique, mise à jour de l'heure, etc.)
 */
static void refresh_timer_callback(TimerHandle_t timer)
{
    ScreenSettings_t* screen = (ScreenSettings_t*)Timer_GetContext(timer);
    if (!screen || !screen->base.is_visible) return;

    /* Ne pas rafraîchir si un sous-écran est ouvert */
    if (screen->state != SETTINGS_STATE_MAIN_LIST) return;

    /* Relire les valeurs depuis le service */
    refresh_all_values(screen);

    /* Redessiner uniquement les lignes modifiées */
    UIList_RedrawAll(screen->list_widget);
}

/**
 * @brief Rafraîchit toutes les valeurs des catégories
 */
static void refresh_all_values(ScreenSettings_t* screen)
{
    if (!screen) return;

    for (int i = 0; i < screen->category_count; i++) {
        update_category_value(screen, screen->categories[i].id);
    }
}

/* ======================================================================== */
/*              OUVERTURE DES SOUS-ÉCRANS                                   */
/* ======================================================================== */

/**
 * @brief Ouvre les réglages réseau LoRa
 * 
 * Crée le sous-écran à la demande (lazy initialization)
 * puis l'affiche avec une animation de transition.
 */
static void open_network_settings(ScreenSettings_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Ouverture réglages réseau");

    /* Création lazy du sous-écran réseau */
    if (!screen->network_screen) {
        screen->network_screen = (ScreenSettingsNetwork_t*)malloc(
            sizeof(ScreenSettingsNetwork_t));
        if (!screen->network_screen) {
            DEBUG_ERROR(TAG, "Échec allocation sous-écran réseau");
            return;
        }
        ScreenSettingsNetwork_Init(screen->network_screen,
                                   screen->settings_service);
        DEBUG_INFO(TAG, "Sous-écran réseau créé");
    }

    /* Animation de transition vers la droite */
    UIAnimation_SlideToLeft(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                            TRANSITION_DURATION_MS);

    /* Afficher le sous-écran */
    ScreenSettingsNetwork_Show(screen->network_screen);
}

/**
 * @brief Ouvre les réglages audio
 */
static void open_audio_settings(ScreenSettings_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Ouverture réglages audio");

    /* Création lazy du sous-écran audio */
    if (!screen->audio_screen) {
        screen->audio_screen = (ScreenSettingsAudio_t*)malloc(
            sizeof(ScreenSettingsAudio_t));
        if (!screen->audio_screen) {
            DEBUG_ERROR(TAG, "Échec allocation sous-écran audio");
            return;
        }
        ScreenSettingsAudio_Init(screen->audio_screen,
                                 screen->settings_service);
        DEBUG_INFO(TAG, "Sous-écran audio créé");
    }

    UIAnimation_SlideToLeft(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                            TRANSITION_DURATION_MS);
    ScreenSettingsAudio_Show(screen->audio_screen);
}

/**
 * @brief Ouvre les réglages d'affichage
 * 
 * Particularité : permet un aperçu en direct de la luminosité.
 * Un slider ajuste le PWM du rétroéclairage en temps réel.
 */
static void open_display_settings(ScreenSettings_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Ouverture réglages affichage");

    if (!screen->display_screen) {
        screen->display_screen = (ScreenSettingsDisplay_t*)malloc(
            sizeof(ScreenSettingsDisplay_t));
        if (!screen->display_screen) {
            DEBUG_ERROR(TAG, "Échec allocation sous-écran affichage");
            return;
        }
        ScreenSettingsDisplay_Init(screen->display_screen,
                                   screen->settings_service,
                                   screen->backlight);
        DEBUG_INFO(TAG, "Sous-écran affichage créé");
    }

    UIAnimation_SlideToLeft(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                            TRANSITION_DURATION_MS);
    ScreenSettingsDisplay_Show(screen->display_screen);
}

/**
 * @brief Ouvre les réglages des sonneries
 * 
 * Pour l'instant, intégré directement (pas de sous-écran séparé).
 * Affiche un dialogue avec la liste des mélodies disponibles.
 */
static void open_ringtones_settings(ScreenSettings_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Ouverture réglages sonneries");

    /* TODO: Implémenter le sous-écran sonneries */
    /* Pour l'instant, on utilise un dialogue simple */

    UIDialog_SetTitle(screen->confirm_dialog, "Sonneries");
    UIDialog_SetMessage(screen->confirm_dialog,
                        "Melodies disponibles:\n\n"
                        "1. Classique\n"
                        "2. Vibreur\n"
                        "3. Douce\n"
                        "4. Urgence\n\n"
                        "Appuyez sur OK pour changer");
    UIDialog_SetVisible(screen->confirm_dialog, true);
    screen->state = SETTINGS_STATE_MAIN_LIST;

    if (screen->base.is_visible) {
        UIDialog_Draw(screen->confirm_dialog);
    }
}

/**
 * @brief Ouvre les réglages de sécurité
 * 
 * Permet de changer le code PIN et d'activer/désactiver
 * le verrouillage automatique.
 */
static void open_security_settings(ScreenSettings_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Ouverture réglages sécurité");

    /* TODO: Implémenter le sous-écran sécurité avec saisie PIN */
    
    UIDialog_SetTitle(screen->confirm_dialog, "Securite");
    UIDialog_SetMessage(screen->confirm_dialog,
                        "Reglages de securite:\n\n"
                        "- Code PIN\n"
                        "- Verrouillage auto\n"
                        "- Duree avant verrouillage\n\n"
                        "(Fonctionnalite a venir)");
    UIDialog_SetVisible(screen->confirm_dialog, true);
    screen->state = SETTINGS_STATE_MAIN_LIST;

    if (screen->base.is_visible) {
        UIDialog_Draw(screen->confirm_dialog);
    }
}

/**
 * @brief Ouvre les réglages système
 * 
 * Affiche les informations système et propose :
 * - Réinitialisation usine
 * - Informations de version
 * - État du stockage
 */
static void open_system_settings(ScreenSettings_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Ouverture réglages système");

    /* Afficher les infos système dans un dialogue */
    char sys_info[256];

    /* Calculer l'espace flash utilisé */
    uint32_t flash_total = 2048;  /* 2 Mo */
    uint32_t flash_used = 512;    /* Exemple : 512 Ko */
    uint32_t flash_free = flash_total - flash_used;

    /* Uptime */
    uint32_t uptime_sec = HAL_GetTick() / 1000;
    uint32_t uptime_h = uptime_sec / 3600;
    uint32_t uptime_m = (uptime_sec % 3600) / 60;

    snprintf(sys_info, sizeof(sys_info),
             "💾 SYSTEME\n\n"
             "Version:   %s\n"
             "Compilee:  %s %s\n"
             "Flash:     %lu Ko / %lu Ko\n"
             "Uptime:    %luh %02lum\n"
             "STM32:     F429 @ 180 MHz\n"
             "LTDC:      Actif\n"
             "DMA2D:     Actif\n\n"
             "⚠️  Reset usine ci-dessous",
             FIRMWARE_VERSION,
             __DATE__, __TIME__,
             flash_used, flash_total,
             uptime_h, uptime_m);

    UIDialog_SetTitle(screen->confirm_dialog, "Systeme");
    UIDialog_SetMessage(screen->confirm_dialog, sys_info);
    UIDialog_SetVisible(screen->confirm_dialog, true);

    /* Ajouter un bouton "Reset usine" */
    screen->state = SETTINGS_STATE_MAIN_LIST;

    if (screen->base.is_visible) {
        UIDialog_Draw(screen->confirm_dialog);
    }
}

/**
 * @brief Ouvre les réglages du profil
 * 
 * Permet de modifier :
 * - Nom de l'appareil
 * - Indicatif pays
 * - ID réseau
 */
static void open_profile_settings(ScreenSettings_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Ouverture réglages profil");

    /* Lire les valeurs actuelles */
    char device_name[17];
    char country_code[8];
    uint8_t network_id;

    SettingsService_GetDeviceName(screen->settings_service, 
                                  device_name, sizeof(device_name));
    SettingsService_GetCountryCode(screen->settings_service,
                                   country_code, sizeof(country_code));
    SettingsService_GetNetworkId(screen->settings_service, &network_id);

    /* Afficher dans un dialogue */
    char profile_info[200];
    snprintf(profile_info, sizeof(profile_info),
             "👤 PROFIL\n\n"
             "Nom:      %s\n"
             "Indicatif: %s\n"
             "ID Reseau: %d\n\n"
             "(Modification via saisie\ntexte - fonction a venir)",
             device_name, country_code, network_id);

    UIDialog_SetTitle(screen->confirm_dialog, "Profil");
    UIDialog_SetMessage(screen->confirm_dialog, profile_info);
    UIDialog_SetVisible(screen->confirm_dialog, true);
    screen->state = SETTINGS_STATE_MAIN_LIST;

    if (screen->base.is_visible) {
        UIDialog_Draw(screen->confirm_dialog);
    }
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */