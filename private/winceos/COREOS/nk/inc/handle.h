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
//    handle.h - internal kernel handle header
//

#ifndef _NK_HANDLE_H_
#define _NK_HANDLE_H_

#include "kerncmn.h"
#include "apicall.h"
#include "vm.h"

//------------------------------------------------------------------------
//
// handle constants
//
#define MAX_HNDL                0x10000     // max 64K handles per-proc
#define NUM_HNDL_TABLE          ((MAX_HNDL/NUM_ENTRY_PER_TABLE) - 1)        // -1 for 1st level table

#define MIN_HNDL                ((NUM_HNDL_TABLE+1)/2)                      // minimum handle index value

#define NUM_ENTRY_PER_TABLE     (VM_PAGE_SIZE/sizeof(HNDLENTRY))
#define NUM_HNDL_2ND            (NUM_ENTRY_PER_TABLE - 1)
#define NUM_HNDL_1ST            (NUM_HNDL_2ND - MIN_HNDL)

#define INVALID_HNDL_INDEX      (MAX_HNDL - 1)


//------------------------------------------------------------------------
//
// handle types
//

// handle
typedef struct _KHNDL {
    union {
        HANDLE      hValue;                 // handle value
        struct {
            WORD    reuseCnt;               // reuse count
            WORD    idx;                    // index to handle table
        };
    };
} KHNDL, *PKHNDL;

// HDATA object (one per object)
struct _HDATA {
    DLIST           dl;                     // doubly linked list. Used only for named handle, MUST BE 1st in struct
    PCCINFO         pci;                    // handle server information
    LPVOID          pvObj;                  // pointer to the real object
    DWORD           dwRefCnt;               // total ref count
    DWORD           dwData;                 // per-object data
    PNAME           pName;                  // Name of the object
    DWORD           dwAccessMask;           // access bits which specify what rights are given for this handle object
    PHDATA          phdPolicy;              // security policy associated with this object (valid only for named handle)
};

// handle entry (one per handle)
typedef struct _HNDLENTRY {
    KHNDL           hndl;
    PHDATA          phd;
} HNDLENTRY, *PHNDLENTRY;



// handle table
struct _HNDLTABLE {
    WORD            idxFirstFree;                               // idx to the 1st free entry (free list for entries)
    WORD            idxLastFree;                                // idx to the last free entry (free list for entries)
    WORD            idxNextTable;                               // idx to the next able with free entry (free list of tables)
    WORD            wReuseCntBase;                              // base of reuse count. to distinguish between processes for ease of debugging
    HNDLENTRY       hde[NUM_HNDL_1ST];                          // entry in both 1st/2nd level table
    union {
        struct {
            PHNDLTABLE  p2ndhtbls[NUM_HNDL_TABLE];              // ptrs to 2nd-level tables
            WORD        n2ndUsed;                               // # of 2nd level handle tables used
            WORD        idxLastTable;                           // idx to last table with free entry (valid only if idxNextTable is valid)
        };
        HNDLENTRY       __hde[NUM_HNDL_2ND-NUM_HNDL_1ST];       // the extra handle entries for 2nd level table
    };
};

ERRFALSE(sizeof(HNDLENTRY) == 8);
ERRFALSE(VM_PAGE_SIZE == sizeof(HNDLTABLE));
ERRFALSE(MIN_HNDL >= ((NUM_HNDL_TABLE+1)/2));
ERRFALSE(NUM_ENTRY_PER_TABLE == 512);
ERRFALSE(NUM_HNDL_TABLE < INVALID_HNDL_INDEX);


#define HNDLCNT_INCR                0x10000         // handle count is in the upper word
#define LOCKCNT_INCR                0x1             // lock count is in the lower word

#define HND_IDX_INVALID     0x1ff
#define HND_IDX_MASK        0x1ff   // lower 9 bits
#define HND_HD2ND_SHIFT     9

__inline DWORD Idx2ndHdtbl (DWORD idx)
{
    DEBUGCHK (idx < MAX_HNDL);
    return idx >> HND_HD2ND_SHIFT;    // idx/512
}

__inline DWORD IdxInHdtbl (DWORD idx)
{
    return idx & HND_IDX_MASK;
}

//------------------------------------------------------------------------
//
// function prototypes
//

// start using a handle, will increment refcnt
PHDATA LockHandleData (HANDLE h, PPROCESS pprc);

// advanced version of lock, with dwAddend to be the number of ref to be added (positive number)
BOOL DoLockHDATA (PHDATA phd, DWORD dwAddend);

// advanced version of unlock, with dwAddend to be the number of ref to be removed (negative number)
BOOL DoUnlockHDATA (PHDATA phd, LONG lAddend);

// finish using a handle, will decrement ref count. The handle object will be destroyed if ref-count gets to 0
FORCEINLINE BOOL UnlockHandleData (PHDATA phd)
{
    return DoUnlockHDATA (phd, -LOCKCNT_INCR);
}


// locking via PHDATA directly
void LockHDATA (PHDATA phd);

