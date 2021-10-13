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
*awint.h - internal definitions for A&W Win32 wrapper routines.
*
*
*Purpose:
*       Contains internal definitions/declarations for A&W wrapper functions.
*       Not included in internal.h since windows.h is required for these.
*
*       [Internal]
*
****/

#if _MSC_VER > 1000
#pragma once
#endif

#ifdef _WIN32

#ifndef _INC_AWINC
#define _INC_AWINC

#ifndef _CRTBLD
#ifndef WINCEINTERNAL
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* WINCEINTERNAL */
#endif /* _CRTBLD */

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>

/* internal A&W routines */

int __cdecl __crtCompareStringW(LCID, DWORD, LPCWSTR, int, LPCWSTR, int, int);
int __cdecl __crtCompareStringA(LCID, DWORD, LPCSTR, int, LPCSTR, int, int);

int __cdecl __crtGetLocaleInfoW(LCID, LCTYPE, LPWSTR, int, int);
int __cdecl __crtGetLocaleInfoA(LCID, LCTYPE, LPSTR, int, int);
 
int __cdecl __crtLCMapStringW(LCID, DWORD, LPCWSTR, int, LPWSTR, int, int);
int __cdecl __crtLCMapStringA(LCID, DWORD, LPCSTR, int, LPSTR, int, int, BOOL);

BOOL __cdecl __crtGetStringTypeW(DWORD, LPCWSTR, int, LPWORD, int, int);
BOOL __cdecl __crtGetStringTypeA(DWORD, LPCSTR, int, LPWORD, int, int, BOOL);

LPVOID __cdecl __crtGetEnvironmentStringsW(VOID);
LPVOID __cdecl __crtGetEnvironmentStringsA(VOID);

LPWSTR __cdecl __crtGetCommandLineW(VOID);

int __cdecl __crtMessageBoxA(LPCSTR, LPCSTR, UINT);

#ifdef __cplusplus
}
#endif

#endif /* _INC_AWINC */

#endif /* _WIN32 */
