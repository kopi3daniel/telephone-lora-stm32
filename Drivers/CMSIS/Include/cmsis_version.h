/**
 * @file    cmsis_version.h
 * @brief   CMSIS version definitions
 * @version V5.6.0
 */

#ifndef CMSIS_VERSION_H
#define CMSIS_VERSION_H

/* CMSIS Version */
#define __CM_CMSIS_VERSION_MAIN  (5U)       /* [31:24] main version */
#define __CM_CMSIS_VERSION_SUB   (6U)       /* [23:16] sub version  */
#define __CM_CMSIS_VERSION       ((__CM_CMSIS_VERSION_MAIN << 24) | \
                                   (__CM_CMSIS_VERSION_SUB  << 16))

/* Compiler detection */
#if defined ( __GNUC__ )
  #define __COMPILER_GCC
#elif defined ( __ICCARM__ )
  #define __COMPILER_IAR
#elif defined ( __CC_ARM )
  #define __COMPILER_ARMCC
#endif

#endif /* CMSIS_VERSION_H */