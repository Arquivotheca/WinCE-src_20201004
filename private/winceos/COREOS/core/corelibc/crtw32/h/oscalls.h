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
*oscalls.h - contains declarations of Operating System types and constants.
*
*Purpose:
*       Declares types and constants that are defined by the target OS.
*
*       [Internal]
*
****/

#if     _MSC_VER > 1000
#pragma once
#endif

#ifndef _INC_OSCALLS
#define _INC_OSCALLS

#ifndef _CRTBLD
#ifndef WINCEINTERNAL
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* WINCEINTERNAL */
#endif  /* _CRTBLD */

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef  _WIN32

#ifdef  NULL
#undef  NULL
#endif

#if     defined(_DEBUG) && defined(_WIN32)

void DbgBreakPoint(void);
int DbgPrint(char *Format, ...);

#endif  /* _DEBUG && _WIN32 */

#define NOMINMAX

#include <windows.h>

#undef  NULL
#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

/* File time and date types */

typedef struct _FTIME {         /* ftime */
    unsigned short twosecs : 5;
    unsigned short minutes : 6;
    unsigned short hours   : 5;
} FTIME;
typedef FTIME   *PFTIME;

typedef struct _FDATE {         /* fdate */
    unsigned short day     : 5;
    unsigned short month   : 4;
    unsigned short year    : 7;
} FDATE;
typedef FDATE   *PFDATE;

#else   /* ndef _WIN32 */

#ifdef  _POSIX_

#undef  NULL
#ifdef  __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif

#include <windows.h>

#else   /* ndef _POSIX_ */

#ifdef  _MAC

#else   /* ndef _MAC */

#error ERROR - ONLY WIN32, POSIX, OR MAC TARGET SUPPORTED!

#endif  /* _POSIX_ */

#endif  /* _MAC */

#endif  /* _WIN32 */

#ifdef  __cplusplus
}
#endif

#endif  /* _INC_OSCALLS */
