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
//-***********************************************************************
//
//  This file contains all of the code that sits under an IP interface
//  and does the necessary multiplexing/demultiplexing for WAN connections.
//
//-***********************************************************************

#include "windows.h"
#include "cxport.h"
#include "types.h"

// IP Header files to bind to the LLIP interface
#include "ndis.h"
#include "ntddip.h"
#include "ip.h"
#include "llipif.h"
#include "tdiinfo.h"
#include "ipinfo.h"
#include "llinfo.h"
#include "tdistat.h"
#include "cellip.h"

#include "memory.h"
#include "afdfunc.h"
#include "cclib.h"

#include "iphlpapi.h"
#include <sha2.h>

// VJ Compression Include Files

#define _TCPIP_H_NOIP
#include "tcpip.h"
#include "vjcomp.h"

//  PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "lcp.h"
#include "ipcp.h"
#include "ipv6cp.h"
#include "ncp.h"
#include "mac.h"
#include "ip_intf.h"
#include "crypt.h"
#include "pppserver.h"
#include "auth.h"
#include "util.h"
#include "debug.h"

#include <ndistapi.h>
#include <cenet.h>

extern void
SetPPPPeerIPAddress(
    DWORD dwPeerIPAddress);

/******************************************************************************

 @doc INTERNAL

 @api ULONG | IPGetNetMask | Get the netmask for an address

 @parm ULONG | Addr | an IP address

 @rdesc ULONG | The netmask

********************************************************************/
ULONG
IPGetNetMask(ULONG Addr)
{
    ULONG   AddrType;

    //
    // Get the high byte of the address, which specifies the type
    // (A, B, C, D, E) or IP address.
    //
    AddrType = Addr >> 24;

    if (AddrType <= 127)
    {
        // Class A: Network address is upper 8 bits of an IP address
        return 0xff000000;
    }
    else if (AddrType <= 191)
    {
        // Class B: Network address is upper 16 bits of an IP address
        return 0xffff0000;
    }
    else
    {
        // Class C: Network address is upper 24 bits on an IP addres
        // Don't sweat the wierd stuff (class D or E, ...)

        return 0xffffff00;
    }

}

#ifdef DEBUG

// Utility code to decode and display a TCP/IP header

#define PROTOCOL_TCP    6       // from tcpipw\h\tcp.h
#define IP_VER_FLAG     0xF0    // from tcpipw\ip\ipdef.h

BOOL
DumpIP (BOOL Direction, const BYTE *Buffer, DWORD Len)
{
    IPHeader IPHdr;
    TCPHeader TCPHdr;

    if (Len < sizeof(IPHeader)) {
        return FALSE;
    }

    memcpy ((char *)&IPHdr, Buffer, sizeof(IPHdr));

    if (IPHdr.iph_protocol == PROTOCOL_TCP) {
        memcpy ((char *)&TCPHdr, Buffer + (IPHdr.iph_verlen & 0x0F)*4, sizeof(TCPHeader));
        NKDbgPrintfW (TEXT("%hs TCP %hs%hs%hs%hs%hs%hs, %d.%d.%d.%d(%d) => %d.%d.%d.%d(%d) Seq=%d-%d Ack=%d Win=%d\r\n"),
                      Direction ? "XMIT" : "RECV",
                      TCPHdr.tcp_flags & TCP_FLAG_URG ? "U" : ".",
                      TCPHdr.tcp_flags & TCP_FLAG_ACK  ? "A" : ".",
                      TCPHdr.tcp_flags & TCP_FLAG_PUSH ? "P" : ".",
                      TCPHdr.tcp_flags & TCP_FLAG_RST  ? "R" : ".",
                      TCPHdr.tcp_flags & TCP_FLAG_SYN  ? "S" : ".",
                      TCPHdr.tcp_flags & TCP_FLAG_FIN  ? "F" : ".",
                      IPHdr.iph_src & 0xFF,
                      (IPHdr.iph_src >> 8) & 0xFF,
                      (IPHdr.iph_src >> 16) & 0xFF,
                      (IPHdr.iph_src >> 24) & 0xFF,
                      net_short(TCPHdr.tcp_src),
                      IPHdr.iph_dest & 0xFF,
                      (IPHdr.iph_dest >> 8) & 0xFF,
                      (IPHdr.iph_dest >> 16) & 0xFF,
                      (IPHdr.iph_dest >> 24) & 0xFF,
                      net_short(TCPHdr.tcp_dest),
                      net_long(TCPHdr.tcp_seq),
                      net_long(TCPHdr.tcp_seq) + net_short(IPHdr.iph_length) -
                      ((IPHdr.iph_verlen & 0x0F)*4 +
                       (net_short(TCPHdr.tcp_flags) >> 12)*4),
                      net_long(TCPHdr.tcp_ack),
                      net_short(TCPHdr.tcp_window));
        return FALSE;
    } else {
        NKDbgPrintfW (TEXT("%hs IP ver=%d hlen=%d tos=0x%X len=%d id=%d offset=%d ttl=%d prot=%d xsum=%d Src=%d.%d.%d.%d Dest=%d.%d.%d.%d\r\n"),
                      Direction ? "XMIT" : "RECV",
                      (IPHdr.iph_verlen & IP_VER_FLAG) >> 4,
                      (IPHdr.iph_verlen & 0x0F)*4,
                      IPHdr.iph_tos,
                      net_short(IPHdr.iph_length),
                      net_short(IPHdr.iph_id), net_short(IPHdr.iph_offset),
                      IPHdr.iph_ttl, IPHdr.iph_protocol, net_short(IPHdr.iph_xsum),
                      IPHdr.iph_src & 0xFF,
                      (IPHdr.iph_src >> 8) & 0xFF,
                      (IPHdr.iph_src >> 16) & 0xFF,
                      (IPHdr.iph_src >> 24) & 0xFF,
                      IPHdr.iph_dest & 0xFF,
                      (IPHdr.iph_dest >> 8) & 0xFF,
                      (IPHdr.iph_dest >> 16) & 0xFF,
                      (IPHdr.iph_dest >> 24) & 0xFF);

    }

    return FALSE;
}
#endif

NDIS_STATUS
PppSendIPvXLocked(
    PPPP_SESSION          pSession,
    PPPP_SEND_PACKET_INFO pSendPacketInfo,
    PNDIS_WAN_PACKET      pWanPacket)
