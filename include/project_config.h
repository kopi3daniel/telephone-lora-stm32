/**
 * @file project_config.h
 * @brief Configuration globale du Téléphone LoRa STM32F429
 * 
 * Ce fichier contient toutes les définitions de configuration
 * qui contrôlent les fonctionnalités du projet.
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// SECTION 1 : VERSION DU FIRMWARE
// ============================================================
// Ces définitions sont utilisées pour identifier la version du
// firmware. Elles apparaissent dans le message de démarrage et
// peuvent être lues par un outil de mise à jour.

#define FIRMWARE_VERSION_MAJOR      1       // Version majeure (changée pour les grandes releases)
#define FIRMWARE_VERSION_MINOR      0       // Version mineure (nouvelles fonctionnalités)
#define FIRMWARE_VERSION_PATCH      0       // Patch (corrections de bugs)

// Conversion en chaîne de caractères pour affichage
#define STRINGIFY_HELPER(x)         #x
#define STRINGIFY(x)                STRINGIFY_HELPER(x)

#define FIRMWARE_VERSION_STRING     STRINGIFY(FIRMWARE_VERSION_MAJOR) "." \
                                    STRINGIFY(FIRMWARE_VERSION_MINOR) "." \
                                    STRINGIFY(FIRMWARE_VERSION_PATCH)

// ============================================================
// SECTION 2 : INFORMATIONS DU DISPOSITIF
// ============================================================

#define DEVICE_NAME                 "LoRaPhone"         // Nom du dispositif
#define DEVICE_MANUFACTURER         "DIY"               // Fabricant
#define DEVICE_MODEL                "LP-STM32F429"      // Modèle
#define DEVICE_SERIAL_PREFIX        "LP"                // Préfixe numéro de série

// ============================================================
// SECTION 3 : ACTIVATION/DÉSACTIVATION DES FONCTIONNALITÉS
// ============================================================
// Mettre à 1 pour activer, 0 pour désactiver.
// Désactiver des fonctionnalités non utilisées réduit la taille
// du firmware et la consommation de ressources.

// --- Communication ---
#define ENABLE_LORA                 1       // Module LoRa RA-02 (SX1278)
#define ENABLE_LORA_ENCRYPTION      0       // Chiffrement LoRa (à implémenter)
#define ENABLE_MESH_NETWORK         0       // Réseau maillé (futur)

// --- Interface Utilisateur ---
#define ENABLE_DISPLAY              1       // Écran TFT ILI9488
#define ENABLE_TOUCH                1       // Écran tactile XPT2046
#define ENABLE_KEYPAD               1       // Clavier physique 24 touches
#define ENABLE_KEYPAD_BACKLIGHT     1       // Rétroéclairage clavier
#define ENABLE_LAMP                 1       // Lampe torche LED

// --- Audio ---
#define ENABLE_AUDIO                1       // Audio (micro + haut-parleur)
#define ENABLE_AUDIO_COMPRESSION    1       // Compression ADPCM
#define ENABLE_AUDIO_NOISE_FILTER   0       // Filtre de bruit (futur)
#define ENABLE_RINGTONE             1       // Sonneries

// --- Stockage ---
#define ENABLE_SD_CARD              0       // Carte SD (à implémenter)
#define ENABLE_FLASH_STORAGE        1       // Stockage en Flash interne
#define ENABLE_EEPROM_EMULATION     1       // Émulation EEPROM

// --- Réseau ---
#define ENABLE_CALL_PROTOCOL        1       // Protocole d'appel
#define ENABLE_SMS_PROTOCOL         1       // Protocole SMS
#define ENABLE_DISCOVERY_PROTOCOL   1       // Découverte de réseau
#define ENABLE_REGISTRATION         0       // Enregistrement réseau (futur)

// --- Débogage ---
#define ENABLE_DEBUG                1       // Messages de débogage
#define ENABLE_DEBUG_LORA           0       // Débogage détaillé LoRa
#define ENABLE_DEBUG_AUDIO          0       // Débogage détaillé Audio
#define ENABLE_DEBUG_KEYPAD         0       // Débogage détaillé Clavier
#define ENABLE_SERIAL_CONSOLE       1       // Console série (commandes)

// --- Optimisation ---
#define ENABLE_SLEEP_MODE           1       // Mode veille
#define ENABLE_DEEP_SLEEP           0       // Mode veille profonde (futur)
#define ENABLE_WATCHDOG             1       // Chien de garde

// ============================================================
// SECTION 4 : CONFIGURATION LORA
// ============================================================

// --- Fréquences par région (en Hz) ---
#define LORA_FREQ_EUROPE            868000000UL     // 868 MHz (Europe, Afrique)
#define LORA_FREQ_NORTH_AMERICA     915000000UL     // 915 MHz (Amérique du Nord)
#define LORA_FREQ_ASIA              433000000UL     // 433 MHz (Asie)
#define LORA_FREQ_AUSTRALIA         915000000UL     // 915 MHz (Australie)

// Fréquence active (modifier selon votre région)
#define LORA_FREQUENCY              LORA_FREQ_EUROPE

// --- Paramètres de modulation ---
#define LORA_SPREADING_FACTOR       7               // SF7 (128 chips/symbole)
                                                    // SF6 = plus rapide, moins longue portée
                                                    // SF12 = plus lent, plus longue portée

#define LORA_BANDWIDTH              125000          // 125 kHz
                                                    // Options: 7800, 10400, 15600, 20800,
                                                    //          31250, 41700, 62500, 125000,
                                                    //          250000, 500000

#define LORA_CODING_RATE            5               // 4/5 (4 bits data pour 5 bits transmis)
                                                    // Options: 5=4/5, 6=4/6, 7=4/7, 8=4/8

#define LORA_TX_POWER               17              // Puissance d'émission (2-20 dBm)
                                                    // 17 dBm = 50 mW (max autorisé en Europe)

#define LORA_PREAMBLE_LENGTH        8               // Longueur du préambule (symboles)
#define LORA_SYNC_WORD              0x34            // Mot de synchronisation (0x34 = public)

// --- Timings ---
#define LORA_TX_TIMEOUT_MS          5000            // Timeout transmission (ms)
#define LORA_RX_TIMEOUT_MS          10000           // Timeout réception (ms)
#define LORA_RETRY_COUNT            3               // Nombre de tentatives

// ============================================================
// SECTION 5 : CONFIGURATION ÉCRAN
// ============================================================

// --- Dimensions physiques ---
#define TFT_WIDTH                   320             // Largeur en pixels
#define TFT_HEIGHT                  480             // Hauteur en pixels

// --- Format des couleurs ---
#define TFT_COLOR_FORMAT            RGB565          // 16 bits par pixel (5-6-5)
#define TFT_BITS_PER_PIXEL          16

// --- Timings LTDC (pour ILI9488) ---
#define LTDC_PIXEL_CLOCK_HZ         10000000        // 10 MHz
#define LTDC_HORIZONTAL_SYNC        10              // HSYNC width
#define LTDC_HORIZONTAL_BACK_PORCH  10              // H Back Porch
#define LTDC_HORIZONTAL_FRONT_PORCH 20              // H Front Porch
#define LTDC_VERTICAL_SYNC          2               // VSYNC width
#define LTDC_VERTICAL_BACK_PORCH    2               // V Back Porch
#define LTDC_VERTICAL_FRONT_PORCH   1               // V Front Porch

// --- Rétroéclairage ---
#define TFT_BRIGHTNESS_DEFAULT      100             // Luminosité par défaut (0-100)
#define TFT_BRIGHTNESS_MIN          10              // Luminosité minimale
#define TFT_TIMEOUT_SECONDS         30              // Extinction après inactivité (secondes)

// ============================================================
// SECTION 6 : CONFIGURATION AUDIO
// ============================================================

// --- Échantillonnage ---
#define AUDIO_SAMPLE_RATE           8000            // 8 kHz (qualité téléphonique)
                                                    // Options: 8000, 11025, 16000, 22050, 44100

#define AUDIO_BITS_PER_SAMPLE       12              // Résolution ADC/DAC (12 bits)
#define AUDIO_BUFFER_SIZE           256             // Taille buffer en échantillons

// --- Volume ---
#define AUDIO_VOLUME_DEFAULT        80              // Volume par défaut (0-100)
#define AUDIO_VOLUME_MIN            0               // Volume minimum (muet)
#define AUDIO_VOLUME_MAX            100             // Volume maximum
#define AUDIO_VOLUME_STEP           5               // Incrément par appui

// --- Microphone ---
#define MIC_GAIN_DEFAULT            100             // Gain micro par défaut
#define MIC_BIAS_VOLTAGE            3.3f            // Tension de bias (V)

// --- Compression ADPCM ---
#define ADPCM_FRAME_SIZE            128             // Taille d'une trame ADPCM
#define ADPCM_COMPRESSION_RATIO     2               // Ratio compression (2:1)

// ============================================================
// SECTION 7 : CONFIGURATION CLAVIER
// ============================================================

// --- Matrice ---
#define KEYPAD_ROWS                 6               // Nombre de lignes
#define KEYPAD_COLS                 4               // Nombre de colonnes
#define KEYPAD_TOTAL_KEYS           (KEYPAD_ROWS * KEYPAD_COLS)  // 24 touches

// --- Timings ---
#define KEYPAD_SCAN_INTERVAL_MS     10              // Intervalle de scan (ms)
#define KEYPAD_DEBOUNCE_MS          20              // Anti-rebond (ms)
#define KEYPAD_LONG_PRESS_MS        500             // Appui long (ms)
#define KEYPAD_REPEAT_MS            100             // Répétition appui long (ms)
#define KEYPAD_MULTITAP_TIMEOUT_MS  1000            // Timeout Multitap (ms)

// --- Rétroéclairage ---
#define KEYPAD_BACKLIGHT_DEFAULT    128             // Luminosité par défaut (0-255)
#define KEYPAD_BACKLIGHT_TIMEOUT_S  10              // Extinction après (secondes)

// ============================================================
// SECTION 8 : CONFIGURATION LAMPE TORCHE
// ============================================================

#define LAMP_BRIGHTNESS_DEFAULT     255             // Luminosité par défaut (0-255)
#define LAMP_BRIGHTNESS_MAX         255             // Luminosité maximale
#define LAMP_STROBE_PERIOD_MS       80              // Période stroboscope (ms)
#define LAMP_SOS_SHORT_MS           100             // Signal SOS court (ms)
#define LAMP_SOS_LONG_MS            300             // Signal SOS long (ms)

// ============================================================
// SECTION 9 : CONFIGURATION MÉMOIRE
// ============================================================

// --- SDRAM (8 Mo sur Discovery) ---
#define SDRAM_SIZE                  0x800000        // 8 Mo
#define SDRAM_BASE_ADDRESS          0xC0000000      // Adresse de base

// --- Framebuffer écran ---
#define FRAMEBUFFER_SIZE            (TFT_WIDTH * TFT_HEIGHT * 2)  // 307 200 octets
#define FRAMEBUFFER_ADDRESS         SDRAM_BASE_ADDRESS
#define FRAMEBUFFER2_ADDRESS        (SDRAM_BASE_ADDRESS + FRAMEBUFFER_SIZE)

// --- Flash interne (2 Mo) ---
#define FLASH_SIZE                  0x200000        // 2 Mo
#define FLASH_BASE_ADDRESS          0x08000000

// --- RAM interne (256 Ko) ---
#define RAM_SIZE                    0x40000         // 256 Ko
#define RAM_BASE_ADDRESS            0x20000000

// ============================================================
// SECTION 10 : CONFIGURATION RÉSEAU/TÉLÉPHONIE
// ============================================================

// --- Numéro de téléphone ---
#define PHONE_NUMBER_MAX_LENGTH     16              // Longueur max numéro
#define PHONE_NUMBER_PREFIX         "06"            // Préfixe par défaut

// --- Appels ---
#define CALL_MAX_DURATION_MINUTES   120             // Durée max d'un appel
#define CALL_RING_TIMEOUT_MS        30000           // Timeout sonnerie (30s)
#define CALL_RETRY_COUNT            3               // Tentatives d'appel
#define CALL_RETRY_DELAY_MS         5000            // Délai entre tentatives

// --- Messagerie ---
#define SMS_MAX_LENGTH              160             // Longueur max SMS
#define SMS_MAX_STORED              100             // Messages stockés max
#define SMS_RETRY_COUNT             3               // Tentatives envoi SMS

// --- Contacts ---
#define CONTACTS_MAX                200             // Nombre max de contacts
#define CONTACT_NAME_MAX_LENGTH     32              // Longueur max nom
#define FAVORITES_MAX               20              // Favoris max

// --- Journal d'appels ---
#define CALL_LOG_MAX                100             // Entrées max journal

// --- Réseau ---
#define NETWORK_DISCOVERY_INTERVAL_S 60             // Intervalle découverte (s)
#define NETWORK_KEEPALIVE_INTERVAL_S 30             // Intervalle keepalive (s)
#define NETWORK_MAX_KNOWN_PHONES    50              // Téléphones connus max

// ============================================================
// SECTION 11 : CONFIGURATION ÉNERGIE
// ============================================================

// --- Batterie ---
#define BATTERY_ADC_CHANNEL         ADC_CHANNEL_8   // Canal ADC batterie
#define BATTERY_VOLTAGE_MAX         4.2f            // Tension max (V)
#define BATTERY_VOLTAGE_MIN         3.3f            // Tension min (V)
#define BATTERY_CHECK_INTERVAL_S    60              // Vérification toutes les 60s

// --- Modes veille ---
#define SLEEP_ENTER_TIMEOUT_S       300             // Entrée veille après (5 min)
#define SLEEP_WAKEUP_PIN            GPIO_PIN_0      // Pin réveil (bouton)
#define DEEP_SLEEP_ENTER_TIMEOUT_S  1800            // Veille profonde après (30 min)

// ============================================================
// SECTION 12 : CONFIGURATION DÉBOGAGE
// ============================================================

// --- UART Debug ---
#define DEBUG_UART_BAUD             115200          // Vitesse UART debug
#define DEBUG_UART_INSTANCE         USART1          // Instance UART
#define DEBUG_PRINTF_BUFFER_SIZE    256             // Buffer printf

// --- LED Statut ---
#define STATUS_LED_PORT             GPIOG           // Port LED statut
#define STATUS_LED_PIN              GPIO_PIN_13     // Pin LED statut

// --- Niveaux de log ---
typedef enum {
    LOG_LEVEL_NONE      = 0,    // Aucun log
    LOG_LEVEL_ERROR     = 1,    // Erreurs seulement
    LOG_LEVEL_WARNING   = 2,    // Erreurs + avertissements
    LOG_LEVEL_INFO      = 3,    // Informations générales
    LOG_LEVEL_DEBUG     = 4,    // Débogage détaillé
    LOG_LEVEL_TRACE     = 5     // Traces très détaillées
} LogLevel;

#define CURRENT_LOG_LEVEL          LOG_LEVEL_INFO   // Niveau de log actuel

// ============================================================
// SECTION 13 : MACROS DE DÉBOGAGE CONDITIONNEL
// ============================================================

#if ENABLE_DEBUG
    #define DEBUG_PRINTF(fmt, ...)      printf("[DEBUG] " fmt, ##__VA_ARGS__)
    #define DEBUG_PRINT(fmt, ...)       printf(fmt, ##__VA_ARGS__)
    
    #if ENABLE_DEBUG_LORA
        #define LORA_DEBUG(fmt, ...)    printf("[LORA] " fmt, ##__VA_ARGS__)
    #else
        #define LORA_DEBUG(fmt, ...)
    #endif
    
    #if ENABLE_DEBUG_AUDIO
        #define AUDIO_DEBUG(fmt, ...)   printf("[AUDIO] " fmt, ##__VA_ARGS__)
    #else
        #define AUDIO_DEBUG(fmt, ...)
    #endif
    
    #if ENABLE_DEBUG_KEYPAD
        #define KEYPAD_DEBUG(fmt, ...)  printf("[KEYPAD] " fmt, ##__VA_ARGS__)
    #else
        #define KEYPAD_DEBUG(fmt, ...)
    #endif
#else
    #define DEBUG_PRINTF(fmt, ...)
    #define DEBUG_PRINT(fmt, ...)
    #define LORA_DEBUG(fmt, ...)
    #define AUDIO_DEBUG(fmt, ...)
    #define KEYPAD_DEBUG(fmt, ...)
#endif

// ============================================================
// SECTION 14 : MACROS UTILITAIRES
// ============================================================

// --- Calculs ---
#define ARRAY_SIZE(arr)                 (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b)                       ((a) < (b) ? (a) : (b))
#define MAX(a, b)                       ((a) > (b) ? (a) : (b))
#define CLAMP(x, min, max)              MIN(MAX((x), (min)), (max))
#define ABS(x)                          ((x) < 0 ? -(x) : (x))
#define MAP(val, in_min, in_max, out_min, out_max) \
    (((val) - (in_min)) * ((out_max) - (out_min)) / ((in_max) - (in_min)) + (out_min))

// --- Opérations sur bits ---
#define BIT_SET(reg, bit)               ((reg) |= (bit))
#define BIT_CLEAR(reg, bit)             ((reg) &= ~(bit))
#define BIT_TOGGLE(reg, bit)            ((reg) ^= (bit))
#define BIT_CHECK(reg, bit)             (((reg) & (bit)) != 0)
#define BIT_WRITE(reg, bit, val)        ((val) ? BIT_SET(reg, bit) : BIT_CLEAR(reg, bit))

// --- Construction de masques ---
#define BIT_MASK(bit)                   (1UL << (bit))
#define BIT_MASK_RANGE(high, low)       (((1UL << ((high) - (low) + 1)) - 1) << (low))

// --- Alignement mémoire ---
#define ALIGN_UP(val, align)            (((val) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(val, align)          ((val) & ~((align) - 1))
#define IS_ALIGNED(val, align)          (((val) & ((align) - 1)) == 0)

// --- Conversion ---
#define DEG_TO_RAD(deg)                 ((deg) * 3.14159265359f / 180.0f)
#define RAD_TO_DEG(rad)                 ((rad) * 180.0f / 3.14159265359f)

// --- Délais ---
#define DELAY_MS(ms)                    HAL_Delay(ms)
#define DELAY_US(us)                    for(volatile uint32_t _d=0; _d<((us)*180/8); _d++)

// --- Variable inutilisée ---
#define UNUSED(x)                       ((void)(x))

// ============================================================
// SECTION 15 : VÉRIFICATIONS DE CONFIGURATION
// ============================================================
// Vérifications à la compilation pour détecter les incohérences

#if ENABLE_MESH_NETWORK && !ENABLE_LORA
    #error "Le réseau maillé nécessite le module LoRa (ENABLE_LORA=1)"
#endif

#if ENABLE_SMS_PROTOCOL && !ENABLE_LORA
    #error "Le protocole SMS nécessite le module LoRa (ENABLE_LORA=1)"
#endif

#if FRAMEBUFFER_SIZE > (SDRAM_SIZE / 2)
    #warning "Le framebuffer utilise plus de 50% de la SDRAM"
#endif

#if AUDIO_SAMPLE_RATE > 16000
    #warning "Les fréquences d'échantillonnage > 16 kHz peuvent surcharger le CPU"
#endif

#if CALL_MAX_DURATION_MINUTES > 1440
    #error "La durée max d'appel ne peut pas dépasser 1440 minutes (24h)"
#endif

// ============================================================
// SECTION 16 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // PROJECT_CONFIG_H