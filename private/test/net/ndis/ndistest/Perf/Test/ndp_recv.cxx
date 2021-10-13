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
#include <windows.h>
#include <tchar.h>
#include "utils.h"
#include "log.h"
#include "ndp_protocol.h"
#include "ndp_lib.h"
#include "globals.h"

DWORD g_acceptTimeout = 0xFFFFFFFF;
extern HANDLE g_hAdapter;
extern BOOL g_fUseNdisSendOnly;
HANDLE g_hFinishEvent = NULL;
extern TCHAR  *g_szAdapter;

//------------------------------------------------------------------------------

#ifndef UNDER_CE

// To modify the Ctrl+c behaviour for windows XP
BOOL CtrlHandler(DWORD fdwCtrlType)
{
    /*
    When a CTRL+C signal is received, the control handler returns TRUE, indicating
    that it has handled the signal. Doing this prevents other control handlers from being called.

    When a CTRL_CLOSE_EVENT signal is received, the control handler returns TRUE,
    causing the system to display a dialog box that gives the user the choice of terminating
    the process and closing the console or allowing the process to continue execution.
    If the user chooses not to terminate the process, the system closes the console when
    the process finally terminates.

    When a CTRL+BREAK, CTRL_LOGOFF_EVENT, or CTRL_SHUTDOWN_EVENT signal is received,
    the control handler returns FALSE. Doing this causes the signal to be passed to the
    next control handler function. If no other control handlers have been registered or
    none of the registered handlers returns TRUE, the default handler will be used,
    resulting in the process being terminated.
    */

    switch (fdwCtrlType)
    {
        // Handle the CTRL+C signal.

    case CTRL_C_EVENT:

        Beep(1000, 1000);
        LogMsg(_T("Closing the perf_ndis instance and exiting."));
        SetEvent(g_hFinishEvent);
        return TRUE;

        // CTRL+CLOSE: confirm that the user wants to exit.

    case CTRL_CLOSE_EVENT:

        return TRUE;

        // Pass other signals to the next handler.

    case CTRL_BREAK_EVENT:

    case CTRL_LOGOFF_EVENT:

    case CTRL_SHUTDOWN_EVENT:

    default:

        return FALSE;
    }
}

#endif

BOOL StartServer(const TCHAR * szServerAddress, ULONG ulPort)
{
    ULONG ulWSAErrorCode = 0;

    LogMsg(_T("Initializing the server TCP socket: IP %s Port %d"), szServerAddress, ulPort);

    if (StartServer(szServerAddress, ulPort, &ulWSAErrorCode) == FALSE)
    {
        LogMsg(_T("Failed to start server. Error code %d"), ulWSAErrorCode);
        return FALSE;
    }
    return TRUE;
}

BOOL AcceptPeer(UCHAR peerAddr[])
{
    BOOL ok = FALSE;
    teNDPPacketType packetType;
    UCHAR srcAddr[ETH_ADDR_SIZE];
    DWORD startTime = 0;
    ULONG ulWSAErrorCode = 0;

    if (Accept(&ulWSAErrorCode) == FALSE)
    {
        LogErr(_T("Failed to accept socket. Error code %d"), ulWSAErrorCode);
        return FALSE;
    }

    startTime = GetTickCount();

    // Start listen for 'PEER' packet

    if (ReceivePacket(&packetType, peerAddr, &ulWSAErrorCode) == FALSE ||
        packetType != NDP_PACKET_PEER)
    {
        LogMsg(_T("Failed to receive PEER packet. Error code %d"), ulWSAErrorCode);
        return FALSE;
    }

    //Get MAC and send to peer
    if (!GetMACAddress(g_szAdapter, srcAddr))
    {
        LogMsg(_T("Failed to retrieve local Mac address."));
        return FALSE;
    }

    if (!SendPacket(NDP_PACKET_PEER, srcAddr, &ulWSAErrorCode))
    {
        LogMsg(_T("Failed to send PEER packet. Error code %d"), ulWSAErrorCode);
        return FALSE;
    }

    return TRUE;
}

//------------------------------------------------------------------------------

