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
#ifndef __NDP_PROTOCOL_H
#define __NDP_PROTOCOL_H

#include "ndp_enum.h"

//------------------------------------------------------------------------------

#define NDP_TIMEOUT_DELAY        500
#define NDP_TIMEOUT_SHORT        2000
#define NDP_TIMEOUT_LONG         30000


//------------------------------------------------------------------------------

typedef struct {
   LONGLONG time;
   LONGLONG idleTime;
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
