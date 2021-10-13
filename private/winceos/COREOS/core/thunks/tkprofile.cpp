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

/*
 *      NK Statistical Profiler API's
 *
 *
 * Module Name:
 *
 *      profiler.c
 *
 * Abstract:
 *
 *      This file implements the NK statistical profiler API's
 *
 *
 * Revision History:
 *
 */

#ifdef KCOREDLL
#define WIN32_CALL(type, api, args) (*(type (*) args) g_ppfnWin32Calls[W32_ ## api])
#else
#define WIN32_CALL(type, api, args) IMPLICIT_DECL(type, SH_WIN32, W32_ ## api, args)
#endif
     
#include <windows.h>
#include <coredll.h>
#define C_ONLY
#include "profiler.h"
#include <celog.h>
#include <ceperf.h>

#define MAX_STACK_FRAME 20

extern "C"
ULONG
xxx_GetThreadCallStack(
    HANDLE          hThrd,
    ULONG           dwMaxFrames,
    LPVOID          lpFrames,
    DWORD           dwFlags,
    DWORD           dwSkip
    );


// Instead of using structs that have zero-size arrays, declare some
// new types with explicit array sizes

typedef struct { // Same as StackSnapshot except rgCalls is MAX_STACK_FRAME
    WORD  wVersion;
    WORD  wNumCalls;
    CallSnapshot rgCalls[MAX_STACK_FRAME];
} StackSnapshot_WITHSTACK;

typedef struct { // Same as CEL_HEAP_ALLOC except the stack struct is explicitly defined
    HANDLE  hHeap;
    DWORD   dwFlags;
    DWORD   dwBytes;
    DWORD   lpMem;
    DWORD   dwTID;
    DWORD   dwPID;
    DWORD   dwCallerPID;
    StackSnapshot_WITHSTACK stack;
} CEL_HEAP_ALLOC_WITHSTACK;

typedef struct { // Same as CEL_HEAP_REALLOC except the stack struct is explicitly defined
    HANDLE  hHeap;
    DWORD   dwFlags;
    DWORD   dwBytes;
    DWORD   lpMemOld;
    DWORD   lpMem;
    DWORD   dwTID;
    DWORD   dwPID;
    DWORD   dwCallerPID;
    StackSnapshot_WITHSTACK stack;
} CEL_HEAP_REALLOC_WITHSTACK;

typedef struct { // Same as CEL_HEAP_FREE except the stack struct is explicitly defined
    HANDLE  hHeap;
    DWORD   dwFlags;
    DWORD   lpMem;
    DWORD   dwTID;
    DWORD   dwPID;
    DWORD   dwCallerPID;
    StackSnapshot_WITHSTACK stack;
} CEL_HEAP_FREE_WITHSTACK;


// CePerf globals
HMODULE g_hCePerfDLL = NULL;
BOOL g_IsCePerfMissing = FALSE;


//---------------------------------------------------------------------------
// ProfileStart - clear profile counters and enable kernel profiler ISR
//
// INPUT:  dwUSecInterval - non-zero = sample interval in uSec
//                          zero     = reset uSec counter
//         dwOptions - PROFILE_* flags to control type of data collected and
//                     method of collection
//---------------------------------------------------------------------------
extern "C"
DWORD ProfileStart(DWORD dwUSecInterval, DWORD dwOptions)
{
    // C4815: zero-sized array in stack object will have no elements
    // zero sized array is unused in this scenario
#pragma warning(push)
#pragma warning(disable : 4815)
    ProfilerControl control;
#pragma warning(pop)

    // Block OEMDEFINED flag
    if (dwOptions & PROFILE_OEMDEFINED) {
        RETAILMSG(1, (TEXT("Cannot start OEM profiler with ProfileStart, use ProfileStartEx\r\n")));
        return 0;
    }

    // Add START flag if necessary
    if ((dwOptions & (PROFILE_STOP | PROFILE_OEM_QUERY)) == 0) {
        dwOptions |= PROFILE_START;
    }
    
    control.dwVersion  = 1;
    control.dwOptions  = dwOptions;
    control.dwReserved = 0;
    control.Kernel.dwUSecInterval = dwUSecInterval;
    
    ProfileSyscall(&control, sizeof(ProfilerControl));

    return 0;
}



