/**
 * @file    screen_lock.cpp
 * @brief   Implémentation de l'écran de verrouillage
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente le verrouillage/déverrouillage par code PIN.
 * 
 * FONCTIONNEMENT DÉTAILLÉ :
 * 
 * 1. AFFICHAGE INITIAL :
 *    - Horloge au centre (mise à jour chaque seconde)
 *    - Date en dessous
 *    - Icône cadenas 🔒
 *    - 4 à 8 points vides (○) selon la longueur du PIN
 *    - Message "Entrez le code"
 *    - Pavé numérique 0-9 + ⌫ (effacer)
 *    - Bouton "Appel d'urgence" en bas
 * 
 * 2. SAISIE DU PIN :
 *    - Chaque appui sur un chiffre l'ajoute au buffer
 *    - Le point correspondant devient plein (●) avec animation
 *    - Un bip court est émis à chaque appui
 *    - Au dernier chiffre, vérification automatique
 * 
 * 3. VÉRIFICATION :
 *    - Hash du PIN saisi + UID
 *    - Comparaison temps constant avec le hash stocké
 *    - Si OK : animation déverrouillage → callback on_unlocked
 *    - Si KO : animation shake → décrémentation compteur
 * 
 * 4. GESTION DES ÉCHECS :
 *    - 3 tentatives maximum
 *    - Affichage "X tentatives restantes"
 *    - Après 3 échecs : blocage temporaire
 *    - Blocage progressif : 30s → 1min → 5min → 15min
 *    - Décompte affiché pendant le blocage
 *    - Compteur persistant en flash
 * 
 * 5. APPEL D'URGENCE :
 *    - Bouton toujours accessible
 *    - Ouvre le composeur en mode urgence
 *    - Limité aux numéros : 112, 15, 17, 18, 911
 *    - Ne déverrouille pas le téléphone
 * 
 * 6. PIN OUBLIÉ :
 *    - Bouton "PIN oublié" (après 1er échec)
 *    - Avertissement : "Toutes les données seront effacées"
 *    - Confirmation obligatoire
 *    - Reset usine : efface tout + redémarrage
 * 
 * SÉCURITÉ :
 *    - PIN jamais stocké en clair
 *    - Hash SHA-256 avec sel (UID MCU)
 *    - Comparaison temps constant
 *    - Blocage anti-bruteforce
 *    - Compteur persistant (survit reboot)
 *    - UID unique par appareil (salage différent)
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "screen_lock.h"

/* UI */
#include "../ui/ui_core.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_widgets.h"
#include "../ui/ui_label.h"
#include "../ui/ui_button.h"
#include "../ui/ui_numpad.h"
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
#include "../drivers/audio/audio_manager.h"
#include "../drivers/power/power_manager.h"

/* Utilitaires */
#include "../utils/string_utils.h"
#include "../utils/timer_utils.h"
#include "../utils/debug_utils.h"
#include "../utils/crypto_utils.h"
#include "../utils/math_utils.h"

/* HAL */
#include "stm32f4xx_hal.h"

/* Standard */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs de debug */
#define TAG                                 "ScreenLock"

/** Dimensions de l'écran */
#define SCREEN_WIDTH                        320
#define SCREEN_HEIGHT                       480

/** Centre de l'écran */
#define CENTER_X                            (SCREEN_WIDTH / 2)
#define CENTER_Y                            (SCREEN_HEIGHT / 2)

/** Positions des éléments */
#define CLOCK_Y                             100
#define DATE_Y                              140
#define DAY_Y                               165
#define LOCK_ICON_Y                         220
#define MESSAGE_Y                           260
#define PIN_DOTS_Y                          300
#define PIN_DOT_SPACING                     28
#define NUMPAD_Y                            340
#define ATTEMPTS_Y                          460
#define EMERGENCY_BUTTON_Y                  430

/** Dimensions pavé numérique */
#define NUMPAD_WIDTH                        240
#define NUMPAD_HEIGHT                       120
#define NUMPAD_BUTTON_SIZE                  56
#define NUMPAD_SPACING                      8

/** Durées (ms) */
#define CLOCK_UPDATE_MS                     1000
#define BLOCK_COUNTDOWN_MS                  1000
#define SHAKE_ANIM_STEP_MS                  40
#define SHAKE_ANIM_STEPS                    10

/** Nombre de cycles shake */
#define SHAKE_AMPLITUDE                     8

/** Touch ID unique du STM32 (3 x 32 bits = 96 bits) */
#define UID_ADDR_0                          ((uint32_t*)0x1FFF7A10)
#define UID_ADDR_1                          ((uint32_t*)0x1FFF7A14)
#define UID_ADDR_2                          ((uint32_t*)0x1FFF7A18)

/** Numéros d'urgence autorisés */
#define EMERGENCY_NUMBERS_COUNT             5

/* ======================================================================== */
/*                VARIABLES STATIQUES                                       */
/* ======================================================================== */

/** Numéros d'urgence */
static const char* EMERGENCY_NUMBERS[EMERGENCY_NUMBERS_COUNT] = {
    "112",   /**< Urgences européennes    */
    "15",    /**< SAMU France             */
    "17",    /**< Police France           */
    "18",    /**< Pompiers France         */
    "911",   /**< Urgences US/international */
};

/** Noms des jours en français */
static const char* DAY_NAMES[] = {
    "Dimanche", "Lundi", "Mardi", "Mercredi",
    "Jeudi", "Vendredi", "Samedi"
};

/** Noms des mois en français */
static const char* MONTH_NAMES[] = {
    "Janvier", "Fevrier", "Mars", "Avril",
    "Mai", "Juin", "Juillet", "Aout",
    "Septembre", "Octobre", "Novembre", "Decembre"
};

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

/* --- États --- */
static void enter_locked_state(ScreenLock_t* screen);
static void enter_entering_state(ScreenLock_t* screen);
static void enter_error_state(ScreenLock_t* screen);
static void enter_blocked_state(ScreenLock_t* screen);
static void enter_unlocked_state(ScreenLock_t* screen);

/* --- Sécurité --- */
static void load_security_state(ScreenLock_t* screen);
static void save_security_state(ScreenLock_t* screen);
static bool verify_pin_internal(ScreenLock_t* screen);
static void handle_failed_attempt(ScreenLock_t* screen);
static void handle_successful_unlock(ScreenLock_t* screen);

