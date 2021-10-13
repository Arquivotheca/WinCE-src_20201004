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
//    process.c - implementations of process related functions
//
#include <windows.h>
#include <kernel.h>
#include <dbgrapi.h>

extern FARPROC g_pfnUsrLoadLibraryW;
long g_cpMinPageProcCreate;

void RestoreCoProcRegisters (PTHREAD pth);

#ifdef ARM

DWORD AllocateAlias (PALIAS *ppAlias, LPVOID pAddr, LPDWORD pfProtect)
{
    DWORD dwErr = 0;
    if (IsVirtualTaggedCache ()
        && !(*pfProtect & PAGE_PHYSICAL)
        && !IsKernelVa (pAddr)) {
        PALIAS pAlias = AllocMem (HEAP_ALIAS);

        if (!pAlias) {
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
        } else {
            pAlias->dwAliasBase = 0;
            *ppAlias = pAlias;
            *pfProtect |= PAGE_NOCACHE;
        }
    }
    return dwErr;
}

#endif

static BOOL PROCVMCopy (
    PPROCESS pprcDst,       // the destination process
    DWORD dwDestAddr,       // destination address
    HANDLE hProcSrc,        // the source process, NULL if PAGE_PHYSICAL or same as destination
    DWORD dwSrcAddr,        // source address
    DWORD cbSize,           // size, in bytes
    DWORD fProtect          // protection
)
{
    DWORD dwErr = 0;
    PHDATA   phdSrc  = LockHandleParam (hProcSrc, pActvProc);
    PPROCESS pprcSrc = GetProcPtr (phdSrc);

    DEBUGCHK (pActvProc == g_pprcNK);

#ifdef ARM
    DEBUGMSG ((IsVirtualTaggedCache() && !(fProtect & PAGE_PHYSICAL) && !IsKernelVa ((LPCVOID) dwSrcAddr)), (L"Detected alias with VirtualCopy. Callers are responsible for data consistency in cache.\r\n"));
#endif

    if (hProcSrc && !pprcSrc) {
        dwErr = ERROR_INVALID_HANDLE;

    } else if (IsQueryHandle (phdSrc)) {
        dwErr = ERROR_ACCESS_DENIED;

    } else if (   !VMFindAlloc (pprcDst, (LPVOID) dwDestAddr, cbSize, NULL)
               || !VMCopy (pprcDst, dwDestAddr, pprcSrc, dwSrcAddr, cbSize, fProtect)) {
        dwErr = ERROR_INVALID_PARAMETER;
    }
    UnlockHandleData (phdSrc);

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    return !dwErr;
}

LPVOID PROCVMAllocCopy (
    PPROCESS pprc,
    HANDLE   hDstProc,
    LPVOID   pAddr,
    DWORD    cbSize,
    DWORD    fProtect
    )
{
    DWORD    dwErr      = 0;
    LPVOID   lpRet      = NULL;
    PHDATA   phdDstProc = NULL;
    PPROCESS pprcDst    = g_pprcNK;
    PALIAS   pAlias     = NULL;

    DEBUGCHK (pActvProc == g_pprcNK);

    if (((int) cbSize <= 0)
        || ((DWORD) pAddr < VM_USER_BASE)) {
        dwErr = ERROR_INVALID_PARAMETER;

    } else if (hDstProc) {
        phdDstProc = LockHandleParam (hDstProc, g_pprcNK);
        pprcDst = GetProcPtr (phdDstProc);
        if (!pprcDst) {
            // error
            dwErr = ERROR_INVALID_HANDLE;

        } else if (IsQueryHandle(phdDstProc)) {
            // tgt process is query only handle
            dwErr = ERROR_ACCESS_DENIED;
        }
    }

    if (!dwErr
#ifdef ARM
        && (0 == (dwErr = AllocateAlias (&pAlias, pAddr, &fProtect)))
#endif
        ) {

        DWORD dwOfst = (DWORD) pAddr & VM_BLOCK_OFST_MASK;
        lpRet = VMAllocCopy (pprc, pprcDst, pAddr, cbSize, fProtect, pAlias);
        if (!lpRet || !VMAddAlloc (pprcDst, (LPBYTE) lpRet - dwOfst, cbSize + dwOfst, pAlias, NULL)) {
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
            if (lpRet) {
                VMFreeAndRelease (pprcDst, lpRet, cbSize);
                lpRet = NULL;
            }
#ifdef ARM
            VMUnalias (pAlias);
#endif
        }
    }

    UnlockHandleData (phdDstProc);

    if (dwErr) {
        SetLastError (dwErr);
#ifdef ARM
    } else {
        DEBUGMSG (pAlias && ZONE_VIRTMEM, (L"VMAllocCopy: Alias Created %8.8lx:%8.8lx -> %8.8lx:%8.8lx, size = %8.8lx\r\n",
            pprc->dwId, pAddr, pprcDst->dwId, lpRet, cbSize));
#endif
    }
    return lpRet;
}

//
// Internal version of VirtualAllocEx
//
LPVOID PROCVMAlloc (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, DWORD fAllocType, DWORD fProtect, PDWORD pdwTag)
{
    BOOL fReserve     = (!pAddr || (fAllocType & MEM_RESERVE));
    LPVOID pRet = NULL;
#ifdef ARM
    PVALIST pUncached = NULL;
#endif

    // disallow internal allocation flags
    if (fAllocType & VM_INTERNAL_ALLOC_TYPE) {
        DEBUGMSG (ZONE_ERROR, (L"ERROR: PROCVMAlloc - invalid allocation type = %8.8lx\r\n", fAllocType));
        SetLastError (ERROR_INVALID_PARAMETER);

#ifdef ARM
    } else if ((fProtect & PAGE_NOCACHE)
                && IsVirtualTaggedCache ()
                && (NULL == (pUncached = AllocMem (HEAP_VALIST)))) {

        DEBUGMSG (ZONE_ERROR, (L"ERROR: PROCVMAlloc - can't allocate uncached list (OOM)\r\n"));
        SetLastError (ERROR_NOT_ENOUGH_MEMORY);
#endif
    } else {

        pRet = VMAlloc (pprc, pAddr, cbSize, fAllocType, fProtect);

        if (pRet                                                // allocation successful
            && fReserve                                         // new VM reserved
            && (!VMAddAlloc (pprc, pRet, cbSize, NULL, pdwTag))) {        // add to VM-allocation list

            // new memory reserved, but can't add to process VA'd list
            VMFreeAndRelease (pprc, pRet, cbSize);
            pRet = NULL;
            SetLastError (ERROR_NOT_ENOUGH_MEMORY);
        }
#ifdef ARM
        if (pUncached) {
            if (pRet) {
                VMAddUncachedAllocation (pprc, (DWORD) pRet, pUncached);
            } else {
                FreeMem (pUncached, HEAP_VALIST);
            }
        }
#endif
    }

    return pRet;
}

extern DWORD dwMaxUserAllocAddr;

