/**
 * @file    string_utils.cpp
 * @brief   Implémentation des utilitaires de manipulation de chaînes
 * @author  Votre Nom
 * @date    2026
 * 
 * Implémente des fonctions de manipulation de chaînes sécurisées
 * et optimisées pour l'embarqué.
 * 
 * PRINCIPES :
 *   1. Toutes les fonctions vérifient les bornes (buffer overflow)
 *   2. Les buffers sont toujours terminés par '\0'
 *   3. Pas d'allocation dynamique
 *   4. Pas de fonctions récursives (pile limitée)
 *   5. Optimisé pour la vitesse (pas de scanf/printf)
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "string_utils.h"
#include "math_utils.h"

/* Standard */
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Tag pour les logs */
#define TAG                                 "StringUtils"

/** Buffer temporaire pour conversions */
#define TEMP_BUFFER_SIZE                    32

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

static void reverse_string(char* str, size_t len);
static int digit_count(int32_t value);
static int digit_count_u(uint32_t value);
static bool is_digit_char(char c);
static bool is_alpha_char(char c);
static bool is_alphanum_char(char c);
static bool is_hex_char(char c);
static char to_lower_char(char c);
static char to_upper_char(char c);
static int hex_char_to_value(char c);

/* ======================================================================== */
/*              FORMATAGE                                                   */
/* ======================================================================== */

/**
 * @brief Convertit un entier signé en chaîne
 */
int StringUtils_FormatInt(char* dst, size_t dst_size, int32_t value)
{
    if (!dst || dst_size < 2) return 0;

    char temp[TEMP_BUFFER_SIZE];
    int pos = 0;
    bool negative = false;

    /* Gérer le signe */
    if (value < 0) {
        negative = true;
        value = -value;
    } else if (value == 0) {
        temp[pos++] = '0';
    }

    /* Convertir les chiffres (ordre inverse) */
    while (value > 0 && pos < (int)(sizeof(temp) - 1)) {
        temp[pos++] = '0' + (value % 10);
        value /= 10;
    }
    if (negative && pos < (int)(sizeof(temp) - 1)) {
        temp[pos++] = '-';
    }
    temp[pos] = '\0';

    /* Inverser dans le buffer destination */
    size_t len = pos;
    if (len >= dst_size) len = dst_size - 1;

    for (size_t i = 0; i < len; i++) {
        dst[i] = temp[len - 1 - i];
    }
    dst[len] = '\0';

    return (int)len;
}

/**
 * @brief Convertit un entier non signé en chaîne
 */
int StringUtils_FormatUInt(char* dst, size_t dst_size, uint32_t value)
{
    if (!dst || dst_size < 2) return 0;

    char temp[TEMP_BUFFER_SIZE];
    int pos = 0;

    if (value == 0) {
        temp[pos++] = '0';
    }

    while (value > 0 && pos < (int)(sizeof(temp) - 1)) {
        temp[pos++] = '0' + (value % 10);
        value /= 10;
    }
    temp[pos] = '\0';

    size_t len = pos;
    if (len >= dst_size) len = dst_size - 1;

    for (size_t i = 0; i < len; i++) {
        dst[i] = temp[len - 1 - i];
    }
    dst[len] = '\0';

    return (int)len;
}

/**
 * @brief Convertit en hexadécimal
 */
int StringUtils_FormatHex(char* dst, size_t dst_size,
                          uint32_t value, uint8_t digits, bool uppercase)
{
    if (!dst || dst_size < 3) return 0;

    const char* hex_chars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char temp[TEMP_BUFFER_SIZE];
    int pos = 0;

    /* Si digits == 0, calculer le nombre de chiffres nécessaires */
    if (digits == 0) {
        uint32_t temp_val = value;
        do {
            digits++;
            temp_val >>= 4;
        } while (temp_val > 0);
    }

    /* Limiter digits */
    if (digits > 8) digits = 8;
    if (digits > (int)(dst_size - 1)) digits = (uint8_t)(dst_size - 1);

    /* Convertir */
    for (int i = digits - 1; i >= 0; i--) {
        temp[pos++] = hex_chars[(value >> (i * 4)) & 0x0F];
    }
    temp[pos] = '\0';

    /* Copier */
    memcpy(dst, temp, pos + 1);

    return pos;
}

