/**
 * @file    screen_call_log.h
 * @brief   Écran journal des appels - Téléphone LoRa STM32F429
 * @author  Votre Nom
 * @date    2026
 * 
 * Affiche l'historique des appels (entrants, sortants, manqués)
 * avec défilement, icônes de type d'appel et navigation.
 */

#ifndef SCREEN_CALL_LOG_H
#define SCREEN_CALL_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "screen_base.h"                   // Classe de base des écrans
#include "../ui/ui_core.h"                 // Noyau UI (états, boucle)
#include "../ui/ui_theme.h"                // Thème couleurs/polices
#include "../ui/ui_widgets.h"              // Widgets communs
#include "../ui/ui_list.h"                 // Widget liste avec défilement
#include "../ui/ui_label.h"                // Widget texte
#include "../ui/ui_icons.h"                // Icônes bitmap
#include "../ui/ui_statusbar.h"            // Barre de statut
#include "../ui/ui_navigation.h"           // Navigation entre écrans
#include "../services/call_log_service.h"  // Service historique appels
#include "../services/contact_service.h"   // Service contacts (pour noms)
#include "../app/app_events.h"             // Événements application
#include "../utils/string_utils.h"         // Formatage chaînes
#include "../utils/timer_utils.h"          // Timers (rafraîchissement)

/* ======================================================================== */
/*                            DÉFINITIONS                                    */
/* ======================================================================== */

/** Nombre maximum d'entrées affichables simultanément */
#define CALL_LOG_VISIBLE_ENTRIES        8

/** Hauteur d'une ligne dans la liste */
#define CALL_LOG_ROW_HEIGHT             52

/** Nombre total d'entrées dans l'historique */
#define CALL_LOG_MAX_ENTRIES            CALL_LOG_MAX_STORED

/** Types de filtres pour l'affichage */
typedef enum {
    CALL_LOG_FILTER_ALL = 0,             /**< Tous les appels       */
    CALL_LOG_FILTER_MISSED,              /**< Appels manqués        */
    CALL_LOG_FILTER_INCOMING,            /**< Appels entrants       */
    CALL_LOG_FILTER_OUTGOING,            /**< Appels sortants       */
    CALL_LOG_FILTER_COUNT                /**< Nombre de filtres     */
} CallLogFilter_t;

/** États de l'écran journal */
typedef enum {
    CALL_LOG_STATE_IDLE = 0,             /**< Affichage normal      */
    CALL_LOG_STATE_SCROLLING,            /**< Défilement en cours   */
    CALL_LOG_STATE_DETAIL,               /**< Détail d'une entrée   */
    CALL_LOG_STATE_DELETE_CONFIRM,       /**< Confirmation suppr.   */
    CALL_LOG_STATE_EMPTY                 /**< Journal vide          */
} CallLogScreenState_t;

/** Structure d'affichage d'une entrée */
typedef struct {
    uint32_t    entry_id;                /**< ID unique entrée       */
    char        display_name[32];        /**< Nom affiché (contact)  */
    char        display_number[20];      /**< Numéro formaté         */
    char        time_str[8];             /**< Heure formatée         */
    char        date_str[12];            /**< Date formatée          */
    CallType_t  call_type;               /**< Type d'appel           */
    uint16_t    duration_sec;            /**< Durée en secondes      */
    uint8_t     icon_index;              /**< Index icône type       */
    bool        is_highlighted;          /**< Ligne sélectionnée     */
} CallLogDisplayEntry_t;

/* ======================================================================== */
/*                        STRUCTURE PRINCIPALE                               */
/* ======================================================================== */

/**
 * @brief Structure de l'écran journal des appels
 * 
 * Gère l'affichage complet de l'historique avec :
 * - Filtrage par type d'appel
 * - Défilement fluide via DMA2D
 * - Affichage des détails au tap
 * - Suppression d'entrées
 * - Indicateurs visuels (manqué = rouge, etc.)
 */
