/**
 * @file    screen_settings_network.h
 * @brief   Écran des réglages réseau LoRa - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Cet écran permet de configurer TOUS les paramètres de la radio LoRa.
 * Il est accessible depuis l'écran Paramètres → Réseau LoRa.
 * 
 * ORGANISATION DE L'ÉCRAN :
 * ┌─────────────────────────────────────────────────────────┐
 * │  ← Réseau LoRa                                12:45  ███ │
 * │─────────────────────────────────────────────────────────│
 * │                                                         │
 * │  Fréquence                                              │
 * │  [868.000] MHz                              ▲          │
 * │  ─────────────────────────────────────────●──────────  │
 * │                                            ▼           │
 * │                                                         │
 * │  Puissance d'émission                                   │
 * │  [20 dBm]                                              │
 * │  ───────────────────────────────────────●────────────  │
 * │                                                         │
 * │  Spreading Factor (SF)                                  │
 * │  ◉ SF7  ○ SF8  ○ SF9  ○ SF10  ○ SF11  ○ SF12         │
 * │                                                         │
 * │  Bande passante                                         │
 * │  ◉ 125 kHz  ○ 250 kHz  ○ 500 kHz                       │
 * │                                                         │
 * │  Coding Rate                                            │
 * │  ◉ 4/5  ○ 4/6  ○ 4/7  ○ 4/8                           │
 * │                                                         │
 * │  Synchronisation                                        │
 * │  [✓] Enable Sync Word  0x[12]                          │
 * │                                                         │
 * │─────────────────────────────────────────────────────────│
 * │  [Appliquer]                    [Valeurs par défaut]    │
 * └─────────────────────────────────────────────────────────┘
 * 
 * PARTICULARITÉS TECHNIQUES :
 * 
 * 1. Les modifications sont appliquées EN TEMPS RÉEL sur le module SX1278.
 *    Chaque changement de slider ou de bouton radio envoie immédiatement
 *    la commande SPI correspondante au registre du RA-02.
 * 
 * 2. Un indicateur visuel montre si la modification a été prise en compte :
 *    - ✓ Vert  : Appliqué avec succès (ACK du module)
 *    - ⏳ Jaune : En cours d'application
 *    - ✗ Rouge : Échec (module non répondant)
 * 
 * 3. Les valeurs sont sauvegardées en flash (SettingsService) pour
 *    persister après un redémarrage.
 * 
 * 4. La fréquence est ajustable par pas de 0.1 MHz via les boutons +/-,
 *    ou par saisie directe (clavier virtuel à venir).
 * 
 * 5. La puissance est limitée à 20 dBm (100 mW) pour respecter les
 *    réglementations européennes (868 MHz ISM band).
 * 
 * ARCHITECTURE STM32F429 UTILISÉE :
 * - SPI1 (ou SPI partagé) : Communication avec le SX1278
 * - DMA2D : Rendu accéléré des sliders et icônes
 * - LTDC couche 1 : Affichage principal
 * - Timers : Debounce des boutons +/-
 * 
 * REGISTRES SX1278 CONCERNÉS :
 * - RegFrf (0x06-0x08)    : Fréquence porteuse (24 bits)
 * - RegPaConfig (0x09)    : Puissance d'émission + PA boost
 * - RegModemConfig1 (0x1D): Bande passante + Coding Rate
 * - RegModemConfig2 (0x1E): Spreading Factor + CRC
 * - RegSyncConfig (0x27)  : Sync Word
 * 
 * PLAGES DE VALEURS (868 MHz ISM Band) :
 * | Paramètre        | Min     | Max     | Défaut  | Pas      |
 * |------------------|---------|---------|---------|----------|
 * | Fréquence        | 863.0   | 870.0   | 868.0   | 0.1 MHz  |
 * | Puissance        | 2       | 20      | 20      | 1 dBm    |
 * | Spreading Factor | 6       | 12      | 7       | 1        |
 * | Bande passante   | 125 kHz | 500 kHz | 250 kHz | -        |
 * | Coding Rate      | 4/5     | 4/8     | 4/5     | -        |
 * | Sync Word        | 0x00    | 0xFF    | 0x12    | 0x01     |
 * 
 * ⚠️ ATTENTION :
 * - SF6 nécessite un Sync Word spécifique (0x1A) et une configuration
 *   particulière (implicit header mode). Réservé aux usages avancés.
 * - Les fréquences hors bande ISM peuvent être illégales.
 * - Une puissance excessive peut endommager le PA du SX1278.
 */

