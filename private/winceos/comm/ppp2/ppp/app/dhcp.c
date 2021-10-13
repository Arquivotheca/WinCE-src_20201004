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
*   @module dhcp.c | DHCP functions integrated into PPP
*
*   @comm
*      Domain names are obtained by sending a DHCP Inform
*      packet to the RAS Server, which responds with a DHCP
*      ACK packet containing the domain name.
*/


//  Include Files

#include "windows.h"
#include "types.h"
#include "cclib.h"

#include "cxport.h"
#include "crypt.h"
#include "memory.h"

//  PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "lcp.h"
#include "auth.h"
#include "ccp.h"
#include "mac.h"
#include "ras.h"

#include "ndis.h"
#include "tcpip.h"
#include "vjcomp.h"
#include "ncp.h"
#include "ipcp.h"
#include "ip_intf.h"

#include "pppserver.h"

#include "iphlpapi.h"
#include "dhcp.h"

/////////////////////// UDP/IP Packet Support ////////////////////////////////////

//* Structure of a UDP header.
typedef struct UDPHeader {
    ushort      uh_src;             // Source port.
    ushort      uh_dest;            // Destination port.
    ushort      uh_length;          // Length
    ushort      uh_xsum;            // Checksum.
} UDPHeader;

#define IP_VERSION              0x40


// ** xsum - Checksum a flat buffer.
//
//  This is the lowest level checksum routine. It returns the uncomplemented
//  checksum of a flat buffer.
//
//  Entry:  Buffer      - Buffer to be checksummed.
//          Size        - Size in bytes of Buffer.
//          InitialValue - Value of previous Xsum to add this Xsum to.
//  
//  Returns: The uncomplemented checksum of buffer.
//
USHORT
xsumComp(
	IN	__bcount(count) const void *Buffer,
	IN  int         count, 
	IN  USHORT      InitialValue)
{
    const USHORT  UNALIGNED *addr = (const USHORT UNALIGNED *)Buffer; // Buffer expressed as shorts.
    ULONG   sum = InitialValue;
    USHORT  tmp;

    for (; count >= sizeof(USHORT); count -= sizeof(USHORT))
	{
		tmp   = *addr++;
		sum += htons(tmp);
    }

	// Add left-over byte, if any
   if (count > 0 )
        sum += *(PBYTE)addr;

    // Fold 32-bit sum to 16 bits
    while (sum >> 16)
           sum = (sum & 0xffff) + (sum >> 16);

	return (USHORT)sum;
}

//
// DHCP packets are encapsulated in UDP packets, so here are support functions
// to build UDP/IP headers.
//

ULONG
BuildIpUdpHeaders(
    IN DWORD SrcIpAddr,
    IN DWORD DestIpAddr,
	IN USHORT SrcPort,
	IN USHORT DestPort,
    IN OUT CHAR* pBuffer,
    IN ULONG ulLength)
{
    IPHeader *IPH = (IPHeader *) pBuffer;
    UDPHeader *UDPH = (UDPHeader *) (pBuffer + sizeof(IPHeader));
	USHORT     iphxsum;
    
    IPH->iph_verlen = IP_VERSION + (sizeof(IPHeader) >> 2);
    IPH->iph_tos=0;
    IPH->iph_length=htons((USHORT)ulLength + sizeof(IPHeader) + sizeof(UDPHeader));
    IPH->iph_id=0;          // filled by TCPIP
    IPH->iph_offset=0;
    IPH->iph_ttl=128;
    IPH->iph_protocol=17;   // IP_PROTO_UDP
    IPH->iph_xsum = 0;
    IPH->iph_src = htonl(SrcIpAddr);
    IPH->iph_dest = htonl(DestIpAddr);
    iphxsum = ~xsumComp(IPH, sizeof(IPHeader), 0);
    IPH->iph_xsum = htons(iphxsum);

    UDPH->uh_src = htons(SrcPort);
    UDPH->uh_dest = htons(DestPort);
    UDPH->uh_length = htons((USHORT)ulLength + sizeof(UDPHeader));
    UDPH->uh_xsum = 0; // UDP checksum is optional, we won't bother.

    ulLength += sizeof(IPHeader) + sizeof(UDPHeader);
    return ulLength;
}

#define GET_NET_USHORT(p) (USHORT)(((((PBYTE)(p))[0]) << 8) | ((((PBYTE)(p))[1]) << 0))

BOOL
CheckIpUdpHeader(
	IN USHORT                          SrcPort,
	IN USHORT                          DestPort,
    IN __bcount(cbBuffer) const BYTE  *pBuffer,
    IN ULONG                           cbBuffer,
	OUT const BYTE                   **ppPayload,
	OUT PDWORD                         pcbPayload)
