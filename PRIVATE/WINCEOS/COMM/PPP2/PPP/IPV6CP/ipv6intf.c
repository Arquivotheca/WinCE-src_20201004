//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//	This file contains the functions to interface into the lower layer of IPV6
//	as implemented in tcpip6.dll
//

#include "windows.h"
#include "cxport.h"
#include "types.h"
#include "ndis.h"
#include "util.h"

// PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "pppserver.h"
#include "lcp.h"
#include "ncp.h"
#include "ip_intf.h"
#include "ipv6intf.h"
#include "ipv6cp.h"
#include "tcpip.h"

#include "ipexport.h"
typedef PVOID IPv6Packet; // kludge to allow inclusion of llip6if.h, IPv6Packet is opaque to us
#include "llip6if.h"
#include "llipv6.h"
#include <ntddip6.h>
#include <cenet.h>


// Function pointers into the upper layer IP stack

IPV6AddInterfaceRtn	g_pfnIPv6AddInterface;
IPV6DelInterfaceRtn	g_pfnIPv6DeleteInterface;
IPV6SendCompleteRtn g_pfnIPv6SendComplete;
IPV6ReceiveRtn      g_pfnIPv6Receive;

HINSTANCE g_hIPV6Module;

#ifdef DEBUG
//
// Debug display routine for TCP/IPv6 packet headers
//

typedef struct IPv6Header {
    BYTE   VersClassFlow[4];   // 4 bits Version, 8 Traffic Class, 20 Flow Label.
    BYTE   PayloadLength[2];   // Zero indicates Jumbo Payload hop-by-hop option.
    BYTE   NextHeader;        // Values are superset of IPv4's Protocol field.
    BYTE   HopLimit;
    BYTE   Source[16];
    BYTE   Dest[16];
} IPv6Header;

#define PROTOCOL_TCP	6		// from tcpipw\h\tcp.h
#define IP_VER_FLAG		0xF0	// from tcpipw\ip\ipdef.h

void
IPV6AddrToSz(
	IN	PBYTE pAddr,
	OUT PSTR  szAddr)
{
	int    iWord;
	USHORT word;

	for (iWord = 0; iWord < 8; iWord++, pAddr += 2)
	{
		word = (pAddr[0] << 8) | pAddr[1];
		szAddr += sprintf(szAddr, "%X%s", word, iWord == 7 ? "" : ":");
	}
}

BOOL
DumpIPv6 (BOOL Direction, PBYTE Buffer, DWORD Len)
{
	IPv6Header IPHdr;
	TCPHeader TCPHdr;
	char       szSrcIP[64],
		       szDstIP[64];
	DWORD      payloadLength;

	if (Len < sizeof(IPv6Header)) {
		return FALSE;
	}

	memcpy ((char *)&IPHdr, Buffer, sizeof(IPHdr));

	IPV6AddrToSz(&IPHdr.Source[0], &szSrcIP[0]);
	IPV6AddrToSz(&IPHdr.Dest[0],   &szDstIP[0]);
	payloadLength = (IPHdr.PayloadLength[0] << 8) | IPHdr.PayloadLength[1];

	if (IPHdr.NextHeader == PROTOCOL_TCP)
	{
		memcpy ((char *)&TCPHdr, Buffer + sizeof(IPv6Header), sizeof(TCPHeader));
		NKDbgPrintfW (TEXT("%hs TCP %hs%hs%hs%hs%hs%hs, %hs(%d) => %hs(%d) Seq=%d-%d Ack=%d Win=%d\r\n"),
					  Direction ? "XMIT" : "RECV",
					  TCPHdr.tcp_flags & TCP_FLAG_URG ? "U" : ".",
					  TCPHdr.tcp_flags & TCP_FLAG_ACK  ? "A" : ".",
					  TCPHdr.tcp_flags & TCP_FLAG_PUSH ? "P" : ".",
					  TCPHdr.tcp_flags & TCP_FLAG_RST  ? "R" : ".",
					  TCPHdr.tcp_flags & TCP_FLAG_SYN  ? "S" : ".",
					  TCPHdr.tcp_flags & TCP_FLAG_FIN  ? "F" : ".",
					  &szSrcIP[0],
					  net_short(TCPHdr.tcp_src),
					  &szDstIP[0],
					  net_short(TCPHdr.tcp_dest),
					  net_long(TCPHdr.tcp_seq),
					  net_long(TCPHdr.tcp_seq) + payloadLength -
					   (net_short(TCPHdr.tcp_flags) >> 12)*4,
					  net_long(TCPHdr.tcp_ack),
					  net_short(TCPHdr.tcp_window));
		return FALSE;
	}
	else
	{
		NKDbgPrintfW (TEXT("%hs IPv6 ver=%d Len=%d Next=%d HopL=%d Src=%hs Dest=%hs\r\n"),
					  Direction ? "XMIT" : "RECV",
					  (IPHdr.VersClassFlow[0] & 0xF0) >> 4,
					  payloadLength,
					  IPHdr.NextHeader,
					  IPHdr.HopLimit,
					  &szSrcIP[0],
					  &szDstIP[0]);
	}

	return FALSE;
}
#endif // DEBUG


