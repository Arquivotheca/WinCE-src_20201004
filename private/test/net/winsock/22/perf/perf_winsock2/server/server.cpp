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
// server.cpp

#ifdef SUPPORT_IPV6
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <winsock.h>
#endif
#ifndef UNDER_CE
#include "service.h"
#endif
#include "server.h"
#include "common.h"
#include "loglib.h"
#include "consetup.h"
#include "commprot.h"
#include "conntrk.h"

//
// Don't really need these for Server app, but need for compilation (extern variable)
//
HWND g_hwndEdit = NULL;
HWND g_hMainWnd = NULL;

TCHAR  *g_tszInterfaceToBind;
BOOL   g_fHardBindToInterface;

////////////////////////////////////////////////////////
//
// Constants
//
const DWORD WORKER_THREAD_STACK_SIZE = 131072;

////////////////////////////////////////////////////////
//
// Global variables
//
HANDLE g_hServerStopEvent = NULL;
HANDLE g_hServerAvailableEvent = NULL;
HANDLE g_hServerDoneEvent = NULL;

struct WorkerThreadArgs
{
    ServiceRequest req;     // in
    SOCKET ctl_sock;        // in
    SOCKET test_sock;       // in
    TestResult result;      // in
    PTestForm ptest_form;   // in
    DWORD wsa_error;        // out
};

