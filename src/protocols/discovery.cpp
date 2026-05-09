/**
 * @file discovery.cpp
 * @brief Implémentation du protocole de découverte réseau
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans discovery.h.
 * 
 * Il gère :
 * - L'envoi périodique de beacons de présence
 * - Le scan du réseau pour découvrir les pairs
 * - La gestion de la liste des pairs connus
 * - La détection des départs (timeout)
 * - Les callbacks de notification
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "discovery.h"
#include "../drivers/lora/lora_driver.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du module de découverte */
static DiscoveryState discovery_state;

/** @brief Callbacks */
static Discovery_NewPeerCallback new_peer_callback = NULL;
static Discovery_PeerStateCallback peer_state_callback = NULL;
static Discovery_PeerLostCallback peer_lost_callback = NULL;
static Discovery_ScanCompleteCallback scan_complete_callback = NULL;

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le module de découverte
 */
bool discovery_init(void)
{
    DISCOVERY_DEBUG("Initialisation du protocole de découverte...\n");
    
    memset(&discovery_state, 0, sizeof(DiscoveryState));
    
    discovery_state.beaconIntervalS = DISCOVERY_DEFAULT_BEACON_INTERVAL_S;
    discovery_state.presenceTimeoutS = DISCOVERY_PRESENCE_TIMEOUT_S;
    discovery_state.scanDurationMs = DISCOVERY_SCAN_DURATION_MS;
    discovery_state.enabled = false;
    discovery_state.scanning = false;
    
    discovery_state.initialized = true;
    
    DISCOVERY_DEBUG("Module initialisé (beacon=%ds, timeout=%ds)\n",
                   discovery_state.beaconIntervalS,
                   discovery_state.presenceTimeoutS);
    
    return true;
}

/**
 * @brief Désinitialise
 */
void discovery_deinit(void)
{
    discovery_state.enabled = false;
    discovery_state.initialized = false;
}

/**
 * @brief Vérifie si prêt
 */
bool discovery_is_ready(void)
{
    return discovery_state.initialized;
}

// ============================================================
// SECTION 2 : CONTRÔLE
// ============================================================

void discovery_enable(bool enable)
{
    discovery_state.enabled = enable;
    DISCOVERY_DEBUG("Découverte %s\n", enable ? "activée" : "désactivée");
}

bool discovery_is_enabled(void)
{
    return discovery_state.enabled;
}

void discovery_set_beacon_interval(uint16_t seconds)
{
    if (seconds < 5) seconds = 5;
    if (seconds > 600) seconds = 600;
    discovery_state.beaconIntervalS = seconds;
}

void discovery_set_presence_timeout(uint16_t seconds)
{
    if (seconds < 10) seconds = 10;
    if (seconds > 3600) seconds = 3600;
    discovery_state.presenceTimeoutS = seconds;
}

// ============================================================
// SECTION 3 : BEACON
// ============================================================

/**
 * @brief Envoie un beacon de présence
 */
void discovery_send_beacon(void)
{
    if (!discovery_state.enabled) return;
    
    DiscoveryMessage message;
    discovery_build_message(&message, DISCOVERY_MSG_BEACON);
    
    // Envoyer via LoRa (broadcast)
    LoRaPacket packet;
    lora_packet_build_discovery(&packet, message.senderName, message.capabilities);
    
    if (lora_driver_send_packet(&packet, false))
    {
        discovery_state.lastBeaconTime = HAL_GetTick();
        discovery_state.beaconsSent++;
        DISCOVERY_DEBUG("Beacon envoyé\n");
    }
}

/**
 * @brief Active/désactive l'envoi automatique
 */
void discovery_beacon_auto_enable(bool enable)
{
    if (enable)
    {
        discovery_state.lastBeaconTime = HAL_GetTick();
    }
}

/**
 * @brief Traitement périodique
 */
void discovery_process(void)
{
    if (!discovery_state.enabled || !discovery_state.initialized) return;
    
    uint32_t now = HAL_GetTick();
    
    // Envoyer un beacon périodiquement
    if ((now - discovery_state.lastBeaconTime) >= (discovery_state.beaconIntervalS * 1000))
    {
        discovery_send_beacon();
    }
    
    // Vérifier le timeout du scan
    if (discovery_state.scanning)
    {
        if ((now - discovery_state.scanStartTime) >= discovery_state.scanDurationMs)
        {
            discovery_scan_stop();
        }
    }
    
    // Vérifier la présence des pairs
    discovery_check_presence();
}

// ============================================================
// SECTION 4 : SCAN
// ============================================================

