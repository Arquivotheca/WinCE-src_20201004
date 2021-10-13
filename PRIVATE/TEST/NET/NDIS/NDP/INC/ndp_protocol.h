//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
