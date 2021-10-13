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
#include "snsDebug.h"


//------------------------------------------------------------------------------
// LogHelper
//
void LogHelper(BOOL bDbgOn, LPCTSTR szPrepend, LPCTSTR szFormat, ...)
{
    TCHAR   szBuffer[1024];
    va_list pArgs;
    va_start(pArgs, szFormat);
    StringCchVPrintfEx(szBuffer,_countof(szBuffer),NULL,NULL,STRSAFE_IGNORE_NULLS, szFormat, pArgs);
    RETAILMSG(bDbgOn,(_T("%s %s" ), szPrepend, szBuffer));
    va_end(pArgs);
}//LogHelper


void LogHelperEx(BOOL bDbgOn, LPCTSTR szPrepend, LPCTSTR szLabel, LPCTSTR szFormat, ...)
{
    TCHAR   szBuffer[1024];
    va_list pArgs;
    va_start(pArgs, szFormat);
    StringCchVPrintfEx(szBuffer,_countof(szBuffer),NULL,NULL,STRSAFE_IGNORE_NULLS, szFormat, pArgs);
    RETAILMSG(bDbgOn,(_T("%s %s| %s" ), szPrepend, szLabel, szBuffer));
    va_end(pArgs);
}//LogHelperEx