/**
 * @brief Convertit en binaire
 */
int StringUtils_FormatBinary(char* dst, size_t dst_size,
                             uint32_t value, uint8_t bits)
{
    if (!dst || dst_size < 2) return 0;

    if (bits > 32) bits = 32;
    if (bits > dst_size - 1) bits = (uint8_t)(dst_size - 1);

    for (int i = bits - 1; i >= 0; i--) {
        dst[bits - 1 - i] = (value & (1 << i)) ? '1' : '0';
    }
    dst[bits] = '\0';

    return bits;
}

/**
 * @brief Convertit un float en chaîne
 */
int StringUtils_FormatFloat(char* dst, size_t dst_size,
                            float value, uint8_t decimals)
{
    if (!dst || dst_size < 2) return 0;

    if (decimals > 6) decimals = 6;

    /* Partie entière */
    int32_t int_part = (int32_t)value;

    /* Partie décimale */
    float frac = value - (float)int_part;
    if (frac < 0) frac = -frac;

    for (uint8_t i = 0; i < decimals; i++) {
        frac *= 10.0f;
    }
    uint32_t frac_part = (uint32_t)(frac + 0.5f);  /* Arrondi */

    /* Formater */
    int total = 0;
    total += StringUtils_FormatInt(dst, dst_size, int_part);

    if (decimals > 0 && total + 1 < (int)dst_size) {
        dst[total++] = '.';

        /* Formater la partie décimale avec zéros */
        char frac_str[8];
        int frac_len = StringUtils_FormatUInt(frac_str, sizeof(frac_str), frac_part);

        /* Ajouter les zéros manquants */
        int zeros_needed = (int)decimals - frac_len;
        while (zeros_needed > 0 && total < (int)dst_size - 1) {
            dst[total++] = '0';
            zeros_needed--;
        }

        /* Copier les chiffres */
        for (int i = 0; i < frac_len && total < (int)dst_size - 1; i++) {
            dst[total++] = frac_str[i];
        }
    }

    dst[total] = '\0';
    return total;
}

/**
 * @brief Formate une durée
 */
int StringUtils_FormatDuration(char* dst, size_t dst_size,
                               uint32_t total_seconds, bool show_hours)
{
    if (!dst || dst_size < 6) return 0;

    uint32_t hours = total_seconds / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;
    uint32_t seconds = total_seconds % 60;

    if (show_hours && hours > 0) {
        return snprintf(dst, dst_size, "%02lu:%02lu:%02lu", hours, minutes, seconds);
    } else {
        return snprintf(dst, dst_size, "%02lu:%02lu", minutes, seconds);
    }
}

/**
 * @brief Formate une taille en octets
 */
int StringUtils_FormatSize(char* dst, size_t dst_size, uint32_t bytes)
{
    if (!dst || dst_size < 2) return 0;

    if (bytes < 1024) {
        return snprintf(dst, dst_size, "%lu o", bytes);
    } else if (bytes < 1048576) {
        return snprintf(dst, dst_size, "%.2f Ko", bytes / 1024.0f);
    } else {
        return snprintf(dst, dst_size, "%.2f Mo", bytes / 1048576.0f);
    }
}

/**
 * @brief Formate un numéro de téléphone
 */
int StringUtils_FormatPhone(char* dst, size_t dst_size,
                            const char* number, const char* country)
{
    if (!dst || !number || dst_size < 2) return 0;

    /* Format : +33 6 12 34 56 78 (par paires) */
    size_t pos = 0;

    /* Code pays */
    if (country && country[0]) {
        dst[pos++] = '+';
        for (size_t i = 0; country[i] && pos < dst_size - 1; i++) {
            if (country[i] >= '0' && country[i] <= '9') {
                dst[pos++] = country[i];
            }
        }
        if (pos < dst_size - 1) dst[pos++] = ' ';
    }

    /* Numéro par paires */
    size_t len = strlen(number);
    for (size_t i = 0; i < len && pos < dst_size - 1; i++) {
        if (number[i] >= '0' && number[i] <= '9') {
            if (i > 0 && i % 2 == 0 && pos < dst_size - 1) {
                dst[pos++] = ' ';
            }
            dst[pos++] = number[i];
        }
    }

    dst[pos] = '\0';
    return (int)pos;
}

