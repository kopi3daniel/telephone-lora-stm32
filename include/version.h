/**
 * @file version.h
 * @brief Gestion des versions du firmware
 * 
 * Ce fichier centralise toutes les informations de version
 * du firmware. Il est utilisé par :
 * - Le message de démarrage
 * - La console série (commande "version")
 * - Le menu "À propos" dans l'interface
 * - Les éventuels outils de mise à jour OTA
 * 
 * @author Votre Nom
 * @date 2024
 */

#ifndef VERSION_H
#define VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// SECTION 1 : VERSION DU FIRMWARE
// ============================================================

/**
 * @def VERSION_MAJOR
 * @brief Version majeure du firmware
 * 
 * Incrémentée pour :
 * - Changements majeurs d'architecture
 * - Refonte complète de l'interface
 * - Incompatibilité avec les versions précédentes
 * 
 * Exemple : 1 → 2 (nouveau protocole réseau)
 */
#define VERSION_MAJOR               0

/**
 * @def VERSION_MINOR
 * @brief Version mineure du firmware
 * 
 * Incrémentée pour :
 * - Nouvelles fonctionnalités
 * - Améliorations significatives
 * - Ajout de nouveaux écrans ou protocoles
 * 
 * Exemple : 0 → 1 (ajout du support SMS)
 */
#define VERSION_MINOR               0

/**
 * @def VERSION_PATCH
 * @brief Numéro de patch/correction
 * 
 * Incrémenté pour :
 * - Corrections de bugs
 * - Optimisations mineures
 * - Ajustements de configuration
 * 
 * Exemple : 0 → 1 (correction bug audio)
 */
#define VERSION_PATCH               0

// ============================================================
// SECTION 2 : CONVERSION EN CHAÎNE DE CARACTÈRES
// ============================================================

/**
 * @def STRINGIFY_HELPER
 * @brief Macro interne pour la conversion en chaîne
 * 
 * Nécessaire car le préprocesseur a besoin de deux niveaux
 * d'expansion pour convertir correctement une macro en chaîne.
 */
#define STRINGIFY_HELPER(x)         #x

/**
 * @def STRINGIFY
 * @brief Convertit une macro en chaîne de caractères
 * 
 * @param x La macro à convertir
 * @return La valeur de la macro sous forme de chaîne
 * 
 * Exemple :
 *   STRINGIFY(VERSION_MAJOR) → "0"
 *   STRINGIFY(123)           → "123"
 */
#define STRINGIFY(x)                STRINGIFY_HELPER(x)

// ============================================================
// SECTION 3 : CHAÎNES DE VERSION
// ============================================================

/**
 * @def VERSION_MAJOR_STR
 * @brief Version majeure en chaîne
 */
#define VERSION_MAJOR_STR           STRINGIFY(VERSION_MAJOR)

/**
 * @def VERSION_MINOR_STR
 * @brief Version mineure en chaîne
 */
#define VERSION_MINOR_STR           STRINGIFY(VERSION_MINOR)

/**
 * @def VERSION_PATCH_STR
 * @brief Version patch en chaîne
 */
#define VERSION_PATCH_STR           STRINGIFY(VERSION_PATCH)

/**
 * @def VERSION_STRING
 * @brief Version complète au format "MAJEUR.MINEUR.PATCH"
 * 
 * Exemple : "0.0.0", "1.2.3", "2.0.1"
 * 
 * Utilisation :
 *   printf("Version: %s\n", VERSION_STRING);
 */
#define VERSION_STRING              VERSION_MAJOR_STR "." \
                                    VERSION_MINOR_STR "." \
                                    VERSION_PATCH_STR

/**
 * @def VERSION_SHORT_STRING
 * @brief Version courte au format "vMAJEUR.MINEUR"
 * 
 * Exemple : "v0.0", "v1.2"
 */
#define VERSION_SHORT_STRING        "v" VERSION_MAJOR_STR "." VERSION_MINOR_STR

/**
 * @def VERSION_FULL_STRING
 * @brief Version complète avec préfixe
 * 
 * Exemple : "Version 0.0.0"
 */
#define VERSION_FULL_STRING         "Version " VERSION_STRING

// ============================================================
// SECTION 4 : VERSION AU FORMAT NUMÉRIQUE
// ============================================================

