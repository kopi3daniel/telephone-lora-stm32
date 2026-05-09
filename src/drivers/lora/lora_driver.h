/**
 * @file lora_driver.h
 * @brief Driver LoRa haut niveau pour le téléphone
 * 
 * Ce fichier déclare l'interface haut niveau du driver LoRa.
 * Il encapsule toute la complexité du SX1278 derrière une API
 * simple et adaptée aux besoins du téléphone :
 * - Appels vocaux (mode audio temps réel)
 * - Messages SMS (mode longue portée)
 * - Découverte de réseau
 * - Gestion des paquets
 * 
 * Cette couche utilise sx1278_hal.h pour la communication
 * et sx1278_config.h pour les profils de configuration.
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef LORA_DRIVER_H
#define LORA_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include "sx1278_defs.h"
#include "sx1278_hal.h"
#include "sx1278_config.h"
#include "../../config.h"
#include "../../../include/project_config.h"

// ============================================================
// SECTION 1 : CONSTANTES DU DRIVER
// ============================================================

/** @brief Taille maximale d'un paquet de données */
#define LORA_MAX_PACKET_SIZE        255

/** @brief Taille maximale d'un paquet audio */
#define LORA_AUDIO_PACKET_SIZE      128

/** @brief Taille maximale d'un SMS */
#define LORA_SMS_MAX_LENGTH         160

/** @brief Nombre de tentatives de retransmission */
#define LORA_RETRY_COUNT            3

/** @brief Délai entre les tentatives (ms) */
#define LORA_RETRY_DELAY_MS         500

/** @brief Timeout par défaut pour les ACK (ms) */
#define LORA_ACK_TIMEOUT_MS         2000

/** @brief Intervalle de keep-alive (ms) */
#define LORA_KEEPALIVE_INTERVAL_MS  30000

/** @brief Nombre maximum de téléphones dans le cache */
#define LORA_MAX_KNOWN_PHONES       50

/** @brief Taille du buffer de réception */
#define LORA_RX_BUFFER_SIZE         256

/** @brief Taille du buffer de transmission */
#define LORA_TX_BUFFER_SIZE         256

// ============================================================
// SECTION 2 : TYPES DE PAQUETS DU TÉLÉPHONE
// ============================================================

/**
 * @brief Types de paquets utilisés par le téléphone
 * 
 * Chaque type correspond à un usage spécifique dans le protocole.
 */
typedef enum {
    // --- Appels ---
    PACKET_CALL_REQUEST = 0x10,      // Demande d'appel
    PACKET_CALL_ACCEPT  = 0x11,      // Appel accepté
    PACKET_CALL_REJECT  = 0x12,      // Appel refusé
    PACKET_CALL_BUSY    = 0x13,      // Occupé
    PACKET_CALL_END     = 0x14,      // Fin d'appel
    PACKET_CALL_MISSED  = 0x15,      // Appel manqué
    
    // --- Audio ---
    PACKET_AUDIO_DATA   = 0x20,      // Données audio (voix)
    PACKET_AUDIO_START  = 0x21,      // Début flux audio
    PACKET_AUDIO_STOP   = 0x22,      // Fin flux audio
    
    // --- Messages ---
    PACKET_SMS          = 0x30,      // Message texte
    PACKET_SMS_ACK      = 0x31,      // Accusé réception SMS
    
    // --- Réseau ---
    PACKET_DISCOVERY    = 0x40,      // Découverte réseau
    PACKET_PING         = 0x41,      // Test de connexion
    PACKET_PONG         = 0x42,      // Réponse ping
    PACKET_KEEPALIVE    = 0x43,      // Maintien connexion
    
    // --- Système ---
    PACKET_ACK          = 0xF0,      // Accusé réception générique
    PACKET_ERROR        = 0xFF       // Erreur
} LoRaPacketType;

// ============================================================
// SECTION 3 : STRUCTURES DE DONNÉES
// ============================================================

/**
 * @brief En-tête d'un paquet LoRa du téléphone
 * 
 * Format binaire :
 * - Octet 0    : Type de paquet (LoRaPacketType)
 * - Octets 1-4 : ID du paquet (32 bits)
 * - Octets 5-20: ID de l'expéditeur (16 octets max)
 * - Octets 21-36: ID du destinataire (16 octets max)
 * - Octets 37-38: Longueur des données (16 bits)
 * - Octets 39+  : Données (maximum 216 octets)
 */
