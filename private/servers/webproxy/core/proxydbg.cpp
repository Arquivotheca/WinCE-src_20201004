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

#include "proxydbg.h"


// Debug functions
#ifdef DEBUG

DBGPARAM dpCurSettings =
{
    TEXT("WEBPROXY"),
 {
  TEXT("Output"),
  TEXT("Connections"),
  TEXT("Request"),
  TEXT("Response"),
  TEXT("Packets"),
  TEXT("Parser"),
  TEXT("Authentication"),
  TEXT("Service"),
  TEXT("Session"),
  TEXT("Filter"),
  TEXT("Undefined"),
  TEXT("Undefined"),
  TEXT("Undefined"),
  TEXT("Undefined"),
  TEXT("Warning"),
  TEXT("Error")
 },

    0x0000C000
};

CRITICAL_SECTION g_csdebug;

void DebugInit(void)
{
    InitializeCriticalSection(&g_csdebug);
}

void DebugDeinit(void)
{
    DeleteCriticalSection(&g_csdebug);
}

void DebugOut (unsigned int cMask, WCHAR *lpszFormat, ...)
{
    static WCHAR szBigBuffer[2000];
    va_list arglist;
    va_start (arglist, lpszFormat);
    wvsprintf (szBigBuffer, lpszFormat, arglist);
    va_end (arglist);

    EnterCriticalSection(&g_csdebug);
    DEBUGMSG(cMask, (szBigBuffer));
    LeaveCriticalSection(&g_csdebug);
}

#define BPR        64

void DumpBuff(unsigned char *lpBuffer, unsigned int cBuffer)
{
    WCHAR szLine[5 + 7 + 2 + 4 * BPR];

    EnterCriticalSection (&g_csdebug);

    for (int i = 0 ; i < (int)cBuffer ; i += BPR) {
        int bpr = cBuffer - i;
        if (bpr > BPR)
            bpr = BPR;

        wsprintf (szLine, L"%04x ", i);
        WCHAR *p = szLine + wcslen (szLine);

        for (int j = 0 ; j < bpr ; ++j) {
            WCHAR c = lpBuffer[i + j];
            if ((c < L' ') || (c >= 127))
                c = L'.';

            *p++ = c;
        }

        for ( ; j < BPR ; ++j) {
            *p++ = L' ';
        }

        *p++ = L'\n';
        *p++ = L'\0';

        DEBUGMSG(ZONE_PACKETS, (_T("%s"), szLine));
    }

    LeaveCriticalSection (&g_csdebug);
}

#else // ! DEBUG

void DumpBuff(unsigned char *lpBuffer, unsigned int cBuffer) {}
void DebugOut (unsigned int cMask, WCHAR *lpszFormat, ...) {}
void DebugInit(void) {}
void DebugDeinit(void) {}

#endif // DEBUG
