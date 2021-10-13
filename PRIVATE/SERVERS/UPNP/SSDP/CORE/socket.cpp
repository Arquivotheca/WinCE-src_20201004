//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


File Name:

    socket.c

Abstract:

    This file contains code which opens and close ssdp socket, send and receive over the socket.



Created: 07/10/1999

--*/

#include <ssdppch.h>
#pragma hdrstop

#include "mstcpip.h"
#include "ssdpnetwork.h"
#include "ssdpparser.h"
#include "ipsupport.h"
#include "auto_xxx.hxx"
#include "upnp_config.h"

#define WSA_MAX_MAJOR 0x02
#define WSA_MAX_MINOR 0x00

#define RECEIVE_BUFFER_SIZE 256

#define HEADER_PREFIX "01"
#define EXTENSION_URI "\"http://schemas.upnp.org/upnp/1/0/\""
#define NLS_HEADER "Nls"
#define NLS_FORMAT "%04x%04x-%04x-%04x-%04x-%04x%04x%04x"

static BOOLEAN bStopRecv = FALSE;
static LONG bStartup = 0;

const struct in6_addr in6addrSSDP = SSDP_ADDR_IPV6; //  SSDP IPv6 Multicast Address
const struct in_addr inaddrSSDP = SSDP_ADDR_IPV4;   //  SSDP IPv4 Multicast Address

SOCKADDR_IN  MulticastAddressIPv4;
SOCKADDR_IN6 MulticastLinkScopeAddressIPv6;
SOCKADDR_IN6 MulticastSiteScopeAddressIPv6;

CHAR g_pszExtensionURI[500] = EXTENSION_URI;
CHAR g_pszHeaderPrefix[10] = HEADER_PREFIX;
CHAR g_pszNlsHeader[50] = NLS_HEADER;
CHAR g_pszNlsFormat[65] = NLS_FORMAT;

// SocketInit() returns 0 on success, and places failure codes in GetLastError()
INT SocketInit()
{
    INT iRet;
    WSADATA wsadata;
    WORD wVersionRequested = MAKEWORD(WSA_MAX_MAJOR, WSA_MAX_MINOR);

    // WinSock version negotiation. Use 2.0
    iRet = WSAStartup(wVersionRequested, &wsadata);
    if ( iRet != 0)
    {
        if (iRet == WSAVERNOTSUPPORTED)
        {
            TraceTag(ttidError, "WSAStartup failed with error %d. DLL "
                     "supports version higher than 2.0, but not also 2.0.",
                     GetLastError());
            
            return -1;
        }
        else
        {
            TraceTag(ttidError, "WSAStartup failed with error %d.", GetLastError());
            return -1;
        }
    }

    if (wVersionRequested != wsadata.wVersion)
    {
        TraceTag(ttidError, "Major version supported is below our min. requirement.");

        int result = WSACleanup();

        if (0 == result)
        {
            ::SetLastError(WSAVERNOTSUPPORTED);
        }

        return -1;
    }

    // we are implementing UPnP over IPv6 when the architecture specification is not final
    // following registry entries will allow us to tweak the implementation to some minor change in the spec
    
    HKEY hkey = NULL;
    
    if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"COMM\\UPnP\\IPv6Impl", 0, 0, &hkey))
    {
        DWORD cbSize;
        WCHAR pwszBuffer[500];
        
        if(ERROR_SUCCESS == RegQueryValueEx(hkey, L"ExtensionURL", NULL, NULL, reinterpret_cast<LPBYTE>(pwszBuffer), &(cbSize = sizeof(pwszBuffer))))
            wcstombs(g_pszExtensionURI, pwszBuffer, sizeof(g_pszExtensionURI));
            
        if(ERROR_SUCCESS == RegQueryValueEx(hkey, L"HeaderPrefix", NULL, NULL, reinterpret_cast<LPBYTE>(pwszBuffer), &(cbSize = sizeof(pwszBuffer))))
            wcstombs(g_pszHeaderPrefix, pwszBuffer, sizeof(g_pszHeaderPrefix));
            
        if(ERROR_SUCCESS == RegQueryValueEx(hkey, L"NlsHeader", NULL, NULL, reinterpret_cast<LPBYTE>(pwszBuffer), &(cbSize = sizeof(pwszBuffer))))
            wcstombs(g_pszNlsHeader, pwszBuffer, sizeof(g_pszNlsHeader));
        
        if(ERROR_SUCCESS == RegQueryValueEx(hkey, L"NlsFormat", NULL, NULL, reinterpret_cast<LPBYTE>(pwszBuffer), &(cbSize = sizeof(pwszBuffer))))
            wcstombs(g_pszNlsFormat, pwszBuffer, sizeof(g_pszNlsFormat));
        
        RegCloseKey(hkey);
    }
    
    
    //Set up the Multicast Addresses
    MulticastAddressIPv4.sin_family = AF_INET;
    MulticastAddressIPv4.sin_port = htons(SSDP_PORT);
    MulticastAddressIPv4.sin_addr = inaddrSSDP;
    
    MulticastLinkScopeAddressIPv6.sin6_family = AF_INET6;
    MulticastLinkScopeAddressIPv6.sin6_flowinfo = 0;
    MulticastLinkScopeAddressIPv6.sin6_scope_id = 0; 
    MulticastLinkScopeAddressIPv6.sin6_port = htons(SSDP_PORT);
    MulticastLinkScopeAddressIPv6.sin6_addr = in6addrSSDP;
    
    MulticastSiteScopeAddressIPv6 = MulticastLinkScopeAddressIPv6;
    MulticastSiteScopeAddressIPv6.sin6_addr.u.Byte[1] = upnp_config::site_scope();

    InterlockedIncrement(&bStartup);

    TraceTag(ttidSsdpSocket, "WSAStartup suceeded.");

    return 0;
}

