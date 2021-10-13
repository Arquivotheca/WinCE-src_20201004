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
//    handle.c - implementations of HANDLE
//

#include <kernel.h>

#define PHDE2PHNDTBL(phde)          ((PHNDLTABLE)((DWORD) (phde) & ~VM_PAGE_OFST_MASK))

// 32K instead of 64K so that it would take a race of greater than 32K Duplicate/
// CreateNamedEvent at the same time for it to overflow.  You'd need 32K threads
// to cause that situation (and 32K threads take a lot of RAM).
#define MAX_HANDLE_REF              0x7FFF0000

#define IDX_PRECLOSE                1               // index 1 of the function table is the preclose function

#define HNDLCNT_FROM_REF(dwRef)     HIWORD(dwRef)
#define LOCKCNT_FROM_REF(dwRef)     LOWORD(dwRef)

DLIST            g_NameList = { &g_NameList, &g_NameList };
extern CRITICAL_SECTION NameCS;

DWORD GetHandleCountFromHDATA (PHDATA phd)
{
    return phd? HNDLCNT_FROM_REF (phd->dwRefCnt) : 0;
}

PPROCESS GetProcPtr (PHDATA phd)
{
    PPROCESS pprc = (PPROCESS) GetObjPtrByType (phd, SH_CURPROC);
    return (pprc && pprc->dwId && IsProcessAlive (pprc))? pprc : NULL;
}

FORCEINLINE void FreePHData (PHDATA phd)
{
    if (phd->phdPolicy) {
        UnlockHandleData (phd->phdPolicy);
    }
    FreeMem (phd, HEAP_HDATA);
}


//
// Handle table locking functions
//
// NOTE: LockHandleTable and UnlockHandleTable acquires/release WRITE lock.
//       the only place where READ lock is used is in LockHandleData, where 
//       it explicitly use AcquireReadLock/ReleaseReadLock on the fast lock.
//

__inline PHNDLTABLE LockHandleTable (PPROCESS pprc)
{
    AcquireWriteLock (&pprc->hndlLock);
    if (pprc->phndtbl) {
        return pprc->phndtbl;
    }
    ReleaseWriteLock (&pprc->hndlLock);
    return NULL;
}
__inline void UnlockHandleTable (PPROCESS pprc)
{
    ReleaseWriteLock (&pprc->hndlLock);
}


__inline DWORD Idx2ndHdtblFromHandle (HANDLE h)
{
    return (DWORD) h >> 25;                     // top 7 bits (bit 25 to bit 31)
}

__inline DWORD IdxInHdtblFromHandle (HANDLE h)
{
    return (((DWORD) h << 7) >> 23);            // bottom 9 bits in HIWORD (bit 16 to bit 24)
}


//
// convert a handle to hdata, handle table is locked
//
PHDATA h2pHDATA (HANDLE h, PHNDLTABLE phndtbl)
{
    int    idx_plus_1 = IdxInHdtblFromHandle (h) + 1;
    PHDATA phd        = NULL;

    if (idx_plus_1 < (NUM_HNDL_2ND+1)) {
        // valid index for handle table (between 0- 510)
        
        DWORD idx2ndTbl = Idx2ndHdtblFromHandle (h);
        if (idx2ndTbl) {
            // 2nd level handle table
            phndtbl = phndtbl->p2ndhtbls[idx2ndTbl-1];
        } else {
            // 1st handle table
            idx_plus_1 -= MIN_HNDL;
        }
        if (phndtbl && (idx_plus_1 > 0)) {
            PHNDLENTRY phde = &phndtbl->hde[idx_plus_1-1];
            if ((HANDLE) ((DWORD)h|1) == phde->hndl.hValue) {
                phd = phde->phd;
            }
        }
    }
    return phd;
}
//
// create a handle table
//  1st-level (master) table - p1stTbl == NULL
//  2nd-level table          - p1stTbl == master handle table
//
PHNDLTABLE CreateHandleTable (PHNDLTABLE p1stTbl)
{
    PHNDLTABLE phndltbl = (PHNDLTABLE) GrabOnePage (PM_PT_ZEROED);

    if (phndltbl) {
        DWORD idx;
        DWORD idxLast;
        WORD  wReuseCnt;

        if (p1stTbl) {
            // creating secondary handle table
            wReuseCnt = p1stTbl->wReuseCntBase;
            idxLast    = NUM_HNDL_2ND - 1;
            
        } else {
            // creating master handle table
            static LONG lBaseReuseCnt = 0;
            wReuseCnt = (WORD) (((InterlockedIncrement (&lBaseReuseCnt) - 1) << 8) & 0xff00) | 0x3;
            idxLast    = NUM_HNDL_1ST - 1;

            // phndltbl->n2ndUsed      = 0; // page already zero'd
            phndltbl->idxLastTable  = INVALID_HNDL_INDEX;
        }

        phndltbl->wReuseCntBase = wReuseCnt;
        phndltbl->idxNextTable  = INVALID_HNDL_INDEX;
        //  phndltbl->idxFirstFree  = 0; // page already zero'd
        phndltbl->idxLastFree   = (WORD) idxLast;
            
        
        for (idx = 0; idx < idxLast; idx ++) {
            // phde->hndl.idx = (WORD) (idx + 1);   -- link free list
            // phde->hndl.reuseCnt = wReuseCnt;     -- initialize reuse cnt
            // use hndl.hValue to setup the value (better perf)
            phndltbl->hde[idx].hndl.hValue = (HANDLE) MAKELONG (wReuseCnt, idx+1);
            //phndltbl->hde[idx].phd = NULL;    // page already zero'd
        }
        // terminate the free list
        phndltbl->hde[idxLast].hndl.hValue = (HANDLE) MAKELONG (wReuseCnt, INVALID_HNDL_INDEX);
    }

    return phndltbl;
}

