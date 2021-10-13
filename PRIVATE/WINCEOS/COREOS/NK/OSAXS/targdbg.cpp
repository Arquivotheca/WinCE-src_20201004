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

    targdbg.cpp

Module Description:

    Target routines to support debugging.  Basically a hack upon Debugging
    zones to print messages out the serial port.

--*/
#include "osaxs_p.h"

#define DBG_PRINTF_BUFSIZE 512

DBGPARAM dpCurSettings =
{
    L"OsaxsT0",
    {

        L"Init",            // 0x1
        L"PSL Api",         // 0x2
        L"FlexAPI",         // 0x4
        L"Memory",          // 0x8
        L"Pack Data",       // 0x10
        L"Debug CV Info",   // 0x20
        L"IOCTL",           // 0x40
        L"",                // 0x80
        L"",                // 0x100
        L"",                // 0x200
        L"",                // 0x400
        L"",                // 0x800
        L"",                // 0x1000
        L"",                // 0x2000
        L"Dump Generator",  // 0x4000
        L"Alert"            // 0x8000
    },
    OXZONE_DEFAULT
};

static WCHAR wszDbgPrintfBuf[DBG_PRINTF_BUFSIZE];

static void WideToSingleInPlace (LPWSTR wsz)
{
    char *sz = (char *)wsz;
    for ( ; *wsz; sz++, wsz ++)
    {
        *sz = (char) *wsz;
    }
    *sz = 0;
}

void DbgPrintf (LPCWSTR wszFmt, ...)
{
    va_list varargs;

    if (g_OsaxsData.pNKvsprintfW)
    {
        BOOL fIntSave = g_OsaxsData.pfnINTERRUPTS_ENABLE(FALSE); 

        va_start(varargs, wszFmt);
        g_OsaxsData.pNKvsprintfW(wszDbgPrintfBuf, wszFmt, varargs, DBG_PRINTF_BUFSIZE);
        va_end(varargs);

        if (g_pfnOutputDebugString)
        {         
            WideToSingleInPlace(wszDbgPrintfBuf);
            g_pfnOutputDebugString((char *)wszDbgPrintfBuf);
        }

        g_OsaxsData.pfnINTERRUPTS_ENABLE(fIntSave); 
    }
}
