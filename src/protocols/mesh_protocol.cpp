/**
 * @file mesh_protocol.cpp
 * @brief Implémentation du protocole de réseau maillé (Mesh)
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans mesh_protocol.h.
 * 
 * Il gère :
 * - Le routage par inondation contrôlée
 * - La table de routage simplifiée
 * - Le cache anti-doublons
 * - Le relais des paquets
 * - La découverte des nœuds
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "mesh_protocol.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du protocole mesh */
static MeshProtocolState mesh_state;

/** @brief Callbacks */
static Mesh_DataReceivedCallback data_received_cb = NULL;
static Mesh_NodeDiscoveredCallback node_discovered_cb = NULL;
static Mesh_NodeLostCallback node_lost_cb = NULL;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le protocole mesh
 */
bool mesh_protocol_init(void)
{
    MESH_DEBUG("Initialisation du protocole mesh...\n");
    
    memset(&mesh_state, 0, sizeof(MeshProtocolState));
    
    mesh_state.role = MESH_ROLE_NODE;
    mesh_state.defaultTtl = MESH_DEFAULT_TTL;
    mesh_state.beaconIntervalS = MESH_BEACON_INTERVAL_S;
    mesh_state.enabled = false;
    
    mesh_state.initialized = true;
    
    MESH_DEBUG("Protocole mesh initialisé\n");
    return true;
}

void mesh_protocol_deinit(void)
{
    mesh_state.initialized = false;
}

bool mesh_protocol_is_ready(void)
{
    return mesh_state.initialized;
}

// ============================================================
// SECTION 2 : CONTRÔLE
// ============================================================

void mesh_protocol_enable(bool enable)
{
    mesh_state.enabled = enable;
    MESH_DEBUG("Mesh %s\n", enable ? "activé" : "désactivé");
}

bool mesh_protocol_is_enabled(void)
{
    return mesh_state.enabled;
}

void mesh_protocol_set_role(MeshRole role)
{
    mesh_state.role = role;
    MESH_DEBUG("Rôle : %d\n", role);
}

MeshRole mesh_protocol_get_role(void)
{
    return mesh_state.role;
}

void mesh_protocol_set_ttl(uint8_t ttl)
{
    if (ttl > MESH_MAX_TTL) ttl = MESH_MAX_TTL;
    mesh_state.defaultTtl = ttl;
}

// ============================================================
// SECTION 3 : ENVOI
// ============================================================

/**
 * @brief Envoie un paquet via le maillage
 */
bool mesh_protocol_send(uint32_t destinationUid, const uint8_t* data, uint16_t length)
{
    if (!mesh_state.initialized || !mesh_state.enabled) return false;
    if (data == NULL || length == 0) return false;
    if (length > 200) length = 200;  // Limite payload
    
    MESH_DEBUG("Envoi mesh vers 0x%08lX (%d octets)\n", (unsigned long)destinationUid, length);
    
    // Construire le paquet mesh
    MeshPacket packet;
    build_mesh_packet(&packet, MESH_MSG_DATA, destinationUid, data, length);
    
    // Envoyer via LoRa
    if (send_mesh_packet(&packet))
    {
        mesh_state.packetsOriginated++;
        
        // Ajouter au cache local
        mesh_protocol_cache_add(packet.sourceUid, packet.sequenceNumber);
        
        return true;
    }
    
    return false;
}

/**
 * @brief Envoie un broadcast sur le maillage
 */
bool mesh_protocol_send_broadcast(const uint8_t* data, uint16_t length)
{
    return mesh_protocol_send(0xFFFFFFFF, data, length);  // UID broadcast
}

/**
 * @brief Envoie vers un numéro MSISDN
 */
bool mesh_protocol_send_to_msisdn(const char* msisdn, const uint8_t* data, uint16_t length)
{
    // Chercher l'UID correspondant au MSISDN
    MeshNode* node = find_node_by_msisdn(msisdn);
    if (node == NULL)
    {
        MESH_DEBUG("Nœud %s inconnu, broadcast\n", msisdn);
        return mesh_protocol_send_broadcast(data, length);
    }
    
    return mesh_protocol_send(node->uid, data, length);
}

