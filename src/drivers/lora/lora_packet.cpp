/**
 * @file lora_packet.cpp
 * @brief Implémentation des fonctions de gestion des paquets LoRa
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans lora_packet.h :
 * - Construction des paquets (build)
 * - Analyse des paquets (parse)
 * - Sérialisation/désérialisation
 * - Validation
 * - Fonctions de débogage
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "lora_packet.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief Compteur global d'ID de paquets */
static uint32_t global_packet_id = 0;

/** @brief Numéro de téléphone local (pour les fonctions build) */
static char local_phone_number[PACKET_ID_SIZE] = "0600000000";

// ============================================================
// SECTION 1 : INITIALISATION ET UTILITAIRES
// ============================================================

/**
 * @brief Initialise un paquet vide
 */
void lora_packet_init(LoraPacket* packet)
{
    if (packet == NULL) return;
    
    memset(packet, 0, sizeof(LoraPacket));
    packet->packetId = ++global_packet_id;
}

/**
 * @brief Définit le numéro de téléphone local
 */
void lora_packet_set_local_number(const char* number)
{
    if (number != NULL)
    {
        strncpy(local_phone_number, number, PACKET_ID_SIZE - 1);
        local_phone_number[PACKET_ID_SIZE - 1] = '\0';
    }
}

/**
 * @brief Récupère le numéro de téléphone local
 */
const char* lora_packet_get_local_number(void)
{
    return local_phone_number;
}

// ============================================================
// SECTION 2 : FONCTIONS DE CONSTRUCTION (BUILD)
// ============================================================

/**
 * @brief Construit un paquet de demande d'appel
 */
bool lora_packet_build_call_request(LoraPacket* packet, 
                                     const char* receiver,
                                     const char* callerName)
{
    if (packet == NULL || receiver == NULL) return false;
    
    lora_packet_init(packet);
    packet->type = PACKET_CALL_REQUEST;
    strncpy(packet->sender, local_phone_number, PACKET_ID_SIZE - 1);
    strncpy(packet->receiver, receiver, PACKET_ID_SIZE - 1);
    
    // Ajouter les données d'appel
    CallRequestData data;
    memset(&data, 0, sizeof(data));
    data.protocolVersion = PACKET_PROTOCOL_VERSION;
    strncpy(data.callerName, callerName ? callerName : "Inconnu", 15);
    data.capabilities = CAP_AUDIO_CALL | CAP_SMS;
    data.priority = 0;  // Appel normal
    
    packet->dataLength = sizeof(CallRequestData);
    memcpy(packet->data, &data, sizeof(CallRequestData));
    
    return true;
}

/**
 * @brief Construit un paquet d'acceptation d'appel
 */
bool lora_packet_build_call_accept(LoraPacket* packet, const char* receiver)
{
    if (packet == NULL || receiver == NULL) return false;
    
    lora_packet_init(packet);
    packet->type = PACKET_CALL_ACCEPT;
    strncpy(packet->sender, local_phone_number, PACKET_ID_SIZE - 1);
    strncpy(packet->receiver, receiver, PACKET_ID_SIZE - 1);
    packet->dataLength = 0;
    
    return true;
}

/**
 * @brief Construit un paquet de refus d'appel
 */
bool lora_packet_build_call_reject(LoraPacket* packet, 
                                    const char* receiver, 
                                    CallReason reason)
{
    if (packet == NULL || receiver == NULL) return false;
    
    lora_packet_init(packet);
    packet->type = PACKET_CALL_REJECT;
    strncpy(packet->sender, local_phone_number, PACKET_ID_SIZE - 1);
    strncpy(packet->receiver, receiver, PACKET_ID_SIZE - 1);
    
    // Ajouter les données de refus
    CallRejectData data;
    memset(&data, 0, sizeof(data));
    data.reason = reason;
    strncpy(data.message, lora_packet_reason_to_string(reason), 31);
    
    packet->dataLength = sizeof(CallRejectData);
    memcpy(packet->data, &data, sizeof(CallRejectData));
    
    return true;
}

/**
 * @brief Construit un paquet de fin d'appel
 */
bool lora_packet_build_call_end(LoraPacket* packet, 
                                 const char* receiver, 
                                 CallReason reason)
{
    if (packet == NULL || receiver == NULL) return false;
    
    lora_packet_init(packet);
    packet->type = PACKET_CALL_END;
    strncpy(packet->sender, local_phone_number, PACKET_ID_SIZE - 1);
    strncpy(packet->receiver, receiver, PACKET_ID_SIZE - 1);
    
    // Ajouter le code de raison
    packet->data[0] = reason;
    packet->dataLength = 1;
    
    return true;
}