/* --- UI --- */
static void create_lock_ui(ScreenLock_t* screen);
static void draw_clock(ScreenLock_t* screen);
static void draw_pin_dots(ScreenLock_t* screen);
static void draw_attempts_remaining(ScreenLock_t* screen);
static void draw_blocked_countdown(ScreenLock_t* screen);
static void update_clock_display(ScreenLock_t* screen);
static void update_pin_display(ScreenLock_t* screen);
static void clear_pin_display(ScreenLock_t* screen);
static void show_pin_error(ScreenLock_t* screen);
static void show_shake_animation(ScreenLock_t* screen);
static void play_key_tone(void);
static void play_error_tone(void);
static void play_unlock_tone(void);

/* --- Pavé numérique --- */
static void on_numpad_key(void* context, char key);
static void on_numpad_complete(void* context, const char* code, uint8_t length);
static void on_emergency_clicked(void* context);
static void on_forgot_pin_clicked(void* context);

/* --- Timers --- */
static void clock_timer_callback(TimerHandle_t timer);
static void block_timer_callback(TimerHandle_t timer);
static void shake_timer_callback(TimerHandle_t timer);

/* --- Utilitaires --- */
static void get_current_time_strings(ScreenLock_t* screen);
static uint32_t get_current_timestamp(void);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise l'écran de verrouillage
 */
bool ScreenLock_Init(ScreenLock_t* screen,
                     SettingsService_t* settings_service)
{
    if (!screen || !settings_service) {
        DEBUG_ERROR(TAG, "Paramètres invalides");
        return false;
    }

    DEBUG_INFO(TAG, "Initialisation de l'écran de verrouillage...");

    /* Mise à zéro */
    memset(screen, 0, sizeof(ScreenLock_t));

    /* Classe de base */
    ScreenBase_Init(&screen->base, SCREEN_ID_LOCK, "Verrouillage");

    /* Services */
    screen->settings_service = settings_service;
    screen->audio_manager = AudioManager_GetInstance();
    screen->power_manager = PowerManager_GetInstance();

    /* État initial */
    screen->state = LOCK_STATE_LOCKED;
    screen->mode = LOCK_MODE_NORMAL;

    /* Charger l'état de sécurité */
    load_security_state(screen);

    /* Si pas de PIN configuré, pas besoin de verrouillage */
    if (!screen->security.pin_configured) {
        DEBUG_INFO(TAG, "Aucun PIN configuré, verrouillage désactivé");
        screen->state = LOCK_STATE_UNLOCKED;
        return true;
    }

    /* Vérifier si bloqué */
    if (screen->security.blocked_until > 0) {
        uint32_t now = get_current_timestamp();
        if (now < screen->security.blocked_until) {
            screen->state = LOCK_STATE_BLOCKED;
            DEBUG_INFO(TAG, "Téléphone bloqué jusqu'à timestamp %lu", 
                       screen->security.blocked_until);
        } else {
            /* Déblocage expiré, réinitialiser */
            screen->security.blocked_until = 0;
            screen->security.attempts_remaining = LOCK_MAX_ATTEMPTS;
            save_security_state(screen);
        }
    }

    /* Créer l'interface */
    create_lock_ui(screen);

    /* Timers */
    screen->clock_timer = Timer_Create("LockClock",
                                       CLOCK_UPDATE_MS,
                                       true,  /* auto-reload */
                                       clock_timer_callback,
                                       screen);
    screen->block_timer = Timer_Create("LockBlock",
                                       BLOCK_COUNTDOWN_MS,
                                       true,  /* auto-reload */
                                       block_timer_callback,
                                       screen);
    screen->shake_timer = Timer_Create("LockShake",
                                       SHAKE_ANIM_STEP_MS,
                                       true,  /* auto-reload */
                                       shake_timer_callback,
                                       screen);

    /* Initialiser l'horloge */
    get_current_time_strings(screen);

    DEBUG_INFO(TAG, "Initialisation terminée (PIN=%sconfiguré, blocage=%s)",
               screen->security.pin_configured ? "" : "non ",
               screen->state == LOCK_STATE_BLOCKED ? "oui" : "non");

    return true;
}

/**
 * @brief Affiche l'écran de verrouillage
 */
void ScreenLock_Show(ScreenLock_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Affichage de l'écran de verrouillage");

    /* Réinitialiser le buffer PIN */
    memset(screen->entered_pin, 0, sizeof(screen->entered_pin));
    screen->entered_pin_length = 0;

    /* Fond d'écran (fond uni sombre, pas de barre de statut) */
    Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);

    /* Déterminer l'état et afficher */
    switch (screen->state) {
        case LOCK_STATE_BLOCKED:
            enter_blocked_state(screen);
            break;
        case LOCK_STATE_LOCKED:
        default:
            enter_locked_state(screen);
            break;
    }

    /* Démarrer le timer d'horloge */
    Timer_Start(screen->clock_timer);

    screen->base.is_visible = true;
}

/**
 * @brief Masque l'écran de verrouillage
 */
void ScreenLock_Hide(ScreenLock_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Masquage de l'écran de verrouillage");

    Timer_Stop(screen->clock_timer);
    Timer_Stop(screen->block_timer);
    Timer_Stop(screen->shake_timer);

    screen->base.is_visible = false;
}

/**
 * @brief Mise à jour périodique
 */
void ScreenLock_Update(ScreenLock_t* screen)
{
    if (!screen || !screen->base.is_visible) return;

    /* L'horloge est mise à jour par le timer, rien à faire ici */
}

/**
 * @brief Gère les événements tactiles
 */
bool ScreenLock_HandleTouch(ScreenLock_t* screen,
                            const TouchEvent_t* event)
{
    if (!screen || !event) return false;

    /* Dialogue de confirmation */
    if (UIDialog_IsVisible(screen->confirm_dialog)) {
        return UIDialog_HandleTouch(screen->confirm_dialog, event);
    }
    if (UIDialog_IsVisible(screen->emergency_dialog)) {
        return UIDialog_HandleTouch(screen->emergency_dialog, event);
    }

    /* Si bloqué, ignorer tout sauf urgence */
    if (screen->state == LOCK_STATE_BLOCKED) {
        if (UIButton_HitTest(screen->emergency_button, event->x, event->y)) {
            if (event->type == TOUCH_EVENT_TAP) {
                UIButton_TriggerClick(screen->emergency_button);
            }
            return true;
        }
        return false;
    }

    /* Pavé numérique */
    if (screen->numpad) {
        return UINumpad_HandleTouch(screen->numpad, event);
    }

    /* Bouton urgence */
    if (UIButton_HitTest(screen->emergency_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->emergency_button);
        }
        return true;
    }

    /* Bouton PIN oublié */
    if (screen->forgot_pin_button) {
        if (UIButton_HitTest(screen->forgot_pin_button, event->x, event->y)) {
            if (event->type == TOUCH_EVENT_TAP) {
                UIButton_TriggerClick(screen->forgot_pin_button);
            }
            return true;
        }
    }

    return false;
}

