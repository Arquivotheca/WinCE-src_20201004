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
#ifndef __queue_h__
#define __queue_h__

extern "C"{
	#include	<ndis.h>
}

typedef struct _NEXT {

    // next packet in the chain of queued packets being allocated,
    // or waiting for the finish of transmission.
    //
    // We always keep the packet on a list so that in case the
    // the adapter is closing down or resetting, all the packets
    // can easily be located and "canceled".
    //
    PNDIS_PACKET Next;
} NEXT,*PNEXT;

#define RESERVED(_Packet) ((PNEXT)((_Packet)->MiniportReserved))

#define EnqueuePacket(_Head, _Tail, _Packet)			\
{														\
    if (!_Head) {										\
		_Head = _Packet;								\
    } else {											\
		RESERVED(_Tail)->Next = _Packet;				\
	}													\
    RESERVED(_Packet)->Next = NULL;						\
    _Tail = _Packet;									\
}

#define DequeuePacket(Head, Tail)                      \
{                                                      \
    PNEXT Reserved =									\
    RESERVED(Head);										\
    if (!Reserved->Next) {                             \
		Tail = NULL;									\
    }                                                  \
    Head = Reserved->Next;                             \
}

//------

#define QueuePushHead(_L,_E)						\
    if (!((_E)->Link.Flink = (_L)->Link.Flink))		\
    {												\
        (_L)->Link.Blink = (PLIST_ENTRY)(_E);		\
    }												\
	(_L)->Link.Flink = (PLIST_ENTRY)(_E);



//-------------------------------------------------------------------------
// QueueRemoveHead -- Macro which removes the head of the head of queue.
//-------------------------------------------------------------------------
#define QueueRemoveHead(_L)									\
{															\
    PD100_LIST_ENTRY ListElem;								\
    if (ListElem = (PD100_LIST_ENTRY)(_L)->Link.Flink)		\
    {														\
            if(!((_L)->Link.Flink = ListElem->Link.Flink))  \
                (_L)->Link.Blink = (PLIST_ENTRY)0;			\
	}}															


//-------------------------------------------------------------------------
// QueuePutTail -- Macro which puts an element at the tail (end) of the queue.
//-------------------------------------------------------------------------
#define QueuePutTail(_L,_E)													\
	if ((_L)->Link.Blink)													\
    {																		\
        ((PD100_LIST_ENTRY)(_L)->Link.Blink)->Link.Flink = (PLIST_ENTRY)(_E); \
        (_L)->Link.Blink = (PLIST_ENTRY)(_E);								\
    }																		\
    else																	\
    {																		\
        (_L)->Link.Flink =													\
        (_L)->Link.Blink = (PLIST_ENTRY)(_E);								\
    }																		\
	(_E)->Link.Flink = (PLIST_ENTRY)0;


#define QueueInitList(_L) (_L)->Link.Flink = (_L)->Link.Blink = (PLIST_ENTRY)0;
#define QueueEmpty(_L)		(QueueGetHead((_L)) == (PD100_LIST_ENTRY)0)
#define QueueGetTail(_L)	((PD100_LIST_ENTRY)((_L)->Link.Blink))
#define QueueGetHead(_L)	((PD100_LIST_ENTRY)((_L)->Link.Flink))
//#define QueuePopHead(_L)	(PD100_LIST_ENTRY) (_L)->Link.Flink; QueueRemoveHead(_L)


#endif