#ifndef SCREEN_SETTINGS_NETWORK_H
#define SCREEN_SETTINGS_NETWORK_H

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
#include "../ui/ui_slider.h"                    // Sliders (fréquence, puissance)
#include "../ui/ui_radio_group.h"               // Groupes de boutons radio
#include "../ui/ui_switch.h"                    // Interrupteurs ON/OFF
#include "../ui/ui_numpad.h"                    // Pavé numérique (saisie fréquence)
#include "../ui/ui_statusbar.h"                 // Barre de statut
#include "../ui/ui_navigation.h"                // Navigation
#include "../ui/ui_icons.h"                     // Icônes
#include "../services/settings_service.h"       // Persistance des paramètres
#include "../drivers/lora/lora_driver.h"        // Driver LoRa SX1278
#include "../drivers/lora/sx1278_defs.h"        // Définitions registres
#include "../app/app_events.h"                  // Événements
#include "../utils/timer_utils.h"               // Timers

/* ======================================================================== */
/*                     CONSTANTES SYMBOLIQUES                                */
/* ======================================================================== */

/**
 * @brief Nombre de paramètres réseau configurables
 * 
 * Chaque paramètre correspond à une ligne dans l'écran.
 * L'ordre définit l'affichage de haut en bas.
 */
#define NETWORK_PARAM_COUNT                 6

/**
 * @brief Identifiants des paramètres réseau
 * 
 * Utilisés pour :
 * - Identifier quel paramètre est modifié par l'utilisateur
 * - Indexer les callbacks de changement
 * - Sauvegarder l'historique des modifications
 */
typedef enum {
    NETWORK_PARAM_FREQUENCY = 0,        /**< Fréquence porteuse (MHz)          */
    NETWORK_PARAM_POWER,                /**< Puissance d'émission (dBm)        */
    NETWORK_PARAM_SPREADING_FACTOR,     /**< Spreading Factor (SF6-SF12)       */
    NETWORK_PARAM_BANDWIDTH,            /**< Bande passante (kHz)              */
    NETWORK_PARAM_CODING_RATE,          /**< Coding Rate (4/5 à 4/8)           */
    NETWORK_PARAM_SYNC_WORD,            /**< Mot de synchronisation            */
} NetworkParamId_t;

/**
 * @brief États de l'écran réseau
 * 
 * Machine d'états pour gérer les différentes interactions.
 */
typedef enum {
    NETWORK_STATE_IDLE,                 /**< Affichage normal, pas d'interaction */
    NETWORK_STATE_EDITING_FREQ,         /**< Édition fréquence via +/-         */
    NETWORK_STATE_EDITING_POWER,        /**< Édition puissance via slider      */
    NETWORK_STATE_EDITING_SF,           /**< Sélection SF en cours             */
    NETWORK_STATE_EDITING_BW,           /**< Sélection bande passante          */
    NETWORK_STATE_EDITING_CR,           /**< Sélection coding rate             */
    NETWORK_STATE_EDITING_SYNC,         /**< Édition sync word                 */
    NETWORK_STATE_APPLYING,             /**< Application des changements       */
    NETWORK_STATE_ERROR,                /**< Erreur de communication LoRa      */
} NetworkScreenState_t;

/**
 * @brief Statut d'application d'un paramètre
 * 
 * Indique si la modification a été prise en compte par le module.
 */
typedef enum {
    APPLY_STATUS_IDLE = 0,              /**< Pas de modification en attente    */
    APPLY_STATUS_PENDING,               /**< Modification en cours d'envoi     */
    APPLY_STATUS_SUCCESS,               /**< Modification appliquée avec succès*/
    APPLY_STATUS_FAILED,                /**< Échec de l'application            */
} ApplyStatus_t;

/* ======================================================================== */
/*              STRUCTURES DE DONNÉES                                        */
/* ======================================================================== */

