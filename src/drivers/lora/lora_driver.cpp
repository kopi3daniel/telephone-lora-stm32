/**
 * @file lora_driver.cpp
 * @brief Implémentation du driver LoRa haut niveau
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans lora_driver.h.
 * 
 * Il gère :
 * - L'initialisation et la configuration du module
 * - La transmission et la réception des paquets
 * - La logique des appels (établissement, maintien, fin)
 * - La messagerie SMS
 * - La découverte du réseau
 * - Les statistiques et la maintenance
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "lora_driver.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// VARIABLES GLOBALES DU DRIVER
// ============================================================

/** @brief Numéro de téléphone local */
static char local_phone_number[16] = "0600000000";

/** @brief Nom du dispositif local */
static char local_device_name[16] = "LoRaPhone";

/** @brief Profil actif */
static const SX1278_Profile* active_profile = &PROFILE_BALANCED_CONFIG;

/** @brief Buffer de réception */
static uint8_t rx_buffer[LORA_RX_BUFFER_SIZE];

/** @brief Buffer de transmission */
static uint8_t tx_buffer[LORA_TX_BUFFER_SIZE];

/** @brief Compteur d'ID de paquets */
static uint32_t packet_id_counter = 0;

/** @brief Cache des téléphones connus */
static LoRaKnownPhone known_phones[LORA_MAX_KNOWN_PHONES];
static uint8_t known_phone_count = 0;

/** @brief Statistiques */
static LoRaStatistics statistics = {0};

// ============================================================
// ÉTAT DE LA TÉLÉPHONIE
// ============================================================

/** @brief État de l'appel en cours */
typedef enum {
    CALL_STATE_IDLE,            // Pas d'appel
    CALL_STATE_DIALING,         // Numérotation en cours
    CALL_STATE_RINGING,         // Sonnerie chez le destinataire
    CALL_STATE_INCOMING,        // Appel entrant
    CALL_STATE_CONNECTED,       // Communication établie
    CALL_STATE_ENDING           // Fin d'appel en cours
} CallState;

static CallState call_state = CALL_STATE_IDLE;
static char call_partner_number[16] = "";
static char call_partner_name[16] = "";
static uint32_t call_start_time = 0;
static uint32_t call_duration = 0;
static uint32_t last_audio_send = 0;
static uint32_t last_audio_receive = 0;

// ============================================================
// CALLBACKS
// ============================================================

static LoRa_OnIncomingCall    on_incoming_call    = NULL;
static LoRa_OnCallAccepted    on_call_accepted    = NULL;
static LoRa_OnCallRejected    on_call_rejected    = NULL;
static LoRa_OnCallEnded       on_call_ended       = NULL;
static LoRa_OnAudioData       on_audio_data       = NULL;
static LoRa_OnSMSReceived     on_sms_received     = NULL;
static LoRa_OnPhoneDiscovered on_phone_discovered = NULL;
static LoRa_OnError           on_error            = NULL;

// ============================================================
// FONCTIONS INTERNES (PROTOTYPES)
// ============================================================

static void process_incoming_packet(LoRaPacket* packet);
static void handle_call_request(LoRaPacket* packet);
static void handle_call_accept(LoRaPacket* packet);
static void handle_call_reject(LoRaPacket* packet);
static void handle_call_end(LoRaPacket* packet);
static void handle_audio_data(LoRaPacket* packet);
static void handle_sms(LoRaPacket* packet);
static void handle_discovery(LoRaPacket* packet);
static void handle_ping(LoRaPacket* packet);
static void handle_ack(LoRaPacket* packet);
static void add_or_update_known_phone(const char* number, const char* name, int16_t rssi);
static LoRaKnownPhone* find_known_phone(const char* number);
static uint32_t generate_packet_id(void);
static void build_packet_header(LoRaPacket* packet, LoRaPacketType type, const char* receiver);

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le driver LoRa
 */
