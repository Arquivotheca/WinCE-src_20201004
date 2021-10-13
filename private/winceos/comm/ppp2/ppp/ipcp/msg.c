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

// PPP IPCP Layer Message Processing

//  Include Files

#include "windows.h"
#include "cxport.h"

// VJ Compression Include Files

#include "ndis.h"
#include "tcpip.h"
#include "vjcomp.h"

// PPP Include Files

#include "protocol.h"
#include "layerfsm.h"
#include "ppp.h"
#include "lcp.h"
#include "auth.h"
#include "ncp.h"
#include "ccp.h"
#include "ipcp.h"
#include "mac.h"
#include "ip_intf.h"

/*****************************************************************************
* 
*   @func   NDIS_STATUS | pppIpcpSndData |   IPCP send data entry point.
*
*   @parm   void *     | context | Instance context pointer.
*   @parm   pppMsg_t * | pMsg   | Pointer to a ppp message.
*               
*   @comm   This function is called to send an IPv4 packet via PPP.
*/

NDIS_STATUS
pppIpcpSndData(
    PVOID             context,
    PNDIS_WAN_PACKET  pWanPacket)
{
    PIPCPContext    pContext = (PIPCPContext)context;
    pppSession_t    *pSession = (pppSession_t *)(pContext->session);
    ncpCntxt_t     *ncp_p = (ncpCntxt_t *)pSession->ncpCntxt;
    USHORT            wProtocol;
    void            *pMac = pSession->macCntxt;
    NDIS_STATUS        Status = NDIS_STATUS_FAILURE;
    DWORD            dwFlatLen;

    DEBUGMSG(ZONE_FUNCTION, (L"PPP: pppIpcpSndData\n"));
    ASSERT( pWanPacket );

    do
    {
        // Protocol State MUST be OPEN to accept messages

        if (pContext->pFsm->state != PFS_Opened)
        {
            DEBUGMSG(ZONE_IPCP, (TEXT("PPP: IPCP Down - Discarding TX packet\n")));
            break;
        }

        dwFlatLen = pWanPacket->CurrentLength;    // Save for later
        pContext->session->Stats.BytesSent += dwFlatLen;
        pContext->session->Stats.FramesSent++;

        //
        //    Save the IP destination address in the WAN packet before compression/encryption
        //  munges it.  The IP destination address is used by PPTP to avoid endless loops
        //  when IP routes a PPTP encapsulated packet back to the PPTP adapter.
        //
        memcpy((char *)&pWanPacket->ProtocolReserved2, (char *)&((struct IPHeader *)pWanPacket->CurrentBuffer)->iph_dest, 4);

        wProtocol = PPP_PROTOCOL_IP;

        // VJ Header Compression 

        if( pContext->peer.VJCompressionEnabled )
        {
            //
            // Peer is willing to accept VJ compressed packets
            //
            wProtocol = sl_compress_tcp(pWanPacket, &pContext->vjcomp);
        }

        // CCP Datagram Compression
        //
        // If CCP is enabled pass the packet to the CCP protocol for compression. 
        // If this option has been negotiated the pppData structure will be 
        // suitably modified to pass to PPP for transmission.
        
        if( ncp_p->protocol[NCP_CCP].enabled )
        {
            // Pass in a pointer to the current Wan Packet.
            // The compression code might replace our packet if it can compress
            // the data
            if (!pppCcp_Compress( pSession, &pWanPacket, &wProtocol))
            {
                // Encryption is required and CCP is not open, return with error.
                break;
            }
        }

        // Keep track of compression stats.
        pSession->Stats.BytesTransmittedUncompressed += dwFlatLen;
        pSession->Stats.BytesTransmittedCompressed += pWanPacket->CurrentLength;
        
        DEBUGMSG(ZONE_STATS, (L"PPP: TX Stats: %u Frames, Bytes %u/%u (ratio=%u%%)\n",
            pSession->Stats.FramesSent,
            pSession->Stats.BytesTransmittedCompressed,
            pSession->Stats.BytesTransmittedUncompressed,
            (pSession->Stats.BytesTransmittedCompressed * 100) / (pSession->Stats.BytesTransmittedUncompressed ? pSession->Stats.BytesTransmittedUncompressed : 1)));

        // Send down to the miniport.
        Status = pppMacSndData (pMac, wProtocol, pWanPacket);
    } while (FALSE); // end do

    return Status;
}

/*****************************************************************************
* 
*   @func   void | pppIpcpRcvData |   IPCP receive data input entry point.
*
*   @parm   void *     | context | Instance context pointer.
*   @parm   pppMsg_t * | pMsg   | Pointer to a ppp message.
*               
*   @comm   This function is the single entry point for the IPCP layer.
*/

void
pppIpcpRcvData(
    PVOID       context,
    pppMsg_t    *pMsg )
//
//  RX Packet handler for ProtocolType = PPP_PROTOCOL_IPCP
//
//  Configuration packet used to set parameters.
//
{
    PIPCPContext    pContext = (PIPCPContext)context;

    ASSERT( PPP_PROTOCOL_IPCP == pMsg->ProtocolType );

    // If the layer below IPCP (usually auth) has not indicated that it is up yet,
    // then receiving a IPCP packet means that we probably lost an
    // authentication success packet from the peer. So, for some
    // authentication protocols the reception of a network layer
    // packet (e.g. CCP, IPCP) is implicit authentication success.
    //
    if (pContext->pFsm->state <= PFS_Starting)
    {
        AuthRxImpliedSuccessPacket(pContext->session->authCntxt);
    }

    // IPCP Control Messages
    PppFsmProcessRxPacket(pContext->pFsm, pMsg->data, pMsg->len);
}

void    
PppIPV4ReceiveIP(
    IN    PIPCPContext pContext,
    IN  pppMsg_t *pMsg)
//
//  RX Packet handler for ProtocolType = PPP_PROTOCOL_IP
//
//  A straight uncompressed IP packet.
//
{
    pppSession_t *pSession = pContext->session;

    PPP_LOG_FN_ENTER();

    if (pContext->pFsm->state == PFS_Opened)
    {
        // Announce the packet to IP

        DEBUGMSG( ZONE_IPCP,
                  ( TEXT( "PPP: (%hs) - RX IP Pkt. Len: %u\r\n"),
                  __FUNCTION__,  pMsg->len ) );

        PPPSessionUpdateRxPacketStats(pSession, pMsg);
        PPPVEMIPvXIndicateReceivePacket(pSession, pMsg, EthPacketTypeIPv4);
    }

    PPP_LOG_FN_LEAVE( 0 );

}

void    
PppIPV4ReceiveVJIP(
    IN    PIPCPContext pContext,
    IN  pppMsg_t *pMsg)
//
//  RX Packet handler for ProtocolType = PPP_PROTOCOL_COMPRESSED_TCP
//
//  The TCP/IP header of the packet has been compressed by the VJ algorithm.
//
{
    PBYTE pUncompressedIPHeader;

    if (pContext->pFsm->state == PFS_Opened)
    {
        if (pContext->local.VJCompressionEnabled)
        {
            //
            // VJ header decompression will require up to 128 bytes of space
            // ahead of pMsg->data. So copy the packet to the scratch
            // receive buffer and reserve this space.
            //
            ASSERT(MAX_HDR + pMsg->len <= pContext->cbVJRxBuf);
            memcpy(MAX_HDR + pContext->pVJRxBuf, pMsg->data, pMsg->len);
            pMsg->data = MAX_HDR + pContext->pVJRxBuf;

            // Uncompress the packet header
            pUncompressedIPHeader = sl_uncompress_tcp(pMsg->data, &pMsg->len, &pContext->vjcomp);

            if (pUncompressedIPHeader)
            {
                pMsg->data = pUncompressedIPHeader;
                 PppIPV4ReceiveIP(pContext, pMsg);
            }
            else
            {
                DEBUGMSG(ZONE_WARN, (TEXT( "PPP: WARNING - VJIP decompress failed, drop packet\r\n" )));
            }    
        }
        else
        {
            DEBUGMSG(ZONE_WARN, (L"PPP: WARNING - RX VJIP but VJComp not enabled, drop packet\n"));
        }
    }
}

void    
PppIPV4ReceiveVJUIP(
    IN    PIPCPContext pContext,
    IN  pppMsg_t *pMsg)
//
//  RX Packet handler for ProtocolType = PPP_PROTOCOL_UNCOMPRESSED_TCP
//
//  The TCP/IP header of the packet has not been compressed by the VJ algorithm,
//  but we must update our VJ compression state with the data from the TCP/IP header.
//
{
    if (pContext->pFsm->state == PFS_Opened && pContext->local.VJCompressionEnabled)
    {
        // Uncompress the packet

        if (sl_update_tcp( pMsg->data, pMsg->len, &pContext->vjcomp ))
        {
            PppIPV4ReceiveIP(pContext, pMsg);
        }
    }
}