static void FreeHandleTable (PHNDLTABLE phtbl)
{
    FreeKernelPage (phtbl);
}

static void LinkHndlTable (PHNDLTABLE p1stTbl, DWORD idxTbl)
{
    DEBUGCHK (idxTbl < NUM_HNDL_TABLE);
    DEBUGCHK (INVALID_HNDL_INDEX == p1stTbl->p2ndhtbls[idxTbl]->idxNextTable);

    if (INVALID_HNDL_INDEX == p1stTbl->idxNextTable) {
        p1stTbl->idxNextTable = p1stTbl->idxLastTable = (WORD) idxTbl;
    } else {
        DEBUGCHK (p1stTbl->idxLastTable < NUM_HNDL_TABLE);
        DEBUGCHK (INVALID_HNDL_INDEX == p1stTbl->p2ndhtbls[p1stTbl->idxLastTable]->idxNextTable);
        p1stTbl->p2ndhtbls[p1stTbl->idxLastTable]->idxNextTable = (WORD) idxTbl;
        p1stTbl->idxLastTable = (WORD) idxTbl;
    }
}

static void LinkHndlEntry (PHNDLTABLE p1stTbl, DWORD idxTbl, DWORD idxEntry)
{
    PHNDLTABLE phndtbl = (idxTbl < NUM_HNDL_TABLE)? p1stTbl->p2ndhtbls[idxTbl] : p1stTbl;

    DEBUGCHK (idxEntry < ((phndtbl == p1stTbl)? NUM_HNDL_1ST : NUM_HNDL_2ND));
    DEBUGCHK (INVALID_HNDL_INDEX == phndtbl->hde[idxEntry].hndl.idx);

    if (INVALID_HNDL_INDEX == phndtbl->idxFirstFree) {
        // handle table was full before
        phndtbl->idxFirstFree = phndtbl->idxLastFree = (WORD) idxEntry;
        if (phndtbl != p1stTbl) {
            phndtbl->idxNextTable = INVALID_HNDL_INDEX;
            LinkHndlTable (p1stTbl, idxTbl);
        }
    } else {
        // handle table was not full
        DEBUGCHK (INVALID_HNDL_INDEX != phndtbl->idxLastFree);
        DEBUGCHK (INVALID_HNDL_INDEX == phndtbl->hde[phndtbl->idxLastFree].hndl.idx);
        phndtbl->hde[phndtbl->idxLastFree].hndl.idx = (WORD) idxEntry;
        phndtbl->idxLastFree = (WORD) idxEntry;
    }
    
}

#define IsHandleTableClosing(p1sttbl)       (INVALID_HNDL_INDEX == (p1sttbl)->n2ndUsed)

