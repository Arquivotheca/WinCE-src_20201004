//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
// 
// 
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include "kernel.h"

CRITICAL_SECTION IntChainCS;

extern ROMChain_t *ROMChain;

typedef DWORD (* IntChainHandler_t)(DWORD dwInstData);

typedef struct _INTCHAIN {
    struct _INTCHAIN* pNext;
    PKERNELMOD pkMod;

} INTCHAIN, *PINTCHAIN;

ERRFALSE(sizeof(INTCHAIN) <= sizeof(PROXY));
#define HEAP_INTCHAIN       HEAP_PROXY

extern DWORD InDaylight;
static PINTCHAIN pIntChainTable[256];

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
        DEBUGCHK (pChainTemp->pkMod && pChainTemp->pkMod->pfnHandler);
        dwRet = pChainTemp->pkMod->pfnHandler (pChainTemp->pkMod->dwInstData);
        pChainTemp = pChainTemp->pNext;
    }

    return dwRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
HookIntChain(
    BYTE bIRQ, 
    PKERNELMOD pkMod
    )
{
    PINTCHAIN pIntChain;
    PINTCHAIN pWalk;

    pIntChain = (PINTCHAIN) AllocMem(HEAP_INTCHAIN);

    if (!pIntChain) {
        KSetLastError(pCurThread,ERROR_OUTOFMEMORY);
        return FALSE;
    }
    
    pIntChain->pkMod = pkMod;
    pIntChain->pNext = NULL;

    //
    // Link it in at the end of the list.
    //
    EnterCriticalSection(&IntChainCS);
    pWalk = pIntChainTable[bIRQ];
    if (pWalk == NULL) {
        // First node inserted
        pIntChainTable[bIRQ] = pIntChain;
    } else {
        while (pWalk->pNext) {
            pWalk = pWalk->pNext;
        }
        pWalk->pNext = pIntChain;
    }
    LeaveCriticalSection(&IntChainCS);

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
UnhookIntChain(
    BYTE bIRQ, 
    PKERNELMOD pkMod
    )
{
    PINTCHAIN   pWalk;
    PINTCHAIN* ppPrev;
    BOOL fRet = FALSE;

    EnterCriticalSection(&IntChainCS);
    pWalk  =   pIntChainTable[bIRQ];
    ppPrev = &(pIntChainTable[bIRQ]);

    while(pWalk) {
        if (pWalk->pkMod == pkMod) {
            //
            // Found it. Unhook it. Don't need to turn interrupt off because FreeMem 
            // is called AFTER we update the list.
            //
            *ppPrev = pWalk->pNext;
            FreeMem(pWalk, HEAP_INTCHAIN);
            pWalk = NULL;               // break loop
            fRet = TRUE;
        } else {
            ppPrev = &(pWalk->pNext);
            pWalk = pWalk->pNext;
        }
    }
    LeaveCriticalSection(&IntChainCS);
    
    if (fRet == FALSE) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
    }
    return fRet;
}


static BOOL DoAllocShareMem (BOOL fIsFree, DWORD nPages, BOOL fNoCache, LPVOID *pVa, LPVOID *pPa)
{
    ULONG physAddr;

    DEBUGMSG (ZONE_ENTRY, (L"DoAllocShareMem (%d, %d, %d, %8.8lx, %8.8lx)\n", fIsFree, fNoCache, nPages, pVa, pPa));
    if (!nPages || !pVa || !pPa) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (fIsFree) {
        // pPa is unused for freeing
        return SC_FreePhysMem (pVa);
    }

    // allocate the memory from process slot
    if (*pVa = SC_AllocPhysMem(nPages * PAGE_SIZE, PAGE_READWRITE, 0, 0, &physAddr)) {
        *pPa = (LPVOID) (Phys2VirtUC (PA2PFN (physAddr)));
    }

    // translate the physical address into statically mapped uncached address
    return NULL != *pVa;
}

extern BOOL SetROMDllBase (DWORD cbSize, PROMINFO pInfo);
extern BOOL GetVMInfo (int idxProc, PPROCVMINFO pInfo);
extern ULONG NKGetCallStackSnapshot (ULONG dwMaxFrames, CallSnapshot lpFrames[], DWORD dwFlags, DWORD dwSkip, PCONTEXT pCtx);
extern DWORD JITGetCallStack (HANDLE hThrd, ULONG dwMaxFrames, CallSnapshot lpFrames[]);
extern BOOL NKSetMemoryAttributes (LPVOID pVirtualAddr, LPVOID pShiftedPhysAddr, DWORD cbSize, DWORD dwAttributes);

