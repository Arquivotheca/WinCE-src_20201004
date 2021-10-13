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
#ifdef UNDER_CE
#include "prettyprint.hxx"
#include "ndis_ceperf.h"
#endif
/////////////////////////////Externs

extern BOOL  g_receivePeer;
#ifdef UNDER_CE
extern BOOL g_noRelease;
extern BOOL g_perfScenarioEnded;
#endif
extern HANDLE g_hAdapter;
extern DWORD g_packetsSend;
extern DWORD g_minSize;
extern DWORD g_maxSize;
extern DWORD g_sizeStep;
extern DWORD g_poolSize;
extern teNDPTestType g_eTestType;
extern BOOL  g_RunAsPerfWinsock;
extern DWORD g_AddendumHdrSize;
extern BOOL g_fUseNdisSendOnly;
extern TCHAR  *g_szAdapter;

#ifdef UNDER_CE
// PerfScenario entry points.
extern PFN_PERFSCEN_OPENSESSION  g_lpfnPerfScenarioOpenSession;
extern PFN_PERFSCEN_CLOSESESSION g_lpfnPerfScenarioCloseSession;
extern PFN_PERFSCEN_ADDAUXDATA   g_lpfnPerfScenarioAddAuxData;
extern PFN_PERFSCEN_FLUSHMETRICS g_lpfnPerfScenarioFlushMetrics;

// Used by CePerf
HANDLE hPerfSession = INVALID_HANDLE_VALUE;
LPVOID pCePerfInternal;

#define MILLISEC_TO_SEC 0.001
#define MEGABYTES 1048576
#define BITS 8
#endif
//------------------------------------------------------------------------------

BOOL ConnectToPeer(const TCHAR * szServerAddress, ULONG ulPort, UCHAR peerAddr[])
{
    BOOL ok = FALSE;
    static UCHAR bcastAddr[ETH_ADDR_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    UCHAR srcAddr[ETH_ADDR_SIZE];
    teNDPPacketType packetType = NDP_PACKET_NONE;
    ULONG ulWSAErrorCode = 0;

    LogMsg(_T("Connecting to the server at IP %s Port %d"), szServerAddress, ulPort);

    if (StartClient(szServerAddress, ulPort, &ulWSAErrorCode) == FALSE)
    {
        LogMsg(_T("Failed to start client. Error code %d"), ulWSAErrorCode);
        return FALSE;
    }

    if (!GetMACAddress(g_szAdapter, srcAddr))
    {
        LogMsg(_T("Failed to retrieve local Mac address."));
        return FALSE;    
    }

    // Send PEER packet
    if (!SendPacket(NDP_PACKET_PEER, srcAddr, &ulWSAErrorCode))
    {
        LogErr(_T("Failed send 'PEER' packet. Error code %d"), ulWSAErrorCode);
        goto cleanUp;
    }

    LogVbs(_T("Sent 'PEER' packet"));
    LogVbs(_T("Wait for 'PEER' packet"));

    // Get PEER
    if (!ReceivePacket(&packetType, peerAddr, &ulWSAErrorCode) ||
        packetType != NDP_PACKET_PEER )
    {
        LogWrn(_T("Failed to receive 'PEER' packet"));
        goto cleanUp;
    }

    LogMsg(
        _T("Got 'PEER' packet from %02x:%02x:%02x:%02x:%02x:%02x"),
        peerAddr[0], peerAddr[1], peerAddr[2], peerAddr[3], peerAddr[4],
        peerAddr[5], peerAddr[6]
    );

    ok = TRUE;

cleanUp:
    return ok;
}

//------------------------------------------------------------------------------

BOOL ContinueWithPeer()
{
    BOOL ok = FALSE; 
    teNDPPacketType packetType = NDP_PACKET_NONE;
    ULONG ulWSAErrorCode = 0;

    // Send NEXT packet
    if (!SendPacket(NDP_PACKET_NEXT, 0, &ulWSAErrorCode))
    {
        LogErr(_T("Failed send 'NEXT' packet. Error code %d"), ulWSAErrorCode);
        goto cleanUp;
    }

    LogVbs(_T("Sent 'NEXT' packet"));
    LogVbs(_T("Wait for 'OK' packet"));

    // Wait for OK packet
    if (!ReceivePacket(&packetType, NULL, &ulWSAErrorCode) ||
        packetType != NDP_PACKET_OK) 
    {
        LogErr(_T("Failed waiting for 'OK' packet. Error code %d"), ulWSAErrorCode);
        goto cleanUp;
    }

    LogVbs(_T("Received 'OK' packet"));

    ok = TRUE;

cleanUp:
    return ok;
}

//------------------------------------------------------------------------------

BOOL DisconnectFromPeer()
{
    BOOL ok = FALSE;
    ULONG ulWSAErrorCode = 0;

    // Send Accept packet
    if (!SendPacket(NDP_PACKET_EXIT, 0, &ulWSAErrorCode))
    {
        LogErr(_T("Failed send 'EXIT' packet. Error code %d"), ulWSAErrorCode);
        goto cleanUp;
    }

    LogVbs(_T("Sent 'EXIT' packet"));

    ok = TRUE;

cleanUp:
    return ok;
}

//------------------------------------------------------------------------------
#ifdef NDIS60
BOOL SendIterationWithPeer(
    HANDLE hAdapter, PtsNDPParams pNDPParams, UCHAR peerAddr[], DWORD packetSize, 
    DWORD packetsSend, LONGLONG *pSendTime, LONGLONG *pSendIdleTime,
    DWORD *pPacketsSent, DWORD *pBytesSent, LONGLONG *pRecvTime, 
    LONGLONG *pRecvIdleTime, DWORD *pPacketsReceived, DWORD *pBytesReceived
    ) 
#else
BOOL SendIterationWithPeer(
    HANDLE hAdapter, PtsNDPParams pNDPParams, UCHAR peerAddr[], DWORD packetSize, 
    DWORD packetsSend, DWORD *pSendTime, DWORD *pSendIdleTime,
    DWORD *pPacketsSent, DWORD *pBytesSent, DWORD *pRecvTime, 
    DWORD *pRecvIdleTime, DWORD *pPacketsReceived, DWORD *pBytesReceived
    ) 
#endif
{
    BOOL ok = FALSE;
    teNDPPacketType packetType;
    NDP_STRESS_RESULT receiveResult;
    ULONG ulWSAErrorCode;

#ifdef NDIS60
    LONGLONG *pSendRecvTime = NULL, *pSendRecvIdleTime = NULL;
#else
    DWORD *pSendRecvTime = NULL, *pSendRecvIdleTime = NULL;
#endif

    DWORD *pPacketsSentRecv = NULL,*pBytesSentRecv = NULL;

    // Send PARAMS packet (poolSize for now)
    if (!SendPacket(NDP_PACKET_PARAMS, (UCHAR*)pNDPParams, &ulWSAErrorCode)) 
    {
        LogErr(_T("Failed send 'PARAMS' packet. Error code %d"), ulWSAErrorCode);
        goto cleanUp;
    }

    LogVbs(_T("Sent 'PARAMS' packet"));
    LogVbs(_T("Wait for 'OK' packet"));

    // Get results
    if (!ReceivePacket(&packetType, NULL, &ulWSAErrorCode) ||
        packetType != NDP_PACKET_OK) 
    {
        LogErr(_T("Failed waiting for 'OK' packet. Error code %d"), ulWSAErrorCode);
        goto cleanUp;
    }

    LogVbs(_T("Got 'OK' packet"));

    switch(pNDPParams->dwNDPTestType)
    {
    case SEND_THROUGHPUT:

        //Wait for ready packet.
        if (!ReceivePacket(&packetType, NULL, &ulWSAErrorCode) ||
            packetType != NDP_PACKET_READY) 
        {
            LogErr(_T("Failed waiting for 'READY' packet. Error code %d"), ulWSAErrorCode);
            goto cleanUp;
        }

        LogVbs(_T("Got 'READY' packet"));

        Sleep(NDP_TIMEOUT_DELAY);

        LogVbs(_T("Start send 'STRESS' packets"));

        // Send packets   
        if (!StressSend(
            hAdapter, g_fUseNdisSendOnly, peerAddr, NDP_PACKET_STRESS, pNDPParams->dwPoolSize, packetSize, 
            packetsSend, pNDPParams->dwFlagBurstControl, pNDPParams->dwDelayInABurst, pNDPParams->dwPacketsInABurst,
            pSendTime, pSendIdleTime, pPacketsSent, pBytesSent
            )) 
        {
            LogErr(_T("Stress send failed"));
            goto cleanUp;
        }

        LogVbs(_T("Sent 'STRESS' packets"));

        // Wait a while before we end stress iteration
        Sleep(NDP_TIMEOUT_SHORT);

        // Send DONE packet
        if (!SendPacket(NDP_PACKET_DONE, 0, &ulWSAErrorCode)) 
        {
            LogErr(_T("Failed send 'DONE' packet. Error code %d"), ulWSAErrorCode);
            goto cleanUp;
        }

        LogVbs(_T("Sent 'DONE' packet"));

        //Wait for Done packet from peer.
        if (!ReceivePacket(&packetType, NULL, &ulWSAErrorCode) ||
            packetType != NDP_PACKET_DONE)
        {
            LogErr(_T("Failed waiting for 'DONE' packet. Error code %d"), ulWSAErrorCode);
            goto cleanUp;
        }

        pSendRecvTime = pRecvTime;
        pSendRecvIdleTime = pRecvIdleTime;
        pPacketsSentRecv = pPacketsReceived;
        pBytesSentRecv = pBytesReceived;

        ok = TRUE;

        break;
    case RECV_THROUGHPUT:

        // Send 'READY' packet
        if (!SendPacket(NDP_PACKET_READY, 0, &ulWSAErrorCode))
        {
            LogErr(_T("Failed send 'READY' packet. Error code %d"), ulWSAErrorCode);
            goto cleanUp;
        }

        LogVbs(_T("Sent 'READY' packet"));

        //Wait async for DONE packet
        if (EndStressReceiveAsync(hAdapter) == FALSE) 
        {
            LogErr(_T("Daemon: Failed to start async wait for 'DONE' packet"));            
            goto cleanUp;
        }

        LogVbs(_T("Wait for 'STRESS' packets"));

        // Start stress receive
        if (!StressReceive(
            hAdapter, pNDPParams->dwPoolSize, NDP_PACKET_STRESS, peerAddr, &packetType, NULL,
            NULL, pRecvTime, pRecvIdleTime, pPacketsReceived, pBytesReceived
            ))
        {
            LogErr(_T("Failed start stress receive"));
            goto cleanUp;
        }

        LogVbs(_T("Done with 'STRESS' packets"));

        // Send DONE packet
        if (!SendPacket(NDP_PACKET_DONE, 0, &ulWSAErrorCode))
        {
            LogErr(_T("Failed send 'DONE' packet. Error code %d"), ulWSAErrorCode);
            goto cleanUp;
        }

        pSendRecvTime = pSendTime;
        pSendRecvIdleTime = pSendIdleTime;
        pPacketsSentRecv = pPacketsSent;
        pBytesSentRecv = pBytesSent;

        ok = TRUE;
        break;

    case PING_THROUGHPUT:
        break;
    }

    // Send GET RESULT packet
    if (!SendPacket(NDP_PACKET_GET_RESULT, 0, &ulWSAErrorCode)) 
    {
        LogErr(_T("Failed send 'RESULT' packet. Error code %d"), ulWSAErrorCode);
        goto cleanUp;
    }

    LogVbs(_T("Sent 'GET RESULT' packet"));

    // Wait a while before we end stress iteration
    Sleep(NDP_TIMEOUT_DELAY);

    LogVbs(_T("Wait for 'RESULT' packet"));

    // Get results
    if (!ReceivePacket(&packetType, (UCHAR*)&receiveResult, &ulWSAErrorCode) ||
        packetType != NDP_PACKET_RESULT) 
    {
        LogErr(_T("Failed waiting for 'RESULT' packet. Error code %d"), ulWSAErrorCode);
        goto cleanUp;
    }

    LogVbs(_T("Get 'RESULT' packet"));

    *pSendRecvTime = receiveResult.time;
    *pSendRecvIdleTime = receiveResult.idleTime;
    *pPacketsSentRecv = receiveResult.packetsReceived;
    *pBytesSentRecv = receiveResult.bytesReceived;

    ok = TRUE;

cleanUp:
    return ok;
}

//------------------------------------------------------------------------------
#ifdef NDIS60
BOOL SendIteration(
    HANDLE hAdapter, DWORD poolSize, DWORD packetSize, DWORD packetsSend, 
    LONGLONG *pSendTime, LONGLONG *pSendIdleTime, DWORD *pPacketsSent, 
    DWORD *pBytesSent
    ) 
#else
BOOL SendIteration(
    HANDLE hAdapter, DWORD poolSize, DWORD packetSize, DWORD packetsSend, 
    DWORD *pSendTime, DWORD *pSendIdleTime, DWORD *pPacketsSent, 
    DWORD *pBytesSent
    ) 
#endif
{
    BOOL ok = FALSE;
    static UCHAR dstAddr[] = {0x01, 0x02, 0x4, 0x06, 0x08, 0x0A};


    LogVbs(_T("Start send 'STRESS' packets"));

    // Send packets   
    if (!StressSend(
        hAdapter, g_fUseNdisSendOnly, dstAddr, NDP_PACKET_STRESS, poolSize, packetSize, 
        packetsSend, 0,0,0, pSendTime, pSendIdleTime, pPacketsSent, pBytesSent
        )) {
            LogErr(_T("Stress send failed"));
            goto cleanUp;
    }

    LogVbs(_T("Sent 'STRESS' packets"));

    ok = TRUE;

cleanUp:
    return ok;
}

//------------------------------------------------------------------------------

int SendPerf(const TCHAR * szServerAddress, ULONG ulPort)
{
    UCHAR peerAddr[ETH_ADDR_SIZE];
    DWORD packetSize = 0;
#ifdef NDIS60
    LONGLONG sendTime = 0, sendIdleTime = 0;
    LONGLONG receiveTime = 0, receiveIdleTime = 0;
#else
    DWORD sendTime = 0, sendIdleTime = 0;
    DWORD receiveTime = 0, receiveIdleTime; = 0
#endif
        LONG lSendTime = 0, lSendIdleTime = 0;
    DWORD packetsSent = 0, bytesSent = 0;
    LONG lReceiveTime = 0, lReceiveIdleTime = 0;
    DWORD packetsReceived = 0, bytesReceived = 0;
    float fBytesSent = 0, fBytesReceived = 0;

    int rc = TPR_FAIL;
    tsNDPParams sNDPParams = {g_poolSize,g_eTestType};

    float fMaxPeerSendSpeed = 0;
    float fRatio = 0;
    DWORD dwPercent = 100;
    BOOL bFindMaxPeerSendSpeed = FALSE;
    BOOL bFlagRecvTPDone = FALSE;
#ifdef UNDER_CE
    LPCTSTR lpszTestName = 0;
    LPCTSTR lpszScenarioNamespace = 0;
    TCHAR rgszScenarioName[32] = {0};
    TCHAR rgszSeperator[] = _T("; ");

    if (g_eTestType == SEND_THROUGHPUT) 
    {
        lpszTestName = _T("NDIS Send Throughput");
        lpszScenarioNamespace = _T("NDISPerf\\NDIS Send Throughput");
    }
    else if (g_eTestType == RECV_THROUGHPUT)
    {
        lpszTestName = _T("NDIS Recv Throughput");
        lpszScenarioNamespace = _T("NDISPerf\\NDIS Recv Throughput");
    }

    BOOL fRet = FALSE;
    CEPERF_SESSION_INFO ceperf_session_info = {0};
    ceperf_session_info.dwStorageFlags = CEPERF_STORE_DEBUGOUT;
    ceperf_session_info.wVersion = 1;
    HRESULT result = E_FAIL;
    LPCTSTR lpszPerfFilename = 0;
    TCHAR rgszAuxDataBuffer[16] = {0};

    //Initializing with default guid 
    // {BF60EFD7-A03B-4DF1-B87A-8B0748276B0F}
    GUID guidNdisPerf = { 0xbf60efd7, 0xa03b, 0x4df1, { 0xb8, 0x7a, 0x8b, 0x7, 0x48, 0x27, 0x6b, 0xf } };;

    //if  g_NoRelease is true results log file is saved to current directory
    //otherwise it is saved to release directory
    if (g_noRelease)
    {
        lpszPerfFilename = _T("\\perf_ndis6.xml");
    }
    else
    {
        lpszPerfFilename = _T("\\release\\perf_ndis6.xml");
    }

    PrettyPrint prettyPrint;
#endif

    // Connect to receive peer
    if (g_receivePeer)
    {
        if (!ConnectToPeer(szServerAddress, ulPort, peerAddr))
        {
            goto cleanUp;
        }
    }
#ifdef UNDER_CE
    //Open CePerf Session
    if (FALSE == g_perfScenarioEnded) 
    {
        LogMsg(_T("Opening CePerf & PerfScenario Sessions"));
        result = CePerfOpenSession(&hPerfSession, PERF_SESSION, DEFAULTSTATUSFLAGS, &ceperf_session_info);
        if (result != ERROR_SUCCESS)
        {
            LogMsg(_T("Opening CePerf Session failed"));
            PerfScenarioEnd();
        }
    }

    //Register Perf Items to log in CePerf
    if (FALSE == g_perfScenarioEnded) 
    {
        result = CePerfRegisterBulk(hPerfSession, g_rgPerfItems, _countof(g_rgPerfItems), 0);
        if (result != ERROR_SUCCESS)
        {
            LogMsg(_T("Registering CePerf Items failed"));
            PerfScenarioEnd();
        }
    }

    //Open PerfScenario Session
    if (FALSE == g_perfScenarioEnded)
    {
        result = g_lpfnPerfScenarioOpenSession(PERF_SESSION, TRUE);
        if (result != ERROR_SUCCESS) 
        {
            LogMsg(_T("Opening PerfScenario Session failed"));
            PerfScenarioEnd();
        }
    }
#endif
    packetSize = g_minSize;
    while (packetSize <= g_maxSize)
    {
#ifdef UNDER_CE
        //Each packet size will have unique GUID for the default packets.
        //All user selected packet sizes will have a single GUID.
        guidNdisPerf = GetGUIDforPacketSize(packetSize);
#endif
        if (g_receivePeer)
        {

            dwPercent = 100;
            bFindMaxPeerSendSpeed = FALSE;
            bFlagRecvTPDone = FALSE;

            sNDPParams.dwPacketSize = packetSize;
            sNDPParams.dwNosOfPacketsToSend = g_packetsSend;
            sNDPParams.dwFlagBurstControl = 0;
            sNDPParams.dwDelayInABurst = 0;
            sNDPParams.dwPacketsInABurst = 0;

            do
            {
                if (!SendIterationWithPeer(
                    g_hAdapter, &sNDPParams, peerAddr, packetSize, g_packetsSend, 
                    &sendTime, &sendIdleTime, &packetsSent, &bytesSent, &receiveTime, 
                    &receiveIdleTime, &packetsReceived, &bytesReceived
                    ))
                {
                    goto cleanUp;
                }

                lSendTime = (long) sendTime;
                lSendIdleTime = (long) sendIdleTime;
                lReceiveTime = (long) receiveTime;
                lReceiveIdleTime = (long) receiveIdleTime;
                fBytesSent = (float) bytesSent;
                fBytesReceived = (float) bytesReceived;

                LogMsg(
                    _T("Send %6d packets : size %4d bytes : pool %d packets : ")
                    _T("%5d ms : %5.1f MB : %4.1f Mbps : %8.1f pps : %4.1f idle"), 
                    packetsSent, packetSize, g_poolSize, lSendTime, fBytesSent/1048576.0,
                    (fBytesSent * 8.0 * 1000.0)/(lSendTime*1048576.0), 
                    packetsSent * 1000.0/lSendTime, (100.0 * lSendIdleTime)/lSendTime
                    );
                LogMsg(
                    _T("Recv %6d packets : size %4d bytes : pool %d packets : ")
                    _T("%5d ms : %5.1f MB : %4.1f Mbps : %8.1f pps : %4.1f idle : ")
                    _T("%4.1f lost"), 
                    packetsReceived, packetSize, g_poolSize, lReceiveTime, 
                    fBytesReceived/1048576.0, 
                    (fBytesReceived * 8000.0)/(lReceiveTime*1048576.0), 
                    packetsReceived * 1000.0/lReceiveTime, 
                    (100.0*lReceiveIdleTime)/lReceiveTime, 
                    100.0 - (fBytesReceived*100.0) /fBytesSent
                    );
#ifdef UNDER_CE
                //Adding Statistic data for Ceperf log
                if (FALSE == g_perfScenarioEnded)
                {
                    result = CePerfSetStatistic(g_rgPerfItems[PERF_ITEM_NDIS_SEND_TIME].hTrackedItem,
                        lSendTime);
                    if (result != ERROR_SUCCESS)
                    {
                        LogMsg(_T("Unable to set CePerf Statistic failed for Send Time"));
                        goto SkipStatistics;
                    }

                    result = CePerfSetStatistic(g_rgPerfItems[PERF_ITEM_NDIS_SEND_BYTES].hTrackedItem,
                        fBytesSent/MEGABYTES);
                    if (result != ERROR_SUCCESS)
                    {
                        LogMsg(_T("Unable to set CePerf Statistic failed for Send Bytes"));
                        goto SkipStatistics;
                    }

                    result = CePerfSetStatistic(g_rgPerfItems[PERF_ITEM_NDIS_SEND_RATE].hTrackedItem,
                        (fBytesSent * BITS )/(lSendTime * MILLISEC_TO_SEC * MEGABYTES));
                    if (result != ERROR_SUCCESS) 
                    {
                        LogMsg(_T("Unable to set CePerf Statistic failed for Send Rate"));
                        goto SkipStatistics;
                    }

                    result = CePerfSetStatistic(g_rgPerfItems[PERF_ITEM_NDIS_SEND_PPS].hTrackedItem,
                        (packetsSent /lSendTime * MILLISEC_TO_SEC));
                    if (result != ERROR_SUCCESS)
                    {
                        LogMsg(_T("Unable to set CePerf Statistic failed for Send PPS"));
                        goto SkipStatistics;
                    }

                    result = CePerfSetStatistic(g_rgPerfItems[PERF_ITEM_NDIS_PACKETS_RECV].hTrackedItem,
                        packetsReceived);
                    if (result != ERROR_SUCCESS)
                    {
                        LogMsg(_T("Unable to set CePerf Statistic failed for Packets Recv"));
                        goto SkipStatistics;
                    }

                    result = CePerfSetStatistic(g_rgPerfItems[PERF_ITEM_NDIS_RECV_TIME].hTrackedItem,
                        lReceiveTime);
                    if (result != ERROR_SUCCESS) 
                    {
                        LogMsg(_T("Unable to set CePerf Statistic failed for Recv Time"));
                        goto SkipStatistics;
                    }

                    result = CePerfSetStatistic(g_rgPerfItems[PERF_ITEM_NDIS_RECV_BYTES].hTrackedItem,
                        fBytesReceived/MEGABYTES);
                    if (result != ERROR_SUCCESS)
                    {
                        LogMsg(_T("Unable to set CePerf Statistic failed Recv Bytes"));
                        goto SkipStatistics;
                    }

                    result = CePerfSetStatistic(g_rgPerfItems[PERF_ITEM_NDIS_RECV_RATE].hTrackedItem,
                        (fBytesReceived * BITS)/(lReceiveTime* MILLISEC_TO_SEC * MEGABYTES));
                    if (result != ERROR_SUCCESS) 
                    {
                        LogMsg(_T("Unable to set CePerf Statistic failed Recv Rate"));
                        goto SkipStatistics;
                    }

                    result = CePerfSetStatistic(g_rgPerfItems[PERF_ITEM_NDIS_RECV_PPS].hTrackedItem,
                        packetsReceived /lReceiveTime * MILLISEC_TO_SEC);
                    if (result != ERROR_SUCCESS)
                    {
                        LogMsg(_T("Unable to set CePerf Statistic failed for Recv PPS"));
                        goto SkipStatistics;
                    }

                    result = CePerfSetStatistic(g_rgPerfItems[PERF_ITEM_NDIS_PACKETS_LOST].hTrackedItem,
                        (100.0 - (fBytesReceived*100.0) /fBytesSent));
                    if (result != ERROR_SUCCESS)
                    {
                        LogMsg(_T("Unable to set CePerf Statistic failed for Packets Lost"));
                        goto SkipStatistics;
                    }
                }

SkipStatistics:
                if (result != ERROR_SUCCESS) 
                {
                    PerfScenarioEnd();
                }
#endif
                if (!bFindMaxPeerSendSpeed)
                {
                    bFindMaxPeerSendSpeed = TRUE;
                    //Speed in Bytes/second
                    fMaxPeerSendSpeed = float(bytesSent * 1000.0)/(sendTime);
                    sNDPParams.dwFlagBurstControl = 1;
                    LogMsg(_T("\t\t Max Send throughput by daemon = %8.2f bytes/sec"),fMaxPeerSendSpeed);
#ifdef UNDER_CE
                    //Adding Statistic data for Ceperf log
                    if (FALSE == g_perfScenarioEnded)
                    {
                        result = CePerfSetStatistic(g_rgPerfItems[PERF_ITEM_NDIS_MAX_SEND_THROUGHPUT_DAEMON].hTrackedItem,
                            fMaxPeerSendSpeed);
                        if (result != ERROR_SUCCESS)
                        {
                            LogMsg(_T("Unable to set CePerf Statistic failed for Max Throughput Daemon"));
                            PerfScenarioEnd();
                        }
                    }
#endif
                }

                LogMsg(_T("\t\t Packets Sent = %d, Packets Received = %d"),packetsSent,packetsReceived);

                if (packetsReceived < packetsSent)
                {
                    dwPercent = dwPercent - 5;
                    if (dwPercent <= 0)
                    {
#ifdef UNDER_CE                   
                        prettyPrint.AddTuple(
                            PrintTuple(packetSize, 
                            (fBytesReceived * 8000.0f)/(lReceiveTime*1048576.0f),
                            packetsReceived)
                            );
#endif
                        bFlagRecvTPDone = TRUE;
                        break;
                    }

                    fRatio = float(sNDPParams.dwPacketSize) * float(1000.0) * ((float(100.0)/float(dwPercent)) -1)/fMaxPeerSendSpeed;
                    LogMsg(_T("\t\t Offsetting daemon's  Send throughput to %d percent of MaxSpeed, delay/packets = %8.2f"),
                        dwPercent,fRatio);

                    if (fRatio >= 1.0)
                    {
                        sNDPParams.dwDelayInABurst = DWORD (fRatio * 10.0);
                        sNDPParams.dwPacketsInABurst = 10;
                    }
                    else
                    {
                        sNDPParams.dwPacketsInABurst = DWORD (10.0/fRatio);
                        sNDPParams.dwDelayInABurst = 10;
                    }

                    LogMsg(_T("\t\t Packets in a burst = %d, delay in a burst = %d"),
                        sNDPParams.dwPacketsInABurst,sNDPParams.dwDelayInABurst);

                    if (!ContinueWithPeer()) 
                    {
                        goto cleanUp;
                    }
                }
                else
                {
#ifdef UNDER_CE                   
                    prettyPrint.AddTuple(
                        PrintTuple(packetSize, 
                        (fBytesReceived * 8000.0f)/(lReceiveTime*1048576.0f),
                        packetsReceived)
                        );
#endif
                    bFlagRecvTPDone = TRUE;
                }

            } while (!bFlagRecvTPDone);

        }
        else
        {

            if (!SendIteration(
                g_hAdapter, g_poolSize, packetSize, g_packetsSend, &sendTime, 
                &sendIdleTime, &packetsSent, &bytesSent
                )) 
            {
                goto cleanUp;
            }

            LogMsg(
                _T("Send %6d packets : size %4d bytes : pool %d packets : ")
                _T("%5d ms : %4.1f Mbps : %8.1f pps : %4.1f idle"), 
                packetsSent, packetSize, g_poolSize, (long) sendTime, 
                (bytesSent * 8.0 * 1000.0)/(((long) sendTime)*1048576.0), 
                packetsSent * 1000.0/((long) sendTime), (100.0 * ((long) sendIdleTime))/((long) sendTime)
                );

        }
#ifdef UNDER_CE
        //Adding aux data for Ceperf log
        if (FALSE == g_perfScenarioEnded)
        {
            result = StringCchPrintf(rgszAuxDataBuffer, _countof(rgszAuxDataBuffer), TEXT("%d"), packetSize);
            if (result != S_OK)
            {
                LogMsg(_T("Copying Packet Size to Aux Data Buffer failed"));
                goto SkipPerfScenario;
            }

            result = g_lpfnPerfScenarioAddAuxData(L"Packet Size (Bytes)", rgszAuxDataBuffer);
            if (result != ERROR_SUCCESS) 
            {
                LogMsg(_T("Adding Packet Size value to PerfScenario AuxData failed"));
                goto SkipPerfScenario;
            }

            result = StringCchCat(rgszScenarioName, _countof(rgszScenarioName), lpszTestName);
            if (result != S_OK) {
                LogMsg(_T("Adding Scenario Name failed"));
                goto SkipPerfScenario;
            }

            result = StringCchCat(rgszScenarioName, _countof(rgszScenarioName), rgszSeperator);
            if (result != S_OK) 
            {
                LogMsg(_T("Adding Scenario Name failed"));
                goto SkipPerfScenario;
            }

            result = StringCchCat(rgszScenarioName, _countof(rgszScenarioName), rgszAuxDataBuffer);
            if (result != S_OK) 
            {
                LogMsg(_T("Adding Scenario Name failed"));
                goto SkipPerfScenario;
            }

            result = StringCchPrintf(rgszAuxDataBuffer, _countof(rgszAuxDataBuffer), TEXT("%d"), packetsSent);
            if (result != S_OK)
            {
                LogMsg(_T("Copying Packets Sent to Aux Data Buffer failed"));
                goto SkipPerfScenario;
            }

            result = g_lpfnPerfScenarioAddAuxData(L"Packets Sent", rgszAuxDataBuffer);
            if (result != ERROR_SUCCESS) 
            {
                LogMsg(_T("Adding Packets Sent value to PerfScenario AuxData failed"));
                goto SkipPerfScenario;
            }

            //Sending data to output xml file
            result = g_lpfnPerfScenarioFlushMetrics(
                FALSE,
                &guidNdisPerf,
                lpszScenarioNamespace,
                rgszScenarioName,
                lpszPerfFilename,
                NULL,
                NULL
                );
            if (result != ERROR_SUCCESS) 
            {
                LogMsg(_T("Flushing Ceperf data to log file failed"));
                goto SkipPerfScenario;
            }

            //reset the scenario name variable
            result = StringCchCopy(rgszScenarioName, _countof(rgszScenarioName), _T("\0"));
            if (result != S_OK) 
            {
                LogMsg(_T("Resetting Scenario name failed"));
                goto SkipPerfScenario;
            }
        }

SkipPerfScenario:
        if (result != S_OK)
        {
            PerfScenarioEnd();
        }
#endif
        if (packetSize >= g_maxSize) break;      

        if (!g_RunAsPerfWinsock)
        {
            packetSize += g_sizeStep;
        }
        else
        {
            g_sizeStep = g_sizeStep << 1;
            packetSize = g_sizeStep + g_AddendumHdrSize;
        }

        if (packetSize > ETH_MAX_FRAME_SIZE) packetSize = ETH_MAX_FRAME_SIZE;

        if (packetSize <= g_maxSize && g_receivePeer)
        {
            if (!ContinueWithPeer()) goto cleanUp;
        }
    }
#ifdef UNDER_CE
    //Closing Ceperf and PerfScenario sessions
    if (FALSE == g_perfScenarioEnded) 
    {
        PerfScenarioEnd();
    }

    prettyPrint.Print();
#endif

    // Disconnect from peer
    if (g_receivePeer) 
    {
        if (!DisconnectFromPeer()) goto cleanUp;
    }

    rc = TPR_INFORMATIVE;
cleanUp:
    return rc;
}
#ifdef UNDER_CE
static GUID GetGUIDforPacketSize(DWORD dwPacketSize)
{
    int nGUIDIndex = 0;
    if (((63 < dwPacketSize) && (dwPacketSize < g_maxSize)) && ((dwPacketSize % 64) == 0))
    {
        nGUIDIndex = dwPacketSize/64;
        return s_guidNdisPerfPacketSizes[nGUIDIndex - 1];
    }
    else if (dwPacketSize == g_maxSize)
    {
        nGUIDIndex = dwPacketSize/64;
        return s_guidNdisPerfPacketSizes[nGUIDIndex];
    }
    else
    {
        return s_guidNdisPerfDefault;
    }
}

static void PerfScenarioEnd()
{
    //Closing Ceperf and PerfScenario sessions
    if (g_perfScenarioEnded == FALSE) 
    {
        LogMsg(_T("Closing CePerf & PerfScenario Sessions"));
        g_lpfnPerfScenarioCloseSession(PERF_SESSION);
        CePerfDeregisterBulk(g_rgPerfItems, NUM_PERF_ITEMS);
        CePerfCloseSession(hPerfSession);
    }

    g_perfScenarioEnded = TRUE;
}
//------------------------------------------------------------------------------
#endif