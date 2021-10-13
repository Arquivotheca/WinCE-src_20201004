//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
// 
//      Bluetooth Name Space Layer
// 
// 
// Module Name:
// 
//      bthns.cxx
// 
// Abstract:
// 
//      This file implements the Name Space provider.  This name space provider
//      lives in btd.dll and is called by Winsock2 during WSASetService
//      and WSALookup calls.
// 
// 
//------------------------------------------------------------------------------


#include <windows.h>
#include <winsock2.h>
#include <ws2spi.h>
#include <svsutil.hxx>
#include <bt_api.h>



#define BTHNS_THREAD_SAFTEY 0

#if 0

#if BTHNS_THREAD_SAFTEY
// Currently there is no need for any thread saftey at this level,
// leaving code in place (#ifdef'd out) in case this changes.
// Note that if this is added, would need to be placed in btdrt.dll's DllMain.
CRITICAL_SECTION	g_bthNsCritSec;
static int			g_cStartups;
#endif

BOOL WINAPI DllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpReserved) {
#if BTHNS_THREAD_SAFTEY
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		g_cStartups = 0;
		InitializeCriticalSection(&g_bthNsCritSec);
		DisableThreadLibraryCalls ((HMODULE)hInstance);
		break;

	case DLL_PROCESS_DETACH:
		DeleteCriticalSection(&g_bthNsCritSec);
		break;

	default:
		break;
	}
#endif // BTHNS_THREAD_SAFTEY
	return TRUE;
}
#endif // 0


int WINAPI BTH_NSPCleanup(LPGUID lpProviderId) {
#if BTHNS_THREAD_SAFTEY
	int iError = ERROR_SUCCESS;

	__try {
		if (memcmp(lpProviderId, &NsId, sizeof(GUID)))
			iError = WSAEINVAL;
		else {
			EnterCriticalSection(&g_bthNsCritSec);
			g_cStartups--;
			if (g_cStartups == 0) {
				// cleanup - currently nothing is allocated at this layer.
			}

			LeaveCriticalSection(&g_bthNsCritSec);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		iError = WSAEFAULT;
	}

	if (iError) {
		SetLastError(iError);
		iError = SOCKET_ERROR;
	}
	return iError;
#else
	return ERROR_SUCCESS;
#endif // BTHNS_THREAD_SAFTEY
}

int WSAAPI BTH_NSPSetServiceThunk(LPGUID lpProviderId, LPWSASERVICECLASSINFOW lpServiceClassInfo, LPWSAQUERYSETW lpqsRegInfo, WSAESETSERVICEOP essOperation, DWORD dwControlFlags) {
	return BthNsSetService(lpqsRegInfo,essOperation,dwControlFlags);
}

int WSAAPI BTH_NSPLookupServiceBeginThunk(LPGUID pProviderId, LPWSAQUERYSETW pRestrict, LPWSASERVICECLASSINFOW pServiceClass, DWORD ControlFlags, LPHANDLE phLookup) {
	return BthNsLookupServiceBegin(pRestrict,ControlFlags,phLookup);
}

int WSAAPI BTH_NSPLookupServiceNextThunk(HANDLE hLookup, DWORD ControlFlags, DWORD *pcBuf, LPWSAQUERYSETW pResults) {
	return BthNsLookupServiceNext(hLookup,ControlFlags,pcBuf,pResults);
}

int WINAPI BTH_NSPLookupServiceEndThunk(HANDLE hLookup) {
	return BthNsLookupServiceEnd(hLookup);
}

int WSAAPI BTH_NSPRemoveServiceClass(LPGUID pProviderId, LPGUID lpServiceClassId) {
	ASSERT(0);
	SetLastError(WSAEOPNOTSUPP);
	return SOCKET_ERROR;
}

int WSAAPI BTH_NSPGetServiceClassInfo(LPGUID lpProviderId, LPDWORD lpdwBufSize, LPWSASERVICECLASSINFOW lpServiceClass) {
	ASSERT(0);
	SetLastError(WSAEOPNOTSUPP);
	return SOCKET_ERROR;
}

int WSAAPI BTH_NSPIoctlThunk(HANDLE hLookup,	DWORD dwControlCode, LPVOID lpvInBuffer, DWORD cbInBuffer, LPVOID lpvOutBuffer,
                    DWORD cbOutBuffer, LPDWORD lpcbBytesReturned, LPWSACOMPLETION lpCompletion,	LPWSATHREADID lpThreadId) 
{
	ASSERT(0);
	SetLastError(WSAEOPNOTSUPP);
	return SOCKET_ERROR;
}

int WINAPI BTH_NSPInstallServiceClass(LPGUID pProviderId, PWSASERVICECLASSINFOW  pServiceClassInfo) {
	ASSERT(0);
	SetLastError( WSAEOPNOTSUPP );
	return( SOCKET_ERROR );
}

//
//   Called during name space provider installation.
//
int WSAAPI NSPStartup(LPGUID lpProviderId, LPNSP_ROUTINE lpnspRoutines) {
	int iError = ERROR_SUCCESS;

	__try {
		// should we check the provider id???
		
		// NSPIoctl was apparently newly added so size they give us may not
		// include it
		if (lpnspRoutines->cbSize < ((int)(&(((NSP_ROUTINE *)0)->NSPIoctl)))) {
			iError = WSAEINVAL;
		} else {
			lpnspRoutines->dwMajorVersion = 1;
			lpnspRoutines->dwMinorVersion = 1;

			lpnspRoutines->NSPCleanup = &BTH_NSPCleanup;
			lpnspRoutines->NSPLookupServiceBegin = &BTH_NSPLookupServiceBeginThunk;
			lpnspRoutines->NSPLookupServiceNext = &BTH_NSPLookupServiceNextThunk;
			lpnspRoutines->NSPLookupServiceEnd = &BTH_NSPLookupServiceEndThunk;
			lpnspRoutines->NSPSetService = &BTH_NSPSetServiceThunk;
			lpnspRoutines->NSPInstallServiceClass = &BTH_NSPInstallServiceClass;
			lpnspRoutines->NSPRemoveServiceClass = &BTH_NSPRemoveServiceClass;
			lpnspRoutines->NSPGetServiceClassInfo = &BTH_NSPGetServiceClassInfo;

#if BTHNS_THREAD_SAFTEY
			EnterCriticalSection(&g_bthNsCritSec);
			g_cStartups++;
			ASSERT(g_cStartups > 0);
			LeaveCriticalSection(&g_bthNsCritSec);
#endif // BTHNS_THREAD_SAFTEY
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		iError = WSAEFAULT;
	}

	if (iError) {
		SetLastError(iError);
		iError = SOCKET_ERROR;
	}

	return iError;
}