/**
 * @brief Structure décrivant un paramètre réseau
 * 
 * Chaque paramètre a :
 * - Un identifiant unique
 * - Un nom affiché
 * - Une valeur courante (dans différents types)
 * - Des valeurs min/max
 * - Un statut d'application
 * - Un callback appelé quand la valeur change
 */
typedef struct {
    NetworkParamId_t    id;             /**< Identifiant du paramètre          */
    const char*         name;           /**< Nom affiché ("Fréquence")         */
    const char*         unit;           /**< Unité ("MHz", "dBm", "")          */

    /* Valeur courante (union pour les différents types) */
    union {
        float   freq_mhz;              /**< Fréquence en MHz                  */
        int8_t  power_dbm;             /**< Puissance en dBm                  */
        uint8_t sf_value;              /**< Spreading Factor (6-12)           */
        uint8_t bw_index;              /**< Index bande passante (0-2)        */
        uint8_t cr_index;              /**< Index coding rate (0-3)           */
        uint8_t sync_byte;             /**< Sync Word (0x00-0xFF)             */
    } value;

    /* Limites */
    union {
        struct { float min; float max; float step; } freq_range;
        struct { int8_t min; int8_t max; } power_range;
        struct { uint8_t min; uint8_t max; } sf_range;
    } limits;

    /* Options pour les choix discrets (SF, BW, CR) */
    const char**        options;        /**< Labels des options                */
    uint8_t             option_count;   /**< Nombre d'options                  */

    /* État */
    ApplyStatus_t       apply_status;   /**< État d'application               */
    bool                is_modified;    /**< true si modifié depuis dernier Apply */

    /* Widget associé (pour mise à jour rapide) */
    void*               widget;         /**< Pointeur générique vers le widget */

} NetworkParam_t;

/* ======================================================================== */
/*                     STRUCTURE PRINCIPALE                                  */
/* ======================================================================== */

/**
 * @brief Structure de l'écran des réglages réseau
 * 
 * Contient tous les paramètres configurables et les widgets
 * nécessaires à leur affichage et modification.
 * 
 * Taille approximative : ~3 Ko (vérifier avec sizeof)
 * 
 * Relations clés :
 *   - settings_service : persistance des valeurs
 *   - lora_driver      : application directe sur le SX1278
 *   - Écran parent     : screen_settings (retour)
 */
typedef struct {
    /* ---- Héritage de ScreenBase ---- */
    ScreenBase_t base;                      /**< Classe de base                */

    /* ---- État actuel ---- */
    NetworkScreenState_t state;             /**< État de la machine d'états    */
    NetworkParamId_t active_param;          /**< Paramètre en cours d'édition  */
    int16_t selected_row;                   /**< Ligne sélectionnée            */

    /* ---- Paramètres réseau ---- */
    NetworkParam_t params[NETWORK_PARAM_COUNT];  /**< Tableau des paramètres   */

    /* ---- Widgets UI ---- */
    UIStatusBar_t   status_bar;             /**< Barre de statut               */
    UILabel_t*      title_label;            /**< Titre "Réseau LoRa"           */
    UIButton_t*     back_button;            /**< Bouton retour                 */
    UIButton_t*     apply_button;           /**< Bouton Appliquer              */
    UIButton_t*     defaults_button;        /**< Bouton Valeurs par défaut     */

    /* Widgets pour chaque paramètre */
    UILabel_t*      param_labels[NETWORK_PARAM_COUNT];    /**< Noms paramètres */
    UILabel_t*      value_labels[NETWORK_PARAM_COUNT];    /**< Valeurs affichées */
    UISlider_t*     freq_slider;            /**< Slider fréquence              */
    UISlider_t*     power_slider;           /**< Slider puissance              */
    UIRadioGroup_t* sf_radio_group;         /**< Radio buttons SF              */
    UIRadioGroup_t* bw_radio_group;         /**< Radio buttons bande passante  */
    UIRadioGroup_t* cr_radio_group;         /**< Radio buttons coding rate     */
    UILabel_t*      sync_value_label;       /**< Label sync word               */
    UIButton_t*     sync_edit_button;       /**< Bouton édition sync word      */

    /* Widgets d'édition */
    UINumpad_t*     numpad;                 /**< Pavé numérique                */
    UIButton_t*     freq_plus_button;       /**< Bouton + fréquence            */
    UIButton_t*     freq_minus_button;      /**< Bouton - fréquence            */

    /* Indicateur de statut */
    UILabel_t*      status_label;           /**< Label statut (✓/⏳/✗)        */

    /* ---- Services ---- */
    SettingsService_t*  settings_service;   /**< Service de persistance        */
    LoRaDriver_t*       lora_driver;        /**< Driver LoRa SX1278            */

    /* ---- Dialogue de confirmation ---- */
    UIDialog_t*     confirm_dialog;         /**< Pour "Appliquer" et "Défaut"  */

    /* ---- Callbacks ---- */
    void (*on_back_pressed)(void);          /**< Callback retour               */
    void (*on_params_applied)(void);        /**< Notifie application effectuée */

    /* ---- Timers ---- */
    TimerHandle_t   apply_status_timer;     /**< Timeout statut application    */
    TimerHandle_t   debounce_timer;         /**< Anti-rebond boutons +/-       */

} ScreenSettingsNetwork_t;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/**
 * @brief Initialise l'écran des réglages réseau
 * 
 * Crée tous les widgets, lit les valeurs actuelles depuis le
 * SettingsService et le module LoRa, prépare les callbacks.
 * 
 * @param screen            Pointeur vers la structure (allouée par l'appelant)
 * @param settings_service  Service de paramètres persistants
 * @return                  true si l'initialisation a réussi
 * 
 * @note Le driver LoRa (lora_driver) est récupéré via le service global
 * @warning Le module LoRa doit être initialisé AVANT cet écran
 */