/**
 * @brief Construit un paquet audio
 */
bool lora_packet_build_audio(LoraPacket* packet,
                              const char* receiver,
                              uint8_t* audioData,
                              uint16_t audioLength,
                              uint16_t sequenceNumber)
{
    if (packet == NULL || receiver == NULL || audioData == NULL) return false;
    if (audioLength > 200) audioLength = 200;  // Limite
    
    lora_packet_init(packet);
    packet->type = PACKET_AUDIO_DATA;
    strncpy(packet->sender, local_phone_number, PACKET_ID_SIZE - 1);
    strncpy(packet->receiver, receiver, PACKET_ID_SIZE - 1);
    
    // Construire l'en-tête audio
    AudioPacketData audio;
    audio.sequenceNumber = sequenceNumber;
    audio.compressionType = 0;   // Pas de compression
    audio.sampleRate = 8;        // 8 kHz
    audio.bitDepth = 8;          // 8 bits
    audio.channels = 1;          // Mono
    
    uint16_t headerSize = sizeof(AudioPacketData) - 200;  // Taille sans le buffer
    memcpy(packet->data, &audio, headerSize);
    memcpy(packet->data + headerSize, audioData, audioLength);
    
    packet->dataLength = headerSize + audioLength;
    
    return true;
}

/**
 * @brief Construit un paquet SMS
 */
bool lora_packet_build_sms(LoraPacket* packet,
                            const char* receiver,
                            const char* message)
{
    if (packet == NULL || receiver == NULL || message == NULL) return false;
    
    uint16_t msgLen = strlen(message);
    if (msgLen > 200) msgLen = 200;  // Limite
    
    lora_packet_init(packet);
    packet->type = PACKET_SMS;
    strncpy(packet->sender, local_phone_number, PACKET_ID_SIZE - 1);
    strncpy(packet->receiver, receiver, PACKET_ID_SIZE - 1);
    
    // Construire les données SMS
    SMSPacketData sms;
    sms.smsId = packet->packetId;
    sms.timestamp = (uint32_t)time(NULL);
    sms.encoding = 0;  // ASCII
    sms.messageLength = msgLen;
    memcpy(sms.message, message, msgLen);
    sms.message[msgLen] = '\0';
    
    uint16_t headerSize = sizeof(SMSPacketData) - 200;
    memcpy(packet->data, &sms, headerSize + msgLen);
    packet->dataLength = headerSize + msgLen;
    
    return true;
}

/**
 * @brief Construit un paquet de découverte
 */
bool lora_packet_build_discovery(LoraPacket* packet,
                                  const char* deviceName,
                                  uint32_t capabilities)
{
    if (packet == NULL) return false;
    
    lora_packet_init(packet);
    packet->type = PACKET_DISCOVERY;
    strncpy(packet->sender, local_phone_number, PACKET_ID_SIZE - 1);
    strncpy(packet->receiver, PACKET_BROADCAST_ADDRESS, PACKET_ID_SIZE - 1);
    
    // Construire les données de découverte
    DiscoveryData data;
    memset(&data, 0, sizeof(data));
    data.protocolVersion = PACKET_PROTOCOL_VERSION;
    strncpy(data.deviceName, deviceName ? deviceName : "LoRaPhone", 15);
    strncpy(data.deviceModel, "LP-STM32F429", 15);
    data.capabilities = capabilities;
    data.batteryLevel = 100;  // À remplacer par la vraie valeur
    data.signalStrength = 0;
    data.uptime = HAL_GetTick() / 1000;
    
    packet->dataLength = sizeof(DiscoveryData);
    memcpy(packet->data, &data, sizeof(DiscoveryData));
    
    return true;
}

/**
 * @brief Construit un paquet de présence
 */
bool lora_packet_build_presence(LoraPacket* packet,
                                 uint8_t batteryLevel,
                                 uint8_t status)
{
    if (packet == NULL) return false;
    
    lora_packet_init(packet);
    packet->type = PACKET_PRESENCE;
    strncpy(packet->sender, local_phone_number, PACKET_ID_SIZE - 1);
    strncpy(packet->receiver, PACKET_BROADCAST_ADDRESS, PACKET_ID_SIZE - 1);
    
    PresenceData data;
    memset(&data, 0, sizeof(data));
    data.batteryLevel = batteryLevel;
    data.status = status;
    
    packet->dataLength = sizeof(PresenceData);
    memcpy(packet->data, &data, sizeof(PresenceData));
    
    return true;
}

