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
/* Adding or removing entries requires changes to the structures */
/* in heap.c */
#ifndef __NK_HEAP_H__
#define __NK_HEAP_H__

#if defined(x86) || defined(MIPS)
#define HELPER_STACK_SIZE 2048
#else
#define HELPER_STACK_SIZE 1024
#endif

#define HEAP_THREAD     0
#define HEAP_SIZE0      sizeof(THREAD)
#define HEAP_NAME0      "Thrd"

#define HEAP_MAPVIEW    1
#define HEAP_SIZE1      sizeof(MAPVIEW)
#define HEAP_NAME1      "MapView"

ERRFALSE(sizeof(HDATA) <= sizeof(PROXY));
ERRFALSE(sizeof(APISET) <= sizeof(PROXY));
ERRFALSE(sizeof(WatchDog) <= sizeof(PROXY));


#define HEAP_KERNELMOD  2
#define HEAP_APISET     2
#define HEAP_HDATA      2
#define HEAP_PROXY      2
#define HEAP_CELOGDLLINFO 2
#define HEAP_WATCHDOG   2
#define HEAP_TIMERENTRY 2

#define HEAP_SIZE2      sizeof(PROXY)
#define HEAP_NAME2      "API/CStk/Prxy/HData/KMod/Celog/Wdog"

ERRFALSE(sizeof(EVENT) <= sizeof(MUTEX));
ERRFALSE(sizeof(SEMAPHORE) <= sizeof(MUTEX));
ERRFALSE(sizeof(CRIT) <= sizeof(MUTEX));
ERRFALSE(sizeof(MODULE) <= sizeof(MUTEX));
ERRFALSE(sizeof(PagePool_t) <= sizeof(MUTEX));
ERRFALSE(sizeof(FSMAP) <= sizeof(MUTEX));
// sizeof(LockedProcessList_t) is verified in mapfile.c

#define HEAP_SEMAPHORE  3
#define HEAP_EVENT      3
#define HEAP_MUTEX      3
#define HEAP_CRIT       3
#define HEAP_SYNC_OBJ   3
#define HEAP_MODULE     3
#define HEAP_PAGEPOOL   3
#define HEAP_FSMAP      3
#define HEAP_LOCKED_PROCESS_LIST 3
#define HEAP_SIZE3      sizeof(MUTEX)
#define HEAP_NAME3      "Crit/Evt/Sem/Mut/Module/PgPool/FSMap/LockPrcList"

ERRFALSE(HEAP_SYNC_OBJ == HEAP_SEMAPHORE);
ERRFALSE(HEAP_SYNC_OBJ == HEAP_EVENT);
ERRFALSE(HEAP_SYNC_OBJ == HEAP_MUTEX);
ERRFALSE(HEAP_SYNC_OBJ == HEAP_CRIT);
#define HEAP_SIZE_SYNC_OBJ      HEAP_SIZE3

#define HEAP_SIZE_LOCKPROCLIST  HEAP_SIZE3

#define HEAP_PROCESS    4
#define HEAP_SIZE4      sizeof(PROCESS)
#define HEAP_NAME4      "Process"

#define HEAP_NAME       5
#define HEAP_SIZE5      sizeof(NAME)
#define HEAP_NAME5      "Name"

#define HEAP_HLPRSTK    6
#define HEAP_SIZE6      HELPER_STACK_SIZE
#define HEAP_NAME6      "HlprStk"


// heap 7 - small items (<= 16 bytes)
ERRFALSE(sizeof(CLEANEVENT) <= 16);
ERRFALSE(sizeof(STUBEVENT) <= sizeof(CLEANEVENT));
ERRFALSE(sizeof(CLEANEVENT) <= sizeof(CLEANEVENT));
ERRFALSE(sizeof(MODULELIST) <= sizeof(CLEANEVENT));
ERRFALSE(sizeof(TOKENLIST) <= sizeof(CLEANEVENT));
ERRFALSE(sizeof(STKLIST) <= sizeof(CLEANEVENT));
ERRFALSE(sizeof(LOCKPAGELIST) <= sizeof (CLEANEVENT));
ERRFALSE(sizeof(DWORD) < sizeof (CLEANEVENT));
ERRFALSE(sizeof(ViewCount_t) <= sizeof (CLEANEVENT));
ERRFALSE(sizeof(ALIAS) <= sizeof (CLEANEVENT));
ERRFALSE(sizeof(STATICMAPPINGENTRY) <= sizeof(CLEANEVENT));

#define HEAP_CLEANEVENT 7
#define HEAP_STUBEVENT  7
#define HEAP_MODLIST    7
#define HEAP_TOKLIST    7
#define HEAP_STKLIST    7
#define HEAP_LOCKPAGELIST 7
#define HEAP_VIEWCNTOFMAPINPROC 7
#define HEAP_ALIAS      7
#define HEAP_STATICMAPPING_ENTRY 7

#define HEAP_SIZE7      sizeof(CLEANEVENT)
#define HEAP_NAME7      "cleanEvt/StubEvt/ModList/TokList/STKLIST/LockList/Alias"


#define NUMARENAS       8   /* Should be one higher than the last #define */

typedef struct heapptr_t {
    ushort size;    // size of blocks in this page
    ushort wPad;
    const char *classname;
    LPBYTE fptr;    // pointer to first free block
    long cUsed;     // # of entries in use
    long cMax;      // maximum # of entries used
} heapptr_t;

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
