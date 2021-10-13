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
#include <kernel.h>
#include <osaxs_common.h>
#include <spinlock.h>
#include <bldver.h>

#include <kdbgpublic.h>

volatile SPINLOCK *g_pKitlSpinLock;

BOOL g_fForcedPaging = FALSE; // This is global because DbgVerify is called from DBG_CallCheck too
BOOL IsDesktopDbgrExist(void);
BOOL KDCleanup(void);
void kdpInvalidateRange(PVOID pvAddr,  ULONG ulSize);
DWORD NKSnapshotSMPStart();

const WCHAR* g_wszKdDll = L"kd.dll";

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

BOOL kdpIsROM (PPROCESS pprc, LPVOID pvAddr, DWORD cbSize)
{
    BOOL fRet = FALSE;
    DWORD dwPfn = GetPFNOfProcess (pprc, (LPVOID) ((DWORD)pvAddr&~VM_PAGE_OFST_MASK));
    if (INVALID_PHYSICAL_ADDRESS != dwPfn)
    {       
        fRet = g_pOemGlobal->pfnIsRom (PA256FromPfn(dwPfn));
    }
    return fRet;
}




//------------------------------------------------------------------------------
// HDStub Fake interface (Runs when hd.dll is not loaded.)
//------------------------------------------------------------------------------
BOOL 
FakeHDException(
    PEXCEPTION_RECORD ExceptionRecord,
    CONTEXT *ContextRecord,
    BOOLEAN SecondChance
    ) 
{
    return FALSE;
}

static void FakeHDPageIn (DWORD dw, BOOL f)
{
}

static void FakeHDPageOut (DWORD dw, DWORD dw1)
{
}

static void FakeHDModLoad (DWORD dw, BOOL f)
{
}

static void FakeHDModUnload (DWORD dw, BOOL f)
{
}

static BOOL FakeHDIsDataBreakpoint (DWORD dw, BOOL f)
{
    return FALSE;
}

static void FakeHDStopAllCpuCB (void)
{
}

// arm\armtrap.s
// sh\shexcept.src
// mips\except.s
// x86\fault.c
extern void HwTrap(struct _HDSTUB_EVENT2 *pEvent);


//------------------------------------------------------------------------------
// HDStub interface
//------------------------------------------------------------------------------
static BOOL s_fHdConnected = FALSE;
BOOL  (*g_pHdInit)(KDBG_INIT *) = NULL;
BOOL  (*HDException) (PEXCEPTION_RECORD, CONTEXT *, BOOLEAN) = FakeHDException;
void  (*HDPageIn) (DWORD, BOOL) = FakeHDPageIn;
void  (*HDPageOut) (DWORD, DWORD) = FakeHDPageOut;
void  (*HDModLoad) (DWORD, BOOL) = FakeHDModLoad;
void  (*HDModUnload) (DWORD, BOOL) = FakeHDModUnload;
BOOL  (*HDIsDataBreakpoint) (DWORD, BOOL) = FakeHDIsDataBreakpoint;
void  (*HDStopAllCpuCB) (void) = FakeHDStopAllCpuCB;

// This pointer is still necessary because the OsAxsDataBlock is expecting the address of a pointer to the HWTrap
// notification.
void   *pvHDNotifyExdi = NULL;
HDSTUB_SHARED_GLOBALS *pHdSharedGlobals = NULL;
BOOL  (*HDConnectClient)(KDBG_DLLINITFN, void *) = NULL;
// Hardware event filter.
ULONG g_ulHDEventFilter = 0;


void HDCleanup ()
{
    g_pHdInit = NULL;
    HDException = FakeHDException;
    HDPageIn = FakeHDPageIn;
    HDPageOut = FakeHDPageOut;
    HDModLoad = FakeHDModLoad;
    HDModUnload = FakeHDModUnload;
    HDIsDataBreakpoint = FakeHDIsDataBreakpoint;
    HDStopAllCpuCB = FakeHDStopAllCpuCB;
    pvHDNotifyExdi = NULL;
    pHdSharedGlobals = NULL;
    HDConnectClient = NULL;
    s_fHdConnected = FALSE;
}


