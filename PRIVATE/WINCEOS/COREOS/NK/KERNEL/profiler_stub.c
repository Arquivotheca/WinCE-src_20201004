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
//------------------------------------------------------------------------------
//
// PROFILER INTERFACE
//
// The actual implementation is in profiler.c, but the outward-facing wrapper
// is here.  This code is intended to always be present in the kernel, while
// the profiler implementation may be removed from the kernel.
//
//------------------------------------------------------------------------------

#define C_ONLY
#include "kernel.h"
#include <profiler.h>

ProfilerState g_ProfilerState;
DWORD g_ProfilerState_dwProfilerIntsInTLB;

// These values can be overwritten as FIXUPVARs in config.bib
const volatile DWORD dwProfileBufferMax        = 0;
const volatile DWORD dwProfileBootUSecInterval = 0;
const volatile DWORD dwProfileBootOptions      = 0;


//------------------------------------------------------------------------------
//  GetEPC - Get exception program counter
//
//  Output: returns EPC, zero if cpu not supported
//------------------------------------------------------------------------------
DWORD GetEPC(void)
{
#if defined(x86) && defined(NKPROF)

    extern PTHREAD pthFakeStruct;  // Only exists on x86 NKPROF builds
    PTHREAD pTh;

    if (g_pKData->cNest < 0) {
        pTh = pthFakeStruct;
    } else {
        pTh = pCurThread;
    }
    return GetThreadIP(pTh);

#elif defined(ARM)

    // -- NOT SUPPORTED ON ARM --
    // The address is not saved to the thread context on ARM, so GetEPC
    // cannot be used within an ISR.  Instead use the "ra" value that is
    // passed to OEMInterruptHandler.
    return 0;
 
#else

    return GetThreadIP(pCurThread);

#endif
}


//------------------------------------------------------------------------------
// ProfilerHit - OEMProfilerISR calls this routine to record profile hit
// 
// Input:  ra - interrupt return address
// 
// if buffer profiling, save ra and pCurProc->oe.tocptr in buffer
// else do symbol lookup in ISR
//------------------------------------------------------------------------------
void 
ProfilerHit(
    unsigned int ra
    ) 
{
#ifdef NKPROF

    OEMProfilerData data;

    data.ra = ra;
    // Process ID will be set inside ProfilerHitEx
    data.dwBufSize = 0;

    PROF_ProfilerHitEx(&data);

#endif // NKPROF
}


//------------------------------------------------------------------------------
// ProfilerHitEx - OEMProfilerISR calls this routine to record profile hit
// plus CPU-specific profiler data.
//
// Input:  pData - interrupt return address plus CPU-specific data
//------------------------------------------------------------------------------
void 
ProfilerHitEx(
    OEMProfilerData *pData
    ) 
{
#ifdef NKPROF
    PROF_ProfilerHitEx(pData);
#endif // NKPROF
}


//------------------------------------------------------------------------------
// ProfileSyscall - Turns profiler on and off.  The workhorse behind
// ProfileStart(), ProfileStartEx(), ProfileStop() and ProfileCaptureStatus().
//------------------------------------------------------------------------------
void 
NKProfileSyscall(
    const void* pBuffer,    // I_PTR, variable-sized struct
    DWORD cbControl
    )
{
#ifdef NKPROF

    // BUGBUG need a privilege check?
    PPROCESS pprc = SwitchActiveProcess(g_pprcNK);
    const ProfilerControl* pControl = (const ProfilerControl*) pBuffer;
    
    if (pControl
        && (pControl->dwVersion == 1)
        && (pControl->dwReserved == 0)
        && (cbControl >= sizeof(ProfilerControl))) {

        if (pControl->dwOptions & PROFILE_START) {
            PROF_ProfileStartEx(pControl);
        
        } else if (pControl->dwOptions & PROFILE_STOP) {
            PROF_ProfileStop(pControl);
        
        } else if (pControl->dwOptions & PROFILE_OEM_QUERY) {
            PROF_ProfileCaptureStatus(pControl);
        }
    }

    SwitchActiveProcess(pprc);

#else  // NKPROF

    RETAILMSG(1, (TEXT("IMGPROFILER is not set, kernprof.dll is not running!\r\n")));
    
#endif // NKPROF
}


//------------------------------------------------------------------------------
//  GetThreadName is used by CeLog on profiling builds.
//------------------------------------------------------------------------------
void
GetThreadName(
    PTHREAD pth,
    HANDLE* phModule,
    WCHAR*  pszThreadFunctionName
    )
{
#ifdef NKPROF

    PROF_GetThreadName(pth, phModule, pszThreadFunctionName);

#else  // NKPROF

    if (phModule != NULL) {
        *phModule = INVALID_HANDLE_VALUE;
    }
    if (pszThreadFunctionName != NULL) {
        pszThreadFunctionName[0] = 0;
    }

#endif // NKPROF
}


//------------------------------------------------------------------------------
// CeLog ReSync API: this code is here instead of logger.c because its KCALL
// profiling macros vary depending on the NKPROF flag.
//------------------------------------------------------------------------------
BOOL
NKCeLogReSync()
{
    BOOL fResult = FALSE;

    // KCall so nobody changes ProcArray or pModList out from under us
    if (!InSysCall()) {
        return KCall((PKFN)NKCeLogReSync);
    }
    
    KCALLPROFON(74);
    fResult = KCallCeLogReSync();
    KCALLPROFOFF(74);

    return fResult;
}

