//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/* Adding or removing entries requires changes to the structures */
/* in heap.c */

#if defined(x86) || defined(MIPS)
#define HELPER_STACK_SIZE 2048
#else
#define HELPER_STACK_SIZE 1024
#endif

#define HEAP_THREAD     0
#define HEAP_SIZE0      sizeof(THREAD)
#define HEAP_NAME0      "Thrd"

#define HEAP_MODULE     1
#define HEAP_ROMINFO    1
#define HEAP_SIZE1      sizeof(MODULE)
#define HEAP_NAME1      "Mod"

ERRFALSE(sizeof(KERNELMOD) <= sizeof(PROXY));
ERRFALSE(sizeof(STUBEVENT) <= sizeof(PROXY));
ERRFALSE(sizeof(CLEANEVENT) <= sizeof(PROXY));
ERRFALSE(sizeof(HDATA) <= sizeof(PROXY));
ERRFALSE(sizeof(APISET) <= sizeof(PROXY));

#ifdef x86
ERRFALSE(sizeof(PROXY) <= sizeof(CALLSTACK));
#else
ERRFALSE(sizeof(CALLSTACK) <= sizeof(PROXY));
#endif

#define HEAP_KERNELMOD  2
#define HEAP_CALLSTACK  2
#define HEAP_CLEANEVENT 2
#define HEAP_STUBEVENT  2
#define HEAP_APISET     2
#define HEAP_HDATA      2
#define HEAP_PROXY      2
#define HEAP_CELOGDLLINFO 2
#ifdef x86
#define HEAP_SIZE2      sizeof(CALLSTACK)
#else
#define HEAP_SIZE2      sizeof(PROXY)
#endif
#define HEAP_NAME2      "API/CStk/ClnEvt/StbEvt/Prxy/HData/KMod"

ERRFALSE(sizeof(THRDDBG) <= sizeof(MUTEX));
ERRFALSE(sizeof(EVENT) <= sizeof(MUTEX));
ERRFALSE(sizeof(SEMAPHORE) <= sizeof(MUTEX));
ERRFALSE(sizeof(CRIT) <= sizeof(MUTEX));

#define HEAP_SEMAPHORE  3
#define HEAP_EVENT      3
#define HEAP_MUTEX      3
#define HEAP_CRIT       3
#define HEAP_THREADDBG  3
#define HEAP_SYNC_OBJ   3
#define HEAP_SIZE3      sizeof(MUTEX)
#define HEAP_NAME3      "Crit/Evt/Sem/Mut/ThrdDbg"

ERRFALSE(HEAP_SYNC_OBJ == HEAP_SEMAPHORE);
ERRFALSE(HEAP_SYNC_OBJ == HEAP_EVENT);
ERRFALSE(HEAP_SYNC_OBJ == HEAP_MUTEX);
ERRFALSE(HEAP_SYNC_OBJ == HEAP_CRIT);
#define HEAP_SIZE_SYNC_OBJ      HEAP_SIZE3

ERRFALSE(sizeof(FSMAP) <= sizeof(FULLREF));
ERRFALSE(sizeof(THREADTIME) <= sizeof(FULLREF));

#define HEAP_FSMAP      4
#define HEAP_THREADTIME 4
#define HEAP_FULLREF    4
#define HEAP_WATCHDOG   4
#define HEAP_SIZE4      sizeof(FULLREF)
#define HEAP_NAME4      "FullRef/FSMap/ThrdTm"

#define HEAP_MEMBLOCK   5
#define HEAP_SIZE5      sizeof(MEMBLOCK)
#define HEAP_NAME5      "MemBlock"

#define HEAP_NAME       6
#define HEAP_SIZE6      sizeof(Name)
#define HEAP_NAME6      "Name"

#if HARDWARE_PT_PER_PROC
#define HEAP_MODLIST    HEAP_MEMBLOCK
ERRFALSE(sizeof(MODULELIST) <= sizeof(MEMBLOCK));
#else
#define HEAP_MODLIST    HEAP_PROXY
ERRFALSE(sizeof(MODULELIST) <= sizeof(PROXY));
#endif

#define HEAP_TOKLIST    HEAP_MODLIST

#if PAGE_SIZE == 4096
#define HEAP_HLPRSTK    7
#define HEAP_SIZE7      HELPER_STACK_SIZE
#define HEAP_NAME7      "HlprStk"
#endif

#if PAGE_SIZE == 4096
#define NUMARENAS       8   /* Should be one higher than the last #define */
#else
#define NUMARENAS       7   /* Should be one higher than the last #define */
#endif

typedef struct heapptr_t {
    ushort size;    // size of blocks in this page
    ushort wPad;
    const char *classname;
    LPBYTE fptr;    // pointer to first free block
    long cUsed;     // # of entries in use
    long cMax;      // maximum # of entries used
} heapptr_t;

