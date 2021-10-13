//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
extern "C" void HttpdStart() {
	HANDLE hFile;
	DWORD dwBytesReturned;
	
	hFile = CreateFile(HTTPD_DEV_NAME,GENERIC_READ|GENERIC_WRITE,0,
	        NULL,OPEN_EXISTING,0,NULL);

	if (INVALID_HANDLE_VALUE == hFile)
		return;

	DeviceIoControl(hFile,IOCTL_SERVICE_START,0,0,0,0,&dwBytesReturned,0);
	CloseHandle(hFile);
}

extern "C" void HttpdStop() {
	HANDLE hFile;
	DWORD dwBytesReturned;

	hFile = CreateFile(HTTPD_DEV_NAME,GENERIC_READ|GENERIC_WRITE,0,
	        NULL,OPEN_EXISTING,0,NULL);

	if (INVALID_HANDLE_VALUE == hFile)
		return;

	DeviceIoControl(hFile,IOCTL_SERVICE_STOP,0,0,0,0,&dwBytesReturned,0);
	CloseHandle(hFile);
}

//  This call must NOT block!  It's possible for an ISAPI extension to
//  call this function, in which case we want the ISAPI extension to continue
//  executing so that request (along with any others) can terminate.
//  Only after the request has terminated can we start up again.

extern "C" void HttpdStopAndRestart() {
	HANDLE hFile;
	DWORD dwBytesReturned;
 
	// It is possible to refresh web server when it's running as an executable;
	// however the call to HttpdStopAndRestart MUST be made from within an
	// ISAPI running from the context of httpdexe.exe
	if (SERVICE_CALLER_PROCESS_HTTPDEXE_EXE == g_CallerExe) {
		HttpStopAndRestart();
		return;
	}

	hFile = CreateFile(HTTPD_DEV_NAME,GENERIC_READ|GENERIC_WRITE,0,
	        NULL,OPEN_EXISTING,0,NULL);

	if (INVALID_HANDLE_VALUE == hFile)
		return;

	DeviceIoControl(hFile,IOCTL_SERVICE_REFRESH,0,0,0,0,&dwBytesReturned,0);
	CloseHandle(hFile);
}

// 
// Internal helper functions to handle state changes.
//
void WaitForHttpToShutDown(void) {
	CloseAllConnections();

	if (g_hListenThread) {
		// Accept model (server spun its own accept thread)
		g_pVars->CloseHTTPSockets();
		WaitForSingleObject(g_hListenThread,INFINITE);
	}
	else { // running as a super-server, services.exe owns our sockets.
		g_pVars->m_fAcceptConnections = FALSE;
		WaitForConnectionThreadsToEnd();
		CleanupGlobalMemory(g_cWebsites);
	}
}

// lpv = NULL when we don't want to start up again, lpv=1 when we'll restart immediatly.
DWORD WINAPI HttpdStopThread(LPVOID lpv) {
	DEBUGCHK(g_fState == SERVICE_STATE_SHUTTING_DOWN && g_hAdminThread);

	WaitForHttpToShutDown();

	DEBUGCHK(g_fState == SERVICE_STATE_SHUTTING_DOWN);
	DEBUGCHK(!g_pVars && !g_hListenThread && !g_pWebsites && (g_cWebsites==0));

	if (!lpv) {
		g_fState = SERVICE_STATE_OFF;
		g_hAdminThread = 0;
	}
	return 0;
}

//  This thread stops the server and restarts it.
DWORD WINAPI HttpdStopAndRestartThread(LPVOID lpv)  {
	BOOL fReInit = g_hListenThread ? TRUE : FALSE;
	HttpdStopThread((void*)1);

	if (fReInit) {
		g_fState = SERVICE_STATE_OFF;
		HttpInitialize(NULL);
	}
	else {
		g_fState = SERVICE_STATE_STARTING_UP;
		InitializeGlobals(TRUE);
	}
	g_hAdminThread = 0;
	return 0;
}

