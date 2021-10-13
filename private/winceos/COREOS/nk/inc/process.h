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
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    process.h - internal kernel process header
//

#ifndef _NK_PROCESS_H_
#define _NK_PROCESS_H_

#include "loader.h"
#include "syncobj.h"
#include "thread.h"


struct _LOCKPAGELIST {
    PLOCKPAGELIST pNext;        // next pointer
    DWORD       dwAddr;         // base address (block aligned)
    DWORD       cbSize;         // size of the locked region (block aligned)
};


// Note: 0x80 (MF_DEP_COMPACT) is already used in flag field. 
// Make sure new flags added DO NOT USE 0x80.
#define PROC_NOT_DEBUGGABLE         0x01
#define PROC_PSL_SERVER             0x02
#define PROC_TERMINATED             0x04
#define PROC_PGF_INPAGEOUT          0x08  // process is in middle of page out to page file
#define PROC_PGF_DISABLE            0x10  // page file support is disabled for this process
#define PROC_BP_MIGHT_BE_SET        0x20  // indicate that a break point might be set in this process (no code page-out in this case)
#define PROC_LICENSE_STATE          0x40  // used by managed loader to query if license check should be done

// Is the handle access only query? used only for process handles currently.
#define IsQueryHandle(phd) ((phd) && ((phd)->dwAccessMask == PROCESS_QUERY_INFORMATION))

//
// Process structure
//
struct _PROCESS {
    DLIST       prclist;        // 00: Doubly linked list of processes, must be 1st
    PHDATA      phdProcEvt;     // 08: the 'process event' to be signaled on process exit
    DWORD       dwId;           // 0C: process id, (handle for this process in NK's handle table 
    DLIST       thrdList;       // 10: Doubly linked list of all threads in the process 
    LPVOID      BasePtr;        // 18: Base pointer of exe load 
    PTHREAD     pDbgrThrd;      // 1C: thread debugging this process, if any 
    LPCWSTR     lpszProcName;   // 20: name of process 
    DWORD       tlsLowUsed;     // 24: TLS in use bitmask (first 32 slots) 
    DWORD       tlsHighUsed;    // 28: TLS in use bitmask (second 32 slots) 
    PPAGEDIRECTORY ppdir;       // 2C: page directory
    HANDLE      hTok;           // 30: process default token 
    PMODULE     pmodResource;   // 34: module that contains the resources 
    PHNDLTABLE  phndtbl;        // 38: handle table 
    DWORD       vaFree;         // 3C: lowest address of free VM 
    PNAME       pStdNames[3];   // 40: Pointer to names for stdio
    PVALIST     pVaList;        // 4C: list of externally VirtualAlloc'd addresses
    BYTE        bASID;          // 50: ASID for ARM.
    BYTE        fNotifyExiting; // 51: we're in the middle of notifying process exiting 
    BYTE        bChainDebug;    // 52: Did the creator want to debug child processes? 
    BYTE        bState;         // 53: process state
    LPDBGPARAM  ZonePtr;        // 54: address of Debug zone pointer 
    PLOCKPAGELIST pLockList;    // 58: locked page list
    openexe_t   oe;             // 5c: Pointer to executable file handle 
    e32_lite    e32;            // ??: structure containing exe header 
    o32_lite    *o32_ptr;       // ??: o32 array pointer for exe 
    long        nCallers;       // ??: number of callers into this process (PSL server only)
    WORD        wThrdCnt;       // ??: #of threads (MUST BE DWORD boundary, we use interlock operation on this)
    BYTE        bPrio;          // ??: highest priority of all threads of the process 
    BYTE        fFlags;         // ??: process flag
    DWORD       dwAffinity;     // ??: process affinity
    LPVOID      lpUnused;       // ??: was stub thread
    CRITICAL_SECTION csVM;      // ??: CS to control VM access
    CRITICAL_SECTION csLoader;  // ??: CS to control loading of DLL
    FAST_LOCK   hndlLock;
    DLIST       modList;        // ??: list of module loaded by the process 
    DLIST       viewList;       // ??: per-process view list 
    HANDLE      hMQDebuggeeWrite;    // ??: Message queue to write debug events
    PFNVOID     pfnTlsCleanup;  // ??: function to call on thread detach for cleanup of any TLS stored in this process
    HANDLE      hProcQuery;     // ??: handle to current process but with only query privileges
    DWORD       dwKrnTime;      // ??: total kernel time for all exited threads in the process
    DWORD       dwUsrTime;      // ??: total User time for all exited threads in the process
    FILETIME    ftCreate;       // ??: process creation time
    FILETIME    ftExit;         // ??: process exit time
    HANDLE      hTokCreator;    // ??: token of thread who created this process (could be NULL if process created by system)
    LONG        nCurSysTag;     // ??: current system virtual allocation tag value in this process
    WORD        wMemoryPriority; // ??: memory allocation priority
    WORD        fSystemProcess; // ??  if the process is a system process
    PAGEOUTOBJ  pageoutobj;     // ??: pageout object
    MEMSTAT     msCommit;       // ??; committed pages (VirtualAlloc + DLL/EXE globals + stack)
    MEMSTAT     msCodePaged;    // ??: pageable code pages
    MEMSTAT     msCodeNonPaged; // ??: non-pageable code pages
    MEMSTAT     msSharePaged;   // ??: pageable shared pages (e.g. file-backed mapfile)
    MEMSTAT     msShareNonPaged;// ??: non-pageable shared pages (e.g. ram-backed mapfile)
    LONG        lCntPageFault;  // ??: # of page fault taken
};


