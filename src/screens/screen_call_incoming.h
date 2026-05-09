/**
 * @file screen_call_incoming.h
 * @brief Écran d'appel entrant
 * 
 * Cet écran s'affiche lorsqu'un appel est reçu :
 * - Nom/numéro de l'appelant
 * - Animation de sonnerie (icône clignotante)
 * - Bouton Accepter (vert)
 * - Bouton Refuser (rouge)
 * - Possibilité de répondre par SMS rapide
 * - Affichage de la photo du contact (si disponible)
 * 
 * Disposition :
 * ┌─────────────────────────────────────────────────────────────┐
 * │                                                             │
 * │                                                             │
 * │                    📞 Appel entrant                         │
 * │                                                             │
 * │                    Jean Dupont                              │
 * │                   06 12 34 56 78                            │
 * │                                                             │
 * │                      ⭕ (clignote)                          │
 * │                                                             │
 * │                                                             │
 * │         ┌────────────────┐  ┌────────────────┐              │
 * │         │  ✅ Accepter   │  │  ❌ Refuser    │              │
 * │         └────────────────┘  └────────────────┘              │
 * │                                                             │
 * │                   💬 Répondre par SMS                       │
 * │                                                             │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SCREEN_CALL_INCOMING_H
#define SCREEN_CALL_INCOMING_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "screen_base.h"
#include "../ui/ui_widgets.h"
#include "../services/phone_service.h"
#include "../services/contact_service.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Nom de l'écran */
#define SCREEN_CALL_INCOMING_NAME       "CallIncomingScreen"

/** @brief Durée maximale de la sonnerie (secondes) */
#define INCOMING_CALL_TIMEOUT_S         30

/** @brief Intervalle de clignotement (ms) */
#define INCOMING_BLINK_INTERVAL_MS      500

/** @brief Nombre de réponses SMS rapides */
#define INCOMING_QUICK_SMS_COUNT        4

// ============================================================
// SECTION 2 : ÉTAT DE L'ÉCRAN D'APPEL ENTRANT
// ============================================================

/**
 * @brief État spécifique à l'écran d'appel entrant
 */
typedef struct {
    // --- Informations de l'appelant ---
    char callerName[32];                // Nom de l'appelant
    char callerNumber[16];              // Numéro de l'appelant
    bool hasContact;                    // Contact existant ?
    
    // --- Widgets ---
    UILabel* titleLabel;                // "Appel entrant"
    UILabel* nameLabel;                 // Nom de l'appelant
    UILabel* numberLabel;               // Numéro de l'appelant
    UILabel* statusLabel;               // Statut (durée sonnerie)
    
    UIButton* btnAccept;                // Bouton Accepter
    UIButton* btnReject;                // Bouton Refuser
    UIButton* btnQuickSMS;              // Bouton SMS rapide
    
    // --- Animation ---
    bool blinkState;                    // État du clignotement
    uint32_t lastBlinkTime;             // Dernier clignotement
    uint32_t ringStartTime;             // Début de la sonnerie
    
    // --- État ---
    bool answered;                      // Appel accepté ?
    bool rejected;                      // Appel refusé ?
    uint8_t ringDuration;               // Durée sonnerie (secondes)
    
    // --- Réponses SMS rapides ---
    const char* quickSMSReplies[INCOMING_QUICK_SMS_COUNT];
    
} CallIncomingScreenState;

// ============================================================
// SECTION 3 : FONCTIONS DE CRÉATION
// ============================================================

/**
 * @brief Crée l'écran d'appel entrant
 * @param callerNumber Numéro de l'appelant
 * @param callerName Nom de l'appelant (peut être NULL)
 * @return Pointeur vers l'écran créé
 */
ScreenBase* screen_call_incoming_create(const char* callerNumber, const char* callerName);

/**
 * @brief Initialise les widgets de l'écran
 */
void screen_call_incoming_init_widgets(ScreenBase* screen);

/**
 * @brief Accepte l'appel entrant
 */
void screen_call_incoming_accept(ScreenBase* screen);

/**
 * @brief Refuse l'appel entrant
 */
void screen_call_incoming_reject(ScreenBase* screen);

/**
 * @brief Envoie un SMS rapide et refuse l'appel
 */
void screen_call_incoming_quick_sms(ScreenBase* screen, uint8_t replyIndex);

/**
 * @brief Met à jour l'animation de sonnerie
 */
void screen_call_incoming_update_blink(ScreenBase* screen);

// ============================================================
// SECTION 4 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define INCOMING_DEBUG(fmt, ...)    printf("[INCOMING] " fmt, ##__VA_ARGS__)
#else
    #define INCOMING_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 5 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SCREEN_CALL_INCOMING_H