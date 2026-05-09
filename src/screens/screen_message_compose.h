/**
 * @file screen_message_compose.h
 * @brief Écran de composition d'un message SMS
 * 
 * Cet écran permet de rédiger et d'envoyer un SMS :
 * - Champ destinataire (numéro ou contact)
 * - Zone de saisie du message
 * - Compteur de caractères (160 max)
 * - Bouton Envoyer
 * - Historique de la conversation (si répondre)
 * - Clavier virtuel pour la saisie
 * 
 * Disposition :
 * ┌─────────────────────────────────────────────────────────────┐
 * │ ← Nouveau message                          👤 Contacts      │
 * ├─────────────────────────────────────────────────────────────┤
 * │ Destinataire : [________________________]                   │
 * ├─────────────────────────────────────────────────────────────┤
 * │                                                             │
 * │ ┌─────────────────────────────────────────────────────────┐ │
 * │ │                                                         │ │
 * │ │              (messages de la conversation)              │ │
 * │ │                                                         │ │
 * │ │  ← Salut, ça va ?                         10:30         │ │
 * │ │                                    Oui et toi ? → 10:31 │ │
 * │ │                                                         │ │
 * │ └─────────────────────────────────────────────────────────┘ │
 * │                                                             │
 * ├─────────────────────────────────────────────────────────────┤
 * │ [____________________________________________] 120/160      │
 * ├─────────────────────────────────────────────────────────────┤
 * │                       [Envoyer]                             │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SCREEN_MESSAGE_COMPOSE_H
#define SCREEN_MESSAGE_COMPOSE_H

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
#define SCREEN_MESSAGE_COMPOSE_NAME     "MessageComposeScreen"

/** @brief Longueur maximale d'un SMS */
#define COMPOSE_SMS_MAX_LENGTH          160

/** @brief Nombre maximum de messages affichés dans l'historique */
#define COMPOSE_MAX_HISTORY             50

// ============================================================
// SECTION 2 : ÉTAT DE L'ÉCRAN DE COMPOSITION
// ============================================================

/**
 * @brief État spécifique à l'écran de composition
 */
typedef struct {
    // --- Destinataire ---
    char recipientNumber[16];           // Numéro du destinataire
    char recipientName[32];             // Nom du destinataire
    
    // --- Widgets ---
    UITextBox* recipientInput;          // Champ destinataire
    UITextBox* messageInput;            // Zone de saisie du message
    UIButton* btnSend;                  // Bouton Envoyer
    UIButton* btnContacts;              // Bouton Contacts
    UILabel* charCountLabel;            // Compteur de caractères
    UIList* historyList;                // Historique de la conversation
    
    // --- État ---
    uint16_t charCount;                 // Nombre de caractères
    bool isReply;                       // Mode réponse ?
    uint16_t historyCount;              // Nombre de messages dans l'historique
    
} MessageComposeScreenState;

// ============================================================
// SECTION 3 : FONCTIONS DE CRÉATION
// ============================================================

/**
 * @brief Crée l'écran de composition de message
 * @param recipientNumber Numéro du destinataire (NULL = nouveau message)
 * @return Pointeur vers l'écran créé
 */
ScreenBase* screen_message_compose_create(const char* recipientNumber);

/**
 * @brief Initialise les widgets de l'écran
 */
void screen_message_compose_init_widgets(ScreenBase* screen);

/**
 * @brief Envoie le message
 */
void screen_message_compose_send(ScreenBase* screen);

/**
 * @brief Met à jour le compteur de caractères
 */
void screen_message_compose_update_counter(ScreenBase* screen);

/**
 * @brief Charge l'historique de la conversation
 */
void screen_message_compose_load_history(ScreenBase* screen);

// ============================================================
// SECTION 4 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define COMPOSE_DEBUG(fmt, ...)     printf("[COMPOSE] " fmt, ##__VA_ARGS__)
#else
    #define COMPOSE_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 5 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SCREEN_MESSAGE_COMPOSE_H