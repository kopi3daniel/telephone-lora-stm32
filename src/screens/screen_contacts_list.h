/**
 * @file screen_contacts_list.h
 * @brief Écran de liste des contacts
 * 
 * Cet écran affiche le carnet d'adresses :
 * - Liste alphabétique des contacts
 * - Barre de recherche
 * - Bouton d'ajout de contact
 * - Favoris en haut de liste
 * - Numérotation rapide
 * - Actions : Appeler, Envoyer SMS, Modifier, Supprimer
 * 
 * Disposition :
 * ┌─────────────────────────────────────────────────────────────┐
 * │ ← Contacts                               ＋ Ajouter         │
 * ├─────────────────────────────────────────────────────────────┤
 * │ 🔍 Rechercher...                                           │
 * ├─────────────────────────────────────────────────────────────┤
 * │ ⭐ Favoris                                                 │
 * │ ┌─────────────────────────────────────────────────────────┐ │
 * │ │ ⭐ Jean Dupont                          0612345678      │ │
 * │ │ ⭐ Marie Martin                         0687654321      │ │
 * │ └─────────────────────────────────────────────────────────┘ │
 * ├─────────────────────────────────────────────────────────────┤
 * │ A                                                          │
 * │ ┌─────────────────────────────────────────────────────────┐ │
 * │ │   Alice Bernard                        0690123456       │ │
 * │ │   Antoine Durand                      0611223344       │ │
 * │ └─────────────────────────────────────────────────────────┘ │
 * │ B                                                          │
 * │ ┌─────────────────────────────────────────────────────────┐ │
 * │ │   Bruno Petit                          0622334455       │ │
 * │ └─────────────────────────────────────────────────────────┘ │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SCREEN_CONTACTS_LIST_H
#define SCREEN_CONTACTS_LIST_H

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
#define SCREEN_CONTACTS_LIST_NAME       "ContactsListScreen"

/** @brief Nombre maximum de contacts affichés */
#define CONTACTS_LIST_MAX_VISIBLE       100

/** @brief Hauteur d'un élément de la liste */
#define CONTACTS_LIST_ITEM_HEIGHT       52

/** @brief Mode de l'écran */
typedef enum {
    CONTACTS_MODE_BROWSE    = 0,    // Navigation normale
    CONTACTS_MODE_SELECT    = 1,    // Sélection (pour SMS ou appel)
    CONTACTS_MODE_PICK      = 2     // Choix unique (retourne le contact)
} ContactsMode;

// ============================================================
// SECTION 2 : ÉTAT DE L'ÉCRAN DE CONTACTS
// ============================================================

/**
 * @brief État spécifique à l'écran de contacts
 */
typedef struct {
    // --- Widgets ---
    UIList* contactsList;               // Liste des contacts
    UIButton* btnAdd;                   // Bouton Ajouter
    UITextBox* searchInput;             // Champ de recherche
    
    // --- État ---
    ContactsMode mode;                  // Mode de l'écran
    uint16_t contactCount;              // Nombre de contacts
    uint16_t filteredCount;             // Nombre après filtrage
    char searchQuery[32];               // Texte de recherche
    bool showFavoritesFirst;            // Favoris en premier
    
    // --- Callbacks ---
    void (*onContactSelected)(uint16_t contactIndex);  // Contact sélectionné
    void (*onContactLongPress)(uint16_t contactIndex); // Appui long
    
} ContactsListScreenState;

// ============================================================
// SECTION 3 : FONCTIONS DE CRÉATION
// ============================================================

/**
 * @brief Crée l'écran de liste des contacts
 * @return Pointeur vers l'écran créé
 */
ScreenBase* screen_contacts_list_create(void);

/**
 * @brief Crée l'écran en mode sélection (pour appel/SMS)
 * @param mode Mode de sélection
 * @return Pointeur vers l'écran créé
 */
ScreenBase* screen_contacts_list_create_mode(ContactsMode mode);

/**
 * @brief Initialise les widgets de l'écran
 */
void screen_contacts_list_init_widgets(ScreenBase* screen);

/**
 * @brief Rafraîchit la liste des contacts
 */
void screen_contacts_list_refresh(ScreenBase* screen);

/**
 * @brief Filtre les contacts selon la recherche
 */
void screen_contacts_list_filter(ScreenBase* screen, const char* query);

/**
 * @brief Ouvre l'écran de détail d'un contact
 */
void screen_contacts_list_open_contact(ScreenBase* screen, uint16_t index);

/**
 * @brief Appelle un contact directement
 */
void screen_contacts_list_call_contact(ScreenBase* screen, uint16_t index);

/**
 * @brief Envoie un SMS à un contact
 */
void screen_contacts_list_sms_contact(ScreenBase* screen, uint16_t index);

// ============================================================
// SECTION 4 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define CONTACTS_DEBUG(fmt, ...)    printf("[CONTACTS] " fmt, ##__VA_ARGS__)
#else
    #define CONTACTS_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 5 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SCREEN_CONTACTS_LIST_H