BOOL KD_EventModify (HANDLE hEvent, DWORD type)
{
    return (NKEventModify (pActvProc, hEvent, type));
}

BOOL ConnectKDBGDll (KDBG_DLLINITFN pfnInit, BOOL *pfConnected)
{
    if (!*pfConnected)
    {
        if (HDConnectClient)
        {
            *pfConnected = HDConnectClient(pfnInit, NULL);
        }
    }

    return *pfConnected;
}

//------------------------------------------------------------------------------
// OsAxs common Interface
//------------------------------------------------------------------------------

BOOL s_fOsAxsT0Connected = FALSE;
KDBG_DLLINITFN g_pOsAxsT0Init;
KDBG_DLLINITFN g_pOsAxsT1Init;
BOOL s_fOsAxsT1Connected = FALSE;


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

//
// Stub out Ignore Illegal Instruction.  This is important on ARM.
//
static BOOL FakeKDIgnoreIllegalInstruction(DWORD unused)
{
    return FALSE;
}

/* Kernel Debugger interface pointers */
KDBG_DLLINITFN g_pKdInit;
BOOL  (*KDSanitize)(BYTE* pbClean, VOID* pvAddrMem, ULONG nSize, BOOL fAlwaysCopy) = FakeKDSanitize;
BOOL  (*KDIgnoreIllegalInstruction)(DWORD) = FakeKDIgnoreIllegalInstruction;

VOID (*KDPostInit)() = NULL;
extern void FakeKDReboot(BOOL fReboot); 

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
    KDIgnoreIllegalInstruction = FakeKDIgnoreIllegalInstruction;
    g_fKDebuggerLoaded = FALSE;
    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

void kdpInvalidateRange (PVOID pvAddr,  ULONG ulSize)
{
    // just flush TLB
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_FLUSH_TLB);
}

BOOL IsDesktopDbgrExist (void)
{
    return g_pNKGlobal->pfnKITLIoctl (IOCTL_KITL_IS_KDBG_REGISTERED, NULL, 0, NULL, 0, NULL);
}