BOOL SocketClose(SOCKET socketToClose);

// Pre-Conditon: SocketInit was succesful.
// Post-Conditon: Create a socket on a given interface.
BOOL SocketOpen(SOCKET *psocketToOpen, PSOCKADDR IpAddress, DWORD dwMulticastInterfaceIndex, PSOCKADDR pMulticastAddres)
{
    INT iRet;
    INT iTTL;
    DWORD dw;
    SOCKET socketSSDP;
    DWORD dwSockAddrSize = 0;

	// Create a socket to listen on the multicast channel

    socketSSDP = socket(IpAddress->sa_family, SOCK_DGRAM, 0);
    if (socketSSDP == INVALID_SOCKET)
    {
        TraceTag(ttidError, "Failed to create socket (%d)",
                 GetLastError());
        return FALSE;
    }

    // Bind
    dwSockAddrSize = (IpAddress->sa_family == AF_INET6 ? sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN ) );
    iRet = bind(socketSSDP, (struct sockaddr *)IpAddress, dwSockAddrSize);
    if (iRet == SOCKET_ERROR)
    {
        TraceTag(ttidError, "bind failed with error (%d)",
                 GetLastError());
        SocketClose(socketSSDP);
        *psocketToOpen = INVALID_SOCKET;
        return FALSE;
    }

    TraceTag(ttidSsdpSocket, "bound to address succesfully");

    // Bind to adapter index to work around week host model of TCPIP stack
    WSAIoctl(socketSSDP, SIO_INDEX_BIND, &dwMulticastInterfaceIndex, sizeof(dwMulticastInterfaceIndex), NULL, 0, &dw, NULL, NULL);
    
    // Socket Options
    //
    char *optVal=NULL;
    int   optLevel =0;
    int   option =0;
    int   optLen =0;

    if (pMulticastAddres)
    {
        struct ip_mreq mreqv4;
        struct ipv6_mreq mreqv6;
        
        if (IpAddress->sa_family == AF_INET)
        {
            // Setup the v4 option values and ip_mreq structure
            
            mreqv4.imr_multiaddr.s_addr = ((SOCKADDR_IN*)pMulticastAddres)->sin_addr.s_addr;
            mreqv4.imr_interface.s_addr = ((SOCKADDR_IN*)IpAddress)->sin_addr.s_addr;

            optLevel = IPPROTO_IP;
            option   = IP_ADD_MEMBERSHIP;
            optVal   = (char *)& mreqv4;
            optLen   = sizeof(mreqv4);
        }
        else if (IpAddress->sa_family == AF_INET6)
        {
            // Setup the v6 option values and ipv6_mreq structure
            
            mreqv6.ipv6mr_multiaddr =  ((SOCKADDR_IN6*)pMulticastAddres)->sin6_addr;
            mreqv6.ipv6mr_interface =  dwMulticastInterfaceIndex; 

            TraceTag(ttidSsdpSocket,"IPv6 scope Id %d",((SOCKADDR_IN6 *)IpAddress)->sin6_scope_id);

            optLevel = IPPROTO_IPV6;
            option   = IPV6_ADD_MEMBERSHIP;
            optVal   = (char *) &mreqv6;
            optLen   = sizeof(mreqv6);
        }
        else //unhandled family
        {
                TraceTag(ttidSsdpSocket,"Attempting to join multicast group for invalid address family!\n");
                SocketClose(socketSSDP);
                *psocketToOpen = INVALID_SOCKET;
                return FALSE;
        }
        
        // Join the multicast group
        if (setsockopt(socketSSDP, optLevel, option, optVal, optLen) == SOCKET_ERROR)
        {
            TraceTag(ttidError, "Join mulitcast group failed with error "
                     "%d.",GetLastError());
            SocketClose(socketSSDP);
            *psocketToOpen = INVALID_SOCKET;
            return FALSE;
        }
        TraceTag(ttidSsdpSocket, "Ready to listen on multicast channel.");
    }

    // set the interface to send multicast packets from
    if (IpAddress->sa_family == AF_INET)
    {
        // Set the options for V4
        optLevel = IPPROTO_IP;
        option   = IP_MULTICAST_IF;
        optVal   = (char *)&((SOCKADDR_IN*)IpAddress)->sin_addr.s_addr;
        optLen   = sizeof(((SOCKADDR_IN*)IpAddress)->sin_addr.s_addr);
    }
    else if (IpAddress->sa_family == AF_INET6)
    {
        // Set the options for V6
        optLevel = IPPROTO_IPV6;
        option   = IPV6_MULTICAST_IF;
        optVal   = (char*)&dwMulticastInterfaceIndex;
        optLen   = sizeof(dwMulticastInterfaceIndex);
    }
    else //unhandled family
    {
       TraceTag(ttidSsdpSocket,"Attempting to set sent interface for invalid address family");
        SocketClose(socketSSDP);
        *psocketToOpen = INVALID_SOCKET;
        return FALSE;
    }
    
    if (setsockopt(socketSSDP, optLevel, option, optVal, optLen) == SOCKET_ERROR)
    {
        TraceTag(ttidSsdpSocket, "Error %d occured in setting"
                 "IP_MULTICAST_IF option", GetLastError());
        SocketClose(socketSSDP);
        *psocketToOpen = INVALID_SOCKET;
        return FALSE;
    }

    
    // Set Multicast TTL
    //
    if (IpAddress->sa_family == AF_INET)
    {
        // Set the options for V4
        optLevel = IPPROTO_IP;
        option   = IP_MULTICAST_TTL;
        optVal   = (char *) &(iTTL = upnp_config::TTL());
        optLen   = sizeof(iTTL);
    }
    else if (IpAddress->sa_family == AF_INET6)
    {
        // Set the options for V6
        optLevel = IPPROTO_IPV6;
        option   = IPV6_MULTICAST_HOPS;
        optVal   = (char *) &(iTTL = upnp_config::IPv6TTL());
        optLen   = sizeof(iTTL);
    }
    else //unhandled family
    {
        TraceTag(ttidSsdpSocket, "Attempting to set TTL for invalid address family!");
        SocketClose(socketSSDP);
        *psocketToOpen = INVALID_SOCKET;
        return FALSE;
    }
    
    
    if (setsockopt(socketSSDP, optLevel, option, optVal, optLen) == SOCKET_ERROR)
    {
        TraceTag(ttidSsdpSocket, "Error %d occured in setting  "
                 "IP_MULTICAST_TTL option with value %d.", GetLastError(),
                 iTTL);
        SocketClose(socketSSDP);
        *psocketToOpen = INVALID_SOCKET;
        return FALSE;
    }

    // Check if Address not loopback (IPv4 only)
    if ((!AddressIsLoopback(  (struct sockaddr *)IpAddress)) && (IpAddress->sa_family == AF_INET))
    {
        BOOL bVal = FALSE;
        DWORD dwBytesReturned = 0;

        TraceTag(ttidSsdpSocket, "Address is IPv4 and not Loopback, so set SIO_MULTIPOINT_LOOPBACK.");

        if (WSAIoctl(socketSSDP, SIO_MULTIPOINT_LOOPBACK, &bVal, sizeof(bVal), NULL, 0, &dwBytesReturned, NULL, NULL) == SOCKET_ERROR)
        {
            TraceTag(ttidSsdpSocket, "Error %d occured in setting SIO_MULTIPOINT_LOOPBACK option with value %d.", GetLastError(), bVal);
            SocketClose(socketSSDP);
            *psocketToOpen = INVALID_SOCKET;
            return FALSE;
        }
    }
    else
    {
        TraceTag(ttidSsdpSocket, "Address is IPv6 or IPv4 Loopback, so don't set SIO_MULTIPOINT_LOOPBACK.");
    }
    
    TraceTag(ttidSsdpSocket, "Ready to send on multicast channel.");

    *psocketToOpen = socketSSDP;
    
    return TRUE;
}