//  Receive
//  
//  Function: Send data up to IPV6
//

void
PppIPV6Receive(
	void		*context,
	pppMsg_t	*pMsg)
{
	PPP_CONTEXT         *pContext = (PPP_CONTEXT *)context;

    pppUnLock( pContext->Session );
	
	//
	//	IPCP may be OPEN but the interface not registered if
	//	RequireDataEncryption is set and CCP is not OPEN yet.  In
	//	this case IPContext will be NULL until CCP enters the OPEN
	//	state and registers the interface with IP.  Until the
	//	interface is registered IP packets are discarded.
	//
	if (pContext->IPV6Context && g_pfnIPv6Receive)
	{
#ifdef DEBUG
		if (dpCurSettings.ulZoneMask & 0x80000000)
			DumpIPv6(FALSE, pMsg->data, pMsg->len);
#endif
		g_pfnIPv6Receive( pContext->IPV6Context,      // IP's Context
			   pMsg->data,                  // The Packet
			   pMsg->len);                  // Data Size
	}

    pppLock( pContext->Session );
}


//* IPV6PppCreateToken
//
//  Initializes the interface identifer in the address.
//
void
PppIPv6CreateToken(
	void      *Context,
	IPv6Addr  *Address)
{
	PPP_CONTEXT *pContext = Context;
	PBYTE		 pIFID;

	pIFID = &((PIPV6Context)( ((ncpCntxt_t *)(pContext->Session->ncpCntxt)) ->protocol[NCP_IPV6CP].context)) ->LocalInterfaceIdentifier[0];
    memcpy(&Address->s6_bytes[8], pIFID, IPV6_IFID_LENGTH);
}

//* PppIPV6ConvertAddr
//
//  PppIPV6ConvertAddr does not use Neighbor Discovery or link-layer addresses.
//
ushort
PppIPV6ConvertAddr(
    void *Context,
    const IPv6Addr *Address,
    void *LinkAddress)
{
    return ND_STATE_PERMANENT;
}

//  PppIPV6Transmit
//
//  Called by TCP/IPV6 stack to transmit a packet
//
void
PppIPV6Transmit(
    PVOID        Context,       // Pointer to ppp context.
    PNDIS_PACKET Packet,        // Pointer to packet to be transmitted.
    uint         Offset,        // Offset from start of packet to IPv6 header.
    const void  *LinkAddress)   // Link-level address.
{
	PPP_CONTEXT *pContext = (PPP_CONTEXT *)Context;
	pppSession_t   *s_p = (pppSession_t *)(pContext->Session);
	NDIS_STATUS     Status;
	PVOID           IPV6Context = pContext->IPV6Context;

    DEBUGMSG( ZONE_IPV6, (TEXT("PPP: +PppIPV6Transmit\n")));
	
	ASSERT(Offset == 0);

	Status = PppSendIPvX(Context, Packet, 6);

	if (IPV6Context)
		g_pfnIPv6SendComplete(IPV6Context, Packet, Status == NDIS_STATUS_SUCCESS ? IP_SUCCESS : IP_PARAM_PROBLEM);

    DEBUGMSG( ZONE_IPV6, (TEXT("PPP: -PppIPV6Transmit Status = %u\n"), Status));
}