////////////////////////////////////////////////////////
//
// Implementation of the worker thread.
//
// Worker thread handles requests received by the
//   main application loop.
//
unsigned long __stdcall WorkerThread(void* pParams)
{
    WorkerThreadArgs* pArgs = (WorkerThreadArgs*)pParams;
    DWORD dwRet = 0; // Generic return value
    TestResponse test_resp;

    Log(DEBUG_MSG, TEXT("+WorkerThread()"));

    // Initialize Connection Tracker
    ConnTrack_Init(1, TEST_TIMEOUT_MS);

    // process service request
    if (pArgs->req.type == Send)
    {
        ////////////////////////////////////////////////////
        //
        // Send Test - client listens; server connects to client
        //

        pArgs->test_sock = ConnectSocketByAddrFromSocket(
            pArgs->test_sock,
            pArgs->ctl_sock,
            pArgs->req.port,
            (WORD)pArgs->req.ip_version,
            pArgs->req.protocol,
            FALSE,
            NULL, NULL, NULL, NULL,
            &pArgs->wsa_error);
        if (pArgs->test_sock == SOCKET_ERROR)
        {
            Log(ERROR_MSG, TEXT("ConnectSocketByAddr() failed, WSAError=%ld"), pArgs->wsa_error);
            test_resp.error = Test_Error;
            goto Done;
        }        
    }
    else
    {
        ////////////////////////////////////////////////////
        //
        // Receive Test - client connects to server
        //

        // on TCP wait for connection on one of the sockets in the array
        if (pArgs->req.protocol == SOCK_STREAM)
        {
            pArgs->test_sock = AcceptConnection(
                &pArgs->test_sock,
                1,
                NULL,
                TEST_CONNECTION_TIMEOUT_MS,
                &dwRet);
            if (pArgs->test_sock == SOCKET_ERROR)
            {
                Log(ERROR_MSG, TEXT("AcceptConnection() failed, WSAError=%ld"),
                    dwRet);
                pArgs->wsa_error = dwRet;
                test_resp.error = Test_Error;
                goto Done;
            }
        }
    }

    // Apply all required settings to test socket
    if (FALSE == ApplySocketSettings(
            pArgs->test_sock,
            pArgs->req.protocol,
            FALSE,                      // No need for keepalive
            pArgs->req.sock_nagle_off,  // nagle off
            pArgs->req.sock_send_buf,   // send buffer for socket
            pArgs->req.sock_recv_buf,   // recv buffer for socket
            &dwRet))
    {
        Log(ERROR_MSG, TEXT("ApplySocketSettings() failed, WSAError=%ld"), dwRet);
        pArgs->wsa_error = dwRet;
        test_resp.error = Test_Error;
        goto Done;
    }

    // Wait for setup confirmation from client
    if (SOCKET_ERROR == ReceiveGoMsg(pArgs->ctl_sock, &pArgs->wsa_error))
    {
        if (pArgs->wsa_error == 0)
        {
            Log(ERROR_MSG, TEXT("ReceiveGoMsg() timed-out"));
            test_resp.error = Test_Comm_Timeout;
        }
        else
        {
            Log(ERROR_MSG, TEXT("ReceiveGoMsg() failed, WSAError=%ld"), pArgs->wsa_error);
            test_resp.error = Test_Error;
        }
        goto Done;
    }  

    // Send setup confirmation to client
    if (SOCKET_ERROR == SendGoMsg(pArgs->ctl_sock, &pArgs->wsa_error))
    {
        Log(ERROR_MSG, TEXT("SendGoMsg() failed, WSAError=%ld"), pArgs->wsa_error);
        test_resp.error = Test_Error;
        goto Done;
    }

    // Insert test connection into the Connection Tracker
    CONNTRACK_HANDLE hConnTrack;
    hConnTrack = ConnTrack_Insert(pArgs->test_sock);

    // Run the test
    dwRet = Test(
        pArgs->ptest_form,
        pArgs->test_sock,
        pArgs->req.send_req_rate,
        pArgs->req.recv_req_rate,
        hConnTrack,
        &pArgs->result
    );

    test_resp.result = &pArgs->result;
    if ( ConnTrack_Remove(hConnTrack) )
        // Socket was closed by ConnTrack, test timed-out
        test_resp.error = Test_Run_Timeout;
    else if (dwRet != SOCKET_ERROR)
        test_resp.error = Test_Success;
    else
        test_resp.error = Test_Error;

Done:
    //
    // Wait for GO message from client before sending back the results.
    //
    // This will ensure the client is finished with the test and is ready to accept
    // results.
    //
    // Note that ReceiveGoMsg will wait for RECV_TIMEOUT_SEC before timing
    // out
    //
    if (SOCKET_ERROR == ReceiveGoMsg(pArgs->ctl_sock, &pArgs->wsa_error))
    {
        if (pArgs->wsa_error == 0)
        {
            Log(ERROR_MSG, TEXT("ReceiveGoMsg() timed-out, can not send Test response. "));
            test_resp.error = Test_Comm_Timeout;
        }
        else
        {
            Log(ERROR_MSG, TEXT("ReceiveGoMsg() failed, can not send Test response. WSAError=%ld"), pArgs->wsa_error);
            test_resp.error = Test_Error;
        }
        goto Done;
    }  
    // Send back result
    if (SendTestResponse(pArgs->ctl_sock, &test_resp, &dwRet) == SOCKET_ERROR)
        // Just an indication, we don't need to do anything else with this.
        Log(ERROR_MSG, TEXT("SendTestResponse() failed, WSAError=%ld"), dwRet);

    // Cleanup
    ConnTrack_Deinit();
    DisconnectSocket(pArgs->test_sock);
    DisconnectSocket(pArgs->ctl_sock);
    FreeTestResult(&pArgs->result);
    TestDeinit(&pArgs->ptest_form);
    delete pArgs;

    // Server is now available to serve the next pending request
    SetEvent(g_hServerAvailableEvent);

    Log(DEBUG_MSG, TEXT("-WorkerThread()"));
    return 0;
}