static PHNDLENTRY HNDLAlloc (PPROCESS pprc, PHDATA phd, PHANDLE phndl)
{
    PHNDLENTRY phde      = NULL;
    PHNDLTABLE p1stTbl   = LockHandleTable (pprc);

    if (p1stTbl) {

        DWORD      idxBase = MIN_HNDL;
        PHNDLTABLE phndtbl = p1stTbl;

        if (INVALID_HNDL_INDEX == p1stTbl->idxFirstFree) {

            DWORD idx2ndTbl = p1stTbl->idxNextTable;
            
            // nothing in the 1st handle table
            if (INVALID_HNDL_INDEX != idx2ndTbl) {
                // got free entry in 2ndary handle tables
                phndtbl = p1stTbl->p2ndhtbls[idx2ndTbl];
                
            } else if (p1stTbl->n2ndUsed >= NUM_HNDL_TABLE) {
                // completely out of handles, or process exiting
                phndtbl = NULL;
                
            } else {
                // no free handle entry, but still room to create new secondary handle table.
                
                // Try to create a new handle table.
                // NOTE: release handle table lock while allocating
                UnlockHandleTable (pprc);
                phndtbl = CreateHandleTable (p1stTbl);
                VERIFY (LockHandleTable (pprc) == p1stTbl);

                if (phndtbl) {
                    // need to check # of handle table used again as we can get preempted, and 
                    // other threads exhausted the handle table.
                    if (p1stTbl->n2ndUsed < NUM_HNDL_TABLE) {
                        // link the table into the free list
                        idx2ndTbl = p1stTbl->n2ndUsed ++;
                        p1stTbl->p2ndhtbls[idx2ndTbl] = phndtbl;
                        LinkHndlTable (p1stTbl, idx2ndTbl);

                    } else {
                        // preempted and handle table exhausted, free the handle table that we just created.
                        // NOTE: release handle table lock before freeing memory
                        UnlockHandleTable (pprc);
                        FreeHandleTable (phndtbl);
                        VERIFY (LockHandleTable (pprc) == p1stTbl);
                    }

                    idx2ndTbl = p1stTbl->idxNextTable;
                    phndtbl = (INVALID_HNDL_INDEX == idx2ndTbl)
                            ? NULL
                            : p1stTbl->p2ndhtbls[idx2ndTbl];
                }
            }

            idxBase = (idx2ndTbl+1) << HND_HD2ND_SHIFT;

        }

        DEBUGMSG (!phndtbl && !IsHandleTableClosing (p1stTbl), (L"HNDLAlloc failed due to out of %s\r\n", 
                    (p1stTbl->n2ndUsed < NUM_HNDL_TABLE)? L"Memory" : L"Handle"));

        if (phndtbl && !IsHandleTableClosing (p1stTbl)) {
            DWORD   idx = phndtbl->idxFirstFree;
            PHDATA  phdAPISet = phd? phd->pci->phdApiSet : NULL;

            DEBUGCHK (idx < ((phndtbl == p1stTbl)? NUM_HNDL_1ST : NUM_HNDL_2ND));
            
            if (phdAPISet) {
                // user mode API set, increment ref count
                LockHDATA (phdAPISet);
            }
            
            phde = &phndtbl->hde[idx];
            phndtbl->idxFirstFree = phde->hndl.idx;                 // advance free index to next

            if (INVALID_HNDL_INDEX == phndtbl->idxFirstFree) {
                // last handle used in this handle table. Move on to the next handle table
                p1stTbl->idxNextTable = phndtbl->idxNextTable;
            }
            
            phde->phd = phd;
            phde->hndl.idx = (WORD) (idx + idxBase);

            *phndl = phde->hndl.hValue;

        }
        
        UnlockHandleTable (pprc);
    }

    return phde;

}


static void HNDLReleaseReservedHandle (PPROCESS pprc, DWORD idx)
{
    DWORD idx2ndTbl = Idx2ndHdtbl (idx);
    PHNDLTABLE  p1stTbl;

    DEBUGCHK (idx >= MIN_HNDL);

    if (!idx2ndTbl --) {
        // 1st handle table
        idx -= MIN_HNDL;

    } else {

        // 2nd level handle table
        idx = IdxInHdtbl (idx);
    }

    p1stTbl = LockHandleTable (pprc);
    
    DEBUGCHK (p1stTbl);

    LinkHndlEntry (p1stTbl, idx2ndTbl, idx);
    
    UnlockHandleTable (pprc);
}

//
// LockHandleData: Locking a handle prevent the HDATA from being destroyed
// when a handle is locked, the HDATA of the handle cannot be destroyed.
// NOTE: Locking a handle doesn't guarantee serialization
//
PHDATA LockHandleData (HANDLE h, PPROCESS pprc)
{
    PHDATA     phd;
    PHNDLTABLE phndtbl;

    AcquireReadLock (&pprc->hndlLock);

    phndtbl = pprc->phndtbl;
    phd     = phndtbl? h2pHDATA (h, phndtbl) : NULL;
    if (phd) {
        DEBUGCHK ((int) phd->dwRefCnt > 0);
        InterlockedIncrement ((PLONG) &phd->dwRefCnt);  // lock increment is 1
    }
    
    ReleaseReadLock (&pprc->hndlLock);

    return phd;
}

// locking via PHDATA directly
void LockHDATA (PHDATA phd)
{
    InterlockedIncrement ((PLONG) &phd->dwRefCnt);
}

