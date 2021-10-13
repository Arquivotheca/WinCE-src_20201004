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
    
    hdstub.c : HDStub layer

Abstract:

    HDStub is a small kernel dll that accepts and filters notifications from
    the kernel.  These notifications include (and are not limited to)
    exceptions, virtual memory paging, module load, and module unload.

Environment:

    CE kernel

History:

--*/

#include "hdstub_p.h"
#include <kdbgpublic.h>
#include "hdbreak.h"
#include "hddatabreakpoint.h"
#include "dwprivate.h"

// BEGIN OsAxsHCe50.dll parameters
// Global stopping event structure.

// Taint flag for modules.  Indicates the number of module events that OsAccess
// has missed.
static HDSTUB_SHARED_GLOBALS s_HdSharedGlobals = {  sizeof (HDSTUB_SHARED_GLOBALS),
                                                    (DWORD) MLNO_ALWAYS_ON_UNINIT, /* Notify by default */
                                                    TRUE}; /* Notify host-side of all mod loads and unlaods */
// END parameters

void WINAPIV NKDbgPrintfW(LPCWSTR lpszFmt, ...)  
{
    va_list arglist;
    va_start(arglist, lpszFmt);
    NKvDbgPrintfW(lpszFmt, arglist);
    va_end(arglist);
}

DEBUGGER_DATA DebuggerData =
{
    DEBUGGER_DATA_SIG0,
    DEBUGGER_DATA_SIG1,
    DEBUGGER_DATA_SIG2,
    DEBUGGER_DATA_SIG3,
};

// DEBUGGER_DATA *g_pDebuggerData = &DebuggerData;

static ULONG s_ulOldHDEventFilter = 0;

// During boot, allow 2 load events to go through without altering the event flags.
// This is for performance reasons, because 2 load events would guarantee the hardware debugger starting up.
DWORD g_dwModInitCount = 2;


void HdstubFreezeSystem(void);
void HdstubThawSystem(void);
void HdstubFlushIllegalInstruction(void);
void HdstubClearIoctl();
void HdstubStopAllCpuCB();
void EnableHDEvents(BOOL);
void HdstubUpdateModLoadNotif ();
ULONG HdstubGetCallStack(PTHREAD pth, PCONTEXT pCtx, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip);

#define MAX_MPIC 16 //MAX HW DBP * 2

typedef struct 
{
    DWORD dwIoControlCode;
    KD_BPINFO kdbpi;    
} MP_IOCTL_CMD;

MP_IOCTL_CMD g_mpic[MAX_MPIC] = {0}; 
DWORD g_dw_mpic_idx = 0;
volatile DWORD g_fDoCommand = FALSE;


ULONG EntranceCount;
HINSTANCE g_hInstance; //global handle to hInstance for hd.dll

// If a BSP supports KD_IOCTL_KERNEL_HALT / CONTINUE then we will set the following flag to TRUE.
static BOOL g_fBreakContinueSupported = FALSE;
static DWORD g_dwBreakRefCount = 0;

//Saved interrupt state.
static BOOL g_fInterruptState = FALSE;


/*++

Routine Name:

    HdstubConnectKdstub

Routine Description:

    Hook KDStub to HDStub.  Assume that the Kernel debugger dll is already
    loaded.

Arguments:

    pfnCliInit    - in, Pointer to Client init function
    pvExtra       - in, Pointer to Extra Data to pass through to Client

Return Value:

    TRUE    : Load successful
    FALSE   : Failure

--*/
BOOL HdstubConnectClient (KDBG_DLLINITFN pfnDllInit, void *pvExtra)
{
    BOOL fReturn;
   
    DEBUGGERMSG (HDZONE_INIT, (L"++HdstubConnectKdstub: CliInit=0x%.8x, pvExtra=0x%.8x\r\n", pfnDllInit, pvExtra));
    fReturn = pfnDllInit(&DebuggerData, pvExtra);
    DEBUGGERMSG (HDZONE_INIT, (L"--HdstubConnectKdstub: return %d\r\n", fReturn));
    return fReturn;
}


/*++

Routine Name:

    HdstubDLLEntry

Routine Description:

    Attach to nk.exe.  Provide nk.exe with a pointer to HdstubInit

Arguments:

    hInstance -
    ulReason -
    pvReserved -

Return Value:

    TRUE  - Loaded
    FALSE - Error during entry.

--*/
extern "C" BOOL WINAPI HdstubDLLEntry (HINSTANCE hInstance, ULONG ulReason,
        LPVOID pvReserved)
{
    BOOL bResult;
    BOOL (*kernIoctl)(HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
    FARPROC pfnInit;
    
    kernIoctl = reinterpret_cast<BOOL(*)(HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD)>(pvReserved);
    pfnInit = reinterpret_cast<FARPROC>(HdstubInit);
    bResult = TRUE;

    g_hInstance = hInstance;
    
    switch (ulReason)
    {
    case DLL_PROCESS_ATTACH:
        bResult = kernIoctl(reinterpret_cast<HANDLE>(KMOD_DBG),
                IOCTL_DBG_HDSTUB_INIT, &pfnInit, sizeof(pfnInit),
                NULL, 0, NULL);
        break;

    case DLL_PROCESS_DETACH:
        break;
    }

    return bResult;
}


/*++

Routine Name:

    HdstubInit

Routine Description:

    Initialize Hdstub.

Arguments:

    pHdInit     - out, export event notification functions.

Return Value:

    TRUE : success.

--*/
extern "C" BOOL HdstubInit(KDBG_INIT *pKdbgInit)
{
    BOOL fResult = FALSE;

#ifdef ARM
    InitPCBFunctions ();
#endif

    if (pKdbgInit && VALIDATE_KDBG_INIT(pKdbgInit))
    {
        memcpy(&DebuggerData.StartOfKernelImports,
            &pKdbgInit->StartOfKernelImports,
            offsetof(DEBUGGER_DATA, EndOfKernelImports) - offsetof(DEBUGGER_DATA, StartOfKernelImports));

        DD_DebugPrintf = HdstubDbgPrintf;
        REGISTERDBGZONES((PMODULE)g_hInstance);
        //NOTE: Debug message are available after this point.
            
        DD_MoveMemory0 = MoveMemory0;
        DD_MapAddr= HdstubMapAddr;
        DD_UnMapAddr= HdstubUnMapAddr;
        DD_GetProperVM = GetProperVM;
        DD_GetPhysicalAddress = GetPhysicalAddress;
        DD_AddrToPA = AddrToPA;
        DD_FreezeSystem = HdstubFreezeSystem;
        DD_ThawSystem = HdstubThawSystem;
        DD_OnEmbeddedBreakpoint = OnEmbeddedBreakpoint;
        DD_Currently16Bit = Currently16Bit;
        DD_IoControl = HdstubIoControl;
        DD_AddVMBreakpoint = BreakpointImpl::Add;
        DD_DeleteVMBreakpoint = BreakpointImpl::Delete;
        DD_FindVMBreakpoint = BreakpointImpl::Find;        
        DD_EnableVMBreakpoint = BreakpointImpl::Enable;
        DD_DisableVMBreakpoint = BreakpointImpl::Disable;

        DD_AddDataBreakpoint = AddDataBreakpoint;
        DD_DeleteDataBreakpoint = DeleteDataBreakpoint;
        DD_FindDataBreakpoint = FindDataBreakpoint;
        DD_EnableDataBreakpointsOnPage = EnableDataBreakpointsOnPage;
        DD_DisableDataBreakpointsOnPage = DisableDataBreakpointsOnPage;
        DD_DisableAllDataBreakpoints = DisableAllDataBreakpoints;
        DD_EnableAllDataBreakpoints = EnableAllDataBreakpoints;

        DD_IsValidProcPtr = IsValidProcess;
        DD_UpdateModLoadNotif = HdstubUpdateModLoadNotif;
        DD_GetCallStack = HdstubGetCallStack;
        DD_SetDisableAllBPOnHalt = BreakpointImpl::SetDisableAllOnHalt;
        DD_DisableAllBPOnHalt = BreakpointImpl::DisableAllOnHalt;
        DD_SetBPState = BreakpointImpl::SetState;
        DD_GetBPState = BreakpointImpl::GetState;
        DD_Sanitize = BreakpointImpl::Sanitize;
        DD_EnableAllBP = BreakpointImpl::EnableAll;
                    
        DebuggerData.exceptionIdx = EXCEPTION_STACK_UNUSED;
        
        // Export trap functions
        pKdbgInit->pfnException = HdstubTrapException;
        pKdbgInit->pfnVmPageIn = HdstubTrapVmPageIn;
        pKdbgInit->pfnVmPageOut = HdstubTrapVmPageOut;
        pKdbgInit->pfnModLoad = HdstubTrapModuleLoad;
        pKdbgInit->pfnModUnload = HdstubTrapModuleUnload;
        pKdbgInit->pfnIsDataBreakpoint = HdstubIsDataBreakpoint;
        pKdbgInit->pfnStopAllCpuCB = HdstubStopAllCpuCB;
        pKdbgInit->pfnConnectClient = HdstubConnectClient;

        // Export hw debug notification function and structure.
        pKdbgInit->pHdSharedGlobals = &s_HdSharedGlobals;

        // Export Debugger API
        pKdbgInit->pfnKdSanitize = HdstubSanitize;
        pKdbgInit->pfnKdIgnoreIllegalInstruction = HdstubIgnoreIllegalInstruction;
        //pKdbgInit->pfnKdReboot   = HdstubReboot;
        //pKdbgInit->pfnKdPostInit = HdstubPostInit;

        ROMMap::Init();
        HdstubInitMemAccess();
        DataBreakpointsInit();

        DD_EnableHDEvents = EnableHDEvents;

        EntranceCount = 0;
        fResult = TRUE;
    }
    else
    {
        HdstubDbgPrintf(TEXT("Bad versioning between kernel.dll and hd.dll.  Rebuild both _clean_\r\n"));
    }

    // Call HALT Ioctl to check if the BSP supports break/continue. 
    BOOL fIoctlResult = DD_IoControl (KD_IOCTL_KERNEL_HALT, NULL, 0);

    if (TRUE == fIoctlResult)
    {
        // BSP supports break/continue, now back to run state...
        DD_IoControl (KD_IOCTL_KERNEL_CONTINUE, NULL, 0);
        g_fBreakContinueSupported = TRUE;
    }
    
    return fResult;
}


void DispModNotifDbgState (void)
{
    static DWORD s_dwPrevNmlState;
    static BOOL s_dwCount = 1;
    if (!s_dwCount && (s_dwPrevNmlState != s_HdSharedGlobals.dwModLoadNotifDbg))
    {
        if (s_HdSharedGlobals.dwModLoadNotifDbg & ~MLNO_OFF_UNTIL_HALT)
        {
            RETAILMSG (1, (L"HD: Immediate debugger module load notifications ACTIVE (slower boot - non real-time).\r\n"));
        }
        else
        {
            RETAILMSG (1, (L"HD: Immediate debugger module load notifications INACTIVE (faster boot - real-time).\r\n"));
            RETAILMSG (1, (L"HD: WARNING: Module load and unload notifications to the host debugger will be delayed until the first break or exception.\r\n"));
            RETAILMSG (1, (L"HD:          User triggers (breakpoints) will NOT be instantiated until that initial break or exception.\r\n"));
        }        
    }
    s_dwPrevNmlState = s_HdSharedGlobals.dwModLoadNotifDbg;
    if (s_dwCount) s_dwCount--;
}


BOOL HdstubFilterException(ULONG FilterMask)
{
    for (ULONG i = 0; i < KDBG_EH_MAX; ++i)
    {
        if (DD_eventHandlers[i].dwFilter & FilterMask)
            return TRUE;
    }
    return FALSE;
}


void HdstubFreezeSystem()
{
    // Acquire and release these locks to safely freeze the system.
    AcquireSpinLock(&g_schedLock);
    AcquireKitlSpinLock();
    AcquireSpinLock(&g_physLock);
    AcquireSpinLock(&g_oalLock);
    StopAllOtherCPUs();                     // Halt all other CPUs on the system
    DEBUGGERMSG(HDZONE_ENTRY, (L"<<< SYSTEM HALTED\r\n"));
    HdstubClearIoctl();
    ReleaseSpinLock(&g_schedLock);
    ReleaseKitlSpinLock();
    ReleaseSpinLock(&g_physLock);
    ReleaseSpinLock(&g_oalLock);

    // The lines above guarantee the following:
    // - KITL lock is not taken
    // - Physical memory lock is not taken
    // - no other CPU is running
    // - we are holding the scheduler lock
    // So this is effectively a single threaded state. No need to worry
    // about concurrency issues.

    g_dwBreakRefCount++;

    if (g_dwBreakRefCount == 1)
    {
        if (g_fBreakContinueSupported)
        {
            DD_IoControl (KD_IOCTL_KERNEL_HALT, NULL, 0); 
        }
        
        if (g_pKData->nCpus <= 1)
        {
            g_fInterruptState = INTERRUPTS_ENABLE(FALSE);
        }
        if (((OEMGLOBAL*)(g_pKData->pOem))->pfnKdEnableTimer)
        {
            ((OEMGLOBAL*)(g_pKData->pOem))->pfnKdEnableTimer(FALSE);            
        }
    }
}


void HdstubThawSystem()
{    
    g_dwBreakRefCount--;

    if (g_dwBreakRefCount == 0)
    {
        if (g_fBreakContinueSupported)
        {
            DD_IoControl (KD_IOCTL_KERNEL_CONTINUE, NULL, 0); 
        }
        
        if (((OEMGLOBAL*)(g_pKData->pOem))->pfnKdEnableTimer)
        {
            ((OEMGLOBAL*)(g_pKData->pOem))->pfnKdEnableTimer(TRUE);
        }
        if (g_pKData->nCpus <= 1)
        {
            INTERRUPTS_ENABLE(g_fInterruptState);
        }
    }
    
    HdstubCommitHWDataBreakpoints();
    DEBUGGERMSG(HDZONE_ENTRY, (L">>> RESUMING SYSTEM \r\n"));
    ResumeAllOtherCPUs();
}


void HdstubUpdateModLoadNotif ()
{
    if (s_HdSharedGlobals.dwModLoadNotifDbg != MLNO_ALWAYS_ON)
    {
        // Device hasn't been reporting load notifications.
        DD_fIgnoredModuleNotifications = TRUE;
    }
    if (s_HdSharedGlobals.dwModLoadNotifDbg == MLNO_OFF_UNTIL_HALT)
    { // We can only be here if an external exception happen (real halt)
        s_HdSharedGlobals.dwModLoadNotifDbg = MLNO_ALWAYS_ON;
    }
}


ULONG HdstubGetCallStack(PTHREAD pth, PCONTEXT pCtx, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip)
{
    DWORD dwFrames = 0;
    BOOL RestorePrcInfo = FALSE;
    DWORD originalPrcInfo = 0;

    if (pth == pCurThread)
    {
        // If this is the current thread we need to start unwinding from the point of 
        // the exception, otherwise we get the HDSTUB + OSAXS frames included

        if (pth->pcstkTop != NULL && (pth->pcstkTop->dwPrcInfo & CST_EXCEPTION) == 0)
        {
            // Special case for the current thread.
            //
            // If the current thread has a pcstkTop record (it better!) and the dwProcInfo field
            // is not flagged as an exception field, we made need to fake an exception here.
            // (This is for the RaiseException()/CaptureDumpFile() case.  The kernel does not
            // set CST_EXCEPTION on calls to RaiseException.)
            //
            // Check whether the stackpointer from the exception record will fit within the thread's
            // secure stack range. If it does, then do nothing. If it does not, temporarily or
            // CST_EXCEPTION to CALLSTACK.dwPrcInfo.
            //
            // (When dwPrcInfo has the CST_EXCEPTION bit set, the kernel unwinder will automatically
            // pop off the topmost CALLSTACK record before doing any stack walking.  This should
            // synchronize the pcstkTop pointer with the exception context.)
            const DWORD StackPointer = static_cast<DWORD>(CONTEXT_TO_STACK_POINTER(pCtx));
            const DWORD SecureStackLo = pth->tlsSecure[PRETLS_STACKBASE];
            const DWORD SecureStackHi = SecureStackLo + pth->tlsSecure[PRETLS_STACKSIZE] - (4 * REG_SIZE);

            if (StackPointer < SecureStackLo || SecureStackHi <= StackPointer)
            {
                DEBUGGERMSG(HDZONE_UNWIND,(L"  HdstubGetCallStack: Thread %08X Stackpointer %08X is not in Secure Stack Range: %08X-%08X\r\n",
                                             pth, StackPointer, SecureStackLo, SecureStackHi));
                originalPrcInfo = pth->pcstkTop->dwPrcInfo;
                pth->pcstkTop->dwPrcInfo |= CST_EXCEPTION;
                RestorePrcInfo = TRUE;
                DEBUGGERMSG(HDZONE_UNWIND,(L"  HdstubGetCallStack: Thread %08X dwPrcInfo was %08X, is %08X\r\n", pth, originalPrcInfo,
                                             pth->pcstkTop->dwPrcInfo));
            }
        }
    }
    dwFrames = NKGetThreadCallStack(pth, dwMaxFrames, lpFrames, dwFlags, dwSkip, pCtx);

    if (RestorePrcInfo)
    {
        // Remember to restore the CALLSTACK.dwPrcInfo.  (Currently, the kernel doesn't really
        // check this flag anywhere else but in the unwinder.)
        if (pth->pcstkTop != NULL)
        {
            pth->pcstkTop->dwPrcInfo = originalPrcInfo;
            DEBUGGERMSG(HDZONE_UNWIND,(L"  HdstubGetCallStack: Thread %08X restore dwPrcInfo to %08X\r\n", pth, pth->pcstkTop->dwPrcInfo));
        }
    }

    return dwFrames;
}


void HdstubPageInCallstack(PCONTEXT pContext)
{
    CallSnapshot3 g_callstack3[MAX_STACK_FRAMES_CONTEXT];
    CONTEXT ctx = *pContext;
    
    HdstubGetCallStack(pCurThread, &ctx, MAX_STACK_FRAMES_CONTEXT,
            (LPVOID)g_callstack3,
            STACKSNAP_NEW_VM | STACKSNAP_RETURN_FRAMES_ON_ERROR, 0);
}


//
// Trap functions for hdstub.  These functions are called by kernel
// to notify of a specific event.
//
const DWORD c_dwProcessIdIndex = 0;
const DWORD c_dwThreadIdIndex = 1;
const DWORD c_dwExtraFilesPathIndex = 2;
const DWORD c_dwTrustedAppIndex = 3;
const DWORD c_dwProcessOverrideIndex = 5;

BOOL HdstubTrapException(PEXCEPTION_RECORD pex, CONTEXT *pContext,
        BOOLEAN b2ndChance)
{
    BOOL fHandled = FALSE;
    BOOL fIgnoreExceptionOnContinue = FALSE;
    DWORD i;
    DEBUGGERMSG(HDZONE_ENTRY, 
        (TEXT("++HdstubTrapException: pex = 0x%.8x, pContext = 0x%.8x, b2ndChance = %d, ExcAddr=0x%08X\r\n"),
        pex, pContext, b2ndChance, pex ? pex->ExceptionAddress : NULL));

    //Figure out if we need to page in the PDATA by waling the callstacks.
    //We only want to page in the PDATA if we are going to stop in the debugger.
    //If we aren't going to stop in the debugger, we are just wasting cycles by walking the callstack.

    //Is kd.dll present?
    BOOL fKDPresent = DD_eventHandlers[KDBG_EH_DEBUGGER].pfnException != NULL; 
    //Is this an Hdstub event? (i.e module load/unload, VmPageOut, etc.)  
    //The desktop debugger doesn't stop for those events.
    BOOL fHDEvent = (pex && pex->ExceptionAddress >= HwTrap && pex->ExceptionAddress < ((char *)HwTrap + HD_NOTIFY_MARGIN));

    if(fKDPresent && !fHDEvent && !b2ndChance)
    {
        HdstubPageInCallstack(pContext);
    }

    if (HdstubFilterException(HDSTUB_FILTER_EXCEPTION))
    {
        pex->ExceptionInformation[c_dwTrustedAppIndex] = TRUE;
        
        HdstubFreezeSystem();
        
        if (InterlockedIncrement((LONG *)&EntranceCount) == 1)
        {
            if (BreakpointImpl::DisableAllOnHalt())
            {
                BreakpointImpl::DisableAll();
            }
            HdstubFlushIllegalInstruction();
        }

        for (i = 0; !fHandled && i < KDBG_EH_MAX; ++i)
        {
            if (DD_eventHandlers[i].pfnException &&
                (DD_eventHandlers[i].dwFilter & HDSTUB_FILTER_EXCEPTION))
            {
                DEBUGGERMSG(HDZONE_ENTRY,(L"  HdstubTrapException calling client %d, handler %08X\r\n", i, DD_eventHandlers[i].pfnException));
                fHandled = DD_eventHandlers[i].pfnException(pex, pContext, b2ndChance, &fIgnoreExceptionOnContinue);
                DEBUGGERMSG(HDZONE_ENTRY,(L"--HdstubTrapException client %d returned %d\r\n", i, fHandled));
            }
        }

        if (InterlockedDecrement((LONG *)&EntranceCount) == 0)
        {
            BreakpointImpl::EnableAll();
        }

        HdstubThawSystem();
    }

    // Notify hardware support if necessary
    if (!fHandled &&
        (*pulHDEventFilter & HDSTUB_FILTER_EXCEPTION))
    {
        if (*pulHDEventFilter & HDSTUB_FILTER_NONMP_PROBE)
        {
            HdstubFreezeSystem();
        }
        
        fIgnoreExceptionOnContinue = HwExceptionHandler (pex, pContext, b2ndChance);
        DEBUGGERMSG (HDZONE_HW, (L"  HdstubTrapException: hardware fHandled=%d\r\n", fHandled));

        if (*pulHDEventFilter & HDSTUB_FILTER_NONMP_PROBE)
        {
            HdstubThawSystem();
        }
    }

    DispModNotifDbgState ();

    DEBUGGERMSG (HDZONE_ENTRY, (TEXT ("--HdstubTrapException: pex = 0x%.8x, fHandled = %d\r\n"), pex, fHandled));
    return fIgnoreExceptionOnContinue;
}


static void HdstubTrapVmPageInRange (DWORD dwPageAddr, DWORD dwNumPages, BOOL bWriteable)
{
    BOOL fHandled = FALSE;
    DWORD i;
    
    DEBUGGERMSG(HDZONE_ENTRY, (TEXT ("++HdstubTrapVmPageInRange: dwPageAddr=0x%.8x, dwNumPages=%d bWriteable=%d\r\n"),
        dwPageAddr, dwNumPages, bWriteable));

    for (i = 0; !fHandled && i < KDBG_EH_MAX; ++i)
    {
        if (DD_eventHandlers[i].pfnVmPageIn &&
            (DD_eventHandlers[i].dwFilter & HDSTUB_FILTER_VMPAGEIN))
        {
            fHandled = DD_eventHandlers[i].pfnVmPageIn(dwPageAddr, dwNumPages, bWriteable);
        }

    }

    if (!fHandled &&
        (*pulHDEventFilter & HDSTUB_FILTER_VMPAGEIN))
    {
        HwPageInHandler (dwPageAddr, dwNumPages, bWriteable);
    }

    DEBUGGERMSG(HDZONE_ENTRY, (TEXT ("--HdstubTrapVmPageInRange\r\n")));
}


void HdstubTrapVmPageIn (DWORD dwPageAddr, BOOL bWriteable)
{
    HdstubTrapVmPageInRange (dwPageAddr, 1, bWriteable);
}


void HdstubTrapVmPageOut (DWORD dwPageAddr, DWORD dwNumPages)
{
    BOOL fHandled = FALSE;
    DWORD i;
    
    DEBUGGERMSG(HDZONE_ENTRY, (TEXT ("++HdstubTrapVmPageOut: dwPageAddr=0x%.8x, dwNumPages=%d\r\n"),
        dwPageAddr, dwNumPages));

    for (i = 0; !fHandled && i < KDBG_EH_MAX; ++i)
    {
        if (DD_eventHandlers[i].pfnVmPageOut &&
            (DD_eventHandlers[i].dwFilter & HDSTUB_FILTER_VMPAGEOUT))
        {
            fHandled = DD_eventHandlers[i].pfnVmPageOut(dwPageAddr, dwNumPages);
        }
    }

    if (!fHandled &&
        (*pulHDEventFilter & HDSTUB_FILTER_VMPAGEOUT))
    {
        HwPageOutHandler (dwPageAddr, dwNumPages);
    }

    DEBUGGERMSG(HDZONE_ENTRY, (TEXT ("--HdstubTrapVmPageOut\r\n")));
}


void HdstubTrapModuleLoad(DWORD dwStructAddr, BOOL fFirstLoad)
{
    BOOL fHandled = FALSE;
    DWORD i;

    DEBUGGERMSG(HDZONE_ENTRY, (TEXT("++HdstubTrapModuleLoad, dwStructAddr=0x%08X\r\n"),dwStructAddr));

    for (i = 0; !fHandled && i < KDBG_EH_MAX; ++i)
    {
        if (DD_eventHandlers[i].pfnModLoad &&
            (DD_eventHandlers[i].dwFilter & HDSTUB_FILTER_MODLOAD))
        {
            fHandled = DD_eventHandlers[i].pfnModLoad(dwStructAddr);
        }
    }

    if (s_HdSharedGlobals.dwModLoadNotifDbg & ~MLNO_OFF_UNTIL_HALT)
    {
        if (!fHandled && ((*pulHDEventFilter & HDSTUB_FILTER_MODLOAD) || g_dwModInitCount))
        {
            if (g_dwModInitCount)
            {
                -- g_dwModInitCount;
            }
            if (s_HdSharedGlobals.dwAllModuleLoadsAndUnloads || fFirstLoad)
            {
                HwModLoadHandler(dwStructAddr);
            }
        }
    }
    DispModNotifDbgState ();

    DEBUGGERMSG(HDZONE_ENTRY, (TEXT("--HdstubTrapModuleLoad\r\n")));
}


void HdstubTrapModuleUnload(DWORD dwStructAddr, BOOL fLastUnload)
{
    BOOL fHandled = FALSE;
    DWORD i;
    
    DEBUGGERMSG(HDZONE_ENTRY, (TEXT("++HdstubTrapModuleUnload, dwStructAddr=0x%08X\r\n"),dwStructAddr));

    for (i = 0; !fHandled && i < KDBG_EH_MAX; ++i)
    {
        if (DD_eventHandlers[i].pfnModUnload &&
            (DD_eventHandlers[i].dwFilter & HDSTUB_FILTER_MODUNLOAD))
        {
            fHandled = DD_eventHandlers[i].pfnModUnload(dwStructAddr);
        }
    }

    if (s_HdSharedGlobals.dwModLoadNotifDbg & ~MLNO_OFF_UNTIL_HALT)
    {
        if (!fHandled && (*pulHDEventFilter & HDSTUB_FILTER_MODUNLOAD))
        {
            if (s_HdSharedGlobals.dwAllModuleLoadsAndUnloads || fLastUnload)
            {
                HwModUnloadHandler(dwStructAddr);
            }                
        }
    }
    DispModNotifDbgState ();

    DEBUGGERMSG(HDZONE_ENTRY, (TEXT("--HdstubTrapModuleUnload\r\n")));
}

BOOL HdstubIoControl (DWORD dwIoControlCode, LPVOID lpBuf, DWORD nBufSize)
{
    BOOL fRet = FALSE;

    if (OEMKDIoControl)
    {
#if defined(x86)
        fRet = KCall((PKFN)OEMKDIoControl, dwIoControlCode, lpBuf, nBufSize);
#else
        fRet = OEMKDIoControl(dwIoControlCode, lpBuf, nBufSize);
#endif
    }
    return fRet;
}


//
// ARM specific workaround for breakpoints.  This becomes a non-issue on
// ARMv6, if the BKPT instruction is used.
//
struct IllegalInstructionException
{
    DWORD ThreadID;
    DWORD LastAddress;
};

const DWORD MaxIIE = 32;

struct IllegalInstructionException gIIE[MaxIIE];

//
// Track potential illegal instruction exceptions for a small number of threads
// on a specific address.  If the exception occurs twice, then consider the
// illegal instruction to be real.
//
BOOL HdstubIgnoreIllegalInstruction(DWORD dwAddr)
{
    BOOL fIgnoreIllegalInstruction;
    
    HdstubFreezeSystem();

    DWORD dwCurThreadID = PcbGetCurThreadId();
    DWORD i = HIWORD(dwCurThreadID) % MaxIIE;

    if (dwCurThreadID == gIIE[i].ThreadID &&
        dwAddr == gIIE[i].LastAddress)
    {
        // This thread has raised an illegal instruction twice on the same 
        // address.
        gIIE[i].ThreadID = 0;
        gIIE[i].LastAddress = 0;
        fIgnoreIllegalInstruction = FALSE;
    }
    else
    {
        gIIE[i].ThreadID = dwCurThreadID;
        gIIE[i].LastAddress = dwAddr;
        fIgnoreIllegalInstruction = TRUE;
    }

    HdstubThawSystem();

    return fIgnoreIllegalInstruction;
}


//
// Flush the table tracking illegal instruction exceptions.  This is proactively
// attempting to prevent the exception which occurs twice, with a long period
// of time in between.
//
void HdstubFlushIllegalInstruction()
{
    memset(&gIIE, 0, sizeof(gIIE));
}


BOOL HdstubSanitize(BYTE *buffer, void *pvAddress, ULONG nSize, BOOL fAlwaysCopy)
{
    // Called from OS.  Lock up so breakpoint table will be consistent
    HdstubFreezeSystem();
    MEMADDR Addr;
    PPROCESS pprcVM = PcbGetVMProc();
    SetVirtAddr(&Addr, pprcVM, pvAddress);
    BOOL fRet = BreakpointImpl::DoSanitize(buffer, &Addr, nSize, fAlwaysCopy, TRUE);
    HdstubThawSystem();
    return fRet;
}


void HdstubClearIoctl()
{
    g_fDoCommand = FALSE;
    g_dw_mpic_idx = 0;    
}


BOOL HdstubSetIoctl(DWORD dwIoControlCode, PKD_BPINFO pkdbpi)
{
    DEBUGGERMSG (HDZONE_DBP, (L"++HdstubSetIoctl: Ioctl=0x%08x, addr=0x%08x, handle=0x%08x\r\n", dwIoControlCode, (pkdbpi ? pkdbpi->ulAddress : 0), (pkdbpi ? pkdbpi->ulHandle : 0)));
    BOOL fRet = FALSE;

    if(g_dw_mpic_idx < MAX_MPIC)
    {
        g_fDoCommand = TRUE;            
        g_mpic[g_dw_mpic_idx].dwIoControlCode = dwIoControlCode;
        g_mpic[g_dw_mpic_idx].kdbpi = *pkdbpi;
        g_dw_mpic_idx++;
        fRet = TRUE;
    }
    
    DEBUGGERMSG (HDZONE_DBP || !fRet, (L"++HdstubSetIoctl: return %d commands %u\r\n", fRet, g_dw_mpic_idx));
    return fRet;
}


/* 
  * NOTE: THIS FUNCTION IS RUN IN ISR CONTEXT.  KEEP THE CODE TO A MINIMUM.  
  * NO FAULTS, NO PAGING, NO BLOCKING, NO CRITICAL SECTIONS.
  */
void HdstubStopAllCpuCB()
{
    if(g_fDoCommand)
    {
        for(DWORD i=0; i<g_dw_mpic_idx; i++)
        {
            HdstubIoControl (g_mpic[i].dwIoControlCode, (LPVOID)&g_mpic[i].kdbpi, sizeof(KD_BPINFO));            
        }
    }    
}

void EnableHDEvents(BOOL fEnable)
{
    if(fEnable)
    {
        DD_eventHandlers[KDBG_EH_CODEBP].pfnVmPageIn  = BreakpointImpl::OnPageIn;
        DD_eventHandlers[KDBG_EH_CODEBP].dwFilter     = HDSTUB_FILTER_VMPAGEIN;
        DD_eventHandlers[KDBG_EH_DATABP].pfnException = DataBreakpointTrap;
        DD_eventHandlers[KDBG_EH_DATABP].pfnVmPageOut = DataBreakpointVMPageOut;
        DD_eventHandlers[KDBG_EH_DATABP].dwFilter = HDSTUB_FILTER_EXCEPTION | HDSTUB_FILTER_VMPAGEOUT;
    }
    else
    {
        DD_eventHandlers[KDBG_EH_CODEBP].pfnVmPageIn  = NULL;
        DD_eventHandlers[KDBG_EH_CODEBP].dwFilter     = HDSTUB_FILTER_NIL;
        DD_eventHandlers[KDBG_EH_DATABP].pfnException = NULL;
        DD_eventHandlers[KDBG_EH_DATABP].pfnVmPageOut = NULL;
        DD_eventHandlers[KDBG_EH_DATABP].dwFilter = HDSTUB_FILTER_NIL;
    }

}
