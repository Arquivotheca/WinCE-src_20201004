//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#ifndef IP_INTF_H
#define IP_INTF_H

//
// Function Prototypes
//


void
LinkUpIndication( PPP_CONTEXT *pppContext );

void
LinkDownIndication( PPP_CONTEXT *pppContext, pppSession_t *s_p ); 

void
Receive( void *session, 	pppMsg_t	*pMsg);

void
ReceiveComplete( void *session);

NDIS_STATUS
XferData(   void            *Context, 
            NDIS_HANDLE     MACContext,  
            uint            MyOffset,
            uint            ByteOffset,
            uint            BytesWanted, 
            PNDIS_PACKET    Packet, 
            uint            *Transferred );

void
Close( void *Context );

NDIS_STATUS
PppSendIPvX(
    PVOID		 Context,       // Pointer to ppp context.
    PNDIS_PACKET Packet,        // Pointer to packet to be transmitted.
	DWORD		 ipProto);		// 4 or 6

#endif
