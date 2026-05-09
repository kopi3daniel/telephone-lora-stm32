/**
 * @file lora_packet.h
 * @brief Gestion des paquets LoRa - Structures et fonctions de manipulation
 * 
 * Ce fichier contient :
 * - La définition complète du format des paquets
 * - Les structures de données pour chaque type de paquet
 * - Les fonctions de construction (build) et d'analyse (parse)
 * - Les fonctions de validation et de débogage
 * 
 * Format binaire d'un paquet LoRa (39 octets minimum) :
 * ┌────────┬──────────┬────────────┬────────────┬──────────┬──────────┐
 * │ Type   │ PacketID │  Sender    │  Receiver  │ DataLen  │  Data    │
 * │ 1 octet│ 4 octets │ 16 octets  │ 16 octets  │ 2 octets │ 0-216 oct│
 * └────────┴──────────┴────────────┴────────────┴──────────┴──────────┘
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef LORA_PACKET_H
#define LORA_PACKET_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "sx1278_defs.h"
#include "../../config.h"
#include "../../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES DES PAQUETS
// ============================================================

/** @brief Taille maximale d'un paquet complet (255 octets FIFO) */
#define PACKET_MAX_SIZE                 255

/** @brief Taille de l'en-tête (header) */
#define PACKET_HEADER_SIZE              39

/** @brief Taille maximale des données utiles (payload) */
#define PACKET_MAX_PAYLOAD_SIZE         (PACKET_MAX_SIZE - PACKET_HEADER_SIZE)  // 216 octets

/** @brief Taille d'un identifiant (numéro de téléphone) */
#define PACKET_ID_SIZE                  16

/** @brief Adresse de broadcast */
#define PACKET_BROADCAST_ADDRESS        "FFFFFFFFFFFF"

/** @brief Version du protocole */
#define PACKET_PROTOCOL_VERSION         0x01

// ============================================================
// SECTION 2 : TYPES DE PAQUETS (ENUM)
// ============================================================

/**
 * @brief Types de paquets du protocole téléphonique LoRa
 */
typedef enum {
    // --- Appels (0x10-0x1F) ---
    PACKET_CALL_REQUEST     = 0x10,  // Demande d'appel
    PACKET_CALL_ACCEPT      = 0x11,  // Acceptation d'appel
    PACKET_CALL_REJECT      = 0x12,  // Refus d'appel
    PACKET_CALL_BUSY        = 0x13,  // Occupé
    PACKET_CALL_END         = 0x14,  // Fin d'appel
    PACKET_CALL_MISSED      = 0x15,  // Appel manqué (notification)
    PACKET_CALL_HOLD        = 0x16,  // Mise en attente
    PACKET_CALL_RESUME      = 0x17,  // Reprise d'appel

    // --- Audio (0x20-0x2F) ---
    PACKET_AUDIO_DATA       = 0x20,  // Données audio brutes
    PACKET_AUDIO_COMPRESSED = 0x21,  // Données audio compressées (ADPCM)
    PACKET_AUDIO_SILENCE    = 0x22,  // Silence (pas de parole)
    PACKET_AUDIO_START      = 0x23,  // Début de flux audio
    PACKET_AUDIO_STOP       = 0x24,  // Fin de flux audio

    // --- Messages (0x30-0x3F) ---
    PACKET_SMS              = 0x30,  // Message texte
    PACKET_SMS_ACK          = 0x31,  // Accusé réception SMS
    PACKET_SMS_DELIVERY     = 0x32,  // Notification de livraison
    PACKET_SMS_READ         = 0x33,  // Notification de lecture

    // --- Réseau (0x40-0x4F) ---
    PACKET_DISCOVERY        = 0x40,  // Découverte réseau (broadcast)
    PACKET_DISCOVERY_REPLY  = 0x41,  // Réponse découverte
    PACKET_PING             = 0x42,  // Test de connexion
    PACKET_PONG             = 0x43,  // Réponse ping
    PACKET_KEEPALIVE        = 0x44,  // Maintien de connexion
    PACKET_PRESENCE         = 0x45,  // Annonce de présence

    // --- Contacts (0x50-0x5F) ---
    PACKET_CONTACT_SHARE    = 0x50,  // Partage de contact
    PACKET_CONTACT_REQUEST  = 0x51,  // Demande de contact

    // --- Fichiers (0x60-0x6F) ---
    PACKET_FILE_START       = 0x60,  // Début transfert fichier
    PACKET_FILE_DATA        = 0x61,  // Données fichier
    PACKET_FILE_END         = 0x62,  // Fin transfert fichier
    PACKET_FILE_ACK         = 0x63,  // Accusé réception fichier

    // --- Système (0xF0-0xFF) ---
    PACKET_ACK              = 0xF0,  // Accusé réception générique
    PACKET_NACK             = 0xF1,  // Accusé réception négatif
    PACKET_ERROR            = 0xFE,  // Erreur
    PACKET_DEBUG            = 0xFF   // Débogage

} LoraPacketType;

