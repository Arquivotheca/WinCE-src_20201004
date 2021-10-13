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

install.c

FILE HISTORY:
	OmarM     14-Sep-2000

*/

#include "winsock2p.h"

//	Private type definitions

// these are private defines in pm\providers.c
// we should in the future put them in a common/shared place

// flags used by PMInstallProvider
#define PMPROV_DEINSTALL	0x01

// flags used by PMFindProvider
#define PMPROV_USECATID		0x01

// flags used by PMEnumProtocols
#define PMPROV_WSAENUM		0x01


// flags used by PMInstallNameSpace
// share the PMPROV_DEINSTALL flag above
#define PMPROV_ENABLE		0x02
#define PMPROV_DISABLE		0x04


int WSPAPI WSCInstallProvider(
	IN  LPGUID lpProviderId,
	IN  const WCHAR FAR * lpszProviderDllPath,
	IN  const LPWSAPROTOCOL_INFOW lpProtocolInfoList,
	IN  DWORD dwNumberOfEntries,
	OUT LPINT lpErrno) {
	
	INT	Err;


	Err = PMInstallProvider(lpProviderId, lpszProviderDllPath, 
		lpProtocolInfoList, dwNumberOfEntries, 0);

	if (Err) {
		__try {
			*lpErrno = Err;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			// well at least we'll return socket error but they won't know
			// what the error was unless they call WSAGetLastError()
			SetLastError(WSAEFAULT);
		}
		return SOCKET_ERROR;
	}
	
	return ERROR_SUCCESS;

}	// WSCInstallProvider()


int WSPAPI WSCDeinstallProvider(
    IN LPGUID lpProviderId,
    OUT LPINT lpErrno) {
    
	int	Err;

	Err = PMInstallProvider(lpProviderId, NULL, NULL, 0, PMPROV_DEINSTALL);
	
	if (Err) {
		__try {
			*lpErrno = Err;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			// well at least we'll return socket error but they won't know
			// what the error was unless they call WSAGetLastError()
			SetLastError(WSAEFAULT);
		}
		return SOCKET_ERROR;
	}
	
	return ERROR_SUCCESS;	

}	// WSCDeinstallProvider()

 
int WSPAPI WSCEnumProtocols(
	LPINT lpiProtocols,
	LPWSAPROTOCOL_INFOW lpProtocolBuffer,
	LPDWORD lpdwBufferLength,
	LPINT lpErrno) {

	int		cProv;
	// we allocate these two to make sure we don't crash b/c of bad user params
	// PM protects against bad lpProtocolBuffer, so we'll only crash if
	// lpiProtocols is bad.
	INT		Err;
	DWORD	cBuf, Flags = 0;

	
	cBuf = *lpdwBufferLength;

	cProv = PMEnumProtocols(lpiProtocols, lpProtocolBuffer, &cBuf, &Flags, 
				&Err);

	if (SOCKET_ERROR == cProv) {
		__try {
			*lpdwBufferLength = cBuf;
			*lpErrno = Err;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			// well at least we'll return socket error but they won't know
			// what the error was unless they call WSAGetLastError()
			SetLastError(WSAEFAULT);
		}
		return SOCKET_ERROR;
	}
	
	return cProv;

}	// WSCEnumProtocols()


int WSPAPI WSAEnumProtocols(
	LPINT lpiProtocols,
	LPWSAPROTOCOL_INFOW lpProtocolBuffer,
	LPDWORD lpdwBufferLength) {

	int		cProv;
	INT		Err;
	DWORD	Flags = PMPROV_WSAENUM;

	cProv = PMEnumProtocols(lpiProtocols, lpProtocolBuffer, lpdwBufferLength, 
		&Flags, &Err);

	if (SOCKET_ERROR == cProv) {
		ASSERT(Err);
		SetLastError(Err);
	}
	
	return cProv;

}	// WSAEnumProtocols()


INT WSPAPI WSCInstallNameSpace(
	IN LPWSTR lpszIdentifier,
	IN LPWSTR lpszPathName,
	IN DWORD dwNameSpace,
	IN DWORD dwVersion,
	IN LPGUID lpProviderId) {

	int		Err;
	DWORD	Flags;

	Flags = 0;

	Err = PMInstallNameSpace(lpszIdentifier, lpszPathName, dwNameSpace, 
		dwVersion, lpProviderId, &Flags);

	if (Err) {
		SetLastError(Err);
		Err = SOCKET_ERROR;
	}

	return Err;

}	// WSCInstallNameSpace()


INT WSPAPI WSCUnInstallNameSpace(
	IN LPGUID lpProviderId) {

	int		Err;
	DWORD	Flags;

	Flags = PMPROV_DEINSTALL;

	Err = PMInstallNameSpace(NULL, NULL, 0, 0, lpProviderId, &Flags);

	if (Err) {
		SetLastError(Err);
		Err = SOCKET_ERROR;
	}

	return Err;

}	// WSCUnInstallNameSpace()


INT WSAAPI WSAEnumNameSpaceProvidersW(
	IN OUT LPDWORD              lpdwBufferLength,
	OUT    LPWSANAMESPACE_INFOW lpnspBuffer) {
	
	int		cProv, Err;
	DWORD	Flags = 0;

	cProv = PMEnumNameSpaceProviders(lpdwBufferLength, lpnspBuffer, &Flags, 
		&Err);

	if (SOCKET_ERROR == cProv) {
		ASSERT(Err);
		SetLastError(Err);
	}

	return cProv;

}	// WSAEnumNameSpaceProvidersW()