//
// lock a handle, possibly pseudo handle like GetCurrentProcess()/GetCurrentThread()
//
PHDATA LockPseudoHandle (HANDLE h, PPROCESS pprc)
{
    return (GetCurrentProcess () == h)
         ? LockHandleData (hActvProc, g_pprcNK)
         : ((GetCurrentThread () == h)
             ? LockHandleData (hCurThrd, g_pprcNK)
             : NULL);
}


//
// DoLockHDATA: Increment the refcount by the amount specified
//
BOOL DoLockHDATA (PHDATA phd, DWORD dwAddend)
{
    if (phd->dwRefCnt < MAX_HANDLE_REF) {
        InterlockedExchangeAdd ((PLONG) &phd->dwRefCnt, dwAddend);
        return TRUE;
    }
    return FALSE;
}


//
// DoUnlockHDATA: finish using a handle, will decrement ref count. The object object will be destroyed if ref-count gets to 0
//
BOOL DoUnlockHDATA (PHDATA phd, LONG lAddend)
{
    BOOL fRet = TRUE;

    if (phd) {
        DWORD dwRefCnt;

        if (phd->pName) {
            // grab the name cs before decrementing the ref count, remove the HDATA from named list
            // as soon as handle count gets to 0, regardless of the lock count.
            EnterCriticalSection (&NameCS);
            dwRefCnt = InterlockedExchangeAdd ((PLONG) &phd->dwRefCnt, lAddend);  // lRefCnt = previous refcnt
            if (HNDLCNT_FROM_REF (dwRefCnt) && !HNDLCNT_FROM_REF (dwRefCnt + lAddend)) {
                DEBUGCHK ((phd->dl.pBack != &phd->dl) && (phd->dl.pFwd != &phd->dl));  // check for double-remove
                RemoveDList (&phd->dl);
                InitDList (&phd->dl);  // point at self for safety against double-remove
            }
            LeaveCriticalSection (&NameCS);
            dwRefCnt += lAddend;
        } else {
            dwRefCnt = InterlockedExchangeAdd ((PLONG) &phd->dwRefCnt, lAddend) + lAddend;
        }

        if (!dwRefCnt) {
            //
            // last reference to the HDATA object, destroy it
            //
            fRet = PHD_CloseHandle (phd);

            if (phd->pName) {
                FreeName (phd->pName);
            }
            
            DEBUGMSG (ZONE_OBJDISP, (L"Freeing HDATA %8.8lx\r\n", phd));
            FreePHData (phd);

        // call PreClose function if this is a CloseHandle call and it's the last handle to the HDATA            
        } else if (!HNDLCNT_FROM_REF (dwRefCnt) && (-HNDLCNT_INCR == lAddend)) {
            const CINFO *pci = phd->pci;
            FARPROC pfnPreClose = (FARPROC) (pci? pci->ppfnIntMethods[IDX_PRECLOSE] : NULL);
            if (pfnPreClose && ((LPVOID)-1 != pfnPreClose)) {
                PHD_PreCloseHandle (phd);
            }
        }
    }
    return fRet;
}

//
// HNDLGetUserInfo - retrieve the data field of HDATA
//
DWORD HNDLGetUserInfo (HANDLE h, PPROCESS pprc)
{
    PHDATA phd = LockHandleData (h, pprc);
    DWORD dwRet = 0;
    if (phd) {
        dwRet = phd->dwData;
        UnlockHandleData (phd);
    }
    return dwRet;
}
    
//
// HNDLSetUserInfo - update the data field of HDATA
//
BOOL HNDLSetUserInfo (HANDLE h, DWORD dwData, PPROCESS pprc)
{
    PHDATA phd = LockHandleData (h, pprc);
    if (phd) {
        phd->dwData = dwData;
        UnlockHandleData (phd);
    }
    return NULL != phd;
}

//
// HNDLSetObjPtr - update the pvObj field of HDATA
//
BOOL HNDLSetObjPtr (HANDLE h, LPVOID pvObj, PPROCESS pprc)
{
    PHDATA phd = LockHandleData (h, pprc);
    if (phd) {
        phd->pvObj= pvObj;
        UnlockHandleData (phd);
    }
    return NULL != phd;
}

//
// HNDLGetServerId - get the server process id of a handle
//
DWORD HNDLGetServerId (HANDLE h)
{
    PHDATA phd  = LockHandleParam (h, pActvProc);
    DWORD  dwId = 0;

    if (!phd) {
        SetLastError (ERROR_INVALID_HANDLE);
    } else {
        dwId = phd->pci->dwServerId;
        if (!dwId) {
            dwId = g_pprcNK->dwId;
        }
        UnlockHandleData (phd);
    }
    return dwId;
}

static PHDATA g_phdDelayFreeThread;

