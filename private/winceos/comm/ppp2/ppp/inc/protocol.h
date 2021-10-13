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
/*****************************************************************************
* 
*
*   @doc
*   @module protocol.h | PPP Protocols
*
*   Date: 6-26-95
*
*   @comm   This header defines the PPP protocols. These constants are used
*           to route packets to the appropriate layer management entity
*           (LME).
*/

#pragma once

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "ndis.h"

// PPP Protocols

#define PPP_PROTOCOL_LCP        0xC021  // Link Control Protocol 
#define PPP_PROTOCOL_PAP        0xC023  // Password Authentication Protocol 
#define PPP_PROTOCOL_CHAP       0xC223  // Challenge Handshake Auth. Protocol
#define PPP_PROTOCOL_EAP        0xC227  // Extensible Authentication Protocol, RFC 2284
#define PPP_PROTOCOL_SPAP_OLD   0xC123  // Shiva PAP old protocol
#define PPP_PROTOCOL_SPAP_NEW   0xC027  // Shiva PAP new protocol
#define PPP_PROTOCOL_CBCP       0xC029  // Callback Control Protocol

// IP Protocols

#define PPP_PROTOCOL_IPCP               0x8021 // Internet Protocol Control Protocol
#define PPP_PROTOCOL_IPV6CP             0x8057 // Internet Protocol V6 Control Protocol
#define PPP_PROTOCOL_IP                 0x0021 // Internet Protocol (IP)
#define PPP_PROTOCOL_IPV6               0x0057 // Internet Protocol Version 6 (IPv6)
#define PPP_PROTOCOL_COMPRESSED_TCP     0x002D // Van Jacobson Compressed TCP
#define PPP_PROTOCOL_UNCOMPRESSED_TCP   0x002F // Van Jacobson Uncompressed TCP

// Compressed Packet

#define PPP_PROTOCOL_COMPRESSION        0x00FD // Compressed datagram
#define PPP_PROTOCOL_CCP                0x80FD // Compression Control Protocol
#define PPP_PROTOCOL_APPLETALK          0x0029 // Appletalk
#define PPP_PROTOCOL_ATCP               0x8029 // Appletalk Control Protocol 
#define PPP_PROTOCOL_IPX                0x002B // IPX
#define PPP_PROTOCOL_IPXCP              0x802B // Novel IPX Control Protocol 
#define PPP_PROTOCOL_NBF                0x003F // NetBIOS
#define PPP_PROTOCOL_NBFCP              0x803F // NetBIOS Framing Control Protocol 

#define PPP_SLIP_PROTOCOL		        0x0000 // Not a real PPP protocol

// Protocol Interface

typedef struct
{
	// Protocol's Rx Data Handler
	void			(*RcvData)( PVOID context, struct ppp_message *pMsg);

	// Protocol's Reject Handler
	void			(*ProtReject)(PVOID context);

	// Protocol's Open Handler
	DWORD			(*Open)(PVOID context);

	// Protocol's Close Handler
	DWORD			(*Close)(PVOID context);

	// Protocol's Renegotiate Handler
	DWORD			(*Renegotiate)(PVOID context);

	// Protocol's Parameter Query Handler
	DWORD           (*GetParameter)(PVOID context, IN OUT struct _RASCNTL_LAYER_PARAMETER *pParm, IN OUT PDWORD pcbParm);

	// Protocol's Parameter Set Handler
	DWORD           (*SetParameter)(PVOID context, IN struct _RASCNTL_LAYER_PARAMETER *pParm, IN DWORD cbParm);

} PROTOCOL_DESCRIPTOR, *PPROTOCOL_DESCRIPTOR;

typedef struct _PROTOCOL_CONTEXT
{
	DWORD                     Type;
	PPROTOCOL_DESCRIPTOR      pDescriptor;
	PVOID                     Context;

} PROTOCOL_CONTEXT, *PPROTOCOL_CONTEXT;



#endif
