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

#include <windows.h>
#include <celog.h>
#include <profiler.h>
#include <pkfuncs.h>
#include "AppProfiler.h"
#include "AppProfKDll.h"


// Define values if building in pre-Seven environment
#ifndef CELID_APP_PROFILER_HIT
#define CELID_APP_PROFILER_HIT          118
typedef struct _CEL_APP_PROFILER_HIT {
    DWORD   ThreadId;
} CEL_APP_PROFILER_HIT, *PCEL_APP_PROFILER_HIT;
#endif


//
// All globals are stored inside one struct
//
static struct {
    // Profiling status
    HANDLE              hKDll;              // Handle of helper DLL loaded into the kernel
    HANDLE              hForceExit;         // Event used to tell the profiler thread to stop
    HANDLE              hReportExit;        // Event set when the profiler thread stops
    HANDLE              hThread;            // Profiler thread handle
    DWORD               period;             // Profiler sampling interval
    PROCESS_INFORMATION ProcInfo;           // If a target app was started, its info is here
} g_AppProfiler;


static DWORD WINAPI AppProfilerThread (LPVOID lpParameter)
{
    DWORD ZoneUser, ZoneCE, ZoneProcess;
#pragma warning(push)
#pragma warning(disable : 4815) // C4815: zero-sized array in stack object will have no elements (zero sized array is unused in this scenario)
    ProfilerControl control;
#pragma warning(pop)
    BYTE StackBuffer[APP_PROFILER_MAX_STACK_FRAME * sizeof(CallSnapshot)];
    

    // Enable the process, thread and module zones
    ZoneUser = 0;
    ZoneCE = 0;
    ZoneProcess = 0;
    CeLogGetZones (&ZoneUser, &ZoneCE, &ZoneProcess, NULL);
    CeLogSetZones (ZoneUser,
                   ZoneCE | CELZONE_THREAD | CELZONE_PROCESS | CELZONE_LOADER | CELZONE_RESCHEDULE,
                   ZoneProcess);
    CeLogReSync ();

    // Log a CeLog event marking the start of profiling
    control.dwVersion = 1;
    control.dwOptions = PROFILE_CELOG | PROFILE_CALLSTACK;
    control.Kernel.dwUSecInterval = 1000;
    CeLogData (TRUE, CELID_PROFILER_START, &control, sizeof(ProfilerControl),
               0, CELZONE_ALWAYSON, 0, FALSE);

    // Start the test process, if there is one
    if (g_AppProfiler.ProcInfo.hThread
        && !ResumeThread (g_AppProfiler.ProcInfo.hThread)) {
        RETAILMSG (1, (TEXT("!! ResumeThread failed (error=%u)\r\n"), GetLastError ()));
    } else {

        // To achieve a 1ms we can't use a normal wait -- use SleepTillTick
        if (1 == g_AppProfiler.period) {
            g_AppProfiler.period = 0;
        }
        
        // Loop until the event gets signaled or the process exits
        HANDLE WaitHandles[2] = { g_AppProfiler.hForceExit, g_AppProfiler.ProcInfo.hProcess };
        DWORD  NumHandles = g_AppProfiler.ProcInfo.hProcess ? 2 : 1;
        while (WAIT_TIMEOUT == WaitForMultipleObjects (NumHandles, WaitHandles, FALSE,
                                                       g_AppProfiler.period))
        {
            CEL_APP_PROFILER_HIT hit;
            
            if (0 == g_AppProfiler.period) {
                SleepTillTick ();
            }

            hit.ThreadId = 0;
            if (!KernelLibIoControl (g_AppProfiler.hKDll, IOCTL_KDLL_GET_PREV_THREAD, NULL, 0,
                                     &hit.ThreadId, sizeof(DWORD), NULL)) {
                RETAILMSG (1, (TEXT("!! IOCTL_KDLL_GET_PREV_THREAD failed (lasterror=%u)\r\n"),
                               GetLastError ()));
                break;
            }

            if ((DWORD)-1 != hit.ThreadId) {
                CeLogData (TRUE, CELID_APP_PROFILER_HIT, &hit, sizeof(CEL_APP_PROFILER_HIT),
                           0, CELZONE_ALWAYSON, 0, FALSE);
            }

            // Also log the callstack
            if (((DWORD)-1 != hit.ThreadId) && (0 != hit.ThreadId)) {
                // BUGBUG need to log extended snapshot (STACKSNAP_EXTENDED_INFO + CallSnapshotEx)
                DWORD dwSkip, dwNumFrames;

                // Iterate if the buffer is not large enough to hold the whole stack
                dwSkip = 0;
                do {
                    // Use the context to get the callstack
                    dwNumFrames = GetThreadCallStack ((HANDLE) hit.ThreadId,
                                                      APP_PROFILER_MAX_STACK_FRAME,
                                                      StackBuffer, STACKSNAP_FAIL_IF_INCOMPLETE,
                                                      dwSkip);

                    // Log the callstack to CeLog
                    if (dwNumFrames) {
                        CeLogData (FALSE, CELID_CALLSTACK, StackBuffer,
                                   (WORD) (dwNumFrames * sizeof(CallSnapshot)),
                                   0, CELZONE_ALWAYSON, 0, FALSE);
                        dwSkip += dwNumFrames;
                    }

                } while ((APP_PROFILER_MAX_STACK_FRAME == dwNumFrames)
                         && (ERROR_INSUFFICIENT_BUFFER == GetLastError ()));
            }
        }
    }

    // Log a profiler stop event
    CeLogData (TRUE, CELID_PROFILER_STOP, NULL, 0, 0, CELZONE_ALWAYSON, 0, FALSE);

    // Restore original zone settings
    CeLogSetZones (ZoneUser, ZoneCE, ZoneProcess);

    if (g_AppProfiler.hReportExit) {
        SetEvent (g_AppProfiler.hReportExit);
    }

    return 0;
}


