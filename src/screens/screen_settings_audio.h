/**
 * @file    screen_settings_audio.h
 * @brief   Écran des réglages audio - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Cet écran permet de configurer tous les paramètres audio du téléphone.
 * Il est accessible depuis l'écran Paramètres → Audio.
 * 
 * ORGANISATION DE L'ÉCRAN :
 * ┌──────────────────────────────────────────────────────────┐
 * │  ← Audio                                       12:45 ███ │ ← Barre statut
 * │──────────────────────────────────────────────────────────│
 * │                                                          │
 * │  VOLUME MICROPHONE                                       │
 * │  [████████████████░░░░]  80%                             │ ← Slider + %
 * │  ─────────────────────────●─────────────────────────      │
 * │  Niveau actuel: ▁▃▅█▆▄▂▁                                │ ← VU-mètre micro
 * │                                                          │
 * │  VOLUME HAUT-PARLEUR                                     │
 * │  [███████████████░░░░░]  75%                             │ ← Slider + %
 * │  ───────────────────────────●───────────────────────      │
 * │  [▶] Tester                     [✓] OK                   │ ← Bouton test son
 * │                                                          │
 * │  VOLUME SONNERIE                                         │
 * │  [████████████████░░░░]  85%                              │ ← Slider + %
 * │  ───────────────────────────●───────────────────────      │
 * │  [♪] Écouter mélodie                                    │ ← Bouton écoute
 * │                                                          │
 * │  MODE SILENCE                                            │
 * │  [○] Désactivé    [●] Activé                             │ ← Switch ON/OFF
 * │                                                          │
 * │  VIBREUR                                                 │
 * │  [○] Désactivé    [●] Activé    [Test]                   │ ← Switch + test
 * │                                                          │
 * │  MODE AUDIO                                               │
 * │  ◉ Haut-parleur  ○ Écouteur  ○ Casque                   │ ← Radio buttons
 * │                                                          │
 * │  RÉDUCTION DE BRUIT (DSP)                                │
 * │  [○] Désactivée  [●] Faible  ○ Moyen  ○ Fort            │ ← Radio buttons
 * │                                                          │
 * │  GAIN MICRO (ampli MAX9814)                              │
 * │  [████████████░░░░░░]  60%                                │ ← Slider gain
 * │  ───────────────────────●───────────────────────────      │
 * │                                                          │
 * │──────────────────────────────────────────────────────────│
 * │  [Appliquer]              [Valeurs par défaut]            │
 * └──────────────────────────────────────────────────────────┘
 * 
 * ARCHITECTURE AUDIO STM32F429 :
 * 
 * Chaîne d'entrée (Microphone) :
 *   MAX9814/MAX4466 → ADC1_IN4 (GPIO34/PA4) → DMA2 Stream0
 *   Résolution : 12 bits
 *   Fréquence échantillonnage : 8 kHz (voix) ou 16 kHz (qualité)
 *   Buffer DMA circulaire : 512 échantillons
 * 
 * Chaîne de sortie (Haut-parleur) :
 *   Buffer audio → DAC1_OUT1 (GPIO25/PA4) → Ampli PAM8403 → HP 3W
 *   Résolution : 12 bits
 *   Fréquence : 8 kHz ou 16 kHz
 *   Buffer DMA circulaire : 512 échantillons
 * 
 * Traitement DSP (optionnel) :
 *   - Filtre passe-bas 3.4 kHz (anti-aliasing)
 *   - Filtre passe-haut 300 Hz (élimination basses)
 *   - Réduction de bruit (algorithme simple)
 *   - Compression/expansion (compandeur)
 *   - ADPCM pour transmission LoRa
 * 
 * VOLUME ET GAIN :
 * 
 * Le volume est géré à plusieurs niveaux :
 * 1. Gain analogique (MAX9814) : réglable par potentiomètre numérique
 * 2. Gain numérique (multiplication logicielle) : 0.0x à 4.0x
 * 3. Atténuation DAC : réglable par registre
 * 4. Volume amplificateur PAM8403 : fixe (réglable par potentiomètre)
 * 
 * Le slider "Volume microphone" contrôle le gain numérique (niveau 2).
 * Le slider "Gain micro" contrôle le gain analogique si disponible.
 */

#ifndef SCREEN_SETTINGS_AUDIO_H
#define SCREEN_SETTINGS_AUDIO_H

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
#include "../ui/ui_vu_meter.h"                  // VU-mètre (barre de niveau)
#include "../ui/ui_statusbar.h"                 // Barre de statut
#include "../ui/ui_navigation.h"                // Navigation
#include "../ui/ui_icons.h"                     // Icônes 🔊🎤🔔
#include "../services/settings_service.h"       // Persistance paramètres
#include "../drivers/audio/audio_manager.h"     // Gestionnaire audio
#include "../drivers/audio/audio_adc.h"         // ADC micro
#include "../drivers/audio/audio_dac.h"         // DAC haut-parleur
#include "../drivers/audio/audio_mixer.h"       // Mixage volumes
#include "../app/app_events.h"                  // Événements
#include "../utils/timer_utils.h"               // Timers

/* ======================================================================== */
/*                     CONSTANTES SYMBOLIQUES                                */
/* ======================================================================== */

/**
 * @brief Nombre de paramètres audio configurables
 */
#define AUDIO_PARAM_COUNT                   8

/**
 * @brief Identifiants des paramètres audio
 */
typedef enum {
    AUDIO_PARAM_MIC_VOLUME = 0,         /**< Volume microphone (0-100%)       */
    AUDIO_PARAM_SPEAKER_VOLUME,         /**< Volume haut-parleur (0-100%)     */
    AUDIO_PARAM_RINGTONE_VOLUME,        /**< Volume sonnerie (0-100%)         */
    AUDIO_PARAM_SILENT_MODE,            /**< Mode silence (ON/OFF)            */
    AUDIO_PARAM_VIBRATOR,               /**< Vibreur (ON/OFF)                 */
    AUDIO_PARAM_AUDIO_MODE,             /**< Mode audio (HP/Écouteur/Casque)  */
    AUDIO_PARAM_NOISE_REDUCTION,        /**< Réduction de bruit (Off/Low/Med/High) */
    AUDIO_PARAM_MIC_GAIN,               /**< Gain micro analogique (0-100%)   */
} AudioParamId_t;

/**
 * @brief États de l'écran audio
 */
typedef enum {
    AUDIO_STATE_IDLE,                   /**< Affichage normal                 */
    AUDIO_STATE_TESTING_SPEAKER,        /**< Test haut-parleur en cours       */
    AUDIO_STATE_TESTING_RINGTONE,       /**< Test sonnerie en cours           */
    AUDIO_STATE_TESTING_VIBRATOR,       /**< Test vibreur en cours            */
    AUDIO_STATE_VU_METER_ACTIVE,        /**< VU-mètre micro actif             */
    AUDIO_STATE_APPLYING,               /**< Application des changements      */
} AudioScreenState_t;

/**
 * @brief Modes de sortie audio
 */
typedef enum {
    AUDIO_MODE_SPEAKER = 0,             /**< Haut-parleur intégré             */
    AUDIO_MODE_EARPIECE,                /**< Écouteur (faible puissance)      */
    AUDIO_MODE_HEADSET,                 /**< Casque/Main-libres               */
    AUDIO_MODE_COUNT
} AudioOutputMode_t;

/**
 * @brief Niveaux de réduction de bruit
 */
typedef enum {
    NOISE_REDUCTION_OFF = 0,            /**< Désactivée                       */
    NOISE_REDUCTION_LOW,                /**< Faible                           */
    NOISE_REDUCTION_MEDIUM,             /**< Moyen                            */
    NOISE_REDUCTION_HIGH,               /**< Fort                             */
    NOISE_REDUCTION_COUNT
} NoiseReductionLevel_t;

/* ======================================================================== */
/*              STRUCTURES DE DONNÉES                                        */
/* ======================================================================== */

/**
 * @brief Structure décrivant un paramètre audio
 */
