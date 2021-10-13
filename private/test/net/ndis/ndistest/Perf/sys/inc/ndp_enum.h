//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef __NDP_ENUM_H
#define __NDP_ENUM_H

typedef enum {
	NDP_PACKET_NONE=0,
	NDP_PACKET_PEER=1,
	NDP_PACKET_OFFER=2,
	NDP_PACKET_ACCEPT=3,
	NDP_PACKET_OK=4,
	NDP_PACKET_PARAMS=5,
	NDP_PACKET_READY=6,
	NDP_PACKET_DONE=7,
	NDP_PACKET_GET_RESULT = 8,
	NDP_PACKET_RESULT=9,
	NDP_PACKET_NEXT=10,
	NDP_PACKET_EXIT=11,
	NDP_PACKET_STRESS=999
} teNDPPacketType;

#endif