BOOL HttpStopAndRestart(void)  {
	if (InterlockedCompareExchange(&g_fState,(INTERLOCKED_COMP) SERVICE_STATE_SHUTTING_DOWN, (INTERLOCKED_COMP) SERVICE_STATE_ON) != (INTERLOCKED_RESULT)SERVICE_STATE_ON) {
		SetLastError(ERROR_SERVICE_NOT_ACTIVE);
		return FALSE;
	}
	
	DEBUGCHK(!g_hAdminThread);
	
	g_hAdminThread = MyCreateThread(HttpdStopAndRestartThread,0);
	if (g_hAdminThread)
		CloseHandle(g_hAdminThread);

	return TRUE;
}

// Called to shutdown all HTTPD threads.
BOOL HttpShutDown(void) {
	// persist our shutdown state to registry.
	CReg reg(HKEY_LOCAL_MACHINE, RK_HTTPD);
	reg.SetDW(RV_ISENABLED,0);

	if (InterlockedCompareExchange(&g_fState,SERVICE_STATE_SHUTTING_DOWN, 
									      SERVICE_STATE_ON) != (INTERLOCKED_RESULT)SERVICE_STATE_ON) {
		SetLastError(ERROR_SERVICE_NOT_ACTIVE);
		return FALSE;
	}

	DEBUGCHK(!g_hAdminThread);
	g_hAdminThread = MyCreateThread(HttpdStopThread,0);
	if (g_hAdminThread)
		CloseHandle(g_hAdminThread);

	return TRUE;
}

BOOL HttpUnload(void)  {
	LONG fOldState;

	DEBUGMSG(ZONE_INIT,(L"HTTPD: Starting to unload service\r\n"));

	// If a stop/start/restart thread is running already, then wait for it before continuing.
	if (g_hAdminThread)
		WaitForSingleObject(g_hAdminThread,INFINITE);

	Sleep(100);

	// If the service is either shutting down or starting up then we can't unload.
	// Succeeding on unload but not really unloading is NOT an option, lib will get freed by services.exe or device.exe 
	// when this function returns and running threads in httpd's context will crash, so keep polling as long as it takes.
	while (1) {
		if ((fOldState = (long)InterlockedCompareExchange(&g_fState,SERVICE_STATE_UNLOADING,SERVICE_STATE_ON)) == SERVICE_STATE_ON ||
		    (fOldState = (long)InterlockedCompareExchange(&g_fState,SERVICE_STATE_UNLOADING,SERVICE_STATE_OFF)) == SERVICE_STATE_OFF)
		{
			break;
		}
		DEBUGMSG(ZONE_INIT,(L"HTTPD: HttpUnload can't unload service, HTTPD state must be on or off, current state = %d, sleep for 2 seconds\r\n",g_fState));
		Sleep(2000);
	}
	DEBUGCHK(!g_hAdminThread);

	if (fOldState == SERVICE_STATE_ON) {
		WaitForHttpToShutDown();
	}

	if (g_pTimer) {
		g_pTimer->Shutdown();
		delete g_pTimer;
		g_pTimer = NULL;
	}

	svsutil_DeInitializeInterfaceMapperOnce(); // not DLLMain safe.

	DEBUGCHK(g_fState == SERVICE_STATE_UNLOADING);
	DEBUGCHK(!g_pVars && !g_hListenThread && !g_hAdminThread && !g_pWebsites && (g_cWebsites==0));
	DEBUGMSG(ZONE_INIT,(L"HTTPD: Done unloading service\r\n"));
	return TRUE;
}


//	@func BOOL | HTP_IOControl | Device IO control routine
//  @parm DWORD | dwOpenData | value returned from HTP_Open call
//  @parm DWORD | dwCode | io control code to be performed
//  @parm PBYTE | pBufIn | input data to the device
//  @parm DWORD | dwLenIn | number of bytes being passed in
//  @parm PBYTE | pBufOut | output data from the device
//  @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device
//  @rdesc	Returns TRUE for success, FALSE for failure
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//		in as lpszType in RegisterDevice