extern PPROCESS g_pprcNK;
extern PPAGEDIRECTORY g_ppdirNK;

#define ProcessNotDebuggable(pprc)              ((pprc)->fFlags & PROC_NOT_DEBUGGABLE)
#define IsProcNxCompatible(pprc)                ((pprc)->fFlags & MF_DEP_COMPACT)
#define IsProcessTerminated(pprc)               ((pprc)->fFlags & PROC_TERMINATED)
#define BreakPointMightBeSet(pprc)              ((pprc)->fFlags & PROC_BP_MIGHT_BE_SET)
#define IsProcSystemProcess(pprc)               ((pprc)->fSystemProcess)

#define     PROCESS_STATE_STARTING              0       // process is starting up
#define     PROCESS_STATE_NORMAL                1       // process is running normally
#define     PROCESS_STATE_START_EXITING         2       // process starts exiting
#define     PROCESS_STATE_VIEW_UNMAPPED         3       // exiting stage #2 - view are unmapped (view creation fails from here on)
#define     PROCESS_STATE_NO_LOCK_PAGES         4       // exiting stage #3 - locked page cleared (LockPages fails from here on)
#define     PROCESS_STATE_VM_CLEARED            5       // final exiting stage - VM cleared (VM allocation fails from here on)

typedef struct _ProcStartInfo {
    PHDATA phdCreator;      // creater thread's token handle data
    LPCWSTR pszImageName;   // image name
    LPCWSTR pszCmdLine;     // command line
    DWORD   dwErr;          // error code, 0 if success
    BOOL    fdwCreate;      // create flags (debug/suspended, etc)
    HANDLE  hProcAccount;   // set if process should be launched in a specific account
} ProcStartInfo, *LPProcStartInfo;
    

FORCEINLINE BOOL IsAddrCommitted (PPROCESS pprc, DWORD dwAddr)
{
    const PAGETABLE *pptbl = GetPageTable (pprc->ppdir, VA2PDIDX(dwAddr));
    return pptbl? IsPageCommitted (pptbl->pte[VA2PT2ND(dwAddr)]) : FALSE;
}

FORCEINLINE void LockLoader (PPROCESS pprc)
{
    EnterCriticalSection (&pprc->csLoader);
}

FORCEINLINE void UnlockLoader (PPROCESS pprc)
{
    LeaveCriticalSection (&pprc->csLoader);
}

//
// process state functions
//
FORCEINLINE BOOL IsProcessStartingUp (PCPROCESS pprc)
{
    return (PROCESS_STATE_STARTING == pprc->bState);
}

