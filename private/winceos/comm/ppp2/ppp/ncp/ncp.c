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
#include "ip_intf.h"
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

static BOOL bWSAStartupDone = FALSE;

BOOL
IPv6Present()
//
//  Return TRUE if IPV6 appears to be available for use.
//
{
    SOCKET s = INVALID_SOCKET;

	if (!bWSAStartupDone)
	{
		WSADATA data;
		int     result;

		result = WSAStartup(0x0202, &data);
		if (result == 0)
			bWSAStartupDone = TRUE;

		DEBUGMSG(result != 0, (TEXT("PPTP: ERROR - WSAStartup failed with error=%d\n"), result));
	}

    if (bWSAStartupDone)
    {
		// Check if v6 is running
		s = socket(AF_INET6, SOCK_DGRAM, 0);
		if (s != INVALID_SOCKET)
			closesocket(s);
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
		c_p = (ncpCntxt_t *)pppAlloc( s_p, sizeof( ncpCntxt_t ));
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
		if (!dwDisableIPV6 && IPv6Present())
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

		pppFree( c_p->session, context);
	}
}

void
pppNcp_IndicateConnected(
    IN  PVOID context)
//
//  When an NCP protocol goes up it calls this function to
//  determine whether to report the link as being connected.
//
//
{
    ncpCntxt_t     *pContext  = (ncpCntxt_t *)context;
    PPPP_SESSION    pSession = pContext->session;
    RASPENTRY       *pRasEntry = &pSession->rasEntry;
    PIPV6Context    pIPV6Context = (PIPV6Context)pContext->protocol[NCP_IPV6CP].context;
    PIPCPContext    pIPCPContext = (PIPCPContext)pContext->protocol[NCP_IPCP].context;
    PCCPContext     pCCPContext  = (PCCPContext) pContext->protocol[NCP_CCP].context;
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;

    BOOL bIPV6up = pIPV6Context && pIPV6Context->pFsm->state == PFS_Opened;
    BOOL bIPCPup = pIPCPContext && pIPCPContext->pFsm->state == PFS_Opened;
    BOOL bCCPup  = pCCPContext  && pCCPContext->pFsm->state  == PFS_Opened;
    BOOL bIPV6InProgress = pIPV6Context && PFS_Stopped < pIPV6Context->pFsm->state && pIPV6Context->pFsm->state < PFS_Opened;
    BOOL bIPCPInProgress = pIPCPContext && PFS_Stopped < pIPCPContext->pFsm->state && pIPCPContext->pFsm->state < PFS_Opened;
    BOOL bDHCPFinished   = 0 != (pSession->OpenFlags & PPP_FLAG_DHCP_OPEN);
    BOOL bIPCPClosed     = pIPCPContext && pIPCPContext->pFsm->state == PFS_Closed;

    do
    {
        if (bIPCPup && (bCCPup || !NEED_ENCRYPTION(pRasEntry)))
        {
            // We can send IP packets. Do DHCP configuration first if needed, then bring up the IPV4 interface.
            if (bDHCPFinished)
            {
                Status = PPPVEMIPV4InterfaceUp(pSession);
                if (NDIS_STATUS_SUCCESS != Status)
                    break;
            }
            else
            {
                // Start or continue running DHCP
                PppDhcpStart(pSession);
            }
        }

        if (bIPV6up && (bCCPup || !NEED_ENCRYPTION(pRasEntry)))
        {
            // We can send IPV6 packets. Bring up the IPV6 interface.
            Status = PPPVEMIPV6InterfaceUp(pSession);
            if (NDIS_STATUS_SUCCESS != Status)
            {
                DEBUGMSG(ZONE_ERROR, (L"PPP: ERROR - PPPVEMIPV6InterfaceUp failed, Status=%x\n", Status));
                break;
            }
        }

        //  To be considered connected:
        //    IPCP has to be up OR IPV6CP has to be up, AND
        //    IPCP and IPV6CP negotations MUST BOTH have completed (successfully or not), AND
        //    NEED_ENCRYPTION must be FALSE or CCP must be up, AND
        //    DHCP configuration of the domain name must have completed for IP (Client only) OR
        //    IPCP is closed

        if ((bIPCPup || bIPV6up) 
        && (bCCPup || !NEED_ENCRYPTION(pRasEntry)) 
        && !bIPCPInProgress 
        && (bDHCPFinished || bIPCPClosed)
    #ifdef ACTIVESYNC_WILL_SEND_REJECT_FOR_IPV6CP_CR // Activesync currently just silently ignores IPV6CP Configure-requests
                                                 // Waiting for IPV6CP to complete negotiations (10 retries) before bringing
                                                 // up the Activesync connection is not acceptable, so we will send
                                                 // the RASCS_Connected indication even while IPV6CP is still negotiating.
        && !bIPV6InProgress 
    #endif
        )
        {
            // Start the (optional) idle disconnect timer
            pContext->dwLastTxRxIpDataTickCount = GetTickCount();
            ncpIdleDisconnectTimerStop(pContext);
            ncpIdleDisconnectTimerStart(pContext, pContext->dwIdleDisconnectMs);

            pppChangeOfState( pSession, RASCS_Connected, 0 );
        }
    } while (FALSE);

    if (NDIS_STATUS_SUCCESS != Status)
    {
        pppLcp_Close(pSession->lcpCntxt, NULL, NULL);
    }
}

NDIS_STATUS
pppNcp_SendIPvX(
	PVOID		     context,
	PNDIS_WAN_PACKET Packet,
	IN	DWORD	     ncpProtoId)
//
//  Send an IP (v4 or v6) packet through a PPP connection.
//
{
	ncpCntxt_t  *c_p = (ncpCntxt_t *)context;          // context pntr 
	NDIS_STATUS Status;

    DEBUGMSG( ZONE_NCP | ZONE_FUNCTION, (TEXT( "pppNcp_SendIPvX( %08X %08X %08X )\r\n" ), context, Packet ));

	ASSERT(ncpProtoId == NCP_IPCP || ncpProtoId == NCP_IPV6CP);

	if( c_p->protocol[ncpProtoId].enabled )
	{
		c_p->dwLastTxRxIpDataTickCount = GetTickCount();
    	Status = c_p->protocol[ncpProtoId].SendData( c_p->protocol[ncpProtoId].context, Packet);
	}
	else
	{
       	DEBUGMSG(ZONE_ERROR,(TEXT( "PPP: ERROR - TX NCP - TOSS PKT - protoid %u DISABLED\n" ), ncpProtoId));
		Status = NDIS_STATUS_FAILURE;
	}

	return Status;
}
