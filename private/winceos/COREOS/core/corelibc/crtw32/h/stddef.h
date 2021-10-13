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
*stddef.h - definitions/declarations for common constants, types, variables
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains definitions and declarations for some commonly
*       used constants, types, and variables.
*       [ANSI]
*
*       [Public]
*
*Revision History:
*       10-02-87  JCR   Changed NULL definition #else to #elif (C || L || H)
*       12-11-87  JCR   Added "_loadds" functionality
*       12-16-87  JCR   Added threadid definition
*       12-18-87  JCR   Added _FAR_ to declarations
*       02-10-88  JCR   Cleaned up white space
*       08-19-88  GJF   Revised to also work for the 386
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       06-06-89  JCR   386: Made _threadid a function
*       08-01-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat
*                       model). Also added parens to *_errno definition
*                       (same as 11-14-88 change to CRT version).
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       03-02-90  GJF   Added #ifndef _INC_STDDEF and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessor
*                       directives.
*       04-10-90  GJF   Replaced _cdecl with _VARTYPE1 or _CALLTYPE1, as
*                       appropriate.
*       08-16-90  SBM   Made MTHREAD _errno return int *
*       10-09-90  GJF   Changed return type of __threadid() to unsigned long *.
*       11-12-90  GJF   Changed NULL to (void *)0.
*       02-11-91  GJF   Added offsetof() macro.
*       02-12-91  GJF   Only #define NULL if it isn't #define-d.
*       03-21-91  KRS   Added wchar_t typedef, also in stdlib.h.
*       06-27-91  GJF   Revised __threadid, added __threadhandle, both
*                       for Win32 [_WIN32_].
*       08-20-91  JCR   C++ and ANSI naming
*       01-29-92  GJF   Got rid of macro defining _threadhandle to be
*                       __threadhandle (no reason for the former name to be
*                       be defined).
*       08-05-92  GJF   Function calling type and variable type macros.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*                       Remove support for OS/2, etc.
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*       10-12-93  GJF   Support NT and Cuda versions. Also, replace MTHREAD
*                       with _MT.
*       03-14-94  GJF   Made declaration of errno match one in stdlib.h.
*       06-06-94  SKS   Change if def(_MT) to if def(_MT) || def(_DLL)
*                       This will support single-thread apps using MSVCRT*.DLL
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       04-03-95  JCF   Remove #ifdef _WIN32 around wchar_t.
*       12-14-95  JWM   Add "#pragma once".
*       02-05-97  GJF   Cleaned out obsolete support for _CRTAPI* and _NTSDK.
*                       Also, detab-ed
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       01-30-98  GJF   Added appropriate defs for ptrdiff_t and offsetof
*       02-03-98  GJF   Changes for Win64: added int_ptr and uint_ptr.
*       12-15-98  GJF   Changes for 64-bit size_t.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       03-12-03  SSM   C++ version of offset macro handles types that have
*                       operator&
*       06-13-03  SJ    Added volatile to offsetof macro - VSWhidbey#139039
*       12-09-03  SJ    VSWhidbey#210596 - Remove dllimport for _STATIC_CPPLIB
*       04-07-04  MSL   Added errno_t
*       04-21-04  AC    Removed some definitions already in crtdefs.h
*       04-14-05  MSL   Compile clean under MIDL
*       05-27-05  MSL   64 bit definition of NULL
*       06-03-05  MSL   Revert 64 bit definition of NULL due to calling-convention fix
*
****/

#pragma once

#ifndef _INC_STDDEF
#define _INC_STDDEF

#include <crtdefs.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* Define NULL pointer value */
#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#ifdef __cplusplus
namespace std { typedef decltype(__nullptr) nullptr_t; }
using ::std::nullptr_t;
#endif

/* Declare reference to errno */
#ifndef _CRT_ERRNO_DEFINED
#define _CRT_ERRNO_DEFINED
_CRTIMP extern int * __cdecl _errno(void);
#define errno   (*_errno())

errno_t __cdecl _set_errno(_In_ int _Value);
errno_t __cdecl _get_errno(_Out_ int * _Value);
#endif

/* Define offsetof macro */
#ifndef offsetof // CE OS defines it within winnt.h
#ifdef __cplusplus

#ifdef  _WIN64
#define offsetof(s,m)   (size_t)( (ptrdiff_t)&reinterpret_cast<const volatile char&>((((s *)0)->m)) )
#else
#define offsetof(s,m)   (size_t)&reinterpret_cast<const volatile char&>((((s *)0)->m))
#endif

#else

#ifdef  _WIN64
#define offsetof(s,m)   (size_t)( (ptrdiff_t)&(((s *)0)->m) )
#else
#define offsetof(s,m)   (size_t)&(((s *)0)->m)
#endif

#endif	/* __cplusplus */
#endif

_CRTIMP extern unsigned long  __cdecl __threadid(void);
#define _threadid       (__threadid())
_CRTIMP extern uintptr_t __cdecl __threadhandle(void);

#ifdef  __cplusplus
}
#endif

#endif  /* _INC_STDDEF */
