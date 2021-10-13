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
//
//    softlog.c - implementat simple low-overhead softwarelog
//
#include <kernel.h>

#ifdef DEBUG

typedef struct {
    DWORD dwTag;
    DWORD dwData;
} SOFTLOG, *PSOFTLOG;

PSOFTLOG g_pSoftLog;
LONG g_nLogMask;
LONG g_idxSoftLog;

BOOL fStopLog;

void SoftLog (DWORD dwTag, DWORD dwData)
{
    if (g_pSoftLog && !fStopLog) {
        LONG idx = (InterlockedIncrement (&g_idxSoftLog) - 1) & g_nLogMask;
        g_pSoftLog[idx].dwTag = dwTag;
        g_pSoftLog[idx].dwData = dwData;
    }
}

// cbSize must be power of 2.
void InitSoftLog (void)
{
    LONG nLogSize = 4 * VM_PAGE_SIZE;

    DEBUGCHK (!(nLogSize & (nLogSize-1)));          // nLogSize must be a power of 2
    DEBUGCHK (!(nLogSize & VM_PAGE_OFST_MASK));     // nLogSize must be multiple of pages
    
    // grab 4 pages for softlog
    g_pOemGlobal->dwMainMemoryEndAddress -= nLogSize;

    nLogSize /= sizeof (SOFTLOG);
    DEBUGCHK (!(nLogSize & (nLogSize-1)));  // nLogSize must be a power of 2
    
    g_pSoftLog = (PSOFTLOG) g_pOemGlobal->dwMainMemoryEndAddress;
    g_nLogMask = nLogSize - 1;
    DEBUGMSG (ZONE_DEBUG, (L"Setting up softlog at 0x%8.8lx for 0x%x entries\r\n", g_pSoftLog, nLogSize));
}

//
// Dump the last N entries of the softlog
//
void DumpSoftLogs (DWORD dwLastNEntries)
{
    if (g_pSoftLog) {

        LONG idx;
        fStopLog = TRUE;
        if (dwLastNEntries > (DWORD) g_idxSoftLog) {
            dwLastNEntries = g_idxSoftLog;
        }
        idx = g_idxSoftLog - dwLastNEntries;

        for ( ; dwLastNEntries > 0; dwLastNEntries --, idx ++) {
            NKDbgPrintfW (L"0x%8.8lx 0x%8.8lx\r\n",
                 g_pSoftLog[idx&g_nLogMask].dwTag,  g_pSoftLog[idx&g_nLogMask].dwData);
        }
    }
}

#endif
