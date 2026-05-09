/**
 * @file screen_contact_detail.cpp
 * @brief Implémentation de l'écran de détail d'un contact
 * 
 * Fonctionnalités :
 * - Affichage des informations du contact
 * - Boutons d'action (Appeler, SMS, Favori, Modifier, Supprimer)
 * - Historique des appels et des SMS avec ce contact
 * - Mode création et mode édition
 * - Validation des champs
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "screen_contact_detail.h"
#include "screen_contacts_list.h"
#include "screen_dialer.h"
#include "screen_message_compose.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_icons.h"
#include "../ui/ui_fonts.h"
#include "../ui/ui_dialog.h"
#include "../services/contact_service.h"
#include "../services/phone_service.h"
#include "../services/sms_service.h"
#include "../services/call_log_service.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES
// ============================================================

/** @brief État de l'écran de détail */
static ContactDetailScreenState detailState;

// ============================================================
// CALLBACKS
// ============================================================

/**
 * @brief Callback de création de l'écran
 */
static void detail_on_create(ScreenBase* screen)
{
    DETAIL_DEBUG("Création de l'écran de détail\n");
    
    memset(&detailState, 0, sizeof(ContactDetailScreenState));
    
    screen_contact_detail_init_widgets(screen);
}

/**
 * @brief Callback d'entrée dans l'écran
 */
static void detail_on_enter(ScreenBase* screen)
{
    DETAIL_DEBUG("Entrée dans l'écran de détail\n");
    
    // Charger les données du contact
    screen_contact_detail_load_contact(screen);
}

// ============================================================
// CRÉATION DE L'ÉCRAN
// ============================================================

ScreenBase* screen_contact_detail_create(int16_t contactIndex)
{
    ScreenBase* screen = screen_create(SCREEN_CONTACT_DETAIL_NAME);
    if (screen == NULL) return NULL;
    
    screen->onCreate = detail_on_create;
    screen->onEnter = detail_on_enter;
    
    detailState.contactIndex = contactIndex;
    detailState.isNewContact = (contactIndex < 0);
    detailState.isEditing = detailState.isNewContact;
    
    if (detailState.isNewContact)
    {
        strncpy(screen->title, "Nouveau contact", 63);
    }
    else
    {
        strncpy(screen->title, "Contact", 63);
    }
    
    screen_contact_detail_init_widgets(screen);
    
    return screen;
}

// ============================================================
// INITIALISATION DES WIDGETS
// ============================================================

void screen_contact_detail_init_widgets(ScreenBase* screen)
{
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    uint16_t yPos = 60;
    
    // --- Nom du contact ---
    detailState.nameLabel = ui_label_create("name", "",
                                             UI_RECT(20, yPos, 280, 36));
    ui_label_set_style(detailState.nameLabel, LABEL_STYLE_TITLE);
    ui_label_set_alignment(detailState.nameLabel, UI_ALIGN_CENTER);
    detailState.nameLabel->base.rect.height = 36;
    screen_add_widget(screen, (UIWidget*)detailState.nameLabel);
    yPos += 42;
    
    // --- Numéro de téléphone ---
    detailState.numberLabel = ui_label_create("number", "",
                                               UI_RECT(20, yPos, 280, 24));
    ui_label_set_style(detailState.numberLabel, LABEL_STYLE_SUBTITLE);
    ui_label_set_alignment(detailState.numberLabel, UI_ALIGN_CENTER);
    screen_add_widget(screen, (UIWidget*)detailState.numberLabel);
    yPos += 36;
    
    // --- Boutons d'action ---
    uint16_t btnY = yPos;
    uint16_t btnW = 90;
    uint16_t btnH = 44;
    uint16_t spacing = 10;
    uint16_t totalWidth = 3 * btnW + 2 * spacing;
    uint16_t startX = (DISPLAY_WIDTH - totalWidth) / 2;
    
    // Bouton Appeler
    detailState.btnCall = ui_button_create("btnCall", "📞 Appeler",
                                            UI_RECT(startX, btnY, btnW, btnH));
    ui_button_set_style(detailState.btnCall, BUTTON_STYLE_CALL);
    detailState.btnCall->base.rect = (UIRect){startX, btnY, btnW, btnH};
    detailState.btnCall->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_contact_detail_call(parent);
    };
    screen_add_widget(screen, (UIWidget*)detailState.btnCall);
    
    // Bouton SMS
    detailState.btnSMS = ui_button_create("btnSMS", "💬 SMS",
                                           UI_RECT(startX + btnW + spacing, btnY, btnW, btnH));
    ui_button_set_style(detailState.btnSMS, BUTTON_STYLE_PRIMARY);
    detailState.btnSMS->base.rect = (UIRect){startX + btnW + spacing, btnY, btnW, btnH};
    detailState.btnSMS->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_contact_detail_sms(parent);
    };
    screen_add_widget(screen, (UIWidget*)detailState.btnSMS);
    
    // Bouton Favori
    detailState.btnFavorite = ui_button_create("btnFav", "⭐",
                                                UI_RECT(startX + 2*(btnW + spacing), btnY, btnW, btnH));
    ui_button_set_style(detailState.btnFavorite, BUTTON_STYLE_SECONDARY);
    detailState.btnFavorite->base.rect = (UIRect){startX + 2*(btnW + spacing), btnY, btnW, btnH};
    detailState.btnFavorite->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_contact_detail_toggle_favorite(parent);
    };
    screen_add_widget(screen, (UIWidget*)detailState.btnFavorite);
    
    yPos = btnY + btnH + 16;
    
    // --- Séparateur ---
    yPos += 8;
    
    // --- Informations supplémentaires ---
    // Email
    detailState.emailLabel = ui_label_create("email", "",
                                              UI_RECT(20, yPos, 280, 20));
    ui_label_set_style(detailState.emailLabel, LABEL_STYLE_BODY);
    ui_label_set_color(detailState.emailLabel, colors->textSecondary);
    screen_add_widget(screen, (UIWidget*)detailState.emailLabel);
    yPos += 22;
    
    // Adresse
    detailState.addressLabel = ui_label_create("address", "",
                                                UI_RECT(20, yPos, 280, 20));
    ui_label_set_style(detailState.addressLabel, LABEL_STYLE_BODY);
    ui_label_set_color(detailState.addressLabel, colors->textSecondary);
    screen_add_widget(screen, (UIWidget*)detailState.addressLabel);
    yPos += 22;
    
    // Notes
    detailState.notesLabel = ui_label_create("notes", "",
                                              UI_RECT(20, yPos, 280, 20));
    ui_label_set_style(detailState.notesLabel, LABEL_STYLE_BODY);
    ui_label_set_color(detailState.notesLabel, colors->textSecondary);
    screen_add_widget(screen, (UIWidget*)detailState.notesLabel);
    yPos += 28;
    
    // --- Historique des appels ---
    uint16_t historyHeight = 60;
    detailState.callHistoryList = ui_list_create("callHistory",
                                                   UI_RECT(0, yPos, DISPLAY_WIDTH, historyHeight),
                                                   CONTACT_DETAIL_MAX_HISTORY);
    ui_list_set_style(detailState.callHistoryList, LIST_STYLE_COMPACT);
    ui_list_set_item_height(detailState.callHistoryList, 24);
    screen_add_widget(screen, (UIWidget*)detailState.callHistoryList);
    yPos += historyHeight + 8;
    
    // --- Historique des SMS ---
    detailState.smsHistoryList = ui_list_create("smsHistory",
                                                  UI_RECT(0, yPos, DISPLAY_WIDTH, historyHeight),
                                                  CONTACT_DETAIL_MAX_HISTORY);
    ui_list_set_style(detailState.smsHistoryList, LIST_STYLE_COMPACT);
    ui_list_set_item_height(detailState.smsHistoryList, 24);
    screen_add_widget(screen, (UIWidget*)detailState.smsHistoryList);
    yPos += historyHeight + 16;
    
    // --- Boutons Modifier / Supprimer ---
    if (!detailState.isNewContact)
    {
        // Bouton Modifier
        detailState.btnEdit = ui_button_create("btnEdit", "✏️ Modifier",
                                                UI_RECT(20, yPos, 130, 40));
        ui_button_set_style(detailState.btnEdit, BUTTON_STYLE_PRIMARY);
        detailState.btnEdit->onClick = [](UIButton* btn) {
            ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
            if (parent) screen_contact_detail_edit(parent);
        };
        screen_add_widget(screen, (UIWidget*)detailState.btnEdit);
        
        // Bouton Supprimer
        detailState.btnDelete = ui_button_create("btnDelete", "🗑️ Supprimer",
                                                   UI_RECT(170, yPos, 130, 40));
        ui_button_set_style(detailState.btnDelete, BUTTON_STYLE_DANGER);
        detailState.btnDelete->onClick = [](UIButton* btn) {
            ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
            if (parent) screen_contact_detail_delete(parent);
        };
        screen_add_widget(screen, (UIWidget*)detailState.btnDelete);
    }
    else
    {
        // Bouton Enregistrer (nouveau contact)
        detailState.btnEdit = ui_button_create("btnSave", "💾 Enregistrer",
                                                UI_RECT(60, yPos, 200, 44));
        ui_button_set_style(detailState.btnEdit, BUTTON_STYLE_PRIMARY);
        detailState.btnEdit->onClick = [](UIButton* btn) {
            ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
            if (parent) screen_contact_detail_save(parent);
        };
        screen_add_widget(screen, (UIWidget*)detailState.btnEdit);
    }
    
    DETAIL_DEBUG("Widgets de l'écran de détail initialisés (%d widgets)\n", screen->widgetCount);
}

// ============================================================
// CHARGEMENT DU CONTACT
// ============================================================

void screen_contact_detail_load_contact(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    if (detailState.isNewContact)
    {
        // Nouveau contact : champs vides
        if (detailState.nameLabel) ui_label_set_text(detailState.nameLabel, "Nouveau contact");
        if (detailState.numberLabel) ui_label_set_text(detailState.numberLabel, "");
        return;
    }
    
    // Charger le contact
    Contact* contact = contact_service_get(detailState.contactIndex);
    if (contact == NULL) return;
    
    // Copier les données
    memcpy(&detailState.contactData, contact, sizeof(Contact));
    
    // Mettre à jour les labels
    if (detailState.nameLabel)
        ui_label_set_text(detailState.nameLabel, contact->name);
    
    if (detailState.numberLabel)
    {
        char formattedNumber[32];
        format_phone_number_display(contact->number, formattedNumber, sizeof(formattedNumber));
        ui_label_set_text(detailState.numberLabel, formattedNumber);
    }
    
    if (detailState.emailLabel)
    {
        char emailStr[64];
        if (contact->email[0])
            snprintf(emailStr, sizeof(emailStr), "📧 %s", contact->email);
        else
            strcpy(emailStr, "📧 Aucun email");
        ui_label_set_text(detailState.emailLabel, emailStr);
    }
    
    if (detailState.addressLabel)
    {
        char addrStr[80];
        if (contact->address[0])
            snprintf(addrStr, sizeof(addrStr), "📍 %s", contact->address);
        else
            strcpy(addrStr, "📍 Aucune adresse");
        ui_label_set_text(detailState.addressLabel, addrStr);
    }
    
    if (detailState.notesLabel)
    {
        char notesStr[80];
        if (contact->notes[0])
            snprintf(notesStr, sizeof(notesStr), "📝 %s", contact->notes);
        else
            strcpy(notesStr, "📝 Aucune note");
        ui_label_set_text(detailState.notesLabel, notesStr);
    }
    
    // Mettre à jour le bouton favori
    if (detailState.btnFavorite)
    {
        ui_button_set_text(detailState.btnFavorite, contact->favorite ? "⭐" : "☆");
    }
    
    // Charger l'historique des appels
    load_call_history(screen, contact->number);
    
    // Charger l'historique des SMS
    load_sms_history(screen, contact->number);
    
    DETAIL_DEBUG("Contact chargé : %s\n", contact->name);
}

// ============================================================
// ACTIONS
// ============================================================

void screen_contact_detail_call(ScreenBase* screen)
{
    if (screen == NULL) return;
    if (detailState.contactData.number[0] == '\0') return;
    
    DETAIL_DEBUG("Appel de %s\n", detailState.contactData.number);
    phone_service_dial(detailState.contactData.number);
}

void screen_contact_detail_sms(ScreenBase* screen)
{
    if (screen == NULL) return;
    if (detailState.contactData.number[0] == '\0') return;
    
    DETAIL_DEBUG("SMS à %s\n", detailState.contactData.number);
    
    ScreenBase* compose = screen_message_compose_create(detailState.contactData.number);
    if (compose)
    {
        screen_set_transition(compose, SCREEN_TRANSITION_SLIDE_LEFT,
                              SCREEN_TRANSITION_SLIDE_RIGHT, 250);
        ui_push_screen((UIScreen*)compose);
    }
}

void screen_contact_detail_toggle_favorite(ScreenBase* screen)
{
    if (screen == NULL || detailState.isNewContact) return;
    
    contact_service_toggle_favorite(detailState.contactIndex);
    detailState.contactData.favorite = !detailState.contactData.favorite;
    
    if (detailState.btnFavorite)
    {
        ui_button_set_text(detailState.btnFavorite, 
                          detailState.contactData.favorite ? "⭐" : "☆");
    }
    
    DETAIL_DEBUG("Favori : %s\n", detailState.contactData.favorite ? "ON" : "OFF");
}

void screen_contact_detail_edit(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    detailState.isEditing = true;
    
    // TODO: Passer les labels en mode édition (TextBox)
    // Pour l'instant, on ouvre une boîte de dialogue de saisie
    
    DETAIL_DEBUG("Mode édition activé\n");
}

void screen_contact_detail_save(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    if (detailState.isNewContact)
    {
        // Créer un nouveau contact
        if (detailState.contactData.name[0] == '\0' || detailState.contactData.number[0] == '\0')
        {
            DETAIL_DEBUG("Nom ou numéro manquant\n");
            return;
        }
        
        contact_service_add(detailState.contactData.name, detailState.contactData.number);
        DETAIL_DEBUG("Contact créé : %s\n", detailState.contactData.name);
    }
    else
    {
        // Mettre à jour le contact existant
        contact_service_update(detailState.contactIndex, &detailState.contactData);
        DETAIL_DEBUG("Contact mis à jour : %s\n", detailState.contactData.name);
    }
    
    detailState.isEditing = false;
    
    // Revenir à la liste des contacts
    ui_pop_screen();
}

void screen_contact_detail_delete(ScreenBase* screen)
{
    if (screen == NULL || detailState.isNewContact) return;
    
    // Demander confirmation
    UIDialog* confirm = UI_DIALOG_CONFIRM("Supprimer", "Voulez-vous vraiment supprimer ce contact ?");
    confirm->onResult = [](UIDialog* d, DialogResult r) {
        if (r == DIALOG_RESULT_OK)
        {
            contact_service_delete(detailState.contactIndex);
            DETAIL_DEBUG("Contact supprimé\n");
            ui_pop_screen();  // Revenir à la liste
        }
    };
    ui_dialog_show(confirm);
}

// ============================================================
// FONCTIONS INTERNES
// ============================================================

static void load_call_history(ScreenBase* screen, const char* number)
{
    if (screen == NULL || number == NULL) return;
    
    ui_list_clear(detailState.callHistoryList);
    
    CallLogEntry entries[CONTACT_DETAIL_MAX_HISTORY];
    uint16_t count = call_log_find_by_number(number, entries, CONTACT_DETAIL_MAX_HISTORY);
    
    if (count == 0)
    {
        ui_list_add_item(detailState.callHistoryList, "Aucun appel récent", "", NULL);
        return;
    }
    
    for (uint16_t i = 0; i < count && i < CONTACT_DETAIL_MAX_HISTORY; i++)
    {
        const char* direction = (entries[i].type == CALL_LOG_TYPE_INCOMING) ? "←" : "→";
        char text[64];
        
        uint32_t durMin = entries[i].duration / 60;
        uint32_t durSec = entries[i].duration % 60;
        
        snprintf(text, sizeof(text), "%s %02lu:%02lu",
                 direction, (unsigned long)durMin, (unsigned long)durSec);
        
        ui_list_add_item(detailState.callHistoryList, text, "", NULL);
    }
}

static void load_sms_history(ScreenBase* screen, const char* number)
{
    if (screen == NULL || number == NULL) return;
    
    ui_list_clear(detailState.smsHistoryList);
    
    SMSConversation* conv = sms_service_get_conversation(number);
    
    if (conv == NULL || conv->messageCount == 0)
    {
        ui_list_add_item(detailState.smsHistoryList, "Aucun message récent", "", NULL);
        return;
    }
    
    uint16_t startIdx = (conv->messageCount > CONTACT_DETAIL_MAX_HISTORY) ?
                         (conv->messageCount - CONTACT_DETAIL_MAX_HISTORY) : 0;
    
    for (uint16_t i = startIdx; i < conv->messageCount; i++)
    {
        const char* direction = conv->messages[i].isIncoming ? "←" : "→";
        char text[128];
        snprintf(text, sizeof(text), "%s %s", direction, conv->messages[i].message);
        ui_list_add_item(detailState.smsHistoryList, text, "", NULL);
    }
}

static void format_phone_number_display(const char* raw, char* formatted, uint16_t maxLen)
{
    if (raw == NULL || formatted == NULL) return;
    
    uint16_t len = strlen(raw);
    uint16_t outPos = 0;
    
    for (uint16_t i = 0; i < len && outPos < maxLen - 1; i++)
    {
        formatted[outPos++] = raw[i];
        if ((i + 1) % 2 == 0 && i < len - 1 && outPos < maxLen - 1)
        {
            formatted[outPos++] = ' ';
        }
    }
    formatted[outPos] = '\0';
}