FORCEINLINE BOOL IsProcessNormal (PCPROCESS pprc)
{
    return (PROCESS_STATE_NORMAL == pprc->bState);
}

FORCEINLINE BOOL IsProcessExiting (PCPROCESS pprc)
{
    return (pprc->bState >= PROCESS_STATE_START_EXITING) || IsProcessTerminated (pprc);
}

FORCEINLINE BOOL IsProcessHandlesAllowed (PCPROCESS pprc)
{
    return ((pprc->bState >= PROCESS_STATE_NORMAL) && (pprc->bState < PROCESS_STATE_NO_LOCK_PAGES));
}

FORCEINLINE BOOL IsViewCreationAllowed (PCPROCESS pprc)
{
    return (pprc->bState < PROCESS_STATE_VIEW_UNMAPPED);
}

FORCEINLINE BOOL IsPagingAllowed (PCPROCESS pprc)
{
    return (pprc->bState < PROCESS_STATE_VIEW_UNMAPPED);
}

FORCEINLINE BOOL IsLockPageAllowed (PCPROCESS pprc)
{
    return (pprc->bState < PROCESS_STATE_NO_LOCK_PAGES);
}

FORCEINLINE BOOL IsVMAllocationAllowed (PCPROCESS pprc)
{
    return (pprc->bState < PROCESS_STATE_VM_CLEARED);
}

FORCEINLINE BOOL IsProcessBeingDebugged (PCPROCESS pprc)
{
    return (pprc->pDbgrThrd != NULL);
}

#ifdef DEBUG
FORCEINLINE BOOL OwnLoaderLock (PCPROCESS pprc)
{
    return OwnCS (&pprc->csLoader);
}
#endif

BOOL PROCTerminate (PPROCESS pprc, DWORD dwExitCode);
BOOL PROCFlushICache (PPROCESS pprc, LPCVOID lpBaseAddress, DWORD dwSize);
BOOL PROCReadMemory (PPROCESS pprc, LPCVOID lpBaseAddress, LPVOID lpBuffer, DWORD nSize);
BOOL PROCWriteMemory (PPROCESS pprc, LPVOID lpBaseAddress, LPVOID lpBuffer, DWORD nSize);
BOOL PROCSetVer (PPROCESS pprc, DWORD dwVersion);
DWORD PROCGetModInfo (PPROCESS pprc, LPVOID pBaseAddr, DWORD infoType, LPVOID pBuf, DWORD cb);
DWORD PROCGetModName (PPROCESS pprc, PMODULE pMod, LPWSTR lpFilename, DWORD cchLen);
BOOL PROCReadPEHeader (PPROCESS pprc, DWORD dwFlags, DWORD dwAddr, LPVOID pBuf, DWORD cbSize);
BOOL PROCReadKernelPEHeader (DWORD dwFlags, DWORD dwAddr, LPVOID pBuf, DWORD cbSize);
BOOL PROCChekDbgr (PPROCESS pprc, PBOOL pfRet);
DWORD PROCGetID (PPROCESS pprc);
BOOL PROCPageOutModule (PPROCESS pprc, PMODULE pMod, DWORD dwFlags);

LPVOID EXTPROCVMAlloc (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, DWORD fAllocType, DWORD fProtect, PDWORD pdwTag);
LPVOID PROCVMAlloc (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, DWORD fAllocType, DWORD fProtect, PDWORD pdwTag);
LPVOID PROCVMAllocCopy (PPROCESS pprc, HANDLE hDstProc, LPVOID pAddr, DWORD cbSize, DWORD fProtect);
BOOL PROCVMFree (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, DWORD dwFreeType, DWORD dwTag);
BOOL PROCSetAffinity (PPROCESS pprc, DWORD dwAffinity);
BOOL PROCGetAffinity (PPROCESS pprc, LPDWORD pdwAffinity);
BOOL PROCLoadModule (PPROCESS pprc, LPCWSTR lpwszModuleName);
BOOL PROCGetTimes (PPROCESS pprc, LPFILETIME lpCreationTime, LPFILETIME lpExitTime, LPFILETIME lpKernelTime, LPFILETIME lpUserTime);
BOOL PROCSetMemoryPriority (PPROCESS pprc, DWORD dwMemoryPriority);

