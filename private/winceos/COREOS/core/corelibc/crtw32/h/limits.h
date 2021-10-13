//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/***
*limits.h - implementation dependent values
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Contains defines for a number of implementation dependent values
*       which are commonly used in C programs.
*       [ANSI]
*
*       [Public]
*
*Revision History:
*       06-03-87  JMB   Added support for unsigned char max values
*       08-19-88  GJF   Revised to also support the 386
*       04-28-89  SKS   Put parentheses around negative constants
*       08-17-89  GJF   Cleanup, now specific to 386
*       10-30-89  GJF   Fixed copyright
*       11-15-89  KRS   Add MB_LEN_MAX, fix CHAR_MIN/MAX, order like ANSI spec.
*       03-01-90  GJF   Added #ifndef _INC_LIMITS stuff
*       02-21-91  KRS   Change MB_LEN_MAX to 2 for C 7.00.
*       03-30-92  DJM   POSIX support.
*       08-22-92  SRW   Fix value of _POSIX_ARG_MAX
*       12-14-92  SRW   Fix value of _POSIX_ARG_MAX again
*       12-14-92  SRW   Back merge MattBr's changes from 10/29/92
*       01-15-93  CFW   Added __c_lconvinit dummy variable.
*       01-28-93  SRW   MAke PATH_MAX for Posix right for Posix
*       02-01-93  CFW   Removed __c_lconvinit vars to locale.h.
*       03-10-93  MJB   More fixes for Posix.
*       04-07-93  SKS   Fix copyright
*       04-20-93  GJF   Changed SCHAR_MIN, SHRT_MIN, INT_MIN, LONG_MIN
*       04-21-93  GJF   By directive of JonM, backed out prior change.
*       04-26-93  GJF   By directive of JonM, restored change.
*       02-01-94  GJF   Added type suffixes to LONG_MAX, LONG_MIN and
*                       ULONG_MAX. Added constants for sized integer types.
*       02-23-94  CFW   Add u suffixes.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       12-14-95  JWM   Add "#pragma once".
*       01-01-96  BWT   Define LINK_MAX for POSIX.
*       02-24-97  GJF   Detab-ed.
*       05-17-99  PML   Remove all Macintosh support.
*       12-16-99  GB    Updated MB_LEN_MAX
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*       11-22-04  AC    Added LLONG_MAX, LLONG_MIN, ULLONG_MAX
*                       VSW#415215
*       03-23-05  MSL   Review comment removal
*       11-10-06  PMB   Removed most _INTEGRAL_MAX_BITS #ifdefs
*                       DDB#11174
*
****/

#pragma once

#include <crtdefs.h>

#ifndef _INC_LIMITS
#define _INC_LIMITS

#ifndef _CRTBLD
/* This version of the header files is NOT for user programs.
 * It is intended for use when building the C runtimes ONLY.
 * The version intended for public use will not have this message.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#define CHAR_BIT      8         /* number of bits in a char */
#define SCHAR_MIN   (-128)      /* minimum signed char value */
#define SCHAR_MAX     127       /* maximum signed char value */
#define UCHAR_MAX     0xff      /* maximum unsigned char value */

#ifndef _CHAR_UNSIGNED
#define CHAR_MIN    SCHAR_MIN   /* mimimum char value */
#define CHAR_MAX    SCHAR_MAX   /* maximum char value */
#else
#define CHAR_MIN      0
#define CHAR_MAX    UCHAR_MAX
#endif  /* _CHAR_UNSIGNED */

#define MB_LEN_MAX    5             /* max. # bytes in multibyte char */
#define SHRT_MIN    (-32768)        /* minimum (signed) short value */
#define SHRT_MAX      32767         /* maximum (signed) short value */
#define USHRT_MAX     0xffff        /* maximum unsigned short value */
#define INT_MIN     (-2147483647 - 1) /* minimum (signed) int value */
#define INT_MAX       2147483647    /* maximum (signed) int value */
#define UINT_MAX      0xffffffff    /* maximum unsigned int value */
#define LONG_MIN    (-2147483647L - 1) /* minimum (signed) long value */
#define LONG_MAX      2147483647L   /* maximum (signed) long value */
#define ULONG_MAX     0xffffffffUL  /* maximum unsigned long value */
#define LLONG_MAX     9223372036854775807i64       /* maximum signed long long int value */
#define LLONG_MIN   (-9223372036854775807i64 - 1)  /* minimum signed long long int value */
#define ULLONG_MAX    0xffffffffffffffffui64       /* maximum unsigned long long int value */

#define _I8_MIN     (-127i8 - 1)    /* minimum signed 8 bit value */
#define _I8_MAX       127i8         /* maximum signed 8 bit value */
#define _UI8_MAX      0xffui8       /* maximum unsigned 8 bit value */

#define _I16_MIN    (-32767i16 - 1) /* minimum signed 16 bit value */
#define _I16_MAX      32767i16      /* maximum signed 16 bit value */
#define _UI16_MAX     0xffffui16    /* maximum unsigned 16 bit value */

#define _I32_MIN    (-2147483647i32 - 1) /* minimum signed 32 bit value */
#define _I32_MAX      2147483647i32 /* maximum signed 32 bit value */
#define _UI32_MAX     0xffffffffui32 /* maximum unsigned 32 bit value */

/* minimum signed 64 bit value */
#define _I64_MIN    (-9223372036854775807i64 - 1)
/* maximum signed 64 bit value */
#define _I64_MAX      9223372036854775807i64
/* maximum unsigned 64 bit value */
#define _UI64_MAX     0xffffffffffffffffui64

#if     _INTEGRAL_MAX_BITS >= 128 /*IFSTRIP=IGN*/
/* minimum signed 128 bit value */
#define _I128_MIN   (-170141183460469231731687303715884105727i128 - 1)
/* maximum signed 128 bit value */
#define _I128_MAX     170141183460469231731687303715884105727i128
/* maximum unsigned 128 bit value */
#define _UI128_MAX    0xffffffffffffffffffffffffffffffffui128
#endif

#ifndef SIZE_MAX
#ifdef _WIN64 
#define SIZE_MAX _UI64_MAX
#else
#define SIZE_MAX UINT_MAX
#endif
#endif

#if __STDC_WANT_SECURE_LIB__
#ifndef RSIZE_MAX
#define RSIZE_MAX    (SIZE_MAX >> 1)
#endif
#endif

#ifdef  _POSIX_

#define _POSIX_ARG_MAX      4096
#define _POSIX_CHILD_MAX    6
#define _POSIX_LINK_MAX     8
#define _POSIX_MAX_CANON    255
#define _POSIX_MAX_INPUT    255
#define _POSIX_NAME_MAX     14
#define _POSIX_NGROUPS_MAX  0
#define _POSIX_OPEN_MAX     16
#define _POSIX_PATH_MAX     255
#define _POSIX_PIPE_BUF     512
#define _POSIX_SSIZE_MAX    32767
#define _POSIX_STREAM_MAX   8
#define _POSIX_TZNAME_MAX   3

#define ARG_MAX             14500       /* 16k heap, minus overhead */
#define LINK_MAX            1024
#define MAX_CANON           _POSIX_MAX_CANON
#define MAX_INPUT           _POSIX_MAX_INPUT
#define NAME_MAX            255
#define NGROUPS_MAX         16
#define OPEN_MAX            32
#define PATH_MAX            512
#define PIPE_BUF            _POSIX_PIPE_BUF
#define SSIZE_MAX           _POSIX_SSIZE_MAX
#define STREAM_MAX          20
#define TZNAME_MAX          10

#endif  /* POSIX */

#endif  /* _INC_LIMITS */
