//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
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
    void LogResult(
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
        BOOL fMarkIt
        );
};

#endif // _RESULTLOG_H_