/**
 * @brief Gère les touches physiques
 */
bool ScreenLock_HandleKey(ScreenLock_t* screen,
                          KeyCode_t key)
{
    if (!screen) return false;

    /* Si bloqué, ignorer */
    if (screen->state == LOCK_STATE_BLOCKED) {
        return false;
    }

    switch (key) {
        case KEY_0:
        case KEY_1:
        case KEY_2:
        case KEY_3:
        case KEY_4:
        case KEY_5:
        case KEY_6:
        case KEY_7:
        case KEY_8:
        case KEY_9: {
            /* Ajouter le chiffre */
            char digit = '0' + (key - KEY_0);
            if (screen->entered_pin_length < screen->security.pin_length) {
                screen->entered_pin[screen->entered_pin_length++] = digit;
                screen->entered_pin[screen->entered_pin_length] = '\0';
                play_key_tone();
                update_pin_display(screen);

                /* Vérification automatique si PIN complet */
                if (screen->entered_pin_length >= screen->security.pin_length) {
                    ScreenLock_VerifyPin(screen);
                }
            }
            return true;
        }

        case KEY_BACK:
        case KEY_DELETE:
            /* Effacer le dernier chiffre */
            if (screen->entered_pin_length > 0) {
                screen->entered_pin[--screen->entered_pin_length] = '\0';
                update_pin_display(screen);
            }
            return true;

        case KEY_OK:
        case KEY_SELECT:
            /* Valider manuellement */
            if (screen->entered_pin_length > 0) {
                ScreenLock_VerifyPin(screen);
            }
            return true;

        default:
            break;
    }

    return false;
}

/**
 * @brief Vérifie le PIN saisi
 */
PinVerifyResult_t ScreenLock_VerifyPin(ScreenLock_t* screen)
{
    if (!screen) return PIN_RESULT_INCORRECT;

    /* Vérifier la longueur minimale */
    if (screen->entered_pin_length < LOCK_PIN_MIN_DIGITS) {
        DEBUG_WARN(TAG, "PIN trop court: %d chiffres", screen->entered_pin_length);
        return PIN_RESULT_TOO_SHORT;
    }

    /* Vérifier si bloqué */
    if (screen->state == LOCK_STATE_BLOCKED) {
        return PIN_RESULT_BLOCKED;
    }

    DEBUG_INFO(TAG, "Vérification du PIN (%d chiffres)", screen->entered_pin_length);

    screen->state = LOCK_STATE_VERIFYING;

    /* Vérifier le PIN */
    bool correct = verify_pin_internal(screen);

    if (correct) {
        handle_successful_unlock(screen);
        return PIN_RESULT_SUCCESS;
    } else {
        handle_failed_attempt(screen);
        return PIN_RESULT_INCORRECT;
    }
}

/**
 * @brief Définit un nouveau code PIN
 */
bool ScreenLock_SetPin(ScreenLock_t* screen,
                       const char* new_pin,
                       uint8_t length)
{
    if (!screen || !new_pin) return false;

    if (length < LOCK_PIN_MIN_DIGITS || length > LOCK_PIN_MAX_DIGITS) {
        DEBUG_ERROR(TAG, "Longueur PIN invalide: %d", length);
        return false;
    }

    DEBUG_INFO(TAG, "Définition d'un nouveau PIN (%d chiffres)", length);

    /* Hasher le nouveau PIN */
    ScreenLock_HashPin(new_pin, length, screen->security.pin_hash);
    screen->security.pin_length = length;
    screen->security.pin_configured = true;
    screen->security.attempts_remaining = LOCK_MAX_ATTEMPTS;
    screen->security.blocked_until = 0;

    /* Sauvegarder */
    save_security_state(screen);

    return true;
}

/**
 * @brief Désactive le verrouillage
 */
void ScreenLock_DisablePin(ScreenLock_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Désactivation du PIN");

    memset(&screen->security, 0, sizeof(screen->security));
    save_security_state(screen);
}

/**
 * @brief Vérifie si le téléphone est verrouillé
 */
bool ScreenLock_IsLocked(ScreenLock_t* screen)
{
    if (!screen) return false;
    return screen->state != LOCK_STATE_UNLOCKED;
}

/**
 * @brief Vérifie si un PIN est configuré
 */
bool ScreenLock_IsPinConfigured(ScreenLock_t* screen)
{
    if (!screen) return false;
    return screen->security.pin_configured;
}

/**
 * @brief Force le verrouillage
 */
void ScreenLock_Lock(ScreenLock_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Verrouillage du téléphone");

    screen->state = LOCK_STATE_LOCKED;
    memset(screen->entered_pin, 0, sizeof(screen->entered_pin));
    screen->entered_pin_length = 0;

    if (screen->base.is_visible) {
        enter_locked_state(screen);
    } else {
        ScreenLock_Show(screen);
    }
}

/**
 * @brief Déverrouille le téléphone
 */
void ScreenLock_Unlock(ScreenLock_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Déverrouillage du téléphone");
    enter_unlocked_state(screen);
}

/**
 * @brief Réinitialise les tentatives
 */
void ScreenLock_ResetAttempts(ScreenLock_t* screen)
{
    if (!screen) return;

    screen->security.attempts_remaining = LOCK_MAX_ATTEMPTS;
    screen->security.blocked_until = 0;
    screen->security.block_count = 0;
    save_security_state(screen);
}

/**
 * @brief Active/désactive le verrouillage auto
 */
void ScreenLock_SetAutoLock(ScreenLock_t* screen, bool enabled)
{
    if (!screen) return;

    screen->security.auto_lock_enabled = enabled;
    save_security_state(screen);

    if (enabled && screen->power_manager) {
        PowerManager_SetScreenTimeout(screen->power_manager,
                                      screen->security.auto_lock_timeout);
    }
}

/**
 * @brief Définit le délai de verrouillage auto
 */
void ScreenLock_SetAutoLockTimeout(ScreenLock_t* screen, uint16_t timeout_sec)
{
    if (!screen) return;

    screen->security.auto_lock_timeout = timeout_sec;
    save_security_state(screen);
}

/**
 * @brief Ouvre l'appel d'urgence
 */