//
//	Check to see if pBuffer points to a UDP/IP header.
//
//  Return TRUE if it does.
//
{
	BOOL       bIsIpUdp = FALSE;
    IPHeader  *IPH = (IPHeader *) pBuffer;
    UDPHeader *UDPH = (UDPHeader *) (pBuffer + sizeof(IPHeader));
	USHORT    PktSrcPort, PktDestPort;

	if (cbBuffer >= sizeof(IPHeader) + sizeof(UDPHeader))
	{
		PktSrcPort = GET_NET_USHORT(&UDPH->uh_src);
		PktDestPort = GET_NET_USHORT(&UDPH->uh_dest);
		if ((IPH->iph_verlen == IP_VERSION + (sizeof(IPHeader) >> 2))
		&&  (IPH->iph_protocol == 17)
        &&  (PktSrcPort == SrcPort)
		&&  (PktDestPort == DestPort))
		{
			bIsIpUdp = TRUE;
			*ppPayload = pBuffer + sizeof(IPHeader) + sizeof(UDPHeader);
			*pcbPayload = cbBuffer - (sizeof(IPHeader) + sizeof(UDPHeader));
		}
	}

	return bIsIpUdp;
}

/////////////////////////////////// DHCP Packet Support ///////////////////////////////

#define OPTIONS_LEN	64
typedef struct PPPDhcpPkt {
	
	unsigned char	Op;
	unsigned char	Htype;
	unsigned char	Hlen;
	unsigned char	Hops;
	unsigned int	Xid;
	unsigned short	Secs;
	unsigned short	Flags;
	unsigned int	Ciaddr;
	unsigned int	Yiaddr;
	unsigned int	Siaddr;
	unsigned int	Giaddr;
	unsigned char	aChaddr[CHADDR_LEN];
	unsigned char	aSname[SNAME_LEN];
	unsigned char	aFile[FILE_LEN];
	//
	// Options follow, but per RFC2131:
	// The first four octets of the 'options' field of the DHCP message
    // contain the (decimal) values 99, 130, 83 and 99, respectively 
	//
	BYTE            magicCookie[4];
	unsigned char	aOptions[OPTIONS_LEN];

} PPPDhcpPkt;


#define DHCP_PARAMETER_REQUEST_LIST_OP		55

BYTE g_DhcpMagicCookie[4] = {99, 130, 83, 99};

void 
BuildDhcpInformPacket(
    IN  DWORD       dwClientIpAddr,
	OUT PBYTE		pPacket,
	OUT PDWORD      pcbPkt)
{
	PPPDhcpPkt		*pPkt = (PPPDhcpPkt *)pPacket;
    PBYTE p;

	memset(pPkt, 0, sizeof(*pPkt));
    pPkt->Op = 1;
    pPkt->Htype = 8; // Hyperchannel?
    pPkt->Hlen = 6;  // Ethernet MAC len?
    //pPkt->Hops = 0;

	pPkt->Xid  = htonl(0x12345678);
	pPkt->Secs = htons(6);

	pPkt->Ciaddr = htonl(dwClientIpAddr);

	pPkt->aChaddr[1] = 0x53;
	pPkt->aChaddr[2] = 0x45;

    memcpy(&pPkt->magicCookie[0], &g_DhcpMagicCookie[0], 4);

	//
	// Build the list of options at the end of the packet
	//
    p = &pPkt->aOptions[0];

    // Set the Message Type option
    *p++ = DHCP_MSG_TYPE_OP;
    *p++ = 1;                // option length
    *p++ = DHCPINFORM;

	// Set the Parameter Request List option to request domain name
    *p++ = DHCP_PARAMETER_REQUEST_LIST_OP;
    *p++ = 1;                // option length
    *p++ = DHCP_DOMAIN_NAME_OP;

	// terminator option
	*p++ = DHCP_END_OP;

	*pcbPkt = p - (PBYTE)pPkt;
}

BOOL
ParseDhcpAckPacketDomain(
	IN __in_bcount(cbPkt) const BYTE	*pPacket,
	IN DWORD                            cbPkt,
	OUT __out_ecount(*pcbDomain) PSTR   pDomain,
	OUT PDWORD                          pcbDomain)
