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
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains internal definitions/declarations for A&W wrapper functions.
*       Not included in internal.h since windows.h is required for these.
*
*       [Internal]
*
*Revision History:
*       03-30-94  CFW   Module created.
*       04-18-94  CFW   Add lcid parameter.
*       02-14-95  CFW   Clean up Mac merge.
*       02-24-95  CFW   Add _crtMessageBox.
*       02-27-95  CFW   Change __crtMessageBoxA params.
*       03-29-95  CFW   Add error message to internal headers.
*       05-26-95  GJF   Changed prototype for __crtGetEnvironmentStringsA.
*       12-14-95  JWM   Add "#pragma once".
*       03-16-97  RDK   Added error flag to __crtGetStringTypeA.
*       03-17-97  RDK   Added error flag to __crtLCMapStringA.
*       08-22-00  GB    Added __ansicp and __convertcp
*       08-28-03  SJ    Added _wassert, CrtSetReportHookW2,CrtDbgReportW, 
*                       & other helper functions. VSWhidbey#55308
*       09-24-04  MSL   Param names
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       01-21-05  MSL   SAL Annotate _mt variants of functions
*       03-23-05  MSL   _P removal - not needed
*
****/

#pragma once

#ifdef _WIN32

#ifndef _INC_AWINC
#define _INC_AWINC

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif /* _CRTBLD */