void ScreenLock_OpenEmergencyCall(ScreenLock_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Ouverture appel d'urgence");

    screen->state = LOCK_STATE_EMERGENCY_CALL;

    /* Afficher les numéros d'urgence */
    char emergency_msg[256];
    snprintf(emergency_msg, sizeof(emergency_msg),
             "APPEL D'URGENCE\n\n"
             "Numeros disponibles :\n"
             "  • %s\n  • %s\n  • %s\n  • %s\n  • %s\n\n"
             "Appuyez sur un numero\n"
             "pour appeler.",
             EMERGENCY_NUMBERS[0],
             EMERGENCY_NUMBERS[1],
             EMERGENCY_NUMBERS[2],
             EMERGENCY_NUMBERS[3],
             EMERGENCY_NUMBERS[4]);

    UIDialog_SetTitle(screen->emergency_dialog, "URGENCE");
    UIDialog_SetMessage(screen->emergency_dialog, emergency_msg);
    UIDialog_SetVisible(screen->emergency_dialog, true);

    if (screen->base.is_visible) {
        UIDialog_Draw(screen->emergency_dialog);
    }
}

/**
 * @brief PIN oublié → reset usine
 */
void ScreenLock_ForgotPin(ScreenLock_t* screen)
{
    if (!screen) return;

    DEBUG_WARN(TAG, "PIN oublié - Proposition reset usine");

    screen->state = LOCK_STATE_FORGOT_PIN;

    UIDialog_SetTitle(screen->confirm_dialog, "PIN OUBLIE");
    UIDialog_SetMessage(screen->confirm_dialog,
                        "Si vous avez oublie votre PIN,\n"
                        "la seule option est de reinitialiser\n"
                        "le telephone aux parametres d'usine.\n\n"
                        "⚠️  TOUTES les donnees seront\n"
                        "    EFFACEES !\n\n"
                        "Continuer ?");
    UIDialog_SetOnResult(screen->confirm_dialog,
                        (void(*)(void*, bool))on_forgot_pin_confirmed,
                        screen);
    UIDialog_SetVisible(screen->confirm_dialog, true);

    if (screen->base.is_visible) {
        UIDialog_Draw(screen->confirm_dialog);
    }
}

/**
 * @brief Callback confirmation reset usine
 */
static void on_forgot_pin_confirmed(void* context, bool confirmed)
{
    ScreenLock_t* screen = (ScreenLock_t*)context;
    if (!screen) return;

    UIDialog_SetVisible(screen->confirm_dialog, false);

    if (confirmed) {
        DEBUG_WARN(TAG, "RESET USINE CONFIRMÉ - Effacement des données");

        /* Message */
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
        Display_DrawText(CENTER_X - 80, CENTER_Y - 10,
                        "Reinitialisation...",
                        &font_large, THEME_TEXT_PRIMARY, THEME_BG_MAIN);
        Display_SwapBuffers();

        /* Effacer la configuration */
        SettingsService_FactoryReset(screen->settings_service);

        /* Redémarrer */
        HAL_Delay(2000);
        HAL_NVIC_SystemReset();

        while (1) {}
    } else {
        screen->state = LOCK_STATE_LOCKED;
        if (screen->base.is_visible) {
            Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
            enter_locked_state(screen);
        }
    }
}

/**
 * @brief Libère les ressources
 */
void ScreenLock_Deinit(ScreenLock_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Libération des ressources verrouillage");

    Timer_Delete(screen->clock_timer);
    Timer_Delete(screen->block_timer);
    Timer_Delete(screen->shake_timer);

    UILabel_Destroy(screen->clock_label);
    UILabel_Destroy(screen->date_label);
    UILabel_Destroy(screen->day_label);
    UILabel_Destroy(screen->title_label);
    UILabel_Destroy(screen->message_label);
    UILabel_Destroy(screen->attempts_label);
    UILabel_Destroy(screen->blocked_label);

    for (int i = 0; i < LOCK_PIN_MAX_DIGITS; i++) {
        UILabel_Destroy(screen->pin_dots[i]);
    }

    if (screen->numpad) {
        UINumpad_Destroy(screen->numpad);
    }

    UIButton_Destroy(screen->emergency_button);
    UIButton_Destroy(screen->forgot_pin_button);

    UIDialog_Destroy(screen->emergency_dialog);
    UIDialog_Destroy(screen->confirm_dialog);

    memset(screen, 0, sizeof(ScreenLock_t));
}

/* ======================================================================== */
/*              GESTION DES ÉTATS                                           */
/* ======================================================================== */

static void enter_locked_state(ScreenLock_t* screen)
{
    if (!screen) return;

    screen->state = LOCK_STATE_LOCKED;

    /* Fond */
    Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);

    /* Titre */
    UILabel_Draw(screen->title_label);

    /* Horloge */
    get_current_time_strings(screen);
    update_clock_display(screen);

    /* Icône cadenas */
    UIIcons_Draw(screen->lock_icon, CENTER_X - 16, LOCK_ICON_Y, THEME_ACCENT);

    /* Message */
    UILabel_SetText(screen->message_label, "Entrez le code");
    UILabel_Draw(screen->message_label);

    /* Points PIN (vides) */
    clear_pin_display(screen);
    draw_pin_dots(screen);

    /* Pavé numérique */
    UINumpad_Draw(screen->numpad);

    /* Bouton urgence */
    UIButton_Draw(screen->emergency_button);

    /* Tentatives restantes (si < max) */
    if (screen->security.attempts_remaining < LOCK_MAX_ATTEMPTS) {
        draw_attempts_remaining(screen);
    }

    /* Bouton PIN oublié (après 1er échec) */
    if (screen->security.attempts_remaining <= LOCK_MAX_ATTEMPTS - 1) {
        if (!screen->forgot_pin_button) {
            screen->forgot_pin_button = UIButton_Create();
            UIButton_SetText(screen->forgot_pin_button, "PIN oublie ?");
            UIButton_SetFont(screen->forgot_pin_button, &font_small);
            UIButton_SetSize(screen->forgot_pin_button, 120, 28);
            UIButton_SetPosition(screen->forgot_pin_button, CENTER_X - 60, 470);
            UIButton_SetOnClick(screen->forgot_pin_button, on_forgot_pin_clicked, screen);
            UIButton_SetCornerRadius(screen->forgot_pin_button, 6);
            UIButton_SetColor(screen->forgot_pin_button, THEME_BUTTON_DANGER);
        }
        UIButton_Draw(screen->forgot_pin_button);
    }

    /* Démarrer le timer d'horloge */
    Timer_Start(screen->clock_timer);
}

static void enter_entering_state(ScreenLock_t* screen)
{
    screen->state = LOCK_STATE_ENTERING_PIN;
    /* L'horloge continue de s'afficher */
}

