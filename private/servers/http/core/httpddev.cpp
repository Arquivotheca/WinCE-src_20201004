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
Module Name: httpddev.cpp
Abstract: HTTP device driver exports, and routines that are
                  exposed to facilitate their manipulation.
--*/


#include "httpd.h"


BOOL HttpStopAndRestart(void);

//
//   Functions exported from HTTPD.
//
extern "C" void HttpdStart() 
{
    HANDLE hFile;
    DWORD dwBytesReturned;

    hFile = CreateFile(HTTPD_DEV_NAME,GENERIC_READ|GENERIC_WRITE,0,
        NULL,OPEN_EXISTING,0,NULL);

    if (INVALID_HANDLE_VALUE == hFile)
    {
        return;
    }

    DeviceIoControl(hFile,IOCTL_SERVICE_START,0,0,0,0,&dwBytesReturned,0);
    CloseHandle(hFile);
}

extern "C" void HttpdStop() 
{
    HANDLE hFile;
    DWORD dwBytesReturned;

    hFile = CreateFile(HTTPD_DEV_NAME,GENERIC_READ|GENERIC_WRITE,0,
        NULL,OPEN_EXISTING,0,NULL);

    if (INVALID_HANDLE_VALUE == hFile)
    {
        return;
    }

    DeviceIoControl(hFile,IOCTL_SERVICE_STOP,0,0,0,0,&dwBytesReturned,0);
    CloseHandle(hFile);
}

//  This call must NOT block!  It's possible for an ISAPI extension to
//  call this function, in which case we want the ISAPI extension to continue
//  executing so that request (along with any others) can terminate.
//  Only after the request has terminated can we start up again.

extern "C" void HttpdStopAndRestart() 
{
    HANDLE hFile;
    DWORD dwBytesReturned;

    hFile = CreateFile(HTTPD_DEV_NAME,GENERIC_READ|GENERIC_WRITE,0,
        NULL,OPEN_EXISTING,0,NULL);

    if (INVALID_HANDLE_VALUE == hFile)
    {
        return;
    }

    DeviceIoControl(hFile,IOCTL_SERVICE_REFRESH,0,0,0,0,&dwBytesReturned,0);
    CloseHandle(hFile);
}

//
// Internal helper functions to handle state changes.
//
void WaitForHttpToShutDown(void) 
{
    CloseAllConnections();
    g_pVars->m_fAcceptConnections = FALSE;
    WaitForConnectionThreadsToEnd();
    CleanupGlobalMemory(g_cWebsites);
}

// lpv = NULL when we don't want to start up again, lpv=1 when we'll restart immediatly.
DWORD WINAPI HttpdStopThread(LPVOID lpv) 
{
    MyWaitForAdminThreadReadyState();

    DEBUGCHK(g_fState == SERVICE_STATE_SHUTTING_DOWN && g_hAdminThread);

    WaitForHttpToShutDown();

    DEBUGCHK(g_fState == SERVICE_STATE_SHUTTING_DOWN);
    DEBUGCHK(!g_pVars && !g_pWebsites && (g_cWebsites==0));

    if (!lpv) 
    {
        SetWebServerState(SERVICE_STATE_OFF);
        CloseHandle(g_hAdminThread);
        g_hAdminThread = 0;
    }
    return 0;
}

//  This thread stops the server and restarts it.
DWORD WINAPI HttpdStopAndRestartThread(LPVOID lpv)  
{
    MyWaitForAdminThreadReadyState();
    HttpdStopThread((void*)1);

    SetWebServerState(SERVICE_STATE_STARTING_UP);
    InitializeGlobals();

    CloseHandle(g_hAdminThread);
    g_hAdminThread = 0;
    return 0;
}

BOOL HttpStopAndRestart(void)  
{
    if (! SetWebServerState(SERVICE_STATE_SHUTTING_DOWN,SERVICE_STATE_ON)) 
    {
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }

    DEBUGCHK(!g_hAdminThread);

    g_hAdminThread = MyCreateThread(HttpdStopAndRestartThread,0);
    return g_hAdminThread ? TRUE : FALSE;
}

