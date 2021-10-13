//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
**
** rasxport.h
** Remote Access external API
*/

#ifndef _RASXPORT_H_
#define _RASXPORT_H_

#include "ras.h"
#include "tapi.h"


// Prototypes

DWORD APIENTRY
AfdRasDial( LPRASDIALEXTENSIONS, LPTSTR, LPRASDIALPARAMS,
		   DWORD, LPVOID, LPHRASCONN );

DWORD APIENTRY
AfdRasEnumConnections( LPRASCONN, LPDWORD, LPDWORD );

DWORD APIENTRY
AfdRasEnumEntries( LPTSTR, LPTSTR, LPRASENTRYNAME, LPDWORD, LPDWORD );

DWORD APIENTRY
AfdRasGetConnectStatus( HRASCONN, LPRASCONNSTATUS );

DWORD APIENTRY
AfdRasGetErrorString( UINT, LPTSTR, DWORD );

DWORD APIENTRY
AfdRasHangUp ( HRASCONN );

DWORD APIENTRY
AfdRasGetProjectionInfo( HRASCONN, RASPROJECTION, LPVOID, LPDWORD );

DWORD APIENTRY
AfdRasIOControl( LPVOID, DWORD, PBYTE, DWORD, PBYTE, DWORD, PDWORD );

DWORD APIENTRY
AfdRasGetEntryDialParams( LPTSTR, LPRASDIALPARAMS, LPBOOL );

DWORD APIENTRY
AfdRasSetEntryDialParams( LPTSTR, LPRASDIALPARAMS, BOOL );

DWORD APIENTRY
AfdRasGetEntryProperties(LPTSTR lpszPhonebook, LPTSTR szEntry,
	LPBYTE lpbEntry, LPDWORD lpdwEntrySize, LPBYTE lpb, LPDWORD lpdwSize);

DWORD APIENTRY
AfdRasValidateEntryName(LPCTSTR lpszPhonebook, LPCTSTR lpszEntry);

DWORD APIENTRY
AfdRasSetEntryProperties(LPTSTR lpszPhonebook, LPTSTR szEntry,
	LPBYTE lpbEntry, DWORD dwEntrySize, LPBYTE lpb, DWORD dwSize);

DWORD APIENTRY
AfdRasRenameEntry(LPTSTR lpszPhonebook, LPTSTR szEntryOld, LPTSTR szEntryNew);

DWORD APIENTRY
AfdRasDeleteEntry(LPTSTR lpszPhonebook, LPTSTR szEntry);

DWORD APIENTRY
AfdRasEnumConnections( LPRASCONN, LPDWORD, LPDWORD );

DWORD APIENTRY
AfdRasGetConnectStatus( HRASCONN, LPRASCONNSTATUS );

DWORD APIENTRY
AfdRasGetEntryDevConfig (LPCTSTR szPhonebook, LPCTSTR szEntry,
						 LPDWORD pdwDeviceID, LPDWORD pdwSize,
						 LPVARSTRING pDeviceConfig);

DWORD APIENTRY
AfdRasSetEntryDevConfig (LPCTSTR szPhonebook, LPCTSTR szEntry,
						 DWORD dwDeviceID, LPVARSTRING lpDeviceConfig);


#endif // _RASXPORT_H_
