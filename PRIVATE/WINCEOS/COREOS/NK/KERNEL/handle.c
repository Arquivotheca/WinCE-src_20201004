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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//
//    handle.c - implementations of HANDLE
//

#include <kernel.h>
#include <aclpriv.h>

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

BOOL ACLCheck (PCSECDESCHDR psd);
DWORD AllocPSD (LPSECURITY_ATTRIBUTES lpsa, PNAME *psd);

DWORD GetHandleCountFromHDATA (PHDATA phd)
{
    return phd? HNDLCNT_FROM_REF (phd->dwRefCnt) : 0;
}

PPROCESS GetProcPtr (PHDATA phd)
{
    PPROCESS pprc = (PPROCESS) GetObjPtrByType (phd, SH_CURPROC);
    return (pprc && IsProcessAlive (pprc))? pprc : NULL;
}

//
// Handle table locking functions
//
__inline PHNDLTABLE LockHandleTable (PPROCESS pprc)
{
    EnterCriticalSection (&pprc->csHndl);
    if (pprc->phndtbl) {
        return pprc->phndtbl;
    }
    LeaveCriticalSection (&pprc->csHndl);
    return NULL;
}
__inline void UnlockHandleTable (PPROCESS pprc)
{
    LeaveCriticalSection (&pprc->csHndl);
}

//
// convert a handle to hdata, handle table is locked
//
PHDATA h2pHDATA (HANDLE h, PHNDLTABLE phndtbl)
{
    PHNDLENTRY  phde = NULL;
    KHNDL       khd;
    DWORD       idx;
    
    khd.hValue  = h;
    idx         = khd.idx;
        
    if (idx >= MIN_HNDL) {
        DWORD idxLimit  = 0;
        DWORD idx2ndTbl = Idx2ndHdtbl (idx);

        if (!idx2ndTbl --) {
            // 1st level handle table, idx is base MIN_HNDL
            idx     -= MIN_HNDL;
            idxLimit = NUM_HNDL_1ST;
            
        } else if (phndtbl = phndtbl->p2ndhtbls[idx2ndTbl]) {
            // 2nd level handle table
            idx      = IdxInHdtbl (idx);
            idxLimit = NUM_HNDL_2ND;
        }

        if (idx < idxLimit) {
            phde = &phndtbl->hde[idx];
        }
    }

    return (phde && ((HANDLE) ((DWORD)h|1) == phde->hndl.hValue))? phde->phd : NULL;    
}

//
// create a handle table
//  1st-level (master) table: nFree = NUM_HNDL_1ST
//  2nd-level table: nFree = NUM_HNDL_2ND
//
PHNDLTABLE CreateHandleTable (DWORD nFree)
{
    PHNDLTABLE phndltbl = (PHNDLTABLE) GrabOnePage (PM_PT_ZEROED);

    if (phndltbl) {
        phndltbl->nFree = nFree;
        if (NUM_HNDL_1ST == nFree) {
            static LONG lCnt;
            phndltbl->wReuseCntBase = (WORD) (((InterlockedIncrement (&lCnt) - 1) << 8) & 0xff00);
        }
    }
        
    return phndltbl;
}

static void FreeHandleTable (PHNDLTABLE phtbl)
{
    FreePhysPage (GetPFN (phtbl));
}

static DWORD FindFreeHndl (PHNDLENTRY pEnt, DWORD idxStart, DWORD idxLimit)
{
    for ( ; idxStart < idxLimit; idxStart ++) {
        if (!pEnt[idxStart].phd) {
            break;
        }
    }
    return idxStart;
}

