/**
 * @file    screen_call_log.cpp
 * @brief   Implémentation de l'écran journal des appels
 * @author  Votre Nom
 * @date    2026
 * 
 * Affiche l'historique des appels avec :
 * - Liste défilante fluide (DMA2D)
 * - Filtrage par type (tous/manqués/entrants/sortants)
 * - Détail d'un appel au tap
 * - Suppression unitaire ou complète
 * - Icônes et couleurs distinctes par type d'appel
 * 
 * Dépendances matérielles STM32F429 :
 * - LTDC   : Affichage framebuffer (couche 1)
 * - DMA2D  : Accélération scroll et rendu
 * - SDRAM  : Framebuffer secondaire (si disponible)
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "screen_call_log.h"

#include "../ui/ui_core.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_widgets.h"
#include "../ui/ui_list.h"
#include "../ui/ui_label.h"
#include "../ui/ui_button.h"
#include "../ui/ui_dialog.h"
#include "../ui/ui_icons.h"
#include "../ui/ui_statusbar.h"
#include "../ui/ui_navigation.h"
#include "../ui/ui_animations.h"
#include "../ui/ui_draw_primitives.h"
#include "../ui/ui_fonts.h"

#include "../services/call_log_service.h"
#include "../services/contact_service.h"
#include "../app/app_events.h"
#include "../app/app_state_machine.h"

#include "../utils/string_utils.h"
#include "../utils/timer_utils.h"
#include "../utils/debug_utils.h"

#include "../drivers/display/display_manager.h"
#include "../drivers/display/dma2d_driver.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs de debug */
#define TAG                         "ScreenCallLog"

/** Dimensions de l'écran */
#define SCREEN_WIDTH                320
#define SCREEN_HEIGHT               480

/** Zone de la liste (sous la barre de titre et les filtres) */
#define LIST_AREA_Y                 90
#define LIST_AREA_HEIGHT            300
#define LIST_AREA_WIDTH             310
#define LIST_AREA_X                 5

/** Position des boutons de filtre */
#define FILTER_BUTTONS_Y            50
#define FILTER_BUTTON_HEIGHT        32
#define FILTER_BUTTON_SPACING       4

/** Zone de détail */
#define DETAIL_PANEL_Y              100
#define DETAIL_PANEL_HEIGHT         280
#define DETAIL_PANEL_WIDTH          290
#define DETAIL_PANEL_X              15

/** Durée animation ouverture détail (ms) */
#define DETAIL_ANIM_DURATION_MS     200

/** Timeout rafraîchissement automatique (ms) */
#define REFRESH_TIMEOUT_MS          5000

/** Tampon pour formatage chaînes */
#define TEMP_STR_BUFFER_SIZE        64

/* ======================================================================== */
/*                    VARIABLES STATIQUES PRIVÉES                            */
/* ======================================================================== */

/** Couleurs par type d'appel (indexées par CallType_t) */
static const uint16_t CALL_TYPE_COLORS[] = {
    [CALL_TYPE_INCOMING] = 0x07E0,   /**< Vert pour entrants    */
    [CALL_TYPE_OUTGOING] = 0x041F,   /**< Bleu pour sortants    */
    [CALL_TYPE_MISSED]   = 0xF800,   /**< Rouge pour manqués    */
};

/** Icônes par type d'appel */
static const UIIcon_t CALL_TYPE_ICONS[] = {
    [CALL_TYPE_INCOMING] = ICON_CALL_INCOMING,
    [CALL_TYPE_OUTGOING] = ICON_CALL_OUTGOING,
    [CALL_TYPE_MISSED]   = ICON_CALL_MISSED,
};

/** Labels des boutons de filtre */
static const char* FILTER_LABELS[] = {
    "Tous",
    "Manques",
    "Entrants",
    "Sortants"
};

/** Largeurs des boutons de filtre */
static const uint16_t FILTER_BUTTON_WIDTHS[] = { 68, 78, 80, 76 };

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

static void build_display_list(ScreenCallLog_t* screen);
static void format_display_entry(const CallLogEntry_t* raw,
                                 CallLogDisplayEntry_t* display,
                                 ContactService_t* contacts);
static void draw_row(void* context, int16_t index, int16_t y_position);
static void update_filter_buttons(ScreenCallLog_t* screen);
static void on_row_selected(void* context, int16_t index);
static void on_delete_confirmed(void* context, bool confirmed);
static void on_back_clicked(void* context);
static void on_clear_clicked(void* context);
static void on_filter_clicked(void* context, uint8_t filter_index);
static void draw_detail_panel(ScreenCallLog_t* screen);
static void refresh_timer_callback(TimerHandle_t timer);
static void scroll_list_up(ScreenCallLog_t* screen);
static void scroll_list_down(ScreenCallLog_t* screen);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise l'écran journal des appels
 */
