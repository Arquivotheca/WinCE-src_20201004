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
// commprot.h
// Communication protocol between client and server

#ifndef _COMMPROT_H_
#define _COMMPROT_H_

#ifdef UNDER_CE
#ifdef SUPPORT_IPV6
#include <winsock2.h>
#else
#include <winsock.h>
#endif
#else
#include <winsock2.h>
#endif
#include <windows.h>
#include <tchar.h>
#include "commprot.h"
#include "tests.h"

const WORD COMMPROT_VERSION = 4;

typedef enum
{
    Test_Success       = 0,
    Test_Error         = 1, // Generic test/comminication link error
    Test_Version_Error = 2, // Client-server versions don't match
    Test_Comm_Timeout  = 3, // Client-server Communication link broken
    Test_Run_Timeout   = 4  // Timeout during test, client-server comm link up
} ServiceError;

extern const TCHAR* SERVICE_ERROR_STRING[];

struct ServiceRequest
{
    WORD version;
    TestType type;
    INT ip_version;
    INT protocol;
    WORD port;
    BOOL sock_nagle_off;
    int sock_send_buf;
    int sock_recv_buf;
    DWORD num_iterations;
    DWORD send_total_size;
    DWORD send_packet_size;
    DWORD recv_total_size;
    DWORD recv_packet_size;
    DWORD send_req_rate;
    DWORD recv_req_rate;
};

struct ServiceResponse
{
    WORD port;
    ServiceError error;
};

struct TestResponse
{
    TestResult* result;
    ServiceError error;
};

// Stage 1: Client requests server participation in the test
INT SendServiceRequest(SOCKET sock, ServiceRequest* preq, DWORD* pdwWSAError);
INT ReceiveServiceRequest(SOCKET sock, ServiceRequest* preq, DWORD* pdwWSAError);

// Stage 2: Server replies to client
INT SendServiceResponse(SOCKET sock, ServiceResponse* presp, DWORD* pdwWSAError);
INT ReceiveServiceResponse(SOCKET sock, ServiceResponse* presp, DWORD* pdwWSAError);

// Stage 3: Client and server sync up just before beginning to run test code
INT SendGoMsg(SOCKET sock, DWORD* pdwWSAError);
INT ReceiveGoMsg(SOCKET sock, DWORD* pdwWSAError);

// Stage 4: Server sends result of test data (captured at the server) to client
INT SendTestResponse(SOCKET sock, TestResponse* presp, DWORD* pdwWSAError);
INT ReceiveTestResponse(SOCKET sock, TestResponse* presp, DWORD* pdwWSAError);

#endif // _COMMPROT_H_