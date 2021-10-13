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

install.c

FILE HISTORY:
	OmarM     14-Sep-2000

*/

#include "winsock2p.h"



int WSPAPI WSCInstallProvider(
	IN  LPGUID lpProviderId,
	IN  const WCHAR FAR * lpszProviderDllPath,
	IN  const LPWSAPROTOCOL_INFOW lpProtocolInfoList,
	IN  DWORD dwNumberOfEntries,
	OUT LPINT lpErrno) {
	
	INT	Err;


	Err = PMInstallProvider(lpProviderId, lpszProviderDllPath, 
		lpProtocolInfoList, sizeof(*lpProtocolInfoList), dwNumberOfEntries, 0);

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

	Err = PMInstallProvider(lpProviderId, NULL, NULL, 0, 0, PMPROV_DEINSTALL);
	
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

	// BUGBUG: check for bad params
	cBuf = *lpdwBufferLength;

	cProv = PMEnumProtocols(lpiProtocols, lpProtocolBuffer, cBuf,  &cBuf,
			&Flags, &Err);

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

	if (lpdwBufferLength) {

		cProv = PMEnumProtocols(lpiProtocols, lpProtocolBuffer, *lpdwBufferLength, 
			lpdwBufferLength, &Flags, &Err);
	} else {
		cProv = SOCKET_ERROR;
		Err = WSAEFAULT;
	}

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

	if (lpdwBufferLength) {
		cProv = PMEnumNameSpaceProviders(lpdwBufferLength, lpnspBuffer, 
			*lpdwBufferLength, &Flags, &Err);
	} else {
		cProv = SOCKET_ERROR;
		Err = WSAEFAULT;
	}

	if (SOCKET_ERROR == cProv) {
		ASSERT(Err);
		SetLastError(Err);
	}

	return cProv;

}	// WSAEnumNameSpaceProvidersW()


