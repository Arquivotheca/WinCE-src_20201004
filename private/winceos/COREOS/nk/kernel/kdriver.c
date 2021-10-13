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
//------------------------------------------------------------------------------
//
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include "kernel.h"
#include "kitlpriv.h"
#include <oalioctl.h>
#include "spinlock.h"
#include <ksysdbg.h>
#include <sqmutil.h>
#include <pm.h>
#include <snapboot.h>

CRITICAL_SECTION IntChainCS;

extern ROMChain_t *ROMChain;
extern FARPROC g_pfnCompactAllHeaps;
extern PFN_SetSystemPowerState g_pfnSetSystemPowerState;
extern TSnapState g_snapState;

typedef DWORD (* IntChainHandler_t)(DWORD dwInstData);

struct _INTCHAIN {
    struct _INTCHAIN* pNext;
    PMODULE    pMod;
    FARPROC    pfnHandler;
    DWORD      dwInstData;
    BYTE       bIrq;
    BYTE       bPad[3];
};

ERRFALSE(sizeof(INTCHAIN) <= sizeof(PROXY));
#define HEAP_INTCHAIN       HEAP_PROXY

extern DWORD InDaylight;
static PINTCHAIN pIntChainTable[256];
BOOL SetDbgList (LPCWSTR pList, int nTotalLen);
BOOL CallOalIoctl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);

extern BOOL g_fAppVerifierEnabled; // Indicates whether app verifier is enabled or not in the system

PMODULELIST
AppVerifierFindModule (
    LPCWSTR pszName
    );

// set the TLS cleanup function for the current process
extern BOOL SetProcTlsCleanup (FARPROC lpv);

