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
*varargs.h - XENIX style macros for variable argument functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file defines XENIX style macros for accessing arguments of a
*       function which takes a variable number of arguments.
*       [System V]
*
*       [Public]
*
*Revision History:
*       08-22-88  GJF   Modified to also work for the 386 (small model only)
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       08-15-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       01-05-90  JCR   Added NULL definition
*       03-02-90  GJF   Added #ifndef _INC_VARARGS stuff. Also, removed some
*                       (now) useless preprocessor directives.
*       05-29-90  GJF   Replaced sizeof() with _INTSIZEOF() and revised the
*                       va_arg() macro (fixes PTM 60)
*       05-31-90  GJF   Revised va_end() macro (propagated 5-25-90 change to
*                       crt7 version by WAJ)
*       01-24-91  JCR   Generate an error on ANSI compilations
*       08-20-91  JCR   C++ and ANSI naming
*       11-01-91  GDP   MIPS Compiler support
*       10-16-92  SKS   Replaced "#ifdef i386" with "#ifdef _M_IX86".
*       11-03-92  GJF   Fixed several conditionals, dropped _DOSX32_ support.
*       01-09-93  SRW   Remove usage of MIPS and ALPHA to conform to ANSI
*                       Use _MIPS_ and _ALPHA_ instead.
*       10-04-93  SRW   Fix ifdefs for MIPS and ALPHA to only check for _M_?????? defines
*       10-13-93  GJF   Merged NT and Cuda versions.
*       04-05-94  SKS   Add prototype of __builtin_va_start for ALPHA
*       10-02-94  BWT   PPC merge
*       11-03-94  GJF   Ensure 8 byte alignment.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       12-14-95  JWM   Add "#pragma once".
*       02-24-97  GJF   Detab-ed.
*       11-07-97  RDL   Soft23 varargs.
*       05-17-99  PML   Remove all Macintosh support.
*       10-25-99  PML   Add support for _M_CEE (VS7#54572).
*       01-20-00  PML   Remove __epcg__.
*       03-26-01  GB    Added va_args for AMD64
*       03-26-01  GB    Added va_args for AMD64
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       10-10-04  ESC   Use _CRT_PACKING
*       03-08-05  PAL   Include crtdefs.h to define _CRT_PACKING.
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*
****/

#pragma once

#ifndef _INC_VARARGS
#define _INC_VARARGS

#if     !defined(_WIN32)
#error ERROR: Only Win32 target supported!
#endif

#ifndef _CRTBLD
/* This version of the header files is NOT for user programs.
 * It is intended for use when building the C runtimes ONLY.
 * The version intended for public use will not have this message.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#include <crtdefs.h>

/*
 * Currently, all MS C compilers for Win32 platforms default to 8 byte
 * alignment.
 */
#pragma pack(push,_CRT_PACKING)

#ifdef  __cplusplus
extern "C" {
#endif

#if     __STDC__
#error varargs.h incompatible with ANSI (use stdarg.h)
#endif

#if !defined(_W64)
#if !defined(__midl) && (defined(_X86_) || defined(_M_IX86)) /*IFSTRIP=IGN*/
#define _W64 __w64
#else
#define _W64
#endif
#endif

#ifndef _UINTPTR_T_DEFINED
#ifdef  _WIN64
typedef unsigned __int64    uintptr_t;
#else
typedef _W64 unsigned int   uintptr_t;
#endif
#define _UINTPTR_T_DEFINED
#endif

#ifndef _VA_LIST_DEFINED
#ifdef _M_CEE_PURE
typedef System::ArgIterator va_list;
#else
typedef char *  va_list;
#endif
#define _VA_LIST_DEFINED
#endif

#ifndef va_arg

#if     defined(_M_CEE)

#error varargs.h not supported when targetting _M_CEE (use stdarg.h)

#elif   defined(_M_IX86)

/*
 * define a macro to compute the size of a type, variable or expression,
 * rounded up to the nearest multiple of sizeof(int). This number is its
 * size as function argument (Intel architecture). Note that the macro
 * depends on sizeof(int) being a power of 2!
 */
#define _INTSIZEOF(n)    ( (sizeof(n) + sizeof(int) - 1) & ~(sizeof(int) - 1) )

#define va_dcl va_list va_alist;
#define va_start(ap) ap = (va_list)&va_alist
#define va_arg(ap,t)    ( *(t *)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)) )
#define va_end(ap) ap = (va_list)0

#elif defined(_M_ARM)

#define _VA_ALIGN       4
#define _APALIGN(t,ap)  ( ((va_list)0 - (ap)) & (__alignof(t) - 1) )
#define va_dcl          va_list va_alist;

#define _SLOTSIZEOF(t)  ( (sizeof(t) + _VA_ALIGN - 1) & ~(_VA_ALIGN - 1) )
#define va_start(ap)    ( ap = (va_list)&va_alist )
#define va_arg(ap,t)    (*(t *)((ap += _SLOTSIZEOF(t) + _APALIGN(t,ap)) \
                        - _SLOTSIZEOF(t)))
#define va_end(ap)      ( ap = (va_list)0 )

#elif defined(_M_X64)

extern void __cdecl __va_start(va_list *, ...);
#define va_dcl          va_list va_alist;
#define va_start(ap)    ( __va_start(&ap, 0) )
#define va_arg(ap, t)   \
    ( ( sizeof(t) > sizeof(__int64) || ( sizeof(t) & (sizeof(t) - 1) ) != 0 ) \
        ? **(t **)( ( ap += sizeof(__int64) ) - sizeof(__int64) ) \
        :  *(t  *)( ( ap += sizeof(__int64) ) - sizeof(__int64) ) )
#define va_end(ap)      ( ap = (va_list)0 )

#else

/* A guess at the proper definitions for other platforms */

#define _INTSIZEOF(n)    ( (sizeof(n) + sizeof(int) - 1) & ~(sizeof(int) - 1) )

#define va_dcl va_list va_alist;
#define va_start(ap) ap = (va_list)&va_alist
#define va_arg(ap,t) ( *(t *)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)) )
#define va_end(ap) ap = (va_list)0

#endif

#endif


#ifdef  __cplusplus
}
#endif

#pragma pack(pop)

#endif  /* _INC_VARARGS */
