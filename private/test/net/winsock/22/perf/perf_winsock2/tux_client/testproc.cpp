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
// testproc.cpp

#include "tuxmain.h"
#include <winbase.h>
#include <mstcpip.h>
#include "testproc.h"
#include "common.h"
#include "commprot.h"
#include "consetup.h"
#include "tests.h"
#include "loglib.h"
#include "resultlog.h"
#ifdef UNDER_CE
#include "profiler.h"
#endif

// --------------------------------------------------------------------
// Globals
//
// From tuxmain.cpp:
extern DWORD  g_dwNumberOfBuffers;
extern DWORD  g_dwNumberOfPingBuffers;
extern WORD   g_wUseIpVer;
extern TCHAR  g_tszServerAddressString[];
extern BOOL   g_fUserSelectedBuffer;
extern BOOL   g_fUserSelectedNumOfBuffers;
extern int*   g_aTestBufferSizes;
extern int    g_nTestBufferCount;
extern int*   g_aTestBufferSizesUdp;
extern int    g_nTestBufferCountUdp;
extern DWORD  g_dwReqSendRateKbps;
extern DWORD  g_dwReqRecvRateKbps;
extern BOOL   g_fGatewayTestOption;
extern int    g_iThreadPriority;
extern BOOL   g_fEnableMonteCarlo;
extern BOOL   g_fEnableAltCpuMon;
extern int    g_iUdpRcvQueueBytes;
extern BOOL   g_fKeepAliveControlChannel;
extern BOOL   g_fGatherIphlpapiStats;
extern BOOL   g_fHardBindToInterface;
extern TCHAR  *g_tszInterfaceToBind;
extern int g_nTestCpuUtilCount;
extern int* g_aTestCpuUtils;
extern BOOL g_fTestCpuUtilOptionSet;
extern DWORD g_dwMinTotalPacketsCpuUtil;
extern BOOL g_fConsumeCpu;

extern PFN_INITCONSUMECPU initConsumeCpu;
extern PFN_STARTCONSUMECPUTIME startConsumeCpuTime;
extern PFN_STOPCONSUMECPUTIME stopConsumeCpuTime;
extern PFN_GETCPUUSAGE getCpuUsage;
extern PFN_DEINITCONSUMECPU deinitConsumeCpu;
extern HINSTANCE hinstConsumeCpuLib; 

// --------------------------------------------------------------------
// Constants
//
const DWORD CONNTRACK_TIMEOUT_MS = TEST_TIMEOUT_MS;

// --------------------------------------------------------------------
// Local function prototypes
//
TESTPROCAPI RunTest(
    TestType param_TestType,
    INT param_Protocol,         // SOCK_STREAM or SOCK_DGRAM
    INT param_IpVer,            // 4 or 6
    BOOL param_SockNagleOff,    // Nagle off, FALSE will preserve default (= Nagle enabled)
    INT param_SockSendBuf,      // Send and
    INT param_SockRecvBuf,      //  Recv buffers for the socket, < 0 will preserve defaults
    INT param_NumIterats,
    INT param_SendTotal,
    INT param_SendPacket,
    INT param_RecvTotal,
    INT param_RecvPacket,
    INT param_SendReqRate,
    INT param_SendReqRateClient,
    INT param_RecvReqRate,
    INT param_RecvReqRateClient,
    DWORD param_cpu_util,    
    TestResult* param_ClientResult,
    TestResult* param_ServerResult,
    ServiceError* param_pError
);

// --------------------------------------------------------------------
TESTPROCAPI ProcessTuxMessages(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
// --------------------------------------------------------------------
{
    // process incoming tux messages
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // return the thread count that should be used to run this test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 1;
        return TPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }
    return TPR_PASS;
}