//
// External version of VirtualAllocEx
//
LPVOID EXTPROCVMAlloc (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, DWORD fAllocType, DWORD fProtect, PDWORD pdwTag)
{
    DWORD dwErr     = 0;
    LPVOID pRet     = NULL;
    PTHREAD pth     = pCurThread;
    BOOL fReserve   = (!pAddr || (fAllocType & MEM_RESERVE));

    if ((DWORD) pAddr >= dwMaxUserAllocAddr) {
        DEBUGMSG (ZONE_ERROR, (L"ERROR: PROCVMAlloc -Address out of range - pAddr = %8.8lx, cbSize = %8.8lx\r\n", pAddr, cbSize));
        dwErr = ERROR_INVALID_PARAMETER;

    } else if (!fReserve && !VMFindAlloc (pprc, pAddr, cbSize, NULL)) {
        DEBUGMSG (ZONE_ERROR, (L"ERROR: PROCVMAlloc -committing region not reserved, pAddr = %8.8lx, cbSize = %8.8lx\r\n", pAddr, cbSize));
        dwErr = ERROR_INVALID_PARAMETER;

    } else if (MEM_COMMIT & fAllocType) {

        PPROCESS pprcActv = pActvProc;

        // check for low mem conditions
        while ((PageFreeCount < g_cpCriticalThreshold) && (PageFreeCount_Pool > g_cpCriticalThreshold)) {

            // we are in low memory; let page out thread reclaim memory
            UB_Sleep (250);

            // check for terminated thread
            if ((pprcActv == pth->pprcOwner) && GET_DYING(pth) && !GET_DEAD(pth)) {
                DEBUGMSG (ZONE_ERROR, (L"ERROR: PROCVMAlloc -thread terminated - pth = %8.8x, pAddr = %8.8lx, cbSize = %8.8lx\r\n", pth, pAddr, cbSize));
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }
        }

        // limit allocations to only psl servers if memory is below critical
        if (!dwErr && !(pprc->fFlags & PROC_PSL_SERVER) && (PageFreeCount_Pool <= g_cpCriticalThreshold)) {
            DEBUGMSG (ZONE_ERROR, (L"ERROR: PROCVMAlloc -memory low for non-psl server - pAddr = %8.8lx, cbSize = %8.8lx\r\n", pAddr, cbSize));
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    if (!dwErr) {
        pRet = PROCVMAlloc (pprc, pAddr, cbSize, fAllocType, fProtect, pdwTag);

    } else {
        SetLastError (dwErr);
    }

    return pRet;
}


static BOOL PROCVMProtect (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, DWORD fNewProtect, LPDWORD pfOldProtect)
{
    BOOL fRet = FALSE;

    if (((int) cbSize <= 0)
        || !VMFindAlloc (pprc, pAddr, cbSize, NULL)) {
        SetLastError (ERROR_INVALID_PARAMETER);
    } else {
        fRet = VMProtect (pprc, pAddr, cbSize, fNewProtect, pfOldProtect);

    }

    return fRet;
}

BOOL PROCVMFree (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, DWORD dwFreeType, DWORD dwTag)
{
    BOOL fRet = FALSE;
    BOOL fRelease = (MEM_RELEASE == dwFreeType) && !cbSize;
    BOOL fDecommit = (MEM_DECOMMIT == dwFreeType) && ((int)cbSize > 0);
    DWORD dwItemTag = 0;

    if (fRelease) {
        fRet = VMRemoveAlloc (pprc, pAddr, dwTag);

    } else if (fDecommit) {
        fRet = VMFindAlloc (pprc, pAddr, cbSize, &dwItemTag)
            && (dwTag == dwItemTag)
            && VMDecommit (pprc, pAddr, cbSize, VM_PAGER_NONE);
    }

    if (!fRet) {
        SetLastError (ERROR_INVALID_PARAMETER);
        // failed VirtualFree usually means something really bad...
        NKDbgPrintfW (L"Virtual free failure!!! addr: 0x%8.8lx, cbsize: 0x%8.8lx, usr_tag: 0x%8.8lx, item_tag: 0x%8.8lx\r\n", (DWORD)pAddr, cbSize, dwTag, dwItemTag);
        DEBUGCHK (0);
    }

    return fRet;
}

static BOOL PROCCloseHandle (PPROCESS pprc, HANDLE h)
{
    BOOL fRet = FALSE;

    if ((DWORD) h >= 0x10000) {
        if (!((DWORD) h & 1)) {
            // Calling CloseHandle on a Process-id or thread-id
            DEBUGCHK (0);
        } else {
            fRet = HNDLCloseHandle (pprc, h);
        }
    }
    if (!fRet) {
        KSetLastError (pCurThread, ERROR_INVALID_HANDLE);
    }
    return fRet;
}

static BOOL EXTReadProcMemory (PPROCESS pprc, LPVOID pAddr, LPVOID pBuffer, DWORD cbSize)
{
    BOOL fRet = FALSE;

    if (!IsValidUsrPtr (pAddr, cbSize, FALSE)) {
        // reading kernel address - failed
        SetLastError (ERROR_INVALID_PARAMETER);
    } else {
        fRet = PROCReadMemory (pprc, pAddr, pBuffer, cbSize);

        // if read failed, check if this is a special address from debuggers
        if (    !fRet
            && pprc->pDbgrThrd
            && (pprc->pDbgrThrd->pprcOwner == pCurThread->pprcOwner)) {

            DWORD len = 0;
            LPCWSTR lpszName = NULL;
            PMODULE pModule = NULL;

            if (pAddr == pprc->BasePtr) {
                // process name is requested
                lpszName = pprc->lpszProcName;

            } else if (((DWORD)pAddr > VM_DLL_BASE)) {
                // module name is requested
                LockModuleList();
                pModule = FindModInProcByAddr(pprc, (DWORD)pAddr);
                if (pModule && pModule->BasePtr == pAddr) {
                    lpszName = pModule->lpszModName;
                }
                UnlockModuleList();
            }

            // copy the name out to the caller
            if (lpszName) {
                len = sizeof(WCHAR) * (NKwcslen(lpszName) + 1);
                if (!CeSafeCopyMemory (pBuffer, lpszName, (cbSize < len) ? cbSize : len)) {
                    SetLastError(ERROR_INVALID_PARAMETER);
                } else {
                    fRet = TRUE;
                }
            }
        }
    }

    return fRet;
}

static BOOL EXTWriteProcMemory (PPROCESS pprc, LPVOID pAddr, LPVOID pBuffer, DWORD cbSize)
{
    BOOL fRet = FALSE;
    if (!IsValidUsrPtr (pAddr, cbSize, TRUE)) {
        // writing to kernel address - failed
        SetLastError (ERROR_INVALID_PARAMETER);
    } else {
        fRet = PROCWriteMemory (pprc, pAddr, pBuffer, cbSize);
    }
    return fRet;
}

DWORD
EXTVMQuery (
    PPROCESS pprc,                      // process
    LPVOID lpvaddr,                     // address to query
    PMEMORY_BASIC_INFORMATION  pmi,     // structure to fill
    DWORD  dwLength                     // size of the buffer allocate for pmi
)
{
    MEMORY_BASIC_INFORMATION mi;    // use a local copy to guarantee no exception
    DWORD cbRet = VMQuery (pprc, lpvaddr, &mi, dwLength);
    if (cbRet && !CeSafeCopyMemory (pmi, &mi, cbRet)) {
        SetLastError (ERROR_INVALID_PARAMETER);
        cbRet = 0;
    }
    return cbRet;
}

static BOOL PROCPreClose (PPROCESS pprc)
{
    // if pprc->dwId is 0, the handle is closed due to process creation failure.
    // clean up will be done in PROCDelete.
    if (pprc->dwId) {
        // pre-close - close the special "process id" handle
        PHDATA phd = HNDLZapHandle (g_pprcNK, (HANDLE)pprc->dwId);
        DEBUGCHK (phd);
        // unlock twice, HNDLZapHandle locked the handle to get to phd
        DoUnlockHDATA (phd, -2*LOCKCNT_INCR);
    }
    return TRUE;
}

//
// CloseHandle() implementation for a query only process handle
//
static BOOL PROCQryDelete (PPROCESS pprc)
{
    if (pprc->dwId) {
        PHDATA phd = LockHandleData ((HANDLE)pprc->dwId, g_pprcNK);
        // unlock twice, LockHandleData locked the handle to get to phd
        DEBUGMSG (ZONE_LOADER1, (L"Process id=%8.8lx query handle is deleted\r\n", pprc->dwId));
        DoUnlockHDATA (phd, -2*LOCKCNT_INCR);
    }
    return TRUE;
}

//
// Function called for unsupported apis on query only process handle
//
static BOOL PROCSecFailed()
{
    if (pCurThread) {
        KSetLastError(pCurThread, ERROR_ACCESS_DENIED);
        DEBUGMSG (ZONE_ERROR, (L"Error! Calling unsupported process function on a query only handle.\r\n"));
        DEBUGCHK (0);
    }
    return FALSE;
}

/*

Note about the API set: All APIs should either return 0 on error or return
no value at-all. This will allow us to return gracefully to the user when an
argument to an API doesn't pass the PSL checks. Otherwise we will end
up throwing exceptions on invalid arguments to API calls.

*/

static const PFNVOID ProcExtMthds[] = {
    (PFNVOID)PROCDelete,            // CloseHandle
    (PFNVOID)PROCPreClose,
    (PFNVOID)PROCTerminate,         // 2 - TerminateProcess
    (PFNVOID)NotSupported,          // 3 - VirtualSetAttributes
    (PFNVOID)PROCFlushICache,       // 4 - FlushInstructionCache
    (PFNVOID)EXTReadProcMemory,     // 5 - ReadProcessMemory
    (PFNVOID)EXTWriteProcMemory,    // 6 - WriteProcessMemory
    (PFNVOID)PROCLoadModule,        // 7 - CeLoadLibraryInProcess
    (PFNVOID)PROCGetModInfo,        // 8 - CeGetModuleInfo
    (PFNVOID)PROCSetVer,            // 9 - SetProcessVersion
    (PFNVOID)EXTPROCVMAlloc,        // 10 - VirtualAllocEx
    (PFNVOID)PROCVMFree,            // 11 - VirtualFreeEx
    (PFNVOID)EXTVMQuery,            // 12 - VirtualQueryEx
    (PFNVOID)PROCVMProtect,         // 13 - VirtualProtectEx
    (PFNVOID)NotSupported,          // 14 - VirtualCopyEx
    (PFNVOID)NotSupported,          // 15 - LockPagesEx
    (PFNVOID)NotSupported,          // 16 - UnlockPagesEx
    (PFNVOID)PROCCloseHandle,       // 17 - CloseHandleInProc
    (PFNVOID)MAPUnmapView,          // 18 - UnmapViewOfFile
    (PFNVOID)MAPFlushView,          // 19 - FlushViewOfFile
    (PFNVOID)MAPMapView,            // 20 - MapViewOfFile
    (PFNVOID)NKCreateFileMapping,   // 21 - CreateFileMapping
    (PFNVOID)NotSupported,          // 22 - Unsupported
    (PFNVOID)PROCGetModName,        // 23 - GetModuleFileNameW
    (PFNVOID)LDRGetModByName,       // 24 - GetModuleHandleW
    (PFNVOID)VMAllocPhys,           // 25 - AllocPhysMemEx (FreePhysMem is just VirtualFreeEx)
    (PFNVOID)PROCReadPEHeader,      // 26 - ReadPEHeader
    (PFNVOID)PROCChekDbgr,          // 27 - CheckRemoteDebuggerPresent
    (PFNVOID)NKOpenMsgQueueEx,      // 28 - OpenMsgQueueEx
    (PFNVOID)PROCGetID,             // 29 - GetProcessId
    (PFNVOID)NotSupported,          // 30 - VirtualAllocCopyEx
    (PFNVOID)NotSupported,          // 31 - PageOutModule
    (PFNVOID)PROCGetAffinity,       // 32 - CeGetProcessAffinity
    (PFNVOID)PROCSetAffinity,       // 33 - CeSetProcessAffinity
    (PFNVOID)NotSupported,          // 34 - CeMapUserAddressesToVoid
    (PFNVOID)PROCGetTimes,          // 35 - GetProcessTimes
};

static const PFNVOID ProcQueryMthds[] = {
    (PFNVOID)PROCQryDelete,          // 0 - CloseHandle
    (PFNVOID)0,
    (PFNVOID)PROCSecFailed,          // 2 - TerminateProcess
    (PFNVOID)PROCSecFailed,          // 3 - VirtualSetAttributes
    (PFNVOID)PROCSecFailed,          // 4 - FlushInstructionCache
    (PFNVOID)PROCSecFailed,          // 5 - ReadProcessMemory
    (PFNVOID)PROCSecFailed,          // 6 - WriteProcessMemory
    (PFNVOID)PROCSecFailed,          // 7 - CeLoadLibraryInProcess
    (PFNVOID)PROCGetModInfo,         // 8 - CeGetModuleInfo
    (PFNVOID)PROCSecFailed,          // 9 - SetProcessVersion
    (PFNVOID)PROCSecFailed,          // 10 - VirtualAllocEx
    (PFNVOID)PROCSecFailed,          // 11 - VirtualFreeEx
    (PFNVOID)EXTVMQuery,             // 12 - VirtualQueryEx
    (PFNVOID)PROCSecFailed,          // 13 - VirtualProtectEx
    (PFNVOID)PROCSecFailed,          // 14 - VirtualCopyEx
    (PFNVOID)PROCSecFailed,          // 15 - LockPagesEx
    (PFNVOID)PROCSecFailed,          // 16 - UnlockPagesEx
    (PFNVOID)PROCSecFailed,          // 17 - CloseHandleInProc
    (PFNVOID)PROCSecFailed,          // 18 - UnmapViewOfFile
    (PFNVOID)PROCSecFailed,          // 19 - FlushViewOfFile
    (PFNVOID)PROCSecFailed,          // 20 - MapViewOfFile
    (PFNVOID)PROCSecFailed,          // 21 - CreateFileMapping
    (PFNVOID)NotSupported,           // 22 - Unsupported
    (PFNVOID)PROCGetModName,         // 23 - GetModuleFileNameW
    (PFNVOID)LDRGetModByName,        // 24 - GetModuleHandleW
    (PFNVOID)PROCSecFailed,          // 25 - AllocPhysMemEx
    (PFNVOID)PROCSecFailed,          // 26 - ReadPEHeader
    (PFNVOID)PROCChekDbgr,           // 27 - CheckRemoteDebuggerPresent
    (PFNVOID)PROCSecFailed,          // 28 - OpenMsgQueueEx
    (PFNVOID)PROCGetID,              // 29 - GetProcessId
    (PFNVOID)PROCSecFailed,          // 30 - VirtualAllocCopyEx
    (PFNVOID)PROCSecFailed,          // 31 - PageOutModule
    (PFNVOID)PROCGetAffinity,        // 32 - CeGetProcessAffinity
    (PFNVOID)PROCSecFailed,          // 33 - CeSetProcessAffinity
    (PFNVOID)PROCSecFailed,          // 34 - CeMapUserAddressesToVoid
    (PFNVOID)PROCGetTimes,           // 35 - GetProcessTimes
};

static const PFNVOID ProcIntMthds[] = {
    (PFNVOID)PROCDelete,            // CloseHandle
    (PFNVOID)PROCPreClose,
    (PFNVOID)PROCTerminate,         // 2 - TerminateProcess
    (PFNVOID)VMSetAttributes,       // 3 - VirtualSetAttributes
    (PFNVOID)PROCFlushICache,       // 4 - FlushInstructionCache
    (PFNVOID)PROCReadMemory,        // 5 - ReadProcessMemory
    (PFNVOID)PROCWriteMemory,       // 6 - WriteProcessMemory
    (PFNVOID)PROCLoadModule,        // 7 - CeLoadLibraryInProcess
    (PFNVOID)PROCGetModInfo,        // 8 - CeGetModuleInfo
    (PFNVOID)PROCSetVer,            // 9 - SetProcessVersion
    (PFNVOID)PROCVMAlloc,           // 10 - VirtualAllocEx
    (PFNVOID)PROCVMFree,            // 11 - VirtualFreeEx
    (PFNVOID)VMQuery,               // 12 - VirtualQueryEx
    (PFNVOID)PROCVMProtect,         // 13 - VirtualProtectEx
    (PFNVOID)PROCVMCopy,            // 14 - VirtualCopyEx
    (PFNVOID)VMLockPages,           // 15 - LockPagesEx
    (PFNVOID)VMUnlockPages,         // 16 - UnlockPagesEx
    (PFNVOID)PROCCloseHandle,       // 17 - CloseHandleInProc
    (PFNVOID)MAPUnmapView,          // 18 - UnmapViewOfFile
    (PFNVOID)MAPFlushView,          // 19 - FlushViewOfFile
    (PFNVOID)MAPMapView,            // 20 - MapViewOfFile
    (PFNVOID)NKCreateFileMapping,   // 21 - CreateFileMapping
    (PFNVOID)NotSupported,          // 22 - Unsupported
    (PFNVOID)PROCGetModName,        // 23 - GetModuleFileNameW
    (PFNVOID)LDRGetModByName,       // 24 - GetModuleHandleW
    (PFNVOID)VMAllocPhys,           // 25 - AllocPhysMemEx (FreePhysMem is just VirtualFreeEx)
    (PFNVOID)PROCReadPEHeader,      // 26 - ReadPEHeader
    (PFNVOID)PROCChekDbgr,          // 27 - CheckRemoteDebuggerPresent
    (PFNVOID)NKOpenMsgQueueEx,      // 28 - OpenMsgQueueEx
    (PFNVOID)PROCGetID,             // 29 - GetProcessId
    (PFNVOID)PROCVMAllocCopy,       // 30 - VirtualAllocCopyEx
    (PFNVOID)PROCPageOutModule,     // 31 - PageOutModule
    (PFNVOID)PROCGetAffinity,       // 32 - CeGetProcessAffinity
    (PFNVOID)PROCSetAffinity,       // 33 - CeSetProcessAffinity
    (PFNVOID)VMMapUserAddrToVoid,   // 34 - CeMapUserAddressesToVoid
    (PFNVOID)PROCGetTimes,          // 35 - GetProcessTimes
};


static const ULONGLONG ProcSigs[] = {
    FNSIG1 (DW),                                // CloseHandle
    0,
    FNSIG2 (DW, DW),                            // 2 - TerminateProcess
    FNSIG6 (DW, O_PTR, DW, DW, DW, IO_PDW),     // 3 - VirtualSetAttributes
    FNSIG3 (DW, I_PTR, DW),                     // 4 - FlushInstructionCache
    FNSIG4 (DW, DW, O_PTR, DW),                 // 5 - ReadProcessMemory - NOTE: the "address" argument of Read/WriteProcessMemory
    FNSIG4 (DW, DW, I_PTR, DW),                 // 6 - WriteProcessMemory        is validated in the API itself
    FNSIG2 (DW, I_WSTR),                        // 7 - CeLoadLibraryInProcess
    FNSIG5 (DW, DW, DW, O_PTR, DW),             // 8 - CeGetModuleInfo
    FNSIG2 (DW, DW),                            // 9 - SetProcessVersion
    FNSIG6 (DW, O_PTR, DW, DW, DW, IO_PDW),     // 10 - VirtualAllocEx
    FNSIG5 (DW, O_PTR, DW, DW, DW),             // 11 - VirtualFreeEx
    FNSIG4 (DW, I_PDW, O_PTR, DW),              // 12 - VirtualQueryEx
    FNSIG5 (DW, O_PTR, DW, DW, IO_PDW),         // 13 - VirtualProtectEx
    FNSIG6 (DW, DW, DW, I_PTR, DW, DW),         // 14 - VirtualCopyEx
    FNSIG5 (DW, O_PTR, DW, IO_PDW, DW),         // 15 - LockPagesEx
    FNSIG3 (DW, O_PTR, DW),                     // 16 - UnlockPagesEx
    FNSIG2 (DW, DW),                            // 17 - CloseHandleInProc
    FNSIG2 (DW, DW),                            // 18 - UnmapViewOfFile
    FNSIG3 (DW, O_PTR, DW),                     // 19 - FlushViewOfFile
    FNSIG6 (DW, DW, DW, DW, DW, DW),            // 20 - MapViewOfFile
    FNSIG7 (DW, DW, IO_PDW, DW, DW, DW, I_WSTR),// 21 - CreateFileMapping
    FNSIG3 (DW, O_PTR, DW),                     // 22 - Unsupported
    FNSIG4 (DW, DW, O_PTR, DW),                 // 23 - GetModuleFileNameW
    FNSIG3 (DW, I_WSTR, DW),                    // 24 - GetModuleHandleW
    FNSIG6 (DW, DW, DW, DW, DW, IO_PDW),        // 25 - AllocPhysMemEx (FreePhysMem is just VirtualFreeEx)
    FNSIG5 (DW, DW, DW, O_PTR, DW),             // 26 - ReadPEHeader
    FNSIG2 (DW, IO_PDW),                        // 27 - CheckRemoteDebuggerPresent
    FNSIG4 (DW, DW, DW, IO_PDW),                // 28 - OpenMsgQueueEx
    FNSIG1 (DW),                                // 29 - GetProcessId
    FNSIG5 (DW, DW, I_PTR, DW, DW),             // 30 - VirtualAllocCopyEx
    FNSIG3 (DW, DW, DW),                        // 31 - PageOutModule
    FNSIG2 (DW, IO_PDW),                        // 32 - CeGetProcessAffinity
    FNSIG2 (DW, DW),                            // 33 - CeSetProcessAffinity
    FNSIG3 (DW, O_PTR, DW),                     // 34 - CeMapUserAddressesToVoid
    FNSIG5 (DW, O_PI64, O_PI64, O_PI64, O_PI64),// 35 - GetProcessTimes
};

// taken from sdk\inc\winnt.h
static const DWORD ProcAccessMask[] = {
    0,                                          // 0 - CloseHandle
    0,                                          // 1 - PreCloseHandle
    PROCESS_TERMINATE,                          // 2 - TerminateProcess
    PROCESS_VM_OPERATION,                       // 3 - VirtualSetAttributes
    PROCESS_VM_OPERATION,                       // 4 - FlushInstructionCache
    PROCESS_VM_OPERATION | PROCESS_VM_READ,     // 5 - ReadProcessMemory
    PROCESS_VM_OPERATION | PROCESS_VM_WRITE,    // 6 - WriteProcessMemory
    PROCESS_VM_OPERATION,                       // 7 - CeLoadLibraryInProcess
    PROCESS_QUERY_INFORMATION,                  // 8 - CeGetModuleInfo
    PROCESS_SET_INFORMATION,                    // 9 - SetProcessVersion
    PROCESS_VM_OPERATION,                       // 10 - VirtualAllocEx
    PROCESS_VM_OPERATION,                       // 11 - VirtualFreeEx
    PROCESS_QUERY_INFORMATION,                  // 12 - VirtualQueryEx
    PROCESS_VM_OPERATION,                       // 13 - VirtualProtectEx
    PROCESS_VM_OPERATION,                       // 14 - VirtualCopyEx
    PROCESS_VM_OPERATION,                       // 15 - LockPagesEx
    PROCESS_VM_OPERATION,                       // 16 - UnlockPagesEx
    PROCESS_VM_OPERATION,                       // 17 - CloseHandleInProc
    PROCESS_VM_OPERATION,                       // 18 - UnmapViewOfFile
    PROCESS_VM_OPERATION,                       // 19 - FlushViewOfFile
    PROCESS_VM_OPERATION,                       // 20 - MapViewOfFile
    PROCESS_VM_OPERATION,                       // 21 - CreateFileMapping
    PROCESS_QUERY_INFORMATION,                  // 22 - Unsupported
    PROCESS_QUERY_INFORMATION,                  // 23 - GetModuleFileNameW
    PROCESS_QUERY_INFORMATION,                  // 24 - GetModuleHandleW
    PROCESS_VM_OPERATION,                       // 25 - AllocPhysMemEx (FreePhysMem is just VirtualFreeEx)
    PROCESS_QUERY_INFORMATION,                  // 26 - ReadPEHeader
    PROCESS_QUERY_INFORMATION,                  // 27 - CheckRemoteDebuggerPresent
    PROCESS_DUP_HANDLE,                         // 28 - OpenMsgQueueEx
    PROCESS_QUERY_INFORMATION,                  // 29 - GetProcessId
    PROCESS_VM_OPERATION,                       // 30 - VirtualAllocCopyEx
    PROCESS_VM_OPERATION,                       // 31 - PageOutModule
    PROCESS_QUERY_INFORMATION,                  // 32 - CeGetProcessAffinity
    PROCESS_SET_INFORMATION,                    // 33 - CeSetProcessAffinity
    PROCESS_VM_OPERATION,                       // 34 - CeMapUserAddressesToVoid
    PROCESS_QUERY_INFORMATION,                  // 35 - GetProcessTimes
};

ERRFALSE (sizeof(ProcExtMthds) == sizeof(ProcIntMthds));
ERRFALSE (sizeof(ProcExtMthds) == sizeof(ProcAccessMask));
ERRFALSE ((sizeof(ProcExtMthds) / sizeof(ProcExtMthds[0])) == (sizeof(ProcSigs) / sizeof(ProcSigs[0])));


const CINFO cinfProcQuery = {
    { 'P', 'R', 'O', 'C' },
    DISPATCH_KERNEL_PSL,
    SH_CURPROC,
    sizeof(ProcQueryMthds)/sizeof(ProcQueryMthds[0]),
    ProcQueryMthds,
    ProcQueryMthds,
    ProcSigs,
    0,
    0,
    0,
    ProcAccessMask
};

const CINFO cinfProc = {
    { 'P', 'R', 'O', 'C' },
    DISPATCH_KERNEL_PSL,
    SH_CURPROC,
    sizeof(ProcExtMthds)/sizeof(ProcExtMthds[0]),
    ProcExtMthds,
    ProcIntMthds,
    ProcSigs,
    0,
    0,
    0,
    ProcAccessMask
};

//
// SwitchVM - switch VM to another process, return the old process
//
PPROCESS SwitchVM (PPROCESS pprc)
{
    PTHREAD  pth     = pCurThread;
    PPROCESS pprcRet = pth->pprcVM;

    pth->pprcVM = pprc;

    if (pprc && (pprc != pVMProc)) {
        PcbSetVMProc (pprc);
        MDSwitchVM (pprc);
    }
    return pprcRet;
}

LPCWSTR SafeGetProcName (PPROCESS pprc)
{
    LPCWSTR pszProcName = pprc->lpszProcName? pprc->lpszProcName : L"";
    return IsKernelVa (pszProcName)? pszProcName : L"";
}

void SetCPUASID (PTHREAD pth)
{
    PPROCESS pprcNewVM = pth->pprcVM;
    PPCB     ppcb = GetPCB ();

    DEBUGCHK (InPrivilegeCall ());

    if (!pprcNewVM) {
        // kernel thread with no VM affinity. Only switch VM if the current VM is in trancient state
        pprcNewVM = IsProcessNormal (ppcb->pVMPrc)? ppcb->pVMPrc : g_pprcNK;
    }

    ppcb->pVMPrc      = pprcNewVM;
    ppcb->pCurPrc     = pth->pprcActv;
    ppcb->lpvTls      = pth->tlsPtr;
    ppcb->pCurThd     = pth;
    ppcb->CurThId     = pth->dwId;
    ppcb->ActvProcId  = pth->pprcActv->dwId;
    ppcb->OwnerProcId = pth->pprcOwner->dwId;

#ifdef x86
    // x86 - update registration pointer (fs)
    UpdateRegistrationPtr (TLS2PCR (pth->tlsPtr));

    if (pth->lpFPEmul) {
        // if using floating point emulation, switch to the right handler
        InitFPUIDTs (pth->lpFPEmul);
    }

#endif

    MDSetCPUASID ();
    
    if(pth != pCurThread) {
        RestoreCoProcRegisters (pth);
    }
}

//
// SwitchActiveProcess - switch to another process, return the the old process
//
PPROCESS SwitchActiveProcess (PPROCESS pprc)
{
    PPROCESS pprcOld = pActvProc;
    if (pprc != pprcOld) {
        pCurThread->pprcActv = pprc;
        PcbSetActvProc (pprc);
        PcbSetActvProcId (pprc->dwId);
        MDSwitchActive (pprc);
        CELOG_ThreadMigrate(dwActvProcId);
    }
    return pprcOld;
}


//
// PROCTerminate - terminate a process
//
BOOL PROCTerminate (PPROCESS pprc, DWORD dwExitCode)
{
    DWORD   dwErr = 0;
    PTHREAD pth   = pCurThread;

    DEBUGMSG(ZONE_ENTRY,(L"PROCTerminate entry: %8.8lx %8.8lx\r\n", pprc, dwExitCode));

    // check for kernel process
    if (pprc == g_pprcNK) {
        DEBUGCHK (0);
        dwErr = ERROR_ACCESS_DENIED;

    } else {

        PTHREAD pMainTh = NULL;

        if (pprc == pActvProc) {
            // exit process. No need to grab loader lock, for the main thread must be the last
            // thread exited. i.e. pMainTh below must be valid.
            DEBUGCHK (pprc == pth->pprcOwner);

            pMainTh = MainThreadOfProcess (pprc);

            KillOneThread (pMainTh, dwExitCode);

            // only notify PSL of process exiting if it's not main thread calling ExitProcess.
            if (pth != pMainTh) {
                NKPSLNotify (DLL_PROCESS_EXITING, pprc->dwId, 0);

                // if we're not main thread, make sure we don't return to caller of TerminateProcess/ExitProcess.
                KillOneThread (pth, 0);
            }

        } else {
            // terminate process, need to grab the loader lock to be safe to look at main thread
            LockLoader (pprc);

            if (IsProcessAlive (pprc)) {
                pprc->fFlags |= PROC_TERMINATED;
                pMainTh = MainThreadOfProcess (pprc);
                KillOneThread (pMainTh, dwExitCode);
            } else {
                // process already exited
                dwErr = ERROR_INVALID_HANDLE;
            }

            UnlockLoader (pprc);

            if (pMainTh) {
                NKPSLNotify (DLL_PROCESS_EXITING, pprc->dwId, 0);

                if (NKWaitForKernelObject ((HANDLE) pprc->dwId, 3000) == WAIT_TIMEOUT) {
                    DEBUGMSG (ZONE_ERROR, (L"ERROR: PROCTerminate - Unable to Terminate Process, id = %8.8lx\r\n", pprc->dwId));
                    dwErr = ERROR_BUSY;
                }
            }
        }

    }

    if (dwErr) {
        KSetLastError (pth, dwErr);
    }

    DEBUGMSG(ZONE_ENTRY,(L"PROCTerminate exit: dwErr = %8.8lx\r\n", dwErr));

    return !dwErr;
}

//------------------------------------------------------------------------------
// PROCFlushICache - flush instruction cache
//------------------------------------------------------------------------------
BOOL PROCFlushICache (PPROCESS pprc, LPCVOID lpBaseAddress, DWORD dwSize)
{
    // write-back d-cache and flush i-cache
    NKCacheRangeFlush (NULL, 0, CACHE_SYNC_WRITEBACK|CACHE_SYNC_INSTRUCTIONS);
    return TRUE;
}

//------------------------------------------------------------------------------
// PROCReadMemory - read process memory
//------------------------------------------------------------------------------
BOOL PROCReadMemory (PPROCESS pprc, LPVOID pAddr, LPVOID pBuffer, DWORD cbSize)
{
    DWORD dwErr;

    DEBUGMSG(ZONE_ENTRY,(L"PROCReadMemory entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                    pprc, pAddr, pBuffer, cbSize));

    dwErr = VMReadProcessMemory (pprc, pAddr, pBuffer, cbSize);

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    DEBUGMSG(ZONE_ENTRY,(L"PROCReadMemory exit: dwErr = %8.8lx\r\n", dwErr));
    return !dwErr;
}

//------------------------------------------------------------------------------
// PROCWriteMemory - write process memory
//------------------------------------------------------------------------------
BOOL PROCWriteMemory (PPROCESS pprc, LPVOID pAddr, LPVOID pBuffer, DWORD cbSize)
{
    DWORD dwErr;

    DEBUGMSG(ZONE_ENTRY,(L"PROCWriteMemory entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                    pprc, pAddr, pBuffer, cbSize));

    dwErr = VMWriteProcessMemory (pprc, pAddr, pBuffer, cbSize);

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    DEBUGMSG(ZONE_ENTRY,(L"PROCWriteMemory exit: dwErr = %8.8lx\r\n", dwErr));
    return !dwErr;
}

//------------------------------------------------------------------------------
// PROCSetVer - change process version
//------------------------------------------------------------------------------
BOOL PROCSetVer (PPROCESS pprc, DWORD dwVersion)
{

    DEBUGMSG(ZONE_ENTRY,(L"PROCSetVer entry: %8.8lx %8.8lx\r\n", pprc, dwVersion));

    pprc->e32.e32_cevermajor = (BYTE) HIWORD (dwVersion);
    pprc->e32.e32_ceverminor = (BYTE) LOWORD (dwVersion);

    DEBUGMSG(ZONE_ENTRY,(L"PROCSetVer exit: %8.8lx\r\n", TRUE));
    return TRUE;
}


//------------------------------------------------------------------------------
// PROCReadPEHeader - find the address within a module in given process
//------------------------------------------------------------------------------
BOOL PROCReadPEHeader (PPROCESS pprc, DWORD dwFlags, DWORD dwAddr, LPVOID pBuf, DWORD cbSize)
{
    DWORD dwErr = 0;
    DWORD dwRet = RP_RESULT_FROM_MODULE;

    // validate parameter
    if (cbSize < sizeof (e32_lite)) {                            // size big enough?
        dwErr = ERROR_INVALID_PARAMETER;

    } else {

        PMODULE pModRes = NULL;
        e32_lite *eptr  = NULL;
        DWORD   dwBase  = 0;

        if (!dwAddr) {

            if (dwFlags & RP_READ_PE_BY_MODULE) {
                // process
                eptr    = &pprc->e32;
                dwBase  = (DWORD) pprc->BasePtr;
                pModRes = pprc->pmodResource;
            }

        } else {

            PMODULE   pMod = NULL;

            LockModuleList ();

            if (!(dwFlags & RP_READ_PE_BY_MODULE)) {
                pMod = FindModInProcByAddr (pprc, dwAddr);

            } else if (FindModInProc (pprc, (PMODULE) dwAddr)) {
                pMod = (PMODULE) dwAddr;
            }

            UnlockModuleList ();

            if (pMod) {
                eptr    = &pMod->e32;
                dwBase  = (DWORD) pMod->BasePtr;
                if (dwFlags & RP_READ_PE_BY_MODULE) {
                    pModRes = pMod->pmodResource;
                }

            } else if (!(dwFlags & RP_READ_PE_BY_MODULE)) {

                if  (dwAddr - (DWORD) g_pprcNK->BasePtr < g_pprcNK->e32.e32_vsize) {
                    eptr    = &g_pprcNK->e32;
                    dwBase  = (DWORD) g_pprcNK->BasePtr;

                } else if (dwAddr - (DWORD) pprc->BasePtr < pprc->e32.e32_vsize) {
                    eptr    = &pprc->e32;
                    dwBase  = (DWORD) pprc->BasePtr;

                }

            }
        }

        if (eptr) {
            __try {
                // try MUI if specified in flag
                if (pModRes && (RP_MUI_IF_EXIST & dwFlags)) {
                    eptr   = &pModRes->e32;
                    dwBase = (DWORD) pModRes->BasePtr;
                    dwRet = RP_RESULT_FROM_MUI;
                }

                memcpy (pBuf, eptr, sizeof (e32_lite));
                ((e32_lite*)pBuf)->e32_vbase = dwBase;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                dwErr = ERROR_INVALID_PARAMETER;
            }
        } else {
            dwErr = ERROR_FILE_NOT_FOUND;
        }
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    return dwErr? RP_RESULT_FAILED : dwRet;
}

//------------------------------------------------------------------------------
// PROCReadKernelPEHeader - find the address within a module in kernel process
//------------------------------------------------------------------------------
BOOL PROCReadKernelPEHeader (DWORD dwFlags, DWORD dwAddr, LPVOID pBuf, DWORD cbSize)
{
    return PROCReadPEHeader(g_pprcNK, dwFlags, dwAddr, pBuf, cbSize);
}


BOOL PROCChekDbgr (PPROCESS pprc, PBOOL pfRet)
{
    if (pprc && pfRet)
    {
        *pfRet = (pprc->pDbgrThrd) ? TRUE : FALSE;
        return TRUE;
    }
    else
    {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        return FALSE;
    }
}


//
// Load a dll in a given process. Used by AppVerifier
// to connect to a running process.
//
// Note: Caller is assumed to have OpenProcess()
// privilege on the target process.
//
BOOL PROCLoadModule (PPROCESS pprc, LPCWSTR lpszModName)
{
    BOOL fRet = FALSE;

    // if kernel fail the call
    if (pprc == g_pprcNK) {
        DEBUGMSG (ZONE_ERROR, (L"CeLoadLibraryInProcess: Failed to load dll in kernel. Use LoadKernelLibrary().\r\n"));

    } else {

        // make a kernel copy of the module name
        WCHAR wszModuleName[MAX_PATH];
        DWORD cchModuleName = CopyPath(wszModuleName, lpszModName, MAX_PATH);

        if (!cchModuleName) {
            DEBUGMSG (ZONE_ERROR, (L"CeLoadLibraryInProcess: Invalid module name\r\n"));

        } else {

            // switch to target process
            PPROCESS pprcActv = SwitchActiveProcess (g_pprcNK);
            PPROCESS pprcVM   = SwitchVM (pprc);

            // create a thread in target process to load the dll
            PHDATA phd = THRDCreate (pprc, g_pfnUsrLoadLibraryW, NULL, KRN_STACK_SIZE, TH_UMODE, THREAD_RT_PRIORITY_NORMAL);
            PTHREAD pth = GetThreadPtr(phd);

            if (!pth) {
                DEBUGMSG (ZONE_ERROR, (L"CeLoadLibraryInProcess: Failed to set target thread properties\r\n"));

            } else {

                // leave space on stack for name argument
                DWORD cbExtraArgs = 4*REGSIZE; // space for calling convention (mips/sh) and args on stack (x86)
                DWORD cbLen = (cchModuleName + 1) * sizeof(WCHAR);
                LPBYTE CurSp, lpNameBuffer;

PREFAST_SUPPRESS(6305, "The calculation below always uses # of bytes to figure out stack pointer position");
                CurSp = (LPBYTE) (pth->tlsPtr - SECURESTK_RESERVE - ALIGNSTK(cbLen+cbExtraArgs));
                lpNameBuffer = CurSp + cbExtraArgs;

                // copy the module name to stack
                memcpy (lpNameBuffer, wszModuleName, cbLen);

                // adjust the sp and the arguments
                THRD_CTX_TO_SP(pth) = (DWORD)CurSp;
                THRD_CTX_TO_PARAM_1(pth) = (ulong)g_pfnUsrLoadLibraryW;
                THRD_CTX_TO_PARAM_2(pth) = (ulong)lpNameBuffer;

                // set the return value (to fault since this thread should never return)
#ifdef x86
                *((LPDWORD)(CurSp)) = 0; // return address
#elif defined (MIPS)
                pth->ctx.IntRa = 0;
#else
                CONTEXT_TO_RETURN_ADDRESS((&(pth->ctx))) = 0;
#endif
                // resume thread
                THRDResume (pth, NULL);
                fRet = TRUE;
            }

            UnlockHandleData (phd);

            // switch back
            SwitchActiveProcess (pprcActv);
            SwitchVM (pprcVM);
        }
    }

    return fRet;
}

#ifndef x86
static DWORD DoGetFncTbl (PPROCESS pprc, const e32_lite *eptr, LPVOID lpBaseAddr, LPBYTE pBuf, DWORD cbSize)
{

    DWORD cbRet = eptr->e32_unit[EXC].size;

    DEBUGCHK (pBuf);

    if (cbRet > cbSize) {
        KSetLastError (pCurThread, ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }

    if ((pprc == pVMProc) || IsKModeAddr ((DWORD) lpBaseAddr)) {
        // kernel address or current VM, memcpy
         memcpy (pBuf, (LPBYTE) lpBaseAddr + eptr->e32_unit[EXC].rva, cbRet);
    } else {
        DWORD dwErr = VMReadProcessMemory (pprc, (LPBYTE) lpBaseAddr + eptr->e32_unit[EXC].rva, pBuf, cbRet);
        if (dwErr) {
            cbRet = 0;
            KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        }
    }

    return cbRet;

}
#endif

static DWORD DoGetFileAttr (const openexe_t *oeptr, LPBYTE pBuf, DWORD cbSize)
{
    DWORD dwAttr = 0;
    DWORD dwErr  = 0;
    BY_HANDLE_FILE_INFORMATION bhfi;

    DEBUGCHK (pBuf);

    if (sizeof(DWORD) > cbSize) {
        dwErr = ERROR_INSUFFICIENT_BUFFER;

    } else if (FA_DIRECTROM & oeptr->filetype) {
        // XIP kernel
        dwAttr = oeptr->tocptr->dwFileAttributes;

    } else if (!FSGetFileInfo (oeptr->hf, &bhfi)) {
        dwErr = ERROR_INVALID_PARAMETER;

    } else {
        dwAttr = bhfi.dwFileAttributes;
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    } else {
        *(LPDWORD) pBuf = dwAttr;
    }

    return dwErr? 0 : sizeof (DWORD);
 }

DWORD DoGetFullPath (const openexe_t *oeptr, LPWSTR pBuf, DWORD cchMax)
{
    DWORD cchRet = 0;

    if (oeptr->lpName) {
        cchRet = NKwcslen (oeptr->lpName->name);
    } else {
        DEBUGCHK (FA_DIRECTROM & oeptr->filetype);
        cchRet = strlen (oeptr->tocptr->lpszFileName) + 9;  // + 9 for "\\windows\\"
    }

    if (pBuf && (cchRet >= cchMax)) {
        SetLastError (ERROR_INSUFFICIENT_BUFFER);
        return 0;

    }

    if (pBuf && oeptr->lpName) {
        NKwcsncpy (pBuf, oeptr->lpName->name, cchMax);

    } else if (pBuf) {
        LPCSTR pROMFileName = oeptr->tocptr->lpszFileName;          // ROM file name is ASCII string
#define WINDIR_LEN      9
        memcpy (pBuf, L"\\Windows\\", WINDIR_LEN*sizeof(WCHAR));
        pBuf += WINDIR_LEN;
        while (0 != (*pBuf ++ = *pROMFileName ++))
            ;
    }

    return cchRet;
}

//------------------------------------------------------------------------------
// PROCGetModInfo - get process/module information
//------------------------------------------------------------------------------
DWORD PROCGetModInfo (PPROCESS pprc, LPVOID pBaseAddr, DWORD infoType, LPVOID pBuf, DWORD cbBuf)
{
    DWORD       dwErr  = ERROR_INVALID_PARAMETER;
    PMODULELIST pEntry = NULL;
    PMODULE     pMod   = NULL;
    const e32_lite   *eptr = NULL;
    const openexe_t *oeptr = NULL;
    DWORD       cbRet  = 0;

    DEBUGMSG(ZONE_ENTRY,(L"PROCGetModInfo entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                    pprc, pBaseAddr, infoType, pBuf, cbBuf));

    __try {
        switch (infoType) {
        case MINFO_CREATOR_TOKEN:
            if (sizeof (HANDLE) == cbBuf) {
                *(HANDLE *)pBuf = (pActvProc)->hTokCreator;
                dwErr = 0;
                cbRet = cbBuf;
            }
            break;

        case MINFO_MODULE_INFO:
        case MINFO_FILE_HANDLE:
            if (pBaseAddr) {
                LockModuleList ();
                pEntry = FindModInProc (pprc, (PMODULE) pBaseAddr);
                UnlockModuleList ();
                if (!pEntry) {
                    break;
                }
                pMod = pEntry->pMod;
            }

            if (MINFO_MODULE_INFO == infoType) {
                // GetModuleInformation
                LPMODULEINFO pmi = (LPMODULEINFO) pBuf;
                if (sizeof (MODULEINFO) != cbBuf) {
                    break;
                }
                pmi->lpBaseOfDll = pMod? pMod->BasePtr : pprc->BasePtr;
                pmi->EntryPoint  = pMod? (LPVOID) pMod->startip : (LPVOID) (DWORD) MTBFf;
                pmi->SizeOfImage = pMod? pMod->e32.e32_vsize : pprc->e32.e32_vsize;

            } else {
                // CeOpenFileHandle
                HANDLE hf = INVALID_HANDLE_VALUE;
                oeptr = pMod? &pMod->oe : &pprc->oe;
                if ((sizeof (HANDLE) != cbBuf)
                    || (FA_PREFIXUP & oeptr->filetype)) {
                    break;
                }
                    VERIFY (!HNDLDuplicate (g_pprcNK, oeptr->hf, pActvProc, &hf));
                    *(HANDLE *)pBuf = hf;

            }

            dwErr = 0;
            cbRet = cbBuf;

            break;

        case MINFO_FUNCTION_TABLE:
#ifdef x86
            // x86 doesn't support function table
            dwErr = ERROR_NOT_SUPPORTED;
            break;
#endif
            // fall through to other cases or for non-x86 case
        case MINFO_FILE_ATTRIBUTES:
        case MINFO_FULL_PATH:
            if (pBaseAddr != pprc->BasePtr) {

                LockModuleList ();
                pMod = FindModInProcByAddr (pprc, (DWORD) pBaseAddr);
                UnlockModuleList ();

                if (!pMod || (pMod->BasePtr != pBaseAddr)) {
                    break;
                }
                oeptr = &pMod->oe;
                eptr  = &pMod->e32;
            } else {
                oeptr = &pprc->oe;
                eptr  = &pprc->e32;
            }

            if (MINFO_FILE_ATTRIBUTES == infoType) {
                // get file attributes
                cbRet = pBuf? DoGetFileAttr (oeptr, pBuf, cbBuf) : sizeof (DWORD);
            } else if (MINFO_FULL_PATH == infoType) {
                // get full path
                cbRet = DoGetFullPath (oeptr, pBuf, cbBuf/sizeof(WCHAR));
                if (cbRet) {
                    cbRet = (cbRet + 1) * sizeof(WCHAR);    // take EOS into account
                }
            }
#ifndef x86
            else {
                // get function table
                cbRet = pBuf? DoGetFncTbl (pprc, eptr, pBaseAddr, pBuf, cbBuf) : eptr->e32_unit[EXC].size;
            }
#endif
            dwErr = 0;

            break;

        case MINFO_PROC_TERMINATED:
            dwErr = 0;
            cbRet = IsProcessTerminated (pprc);
            break;
        default:
            break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // empty, dwErr already set.
    }

    if (dwErr) {
        SetLastError (dwErr);
        cbRet = 0;
    }

    return cbRet;

}

//------------------------------------------------------------------------------
// PROCGetModName - implement GetModuleFileNameW
//------------------------------------------------------------------------------
DWORD PROCGetModName (PPROCESS pprc, PMODULE pMod, LPWSTR lpFilename, DWORD cchLen)
{
    DWORD cchRet = 0;

    if ((int) cchLen > 0) {
        openexe_t *oeptr = NULL;

        LockModuleList ();
        if (!pMod) {
            // process
            oeptr = &pprc->oe;
        } else if (FindModInProc (pprc, pMod)) {
            // module
            oeptr = &pMod->oe;
        }
        UnlockModuleList ();

        __try {
            if (oeptr) {
                cchRet = DoGetFullPath (oeptr, lpFilename, cchLen);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            cchRet = 0;
        }
    }

    if (!cchRet) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    }

    return cchRet;

}

// PROCPageOutModule - implement PageOutModule
BOOL PROCPageOutModule (PPROCESS pprc, PMODULE pMod, DWORD dwFlags)
{
    DWORD dwErr;

    TRUSTED_API (L"PROCPageOutModule", FALSE);

    dwErr = DoPageOutModule (pprc, pMod, dwFlags);
    if (dwErr)
        KSetLastError (pCurThread, dwErr);

    return (0 == dwErr);
}

//
// PROCGetID - return process id of a process
//
DWORD PROCGetID (PPROCESS pprc)
{
    return pprc->dwId;
}

//------------------------------------------------------------------------------
// PROCDelete - delete process object
// NOTE: a process object can be delete way before process handle got closed. The reason being that a process
//       might have exited, but other process still holding a handle to the "process", which can be used to
//       query exit code.
//------------------------------------------------------------------------------
BOOL PROCDelete (PPROCESS pprc)
{
    DEBUGCHK (!pprc->csVM.OwnerThread);
    DEBUGCHK (!pprc->hndlLock.lLock);
    DEBUGCHK (!pprc->csLoader.OwnerThread);
    DEBUGCHK (IsDListEmpty (&pprc->modList));
    DEBUGCHK (IsDListEmpty (&pprc->viewList));
    DEBUGCHK (!pprc->phndtbl);

    DEBUGMSG (ZONE_LOADER1, (L"Process id=%8.8lx is deleted\r\n", pprc->dwId));

    DEBUGMSG (ZONE_VIRTMEM, (L"Releasing VM of process %8.8lx\r\n", pprc->dwId));

    // free o32 and name (same as Module, as in FreeModuleMemory)
    // process name in-accessible from here one
    if (pprc->o32_ptr) {
        PNAME pAlloc = (PNAME) ((DWORD) pprc->o32_ptr - 4);
        pprc->lpszProcName = L"";
        FreeMem (pAlloc, pAlloc->wPool);
    }

    // also free oe.lpName as main thread exit leaves it alone
    FreeExeName(&pprc->oe);

    // release VM
    VMDelete (pprc);

    // free the process event
    DoUnlockHDATA (pprc->phdProcEvt, -HNDLCNT_INCR);

    // delete all the CS of the process
    DeleteFastLock (&pprc->hndlLock);
    DeleteCriticalSection (&pprc->csVM);
    DeleteCriticalSection (&pprc->csLoader);

    CELOG_ProcessDelete(pprc);
    FreeMem (pprc, HEAP_PROCESS);

    return TRUE;
}

static BOOL EnumSetAffinity (PDLIST pItem, LPVOID pEnumData)
{
    SCHL_SetThreadAffinity ((PTHREAD) pItem, (DWORD) pEnumData);
    return FALSE;
}

BOOL PROCSetAffinity (PPROCESS pprc, DWORD dwAffinity)
{
    DWORD   dwErr  = 0;
    DEBUGMSG(ZONE_ENTRY,(L"PROCSetAffinity entry: %8.8lx %8.8lx\r\n", pprc, dwAffinity));

    if (dwAffinity > g_pKData->nCpus) {
        dwErr = ERROR_INVALID_PARAMETER;
    } else if (g_pKData->nCpus > 1){
        if (!AffinityChangeAllowed (pprc)) {
            // cannot change affinity if EXE is built before 7.
            dwErr = ERROR_ACCESS_DENIED;
        } else if (dwAffinity && (CE_PROCESSOR_STATE_POWERED_ON != g_ppcbs[dwAffinity-1]->dwCpuState)) {
            dwErr = ERROR_NOT_READY;
        } else {
            LockLoader (pprc);
            pprc->dwAffinity = dwAffinity;
            EnumerateDList (&pprc->thrdList, EnumSetAffinity, (LPVOID) dwAffinity);
            UnlockLoader (pprc);
        }
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }
    DEBUGMSG(ZONE_ENTRY,(L"PROCSetAffinity exit: dwErr = %8.8lx\r\n", dwErr));
    return !dwErr;
}

DWORD DoGetAffinity (DWORD dwBaseAffinity);

BOOL PROCGetAffinity (PPROCESS pprc, LPDWORD pdwAffinity)
{
    *pdwAffinity = DoGetAffinity (pprc->dwAffinity);
    return TRUE;
}

typedef struct {
    ULONGLONG utime;
    ULONGLONG ktime;
} EnumTimeStruct, *PEnumTimeStruct;

BOOL EnumAccumulateTime (PDLIST pItem, LPVOID pEnumData)
{
    PTHREAD pth = (PTHREAD) pItem;
    PEnumTimeStruct pets = (PEnumTimeStruct) pEnumData;

    if (pCurThread == pth) {
        pets->utime += SCHL_GetCurThreadUTime ();
        pets->ktime += SCHL_GetCurThreadKTime ();
    } else {
        pets->utime += pth->dwUTime;
        pets->ktime += pth->dwKTime;
    }

    return FALSE;   // keep enumlating
}

//
// PROCGetTimes - get the total run time of a process
//
BOOL PROCGetTimes (PPROCESS pprc, LPFILETIME lpCreationTime, LPFILETIME lpExitTime, LPFILETIME lpKernelTime, LPFILETIME lpUserTime)
{
    EnumTimeStruct ets;

    LockLoader (pprc);

    // initialized with time used by threads already exited
    ets.ktime = pprc->dwKrnTime;
    ets.utime = pprc->dwUsrTime;

    // enumerate the thread list to accumulate kernel/user time
    EnumerateDList (&pprc->thrdList, EnumAccumulateTime, &ets);

    UnlockLoader (pprc);

    // convert from milliseconds to 100ns units
    ets.ktime *= 10000;
    ets.utime *= 10000;

    // update return value
    *lpCreationTime = pprc->ftCreate;
    *lpExitTime     = pprc->ftExit;
    *lpKernelTime   = *(LPFILETIME) &ets.ktime;
    *lpUserTime     = *(LPFILETIME) &ets.utime;

    return TRUE;
}

//
// these following apis are not 'process method', but process related
//

//------------------------------------------------------------------------------
// PROCGetCode - Get Process exit code
//------------------------------------------------------------------------------
BOOL PROCGetCode (HANDLE hProc, LPDWORD pdwExit)
{
    PHDATA  phd   = LockHandleData (hProc, pActvProc);
    DWORD   dwErr = ERROR_INVALID_HANDLE;
    DEBUGMSG(ZONE_ENTRY,(L"PROCGetCode entry: %8.8lx %8.8lx\r\n", hProc, pdwExit));
    if (phd) {
        __try {
            if ((&cinfProc == phd->pci) || (&cinfProcQuery == phd->pci)){
                PHDATA phdEvt = ((PPROCESS) phd->pvObj)->phdProcEvt;
                *pdwExit = phdEvt? phdEvt->dwData : STILL_ACTIVE;
                dwErr = 0;

            } else if ((&cinfEvent == phd->pci)
                && (((PEVENT) phd->pvObj)->manualreset & PROCESS_EVENT)) {
                // the pseudo handle returned by CreateProcess
                *pdwExit = phd->dwData;
                dwErr = 0;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = ERROR_INVALID_PARAMETER;
        }
        UnlockHandleData (phd);
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    DEBUGMSG(ZONE_ENTRY,(L"PROCGetCode exit: dwErr = %8.8lx\r\n", dwErr));
    return !dwErr;
}

//------------------------------------------------------------------------------
// Allocate and initialize a process strucutre
//------------------------------------------------------------------------------
static PPROCESS AllocateProcess (DWORD fdwCreate)
{
    PHDATA phdEvt = NKCreateAndLockEvent (TRUE, FALSE);
    PPROCESS pprc = NULL;

    if (phdEvt) {
        pprc = AllocMem (HEAP_PROCESS);
        if (!pprc) {
            UnlockHandleData (phdEvt);

        } else {
            PPROCESS pprcCreator = pActvProc;
            BOOL stdAllocSuccess = TRUE;

            // turn the lock into a handle count, such that it can be duplicated internally.
            VERIFY (DoLockHDATA (phdEvt, HNDLCNT_INCR-LOCKCNT_INCR));
            GetEventPtr(phdEvt)->manualreset |= PROCESS_EVENT;

            memset (pprc, 0, sizeof (PROCESS));

            pprc->lpszProcName      = L"";
            pprc->tlsLowUsed        = TLSSLOT_RESERVE;
            pprc->bPrio             = THREAD_RT_PRIORITY_NORMAL;
            pprc->phdProcEvt        = phdEvt;
            pprc->bState            = PROCESS_STATE_STARTING;

            InitDList (&pprc->modList);
            InitDList (&pprc->viewList);
            InitDList (&pprc->thrdList);

            InitializeFastLock (&pprc->hndlLock);
            InitializeCriticalSection (&pprc->csVM);
            InitializeCriticalSection (&pprc->csLoader);

            // Copy standard paths from current process
            if (!(fdwCreate & CREATE_NEW_CONSOLE)) {
                int loop;
                for (loop = 0; loop < 3; loop++) {
                    if (pprcCreator->pStdNames[loop]) {
                        LPCWSTR stdName = pprcCreator->pStdNames[loop]->name;
                        if (NULL == (pprc->pStdNames[loop] = DupStrToPNAME(stdName))) {
                            stdAllocSuccess = FALSE;
                            break;
                        }
                    }
                }
            }

            if (stdAllocSuccess                                                         // allocated pStdNames, if needed
                && (0 != (pprc->dwId = (DWORD) HNDLCreateHandle (&cinfProc, pprc, g_pprcNK, PROCESS_ALL_ACCESS)))     // create process id
                && VMInit (pprc)                                                        // initialize VM
                && (NULL != (pprc->phndtbl = CreateHandleTable (NULL)))) {              // create handle table

                pprc->dwId &= ~1;

                // create a query only handle to the process
                pprc->hProcQuery = HNDLCreateHandle (&cinfProcQuery, pprc, g_pprcNK, PROCESS_QUERY_INFORMATION);
                if (pprc->hProcQuery) {
                    // +1 lock cnt for the original process handle (will decrement on process exit)
                    VERIFY (LockHandleData ((HANDLE)pprc->dwId, g_pprcNK));
                }

                // success, set currnt state to alive
                phdEvt->dwData = STILL_ACTIVE;

            } else {
                // failed, cleanup
                HANDLE hProc = (HANDLE) pprc->dwId;
                pprc->dwId = 0; // no pre-close handling

                // free the pStdNames
                if (pprc->pStdNames[0])     FreeName (pprc->pStdNames[0]);
                if (pprc->pStdNames[1])     FreeName (pprc->pStdNames[1]);
                if (pprc->pStdNames[2])     FreeName (pprc->pStdNames[2]);

                if (hProc) {
                    // handle created, just close the handle and the handle closing code will take care of cleanup
                    HNDLCloseHandle (g_pprcNK, hProc);

                } else {
                    // free VM, if created
                    VMDelete (pprc);

                    // free the process event
                    DoUnlockHDATA (phdEvt, -HNDLCNT_INCR);

                    // handle not created, freeing the allocated objects
                    DeleteFastLock (&pprc->hndlLock);
                    DeleteCriticalSection (&pprc->csVM);
                    DeleteCriticalSection (&pprc->csLoader);

                    FreeMem (pprc, HEAP_PROCESS);
                }
                pprc = NULL;
            }
        }
    }

    return pprc;

}

#define VALID_CREATE_PROCESS_FLAGS (CREATE_NEW_CONSOLE|DEBUG_ONLY_THIS_PROCESS|DEBUG_PROCESS|CREATE_SUSPENDED|INHERIT_CALLER_PRIORITY|0x80000000)


#ifdef DEBUG
int DumpProcess (PPROCESS pprc)
{
    DEBUGMSG (1, (L"Dumping Process %8.8lx\r\n", pprc));
    NKLOG (pprc->BasePtr);
    NKLOG (pprc->bChainDebug);
    NKLOG (pprc->bPrio);
    NKLOG (pprc->dwId);
    NKLOG (pprc->lpszProcName);
    NKLOG (pprc->thrdList.pBack);
    NKLOG (pprc->thrdList.pFwd);
    NKLOG (pprc->ppdir);
    NKLOG (pprc->phndtbl);
    NKLOG (pprc->vaFree);
    NKOutputDebugString (L"----------------\r\n");
    return 0;
}

#endif

static void AppendExeSuffixIfNeeded (LPWSTR szDst, DWORD cchLen, DWORD cchMax)
{
    DEBUGCHK ((int) cchLen >= 0);
    DEBUGCHK ((int) cchMax > 4);
    if ((cchLen < 4) || ((cchLen < cchMax - 4) && NKwcsicmp (L".exe", &szDst[cchLen-4]))) {
        memcpy (szDst+cchLen, L".exe", 10);     // 10 = sizeof (L".exe"
    }
}

#define MAX_CCH_COMMAND_LINE    ((KRN_STACK_SIZE/2)/sizeof(WCHAR)) // half the default stack size, for it's store on stack

//------------------------------------------------------------------------------
// NKCreateProcess - Creaet a new Process
//------------------------------------------------------------------------------
BOOL NKCreateProcess (
    LPCWSTR lpszImageName,
    LPCWSTR lpszCommandLine,
    LPCE_PROCESS_INFORMATION pCeProcInfo
)
{
    ProcStartInfo psi;
    PPROCESS pNewproc   = NULL;
    PTHREAD  pNewth     = NULL;
    PTHREAD  pth        = pCurThread;
    DWORD    fdwCreate  = (pCeProcInfo) ? (pCeProcInfo->dwFlags) : 0;
    DWORD    prio       = (fdwCreate & INHERIT_CALLER_PRIORITY)? GET_BPRIO(pth) : THREAD_RT_PRIORITY_NORMAL;
    PPROCESS pprc       = pActvProc;
    PHDATA   phdNewProc = NULL, phdNewTh = NULL;
    WCHAR    szImgNameCopy[MAX_PATH];
    WCHAR    _szCmdLineCopy[MAX_PATH];
    WCHAR    *pszCmdLineCopy = _szCmdLineCopy;
    CE_PROCESS_INFORMATION LocalCeProcInfo;
    LPCE_PROCESS_INFORMATION pLocalCeProcInfo = NULL;

    DEBUGMSG(ZONE_LOADER1,(L"NKCreateProcess entry: %8.8lx %8.8lx %8.8lx\r\n", lpszImageName, lpszCommandLine, fdwCreate));

    psi.dwErr = 0;
    psi.phdCreator = 0;
    psi.hProcAccount = 0;

    // validate argumemts
    if (!lpszImageName || (fdwCreate & ~VALID_CREATE_PROCESS_FLAGS) || !MTBFf) {
        psi.dwErr = ERROR_INVALID_PARAMETER;

    } else if (pCeProcInfo) {

        if (pCeProcInfo->cbSize != sizeof(CE_PROCESS_INFORMATION)) {
            psi.dwErr = ERROR_INVALID_PARAMETER;

        } else if (pprc != g_pprcNK) {

            // make a copy of the pCeProcInfo structure if called from user
            memcpy (&LocalCeProcInfo, pCeProcInfo, sizeof(LocalCeProcInfo));
            pLocalCeProcInfo = &LocalCeProcInfo;

        } else {

            // kernel mode caller; use passed in pointers
            pLocalCeProcInfo = pCeProcInfo;
        }
    }

    if (!psi.dwErr) {

        // make copy of string arguments
        DWORD dwLen = CopyPath (szImgNameCopy, lpszImageName, MAX_PATH);   // can except here, no resource leak

        if (!dwLen) {
            psi.dwErr = ERROR_BAD_PATHNAME;

        } else {

            // append .exe if needed
            AppendExeSuffixIfNeeded (szImgNameCopy, dwLen, MAX_PATH);

            pszCmdLineCopy[0] = 0;  // empty string

            // make a copy of the command line
            if (lpszCommandLine) {

                dwLen = NKwcslen (lpszCommandLine);
                if (dwLen) {
                    if (dwLen >= MAX_CCH_COMMAND_LINE) {
                        psi.dwErr = ERROR_INVALID_PARAMETER;
                    } else {
                        if (dwLen >= MAX_PATH) {
                            // long command line, allocate it on heap
                            pszCmdLineCopy = (LPWSTR) NKmalloc ((dwLen+1)*sizeof(WCHAR));

                            // NO unhandled EXCEPTION allowed past this line in NKCreateProcess, or resource leak.
                        }
                        if (!pszCmdLineCopy) {
                            psi.dwErr = ERROR_NOT_ENOUGH_MEMORY;
                        } else if (!CeSafeCopyMemory (pszCmdLineCopy, lpszCommandLine, dwLen*sizeof(WCHAR))) {
                            psi.dwErr = ERROR_INVALID_PARAMETER;
                        } else {
                            pszCmdLineCopy[dwLen] = 0;      // terminate string
                        }
                    }
                }
            }
        }
    }

    if (!psi.dwErr) {

        psi.fdwCreate    = fdwCreate;
        psi.pszImageName = szImgNameCopy;
        psi.pszCmdLine   = pszCmdLineCopy;

        // allocate memory
        if ((PageFreeCount_Pool < g_cpMinPageProcCreate)                                      // memory too low?
            || (NULL == (pNewproc = AllocateProcess (fdwCreate)))                             // allocate process memory
            || (NULL == (phdNewTh = THRDCreate (pNewproc, (FARPROC) CreateNewProc, &psi,
                                                KRN_STACK_SIZE, TH_KMODE, prio | THRD_NO_BASE_FUNCITON)))) {
            psi.dwErr = (psi.dwErr) ? psi.dwErr : ERROR_OUTOFMEMORY;

        } else {

            DEBUGMSG (ZONE_LOADER2, (L"", DumpProcess (pNewproc)));

            // lock the process/thread object such that the object won't get destroyed
            // if the new process exited before we get a chance to run
            phdNewProc = LockHandleData ((HANDLE) pNewproc->dwId, g_pprcNK);
            pNewth     = GetThreadPtr (phdNewTh);
            DEBUGCHK (pNewth);

            // promote the 'lock' into 'handle'. The reason is that if the process exited before we get to
            // OpenProcess/OpenThread, the pre-close function will be called without doing this. And the handle returned
            // will become a bad handle
            DoLockHDATA (phdNewProc, HNDLCNT_INCR);
            DoLockHDATA (phdNewTh,   HNDLCNT_INCR);

            // save main thread id in the process event.
            GetEventPtr (pNewproc->phdProcEvt)->dwData = pNewth->dwId;

            THRDResume (pNewth, NULL);

            // wait for the thread to finish loading
            DoWaitForObjects (1, &pNewproc->phdProcEvt, INFINITE);

            // reset the process event
            SCHL_ResetProcEvent (GetEventPtr (pNewproc->phdProcEvt));

            // initialize app debugger support if this process is being created by a debugger
            // note: main thread is still suspended (no need to hold locks in DbgrInit... api)
            if (!ProcessNotDebuggable (pNewproc)) {
                DbgrInitProcess(pNewproc, fdwCreate);
            }

            // resume the thread if it's not created suspended
            if (!(fdwCreate & CREATE_SUSPENDED)) {
                THRDResume (pNewth, NULL);
            }
        }
    }

    // free the commandline copy if allocated on heap
    if ((_szCmdLineCopy != pszCmdLineCopy) && pszCmdLineCopy) {
        NKfree (pszCmdLineCopy);
    }

    if (psi.dwErr) {
        // cleanup the original handle only if pNewth is NULL, for CreateNewProc had cleaned up all the objects
        if (!phdNewTh) {
            if (pNewproc) {
                HANDLE hNewProc = (HANDLE) pNewproc->dwId;
                pNewproc->dwId = 0;     // no pre-close handling.

                DEBUGCHK (hNewProc);

                if (pNewproc->phndtbl) {
                    HNDLDeleteHandleTable (pNewproc);
                }
                HNDLCloseHandle (g_pprcNK, hNewProc);
            }
        }

        if (psi.hProcAccount) {
            HNDLCloseHandle (g_pprcNK, psi.hProcAccount);
        }

        KSetLastError (pth, psi.dwErr);

    } else {

        if (pLocalCeProcInfo) {
            pLocalCeProcInfo->ProcInfo.dwProcessId = pNewproc->dwId;
            pLocalCeProcInfo->ProcInfo.dwThreadId  = pNewth->dwId;

            // duplicate the "process event" handle as hProcess and thread, the handle can
            // only be used for
            // (1) waited on with WaitForSingleObject/WaitForMulitpleObjects, or
            // (2) used to call GetExitCodeProcesss/GetExitCodeThread
            //
            // NOTE: we use the "process event" for both main thread and the process to save
            //       an event, and avoid synchronization issues. The only consequence is that
            //       you can use pi.hThread to get "process exit code", and use pi.hProcess
            //       to get "thread exit code". I think they are harmless and we can revisit
            //       if we found this to be a problem.
            //


            // BC concern: we'll try OpenProcess/OpenThread on the id's such that we'll return a valid handle
            // if the caller has the privilege to Open the process/thread. Otherwise, we'll return the pseudo handle.
            pLocalCeProcInfo->ProcInfo.hProcess = NKOpenProcess (0, FALSE, pLocalCeProcInfo->ProcInfo.dwProcessId);
            if (!pLocalCeProcInfo->ProcInfo.hProcess) {
                pLocalCeProcInfo->ProcInfo.hProcess = HNDLDupWithHDATA (pprc, pNewproc->phdProcEvt, &psi.dwErr);
                DEBUGCHK (pLocalCeProcInfo->ProcInfo.hProcess);
            }

            pLocalCeProcInfo->ProcInfo.hThread = NKOpenThread (0, FALSE, pLocalCeProcInfo->ProcInfo.dwThreadId);
            if (!pLocalCeProcInfo->ProcInfo.hThread) {
                if (ERROR_ACCESS_DENIED == GetLastError()) {
                    RETAILMSG (1, (L"CreateProcess: Failed to get handle to application: %s. Returning wait-only handle to the application.\r\n", pNewproc->lpszProcName));
                }
                pLocalCeProcInfo->ProcInfo.hThread  = HNDLDupWithHDATA (pprc, pNewproc->phdProcEvt, &psi.dwErr);
                DEBUGCHK (pLocalCeProcInfo->ProcInfo.hThread);
            }

            if (pLocalCeProcInfo != pCeProcInfo) {
                // copy the [out] args for user mode caller
                CeSafeCopyMemory (&pCeProcInfo->ProcInfo, &pLocalCeProcInfo->ProcInfo, sizeof(PROCESS_INFORMATION));
            }
        }
    }

    DoUnlockHDATA (phdNewProc, -HNDLCNT_INCR);  // can trigger pre-close if the new process exited
    DoUnlockHDATA (phdNewTh,   -HNDLCNT_INCR);  // can trigger pre-close if the new thread exited
    UnlockHandleData (phdNewProc);
    UnlockHandleData (phdNewTh);
    DoUnlockHDATA (psi.phdCreator, -HNDLCNT_INCR);

    DEBUGMSG(ZONE_LOADER1 || psi.dwErr, (L"NKCreateProcess exit: dwErr = %8.8lx\r\n", psi.dwErr));
    return !psi.dwErr;
}

//------------------------------------------------------------------------------
// NKOpenProcess - given process id, get the handle to the process.
//------------------------------------------------------------------------------
HANDLE NKOpenProcess (DWORD dwAccessRequested, BOOL fInherit, DWORD dwProcessId)
{
    DWORD dwErr = 0;
    HANDLE hRet = NULL;
    PPROCESS pprcCurrent = pActvProc;
    PHDATA phdTarget = LockHandleData ((HANDLE) dwProcessId, g_pprcNK); // process we want to open
    PPROCESS pprcTarget = GetProcPtr (phdTarget);

    if (fInherit
        || !phdTarget
        || !pprcTarget
        || (&cinfProc != phdTarget->pci)
        || !IsProcessHandlesAllowed (pprcTarget)
        ) {
        dwErr = ERROR_INVALID_PARAMETER;

    } else {
        //check if request from a caller is 'query process' else set request to 'all access'
        if ( PROCESS_QUERY_INFORMATION != dwAccessRequested ){
            dwAccessRequested = PROCESS_ALL_ACCESS;
        }

        if (PROCESS_ALL_ACCESS == dwAccessRequested) {
            // give full access to caller
            dwErr = HNDLDuplicate (g_pprcNK, (HANDLE) dwProcessId, pprcCurrent, &hRet);

        } else if (PROCESS_QUERY_INFORMATION & dwAccessRequested) {
            // give query only access to caller
            dwErr = HNDLDuplicate (g_pprcNK, pprcTarget->hProcQuery, pprcCurrent, &hRet);

        } else {
            // invalid access mask
            dwErr = ERROR_INVALID_PARAMETER;
            DEBUGCHK (0);
        }
    }

    UnlockHandleData (phdTarget);

    if (dwErr) {
        DEBUGCHK (!hRet);
        SetLastError(dwErr);
    }

    return hRet;
}

//------------------------------------------------------------------------------
// PROCInit - initialize process handling (called at system startup)
//------------------------------------------------------------------------------
void PROCInit (void)
{
    HANDLE hPrcNK;

    PcbSetActvProc (g_pprcNK);

    // create the handle table for kernel
    g_phndtblNK = g_pprcNK->phndtbl = CreateHandleTable (NULL);
    DEBUGCHK (g_pprcNK->phndtbl);

    hPrcNK = HNDLCreateHandle (&cinfProc, g_pprcNK, g_pprcNK, PROCESS_ALL_ACCESS);
    DEBUGCHK (hPrcNK);

    HNDLSetUserInfo (hPrcNK, STILL_ACTIVE, g_pprcNK);
    PcbSetActvProcId ((DWORD) hPrcNK & ~1);

    g_pprcNK->dwId = dwActvProcId;
    g_pprcNK->lpszProcName = L"NK.EXE";
    g_pprcNK->tlsLowUsed = TLSSLOT_RESERVE;
    g_pprcNK->bPrio = THREAD_RT_PRIORITY_NORMAL;
#ifdef DEBUG
    g_pprcNK->ZonePtr = g_pOemGlobal->pdpCurSettings;
#endif
    g_pprcNK->e32.e32_stackmax = KRN_STACK_SIZE;
    g_pprcNK->fFlags  = (BYTE) MF_DEP_COMPACT;

    // initialized all the process lists
    InitDList (&g_pprcNK->thrdList);
    InitDList (&g_pprcNK->modList);
    InitDList (&g_pprcNK->viewList);
    InitDList (&g_pprcNK->prclist);

    // don't allow thread create one memory drop below 2% available
    if (g_cpMinPageProcCreate < PageFreeCount / 50) {
        g_cpMinPageProcCreate = PageFreeCount / 50;
    }


}


