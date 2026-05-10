/**
 * @file    tinyprintf.c
 * @brief   Implémentation du printf minimaliste
 * @author  Georges Menie
 * @license MIT
 * @version 1.2
 * 
 * Implémente un sous-ensemble de printf pour systèmes embarqués.
 * 
 * FONCTIONNEMENT :
 * 
 * 1. INITIALISATION :
 *    - init_printf() enregistre le callback de sortie
 *    - Le callback est appelé pour chaque caractère
 * 
 * 2. FORMATAGE :
 *    - tfp_format() parse la chaîne de format
 *    - Pour chaque % rencontré :
 *      a. Parse les flags (-, 0, +, espace)
 *      b. Parse la largeur minimale
 *      c. Parse le modificateur de longueur (l)
 *      d. Parse le spécificateur (s, c, d, u, x, X, o, b, p)
 *      e. Formate la valeur selon le spécificateur
 *      f. Émet les caractères via le callback
 * 
 * 3. SORTIE :
 *    - Chaque caractère est émis individuellement
 *    - Pas de buffer interne (économie RAM)
 *    - Le callback peut être UART, ITM, buffer, etc.
 * 
 * EXEMPLE DE FLUX :
 * 
 *   tfp_printf("Val=%03d", 7);
 * 
 *   1. Émet 'V', 'a', 'l', '='
 *   2. Rencontre %03d
 *      - Flag : 0 (zéro padding)
 *      - Largeur : 3
 *      - Spécificateur : d
 *      - Valeur : 7
 *   3. Émet '0', '0', '7'  (padding zéro + valeur)
 */

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include "tinyprintf.h"
#include <string.h>

/* ======================================================================== */
/*                       MACROS PRIVÉES                                      */
/* ======================================================================== */

/** Taille maximale pour la conversion de nombre */
#define PRINTF_NTOA_BUFFER_SIZE             32

/** Caractères pour les bases numériques */
#define PRINTF_DIGITS_LOWER                 "0123456789abcdef"
#define PRINTF_DIGITS_UPPER                 "0123456789ABCDEF"

/* ======================================================================== */
/*                VARIABLES STATIQUES                                       */
/* ======================================================================== */

/** Callback de sortie enregistré */
static putcf_t g_putf = NULL;

/** Contexte utilisateur */
static void* g_putf_ctx = NULL;

/* ======================================================================== */
/*                PROTOTYPES FONCTIONS PRIVÉES                              */
/* ======================================================================== */

static void putchar_wrapper(char c);
static char* number_to_string(char* buf, unsigned int value, int base, 
                               int uppercase, int width, char pad, int left_align);
static int atoi_simple(const char** str);

/* ======================================================================== */
/*                  FONCTIONS PUBLIQUES                                      */
/* ======================================================================== */

/**
 * @brief Initialise le système printf
 */
void init_printf(void* ctx, putcf_t putf)
{
    g_putf_ctx = ctx;
    g_putf = putf;
}

/**
 * @brief Formate et émet une chaîne (printf-like)
 */
void tfp_printf(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    tfp_format(g_putf_ctx, g_putf, fmt, va);
    va_end(va);
}

/**
 * @brief Formate dans un buffer (snprintf-like)
 */
int tfp_snprintf(char* dst, size_t size, const char* fmt, ...)
{
    if (!dst || size == 0) return 0;

    va_list va;
    va_start(va, fmt);
    int result = tfp_vsnprintf(dst, size, fmt, va);
    va_end(va);

    return result;
}

/**
 * @brief Formate dans un buffer (vsnprintf-like)
 */
