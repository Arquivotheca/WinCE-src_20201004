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
#ifndef _RESULTLOG_H_
#define _RESULTLOG_H_

#include <windows.h>

#define SHOW_SEND_PKT_SIZE   (1)
#define SHOW_RECV_PKT_SIZE   (1<<1)
#define SHOW_BYTES_SENT      (1<<2)
#define SHOW_SEND_RATE       (1<<3)
#define HIGHLIGHT_SEND_RATE  (1<<4)     // Will show recv rate as part of perf log marker
#define SHOW_BYTES_RECVD     (1<<5)
#define SHOW_RECV_RATE       (1<<6)
#define HIGHLIGHT_RECV_RATE  (1<<7)     // Will show send rate as part of perf log marker
#define SHOW_LATENCY         (1<<8)
#define SHOW_JITTER          (1<<9)
#define SHOW_CPU_UTIL        (1<<10)
#define SHOW_PKT_LOSS        (1<<11)
// Below four values are introduced for TCP, UDP SendRecv Test
#define SHOW_SEND_PKT_LOSS   (1<<12)    // will show packet loss for send test
#define SHOW_RECV_PKT_LOSS   (1<<13)    // will show packet loss for recv test
#define SHOW_SEND_CPU_UTIL   (1<<14)    // will show CPU util for send test
#define SHOW_RECV_CPU_UTIL   (1<<15)    // will show CPU util for recv test

#define APPLY_TO_CLIENT(x)   x
#define APPLY_TO_SERVER(x)   (x<<16)

#define MAX_TEST_NAME_LEN 128

class ResultLog {
    TCHAR m_szTestName[MAX_TEST_NAME_LEN];
    DWORD m_dwTestOpt;
public:
    ResultLog(
        TCHAR *szTestName,
        DWORD dwTestOpt
        );
    void ShowHeader();
    // Below function introduced for TCP, UDP SendRecv Test
    void ShowHeaderSendRecv();
    BOOL LogResult(
        DWORD dwSendPacketSize, // in bytes
        DWORD dwRecvPacketSize, // in bytes
        // Send:
        DWORD dwPacketsSent,    // number of packets sent
        DWORD dwSendTime,       // send time (in milliseconds)
        // Recv:
        DWORD dwPacketsRecvd,   // number of packets recvd
        DWORD dwRecvTime,       // recv time (in milliseconds)
        // Latency:
        DWORD dwLatencyUs,
        // Jitter:
        DWORD dwJitterUs,
        // CPU utilization
        DWORD dwCpuIdle,        // in milliseconds
        DWORD dwTotalTime,      // in milliseconds
        // Mark it
        BOOL fMarkIt,
        double dConsumedSysCpu
        );
    // Below function introduced for TCP, UDP SendRecv Test
    BOOL LogResultSendRecv(
        DWORD dwSendPacketSize, // in bytes
        DWORD dwRecvPacketSize, // in bytes
        // Send:
        DWORD dwPacketsSent,    // number of packets sent
        DWORD dwSendTime,       // send time (in milliseconds)
        // Recv:
        DWORD dwPacketsRecvd,   // number of packets recvd
        DWORD dwRecvTime,       // recv time (in milliseconds)
        // Server Data:
        DWORD dwPacketsSentSrv,    // number of packets sent from server
        DWORD dwPacketsRecvdSrv,   // number of packets recvd at server
        // Latency:
        DWORD dwLatencyUs,
        // Jitter:
        DWORD dwJitterUs,
        // CPU utilization
        DWORD dwCpuIdle,        // in milliseconds
        DWORD dwTotalTime,      // in milliseconds
        // CPU utilization for Send
        DWORD dwCpuIdleSend,        // in milliseconds
        DWORD dwTotalTimeSend,      // in milliseconds
        // CPU utilization for Recv
        DWORD dwCpuIdleRecv,        // in milliseconds
        DWORD dwTotalTimeRecv,      // in milliseconds
        // Mark it
        BOOL fMarkIt
        );
};

#endif // _RESULTLOG_H_