/**
 * @brief Construit un accusé de réception
 */
bool lora_packet_build_ack(LoraPacket* packet,
                            const char* receiver,
                            uint32_t originalPacketId)
{
    if (packet == NULL || receiver == NULL) return false;
    
    lora_packet_init(packet);
    packet->type = PACKET_ACK;
    strncpy(packet->sender, local_phone_number, PACKET_ID_SIZE - 1);
    strncpy(packet->receiver, receiver, PACKET_ID_SIZE - 1);
    
    AckData ack;
    ack.originalPacketId = originalPacketId;
    ack.status = 0;  // OK
    ack.errorCode = 0;
    
    packet->dataLength = sizeof(AckData);
    memcpy(packet->data, &ack, sizeof(AckData));
    
    return true;
}

/**
 * @brief Construit un paquet d'erreur
 */
bool lora_packet_build_error(LoraPacket* packet,
                              const char* receiver,
                              uint8_t errorCode,
                              const char* message)
{
    if (packet == NULL) return false;
    
    lora_packet_init(packet);
    packet->type = PACKET_ERROR;
    strncpy(packet->sender, local_phone_number, PACKET_ID_SIZE - 1);
    
    if (receiver != NULL) {
        strncpy(packet->receiver, receiver, PACKET_ID_SIZE - 1);
    }
    
    ErrorData error;
    error.errorCode = errorCode;
    error.originalPacketId = 0;
    
    if (message != NULL) {
        strncpy(error.errorMessage, message, 63);
        error.errorMessage[63] = '\0';
    } else {
        error.errorMessage[0] = '\0';
    }
    
    packet->dataLength = sizeof(ErrorData);
    memcpy(packet->data, &error, sizeof(ErrorData));
    
    return true;
}

// ============================================================
// SECTION 3 : FONCTIONS D'ANALYSE (PARSE)
// ============================================================

/**
 * @brief Extrait les données d'appel
 */
bool lora_packet_parse_call_request(LoraPacket* packet, CallRequestData* data)
{
    if (packet == NULL || data == NULL) return false;
    if (packet->type != PACKET_CALL_REQUEST) return false;
    if (packet->dataLength < sizeof(CallRequestData)) return false;
    
    memcpy(data, packet->data, sizeof(CallRequestData));
    return true;
}

/**
 * @brief Extrait les données audio
 */
bool lora_packet_parse_audio(LoraPacket* packet, AudioPacketData* data)
{
    if (packet == NULL || data == NULL) return false;
    if (packet->type != PACKET_AUDIO_DATA) return false;
    
    uint16_t headerSize = sizeof(AudioPacketData) - 200;
    if (packet->dataLength < headerSize) return false;
    
    memset(data, 0, sizeof(AudioPacketData));
    memcpy(data, packet->data, headerSize);
    
    uint16_t audioLen = packet->dataLength - headerSize;
    if (audioLen > 200) audioLen = 200;
    memcpy(data->audioData, packet->data + headerSize, audioLen);
    
    return true;
}

/**
 * @brief Extrait les données SMS
 */
bool lora_packet_parse_sms(LoraPacket* packet, SMSPacketData* data)
{
    if (packet == NULL || data == NULL) return false;
    if (packet->type != PACKET_SMS) return false;
    
    uint16_t headerSize = sizeof(SMSPacketData) - 200;
    if (packet->dataLength < headerSize) return false;
    
    memset(data, 0, sizeof(SMSPacketData));
    memcpy(data, packet->data, packet->dataLength);
    
    // S'assurer que le message est terminé par un '\0'
    if (data->messageLength < 200) {
        data->message[data->messageLength] = '\0';
    }
    
    return true;
}

/**
 * @brief Extrait les données de découverte
 */
bool lora_packet_parse_discovery(LoraPacket* packet, DiscoveryData* data)
{
    if (packet == NULL || data == NULL) return false;
    if (packet->type != PACKET_DISCOVERY && packet->type != PACKET_DISCOVERY_REPLY) return false;
    if (packet->dataLength < sizeof(DiscoveryData)) return false;
    
    memcpy(data, packet->data, sizeof(DiscoveryData));
    return true;
}

/**
 * @brief Extrait les données de présence
 */
