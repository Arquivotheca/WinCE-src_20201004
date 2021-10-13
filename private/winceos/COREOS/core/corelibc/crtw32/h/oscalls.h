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
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Declares types and constants that are defined by the target OS.
*
*       [Internal]
*
*Revision History:
*       12-01-90  SRW   Module created
*       02-01-91  SRW   Removed usage of NT header files (_WIN32)
*       02-28-91  SRW   Removed usage of ntconapi.h (_WIN32)
*       04-09-91  PNT   Added _MAC_ definitions
*       04-26-91  SRW   Disable min/max definitions in windows.h and added debug
*                       definitions for DbgPrint and DbgBreakPoint(_WIN32)
*       08-05-91  GJF   Use win32.h instead of windows.h for now.
*       08-20-91  JCR   C++ and ANSI naming
*       09-12-91  GJF   Go back to using windows.h for win32 build.
*       09-26-91  GJF   Don't use error.h for Win32.
*       11-07-91  GJF   win32.h renamed to dosx32.h
*       11-08-91  GJF   Don't use windows.h, excpt.h. Add ntstatus.h.
*       12-13-91  GJF   Fixed so that exception stuff will build for Win32
*       02-04-92  GJF   Now must include ntdef.h to get LPSTR type.
*       02-07-92  GJF   Backed out change above, LPSTR also got added to
*                       winnt.h
*       03-30-92  DJM   POSIX support.
*       04-06-92  SRW   Backed out 11-08-91 change and went back to using
*                       windows.h only.
*       05-12-92  DJM   Moved POSIX code to it's own ifdef.
*       08-01-92  SRW   Let windows.h include excpt.h now that it replaces winxcpt.h
*       09-30-92  SRW   Use windows.h for _POSIX_ as well
*       02-23-93  SKS   Update copyright to 1993
*       09-06-94  CFW   Remove Cruiser support.
*       02-06-95  CFW   DEBUG -> _DEBUG
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       12-14-95  JWM   Add "#pragma once".
*       02-24-97  GJF   Replaced defined(_M_M68K) || defined(_M_MPPC) with
*                       defined(_MAC). Also, detab-ed.
*       05-17-99  PML   Remove all Macintosh support.
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-29-05  SSM   Define _WIN32_FUSION to allow using Activation context
*                       API to load msvcm* from Pure Managed startup code.
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       05-27-05  MSL   64 bit definition of NULL
*       06-03-05  MSL   Revert 64 bit definition of NULL due to calling-convention fix
*
****/

#pragma once

#ifndef _INC_OSCALLS
#define _INC_OSCALLS

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#include <crtdefs.h>

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef  _WIN32

#ifdef  NULL
#undef  NULL
#endif

#if     defined(_DEBUG) && defined(_WIN32)

void DbgBreakPoint(void);
int DbgPrint(_In_z_ _Printf_format_string_ char *_Format, ...);

#endif  /* _DEBUG && _WIN32 */

#define NOMINMAX

#define _WIN32_FUSION 0x0100
#include <windows.h>

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

#error ERROR - ONLY WIN32 OR POSIX TARGETS SUPPORTED!

#endif  /* _POSIX_ */

#endif  /* _WIN32 */

#ifdef  __cplusplus
}
#endif

#endif  /* _INC_OSCALLS */