int tfp_vsnprintf(char* dst, size_t size, const char* fmt, va_list va)
{
    if (!dst || size == 0) return 0;

    /* Structure pour l'écriture dans un buffer */
    struct buffer_ctx {
        char* buf;
        size_t size;
        size_t pos;
    } ctx = { dst, size - 1, 0 };

    /* Callback qui écrit dans le buffer */
    void buffer_putc(void* ctx_ptr, char c) {
        struct buffer_ctx* b = (struct buffer_ctx*)ctx_ptr;
        if (b->pos < b->size) {
            b->buf[b->pos++] = c;
        }
    }

    /* Utiliser tfp_format avec notre callback buffer */
    tfp_format(&ctx, buffer_putc, fmt, va);

    /* Terminer la chaîne */
    dst[ctx.pos] = '\0';

    return (int)ctx.pos;
}

/**
 * @brief Formate et émet une chaîne (version complète avec va_list)
 */
void tfp_format(void* ctx, putcf_t putf, const char* fmt, va_list va)
{
    /* Sauvegarder le callback global */
    putcf_t saved_putf = g_putf;
    void* saved_ctx = g_putf_ctx;

    /* Utiliser le callback fourni */
    g_putf = putf;
    g_putf_ctx = ctx;

    if (!g_putf || !fmt) {
        g_putf = saved_putf;
        g_putf_ctx = saved_ctx;
        return;
    }

    char ch;
    char ntoa_buf[PRINTF_NTOA_BUFFER_SIZE];

    while ((ch = *(fmt++))) {
        if (ch != '%') {
            /* Caractère normal : émettre directement */
            putchar_wrapper(ch);
            continue;
        }

        /* Début d'un spécificateur % */
        char pad = ' ';      /* Caractère de padding (espace par défaut) */
        int width = 0;       /* Largeur minimale */
        int left_align = 0;  /* Alignement gauche */
        int long_flag = 0;   /* Modificateur long (l) */

        /* Flags */
        char flags_done = 0;
        while (!flags_done) {
            ch = *(fmt++);
            switch (ch) {
                case '-': left_align = 1; break;
                case '0': pad = '0'; break;
                case ' ': /* Ignoré */ break;
                case '+': /* Ignoré */ break;
                default:  flags_done = 1; break;
            }
        }

        /* Largeur */
        if (ch >= '0' && ch <= '9') {
            width = atoi_simple(&fmt);
            ch = *(fmt - 1);  /* atoi a avancé fmt d'un cran de trop */
        }

        /* Modificateur de longueur */
        if (ch == 'l') {
            long_flag = 1;
            ch = *(fmt++);
        }

        /* Spécificateur */
        switch (ch) {
            case '\0':
                /* Fin de chaîne après % : ne rien faire */
                fmt--;
                break;

            case '%':
                /* %% → émettre un % */
                putchar_wrapper('%');
                break;

            case 'c': {
                /* Caractère */
                char c = (char)va_arg(va, int);
                /* Padding si largeur > 1 */
                if (!left_align) {
                    for (int i = 1; i < width; i++) putchar_wrapper(' ');
                }
                putchar_wrapper(c);
                if (left_align) {
                    for (int i = 1; i < width; i++) putchar_wrapper(' ');
                }
                break;
            }

            case 's': {
                /* Chaîne */
                const char* str = va_arg(va, const char*);
                if (!str) str = "(null)";
                int len = (int)strlen(str);
                /* Padding */
                if (!left_align) {
                    for (int i = len; i < width; i++) putchar_wrapper(' ');
                }
                while (*str) putchar_wrapper(*str++);
                if (left_align) {
                    for (int i = len; i < width; i++) putchar_wrapper(' ');
                }
                break;
            }

            case 'd':
            case 'i': {
                /* Entier signé */
                int value;
                if (long_flag) {
                    value = (int)va_arg(va, long int);
                } else {
                    value = va_arg(va, int);
                }
                if (value < 0) {
                    putchar_wrapper('-');
                    value = -value;
                    width--;
                }
                number_to_string(ntoa_buf, (unsigned int)value, 10, 0, 
                                width, pad, left_align);
                break;
            }

            case 'u': {
                /* Entier non signé */
                unsigned int value;
                if (long_flag) {
                    value = (unsigned int)va_arg(va, unsigned long int);
                } else {
                    value = va_arg(va, unsigned int);
                }
                number_to_string(ntoa_buf, value, 10, 0, 
                                width, pad, left_align);
                break;
            }

            case 'x': {
                /* Hexadécimal minuscule */
                unsigned int value = va_arg(va, unsigned int);
                number_to_string(ntoa_buf, value, 16, 0, 
                                width, pad, left_align);
                break;
            }

            case 'X': {
                /* Hexadécimal majuscule */
                unsigned int value = va_arg(va, unsigned int);
                number_to_string(ntoa_buf, value, 16, 1, 
                                width, pad, left_align);
                break;
            }

            case 'o': {
                /* Octal */
                unsigned int value = va_arg(va, unsigned int);
                number_to_string(ntoa_buf, value, 8, 0, 
                                width, pad, left_align);
                break;
            }

            case 'b': {
                /* Binaire */
                unsigned int value = va_arg(va, unsigned int);
                number_to_string(ntoa_buf, value, 2, 0, 
                                width, pad, left_align);
                break;
            }

            case 'p': {
                /* Pointeur (format 0xXXXXXXXX) */
                void* ptr = va_arg(va, void*);
                putchar_wrapper('0');
                putchar_wrapper('x');
                width = 8;  /* 8 chiffres hex */
                pad = '0';
                number_to_string(ntoa_buf, (unsigned int)(uintptr_t)ptr, 16, 1,
                                width, pad, left_align);
                break;
            }

            default:
                /* Spécificateur inconnu : émettre tel quel */
                putchar_wrapper('%');
                putchar_wrapper(ch);
                break;
        }
    }

    /* Restaurer le callback global */
    g_putf = saved_putf;
    g_putf_ctx = saved_ctx;
}