// Called to shutdown all HTTPD threads.
BOOL HttpShutDown(void) 
{
    // persist our shutdown state to registry.
    CReg reg(HKEY_LOCAL_MACHINE, RK_HTTPD);
    reg.SetDW(RV_ISENABLED,0);

    g_pSecurePortList->ResetSecureList();

    if (! SetWebServerState(SERVICE_STATE_SHUTTING_DOWN,SERVICE_STATE_ON)) 
    {
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }

    DEBUGCHK(!g_hAdminThread);
    g_hAdminThread = MyCreateThread(HttpdStopThread,0);
    return g_hAdminThread ? TRUE : FALSE;
}

BOOL HttpUnload(void)  
{
    LONG fWaitForHttpShutdown = FALSE;

    DEBUGMSG(ZONE_INIT,(L"HTTPD: Starting to unload service\r\n"));

    // If a stop/start/restart thread is running already, then wait for it before continuing.
    if (g_hAdminThread)
    {
        WaitForSingleObject(g_hAdminThread,INFINITE);
    }

    Sleep(100);

    // If the service is either shutting down or starting up then we can't unload.
    // Succeeding on unload but not really unloading is NOT an option, lib will get freed by services.exe or device.exe
    // when this function returns and running threads in httpd's context will crash, so keep polling as long as it takes.
    while (1) 
    {
        if (SetWebServerState(SERVICE_STATE_UNLOADING,SERVICE_STATE_ON)) 
        {
            fWaitForHttpShutdown = TRUE;
            break;
        }
        if (SetWebServerState(SERVICE_STATE_UNLOADING,SERVICE_STATE_OFF))
        {
            break;
        }
        DEBUGMSG(ZONE_INIT,(L"HTTPD: HttpUnload can't unload service, HTTPD state must be on or off, current state = %d, sleep for 2 seconds\r\n",g_fState));
        Sleep(2000);
    }
    DEBUGCHK(!g_hAdminThread);

    if (fWaitForHttpShutdown) 
    {
        WaitForHttpToShutDown();
    }

    if (g_pSecurePortList) 
    {
        delete g_pSecurePortList;
        g_pSecurePortList = NULL;
    }

    if (g_pTimer) 
    {
        g_pTimer->Shutdown();
        delete g_pTimer;
        g_pTimer = NULL;
    }

    svsutil_DeInitializeInterfaceMapperOnce(); // not DLLMain safe.

    DEBUGCHK(g_fState == SERVICE_STATE_UNLOADING);
    DEBUGCHK(!g_pVars && !g_hAdminThread && !g_pWebsites && (g_cWebsites==0));
    DEBUGMSG(ZONE_INIT,(L"HTTPD: Done unloading service\r\n"));
    return TRUE;
}


// Certain operations may only be performed by trusted caller (mostly services.exe itself)
BOOL CheckCallerTrusted(void) 
{
    // Currently not tied into ACLs.
    return TRUE;
}


//      @func BOOL | HTP_IOControl | Device IO control routine
//  @parm DWORD | dwOpenData | value returned from HTP_Open call
//  @parm DWORD | dwCode | io control code to be performed
//  @parm PBYTE | pBufIn | input data to the device
//  @parm DWORD | dwLenIn | number of bytes being passed in
//  @parm PBYTE | pBufOut | output data from the device
//  @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device
//  @rdesc      Returns TRUE for success, FALSE for failure
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//          in as lpszType in RegisterDevice