/**
 * @def VERSION_NUMERIC
 * @brief Version au format numérique pour comparaison
 * 
 * Format : 0xAABBCC (AA=majeure, BB=mineure, CC=patch)
 * 
 * Utile pour :
 * - Comparer deux versions (if VERSION_NUMERIC > 0x010000)
 * - Stocker la version dans un registre
 * - Transmettre la version dans un paquet réseau
 * 
 * Exemple : version 1.2.3 → 0x010203
 */
#define VERSION_NUMERIC             ((VERSION_MAJOR << 16) | \
                                     (VERSION_MINOR << 8)  | \
                                     (VERSION_PATCH))

/**
 * @def VERSION_BINARY
 * @brief Version en BCD (Binary Coded Decimal) pour affichage
 * 
 * Format : 0xMMPP (MM=majeure, PP=patch)
 * 
 * Exemple : version 1.23 → 0x0123
 */
#define VERSION_BCD                 ((VERSION_MAJOR * 100) + (VERSION_MINOR * 10) + VERSION_PATCH)

// ============================================================
// SECTION 5 : INFORMATIONS DE COMPILATION
// ============================================================

/**
 * @def BUILD_DATE
 * @brief Date de compilation au format "Mmm DD YYYY"
 * 
 * Généré automatiquement par le compilateur.
 * 
 * Exemple : "Jan 15 2024"
 */
#define BUILD_DATE                  __DATE__

/**
 * @def BUILD_TIME
 * @brief Heure de compilation au format "HH:MM:SS"
 * 
 * Généré automatiquement par le compilateur.
 * 
 * Exemple : "14:30:00"
 */
#define BUILD_TIME                  __TIME__

/**
 * @def BUILD_TIMESTAMP
 * @brief Horodatage complet de la compilation
 * 
 * Exemple : "Jan 15 2024 14:30:00"
 */
#define BUILD_TIMESTAMP             BUILD_DATE " " BUILD_TIME

/**
 * @def BUILD_YEAR
 * @brief Année de compilation (4 chiffres)
 * 
 * Extrait de __DATE__
 * Exemple : "2024"
 */
#define BUILD_YEAR                  __DATE__[7] __DATE__[8] __DATE__[9] __DATE__[10]
#define BUILD_YEAR_STR              STRINGIFY(__DATE__[7]) STRINGIFY(__DATE__[8]) STRINGIFY(__DATE__[9]) STRINGIFY(__DATE__[10])

// ============================================================
// SECTION 6 : INFORMATIONS DU COMPILATEUR
// ============================================================

/**
 * @def COMPILER_NAME
 * @brief Nom et version du compilateur
 * 
 * Exemple : "GCC 10.3.1"
 */
#define COMPILER_NAME               "GCC " __VERSION__

/**
 * @def COMPILER_VERSION
 * @brief Version complète du compilateur
 * 
 * Exemple : "10.3.1 20210621 (release)"
 */
#define COMPILER_VERSION            __VERSION__

/**
 * @def TOOLCHAIN_NAME
 * @brief Nom de la toolchain
 */
#define TOOLCHAIN_NAME              "GNU Arm Embedded Toolchain"

/**
 * @def TOOLCHAIN_VERSION
 * @brief Version de la toolchain
 * 
 * Exemple : "10.3-2021.10"
 */
#define TOOLCHAIN_VERSION           "10.3-2021.10"

// ============================================================
// SECTION 7 : INFORMATIONS DU PROJET
// ============================================================

/**
 * @def PROJECT_NAME
 * @brief Nom complet du projet
 */
#define PROJECT_NAME                "Téléphone LoRa STM32F429"

/**
 * @def PROJECT_SHORT_NAME
 * @brief Nom court du projet
 */
#define PROJECT_SHORT_NAME          "LoRaPhone"

/**
 * @def PROJECT_DESCRIPTION
 * @brief Description du projet
 */
#define PROJECT_DESCRIPTION         "Téléphone portable utilisant la technologie LoRa"

/**
 * @def PROJECT_URL
 * @brief URL du dépôt Git
 */
#define PROJECT_URL                 "https://github.com/votre-username/telephone-lora-stm32"

/**
 * @def PROJECT_AUTHOR
 * @brief Auteur principal
 */
#define PROJECT_AUTHOR              "Votre Nom"

/**
 * @def PROJECT_EMAIL
 * @brief Email de contact
 */
