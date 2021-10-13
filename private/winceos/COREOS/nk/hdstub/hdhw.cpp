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

#include "hdstub_p.h"

BOOL HwExceptionHandler (PEXCEPTION_RECORD pex, CONTEXT *pContext, BOOLEAN b2ndChance)
{
    HDSTUB_EVENT2 Event = 
    {
        HDSTUB_EVENT2_SIGNATURE,
        sizeof(HDSTUB_EVENT),
        HDSTUB_CURRENT_VERSION,
        HDSTUB_EVENT_EXCEPTION,
    };
    
    DEBUGGERMSG(HDZONE_HW, (L"++HwExceptionHandler: pex=0x%08x, pContext=0x%08x, b2ndChance=%d\r\n",
                            pex, pContext, b2ndChance));

    DEBUGGERMSG (HDZONE_HW, (L"  HwExceptionHandler: Unmapped Exception Address = 0x%08x\r\n", CONTEXT_TO_PROGRAM_COUNTER (pContext)));

    /* Protect against tripping an exception on the DebugBreak() in HdstubNotify */
    if ((CONTEXT_TO_PROGRAM_COUNTER (pContext) < (DWORD)HwTrap) ||
        (CONTEXT_TO_PROGRAM_COUNTER (pContext) > ((DWORD)HwTrap + 20)))
    {
        // Pack the exception record and context and flag into structure
        Event.Event.Exception.pExceptionRecord = (ULONG32) pex;
        Event.Event.Exception.pContext         = (ULONG32) pContext;
        Event.Event.Exception.SecondChance     = b2ndChance;

#ifdef HDHW_NEEDS_CACHE_FLUSH
        if (pulHDEventFilter && (*pulHDEventFilter & HDSTUB_FILTER_ENABLE_CACHE_FLUSH))
        {
            DEBUGGERMSG(HDZONE_HW, (L"  HwExceptionHandler: Flushing cache\r\n"));
            NKCacheRangeFlush(0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
        }
#endif
        HwTrap(&Event);
    }

    DEBUGGERMSG(HDZONE_HW, (L"--HwExceptionHandler: %d\r\n", Event.Event.Exception.fHandled));
    return Event.Event.Exception.fHandled;
}


void HwPageInHandler (DWORD dwPageAddr, DWORD dwNumPages, BOOL bWriteable)
{
    HDSTUB_EVENT2 Event = 
    {
        HDSTUB_EVENT2_SIGNATURE,
        sizeof(HDSTUB_EVENT),
        HDSTUB_CURRENT_VERSION,
        HDSTUB_EVENT_PAGEIN,
    };
    
    DEBUGGERMSG(HDZONE_HW, (L"++HwPageInHandler: dwPageAddr=0x%08x, bWriteable=%d\r\n",
                            dwPageAddr, bWriteable));

    Event.Event.PageIn.PageAddress = dwPageAddr;
    Event.Event.PageIn.NumPages    = dwNumPages;
    Event.Event.PageIn.VmId        = pVMProc->dwId;

#ifdef HDHW_NEEDS_CACHE_FLUSH
    if (pulHDEventFilter && (*pulHDEventFilter & HDSTUB_FILTER_ENABLE_CACHE_FLUSH))
    {
        DEBUGGERMSG(HDZONE_HW, (L"  HwPageInHandler: Flushing cache\r\n"));
        NKCacheRangeFlush(0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
    }
#endif
    HwTrap(&Event);

    DEBUGGERMSG(HDZONE_HW, (L"--HwPageInHandler\r\n"));
}


void HwPageOutHandler (DWORD dwPageAddr, DWORD dwNumPages)
{
    HDSTUB_EVENT2 Event = 
    {
        HDSTUB_EVENT2_SIGNATURE,
        sizeof(HDSTUB_EVENT),
        HDSTUB_CURRENT_VERSION,
        HDSTUB_EVENT_PAGEOUT,
    };
    
    DEBUGGERMSG(HDZONE_HW, (L"++HwPageOutHandler: dwPageAddr=0x%08x, dwNumPages=%d\r\n",
                            dwPageAddr, dwNumPages));

    Event.Event.PageIn.PageAddress = dwPageAddr;
    Event.Event.PageIn.NumPages    = dwNumPages;
    PPROCESS pVM = (dwPageAddr > VM_SHARED_HEAP_BASE ? g_pprcNK : GetPCB()->pVMPrc);
    Event.Event.PageIn.VmId        = pVM->dwId;

#ifdef HDHW_NEEDS_CACHE_FLUSH
    if (pulHDEventFilter && (*pulHDEventFilter & HDSTUB_FILTER_ENABLE_CACHE_FLUSH))
    {
        DEBUGGERMSG(HDZONE_HW, (L"  HwPageOutHandler: Flushing cache\r\n"));
        NKCacheRangeFlush(0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
    }
#endif
    HwTrap(&Event);

    DEBUGGERMSG(HDZONE_HW, (L"--HwPageOutHandler\r\n"));
}


void HwModLoadHandler(DWORD dwVmBaseAddr)
{
    HDSTUB_EVENT2 Event = 
    {
        HDSTUB_EVENT2_SIGNATURE,
        sizeof(HDSTUB_EVENT),
        HDSTUB_CURRENT_VERSION,
        HDSTUB_EVENT_MODULE_LOAD,
    };
    
    DEBUGGERMSG(HDZONE_HW, (L"++HwModLoadHandler: dwVmBaseAddr=0x%08x\r\n", dwVmBaseAddr));

    Event.Event.Module.StructAddr = dwVmBaseAddr;

#ifdef HDHW_NEEDS_CACHE_FLUSH
    if (pulHDEventFilter && (*pulHDEventFilter & HDSTUB_FILTER_ENABLE_CACHE_FLUSH))
    {
        DEBUGGERMSG(HDZONE_HW, (L"  HwModLoadHandler: Flushing cache\r\n"));
        NKCacheRangeFlush(0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
    }
#endif
    HwTrap(&Event);

    DEBUGGERMSG(HDZONE_HW, (L"--HwModLoadHandler\r\n"));
}


void HwModUnloadHandler(DWORD dwVmBaseAddr)
{
    HDSTUB_EVENT2 Event = 
    {
        HDSTUB_EVENT2_SIGNATURE,
        sizeof(HDSTUB_EVENT),
        HDSTUB_CURRENT_VERSION,
        HDSTUB_EVENT_MODULE_UNLOAD,
    };
    
    DEBUGGERMSG(HDZONE_HW, (L"++HwModUnloadHandler: dwVmBaseAddr\r\n", dwVmBaseAddr));

    Event.Event.Module.StructAddr = dwVmBaseAddr;

#ifdef HDHW_NEEDS_CACHE_FLUSH
    if (pulHDEventFilter && (*pulHDEventFilter & HDSTUB_FILTER_ENABLE_CACHE_FLUSH))
    {
        DEBUGGERMSG(HDZONE_HW, (L"  HwModUnloadHandler: Flushing cache\r\n"));
        NKCacheRangeFlush(0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
    }
#endif
    HwTrap(&Event);

    DEBUGGERMSG(HDZONE_HW, (L"--HwModUnloadHandler\r\n"));
}
