/**
 * @file screen_messages_list.h
 * @brief Écran de liste des conversations SMS
 * 
 * Cet écran affiche la liste des conversations SMS :
 * - Liste des contacts avec aperçu du dernier message
 * - Badge de messages non lus
 * - Bouton pour créer un nouveau message
 * - Recherche dans les conversations
 * - Appui long pour supprimer une conversation
 * 
 * Disposition :
 * ┌─────────────────────────────────────────────────────────────┐
 * │ ← Messages                              ✏️ Nouveau         │
 * ├─────────────────────────────────────────────────────────────┤
 * │ 🔍 Rechercher...                                           │
 * ├─────────────────────────────────────────────────────────────┤
 * │ ┌─────────────────────────────────────────────────────────┐ │
 * │ │ 👤 Jean Dupont                          ● 2 non lus     │ │
 * │ │    Salut, ça va ?                      10:30            │ │
 * │ ├─────────────────────────────────────────────────────────┤ │
 * │ │ 👤 Marie Martin                                         │ │
 * │ │    OK, à tout à l'heure !             09:45             │ │
 * │ ├─────────────────────────────────────────────────────────┤ │
 * │ │ 👤 Paul Bernard                          ● 1 non lu     │ │
 * │ │    Rendez-vous à 14h                   08:15             │ │
 * │ └─────────────────────────────────────────────────────────┘ │
 * │                                                             │
 * │              ┌──────────────────────────┐                   │
 * │              │     ✏️  Nouveau message   │                   │
 * │              └──────────────────────────┘                   │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SCREEN_MESSAGES_LIST_H
#define SCREEN_MESSAGES_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "screen_base.h"
#include "../ui/ui_widgets.h"
#include "../services/sms_service.h"
#include "../services/contact_service.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Nom de l'écran */
#define SCREEN_MESSAGES_LIST_NAME       "MessagesListScreen"

/** @brief Nombre maximum de conversations affichées */
#define MESSAGES_LIST_MAX_VISIBLE       20

/** @brief Hauteur d'un élément de la liste */
#define MESSAGES_LIST_ITEM_HEIGHT       64

// ============================================================
// SECTION 2 : ÉTAT DE L'ÉCRAN DE MESSAGES
// ============================================================

/**
 * @brief État spécifique à l'écran de liste des messages
 */
typedef struct {
    // --- Widgets ---
    UIButton* btnNewMessage;            // Bouton Nouveau message
    UIList* conversationsList;          // Liste des conversations
    
    // --- État ---
    uint16_t conversationCount;         // Nombre de conversations
    uint16_t totalUnread;               // Total messages non lus
    bool searchActive;                  // Recherche active
    
    // --- Callback ---
    void (*onConversationSelected)(const char* phoneNumber);  // Conversation sélectionnée
    
} MessagesListScreenState;

// ============================================================
// SECTION 3 : FONCTIONS DE CRÉATION
// ============================================================

ScreenBase* screen_messages_list_create(void);
void screen_messages_list_init_widgets(ScreenBase* screen);
void screen_messages_list_refresh(ScreenBase* screen);

// ============================================================
// SECTION 4 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define MESSAGES_DEBUG(fmt, ...)    printf("[MESSAGES] " fmt, ##__VA_ARGS__)
#else
    #define MESSAGES_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 5 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SCREEN_MESSAGES_LIST_H