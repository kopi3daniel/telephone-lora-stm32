/**
 * @file packet_router.h
 * @brief Routeur de paquets - Distribution des messages reçus
 * 
 * Ce fichier implémente le routeur central qui :
 * - Reçoit tous les paquets LoRa entrants
 * - Identifie le type de paquet
 * - Le distribue au protocole approprié
 * - Gère les priorités et files d'attente
 * 
 * Architecture :
 * ┌─────────────────────────────────────────────────────────┐
 * │                    LoRa RX                               │
 * └───────────────────────┬─────────────────────────────────┘
 *                         │
 *                         ▼
 * ┌─────────────────────────────────────────────────────────┐
 * │                 PACKET ROUTER                           │
 * │  ┌──────────┐  ┌──────────┐  ┌──────────┐             │
 * │  │ Discovery│  │  Call    │  │   SMS    │  ...        │
 * │  └──────────┘  └──────────┘  └──────────┘             │
 * └─────────────────────────────────────────────────────────┘
 * 
 * Types de paquets reconnus :
 * - DISCOVERY (0x40-0x4F) → Protocole de découverte
 * - CALL (0x10-0x1F)       → Protocole d'appel
 * - AUDIO (0x20-0x2F)      → Données audio
 * - SMS (0x30-0x3F)        → Messages texte
 * - SYSTEM (0xF0-0xFF)     → Paquets système (ACK, erreurs)
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef PACKET_ROUTER_H
#define PACKET_ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "../drivers/lora/lora_driver.h"
#include "../drivers/lora/lora_packet.h"

// ============================================================
// SECTION 1 : CONSTANTES
// ============================================================

/** @brief Version du module */
#define PACKET_ROUTER_VERSION           "1.0.0"

/** @brief Taille maximale de la file d'attente */
#define PACKET_ROUTER_QUEUE_SIZE        32

/** @brief Nombre maximum de handlers par type */
#define PACKET_ROUTER_MAX_HANDLERS      5

/** @brief Timeout de traitement d'un paquet (ms) */
#define PACKET_ROUTER_PROCESS_TIMEOUT   100

// ============================================================
// SECTION 2 : TYPES DE PAQUETS RECONNUS
// ============================================================

/**
 * @brief Catégories de paquets pour le routage
 */
typedef enum {
    PACKET_CATEGORY_DISCOVERY  = 0,     // Découverte réseau (0x40-0x4F)
    PACKET_CATEGORY_CALL       = 1,     // Appels (0x10-0x1F)
    PACKET_CATEGORY_AUDIO      = 2,     // Audio (0x20-0x2F)
    PACKET_CATEGORY_SMS        = 3,     // SMS (0x30-0x3F)
    PACKET_CATEGORY_SYSTEM     = 4,     // Système (0xF0-0xFF)
    PACKET_CATEGORY_UNKNOWN    = 5      // Inconnu
} PacketCategory;

/**
 * @brief Priorité de traitement
 */
typedef enum {
    PACKET_PRIORITY_LOW        = 0,     // Basse (découverte)
    PACKET_PRIORITY_NORMAL     = 1,     // Normale (SMS)
    PACKET_PRIORITY_HIGH       = 2,     // Haute (signalisation appel)
    PACKET_PRIORITY_CRITICAL   = 3      // Critique (audio en cours)
} PacketPriority;

// ============================================================
// SECTION 3 : HANDLER DE PAQUET
// ============================================================

/**
 * @brief Signature d'une fonction de traitement de paquet
 * @param packet Paquet à traiter
 * @param rssi Force du signal (dBm)
 * @param snr Rapport signal/bruit (dB)
 */
typedef void (*PacketHandler)(const LoRaPacket* packet, int16_t rssi, int8_t snr);

/**
 * @brief Entrée dans la table de routage
 */
typedef struct {
    LoRaPacketType packetType;          // Type de paquet
    PacketCategory category;            // Catégorie
    PacketPriority priority;            // Priorité
    PacketHandler handler;              // Fonction de traitement
    bool enabled;                       // Activé
} PacketRoute;

