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
// --------------------------------------------------------------------------
//
//
// Module:
//
//     coredll.h
//
// Purpose:
//
//    Internal include file used by the core dll.
//
// --------------------------------------------------------------------------

#ifndef __COREDLL_H__
#define __COREDLL_H__


#if !defined(SHIP_BUILD)
#define DBGFIXHP    DEBUGZONE(0)
#define DBGLMEM     DEBUGZONE(1)
#define DBGMOV      DEBUGZONE(2)
#define DBGMEMCHK   DEBUGZONE(3)  // heap checks zone
#define DBGVIRTMEM  DEBUGZONE(4)
#define DBGDEVICE   DEBUGZONE(5)
#define DBGDEPRE    DEBUGZONE(6) // debug check on deprecated API usage
#define DBGLOADER   DEBUGZONE(7)
#define DBGSTDIO    DEBUGZONE(8)
#define DBGSTDIOHF  DEBUGZONE(9)
#define DBGSHELL    DEBUGZONE(10)
#define DBGIMM      DEBUGZONE(11)
#define DBGEH       DEBUGZONE(11)
#define DBGHEAPVAL  DEBUGZONE(12)
#define DBGRMEM     DEBUGZONE(13)
#define DBGHEAP2    DEBUGZONE(14)
#define DBGDSA      DEBUGZONE(15)
#endif // SHIP_BUILD

#ifdef DEBUG
BOOL WINAPI HeapValidate(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem);

#define DEBUGHEAPVALIDATE(heap) \
    ((void)((DBGHEAPVAL) ? (HeapValidate((HANDLE) heap, 0, NULL)), 1 : 0))
#else
#define DEBUGHEAPVALIDATE(heap) ((void)0)

#endif // DEBUG


#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI LMemInit(VOID);
extern HANDLE hInstCoreDll;

#define hActiveProc                 (GetCurrentProcess ())
extern BOOL IsExiting;              // if process is exiting

// the fiber structure
typedef struct _FIBERSTRUCT {
    DWORD dwThrdId;     // 00: current running thread (MUST BE FIRST, KERNEL USE IT TO DETECT FIBER STATE)
    DWORD dwStkBase;    // 04: stack base
    DWORD dwStkBound;   // 08: stack bound
    DWORD dwStkPtr;     // 0c: current stack ptr
    DWORD dwStkSize;    // 10: stack size
    LPVOID lpParam;     // 14: fiber parameter
} FIBERSTRUCT, *PFIBERSTRUCT;

extern PFN_KLIBIOCTL g_pfnKLibIoctl;


// Per-process lock, used primarily for loader
extern CRITICAL_SECTION g_csProc;

// Per-process CFFM lock
extern CRITICAL_SECTION g_csCFFM;

void ReleaseProcCS ();

__inline void LockProcCS (void)
{
    EnterCriticalSection (&g_csProc);
}

__inline void UnlockProcCS (void)
{
    LeaveCriticalSection (&g_csProc);
}

__inline BOOL OwnProcCS (void)
{
    return ((HANDLE) GetCurrentThreadId () == g_csProc.OwnerThread);
}

__inline void LockProcCFFM (void)
{
    EnterCriticalSection (&g_csCFFM);
}

__inline void UnlockProcCFFM (void)
{
    LeaveCriticalSection (&g_csCFFM);
}

#ifdef KCOREDLL

_inline BOOL KDbgSanitize (LPVOID pDst, LPVOID pAddrMem, DWORD cbSize)
{
    return (* g_pfnKLibIoctl) ((HANDLE) KMOD_DBG, IOCTL_DBG_SANITIZE, pAddrMem, cbSize, pDst, cbSize, NULL);
}

#else // KCOREDLL

extern DWORD g_dwMainThId;

extern BOOL xxx_KernelLibIoControl (HANDLE hLib, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);
_inline BOOL KDbgSanitize (LPVOID pDst, LPVOID pAddrMem, DWORD cbSize)
{
    return xxx_KernelLibIoControl ((HANDLE) KMOD_DBG, IOCTL_DBG_SANITIZE, pAddrMem, cbSize, pDst, cbSize, NULL);
}
extern BOOL IsCurrentProcessTerminated (void);

#endif // KCOREDLL


void CELOG_HeapCreate(DWORD dwOptions, DWORD dwInitSize, DWORD dwMaxSize, HANDLE hHeap);
void CELOG_HeapAlloc(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes, DWORD lpMem);
void CELOG_HeapRealloc(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes, DWORD lpMemOld, DWORD lpMem);
void CELOG_HeapFree(HANDLE hHeap, DWORD dwFlags, DWORD lpMem);
void CELOG_HeapDestroy(HANDLE hHeap);

//
// TLS in PSL support
//
BOOL   CeTlsCoreInit (); // called by coredll to initialize TLS
BOOL   CeTlsSetValue (DWORD slot, LPVOID lpValue, BOOL fThrdInPSL); // for local and PSL threads
LPVOID CeTlsGetValue (DWORD slot, LPBOOL pfNoTlsData); // for PSL threads
BOOL   CeTlsFreeInAllThreads (DWORD slot); // for local and PSL threads
BOOL   CeTlsThreadExit(void); // called on thread detach (only valid for kcoredll or local TLS)
BOOL   CeTlsFreeInCurrentThread (DWORD slot); // called on thread detach (only valid for coredll)
BOOL   CeTlsSetCleanupFunction (DWORD slot, PFNVOID pfnCleanupFunc); // to set a cleanup function for a slot

#ifdef __cplusplus
}
#endif

#endif /* __COREDLL_H__ */

