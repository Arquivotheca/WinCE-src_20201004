//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*--
Module Name: svc.cxx
Abstract: service core
--*/
#include "dst.h"



#if defined (DEBUG) || defined (_DEBUG)
DBGPARAM dpCurSettings = 
{
    TEXT("DSTSVC"), 
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

CRITICAL_SECTION g_csDSTCriticalSection;

BOOL WINAPI DllMain (HANDLE hinstDLL, DWORD   Op, LPVOID  lpvReserved) 
{
    switch (Op) 
    {
    case DLL_PROCESS_ATTACH :
        DisableThreadLibraryCalls((HMODULE)hinstDLL);
        if (ERROR_SUCCESS != InitializeDST ((HINSTANCE)hinstDLL))
            return FALSE;

        break;

    case DLL_PROCESS_DETACH :
        DestroyDST ();
        break;
    }

    return TRUE;
}

//  @func PVOID | DST_Init | Service initialization routine
//  @parm DWORD | dwData   | Info passed to RegisterDevice
//  @rdesc      Returns a DWORD which will be passed to Open & Deinit or NULL if
//              unable to initialize the device.
//
//  This is called only once. Do the startup initialization in a thread
//  spawned by this function, but DO NOT BLOCK HERE!
//        
extern "C" DWORD DST_Init (DWORD dwData) 
{
    DEBUGMSG(ZONE_TRACE, (L"[DSTSVC] DST_Init\r\n"));

    if (! (dwData & SERVICE_INIT_STOPPED)) 
    {
        InitializeCriticalSection(&g_csDSTCriticalSection);
        if (StartDST () != ERROR_SUCCESS)
            return FALSE;
    }

    return TRUE;
}

//  @func PVOID | DST_Deinit | Device deinitialization routine
//  @parm DWORD | dwData     | value returned from DST_Init call
//  @rdesc  Returns TRUE for success, FALSE for failure.
//
//  The library WILL BE UNLOADED after this. Block here
//  until the state is completely clear and all the
//  threads are gone.
//
extern "C" BOOL DST_Deinit(DWORD dwData) 
{
    DEBUGMSG(ZONE_TRACE, (L"[DSTSVC] DST_Deinit\r\n"));

    StopDST ();
    DeleteCriticalSection(&g_csDSTCriticalSection);

    return TRUE;
}

//  @func PVOID | DST_Open     | Device open routine
//  @parm DWORD | dwData       | value returned from DST_Init call
//  @parm DWORD | dwAccess     | requested access (combination of GENERIC_READ
//                               and GENERIC_WRITE)
//  @parm DWORD | dwShareMode  | requested share mode (combination of
//                               FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc  Returns a DWORD which will be passed to Read, Write, etc or NULL if
//                               unable to open device.
//
//  We don't do anything here, but in a real service this is a place to create
//  client process state. DST_Close will be called with this handle when the process
//  exits or terminates, so the clean-up is easy.
//
extern "C" DWORD DST_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode) 
{
    DEBUGMSG(ZONE_TRACE, (L"[DSTSVC] DST_Open\r\n"));
	return TRUE;
}

//  @func BOOL  | DST_Close | Device close routine
//  @parm DWORD | dwData    | value returned from DST_Open call
//  @rdesc      Returns TRUE for success, FALSE for failure
//
//  Clean-up the client process state here.
//
extern "C" BOOL DST_Close (DWORD dwData)  
{
    DEBUGMSG(ZONE_TRACE, (L"[DSTSVC] DST_Close\r\n"));
    return TRUE;
}

//  @func DWORD   | DST_Write  | Device write routine
//  @parm DWORD   | dwData     | value returned from DST_Open call
//  @parm LPCVOID | pBuf       | buffer containing data
//  @parm DWORD   | len        | maximum length to write [IN BYTES, NOT WORDS!!!]
//  @rdesc           Returns -1 for error, otherwise the number of bytes written.  The
//                   length returned is guaranteed to be the length requested unless an
//                   error condition occurs.
//
//  This is a vestige of streaming driver interface. We don't use it for services.
//
extern "C" DWORD DST_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen) 
{
    return -1;
}

//  @func DWORD  | DST_Read   | Device read routine
//  @parm DWORD  | dwData     | value returned from DST_Open call
//  @parm LPVOID | pBuf       | buffer to receive data
//  @parm DWORD  | len        | maximum length to read [IN BYTES, not WORDS!!]
//  @rdesc      Returns 0 for end of file, -1 for error, otherwise the number of
//              bytes read.  The length returned is guaranteed to be the length
//              requested unless end of file or an error condition occurs.
//
//  This is a vestige of streaming driver interface. We don't use it for services.
//
extern "C" DWORD DST_Read (DWORD dwData, LPVOID pBuf, DWORD dwLen) 
{
    return -1;
}

//  @func DWORD | DST_Seek   | Device seek routine
//  @parm DWORD | dwData     | value returned from DST_Open call
//  @parm long  | pos        | position to seek to (relative to type)
//  @parm DWORD | type       | FILE_BEGIN, FILE_CURRENT, or FILE_END
//  @rdesc      Returns current position relative to start of file, or -1 on error
//
//  This is a vestige of streaming driver interface. We don't use it for services.
//
extern "C" DWORD DST_Seek (DWORD dwData, long pos, DWORD type) 
{
    return (DWORD)-1;
}

//  @func void | DST_PowerUp | Device powerup routine
//  @comm       Called to restore device from suspend mode. 
//              You cannot call any routines aside from those in your dll in this call.
//
//  This is a vestige of streaming driver interface. We don't use it for services.
//
extern "C" void DST_PowerUp(void) 
{
    return;
}