/**
 * @brief Formate une date
 */
int StringUtils_FormatDate(char* dst, size_t dst_size,
                           uint8_t day, uint8_t month, uint16_t year)
{
    if (!dst || dst_size < 2) return 0;
    return snprintf(dst, dst_size, "%02u/%02u/%04u", day, month, year);
}

/**
 * @brief Formate une heure
 */
int StringUtils_FormatTime(char* dst, size_t dst_size,
                           uint8_t hour, uint8_t minute, uint8_t second)
{
    if (!dst || dst_size < 2) return 0;
    return snprintf(dst, dst_size, "%02u:%02u:%02u", hour, minute, second);
}

/* ======================================================================== */
/*              MANIPULATION                                                */
/* ======================================================================== */

/**
 * @brief Copie sécurisée
 */
size_t StringUtils_Copy(char* dst, size_t dst_size, const char* src)
{
    if (!dst || dst_size == 0) return 0;
    if (!src) {
        dst[0] = '\0';
        return 0;
    }

    size_t i;
    for (i = 0; i < dst_size - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';

    return i;
}

/**
 * @brief Concaténation sécurisée
 */
size_t StringUtils_Concat(char* dst, size_t dst_size, const char* src)
{
    if (!dst || dst_size == 0) return 0;
    if (!src) return strlen(dst);

    size_t dst_len = strlen(dst);
    size_t remaining = dst_size - dst_len - 1;
    size_t i;

    for (i = 0; i < remaining && src[i] != '\0'; i++) {
        dst[dst_len + i] = src[i];
    }
    dst[dst_len + i] = '\0';

    return dst_len + i;
}

/**
 * @brief Tronque avec "..."
 */
int StringUtils_Truncate(char* dst, size_t dst_size,
                         const char* src, size_t max_len)
{
    if (!dst || !src || dst_size == 0) return 0;

    size_t src_len = strlen(src);

    if (src_len <= max_len) {
        return StringUtils_Copy(dst, dst_size, src);
    }

    /* Copier max_len-3 caractères + "..." */
    size_t copy_len = (max_len > 3) ? max_len - 3 : 0;
    if (copy_len >= dst_size) copy_len = dst_size - 4;

    size_t i;
    for (i = 0; i < copy_len && i < dst_size - 4; i++) {
        dst[i] = src[i];
    }

    /* Ajouter "..." */
    if (i + 3 < dst_size) {
        dst[i++] = '.';
        dst[i++] = '.';
        dst[i++] = '.';
    }
    dst[i] = '\0';

    return (int)i;
}

/**
 * @brief Supprime les espaces (trim)
 */
char* StringUtils_Trim(char* str)
{
    if (!str) return NULL;
    return StringUtils_TrimRight(StringUtils_TrimLeft(str));
}

/**
 * @brief Trim gauche
 */
char* StringUtils_TrimLeft(char* str)
{
    if (!str) return NULL;

    /* Trouver le premier caractère non-espace */
    char* start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')) {
        start++;
    }

    /* Déplacer le contenu */
    if (start != str) {
        char* dst = str;
        while (*start) {
            *dst++ = *start++;
        }
        *dst = '\0';
    }

    return str;
}

/**
 * @brief Trim droit
 */
char* StringUtils_TrimRight(char* str)
{
    if (!str || *str == '\0') return str;

    char* end = str + strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }

    return str;
}

/**
 * @brief Convertit en majuscules
 */
void StringUtils_ToUpper(char* str)
{
    if (!str) return;
    while (*str) {
        *str = to_upper_char(*str);
        str++;
    }
}

/**
 * @brief Convertit en minuscules
 */
void StringUtils_ToLower(char* str)
{
    if (!str) return;
    while (*str) {
        *str = to_lower_char(*str);
        str++;
    }
}

/**
 * @brief Inverse une chaîne
 */
void StringUtils_Reverse(char* str)
{
    if (!str) return;
    size_t len = strlen(str);
    for (size_t i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = temp;
    }
}

/**
 * @brief Remplit un buffer
 */
int StringUtils_Fill(char* dst, size_t dst_size, char c, size_t count)
{
    if (!dst || dst_size == 0) return 0;
    if (count >= dst_size) count = dst_size - 1;

    for (size_t i = 0; i < count; i++) {
        dst[i] = c;
    }
    dst[count] = '\0';

    return (int)count;
}

