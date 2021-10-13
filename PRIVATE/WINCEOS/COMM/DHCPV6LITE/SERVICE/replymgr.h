//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++



Module Name:

    replymgr.h

Abstract:

    Reply manager for DhcpV6 client



    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/


#define DHCPV6_MAX_MESSAGE_LENGTH 1500


typedef struct _DHCPV6_REPLY_MESSAGE {
    ULONG uRecvMessageLength;
    UCHAR ucMessage[DHCPV6_MAX_MESSAGE_LENGTH];
} DHCPV6_REPLY_MESSAGE, * PDHCPV6_REPLY_MESSAGE;

typedef struct _DHCPV6_REPLY_MODULE {
    SOCKET Socket;
    SOCKADDR_IN6 SockAddrIn6;
    WSAEVENT WSAEventReadReady;
    WSAEVENT WSAEventShutdown;
} DHCPV6_REPLY_MODULE, * PDHCPV6_REPLY_MODULE;


DWORD
InitDhcpV6ReplyManager(
    PDHCPV6_REPLY_MODULE pDhcpV6ReplyModule
    );

DWORD
DeInitDhcpV6ReplyManager(
    PDHCPV6_REPLY_MODULE pDhcpV6ReplyModule
    );

