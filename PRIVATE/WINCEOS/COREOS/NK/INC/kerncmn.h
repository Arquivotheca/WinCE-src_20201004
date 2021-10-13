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
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    kerncmn.h - common kernel header
//

#ifndef _NK_KERNCMN_H_
#define _NK_KERNCMN_H_


// common type forward declarations
typedef struct _PROCESS         PROCESS, *PPROCESS;             // process
typedef const PROCESS           *PCPROCESS;

typedef struct _PPAGEDIRECTORY PAGEDIRECTORY, *PPAGEDIRECTORY;  // page directory
typedef const PAGEDIRECTORY     PCPAGEDIRECTORY;

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

typedef struct _CRIT            CRIT, *PCRIT;                   // critical section
typedef const CRIT              *PCCRIT;

typedef struct _MSGNODE         MSGNODE, *PMSGNODE;             // message queue node
typedef const MSGNODE           *PCMSGNODE;

typedef struct _MSGQUEUE        MSGQUEUE, *PMSGQUEUE;           // message queue
typedef const MSGQUEUE          *PCMSGQUEUE;

typedef struct _STUBEVENT       STUBEVENT, *PSTUBEVENT;         // stub event??
typedef const STUBEVENT         *PCSTUBEVENT;

typedef struct _CLEANEVENT      CLEANEVENT, *PCLEANEVENT;       // clean event
typedef const CLEANEVENT        *PCCLEANEVENT;

typedef struct _MAPVIEW         MAPVIEW, *PMAPVIEW;             // view of mapfiles
typedef const MAPVIEW           *PCMAPVIEW;

typedef struct _FSMAP           FSMAP, *PFSMAP;                 // mapfile
typedef const FSMAP             *PCFSMAP;

typedef struct _HNDLTABLE       HNDLTABLE, *PHNDLTABLE;         // handle table
typedef const HNDLTABLE         *PCHNDLTABLE;

typedef struct _PAGETABLE       PAGETABLE, *PPAGETABLE;         // page table
typedef const PAGETABLE         *PCPAGETABLE;

typedef struct _APISET          APISET, *PAPISET;               // api set
typedef const APISET            *PCAPISET;

typedef struct _FREEINFO        FREEINFO, *PFREEINFO;           // free info
typedef const FREEINFO          *PCFREEINFO;

typedef struct _MEMORYINFO      MEMORYINFO, *PMEMORYINFO;       // memory info
typedef const MEMORYINFO        *PCMEMORYINFO;

typedef struct _CALLSTACK       CALLSTACK, *PCALLSTACK;         // call stack
typedef const CALLSTACK         *PCCALLSTACK;

typedef struct _OBJCALLSTRUCT   OBJCALLSTRUCT, *POBJCALLSTRUCT; // API call information
typedef const OBJCALLSTRUCT     *PCOBJCALLSTRUCT;

typedef struct _SVRRTNSTRUCT    SVRRTNSTRUCT, *PSVRRTNSTRUCT;   // API return information
typedef const SVRRTNSTRUCT      *PCSVRRTNSTRUCT;

typedef struct _TOKENLIST       TOKENLIST, *PTOKENLIST;         // token list
typedef const TOKENLIST         *PCTOKENLIST;

typedef struct _TOKENINFO       TOKENINFO, *PTOKENINFO;
typedef const TOKENINFO         *PCTOKENINFO;

typedef struct _MODULE          MODULE, *PMODULE, *LPMODULE;    // DLLs
typedef const MODULE            *PCMODULE;

typedef struct _ADDRMAP         ADDRMAP, *PADDRMAP;             // OEMAddressTable entry
typedef const ADDRMAP           *PCADDRMAP;

typedef struct _CINFO           CINFO, *PCINFO;                 // server information
typedef const CINFO             *PCCINFO;

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

typedef struct _DLIST           DLIST, *PDLIST;                 // doubly linked list (circular)
typedef const DLIST             *PCDLIST;

struct _DLIST {
    PDLIST pFwd;
    PDLIST pBack;
};

#include <fslog.h>  // common header shared between kernel and filesys

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
    WCHAR name[MAX_PATH];   /* name of item */
};

// ?????
typedef BOOL (*PFNIOCTL)(DWORD dwInstData, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);
typedef void (*PKINTERRUPT_ROUTINE)(void);
typedef void (*RETADDR)();
typedef int (*PKFN)();

// Call a function in non-preemtible kernel mode.
//   Returns the return value from the function.
int KCall(PKFN pfn, ...);


// utility functions

// add an item to the head of a circular list
__inline void AddToDListHead (PDLIST pHead, PDLIST pItem)
{
    PDLIST pFwd  = pHead->pFwd;
    pItem->pFwd  = pFwd;
    pItem->pBack = pHead;
    pHead->pFwd  = pFwd->pBack = pItem;
}

// add an item to the tail a circular list
__inline void AddToDListTail (PDLIST pHead, PDLIST pItem)
{
    PDLIST pBack  = pHead->pBack;
    pItem->pBack  = pBack;
    pItem->pFwd   = pHead;
    pHead->pBack  = pBack->pFwd = pItem;
}

// remove an item from a circular list
__inline void RemoveDList (PDLIST pItem)
{
    pItem->pFwd->pBack = pItem->pBack;
    pItem->pBack->pFwd = pItem->pFwd;
}

__inline void InitDList (PDLIST pHead)
{
    pHead->pBack = pHead->pFwd = pHead;
}

__inline BOOL IsDListEmpty (PDLIST pHead)
{
    return pHead == pHead->pFwd;
}

typedef BOOL (* PFN_DLISTENUM) (PDLIST pItem, LPVOID pEnumData);

// EnumerateDList - enumerate DList, call pfnEnum on every item. Stop when pfnEnum returns TRUE, 
// or the list is exhausted. Return the item when pfnEnum returns TRUE, or NULL
// if list is exhausted.
PDLIST EnumerateDList (PDLIST phead, PFN_DLISTENUM pfnEnum, LPVOID pEnumData);

//
// DestroyDList - Remove all items in a DLIST, call pfnEnum on every item destroyed, return value ignored.
//
void DestroyDList (PDLIST phead, PFN_DLISTENUM pfnEnum, LPVOID pEnumData);

//
// MergeDList - merge two DList, the source DList will be emptied
// 
void MergeDList (PDLIST pdlDst, PDLIST pdlSrc);

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
#define CSARRAY_FLUSH      8
#define CSARRAY_NKLOADER   13


#endif // _NK_KERNCMN_H_