typedef struct {
    /* ---- Classe de base (héritage) ---- */
    ScreenBase_t base;                   /**< Écran de base          */

    /* ---- État actuel ---- */
    CallLogScreenState_t state;          /**< État de l'écran        */
    CallLogFilter_t active_filter;       /**< Filtre actif           */

    /* ---- Données ---- */
    CallLogDisplayEntry_t entries[CALL_LOG_MAX_ENTRIES];  /**< Entrées formatées */
    uint16_t total_entries;              /**< Nombre total entrées   */
    int16_t selected_index;              /**< Index sélectionné      */
    int16_t scroll_offset;               /**< Offset défilement      */

    /* ---- Widgets UI ---- */
    UIList_t* list_widget;               /**< Widget liste           */
    UILabel_t* title_label;              /**< Titre "Journal"        */
    UILabel_t* filter_label;             /**< Label filtre actif     */
    UILabel_t* empty_label;              /**< "Aucun appel"          */
    UIButton_t* filter_buttons[4];       /**< Boutons filtre         */
    UIButton_t* back_button;             /**< Bouton retour          */
    UIButton_t* clear_button;            /**< Bouton effacer tout    */
    UIStatusBar_t status_bar;            /**< Barre de statut        */

    /* ---- Gestion détail ---- */
    bool detail_visible;                 /**< Panneau détail ouvert  */
    CallLogDisplayEntry_t detail_entry;  /**< Entrée en détail       */
    UIDialog_t* confirm_dialog;          /**< Dialogue confirmation  */

    /* ---- Services ---- */
    CallLogService_t* call_log_service;  /**< Service historique     */
    ContactService_t* contact_service;   /**< Service contacts       */

    /* ---- Timers ---- */
    TimerHandle_t refresh_timer;         /**< Timer rafraîchissement */

    /* ---- Callbacks ---- */
    void (*on_back_pressed)(void);       /**< Callback retour        */
    void (*on_entry_selected)(uint32_t entry_id, CallType_t type, 
                               const char* number, const char* name);

} ScreenCallLog_t;

/* ======================================================================== */
/*                         FONCTIONS PUBLIQUES                               */
/* ======================================================================== */

/**
 * @brief Initialise l'écran journal des appels
 * 
 * Alloue les widgets, charge les entrées depuis le service,
 * configure les callbacks et l'affichage initial.
 * 
 * @param screen        Pointeur vers la structure de l'écran
 * @param call_log      Service historique des appels
 * @param contacts      Service contacts (pour résoudre les noms)
 * @return              true si initialisation réussie
 */
bool ScreenCallLog_Init(ScreenCallLog_t* screen,
                        CallLogService_t* call_log,
                        ContactService_t* contacts);

/**
 * @brief Affiche l'écran journal des appels
 * 
 * Rendu complet de l'écran : barre de statut, liste filtrée,
 * boutons de navigation. Appelé lors de la transition vers cet écran.
 * 
 * @param screen        Pointeur vers la structure de l'écran
 */
void ScreenCallLog_Show(ScreenCallLog_t* screen);

/**
 * @brief Masque l'écran (avant transition vers un autre)
 * @param screen        Pointeur vers la structure de l'écran
 */
void ScreenCallLog_Hide(ScreenCallLog_t* screen);

/**
 * @brief Met à jour l'écran (appelé périodiquement)
 * 
 * Rafraîchit l'affichage si des changements sont détectés
 * (nouvel appel, suppression, etc.)
 * 
 * @param screen        Pointeur vers la structure de l'écran
 */
void ScreenCallLog_Update(ScreenCallLog_t* screen);

/**
 * @brief Gère les événements tactiles
 * 
 * Traite les appuis sur la liste, les boutons de filtre,
 * le défilement, etc.
 * 
 * @param screen        Pointeur vers la structure de l'écran
 * @param event         Événement tactile reçu
 * @return              true si l'événement a été consommé
 */