//
// SwitchVM - switch to another process, return the old process
//
PPROCESS SwitchVM (PPROCESS pprc);


// PROCDelete - delete process object
// NOTE: a process object can be delete way before process handle got closed. The reason being that a process
//       might have exited, but other process still holding a handle to the "process", which can be used to 
//       query exit code.
BOOL PROCDelete (PPROCESS pprc);


// these following apis are not 'process method', but process related
BOOL PROCGetCode (HANDLE hth, LPDWORD dwExit);

BOOL NKCreateProcess (
    LPCWSTR lpszImageName,
    LPCWSTR lpszCommandLine,
    LPCE_PROCESS_INFORMATION pCeProcInfo
    );

// NKOpenProcess - given process id, get the handle to the process.
HANDLE NKOpenProcess (DWORD fdwAccess, BOOL fInherit, DWORD IDProcess);

// PROCInit - initialize process handling (called at system startup)
void PROCInit (void);

// foreground/killable process maintanance
void UpdateForegroundProcess (PPROCESS pprc);
void ResetForegroundProcess (PPROCESS pprc);

// update memory statistics
void UpdateMemoryStatistic (PMEMSTAT pmst, LONG lAddent);

FORCEINLINE PTHREAD MainThreadOfProcess (PPROCESS pprc)
{
    return (PTHREAD) pprc->thrdList.pBack;
}

FORCEINLINE BOOL IsMainThread (PCTHREAD pth)
{
    return pth == MainThreadOfProcess (pth->pprcOwner);
}

FORCEINLINE BOOL IsProcessAlive (PCPROCESS pprc)
{
    return !IsDListEmpty (&pprc->thrdList);
}

// validate process inside a kcall
FORCEINLINE BOOL KC_IsValidProcess (PCPROCESS pprc)
{
    return KC_ProcFromId (pprc->dwId) == pprc;
}


// check to see if all secondary threads exited
FORCEINLINE BOOL IsAll2ndThrdExited (PCPROCESS pprc)
{
    return (pprc->wThrdCnt <= 1);
}

// locking a process object with pprc object.
// NOTE: this function must be called with the right lock held such
//       that the process will not be destroyed while locking
FORCEINLINE void LockProcessObject (PPROCESS pprc)
{
    LockHDATA ((PHDATA) pprc->pageoutobj.pObj);
}

// unlocking a process object with pprc object
FORCEINLINE void UnlockProcessObject (PPROCESS pprc)
{
    UnlockHandleData ((PHDATA) pprc->pageoutobj.pObj);
}

// caluclate memory priority for paging
FORCEINLINE
DWORD
CalcMemPrioForPaging (PPROCESS pprc)
{
    DWORD dwMemPrio = pprc->wMemoryPriority;
    // since paging activity is not controled by user, it's hard for apps to be able to deal with paging failure.
    // we would give apps 1 level higher in memory priority when it needs to page in memory.
    // 1) if current thread is been terminated, let it page in as much as possible so that it can exit
    // 2) if the process is not kernel, let it get a level higher
    if (GET_DYING (pCurThread)) {
        dwMemPrio = MEMPRIO_KERNEL;

    } else if (dwMemPrio > MEMPRIO_KERNEL) {
        dwMemPrio --;
    }
    return dwMemPrio;
}

//
// safe get process name
//
LPCWSTR SafeGetProcName (PPROCESS pprc);

//
// SwitchActiveProcess - change the active process
//
PPROCESS SwitchActiveProcess (PPROCESS pprc);

// page out funciton
BOOL PageOutOneProcess (PPROCESS pprc);

//
// VM locking functions
//
__inline void UnlockVM (PPROCESS pprc)
{
    LeaveCriticalSection (&pprc->csVM);
}

__inline PPAGEDIRECTORY LockVM (PPROCESS pprc)
{
    EnterCriticalSection (&pprc->csVM);
    if (pprc->ppdir) {
        return pprc->ppdir;
    }
    LeaveCriticalSection (&pprc->csVM);
    return NULL;
}

#endif  // _NK_PROCESS_H_