// --------------------------------------------------------------------
TESTPROCAPI TestTCPSendThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    INT rateResult = TPR_PASS;
    INT iTestCpuUtilCount;
    double consumedSysCpu = 0;

    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    Log(DEBUG_MSG, TEXT("+TestTCPSendThroughput()"));
    FileAddNewTestBreak(TEXT("TCP Send Throughput"));

    ResultLog result(
        (TCHAR*)lpFTE->lpDescription,
        APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_BYTES_SENT) |
        APPLY_TO_CLIENT(SHOW_SEND_RATE) |
        APPLY_TO_CLIENT(SHOW_CPU_UTIL)
        );

    TestResult result_Client, result_Server;
    if (!AllocTestResult(1, &result_Client)) goto Cleanup;
    if (!AllocTestResult(1, &result_Server)) goto Cleanup;

    result.ShowHeader();

    // When running the TCP CPU util tests (2001-2004) or -o command line option,
    // the minimum number of buffers should be at least the default number of buffers
    // (10,000) for getIdleTime() to increment properly
    if((lpFTE->dwUserData & TestCpuUsages) || g_fTestCpuUtilOptionSet)
    {
        // When running the TCP test all CPU util tests (2001-2004), -o and -e command line options can't be used
        if(((lpFTE->dwUserData & TestCpuUsages) && g_fConsumeCpu)
            || ((lpFTE->dwUserData & TestCpuUsages) && g_fTestCpuUtilOptionSet))
        {
            Log(REQUIRED_MSG, TEXT("-e and -o options are not applicable to the test"));
            nResult = TPR_SKIP;
            goto Cleanup;
        }

        if(g_fTestCpuUtilOptionSet && g_fConsumeCpu)
        {
            Log(REQUIRED_MSG, TEXT("-o and -e options can't be used at the same time for the test"));
            Log(REQUIRED_MSG, TEXT("Skipping the test..."));
            nResult = TPR_SKIP;
            goto Cleanup;
        }

        if(g_dwReqSendRateKbps !=0 || g_dwReqRecvRateKbps != 0)
        {
            Log(ERROR_MSG, TEXT("Do not specify send/recv rates when running cpu utilization tests"), g_dwMinTotalPacketsCpuUtil);
            goto Cleanup;
        }

        if(g_dwNumberOfBuffers < g_dwMinTotalPacketsCpuUtil)
        {
            Log(ERROR_MSG, TEXT("Total packets should at least %u (default) when running cpu utilization tests"), g_dwMinTotalPacketsCpuUtil);
            goto Cleanup;
        }
        iTestCpuUtilCount = g_nTestCpuUtilCount;
    }
    else
    {
        // TestCpuUsages not set and no -o option specified
        iTestCpuUtilCount = 0;
    }


    INT k=0;
    do
    {
        DWORD cpuUtil;

        // cpuUtil is not used when TestCpuUsages is not set and when -o is not used
        cpuUtil = (iTestCpuUtilCount == 0)? 0: g_aTestCpuUtils[k];

        for (INT i=0; i<g_nTestBufferCount; i+=1)
        {
            DWORD bufsize = g_aTestBufferSizes[i];
            DWORD numbufs = g_dwNumberOfBuffers;

            ServiceError err;

            // record start time if g_fConsumeCpu is enabled
            if(g_fConsumeCpu)
            {
                if(startConsumeCpuTime() == FALSE)
                {
                    nResult = TPR_FAIL;
                    goto Cleanup;
                }
            }

            // run the test
            nResult = RunTest(
                Send,           // Test type
                SOCK_STREAM,    // SOCK_STREAM or SOCK_DGRAM
                g_wUseIpVer,    // IP version
                lpFTE->dwUserData & TestOpt_NagleOff, // Disable Nagle setting (FALSE leaves Nagle on)
                128*1024,             // Socket send buffer (128KB)
                128*1024,             // Socket recv buffer (128KB)
                1,              // Number of iterations
                bufsize * numbufs,  // Send total (bytes)
                bufsize,            // Send packet/buffer size (bytes)
                1,              // Receive total (bytes)
                1,              // Receive packet/buffer size (bytes)
                0,                                  // Requested send rate (bytes/s); note if 0 will go max
                (g_dwReqSendRateKbps * 1000 / 8),   // Requested send rate at client (bytes/s)
                (g_dwReqRecvRateKbps * 1000 / 8),   // Requested recv rate (bytes/s)
                0,                                  // Requested recv rate at client (bytes/s)
                cpuUtil,       
                &result_Client,     // Client result array
                &result_Server,     // Server result array
                &err
            );

            // calculate the cpu usage of the spinning thread at the end of test
            // if g_fConsumeCpu is set
            if(g_fConsumeCpu)
            {
                if(stopConsumeCpuTime() == FALSE)
                {
                    nResult = TPR_FAIL;
                    goto Cleanup;
                }
                consumedSysCpu = getCpuUsage();
            }

            // To avoid any problems with divide-by-zero
            if (result_Client.time_array[0] < 1) result_Client.time_array[0] = 1;
            if (result_Server.time_array[0] < 1) result_Server.time_array[0] = 1;

            if (nResult == TPR_PASS && err == Test_Success)
            {
                Log(DEBUG_MSG, TEXT("Buf %ld: TCP send(), MAX rate of %ld Kbytes/s"),
                    bufsize, bufsize * result_Server.recvd_array[0] / result_Server.time_array[0]);
                Log(DEBUG_MSG, TEXT("  Client time: %d ms => %ld Kbytes/s"),
                    result_Client.time_array[0],
                    bufsize * numbufs / result_Client.time_array[0]);
                Log(DEBUG_MSG, TEXT("  Server time: %d ms => %ld Kbytes/s"),
                    result_Server.time_array[0],
                    bufsize * numbufs / result_Server.time_array[0]);

                if(!result.LogResult(
                    bufsize,                        // Send size (in bytes)
                    0,                              // Recv size (in bytes)
                    result_Client.sent_array[0],    // Number of packets sent
                    result_Client.time_array[0],    // Send time (in milliseconds)
                    0,                              // Number of packets recv'd
                    0,                              // Recv time (in milliseconds)
                    0,                              // Latency (us)
                    0,                              // Jitter (us)
                    result_Client.cpuutil_array[0], // Idle time (ms)
                    result_Client.time_array[0],    // Total test time (ms)
                    TRUE,                           // Mark it
                    consumedSysCpu                  // CPU usage of the spinning thread
                    ))
                {
                    rateResult = TPR_FAIL;
                }
            }
            else
            {
                Log(ERROR_MSG, TEXT("TestTCPSendThroughput test Failed"));
                nResult = TPR_FAIL;
                goto Cleanup;
            }
        }
        ++k;
    } while(k < iTestCpuUtilCount);
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);

    Log(DEBUG_MSG, TEXT("-TestTCPSendThroughput()"));

    if(rateResult == TPR_PASS)
       return nResult;
    else
      return rateResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestTCPSendRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst
    INT nTestBufferCount = 0;
    INT nRateResult = TPR_PASS;
    int* aTestBufferSizes = NULL;
    if(g_fConsumeCpu || g_fTestCpuUtilOptionSet)
    {
        Log(REQUIRED_MSG, TEXT("-e and -o options are not applicable to the test"));
        Log(REQUIRED_MSG, TEXT("Skipping the test..."));
        return TPR_SKIP;
    }

    Log(DEBUG_MSG, TEXT("+TestTCPSendRecvThroughput()"));
    FileAddNewTestBreak(TEXT("TCP SendRecv Throughput"));

    ResultLog result(
        (TCHAR*)lpFTE->lpDescription,
        APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_SEND_RATE) |
        APPLY_TO_CLIENT(SHOW_SEND_CPU_UTIL) |
        APPLY_TO_CLIENT(SHOW_RECV_RATE) |
        APPLY_TO_CLIENT(SHOW_RECV_CPU_UTIL)
        );

    TestResult result_Client, result_Client_Send, result_Server, result_Server_Send;
    if (!AllocTestResult(1, &result_Client)) goto Cleanup;
    if (!AllocTestResult(1, &result_Client_Send)) goto Cleanup;
    if (!AllocTestResult(1, &result_Server)) goto Cleanup;
    if (!AllocTestResult(1, &result_Server_Send)) goto Cleanup;

    result.ShowHeaderSendRecv();

    if (!g_fUserSelectedBuffer)
    {
        INT aTestBufs[]
                = {16, 32, 64, 128, 256, 512, 1024, 1460, 2048, 2920, 4096, 8192, 16384, 32768, 65536};

        nTestBufferCount = sizeof(aTestBufs)/sizeof(INT);
        if (aTestBufferSizes != NULL) delete [] aTestBufferSizes; // de-allocate memory allocated earlier
        aTestBufferSizes = new INT[nTestBufferCount]; //allocate memory now specific to this test

        if (aTestBufferSizes == NULL)
        {
            Log(ERROR_MSG, TEXT("Memory allocation failed in TCP SendRecvThroughput Test, Terminating test"));
            nResult = TPR_FAIL;
            goto Cleanup;
        }
        memcpy(aTestBufferSizes, aTestBufs, nTestBufferCount * sizeof(INT));
    }
    else
    {
        nTestBufferCount = g_nTestBufferCount;
        if (aTestBufferSizes != NULL) delete [] aTestBufferSizes; // de-allocate memory allocated earlier
        aTestBufferSizes = new INT[nTestBufferCount]; //allocate memory now specific to this test

        if (aTestBufferSizes == NULL)
        {
            Log(ERROR_MSG, TEXT("Memory allocation failed in TCP SendRecvThroughput Test, Terminating test"));
            nResult = TPR_FAIL;
            goto Cleanup;
        }
        memcpy(aTestBufferSizes, g_aTestBufferSizes, nTestBufferCount * sizeof(INT));
    }

    for (INT nBufferIndex = 0; nBufferIndex < nTestBufferCount; nBufferIndex++)
    {
        DWORD dwBufSize = aTestBufferSizes[nBufferIndex];
        DWORD dwNumBufs = 0;
        if (g_fUserSelectedNumOfBuffers)
        {
            dwNumBufs = g_dwNumberOfBuffers;
        }
        else
        {
            dwNumBufs = (128 * KILO_BYTES / dwBufSize);  //to send/recv 128 kbytes data for each packet size
        }

        ServiceError err;

        // run the Send test
        nResult = RunTest(
            Send,                                   // Test type
            SOCK_STREAM,                            // SOCK_STREAM or SOCK_DGRAM
            g_wUseIpVer,                            // IP version
            lpFTE->dwUserData & TestOpt_NagleOff,   // Disable Nagle setting (FALSE leaves Nagle on)
            128*1024,                               // Socket send buffer (128KB)
            128*1024,                               // Socket recv buffer (128KB)
            1,                                      // Number of iterations
            dwBufSize * dwNumBufs,                  // Send total (bytes)
            dwBufSize,                              // Send packet/buffer size (bytes)
            1,                                      // Receive total (bytes)
            1,                                      // Receive packet/buffer size (bytes)
            0,                                      // Requested send rate (bytes/s); note if 0 will go max
            (g_dwReqSendRateKbps * KILO_BYTES / 8), // Requested send rate at client (bytes/s)
            (g_dwReqRecvRateKbps * KILO_BYTES / 8), // Requested recv rate (bytes/s)
            0,                                      // Requested recv rate at client (bytes/s)
            0,                  // CPU usage parameter is not used
            &result_Client,                         // Client result array
            &result_Server,                         // Server result array
            &err
        );

        if ((nResult != TPR_PASS) && (err != Test_Success))
        {
            Log (ERROR_MSG, TEXT("TestTCPSendRecvThroughput test failed, Failed in Send"));
            nResult = TPR_FAIL;
            goto Cleanup;
        }

        // To avoid any problems with divide-by-zero
        if (result_Client.time_array[0] < 1) result_Client.time_array[0] = 1;
        if (result_Server.time_array[0] < 1) result_Server.time_array[0] = 1;

        Log(DEBUG_MSG, TEXT("Buf %ld: TCP send(), MAX rate of %ld KBytes/s"),
            dwBufSize,
            (double)(dwBufSize * result_Server.recvd_array[0]) / (double)(KILO_BYTES * result_Server.time_array[0] * MILLISEC_TO_SEC));
        Log(DEBUG_MSG, TEXT("  Client time: %d ms => %ld KBytes/s"),
            result_Client.time_array[0],
            (double)(dwBufSize * dwNumBufs) / (double)(KILO_BYTES * result_Client.time_array[0] * MILLISEC_TO_SEC));
        Log(DEBUG_MSG, TEXT("  Server time: %d ms => %ld KBytes/s"),
            result_Server.time_array[0],
            (double)(dwBufSize * dwNumBufs) / (double)(KILO_BYTES * result_Server.time_array[0] * MILLISEC_TO_SEC));

        // copy the results from send test and execute receive test
        CopyTestResult(&result_Client_Send, &result_Client);
        CopyTestResult(&result_Server_Send, &result_Server);

        // run the Receive test
        nResult = RunTest(
            Send,                                   // Test type
            SOCK_STREAM,                            // SOCK_STREAM or SOCK_DGRAM
            g_wUseIpVer,                            // IP version
            lpFTE->dwUserData & TestOpt_NagleOff,   // Disable Nagle setting (FALSE leaves Nagle on)
            128*1024,                               // Socket send buffer (128KB)
            128*1024,                               // Socket recv buffer (128KB)
            1,                                      // Number of iterations
            1,                                      // Send total (bytes)
            1,                                      // Send packet/buffer size (bytes)
            dwBufSize * dwNumBufs,                  // Receive total (bytes)
            dwBufSize,                              // Receive packet/buffer size (bytes)
            (g_dwReqSendRateKbps * KILO_BYTES / 8), // Requested send rate (bytes/s); note if 0 will go max
            0,                                      // Requested send rate at client (bytes/s)
            0,                                      // Requested recv rate (bytes/s)
            (g_dwReqRecvRateKbps * KILO_BYTES / 8), // Requested recv rate at client (bytes/s)
            0,                  // CPU usage parameter is not used
            &result_Client,                         // Client result array
            &result_Server,                         // Server result array
            &err
        );

        if ((nResult != TPR_PASS) && (err != Test_Success))
        {
            Log(ERROR_MSG, TEXT("TestTCPSendRecvThroughput test failed, Failed in Recv"));
            nResult = TPR_FAIL;
            goto Cleanup;
        }

        // To avoid any problems with divide-by-zero
        if (result_Client.time_array[0] < 1) result_Client.time_array[0] = 1;
        if (result_Server.time_array[0] < 1) result_Server.time_array[0] = 1;

        Log(DEBUG_MSG, TEXT("Buf %ld: TCP recv(), MAX rate of %11.2f Kbytes/s"),
            dwBufSize,
            (double)(dwBufSize * result_Client.recvd_array[0]) / (double)(KILO_BYTES * result_Client.time_array[0] * MILLISEC_TO_SEC));
        Log(DEBUG_MSG, TEXT("  Client time: %d ms => %11.2f Kbytes/s"),
            result_Client.time_array[0],
            (double)(dwBufSize * dwNumBufs) / (double)(KILO_BYTES * result_Client.time_array[0] * MILLISEC_TO_SEC));
        Log(DEBUG_MSG, TEXT("  Server time: %d ms => %11.2f Kbytes/s"),
            result_Server.time_array[0],
            (double)(dwBufSize * dwNumBufs) / (double)(KILO_BYTES * result_Server.time_array[0] * MILLISEC_TO_SEC));

        if(!result.LogResultSendRecv(
            dwBufSize,                           // Send size (in bytes)
            dwBufSize,                           // Recv size (in bytes)
            result_Client_Send.sent_array[0],    // Number of packets sent
            result_Client_Send.time_array[0],    // Send time (in milliseconds)
            result_Client.recvd_array[0],        // Number of packets recv'd
            result_Client.time_array[0],         // Recv time (in milliseconds)
            result_Server.sent_array[0],         // Number of packets sent from server
            result_Server_Send.recvd_array[0],   // Number of packets recv'd at server
            0,                                   // Latency (us)
            0,                                   // Jitter (us)
            0,                                   // Idle time (ms)
            0,                                   // Total test time (ms)
            result_Client_Send.cpuutil_array[0], // Send Test Idle time (ms)
            result_Client_Send.time_array[0],    // Total Send test time (ms)
            result_Client.cpuutil_array[0],      // Idle time (ms)
            result_Client.time_array[0],         // Total test time (ms)
            TRUE                                 // Mark it
            ))
            {
                nRateResult = TPR_FAIL;
            }
    }
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Client_Send);
    FreeTestResult(&result_Server);
    FreeTestResult(&result_Server_Send);

    //De-allocating memory for buffer sizes array
    if (aTestBufferSizes != NULL) delete [] aTestBufferSizes;

    Log(DEBUG_MSG, TEXT("-TestTCPSendRecvThroughput()"));

    if(nRateResult == TPR_PASS)
       return nResult;
    else
      return nRateResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestTCPRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    
    INT rateResult = TPR_PASS;
    INT iTestCpuUtilCount;
    double consumedSysCpu = 0;

    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    Log(DEBUG_MSG, TEXT("+TestTCPRecvThroughput()"));
    FileAddNewTestBreak(TEXT("TCP Recv Throughput"));

    ResultLog result(
        (TCHAR*)lpFTE->lpDescription,
        APPLY_TO_CLIENT(SHOW_RECV_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_BYTES_RECVD) |
        APPLY_TO_CLIENT(SHOW_RECV_RATE) |
        APPLY_TO_CLIENT(SHOW_CPU_UTIL)
        );

    TestResult result_Client, result_Server;
    if (!AllocTestResult(1, &result_Client)) goto Cleanup;
    if (!AllocTestResult(1, &result_Server)) goto Cleanup;

    result.ShowHeader();

    // When running the TCP CPU util tests (2001-2004) or -o command line option,
    // the minimum number of buffers should be at least the default number of buffers
    // (10,000) for getIdleTime() to increment properly
    if((lpFTE->dwUserData & TestCpuUsages) || g_fTestCpuUtilOptionSet)
    {
        // When running the TCP test all CPU util tests (2001-2004), -o and -e command line options can't be used
        if(((lpFTE->dwUserData & TestCpuUsages) && g_fConsumeCpu)
            || ((lpFTE->dwUserData & TestCpuUsages) && g_fTestCpuUtilOptionSet))
        {
            Log(REQUIRED_MSG, TEXT("-e and -o options are not applicable to the test"));
            nResult = TPR_SKIP;
            goto Cleanup;
        }

        if(g_fTestCpuUtilOptionSet && g_fConsumeCpu)
        {
            Log(REQUIRED_MSG, TEXT("-o and -e options can't be used at the same time for the test"));
            Log(REQUIRED_MSG, TEXT("Skipping the test..."));
            nResult = TPR_SKIP;
            goto Cleanup;
        }

        if(g_dwReqSendRateKbps !=0 || g_dwReqRecvRateKbps != 0)
        {
            Log(ERROR_MSG, TEXT("Do not specify send/recv rates when running cpu utilization tests"), g_dwMinTotalPacketsCpuUtil);
            goto Cleanup;
        }

        if(g_dwNumberOfBuffers < g_dwMinTotalPacketsCpuUtil)
        {
            Log(ERROR_MSG, TEXT("Total packets should at least %u (default) when running cpu utilization tests"), g_dwMinTotalPacketsCpuUtil);
            goto Cleanup;
        }
        iTestCpuUtilCount = g_nTestCpuUtilCount;
    }
    else
    {
        // TestCpuUsages not set and no -o option specified
        iTestCpuUtilCount = 0;
    }

    INT k=0;
    do
    {
        DWORD cpuUtil;

        // cpuUtil is not used when TestCpuUsages is not set and when -o is not used
        cpuUtil = (iTestCpuUtilCount == 0)? 0: g_aTestCpuUtils[k];

        for (INT i=0; i<g_nTestBufferCount; i+=1)
        {
            DWORD bufsize = g_aTestBufferSizes[i];
            DWORD numbufs = g_dwNumberOfBuffers;

            ServiceError err;

            if(g_fConsumeCpu)
            {
                if(startConsumeCpuTime() == FALSE)
                {
                    nResult = TPR_FAIL;
                    goto Cleanup;
                }
            }

            // run the test
            nResult = RunTest(
                Send,           // Test type
                SOCK_STREAM,    // SOCK_STREAM or SOCK_DGRAM
                g_wUseIpVer,    // IP version
                lpFTE->dwUserData & TestOpt_NagleOff, // Disable Nagle setting (FALSE leaves Nagle on)
                128*1024,             // Socket send buffer (128KB)
                128*1024,             // Socket recv buffer (128KB)
                1,              // Number of iterations
                1,              // Send total (bytes)
                1,              // Send packet/buffer size (bytes)
                bufsize * numbufs,  // Receive total (bytes)
                bufsize,            // Receive packet/buffer size (bytes)
                (g_dwReqSendRateKbps * 1000 / 8),   // Requested send rate (bytes/s); note if 0 will go max
                0,                                  // Requested send rate at client (bytes/s)
                0,                                  // Requested recv rate (bytes/s)
                (g_dwReqRecvRateKbps * 1000 / 8),   // Requested recv rate at client (bytes/s)
                cpuUtil,          
                &result_Client,  // Client result array
                &result_Server,  // Server result array
                &err
            );

            // calculate the cpu usage of the spinning thread at the end of test
            // if g_fConsumeCpu is set
            if(g_fConsumeCpu)
            {
                if(stopConsumeCpuTime() == FALSE)
                {
                    nResult = TPR_FAIL;
                    goto Cleanup;
                }
                consumedSysCpu = getCpuUsage();
            }

            // To avoid any problems with divide-by-zero
            if (result_Client.time_array[0] < 1) result_Client.time_array[0] = 1;
            if (result_Server.time_array[0] < 1) result_Server.time_array[0] = 1;

            if (nResult == TPR_PASS && err == Test_Success)
            {
                Log(DEBUG_MSG, TEXT("Buf %ld: TCP recv(), MAX rate of %ld Kbytes/s"),
                    bufsize, bufsize * result_Client.recvd_array[0] / result_Client.time_array[0]);
                Log(DEBUG_MSG, TEXT("  Client time: %d ms => %ld Kbytes/s"),
                    result_Client.time_array[0],
                    bufsize * numbufs / result_Client.time_array[0]);
                Log(DEBUG_MSG, TEXT("  Server time: %d ms => %ld Kbytes/s"),
                    result_Server.time_array[0],
                    bufsize * numbufs / result_Server.time_array[0]);

                if(!result.LogResult(
                    0,                          // Send size (in bytes)
                    bufsize,                    // Recv size (in bytes)
                    0,                          // Number of packets sent
                    0,                          // Send time (in milliseconds)
                    result_Client.recvd_array[0],   // Number of packets recv'd
                    result_Client.time_array[0],    // Recv time (in milliseconds)
                    0,                          // Latency (us)
                    0,                          // Jitter (us)
                    result_Client.cpuutil_array[0], // Idle time (ms)
                    result_Client.time_array[0],    // Total test time (ms)
                    TRUE,                           // Mark it
                    consumedSysCpu                  // CPU usage of the spinning thread
                    ))
                {
                    rateResult = TPR_FAIL;
                }
            }
            else
            {
                Log(ERROR_MSG, TEXT("TestTCPRecvThroughput test Failed"));
                nResult = TPR_FAIL;
                goto Cleanup;
            }
        } // end for (INT i=0; i<g_nTestBufferCount; i+=1)
        ++k;
    } while(k < iTestCpuUtilCount);
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);

    Log(DEBUG_MSG, TEXT("-TestTCPRecvThroughput()"));

    if(rateResult == TPR_PASS)
       return nResult;
    else
      return rateResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestTCPCPUSendRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    INT nRateResult = TPR_PASS;
    INT iTestCpuUtilCount;

    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    if(g_fConsumeCpu || g_fTestCpuUtilOptionSet)
    {
        Log(REQUIRED_MSG, TEXT("-e and -o options are not applicable to the test"));
        Log(REQUIRED_MSG, TEXT("Skipping the test..."));
        return TPR_SKIP;
    }

    Log(DEBUG_MSG, TEXT("+TestTCPSendRecvThroughput()"));
    FileAddNewTestBreak(TEXT("TCP SendRecv Throughput"));
    
    ResultLog result(
        (TCHAR*)lpFTE->lpDescription,
        APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_SEND_RATE) |
        APPLY_TO_CLIENT(SHOW_SEND_CPU_UTIL) |
        APPLY_TO_CLIENT(SHOW_RECV_RATE) |
        APPLY_TO_CLIENT(SHOW_RECV_CPU_UTIL)
        );


    TestResult result_Client, result_Client_Send, result_Server, result_Server_Send;
    if (!AllocTestResult(1, &result_Client)) goto Cleanup;
    if (!AllocTestResult(1, &result_Client_Send)) goto Cleanup;
    if (!AllocTestResult(1, &result_Server)) goto Cleanup;
    if (!AllocTestResult(1, &result_Server_Send)) goto Cleanup;

    result.ShowHeaderSendRecv();

    if(g_dwReqSendRateKbps !=0 || g_dwReqRecvRateKbps != 0)
    {
        Log(ERROR_MSG, TEXT("Do not specify send/recv rates when running cpu utilization tests"), g_dwMinTotalPacketsCpuUtil);
        goto Cleanup;
    }

    if(g_dwNumberOfBuffers < g_dwMinTotalPacketsCpuUtil)
    {
        Log(ERROR_MSG, TEXT("Total packets should at least %u (default) when running cpu utilization tests"), g_dwMinTotalPacketsCpuUtil);
        goto Cleanup;
    }
    iTestCpuUtilCount = g_nTestCpuUtilCount;

    INT k=0;
    do
    {
        DWORD cpuUtil;

        // cpuUtil is not used when TestCpuUsages is not set and when -o is not used
        cpuUtil = (iTestCpuUtilCount == 0)? 0: g_aTestCpuUtils[k];

        for (INT i=0; i<g_nTestBufferCount; i+=1)
        {
            DWORD bufsize = g_aTestBufferSizes[i];
            DWORD numbufs = g_dwNumberOfBuffers;
            ServiceError err;

            // run the Send test
            nResult = RunTest(
                Send,                                   // Test type
                SOCK_STREAM,                            // SOCK_STREAM or SOCK_DGRAM
                g_wUseIpVer,                            // IP version
                lpFTE->dwUserData & TestOpt_NagleOff,   // Disable Nagle setting (FALSE leaves Nagle on)
                128*1024,                               // Socket send buffer (128KB)
                128*1024,                               // Socket recv buffer (128KB)
                1,                                      // Number of iterations
                bufsize * numbufs,                      // Send total (bytes)
                bufsize,                                // Send packet/buffer size (bytes)
                1,                                      // Receive total (bytes)
                1,                                      // Receive packet/buffer size (bytes)
                0,                                      // Requested send rate (bytes/s); note if 0 will go max
                (g_dwReqSendRateKbps * KILO_BYTES / 8), // Requested send rate at client (bytes/s)
                (g_dwReqRecvRateKbps * KILO_BYTES / 8), // Requested recv rate (bytes/s)
                0,                                      // Requested recv rate at client (bytes/s)
                cpuUtil,
                &result_Client,                         // Client result array
                &result_Server,                         // Server result array
                &err
            );

            if ((nResult != TPR_PASS) && (err != Test_Success))
            {
                Log (ERROR_MSG, TEXT("Test failed in Send Test, terminating test"));
                nResult = TPR_FAIL;
                goto Cleanup;
            }

            // To avoid any problems with divide-by-zero
            if (result_Client.time_array[0] < 1) result_Client.time_array[0] = 1;
            if (result_Server.time_array[0] < 1) result_Server.time_array[0] = 1;


            Log(DEBUG_MSG, TEXT("Buf %ld: TCP send(), MAX rate of %ld KBytes/s"),
                bufsize,
                (double)(bufsize * result_Server.recvd_array[0]) / (double)(KILO_BYTES * result_Server.time_array[0] * MILLISEC_TO_SEC));
            Log(DEBUG_MSG, TEXT("  Client time: %d ms => %ld KBytes/s"),
                result_Client.time_array[0],
                (double)(bufsize * numbufs) / (double)(KILO_BYTES * result_Client.time_array[0] * MILLISEC_TO_SEC));
            Log(DEBUG_MSG, TEXT("  Server time: %d ms => %ld KBytes/s"),
                result_Server.time_array[0],
                (double)(bufsize * numbufs) / (double)(KILO_BYTES * result_Server.time_array[0] * MILLISEC_TO_SEC));

            // copy the results from send test and execute receive test
            CopyTestResult(&result_Client_Send, &result_Client);
            CopyTestResult(&result_Server_Send, &result_Server);

            // run the Receive test
            nResult = RunTest(
                Send,                                   // Test type
                SOCK_STREAM,                            // SOCK_STREAM or SOCK_DGRAM
                g_wUseIpVer,                            // IP version
                lpFTE->dwUserData & TestOpt_NagleOff,   // Disable Nagle setting (FALSE leaves Nagle on)
                -1,                                     // Socket send buffer (< 0 leaves at default)
                -1,                                     // Socket recv buffer (< 0 leaves at default)
                1,                                      // Number of iterations
                1,                                      // Send total (bytes)
                1,                                      // Send packet/buffer size (bytes)
                bufsize * numbufs,                  // Receive total (bytes)
                bufsize,                              // Receive packet/buffer size (bytes)
                (g_dwReqSendRateKbps * KILO_BYTES / 8), // Requested send rate (bytes/s); note if 0 will go max
                0,                                      // Requested send rate at client (bytes/s)
                0,                                      // Requested recv rate (bytes/s)
                (g_dwReqRecvRateKbps * KILO_BYTES / 8), // Requested recv rate at client (bytes/s)
                cpuUtil,
                &result_Client,                         // Client result array
                &result_Server,                         // Server result array
                &err
            );

            if ((nResult != TPR_PASS) && (err != Test_Success))
            {
                Log(ERROR_MSG, TEXT("Test failed in Recv Test; terminating test"));
                nResult = TPR_FAIL;
                goto Cleanup;
            }

            // To avoid any problems with divide-by-zero
            if (result_Client.time_array[0] < 1) result_Client.time_array[0] = 1;
            if (result_Server.time_array[0] < 1) result_Server.time_array[0] = 1;

            Log(DEBUG_MSG, TEXT("Buf %ld: TCP recv(), MAX rate of %11.2f Kbytes/s"),
                bufsize,
                (double)(bufsize * result_Client.recvd_array[0]) / (double)(KILO_BYTES * result_Client.time_array[0] * MILLISEC_TO_SEC));
            Log(DEBUG_MSG, TEXT("  Client time: %d ms => %11.2f Kbytes/s"),
                result_Client.time_array[0],
                (double)(bufsize * numbufs) / (double)(KILO_BYTES * result_Client.time_array[0] * MILLISEC_TO_SEC));
            Log(DEBUG_MSG, TEXT("  Server time: %d ms => %11.2f Kbytes/s"),
                result_Server.time_array[0],
                (double)(bufsize * numbufs) / (double)(KILO_BYTES * result_Server.time_array[0] * MILLISEC_TO_SEC));

            if(!result.LogResultSendRecv(
                bufsize,                           // Send size (in bytes)
                bufsize,                           // Recv size (in bytes)
                result_Client_Send.sent_array[0],    // Number of packets sent
                result_Client_Send.time_array[0],    // Send time (in milliseconds)
                result_Client.recvd_array[0],        // Number of packets recv'd
                result_Client.time_array[0],         // Recv time (in milliseconds)
                result_Server.sent_array[0],         // Number of packets sent from server
                result_Server_Send.recvd_array[0],   // Number of packets recv'd at server
                0,                                   // Latency (us)
                0,                                   // Jitter (us)
                0,                                   // Idle time (ms)
                0,                                   // Total test time (ms)
                result_Client_Send.cpuutil_array[0], // Send Test Idle time (ms)
                result_Client_Send.time_array[0],    // Total Send test time (ms)
                result_Client.cpuutil_array[0],      // Idle time (ms)
                result_Client.time_array[0],         // Total test time (ms)
                TRUE                                 // Mark it
                ))
                {
                    nRateResult = TPR_FAIL;
                }
        } // end for (INT i=0; i<g_nTestBufferCount; i+=1)
        ++k;
    } while(k < iTestCpuUtilCount);
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Client_Send);
    FreeTestResult(&result_Server);
    FreeTestResult(&result_Server_Send);

    Log(DEBUG_MSG, TEXT("-TestTCPSendRecvThroughput()"));

    if(nRateResult == TPR_PASS)
       return nResult;
    else
      return nRateResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestTCPPing(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    if(g_fConsumeCpu || g_fTestCpuUtilOptionSet)
    {
        Log(REQUIRED_MSG, TEXT("-e and -o options are not applicable to the test"));
        Log(REQUIRED_MSG, TEXT("Skipping the test..."));
        return TPR_SKIP;
    }

    Log(DEBUG_MSG, TEXT("+TestTCPPing()"));
    FileAddNewTestBreak(TEXT("TCP Ping"));

    ResultLog result(
        (TCHAR*)lpFTE->lpDescription,
        APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_RECV_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_BYTES_SENT) |
        APPLY_TO_CLIENT(SHOW_BYTES_RECVD) |
        APPLY_TO_CLIENT(SHOW_LATENCY)
        );

    TestResult result_Client, result_Server;
    if (!AllocTestResult(g_dwNumberOfPingBuffers, &result_Client)) goto Cleanup;
    if (!AllocTestResult(g_dwNumberOfPingBuffers, &result_Server)) goto Cleanup;

    result.ShowHeader();

    for (INT i=0; i<g_nTestBufferCount; i+=1)
    {
        DWORD bufsize = g_aTestBufferSizes[i];
        DWORD numbufs = 1;

        ServiceError err;

        // run the test
        nResult = RunTest(
            Send,           // Test type
            SOCK_STREAM,    // SOCK_STREAM or SOCK_DGRAM
            g_wUseIpVer,    // IP version
            lpFTE->dwUserData & TestOpt_NagleOff, // Disable Nagle setting (FALSE leaves Nagle on)
            -1,             // Socket send buffer (< 0 leaves at default)
            -1,             // Socket recv buffer (< 0 leaves at default)
            g_dwNumberOfPingBuffers, // Number of iterations
            bufsize,        // Send total (bytes)
            bufsize,        // Send packet/buffer size (bytes)
            bufsize,        // Receive total (bytes)
            bufsize,        // Receive packet/buffer size (bytes)
            0,              // Requested send rate (bytes/s)
            0,              // Requested send rate at client (bytes/s)
            0,              // Requested recv rate (bytes/s)
            0,              // Requested recv rate at client (bytes/s)
            0,              // CPU usage parameter is not used
            &result_Client, // Client result array
            &result_Server, // Server result array
            &err
        );


        if (nResult == TPR_PASS && err == Test_Success)
        {
            // Compute latency (overdoing here with calculation as there will be no
            // detectable loss by application with TCP)
            DWORD pings;
            for (pings = 0; pings < g_dwNumberOfPingBuffers; pings += 1)
                if (result_Client.recvd_array[pings] == 0) break;

            if (pings != g_dwNumberOfPingBuffers && pings > 1)
                for (DWORD k = pings - 1; k < g_dwNumberOfPingBuffers; k += 1)
                    result_Client.total_time -= result_Client.time_array[k];

            // Make sure to save-guard against divide-by-zero
            DWORD latency = pings > 0 ? (result_Client.total_time * 1000) / pings : 0;

            result.LogResult(
                bufsize,    // Send size (in bytes)
                bufsize,    // Recv size (in bytes)
                pings,      // Number of packets sent
                0,          // Send time (in milliseconds)
                pings,      // Number of packets recv'd
                0,          // Recv time (in milliseconds)
                latency,    // Latency (us)
                0,          // Jitter (us)
                0,          // Idle time (ms)
                0,          // Total test time (ms)
                TRUE,       // Mark it
                0           // spinning thread cpu usage not used
                );
        }
        else
        {
            Log(ERROR_MSG, TEXT("TestTCPPing test Failed"));
            nResult = TPR_FAIL;
            goto Cleanup;
        }
    }
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);

    Log(DEBUG_MSG, TEXT("-TestTCPPing()"));

    return nResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestUDPSendThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    INT rateResult = TPR_PASS;
    double consumedSysCpu = 0;

    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    if(g_fTestCpuUtilOptionSet)
    {
        Log(REQUIRED_MSG, TEXT("-o option is not applicable to the test"));
        Log(REQUIRED_MSG, TEXT("Skipping the test..."));
        return TPR_SKIP;
    }

    Log(DEBUG_MSG, TEXT("+TestUDPSendThroughput()"));
    FileAddNewTestBreak(TEXT("UDP Send Throughput"));

    ResultLog result(
        (TCHAR*)lpFTE->lpDescription,
        APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_BYTES_SENT) |
        APPLY_TO_CLIENT(SHOW_SEND_RATE) |
        APPLY_TO_CLIENT(SHOW_RECV_RATE) |
        APPLY_TO_CLIENT(SHOW_CPU_UTIL) |
        APPLY_TO_CLIENT(SHOW_PKT_LOSS)
        );

    TestResult result_Client, result_Server,
               result_ClientBackup, result_ServerBackup;
    if (!AllocTestResult(1, &result_Client)) goto Cleanup;
    if (!AllocTestResult(1, &result_Server)) goto Cleanup;
    if (!AllocTestResult(1, &result_ClientBackup)) goto Cleanup;
    if (!AllocTestResult(1, &result_ServerBackup)) goto Cleanup;

    result.ShowHeader();

    for (INT i=0; i<g_nTestBufferCountUdp; i+=1)
    {
        DWORD bufsize = g_aTestBufferSizesUdp[i];
        DWORD numbufs = g_dwNumberOfBuffers;

        DWORD req_send_rate =  // For the client
            g_dwReqSendRateKbps != 0 ?
            g_dwReqSendRateKbps * 1000 / 8 : 0;
        DWORD last_passed_send_rate = 0;
        DWORD last_failed_send_rate = 0;

        // choose a tolerance level
        DWORD tolerance_kbytespersec = 10000;

        for (;;)
        {
            ServiceError err;

            // record start time if g_fConsumeCpu is enabled
            if(g_fConsumeCpu)
            {
                if(startConsumeCpuTime() == FALSE)
                {
                    nResult = TPR_FAIL;
                    goto Cleanup;
                }
            }

            // run the test
            nResult = RunTest(
                Send,               // Test type
                SOCK_DGRAM,         // SOCK_STREAM or SOCK_DGRAM
                g_wUseIpVer,        // IP version
                FALSE,              // Disable Nagle setting (FALSE leaves Nagle on)
                -1,                 // Socket send buffer (< 0 leaves at default)
                -1,                 // Socket recv buffer (< 0 leaves at default)
                1,                  // Number of iterations
                bufsize * numbufs,  // Send total (bytes)
                bufsize,            // Send packet/buffer size (bytes)
                0,                  // Receive total (bytes)
                0,                  // Receive packet/buffer size (bytes)
                0,                  // Requested send rate (bytes/s)
                req_send_rate,      // Requested send rate at client (bytes/s)
                0,                  // Requested recv rate (bytes/s)
                0,                  // Requested recv rate at client (bytes/s)
                0,                  // CPU usage parameter is not used
                &result_Client,     // Client result array
                &result_Server,     // Server result array
                &err
            );

            // calculate the cpu usage of the spinning thread at the end of test
            // if g_fConsumeCpu is set
            if(g_fConsumeCpu)
            {
                if(stopConsumeCpuTime() == FALSE)
                {
                    nResult = TPR_FAIL;
                    goto Cleanup;
                }
                consumedSysCpu = getCpuUsage();
            }

            // If not all packets were received, can adjust client-time to compensate for
            // the timeout
            if (result_Server.recvd_array[0] != result_Client.sent_array[0]) {
                // CONNTRACK_TIMEOUT_MS is the timeout value of the test, the returned
                // server time is always < CONNTRACK_TIMEOUT_MS.
                if (result_Server.time_array[0] >= CONNTRACK_TIMEOUT_MS)
                    result_Server.time_array[0] -= CONNTRACK_TIMEOUT_MS;
                // If packets are dropped during UDP, this ELSE is always executed,
                // and should not set the return time to 0. Commenting it out.
                // else
                    //result_Server.time_array[0] = 0;

                // CONNTRACK_TIMEOUT_MS is the timeout value of the test, the returned
                // server time is always < CONNTRACK_TIMEOUT_MS.
                if (result_Server.cpuutil_array[0] >= CONNTRACK_TIMEOUT_MS)
                    result_Server.cpuutil_array[0] -= CONNTRACK_TIMEOUT_MS;
                // If packets are dropped during UDP, this ELSE is always executed,
                // and should not set the return CPU utilization to 0. Commenting it out.
                // else
                    //result_Server.cpuutil_array[0] = 0;
            }

            // To avoid any problems with divide-by-zero
            if (result_Client.time_array[0] < 1) result_Client.time_array[0] = 1;
            if (result_Server.time_array[0] < 1) result_Server.time_array[0] = 1;

            if (nResult == TPR_PASS)
            {
                Log(DEBUG_MSG,   TEXT("    Client:"));
                Log(DEBUG_MSG,      TEXT("        Packets/buffers sent: %ld (%ld total bytes)"),
                    result_Client.sent_array[0], result_Client.sent_array[0] * bufsize);
                Log(DEBUG_MSG,      TEXT("        Packets/buffers rcvd: %ld (%ld total bytes)"),
                    result_Client.recvd_array[0], result_Client.recvd_array[0] * bufsize);
                Log(DEBUG_MSG,   TEXT("        Time: %ld ms (%ld Kbytes/s)"),
                    result_Client.time_array[0],
                    bufsize * result_Client.recvd_array[0] / result_Client.time_array[0]);
                Log(DEBUG_MSG,   TEXT("    Server:"));
                Log(DEBUG_MSG,      TEXT("        Packets/buffers sent: %ld (%ld total bytes)"),
                    result_Server.sent_array[0], result_Server.sent_array[0] * bufsize);
                Log(DEBUG_MSG,      TEXT("        Packets/buffers rcvd: %ld (%ld total bytes)"),
                    result_Server.recvd_array[0], result_Server.recvd_array[0] * bufsize);
                Log(DEBUG_MSG,   TEXT("        Server time: %d ms (%ld Kbytes/s)"),
                    result_Server.time_array[0],
                    bufsize * result_Server.sent_array[0] / result_Server.time_array[0]);
            }
            else
            {
                Log(ERROR_MSG, TEXT("TestUDPSendThroughput test Failed"));
                goto Cleanup;
            }

            if(!result.LogResult(
                bufsize,                        // Send size (in bytes)
                bufsize,                        // Recv size (in bytes)
                result_Client.sent_array[0],    // Number of packets sent
                result_Client.time_array[0],    // Send time (in milliseconds)
                result_Server.recvd_array[0],   // Number of packets recv'd
                result_Server.time_array[0],    // Recv time (in milliseconds)
                0,                              // Latency (us)
                0,                              // Jitter (us)
                result_Client.cpuutil_array[0], // Idle time (ms)
                result_Client.time_array[0],    // Total test time (ms)
                TRUE,                           // Mark it?
                consumedSysCpu                  // CPU usage of the spinning thread
            ))
            {
                rateResult = TPR_FAIL;
            }

            // Terminate after the first iteration if rate was selected by the user
            // on command line.
            if (g_dwReqSendRateKbps != 0 || !g_fGatewayTestOption) {
                CopyTestResult(&result_ClientBackup, &result_Client);
                CopyTestResult(&result_ServerBackup, &result_Server);
                break;
            }


            // Implement divide and conquer alg to find the next rate at which the client
            // should send packets to the server
            double actual_send_rate = (double)(bufsize * result_Client.sent_array[0]) * 1000. / result_Client.time_array[0];

            if (req_send_rate == 0)
                req_send_rate = (DWORD)actual_send_rate;

            if (result_Server.recvd_array[0] != numbufs)
            {
                if (last_failed_send_rate > actual_send_rate || last_failed_send_rate == 0)
                    last_failed_send_rate = (DWORD)actual_send_rate;
                if (last_passed_send_rate != 0 &&
                    last_passed_send_rate > last_failed_send_rate) break;

                // Calculate new send rate
                if (last_passed_send_rate == 0)
                {
                    req_send_rate = last_failed_send_rate / 2;
                    if (req_send_rate <= tolerance_kbytespersec) {
                        last_passed_send_rate = tolerance_kbytespersec / 2; // between 0 and tolerance
                        break;
                    }
                }
                else
                {
                    if (last_failed_send_rate - last_passed_send_rate <= tolerance_kbytespersec) break;

                    DWORD new_req_send_rate = last_passed_send_rate
                        + (last_failed_send_rate - last_passed_send_rate)/2;
                    if (new_req_send_rate == req_send_rate)
                        break;
                    else
                        req_send_rate = new_req_send_rate;
                }
            }
            else
            {
                // Save this result for reporting later on
                if (last_passed_send_rate == 0 ||
                    result_ServerBackup.time_array[0] > result_Server.time_array[0])
                {
                    CopyTestResult(&result_ClientBackup, &result_Client);
                    CopyTestResult(&result_ServerBackup, &result_Server);
                }

                if (last_passed_send_rate < actual_send_rate || last_passed_send_rate == 0)
                {
                    last_passed_send_rate = (DWORD)actual_send_rate;

                    CopyTestResult(&result_ClientBackup, &result_Client);
                    CopyTestResult(&result_ServerBackup, &result_Server);
                }
                if (last_failed_send_rate == 0) {
                    // Packets were already sent at max rate
                    if (!g_fGatewayTestOption) {
                        // User just cares about max send rate;
                        // No need to find the optimal send rate so that the receiver receives all packets
                        CopyTestResult(&result_ClientBackup, &result_Client);
                        CopyTestResult(&result_ServerBackup, &result_Server);
                    }
                    break;
                }

                if (last_passed_send_rate > last_failed_send_rate) break;
                if (last_failed_send_rate - last_passed_send_rate <= tolerance_kbytespersec) break;

                DWORD new_req_send_rate = last_passed_send_rate
                    + (last_failed_send_rate - last_passed_send_rate)/2;
                if (new_req_send_rate == req_send_rate)
                    break;
                else
                    req_send_rate = new_req_send_rate;
            }

            Log(DEBUG_MSG, TEXT("  New send rate: %ld Kbytes/s (pass=%ld, fail=%ld)"),
                req_send_rate / 1000, last_passed_send_rate / 1000, last_failed_send_rate / 1000);
        }

        //
        // TODO: Still need to handle the condition where we fail, i.e. cannot find the
        //       optimal send rate s.t. the receiver receives all packets.
        //

        result.LogResult(
            bufsize,                        // Send size (in bytes)
            bufsize,                        // Recv size (in bytes)
            result_ClientBackup.sent_array[0],    // Number of packets sent
            result_ClientBackup.time_array[0],    // Send time (in milliseconds)
            result_ServerBackup.recvd_array[0],   // Number of packets recv'd
            result_ServerBackup.time_array[0],    // Recv time (in milliseconds)
            0,                              // Latency (us)
            0,                              // Jitter (us)
            result_ClientBackup.cpuutil_array[0], // Idle time (ms)
            result_ClientBackup.time_array[0],    // Total test time (ms)
            FALSE,                            // Mark it?
            consumedSysCpu              // CPU usage of the spinning thread
            );

    }
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);

    FreeTestResult(&result_ClientBackup);
    FreeTestResult(&result_ServerBackup);

    Log(DEBUG_MSG, TEXT("-TestUDPSendThroughput()"));

    if(rateResult == TPR_PASS)
       return nResult;
    else
      return rateResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestUDPSendRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst
    INT nTestBufferCountUdp = 0;
    INT nRateResult = TPR_PASS;
    int* aTestBufferSizesUdp = NULL;

    if(g_fConsumeCpu || g_fTestCpuUtilOptionSet)
    {
        Log(REQUIRED_MSG, TEXT("-e and -o options are not applicable to the test"));
        Log(REQUIRED_MSG, TEXT("Skipping the test..."));
        return TPR_SKIP;
    }

    Log(DEBUG_MSG, TEXT("+TestUDPSendRecvThroughput()"));
    FileAddNewTestBreak(TEXT("UDP SendRecv Throughput"));

    ResultLog result(
        (TCHAR*)lpFTE->lpDescription,
        APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_SEND_RATE) |
        APPLY_TO_CLIENT(SHOW_SEND_CPU_UTIL) |
        APPLY_TO_CLIENT(SHOW_SEND_PKT_LOSS) |
        APPLY_TO_CLIENT(SHOW_RECV_RATE) |
        APPLY_TO_CLIENT(SHOW_RECV_CPU_UTIL) |
        APPLY_TO_CLIENT(SHOW_RECV_PKT_LOSS)
        );

    TestResult result_Client, result_Server,
               result_Client_Send, result_Server_Send,
               result_ClientBackup, result_ServerBackup;
    if (!AllocTestResult(1, &result_Client)) goto Cleanup;
    if (!AllocTestResult(1, &result_Server)) goto Cleanup;
    if (!AllocTestResult(1, &result_Client_Send)) goto Cleanup;
    if (!AllocTestResult(1, &result_Server_Send)) goto Cleanup;
    if (!AllocTestResult(1, &result_ClientBackup)) goto Cleanup;
    if (!AllocTestResult(1, &result_ServerBackup)) goto Cleanup;

    result.ShowHeaderSendRecv();

    if (!g_fUserSelectedBuffer)
    {
        INT aTestBufs[]
                    = {16, 32, 64, 128, 256, 512, 1024, 1460, 2048, 2920, 4096, 8192, 16384, 32768};

        nTestBufferCountUdp = sizeof(aTestBufs)/sizeof(INT);
        if (aTestBufferSizesUdp != NULL) delete [] aTestBufferSizesUdp; // de allocate the memory allocated earlier
        aTestBufferSizesUdp = new INT[nTestBufferCountUdp];             // allocate memory now specific to this test

        if (aTestBufferSizesUdp == NULL)
        {
            Log(ERROR_MSG, TEXT("Memory allocation failed for UDP Test Buffer Sizes, Terminating Test"));
            nResult = TPR_FAIL;
            goto Cleanup;
        }
        memcpy(aTestBufferSizesUdp, aTestBufs, nTestBufferCountUdp * sizeof(INT));
    }
    else
    {
        nTestBufferCountUdp = g_nTestBufferCountUdp;
        if (aTestBufferSizesUdp != NULL) delete [] aTestBufferSizesUdp; // de allocate the memory allocated earlier
        aTestBufferSizesUdp = new INT[nTestBufferCountUdp];             // allocate memory now specific to this test

        if (aTestBufferSizesUdp == NULL)
        {
            Log(ERROR_MSG, TEXT("Memory allocation failed for UDP Test Buffer Sizes, Terminating Test"));
            nResult = TPR_FAIL;
            goto Cleanup;
        }
        memcpy(aTestBufferSizesUdp, g_aTestBufferSizesUdp, nTestBufferCountUdp * sizeof(INT));
    }

    for (INT nBufferIndex = 0; nBufferIndex < nTestBufferCountUdp; nBufferIndex++)
    {
        DWORD dwBufSize = aTestBufferSizesUdp[nBufferIndex];
        DWORD dwNumBufs = 0;
        if (g_fUserSelectedNumOfBuffers)
        {
            dwNumBufs = g_dwNumberOfBuffers;
        }
        else
        {
            dwNumBufs = (128 * KILO_BYTES / dwBufSize);  //to send/recv 128 kbytes data for each packet size
        }
        
        INT nCount = 0;
        BOOL fSendFlag = TRUE;

        while (nCount < 2)
        {
            DWORD dwRequestedSendRate =                     // For the client
                g_dwReqSendRateKbps != 0 ?
                g_dwReqSendRateKbps * KILO_BYTES / 8 : 0;
            double lflActualSendRate = 0;
            DWORD dwLastPassedSendRate = 0;
            DWORD dwLastFailedSendRate = 0;
            INT nRcvQueueBytes = 0;
            DWORD dwSendTotalBytes = 0;
            DWORD dwRecvTotalBytes = 0;
            DWORD dwSendBufSize = 0;
            DWORD dwRecvBufSize = 0;
            DWORD dwReqSendRate = 0;
            DWORD dwReqSendRateClient = 0;
            DWORD dwReceivedArray = 0;

            // choose a tolerance level
            DWORD dwToleranceSendRate = 10000;

            for (;;)
            {
                ServiceError err;

                if (fSendFlag)  // parameters to be passed for send test
                {
                    nRcvQueueBytes = -1;
                    dwSendTotalBytes = dwBufSize * dwNumBufs;
                    dwSendBufSize = dwBufSize;
                    dwRecvTotalBytes = 0;
                    dwRecvBufSize = 0;
                    dwReqSendRate = 0;
                    dwReqSendRateClient = dwRequestedSendRate;
                }
                else             // parameters to be passed for receive test
                {
                    nRcvQueueBytes = g_iUdpRcvQueueBytes;
                    dwSendTotalBytes = 1;
                    dwSendBufSize = 1;
                    dwRecvTotalBytes = dwBufSize * dwNumBufs;
                    dwRecvBufSize = dwBufSize;
                    dwReqSendRate = dwRequestedSendRate;
                    dwReqSendRateClient = 0;
                }

                // run the test
                nResult = RunTest(
                    Send,                 // Test type
                    SOCK_DGRAM,           // SOCK_STREAM or SOCK_DGRAM
                    g_wUseIpVer,          // IP version
                    FALSE,                // Disable Nagle setting (FALSE leaves Nagle on)
                    -1,                   // Socket send buffer (< 0 leaves at default)
                    nRcvQueueBytes,       // Socket recv buffer (< 0 leaves at default)
                    1,                    // Number of iterations
                    dwSendTotalBytes,     // Send total (bytes)
                    dwSendBufSize,        // Send packet/buffer size (bytes)
                    dwRecvTotalBytes,     // Receive total (bytes)
                    dwRecvBufSize,        // Receive packet/buffer size (bytes)
                    dwReqSendRate,        // Requested send rate (bytes/s)
                    dwReqSendRateClient,  // Requested send rate at client (bytes/s)
                    0,                    // Requested recv rate (bytes/s)
                    0,                    // Requested recv rate at client (bytes/s)
                    0,                  // CPU usage parameter is not used       
                    &result_Client,       // Client result array
                    &result_Server,       // Server result array
                    &err
                );

                if ((nResult != TPR_PASS) && (err != Test_Success))
                {
                    Log(ERROR_MSG, TEXT("TestUDPSendRecvThroughput test Failed"));
                    nResult = TPR_FAIL;
                    goto Cleanup;
                }

                if (fSendFlag)
                {
                    // If not all packets were received, can adjust client-time to compensate for
                    // the timeout
                    if (result_Server.recvd_array[0] != result_Client.sent_array[0]) {
                        // CONNTRACK_TIMEOUT_MS is the timeout value of the test, the returned
                        // server time is always < CONNTRACK_TIMEOUT_MS.
                        if (result_Server.time_array[0] >= CONNTRACK_TIMEOUT_MS)
                            result_Server.time_array[0] -= CONNTRACK_TIMEOUT_MS;

                        // CONNTRACK_TIMEOUT_MS is the timeout value of the test, the returned
                        // server time is always < CONNTRACK_TIMEOUT_MS.
                        if (result_Server.cpuutil_array[0] >= CONNTRACK_TIMEOUT_MS)
                            result_Server.cpuutil_array[0] -= CONNTRACK_TIMEOUT_MS;
                    }
                }

                // To avoid any problems with divide-by-zero
                if (result_Client.time_array[0] < 1) result_Client.time_array[0] = 1;
                if (result_Server.time_array[0] < 1) result_Server.time_array[0] = 1;

                Log(DEBUG_MSG,   TEXT("    Client:"));
                Log(DEBUG_MSG,      TEXT("        Packets/buffers sent: %ld (%ld total bytes)"),
                    result_Client.sent_array[0], result_Client.sent_array[0] * dwBufSize);
                Log(DEBUG_MSG,      TEXT("        Packets/buffers rcvd: %ld (%ld total bytes)"),
                    result_Client.recvd_array[0], result_Client.recvd_array[0] * dwBufSize);
                Log(DEBUG_MSG,   TEXT("        Time: %ld ms (%ld Kbytes/s)"),
                    result_Client.time_array[0],
                    dwBufSize * result_Client.recvd_array[0] / result_Client.time_array[0]);
                Log(DEBUG_MSG,   TEXT("    Server:"));
                Log(DEBUG_MSG,      TEXT("        Packets/buffers sent: %ld (%ld total bytes)"),
                    result_Server.sent_array[0], result_Server.sent_array[0] * dwBufSize);
                Log(DEBUG_MSG,      TEXT("        Packets/buffers rcvd: %ld (%ld total bytes)"),
                    result_Server.recvd_array[0], result_Server.recvd_array[0] * dwBufSize);
                Log(DEBUG_MSG,   TEXT("        Server time: %d ms (%11.2f Kbytes/s)"),
                    result_Server.time_array[0],
                    (double)(dwBufSize * result_Server.sent_array[0]) / (double)(KILO_BYTES * result_Server.time_array[0] * MILLISEC_TO_SEC));

                // Terminate after the first iteration if rate was selected by the user
                // on command line.
                if (fSendFlag)
                {
                    if (g_dwReqSendRateKbps != 0 || !g_fGatewayTestOption) {
                        CopyTestResult(&result_ClientBackup, &result_Client);
                        CopyTestResult(&result_ServerBackup, &result_Server);
                        break;
                    }
                }
                else
                {
                    if (g_dwReqSendRateKbps != 0) {
                        CopyTestResult(&result_ClientBackup, &result_Client);
                        CopyTestResult(&result_ServerBackup, &result_Server);
                        break;
                    }
                }

                // Implement divide and conquer alg to find the next rate at which the client
                // should send packets to the server
                if (fSendFlag){
                    lflActualSendRate = (double)(dwBufSize * result_Client.sent_array[0]) / (double)(result_Client.time_array[0] * MILLISEC_TO_SEC);
                    dwReceivedArray = result_Server.recvd_array[0];
                }
                else {
                    lflActualSendRate = (double)(dwBufSize * result_Server.sent_array[0]) / (double)(result_Server.time_array[0] * MILLISEC_TO_SEC);
                    dwReceivedArray = result_Client.recvd_array[0];
                }

                if (0 == dwRequestedSendRate)
                    dwRequestedSendRate = (DWORD)lflActualSendRate;

                if (dwReceivedArray != dwNumBufs)
                {
                    if (dwLastFailedSendRate > lflActualSendRate || dwLastFailedSendRate == 0)
                        dwLastFailedSendRate = (DWORD)lflActualSendRate;
                    if (dwLastPassedSendRate != 0 &&
                        dwLastPassedSendRate > dwLastFailedSendRate)
                        break;

                    // Calculate new send rate
                    if (0 == dwLastPassedSendRate)
                    {
                        dwRequestedSendRate = dwLastFailedSendRate / 2;
                        if (dwRequestedSendRate <= dwToleranceSendRate) {
                            dwLastPassedSendRate = dwToleranceSendRate / 2; // between 0 and tolerance
                            break;
                        }
                    }
                    else
                    {
                        if (dwLastFailedSendRate - dwLastPassedSendRate <= dwToleranceSendRate)
                            break;

                        DWORD dwNewReqSendRate = dwLastPassedSendRate
                            + (dwLastFailedSendRate - dwLastPassedSendRate)/2;
                        if (dwNewReqSendRate == dwRequestedSendRate)
                            break;
                        else
                            dwRequestedSendRate = dwNewReqSendRate;
                    }
                }
                else
                {
                    // Save this result for reporting later on
                    if ((0 == dwLastPassedSendRate) ||
                        (result_ServerBackup.time_array[0] > result_Server.time_array[0]))
                    {
                        CopyTestResult(&result_ClientBackup, &result_Client);
                        CopyTestResult(&result_ServerBackup, &result_Server);
                    }

                    if ((dwLastPassedSendRate < lflActualSendRate) || (dwLastPassedSendRate == 0))
                    {
                        dwLastPassedSendRate = (DWORD)lflActualSendRate;

                        CopyTestResult(&result_ClientBackup, &result_Client);
                        CopyTestResult(&result_ServerBackup, &result_Server);
                    }
                    if (0 == dwLastFailedSendRate) {
                        // Packets were already sent at max rate
                        if (fSendFlag){
                            if (!g_fGatewayTestOption) {
                                // User just cares about max send rate;
                                // No need to find the optimal send rate so that the receiver receives all packets
                                CopyTestResult(&result_ClientBackup, &result_Client);
                                CopyTestResult(&result_ServerBackup, &result_Server);
                            }
                        }
                        break;
                    }

                    if (dwLastPassedSendRate > dwLastFailedSendRate)
                        break;
                    if (dwLastFailedSendRate - dwLastPassedSendRate <= dwToleranceSendRate)
                        break;

                    DWORD dwNewReqSendRate = dwLastPassedSendRate
                        + (dwLastFailedSendRate - dwLastPassedSendRate)/2;
                    if (dwNewReqSendRate == dwRequestedSendRate)
                        break;
                    else
                        dwRequestedSendRate = dwNewReqSendRate;
                }

                Log(DEBUG_MSG, TEXT("  New send rate: %ld Kbytes/s (pass=%ld, fail=%ld)"),
                    dwRequestedSendRate / KILO_BYTES, dwLastPassedSendRate / KILO_BYTES, dwLastFailedSendRate / KILO_BYTES);

                // If you're receiving large packets, then pause for the system to restore its reassembly buffers
                if (dwBufSize > (MTUBytes - (IPHeaderBytes + UDPHeaderBytes)))
                {
                    Sleep(ReassemblyTimeoutMs);
                }
            }

            if (fSendFlag){
                // copy test result from send test and execute recv test
                CopyTestResult(&result_Client_Send, &result_ClientBackup);
                CopyTestResult(&result_Server_Send, &result_ServerBackup);
            }

            nCount++;                // increment this for recv test
            fSendFlag = !fSendFlag;  // this will run recv test
        }

        if(!result.LogResultSendRecv(
            dwBufSize,                           // Send size (in bytes)
            dwBufSize,                           // Recv size (in bytes)
            result_Client_Send.sent_array[0],    // Number of packets sent
            result_Client_Send.time_array[0],    // Send time (in milliseconds)
            result_ClientBackup.recvd_array[0],  // Number of packets recv'd
            result_ClientBackup.time_array[0],   // Recv time (in milliseconds)
            result_ServerBackup.sent_array[0],   // Number of packets sent from server
            result_Server_Send.recvd_array[0],   // Number of packets recv'd at server
            0,                                   // Latency (us)
            0,                                   // Jitter (us)
            0,                                   // Idle time (ms)
            0,                                   // Total Test time (ms)
            result_Client_Send.cpuutil_array[0], // Send Test Idle Time (ms)
            result_Client_Send.time_array[0],    // Total Send test time (ms)
            result_ClientBackup.cpuutil_array[0],// Recv Test Idle time (ms)
            result_ClientBackup.time_array[0],   // Total Recv test time (ms)
            TRUE                                 // Mark it?
        ))
        {
            nRateResult = TPR_FAIL;
        }
    }
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);
    FreeTestResult(&result_Client_Send);
    FreeTestResult(&result_Server_Send);
    FreeTestResult(&result_ClientBackup);
    FreeTestResult(&result_ServerBackup);

    if (aTestBufferSizesUdp != NULL) delete [] aTestBufferSizesUdp;

    Log(DEBUG_MSG, TEXT("-TestUDPSendRecvThroughput()"));

    if(nRateResult == TPR_PASS)
       return nResult;
    else
      return nRateResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestUDPSendPacketLoss(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    if(g_fConsumeCpu || g_fTestCpuUtilOptionSet)
    {
        Log(REQUIRED_MSG, TEXT("-e and -o options are not applicable to the test"));
        Log(REQUIRED_MSG, TEXT("Skipping the test..."));
        return TPR_SKIP;
    }

    Log(DEBUG_MSG, TEXT("+TestUDPSendPacketLoss()"));
    FileAddNewTestBreak(TEXT("UDP Send Packet Loss"));

    ResultLog result(
        (TCHAR*)lpFTE->lpDescription,
        APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_BYTES_SENT) |
        APPLY_TO_CLIENT(SHOW_SEND_RATE) |
        APPLY_TO_CLIENT(SHOW_CPU_UTIL) |
        APPLY_TO_CLIENT(SHOW_PKT_LOSS) |
        APPLY_TO_CLIENT(HIGHLIGHT_SEND_RATE)
        );

    TestResult result_Client, result_Server;
    if (!AllocTestResult(1, &result_Client)) goto Cleanup;
    if (!AllocTestResult(1, &result_Server)) goto Cleanup;

    result.ShowHeader();

    for (INT i=0; i<g_nTestBufferCountUdp; i+=1)
    {
        DWORD bufsize = g_aTestBufferSizesUdp[i];
        DWORD numbufs = g_dwNumberOfBuffers;

        DWORD req_send_rate = 0;
        DWORD max_send_rate = 0;
        DWORD last_passed_send_rate = 0;
        DWORD last_failed_send_rate = 0;

        if (g_dwReqSendRateKbps != 0)
            req_send_rate = g_dwReqSendRateKbps * 1000 / 8; // Convert to Kbytes/s
        else
            req_send_rate = 125000; // 125000 Kbytes/s = 1 Mbps

        for (;;)
        {
            ServiceError err;

            // run the test
            nResult = RunTest(
                Send,               // Test type
                SOCK_DGRAM,         // SOCK_STREAM or SOCK_DGRAM
                g_wUseIpVer,        // IP version
                FALSE,              // Disable Nagle setting (FALSE leaves Nagle on)
                -1,                 // Socket send buffer (< 0 leaves at default)
                -1,                 // Socket recv buffer (< 0 leaves at default)
                1,                  // Number of iterations
                bufsize * numbufs,  // Send total (bytes)
                bufsize,            // Send packet/buffer size (bytes)
                0,                  // Receive total (bytes)
                0,                  // Receive packet/buffer size (bytes)
                0,                  // Requested send rate (bytes/s)
                req_send_rate,      // Requested send rate at client (bytes/s)
                0,                  // Requested recv rate (bytes/s)
                0,                  // Requested recv rate at client (bytes/s)
                0,                  // CPU usage parameter is not used    
                &result_Client,     // Client result array
                &result_Server,     // Server result array
                &err
            );

            // To avoid any problems with divide-by-zero
            if (result_Client.time_array[0] < 1) result_Client.time_array[0] = 1;
            if (result_Server.time_array[0] < 1) result_Server.time_array[0] = 1;

            if (nResult == TPR_PASS)
            {
                Log(DEBUG_MSG,   TEXT("    Client:"));
                Log(DEBUG_MSG,      TEXT("        Packets/buffers sent: %ld (%ld total bytes)"),
                    result_Client.sent_array[0], result_Client.sent_array[0] * bufsize);
                Log(DEBUG_MSG,      TEXT("        Packets/buffers rcvd: %ld (%ld total bytes)"),
                    result_Client.recvd_array[0], result_Client.recvd_array[0] * bufsize);
                Log(DEBUG_MSG,   TEXT("        Time: %ld ms (%ld Kbytes/s)"),
                    result_Client.time_array[0],
                    bufsize * result_Client.recvd_array[0] / result_Client.time_array[0]);
                Log(DEBUG_MSG,   TEXT("    Server:"));
                Log(DEBUG_MSG,      TEXT("        Packets/buffers sent: %ld (%ld total bytes)"),
                    result_Server.sent_array[0], result_Server.sent_array[0] * bufsize);
                Log(DEBUG_MSG,      TEXT("        Packets/buffers rcvd: %ld (%ld total bytes)"),
                    result_Server.recvd_array[0], result_Server.recvd_array[0] * bufsize);
                Log(DEBUG_MSG,   TEXT("        Server time: %d ms (%ld Kbytes/s)"),
                    result_Server.time_array[0],
                    bufsize * result_Server.sent_array[0] / result_Server.time_array[0]);
            }
            else
            {
                Log(ERROR_MSG, TEXT("TestUDPSendPacketLoss test Failed"));
                goto Cleanup;
            }

            // Implement divide and conquer alg to find the next rate at which the server
            // should send packets to the client
            double actual_send_rate = (double)(bufsize * result_Client.sent_array[0]) * 1000. / result_Client.time_array[0];
            double actual_recv_rate = (double)(bufsize * result_Server.recvd_array[0]) * 1000. / result_Server.time_array[0];

            Log(DEBUG_MSG, TEXT("Requested rate = %ld Kbps (Actual = %.2f Kbps)"),
                req_send_rate * 8 / 1000,
                actual_send_rate * 8 / 1000);

            result.LogResult(
                bufsize,                        // Send size (in bytes)
                bufsize,                        // Recv size (in bytes)
                result_Client.sent_array[0],    // Number of packets sent
                result_Client.time_array[0],    // Send time (in milliseconds)
                result_Server.recvd_array[0],   // Number of packets recv'd
                result_Server.time_array[0],    // Recv time (in milliseconds)
                0,                              // Latency (us)
                0,                              // Jitter (us)
                result_Server.cpuutil_array[0], // Idle time (ms)
                result_Server.time_array[0],    // Total test time (ms)
                TRUE,                           // Mark it
                0                               // spinning thread cpu usage not used
            );

            // Terminate after first iteration if rate was selected by the user
            // on command line or gateway option was not selected
            if (g_dwReqSendRateKbps != 0) {
                break;
            }

            if (req_send_rate == 0) break;
            if (actual_send_rate + 125000/2 < req_send_rate)
                req_send_rate = 0;
            else
                req_send_rate += 125000;
            
        }
    }

Cleanup:
    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);

    Log(DEBUG_MSG, TEXT("-TestUDPSendPacketLoss()"));

    return nResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestUDPRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    if(g_fTestCpuUtilOptionSet)
    {
        Log(REQUIRED_MSG, TEXT("-o option is not applicable to the test"));
        Log(REQUIRED_MSG, TEXT("Skipping the test..."));
        return TPR_SKIP;
    }

    Log(DEBUG_MSG, TEXT("+TestUDPRecvThroughput()"));
    FileAddNewTestBreak(TEXT("UDP Recv Throughput"));

    ResultLog result(
        (TCHAR*)lpFTE->lpDescription,
        APPLY_TO_CLIENT(SHOW_RECV_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_BYTES_RECVD) |
        APPLY_TO_CLIENT(SHOW_SEND_RATE) |
        APPLY_TO_CLIENT(SHOW_CPU_UTIL) |
        APPLY_TO_CLIENT(SHOW_PKT_LOSS)|
        APPLY_TO_CLIENT(SHOW_RECV_RATE)
        );

    INT rateResult = TPR_PASS;
    double consumedSysCpu = 0;

    TestResult result_Client, result_Server,
               result_ClientBackup, result_ServerBackup;
    if (!AllocTestResult(1, &result_Client)) goto Cleanup;
    if (!AllocTestResult(1, &result_Server)) goto Cleanup;
    if (!AllocTestResult(1, &result_ClientBackup)) goto Cleanup;
    if (!AllocTestResult(1, &result_ServerBackup)) goto Cleanup;

    result.ShowHeader();

    for (INT i=0; i<g_nTestBufferCountUdp; i+=1)
    {
        DWORD bufsize = g_aTestBufferSizesUdp[i];
        DWORD numbufs = g_dwNumberOfBuffers;

        DWORD req_send_rate =
            g_dwReqSendRateKbps != 0 ?
            g_dwReqSendRateKbps * 1000 / 8 : 0;
        DWORD max_send_rate = 0;
        DWORD last_passed_send_rate = 0;
        DWORD last_failed_send_rate = 0;

        // choose a tolerance level
        DWORD tolerance_kbytespersec = 10000;

        for (;;)
        {
            ServiceError  err;

            // record start time if g_fConsumeCpu is enabled
            if(g_fConsumeCpu)
            {
                if(startConsumeCpuTime() == FALSE)
                {
                    nResult = TPR_FAIL;
                    goto Cleanup;
                }
            }           

            // run the test
            nResult = RunTest(
                Send,               // Test type
                SOCK_DGRAM,         // SOCK_STREAM or SOCK_DGRAM
                g_wUseIpVer,        // IP version
                FALSE,              // Disable Nagle setting (FALSE leaves Nagle on)
                -1,                 // Socket send buffer (< 0 leaves at default)
                g_iUdpRcvQueueBytes,// Socket recv buffer (< 0 leaves at default)
                1,                  // Number of iterations
                1,                  // Send total (bytes)
                1,                  // Send packet/buffer size (bytes)
                bufsize * numbufs,  // Receive total (bytes)
                bufsize,            // Receive packet/buffer size (bytes)

                req_send_rate,      // Requested send rate (bytes/s)
                0,                  // Requested send rate at client (bytes/s)
                0,                  // Requested recv rate (bytes/s)
                0,                  // Requested recv rate at client (bytes/s)
                0,                  // CPU usage parameter is not used  
                &result_Client,     // Client result array
                &result_Server,     // Server result array
                &err
            );

            // calculate the cpu usage of the spinning thread at the end of test
            // if g_fConsumeCpu is set
            if(g_fConsumeCpu)
            {
                if(stopConsumeCpuTime() == FALSE)
                {
                    nResult = TPR_FAIL;
                    goto Cleanup;
                }
                consumedSysCpu = getCpuUsage();
            }     

            // To avoid any problems with divide-by-zero
            if (result_Client.time_array[0] < 1) result_Client.time_array[0] = 1;
            if (result_Server.time_array[0] < 1) result_Server.time_array[0] = 1;

            if (nResult == TPR_PASS)
            {
                Log(DEBUG_MSG,   TEXT("    Client:"));
                Log(DEBUG_MSG,      TEXT("        Packets/buffers sent: %ld (%ld total bytes)"),
                    result_Client.sent_array[0], result_Client.sent_array[0] * bufsize);
                Log(DEBUG_MSG,      TEXT("        Packets/buffers rcvd: %ld (%ld total bytes)"),
                    result_Client.recvd_array[0], result_Client.recvd_array[0] * bufsize);
                Log(DEBUG_MSG,   TEXT("        Time: %ld ms (%ld Kbytes/s)"),
                    result_Client.time_array[0],
                    bufsize * result_Client.recvd_array[0] / result_Client.time_array[0]);
                Log(DEBUG_MSG,   TEXT("    Server:"));
                Log(DEBUG_MSG,      TEXT("        Packets/buffers sent: %ld (%ld total bytes)"),
                    result_Server.sent_array[0], result_Server.sent_array[0] * bufsize);
                Log(DEBUG_MSG,      TEXT("        Packets/buffers rcvd: %ld (%ld total bytes)"),
                    result_Server.recvd_array[0], result_Server.recvd_array[0] * bufsize);
                Log(DEBUG_MSG,   TEXT("        Server time: %d ms (%ld Kbytes/s)"),
                    result_Server.time_array[0],
                    bufsize * result_Server.sent_array[0] / result_Server.time_array[0]);
            }
            else
            {
                Log(ERROR_MSG, TEXT("TestUDPRecvThroughput test Failed"));
                goto Cleanup;
            }

            if(!result.LogResult(
                bufsize,                        // Send size (in bytes)
                bufsize,                        // Recv size (in bytes)
                result_Server.sent_array[0],    // Number of packets sent
                result_Server.time_array[0],    // Send time (in milliseconds)
                result_Client.recvd_array[0],   // Number of packets recv'd
                result_Client.time_array[0],    // Recv time (in milliseconds)
                0,                              // Latency (us)
                0,                              // Jitter (us)
                result_Client.cpuutil_array[0], // Idle time (ms)
                result_Client.time_array[0],    // Total test time (ms)
                TRUE,                           // Mark it
                consumedSysCpu                  // CPU usage of the spinning thread
            ))
            {
                rateResult = TPR_FAIL;
            }

            // Terminate after first iteration if rate was selected by the user
            // on command line.
            if (g_dwReqSendRateKbps != 0) {
                CopyTestResult(&result_ClientBackup, &result_Client);
                CopyTestResult(&result_ServerBackup, &result_Server);
                break;
            }

            // Implement divide and conquer alg to find the next rate at which the server
            // should send packets to the client
            double actual_send_rate = (double)(bufsize * result_Server.sent_array[0]) * 1000. / result_Server.time_array[0];

            if (max_send_rate == 0)
                max_send_rate = (DWORD)actual_send_rate;
            if (req_send_rate == 0)
                req_send_rate = max_send_rate;

            if (result_Client.recvd_array[0] != numbufs)
            {
                if (last_failed_send_rate > actual_send_rate || last_failed_send_rate == 0)
                    last_failed_send_rate = (DWORD)actual_send_rate;
                if (last_passed_send_rate != 0 &&
                    last_passed_send_rate > last_failed_send_rate) break;

                // Calculate new send rate
                if (last_passed_send_rate == 0)
                {
                    req_send_rate = last_failed_send_rate / 2;
                    if (req_send_rate <= tolerance_kbytespersec) {
                        last_passed_send_rate = tolerance_kbytespersec / 2; // between 0 and tolerance
                        break;
                    }
                }
                else
                {
                    if (last_failed_send_rate - last_passed_send_rate <= tolerance_kbytespersec) break;

                    DWORD new_req_send_rate = last_passed_send_rate
                        + (last_failed_send_rate - last_passed_send_rate)/2;
                    if (new_req_send_rate == req_send_rate)
                        break;
                    else
                        req_send_rate = new_req_send_rate;
                }
            }
            else
            {
                // Save this result for reporting later on
                if (last_passed_send_rate == 0 ||
                    result_ServerBackup.time_array[0] > result_Server.time_array[0])
                {
                    CopyTestResult(&result_ClientBackup, &result_Client);
                    CopyTestResult(&result_ServerBackup, &result_Server);
                }

                if (last_passed_send_rate < actual_send_rate || last_passed_send_rate == 0)
                {
                    last_passed_send_rate = (DWORD)actual_send_rate;

                    CopyTestResult(&result_ClientBackup, &result_Client);
                    CopyTestResult(&result_ServerBackup, &result_Server);
                }
                if (last_failed_send_rate == 0)
                    // Packets were already sent at max rate
                    break;

                if (last_passed_send_rate > last_failed_send_rate) break;
                if (last_failed_send_rate - last_passed_send_rate <= tolerance_kbytespersec) break;

                DWORD new_req_send_rate = last_passed_send_rate
                    + (last_failed_send_rate - last_passed_send_rate)/2;
                if (new_req_send_rate == req_send_rate)
                    break;
                else
                    req_send_rate = new_req_send_rate;
            }

            Log(DEBUG_MSG, TEXT("  New send rate: %ld Kbytes/s (pass=%ld, fail=%ld)"),
                req_send_rate / 1000, last_passed_send_rate / 1000, last_failed_send_rate / 1000);

            // If you're receiving large packets, then pause for the system to restore its reassembly buffers
            if (bufsize > MTUBytes - (IPHeaderBytes + UDPHeaderBytes))
            {
                Sleep(ReassemblyTimeoutMs);
            }
        }

        result.LogResult(
            bufsize,                                // Send size (in bytes)
            bufsize,                                // Recv size (in bytes)
            result_ServerBackup.sent_array[0],      // Number of packets sent
            result_ServerBackup.time_array[0],      // Send time (in milliseconds)
            result_ClientBackup.recvd_array[0],     // Number of packets recv'd
            result_ClientBackup.time_array[0],      // Recv time (in milliseconds)
            0,                                      // Latency (us)
            0,                                      // Jitter (us)
            result_ClientBackup.cpuutil_array[0],   // Idle time (ms)
            result_ClientBackup.time_array[0],      // Total test time (ms)
            FALSE,                                  // Mark it
            consumedSysCpu                          // CPU usage of the spinning thread
        );


    }
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);

    FreeTestResult(&result_ClientBackup);
    FreeTestResult(&result_ServerBackup);

    Log(DEBUG_MSG, TEXT("-TestUDPRecvThroughput()"));

    if(rateResult == TPR_PASS)
       return nResult;
    else
      return rateResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestUDPRecvPacketLoss(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    if(g_fConsumeCpu || g_fTestCpuUtilOptionSet)
    {
        Log(REQUIRED_MSG, TEXT("-e and -o options are not applicable to the test"));
        Log(REQUIRED_MSG, TEXT("Skipping the test..."));
        return TPR_SKIP;
    }

    Log(DEBUG_MSG, TEXT("+TestUDPRecvPacketLoss()"));
    FileAddNewTestBreak(TEXT("UDP Recv Packet Loss"));

    ResultLog result(
        (TCHAR*)lpFTE->lpDescription,
        APPLY_TO_CLIENT(SHOW_RECV_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_BYTES_RECVD) |
        APPLY_TO_CLIENT(SHOW_RECV_RATE) |
        APPLY_TO_CLIENT(SHOW_SEND_RATE) |
        APPLY_TO_CLIENT(SHOW_CPU_UTIL) |
        APPLY_TO_CLIENT(SHOW_PKT_LOSS) |
        APPLY_TO_CLIENT(HIGHLIGHT_SEND_RATE)
        );

    TestResult result_Client, result_Server;
    if (!AllocTestResult(1, &result_Client)) goto Cleanup;
    if (!AllocTestResult(1, &result_Server)) goto Cleanup;

    result.ShowHeader();

    for (INT i=0; i<g_nTestBufferCountUdp; i+=1)
    {
        DWORD bufsize = g_aTestBufferSizesUdp[i];
        DWORD numbufs = g_dwNumberOfBuffers;

        DWORD req_send_rate =
            g_dwReqSendRateKbps != 0 ?
            g_dwReqSendRateKbps * 1000 / 8 : 125000; // 125000 Kbytes/s = 1 Mbps
        DWORD max_send_rate = 0;
        DWORD last_passed_send_rate = 0;
        DWORD last_failed_send_rate = 0;

        for (;;)
        {
            ServiceError err;

            // run the test
            nResult = RunTest(
                Send,               // Test type
                SOCK_DGRAM,         // SOCK_STREAM or SOCK_DGRAM
                g_wUseIpVer,        // IP version
                FALSE,              // Disable Nagle setting (FALSE leaves Nagle on)
                -1,                 // Socket send buffer (< 0 leaves at default)
                g_iUdpRcvQueueBytes,// Socket recv buffer (< 0 leaves at default)
                1,                  // Number of iterations
                1,                  // Send total (bytes)
                1,                  // Send packet/buffer size (bytes)
                bufsize * numbufs,  // Receive total (bytes)
                bufsize,            // Receive packet/buffer size (bytes)
                req_send_rate,      // Requested send rate (bytes/s)
                0,                  // Requested send rate at client (bytes/s)
                0,                  // Requested recv rate (bytes/s)
                0,                  // Requested recv rate at client (bytes/s)
                0,                  // CPU usage parameter is not used  
                &result_Client,     // Client result array
                &result_Server,     // Server result array
                &err
            );

            // If not all packets were received, can adjust client-time to compensate for
            // the timeout
            if (result_Client.recvd_array[0] != result_Server.sent_array[0]) {
                if (result_Client.time_array[0] >= CONNTRACK_TIMEOUT_MS)
                    result_Client.time_array[0] -= CONNTRACK_TIMEOUT_MS;
                else
                    result_Client.time_array[0] = 0;

                if (result_Client.cpuutil_array[0] >= CONNTRACK_TIMEOUT_MS)
                    result_Client.cpuutil_array[0] -= CONNTRACK_TIMEOUT_MS;
                else
                    result_Client.cpuutil_array[0] = 0;
            }


            // To avoid any problems with divide-by-zero
            if (result_Client.time_array[0] < 1) result_Client.time_array[0] = 1;
            if (result_Server.time_array[0] < 1) result_Server.time_array[0] = 1;

            if (nResult == TPR_PASS)
            {
                Log(DEBUG_MSG,   TEXT("    Client:"));
                Log(DEBUG_MSG,      TEXT("        Packets/buffers sent: %ld (%ld total bytes)"),
                    result_Client.sent_array[0], result_Client.sent_array[0] * bufsize);
                Log(DEBUG_MSG,      TEXT("        Packets/buffers rcvd: %ld (%ld total bytes)"),
                    result_Client.recvd_array[0], result_Client.recvd_array[0] * bufsize);
                Log(DEBUG_MSG,   TEXT("        Time: %ld ms (%ld Kbytes/s)"),
                    result_Client.time_array[0],
                    bufsize * result_Client.recvd_array[0] / result_Client.time_array[0]);
                Log(DEBUG_MSG,   TEXT("    Server:"));
                Log(DEBUG_MSG,      TEXT("        Packets/buffers sent: %ld (%ld total bytes)"),
                    result_Server.sent_array[0], result_Server.sent_array[0] * bufsize);
                Log(DEBUG_MSG,      TEXT("        Packets/buffers rcvd: %ld (%ld total bytes)"),
                    result_Server.recvd_array[0], result_Server.recvd_array[0] * bufsize);
                Log(DEBUG_MSG,   TEXT("        Server time: %d ms (%ld Kbytes/s)"),
                    result_Server.time_array[0],
                    bufsize * result_Server.sent_array[0] / result_Server.time_array[0]);
            }
            else
            {
                Log(ERROR_MSG, TEXT("TestUDPRecvPacketLoss test Failed"));
                goto Cleanup;
            }

            // Implement divide and conquer alg to find the next rate at which the server
            // should send packets to the client
            double actual_send_rate = (double)(bufsize * result_Server.sent_array[0]) * 1000. / result_Server.time_array[0];
            double actual_recv_rate = (double)(bufsize * result_Client.recvd_array[0]) * 1000. / result_Client.time_array[0];

            Log(DEBUG_MSG, TEXT("Requested rate = %ld Kbps (Actual = %.2f Kbps)"),
                req_send_rate * 8 / 1000,
                actual_send_rate * 8 / 1000);

            result.LogResult(
                bufsize,                        // Send size (in bytes)
                bufsize,                        // Recv size (in bytes)
                result_Server.sent_array[0],    // Number of packets sent
                result_Server.time_array[0],    // Send time (in milliseconds)
                result_Client.recvd_array[0],   // Number of packets recv'd
                result_Client.time_array[0],    // Recv time (in milliseconds)
                0,                              // Latency (us)
                0,                              // Jitter (us)
                result_Client.cpuutil_array[0], // Idle time (ms)
                result_Client.time_array[0],    // Total test time (ms)
                TRUE,                           // Mark it
                0                               // spinning thread cpu usage not used
            );

            // Terminate after first iteration if rate was selected by the user
            // on command line.
            if (g_dwReqSendRateKbps != 0) {
                break;
            }

            if (req_send_rate == 0) break;
            if (actual_send_rate + 125000/2 < req_send_rate)
                req_send_rate = 0;
            else
                req_send_rate += 125000;

            // If you're receiving large packets, then pause for the system to restore its reassembly buffers
            if (bufsize > MTUBytes - (IPHeaderBytes + UDPHeaderBytes))
            {
                Sleep(ReassemblyTimeoutMs);
            }
        }
    }

Cleanup:
    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);

    Log(DEBUG_MSG, TEXT("-TestUDPRecvPacketLoss()"));

    return nResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestUDPPing(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    if(g_fConsumeCpu || g_fTestCpuUtilOptionSet)
    {
        Log(REQUIRED_MSG, TEXT("-e and -o options are not applicable to the test"));
        Log(REQUIRED_MSG, TEXT("Skipping the test..."));
        return TPR_SKIP;
    }

    Log(DEBUG_MSG, TEXT("+TestUDPPing()"));
    FileAddNewTestBreak(TEXT("UDP Ping"));

    ResultLog result(
        (TCHAR*)lpFTE->lpDescription,
        APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_RECV_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_BYTES_SENT) |
        APPLY_TO_CLIENT(SHOW_BYTES_RECVD) |
        APPLY_TO_CLIENT(SHOW_LATENCY)
        );
    result.ShowHeader();

    TestResult result_Client, result_Server;
    if (!AllocTestResult(g_dwNumberOfPingBuffers, &result_Client)) goto Cleanup;
    if (!AllocTestResult(g_dwNumberOfPingBuffers, &result_Server)) goto Cleanup;

    for (INT i=0; i<g_nTestBufferCountUdp; i+=1)
    {
        DWORD bufsize = g_aTestBufferSizesUdp[i];
        DWORD numbufs = 1;
        ServiceError err;

        // run the test
        nResult = RunTest(
            Send,           // Test type
            SOCK_DGRAM,     // SOCK_STREAM or SOCK_DGRAM
            g_wUseIpVer,    // IP version
            FALSE,          // Disable Nagle setting (FALSE leaves Nagle on)
            -1,             // Socket send buffer (< 0 leaves at default)
            -1,             // Socket recv buffer (< 0 leaves at default)
            g_dwNumberOfPingBuffers, // Number of iterations
            bufsize,        // Send total (bytes)
            bufsize,        // Send packet/buffer size (bytes)
            bufsize,        // Receive total (bytes)
            bufsize,        // Receive packet/buffer size (bytes)
            0,              // Requested send rate (bytes/s)
            0,              // Requested send rate at client (bytes/s)
            0,              // Requested recv rate (bytes/s)
            0,              // Requested recv rate at client (bytes/s)
            0,              // CPU usage parameter is not used
            &result_Client, // Client result array
            &result_Server, // Server result array
            &err
        );

        if (nResult == TPR_PASS)
        {
            // Compute latency
            DWORD pings;
            for (pings = 0; pings < g_dwNumberOfPingBuffers; pings += 1)
                if (result_Client.recvd_array[pings] == 0) break;

            if (pings != g_dwNumberOfPingBuffers && pings > 1)
                for (DWORD k = pings - 1; k < g_dwNumberOfPingBuffers; k += 1)
                    result_Client.total_time -= result_Client.time_array[k];

            // Make sure to save-guard against divide-by-zero
            DWORD latency = pings > 0 ? (result_Client.total_time * 1000) / pings : 0;

            result.LogResult(
                bufsize,    // Send size (in bytes)
                bufsize,    // Recv size (in bytes)
                pings,      // Number of packets sent
                0,          // Send time (in milliseconds)
                pings,      // Number of packets recv'd
                0,          // Recv time (in milliseconds)
                latency,    // Latency (us)
                0,          // Jitter (us)
                0,          // Idle time (ms)
                0,          // Total test time (ms)
                TRUE,       // Mark it
                0           // spinning thread cpu usage not used
                );
        }
        else
        {
            Log(ERROR_MSG, TEXT("TestUDPPing test Failed"));
            goto Cleanup;
        }
    }
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);

    Log(DEBUG_MSG, TEXT("-TestUDPPing()"));

    return nResult;
}

// --------------------------------------------------------------------
TESTPROCAPI RunTest(
    TestType param_TestType,
    INT param_Protocol,         // SOCK_STREAM or SOCK_DGRAM
    INT param_IpVer,            // 4 or 6
    BOOL param_SockNagleOff,    // Nagle off, FALSE will preserve default (= Nagle enabled)
    INT param_SockSendBuf,      // Send and
    INT param_SockRecvBuf,      //  Recv buffers for the socket, < 0 will preserve defaults
    INT param_NumIterats,
    INT param_SendTotal,
    INT param_SendPacket,
    INT param_RecvTotal,
    INT param_RecvPacket,
    INT param_SendReqRate,
    INT param_SendReqRateClient,
    INT param_RecvReqRate,
    INT param_RecvReqRateClient,
    DWORD param_cpu_util,   
    TestResult* param_ClientResult,
    TestResult* param_ServerResult,
    ServiceError* param_pError
)
// --------------------------------------------------------------------
{
    MIB_TCPSTATS mibTcpStats_before_v4, mibTcpStats_after_v4;
    MIB_UDPSTATS mibUdpStats_before_v4, mibUdpStats_after_v4;
    MIB_IPSTATS mibIpStats_before_v4, mibIpStats_after_v4;

    MIB_TCPSTATS mibTcpStats_before_v6, mibTcpStats_after_v6;
    MIB_UDPSTATS mibUdpStats_before_v6, mibUdpStats_after_v6;
    MIB_IPSTATS mibIpStats_before_v6, mibIpStats_after_v6;

    INT TestResult = TPR_FAIL; // assume the worst
    *param_pError = Test_Error;

    Log(DEBUG_MSG, TEXT("+RunTest()"));

#ifdef UNDER_CE
    int iOldThreadPriority = CeGetThreadPriority(GetCurrentThread());
    CeSetThreadPriority(GetCurrentThread(), g_iThreadPriority);
#endif

    SOCKET* listening_sock_array = NULL;
    INT listening_sock_count = 0;
    SOCKET ctl_sock = INVALID_SOCKET;
    SOCKET test_sock = INVALID_SOCKET;
    SOCKET accept_sock = INVALID_SOCKET;
    DWORD dwRet = 0;
    CONNTRACK_HANDLE hConnTrack;

    // initialize responses
    ClearTestResult(param_ClientResult);
    ClearTestResult(param_ServerResult);

    // Initialize ConnTrack - connection tracker
    ConnTrack_Init(1, CONNTRACK_TIMEOUT_MS);

    // form service request
    ServiceRequest req;
    req.version = COMMPROT_VERSION;
    req.ip_version = param_IpVer;
    req.protocol = param_Protocol;
    req.port = 0;
    req.sock_nagle_off = param_SockNagleOff;
    req.sock_send_buf = param_SockRecvBuf;
    req.sock_recv_buf = param_SockSendBuf;
    req.num_iterations = param_NumIterats;
    req.type = param_TestType == Send ? Receive : Send;
    req.send_req_rate = param_SendReqRate;
    req.send_total_size = param_RecvTotal;
    req.send_packet_size = param_RecvPacket;
    req.recv_req_rate = param_RecvReqRate;
    req.recv_total_size = param_SendTotal;
    req.recv_packet_size = param_SendPacket;

    // Initialize test
    PTestForm ptest_form;
    TestInit(
        param_TestType,
        param_Protocol,
        param_NumIterats,
        param_SendTotal,  // total size of bytes to send per iteration
        param_SendPacket, //  packet/buffer to use to send
        param_RecvTotal,  // total size of bytes to receive per iteration
        param_RecvPacket, //  packets/buffer to use to receive
        g_fEnableAltCpuMon,   // alternative CPU monitor
        &ptest_form
    );

    if ((param_TestType == Send && param_Protocol == SOCK_DGRAM) ||
        param_TestType == Receive)
    {
        if (ListenSocket(
            (WORD)req.ip_version,
            req.protocol,
            &req.port,
            &listening_sock_array,
            &listening_sock_count,
            &dwRet) == SOCKET_ERROR)
        {
            Log(ERROR_MSG, TEXT("ListenSocket() failed, WSAError=%ld"), dwRet);
            goto Cleanup;
        }
        if (listening_sock_count != 1)
        {
            Log(ERROR_MSG, TEXT("ListenSocket() returned array of %d sockets, expecting only 1"));
            goto Cleanup;
        }
        test_sock = listening_sock_array[0];
    }

    // Send a request
    ctl_sock = ConnectSocket(
        INVALID_SOCKET,
        g_tszServerAddressString,
        SERVER_SERVICE_REQUEST_PORT,
        g_wUseIpVer,
        SOCK_STREAM,
        FALSE,
        NULL, NULL, NULL, NULL,
        &dwRet);
    if (ctl_sock == SOCKET_ERROR)
    {
        Log(ERROR_MSG, TEXT("ConnectSocket() for control connection failed, WSAError=%ld"), dwRet);
        goto Cleanup;
    }
    Log(DEBUG_MSG, TEXT("Established a control connection with the server"));

    // This is a control channel - turn nagle off
    if (FALSE == ApplySocketSettings(
            ctl_sock,
            SOCK_STREAM,
            g_fKeepAliveControlChannel == TRUE? TRUE: FALSE,   // Turn on KEEPALIVE
            TRUE,   // turn nagle off
            -1, -1, // leave send/recv buffer at default
            &dwRet))
    {
        Log(ERROR_MSG, TEXT("ApplySocketSettings() failed, WSA Error: %ld"), dwRet);
        goto Cleanup;
    }

    if (SendServiceRequest(ctl_sock, &req, &dwRet) == SOCKET_ERROR)
    {
        Log(ERROR_MSG, TEXT("SendServiceRequest() failed, WSAError=%ld"), dwRet);
        goto Cleanup;
    }

    // Get a response
    ServiceResponse resp;
    if (ReceiveServiceResponse(ctl_sock, &resp, &dwRet) == SOCKET_ERROR)
    {
        if (dwRet == 0)
        {
            Log(ERROR_MSG, TEXT("ReceiveServiceReponse() timed-out"));
            *param_pError = Test_Comm_Timeout;
        }
        else
            Log(ERROR_MSG, TEXT("ReceiveServiceReponse() failed, WSAError=%ld"), dwRet);
        goto Cleanup;
    }
    if (resp.error != Test_Success)
    {
        Log(ERROR_MSG, TEXT("Service response contained error: %s (%u)"),
            SERVICE_ERROR_STRING[(UINT)resp.error], (UINT)resp.error);
        goto Cleanup;
    }

    Log(DEBUG_MSG, TEXT("Received success response from server"));

    if (param_TestType == Send)
    {
        accept_sock = ConnectSocketByAddrFromSocket(
            test_sock,
            ctl_sock,
            resp.port,
            (WORD)param_IpVer,
            param_Protocol,
            FALSE,
            NULL, NULL, NULL, NULL,
            &dwRet);
        if (accept_sock == SOCKET_ERROR)
        {
            Log(ERROR_MSG, TEXT("ConnectSocketByAddr() failed, WSAError=%ld"), dwRet);
            goto Cleanup;
        }
    }
    else if (param_TestType == Receive)
    {
        if (param_Protocol == SOCK_STREAM)
        {
            // Pickup connection
            accept_sock = AcceptConnection(
                listening_sock_array,
                listening_sock_count,
                NULL,
                TEST_CONNECTION_TIMEOUT_MS,
                &dwRet);
            if (accept_sock == SOCKET_ERROR)
            {
                Log(ERROR_MSG, TEXT("AcceptConnection() failed, WSAError=%ld"),
                    dwRet);
                goto Cleanup;
            }
        }
        else
        {
            accept_sock = ConnectSocketByAddrFromSocket(
                test_sock, //listening_sock_array[0],
                ctl_sock,
                resp.port,
                (WORD)req.ip_version,
                req.protocol,
                FALSE,
                NULL, NULL, NULL, NULL,
                &dwRet);
            if (accept_sock == SOCKET_ERROR)
            {
                Log(ERROR_MSG, TEXT("ConnectSocketByAddrFromSocket() failed, WSAError=%ld"),
                    dwRet);
                goto Cleanup;
            }
            req.port = GetSocketLocalPort(accept_sock);
            if (req.port == 0)
            {
                Log(ERROR_MSG, TEXT("GetSocketLocalPort() returned port %d. Unable to continue."),
                    req.port);
                goto Cleanup;
            }
        }
    }
    Log(DEBUG_MSG, TEXT("Local port = %d"), GetSocketLocalPort(accept_sock));

    // Apply all required settings to test socket
    if (FALSE == ApplySocketSettings(
            accept_sock,
            param_Protocol,
            FALSE,  // No need for keepalive
            param_SockNagleOff, // nagle off
            param_SockSendBuf,  // send buffer for socket
            param_SockRecvBuf,  // recv buffer for socket
            &dwRet))
    {
        Log(ERROR_MSG, TEXT("ApplySocketSettings() failed, WSAError=%ld"), dwRet);
        goto Cleanup;
    }

    // Send setup confirmation to server
    if (SOCKET_ERROR == SendGoMsg(ctl_sock, &dwRet))
    {
        Log(ERROR_MSG, TEXT("SendGoMsg() failed, WSAError=%ld"), dwRet);
        goto Cleanup;
    }

    // Wait for setup confirmation from server
    if (SOCKET_ERROR == ReceiveGoMsg(ctl_sock, &dwRet))
    {
        if (dwRet == 0)
        {
            Log(ERROR_MSG, TEXT("ReceiveGoMsg() timed-out"));
            *param_pError = Test_Comm_Timeout;
        }
        else
            Log(ERROR_MSG, TEXT("ReceiveGoMsg() failed, WSAError=%ld"), dwRet);
        goto Cleanup;
    }

    hConnTrack = ConnTrack_Insert(accept_sock);

    //
    // Gather some IPHlpAPI statistics BEFORE the test. We are interested in IP, TCP, UDP
    //
    if(g_fGatherIphlpapiStats == TRUE)
    {
        GetTCPStats(&mibTcpStats_before_v4, &mibTcpStats_before_v6);
        GetUDPStats(&mibUdpStats_before_v4, &mibUdpStats_before_v6);
        GetIPStats(&mibIpStats_before_v4, &mibIpStats_before_v6);
    }

#ifdef UNDER_CE
#define PROFILE_FREQ_US 200
    if (g_fEnableMonteCarlo) ProfileStart(PROFILE_FREQ_US, PROFILE_BUFFER);
#endif

    if(param_cpu_util != 0)
    {
        dwRet = TestSendRecvRate(
            ptest_form,
            accept_sock,           
            param_cpu_util,
            hConnTrack,
            param_ClientResult);  
    }
    else
    {
    dwRet = Test(
        ptest_form,
        accept_sock,
        param_SendReqRateClient,
        param_RecvReqRateClient,
        hConnTrack,
        param_ClientResult);
    }

#ifdef UNDER_CE
    if (g_fEnableMonteCarlo) ProfileStop();
#endif

    //
    // Gather some IPHlpAPI statistics AFTER the test. We are interested in IP, TCP, UDP
    //
    if(g_fGatherIphlpapiStats == TRUE)
    {
        GetTCPStats(&mibTcpStats_after_v4, &mibTcpStats_after_v6);
        GetUDPStats(&mibUdpStats_after_v4, &mibUdpStats_after_v6);
        GetIPStats(&mibIpStats_after_v4, &mibIpStats_after_v6);

        PrintRelevantStats(&mibTcpStats_before_v4, &mibUdpStats_before_v4, &mibIpStats_before_v4,
                           &mibTcpStats_after_v4, &mibUdpStats_after_v4, &mibIpStats_after_v4,
                           &mibTcpStats_before_v6, &mibUdpStats_before_v6, &mibIpStats_before_v6,
                           &mibTcpStats_after_v6, &mibUdpStats_after_v6, &mibIpStats_after_v6
                           );
    }

    if ( hConnTrack != NULL &&
         ConnTrack_Remove(hConnTrack) )
    {
        // ConnTrack_Remove returns TRUE if it closed socket prior to removal
        *param_pError = Test_Run_Timeout;
    }
    else if (dwRet == SOCKET_ERROR)
    {
        Log(DEBUG_MSG, TEXT("Test() indicated failure. ConnTrack WAS%s running."),
            hConnTrack != NULL ? TEXT("") : TEXT(" NOT"));
    }
    else
        *param_pError = Test_Success;

    // Get test result
    TestResponse tresp;
    tresp.result = param_ServerResult;

    //
    // Before waiting for test response, send a GO message to server
    //
    // This will ensure the client is finished with the test and is ready to accept
    // results.
    //
    // Note that ReceiveGoMsg on the server will wait for RECV_TIMEOUT_SEC before timing
    // out
    //
    if (SOCKET_ERROR == SendGoMsg(ctl_sock, &dwRet))
    {
        Log(ERROR_MSG, TEXT("SendGoMsg() failed - Test response not received, WSAError=%ld"), dwRet);
        goto Cleanup;
    }

    if (ReceiveTestResponse(ctl_sock, &tresp, &dwRet) == SOCKET_ERROR)
    {
        if (dwRet == 0)
        {
            Log(ERROR_MSG, TEXT("ReceiveTestResponse() timed-out"));
            *param_pError = Test_Comm_Timeout;
        }
        else
        {
            Log(ERROR_MSG, TEXT("ReceiveTestResponse() failed, WSAError=%ld"), dwRet);
            *param_pError = Test_Error;
        }
        goto Cleanup;
    }
    else
        *param_pError = tresp.error;

    if (*param_pError == Test_Success ||
        *param_pError == Test_Run_Timeout)
        TestResult = TPR_PASS;

Cleanup:
    // Cleanup
    TestDeinit(&ptest_form);
    ConnTrack_Deinit();
#ifdef UNDER_CE
    CeSetThreadPriority(GetCurrentThread(), iOldThreadPriority);
#endif

    if (listening_sock_count > 0 && listening_sock_array != NULL)
    {
        for (INT i=0; i<listening_sock_count; i+=1)
            DisconnectSocket(listening_sock_array[i]);
        delete [] listening_sock_array;
    }
    if (ctl_sock != SOCKET_ERROR && ctl_sock != INVALID_SOCKET)
        DisconnectSocket(ctl_sock);
    if (test_sock != SOCKET_ERROR && test_sock != INVALID_SOCKET)
        DisconnectSocket(test_sock);
    if (accept_sock != SOCKET_ERROR && accept_sock != INVALID_SOCKET)
        DisconnectSocket(accept_sock);

    Log(DEBUG_MSG, TEXT("-RunTest(); returning %s"),
        TestResult == TPR_PASS ? TEXT("TPR_PASS") : TEXT("TPR_FAIL"));

    return TestResult;
}

#ifdef UNDER_CE
void GetTCPStats(PMIB_TCPSTATS stats_v4, PMIB_TCPSTATS stats_v6)
{
    GetTcpStatisticsEx(stats_v6, AF_INET6);
    GetTcpStatistics(stats_v4);
}

void GetUDPStats(PMIB_UDPSTATS stats_v4, PMIB_UDPSTATS stats_v6)
{
    GetUdpStatisticsEx(stats_v6, AF_INET6);
    GetUdpStatistics(stats_v4);
}

void GetIPStats(PMIB_IPSTATS stats_v4, PMIB_IPSTATS stats_v6)
{
    GetIpStatisticsEx(stats_v6, AF_INET6);
    GetIpStatistics(stats_v4);
}

void PrintRelevantStats(PMIB_TCPSTATS tcpv4_stats_before,
                        PMIB_UDPSTATS udpv4_stats_before,
                        PMIB_IPSTATS ipv4_stats_before,
                        PMIB_TCPSTATS tcpv4_stats_after,
                        PMIB_UDPSTATS udpv4_stats_after,
                        PMIB_IPSTATS ipv4_stats_after,
                        PMIB_TCPSTATS tcpv6_stats_before,
                        PMIB_UDPSTATS udpv6_stats_before,
                        PMIB_IPSTATS ipv6_stats_before,
                        PMIB_TCPSTATS tcpv6_stats_after,
                        PMIB_UDPSTATS udpv6_stats_after,
                        PMIB_IPSTATS ipv6_stats_after
                        )
{
    // TCP stats
    Log(REQUIRED_MSG, TEXT("Network statistics (for the duration of this test):"));
    Log(REQUIRED_MSG, TEXT("Connect Attempt Fails: \r\nTCPv4=%d\tTCPv6=%d"), tcpv4_stats_after->dwAttemptFails - tcpv4_stats_before->dwAttemptFails, tcpv6_stats_after->dwAttemptFails - tcpv6_stats_before->dwAttemptFails);
    Log(REQUIRED_MSG, TEXT("Reset Connections: \r\nTCPv4=%d\tTCPv6=%d"), tcpv4_stats_after->dwEstabResets - tcpv4_stats_before->dwEstabResets, tcpv6_stats_after->dwEstabResets - tcpv6_stats_before->dwEstabResets);
    Log(REQUIRED_MSG, TEXT("Current Connections: \r\nTCPv4=%d\tTCPv6=%d"), tcpv4_stats_after->dwCurrEstab - tcpv4_stats_before->dwCurrEstab, tcpv6_stats_after->dwCurrEstab - tcpv6_stats_before->dwCurrEstab);
    Log(REQUIRED_MSG, TEXT("Errors Received: \r\nTCPv4=%d\tTCPv6=%d"), tcpv4_stats_after->dwInErrs - tcpv4_stats_before->dwInErrs, tcpv6_stats_after->dwInErrs - tcpv6_stats_before->dwInErrs);
    Log(REQUIRED_MSG, TEXT("Sgmnts sent with Reset Flag: \r\nTCPv4=%d\tTCPv6=%d"), tcpv4_stats_after->dwOutRsts - tcpv4_stats_before->dwOutRsts, tcpv6_stats_after->dwOutRsts - tcpv6_stats_before->dwOutRsts);

    // UDP stats
    Log(REQUIRED_MSG, TEXT("Datagrams Received: \r\nUDPv4=%d\tUDPv6=%d"), udpv4_stats_after->dwInDatagrams - udpv4_stats_before->dwInDatagrams, udpv6_stats_after->dwInDatagrams - udpv6_stats_before->dwInDatagrams);
    Log(REQUIRED_MSG, TEXT("Receive Errors: \r\nUDPv4=%d\tUDPv6=%d"), udpv4_stats_after->dwInErrors - udpv4_stats_before->dwInErrors, udpv6_stats_after->dwInErrors - udpv6_stats_before->dwInErrors);
    Log(REQUIRED_MSG, TEXT("Datagrams sent: \r\nUDPv4=%d\tUDPv6=%d"), udpv4_stats_after->dwOutDatagrams - udpv4_stats_before->dwOutDatagrams, udpv6_stats_after->dwOutDatagrams - udpv6_stats_before->dwOutDatagrams);
    Log(REQUIRED_MSG, TEXT("Num UDP entries: \r\nUDPv4=%d\tUDPv6=%d"), udpv4_stats_after->dwNumAddrs - udpv4_stats_before->dwNumAddrs, udpv6_stats_after->dwNumAddrs - udpv6_stats_before->dwNumAddrs);

    // IP stats
    Log(REQUIRED_MSG, TEXT("Packets Received: \r\nIPv4=%d\tIPv6=%d"), ipv4_stats_after->dwInReceives - ipv4_stats_before->dwInReceives, ipv6_stats_after->dwInReceives - ipv6_stats_before->dwInReceives);
    Log(REQUIRED_MSG, TEXT("Received Header Errors: \r\nIPv4=%d\tIPv6=%d"), ipv4_stats_after->dwInHdrErrors - ipv4_stats_before->dwInHdrErrors, ipv6_stats_after->dwInHdrErrors - ipv6_stats_before->dwInHdrErrors);
    Log(REQUIRED_MSG, TEXT("Unknown Protocols received: \r\nIPv4=%d\tIPv6=%d"), ipv4_stats_after->dwInUnknownProtos - ipv4_stats_before->dwInUnknownProtos, ipv6_stats_after->dwInUnknownProtos - ipv6_stats_before->dwInUnknownProtos);
    Log(REQUIRED_MSG, TEXT("Received Packets Discarded: \r\nIPv4=%d\tIPv6=%d\r\n"), ipv4_stats_after->dwInDiscards - ipv4_stats_before->dwInDiscards, ipv6_stats_after->dwInDiscards - ipv6_stats_before->dwInDiscards);
}

#else
void GetTCPStats(PMIB_TCPSTATS stats_v4, PMIB_TCPSTATS stats_v6)
{}

void GetUDPStats(PMIB_UDPSTATS stats_v4, PMIB_UDPSTATS stats_v6)
{}

void GetIPStats(PMIB_IPSTATS stats_v4, PMIB_IPSTATS stats_v6)
{}

void PrintRelevantStats(PMIB_TCPSTATS tcpv4_stats_before,
                        PMIB_UDPSTATS udpv4_stats_before,
                        PMIB_IPSTATS ipv4_stats_before,
                        PMIB_TCPSTATS tcpv4_stats_after,
                        PMIB_UDPSTATS udpv4_stats_after,
                        PMIB_IPSTATS ipv4_stats_after,
                        PMIB_TCPSTATS tcpv6_stats_before,
                        PMIB_UDPSTATS udpv6_stats_before,
                        PMIB_IPSTATS ipv6_stats_before,
                        PMIB_TCPSTATS tcpv6_stats_after,
                        PMIB_UDPSTATS udpv6_stats_after,
                        PMIB_IPSTATS ipv6_stats_after
                        )
{}
#endif