// ============================================================
// SECTION 3 : RAISONS DE FIN/REFUS D'APPEL
// ============================================================

/**
 * @brief Codes de raison pour les refus/fins d'appel
 */
typedef enum {
    CALL_REASON_USER_REJECT     = 0x00,  // Refusé par l'utilisateur
    CALL_REASON_BUSY            = 0x01,  // Ligne occupée
    CALL_REASON_NO_ANSWER       = 0x02,  // Pas de réponse (timeout)
    CALL_REASON_NETWORK_ERROR   = 0x03,  // Erreur réseau
    CALL_REASON_POWER_OFF       = 0x04,  // Téléphone éteint
    CALL_REASON_OUT_OF_RANGE    = 0x05,  // Hors de portée
    CALL_REASON_USER_END        = 0x06,  // Raccroché par l'utilisateur
    CALL_REASON_AUDIO_ERROR     = 0x07,  // Erreur audio
    CALL_REASON_EMERGENCY       = 0x08,  // Interrompu par appel urgence
    CALL_REASON_UNKNOWN         = 0xFF   // Raison inconnue
} CallReason;

// ============================================================
// SECTION 4 : STRUCTURE PRINCIPALE D'UN PAQUET
// ============================================================

/**
 * @brief Structure complète d'un paquet LoRa
 * 
 * Format binaire (big-endian) :
 * ┌────────┬──────────┬────────────┬────────────┬──────────┬──────────┐
 * │ Type   │ PacketID │  Sender    │  Receiver  │ DataLen  │  Data    │
 * │ 1 octet│ 4 octets │ 16 octets  │ 16 octets  │ 2 octets │ 0-216 oct│
 * └────────┴──────────┴────────────┴────────────┴──────────┴──────────┘
 * 
 * Taille totale : 39 + dataLength octets (max 255)
 */
typedef struct __attribute__((packed)) {
    uint8_t type;                       // Type de paquet (LoraPacketType)
    uint32_t packetId;                  // Identifiant unique (incrémental)
    char sender[PACKET_ID_SIZE];        // Numéro de l'expéditeur
    char receiver[PACKET_ID_SIZE];      // Numéro du destinataire
    uint16_t dataLength;                // Longueur des données utiles
    uint8_t data[PACKET_MAX_PAYLOAD_SIZE];  // Données utiles (max 216 octets)
} LoraPacket;

// ============================================================
// SECTION 5 : STRUCTURES SPÉCIFIQUES PAR TYPE DE PAQUET
// ============================================================

/**
 * @brief Données d'un paquet d'appel (CALL_REQUEST)
 */
typedef struct __attribute__((packed)) {
    uint8_t protocolVersion;        // Version du protocole
    char callerName[16];            // Nom de l'appelant
    uint32_t capabilities;          // Capacités (flags)
    uint8_t priority;               // Priorité (0=normal, 255=urgence)
} CallRequestData;

/**
 * @brief Données d'un paquet de refus d'appel (CALL_REJECT)
 */
typedef struct __attribute__((packed)) {
    uint8_t reason;                 // Code de raison (CallReason)
    char message[32];               // Message optionnel
} CallRejectData;

/**
 * @brief Données d'un paquet audio
 */
typedef struct __attribute__((packed)) {
    uint16_t sequenceNumber;        // Numéro de séquence (détection perte)
    uint8_t compressionType;        // Type de compression (0=aucune, 1=ADPCM)
    uint8_t sampleRate;             // Fréquence d'échantillonnage (kHz)
    uint8_t bitDepth;               // Résolution (bits)
    uint8_t channels;               // Nombre de canaux (1=mono)
    uint8_t audioData[200];         // Données audio
} AudioPacketData;

/**
 * @brief Données d'un paquet SMS
 */
typedef struct __attribute__((packed)) {
    uint32_t smsId;                 // Identifiant unique du SMS
    uint32_t timestamp;             // Horodatage (epoch)
    uint8_t encoding;               // Encodage (0=ASCII, 1=UTF-8)
    uint16_t messageLength;         // Longueur du message
    char message[200];              // Contenu du message
} SMSPacketData;

/**
 * @brief Données d'un paquet de découverte
 */