//
//  Similar to PppSendIPvX, but called with the session locked and the WAN packet obtained.
//
{
    NDIS_STATUS           Status   = NDIS_STATUS_SUCCESS;
    PNDIS_BUFFER          pNdisBuf = pSendPacketInfo->pNdisBuffer;
    USHORT                Offset   = pSendPacketInfo->Offset;
    USHORT                ipProto  = pSendPacketInfo->ipProto;
    PVOID                 pMac     = pSession->macCntxt;

    ASSERT(ipProto == 4 || ipProto == 6);

    // Reserve 8 bytes for the maximum possible size PPP header:
    //   ADDRESS/CONTROL (FF 03) - 2 bytes
    //   Protocol field  (00 21) - 2 bytes
    //   CCP header      (00 FD xx xx) - 4 bytes
    //
    // Note: reserving this space isn't needed in SLIP mode, but doesn't hurt.
    //
    pWanPacket->CurrentBuffer += 8;
    pWanPacket->CurrentLength -= 8;

    // Copy the packet data to the WAN packet
    if (!CopyNdisBufferChainToFlatBuffer(pNdisBuf, Offset, pWanPacket->CurrentBuffer, &pWanPacket->CurrentLength))
    {
        DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - TX Packet too long, >%u bytes\n"), pWanPacket->CurrentLength));
        NdisWanFreePacket (pMac, pWanPacket);
        Status = NDIS_STATUS_INVALID_PACKET;
    }
    else switch( pSession->Mode )
    {
    case PPPMODE_PPP:
        // Route to IP Data Transmit
        Status = pppNcp_SendIPvX( pSession->ncpCntxt, pWanPacket, ipProto == 4 ? NCP_IPCP : NCP_IPV6CP );
        if (NDIS_STATUS_SUCCESS != Status)
            NdisWanFreePacket (pMac, pWanPacket);
        break;

    case PPPMODE_SLIP:
        Status = pppMacSndData(pMac, PPP_SLIP_PROTOCOL, pWanPacket);
        break;

    case PPPMODE_CSLIP:
        // TBD - VJ Header compression
        // TBD - Send the packet
        break;

    default:
        ASSERT( 0 );
    }
    return Status;
}

NDIS_STATUS
PppSendIPvX(
    PPPP_SESSION          pSession,
    PNDIS_PACKET          Packet,    OPTIONAL // Pointer to packet to be transmitted.
    PPPP_SEND_PACKET_INFO pSendPacketInfo)
//
//  Send an IP packet down to the WAN MAC layer (e.g. AsyncMac)
//
{
    NDIS_STATUS           Status = NDIS_STATUS_SUCCESS;
    PNDIS_WAN_PACKET      pWanPacket;

    // Validate the session and make sure it doesn't go away until transmit is done

    if (!PPPADDREF(pSession, REF_SENDIPVX))
    {
        Status = NDIS_STATUS_INVALID_PACKET;
    }
    else
    {
        pppLock( pSession );

        // Get a WAN packet.
        pWanPacket = pppMac_GetPacket(pSession->macCntxt);
        if (pWanPacket)
        {
            Status = PppSendIPvXLocked(pSession, pSendPacketInfo, pWanPacket);
        }
        else
        {
            DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - Out of TX buffers\n")));

            Status = NDIS_STATUS_FAILURE;
            if (Packet)
            {
                // Queue packet to be sent when WAN packets become available, return PENDING
                InsertTailList(&pSession->PendingSendPacketList, &pSendPacketInfo->PendingSendPacketListEntry);
                Status = NDIS_STATUS_PENDING;
            }
        }

        pppUnLock( pSession );

        PPPDELREF(pSession, REF_SENDIPVX);
    }

    return Status;
}

HINSTANCE              g_IpHlpApiMod;

typedef DWORD (*pfnGetIpForwardTable) (PMIB_IPFORWARDTABLE pIpForwardTable,PULONG pdwSize,BOOL bOrder);
pfnGetIpForwardTable g_pfnGetIpForwardTable;
typedef DWORD (*pfnSetIpForwardEntry) (PMIB_IPFORWARDROW pRoute);
pfnSetIpForwardEntry g_pfnSetIpForwardEntry;
typedef DWORD (*pfnGetAdapterIndex)   (LPWSTR AdapterName, PULONG IfIndex);
pfnGetAdapterIndex g_GetAdapterIndex;

void
pppLoadIPHelperHooks()
{
    DWORD dwResult;

    if (!g_IpHlpApiMod)
    {
        dwResult = CXUtilGetProcAddresses(TEXT("iphlpapi.dll"), &g_IpHlpApiMod,
                        TEXT("GetIpForwardTable"), &g_pfnGetIpForwardTable,
                        TEXT("SetIpForwardEntry"), &g_pfnSetIpForwardEntry,
                        TEXT("GetAdapterIndex"),   &g_GetAdapterIndex,
                        NULL);
    }
}

void
PPPChangeDefaultRoutesMetric(
    IN      int              metricDelta)
//
//  Modify the Metric1 for the default routes in the net table by the specified delta.
//
{
    int                 newMetric;
    PMIB_IPFORWARDTABLE pTable;
    DWORD               dwTableSize;
    PMIB_IPFORWARDROW   pRow;
    DWORD               numRoutes,
                        dwResult;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +ChangeDefaultRoutesMetric delta=%d\n"), metricDelta));

    pppLoadIPHelperHooks();

    if (!g_IpHlpApiMod)
    {
        RETAILMSG(1,(TEXT("!PPPChangeDefaultRoutesMetric could not load IPHlpAPI fcn pointers, RASDefaultGateway option disabled\r\n")));
        return;
    }

    dwTableSize = 0;
    g_pfnGetIpForwardTable(NULL, &dwTableSize, FALSE);

    pTable = pppAllocateMemory(dwTableSize);
    if (pTable == NULL)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR, unable to allocate %d bytes for route table \n"), dwTableSize));
    }
    else
    {
        dwResult = g_pfnGetIpForwardTable(pTable, &dwTableSize, FALSE);

        if (dwResult != ERROR_SUCCESS)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR %d from GetIpForwardTable\n"), dwResult));
        }
        else
        {
            for (pRow = &pTable->table[0], numRoutes = pTable->dwNumEntries;
                 numRoutes--;
                 pRow++)
            {
                // Check to see if the route is a default route
                if (pRow->dwForwardDest == 0)
                {
                    newMetric = pRow->dwForwardMetric1 + metricDelta;

                    // Don't try to reduce a metric below 1.

                    if (newMetric > 0)
                    {
                        pRow->dwForwardMetric1 = (ULONG)newMetric;

                        // TCPIP only allows the set on DIRECT routes
                        if (pRow->dwForwardType == 4 /*IRE_TYPE_INDIRECT*/)
                            pRow->dwForwardType =  3 /*IRE_TYPE_DIRECT*/;

                        dwResult = g_pfnSetIpForwardEntry(pRow);
                        if (dwResult != NO_ERROR)
                        {
                            DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR %d from SetIPForwardEntry\n"), dwResult));
                        }
                    }
                }
            }
        }

        pppFreeMemory(pTable, dwTableSize);
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -ChangeDefaultRoutesMetric\n")));
}