bool lora_packet_parse_presence(LoraPacket* packet, PresenceData* data)
{
    if (packet == NULL || data == NULL) return false;
    if (packet->type != PACKET_PRESENCE) return false;
    if (packet->dataLength < sizeof(PresenceData)) return false;
    
    memcpy(data, packet->data, sizeof(PresenceData));
    return true;
}

/**
 * @brief Extrait les données d'ACK
 */
bool lora_packet_parse_ack(LoraPacket* packet, AckData* data)
{
    if (packet == NULL || data == NULL) return false;
    if (packet->type != PACKET_ACK && packet->type != PACKET_NACK) return false;
    if (packet->dataLength < sizeof(AckData)) return false;
    
    memcpy(data, packet->data, sizeof(AckData));
    return true;
}

/**
 * @brief Extrait les données d'erreur
 */
bool lora_packet_parse_error(LoraPacket* packet, ErrorData* data)
{
    if (packet == NULL || data == NULL) return false;
    if (packet->type != PACKET_ERROR) return false;
    if (packet->dataLength < sizeof(ErrorData)) return false;
    
    memcpy(data, packet->data, sizeof(ErrorData));
    return true;
}

// ============================================================
// SECTION 4 : SÉRIALISATION/DÉSÉRIALISATION
// ============================================================

/**
 * @brief Sérialise un paquet en buffer binaire
 */
bool lora_packet_serialize(LoraPacket* packet, uint8_t* buffer, uint16_t* length)
{
    if (packet == NULL || buffer == NULL || length == NULL) return false;
    
    uint16_t totalSize = PACKET_HEADER_SIZE + packet->dataLength;
    
    if (totalSize > PACKET_MAX_SIZE) {
        printf("[PACKET] Erreur: paquet trop grand (%d octets)\n", totalSize);
        return false;
    }
    
    uint8_t* ptr = buffer;
    
    // Octet 0 : Type
    *ptr++ = packet->type;
    
    // Octets 1-4 : Packet ID (big-endian)
    *ptr++ = (packet->packetId >> 24) & 0xFF;
    *ptr++ = (packet->packetId >> 16) & 0xFF;
    *ptr++ = (packet->packetId >> 8) & 0xFF;
    *ptr++ = packet->packetId & 0xFF;
    
    // Octets 5-20 : Sender (16 octets)
    memcpy(ptr, packet->sender, PACKET_ID_SIZE);
    ptr += PACKET_ID_SIZE;
    
    // Octets 21-36 : Receiver (16 octets)
    memcpy(ptr, packet->receiver, PACKET_ID_SIZE);
    ptr += PACKET_ID_SIZE;
    
    // Octets 37-38 : Data Length (big-endian)
    *ptr++ = (packet->dataLength >> 8) & 0xFF;
    *ptr++ = packet->dataLength & 0xFF;
    
    // Octets 39+ : Data
    if (packet->dataLength > 0)
    {
        memcpy(ptr, packet->data, packet->dataLength);
        ptr += packet->dataLength;
    }
    
    *length = totalSize;
    return true;
}

/**
 * @brief Désérialise un buffer binaire en paquet
 */
bool lora_packet_deserialize(uint8_t* buffer, uint16_t length, LoraPacket* packet)
{
    if (buffer == NULL || packet == NULL) return false;
    if (length < PACKET_HEADER_SIZE) {
        printf("[PACKET] Erreur: buffer trop court (%d octets)\n", length);
        return false;
    }
    
    memset(packet, 0, sizeof(LoraPacket));
    
    uint8_t* ptr = buffer;
    
    // Octet 0 : Type
    packet->type = *ptr++;
    
    // Octets 1-4 : Packet ID (big-endian)
    packet->packetId  = ((uint32_t)*ptr++ << 24);
    packet->packetId |= ((uint32_t)*ptr++ << 16);
    packet->packetId |= ((uint32_t)*ptr++ << 8);
    packet->packetId |= *ptr++;
    
    // Octets 5-20 : Sender (16 octets)
    memcpy(packet->sender, ptr, PACKET_ID_SIZE);
    packet->sender[PACKET_ID_SIZE - 1] = '\0';  // Sécurité
    ptr += PACKET_ID_SIZE;
    
    // Octets 21-36 : Receiver (16 octets)
    memcpy(packet->receiver, ptr, PACKET_ID_SIZE);
    packet->receiver[PACKET_ID_SIZE - 1] = '\0';  // Sécurité
    ptr += PACKET_ID_SIZE;
    
    // Octets 37-38 : Data Length (big-endian)
    packet->dataLength  = ((uint16_t)*ptr++ << 8);
    packet->dataLength |= *ptr++;
    
    // Vérifier la longueur
    if (packet->dataLength > PACKET_MAX_PAYLOAD_SIZE) {
        printf("[PACKET] Erreur: dataLength trop grand (%d)\n", packet->dataLength);
        packet->dataLength = PACKET_MAX_PAYLOAD_SIZE;
    }
    
    // Vérifier que le buffer est assez long
    uint16_t expectedLength = PACKET_HEADER_SIZE + packet->dataLength;
    if (length < expectedLength) {
        printf("[PACKET] Attention: buffer plus court qu'annoncé (%d < %d)\n", length, expectedLength);
        packet->dataLength = length - PACKET_HEADER_SIZE;
    }
    
    // Octets 39+ : Data
    if (packet->dataLength > 0)
    {
        memcpy(packet->data, ptr, packet->dataLength);
    }
    
    return true;
}