typedef struct __attribute__((packed)) {
    uint8_t type;                   // Type de paquet
    uint32_t packetId;              // Identifiant unique du paquet
    char sender[16];                // Numéro/ID de l'expéditeur
    char receiver[16];              // Numéro/ID du destinataire
    uint16_t dataLength;            // Longueur des données
    uint8_t data[LORA_MAX_PACKET_SIZE - 39];  // Données (max 216 octets)
} LoRaPacket;

/**
 * @brief Structure d'un téléphone connu (cache réseau)
 */
typedef struct {
    char phoneNumber[16];           // Numéro de téléphone
    char deviceName[16];            // Nom du dispositif
    uint32_t lastSeen;              // Timestamp dernière activité
    int16_t rssi;                   // Dernier RSSI mesuré
    int8_t snr;                     // Dernier SNR mesuré
    bool online;                    // En ligne
    uint32_t missedCalls;           // Appels manqués
} LoRaKnownPhone;

/**
 * @brief Statistiques du driver LoRa
 */
typedef struct {
    uint32_t packetsSent;           // Paquets envoyés
    uint32_t packetsReceived;       // Paquets reçus
    uint32_t packetsError;          // Paquets en erreur
    uint32_t packetsRetried;        // Retransmissions
    uint32_t audioPacketsSent;      // Paquets audio envoyés
    uint32_t audioPacketsReceived;  // Paquets audio reçus
    uint32_t smsSent;               // SMS envoyés
    uint32_t smsReceived;           // SMS reçus
    uint32_t callsMade;             // Appels émis
    uint32_t callsReceived;         // Appels reçus
    uint32_t bytesSent;             // Total octets envoyés
    uint32_t bytesReceived;         // Total octets reçus
    uint32_t uptime;                // Temps de fonctionnement (secondes)
    float dutyCycle;                // Cycle d'utilisation estimé (%)
} LoRaStatistics;

// ============================================================
// SECTION 4 : CALLBACKS
// ============================================================

/**
 * @brief Callback : paquet reçu
 * @param packet Paquet reçu
 */
typedef void (*LoRa_OnPacketReceived)(LoRaPacket* packet);

/**
 * @brief Callback : appel entrant
 * @param callerNumber Numéro de l'appelant
 * @param callerName Nom de l'appelant
 */
typedef void (*LoRa_OnIncomingCall)(const char* callerNumber, const char* callerName);

/**
 * @brief Callback : appel accepté par le correspondant
 */
typedef void (*LoRa_OnCallAccepted)(void);

/**
 * @brief Callback : appel refusé par le correspondant
 * @param reason Code de raison
 */
typedef void (*LoRa_OnCallRejected)(uint8_t reason);

/**
 * @brief Callback : appel terminé
 * @param duration Durée en secondes
 */
typedef void (*LoRa_OnCallEnded)(uint32_t duration);

/**
 * @brief Callback : données audio reçues
 * @param data Buffer audio
 * @param length Longueur en octets
 */
typedef void (*LoRa_OnAudioData)(uint8_t* data, uint16_t length);

/**
 * @brief Callback : SMS reçu
 * @param sender Numéro de l'expéditeur
 * @param message Contenu du message
 */
typedef void (*LoRa_OnSMSReceived)(const char* sender, const char* message);

/**
 * @brief Callback : téléphone découvert
 * @param phone Informations du téléphone
 */
typedef void (*LoRa_OnPhoneDiscovered)(LoRaKnownPhone* phone);

/**
 * @brief Callback : erreur
 * @param errorCode Code d'erreur
 * @param message Message descriptif
 */
typedef void (*LoRa_OnError)(int errorCode, const char* message);

// ============================================================
// SECTION 5 : FONCTIONS D'INITIALISATION
// ============================================================

/**
 * @brief Initialise le driver LoRa
 * 
 * Configure le module avec le profil par défaut (BALANCED).
 * Initialise les buffers et le cache réseau.
 * 
 * @return true si succès, false si échec
 */
bool lora_driver_init(void);

/**
 * @brief Désinitialise le driver LoRa
 * 
 * Libère les ressources et met le module en veille.
 */
void lora_driver_deinit(void);

/**
 * @brief Vérifie si le driver est initialisé
 * @return true si prêt
 */
bool lora_driver_is_ready(void);

