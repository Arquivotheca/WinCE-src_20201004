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
// PROFILER INTERFACE
//
// This file implements the kernel profiler support that is always included in
// the kernel, not just IMGPROFILER=1 builds.  
//
// Non-IMGPROFILER images require PROFILE_CELOG; buffered and unbuffered
// profiling are only supported on IMGPROFILER images.  Monte Carlo profiling,
// system call profiling, and OEM-defined profiling are all supported on
// non-IMGPROFILER builds.  KCall profiling requires IMGPROFILER.
//
//------------------------------------------------------------------------------

#define C_ONLY
#include "kernel.h"
#include <profiler.h>


ProfilerState g_ProfilerState;
DWORD g_ProfilerState_dwProfilerIntsInTLB;
PFN_ProfileTimerEnable  pfnProfileTimerEnable;
PFN_ProfileTimerDisable pfnProfileTimerDisable;

// These values can be overwritten as FIXUPVARs in config.bib
const volatile DWORD dwProfileBufferMax        = 0;
const volatile DWORD dwProfileBootUSecInterval = 0;
const volatile DWORD dwProfileBootOptions      = 0;

// On NKPROF builds PROFILEMSG always prints, on regular NK it only prints on non-ship
#ifdef NKPROF
#define PROFILEMSG(cond,printf_exp) ((cond)?(NKDbgPrintfW printf_exp),1:0)
#else // NKPROF
#define PROFILEMSG RETAILMSG
#endif // NKPROF

static DWORD GetTLBMissCount (void)
{
    DWORD dwTlbMiss = 0;
#if defined(MIPS) || defined(SHx)
    // NOTE: we don't take any spinlock here. The count is a snapshot and
    //       it's okay that it got incremented again after we read.
    DWORD idx;
    for (idx = 0; idx < g_pKData->nCpus; idx ++) {
        dwTlbMiss += g_ppcbs[idx]->dwTlbMissCnt;
    }
#endif
    return dwTlbMiss;
}