extern "C" BOOL HTP_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn,
              DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
              PDWORD pdwActualOut)  
{
    DEBUGMSG (ZONE_DEVICE, (TEXT("HTP_IOControl(dwCode = 0x%X)\r\n"), dwCode));

    DWORD dwOut;

    switch (dwCode)
    {
        case IOCTL_SERVICE_START:
        {
            CReg reg;
            reg.Open(HKEY_LOCAL_MACHINE,RK_HTTPD);
            reg.SetDW(RV_ISENABLED,1);

            if (SetWebServerState(SERVICE_STATE_STARTING_UP,SERVICE_STATE_OFF)) 
            {
                DEBUGCHK(!g_hAdminThread);
                g_hAdminThread = MyCreateThread(InitializeGlobalsThread,0);
                if (g_hAdminThread) 
                {
                    return TRUE;
                }
                SetWebServerState(SERVICE_STATE_OFF);
                return FALSE;
            }
            SetLastError(ERROR_SERVICE_ALREADY_RUNNING);
            DEBUGMSG(ZONE_INIT,(L"HTTPD: HTP_IOControl cannot process, IOCTL_SERVICE_START, state != starting up, serviceState=%d\r\n",g_fState));
            return FALSE;
        }
        break;

        case IOCTL_SERVICE_STOP:
            return HttpShutDown();
        break;

        case IOCTL_SERVICE_REFRESH:
            return HttpStopAndRestart();
        break;

        case IOCTL_SERVICE_STATUS:
            __try 
            {
                if (!pBufOut || dwLenOut < sizeof(DWORD)) 
                {
                    SetLastError(ERROR_INVALID_PARAMETER);
                    return FALSE;
                }
                *(DWORD *)pBufOut = g_fState;
                if (pdwActualOut)
                    *pdwActualOut = sizeof(DWORD);
            }
            __except (ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) 
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
            return TRUE;
        break;

        case IOCTL_SERVICE_REGISTER_SOCKADDR:
            if (pBufIn == NULL) 
            {
                // pBufIn = NULL on first call by services.exe to query whether this is supported or not.
                // if we return FALSE there will be no more calls.
                // pBuf is SOCKADDR being registered on subsequent calls.  We don't care what
                // we're bound to, so don't bother checking at this stage.
                return IsHttpdEnabled();
            }
            return TRUE;
        break;

        case IOCTL_SERVICE_DEREGISTER_SOCKADDR:
            SOCKADDR sockAddr;

            // Received when from a super service.
            if ((pBufIn==NULL) || (dwLenIn < sizeof(sockAddr)) ||
                (0 == CeSafeCopyMemory(&sockAddr,pBufIn,sizeof(sockAddr))))
            
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            if (sockAddr.sa_family == AF_INET || sockAddr.sa_family == AF_INET6)
            {
                g_pSecurePortList->RemovePortFromSecureList(GetSocketPort(&sockAddr));
            }

            return TRUE;

        break;

        case IOCTL_SERVICE_STARTED:
            // This should only be generated by services.exe itself, so we're fine checking trust.
            if (! CheckCallerTrusted())
            {
                return FALSE;
            }

            if (SetWebServerState(SERVICE_STATE_STARTING_UP,SERVICE_STATE_OFF)) 
            {
                DEBUGMSG(ZONE_INIT,(L"HTTPD:IOControl(IOCTL_SERVICE_STARTED) creating global variales\r\n"));
                DEBUGCHK(!g_hAdminThread);
                g_hAdminThread = MyCreateThread(InitializeGlobalsThread,0);
                if (g_hAdminThread) 
                {
                    return TRUE;
                }
                return FALSE;
            }
            SetLastError(ERROR_SERVICE_ALREADY_RUNNING);
            return FALSE;
        break;

        case IOCTL_SERVICE_CONNECTION:
        {
            // This should only be generated by services.exe itself, so we're fine checking trust.
            if (! CheckCallerTrusted())
            {
                return FALSE;
            }

            SOCKET sock;

            // Received when from a super service.
            if ((pBufIn==NULL) || (dwLenIn!=sizeof(SOCKET)) ||
                (0 == CeSafeCopyMemory(&sock,pBufIn,sizeof(sock))))
            
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            if (g_fState != SERVICE_STATE_ON || !g_pVars->m_fAcceptConnections) 
            {
                SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
                DEBUGMSG(ZONE_ERROR,(L"HTTP: HTP_IOCTL(IOCTL_SERVICE_CONNECTION) fails, HTTPD not running\r\n"));
                closesocket(sock); // we still have to do this much.
                return FALSE;
            }

            SOCKADDR_STORAGE sockAddr;
            int          cbSockAddr = sizeof(sockAddr);

            if ((0 != getsockname(sock,(PSOCKADDR) &sockAddr,&cbSockAddr))) 
            {
                DEBUGMSG(ZONE_ERROR,(L"HTTPD: getsockname fails, GLE=0x%08x\r\n",GetLastError()));
                closesocket(sock);
                return FALSE;
            }
            return CreateHTTPConnection(sock,(PSOCKADDR)&sockAddr,cbSockAddr);
        }
        break;

        // network addresses are changing.
        case IOCTL_SERVICE_NOTIFY_ADDR_CHANGE:
            // This should only be generated by services.exe itself, so we're fine checking trust.
            if (! CheckCallerTrusted())
            {
                return FALSE;
            }

            if ((g_fState == SERVICE_STATE_ON) || (g_fState == SERVICE_STATE_STARTING_UP))
            {
                svsutil_HandleNotifyAddrChange();
            }
        break;

        case IOCTL_SERVICE_SUPPORTED_OPTIONS:
            if (!pBufOut || dwLenOut < sizeof(DWORD)) 
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            dwOut = 0;

            if (g_fIsapiExtensionModule)
            {
                dwOut |= HTTPD_OPTION_ISAPI_EXTENSIONS;
            }

            if (g_fIsapiFilterModule)
            {
                dwOut |= HTTPD_OPTION_ISAPI_FILTERS;
            }

            if (g_fAuthModule)
            {
                dwOut |= (HTTPD_OPTION_AUTHENTICATION | HTTPD_OPTION_SSL);
            }

            if (g_fWebDavModule)
            {
                dwOut |= HTTPD_OPTION_WEBDAV;
            }

            __try 
            {
                *(DWORD *)pBufOut = dwOut;
                if (pdwActualOut)
                {
                    *pdwActualOut = sizeof(DWORD);
                }
            }
            __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) 
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            return TRUE;

        break;

        case IOCTL_HTTPD_REFRESH_VROOTS:
            return RefreshVRootTable();
            break;

        case IOCTL_HTTPD_SET_PORT_SECURE:
        case IOCTL_HTTPD_SET_PORT_NON_SECURE:
        {
            DWORD dwPort;
            if ((pBufIn==NULL) || (dwLenIn!=sizeof(DWORD)) ||
                (0 == CeSafeCopyMemory(&dwPort,pBufIn,sizeof(DWORD))))
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            if (dwCode == IOCTL_HTTPD_SET_PORT_SECURE)
            {
                return g_pSecurePortList->AddPortToSecureList(dwPort);
            }
            else
            {
                return g_pSecurePortList->RemovePortFromSecureList(dwPort);
            }
        }
        break;

        default:
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: HTP_IOControl unhandled dwCode 0x%08x\r\n",dwCode));
        break;
    }
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}