/**
 * @brief Pad gauche
 */
void StringUtils_PadLeft(char* dst, size_t dst_size,
                         size_t total_len, char pad_char)
{
    if (!dst || dst_size == 0) return;

    size_t current_len = strlen(dst);
    if (current_len >= total_len) return;

    size_t pad_count = total_len - current_len;
    if (current_len + pad_count >= dst_size) {
        pad_count = dst_size - current_len - 1;
    }

    /* Déplacer les caractères vers la droite */
    memmove(dst + pad_count, dst, current_len + 1);

    /* Remplir avec le caractère de padding */
    for (size_t i = 0; i < pad_count; i++) {
        dst[i] = pad_char;
    }
}

/**
 * @brief Pad droite
 */
void StringUtils_PadRight(char* dst, size_t dst_size,
                          size_t total_len, char pad_char)
{
    if (!dst || dst_size == 0) return;

    size_t current_len = strlen(dst);
    if (current_len >= total_len) return;

    size_t pad_count = total_len - current_len;
    if (current_len + pad_count >= dst_size) {
        pad_count = dst_size - current_len - 1;
    }

    for (size_t i = 0; i < pad_count; i++) {
        dst[current_len + i] = pad_char;
    }
    dst[current_len + pad_count] = '\0';
}

/* ======================================================================== */
/*              RECHERCHE                                                   */
/* ======================================================================== */

/**
 * @brief Trouve un caractère
 */
const char* StringUtils_FindChar(const char* str, char c)
{
    if (!str) return NULL;
    return strchr(str, c);
}

/**
 * @brief Trouve le dernier caractère
 */
const char* StringUtils_FindLastChar(const char* str, char c)
{
    if (!str) return NULL;
    return strrchr(str, c);
}

/**
 * @brief Trouve une sous-chaîne
 */
const char* StringUtils_FindSubstr(const char* str, const char* substr,
                                   bool case_insensitive)
{
    if (!str || !substr) return NULL;

    if (case_insensitive) {
        size_t str_len = strlen(str);
        size_t sub_len = strlen(substr);
        if (sub_len > str_len) return NULL;

        for (size_t i = 0; i <= str_len - sub_len; i++) {
            bool match = true;
            for (size_t j = 0; j < sub_len; j++) {
                if (to_lower_char(str[i + j]) != to_lower_char(substr[j])) {
                    match = false;
                    break;
                }
            }
            if (match) return str + i;
        }
        return NULL;
    } else {
        return strstr(str, substr);
    }
}

/**
 * @brief Vérifie le préfixe
 */
bool StringUtils_StartsWith(const char* str, const char* prefix)
{
    if (!str || !prefix) return false;
    size_t prefix_len = strlen(prefix);
    return strncmp(str, prefix, prefix_len) == 0;
}

/**
 * @brief Vérifie le suffixe
 */