DWORD
GetPeerIPAddress(
    PPPP_SESSION pSession)
{
    DWORD dwPeerIPAddress = 0;

    if (pSession)
    {
        ncpCntxt_t      *ncp_p  = (ncpCntxt_t  *)pSession->ncpCntxt;
        ipcpCntxt_t     *ipcp_p = (ipcpCntxt_t *)ncp_p->protocol[ NCP_IPCP ].context;

        if (ipcp_p)
            dwPeerIPAddress = ipcp_p->peer.ipAddress;
    }

    return dwPeerIPAddress;
}

void
SetPeerIPAddressToYoungest()
//
//  Set the "ppp_peer" name to resolve to the IP address of the
//  most recently connected and still active PPP IPV4 connection.
//
{
    PPPP_SESSION pSession;
    DWORD dwPeerIPAddress = 0;

    //
    //  Set the ppp_peer's IP address to be that of the youngest
    //  remaining active PPP session, or NULL if none. Since new
    //  PPP contexts are pushed onto the head of the list, the
    //  newest one will be first in the list.
    //
    EnterCriticalSection (&v_ListCS);
    for (pSession = g_PPPSessionList;
            pSession != NULL;
            pSession = pSession->Next)
    {
        if ((pSession->OpenFlags & PPP_FLAG_IP_OPEN)
        &&  PPPMODE_PPP == pSession->Mode)
        {
            dwPeerIPAddress = GetPeerIPAddress(pSession);
            break;
        }
    }
    LeaveCriticalSection (&v_ListCS);

    SetPPPPeerIPAddress(dwPeerIPAddress);
}


#ifdef DEBUG

void
DumpNdisPacket(
    IN PNDIS_BUFFER pBuffer,
    IN DWORD        dwOffset)
{
    char         Packet[2048]={0};
    DWORD        Length = 0;
    DWORD        cbPacket;

    cbPacket = sizeof(Packet);
    CopyNdisBufferChainToFlatBuffer(pBuffer, dwOffset, &Packet[0], &cbPacket);

    if (ZONE_IPHEADER)
    {
        // Dump decoded TCP/IP header
        DumpIP (TRUE, &Packet[0], cbPacket);
    }
    if (ZONE_IPHEX)
    {
        // Dump Hex packet data
        NKDbgPrintfW (TEXT("PPP: TX Len=%u\n"), Length);
        DumpMem(&Packet[0], cbPacket);
    }
}
#endif

//
// Callback handlers where VEM calls PPP to process some request to
// send a packet.
//
NDIS_STATUS
PPPVEMSendPacketHandler(
    IN PVOID         pContext,
    IN PNDIS_PACKET  pNdisPacket,
    IN PNDIS_BUFFER  pBuffer,
    IN UINT          headerOffset,
    IN EthPacketType Type)
//
//  VEM calls this to send a packet.
//      pBuffer - first in the chain of buffers containing
//                the packet data to be sent.
//      headerOffset - length of data at the beginning of the buffer chain
//                     that precedes the payload we are to send.
//
//  If successful, return NDIS_STATUS_PENDING which allows us to retain
//  control of the packet and buffers.
//  When the send completes, we return the packet and buffers
//  to TCP/IP/NDIS by calling VEMSendComplete.
//
{
    PPPP_SESSION                 pSession = (PPPP_SESSION)pContext;
    NDIS_STATUS                  Status;
    PPPP_SEND_PACKET_INFO        pSendPacketInfo;


    // pBuffer points to the NDIS buffer chain containing data that
    // follows the MAC (Ethernet) header. Typically this would be
    // an IP packet.

#ifdef DEBUG
    DumpNdisPacket(pBuffer, headerOffset);
#endif

    pSendPacketInfo = (PPPP_SEND_PACKET_INFO)&pNdisPacket->MiniportReserved[0];
    pSendPacketInfo->pNdisBuffer = pBuffer;
    pSendPacketInfo->Offset = (USHORT)headerOffset;
    pSendPacketInfo->ipProto = Type == EthPacketTypeIPv4 ? 4 : 6;

    Status = PppSendIPvX(pSession, pNdisPacket, pSendPacketInfo);

    return Status;
}

PBYTE
pppGetRxBuffer(
    IN  OUT PPPP_SESSION    pSession)
//
//  Get a pointer to a buffer that can be used to store RX packet data.
//
{
    PPPP_RX_BUFFER pRxBuffer;

    //
    // First we try to get a buffer from our cached free list.
    // If none is available there then we try to allocate memory for a new buffer.
    //
    pRxBuffer = pSession->RxBufferList;
    if (pRxBuffer)
    {
        pSession->RxBufferList = pRxBuffer->Next;
        pRxBuffer->Next = NULL;
    }
    else
    {
        PREFAST_ASSERT(pSession->RxBufferSize <= 65535);
        pRxBuffer = (PPPP_RX_BUFFER)pppAlloc(pSession, sizeof(PPP_RX_BUFFER) + pSession->RxBufferSize);
        if (pRxBuffer)
            pRxBuffer->MaxDataSize = pSession->RxBufferSize;
    }

    return pRxBuffer ? &pRxBuffer->Data[0] : NULL;
}

void
pppFreeRxBuffer(
    IN  OUT PPPP_SESSION    pSession,
    IN      PBYTE           pBuffer)
//
//  Return a buffer to the free pool, or, if it is smaller than the maximum packet size,
//  free it to the heap. We only want to keep max size buffers in the free pool, so that
//  the first one in the free list will always be big enough to handle any packet.
//
{
    PPPP_RX_BUFFER pRxBuffer;

    pRxBuffer = (PPPP_RX_BUFFER)(pBuffer - offsetof(PPP_RX_BUFFER, Data[0]));
    if (pRxBuffer->MaxDataSize >= pSession->RxBufferSize)
    {
        pRxBuffer->Next = pSession->RxBufferList;
        pSession->RxBufferList = pRxBuffer;
    }
    else
    {
        pppFree(pSession, pRxBuffer);
    }
}

#ifdef DEBUG
void
DumpMsg(
    IN PPPP_MSG pMsg)
{
    if (ZONE_IPHEADER)
    {
        // Decode and display TCP/IP header
        DumpIP(FALSE, pMsg->data, pMsg->len);
    }
    if (ZONE_IPHEX)
    {
        NKDbgPrintfW (TEXT("PPP: Receive Len=%u\r\n"), pMsg->len);
        DumpMem(pMsg->data, pMsg->len);
    }
}
#endif

NDIS_STATUS
PPPVEMReturnPacketHandler(
    IN PVOID        pContext,
    IN PNDIS_PACKET Packet)