//  @func void | DST_PowerDown | Device powerdown routine
//  @comm      Called to suspend device.  You cannot call any routines aside from
//             those in your dll in this call.
//
//  This is a vestige of streaming driver interface. We don't use it for services.
//
extern "C" void DST_PowerDown(void) 
{
    return;
}

//  @func BOOL   | DST_IOControl | Device IO control routine
//  @parm DWORD  | dwData        | value returned from DST_Open call
//  @parm DWORD  | dwCode        | io control code to be performed
//  @parm PBYTE  | pBufIn        | input data to the device
//  @parm DWORD  | dwLenIn       | number of bytes being passed in
//  @parm PBYTE  | pBufOut       | output data from the device
//  @parm DWORD  | dwLenOut      | maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut  | actual number of bytes received from device
//  @rdesc         Returns TRUE for success, FALSE for failure
//
//  This is THE way to expose both manageability of the service and the feature API set.
//  Consumer of the API set will marshal input arguments into input buffer (pBufIn).
//  This function unmarshals it and executes the API, and then marshals output parameters
//  into output buffer (pBufOut).
//
extern "C" BOOL DST_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn,
              DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) 
{
    DEBUGMSG(ZONE_TRACE, (L"[DSTSVC] DST_IOControl 0x%08x\r\n", dwCode));

    int iErr  = ERROR_INVALID_PARAMETER;
    switch (dwCode) 
    {
    case IOCTL_SERVICE_START: // start a service that is currently in the stopped stage.
        iErr = StartDST ();
        break;

    case IOCTL_SERVICE_STOP: // start a service that is currently in the stopped stage.
        iErr = StopDST ();
        break;

    case IOCTL_SERVICE_STATUS:
        if (pBufOut && (dwLenOut == sizeof(DWORD)))  
        {
            DWORD dwState = GetStateDST ();
            __try 
            {
                *(DWORD *)pBufOut = dwState;

                if (pdwActualOut)
                    *pdwActualOut = sizeof(DWORD);

                    iErr = ERROR_SUCCESS;
            } __except (1) 
            {
                iErr = ERROR_INVALID_PARAMETER;
            }
        } 
        else 
        {
            iErr = ERROR_INVALID_PARAMETER;
        }
        break;
    case IOCTL_SERVICE_DEBUG:
#if defined (DEBUG) || defined (_DEBUG)
        if ((dwLenIn == sizeof(DWORD)) && pBufIn) 
        {
            dpCurSettings.ulZoneMask = (*(DWORD *)pBufIn);
            iErr = ERROR_SUCCESS;
        } 
        else
        {
            iErr = ERROR_INVALID_PARAMETER;
        }
#endif
        break;
    case IOCTL_DSTSVC_NOTIFY_TIME_CHANGE:
        ASSERT(GetCurrentThreadId() != g_dwDSTThreadId); // Should not be called by DSTThread   
        if ((dwLenIn == sizeof(DSTSVC_TIME_CHANGE_INFORMATION)) && pBufIn)
        {
            EnterCriticalSection(&g_csDSTCriticalSection);
            g_lpTimeChangeInformation = (LPDSTSVC_TIME_CHANGE_INFORMATION)pBufIn;
            DEBUGMSG(TRUE, (L"[DSTSVC] IOCTL_DSTSVC_NOTIFY_TIME_CHANGE Set%s [%02d/%02d/%04d %02d:%02d:%02d]\r\n",
                g_lpTimeChangeInformation->dwType == TM_SYSTEMTIME ? L"SystemTime" : L"LocalTime", 
                g_lpTimeChangeInformation->stNew.wMonth,
                g_lpTimeChangeInformation->stNew.wDay,
                g_lpTimeChangeInformation->stNew.wYear,
                g_lpTimeChangeInformation->stNew.wHour,
                g_lpTimeChangeInformation->stNew.wMinute,
                g_lpTimeChangeInformation->stNew.wSecond
                ));
            iErr = ERROR_GEN_FAILURE;
            if (SetEvent(g_rghEvents[EI_PRE_TIME_CHANGE]))
            {
                if(WaitForSingleObject(g_hDSTHandled,INFINITE) == WAIT_OBJECT_0)
                {
                    iErr = ERROR_SUCCESS;
                }
            }
            LeaveCriticalSection(&g_csDSTCriticalSection);
        }
        else
        {
            iErr = ERROR_INVALID_PARAMETER;
        }
        break;
    case IOCTL_DSTSVC_NOTIFY_TZ_CHANGE:
        if (GetCurrentThreadId() == g_dwDSTThreadId)
        {
            DEBUGMSG(TRUE, (L"[DSTSVC] Do not handle IOCTL_DSTSVC_NOTIFY_TZ_CHANGE when called from DSTThread\r\n"));
            iErr = ERROR_SUCCESS;
        }
        else
        {
            DEBUGMSG(TRUE, (L"[DSTSVC] IOCTL_DSTSVC_NOTIFY_TZ_CHANGE\r\n"));
            iErr = ERROR_GEN_FAILURE;
            EnterCriticalSection(&g_csDSTCriticalSection);
            if (SetEvent(g_rghEvents[EI_TZ_CHANGE]))
            {
                if(WaitForSingleObject(g_hDSTHandled,INFINITE) == WAIT_OBJECT_0)
                {
                    iErr = ERROR_SUCCESS;
                }
            }
            LeaveCriticalSection(&g_csDSTCriticalSection);
        }
        break;
    }

    SetLastError(iErr);
    return iErr == ERROR_SUCCESS;
}
