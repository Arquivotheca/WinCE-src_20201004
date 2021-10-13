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
/* Adding or removing entries requires changes to the structures */
/* in heap.c */
#ifndef __NK_HEAP_H__
#define __NK_HEAP_H__

#define HEAP_MODLIST                0
#define HEAP_STKLIST                0
#define HEAP_LOCKPAGELIST           0
#define HEAP_VIEWCNTOFMAPINPROC     0
#define HEAP_ALIAS                  0
#define HEAP_STATICMAPPING_ENTRY    0
#define HEAP_PSLSERVER              0
#define HEAP_CELOGLOCKEDEVENT       0

#define HEAP_SIZE0                  sizeof(MODULELIST)
#define HEAP_NAME0                  "ModList/STKLIST/LockList/Alias/CeLogEvent"

// heap 0 - small items (<= 16 bytes)
ERRFALSE(sizeof(MODULELIST) <= 16);
ERRFALSE(sizeof(STKLIST) <= sizeof(MODULELIST));
ERRFALSE(sizeof(LOCKPAGELIST) <= sizeof (MODULELIST));
ERRFALSE(sizeof(DWORD) < sizeof (MODULELIST));
ERRFALSE(sizeof(ViewCount_t) <= sizeof (MODULELIST));
ERRFALSE(sizeof(ALIAS) <= sizeof (MODULELIST));
ERRFALSE(sizeof(STATICMAPPINGENTRY) <= sizeof(MODULELIST));
ERRFALSE(sizeof(PSLSERVER) <= sizeof(MODULELIST));

// heap 1
#define HEAP_KERNELMOD              1
#define HEAP_APISET                 1
#define HEAP_HDATA                  1
#define HEAP_PROXY                  1
#define HEAP_WATCHDOG               1
#define HEAP_TIMERENTRY             1

#define HEAP_SIZE1                  sizeof(APISET)
#define HEAP_NAME1                  "API/Prxy/HData/KMod/Wdog/TokList"

ERRFALSE(sizeof(HDATA) <= sizeof(APISET));
ERRFALSE(sizeof(PROXY) <= sizeof(APISET));
ERRFALSE(sizeof(WatchDog) <= sizeof(APISET));

// heap 2
#define HEAP_MAPVIEW                2
#define HEAP_CELOGDLLINFO           2
#define HEAP_SIZE2                  sizeof(MAPVIEW)
#define HEAP_NAME2                  "MapView/CeLogDLL"

// heap 3
#define HEAP_SEMAPHORE              3
#define HEAP_EVENT                  3
#define HEAP_MUTEX                  3
#define HEAP_SYNC_OBJ               3
#define HEAP_PAGEPOOL               3
#define HEAP_FSMAP                  3
#define HEAP_LOCKED_PROCESS_LIST    3
#define HEAP_PROC_READYQ            3

#define HEAP_SIZE3                  sizeof(MUTEX)
#define HEAP_NAME3                  "Crit/Evt/Sem/Mut/PgPool/FSMap/LockPrcList"

#define HEAP_SIZE_SYNC_OBJ          HEAP_SIZE3
#define HEAP_SIZE_LOCKPROCLIST      HEAP_SIZE3

ERRFALSE(HEAP_SYNC_OBJ == HEAP_SEMAPHORE);
ERRFALSE(HEAP_SYNC_OBJ == HEAP_EVENT);
ERRFALSE(HEAP_SYNC_OBJ == HEAP_MUTEX);
ERRFALSE(sizeof(EVENT) <= sizeof(MUTEX));
ERRFALSE(sizeof(SEMAPHORE) <= sizeof(MUTEX));
ERRFALSE(sizeof(PagePool_t) <= sizeof(MUTEX));
ERRFALSE(sizeof(FSMAP) <= sizeof(MUTEX));
// sizeof(LockedProcessList_t) is verified in mapfile.c

// heap 4
#define HEAP_MODULE                 4
#define HEAP_STUBTHREAD             4

#define HEAP_SIZE4                  sizeof(MODULE)
#define HEAP_NAME4                  "Module/StubThrd"

// heap 5
#define HEAP_PROCESS                5
#define HEAP_SIZE5                  sizeof(PROCESS)
#define HEAP_NAME5                  "Process"

// heap 6
#define HEAP_THREAD                 6
#define HEAP_SIZE6                  sizeof(THREAD)
#define HEAP_NAME6                  "Thrd"

// heap 7 - maximum allowable allocation from AllocMem
#define HEAP_MAXALLOC               7
#define HEAP_SIZE7                  1024
#define HEAP_NAME7                  "Maximum"

// heap size must be ordered
ERRFALSE (HEAP_SIZE1 < HEAP_SIZE2);
ERRFALSE (HEAP_SIZE2 < HEAP_SIZE3);
ERRFALSE (HEAP_SIZE3 < HEAP_SIZE4);
ERRFALSE (HEAP_SIZE4 < HEAP_SIZE5);
//ERRFALSE (HEAP_SIZE5 < HEAP_SIZE6);
ERRFALSE (HEAP_SIZE6 < HEAP_SIZE7);

#define NUMARENAS       8   /* Should be one higher than the last #define */

typedef struct heapptr_t {
    ushort size;    // size of blocks in this page
    ushort wPad;
    const char *classname;
    LPBYTE fptr;    // pointer to first free block
    long cUsed;     // # of entries in use
    long cMax;      // maximum # of entries used
} heapptr_t;

#if defined (MIPS) || defined (SH4)
#define IsValidKPtr(p)      (IsDwordAligned(p) && IsStaticMappedAddress((DWORD)p))
#else
extern LPBYTE g_pKHeapBase;
extern LPBYTE g_pCurKHeapFree;

#define IsValidKPtr(p)  ( \
           IsDwordAligned (p) \
        && ((char *)(p) >= (char *)g_pKHeapBase)   \
        && ((char *)(p) <  (char *)g_pCurKHeapFree)    \
        )
#endif

//
// function prototypes
//

// Initialize the kernel heap
void HeapInit (void);

// Allocate a block from the kernel heap
LPVOID AllocMem (ulong poolnum);

// Free a block from the kernel heap
VOID FreeMem (LPVOID pMem, ulong poolnum);

// allocate a variable length block for kernel heap
PNAME AllocName (DWORD dwLen);

// free a variable length block allocated with AllocName
__inline VOID FreeName (PNAME lpn)
{
    FreeMem (lpn, lpn->wPool);
}

// get kernel heap information
BOOL GetKHeapInfo (DWORD idx, LPVOID pBuf, DWORD cbBuf, LPDWORD pcbRet);

#ifdef DEBUG
int ShowKernelHeapInfo (void);
#endif


//---------------------------------------------------------
// generic heap support
//

// initialize generic heap support
BOOL InitGenericHeap (HANDLE hKCoreDll);

// creating a generic heap (variable sized)
HANDLE NKCreateHeap (void);
LPVOID NKHeapAlloc (HANDLE hHeap, DWORD cbSize);
BOOL   NKHeapFree (HANDLE hHeap, LPVOID ptr);
BOOL   NKHeapCompact (HANDLE hHeap);

LPVOID NKmalloc (DWORD cbSize);
BOOL   NKfree (LPVOID ptr);
BOOL   NKcompact (void);  // compact the generic kernel heap


#endif  // __NK_HEAP_H__