bool lora_driver_init(void)
{
    printf("[LORA] Initialisation du driver...\n");
    
    // Initialiser la couche HAL
    SX1278_Error err = sx1278_hal_init();
    if (err != SX1278_OK)
    {
        printf("[LORA] Échec initialisation HAL (code %d)\n", err);
        return false;
    }
    
    // Appliquer le profil par défaut
    sx1278_config_apply_profile(active_profile);
    
    // Vider le cache réseau
    memset(known_phones, 0, sizeof(known_phones));
    known_phone_count = 0;
    
    // Réinitialiser les statistiques
    memset(&statistics, 0, sizeof(statistics));
    statistics.uptime = HAL_GetTick() / 1000;
    
    // Démarrer la réception
    sx1278_hal_start_receive();
    
    printf("[LORA] Driver initialisé avec succès\n");
    printf("[LORA] Profil: %s\n", active_profile->name);
    printf("[LORA] Fréquence: %lu Hz\n", (unsigned long)active_profile->frequency);
    printf("[LORA] SF: %d, BW: %lu Hz\n", active_profile->spreadingFactor, (unsigned long)active_profile->bandwidth);
    
    return true;
}

/**
 * @brief Désinitialise le driver
 */
void lora_driver_deinit(void)
{
    sx1278_hal_sleep();
    call_state = CALL_STATE_IDLE;
    printf("[LORA] Driver désinitialisé\n");
}

/**
 * @brief Vérifie si le driver est prêt
 */
bool lora_driver_is_ready(void)
{
    return sx1278_state.initialized;
}

// ============================================================
// SECTION 2 : CONFIGURATION
// ============================================================

/**
 * @brief Change le profil de configuration
 */
bool lora_driver_set_profile(SX1278_ProfileType type)
{
    const SX1278_Profile* profile = sx1278_config_get_profile(type);
    if (profile == NULL) return false;
    
    SX1278_Error err = sx1278_config_apply_profile(profile);
    if (err == SX1278_OK)
    {
        active_profile = profile;
        printf("[LORA] Profil changé: %s\n", profile->name);
        return true;
    }
    return false;
}

/**
 * @brief Récupère le profil actuel
 */
const SX1278_Profile* lora_driver_get_profile(void)
{
    return active_profile;
}

/**
 * @brief Définit le numéro de téléphone local
 */
void lora_driver_set_phone_number(const char* phoneNumber)
{
    strncpy(local_phone_number, phoneNumber, 15);
    local_phone_number[15] = '\0';
    printf("[LORA] Numéro local: %s\n", local_phone_number);
}

/**
 * @brief Récupère le numéro de téléphone local
 */
const char* lora_driver_get_phone_number(void)
{
    return local_phone_number;
}

/**
 * @brief Définit le nom du dispositif
 */
void lora_driver_set_device_name(const char* name)
{
    strncpy(local_device_name, name, 15);
    local_device_name[15] = '\0';
    printf("[LORA] Nom: %s\n", local_device_name);
}

/**
 * @brief Règle la puissance d'émission
 */
void lora_driver_set_power(int8_t power_dbm)
{
    sx1278_hal_set_tx_power(power_dbm);
}

/**
 * @brief Récupère le RSSI actuel
 */
int16_t lora_driver_get_rssi(void)
{
    return sx1278_hal_get_rssi();
}

/**
 * @brief Récupère le SNR actuel
 */
int8_t lora_driver_get_snr(void)
{
    return sx1278_hal_get_snr();
}

// ============================================================
// SECTION 3 : COMMUNICATION DE BASE
// ============================================================

/**
 * @brief Génère un ID de paquet unique
 */
static uint32_t generate_packet_id(void)
{
    return ++packet_id_counter;
}

/**
 * @brief Construit l'en-tête d'un paquet
 */
static void build_packet_header(LoRaPacket* packet, LoRaPacketType type, const char* receiver)
{
    memset(packet, 0, sizeof(LoRaPacket));
    packet->type = type;
    packet->packetId = generate_packet_id();
    strncpy(packet->sender, local_phone_number, 15);
    strncpy(packet->receiver, receiver ? receiver : "BROADCAST", 15);
}

/**
 * @brief Envoie un paquet
 */
