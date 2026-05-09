/**
 * @file screen_contacts_list.cpp
 * @brief Implémentation de l'écran de liste des contacts
 * 
 * Fonctionnalités :
 * - Affichage de la liste alphabétique des contacts
 * - Barre de recherche avec filtrage
 * - Favoris en tête de liste
 * - Ajout de nouveau contact
 * - Actions rapides (appeler, SMS)
 * - Modes : navigation, sélection, choix unique
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "screen_contacts_list.h"
#include "screen_contact_detail.h"
#include "screen_dialer.h"
#include "screen_message_compose.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_icons.h"
#include "../ui/ui_fonts.h"
#include "../services/contact_service.h"
#include "../services/phone_service.h"
#include "../services/sms_service.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// ============================================================
// VARIABLES
// ============================================================

/** @brief État de l'écran de contacts */
static ContactsListScreenState contactsState;

// ============================================================
// CALLBACKS
// ============================================================

/**
 * @brief Callback de création de l'écran
 */
static void contacts_on_create(ScreenBase* screen)
{
    CONTACTS_DEBUG("Création de l'écran de contacts\n");
    
    memset(&contactsState, 0, sizeof(ContactsListScreenState));
    contactsState.mode = CONTACTS_MODE_BROWSE;
    contactsState.showFavoritesFirst = true;
    
    screen_contacts_list_init_widgets(screen);
}

/**
 * @brief Callback d'entrée dans l'écran
 */
static void contacts_on_enter(ScreenBase* screen)
{
    CONTACTS_DEBUG("Entrée dans l'écran de contacts\n");
    
    // Rafraîchir la liste
    screen_contacts_list_refresh(screen);
}

// ============================================================
// CRÉATION DE L'ÉCRAN
// ============================================================

ScreenBase* screen_contacts_list_create(void)
{
    return screen_contacts_list_create_mode(CONTACTS_MODE_BROWSE);
}

ScreenBase* screen_contacts_list_create_mode(ContactsMode mode)
{
    ScreenBase* screen = screen_create(SCREEN_CONTACTS_LIST_NAME);
    if (screen == NULL) return NULL;
    
    screen->onCreate = contacts_on_create;
    screen->onEnter = contacts_on_enter;
    
    contactsState.mode = mode;
    
    // Titre selon le mode
    switch (mode)
    {
        case CONTACTS_MODE_SELECT:
            strncpy(screen->title, "Choisir un contact", 63);
            break;
        case CONTACTS_MODE_PICK:
            strncpy(screen->title, "Sélectionner", 63);
            break;
        default:
            strncpy(screen->title, "Contacts", 63);
            break;
    }
    
    screen_contacts_list_init_widgets(screen);
    
    return screen;
}

// ============================================================
// INITIALISATION DES WIDGETS
// ============================================================

void screen_contacts_list_init_widgets(ScreenBase* screen)
{
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    // --- Champ de recherche ---
    uint16_t searchY = 48;
    contactsState.searchInput = ui_textbox_create("search",
                                                    UI_RECT(10, searchY, 260, 36));
    ui_textbox_set_style(contactsState.searchInput, TEXTBOX_STYLE_NORMAL);
    ui_textbox_set_placeholder(contactsState.searchInput, "🔍 Rechercher...");
    
    // Callback de recherche en temps réel
    contactsState.searchInput->onTextChanged = [](UITextBox* tb) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent)
        {
            const char* query = ui_textbox_get_text(tb);
            screen_contacts_list_filter(parent, query);
        }
    };
    
    screen_add_widget(screen, (UIWidget*)contactsState.searchInput);
    
    // --- Bouton Ajouter ---
    if (contactsState.mode == CONTACTS_MODE_BROWSE)
    {
        contactsState.btnAdd = ui_button_create("btnAdd", "＋",
                                                 UI_RECT(278, searchY, 34, 36));
        ui_button_set_style(contactsState.btnAdd, BUTTON_STYLE_PRIMARY);
        contactsState.btnAdd->onClick = [](UIButton* btn) {
            // Ouvrir l'écran de détail en mode création
            ScreenBase* detail = screen_contact_detail_create(-1);  // -1 = nouveau
            if (detail) ui_push_screen((UIScreen*)detail);
        };
        screen_add_widget(screen, (UIWidget*)contactsState.btnAdd);
    }
    
    // --- Liste des contacts ---
    uint16_t listY = searchY + 44;
    uint16_t listH = DISPLAY_HEIGHT - listY;
    
    contactsState.contactsList = ui_list_create("contacts",
                                                  UI_RECT(0, listY, DISPLAY_WIDTH, listH),
                                                  CONTACTS_LIST_MAX_VISIBLE);
    ui_list_set_style(contactsState.contactsList, LIST_STYLE_PLAIN);
    ui_list_set_item_height(contactsState.contactsList, CONTACTS_LIST_ITEM_HEIGHT);
    
    // Callback de sélection
    contactsState.contactsList->onSelect = [](UIList* list, int16_t index) {
        if (index >= 0)
        {
            UIListItem* item = ui_list_get_item(list, index);
            if (item && item->userData)
            {
                uint16_t contactIndex = (uint16_t)(uintptr_t)item->userData;
                ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
                
                switch (contactsState.mode)
                {
                    case CONTACTS_MODE_BROWSE:
                        // Ouvrir le détail du contact
                        screen_contacts_list_open_contact(parent, contactIndex);
                        break;
                        
                    case CONTACTS_MODE_SELECT:
                    case CONTACTS_MODE_PICK:
                        // Retourner le contact sélectionné
                        if (contactsState.onContactSelected)
                        {
                            contactsState.onContactSelected(contactIndex);
                        }
                        ui_pop_screen();
                        break;
                }
            }
        }
    };
    
    screen_add_widget(screen, (UIWidget*)contactsState.contactsList);
    
    CONTACTS_DEBUG("Widgets de l'écran de contacts initialisés (%d widgets)\n", screen->widgetCount);
}

// ============================================================
// RAFRAÎCHISSEMENT
// ============================================================

void screen_contacts_list_refresh(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    // Vider la liste
    ui_list_clear(contactsState.contactsList);
    
    // Récupérer les contacts
    Contact* contacts = contact_service_get_all(&contactsState.contactCount);
    
    if (contacts == NULL || contactsState.contactCount == 0)
    {
        ui_list_add_item(contactsState.contactsList,
                        "Aucun contact",
                        "Appuyez sur ＋ pour ajouter un contact",
                        NULL);
        return;
    }
    
    // Ajouter les contacts à la liste
    char lastInitial = '\0';
    
    for (uint16_t i = 0; i < contactsState.contactCount; i++)
    {
        Contact* contact = &contacts[i];
        
        // Obtenir l'initiale pour le regroupement
        char initial = toupper(contact->name[0]);
        if (initial < 'A' || initial > 'Z') initial = '#';
        
        // Ajouter un séparateur si nouvelle initiale
        if (initial != lastInitial && contactsState.searchQuery[0] == '\0')
        {
            char header[4];
            snprintf(header, sizeof(header), "%c", initial);
            
            // Ajouter un élément non sélectionnable comme en-tête
            ui_list_add_item(contactsState.contactsList, header, "", NULL);
            lastInitial = initial;
        }
        
        // Texte principal : nom du contact
        char displayName[128];
        if (contact->favorite)
        {
            snprintf(displayName, sizeof(displayName), "⭐ %s", contact->name);
        }
        else
        {
            strncpy(displayName, contact->name, 127);
        }
        
        // Sous-texte : numéro de téléphone
        // Stocker l'index du contact dans userData pour le retrouver
        ui_list_add_item(contactsState.contactsList,
                        displayName,
                        contact->number,
                        (void*)(uintptr_t)i);
    }
    
    CONTACTS_DEBUG("Liste rafraîchie : %d contacts\n", contactsState.contactCount);
}

void screen_contacts_list_filter(ScreenBase* screen, const char* query)
{
    if (screen == NULL) return;
    
    strncpy(contactsState.searchQuery, query ? query : "", 31);
    
    // Vider la liste
    ui_list_clear(contactsState.contactsList);
    
    // Si pas de recherche, tout afficher
    if (query == NULL || query[0] == '\0')
    {
        screen_contacts_list_refresh(screen);
        return;
    }
    
    // Chercher les contacts correspondants
    uint16_t results[50];
    uint16_t found = contact_service_search(query, results, 50);
    
    if (found == 0)
    {
        ui_list_add_item(contactsState.contactsList,
                        "Aucun résultat",
                        "Essayez un autre nom ou numéro",
                        NULL);
        return;
    }
    
    contactsState.filteredCount = found;
    
    for (uint16_t i = 0; i < found; i++)
    {
        Contact* contact = contact_service_get(results[i]);
        if (contact)
        {
            char displayName[128];
            if (contact->favorite)
                snprintf(displayName, sizeof(displayName), "⭐ %s", contact->name);
            else
                strncpy(displayName, contact->name, 127);
            
            ui_list_add_item(contactsState.contactsList,
                            displayName,
                            contact->number,
                            (void*)(uintptr_t)results[i]);
        }
    }
    
    CONTACTS_DEBUG("Filtre '%s' : %d résultats\n", query, found);
}

// ============================================================
// ACTIONS
// ============================================================

void screen_contacts_list_open_contact(ScreenBase* screen, uint16_t index)
{
    if (screen == NULL) return;
    
    ScreenBase* detail = screen_contact_detail_create(index);
    if (detail)
    {
        screen_set_transition(detail, SCREEN_TRANSITION_SLIDE_LEFT,
                              SCREEN_TRANSITION_SLIDE_RIGHT, 250);
        ui_push_screen((UIScreen*)detail);
    }
}

void screen_contacts_list_call_contact(ScreenBase* screen, uint16_t index)
{
    if (screen == NULL) return;
    
    Contact* contact = contact_service_get(index);
    if (contact)
    {
        CONTACTS_DEBUG("Appel de %s (%s)\n", contact->name, contact->number);
        phone_service_dial(contact->number);
    }
}

void screen_contacts_list_sms_contact(ScreenBase* screen, uint16_t index)
{
    if (screen == NULL) return;
    
    Contact* contact = contact_service_get(index);
    if (contact)
    {
        CONTACTS_DEBUG("SMS à %s (%s)\n", contact->name, contact->number);
        
        ScreenBase* compose = screen_message_compose_create(contact->number);
        if (compose)
        {
            screen_set_transition(compose, SCREEN_TRANSITION_SLIDE_LEFT,
                                  SCREEN_TRANSITION_SLIDE_RIGHT, 250);
            ui_push_screen((UIScreen*)compose);
        }
    }
}