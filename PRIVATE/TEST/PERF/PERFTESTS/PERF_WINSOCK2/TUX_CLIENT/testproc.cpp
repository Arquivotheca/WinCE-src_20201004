//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
// testproc.cpp

#include "tuxmain.h"
#include <winbase.h>
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
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    Log(DEBUG_MSG, TEXT("+TestTCPSendThroughput()"));

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

    for (INT i=0; i<g_nTestBufferCount; i+=1)
    {
        DWORD bufsize = g_aTestBufferSizes[i];
        DWORD numbufs = g_dwNumberOfBuffers;

        ServiceError err;

        // run the test
        nResult = RunTest(
            Send,           // Test type
            SOCK_STREAM,    // SOCK_STREAM or SOCK_DGRAM
            g_wUseIpVer,    // IP version
            lpFTE->dwUserData & TestOpt_NagleOff, // Disable Nagle setting (FALSE leaves Nagle on)
            -1,             // Socket send buffer (< 0 leaves at default)
            -1,             // Socket recv buffer (< 0 leaves at default)
            1,              // Number of iterations
            bufsize * numbufs,  // Send total (bytes)
            bufsize,            // Send packet/buffer size (bytes)
            1,              // Receive total (bytes)
            1,              // Receive packet/buffer size (bytes)
            0,                                  // Requested send rate (bytes/s); note if 0 will go max
            (g_dwReqSendRateKbps * 1000 / 8),   // Requested send rate at client (bytes/s)
            (g_dwReqRecvRateKbps * 1000 / 8),   // Requested recv rate (bytes/s)
            0,                                  // Requested recv rate at client (bytes/s)
            &result_Client,     // Client result array
            &result_Server,     // Server result array
            &err
        );

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

            result.LogResult(
                bufsize,                    // Send size (in bytes)
                0,                          // Recv size (in bytes)
                result_Client.sent_array[0],    // Number of packets sent
                result_Client.time_array[0],    // Send time (in milliseconds)
                0,                          // Number of packets recv'd
                0,                          // Recv time (in milliseconds)
                0,                          // Latency (us)
                0,                          // Jitter (us)
                result_Client.cpuutil_array[0], // Idle time (ms)
                result_Client.time_array[0],    // Total test time (ms)
                TRUE                        // Mark it
                );
        }
        else
        {
            Log(ERROR_MSG, TEXT("Communication between server and client failed; terminating test"));
            nResult = TPR_FAIL;
            goto Cleanup;
        }
    }
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);

    Log(DEBUG_MSG, TEXT("-TestTCPSendThroughput()"));
    
    return nResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestTCPRecvThroughput(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    Log(DEBUG_MSG, TEXT("+TestTCPRecvThroughput()"));

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

    for (INT i=0; i<g_nTestBufferCount; i+=1)
    {
        DWORD bufsize = g_aTestBufferSizes[i];
        DWORD numbufs = g_dwNumberOfBuffers;

        ServiceError err;
        
        // run the test
        nResult = RunTest(
            Send,           // Test type
            SOCK_STREAM,    // SOCK_STREAM or SOCK_DGRAM
            g_wUseIpVer,    // IP version
            lpFTE->dwUserData & TestOpt_NagleOff, // Disable Nagle setting (FALSE leaves Nagle on)
            -1,             // Socket send buffer (< 0 leaves at default)
            -1,             // Socket recv buffer (< 0 leaves at default)
            1,              // Number of iterations
            1,              // Send total (bytes)
            1,              // Send packet/buffer size (bytes)
            bufsize * numbufs,  // Receive total (bytes)
            bufsize,            // Receive packet/buffer size (bytes)
            (g_dwReqSendRateKbps * 1000 / 8),   // Requested send rate (bytes/s); note if 0 will go max
            0,                                  // Requested send rate at client (bytes/s)
            0,                                  // Requested recv rate (bytes/s)
            (g_dwReqRecvRateKbps * 1000 / 8),   // Requested recv rate at client (bytes/s)
            &result_Client,  // Client result array
            &result_Server,  // Server result array
            &err
        );

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

            result.LogResult(
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
                TRUE                        // Mark it
                );
        }
        else
        {
            Log(ERROR_MSG, TEXT("Communication between server and client failed; terminating test"));
            nResult = TPR_FAIL;
            goto Cleanup;
        }
    }
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);

    Log(DEBUG_MSG, TEXT("-TestTCPRecvThroughput()"));
    
    return nResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestTCPPing(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    Log(DEBUG_MSG, TEXT("+TestTCPPing()"));

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
                TRUE        // Mark it
                );
        }
        else
        {
            Log(ERROR_MSG, TEXT("Communication between server and client failed; terminating test"));
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
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    Log(DEBUG_MSG, TEXT("+TestUDPSendThroughput()"));

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
                &result_Client,     // Client result array
                &result_Server,     // Server result array
                &err
            );

            // If not all packets were received, can adjust client-time to compensate for
            // the timeout
            if (result_Server.recvd_array[0] != result_Client.sent_array[0]) {
                if (result_Server.time_array[0] >= CONNTRACK_TIMEOUT_MS)
                    result_Server.time_array[0] -= CONNTRACK_TIMEOUT_MS;
                else
                    result_Server.time_array[0] = 0;

                if (result_Server.cpuutil_array[0] >= CONNTRACK_TIMEOUT_MS)
                    result_Server.cpuutil_array[0] -= CONNTRACK_TIMEOUT_MS;
                else
                    result_Server.cpuutil_array[0] = 0;
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
                Log(ERROR_MSG, TEXT("Communication between server and client failed; terminating test"));
                goto Cleanup;
            }

            result.LogResult(
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
                FALSE                           // Mark it?
            );


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

            // If you're receiving large packets, then pause for the system to restore its reassembly buffers
            if (bufsize > MTUBytes - (IPHeaderBytes + UDPHeaderBytes))
            {
                Sleep(ReassemblyTimeoutMs);
            }
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
            TRUE                            // Mark it?
            );
    }
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);

    FreeTestResult(&result_ClientBackup);
    FreeTestResult(&result_ServerBackup);

    Log(DEBUG_MSG, TEXT("-TestUDPSendThroughput()"));
    
    return nResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestUDPSendPacketLoss(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    Log(DEBUG_MSG, TEXT("+TestUDPSendPacketLoss()"));

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

        // choose a tolerance level
        DWORD tolerance_kbytespersec = 10000;

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
                Log(ERROR_MSG, TEXT("Communication between server and client failed; terminating test"));
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
                TRUE                            // Mark it
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

    Log(DEBUG_MSG, TEXT("+TestUDPRecvThroughput()"));

    ResultLog result(
        (TCHAR*)lpFTE->lpDescription,
        APPLY_TO_CLIENT(SHOW_RECV_PKT_SIZE) |
        APPLY_TO_CLIENT(SHOW_BYTES_RECVD) |
        APPLY_TO_CLIENT(SHOW_SEND_RATE) |
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
                Log(ERROR_MSG, TEXT("Communication between server and client failed; terminating test"));
                goto Cleanup;
            }

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
                FALSE                           // Mark it
            );

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
            bufsize,                        // Send size (in bytes)
            bufsize,                        // Recv size (in bytes)
            result_ServerBackup.sent_array[0],    // Number of packets sent
            result_ServerBackup.time_array[0],    // Send time (in milliseconds)
            result_ClientBackup.recvd_array[0],   // Number of packets recv'd
            result_ClientBackup.time_array[0],    // Recv time (in milliseconds)
            0,                              // Latency (us)
            0,                              // Jitter (us)
            result_ClientBackup.cpuutil_array[0], // Idle time (ms)
            result_ClientBackup.time_array[0],    // Total test time (ms)
            TRUE                            // Mark it
        );
    }
Cleanup:

    FreeTestResult(&result_Client);
    FreeTestResult(&result_Server);

    FreeTestResult(&result_ClientBackup);
    FreeTestResult(&result_ServerBackup);

    Log(DEBUG_MSG, TEXT("-TestUDPRecvThroughput()"));
    
    return nResult;
}

// --------------------------------------------------------------------
TESTPROCAPI TestUDPRecvPacketLoss(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    INT nResult;
    nResult = ProcessTuxMessages(uMsg, tpParam, lpFTE);
    if (nResult != TPR_PASS) return nResult;
    nResult = TPR_FAIL; // assume the worst

    Log(DEBUG_MSG, TEXT("+TestUDPRecvPacketLoss()"));

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

        // choose a tolerance level
        DWORD tolerance_kbytespersec = 10000;

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
                Log(ERROR_MSG, TEXT("Communication between server and client failed; terminating test"));
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
                TRUE                            // Mark it
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

    Log(DEBUG_MSG, TEXT("+TestUDPPing()"));

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
                TRUE        // Mark it
                );
        }
        else
        {
            Log(ERROR_MSG, TEXT("Communication between server and client failed; terminating test"));
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
    TestResult* param_ClientResult,
    TestResult* param_ServerResult,
    ServiceError* param_pError
)
// --------------------------------------------------------------------
{
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
            req.ip_version,
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
            param_IpVer,
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
                req.ip_version,
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

#ifdef UNDER_CE
#define PROFILE_FREQ_US 200
    if (g_fEnableMonteCarlo) ProfileStart(PROFILE_FREQ_US, PROFILE_BUFFER);
#endif

    dwRet = Test(
        ptest_form,
        accept_sock,
        param_SendReqRateClient,
        param_RecvReqRateClient,
        hConnTrack,
        param_ClientResult);

#ifdef UNDER_CE
    if (g_fEnableMonteCarlo) ProfileStop();
#endif

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