static HANDLE HNDLAlloc (PHNDLTABLE phndtbl, PHDATA phd)
{
    DWORD idxLimit = NUM_HNDL_1ST;
    DWORD idxBase  = MIN_HNDL;
    DWORD idx;
    PHDATA phdAPISet;
    PHNDLENTRY phde;
    
    if (!phndtbl->nFree) {
        // no more room in 1st level handle table, search 2nd level handle table

        DWORD       idx2ndTbl;
        PHNDLTABLE  p1stTbl = phndtbl;
       
        idxLimit = NUM_HNDL_2ND;   // change limit to 2nd level handle table
        
        for (idx2ndTbl = p1stTbl->idx2ndFree; idx2ndTbl < NUM_HNDL_TABLE; idx2ndTbl ++) {
            if (!(phndtbl = p1stTbl->p2ndhtbls[idx2ndTbl])) {
                // not allocated yet, allocate a new table
                if (!(phndtbl = CreateHandleTable (NUM_HNDL_2ND))) {
                    break;
                }
                p1stTbl->p2ndhtbls[idx2ndTbl] = phndtbl;
                phndtbl->wReuseCntBase = p1stTbl->wReuseCntBase;
                break;
            }

            // any room in this 2nd-level handle table?
            if (phndtbl->nFree) {
                break;
            }
        }

        if ((idx2ndTbl >= NUM_HNDL_TABLE) || !phndtbl) {
            // out of handles
            if (!phndtbl) {
                NKDbgPrintfW (L"HNDLAlloc failed, out of memory\r\n");
            } else {
                NKDbgPrintfW (L">>>>> Completely Out Of Handles <<<<<\r\n");
            }
            return NULL;
        }

        p1stTbl->idx2ndFree = (WORD) idx2ndTbl;
        idxBase = (idx2ndTbl+1) << HND_HD2ND_SHIFT;
    }

    // there is a free entry in the table pointed by phndtbl
    DEBUGCHK (phndtbl->nFree);

    if (phdAPISet = phd->pci->phdApiSet) {
        // user mode API set, increment ref count
        InterlockedIncrement (&phdAPISet->dwRefCnt);
    }

    // search for the free entry
    idx = phndtbl->idxFree;
    do {
        if (idx >= idxLimit) {
            idx = 0;
        }
        phde = &phndtbl->hde[idx ++];

    } while (phde->phd);

    // note: at loop exit, idx is at one past the newly allocated entry

    // setup the entry
    phde->phd = phd;
    phde->hndl.idx = (WORD) (idx - 1 + idxBase);

//#ifdef DEBUG
// Shouldn't affect perf too much adding a simple check. If we find this to cost
// perf too much, uncomment the #ifdef DEBUG.
    if (!phde->hndl.reuseCnt) {
        phde->hndl.reuseCnt = phndtbl->wReuseCntBase;
    }
//#endif
    phde->hndl.reuseCnt |= 3;       // lower 2 bit of a handle must be 0b11

    // update handle table bookkeeping values
    phndtbl->nFree --;
    phndtbl->idxFree = (WORD) idx;  // free set to next one (idx is current + 1)

    return phde->hndl.hValue;
}


//
// LockHandleData: Locking a handle prevent the HDATA from being destroyed
// when a handle is locked, the HDATA of the handle cannot be destroyed.
// NOTE: Locking a handle doesn't guarantee serialization
//
PHDATA LockHandleData (HANDLE h, PPROCESS pprc)
{
    PHDATA phd = NULL;
    PHNDLTABLE phndtbl = LockHandleTable (pprc);

    if (phndtbl) {
        if (phd = h2pHDATA (h, phndtbl)) {
            DEBUGCHK ((int) phd->dwRefCnt > 0);
            InterlockedIncrement (&phd->dwRefCnt);  // lock increment is 1, thus using InterlockedIncrement
        }
        UnlockHandleTable (pprc);
    }
    return phd;
}

// locking via PHDATA directly
void LockHDATA (PHDATA phd)
{
    InterlockedIncrement (&phd->dwRefCnt);
}


//
// lock a handle, possibly pseudo handle like GetCurrentProcess()/GetCurrentThread()
//
PHDATA LockHandleParam (HANDLE h, PPROCESS pprc)
{
    return (GetCurrentProcess () == h)
                ? LockHandleData (hActvProc, g_pprcNK)
                : ((GetCurrentThread () == h)
                        ? LockHandleData (hCurThrd, g_pprcNK)
                        : ((GetCurrentToken () == h)
                                ? LockThrdToken (pCurThread)
                                : LockHandleData (h, pprc)));
}


//
// DoLockHDATA: Increment the refcount by the amount specified
//
BOOL DoLockHDATA (PHDATA phd, DWORD dwAddend)
{
    if (phd->dwRefCnt < MAX_HANDLE_REF) {
        InterlockedExchangeAdd (&phd->dwRefCnt, dwAddend);
        return TRUE;
    }
    return FALSE;
}


//
// DoUnlockHDATA: finish using a handle, will decrement ref count. The object object will be destroyed if ref-count gets to 0
//
BOOL DoUnlockHDATA (PHDATA phd, DWORD dwAddend)
{
    BOOL fRet = TRUE;
    if (phd) {
        DWORD dwRefCnt;

        if (phd->pName) {
            // grab the name cs before decrementing the ref count, remove the HDATA from named list
            // as soon as handle count gets to 0, regardless of the lock count.
            EnterCriticalSection (&NameCS);
            dwRefCnt = InterlockedExchangeAdd (&phd->dwRefCnt, dwAddend);  // dwRefCnt = previous refcnt
            if (HNDLCNT_FROM_REF (dwRefCnt) && !HNDLCNT_FROM_REF (dwRefCnt + dwAddend)) {
                DEBUGCHK ((phd->dl.pBack != &phd->dl) && (phd->dl.pFwd != &phd->dl));  // check for double-remove
                RemoveDList (&phd->dl);
                InitDList (&phd->dl);  // point at self for safety against double-remove
            }
            LeaveCriticalSection (&NameCS);
            dwRefCnt += dwAddend;
        } else {
            dwRefCnt = InterlockedExchangeAdd (&phd->dwRefCnt, dwAddend) + dwAddend;
        }

        if (!dwRefCnt) {
            //
            // last reference to the HDATA object, destroy it
            //
            fRet = PHD_CloseHandle (phd);

            if (phd->psd) {
                FreeName (phd->psd);
            }
            if (phd->pName) {
                FreeName (phd->pName);
            }
            DEBUGMSG (ZONE_OBJDISP, (L"Freeing HDATA %8.8lx\r\n", phd));
            FreeMem (phd, HEAP_HDATA);

        // call PreClose function if this is a CloseHandle call and it's the last handle to the HDATA            
        } else if (!HNDLCNT_FROM_REF (dwRefCnt) && (-HNDLCNT_INCR == dwAddend)) {
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
// UnlockHandleData: finish using a handle, will decrement ref count. The object object will be destroyed if ref-count gets to 0
//
BOOL UnlockHandleData (PHDATA phd)
{
    return DoUnlockHDATA (phd, -LOCKCNT_INCR);
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
    } else if (!(dwId = phd->pci->dwServerId)) {
        dwId = g_pprcNK->dwId;
    }
    UnlockHandleData (phd);
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
    while (phdThread = InterlockedPopList (&g_phdDelayFreeThread)) {
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
        phd->dwData   = 0;
        phd->dwRefCnt = HNDLCNT_INCR;
        phd->psd      = phd->pName = NULL;
        phd->pci      = pci;
        phd->pvObj    = pvObj;
        DEBUGMSG (ZONE_OBJDISP, (L"Allocated HDATA %8.8lx, pci = %8.8lx, type = %8.8lx\r\n", phd, pci, pci->type));
    }

    return phd;
}

//
// HNDLDupWithHDATA - duplicate a handle with hdata, the handle *must* have been locked
//
HANDLE HNDLDupWithHDATA (PPROCESS pprc, PHDATA phd)
{
    HANDLE hndl = NULL;
    if (phd->dwRefCnt < MAX_HANDLE_REF) {
        PHNDLTABLE phndtbl = LockHandleTable (pprc);

        if (phndtbl) {
            
            if (!phndtbl->fClosing && (hndl = HNDLAlloc (phndtbl, phd))) {
                InterlockedExchangeAdd (&phd->dwRefCnt, HNDLCNT_INCR);
            }
            UnlockHandleTable (pprc);
        }
    }
    return hndl;
}

//
// HNDLCreateHandle - Create a handle, the process object had been locked.
//
HANDLE HNDLCreateHandle (PCCINFO pci, PVOID pvObj, PPROCESS pprc)
{
    PHDATA phd;
    HANDLE h = NULL;

    DEBUGMSG (ZONE_MEMORY, (L"+HNDLCreateHandle %8.8lx, %8.8lx %8.8lx\r\n", pci, pvObj, pprc));
    if (phd = AllocHData (pci, pvObj)) {
        PHNDLTABLE phndtbl = LockHandleTable (pprc);
        
        if (phndtbl) {

            if (!phndtbl->fClosing) {
                if ((&cinfAPISet == pci)
                    && (pprc != g_pprcNK)) {
                    // creating an API set
                    PAPISET pas = (PAPISET) pvObj;
                    pas->cinfo.phdApiSet = phd;
                    
                }
                h = HNDLAlloc (phndtbl, phd);
            }
            
            UnlockHandleTable (pprc);
        }

        if (!h) {
            DEBUGMSG (ZONE_OBJDISP, (L"Freeing HDATA %8.8lx\r\n", phd));
            FreeMem (phd, HEAP_HDATA);
        } else {
            CELOG_OpenOrCreateHandle(phd, TRUE);
        }
    }
    DEBUGMSG (ZONE_MEMORY, (L"-HNDLCreateHandle %8.8lx\r\n", h));
    return h;
}

//
// HNDLCreateNamedHandle - create a named handle, the process object is locked. In case already exist, return the 
//                         existing object and *pdwErr set to ERROR_ALREADY_EXIST
//
HANDLE HNDLCreateNamedHandle (PCCINFO pci, PVOID pvObj, PPROCESS pprc, LPSECURITY_ATTRIBUTES lpsa, PNAME pName, BOOL fCreate, LPDWORD pdwErr)
{
    PHDATA phd = NULL, phdwalk;
    HANDLE hRet = NULL;
    DWORD  dwErr = 0;
    PHNDLTABLE phndtbl;
    
    EnterCriticalSection (&NameCS);

    // iterate through the name list to find existing named handles
    for (phdwalk = (PHDATA) g_NameList.pFwd; (PHDATA) &g_NameList != phdwalk; phdwalk = (PHDATA) phdwalk->dl.pFwd) {
        if ((phdwalk->pci == pci) && !NKwcscmp (phdwalk->pName->name, pName->name)) {
            if (phdwalk->dwRefCnt >= MAX_HANDLE_REF) {
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
            } else {
                phd = phdwalk;
                InterlockedExchangeAdd (&phd->dwRefCnt, HNDLCNT_INCR);  // increment the ref count of the phd
                dwErr = ERROR_ALREADY_EXISTS;
                CELOG_OpenOrCreateHandle(phd, FALSE);
            }
            break;
        }
    }
    
    if (!dwErr) {

        if (!fCreate) {
            dwErr = ERROR_FILE_NOT_FOUND;
            
        } else {
        
            // the name doesn't exist, create one
            PNAME psd = NULL;
            if (!(phd = AllocHData (pci, pvObj))) {
                dwErr = ERROR_OUTOFMEMORY;
            } else if (lpsa) {
                dwErr = AllocPSD (lpsa, &psd);
            }

            if (!dwErr) {

                // created successfully, add to the global name list
                phd->pName = pName;
                phd->psd   = psd;
                AddToDListHead (&g_NameList, &phd->dl);
                CELOG_OpenOrCreateHandle(phd, TRUE);

            // clean up on error
            } else if (phd) {
                DEBUGMSG (ZONE_OBJDISP, (L"Freeing HDATA %8.8lx\r\n", phd));
                FreeMem (phd, HEAP_HDATA);
                phd = NULL;
            }
        }
    }
    LeaveCriticalSection (&NameCS);

    switch (dwErr) {
    case ERROR_ALREADY_EXISTS:
        if (phd->psd && !ACLCheck (PSDFromLPName (phd->psd))) {
            // existing named handle and access check failed
            dwErr = ERROR_ACCESS_DENIED;
            break;
        }
        
        // fall through to allocate handle entry
    case 0:
        // allocate an entry in the handle table
        if (phndtbl = LockHandleTable (pprc)) {
            if (phndtbl->fClosing || !(hRet = HNDLAlloc (phndtbl, phd))) {
                dwErr = ERROR_OUTOFMEMORY;
            }
            UnlockHandleTable (pprc);
        } else {
            // process exited???
            dwErr = ERROR_INVALID_PARAMETER;
            DEBUGCHK (0);
        }
        break;
    default:
        break;
    }

    *pdwErr = dwErr;

    if (!hRet) {
        DoUnlockHDATA (phd, -HNDLCNT_INCR);
    }

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
            PHNDLTABLE  phndtbl = p1sttbl;
            DWORD       idxLimit = 0;

            if (!idx2ndTbl --) {
                // 1st handle table
                idxLimit = NUM_HNDL_1ST;
                idx     -= MIN_HNDL;

            } else if (phndtbl = p1sttbl->p2ndhtbls[idx2ndTbl]) {
                // 2nd level handle table
                idxLimit = NUM_HNDL_2ND;
                idx      = IdxInHdtbl (idx);
            }

            if (idx < idxLimit) {
                PHNDLENTRY phde = &phndtbl->hde[idx];
                if (((HANDLE) ((DWORD)h|1) == phde->hndl.hValue) && (phd = phde->phd)) {
                    // valid handle, lock it. 
                    InterlockedIncrement (&phd->dwRefCnt);
                    
                    // update idx2ndFree in 1st level handle table
                    // NOTE: if on 1st level handle table, idx2ndTbl will be ((DWORD) -1),
                    //       and the following test will always fail
                    if (p1sttbl->idx2ndFree > idx2ndTbl) {
                        p1sttbl->idx2ndFree = (WORD) idx2ndTbl;
                    }

                    phndtbl->nFree ++;
                    phde->hndl.reuseCnt += 4; // increment reuse count by 4 (lower 2 bits remains the same)
                    phde->phd = NULL;
                }
            }

            UnlockHandleTable (pprc);
        }
    }
    return phd;
}

//
// HNDLCloseHandle - close a handle
//
BOOL HNDLCloseHandle (PPROCESS pprc, HANDLE h)
{
    PHDATA phd = HNDLZapHandle (pprc, h);

    DEBUGMSG (ZONE_OBJDISP, (L"Closing Handle h = %8.8lx, pprc id = %8.8lx\r\n", h, pprc->dwId));

    if (phd) {
        PHDATA phdAPISet = phd->pci->phdApiSet;
        DoUnlockHDATA (phd, -HNDLCNT_INCR);     // close the handle, can trigger pre-close
        DoUnlockHDATA (phd, -LOCKCNT_INCR);     // unlock the handle, can trigger close, and destroy the object

        // decrement ref-count to the API set
        DoUnlockHDATA (phdAPISet, -LOCKCNT_INCR);
    }

    return phd != NULL;

}


//
// HNDLCloseOneHndlTbl - get all handles to be closed in a handle table
//          returns the number of handle to be closed, phd in phd array.
//
static DWORD HNDLCloseOneHndlTbl (PHNDLTABLE phndtbl, PHDATA phds[], DWORD nMaxFree)
{
    DWORD nClosed = 0;
    DWORD idx;
    for (idx = 0; phndtbl->nFree < nMaxFree; idx ++) {
        DEBUGCHK (idx < nMaxFree);
        if (phds[nClosed] = phndtbl->hde[idx].phd) {
            // lock the handle
            InterlockedIncrement (&phds[nClosed]->dwRefCnt);
            nClosed ++;
            InterlockedIncrement (&phndtbl->nFree);
            phndtbl->hde[idx].phd = 0;
            phndtbl->hde[idx].hndl.reuseCnt += 4;   // increment reuse count by 4 (lower 2 bits remains the same)
        }
    }
    phndtbl->idxFree = 0;
    return nClosed;
}

