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

#pragma once

#define WSS_IOCTL_INVOKE    100

typedef  ce::byte_array_w_alignment <DWORD> sockaddr_buf;
typedef  ce::byte_array_w_alignment <DWORD> byte_array_dw_aligned;


const int MaxWsaBufs = 5;

enum WSS_IOCTL_FN_VALUES {
    WSS_STARTUP = 0,
    WSS_CLEANUP,
    WSS_SOCKET,
    WSS_BIND,
    WSS_ACCEPT,
    WSS_CLOSESOCKET,
    WSS_CONNECT,
    WSS_GETSOCKOPT,
    WSS_IOCTLSOCKET,
    WSS_LISTEN,
    WSS_RECV,
    WSS_RECVFROM,
    WSS_SELECT,
    WSS_SEND,
    WSS_SENDTO,
    WSS_SETSOCKOPT,
    WSS_SHUTDOWN,
    WSS_FREEADDRINFO,
    WSS_GETADDRINFO,
    WSS_GETHOSTBYADDR,
    WSS_GETHOSTBYNAME,
    WSS_GETHOSTNAME,
    WSS_GETNAMEINFO,
    WSS_GETPEERNAME,
    WSS_GETSOCKNAME,
    WSS_SETHOSTNAME,
    WSS_WSASOCKET,
    WSS_WSAACCEPT,
    WSS_WSAADDRTOSTRING,
    WSS_CLOSEEVENT,
    WSS_CREATEEVENT,
    WSS_ENUMNAMESPACE,
    WSS_ENUMNETWORKEVENTS,
    WSS_ENUMPROTOCOLS,
    WSS_EVENTSELECT,
    WSS_GETOVERLAPPEDRESULT,
    WSS_WSAHTONL,
    WSS_WSAHTONS,
    WSS_WSAIOCTL,
    WSS_NSPIOCTL,
    WSS_WSARECV,
    WSS_WSARECVFROM,
    WSS_WSARESETEVENT,
    WSS_WSASEND,
    WSS_WSASENDTO,
    WSS_WSASETEVENT,
    WSS_SETSERVICE,
    WSS_WSASTRINGTOADDR,
    WSS_WSAWAITFORMULTIPLE,
    WSS_WSACONTROL,
    
};