/**
 * @brief Lance un scan du réseau
 */
void discovery_scan_start(uint32_t durationMs)
{
    if (!discovery_state.enabled) return;
    
    if (durationMs == 0)
    {
        durationMs = discovery_state.scanDurationMs;
    }
    
    DISCOVERY_DEBUG("Démarrage scan (%lu ms)...\n", (unsigned long)durationMs);
    
    discovery_state.scanning = true;
    discovery_state.scanStartTime = HAL_GetTick();
    discovery_state.scanDurationMs = durationMs;
    
    // Envoyer une demande de découverte
    DiscoveryMessage message;
    discovery_build_message(&message, DISCOVERY_MSG_REQUEST);
    
    LoRaPacket packet;
    lora_packet_build_discovery(&packet, message.senderName, message.capabilities);
    lora_driver_send_packet(&packet, false);
    
    // Passer en mode réception pour écouter les réponses
    lora_driver_set_profile(PROFILE_BALANCED);
}

/**
 * @brief Arrête le scan
 */
void discovery_scan_stop(void)
{
    if (!discovery_state.scanning) return;
    
    discovery_state.scanning = false;
    discovery_state.lastScanTime = HAL_GetTick();
    
    DISCOVERY_DEBUG("Scan terminé (%d pairs)\n", discovery_get_online_count());
    
    if (scan_complete_callback)
    {
        scan_complete_callback(discovery_get_online_count());
    }
}

/**
 * @brief Vérifie si un scan est en cours
 */
bool discovery_is_scanning(void)
{
    return discovery_state.scanning;
}

// ============================================================
// SECTION 5 : GESTION DES PAIRS
// ============================================================

/**
 * @brief Recherche un pair par son UID
 */
static DiscoveryPeer* find_peer_by_uid_internal(uint32_t uid)
{
    for (uint8_t i = 0; i < discovery_state.peerCount; i++)
    {
        if (discovery_state.peers[i].uid == uid)
        {
            return &discovery_state.peers[i];
        }
    }
    return NULL;
}

/**
 * @brief Ajoute ou met à jour un pair dans la liste
 */
static DiscoveryPeer* add_or_update_peer(const DiscoveryMessage* message, 
                                          int16_t rssi, int8_t snr)
{
    DiscoveryPeer* peer = find_peer_by_uid_internal(message->senderUid);
    
    if (peer == NULL)
    {
        // Nouveau pair
        if (discovery_state.peerCount >= DISCOVERY_MAX_PEERS)
        {
            DISCOVERY_DEBUG("Liste de pairs pleine (%d max)\n", DISCOVERY_MAX_PEERS);
            return NULL;
        }
        
        peer = &discovery_state.peers[discovery_state.peerCount++];
        memset(peer, 0, sizeof(DiscoveryPeer));
        
        // Initialiser les informations
        peer->uid = message->senderUid;
        strncpy(peer->msisdn, message->senderMsisdn, IDENTITY_PHONE_NUMBER_MAX - 1);
        strncpy(peer->deviceName, message->senderName, IDENTITY_DEVICE_NAME_MAX - 1);
        peer->firstSeen = HAL_GetTick();
        peer->state = PEER_STATE_DISCOVERED;
        peer->capabilities = message->capabilities;
        peer->protocolVersion = message->protocolVersion;
        
        discovery_state.peersDiscovered++;
        
        DISCOVERY_DEBUG("NOUVEAU pair : %s (%s) [UID=0x%08lX]\n",
                       peer->deviceName, peer->msisdn, (unsigned long)peer->uid);
        
        if (new_peer_callback)
        {
            new_peer_callback(peer);
        }
    }
    else
    {
        // Mise à jour du pair existant
        PeerState oldState = peer->state;
        
        // Mettre à jour les informations
        strncpy(peer->msisdn, message->senderMsisdn, IDENTITY_PHONE_NUMBER_MAX - 1);
        strncpy(peer->deviceName, message->senderName, IDENTITY_DEVICE_NAME_MAX - 1);
        peer->capabilities = message->capabilities;
        peer->batteryLevel = message->batteryLevel;
        
        // Mettre à jour l'état
        if (peer->state != PEER_STATE_ONLINE)
        {
            peer->state = PEER_STATE_ONLINE;
            
            if (peer_state_callback)
            {
                peer_state_callback(peer, oldState, PEER_STATE_ONLINE);
            }
        }
    }
    
    // Mettre à jour les timestamps et le signal
    peer->lastSeen = HAL_GetTick();
    peer->lastBeacon = HAL_GetTick();
    peer->rssi = rssi;
    peer->snr = snr;
    peer->signalQuality = calculate_signal_quality(rssi, snr);
    peer->beaconsReceived++;
    peer->messagesExchanged++;
    
    return peer;
}