// SocketClose
BOOL SocketClose(SOCKET socketToClose)
{
    shutdown(socketToClose, 1);
	closesocket(socketToClose);
    return TRUE;
}

// SocketFinish
VOID SocketFinish()
{
    if (InterlockedExchange(&bStartup, bStartup) != 0)
    {
        WSACleanup();
    }
}

// SocketSend
VOID SocketSend(const CHAR *szBytes, SOCKET socket, PSOCKADDR RemoteAddress)
{
    INT iBytesToSend, iBytesSent;
    DWORD  dwSockAddrSize = 0;  //  **    Ipv6 Port    **    Size of remote addr storage

    TraceTag(ttidSsdpSocket, "Sending ssdp message");

    ASSERT(RemoteAddress != NULL);

    // set remote addr size according to family
    dwSockAddrSize = (RemoteAddress->sa_family == AF_INET6 ? sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN ) );

    iBytesToSend = strlen(szBytes);
    
    // To-Do: make sure the size is no larger than the UDP limit.
    iBytesSent = sendto(socket, szBytes, iBytesToSend, 0, (struct sockaddr *) RemoteAddress, dwSockAddrSize);
    
    if (iBytesSent == SOCKET_ERROR)
    {
        TraceTag(ttidError, "socket 0x%x : Failed to send SSDP message - error: 0x%x", socket, GetLastError());
    }
    else if (iBytesSent != iBytesToSend)
    {
        TraceTag(ttidError, "Only sent %d bytes instead of %d bytes.", iBytesSent, iBytesToSend);
    }
    else
    {
        TraceTag(ttidSsdpSocket, "socket 0x%x : SSDP message was sent successfully", socket);
    }
}


// SocketReceive
BOOL SocketReceive(SOCKET socket, CHAR **pszData, PSOCKADDR_STORAGE fromSocket)
{
    u_long BufferSize;
    u_long BytesReceived;
    CHAR *ReceiveBuffer;
    SOCKADDR_STORAGE RemoteSocket;
    INT SocketSize = sizeof(RemoteSocket);

    if (ioctlsocket(socket, FIONREAD, &BufferSize) != 0)
    {
        TraceTag(ttidSsdpSocket, "Error: ioctlsocket(%x,FIONREAD) failed ",socket);
        return FALSE;
    }

    ReceiveBuffer = (CHAR *) malloc(sizeof(CHAR) * (BufferSize+1));
    if (ReceiveBuffer == NULL)
    {
        TraceTag(ttidError, "Error: failed to allocate LargeBuffer of "
                 "(%d) bytes",BufferSize + 1);
        return FALSE;
    }

    BytesReceived = recvfrom(socket, ReceiveBuffer, BufferSize, 0,
                             (struct sockaddr *) &RemoteSocket, &SocketSize);

    DWORD err = WSAGetLastError();
    
    if (BytesReceived == SOCKET_ERROR)
    {
        free(ReceiveBuffer);
        TraceTag(ttidSsdpSocket, "Error: recvfrom failed with error code (%d)",
                 WSAGetLastError());
        return FALSE;
    }
    else
    {
        ASSERT(BytesReceived <= BufferSize);

        ReceiveBuffer[BytesReceived] = '\0';

        TraceTag(ttidSsdpRecv, "Received packet: \n%s\n", ReceiveBuffer); 

        *pszData = ReceiveBuffer;

        *fromSocket = RemoteSocket;

        return TRUE;
    }
}