//---------------------------------------------------------------------------
// ProfileStartEx - clear profile counters and enable kernel profiler ISR,
//                  or enable OEM-defined profiling
//
// INPUT:  pControl - Information about what profiler to use, what data to
//                    collect and how to collect it.
//---------------------------------------------------------------------------
extern "C"
DWORD ProfileStartEx(ProfilerControl *pControl)
{
    if (pControl) {
        DWORD cbControl = sizeof(ProfilerControl);
        if (pControl->dwOptions & PROFILE_OEMDEFINED) {
            cbControl += pControl->OEM.dwControlSize;
        }
        
        ProfileSyscall(pControl, cbControl);
    }
    return 0;
}


//---------------------------------------------------------------------------
// ProfileStop - Stop profile and display hit report
//---------------------------------------------------------------------------
extern "C"
void ProfileStop(void)
{
    // C4815: zero-sized array in stack object will have no elements 
    // zero sized array is unused in this scenario    
#pragma warning(push)
#pragma warning(disable : 4815)
    ProfilerControl control;
#pragma warning(pop)

    control.dwVersion  = 1;
    control.dwOptions  = PROFILE_STOP;
    control.dwReserved = 0;
    
    ProfileSyscall(&control, sizeof(ProfilerControl));
    
    return;
}


//---------------------------------------------------------------------------
// ProfileCaptureStatus - Insert the current status of the OEM-defined profiler
//                        into the CeLog data stream
//---------------------------------------------------------------------------
extern "C"
VOID ProfileCaptureStatus()
{
    // C4815: zero-sized array in stack object will have no elements 
    // zero sized array is unused in this scenario    
#pragma warning(push)
#pragma warning(disable : 4815)
    ProfilerControl control;
#pragma warning(pop)

    control.dwVersion  = 1;
    control.dwOptions  = PROFILE_OEM_QUERY;
    control.dwReserved = 0;
    
    ProfileSyscall(&control, sizeof(ProfilerControl));
    
    return;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
void
xxx_DumpKCallProfile(
    DWORD bReset
    )
{
    // This functionality has been removed from the OS.  There is no replacement.
    RETAILMSG (1, (L"xxx_DumpKCallProfile not implemented\r\n"));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
VOID xxx_CeLogData(
    BOOL  fTimeStamp, 
    WORD  wID, 
    PVOID pData, 
    WORD  wLen, 
    DWORD dwZoneUser, 
    DWORD dwZoneCE,
    WORD  wFlag,
    BOOL  bFlagged)
{
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        CeLogData(fTimeStamp, wID, pData, wLen, dwZoneUser, dwZoneCE,
                  wFlag, bFlagged);
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
VOID xxx_CeLogSetZones(
    DWORD dwZoneUser, 
    DWORD dwZoneCE, 
    DWORD dwZoneProcess)
{
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        CeLogSetZones(dwZoneUser, dwZoneCE, dwZoneProcess);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL xxx_CeLogGetZones(
    LPDWORD lpdwZoneUser,
    LPDWORD lpdwZoneCE,
    LPDWORD lpdwZoneProcess,
    LPDWORD lpdwAvailableZones)
{
    BOOL fResult = FALSE;

    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        fResult = CeLogGetZones(lpdwZoneUser, lpdwZoneCE, lpdwZoneProcess,
                                lpdwAvailableZones);
    } else {
        SetLastError(ERROR_NOT_READY);
    }

    return fResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL xxx_CeLogReSync()
{
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        return CeLogReSync();
    }
    SetLastError(ERROR_NOT_READY);
    return FALSE;
}


//----------------------------------------------------------
extern "C"
void CELOG_HeapCreate(DWORD dwOptions, DWORD dwInitSize, DWORD dwMaxSize, HANDLE hHeap)
{
    if (IsCeLogZoneEnabled(CELZONE_HEAP | CELZONE_MEMTRACKING)) {
        CEL_HEAP_CREATE cl;

        cl.dwOptions = dwOptions;
        cl.dwInitSize = dwInitSize;
        cl.dwMaxSize = dwMaxSize;
        cl.hHeap = hHeap;
        cl.dwPID = GetCurrentProcessId();
        cl.dwTID = GetCurrentThreadId();

        CeLogData(TRUE, CELID_HEAP_CREATE, &cl, sizeof(CEL_HEAP_CREATE), 0,
                  CELZONE_HEAP | CELZONE_MEMTRACKING, 0, FALSE);
    }
}

//----------------------------------------------------------
extern "C"
void CELOG_HeapAlloc(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes, DWORD lpMem)
{
    if (IsCeLogZoneEnabled(CELZONE_HEAP | CELZONE_MEMTRACKING) && lpMem) {
        CEL_HEAP_ALLOC_WITHSTACK cl;
        DWORD dwLastError = GetLastError();
        WORD cbData;

        cl.hHeap   = hHeap;
        cl.dwFlags = dwFlags;
        cl.dwBytes = dwBytes;
        cl.lpMem   = lpMem;
        cl.dwTID = GetCurrentThreadId();
        cl.dwPID = GetCurrentProcessId();
        cl.dwCallerPID = GetCallerVMProcessId();

        cl.stack.wVersion = 1;
        cl.stack.wNumCalls = (WORD) xxx_GetThreadCallStack (GetCurrentThread (), MAX_STACK_FRAME, cl.stack.rgCalls, 0, 0);

        // Still use the original struct size when reporting the data size
        cbData = (WORD) (sizeof(CEL_HEAP_ALLOC) + sizeof(StackSnapshot) + cl.stack.wNumCalls*sizeof(CallSnapshot));

        CeLogData(TRUE, CELID_HEAP_ALLOC, (PVOID) &cl, cbData,
                  0, CELZONE_HEAP | CELZONE_MEMTRACKING, 0, FALSE);
        SetLastError(dwLastError);
    }
}

//----------------------------------------------------------
extern "C"
void CELOG_HeapRealloc(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes, DWORD lpMemOld, DWORD lpMem)
{
    if (IsCeLogZoneEnabled(CELZONE_HEAP | CELZONE_MEMTRACKING) && lpMem) {
        CEL_HEAP_REALLOC_WITHSTACK cl;
        DWORD dwLastError;  // Don't change lasterr while capturing callstack
        WORD cbData;
        
        dwLastError = GetLastError();

        cl.hHeap = hHeap;
        cl.dwFlags = dwFlags;
        cl.dwBytes = dwBytes;
        cl.lpMemOld = lpMemOld;
        cl.lpMem = lpMem;
        cl.dwTID = GetCurrentThreadId();
        cl.dwPID = GetCurrentProcessId();
        cl.dwCallerPID = GetCallerVMProcessId();

        cl.stack.wVersion = 1;
        cl.stack.wNumCalls = (WORD) xxx_GetThreadCallStack (GetCurrentThread (), MAX_STACK_FRAME, cl.stack.rgCalls, 0, 0);

        // Still use the original struct size when reporting the data size
        cbData = (WORD) (sizeof(CEL_HEAP_REALLOC) + sizeof(StackSnapshot) + cl.stack.wNumCalls*sizeof(CallSnapshot));

        CeLogData(TRUE, CELID_HEAP_REALLOC, (PVOID) &cl, cbData,
                  0, CELZONE_HEAP | CELZONE_MEMTRACKING, 0, FALSE);
        SetLastError(dwLastError);
    }
}

//----------------------------------------------------------
extern "C"
void CELOG_HeapFree(HANDLE hHeap, DWORD dwFlags, DWORD lpMem)
{
    if (IsCeLogZoneEnabled(CELZONE_HEAP | CELZONE_MEMTRACKING)) {
        CEL_HEAP_FREE_WITHSTACK cl;
        DWORD dwLastError;  // Don't change lasterr while capturing callstack
        WORD cbData;
        
        dwLastError = GetLastError();

        cl.hHeap = hHeap;
        cl.dwFlags = dwFlags;
        cl.lpMem = lpMem;
        cl.dwTID = GetCurrentThreadId();
        cl.dwPID = GetCurrentProcessId();
        cl.dwCallerPID = GetCallerVMProcessId ();

        cl.stack.wVersion = 1;
        cl.stack.wNumCalls = (WORD) xxx_GetThreadCallStack (GetCurrentThread (), MAX_STACK_FRAME, cl.stack.rgCalls, 0, 0);

        // Still use the original struct size when reporting the data size
        cbData = (WORD) (sizeof(CEL_HEAP_FREE) + sizeof(StackSnapshot) + cl.stack.wNumCalls*sizeof(CallSnapshot));

        CeLogData(TRUE, CELID_HEAP_FREE, (PVOID) &cl, cbData,
                  0, CELZONE_HEAP | CELZONE_MEMTRACKING, 0, FALSE);
        SetLastError(dwLastError);
    }
}

//----------------------------------------------------------
extern "C"
void CELOG_HeapDestroy(HANDLE hHeap)
{
    if (IsCeLogZoneEnabled(CELZONE_HEAP | CELZONE_MEMTRACKING)) {
        CEL_HEAP_DESTROY cl;
    
        cl.hHeap = hHeap;
        cl.dwPID = GetCurrentProcessId();
        cl.dwTID = GetCurrentThreadId();
    
        CeLogData(TRUE, CELID_HEAP_DESTROY, &cl, sizeof(CEL_HEAP_DESTROY), 0,
                  CELZONE_HEAP | CELZONE_MEMTRACKING, 0, FALSE);
    }
}


//------------------------------------------------------------------------------
// Load the CePerf DLL and set up the globals.  Called once for each caller
// module (DLL/EXE) which uses CePerf.
//------------------------------------------------------------------------------
extern "C"
CePerfGlobals* CePerfDLLSetup(
    LPVOID* ppCePerfInternal,   // Pointer to caller's global state variable
    BOOL AlwaysReload           // FALSE: don't try loading ceperf.dll if it
                                // previously failed, TRUE: Try again.
    )
{
    CePerfGlobals* pGlobals = NULL;

    // Try/except to protect against bad globals in the caller
    __try {

        if (*ppCePerfInternal) {
            // This caller has already loaded CePerf - return its current state
            pGlobals = (CePerfGlobals*) *ppCePerfInternal;
        
        } else if (g_IsCePerfMissing && !AlwaysReload) {
            // Already found the DLL missing once - don't keep trying to reload
            pGlobals = NULL;
        
        } else {
            // This caller has not loaded CePerf before

            // Load ceperf.dll once per caller
            g_hCePerfDLL = LoadLibrary(TEXT("CePerf.dll"));
            if (!g_hCePerfDLL) {
                // The load failed
                g_IsCePerfMissing = TRUE;
            } else {
                // The load succeeded. Update g_IsCePerfMissing in case it was
                // set to TRUE from a previous failed load attempt.
                g_IsCePerfMissing = FALSE;

                // Call ceperf.dll to get a new CePerfGlobals struct for the caller
                PFN_CePerfInit pCePerfInit
                    = (PFN_CePerfInit) GetProcAddressA(g_hCePerfDLL, "CePerfInit");
                if (pCePerfInit) {
                    HRESULT hr = pCePerfInit((CePerfGlobals**) ppCePerfInternal);
                    if (CEPERF_HR_ALREADY_EXISTS == hr) {
                        // Another thread from the caller module beat us here.
                        // Clean up the extra reference to ceperf.dll made above.
                        FreeLibrary(g_hCePerfDLL);
                        hr = ERROR_SUCCESS;  // Still a success case
                    }
                    if (ERROR_SUCCESS == hr) {
                        pGlobals = (CePerfGlobals*) *ppCePerfInternal;
                        DEBUGCHK(pGlobals);
                    }
                }
            }
        }

    } __except(EXCEPTION_EXECUTE_HANDLER) {
        ;
    }
    
    DEBUGMSG(!pGlobals, (TEXT("CePerf.dll is absent - performance data will not be recorded\r\n")));

    return pGlobals;
}


//------------------------------------------------------------------------------
// Unload the CePerf DLL and clean up the globals.  Called once for each caller
// module (DLL/EXE) which uses CePerf.
//------------------------------------------------------------------------------
extern "C"
BOOL CePerfDLLCleanup(LPVOID* ppCePerfInternal)
{
    // Try/except to protect against bad globals in the caller
    __try {

        // Get the pointer to the CePerf globals inside the caller DLL/EXE.
        // This will always be NULL if ceperf.dll is unavailable.
        CePerfGlobals* pGlobals = (CePerfGlobals*) *ppCePerfInternal;
        if (pGlobals) {
            // The caller's refcount on ceperf.dll should now be zero
            DEBUGCHK(0 == pGlobals->dwCePerfDLLRefcount);

            if (g_hCePerfDLL) {
                // Call ceperf.dll to clean up the CePerfGlobals struct for the caller
                PFN_CePerfDeinit pCePerfDeinit
                    = (PFN_CePerfDeinit) GetProcAddressA(g_hCePerfDLL, "CePerfDeinit");
                if (pCePerfDeinit) {            
                    HRESULT hr = pCePerfDeinit((CePerfGlobals**) ppCePerfInternal);
                    // --> CePerf code is now inaccessible to the caller

                    if (ERROR_SUCCESS == hr) {
                        // Free ceperf.dll once per caller
                        FreeLibrary(g_hCePerfDLL);
                    }
                }
            }
        }

    } __except(EXCEPTION_EXECUTE_HANDLER) {
        ;
    }
    
    return TRUE;
}
