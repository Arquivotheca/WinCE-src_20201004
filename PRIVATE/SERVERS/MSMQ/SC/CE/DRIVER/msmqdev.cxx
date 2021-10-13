//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

/*--
Module Name: MSMQD.CXX
Abstract: MSMQ service
--*/


#include <windows.h>
#include <stdio.h>

#include <mq.h>

#include <sc.hxx>
#include <scapi.h>
#include <scsrmp.hxx>

#include <service.h>
#include <mqmgmt.h>

extern "C" DWORD WINAPI scmain_StartDLL (LPVOID lpParm);
extern "C" HRESULT	scmain_StopDLL (void);

static long		    gs_fApiReady	= FALSE;
// static HANDLE       gs_hDevice		= NULL;
static const WCHAR *gs_MachineToken = MO_MACHINE_TOKEN;
unsigned long       glServiceState  = SERVICE_STATE_OFF;

#if defined (SC_VERBOSE)
extern unsigned int g_bCurrentMask;
extern unsigned int g_bOutputChannels;
#endif

extern "C" BOOL WINAPI DllEntry( HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
{
	switch(fdwReason) {
		case DLL_PROCESS_ATTACH:	{
			SERVICE_FIND_CALLER(gfCallerExecutable);
			SVSUTIL_ASSERT(gfCallerExecutable == SERVICE_CALLER_PROCESS_SERVICES_EXE || gfCallerExecutable == SERVICE_CALLER_PROCESS_DEVICE_EXE);

#if ! defined (MSMQ_ANCIENT_OS)
			DisableThreadLibraryCalls((HMODULE)hInstDll);
#endif
		}
			break;

		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

extern "C" int MSMQDInitialize(TCHAR *szRegPath)
{
	return scmain_StartDLL (NULL) ? TRUE : FALSE;
}

extern "C" int MSMQDUnInitialize(void)
{
	return scmain_StopDLL ();
}

int scce_RegisterDLL (void) {
	if (InterlockedExchange (&gs_fApiReady, TRUE))
		return FALSE;

	return gs_fApiReady;
}

int scce_UnregisterDLL (void) {
	gs_fApiReady = FALSE;

	return TRUE;
}

// Changes whether MSMQ is started or stopped at boot time
void SetRegistryStartup(DWORD dwValue) {
	HKEY  hKey;

	// if registry key doesn't exist, don't create it.  "msmqadm -register" needs
	// to create it to fill in all the other relevant registry settings.
	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, MSMQ_SC_REGISTRY_KEY, 0, KEY_READ | KEY_WRITE, &hKey)) {
		RegSetValueEx(hKey, L"CEStartAtBoot", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
		RegCloseKey(hKey);
	}
}


//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//		EXECUTION THREAD: Client-application!
//			These functions are only executed on the caller's thread
// 			i.e. the thread belongs to the client application
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//	@func PVOID | MMQ_Init | Device initialization routine
//  @parm DWORD | dwInfo | Info passed to RegisterDevice
//  @rdesc	Returns a DWORD which will be passed to Open & Deinit or NULL if
//			unable to initialize the device.
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//			in as lpszType in RegisterDevice

// NOTE: Since this function can be called in DllMain it cannot block for 
// anything otherwise we run a risk of deadlocking
extern "C" DWORD MMQ_Init (DWORD Index)
{
	if (Index & SERVICE_INIT_STANDALONE) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FATAL, L"service cannot run in SERVICE_INIT_STANDALONE mode, will not initialize\n");
#endif
		SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
		return 0;
	}

	return MSMQDInitialize(NULL);
}

//	@func PVOID | MMQ_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from CON_Init call
//  @rdesc	Returns TRUE for success, FALSE for failure.
//	@remark	Routine exported by a device driver.  "PRF" is the string
//			passed in as lpszType in RegisterDevice

// NOTE: Since this function can be called in DllMain it cannot block for 
// anything otherwise we run a risk of deadlocking
extern "C" BOOL MMQ_Deinit(DWORD dwData)
{
	return scmain_ForceExit();
}