//
//	Return TRUE if the packet is a DHCP ACK packet.
//
//  Sets the domain name if found in the ACK packet,
//  otherwise domain name set to null string.
//
{
	PPPDhcpPkt		*pPkt = (PPPDhcpPkt *)pPacket;
    const BYTE      *p;
	DWORD            cbOptions;
	BOOL             bGotAckPacket = FALSE;
	BYTE             optType, optLen, optValue;

	if (cbPkt >= offsetof(PPPDhcpPkt, aOptions[0]))
	{
		p = &pPkt->aOptions[0];
		cbOptions = cbPkt - offsetof(PPPDhcpPkt, aOptions[0]);

		// First option MUST be DHCP_MSG_TYPE_OP signifying ACK
		if (cbOptions < 3)
		{
			// No room for option type, length, 1 byte ACK MSG type value - invalid packet
		}
		else
		{
			optType = p[0];
			optLen = p[1];
			optValue = p[2];
			p += 3;
			cbOptions -= 3;

			if (optType == DHCP_MSG_TYPE_OP
			&&  optLen  == 1
			&&  optValue == DHCPACK)
			{
				// Search remaining options for the domain name option
				bGotAckPacket = TRUE;
				*pDomain = 0;
				*pcbDomain = 0;

				while (cbOptions > 2)
				{
					optType = p[0];
					if (optType == DHCP_END_OP)
						break;

					optLen = p[1];
					cbOptions -= 2;

					if (optLen > cbOptions)
						// Invalid option length, longer than packet data
						break;

					cbOptions -= optLen;

					if (optType == DHCP_DOMAIN_NAME_OP)
					{
						// Found domain name option
						*pcbDomain = optLen;
						memcpy(pDomain, &p[2], optLen);
						break;
					}

					p += 2 + optLen;
				}
			}
		}
	}
	return bGotAckPacket;
}

/////////////////////////////////////////////////////////////////////////////////////////

void
PppDhcpSendInform(
	pppSession_t *pSession);

void
PppDhcpAddInterface(
	pppSession_t *pSession)
//
//  This function is called after our DHCP negotiations, if any, are completed.
//
{
    pSession->OpenFlags |= PPP_FLAG_DHCP_OPEN;

	// Indicate connected to RAS (unless still waiting on encryption)
	// This will bring up the IPV4 interface as appropriate.
	pppNcp_IndicateConnected(pSession->ncpCntxt);
}

#define BOOTP_CLIENT_UDP_PORT 68
#define BOOTP_SERVER_UDP_PORT 67


void
PppDhcpSendTimeoutCb(
	CTETimer *pTimer,
	PVOID	  CbContext)
//
//	This function is called when no response is received
//  to the DHCP Inform packet within the set time.
//
{
	pppSession_t *pSession = (pppSession_t *)CbContext;

	if (PPPADDREF(pSession, REF_DHCP))
	{
		pppLock(pSession);
		if (pSession->bDhcpTimerRunning)
		{
			pSession->bDhcpTimerRunning = FALSE;
			PppDhcpSendInform(pSession);

		}
		pppUnLock(pSession);
		PPPDELREF(pSession, REF_DHCP);
	}
}

void
PppDhcpStopTimer(
	IN  OUT pppSession_t *pSession)
{
	if (pSession->bDhcpTimerRunning)
	{
		CTEStopTimer(&pSession->dhcpTimer);
		pSession->bDhcpTimerRunning = FALSE;
	}
}

void
PppDhcpStartTimer(
	IN  OUT pppSession_t *pSession)
{
	PppDhcpStopTimer(pSession);
	CTEStartTimer(&pSession->dhcpTimer, pSession->dwDhcpTimeoutMs, PppDhcpSendTimeoutCb, (PVOID)pSession);
	pSession->bDhcpTimerRunning = TRUE;
}

void
PppDhcpSendInform(
	pppSession_t *pSession)