static BOOL NKConnectHdstub (void)
{
    KDBG_INIT ki;
    BOOL fRet;

    memset(&ki, 0, sizeof(ki));
    ki.sig0 = KDBG_INIT_SIG0;
    ki.sig1 = KDBG_INIT_SIG1;
    ki.sig2 = KDBG_INIT_SIG2;

    ki.pKData = g_pKData;
    ki.pprcNK = g_pprcNK;
    ki.rgpPCBs = g_ppcbs;
    ki.cPCBs = MAX_CPU;
#ifdef x86
    ki.pdwProcessorFeatures = &ProcessorFeatures;
#endif

    ki.dwCEInstructionSet = CEInstructionSet;
    ki.ppCaptureDumpFileOnDevice = (PPVOID)&pCaptureDumpFileOnDevice;
    ki.ppKCaptureDumpFileOnDevice = (PPVOID)&pKCaptureDumpFileOnDevice;
    ki.pKitlSpinLock = g_pKitlSpinLock;
    ki.pSchedLock = &g_schedLock;
    ki.pPhysLock = &g_physLock;
    ki.pOalLock = &g_oalLock;
    ki.pulHDEventFilter = &g_ulHDEventFilter;
    ki.pSystemAPISets = SystemAPISets;
    ki.pRomHdr = pTOC;
    ki.pLogPtr = LogPtr;
    ki.phCoreDll = &hCoreDll;
    ki.phKCoreDll = &hKCoreDll;
#if defined(x86) || defined(ARM)
    ki.ppKHeapBase = &g_pKHeapBase;
    ki.ppCurKHeapFree = &g_pCurKHeapFree;
    ki.pOEMAddressTable = g_pOEMAddressTable;
#endif
    ki.pfIntrusivePaging = &g_fForcedPaging;
    ki.pfnINTERRUPTS_ENABLE = INTERRUPTS_ENABLE;
    ki.pfnAcquireSpinLock = AcquireSpinLock;
    ki.pfnReleaseSpinLock = ReleaseSpinLock;
    ki.pfnDeleteCriticalSection = DeleteCriticalSection;
    ki.pfnEnterCriticalSection = EnterCriticalSection;
    ki.pfnInitializeCriticalSection = InitializeCriticalSection;
    ki.pfnLeaveCriticalSection = LeaveCriticalSection;
    ki.pfnDoThreadGetContext = SCHL_DoThreadGetContext;
    ki.pfnMDCaptureFPUContext = MDCaptureFPUContext;
    ki.pfnEventModify = KD_EventModify;
    ki.pfnEVNTModify = EVNTModify;
    ki.pfnGetPageTable = GetPageTable;
    ki.pfnGetPFNOfProcess = GetPFNOfProcess;
    ki.pfnVMCreateKernelPageMapping = VMCreateKernelPageMapping;
    ki.pfnVMRemoveKernelPageMapping = VMRemoveKernelPageMapping;
    ki.pfnGetProcPtr = GetProcPtr;
    ki.pfnh2pHDATA = h2pHDATA;
    ki.pfnHDCleanup = HDCleanup;
    ki.pKDCleanup = KDCleanup;
#pragma warning(suppress:4055)
    ki.pfnHwTrap = (void (*)(struct _HDSTUB_EVENT2 *))g_pKData->pOsAxsHwTrap->rgHwTrap;
#ifdef ARM
    ki.pfnInSysCall = InSysCall;
#endif
#ifndef x86
    ki.pfnInterlockedDecrement = InterlockedDecrement;
    ki.pfnInterlockedIncrement = InterlockedIncrement;
#endif
    ki.pfnIsDesktopDbgrExist = IsDesktopDbgrExist;
    ki.pfnKCall = KCall;
    ki.pfnKdpInvalidateRange = kdpInvalidateRange;
    ki.pfnKdpIsROM = kdpIsROM;
    ki.pfnKITLIoctl = CallKitlIoctl;
    ki.pfnNKCacheRangeFlush = NKCacheRangeFlush;
    ki.pfnNKRegisterDbgZones = NKRegisterDbgZones;
    ki.pfnNKvDbgPrintfW = NKvDbgPrintfW;
    ki.pfnNKGetThreadCallStack = NKGetThreadCallStack;
    ki.pfnNKIsProcessorFeaturePresent = NKIsProcessorFeaturePresent;
    ki.pfnNKKernelLibIoControl = NKKernelLibIoControl;
    ki.pfnNKwvsprintfW = NKwvsprintfW;
    ki.pfnNKLoadKernelLibrary = NKLoadKernelLibrary;
    ki.pfnGetProcAddressA = GetProcAddressA;
    ki.pfnOEMKDIoControl = g_pOemGlobal->pfnKDIoctl;
    ki.pfnPfn2Virt = Pfn2Virt;
    ki.pfnResumeAllOtherCPUs = ResumeAllOtherCPUs;
    ki.pfnStopAllOtherCPUs = StopAllOtherCPUs;
    ki.pfnSCHL_SetThreadBasePrio = SCHL_SetThreadBasePrio;
    ki.pfnSCHL_SetThreadQuantum = SCHL_SetThreadQuantum; 
    ki.pfnSwitchActiveProcess = SwitchActiveProcess;
    ki.pfnSwitchVM = SwitchVM;
    ki.pfnVMAlloc = VMAlloc;
    ki.pfnVMFreeAndRelease = VMFreeAndRelease;
    ki.pfnMakeWritableEntry = MakeWritableEntry;
    ki.pfnOEMGetTickCount = OEMGetTickCount;
#ifdef x86
    ki.p_except_handler3 = _except_handler3;
    ki.p__abnormal_termination = __abnormal_termination;
#else
    ki.p__C_specific_handler = __C_specific_handler;
#endif
    ki.pfnNKSnapshotSMPStart = NKSnapshotSMPStart;

    if (!s_fHdConnected)
    {
        fRet = g_pHdInit && (*g_pHdInit)(&ki);

        if (fRet) 
        {
            memcpy (g_pKData->pOsAxsHwTrap->rgHwTrap, (LPCVOID) (DWORD) HwTrap, sizeof (g_pKData->pOsAxsHwTrap->rgHwTrap));
            pvHDNotifyExdi = g_pKData->pOsAxsHwTrap->rgHwTrap;
            g_pKData->pOsAxsHwTrap->dwPtrOsAxsKernPointers = (DWORD)g_pKData->pOsAxsDataBlock;
            g_pKData->pOsAxsHwTrap->dwImageBase = (ULONG) g_pprcNK->BasePtr;
            g_pKData->pOsAxsHwTrap->dwImageSize = g_pprcNK->e32.e32_vsize;
            g_pKData->pOsAxsHwTrap->dwRelocDataSec = ((COPYentry *) (pTOC->ulCopyOffset))->ulDest;
            g_pKData->pOsAxsHwTrap->dwDateSecLen = ((COPYentry *) (pTOC->ulCopyOffset))->ulDestLen;

            if (ki.pfnKdSanitize != NULL)
            {
                KDSanitize = ki.pfnKdSanitize;
            }
            if (ki.pfnKdReboot != NULL)
            {
                KDReboot = ki.pfnKdReboot;
            }
            if (ki.pfnKdPostInit != NULL)
            {
                KDPostInit = ki.pfnKdPostInit;
            }
            if (ki.pfnKdIgnoreIllegalInstruction != NULL)
            {
                KDIgnoreIllegalInstruction = ki.pfnKdIgnoreIllegalInstruction;
            }
 
            // Update the signature block so that OsAccess on host side can find it.
            g_pKData->pOsAxsDataBlock->fHdstubLoaded = TRUE;
            HDException             = ki.pfnException;
            HDPageIn                = ki.pfnVmPageIn;
            HDPageOut               = ki.pfnVmPageOut;
            HDModLoad               = ki.pfnModLoad;
            HDModUnload             = ki.pfnModUnload;
            HDIsDataBreakpoint      = ki.pfnIsDataBreakpoint;
            HDStopAllCpuCB          = ki.pfnStopAllCpuCB;
            pHdSharedGlobals        = ki.pHdSharedGlobals;

            HDConnectClient         = ki.pfnConnectClient;

            s_fHdConnected = TRUE;
            NKCacheRangeFlush(NULL, 0, CACHE_SYNC_ALL);

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
// SysDebugInit - System Debugger intialization 
//  -HW Debug stub
//  -Kernel dump capture
//  -SW Kernel Debug stub
//------------------------------------------------------------------------------


BOOL fSysStarted;

BOOL DoAttachDebugger (LPCWSTR *pszDbgrNames, int nNames)
{
    // HDSTUB (hd.dll) = 
    // Low level functions common to KDump capture (OSAXST0 required), SW System Debugging (KDSTUB also required) and HW Assisted System debugging (no other stub needed)

    //
    // hdstub is required (hardware or software debugging)
    // osaxst0 - KDump capture (optional)
    // osasxt1 - KDump capture (optional)
    // kd - software debugging (optional)
    //

    BOOL   fRet = LoadKernelLibrary (TEXT ("hd.dll"))
               && g_pHdInit
               && NKConnectHdstub ();

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
                ConnectKDBGDll(g_pOsAxsT0Init, &s_fOsAxsT0Connected);
            }
        }

        // OSAXST1 (osaxst1.dll) = 
        // Target-side OS Awareness functions specific to Action points.
        if (LoadKernelLibrary (TEXT ("osaxst1.dll"))) // load OsAxsT0 if present
        {
            if (g_pOsAxsT1Init) // This function pointer should be initialized by the DllMain function, via a callback to KernelLibIoControl_Dbg
            {
                DEBUGMSG(ZONE_DEBUGGER, (TEXT("OsaxsT1 loaded\r\n")));
                ConnectKDBGDll(g_pOsAxsT1Init, &s_fOsAxsT1Connected);
            }
        }

        // KDSTUB (kd.dll) = 
        // Software System Debugging probe functions (Kernel debugger stub) - HDSTUB + OSAXST0 + OSAXST1 are required
        for (idx = 0; idx < nNames; idx++)
        {
            hKdLib = LoadKernelLibrary(pszDbgrNames[idx]);
            if(hKdLib != NULL)
            {
                DEBUGMSG(ZONE_DEBUGGER, (TEXT("Debugger '%s' loaded\r\n"), pszDbgrNames[idx]));
                if (g_pKdInit) // This function pointer should be initialized by the DllMain function, via a callback to KernelLibIoControl_Dbg
                {
                    fRet = ConnectKDBGDll(g_pKdInit, &g_fKDebuggerLoaded);
                }
                else
                {
                    ERRORMSG(1, (TEXT("'%s' is not a debugger DLL\r\n"), pszDbgrNames[idx]));                   
                    fRet = FALSE;
                }
                break;
            }
        }

        if (fRet)
        {
            HDModLoad((DWORD)g_pprcNK, TRUE);
        }
    }

    return fRet;
}