// create/close a handle
HANDLE HNDLCreateHandle (PCCINFO pci, PVOID pvObj, PPROCESS pprc, DWORD dwAccessMask);
HANDLE HNDLCreateNamedHandle (PCCINFO pci, PVOID pvObj, PPROCESS pprc, DWORD dwRequestedAccess, PNAME pName, DWORD dwObjectType, BOOL fCreate, LPDWORD pdwErr);
BOOL HNDLCloseHandle (PPROCESS pprc, HANDLE h);
//
// HNDLZapHandle - remove a handle from the handle table, lock the hdata, and return the hdata of the handle
//
PHDATA HNDLZapHandle (PPROCESS pprc, HANDLE h);

// duplicate a handle
BOOL NKDuplicateHandle (
    HANDLE  hSrcProc,           // source process of the handle to be duplicated
    HANDLE  hSrcHndl,           // source handle
    HANDLE  hDstProc,           // target process of the handle to be duplicated
    LPHANDLE lpDstHndl,         // (OUT) the result handle
    DWORD   dwAccess,           // desired access (ignored)
    BOOL    bInherit,           // inherit (must be false)
    DWORD   dwOptions           // options (only DUPLICAT_SAME_ACCESS and DUPLICATE_CLOSE SOURCE supported)
);

// get object pointer from HDATA
__inline LPVOID GetObjPtr (PHDATA phd)
{
    return phd? phd->pvObj : NULL;
}

// get object type from HDATA
__inline DWORD GetObjType (const HDATA *phd)
{
    return phd? phd->pci->type : 0;
}

// get object pointer by type
__inline LPVOID GetObjPtrByType (PHDATA phd, DWORD type)
{
    return (phd && (phd->pci->type == type))? phd->pvObj : NULL;
}

// get process pointer is a little different, need to check if the process exited
PPROCESS GetProcPtr (PHDATA phd);

#define GetEventPtr(phd)        ((PEVENT) GetObjPtrByType(phd, HT_EVENT))
#define GetMutexPtr(phd)        ((PMUTEX) GetObjPtrByType(phd, HT_MUTEX))
#define GetSemPtr(phd)          ((PSEMAPHORE) GetObjPtrByType(phd, HT_SEMAPHORE))
#define GetMapFilePtr(phd)      ((PFSMAP) GetObjPtrByType(phd, HT_FSMAP))
#define GetAPISetPtr(phd)       ((PAPISET) GetObjPtrByType(phd, HT_APISET))

//
// HNDLGetUserInfo - retrieve the data field of HDATA
//
DWORD HNDLGetUserInfo (HANDLE h, PPROCESS pprc);
//
// HNDLSetUserInfo - update the data field of HDATA
//
BOOL HNDLSetUserInfo (HANDLE h, DWORD dwData, PPROCESS pprc);
//
// HNDLSetObjPtr - update the pvObj field of HDATA
//
BOOL HNDLSetObjPtr (HANDLE h, LPVOID pvObj, PPROCESS pprc);

//
// HNDLGetServerId - get the server process id of a handle
//
DWORD HNDLGetServerId (HANDLE h);
//
// HNDLDuplicate - worker function to duplicate a handle
//      return error code
//
DWORD HNDLDuplicate (PPROCESS pprcSrc, HANDLE hSrc, PPROCESS pprcDst, PHANDLE pDstHndl);

//
// HNDLDupWithHDATA - duplicate a handle with hdata
//
HANDLE HNDLDupWithHDATA (PPROCESS pprc, PHDATA phd, LPDWORD pdwErr);

// HNDLCloseAllHandles - close all handles of a process
void HNDLCloseAllHandles (PPROCESS pprc);

//
// create a handle table
//  1st-level (master) table - p1stTbl == NULL
//  2nd-level table          - p1stTbl == master handle table
//
PHNDLTABLE CreateHandleTable (PHNDLTABLE p1stTbl);

//
// HNDLDeleteHandleTable - final clean for handle table, delete the handle table allocated
//
void HNDLDeleteHandleTable (PPROCESS pprc);

extern PHNDLTABLE g_phndtblNK;


//
// functions that can only be called inside KCall
//
// NOTE: The returned pointer is only valid inside the same KCall
//
PHDATA h2pHDATA (HANDLE h, PHNDLTABLE phndtbl);

__inline PPROCESS KC_ProcFromId (DWORD dwThId)
{
    return GetProcPtr (h2pHDATA ((HANDLE) dwThId, g_phndtblNK));
}

//
// For a fully exited thread - add the "thread-id" to the delayed free list
//
void KC_AddDelayFreeThread (PHDATA phdThread);

//
// clean up all the threads in the delayed free list
//
void CleanDelayFreeThread (void);

//
// get actual handle count, not including lock count, from pdata
//
DWORD GetHandleCountFromHDATA (PHDATA phd);

//
// lock a pseudo handle like GetCurrentProcess()/GetCurrentThread()
//
PHDATA LockPseudoHandle (HANDLE h, PPROCESS pprc);

//
// lock a handle, possibly pseudo handle like GetCurrentProcess()/GetCurrentThread()
//
FORCEINLINE PHDATA LockHandleParam (HANDLE h, PPROCESS pprc)
{
    return ((DWORD) h > 0x10000) ? LockHandleData (h, pprc) : LockPseudoHandle (h, pprc);
}


#endif  // _NK_HANDLE_H_

