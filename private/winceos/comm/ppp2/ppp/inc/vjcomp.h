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
*   @module vjcomp.h | Van Jacobsen TCP/IP Header Compression 
*
*   Date:   11-13-95
*
*/

#pragma once

// Packet Types 
//
// The 'ERROR' type never appears on the wire.  The receive framer uses 
// it to tell the decompressor there was a packet transmission error. 

#define TYPE_IP                 (0x40)
#define TYPE_UNCOMPRESSED_TCP   (0x70)
#define TYPE_COMPRESSED_TCP     (0x80)
#define TYPE_ERROR              (0x00)  

// Definitions and State Data

#define MAX_STATES              (16)        // must be >2 and <255
#define MAX_HDR                 (128)       // max TCP+IP hdr length 

// Active TCP Conversation State Data
//
// This is a copy of the entire IP/TCP header from the last packet 
// together with a small identifier the transmit & receive ends of the 
// line use to locate saved header.

typedef struct  cstate 
{
    struct cstate   *cs_next;           // next most recently used (tx only) 
    USHORT          cs_hlen;            // size of hdr (receive only) 
    BYTE            cs_id;              // state's connection # 
    BYTE            cs_filler;

    union 
    {
        struct IPHeader csu_ip;         // most recent ip/tcp hdr 
        char            hdr[ MAX_HDR ]; // size the union to max size
    } 
    slcs_u;

    // EEB - added for speed & convenience

    TCPHeader       *cs_tcp;            // points into slcs_u to tcp header
    BYTE            cs_ipLen;           // state's ip header length
    BYTE            cs_tcpLen;          // state's tcp header length
}
cstate_t;

#define cs_ip   slcs_u.csu_ip           // hide the union label

//
// Serial Line State Data
//

typedef struct slcompress 
{
    cstate_t    *last_cs;               // most recently used tstate 
    BYTE        last_recv;              // last rcvd conn. id 
    BYTE        last_xmit;              // last sent conn. id 
	BYTE        LastRxFrameBad;         // last received frame had errors
    BYTE        flags;
#define SLF_ENABLE_SLOTID_COMPRESSION_TX  (1<<1)
#define SLF_ENABLE_SLOTID_COMPRESSION_RX  (1<<2)

    BYTE        MaxStatesTx;            // Max states to use when sending
    BYTE        MaxStatesRx;            // Max states to use receiving
    cstate_t    tstate[ MAX_STATES ];   // xmit connection states 
    cstate_t    rstate[ MAX_STATES ];   // receive connection states 
}
slcompress_t;


// Function Prototypes

USHORT  
sl_compress_tcp( PNDIS_WAN_PACKET pPacket, slcompress_t *comp);

BOOL
sl_update_tcp(
	IN      BYTE         *bufp,
	IN      DWORD         len,
	IN OUT  slcompress_t *comp);

BYTE *
sl_uncompress_tcp( BYTE *bufp, DWORD *len, slcompress_t *comp );

void
sl_compress_init(
	OUT	slcompress_t *comp,
	IN  BYTE          MaxStatesRx,
	IN  BYTE          MaxStatesTx,
	IN  BOOL          CompressSlotIdsRx,
	IN  BOOL          CompressSlotIdsTx);