//
//  Called by VEM sometime after we called VEMReceivePacket, so that we can free resources
//  allocated for the packet.
//
{
    PPPP_SESSION         pSession = (PPPP_SESSION)pContext;
    NDIS_STATUS          Status = NDIS_STATUS_SUCCESS;
    PNDIS_BUFFER         pNdisBuffer = NULL;
    PBYTE                pDataBuffer = NULL;
    DWORD                cbDataBuffer = 0;

    //
    // Should be only 1 NDIS_BUFFER in the chain of the packet
    //
    NdisUnchainBufferAtFront(Packet, &pNdisBuffer);
    NdisQueryBuffer(pNdisBuffer, &pDataBuffer, &cbDataBuffer);
    pppFreeRxBuffer(pSession, pDataBuffer);
    VEMFreeNDISBuffer(pSession->pVEMContext, pNdisBuffer);

#ifdef DEBUG
    NdisUnchainBufferAtFront(Packet, &pNdisBuffer);
    ASSERT(NULL == pNdisBuffer);
#endif

    PPPDELREF(pSession, REF_RXBUF); // Matched ADDREF done for VEMIndicateReceive

    return Status;
}

void
PPPVEMIPvXIndicateReceivePacket(
    IN PPPP_SESSION    pSession,
    IN PPPP_MSG        pMsg,
    IN EthPacketType   Type)
//
//  This is called to indicate an IPV4 or IPV6 packet up to TCP/IP via VEM.
//
{
    PNDIS_PACKET         pNdisPacket = NULL;
    PNDIS_BUFFER         pNdisBuffer = NULL;
    NDIS_STATUS          Status = NDIS_STATUS_SUCCESS;
    PVEM_ADAPTER         pVEMContext = pSession->pVEMContext;
    PBYTE                pDataBuffer = NULL;

    do
    {
#ifdef DEBUG
        DumpMsg(pMsg);
#endif
        // Intercept any DHCP ACK packet containing domain name
        if (Type == EthPacketTypeIPv4 && pSession->bDhcpTimerRunning)
        {
            if (PppDhcpRxPacket(pSession, pMsg->data, pMsg->len))
            {
                break;
            }
        }

        if (NULL == pVEMContext)
        {
            // Have not created the VEM adapter yet, so we can't
            // indicate packets.
            break;
        }
    
        if (pMsg->len > pSession->RxBufferSize)
        {
            DEBUGMSG(ZONE_ERROR, (L"PPP: ERROR - RX packet payload len %u exceeds RxBufferSize %u\n", pMsg->len, pSession->RxBufferSize));
            Status = NDIS_STATUS_FAILURE;
            break;
        }

        //
        // Get an NDIS packet
        //
        pppUnLock( pSession );
        pNdisPacket = VEMGetNDISPacket(pVEMContext);
        pppLock( pSession );
        if (NULL == pNdisPacket)
        {
            DEBUGMSG(ZONE_ERROR, (L"PPP: ERROR - No NDIS_PACKET available for received packet\n"));
            Status = NDIS_STATUS_RESOURCES;
            break;
        }

        //
        // Copy the WAN_PACKET data from the miniport (which will be returned to the miniport
        // when this function returns) into a new buffer.
        //
        pDataBuffer = pppGetRxBuffer(pSession);
        if (NULL == pDataBuffer)
        {
            DEBUGMSG(ZONE_ERROR, (L"PPP: ERROR - No buffer available for received packet len %u\n", pMsg->len));
            Status = NDIS_STATUS_RESOURCES;
            break;
        }
        memcpy(pDataBuffer, pMsg->data, pMsg->len);

        //
        // Get an NDIS Buffer, point it to the received packet data and attach it to the NDIS packet.
        //
        pppUnLock( pSession );
        pNdisBuffer = VEMGetNDISBuffer(pVEMContext, pDataBuffer, pMsg->len);
        pppLock( pSession );
        if (NULL == pNdisBuffer)
        {
            Status = NDIS_STATUS_RESOURCES;
            break;
        }

        NdisChainBufferAtBack(pNdisPacket, pNdisBuffer);

        //
        // Pass the packet up to IP via the Virtual Ethernet Miniport interface
        // If successful, VEM will call our ReturnPacketHandler sometime later
        // to return the packet to us.
        //
        PPPADDREF(pSession, REF_RXBUF);
        pppUnLock(pSession);
        Status = VEMReceivePacket(pVEMContext, pNdisPacket, Type);
        pppLock(pSession);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            // ReturnPacket handler will not be called, delete ref here
            PPPDELREF(pSession, REF_RXBUF);
        }

    } while (FALSE);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        // Unable to receive the packet, so we will not get a callback later to
        // our ReturnPacketHandler and must release all resources now.
        pppUnLock(pSession);
        if (pNdisPacket)
            VEMFreeNDISPacket(pVEMContext, pNdisPacket);

        if (pNdisBuffer)
            VEMFreeNDISBuffer(pVEMContext, pNdisBuffer);
        pppLock(pSession);

        if (pDataBuffer)
            pppFreeRxBuffer(pSession, pDataBuffer);
    }
}

NDIS_STATUS
PPPVEMShutdownHandler(
    IN PVOID pContext)
//
//  This will be called by VEM when an adapter is being destroyed (i.e. MiniportHalt called)
//
{
    PPPP_SESSION         pSession = (PPPP_SESSION)pContext;
    NDIS_STATUS          Status = NDIS_STATUS_SUCCESS;

    return Status;
}

NDIS_STATUS
PPPVEMQueryLinkSpeed(
    IN  PVOID  pContext,
    IN  PVOID  Buffer,
    IN  ULONG  BufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded)
