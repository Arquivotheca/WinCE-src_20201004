//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "hdstub_p.h"

BOOL HwExceptionHandler (PEXCEPTION_RECORD pex, CONTEXT *pContext, BOOLEAN b2ndChance)
{
    BOOL fHandled = TRUE;
    DEBUGGERMSG(HDZONE_HW, (L"++HwExceptionHandler: pex=0x%08x, pContext=0x%08x, b2ndChance=%d\r\n",
                            pex, pContext, b2ndChance));

    DEBUGGERMSG (HDZONE_HW, (L"  HwExceptionHandler: Unmapped Exception Address = 0x%08x\r\n", CONTEXT_TO_PROGRAM_COUNTER (pContext)));
    DEBUGGERMSG (HDZONE_HW, (L"  HwExceptionHandler: Mapped Exception Address   = 0x%08x\r\n", MapPtr (CONTEXT_TO_PROGRAM_COUNTER (pContext))));

    /* Protect against tripping an exception on the DebugBreak() in HdstubNotify */
    if ((CONTEXT_TO_PROGRAM_COUNTER (pContext) < (DWORD)HdstubNotify) ||
        (CONTEXT_TO_PROGRAM_COUNTER (pContext) > ((DWORD)HdstubNotify + 20)))
    {
        //SafeEnterCriticalSection(&csHdStubEventRecord);
        {
            // Pack the exception record and context and flag into structure
            g_Event.dwType = HDSTUB_EVENT_EXCEPTION;
            g_Event.dwArg1 = (DWORD) pex;
            g_Event.dwArg2 = (DWORD) pContext;
            g_Event.dwArg3 = (DWORD) b2ndChance;

#ifdef HDHW_NEEDS_CACHE_FLUSH
            if (g_HdStubData.pulHDEventFilter && (*g_HdStubData.pulHDEventFilter & HDSTUB_FILTER_ENABLE_CACHE_FLUSH))
            {
                DEBUGGERMSG(HDZONE_HW, (L"  HwExceptionHandler: Flushing cache\r\n"));
                g_HdStubData.FlushCacheRange (0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
            }
#endif
            HdstubNotify ();
            fHandled = g_Event.fHandled;
        }
        //SafeLeaveCriticalSection(&csHdStubEventRecord);
    }

    DEBUGGERMSG(HDZONE_HW, (L"--HwExceptionHandler: %d\r\n", fHandled));
    return fHandled;
}


void HwPageInHandler (DWORD dwPageAddr, DWORD dwNumPages, BOOL bWriteable)
{
    DEBUGGERMSG(HDZONE_HW, (L"++HwPageInHandler: dwPageAddr=0x%08x, bWriteable=%d\r\n",
                            dwPageAddr, bWriteable));

    //SafeEnterCriticalSection(&csHdStubEventRecord);
    {
        g_Event.dwType = HDSTUB_EVENT_PAGEIN;
        g_Event.dwArg1 = dwPageAddr;
        g_Event.dwArg2 = dwNumPages;
        g_Event.dwArg3 = (DWORD) bWriteable;

#ifdef HDHW_NEEDS_CACHE_FLUSH
        if (g_HdStubData.pulHDEventFilter && (*g_HdStubData.pulHDEventFilter & HDSTUB_FILTER_ENABLE_CACHE_FLUSH))
        {
            DEBUGGERMSG(HDZONE_HW, (L"  HwPageInHandler: Flushing cache\r\n"));
            g_HdStubData.FlushCacheRange (0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
        }
#endif
        HdstubNotify ();
    }
    //SafeLeaveCriticalSection(&csHdStubEventRecord);

    DEBUGGERMSG(HDZONE_HW, (L"--HwPageInHandler\r\n"));
}


void HwModLoadHandler(DWORD dwVmBaseAddr)
{
    DEBUGGERMSG(HDZONE_HW, (L"++HwModLoadHandler: dwVmBaseAddr=0x%08x\r\n", dwVmBaseAddr));

    //SafeEnterCriticalSection(&csHdStubEventRecord);
    {
        g_Event.dwType = HDSTUB_EVENT_MODULE_LOAD;
        g_Event.dwArg1 = dwVmBaseAddr;

#ifdef HDHW_NEEDS_CACHE_FLUSH
        if (g_HdStubData.pulHDEventFilter && (*g_HdStubData.pulHDEventFilter & HDSTUB_FILTER_ENABLE_CACHE_FLUSH))
        {
            DEBUGGERMSG(HDZONE_HW, (L"  HwModLoadHandler: Flushing cache\r\n"));
            g_HdStubData.FlushCacheRange (0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
        }
#endif
        HdstubNotify ();
    }
    //SafeLeaveCriticalSection(&csHdStubEventRecord);

    DEBUGGERMSG(HDZONE_HW, (L"--HwModLoadHandler\r\n"));
}


void HwModUnloadHandler(DWORD dwVmBaseAddr)
{
    DEBUGGERMSG(HDZONE_HW, (L"++HwModUnloadHandler: dwVmBaseAddr\r\n", dwVmBaseAddr));

    //SafeEnterCriticalSection(&csHdStubEventRecord);
    {
        g_Event.dwType = HDSTUB_EVENT_MODULE_UNLOAD;
        g_Event.dwArg1 = dwVmBaseAddr;

#ifdef HDHW_NEEDS_CACHE_FLUSH
        if (g_HdStubData.pulHDEventFilter && (*g_HdStubData.pulHDEventFilter & HDSTUB_FILTER_ENABLE_CACHE_FLUSH))
        {
            DEBUGGERMSG(HDZONE_HW, (L"  HwModUnloadHandler: Flushing cache\r\n"));
            g_HdStubData.FlushCacheRange (0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
        }
#endif
        HdstubNotify ();
    }
    //SafeLeaveCriticalSection(&csHdStubEventRecord);

    DEBUGGERMSG(HDZONE_HW, (L"--HwModUnloadHandler\r\n"));
}
