//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _HEAP_H
#define _HEAP_H

#define CE_FIXED_HEAP_MAXSIZE       0x0002f000  // each heap has at most 188Kbytes
#define CE_VALLOC_MINSIZE           0x00018000  // allocations of 96K or more are VirtualAlloc's

#define HEAPSIG         0x50616548UL
#define FREESIGHEAD     0x7f362a4bUL
#define ALLOCSIGHEAD    0xa9e4b6f1UL
#define HOLESIG         0x36a9b64bUL
#define TAILSIG         0xe47f2a4bUL

// Internal: cannot conflict with any HEAP_XXX flags in winnt.h
#define HEAP_IS_PROC_HEAP       0x00002000
#define HEAP_IS_SHARED          0x00004000

#define REGION_MAX_IS_ESTIMATE      0x00000001  // if the bit is set in cbMaxFree, it's an estimate

//#ifdef DEBUG
#ifndef SHIP_BUILD
#define HEAP_SENTINELS 4
#else
#define HEAP_SENTINELS 0
#endif

#define ALIGNBYTES  16      // all heap allocations are multiples of 16 bytes
#define ALIGNSIZE(sz)   (((sz)+ALIGNBYTES-1) & ~(ALIGNBYTES-1))

typedef struct heap heap, *pheap;
typedef struct region region, *pregion;
typedef struct item item, *pitem;
typedef struct vaitem vaitem, *pvaitem;

// Heap structures:
//
//  These structures contain the data for a heap that are required to perform
// allocations, etc. and to identify all of the items owned by the heap.
//
//  A heap consists of a list of regions which contain the heap items and
// a list of VirtualAlloc'ed items which were too large to allocate from
// a region.

struct region {
    pitem   pitFree;                // 00: ptr to next available allocation
    pregion prgnLast;               // 04: ptr to last region (only valid for the original region)
    int     cbMaxFree;              // 08: size of largest free item in this region
    pitem   pitLast;                // 0C: last free item proceeding end-marker (or end-marker itself)
    pheap   phpOwner;               // 10: ptr to heap this region is part of
    pregion prgnNext;               // 14: ptr to next region in the heap
    DWORD   dwRgnData;              // 18: data for allocator
    DWORD   pad;                    // 1c: padding
};                                  // 20: size

struct heap {
    DWORD   dwSig;                  // 00: heap signature "HeaP"
    pvaitem pvaList;                // 04: list of VirtualAlloc'ed items
    pheap   phpNext;                // 08: next heap in linked list of all heaps
    WORD    flOptions;              // 0C: options from HeapCreate
#ifdef MEMTRACKING
    WORD    wMemType;               // 0E: registered memory type
#else
    WORD    wPad;                   // 0E: padding to maintain alignment
#endif
    DWORD   cbMaximum;              // 10: maximum size of this heap (0 == growable)
    PFN_AllocHeapMem pfnAlloc;      // 14: allocator
    PFN_FreeHeapMem pfnFree;        // 18: de-allocator
    CRITICAL_SECTION cs;            // 1c: critical section controlling this heap.
    region  rgn;                    // 30: first memory region in this heap
};

// Item structure:
//  There are 4 types of items in a region:
//      busy items: size > 0 and prgn points to the region the item belongs to
//      free items: size < 0
//      hole items: size > 0 and prgn == NULL
//      end maker: size == 0 and cbTail is the # of bytes beyond the tail
//
//  and a last item type for items which are too large to allocate from
//  a region.  These are VirtualAlloc'ed blocks:
//      virtual item: size > 0 and size is odd.

struct item {
    int         size;       // size of item (including header and trailer)
    union {
        pregion prgn;       // ptr to region item is contained in
        pheap   php;        // OR ptr to heap for VirtualAlloc'ed items
        int     cbTail;     // OR # of bytes left in region beyond the tail
    };
#if HEAP_SENTINELS
    int         cbTrueSize; // actual requested size in bytes
    DWORD       dwSig;      // item header guard bytes
#endif
};

struct vaitem {
    pvaitem pvaFwd;         // ptr to next VA'ed item in heap
    pvaitem pvaBak;         // ptr to previous VA'ed item
    DWORD   dwData;         // data for allocator
    DWORD   pad;            // padding
    item    it;             // item info
};

// The size of a heap header structure:
#define SIZE_HEAP_HEAD      (ALIGNSIZE(sizeof(heap)+sizeof(item)) - sizeof(item))

// and the size of a region header structure:
#define SIZE_REGION_HEAD    (ALIGNSIZE(sizeof(region) + sizeof(item)) - sizeof(item))

// Macro to compute the address of the first item within a region.  The items within a
// region are aligned so that the data contained in the item will be aligned according
// to the ALIGNBYTES macro.
#define FIRSTITEM(prgn) ((pitem)(ALIGNSIZE((DWORD)(prgn) + sizeof(region) + sizeof(item)) - sizeof(item)))

// Current process's heap for LocalAlloc, etc.
extern HANDLE hProcessHeap;

UINT WINAPI _HeapDump(HLOCAL hHeap);

#endif // _HEAP_H

