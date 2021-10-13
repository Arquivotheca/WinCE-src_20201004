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
*dbgrptw.c - Debug CRT Reporting Functions
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*Revision History:
*       08-28-03  SJ   Module created.VSWhidbey#55308
*       04-05-04  AJS  Build file as C++; add unsigned short overload of _CrtDbgReportW
*
*******************************************************************************/

#ifdef _WIN32_WCE

#include <windows.h>
#include <stdlib.h>
#include <wchar.h>
#include <crtdbg.h>

#ifdef  __cplusplus

extern "C" _CRTIMP
int
__cdecl
_CrtDbgReportW(
    _In_ int reportType,
    _In_opt_z_ const wchar_t * szFileName,
    _In_ int lineNumber,
    _In_opt_z_ const wchar_t * szModuleName,
    _In_opt_z_ const wchar_t * szFormat,
    ...)
{

#ifdef  DEBUG
    wchar_t szLine[12];
    int bShouldBreak = 0;
    _snwprintf_s(szLine, _countof(szLine), _TRUNCATE, L"%d", lineNumber);

    const wchar_t* szReportType;
    wchar_t szType[12];
    switch (reportType)
    {
        case _CRT_WARN:
            szReportType = L"warning";
            break;
        case _CRT_ERROR:
            szReportType = L"error";
            break;
        case _CRT_ASSERT:
            szReportType = L"assertion failure";
            bShouldBreak = 1;
            break;
        default:
            _snwprintf_s(szType, _countof(szType), _TRUNCATE, L"%d", reportType);
            szReportType = szType;
            break;
    }

    wchar_t szBuffer[1024];
    _snwprintf_s(
        szBuffer,
        _countof(szBuffer),
        _TRUNCATE,
        L"CrtDbgReport %s%s%s%s%s%s%s%s%s: ",
        szReportType,
        szFileName ? L" in " : L"",
        szFileName ? szFileName : L"",
        lineNumber ? L" (" : L"",
        lineNumber ? szLine : L"",
        lineNumber ? L")" : L"",
        szModuleName ? L" [" : L"",
        szModuleName ? szModuleName : L"",
        szModuleName ? L"]" : L"");

    OutputDebugStringW(szBuffer);

    if (szFormat)
    {
        va_list args;
        va_start(args, szFormat);
        _vsnwprintf_s(szBuffer, _countof(szBuffer), _TRUNCATE, szFormat, args);
        OutputDebugStringW(szBuffer);
        va_end(args);
    }

    OutputDebugStringW(L"\n");
    if (bShouldBreak)
    {
        // break
        // The _DEBUG code is not enable so use __debugbreak instead of _CrtDbgBreak();
        __debugbreak();
    }

    return 0;
#else
    return -1;
#endif /* DEBUG */

}

#endif /* __cplusplus */

#else

#ifndef _POSIX_

#ifndef _UNICODE   /* CRT flag */
#define _UNICODE 1
#endif

#ifndef UNICODE	   /* NT flag */
#define UNICODE 1
#endif

#include "dbgrpt.c"

#ifdef  _DEBUG

#ifdef  __cplusplus

#if defined(_NATIVE_WCHAR_T_DEFINED)

extern "C++"

_CRTIMP int __cdecl _CrtDbgReportW(
        int nRptType, 
        const unsigned short * szFile, 
        int nLine,
        const unsigned short * szModule,
        const unsigned short * szFormat, 
        ...
        )
{
    int retval;
    va_list arglist;

    va_start(arglist,szFormat);
    
    retval = _CrtDbgReportTV(nRptType, _ReturnAddress(), reinterpret_cast<const wchar_t*>(szFile), nLine, reinterpret_cast<const wchar_t*>(szModule), reinterpret_cast<const wchar_t*>(szFormat), arglist);

    va_end(arglist);

    return retval;
}

#endif /* defined(_NATIVE_WCHAR_T_DEFINED) */

#endif /* __cplusplus */

#endif  /* _DEBUG */

#endif /* _POSIX_ */
#endif /* _WIN32_WCE */