#define PROJECT_EMAIL               "votre.email@example.com"

/**
 * @def PROJECT_LICENSE
 * @brief Licence du projet
 */
#define PROJECT_LICENSE             "MIT"

// ============================================================
// SECTION 8 : INFORMATIONS MATÉRIELLES
// ============================================================

/**
 * @def HARDWARE_PLATFORM
 * @brief Plateforme matérielle
 * 
 * Détectée automatiquement selon les définitions du compilateur
 */
#if defined(STM32F429I_DISCO)
    #define HARDWARE_PLATFORM       "STM32F429I-DISCOVERY"
#elif defined(STM32F429I_NUCLEO)
    #define HARDWARE_PLATFORM       "NUCLEO-F429ZI"
#else
    #define HARDWARE_PLATFORM       "Generic STM32F429"
#endif

/**
 * @def HARDWARE_MCU
 * @brief Microcontrôleur utilisé
 */
#define HARDWARE_MCU                "STM32F429ZIT6"

/**
 * @def HARDWARE_CORE
 * @brief Cœur du processeur
 */
#define HARDWARE_CORE               "ARM Cortex-M4"

/**
 * @def HARDWARE_FREQ_MHZ
 * @brief Fréquence CPU en MHz
 */
#define HARDWARE_FREQ_MHZ           180

// ============================================================
// SECTION 9 : FONCTIONS UTILITAIRES DE VERSION
// ============================================================

/**
 * @brief Obtient la version sous forme de chaîne
 * @return Pointeur vers la chaîne de version statique
 * 
 * Exemple : "0.0.0"
 */
static inline const char* version_get_string(void) {
    return VERSION_STRING;
}

/**
 * @brief Obtient la version sous forme numérique
 * @return Version au format 0xAABBCC
 * 
 * Exemple : 0x000000 pour 0.0.0
 */
static inline uint32_t version_get_numeric(void) {
    return VERSION_NUMERIC;
}

/**
 * @brief Compare deux versions numériques
 * @param v1 Première version au format 0xAABBCC
 * @param v2 Deuxième version au format 0xAABBCC
 * @return -1 si v1 < v2, 0 si égal, 1 si v1 > v2
 */
static inline int version_compare(uint32_t v1, uint32_t v2) {
    if (v1 < v2) return -1;
    if (v1 > v2) return 1;
    return 0;
}

/**
 * @brief Vérifie si une mise à jour est disponible
 * @param newVersion Version proposée au format 0xAABBCC
 * @return true si la nouvelle version est plus récente
 */
static inline bool version_is_newer(uint32_t newVersion) {
    return version_compare(newVersion, VERSION_NUMERIC) > 0;
}

/**
 * @brief Affiche les informations de version complètes
 * Utile pour la console série ou le menu "À propos"
 */
static inline void version_print_full(void) {
    printf("╔══════════════════════════════════════════╗\n");
    printf("║          INFORMATIONS VERSION            ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Projet:     %-29s ║\n", PROJECT_NAME);
    printf("║ Version:    %-29s ║\n", VERSION_STRING);
    printf("║ Build:      %-29s ║\n", BUILD_TIMESTAMP);
    printf("║ Compilateur:%-29s ║\n", COMPILER_NAME);
    printf("║ Plateforme: %-29s ║\n", HARDWARE_PLATFORM);
    printf("║ MCU:        %-29s ║\n", HARDWARE_MCU);
    printf("║ Licence:    %-29s ║\n", PROJECT_LICENSE);
    printf("║ Auteur:     %-29s ║\n", PROJECT_AUTHOR);
    printf("╚══════════════════════════════════════════╝\n");
}

/**
 * @brief Affiche la version en une ligne (format court)
 */
static inline void version_print_short(void) {
    printf("%s v%s (%s)\n", PROJECT_SHORT_NAME, VERSION_STRING, BUILD_DATE);
}

/**
 * @brief Retourne la version dans un buffer (pour interface graphique)
 * @param buffer Buffer de destination (doit faire au moins 32 octets)
 * @param size Taille du buffer
 */
static inline void version_get_string_buffer(char* buffer, size_t size) {
    snprintf(buffer, size, "v%s", VERSION_STRING);
}

// ============================================================
// SECTION 10 : COMPATIBILITÉ C/C++
// ============================================================

#ifdef __cplusplus
}
#endif

#endif // VERSION_H