//
// For a fully exited thread - add the "thread-id" to the delayed free list
//
void KC_AddDelayFreeThread (PHDATA phdThread)
{
    // can't be a named handle, for we're using the DLIST field to link the delayed free list
    DEBUGCHK (!phdThread->pName);

    InterlockedPushList (&g_phdDelayFreeThread, phdThread);
}

//
// clean up all the threads in the delayed free list
//
void CleanDelayFreeThread (void)
{
    PHDATA phdThread;
    while (NULL != (phdThread = InterlockedPopList (&g_phdDelayFreeThread))) {
        DEBUGCHK (!phdThread->pvObj);
        DEBUGCHK (phdThread->dwRefCnt > HNDLCNT_INCR);
        DoUnlockHDATA (phdThread, -HNDLCNT_INCR);
    }
}

//
// AllocHData - allocate/init an HDATA structure
//
PHDATA AllocHData (PCCINFO pci, PVOID pvObj)
{
    PHDATA phd;

    // clean up delayed free list, if any
    CleanDelayFreeThread ();
    
    phd = AllocMem (HEAP_HDATA);

    if (phd) {
        phd->dwData       = 0;
        phd->dwRefCnt     = HNDLCNT_INCR;
        phd->pName        = NULL;
        phd->dwAccessMask = 0; // initially assume handle has no access set
        phd->pci          = pci;
        phd->pvObj        = pvObj;
        phd->phdPolicy    = NULL;
        DEBUGMSG (ZONE_OBJDISP, (L"Allocated HDATA %8.8lx, pci = %8.8lx, type = %8.8lx\r\n", phd, pci, pci->type));
    }

    return phd;
}

//
// HNDLDupWithHDATA - duplicate a handle with hdata, the handle *must* have been locked
//
HANDLE HNDLDupWithHDATA (PPROCESS pprc, PHDATA phd, LPDWORD pdwErr)
{
    HANDLE hndl     = NULL;
    DWORD  dwRefCnt = phd->dwRefCnt;
    
    //
    // don't duplicate a handle with handle-count == 0. Otherwise, we can call into pre-close twice in the following
    // race condition.
    // 
    //  thread 1: DuplicateHandle, preempted after locked the source handle, before increment handle count
    //  thread 2: close handle, calls preclose for handle count drop to 0
    //  thread 1: increment handle count, returned a handle
    //  thread 1: close handle, call preclose again.
    //
    //
    *pdwErr = 0;
    while ((dwRefCnt < MAX_HANDLE_REF) && HNDLCNT_FROM_REF (dwRefCnt)) {
        if (InterlockedCompareExchange ((PLONG)&phd->dwRefCnt, dwRefCnt+HNDLCNT_INCR, dwRefCnt) == (LONG) dwRefCnt) {

            if (!HNDLAlloc (pprc, phd, &hndl)) {
                *pdwErr = ERROR_NO_MORE_USER_HANDLES;
                // failed to allocate handle, decrement refcount we added
                DoUnlockHDATA (phd, -HNDLCNT_INCR);
            }
            break;
        }
        dwRefCnt = phd->dwRefCnt;
    }
    
    if (!hndl && !*pdwErr) {
        *pdwErr = HNDLCNT_FROM_REF (dwRefCnt)? ERROR_NOT_ENOUGH_MEMORY : ERROR_INVALID_HANDLE;
    }
    return hndl;
}


//
// HNDLCreateHandle - Create a handle, the process object had been locked.
//
HANDLE HNDLCreateHandle (PCCINFO pci, PVOID pvObj, PPROCESS pprc, DWORD dwAccessMask)
{
    PHDATA phd = AllocHData (pci, pvObj);
    HANDLE h   = NULL;

    DEBUGMSG (ZONE_MEMORY, (L"+HNDLCreateHandle %8.8lx, %8.8lx %8.8lx\r\n", pci, pvObj, pprc));
    if (phd) {

        if (!HNDLAlloc (pprc, phd, &h)) {
            DEBUGMSG (ZONE_OBJDISP, (L"Freeing HDATA %8.8lx\r\n", phd));
            FreeMem (phd, HEAP_HDATA);
        } else {
            phd->dwAccessMask = dwAccessMask;
            CELOG_OpenOrCreateHandle(phd, TRUE);
        }
    }
    DEBUGMSG (ZONE_MEMORY, (L"-HNDLCreateHandle %8.8lx\r\n", h));
    return h;
}