bool StringUtils_EndsWith(const char* str, const char* suffix)
{
    if (!str || !suffix) return false;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

/**
 * @brief Compte les occurrences d'un caractère
 */
int StringUtils_CountChar(const char* str, char c)
{
    if (!str) return 0;
    int count = 0;
    while (*str) {
        if (*str == c) count++;
        str++;
    }
    return count;
}

/**
 * @brief Compte les occurrences d'une sous-chaîne
 */
int StringUtils_CountSubstr(const char* str, const char* substr)
{
    if (!str || !substr || substr[0] == '\0') return 0;

    int count = 0;
    size_t sub_len = strlen(substr);
    const char* ptr = str;

    while ((ptr = strstr(ptr, substr)) != NULL) {
        count++;
        ptr += sub_len;
    }

    return count;
}

/* ======================================================================== */
/*              PARSING                                                     */
/* ======================================================================== */

/**
 * @brief Tokenize
 */
char* StringUtils_Tokenize(char* str, const char* delim)
{
    static char* saved = NULL;
    if (str) saved = str;
    if (!saved) return NULL;

    /* Sauter les délimiteurs */
    while (*saved && strchr(delim, *saved)) {
        saved++;
    }

    if (*saved == '\0') {
        saved = NULL;
        return NULL;
    }

    char* token = saved;

    /* Trouver le prochain délimiteur */
    while (*saved && !strchr(delim, *saved)) {
        saved++;
    }

    if (*saved) {
        *saved = '\0';
        saved++;
    }

    return token;
}

/**
 * @brief Parse un entier signé
 */
bool StringUtils_ParseInt(const char* str, int32_t* value)
{
    if (!str || !value) return false;

    char* endptr;
    long result = strtol(str, &endptr, 10);

    if (endptr == str || *endptr != '\0') return false;

    *value = (int32_t)result;
    return true;
}

/**
 * @brief Parse un entier non signé
 */
bool StringUtils_ParseUInt(const char* str, uint32_t* value)
{
    if (!str || !value) return false;

    char* endptr;
    unsigned long result = strtoul(str, &endptr, 10);

    if (endptr == str || *endptr != '\0') return false;

    *value = (uint32_t)result;
    return true;
}

/**
 * @brief Parse un hexadécimal
 */
bool StringUtils_ParseHex(const char* str, uint32_t* value)
{
    if (!str || !value) return false;

    /* Sauter "0x" ou "#" */
    if (StringUtils_StartsWith(str, "0x") || StringUtils_StartsWith(str, "0X")) {
        str += 2;
    } else if (str[0] == '#') {
        str++;
    }

    char* endptr;
    unsigned long result = strtoul(str, &endptr, 16);

    if (endptr == str || *endptr != '\0') return false;

    *value = (uint32_t)result;
    return true;
}

/**
 * @brief Parse un float
 */
bool StringUtils_ParseFloat(const char* str, float* value)
{
    if (!str || !value) return false;

    char* endptr;
    float result = strtof(str, &endptr);

    if (endptr == str || *endptr != '\0') return false;

    *value = result;
    return true;
}

/**
 * @brief Parse un booléen
 */
bool StringUtils_ParseBool(const char* str, bool* value)
{
    if (!str || !value) return false;

    /* Insensible à la casse */
    if (strcasecmp(str, "true") == 0 || strcmp(str, "1") == 0 ||
        strcasecmp(str, "on") == 0 || strcasecmp(str, "yes") == 0) {
        *value = true;
        return true;
    }

    if (strcasecmp(str, "false") == 0 || strcmp(str, "0") == 0 ||
        strcasecmp(str, "off") == 0 || strcasecmp(str, "no") == 0) {
        *value = false;
        return true;
    }

    return false;
}

/* ======================================================================== */
/*              COMPARAISON                                                 */
/* ======================================================================== */

/**
 * @brief Compare deux chaînes
 */
int StringUtils_Compare(const char* a, const char* b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a, b);
}

/**
 * @brief Compare insensible à la casse
 */
int StringUtils_CompareIgnoreCase(const char* a, const char* b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;

    while (*a && *b) {
        char ca = to_lower_char(*a);
        char cb = to_lower_char(*b);
        if (ca != cb) return (ca - cb);
        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}

/**
 * @brief Compare les n premiers caractères
 */
int StringUtils_CompareN(const char* a, const char* b, size_t n)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strncmp(a, b, n);
}

/* ======================================================================== */
/*              VALIDATION                                                  */
/* ======================================================================== */

bool StringUtils_IsEmpty(const char* str)
{
    return str == NULL || str[0] == '\0';
}

bool StringUtils_IsDigits(const char* str)
{
    if (!str || *str == '\0') return false;
    while (*str) {
        if (!is_digit_char(*str)) return false;
        str++;
    }
    return true;
}

bool StringUtils_IsAlpha(const char* str)
{
    if (!str || *str == '\0') return false;
    while (*str) {
        if (!is_alpha_char(*str)) return false;
        str++;
    }
    return true;
}

bool StringUtils_IsAlphaNumeric(const char* str)
{
    if (!str || *str == '\0') return false;
    while (*str) {
        if (!is_alphanum_char(*str)) return false;
        str++;
    }
    return true;
}

bool StringUtils_IsHex(const char* str)
{
    if (!str || *str == '\0') return false;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }
    while (*str) {
        if (!is_hex_char(*str)) return false;
        str++;
    }
    return true;
}

/**
 * @brief Valide un numéro de téléphone
 */
