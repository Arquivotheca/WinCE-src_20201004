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
//
//	See RFC2472 "IP Version 6 over PPP"
//


#ifndef IPV6CP_H
#define IPV6CP_H

#include "layerfsm.h"


// IPCP Option Values

#define IPV6CP_OPT_INTERFACE_IDENTIFIER    1
#define IPV6CP_OPT_COMPRESSION_PROTOCOL    2

//  IPV6 Interface Identifier Length is 8 octets,
//	the lower 8 bytes of the 16 byte IPV6 address

#define IPV6_IFID_LENGTH	8

//  Different methods for generating IFIDs
#define METHOD_IFID_REGISTRY          0
#define METHOD_IFID_PERSISTENT_RANDOM 1
#define METHOD_IFID_EUI               2
#define METHOD_IFID_DEVICE_ID         3
#define METHOD_IFID_RANDOM            4

//  IPV6CP Context

typedef struct  IPV6Context
{
    pppSession_t  	*pSession;                   // PPP session context

	PPppFsm			pFsm;

	//
	// Note: There may be a requirement imposed by tcpip6 that the
	// peer IFID immediately follow the Local IFID.
	//
	BYTE			LocalInterfaceIdentifier[IPV6_IFID_LENGTH];
	BYTE			PeerInterfaceIdentifier[IPV6_IFID_LENGTH];
	BYTE			IFIDSuggestedToPeer[IPV6_IFID_LENGTH];
	BYTE			LocalCompressionProtocol[2];
	BYTE			PeerCompressionProtocol[2];

	DWORD           dwOptionFlags;
#define OPTION_DISABLE_REGISTRY_IFID		(1<<METHOD_IFID_REGISTRY)
#define OPTION_DISABLE_PERSISTENT_RANDOM	(1<<METHOD_IFID_PERSISTENT_RANDOM)
#define OPTION_DISABLE_EUI_IFID	            (1<<METHOD_IFID_EUI)
#define OPTION_DISABLE_DEVICE_IFID	        (1<<METHOD_IFID_DEVICE_ID)

	//
	// Keeps track of the method that will be used to try to generate
	// a unique Interface Identifier.
	//
	DWORD           NextIFIDMethod;

	//
	// The IFID may be specified in the registry.
	//
	BYTE            RegistryIFID[IPV6_IFID_LENGTH];
	BOOL            bRegistryIFIDPresent;

	//
	// Random IFID generated in an earlier session and preserved in the registry
	//
	BYTE            RegistryRandomIFID[IPV6_IFID_LENGTH];
	BOOL            bRegistryRandomIFIDPresent;

	//
	// EUI of some adapter on the local machine, if available.
	// cbEUI set to 0 if no EUI available locally.
	//
	BYTE	        EUI[8];
	DWORD           cbEUI;

	//
	// Unique device ID for this computer, if available.
	//
	BYTE            DeviceID[IPV6_IFID_LENGTH];
	DWORD           cbDeviceID;

	//
	// Network prefix (to be sent by server in Routing Advertisements)
	//
	BYTE            NetPrefix[16];
	DWORD           NetPrefixBitLength;

} IPV6Context, *PIPV6Context;

DWORD	pppIpv6cp_InstanceCreate( void *SessionContext, void **ReturnContext );
void    pppIpv6cp_InstanceDelete( void  *context );
DWORD   Ipv6cpOpen( void  *context );
DWORD   Ipv6cpClose( void  *context );
DWORD   Ipv6cpRenegotiate( void  *context );
void	pppIpv6cp_LowerLayerUp(void *context);
void    pppIpv6cp_LowerLayerDown(IN	PVOID	context);
void    pppIpv6cp_Rejected( void  *context );

void    pppIpv6cpRcvData( void *context, pppMsg_t *msg_p );
NDIS_STATUS    pppIpv6cpSendData( void *context, PNDIS_WAN_PACKET Packet);

void
pppIpv6ReceiveIPV6(
	IN  PIPV6Context pContext,
	IN  pppMsg_t    *pMsg );

DWORD   ipv6cpOptionInit(PIPV6Context pContext);
void    ipv6cpOptionValueReset(PIPV6Context pContext);

#endif