static void enter_error_state(ScreenLock_t* screen)
{
    screen->state = LOCK_STATE_ERROR;

    /* Animation shake */
    screen->shake_count = 0;
    screen->shake_offset_x = 0;
    Timer_Start(screen->shake_timer);

    /* Afficher message d'erreur */
    UILabel_SetText(screen->message_label, "Code incorrect !");
    UILabel_SetColor(screen->message_label, THEME_DANGER);
    UILabel_Draw(screen->message_label);

    /* Jouer le son d'erreur */
    play_error_tone();

    /* Effacer le PIN */
    memset(screen->entered_pin, 0, sizeof(screen->entered_pin));
    screen->entered_pin_length = 0;
}

static void enter_blocked_state(ScreenLock_t* screen)
{
    screen->state = LOCK_STATE_BLOCKED;

    /* Masquer le pavé numérique */
    UINumpad_SetVisible(screen->numpad, false);

    /* Fond rouge foncé */
    Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x3000);

    /* Message de blocage */
    UILabel_SetText(screen->message_label, "Telephone bloque");
    UILabel_SetColor(screen->message_label, THEME_DANGER);
    UILabel_Draw(screen->message_label);

    /* Décompte */
    draw_blocked_countdown(screen);

    /* Bouton urgence toujours accessible */
    UIButton_Draw(screen->emergency_button);

    /* Timer de décompte */
    Timer_Start(screen->block_timer);
}

static void enter_unlocked_state(ScreenLock_t* screen)
{
    screen->state = LOCK_STATE_UNLOCKED;

    /* Animation de déverrouillage */
    play_unlock_tone();

    /* Animation flash vert */
    for (int i = 0; i < 3; i++) {
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_SUCCESS);
        Display_SwapBuffers();
        HAL_Delay(80);
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
        Display_SwapBuffers();
        HAL_Delay(80);
    }

    Timer_Stop(screen->clock_timer);

    /* Notifier l'application */
    if (screen->on_unlocked) {
        screen->on_unlocked();
    }
}

/* ======================================================================== */
/*              SÉCURITÉ                                                    */
/* ======================================================================== */

static void load_security_state(ScreenLock_t* screen)
{
    if (!screen || !screen->settings_service) return;

    SettingsService_t* svc = screen->settings_service;

    /* Charger le hash du PIN */
    SettingsService_GetPinHash(svc, screen->security.pin_hash, LOCK_PIN_HASH_SIZE);

    /* Vérifier si un PIN est configuré (hash non nul) */
    screen->security.pin_configured = false;
    for (int i = 0; i < LOCK_PIN_HASH_SIZE; i++) {
        if (screen->security.pin_hash[i] != 0) {
            screen->security.pin_configured = true;
            break;
        }
    }

    /* Charger les autres paramètres */
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    if (SettingsService_GetPinLength(svc, &val8))
        screen->security.pin_length = val8;
    else
        screen->security.pin_length = 4;

    if (SettingsService_GetPinAttempts(svc, &val8))
        screen->security.attempts_remaining = val8;
    else
        screen->security.attempts_remaining = LOCK_MAX_ATTEMPTS;

    if (SettingsService_GetPinBlockCount(svc, &val8))
        screen->security.block_count = val8;
    else
        screen->security.block_count = 0;

    if (SettingsService_GetPinBlockedUntil(svc, &val32))
        screen->security.blocked_until = val32;
    else
        screen->security.blocked_until = 0;

    if (SettingsService_GetAutoLock(svc, &screen->security.auto_lock_enabled))
        {}
    else
        screen->security.auto_lock_enabled = true;

    if (SettingsService_GetAutoLockTimeout(svc, &val16))
        screen->security.auto_lock_timeout = val16;
    else
        screen->security.auto_lock_timeout = 30;

    DEBUG_VERBOSE(TAG, "État sécurité chargé: config=%s, tentatives=%d, blocages=%d",
                  screen->security.pin_configured ? "oui" : "non",
                  screen->security.attempts_remaining,
                  screen->security.block_count);
}

static void save_security_state(ScreenLock_t* screen)
{
    if (!screen || !screen->settings_service) return;

    SettingsService_t* svc = screen->settings_service;

    SettingsService_SetPinHash(svc, screen->security.pin_hash, LOCK_PIN_HASH_SIZE);
    SettingsService_SetPinLength(svc, screen->security.pin_length);
    SettingsService_SetPinAttempts(svc, screen->security.attempts_remaining);
    SettingsService_SetPinBlockCount(svc, screen->security.block_count);
    SettingsService_SetPinBlockedUntil(svc, screen->security.blocked_until);
    SettingsService_SetAutoLock(svc, screen->security.auto_lock_enabled);
    SettingsService_SetAutoLockTimeout(svc, screen->security.auto_lock_timeout);

    SettingsService_Save(svc);

    DEBUG_VERBOSE(TAG, "État sécurité sauvegardé");
}

static bool verify_pin_internal(ScreenLock_t* screen)
{
    if (!screen) return false;

    /* Hasher le PIN saisi */
    uint8_t input_hash[LOCK_PIN_HASH_SIZE];
    ScreenLock_HashPin(screen->entered_pin,
                       screen->entered_pin_length,
                       input_hash);

    /* Comparaison temps constant */
    return ScreenLock_ConstantTimeCompare(input_hash,
                                          screen->security.pin_hash,
                                          LOCK_PIN_HASH_SIZE);
}

static void handle_failed_attempt(ScreenLock_t* screen)
{
    if (!screen) return;

    screen->security.attempts_remaining--;

    DEBUG_WARN(TAG, "Échec vérification PIN - %d tentative(s) restante(s)",
               screen->security.attempts_remaining);

    if (screen->security.attempts_remaining <= 0) {
        /* Blocage */
        screen->security.block_count++;
        uint32_t block_duration = ScreenLock_GetBlockDuration(screen->security.block_count);
        screen->security.blocked_until = get_current_timestamp() + block_duration;

        DEBUG_WARN(TAG, "Téléphone bloqué pour %lu secondes", block_duration);

        save_security_state(screen);

        if (screen->base.is_visible) {
            enter_blocked_state(screen);
        }
    } else {
        save_security_state(screen);

        if (screen->base.is_visible) {
            enter_error_state(screen);

            /* Attendre la fin de l'animation puis revenir à l'état normal */
            /* Le timer shake gère la transition */
        }
    }
}

