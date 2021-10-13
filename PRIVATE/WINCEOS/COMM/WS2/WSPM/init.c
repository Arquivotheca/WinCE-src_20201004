//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************/
/**                            Microsoft Windows                            **/
/*****************************************************************************/

/*

init.c

initialization for the winsock service provider mapping into pm
the Win CE protocol manager (afd layer)

FILE HISTORY:
	OmarM     13-Sep-2000
	

*/

#include "wspmp.h"

extern WSPPROC_TABLE	v_aWspmProcTable;

CRITICAL_SECTION	v_WspmCS;
static int			s_cStartups;
WSPUPCALLTABLE		v_UpcallTable;

#define WSPM_SPI_VERSION	MAKEWORD(2,2)


// accept version 2.0 or greater

int WSPStartup (
	WORD			wVersionRequested,
	LPWSPDATA		lpWSPData,
	LPWSAPROTOCOL_INFOW	lpProtocolInfo,
	WSPUPCALLTABLE	UpcallTable,
	LPWSPPROC_TABLE	lpProcTable) {

    int		Err = 0;
    WORD	Major, Minor, OfferMajor, OfferMinor;

	EnterCriticalSection(&v_WspmCS);

	Major = LOBYTE(wVersionRequested);
	Minor = HIBYTE(wVersionRequested);

	if (Major < 2) {
		Err = WSAVERNOTSUPPORTED;
	} else {

		OfferMajor = 2;
		if (Minor < 2)
			OfferMinor = Minor;
		else
			OfferMinor = 2;

		lpWSPData->wVersion = MAKEWORD(OfferMajor, OfferMinor);
		lpWSPData->wHighVersion = WSPM_SPI_VERSION;
		wcscpy(lpWSPData->szDescription, TEXT("Winsock 2.2"));

		s_cStartups++;
		ASSERT(s_cStartups > 0);

		v_UpcallTable = UpcallTable;
		*lpProcTable = v_aWspmProcTable;

	}
	
	LeaveCriticalSection(&v_WspmCS);

	return Err;
	
}	// WSPStartup()


int WSPAPI WSPCleanup (
	LPINT lpErrno) {

	int	Err = 0;

	EnterCriticalSection(&v_WspmCS);

	if (s_cStartups > 0) {
		s_cStartups--;
	} else {
		Err = WSANOTINITIALISED;
		ASSERT(0);
		RETAILMSG(1, (TEXT("WSPM: WSPCleanup: s_cStartups = %d\r\n"), 
			s_cStartups));
	}
	
	LeaveCriticalSection(&v_WspmCS);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}

	return Err;

}	// WSPCleanup()


BOOL __stdcall
DllEntry (HANDLE  hinstDLL, DWORD Op, LPVOID  lpvReserved) {

	switch (Op) {
	case DLL_PROCESS_ATTACH:
		InitializeCriticalSection(&v_WspmCS);
        DisableThreadLibraryCalls ((HMODULE)hinstDLL);
		break;

	case DLL_PROCESS_DETACH:
		DeleteCriticalSection(&v_WspmCS);
		break;
		
	default:
		break;
	}
	return TRUE;
	
}	// dllentry()