/**
 * @brief Calcule la qualité du signal (0-100)
 */
static uint8_t calculate_signal_quality(int16_t rssi, int8_t snr)
{
    // RSSI : -50 dBm = excellent, -130 dBm = très faible
    int16_t rssiQuality;
    if (rssi >= -50) rssiQuality = 100;
    else if (rssi <= -130) rssiQuality = 0;
    else rssiQuality = (rssi + 130) * 100 / 80;
    
    // Bonus SNR
    int16_t snrBonus = (snr > 0) ? snr * 2 : 0;
    
    uint8_t quality = (uint8_t)(rssiQuality + snrBonus);
    if (quality > 100) quality = 100;
    
    return quality;
}

/**
 * @brief Récupère la liste des pairs
 */
uint8_t discovery_get_peers(DiscoveryPeer* peers, uint8_t maxCount)
{
    if (peers == NULL) return 0;
    
    uint8_t count = (discovery_state.peerCount < maxCount) ? 
                     discovery_state.peerCount : maxCount;
    
    memcpy(peers, discovery_state.peers, count * sizeof(DiscoveryPeer));
    return count;
}

/**
 * @brief Récupère le nombre de pairs
 */
uint8_t discovery_get_peer_count(void)
{
    return discovery_state.peerCount;
}

/**
 * @brief Récupère le nombre de pairs en ligne
 */
uint8_t discovery_get_online_count(void)
{
    uint8_t count = 0;
    
    for (uint8_t i = 0; i < discovery_state.peerCount; i++)
    {
        if (discovery_state.peers[i].state == PEER_STATE_ONLINE ||
            discovery_state.peers[i].state == PEER_STATE_DISCOVERED)
        {
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Recherche un pair par son numéro
 */
DiscoveryPeer* discovery_find_peer_by_msisdn(const char* msisdn)
{
    if (msisdn == NULL) return NULL;
    
    for (uint8_t i = 0; i < discovery_state.peerCount; i++)
    {
        if (strcmp(discovery_state.peers[i].msisdn, msisdn) == 0)
        {
            return &discovery_state.peers[i];
        }
    }
    return NULL;
}

/**
 * @brief Recherche un pair par son UID
 */
DiscoveryPeer* discovery_find_peer_by_uid(uint32_t uid)
{
    return find_peer_by_uid_internal(uid);
}

/**
 * @brief Recherche un pair par son nom
 */
DiscoveryPeer* discovery_find_peer_by_name(const char* name)
{
    if (name == NULL) return NULL;
    
    for (uint8_t i = 0; i < discovery_state.peerCount; i++)
    {
        if (strcasecmp(discovery_state.peers[i].deviceName, name) == 0)
        {
            return &discovery_state.peers[i];
        }
    }
    return NULL;
}

/**
 * @brief Supprime un pair
 */
void discovery_remove_peer(uint32_t uid)
{
    for (uint8_t i = 0; i < discovery_state.peerCount; i++)
    {
        if (discovery_state.peers[i].uid == uid)
        {
            // Décaler les pairs suivants
            if (i < discovery_state.peerCount - 1)
            {
                memmove(&discovery_state.peers[i], 
                        &discovery_state.peers[i + 1],
                        (discovery_state.peerCount - i - 1) * sizeof(DiscoveryPeer));
            }
            discovery_state.peerCount--;
            DISCOVERY_DEBUG("Pair supprimé : UID=0x%08lX\n", (unsigned long)uid);
            return;
        }
    }
}

/**
 * @brief Vide la liste
 */
void discovery_clear_peers(void)
{
    discovery_state.peerCount = 0;
    DISCOVERY_DEBUG("Liste des pairs vidée\n");
}

/**
 * @brief Vérifie la présence des pairs (timeout)
 */
void discovery_check_presence(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t timeoutMs = discovery_state.presenceTimeoutS * 1000;
    
    for (uint8_t i = 0; i < discovery_state.peerCount; i++)
    {
        DiscoveryPeer* peer = &discovery_state.peers[i];
        
        if (peer->state == PEER_STATE_ONLINE || peer->state == PEER_STATE_DISCOVERED)
        {
            uint32_t elapsed = now - peer->lastBeacon;
            
            if (elapsed > timeoutMs)
            {
                // Le pair est considéré absent
                PeerState oldState = peer->state;
                peer->state = PEER_STATE_AWAY;
                
                DISCOVERY_DEBUG("Pair absent (timeout) : %s\n", peer->deviceName);
                
                if (peer_lost_callback)
                {
                    peer_lost_callback(peer);
                }
                
                if (peer_state_callback)
                {
                    peer_state_callback(peer, oldState, PEER_STATE_AWAY);
                }
            }
        }
    }
}

// ============================================================
// SECTION 6 : TRAITEMENT DES MESSAGES
// ============================================================

/**
 * @brief Traite un message de découverte reçu
 */
void discovery_process_message(const DiscoveryMessage* message, int16_t rssi, int8_t snr)
{
    if (message == NULL) return;
    
    DISCOVERY_DEBUG("Message reçu : type=%d de %s\n", 
                   message->messageType, message->senderName);
    
    switch (message->messageType)
    {
        case DISCOVERY_MSG_REQUEST:
            // Quelqu'un demande qui est là → répondre
            {
                DiscoveryMessage response;
                discovery_build_message(&response, DISCOVERY_MSG_RESPONSE);
                
                LoRaPacket packet;
                lora_packet_build_discovery(&packet, response.senderName, response.capabilities);
                lora_driver_send_packet(&packet, false);
            }
            break;
            
        case DISCOVERY_MSG_RESPONSE:
        case DISCOVERY_MSG_BEACON:
        case DISCOVERY_MSG_UPDATE:
            // Ajouter/mettre à jour le pair
            add_or_update_peer(message, rssi, snr);
            break;
            
        case DISCOVERY_MSG_GOODBYE:
            // Un pair annonce son départ
            {
                DiscoveryPeer* peer = find_peer_by_uid_internal(message->senderUid);
                
                if (peer != NULL)
                {
                    PeerState oldState = peer->state;
                    peer->state = PEER_STATE_OFFLINE;
                    
                    DISCOVERY_DEBUG("Pair parti : %s\n", peer->deviceName);
                    
                    if (peer_state_callback)
                    {
                        peer_state_callback(peer, oldState, PEER_STATE_OFFLINE);
                    }
                }
            }
            break;
            
        default:
            DISCOVERY_DEBUG("Type de message inconnu : %d\n", message->messageType);
            break;
    }
}

/**
 * @brief Construit un message de découverte
 */
void discovery_build_message(DiscoveryMessage* message, DiscoveryMessageType type)
{
    if (message == NULL) return;
    
    memset(message, 0, sizeof(DiscoveryMessage));
    
    message->messageType = type;
    message->protocolVersion = DISCOVERY_PROTOCOL_VERSION;
    
    // Remplir avec l'identité locale
    DeviceIdentity* identity = identity_get();
    
    message->senderUid = identity->uid;
    strncpy(message->senderMsisdn, identity->msisdn, IDENTITY_PHONE_NUMBER_MAX - 1);
    strncpy(message->senderName, identity->deviceName, IDENTITY_DEVICE_NAME_MAX - 1);
    
    message->capabilities = identity->capabilities;
    message->batteryLevel = battery_monitor_get_percent();
    message->signalQuality = 100;  // Local = parfait
    message->timestamp = HAL_GetTick();
}

// ============================================================
// SECTION 7 : DÉPART
// ============================================================

/**
 * @brief Annonce son départ du réseau
 */
void discovery_send_goodbye(void)
{
    if (!discovery_state.enabled) return;
    
    DiscoveryMessage message;
    discovery_build_message(&message, DISCOVERY_MSG_GOODBYE);
    
    LoRaPacket packet;
    lora_packet_build_discovery(&packet, message.senderName, message.capabilities);
    lora_driver_send_packet(&packet, false);
    
    DISCOVERY_DEBUG("Goodbye envoyé\n");
}

// ============================================================
// SECTION 8 : CALLBACKS
// ============================================================

void discovery_set_new_peer_callback(Discovery_NewPeerCallback callback)
{
    new_peer_callback = callback;
}

void discovery_set_peer_state_callback(Discovery_PeerStateCallback callback)
{
    peer_state_callback = callback;
}

void discovery_set_peer_lost_callback(Discovery_PeerLostCallback callback)
{
    peer_lost_callback = callback;
}

void discovery_set_scan_complete_callback(Discovery_ScanCompleteCallback callback)
{
    scan_complete_callback = callback;
}

// ============================================================
// SECTION 9 : DÉBOGAGE
// ============================================================

void discovery_print_state(void)
{
    printf("\n═══ ÉTAT DÉCOUVERTE ═══\n");
    printf("Activée      : %s\n", discovery_state.enabled ? "Oui" : "Non");
    printf("Scan en cours: %s\n", discovery_state.scanning ? "Oui" : "Non");
    printf("Pairs connus : %d\n", discovery_state.peerCount);
    printf("En ligne     : %d\n", discovery_get_online_count());
    printf("Beacons envoyés : %lu\n", (unsigned long)discovery_state.beaconsSent);
    printf("Paires découverts: %lu\n", (unsigned long)discovery_state.peersDiscovered);
    printf("══════════════════════\n\n");
}

void discovery_print_peers(void)
{
    printf("\n═══ PAIRS CONNUS (%d) ═══\n", discovery_state.peerCount);
    printf("%-4s %-16s %-16s %-8s %-6s %-10s\n", 
           "St.", "Nom", "Numéro", "Signal", "Batt.", "Dernier");
    printf("────────────────────────────────────────────────────────\n");
    
    for (uint8_t i = 0; i < discovery_state.peerCount; i++)
    {
        DiscoveryPeer* p = &discovery_state.peers[i];
        
        const char* stateStr = "?";
        switch (p->state)
        {
            case PEER_STATE_ONLINE:    stateStr = "ON"; break;
            case PEER_STATE_DISCOVERED:stateStr = "NEW"; break;
            case PEER_STATE_AWAY:      stateStr = "AWAY"; break;
            case PEER_STATE_OFFLINE:   stateStr = "OFF"; break;
            default:                   stateStr = "???"; break;
        }
        
        uint32_t lastSeenSec = (HAL_GetTick() - p->lastSeen) / 1000;
        
        printf("%-4s %-16s %-16s %-3d%%    %-3d%%  %-4lus\n",
               stateStr, p->deviceName, p->msisdn,
               p->signalQuality, p->batteryLevel,
               (unsigned long)lastSeenSec);
    }
    printf("══════════════════════════════\n\n");
}

void discovery_print_peer(const DiscoveryPeer* peer)
{
    if (peer == NULL) return;
    
    printf("\n═══ PAIR : %s ═══\n", peer->deviceName);
    printf("UID       : 0x%08lX\n", (unsigned long)peer->uid);
    printf("MSISDN    : %s\n", peer->msisdn);
    printf("État      : %d\n", peer->state);
    printf("RSSI      : %d dBm\n", peer->rssi);
    printf("SNR       : %d dB\n", peer->snr);
    printf("Qualité   : %d%%\n", peer->signalQuality);
    printf("Batterie  : %d%%\n", peer->batteryLevel);
    printf("Beacons   : %lu\n", (unsigned long)peer->beaconsReceived);
    printf("══════════════════════\n\n");
}

void discovery_print_statistics(void)
{
    printf("\n═══ STATISTIQUES DÉCOUVERTE ═══\n");
    printf("Beacons envoyés   : %lu\n", (unsigned long)discovery_state.beaconsSent);
    printf("Beacons reçus     : %lu\n", (unsigned long)discovery_state.beaconsReceived);
    printf("Pairs découverts  : %lu\n", (unsigned long)discovery_state.peersDiscovered);
    printf("Pairs perdus      : %lu\n", (unsigned long)discovery_state.peersLost);
    printf("══════════════════════════════\n\n");
}

bool discovery_self_test(void)
{
    DISCOVERY_DEBUG("Auto-test...\n");
    
    if (!discovery_state.initialized)
    {
        DISCOVERY_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test : construire un message
    DiscoveryMessage msg;
    discovery_build_message(&msg, DISCOVERY_MSG_BEACON);
    
    if (msg.messageType != DISCOVERY_MSG_BEACON)
    {
        DISCOVERY_DEBUG("Échec : type message incorrect\n");
        return false;
    }
    
    // Test : ajouter un pair fictif
    DiscoveryMessage fakeMsg;
    memset(&fakeMsg, 0, sizeof(DiscoveryMessage));
    fakeMsg.senderUid = 0x12345678;
    strcpy(fakeMsg.senderMsisdn, "0600000000");
    strcpy(fakeMsg.senderName, "TestPeer");
    fakeMsg.messageType = DISCOVERY_MSG_BEACON;
    
    add_or_update_peer(&fakeMsg, -50, 10);
    
    DiscoveryPeer* found = discovery_find_peer_by_uid(0x12345678);
    if (found == NULL)
    {
        DISCOVERY_DEBUG("Échec : pair non trouvé\n");
        return false;
    }
    
    // Nettoyer
    discovery_remove_peer(0x12345678);
    
    DISCOVERY_DEBUG("Auto-test OK\n");
    return true;
}