//
//  If we haven't reached our max try count, send a DHCP inform packet to the server.
//  Otherwise, just bring up the link (with no domain name).
//
{
	DWORD                Packet[(sizeof(IPHeader) + sizeof(UDPHeader) + sizeof(PPPDhcpPkt) + 3)/ sizeof(DWORD)]; // Allocated as DWORD to align it
	PBYTE                pPacket = (PBYTE)&Packet[0];
	PBYTE                pDhcp;
	DWORD                cbPacket,
		                 cbDhcp;
	DWORD                SrcIpAddr;
	NDIS_BUFFER          NdisBuffer;
	PPP_SEND_PACKET_INFO SendInfo;

	pSession->bSentDhcpInformPacket = FALSE;

	// Check to see if we have made the configured number
	// of attempts to send DHCP Inform packets.
	if (pSession->dwDhcpInformTries < pSession->dwMaxDhcpInformTries)
	{
		pSession->dwDhcpInformTries++;

		SrcIpAddr = *(PDWORD)&pSession->rasEntry.ipaddr;
		if (PPPMODE_PPP == pSession->Mode)
		{
			ncpCntxt_t      *ncp_p  = (ncpCntxt_t *)pSession->ncpCntxt;
			ipcpCntxt_t     *ipcp_p = (ipcpCntxt_t *)(ncp_p->protocol[NCP_IPCP].context);

			SrcIpAddr = ipcp_p->local.ipAddress;
		}

		// Build DHCP Inform packet

		pDhcp = pPacket + sizeof(IPHeader) + sizeof(UDPHeader);
		BuildDhcpInformPacket(SrcIpAddr, pDhcp, &cbDhcp);

		// Build UDP/IP header

		cbPacket = BuildIpUdpHeaders(
			SrcIpAddr,
			0xFFFFFFFF,
			BOOTP_CLIENT_UDP_PORT,
			BOOTP_SERVER_UDP_PORT,
			pPacket,
			cbDhcp);

		// Start Timer

		PppDhcpStartTimer(pSession);

		// Send packet to peer

		NdisBuffer.MdlFlags = 0;
		NdisBuffer.MappedSystemVa = pPacket;
		NdisBuffer.StartVa = pPacket;
		NdisBuffer.ByteCount = cbPacket;
		NdisBuffer.ByteOffset = 0;
		NdisBuffer.Next = NULL;

		SendInfo.pNdisBuffer = &NdisBuffer;
		SendInfo.Offset = 0;
		SendInfo.ipProto = 4;

		pppUnLock(pSession);
		PppSendIPvX(pSession, NULL, &SendInfo);
		pppLock(pSession);

		pSession->bSentDhcpInformPacket = TRUE;

	}
	else
	{
		// Max number of tries reached, just bring the interface
		// up with no domain name

		DEBUGMSG(ZONE_TRACE, (TEXT("PPP: No DHCP Domain obtained\n")));

		PppDhcpAddInterface(pSession);
	}
}


BOOL
PppDhcpRxPacket(
	IN pppSession_t                       *pSession,
	IN __in_bcount(cbBuffer) const BYTE   *pBuffer,
	IN DWORD                               cbBuffer)
//
//	Check to see if the packet is a DHCP Ack packet, and if
//  so process accordingly.
//
{
	const BYTE *pDhcp;
	DWORD       cbDhcp;
	DWORD       cbDomain;
	BOOL        bRecognized = FALSE;

	if (CheckIpUdpHeader(BOOTP_SERVER_UDP_PORT, BOOTP_CLIENT_UDP_PORT, pBuffer, cbBuffer, &pDhcp, &cbDhcp))
	{
		memset(pSession->szDomain, 0, sizeof(pSession->szDomain));
		if (ParseDhcpAckPacketDomain(pDhcp, cbDhcp, &pSession->szDomain[0], &cbDomain))
		{
			// packet is a DHCP ack packet
			DEBUGMSG(ZONE_TRACE, (TEXT("PPP: DHCP Domain=%hs\n"), pSession->szDomain));

			// stop timer
			PppDhcpStopTimer(pSession);

			// bring up the interface
			PppDhcpAddInterface(pSession);

			bRecognized = TRUE;
		}
	}

	return bRecognized;
}

void
PppDhcpStart(
	IN pppSession_t *pSession)
//
// Send DHCP Inform packets to the server, trying to elicit
// a DHCP Ack response that will let us know the domain name.
//
// This process will terminate when either dwMaxDhcpInformTries DHCP Inform packets
// have been sent with no response and we give up (and bring the
// interface up without a domain name), or a DHCP Ack is received,
// in which case the interface is brought up with the supplied domain name.
//
{
    if (pSession->OpenFlags & PPP_FLAG_DHCP_OPEN)
    {
        // DHCP already completed. Do nothing
    }
	else if (pSession->bIsServer)
    {
	    //
	    //	Clients try to get domain name from server, not vice versa.
	    //  So only send an inform if we are a client,
	    //  for servers just register the interface immediately.
	    //
		PppDhcpAddInterface(pSession);
    }
	else if (pSession->bDhcpTimerRunning)
    {
        // Do not take any new action, DHCP already in progress
    }
    else
    {
        // Start DHCP going
	    pSession->dwDhcpInformTries = 0;
		PppDhcpSendInform(pSession);
    }
}

void
PppDhcpStop(
	IN pppSession_t *pSession)
{
	PppDhcpStopTimer(pSession);
}