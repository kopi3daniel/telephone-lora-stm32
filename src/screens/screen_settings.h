/**
 * @file    screen_settings.h
 * @brief   Écran des paramètres - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Cet écran centralise TOUS les réglages du téléphone LoRa.
 * Il est organisé en catégories accessibles via une liste :
 * 
 *   📡 Réseau      → Fréquence, puissance, SF, bande passante LoRa
 *   🔊 Audio       → Volume micro, volume HP, mode silence
 *   🖥️ Affichage   → Luminosité, timeout écran, rotation
 *   🔔 Sonneries   → Choix mélodie, vibreur, volume sonnerie
 *   🔒 Sécurité    → Code PIN, verrouillage
 *   💾 Système     → Stockage, info version, reset usine
 *   👤 Profil      → Nom appareil, indicatif, ID réseau
 * 
 * Chaque catégorie ouvre un sous-écran spécialisé :
 *   - screen_settings_network.cpp  (réglages LoRa)
 *   - screen_settings_audio.cpp    (volumes et sons)
 *   - screen_settings_display.cpp  (luminosité, timeout)
 * 
 * Architecture STM32F429 utilisée :
 *   - LTDC couche 1 : Rendu de la liste des catégories
 *   - DMA2D        : Accélération des icônes et transitions
 *   - Flash/SDRAM  : Persistance des réglages (settings_nvram)
 * 
 * Les modifications sont sauvegardées en temps réel dans la
 * flash émulée EEPROM via le service SettingsService.
 */

#ifndef SCREEN_SETTINGS_H
#define SCREEN_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "screen_base.h"                    // Classe de base écran
#include "../ui/ui_core.h"                  // Noyau UI
#include "../ui/ui_theme.h"                 // Thème couleurs
#include "../ui/ui_widgets.h"               // Widgets communs
#include "../ui/ui_list.h"                  // Liste défilante
#include "../ui/ui_label.h"                 // Texte
#include "../ui/ui_button.h"                // Boutons
#include "../ui/ui_slider.h"                // Sliders (volume, luminosité)
#include "../ui/ui_switch.h"                // Interrupteurs ON/OFF
#include "../ui/ui_dialog.h"                // Boîtes de dialogue
#include "../ui/ui_statusbar.h"             // Barre de statut
#include "../ui/ui_navigation.h"            // Navigation
#include "../ui/ui_icons.h"                 // Icônes 📡🔊🖥️🔔🔒💾👤
#include "../ui/ui_animations.h"            // Transitions entre écrans
#include "../services/settings_service.h"   // Service de paramètres
#include "../app/app_events.h"              // Événements
#include "../drivers/power/backlight_control.h"  // Contrôle luminosité
#include "../utils/timer_utils.h"           // Timers

/* ======================================================================== */
/*                     CONSTANTES SYMBOLIQUES                                */
/* ======================================================================== */

/**
 * @brief Nombre de catégories de paramètres
 * 
 * Chaque catégorie est une entrée dans la liste principale.
 * L'ordre définit l'affichage de haut en bas.
 */
#define SETTINGS_CATEGORY_COUNT             7

/**
 * @brief Identifiants uniques pour chaque catégorie
 * 
 * Utilisés pour la navigation : quand l'utilisateur tape sur
 * une catégorie, l'ID détermine quel sous-écran ouvrir.
 * 
 * ⚠️ Ne pas changer les valeurs une fois le firmware déployé,
 *    elles peuvent être stockées dans les logs de navigation.
 */
typedef enum {
    SETTINGS_CAT_NETWORK    = 0,    /**< 📡 Réseau LoRa (fréquence, SF, etc.)    */
    SETTINGS_CAT_AUDIO      = 1,    /**< 🔊 Audio (volumes, mode silence)        */
    SETTINGS_CAT_DISPLAY    = 2,    /**< 🖥️ Affichage (luminosité, timeout)       */
    SETTINGS_CAT_RINGTONES  = 3,    /**< 🔔 Sonneries (mélodies, vibreur)         */
    SETTINGS_CAT_SECURITY   = 4,    /**< 🔒 Sécurité (PIN, verrouillage)          */
    SETTINGS_CAT_SYSTEM     = 5,    /**< 💾 Système (stockage, version, reset)    */
    SETTINGS_CAT_PROFILE    = 6,    /**< 👤 Profil (nom, indicatif, ID)           */
} SettingsCategory_t;