typedef struct __attribute__((packed)) {
    uint8_t protocolVersion;        // Version du protocole
    char deviceName[16];            // Nom du dispositif
    char deviceModel[16];           // Modèle du dispositif
    uint32_t capabilities;          // Capacités (flags)
    uint8_t batteryLevel;           // Niveau de batterie (0-100)
    int16_t signalStrength;         // Force du signal (RSSI)
    uint32_t uptime;                // Temps de fonctionnement (secondes)
} DiscoveryData;

/**
 * @brief Données d'un paquet de présence
 */
typedef struct __attribute__((packed)) {
    uint8_t batteryLevel;           // Niveau de batterie (0-100)
    uint8_t status;                 // Statut (0=disponible, 1=occupé, 2=absent)
    char statusMessage[32];         // Message de statut
} PresenceData;

/**
 * @brief Données d'un paquet d'accusé de réception
 */
typedef struct __attribute__((packed)) {
    uint32_t originalPacketId;      // ID du paquet acquitté
    uint8_t status;                 // Statut (0=OK, 1=erreur)
    uint8_t errorCode;              // Code d'erreur si applicable
} AckData;

/**
 * @brief Données d'un paquet d'erreur
 */
typedef struct __attribute__((packed)) {
    uint8_t errorCode;              // Code d'erreur
    uint32_t originalPacketId;      // ID du paquet ayant causé l'erreur
    char errorMessage[64];          // Message d'erreur
} ErrorData;

// ============================================================
// SECTION 6 : CAPACITÉS (FLAGS)
// ============================================================

/**
 * @brief Flags de capacités du dispositif
 */
typedef enum {
    CAP_AUDIO_CALL      = (1 << 0),   // Support appels audio
    CAP_SMS             = (1 << 1),   // Support SMS
    CAP_ENCRYPTION      = (1 << 2),   // Support chiffrement
    CAP_FILE_TRANSFER   = (1 << 3),   // Support transfert fichiers
    CAP_CONFERENCE      = (1 << 4),   // Support appel conférence
    CAP_GPS             = (1 << 5),   // Support GPS
    CAP_REPEATER        = (1 << 6),   // Mode répéteur
    CAP_EMERGENCY       = (1 << 7),   // Appels d'urgence
    CAP_LOW_POWER       = (1 << 8),   // Mode basse consommation
    CAP_FIRMWARE_UPDATE = (1 << 9)    // Support mise à jour firmware
} DeviceCapability;

// ============================================================
// SECTION 7 : FONCTIONS DE CONSTRUCTION (BUILD)
// ============================================================

/**
 * @brief Initialise un paquet vide
 * @param packet Paquet à initialiser
 */
void lora_packet_init(LoraPacket* packet);

/**
 * @brief Construit un paquet d'appel
 * @param packet Paquet à remplir
 * @param receiver Numéro du destinataire
 * @param callerName Nom de l'appelant
 * @return true si succès
 */
bool lora_packet_build_call_request(LoraPacket* packet, 
                                     const char* receiver,
                                     const char* callerName);

/**
 * @brief Construit un paquet d'acceptation d'appel
 */
bool lora_packet_build_call_accept(LoraPacket* packet, const char* receiver);

/**
 * @brief Construit un paquet de refus d'appel
 */
bool lora_packet_build_call_reject(LoraPacket* packet, 
                                    const char* receiver, 
                                    CallReason reason);

/**
 * @brief Construit un paquet de fin d'appel
 */
bool lora_packet_build_call_end(LoraPacket* packet, 
                                 const char* receiver, 
                                 CallReason reason);

/**
 * @brief Construit un paquet audio
 */
bool lora_packet_build_audio(LoraPacket* packet,
                              const char* receiver,
                              uint8_t* audioData,
                              uint16_t audioLength,
                              uint16_t sequenceNumber);

/**
 * @brief Construit un paquet SMS
 */
bool lora_packet_build_sms(LoraPacket* packet,
                            const char* receiver,
                            const char* message);

/**
 * @brief Construit un paquet de découverte
 */
bool lora_packet_build_discovery(LoraPacket* packet,
                                  const char* deviceName,
                                  uint32_t capabilities);

/**
 * @brief Construit un paquet de présence
 */
bool lora_packet_build_presence(LoraPacket* packet,
                                 uint8_t batteryLevel,
                                 uint8_t status);

/**
 * @brief Construit un accusé de réception
 */
bool lora_packet_build_ack(LoraPacket* packet,
                            const char* receiver,
                            uint32_t originalPacketId);

/**
 * @brief Construit un paquet d'erreur
 */