/* ======================================================================== */
/*              FONCTIONS PRIVÉES                                           */
/* ======================================================================== */

/**
 * @brief Émet un caractère via le callback enregistré
 */
static void putchar_wrapper(char c)
{
    if (g_putf) {
        g_putf(g_putf_ctx, c);
    }
}

/**
 * @brief Convertit un nombre en chaîne et l'émet
 * 
 * @param buf           Buffer temporaire
 * @param value         Valeur à convertir
 * @param base          Base (2=binaire, 8=octal, 10=décimal, 16=hex)
 * @param uppercase     true = hex majuscule
 * @param width         Largeur minimale (padding)
 * @param pad           Caractère de padding (' ' ou '0')
 * @param left_align    Alignement gauche
 * @return              Pointeur dans buf (non utilisé)
 */
static char* number_to_string(char* buf, unsigned int value, int base,
                               int uppercase, int width, char pad, int left_align)
{
    const char* digits = uppercase ? PRINTF_DIGITS_UPPER : PRINTF_DIGITS_LOWER;
    int pos = 0;

    /* Conversion (chiffres dans l'ordre inverse) */
    do {
        buf[pos++] = digits[value % base];
        value /= base;
    } while (value > 0);

    int num_digits = pos;

    /* Padding si nécessaire */
    if (!left_align) {
        for (int i = num_digits; i < width; i++) {
            putchar_wrapper(pad);
        }
    }

    /* Émettre les chiffres (ordre inverse) */
    while (pos > 0) {
        putchar_wrapper(buf[--pos]);
    }

    /* Padding gauche si aligné à gauche */
    if (left_align) {
        for (int i = num_digits; i < width; i++) {
            putchar_wrapper(' ');
        }
    }

    return buf;
}

/**
 * @brief Parse un entier simple depuis une chaîne
 * 
 * Avance le pointeur de chaîne pendant le parsing.
 * 
 * @param str       Pointeur vers la chaîne (modifié)
 * @return          Entier parsé
 */
static int atoi_simple(const char** str)
{
    int value = 0;
    while (**str >= '0' && **str <= '9') {
        value = value * 10 + (**str - '0');
        (*str)++;
    }
    return value;
}

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */