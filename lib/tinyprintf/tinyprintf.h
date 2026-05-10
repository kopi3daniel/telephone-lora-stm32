/**
 * @file    tinyprintf.h
 * @brief   printf minimaliste pour systèmes embarqués
 * @author  Georges Menie
 * @license MIT
 * @version 1.2
 * 
 * Implémentation légère de printf pour STM32 et autres MCU.
 * 
 * CARACTÉRISTIQUES :
 *   - Pas d'allocation dynamique
 *   - Sortie caractère par caractère (callback)
 *   - Supporte les formats essentiels
 *   - Taille compilée : ~2 Ko
 * 
 * FORMATS SUPPORTÉS :
 *   %s  : chaîne de caractères
 *   %c  : caractère
 *   %d  : entier signé décimal
 *   %u  : entier non signé décimal
 *   %x  : hexadécimal minuscule
 *   %X  : hexadécimal majuscule
 *   %o  : octal
 *   %b  : binaire
 *   %p  : pointeur
 *   %%  : symbole pourcentage
 * 
 * MODIFICATEURS SUPPORTÉS :
 *   %5d    : largeur minimale (padding espace)
 *   %05d   : largeur minimale (padding zéro)
 *   %-5d   : alignement gauche
 *   %ld    : long (32 bits)
 *   %lu    : unsigned long
 *   %lx    : hex long
 * 
 * NON SUPPORTÉ (volontairement) :
 *   %f, %e, %g  : flottants (utiliser StringUtils_FormatFloat)
 *   %.*s        : précision chaîne
 *   %n          : stockage nombre caractères
 * 
 * UTILISATION :
 * 
 *   #include "tinyprintf.h"
 * 
 *   // Définir la fonction de sortie
 *   static void putchar_cb(void* ctx, char c) {
 *       USART1->TDR = c;
 *       while (!(USART1->SR & USART_SR_TXE));
 *   }
 * 
 *   int main(void) {
 *       // Initialiser
 *       init_printf(NULL, putchar_cb);
 * 
 *       // Utiliser
 *       tfp_printf("Hello %s! Value=%d (0x%X)\r\n", "World", 42, 42);
 *   }
 */

#ifndef TINYPRINTF_H
#define TINYPRINTF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*                               INCLUDES                                    */
/* ======================================================================== */

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

/* ======================================================================== */
/*                     TYPES                                                 */
/* ======================================================================== */

/**
 * @brief Callback de sortie pour un caractère
 * 
 * @param ctx   Contexte utilisateur (passé à init_printf)
 * @param c     Caractère à émettre
 */
typedef void (*putcf_t)(void* ctx, char c);

/* ======================================================================== */
/*              PROTOTYPES DES FONCTIONS                                    */
/* ======================================================================== */

/**
 * @brief Initialise le système printf
 * 
 * @param ctx           Contexte utilisateur (passé au callback)
 * @param putf          Fonction de sortie pour chaque caractère
 */
void init_printf(void* ctx, putcf_t putf);

/**
 * @brief Formate et émet une chaîne (version complète)
 * 
 * @param fmt       Format
 * @param va        Liste d'arguments variable
 */
void tfp_format(void* ctx, putcf_t putf, const char* fmt, va_list va);

/**
 * @brief Formate et émet une chaîne (printf-like)
 * 
 * @param fmt       Format
 * @param ...       Arguments
 */
void tfp_printf(const char* fmt, ...);

/**
 * @brief Formate dans un buffer (snprintf-like)
 * 
 * @param dst       Buffer destination
 * @param size      Taille du buffer
 * @param fmt       Format
 * @param ...       Arguments
 * @return          Nombre de caractères écrits (sans le '\0')
 */
int tfp_snprintf(char* dst, size_t size, const char* fmt, ...);

/**
 * @brief Formate dans un buffer (vsnprintf-like)
 * 
 * @param dst       Buffer
 * @param size      Taille
 * @param fmt       Format
 * @param va        Liste d'arguments
 * @return          Nombre de caractères écrits
 */
int tfp_vsnprintf(char* dst, size_t size, const char* fmt, va_list va);

/* ======================================================================== */
/*              MACROS DE COMPATIBILITÉ                                     */
/* ======================================================================== */

/**
 * @brief Redéfinit printf pour utiliser tinyprintf
 * 
 * Décommenter pour remplacer le printf standard.
 * Attention : conflit avec newlib si celle-ci est linkée.
 */

/*
#define printf          tfp_printf
#define snprintf        tfp_snprintf
#define vsnprintf       tfp_vsnprintf
*/

/* ======================================================================== */
/*                           FIN DU FICHIER                                  */
/* ======================================================================== */