//
// HNDLCloseAllHandles - close all handles of a process, the process object had been locked.
//
void HNDLCloseAllHandles (PPROCESS pprc)
{
    PHDATA phds[NUM_HNDL_2ND]; // max hanles per page table is NUM_HNDL_2ND
    PHDATA phdAPISet;
    DWORD  nHndls, idx, idx2nd = 0, nMaxFree = NUM_HNDL_1ST;
    PHNDLTABLE p1sttbl = pprc->phndtbl;
    PHNDLTABLE phndtbl = p1sttbl;

    do {

        // get all the handls to be closed in a handle table
        VERIFY (LockHandleTable (pprc));
        nHndls = HNDLCloseOneHndlTbl (phndtbl, phds, nMaxFree);
        p1sttbl->idx2ndFree = 0;
        p1sttbl->fClosing   = TRUE;
        UnlockHandleTable (pprc);

        // close the handles
        for (idx = 0; idx < nHndls; idx ++) {
            phdAPISet = phds[idx]->pci->phdApiSet;
            DEBUGCHK (phds[idx]->dwRefCnt >= HNDLCNT_INCR);
            DoUnlockHDATA (phds[idx], -HNDLCNT_INCR);    // close the handle, can trigger pre-close
            DoUnlockHDATA (phds[idx], -LOCKCNT_INCR);    // unlock the handle, can trigger close, and destroy the object

            // unlock API Set
            DoUnlockHDATA (phdAPISet, -LOCKCNT_INCR);
        }

        phndtbl = p1sttbl->p2ndhtbls[idx2nd ++];
        nMaxFree = NUM_HNDL_2ND;
        
    } while (phndtbl && (idx2nd <= NUM_HNDL_TABLE));

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
        PHNDLTABLE  phndtbl = NULL;
        HANDLE      hndl = NULL;
        DWORD       dwRefCnt = phd->dwRefCnt;
        
        if ((phndtbl = LockHandleTable (pprcDst))
            && !phndtbl->fClosing
            && (hndl = HNDLAlloc (phndtbl, phd))) {

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
            while ((dwRefCnt < MAX_HANDLE_REF) && HNDLCNT_FROM_REF (dwRefCnt)) {
                if (InterlockedCompareExchange (&phd->dwRefCnt, dwRefCnt+HNDLCNT_INCR, dwRefCnt) == dwRefCnt) {
                    dwErr = 0;
                    break;
                }
                dwRefCnt = phd->dwRefCnt;
            }
        }

        if (!dwErr) {
            *pDstHndl = hndl;
            
        } else if (hndl) {
            // failed due to ref-count overflow, or the last handle had already closed (still has lock on it).
            // error code: ERROR_NOT_ENOUGH_MEMORY if ref-count overflow
            //             ERROR_INVALID_HANDLE if last handle already closed.
            HNDLZapHandle (pprcDst, hndl);

            if (dwRefCnt >= MAX_HANDLE_REF) {
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
            }
                
        } else {
            // handle table full
            dwErr = ERROR_NO_MORE_USER_HANDLES;
        }

        if (phndtbl) {
            UnlockHandleTable (pprcDst);
        }
    
    }
    UnlockHandleData (phd);

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
    DWORD    dwErr;
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
        
    } else if (!(dwErr = HNDLDuplicate (pprcSrc, hSrcHndl, pprcDst, &hDst))
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
    

