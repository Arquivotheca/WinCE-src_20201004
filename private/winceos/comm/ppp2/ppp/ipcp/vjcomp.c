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

/*
    * Portions Copyright (c) 1989 Regents of the University of California.
    * All rights reserved.
    *
    * Redistribution and use in source and binary forms are
    * permitted provided that the above copyright notice and this
    * paragraph are duplicated in all such forms and that any
    * documentation, advertising materials, and other materials
    * related to such distribution and use acknowledge that the
    * software was developed by the University of California,
    * Berkeley.  The name of the University may not be used to
    * endorse or promote products derived from this software
    * without specific prior written permission.
    * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS
    * OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE
    * IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A
    * PARTICULAR PURPOSE.
    */

/*****************************************************************************
* 
*
*   @doc
*   @module vjcomp.c | Van Jacobsen TCP/IP Header Compression 
*
*   Date:   11-13-95
*
*   @comm   Modified originial VJ Header Compression Code for ndis buffer
*           input. Performed cleanup on general structure and comments for
*           functional clarity.
*
*/

//  Include Files

#include "windows.h"
#include "cclib.h"
#include "memory.h"
#include "cxport.h"

// VJ Compression Include Files
#include "winsock.h"
#include "ndis.h"
#include "ndiswan.h"
#include "tcpip.h"
#include "vjcomp.h"

// PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "lcp.h"
#include "ipcp.h"
#include "mac.h"
#include "ip_intf.h"
#include "ras.h"


// TCP and IP Header Length Access Macros
// 
// Note: Both macros incorporate a times 4 factor for int to byte conversion.
//       All header arithmetic is always done as bytes.

#define IpHdrLen( x )           ((((x)->iph_verlen) & 0x0f) << 2)

// Bits in first octet of compressed packet 
// flag bits for what changed in a packet.

#define NEW_C                   (0x40)
#define NEW_I                   (0x20)
#define TCP_PUSH_BIT            (0x10)
#define NEW_S                   (0x08)
#define NEW_A                   (0x04)
#define NEW_W                   (0x02)
#define NEW_U                   (0x01)


// Reserved, special-case values of above 
//
// Echoed interactive traffic 

#define SPECIAL_I               ( NEW_S | NEW_W | NEW_U )     

// Unidirectional data 

#define SPECIAL_D               ( NEW_S | NEW_A | NEW_W | NEW_U )  
#define SPECIALS_MASK           ( NEW_S | NEW_A | NEW_W | NEW_U )

// Debug macro to help pretty-print changes
#define DEBUG_OUTPUT_CHANGES(c) \
	((c) & NEW_C        ? 'C' : ' '), \
	((c) & NEW_I        ? 'I' : ' '), \
	((c) & TCP_PUSH_BIT ? 'P' : ' '), \
	((c) & NEW_S        ? 'S' : ' '), \
	((c) & NEW_A        ? 'A' : ' '), \
	((c) & NEW_W        ? 'W' : ' '), \
	((c) & NEW_U        ? 'U' : ' ')

static PBYTE
EncodeValue(
    IN  PBYTE     pEncode,
	IN  USHORT    value)
{
	if (1 <= value && value <= 255)
	{
		*pEncode++ = (BYTE)value;
	}
	else
	{
		pEncode[0] = 0;
		pEncode[1] = (BYTE)(value >> 8); // MSB first
		pEncode[2] = (BYTE)(value);      // LSB second
		pEncode += 3;
	}
	return pEncode;
}

#define ENCODE( n )  cp = EncodeValue(cp, (n))
#define ENCODEZ( n ) cp = EncodeValue(cp, (n))

static USHORT
CompressedValueGet(
	IN OUT PBYTE *pcp,
	IN     PBYTE  end)
//
//  Value may be encoded as one byte if it is in range 1-255.
//  If it is >255, then it will be encoded as 0x00 followed by 2 bytes.
//
{
	USHORT Value;
	PBYTE  cp = *pcp;
	
	if (cp >= end)
	{
		// No more data available, bad packet.
		// We tell the caller of the bad packet by setting cp
		// beyond the end of the data.
		Value = 0;
		cp = end + 1;
	}
	else
	{
		Value = *cp++;
		if (Value == 0)
		{
			// Value is in next 2 bytes
			if ((cp + 1) < end)
				Value = (cp[0] << 8) | cp[1];

			cp += 2;
		}
		*pcp = cp;
	}

	return Value;
}

static ULONG 
NetLongAdd(
	IN     ULONG  OldValue,
	IN     USHORT Addend)
//
//  Add a USHORT (in host byte order) to a ULONG (in net byte order).
//  Return the sum as a ULONG (in net byte order).
//
{
	ULONG  NewValue;

	OldValue = ntohl(OldValue);
	NewValue = OldValue + Addend;
	return htonl(NewValue);
}

static ULONG
NetLongAddCompressed(
	IN OUT PBYTE *pcp,
	IN     PBYTE  end,
	IN     ULONG  OldValue)
{
	USHORT  Addend;

	Addend = CompressedValueGet(pcp, end);
	return NetLongAdd(OldValue, Addend);
}

static USHORT 
NetShortAdd(
	IN     USHORT OldValue,
	IN     USHORT Addend)
//
//  Add a USHORT (in host byte order) to a USHORT (in net byte order).
//  Return the sum as a USHORT (in net byte order).
//
{
	USHORT  NewValue;

	OldValue = ntohs(OldValue);
	NewValue = OldValue + Addend;
	return htons(NewValue);
}

static USHORT
NetShortAddCompressed(
	IN OUT PBYTE *pcp,
	IN     PBYTE  end,
	IN     USHORT OldValue)
{
	USHORT  Addend;

	Addend = CompressedValueGet(pcp, end);
	return NetShortAdd(OldValue, Addend);
}

static  BYTE
TcpHdrLen( struct TCPHeader *arg )
//
//	Return the length in bytes of the TCP header.
//
{
	BYTE	bHeaderLength;

	// Flags is a USHORT field that looks like:
	//
	//     4 bit header length in DWORDS
	//     6 bits reserved
	//     6 bits URG/ACK/PSH/RST/SYN/FIN
	//
	bHeaderLength = *(PBYTE)(&arg->tcp_flags);
	bHeaderLength = (bHeaderLength & 0xF0) >> 2;

	return bHeaderLength;
}

//   A.2  Compression
//
//   This routine looks daunting but isn't really.  The code splits into four
//   approximately equal sized sections:  The first quarter manages a
//   circularly linked, least-recently-used list of `active' TCP
//   connections./47/  The second figures out the sequence/ack/window/urg
//   changes and builds the bulk of the compressed packet.  The third handles
//   the special-case encodings.  The last quarter does packet ID and
//   connection ID encoding and replaces the original packet header with the
//   compressed header.
//
//   The arguments to this routine are a pointer to a packet to be
//   compressed, a pointer to the compression state data for the serial line,
//   and a flag which enables or disables connection id (C bit) compression.
//
//   Compression is done `in-place' so, if a compressed packet is created,
//   both the start address and length of the incoming packet (the off and
//   len fields of m) will be updated to reflect the removal of the original
//   header and its replacement by the compressed header.  If either a
//   compressed or uncompressed packet is created, the compression state is
//   updated.  This routines returns the packet type for the transmit framer
//   (TYPE_IP, TYPE_UNCOMPRESSED_TCP or TYPE_COMPRESSED_TCP).
//
//   Because 16 and 32 bit arithmetic is done on various header fields, the
//   incoming IP packet must be aligned appropriately (e.g., on a SPARC, the
//   IP header is aligned on a 32-bit boundary).  Substantial changes would
//   have to be made to the code below if this were not true (and it would
//   probably be cheaper to byte copy the incoming header to somewhere
//   correctly aligned than to make those changes).
//
//   Note that the outgoing packet will be aligned arbitrarily (e.g., it
//   could easily start on an odd-byte boundary).
//
//   ----------------------------
//    47. The two most common operations on the connection list are a `find'
//   that terminates at the first entry (a new packet for the most recently
//   used connection) and moving the last entry on the list to the head of
//   the list (the first packet from a new connection).  A circular list
//   efficiently handles these two operations.
//   ----------------------------

USHORT
sl_compress_tcp(
	IN  PNDIS_WAN_PACKET  pPacket, 
	IN  slcompress_t     *comp)
{
	cstate_t            *cs;                    // compression state pntr
	struct IPHeader     *ip;                    // current ip header 
	struct TCPHeader    *tcp;                   // current TCP header 
	BYTE                ipLen;                  // ip header length
	BYTE                tcpLen;                 // tcp header length
	BYTE                totalLen;               // total length of headers
	u_int               orig_xsum;              // original checksum
	u_int               newHdrLen;              // new header length
	u_int               changes = 0;            // change mask 
	BYTE                new_seq[ 16 ];          // last to current changes
	BYTE                *cp = new_seq;
	u_int               deltaS;                 // general purpose vars 
	u_int               deltaA;                 // general purpose vars 
	u_int               deltaW;                 // general purpose vars 

	memset(new_seq, 0, 16);

    if (pPacket->CurrentLength < sizeof(TCPHeader) + sizeof(IPHeader))
    {
		// Too small to be a TCP packet, no compression possible
        return PPP_PROTOCOL_IP;
    }

    // Init headers pointers and lengths

    ip     = (struct IPHeader *)pPacket->CurrentBuffer;
    ipLen  = IpHdrLen(ip);

	//
	// Check the protocol type to confirm that it is TCP
	// If not TCP, don't try to do TCP header compression!
	//
	if (ip->iph_protocol != 6) // 6==PROTOCOL_TCP
	{
		return PPP_PROTOCOL_IP;
	}

    tcp    = (struct TCPHeader *)((BYTE *)ip + ipLen);
    tcpLen = TcpHdrLen(tcp); 

    totalLen = ipLen + tcpLen;

    // Exit if this is an ip fragment

    if (ip->iph_offset & 0xff3f)
    {
		DEBUGMSG(ZONE_VJ, (L"PPP: TX VJ cannot compress - fragmented (offset=%x)\n", ip->iph_offset));
        return PPP_PROTOCOL_IP;
    }

    // Exit if the TCP packet isn't `compressible' i.e.the ACK isn't set 
    // or some other control bit is set.

    if ((tcp->tcp_flags & 
        (TCP_FLAG_SYN | TCP_FLAG_FIN | TCP_FLAG_RST | TCP_FLAG_ACK)) 
        != TCP_FLAG_ACK)
    {
		DEBUGMSG(ZONE_VJ, (L"PPP: TX VJ cannot compress - tcp_flags=0x%x\n", tcp->tcp_flags));
        return PPP_PROTOCOL_IP;
    }

    // Packet is compressible
    //
    // We're going to send either a COMPRESSED_TCP or UNCOMPRESSED_TCP packet.
    // Either way we need to locate (or create) the connection state.  Special 
    // case the most recently used connection since it's most likely to be 
    // used again & we don't have to do any reordering if it's used.
    // Compare the ip/tcp src and dest fields.

    cs = comp->last_cs->cs_next;                // access compression state

    if ((ip->iph_src   != cs->cs_ip.iph_src   ) ||
        (ip->iph_dest  != cs->cs_ip.iph_dest  ) ||
        (tcp->tcp_src  != cs->cs_tcp->tcp_src ) ||
        (tcp->tcp_dest != cs->cs_tcp->tcp_dest) )
    {
        // Wasn't the first -- search for it.
        //
        // States are kept in a circularly linked list with last_cs
        // pointing to the end of the list.  The list is kept in lru
        // order by moving a state to the head of the list whenever
        // it is referenced.  Since the list is short and,
        // empirically, the connection we want is almost always near
        // the front, we locate states via linear search.  If we
        // don't find a state for the datagram, the oldest state is
        // (re-)used.
             
        cstate_t *lcs;
        cstate_t *lastcs = comp->last_cs;

        do 
        {
            lcs = cs;
            cs = cs->cs_next;

            // Compare connection data

            if ((ip->iph_src   == cs->cs_ip.iph_src   ) && 
                (ip->iph_dest  == cs->cs_ip.iph_dest  ) && 
                (tcp->tcp_src  == cs->cs_tcp->tcp_src ) &&
                (tcp->tcp_dest == cs->cs_tcp->tcp_dest) )
            {

                goto found;
            }

        } 
        while(cs != lastcs);

        // Didn't find it -- re-use oldest cstate_t. Send an uncompressed 
        // packet that tells the other side what connection number we're 
        // using for this conversation. Note that since the state list is 
        // circular, the oldest state points to the newest and we only need 
        // to set last_cs to update the lru linkage.
              
        comp->last_cs = lcs;
		DEBUGMSG(ZONE_VJ, (L"PPP: TX VJ slot %x No previous cstate, uncompressed\n", cs->cs_id));
        goto uncompressed;


found:  //----------------------------------------------------------------------
        // Found State -- move to the front on the connection list. 

        if (lastcs == cs)
        {
            comp->last_cs = lcs;
        }
        else 
        {
            lcs->cs_next    = cs->cs_next;
            cs->cs_next     = lastcs->cs_next;
            lastcs->cs_next = cs;
        }
    }

    // Make sure that only what we expect to change changed. 
    //
    // Line 1: checks the IP protocol version, header length & TOS
    // Line 2: checks the "Don't fragment" bit.
    // Line 3: checks the time-to-live and protocol - unnecessary but free.
    // Line 4: checks the TCP header length.  
    // Line 5: checks IP options, if any.  '
    // Line 6: checks TCP options, if any.  
    //
    // If any of these things are different between the previous & current 
    // datagram, we send the current datagram `uncompressed'.
         
    if (((USHORT *)ip)[ 0 ]  != ((USHORT *)&cs->cs_ip)[ 0 ]  ||
        ((USHORT *)ip)[ 3 ]  != ((USHORT *)&cs->cs_ip)[ 3 ]  ||
        ((USHORT *)ip)[ 4 ]  != ((USHORT *)&cs->cs_ip)[ 4 ]  ||
        (tcpLen           != cs->cs_tcpLen)                ||
        ((ipLen  > 20) && memcmp(ip+1, &(cs->cs_ip) +1, ipLen  - 20)) ||
        ((tcpLen > 20) && memcmp(tcp+1,  cs->cs_tcp +1, tcpLen - 20)) ) 
    {
		DEBUGMSG(ZONE_VJ, (L"PPP: TX VJ slot %x Header changed, uncompressed\n", cs->cs_id));
        goto uncompressed;
    }

    // Figure out which of the changing fields changed.  The receiver
    // expects changes in the order: urgent, window, ack, seq.
         
    if (tcp->tcp_flags & TCP_FLAG_URG) 
    {
        deltaS = ntohs(tcp->tcp_urgent);
        ENCODEZ(deltaS);
        changes |= NEW_U;
    } 
    else if (tcp->tcp_urgent != cs->cs_tcp->tcp_urgent)
    {
        // URG not set but urp changed -- a sensible implementation 
        // should never do this but RFC793 doesn't prohibit the change 
        // so we have to deal with it.
              
		DEBUGMSG(ZONE_VJ, (L"PPP: TX VJ slot %x URG not set but urp changed, uncompressed\n", cs->cs_id));
        goto uncompressed;
    }

    // Window size change

    deltaW = ntohs(tcp->tcp_window) - ntohs(cs->cs_tcp->tcp_window);
    if (deltaW)
    {
        ENCODE(deltaW);
        changes |= NEW_W;
    }

    // Ack changes

    deltaA = ntohl(tcp->tcp_ack) - ntohl(cs->cs_tcp->tcp_ack);
    if (deltaA > 0xffff)
	{
		DEBUGMSG(ZONE_VJ, (L"PPP: TX VJ slot %x deltaA %x > 0xffff, uncompressed\n", cs->cs_id, deltaA));
        goto uncompressed;
	}
	if (deltaA)
    {
        ENCODE(deltaA);
        changes |= NEW_A;
    }

    // Sequence # changes

    deltaS = ntohl(tcp->tcp_seq) - ntohl(cs->cs_tcp->tcp_seq);
    if (deltaS > 0xffff)
	{
		DEBUGMSG(ZONE_VJ, (L"PPP: TX VJ slot %x tcp_seq deltaS %x > 0xffff, uncompressed\n", cs->cs_id, deltaS));
        goto uncompressed;
	}
	if (deltaS)
	{
        ENCODE(deltaS);
        changes |= NEW_S;
    }

    // Look for the special-case encodings.

    switch(changes) 
    {
    case 0:

        // Nothing changed. If this packet contains data and the last one 
        // didn't, this is probably a data packet following an ack (normal 
        // on an interactive connection) and we send it compressed.  
        // Otherwise it's probably a retransmit, retransmitted ack or window 
        // probe.  Send it uncompressed in case the other side missed the 
        // compressed version.
              
        if ((ip->iph_length != cs->cs_ip.iph_length)  &&
            (ntohs(cs->cs_ip.iph_length) == totalLen))        
        {
            break;
        }

        // (fall through) 

    case SPECIAL_I:
    case SPECIAL_D:
        
        // Actual changes match one of our special case encodings --
        // send packet uncompressed.
              
		DEBUGMSG(ZONE_VJ, (L"PPP: TX VJ slot %x special case or retransmit (changes=%x), uncompressed\n", cs->cs_id, changes));
        goto uncompressed;
        break;

    case NEW_S | NEW_A:

         if (deltaS == deltaA &&
             deltaS == (u_int)(ntohs(cs->cs_ip.iph_length) - totalLen)) 
         {
			// special case for echoed terminal traffic 

            changes = SPECIAL_I;
            cp = new_seq;
         }
         break;

    case NEW_S:

         if (deltaS == (u_int)(ntohs(cs->cs_ip.iph_length) - totalLen)) 
         {
			// special case for data xfer 

			changes = SPECIAL_D;
			cp = new_seq;
         }
         break;
    }


    deltaS = ntohs(ip->iph_id) - ntohs(cs->cs_ip.iph_id);

    if (deltaS != 1) 
    {
        ENCODEZ(deltaS);
        changes |= NEW_I;
    }

    if (tcp->tcp_flags & TCP_FLAG_PUSH)
    {
        changes |= TCP_PUSH_BIT;
    }

    // Save the cksum before we overwrite it below. 
    // Then update our state with this packet's header.

    orig_xsum = ntohs(tcp->tcp_xsum);

    // Save connection's state

    CTEMemCopy(&cs->cs_ip, ip, ipLen + tcpLen);
    cs->cs_tcp    = (struct TCPHeader *)(cs->slcs_u.hdr + ipLen);        
    cs->cs_ipLen  = ipLen;
    cs->cs_tcpLen = tcpLen;

    // We want to use the original packet as our compressed packet. (cp -
    // new_seq) is the number of bytes we need for compressed sequence
    // numbers.  In addition we need one byte for the change mask, one
    // for the connection id and two for the tcp checksum. So, (cp -
    // new_seq) + 4 bytes of header are needed.  totalLen is how many bytes
    // of the original packet to toss so subtract the two to get the new
    // packet size.

    newHdrLen = cp - new_seq;                   // compute new header length
    cp = (BYTE *)ip;

    if ((0 == (comp->flags & SLF_ENABLE_SLOTID_COMPRESSION_TX)) || (comp->last_xmit != cs->cs_id)) 
    {
        // Add compressed header with cid

         comp->last_xmit = cs->cs_id;

         totalLen -= newHdrLen + 4;             // adjust total header size
         cp += totalLen;                        // update start of packet
		 changes |= NEW_C;
         *cp++ = changes;                       // add change mask + cid flag 
         *cp++ = cs->cs_id;                     // add cid
    } 
    else 
    {
        // Add compressed header 

         totalLen -= newHdrLen + 3;             // adjust total header size
         cp += totalLen;                        // update start of packet
         *cp++ = changes;                       // add change mask
    }

	DEBUGMSG(ZONE_VJ, (L"PPP: TX VJ slot %x %c%c%c%c%c%c deltaS=%d deltaA=%d, deltaW=%d\n", cs->cs_id, DEBUG_OUTPUT_CHANGES(changes), deltaS, deltaA, deltaW));

    *cp++ = orig_xsum >> 8;                     // Put back the checksum
    *cp++ = orig_xsum;

    CTEMemCopy(cp, new_seq, newHdrLen);       // Copy in the new header 

    // Update TX buffer elements 

    pPacket->CurrentLength -= totalLen;
    (BYTE *)pPacket->CurrentBuffer += totalLen;

    return PPP_PROTOCOL_COMPRESSED_TCP;              // packet is compressed


uncompressed: //---------------------------------------------------------------

    // Update connection state cs & send uncompressed packet ('uncompressed' 
    // means a regular ip/tcp packet but with the 'conversation id' we hope 
    // to use on future compressed packets in the protocol field).

    // Save connection's state

    CTEMemCopy(&cs->cs_ip, ip, ipLen + tcpLen);
    cs->cs_tcp    = (struct TCPHeader *)(cs->slcs_u.hdr + ipLen);        
    cs->cs_ipLen  = ipLen;
    cs->cs_tcpLen = tcpLen;

    // Update the protocol field with the 'conversation id'

    ip->iph_protocol = cs->cs_id;

    // Save this id as the last

    comp->last_xmit = cs->cs_id;

    return PPP_PROTOCOL_UNCOMPRESSED_TCP;
}

//   A.3  Decompression
//
//   This routine decompresses a received packet.  It is called with a
//   pointer to the packet, the packet length and type, and a pointer to the
//   compression state structure for the incoming serial line.  It returns a
//   pointer to the resulting packet or zero if there were errors in the
//   incoming packet.  If the packet is COMPRESSED_TCP or UNCOMPRESSED_TCP,
//   the compression state will be updated.
//
//   The new packet will be constructed in-place.  That means that there must
//   be 128 bytes of free space in front of bufp to allow room for the
//   reconstructed IP and TCP headers.  The reconstructed packet will be
//   aligned on a 32-bit boundary.
//
//   NOTE: The received packet is NOT necessarily even aligned. All access
//         must take this into account.

BOOL
sl_update_tcp(
	IN      BYTE         *bufp,
	IN      DWORD         len,
	IN OUT  slcompress_t *comp)
//
//  Handle an PPP_PROTOCOL_UNCOMPRESSED_TCP packet header used to update our state.
//
//  The TCP/IP header of a VJUIP packet will be unmodified except for the IPHeader
//  iph_protocol field which the sender will have changed from 0x06 (TCP) to the
//  index of the state element to be updated. This function will
//  restore the protocol field to 0x06.
//
//  If the packet is invalid return FALSE. If it is valid return TRUE.
//
{
	u_int               hlen;
	struct IPHeader     *ip;
	cstate_t            *cs;
	BYTE                iState;

    // Locate the saved state for this connection.  If the state
    // index is legal, clear the 'discard' flag.

	if (len < sizeof(struct IPHeader))
		return FALSE;

	ip = (struct IPHeader *)bufp;
	hlen = IpHdrLen(ip);

	if (hlen + sizeof(TCPHeader) > len)
		return FALSE;

    hlen += TcpHdrLen((TCPHeader *)(((BYTE *)ip) + hlen));
	if (hlen > len)
		return FALSE;

	iState = ip->iph_protocol;
    if (iState >= MAX_STATES) 
        return FALSE;

	if (iState >= comp->MaxStatesRx)
	{
		DEBUGMSG(ZONE_WARN, (L"PPP: WARNING - Peer sent VJ uncompressed packet slot=%u > negotiated max=%u\n", iState, comp->MaxStatesRx - 1));
	}

	DEBUGMSG(ZONE_VJ, (L"PPP: RX VJ slot %x uncompressed\n", iState));

	comp->last_recv = iState;
    cs = &comp->rstate[ iState ];

    comp->LastRxFrameBad = FALSE;

    // Restore the IP protocol field then save a copy of this packet 
    // header. The checksum is zeroed in the copy so we don't have to 
    // zero it each time we process a compressed packet.

    ip->iph_protocol = IPPROTO_TCP;

    CTEMemCopy(&cs->cs_ip, ip, hlen);

    cs->cs_ip.iph_xsum = 0;
    cs->cs_hlen = hlen;

	return TRUE;
}

BYTE *
sl_uncompress_tcp(
	IN      BYTE         *bufp,
	IN OUT  DWORD        *lenp,
	IN OUT  slcompress_t *comp)
//
//  Handle an PPP_PROTOCOL_COMPRESSED_TCP packet header.
//
//  Returns a pointer to the uncompressed packet header if successful,
//  NULL if it fails.
//
//  Format of compressed header:
//		BYTE    Changes
//		BYTE    StateIndex (present iff NEW_C is set in Changes)
//      BYTE[2] TCPChecksum
// 
{
	BYTE                *cp;
	cstate_t            *cs;
	struct TCPHeader    *th;
	u_int               hlen;
	u_int               changes;
	DWORD               cbBuf = *lenp;
	USHORT              cbData,
						cbPacket,
						cbUserDataInLastPacket;
	PBYTE               pBufEnd = bufp + cbBuf;
	BYTE                iState;
	USHORT              IpIdIncrement;
	USHORT              tcp_flags;

    // Compressed packet - set frame pointer

    cp = bufp;

	if (cp >= pBufEnd)
		goto bad;
    changes = *cp++;

    if (changes & NEW_C) 
    {
		if (cp >= pBufEnd)
			goto bad;
		iState = *cp++;

        // Make sure the state index is in range, then grab the state. 
        // If we have a good state index, clear the 'discard' flag.

        if (iState >= MAX_STATES)
		{
			DEBUGMSG(ZONE_WARN, (L"PPP: WARNING - Peer sent VJ compressed packet slot=%u >= MAX_STATES(%u)\n", iState, MAX_STATES));
            goto bad;
		}

        if (iState >= comp->MaxStatesRx)
		{
			// Since we allocated MAX_STATES slots, we can handle this slotID. However, the peer
			// is technically in violation of the negotiated value so log a debug message.
			DEBUGMSG(ZONE_WARN, (L"PPP: WARNING - Peer sent VJ compressed packet slot=%u >= negotiated max=%u\n", iState, comp->MaxStatesRx));
		}

		comp->LastRxFrameBad = FALSE;
        comp->last_recv = iState;
    } 
    else 
    {
		DEBUGMSG(ZONE_WARN && (!(comp->flags & SLF_ENABLE_SLOTID_COMPRESSION_RX)), (L"PPP: RX VJ compressed slot ID, but we requested no slot ID compression\n"));

        // This packet has an implicit state index.  If we've had a
        // line error since the last time we got an explicit state
        // index, we have to toss the packet.
              
        if (comp->LastRxFrameBad)
		{
			DEBUGMSG(ZONE_VJ, (L"PPP: RX VJ implicit state index but no prior state, toss\n"));
            return NULL;
		}

		iState = comp->last_recv;

		//
		// If no previous packet was received to initialize the last_recv
		// state index, then it will be 255 and this packet is invalid. However,
		// the LastRxFrameBad flag which was checked above should have been set at
		// init time, so we shouldn't get here.
		//
		ASSERT(iState < comp->MaxStatesRx);
    }

	DEBUGMSG(ZONE_VJ, (L"PPP: RX VJ slot %x %c%c%c%c%c%c\n", iState, DEBUG_OUTPUT_CHANGES(changes)));

    // Find the state then fill in the TCP checksum and PUSH bit.

    cs = &comp->rstate[iState];
    hlen = IpHdrLen(&cs->cs_ip);

    th = (struct TCPHeader *)&((BYTE *)&cs->cs_ip)[ hlen ];

	// cp[0] and cp[1] contain the new tcp xsum
	if ((cp +  1) >= pBufEnd)
		goto bad;

	memcpy(&th->tcp_xsum, cp, 2);
	cp += 2;

	tcp_flags = th->tcp_flags;
    tcp_flags &= ~TCP_FLAG_PUSH;
    if (changes & TCP_PUSH_BIT)
		tcp_flags |= TCP_FLAG_PUSH;

    // Fix up the state's ack, seq, urg and win fields based on the changemask.

	// The amount of user data in the last packet is calculated by
	// subtracting the TCP and IP header lengths from the IP total
	// length in the saved header. This value will be used in the
	// SPECIAL_I and SPECIAL_D cases below. We calculate it once here
	// to minimize size.
	cbUserDataInLastPacket = ntohs(cs->cs_ip.iph_length) - cs->cs_hlen;

    switch(changes & SPECIALS_MASK)
    {
    case SPECIAL_I:
		//
		// Terminal traffic special case
		// The amount of user data in the last packet is added to
		// both the TCP sequence number and ACK fields in the saved header.
		//
		th->tcp_ack = NetLongAdd(th->tcp_ack, cbUserDataInLastPacket);
        // FALLTHROUGH
	case SPECIAL_D:
		//
		// Unidirectional special case
		// The amount of user data in the last packet is added to
		// the TCP sequence number in the saved header.
		//
		th->tcp_seq = NetLongAdd(th->tcp_seq, cbUserDataInLastPacket);
        break;

    default:
       tcp_flags &= ~TCP_FLAG_URG;
       if (changes & NEW_U) 
        {
            tcp_flags |= TCP_FLAG_URG;
			th->tcp_urgent = NetShortAddCompressed(&cp, pBufEnd, 0);
        } 

        if (changes & NEW_W)
			th->tcp_window = NetShortAddCompressed(&cp, pBufEnd, th->tcp_window);

        if (changes & NEW_A)
			th->tcp_ack = NetLongAddCompressed(&cp, pBufEnd, th->tcp_ack);

        if (changes & NEW_S)
			th->tcp_seq = NetLongAddCompressed(&cp, pBufEnd, th->tcp_seq);
        break;
    }

	th->tcp_flags = tcp_flags;

	//
    // Update the IP ID, by default it will increment by 1, but if the
	// NEW_I flag was set in the changes byte then it increments by the
	// amount encoded in the packet as a compressed value.
	//
	IpIdIncrement = 1;
    if (changes & NEW_I)
		IpIdIncrement = CompressedValueGet(&cp, pBufEnd);
	cs->cs_ip.iph_id = NetShortAdd(cs->cs_ip.iph_id, IpIdIncrement);

	if (cp > pBufEnd)
	{
		// 
		// The VJ compressed header was too short / incomplete.
		//
#ifdef DEBUG
		if (ZONE_WARN)
		{
			DEBUGMSG(1, (L"PPP: WARNING - Incomplete VJ header received, discarding packet\n"));
			DumpMem(bufp, cp - bufp);
		}
#endif
		goto bad;
	}

    // At this point, cp points to the first byte of data in the packet.
    // If we're not aligned on a 4-byte boundary, copy the data down so
    // the IP & TCP headers will be aligned.  Then back up cp by the
    // TCP/IP header length to make room for the reconstructed header (we
    // assume the packet we were handed has enough space to prepend 128
    // bytes of header).  Adjust the length to account for the new header
    // & fill in the IP total length.

	cbData = pBufEnd - cp;

	// If data is not DWORD aligned, then copy it down 1-3 bytes to where it is aligned

    if ((int)cp & 3) 
    {
		// Perform safe overlapping copy
        memmove((BYTE *)((int)cp & ~3), cp, cbData);    
		cp = (BYTE *)((int)cp & ~3);
    }

	// Now fill in the TCP/IP header in front of the data
    cp = cp - cs->cs_hlen;
    cbPacket = cs->cs_hlen + cbData;
	*lenp = cbPacket;
    cs->cs_ip.iph_length = (USHORT) htons(cbPacket);

    memcpy(cp, &cs->cs_ip, cs->cs_hlen);

    // Recompute the ip header checksum 

    {
        USHORT *bp = (USHORT *)cp;

        for(changes = 0; hlen > 0; hlen -= 2, bp++)
		{
			changes = changes + *bp;
		}
        changes = (changes & 0xffff) + (changes >> 16);
        changes = (changes & 0xffff) + (changes >> 16);
        ((struct IPHeader *)cp)->iph_xsum = ~changes;
    }
    return cp;


bad://--------------------------------------------------------------------------
    // Bad Packet

    comp->LastRxFrameBad = TRUE;
    return NULL;
}

// Initialization
//
// This routine initializes the state structure for both the transmit and
// receive halves of some serial line.  It must be called each time the
// line is brought up.

void
sl_compress_init(
	OUT	slcompress_t *comp,
	IN  BYTE          MaxStatesRx,
	IN  BYTE          MaxStatesTx,
	IN  BOOL          CompressSlotIdsRx,
	IN  BOOL          CompressSlotIdsTx)
{
    u_int i;
    cstate_t *tstate = comp->tstate;

    // Reset the compression record 

    CTEMemSet((char *) comp, 0, sizeof(slcompress_t));

	if (MaxStatesRx > MAX_STATES)
		MaxStatesRx = MAX_STATES;

	if (MaxStatesTx > MAX_STATES)
		MaxStatesTx = MAX_STATES;

    // Link the transmit states into a circular list.

    for(i = MaxStatesTx - 1; i > 0; --i) 
    {
         tstate[i].cs_id   = i;
         tstate[i].cs_next = &tstate[ i-1 ];
    }

    tstate[0].cs_next = &tstate[ MaxStatesTx - 1 ];
    tstate[0].cs_id   = 0;
    comp->last_cs     = &tstate[ 0 ];

    // Make sure we don't accidentally do CID compression
    // (assumes MAX_STATES < 255).
         
    comp->last_recv = 255;
    comp->last_xmit = 255;

	comp->LastRxFrameBad = TRUE;
	if (CompressSlotIdsRx)
		comp->flags |= SLF_ENABLE_SLOTID_COMPRESSION_RX;
	if (CompressSlotIdsTx)
		comp->flags |= SLF_ENABLE_SLOTID_COMPRESSION_TX;

	comp->MaxStatesTx = MaxStatesTx;
	comp->MaxStatesRx = MaxStatesRx;
}

/*
   A.5  Berkeley Unix dependencies

   Note:  The following is of interest only if you are trying to bring the
   sample code up on a system that is not derived from 4BSD (Berkeley
   Unix).

   The code uses the normal Berkeley Unix header files (from
   /usr/include/netinet) for definitions of the structure of IP and TCP
   headers.  The structure tags tend to follow the protocol RFCs closely
   and should be obvious even if you do not have access to a 4BSD
   system./48/

   ----------------------------
    48. In the event they are not obvious, the header files (and all the
   Berkeley networking code) can be anonymous ftp'd from host


   The macro BCOPY(src, dst, amt) is invoked to copy amt bytes from src to
   dst.  In BSD, it translates into a call to bcopy.  If you have the
   misfortune to be running System-V Unix, it can be translated into a call
   to memcpy.  The macro OVBCOPY(src, dst, amt) is used to copy when src
   and dst overlap (i.e., when doing the 4-byte alignment copy).  In the
   BSD kernel, it translates into a call to ovbcopy.  Since AT&T botched
   the definition of memcpy, this should probably translate into a copy
   loop under System-V.

   The macro BCMP(src, dst, amt) is invoked to compare amt bytes of src and
   dst for equality.  In BSD, it translates into a call to bcmp.  In
   System-V, it can be translated into a call to memcmp or you can write a
   routine to do the compare.  The routine should return zero if all bytes
   of src and dst are equal and non-zero otherwise.

   The routine ntohl(dat) converts (4 byte) long dat from network byte
   order to host byte order.  On a reasonable cpu this can be the no-op
   macro:
                           #define ntohl(dat) (dat)

   On a Vax or IBM PC (or anything with Intel byte order), you will have to
   define a macro or routine to rearrange bytes.

   The routine ntohs(dat) is like ntohl but converts (2 byte) shorts
   instead of longs.  The routines htonl(dat) and htons(dat) do the inverse
   transform (host to network byte order) for longs and shorts.

   A struct mbuf is used in the call to sl_compress_tcp because that
   routine needs to modify both the start address and length if the
   incoming packet is compressed.  In BSD, an mbuf is the kernel's buffer
   management structure.  If other systems, the following definition should
   be sufficient:

struct mbuf 
{
    BYTE  *m_off;     // pointer to start of data 
    int     m_len;      // length of dat
};


#define mtod(m, t) ((t)(m->m_off))

   ----------------------------
   ucbarpa.berkeley.edu, files pub/4.3/tcp.tar and pub/4.3/inet.tar.
   ----------------------------

   B  Compatibility with past mistakes

   When combined with the modern PPP serial line protocol[9], the use of
   header compression is automatic and invisible to the user.
   Unfortunately, many sites have existing users of the SLIP described in
   [12] which doesn't allow for different protocol types to distinguish
   header compressed packets from IP packets or for version numbers or an
   option exchange that could be used to automatically negotiate header
   compression.

   The author has used the following tricks to allow header compressed SLIP
   to interoperate with the existing servers and clients.  Note that these
   are hacks for compatibility with past mistakes and should be offensive
   to any right thinking person.  They are offered solely to ease the pain
   of running SLIP while users wait patiently for vendors to release PPP.


   B.1  Living without a framing `type' byte

   The bizarre packet type numbers in sec. A.1 were chosen to allow a
   `packet type' to be sent on lines where it is undesirable or impossible
   to add an explicit type byte.  Note that the first byte of an IP packet
   always contains `4' (the IP protocol version) in the top four bits.  And
   that the most significant bit of the first byte of the compressed header
   is ignored.  Using the packet types in sec. A.1, the type can be encoded
   in the most significant bits of the outgoing packet using the code

                    p->dat[0] |= sl_compress_tcp(p, comp);

    and decoded on the receive side by

                  if (p->dat[0] & 0x80)
                          type = TYPE_COMPRESSED_TCP;
                  else if (p->dat[0] >= 0x70) {
                          type = TYPE_UNCOMPRESSED_TCP;
                          p->dat[0] &=~ 0x30;
                  } else
                          type = TYPE_IP;
                  status = sl_uncompress_tcp(p, type, comp);


   B.2  Backwards compatible SLIP servers

   The SLIP described in [12] doesn't include any mechanism that could be
   used to automatically negotiate header compression.  It would be nice to
   allow users of this SLIP to use header compression but, when users of
   the two SLIP varients share a common server, it would be annoying and
   difficult to manually configure both ends of each connection to enable
   compression.  The following procedure can be used to avoid manual
   configuration.

   Since there are two types of dial-in clients (those that implement
   compression and those that don't) but one server for both types, it's
   clear that the server will be reconfiguring for each new client session
   but clients change configuration seldom if ever.  If manual
   configuration has to be done, it should be done on the side that changes
   infrequently --- the client.  This suggests that the server should
   somehow learn from the client whether to use header compression.
   Assuming symmetry (i.e., if compression is used at all it should be used
   both directions) the server can use the receipt of a compressed packet
   from some client to indicate that it can send compressed packets to that
   client.  This leads to the following algorithm:

   There are two bits per line to control header compression:  allowed and
   on.  If on is set, compressed packets are sent, otherwise not.  If
   allowed is set, compressed packets can be received and, if an
   UNCOMPRESSED_TCP packet arrives when on is clear, on will be set./49/
   If a compressed packet arrives when allowed is clear, it will be
   ignored.

   Clients are configured with both bits set (allowed is always set if on
   is set) and the server starts each session with allowed set and on
   clear.  The first compressed packet from the client (which must be a
   UNCOMPRESSED_TCP packet) turns on compression for the server.


   ----------------------------
    49. Since [12] framing doesn't include error detection, one should be
   careful not to `false trigger' compression on the server.  The
   UNCOMPRESSED_TCP packet should checked for consistency (e.g., IP
   checksum correctness) before compression is enabled.  Arrival of
   COMPRESSED_TCP packets should not be used to enable compression.
   ----------------------------


   As noted in sec. 3.2.2, easily detected patterns exist in the stream of
   compressed headers, indicating that more compression could be done.
   Would this be worthwhile?

   The average compressed datagram has only seven bits of header./50/  The
   framing must be at least one bit (to encode the `type') and will
   probably be more like two to three bytes.  In most interesting cases
   there will be at least one byte of data.  Finally, the end-to-end
   check---the TCP checksum---must be passed through unmodified./51/

   The framing, data and checksum will remain even if the header is
   completely compressed out so the change in average packet size is, at
   best, four bytes down to three bytes and one bit --- roughly a 25%
   improvement in delay./52/  While this may seem significant, on a 2400
   bps line it means that typing echo response takes 25 rather than 29 ms.
   At the present stage of human evolution, this difference is not
   detectable.

   However, the author sheepishly admits to perverting this compression
   scheme for a very special case data-acquisition problem:  We had an
   instrument and control package floating at 200KV, communicating with
   ground level via a telemetry system.  For many reasons (multiplexed
   communication, pipelining, error recovery, availability of well tested
   implementations, etc.), it was convenient to talk to the package using
   TCP/IP. However, since the primary use of the telemetry link was data
   acquisition, it was designed with an uplink channel capacity <0.5% the
   downlink's.  To meet application delay budgets, data packets were 100
   bytes and, since TCP acks every other packet, the relative uplink
   bandwidth for acks is a/200 where `a' is the total size of ack packets.
   Using the scheme in this paper, the smallest ack is four bytes which
   would imply an uplink bandwidth 2% of the downlink.  This wasn't

   ----------------------------
    50. Tests run with several million packets from a mixed traffic load
   (i.e., statistics kept on a year's traffic from my home to work) show
   that 80% of packets use one of the two special encodings and, thus, the
   only header is the change mask.
    51. If someone tries to sell you a scheme that compresses the TCP
   checksum `Just say no'.  Some poor fool has yet to have the sad
   experience that reveals the end-to-end argument is gospel truth.  Worse,
   since the fool is subverting your end-to-end error check, you may pay
   the price for this education and they will be none the wiser.  What does
   it profit a man to gain two byte times of delay and lose peace of mind?
    52. Note again that we must be concerned about interactive delay to be
   making this argument:  Bulk data transfer performance will be dominated
   by the time to send the data and the difference between three and four
   byte headers on a datagram containing tens or hundreds of data bytes is,
   practically, no difference.
   ----------------------------

   possible so we used the scheme described in footnote 15:  If the first
   bit of the frame was one, it meant `same compressed header as last
   time'.  Otherwise the next two bits gave one of the types described in
   sec. 3.2.  Since the link had excellent forward error correction and
   traffic made only a single hop, the TCP checksum was compressed out
   (blush!) of the `same header' packet types/53/ so the total header size
   for these packets was one bit.  Over several months of operation, more
   than 99% of the 40 byte TCP/IP headers were compressed down to one
   bit./54/

   ----------------------------
    53. The checksum was re-generated in the decompressor and, of course,
   the `toss' logic was made considerably more aggressive to prevent error
   propagation.
    54. We have heard the suggestion that `real-time' needs require
   abandoning TCP/IP in favor of a `light-weight' protocol with smaller
   headers.  It is difficult to envision a protocol that averages less than
   one header bit per packet.
   ----------------------------
*/
