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
*   @module ipcp.h | IPCP NCP Header File
*
*   Date: 7-07-95
*
*   @comm
*/


#ifndef IPCP_H
#define IPCP_H


#include "ndis.h"
#include "ipexport.h"
#include "layerfsm.h"

// IPCP Option Values

#define IPCP_OPT_IPCOMPRESSION          (2)
#define IPCP_OPT_IPADDRESS              (3)
#define IPCP_OPT_DNS_IP_ADDR            (129)			// RAS Codes
#define IPCP_OPT_WINS_IP_ADDR           (130)
#define IPCP_OPT_DNS_BACKUP_IP_ADDR     (131)
#define IPCP_OPT_WINS_BACKUP_IP_ADDR    (132)

// Option Values

typedef struct ipcp_option_values
{
    // IP Compression

	BOOL	VJCompressionEnabled;

	// From RFC1332:
    //
    // Max-Slot-Id
    //
    // The Max-Slot-Id field is one octet and indicates the maximum slot
    // identifier.  This is one less than the actual number of slots; the
    // slot identifier has values from zero to Max-Slot-Id.
    // 
    // Note: There may be implementations that have problems with only
    // one slot (Max-Slot-Id = 0).  See the discussion in reference
    // [3].  The example implementation in [3] will only work with 3
    // through 254 slots.

    BYTE    MaxSlotId;

    // Comp-Slot-Id

    // The Comp-Slot-Id field is one octet and indicates whether the slot
    // identifier field may be compressed.
    // 
    //   0  The slot identifier must not be compressed.  All compressed
    //      TCP packets must set the C bit in every change mask, and
    //      must include the slot identifier.
    // 
    //   1  The slot identifer may be compressed.
    // 
    // The slot identifier must not be compressed if there is no ability
    // for the PPP link level to indicate an error in reception to the
    // decompression module.  Synchronization after errors depends on
    // receiving a packet with the slot identifier.  See the discussion
    // in reference [3].

    BYTE    CompSlotId;

    DWORD   ipAddress;                          // IP Address
    DWORD   SubnetMask;                         // IP Subnet Mask
} 
ipcpOptValue_t;

//  IPCP Context

typedef struct  ipcp_context
{
    pppSession_t  	*session;                   // session context

	PPppFsm			pFsm;

	DWORD           VJMaxSlotIdTx;
	DWORD           VJMaxSlotIdRx;
	DWORD           VJEnableSlotIdCompressionTx;
	DWORD           VJEnableSlotIdCompressionRx;

    // IPCP Option Values

    ipcpOptValue_t  local;                      // how we want the peer to send
    ipcpOptValue_t  peer;                       // how the peer wants us to send


	// TCP/IP VJ Compression

	slcompress_t	vjcomp;						// compression control rec.

	// VJ decompression requires MAX_HDR (128) bytes of header space,
	// so we maintain a decompression buffer to handle
	// received packets.

	PBYTE           pVJRxBuf;
	DWORD           cbVJRxBuf;

	//
	//	Scratch buffer used for NAKing an IP-compression option
	//	2 bytes for protocol (00 2D)
	//  1 byte for MaxSlotId
	//  1 byte for CompSlotId
	//
	BYTE			optDataVJ[4];

	//
	//  Scratch buffer used for NAKing an IP-address type option
	//
	BYTE			optDataIPAddress[4];

	//
	//  Tracks whether a NAK packet has been received,
	//  to workaround ActiveSync bug.
	//
	BOOL			bNakReceived;
}
ipcpCntxt_t;

typedef struct ipcp_context IPCPContext, *PIPCPContext;


// Function Prototypes

// Instance Management

DWORD	pppIpcp_InstanceCreate( void *SessionContext, void **ReturnContext );
void    pppIpcp_InstanceDelete( void  *context );
DWORD   IpcpOpen( void  *context );
DWORD   IpcpClose( void  *context );
DWORD   IpcpRenegotiate( void  *context );
void    pppIpcp_Rejected( void  *context );
void	pppIpcp_LowerLayerUp(void *context);
void	pppIpcp_LowerLayerDown(void *context);

// Protocol Processing

void    pppIpcpRcvData( void *context, pppMsg_t *msg_p );
NDIS_STATUS    pppIpcpSndData( void *context, PNDIS_WAN_PACKET Packet);

void    PppIPV4ReceiveIP(IN	PIPCPContext pContext, IN pppMsg_t *pMsg);
void    PppIPV4ReceiveVJIP(IN	PIPCPContext pContext, IN pppMsg_t *pMsg);
void    PppIPV4ReceiveVJUIP(IN	PIPCPContext pContext, IN pppMsg_t *pMsg);

DWORD   ipcpOptionInit(PIPCPContext pContext);
void    ipcpResetPeerOptionValuesCb(IN		PVOID  context);
void    ipcpOptionValueReset(PIPCPContext pContext);

#endif
