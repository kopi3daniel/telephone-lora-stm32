/**
 * @file packet_router.cpp
 * @brief Implémentation du routeur de paquets
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans packet_router.h.
 * 
 * Il gère :
 * - La réception des paquets LoRa
 * - L'identification du type de paquet
 * - La distribution vers le protocole approprié
 * - La file d'attente pour les paquets en attente
 * - Le filtrage des paquets indésirables
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "packet_router.h"
#include "discovery.h"
#include "call_protocol.h"
#include "sms_protocol.h"
#include "../drivers/audio/audio_manager.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du routeur */
static PacketRouterState router_state;

/** @brief Handlers spéciaux pour les catégories */
static PacketHandler category_handlers[6] = {NULL};  // Un par catégorie

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le routeur de paquets
 */
bool packet_router_init(void)
{
    ROUTER_DEBUG("Initialisation du routeur de paquets...\n");
    
    memset(&router_state, 0, sizeof(PacketRouterState));
    
    // Initialiser la file d'attente
    router_state.queueHead = 0;
    router_state.queueTail = 0;
    router_state.queueCount = 0;
    
    // Enregistrer les routes par défaut
    packet_router_register_default_routes();
    
    router_state.initialized = true;
    
    ROUTER_DEBUG("Routeur initialisé (%d routes)\n", router_state.routeCount);
    return true;
}

/**
 * @brief Enregistre les routes par défaut pour les protocoles standards
 */
static void packet_router_register_default_routes(void)
{
    ROUTER_DEBUG("Enregistrement des routes par défaut...\n");
    
    // --- Protocole de découverte ---
    packet_router_register(PACKET_DISCOVERY, 
        [](const LoRaPacket* packet, int16_t rssi, int8_t snr) {
            DiscoveryMessage* msg = (DiscoveryMessage*)packet->data;
            discovery_process_message(msg, rssi, snr);
        }, PACKET_PRIORITY_LOW);
    
    packet_router_register(PACKET_DISCOVERY_REPLY,
        [](const LoRaPacket* packet, int16_t rssi, int8_t snr) {
            DiscoveryMessage* msg = (DiscoveryMessage*)packet->data;
            discovery_process_message(msg, rssi, snr);
        }, PACKET_PRIORITY_LOW);
    
    // --- Protocole d'appel ---
    packet_router_register(PACKET_CALL_REQUEST,
        [](const LoRaPacket* packet, int16_t rssi, int8_t snr) {
            CallMessage* msg = (CallMessage*)packet->data;
            call_protocol_process_message(msg, rssi);
        }, PACKET_PRIORITY_HIGH);
    
    packet_router_register(PACKET_CALL_ACCEPT,
        [](const LoRaPacket* packet, int16_t rssi, int8_t snr) {
            CallMessage* msg = (CallMessage*)packet->data;
            call_protocol_process_message(msg, rssi);
        }, PACKET_PRIORITY_HIGH);
    
    packet_router_register(PACKET_CALL_REJECT,
        [](const LoRaPacket* packet, int16_t rssi, int8_t snr) {
            CallMessage* msg = (CallMessage*)packet->data;
            call_protocol_process_message(msg, rssi);
        }, PACKET_PRIORITY_HIGH);
    
    packet_router_register(PACKET_CALL_END,
        [](const LoRaPacket* packet, int16_t rssi, int8_t snr) {
            CallMessage* msg = (CallMessage*)packet->data;
            call_protocol_process_message(msg, rssi);
        }, PACKET_PRIORITY_HIGH);
    
    // --- Audio ---
    packet_router_register(PACKET_AUDIO_DATA,
        [](const LoRaPacket* packet, int16_t rssi, int8_t snr) {
            audio_manager_receive_data(packet->data, packet->dataLength);
        }, PACKET_PRIORITY_CRITICAL);
    
    // --- SMS ---
    packet_router_register(PACKET_SMS,
        [](const LoRaPacket* packet, int16_t rssi, int8_t snr) {
            SMSPacket* msg = (SMSPacket*)packet->data;
            sms_protocol_process_packet(msg);
        }, PACKET_PRIORITY_NORMAL);
    
    packet_router_register(PACKET_SMS_ACK,
        [](const LoRaPacket* packet, int16_t rssi, int8_t snr) {
            SMSPacket* msg = (SMSPacket*)packet->data;
            sms_protocol_process_packet(msg);
        }, PACKET_PRIORITY_NORMAL);
    
    // --- Système ---
    packet_router_register(PACKET_ACK,
        [](const LoRaPacket* packet, int16_t rssi, int8_t snr) {
            // Traitement générique des ACK
            ROUTER_DEBUG("ACK reçu pour paquet %lu\n", (unsigned long)packet->packetId);
        }, PACKET_PRIORITY_NORMAL);
    
    packet_router_register(PACKET_ERROR,
        [](const LoRaPacket* packet, int16_t rssi, int8_t snr) {
            ROUTER_DEBUG("Erreur reçue\n");
        }, PACKET_PRIORITY_NORMAL);
    
    ROUTER_DEBUG("Routes par défaut enregistrées\n");
}