DWORD
PppIPV6AddRoute(
	IN PPP_CONTEXT         *pContext,
	IN PBYTE                pPrefix,
	IN DWORD                PrefixBitLength,
	IN DWORD                InterfaceIndex)
//
//  When the server creates a new interface, it needs to add a route
//  for that interface to specify the network prefix to be sent to
//  the client in the routing advertisement. Otherwise, the client
//  will only have a link local (FE80::) address.
//
{
    IPV6_INFO_ROUTE_TABLE Route;
    DWORD                 BytesReturned;
	HANDLE                Handle;
	DWORD                 dwResult = NO_ERROR;

	ASSERT(PrefixBitLength <= 128);

	memset(&Route, 0, sizeof(Route));
    Route.SitePrefixLength = 0;
    Route.ValidLifetime = INFINITE_LIFETIME;
    Route.PreferredLifetime = INFINITE_LIFETIME;
    Route.Preference = ROUTE_PREF_HIGHEST;
    Route.Type = RTE_TYPE_MANUAL;
    Route.Publish = TRUE;
    Route.Immortal = Route.Publish;

	Route.This.Neighbor.IF.Index = InterfaceIndex;
	//Route.This.Neighbor.Address = { 0 };

	memcpy(&Route.This.Prefix, pPrefix, (PrefixBitLength + 7) / 8);
	Route.This.PrefixLength = PrefixBitLength;

	Handle = CreateFileW(L"IP60:", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		DEBUGMSG(ZONE_ERROR, (L"PPP: Unable to open IP60: device\n"));
		dwResult = GetLastError();
	}
	else
	{
		if (!DeviceIoControl(Handle, IOCTL_IPV6_UPDATE_ROUTE_TABLE,
							 &Route, sizeof Route,
							 NULL, 0, &BytesReturned, NULL))
		{
			DEBUGMSG(ZONE_ERROR, (L"PPP: Unable to update IPV6 route table\n"));
			dwResult = GetLastError();
		}
		CloseHandle(Handle);
	}

	return dwResult;
}

