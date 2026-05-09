/**
 * @file mesh_protocol.h
 * @brief Protocole de réseau maillé (Mesh Network)
 * 
 * Ce fichier implémente un protocole de réseau maillé simple
 * permettant aux téléphones LoRa de relayer les messages.
 * 
 * Fonctionnalités :
 * - Routage par inondation contrôlée (flooding)
 * - Détection de boucles (TTL)
 * - Table de routage simplifiée
 * - Relais des paquets
 * - Découverte des nœuds du maillage
 * 
 * Topologie maillée :
 *     ┌───────┐         ┌───────┐
 *     │Phone A│────────▶│Phone B│
 *     └───┬───┘         └───┬───┘
 *         │                 │
 *         ▼                 ▼
 *     ┌───────┐         ┌───────┐
 *     │Phone C│◀────────│Phone D│
 *     └───────┘         └───────┘
 * 
 * Format d'un paquet mesh :
 * ┌──────────┬──────────┬──────────┬──────────┬──────────────┐
 * │ Source   │ Dest.    │ TTL      │ Seq.     │ Payload      │
 * │ 4 bytes  │ 4 bytes  │ 1 byte   │ 2 bytes  │ N bytes      │
 * └──────────┴──────────┴──────────┴──────────┴──────────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef MESH_PROTOCOL_H
#define MESH_PROTOCOL_H

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
#define MESH_PROTOCOL_VERSION           "1.0.0"

/** @brief TTL maximum (nombre de sauts) */
#define MESH_MAX_TTL                    8

/** @brief TTL par défaut */
#define MESH_DEFAULT_TTL                4

/** @brief Taille maximale de la table de routage */
#define MESH_MAX_ROUTES                 32

/** @brief Timeout d'une entrée de routage (secondes) */
#define MESH_ROUTE_TIMEOUT_S            300     // 5 minutes

/** @brief Taille du cache des paquets relayés */
#define MESH_CACHE_SIZE                 64

/** @brief Timeout du cache (secondes) */
#define MESH_CACHE_TIMEOUT_S            60

/** @brief Intervalle de beacon mesh (secondes) */
#define MESH_BEACON_INTERVAL_S          60

/** @brief Nombre maximum de nœuds dans le maillage */
#define MESH_MAX_NODES                  50

// ============================================================
// SECTION 2 : TYPES DE MESSAGES MESH
// ============================================================

/**
 * @brief Types de messages du protocole mesh
 */
typedef enum {
    MESH_MSG_DATA           = 0x60,     // Données à relayer
    MESH_MSG_ROUTE_REQUEST  = 0x61,     // Demande de route
    MESH_MSG_ROUTE_REPLY    = 0x62,     // Réponse de route
    MESH_MSG_BEACON         = 0x63,     // Balise de présence mesh
    MESH_MSG_HELLO          = 0x64,     // Message de voisinage
    MESH_MSG_ERROR          = 0x65      // Erreur
} MeshMessageType;

/**
 * @brief Rôle d'un nœud dans le maillage
 */
typedef enum {
    MESH_ROLE_NODE          = 0,    // Nœud simple
    MESH_ROLE_RELAY         = 1,    // Relais (retransmet les paquets)
    MESH_ROLE_GATEWAY       = 2,    // Passerelle (connecte à un autre réseau)
    MESH_ROLE_COORDINATOR   = 3     // Coordinateur (gère le réseau)
} MeshRole;

// ============================================================
// SECTION 3 : STRUCTURES DE DONNÉES
// ============================================================

/**
 * @brief Format d'un paquet mesh
 */
typedef struct __attribute__((packed)) {
    uint8_t messageType;                // Type de message
    uint8_t ttl;                        // Time To Live (décrémenté à chaque saut)
    uint16_t sequenceNumber;            // Numéro de séquence (détection doublons)
    
    uint32_t sourceUid;                 // UID de la source originale
    uint32_t destinationUid;            // UID de la destination finale
    uint32_t previousHopUid;            // UID du nœud précédent
    
    uint16_t payloadLength;             // Longueur des données
    uint8_t payload[200];               // Données utiles
    
    uint16_t crc;                       // CRC16
} MeshPacket;

/**
 * @brief Entrée de la table de routage
 */
typedef struct {
    uint32_t destinationUid;            // UID de destination
    uint32_t nextHopUid;                // Prochain saut
    uint8_t hopCount;                   // Nombre de sauts
    uint8_t signalQuality;              // Qualité du lien (0-100)
    uint32_t lastUpdated;               // Dernière mise à jour
    bool valid;                         // Entrée valide
} MeshRoute;

/**
 * @brief Entrée du cache (anti-doublons)
 */
typedef struct {
    uint32_t sourceUid;                 // UID source
    uint16_t sequenceNumber;            // Numéro de séquence
    uint32_t timestamp;                 // Horodatage
} MeshCacheEntry;

/**
 * @brief Nœud du maillage
 */
typedef struct {
    uint32_t uid;                       // UID du nœud
    char msisdn[IDENTITY_PHONE_NUMBER_MAX];  // Numéro
    char name[IDENTITY_DEVICE_NAME_MAX];     // Nom
    MeshRole role;                      // Rôle
    uint8_t hopCount;                   // Nombre de sauts pour l'atteindre
    int16_t rssi;                       // RSSI du dernier contact
    uint32_t lastSeen;                  // Dernière activité
    bool online;                        // En ligne
} MeshNode;

