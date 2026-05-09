/**
 * @file screen_contact_detail.h
 * @brief Écran de détail d'un contact
 * 
 * Cet écran affiche les informations détaillées d'un contact :
 * - Photo (avatar) du contact
 * - Nom complet
 * - Numéro de téléphone
 * - Actions : Appeler, Envoyer SMS, Modifier, Supprimer
 * - Informations supplémentaires (email, adresse, notes)
 * - Historique des appels avec ce contact
 * - Historique des SMS avec ce contact
 * 
 * Disposition :
 * ┌─────────────────────────────────────────────────────────────┐
 * │ ← Contact                                                 │
 * ├─────────────────────────────────────────────────────────────┤
 * │                                                             │
 * │                    👤 (avatar)                              │
 * │                                                             │
 * │                   Jean Dupont                               │
 * │                  06 12 34 56 78                             │
 * │                                                             │
 * │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
 * │  │  📞 Appeler  │  │  💬 SMS     │  │  ⭐ Favori   │         │
 * │  └─────────────┘  └─────────────┘  └─────────────┘         │
 * │                                                             │
 * │  ─── Informations ────────────────────────────────────     │
 * │  📧 Email : jean@email.com                                  │
 * │  📍 Adresse : 123 rue Example                               │
 * │  📝 Notes : À rappeler demain                               │
 * │                                                             │
 * │  ─── Derniers appels ─────────────────────────────────     │
 * │  ← 10:30 - 5 min 32 s                                       │
 * │  → 09:15 - 2 min 10 s                                       │
 * │                                                             │
 * │  ─── Derniers messages ────────────────────────────────    │
 * │  ← Salut, ça va ?                             10:30         │
 * │  → Oui et toi ?                               10:31         │
 * │                                                             │
 * │              ┌──────────────────────────┐                   │
 * │              │   🗑️  Supprimer le contact │                   │
 * │              └──────────────────────────┘                   │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SCREEN_CONTACT_DETAIL_H
#define SCREEN_CONTACT_DETAIL_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "screen_base.h"
#include "../ui/ui_widgets.h"
#include "../services/contact_service.h"
#include "../services/phone_service.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Nom de l'écran */
#define SCREEN_CONTACT_DETAIL_NAME      "ContactDetailScreen"

/** @brief Nombre maximum d'entrées dans l'historique */
#define CONTACT_DETAIL_MAX_HISTORY      10

// ============================================================
// SECTION 2 : ÉTAT DE L'ÉCRAN DE DÉTAIL
// ============================================================

/**
 * @brief État spécifique à l'écran de détail d'un contact
 */
typedef struct {
    // --- Contact ---
    int16_t contactIndex;               // Index du contact (-1 = nouveau)
    Contact contactData;                // Données du contact
    
    // --- Widgets ---
    UILabel* nameLabel;                 // Nom du contact
    UILabel* numberLabel;               // Numéro de téléphone
    UILabel* emailLabel;                // Email
    UILabel* addressLabel;              // Adresse
    UILabel* notesLabel;                // Notes
    
    UIButton* btnCall;                  // Bouton Appeler
    UIButton* btnSMS;                   // Bouton SMS
    UIButton* btnFavorite;              // Bouton Favori
    UIButton* btnEdit;                  // Bouton Modifier
    UIButton* btnDelete;                // Bouton Supprimer
    
    UIList* callHistoryList;            // Historique des appels
    UIList* smsHistoryList;             // Historique des SMS
    
    // --- État ---
    bool isNewContact;                  // Nouveau contact ?
    bool isEditing;                     // Mode édition ?
    
} ContactDetailScreenState;

// ============================================================
// SECTION 3 : FONCTIONS DE CRÉATION
// ============================================================

/**
 * @brief Crée l'écran de détail d'un contact
 * @param contactIndex Index du contact (-1 = nouveau contact)
 * @return Pointeur vers l'écran créé
 */
ScreenBase* screen_contact_detail_create(int16_t contactIndex);

/**
 * @brief Initialise les widgets de l'écran
 */
void screen_contact_detail_init_widgets(ScreenBase* screen);

/**
 * @brief Charge les données du contact
 */
void screen_contact_detail_load_contact(ScreenBase* screen);

/**
 * @brief Appelle le contact
 */
void screen_contact_detail_call(ScreenBase* screen);

/**
 * @brief Envoie un SMS au contact
 */
void screen_contact_detail_sms(ScreenBase* screen);

/**
 * @brief Bascule l'état favori
 */
void screen_contact_detail_toggle_favorite(ScreenBase* screen);

/**
 * @brief Passe en mode édition
 */
void screen_contact_detail_edit(ScreenBase* screen);

/**
 * @brief Sauvegarde les modifications
 */
void screen_contact_detail_save(ScreenBase* screen);

/**
 * @brief Supprime le contact (avec confirmation)
 */
void screen_contact_detail_delete(ScreenBase* screen);

// ============================================================
// SECTION 4 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define DETAIL_DEBUG(fmt, ...)      printf("[DETAIL] " fmt, ##__VA_ARGS__)
#else
    #define DETAIL_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 5 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SCREEN_CONTACT_DETAIL_H