BOOL NKAttachDebugger (LPCWSTR pszDbgrName)
{
    BOOL fRet = FALSE;
    WCHAR szLocalName[MAX_PATH];
    PPROCESS pprc;

    if (!CopyPath (szLocalName, pszDbgrName, MAX_PATH)) {
        // failed to make local copy of name for security reason (can except, but okay as we don't hold any lock
        NKSetLastError (ERROR_BAD_PATHNAME);
        
    } else {
        pszDbgrName = szLocalName;
        pprc = SwitchActiveProcess (g_pprcNK);
        fRet = DoAttachDebugger (&pszDbgrName, 1);
        SwitchActiveProcess (pprc);
    }

    return fRet;
}

void SysDebugInit (void) 
{
    OSAXS_KERN_POINTERS_2 *kernptrs = g_pKData->pOsAxsDataBlock;

    DEBUGCHK(g_pKData->pOsAxsDataBlock);
    DEBUGCHK(g_pKData->pOsAxsHwTrap);

    pvHDNotifyExdi = g_pKData->pOsAxsHwTrap->rgHwTrap;
    
    kernptrs->dwPtrKDataOffset         = (DWORD)g_pKData;
    kernptrs->dwProcArrayOffset        = (DWORD)g_pprcNK;
    kernptrs->dwPtrHdstubNotifOffset   = (DWORD)&pvHDNotifyExdi;
    kernptrs->dwPtrHdstubFilterOffset  = (DWORD)&g_ulHDEventFilter;
    kernptrs->dwSystemAPISetsOffset    = (DWORD)&SystemAPISets[0];
    kernptrs->dwMemoryInfoOffset       = (DWORD)&MemoryInfo;
    kernptrs->dwPtrRomHdrOffset        = (DWORD)&pTOC;
#if defined(ARM)||defined(x86)
    kernptrs->dwPtrOEMAddressTable     = (DWORD)&g_pOEMAddressTable;
#endif
    kernptrs->dwPtrHdSharedGlobals     = (DWORD)&pHdSharedGlobals;
    kernptrs->dwOsMajorVersion         = CE_MAJOR_VER;
    kernptrs->dwOsMinorVersion         = CE_MINOR_VER;
    kernptrs->dwOsAxsSubVersion        = OSAXS_SUBVER_CURRENT;
    kernptrs->dwPointerToPcbArray      = (DWORD)g_ppcbs;
    kernptrs->dwMaximumPcbs            = MAX_CPU;
    kernptrs->dwImageBase = (ULONG) g_pprcNK->BasePtr;
    kernptrs->dwImageSize = g_pprcNK->e32.e32_vsize;
    kernptrs->dwRelocDataSec = ((COPYentry *) (pTOC->ulCopyOffset))->ulDest;
    kernptrs->dwDataSecLen = ((COPYentry *) (pTOC->ulCopyOffset))->ulDestLen;

    DEBUGMSG(ZONE_DEBUGGER, (L"  SysDebugInit: OsAxs Kernel Pointers Structure initialized.\r\n"));

    fSysStarted = TRUE;

    DoAttachDebugger (&g_wszKdDll, 1);
}


// ***************************************************************************
//  DebuggerPostInit
//
//  Purpose:
//      Give debugger a chance to do initialization after filesys is running
//
//  Parameters:
//      none
//
//  Side Effects:
//      none
//
//  Returns:
//      none
//
// ***************************************************************************
void DebuggerPostInit()
{
    if (KDPostInit)
    {
        KDPostInit();
    }
}



