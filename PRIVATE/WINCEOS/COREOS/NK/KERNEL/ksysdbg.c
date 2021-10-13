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
/**     TITLE("Kernel System Debugger")
 *++
 *
 *
 * Module Name:
 *
 *    KSysDbg.c
 *
 * Abstract:
 *
 *  This file contains initialization and interfacing code for the system debugger components.
 *
 *--
 */
#include "kernel.h"
#include "kdstub.h"
#include "hdstub.h"
#include "osaxs.h"
#include "osaxs_common.h"


BOOL g_fForcedPaging = FALSE; // This is global because DbgVerify is called from DBG_CallCheck too


/*++

Routine Name:

    DbgVerify

    Routine Description:

    This function takes a void pointer to an address that the caller wants to read
    or write to and determines whether it is possible. Through this process, address 
    in pages that are not currently mapped (paged in) will be paged in if Force Page mode is ON.

    Most of the work is done through calls to VerifyAccess(), which determines 
    specific characteristics of the page in question and will specify whether access 
    is allowed.

    Argument(s):

    pvAddr - [IN] the address to be verified
    fProbeOnly - [IN] Should only test the presence of the VM, do not page-in even if Force Paging is ON
    pfPageInFailed - [OUT] an optional parameter that a caller can use to see if a page
                                was to be paged in, but was not since forced paging was disabled

    Return Value:

    Returns the same address if access if okay, or a different address, or NULL

--*/
static PVOID
DbgVerify(
    PROCESS *pVM,
    PVOID pvAddr,
    BOOL fProbeOnly,
    BOOL* pfPageInFailed
    )
{    
    void *pvRet = NULL;
    // Only query because this way the flag will be brought in, but not put into the lock list.
    DWORD dwVMLockPagesFlags = LOCKFLAG_QUERY_ONLY | LOCKFLAG_READ;
    DWORD dwPFN;

    if (pfPageInFailed)
    {
        *pfPageInFailed = FALSE;
    }
    
    if (pVM == NULL)
    { // Default to the current VM if KDstub or OsAccess sends in NULL
        pVM = pVMProc;
    }

    if (((DWORD)pvAddr) >= VM_SHARED_HEAP_BASE)
    { // Default to NK if address is above shared base.
        pVM = g_pprcNK;
    }

    pvRet = GetKAddrOfProcess(pVM, pvAddr);
    if (!pvRet && !fProbeOnly && g_fForcedPaging)
    { // Failed first time.  Probably not mapped in.  If caller wants more than probing, then lock the page and try again
        if (!IsKernelVa(pvAddr) && !pVM->csVM.OwnerThread)
        { // Not a kernel VA, not in a system call and the vm lock is free.
            if (VMLockPages(pVM, pvAddr, 1, &dwPFN, dwVMLockPagesFlags))
            { // Page is successfully locked.  Time to get the KAddr for real.
                pvRet = GetKAddrOfProcess(pVM, pvAddr);
                if (!pvRet && pfPageInFailed)
                {
                    *pfPageInFailed = TRUE;
                }
            }
            else if (pvAddr)
            {
                // Failed in locking a non-null address.
                DEBUGMSG(ZONE_DEBUGGER, (TEXT("  DebugVerify: Failed in VMLockPages (%d), %08X, %08X.\r\n"), KGetLastError(pCurThread), pVM, pvAddr));
            }
        }
    }
    return pvRet;
}
// END VM


// BEGIN HANDLE

static PHDATA KdpHandleToHDATA(HANDLE h, PHNDLTABLE phndtbl)
{
    if (phndtbl == NULL)
    {
        phndtbl = g_phndtblNK;
    }
    return h2pHDATA(h, phndtbl);
}

// END HANDLE

extern BOOL g_fForcedPaging;
extern BOOL (* __abnormal_termination)(VOID);
extern void FPUFlushContext(void);

BOOL kdpIsROM (LPVOID pvAddr, DWORD cbSize)
{
    BOOL fRet = FALSE;
    DWORD dwPfn = GetPFN ((LPVOID) ((DWORD)pvAddr&~VM_PAGE_OFST_MASK));
    if (INVALID_PHYSICAL_ADDRESS != dwPfn)
    {
        fRet = g_pOemGlobal->pfnIsRom (PA256FromPfn(dwPfn));
    }
    return fRet;
}


//------------------------------------------------------------------------------
// HDStub Fake interface (Runs when hd.dll is not loaded.)
//------------------------------------------------------------------------------
ULONG 
FakeHDException(
    PEXCEPTION_RECORD ExceptionRecord,
    CONTEXT *ContextRecord,
    DWORD ExceptionPhase
    ) 
{
    return FALSE;
}

static void FakeHDPageIn (DWORD dw, BOOL f)
{
}

static void FakeHDModLoad (DWORD dw)
{
}

static void FakeHDModUnload (DWORD dw)
{
}


//------------------------------------------------------------------------------
// HwTrap - This code is in the Kernel to give OsAccessH capability to set
//          HW breakpoint before the kernel is booted into memory.
//------------------------------------------------------------------------------

int g_iHwTrapCount=0; // Dummy - for optimization prevention see bellow

void HwTrap (void)
{
    DebugBreak ();
    g_iHwTrapCount++; // This make this function unique enough to prevent ICF optimization (with DoDebugBreak())
}


//------------------------------------------------------------------------------
// HDStub interface
//------------------------------------------------------------------------------
static BOOL s_fHdConnected = FALSE;
BOOL  (*g_pHdInit) (HDSTUB_INIT *) = NULL;
ULONG (*HDException) (EXCEPTION_RECORD *, CONTEXT *, DWORD) = FakeHDException;
void  (*HDPageIn) (DWORD, BOOL) = FakeHDPageIn;
void  (*HDModLoad) (DWORD) = FakeHDModLoad;
void  (*HDModUnload) (DWORD) = FakeHDModUnload;

// This pointer is still necessary because the OsAxsDataBlock is expecting the address of a pointer to the HWTrap
// notification.
void   *pvHDNotifyExdi = NULL;
DWORD  *pdwHDTaintedModuleCount = NULL;
HDSTUB_SHARED_GLOBALS *pHdSharedGlobals = NULL;
BOOL  (*HDConnectClient) (HDSTUB_CLINIT_FUNC, void *) = NULL;
HDSTUB_EVENT *g_pHdEvent = NULL;
// Hardware event filter.
ULONG g_ulHDEventFilter = 0;


//------------------------------------------------------------------------------
// OsAccess synchronization block
// Aligned on a 64byte boundary.  Makes it easier to find.
//------------------------------------------------------------------------------

#if defined (ARM) || defined (x86)
extern PADDRMAP g_pOEMAddressTable;
#endif

void HDCleanup ()
{
    g_pHdInit = NULL;
    HDException = FakeHDException;
    HDPageIn = FakeHDPageIn;
    HDModLoad = FakeHDModLoad;
    HDModUnload = FakeHDModUnload;
    pvHDNotifyExdi = NULL;
    pdwHDTaintedModuleCount = NULL;
    pHdSharedGlobals = NULL;
    HDConnectClient = NULL;
    g_pHdEvent = NULL;
    s_fHdConnected = FALSE;
}


static BOOL ConnectHdstub (void *pvUnused)
{
    BOOL fRet;
    HDSTUB_INIT hd = {
        sizeof(HDSTUB_INIT),
        INTERRUPTS_ENABLE,
        InitializeCriticalSection,
        DeleteCriticalSection,
        EnterCriticalSection,
        LeaveCriticalSection,
#ifdef MIPS
        InterlockedDecrement,
        InterlockedIncrement,
#endif
#ifdef  ARM
        InSysCall,
#endif        
        HDCleanup,
        CallKitlIoctl,
        NKwvsprintfW,
        g_pKData,
        g_pprcNK,
        &g_ulHDEventFilter,
        (void*)g_pKData->pOsAxsHwTrap->rgHwTrap,       // Since this is part of KData now, probably don't need to pass this.
        NKCacheRangeFlush,
    };

    if (!s_fHdConnected)
    {
        
        fRet = g_pHdInit && (*g_pHdInit) (&hd);

        if (fRet) 
        {
            memcpy (g_pKData->pOsAxsHwTrap->rgHwTrap, HwTrap, sizeof (g_pKData->pOsAxsHwTrap->rgHwTrap));
            pvHDNotifyExdi = g_pKData->pOsAxsHwTrap->rgHwTrap;
            
            g_pKData->pOsAxsHwTrap->dwPtrOsAxsKernPointers = (DWORD)g_pKData->pOsAxsDataBlock;

            g_pKData->pOsAxsHwTrap->dwImageBase = (ULONG) g_pprcNK->BasePtr;
            g_pKData->pOsAxsHwTrap->dwImageSize = g_pprcNK->e32.e32_vsize;
            g_pKData->pOsAxsHwTrap->dwRelocDataSec = ((COPYentry *) (pTOC->ulCopyOffset))->ulDest;
            g_pKData->pOsAxsHwTrap->dwDateSecLen = ((COPYentry *) (pTOC->ulCopyOffset))->ulDestLen;
            
            // Update the signature block so that OsAccess on host side can find it.
            g_pKData->pOsAxsDataBlock->fHdstubLoaded = TRUE;

            HDException = hd.pfnException;
            HDPageIn = hd.pfnVmPageIn;
            HDModLoad = hd.pfnModLoad;
            HDModUnload = hd.pfnModUnload;
            pdwHDTaintedModuleCount = hd.pdwTaintedModuleCount;
            pHdSharedGlobals = hd.pHdSharedGlobals;
            g_pHdEvent = hd.pEvent;
            HDConnectClient = hd.pfnConnectClient;

            s_fHdConnected = TRUE;
        }
    }
    else
    {
        DEBUGMSG(ZONE_DEBUGGER, (TEXT("  NKConnectHdstub: HD is already connected, ignoring connect request\r\n")));
        fRet = TRUE;
    }
    return fRet;
}


