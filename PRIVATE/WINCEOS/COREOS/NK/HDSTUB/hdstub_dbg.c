//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++

Module Name:

    Hdstub_dbg.c

Abstract:

    Debugging functions for hdstub.

--*/

#include "hdstub_p.h"

#define DBG_PRINTF_BUFSIZE 512

DBGPARAM dpCurSettings =
{
    L"HdStub",
    {
        L"Init",     // 0x0001
        L"Entry",    // 0x0002
        L"Client",   // 0x0004
        L"Hardware", // 0x0008
        L"CritSec",  // 0x0010
        L" ",        // 0x0020
        L" ",        // 0x0040
        L" ",        // 0x0080
        L" ",        // 0x0100
        L" ",        // 0x0200
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

static WCHAR wszDbgPrintfBuf[DBG_PRINTF_BUFSIZE];

static void WideToSingleInPlace(LPWSTR wsz)
{
    LPSTR sz = (LPSTR)wsz;
    while (*wsz)
    {
        *sz = (char) *wsz;
#pragma prefast (suppress:394, "sz is always < wsz")
        ++sz;
        ++wsz;
    }
    *sz = 0;
}

void HdstubDbgPrintf(LPCWSTR wszFmt, ...)
{
    va_list varargs;

    if (g_pfnNKvsprintfW)
    {
        BOOL fIntSave = g_HdStubData.pfnINTERRUPTS_ENABLE(FALSE); 
        
        va_start(varargs, wszFmt);
        g_pfnNKvsprintfW(wszDbgPrintfBuf, wszFmt, varargs, DBG_PRINTF_BUFSIZE);
        va_end(varargs);

        if (g_pfnOutputDebugString)
        {         
            WideToSingleInPlace(wszDbgPrintfBuf);
            g_pfnOutputDebugString((char *)wszDbgPrintfBuf);
        }
        
        g_HdStubData.pfnINTERRUPTS_ENABLE(fIntSave); 
    }
}