bool ScreenCallLog_HandleTouch(ScreenCallLog_t* screen,
                               const TouchEvent_t* event);

/**
 * @brief Gère les événements clavier physiques
 * 
 * Navigation dans la liste avec les touches fléchées,
 * sélection avec OK, retour avec touche Retour.
 * 
 * @param screen        Pointeur vers la structure de l'écran
 * @param key           Code de la touche pressée
 * @return              true si l'événement a été consommé
 */
bool ScreenCallLog_HandleKey(ScreenCallLog_t* screen, 
                             KeyCode_t key);

/**
 * @brief Rafraîchit la liste depuis le service
 * 
 * Recharge toutes les entrées et reconstruit l'affichage.
 * Appelé après un nouvel appel ou une suppression.
 * 
 * @param screen        Pointeur vers la structure de l'écran
 */
void ScreenCallLog_RefreshList(ScreenCallLog_t* screen);

/**
 * @brief Définit le filtre actif
 * 
 * @param screen        Pointeur vers la structure de l'écran
 * @param filter        Type de filtre à appliquer
 */
void ScreenCallLog_SetFilter(ScreenCallLog_t* screen, 
                             CallLogFilter_t filter);

/**
 * @brief Supprime une entrée de l'historique
 * 
 * Affiche une confirmation puis supprime si confirmé.
 * 
 * @param screen        Pointeur vers la structure de l'écran
 * @param entry_id      ID de l'entrée à supprimer
 */
void ScreenCallLog_DeleteEntry(ScreenCallLog_t* screen,
                               uint32_t entry_id);

/**
 * @brief Efface tout l'historique
 * 
 * @param screen        Pointeur vers la structure de l'écran
 */
void ScreenCallLog_ClearAll(ScreenCallLog_t* screen);

/**
 * @brief Affiche le détail d'une entrée
 * 
 * Ouvre un panneau avec les informations complètes :
 * nom, numéro, date/heure, durée, type d'appel.
 * 
 * @param screen        Pointeur vers la structure de l'écran
 * @param index         Index de l'entrée dans la liste affichée
 */
void ScreenCallLog_ShowDetail(ScreenCallLog_t* screen, 
                              int16_t index);

/**
 * @brief Masque le panneau de détail
 * @param screen        Pointeur vers la structure de l'écran
 */
void ScreenCallLog_HideDetail(ScreenCallLog_t* screen);

/**
 * @brief Libère les ressources de l'écran
 * @param screen        Pointeur vers la structure de l'écran
 */
void ScreenCallLog_Deinit(ScreenCallLog_t* screen);

/* ======================================================================== */
/*                    FONCTIONS INTERNES (privées)                           */
/* ======================================================================== */

/* Ces fonctions sont déclarées dans le .c, pas ici.
   Présentes uniquement pour référence de l'architecture. */

#if 0  /* Documentation uniquement */

/**
 * @brief Construit la liste d'affichage à partir des données brutes
 */
static void build_display_list(ScreenCallLog_t* screen);

/**
 * @brief Formate une entrée pour l'affichage
 */
static void format_display_entry(const CallLogEntry_t* raw,
                                 CallLogDisplayEntry_t* display,
                                 ContactService_t* contacts);

/**
 * @brief Dessine une ligne de la liste
 */
static void draw_row(ScreenCallLog_t* screen, 
                     int16_t index, 
                     int16_t y_position);

/**
 * @brief Met à jour les boutons de filtre
 */
static void update_filter_buttons(ScreenCallLog_t* screen);

/**
 * @brief Callback de sélection d'une entrée
 */
static void on_row_selected(void* context, int16_t index);

/**
 * @brief Callback du dialogue de confirmation
 */
static void on_delete_confirmed(void* context, bool confirmed);

#endif /* Documentation */

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_CALL_LOG_H */

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */