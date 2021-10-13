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

//------------------------------------------------------------------------------
//
//  Module Name:  
//  
//      pceperf.h
//  
//  Abstract:  
//
//      Private definitions used for implementing Windows CE Performance
//      Measurement API.
//      
//------------------------------------------------------------------------------


#ifndef _PCEPERF_H_
#define _PCEPERF_H_

#include <ceperf_cpu.h>


//------------------------------------------------------------------------------
// NT COMPATIBILITY
//------------------------------------------------------------------------------

#ifndef UNDER_CE

#include <stdio.h>
#define NKDbgPrintfW                 wprintf
#define DEBUGMSG(cond, printf_exp)   ((cond) ? (wprintf printf_exp),1 : 0)
#define RETAILMSG(cond, printf_exp)  ((cond) ? (wprintf printf_exp),1 : 0)
#define DEBUGCHK(cond)               (!(cond) ? DebugBreak(),1 : 0)
#define CeLogData(T,I,D,L,Y,Z,F,G)   DEBUGCHK(0)

#endif // UNDER_CE


//------------------------------------------------------------------------------
// STRUCTURES AND FLAGS
//------------------------------------------------------------------------------

// Status
#define VALID_RECORDING_STATUS_FLAGS                                           \
    (CEPERF_STATUS_RECORDING_ENABLED | CEPERF_STATUS_RECORDING_DISABLED)
#define VALID_STORAGE_STATUS_FLAGS                                             \
    (CEPERF_STATUS_STORAGE_ENABLED | CEPERF_STATUS_STORAGE_DISABLED)
#define VALID_STATUS_FLAGS                                                     \
    (VALID_RECORDING_STATUS_FLAGS | VALID_STORAGE_STATUS_FLAGS                 \
     | CEPERF_STATUS_NOT_THREAD_SAFE)

// Storage
#define VALID_STORAGE_LOCATION_FLAGS                                           \
    (CEPERF_STORE_FILE | CEPERF_STORE_CELOG                                    \
     | CEPERF_STORE_DEBUGOUT | CEPERF_STORE_REGISTRY)
#define VALID_STORAGE_TYPE_FLAGS                                               \
    (CEPERF_STORE_BINARY | CEPERF_STORE_TEXT | CEPERF_STORE_CSV)
#define VALID_STORAGE_OVERFLOW_FLAGS                                           \
    (CEPERF_STORE_OLDEST)
#define VALID_STORAGE_FLAGS                                                    \
    (VALID_STORAGE_LOCATION_FLAGS | VALID_STORAGE_TYPE_FLAGS                   \
     | VALID_STORAGE_OVERFLOW_FLAGS)

// Recording
#define VALID_COMMON_RECORDING_FLAGS                                           \
    (CEPERF_RECORD_ABSOLUTE_TICKCOUNT | CEPERF_RECORD_THREAD_TICKCOUNT         \
     | CEPERF_RECORD_ABSOLUTE_PERFCOUNT                                        \
     | CEPERF_RECORD_ABSOLUTE_CPUPERFCTR)
#define VALID_DURATION_RECORDING_FLAGS                                         \
    (CEPERF_DURATION_RECORD_NONE | CEPERF_DURATION_RECORD_MIN                  \
     | CEPERF_DURATION_RECORD_FULL)
#define VALID_DURATION_RECORDING_SEMANTIC_FLAGS                                \
    (CEPERF_DURATION_RECORD_UNLIMITED | CEPERF_DURATION_RECORD_SHARED          \
     | CEPERF_DURATION_RECORD_TTRACKER | CEPERF_DURATION_RECORD_UNIQUE         \
     | VALID_DURATION_UNIQUE_REPLACEMENT_FLAGS)
#define VALID_DURATION_UNIQUE_REPLACEMENT_FLAGS                                \
    (CEPERF_DURATION_REPLACE_BEGIN)
#define VALID_STATISTIC_RECORDING_FLAGS                                        \
    (CEPERF_STATISTIC_RECORD_NONE | CEPERF_STATISTIC_RECORD_MIN                \
     | CEPERF_STATISTIC_RECORD_SHORT | CEPERF_STATISTIC_RECORD_FULL)
#define VALID_LOCALSTATISTIC_RECORDING_FLAGS                                   \
    (CEPERF_LOCALSTATISTIC_RECORD_NONE | CEPERF_LOCALSTATISTIC_RECORD_MIN)

// Control
#define VALID_CONTROL_FLAGS                                                    \
    (CEPERF_CONTROL_HIERARCHY)

// Flush
#define VALID_FLUSH_FLAGS                                                      \
    (CEPERF_FLUSH_HIERARCHY | CEPERF_FLUSH_DESCRIPTORS | CEPERF_FLUSH_DATA     \
     | CEPERF_FLUSH_AND_CLEAR)

// CPU
#define VALID_CPU_STATUS_FLAGS                                                 \
    (CEPERF_CPU_ENABLE | CEPERF_CPU_DISABLE)
#define VALID_CPU_RECORDING_FLAGS                                              \
    (CEPERF_CPU_TLB_MISSES | CEPERF_CPU_ICACHE_MISSES | CEPERF_CPU_ICACHE_HITS \
     | CEPERF_CPU_DCACHE_MISSES | CEPERF_CPU_DCACHE_HITS                       \
     | CEPERF_CPU_INSTRUCTION_COUNT | CEPERF_CPU_CYCLE_COUNT)


// DataPageHeader is used to chain together pages of data into long arrays.
// The actual type of data on the page can vary.  (Used for tracked items,
// DurationDataLists, Statistic deltas.)  Free items on the page are chained
// together into a linked list.
typedef struct _DataPageHeader {
    DWORD  offsetNextPage;              // Linked list of pages full of items via offsets
    WORD   wFirstFree;                  // Index of first free item on the page
    WORD   wStartIndex;                 // Index of first item in overall array
};


// Master --> SessionHeader --> ItemPage
//        --> SessionHeader --> ItemPage --> ItemPage --> ItemPage
//        --> SessionHeader --> ItemPage

// ItemPageHeader is the header for each page containing lists of tracked items
// for the same session.  Tracked items are referred to via their indexes into
// the array, so pages in the middle of the list cannot be deallocated even if
// they do not contain any used data.
// (BUGBUG improvement would be to use wStartIndex to remove that restriction.)
//
// A tracked item on the page is free if TRACKEDITEM_ISFREE(pItem) is TRUE.
typedef struct _DataPageHeader ItemPageHeader;
// Followed by the array of tracked items...

#define ITEMPAGE_PFIRSTITEM(pHeader)                                           \
    ((TrackedItem*) (((LPBYTE)(pHeader)) + sizeof(ItemPageHeader)))


// ListPageHeader is the header for each page containing lists of fixed-size
// objects, across all sessions.  All of these objects are referred to by
// pointer, so whenever a page becomes empty it can be deallocated.
// (BUGBUG improvement could be to compact the list after a bulk deregister.)
//
// Every list has a DLLListFuncs struct used to operate on the list.
typedef struct _DataPageHeader ListPageHeader;
// Followed by the array of fixed-size objects...

#define LISTPAGE_PFIRSTOBJECT(pHeader)                                         \
    (((LPBYTE)(pHeader)) + sizeof(ListPageHeader))


// These functions are used to perform operations on a list.  This struct is
// replicated in each instance of the CePerf DLL.
typedef struct _DLLListFuncs {
    BOOL (*pIsItemFree)      (LPBYTE pItem);    // Test if the item is free
    WORD (*pGetNextFreeItem) (LPBYTE pItem);    // For a free item, get index (on current page) of next item in freelist
    VOID (*pFreeItem)        (LPBYTE pItem, WORD wNextFreeIndex);   // Mark an item as being free
} DLLListFuncs;

#define MAX_LIST_ITEMS_PER_PAGE(wItemSize)  ((WORD) (((DWORD)PGSIZE - sizeof(ListPageHeader)) / (wItemSize)))


// Logger lock abstraction
// BUGBUG Just a DWORD for now but probably needs to contain a handle list?
typedef struct _LoggerLock {
    DWORD  dwLock;
} LoggerLock;


// Useful to have settings in a separate struct so that they can be passed
// around by pointer.
typedef struct _SessionSettings {
    DWORD  dwStatusFlags;
    DWORD  dwStorageFlags;
    WCHAR  szStoragePath[MAX_PATH];
    DWORD  rgdwRecordingFlags[CEPERF_NUMBER_OF_TYPES];
} SessionSettings;  // sizeof(SessionSettings) = 540


// Session header is on the first page, along with the beginning of the list of objects.
typedef struct _SessionHeader {
    WCHAR szSessionName[MAX_PATH];  // Full path name of session
    DWORD dwRefCount;               // Total # handles open in all processes
    SessionSettings settings;       // Current settings
    LoggerLock lock;                // Logger lock for this session; used to
                                    // block loggers during session cleanup,
                                    // item deregistration, setting changes
    DWORD dwNumObjects;             // Total number of tracked items; may not
                                    // all be on one page.  Does not include
                                    // items that are allocated but not in use.
    ItemPageHeader items;           // Head of list of pages full of items
} SessionHeader;  // sizeof(SessionHeader) = 1080
// Followed by the array of tracked items...


// Assumed page size...  It doesn't actually matter whether pages are really
// this size, it just makes a convenient minimum unit
#define PGSIZE                      (4096)

// Maximum number of items that can be tracked per session
#define MAX_ITEMS_ON_FIRST_PAGE     ((WORD) ((PGSIZE - sizeof(SessionHeader)) / sizeof(TrackedItem)))  // =34
#define MAX_ITEMS_ON_OTHER_PAGES    ((WORD) ((PGSIZE - sizeof(ItemPageHeader)) / sizeof(TrackedItem))) // =46


#define MASTER_MUTEX_NAME           TEXT("CePerfMasterMutex")

#define MASTER_MAP_NAME             TEXT("CePerfMasterMap")

// The memory-mapped file is a fixed size right now
#define CEPERF_MAP_SIZE             (4*1024*1024)
// Number of DWORDs required to track 1 bit for every page
#define PAGE_MASK_LEN               ((CEPERF_MAP_SIZE / (PGSIZE)) / 32)  // 32=number of pages that can be tracked in one DWORD

// Rough heaps created by chaining together linked lists of data pages
typedef struct {
    WORD  wDurBLSize;               // Size of each item in the DurBL list, in bytes
    WORD  wDurDLSize;               // Size of each item in the DurDL list, in bytes
    DWORD offsetDurBLHead;          // offset of list of DurationBeginList pages
    DWORD offsetDurDLHead;          // offset of list of DurationDataList pages
    DWORD offsetStatDataHead;       // offset of list of StatisticData pages for tracking
                                    // Statistic changes (constant size)
} MasterListInfo;

// Info about CPU perf counters, to be stored in master map
typedef struct {
    WORD  wNumCounters;             // Number of perf counters, also used to
                                    // detect whether counters are active
                                    // (0 = inactive)
    WORD  wTotalCounterSize;        // Total size of perf counter data, in bytes,
                                    // including any padding needed to bring it
                                    // up to an 8-byte boundary (sizeof(ULARGE_INTEGER))
                                    // (Redundant with rgbCounterSize but used for speed)
                                    // Cannot exceed CEPERF_MAX_CPU_DATA.
    BYTE  rgbCounterSize[CEPERF_MAX_CPUCOUNTER_COUNT];  // Array of sizes of each perf
                                    // counter, in bytes
} MasterCPUSettings;


// Maximum number of sessions that can be open at one time (10 bits max)
// Defined to keep sizeof(CePerfMaster) == 1 page
#define MAX_SESSION_COUNT                                                      \
    ((PGSIZE - sizeof(DWORD) - 2*sizeof(WORD)                                  \
      - sizeof(MasterListInfo)                                                 \
      - sizeof(MasterCPUSettings)                                              \
      - PAGE_MASK_LEN*sizeof(DWORD))                                           \
     / sizeof(DWORD))  // =981


// The CePerf master page contains information about all active sessions.  It
// is created when the very first session is opened and destroyed when the very
// last session is closed.
//
// The session table is designed to make session lookup by handle fast.  It is
// just an array of session references plus the index of the first free pointer.
// The bottom bit of each reference says whether it is allocated.  If allocated,
// the reference is simply a pointer to the session data.  If free, the 
// reference is the index of the next free session.  So the free session 
// references are a linked list.  If there are no free sessions, the first-free
// index will be MAX_SESSION_COUNT.  Each session reference (allocated or free) 
// also has a 4-bit version counter that helps avoid use of stale session
// handles.
//
// The memory-mapped file is 4MB of reserved memory.  The page table keeps 
// track of which of those pages are committed.  Pages are only committed when
// they are needed, and are decommitted when they are not in use.  There is
// currently no way to compact the pages that are in use, so it is possible for
// the VM to become fragmented.  The only way to recover from such fragmentation
// would be to close all open session handles.

// Master flags for tracking global status
#define MASTER_CPUPERFCTR_ENABLED   0x00000001  // CPU perf counters currently enabled
#define MASTER_TTRACKER_FAILURE     0x00000002  // Don't keep trying to reload TTracker DLL if it fails once

typedef struct _CePerfMaster {
    DWORD dwGlobalFlags;    // Flags used to track global status
    
    // Table for mapping hSession -> pSession
    WORD  wFirstFree;       // First index in the linked list of free handles
    WORD  wMaxAlloc;        // Highest non-free index, to shorten scans of the whole table
    DWORD rgSessionRef[MAX_SESSION_COUNT];  // Array of session references
    
    MasterListInfo list;    // Heaps for Duration and Statistic data
    MasterCPUSettings CPU;  // Info about CPU perf counters
    
    // Table of page allocations
    DWORD rgdwPageMask[PAGE_MASK_LEN];  // 1 bit per page: 1=committed & in use,
                                        // 0=reserved only
} CePerfMaster;

C_ASSERT(sizeof(CePerfMaster) == PGSIZE);  // Compiler error if not page-aligned


// SESSION REFERENCES:
//   FREE SESSION:
//      10 bits of next free index
//      17 bits reserved
//      4 bits of session version counter
//      1 bit indicating free (1)
//   ALLOCATED SESSION:
//      20 bits of session offset - add to base address to make pointer
//      7 bits reserved
//      4 bits of session version counter
//      1 bit indicating allocated (0)

// Allocated session pointers are always 4k-aligned
#define MASTER_FREEBIT                              ((DWORD)0x00000001)
#define MASTER_ISFREE(dwSessionRef)                 ((BOOL)           ((dwSessionRef) & MASTER_FREEBIT))
#define MASTER_VERSION_SHIFT                        1
#define MASTER_VERSION(dwSessionRef)                ((WORD)           (((dwSessionRef) >> MASTER_VERSION_SHIFT) & 0x0000000F))
#define MASTER_SESSION_MASK                         ((DWORD)0xFFFFF000)
#define MASTER_NEXTFREE_SHIFT                       22
#define MASTER_NEXTFREE(dwFreeSessionRef)           ((WORD)           ((dwFreeSessionRef) >> MASTER_NEXTFREE_SHIFT))
#define MASTER_SESSIONPTR(dwAllocSessionRef)        ((SessionHeader*) ((DWORD)GET_MAP_POINTER(dwAllocSessionRef) & MASTER_SESSION_MASK))
#define MAKE_MASTER_ALLOCREF(wVersion, pSession)    ((DWORD)          ((((DWORD)((GET_MAP_OFFSET(pSession))) & MASTER_SESSION_MASK)     | (((DWORD)(wVersion)) << MASTER_VERSION_SHIFT))))
#define MAKE_MASTER_FREEREF(wVersion, wNextFree)    ((DWORD)          ((((DWORD)(wNextFree)) << MASTER_NEXTFREE_SHIFT) | (((DWORD)(wVersion)) << MASTER_VERSION_SHIFT)) | MASTER_FREEBIT)

// PAGE MASKING
// Index into the PageMask array for this page
#define MASTER_PAGEINDEX(iPage)                     ((DWORD) (((DWORD)(iPage)) / 32))
// Bitmask for the page within its DWORD
#define MASTER_PAGEBIT(iPage)                       ((DWORD) (1 << (((DWORD)(iPage)) % 32)))


// HANDLE OF SESSION:
//       4 bits of session version counter
//      10 bits of session index
//       2 bits reserved
//      16 bits reserved for item handle

#define SESSION_VERSION_SHIFT                       28
#define SESSION_INDEX_SHIFT                         18
#define SESSION_INDEX_MASK                          ((DWORD)0x000003FF)  // used after shifting
#define SESSION_VERSION(hSession)                   ((WORD)   (((DWORD)(hSession)) >> SESSION_VERSION_SHIFT))
#define SESSION_INDEX(hSession)                     ((WORD)   ((((DWORD)(hSession)) >> SESSION_INDEX_SHIFT) & SESSION_INDEX_MASK))
#define MAKE_SESSION_HANDLE(wVersion, wIndex)       ((HANDLE) ((((DWORD)(wVersion)) << SESSION_VERSION_SHIFT) | (((DWORD)(wIndex)) << SESSION_INDEX_SHIFT)))
#define SESSION_PFIRSTITEM(pSession)                                           \
    ((TrackedItem*) (((LPBYTE)(pSession)) + sizeof(SessionHeader)))


// Table of open session handles for one instance of a DLL; used for leak 
// cleanup on process exit
#define TABLE_COUNT 10
typedef struct DLLSessionTable {
    HANDLE rghSession[TABLE_COUNT];
    DWORD  rgdwRefCount[TABLE_COUNT];
    struct DLLSessionTable *pNext;
} DLLSessionTable;  // sizeof(DLLSessionTable) = 84


// Linked list of Internal pointers for each module that has loaded CePerf
// within one process (ie. list per instance of CePerf.dll)  There may be more
// than one module that has loaded CePerf within the same process.  The list is
// used to safely clean up those modules' references to CePerf.dll when
// CePerf.dll unloads at process exit.
typedef struct DLLInternalList {
    CePerfGlobals** ppGlobals;  // Caller's global pointer (&pCePerfInternal)
                                // Stored in order to clean up the caller's
                                // references to CePerf if CePerf.dll is unloaded
    CePerfGlobals*  pGlobals;   // Caller's data buffer (pCePerfInternal) which
                                // is assigned to the global pointer.  Stored
                                // in case the caller is unloaded before the
                                // buffer is freed, in which case the global
                                // pointer is in freed memory
    struct DLLInternalList* pNext;
} DLLInternalList;


