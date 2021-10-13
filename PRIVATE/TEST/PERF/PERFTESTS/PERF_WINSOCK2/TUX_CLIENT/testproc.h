//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
// testproc.h

#ifndef _TESTPROC_H_
#define _TESTPROC_H_

// TCP test options
const DWORD TestOpt_NagleOff = 0x0001;

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

// Test procedures
TESTPROCAPI TestTCPSendThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestUDPSendThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestUDPSendPacketLoss(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestTCPRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestUDPRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestUDPRecvPacketLoss(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestTCPPing(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI TestUDPPing(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

#endif // _TESTPROC_H_