//
// Find an existing named object which matches given pci type and name
//
// Note: lpdwErr is modified only if an object with the same name is found.
//
PHDATA HNDLFindNamedObject (PCCINFO pci, PNAME pName, LPDWORD lpdwErr)
{
    PHDATA phd = NULL, phdwalk;

    DEBUGCHK (OwnCS (&NameCS));

    // iterate through the name list to find existing named handles
    for (phdwalk = (PHDATA) g_NameList.pFwd; (PHDATA) &g_NameList != phdwalk; phdwalk = (PHDATA) phdwalk->dl.pFwd) {
        if ((phdwalk->pci == pci) && !NKwcscmp (phdwalk->pName->name, pName->name)) {
            if (phdwalk->dwRefCnt >= MAX_HANDLE_REF) {
                *lpdwErr = ERROR_NOT_ENOUGH_MEMORY;
            } else {
                phd = phdwalk;
                InterlockedExchangeAdd ((PLONG)&phd->dwRefCnt, HNDLCNT_INCR);  // increment the ref count of the phd
                *lpdwErr = ERROR_ALREADY_EXISTS;
            }
            break;
        }
    }
    
    return phd;
}

//
// HNDLCreateNamedHandle - create a named handle, the process object is locked. In case already exist, return the 
//                         existing object and *pdwErr set to ERROR_ALREADY_EXIST
//
// pName is assumed to be canonicalized before this call.
//
//
HANDLE HNDLCreateNamedHandle (PCCINFO pci, PVOID pvObj, PPROCESS pprc, DWORD dwRequestedAccess, PNAME pName, DWORD dwObjectType, BOOL fCreate, LPDWORD pdwErr)
{
    HANDLE           hRet  = NULL;
    DWORD            dwErr = 0;
    KHNDL            hkndl = {0};
    PHNDLENTRY       phde  = HNDLAlloc (pprc, NULL, &hkndl.hValue);

    if (!phde) {
        dwErr = ERROR_OUTOFMEMORY;
        
    } else {

        static volatile DWORD s_glblTicket = 0; // protected by NameCS
        DWORD   currTicket;
        PHDATA  phd;

        DEBUGCHK (hkndl.hValue);

        EnterCriticalSection (&NameCS);

        // currTicket is used to decide if we need to rescan the list if needed.
        currTicket = s_glblTicket;
        phd = HNDLFindNamedObject (pci, pName, &dwErr);

        LeaveCriticalSection (&NameCS);

        if (!dwErr) {
            // named object not yet exist, try to create a new one
            DEBUGCHK (!phd);
            
            if (fCreate) {
                phd = AllocHData (pci, pvObj);
                
                if (phd) {
                    phd->pName = pName;
                    phd->dwAccessMask = dwRequestedAccess;

                } else {
                    dwErr = ERROR_OUTOFMEMORY;
                }
            } else {
                dwErr = ERROR_FILE_NOT_FOUND;
            }
        }

        if (phd) {
            
            switch (dwErr) {
            case 0:
                // newely created, add it to the named object list
                EnterCriticalSection (&NameCS);

                // if named object list got updated, scan again.
                if (currTicket != s_glblTicket) {
                    PHDATA phdOld = HNDLFindNamedObject (pci, pName, &dwErr);
                    if (dwErr) {
                        // found existing object with same name or there was an error; free the new object.
                        DEBUGCHK (!phdOld ||  (dwErr == ERROR_ALREADY_EXISTS));
                        FreePHData (phd);
                        phd = phdOld;
                    }
                }

                // if new object, add it to the list.
                if (!dwErr) {
                    s_glblTicket++;
                    AddToDListHead (&g_NameList, &phd->dl);
                }
                
                LeaveCriticalSection (&NameCS);

                if (!phd) {
                    // HNDLFindNamedObject failed, (more than 32K refs to the same object created???)
                    DEBUGCHK (dwErr && (ERROR_ALREADY_EXISTS != dwErr));
                    break;
                }

                // fall through to assocate the object with handle
                __fallthrough;
                
            case ERROR_ALREADY_EXISTS:
                // Note - handle table lock is not required here, as we have the handle table entry reserved
                //        and we only update the phd field from 0 -> valid
                {
                    PHDATA phdAPISet;

                    DEBUGCHK (!phde->phd && phd);

                    phdAPISet = phd->pci->phdApiSet;
                    
                    if (phdAPISet) {
                        // user mode API set, increment ref count
                        LockHDATA (phdAPISet);
                    }
                    
                    phde->phd  = phd;
                    hRet = hkndl.hValue;
                }
                break;
                
            default:
                DEBUGCHK (!hRet);
                break;
            }
        }

        // release the reserved handle in case of failure
        if (!hRet) {
            DEBUGCHK (!phde->phd);
            phde->hndl.idx = INVALID_HNDL_INDEX;
            HNDLReleaseReservedHandle (pprc, hkndl.idx);
        }
    }
    
    *pdwErr = dwErr;
    return hRet;
}