// HANDLE OF TRACKED ITEM:
//      14 bits of session handle
//       2 bits reserved
//      16 bits of item index

#define ITEM_INDEX_MASK                             ((DWORD)0x0000FFFF)
#define ITEM_SESSION_HANDLE(hItem)                  ((HANDLE) (((DWORD)(hItem)) & ~ITEM_INDEX_MASK))
#define ITEM_INDEX(hItem)                           ((WORD)   (((DWORD)(hItem)) & ITEM_INDEX_MASK))
#define MAKE_ITEM_HANDLE(hSession, wIndex)          ((HANDLE) (((DWORD)(hSession)) | ((DWORD)(wIndex))))

// Maximum number of items that can be registered in one session at one time.
// (16 bits max)  Defined to stay within handle definition.
#define MAX_ITEM_COUNT              ((WORD) (((DWORD)1 << 16) - 1))


// Header for a list of DiscreteCounterData structs; since all 
// DiscreteCounterData entries are going to have the same value count,
// we store it only once.
typedef struct _DurationDataList {
    DWORD dwNumVals;            // Number of valid begin/end pairs recorded
} DurationDataList;  // sizeof(DurationDataList) = 4
// DURATION:
//   Followed by array of DiscreteCounterData structs, based on
//   recording flags for this item.  The counter data will be in the following
//   order:
//   1.  Absolute performance counter delta between begin/end, if recorded
//   2.  Thread run-time tick count delta between begin/end, if recorded
//   3.  Absolute tick count delta between begin/end, if recorded
//   4.  Array of CPU performance counter deltas between begin/end, if recorded.
//       The counters are based on the current CPU settings.  See the
//       definitions for the specific CPU for more details.
// STATISTIC:
//   Followed by a single DiscreteCounterData struct, for tracking the
//   changes that were made to the Statistic item.

#define DATALIST_FREEBIT                    ((DWORD)0x80000000)
#define DATALIST_NEXTFREEMASK               ((DWORD)0x0000FFFF)
#define DATALIST_ISFREE(pDL)                ((DWORD) (((DurationDataList*)(pDL))->dwNumVals & DATALIST_FREEBIT))
#define DATALIST_NEXTFREE(pDL)              ((WORD)  (((DurationDataList*)(pDL))->dwNumVals & DATALIST_NEXTFREEMASK))
#define DATALIST_MARKFREE(pDL, wNextFree)   ((WORD)  (((DurationDataList*)(pDL))->dwNumVals = (DATALIST_FREEBIT | (wNextFree))))
#define DATALIST_PDATA(pDL)                                                    \
    ((DiscreteCounterData*) (((LPBYTE)(pDL)) + sizeof(DurationDataList)))

// The number of counters we always leave room for: Absolute performance counter
// + thread run-time tick count + absolute tick count
#define NUM_BUILTIN_COUNTERS 3


// Counter values recorded when a Duration object is begun
typedef struct _DurationBeginCounters {
    HANDLE hThread;              // The thread that called CePerfBeginDuration,
                                 // or NULL if this struct contains no data.
    DWORD  dwTickCount;          // Tick count timestamp, if tracked
    LARGE_INTEGER liThreadTicks; // Thread run-time in ticks, if tracked
    LARGE_INTEGER liPerfCount;   // Perf count timestamp, if tracked
} DurationBeginCounters;
// Followed by the array of CPU counter data structs, as defined in 
// g_pMaster->rgbCounterSize, if the CPU counters are turned on (regardless of
// whether they are being tracked for this particular item).  They will be a
// total of g_pMaster->CPU.wTotalCounterSize bytes.
//
// The Duration's dwRecordingFlags say which counters are being tracked.
// Only those counters will be valid for an item, even though memory is always
// allocated for them.

C_ASSERT((sizeof(DurationBeginCounters) % sizeof(LARGE_INTEGER)) == 0);  // Compiler error if alignment problem

#define BEGINCOUNTERS_PCPU(pCounters)                                          \
    (((LPBYTE)(pCounters)) + sizeof(DurationBeginCounters))


// Maximum number of frames that can be open for one Duration object at one time
// (max # of begin calls without ends), whether due to nesting or multithreading
//
// Value of 3 chosen so that bFrameHigh+rghThread would fit into a 16-byte cache
// line, and so that following counters are aligned on a natural boundary.
#define CEPERF_DURATION_FRAME_LIMIT 3


// The BeginList is an array of "frames," where a frame is a snapshot of the 
// counters at the beginning of the Duration.  If Duration begin and end calls
// are never nested and never used by more than one thread at a time, then we
// would only need a single frame.  If begin/end calls are nested
// (begin1-begin2-end2-end1) the array becomes a stack.
// If begin/end calls are interleaved by two threads
// (begin1-begin2-end1-end2) then the two threads' entries will be interleaved 
// with each other but will always be in proper stacked order with respect to 
// other nested entries for the same thread.
//
// Un-shared and Shared Record Mode use the same algorithm by using
// INVALID_HANDLE_VALUE for all threads in shared mode.
//
// ALGORITHM TO FIND OPEN SLOT DURING BEGIN:  Loop bFrameHigh --> 0.
// Need to find first NULL slot after last entry for the current thread.
// Can't just take the first NULL slot because it must be in proper
// nested order with respect to other entries for this thread.
// 1. If encountered an entry for this thread, take the first NULL
//    slot after it.
//    * Limited record mode:  If no such NULL slot is available, fail to
//      record data.
//    * Unlimited record mode:  If no such NULL slot is available, allocate
//      a new DurationBeginList and chain on the head of the whole list.
// 2. If no entry for this thread is encountered, take the first NULL
//    slot.
//    * Limited record mode:  If no such NULL slot is available, fail to
//      record data.
//    * Unlimited record mode:  If no NULL slot is available, walk to pNext
//      and repeat this algorithm.  If hit an entry for this thread or hit
//      pNext=NULL, allocate a new DurationBeginList and chain on the
//      head of the whole list.
// (This algorithm is intended to keep the most recent Begins at the
// head of the list, balanced with the need to keep fragmentation
// under control.)
//
// ALGORITHM TO FIND MATCHING BEGIN SLOT DURING INTERMEDIATE/END:
// Loop bFrameHigh --> 0.  Stop at first entry for the current thread.
// * Limited record mode: if not found, record an error.
// * Unlimited record mode: if not found, jump to pNext and keep
//   looking.
//
typedef struct _DurationBeginList {
    BYTE   bFrameHigh;                  // High-water-mark on frames used, to
                                        // trim iterations when nesting & 
                                        // multithreading aren't happening
    BYTE   _bPad;                       // Padding
    WORD   _wPad;                       // Padding
    DWORD  offsetNextDBL;               // Next BeginList in the linked list,
                                        // if the Duration is in UNLIMITED mode.
} DurationBeginList;
// Followed by the array of DurationBeginCounters structs and their sets of CPU
// counters.  There will be CEPERF_DURATION_FRAME_LIMIT of them and they will
// each be (sizeof(DurationBeginCounters) + g_pMaster->CPU.wTotalCounterSize) bytes.

C_ASSERT((sizeof(DurationBeginList) % sizeof(LARGE_INTEGER)) == 0);  // Compiler error if alignment problem

#define BEGINLIST_FREEBIT                     ((WORD)0x8000)
#define BEGINLIST_NEXTFREEMASK                ((WORD)0x7FFF)
#define BEGINLIST_ISFREE(pBL)                 ((WORD) (((DurationBeginList*)(pBL))->_wPad & BEGINLIST_FREEBIT))
#define BEGINLIST_NEXTFREE(pBL)               ((WORD) (((DurationBeginList*)(pBL))->_wPad & BEGINLIST_NEXTFREEMASK))
#define BEGINLIST_MARKFREE(pBL, wNextFree)    ((WORD) (((DurationBeginList*)(pBL))->_wPad = (BEGINLIST_FREEBIT | (wNextFree))))
#define BEGINLIST_PCOUNTERS(pBL, bFrame, wTotalCounterSize)                    \
    ((DurationBeginCounters*)                                                  \
     (((LPBYTE)(pBL)) + sizeof(DurationBeginList)                              \
      + ((DWORD)(bFrame) * (sizeof(DurationBeginCounters) + wTotalCounterSize))))



// Used to track and report discrete data for a Duration, and change data for a
// Statistic item in short-record mode; useful for time plus other data like
// CPU perf counters.  Even though counters may vary in size from 1 to 8 bytes,
// we only use 8-byte values, to minimize overflow issues.
typedef struct {
    ULARGE_INTEGER ulTotal;     // Divide by number of values to get average value
                                // (Number of values is not stored in this struct)
    ULARGE_INTEGER ulMinVal;
    ULARGE_INTEGER ulMaxVal;
    LARGE_INTEGER liS;          // To get standard deviation, divide S by (n-1)
                                // and then take the square root
    LARGE_INTEGER liM;          // Don't use this directly, it's used to track S
} DiscreteCounterData;


// Statistic objects in short-record mode use this data to track value changes.
// They use a DiscreteCounterData "List" with only one list item,
// because the list header has the required change counter.  The list objects
// are always the same size.
typedef DurationDataList StatisticData;

#define STATDATA_ISFREE                     DATALIST_ISFREE
#define STATDATA_NEXTFREE                   DATALIST_NEXTFREE
#define STATDATA_MARKFREE                   DATALIST_MARKFREE
#define STATDATA_PDATA(offsetChangeData)    DATALIST_PDATA(GET_MAP_POINTER(offsetChangeData))
#define STATDATA_SIZE                       (sizeof(StatisticData) + sizeof(DiscreteCounterData))

#pragma pack(push,4)
typedef union _TypeData {

    struct {
        DWORD offsetBL;                 // Pointer to begin data in private RAM
        DWORD dwErrorCount;             // Number of errors encountered
        union {
            struct {
                DWORD dwReserved;       // UNUSED
            } FullRecord;
            struct {
                DWORD dwReserved;       // UNUSED
            } ShortRecord;
            struct {
                DWORD offsetDL;         // Array of DiscreteCounterData
                                        // structs in private RAM (one for each
                                        // type of data being kept)
            } MinRecord;
        };
    } Duration;

    struct {
        ULARGE_INTEGER ulValue;         // Current value
        union {
            struct {
                DWORD dwReserved;       // UNUSED
            } FullRecord;
            struct {
                DWORD offsetChangeData; // Pointer to StatisticData
                                        // struct containing data about changes,
                                        // in private RAM
            } ShortRecord;
            struct {
                DWORD dwReserved;       // UNUSED
            } MinRecord;
        };
    } Statistic;

    union {
        struct {
            LPVOID lpValue;             // Pointer to local statistic
            DWORD  dwValSize;           // Value size in bytes
            DWORD  ProcessId;           // Id of process the variable is inside
        } MinRecord;
    } LocalStatistic;

} TypeData;
#pragma pack(pop)

C_ASSERT(sizeof(TypeData) == 12);  // Compiler error if packing problem


// Header info stored for each tracked item
typedef struct _TrackedItem {
    WCHAR szItemName[CEPERF_MAX_ITEM_NAME_LEN];
    DWORD dwRefCount;
    CEPERF_ITEM_TYPE type;
    DWORD dwRecordingFlags;
    TypeData data;
} TrackedItem;  // sizeof(TrackedItem) = 88

C_ASSERT((sizeof(TrackedItem) % sizeof(ULARGE_INTEGER)) == 0);  // Compiler error if alignment problem

#define TRACKEDITEM_FREEBIT                     ((DWORD)0x80000000)
#define TRACKEDITEM_NEXTFREEMASK                ((WORD)0xFFFF)
#define TRACKEDITEM_ISFREE(pItem)               ( ((DWORD)   (((TrackedItem*)(pItem))->type)) & TRACKEDITEM_FREEBIT)
#define TRACKEDITEM_NEXTFREE(pItem)             ( ((WORD)    (((TrackedItem*)(pItem))->type)) & TRACKEDITEM_NEXTFREEMASK)
#define TRACKEDITEM_MARKFREE(pItem, wNextFree)  (*((DWORD*) &(((TrackedItem*)(pItem))->type)) = TRACKEDITEM_FREEBIT | (wNextFree))


// These values are not required but want to know if they change
C_ASSERT(MAX_ITEMS_ON_FIRST_PAGE  == 34);
C_ASSERT(MAX_ITEMS_ON_OTHER_PAGES == 46);


// Used during CePerfFlushSession to track current flush
typedef struct {
    SessionHeader* pSession;          // Session currently being flushed
    DWORD          dwFlushFlags;      // Flags for session flush
    HANDLE         hOutput;           // Open file or registry key for flushing to
    LPBYTE         lpTempBuffer;      // Scratch data buffer
    DWORD          cbTempBuffer;      // Size of scratch buffer, in bytes
} FlushState;

// Helper function prototype used with WalkAllItems
typedef HRESULT (*PFN_ItemFunction)    (HANDLE hTrackedItem, TrackedItem* pItem, LPVOID lpWalkParam);


//------------------------------------------------------------------------------
// SHARED FUNCTIONS
//------------------------------------------------------------------------------

LPVOID AllocPage();
VOID FreePage(LPVOID lpPage);
VOID CleanupProcess();
HRESULT DeleteAllItems(HANDLE hSession, SessionHeader* pSession);

// Lists are basically heaps for fixed-size objects
VOID FreeUnusedListPages(DLLListFuncs* pListFuncs, WORD wListItemSize, DWORD* poffsetListHeader);
VOID FreeAllListPages(DWORD* poffsetListHeader);
LPBYTE AllocListItem(DLLListFuncs* pListFuncs, WORD wListItemSize, DWORD * poffsetListHeader);
HRESULT FreeListItem(DLLListFuncs* pListFuncs, WORD wListItemSize, DWORD * poffsetListHeader, LPBYTE pItem, BOOL fReleasePages);

// Operations common to all logging functions
BOOL ITEM_PROLOG(SessionHeader** ppSession, TrackedItem** ppItem, HANDLE hTrackedItem, CEPERF_ITEM_TYPE type, HRESULT* phResult);
VOID ITEM_EPILOG(SessionHeader* pSession);

SessionHeader* LookupSessionHandle(HANDLE hSession);
TrackedItem* LookupItemHandle(SessionHeader* pSession, HANDLE hItem);

// Generic item functions
HRESULT WalkAllItems(HANDLE hSession, SessionHeader* pSession, PFN_ItemFunction pItemFunction, LPVOID lpWalkParam, BOOL fForce);
HRESULT Walk_InitializeTypeData(HANDLE hTrackedItem, TrackedItem* pItem, LPVOID lpWalkParam);
HRESULT Walk_ChangeTypeSettings(HANDLE hTrackedItem, TrackedItem* pItem, LPVOID lpWalkParam);

BOOL ValidateRecordingFlags(DWORD dwRecordingFlags, CEPERF_ITEM_TYPE type);

// Used during CePerfFlushSession
HRESULT FlushBytes(FlushState* pFlush, LPBYTE lpBuffer, DWORD dwBufSize);
HRESULT FlushChars(FlushState* pFlush, LPWSTR lpszFormat, ...);
HRESULT FlushULargeCSV(FlushState* pFlush, const ULARGE_INTEGER* pulValue);
HRESULT FlushDoubleCSV(FlushState* pFlush, double dValue);
HRESULT FlushItemDescriptorBinary(FlushState* pFlush, HANDLE hTrackedItem, TrackedItem* pItem, CEPERF_ITEM_TYPE type);

// Duration functions
BOOL ValidateDurationDescriptor(const CEPERF_BASIC_ITEM_DESCRIPTOR* pBasic, const CEPERF_EXT_ITEM_DESCRIPTOR* pExt, CEPERF_EXT_ITEM_DESCRIPTOR* pNewExt);
BOOL InitializeDurationData(HANDLE hTrackedItem, TrackedItem* pItem, const CEPERF_EXT_ITEM_DESCRIPTOR* pExt);
VOID FreeDurationData(TrackedItem* pItem, BOOL fReleasePages);
VOID FreeUnusedDurationPages();
HRESULT DurationFlushBegin(FlushState* pFlush);
HRESULT Walk_FlushDuration(HANDLE hTrackedItem, TrackedItem* pItem, LPVOID lpWalkParam);

// Statistic & LocalStatistic functions
BOOL ValidateStatisticDescriptor(const CEPERF_BASIC_ITEM_DESCRIPTOR* pBasic, const CEPERF_EXT_ITEM_DESCRIPTOR* pExt, CEPERF_EXT_ITEM_DESCRIPTOR* pNewExt);
BOOL CompareStatisticDescriptor(TrackedItem* pItem, const CEPERF_EXT_ITEM_DESCRIPTOR* pExt);
BOOL InitializeStatisticData(TrackedItem* pItem, const CEPERF_EXT_ITEM_DESCRIPTOR* pExt);
VOID FreeStatisticData(TrackedItem* pItem, BOOL fReleasePages);
VOID FreeUnusedStatisticPages();
HRESULT StatisticFlushBegin(FlushState* pFlush);
HRESULT Walk_FlushStatistic(HANDLE hTrackedItem, TrackedItem* pItem, LPVOID lpWalkParam);


#ifdef DEBUG
VOID DumpMemorySizes();
VOID DumpMasterInfo();
VOID DumpDLLSessionTable();
VOID DumpSession(HANDLE hSession, BOOL fDumpItems, BOOL fDumpData);
VOID DumpSessionItemList(HANDLE hSession, SessionHeader* pSession, BOOL fDumpData);
HRESULT DumpTrackedItem(HANDLE hTrackedItem, TrackedItem* pItem, LPVOID lpWalkParam);
VOID DumpDurationData(DiscreteCounterData* pData, DWORD dwNumVals);
VOID DumpCPUPerfCounters(LPBYTE lpData);
VOID DumpList(DLLListFuncs* pListFuncs, WORD wListItemSize, DWORD* poffsetListHeader);
#else
#define DumpMemorySizes()
#define DumpMasterInfo()
#define DumpDLLSessionTable()
#define DumpSession(hSession, fDumpItems, fDumpData)
#define DumpSessionItemList(hSession, pSession, fDumpData)
#define DumpTrackedItem(hTrackedItem, pItem, lpWalkParam)
#define DumpDurationData(pData, dwNumVals)
#define DumpCPUPerfCounters(pData)
#define DumpList(pListFuncs, wListItemSize, poffsetListHeader)
#endif // DEBUG


// NOTENOTE: Standard Deviation
// Donald Knuth's "The Art of Computer Programming, Volume 2: Seminumerical Algorithms", section 4.2.2.
// The solution is to compute mean and standard deviation using a recurrence relation, like this:
//
// M(1) = x(1), M(k) = M(k-1) + (x(k) - M(k-1)) / k
// S(1) = 0, S(k) = S(k-1) + (x(k) - M(k-1)) * (x(k) - M(k))
//
// for 2 <= k <= n, then
//
// variance = S(n) / (n - 1)
// std.dev = sqrt(variance)


// Duration update function is inline for fastest possible operation
_inline static VOID UpdateDurationDataUL(
    DiscreteCounterData* pData,
    const ULARGE_INTEGER* pulDelta,
    DWORD dwNumVals   // Has already been incremented to include the latest data
    )
{
    if (pulDelta->QuadPart < pData->ulMinVal.QuadPart) {
        pData->ulMinVal.QuadPart = pulDelta->QuadPart;
    }
    if (pulDelta->QuadPart > pData->ulMaxVal.QuadPart) {
        pData->ulMaxVal.QuadPart = pulDelta->QuadPart;
    }
    DEBUGCHK(pData->ulMaxVal.QuadPart >= pData->ulMinVal.QuadPart);  // sanity check
    
    pData->ulTotal.QuadPart += pulDelta->QuadPart;
    DEBUGCHK(pData->ulTotal.QuadPart >= pulDelta->QuadPart);  // detect overflow

    // Track standard deviation information
    if (dwNumVals > 1) {
        LARGE_INTEGER minus;  // MUST be signed because it can be negative
        
        // minus = x(k) - M(k-1)
        minus.QuadPart = pulDelta->QuadPart - pData->liM.QuadPart;

        // M(k) = M(k-1) + (x(k) - M(k-1)) / k
        pData->liM.QuadPart += (minus.QuadPart / dwNumVals);
        // S(k) = S(k-1) + (x(k) - M(k-1)) * (x(k) - M(k))
        pData->liS.QuadPart += (minus.QuadPart * (pulDelta->QuadPart - pData->liM.QuadPart));

    } else {
        // First value added
        pData->liM.QuadPart = pulDelta->QuadPart;   // M(1) = x(1)
        DEBUGCHK(0 == pData->liS.QuadPart);         // S(1) = 0
    }
}

_inline static VOID UpdateDurationDataDW(
    DiscreteCounterData* pData,
    const DWORD dwDelta,
    DWORD dwNumVals   // Has already been incremented to include the latest data
    )
{
    if (dwDelta < pData->ulMinVal.QuadPart) {
        pData->ulMinVal.QuadPart = dwDelta;
    }
    if (dwDelta > pData->ulMaxVal.QuadPart) {
        pData->ulMaxVal.QuadPart = dwDelta;
    }
    DEBUGCHK(pData->ulMaxVal.QuadPart >= pData->ulMinVal.QuadPart);  // sanity check
    
    pData->ulTotal.QuadPart += dwDelta;
    DEBUGCHK(pData->ulTotal.QuadPart >= dwDelta);  // detect overflow

    // Track standard deviation information
    if (dwNumVals > 1) {
        LARGE_INTEGER minus;  // MUST be signed because it can be negative
        
        // minus = x(k) - M(k-1)
        minus.QuadPart = dwDelta - pData->liM.QuadPart;

        // M(k) = M(k-1) + (x(k) - M(k-1)) / k
        pData->liM.QuadPart += (minus.QuadPart / dwNumVals);
        // S(k) = S(k-1) + (x(k) - M(k-1)) * (x(k) - M(k))
        pData->liS.QuadPart += (minus.QuadPart * (dwDelta - pData->liM.QuadPart));

    } else {
        // First value added
        pData->liM.QuadPart = dwDelta;              // M(1) = x(1)
        DEBUGCHK(0 == pData->liS.QuadPart);         // S(1) = 0
    }
}


_inline static double GetStandardDeviation(
    const DiscreteCounterData* pCounter,
    const DWORD dwNumVals
    )
{
    if (dwNumVals > 1) {
        // variance = S(n) / (n - 1)
        // std.dev = sqrt(variance)
        double variance = (double) (pCounter->liS.QuadPart / (dwNumVals - 1));
        return sqrt(variance);
    } else {
        return 0;
    }
}





// CONTROLLER LOCKING:  The controller lock is used to serialize session
// operations.  It does not affect Duration and Statistic operations, so
// data can still be logged while the controller lock is held.

#define AcquireControllerLock()   \
    WaitForSingleObject(g_hMasterMutex, INFINITE); \
    DEBUGMSG(ZONE_LOCK, (TEXT("+Controller Lock, hThread=0x%08x\r\n"), GetCurrentThreadId()));

#define ReleaseControllerLock()   \
    DEBUGMSG(ZONE_LOCK, (TEXT("-Controller Lock, hThread=0x%08x\r\n"), GetCurrentThreadId())); \
    ReleaseMutex(g_hMasterMutex);


// LOGGER LOCKING:  A logger lock is the same as the standard reader/writer
// lock concept.  Many loggers can own the logger lock at any time without
// affecting each other.  When a controller wants to take the logger lock, it
// must block until all the loggers release the lock.  While a controller owns
// the logger lock, all loggers fail to acquire it rather than block.
//
// There is no global logger lock; there is one lock per session.
// Controllers take the controller lock first, and then can control any session
// logger lock in turn while holding the controller lock.

BOOL AcquireLoggerLock(LoggerLock* pLock);
VOID ReleaseLoggerLock(LoggerLock* pLock);
VOID AcquireLoggerLockControl(LoggerLock* pLock);
VOID ReleaseLoggerLockControl(LoggerLock* pLock);


//------------------------------------------------------------------------------
// DEBUG ZONES
//------------------------------------------------------------------------------

#ifdef DEBUG

  #ifdef UNDER_CE
    #define ZONE_BREAK      0
    #define ZONE_API        DEBUGZONE(1)
    #define ZONE_LOCK       DEBUGZONE(2)
    #define ZONE_MEMORY     DEBUGZONE(3)
    #define ZONE_VERBOSE    DEBUGZONE(4)
    #define ZONE_SESSION    DEBUGZONE(8)
    #define ZONE_ITEM       DEBUGZONE(9)
    #define ZONE_DURATION   DEBUGZONE(10)
    #define ZONE_STATISTIC  DEBUGZONE(11)
    #define ZONE_ERROR      DEBUGZONE(15)
  #else // UNDER_CE
    extern DWORD g_dwDebugZones;
    #define ZONE_BREAK      (g_dwDebugZones & 0x00000001)
    #define ZONE_API        (g_dwDebugZones & 0x00000002)
    #define ZONE_LOCK       (g_dwDebugZones & 0x00000004)
    #define ZONE_MEMORY     (g_dwDebugZones & 0x00000008)
    #define ZONE_VERBOSE    (g_dwDebugZones & 0x00000010)
    #define ZONE_SESSION    (g_dwDebugZones & 0x00000100)
    #define ZONE_ITEM       (g_dwDebugZones & 0x00000200)
    #define ZONE_DURATION   (g_dwDebugZones & 0x00000400)
    #define ZONE_STATISTIC  (g_dwDebugZones & 0x00000800)
    #define ZONE_ERROR      (g_dwDebugZones & 0x00008000)
  #endif // UNDER_CE

#else // DEBUG
    
    #define ZONE_BREAK      0
    #define ZONE_API        0
    #define ZONE_LOCK       0
    #define ZONE_MEMORY     0
    #define ZONE_VERBOSE    0
    #define ZONE_SESSION    0
    #define ZONE_ITEM       0
    #define ZONE_DURATION   0
    #define ZONE_STATISTIC  0
    #define ZONE_ERROR      1

#endif // DEBUG

#define DEFAULT_ZONES   0x00008000  // ZONE_ERROR



//------------------------------------------------------------------------------
// DLL GLOBALS
//------------------------------------------------------------------------------

//
// Shared across all DLL instances
//

extern HANDLE g_hMasterMap;     // Handle of master map
extern CePerfMaster* g_pMaster; // Pointer to master map (size is CEPERF_MAP_SIZE)
extern HANDLE g_hMasterMutex;   // Handle of mutex protecting master map and
                                // session operations
extern WCHAR g_szDefaultFlushSubKey[MAX_PATH];                          

//
// Different for each DLL instance
//

extern DWORD g_dwDLLSessionCount;        // # sessions open with this DLL instance
extern DLLSessionTable* g_pDLLSessions;  // List of sessions open with this DLL instance


//------------------------------------------------------------------------------
// Convert between pointers and offsets
//------------------------------------------------------------------------------

__inline static PVOID GET_MAP_POINTER (DWORD dwOffset)
{
    if (dwOffset && (dwOffset < CEPERF_MAP_SIZE)) {
        return (PVOID) ((DWORD) g_pMaster + dwOffset);
    }
    return NULL;
}

__inline static DWORD GET_MAP_OFFSET (PVOID pPointer)
{
    DWORD dwOffset = (DWORD)pPointer - (DWORD)g_pMaster;
    if (pPointer && (dwOffset < CEPERF_MAP_SIZE)) {
        return dwOffset;
    }
    return 0;
}


//------------------------------------------------------------------------------
// Unit testing definitions
//------------------------------------------------------------------------------

#ifdef CEPERF_UNIT_TEST

// Used with IOCTL_CEPERF_REPLACE_OSCALLS
typedef BOOL (WINAPI *PFN_QueryPerformanceCounter) (LARGE_INTEGER *lpPerformanceCount);
typedef BOOL (WINAPI *PFN_QueryPerformanceFrequency) (LARGE_INTEGER *lpFrequency);
typedef WINBASEAPI DWORD (WINAPI *PFN_GetTickCount) (VOID);
typedef WINBASEAPI BOOL (WINAPI *PFN_GetThreadTimes) (HANDLE hThread,
    LPFILETIME lpCreationTime, LPFILETIME lpExitTime, LPFILETIME lpKernelTime,
    LPFILETIME lpUserTime);
typedef DWORD (WINAPI *PFN_GetCurrentThreadId) (VOID);
typedef struct {
    PFN_QueryPerformanceCounter   pfnQueryPerformanceCounter;
    PFN_QueryPerformanceFrequency pfnQueryPerformanceFrequency;
    PFN_GetTickCount              pfnGetTickCount;
    PFN_GetThreadTimes            pfnGetThreadTimes;
    PFN_GetCurrentThreadId        pfnGetCurrentThreadId;
} CePerfOSCalls;

extern CePerfOSCalls g_OSCalls;
#define CePerf_QueryPerformanceCounter   g_OSCalls.pfnQueryPerformanceCounter
#define CePerf_QueryPerformanceFrequency g_OSCalls.pfnQueryPerformanceFrequency
#define CePerf_GetTickCount              g_OSCalls.pfnGetTickCount
#define CePerf_GetThreadTimes            g_OSCalls.pfnGetThreadTimes
#define CePerf_GetCurrentThreadId        g_OSCalls.pfnGetCurrentThreadId

#define IOCTL_CEPERF_REPLACE_OSCALLS 1


#else // CEPERF_UNIT_TEST

// When we're not unit testing, we always call the OS APIs directly
#define CePerf_QueryPerformanceCounter   QueryPerformanceCounter
#define CePerf_QueryPerformanceFrequency QueryPerformanceFrequency
#define CePerf_GetTickCount              GetTickCount
#define CePerf_GetThreadTimes            GetThreadTimes
#define CePerf_GetCurrentThreadId        GetCurrentThreadId

#endif // CEPERF_UNIT_TEST

#endif // _PCEPERF_H_

