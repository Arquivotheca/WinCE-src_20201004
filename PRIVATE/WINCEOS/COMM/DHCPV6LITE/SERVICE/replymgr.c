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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++



Module Name:

    replymgr.c

Abstract:

    Reply manager for DhcpV6 client.

Author:

    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/

#include "dhcpv6p.h"
//#include "precomp.h"
//#include "replymgr.tmh"


enum {
    SERVICE_STOP_EVENT = 0,
    RECV_MSG_EVENT = 1,
    EVENT_COUNT,
};


VOID
CALLBACK
DHCPV6ReplyMgrReplyCallback(
    PVOID pvContext1,
    PVOID pvContext2
    )
{
    DWORD dwError = 0;
    PDHCPV6_ADAPT pDhcpV6Adapt = pvContext1;
    PDHCPV6_REPLY_MESSAGE pDhcpV6ReplyMessage = pvContext2;
    PDHCPV6_MESSAGE_HEADER pDhcpV6MsgHdr = NULL;
    ULONG uRemainingMessageLength = pDhcpV6ReplyMessage->uRecvMessageLength;
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule = &pDhcpV6Adapt->DhcpV6OptionModule;
    PUCHAR pucOptionStart = NULL;


    DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_TRACE, ("Begin Process Reply Packet for Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    AcquireExclusiveLock(&pDhcpV6Adapt->RWLock);
    if ((dhcpv6_state_deinit == pDhcpV6Adapt->DhcpV6State) ||
        ((! gbDHCPV6PDEnabled) && 
        pDhcpV6Adapt->DhcpV6State >= dhcpv6_state_configured)) {
        
        dwError = ERROR_ALREADY_EXISTS;
        DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_WARN, ("Adapt: %d Already Configured", pDhcpV6Adapt->dwIPv6IfIndex));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    if (uRemainingMessageLength < sizeof(DHCPV6_MESSAGE_HEADER)) {
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_WARN, ("Packet: Invalid Message Length - Discarding"));
        BAIL_ON_WIN32_ERROR(dwError);
    }
    uRemainingMessageLength -= sizeof(DHCPV6_MESSAGE_HEADER);
    pDhcpV6MsgHdr = (PDHCPV6_MESSAGE_HEADER)pDhcpV6ReplyMessage->ucMessage;


    if (dhcpv6_state_solicit == pDhcpV6Adapt->DhcpV6State) {
        if (DHCPV6_MESSAGE_TYPE_ADVERTISE != pDhcpV6MsgHdr->MessageType) {
            dwError = ERROR_INVALID_PARAMETER;
            DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_WARN, 
                ("Not an advertise Packet - Discarding"));
            BAIL_ON_WIN32_ERROR(dwError);
        }
    } else if (pDhcpV6MsgHdr->MessageType != DHCPV6_MESSAGE_TYPE_REPLY) {
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_WARN, ("Not a Reply Packet - Discarding"));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    if (0 != memcmp(pDhcpV6MsgHdr->TransactionID, pDhcpV6Adapt->ucTransactionID, sizeof(pDhcpV6Adapt->ucTransactionID))) {
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_WARN, ("Reply Packet: Recv'd Transaction ID does not match - Discarding"));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    pucOptionStart = pDhcpV6ReplyMessage->ucMessage + sizeof(DHCPV6_MESSAGE_HEADER);

    dwError = DhcpV6OptionMgrProcessReply(
                    pDhcpV6Adapt,
                    pDhcpV6OptionModule,
                    pucOptionStart,
                    uRemainingMessageLength
                    );