/**
 * @brief Construit un paquet mesh
 */
static void build_mesh_packet(MeshPacket* packet, MeshMessageType type, 
                               uint32_t destinationUid, const uint8_t* data, uint16_t length)
{
    if (packet == NULL) return;
    
    memset(packet, 0, sizeof(MeshPacket));
    
    DeviceIdentity* identity = identity_get();
    
    packet->messageType = type;
    packet->ttl = mesh_state.defaultTtl;
    packet->sequenceNumber = mesh_state.sequenceNumber++;
    packet->sourceUid = identity->uid;
    packet->destinationUid = destinationUid;
    packet->previousHopUid = identity->uid;
    packet->payloadLength = length;
    
    if (data != NULL && length > 0)
    {
        memcpy(packet->payload, data, length);
    }
    
    // Calculer le CRC
    packet->crc = calculate_mesh_crc(packet);
}

/**
 * @brief Envoie un paquet mesh via LoRa
 */
static bool send_mesh_packet(const MeshPacket* packet)
{
    if (packet == NULL) return false;
    
    LoRaPacket loraPacket;
    memset(&loraPacket, 0, sizeof(LoRaPacket));
    
    loraPacket.type = PACKET_DISCOVERY;  // Réutiliser le type découverte ou créer un type dédié
    loraPacket.packetId = packet->sequenceNumber;
    snprintf(loraPacket.sender, 16, "%08lX", (unsigned long)packet->sourceUid);
    snprintf(loraPacket.receiver, 16, "%08lX", (unsigned long)packet->destinationUid);
    loraPacket.dataLength = sizeof(MeshPacket);
    memcpy(loraPacket.data, packet, sizeof(MeshPacket));
    
    return lora_driver_send_packet(&loraPacket, false);
}

// ============================================================
// SECTION 4 : RÉCEPTION/RELAIS
// ============================================================

/**
 * @brief Traitement périodique
 */
void mesh_protocol_process(void)
{
    if (!mesh_state.initialized || !mesh_state.enabled) return;
    
    uint32_t now = HAL_GetTick();
    
    // Envoyer un beacon périodique
    if ((now - mesh_state.lastBeaconTime) >= (mesh_state.beaconIntervalS * 1000))
    {
        mesh_protocol_send_beacon();
        mesh_state.lastBeaconTime = now;
    }
    
    // Maintenance des routes
    if ((now - mesh_state.lastRouteMaintenance) >= 60000)  // Toutes les 60 secondes
    {
        mesh_protocol_maintain_routes();
        mesh_state.lastRouteMaintenance = now;
    }
    
    // Nettoyage du cache
    mesh_protocol_cache_cleanup();
}

/**
 * @brief Traite un paquet mesh reçu
 */
void mesh_protocol_process_packet(const MeshPacket* packet, int16_t rssi)
{
    if (packet == NULL) return;
    
    // Vérifier le CRC
    uint16_t computedCrc = calculate_mesh_crc(packet);
    if (computedCrc != packet->crc)
    {
        MESH_DEBUG("CRC invalide\n");
        return;
    }
    
    // Vérifier si c'est un doublon
    if (mesh_protocol_is_duplicate(packet->sourceUid, packet->sequenceNumber))
    {
        MESH_DEBUG("Paquet dupliqué, ignoré\n");
        mesh_state.packetsDropped++;
        return;
    }
    
    // Ajouter au cache
    mesh_protocol_cache_add(packet->sourceUid, packet->sequenceNumber);
    
    DeviceIdentity* identity = identity_get();
    
    // Vérifier si ce paquet nous est destiné
    if (packet->destinationUid == identity->uid || 
        packet->destinationUid == 0xFFFFFFFF)  // Broadcast
    {
        // C'est pour nous !
        MESH_DEBUG("Paquet reçu pour nous (%d octets)\n", packet->payloadLength);
        
        // Mettre à jour la table de routage (chemin inverse)
        mesh_protocol_add_route(packet->sourceUid, packet->previousHopUid, 
                                MESH_MAX_TTL - packet->ttl + 1);
        
        // Ajouter/mettre à jour le nœud source
        add_or_update_node(packet->sourceUid, rssi);
        
        // Notifier l'application
        if (data_received_cb)
        {
            data_received_cb(packet->sourceUid, packet->payload, packet->payloadLength);
        }
        
        return;
    }
    
    // Ce n'est pas pour nous, faut-il relayer ?
    if (mesh_protocol_should_relay(packet))
    {
        mesh_protocol_relay_packet(packet);
    }
}

