/**
 * @file screen_message_compose.cpp
 * @brief Implémentation de l'écran de composition de message SMS
 * 
 * Fonctionnalités :
 * - Saisie du destinataire (numéro ou choix dans les contacts)
 * - Zone de saisie du message avec compteur de caractères
 * - Bouton Envoyer avec validation
 * - Historique de la conversation (si réponse)
 * - Clavier virtuel pour la saisie
 * - Formatage automatique du numéro
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "screen_message_compose.h"
#include "screen_contacts.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_icons.h"
#include "../ui/ui_fonts.h"
#include "../ui/ui_keyboard.h"
#include "../services/sms_service.h"
#include "../services/contact_service.h"
#include "../services/notification_service.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES
// ============================================================

/** @brief État de l'écran de composition */
static MessageComposeScreenState composeState;

// ============================================================
// CALLBACKS
// ============================================================

/**
 * @brief Callback de création de l'écran
 */
static void compose_on_create(ScreenBase* screen)
{
    COMPOSE_DEBUG("Création de l'écran de composition\n");
    
    memset(&composeState, 0, sizeof(MessageComposeScreenState));
    
    screen_message_compose_init_widgets(screen);
}

/**
 * @brief Callback d'entrée dans l'écran
 */
static void compose_on_enter(ScreenBase* screen)
{
    COMPOSE_DEBUG("Entrée dans l'écran de composition\n");
    
    // Si on a un destinataire (mode réponse), charger l'historique
    if (composeState.recipientNumber[0] != '\0')
    {
        composeState.isReply = true;
        screen_message_compose_load_history(screen);
        
        // Mettre à jour le titre
        screen_set_title(screen, composeState.recipientName);
    }
    else
    {
        screen_set_title(screen, "Nouveau message");
        // Donner le focus au champ destinataire
        if (composeState.recipientInput)
        {
            ui_widget_set_focus((UIWidget*)composeState.recipientInput);
        }
    }
}

/**
 * @brief Callback de pression sur une touche
 */
static void compose_on_key(ScreenBase* screen, KeyCode key, KeyEvent event)
{
    if (event != KEY_EVENT_PRESS) return;
    
    switch (key)
    {
        case KEY_OK:
            // Envoyer le message
            screen_message_compose_send(screen);
            break;
            
        case KEY_BACK:
            // Géré par le comportement par défaut (retour)
            break;
            
        default:
            break;
    }
}

// ============================================================
// CRÉATION DE L'ÉCRAN
// ============================================================

ScreenBase* screen_message_compose_create(const char* recipientNumber)
{
    ScreenBase* screen = screen_create(SCREEN_MESSAGE_COMPOSE_NAME);
    if (screen == NULL) return NULL;
    
    screen->onCreate = compose_on_create;
    screen->onEnter = compose_on_enter;
    screen->onKeyPress = compose_on_key;
    
    // Sauvegarder le destinataire si fourni
    if (recipientNumber)
    {
        strncpy(composeState.recipientNumber, recipientNumber, 15);
        
        // Chercher le nom du contact
        int16_t contactIdx = contact_service_find_by_number(recipientNumber);
        if (contactIdx >= 0)
        {
            Contact* contact = contact_service_get(contactIdx);
            if (contact)
            {
                strncpy(composeState.recipientName, contact->name, 31);
            }
        }
        if (composeState.recipientName[0] == '\0')
        {
            strncpy(composeState.recipientName, recipientNumber, 31);
        }
    }
    
    screen_message_compose_init_widgets(screen);
    
    return screen;
}

// ============================================================
// INITIALISATION DES WIDGETS
// ============================================================