bool ScreenSettingsNetwork_Init(ScreenSettingsNetwork_t* screen,
                                SettingsService_t* settings_service);

/**
 * @brief Affiche l'écran des réglages réseau
 * 
 * Rend tous les paramètres avec leurs valeurs actuelles,
 * sliders, boutons radio et indicateurs de statut.
 * 
 * @param screen    Pointeur vers la structure de l'écran
 */
void ScreenSettingsNetwork_Show(ScreenSettingsNetwork_t* screen);

/**
 * @brief Masque l'écran réseau
 * 
 * Arrête les timers, nettoie l'état d'édition en cours.
 * 
 * @param screen    Pointeur vers la structure de l'écran
 */
void ScreenSettingsNetwork_Hide(ScreenSettingsNetwork_t* screen);

/**
 * @brief Met à jour l'affichage (périodique)
 * 
 * Rafraîchit les valeurs affichées, vérifie les timeouts
 * des statuts d'application.
 * 
 * @param screen    Pointeur vers la structure de l'écran
 */
void ScreenSettingsNetwork_Update(ScreenSettingsNetwork_t* screen);

/**
 * @brief Gère les événements tactiles
 * 
 * Distribue aux widgets appropriés :
 * - Sliders (fréquence, puissance)
 * - Boutons radio (SF, BW, CR)
 * - Boutons +/- (ajustement fin fréquence)
 * - Boutons Appliquer/Retour/Défauts
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @param event     Événement tactile
 * @return          true si l'événement a été consommé
 */
bool ScreenSettingsNetwork_HandleTouch(ScreenSettingsNetwork_t* screen,
                                       const TouchEvent_t* event);

/**
 * @brief Gère les touches du clavier physique
 * 
 * Navigation :
 * - HAUT/BAS : sélectionner un paramètre
 * - GAUCHE/DROITE : ajuster la valeur
 * - OK : valider
 * - RETOUR : revenir en arrière
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @param key       Code de la touche
 * @return          true si l'événement a été consommé
 */
bool ScreenSettingsNetwork_HandleKey(ScreenSettingsNetwork_t* screen,
                                     KeyCode_t key);

/**
 * @brief Applique tous les paramètres modifiés au module LoRa
 * 
 * Envoie les commandes SPI au SX1278 pour chaque paramètre
 * qui a été modifié (is_modified == true).
 * 
 * L'application se fait dans l'ordre recommandé par la datasheet :
 * 1. Mode Sleep (RegOpMode = 0x00)
 * 2. Fréquence (RegFrf)
 * 3. PA Config (RegPaConfig)
 * 4. Modem Config 1 (RegModemConfig1) : BW + CR
 * 5. Modem Config 2 (RegModemConfig2) : SF
 * 6. Sync Config (RegSyncConfig)
 * 7. Mode Standby (RegOpMode = 0x01)
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @return          Nombre de paramètres appliqués avec succès
 */
uint8_t ScreenSettingsNetwork_ApplyParams(ScreenSettingsNetwork_t* screen);

/**
 * @brief Applique un seul paramètre au module LoRa
 * 
 * Utile pour les modifications en temps réel (slider fréquence).
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @param param_id  Identifiant du paramètre à appliquer
 * @return          APPLY_STATUS_SUCCESS ou APPLY_STATUS_FAILED
 */
ApplyStatus_t ScreenSettingsNetwork_ApplySingleParam(ScreenSettingsNetwork_t* screen,
                                                      NetworkParamId_t param_id);

/**
 * @brief Restaure les valeurs par défaut
 * 
 * Remet tous les paramètres à leurs valeurs d'usine,
 * les applique au module LoRa, et sauvegarde en flash.
 * 
 * @param screen    Pointeur vers la structure de l'écran
 */
void ScreenSettingsNetwork_RestoreDefaults(ScreenSettingsNetwork_t* screen);

/**
 * @brief Lit les paramètres actuels depuis le module LoRa
 * 
 * Effectue une lecture SPI des registres du SX1278 pour
 * synchroniser l'affichage avec l'état réel du module.
 * Utile après un reset ou une reprogrammation externe.
 * 
 * @param screen    Pointeur vers la structure de l'écran
 */
void ScreenSettingsNetwork_ReadFromModule(ScreenSettingsNetwork_t* screen);

/**
 * @brief Définit la fréquence
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @param freq_mhz  Fréquence en MHz (863.0 - 870.0)
 */
void ScreenSettingsNetwork_SetFrequency(ScreenSettingsNetwork_t* screen,
                                        float freq_mhz);

/**
 * @brief Définit la puissance d'émission
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @param power_dbm Puissance en dBm (2 - 20)
 */
void ScreenSettingsNetwork_SetPower(ScreenSettingsNetwork_t* screen,
                                    int8_t power_dbm);

/**
 * @brief Définit le Spreading Factor
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @param sf        SF (6-12). Note: SF6 nécessite config spéciale.
 */
void ScreenSettingsNetwork_SetSpreadingFactor(ScreenSettingsNetwork_t* screen,
                                              uint8_t sf);

/**
 * @brief Définit la bande passante
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @param bw_khz    Bande passante en kHz (125, 250, 500)
 */
void ScreenSettingsNetwork_SetBandwidth(ScreenSettingsNetwork_t* screen,
                                        uint16_t bw_khz);

/**
 * @brief Définit le Coding Rate
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @param cr        Coding Rate (5=4/5, 6=4/6, 7=4/7, 8=4/8)
 */
void ScreenSettingsNetwork_SetCodingRate(ScreenSettingsNetwork_t* screen,
                                         uint8_t cr);

/**
 * @brief Définit le Sync Word
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @param sync_byte Sync Word (0x00-0xFF). Défaut: 0x12 pour LoRa public.
 */
void ScreenSettingsNetwork_SetSyncWord(ScreenSettingsNetwork_t* screen,
                                       uint8_t sync_byte);

/**
 * @brief Libère toutes les ressources de l'écran
 * 
 * @param screen    Pointeur vers la structure de l'écran
 */
void ScreenSettingsNetwork_Deinit(ScreenSettingsNetwork_t* screen);

/* ======================================================================== */
/*              FONCTIONS DE CONVERSION (UTILITAIRES)                        */
/* ======================================================================== */

/**
 * @brief Convertit un Spreading Factor en durée de symbole (ms)
 * 
 * Utile pour afficher le temps d'émission estimé.
 * Ts = 2^SF / BW
 * 
 * @param sf    Spreading Factor (6-12)
 * @param bw_hz Bande passante en Hz
 * @return      Durée d'un symbole en millisecondes
 */
float NetworkSettings_SFToSymbolTime(uint8_t sf, uint32_t bw_hz);

/**
 * @brief Convertit la configuration en débit binaire estimé (bps)
 * 
 * Rb = SF * BW / (2^SF) * CR
 * 
 * @param sf    Spreading Factor
 * @param bw_hz Bande passante en Hz
 * @param cr    Coding Rate (5=4/5, 6=4/6, 7=4/7, 8=4/8)
 * @return      Débit binaire en bits par seconde
 */