//
//  Called by VEM to handle a query of OID_GEN_LINK_SPEED
//
{
    PPPP_SESSION          pSession = (PPPP_SESSION)pContext;
    PULONG                pLinkSpeed = (PULONG )Buffer;
    DWORD                 dwBaudRate = 0;

    pppMac_GetCallSpeed(pSession->macCntxt, &dwBaudRate);

    // Link speed in units of 100 bps.
    *pLinkSpeed = dwBaudRate / 100;

    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
PPPVEMQueryPhysicalMedium(
    IN  PVOID  pContext,
    IN  PVOID  Buffer,
    IN  ULONG  BufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded)
//
//  Called by VEM to handle a query of OID_GEN_PHYSICAL_MEDIUM.
//
{
    PNDIS_PHYSICAL_MEDIUM pPhysMedium = (PNDIS_PHYSICAL_MEDIUM )Buffer;

    // Indicate that this is a RAS connection.
    *pPhysMedium = NdisPhysicalMediumRAS;

    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
PPPVEMQueryIPV6IFID(
    IN  PVOID  pContext,
    IN  PVOID  Buffer,
    IN  ULONG  BufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded)
//
//  Called by VEM to handle a query of OID_WAN_IPV6CP_INTERFACE_ID.
//
{
    PPPP_SESSION    pSession = (PPPP_SESSION)pContext;
    PNCPContext     pNcpContext  = (PNCPContext)pSession->ncpCntxt;
    PIPV6Context    pIPV6Context = (PIPV6Context)pNcpContext->protocol[NCP_IPV6CP].context;
    NDIS_STATUS     Status = NDIS_STATUS_FAILURE;

    if (pIPV6Context && pIPV6Context->pFsm->state == PFS_Opened)
    {
        memcpy(Buffer, &pIPV6Context->LocalInterfaceIdentifier[0], IPV6_IFID_LENGTH);
    }
    return NDIS_STATUS_SUCCESS;
}

//
//  This table tells VEM how to handle OIDs for an Active Context Query Request.
//  OIDs not in this table will be handled by VEM itself.
//
VEMOidHandlerEntry PPPVEMQueryOidHandlerTable[] =
{
    { OID_GEN_LINK_SPEED,          sizeof(ULONG),                  PPPVEMQueryLinkSpeed},
    { OID_GEN_PHYSICAL_MEDIUM,     sizeof(NDIS_PHYSICAL_MEDIUM),   PPPVEMQueryPhysicalMedium},
    { OID_WAN_IPV6CP_INTERFACE_ID, IPV6_IFID_LENGTH,               PPPVEMQueryIPV6IFID},
};
ULONG PPPVEMQueryOidHandlerTableCount = COUNTOF(PPPVEMQueryOidHandlerTable);

DWORD
PPPVEMCreate(
    PPPP_SESSION pSession)
//
//  Create NDIS Ethernet Adapter for sending/receiving IP packets
//
{
    VEMAdapterConfigParams Config;
    NDIS_STATUS            Status;
    DWORD                  IPMTU = 0;
    PLCPContext            lcp_p  = (PLCPContext)pSession->lcpCntxt;

    PPP_LOG_FN_ENTER();

    ASSERT(NULL == pSession->pVEMContext);

    switch( pSession->Mode )
    {
        case PPPMODE_PPP:
        {
            // Use the negotiated peer MTU
            IPMTU = lcp_p->peer.MRU;

            break;
        }

        case PPPMODE_SLIP:
        case PPPMODE_CSLIP:
        {
            IPMTU = SLIP_DEFAULT_MTU;

            break;
        }

        default: 
        {
            ASSERT( 0 );

            break;
        }
    }

    memset(&Config, 0, sizeof(Config));
    Config.version = VEM_CONFIG_VERSION_0;
    Config.flags  = ( VEM_CONFIG_FLAG_NO_DHCP_SERVER          |
                      VEM_CONFIG_FLAG_USE_STATIC_IP           |
                      VEM_CONFIG_FLAG_SPECIFIC_BINDINGS       |
                      VEM_CONFIG_FLAG_SPECIFY_MAC_ADDRESSES );

    //
    //  Do not create a default route (gateways) if:
    //      We are a server side connection, OR
    //      RASEO_RemoteDefaultGateway is not set.
    //
    if ( pSession->bIsServer || 
        ( !pSession->dwAlwaysAddDefaultRoute && 
          !( pSession->rasEntry.dwfOptions & RASEO_RemoteDefaultGateway ) ) )
    {
        Config.flags |= VEM_CONFIG_FLAG_NO_DEFAULT_GATEWAY;
    }

    memcpy(&Config.LocalEthMACAddress[0], " RAS", 4);
    memcpy(&Config.PeerEthMACAddress[0],  " RAS", 4);
    Config.LocalEthMACAddress[4] = 'L'; // L for "Local"
    Config.PeerEthMACAddress[4] = 'P';  // P for "Peer"
    Config.LocalEthMACAddress[5] = pSession->bIsServer ? 's' : 'c'; // s for server, c for client
    Config.PeerEthMACAddress[5]  = pSession->bIsServer ? 's' : 'c'; // s for server, c for client

    //
    // Make the default route for this interface have a metric of 1
    // so that it will be chosen over other default routes.
    //
    if (pSession->rasEntry.dwfOptions & RASEO_RemoteDefaultGateway)
        Config.flags |= VEM_CONFIG_FLAG_BEST_DEFAULT_GATEWAY;

    //Config.IfIPAddress                   = 0;
    //Config.IfSubnetMask                  = 0;
    //Config.GatewayIPAddress              = 0;
    Config.SendPacketHandler             = PPPVEMSendPacketHandler;
    Config.ReturnPacketHandler           = PPPVEMReturnPacketHandler;
    Config.ShutdownHandler               = PPPVEMShutdownHandler;
    Config.pQueryHandlerTable            = PPPVEMQueryOidHandlerTable;
    Config.numQueryHandlerTableEntries   = PPPVEMQueryOidHandlerTableCount;
    //Config.pSetHandlerTable              = NULL;
    //Config.numSetHandlerTableEntries     = 0;
    Config.MaxFrameSize                  = IPMTU;

    pppMac_GetCallSpeed(pSession->macCntxt, &Config.LinkBps);
    DEBUGMSG( ZONE_PPP,
              (L"PPP: (%hs) - Interface Bit Rate is %u b/s\n",
              __FUNCTION__,
              Config.LinkBps));

    Config.MediumType = NdisMedium802_3;

    Status = VEMDeviceCreate( pSession->AdapterName,
                              &Config,
                              pSession,
                              &pSession->pVEMContext );

    PPP_LOG_FN_LEAVE( 0 );

    return Status;
}

const WCHAR TcpipParametersKey[] =  L"Comm\\Tcpip\\Parms";

void
SetSystemDNSDomain(
    IN const char *szDomain)
//
//  Set system-wide DNSDomain name
//
{
    WCHAR           Buffer[MAX_PATH + 1];
    HKEY            hKey = NULL;
    LONG            lRes = ERROR_SUCCESS;

    if ('\0' != szDomain[0])
    {
        memset(Buffer, 0, sizeof(Buffer));
        MultiByteToWideChar(CP_OEMCP, 0, szDomain, -1, Buffer, MAX_PATH);
        lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TcpipParametersKey, 0, 0, &hKey);
        if (ERROR_SUCCESS == lRes)
        {
            RegSetValueEx(hKey, TEXT("DNSDomain"), 0, REG_SZ, (PBYTE)&Buffer[0], (wcslen(Buffer) + 1) * sizeof(WCHAR));
            RegCloseKey(hKey);
        }
    }
}

static WCHAR defaultIPV4ProtocolNames[] = L"TCPIP\0";
static WCHAR defaultIPV6ProtocolNames[] = L"TCPIP6\0";
#define PPP_IPV4_PROTOCOLS_VALUE_NAME L"IPV4Protocols"
#define PPP_IPV6_PROTOCOLS_VALUE_NAME L"IPV6Protocols"

void
pppGetIPProtocolNames(
    IN  const PPP_SESSION *pSession,
    IN  const WCHAR       *wszValueName,
    OUT PWSTR              mwszIPProtocols)
//
//  Retrieve the MULTI_SZ list of NDIS protocol driver names
//  that handle IPV4 or IPV6 packets.
//
{
    WCHAR wszLineRegKey[MAX_PATH];

    wszLineRegKey[0] = L'\0';

    // First assign default value (for when no registry overrides configured)
    memcpy(mwszIPProtocols, defaultIPV4ProtocolNames, sizeof(defaultIPV4ProtocolNames));
    if (0 == wcscmp(wszValueName, PPP_IPV6_PROTOCOLS_VALUE_NAME))
        memcpy(mwszIPProtocols, defaultIPV6ProtocolNames, sizeof(defaultIPV6ProtocolNames));

    // Second try to load global PPP registry override (applies to all lines)
    (void)ReadRegistryValues(HKEY_LOCAL_MACHINE, L"Comm\\PPP\\Parms",
                                wszValueName, REG_MULTI_SZ, 0, mwszIPProtocols, MAX_PATH * sizeof(WCHAR),
                                NULL);

    // Third try to load per-line PPP registry override
    StringCchPrintfW(&wszLineRegKey[0], _countof(wszLineRegKey), L"Comm\\PPP\\Parms\\Line\\%s", pSession->rasEntry.szDeviceName);
    (void)ReadRegistryValues(HKEY_LOCAL_MACHINE, wszLineRegKey,
                                wszValueName, REG_MULTI_SZ, 0, mwszIPProtocols, MAX_PATH * sizeof(WCHAR),
                                NULL);
}

void
pppGetIPV4Addresses(
    IN  PPPP_SESSION pSession,
    OUT PDWORD       pdwIPAddr,
    OUT PDWORD       pdwSubnetMask,
    OUT PDWORD       pdwDefaultGatewayIPAddr)
//
//  Get the negotiated IPV4 address, subnet mask, and gateway address for the session.
//
{
    ncpCntxt_t    *ncp_p  = pSession->ncpCntxt;
    ipcpCntxt_t   *ipcp_p = (ipcpCntxt_t *)ncp_p->protocol[NCP_IPCP].context;
    DWORD         dwIPAddr = 0;
    DWORD         dwSubnetMask = 0;
    DWORD         dwDefaultGatewayIPAddr = 0;

    switch( pSession->Mode )
    {
    case PPPMODE_PPP:
        dwIPAddr = ipcp_p->local.ipAddress;
        dwDefaultGatewayIPAddr = ipcp_p->peer.ipAddress;

        //
        // If the server did not tell us its IP address, generate
        // a default gateway address based upon our address.
        //
        // If the server told us that its address is the same as
        // our address, that's strange and really confuses our routing
        // tables, so replace it with a fake address instead.
        //
        if (dwDefaultGatewayIPAddr == 0 || dwDefaultGatewayIPAddr == dwIPAddr)
        {
            dwDefaultGatewayIPAddr = dwIPAddr;
            DEBUGMSG(ZONE_WARN, (TEXT("PPP: Server did not tell us its IP address, using own IP as default gateway\n")));
        }
        break;

    case PPPMODE_SLIP:
    case PPPMODE_CSLIP:
        dwIPAddr = *(PDWORD)&pSession->rasEntry.ipaddr;

        //
        // We need to have a default gateway assigned so that IP will add a default route to the route
        // table.  The IP address of the default gateway doesn't matter so long as it is on the same
        // subnet as the SLIP adapter's IP address.  Since SLIP has no mechanism to tell us what our
        // SLIP peer's IP address is, we just fudge an address on the same subnet.  The gateway's IP
        // address will get passed into our send handler by IP when the default route kicks in, but
        // PPP just ignores this address as PPP only has one place to send IP packets to.  On a LAN
        // adapter, the gateway's MAC address would get put into the LAN packet.
        //
        dwDefaultGatewayIPAddr = dwIPAddr;
        break;
    }

    if (pSession->bIsServer)
    {
        //
        // For a server, we only want to send packets on the client interface if
        // the destination IP address exactly matches the IP address of the client.
        // A direct host route to the client will be added to accomplish that, see
        // PPPServerAddRouteToClient.
        //
        // Here, we set our subnet mask for the client interface such that
        // packets that don't exactly match the client IP address are not routed to
        // the client interface.
        //
        dwSubnetMask = 0xFFFFFFFF; // PPPServerGetSessionIPMask(pSession);
    }
    else
    {
        // Just derive the subnet from the IP address (class A, B, or C)
        dwSubnetMask = IPGetNetMask( dwIPAddr );
    }


    *pdwIPAddr               = dwIPAddr;
    *pdwSubnetMask           = dwSubnetMask;
    *pdwDefaultGatewayIPAddr = dwDefaultGatewayIPAddr;
}

typedef enum ChangeBindingCommand
{
    AddBindings,
    DeleteBindings
} ChangeBindingCommand;

typedef struct ChangeBindingsInfo
{
    CTEEvent               CTEEvent;        // CXPORT event data
    PPPP_SESSION           pSession;
    const WCHAR           *RegValueName;    // Must be a string constant
    ChangeBindingCommand   Command; 
} ChangeBindingsInfo;

VOID
PppVEMChangeBindingsWorker(
    IN      struct CTEEvent *pEvent,
    IN  OUT PVOID            pvarg)
//
//  Calls VEMDeleteBindings to bind/unbind a VEM adapter from
//  a set of protocols.
//
{
    ChangeBindingsInfo *pChangeInfo = (ChangeBindingsInfo *)pvarg;
    PPPP_SESSION        pSession = pChangeInfo->pSession;
    WCHAR               mwszProtocols[MAX_PATH];
    PVEM_ADAPTER        pAdapter = NULL;

    mwszProtocols[0] = L'\0';

    if (PPPADDREF(pSession, REF_DELETE_BINDINGS))
    {
        pppGetIPProtocolNames(pSession, pChangeInfo->RegValueName, mwszProtocols);
        pAdapter = pSession->pVEMContext;
        if (pAdapter)
        {
            switch(pChangeInfo->Command)
            {
            case AddBindings:
                VEMAddBindings(pAdapter, mwszProtocols);
                break;

            case DeleteBindings:
                VEMDeleteBindings(pAdapter, mwszProtocols);
                break;

            default:
                ASSERT(FALSE);
                break;
            }
        }

        PPPDELREF(pSession, REF_DELETE_BINDINGS);
    }

    CTEFreeMem(pChangeInfo);
}

void
ScheduleVEMChangeBindings(
    PPPP_SESSION              pSession,
    const WCHAR              *RegValueName,
    ChangeBindingCommand      Command)
//
//  Schedule a worker to bind/unbind the VEM adapter from some
//  protocols. This is done on a worker to avoid potential deadlock
//  with NDIS holding its global critical section.
//
{
    ChangeBindingsInfo *pInfo;

    pInfo = CTEAllocMem(sizeof(*pInfo));
    if (NULL == pInfo)
    {
        DEBUGMSG(ZONE_ERROR, (L"PPP: OOM in ScheduleVEMDeleteBindings"));
    }
    else
    {
        CTEInitEvent(&pInfo->CTEEvent, PppVEMChangeBindingsWorker);
        pInfo->pSession = pSession;
        pInfo->RegValueName = RegValueName;
        pInfo->Command = Command;

        if (FALSE == CTEScheduleEvent(&pInfo->CTEEvent, (PVOID)pInfo))
        {
            DEBUGMSG(ZONE_ERROR, (L"PPP: CTEScheduleEvent failed"));
            CTEFreeMem(pInfo);
        }
    }
}

NDIS_STATUS
PPPVEMIPV4InterfaceUp(
    PPPP_SESSION pSession)
//
//  This function is called when IPCP enters the opened state, such
//  that the PPP session is able to transfer IPV4 packets over the link.
//
//  If encryption is required, then CCP must also be Opened for the link
//  to be able to transfer IPV4 packets.
//
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    DWORD dwSubnetMask = 0;
    DWORD dwIPAddr = 0;
    DWORD dwDefaultGatewayIPAddr = 0;

    SetSystemDNSDomain(&pSession->szDomain[0]);

    do
    {
        if (pSession->OpenFlags & PPP_FLAG_IP_OPEN)
        {
            // Already open, nothing to do
            break;
        }

        if (NULL == pSession->pVEMContext)
        {
            Status = PPPVEMCreate(pSession);
            if (NDIS_STATUS_SUCCESS != Status)
                break;
        }

        dwDefaultGatewayIPAddr = GetPeerIPAddress(pSession);
        if (dwDefaultGatewayIPAddr)
            SetPPPPeerIPAddress(dwDefaultGatewayIPAddr);

        pppGetIPV4Addresses(pSession, &dwIPAddr, &dwSubnetMask, &dwDefaultGatewayIPAddr);

        if ((pSession->rasEntry.dwfOptions & RASEO_RemoteDefaultGateway)
        &&  !(pSession->dwAlwaysAddSubnetRoute))
        {
            //
            // We are adding a default route for this PPP interface, and don't
            // want a subnet route as well.
            //
            // So, we set the subnet mask so that there is no subnet route added to the route table.
            //
            dwSubnetMask = 0xFFFFFFFF;
        }

        //
        // If we are adding a default route, make sure that the peer's IP address is
        // within the same subnet as our IP address. Otherwise, with the peer IP outside
        // the subnet, the routing table will be configured such that PPP interface may
        // not be used to send packets matching the PPP default route!
        //
        if ((dwIPAddr & dwSubnetMask) != (dwDefaultGatewayIPAddr & dwSubnetMask))
        {
            DEBUGMSG(ZONE_WARN, (L"PPP: Peer IP addr %x outside our subnet %x/%x, using own IP as gateway\n", dwDefaultGatewayIPAddr, dwIPAddr, dwSubnetMask));
            dwDefaultGatewayIPAddr = dwIPAddr;
        }

        // Update our Virtual Ethernet Miniport with the IPCP configured address information
        VEMSetIPConfig(pSession->pVEMContext, dwIPAddr, dwSubnetMask, dwDefaultGatewayIPAddr, *(PDWORD)&pSession->rasEntry.ipaddrDns, *(PDWORD)&pSession->rasEntry.ipaddrDnsAlt);
        VEMSetWINSConfig(pSession->pVEMContext, *(PDWORD)&pSession->rasEntry.ipaddrWins, *(PDWORD)&pSession->rasEntry.ipaddrWinsAlt);
        VEMSetDomain(pSession->pVEMContext, pSession->szDomain);

        // Set media state to connected prior to binding to TCPIP. Otherwise, TCPIP
        // media sense damping interval will  kick in if we change from disconnected
        // to connected after binding.
        VEMSetMediaState(pSession->pVEMContext, NdisMediaStateConnected);

        // Bind the interface to the IPV4 protocol drivers
        ScheduleVEMChangeBindings(pSession, PPP_IPV4_PROTOCOLS_VALUE_NAME, AddBindings);

        pSession->OpenFlags |= PPP_FLAG_IP_OPEN;

        if (pSession->bIsServer)
        {
            pSession->OpenFlags |= PPP_FLAG_PPP_OPEN;
            // We can add route as soon as the VPN interface up, on the old protocol stack.
            // But on the new stack, we have to postpone a little to do this. Otherwise, it will fail.
        }

    } while (FALSE); // end do

    DEBUGMSG(ZONE_ERROR && (NDIS_STATUS_SUCCESS != Status), (L"PPP: ERROR - PPPVEMIPV4InterfaceUp failed, Status=%x\n", Status));

    return Status;
}

