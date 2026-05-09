/**
 * @file screen_call_active.h
 * @brief Écran d'appel en cours (communication établie)
 * 
 * Cet écran s'affiche pendant un appel téléphonique :
 * - Nom/numéro du correspondant
 * - Durée de l'appel (timer)
 * - Qualité du signal (RSSI/SNR)
 * - Contrôles d'appel (mute, haut-parleur, clavier, fin)
 * - Indicateur VU-meter
 * - État de la compression audio
 * 
 * Disposition :
 * ┌─────────────────────────────────────────────────────────────┐
 * │ ← En appel                                                  │
 * ├─────────────────────────────────────────────────────────────┤
 * │                                                             │
 * │                    Jean Dupont                              │
 * │                   06 12 34 56 78                            │
 * │                                                             │
 * │                      02:35                                  │
 * │                                                             │
 * │              ▁▂▃▄▅▆▇█  (VU-meter)                         │
 * │                                                             │
 * │     ┌─────────┐  ┌─────────┐  ┌─────────┐                  │
 * │     │   🔇    │  │   🔊    │  │   📞    │                  │
 * │     │  Muet   │  │  HP     │  │ Clavier │                  │
 * │     └─────────┘  └─────────┘  └─────────┘                  │
 * │                                                             │
 * │              ┌──────────────────────────┐                   │
 * │              │      ⏹  Raccrocher       │                   │
 * │              └──────────────────────────┘                   │
 * │                                                             │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef SCREEN_CALL_ACTIVE_H
#define SCREEN_CALL_ACTIVE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include "screen_base.h"
#include "../ui/ui_widgets.h"
#include "../services/phone_service.h"
#include "../drivers/audio/audio_manager.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Nom de l'écran */
#define SCREEN_CALL_ACTIVE_NAME         "CallActiveScreen"

/** @brief États de l'appel */
typedef enum {
    CALL_DISPLAY_DIALING    = 0,    // Numérotation en cours
    CALL_DISPLAY_RINGING    = 1,    // Sonnerie
    CALL_DISPLAY_CONNECTED  = 2,    // Communication établie
    CALL_DISPLAY_ENDED      = 3     // Appel terminé
} CallDisplayState;

// ============================================================
// SECTION 2 : ÉTAT DE L'ÉCRAN D'APPEL
// ============================================================

/**
 * @brief État spécifique à l'écran d'appel
 */
typedef struct {
    // --- Informations de l'appel ---
    char contactName[32];               // Nom du correspondant
    char phoneNumber[16];               // Numéro du correspondant
    CallDisplayState displayState;      // État affiché
    uint32_t callStartTime;             // Début de l'appel
    uint32_t callDuration;              // Durée en secondes
    
    // --- Widgets ---
    UILabel* nameLabel;                 // Nom du correspondant
    UILabel* numberLabel;               // Numéro du correspondant
    UILabel* durationLabel;             // Timer (MM:SS)
    UILabel* statusLabel;               // Statut (Numérotation... / En appel)
    UILabel* signalLabel;               // Qualité du signal
    
    UIButton* btnMute;                  // Bouton Muet
    UIButton* btnSpeaker;               // Bouton Haut-parleur
    UIButton* btnDialpad;               // Bouton Clavier
    UIButton* btnEnd;                   // Bouton Raccrocher
    
    // --- VU-meter ---
    uint8_t vuLevel;                    // Niveau VU (0-100)
    uint8_t vuPeak;                     // Crête VU
    
    // --- État ---
    bool muted;                         // Mode muet
    bool speakerOn;                     // Haut-parleur
    bool dialpadVisible;                // Clavier visible
    int16_t signalRssi;                 // RSSI actuel
    uint8_t signalQuality;              // Qualité (0-100)
    
} CallActiveScreenState;

// ============================================================
// SECTION 3 : FONCTIONS DE CRÉATION
// ============================================================

/**
 * @brief Crée l'écran d'appel
 * @param phoneNumber Numéro du correspondant
 * @param contactName Nom du correspondant (peut être NULL)
 * @return Pointeur vers l'écran créé
 */
ScreenBase* screen_call_active_create(const char* phoneNumber, const char* contactName);

/**
 * @brief Initialise les widgets de l'écran d'appel
 */
void screen_call_active_init_widgets(ScreenBase* screen);

/**
 * @brief Met à jour la durée de l'appel
 */
void screen_call_active_update_duration(ScreenBase* screen);

/**
 * @brief Met à jour le VU-meter
 */
void screen_call_active_update_vu(ScreenBase* screen, uint8_t level, uint8_t peak);

/**
 * @brief Met à jour la qualité du signal
 */
void screen_call_active_update_signal(ScreenBase* screen, int16_t rssi);

/**
 * @brief Bascule le mode muet
 */
void screen_call_active_toggle_mute(ScreenBase* screen);

/**
 * @brief Bascule le haut-parleur
 */
void screen_call_active_toggle_speaker(ScreenBase* screen);

/**
 * @brief Affiche/masque le clavier DTMF
 */
void screen_call_active_toggle_dialpad(ScreenBase* screen);

/**
 * @brief Termine l'appel
 */
void screen_call_active_end_call(ScreenBase* screen);

// ============================================================
// SECTION 4 : FONCTIONS D'ÉTAT
// ============================================================

void screen_call_active_set_state(ScreenBase* screen, CallDisplayState state);
CallDisplayState screen_call_active_get_state(ScreenBase* screen);

// ============================================================
// SECTION 5 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define CALL_ACTIVE_DEBUG(fmt, ...) printf("[CALL_ACTIVE] " fmt, ##__VA_ARGS__)
#else
    #define CALL_ACTIVE_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 6 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // SCREEN_CALL_ACTIVE_H