// ============================================================
// SECTION 6 : FONCTIONS DE CONFIGURATION
// ============================================================

/**
 * @brief Change le profil de configuration
 * 
 * @param type Type de profil (PROFILE_AUDIO, PROFILE_SMS, etc.)
 * @return true si succès
 */
bool lora_driver_set_profile(SX1278_ProfileType type);

/**
 * @brief Récupère le profil actuel
 * @return Pointeur vers le profil actif
 */
const SX1278_Profile* lora_driver_get_profile(void);

/**
 * @brief Définit le numéro de téléphone local
 * @param phoneNumber Numéro de téléphone (max 15 caractères)
 */
void lora_driver_set_phone_number(const char* phoneNumber);

/**
 * @brief Récupère le numéro de téléphone local
 * @return Numéro de téléphone
 */
const char* lora_driver_get_phone_number(void);

/**
 * @brief Définit le nom du dispositif
 * @param name Nom (max 15 caractères)
 */
void lora_driver_set_device_name(const char* name);

/**
 * @brief Régle la puissance d'émission
 * @param power_dbm Puissance en dBm (2-20)
 */
void lora_driver_set_power(int8_t power_dbm);

/**
 * @brief Récupère le RSSI actuel
 * @return RSSI en dBm
 */
int16_t lora_driver_get_rssi(void);

/**
 * @brief Récupère le SNR actuel
 * @return SNR en dB
 */
int8_t lora_driver_get_snr(void);

// ============================================================
// SECTION 7 : FONCTIONS DE COMMUNICATION DE BASE
// ============================================================

/**
 * @brief Envoie un paquet générique
 * 
 * @param packet Paquet à envoyer
 * @param waitAck Attendre un accusé de réception
 * @return true si envoyé avec succès
 */
bool lora_driver_send_packet(LoRaPacket* packet, bool waitAck);

/**
 * @brief Vérifie si un paquet est disponible
 * @return true si un paquet attend
 */
bool lora_driver_is_packet_available(void);

/**
 * @brief Récupère le dernier paquet reçu
 * 
 * @param packet Structure à remplir
 * @return true si un paquet a été lu
 */
bool lora_driver_receive_packet(LoRaPacket* packet);

/**
 * @brief Envoie un accusé de réception
 * 
 * @param originalPacketId ID du paquet à acquitter
 * @param receiver Destinataire
 */
void lora_driver_send_ack(uint32_t originalPacketId, const char* receiver);

// ============================================================
// SECTION 8 : FONCTIONS DE TÉLÉPHONIE (APPELS)
// ============================================================

/**
 * @brief Initie un appel sortant
 * 
 * Configure le module en mode AUDIO et envoie une demande d'appel.
 * 
 * @param calleeNumber Numéro du destinataire
 * @return true si la demande est partie
 */
bool lora_driver_call(const char* calleeNumber);

/**
 * @brief Accepte un appel entrant
 * 
 * Passe en mode AUDIO et démarre le flux audio.
 * 
 * @return true si accepté
 */
bool lora_driver_accept_call(void);

/**
 * @brief Refuse un appel entrant
 * 
 * @param reason Code de raison (0=utilisateur, 1=occupé)
 * @return true si refus envoyé
 */
bool lora_driver_reject_call(uint8_t reason);

/**
 * @brief Termine un appel en cours
 * 
 * @return true si terminé
 */
bool lora_driver_end_call(void);

/**
 * @brief Vérifie si un appel est en cours
 * @return true si en communication
 */
bool lora_driver_is_in_call(void);

/**
 * @brief Récupère le numéro du correspondant actuel
 * @return Numéro ou NULL si pas d'appel
 */
const char* lora_driver_get_call_partner(void);

/**
 * @brief Récupère la durée de l'appel en cours
 * @return Durée en secondes
 */
uint32_t lora_driver_get_call_duration(void);

// ============================================================
// SECTION 9 : FONCTIONS AUDIO
// ============================================================

/**
 * @brief Envoie un buffer audio
 * 
 * @param audioData Données audio (8 kHz, 8 bits)
 * @param length Longueur en octets
 * @return true si envoyé
 */
bool lora_driver_send_audio(uint8_t* audioData, uint16_t length);

/**
 * @brief Vérifie si des données audio sont disponibles
 * @return true si audio en attente
 */
bool lora_driver_is_audio_available(void);