//------------------------------------------------------------------------------
// OsAxs common Interface
//------------------------------------------------------------------------------

BOOL s_fOsAxsT0Connected = FALSE;
BOOL (*g_pOsAxsT0Init) (struct _HDSTUB_DATA *, void *);
BOOL s_fOsAxsT1Connected = FALSE;


void OsaxsT0Cleanup ()
{
    g_pOsAxsT0Init = NULL;
    s_fOsAxsT0Connected = FALSE;
}


BOOL KD_EventModify (HANDLE hEvent, DWORD type)
{
    return (NKEventModify (pActvProc, hEvent, type));
}


static BOOL ConnectOsAxs(BOOL (*pInitFunc)(struct _HDSTUB_DATA *, void *))
{
    BOOL fRet;

    OSAXS_DATA osaxs = 
    {
        sizeof (OSAXS_DATA),
        INTERRUPTS_ENABLE,
        InitializeCriticalSection,
        DeleteCriticalSection,
        EnterCriticalSection,
        LeaveCriticalSection,
        g_pKData,
        &hCoreDll,
        &pCaptureDumpFileOnDevice,
        &pKCaptureDumpFileOnDevice,
        SystemAPISets,
        g_pprcNK,
        pTOC,
        LogPtr,
        g_pOemGlobal->pfnKDIoctl,
        NKKernelLibIoControl,
        NKCacheRangeFlush,
        kdpIsROM,
        DbgVerify,
        GetKAddrOfProcess,
        GetPFNOfProcess,
        DoThreadGetContext,
        NKGetThreadCallStack,
        KD_EventModify,
        CEInstructionSet,
#if defined(MIPS)
        InterlockedDecrement,
        InterlockedIncrement,
#endif
#ifdef x86
        _except_handler3,
        __abnormal_termination,
#else
        __C_specific_handler,
#endif
        NULL,                   // OEMGetRegDesc,
        NULL,                   // OEMReadRegs,
        NULL,                   // OEMWriteRegs

        KCall,
        NULL,
        NULL,
        NULL,
        NULL,
        CallKitlIoctl,
        NKwvsprintfW,
        KdpHandleToHDATA,
        GetProcPtr,
        Pfn2Virt,
        SwitchActiveProcess,
        SwitchVM,
        Entry2PTBL
    };

#if defined (SHx) && !defined (SH4) && !defined (SH3e)
        osaxs.DSPFlushContext = DSPFlushContext;
#endif

#if defined (SH4) || defined (ARM) || defined (MIPS_HAS_FPU) || defined (x86)
        osaxs.FPUFlushContext = FPUFlushContext;
#endif

#ifdef x86
        osaxs.ppCurFPUOwner = &g_CurFPUOwner;
        osaxs.pdwProcessorFeatures = &ProcessorFeatures;
#endif

    fRet = pInitFunc && HDConnectClient &&
        HDConnectClient ((HDSTUB_CLINIT_FUNC) pInitFunc, &osaxs);

    return fRet;
}