typedef struct {
    AudioParamId_t      id;             /**< Identifiant du paramètre         */
    const char*         name;           /**< Nom affiché                      */
    const char*         unit;           /**< Unité ("%", "dB", "")            */

    /* Valeur courante */
    union {
        uint8_t     percentage;         /**< Pourcentage (0-100)              */
        bool        boolean;            /**< ON/OFF                          */
        uint8_t     mode_index;         /**< Index mode audio (0-2)          */
        uint8_t     nr_level;           /**< Niveau réduction bruit (0-3)    */
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

} AudioParam_t;

/* ======================================================================== */
/*                     STRUCTURE PRINCIPALE                                  */
/* ======================================================================== */

/**
 * @brief Structure de l'écran des réglages audio
 * 
 * Taille approximative : ~2.5 Ko
 */
typedef struct {
    /* ---- Héritage de ScreenBase ---- */
    ScreenBase_t base;                          /**< Classe de base           */

    /* ---- État actuel ---- */
    AudioScreenState_t state;                   /**< État machine d'états     */
    int16_t selected_row;                       /**< Ligne sélectionnée       */

    /* ---- Paramètres audio ---- */
    AudioParam_t params[AUDIO_PARAM_COUNT];     /**< Tableau des paramètres   */

    /* ---- Widgets UI ---- */
    UIStatusBar_t   status_bar;                 /**< Barre de statut          */
    UILabel_t*      title_label;                /**< Titre "Audio"            */
    UIButton_t*     back_button;                /**< Bouton retour            */
    UIButton_t*     apply_button;               /**< Bouton Appliquer         */
    UIButton_t*     defaults_button;            /**< Bouton Valeurs défaut    */

    /* Widgets pour chaque paramètre */
    UILabel_t*      param_labels[AUDIO_PARAM_COUNT];    /**< Noms paramètres  */
    UILabel_t*      value_labels[AUDIO_PARAM_COUNT];    /**< Valeurs          */

    /* Sliders */
    UISlider_t*     mic_volume_slider;          /**< Slider volume micro      */
    UISlider_t*     speaker_volume_slider;      /**< Slider volume HP         */
    UISlider_t*     ringtone_volume_slider;     /**< Slider volume sonnerie   */
    UISlider_t*     mic_gain_slider;            /**< Slider gain micro        */

    /* VU-mètre */
    UIVuMeter_t*    mic_vu_meter;               /**< VU-mètre niveau micro    */

    /* Switches */
    UISwitch_t*     silent_mode_switch;         /**< Switch mode silence      */
    UISwitch_t*     vibrator_switch;            /**< Switch vibreur           */

    /* Radio groups */
    UIRadioGroup_t* audio_mode_radio;           /**< Radio mode audio         */
    UIRadioGroup_t* noise_reduction_radio;      /**< Radio réduction bruit    */

    /* Boutons d'action */
    UIButton_t*     test_speaker_button;        /**< Bouton test HP           */
    UIButton_t*     test_ringtone_button;       /**< Bouton test sonnerie     */
    UIButton_t*     test_vibrator_button;       /**< Bouton test vibreur      */

    /* Label de statut */
    UILabel_t*      status_label;               /**< Message statut           */

    /* ---- Services ---- */
    SettingsService_t*  settings_service;       /**< Service persistance      */
    AudioManager_t*     audio_manager;          /**< Gestionnaire audio       */

    /* ---- Dialogue de confirmation ---- */
    UIDialog_t*     confirm_dialog;             /**< Dialogue confirmation    */

    /* ---- Callbacks ---- */
    void (*on_back_pressed)(void);              /**< Callback retour          */

    /* ---- Timers ---- */
    TimerHandle_t   vu_meter_timer;             /**< Rafraîchissement VU-mètre */
    TimerHandle_t   test_timer;                 /**< Fin de test audio        */

} ScreenSettingsAudio_t;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/**
 * @brief Initialise l'écran des réglages audio
 * 
 * Crée tous les widgets, initialise les paramètres depuis
 * le service persistant, configure les callbacks.
 * 
 * @param screen            Structure à initialiser
 * @param settings_service  Service de paramètres persistants
 * @return                  true si succès
 */
bool ScreenSettingsAudio_Init(ScreenSettingsAudio_t* screen,
                              SettingsService_t* settings_service);

/**
 * @brief Affiche l'écran audio
 * @param screen    Écran à afficher
 */
void ScreenSettingsAudio_Show(ScreenSettingsAudio_t* screen);

/**
 * @brief Masque l'écran audio
 * @param screen    Écran à masquer
 */
void ScreenSettingsAudio_Hide(ScreenSettingsAudio_t* screen);

/**
 * @brief Mise à jour périodique
 * @param screen    Écran à mettre à jour
 */
void ScreenSettingsAudio_Update(ScreenSettingsAudio_t* screen);

/**
 * @brief Gère les événements tactiles
 * @param screen    Écran
 * @param event     Événement tactile
 * @return          true si consommé
 */
bool ScreenSettingsAudio_HandleTouch(ScreenSettingsAudio_t* screen,
                                     const TouchEvent_t* event);

/**
 * @brief Gère les touches physiques
 * @param screen    Écran
 * @param key       Code touche
 * @return          true si consommé
 */
bool ScreenSettingsAudio_HandleKey(ScreenSettingsAudio_t* screen,
                                   KeyCode_t key);

/**
 * @brief Applique tous les paramètres modifiés
 * @param screen    Écran
 * @return          Nombre de paramètres appliqués
 */
uint8_t ScreenSettingsAudio_ApplyParams(ScreenSettingsAudio_t* screen);

/**
 * @brief Restaure les valeurs par défaut
 * @param screen    Écran
 */
void ScreenSettingsAudio_RestoreDefaults(ScreenSettingsAudio_t* screen);

/**
 * @brief Joue un son de test dans le haut-parleur
 * 
 * Génère une tonalité sinusoïdale de 1 kHz pendant 1 seconde
 * au volume actuellement réglé.
 * 
 * @param screen    Écran
 */
void ScreenSettingsAudio_TestSpeaker(ScreenSettingsAudio_t* screen);

/**
 * @brief Joue la sonnerie sélectionnée
 * @param screen    Écran
 */
void ScreenSettingsAudio_TestRingtone(ScreenSettingsAudio_t* screen);

/**
 * @brief Active le vibreur pour test
 * @param screen    Écran
 */
void ScreenSettingsAudio_TestVibrator(ScreenSettingsAudio_t* screen);

/**
 * @brief Active/désactive le VU-mètre du microphone
 * 
 * Le VU-mètre affiche en temps réel le niveau audio
 * capté par le microphone. Utile pour régler le gain.
 * 
 * @param screen    Écran
 * @param active    true pour activer
 */
void ScreenSettingsAudio_SetVuMeterActive(ScreenSettingsAudio_t* screen,
                                          bool active);

/**
 * @brief Définit le volume du microphone
 * @param screen    Écran
 * @param volume    Volume en pourcentage (0-100)
 */
void ScreenSettingsAudio_SetMicVolume(ScreenSettingsAudio_t* screen,
                                      uint8_t volume);

/**
 * @brief Définit le volume du haut-parleur
 * @param screen    Écran
 * @param volume    Volume en pourcentage (0-100)
 */
void ScreenSettingsAudio_SetSpeakerVolume(ScreenSettingsAudio_t* screen,
                                          uint8_t volume);

/**
 * @brief Définit le volume de la sonnerie
 * @param screen    Écran
 * @param volume    Volume en pourcentage (0-100)
 */
void ScreenSettingsAudio_SetRingtoneVolume(ScreenSettingsAudio_t* screen,
                                           uint8_t volume);

/**
 * @brief Active/désactive le mode silence
 * @param screen    Écran
 * @param silent    true = silence activé
 */
void ScreenSettingsAudio_SetSilentMode(ScreenSettingsAudio_t* screen,
                                       bool silent);

/**
 * @brief Active/désactive le vibreur
 * @param screen    Écran
 * @param enabled   true = vibreur activé
 */
void ScreenSettingsAudio_SetVibrator(ScreenSettingsAudio_t* screen,
                                     bool enabled);

/**
 * @brief Définit le mode de sortie audio
 * @param screen    Écran
 * @param mode      Mode (HP, Écouteur, Casque)
 */
void ScreenSettingsAudio_SetAudioMode(ScreenSettingsAudio_t* screen,
                                      AudioOutputMode_t mode);

/**
 * @brief Définit le niveau de réduction de bruit
 * @param screen    Écran
 * @param level     Niveau (Off, Low, Medium, High)
 */
void ScreenSettingsAudio_SetNoiseReduction(ScreenSettingsAudio_t* screen,
                                           NoiseReductionLevel_t level);

/**
 * @brief Définit le gain du microphone
 * @param screen    Écran
 * @param gain      Gain en pourcentage (0-100)
 */
void ScreenSettingsAudio_SetMicGain(ScreenSettingsAudio_t* screen,
                                    uint8_t gain);

/**
 * @brief Libère les ressources
 * @param screen    Écran
 */
void ScreenSettingsAudio_Deinit(ScreenSettingsAudio_t* screen);

/* ======================================================================== */
/*              VALEURS PAR DÉFAUT                                          */
/* ======================================================================== */

/*
 * | Paramètre              | Défaut | Plage    |
 * |------------------------|--------|----------|
 * | Volume micro           | 80%    | 0-100%   |
 * | Volume haut-parleur    | 75%    | 0-100%   |
 * | Volume sonnerie        | 85%    | 0-100%   |
 * | Mode silence           | OFF    | ON/OFF   |
 * | Vibreur                | ON     | ON/OFF   |
 * | Mode audio             | HP     | HP/Éc/Csq|
 * | Réduction bruit        | OFF    | Off-Élevé|
 * | Gain micro             | 60%    | 0-100%   |
 */

/* ======================================================================== */
/*              DÉPENDANCES MATÉRIELLES                                      */
/* ======================================================================== */

/*
 * Connexions audio STM32F429 :
 * 
 * ENTRÉE MICRO :
 *   MAX9814 OUT  →  PA4 (ADC1_IN4)   : Signal audio analogique
 *   MAX9814 VDD  →  3.3V             : Alimentation
 *   MAX9814 GND  →  GND              : Masse
 *   MAX9814 GAIN →  PB0 (DAC OUT2)   : Contrôle gain (optionnel)
 * 
 * SORTIE HAUT-PARLEUR :
 *   PA5 (DAC1_OUT1) → PAM8403 IN_L   : Signal audio gauche
 *   PAM8403 OUT_L   → Haut-parleur 3W
 *   PAM8403 VDD     → 5V (batterie)
 *   PAM8403 GND     → GND
 * 
 * VIBREUR :
 *   PB1 (TIM3_CH4)  → Moteur vibrant  : PWM pour vibreur
 * 
 * ⚠️ Toutes les broches sont configurables dans config.h
 */

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_SETTINGS_AUDIO_H */

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */