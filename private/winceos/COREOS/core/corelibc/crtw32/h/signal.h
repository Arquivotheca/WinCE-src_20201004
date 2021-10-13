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
*signal.h - defines signal values and routines
*
*Purpose:
*       This file defines the signal values and declares the signal functions.
*       [ANSI/System V]
*
*       [Public]
*
****/

#if     _MSC_VER > 1000
#pragma once
#endif

#ifndef _INC_SIGNAL
#define _INC_SIGNAL

#if     !defined(_WIN32)
#error ERROR: Only Win32 target supported!
#endif

/* This version of the header files is NOT for user programs.
 * It is intended for use when building the C runtimes ONLY.
 * The version intended for public use will not have this message.
 */

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef _INTERNAL_IFSTRIP_
#include <cruntime.h>
#endif  /* _INTERNAL_IFSTRIP_ */


/* Define _CRTIMP */

#ifndef _CRTIMP
#ifdef  CRTDLL
#define _CRTIMP __declspec(dllexport)
#else   /* ndef CRTDLL */
#ifdef  _DLL
#define _CRTIMP __declspec(dllimport)
#else   /* ndef _DLL */
#define _CRTIMP
#endif  /* _DLL */
#endif  /* CRTDLL */
#endif  /* _CRTIMP */

/* Define __cdecl for non-Microsoft compilers */

#if     ( !defined(_MSC_VER) && !defined(__cdecl) )
#define __cdecl
#endif

#ifndef _SIG_ATOMIC_T_DEFINED
typedef int sig_atomic_t;
#define _SIG_ATOMIC_T_DEFINED
#endif

#define NSIG 23     /* maximum signal number + 1 */


/* Signal types */

#define SIGINT          2       /* interrupt */
#define SIGILL          4       /* illegal instruction - invalid function image */
#define SIGFPE          8       /* floating point exception */
#define SIGSEGV         11      /* segment violation */
#define SIGTERM         15      /* Software termination signal from kill */
#define SIGBREAK        21      /* Ctrl-Break sequence */
#define SIGABRT         22      /* abnormal termination triggered by abort call */


/* signal action codes */

#define SIG_DFL (void (__cdecl *)(int))0           /* default signal action */
#define SIG_IGN (void (__cdecl *)(int))1           /* ignore signal */
#define SIG_SGE (void (__cdecl *)(int))3           /* signal gets error */
#define SIG_ACK (void (__cdecl *)(int))4           /* acknowledge */

#ifndef _INTERNAL_IFSTRIP_
/* internal use only! not valid as an argument to signal() */

#define SIG_GET (void (__cdecl *)(int))2           /* accept signal */
#define SIG_DIE (void (__cdecl *)(int))5           /* terminate process */
#endif

/* signal error value (returned by signal call on error) */

#define SIG_ERR (void (__cdecl *)(int))-1          /* signal error value */


/* pointer to exception information pointers structure */

#if 0 //    defined(_MT) || defined(_DLL)
extern void * * __cdecl __pxcptinfoptrs(void);
#define _pxcptinfoptrs  (*__pxcptinfoptrs())
#else   /* ndef _MT && ndef _DLL */
extern void * _pxcptinfoptrs;
#endif  /* _MT || _DLL */


/* Function prototypes */

_CRTIMP void (__cdecl * __cdecl signal(int, void (__cdecl *)(int)))(int);
_CRTIMP int __cdecl raise(int);


#ifdef  __cplusplus
}
#endif

#endif  /* _INC_SIGNAL */