static BOOL ConnectOsAxsT0(void *pvUnused)
{
    BOOL fRet = FALSE;

    if (!s_fOsAxsT0Connected)
    {
        fRet = ConnectOsAxs(g_pOsAxsT0Init);
        if (fRet)
        {
            s_fOsAxsT0Connected = TRUE;
        }
    }
    else
    {
        DEBUGMSG(ZONE_DEBUGGER, (TEXT("  ConnectOsAxsT0: OsAxsT0 is already attached, ignoring connect request.\r\n")));
        fRet = TRUE;
    }

    return fRet;
}


//------------------------------------------------------------------------------
// OsAxsT1 Interface
//------------------------------------------------------------------------------


BOOL (*g_pOsAxsT1Init)(struct _HDSTUB_DATA *, void *);


void OsaxsT1Cleanup()
{
    g_pOsAxsT1Init = NULL;
    s_fOsAxsT1Connected = FALSE;
}


static BOOL ConnectOsAxsT1 (void *pvUnused)
{
    BOOL fRet = FALSE;

    if (!s_fOsAxsT1Connected)
    {
        fRet = ConnectOsAxs (g_pOsAxsT1Init);

        if (fRet)
        {
            s_fOsAxsT1Connected = TRUE;
        }
    }
    else
    {
        DEBUGMSG(ZONE_DEBUGGER, (TEXT("  NKConnectOsAxsT1: OsAxsT1 is already attached, ignoring connect request.\r\n")));
        fRet = TRUE;
    }
    return fRet;
}


//------------------------------------------------------------------------------
// Placeholder for when debugger is not present
//------------------------------------------------------------------------------
static BOOL
FakeKDSanitize(
    BYTE* pbClean,
    VOID* pvMem,
    ULONG nSize,
    BOOL  fAlwaysCopy
    )
{
    PREFAST_SUPPRESS(419, "This function has the same semantics as memcpy.")
    if (fAlwaysCopy) memcpy(pbClean, pvMem, nSize);
    return FALSE;
}

//------------------------------------------------------------------------------
// Placeholder for when debugger is not present
//------------------------------------------------------------------------------
static void 
FakeKDReboot(
    BOOL fReboot
    ) 
{
    return;
}

/* Kernel Debugger interface pointers */
BOOL  (*g_pKdInit)(KDSTUB_INIT* pKernData) = NULL;
BOOL  (*KDSanitize)(BYTE* pbClean, VOID* pvAddrMem, ULONG nSize, BOOL fAlwaysCopy) = FakeKDSanitize;
VOID  (*KDReboot)(BOOL fReboot) = FakeKDReboot;

//------------------------------------------------------------------------------
// This function is called when kdstub gets Terminate Message from the debugger
// It simply reverts back KDSanitize and KDReboot function 
// pointers to FAKE functions.
//------------------------------------------------------------------------------
BOOL
KDCleanup(void)
{
    KDSanitize      = FakeKDSanitize;
    KDReboot        = FakeKDReboot;
    g_fKDebuggerLoaded = FALSE;
    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


// RestorePrio & KDEnableInt are used by KD to boost thread priority and
// enable and disable interrupts
int g_cInterruptsOff = 0;


static void RestorePrio (SAVED_THREAD_STATE *psvdThread)
{
    // doesn't do KCall profiling here since this is part
    // of the debugger

    if (!psvdThread) return;
    if (!psvdThread->fSaved)
    {
        DEBUGMSG (1, (L"Calling KDEnableInt(TRUE, ...) with invalid psvdThread!!!\r\n"));
        return;
    }    
    
    pCurThread->dwQuantum = psvdThread->dwQuantum;
    psvdThread->fSaved = FALSE;

    SetThreadBasePrio (pCurThread, psvdThread->bBPrio);
}


static void KDEnableInt (BOOL fEnable, SAVED_THREAD_STATE *psvdThread)
{        
    if (!pCurThread)
    {
        DEBUGMSG (1, (L"KDEnableInt with pCurThread == 0!!!\r\n"));
        return;
    }
    
    if (fEnable)
    {
        if (psvdThread)
        {
            if (g_cInterruptsOff < 1)
            {
                DEBUGMSG (1, (L"Calling KDEnableInt(TRUE, ...) without first calling KDEnableInt(FALSE, ...)!!!\r\n"));
            }
            else
            {
                --g_cInterruptsOff;
            }
            KCall ((FARPROC) RestorePrio, psvdThread);
        }
        INTERRUPTS_ENABLE (TRUE); // KCall should turn interrupts on for us, this may not be necessary
    } 
    else
    {

        if (psvdThread)
        {
            INTERRUPTS_ENABLE (FALSE); // Do not use INTERRUPTS_OFF, it will cause problems with MIPIV FP registers
            if (g_cInterruptsOff)
            {
                DEBUGMSG (1, (L"Recursively calling KDEnableInt(FALSE, psvdThread != NULL) %i time(s). This OK if KdStub stumbling on its own BP.\r\n", g_cInterruptsOff));
            }
            psvdThread->bCPrio = (BYTE)GET_CPRIO (pCurThread);
            psvdThread->bBPrio = (BYTE)GET_BPRIO (pCurThread);
            psvdThread->dwQuantum = pCurThread->dwQuantum;
            psvdThread->fSaved = TRUE;

            pCurThread->dwQuantum = 0;
            SET_CPRIO (pCurThread, 0);
            SET_BPRIO (pCurThread, 0);
            ++g_cInterruptsOff;
        } 
        else 
        {
            if (!g_cInterruptsOff) 
            {
                DEBUGMSG (1, (L"Calling KDEnableInt (FALSE, NULL) without previously calling KDEnableInt (FALSE, p)\r\n"));;
            } 
            else 
            {
                INTERRUPTS_ENABLE (FALSE); // Do not use INTERRUPTS_OFF, it will cause problems with MIPIV FP registers
            }
        }
        //INTERRUPTS_ON();      // un-comment this line if we want KD running with interrupt on
    }
}


void kdpInvalidateRange (PVOID pvAddr,  ULONG ulSize)
{
    // just flush TLB
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_FLUSH_TLB);
}

BOOL IsDesktopDbgrExist (void)
{
    return g_pNKGlobal->pfnKITLIoctl (IOCTL_KITL_IS_KDBG_REGISTERED, NULL, 0, NULL, 0, NULL);
}


extern CRITICAL_SECTION ModListcs;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL 
ConnectKD (
    LPVOID pInit  // ignored
    ) 
{
    BOOL fRet;

    KDSTUB_INIT kd =
    {
        sizeof(KDSTUB_INIT),
        INTERRUPTS_ENABLE,
        InitializeCriticalSection,
        DeleteCriticalSection,
        EnterCriticalSection,
        LeaveCriticalSection,

        // OUT params (pass default values, so that Kd can ignore partially some of them)
        KDSanitize,
        KDReboot,

        // IN params
        pTOC,
        ROMChain,
        g_pprcNK,
        g_pKData,
        &hCoreDll,
        KDCleanup,
        KDEnableInt,
        &pCaptureDumpFileOnDevice,
        &pKCaptureDumpFileOnDevice,
        IsDesktopDbgrExist,
        NKwvsprintfW,
        NKDbgPrintfW,
        KCall,
        DbgVerify,
        kdpInvalidateRange,
        kdpIsROM,
        DBG_CallCheck,
        g_pOemGlobal->pfnKDIoctl,        
        CallKitlIoctl,
        FALSE, // fFPUPresent
        FALSE, // fDSPPresent
        &g_fForcedPaging,
        NKCacheRangeFlush,
        &g_ulHDEventFilter,
        (void*)&g_pKData->pOsAxsHwTrap->rgHwTrap,
        NKIsProcessorFeaturePresent,
        Pfn2Virt,
        KdpHandleToHDATA,
        Entry2PTBL,
        // ...
    };

    DEBUGMSG(ZONE_DEBUGGER, (TEXT("Entering ConnectKD\r\n")));

#ifdef SHx
    kd.p__C_specific_handler   = __C_specific_handler;
#if defined(SH4)
    kd.FPUFlushContext         = FPUFlushContext;
    kd.fFPUPresent             = TRUE;
#elif !defined(SH3e)
    kd.DSPFlushContext         = DSPFlushContext;
#if defined(SH3)
    kd.fDSPPresent             = (1 == SH3DSP);
#endif
#endif

#elif  MIPS
    kd.pInterlockedDecrement   = InterlockedDecrement;
    kd.pInterlockedIncrement   = InterlockedIncrement;
    kd.p__C_specific_handler   = __C_specific_handler;
#if defined(MIPS_HAS_FPU)
    kd.FPUFlushContext         = FPUFlushContext;
    kd.fFPUPresent             = TRUE;
#endif

#elif  ARM
    kd.p__C_specific_handler   = __C_specific_handler;
    kd.pInSysCall              = InSysCall;
    kd.FPUFlushContext         = FPUFlushContext;
    kd.fFPUPresent             = (1 == vfpStat);

#elif  x86
    kd.p__abnormal_termination = __abnormal_termination;
    kd.p_except_handler3       = _except_handler3;
    kd.FPUFlushContext         = FPUFlushContext;
    kd.ppCurFPUOwner           = &g_CurFPUOwner;
    kd.pdwProcessorFeatures    = &ProcessorFeatures;
    kd.fFPUPresent             = TRUE;
#else
    #pragma message("ERROR: ConnectKD is not supported on this CPU!")
    ERRORMSG(1, (TEXT("ConnectKD is not supported on this CPU!\r\n")));
    return FALSE;
#endif

    if (!g_fKDebuggerLoaded)
    {
        // Grab loader CS
        EnterCriticalSection (&ModListcs);

        if (fRet = (!g_fNoKDebugger && g_pKdInit && HDConnectClient &&
            HDConnectClient ((HDSTUB_CLINIT_FUNC)g_pKdInit, &kd)))
        {
            KDSanitize      = kd.pKdSanitize;
            KDReboot        = kd.pKdReboot;
            HDModLoad ((DWORD) g_pprcNK); // Initial NK.EXE module load notification
            //ReadyForStrings = TRUE;
            g_fKDebuggerLoaded = TRUE;
        } else {
            ERRORMSG(1, (TEXT("ConnectKD failed\r\n")));
        }
        
        LeaveCriticalSection (&ModListcs);
    }
    else
    {
        DEBUGMSG(ZONE_DEBUGGER, (TEXT("  ConnectKD: Debugger is already connected, ignoring connect request.\r\n")));
        fRet = TRUE;
    }

    return fRet;
}


//------------------------------------------------------------------------------
// SysDebugInit - System Debugger intialization 
//  -HW Debug stub
//  -Kernel dump capture
//  -SW Kernel Debug stub
//------------------------------------------------------------------------------


BOOL fSysStarted;

static BOOL DoAttachDebugger (LPCWSTR *pszDbgrNames, int nNames)
{
    // HDSTUB (hd.dll) = 
    // Low level functions common to KDump capture (OSAXST0 required), SW System Debugging (KDSTUB also required) and HW Assisted System debugging (no other stub needed)

    //
    // hdstub is required (hardware or software debugging)
    // osaxst0 - KDump capture (optional)
    // osasxt1 - KDump capture (optional)
    // kd - software debugging (optional)
    //

    BOOL   fRet = (LoadKernelLibrary (TEXT ("hd.dll"))
                && g_pHdInit
                && ConnectHdstub ((void*)g_pHdInit));

    if (fRet) {

        HANDLE hKdLib;
        int    idx;
        
        // OSAXST0 (osaxst0.dll) = 
        // Target-side OS Awareness functions common to KDump capture, SW System Debugging (KDSTUB also required) - HDSTUB is required
        if (LoadKernelLibrary (TEXT ("osaxst0.dll"))) // load OsAxsT0 if present
        {
            if (g_pOsAxsT0Init) // This function pointer should be initialized by the DllMain function, via a callback to KernelLibIoControl_Dbg
            {
                DEBUGMSG(ZONE_DEBUGGER, (TEXT("OsaxsT0 loaded\r\n")));
                ConnectOsAxsT0 (NULL);
            }
        }

        // OSAXST1 (osaxst1.dll) = 
        // Target-side OS Awareness functions specific to SW System Debugging (KDSTUB also required)
        if (LoadKernelLibrary (TEXT ("osaxst1.dll"))) // load OsAxsT0 if present
        {
            if (g_pOsAxsT1Init) // This function pointer should be initialized by the DllMain function, via a callback to KernelLibIoControl_Dbg
            {
                DEBUGMSG(ZONE_DEBUGGER, (TEXT("OsaxsT1 loaded\r\n")));
                ConnectOsAxsT1 (NULL);
            }
        }

        // KDSTUB (kd.dll) = 
        // Software System Debugging probe functions (Kernel debugger stub) - HDSTUB + OSAXST0 + OSAXST1 are required
        for (idx = 0; idx < nNames; idx++)
        {
            if (hKdLib = LoadKernelLibrary (pszDbgrNames[idx]))
            {
                DEBUGMSG(ZONE_DEBUGGER, (TEXT("Debugger '%s' loaded\r\n"), pszDbgrNames[idx]));
                if (g_pKdInit) // This function pointer should be initialized by the DllMain function, via a callback to KernelLibIoControl_Dbg
                {
                    fRet = ConnectKD ((VOID*)g_pKdInit);
                }
                else
                {
                    ERRORMSG(1, (TEXT("'%s' is not a debugger DLL\r\n"), pszDbgrNames[idx]));                   
                    fRet = FALSE;
                }
                break;
            }
        }

    }

    return fRet;
}

BOOL NKAttachDebugger (LPCWSTR pszDbgrName)
{
    DWORD dwPriv = CEPRI_LOAD_KERNEL_LIBRARY;
    BOOL fRet = FALSE;

    if (SimplePrivilegeCheck (CEPRI_LOAD_KERNEL_LIBRARY)) {

        WCHAR szLocalName[MAX_PATH];
        PPROCESS pprc;

        // use local copy of name for security reason (can except, but okay as we don't hold any lock
        if (!CopyPath (szLocalName, pszDbgrName, MAX_PATH)) {
            NKSetLastError (ERROR_BAD_PATHNAME);
            
        } else {

            pszDbgrName = szLocalName;
            pprc = SwitchActiveProcess (g_pprcNK);
            fRet = DoAttachDebugger (&pszDbgrName, 1);
            SwitchActiveProcess (pprc);
        }
    } else {
        NKSetLastError (ERROR_ACCESS_DENIED);
    }
    return fRet;
}

void SysDebugInit (void) 
{
    LPCWSTR rgpszKdLibNames[] = // this is not really useful
    {
        TEXT("kd.dll")
        // more can be added in order of descending preference
    };

    DEBUGCHK(g_pKData->pOsAxsDataBlock);
    DEBUGCHK(g_pKData->pOsAxsHwTrap);

    pvHDNotifyExdi = g_pKData->pOsAxsHwTrap->rgHwTrap;

    g_pKData->pOsAxsDataBlock->dwPtrKDataOffset = (DWORD)g_pKData;
    g_pKData->pOsAxsDataBlock->dwProcArrayOffset = (DWORD)g_pprcNK;
    g_pKData->pOsAxsDataBlock->dwPtrHdstubNotifOffset = (DWORD)&pvHDNotifyExdi;
    g_pKData->pOsAxsDataBlock->dwPtrHdstubEventOffset = (DWORD)&g_pHdEvent;
    g_pKData->pOsAxsDataBlock->dwPtrHdstubTaintOffset = (DWORD)&pdwHDTaintedModuleCount;
    g_pKData->pOsAxsDataBlock->dwPtrHdstubFilterOffset = (DWORD)&g_ulHDEventFilter;
    g_pKData->pOsAxsDataBlock->dwSystemAPISetsOffset = (DWORD)&SystemAPISets[0];
    g_pKData->pOsAxsDataBlock->dwMemoryInfoOffset = (DWORD)&MemoryInfo;
    g_pKData->pOsAxsDataBlock->dwPtrRomHdrOffset = (DWORD)&pTOC;
#if defined(ARM)||defined(x86)
    g_pKData->pOsAxsDataBlock->dwPtrOEMAddressTable = (DWORD)&g_pOEMAddressTable;
#endif
    g_pKData->pOsAxsDataBlock->dwPtrHdSharedGlobals = (DWORD)&pHdSharedGlobals;
    // TODO: Find out if there is a constant that can be used for this
    g_pKData->pOsAxsDataBlock->dwOsMajorVersion = 6;
    g_pKData->pOsAxsDataBlock->dwOsMinorVersion = 0;
    g_pKData->pOsAxsDataBlock->dwOsAxsSubVersion = OSAXS_SUBVER_CURRENT;
    DEBUGMSG(ZONE_DEBUGGER, (L"  SysDebugInit: OsAxs Kernel Pointers Structure initialized.\r\n"));
    
    fSysStarted = TRUE;

    DoAttachDebugger (rgpszKdLibNames, sizeof(rgpszKdLibNames)/sizeof(LPCWSTR));
}


