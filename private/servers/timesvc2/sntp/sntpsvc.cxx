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
/*--
Module Name: svc.cxx
Abstract: service core
--*/
#include <windows.h>
#include <stdio.h>

#include <svsutil.hxx>
#include <service.h>
#include "sntp.h"

#if defined (DEBUG) || defined (_DEBUG)
DBGPARAM dpCurSettings = 
{
    TEXT("SNTPSVC"), 
    {
        TEXT("INIT"),
        TEXT("SERVER"),       
        TEXT("CLIENT"),
        TEXT("PACKETS"),
        TEXT("TRACE"),
        TEXT("DST"),
        TEXT("<unused>"),
        TEXT("<unused>"),
        TEXT("<unused>"),
        TEXT("<unused>"),
        TEXT("<unused>"),       
        TEXT("<unused>"),
        TEXT("<unused>"),       
        TEXT("<unused>"),
        TEXT("Warning"),
        TEXT("Error"),
    },

    0xc001
}; 

#endif  // DEBUG

STDAPI_(BOOL) WINAPI DllEntry (HINSTANCE hinstDLL, DWORD   Op, LPVOID  lpvReserved) {
    switch (Op) {
    case DLL_PROCESS_ATTACH :
        DisableThreadLibraryCalls((HMODULE)hinstDLL);
        if (ERROR_SUCCESS != InitializeSNTP (hinstDLL))
            return FALSE;

        break;

    case DLL_PROCESS_DETACH :
        DestroySNTP ();
        break;
    }

    return TRUE;
}

//  @func PVOID | NTP_Init | Service initialization routine
//  @parm DWORD | dwData | Info passed to RegisterDevice
//  @rdesc      Returns a DWORD which will be passed to Open & Deinit or NULL if
//              unable to initialize the device.
//
//  This is called only once. Do the startup initialization in a thread
//  spawned by this function, but DO NOT BLOCK HERE!
//        
extern "C" DWORD NTP_Init (DWORD dwData) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] NTP_Init\r\n"));

    if (! (dwData & SERVICE_INIT_STOPPED)) {
        if (StartSNTP () != ERROR_SUCCESS)
            return FALSE;
    }

    return TRUE;
}

//  @func PVOID | NTP_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from HLO_Init call
//  @rdesc  Returns TRUE for success, FALSE for failure.
//
//  The library WILL BE UNLOADED after this. Block here
//  until the state is completely clear and all the
//  threads are gone.
//
extern "C" BOOL NTP_Deinit(DWORD dwData) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] NTP_Deinit\r\n"));

    StopSNTP ();

    return TRUE;
}

//  @func BOOL   | NTP_IOControl | Device IO control routine
//  @parm DWORD  | dwOpenData | value returned from HLO_Open call
//  @parm DWORD  | dwCode | io control code to be performed
//  @parm PBYTE  | pBufIn | input data to the device
//  @parm DWORD  | dwLenIn | number of bytes being passed in
//  @parm PBYTE  | pBufOut | output data from the device
//  @parm DWORD  | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device//  @rdesc         Returns TRUE for success, FALSE for failure
//  @remark Routine exported by a device driver.  "PRF" is the string passed
//      in as lpszType in RegisterDevice
//
//  This is THE way to expose both manageability of the service and the feature API set.
//  Consumer of the API set will marshal input arguments into input buffer (pBufIn).
//  This function unmarshals it and executes the API, and then marshals output parameters
//  into output buffer (pBufOut).
//
extern "C" BOOL NTP_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn,
              DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] NTP_IOControl 0x%08x\r\n", dwCode));

    int iErr  = ERROR_INVALID_PARAMETER;
    switch (dwCode) {
        case IOCTL_SERVICE_START: // start a service that is currently in the stopped stage.
            iErr = StartSNTP ();
        break;

        case IOCTL_SERVICE_STOP: // start a service that is currently in the stopped stage.
            iErr = StopSNTP ();
            break;

        case IOCTL_SERVICE_REFRESH:
            iErr = RefreshSNTP ();
            break;

        case IOCTL_SERVICE_STATUS:
            if (pBufOut && (dwLenOut == sizeof(DWORD)))  {
                DWORD dwState = GetStateSNTP ();
                __try {
                    *(DWORD *)pBufOut = dwState;

                    if (pdwActualOut)
                        *pdwActualOut = sizeof(DWORD);

                        iErr = ERROR_SUCCESS;
                } __except (1) {
                    iErr = ERROR_INVALID_PARAMETER;
                }
            } else {
                iErr = ERROR_INVALID_PARAMETER;
            }
        break;

        case IOCTL_SERVICE_CONTROL:
            {
            __try {
                int fProcessed = FALSE;

                iErr = ServiceControlSNTP (pBufIn, dwLenIn, pBufOut, dwLenOut, pdwActualOut, &fProcessed);
                if (fProcessed)
                    break;

                iErr = ERROR_INVALID_PARAMETER;
            } __except (1) {
                iErr = ERROR_INVALID_PARAMETER;
            }
            }
            break;

        case IOCTL_SERVICE_DEBUG:
#if defined (DEBUG) || defined (_DEBUG)
            if ((dwLenIn == sizeof(DWORD)) && pBufIn) {
                dpCurSettings.ulZoneMask = (*(DWORD *)pBufIn);
                iErr = ERROR_SUCCESS;
            } else
                iErr = ERROR_INVALID_PARAMETER;
#endif
        break;
    }

    SetLastError(iErr);
    return iErr == ERROR_SUCCESS;
}

extern "C" DWORD NTP_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode)
{
	DEBUGMSG (ZONE_TRACE, (TEXT("NTP_Open(0x%X, 0x%X, 0x%X)\r\n"), dwData, dwAccess, dwShareMode));

	return (DWORD)TRUE;;
}

extern "C" BOOL NTP_Close (DWORD dwData) 
{
	DEBUGMSG (ZONE_TRACE, (TEXT("NTP_Close(0x%X)\r\n"), dwData));

	return TRUE;
}