extern "C" BOOL HTP_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn,
			  DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
			  PDWORD pdwActualOut)  {
	DEBUGMSG (ZONE_DEVICE, (TEXT("HTP_IOControl(dwCode = 0x%X)\r\n"), dwCode));
	CReg reg;

	DWORD dwOut;

	switch (dwCode)
	{
		case IOCTL_SERVICE_START:
			reg.Open(HKEY_LOCAL_MACHINE,RK_HTTPD);
			reg.SetDW(RV_ISENABLED,1);

			if (g_fSuperServices) {
				if (InterlockedCompareExchange(&g_fState,  SERVICE_STATE_STARTING_UP,
				                   SERVICE_STATE_OFF) == SERVICE_STATE_OFF) {

					DEBUGCHK(!g_hAdminThread);
					g_hAdminThread = MyCreateThread(InitializeGlobalsThread,0);
					if (g_hAdminThread) {
						CloseHandle(g_hAdminThread);
						return TRUE;
					}
					g_fState = SERVICE_STATE_OFF;
					return FALSE;
				}
				SetLastError(ERROR_SERVICE_ALREADY_RUNNING);
				DEBUGMSG(ZONE_INIT,(L"HTTPD: HTP_IOControl cannot process, IOCTL_SERVICE_START, state != starting up, serviceState=%d\r\n",g_fState));
				return FALSE;
			}
			else
				return HttpInitialize(NULL);
		break;

		case IOCTL_SERVICE_STOP:
			return HttpShutDown();
		break;

		case IOCTL_SERVICE_REFRESH:
			return HttpStopAndRestart();
		break;

#if ! defined (OLD_CE_BUILD)
		case IOCTL_SERVICE_STATUS:
			if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
				pBufOut = (PBYTE) MapCallerPtr(pBufOut,sizeof(DWORD));
				pdwActualOut = (PDWORD) MapCallerPtr(pdwActualOut,sizeof(DWORD));
			}

			if (!pBufOut || dwLenOut < sizeof(DWORD)) {
				SetLastError(ERROR_INVALID_PARAMETER);
				return FALSE;
			}
			*(DWORD *)pBufOut = g_fState;
			if (pdwActualOut)
				*pdwActualOut = sizeof(DWORD);
			return TRUE;
		break;
#endif

#if defined(UNDER_CE) && defined (DEBUG) && ! defined (OLD_CE_BUILD)
		case IOCTL_SERVICE_DEBUG:
			if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST)
				pBufIn = (PBYTE) MapCallerPtr(pBufIn,sizeof(DWORD));

			if (dwLenIn == sizeof(DWORD) && pBufIn)
				dpCurSettings.ulZoneMask = (*(DWORD *)pBufIn);
		break;
