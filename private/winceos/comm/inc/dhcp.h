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
/**								Microsoft Windows							**/
/*****************************************************************************/

/*
	dhcp.h

  DESCRIPTION:


*/

#ifndef _DHCP_H_
#define _DHCP_H_

#ifndef FAR
#define FAR
#endif

#include "ipexport.h"

//
//	Callback to obtain an NTE pointer?
//
typedef unsigned int
	(*PFNSetDHCPNTE)(
					IN		unsigned Context,	// Context value passed in to DhcpNotify, or 0x1ffff to clear
		OPTIONAL	IN	OUT	void **ppNTE,
		OPTIONAL	IN		char *pAddr,		// Client hardware address
		OPTIONAL	IN	OUT	DWORD *cAddr);		// If pAddr != NULL this is length of client HW addr

//
//	Callback to assign or remove an IP address from an interface
//	when a lease is obtained or lost.
//
typedef unsigned int (*PFNIPSetNTEAddr)(
							ushort Context,	  // Context value passed to DhcpNotify
							void *NTEp,		  // Always NULL?
							IPAddr Addr,	  // IP address leased, or 0 if lease expired/released
							IPMask Mask,      // Subnet mask for IP address
							IPAddr GWAddr);   // IP address of gateway

#define MAX_DHCP_PROTOCOL_NAME_LEN	16
//
//	Structure used to maintain information about each protocol registered with DHCP.
//
typedef struct _DHCP_PROTOCOL_CONTEXT
{
	struct _DHCP_PROTOCOL_CONTEXT  *pNext;
	WCHAR							wszProtocolName[MAX_DHCP_PROTOCOL_NAME_LEN];
	PFNSetDHCPNTE					pfnSetNTE;
	PFNIPSetNTEAddr					pfnSetAddr; 
} DHCP_PROTOCOL_CONTEXT, *PDHCP_PROTOCOL_CONTEXT;

typedef DWORD (*PFN_DHCP_NOTIFY)(uint Code,
								 PDHCP_PROTOCOL_CONTEXT pProtocolContext,
								 PTSTR pAdapter, 
								 void *Nte, 
								 unsigned Context,
								 char *pAddr, 
								 unsigned cAddr);


typedef PDHCP_PROTOCOL_CONTEXT  (*PFNDhcpRegister)(
	PWCHAR				wszProtocolName,
	PFNSetDHCPNTE		pfnSetNTE,
	PFNIPSetNTEAddr		pfnSetAddr, 
	PFN_DHCP_NOTIFY		*ppDhcpNotify);

extern PDHCP_PROTOCOL_CONTEXT DhcpRegister(
	PWCHAR				wszProtocolName,
	PFNSetDHCPNTE		pfnSetNTE,
	PFNIPSetNTEAddr		pfnSetAddr, 
	PFN_DHCP_NOTIFY		*ppDhcpNotify);


// Network Control Block structure



//	Return Codes for Dhcp
#define	DHCP_SUCCESS		0
#define DHCP_FAILURE		1
#define DHCP_NOMEM			2
#define DHCP_NACK			3
#define DHCP_NOCARD			4
#define DHCP_COLLISION		5
#define DHCP_NOINTERFACE	6	// specified interface doesn't exist
#define DHCP_MEDIA_DISC		7
#define DHCP_DELETED		8


//	OpCodes for the Dhcp Function

#define	DHCP_REGISTER			0x01	// this is a must for all Helper funcs
#define DHCP_PROBE				0x02	// reserved for all helper funcs

// note; need a notification mechanism

#define DHCP_RENEW				0x08
#define DHCP_RELEASE			0x09


//	Codes for PFN_DHCP_NOTIFY
#define DHCP_NOTIFY_ADD_INTERFACE		0x0001	// unused
#define DHCP_NOTIFY_DEL_INTERFACE		0x0002

#define DHCP_REQUEST_ADDRESS			0x1001
#define DHCP_REQUEST_RELEASE			0x1002
#define DHCP_REQUEST_RENEW				0x1003
#define DHCP_REQUEST_WLAN				0x1004	// special case for WLAN


//
// Max number of name resolution server addresses that one interface can support
// (i.e. MAX_RESOLVER DNS servers and MAX_RESOLVER WINS servers per interface
//
#define MAX_RESOLVER 4
#endif	// _DHCP_H_