bool ScreenCallLog_Init(ScreenCallLog_t* screen,
                        CallLogService_t* call_log,
                        ContactService_t* contacts)
{
    if (!screen || !call_log || !contacts) {
        DEBUG_ERROR(TAG, "Paramètres invalides");
        return false;
    }

    DEBUG_INFO(TAG, "Initialisation de l'écran journal des appels");

    /* Initialisation de la classe de base */
    memset(screen, 0, sizeof(ScreenCallLog_t));
    ScreenBase_Init(&screen->base, SCREEN_ID_CALL_LOG, "Journal appels");

    /* Sauvegarde des services */
    screen->call_log_service = call_log;
    screen->contact_service = contacts;

    /* État initial */
    screen->state = CALL_LOG_STATE_IDLE;
    screen->active_filter = CALL_LOG_FILTER_ALL;
    screen->selected_index = -1;
    screen->scroll_offset = 0;
    screen->detail_visible = false;
    screen->total_entries = 0;

    /* Création des widgets UI */
    
    /* 1. Label titre */
    screen->title_label = UILabel_Create();
    UILabel_SetText(screen->title_label, "Journal appels");
    UILabel_SetFont(screen->title_label, &font_large_bold);
    UILabel_SetColor(screen->title_label, THEME_TEXT_PRIMARY);
    UILabel_SetPosition(screen->title_label, 10, 5);

    /* 2. Label filtre actif */
    screen->filter_label = UILabel_Create();
    UILabel_SetText(screen->filter_label, FILTER_LABELS[CALL_LOG_FILTER_ALL]);
    UILabel_SetFont(screen->filter_label, &font_small);
    UILabel_SetColor(screen->filter_label, THEME_ACCENT);
    UILabel_SetPosition(screen->filter_label, 220, 8);

    /* 3. Label "vide" */
    screen->empty_label = UILabel_Create();
    UILabel_SetText(screen->empty_label, "Aucun appel");
    UILabel_SetFont(screen->empty_label, &font_medium);
    UILabel_SetColor(screen->empty_label, THEME_TEXT_SECONDARY);
    UILabel_SetPosition(screen->empty_label, 
                        SCREEN_WIDTH / 2 - 40, 
                        SCREEN_HEIGHT / 2 - 10);
    UILabel_SetVisible(screen->empty_label, false);

    /* 4. Boutons de filtre */
    uint16_t filter_x = 8;
    for (int i = 0; i < 4; i++) {
        screen->filter_buttons[i] = UIButton_Create();
        UIButton_SetText(screen->filter_buttons[i], FILTER_LABELS[i]);
        UIButton_SetFont(screen->filter_buttons[i], &font_small);
        UIButton_SetSize(screen->filter_buttons[i], 
                         FILTER_BUTTON_WIDTHS[i], 
                         FILTER_BUTTON_HEIGHT);
        UIButton_SetPosition(screen->filter_buttons[i], 
                             filter_x, 
                             FILTER_BUTTONS_Y);
        UIButton_SetOnClick(screen->filter_buttons[i], 
                            on_filter_clicked, 
                            screen);
        UIButton_SetUserData(screen->filter_buttons[i], (void*)(uintptr_t)i);
        UIButton_SetCornerRadius(screen->filter_buttons[i], 6);
        
        filter_x += FILTER_BUTTON_WIDTHS[i] + FILTER_BUTTON_SPACING;
    }

    /* 5. Widget liste */
    screen->list_widget = UIList_Create();
    UIList_SetPosition(screen->list_widget, LIST_AREA_X, LIST_AREA_Y);
    UIList_SetSize(screen->list_widget, LIST_AREA_WIDTH, LIST_AREA_HEIGHT);
    UIList_SetRowHeight(screen->list_widget, CALL_LOG_ROW_HEIGHT);
    UIList_SetRowCount(screen->list_widget, 0);
    UIList_SetDrawRowCallback(screen->list_widget, draw_row, screen);
    UIList_SetOnSelectCallback(screen->list_widget, on_row_selected, screen);
    UIList_SetScrollBarVisible(screen->list_widget, true);
    UIList_SetScrollBarColor(screen->list_widget, THEME_SCROLLBAR);

    /* 6. Bouton Retour */
    screen->back_button = UIButton_Create();
    UIButton_SetText(screen->back_button, "Retour");
    UIButton_SetFont(screen->back_button, &font_medium);
    UIButton_SetSize(screen->back_button, 140, 40);
    UIButton_SetPosition(screen->back_button, 10, SCREEN_HEIGHT - 50);
    UIButton_SetOnClick(screen->back_button, on_back_clicked, screen);
    UIButton_SetCornerRadius(screen->back_button, 8);
    UIButton_SetColor(screen->back_button, THEME_BUTTON_SECONDARY);

    /* 7. Bouton "Tout effacer" */
    screen->clear_button = UIButton_Create();
    UIButton_SetText(screen->clear_button, "Effacer tout");
    UIButton_SetFont(screen->clear_button, &font_medium);
    UIButton_SetSize(screen->clear_button, 150, 40);
    UIButton_SetPosition(screen->clear_button, 160, SCREEN_HEIGHT - 50);
    UIButton_SetOnClick(screen->clear_button, on_clear_clicked, screen);
    UIButton_SetCornerRadius(screen->clear_button, 8);
    UIButton_SetColor(screen->clear_button, THEME_BUTTON_DANGER);

    /* 8. Dialogue de confirmation */
    screen->confirm_dialog = UIDialog_Create();
    UIDialog_SetTitle(screen->confirm_dialog, "Confirmation");
    UIDialog_SetMessage(screen->confirm_dialog, 
                        "Supprimer cet appel ?");
    UIDialog_SetOnResult(screen->confirm_dialog, 
                         on_delete_confirmed, 
                         screen);
    UIDialog_SetVisible(screen->confirm_dialog, false);

    /* 9. Barre de statut */
    UIStatusBar_Init(&screen->status_bar);

    /* Chargement initial des données */
    build_display_list(screen);

    /* Timer de rafraîchissement */
    screen->refresh_timer = Timer_Create("CallLogRefresh",
                                         REFRESH_TIMEOUT_MS,
                                         true,  /* auto-reload */
                                         refresh_timer_callback,
                                         screen);

    DEBUG_INFO(TAG, "Écran journal initialisé avec %d entrées", 
               screen->total_entries);

    return true;
}