bool StringUtils_IsPhoneNumber(const char* str)
{
    if (!str || *str == '\0') return false;

    int digit_count = 0;
    while (*str) {
        if (*str >= '0' && *str <= '9') {
            digit_count++;
        } else if (*str == '+' || *str == '-' || *str == ' ' || 
                   *str == '(' || *str == ')' || *str == '.') {
            /* Caractères autorisés */
        } else {
            return false;
        }
        str++;
    }

    /* Au moins 8 chiffres, max 15 */
    return digit_count >= 8 && digit_count <= 15;
}

/**
 * @brief Valide un email (simplifié)
 */
bool StringUtils_IsEmail(const char* str)
{
    if (!str || *str == '\0') return false;

    /* Doit contenir @ et . */
    const char* at = strchr(str, '@');
    if (!at || at == str) return false;  /* Pas de @, ou au début */

    const char* dot = strrchr(at, '.');
    if (!dot || dot == at + 1 || dot[1] == '\0') return false;  /* Pas de point après @ */

    /* Vérifier les caractères valides */
    for (const char* p = str; *p; p++) {
        if (!is_alphanum_char(*p) && *p != '@' && *p != '.' && 
            *p != '_' && *p != '-' && *p != '+') {
            return false;
        }
    }

    return true;
}

/* ======================================================================== */
/*              DIVERS                                                      */
/* ======================================================================== */

size_t StringUtils_Length(const char* str)
{
    return str ? strlen(str) : 0;
}

/**
 * @brief Remplace un caractère
 */
int StringUtils_ReplaceChar(char* str, char from, char to)
{
    if (!str) return 0;
    int count = 0;
    while (*str) {
        if (*str == from) {
            *str = to;
            count++;
        }
        str++;
    }
    return count;
}

/**
 * @brief Échappe pour CSV
 */
int StringUtils_EscapeCSV(char* dst, size_t dst_size, const char* src)
{
    if (!dst || !src || dst_size < 3) return 0;

    /* Si le champ contient une virgule, des guillemets ou un retour ligne */
    bool needs_quotes = false;
    for (const char* p = src; *p; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        return StringUtils_Copy(dst, dst_size, src);
    }

    /* Entourer de guillemets et doubler les guillemets internes */
    size_t pos = 0;
    dst[pos++] = '"';

    for (const char* p = src; *p && pos < dst_size - 2; p++) {
        if (*p == '"' && pos < dst_size - 2) {
            dst[pos++] = '"';  /* Doubler */
        }
        dst[pos++] = *p;
    }

    if (pos < dst_size - 1) dst[pos++] = '"';
    dst[pos] = '\0';

    return (int)pos;
}

/**
 * @brief Encode en Base64 simplifié
 */
int StringUtils_Base64Encode(char* dst, size_t dst_size,
                             const uint8_t* src, size_t src_len)
{
    if (!dst || !src || dst_size < 2) return 0;

    static const char b64_chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    size_t out_pos = 0;
    size_t i = 0;
    uint32_t triple;

    while (i < src_len) {
        /* Collecter 3 octets */
        triple = 0;
        int bytes_in_triple = 0;

        for (int j = 0; j < 3 && i < src_len; j++, i++) {
            triple = (triple << 8) | src[i];
            bytes_in_triple++;
        }

        /* Padding */
        while (bytes_in_triple < 3) {
            triple <<= 8;
            bytes_in_triple++;
        }

        /* Encoder 4 caractères */
        for (int j = 18; j >= 0 && out_pos < dst_size - 1; j -= 6) {
            uint8_t index = (triple >> j) & 0x3F;
            dst[out_pos++] = b64_chars[index];
        }
    }

    /* Remplacer les derniers caractères par = si nécessaire */
    size_t padding = (3 - (src_len % 3)) % 3;
    for (size_t p = 0; p < padding && out_pos > 0 && out_pos < dst_size - 1; p++) {
        dst[out_pos - 1 - p] = '=';
    }

    dst[out_pos] = '\0';
    return (int)out_pos;
}

/* ======================================================================== */
/*              FONCTIONS PRIVÉES                                           */
/* ======================================================================== */

static char to_lower_char(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static char to_upper_char(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

static bool is_digit_char(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_alpha_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_alphanum_char(char c)
{
    return is_digit_char(c) || is_alpha_char(c);
}

static bool is_hex_char(char c)
{
    return is_digit_char(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */