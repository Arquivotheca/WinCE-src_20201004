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
*	tcpip.h
*
*	Private header file for the Ras Registry Functions
*
*	Date:	2/24/99
*
*	Modification History:
*		Date		Who			What
*	
* This header file contains a bunch of defines taken from the TCP/IP header
* files.  They are duplicated here since we needed so little of the TCP headers
* yet would have needed to include almost all of them.
*/

#ifndef _TCPIP_H
#define _TCPIP_H_


#define INVALID_ENTITY_INSTANCE     -1


//	Sequence numbers are kept as signed 32 bit quantities, with macros
//	defined to do wraparound comparisons on them.
typedef	int		SeqNum;				// A sequence number.

//* Definitions for header flags.
#define	TCP_FLAG_FIN	0x00000100
#define	TCP_FLAG_SYN	0x00000200
#define	TCP_FLAG_RST	0x00000400
#define	TCP_FLAG_PUSH	0x00000800
#define	TCP_FLAG_ACK	0x00001000
#define	TCP_FLAG_URG	0x00002000

struct TCPHeader {
	ushort				tcp_src;			// Source port.
	ushort				tcp_dest;			// Destination port.
	SeqNum				tcp_seq;			// Sequence number.
	SeqNum				tcp_ack;			// Ack number.
	ushort				tcp_flags;			// Flags and data offset.
	ushort				tcp_window;			// Window offered.
	ushort				tcp_xsum;			// Checksum.
	ushort				tcp_urgent;			// Urgent pointer.
};

typedef struct TCPHeader TCPHeader;

#ifndef _TCPIP_H_NOIP

typedef unsigned long   IPAddr;     // An IP address.
typedef unsigned long   IPMask;     // An IP subnet mask.
typedef unsigned long   IP_STATUS;  // Status code returned from IP APIs.


//*	IP Header format.
struct IPHeader {
	uchar		iph_verlen;				// Version and length.
	uchar		iph_tos;				// Type of service.
	ushort		iph_length;				// Total length of datagram.
	ushort		iph_id;					// Identification.
	ushort		iph_offset;				// Flags and fragment offset.
	uchar		iph_ttl;				// Time to live.
	uchar		iph_protocol;			// Protocol.
	ushort		iph_xsum;				// Header checksum.
	IPAddr		iph_src;				// Source address.
	IPAddr		iph_dest;				// Destination address.
}; /* IPHeader */

typedef struct IPHeader IPHeader;

#endif

#endif // ifndef _TCPIP_H
