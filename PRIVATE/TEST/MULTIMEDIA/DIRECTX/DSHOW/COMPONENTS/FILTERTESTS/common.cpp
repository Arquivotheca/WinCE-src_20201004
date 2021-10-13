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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <windows.h>
#include "common.h"

extern CKato  *aLog;

////////////////////////////////////////////////////////////////////////////////
// Debug
//  Printf-like wrapping around OutputDebugString.


//
// Parameters:
//  szFormat        Formatting string (as in printf).
//  ...             Additional arguments.
//
// Return value:
//  None.

void Debug(LPCTSTR szFormat, ...)
{
    static  TCHAR   szHeader[] = TEXT("DShowComp: ");
    TCHAR   szBuffer[1024];

    va_list pArgs;
    va_start(pArgs, szFormat);
    lstrcpy(szBuffer, szHeader);
    wvsprintf(
        szBuffer + countof(szHeader) - 1,
        szFormat,
        pArgs);
    va_end(pArgs);

    _tcscat(szBuffer, TEXT("\r\n"));

    OutputDebugString(szBuffer);
}

void LOG(LPWSTR szFmt, ...)
{
    va_list va;

    if(aLog)
    {
        va_start(va, szFmt);
        aLog->LogV( 0, /*LOG_DETAIL,*/ szFmt, va);
        va_end(va);
    }
}