DWORD
PPPVEMIPV4InterfaceDown(
    PPPP_SESSION pSession)
//
//  This is called when IPCP leaves the opened state, indicating that
//  we can no longer transport IPV4 packets.
//
//  This also is called if encryption is required and CCP leaves the opened state.
//
{
    pSession->OpenFlags &= ~(PPP_FLAG_PPP_OPEN | PPP_FLAG_IP_OPEN);

    if (pSession->pVEMContext)
    {
        AbortPendingPackets(pSession);
        // Unbind the interface from the IPV4 protocol drivers
        ScheduleVEMChangeBindings(pSession, PPP_IPV4_PROTOCOLS_VALUE_NAME, DeleteBindings);
    }

    SetPeerIPAddressToYoungest();

    return NDIS_STATUS_SUCCESS;
}

//* CreateGUIDFromName
//
//  Given the string name of an interface, creates a corresponding guid.
//  The guid is a hash of the string name.
//
void
CreateGUIDFromName(
    const char *Name,
    GUID       *Guid)
{
    uchar UuidTemp[SHA256_DIGEST_LEN]={0};
    //we have modified this function to use SHA2 instead of MD5 hashing.This was done as a part of Crypto BADAPI removal.
    SHA256_CTX Context = {0};

    SHA256Init(&Context);
    SHA256Update(&Context, (uchar *)Name, strlen((const char *)Name));
    SHA256Final(&Context,UuidTemp);

    memcpy(Guid, UuidTemp, 8);
    memcpy(Guid->Data4, (char *)UuidTemp + 8, 8);
}