// ============================================================
// SECTION 5 : VALIDATION
// ============================================================

/**
 * @brief Vérifie si un paquet est valide
 */
bool lora_packet_is_valid(LoraPacket* packet)
{
    if (packet == NULL) return false;
    
    // Vérifier le type
    if (packet->type > 0xFF) return false;
    
    // Vérifier la longueur des données
    if (packet->dataLength > PACKET_MAX_PAYLOAD_SIZE) return false;
    
    // Vérifier les identifiants
    if (strlen(packet->sender) == 0) return false;
    
    return true;
}

/**
 * @brief Vérifie si un paquet nous est destiné
 */
bool lora_packet_is_for_us(LoraPacket* packet, const char* localNumber)
{
    if (packet == NULL || localNumber == NULL) return false;
    
    // Vérifier si c'est pour nous
    if (strcmp(packet->receiver, localNumber) == 0) return true;
    
    // Vérifier si c'est un broadcast
    if (lora_packet_is_broadcast(packet)) return true;
    
    return false;
}

/**
 * @brief Vérifie si un paquet est un broadcast
 */
bool lora_packet_is_broadcast(LoraPacket* packet)
{
    if (packet == NULL) return false;
    
    // Vérifier l'adresse de broadcast
    if (strcmp(packet->receiver, PACKET_BROADCAST_ADDRESS) == 0) return true;
    if (strcmp(packet->receiver, "BROADCAST") == 0) return true;
    if (strcmp(packet->receiver, "ALL") == 0) return true;
    
    return false;
}

/**
 * @brief Vérifie le type d'un paquet
 */
bool lora_packet_is_type(LoraPacket* packet, LoraPacketType type)
{
    if (packet == NULL) return false;
    return (packet->type == type);
}

/**
 * @brief Calcule la taille totale d'un paquet
 */
uint16_t lora_packet_get_size(LoraPacket* packet)
{
    if (packet == NULL) return 0;
    return PACKET_HEADER_SIZE + packet->dataLength;
}

// ============================================================
// SECTION 6 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Convertit un type de paquet en chaîne
 */
const char* lora_packet_type_to_string(LoraPacketType type)
{
    switch (type)
    {
        // Appels
        case PACKET_CALL_REQUEST:    return "CALL_REQUEST";
        case PACKET_CALL_ACCEPT:     return "CALL_ACCEPT";
        case PACKET_CALL_REJECT:     return "CALL_REJECT";
        case PACKET_CALL_BUSY:       return "CALL_BUSY";
        case PACKET_CALL_END:        return "CALL_END";
        case PACKET_CALL_MISSED:     return "CALL_MISSED";
        
        // Audio
        case PACKET_AUDIO_DATA:      return "AUDIO_DATA";
        case PACKET_AUDIO_COMPRESSED:return "AUDIO_COMPRESSED";
        case PACKET_AUDIO_SILENCE:   return "AUDIO_SILENCE";
        
        // Messages
        case PACKET_SMS:             return "SMS";
        case PACKET_SMS_ACK:         return "SMS_ACK";
        case PACKET_SMS_DELIVERY:    return "SMS_DELIVERY";
        
        // Réseau
        case PACKET_DISCOVERY:       return "DISCOVERY";
        case PACKET_DISCOVERY_REPLY: return "DISCOVERY_REPLY";
        case PACKET_PING:            return "PING";
        case PACKET_PONG:            return "PONG";
        case PACKET_KEEPALIVE:       return "KEEPALIVE";
        case PACKET_PRESENCE:        return "PRESENCE";
        
        // Système
        case PACKET_ACK:             return "ACK";
        case PACKET_NACK:            return "NACK";
        case PACKET_ERROR:           return "ERROR";
        case PACKET_DEBUG:           return "DEBUG";
        
        default:                     return "UNKNOWN";
    }
}

