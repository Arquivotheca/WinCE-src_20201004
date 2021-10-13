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
/*--
Module Name: timesvc.cxx
Abstract: service core
--*/
#include <windows.h>
#include <stdio.h>

#include <svsutil.hxx>
#include <service.h>

#include "../inc/timesvc.h"

#if defined (DEBUG) || defined (_DEBUG)
DBGPARAM dpCurSettings = 
{
    TEXT("TIMESVC"), 
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

#endif	// DEBUG

static DWORD gStateMap[6][6] = {
    {SERVICE_STATE_OFF, SERVICE_STATE_UNKNOWN, SERVICE_STATE_STARTING_UP, SERVICE_STATE_SHUTTING_DOWN, SERVICE_STATE_UNLOADING, SERVICE_STATE_UNLOADING},
    {SERVICE_STATE_UNKNOWN, SERVICE_STATE_ON, SERVICE_STATE_STARTING_UP, SERVICE_STATE_SHUTTING_DOWN, SERVICE_STATE_UNLOADING, SERVICE_STATE_UNLOADING},
    {SERVICE_STATE_STARTING_UP, SERVICE_STATE_STARTING_UP, SERVICE_STATE_STARTING_UP, SERVICE_STATE_UNKNOWN, SERVICE_STATE_UNLOADING, SERVICE_STATE_UNLOADING},
    {SERVICE_STATE_SHUTTING_DOWN, SERVICE_STATE_SHUTTING_DOWN, SERVICE_STATE_UNKNOWN, SERVICE_STATE_SHUTTING_DOWN, SERVICE_STATE_UNLOADING, SERVICE_STATE_UNLOADING},
    {SERVICE_STATE_UNLOADING, SERVICE_STATE_UNLOADING, SERVICE_STATE_UNKNOWN, SERVICE_STATE_UNLOADING, SERVICE_STATE_UNLOADING, SERVICE_STATE_UNLOADING},
    {SERVICE_STATE_UNLOADING, SERVICE_STATE_UNLOADING, SERVICE_STATE_UNLOADING, SERVICE_STATE_UNLOADING, SERVICE_STATE_UNLOADING, SERVICE_STATE_UNINITIALIZED}
};

STDAPI_(BOOL) WINAPI DllEntry (HINSTANCE hinstDLL, DWORD   Op, LPVOID  lpvReserved) {
    switch (Op) {
    case DLL_PROCESS_ATTACH :
        DisableThreadLibraryCalls((HMODULE)hinstDLL);
        if (ERROR_SUCCESS != InitializeSNTP (hinstDLL))
            return FALSE;

        if (ERROR_SUCCESS != InitializeDST (hinstDLL)) {
            DestroySNTP ();
            return FALSE;
        }

        break;

    case DLL_PROCESS_DETACH :
        DestroyDST ();
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

        if (StartDST () != ERROR_SUCCESS) {
            StopSNTP ();
            return FALSE;
        }
    }

    return TRUE;
}

//  @func PVOID | NTP_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from HLO_Init call
//  @rdesc	Returns TRUE for success, FALSE for failure.
//
//  The library WILL BE UNLOADED after this. Block here
//  until the state is completely clear and all the
//  threads are gone.
//
extern "C" BOOL NTP_Deinit(DWORD dwData) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] NTP_Deinit\r\n"));

    StopDST ();
    StopSNTP ();

    return TRUE;
}

//  @func PVOID | NTP_Open     | Device open routine
//  @parm DWORD | dwData       | value returned from HLO_Init call
//  @parm DWORD | dwAccess     | requested access (combination of GENERIC_READ
//                               and GENERIC_WRITE)
//  @parm DWORD | dwShareMode | requested share mode (combination of
//                               FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc	Returns a DWORD which will be passed to Read, Write, etc or NULL if
//                               unable to open device.
//
//  We don't do anything here, but in a real service this is a place to create
//  client process state. HCO_Close will be called with this handle when the process
//  exits or terminates, so the clean-up is easy.
//
extern "C" DWORD NTP_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode) {
    return TRUE;
}

//  @func BOOL  | NTP_Close | Device close routine
//  @parm DWORD | dwOpenData | value returned from HLO_Open call
//  @rdesc      Returns TRUE for success, FALSE for failure
//
//  Clean-up the client process state here.
//
extern "C" BOOL NTP_Close (DWORD dwData)  {
    return TRUE;
}