//
//  Register an interface with the IP6 stack.
//
BOOL
PPPAddIPV6Interface(
	PPP_CONTEXT *pContext)
{
    LLIPBindInfo BindInfo;
	IP_STATUS	 Status = -1;
	DWORD        IFIndex;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+PPPAddIPV6Interface: %s\n"), &pContext->AdapterName[0]));

	ASSERT(pContext->IPV6Context == NULL);

	if (pContext->IPV6Context == NULL && g_pfnIPv6AddInterface)
	{
		memset(&BindInfo, 0, sizeof(BindInfo));

		//
		// A NULL lip_context indicates that we want to use
		// the IPv6 Interface structure instead.
		//
		BindInfo.lip_context = pContext;
		if (pContext->Session->Mode == PPPMODE_PPP)
		{
			// Set the IP MaxSegmentSize (MSS) to the maximum frame size that the PPP peer can
			// receive.  This usually is 1500, but the adapter (e.g. AsyncMac) may restrict
			// the max send size and/or it may be changed during LCP negotiation by the peer sending
			// a Maximum-Receive-Unit option.

			BindInfo.lip_maxmtu = ((PLCPContext)(pContext->Session->lcpCntxt))->peer.MRU;
			BindInfo.lip_defmtu = BindInfo.lip_maxmtu;
		}
		else // SLIP
		{
			BindInfo.lip_maxmtu = SLIP_DEFAULT_MTU;
			BindInfo.lip_defmtu = SLIP_DEFAULT_MTU;
		}
		BindInfo.lip_flags = IF_FLAG_MULTICAST | IF_FLAG_P2P | IF_FLAG_NEIGHBOR_DISCOVERS | IF_FLAG_ROUTER_DISCOVERS;

		//
		// If we are a server, send Router Advertisements to clients
		//
		if (pContext->Session->bIsServer)
			BindInfo.lip_flags |= IF_FLAG_ADVERTISES | IF_FLAG_FORWARDS;

		BindInfo.lip_type = IF_TYPE_IPV6_PPP;
		BindInfo.lip_hdrsize = 0;
		BindInfo.lip_addrlen = IPV6_IFID_LENGTH;
		BindInfo.lip_dadxmit = 0;
		BindInfo.lip_pref = 0;
		BindInfo.lip_addr = &((PIPV6Context)( ((ncpCntxt_t *)(pContext->Session->ncpCntxt)) ->protocol[NCP_IPV6CP].context)) ->LocalInterfaceIdentifier[0];
		BindInfo.lip_token = PppIPv6CreateToken;
		BindInfo.lip_rdllopt = NULL;
		BindInfo.lip_wrllopt = NULL;
		BindInfo.lip_cvaddr = PppIPV6ConvertAddr;
		BindInfo.lip_transmit = PppIPV6Transmit;
		BindInfo.lip_mclist = NULL;
		BindInfo.lip_close = NULL;
		BindInfo.lip_cleanup = NULL;

		Status = g_pfnIPv6AddInterface(&pContext->AdapterName[0], &BindInfo, &pContext->IPV6Context, &IFIndex);
		if (Status != IP_SUCCESS)
		{
			DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR - IPV6AddInterface failed, status=%u\n"), Status));
		}
		else
		{
			PIPV6Context pIPV6Context = (PIPV6Context)( ((ncpCntxt_t *)(pContext->Session->ncpCntxt))->protocol[NCP_IPV6CP].context);

			//
			// If we are a server, configure the network prefix if available.
			//
			if (pContext->Session->bIsServer)
			{
				PPPServerGetSessionIPV6NetPrefix(pContext->Session, &pIPV6Context->NetPrefix[0], &pIPV6Context->NetPrefixBitLength);
				if (pIPV6Context->NetPrefixBitLength)
				{
					DWORD dwResult;

					dwResult = PppIPV6AddRoute(pContext, &pIPV6Context->NetPrefix[0], pIPV6Context->NetPrefixBitLength, IFIndex);
				}
			}
		}
	}

	return Status == IP_SUCCESS;
}

void
PPPDeleteIPV6Interface(
	PPP_CONTEXT *pContext)
{
	if (pContext->IPV6Context && g_pfnIPv6DeleteInterface)
	{
		g_pfnIPv6DeleteInterface(pContext->IPV6Context);
		pContext->IPV6Context = NULL;
	}
}

DWORD
PPPIPV6InterfaceInitialize()
//
//	Initialize function pointers into tcpip6.dll for adding
//	and removing interfaces.
//
{
	DWORD	dwResult = NO_ERROR;

	if (g_hIPV6Module == NULL)
	{
		dwResult = CXUtilGetProcAddresses(TEXT("tcpip6.dll"), &g_hIPV6Module,
						TEXT("CEIPv6AddInterface"),    &g_pfnIPv6AddInterface,
						TEXT("CEIPv6DeleteInterface"), &g_pfnIPv6DeleteInterface,
						TEXT("CEIPv6Receive"),		   &g_pfnIPv6Receive,
						TEXT("CEIPv6SendComplete"),   &g_pfnIPv6SendComplete,
						NULL);

		if (dwResult != NO_ERROR)
		{
			DEBUGMSG(ZONE_INIT, (TEXT("PPP: LoadLibrary of tcpip6.dll failed, IPV6 over PPP will not be supported\n")));
		}
	}
	return dwResult;
}