bool lora_driver_send_packet(LoRaPacket* packet, bool waitAck)
{
    if (!sx1278_state.initialized) return false;
    
    // Calculer la taille totale
    uint16_t totalLength = 39 + packet->dataLength; // Header (39) + données
    
    if (totalLength > LORA_MAX_PACKET_SIZE)
    {
        printf("[LORA] Paquet trop grand (%d octets)\n", totalLength);
        return false;
    }
    
    // Sérialiser le paquet dans le buffer TX
    uint8_t* ptr = tx_buffer;
    
    *ptr++ = packet->type;
    
    *ptr++ = (packet->packetId >> 24) & 0xFF;
    *ptr++ = (packet->packetId >> 16) & 0xFF;
    *ptr++ = (packet->packetId >> 8) & 0xFF;
    *ptr++ = packet->packetId & 0xFF;
    
    memcpy(ptr, packet->sender, 16);  ptr += 16;
    memcpy(ptr, packet->receiver, 16); ptr += 16;
    
    *ptr++ = (packet->dataLength >> 8) & 0xFF;
    *ptr++ = packet->dataLength & 0xFF;
    
    memcpy(ptr, packet->data, packet->dataLength);
    
    // Envoyer
    SX1278_Error err = sx1278_hal_transmit(tx_buffer, totalLength);
    
    if (err == SX1278_OK)
    {
        statistics.packetsSent++;
        statistics.bytesSent += totalLength;
        
        if (packet->type == PACKET_AUDIO_DATA) statistics.audioPacketsSent++;
        
        // Remettre en réception
        sx1278_hal_start_receive();
        return true;
    }
    else
    {
        statistics.packetsError++;
        printf("[LORA] Erreur transmission (code %d)\n", err);
        return false;
    }
}

/**
 * @brief Vérifie si un paquet est disponible
 */
bool lora_driver_is_packet_available(void)
{
    return sx1278_hal_is_packet_received();
}

/**
 * @brief Récupère le dernier paquet reçu
 */
bool lora_driver_receive_packet(LoRaPacket* packet)
{
    if (!sx1278_state.initialized) return false;
    
    uint16_t length;
    SX1278_Error err = sx1278_hal_read_packet(rx_buffer, &length);
    
    if (err != SX1278_OK) return false;
    if (length < 39) return false;  // Paquet trop court
    
    // Désérialiser le paquet
    uint8_t* ptr = rx_buffer;
    
    packet->type = *ptr++;
    
    packet->packetId  = ((uint32_t)*ptr++ << 24);
    packet->packetId |= ((uint32_t)*ptr++ << 16);
    packet->packetId |= ((uint32_t)*ptr++ << 8);
    packet->packetId |= *ptr++;
    
    memcpy(packet->sender, ptr, 16);   ptr += 16;
    memcpy(packet->receiver, ptr, 16); ptr += 16;
    
    packet->dataLength  = ((uint16_t)*ptr++ << 8);
    packet->dataLength |= *ptr++;
    
    if (packet->dataLength > sizeof(packet->data))
    {
        packet->dataLength = sizeof(packet->data);
    }
    
    memcpy(packet->data, ptr, packet->dataLength);
    
    statistics.packetsReceived++;
    statistics.bytesReceived += length;
    
    return true;
}

/**
 * @brief Envoie un accusé de réception
 */
void lora_driver_send_ack(uint32_t originalPacketId, const char* receiver)
{
    LoRaPacket ack;
    build_packet_header(&ack, PACKET_ACK, receiver);
    ack.dataLength = 4;
    memcpy(ack.data, &originalPacketId, 4);
    lora_driver_send_packet(&ack, false);
}

// ============================================================
// SECTION 4 : TÉLÉPHONIE (APPELS)
// ============================================================

/**
 * @brief Initie un appel sortant
 */
bool lora_driver_call(const char* calleeNumber)
{
    if (call_state != CALL_STATE_IDLE) return false;
    
    printf("[LORA] Appel de %s...\n", calleeNumber);
    
    // Passer en mode Audio
    lora_driver_set_profile(PROFILE_AUDIO);
    
    // Construire la demande d'appel
    LoRaPacket packet;
    build_packet_header(&packet, PACKET_CALL_REQUEST, calleeNumber);
    strncpy(packet.sender, local_device_name, 15);  // Envoyer le nom en plus
    packet.dataLength = 0;
    
    if (lora_driver_send_packet(&packet, false))
    {
        call_state = CALL_STATE_DIALING;
        strncpy(call_partner_number, calleeNumber, 15);
        call_start_time = HAL_GetTick();
        statistics.callsMade++;
        return true;
    }
    
    return false;
}