#ifdef __cplusplus
extern "C" {
#endif

#include <sal.h>
#include <windows.h>

/* internal A&W routines */
struct  threadlocaleinfostruct;
typedef struct threadlocaleinfostruct * pthreadlocinfo;

// Fast fail error codes
#define FAST_FAIL_VTGUARD_CHECK_FAILURE       1
#define FAST_FAIL_STACK_COOKIE_CHECK_FAILURE  2
#define FAST_FAIL_CORRUPT_LIST_ENTRY          3
#define FAST_FAIL_INCORRECT_STACK             4
#define FAST_FAIL_INVALID_ARG                 5
#define FAST_FAIL_GS_COOKIE_INIT              6
#define FAST_FAIL_FATAL_APP_EXIT              7
#define FAST_FAIL_RANGE_CHECK_FAILURE         8

// Remove when winnt.h has the definition
#ifndef PF_FASTFAIL_AVAILABLE
#define PF_FASTFAIL_AVAILABLE                23
#endif

int __cdecl __crtCompareStringW
(
    _In_ LPCWSTR _LocaleName, 
    _In_ DWORD	  _DwCmpFlags,
    _In_reads_(_CchCount1) LPCWSTR  _LpString1,
    _In_ int      _CchCount1,
    _In_reads_(_CchCount2) LPCWSTR  _LpString2,
    _In_ int      _CchCount2
);

int __cdecl __crtCompareStringA
(
    _In_opt_ _locale_t _Plocinfo,
    _In_ LPCWSTR _LocaleName, 
    _In_ DWORD    _DwCmpFlags,
    _In_reads_(_CchCount1) LPCSTR   _LpString1,
    _In_ int      _CchCount1,
    _In_reads_(_CchCount2) LPCSTR   _LpString2,
    _In_ int      _CchCount2,
    _In_ int      _Code_page
);

int __cdecl __crtGetLocaleInfoA
(
    _In_opt_ _locale_t _Plocinfo,
    _In_ LPCWSTR _LocaleName,
    _In_ LCTYPE  _LCType,
    _Out_writes_opt_(_CchData) LPSTR   _LpLCData,
    _In_ int     _CchData
);
 
int __cdecl __crtLCMapStringW
(
    _In_ LPCWSTR _LocaleName, 
    _In_ DWORD _DWMapFlag, 
    _In_reads_(_CchSrc) LPCWSTR _LpSrcStr , 
    _In_ int _CchSrc, 
    _Out_writes_opt_(_CchDest) LPWSTR _LpDestStr, 
    _In_ int _CchDest
);

int __cdecl __crtLCMapStringA
(
    _In_opt_ _locale_t _Plocinfo, 
    _In_ LPCWSTR _LocaleName, 
	_In_ DWORD _DwMapFlag, 
    _In_reads_(_CchSrc) LPCSTR _LpSrcStr, 
    _In_ int _CchSrc, 
    _Out_writes_opt_(_CchDest) LPSTR _LpDestStr, 
    _In_ int _CchDest, 
    _In_ int _Code_page, 
    _In_ BOOL _BError
);

BOOL __cdecl __crtGetStringTypeA
(
    _In_opt_ _locale_t _Plocinfo, 
    _In_ DWORD _DWInfoType, 
    _In_ LPCSTR _LpSrcStr, 
    _In_ int _CchSrc, 
    _Out_ LPWORD _LpCharType, 
    _In_ int _Code_page, 
    _In_ BOOL _BError
);
#ifdef _WCE_BOOTCRT
int __cdecl __crtLCMapStringA2
(
    _In_opt_ _locale_t _Plocinfo, 
    _In_ LPCWSTR _LocaleName, 
	_In_ DWORD _DwMapFlag, 
    _In_reads_(_CchSrc) LPCSTR _LpSrcStr, 
    _In_ int _CchSrc, 
    _Out_writes_opt_(_CchDest) LPSTR _LpDestStr, 
    _In_ int _CchDest, 
    _In_ int _Code_page,
    _In_ LPWSTR _LpwTmpInBuf,
    _In_ LPWSTR _LpwTmpOutBuf,
    _In_ BOOL _BError
);

BOOL __cdecl __crtGetStringTypeA2
(
    _In_opt_ _locale_t _Plocinfo, 
    _In_ DWORD _DWInfoType, 
    _In_ LPCSTR _LpSrcStr, 
    _In_ int _CchSrc, 
    _Out_ LPWORD _LpCharType, 
    _In_ int _Code_page,
    _In_ LPWSTR _LpwTmpStr,
    _In_ BOOL _BError
);
#endif
LPVOID __cdecl __crtGetEnvironmentStringsA(VOID);
LPVOID __cdecl __crtGetEnvironmentStringsW(VOID);

int __cdecl __crtMessageBoxA
(
    _In_ LPCSTR _LpText, 
    _In_ LPCSTR _LpCaption, 
    _In_ UINT _UType
);

int __cdecl __crtMessageBoxW
(
    _In_ LPCWSTR _LpText, 
    _In_ LPCWSTR _LpCaption, 
    _In_ UINT _UType
);

/* Helper function for Packaged apps */
_CRTIMP BOOL __cdecl __crtIsPackagedApp(void);

WORD __cdecl __crtGetShowWindowMode(void);

/* typedef from errhandlingapi.h */
typedef LONG (WINAPI *PTOP_LEVEL_EXCEPTION_FILTER)(
    __in struct _EXCEPTION_POINTERS *ExceptionInfo
    );
typedef PTOP_LEVEL_EXCEPTION_FILTER LPTOP_LEVEL_EXCEPTION_FILTER;

void __cdecl __crtSetUnhandledExceptionFilter
(
    _In_ LPTOP_LEVEL_EXCEPTION_FILTER exceptionFilter
);

#if defined(_M_IX86) || defined(_M_X64)

LONG __cdecl __crtUnhandledException 
(
    _In_ EXCEPTION_POINTERS *exceptionInfo
);

void __cdecl __crtTerminateProcess
(
    _In_ UINT uExitCode
);
#endif /* defined(_M_IX86) || defined(_M_X64) */

#if defined(_M_X64)
void __cdecl __crtCaptureCurrentContext 
(
    _Out_ CONTEXT *pContextRecord
);

void __cdecl __crtCapturePreviousContext 
(
    _Out_ CONTEXT *pContextRecord
);
#endif /* defined(_M_X64) */

#ifndef _WIN32_WCE

/* Helper functions for thread-level storage Win32 APIs */
DWORD __crtFlsAlloc(
  __in  PFLS_CALLBACK_FUNCTION lpCallback
);

BOOL __crtFlsFree(
  __in  DWORD dwFlsIndex
);

PVOID __crtFlsGetValue(
  __in  DWORD dwFlsIndex
);

BOOL __crtFlsSetValue(
  __in      DWORD dwFlsIndex,
  __in_opt  PVOID lpFlsData
);

#endif  


/* Helper functions for NLS-specific Win32 APIs */
int __cdecl __crtCompareStringEx(
  __in_opt  LPCWSTR lpLocaleName,
  __in      DWORD dwCmpFlags,
  __in      LPCWSTR lpString1,
  __in      int cchCount1,
  __in      LPCWSTR lpString2,
  __in      int cchCount2);

BOOL __cdecl __crtEnumSystemLocalesEx(
  __in      LOCALE_ENUMPROCEX lpLocaleEnumProcEx,
  __in      DWORD dwFlags,
  __in      LPARAM lParam);

int __cdecl __crtGetDateFormatEx(
  __in_opt   LPCWSTR lpLocaleName,
  __in       DWORD dwFlags,
  __in_opt   const SYSTEMTIME *lpDate,
  __in_opt   LPCWSTR lpFormat,
  __out_opt  LPWSTR lpDateStr,
  __in       int cchDate);

int  __cdecl __crtGetLocaleInfoEx(
  __in_opt   LPCWSTR lpLocaleName,
  __in       LCTYPE LCType,
  __out_opt  LPWSTR lpLCData,
  __in       int cchData);

int  __cdecl __crtGetTimeFormatEx(
  __in_opt   LPCWSTR lpLocaleName,
  __in       DWORD dwFlags,
  __in_opt   const SYSTEMTIME *lpTime,
  __in_opt   LPCWSTR lpFormat,
  __out_opt  LPWSTR lpTimeStr,
  __in       int cchTime);

int  __cdecl __crtGetUserDefaultLocaleName(
  __out  LPWSTR lpLocaleName,
  __in   int cchLocaleName);

BOOL __cdecl __crtIsValidLocaleName(
     __in  LPCWSTR lpLocaleName);

int __cdecl __crtLCMapStringEx(
  __in_opt   LPCWSTR lpLocaleName,
  __in       DWORD dwMapFlags,
  __in       LPCWSTR lpSrcStr,
  __in       int cchSrc,
  __out_opt  LPWSTR lpDestStr,
  __in       int cchDest);

#ifdef __cplusplus
}
#endif

#endif /* _INC_AWINC */

#endif /* _WIN32 */
