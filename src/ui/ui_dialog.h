/**
 * @file ui_dialog.h
 * @brief Widget Boîte de Dialogue (UIDialog) - Définition et fonctions
 * 
 * Ce fichier définit le widget de boîte de dialogue pour :
 * - Messages d'information
 * - Confirmations (Oui/Non)
 * - Alertes et erreurs
 * - Saisie de texte
 * - Dialogues personnalisés
 * 
 * Types de dialogues :
 * - INFO      : Information simple (bouton OK)
 * - CONFIRM   : Confirmation (OK / Annuler)
 * - WARNING   : Avertissement (OK / Annuler)
 * - ERROR     : Erreur (OK)
 * - INPUT     : Saisie de texte
 * - PROGRESS  : Barre de progression
 * - CUSTOM    : Contenu personnalisé
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef UI_DIALOG_H
#define UI_DIALOG_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "ui_core.h"
#include "ui_theme.h"

// ============================================================
// SECTION 1 : TYPES DE DIALOGUES
// ============================================================

/**
 * @brief Types de dialogues
 */
typedef enum {
    DIALOG_TYPE_INFO        = 0,    // Information (OK)
    DIALOG_TYPE_CONFIRM     = 1,    // Confirmation (OK / Annuler)
    DIALOG_TYPE_WARNING     = 2,    // Avertissement (OK / Annuler)
    DIALOG_TYPE_ERROR       = 3,    // Erreur (OK)
    DIALOG_TYPE_INPUT       = 4,    // Saisie de texte (OK / Annuler)
    DIALOG_TYPE_PROGRESS    = 5,    // Barre de progression
    DIALOG_TYPE_CUSTOM      = 6     // Contenu personnalisé
} DialogType;

/**
 * @brief Résultat d'un dialogue
 */
typedef enum {
    DIALOG_RESULT_NONE      = 0,    // Aucun (pas encore fermé)
    DIALOG_RESULT_OK        = 1,    // OK / Confirmer
    DIALOG_RESULT_CANCEL    = 2,    // Annuler
    DIALOG_RESULT_YES       = 3,    // Oui
    DIALOG_RESULT_NO        = 4,    // Non
    DIALOG_RESULT_CLOSE     = 5     // Fermé sans choix
} DialogResult;

/**
 * @brief Icônes de dialogue
 */
typedef enum {
    DIALOG_ICON_NONE        = 0,
    DIALOG_ICON_INFO        = 1,    // ℹ️
    DIALOG_ICON_QUESTION    = 2,    // ❓
    DIALOG_ICON_WARNING     = 3,    // ⚠️
    DIALOG_ICON_ERROR       = 4,    // ❌
    DIALOG_ICON_SUCCESS     = 5,    // ✅
    DIALOG_ICON_LOCK        = 6,    // 🔒
    DIALOG_ICON_BATTERY     = 7     // 🔋
} DialogIcon;

// ============================================================
// SECTION 2 : STRUCTURE DU DIALOGUE
// ============================================================

/**
 * @brief Widget Boîte de Dialogue
 */
typedef struct UIDialog {
    UIWidget base;                      // Widget de base (héritage)
    
    // --- Type ---
    DialogType type;                    // Type de dialogue
    DialogResult result;                // Résultat
    
    // --- Contenu ---
    char title[64];                     // Titre
    char message[256];                  // Message
    DialogIcon icon;                    // Icône
    
    // --- Boutons ---
    char buttonOkText[16];              // Texte du bouton OK (défaut: "OK")
    char buttonCancelText[16];          // Texte du bouton Annuler (défaut: "Annuler")
    bool showCancel;                    // Afficher le bouton Annuler
    
    // --- Saisie (type INPUT) ---
    char inputText[128];                // Texte saisi
    char inputPlaceholder[64];          // Placeholder
    bool inputSecure;                   // Mode mot de passe
    
    // --- Progression (type PROGRESS) ---
    uint8_t progressValue;              // Valeur (0-100)
    bool progressIndeterminate;         // Mode indéterminé (barre animée)
    
    // --- Apparence ---
    uint16_t titleColor;                // Couleur du titre
    uint16_t messageColor;              // Couleur du message
    uint8_t cornerRadius;               // Rayon des coins
    
    // --- Comportement ---
    bool dismissOnBack;                 // Fermer avec la touche Retour
    bool dismissOnOutside;              // Fermer en cliquant à l'extérieur
    bool isModal;                       // Bloque l'interaction avec le reste
    
    // --- Callbacks ---
    void (*onResult)(struct UIDialog* dialog, DialogResult result);
    void (*onShow)(struct UIDialog* dialog);
    void (*onDismiss)(struct UIDialog* dialog);
    
    // --- Rendu personnalisé (type CUSTOM) ---
    void (*customDraw)(struct UIDialog* dialog, UIRect* contentRect);
    
} UIDialog;