BOOL fDisableNoFault;

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
    switch (dwIoControlCode) {
    case IOCTL_KLIB_ALLOCSHAREMEM:
    case IOCTL_KLIB_FREESHAREMEM:
        return DoAllocShareMem (IOCTL_KLIB_FREESHAREMEM == dwIoControlCode, nInBufSize, nOutBufSize, lpInBuf, lpOutBuf);

    // irregular usage.
    case IOCTL_KLIB_GETROMCHAIN:
        return (BOOL) ROMChain;
        
    case IOCTL_KLIB_GETCOMPRESSIONINFO:
        if (lpInBuf || nInBufSize || !lpOutBuf || (nOutBufSize != sizeof(BOOL))) {
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        *((BOOL*)lpOutBuf) = g_fCompressionSupported;
        return TRUE;

    case IOCTL_KLIB_CHANGEMAPFLUSHING: {
        MapFlushInfo *pInfo;

        if (!lpInBuf || (nInBufSize != sizeof(MapFlushInfo))
            || !lpOutBuf || (nOutBufSize != sizeof(BOOL))) {
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        
        pInfo = (MapFlushInfo*) lpInBuf;
        *((BOOL*)lpOutBuf) = ChangeMapFlushing(pInfo->lpBaseAddress, pInfo->dwFlags);
        return TRUE;
    }
    case IOCTL_KLIB_GETALARMRESOLUTION: {
        extern DWORD dwNKAlarmResolutionMSec;
        if (lpInBuf || nInBufSize || !lpOutBuf || (nOutBufSize != sizeof(DWORD))) {
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        *((DWORD*)lpOutBuf) = dwNKAlarmResolutionMSec;
        return TRUE;
    }    
    case IOCTL_KLIB_ISKDPRESENT: {
        if (!lpOutBuf || (nOutBufSize != sizeof(DWORD))) {
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            return FALSE;
        } 
        *((DWORD*)lpOutBuf) = (g_pKdInit != NULL);
        return TRUE;
    }

    case IOCTL_KLIB_SETROMDLLBASE:
        return SetROMDllBase (nInBufSize, (PROMINFO) lpInBuf);

    case IOCTL_KLIB_GETPROCMEMINFO:
        // nInBufSize: process id
        // nOutBufSize: sizeof (PROCVMINFO)
        // lpOutBuf: pointer to PROCVMINFO structure
        if ((sizeof (PROCVMINFO) != nOutBufSize)    // size mis-match
            || !lpOutBuf                            // output NULL
            || (nInBufSize < 1)                     // valid process 1-31
            || (nInBufSize >= MAX_PROCESSES)) {
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        return GetVMInfo (nInBufSize, (PPROCVMINFO) lpOutBuf);

    case IOCTL_KLIB_GETCALLSTACK:
        //
        // nInBufSize: specify flag and skip (dwFlags = LOWORD(nInBufSize), dwSkip = HIWORD(nInBufSize))
        // lpInBuf: pointer to context (pCtx)
        // nOutBufSize: specify max # of frames (dwMaxFrames)
        // lpOutBuf: pointer to buffer to receive callstack frames (lpFrames)
        //
        return NKGetCallStackSnapshot (nOutBufSize, (CallSnapshot *) lpOutBuf, LOWORD(nInBufSize), HIWORD(nInBufSize), (PCONTEXT) lpInBuf);

    case IOCTL_KLIB_JITGETCALLSTACK:
        // nOutBufSize: max # of frames
        // lpOutBuf: pointer to buffer to receive the callstack
        return JITGetCallStack (NULL, nOutBufSize, (CallSnapshot *) lpOutBuf);

    case IOCTL_KLIB_SETIGNORENOFAULT:
        // nInBufSize: (BOOL) fDisableNoFault
        fDisableNoFault = nInBufSize;
        return TRUE;
    case IOCTL_KLIB_SETMEMORYATTR:
        // CeSetMemoryAttributes:
        // lpInBuf: pVirtualAddr
        // lpOutBuf: pShiftedPhysAddr
        // nInBufSize: cbSize
        // nOutBufSize: dwFlags
        return NKSetMemoryAttributes (lpInBuf, lpOutBuf, nInBufSize, nOutBufSize);

    default:
        return FALSE;
    }
    
    return TRUE;
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
    switch (dwIoControlCode) {
    case IOCTL_DBG_INIT:
        if (sizeof(g_pKdInit) == nInBufSize)
        {
            memcpy((VOID*)&g_pKdInit, lpInBuf, sizeof(g_pKdInit));
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

BOOL KernelLibIoControl_CeLog(
    DWORD   dwIoControlCode, 
    LPVOID  lpInBuf, 
    DWORD   nInBufSize, 
    LPVOID  lpOutBuf, 
    DWORD   nOutBufSize, 
    LPDWORD lpBytesReturned
    );

BOOL KernelLibIoControl_Verifier(
    DWORD   dwIoControlCode, 
    LPVOID  lpInBuf, 
    DWORD   nInBufSize, 
    LPVOID  lpOutBuf, 
    DWORD   nOutBufSize, 
    LPDWORD lpBytesReturned
    );

//------------------------------------------------------------------------------
//
// Entry to KernelLibIoControl from kernel mode code (trust already verified). 
// User mode code will call into the SC_KernelLibIoControl via trap so that 
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
    PKERNELMOD pKMod = (PKERNELMOD) hLib;

    switch ((DWORD) hLib) {

    case KMOD_OAL:
        return OEMIoControl(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);

    case KMOD_CORE:
        return KernelLibIoControl_Core(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);

    case KMOD_DBG:
        return KernelLibIoControl_Dbg(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        
    case KMOD_CELOG:
        return KernelLibIoControl_CeLog(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
    
    case KMOD_VERIFIER:
        return KernelLibIoControl_Verifier(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);

    default:

        if ((DWORD) pKMod > KMOD_MAX && (!IsValidModule(pKMod) || !IsValidModule (pKMod->pMod))) {
            KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
            return FALSE;
        } 
    }

    return pKMod->pfnIoctl? pKMod->pfnIoctl(pKMod->dwInstData, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned) : FALSE;
        
}


//------------------------------------------------------------------------------
//
// Entry to KernelLibIoControl from user mode code. Installable ISRs will
// call into the NKKernelLibIoControl since they run in kernel context and 
// the trust check can't follow pCurProc.
//
//------------------------------------------------------------------------------
BOOL SC_KernelLibIoControl(
    HANDLE  hLib, 
    DWORD   dwIoControlCode, 
    LPVOID  lpInBuf, 
    DWORD   nInBufSize, 
    LPVOID  lpOutBuf, 
    DWORD   nOutBufSize, 
    LPDWORD lpBytesReturned
    )
{
    TRUSTED_API (L"SC_KernelLibIoControl", FALSE);

    return NKKernelLibIoControl(hLib, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
}