//
// HNDLZapHandle - remove a handle from the handle table, lock the hdata, and return the hdata of the handle
//
PHDATA HNDLZapHandle (PPROCESS pprc, HANDLE h)
{
    KHNDL       khd;
    DWORD       idx, idx2ndTbl;
    PHDATA      phd = NULL;

    DEBUGMSG (ZONE_OBJDISP, (L"Closing Handle h = %8.8lx, pprc id = %8.8lx\r\n", h, pprc->dwId));

    // calculate index based on handle    
    khd.hValue = h;
    idx        = khd.idx;
    idx2ndTbl  = Idx2ndHdtbl (idx);

    if (idx >= MIN_HNDL) {

        PHNDLTABLE  p1sttbl = LockHandleTable (pprc);

        if (p1sttbl) {

            if (!IsHandleTableClosing (p1sttbl)) {
                PHNDLTABLE  phndtbl = p1sttbl;
                DWORD       idxLimit = 0;

                if (!idx2ndTbl --) {
                    // 1st handle table
                    idxLimit = NUM_HNDL_1ST;
                    idx     -= MIN_HNDL;

                } else {

                    phndtbl = p1sttbl->p2ndhtbls[idx2ndTbl];
                    
                    if (phndtbl) {
                        // 2nd level handle table
                        idxLimit = NUM_HNDL_2ND;
                        idx      = IdxInHdtbl (idx);
                    }
                }

                if (idx < idxLimit) {
                    PHNDLENTRY phde = &phndtbl->hde[idx];
                    if (((HANDLE) ((DWORD)h|1) == phde->hndl.hValue)) {
                        
                        phd = phde->phd;
                        if (phd) {
                            // valid handle, lock it. 
                            InterlockedIncrement ((PLONG)&phd->dwRefCnt);

                            phde->hndl.reuseCnt += 4; // increment reuse count by 4 (lower 2 bits remains the same)
                            phde->hndl.idx = INVALID_HNDL_INDEX;
                            phde->phd = NULL;

                            LinkHndlEntry (p1sttbl, idx2ndTbl, idx);
                        }

                    }
                }
            }

            UnlockHandleTable (pprc);
        }
    }

    return phd;
}


static void ReleaseHData (PHDATA phd)
{
    PHDATA phdAPISet = phd->pci->phdApiSet;
    DoUnlockHDATA (phd, -HNDLCNT_INCR);     // close the handle, can trigger pre-close
    DoUnlockHDATA (phd, -LOCKCNT_INCR);     // unlock the handle, can trigger close, and destroy the object

    // decrement ref-count to the API set
    DoUnlockHDATA (phdAPISet, -LOCKCNT_INCR);
}

//
// HNDLCloseHandle - close a handle
//
BOOL HNDLCloseHandle (PPROCESS pprc, HANDLE h)
{
    PHDATA phd = HNDLZapHandle (pprc, h);

    DEBUGMSG (ZONE_OBJDISP, (L"Closing Handle h = %8.8lx, pprc id = %8.8lx\r\n", h, pprc->dwId));

    if (phd) {
        ReleaseHData (phd);
    }

    return phd != NULL;

}


//
// HNDLCloseOneHndlTbl - Close all handles in a handle table
//
static void HNDLCloseOneHndlTbl (PHNDLTABLE phndtbl, DWORD nMaxHndls)
{
    DWORD idx;
    PHDATA phd;
    PHNDLENTRY phde;
    for (idx = 0; idx < nMaxHndls; idx ++) {
        phde = &phndtbl->hde[idx];
        phd  = phde->phd;
        if (phd) {
            phde->hndl.idx = 0;         // 0 is never a valid handle index. i.e. this handle is invalid from here on
            phde->phd      = NULL;

            // lock the handle data
            VERIFY (InterlockedIncrement ((PLONG) &phd->dwRefCnt) > HNDLCNT_INCR);

            ReleaseHData (phd);
        }
    }
}

//
// HNDLCloseAllHandles - close all handles of a process, the process object had been locked.
//
void HNDLCloseAllHandles (PPROCESS pprc)
{
    PHNDLTABLE p1sttbl = LockHandleTable (pprc);

    if (p1sttbl) {
        DWORD idx, n2ndUsed = p1sttbl->n2ndUsed;
        
        p1sttbl->idxFirstFree = INVALID_HNDL_INDEX;
        p1sttbl->idxNextTable = INVALID_HNDL_INDEX;
        p1sttbl->n2ndUsed     = INVALID_HNDL_INDEX;

        // After this, IsHandleTableClosing will be TRUE and that no handle will ever be allocated/freed for this process.
        // So we can zap the handles without holding handle table lock
        
        UnlockHandleTable (pprc);

        // close all handles in secondary handle tables
        for (idx = 0; idx < n2ndUsed; idx ++) {
            DEBUGCHK (p1sttbl->p2ndhtbls[idx]);
            HNDLCloseOneHndlTbl (p1sttbl->p2ndhtbls[idx], NUM_HNDL_2ND);
        }

        // close all handles in master handle tables
        HNDLCloseOneHndlTbl (p1sttbl, NUM_HNDL_1ST);

    }
}