extern "C" BOOL StartProfilerThread (
    DWORD   period,     // Interval between profiler hits (ms)
    LPCWSTR pAppName,   // Name of an application to start, or NULL.  If NULL, the profiler
                        // will keep running until APP_PROFILER_EVENT_NAME is signaled.
                        // If non-NULL, the profiler will start the application and run
                        // until either the application exits or APP_PROFILER_EVENT_NAME is
                        // signaled.
    LPCWSTR pAppParams, // Parameters to pass to pAppName, or NULL.
    HANDLE  hReportExit // Event for the profiler thread to set if it exits, or NULL.
                        // This event does not control thread behavior, only report it.
    )
{
    if (g_AppProfiler.hThread) {
        RETAILMSG (1, (TEXT("!! Profiler thread is already running\r\n")));
        return FALSE;
    }

    // 0 is not a valid period (Protect our SleepTillTick implementation)
    if (0 == period) {
        RETAILMSG (1, (TEXT("!! Invalid profiler period\r\n")));
        StopProfilerThread (TRUE);
        return FALSE;
    }

    g_AppProfiler.period = period;
    g_AppProfiler.hReportExit = hReportExit;

    g_AppProfiler.hKDll = LoadKernelLibrary (KDLL_NAME);
    if (!g_AppProfiler.hKDll) {
        RETAILMSG (1, (TEXT("!! LoadKernelLibrary(%s) failed (error=%u)\r\n"),
                       KDLL_NAME, GetLastError ()));
        StopProfilerThread (TRUE);
        return FALSE;
    }
    // Double check that it's accessible
    DWORD ignored;
    if (!KernelLibIoControl (g_AppProfiler.hKDll, IOCTL_KDLL_GET_PREV_THREAD, NULL, 0,
                             &ignored, sizeof(DWORD), NULL)) {
        RETAILMSG (1, (TEXT("!! IOCTL_KDLL_GET_PREV_THREAD failed (lasterror=%u)\r\n"),
                       GetLastError ()));
        StopProfilerThread (TRUE);
        return FALSE;
    }

    // Use a named event to signal that the profiler should quit
    g_AppProfiler.hForceExit = CreateEvent (NULL, FALSE, FALSE, APP_PROFILER_EVENT_NAME);
    if (!g_AppProfiler.hForceExit) {
        RETAILMSG (1, (TEXT("!! CreateEvent failed (error=%u)\r\n"), GetLastError ()));
        StopProfilerThread (TRUE);
        return FALSE;
    }
        
    // Prepare the profiler thread, but don't start it yet
    g_AppProfiler.hThread = CreateThread (NULL, 0, AppProfilerThread, NULL,
                                          CREATE_SUSPENDED, NULL);
    if (!g_AppProfiler.hThread) {
        RETAILMSG (1, (TEXT("!! CreateThread failed (error=%u)\r\n"), GetLastError ()));
        StopProfilerThread (TRUE);
        return FALSE;
    }
    // Raise to real-time priority so that this thread runs reliably
    if (!CeSetThreadPriority (g_AppProfiler.hThread, 247)) {
        RETAILMSG (1, (TEXT("!! CeSetThreadPriority failed (error=%u)\r\n"), GetLastError ()));
        StopProfilerThread (TRUE);
        return FALSE;
    }

    // Prepare the test process, but don't start it yet
    if (pAppName
        && !CreateProcess (pAppName, pAppParams, NULL, NULL, FALSE,
                           CREATE_SUSPENDED, NULL, NULL, NULL,
                           &g_AppProfiler.ProcInfo)) {
        RETAILMSG (1, (TEXT("!! Unable to create process %s (error=%u)\r\n"),
                       pAppName, GetLastError ()));
        StopProfilerThread (TRUE);
        return FALSE;
    }

    // Now start the profiler thread.  It'll start the test process if necessary.
    if (!ResumeThread (g_AppProfiler.hThread)) {
        RETAILMSG (1, (TEXT("!! ResumeThread failed (error=%u)\r\n"), GetLastError ()));
        StopProfilerThread (TRUE);
        return FALSE;
    }

    return TRUE;
}


BOOL StopProfilerThread (
    BOOL fForceThreadToStop // TRUE: stop immediately, FALSE: wait for it to finish
                            // profiling first
    )
{
    // Will return failure if profiler thread is not running
    BOOL result = (g_AppProfiler.hThread) ? TRUE : FALSE;
    
    if (g_AppProfiler.hThread) {
        ResumeThread (g_AppProfiler.hThread);
        if (fForceThreadToStop) {
            SetEvent (g_AppProfiler.hForceExit);
        }
        WaitForSingleObject (g_AppProfiler.hThread, INFINITE);

        CloseHandle (g_AppProfiler.hThread);
        g_AppProfiler.hThread = NULL;
    
    } else if (g_AppProfiler.hReportExit) {
        // The thread doesn't exist to report profiler exit, so do it here
        SetEvent (g_AppProfiler.hReportExit);
    }

    if (g_AppProfiler.ProcInfo.hProcess) {
        CloseHandle (g_AppProfiler.ProcInfo.hProcess);
        g_AppProfiler.ProcInfo.hProcess = NULL;
    }
    if (g_AppProfiler.ProcInfo.hThread) {
        CloseHandle (g_AppProfiler.ProcInfo.hThread);
        g_AppProfiler.ProcInfo.hThread = NULL;
    }

    if (g_AppProfiler.hForceExit) {
        CloseHandle (g_AppProfiler.hForceExit);
        g_AppProfiler.hForceExit = NULL;
    }
    if (g_AppProfiler.hKDll) {
        // Can't really unload the kernel DLL but let's try anyway
        FreeLibrary ((HMODULE) g_AppProfiler.hKDll);
        g_AppProfiler.hKDll = NULL;
    }

    return result;
}