/**
 * @brief Affiche l'écran
 */
void ScreenCallLog_Show(ScreenCallLog_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Affichage de l'écran journal");

    /* Rafraîchir les données */
    build_display_list(screen);

    /* Rendu du fond */
    Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);

    /* Barre de statut */
    UIStatusBar_Draw(&screen->status_bar);

    /* Titre */
    UILabel_Draw(screen->title_label);

    /* Barre de séparation */
    Display_DrawHLine(0, 30, SCREEN_WIDTH, THEME_DIVIDER);

    /* Boutons de filtre */
    update_filter_buttons(screen);

    /* Séparation sous les filtres */
    Display_DrawHLine(0, FILTER_BUTTONS_Y + FILTER_BUTTON_HEIGHT + 4,
                      SCREEN_WIDTH, THEME_DIVIDER);

    /* Liste ou message vide */
    if (screen->total_entries > 0) {
        UIList_SetVisible(screen->list_widget, true);
        UILabel_SetVisible(screen->empty_label, false);
        UIList_Draw(screen->list_widget);
    } else {
        UIList_SetVisible(screen->list_widget, false);
        UILabel_SetVisible(screen->empty_label, true);
        UILabel_Draw(screen->empty_label);
    }

    /* Boutons du bas */
    UIButton_Draw(screen->back_button);
    UIButton_Draw(screen->clear_button);

    /* Ligne séparation boutons */
    Display_DrawHLine(0, SCREEN_HEIGHT - 58, SCREEN_WIDTH, THEME_DIVIDER);

    /* Détail si ouvert */
    if (screen->detail_visible) {
        draw_detail_panel(screen);
    }

    /* Dialogue si visible */
    UIDialog_Draw(screen->confirm_dialog);

    /* Démarrer le timer de rafraîchissement */
    Timer_Start(screen->refresh_timer);

    /* Marquer comme affiché */
    screen->base.is_visible = true;
}

/**
 * @brief Masque l'écran
 */
void ScreenCallLog_Hide(ScreenCallLog_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Masquage de l'écran journal");

    Timer_Stop(screen->refresh_timer);
    screen->base.is_visible = false;
    screen->detail_visible = false;
    screen->selected_index = -1;
}

/**
 * @brief Mise à jour périodique
 */
void ScreenCallLog_Update(ScreenCallLog_t* screen)
{
    if (!screen || !screen->base.is_visible) return;

    /* Mise à jour de la barre de statut (heure qui change) */
    UIStatusBar_Update(&screen->status_bar);

    /* Si en détail, vérifier si besoin de rafraîchir */
    if (screen->state == CALL_LOG_STATE_DETAIL) {
        /* Pas de mise à jour en mode détail */
        return;
    }
}

/**
 * @brief Gère les événements tactiles
 */
bool ScreenCallLog_HandleTouch(ScreenCallLog_t* screen,
                               const TouchEvent_t* event)
{
    if (!screen || !event) return false;

    /* Si dialogue visible, le gérer en priorité */
    if (UIDialog_IsVisible(screen->confirm_dialog)) {
        return UIDialog_HandleTouch(screen->confirm_dialog, event);
    }

    /* Si détail visible, gérer le tap hors du panneau */
    if (screen->detail_visible) {
        if (event->type == TOUCH_EVENT_TAP) {
            /* Tap en dehors du panneau de détail ? */
            if (event->x < DETAIL_PANEL_X || 
                event->x > DETAIL_PANEL_X + DETAIL_PANEL_WIDTH ||
                event->y < DETAIL_PANEL_Y || 
                event->y > DETAIL_PANEL_Y + DETAIL_PANEL_HEIGHT) {
                ScreenCallLog_HideDetail(screen);
                return true;
            }
        }
        return false;  /* Laisser le détail consommer */
    }

    /* Vérifier les boutons de filtre */
    for (int i = 0; i < 4; i++) {
        if (UIButton_HitTest(screen->filter_buttons[i], event->x, event->y)) {
            if (event->type == TOUCH_EVENT_TAP) {
                UIButton_TriggerClick(screen->filter_buttons[i]);
            }
            return true;
        }
    }

    /* Vérifier la liste */
    if (UIList_HitTest(screen->list_widget, event->x, event->y)) {
        return UIList_HandleTouch(screen->list_widget, event);
    }

    /* Vérifier le bouton Retour */
    if (UIButton_HitTest(screen->back_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->back_button);
        }
        return true;
    }

    /* Vérifier le bouton Effacer */
    if (UIButton_HitTest(screen->clear_button, event->x, event->y)) {
        if (event->type == TOUCH_EVENT_TAP) {
            UIButton_TriggerClick(screen->clear_button);
        }
        return true;
    }

    /* Vérifier le swipe haut/bas pour scroller */
    if (event->type == TOUCH_EVENT_SWIPE_UP) {
        scroll_list_down(screen);
        return true;
    }
    if (event->type == TOUCH_EVENT_SWIPE_DOWN) {
        scroll_list_up(screen);
        return true;
    }

    return false;
}

/**
 * @brief Gère les événements clavier physiques
 */
bool ScreenCallLog_HandleKey(ScreenCallLog_t* screen, KeyCode_t key)
{
    if (!screen) return false;

    switch (key) {
        case KEY_UP:
            scroll_list_up(screen);
            return true;

        case KEY_DOWN:
            scroll_list_down(screen);
            return true;

        case KEY_OK:
        case KEY_SELECT:
            if (screen->selected_index >= 0 && !screen->detail_visible) {
                ScreenCallLog_ShowDetail(screen, screen->selected_index);
            }
            return true;

        case KEY_BACK:
        case KEY_CANCEL:
            if (screen->detail_visible) {
                ScreenCallLog_HideDetail(screen);
            } else {
                on_back_clicked(screen);
            }
            return true;

        case KEY_DELETE:
            if (screen->selected_index >= 0 && !screen->detail_visible) {
                /* Supprimer l'entrée sélectionnée */
                uint32_t entry_id = 
                    screen->entries[screen->selected_index].entry_id;
                ScreenCallLog_DeleteEntry(screen, entry_id);
            }
            return true;

        default:
            break;
    }

    return false;
}

/**
 * @brief Rafraîchit la liste depuis le service
 */
void ScreenCallLog_RefreshList(ScreenCallLog_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Rafraîchissement de la liste");

    build_display_list(screen);

    if (screen->base.is_visible) {
        /* Redessiner la zone de la liste */
        Display_FillRect(LIST_AREA_X, LIST_AREA_Y, 
                         LIST_AREA_WIDTH, LIST_AREA_HEIGHT, 
                         THEME_BG_MAIN);

        if (screen->total_entries > 0) {
            UIList_SetRowCount(screen->list_widget, screen->total_entries);
            UIList_SetVisible(screen->list_widget, true);
            UILabel_SetVisible(screen->empty_label, false);
            UIList_Draw(screen->list_widget);
        } else {
            UIList_SetVisible(screen->list_widget, false);
            UILabel_SetVisible(screen->empty_label, true);
            UILabel_Draw(screen->empty_label);
        }
    }
}

/**
 * @brief Définit le filtre actif
 */
void ScreenCallLog_SetFilter(ScreenCallLog_t* screen, 
                             CallLogFilter_t filter)
{
    if (!screen || filter >= CALL_LOG_FILTER_COUNT) return;

    DEBUG_INFO(TAG, "Changement filtre: %d -> %d", 
               screen->active_filter, filter);

    screen->active_filter = filter;
    screen->selected_index = -1;
    screen->scroll_offset = 0;

    /* Mettre à jour le label */
    UILabel_SetText(screen->filter_label, FILTER_LABELS[filter]);

    build_display_list(screen);

    if (screen->base.is_visible) {
        update_filter_buttons(screen);
        ScreenCallLog_RefreshList(screen);
    }
}

/**
 * @brief Supprime une entrée
 */
void ScreenCallLog_DeleteEntry(ScreenCallLog_t* screen,
                               uint32_t entry_id)
{
    if (!screen) return;

    /* Mémoriser l'ID pour le callback */
    screen->detail_entry.entry_id = entry_id;

    /* Afficher la confirmation */
    UIDialog_SetMessage(screen->confirm_dialog,
                        "Supprimer cet appel de l'historique ?");
    UIDialog_SetVisible(screen->confirm_dialog, true);
    screen->state = CALL_LOG_STATE_DELETE_CONFIRM;

    if (screen->base.is_visible) {
        UIDialog_Draw(screen->confirm_dialog);
    }
}

/**
 * @brief Efface tout l'historique
 */
void ScreenCallLog_ClearAll(ScreenCallLog_t* screen)
{
    if (!screen) return;

    /* Confirmation */
    UIDialog_SetMessage(screen->confirm_dialog,
                        "Supprimer TOUT l'historique ?\nCette action est irreversible.");
    /* ID spécial pour "tout effacer" */
    screen->detail_entry.entry_id = 0xFFFFFFFF;
    UIDialog_SetVisible(screen->confirm_dialog, true);
    screen->state = CALL_LOG_STATE_DELETE_CONFIRM;

    if (screen->base.is_visible) {
        UIDialog_Draw(screen->confirm_dialog);
    }
}

/**
 * @brief Affiche le détail d'une entrée
 */
void ScreenCallLog_ShowDetail(ScreenCallLog_t* screen, int16_t index)
{
    if (!screen || index < 0 || index >= screen->total_entries) return;

    DEBUG_INFO(TAG, "Affichage détail entrée %d", index);

    /* Copier l'entrée */
    memcpy(&screen->detail_entry, 
           &screen->entries[index], 
           sizeof(CallLogDisplayEntry_t));

    screen->detail_visible = true;
    screen->state = CALL_LOG_STATE_DETAIL;

    if (screen->base.is_visible) {
        /* Animation d'ouverture */
        UIAnimation_FadeIn(DETAIL_PANEL_X, DETAIL_PANEL_Y,
                           DETAIL_PANEL_WIDTH, DETAIL_PANEL_HEIGHT,
                           DETAIL_ANIM_DURATION_MS);
        draw_detail_panel(screen);
    }
}

/**
 * @brief Masque le panneau de détail
 */
void ScreenCallLog_HideDetail(ScreenCallLog_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Masquage détail");

    screen->detail_visible = false;
    screen->state = CALL_LOG_STATE_IDLE;

    if (screen->base.is_visible) {
        /* Animation de fermeture */
        UIAnimation_FadeOut(DETAIL_PANEL_X, DETAIL_PANEL_Y,
                            DETAIL_PANEL_WIDTH, DETAIL_PANEL_HEIGHT,
                            DETAIL_ANIM_DURATION_MS / 2);

        /* Redessiner la liste en dessous */
        Display_FillRect(DETAIL_PANEL_X, DETAIL_PANEL_Y,
                         DETAIL_PANEL_WIDTH, DETAIL_PANEL_HEIGHT,
                         THEME_BG_MAIN);
        UIList_Draw(screen->list_widget);
    }
}

/**
 * @brief Libère les ressources
 */
void ScreenCallLog_Deinit(ScreenCallLog_t* screen)
{
    if (!screen) return;

    DEBUG_INFO(TAG, "Libération des ressources");

    Timer_Delete(screen->refresh_timer);

    UILabel_Destroy(screen->title_label);
    UILabel_Destroy(screen->filter_label);
    UILabel_Destroy(screen->empty_label);

    for (int i = 0; i < 4; i++) {
        UIButton_Destroy(screen->filter_buttons[i]);
    }

    UIList_Destroy(screen->list_widget);
    UIButton_Destroy(screen->back_button);
    UIButton_Destroy(screen->clear_button);
    UIDialog_Destroy(screen->confirm_dialog);

    memset(screen, 0, sizeof(ScreenCallLog_t));
}

/* ======================================================================== */
/*                  FONCTIONS PRIVÉES                                       */
/* ======================================================================== */

/**
 * @brief Construit la liste d'affichage filtrée
 */
static void build_display_list(ScreenCallLog_t* screen)
{
    if (!screen || !screen->call_log_service) return;

    /* Récupérer les entrées brutes du service */
    CallLogEntry_t raw_entries[CALL_LOG_MAX_ENTRIES];
    uint16_t raw_count = 0;

    CallLogService_GetAll(screen->call_log_service, 
                          raw_entries, 
                          CALL_LOG_MAX_ENTRIES, 
                          &raw_count);

    /* Filtrer et formater */
    screen->total_entries = 0;

    for (uint16_t i = 0; i < raw_count; i++) {
        /* Appliquer le filtre */
        if (screen->active_filter != CALL_LOG_FILTER_ALL) {
            CallType_t filter_type;
            switch (screen->active_filter) {
                case CALL_LOG_FILTER_MISSED:
                    filter_type = CALL_TYPE_MISSED;
                    break;
                case CALL_LOG_FILTER_INCOMING:
                    filter_type = CALL_TYPE_INCOMING;
                    break;
                case CALL_LOG_FILTER_OUTGOING:
                    filter_type = CALL_TYPE_OUTGOING;
                    break;
                default:
                    filter_type = CALL_TYPE_INCOMING;
                    break;
            }
            if (raw_entries[i].call_type != filter_type) {
                continue;
            }
        }

        /* Formater pour l'affichage */
        if (screen->total_entries < CALL_LOG_MAX_ENTRIES) {
            format_display_entry(&raw_entries[i],
                                &screen->entries[screen->total_entries],
                                screen->contact_service);
            screen->total_entries++;
        }
    }

    /* Mise à jour du widget liste */
    UIList_SetRowCount(screen->list_widget, screen->total_entries);

    DEBUG_VERBOSE(TAG, "Liste construite: %d entrées (filtrées depuis %d)",
                  screen->total_entries, raw_count);
}

/**
 * @brief Formate une entrée brute pour l'affichage
 */
static void format_display_entry(const CallLogEntry_t* raw,
                                 CallLogDisplayEntry_t* display,
                                 ContactService_t* contacts)
{
    if (!raw || !display) return;

    memset(display, 0, sizeof(CallLogDisplayEntry_t));

    display->entry_id = raw->id;
    display->call_type = raw->call_type;
    display->duration_sec = raw->duration_sec;
    display->icon_index = (uint8_t)raw->call_type;

    /* Résoudre le nom du contact */
    ContactEntry_t contact;
    bool contact_found = false;

    if (contacts) {
        contact_found = ContactService_FindByNumber(contacts, 
                                                     raw->number, 
                                                     &contact);
    }

    if (contact_found && strlen(contact.display_name) > 0) {
        strncpy(display->display_name, contact.display_name, 31);
        display->display_name[31] = '\0';
    } else {
        /* Afficher le numéro comme nom */
        strncpy(display->display_name, raw->number, 31);
        display->display_name[31] = '\0';
    }

    /* Numéro formaté */
    strncpy(display->display_number, raw->number, 19);
    display->display_number[19] = '\0';

    /* Formater l'heure */
    struct tm time_info;
    localtime_r(&raw->timestamp, &time_info);
    snprintf(display->time_str, sizeof(display->time_str),
             "%02d:%02d", time_info.tm_hour, time_info.tm_min);

    /* Formater la date (format court) */
    snprintf(display->date_str, sizeof(display->date_str),
             "%02d/%02d/%04d", 
             time_info.tm_mday, 
             time_info.tm_mon + 1, 
             time_info.tm_year + 1900);
}

/**
 * @brief Dessine une ligne de la liste (callback du widget UIList)
 * 
 * Utilise DMA2D pour un rendu accéléré de la ligne complète.
 */
static void draw_row(void* context, int16_t index, int16_t y_position)
{
    ScreenCallLog_t* screen = (ScreenCallLog_t*)context;

    if (!screen || index < 0 || index >= screen->total_entries) return;

    CallLogDisplayEntry_t* entry = &screen->entries[index];

    /* Coordonnées de la ligne */
    int16_t row_x = LIST_AREA_X;
    int16_t row_y = LIST_AREA_Y + y_position;
    int16_t row_w = LIST_AREA_WIDTH;
    int16_t row_h = CALL_LOG_ROW_HEIGHT - 1;  /* -1 pour séparateur */

    /* Fond de la ligne */
    uint16_t bg_color;
    if (index == screen->selected_index) {
        bg_color = THEME_LIST_SELECTED;
    } else if (index % 2 == 0) {
        bg_color = THEME_LIST_EVEN;
    } else {
        bg_color = THEME_LIST_ODD;
    }

    /* Rendu accéléré du fond avec DMA2D */
    DMA2D_FillRect(row_x, row_y, row_w, row_h, bg_color);

    /* Icône du type d'appel */
    int16_t icon_x = row_x + 8;
    int16_t icon_y = row_y + (row_h - 24) / 2;
    uint16_t icon_color = CALL_TYPE_COLORS[entry->call_type];

    UIIcons_Draw(CALL_TYPE_ICONS[entry->call_type], 
                 icon_x, icon_y, 
                 icon_color);

    /* Nom du contact (ou numéro) */
    int16_t text_x = icon_x + 32;
    int16_t text_y = row_y + 4;

    /* Si le nom est trop long, le tronquer */
    char display_name[25];
    StringUtils_Truncate(entry->display_name, 24, display_name);

    /* Couleur du texte selon type */
    uint16_t text_color;
    if (entry->call_type == CALL_TYPE_MISSED) {
        text_color = THEME_TEXT_DANGER;  /* Rouge pour manqués */
    } else {
        text_color = THEME_TEXT_PRIMARY;
    }

    Display_DrawText(text_x, text_y, 
                     display_name, 
                     &font_medium, 
                     text_color,
                     bg_color);

    /* Numéro (en dessous du nom) */
    Display_DrawText(text_x, text_y + 22,
                     entry->display_number,
                     &font_small,
                     THEME_TEXT_SECONDARY,
                     bg_color);

    /* Heure (alignée à droite) */
    int16_t time_x = row_x + row_w - 50;
    int16_t time_y = row_y + 4;

    Display_DrawText(time_x, time_y,
                     entry->time_str,
                     &font_small,
                     THEME_TEXT_SECONDARY,
                     bg_color);

    /* Durée (si appel répondu) */
    if (entry->duration_sec > 0) {
        char duration_str[16];
        uint16_t minutes = entry->duration_sec / 60;
        uint16_t seconds = entry->duration_sec % 60;
        snprintf(duration_str, sizeof(duration_str),
                 "%d:%02d", minutes, seconds);

        Display_DrawText(time_x, time_y + 22,
                         duration_str,
                         &font_small,
                         THEME_TEXT_TERTIARY,
                         bg_color);
    }

    /* Ligne séparatrice fine en bas */
    Display_DrawHLine(row_x, row_y + row_h, row_w, THEME_LIST_SEPARATOR);
}

/**
 * @brief Met à jour l'apparence des boutons de filtre
 */
static void update_filter_buttons(ScreenCallLog_t* screen)
{
    for (int i = 0; i < 4; i++) {
        if (i == (int)screen->active_filter) {
            /* Bouton actif : couleur accentuée */
            UIButton_SetColor(screen->filter_buttons[i], THEME_ACCENT);
            UIButton_SetTextColor(screen->filter_buttons[i], THEME_TEXT_ON_ACCENT);
        } else {
            /* Bouton inactif : couleur neutre */
            UIButton_SetColor(screen->filter_buttons[i], THEME_BUTTON_NEUTRAL);
            UIButton_SetTextColor(screen->filter_buttons[i], THEME_TEXT_PRIMARY);
        }
        UIButton_Draw(screen->filter_buttons[i]);
    }
}

/**
 * @brief Callback quand une ligne est sélectionnée
 */
static void on_row_selected(void* context, int16_t index)
{
    ScreenCallLog_t* screen = (ScreenCallLog_t*)context;
    if (!screen) return;

    DEBUG_INFO(TAG, "Ligne sélectionnée: %d", index);

    /* Mettre à jour la sélection */
    int16_t old_selected = screen->selected_index;
    screen->selected_index = index;

    /* Redessiner l'ancienne et la nouvelle ligne */
    if (old_selected >= 0) {
        UIList_RedrawRow(screen->list_widget, old_selected);
    }
    if (index >= 0) {
        UIList_RedrawRow(screen->list_widget, index);

        /* Afficher le détail après un court délai */
        ScreenCallLog_ShowDetail(screen, index);
    }
}

/**
 * @brief Callback du dialogue de confirmation
 */
static void on_delete_confirmed(void* context, bool confirmed)
{
    ScreenCallLog_t* screen = (ScreenCallLog_t*)context;
    if (!screen) return;

    UIDialog_SetVisible(screen->confirm_dialog, false);
    screen->state = CALL_LOG_STATE_IDLE;

    if (confirmed) {
        if (screen->detail_entry.entry_id == 0xFFFFFFFF) {
            /* Effacer tout */
            DEBUG_INFO(TAG, "Suppression de tout l'historique");
            CallLogService_ClearAll(screen->call_log_service);
        } else {
            /* Effacer une entrée */
            DEBUG_INFO(TAG, "Suppression entrée ID=%lu", 
                       screen->detail_entry.entry_id);
            CallLogService_Delete(screen->call_log_service,
                                  screen->detail_entry.entry_id);
        }

        /* Rafraîchir l'affichage */
        screen->selected_index = -1;
        screen->detail_visible = false;
        ScreenCallLog_RefreshList(screen);
    }

    /* Redessiner sans le dialogue */
    if (screen->base.is_visible) {
        /* Effacer la zone du dialogue */
        Display_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_BG_MAIN);
        ScreenCallLog_Show(screen);
    }
}

/**
 * @brief Callback bouton Retour
 */
static void on_back_clicked(void* context)
{
    ScreenCallLog_t* screen = (ScreenCallLog_t*)context;
    if (!screen) return;

    DEBUG_INFO(TAG, "Retour demandé");

    if (screen->on_back_pressed) {
        screen->on_back_pressed();
    } else {
        /* Comportement par défaut : revenir à l'écran d'accueil */
        AppStateMachine_GoBack();
    }
}

/**
 * @brief Callback bouton Effacer tout
 */
static void on_clear_clicked(void* context)
{
    ScreenCallLog_t* screen = (ScreenCallLog_t*)context;
    if (!screen) return;

    DEBUG_INFO(TAG, "Demande effacement complet");
    ScreenCallLog_ClearAll(screen);
}

/**
 * @brief Callback bouton de filtre
 */
static void on_filter_clicked(void* context, uint8_t filter_index)
{
    ScreenCallLog_t* screen = (ScreenCallLog_t*)context;
    if (!screen || filter_index >= CALL_LOG_FILTER_COUNT) return;

    DEBUG_INFO(TAG, "Filtre cliqué: %d", filter_index);
    ScreenCallLog_SetFilter(screen, (CallLogFilter_t)filter_index);
}

/**
 * @brief Dessine le panneau de détail
 */
