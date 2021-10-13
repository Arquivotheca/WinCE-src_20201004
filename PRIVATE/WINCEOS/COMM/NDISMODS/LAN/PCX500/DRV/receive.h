//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Copyright 2001, Cisco Systems, Inc.  All rights reserved.
// No part of this source, or the resulting binary files, may be reproduced,
// transmitted or redistributed in any form or by any means, electronic or
// mechanical, for any purpose, without the express written permission of Cisco.
//
//	
#ifndef __receive_h__
#define __receive_h__

#define ETHERNET_HEADER_SIZE	sizeof(ETH_HEADER_STRUC)

//#define QueueInitList(_L) (_L)->Link.Flink = (_L)->Link.Blink = (PLIST_ENTRY)0;

#pragma pack( push, struct_pack1 )
#pragma pack( 1 )

typedef struct _ETH_HEADER_STRUC {
    UCHAR       Destination[ETHERNET_ADDRESS_LENGTH];
    UCHAR       Source[ETHERNET_ADDRESS_LENGTH];
    USHORT		TypeLength;
} ETH_HEADER_STRUC, *PETH_HEADER_STRUC;

typedef struct _ETH_RX_BUFFER_STRUC {
    ETH_HEADER_STRUC    RxMacHeader;
    UCHAR               RxBufferData[(RX_BUF_SIZE - sizeof(ETH_HEADER_STRUC))];
} ETH_RX_BUFFER_STRUC, *PETH_RX_BUFFER_STRUC;
typedef ETH_RX_BUFFER_STRUC RFD_STRUC;

#ifndef UNDER_CE
typedef ETH_RX_BUFFER_STRUC* PRFD_STRUC;
#else
typedef ETH_RX_BUFFER_STRUC UNALIGNED* PRFD_STRUC;
#endif

/*
typedef struct _RFD_STRUC {
    ETH_RX_BUFFER_STRUC RfdBuffer;		// Data buffer in TCB
} RFD_STRUC, *PRFD_STRUC;
*/


#ifdef UNDER_CE
#pragma pack( pop, struct_pack1 )
#endif

typedef struct _X500Rcvd {

    D100_LIST_ENTRY     Link;
    PRFD_STRUC			Rfd;             // ptr to hardware RFD
    PNDIS_BUFFER        ReceiveBuffer;   // Pointer to Buffer
    PNDIS_PACKET        ReceivePacket;   // Pointer to Packet
    USHORT              Status;          // receive status (quick retrieval)
    UINT                FrameLength;     // total size of receive frame

} X500RFD, *PX500RFD;

#ifndef UNDER_CE
#pragma pack( pop, struct_pack1 )
#endif


#endif