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
/*++

Module Name:

    Hdstub_dbg.c

Abstract:

    Debugging functions for hdstub.

--*/

#include "hdstub_p.h"

#define MAX_DEBUG_LEN 128

DBGPARAM dpCurSettings =
{
    L"HdStub",
    {
        L"Init",     // 0x0001
        L"Entry",    // 0x0002
        L"Client",   // 0x0004
        L"Hardware", // 0x0008
        L"CritSec",  // 0x0010
        L"Move",     // 0x0020
        L"BP",       // 0x0040
        L"VM",       // 0x0080
        L"DBP ",     // 0x0100
        L"Unwind",   // 0x0200
        L" ",        // 0x0400
        L" ",        // 0x0800
        L" ",        // 0x1000
        L" ",        // 0x2000
        L" ",        // 0x4000
        L"Alert"     // 0x8000
    },
    HDZONE_DEFAULT
};


#ifndef _PREFAST_
#pragma warning(disable:4068)
#endif

void HdstubDbgPrintf(LPCWSTR wszFmt, ...)
{
    WCHAR szBuf[MAX_DEBUG_LEN];
    va_list varargs;

    va_start(varargs, wszFmt);
    NKwvsprintfW(szBuf, wszFmt, varargs, sizeof(szBuf)/sizeof(szBuf[0]));
    va_end(varargs);

    AcquireKitlSpinLock();
    ((OEMGLOBAL*)(g_pKData->pOem))->pfnWriteDebugString(szBuf);
    ReleaseKitlSpinLock();
}
