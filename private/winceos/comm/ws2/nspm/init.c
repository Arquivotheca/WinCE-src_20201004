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
/*****************************************************************************/
/**                            Microsoft Windows                            **/
/*****************************************************************************/

/*

init.c

initialization for the winsock name service provider 

FILE HISTORY:
	OmarM     29-Sep-2000
	

*/

#include "nspmp.h"

extern WSPPROC_TABLE	v_aWspmProcTable;

CRITICAL_SECTION	v_NspmCS;
static int			s_cStartups;

// this is copied from the initialization code in
// pm2\ws2instl
GUID		NsId = { 0x3c8441d3, 0xb4f9, 0x4ea0, {0x89, 0x74, 0xf4, 0xc3,
						0x49, 0x66, 0x51, 0x91}};


int WSAAPI NSPCleanup (
	LPGUID lpProviderId) {

	int	Err = 0;

	__try {
		if (memcmp(lpProviderId, &NsId, sizeof(GUID)))
			Err = WSAEINVAL;
		else {
			EnterCriticalSection(&v_NspmCS);

			if (s_cStartups > 0) {
				s_cStartups--;
			} else {
				Err = WSANOTINITIALISED;
				ASSERT(0);
				RETAILMSG(1, (TEXT("NSPM: NSPCleanup: s_cStartups = %d\r\n"), 
					s_cStartups));
			}
			
			LeaveCriticalSection(&v_NspmCS);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		Err = WSAEFAULT;
		ASSERT(0);
	}

	if (Err) {
		SetLastError(Err);
		Err = SOCKET_ERROR;
	}

	return Err;

}	// NSPCleanup()


INT WSAAPI NSPStartup(
	LPGUID lpProviderId,
	LPNSP_ROUTINE lpnspRoutines) {

    int		Err = 0;

	__try {
		// should we check the provider id???
		
		// NSPIoctl was apparently newly added so size they give us may not
		// include it
		if (lpnspRoutines->cbSize < ((int)(&(((NSP_ROUTINE *)0)->NSPIoctl)))) {
			Err = WSAEINVAL;
		} else {
			lpnspRoutines->dwMajorVersion = 1;
			lpnspRoutines->dwMinorVersion = 1;

			lpnspRoutines->NSPCleanup = &NSPCleanup;
			lpnspRoutines->NSPLookupServiceBegin = &NSPLookupServiceBegin;
			lpnspRoutines->NSPLookupServiceNext = &NSPLookupServiceNext;
			lpnspRoutines->NSPLookupServiceEnd = &NSPLookupServiceEnd;
			lpnspRoutines->NSPSetService = &NSPSetService;
			lpnspRoutines->NSPInstallServiceClass = &NSPInstallServiceClass;
			lpnspRoutines->NSPRemoveServiceClass = &NSPRemoveServiceClass;
			lpnspRoutines->NSPGetServiceClassInfo = &NSPGetServiceClassInfo;

			EnterCriticalSection(&v_NspmCS);
			s_cStartups++;
			ASSERT(s_cStartups > 0);
			LeaveCriticalSection(&v_NspmCS);
		}
	}
	
	__except (EXCEPTION_EXECUTE_HANDLER) {
		Err = WSAEFAULT;
		ASSERT(0);
	}

	if (Err) {
		SetLastError(Err);
		Err = SOCKET_ERROR;
	}
	
	return Err;
	
}	// NSPStartup()



BOOL __stdcall
DllEntry (HANDLE  hinstDLL, DWORD Op, LPVOID  lpvReserved) {

	switch (Op) {
	case DLL_PROCESS_ATTACH:
		InitializeCriticalSection(&v_NspmCS);
        DisableThreadLibraryCalls ((HMODULE)hinstDLL);
		break;

	case DLL_PROCESS_DETACH:
		DeleteCriticalSection(&v_NspmCS);
		break;
		
	default:
		break;
	}
	return TRUE;
	
}	// dllentry()