// ============================================================
// SECTION 4 : ÉTAT DU PROTOCOLE MESH
// ============================================================

/**
 * @brief État du protocole mesh
 */
typedef struct {
    bool initialized;                   // Module initialisé
    bool enabled;                       // Mesh activé
    
    // Identification
    MeshRole role;                      // Rôle de ce nœud
    
    // Table de routage
    MeshRoute routes[MESH_MAX_ROUTES];
    uint8_t routeCount;
    
    // Cache anti-doublons
    MeshCacheEntry cache[MESH_CACHE_SIZE];
    uint8_t cacheHead;
    uint8_t cacheCount;
    
    // Nœuds connus
    MeshNode nodes[MESH_MAX_NODES];
    uint8_t nodeCount;
    
    // Compteurs
    uint16_t sequenceNumber;            // Numéro de séquence local
    uint32_t packetsRelayed;            // Paquets relayés
    uint32_t packetsOriginated;         // Paquets créés
    uint32_t packetsDropped;            // Paquets ignorés
    
    // Timers
    uint32_t lastBeaconTime;            // Dernier beacon
    uint32_t lastRouteMaintenance;      // Dernière maintenance
    
    // Configuration
    uint8_t defaultTtl;                 // TTL par défaut
    uint16_t beaconIntervalS;           // Intervalle beacon
    
} MeshProtocolState;

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

bool mesh_protocol_init(void);
void mesh_protocol_deinit(void);
bool mesh_protocol_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS DE CONTRÔLE
// ============================================================

void mesh_protocol_enable(bool enable);
bool mesh_protocol_is_enabled(void);
void mesh_protocol_set_role(MeshRole role);
MeshRole mesh_protocol_get_role(void);
void mesh_protocol_set_ttl(uint8_t ttl);

// ============================================================
// SECTION 7 : FONCTIONS D'ENVOI
// ============================================================

bool mesh_protocol_send(uint32_t destinationUid, const uint8_t* data, uint16_t length);
bool mesh_protocol_send_broadcast(const uint8_t* data, uint16_t length);
bool mesh_protocol_send_to_msisdn(const char* msisdn, const uint8_t* data, uint16_t length);

// ============================================================
// SECTION 8 : FONCTIONS DE RÉCEPTION/RELAIS
// ============================================================

void mesh_protocol_process(void);
void mesh_protocol_process_packet(const MeshPacket* packet, int16_t rssi);
bool mesh_protocol_should_relay(const MeshPacket* packet);
void mesh_protocol_relay_packet(const MeshPacket* packet);

// ============================================================
// SECTION 9 : FONCTIONS DE ROUTAGE
// ============================================================

bool mesh_protocol_add_route(uint32_t destination, uint32_t nextHop, uint8_t hops);
bool mesh_protocol_remove_route(uint32_t destination);
MeshRoute* mesh_protocol_find_route(uint32_t destination);
uint32_t mesh_protocol_get_next_hop(uint32_t destination);
void mesh_protocol_clear_routes(void);
void mesh_protocol_maintain_routes(void);

// ============================================================
// SECTION 10 : FONCTIONS DE CACHE
// ============================================================

bool mesh_protocol_is_duplicate(uint32_t sourceUid, uint16_t sequenceNumber);
void mesh_protocol_cache_add(uint32_t sourceUid, uint16_t sequenceNumber);
void mesh_protocol_cache_cleanup(void);

// ============================================================
// SECTION 11 : FONCTIONS DE VOISINAGE
// ============================================================

void mesh_protocol_send_beacon(void);
void mesh_protocol_send_hello(void);
uint8_t mesh_protocol_get_node_count(void);
uint8_t mesh_protocol_get_online_count(void);
MeshNode* mesh_protocol_find_node(uint32_t uid);
void mesh_protocol_print_nodes(void);

// ============================================================
// SECTION 12 : FONCTIONS DE CALLBACKS
// ============================================================

typedef void (*Mesh_DataReceivedCallback)(uint32_t sourceUid, const uint8_t* data, uint16_t length);
typedef void (*Mesh_NodeDiscoveredCallback)(const MeshNode* node);
typedef void (*Mesh_NodeLostCallback)(uint32_t uid);

void mesh_protocol_set_data_callback(Mesh_DataReceivedCallback callback);
void mesh_protocol_set_node_discovered_callback(Mesh_NodeDiscoveredCallback callback);
void mesh_protocol_set_node_lost_callback(Mesh_NodeLostCallback callback);

// ============================================================
// SECTION 13 : FONCTIONS DE DÉBOGAGE
// ============================================================

void mesh_protocol_print_state(void);
void mesh_protocol_print_routes(void);
void mesh_protocol_print_cache(void);
void mesh_protocol_print_statistics(void);
bool mesh_protocol_self_test(void);

// ============================================================
// SECTION 14 : MACROS DE DÉBOGAGE
// ============================================================

#if ENABLE_DEBUG
    #define MESH_DEBUG(fmt, ...)        printf("[MESH] " fmt, ##__VA_ARGS__)
#else
    #define MESH_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // MESH_PROTOCOL_H