/**
 * @brief Convertit une raison d'appel en chaîne
 */
const char* lora_packet_reason_to_string(CallReason reason)
{
    switch (reason)
    {
        case CALL_REASON_USER_REJECT:    return "Refusé par l'utilisateur";
        case CALL_REASON_BUSY:           return "Occupé";
        case CALL_REASON_NO_ANSWER:      return "Pas de réponse";
        case CALL_REASON_NETWORK_ERROR:  return "Erreur réseau";
        case CALL_REASON_POWER_OFF:      return "Téléphone éteint";
        case CALL_REASON_OUT_OF_RANGE:   return "Hors de portée";
        case CALL_REASON_USER_END:       return "Raccroché";
        case CALL_REASON_AUDIO_ERROR:    return "Erreur audio";
        case CALL_REASON_EMERGENCY:      return "Appel urgence";
        case CALL_REASON_UNKNOWN:
        default:                         return "Raison inconnue";
    }
}

/**
 * @brief Affiche le contenu complet d'un paquet
 */
void lora_packet_print(LoraPacket* packet)
{
    if (packet == NULL) return;
    
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║           CONTENU DU PAQUET                  ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Type       : %-31s ║\n", lora_packet_type_to_string((LoraPacketType)packet->type));
    printf("║ Type (hex) : 0x%02X                              ║\n", packet->type);
    printf("║ Packet ID  : %-10lu                      ║\n", (unsigned long)packet->packetId);
    printf("║ Expéditeur : %-31s ║\n", packet->sender);
    printf("║ Destinataire: %-31s ║\n", packet->receiver);
    printf("║ Data Length: %-5d octets                    ║\n", packet->dataLength);
    printf("╠══════════════════════════════════════════════╣\n");
    
    if (packet->dataLength > 0)
    {
        printf("║ Données (hex) :                              ║\n");
        printf("║ ");
        for (uint16_t i = 0; i < packet->dataLength && i < 64; i++)
        {
            printf("%02X ", packet->data[i]);
            if ((i + 1) % 16 == 0 && i < packet->dataLength - 1)
            {
                printf("\n║ ");
            }
        }
        printf("\n");
        
        // Afficher en ASCII si c'est du texte
        bool isText = true;
        for (uint16_t i = 0; i < packet->dataLength && i < 64; i++)
        {
            if (packet->data[i] < 32 && packet->data[i] != '\n' && packet->data[i] != '\r')
            {
                isText = false;
                break;
            }
        }
        
        if (isText && packet->dataLength > 0)
        {
            printf("║ Texte : %.*s\n", packet->dataLength, packet->data);
        }
    }
    
    printf("╚══════════════════════════════════════════════╝\n\n");
}

/**
 * @brief Affiche un résumé d'un paquet (une ligne)
 */
void lora_packet_print_summary(LoraPacket* packet)
{
    if (packet == NULL) return;
    
    printf("[PACKET] ID=%lu Type=%s From=%s To=%s Len=%d\n",
           (unsigned long)packet->packetId,
           lora_packet_type_to_string((LoraPacketType)packet->type),
           packet->sender,
           packet->receiver,
           packet->dataLength);
}

/**
 * @brief Affiche un dump hexadécimal d'un buffer
 */
void lora_packet_hexdump(uint8_t* buffer, uint16_t length)
{
    if (buffer == NULL) return;
    
    printf("═══ HEXDUMP (%d octets) ═══\n", length);
    
    for (uint16_t i = 0; i < length; i += 16)
    {
        // Adresse
        printf("%04X  ", i);
        
        // Hex
        for (uint16_t j = 0; j < 16; j++)
        {
            if (i + j < length)
                printf("%02X ", buffer[i + j]);
            else
                printf("   ");
            
            if (j == 7) printf(" ");
        }
        
        // ASCII
        printf(" |");
        for (uint16_t j = 0; j < 16 && (i + j) < length; j++)
        {
            uint8_t c = buffer[i + j];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        printf("|\n");
    }
    printf("══════════════════════════\n\n");
}