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

//
//	PPP Server implementation module
//


//  Include Files

#include "windows.h"
#include "cclib.h"
#include "types.h"
#include "netui.h"

#include "cxport.h"
#include "crypt.h"
#include "memory.h"
#include "wincrypt.h"

// VJ Compression Include Files

#include "ndis.h"
#include "tcpip.h"
#include "vjcomp.h"


//  PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "auth.h"
#include "lcp.h"
#include "ipcp.h"
#include "ncp.h"
#include "mac.h"
#include "raserror.h"

#include "util.h"
#include "ip_intf.h"
#include "pppserver.h"

#include "iphlpapi.h"
#include "dhcp.h"

DWORD	g_dwTotalLineCount = 0;

HINSTANCE  g_hTcpIpMod = NULL;
HINSTANCE  g_hIpHlpApiMod = NULL;

extern DWORD
PPPServerSetParameters(
	IN	PRASCNTL_SERVERSTATUS pBufIn,
	IN	DWORD				  dwLenIn)
{
	return ERROR_NOT_SUPPORTED;
}

extern DWORD
PPPServerGetParameters(
	OUT	PRASCNTL_SERVERSTATUS pBufOut,
	IN	DWORD				  dwLenOut,
	OUT	PDWORD				  pdwActualOut)
{
	return ERROR_NOT_SUPPORTED;
}

DWORD
PPPServerGetIPV6NetPrefix(
	OUT	PRASCNTL_SERVER_IPV6_NET_PREFIX pBufOut,
	IN	DWORD				            dwLenOut,
	OUT	PDWORD				            pdwActualOut)
{
	return ERROR_NOT_SUPPORTED;
}

DWORD
PPPServerSetIPV6NetPrefix(
	IN	PRASCNTL_SERVER_IPV6_NET_PREFIX pBufIn,
	IN	DWORD				            dwLenIn)
{
	return ERROR_NOT_SUPPORTED;
}

extern DWORD
PPPServerGetStatus(
	OUT	PRASCNTL_SERVERSTATUS pBufOut,
	IN	DWORD				  dwLenOut,
	OUT	PDWORD				  pdwActualOut)
{
	return ERROR_NOT_SUPPORTED;
}

extern DWORD
PPPServerLineAdd(
	IN	PRASCNTL_SERVERLINE	pBufIn,
	IN	DWORD				dwLenIn)
{
	return ERROR_NOT_SUPPORTED;
}

extern DWORD 
PPPServerSetEnableState(
	BOOL bNewEnableState)
{
	return ERROR_NOT_SUPPORTED;
}

extern BOOL 
PPPServerUserCredentialsGetByName(
	IN	PWCHAR	                    wszDomainAndUserName,
	OUT PPPServerUserCredentials_t *pCredentialsOut)
//
//	Find the user credentials given an input string of the form:
//		<DomainName>\<UserName>
//	
//	The <DomainName>\ is optional
//
{
	// Shouldn't be possible to enable a line for incoming calls with the stub server,
	// so we shouldn't be trying to validate user credentials.
	ASSERT(FALSE);
	return FALSE;
}

extern DWORD
PPPServerUserSetCredentials(
	IN	PRASCNTL_SERVERUSERCREDENTIALS	pBufIn,
	IN	DWORD							dwLenIn)
{
	return ERROR_NOT_SUPPORTED;
}

extern DWORD
PPPServerUserDeleteCredentials(
	IN	PRASCNTL_SERVERUSERCREDENTIALS	pBufIn,
	IN	DWORD							dwLenIn)
{
	return ERROR_NOT_SUPPORTED;
}

extern DWORD
PPPServerLineIoCtl(
    IN	DWORD				  dwCode,
	IN	PRASCNTL_SERVERLINE   pBufIn,
	IN	DWORD				  dwLenIn,
	OUT	PRASCNTL_SERVERLINE   pBufOut,
	IN	DWORD				  dwLenOut,
	OUT	PDWORD				  pdwActualOut)
//
//	Perform an IOCTL on an existing line
//
{
	return ERROR_NOT_SUPPORTED;
}

extern DWORD
PPPServerGetSessionIPAddress(
	pppSession_t *pSession,
	DWORD		  dwType)
{
	ASSERT(FALSE);
	return 0;
}

extern DWORD
PPPServerGetSessionServerIPAddress(
	pppSession_t *pSession)
//
//	Return the IP address allocated for the server on the line
//	for this session.
//
{
	ASSERT(FALSE);
	return 0;
}

extern DWORD
PPPServerGetSessionClientIPAddress(
	pppSession_t *pSession)
//
//	Return the IP address allocated for the client on the line
//	for this session.
//
{
	ASSERT(FALSE);
	return 0;
}

DWORD
PPPServerGetSessionIPMask(
	pppSession_t *pSession)
//
//  Return the IP subnet mask for this session, should be
//  identical for client and server.
//
{
	ASSERT(FALSE);
	return 0;
}

void
PPPServerGetSessionIPV6NetPrefix(
	IN  pppSession_t *pSession,
	OUT PBYTE         pPrefix,
	OUT PDWORD        pPrefixBitLength) 
{
}

BOOL
PPPServerGetSessionAuthenticationRequired(
	pppSession_t *pSession)
//
//	Return TRUE if authentication is mandatory for the session.
//
{
	ASSERT(FALSE);
	return FALSE;
}

//
//	These variables store the most recently obtained name server IP addresses.
//
DWORD	g_dwDNSIpAddr[2];
DWORD	g_dwWINSIpAddr[2];


extern void
PPPServerAddRouteToClient(
	pppSession_t *pSession)
{
	ASSERT(FALSE);
}

void
PPPServerDeleteRouteToClient(
	pppSession_t *pSession)
{
	ASSERT(FALSE);
}

DWORD
PPPServerGetNameServerAddresses(
	PVOID unused)
//
//	This function attempts to retrieve the DNS and WINS server
//	addresses.  Addresses that it is unable to obtain will be
//	unmodified.
//
{
	return 0;
}

void
PPPServerInitialize()
//
//	Called once during system startup to initialize global variables for
//	the PPP server.
//
{
}