/**
 * @brief Récupère les données audio reçues
 * 
 * @param audioData Buffer de réception
 * @param length Pointeur vers la longueur
 * @return true si des données ont été lues
 */
bool lora_driver_receive_audio(uint8_t* audioData, uint16_t* length);

// ============================================================
// SECTION 10 : FONCTIONS MESSAGERIE (SMS)
// ============================================================

/**
 * @brief Envoie un SMS
 * 
 * Configure le module en mode SMS et envoie le message.
 * 
 * @param receiverNumber Numéro du destinataire
 * @param message Contenu du message (max 160 caractères)
 * @return true si envoyé
 */
bool lora_driver_send_sms(const char* receiverNumber, const char* message);

/**
 * @brief Vérifie si un SMS est disponible
 * @return true si un SMS attend
 */
bool lora_driver_is_sms_available(void);

/**
 * @brief Récupère le dernier SMS reçu
 * 
 * @param sender Buffer pour le numéro de l'expéditeur (min 16 octets)
 * @param message Buffer pour le message (min 161 octets)
 * @return true si un SMS a été lu
 */
bool lora_driver_receive_sms(char* sender, char* message);

// ============================================================
// SECTION 11 : FONCTIONS RÉSEAU
// ============================================================

/**
 * @brief Lance une découverte du réseau
 * 
 * Envoie un broadcast et attend les réponses.
 * Les téléphones découverts sont signalés via le callback.
 * 
 * @param timeoutMs Durée de la découverte (ms)
 * @return Nombre de téléphones découverts
 */
uint8_t lora_driver_discover_network(uint32_t timeoutMs);

/**
 * @brief Envoie un ping à un téléphone
 * 
 * @param phoneNumber Numéro à pinger
 * @return true si le pong est reçu
 */
bool lora_driver_ping(const char* phoneNumber);

/**
 * @brief Récupère la liste des téléphones connus
 * 
 * @param phones Tableau à remplir
 * @param maxCount Nombre maximum à récupérer
 * @return Nombre de téléphones copiés
 */
uint8_t lora_driver_get_known_phones(LoRaKnownPhone* phones, uint8_t maxCount);

/**
 * @brief Récupère un téléphone connu par son numéro
 * 
 * @param phoneNumber Numéro recherché
 * @return Pointeur vers le téléphone, NULL si non trouvé
 */
LoRaKnownPhone* lora_driver_find_phone(const char* phoneNumber);

// ============================================================
// SECTION 12 : FONCTIONS DE MAINTENANCE
// ============================================================

/**
 * @brief Traitement périodique (à appeler dans la boucle principale)
 * 
 * Vérifie les paquets entrants, les timeouts,
 * et met à jour les statistiques.
 */
void lora_driver_process(void);

/**
 * @brief Envoie un keep-alive
 * 
 * À appeler périodiquement pour maintenir la présence réseau.
 */
void lora_driver_keepalive(void);

/**
 * @brief Réinitialise les statistiques
 */
void lora_driver_reset_statistics(void);

/**
 * @brief Récupère les statistiques
 * 
 * @param stats Structure à remplir
 */
void lora_driver_get_statistics(LoRaStatistics* stats);

/**
 * @brief Affiche les statistiques (debug)
 */
void lora_driver_print_statistics(void);

// ============================================================
// SECTION 13 : FONCTIONS D'ÉNERGIE
// ============================================================

/**
 * @brief Met le module en veille
 */
void lora_driver_sleep(void);

/**
 * @brief Réveille le module
 */
void lora_driver_wakeup(void);

/**
 * @brief Vérifie si le module est en veille
 */
bool lora_driver_is_sleeping(void);

// ============================================================
// SECTION 14 : ENREGISTREMENT DES CALLBACKS
// ============================================================

/**
 * @brief Enregistre les callbacks pour les événements
 */
void lora_driver_set_callbacks(
    LoRa_OnIncomingCall onIncomingCall,
    LoRa_OnCallAccepted onCallAccepted,
    LoRa_OnCallRejected onCallRejected,
    LoRa_OnCallEnded onCallEnded,
    LoRa_OnAudioData onAudioData,
    LoRa_OnSMSReceived onSMSReceived,
    LoRa_OnPhoneDiscovered onPhoneDiscovered,
    LoRa_OnError onError
);

// ============================================================
// SECTION 15 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // LORA_DRIVER_H