/**
 * @brief Vérifie si on doit relayer le paquet
 */
bool mesh_protocol_should_relay(const MeshPacket* packet)
{
    // Ne pas relayer si le TTL est épuisé
    if (packet->ttl <= 1) return false;
    
    // Ne pas relayer si on n'est pas un nœud relais
    if (mesh_state.role != MESH_ROLE_RELAY && 
        mesh_state.role != MESH_ROLE_GATEWAY &&
        mesh_state.role != MESH_ROLE_COORDINATOR)
    {
        return false;
    }
    
    // Vérifier si on a déjà relayé ce paquet
    if (mesh_protocol_is_duplicate(packet->sourceUid, packet->sequenceNumber))
    {
        return false;
    }
    
    return true;
}

/**
 * @brief Relaie un paquet vers sa destination
 */
void mesh_protocol_relay_packet(const MeshPacket* packet)
{
    MESH_DEBUG("Relais paquet (TTL=%d)\n", packet->ttl);
    
    // Créer une copie du paquet avec TTL décrémenté
    MeshPacket relayPacket;
    memcpy(&relayPacket, packet, sizeof(MeshPacket));
    
    relayPacket.ttl--;
    relayPacket.previousHopUid = identity_get()->uid;
    relayPacket.crc = calculate_mesh_crc(&relayPacket);
    
    // Envoyer
    if (send_mesh_packet(&relayPacket))
    {
        mesh_state.packetsRelayed++;
        
        // Mettre à jour la table de routage
        mesh_protocol_add_route(packet->sourceUid, packet->previousHopUid, 
                                MESH_MAX_TTL - packet->ttl);
    }
}

// ============================================================
// SECTION 5 : ROUTAGE
// ============================================================

bool mesh_protocol_add_route(uint32_t destination, uint32_t nextHop, uint8_t hops)
{
    // Chercher si la route existe déjà
    for (uint8_t i = 0; i < mesh_state.routeCount; i++)
    {
        if (mesh_state.routes[i].destinationUid == destination)
        {
            // Mettre à jour si meilleure (moins de sauts)
            if (hops < mesh_state.routes[i].hopCount)
            {
                mesh_state.routes[i].nextHopUid = nextHop;
                mesh_state.routes[i].hopCount = hops;
                mesh_state.routes[i].lastUpdated = HAL_GetTick();
            }
            return true;
        }
    }
    
    // Ajouter une nouvelle route
    if (mesh_state.routeCount >= MESH_MAX_ROUTES)
    {
        // Supprimer la plus ancienne
        remove_oldest_route();
    }
    
    MeshRoute* route = &mesh_state.routes[mesh_state.routeCount++];
    route->destinationUid = destination;
    route->nextHopUid = nextHop;
    route->hopCount = hops;
    route->signalQuality = 50;
    route->lastUpdated = HAL_GetTick();
    route->valid = true;
    
    MESH_DEBUG("Route ajoutée : 0x%08lX → 0x%08lX (hops=%d)\n",
              (unsigned long)destination, (unsigned long)nextHop, hops);
    
    return true;
}

bool mesh_protocol_remove_route(uint32_t destination)
{
    for (uint8_t i = 0; i < mesh_state.routeCount; i++)
    {
        if (mesh_state.routes[i].destinationUid == destination)
        {
            mesh_state.routes[i].valid = false;
            return true;
        }
    }
    return false;
}

