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
*   @module ip_intf.h | PPP IP Interface Header File
*
*   Date: 8-25-95
*
*   @comm
*/

#pragma once

//
// Function Prototypes
//

NDIS_STATUS
PppSendIPvXLocked(
    PPPP_SESSION          pSession,
	PPPP_SEND_PACKET_INFO pSendPacketInfo,
	PNDIS_WAN_PACKET	  pWanPacket);

NDIS_STATUS
PppSendIPvX(
    PPPP_SESSION          pSession,
    PNDIS_PACKET          Packet,    OPTIONAL // Pointer to packet to be transmitted.
	PPPP_SEND_PACKET_INFO pSendPacketInfo);

void
AbortPendingPackets(
	IN  OUT PPPP_SESSION pSession);

void
PPPVEMIPvXIndicateReceivePacket(
	IN PPPP_SESSION    pSession,
	IN PPPP_MSG	       pMsg,
	IN EthPacketType   Type);

NDIS_STATUS
PPPVEMIPV4InterfaceUp(
	PPPP_SESSION pSession);

DWORD
PPPVEMIPV4InterfaceDown(
	PPPP_SESSION pSession);

NDIS_STATUS
PPPVEMIPV6InterfaceUp(
	PPPP_SESSION pSession);

DWORD
PPPVEMIPV6InterfaceDown(
	PPPP_SESSION pSession);