/**
 * @brief Accepte un appel entrant
 */
bool lora_driver_accept_call(void)
{
    if (call_state != CALL_STATE_INCOMING) return false;
    
    printf("[LORA] Appel accepté\n");
    
    // Passer en mode Audio
    lora_driver_set_profile(PROFILE_AUDIO);
    
    // Envoyer l'acceptation
    LoRaPacket packet;
    build_packet_header(&packet, PACKET_CALL_ACCEPT, call_partner_number);
    packet.dataLength = 0;
    
    if (lora_driver_send_packet(&packet, false))
    {
        call_state = CALL_STATE_CONNECTED;
        call_start_time = HAL_GetTick();
        
        if (on_call_accepted) on_call_accepted();
        return true;
    }
    
    return false;
}

/**
 * @brief Refuse un appel entrant
 */
bool lora_driver_reject_call(uint8_t reason)
{
    if (call_state != CALL_STATE_INCOMING && call_state != CALL_STATE_RINGING) return false;
    
    printf("[LORA] Appel refusé (raison: %d)\n", reason);
    
    LoRaPacket packet;
    build_packet_header(&packet, PACKET_CALL_REJECT, call_partner_number);
    packet.data[0] = reason;
    packet.dataLength = 1;
    
    if (lora_driver_send_packet(&packet, false))
    {
        call_state = CALL_STATE_IDLE;
        call_partner_number[0] = '\0';
        return true;
    }
    
    return false;
}

/**
 * @brief Termine un appel en cours
 */
bool lora_driver_end_call(void)
{
    if (call_state != CALL_STATE_CONNECTED && 
        call_state != CALL_STATE_DIALING && 
        call_state != CALL_STATE_RINGING) return false;
    
    call_duration = (HAL_GetTick() - call_start_time) / 1000;
    
    printf("[LORA] Fin d'appel (durée: %lu s)\n", (unsigned long)call_duration);
    
    LoRaPacket packet;
    build_packet_header(&packet, PACKET_CALL_END, call_partner_number);
    packet.dataLength = 0;
    
    lora_driver_send_packet(&packet, false);
    
    // Repasser en mode équilibré
    lora_driver_set_profile(PROFILE_BALANCED);
    
    if (on_call_ended) on_call_ended(call_duration);
    
    call_state = CALL_STATE_IDLE;
    call_partner_number[0] = '\0';
    call_duration = 0;
    
    return true;
}

/**
 * @brief Vérifie si un appel est en cours
 */
bool lora_driver_is_in_call(void)
{
    return (call_state == CALL_STATE_CONNECTED);
}

/**
 * @brief Récupère le numéro du correspondant
 */
const char* lora_driver_get_call_partner(void)
{
    if (call_state == CALL_STATE_IDLE) return NULL;
    return call_partner_number;
}

/**
 * @brief Récupère la durée de l'appel
 */
uint32_t lora_driver_get_call_duration(void)
{
    if (call_state == CALL_STATE_CONNECTED)
    {
        return (HAL_GetTick() - call_start_time) / 1000;
    }
    return call_duration;
}

// ============================================================
// SECTION 5 : AUDIO
// ============================================================

/**
 * @brief Envoie un buffer audio
 */
bool lora_driver_send_audio(uint8_t* audioData, uint16_t length)
{
    if (call_state != CALL_STATE_CONNECTED) return false;
    if (length > 216) length = 216;  // Limite de données par paquet
    
    LoRaPacket packet;
    build_packet_header(&packet, PACKET_AUDIO_DATA, call_partner_number);
    memcpy(packet.data, audioData, length);
    packet.dataLength = length;
    
    last_audio_send = HAL_GetTick();
    return lora_driver_send_packet(&packet, false);
}

/**
 * @brief Vérifie si des données audio sont disponibles
 */
bool lora_driver_is_audio_available(void)
{
    // Vérifier si le dernier paquet reçu est de l'audio
    return false;  // Géré par les callbacks
}

/**
 * @brief Récupère les données audio reçues
 */
bool lora_driver_receive_audio(uint8_t* audioData, uint16_t* length)
{
    // La réception audio est gérée par les callbacks
    return false;
}

// ============================================================
// SECTION 6 : MESSAGERIE (SMS)
// ============================================================

/**
 * @brief Envoie un SMS
 */
bool lora_driver_send_sms(const char* receiverNumber, const char* message)
{
    if (!sx1278_state.initialized) return false;
    
    uint16_t msgLen = strlen(message);
    if (msgLen > LORA_SMS_MAX_LENGTH) msgLen = LORA_SMS_MAX_LENGTH;
    
    printf("[LORA] Envoi SMS à %s (%d caractères)\n", receiverNumber, msgLen);
    
    // Passer en mode SMS pour une meilleure portée
    SX1278_ProfileType previousProfile = active_profile->type;
    lora_driver_set_profile(PROFILE_SMS);
    
    LoRaPacket packet;
    build_packet_header(&packet, PACKET_SMS, receiverNumber);
    memcpy(packet.data, message, msgLen);
    packet.dataLength = msgLen;
    
    bool result = lora_driver_send_packet(&packet, true);  // Attendre ACK
    
    // Revenir au profil précédent
    lora_driver_set_profile(previousProfile);
    
    if (result) statistics.smsSent++;
    return result;
}

/**
 * @brief Vérifie si un SMS est disponible
 */
bool lora_driver_is_sms_available(void)
{
    return false;  // Géré par les callbacks
}

/**
 * @brief Récupère le dernier SMS reçu
 */
bool lora_driver_receive_sms(char* sender, char* message)
{
    return false;  // Géré par les callbacks
}

// ============================================================
// SECTION 7 : RÉSEAU
// ============================================================

/**
 * @brief Lance une découverte du réseau
 */
uint8_t lora_driver_discover_network(uint32_t timeoutMs)
{
    printf("[LORA] Découverte réseau (timeout: %lu ms)...\n", (unsigned long)timeoutMs);
    
    // Envoyer un broadcast de découverte
    LoRaPacket packet;
    build_packet_header(&packet, PACKET_DISCOVERY, "BROADCAST");
    strncpy((char*)packet.data, local_device_name, 15);
    packet.dataLength = strlen(local_device_name);
    
    lora_driver_send_packet(&packet, false);
    
    // Attendre les réponses
    uint32_t start = HAL_GetTick();
    uint8_t discovered = 0;
    
    while ((HAL_GetTick() - start) < timeoutMs)
    {
        lora_driver_process();
        
        // Vérifier si on a découvert quelque chose
        // (les téléphones sont ajoutés dans handle_discovery)
        
        HAL_Delay(100);
    }
    
    discovered = known_phone_count;
    printf("[LORA] %d téléphones découverts\n", discovered);
    
    return discovered;
}

/**
 * @brief Envoie un ping
 */
bool lora_driver_ping(const char* phoneNumber)
{
    printf("[LORA] Ping %s...\n", phoneNumber);
    
    LoRaPacket packet;
    build_packet_header(&packet, PACKET_PING, phoneNumber);
    packet.dataLength = 0;
    
    if (!lora_driver_send_packet(&packet, false)) return false;
    
    // Attendre le pong
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < 2000)
    {
        LoRaPacket response;
        if (lora_driver_receive_packet(&response))
        {
            if (response.type == PACKET_PONG && 
                strcmp(response.sender, phoneNumber) == 0)
            {
                printf("[LORA] Pong reçu de %s\n", phoneNumber);
                return true;
            }
        }
        HAL_Delay(50);
    }
    
    printf("[LORA] Timeout ping\n");
    return false;
}

/**
 * @brief Récupère la liste des téléphones connus
 */
uint8_t lora_driver_get_known_phones(LoRaKnownPhone* phones, uint8_t maxCount)
{
    uint8_t count = (known_phone_count < maxCount) ? known_phone_count : maxCount;
    memcpy(phones, known_phones, count * sizeof(LoRaKnownPhone));
    return count;
}

/**
 * @brief Recherche un téléphone connu
 */
LoRaKnownPhone* lora_driver_find_phone(const char* phoneNumber)
{
    return find_known_phone(phoneNumber);
}

// ============================================================
// SECTION 8 : MAINTENANCE
// ============================================================

/**
 * @brief Traitement périodique
 */
void lora_driver_process(void)
{
    if (!sx1278_state.initialized) return;
    
    // Vérifier les paquets entrants
    LoRaPacket packet;
    if (lora_driver_receive_packet(&packet))
    {
        process_incoming_packet(&packet);
    }
    
    // Vérifier le timeout des appels
    if (call_state == CALL_STATE_DIALING)
    {
        if ((HAL_GetTick() - call_start_time) > 30000)  // 30 secondes
        {
            printf("[LORA] Timeout appel (pas de réponse)\n");
            call_state = CALL_STATE_IDLE;
            if (on_call_rejected) on_call_rejected(0xFF);
        }
    }
    
    // Vérifier l'inactivité audio
    if (call_state == CALL_STATE_CONNECTED)
    {
        uint32_t now = HAL_GetTick();
        if ((now - last_audio_receive) > 10000)  // 10 secondes sans audio
        {
            // L'appel est peut-être coupé
            printf("[LORA] Alerte: pas d'audio reçu depuis 10s\n");
        }
    }
    
    // Mettre à jour l'uptime
    statistics.uptime = HAL_GetTick() / 1000;
}

/**
 * @brief Envoie un keep-alive
 */
void lora_driver_keepalive(void)
{
    LoRaPacket packet;
    build_packet_header(&packet, PACKET_KEEPALIVE, "BROADCAST");
    packet.dataLength = 0;
    lora_driver_send_packet(&packet, false);
}

/**
 * @brief Réinitialise les statistiques
 */
void lora_driver_reset_statistics(void)
{
    memset(&statistics, 0, sizeof(statistics));
    statistics.uptime = HAL_GetTick() / 1000;
}

/**
 * @brief Récupère les statistiques
 */
void lora_driver_get_statistics(LoRaStatistics* stats)
{
    memcpy(stats, &statistics, sizeof(LoRaStatistics));
    stats->uptime = HAL_GetTick() / 1000;
}

/**
 * @brief Affiche les statistiques
 */
void lora_driver_print_statistics(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║       STATISTIQUES LORA                  ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Paquets envoyés:    %8lu            ║\n", (unsigned long)statistics.packetsSent);
    printf("║ Paquets reçus:      %8lu            ║\n", (unsigned long)statistics.packetsReceived);
    printf("║ Paquets en erreur:  %8lu            ║\n", (unsigned long)statistics.packetsError);
    printf("║ Audio envoyés:      %8lu            ║\n", (unsigned long)statistics.audioPacketsSent);
    printf("║ Audio reçus:        %8lu            ║\n", (unsigned long)statistics.audioPacketsReceived);
    printf("║ SMS envoyés:        %8lu            ║\n", (unsigned long)statistics.smsSent);
    printf("║ SMS reçus:          %8lu            ║\n", (unsigned long)statistics.smsReceived);
    printf("║ Appels émis:        %8lu            ║\n", (unsigned long)statistics.callsMade);
    printf("║ Appels reçus:       %8lu            ║\n", (unsigned long)statistics.callsReceived);
    printf("║ Octets envoyés:     %8lu            ║\n", (unsigned long)statistics.bytesSent);
    printf("║ Octets reçus:       %8lu            ║\n", (unsigned long)statistics.bytesReceived);
    printf("║ Uptime:             %8lu s          ║\n", (unsigned long)statistics.uptime);
    printf("║ RSSI:               %8d dBm        ║\n", sx1278_hal_get_rssi());
    printf("║ SNR:                %8d dB         ║\n", sx1278_hal_get_snr());
    printf("╚══════════════════════════════════════════╝\n\n");
}

// ============================================================
// SECTION 9 : GESTION DES PAQUETS ENTRANTS
// ============================================================

/**
 * @brief Traite un paquet entrant selon son type
 */
static void process_incoming_packet(LoRaPacket* packet)
{
    // Vérifier si le paquet nous est destiné (ou broadcast)
    bool for_us = (strcmp(packet->receiver, local_phone_number) == 0 ||
                   strcmp(packet->receiver, "BROADCAST") == 0);
    
    if (!for_us && packet->type != PACKET_DISCOVERY) return;
    
    switch (packet->type)
    {
        case PACKET_CALL_REQUEST:  handle_call_request(packet);  break;
        case PACKET_CALL_ACCEPT:   handle_call_accept(packet);   break;
        case PACKET_CALL_REJECT:   handle_call_reject(packet);   break;
        case PACKET_CALL_BUSY:     handle_call_reject(packet);   break;
        case PACKET_CALL_END:      handle_call_end(packet);      break;
        case PACKET_AUDIO_DATA:    handle_audio_data(packet);    break;
        case PACKET_SMS:           handle_sms(packet);           break;
        case PACKET_DISCOVERY:     handle_discovery(packet);     break;
        case PACKET_PING:          handle_ping(packet);          break;
        case PACKET_ACK:           handle_ack(packet);           break;
        default: break;
    }
}

/**
 * @brief Traite une demande d'appel entrante
 */
static void handle_call_request(LoRaPacket* packet)
{
    if (call_state != CALL_STATE_IDLE)
    {
        // Occupé - envoyer BUSY
        LoRaPacket busy;
        build_packet_header(&busy, PACKET_CALL_BUSY, packet->sender);
        busy.dataLength = 0;
        lora_driver_send_packet(&busy, false);
        return;
    }
    
    printf("[LORA] Appel entrant de %s\n", packet->sender);
    
    call_state = CALL_STATE_INCOMING;
    strncpy(call_partner_number, packet->sender, 15);
    strncpy(call_partner_name, (char*)packet->data, 15);
    call_start_time = HAL_GetTick();
    
    statistics.callsReceived++;
    
    // Ajouter au cache réseau
    add_or_update_known_phone(packet->sender, call_partner_name, sx1278_hal_get_packet_rssi());
    
    if (on_incoming_call)
    {
        on_incoming_call(packet->sender, call_partner_name);
    }
}

/**
 * @brief Traite une acceptation d'appel
 */
static void handle_call_accept(LoRaPacket* packet)
{
    if (call_state != CALL_STATE_DIALING && call_state != CALL_STATE_RINGING) return;
    
    printf("[LORA] Appel accepté par %s\n", packet->sender);
    
    call_state = CALL_STATE_CONNECTED;
    call_start_time = HAL_GetTick();
    
    if (on_call_accepted) on_call_accepted();
}

/**
 * @brief Traite un refus d'appel
 */
static void handle_call_reject(LoRaPacket* packet)
{
    if (call_state != CALL_STATE_DIALING && call_state != CALL_STATE_RINGING) return;
    
    uint8_t reason = (packet->dataLength > 0) ? packet->data[0] : 0;
    printf("[LORA] Appel refusé par %s (raison: %d)\n", packet->sender, reason);
    
    call_state = CALL_STATE_IDLE;
    call_partner_number[0] = '\0';
    
    // Repasser en mode équilibré
    lora_driver_set_profile(PROFILE_BALANCED);
    
    if (on_call_rejected) on_call_rejected(reason);
}

/**
 * @brief Traite une fin d'appel
 */
static void handle_call_end(LoRaPacket* packet)
{
    if (call_state != CALL_STATE_CONNECTED && call_state != CALL_STATE_DIALING) return;
    
    call_duration = (HAL_GetTick() - call_start_time) / 1000;
    printf("[LORA] Appel terminé par %s (durée: %lu s)\n", packet->sender, (unsigned long)call_duration);
    
    call_state = CALL_STATE_IDLE;
    call_partner_number[0] = '\0';
    
    // Repasser en mode équilibré
    lora_driver_set_profile(PROFILE_BALANCED);
    
    if (on_call_ended) on_call_ended(call_duration);
    call_duration = 0;
}

/**
 * @brief Traite des données audio entrantes
 */
static void handle_audio_data(LoRaPacket* packet)
{
    if (call_state != CALL_STATE_CONNECTED) return;
    
    last_audio_receive = HAL_GetTick();
    statistics.audioPacketsReceived++;
    
    if (on_audio_data)
    {
        on_audio_data(packet->data, packet->dataLength);
    }
}

/**
 * @brief Traite un SMS entrant
 */
static void handle_sms(LoRaPacket* packet)
{
    printf("[LORA] SMS reçu de %s\n", packet->sender);
    
    // Envoyer un ACK
    lora_driver_send_ack(packet->packetId, packet->sender);
    
    statistics.smsReceived++;
    
    // Ajouter au cache réseau
    add_or_update_known_phone(packet->sender, packet->sender, sx1278_hal_get_packet_rssi());
    
    if (on_sms_received)
    {
        // Ajouter un terminateur de chaîne
        packet->data[packet->dataLength] = '\0';
        on_sms_received(packet->sender, (char*)packet->data);
    }
}

/**
 * @brief Traite une découverte réseau
 */
static void handle_discovery(LoRaPacket* packet)
{
    // Ajouter l'expéditeur au cache
    const char* name = (packet->dataLength > 0) ? (char*)packet->data : packet->sender;
    add_or_update_known_phone(packet->sender, name, sx1278_hal_get_packet_rssi());
    
    // Répondre avec notre présence
    LoRaPacket response;
    build_packet_header(&response, PACKET_DISCOVERY, packet->sender);
    strncpy((char*)response.data, local_device_name, 15);
    response.dataLength = strlen(local_device_name);
    lora_driver_send_packet(&response, false);
}

/**
 * @brief Traite un ping
 */
static void handle_ping(LoRaPacket* packet)
{
    // Répondre avec un pong
    LoRaPacket pong;
    build_packet_header(&pong, PACKET_PONG, packet->sender);
    pong.dataLength = 0;
    lora_driver_send_packet(&pong, false);
}

/**
 * @brief Traite un accusé de réception
 */
static void handle_ack(LoRaPacket* packet)
{
    // Pour l'instant, on ne fait rien de spécial
}

// ============================================================
// SECTION 10 : GESTION DU CACHE RÉSEAU
// ============================================================

/**
 * @brief Ajoute ou met à jour un téléphone connu
 */
static void add_or_update_known_phone(const char* number, const char* name, int16_t rssi)
{
    LoRaKnownPhone* phone = find_known_phone(number);
    
    if (phone)
    {
        // Mettre à jour
        phone->lastSeen = HAL_GetTick();
        phone->rssi = rssi;
        phone->online = true;
    }
    else if (known_phone_count < LORA_MAX_KNOWN_PHONES)
    {
        // Ajouter
        phone = &known_phones[known_phone_count++];
        strncpy(phone->phoneNumber, number, 15);
        strncpy(phone->deviceName, name, 15);
        phone->lastSeen = HAL_GetTick();
        phone->rssi = rssi;
        phone->snr = 0;
        phone->online = true;
        phone->missedCalls = 0;
        
        if (on_phone_discovered) on_phone_discovered(phone);
    }
}

/**
 * @brief Recherche un téléphone connu
 */
static LoRaKnownPhone* find_known_phone(const char* number)
{
    for (uint8_t i = 0; i < known_phone_count; i++)
    {
        if (strcmp(known_phones[i].phoneNumber, number) == 0)
        {
            return &known_phones[i];
        }
    }
    return NULL;
}

// ============================================================
// SECTION 11 : GESTION D'ÉNERGIE
// ============================================================

/**
 * @brief Met le module en veille
 */
void lora_driver_sleep(void)
{
    sx1278_hal_sleep();
}

/**
 * @brief Réveille le module
 */
void lora_driver_wakeup(void)
{
    sx1278_hal_wakeup();
    sx1278_hal_start_receive();
}

/**
 * @brief Vérifie si le module est en veille
 */
bool lora_driver_is_sleeping(void)
{
    return sx1278_hal_is_sleeping();
}

// ============================================================
// SECTION 12 : ENREGISTREMENT DES CALLBACKS
// ============================================================

/**
 * @brief Enregistre les callbacks
 */
void lora_driver_set_callbacks(
    LoRa_OnIncomingCall incomingCall,
    LoRa_OnCallAccepted callAccepted,
    LoRa_OnCallRejected callRejected,
    LoRa_OnCallEnded callEnded,
    LoRa_OnAudioData audioData,
    LoRa_OnSMSReceived smsReceived,
    LoRa_OnPhoneDiscovered phoneDiscovered,
    LoRa_OnError error)
{
    on_incoming_call    = incomingCall;
    on_call_accepted    = callAccepted;
    on_call_rejected    = callRejected;
    on_call_ended       = callEnded;
    on_audio_data       = audioData;
    on_sms_received     = smsReceived;
    on_phone_discovered = phoneDiscovered;
    on_error            = error;
}