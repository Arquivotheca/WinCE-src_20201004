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
// testproc.h

#ifndef _TESTPROC_H_
#define _TESTPROC_H_

#include "iphlpapi.h"

// TCP test options
const DWORD TestOpt_NagleOff = 0x0001;
const DWORD TestCpuUsages = 0x0010;

// This is the time to wait before every iteration for UDP recv tests involving larger than
// MTU sized packets. A delay needs to be applied in case that the algorithm for fragmentation
// kicks in and all fragmented packets are dropped - the delay ensures that enough time has
// passed for CE OS to recover from the attack and begin receiving fragmented packets.
// This setting used to be 125000 ms, but with recent fixes, the timeout is no longer required.
const INT ReassemblyTimeoutMs = 0;

// Some common sizes
const INT MTUBytes = 1500;
const INT IPHeaderBytes = 20;
const INT UDPHeaderBytes = 8;

// Some sizes for using in Test procedures
#define KILO_BYTES 1000
#define MILLISEC_TO_SEC 0.001
// Test procedures
TESTPROCAPI TestTCPSendThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestTCPSendRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestUDPSendThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestUDPSendRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestUDPSendPacketLoss(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestTCPRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestUDPRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestUDPRecvPacketLoss(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestTCPPing(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestUDPPing(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestTCPCPUSendRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

void GetTCPStats(PMIB_TCPSTATS, PMIB_TCPSTATS);
void GetUDPStats(PMIB_UDPSTATS, PMIB_UDPSTATS);
void GetIPStats(PMIB_IPSTATS, PMIB_IPSTATS);
void PrintRelevantStats(PMIB_TCPSTATS , PMIB_UDPSTATS , PMIB_IPSTATS , PMIB_TCPSTATS , PMIB_UDPSTATS , PMIB_IPSTATS,
                        PMIB_TCPSTATS , PMIB_UDPSTATS , PMIB_IPSTATS , PMIB_TCPSTATS , PMIB_UDPSTATS , PMIB_IPSTATS);


#endif // _TESTPROC_H_