#ifdef UNDER_CE
    if (ERROR_INVALID_LEVEL == dwError) {
        DEBUGMSG(ZONE_WARN, 
            (TEXT("!DhcpV6: Server returned prefix not available\r\n")));
        
        // got a Status no prefix avail, but couldn't handle in the lower level
        if (dhcpv6_state_request != pDhcpV6Adapt->DhcpV6State) {
            dwError = DhcpV6TimerCancel(gpDhcpV6TimerModule, pDhcpV6Adapt);

            if (pDhcpV6Adapt->pPdOption) {
                DHCPV6_IA_PREFIX *pPrefix = &pDhcpV6Adapt->pPdOption->IAPrefix;

                // delete existing prefix
                DHCPv6ManagePrefix(
                    pDhcpV6Adapt->dwIPv6IfIndex,
                    pPrefix->PrefixAddr,
                    pPrefix->cPrefix,
                    0, 0);
                DHCPv6ManagePrefixPeriodicCleanup();
                
                FreeDHCPV6Mem(pDhcpV6Adapt->pPdOption);
                pDhcpV6Adapt->pPdOption = NULL;
            }

            DeleteRegistrySettings(pDhcpV6Adapt, DEL_REG_PREFIX_FL);

            pDhcpV6Adapt->DhcpV6State = dhcpv6_state_request;
            SetInitialTimeout(pDhcpV6Adapt, DHCPV6_INF_TIMEOUT, 
                DHCPV6_INF_MAX_RT, 0);

            dwError = DhcpV6EventAddEvent(gpDhcpV6EventModule,
                pDhcpV6Adapt->dwIPv6IfIndex, DHCPV6MessageMgrInfoReqCallback,
                pDhcpV6Adapt, NULL);
            
            ASSERT(! dwError);
            goto error;

        }
    }
#endif

    BAIL_ON_WIN32_ERROR(dwError);

    dwError = DhcpV6TimerCancel(gpDhcpV6TimerModule, pDhcpV6Adapt);

    if (dhcpv6_state_solicit == pDhcpV6Adapt->DhcpV6State) {
        pDhcpV6Adapt->DhcpV6State = dhcpv6_state_srequest;
        SetInitialTimeout(pDhcpV6Adapt, DHCPV6_REQ_TIMEOUT, DHCPV6_REQ_MAX_RT, 
            DHCPV6_REQ_MAX_RC);

        dwError = DhcpV6EventAddEvent(gpDhcpV6EventModule,
            pDhcpV6Adapt->dwIPv6IfIndex, DHCPV6MessageMgrRequestCallback,
            pDhcpV6Adapt, NULL);
        
        ASSERT(! dwError);
        BAIL_ON_WIN32_ERROR(dwError);

    } else {
        if (dhcpv6_state_configured != pDhcpV6Adapt->DhcpV6State) {
            
            if (pDhcpV6Adapt->pPdOption) {
                BOOL    Status;
                DHCPV6_IA_PREFIX *pPrefix = &pDhcpV6Adapt->pPdOption->IAPrefix;

                SetInitialTimeout(pDhcpV6Adapt, DHCPV6_REN_TIMEOUT, 
                    DHCPV6_REN_MAX_RT, 0);
                
                Status = DHCPv6ManagePrefix(pDhcpV6Adapt->dwIPv6IfIndex, 
                    pPrefix->PrefixAddr, pPrefix->cPrefix, 
                    pPrefix->ValidLifetime, pPrefix->PreferedLifetime);

                DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_WARN,
                    ("Prefix %sapplied; received on interface: %d", Status?"":"NOT ", pDhcpV6Adapt->dwIPv6IfIndex));
            }
        }

        SaveRegistrySettings(pDhcpV6Adapt);

        pDhcpV6Adapt->DhcpV6State = dhcpv6_state_configured;

#if 0
        // no longer valid since server may return no prefix avail
        ASSERT(pDhcpV6Adapt->pPdOption || (! gbDHCPV6PDEnabled) ||
            (0 == (DHCPV6_PD_ENABLED_FL & pDhcpV6Adapt->Flags)));
#endif

        if (pDhcpV6Adapt->pPdOption)  {
            DWORD msT1;

            msT1 = pDhcpV6Adapt->pPdOption->T1 * 1000;
            dwError = DhcpV6TimerSetTimer(gpDhcpV6TimerModule, pDhcpV6Adapt, 
                DHCPV6MessageMgrTimerCallbackRoutine, pDhcpV6Adapt, msT1);

            ASSERT(!dwError);
        }
        BAIL_ON_WIN32_ERROR(dwError);
    }
    