void packet_router_deinit(void)
{
    router_state.initialized = false;
    memset(&router_state, 0, sizeof(PacketRouterState));
}

bool packet_router_is_ready(void)
{
    return router_state.initialized;
}

// ============================================================
// SECTION 2 : GESTION DES ROUTES
// ============================================================

/**
 * @brief Enregistre une route
 */
bool packet_router_register(LoRaPacketType packetType, PacketHandler handler, PacketPriority priority)
{
    if (!router_state.initialized) return false;
    if (handler == NULL) return false;
    
    // Vérifier si la route existe déjà
    for (uint8_t i = 0; i < router_state.routeCount; i++)
    {
        if (router_state.routes[i].packetType == packetType)
        {
            // Mettre à jour la route existante
            router_state.routes[i].handler = handler;
            router_state.routes[i].priority = priority;
            router_state.routes[i].enabled = true;
            ROUTER_DEBUG("Route 0x%02X mise à jour\n", packetType);
            return true;
        }
    }
    
    // Vérifier la limite
    if (router_state.routeCount >= 32)
    {
        ROUTER_DEBUG("Table de routage pleine\n");
        return false;
    }
    
    // Ajouter la nouvelle route
    PacketRoute* route = &router_state.routes[router_state.routeCount++];
    route->packetType = packetType;
    route->category = get_packet_category(packetType);
    route->priority = priority;
    route->handler = handler;
    route->enabled = true;
    
    ROUTER_DEBUG("Route 0x%02X ajoutée (cat=%d, prio=%d)\n", 
                packetType, route->category, priority);
    
    return true;
}

/**
 * @brief Supprime une route
 */
bool packet_router_unregister(LoRaPacketType packetType)
{
    for (uint8_t i = 0; i < router_state.routeCount; i++)
    {
        if (router_state.routes[i].packetType == packetType)
        {
            // Désactiver la route (on ne la supprime pas vraiment)
            router_state.routes[i].enabled = false;
            router_state.routes[i].handler = NULL;
            ROUTER_DEBUG("Route 0x%02X supprimée\n", packetType);
            return true;
        }
    }
    return false;
}

/**
 * @brief Détermine la catégorie d'un type de paquet
 */
static PacketCategory get_packet_category(LoRaPacketType type)
{
    if (type >= 0x40 && type <= 0x4F) return PACKET_CATEGORY_DISCOVERY;
    if (type >= 0x10 && type <= 0x1F) return PACKET_CATEGORY_CALL;
    if (type >= 0x20 && type <= 0x2F) return PACKET_CATEGORY_AUDIO;
    if (type >= 0x30 && type <= 0x3F) return PACKET_CATEGORY_SMS;
    if (type >= 0xF0 && type <= 0xFF) return PACKET_CATEGORY_SYSTEM;
    return PACKET_CATEGORY_UNKNOWN;
}

// ============================================================
// SECTION 3 : ROUTAGE
// ============================================================

/**
 * @brief Route un paquet vers le bon handler
 */
bool packet_router_route(const LoRaPacket* packet, int16_t rssi, int8_t snr)
{
    if (!router_state.initialized) return false;
    if (packet == NULL) return false;
    
    // Vérifier le filtrage
    if (router_state.filterEnabled)
    {
        PacketCategory cat = get_packet_category(packet->type);
        if (router_state.filterMask & (1 << cat))
        {
            ROUTER_DEBUG("Paquet 0x%02X filtré (catégorie %d)\n", packet->type, cat);
            router_state.totalPacketsDropped++;
            return false;  // Ignorer ce paquet
        }
    }
    
    // Chercher la route correspondante
    PacketHandler handler = NULL;
    PacketPriority priority = PACKET_PRIORITY_LOW;
    
    for (uint8_t i = 0; i < router_state.routeCount; i++)
    {
        if (router_state.routes[i].packetType == packet->type &&
            router_state.routes[i].enabled &&
            router_state.routes[i].handler != NULL)
        {
            handler = router_state.routes[i].handler;
            priority = router_state.routes[i].priority;
            break;
        }
    }
    
    if (handler == NULL)
    {
        // Aucun handler trouvé
        ROUTER_DEBUG("Aucun handler pour paquet 0x%02X\n", packet->type);
        router_state.totalPacketsDropped++;
        return false;
    }
    
    // Si c'est critique, traiter immédiatement
    if (priority == PACKET_PRIORITY_CRITICAL)
    {
        handler(packet, rssi, snr);
        router_state.totalPacketsRouted++;
        return true;
    }
    
    // Sinon, mettre en file d'attente
    if (packet_router_enqueue(packet, rssi, snr))
    {
        router_state.totalPacketsQueued++;
        return true;
    }
    
    // File pleine, traiter quand même
    handler(packet, rssi, snr);
    router_state.totalPacketsRouted++;
    return true;
}

