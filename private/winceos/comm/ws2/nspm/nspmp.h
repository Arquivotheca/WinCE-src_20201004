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

nspmp.h

common private header file for WSPM


FILE HISTORY:
	OmarM     13-Sep-2000
	

*/

#ifndef _NSPMP_H_
#define _NSPMP_H_


//#define WINSOCK_API_LINKAGE

//
//  System include files.
//

#include <windows.h>
#include <types.h>
#include <wtypes.h>

// DEBUG ZONES
#ifdef DEBUG
#define ZONE_ACCEPT		DEBUGZONE(0)
#define ZONE_BIND		DEBUGZONE(1)
#define ZONE_CONNECT	DEBUGZONE(2)
#define ZONE_SELECT		DEBUGZONE(3)
#define ZONE_RECV		DEBUGZONE(4)
#define ZONE_SEND		DEBUGZONE(5)
#define ZONE_SOCKET		DEBUGZONE(6)
#define ZONE_LISTEN		DEBUGZONE(7)
#define ZONE_IOCTL		DEBUGZONE(8)
#define ZONE_SSL		DEBUGZONE(9)
#define ZONE_INTERFACE	DEBUGZONE(10)
#define ZONE_MISC		DEBUGZONE(11)
#define ZONE_ALLOC		DEBUGZONE(12)
#define ZONE_FUNCTION	DEBUGZONE(13)
#define ZONE_WARN		DEBUGZONE(14)
#define ZONE_ERROR		DEBUGZONE(15)
#endif


#include <winerror.h>
#include <winsock2.h>
#include <ws2spi.h>
#include <winsock.h>
#include <svcguid.h>
#include <ctype.h>

#include <string.h>

//#include <ptype.h>
#include <memory.h>

#undef WINVER
#define WINVER	0x0400	
#include <winnetwk.h>

//
//  Project include files.
//

#include <linklist.h>
// Define LPSOCK_INFO for winsock
#define LPSOCK_INFO_DEFINED
#include <wsock.h>
typedef SOCKHAND LPSOCK_INFO;

#define NSP_SERVICE_KEY_NAME	TEXT("Comm\\Winsock2\\ServiceProvider\\ServiceTypes")

extern CRITICAL_SECTION	v_NspmCS;
extern GUID				NsId;

INT WSAAPI NSPLookupServiceBegin(
	LPGUID lpProviderId,
	LPWSAQUERYSETW lpqsRestrictions,
	LPWSASERVICECLASSINFOW lpServiceClassInfo,
	DWORD dwControlFlags,
	LPHANDLE lphLookup);

INT WSAAPI NSPLookupServiceNext(
	HANDLE hLookup,
	DWORD dwControlFlags,
	LPDWORD lpdwBufferLength,
	LPWSAQUERYSETW lpqsResults);

INT WSAAPI NSPLookupServiceEnd(
	HANDLE hLookup);

INT WSAAPI NSPSetService(
	LPGUID lpProviderId,
	LPWSASERVICECLASSINFOW lpServiceClassInfo,
	LPWSAQUERYSETW lpqsRegInfo,
	WSAESETSERVICEOP essOperation,
	DWORD dwControlFlags);

INT WSAAPI NSPInstallServiceClass(
	LPGUID lpProviderId,
	LPWSASERVICECLASSINFOW lpServiceClassInfo);

INT WSAAPI NSPRemoveServiceClass(
	LPGUID lpProviderId,
	LPGUID lpServiceClassId);

INT WSAAPI NSPGetServiceClassInfo(
	LPGUID lpProviderId,
	LPDWORD lpdwBufSize,
	LPWSASERVICECLASSINFOW lpServiceClassInfo);

INT WSAAPI NSPIoctl(
	HANDLE          hLookup,
	DWORD           dwControlCode,
	LPVOID          lpvInBuffer,
	DWORD           cbInBuffer,
	LPVOID          lpvOutBuffer,
	DWORD           cbOutBuffer,
	LPDWORD         lpcbBytesReturned,
	LPWSACOMPLETION lpCompletion,
	LPWSATHREADID   lpThreadId);

#endif  // _NSPMP_H_

