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
*stdarg.h - defines ANSI-style macros for variable argument functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file defines ANSI-style macros for accessing arguments
*       of functions which take a variable number of arguments.
*       [ANSI]
*
*       [Public]
*
*Revision History:
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       08-15-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       01-05-90  JCR   Added NULL definition
*       03-02-90  GJF   Added #ifndef _INC_STDARG stuff. Also, removed some
*                       (now) useless preprocessor directives.
*       05-29-90  GJF   Replaced sizeof() with _INTSIZEOF() and revised the
*                       va_arg() macro (fixes PTM 60)
*       05-31-90  GJF   Revised va_end() macro (propagated 5-25-90 change to
*                       crt7 version by WAJ)
*       10-30-90  GJF   Moved the real definitions into cruntime.h (for NT
*                       folks) and relinc.sed (to release ANSI compatible
*                       version). Ugly compromise.
*       08-20-91  JCR   C++ and ANSI naming
*       11-01-91  GDP   MIPS Compiler support. Moved real definitions back here
*       10-16-92  SKS   Replaced "#ifdef i386" with "#ifdef _M_IX86".
*       11-03-92  GJF   Fixed several conditionals, dropped _DOSX32_ support.
*       01-03-93  SRW   Fold in ALPHA changes
*       01-09-93  SRW   Remove usage of MIPS and ALPHA to conform to ANSI
*                       Use _MIPS_ and _ALPHA_ instead.
*       01-25-93  GJF   Fix va_list definition.
*       10-04-93  SRW   Fix ifdefs for MIPS and ALPHA to only check for _M_?????? defines
*       10-12-93  GJF   Merged NT version into Cuda version. Also, replaced
*                       _ALPHA_ with _M_ALPHA.
*       11-11-93  GJF   Minor cosmetic changes.
*       04-05-94  SKS   Add prototype of __builtin_va_start for ALPHA
*       10-02-94  BWT   Add PPC support.
*       11-03-94  GJF   Ensure 8 byte alignment.
*       12-28-94  JCF   Merged with mac header.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       12-14-95  JWM   Add "#pragma once".
*       02-24-97  GJF   Detab-ed.
*       11-07-97  RDL   Soft23 definitions.
*       02-06-98  GJF   Changes for Win64: fixed _APALIGN() macro (fix from
*                       Intel)
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       10-25-99  PML   Add support for _M_CEE (VS7#54572).
*       01-20-00  PML   Remove __epcg__.
*       09-07-00  PML   Remove va_list definition for _M_CEE (vs7#159777)
*       02-05-01  PML   Fix va_start for classes with operator& (vs7#201535)
*       03-26-01  GB    Added va_args for AMD64
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       08-28-02  AL    added va_list definition for managed varargs
*       10-25-02  SJ    Fixed Previous Checkin's C++ Style Comments
*       10-10-04  ESC   Use _CRT_PACKING
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*
****/

#pragma once

#ifndef _INC_STDARG
#define _INC_STDARG

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

#include <vadefs.h>

#define va_start _crt_va_start
#define va_arg _crt_va_arg
#define va_end _crt_va_end

#endif  /* _INC_STDARG */