//------------------------------------------------------------------------------
//
// If no CPU specific code is required to wrap for "C" calling convention,
// then it is directly called as NKCallIntChain, otherwise NKCallIntChainWrapped.
//
//------------------------------------------------------------------------------
DWORD
#if defined(x86) || defined(ARM)
NKCallIntChain(
#else
NKCallIntChainWrapped(
#endif
    BYTE bIRQ
    )
{
    PINTCHAIN pChainTemp;
    DWORD dwRet = SYSINTR_CHAIN;

    pChainTemp = pIntChainTable[bIRQ];

    while(pChainTemp && dwRet == SYSINTR_CHAIN) {
        // don't think DEBUGCHK here will help since the system is hosed. Our
        // last desparate attempt to get some information for debugging.
        DEBUGCHK (pChainTemp->pMod && pChainTemp->pfnHandler);
        dwRet = pChainTemp->pfnHandler (pChainTemp->dwInstData);
        pChainTemp = pChainTemp->pNext;
    }

    return dwRet;
}


PINTCHAIN GetIntChainFromHandle (HANDLE hIntChain)
{
    PINTCHAIN pChain = NULL;
    if (((DWORD) hIntChain & 3) == 3) {
        pChain = (PINTCHAIN) ((DWORD) hIntChain & ~3);
        if (!IsValidKPtr (pChain)
            || !IsValidModule (pChain->pMod)) {
            pChain = NULL;
        }
    }

    return pChain;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE
HookIntChain (
    PMODULE pMod,
    FARPROC pfnHandler,
    DWORD   dwInstData,
    DWORD   dwIrq
    )
{
    PINTCHAIN pIntChain = (PINTCHAIN) AllocMem (HEAP_INTCHAIN);

    if (!pIntChain) {
        KSetLastError(pCurThread, ERROR_OUTOFMEMORY);

    } else {

        PINTCHAIN pWalk;
        pIntChain->pMod = pMod;
        pIntChain->pNext = NULL;
        pIntChain->pfnHandler = pfnHandler;
        pIntChain->dwInstData = dwInstData;
        pIntChain->bIrq = (BYTE) dwIrq;

        //
        // Link it in at the end of the list.
        //
        EnterCriticalSection (&IntChainCS);
        pWalk = pIntChainTable[dwIrq];
        if (pWalk == NULL) {
            // First node inserted
            pIntChainTable[dwIrq] = pIntChain;
        } else {
            while (pWalk->pNext) {
                pWalk = pWalk->pNext;
            }
            pWalk->pNext = pIntChain;
        }
        LeaveCriticalSection (&IntChainCS);
    }
    return pIntChain? (HANDLE) ((DWORD) pIntChain|3) : NULL;
}

HANDLE NKLoadKernelLibrary (LPCWSTR lpszFileName)
{
    PMODULE pMod = NULL;

    pMod = (PMODULE) NKLoadLibraryEx (lpszFileName, MAKELONG (LOAD_LIBRARY_IN_KERNEL|MF_NO_THREAD_CALLS, LLIB_NO_PAGING), NULL);

    if (pMod) {
        pMod->pfnIoctl = (PFNIOCTL) GetProcAddressA ((HMODULE) pMod, "IOControl");
    }
    
    return pMod;
}

HANDLE NKLoadIntChainHandler (LPCWSTR lpszFileName, LPCWSTR lpszHandlerName, BYTE bIRQ)
{
    HANDLE  hMod = NKLoadKernelLibrary (lpszFileName);
    HANDLE  hRet = NULL;

    DEBUGCHK (pActvProc == g_pprcNK);
    if (hMod) {
        FARPROC pfnHandler = GetProcAddressW (hMod, lpszHandlerName);
        FARPROC pfnInit    = GetProcAddressA (hMod, "CreateInstance");
        DWORD   dwInstData, dwIRQ = bIRQ;

        if (!pfnHandler
            || !pfnInit
            || (dwIRQ > 0xff)
            || ((dwInstData = (DWORD) pfnInit ()) == -1)
            || (FALSE == (hRet = HookIntChain ((PMODULE) hMod, pfnHandler, dwInstData, dwIRQ)))) {
            NKFreeLibrary (hMod);
        }
    }
    return hRet;
}

//------------------------------------------------------------------------------
// Note: On successful return this call does not delete memory assigned to
// pIntChain; callers are responsible for freeing the memory.
//------------------------------------------------------------------------------
BOOL
UnhookIntChain (
    PINTCHAIN pIntChain
    )
{
    BOOL        fRet = FALSE;
    PINTCHAIN*  ppWalk;

    EnterCriticalSection (&IntChainCS);

    for (ppWalk = &pIntChainTable[pIntChain->bIrq]; *ppWalk; ppWalk = &(*ppWalk)->pNext) {
        if (*ppWalk == pIntChain) {
            //
            // Found it. Unhook it. Don't need to turn interrupt off because FreeMem
            // is called AFTER we update the list.
            //
            *ppWalk = pIntChain->pNext;
            fRet = TRUE;
            break;
        }
    }

    LeaveCriticalSection (&IntChainCS);


    if (!fRet) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
    }
    return fRet;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL NKFreeIntChainHandler (HANDLE hLib)
{
    PINTCHAIN pIntChain = GetIntChainFromHandle (hLib);
    BOOL      fRet      = FALSE;
    FARPROC pfnDestroy;

    DEBUGCHK (pActvProc == g_pprcNK);

    if (pIntChain && UnhookIntChain (pIntChain)) {
        // at this point the pIntChain is no longer in the list
        // and no other thread can execute the following code
        PMODULE pMod = pIntChain->pMod;

        // call the Destroy instance export
        pfnDestroy = GetProcAddressA((HMODULE)pMod, "DestroyInstance");
        if (pfnDestroy) {
            pfnDestroy(pIntChain->dwInstData);
        }

        FreeMem (pIntChain, HEAP_INTCHAIN);
        fRet = NKFreeLibrary ((HMODULE) pMod);
    }

    return fRet;
}


#define MAX_PAGES_ALLOC_SHARED    ((512*1024*1024)/VM_PAGE_SIZE) // max # of pages in 512M
static BOOL DoAllocShareMem (BOOL fIsFree, DWORD nPages, BOOL fNoCache, LPVOID *pVa, LPVOID *pPa)
{
    ULONG physAddr;

    DEBUGMSG (ZONE_ENTRY, (L"DoAllocShareMem (%d, %d, %d, %8.8lx, %8.8lx)\n", fIsFree, fNoCache, nPages, pVa, pPa));
    if (!nPages || !pVa || !pPa || (nPages >= MAX_PAGES_ALLOC_SHARED)) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (fIsFree) {
        // pPa is unused for freeing
        return PROCVMFree (pActvProc, pVa, 0, MEM_RELEASE, 0);
    }

    // allocate the memory from process slot
    *pVa = VMAllocPhys (pActvProc, nPages * VM_PAGE_SIZE, PAGE_READWRITE, 0, 0, &physAddr);
    if (*pVa) {
        *pPa = *pVa;
    }

    // translate the physical address into statically mapped uncached address
    return NULL != *pVa;

}

extern DWORD JITGetCallStack (HANDLE hThrd, ULONG dwMaxFrames, CallSnapshot lpFrames[]);
extern BOOL NKSetMemoryAttributes (LPVOID pVirtualAddr, LPVOID pShiftedPhysAddr, DWORD cbSize, DWORD dwAttributes);


BOOL fDisableNoFault;

//---------------------------------------------------------------------------
// DW Watson support
//---------------------------------------------------------------------------
#define dwNKDrWatsonSize            g_pOemGlobal->cbErrReportSize
static LPBYTE pDrWatsonDump;        // pointer to Dr. Watson dump area.

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void InitDrWatson (void)
{
    if (dwNKDrWatsonSize) {
        dwNKDrWatsonSize = PAGEALIGN_UP (dwNKDrWatsonSize);
        RETAILMSG (1, (L"Error Reporting Memory Reserved, dump size = %8.8lx\r\n", dwNKDrWatsonSize));
        g_pOemGlobal->dwMainMemoryEndAddress -= dwNKDrWatsonSize;
        pDrWatsonDump = (LPBYTE) g_pOemGlobal->dwMainMemoryEndAddress;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL RAMDrWatsonFlush (void)
{
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL RAMDrWatsonClear (void)
{
    memset (pDrWatsonDump, 0, dwNKDrWatsonSize);
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD RAMDrWatsonWrite (DWORD dwOffset, LPVOID pData, DWORD cbSize)
{
    if (!dwNKDrWatsonSize || (dwOffset > dwNKDrWatsonSize) || ((int) cbSize <= 0))
        return 0;

    if (cbSize + dwOffset > dwNKDrWatsonSize)
        cbSize = dwNKDrWatsonSize - dwOffset;

    memcpy (pDrWatsonDump + dwOffset, pData, cbSize);

    return cbSize;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD RAMDrWatsonRead (DWORD dwOffset, LPVOID pData, DWORD cbSize)
{
    if (!dwNKDrWatsonSize || (dwOffset > dwNKDrWatsonSize) || ((int) cbSize <= 0))
        return 0;

    if (cbSize + dwOffset > dwNKDrWatsonSize)
        cbSize = dwNKDrWatsonSize - dwOffset;

    memcpy (pData, pDrWatsonDump + dwOffset, cbSize);

    return cbSize;
}

//------------------------------------------------------------------------------
// Default Dr. Watson implementation carves a range of ram, OEM can override the
// following pointers and dwNKDrWatsonSize to implement if he wishes to persist
// Dr. Watson Data across cold boot.
//------------------------------------------------------------------------------
BOOL (* pfnNKDrWatsonFlush) (void) = RAMDrWatsonFlush;
BOOL (* pfnNKDrWatsonClear) (void) = RAMDrWatsonClear;
DWORD (* pfnNKDrWatsonRead) (DWORD dwOffset, LPVOID pData, DWORD cbSize) = RAMDrWatsonRead;
DWORD (* pfnNKDrWatsonWrite) (DWORD dwOffset, LPVOID pData, DWORD cbSize) = RAMDrWatsonWrite;

extern BOOL NKSetJITDebuggerPath (LPCWSTR pszDbgrPath);
PMODULE FindModInKernel (PMODULE pMod);

BOOL GetPhysMemInfo (PPHYSMEMINFO ppi)
{
    DWORD dwErr = 0;
    __try {
        ppi->dwPfnROMStart = GetPFN ((LPVOID) PAGEALIGN_DOWN (pTOC->physfirst));
        ppi->dwPfnROMEnd   = GetPFN ((LPVOID) PAGEALIGN_UP   (pTOC->physlast));
        ppi->dwPfnRAMStart = GetPFN ((LPVOID) PAGEALIGN_DOWN ((DWORD)MemoryInfo.pKData));
        ppi->dwPfnRAMEnd   = GetPFN ((LPVOID) PAGEALIGN_UP   ((DWORD)MemoryInfo.pKEnd - VM_PAGE_SIZE)) + VM_PAGE_SIZE; // pKEnd points to after the RAM end
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        NKSetLastError (dwErr = ERROR_INVALID_PARAMETER);
    }
    return !dwErr;
}

static BOOL GetRomRamInfo (PROMRAMINFO prri)
{
    const REGIONINFO *pfi, *pfiEnd;
    const ROMChain_t *pROM;
    PPFNRANGE ppfr;

    prri->cRams = prri->cRoms = 0;

    for (pROM = ROMChain; pROM; pROM = pROM->pNext) {
        ppfr = &prri->roms[prri->cRoms ++];
        if (prri->cRoms > MAX_ROM_SECTIONS) {
            return FALSE;
        }
        ppfr->pfnBase   = GetPFN ((LPVOID) PAGEALIGN_DOWN (pROM->pTOC->physfirst));
        ppfr->pfnLength = GetPFN ((LPVOID) PAGEALIGN_UP   (pROM->pTOC->physlast)) - ppfr->pfnBase;
    }

    for (pfi = MemoryInfo.pFi, pfiEnd = pfi + MemoryInfo.cFi; pfi < pfiEnd ; pfi++) {
        ppfr = &prri->rams[prri->cRams ++];
        if (prri->cRams > MAX_RAM_SECTIONS) {
            return FALSE;
        }
        ppfr->pfnBase   = pfi->paStart;
        ppfr->pfnLength = pfi->paEnd - pfi->paStart;
    }
    return TRUE;

}

static BOOL DoSetMemoryAttributes (LPVOID pVirtualAddr, LPVOID pShiftedPhysAddr, DWORD cbSize, DWORD dwAttributes)
{
    BOOL  dwErr = 0;
    DWORD dwPhysAddr = (PHYSICAL_ADDRESS_UNKNOWN == pShiftedPhysAddr)? 0 : ((DWORD) pShiftedPhysAddr << 8);
    // validate parameter
    if (   !cbSize                                                  // invalid size
        || !pVirtualAddr                                            // no virtual address
        || ((DWORD) pVirtualAddr & VM_PAGE_OFST_MASK)               // invalid virtual address
        || (dwPhysAddr & VM_PAGE_OFST_MASK)                         // invalid physical address
        || (cbSize & VM_PAGE_OFST_MASK)) {
        dwErr = ERROR_INVALID_PARAMETER;
    } else {

        extern CRITICAL_SECTION rtccs;
        // we need to serialize calls to the OEM function, but we can't use kernel VM lock, for it can cause CS order vialation.
        // Picking rtccs as it's majorly used to guard OEM access (RTC).
        EnterCriticalSection (&rtccs);
        // call to OEM to set the memory attributes
        // NOTE: we intentionally not checking dwAttributes so we don't have to change kernel
        //       in case we support more attributes in the future.
        if (!g_pOemGlobal->pfnSetMemoryAttributes (pVirtualAddr, pShiftedPhysAddr, cbSize, dwAttributes)) {
            dwErr = ERROR_NOT_SUPPORTED;
        }
        LeaveCriticalSection (&rtccs);
    }
    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }
    return !dwErr;
}

BOOL PostFsInit (PFSINITINFO pfsi);

#ifdef ARM
extern void UnalignedAccessEnable (BOOL fEnable);
#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL KernelLibIoControl_Core(
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    BOOL fRet = TRUE;

    switch (dwIoControlCode) {
    case IOCTL_KLIB_ALLOCSHAREMEM:
    case IOCTL_KLIB_FREESHAREMEM:
        fRet = DoAllocShareMem (IOCTL_KLIB_FREESHAREMEM == dwIoControlCode, nInBufSize, nOutBufSize, lpInBuf, lpOutBuf);
        break;
        
    case IOCTL_KLIB_SETMEMORYATTR:
        //
        // CeSetMemoryAttributes (irregular arugment usage, k-mode direct call only):
        //      lpInBuf: pVirtualAddr
        //      lpOutBuf: pShiftedPhysAddr
        //      nInBufSize: cbSize
        //      nOutBufSize: dwAttributes
        //
        fRet = DoSetMemoryAttributes (lpInBuf, lpOutBuf, nInBufSize, nOutBufSize);
        break;

    case IOCTL_KLIB_GETROMCHAIN:
        fRet = (BOOL) ROMChain;
        break;

    case IOCTL_KLIB_GETALARMRESOLUTION:
        *((DWORD*)lpOutBuf) = g_pOemGlobal->dwAlarmResolution;
        break;

    case IOCTL_KLIB_GETCOMPRESSIONINFO:
        *((BOOL*)lpOutBuf) = g_fCompressionSupported;
        break;

    case IOCTL_KLIB_ISKDPRESENT:
        *((DWORD*)lpOutBuf) = (g_pKdInit != NULL);
        break;


    case IOCTL_KLIB_JITGETCALLSTACK:
        // nOutBufSize: max # of frames
        // lpOutBuf: pointer to buffer to receive the callstack
        fRet = JITGetCallStack (NULL, nOutBufSize, (CallSnapshot *) lpOutBuf);
        break;

    case IOCTL_KLIB_GETCALLSTACK:
        //
        // nInBufSize: specify flag and skip (dwFlags = LOWORD(nInBufSize), dwSkip = HIWORD(nInBufSize))
        // lpInBuf: pointer to context (pCtx)
        // nOutBufSize: specify max # of frames (dwMaxFrames)
        // lpOutBuf: pointer to buffer to receive callstack frames (lpFrames)
        //
        fRet = NKGetThreadCallStack (pCurThread, nOutBufSize, lpOutBuf, LOWORD(nInBufSize), HIWORD(nInBufSize), (PCONTEXT) lpInBuf);
        break;

    case IOCTL_KLIB_SETJITDBGRPATH:
        // change the path of JIT debugger
        fRet = NKSetJITDebuggerPath ((LPCWSTR) lpInBuf);
        break;


    case IOCTL_KLIB_INTERRUPTDONE:
        // call InterruptDone.
        //  nInBufSize: the sysintr value
        if ((nInBufSize >= SYSINTR_DEVICES) && (nInBufSize < SYSINTR_MAXIMUM))
            INTRDone(nInBufSize);

        break;

    case IOCTL_KLIB_SETIGNORENOFAULT:
        // nInBufSize: (BOOL) fDisableNoFault
        fDisableNoFault = nInBufSize;
        break;

    case IOCTL_KLIB_SETDBGLIST:
        // update debug module list
        // lpInBuf: the list of modules in multi-sz format
        // nInBufSize: size, in BYTE, of the list
        fRet = SetDbgList ((LPCWSTR) lpInBuf, nInBufSize/sizeof(WCHAR));
        break;

    case IOCTL_KLIB_GETWATSONSIZE:
        // get the size of Dr. Watson dump area
        fRet = dwNKDrWatsonSize;
        break;

    case IOCTL_KLIB_WRITEWATSON:
        // Write data to Dr. Watson dump area
        // lpInbuf: Data to be Written
        // nInBufSize: Size of the data
        // nOutBufSize: offset from which to be written (irregular usage)
        fRet = (* pfnNKDrWatsonWrite) (nOutBufSize, lpInBuf, nInBufSize);
        break;

    case IOCTL_KLIB_READWATSON:
        // read data from Dr. Watson dump area
        // nInBufSize: offset from which to be read (irregular usage)
        // lpOutBuf: pointer to a buffer of size at least nOutBufSize to receive Dr.Watson Dump
        // nOutBufSize: size of buffer pointed by lpOutBuf
        fRet = (* pfnNKDrWatsonRead) (nInBufSize, lpOutBuf, nOutBufSize);
        break;


    case IOCTL_KLIB_FLUSHWATSON:
        // Flush data to Dr. Watson dump area, if using persistent storage
        fRet = (* pfnNKDrWatsonFlush) ();
        break;

    case IOCTL_KLIB_CLEARWATSON:
        // Clear the Dr. Watson dump area
        fRet = (* pfnNKDrWatsonClear) ();
        break;

    case IOCTL_KLIB_SETROMDLLBASE:
        {
            DWORD dwAddr = (DWORD) VMAlloc (g_pprcNK, lpInBuf, nInBufSize, MEM_RESERVE|MEM_IMAGE, PAGE_NOACCESS);
            if (dwAddr && IsKModeAddr (dwAddr)) {
                // reserve VM for kernel DLL, update KData
                if (g_pKData->dwKDllFirst > dwAddr) {
                    g_pKData->dwKDllFirst = dwAddr;
                } else
                if (g_pKData->dwKDllEnd < (dwAddr + nInBufSize)) {
                    g_pKData->dwKDllEnd = (dwAddr + nInBufSize);
                }
            }
            fRet = (0 != dwAddr);
        }
        break;

    case IOCTL_KLIB_GETUSRCOREDLL:
        fRet = (DWORD) hCoreDll;
        break;

    case IOCTL_KLIB_GETKHEAPINFO:
        fRet = GetKHeapInfo (nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        break;

    case IOCTL_KLIB_GETPHYSMEMINFO:
        fRet = GetPhysMemInfo ((PPHYSMEMINFO) lpOutBuf);
        break;

    case IOCTL_KLIB_GETROMRAMINFO:
        return GetRomRamInfo ((PROMRAMINFO) lpOutBuf);

    case IOCTL_KLIB_GET_POOL_STATE:
        fRet = PagePoolGetState ((NKPagePoolState*) lpOutBuf);
        break;

    case IOCTL_KLIB_TIMEINIT:
        //
        // called by filesys as soon as registry is initialized.
        //
        InitSoftRTC ();
        break;

    case IOCTL_KLIB_NOTIFYCLEANBOOT:
        g_pOemGlobal->pfnNotifyForceCleanBoot (0, 0, 0, 0);
        break;

    case IOCTL_KLIB_FORCECLEANBOOT:
        NKForceCleanBoot ();
        break;

    case IOCTL_KLIB_UNALIGNENABLE:
        // currently only supporting ARM V6 and x86
        // nInBufSize is fEnable
#ifdef ARM
        if (IsV6OrLater ()) {
            UnalignedAccessEnable (*(LPDWORD)lpInBuf);
            break;
        }
#endif

#ifndef x86
        KSetLastError (pCurThread, ERROR_NOT_SUPPORTED);
        fRet = FALSE;
#endif
        break;

    case IOCTL_KLIB_LOADERINIT:
        fRet = !g_pfnGetProcAddrA;
        if (fRet) {
            PLOADERINITINFO plii = (PLOADERINITINFO) lpInBuf;
            g_pfnGetProcAddrA = (PFN_GetProcAddrA) plii->pfnGetProcAddr;
            g_pfnDoImports    = (PFN_DoImport)     plii->pfnImportAndCallDllMain;
            g_pfnUndoDepends  = (PFN_UndoDepends)  plii->pfnUndoDepends;
            g_pfnAppVerifierIoControl = (PFN_AppVerifierIoControl) plii->pfnAppVerifierIoControl;
            plii->pfnFindModule = (FARPROC) FindModInKernel;
            plii->pfnNKwvsprintfW = (FARPROC) NKwvsprintfW;
        }
        break;

    case IOCTL_KLIB_CACHE_REGISTER:
        fRet = RegisterForCaching (lpInBuf, lpOutBuf);
        break;

    case IOCTL_KLIB_FLUSH_MAPFILES:
        WriteBackAllFiles ();
        break;

    case IOCTL_KLIB_FSINIT_COMPLETE:
        fRet = PostFsInit ((PFSINITINFO)lpInBuf);
        break;

    // sqm ioctls
    case IOCTL_GET_MEMORY_MARKERS:
        fRet = GetSQMMarkers (lpOutBuf, nOutBufSize, lpBytesReturned);
        break;

    case IOCTL_RESET_MEMORY_MARKERS:
        fRet = ResetSQMMarkers ();
        break;

    case IOCTL_FILESYS_RUNAPPS_COMPLETED:
        fRet = UpdateSQMMarkers (SQM_MARKER_RAMBASELINEBYTES, PageFreeCount);
        break;

    case IOCTL_KLIB_REGISTER_MONITOR:
        // register current thread as monitor thread. The thread must be a kernel thread.
        // 
        // outbuf - ptr to a DWORD, which will be set to the thread id of the spinner thread
        //          when high prio thread spinning detected.
        // inpuf - ptr to a DWORD, which specifies the maximum delay allowed between the
        //         monitor thread runnable and monitor thread running
        //
        if ((pCurThread->pprcOwner == g_pprcNK) && lpInBuf && lpOutBuf) {
            DWORD dwMaxDelay = *(LPDWORD) lpInBuf;
            if ((dwMaxDelay > g_pOemGlobal->dwDefaultThreadQuantum)
                && (dwMaxDelay <= MAX_TIMEOUT)) {
                SCHL_RegisterMonitorThread ((LPDWORD) lpOutBuf, dwMaxDelay);
            }
        }
        break;

    case IOCTL_KLIB_MAKESNAP:
        if (g_pOemGlobal->pSnapshotSupport
            && g_pfnCompactAllHeaps
            && g_pfnSetSystemPowerState
            && (eSnapNone == g_snapState)) {
            PPROCESS pprc = SwitchActiveProcess (g_pprcNK);

            // page out memory that can be paged in after boot
            g_pfnCompactAllHeaps ();
            NKForcePageout ();

            g_snapState = eSnapPass2;

            // go through suspend/resume code path to actually take the snapshot 
            g_pfnSetSystemPowerState (NULL, POWER_STATE_SUSPEND, POWER_FORCE);

            SwitchActiveProcess (pprc);
        } else {
            NKD (L"==================================================================\r\n");
            NKD (L"Taking snapshot is not supported, or already booted from snapshot.\r\n");
            NKD (L"==================================================================\r\n");
            fRet = FALSE;
        }
        break;
    case IOCTL_KLIB_GETSNAPSTATE:
        fRet = CeSafeCopyMemory (lpOutBuf, &g_snapState, sizeof (g_snapState));
        break;
    default:
        fRet = FALSE;
    }

    return fRet;
}

BOOL KernelLibIoControl_Dbg(
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    DWORD err = ERROR_SUCCESS;
    switch (dwIoControlCode) {
    case IOCTL_DBG_INIT:
        if (sizeof(g_pKdInit) != nInBufSize) {
            err = ERROR_INVALID_DATA;
        } else {
            memcpy((VOID*)&g_pKdInit, lpInBuf, sizeof(g_pKdInit));
        }
        break;

    case IOCTL_DBG_HDSTUB_INIT:
        if (sizeof (g_pHdInit) != nInBufSize) {
            err = ERROR_INVALID_DATA;
        } else {
            memcpy (&g_pHdInit, lpInBuf, sizeof (g_pHdInit));
        }
        break;

    case IOCTL_DBG_OSAXST0_INIT:
        if (sizeof (g_pOsAxsT0Init) != nInBufSize) {
            err = ERROR_INVALID_DATA;
        } else {
            memcpy (&g_pOsAxsT0Init, lpInBuf, sizeof (g_pOsAxsT0Init));
        }
        break;

    case IOCTL_DBG_OSAXST1_INIT:
        if (sizeof (g_pOsAxsT1Init) != nInBufSize) {
            err = ERROR_INVALID_DATA;
        } else {
            memcpy (&g_pOsAxsT1Init, lpInBuf, sizeof (g_pOsAxsT1Init));
        }
        break;

    case IOCTL_DBG_SANITIZE:
        // Arguments.
        // lpInBuf: Address of the instruction to sanitize
        // nInBufSize: Size of instruction
        // lpOutBuf: Address of destination buffer to write sanitized bytes.
        // nOutBufSize: Size of instruction
        // lpBytesReturned: Unused
        //
        // Accessible to all callers, necessary for unwinder.
        DEBUGCHK(nInBufSize == nOutBufSize);
        KDSanitize(lpOutBuf, lpInBuf, nInBufSize, FALSE);
        break;

    default:
        err = ERROR_INVALID_FUNCTION;
        break;
    }

    if (err != ERROR_SUCCESS) {
        NKSetLastError(err);
    }
    return err == ERROR_SUCCESS;
}

BOOL KernelLibIoControl_AppVerif(
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    switch (dwIoControlCode)
    {
        case IOCTL_APPVERIF_ATTACH:
        {
            // Attach to an already-loaded kernel module
            // lpInBuf/nInBufSize: module name to attach
            // lpOutBuf/nOutBufSize: unused
            PMODULELIST pMod;

            if (!lpInBuf)
            {
                NKSetLastError (ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            pMod = AppVerifierFindModule (lpInBuf);
            if (pMod)
            {
                return g_pfnAppVerifierIoControl (dwIoControlCode, pMod->pMod, sizeof (PMODULELIST), NULL, 0, NULL);
            }

            break;
        }

        case IOCTL_APPVERIF_ENABLE:
        {
            if (lpOutBuf && (sizeof (DWORD) == nOutBufSize))
            {
                *(BOOL*)lpOutBuf = (TRUE == g_fAppVerifierEnabled) ? TRUE : FALSE;
            }

            if (lpInBuf && (sizeof (DWORD) == nInBufSize))
            {
                g_fAppVerifierEnabled = *(BOOL*)lpInBuf;
            }

            return TRUE;
        }

        default:
            return g_pfnAppVerifierIoControl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
    }

    return FALSE;
}

extern DWORD g_dwKeys [];

BOOL (* g_pfnExtKITLIoctl) (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);

BOOL KernelLibIoControl_Kitl(
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    switch (dwIoControlCode) {
    case IOCTL_KLIB_KITL_INIT:
        if (sizeof (KITLPRIV) == nOutBufSize) {
            PKITLPRIV pPriv = (PKITLPRIV) lpOutBuf;
            pPriv->pfnAllocMem = AllocMem;
            pPriv->pfnFreeMem = FreeMem;

            pPriv->pfnCreateKernelThread = CreateKernelThread;
            pPriv->pfnIntrInitialize = INTRInitialize;
            pPriv->pfnIntrDone = INTRDone;
            pPriv->pfnIntrDisable = INTRDisable;
            pPriv->pfnIntrMask = INTRMask;

            pPriv->pfnDoWaitForObjects = DoWaitForObjects;

            pPriv->pfnNKCreateEvent = NKCreateEvent;
            pPriv->pfnEVNTModify = EVNTModify;

            pPriv->pfnHNDLCloseHandle = HNDLCloseHandle;
            pPriv->pfnLockHandleData = LockHandleData;
            pPriv->pfnUnlockHandleData = UnlockHandleData;

            pPriv->pfnVMAlloc = VMAlloc;
            pPriv->pfnVMFreeAndRelease = VMFreeAndRelease;

            pPriv->pfnKCall = KCall;

            pPriv->pKData = g_pKData;
            pPriv->pprcNK = g_pprcNK;
            pPriv->pdwKeys = g_dwKeys;
            pPriv->ppKitlSpinLock  = &g_pKitlSpinLock;
#ifdef ARM
            pPriv->pfnInSysCall = InSysCall;
#endif

            pPriv->pfnInitSpinLock          = InitializeSpinLock;
            pPriv->pfnAcquireSpinLock       = AcquireSpinLock;
            pPriv->pfnReleaseSpinLock       = ReleaseSpinLock;
            pPriv->pfnStopAllOtherCPUs      = StopAllOtherCPUs;
            pPriv->pfnResumeAllOtherCPUs    = ResumeAllOtherCPUs;

            g_pfnExtKITLIoctl = pPriv->pfnExtKITLIoctl;

        }
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

void UpdateLogPtrForSoftReset (void);

//------------------------------------------------------------------------------
//
// Entry to KernelLibIoControl from kernel mode code (trust already verified).
// User mode code will call into the EXTKernelLibIoControl via trap so that
// trust can be verified.
//
//------------------------------------------------------------------------------
BOOL NKKernelLibIoControl(
    HANDLE  hLib,
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    BOOL fRet = FALSE;
    PTHREAD pth = pCurThread;
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    switch ((DWORD) hLib) {
        case KMOD_OAL:
            if (IOCTL_HAL_REBOOT == dwIoControlCode) {
                UpdateLogPtrForSoftReset ();
            }
            fRet = OEMIoControl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
            break;

        case KMOD_CORE:
            // policy check for specific ioctls callable by user mode.
            // need to check here instead of external api to account
            // for calls coming from kernel mode on a user thread.
            if (((dwIoControlCode == IOCTL_KLIB_SETJITDBGRPATH)
                    || (dwIoControlCode == IOCTL_KLIB_UNALIGNENABLE)
                    || (dwIoControlCode == IOCTL_RESET_MEMORY_MARKERS)
                    || (dwIoControlCode == IOCTL_KLIB_JITGETCALLSTACK))) {
                if(NO_ERROR == KGetLastError (pth)) {
                    KSetLastError (pth, ERROR_GEN_FAILURE);
                }

            } else {
                fRet = KernelLibIoControl_Core(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
            }
            break;

        case KMOD_DBG:
            fRet = KernelLibIoControl_Dbg(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
            break;

        case KMOD_APPVERIF:
            fRet = KernelLibIoControl_AppVerif(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
            break;

        case KMOD_KITL:
            fRet = KernelLibIoControl_Kitl(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
            break;

        case KMOD_CELOG:
            fRet = KernelLibIoControl_CeLog(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
            break;

        default:
            break;
    }

    if ((DWORD) hLib > KMOD_MAX) {
        
        // check if's installable ISR, or just a kernel module
        PINTCHAIN pChain = GetIntChainFromHandle (hLib);
        PMODULE pMod = pChain
            ? pChain->pMod
            : (IsValidModule ((PMODULE) hLib)
                ? (PMODULE) hLib
                : NULL);
        
        if (pMod && pMod->pfnIoctl) {
            DWORD dwInstData = pChain? pChain->dwInstData : 0;
            fRet = pMod->pfnIoctl (dwInstData, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        }
    }
    
    SwitchActiveProcess (pprc);
    return fRet;
}


//------------------------------------------------------------------------------
//
// Entry to KernelLibIoControl from user mode code. Installable ISRs will
// call into the NKKernelLibIoControl since they run in kernel context and
// the trust check can't follow pCurProc.
//
//------------------------------------------------------------------------------
BOOL EXTKernelLibIoControl(
    HANDLE  hLib,
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    DWORD dwLib = (DWORD) hLib;
    
    if (KMOD_OAL == dwLib) {
        // forward the call to the oal ioctl dll
        BOOL fRet = FALSE;
        PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
        fRet = CallOalIoctl(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        SwitchActiveProcess (pprc);
        return fRet;

    } else if ((KMOD_KITL == dwLib) || (KMOD_CELOG == dwLib)) {
        // only supported from kernel mode
        KSetLastError (pCurThread, ERROR_ACCESS_DENIED);
        return FALSE;
            
    } else if (KMOD_CORE == dwLib) {

        DWORD dwErr = 0;
        
        // user mode is limited to the following IOCTLs, validate args here
        switch (dwIoControlCode) {
    
            // IOCTLs - no validation needed
            case IOCTL_KLIB_MAKESNAP:
            case IOCTL_KLIB_SETIGNORENOFAULT:
            case IOCTL_KLIB_GETUSRCOREDLL:
            case IOCTL_KLIB_SETJITDBGRPATH:
                break;

            case IOCTL_KLIB_SETPROCTLS:
                // buffer validation is done in the function.
                return SetProcTlsCleanup ((FARPROC) (LONG) lpInBuf);
                
            // IOCTLs - simple validation (lpInBuf is a DWORD)
            case IOCTL_KLIB_UNALIGNENABLE:
                if (nInBufSize != sizeof(DWORD)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }                        

                break;

            // IOCTLs - simple validation (result lpOutBuf is a DWORD)
            case IOCTL_KLIB_GETCOMPRESSIONINFO:
            case IOCTL_KLIB_GETALARMRESOLUTION:
            case IOCTL_KLIB_ISKDPRESENT:
            case IOCTL_KLIB_GETSNAPSTATE:
                if (nOutBufSize != sizeof(DWORD)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
                break;

            // IOCTLs - more complicated validation
            case IOCTL_KLIB_JITGETCALLSTACK:
                if (nOutBufSize > MAX_CALLSTACK) {
                    nOutBufSize = MAX_CALLSTACK;    // this will guarantee we don't overflow
                }

                if (!IsValidUsrPtr (lpOutBuf, nOutBufSize * sizeof (CallSnapshot), TRUE)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
                break;

            // sqm ioctls
            case IOCTL_GET_MEMORY_MARKERS:
                if ((lpOutBuf && !IsValidUsrPtr (lpOutBuf, nOutBufSize, TRUE))
                    || (!lpOutBuf && !lpBytesReturned)) {
                    KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
                    return FALSE;
                }
                break;

            case IOCTL_RESET_MEMORY_MARKERS:
                break;
                            
            case IOCTL_KLIB_GETKHEAPINFO:
                if (!lpBytesReturned) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
                break;
                
            case IOCTL_KLIB_GETPHYSMEMINFO:
                if (sizeof (PHYSMEMINFO) != nOutBufSize) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
                break;

            case IOCTL_KLIB_GETROMRAMINFO:
                if (sizeof (ROMRAMINFO) != nOutBufSize) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
                break;

            case IOCTL_KLIB_GET_POOL_STATE:
                // Query page pool state
                // lpInbuf/nInBufSize: unused
                // lpOutBuf: NKPagePoolState struct
                // nOutBufSize: sizeof(NKPagePoolState)
                if (lpInBuf || nInBufSize
                    || (sizeof (NKPagePoolState) != nOutBufSize)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
                break;

            //
            // IOCTL_KLIB_MAPPD/IOCTL_KLIB_UNMAPPD is User mode only. For it's mapping PD is the user VM.
            //
            case IOCTL_KLIB_MAPPD:
                // validate arguments
                if ((sizeof(DWORD) != nInBufSize)
                    || (sizeof (PROCMEMINFO) != nOutBufSize)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                    break;
                }
                return (DWORD) VMMapPD (*(LPDWORD) lpInBuf, (PPROCMEMINFO) lpOutBuf);
            
            case IOCTL_KLIB_UNMAPPD:
                // parameter validated in VMUnmapPD
                return VMUnmapPD (lpInBuf);
                
            case IOCTL_KLIB_CACHE_REGISTER:
            case IOCTL_KLIB_FLUSH_MAPFILES:
            default:
                dwErr = ERROR_ACCESS_DENIED;
                break;
        }
        
        if (dwErr) {
            KSetLastError (pCurThread, dwErr);
            return FALSE;
        }
    }

    if ((DWORD) hLib > KMOD_MAX) {
        PMODULE pMod = FindModInKernel ((PMODULE) hLib);
        if (!pMod || !pMod->pfnIoctl) {
            hLib = NULL;
        }
    }

    return NKKernelLibIoControl(hLib, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
}