BOOL ReceiveIterationWithPeer(
    HANDLE hAdapter, UCHAR peerAddr[], DWORD *pPacketType
    ) {
        BOOL ok = FALSE;
        teNDPPacketType packetType;
        NDP_STRESS_RESULT result = {0};
        tsNDPParams sNDPParams = {0};
        DWORD dwSyncSteps = 0;
        ULONG ulWSAErrorCode = 0;

        // Just for case
        *pPacketType = NDP_PACKET_NONE;

        LogVbs(_T("Daemon: Wait for 'PARAMS' packet"));

        // Wait for test parameters
        if (!ReceivePacket(&packetType, (UCHAR*)&sNDPParams, &ulWSAErrorCode) ||
            packetType != NDP_PACKET_PARAMS) 
        {
                LogMsg(_T("Daemon: Failed to receive 'PARAMS' packet. Error code %d"), ulWSAErrorCode);
                goto cleanUp;
        }
        LogVbs(_T("Daemon: 'PARAMS' get %d"), packetType);

        LogVbs(_T("Daemon: Received 'PARAMS' packet. PoolSize=%d, TestType=%d, PacketSize=%d, Packets =%d"),
            sNDPParams.dwPoolSize, sNDPParams.dwNDPTestType, sNDPParams.dwPacketSize, sNDPParams.dwNosOfPacketsToSend);

        // Send 'OK' packet
        if (!SendPacket(NDP_PACKET_OK, NULL, &ulWSAErrorCode))
        {
            LogErr(_T("Daemon: Failed send 'OK' packet. Error code %d"), ulWSAErrorCode);
            goto cleanUp;
        }

        LogVbs(_T("Daemon: Sent 'OK' packet"));

        switch(sNDPParams.dwNDPTestType)
        {
        case SEND_THROUGHPUT:

            // Send 'READY' packet
            if (!SendPacket(NDP_PACKET_READY, NULL, &ulWSAErrorCode))
            {
                LogErr(_T("Daemon: Failed send 'READY' packet. Error code %d"), ulWSAErrorCode);
                goto cleanUp;
            }

            //Wait async for DONE packet
            if (EndStressReceiveAsync(hAdapter) == FALSE)
            {
                LogErr(_T("Daemon: Failed to start async wait for 'DONE' packet"));
                goto cleanUp;
            }

            // Start stress receive
            if (!StressReceive(
                hAdapter, sNDPParams.dwPoolSize, NDP_PACKET_STRESS, peerAddr, &packetType, NULL,
                NULL, &result.time, &result.idleTime, &result.packetsReceived, &result.bytesReceived
                ))
            {
                    LogErr(_T("Daemon: Failed start stress receive"));
                    goto cleanUp;
            }

            LogVbs(_T("Daemon: Done with 'STRESS' packets"));

            // Send DONE packet
            if (!SendPacket(NDP_PACKET_DONE, NULL, &ulWSAErrorCode))
            {
                LogErr(_T("Daemon: Failed send 'DONE' packet. Error code %d"), ulWSAErrorCode);
                goto cleanUp;
            }
            break;

        case RECV_THROUGHPUT:

            //Wait for ready packet.
            if (!ReceivePacket(&packetType, NULL, &ulWSAErrorCode) ||
                packetType != NDP_PACKET_READY) 
            {
                    LogErr(_T("Daemon: Failed waiting for 'READY' packet. Error code %d"), ulWSAErrorCode);
                    goto cleanUp;
            }

            LogVbs(_T("Daemon: Got 'READY' packet"));

            Sleep(NDP_TIMEOUT_DELAY);
            LogVbs(_T("Daemon: Start send 'STRESS' packets"));

            // Send packets
            if (!StressSend(
                hAdapter, g_fUseNdisSendOnly, peerAddr, NDP_PACKET_STRESS, sNDPParams.dwPoolSize, sNDPParams.dwPacketSize,
                sNDPParams.dwNosOfPacketsToSend, sNDPParams.dwFlagBurstControl,
                sNDPParams.dwDelayInABurst, sNDPParams.dwPacketsInABurst,
                &result.time, &result.idleTime, &result.packetsReceived, &result.bytesReceived
                ))
            {
                    LogErr(_T("Daemon: Stress send failed"));
                    goto cleanUp;
            }

            LogVbs(_T("Daemon: Sent 'STRESS' packets"));

            // Wait a while before we end stress iteration
            Sleep(NDP_TIMEOUT_SHORT);

            // Send DONE packet
            if (!SendPacket(NDP_PACKET_DONE, NULL, &ulWSAErrorCode)) 
            {
                LogErr(_T("Daemon: Failed send 'DONE' packet. Error code %d"), ulWSAErrorCode);
                goto cleanUp;
            }

            LogVbs(_T("Daemon: Sent 'DONE' packet % time"),++dwSyncSteps);

            if (!ReceivePacket(&packetType, NULL, &ulWSAErrorCode) ||
                packetType != NDP_PACKET_DONE)
            {
                    LogErr(_T("Daemon: Failed waiting for 'DONE' packet"));
                    goto cleanUp;
            }

            break;
        case PING_THROUGHPUT:
            break;
        }

        //Wait For GET ME RESULT Packet from Test.
        if (!ReceivePacket(&packetType, NULL, &ulWSAErrorCode) ||
            packetType != NDP_PACKET_GET_RESULT) 
        {
                LogMsg(_T("Daemon: Failed when wait for 'RESULT' packet. Error code %d"), ulWSAErrorCode);
                goto cleanUp;
        }


        if (!SendPacket(NDP_PACKET_RESULT, (UCHAR*)&result, &ulWSAErrorCode)) 
        {
            LogErr(_T("Daemon: Failed send 'RESULT' packet. Error code %d"), ulWSAErrorCode);
            goto cleanUp;
        }

        LogVbs(_T("Daemon: Sent 'RESULT' packet"));

        LogVbs(_T("Daemon: Wait for 'DONE/NEXT' packet"));

        if (!ReceivePacket(&packetType, NULL, &ulWSAErrorCode)) 
        {
            LogMsg(_T("Daemon: Failed when wait for 'NEXT/EXIT' packet. Error code %d"), ulWSAErrorCode);
        }

        if (packetType != NDP_PACKET_NEXT && packetType != NDP_PACKET_EXIT)
        {
            LogErr(_T("Did not get NEXT/EXIT packet. Received %d packet."), packetType);
            goto cleanUp;
        }

        LogVbs(_T("Daemon: Received 'NEXT/EXIT' packet"));

        // Send OK packet
        if (!SendPacket(NDP_PACKET_OK, NULL, &ulWSAErrorCode))
        {
            LogErr(_T("Daemon: Failed send 'OK' packet. Error code %d"), ulWSAErrorCode);
            goto cleanUp;
        }

        *pPacketType = packetType;
        ok = TRUE;

cleanUp:
        return ok;
}

