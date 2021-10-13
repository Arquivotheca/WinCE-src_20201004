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
#include <windows.h>
#include <toolhelp.h>
#include "heap.h"
#include "../../NK/kernel/thgrow.c"

extern BOOL g_fInitDone;

// mapping from RHE_XXX flags to LF32_XXX flags
const DWORD flagMap [] = { 
    LF32_FREE,
    LF32_FIXED,
    LF32_BIGBLOCK
};

typedef struct {
    THSNAP *pSnap;
    DWORD   dwProcessId;
    DWORD   dwHeapId;
    DWORD   dwItemCnt;
} EnumHeapStruct, *PEnumHeapStruct;


BOOL EnumFillHeapItem (LPBYTE pMem, DWORD cbSize, DWORD dwFlags, LPVOID pEnumData)
{
    PEnumHeapStruct pehs = (PEnumHeapStruct) pEnumData;
    PHEAPENTRY32 phitem = (PHEAPENTRY32) THGrow (pehs->pSnap, sizeof (HEAPENTRY32));

    if (phitem) {
        phitem->dwSize          = sizeof (HEAPENTRY32);
        phitem->dwAddress       = (DWORD) pMem;
        phitem->hHandle         = (HANDLE) pMem;
        phitem->dwBlockSize     = cbSize;
        phitem->dwResvd         = 0;
#if HEAP_SENTINELS
        if (RHE_FREE != dwFlags) {
            PRHEAP_SENTINEL_HEADER pHead = (PRHEAP_SENTINEL_HEADER) pMem;
            phitem->dwAddress            = (DWORD) pMem + HEAP_SENTINELS;
            phitem->dwBlockSize          = ((PRHEAP_SENTINEL_HEADER) pMem)->cbSize;
            phitem->dwResvd              = pHead->dwFrames[AllocPcIdx];
        }
#endif
        phitem->dwFlags         = flagMap[dwFlags];
        phitem->dwLockCount     = (RHE_FREE != dwFlags);
        phitem->th32HeapID      = pehs->dwHeapId;
        phitem->th32ProcessID   = pehs->dwProcessId;
        pehs->dwItemCnt ++;
    }

    return NULL != phitem;

}

DWORD WINAPI GetHeapSnapshot (PTHSNAP pSnap)
{
    DWORD dwProcessId = GetCurrentProcessId ();
    EnumHeapStruct ehs = {0};
    PRHEAP php;
    PTH32HEAPLIST phl32;
    DWORD cbTotalSize = 0;

    //
    // This could be called on a process for which heap may not be
    // initialized yet; need to check if coredll init is completed
    // before accessing the heap snapshot
    //
    if (!g_fInitDone) {
        return 0;
    }

    ehs.dwProcessId = dwProcessId;
    ehs.pSnap = pSnap;

    EnterCriticalSection (&g_csHeapList);

    php = (TH32CS_SNAPPROCESS & pSnap->dwFlags)? g_hProcessHeap : g_phpListAll;

    for ( ; php; php = php->phpNext) {
        if (NULL == (phl32 = (PTH32HEAPLIST) THGrow (pSnap, sizeof (TH32HEAPLIST)))) {
            break;
        }
        ehs.dwHeapId  = (DWORD) php;
        ehs.dwItemCnt = 0;

        if (!EnumerateHeapItems (php, EnumFillHeapItem, &ehs)) {
            break;
        }

        phl32->dwHeapEntryCnt           = ehs.dwItemCnt;
        phl32->dwTotalSize              = sizeof (TH32HEAPLIST) + ehs.dwItemCnt * sizeof (HEAPENTRY32);
        phl32->heaplist.dwSize          = sizeof (HEAPLIST32);
        phl32->heaplist.th32ProcessID   = dwProcessId;
        phl32->heaplist.th32HeapID      = (DWORD) php;
        phl32->heaplist.dwFlags         = (php->flOptions & HEAP_IS_PROC_HEAP) ? HF32_DEFAULT : 0;
        pSnap->dwHeapListCnt ++;
        
        cbTotalSize += phl32->dwTotalSize;
    }
    LeaveCriticalSection (&g_csHeapList);

    return cbTotalSize;
}

