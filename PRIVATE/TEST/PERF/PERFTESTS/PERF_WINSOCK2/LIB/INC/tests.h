//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
// tests.h
// Definition of the tests

#ifndef _TESTS_H_
#define _TESTS_H_

#ifndef UNDER_CE
#define _WIN32_WINNT 0x0400
#endif
#ifdef SUPPORT_IPV6
#include <winsock2.h>
#else
#include <winsock.h>
#endif
#include "conntrk.h"
#include <windows.h>

typedef enum { Send, Receive } TestType;

struct TestResult
{
    DWORD total_time;       // total time for test
    DWORD total_cputil;     // total cpu utilization for test
    DWORD array_size;       // number of test iterations
    DWORD* time_array;      // time array per test iteration
    DWORD* cpuutil_array;   // cpu utilization per test iteration
    DWORD* sent_array;      // number of packets sent
    DWORD* recvd_array;     // number of packets received
    TestResult(): total_time(0), total_cputil(0), array_size(0),
                  time_array(NULL), cpuutil_array(NULL), sent_array(NULL),
                  recvd_array(NULL) {}
        
};

BOOL AllocTestResult(DWORD num, TestResult* result);
BOOL ClearTestResult(TestResult* result);
BOOL CopyTestResult(TestResult* result_dest, TestResult* result_src);
BOOL FreeTestResult(TestResult* result);

struct TestForm;
typedef struct TestForm* PTestForm;

INT TestInit(
    TestType test_type,
    INT protocol,
    DWORD num_iterations,
    DWORD send_total_size,  // total size of bytes to send per iteration
    DWORD send_packet_size, //  packet/buffer to use to send
    DWORD recv_total_size,  // total size of bytes to receive per iteration
    DWORD recv_packet_size, //  packets/buffer to use to receive
    BOOL  alt_cpu_mon,      // alternative CPU monitor when GetIdleTime is not implemented
    PTestForm* pptest_form
);

INT TestDeinit(
    PTestForm* pptest_form
);

INT Test(
    PTestForm ptest_form,   // TestForm pointer from TestInit
    SOCKET sock,            // Socket for the established connection
    DWORD send_req_rate,    // Requested send rate (bytes/second)
    DWORD recv_req_rate,    // Requested recv rate (bytes/second)
    CONNTRACK_HANDLE contrack_handle,
    TestResult* presult     // result structure that will be populated with result of the test
);

#endif // _TESTS_H_

