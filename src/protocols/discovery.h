/**
 * @file discovery.h
 * @brief Protocole de découverte réseau LoRa
 * 
 * Ce fichier implémente le protocole de découverte qui permet
 * aux téléphones LoRa de se trouver mutuellement sur le réseau.
 * 
 * Fonctionnalités :
 * - Diffusion périodique de présence (beacon)
 * - Scan du réseau (qui est là ?)
 * - Maintien d'une liste des pairs connus
 * - Détection des départs (timeout)
 * - Échange de capacités
 * - Mesure de la qualité du signal (RSSI/SNR)
 * 
 * Messages échangés :
 * - DISCOVERY_REQUEST  : "Qui est là ?" (broadcast)
 * - DISCOVERY_RESPONSE : "Je suis là !" avec informations
 * - PRESENCE_UPDATE    : Mise à jour périodique
 * - GOODBYE            : "Je quitte le réseau"
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef DISCOVERY_H
#define DISCOVERY_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "identity.h"
#include "../drivers/lora/lora_driver.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du protocole */
#define DISCOVERY_VERSION               "1.0.0"

/** @brief Version du protocole de découverte */
#define DISCOVERY_PROTOCOL_VERSION      1

/** @brief Intervalle de beacon par défaut (secondes) */
#define DISCOVERY_DEFAULT_BEACON_INTERVAL_S  30

/** @brief Timeout de présence (secondes) - au-delà, le pair est considéré absent */
#define DISCOVERY_PRESENCE_TIMEOUT_S    120

/** @brief Nombre maximum de pairs dans la liste */
#define DISCOVERY_MAX_PEERS             50

/** @brief Durée d'un scan (ms) */
#define DISCOVERY_SCAN_DURATION_MS      5000

/** @brief Intervalle minimum entre deux scans (secondes) */
#define DISCOVERY_MIN_SCAN_INTERVAL_S   10

/** @brief Taille maximale d'un message de découverte */
#define DISCOVERY_MAX_MESSAGE_SIZE      128

// ============================================================
// SECTION 2 : TYPES DE MESSAGES DE DÉCOUVERTE
// ============================================================

/**
 * @brief Types de messages du protocole de découverte
 */
typedef enum {
    DISCOVERY_MSG_REQUEST   = 0x01,     // Demande de découverte (broadcast)
    DISCOVERY_MSG_RESPONSE  = 0x02,     // Réponse à une découverte
    DISCOVERY_MSG_BEACON    = 0x03,     // Balise de présence périodique
    DISCOVERY_MSG_GOODBYE   = 0x04,     // Annonce de départ
    DISCOVERY_MSG_UPDATE    = 0x05      // Mise à jour d'informations
} DiscoveryMessageType;

/**
 * @brief État d'un pair dans le réseau
 */
typedef enum {
    PEER_STATE_UNKNOWN      = 0,    // Jamais vu
    PEER_STATE_DISCOVERED   = 1,    // Découvert récemment
    PEER_STATE_ONLINE       = 2,    // En ligne (beacon reçu)
    PEER_STATE_AWAY         = 3,    // Absent (timeout)
    PEER_STATE_OFFLINE      = 4     // Hors ligne (goodbye reçu)
} PeerState;

// ============================================================
// SECTION 3 : INFORMATIONS D'UN PAIR
// ============================================================

/**
 * @brief Informations complètes sur un pair découvert
 */
typedef struct {
    // Identité
    uint32_t uid;                       // UID du pair
    char msisdn[IDENTITY_PHONE_NUMBER_MAX];  // Numéro de téléphone
    char deviceName[IDENTITY_DEVICE_NAME_MAX]; // Nom du dispositif
    
    // État
    PeerState state;                    // État actuel
    uint32_t firstSeen;                 // Première découverte (timestamp)
    uint32_t lastSeen;                  // Dernière activité (timestamp)
    uint32_t lastBeacon;                // Dernier beacon reçu
    
    // Signal
    int16_t rssi;                       // Dernier RSSI mesuré (dBm)
    int8_t snr;                         // Dernier SNR mesuré (dB)
    uint8_t signalQuality;              // Qualité estimée (0-100)
    
    // Capacités
    uint32_t capabilities;             // Capacités du pair
    
    // Statistiques
    uint32_t beaconsReceived;          // Nombre de beacons reçus
    uint32_t messagesExchanged;         // Messages échangés
    
    // Informations supplémentaires
    uint8_t batteryLevel;               // Niveau batterie (0-100)
    uint8_t protocolVersion;            // Version du protocole
    
} DiscoveryPeer;

