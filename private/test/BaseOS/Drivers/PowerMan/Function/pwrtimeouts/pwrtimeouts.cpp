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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//  Copyright (c) Microsoft Corporation
//
//  Module:
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include "thelper.h"

// Max seconds allowed for user/system activity to cause a state change
#define TRANSITION_CHANGE_MAX   60

// Max seconds allowed above the timeout value for a system power change to occur.
// This value should be the basic activity timer timeout (10 seconds)
#define TRANSITION_TOLERANCE   10

#define BUFFER_SIZE     2048
#define POLL_TIME       5000    // milliseconds
#define SUSPEND_TIME      15    // seconds

#define PORTNUM               5000    // Port number
#define MAX_PENDING_CONNECTS  4       // Maximum length of the queue
                                      // of pending connections
#define HOSTNAME        "localhost"   // Server name string

// Idle Timeouts used for testing the DEFAULT version of the Power Manager
IDLE_TIMEOUTS g_defaultIdleTimeouts[] = {
    { 20,   0,   0},
    { 20,  20,   0},
    { 20,  20,  30},
    { 20,   0,  30}
};

// Idle Timeouts used for testing the PDA version of the Power Manager
IDLE_TIMEOUTS g_pdaIdleTimeouts[] = {
    {  0,  20,   0},
    { 40,  20,   0},
    { 40,  20,  60},
    {  0,  20,  30}
};

DWORD g_cDefaultIdleTimeouts = sizeof(g_defaultIdleTimeouts)/sizeof(g_defaultIdleTimeouts[0]);
DWORD g_cPdaIdleTimeouts = sizeof(g_pdaIdleTimeouts)/sizeof(g_pdaIdleTimeouts[0]);


////////////////////////////////////////////////////////////////////////////////
// PassiveStateWatch
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_PMTimeouts(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    IDLE_TIMEOUTS itPrev = {0};
    PIDLE_TIMEOUTS pit = NULL;
    HANDLE hEvUserActive = NULL;

    if(!GetIdleTimeouts(&itPrev,g_bPdaVersion))
    {
        FAIL("GetIdleTimeouts()");
        goto Error;
    }
    DETAIL("Current Idle Timeout settings:");
    PrintIdleTimeouts(&itPrev,g_bPdaVersion);

    // Check for valid input
    if(lpFTE->dwUserData > g_cDefaultIdleTimeouts ||
       lpFTE->dwUserData > g_cPdaIdleTimeouts)
    {
        SKIP("Invalid idle timeout array entry");
        goto Error;
    }
    
    // Set idle timeouts to use for this test
    if(g_bPdaVersion)
    {
        pit = &g_pdaIdleTimeouts[lpFTE->dwUserData-1];
    }
    else
    {
        pit = &g_defaultIdleTimeouts[lpFTE->dwUserData-1];
    }

    // Check if suspending the device is allowed
    if(!g_fAllowSuspend && pit->Suspend != 0)
    {
        SKIP("Skipping this test because it requires the system to suspend");
        DETAIL("Specify this option (-c\"AllowSuspend\") to allow the tests to suspend the system.");
        goto Error;
    }

    if(!SetIdleTimeouts(pit,g_bPdaVersion))
    {
        FAIL("SetIdleTimeouts()");
        goto Error;
    }

    //Manually Signal Reload Timeouts Event
    SimulateReloadTimeoutsEvent();

    if(!GetIdleTimeouts(pit,g_bPdaVersion))
    {
        FAIL("GetIdleTimeouts()");
        goto Error;
    }
    DETAIL("New Idle Timeout settings for this test:");
    PrintIdleTimeouts(pit,g_bPdaVersion);


    // open activity timer manual-reset events
    LOG(TEXT("Opening activity timer event \"%s\""), USER_ACTIVITY_ACTIVE_EVT);
    PREFAST_SUPPRESS(25084, "suppress EVENT_ALL_ACCESS warning");
    hEvUserActive = OpenEvent(EVENT_ALL_ACCESS, FALSE, USER_ACTIVITY_ACTIVE_EVT);
    if(NULL == hEvUserActive)
    {
        ERRFAIL("OpenEvent()");
        goto Error;
    }

    DETAIL("Starting the test in power state \"On\"");
    if(!SimulateUserEvent())
    {
        FAIL("SimulateUserEvent()");
        goto Error;
    }

    if(!PollForSystemPowerChange(POWER_STATE_NAME_ON, 0, TRANSITION_CHANGE_MAX))
    {
        FAIL("User activity did not put the system in state \"On\"");
        goto Error;
    }

    if(g_bPdaVersion) // test the PDA version of the Power Manager
    {
        if(pit->SystemBackLight)
        {
            DWORD dwWaitTime = pit->SystemBackLight;
            if(!WaitForSystemPowerChange(POWER_STATE_NAME_BACKLIGHTOFF, dwWaitTime, dwWaitTime+TRANSITION_TOLERANCE, hEvUserActive))
            {
                FAIL("WaitForSystemPowerChange to \"BacklightOff\"");
                goto Error;
            }

            if(pit->UserIdle)
            {
                dwWaitTime = (pit->UserIdle - pit->SystemBackLight);
                if(!WaitForSystemPowerChange(POWER_STATE_NAME_USERIDLE, dwWaitTime, dwWaitTime+TRANSITION_TOLERANCE, hEvUserActive))
                {
                    FAIL("WaitForSystemPowerChange to \"UserIdle\"");
                    goto Error;
                }

                if(g_fAllowSuspend && pit->Suspend)
                {
                    dwWaitTime = (pit->Suspend - pit->UserIdle);
                    if(!WaitForSystemPowerChange(POWER_STATE_NAME_UNATTENDED, dwWaitTime, dwWaitTime+TRANSITION_TOLERANCE, hEvUserActive))
                    {
                        FAIL("WaitForSystemPowerChange to \"Unattended\"");
                        goto Error;
                    }

                    dwWaitTime = 0;
                    if(!PollForSystemPowerChange(POWER_STATE_NAME_SUSPEND, dwWaitTime, dwWaitTime+TRANSITION_TOLERANCE))
                    {
                        FAIL("PollForSystemPowerChange to \"Suspend\"");
                        goto Error;
                    }

                    DETAIL("User will need to manually resume the device!!");   
                    
                    // Allow time for the device to fully suspend and power down
                    Sleep(5000);

                    // it is not necessarily defined what state the system will end up
                    // in when it resumes from a suspend, so we don't explicitly test 
                    // for a specific transition after the suspend
                }
                else
                {
                    DETAIL("Device is set to never transition to state \"Unattended\"");
                }
            }
            else
            {
                DETAIL("Device is set to never transition to state \"UserIdle\"");
            }
        }
        else
        {
            DETAIL("Device is set to never transition to state \"BacklightOff\"");
        }        
    }
    else // test the DEFAULT version of the Power Manager
    {
        if(pit->UserIdle)
        {
            if(!WaitForSystemPowerChange(POWER_STATE_NAME_USERIDLE, pit->UserIdle, pit->UserIdle+TRANSITION_TOLERANCE, hEvUserActive))
            {
                FAIL("WaitForSystemPowerChange to \"UserIdle\"");
                goto Error;
            }

            // Essentially no time elapses here. If it does (> 0.5 s), we could appear to receive the state
            // transition earlier than expected

            if(pit->SystemBackLight)
            {
                if(!WaitForSystemPowerChange(POWER_STATE_NAME_SYSTEMIDLE, pit->SystemBackLight, pit->SystemBackLight+TRANSITION_TOLERANCE, hEvUserActive))
                {
                    FAIL("WaitForSystemPowerChange to \"SystemIdle\"");
                    goto Error;
                }

                if(g_fAllowSuspend && pit->Suspend)
                {
                    if(!WaitForSystemPowerChange(POWER_STATE_NAME_SUSPEND, pit->Suspend, pit->Suspend+TRANSITION_TOLERANCE, hEvUserActive))
                    {
                        FAIL("WaitForSystemPowerChange to \"Suspend\"");
                        goto Error;
                    }
                    
                    DETAIL("User will need to manually resume the device.");   
                    
                    // Allow time for the device to fully suspend and power down
                    Sleep(5000);

                    // it is not necessarily defined what state the system will end up
                    // in when it resumes from a suspend, so we don't explicitly test 
                    // for a specific transition after the suspend
                }
                else
                {
                    DETAIL("Device is set to never transition to state \"Suspend\"");
                }
            }
            else
            {
                DETAIL("Device is set to never transition to state \"SystemIdle\"");
            }
        }
        else
        {
            DETAIL("Device is set to never transition to state \"UserIdle\"");
        }        
    }

    SimulateUserEvent();
    
    if(!PollForSystemPowerChange(POWER_STATE_NAME_ON, 0, TRANSITION_CHANGE_MAX))
    {
        FAIL("PollForSystemPowerChange to \"On\"");
        goto Error;
    }

Error:
    if(hEvUserActive)
    {
        CloseHandle(hEvUserActive);
    }
    if(!SetIdleTimeouts(&itPrev,g_bPdaVersion))
    {
        FAIL("SetIdleTimeouts()");
    }
    else
    {
        DETAIL("Restored previous Idle Timeout settings:");
        PrintIdleTimeouts(&itPrev,g_bPdaVersion);
    }
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Tst_DiskWriteReadWatch
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_DiskWriteReadWatch(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwBytes = 0;
    DWORD dwAttributes = 0;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    BYTE buffer[BUFFER_SIZE];
    TCHAR szFile[MAX_PATH];
    TCHAR szPath[MAX_PATH];

    LOG(L"Test will try to find the storage path for the storage profile \"%s\"", g_szProfile);
    if(Hlp_GetFATTestRoot(g_szProfile, szPath, MAX_PATH))
    {
        LOG(L"Storage Path for the specified storage profile is \"%s\"", szPath);
    }
    else
    {
        SKIP("The specified storage profile does not exist. Test will skip.");
        goto Error;
    }
    
    dwAttributes = GetFileAttributes(szPath);
    if(0xFFFFFFFF == dwAttributes || !(FILE_ATTRIBUTE_DIRECTORY & dwAttributes))
    {
        // user specified path doesn't exist. Then try to find other valid storage path on device
        FAIL("FAIL: Unable to get File Attributes for the storage path");
        goto Error;
    }

    do
    {
        // waiting for the system inactive event to become set
        DETAIL("Waiting for system activity to cease...");
    } while(!WaitForSystemInactivity(POLL_TIME));

    // perform some disk activity...
    if(!wcscmp(szPath, TEXT("\\")))
    {
        VERIFY(SUCCEEDED(StringCchPrintf(szFile, MAX_PATH, TEXT("%sfile.tst"), szPath)));
    }
    else
    {
        VERIFY(SUCCEEDED(StringCchPrintf(szFile, MAX_PATH, TEXT("%s\\file.tst"), szPath)));
    }
    
    LOG(TEXT("TEST: Creating a file on disk \"%s\" to simulate system activity..."),
        szFile);
    hFile = CreateFile(szFile, GENERIC_READ | GENERIC_WRITE, 0,  NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hFile)
    {
        ERRFAIL("CreateFile()");
        goto Error;
    }

    // check both the ACTIVE and INACTIVE events for the System Activity timer
    if(!WaitForSystemActivity(0))
    {
        FAIL("Creating a file on disk did not set the System Activity timer's Active event!");
        goto Error;
    }
    if(WaitForSystemInactivity(0))
    {
        FAIL("Creating a file on disk did not reset System Activity timer's Inactive event!");
        goto Error;
    }

    DETAIL("PASS: Creating a file correctly signalled the System Activity events.");

    do
    {
        // waiting for the system inactive event to become set
        DETAIL("Waiting for system activity to cease...");
    } while(!WaitForSystemInactivity(POLL_TIME));

    DETAIL("TEST: Writing to file to simulate system activity...");

    if(!WriteFile(hFile, buffer, BUFFER_SIZE, &dwBytes, NULL))
    {
        ERRFAIL("WriteFile()");
        goto Error;
    }

    // check both the ACTIVE and INACTIVE events for the System Activity timer
    if(!WaitForSystemActivity(0))
    {
        FAIL("Writing to a file on disk did not set the System Activity timer's Active event!");
        goto Error;
    }
    if(WaitForSystemInactivity(0))
    {
        FAIL("Writing to a file on disk did not reset System Activity timer's Inactive event!");
        goto Error;
    }

    DETAIL("PASS: Writing to file correctly signalled the System Activity events.");

    do
    {
        DETAIL("Waiting for system activity to cease...");
    } while(!WaitForSystemInactivity(POLL_TIME));

    DETAIL("TEST: Setting file pointer to beginning of file to simulate disk activity...");

    if(0xFFFFFFFF == SetFilePointer(hFile, 0, NULL, FILE_BEGIN))
    {
        ERRFAIL("SetFilePointer()");
        goto Error;
    }

    // check both the ACTIVE and INACTIVE events for the System Activity timer
    if(!WaitForSystemActivity(0))
    {
        FAIL("Setting file pointer did not set the System Activity timer's Active event!");
        goto Error;
    }
    if(WaitForSystemInactivity(0))
    {
        FAIL("Setting file pointer on disk did not reset System Activity timer's Inactive event!");
        goto Error;
    }

    DETAIL("PASS: Setting file pointer correctly signalled the System Activity events.");

    do
    {
        // waiting for the system inactive event to become set
        DETAIL("Waiting for system activity to cease...");
    } while(!WaitForSystemInactivity(POLL_TIME));

    DETAIL("TEST: Reading from file to simulate system activity...");

    if(!ReadFile(hFile, buffer, BUFFER_SIZE, &dwBytes, NULL))
    {
        ERRFAIL("ReadFile()");
        goto Error;
    }

    if(!CloseHandle(hFile))
    {
        ERRFAIL("CloseHandle()");
        goto Error;
    }
    hFile = INVALID_HANDLE_VALUE;

    // check both the ACTIVE and INACTIVE events for the System Activity timer
    if(!WaitForSystemActivity(0))
    {
        FAIL("Reading from a file on disk did not set the System Activity timer's Active event!");
        goto Error;
    }
    if(WaitForSystemInactivity(0))
    {
        FAIL("Reading from a file on disk did not reset System Activity timer's Inactive event!");
        goto Error;
    }

    DETAIL("PASS: Reading from file correctly signalled the System Activity events.");

    do
    {
        DETAIL("Waiting for system activity to cease...");
    } while(!WaitForSystemInactivity(POLL_TIME));

    DETAIL("TEST: Deleting file to simulate system activity...");

    if(!DeleteFile(szFile))
    {
        ERRFAIL("DeleteFile()");
        goto Error;
    }

    // check both the ACTIVE and INACTIVE events for the System Activity timer
    if(!WaitForSystemActivity(0))
    {
        FAIL("Deleting a file on disk did not set the System Activity timer's Active event!");
        goto Error;
    }
    if(WaitForSystemInactivity(0))
    {
        FAIL("Deleting a file on disk did not reset System Activity timer's Inactive event!");
        goto Error;
    }

    DETAIL("PASS: Deleting file correctly signalled the System Activity events.");

Error:
    if(INVALID_HANDLE_VALUE != hFile)
    {
        CloseHandle(hFile);
    }

    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// TCPClientThreadProc
//  Used by the NetworkWatch test to make a TCP connection.
//
// Parameters:
//  lpParam         Not used.
//
// Return value:
//  Always returns zero
////////////////////////////////////////////////////////////////////////////////
DWORD WINAPI TCPClientThreadProc(LPVOID lpParam)
{
    SOCKET ServerSock = INVALID_SOCKET;
    SOCKADDR_IN destination_sin;
    PHOSTENT phostent = NULL;

    Sleep(1000);

    DETAIL("Client opening network socket...");
    ServerSock = socket(AF_INET, SOCK_STREAM, 0);
    if(INVALID_SOCKET == ServerSock)
    {
        ERRFAIL("socket()");
        goto Error;
    }

    destination_sin.sin_family = AF_INET;

    phostent = gethostbyname(HOSTNAME);
    if(NULL == phostent)
    {
        ERRFAIL("gethostbyname()");
        goto Error;
    }

    memcpy((char FAR *)&(destination_sin.sin_addr),
        phostent->h_addr, sizeof(destination_sin.sin_addr));

    destination_sin.sin_port = htons(PORTNUM);

    LOG(L"Client connecting to \"%S\" on port %u ...", HOSTNAME, PORTNUM);
    if(SOCKET_ERROR == connect(ServerSock, (PSOCKADDR)&destination_sin,
        sizeof(destination_sin)))
    {
        FAIL("connect()");
        goto Error;
    }

    LOG(L"Client successfully connected to server \"%S\"!", HOSTNAME);

    Sleep(1000);

Error:
    if(INVALID_SOCKET != ServerSock)
    {
        closesocket(ServerSock);
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Tst_NetworkWatch
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_NetworkWatch(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    SOCKET WinSocket = INVALID_SOCKET,
           ClientSock = INVALID_SOCKET;

    SOCKADDR_IN local_sin,
                accept_sin;

    int accept_sin_len;

    WSADATA wsaData;

    HANDLE hThread = NULL;

    // start winsock
    DETAIL("Setting up WinSock socket...");
    if(0 != WSAStartup(MAKEWORD(1,1), &wsaData))
    {
        SKIP("Winsock unavailable");
        goto Error;
    }
    LOG(L"Winsock Started version %d.%d", HIBYTE(wsaData.wVersion),
        LOBYTE(wsaData.wVersion));

    do
    {
        DETAIL("Waiting for system activity to cease...");
    } while(!WaitForSystemInactivity(POLL_TIME));

    DETAIL("Establishing TCP/IP socket connection");

    // create a TCP/IP socket
    DETAIL("Server opening network socket...");
    WinSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(INVALID_SOCKET == WinSocket)
    {
        ERRFAIL("socket()");
        goto Error;
    }

    // fill out local address information
    local_sin.sin_family = AF_INET;
    local_sin.sin_port = htons (PORTNUM);
    local_sin.sin_addr.s_addr = htonl (INADDR_ANY);

    // associate the address with the server socket
    LOG(L"Server binding socket to port %u...", PORTNUM);
    if(SOCKET_ERROR == bind(WinSocket, (struct sockaddr *)&local_sin, sizeof(local_sin)))
    {
        FAIL("bind()");
        goto Error;
    }

    // establish a socket to listen to incoming connections
    LOG(L"Server listening on port %u...", PORTNUM);
    if(SOCKET_ERROR == listen(WinSocket, MAX_PENDING_CONNECTS))
    {
        FAIL("listen()");
        goto Error;
    }

    // create a client thread...
    DETAIL("Starting client thread...");
    hThread = CreateThread(NULL, 0, TCPClientThreadProc, NULL, 0, NULL);
    if(NULL == hThread)
    {
        ERRFAIL("CreateThread()");
        goto Error;
    }

    accept_sin_len = sizeof(accept_sin);

    // accept an incomming connection attempt on WinSocket
    ClientSock = accept(WinSocket, (struct sockaddr *)&accept_sin, (int *)&accept_sin_len);
    if(INVALID_SOCKET == ClientSock)
    {
        FAIL("accept()");
        goto Error;
    }

    DETAIL("Server connected to client!");

    // check both the ACTIVE and INACTIVE events for the System Activity timer
    if(!WaitForSystemActivity(10*1000))
    {
        FAIL("Open network socket did not set System Activity timer's Active event!");
        goto Error;
    }
    if(WaitForSystemInactivity(0))
    {
        FAIL("Open network socket did not reset System Activity timer's Inactive event!");
        goto Error;
    }

    DETAIL("PASS: Open network socket correctly signalled the System Activity events.");

Error:
    WSACleanup();

    if(INVALID_SOCKET != WinSocket)
    {
        closesocket(WinSocket);
    }

    if(INVALID_SOCKET != ClientSock)
    {
        closesocket(ClientSock);
    }

    if(NULL != hThread)
    {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }

    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Tst_UserInputWatch
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_UserInputWatch(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // verify User Activity events after the User Activity timer expires
    // and a user input event occurs
    
    // wait for user inactivity
    do
    {
        DETAIL("Waiting for user activity to cease...");
    } while(!WaitForUserInactivity(POLL_TIME));

    // simulate keyboard event to bring it out of UserIdle state
    DETAIL("Simulating a keyboard event...");
    keybd_event(VK_SPACE, 0, 0, 0);

    // check both the ACTIVE and INACTIVE events for the User Activity timer
    if(!WaitForUserActivity(10*1000))
    {
        FAIL("Open keybd_event did not set the User Activity timer's Active event!");
        goto Error;
    }
    if(WaitForUserInactivity(0))
    {
        FAIL("Open keybd_event did not reset the User Activity timer's Inactive event!");
        goto Error;
    }

    DETAIL("PASS: keybd_event correctly signalled the User Activity events.");

    // verify User Activity events after both User and System Activity timers expire
    // and a user input event occurs

    // wait for user inactivity
    do
    {
        DETAIL("Waiting for user activity to cease...");
    } while(!WaitForUserInactivity(POLL_TIME));

    // wait for system inactivity
    do
    {
        DETAIL("Waiting for system activity to cease...");
    } while(!WaitForSystemInactivity(POLL_TIME));

    // simulate keyboard event to bring it out of UserIdle state
    DETAIL("Simulating a keyboard event...");
    keybd_event(VK_SPACE, 0, 0, 0);

    // check both the ACTIVE and INACTIVE events for the User Activity timer
    if(!WaitForUserActivity(10*1000))
    {
        FAIL("Open keybd_event did not set the User Activity timer's Active event!");
        goto Error;
    }
    if(WaitForUserInactivity(0))
    {
        FAIL("Open keybd_event did not reset the User Activity timer's Inactive event!");
        goto Error;
    }

    DETAIL("PASS: keybd_event correctly signalled the User Activity events.");

Error:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Tst_SuspendResumeWatch
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_SuspendResumeWatch(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Check if suspending the device is allowed
    if(g_fAllowSuspend == FALSE)
    {
        SKIP("Skipping this test because it requires the system to suspend");
        DETAIL("Specify this option (-c\"AllowSuspend\") to allow the tests to suspend the system.");
        goto Error;
    }

    // suspend the system
    DETAIL("Suspending the device now...");
    DETAIL("User will need to manually resume the device.");
    if(ERROR_SUCCESS != SetSystemPowerState(NULL, POWER_STATE_SUSPEND, POWER_FORCE))
    {
        FAIL("SetSystemPowerState(NULL, POWER_STATE_SUSPEND, POWER_FORCE)");
        goto Error;
    }

    // wait for the system to transition to the On system power state
    if(!PollForSystemPowerChange(POWER_STATE_NAME_ON, 0, TRANSITION_CHANGE_MAX))
    {
        FAIL("PollForSystemPowerChange(\"On\")");
        goto Error;
    }    
    DETAIL("PASS: system transitioned to \"On\" power state");

    // check both the ACTIVE and INACTIVE events for the User Activity timer
    if(!WaitForUserActivity(10*1000))
    {
        FAIL("Manual resume did not set the User Activity timer's Active event!");
        goto Error;
    }
    if(WaitForUserInactivity(0))
    {
        FAIL("Manual resume did not reset the User Activity timer's Inactive event!");
        goto Error;
    }
    DETAIL("PASS: manual resume correctly signalled the User Activity events.");

Error:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Tst_RTCWakeSourceSystem
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RTCWakeSourceSystem(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    // Check if suspending the device is allowed
    if(g_fAllowSuspend == FALSE)
    {
        SKIP("Skipping this test because it requires the system to suspend");
        DETAIL("Specify this option (-c\"AllowSuspend\") to allow the tests to suspend the system.");
        goto Error;
    }

    // Setup RTC timer for resuming the device
    if(!SetupRTCWakeupResource(SUSPEND_TIME))
    {
        SKIP("Unable to setup the RTC timer for wake-up. Test will skip.");
        goto Error;
    }

    // Add the wake source to the USER activity timer
    if(!AddWakesourceToActivityTimer(SYSINTR_RTC_ALARM, USER_ACTIVITY))
    {
        FAIL("AddWakesourceToActivityTimer()");
        goto Error;
    }

    // Add the wake source to the SYSTEM activity timer
    if(!AddWakesourceToActivityTimer(SYSINTR_RTC_ALARM, SYSTEM_ACTIVITY))
    {
        FAIL("AddWakesourceToActivityTimer()");
        goto Error;
    }
    
    // Suspend the system
    DETAIL("Suspending the device now...");
    if(ERROR_SUCCESS != SetSystemPowerState(NULL, POWER_STATE_SUSPEND, POWER_FORCE))
    {
        FAIL("SetSystemPowerState(NULL, POWER_STATE_SUSPEND, POWER_FORCE)");
        goto Error;
    }

    // Since we set the wake-source of the timers, the expected power state
    // is "On". This is because the wake-source should set the User Activity
    // timer's Active event triggering the system to enter the "On" power
    // state

    // wait for the system to transition to the On system power state
    if(!PollForSystemPowerChange(POWER_STATE_NAME_ON, 0, POLL_TIME))
    {
        FAIL("PollForSystemPowerChange(\"On\")");
        goto Error;
    }    
    DETAIL("PASS: system transitioned to \"On\" power state after suspend");

    // Check both the ACTIVE and INACTIVE events for the User Activity timer
    if(!WaitForUserActivity(10*1000))
    {
        FAIL("Resume did not set the User Activity timer's Active event!");
        goto Error;
    }
    if(WaitForUserInactivity(0))
    {
        FAIL("Resume did not reset the User Activity timer's Inactive event!");
        goto Error;
    }
    DETAIL("PASS: resume correctly signalled the User Activity events.");

Error:
    ReleaseRTCWakeupResource();
    RemoveWakesourceFromActivityTimer(SYSINTR_RTC_ALARM, USER_ACTIVITY);
    RemoveWakesourceFromActivityTimer(SYSINTR_RTC_ALARM, SYSTEM_ACTIVITY);

    return GetTestResult();
}