bool lora_packet_build_error(LoraPacket* packet,
                              const char* receiver,
                              uint8_t errorCode,
                              const char* message);

// ============================================================
// SECTION 8 : FONCTIONS D'ANALYSE (PARSE)
// ============================================================

/**
 * @brief Extrait les données d'appel d'un paquet
 * @param packet Paquet reçu
 * @param data Structure à remplir
 * @return true si le paquet est valide
 */
bool lora_packet_parse_call_request(LoraPacket* packet, CallRequestData* data);

/**
 * @brief Extrait les données audio d'un paquet
 */
bool lora_packet_parse_audio(LoraPacket* packet, AudioPacketData* data);

/**
 * @brief Extrait les données SMS d'un paquet
 */
bool lora_packet_parse_sms(LoraPacket* packet, SMSPacketData* data);

/**
 * @brief Extrait les données de découverte d'un paquet
 */
bool lora_packet_parse_discovery(LoraPacket* packet, DiscoveryData* data);

/**
 * @brief Extrait les données de présence d'un paquet
 */
bool lora_packet_parse_presence(LoraPacket* packet, PresenceData* data);

/**
 * @brief Extrait les données d'ACK d'un paquet
 */
bool lora_packet_parse_ack(LoraPacket* packet, AckData* data);

/**
 * @brief Extrait les données d'erreur d'un paquet
 */
bool lora_packet_parse_error(LoraPacket* packet, ErrorData* data);

// ============================================================
// SECTION 9 : FONCTIONS DE SÉRIALISATION/DÉSÉRIALISATION
// ============================================================

/**
 * @brief Sérialise un paquet en buffer binaire
 * @param packet Paquet source
 * @param buffer Buffer destination (doit faire au moins PACKET_MAX_SIZE octets)
 * @param length Pointeur vers la variable recevant la longueur
 * @return true si succès
 */
bool lora_packet_serialize(LoraPacket* packet, uint8_t* buffer, uint16_t* length);

/**
 * @brief Désérialise un buffer binaire en paquet
 * @param buffer Buffer source
 * @param length Longueur du buffer
 * @param packet Paquet destination
 * @return true si le buffer est valide
 */
bool lora_packet_deserialize(uint8_t* buffer, uint16_t length, LoraPacket* packet);

// ============================================================
// SECTION 10 : FONCTIONS DE VALIDATION
// ============================================================

/**
 * @brief Vérifie si un paquet est valide
 * @param packet Paquet à vérifier
 * @return true si valide
 */
bool lora_packet_is_valid(LoraPacket* packet);

/**
 * @brief Vérifie si un paquet nous est destiné
 * @param packet Paquet reçu
 * @param localNumber Notre numéro de téléphone
 * @return true si le paquet est pour nous
 */
bool lora_packet_is_for_us(LoraPacket* packet, const char* localNumber);

/**
 * @brief Vérifie si un paquet est un broadcast
 * @param packet Paquet à vérifier
 * @return true si broadcast
 */
bool lora_packet_is_broadcast(LoraPacket* packet);

/**
 * @brief Vérifie le type d'un paquet
 * @param packet Paquet à vérifier
 * @param type Type attendu
 * @return true si correspond
 */
bool lora_packet_is_type(LoraPacket* packet, LoraPacketType type);

/**
 * @brief Calcule la taille totale d'un paquet
 * @param packet Paquet
 * @return Taille en octets
 */
uint16_t lora_packet_get_size(LoraPacket* packet);

// ============================================================
// SECTION 11 : FONCTIONS DE DÉBOGAGE
// ============================================================

/**
 * @brief Convertit un type de paquet en chaîne lisible
 * @param type Type de paquet
 * @return Chaîne descriptive
 */
const char* lora_packet_type_to_string(LoraPacketType type);

/**
 * @brief Convertit une raison d'appel en chaîne lisible
 * @param reason Code de raison
 * @return Chaîne descriptive
 */
const char* lora_packet_reason_to_string(CallReason reason);

/**
 * @brief Affiche le contenu d'un paquet (debug)
 * @param packet Paquet à afficher
 */
void lora_packet_print(LoraPacket* packet);

/**
 * @brief Affiche un résumé d'un paquet (une ligne)
 * @param packet Paquet à résumer
 */
void lora_packet_print_summary(LoraPacket* packet);

/**
 * @brief Affiche le contenu hexadécimal d'un buffer
 * @param buffer Buffer à afficher
 * @param length Longueur
 */
void lora_packet_hexdump(uint8_t* buffer, uint16_t length);

// ============================================================
// SECTION 12 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // LORA_PACKET_H