////////////////////////////////////////////////////////
//
// Does the preliminary handling of the test request
//   received by the main application loop.
//
INT ProcessRequest(SOCKET ctl_sock)
{
    Log(DEBUG_MSG, TEXT("+ProcessRequest()"));

    ServiceRequest req;
    ServiceResponse resp;
    WorkerThreadArgs* pWorkerArgs;
    DWORD dwErr;
    HANDLE hWorkerThread = NULL;

#pragma prefast(suppress: 14, "pWorkerArgs is deallocated by WorkerThread") 
    pWorkerArgs = new WorkerThreadArgs;
    if (pWorkerArgs == NULL)
    {
        Log(ERROR_MSG, TEXT("Memory error in ProcessRequest(): could not allocate memory for worker args."));
        goto CriticalError;
    }
    dwErr = 0;
    if (ReceiveServiceRequest(ctl_sock, &req, &dwErr) == SOCKET_ERROR)
    {
        if (dwErr == 0)
        {
            Log(ERROR_MSG, TEXT("ReceiveServiceRequest() timed-out"));
            resp.error = Test_Comm_Timeout;
        }
        else
        {
            Log(ERROR_MSG, TEXT("ReceiveServiceRequest() failed, WSAError=%ld"), dwErr);
            resp.error = Test_Error;
        }
        resp.port = 0;
        pWorkerArgs->wsa_error = dwErr;
        goto CriticalError;
    }

    if (req.version != COMMPROT_VERSION)
    {
        resp.error = Test_Version_Error;
    }

    // Process service request
    if (req.type == Send)
    {
        ////////////////////////////////////////////////////
        //
        // Send Test - client listens; server connects to client
        //
        SOCKET* listening_sock_array;
        INT listening_sock_count;
        HANDLE* EventArray = NULL;
        INT nRet;

        resp.port = 0;
        resp.error = Test_Success;

        if (req.protocol == SOCK_DGRAM)
        {
            resp.port = 0;
            nRet = ListenSocket(
                (WORD)req.ip_version,
                req.protocol,
                &resp.port,
                &listening_sock_array,
                &listening_sock_count,
                &dwErr);

            if (nRet == SOCKET_ERROR)
            {
                Log(ERROR_MSG, TEXT("ListenSocket() failed, WSAError=%ld"), dwErr);
                resp.error = Test_Error;
                goto Done;
            }
            else if (nRet == SOCKET_ERROR || listening_sock_count != 1)
            {
                Log(ERROR_MSG, TEXT("ListenSocket() returned array of %d sockets, expecting only 1"));
                resp.error = Test_Error;
                for (int i=0; i<listening_sock_count; i+=1) DisconnectSocket(listening_sock_array[i]);
                delete [] listening_sock_array;
                goto Done;
            }

            pWorkerArgs->test_sock = listening_sock_array[0];
            delete [] listening_sock_array;
        }
        else
            pWorkerArgs->test_sock = INVALID_SOCKET;
    }
    else
    {
        ////////////////////////////////////////////////////
        //
        // Receive Test - client connects to server
        //
        SOCKET* listening_sock_array;
        INT listening_sock_count;
        HANDLE* EventArray = NULL;
        INT nRet;
        
        resp.port = 0;
        resp.error = Test_Success;

        nRet = ListenSocket(
            (WORD)req.ip_version,
            req.protocol,
            &resp.port,
            &listening_sock_array,
            &listening_sock_count,
            &dwErr);

        if (nRet == SOCKET_ERROR)
        {
            Log(ERROR_MSG, TEXT("ListenSocket() failed, WSAError=%ld"), dwErr);
            resp.error = Test_Error;
            goto Done;
        }
        else if (nRet == SOCKET_ERROR || listening_sock_count != 1)
        {
            Log(ERROR_MSG, TEXT("ListenSocket() returned array of %d sockets, expecting only 1"), listening_sock_count);
            resp.error = Test_Error;
            for (int i=0; i<listening_sock_count; i+=1) DisconnectSocket(listening_sock_array[i]);
            delete [] listening_sock_array;
            goto Done;
        }

        if (req.protocol == SOCK_DGRAM)
        {
            pWorkerArgs->test_sock = ConnectSocketByAddrFromSocket(
                listening_sock_array[0],
                ctl_sock,
                req.port,
                (WORD)req.ip_version,
                req.protocol,
                FALSE,
                NULL, NULL, NULL, NULL,
                &dwErr);
            if (pWorkerArgs->test_sock == SOCKET_ERROR)
            {
                Log(ERROR_MSG, TEXT("ConnectSocketByAddrFromSocket() failed, WSAError=%ld"),
                    dwErr);
                resp.error = Test_Error;
                goto Done;
            }
            resp.port = GetSocketLocalPort(pWorkerArgs->test_sock);
            if (resp.port == 0)
            {
                Log(ERROR_MSG, TEXT("GetSocketLocalPort() returned port %d. Unable to continue."),
                    resp.port);
                resp.error = Test_Error;
                goto Done;
            }
            Log(DEBUG_MSG, TEXT("Local port = %d"), resp.port);
        }
        else
            pWorkerArgs->test_sock = listening_sock_array[0];
        
        delete [] listening_sock_array;
    }

    pWorkerArgs->ctl_sock = ctl_sock;
    memcpy(&pWorkerArgs->req, &req, sizeof(ServiceRequest));
    pWorkerArgs->wsa_error = 0;

Done:
    // Before issuing a service response, perform test setup
    // Note: the worker thread is responsible for deallocating this!
    AllocTestResult(pWorkerArgs->req.num_iterations, &pWorkerArgs->result);
    ClearTestResult(&pWorkerArgs->result);

    TestInit(
        pWorkerArgs->req.type,
        pWorkerArgs->req.protocol,
        pWorkerArgs->req.num_iterations,
        pWorkerArgs->req.send_total_size,
        pWorkerArgs->req.send_packet_size,
        pWorkerArgs->req.recv_total_size,
        pWorkerArgs->req.recv_packet_size,
        FALSE,
        &pWorkerArgs->ptest_form
    );

    // issue a service response   
    if (SendServiceResponse(ctl_sock, &resp, &dwErr) == SOCKET_ERROR)
    {
        Log(ERROR_MSG, TEXT("SendServiceResponse() failed, WSAError=%ld"), dwErr);
        pWorkerArgs->wsa_error = dwErr;
    }

    if (pWorkerArgs->wsa_error == 0)
    {
        hWorkerThread = CreateThread(
            NULL,                               // security attributes
            WORKER_THREAD_STACK_SIZE,           // stack size
            WorkerThread,                       // start function
            (void *)pWorkerArgs,                // parameter to pass to the thread
            0,                                  // creation flags
            NULL);                              // threadId of the newly created thread
    }

CriticalError:
    if (hWorkerThread == NULL)
    {
        // if worker thread failed to be started, some serious error occured
        Log(ERROR_MSG, TEXT("Worker thread failed to be started."));

        // Cleanup
        DisconnectSocket(ctl_sock);
        if (pWorkerArgs != NULL)
        {
            DisconnectSocket(pWorkerArgs->test_sock);
            FreeTestResult(&pWorkerArgs->result);
            TestDeinit(&pWorkerArgs->ptest_form);
            delete pWorkerArgs;
        }

        // Server is now available to serve the next pending request
        SetEvent(g_hServerAvailableEvent);
    }
    else
    {
        CloseHandle(hWorkerThread);
    }

    Log(DEBUG_MSG, TEXT("-ProcessRequest()"));

    return 0;
}

////////////////////////////////////////////////////////
//
// ServerMainThread contains the main application loop.
//
unsigned long __stdcall ServerMainThread(void* pParams)
{
    WSADATA WsaData;
    const WORD wSockVer = MAKEWORD(2,2);
    INT i;
    DWORD dwErr;
    WORD wServicePort = SERVER_SERVICE_REQUEST_PORT;
    SOCKET sockAccept = (SOCKET)SOCKET_ERROR;
    unsigned long nTemp;
    INT nListeningSockCount = 0;
    HANDLE* EventArray = NULL;
    SOCKET* ListeningSockArray = NULL;
    DWORD dwRequestCount = 0;

    ////////////////////////////////////////////////////////
    //
    // Initialize server
    //

    LogInitialize(TEXT("PERF_WINSOCKD2"),
#ifdef ENABLE_DEBUGGING_OUTPUT
        DEBUG_MSG
#else
        REQUIRED_MSG
#endif
    );

    Log(REQUIRED_MSG, TEXT("Initializing server"));
    Log(REQUIRED_MSG, TEXT("Using comm prot version %d"), COMMPROT_VERSION);
    
    g_hServerAvailableEvent = CreateEvent(
        NULL,    // no security attributes
        TRUE,    // manual reset event
        TRUE,    // signalled 
        NULL);   // no name
    if (g_hServerAvailableEvent == NULL)
        goto Cleanup;

    // Startup winsock
    if( WSAStartup( wSockVer, &WsaData ) != 0 )
    {
        Log(ERROR_MSG, TEXT("WSAStartup() failed"));
        goto Cleanup;
    }

    // Initialize sockets
#ifdef SUPPORT_IPV6
    if (ListenSocket(PF_UNSPEC, SOCK_STREAM, &wServicePort, &ListeningSockArray, &nListeningSockCount, &dwErr)
#else
    if (ListenSocket(0, SOCK_STREAM, &wServicePort, &ListeningSockArray, &nListeningSockCount, &dwErr)
#endif
        == SOCKET_ERROR)
    {
        Log(ERROR_MSG, TEXT("ListenSocket() returned error."));
    }

    if (nListeningSockCount < 1)
    {
        Log(ERROR_MSG, TEXT("No sockets to listen on! Aborting."));
        goto Cleanup;
    }

    // Event array will hold all events we need to wait on including the
    // the shutdown event.
    EventArray = new HANDLE[nListeningSockCount + 1];
    if (EventArray == NULL) {
        Log(ERROR_MSG, TEXT("Failed to allocate memory for critical functions. Aborting."));
        goto Cleanup;
    }
    for (i = 0; i < nListeningSockCount; i += 1)
    {
#ifdef SUPPORT_IPV6
        EventArray[i] = WSACreateEvent();
#else
        EventArray[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
#endif
    }
    EventArray[nListeningSockCount] = g_hServerStopEvent;

    ////////////////////////////////////////////////////////
    //
    // Service is now running, perform work until shutdown
    //
    for (;;)
    {
        DWORD dwWait;

        Log(REQUIRED_MSG, TEXT("Waiting for client"));

        // Request notification of incoming connection
        // - note that WSAEventSelect operation makes sockListen non-blocking
        for (i = 0; i < nListeningSockCount; i += 1) {
            if (SOCKET_ERROR == WSAEventSelect(
                ListeningSockArray[i],    // server socket
                EventArray[i],            // event to be generated
                FD_ACCEPT))                 // which event to wait on
            {
                Log(ERROR_MSG, TEXT("WSAEventSelect() failed on socket number %d, WSAError=%ld"),
                    i, WSAGetLastError());
                goto Cleanup;
            }
        }

        // Check if we were signalled
        dwWait = WaitForMultipleObjects(
            nListeningSockCount + 1,    // Number of events (to wait on)
            EventArray,                 // The event array
            FALSE,                      // Just wait for any of the events to go off
            INFINITE);                  // Length of time

        if (dwWait >= WAIT_OBJECT_0 && dwWait < WAIT_OBJECT_0 + (DWORD)nListeningSockCount)
        {
            //
            // We have a connection ready to be accepted
            //

            // The event number that went off (which is the same number as the socket number)
            INT nEventNum = dwWait - WAIT_OBJECT_0;

            // Reset listening socket back to blocking mode
            if (SOCKET_ERROR == WSAEventSelect(
                ListeningSockArray[nEventNum],
                EventArray[nEventNum], 0))
            {
                Log(ERROR_MSG, TEXT("WSAEventSelect() failed, WSAError=%ld"), WSAGetLastError());
                goto Cleanup;
            }
            nTemp = 0;
            if (SOCKET_ERROR == ioctlsocket(
                ListeningSockArray[nEventNum],
                FIONBIO, &nTemp))
            {
                Log(ERROR_MSG, TEXT("ioctlsocket() failed, WSA Error: %ld"), WSAGetLastError());
                goto Cleanup;
            }

            // Reset the event - this would of been done automatically by a call to accept,
            // however, when we switch socket to blocking, accept doesn't do the reset.
            WSAResetEvent(EventArray[nEventNum]);

            // Finally, accept the incoming connection.
            // Note: It will be up to the thread to clean up memory associated with this when
            //       it is done.
            Log(DEBUG_MSG, TEXT("We have a connection ready to be accepted on socket %d (#%d)..."),
                ListeningSockArray[nEventNum], nEventNum);

            // Incoming address
#ifdef SUPPORT_IPV6
            SOCKADDR_STORAGE addr;  
#else
            SOCKADDR addr;
#endif
            INT addrlen = sizeof(addr);

            // Before initiating processing of this request - make sure that there are not other
            // clients running at this time
            dwWait = WaitForSingleObject(g_hServerAvailableEvent, 0);
            if (dwWait != WAIT_OBJECT_0)
            {
                Log(REQUIRED_MSG,
                    TEXT("Waiting for completion of previous request before ")
                    TEXT("accepting new request"));
                WaitForSingleObject(g_hServerAvailableEvent, INFINITE);
            }
            
            sockAccept = accept(ListeningSockArray[nEventNum], (LPSOCKADDR)&addr, &addrlen);
            if (SOCKET_ERROR == sockAccept)
            {
                Log(WARNING_MSG, TEXT("accept() failed, WSA Error: %ld"), WSAGetLastError());
                continue;
            }

            // This is a control channel - turn nagle off
            DWORD WSAErr;
            if (FALSE == ApplySocketSettings(
                    sockAccept,
                    SOCK_STREAM,
                    FALSE,  // No need for keepalive
                    TRUE,   // turn nagle off
                    -1, -1, // leave send/recv buffer at default
                    &WSAErr))
            {
                Log(ERROR_MSG, TEXT("ApplySocketSettings() failed, WSA Error: %ld"), WSAErr);
                closesocket(sockAccept);
                continue;
            }

            // Server is now not available
            ResetEvent(g_hServerAvailableEvent);

            // Create a thread to handle the client
            Log(REQUIRED_MSG, TEXT("Processing test request #%ld"), dwRequestCount++);
            ProcessRequest(sockAccept);
        }
        else if (dwWait == WAIT_OBJECT_0 + (DWORD)nListeningSockCount)
        {
            // Request for termination
            Log(DEBUG_MSG, TEXT("Server stop signalled. Exiting."));
            goto Cleanup;
        }
        else
        {
            // Unhandled - panic!
            Log(ERROR_MSG, TEXT("WaitForMultipleObjects() unhandled return code %d"), dwWait);
            goto Cleanup;
        }
    }   

Cleanup:

    for (i = 0; i < nListeningSockCount; i += 1) {
        Log(DEBUG_MSG, TEXT("Closing socket number %d of %d"), i + 1, nListeningSockCount);
        if (ListeningSockArray[i] != SOCKET_ERROR && ListeningSockArray[i] != INVALID_SOCKET)
        {
#ifdef SUPPORT_IPV6
            shutdown(ListeningSockArray[i], SD_BOTH);
#else
            shutdown(ListeningSockArray[i], 0x2);
#endif
            closesocket(ListeningSockArray[i]);
        }
    }
    if (EventArray != NULL) {
        for (i = 0; i < nListeningSockCount; i += 1)
        {
            if (EventArray[i] != NULL)
                CloseHandle(EventArray[i]);
        }
        delete [] EventArray;
    }
    
    // This will also close the last handle in the array (above)
    if (g_hServerStopEvent != NULL) CloseHandle(g_hServerStopEvent);
    
    if (g_hServerAvailableEvent != NULL) CloseHandle(g_hServerAvailableEvent);

    WSACleanup();

    // Signal service completion
    SetEvent(g_hServerDoneEvent);

    return 0;
}

////////////////////////////////////////////////////////
//
// Services-specific functions
//

////////////////////////////////////////////////////////
//
VOID ServiceStart(DWORD dwArgc, LPTSTR *lpszArgv)
{
    g_hServerStopEvent = CreateEvent(
        NULL,    // no security attributes
        TRUE,    // manual reset event
        FALSE,   // not-signalled 
        NULL);   // no name
    if (g_hServerStopEvent == NULL)
        goto Cleanup;

    g_hServerDoneEvent = CreateEvent(
        NULL,    // no security attributes
        TRUE,    // manual reset event
        FALSE,   // not-signalled 
        NULL);   // no name
    if (g_hServerDoneEvent == NULL)
        goto Cleanup;

#ifndef UNDER_CE
    // report the status to the service control manager.
    //
    if (!ReportStatusToSCMgr(
        SERVICE_RUNNING, // service state
        NO_ERROR,              // exit code
        0))                 // wait hint
        goto Cleanup;
#endif

    ServerMainThread(NULL);

Cleanup:
    ServiceStop();
    
}

////////////////////////////////////////////////////////
//
VOID ServiceStop()
{
    if (g_hServerStopEvent != NULL)
        SetEvent(g_hServerStopEvent);

#ifndef UNDER_CE
    // report the status to the service control manager.
    //
    if (!ReportStatusToSCMgr(
        SERVICE_STOP_PENDING, // service state
        NO_ERROR,             // exit code
        60000))               // wait hint
        goto Cleanup;
#endif

    // Wait until service thread exists
    WaitForSingleObject(g_hServerDoneEvent, INFINITE);

#ifndef UNDER_CE
    // report the status to the service control manager.
    //
    if (!ReportStatusToSCMgr(
        SERVICE_STOPPED, // service state
        NO_ERROR,        // exit code
        0))              // wait hint
        goto Cleanup;
#endif

#ifndef UNDER_CE
Cleanup:
#endif
    // Clean-up resources
    if (g_hServerDoneEvent != NULL) CloseHandle(g_hServerDoneEvent);
    if (g_hServerStopEvent != NULL) CloseHandle(g_hServerStopEvent);
}