// ============================================================
// SECTION 4 : ÉTAT DU MODULE DE DÉCOUVERTE
// ============================================================

/**
 * @brief État du module de découverte
 */
typedef struct {
    bool initialized;                   // Module initialisé
    bool enabled;                       // Découverte activée
    bool scanning;                      // Scan en cours
    
    // Liste des pairs
    DiscoveryPeer peers[DISCOVERY_MAX_PEERS];
    uint8_t peerCount;
    
    // Timers
    uint32_t lastBeaconTime;           // Dernier beacon envoyé
    uint32_t lastScanTime;             // Dernier scan
    uint32_t scanStartTime;            // Début du scan actuel
    
    // Configuration
    uint16_t beaconIntervalS;          // Intervalle de beacon
    uint16_t presenceTimeoutS;         // Timeout de présence
    uint16_t scanDurationMs;           // Durée du scan
    
    // Statistiques
    uint32_t beaconsSent;              // Beacons envoyés
    uint32_t beaconsReceived;          // Beacons reçus
    uint32_t peersDiscovered;          // Pairs découverts (total)
    uint32_t peersLost;                // Pairs perdus
    
} DiscoveryState;

// ============================================================
// SECTION 5 : FORMAT DU MESSAGE DE DÉCOUVERTE
// ============================================================

/**
 * @brief Format d'un message de découverte
 */
typedef struct __attribute__((packed)) {
    uint8_t messageType;                // Type de message
    uint8_t protocolVersion;            // Version du protocole
    
    // Identité de l'expéditeur
    uint32_t senderUid;                 // UID
    char senderMsisdn[IDENTITY_PHONE_NUMBER_MAX];  // Numéro
    char senderName[IDENTITY_DEVICE_NAME_MAX];     // Nom
    
    // Informations
    uint32_t capabilities;              // Capacités
    uint8_t batteryLevel;               // Niveau batterie
    uint8_t signalQuality;              // Qualité du signal estimée
    
    // Horodatage
    uint32_t timestamp;                 // Timestamp du message
    
    // Réservé pour extensions futures
    uint8_t reserved[20];
    
} DiscoveryMessage;

// ============================================================
// SECTION 6 : CALLBACKS
// ============================================================

/**
 * @brief Callback quand un nouveau pair est découvert
 * @param peer Informations du pair
 */
typedef void (*Discovery_NewPeerCallback)(const DiscoveryPeer* peer);

/**
 * @brief Callback quand un pair change d'état
 * @param peer Pair concerné
 * @param oldState Ancien état
 * @param newState Nouvel état
 */
typedef void (*Discovery_PeerStateCallback)(const DiscoveryPeer* peer, 
                                             PeerState oldState, 
                                             PeerState newState);

/**
 * @brief Callback quand un pair est perdu (timeout)
 * @param peer Dernières informations du pair
 */
typedef void (*Discovery_PeerLostCallback)(const DiscoveryPeer* peer);

/**
 * @brief Callback quand un scan est terminé
 * @param peersDiscovered Nombre de pairs découverts
 */
typedef void (*Discovery_ScanCompleteCallback)(uint8_t peersDiscovered);

// ============================================================
// SECTION 7 : FONCTIONS D'INITIALISATION
// ============================================================

bool discovery_init(void);
void discovery_deinit(void);
bool discovery_is_ready(void);

// ============================================================
// SECTION 8 : FONCTIONS DE CONTRÔLE
// ============================================================

/**
 * @brief Active/désactive la découverte réseau
 */
void discovery_enable(bool enable);

/**
 * @brief Vérifie si la découverte est activée
 */
bool discovery_is_enabled(void);

/**
 * @brief Définit l'intervalle de beacon
 */
void discovery_set_beacon_interval(uint16_t seconds);

/**
 * @brief Définit le timeout de présence
 */
void discovery_set_presence_timeout(uint16_t seconds);

// ============================================================
// SECTION 9 : FONCTIONS DE BEACON
// ============================================================

/**
 * @brief Envoie un beacon de présence immédiatement
 */
void discovery_send_beacon(void);

/**
 * @brief Active/désactive l'envoi automatique de beacons
 */
void discovery_beacon_auto_enable(bool enable);

/**
 * @brief Traitement périodique (envoi des beacons automatiques)
 */
void discovery_process(void);

// ============================================================
// SECTION 10 : FONCTIONS DE SCAN
// ============================================================

/**
 * @brief Lance un scan du réseau
 * @param durationMs Durée du scan (0 = valeur par défaut)
 */
void discovery_scan_start(uint32_t durationMs);

/**
 * @brief Arrête le scan en cours
 */
void discovery_scan_stop(void);

/**
 * @brief Vérifie si un scan est en cours
 */
bool discovery_is_scanning(void);

// ============================================================
// SECTION 11 : FONCTIONS DE GESTION DES PAIRS
// ============================================================

/**
 * @brief Récupère la liste des pairs connus
 * @param peers Buffer de sortie
 * @param maxCount Nombre maximum à copier
 * @return Nombre de pairs copiés
 */
uint8_t discovery_get_peers(DiscoveryPeer* peers, uint8_t maxCount);

/**
 * @brief Récupère le nombre de pairs connus
 */
uint8_t discovery_get_peer_count(void);

/**
 * @brief Récupère le nombre de pairs en ligne
 */
uint8_t discovery_get_online_count(void);

/**
 * @brief Recherche un pair par son numéro
 * @param msisdn Numéro recherché
 * @return Pointeur vers le pair, NULL si non trouvé
 */
DiscoveryPeer* discovery_find_peer_by_msisdn(const char* msisdn);

/**
 * @brief Recherche un pair par son UID
 */
DiscoveryPeer* discovery_find_peer_by_uid(uint32_t uid);

/**
 * @brief Recherche un pair par son nom
 */
DiscoveryPeer* discovery_find_peer_by_name(const char* name);

/**
 * @brief Supprime un pair de la liste
 */
void discovery_remove_peer(uint32_t uid);

/**
 * @brief Vide la liste des pairs
 */
void discovery_clear_peers(void);

/**
 * @brief Vérifie la présence des pairs (timeout)
 */
void discovery_check_presence(void);

// ============================================================
// SECTION 12 : FONCTIONS DE TRAITEMENT DES MESSAGES
// ============================================================

/**
 * @brief Traite un message de découverte reçu
 * @param message Message reçu
 * @param rssi RSSI du message
 * @param snr SNR du message
 */
void discovery_process_message(const DiscoveryMessage* message, int16_t rssi, int8_t snr);

/**
 * @brief Construit un message de découverte
 * @param message Message à remplir
 * @param type Type de message
 */
void discovery_build_message(DiscoveryMessage* message, DiscoveryMessageType type);

// ============================================================
// SECTION 13 : FONCTIONS DE DÉPART
// ============================================================

/**
 * @brief Annonce son départ du réseau (goodbye)
 */
void discovery_send_goodbye(void);

// ============================================================
// SECTION 14 : FONCTIONS DE CALLBACKS
// ============================================================

void discovery_set_new_peer_callback(Discovery_NewPeerCallback callback);
void discovery_set_peer_state_callback(Discovery_PeerStateCallback callback);
void discovery_set_peer_lost_callback(Discovery_PeerLostCallback callback);
void discovery_set_scan_complete_callback(Discovery_ScanCompleteCallback callback);

// ============================================================
// SECTION 15 : FONCTIONS DE DÉBOGAGE
// ============================================================

void discovery_print_state(void);
void discovery_print_peers(void);
void discovery_print_peer(const DiscoveryPeer* peer);
void discovery_print_statistics(void);
bool discovery_self_test(void);

// ============================================================
// SECTION 16 : MACROS UTILITAIRES
// ============================================================

#define DISCOVERY_PEER_COUNT()          discovery_get_peer_count()
#define DISCOVERY_ONLINE_COUNT()        discovery_get_online_count()
#define DISCOVERY_IS_ONLINE(peer)       ((peer)->state == PEER_STATE_ONLINE)

// ============================================================
// SECTION 17 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define DISCOVERY_DEBUG(fmt, ...)   printf("[DISCOVERY] " fmt, ##__VA_ARGS__)
#else
    #define DISCOVERY_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 18 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // DISCOVERY_H