static void draw_detail_panel(ScreenCallLog_t* screen)
{
    if (!screen || !screen->detail_visible) return;

    CallLogDisplayEntry_t* entry = &screen->detail_entry;

    /* Fond du panneau avec ombre */
    uint16_t panel_bg = THEME_SURFACE;
    int16_t px = DETAIL_PANEL_X;
    int16_t py = DETAIL_PANEL_Y;
    int16_t pw = DETAIL_PANEL_WIDTH;
    int16_t ph = DETAIL_PANEL_HEIGHT;

    /* Ombre (décalage 3px) */
    Display_FillRoundRect(px + 3, py + 3, pw, ph, 10, THEME_SHADOW);

    /* Fond principal */
    Display_FillRoundRect(px, py, pw, ph, 10, panel_bg);

    /* Bordure */
    Display_DrawRoundRect(px, py, pw, ph, 10, THEME_DIVIDER);

    /* En-tête */
    Display_FillRoundRectTop(px, py, pw, 40, 10, THEME_PRIMARY);

    /* Titre */
    Display_DrawText(px + 15, py + 10, 
                     "Detail appel", 
                     &font_medium_bold, 
                     THEME_TEXT_ON_PRIMARY,
                     THEME_PRIMARY);

    /* Icône type */
    int16_t icon_x = px + 20;
    int16_t icon_y = py + 60;
    uint16_t icon_color = CALL_TYPE_COLORS[entry->call_type];

    UIIcons_DrawLarge(CALL_TYPE_ICONS[entry->call_type],
                      icon_x, icon_y,
                      icon_color);

    /* Type d'appel (texte) */
    const char* type_str;
    switch (entry->call_type) {
        case CALL_TYPE_INCOMING: type_str = "Appel entrant";  break;
        case CALL_TYPE_OUTGOING: type_str = "Appel sortant";  break;
        case CALL_TYPE_MISSED:   type_str = "Appel manque";   break;
        default:                 type_str = "Inconnu";        break;
    }
    Display_DrawText(icon_x + 50, icon_y, 
                     type_str, 
                     &font_medium, 
                     icon_color,
                     panel_bg);

    /* Nom */
    int16_t info_y = icon_y + 45;
    Display_DrawText(px + 20, info_y, "Contact:", 
                     &font_small, THEME_TEXT_TERTIARY, panel_bg);
    Display_DrawText(px + 20, info_y + 18,
                     entry->display_name,
                     &font_medium,
                     THEME_TEXT_PRIMARY,
                     panel_bg);

    /* Numéro */
    info_y += 45;
    Display_DrawText(px + 20, info_y, "Numero:",
                     &font_small, THEME_TEXT_TERTIARY, panel_bg);
    Display_DrawText(px + 20, info_y + 18,
                     entry->display_number,
                     &font_medium,
                     THEME_TEXT_PRIMARY,
                     panel_bg);

    /* Date et heure */
    info_y += 45;
    char datetime_str[32];
    snprintf(datetime_str, sizeof(datetime_str), "%s a %s",
             entry->date_str, entry->time_str);
    Display_DrawText(px + 20, info_y, "Date:",
                     &font_small, THEME_TEXT_TERTIARY, panel_bg);
    Display_DrawText(px + 20, info_y + 18,
                     datetime_str,
                     &font_medium,
                     THEME_TEXT_PRIMARY,
                     panel_bg);

    /* Durée */
    info_y += 45;
    if (entry->duration_sec > 0) {
        uint16_t minutes = entry->duration_sec / 60;
        uint16_t seconds = entry->duration_sec % 60;
        char duration_str[24];
        snprintf(duration_str, sizeof(duration_str), 
                 "%d min %02d sec", minutes, seconds);

        Display_DrawText(px + 20, info_y, "Duree:",
                         &font_small, THEME_TEXT_TERTIARY, panel_bg);
        Display_DrawText(px + 20, info_y + 18,
                         duration_str,
                         &font_medium,
                         THEME_TEXT_PRIMARY,
                         panel_bg);
    } else if (entry->call_type == CALL_TYPE_MISSED) {
        Display_DrawText(px + 20, info_y, "Duree:",
                         &font_small, THEME_TEXT_TERTIARY, panel_bg);
        Display_DrawText(px + 20, info_y + 18,
                         "Non repondu",
                         &font_medium,
                         THEME_TEXT_DANGER,
                         panel_bg);
    }

    /* Bouton "Fermer" en bas du panneau */
    int16_t btn_x = px + pw / 2 - 50;
    int16_t btn_y = py + ph - 50;
    Display_FillRoundRect(btn_x, btn_y, 100, 35, 8, THEME_BUTTON_SECONDARY);
    Display_DrawText(btn_x + 20, btn_y + 8, 
                     "Fermer", 
                     &font_medium, 
                     THEME_TEXT_PRIMARY,
                     THEME_BUTTON_SECONDARY);
}

/**
 * @brief Callback du timer de rafraîchissement
 */
static void refresh_timer_callback(TimerHandle_t timer)
{
    ScreenCallLog_t* screen = (ScreenCallLog_t*)Timer_GetContext(timer);
    if (!screen || !screen->base.is_visible) return;

    /* Vérifier si de nouvelles entrées sont disponibles */
    uint16_t current_count;
    CallLogService_GetCount(screen->call_log_service, &current_count);

    static uint16_t last_count = 0;
    if (current_count != last_count) {
        DEBUG_VERBOSE(TAG, "Nouvelles entrées détectées (%d -> %d)",
                      last_count, current_count);
        last_count = current_count;
        ScreenCallLog_RefreshList(screen);
    }
}

/**
 * @brief Fait défiler la liste vers le haut
 */
static void scroll_list_up(ScreenCallLog_t* screen)
{
    if (!screen || screen->total_entries == 0) return;

    if (screen->selected_index > 0) {
        screen->selected_index--;
    } else if (screen->scroll_offset > 0) {
        screen->scroll_offset--;
    }

    UIList_ScrollTo(screen->list_widget, 
                    screen->selected_index >= 0 ? 
                    screen->selected_index : 0);
}

/**
 * @brief Fait défiler la liste vers le bas
 */
static void scroll_list_down(ScreenCallLog_t* screen)
{
    if (!screen || screen->total_entries == 0) return;

    int16_t max_visible = CALL_LOG_VISIBLE_ENTRIES;
    int16_t max_scroll = screen->total_entries - max_visible;

    if (screen->selected_index < screen->total_entries - 1) {
        screen->selected_index++;
    } else if (screen->scroll_offset < max_scroll) {
        screen->scroll_offset++;
    }

    UIList_ScrollTo(screen->list_widget,
                    screen->selected_index >= 0 ?
                    screen->selected_index : 
                    screen->scroll_offset + max_visible - 1);
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */