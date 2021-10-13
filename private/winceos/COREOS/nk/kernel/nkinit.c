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
/**     TITLE("Kernel Initialization")
 *++
 *
 *
 * Module Name:
 *
 *    nkinit.c
 *
 * Abstract:
 *
 *  This file contains the initialization code of kernel
 *
 *--
 */

#include <windows.h>
#include <kernel.h>


extern CRITICAL_SECTION RFBcs, PhysCS, ModListcs, ODScs, CompCS, MapCS, NameCS,
            PagerCS, PageOutCS, IntChainCS, WDcs, rtccs, PslServerCS;

extern PHDATA phdEvtPwrHndlr;
extern PHDATA phdPageOutEvent;
extern PHDATA phdEvtDirtyPage;


#ifdef DEBUG

void DumpMem (LPBYTE pb, DWORD cbSize)
{
    DWORD loop;

    for (loop = 0; loop < (cbSize / 0x10); loop ++, pb += 0x10) {
        NKD (L"%8.8lx: %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x\r\n",
            pb,
            pb[0], pb[1], pb[2], pb[3],
            pb[4], pb[5], pb[6], pb[7],
            pb[8], pb[9], pb[10], pb[11],
            pb[12], pb[13], pb[14], pb[15]);
    }
}

#endif


//------------------------------------------------------------------------------
// KernelInit - Kernel initialization before scheduling the 1st thread
//------------------------------------------------------------------------------

void KernelInit (void) 
{
#ifdef DEBUG
    g_pNKGlobal->pfnWriteDebugString (TEXT("Windows CE KernelInit\r\n"));
#endif

    InitializeSpinLock (&g_schedLock);
    InitializeSpinLock (&g_oalLock);
    InitializeSpinLock (&g_odsLock);
    InitializeSpinLock (&g_physLock);
    KernelFindMemory ();
    APICallInit ();         // setup API set
    HeapInit ();            // setup kernel heap
    PROCInit ();            // initialize process
    KernelVMInit ();        // setup VM for kernel
    THRDInit ();            // initialize threads
    MapfileInit ();

#ifdef DEBUG
    g_pNKGlobal->pfnWriteDebugString (TEXT("Scheduling the first thread.\r\n"));
#endif
}


//------------------------------------------------------------------------------
// KernelInit2 - kernel intialization after the 1st thread is started
//------------------------------------------------------------------------------

void KernelInit2 (void) 
{
    PEVENT pEvt;

    InitializeFastLock (&g_pprcNK->hndlLock);
    InitializeCriticalSection(&g_pprcNK->csVM);
    InitializeCriticalSection(&g_pprcNK->csLoader);
    DEBUGCHK (NULL == csarray[CSARRAY_NKLOADER]);  // Not yet assigned to anything
    csarray[CSARRAY_NKLOADER] = &g_pprcNK->csLoader;
    InitializeCriticalSection(&ODScs);
    InitializeCriticalSection(&CompCS);
    InitializeCriticalSection(&PhysCS);
    InitializeCriticalSection(&ModListcs);
    InitializeCriticalSection(&RFBcs);
    InitializeCriticalSection(&MapCS);
    InitializeCriticalSection(&NameCS);
    InitializeCriticalSection(&WDcs);
    InitializeCriticalSection(&PagerCS);
    InitializeCriticalSection(&PageOutCS);
    InitializeCriticalSection(&IntChainCS);
    InitializeCriticalSection(&PslServerCS);
    InitializeCriticalSection(&rtccs);

    g_phdAPIRegEvt       = NKCreateAndLockEvent (TRUE,  FALSE);
    DEBUGCHK (g_phdAPIRegEvt);
    
    phdAlarmThreadWakeup = NKCreateAndLockEvent (FALSE, FALSE);
    DEBUGCHK (phdAlarmThreadWakeup);
    
    phdEvtPwrHndlr       = NKCreateAndLockEvent (FALSE, FALSE);
    DEBUGCHK (phdEvtPwrHndlr);
    
    phdEvtDirtyPage      = NKCreateAndLockEvent (FALSE, TRUE);
    DEBUGCHK (phdEvtDirtyPage);
    
    phdPslNotifyEvent    = NKCreateAndLockEvent (FALSE, FALSE);
    DEBUGCHK (phdPslNotifyEvent);
    
    phdPageOutEvent      = NKCreateAndLockEvent (FALSE, FALSE);
    DEBUGCHK (phdPageOutEvent);

    // setup RTC alarm interrupt event
    pEvt = GetEventPtr (phdAlarmThreadWakeup);
    DEBUGCHK (pEvt);

    pEvt->phdIntr = phdAlarmThreadWakeup;
    DEBUGCHK (pEvt->phdIntr);
    
    IntrEvents[SYSINTR_RTC_ALARM-SYSINTR_DEVICES] = pEvt;

    // create a dummy event for kernel, in case someone waits for kernel to exit...
    g_pprcNK->phdProcEvt = NKCreateAndLockEvent (TRUE, FALSE);
    DEBUGCHK (g_pprcNK->phdProcEvt);
    g_pprcNK->phdProcEvt->dwData = STILL_ACTIVE;

}