//------------------------------------------------------------------------------

int ndisd(TCHAR *szServAddr, ULONG ulPort)
{
    int rc = TPR_FAIL;
    UCHAR peerAddr[ETH_ADDR_SIZE];
    DWORD packetType = 0;
    DWORD dwConnections = 0;

    g_hFinishEvent = CreateEvent(NULL, FALSE, FALSE, _T("PERF_NDIS"));
    if (g_hFinishEvent == NULL)
    {
        g_hFinishEvent = NULL;
        LogErr(_T("ndisd:: Failed in creating the event. Error = %d"),GetLastError());
        return FALSE;
    }

    // If event already exists then set it and mark the exit
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        LogMsg(_T("First instance of Perf_ndis is found"));
        LogMsg(_T("Stopping the first instance and exiting this current instance"));
        SetEvent(g_hFinishEvent);
        CloseHandle(g_hFinishEvent);
        return FALSE;
    }

#ifndef UNDER_CE
    BOOL fSuccess = SetConsoleCtrlHandler(
        (PHANDLER_ROUTINE) CtrlHandler,  // handler function
        TRUE);                           // add to list

    if (!fSuccess)
    {
        LogErr(_T("Could not set control handler"));
    }
#endif

    if (!StartServer(szServAddr, ulPort))
    {
        goto cleanUp;
    }

    do
    {
        LogMsg(_T("Waiting for new client connection...Start running perf_ndis test on CE device..."));

        // Accept connection from peer
        if (!AcceptPeer(peerAddr))
        {
            goto cleanUp;
        }
        else
        {
            LogMsg(_T("Accepted client: %d connection"), ++dwConnections);
        }

        // Do iteration while we get next packets at end
        do {
            if (!ReceiveIterationWithPeer(g_hAdapter, peerAddr, &packetType))
            {
                goto cleanUp;
            }

            //Check if somebody wants us to leave
            if (WAIT_OBJECT_0 == WaitForSingleObject(g_hFinishEvent,5))
            {
                //Somebody wants us to leave.
                LogMsg(_T("user wants us to leave.OK."));
                goto cleanUp;
            }
        } while (packetType == NDP_PACKET_NEXT);

    } while (TRUE);

    rc = TPR_INFORMATIVE;
cleanUp:
    return rc;
}

//------------------------------------------------------------------------------