// ============================================================
// SECTION 3 : FONCTIONS DE CRÉATION
// ============================================================

/**
 * @brief Crée un dialogue d'information
 */
UIDialog* ui_dialog_create_info(const char* title, const char* message);

/**
 * @brief Crée un dialogue de confirmation
 */
UIDialog* ui_dialog_create_confirm(const char* title, const char* message);

/**
 * @brief Crée un dialogue d'avertissement
 */
UIDialog* ui_dialog_create_warning(const char* title, const char* message);

/**
 * @brief Crée un dialogue d'erreur
 */
UIDialog* ui_dialog_create_error(const char* title, const char* message);

/**
 * @brief Crée un dialogue de saisie
 */
UIDialog* ui_dialog_create_input(const char* title, const char* placeholder);

/**
 * @brief Crée un dialogue de progression
 */
UIDialog* ui_dialog_create_progress(const char* title, const char* message);

/**
 * @brief Crée un dialogue personnalisé
 */
UIDialog* ui_dialog_create_custom(const char* title, UIRect rect);

// ============================================================
// SECTION 4 : FONCTIONS DE CONFIGURATION
// ============================================================

void ui_dialog_set_icon(UIDialog* dialog, DialogIcon icon);
void ui_dialog_set_button_text(UIDialog* dialog, const char* okText, const char* cancelText);
void ui_dialog_set_input_secure(UIDialog* dialog, bool secure);
void ui_dialog_set_progress(UIDialog* dialog, uint8_t value);
void ui_dialog_set_indeterminate(UIDialog* dialog, bool indeterminate);
void ui_dialog_set_dismiss_policy(UIDialog* dialog, bool onBack, bool onOutside);
void ui_dialog_set_colors(UIDialog* dialog, uint16_t titleColor, uint16_t messageColor);

// ============================================================
// SECTION 5 : FONCTIONS D'AFFICHAGE
// ============================================================

/**
 * @brief Affiche le dialogue (retourne le résultat via callback)
 */
void ui_dialog_show(UIDialog* dialog);

/**
 * @brief Ferme le dialogue avec un résultat
 */
void ui_dialog_dismiss(UIDialog* dialog, DialogResult result);

/**
 * @brief Vérifie si un dialogue est actuellement affiché
 */
bool ui_dialog_is_showing(void);

/**
 * @brief Récupère le dialogue actif
 */
UIDialog* ui_dialog_get_active(void);

/**
 * @brief Ferme tous les dialogues
 */
void ui_dialog_dismiss_all(void);

// ============================================================
// SECTION 6 : MACROS RAPIDES
// ============================================================

#define UI_DIALOG_INFO(title, msg) \
    ui_dialog_create_info(title, msg)

#define UI_DIALOG_CONFIRM(title, msg) \
    ui_dialog_create_confirm(title, msg)

#define UI_DIALOG_WARNING(title, msg) \
    ui_dialog_create_warning(title, msg)

#define UI_DIALOG_ERROR(title, msg) \
    ui_dialog_create_error(title, msg)

#define UI_DIALOG_INPUT(title, placeholder) \
    ui_dialog_create_input(title, placeholder)

// ============================================================
// SECTION 7 : FONCTIONS DE DÉBOGAGE
// ============================================================

void ui_dialog_print_state(void);

// ============================================================
// SECTION 8 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // UI_DIALOG_H