uint32_t NetworkSettings_GetBitRate(uint8_t sf, uint32_t bw_hz, uint8_t cr);

/**
 * @brief Calcule la portée théorique estimée (km)
 * 
 * Basé sur le bilan de liaison simplifié.
 * 
 * @param power_dbm     Puissance d'émission (dBm)
 * @param sf            Spreading Factor
 * @param bw_hz         Bande passante (Hz)
 * @param freq_mhz      Fréquence (MHz)
 * @return              Portée estimée en km
 */
float NetworkSettings_EstimateRange(int8_t power_dbm, uint8_t sf,
                                    uint32_t bw_hz, float freq_mhz);

/* ======================================================================== */
/*              EXEMPLE D'UTILISATION                                        */
/* ======================================================================== */

/*
 * // === OUVERTURE DEPUIS L'ÉCRAN PARAMÈTRES ===
 * 
 * static ScreenSettingsNetwork_t g_network_screen;
 * 
 * void open_network_settings(ScreenSettings_t* parent) {
 *     if (!parent->network_screen) {
 *         parent->network_screen = malloc(sizeof(ScreenSettingsNetwork_t));
 *         ScreenSettingsNetwork_Init(parent->network_screen,
 *                                    parent->settings_service);
 *     }
 *     ScreenSettingsNetwork_Show(parent->network_screen);
 * }
 * 
 * // === APPLICATION AUTOMATIQUE AU CHANGEMENT ===
 * 
 * void on_frequency_changed(ScreenSettingsNetwork_t* screen, float new_freq) {
 *     ScreenSettingsNetwork_SetFrequency(screen, new_freq);
 *     ScreenSettingsNetwork_ApplySingleParam(screen, NETWORK_PARAM_FREQUENCY);
 * }
 */

/* ======================================================================== */
/*              VALEURS PAR DÉFAUT (DÉFINIES DANS lora_driver.h)             */
/* ======================================================================== */

/*
 * Les valeurs par défaut suivantes sont appliquées si l'utilisateur
 * choisit "Restaurer les valeurs par défaut" :
 * 
 * | Paramètre        | Valeur défaut | Registre SX1278 |
 * |------------------|---------------|-----------------|
 * | Fréquence        | 868.000 MHz   | RegFrf (0x06)   |
 * | Puissance        | 20 dBm        | RegPaConfig     |
 * | Spreading Factor | 7             | RegModemConfig2 |
 * | Bande passante   | 250 kHz       | RegModemConfig1 |
 * | Coding Rate      | 4/5           | RegModemConfig1 |
 * | Sync Word        | 0x12 (Public) | RegSyncConfig   |
 * 
 * Ces valeurs offrent un bon compromis entre :
 * - Portée (SF7 + 250 kHz = ~3-5 km en urbain)
 * - Débit (environ 5.5 kbps)
 * - Latence (temps de symbole ~5 ms)
 * - Robustesse (CR 4/5 = overhead minimal)
 */

/* ======================================================================== */
/*              DÉPENDANCES MATÉRIELLES                                      */
/* ======================================================================== */

/*
 * Connexions SX1278 (RA-02) vers STM32F429 :
 * 
 * RA-02 Pin  →  STM32F429 Pin  →  Fonction
 * ─────────────────────────────────────────
 * VCC        →  3.3V            →  Alimentation
 * GND        →  GND             →  Masse
 * NSS        →  PA4 (SPI1_CS)   →  Chip Select
 * SCK        →  PA5 (SPI1_SCK)  →  Horloge SPI
 * MOSI       →  PA7 (SPI1_MOSI) →  Données sortantes
 * MISO       →  PA6 (SPI1_MISO) →  Données entrantes
 * RST        →  PB0             →  Reset
 * DIO0       →  PB1             →  Interruption (TxDone/RxDone)
 * DIO1       →  PB2             →  Interruption (RxTimeout)
 * DIO2       →  PB3             →  Interruption (FhssChange)
 * 
 * ⚠️ Toutes les broches sont configurables dans config.h
 */

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_SETTINGS_NETWORK_H */

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */