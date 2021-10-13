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

    replymgr.h

Abstract:

    Reply manager for DhcpV6 client

Author:

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