const CHAR GuidFormat[] = "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}";
#define GUID_STRING_SIZE 38

void
GetGUIDName(
    OUT __out_ecount(39) PSTR   szAdapterGUID,
    IN  const WCHAR *wszAdapterName)
/*++

Routine Description:

    Constructs the standard string version of a GUID, in the form:
    "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}".

--*/{
    CHAR szAdapterName[128];
    GUID Guid = {0};

    szAdapterName[0] = '\0';

    WideCharToMultiByte(CP_OEMCP, 0, wszAdapterName, -1, &szAdapterName[0], sizeof(szAdapterName), NULL, NULL);
    szAdapterName[sizeof(szAdapterName) - 1] = '\0';
    StrToUpper(&szAdapterName[0]);
    CreateGUIDFromName(szAdapterName, &Guid);
    StringCchPrintfA(szAdapterGUID, GUID_STRING_SIZE + 1, GuidFormat, Guid.Data1, Guid.Data2, Guid.Data3, Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3], Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);
}

NDIS_STATUS
PPPVEMIPV6InterfaceUp(
    PPPP_SESSION pSession)
//
//  This function is called when IPV6CP enters the opened state, such
//  that the PPP session is able to transfer IPV6 packets over the link.
//
//  If encryption is required, then CCP must also be Opened for the link
//  to be able to transfer IPV6 packets.
//
{
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
    WCHAR           wszKeyName[MAX_PATH];
    CHAR            szAdapterGUID[64] = {0};
    PNCPContext     pNcpContext  = (PNCPContext)pSession->ncpCntxt;
    PIPV6Context    pIPV6Context = (PIPV6Context)pNcpContext->protocol[NCP_IPV6CP].context;

    wszKeyName[0] = L'\0';
    do
    {
        if (pSession->OpenFlags & PPP_FLAG_IPV6_OPEN)
        {
            // Already open, nothing to do
            break;
        }

        if (NULL == pSession->pVEMContext)
        {
            Status = PPPVEMCreate(pSession);
            if (NDIS_STATUS_SUCCESS != Status)
                break;
        }

        //
        //  When the server creates a new interface, it needs to add a route
        //  for that interface to specify the network prefix to be sent to
        //  the client in the routing advertisement. Otherwise, the client
        //  will only have a link local (FE80::) address.
        //

        if (pSession->bIsServer)
        {
            PPPServerGetSessionIPV6NetPrefix(pSession, &pIPV6Context->NetPrefix[0], &pIPV6Context->NetPrefixBitLength);

            GetGUIDName(szAdapterGUID, pSession->AdapterName);
            StringCchPrintfW(wszKeyName, COUNTOF(wszKeyName), L"Comm\\TcpIp6\\Parms\\Interfaces\\%hs", szAdapterGUID);

            PREFAST_ASSERT(pIPV6Context->NetPrefixBitLength <= 128);
            if (pIPV6Context->NetPrefixBitLength)
            {
                //
                // Configure registry settings for the IPV6 interface so that it will
                // publish the prefix.
                //
                CHAR        szNetPrefix[32 + 1];
                DWORD       dwOne = 1;
                DWORD       cbNetPrefix = (pIPV6Context->NetPrefixBitLength + 7) / 8;
                const BYTE *pPrefix = &pIPV6Context->NetPrefix[0];
                DWORD       iPrefix = 0, iString = 0;

                PREFAST_ASSERT(cbNetPrefix <= 16);

                (void)WriteRegistryValues(HKEY_LOCAL_MACHINE, wszKeyName,
                                          L"Advertises",  REG_DWORD, 0, &dwOne, sizeof(dwOne),
                                          L"Forwards",    REG_DWORD, 0, &dwOne, sizeof(dwOne),
                                          NULL);

                //
                // Convert the prefix into IPV6 string address notation:
                //      1234:5678:ABCD:1234::0/64
                //
                StringCchCatW(wszKeyName, COUNTOF(wszKeyName), L"\\Routes\\");
                szNetPrefix[0] = 0;
                while (TRUE)
                {
                    StringCchPrintfA(&szNetPrefix[iString], COUNTOF(szNetPrefix) - iString, "%02X", *pPrefix++);
                    iString += 2;
                    PREFAST_ASSERT(iString <= 32);
                    iPrefix++;
                    if (iPrefix >= cbNetPrefix)
                        break;
                    if (0 == (iPrefix % 2))
                    {
                        szNetPrefix[iString++] = L':';
                        PREFAST_ASSERT(iString <= 32);
                        szNetPrefix[iString] = 0;
                    }
                }

                StringCchPrintfW(&wszKeyName[wcslen(wszKeyName)], COUNTOF(wszKeyName) - wcslen(wszKeyName), L"%hs::0/%u->::0", szNetPrefix, pIPV6Context->NetPrefixBitLength);

                (void)WriteRegistryValues(HKEY_LOCAL_MACHINE, wszKeyName,
                                          L"Publish",  REG_DWORD, 0, &dwOne, sizeof(dwOne),
                                          L"Immortal", REG_DWORD, 0, &dwOne, sizeof(dwOne),
                                          NULL);
            }
            else
            {
                RegDeleteKey(HKEY_LOCAL_MACHINE, wszKeyName);
            }
            pSession->OpenFlags |= PPP_FLAG_PPP_OPEN;
        }

 
        // Bind the interface to the IPV6 protocol drivers
        pppUnLock(pSession);

        // Set media state to connected prior to binding to TCPIP. Otherwise, TCPIP
        // media sense damping interval will  kick in if we change from disconnected
        // to connected after binding.
        VEMSetMediaState(pSession->pVEMContext, NdisMediaStateConnected);

        ScheduleVEMChangeBindings(pSession, PPP_IPV6_PROTOCOLS_VALUE_NAME, AddBindings);
        pppLock(pSession);

        pSession->OpenFlags |= PPP_FLAG_IPV6_OPEN;

    } while (FALSE); // end do

    return Status;
}

DWORD
PPPVEMIPV6InterfaceDown(
    PPPP_SESSION pSession)
//
//  This is called when IPV6CP leaves the opened state, indicating that
//  we can no longer transport IPV6 packets.
//
//  This also is called if encryption is required and CCP leaves the opened state.
//
{
    pSession->OpenFlags &= ~(PPP_FLAG_IPV6_OPEN);

    if (pSession->pVEMContext)
    {
        AbortPendingPackets(pSession);
        // Unbind the interface from the IPV6 protocol drivers
        ScheduleVEMChangeBindings(pSession, PPP_IPV6_PROTOCOLS_VALUE_NAME, DeleteBindings);
    }

    return NDIS_STATUS_SUCCESS;
}

