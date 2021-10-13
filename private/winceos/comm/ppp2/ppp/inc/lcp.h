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
*   @module lcp.h | Link Control Protocol (LCP)
*
*   Date:   6-26-95
*
*   @comm   This header file defines types and constants used by the
*           LCP layer of PPP.
*/

#pragma once

#ifndef LCP_H
#define LCP_H

#include "layerfsm.h"

// LCP Protocol Negotiation Option Types

#define	LCP_OPT_MRU                  1
#define	LCP_OPT_ACCM                 2
#define	LCP_OPT_AUTH_PROTOCOL        3
#define	LCP_OPT_QUAL_PROTOCOL        4
#define	LCP_OPT_MAGIC_NUMBER         5
#define	LCP_OPT_PFC                  7
#define	LCP_OPT_ACFC                 8


// Option Values

typedef struct
{
    ushort          Protocol;               // authentication protocol 
    BYTE            Algorithm;              // authentication algorithm
} lcpAuthValue_t;

typedef struct lcp_option_values
{
    USHORT          MRU;                        // maximum receive unit 
    DWORD           ACCM;                       // async control char map
	lcpAuthValue_t  Auth;
    DWORD           MagicNumber;                // magic number value
	BOOL			bPFC;						// true if Protocol Field Compression enabled
	BOOL			bACFC;						// true if Address/Control Field Compression enabled
} 
lcpOptValue_t;

#define LCP_DEFAULT_ACCM	0xFFFFFFFF

// LCP Message Codes

#define	LCP_PROTOCOL_REJECT            8
#define	LCP_ECHO_REQUEST               9
#define	LCP_ECHO_REPLY                 10
#define	LCP_DISCARD_REQUEST            11

//  LCP Context

typedef struct  lcp_context
{
    pppSession_t    *session;                   // session context

	PPppFsm			pFsm;

	// Settings from MAC layer

	BOOL			bMACLayerUp;

	DWORD			FramingBits;
	DWORD			DesiredACCM;

	// Link frame size limits used for MRU negotiation

	USHORT			linkMaxSendPPPInfoFieldSize;
	USHORT			linkMaxRecvPPPInfoFieldSize;


    // LCP Option Values

    lcpOptValue_t   local;                      // local values
    lcpOptValue_t   peer;                       // peer values

	// Bitmask of Auth protocols naked by the peer
	
	DWORD           bmPeerNakedProtocols;
	int             iRequestedAuthProto;

    // Id of next protocol reject packet we will send

    BYTE            idNextProtocolReject;

	// List of pending completes
	CompleteHanderInfo_t	*pPendingCloseCompleteList;

	BYTE			abScratchOpt[4];

	// Echo Request data used to manage LCP echo request sending in Opened state

	CTETimer        IdleDisconnectTimer;
	DWORD           dwIdleDisconnectMs;       // Disconnect line if no data received from peer for this long
#define DEFAULT_IDLE_DISCONNECT_MS	15000

	DWORD           LastRxDataTime;           // Time when we last received data

	DWORD           dwEchoRequestIntervalMs;  // Time between sending echo requests when line is idle
#define DEFAULT_ECHO_REQUEST_INTERVAL_MS 1000

	DWORD           EchoRequestId;            // ID of next echo request to send
}
LCPContext, *PLCPContext;

// Function Prototypes
//
// Instance Management

DWORD	pppLcp_InstanceCreate( void *SessionContext, void **ReturnedContext );
void    pppLcp_InstanceDelete( void  *context );

// Layer Management

void    pppLcp_LowerLayerUp(IN	PVOID	context);
void    pppLcp_LowerLayerDown(IN	PVOID	context);

// Protocol Management

void    pppLcpRcvData( void *context, pppMsg_t *msg_p );

void	pppLcp_Open(PLCPContext c_p);

void	pppLcp_Close(
			PLCPContext c_p,
			void		(*pLcpCloseCompleteCallback)(PVOID),
			PVOID		pCallbackData);
void	pppLcpCloseCompleteCallback(
			PLCPContext c_p);

void    lcpResetPeerOptionValuesCb(	IN		PVOID  context);
void	lcpOptionValueReset(PLCPContext pContext);
DWORD	lcpOptionInit(PLCPContext pContext);

DWORD
pppLcp_QueryParameter(
	IN	    PVOID	context,
	IN      BOOL    bLocal,
	IN      DWORD   type,
	    OUT PVOID   pValue,
	IN  OUT PDWORD  pcbValue);

void
pppLcpSendProtocolReject(
	IN	PVOID	      context,
	IN	pppMsg_t      *pMsg );

#endif
