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
#ifndef __send_h__
#define __send_h__
//#define MAX_PHYS_DESC               16

typedef struct _X500_RESERVED {

    // next packet in the chain of queued packets being allocated,
    // or waiting for the finish of transmission.
    //
    // We always keep the packet on a list so that in case the
    // the adapter is closing down or resetting, all the packets
    // can easily be located and "canceled".
    //
    PNDIS_PACKET Next;
} X500_RESERVED,*PX500_RESERVED;

#define PX500_RESERVED_FROM_PACKET(_Packet) ((PX500_RESERVED)((_Packet)->MiniportReserved))

#define EnqueuePacket(_Head, _Tail, _Packet)			\
{														\
    if (!_Head) {										\
		_Head = _Packet;								\
    } else {											\
		PX500_RESERVED_FROM_PACKET(_Tail)->Next = _Packet;\
	}													\
    PX500_RESERVED_FROM_PACKET(_Packet)->Next = NULL;	\
    _Tail = _Packet;									\
}

#define DequeuePacket(Head, Tail)                      \
{                                                      \
    PX500_RESERVED Reserved =                          \
    PX500_RESERVED_FROM_PACKET(Head);                  \
    if (!Reserved->Next) {                             \
		Tail = NULL;									\
    }                                                  \
    Head = Reserved->Next;                             \
}



#endif