error:

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

    DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_TRACE, ("End Process Reply Packet for Adapt: %d with Error: %!status!", pDhcpV6Adapt->dwIPv6IfIndex, dwError));

    if (pDhcpV6Adapt) {
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
    }

    if (pDhcpV6ReplyMessage) {
        FreeDHCPV6Mem(pDhcpV6ReplyMessage);
    }

    return;
}


DWORD
IniDhcpV6ProcessRecv(
    PDHCPV6_REPLY_MODULE pDhcpV6ReplyModule
    )
{
    DWORD dwError = 0;
    WSAMSG WsaMsg = { 0 };
    LPFN_WSARECVMSG WsaRecvMsg = NULL;
    GUID WsaRecvMsgGuid = WSAID_WSARECVMSG;
    BOOL bResetEvent = FALSE;

    DWORD dwBytesReturned = 0;
    PWSACMSGHDR pWsaMsgHdr = NULL;
    WSABUF WsaBuf = { 0 };
    UCHAR ucControl[256] = { 0 };
    PDHCPV6_REPLY_MESSAGE pDhcpV6ReplyMessage = NULL;
    ULONG uMessageLength = 0;
    PWSACMSGHDR pWsaCMsgHdr = NULL;
    PDHCPV6_ADAPT pDhcpV6Adapt = NULL;
    BOOL bInterfaceIndexFound = FALSE;


    DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_TRACE, ("Begin Received Packet"));

    uMessageLength = sizeof(DHCPV6_REPLY_MESSAGE);
    pDhcpV6ReplyMessage = AllocDHCPV6Mem(uMessageLength);
    if (!pDhcpV6ReplyMessage) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_WIN32_ERROR(dwError);
    }
    memset(pDhcpV6ReplyMessage, 0, uMessageLength);

    if (WSAIoctl(pDhcpV6ReplyModule->Socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &WsaRecvMsgGuid, sizeof(WsaRecvMsgGuid),
                 &WsaRecvMsg, sizeof(WsaRecvMsg),
                 &dwBytesReturned, NULL, NULL) == SOCKET_ERROR)
    {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_ERROR, ("FAILED WSAIoctl(WsaRecvMsg) with Error: %!status!", dwError));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    WsaMsg.name = (PSOCKADDR)&pDhcpV6ReplyModule->SockAddrIn6;
    WsaMsg.namelen = sizeof(pDhcpV6ReplyModule->SockAddrIn6);
    WsaMsg.lpBuffers = &WsaBuf;
    WsaMsg.dwBufferCount = 1;
    WsaMsg.Control.len = sizeof(ucControl);
    WsaMsg.Control.buf = ucControl;
    WsaBuf.len = sizeof(pDhcpV6ReplyMessage->ucMessage);
    WsaBuf.buf = pDhcpV6ReplyMessage->ucMessage;

    dwError = WsaRecvMsg(
                pDhcpV6ReplyModule->Socket,
                &WsaMsg,
                &pDhcpV6ReplyMessage->uRecvMessageLength,
                NULL,
                NULL
                );
    if(dwError == SOCKET_ERROR) {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_ERROR, ("FAILED WsaRecvMsg with Error: %!status!", dwError));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    bResetEvent = WSAResetEvent(pDhcpV6ReplyModule->WSAEventReadReady);
    if (!bResetEvent) {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_ERROR, ("FAILED WSAResetEvent(WSAEventReadReady) with Error: %!status!", dwError));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    for (pWsaCMsgHdr = WSA_CMSG_FIRSTHDR(&WsaMsg); pWsaCMsgHdr != NULL; pWsaCMsgHdr = WSA_CMSG_NXTHDR(&WsaMsg, pWsaCMsgHdr)) {
        if (pWsaCMsgHdr->cmsg_level == IPPROTO_IPV6 &&
            pWsaCMsgHdr->cmsg_type == IPV6_PKTINFO)
        {
            IN6_PKTINFO *pIn6PktInfo = (IN6_PKTINFO*)WSA_CMSG_DATA(pWsaCMsgHdr);


            bInterfaceIndexFound = TRUE;
            dwError = DHCPV6AdaptFindAndReference(
                        pIn6PktInfo->ipi6_ifindex,
                        &pDhcpV6Adapt
                        );
            BAIL_ON_WIN32_ERROR(dwError);

            dwError = DhcpV6EventAddEvent(
                        gpDhcpV6EventModule,
                        pIn6PktInfo->ipi6_ifindex,
                        DHCPV6ReplyMgrReplyCallback,
                        pDhcpV6Adapt,
                        pDhcpV6ReplyMessage
                        );
            BAIL_ON_WIN32_ERROR(dwError);

            break;
        }
    }

    if (bInterfaceIndexFound == FALSE) {
        dwError = ERROR_NOT_FOUND;
        BAIL_ON_WIN32_ERROR(dwError);
    }

    DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_TRACE, ("End Received Packet with Error: %!status!", dwError));

    return dwError;

error:

    if (pDhcpV6Adapt) {
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
    }

    if (pDhcpV6ReplyMessage) {
        FreeDHCPV6Mem(pDhcpV6ReplyMessage);
    }

    DhcpV6Trace(DHCPV6_RECEIVE, DHCPV6_LOG_LEVEL_WARN, ("End Received Packet with Error: %!status!", dwError));

    return dwError;
}


DWORD
IniDhcpV6ReplyMgrRecvWorker(
    PDHCPV6_REPLY_MODULE pDhcpV6ReplyModule
    )
{
    DWORD dwError = 0;
    WSAEVENT WsaEvents[EVENT_COUNT] = { 0 };
    DWORD dwStatus = 0;
    BOOL bDoneWaiting = FALSE;


    WsaEvents[SERVICE_STOP_EVENT] = pDhcpV6ReplyModule->WSAEventShutdown;
    WsaEvents[RECV_MSG_EVENT] = pDhcpV6ReplyModule->WSAEventReadReady;

    while (!bDoneWaiting) {

        dwStatus = WSAWaitForMultipleEvents(
                        EVENT_COUNT,
                        WsaEvents,
                        FALSE,
                        WSA_INFINITE,
                        FALSE
                        );

        switch(dwStatus) {

        case WSA_WAIT_EVENT_0 + SERVICE_STOP_EVENT:

            dwError = ERROR_SUCCESS;
            bDoneWaiting = TRUE;
            DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_TRACE, ("WSAWaitForMultipleEvents(SERVICE_STOP_EVENT) Signalled"));
            break;

        case WSA_WAIT_EVENT_0 + RECV_MSG_EVENT:

            IniDhcpV6ProcessRecv(pDhcpV6ReplyModule);

            break;

        case WSA_WAIT_TIMEOUT:
#ifndef UNDER_CE
        case WAIT_IO_COMPLETION:
#endif
            ASSERT(0);
            break;

        default:

            ASSERT(0);
            dwError = ERROR_INVALID_EVENT_COUNT;
            bDoneWaiting = TRUE;
            break;

        }
    }

    return dwError = 0;
}


DWORD
IniDhcpV6XXReplyMgrRecvWorker(
CTEEvent *pEvent, VOID *pVoid) {
#ifdef UNDER_CE
    HANDLE hThd;

    hThd = CreateThread(NULL, 0, IniDhcpV6ReplyMgrRecvWorker, pVoid, 0, NULL);
    if (hThd) {
        CeSetThreadPriority(hThd, CeGetThreadPriority(GetCurrentThread()));
        CloseHandle(hThd);
        return 0;
    }
#endif
    return IniDhcpV6ReplyMgrRecvWorker(pVoid);
}


DWORD
InitDhcpV6ReplyManager(
    PDHCPV6_REPLY_MODULE pDhcpV6ReplyModule
    )
{
    DWORD dwError = 0;
    DWORD dwTrue = 1;
    BOOL bWIQueue = FALSE;


    DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_TRACE, ("Initializing Receive Module"));

    memset(pDhcpV6ReplyModule, 0, sizeof(DHCPV6_REPLY_MODULE));

    pDhcpV6ReplyModule->Socket = WSASocket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
    if (pDhcpV6ReplyModule->Socket == INVALID_SOCKET) {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_FATAL, ("FAILED WSASocket with Error: %!status!", dwError));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    pDhcpV6ReplyModule->SockAddrIn6.sin6_family = AF_INET6;
    pDhcpV6ReplyModule->SockAddrIn6.sin6_port = htons(DHCPV6_CLIENT_LISTEN_PORT);

    dwError = bind(pDhcpV6ReplyModule->Socket, (PSOCKADDR)&pDhcpV6ReplyModule->SockAddrIn6, sizeof(pDhcpV6ReplyModule->SockAddrIn6));
    if(dwError == SOCKET_ERROR) {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_FATAL, ("FAILED bind with Error: %!status!", dwError));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    if(setsockopt(pDhcpV6ReplyModule->Socket, IPPROTO_IPV6, IPV6_PKTINFO, (PCHAR)&dwTrue, sizeof(DWORD)) == SOCKET_ERROR){
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_FATAL, ("FAILED setsockopt(IPPROTO_IPV6, IPV6_PKTINFO) with Error: %!status!", dwError));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    pDhcpV6ReplyModule->WSAEventShutdown = WSACreateEvent();
    if (pDhcpV6ReplyModule->WSAEventShutdown == WSA_INVALID_EVENT) {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_FATAL, ("FAILED WSACreateEvent with Error: %!status!", dwError));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    pDhcpV6ReplyModule->WSAEventReadReady = WSACreateEvent();
    if (pDhcpV6ReplyModule->WSAEventReadReady == WSA_INVALID_EVENT) {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_FATAL, ("FAILED WSACreateEvent with Error: %!status!", dwError));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    dwError = WSAEventSelect(pDhcpV6ReplyModule->Socket, pDhcpV6ReplyModule->WSAEventReadReady, FD_READ);
    if (dwError == SOCKET_ERROR) {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_FATAL, ("FAILED WSAEventSelect(FD_READ) with Error: %!status!", dwError));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    bWIQueue = QueueUserWorkItem(
                    IniDhcpV6XXReplyMgrRecvWorker,
                    pDhcpV6ReplyModule,
                    WT_EXECUTELONGFUNCTION
                    );
    if (!bWIQueue) {
        dwError = GetLastError();
        DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_FATAL, ("FAILED QueueUserWorkItem(IniDhcpV6ReplyMgrRecvWorker) with Error: %!status!", dwError));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_TRACE, ("End Initializing Receive Module"));

    return dwError;

error:

    if (pDhcpV6ReplyModule->WSAEventReadReady != WSA_INVALID_EVENT) {
        WSACloseEvent(pDhcpV6ReplyModule->WSAEventReadReady);
    }

    if (pDhcpV6ReplyModule->WSAEventShutdown != WSA_INVALID_EVENT) {
        WSACloseEvent(pDhcpV6ReplyModule->WSAEventShutdown);
    }

    if (pDhcpV6ReplyModule->Socket != INVALID_SOCKET) {
        closesocket(pDhcpV6ReplyModule->Socket);
    }

    DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_ERROR, ("End Initializing Receive Module with Error: %!status!", dwError));

    return dwError;
}


DWORD
DeInitDhcpV6ReplyManager(
    PDHCPV6_REPLY_MODULE pDhcpV6ReplyModule
    )
{
    DWORD dwError = 0;


    DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_TRACE, ("Begin DeInitializing Receive Module"));

    // Close the event
    dwError = WSAEventSelect(pDhcpV6ReplyModule->Socket, pDhcpV6ReplyModule->WSAEventReadReady, 0);
    if (dwError == SOCKET_ERROR) {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_ERROR, ("Error Cancelling WSAEventSelect(WSAEventReadReady) with Error: %!status!", dwError));
        ASSERT(0);
    }
    WSACloseEvent(pDhcpV6ReplyModule->WSAEventReadReady);

    if (!WSASetEvent(pDhcpV6ReplyModule->WSAEventShutdown)) {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_ERROR, ("Error Signalling WSAEventShutdown with Error: %!status!", dwError));
        ASSERT(0);
    }
    WSACloseEvent(pDhcpV6ReplyModule->WSAEventShutdown);

    if (closesocket(pDhcpV6ReplyModule->Socket) == SOCKET_ERROR) {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_ERROR, ("Error Closing Recv Socket with Error: %!status!", dwError));
    }

    DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_TRACE, ("End DeInitializing Receive Module"));

    return dwError;
}

