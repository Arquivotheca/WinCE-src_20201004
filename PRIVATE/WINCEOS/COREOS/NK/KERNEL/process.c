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

#ifdef ARM

DWORD AllocateAlias (PALIAS *ppAlias, LPVOID pAddr, LPDWORD pfProtect)
{
    DWORD dwErr = 0;
    if (IsVirtualTaggedCache ()
        && !(*pfProtect & PAGE_PHYSICAL)
        && !IsKernelVa (pAddr)) {
        PALIAS pAlias;

        if (!(pAlias = AllocMem (HEAP_ALIAS))) {
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
    } else if (!VMCopy (pprcDst, dwDestAddr, pprcSrc, dwSrcAddr, cbSize, fProtect)) {
        dwErr = ERROR_INVALID_PARAMETER;
    }
    UnlockHandleData (phdSrc);

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    return !dwErr;
}

static LPVOID PROCVMAllocCopy (
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
        if (!(pprcDst = GetProcPtr (phdDstProc))) {
            // error
            dwErr = ERROR_INVALID_HANDLE;
        }
    }

    if (!dwErr
#ifdef ARM
        && !(dwErr = AllocateAlias (&pAlias, pAddr, &fProtect))
#endif
        ) {

        DWORD dwOfst = (DWORD) pAddr & VM_BLOCK_OFST_MASK;

        if (!(lpRet = VMAllocCopy (pprc, pprcDst, pAddr, cbSize, fProtect, pAlias))
            || !VMAddAllocToList (pprcDst, (LPBYTE) lpRet - dwOfst, cbSize + dwOfst, pAlias)) {
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

LPVOID PROCVMAlloc (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, DWORD fAllocType, DWORD fProtect)
{
    BOOL fReserve = (!pAddr || (fAllocType & MEM_RESERVE));
    PVALIST pUncached = NULL;
    LPVOID pRet = NULL;

    if (!fReserve
        && (pActvProc != g_pprcNK)
        && !VMFindAlloc (pprc, pAddr, cbSize, FALSE)) {
        DEBUGMSG (1, (L"PROCVMAlloc: committing region not reserved, pAddr = %8.8lx, cbSize = %8.8lx\r\n", pAddr, cbSize));
        SetLastError (ERROR_INVALID_PARAMETER);
        
#ifndef VM_DEBUG
    // check for obvious size error

    // for VM_DEBUG we're slotize process VM, it's possible the pprc->vaFree is high, but there can still
    // be VM available in low address range. So the check is only be done if VM_DEBUG is not defined.
    } else if (fReserve
            && ((cbSize + pprc->vaFree) > VM_DLL_BASE)
            && (pActvProc != g_pprcNK)) {
        DEBUGMSG (1, (L"PROCVMAlloc: request size too big cbSize = %8.8lx\r\n",  cbSize));
        SetLastError (ERROR_INVALID_PARAMETER);
#endif       
    // disallow internal allocation flags
    } else if (fAllocType & VM_INTERNAL_ALLOC_TYPE) {
        DEBUGMSG (1, (L"PROCVMAlloc: invalid allocation type = %8.8lx\r\n", fAllocType));
        SetLastError (ERROR_INVALID_PARAMETER);

#ifdef ARM
    } else if ((fProtect & PAGE_NOCACHE)
                && IsVirtualTaggedCache ()
                && !(pUncached = AllocMem (HEAP_VALIST))) {
        
        DEBUGMSG (1, (L"PROCVMAlloc: can't allocate uncached list (OOM)\r\n"));
        SetLastError (ERROR_NOT_ENOUGH_MEMORY);
#endif
    } else {

        pRet = VMAlloc (pprc, pAddr, cbSize, fAllocType, fProtect);
    
        if (pRet                                                // allocation successful
            && fReserve                                         // new VM reserved
            && !VMAddAllocToList (pprc, pRet, cbSize, NULL)) {  // add to VM-allocation list

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

static BOOL PROCVMProtect (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, DWORD fNewProtect, LPDWORD pfOldProtect)
{
    BOOL fRet = FALSE;

    if (((int) cbSize <= 0)
        || !VMFindAlloc (pprc, pAddr, cbSize, FALSE)) {
        SetLastError (ERROR_INVALID_PARAMETER);
    } else {
        fRet = VMProtect (pprc, pAddr, cbSize, fNewProtect, pfOldProtect);
        
    }
    
    return fRet;
}

BOOL PROCVMFree (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, DWORD dwFreeType)
{
    BOOL fRelease = (MEM_RELEASE == dwFreeType) && !cbSize;
    BOOL fRet = VMFindAlloc (pprc, pAddr, cbSize, fRelease);

    if (fRet && !fRelease) {

        fRet = (MEM_DECOMMIT == dwFreeType)
            && ((int) cbSize > 0)
            && VMDecommit (pprc, pAddr, cbSize, VM_PAGER_NONE);
    }

    if (!fRet) {
        SetLastError (ERROR_INVALID_PARAMETER);
        // failed VirtualFree usually means something really bad...
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
    (PFNVOID)NotSupported,          // 7 - unused (previously DebugActiveProcess)
    (PFNVOID)PROCGetModInfo,        // 8 - CeGetModuleInfo
    (PFNVOID)PROCSetVer,            // 9 - SetProcessVersion
    (PFNVOID)PROCVMAlloc,           // 10 - VirtualAllocEx
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
    (PFNVOID)NotSupported,          // 22 - unused
    (PFNVOID)PROCGetModName,        // 23 - GetModuleFileNameW
    (PFNVOID)LDRGetModByName,       // 24 - GetModuleHandleW
    (PFNVOID)VMAllocPhys,           // 25 - AllocPhysMemEx (FreePhysMem is just VirtualFreeEx)
    (PFNVOID)PROCReadPEHeader,      // 26 - ReadPEHeader
    (PFNVOID)PROCChekDbgr,          // 27 - CheckRemoteDebuggerPresent
    (PFNVOID)NKOpenMsgQueueEx,      // 28 - OpenMsgQueueEx
    (PFNVOID)PROCGetID,             // 29 - GetProcessId
    (PFNVOID)NotSupported,          // 30 - VirtualAllocCopyEx
    (PFNVOID)NotSupported,          // 31 - PageOutModule
};

static const PFNVOID ProcIntMthds[] = {
    (PFNVOID)PROCDelete,            // CloseHandle
    (PFNVOID)PROCPreClose,
    (PFNVOID)PROCTerminate,         // 2 - TerminateProcess
    (PFNVOID)VMSetAttributes,       // 3 - VirtualSetAttributes
    (PFNVOID)PROCFlushICache,       // 4 - FlushInstructionCache
    (PFNVOID)PROCReadMemory,        // 5 - ReadProcessMemory
    (PFNVOID)PROCWriteMemory,       // 6 - WriteProcessMemory
    (PFNVOID)NotSupported,          // 7 - unused (previously DebugActiveProcess)
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
    (PFNVOID)NotSupported,          // 22 - unused
    (PFNVOID)PROCGetModName,        // 23 - GetModuleFileNameW
    (PFNVOID)LDRGetModByName,       // 24 - GetModuleHandleW
    (PFNVOID)VMAllocPhys,           // 25 - AllocPhysMemEx (FreePhysMem is just VirtualFreeEx)
    (PFNVOID)PROCReadPEHeader,      // 26 - ReadPEHeader
    (PFNVOID)PROCChekDbgr,          // 27 - CheckRemoteDebuggerPresent
    (PFNVOID)NKOpenMsgQueueEx,      // 28 - OpenMsgQueueEx
    (PFNVOID)PROCGetID,             // 29 - GetProcessId
    (PFNVOID)PROCVMAllocCopy,       // 30 - VirtualAllocCopyEx
    (PFNVOID)PROCPageOutModule,     // 31 - PageOutModule
};


static const ULONGLONG procSigs[] = {
    FNSIG1 (DW),                                // CloseHandle
    0,
    FNSIG2 (DW, DW),                            // 2 - TerminateProcess
    FNSIG6 (DW, O_PTR, DW, DW, DW, IO_PDW),     // 3 - VirtualSetAttributes
    FNSIG3 (DW, I_PTR, DW),                     // 4 - FlushInstructionCache
    FNSIG4 (DW, DW, O_PTR, DW),                 // 5 - ReadProcessMemory - NOTE: the "address" argument of Read/WriteProcessMemory
    FNSIG4 (DW, DW, I_PTR, DW),                 // 6 - WriteProcessMemory        is validated in the API itself
    FNSIG0 (),                                  // 7 - unused
    FNSIG5 (DW, DW, DW, O_PTR, DW),             // 8 - CeGetModuleInfo
    FNSIG2 (DW, DW),                            // 9 - SetProcessVersion
    FNSIG5 (DW, O_PTR, DW, DW, DW),             // 10 - VirtualAllocEx
    FNSIG4 (DW, O_PTR, DW, DW),                 // 11 - VirtualFreeEx 
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
    FNSIG0 (),                                  // 22 - unused
    FNSIG4 (DW, DW, O_PTR, DW),                 // 23 - GetModuleFileNameW
    FNSIG3 (DW, I_WSTR, DW),                    // 24 - GetModuleHandleW
    FNSIG6 (DW, DW, DW, DW, DW, IO_PDW),        // 25 - AllocPhysMemEx (FreePhysMem is just VirtualFreeEx)
    FNSIG5 (DW, DW, DW, O_PTR, DW),             // 26 - ReadPEHeader
    FNSIG2 (DW, IO_PDW),                        // 27 - CheckRemoteDebuggerPresent
    FNSIG4 (DW, DW, DW, IO_PDW),                // 28 - OpenMsgQueueEx
    FNSIG1 (DW),                                // 29 - GetProcessId
    FNSIG5 (DW, DW, I_PTR, DW, DW),             // 30 - VirtualAllocCopyEx
    FNSIG3 (DW, DW, DW),                        // 31 - PageOutModule
};

ERRFALSE (sizeof(ProcExtMthds) == sizeof(ProcIntMthds));
ERRFALSE ((sizeof(ProcExtMthds) / sizeof(ProcExtMthds[0])) == (sizeof(procSigs) / sizeof(procSigs[0])));


const CINFO cinfProc = {
    "PROC",
    DISPATCH_KERNEL_PSL,
    SH_CURPROC,
    sizeof(ProcExtMthds)/sizeof(ProcExtMthds[0]),
    ProcExtMthds,
    ProcIntMthds,
    procSigs,
    0,
    0,
    0,
};

static long g_cMinPageProcCreate = 40;

//
// SwitchVM - switch VM to another process, return the old process
//
PPROCESS SwitchVM (PPROCESS pprc)
{
    PTHREAD  pth     = pCurThread;
    PPROCESS pprcRet = pth->pprcVM;
    
    pth->pprcVM = pprc;
    
    if (pprc && (pprc != pVMProc)) {
        pVMProc = pprc;
        MDSwitchVM (pprc);
    }
    return pprcRet;
}

void SetCPUASID (PTHREAD pth)
{
    PPROCESS pprcNewVM = pth->pprcVM;

    DEBUGCHK (InSysCall ());
    
    if (!pprcNewVM) {
        // kernel thread with no process affinity. Only switch VM if the current VM is in trancient state
        pprcNewVM = IsProcessNormal (pVMProc)? pVMProc : g_pprcNK;
    }
    
    pVMProc      = pprcNewVM;
    pActvProc    = pth->pprcActv;
    dwActvProcId = pActvProc->dwId;

#ifdef x86
    if (pth->lpFPEmul) {
        // if using floating point emulation, switch to the right handler        
        InitNPXHPHandler (pth->lpFPEmul);
    }
#endif

    MDSetCPUASID ();

    if(pth != pCurThread) {
        // restore debug register using pSwapStack (see KCNextThread)
        if (g_pOemGlobal->fSaveCoProcReg && pth->pCoProcSaveArea) {
            DEBUGCHK (g_pOemGlobal->pfnRestoreCoProcRegs);
            g_pOemGlobal->pfnRestoreCoProcRegs (pth->pCoProcSaveArea);
        }
    }
}

//
// SwitchActiveProcess - switch to another process, return the the old process
//
PPROCESS SwitchActiveProcess (PPROCESS pprc)
{
    PPROCESS pprcOld = pActvProc;
    if (pprc != pprcOld) {
        pActvProc = pCurThread->pprcActv = pprc;
        dwActvProcId = pprc->dwId;
        MDSwitchActive (pprc);
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

            // only notify PSL of process exiting if it's not main thread calling ExitProcess.
            if (pth != pMainTh) {
                KillOneThread (pMainTh, dwExitCode);
                NKPSLNotify (DLL_PROCESS_EXITING, pprc->dwId, 0);
            } else {
                // main thread calling ExitProcess, update exit code
                pth->dwExitCode = dwExitCode;
            }

            if (!GET_DEAD (pth)) {
                // set the currrent thread to dying, such that the thread will be exiting upon
                // returning from PSL.
                SET_DYING (pth);
            }

        } else {
            // terminate process, need to grab the loader lock to be safe to look at main thread
            LockLoader (pprc);

            if (IsProcessAlive (pprc)) {
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
                    DEBUGMSG (1, (L"PROCTerminate: Unable to Terminate Process, id = %8.8lx\r\n", pprc->dwId));
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





BOOL PROCFlushICache (PPROCESS pprc, LPCVOID lpBaseAddress, DWORD dwSize)
{
    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_INSTRUCTIONS|CACHE_SYNC_WRITEBACK);
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
    DWORD dwAttr;
    DWORD dwErr = 0;
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
        while (*pBuf ++ = *pROMFileName ++)
            ;
    }
    
    return cchRet;
}


//------------------------------------------------------------------------------
// PROCGetModInfo - get process/module information 
//------------------------------------------------------------------------------
BOOL PROCGetModInfo (PPROCESS pprc, LPVOID pBaseAddr, DWORD infoType, LPVOID pBuf, DWORD cbBuf)
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
                pmi->EntryPoint  = pMod? (LPVOID) pMod->startip : (LPVOID) MTBFf;
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
                if (cbRet = DoGetFullPath (oeptr, pBuf, cbBuf/sizeof(WCHAR))) {
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
    DEBUGCHK (!pprc->csHndl.OwnerThread);
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

    // release VM
    VMDelete (pprc);

    // free the process event
    UnlockHandleData (pprc->phdProcEvt);
    // delete all the CS of the process
    DeleteCriticalSection (&pprc->csHndl);
    DeleteCriticalSection (&pprc->csVM);
    DeleteCriticalSection (&pprc->csLoader);

    CELOG_ProcessDelete(pprc);
    FreeMem (pprc, HEAP_PROCESS);

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
    PHDATA  phd;
    DWORD   dwErr = ERROR_INVALID_HANDLE;
    DEBUGMSG(ZONE_ENTRY,(L"PROCGetCode entry: %8.8lx %8.8lx\r\n", hProc, pdwExit));
    if (phd = LockHandleData (hProc, pActvProc)) {
        __try {
            if (&cinfProc == phd->pci) {
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
#ifdef VM_DEBUG
    static LONG nProcs;
#endif
    HANDLE hEvt = NKCreateEvent (NULL, TRUE, FALSE, NULL);
    PHDATA phdEvt = NULL;
    PPROCESS pprc = NULL;
    BOOL stdAllocSuccess = TRUE;

    if (hEvt) {

        phdEvt = LockHandleData (hEvt, pActvProc);
        // once we locked the handle, we can close the event handle safely
        HNDLCloseHandle (pActvProc, hEvt);
        GetEventPtr(phdEvt)->manualreset |= PROCESS_EVENT;
    } 

    if (phdEvt && (pprc = AllocMem (HEAP_PROCESS))) {
        
        memset (pprc, 0, sizeof (PROCESS));

#ifdef VM_DEBUG        
        pprc->vaFree = (InterlockedIncrement (&nProcs) & 0x1f) << 25; // 32M per process
        DEBUGCHK (pprc->vaFree < VM_DLL_BASE);
#endif
        pprc->vaFree           += VM_USER_BASE;
        pprc->hTok              = TOKEN_SYSTEM;
        pprc->lpszProcName      = L"";
        pprc->tlsLowUsed        = TLSSLOT_RESERVE;
        pprc->bPrio             = THREAD_RT_PRIORITY_NORMAL;
        pprc->phdProcEvt        = phdEvt;
        pprc->bState            = PROCESS_STATE_STARTING;
        
        InitDList (&pprc->modList);
        InitDList (&pprc->viewList);
        InitDList (&pprc->thrdList);

        InitializeCriticalSection (&pprc->csVM);
        InitializeCriticalSection (&pprc->csHndl);
        InitializeCriticalSection (&pprc->csLoader);

        // Copy standard paths from current process
        if (!(fdwCreate & CREATE_NEW_CONSOLE)) {
        	int loop;
        	for (loop = 0; loop < 3; loop++) {
                if (pActvProc->pStdNames[loop]) {
                    LPCWSTR stdName = pActvProc->pStdNames[loop]->name;
                    if (NULL == (pprc->pStdNames[loop] = DupStrToPNAME(stdName))) {
                        stdAllocSuccess = FALSE;
                        break;
                    }
                }
            }
        }

        if (stdAllocSuccess &&                                                      // allocated pStdNames, if needed
            (pprc->dwId = (DWORD) HNDLCreateHandle (&cinfProc, pprc, g_pprcNK))     // create process id
            && VMInit (pprc)                                                        // initialize VM
            && (pprc->phndtbl = CreateHandleTable (NUM_HNDL_1ST))) {                // create handle table

            pprc->dwId &= ~1;
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
                
                // handle not created, freeing the allocated objects
                DeleteCriticalSection (&pprc->csVM);
                DeleteCriticalSection (&pprc->csHndl);
                DeleteCriticalSection (&pprc->csLoader);

                FreeMem (pprc, HEAP_PROCESS);
            }
            pprc = NULL;
        }
    }

    // unlock the process event if failed.
    if (!pprc) {
        UnlockHandleData (phdEvt);
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
    OutputDebugStringW (L"----------------\r\n");
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
    LPSECURITY_ATTRIBUTES lpsaProcess,
    LPSECURITY_ATTRIBUTES lpsaThread,
    BOOL fInheritHandles,
    DWORD fdwCreate,
    LPVOID lpvEnvironment,
    LPWSTR lpszCurDir,
    LPSTARTUPINFO lpsiStartInfo,
    LPPROCESS_INFORMATION lppiProcInfo
)
{
    ProcStartInfo psi;
    LPBYTE   lpStack  = NULL;
    PPROCESS pNewproc = NULL;
    PTHREAD  pNewth   = NULL;
    DWORD    prio     = (fdwCreate & INHERIT_CALLER_PRIORITY)? GET_BPRIO(pCurThread) : THREAD_RT_PRIORITY_NORMAL;
    PPROCESS pprc     = pActvProc;
    PHDATA   phdNewProc = NULL, phdNewTh = NULL;
    WCHAR    szImgNameCopy[MAX_PATH];
    WCHAR    _szCmdLineCopy[MAX_PATH];
    WCHAR    *pszCmdLineCopy = _szCmdLineCopy;
    BOOL     fSuspended = FALSE;
    PROCESS_INFORMATION pi;
    
    DEBUGMSG(ZONE_LOADER1,(L"NKCreateProcess entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        lpszImageName, lpszCommandLine, lpsaProcess, lpsaThread, fInheritHandles, fdwCreate, lpvEnvironment,
        lpszCurDir, lpsiStartInfo, lppiProcInfo));

    psi.dwErr = 0;

    // validate argumemts
    if (   fInheritHandles                              // inherit handle not supported
        || !lpszImageName                               // NULL image name
        || (fdwCreate & ~VALID_CREATE_PROCESS_FLAGS)    // valid create flag?
        || lpvEnvironment                               // environment not supported
        || lpszCurDir                                   // we have no concept of "current directory"
        || !MTBFf) {    // coredll not exist??
        psi.dwErr = ERROR_INVALID_PARAMETER;
        
    } else {

        // make copy of string arguments
        DWORD dwLen = CopyPath (szImgNameCopy, lpszImageName, MAX_PATH);   // can except here, no resource leak

        if (!dwLen) {
            psi.dwErr = ERROR_BAD_PATHNAME;
            
        } else {

            // append .exe if needed
            AppendExeSuffixIfNeeded (szImgNameCopy, dwLen, MAX_PATH);

            pszCmdLineCopy[0] = 0;  // empty string
            
            // make a copy of the command line
            if (lpszCommandLine && (dwLen = NKwcslen (lpszCommandLine))) {

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

    if (!psi.dwErr) {
        // if this process is a debuggee, create it suspended
        if (   !(fdwCreate & CREATE_SUSPENDED)    // user did not request the process to be created as suspended
            && ((fdwCreate & (DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS)) // active process is debugging the new process
               ||(pprc->pDbgrThrd && pprc->bChainDebug))) { // active process is a debuggee with chain debug option set
            fdwCreate |= CREATE_SUSPENDED;
            fSuspended = TRUE;        
        }
        
        psi.fdwCreate    = fdwCreate;
        psi.pszImageName = szImgNameCopy;
        psi.pszCmdLine   = pszCmdLineCopy;
        psi.pthCreator   = pCurThread;

        // allocate memory        
        if ((PageFreeCount < g_cMinPageProcCreate)                   // memory too low?
            || !(pNewproc = AllocateProcess (fdwCreate))                    // allocate process memory
            || !(lpStack = VMCreateStack (g_pprcNK, KRN_STACK_SIZE))        // create stack for main thread
            || !(pNewth = THRDCreate (pNewproc, lpStack, KRN_STACK_SIZE, NULL, (LPVOID) CreateNewProc, &psi, TH_KMODE, prio | THRD_NO_BASE_FUNCITON)) // create main thread
            ) {
            psi.dwErr = ERROR_OUTOFMEMORY;
            
        } else {
        
            DEBUGMSG (ZONE_LOADER2, (L"", DumpProcess (pNewproc)));

            // lock the process/thread object such that the object won't get destroyed 
            // if the new process exited before we get a chance to run
            phdNewProc = LockHandleData ((HANDLE) pNewproc->dwId, g_pprcNK);
            phdNewTh   = LockHandleData ((HANDLE) pNewth->dwId,   g_pprcNK);

            // promote the 'lock' into 'handle'. The reason is that if the process exited before we get to
            // OpenProcess/OpenThread, the pre-close function will be called without doing this. And the handle returned
            // will become a bad handle
            DoLockHDATA (phdNewProc, HNDLCNT_INCR);
            DoLockHDATA (phdNewTh,   HNDLCNT_INCR);

            // fill PROCESS_INFORMAITON structure
            pi.dwProcessId = pNewproc->dwId;
            pi.dwThreadId  = pNewth->dwId;

            // save main thread id in the process event.
            GetEventPtr (pNewproc->phdProcEvt)->dwData = pNewth->dwId;

            THRDResume (pNewth, NULL);

            // wait for the thread to finish loading
            DoWaitForObjects (1, &pNewproc->phdProcEvt, INFINITE);

            // initialize app debugger support if this process is being created by a debugger
            // note: main thread is still suspended (no need to hold locks in DbgrInit... api)
            if (!pNewproc->fNoDebug)
                DbgrInitProcess(pNewproc, fdwCreate);

            // debug support is added; now resume the thread if we suspended
            if (fSuspended)
                THRDResume (pNewth, NULL);        

            // if there is an error, the CreateNewProc thread would have cleanup all process/thread related allocations.
        }
    }

    // free the commandline copy if allocated on heap
    if ((_szCmdLineCopy != pszCmdLineCopy) && pszCmdLineCopy) {
        NKfree (pszCmdLineCopy);
    }

    if (psi.dwErr) {
        // cleanup the original handle only if pNewth is NULL, for CreateNewProc had cleaned up all the objects
        if (!pNewth) {
            if (pNewproc) {
                HANDLE hNewProc = (HANDLE) pNewproc->dwId;
                pNewproc->dwId = 0;     // no pre-close handling.
                
                UnlockHandleData (pNewproc->phdProcEvt);
                if (pNewproc->ppdir) {
                    VMDelete (pNewproc);
                }
                if (pNewproc->phndtbl) {
                    HNDLDeleteHandleTable (pNewproc);
                }
                HNDLCloseHandle (g_pprcNK, hNewProc);
            }
            if (lpStack) {
                VMFreeAndRelease (g_pprcNK, lpStack, KRN_STACK_SIZE);
            }
        }
        
        KSetLastError (pCurThread, psi.dwErr);

    } else {

        // reset the process event
        KCall ((PKFN)ResetProcEvent, GetEventPtr (pNewproc->phdProcEvt));
    
        if (lppiProcInfo) {
    
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
            if (!(pi.hProcess = NKOpenProcess (PROCESS_ALL_ACCESS, FALSE, pi.dwProcessId))) {
                VERIFY (pi.hProcess = HNDLDupWithHDATA (pprc, pNewproc->phdProcEvt));
            } 

            if (!(pi.hThread = NKOpenThread (THREAD_ALL_ACCESS, FALSE, pi.dwThreadId))) {
                VERIFY (pi.hThread  = HNDLDupWithHDATA (pprc, pNewproc->phdProcEvt));
            }

            CeSafeCopyMemory (lppiProcInfo, &pi, sizeof (pi));
        }

    }

    DoUnlockHDATA (phdNewProc, -HNDLCNT_INCR);  // can trigger pre-close if the new process exited
    DoUnlockHDATA (phdNewTh,   -HNDLCNT_INCR);  // can trigger pre-close if the new thread exited
    UnlockHandleData (phdNewProc);
    UnlockHandleData (phdNewTh);


    DEBUGMSG(ZONE_LOADER1 || psi.dwErr, (L"NKCreateProcess exit: dwErr = %8.8lx\r\n", psi.dwErr));
    return !psi.dwErr;
}

//------------------------------------------------------------------------------
// NKOpenProcess - given process id, get the handle to the process.
//------------------------------------------------------------------------------
HANDLE NKOpenProcess (DWORD fdwAccess, BOOL fInherit, DWORD IDProcess)
{
    HANDLE hRet = NULL;
    PHDATA phd = LockHandleData ((HANDLE) IDProcess, g_pprcNK);
    DWORD dwErr = ERROR_INVALID_PARAMETER;

    

    // Do not use "GetProcPtr" here, so OpenProcess can be called on a process that had already exited.
    if (!fInherit && phd && (&cinfProc == phd->pci)) {
        dwErr = HNDLDuplicate (g_pprcNK, (HANDLE) IDProcess, pActvProc, &hRet);
    }

    UnlockHandleData (phd);

    if (!hRet) {
        SetLastError (dwErr);
    }
    return hRet;
}

//------------------------------------------------------------------------------
// PROCInit - initialize process handling (called at system startup)
//------------------------------------------------------------------------------
void PROCInit (void) 
{
    HANDLE hPrcNK;
    pActvProc = g_pprcNK;

    // create the handle table for kernel
    g_phndtblNK = g_pprcNK->phndtbl = CreateHandleTable (NUM_HNDL_1ST);
    DEBUGCHK (g_pprcNK->phndtbl);
    
    hPrcNK = HNDLCreateHandle (&cinfProc, g_pprcNK, g_pprcNK);
    DEBUGCHK (hPrcNK);

    HNDLSetUserInfo (hPrcNK, STILL_ACTIVE, g_pprcNK);
    dwActvProcId = (DWORD) hPrcNK & ~1;

    g_pprcNK->dwId = dwActvProcId;
    g_pprcNK->vaFree = VM_NKVM_BASE;
    g_pprcNK->lpszProcName = L"NK.EXE";
    g_pprcNK->tlsLowUsed = TLSSLOT_RESERVE;
    g_pprcNK->bPrio = THREAD_RT_PRIORITY_NORMAL;
#ifdef DEBUG
    g_pprcNK->ZonePtr = g_pOemGlobal->pdpCurSettings;
#endif
    g_pprcNK->e32.e32_stackmax = KRN_STACK_SIZE;
    g_pprcNK->hTok = TOKEN_SYSTEM;

    // initialized all the process lists
    InitDList (&g_pprcNK->thrdList);
    InitDList (&g_pprcNK->modList);
    InitDList (&g_pprcNK->viewList);
    InitDList (&g_pprcNK->prclist);

    // don't allow thread create one memory drop below 2% available
    if (g_cMinPageProcCreate < PageFreeCount / 50) {
        g_cMinPageProcCreate = PageFreeCount / 50;
    }


}