/**
 * @brief Paquet dans la file d'attente
 */
typedef struct {
    LoRaPacket packet;                  // Paquet
    int16_t rssi;                       // RSSI
    int8_t snr;                         // SNR
    uint32_t timestamp;                 // Horodatage
    bool processed;                     // Traité
} QueuedPacket;

// ============================================================
// SECTION 4 : ÉTAT DU ROUTEUR
// ============================================================

/**
 * @brief État du routeur de paquets
 */
typedef struct {
    bool initialized;                   // Module initialisé
    
    // Table de routage
    PacketRoute routes[32];             // Routes configurées
    uint8_t routeCount;                 // Nombre de routes
    
    // File d'attente
    QueuedPacket queue[PACKET_ROUTER_QUEUE_SIZE];
    uint8_t queueHead;                  // Tête de file (écriture)
    uint8_t queueTail;                  // Queue de file (lecture)
    uint8_t queueCount;                 // Nombre de paquets en attente
    
    // Statistiques
    uint32_t totalPacketsRouted;        // Total paquets routés
    uint32_t totalPacketsDropped;       // Total paquets ignorés
    uint32_t totalPacketsQueued;        // Total paquets mis en file
    uint32_t totalErrors;               // Total erreurs
    
    // Filtrage
    bool filterEnabled;                 // Filtrage activé
    uint32_t filterMask;                // Masque de filtrage
    
} PacketRouterState;

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

bool packet_router_init(void);
void packet_router_deinit(void);
bool packet_router_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS DE ROUTAGE
// ============================================================

/**
 * @brief Enregistre une route (handler pour un type de paquet)
 * @param packetType Type de paquet à router
 * @param handler Fonction de traitement
 * @param priority Priorité
 * @return true si enregistré
 */
bool packet_router_register(LoRaPacketType packetType, PacketHandler handler, PacketPriority priority);

/**
 * @brief Supprime une route
 */
bool packet_router_unregister(LoRaPacketType packetType);

/**
 * @brief Route un paquet entrant vers le bon handler
 * @param packet Paquet à router
 * @param rssi RSSI du paquet
 * @param snr SNR du paquet
 * @return true si routé avec succès
 */
bool packet_router_route(const LoRaPacket* packet, int16_t rssi, int8_t snr);

/**
 * @brief Traitement périodique (traite la file d'attente)
 */
void packet_router_process(void);

// ============================================================
// SECTION 7 : FONCTIONS DE FILE D'ATTENTE
// ============================================================

bool packet_router_enqueue(const LoRaPacket* packet, int16_t rssi, int8_t snr);
bool packet_router_dequeue(QueuedPacket* packet);
uint8_t packet_router_queue_count(void);
void packet_router_flush_queue(void);

// ============================================================
// SECTION 8 : FONCTIONS DE FILTRAGE
// ============================================================

void packet_router_filter_enable(bool enable);
void packet_router_filter_set_mask(uint32_t mask);
bool packet_router_is_filtered(LoRaPacketType type);

// ============================================================
// SECTION 9 : FONCTIONS DE STATISTIQUES
// ============================================================

uint32_t packet_router_get_routed_count(void);
uint32_t packet_router_get_dropped_count(void);
uint32_t packet_router_get_queue_count(void);
void packet_router_reset_statistics(void);

// ============================================================
// SECTION 10 : FONCTIONS DE DÉBOGAGE
// ============================================================

void packet_router_print_routes(void);
void packet_router_print_queue(void);
void packet_router_print_statistics(void);
bool packet_router_self_test(void);

// ============================================================
// SECTION 11 : MACROS UTILITAIRES
// ============================================================

#define PACKET_ROUTER_QUEUE_SIZE_GET()  packet_router_queue_count()
#define PACKET_ROUTER_IS_EMPTY()        (packet_router_queue_count() == 0)

// ============================================================
// SECTION 12 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define ROUTER_DEBUG(fmt, ...)      printf("[ROUTER] " fmt, ##__VA_ARGS__)
#else
    #define ROUTER_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 13 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // PACKET_ROUTER_H