//	@func PVOID | MMQ_Open		| Device open routine
//  @parm DWORD | dwData		| value returned from CON_Init call
//  @parm DWORD | dwAccess		| requested access (combination of GENERIC_READ
//								  and GENERIC_WRITE)
//  @parm DWORD | dwShareMode	| requested share mode (combination of
//								  FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc	Returns a DWORD which will be passed to Read, Write, etc or NULL if
//			unable to open device.
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//			in as lpszType in RegisterDevice
extern "C" DWORD MMQ_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode)
{
	HANDLE *hCallerProc = (HANDLE *)g_funcAlloc (sizeof(HANDLE), g_pvAllocData);

	*hCallerProc = NULL;

	return (DWORD)hCallerProc;
}

//	@func BOOL | MMQ_Close | Device close routine
//  @parm DWORD | dwOpenData | value returned from MMQ_Open call
//  @rdesc	Returns TRUE for success, FALSE for failure
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//			in as lpszType in RegisterDevice
extern "C" BOOL MMQ_Close (DWORD dwData) 
{
	HANDLE *hCallerProc = (HANDLE *)dwData;

	if (hCallerProc && (*hCallerProc))
		scapi_ProcExit (*hCallerProc);

	if (hCallerProc)
		g_funcFree (hCallerProc, g_pvFreeData);

	return TRUE;
}

//	@func DWORD | MMQ_Write | Device write routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm LPCVOID | pBuf | buffer containing data
//  @parm DWORD | len | maximum length to write [IN BYTES, NOT WORDS!!!]
//  @rdesc	Returns -1 for error, otherwise the number of bytes written.  The
//			length returned is guaranteed to be the length requested unless an
//			error condition occurs.
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//			in as lpszType in RegisterDevice
//
extern "C" DWORD MMQ_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen)
{
	return -1;
}

//	@func DWORD | MMQ_Read | Device read routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm LPVOID | pBuf | buffer to receive data
//  @parm DWORD | len | maximum length to read [IN BYTES, not WORDS!!]
//  @rdesc	Returns 0 for end of file, -1 for error, otherwise the number of
//			bytes read.  The length returned is guaranteed to be the length
//			requested unless end of file or an error condition occurs.
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//			in as lpszType in RegisterDevice
//
extern "C" DWORD MMQ_Read (DWORD dwData, LPVOID pBuf, DWORD Len)
{
	return -1;
}

//	@func DWORD | MMQ_Seek | Device seek routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm long | pos | position to seek to (relative to type)
//  @parm DWORD | type | FILE_BEGIN, FILE_CURRENT, or FILE_END
//  @rdesc	Returns current position relative to start of file, or -1 on error
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//		 in as lpszType in RegisterDevice

extern "C" DWORD MMQ_Seek (DWORD dwData, long pos, DWORD type)
{
	return (DWORD)-1;
}

//	@func BOOL | MMQ_IOControl | Device IO control routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm DWORD | dwCode | io control code to be performed
//  @parm PBYTE | pBufIn | input data to the device
//  @parm DWORD | dwLenIn | number of bytes being passed in
//  @parm PBYTE | pBufOut | output data from the device
//  @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device
//  @rdesc	Returns TRUE for success, FALSE for failure
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//		in as lpszType in RegisterDevice