MeshRoute* mesh_protocol_find_route(uint32_t destination)
{
    for (uint8_t i = 0; i < mesh_state.routeCount; i++)
    {
        if (mesh_state.routes[i].destinationUid == destination &&
            mesh_state.routes[i].valid)
        {
            return &mesh_state.routes[i];
        }
    }
    return NULL;
}

uint32_t mesh_protocol_get_next_hop(uint32_t destination)
{
    MeshRoute* route = mesh_protocol_find_route(destination);
    return route ? route->nextHopUid : 0;
}

void mesh_protocol_clear_routes(void)
{
    mesh_state.routeCount = 0;
    memset(mesh_state.routes, 0, sizeof(mesh_state.routes));
}

void mesh_protocol_maintain_routes(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t timeoutMs = MESH_ROUTE_TIMEOUT_S * 1000;
    
    for (uint8_t i = 0; i < mesh_state.routeCount; i++)
    {
        if (mesh_state.routes[i].valid)
        {
            uint32_t age = now - mesh_state.routes[i].lastUpdated;
            if (age > timeoutMs)
            {
                mesh_state.routes[i].valid = false;
                MESH_DEBUG("Route expirée : 0x%08lX\n", 
                          (unsigned long)mesh_state.routes[i].destinationUid);
            }
        }
    }
}

// ============================================================
// SECTION 6 : CACHE ANTI-DOUBLONS
// ============================================================

bool mesh_protocol_is_duplicate(uint32_t sourceUid, uint16_t sequenceNumber)
{
    for (uint8_t i = 0; i < mesh_state.cacheCount; i++)
    {
        if (mesh_state.cache[i].sourceUid == sourceUid &&
            mesh_state.cache[i].sequenceNumber == sequenceNumber)
        {
            return true;
        }
    }
    return false;
}

void mesh_protocol_cache_add(uint32_t sourceUid, uint16_t sequenceNumber)
{
    // Ajouter à la position head (buffer circulaire)
    mesh_state.cache[mesh_state.cacheHead].sourceUid = sourceUid;
    mesh_state.cache[mesh_state.cacheHead].sequenceNumber = sequenceNumber;
    mesh_state.cache[mesh_state.cacheHead].timestamp = HAL_GetTick();
    
    mesh_state.cacheHead = (mesh_state.cacheHead + 1) % MESH_CACHE_SIZE;
    
    if (mesh_state.cacheCount < MESH_CACHE_SIZE)
    {
        mesh_state.cacheCount++;
    }
}

void mesh_protocol_cache_cleanup(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t timeoutMs = MESH_CACHE_TIMEOUT_S * 1000;
    
    for (uint8_t i = 0; i < mesh_state.cacheCount; i++)
    {
        uint8_t index = (mesh_state.cacheHead - mesh_state.cacheCount + i) % MESH_CACHE_SIZE;
        
        if (mesh_state.cache[index].sourceUid != 0)
        {
            uint32_t age = now - mesh_state.cache[index].timestamp;
            if (age > timeoutMs)
            {
                mesh_state.cache[index].sourceUid = 0;
            }
        }
    }
}

// ============================================================
// SECTION 7 : VOISINAGE
// ============================================================

void mesh_protocol_send_beacon(void)
{
    if (!mesh_state.enabled) return;
    
    MeshPacket beacon;
    DeviceIdentity* identity = identity_get();
    
    build_mesh_packet(&beacon, MESH_MSG_BEACON, 0xFFFFFFFF, 
                      (uint8_t*)identity->deviceName, strlen(identity->deviceName));
    
    send_mesh_packet(&beacon);
}

void mesh_protocol_send_hello(void)
{
    MeshPacket hello;
    build_mesh_packet(&hello, MESH_MSG_HELLO, 0xFFFFFFFF, NULL, 0);
    send_mesh_packet(&hello);
}

uint8_t mesh_protocol_get_node_count(void)
{
    return mesh_state.nodeCount;
}

uint8_t mesh_protocol_get_online_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < mesh_state.nodeCount; i++)
    {
        if (mesh_state.nodes[i].online) count++;
    }
    return count;
}

MeshNode* mesh_protocol_find_node(uint32_t uid)
{
    for (uint8_t i = 0; i < mesh_state.nodeCount; i++)
    {
        if (mesh_state.nodes[i].uid == uid) return &mesh_state.nodes[i];
    }
    return NULL;
}

// ============================================================
// SECTION 8 : CALLBACKS
// ============================================================

void mesh_protocol_set_data_callback(Mesh_DataReceivedCallback cb) { data_received_cb = cb; }
void mesh_protocol_set_node_discovered_callback(Mesh_NodeDiscoveredCallback cb) { node_discovered_cb = cb; }
void mesh_protocol_set_node_lost_callback(Mesh_NodeLostCallback cb) { node_lost_cb = cb; }

// ============================================================
// SECTION 9 : FONCTIONS INTERNES
// ============================================================

static uint16_t calculate_mesh_crc(const MeshPacket* packet)
{
    if (packet == NULL) return 0;
    
    uint16_t crc = 0xFFFF;
    uint8_t* data = (uint8_t*)packet;
    uint16_t size = sizeof(MeshPacket) - 2;  // Exclure le CRC
    
    for (uint16_t i = 0; i < size; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    
    return crc;
}

static void add_or_update_node(uint32_t uid, int16_t rssi)
{
    MeshNode* node = mesh_protocol_find_node(uid);
    
    if (node == NULL)
    {
        if (mesh_state.nodeCount >= MESH_MAX_NODES) return;
        
        node = &mesh_state.nodes[mesh_state.nodeCount++];
        memset(node, 0, sizeof(MeshNode));
        node->uid = uid;
        node->online = true;
        
        if (node_discovered_cb) node_discovered_cb(node);
    }
    
    node->rssi = rssi;
    node->lastSeen = HAL_GetTick();
    node->online = true;
}

static MeshNode* find_node_by_msisdn(const char* msisdn)
{
    for (uint8_t i = 0; i < mesh_state.nodeCount; i++)
    {
        if (strcmp(mesh_state.nodes[i].msisdn, msisdn) == 0)
            return &mesh_state.nodes[i];
    }
    return NULL;
}

static void remove_oldest_route(void)
{
    uint32_t oldest = 0xFFFFFFFF;
    uint8_t oldestIndex = 0;
    
    for (uint8_t i = 0; i < mesh_state.routeCount; i++)
    {
        if (mesh_state.routes[i].lastUpdated < oldest)
        {
            oldest = mesh_state.routes[i].lastUpdated;
            oldestIndex = i;
        }
    }
    
    // Supprimer la plus ancienne
    if (oldestIndex < mesh_state.routeCount - 1)
    {
        memmove(&mesh_state.routes[oldestIndex], &mesh_state.routes[oldestIndex + 1],
                (mesh_state.routeCount - oldestIndex - 1) * sizeof(MeshRoute));
    }
    mesh_state.routeCount--;
}

// ============================================================
// SECTION 10 : DÉBOGAGE
// ============================================================

void mesh_protocol_print_state(void)
{
    printf("\n═══ ÉTAT PROTOCOLE MESH ═══\n");
    printf("Activé       : %s\n", mesh_state.enabled ? "Oui" : "Non");
    printf("Rôle         : %d\n", mesh_state.role);
    printf("TTL défaut   : %d\n", mesh_state.defaultTtl);
    printf("Routes       : %d\n", mesh_state.routeCount);
    printf("Cache        : %d\n", mesh_state.cacheCount);
    printf("Nœuds        : %d (en ligne: %d)\n", mesh_state.nodeCount, 
           mesh_protocol_get_online_count());
    printf("Relayés      : %lu\n", (unsigned long)mesh_state.packetsRelayed);
    printf("Créés        : %lu\n", (unsigned long)mesh_state.packetsOriginated);
    printf("Ignorés      : %lu\n", (unsigned long)mesh_state.packetsDropped);
    printf("══════════════════════════\n\n");
}

void mesh_protocol_print_routes(void)
{
    printf("\n═══ TABLE DE ROUTAGE (%d) ═══\n", mesh_state.routeCount);
    printf("%-12s %-12s %-6s %-8s\n", "Destination", "Next Hop", "Hops", "Âge(s)");
    printf("──────────────────────────────────────────\n");
    
    for (uint8_t i = 0; i < mesh_state.routeCount; i++)
    {
        MeshRoute* r = &mesh_state.routes[i];
        if (!r->valid) continue;
        
        uint32_t age = (HAL_GetTick() - r->lastUpdated) / 1000;
        
        printf("0x%08lX  0x%08lX  %-4d  %-6lu\n",
               (unsigned long)r->destinationUid,
               (unsigned long)r->nextHopUid,
               r->hopCount, (unsigned long)age);
    }
    printf("══════════════════════════════\n\n");
}

void mesh_protocol_print_cache(void)
{
    printf("\n═══ CACHE (%d entrées) ═══\n", mesh_state.cacheCount);
    
    for (uint8_t i = 0; i < mesh_state.cacheCount; i++)
    {
        uint8_t index = (mesh_state.cacheHead - mesh_state.cacheCount + i) % MESH_CACHE_SIZE;
        if (mesh_state.cache[index].sourceUid != 0)
        {
            printf("[%d] Src=0x%08lX Seq=%d\n", i,
                   (unsigned long)mesh_state.cache[index].sourceUid,
                   mesh_state.cache[index].sequenceNumber);
        }
    }
    printf("══════════════════════════\n\n");
}

void mesh_protocol_print_statistics(void)
{
    printf("\n═══ STATISTIQUES MESH ═══\n");
    printf("Relayés  : %lu\n", (unsigned long)mesh_state.packetsRelayed);
    printf("Créés    : %lu\n", (unsigned long)mesh_state.packetsOriginated);
    printf("Ignorés  : %lu\n", (unsigned long)mesh_state.packetsDropped);
    printf("════════════════════════\n\n");
}

void mesh_protocol_print_nodes(void)
{
    printf("\n═══ NŒUDS DU MAILLAGE (%d) ═══\n", mesh_state.nodeCount);
    printf("%-12s %-16s %-5s %-8s\n", "UID", "Nom", "Sauts", "RSSI");
    printf("──────────────────────────────────────────\n");
    
    for (uint8_t i = 0; i < mesh_state.nodeCount; i++)
    {
        MeshNode* n = &mesh_state.nodes[i];
        printf("0x%08lX  %-16s %-4d  %-5d dBm\n",
               (unsigned long)n->uid, n->name, n->hopCount, n->rssi);
    }
    printf("══════════════════════════════════\n\n");
}

bool mesh_protocol_self_test(void)
{
    MESH_DEBUG("Auto-test...\n");
    
    if (!mesh_state.initialized)
    {
        MESH_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : construire un paquet
    uint8_t testData[] = "Test Mesh";
    MeshPacket packet;
    build_mesh_packet(&packet, MESH_MSG_DATA, 0x12345678, testData, sizeof(testData));
    
    if (packet.ttl != mesh_state.defaultTtl)
    {
        MESH_DEBUG("Échec : TTL incorrect\n");
        return false;
    }
    
    // Test : ajouter une route
    mesh_protocol_add_route(0xAABBCCDD, 0x11223344, 2);
    MeshRoute* route = mesh_protocol_find_route(0xAABBCCDD);
    if (route == NULL || route->nextHopUid != 0x11223344)
    {
        MESH_DEBUG("Échec : route incorrecte\n");
        return false;
    }
    
    mesh_protocol_clear_routes();
    
    MESH_DEBUG("Auto-test OK\n");
    return true;
}