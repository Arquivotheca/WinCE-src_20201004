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
#include "ipv6cp.h"
#include "tcpip.h"

#include "ipexport.h"
typedef PVOID IPv6Packet; // Need this to allow inclusion of llip6if.h, IPv6Packet is opaque to us
#include "llip6if.h"
#include "llipv6.h"
#include <ntddip6.h>
#include <cenet.h>


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
DumpIPv6 (
	BOOL  Direction,
	PBYTE Buffer,
	DWORD Len)
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

//* IPV6PppCreateToken
//
//  Initializes the interface identifer in the address.
//
void
PppIPv6CreateToken(
	PPPP_SESSION pSession,
	IPv6Addr  *Address)
{
	PBYTE		 pIFID;

	pIFID = &((PIPV6Context)( ((ncpCntxt_t *)(pSession->ncpCntxt))->protocol[NCP_IPV6CP].context))->LocalInterfaceIdentifier[0];
    memcpy(&Address->s6_bytes[8], pIFID, IPV6_IFID_LENGTH);
}