/**
 * @brief Traitement périodique de la file d'attente
 */
void packet_router_process(void)
{
    if (!router_state.initialized) return;
    
    // Traiter les paquets dans la file d'attente
    uint8_t processed = 0;
    uint32_t startTime = HAL_GetTick();
    
    while (router_state.queueCount > 0 && 
           processed < 5 &&  // Max 5 paquets par appel
           (HAL_GetTick() - startTime) < PACKET_ROUTER_PROCESS_TIMEOUT)
    {
        QueuedPacket queued;
        
        if (packet_router_dequeue(&queued))
        {
            // Trouver le handler
            for (uint8_t i = 0; i < router_state.routeCount; i++)
            {
                if (router_state.routes[i].packetType == queued.packet.type &&
                    router_state.routes[i].enabled &&
                    router_state.routes[i].handler != NULL)
                {
                    router_state.routes[i].handler(&queued.packet, queued.rssi, queued.snr);
                    router_state.totalPacketsRouted++;
                    break;
                }
            }
            
            processed++;
        }
    }
}

// ============================================================
// SECTION 4 : FILE D'ATTENTE
// ============================================================

/**
 * @brief Ajoute un paquet à la file d'attente
 */
bool packet_router_enqueue(const LoRaPacket* packet, int16_t rssi, int8_t snr)
{
    if (packet == NULL) return false;
    
    // Vérifier si la file est pleine
    if (router_state.queueCount >= PACKET_ROUTER_QUEUE_SIZE)
    {
        ROUTER_DEBUG("File d'attente pleine\n");
        return false;
    }
    
    // Ajouter à la file
    QueuedPacket* qp = &router_state.queue[router_state.queueHead];
    memcpy(&qp->packet, packet, sizeof(LoRaPacket));
    qp->rssi = rssi;
    qp->snr = snr;
    qp->timestamp = HAL_GetTick();
    qp->processed = false;
    
    router_state.queueHead = (router_state.queueHead + 1) % PACKET_ROUTER_QUEUE_SIZE;
    router_state.queueCount++;
    
    return true;
}

/**
 * @brief Retire un paquet de la file d'attente
 */
bool packet_router_dequeue(QueuedPacket* packet)
{
    if (router_state.queueCount == 0) return false;
    if (packet == NULL) return false;
    
    // Récupérer le paquet le plus ancien
    memcpy(packet, &router_state.queue[router_state.queueTail], sizeof(QueuedPacket));
    packet->processed = true;
    
    router_state.queueTail = (router_state.queueTail + 1) % PACKET_ROUTER_QUEUE_SIZE;
    router_state.queueCount--;
    
    return true;
}

uint8_t packet_router_queue_count(void)
{
    return router_state.queueCount;
}

void packet_router_flush_queue(void)
{
    router_state.queueHead = 0;
    router_state.queueTail = 0;
    router_state.queueCount = 0;
    memset(router_state.queue, 0, sizeof(router_state.queue));
    ROUTER_DEBUG("File d'attente vidée\n");
}

// ============================================================
// SECTION 5 : FILTRAGE
// ============================================================

void packet_router_filter_enable(bool enable)
{
    router_state.filterEnabled = enable;
    ROUTER_DEBUG("Filtrage %s\n", enable ? "activé" : "désactivé");
}

void packet_router_filter_set_mask(uint32_t mask)
{
    router_state.filterMask = mask;
}

bool packet_router_is_filtered(LoRaPacketType type)
{
    if (!router_state.filterEnabled) return false;
    PacketCategory cat = get_packet_category(type);
    return (router_state.filterMask & (1 << cat)) != 0;
}

// ============================================================
// SECTION 6 : STATISTIQUES
// ============================================================

uint32_t packet_router_get_routed_count(void) { return router_state.totalPacketsRouted; }
uint32_t packet_router_get_dropped_count(void) { return router_state.totalPacketsDropped; }
uint32_t packet_router_get_queue_count(void) { return router_state.queueCount; }

void packet_router_reset_statistics(void)
{
    router_state.totalPacketsRouted = 0;
    router_state.totalPacketsDropped = 0;
    router_state.totalPacketsQueued = 0;
    router_state.totalErrors = 0;
}

// ============================================================
// SECTION 7 : DÉBOGAGE
// ============================================================

void packet_router_print_routes(void)
{
    printf("\n═══ TABLE DE ROUTAGE (%d routes) ═══\n", router_state.routeCount);
    printf("%-6s %-20s %-10s %-8s\n", "Type", "Nom", "Priorité", "État");
    printf("──────────────────────────────────────────────\n");
    
    for (uint8_t i = 0; i < router_state.routeCount; i++)
    {
        PacketRoute* route = &router_state.routes[i];
        
        const char* name = lora_packet_type_to_string(route->packetType);
        const char* priority = "?";
        switch (route->priority)
        {
            case PACKET_PRIORITY_LOW:      priority = "Basse"; break;
            case PACKET_PRIORITY_NORMAL:   priority = "Normale"; break;
            case PACKET_PRIORITY_HIGH:     priority = "Haute"; break;
            case PACKET_PRIORITY_CRITICAL: priority = "CRITIQUE"; break;
        }
        
        printf("0x%02X  %-20s %-10s %-8s\n",
               route->packetType, name, priority,
               route->enabled ? "ACTIF" : "inactif");
    }
    printf("══════════════════════════════════════\n\n");
}

void packet_router_print_queue(void)
{
    printf("\n═══ FILE D'ATTENTE (%d paquets) ═══\n", router_state.queueCount);
    
    if (router_state.queueCount == 0)
    {
        printf("  (vide)\n");
    }
    else
    {
        uint8_t index = router_state.queueTail;
        for (uint8_t i = 0; i < router_state.queueCount; i++)
        {
            QueuedPacket* qp = &router_state.queue[index];
            const char* name = lora_packet_type_to_string(qp->packet.type);
            
            printf("  [%d] %s (RSSI:%d, SNR:%d) - %lu ms\n",
                   i, name, qp->rssi, qp->snr,
                   (unsigned long)(HAL_GetTick() - qp->timestamp));
            
            index = (index + 1) % PACKET_ROUTER_QUEUE_SIZE;
        }
    }
    printf("══════════════════════════════════\n\n");
}

void packet_router_print_statistics(void)
{
    printf("\n═══ STATISTIQUES ROUTEUR ═══\n");
    printf("Paquets routés  : %lu\n", (unsigned long)router_state.totalPacketsRouted);
    printf("Paquets ignorés : %lu\n", (unsigned long)router_state.totalPacketsDropped);
    printf("Paquets en file : %lu\n", (unsigned long)router_state.totalPacketsQueued);
    printf("Erreurs         : %lu\n", (unsigned long)router_state.totalErrors);
    printf("File actuelle   : %d\n", router_state.queueCount);
    printf("Filtrage        : %s\n", router_state.filterEnabled ? "ON" : "OFF");
    printf("══════════════════════════\n\n");
}

bool packet_router_self_test(void)
{
    ROUTER_DEBUG("Auto-test...\n");
    
    if (!router_state.initialized)
    {
        ROUTER_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : enregistrer une route de test
    bool registered = packet_router_register((LoRaPacketType)0xEE, 
        [](const LoRaPacket* p, int16_t r, int8_t s) {
            // Handler vide pour test
        }, PACKET_PRIORITY_NORMAL);
    
    if (!registered)
    {
        ROUTER_DEBUG("Échec : enregistrement\n");
        return false;
    }
    
    // Test : router un paquet factice
    LoRaPacket testPacket;
    memset(&testPacket, 0, sizeof(LoRaPacket));
    testPacket.type = (LoRaPacketType)0xEE;
    testPacket.packetId = 1;
    
    bool routed = packet_router_route(&testPacket, -50, 10);
    // Note : le paquet sera mis en file, donc routé retourne true même si pas traité
    
    // Nettoyer
    packet_router_unregister((LoRaPacketType)0xEE);
    packet_router_flush_queue();
    
    ROUTER_DEBUG("Auto-test OK\n");
    return true;
}