static void handle_successful_unlock(ScreenLock_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "PIN correct - Déverrouillage");

    /* Réinitialiser les tentatives */
    screen->security.attempts_remaining = LOCK_MAX_ATTEMPTS;
    screen->security.blocked_until = 0;
    screen->security.block_count = 0;
    save_security_state(screen);

    /* Déverrouiller */
    if (screen->base.is_visible) {
        enter_unlocked_state(screen);
    }
}

/* ======================================================================== */
/*              CRÉATION DE L'INTERFACE                                     */
/* ======================================================================== */

static void create_lock_ui(ScreenLock_t* screen)
{
    if (!screen) return;

    /* Titre */
    screen->title_label = UILabel_Create();
    UILabel_SetText(screen->title_label, "LoRa Phone");
    UILabel_SetFont(screen->title_label, &font_medium_bold);
    UILabel_SetColor(screen->title_label, THEME_ACCENT);
    UILabel_SetPosition(screen->title_label, CENTER_X - 40, 40);

    /* Horloge */
    screen->clock_label = UILabel_Create();
    UILabel_SetText(screen->clock_label, "00:00");
    UILabel_SetFont(screen->clock_label, &font_huge);  /* Grande police */
    UILabel_SetColor(screen->clock_label, THEME_TEXT_PRIMARY);
    UILabel_SetPosition(screen->clock_label, CENTER_X - 45, CLOCK_Y);

    /* Date */
    screen->date_label = UILabel_Create();
    UILabel_SetText(screen->date_label, "");
    UILabel_SetFont(screen->date_label, &font_medium);
    UILabel_SetColor(screen->date_label, THEME_TEXT_SECONDARY);
    UILabel_SetPosition(screen->date_label, CENTER_X - 50, DATE_Y);

    /* Jour */
    screen->day_label = UILabel_Create();
    UILabel_SetText(screen->day_label, "");
    UILabel_SetFont(screen->day_label, &font_small);
    UILabel_SetColor(screen->day_label, THEME_TEXT_TERTIARY);
    UILabel_SetPosition(screen->day_label, CENTER_X - 40, DAY_Y);

    /* Icône cadenas */
    screen->lock_icon = ICON_LOCK;

    /* Message */
    screen->message_label = UILabel_Create();
    UILabel_SetText(screen->message_label, "Entrez le code");
    UILabel_SetFont(screen->message_label, &font_medium);
    UILabel_SetColor(screen->message_label, THEME_TEXT_SECONDARY);
    UILabel_SetPosition(screen->message_label, CENTER_X - 60, MESSAGE_Y);

    /* Points PIN */
    screen->pin_dots_x = CENTER_X - ((LOCK_PIN_MAX_DIGITS - 1) * PIN_DOT_SPACING) / 2;
    screen->pin_dots_y = PIN_DOTS_Y;
    for (int i = 0; i < LOCK_PIN_MAX_DIGITS; i++) {
        screen->pin_dots[i] = UILabel_Create();
        UILabel_SetText(screen->pin_dots[i], "○");
        UILabel_SetFont(screen->pin_dots[i], &font_large_bold);
        UILabel_SetColor(screen->pin_dots[i], THEME_TEXT_TERTIARY);
        UILabel_SetPosition(screen->pin_dots[i],
                           screen->pin_dots_x + i * PIN_DOT_SPACING,
                           screen->pin_dots_y);
    }

    /* Pavé numérique */
    screen->numpad = UINumpad_Create();
    UINumpad_SetPosition(screen->numpad, CENTER_X - 120, NUMPAD_Y);
    UINumpad_SetMode(screen->numpad, NUMPAD_MODE_PIN);
    UINumpad_SetMaxDigits(screen->numpad, LOCK_PIN_MAX_DIGITS);
    UINumpad_SetOnKeyPress(screen->numpad, on_numpad_key, screen);
    UINumpad_SetOnComplete(screen->numpad, on_numpad_complete, screen);
    UINumpad_SetShowDelete(screen->numpad, true);
    UINumpad_SetShowOK(screen->numpad, false);

    /* Bouton urgence */
    screen->emergency_button = UIButton_Create();
    UIButton_SetText(screen->emergency_button, "🔴 Appel d'urgence");
    UIButton_SetFont(screen->emergency_button, &font_medium);
    UIButton_SetSize(screen->emergency_button, 220, 40);
    UIButton_SetPosition(screen->emergency_button, CENTER_X - 110, EMERGENCY_BUTTON_Y);
    UIButton_SetOnClick(screen->emergency_button, on_emergency_clicked, screen);
    UIButton_SetCornerRadius(screen->emergency_button, 8);
    UIButton_SetColor(screen->emergency_button, THEME_BUTTON_DANGER);
    UIButton_SetTextColor(screen->emergency_button, THEME_TEXT_ON_DANGER);

    /* Label tentatives */
    screen->attempts_label = UILabel_Create();
    UILabel_SetText(screen->attempts_label, "");
    UILabel_SetFont(screen->attempts_label, &font_small);
    UILabel_SetColor(screen->attempts_label, THEME_TEXT_TERTIARY);
    UILabel_SetPosition(screen->attempts_label, CENTER_X - 60, ATTEMPTS_Y);

    /* Dialogue urgence */
    screen->emergency_dialog = UIDialog_Create();
    UIDialog_SetVisible(screen->emergency_dialog, false);

    /* Dialogue confirmation */
    screen->confirm_dialog = UIDialog_Create();
    UIDialog_SetVisible(screen->confirm_dialog, false);

    /* Label blocage */
    screen->blocked_label = UILabel_Create();
    UILabel_SetText(screen->blocked_label, "");
    UILabel_SetFont(screen->blocked_label, &font_medium_bold);
    UILabel_SetColor(screen->blocked_label, THEME_DANGER);
    UILabel_SetPosition(screen->blocked_label, CENTER_X - 80, 240);

    /* Bouton PIN oublié (créé à la demande) */
    screen->forgot_pin_button = NULL;
}

/* ======================================================================== */
/*              RENDU                                                       */
/* ======================================================================== */

static void draw_clock(ScreenLock_t* screen)
{
    if (!screen) return;
    get_current_time_strings(screen);
    update_clock_display(screen);
}

static void draw_pin_dots(ScreenLock_t* screen)
{
    if (!screen) return;

    for (int i = 0; i < screen->security.pin_length; i++) {
        if (i < screen->entered_pin_length) {
            UILabel_SetText(screen->pin_dots[i], "●");
            UILabel_SetColor(screen->pin_dots[i], THEME_SUCCESS);
        } else {
            UILabel_SetText(screen->pin_dots[i], "○");
            UILabel_SetColor(screen->pin_dots[i], THEME_TEXT_TERTIARY);
        }
        UILabel_Draw(screen->pin_dots[i]);
    }
}

static void draw_attempts_remaining(ScreenLock_t* screen)
{
    if (!screen) return;

    char buf[32];
    if (screen->security.attempts_remaining == 1) {
        snprintf(buf, sizeof(buf), "⚠️ Derniere tentative !");
        UILabel_SetColor(screen->attempts_label, THEME_WARNING);
    } else {
        snprintf(buf, sizeof(buf), "%d tentatives restantes",
                 screen->security.attempts_remaining);
        UILabel_SetColor(screen->attempts_label, THEME_TEXT_TERTIARY);
    }
    UILabel_SetText(screen->attempts_label, buf);
    UILabel_Draw(screen->attempts_label);
}

static void draw_blocked_countdown(ScreenLock_t* screen)
{
    if (!screen) return;

    uint32_t now = get_current_timestamp();
    int32_t remaining = (int32_t)(screen->security.blocked_until - now);

    if (remaining <= 0) {
        /* Déblocage */
        screen->security.blocked_until = 0;
        screen->security.attempts_remaining = LOCK_MAX_ATTEMPTS;
        save_security_state(screen);

        Timer_Stop(screen->block_timer);
        UINumpad_SetVisible(screen->numpad, true);

        if (screen->base.is_visible) {
            Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
            enter_locked_state(screen);
        }
        return;
    }

    /* Afficher le décompte */
    char buf[64];
    int minutes = remaining / 60;
    int seconds = remaining % 60;

    if (minutes > 0) {
        snprintf(buf, sizeof(buf), "Bloque\n%02d:%02d restantes", minutes, seconds);
    } else {
        snprintf(buf, sizeof(buf), "Bloque\n%02d secondes restantes", seconds);
    }

    UILabel_SetText(screen->blocked_label, buf);
    UILabel_Draw(screen->blocked_label);
}

static void update_clock_display(ScreenLock_t* screen)
{
    if (!screen) return;

    UILabel_SetText(screen->clock_label, screen->time_str);
    UILabel_SetText(screen->date_label, screen->date_str);
    UILabel_SetText(screen->day_label, screen->day_str);

    if (screen->base.is_visible) {
        UILabel_Draw(screen->clock_label);
        UILabel_Draw(screen->date_label);
        UILabel_Draw(screen->day_label);
    }
}

static void update_pin_display(ScreenLock_t* screen)
{
    if (!screen) return;

    /* Redessiner les points */
    for (int i = 0; i < screen->security.pin_length; i++) {
        if (i < screen->entered_pin_length) {
            UILabel_SetText(screen->pin_dots[i], "●");
            UILabel_SetColor(screen->pin_dots[i], THEME_SUCCESS);
        } else {
            UILabel_SetText(screen->pin_dots[i], "○");
            UILabel_SetColor(screen->pin_dots[i], THEME_TEXT_TERTIARY);
        }
        UILabel_Draw(screen->pin_dots[i]);
    }

    /* Effacer le message d'erreur */
    UILabel_SetText(screen->message_label, "Entrez le code");
    UILabel_SetColor(screen->message_label, THEME_TEXT_SECONDARY);
    UILabel_Draw(screen->message_label);
}

static void clear_pin_display(ScreenLock_t* screen)
{
    if (!screen) return;

    for (int i = 0; i < screen->security.pin_length; i++) {
        UILabel_SetText(screen->pin_dots[i], "○");
        UILabel_SetColor(screen->pin_dots[i], THEME_TEXT_TERTIARY);
        UILabel_Draw(screen->pin_dots[i]);
    }
}

static void show_pin_error(ScreenLock_t* screen)
{
    if (!screen) return;

    /* Effacer les points */
    clear_pin_display(screen);

    /* Message rouge */
    UILabel_SetText(screen->message_label, "Code incorrect !");
    UILabel_SetColor(screen->message_label, THEME_DANGER);
    UILabel_Draw(screen->message_label);

    /* Afficher tentatives restantes */
    draw_attempts_remaining(screen);
}

static void show_shake_animation(ScreenLock_t* screen)
{
    if (!screen || !screen->base.is_visible) return;

    /* Calculer l'offset de shake */
    if (screen->shake_count < SHAKE_ANIM_STEPS) {
        /* Mouvement sinusoïdal amorti */
        float t = (float)screen->shake_count / SHAKE_ANIM_STEPS;
        float amplitude = SHAKE_AMPLITUDE * (1.0f - t);  /* Amortissement */
        int16_t offset = (int16_t)(amplitude * sinf(t * 3.14159f * 6));

        /* Déplacer l'affichage */
        DMA2D_MoveRect(offset, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

        screen->shake_count++;
    } else {
        /* Fin de l'animation */
        Timer_Stop(screen->shake_timer);
        screen->shake_offset_x = 0;

        /* Redessiner normalement */
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
        show_pin_error(screen);
        draw_pin_dots(screen);
        UINumpad_Draw(screen->numpad);
        UIButton_Draw(screen->emergency_button);

        /* Attendre 1.5s puis réafficher l'état normal */
        HAL_Delay(1500);
        UILabel_SetText(screen->message_label, "Entrez le code");
        UILabel_SetColor(screen->message_label, THEME_TEXT_SECONDARY);
        UILabel_Draw(screen->message_label);
        clear_pin_display(screen);
        draw_pin_dots(screen);

        screen->state = LOCK_STATE_LOCKED;
    }
}

/* ======================================================================== */
/*              TONALITÉS AUDIO                                             */
/* ======================================================================== */

static void play_key_tone(void)
{
    /* Bip court 440 Hz, 50ms */
    /* AudioManager_PlayTone(440, 50); */
    /* Simplifié : utiliser le buzzer */
}

static void play_error_tone(void)
{
    /* Double bip grave 220 Hz */
    /* AudioManager_PlayTone(220, 100); */
    /* HAL_Delay(120); */
    /* AudioManager_PlayTone(220, 150); */
}

static void play_unlock_tone(void)
{
    /* Mélodie montante */
    /* AudioManager_PlayTone(523, 100);  // Do */
    /* HAL_Delay(110); */
    /* AudioManager_PlayTone(659, 100);  // Mi */
    /* HAL_Delay(110); */
    /* AudioManager_PlayTone(784, 150);  // Sol */
}

/* ======================================================================== */
/*              CALLBACKS                                                   */
/* ======================================================================== */

static void on_numpad_key(void* context, char key)
{
    ScreenLock_t* screen = (ScreenLock_t*)context;
    if (!screen) return;

    DEBUG_VERBOSE(TAG, "Touche pavé: %c", key);

    if (key == '\b') {
        /* Effacer */
        if (screen->entered_pin_length > 0) {
            screen->entered_pin[--screen->entered_pin_length] = '\0';
            update_pin_display(screen);
        }
        return;
    }

    if (key >= '0' && key <= '9') {
        /* Ajouter le chiffre */
        if (screen->entered_pin_length < screen->security.pin_length) {
            screen->entered_pin[screen->entered_pin_length++] = key;
            screen->entered_pin[screen->entered_pin_length] = '\0';
            play_key_tone();
            update_pin_display(screen);

            /* Vérification automatique si PIN complet */
            if (screen->entered_pin_length >= screen->security.pin_length) {
                /* Petit délai pour voir le dernier point */
                HAL_Delay(200);
                ScreenLock_VerifyPin(screen);
            }
        }
    }
}

static void on_numpad_complete(void* context, const char* code, uint8_t length)
{
    ScreenLock_t* screen = (ScreenLock_t*)context;
    if (!screen) return;

    /* Vérifier le PIN (déjà fait dans on_numpad_key) */
    ScreenLock_VerifyPin(screen);
}

static void on_emergency_clicked(void* context)
{
    ScreenLock_t* screen = (ScreenLock_t*)context;
    if (!screen) return;

    DEBUG_INFO(TAG, "Bouton urgence cliqué");
    ScreenLock_OpenEmergencyCall(screen);
}

static void on_forgot_pin_clicked(void* context)
{
    ScreenLock_t* screen = (ScreenLock_t*)context;
    if (!screen) return;

    DEBUG_INFO(TAG, "Bouton PIN oublié cliqué");
    ScreenLock_ForgotPin(screen);
}

/* ======================================================================== */
/*              TIMERS                                                      */
/* ======================================================================== */

static void clock_timer_callback(TimerHandle_t timer)
{
    ScreenLock_t* screen = (ScreenLock_t*)Timer_GetContext(timer);
    if (!screen) return;

    /* Mettre à jour l'horloge */
    get_current_time_strings(screen);

    if (screen->base.is_visible &&
        (screen->state == LOCK_STATE_LOCKED ||
         screen->state == LOCK_STATE_ENTERING_PIN ||
         screen->state == LOCK_STATE_ERROR)) {
        update_clock_display(screen);
    }
}

static void block_timer_callback(TimerHandle_t timer)
{
    ScreenLock_t* screen = (ScreenLock_t*)Timer_GetContext(timer);
    if (!screen) return;

    if (screen->state == LOCK_STATE_BLOCKED && screen->base.is_visible) {
        draw_blocked_countdown(screen);
    }
}

static void shake_timer_callback(TimerHandle_t timer)
{
    ScreenLock_t* screen = (ScreenLock_t*)Timer_GetContext(timer);
    if (!screen) return;

    if (screen->state == LOCK_STATE_ERROR && screen->base.is_visible) {
        show_shake_animation(screen);
    }
}

/* ======================================================================== */
/*              UTILITAIRES                                                 */
/* ======================================================================== */

static void get_current_time_strings(ScreenLock_t* screen)
{
    if (!screen) return;

    /* Obtenir l'heure actuelle */
    /* En production : utiliser le RTC du STM32 */
    /* Pour l'exemple : utiliser le temps simulé */

    time_t now;
    struct tm time_info;

    /* Récupérer l'heure depuis le RTC */
    /* RTC_GetTime(&time_info); */
    /* RTC_GetDate(&time_info); */

    /* Fallback : utiliser HAL_GetTick pour une approximation */
    now = time(NULL);  /* Nécessite RTC configuré */
    localtime_r(&now, &time_info);

    /* Formater l'heure */
    snprintf(screen->time_str, sizeof(screen->time_str),
             "%02d:%02d",
             time_info.tm_hour, time_info.tm_min);

    /* Formater la date */
    snprintf(screen->date_str, sizeof(screen->date_str),
             "%s %d %s",
             DAY_NAMES[time_info.tm_wday],
             time_info.tm_mday,
             MONTH_NAMES[time_info.tm_mon]);

    /* Formater le jour */
    snprintf(screen->day_str, sizeof(screen->day_str),
             "%s",
             DAY_NAMES[time_info.tm_wday]);
}

static uint32_t get_current_timestamp(void)
{
    /* Retourner le timestamp Unix actuel depuis le RTC */
    /* return RTC_GetTimestamp(); */
    return (uint32_t)time(NULL);
}

/* ======================================================================== */
/*              HASHAGE DU PIN                                              */
/* ======================================================================== */

void ScreenLock_HashPin(const char* pin,
                        uint8_t length,
                        uint8_t* hash_out)
{
    if (!pin || !hash_out || length == 0) return;

    /* 1. Récupérer l'UID unique du STM32 (96 bits = 12 octets) */
    uint8_t uid[12];
    uint32_t uid0 = *UID_ADDR_0;
    uint32_t uid1 = *UID_ADDR_1;
    uint32_t uid2 = *UID_ADDR_2;

    memcpy(uid, &uid0, 4);
    memcpy(uid + 4, &uid1, 4);
    memcpy(uid + 8, &uid2, 4);

    /* 2. Construire le message : PIN + UID */
    uint8_t message[LOCK_PIN_MAX_DIGITS + 12];
    memcpy(message, pin, length);
    memcpy(message + length, uid, 12);

    /* 3. Hasher avec SHA-256 */
    CryptoUtils_SHA256(message, length + 12, hash_out);
}

bool ScreenLock_ConstantTimeCompare(const uint8_t* hash_a,
                                    const uint8_t* hash_b,
                                    uint8_t length)
{
    if (!hash_a || !hash_b) return false;

    uint8_t diff = 0;

    /* Comparaison XOR cumulative : temps constant */
    for (uint8_t i = 0; i < length; i++) {
        diff |= hash_a[i] ^ hash_b[i];
    }

    return diff == 0;
}

uint32_t ScreenLock_GetBlockDuration(uint8_t block_count)
{
    switch (block_count) {
        case 0:
        case 1:
            return LOCK_BLOCK_DURATION_1ST;   /**< 30 secondes    */
        case 2:
            return LOCK_BLOCK_DURATION_2ND;   /**< 1 minute       */
        case 3:
            return LOCK_BLOCK_DURATION_3RD;   /**< 5 minutes      */
        default:
            return LOCK_BLOCK_DURATION_4TH;   /**< 15 minutes     */
    }
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */