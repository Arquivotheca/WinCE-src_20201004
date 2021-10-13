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
*   @module ncp.h | NCP Header File
*
*   Date: 1-8-96
*
*   @comm
*/


#ifndef NCP_H
#define NCP_H

// NCP Protocol Indexes

#define	NCP_CCP				0
#define NCP_IPCP			1
#define NCP_IPV6CP			2
#define NCP_MAX_PROTOCOLS   3	// CCP, IPCP, IPV6CP


typedef	struct	protocol_interface
{
	BOOL			enabled;					// enabled flag 
	void			*context;					// protocol's context
	
	// Protocol's Close Handler

	void			(*Close)( PVOID context);

	// Protocol's Lower Layer Up/Down Event Handlers

	void			(*LowerLayerUp)( PVOID context);
	void			(*LowerLayerDown)( PVOID context);

	// Protocol's Tx Data Handler

	NDIS_STATUS		(*SendData)( PVOID context, PNDIS_WAN_PACKET Packet);
} pppProtoIntf_t;

//  NCP Context

typedef struct  ncp_context
{
    pppSession_t  	*session;                   // session context

    // Layer Mgmt

    BOOL            bLowerLayerUp;

	// NCP Protocol Mgmt - Array for now

	pppProtoIntf_t	protocol[ NCP_MAX_PROTOCOLS ];

	// Data used to determine whether to disconnect a link due to no IP packet activity
	DWORD               dwIdleDisconnectMs;
	DWORD               dwLastTxRxIpDataTickCount;
	CTETimer            IdleIpTimer;
}
ncpCntxt_t, NCPContext, *PNCPContext;

// Function Prototypes

// Instance Management

DWORD	pppNcp_InstanceCreate( void *SessionContext, void **ReturnedContext );
void    pppNcp_InstanceDelete( void  *context );

// Message Processing

void    pppNcp_IndicateConnected(IN  PVOID context);
NDIS_STATUS pppNcp_SendIPvX(PVOID context, PNDIS_WAN_PACKET Packet, IN	DWORD	 ncpProtoId);

void ncpIdleDisconnectTimerCb(CTETimer *timer, PVOID     context);
void ncpIdleDisconnectTimerStop(IN  PNCPContext	    pContext);
void ncpIdleDisconnectTimerCb(CTETimer *timer, PVOID     context);
void ncpActionThisLayerFinished( ncpCntxt_t *c_p );

void NcpLowerLayerUp( ncpCntxt_t *c_p );
void NcpLowerLayerDown( ncpCntxt_t *c_p );

#endif
