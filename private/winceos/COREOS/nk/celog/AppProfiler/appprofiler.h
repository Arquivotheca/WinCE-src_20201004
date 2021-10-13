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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#define APP_PROFILER_EVENT_NAME        TEXT("App Profiler Exit Event")
#define APP_PROFILER_DEFAULT_PERIOD    1
#define APP_PROFILER_MAX_STACK_FRAME   50


BOOL StartProfilerThread (
    DWORD   period,     // Interval between profiler hits (ms)
    LPCWSTR pAppName,   // Name of an application to start, or NULL.  If NULL, the profiler
                        // will keep running until APP_PROFILER_EVENT_NAME is signaled.
                        // If non-NULL, the profiler will start the application and run
                        // until either the application exits or APP_PROFILER_EVENT_NAME is
                        // signaled.
    LPCWSTR pAppParams, // Parameters to pass to pAppName, or NULL.
    HANDLE  hReportExit // Event for the profiler thread to set if it exits, or NULL.
                        // This event does not control thread behavior, only report it.
    );

BOOL StopProfilerThread (
    BOOL fForceThreadToStop // TRUE: stop immediately, FALSE: wait for it to finish
                            // profiling first
    );


#ifdef __cplusplus
}
#endif