//      @func PVOID | HTP_Init | Device initialization routine
//  @parm DWORD | dwData | Info passed to RegisterDevice
//  @rdesc      Returns a DWORD which will be passed to Open & Deinit or NULL if
//              unable to initialize the device.
//      @remark dwData is a CTelnetContext passed in by our server thread

extern "C" DWORD HTP_Init (DWORD dwData) 
{
    DEBUGMSG(ZONE_INIT,(L"HTTPD:HTP_Init(0x%08x)\r\n",dwData));

    if (! (dwData & SERVICE_INIT_STOPPED)) 
    {
        // If services.exe was not configured to startup super-service
        // ports for HTTPD, then there's nothing we can do
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: Error, must run in super-services mode.  Cannot start\r\n"));
        SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
        DEBUGCHK(0); // Indicates a serious system misconfiguration which needs fixing!
        return 0;
    }

    if (SetWebServerState(SERVICE_STATE_OFF,SERVICE_STATE_UNINITIALIZED))  
    {
        DEBUGCHK(g_pTimer == NULL);

        DEBUGMSG(ZONE_INIT,(L"HTTPD: Initializing web server: creating thread pool, initing interface mapping...\r\n"));
        DEBUGMSG(ZONE_INIT,(L"HTTPD: Web Server supports the following components: %s %s %s %s\r\n",
            g_fIsapiExtensionModule ? L"ISAPI Extensions," : L"",
            g_fIsapiFilterModule    ? L"ISAPI Filters," : L"",
            g_fAuthModule       ? L"Authentication, SSL," : L"",
            g_fWebDavModule     ? L"Web DAV": L""
        ));

        if (! HttpInitializeOnce())
        {
            return FALSE;
        }

        return (DWORD) &g_fState;
    }

    // HTTPD cannot be initialized more than once.
    SetLastError(ERROR_SERVICE_ALREADY_RUNNING);
    return 0;
}