#endif

		case IOCTL_SERVICE_REGISTER_SOCKADDR:
			if (!g_fSuperServices) {
				SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
				return FALSE;
			}

			if (pBufIn == NULL) {
				// pBufIn = NULL on first call by services.exe to query whether this is supported or not.
				// if we return FALSE there will be no more calls.
				return IsHttpdEnabled();
			}
			return TRUE;
		break;

		case IOCTL_SERVICE_DEREGISTER_SOCKADDR:
			return TRUE;
		break;

		case IOCTL_SERVICE_STARTED:
			if (InterlockedCompareExchange(&g_fState,SERVICE_STATE_STARTING_UP,SERVICE_STATE_OFF) == SERVICE_STATE_OFF) {
				DEBUGMSG(ZONE_INIT,(L"HTTPD:IOControl(IOCTL_SERVICE_STARTED) creating global variales\r\n"));
				DEBUGCHK(!g_hAdminThread);
				g_hAdminThread = MyCreateThread(InitializeGlobalsThread,0);
				if (g_hAdminThread) {
					CloseHandle(g_hAdminThread);
					return TRUE;
				}
				return FALSE;
			}
			SetLastError(ERROR_SERVICE_ALREADY_RUNNING);
			return FALSE;
		break;

		case IOCTL_SERVICE_CONNECTION:
			// Received when from a super service.
			if (pBufIn && dwLenIn==sizeof(SOCKET)) {
				SOCKET sock = *( (SOCKET*) pBufIn);
				if (g_fState != SERVICE_STATE_ON || !g_pVars->m_fAcceptConnections) {
					SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
					DEBUGMSG(ZONE_ERROR,(L"HTTP: HTP_IOCTL(IOCTL_SERVICE_CONNECTION) fails, HTTPD not running\r\n"));
					closesocket(sock); // we still have to do this much.
					return FALSE;
				}

#if defined (OLD_CE_BUILD)
				SOCKADDR_IN      sockAddr;
#else
				SOCKADDR_STORAGE sockAddr;
#endif
				int              cbSockAddr = sizeof(sockAddr);

				if ((0 != getsockname(sock,(PSOCKADDR) &sockAddr,&cbSockAddr))) {
					DEBUGMSG(ZONE_ERROR,(L"HTTPD: getsockname fails, GLE=0x%08x\r\n",GetLastError()));
					closesocket(sock);
					return FALSE;
				}
				return CreateHTTPConnection(sock,(PSOCKADDR)&sockAddr,cbSockAddr);
			}
		break;

		// network addresses are changing.
		case IOCTL_SERVICE_NOTIFY_ADDR_CHANGE:
			if ((g_fState == SERVICE_STATE_ON) || (g_fState == SERVICE_STATE_STARTING_UP))
				svsutil_HandleNotifyAddrChange(pBufIn,dwLenIn);
		break;

		case IOCTL_SERVICE_SUPPORTED_OPTIONS:
			if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
				pBufOut = (PBYTE) MapCallerPtr(pBufOut,sizeof(DWORD));
				pdwActualOut = (PDWORD) MapCallerPtr(pdwActualOut,sizeof(DWORD));
			}

			if (!pBufOut || dwLenOut < sizeof(DWORD)) {
				SetLastError(ERROR_INVALID_PARAMETER);
				return FALSE;
			}

			dwOut = 0;

			if (g_fIsapiExtensionModule)
				dwOut |= HTTPD_OPTION_ISAPI_EXTENSIONS;

			if (g_fIsapiFilterModule)
				dwOut |= HTTPD_OPTION_ISAPI_FILTERS;

			if (g_fAuthModule)
				dwOut |= (HTTPD_OPTION_AUTHENTICATION | HTTPD_OPTION_SSL);

			if (g_fWebDavModule)
				dwOut |= HTTPD_OPTION_WEBDAV;
			
			*(DWORD *)pBufOut = dwOut;
			if (pdwActualOut)
				*pdwActualOut = sizeof(DWORD);
			return TRUE;

		break;

		// Unhandled Server IOCTLS
		// case IOCTL_SERVICE_INSTALL:
		// case IOCTL_SERVICE_UNINSTALL:
		// case IOCTL_SERVICE_CONTROL:
		// case IOCTL_SERVICE_CONSOLE:
		default:
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: HTP_IOControl unhandled dwCode 0x%08x\r\n",dwCode));
		break;
	}
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
}


//	@func PVOID | HTP_Init | Device initialization routine
//  @parm DWORD | dwData | Info passed to RegisterDevice
//  @rdesc	Returns a DWORD which will be passed to Open & Deinit or NULL if
//			unable to initialize the device.
//	@remark	dwData is a CTelnetContext passed in by our server thread

extern "C" DWORD HTP_Init (DWORD dwData) {
	DEBUGMSG(ZONE_INIT,(L"HTTPD:HTP_Init(0x%08x)\r\n",dwData));

	if (dwData & SERVICE_INIT_STANDALONE) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Error, cannot run in SERVICE_INIT_STANDALONE mode, will not initialize\r\n"));
		SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
		return 0;
	}

	if (InterlockedCompareExchange(&g_fState,SERVICE_STATE_OFF,SERVICE_STATE_UNINITIALIZED) == (INTERLOCKED_RESULT)SERVICE_STATE_UNINITIALIZED)  {
		DEBUGCHK(g_pTimer == NULL);

		DEBUGMSG(ZONE_INIT,(L"HTTPD: Initializing web server: creating thread pool, initing interface mapping...\r\n"));
		DEBUGMSG(ZONE_INIT,(L"HTTPD: Web Server supports the following components: %s %s %s %s\r\n",
			g_fIsapiExtensionModule ? L"ISAPI Extensions," : L"",
			g_fIsapiFilterModule    ? L"ISAPI Filters," : L"",
			g_fAuthModule           ? L"Authentication, SSL," : L"",
			g_fWebDavModule         ? L"Web DAV": L""
		));

		if (! HttpInitializeOnce())
			return FALSE;

		if (! (dwData & SERVICE_INIT_STOPPED))
			return HttpInitialize(NULL);

		g_fSuperServices = TRUE;
		return (DWORD) &g_fState;
	}

	// HTTPD cannot be initialized more than once.
	SetLastError(ERROR_SERVICE_ALREADY_RUNNING);
	return 0;
}

