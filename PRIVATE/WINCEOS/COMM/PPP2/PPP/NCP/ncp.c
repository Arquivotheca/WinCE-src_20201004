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
*   @module ncp.c | PPP Network Control Protocol (NCP)
*
*   Date:   1-10-95
*
*   @comm
*/


//  Include Files

#include "windows.h"
#include "cclib.h"
#include "memory.h"
#include "cxport.h"
#include "crypt.h"
#include "iphlpapi.h"
#include "winsock.h"

// VJ Compression Include Files
#include "ndis.h"
#include "tcpip.h"
#include "vjcomp.h"

// PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "auth.h"
#include "lcp.h"
#include "ipcp.h"
#include "ipv6intf.h"
#include "ipv6cp.h"
#include "ccp.h"
#include "ncp.h"
#include "mac.h"

/*++
Routine Name:
    IPv6Present

Routine Description:
    Determine if IPv6 is available on the system.

Arguments:

Return Value:
    TRUE if IPv6 is configured, FALSE otherwise
--*/

HMODULE g_hWS2Module;

BOOL
IPv6Present()
{
    SOCKET s = INVALID_SOCKET;
	SOCKET (*pSocket)();
	void   (*pCloseSocket)();
	DWORD  dwResult;

	dwResult = CXUtilGetProcAddresses(
		                TEXT("ws2.dll"),     &g_hWS2Module,
						TEXT("socket"),      &pSocket,
						TEXT("closesocket"), &pCloseSocket,
						NULL);

	if (dwResult == NO_ERROR)
	{
		// WS2 module is present and has socket/closesocket exports

		// Check if v6 is running
		s = pSocket(AF_INET6, SOCK_DGRAM, 0);
		if (s != INVALID_SOCKET)
			pCloseSocket(s);

	}
	else
	{
		DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - ws2.dll not available, IPV6 will not be supported\n")));
	}

	return s != INVALID_SOCKET;
}

////////////////////////////////////////////////////////////////////////////////
//
//  Network layer idle disconnect timer support
//


void
ncpIdleDisconnectTimerStop(
	IN  PNCPContext	    pContext)
{
	CTEStopTimer(&pContext->IdleIpTimer);
}

void
ncpIdleDisconnectTimerStart(
	IN  PNCPContext	    pContext,
	IN  DWORD           DurationMs)
{
	if (DurationMs && pContext->bLowerLayerUp)
	{
		CTEStartTimer(&pContext->IdleIpTimer, DurationMs, ncpIdleDisconnectTimerCb, (PVOID)pContext);
	}
}

void
ncpIdleDisconnectTimerCb(
	CTETimer *timer,
	PVOID     context)
//
//  This function is called periodically to check to see if we are receiving network layer traffic
//  from the peer. If we haven't received traffic in too long a time interval, we disconnect the link.
//  Otherwise, we restart the timer.
//
{
	PNCPContext	    pContext = (PNCPContext)context;
	DWORD           CurrentTime = GetTickCount();
	DWORD           TicksSinceLastRxIpData;

	pppLock(pContext->session);

	TicksSinceLastRxIpData = CurrentTime - pContext->dwLastTxRxIpDataTickCount;

	if (TicksSinceLastRxIpData < pContext->dwIdleDisconnectMs)
	{
		// Not idle for too long, restart timer
		ncpIdleDisconnectTimerStart(pContext, pContext->dwIdleDisconnectMs - TicksSinceLastRxIpData);
	}
	else
	{
		// Idle for too long, disconnect
		DEBUGMSG(ZONE_PPP, (L"PPP: No RX IP data from peer for %u ms, closing connection\n", TicksSinceLastRxIpData));
		pContext->session->bDisconnectDueToUnresponsivePeer = TRUE;
		pppLcp_Close(pContext->session->lcpCntxt, NULL, NULL);
	}

	pppUnLock(pContext->session);
}

/*****************************************************************************
* 
*   @func   DWORD | pppNcp_InstanceCreate | Creates ncp instance.
*
*   @rdesc  Returns error code, 0 for success, non-zero for other errors.
*               
*   @parm   void * | SessionContext | Session context pointer.
*   @parm   void ** | ReturnedContext | Returned context pointer.
*
*   @comm   This function creates an instance of the CCP layer and returns
*           a pointer to it. This pointer is used for all subsequent calls
*           to this context.
*/

DWORD
pppNcp_InstanceCreate( void *SessionContext, void **ReturnedContext )
{
	pppSession_t    *s_p = (pppSession_t *)SessionContext;
	ncpCntxt_t      *c_p = NULL;
	DWORD			RetVal = NO_ERROR;
	DWORD           dwDisableIPV6;

    DEBUGMSG( ZONE_NCP | ZONE_FUNCTION, (TEXT( "pppNcp_InstanceCreate\r\n" )));

	do
	{
		c_p = (ncpCntxt_t *)pppAlloc( s_p, sizeof( ncpCntxt_t ), TEXT( "NCP CONTEXT" ) );
		if( NULL == c_p )
		{
			DEBUGMSG(ZONE_ERROR, ( TEXT( "PPP: ERROR - Out of memory for NCP Context\n" )));
			RetVal = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

		c_p->session = s_p;                             // save session context

		CTEInitTimer(&c_p->IdleIpTimer);

		// Initialize desired Protocols
		//
		// TCP/IP
		// Always enable TCPIP
		c_p->protocol[NCP_IPCP].enabled        = TRUE;
		c_p->protocol[NCP_IPCP].Close          = IpcpClose;
		c_p->protocol[NCP_IPCP].LowerLayerUp   = pppIpcp_LowerLayerUp;
		c_p->protocol[NCP_IPCP].LowerLayerDown = pppIpcp_LowerLayerDown;
		c_p->protocol[NCP_IPCP].SendData       = pppIpcpSndData;
		RetVal = pppIpcp_InstanceCreate(s_p, &(c_p->protocol[NCP_IPCP].context) );
		if (RetVal != NO_ERROR)
			break;

		// TCP/IPV6
		// Enable TCPIPV6 if the tcpipv6 stack is present

		dwDisableIPV6 = FALSE;
		ReadRegistryValues(HKEY_LOCAL_MACHINE, TEXT("Comm\\ppp\\Parms"),
								RAS_VALUENAME_IPV6_DISABLE, REG_DWORD, 0,  &dwDisableIPV6,   sizeof(DWORD),
								NULL);

		c_p->protocol[NCP_IPV6CP].enabled        = FALSE;
		if (!dwDisableIPV6 && IPv6Present() && (PPPIPV6InterfaceInitialize() == NO_ERROR))
		{
			c_p->protocol[NCP_IPV6CP].enabled        = TRUE;
			c_p->protocol[NCP_IPV6CP].Close          = Ipv6cpClose;
			c_p->protocol[NCP_IPV6CP].LowerLayerUp   = pppIpv6cp_LowerLayerUp;
			c_p->protocol[NCP_IPV6CP].LowerLayerDown = pppIpv6cp_LowerLayerDown;
			c_p->protocol[NCP_IPV6CP].SendData       = pppIpv6cpSendData;
			RetVal = pppIpv6cp_InstanceCreate(s_p, &(c_p->protocol[NCP_IPV6CP].context) );
			if (RetVal != NO_ERROR)
				break;
		}

		// CCP
		// Always enable CCP to allow encryption to be negotiated,
		// in case the peer requires it
		c_p->protocol[NCP_CCP].enabled        = TRUE;
		c_p->protocol[NCP_CCP].Close          = CcpClose;
		c_p->protocol[NCP_CCP].LowerLayerUp   = pppCcp_LowerLayerUp;
		c_p->protocol[NCP_CCP].LowerLayerDown = pppCcp_LowerLayerDown;

		RetVal = pppCcp_InstanceCreate( s_p, &(c_p->protocol[NCP_CCP].context));
		if (RetVal != NO_ERROR)
			break;

		// Default to Idle disconnect timeout disabled.
		c_p->dwIdleDisconnectMs = 0;

		{
			//
			// Temporary code to read in ICS RAS timeout settings and confgure our Idle timeout appropriately.
			// To be replaced with RASENTRY configuration changes in CE 5.0.
			//
			WCHAR wszRasEntryName1[MAX_PATH];
			WCHAR wszRasEntryName2[MAX_PATH];
			DWORD IdleTimeoutMS;

			IdleTimeoutMS = 1000 * 60 * 30; // default 30 minute idle timeout
			wszRasEntryName1[0] = 0;
			wszRasEntryName2[0] = 0;

			// Read ICS registry settings
			(void)ReadRegistryValues(
						HKEY_LOCAL_MACHINE, L"Comm\\Autodial",
						L"RasEntryName1",	REG_SZ,    0, (PVOID)wszRasEntryName1, sizeof(wszRasEntryName1),
						L"RasEntryName2",	REG_SZ,    0, (PVOID)wszRasEntryName2, sizeof(wszRasEntryName2),
						L"IdleTimeoutMS",	REG_DWORD, 0, (PVOID)&IdleTimeoutMS,   sizeof(IdleTimeoutMS),
						NULL);

			// If the name of our RAS entry matches that of either ICS ras entry, then apply the timeout
			if (0 == wcsicmp(wszRasEntryName1, s_p->rasDialParams.szEntryName)
			||  0 == wcsicmp(wszRasEntryName2, s_p->rasDialParams.szEntryName))
			{
				c_p->dwIdleDisconnectMs = IdleTimeoutMS;
			}
		}

	} while (FALSE);

	if (RetVal != NO_ERROR)
	{
		pppNcp_InstanceDelete(c_p);
		c_p = NULL;
	}

	*ReturnedContext = c_p;
    return RetVal;
}

/*****************************************************************************
* 
*   @func   void | pppNcp_InstanceDelete | Deletes ncp instance.
*
*   @parm   void * | context | Instance context pointer.
*
*   @comm   This function deletes the passed in context.
*/

void
pppNcp_InstanceDelete( void *context )
{
	ncpCntxt_t *c_p = (ncpCntxt_t *)context;

    DEBUGMSG( ZONE_NCP | ZONE_FUNCTION, (TEXT("pppNcp_InstanceDelete 0x%X\r\n" ), context ));

	if (context)
	{
		// Delete the NCP protocols

    	pppIpv6cp_InstanceDelete(c_p->protocol[NCP_IPV6CP].context );
    	pppIpcp_InstanceDelete(c_p->protocol[NCP_IPCP].context );
    	pppCcp_InstanceDelete (c_p->protocol[NCP_CCP].context );

		// Free the NCP layer
		c_p->session->ncpCntxt = NULL;

		pppFree( c_p->session, context, TEXT( "NCP CONTEXT" ) );

	}
}

void
pppNcp_IndicateConnected(
	IN  PVOID context)
//
//	When an NCP protocol goes up it calls this function to
//  determine whether to report the link as being connected.
//
//  To be considered connected:
//    IPCP has to be up OR IPV6CP has to be up, AND
//    NEED_ENCRYPTION must be FALSE or CCP must be up
//
{
	ncpCntxt_t     *pContext  = (ncpCntxt_t *)context;
	pppSession_t   *pSession = pContext->session;
	RASPENTRY	   *pRasEntry = &pSession->rasEntry;
	PIPV6Context	pIPV6Context = (PIPV6Context)pContext->protocol[NCP_IPV6CP].context;
	PIPCPContext	pIPCPContext = (PIPCPContext)pContext->protocol[NCP_IPCP].context;
	PCCPContext	    pCCPContext  = (PCCPContext) pContext->protocol[NCP_CCP].context;

	BOOL bIPV6up = pIPV6Context && pIPV6Context->pFsm->state == PFS_Opened;
	BOOL bIPCPup = pIPCPContext && pIPCPContext->pFsm->state == PFS_Opened;
	BOOL bCCPup  = pCCPContext  && pCCPContext->pFsm->state  == PFS_Opened;

	if ((bIPCPup || bIPV6up) && (bCCPup || !NEED_ENCRYPTION(pRasEntry)))
	{
		// Start the (optional) idle disconnect timer
		pContext->dwLastTxRxIpDataTickCount = GetTickCount();
		ncpIdleDisconnectTimerStop(pContext);
		ncpIdleDisconnectTimerStart(pContext, pContext->dwIdleDisconnectMs);

		pppChangeOfState( pSession, RASCS_Connected, 0 );
	}
}

/*****************************************************************************
* 
*   @func   void | pppNcp_Send | NCP send data entry point.
*
*               
*   @comm  This function routes the IP packet to the IPCP protocol.
*/

void
pppNcp_SendIPvX(
	PVOID		 context,
	PNDIS_PACKET Packet,
	IN	DWORD	 ncpProtoId)
{
	ncpCntxt_t  *c_p = (ncpCntxt_t *)context;          // context pntr 


    DEBUGMSG( ZONE_NCP | ZONE_FUNCTION, (TEXT( "pppNcp_SendIPvX( %08X %08X %08X )\r\n" ), context, Packet ));

	ASSERT(ncpProtoId == NCP_IPCP || ncpProtoId == NCP_IPV6CP);

	if( c_p->protocol[ncpProtoId].enabled )
	{
		c_p->dwLastTxRxIpDataTickCount = GetTickCount();
    	c_p->protocol[ncpProtoId].SendData( c_p->protocol[ncpProtoId].context, Packet);
	}
	else
	{
       	DEBUGMSG(ZONE_ERROR,(TEXT( "PPP: ERROR - TX NCP - TOSS PKT - protoid %u DISABLED\n" ), ncpProtoId));
	}
}