//      @func PVOID | HTP_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from HTP_Init call
//  @rdesc      Returns TRUE for success, FALSE for failure.

extern "C" BOOL HTP_Deinit(DWORD dwData) 
{
    DEBUGMSG(ZONE_DEVICE, (TEXT("HTP_Deinit(0x%08x)\r\n"),dwData));
    return HttpUnload();
}

//      @func PVOID | HTP_Open      | Device open routine
//  @parm DWORD | dwData        | value returned from HTP_Init call
//  @parm DWORD | dwAccess          | requested access (combination of GENERIC_READ
//                                and GENERIC_WRITE)
//  @parm DWORD | dwShareMode   | requested share mode (combination of
//                                FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc      Returns a DWORD which will be passed to Read, Write, etc or NULL if
//              unable to open device.
extern "C" DWORD HTP_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode) 
{
    DEBUGMSG (ZONE_DEVICE, (TEXT("HTP_Open(0x%X)\r\n"), dwData));
    return (DWORD)dwData;
}

//      @func BOOL | HTP_Close | Device close routine
//  @parm DWORD | dwOpenData | value returned from HTP_Open call
//  @rdesc      Returns TRUE for success, FALSE for failure
extern "C" BOOL HTP_Close (DWORD dwData)  
{
    DEBUGMSG (ZONE_DEVICE, (TEXT("HTP_Close(0x%X)\r\n"), dwData));

    return TRUE;
}

//      @func DWORD | HTP_Write | Device write routine
//  @parm DWORD | dwOpenData | value returned from HTP_Open call
//  @parm LPCVOID | pBuf | buffer containing data
//  @parm DWORD | len | maximum length to write [IN BYTES, NOT WORDS!!!]
//  @rdesc      Returns -1 for error, otherwise the number of bytes written.  The
//              length returned is guaranteed to be the length requested unless an
//              error condition occurs.
extern "C" DWORD HTP_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen) 
{
    DEBUGMSG (ZONE_DEVICE, (TEXT("HTP_Write(0x%X, 0x%X, %d)\r\n"),
                  dwData, pInBuf, dwInLen));
    return (DWORD)-1;
}

//      @func DWORD | HTP_Read | Device read routine
//  @parm DWORD | dwOpenData | value returned from HTP_Open call
//  @parm LPVOID | pBuf | buffer to receive data
//  @parm DWORD | len | maximum length to read [IN BYTES, not WORDS!!]
//  @rdesc      Returns 0 for end of file, -1 for error, otherwise the number of
//              bytes read.  The length returned is guaranteed to be the length
//              requested unless end of file or an error condition occurs.
//
// !!!!NOTE that this function ALWAYS returns ANSI text, NOT UNICODE!!!!
//
extern "C" DWORD HTP_Read (DWORD dwData, LPVOID pBuf, DWORD dwLen) 
{
    DEBUGMSG (ZONE_DEVICE, (TEXT("HTP_Read Enter(0x%X, 0x%X, %d)\r\n"), dwData, pBuf, dwLen));
    return (DWORD)-1;
}


//      @func DWORD | HTP_Seek | Device seek routine
//  @parm DWORD | dwOpenData | value returned from HTP_Open call
//  @parm long | pos | position to seek to (relative to type)
//  @parm DWORD | type | FILE_BEGIN, FILE_CURRENT, or FILE_END
//  @rdesc      Returns current position relative to start of file, or -1 on error

extern "C" DWORD HTP_Seek (DWORD dwData, long pos, DWORD type) 
{
    DEBUGMSG (ZONE_DEVICE, (TEXT("HTP_Seek(0x%X, %d, %d) NOT SUPPORTED\r\n"), dwData, pos, type));
    return (DWORD)-1;
}

//      @func void | HTP_PowerUp | Device powerup routine
//      @comm   Called to restore device from suspend mode.  You cannot call any
//              routines aside from those in your dll in this call.
extern "C" void HTP_PowerUp(void) 
{
    return;
}
//      @func void | HTP_PowerDown | Device powerdown routine
//      @comm   Called to suspend device.  You cannot call any routines aside from
//              those in your dll in this call.
extern "C" void HTP_PowerDown(void) 
{
    return;
}