//	@func PVOID | HTP_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from HTP_Init call
//  @rdesc	Returns TRUE for success, FALSE for failure.

extern "C" BOOL HTP_Deinit(DWORD dwData) {
	DEBUGMSG(ZONE_DEVICE, (TEXT("HTP_Deinit(0x%08x)\r\n"),dwData));
	return HttpUnload();
}

//	@func PVOID | HTP_Open		| Device open routine
//  @parm DWORD | dwData		| value returned from HTP_Init call
//  @parm DWORD | dwAccess		| requested access (combination of GENERIC_READ
//								  and GENERIC_WRITE)
//  @parm DWORD | dwShareMode	| requested share mode (combination of
//								  FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc	Returns a DWORD which will be passed to Read, Write, etc or NULL if
//			unable to open device.
extern "C" DWORD HTP_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode) {
	DEBUGMSG (ZONE_DEVICE, (TEXT("HTP_Open(0x%X)\r\n"), dwData));
	return (DWORD)dwData;
}

//	@func BOOL | HTP_Close | Device close routine
//  @parm DWORD | dwOpenData | value returned from HTP_Open call
//  @rdesc	Returns TRUE for success, FALSE for failure
extern "C" BOOL HTP_Close (DWORD dwData)  {
	DEBUGMSG (ZONE_DEVICE, (TEXT("HTP_Close(0x%X)\r\n"), dwData));

	return TRUE;
}

//	@func DWORD | HTP_Write | Device write routine
//  @parm DWORD | dwOpenData | value returned from HTP_Open call
//  @parm LPCVOID | pBuf | buffer containing data
//  @parm DWORD | len | maximum length to write [IN BYTES, NOT WORDS!!!]
//  @rdesc	Returns -1 for error, otherwise the number of bytes written.  The
//			length returned is guaranteed to be the length requested unless an
//			error condition occurs.
extern "C" DWORD HTP_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen) {
	DEBUGMSG (ZONE_DEVICE, (TEXT("HTP_Write(0x%X, 0x%X, %d)\r\n"),
				  dwData, pInBuf, dwInLen));
	return (DWORD)-1;
}

//	@func DWORD | HTP_Read | Device read routine
//  @parm DWORD | dwOpenData | value returned from HTP_Open call
//  @parm LPVOID | pBuf | buffer to receive data
//  @parm DWORD | len | maximum length to read [IN BYTES, not WORDS!!]
//  @rdesc	Returns 0 for end of file, -1 for error, otherwise the number of
//			bytes read.  The length returned is guaranteed to be the length
//			requested unless end of file or an error condition occurs.
//
// !!!!NOTE that this function ALWAYS returns ANSI text, NOT UNICODE!!!!
//
extern "C" DWORD HTP_Read (DWORD dwData, LPVOID pBuf, DWORD dwLen) {
	DEBUGMSG (ZONE_DEVICE, (TEXT("HTP_Read Enter(0x%X, 0x%X, %d)\r\n"), dwData, pBuf, dwLen));
	return (DWORD)-1;
}


//	@func DWORD | HTP_Seek | Device seek routine
//  @parm DWORD | dwOpenData | value returned from HTP_Open call
//  @parm long | pos | position to seek to (relative to type)
//  @parm DWORD | type | FILE_BEGIN, FILE_CURRENT, or FILE_END
//  @rdesc	Returns current position relative to start of file, or -1 on error

extern "C" DWORD HTP_Seek (DWORD dwData, long pos, DWORD type) {
	DEBUGMSG (ZONE_DEVICE, (TEXT("HTP_Seek(0x%X, %d, %d) NOT SUPPORTED\r\n"), dwData, pos, type));
	return (DWORD)-1;
}

//	@func void | HTP_PowerUp | Device powerup routine
//	@comm	Called to restore device from suspend mode.  You cannot call any
//			routines aside from those in your dll in this call.
extern "C" void HTP_PowerUp(void) {
	return;
}
//	@func void | HTP_PowerDown | Device powerdown routine
//	@comm	Called to suspend device.  You cannot call any routines aside from
//			those in your dll in this call.
extern "C" void HTP_PowerDown(void) {
	return;
}