//
// HNDLDeleteHandleTable - final clean for handle table, delete the handle table allocated
//
void HNDLDeleteHandleTable (PPROCESS pprc)
{
    PHNDLTABLE p1sttbl = LockHandleTable (pprc);

    if (p1sttbl) {
        DWORD  idx2nd = 0;

        pprc->phndtbl = NULL;
        
        UnlockHandleTable (pprc);

        for (idx2nd = 0; idx2nd < NUM_HNDL_TABLE; idx2nd ++) {
            if (p1sttbl->p2ndhtbls[idx2nd]) {
                FreeHandleTable (p1sttbl->p2ndhtbls[idx2nd]);
            }
        }

        FreeHandleTable (p1sttbl);
    }
 }

//
// HNDLDuplicate - worker function to duplicate a handle
//      return error code
//
DWORD HNDLDuplicate (PPROCESS pprcSrc, HANDLE hSrc, PPROCESS pprcDst, PHANDLE pDstHndl)
{
    DWORD dwErr = ERROR_INVALID_HANDLE;
    PHDATA phd = LockHandleParam (hSrc, pprcSrc);

    if (phd) {
        *pDstHndl = HNDLDupWithHDATA (pprcDst, phd, &dwErr);
        UnlockHandleData (phd);
    }

    return dwErr;
}


//
// NKDuplicateHandle - duplicate a handle
//
BOOL NKDuplicateHandle (
    HANDLE  hSrcProc,           // source process of the handle to be duplicated
    HANDLE  hSrcHndl,           // source handle
    HANDLE  hDstProc,           // target process of the handle to be duplicated
    LPHANDLE lpDstHndl,         // (OUT) the result handle
    DWORD   dwAccess,           // desired access (ignored)
    BOOL    bInherit,           // inherit (must be false)
    DWORD   dwOptions           // options (only DUPLICAT_SAME_ACCESS and DUPLICATE_CLOSE SOURCE supported)
) {
    DWORD    dwErr = 0;
    PHDATA   phdSrc  = LockHandleParam (hSrcProc, pActvProc), 
             phdDst  = LockHandleParam (hDstProc, pActvProc);
    PPROCESS pprcSrc = GetProcPtr (phdSrc),
             pprcDst = GetProcPtr (phdDst);
    HANDLE   hDst    = NULL;

    DEBUGMSG (ZONE_ENTRY, (L"NKDuplicateHandle Entry: %8.8lx  %8.8lx  %8.8lx  %8.8lx  %8.8lx  %8.8lx  %8.8lx %8.8lx\r\n",
        hSrcProc, hSrcHndl, hDstProc, lpDstHndl, dwAccess, bInherit, dwOptions, pActvProc->dwId));

    // if dwOptions is 0, set it to DUPLICATE_SAME_ACCESS
    if (!dwOptions) {
        dwOptions = DUPLICATE_SAME_ACCESS;
    }
    
    // validate parameters
    if (bInherit    // bInHerit must be false
        || !(DUPLICATE_SAME_ACCESS & dwOptions)         // option must contain DUPLICATE_SAME_ACCESS
        || (~(DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE) & dwOptions)  // can't have anything other than these two
        || !lpDstHndl) {                                // lpDstHndl can't be NULL
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else if (!pprcSrc || !pprcDst) {
        dwErr = ERROR_INVALID_HANDLE;

    } else if (IsQueryHandle (phdSrc) || IsQueryHandle (phdDst)) {
        dwErr = ERROR_ACCESS_DENIED;
        
    } else if ((0 == (dwErr = HNDLDuplicate (pprcSrc, hSrcHndl, pprcDst, &hDst)))
            && !CeSafeCopyMemory (lpDstHndl, &hDst, sizeof (hDst))) {
        HNDLCloseHandle (pprcDst, hDst);
        dwErr = ERROR_INVALID_PARAMETER;
    }

    // MSDN doc: source handle shoule be closed if DUPLICATE_CLOSE_SOURCE is specified, regardless of error
    if (pprcSrc && (DUPLICATE_CLOSE_SOURCE & dwOptions)) {
        HNDLCloseHandle (pprcSrc, hSrcHndl);
    }

    UnlockHandleData (phdDst);
    UnlockHandleData (phdSrc);

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }
    DEBUGMSG (ZONE_ENTRY, (L"NKDuplicateHandle returns: dwErr = %8.8lx, *lpDstHndl = %8.8lx\r\n", dwErr, *lpDstHndl));

    return !dwErr;
}
    

