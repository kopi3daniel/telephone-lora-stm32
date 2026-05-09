/**
 * @file screen_messages_list.cpp
 * @brief Implémentation de l'écran de liste des conversations SMS
 * 
 * Fonctionnalités :
 * - Liste des conversations avec aperçu du dernier message
 * - Badge de messages non lus
 * - Bouton Nouveau message
 * - Rafraîchissement automatique
 * - Tri par date (plus récent en premier)
 * - Appui pour ouvrir une conversation
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "screen_messages_list.h"
#include "screen_message_compose.h"
#include "../ui/ui_theme.h"
#include "../ui/ui_icons.h"
#include "../ui/ui_fonts.h"
#include "../services/sms_service.h"
#include "../services/contact_service.h"
#include "../services/notification_service.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES
// ============================================================

/** @brief État de l'écran de messages */
static MessagesListScreenState messagesState;

// ============================================================
// CALLBACKS
// ============================================================

/**
 * @brief Callback de création de l'écran
 */
static void messages_on_create(ScreenBase* screen)
{
    MESSAGES_DEBUG("Création de l'écran de messages\n");
    
    memset(&messagesState, 0, sizeof(MessagesListScreenState));
    
    screen_messages_list_init_widgets(screen);
}

/**
 * @brief Callback d'entrée dans l'écran
 */
static void messages_on_enter(ScreenBase* screen)
{
    MESSAGES_DEBUG("Entrée dans l'écran de messages\n");
    
    // Rafraîchir la liste
    screen_messages_list_refresh(screen);
    
    // Marquer tous les messages comme lus
    sms_service_mark_all_read();
    notification_clear_all();
}

/**
 * @brief Callback de mise à jour périodique
 */
static void messages_on_update(ScreenBase* screen)
{
    // Rafraîchir périodiquement (toutes les 5 secondes)
    static uint32_t lastRefresh = 0;
    uint32_t now = HAL_GetTick();
    
    if (now - lastRefresh >= 5000)
    {
        lastRefresh = now;
        screen_messages_list_refresh(screen);
    }
}

// ============================================================
// CRÉATION DE L'ÉCRAN
// ============================================================

ScreenBase* screen_messages_list_create(void)
{
    ScreenBase* screen = screen_create(SCREEN_MESSAGES_LIST_NAME);
    if (screen == NULL) return NULL;
    
    screen->onCreate = messages_on_create;
    screen->onEnter = messages_on_enter;
    screen->onUpdate = messages_on_update;
    
    strncpy(screen->title, "Messages", 63);
    
    screen_messages_list_init_widgets(screen);
    
    return screen;
}

// ============================================================
// INITIALISATION DES WIDGETS
// ============================================================

void screen_messages_list_init_widgets(ScreenBase* screen)
{
    UIThemeColors* colors = &ui_theme_get_active()->colors;
    
    // --- Liste des conversations ---
    uint16_t listY = 44;  // En dessous de la barre de titre
    uint16_t listH = DISPLAY_HEIGHT - listY - 56;  // Laisser place au bouton du bas
    
    messagesState.conversationsList = ui_list_create("conversations",
                                                       UI_RECT(0, listY, DISPLAY_WIDTH, listH),
                                                       50);
    ui_list_set_style(messagesState.conversationsList, LIST_STYLE_PLAIN);
    ui_list_set_item_height(messagesState.conversationsList, MESSAGES_LIST_ITEM_HEIGHT);
    
    // Callback de sélection
    messagesState.conversationsList->onSelect = [](UIList* list, int16_t index) {
        if (index >= 0)
        {
            UIListItem* item = ui_list_get_item(list, index);
            if (item && item->userData)
            {
                const char* phoneNumber = (const char*)item->userData;
                MESSAGES_DEBUG("Ouverture conversation : %s\n", phoneNumber);
                
                // Ouvrir l'écran de conversation
                ScreenBase* compose = screen_message_compose_create(phoneNumber);
                if (compose)
                {
                    screen_set_transition(compose, SCREEN_TRANSITION_SLIDE_LEFT,
                                          SCREEN_TRANSITION_SLIDE_RIGHT, 250);
                    ui_push_screen((UIScreen*)compose);
                }
            }
        }
    };
    
    screen_add_widget(screen, (UIWidget*)messagesState.conversationsList);
    
    // --- Bouton Nouveau message ---
    uint16_t btnY = DISPLAY_HEIGHT - 52;
    messagesState.btnNewMessage = ui_button_create("btnNewMsg", "✏️  Nouveau message",
                                                     UI_RECT(20, btnY, 280, 44));
    ui_button_set_style(messagesState.btnNewMessage, BUTTON_STYLE_PRIMARY);
    messagesState.btnNewMessage->onClick = [](UIButton* btn) {
        // Ouvrir l'écran de composition sans destinataire
        ScreenBase* compose = screen_message_compose_create(NULL);
        if (compose)
        {
            screen_set_transition(compose, SCREEN_TRANSITION_SLIDE_UP,
                                  SCREEN_TRANSITION_SLIDE_DOWN, 250);
            ui_push_screen((UIScreen*)compose);
        }
    };
    screen_add_widget(screen, (UIWidget*)messagesState.btnNewMessage);
    
    MESSAGES_DEBUG("Widgets de l'écran de messages initialisés (%d widgets)\n", screen->widgetCount);
}

// ============================================================
// RAFRAÎCHISSEMENT
// ============================================================

void screen_messages_list_refresh(ScreenBase* screen)
{
    if (screen == NULL) return;
    
    // Vider la liste
    ui_list_clear(messagesState.conversationsList);
    
    // Récupérer les conversations depuis le service SMS
    SMSConversation* conversations = NULL;
    uint16_t count = sms_service_get_conversations_array(&conversations);
    
    if (conversations == NULL || count == 0)
    {
        // Aucun message, afficher un message
        ui_list_add_item(messagesState.conversationsList,
                        "Aucun message",
                        "Appuyez sur 'Nouveau message' pour commencer",
                        NULL);
        messagesState.totalUnread = 0;
        return;
    }
    
    // Trier par date (déjà fait par le service)
    messagesState.conversationCount = count;
    messagesState.totalUnread = 0;
    
    for (uint16_t i = 0; i < count; i++)
    {
        SMSConversation* conv = &conversations[i];
        
        // Chercher le nom du contact
        const char* displayName = conv->contactName;
        if (displayName == NULL || displayName[0] == '\0')
        {
            int16_t contactIdx = contact_service_find_by_number(conv->contactNumber);
            if (contactIdx >= 0)
            {
                Contact* contact = contact_service_get(contactIdx);
                if (contact) displayName = contact->name;
            }
        }
        if (displayName == NULL || displayName[0] == '\0')
        {
            displayName = conv->contactNumber;
        }
        
        // Construire l'aperçu du dernier message
        char preview[128];
        char subtext[128];
        
        if (conv->messageCount > 0)
        {
            SMSMessage* lastMsg = &conv->messages[conv->messageCount - 1];
            
            // Tronquer le message si trop long
            uint16_t msgLen = strlen(lastMsg->message);
            if (msgLen > 40)
            {
                strncpy(preview, lastMsg->message, 37);
                strcpy(preview + 37, "...");
            }
            else
            {
                strcpy(preview, lastMsg->message);
            }
            
            // Sous-texte : heure + indicateur non lu
            uint32_t msgTime = lastMsg->timestamp / 1000;
            uint32_t hours = (msgTime / 3600) % 24;
            uint32_t minutes = (msgTime % 3600) / 60;
            
            if (conv->unreadCount > 0)
            {
                snprintf(subtext, sizeof(subtext), "● %d non lu(s)  %02lu:%02lu",
                         conv->unreadCount, (unsigned long)hours, (unsigned long)minutes);
            }
            else
            {
                snprintf(subtext, sizeof(subtext), "%02lu:%02lu",
                         (unsigned long)hours, (unsigned long)minutes);
            }
        }
        else
        {
            strcpy(preview, "(conversation vide)");
            subtext[0] = '\0';
        }
        
        // Ajouter à la liste
        // Note: userData contient le numéro de téléphone pour la sélection
        ui_list_add_item(messagesState.conversationsList,
                        displayName,
                        preview,
                        (void*)conv->contactNumber);
        
        messagesState.totalUnread += conv->unreadCount;
    }
    
    // Libérer la mémoire si allouée
    if (conversations) free(conversations);
    
    MESSAGES_DEBUG("Liste rafraîchie : %d conversations, %d non lus\n",
                  messagesState.conversationCount, messagesState.totalUnread);
}