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
AfdRasDial( LPRASDIALEXTENSIONS, DWORD, LPCTSTR, LPRASDIALPARAMS,
           DWORD, DWORD, DWORD, LPHRASCONN );

DWORD APIENTRY
AfdRasEnumConnections( LPRASCONN, DWORD, LPDWORD, LPDWORD );

DWORD APIENTRY
AfdRasEnumEntries( LPCTSTR, LPCTSTR, LPRASENTRYNAME, DWORD, LPDWORD, LPDWORD );

DWORD APIENTRY
AfdRasGetConnectStatus( HRASCONN, LPRASCONNSTATUS, DWORD );

DWORD APIENTRY
AfdRasGetErrorString( UINT, LPTSTR, DWORD );

DWORD APIENTRY
AfdRasHangUp ( HRASCONN );

DWORD APIENTRY
AfdRasGetProjectionInfo( HRASCONN, RASPROJECTION, LPVOID, LPDWORD );

DWORD APIENTRY
AfdRasIOControl( LPVOID, DWORD, PBYTE, DWORD, PBYTE, DWORD, PDWORD );

DWORD APIENTRY
AfdRasGetEntryDialParams( LPCTSTR, LPRASDIALPARAMS, DWORD, LPBOOL );

DWORD APIENTRY
AfdRasSetEntryDialParams( LPCTSTR, LPRASDIALPARAMS, DWORD, BOOL );

DWORD APIENTRY
AfdRasGetEntryProperties(LPCTSTR lpszPhonebook, LPCTSTR szEntry,
    LPBYTE lpbEntry, DWORD, LPDWORD lpdwEntrySize, LPBYTE lpb, DWORD, LPDWORD lpdwSize);

DWORD APIENTRY
AfdRasValidateEntryName(LPCTSTR lpszPhonebook, LPCTSTR lpszEntry);

DWORD APIENTRY
AfdRasSetEntryProperties(LPCTSTR lpszPhonebook, LPCTSTR szEntry,
    LPBYTE lpbEntry, DWORD dwEntrySize, LPBYTE lpb, DWORD dwSize);

DWORD APIENTRY
AfdRasRenameEntry(LPCTSTR lpszPhonebook, LPCTSTR szEntryOld, LPCTSTR szEntryNew);

DWORD APIENTRY
AfdRasDeleteEntry(LPCTSTR lpszPhonebook, LPCTSTR szEntry);

DWORD APIENTRY
AfdRasGetEntryDevConfig (LPCTSTR szPhonebook, LPCTSTR szEntry,
                         LPDWORD pdwDeviceID, LPDWORD pdwSize,
                         LPVARSTRING pDeviceConfig, DWORD cbDeviceConfig);

DWORD APIENTRY
AfdRasSetEntryDevConfig (LPCTSTR szPhonebook, LPCTSTR szEntry,
                         DWORD dwDeviceID, LPVARSTRING lpDeviceConfig, DWORD cbDeviceConfig);


#endif // _RASXPORT_H_