//------------------------------------------------------------------------------
//  GetEPC - Get exception program counter
//
//  Output: returns EPC, zero if cpu not supported
//------------------------------------------------------------------------------
DWORD GetEPC(void)
{
#if defined(x86)

    extern PTHREAD pthFakeStruct;
    PTHREAD pTh;

    if (GetPCB ()->cNest < 0) {
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
// Default profiler implementation using the timer-tick, used if the OAL does
// not implement a higher resolution profiler (pfnProfileTimerEnable and
// pfnProfileTimerDisable from OEMGLOBAL).
//------------------------------------------------------------------------------

static void
NKProfileTimerEnable (DWORD dwUSec)
{
    // kdata->dwCeLogStatus must be nonzero for CELOG_Interrupt to
    // be called, but if CeLog is not actually running, just turn on a zone
    if (!IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        CeLogEnable(0, CELZONE_ALWAYSON);
    }

    g_ProfilerState.dwProfilerFlags |= PROFILE_TICK;
}

static void
NKProfileTimerDisable (void)
{
    g_ProfilerState.dwProfilerFlags &= ~PROFILE_TICK;

    if (!IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        CeLogDisable();
    }
}


//------------------------------------------------------------------------------
// This is a dummy function for the profiler
//------------------------------------------------------------------------------
static void
IDLE_STATE()
{
    //
    // Force the function to be non-empty so the hit engine can find it.
    //
    static volatile DWORD dwVal;
    dwVal++;
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
    OEMProfilerData data;

    data.ra = ra;
    // Process ID will be set inside ProfilerHitEx
    data.dwBufSize = 0;

    ProfilerHitEx(&data);
}


//------------------------------------------------------------------------------
// ProfilerHitEx - Generalized version of ProfilerHit.  A hardware-specific
// profiler ISR calls this routine to record a buffer of profiling data.
// Input:  pData - OEM-specified buffer of profiling information (RA may be fixed up)
//------------------------------------------------------------------------------
void 
ProfilerHitEx(
    OEMProfilerData *pData
    )
{
    if (g_ProfilerState.dwProfilerFlags & PROFILE_CELOG) {
        BOOL fIdle = !GetPCB ()->pthSched && !RunList.pHead;
        if (fIdle) {  // IDLE
            g_ProfilerState.dwSamplesInIdle++;
            pData->ra = (DWORD) IDLE_STATE;
        }
        if (IsCeLogStatus(CELOGSTATUS_ENABLED_PROFILE)) {

            pData->dwProcessId = pActvProc->dwId;

            // Send data to celog
            if (g_ProfilerState.dwProfilerFlags & PROFILE_OEMDEFINED) {
                const DWORD dwHeaderSize = sizeof(pData->ra) + sizeof(pData->dwProcessId) + sizeof(pData->dwBufSize);
                g_pfnCeLogInterruptData(FALSE, CELID_OEMPROFILER_HIT, pData,
                                        pData->dwBufSize + dwHeaderSize);  // dwBufSize does not include header; can't use sizeof(OEMProfilerData) because the 0-byte array has nonzero size
            } else {
                // CEL_MONTECARLO_HIT is the first two DWORDs of OEMProfilerData
                g_pfnCeLogInterruptData(FALSE, CELID_MONTECARLO_HIT, pData,
                                        sizeof(CEL_MONTECARLO_HIT));
            }
        
#if defined(x86) && defined(NKPROF)
            // Callstack capture is only supported on x86 IMGPROFILER builds right now
            // Send call stack data if necessary
            if ((g_ProfilerState.dwProfilerFlags & PROFILE_CALLSTACK)
                && !fIdle                               // Not IDLE_STATE - idle has no stack
                && !(GetPCB()->cNest < 0)) {            // Not a nested interrupt or KCall - no way to get stack in those cases?
                PROF_LogIntCallStack();
            }
#endif // defined(x86) && defined(NKPROF)
        }
        
    } else {

#ifdef NKPROF
        // Profiling with CeLog is always possible, but buffered/unbuffered
        // are only possible on IMGPROFILER builds.
        PROF_ClassicProfilerHit(pData);
#endif // NKPROF
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
CommonProfileStart(
    const ProfilerControl* pControl,    // variable-sized struct, IN only
    DWORD dwControlSize
    )
{
    // Determine which type of data is being gathered
    if (pControl->dwOptions & PROFILE_OEMDEFINED) {
        // OEM-specific profiler
        if ((pControl->dwOptions & (PROFILE_OBJCALL | PROFILE_KCALL))
            || !OEMIoControl (IOCTL_HAL_OEM_PROFILER,
                             (ProfilerControl*) pControl,  // non-const
                             dwControlSize,
                             NULL, 0, NULL)) {
            PROFILEMSG(1, (TEXT("Kernel Profiler: OEM Profiler start failed!\r\n")));
            SetLastError(ERROR_NOT_SUPPORTED);
            return FALSE;
        }
        g_ProfilerState.dwProfilerFlags |= PROFILE_OEMDEFINED;

    } else if (pControl->dwOptions & PROFILE_OBJCALL) {
        // Object call profiling
        g_ProfilerState.dwProfilerFlags |= PROFILE_OBJCALL;

    } else if (pControl->dwOptions & PROFILE_KCALL) {
        // Acquire Lock so profiler won't start in between of KCALLPROFON/OFF
        AcquireSpinLock (&g_schedLock);
        // Kernel call profiling
        g_ProfilerState.dwProfilerFlags |= PROFILE_KCALL;
        ReleaseSpinLock (&g_schedLock);

    } else {
        // Monte Carlo profiling

        // Initialize the system state counters that will not run
        // whenever profiling is paused
        g_ProfilerState.dwInterrupts = 0;
        g_ProfilerState_dwProfilerIntsInTLB = 0;

        // Initialize the profiler timer control handlers.
        if (g_pOemGlobal->pfnProfileTimerEnable && g_pOemGlobal->pfnProfileTimerDisable
            && !(pControl->dwOptions & PROFILE_TICK)) {
            PROFILEMSG(1, (TEXT("Using OAL profiler implementation\r\n")));
            pfnProfileTimerEnable = g_pOemGlobal->pfnProfileTimerEnable;
            pfnProfileTimerDisable = g_pOemGlobal->pfnProfileTimerDisable;
        } else {
            PROFILEMSG(!g_pOemGlobal->pfnProfileTimerEnable && !g_pOemGlobal->pfnProfileTimerDisable,
                (TEXT("Profiling is not implemented in the OAL\r\n")));
            PROFILEMSG(1, (TEXT("Using kernel timer-tick (SYSINTR_NOP) profiler interrupt implementation\r\n")));
            pfnProfileTimerEnable = NKProfileTimerEnable;
            pfnProfileTimerDisable = NKProfileTimerDisable;
        }

        if (pControl->dwOptions & PROFILE_STARTPAUSED) {
            // Set elapsed values for system counters that will
            // continue to run whenever profiling is paused
            // (paused now)
            g_ProfilerState.dwTLBCount  = 0;
            g_ProfilerState.dwTickCount = 0;
        } else {
            // Set start values for system counters that will
            // continue to run whenever profiling is paused
            // (running now)
            g_ProfilerState.dwTLBCount  = GetTLBMissCount ();
            g_ProfilerState.dwTickCount = OEMGetTickCount();

            // Start profiler timer
            pfnProfileTimerEnable(pControl->Kernel.dwUSecInterval);

            g_ProfilerState.bProfileTimerRunning = TRUE;
        }
    }

    g_ProfilerState.bStart = TRUE;
    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
CommonProfileStop(
    const ProfilerControl* pControl,    // variable-sized struct, IN only
    DWORD dwControlSize
    )
{
    if (g_ProfilerState.bProfileTimerRunning) {
        // Disable profiler timer
        if (pfnProfileTimerDisable) {
            pfnProfileTimerDisable();
        }

        // Set system state counters to paused (elapsed) values
        g_ProfilerState.dwTLBCount  = GetTLBMissCount () - g_ProfilerState.dwTLBCount;
        g_ProfilerState.dwTickCount = OEMGetTickCount() - g_ProfilerState.dwTickCount;

    } else if (g_ProfilerState.dwProfilerFlags & PROFILE_OEMDEFINED) {
        // OEM-specific profiler
        ProfilerControl control;
        control.dwVersion = 1;
        control.dwOptions = PROFILE_STOP | PROFILE_OEMDEFINED;
        control.dwReserved = 0;
        control.OEM.dwControlSize = 0;    // no OEM struct passed to stop profiler
        control.OEM.dwProcessorType = 0;  // not yet defined
        OEMIoControl (IOCTL_HAL_OEM_PROFILER, &control,
                     sizeof(ProfilerControl), NULL, 0, NULL);
    }

    g_ProfilerState.bProfileTimerRunning = FALSE;
    g_ProfilerState.bStart = FALSE;
    g_ProfilerState.scPauseContinueCalls = 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
CommonProfileContinue(
    const ProfilerControl* pControl,    // variable-sized struct, IN only
    DWORD dwControlSize
    )
{
    ++g_ProfilerState.scPauseContinueCalls;

    // Start profiler timer on 0 to 1 transition
    if (1 == g_ProfilerState.scPauseContinueCalls) {
        if (g_ProfilerState.dwProfilerFlags & PROFILE_OEMDEFINED) {
            // OEM-specific profiler
            OEMIoControl (IOCTL_HAL_OEM_PROFILER,
                         (ProfilerControl*) pControl,  // non-const
                         dwControlSize,
                         NULL, 0, NULL);

        } else {
            // Monte Carlo profiling

            // Set system state counters to running (total) values
            g_ProfilerState.dwTLBCount  = GetTLBMissCount () - g_ProfilerState.dwTLBCount;
            g_ProfilerState.dwTickCount = OEMGetTickCount() - g_ProfilerState.dwTickCount;

            if (pfnProfileTimerEnable) {
                pfnProfileTimerEnable(pControl->Kernel.dwUSecInterval);
            }
            g_ProfilerState.bProfileTimerRunning = TRUE;
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
CommonProfilePause(
    const ProfilerControl* pControl,    // variable-sized struct, IN only
    DWORD dwControlSize
    )
{
    --g_ProfilerState.scPauseContinueCalls;

    // Stop profiler timer on 1 to 0 transition
    if (!g_ProfilerState.scPauseContinueCalls) {
        if (g_ProfilerState.dwProfilerFlags & PROFILE_OEMDEFINED) {
            // OEM-specific profiler
            OEMIoControl (IOCTL_HAL_OEM_PROFILER,
                         (ProfilerControl*) pControl,  // non-const
                         dwControlSize,
                         NULL, 0, NULL);
        } else {
            // Monte Carlo profiling
            if (pfnProfileTimerDisable) {
                pfnProfileTimerDisable();
            }
            g_ProfilerState.bProfileTimerRunning = FALSE;

            // Set system state counters to paused (elapsed) values
            g_ProfilerState.dwTLBCount  = GetTLBMissCount () - g_ProfilerState.dwTLBCount;
            g_ProfilerState.dwTickCount = OEMGetTickCount() - g_ProfilerState.dwTickCount;
        }
    }
}


//------------------------------------------------------------------------------
// Resulting state is in g_ProfilerState.bStart.
//------------------------------------------------------------------------------
static void
CeLogProfileStart(
    const ProfilerControl* pControl,    // variable-sized struct, IN only
    DWORD dwControlSize
    )
{
    // KCall profiling does not combine with CeLog and should not reach here
    DEBUGCHK(!(pControl->dwOptions & PROFILE_KCALL));

    if (!IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        PROFILEMSG(1, (TEXT("Kernel Profiler: Unable to start, CeLog is not loaded!\r\n")));
        return;
    }

    CommonProfileStop(pControl, dwControlSize);

    // Disable CeLog general logging; log only profile events
    CeLogEnableStatus(CELOGSTATUS_ENABLED_PROFILE);

    // Make sure the correct zones are turned on; save the
    // old zone settings to restore later
    NKCeLogGetZones(&g_ProfilerState.SavedCeLogZones.dwZoneUser,
                    &g_ProfilerState.SavedCeLogZones.dwZoneCE,
                    &g_ProfilerState.SavedCeLogZones.dwZoneProcess, NULL);
    NKCeLogSetZones((DWORD)-1, CELZONE_PROFILER, (DWORD)-1);
    NKCeLogReSync();

    // Log the start event
    if (pControl->dwOptions & PROFILE_OEMDEFINED) {
        g_pfnCeLogData(TRUE, CELID_PROFILER_START, pControl,
                       dwControlSize, 0,
                       CELZONE_PROFILER, 0, FALSE);
    } else {
        g_pfnCeLogData(TRUE, CELID_PROFILER_START, pControl,
                       sizeof(ProfilerControl), 0,
                       CELZONE_PROFILER, 0, FALSE);
    }

    g_ProfilerState.dwProfilerFlags |= PROFILE_CELOG;

#if defined(x86)  // Callstack capture is only supported on x86 right now

    // CALLSTACK flag can only be used with CeLog & Monte Carlo right now
    if ((pControl->dwOptions & PROFILE_CALLSTACK)
        && !(pControl->dwOptions & PROFILE_OBJCALL)) {

        g_ProfilerState.dwProfilerFlags |= PROFILE_CALLSTACK;

        // INPROC flag modifies the callstack flag
        if (pControl->dwOptions & PROFILE_CALLSTACK_INPROC) {
            g_ProfilerState.dwProfilerFlags |= PROFILE_CALLSTACK_INPROC;
        }
    }

#endif // defined(x86)

    if (!CommonProfileStart(pControl, dwControlSize)) {
        // Restore CeLog state
        CeLogEnableStatus(CELOGSTATUS_ENABLED_GENERAL);

        // Restore the original zone settings AFTER returning to
        // general logging, or else CeLogSetZones will force the
        // profiler zone to be turned on
        NKCeLogSetZones(g_ProfilerState.SavedCeLogZones.dwZoneUser,
                        g_ProfilerState.SavedCeLogZones.dwZoneCE,
                        g_ProfilerState.SavedCeLogZones.dwZoneProcess);
        
        g_ProfilerState.dwProfilerFlags = 0;
        g_ProfilerState.bStart = FALSE;
    }
}


//------------------------------------------------------------------------------
// Resulting state is in g_ProfilerState.bStart.
//------------------------------------------------------------------------------
static void
CeLogProfileStop(
    const ProfilerControl* pControl,    // variable-sized struct, IN only
    DWORD dwControlSize
    )
{
    // Resume general logging
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        // Log the stop event
        g_pfnCeLogData(TRUE, CELID_PROFILER_STOP, NULL, 0, 0,
                       CELZONE_PROFILER, 0, FALSE);

        // Restore CeLog general logging
        CeLogEnableStatus(CELOGSTATUS_ENABLED_GENERAL);

        // Restore the original zone settings AFTER returning to
        // general logging, or else CeLogSetZones will force the
        // profiler zone to be turned on
        NKCeLogSetZones(g_ProfilerState.SavedCeLogZones.dwZoneUser,
                        g_ProfilerState.SavedCeLogZones.dwZoneCE,
                        g_ProfilerState.SavedCeLogZones.dwZoneProcess);
    }
    g_ProfilerState.dwProfilerFlags &= ~(PROFILE_CELOG | PROFILE_CALLSTACK | PROFILE_CALLSTACK_INPROC);

    PROFILEMSG(1, (TEXT("Profiler data written to CeLog.\r\n")));
    PROFILEMSG(1, (TEXT("MODULES:  Written to CeLog\r\n")));
    PROFILEMSG(1, (TEXT("SYMBOLS:  Written to CeLog\r\n")));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void 
NKProfileStartEx(
    const ProfilerControl* pControl,    // variable-sized struct, IN only
    DWORD dwControlSize
    )
{
    if (pControl->dwOptions & PROFILE_CONTINUE) {
        if (g_ProfilerState.bStart && ((g_ProfilerState.dwProfilerFlags & (PROFILE_OBJCALL | PROFILE_KCALL)) == 0)) {
            CommonProfileContinue(pControl, dwControlSize);
        }

    } else if (pControl->dwOptions & PROFILE_PAUSE) {
        if (g_ProfilerState.bStart && ((g_ProfilerState.dwProfilerFlags & (PROFILE_OBJCALL | PROFILE_KCALL)) == 0)) {
            CommonProfilePause(pControl, dwControlSize);
        }

    } else {
        // Protect against multiple starts
        if (g_ProfilerState.bStart) {
            PROFILEMSG(1, (TEXT("Kernel Profiler: Ignoring multiple profiler starts\r\n")));
            return;
        }

        // Special case: KCall and CeLog don't work together
        if ((pControl->dwOptions & PROFILE_CELOG) && (pControl->dwOptions & PROFILE_KCALL)) {
            PROFILEMSG(1, (TEXT("Kernel Profiler: KCall profiling cannot work with CeLog, failing\r\n")));
            return;
        }

        if (pControl->dwOptions & PROFILE_CELOG) {
            CeLogProfileStart(pControl, dwControlSize);
        } else {
#ifdef NKPROF
            PROF_ClassicProfileStart(pControl, dwControlSize);
#else  // NKPROF

            RETAILMSG(1, (TEXT("IMGPROFILER is not set, kernprof.dll is not running!\r\n")));
            RETAILMSG(1, (TEXT("Without IMGPROFILER, you can only use the kernel profiler in combination with CeLog.\r\n")));

#endif // NKPROF
        }

        // Debug output so the user knows exactly what's going on
        if (g_ProfilerState.bStart) {
            if (pControl->dwOptions & PROFILE_KCALL) {
                PROFILEMSG(1, (TEXT("Kernel Profiler: Gathering KCall data\r\n")));
            } else {
                PROFILEMSG(1, (TEXT("Kernel Profiler: Gathering %s data in %s mode\r\n"),
                               (pControl->dwOptions & PROFILE_OEMDEFINED) ? TEXT("OEM-Defined")
                                   : (pControl->dwOptions & PROFILE_OBJCALL) ? TEXT("ObjectCall")
                                   : TEXT("MonteCarlo"),
                               (pControl->dwOptions & PROFILE_CELOG) ? TEXT("CeLog")
                                   : ((pControl->dwOptions & PROFILE_BUFFER) ? TEXT("buffered")
                                   : TEXT("unbuffered"))));
            }
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void 
NKProfileStop(
    const ProfilerControl* pControl,    // variable-sized struct, IN only
    DWORD dwControlSize
    )
{
    CommonProfileStop(pControl, dwControlSize);

    if (g_ProfilerState.dwProfilerFlags & PROFILE_CELOG) {
        CeLogProfileStop(pControl, dwControlSize);
    } else {
#ifdef NKPROF
        PROF_ClassicProfileStop(pControl, dwControlSize);
#endif // NKPROF
    }

    g_ProfilerState.dwProfilerFlags = 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void 
NKProfileCaptureStatus()
{
    // Insert the current state of the OEM-defined profiler into the
    // CeLog data stream
    union {
        OEMProfilerData data;
        BYTE _b[1024];
    } buf; // Fixed-size stack buffer for capturing data
    DWORD dwBufUsed = 0;

    // Only supported via CeLog for now
    if ((g_ProfilerState.dwProfilerFlags & PROFILE_OEMDEFINED)
        && (g_ProfilerState.dwProfilerFlags & PROFILE_CELOG)
        && IsCeLogStatus(CELOGSTATUS_ENABLED_PROFILE)) {

        ProfilerControl control;
        control.dwVersion = 1;
        control.dwOptions = PROFILE_OEM_QUERY | PROFILE_OEMDEFINED;
        control.dwReserved = 0;
        control.OEM.dwControlSize = 0;    // no OEM struct being passed here
        control.OEM.dwProcessorType = 0;  // not yet defined
        if (OEMIoControl (IOCTL_HAL_OEM_PROFILER, &control,
                         sizeof(ProfilerControl), &buf.data, sizeof(buf),
                         &dwBufUsed)) {

            // Clear the RA since it does not apply when the data is just being queried
            buf.data.ra = 0;

            // Now that we have the data, send it to CeLog
            g_pfnCeLogData(FALSE, CELID_OEMPROFILER_HIT, &buf.data, dwBufUsed, 0,
                           CELZONE_PROFILER, 0, FALSE);
        }
    }
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
    PPROCESS pprc = SwitchActiveProcess(g_pprcNK);
    const ProfilerControl* pControl = (const ProfilerControl*) pBuffer;
    
    if (pControl
        && (pControl->dwVersion == 1)
        && (pControl->dwReserved == 0)
        && (cbControl >= sizeof(ProfilerControl))) {

        DWORD dwControlSize = sizeof(ProfilerControl);
        if (pControl->dwOptions & PROFILE_OEMDEFINED) {
            dwControlSize += pControl->OEM.dwControlSize;
            if ((dwControlSize < sizeof(ProfilerControl))
                || (dwControlSize < pControl->OEM.dwControlSize)) {
                return;
            }
        }
        if (dwControlSize > cbControl) {
            return;
        }

        if (pControl->dwOptions & PROFILE_START) {
            NKProfileStartEx(pControl, dwControlSize);
        
        } else if (pControl->dwOptions & PROFILE_STOP) {
            NKProfileStop(pControl, dwControlSize);
        
        } else if (pControl->dwOptions & PROFILE_OEM_QUERY) {
            NKProfileCaptureStatus();
        }
    }

    SwitchActiveProcess(pprc);
}

void 
EXTProfileSyscall(
    const void* pBuffer,    // I_PTR, variable-sized struct
    DWORD cbControl
    )
{
    if (pBuffer) {
        void* pBufferCopy = NKmalloc (cbControl);
        if (pBufferCopy) {
            __try {
                memcpy (pBufferCopy, pBuffer, cbControl);
                NKProfileSyscall (pBufferCopy, cbControl);
            } __finally {
                NKfree (pBufferCopy);
            }
        }
    }
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

