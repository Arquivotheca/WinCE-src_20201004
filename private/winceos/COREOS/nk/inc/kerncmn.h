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
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    kerncmn.h - common kernel header
//

#ifndef _NK_KERNCMN_H_
#define _NK_KERNCMN_H_

#define SYS_HANDLS_IN_KDATA 4   // number of sys handles kept in KData


// common type forward declarations
typedef struct _PROCESS         PROCESS, *PPROCESS;             // process
typedef const PROCESS           *PCPROCESS;

typedef struct _PPAGEDIRECTORY PAGEDIRECTORY, *PPAGEDIRECTORY;  // page directory
typedef const PAGEDIRECTORY     *PCPAGEDIRECTORY;

typedef struct _THREAD          THREAD, *PTHREAD;               // thread
typedef const THREAD            *PCTHREAD;

typedef struct _NAME            NAME, *PNAME;                   // name
typedef const NAME              *PCName;

typedef struct _PROXY           PROXY, *PPROXY;                 // proxy
typedef const PROXY             PCPROXY;

typedef struct _HDATA           HDATA, *PHDATA;                 // handle data
typedef const HDATA             *PCHDATA;

typedef struct _EVENT           EVENT, *PEVENT;                 // event
typedef const EVENT             *PCEVENT;

typedef struct _MUTEX           MUTEX, *PMUTEX;                 // mutex
typedef const MUTEX             *PCMUTEX;

typedef struct _SEMAPHORE       SEMAPHORE, *PSEMAPHORE;         // semaphore
typedef const SEMAPHORE         *PCSEMAPHORE;

typedef struct _MSGNODE         MSGNODE, *PMSGNODE;             // message queue node
typedef const MSGNODE           *PCMSGNODE;

typedef struct _MSGQUEUE        MSGQUEUE, *PMSGQUEUE;           // message queue
typedef const MSGQUEUE          *PCMSGQUEUE;

typedef struct _MAPVIEW         MAPVIEW, *PMAPVIEW;             // view of mapfiles
typedef const MAPVIEW           *PCMAPVIEW;

typedef struct _FSMAP           FSMAP, *PFSMAP;                 // mapfile
typedef const FSMAP             *PCFSMAP;

typedef struct _FAST_LOCK       FAST_LOCK, *PFAST_LOCK;         // fast lock (reader/writer lock, no nesting support)
typedef volatile FAST_LOCK      *VPFAST_LOCK;

typedef struct _HNDLTABLE       HNDLTABLE, *PHNDLTABLE;         // handle table
typedef const HNDLTABLE         *PCHNDLTABLE;

typedef struct _PAGETABLE       PAGETABLE, *PPAGETABLE;         // page table
typedef const PAGETABLE         *PCPAGETABLE;

typedef struct _APISET          APISET, *PAPISET;               // api set
typedef const APISET            *PCAPISET;

typedef struct _REGIONINFO      REGIONINFO, *PREGIONINFO;           // free info
typedef const REGIONINFO        *PCREGIONINFO;

// to keep tools build
typedef REGIONINFO              FREEINFO;
typedef PREGIONINFO             PFREEINFO;

typedef struct _MEMORYINFO      MEMORYINFO, *PMEMORYINFO;       // memory info
typedef const MEMORYINFO        *PCMEMORYINFO;

typedef struct _CALLSTACK       CALLSTACK, *PCALLSTACK;         // call stack
typedef const CALLSTACK         *PCCALLSTACK;

typedef struct _MODULE          MODULE, *PMODULE, *LPMODULE;    // DLLs
typedef const MODULE            *PCMODULE;

typedef struct _ADDRMAP         ADDRMAP, *PADDRMAP;             // OEMAddressTable entry
typedef const ADDRMAP           *PCADDRMAP;

typedef struct _CINFO           CINFO, *PCINFO;                 // server information
typedef const CINFO             *PCCINFO;

typedef struct _PSLSERVER       PSLSERVER, *PPSLSERVER;         // psl server id
typedef const PSLSERVER         *PCPSLSERVER;

typedef struct _INTCHAIN        INTCHAIN, *PINTCHAIN;           // Installable ISR
typedef const INTCHAIN          *PCINTCHAIN;

typedef struct _WatchDog        WatchDog, *PWatchDog;           // watchdog timer
typedef const WatchDog          *PCWatchDog;

typedef struct _STKLIST         STKLIST, *PSTKLIST;             // stack list
typedef const STKLIST           *PCSTKLIST;

typedef struct _VALIST          VALIST, *PVALIST;               // virtual allocation list
typedef const VALIST            *PCVALIST;

typedef struct _ALIAS           ALIAS, *PALIAS;                 // list of aliased virtual addresses
typedef const ALIAS             *PCALIAS;

typedef struct _LOCKPAGELIST    LOCKPAGELIST, *PLOCKPAGELIST;   // locked page list
typedef const LOCKPAGELIST      *PCLOCKPAGELIST;

typedef struct _WAITSTRUCT      WAITSTRUCT, *PWAITSTRUCT;       // wait stuct
typedef const WAITSTRUCT        *PCWAITSTRUCT;

typedef struct _SLEEPSTRUCT     SLEEPSTRUCT, *PSLEEPSTRUCT;     // structure to put thread to sleep
typedef const SLEEPSTRUCT       *PCSLEEPSTRUCT;

typedef struct _2D_NODE         TWO_D_NODE, *PTWO_D_NODE;       // nodes in 2-D priority queue
typedef const TWO_D_NODE        *PCTWO_D_NODE;

typedef struct _2D_QUEUE        TWO_D_QUEUE, *PTWO_D_QUEUE;     // 2-D priority queue
typedef const TWO_D_QUEUE       *PCTWO_D_QUEUE;

typedef struct _SPINLOCK        SPINLOCK, *PSPINLOCK;           // spinlock
typedef const SPINLOCK          *PCSPINLOCK;

typedef struct _STUBTHREAD      STUBTHREAD, *PSTUBTHREAD;       // stub thread
typedef const STUBTHREAD        *PCSTUBTHREAD;

typedef struct _DeviceTableEntry DeviceTableEntry, *PDeviceTableEntry;
typedef const DeviceTableEntry  *PCDeviceTableEntry;

#define PRIORITY_LEVELS_HASHSIZE 16

#if defined (IN_KERNEL) && defined (ARM)
#define g_pKData        ((struct KDataStruct *) PUserKData)
#else
extern struct KDataStruct *g_pKData;
#endif

//
// Nodes in 2-D priority queue
//
struct _2D_NODE {
    PTWO_D_NODE pQPrev;         // Previous node in 2-D queue
    PTWO_D_NODE pQNext;         // Next node in 2-D queue
    PTWO_D_NODE pQUp;           // Node above in 2-D queue
    PTWO_D_NODE pQDown;         // Node below in 2-D queue
    BYTE        prio;           // Current prio we're enqueued on
    BYTE        _pad_[3];
};

//
// 2-D priority queue
//
struct _2D_QUEUE {
    PTWO_D_NODE pHead;
    PTWO_D_NODE Hash[PRIORITY_LEVELS_HASHSIZE];
};

#include <fslog.h>  // common header shared between kernel and filesys
#include "dlist.h"

// useful macros

#ifndef offsetof
  #define offsetof(s,m) ((unsigned)&(((s *)0)->m))
#endif

#if defined(XREF_CPP_FILE)
#define ERRFALSE(exp)
#elif !defined(ERRFALSE)
// This macro is used to trigger a compile error if exp is false.
// If exp is false, i.e. 0, then the array is declared with size 0, triggering a compile error.
// If exp is true, the array is declared correctly.
// There is no actual array however.  The declaration is extern and the array is never actually referenced.
#define ERRFALSE(exp)           extern char __ERRXX[(exp)!=0]
#endif

//
// time types
//
#define TM_LOCALTIME            0       // local time
#define TM_SYSTEMTIME           1       // system time
#define TM_TIMEBIAS             2       // time bias

//
// include CPU dependent header
//
#include "nkintr.h"

#if defined(MIPS)
#include "nkmips.h"
#elif defined(SHx)
#include "nkshx.h"
#elif defined(x86)
#include "nkx86.h"
#elif defined(ARM)
#include "nkarm.h"
#else
#pragma error("No CPU Defined")
#endif


#define hActvProc   ((HANDLE) dwActvProcId)
#define hCurThrd    ((HANDLE) dwCurThId)

// common types that are used by all
struct _NAME {
    WORD wPool;
    WCHAR name[1];   /* name of item - variable length */
};

// ?????
typedef BOOL (*PFNIOCTL)(DWORD dwInstData, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);
typedef void (*PKINTERRUPT_ROUTINE)(void);
typedef void (*RETADDR)();
typedef int (*PKFN)();

// Call a function in non-preemtible kernel mode.
//   Returns the return value from the function.
int KCall(PKFN pfn, ...);

//
// string utility functions
//

// UNICODE
void NKwcscpy (LPWSTR p1, LPCWSTR p2);
DWORD NKwcsncpy (LPWSTR pDst, LPCWSTR pSrc, DWORD cchLen);
DWORD NKwcslen (LPCWSTR str);
int  NKwcsicmp (LPCWSTR str1, LPCWSTR str2);
int  NKwcscmp (LPCWSTR lpu1, LPCWSTR lpu2);
int  NKwcsnicmp(LPCWSTR str1, LPCWSTR str2, int count);
WCHAR NKtowlower(WCHAR ch);

// ASCII and UNICODE
int  NKstrcmpiAandW (LPCSTR lpa, LPCWSTR lpu);

// duplicate a string into a PNAME structure
PNAME DupStrToPNAME (LPCWSTR pszName);

#ifdef DEBUG
void SoftLog (DWORD dwTag, DWORD dwData);
void InitSoftLog (void);
void DumpSoftLogs (DWORD dwLastNEntries);
#else
#define SoftLog(a,b)
#define InitSoftLog()
#define DumpSoftLogs(x)
#endif

extern LPCRITICAL_SECTION csarray[];
#define CSARRAY_LOADERPOOL 2
#define CSARRAY_FILEPOOL   3
#define CSARRAY_FLUSH      9
#define CSARRAY_NKLOADER   15


#endif // _NK_KERNCMN_H_

