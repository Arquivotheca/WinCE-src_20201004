//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
extern void * *Win32Methods;

#define IsInAllKMode   (IsThrdInKMode () && Win32Methods)

#define WIN32_CALL(type, api, args) (IsInAllKMode ? (*(type (*)args)(Win32Methods[W32_ ## api])) : (IMPLICIT_DECL(type, SH_WIN32, W32_ ## api, args)))
#define KILLTHREADIFNEEDED() {if (IsInAllKMode) (*(void (*)(void))(Win32Methods[151]))();}

#include <windows.h>
#include <coredll.h>
#define C_ONLY
#include "kernel.h"
#include "xtime.h"
#include "profiler.h"
#include "celogcoredll.h"


//---------------------------------------------------------------------------
// ProfileInit - Perform initialization tasks for profiling
//---------------------------------------------------------------------------
DWORD ProfileInit(void)
{
    return 0;
}


//---------------------------------------------------------------------------
// ProfileStart - clear profile counters and enable kernel profiler ISR
//
// INPUT:  dwUSecInterval - non-zero = sample interval in uSec
//                          zero     = reset uSec counter
//         dwOptions - PROFILE_* flags to control type of data collected and
//                     method of collection
//---------------------------------------------------------------------------
DWORD ProfileStart(DWORD dwUSecInterval, DWORD dwOptions)
{
    DWORD dwProfileLen[4];
    ProfilerControl control;

    // Block OEMDEFINED flag
    if (dwOptions & PROFILE_OEMDEFINED) {
        RETAILMSG(1, (TEXT("Cannot start OEM profiler with ProfileStart, use ProfileStartEx\r\n")));
        return 0;
    }

    control.dwVersion = 1;
    control.dwOptions = dwOptions;
    control.Kernel.dwUSecInterval = dwUSecInterval;

    // call ProfileSyscall with op=XTIME_PROFILE_DATA
    // and size=0 to clear counters and enable ISR
    dwProfileLen[0] = XTIME_PROFILE_DATA;
    dwProfileLen[1] = 0;
    dwProfileLen[2] = 0;
    dwProfileLen[3] = (DWORD)&control;
    ProfileSyscall(dwProfileLen);
    return 0;
}



//---------------------------------------------------------------------------
// ProfileStartEx - clear profile counters and enable kernel profiler ISR,
//                  or enable OEM-defined profiling
//
// INPUT:  pControl - Information about what profiler to use, what data to
//                    collect and how to collect it.
//---------------------------------------------------------------------------
DWORD ProfileStartEx(ProfilerControl *pControl)
{
    DWORD dwProfileLen[4];

    // call ProfileSyscall with op=XTIME_PROFILE_DATA
    // and size=0 to clear counters and enable ISR
    dwProfileLen[0] = XTIME_PROFILE_DATA;
    dwProfileLen[1] = 0;
    dwProfileLen[2] = 0;
    dwProfileLen[3] = (DWORD)pControl;
    ProfileSyscall(dwProfileLen);

    return 0;
}


//---------------------------------------------------------------------------
// ProfileStop - Stop profile and display hit report
//---------------------------------------------------------------------------
void ProfileStop(void)
{
    DWORD dwProfileLen[4];

    // call ProfileSyscall with op=XTIME_PROFILE_DATA
    // and size=1 to disable ISR and return profile data size
    dwProfileLen[0] = XTIME_PROFILE_DATA;
    dwProfileLen[1] = 1;
    ProfileSyscall(dwProfileLen);
    return;
}


//---------------------------------------------------------------------------
// ProfileCaptureStatus - Insert the current status of the OEM-defined profiler
//                        into the CeLog data stream
//---------------------------------------------------------------------------
VOID ProfileCaptureStatus()
{
    DWORD dwProfileLen[4];

    // call ProfileSyscall with op=XTIME_PROFILE_DATA
    // and size=3 to capture the profiler status
    dwProfileLen[0] = XTIME_PROFILE_DATA;
    dwProfileLen[1] = 2;
    ProfileSyscall(dwProfileLen);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
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
    if (IsCeLogLoaded()) {
        CeLogData(fTimeStamp, wID, pData, wLen, dwZoneUser, dwZoneCE,
                  wFlag, bFlagged);
        KILLTHREADIFNEEDED();
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID xxx_CeLogSetZones(
    DWORD dwZoneUser, 
    DWORD dwZoneCE, 
    DWORD dwZoneProcess)
{
    if (IsCeLogLoaded()) {
        CeLogSetZones(dwZoneUser, dwZoneCE, dwZoneProcess);
        KILLTHREADIFNEEDED();
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL xxx_CeLogGetZones(
    LPDWORD lpdwZoneUser,
    LPDWORD lpdwZoneCE,
    LPDWORD lpdwZoneProcess,
    LPDWORD lpdwAvailableZones)
{
    BOOL fResult = FALSE;

    if (IsCeLogLoaded()) {
        fResult = CeLogGetZones(lpdwZoneUser, lpdwZoneCE, lpdwZoneProcess,
                                lpdwAvailableZones);
        KILLTHREADIFNEEDED();
    }

    return fResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID xxx_CeLogReSync()
{
    if (IsCeLogLoaded()) {
        CeLogReSync();
        KILLTHREADIFNEEDED();
    }
}