//  @func DWORD   | NTP_Write  | Device write routine
//  @parm DWORD   | dwOpenData | value returned from HLO_Open call
//  @parm LPCVOID | pBuf     | buffer containing data
//  @parm DWORD   | len        | maximum length to write [IN BYTES, NOT WORDS!!!]
//  @rdesc           Returns -1 for error, otherwise the number of bytes written.  The
//                   length returned is guaranteed to be the length requested unless an
//                   error condition occurs.
//
//  This is a vestige of streaming driver interface. We don't use it for services.
//
extern "C" DWORD NTP_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen) {
    return -1;
}

//  @func DWORD  | NTP_Read   | Device read routine
//  @parm DWORD  | dwOpenData | value returned from HLO_Open call
//  @parm LPVOID | pBuf       | buffer to receive data
//  @parm DWORD  | len        | maximum length to read [IN BYTES, not WORDS!!]
//  @rdesc      Returns 0 for end of file, -1 for error, otherwise the number of
//              bytes read.  The length returned is guaranteed to be the length
//              requested unless end of file or an error condition occurs.
//
//  This is a vestige of streaming driver interface. We don't use it for services.
//
extern "C" DWORD NTP_Read (DWORD dwData, LPVOID pBuf, DWORD dwLen) {
    return -1;
}

//  @func DWORD | NTP_Seek   | Device seek routine
//  @parm DWORD | dwOpenData | value returned from HLO_Open call
//  @parm long  | pos        | position to seek to (relative to type)
//  @parm DWORD | type       | FILE_BEGIN, FILE_CURRENT, or FILE_END
//  @rdesc      Returns current position relative to start of file, or -1 on error
//
//  This is a vestige of streaming driver interface. We don't use it for services.
//
extern "C" DWORD NTP_Seek (DWORD dwData, long pos, DWORD type) {
    return (DWORD)-1;
}

//  @func void | NTP_PowerUp | Device powerup routine
//  @comm       Called to restore device from suspend mode. 
//              You cannot call any routines aside from those in your dll in this call.
//
//  This is a vestige of streaming driver interface. We don't use it for services.
//
extern "C" void NTP_PowerUp(void) {
	return;
}

//  @func void | NTP_PowerDown | Device powerdown routine
//  @comm      Called to suspend device.  You cannot call any routines aside from
//             those in your dll in this call.
//
//  This is a vestige of streaming driver interface. We don't use it for services.
//
extern "C" void NTP_PowerDown(void) {
    return;
}

//  @func BOOL   | NTP_IOControl | Device IO control routine
//  @parm DWORD  | dwOpenData | value returned from HLO_Open call
//  @parm DWORD  | dwCode | io control code to be performed
//  @parm PBYTE  | pBufIn | input data to the device
//  @parm DWORD  | dwLenIn | number of bytes being passed in
//  @parm PBYTE  | pBufOut | output data from the device
//  @parm DWORD  | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device//  @rdesc         Returns TRUE for success, FALSE for failure
//  @remark	Routine exported by a device driver.  "PRF" is the string passed
//		in as lpszType in RegisterDevice
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
            if (iErr == ERROR_SUCCESS) {
                iErr = StartDST ();
                if (iErr != ERROR_SUCCESS)
                    StopSNTP ();
            }

        break;

        case IOCTL_SERVICE_STOP: // start a service that is currently in the stopped stage.
            iErr = StopDST ();
            if (iErr == ERROR_SUCCESS)
                iErr = StopSNTP ();
            break;

        case IOCTL_SERVICE_REFRESH:
            iErr = RefreshSNTP ();
            if (iErr == ERROR_SUCCESS)
                iErr = RefreshDST ();
            break;

        case IOCTL_SERVICE_STATUS:
            if (pBufOut && (dwLenOut == sizeof(DWORD)))  {
                DWORD dwState1 = GetStateSNTP ();
                DWORD dwState2 = GetStateDST ();

                DWORD dwState = SERVICE_STATE_UNKNOWN;

                if (dwState1 == SERVICE_STATE_UNKNOWN)
                    dwState = dwState2;
                else if (dwState2 == SERVICE_STATE_UNKNOWN)
                    dwState = dwState1;
                else if ((dwState1 <= 5) && (dwState2 <= 5))
                    dwState = gStateMap[dwState1][dwState2];

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

                iErr = ServiceControlDST (pBufIn, dwLenIn, pBufOut, dwLenOut, pdwActualOut, &fProcessed);
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