/**
 * @brief États possibles de l'écran paramètres
 * 
 * Machine d'états simplifiée pour gérer les différentes
 * situations d'affichage et d'interaction.
 */
typedef enum {
    SETTINGS_STATE_MAIN_LIST,           /**< Affichage de la liste principale    */
    SETTINGS_STATE_SCROLLING,           /**< Défilement en cours                 */
    SETTINGS_STATE_SUBSCREEN_OPEN,      /**< Un sous-écran est ouvert            */
    SETTINGS_STATE_RESET_CONFIRM,       /**< Confirmation reset usine            */
    SETTINGS_STATE_PIN_CHANGE,          /**< Changement de code PIN              */
} SettingsScreenState_t;

/* ======================================================================== */
/*                    STRUCTURE DE CATÉGORIE                                 */
/* ======================================================================== */

/**
 * @brief Définition d'une catégorie de paramètres
 * 
 * Chaque catégorie est affichée comme une ligne dans la liste
 * avec une icône, un titre et une valeur courante.
 * 
 * Exemple d'affichage :
 *   [📡] Réseau LoRa .................... 868 MHz
 *   [🔊] Audio .......................... Vol: 75%
 *   [🖥️] Affichage ...................... Luminosité: 60%
 */
typedef struct {
    SettingsCategory_t  id;             /**< Identifiant unique de la catégorie  */
    const char*         title;          /**< Nom affiché ("Réseau LoRa")         */
    const char*         subtitle;       /**< Description courte                  */
    UIIcon_t            icon;           /**< Icône bitmap associée               */
    uint16_t            icon_color;     /**< Couleur de l'icône                 */
    char                current_value[32];  /**< Valeur actuelle formatée        */
    
    /**
     * @brief Callback appelé quand l'utilisateur sélectionne cette catégorie
     * 
     * @param context   Pointeur vers la structure ScreenSettings_t
     * @param category  La catégorie sélectionnée
     */
    void (*on_selected)(void* context, SettingsCategory_t category);
} SettingsCategory_t;

/* ======================================================================== */
/*                     STRUCTURE PRINCIPALE                                  */
/* ======================================================================== */

/**
 * @brief Structure de l'écran des paramètres
 * 
 * Cette structure contient TOUT l'état de l'écran paramètres.
 * Elle est allouée statiquement dans la section .bss (RAM).
 * 
 * Taille approximative : ~2 Ko (vérifier avec sizeof).
 * 
 * Relations clés :
 *   - settings_service : lit/écrit les paramètres persistants
 *   - backlight_control : ajuste la luminosité en temps réel
 *   - sub_screens[] : pointeurs vers les écrans spécialisés
 */
typedef struct {
    /* ---- Héritage de ScreenBase ---- */
    ScreenBase_t base;                  /**< Classe de base (doit être en premier) */

    /* ---- État actuel ---- */
    SettingsScreenState_t state;        /**< État de la machine d'états           */
    int16_t selected_index;            /**< Index sélectionné (-1 si aucun)       */
    int16_t scroll_offset;             /**< Décalage de défilement               */

    /* ---- Catégories ---- */
    SettingsCategory_t categories[SETTINGS_CATEGORY_COUNT];  /**< Tableau des catégories */
    uint8_t category_count;            /**< Nombre réel de catégories affichées   */

    /* ---- Widgets UI principaux ---- */
    UIList_t*       list_widget;       /**< Widget liste des catégories           */
    UILabel_t*      title_label;       /**< Titre "Paramètres"                   */
    UIButton_t*     back_button;       /**< Bouton retour                        */
    UIStatusBar_t   status_bar;        /**< Barre de statut (heure, batterie)    */

    /* ---- Dialogue de confirmation ---- */
    UIDialog_t*     confirm_dialog;    /**< Pour "Reset usine" et actions critiques */
    UISwitch_t*     demo_switch;       /**< Exemple de switch ON/OFF dans la liste */

    /* ---- Services ---- */
    SettingsService_t*  settings_service;  /**< Service de persistance           */
    BacklightControl_t* backlight;         /**< Contrôleur luminosité            */

    /* ---- Sous-écrans spécialisés (ouverts à la demande) ---- */
    struct ScreenSettingsNetwork_t* network_screen;   /**< Réglages réseau       */
    struct ScreenSettingsAudio_t*   audio_screen;     /**< Réglages audio        */
    struct ScreenSettingsDisplay_t* display_screen;   /**< Réglages affichage    */

    /* ---- Callbacks de navigation ---- */
    void (*on_back_pressed)(void);     /**< Appelé quand l'utilisateur appuie Retour */
    void (*on_category_opened)(SettingsCategory_t category);  /**< Notifie ouverture */

    /* ---- Timers ---- */
    TimerHandle_t refresh_timer;       /**< Rafraîchit les valeurs affichées     */
    TimerHandle_t backlight_preview;   /**< Aperçu luminosité temporaire         */

} ScreenSettings_t;

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS PUBLIQUES                          */
/* ======================================================================== */

/**
 * @brief Initialise l'écran des paramètres
 * 
 * Cette fonction :
 * 1. Initialise la classe de base ScreenBase
 * 2. Crée tous les widgets UI (liste, labels, boutons)
 * 3. Construit le tableau des catégories avec leurs callbacks
 * 4. Lit les valeurs courantes depuis le SettingsService
 * 5. Configure les timers de rafraîchissement
 * 
 * @param screen            Pointeur vers la structure (allouée par l'appelant)
 * @param settings_service  Service de paramètres persistants
 * @param backlight         Contrôleur de luminosité (pour aperçu en direct)
 * @return                  true si l'initialisation a réussi
 * 
 * @note    L'appelant doit allouer la structure (statique ou dynamique)
 * @warning Le settings_service doit être initialisé AVANT cet écran
 * 
 * Exemple d'utilisation :
 * @code
 *   static ScreenSettings_t settings_screen;
 *   ScreenSettings_Init(&settings_screen, &g_settings_service, &g_backlight);
 * @endcode
 */
bool ScreenSettings_Init(ScreenSettings_t* screen,
                         SettingsService_t* settings_service,
                         BacklightControl_t* backlight);

/**
 * @brief Affiche l'écran des paramètres
 * 
 * Rend la liste complète des catégories avec :
 * - Icône colorée à gauche
 * - Titre de la catégorie
 * - Valeur actuelle à droite
 * - Barre de statut en haut
 * - Bouton Retour en bas
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * 
 * @note Appelée par le gestionnaire de navigation lors de la transition
 * @note Si un sous-écran était ouvert, il est fermé
 */
void ScreenSettings_Show(ScreenSettings_t* screen);

/**
 * @brief Masque l'écran des paramètres
 * 
 * Arrête les timers, masque les sous-écrans éventuellement ouverts.
 * 
 * @param screen    Pointeur vers la structure de l'écran
 */
void ScreenSettings_Hide(ScreenSettings_t* screen);

/**
 * @brief Met à jour périodiquement l'affichage
 * 
 * Appelée à chaque itération de la boucle principale.
 * Rafraîchit les valeurs affichées (ex: changement de volume
 * par un bouton physique).
 * 
 * @param screen    Pointeur vers la structure de l'écran
 */
void ScreenSettings_Update(ScreenSettings_t* screen);

/**
 * @brief Gère les événements tactiles
 * 
 * Distribue les événements aux widgets appropriés :
 * - Tap sur une catégorie → ouvre le sous-écran
 * - Swipe → défilement de la liste
 * - Tap sur Retour → retour à l'écran précédent
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @param event     Événement tactile (coordonnées, type)
 * @return          true si l'événement a été consommé
 */
bool ScreenSettings_HandleTouch(ScreenSettings_t* screen,
                                const TouchEvent_t* event);

/**
 * @brief Gère les touches du clavier physique
 * 
 * Navigation :
 * - HAUT/BAS : déplacer la sélection
 * - OK : ouvrir la catégorie sélectionnée
 * - RETOUR : revenir en arrière
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @param key       Code de la touche pressée
 * @return          true si l'événement a été consommé
 */
bool ScreenSettings_HandleKey(ScreenSettings_t* screen,
                              KeyCode_t key);

/**
 * @brief Rafraîchit toutes les valeurs affichées
 * 
 * Relit toutes les valeurs depuis le SettingsService et
 * met à jour les chaînes current_value de chaque catégorie.
 * 
 * @param screen    Pointeur vers la structure de l'écran
 */
void ScreenSettings_RefreshValues(ScreenSettings_t* screen);

/**
 * @brief Ouvre un sous-écran de paramètres spécifique
 * 
 * Navigation interne : remplace temporairement la liste
 * principale par un écran de réglages détaillés.
 * 
 * @param screen    Pointeur vers la structure de l'écran
 * @param category  Catégorie à ouvrir
 */
void ScreenSettings_OpenCategory(ScreenSettings_t* screen,
                                 SettingsCategory_t category);

/**
 * @brief Ferme le sous-écran actif et revient à la liste
 * 
 * @param screen    Pointeur vers la structure de l'écran
 */
void ScreenSettings_CloseSubScreen(ScreenSettings_t* screen);

/**
 * @brief Ferme le dialogue de confirmation s'il est ouvert
 * 
 * @param screen    Pointeur vers la structure de l'écran
 */
void ScreenSettings_CloseDialog(ScreenSettings_t* screen);

/**
 * @brief Libère toutes les ressources allouées
 * 
 * Détruit les widgets, arrête les timers, libère les sous-écrans.
 * 
 * @param screen    Pointeur vers la structure de l'écran
 */
void ScreenSettings_Deinit(ScreenSettings_t* screen);

/* ======================================================================== */
/*           PROTOTYPES DES FONCTIONS INTERNES (PRIVÉES)                    */
/* ======================================================================== */

/*
 * Les fonctions suivantes sont déclarées ici uniquement pour
 * la documentation. Elles sont définies comme static dans le .cpp
 * et ne font PAS partie de l'API publique.
 * 
 * NE PAS les appeler depuis d'autres fichiers !
 */

#if 0  /* Documentation uniquement - NE PAS DÉCOMMENTER */

/**
 * @brief Construit le tableau des catégories avec leurs propriétés
 * @private
 */
static void build_categories(ScreenSettings_t* screen);

/**
 * @brief Met à jour la valeur affichée pour une catégorie
 * @private
 */
static void update_category_value(ScreenSettings_t* screen,
                                  SettingsCategory_t category);

/**
 * @brief Callback quand une catégorie est sélectionnée dans la liste
 * @private
 */
static void on_category_selected(void* context, int16_t index);

/**
 * @brief Callback du bouton Retour
 * @private
 */
static void on_back_clicked(void* context);

/**
 * @brief Callback du dialogue de confirmation (reset usine)
 * @private
 */
static void on_reset_confirmed(void* context, bool confirmed);

/**
 * @brief Dessine une ligne de catégorie dans la liste
 * @private
 */
static void draw_category_row(void* context, 
                              int16_t index, 
                              int16_t y_position);

/**
 * @brief Callback du timer de rafraîchissement
 * @private
 */
static void refresh_timer_callback(TimerHandle_t timer);

#endif /* Documentation */

/* ======================================================================== */
/*              EXEMPLE D'UTILISATION (DANS phone_app.cpp)                   */
/* ======================================================================== */

/*
 * // === INITIALISATION (une seule fois au démarrage) ===
 * 
 * #include "screens/screen_settings.h"
 * 
 * static ScreenSettings_t g_settings_screen;
 * 
 * void App_InitScreens(void) {
 *     ScreenSettings_Init(&g_settings_screen,
 *                         &g_settings_service,
 *                         &g_backlight_control);
 *     
 *     // Configurer le callback de retour
 *     g_settings_screen.on_back_pressed = App_GoToHomeScreen;
 * }
 * 
 * // === AFFICHAGE (quand l'utilisateur ouvre les paramètres) ===
 * 
 * void App_ShowSettings(void) {
 *     ScreenSettings_Show(&g_settings_screen);
 * }
 * 
 * // === GESTION ÉVÉNEMENTS (dans la boucle principale) ===
 * 
 * void App_HandleTouchEvent(const TouchEvent_t* event) {
 *     if (current_screen == SCREEN_SETTINGS) {
 *         ScreenSettings_HandleTouch(&g_settings_screen, event);
 *     }
 * }
 * 
 * void App_HandleKeyEvent(KeyCode_t key) {
 *     if (current_screen == SCREEN_SETTINGS) {
 *         ScreenSettings_HandleKey(&g_settings_screen, key);
 *     }
 * }
 */

/* ======================================================================== */
/*              DIAGRAMME DE NAVIGATION                                      */
/* ======================================================================== */

/*
 * Écran Principal (Home)
 *        │
 *        │ Appui sur "Paramètres"
 *        ▼
 * ┌─────────────────────────────────┐
 * │     PARAMÈTRES (liste)          │
 * │                                 │
 * │  📡 Réseau LoRa ........ 868MHz │──→ screen_settings_network
 * │  🔊 Audio ............. Vol 75% │──→ screen_settings_audio
 * │  🖥️ Affichage ......... Lum 60% │──→ screen_settings_display
 * │  🔔 Sonneries ......... Mélodie1│──→ (intégré ou sous-écran)
 * │  🔒 Sécurité .......... PIN: ON │──→ Dialog PIN
 * │  💾 Système ........... v1.2.0  │──→ Dialog Reset
 * │  👤 Profil ............ MonTél  │──→ (intégré)
 * │                                 │
 * │  [Retour]                       │
 * └─────────────────────────────────┘
 *        │
 *        │ Appui sur une catégorie
 *        ▼
 * ┌─────────────────────────────────┐
 * │     SOUS-ÉCRAN SPÉCIALISÉ       │
 * │  (ex: Réglages Réseau)          │
 * │                                 │
 * │  Fréquence:  [868 MHz]    ▲    │
 * │  Puissance:  [20 dBm]  ──●──  │
 * │  SF:         [SF7]       ▼    │
 * │  Bande:      [250 kHz]        │
 * │                                 │
 * │  [Retour]    [Appliquer]       │
 * └─────────────────────────────────┘
 */

/* ======================================================================== */
/*              DÉPENDANCES MATÉRIELLES (POUR RÉFÉRENCE)                     */
/* ======================================================================== */

/*
 * Broches STM32F429 utilisées par les sous-écrans de paramètres :
 * 
 * ÉCRAN (LTDC) :
 *   - Voir ltdc_config.h pour le mapping complet
 *   - R3-R7, G2-G7, B3-B7, CLK, HSYNC, VSYNC, DE
 * 
 * RÉTROÉCLAIRAGE (PWM) :
 *   - TIM1_CH1 → Broche PE9 (Backlight LED)
 *   - Fréquence PWM : 25 kHz
 *   - Résolution : 12 bits (0-4095)
 * 
 * TOUCH (SPI5) :
 *   - SPI5_SCK  → PF7
 *   - SPI5_MISO → PF8
 *   - SPI5_MOSI → PF9
 *   - TOUCH_CS  → PF10
 *   - TOUCH_IRQ → PF11
 * 
 * STOCKAGE PERSISTANT :
 *   - Flash interne secteur 11 (dernier secteur 128 Ko)
 *   - Émulation EEPROM : 4 Ko disponibles pour les paramètres
 */

/* ======================================================================== */
/*              VALEURS PAR DÉFAUT (DÉFINIES DANS settings_service.h)        */
/* ======================================================================== */

/*
 * Les valeurs par défaut suivantes sont appliquées au premier
 * démarrage (quand la flash est vierge) :
 * 
 * | Paramètre              | Valeur défaut | Plage            |
 * |------------------------|---------------|------------------|
 * | LoRa Fréquence         | 868.0 MHz     | 863-870 MHz      |
 * | LoRa Puissance         | 20 dBm        | 2-20 dBm         |
 * | LoRa SF                | 7             | 6-12             |
 * | LoRa Bande passante    | 250 kHz       | 125/250/500 kHz  |
 * | Volume micro           | 80%           | 0-100%           |
 * | Volume haut-parleur    | 75%           | 0-100%           |
 * | Mode silence           | OFF           | ON/OFF           |
 * | Luminosité écran       | 70%           | 10-100%          |
 * | Timeout écran          | 30 secondes   | 10-300 sec       |
 * | Sonnerie               | Mélodie 1     | 1-10             |
 * | Vibreur                | ON            | ON/OFF           |
 * | Code PIN               | 0000          | 4-8 chiffres     |
 * | Verrouillage auto      | 60 secondes   | 15-300 sec       |
 * | Nom appareil           | "LoRa Phone"  | 16 caractères    |
 * | Indicatif              | +33           | 1-4 chiffres     |
 * | ID Réseau              | 1             | 0-255            |
 */

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_SETTINGS_H */

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */