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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef __NDP_PROTOCOL_H
#define __NDP_PROTOCOL_H

//------------------------------------------------------------------------------

#define NDP_TIMEOUT_DELAY        500
#define NDP_TIMEOUT_SHORT        2000
#define NDP_TIMEOUT_LONG         30000

//------------------------------------------------------------------------------

#define NDP_PACKET_NONE          0

#define NDP_PACKET_LOOKFOR       1
#define NDP_PACKET_OFFER         2
#define NDP_PACKET_ACCEPT        3
#define NDP_PACKET_OK            4
#define NDP_PACKET_PARAMS        5
#define NDP_PACKET_READY         6
#define NDP_PACKET_DONE          7
#define NDP_PACKET_RESULT        8
#define NDP_PACKET_NEXT          9
#define NDP_PACKET_EXIT          10

#define NDP_PACKET_STRESS        999

//------------------------------------------------------------------------------

typedef struct {
   DWORD time;
   DWORD idleTime;
   DWORD packetsReceived;
   DWORD bytesReceived;
} NDP_STRESS_RESULT;

typedef enum { SEND_THROUGHPUT=0, RECV_THROUGHPUT, PING_THROUGHPUT} teNDPTestType;

typedef struct {
   DWORD dwPoolSize;
   DWORD dwNDPTestType;
   DWORD dwPacketSize;
   DWORD dwNosOfPacketsToSend;
   DWORD dwFlagBurstControl;
   DWORD dwDelayInABurst;
   DWORD dwPacketsInABurst;
} tsNDPParams, * PtsNDPParams;

//------------------------------------------------------------------------------

#endif
