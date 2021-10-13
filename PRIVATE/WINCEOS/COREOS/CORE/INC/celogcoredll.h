//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <celog.h>
#define MAX_STACK_FRAME 20


//----------------------------------------------------------
_inline void CELOG_HeapCreate(DWORD dwOptions, DWORD dwInitSize, DWORD dwMaxSize, HANDLE hHeap) {
    CEL_HEAP_CREATE cl;

    cl.dwOptions = dwOptions;
    cl.dwInitSize = dwInitSize;
    cl.dwMaxSize = dwMaxSize;
    cl.hHeap = hHeap;
    cl.dwPID = GetCurrentProcessId();
    cl.dwTID = GetCurrentThreadId();

    CELOGDATA(TRUE, CELID_HEAP_CREATE, &cl, sizeof(CEL_HEAP_CREATE), 0, CELZONE_HEAP | CELZONE_MEMTRACKING);
}

//----------------------------------------------------------
_inline void CELOG_HeapAlloc(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes, DWORD lpMem) {
    
    if (lpMem) {
        BYTE pTmp[sizeof(CEL_HEAP_ALLOC) + sizeof(StackSnapshot) + MAX_STACK_FRAME*sizeof(CallSnapshot)];
        PCEL_HEAP_ALLOC pcl = (PCEL_HEAP_ALLOC) pTmp;
        StackSnapshot *pStack = (StackSnapshot*) &pcl->adwStackTrace[0];

        pcl->hHeap   = hHeap;
        pcl->dwFlags = dwFlags;
        pcl->dwBytes = dwBytes;
        pcl->lpMem   = lpMem;
        pcl->dwTID = GetCurrentThreadId();
        pcl->dwPID = GetCurrentProcessId();
        pcl->dwCallerPID = (DWORD)GetCallerProcess();

        pStack->wVersion = 1;
        pStack->wNumCalls = (WORD) GetCallStackSnapshot (MAX_STACK_FRAME, pStack->rgCalls, 0, 0);

        CELOGDATA(TRUE, CELID_HEAP_ALLOC, pcl,
                  (WORD)(sizeof(CEL_HEAP_ALLOC) + sizeof(StackSnapshot) + pStack->wNumCalls*sizeof(CallSnapshot)),
                  0, CELZONE_HEAP | CELZONE_MEMTRACKING);
    }
}

//----------------------------------------------------------
_inline void CELOG_HeapRealloc(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes, DWORD lpMemOld, DWORD lpMem) {
    
    if (lpMem) {
        BYTE pTmp[sizeof(CEL_HEAP_REALLOC) + sizeof(StackSnapshot) + MAX_STACK_FRAME*sizeof(CallSnapshot)];
        PCEL_HEAP_REALLOC pcl = (PCEL_HEAP_REALLOC) pTmp;
        StackSnapshot *pStack = (StackSnapshot*) &pcl->adwStackTrace[0];

        pcl->hHeap = hHeap;
        pcl->dwFlags = dwFlags;
        pcl->dwBytes = dwBytes;
        pcl->lpMemOld = lpMemOld;
        pcl->lpMem = lpMem;
        pcl->dwTID = GetCurrentThreadId();
        pcl->dwPID = GetCurrentProcessId();
        pcl->dwCallerPID = (DWORD)GetCallerProcess();

        pStack->wVersion = 1;
        pStack->wNumCalls = (WORD) GetCallStackSnapshot (MAX_STACK_FRAME, pStack->rgCalls, 0, 0);

        CELOGDATA(TRUE, CELID_HEAP_REALLOC, pcl,
                  (WORD)(sizeof(CEL_HEAP_REALLOC) + sizeof(StackSnapshot) + pStack->wNumCalls*sizeof(CallSnapshot)),
                  0, CELZONE_HEAP | CELZONE_MEMTRACKING);
    }
}

//----------------------------------------------------------
_inline void CELOG_HeapFree(HANDLE hHeap, DWORD dwFlags, DWORD lpMem) {
    BYTE pTmp[sizeof(CEL_HEAP_FREE) + sizeof(StackSnapshot) + MAX_STACK_FRAME*sizeof(CallSnapshot)];
    PCEL_HEAP_FREE pcl = (PCEL_HEAP_FREE) pTmp;
    StackSnapshot *pStack = (StackSnapshot*) &pcl->adwStackTrace[0];

    pcl->hHeap = hHeap;
    pcl->dwFlags = dwFlags;
    pcl->lpMem = lpMem;
    pcl->dwTID = GetCurrentThreadId();
    pcl->dwPID = GetCurrentProcessId();
    pcl->dwCallerPID = (DWORD)GetCallerProcess();
    
    pStack->wVersion = 1;
    pStack->wNumCalls = (WORD) GetCallStackSnapshot (MAX_STACK_FRAME, pStack->rgCalls, 0, 0);

    CELOGDATA(TRUE, CELID_HEAP_FREE, pcl,
              (WORD)(sizeof(CEL_HEAP_FREE) + sizeof(StackSnapshot) + pStack->wNumCalls*sizeof(CallSnapshot)),
              0, CELZONE_HEAP | CELZONE_MEMTRACKING);
}

//----------------------------------------------------------
_inline void CELOG_HeapDestroy(HANDLE hHeap) {
    
    CEL_HEAP_DESTROY cl;

    cl.hHeap = hHeap;
    cl.dwPID = GetCurrentProcessId();
    cl.dwTID = GetCurrentThreadId();

    CELOGDATA(TRUE, CELID_HEAP_DESTROY, &cl, sizeof(CEL_HEAP_DESTROY), 0, CELZONE_HEAP | CELZONE_MEMTRACKING);
}