void screen_message_compose_init_widgets(ScreenBase* screen)
{
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    uint16_t yPos = 50;
    
    // --- Champ Destinataire ---
    composeState.recipientInput = ui_textbox_create("recipient",
                                                     UI_RECT(10, yPos, 250, 36));
    ui_textbox_set_style(composeState.recipientInput, TEXTBOX_STYLE_NORMAL);
    ui_textbox_set_placeholder(composeState.recipientInput, "Destinataire");
    ui_textbox_set_phone_only(composeState.recipientInput);
    ui_textbox_set_max_length(composeState.recipientInput, 15);
    
    if (composeState.recipientNumber[0] != '\0')
    {
        ui_textbox_set_text(composeState.recipientInput, composeState.recipientNumber);
        composeState.recipientInput->base.enabled = false;  // Non modifiable en mode réponse
    }
    
    screen_add_widget(screen, (UIWidget*)composeState.recipientInput);
    
    // --- Bouton Contacts ---
    composeState.btnContacts = ui_button_create("btnContacts", "👤",
                                                 UI_RECT(268, yPos, 42, 36));
    ui_button_set_style(composeState.btnContacts, BUTTON_STYLE_SECONDARY);
    composeState.btnContacts->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent)
        {
            // Ouvrir les contacts en mode sélection
            // Le numéro sélectionné sera retourné via le résultat
            ScreenBase* contacts = screen_contacts_create();
            if (contacts)
            {
                contacts->result = SCREEN_RESULT_CUSTOM + 2;  // Mode sélection pour SMS
                ui_push_screen((UIScreen*)contacts);
            }
        }
    };
    screen_add_widget(screen, (UIWidget*)composeState.btnContacts);
    yPos += 44;
    
    // --- Séparateur ---
    yPos += 4;
    
    // --- Historique de la conversation ---
    uint16_t historyHeight = 280;
    composeState.historyList = ui_list_create("history",
                                               UI_RECT(0, yPos, DISPLAY_WIDTH, historyHeight),
                                               COMPOSE_MAX_HISTORY);
    ui_list_set_style(composeState.historyList, LIST_STYLE_PLAIN);
    ui_list_set_item_height(composeState.historyList, 40);
    screen_add_widget(screen, (UIWidget*)composeState.historyList);
    yPos += historyHeight + 8;
    
    // --- Zone de saisie du message ---
    composeState.messageInput = ui_textbox_create("message",
                                                   UI_RECT(10, yPos, 240, 36));
    ui_textbox_set_style(composeState.messageInput, TEXTBOX_STYLE_OUTLINED);
    ui_textbox_set_placeholder(composeState.messageInput, "Message...");
    ui_textbox_set_max_length(composeState.messageInput, COMPOSE_SMS_MAX_LENGTH);
    
    // Callback pour le compteur de caractères
    composeState.messageInput->onTextChanged = [](UITextBox* tb) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_message_compose_update_counter(parent);
    };
    
    screen_add_widget(screen, (UIWidget*)composeState.messageInput);
    
    // --- Compteur de caractères ---
    composeState.charCountLabel = ui_label_create("charCount", "0/160",
                                                    UI_RECT(258, yPos, 52, 36));
    ui_label_set_style(composeState.charCountLabel, LABEL_STYLE_CAPTION);
    ui_label_set_alignment(composeState.charCountLabel, UI_ALIGN_RIGHT);
    screen_add_widget(screen, (UIWidget*)composeState.charCountLabel);
    yPos += 44;
    
    // --- Bouton Envoyer ---
    composeState.btnSend = ui_button_create("btnSend", "📤 Envoyer",
                                             UI_RECT(20, yPos, 280, 44));
    ui_button_set_style(composeState.btnSend, BUTTON_STYLE_PRIMARY);
    composeState.btnSend->onClick = [](UIButton* btn) {
        ScreenBase* parent = (ScreenBase*)ui_get_active_screen();
        if (parent) screen_message_compose_send(parent);
    };
    screen_add_widget(screen, (UIWidget*)composeState.btnSend);
    
    // --- Clavier virtuel ---
    UIKeyboard* keyboard = ui_keyboard_create_full_width("keyboard", yPos + 50);
    keyboard->base.visible = false;  // Caché par défaut, affiché au focus
    ui_keyboard_set_target(keyboard, composeState.messageInput);
    screen_add_widget(screen, (UIWidget*)keyboard);
    
    COMPOSE_DEBUG("Widgets de l'écran de composition initialisés (%d widgets)\n", screen->widgetCount);
}

// ============================================================
// FONCTIONS D'ACTION
// ============================================================

void screen_message_compose_send(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    // Récupérer le destinataire
    const char* recipient;
    if (composeState.recipientInput->base.enabled)
    {
        recipient = ui_textbox_get_text(composeState.recipientInput);
    }
    else
    {
        recipient = composeState.recipientNumber;
    }
    
    // Récupérer le message
    const char* message = ui_textbox_get_text(composeState.messageInput);
    
    // Vérifications
    if (recipient == NULL || strlen(recipient) == 0)
    {
        COMPOSE_DEBUG("Destinataire vide\n");
        return;
    }
    
    if (message == NULL || strlen(message) == 0)
    {
        COMPOSE_DEBUG("Message vide\n");
        return;
    }
    
    COMPOSE_DEBUG("Envoi SMS à %s : %s\n", recipient, message);
    
    // Envoyer le SMS
    if (sms_service_send(recipient, message))
    {
        // Effacer la zone de saisie
        ui_textbox_clear(composeState.messageInput);
        composeState.charCount = 0;
        screen_message_compose_update_counter(screen);
        
        // Recharger l'historique
        screen_message_compose_load_history(screen);
        
        // Notification de succès
        notification_system_info("Message envoyé");
    }
    else
    {
        COMPOSE_DEBUG("Échec de l'envoi\n");
        notification_system_error("Échec de l'envoi du message");
    }
}

void screen_message_compose_update_counter(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    const char* text = ui_textbox_get_text(composeState.messageInput);
    composeState.charCount = text ? strlen(text) : 0;
    
    char counterStr[16];
    snprintf(counterStr, sizeof(counterStr), "%d/%d", 
             composeState.charCount, COMPOSE_SMS_MAX_LENGTH);
    
    if (composeState.charCountLabel)
    {
        ui_label_set_text(composeState.charCountLabel, counterStr);
        
        // Changer la couleur si proche de la limite
        if (composeState.charCount > COMPOSE_SMS_MAX_LENGTH - 20)
        {
            ui_label_set_color(composeState.charCountLabel, 
                              ui_theme_get_active()->colors.error);
        }
        else if (composeState.charCount > COMPOSE_SMS_MAX_LENGTH - 50)
        {
            ui_label_set_color(composeState.charCountLabel, 
                              ui_theme_get_active()->colors.warning);
        }
        else
        {
            ui_label_set_color(composeState.charCountLabel, 
                              ui_theme_get_active()->colors.textSecondary);
        }
    }
}

void screen_message_compose_load_history(ScreenBase* screen)
{
    if (screen == NULL || composeState.recipientNumber[0] == '\0') return;
    
    // Vider la liste
    ui_list_clear(composeState.historyList);
    
    // Charger la conversation depuis le service SMS
    SMSConversation* conv = sms_service_get_conversation(composeState.recipientNumber);
    
    if (conv == NULL || conv->messageCount == 0)
    {
        composeState.historyCount = 0;
        return;
    }
    
    composeState.historyCount = conv->messageCount;
    
    // Afficher les messages (du plus ancien au plus récent ? ou l'inverse ?)
    // On affiche les 20 derniers messages
    uint16_t startIdx = (conv->messageCount > COMPOSE_MAX_HISTORY) ? 
                         (conv->messageCount - COMPOSE_MAX_HISTORY) : 0;
    
    for (uint16_t i = startIdx; i < conv->messageCount; i++)
    {
        SMSMessage* msg = &conv->messages[i];
        
        // Format : direction + texte
        char displayText[128];
        const char* direction = msg->isIncoming ? "← " : "→ ";
        
        snprintf(displayText, sizeof(displayText), "%s%s", direction, msg->message);
        
        // Sous-texte : heure
        uint32_t msgTime = msg->timestamp / 1000;
        uint32_t hours = (msgTime / 3600) % 24;
        uint32_t minutes = (msgTime % 3600) / 60;
        
        char subtext[32];
        snprintf(subtext, sizeof(subtext), "%02lu:%02lu", (unsigned long)hours, (unsigned long)minutes);
        
        ui_list_add_item(composeState.historyList, displayText, subtext, NULL);
    }
    
    // Défiler jusqu'au dernier message
    if (composeState.historyList->itemCount > 0)
    {
        ui_list_scroll_to_bottom(composeState.historyList);
    }
    
    COMPOSE_DEBUG("Historique chargé : %d messages\n", composeState.historyCount);
}