extern "C" BOOL MMQ_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn,
			  DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
			  PDWORD pdwActualOut)
{
#if ! defined (SDK_BUILD)
	if (dwCode == IOCTL_PSL_NOTIFY) {
		PDEVICE_PSL_NOTIFY pPslPacket = (PDEVICE_PSL_NOTIFY)pBufIn;
		if ((pPslPacket->dwSize == sizeof(DEVICE_PSL_NOTIFY)) && (pPslPacket->dwFlags == DLL_PROCESS_EXITING)){
			SVSUTIL_ASSERT (*(HANDLE *)dwData == pPslPacket->hProc);
			scapi_ProcExit ((HANDLE)pPslPacket->hProc);
		}

		return STATUS_SUCCESS;
	}
#endif

	if (dwData != TRUE) {
		HANDLE *hCallerProc = (HANDLE *)dwData;
		SVSUTIL_ASSERT (hCallerProc);

		if (! *hCallerProc)
			*hCallerProc = GetCallerProcess ();

		SVSUTIL_ASSERT (*hCallerProc == GetCallerProcess());
	}

	HRESULT hr = MQ_ERROR;

	switch (dwCode) {
	case MQAPI_CODE_MQCreateQueue:
		hr = scapi_MQCreateQueue (0, (SCPROPVAR *)pBufIn, (WCHAR *)pBufOut,
									(DWORD *)pdwActualOut);
		break;

	case MQAPI_CODE_MQDeleteQueue:
		hr = scapi_MQDeleteQueue ((WCHAR *)pBufIn);
		break;

	case MQAPI_CODE_MQGetQueueProperties:
		hr = scapi_MQGetQueueProperties ((WCHAR *)pBufIn, (SCPROPVAR *)pBufOut);
		break;

	case MQAPI_CODE_MQSetQueueProperties:
		hr = scapi_MQSetQueueProperties ((WCHAR *)pBufIn, (SCPROPVAR *)pBufOut);
		break;

	case MQAPI_CODE_MQOpenQueue:
		hr = scapi_MQOpenQueue ((WCHAR *)pBufIn, (DWORD)dwLenIn, (DWORD)dwLenOut, (SCHANDLE *)pBufOut);
		break;

	case MQAPI_CODE_MQCloseQueue:
		hr = scapi_MQCloseQueue ((SCHANDLE)dwLenIn);
		break;

	case MQAPI_CODE_MQCreateCursor:
		hr = scapi_MQCreateCursor ((SCHANDLE)dwLenIn, (SCHANDLE *)pBufIn);
		break;

	case MQAPI_CODE_MQCloseCursor:
		hr = scapi_MQCloseCursor((SCHANDLE)dwLenIn);
		break;

	case MQAPI_CODE_MQHandleToFormatName:
		hr = scapi_MQHandleToFormatName ((SCHANDLE)dwLenIn, (WCHAR *)pBufIn, (DWORD *)pBufOut);
		break;

	case MQAPI_CODE_MQPathNameToFormatName:
		hr = scapi_MQPathNameToFormatName ((WCHAR *)pBufIn, (WCHAR *)pBufOut, (DWORD *)pdwActualOut);
		break;

	case MQAPI_CODE_MQSendMessage:
		hr = scapi_MQSendMessage ((SCHANDLE)dwLenIn, (SCPROPVAR *)pBufIn, (int)dwLenOut);
		break;

	case MQAPI_CODE_MQReceiveMessage:
		{
			DataMQReceiveMessage *pdm = (DataMQReceiveMessage *)pBufIn;
			HANDLE hCallerProc = GetCallerProcess ();
			hr = scapi_MQReceiveMessage (pdm->hQueue, pdm->dwTimeout, pdm->dwAction,
				(SCPROPVAR *)MapPtrToProcess (pdm->pMsgProps, hCallerProc),
				(OVERLAPPED *)MapPtrToProcess(pdm->lpOverlapped, hCallerProc),
				(PMQRECEIVECALLBACK)MapPtrToProcess (pdm->pfnReceiveCallback, hCallerProc),
				pdm->hCursor, pdm->iNull3);
		}
		break;

	case MQAPI_CODE_MQGetMachineProperties:
		hr = scapi_MQGetMachineProperties ((WCHAR *)pBufIn, (GUID *)pBufOut, (SCPROPVAR *)pdwActualOut);
		break;

	case MQAPI_CODE_MQBeginTransaction:
		hr = scapi_MQBeginTransaction ((ITransaction **)pBufIn);
		break;

	case MQAPI_CODE_MQGetQueueSecurity:
		hr = scapi_MQGetQueueSecurity ((WCHAR *)pBufIn, (SECURITY_INFORMATION)dwLenIn, (SECURITY_DESCRIPTOR *)pBufOut,
									(DWORD)dwLenOut, (DWORD *)pdwActualOut);
		break;

	case MQAPI_CODE_MQSetQueueSecurity:
		hr = scapi_MQSetQueueSecurity ((WCHAR *)pBufIn, (SECURITY_INFORMATION)dwLenIn, (SECURITY_DESCRIPTOR *)pBufOut);
		break;

	case MQAPI_CODE_MQGetSecurityContext:
		hr = scapi_MQGetSecurityContext ((VOID *)pBufIn, (DWORD)dwLenIn, (HANDLE *)pBufOut);
		break;

	case MQAPI_CODE_MQFreeSecurityContext:
		hr = MQ_OK;
		scapi_MQFreeSecurityContext ((HANDLE)dwLenIn);
		break;

	case MQAPI_CODE_MQInstanceToFormatName:
		hr = scapi_MQInstanceToFormatName ((GUID *)pBufIn, (WCHAR *)pBufOut, (DWORD *)pdwActualOut);
		break;

	case MQAPI_CODE_MQLocateBegin:
		hr = scapi_MQLocateBegin ((WCHAR *)pBufIn, (MQRESTRICTION *)pBufOut, (MQCOLUMNSET *)pdwActualOut,
			(MQSORTSET *)MapPtrToProcess ((void *)dwLenIn, GetCallerProcess()),
			(HANDLE *)MapPtrToProcess ((void *)dwLenOut, GetCallerProcess()));
		break;

	case MQAPI_CODE_MQLocateNext:
		hr = scapi_MQLocateNext ((HANDLE)dwLenIn, (DWORD *)pBufIn, (PROPVARIANT *)pBufOut);
		break;

	case MQAPI_CODE_MQLocateEnd:
		hr = scapi_MQLocateEnd ((HANDLE)dwLenIn);
		break;

	case MQAPI_CODE_MQMgmtGetInfo2:
		hr = scmgmt_MQMgmtGetInfo2 ((LPCWSTR)pBufIn, (LPCWSTR)pBufOut, (DWORD)dwLenIn,
							(PROPID *)pdwActualOut, (PROPVARIANT *)MapPtrToProcess((void *)dwLenOut, GetCallerProcess()));
		break;

	case MQAPI_CODE_MQMgmtAction:
		hr = scmgmt_MQMgmtAction ((LPCWSTR)pBufIn, (LPCWSTR)pBufOut, (LPCWSTR)pdwActualOut);
		break;


	case MQAPI_Code_SRMPControl: {
		if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
			pBufIn  = (PBYTE) MapCallerPtr(pBufIn, sizeof(SrmpIOCTLPacket));
			pBufOut = (PBYTE) MapCallerPtr(pBufOut,sizeof(DWORD));
		}

		if (!g_fHaveSRMP || !pBufIn || dwLenIn != sizeof(SrmpIOCTLPacket) || !pBufOut || dwLenOut != sizeof(DWORD)) {
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		}

		SrmpIOCTLPacket SrmpPacket;

		SrmpIOCTLPacket *pInPacket = (SrmpIOCTLPacket *) pBufIn;
		SrmpPacket.pszHeaders      = (PSTR) MapCallerPtr(pInPacket->pszHeaders,pInPacket->cbHeaders);
		SrmpPacket.pszPost         = (PSTR) MapCallerPtr(pInPacket->pszPost,pInPacket->cbPost);
		SrmpPacket.cbHeaders       = pInPacket->cbHeaders;
		SrmpPacket.cbPost          = pInPacket->cbPost;
		SrmpPacket.contentType     = pInPacket->contentType;
		SrmpPacket.fSSL            = pInPacket->fSSL;
		SrmpPacket.dwIP4Addr       = pInPacket->dwIP4Addr;

		SrmpAcceptHttpRequest(&SrmpPacket,(DWORD*) pBufOut);
		break;
	}


	//
	// Services IOControl codes
	//
	// for service IO Controls return FALSE (as opposed to HRESULT).  This
	// is necessary because services treats IOControl return value as a BOOL when
	// reporting errors to client above, however MSMQ client relies on hr to return
	// error code.

	case IOCTL_SERVICE_START:
		// only perform registry operations on SERVICE IOControls because other
		// IOControls are called by existing MSMQ admin utilities which are responsible for setting.

		SetRegistryStartup(1);
		hr = (scmgmt_MQMgmtAction (NULL, gs_MachineToken, MACHINE_ACTION_STARTUP) == ERROR_SUCCESS) ? 1 : 0;
		if (!hr)
			SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
		break;

	case IOCTL_SERVICE_STOP:
		SetRegistryStartup(0);
		hr = (scmgmt_MQMgmtAction (NULL, gs_MachineToken, MACHINE_ACTION_SHUTDOWN) == ERROR_SUCCESS) ? 1 : 0;
		if (!hr)
			SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
		break;

	case IOCTL_SERVICE_CONSOLE:
		hr = (scmgmt_MQMgmtAction (NULL, gs_MachineToken, MACHINE_ACTION_CONSOLE) == ERROR_SUCCESS) ? 1 : 0;
		if (!hr)
			SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
		break;

#if defined (SC_VERBOSE)
	case IOCTL_SERVICE_DEBUG:
		if ((dwLenIn == sizeof(L"console")) && (wcsicmp ((WCHAR *)pBufIn, L"console") == 0))
			g_bOutputChannels = VERBOSE_OUTPUT_CONSOLE;
		else if ((dwLenIn == sizeof(L"serial")) && (wcsicmp ((WCHAR *)pBufIn, L"serial") == 0))
			g_bOutputChannels = VERBOSE_OUTPUT_DEBUGMSG;
		else if ((dwLenIn == sizeof(L"file")) && (wcsicmp ((WCHAR *)pBufIn, L"file") == 0))
			g_bOutputChannels = VERBOSE_OUTPUT_LOGFILE;
		else if (dwLenIn == sizeof(DWORD))
			g_bCurrentMask = *(unsigned int *)pBufIn;
		else {
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		}

		return TRUE;
		break;
#endif

	case IOCTL_SERVICE_STATUS:
		if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
			pBufOut = (PBYTE) MapCallerPtr(pBufOut,sizeof(DWORD));
			pdwActualOut = (PDWORD) MapCallerPtr(pdwActualOut,sizeof(DWORD));
		}

		if (!pBufOut || dwLenOut < sizeof(DWORD)) {
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		}

		*(DWORD *)pBufOut = glServiceState;
		if (pdwActualOut)
			*pdwActualOut = sizeof(DWORD);
		return TRUE;

	default:
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FATAL, L"Unknown control code %d\n", dwCode);
#endif
		hr = 0;  // for unknown service IOCtls (not HRESULT).  It's safe
		         // to assume MQAPI_CODE_xxx codes are generated internally
		         // in MSMQ API, so there'll never be case where MSMQ client
		         // is sending in an unknown code.
		SetLastError(ERROR_INVALID_PARAMETER);
	}

	return hr;
}

//	@func void | MMQ_PowerUp | Device powerup routine
//	@comm	Called to restore device from suspend mode.  You cannot call any
//			routines aside from those in your dll in this call.
extern "C" void MMQ_PowerUp(void)
{
	return;
}
//	@func void | MMQ_PowerDown | Device powerdown routine
//	@comm	Called to suspend device.  You cannot call any routines aside from
//			those in your dll in this call.
extern "C" void MMQ_PowerDown(void)
{
	return;
}

//
//	These do nothing in DLL
//
void scce_Listen (void) {
}

HRESULT scce_Shutdown (void) {
	return MQ_OK;
}
