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
#include <winsock2.h>
#include "Ws2tcpip.h"

#include "ipsupport.h"

#ifndef _SSDPNETWORK_
#define _SSDPNETWORK_

#define SSDP_NETWORK_SIGNATURE 0x1602

typedef enum _NetworkState {
    NETWORK_INIT,
    NETWORK_ACTIVE_NO_MASTER,
    NETWORK_ACTIVE_W_MASTER,
    NETWORK_CLEANUP
} NetworkState, *PNetworkState;

typedef struct _SSDPNetwork 
{

   LIST_ENTRY       linkage;

   INT              Type;

   NetworkState     state;

   SOCKET           socket;
   
   DWORD            dwIndex;
   DWORD            dwScopeId;
   DWORD            dwScope;
   
   PSOCKADDR        pMulticastAddr;
   CHAR             pszMulticastAddr[MAX_ADDRESS_SIZE];
   CHAR             pszAddressString[MAX_ADDRESS_SIZE];
   CHAR             pszIPString[MAX_ADDRESS_SIZE];

} SSDPNetwork, *PSSDPNetwork;

typedef VOID (*RECEIVE_CALLBACK_FUNC)(CHAR *szBuffer, PSOCKADDR *RemoteSocket);

// network related functions.

VOID InitializeListNetwork();

INT GetNetworks();
INT StartNetworkMonitorThread();
INT StopNetworkMonitorThread();

VOID GetNetworkLock();

VOID FreeNetworkLock();

VOID CleanupListNetwork();

PSSDPNetwork GetNextNetwork(PSSDPNetwork prev);

INT ListenOnAllNetworks(HWND hWnd);

void StopListenOnAllNetworks();

// socket related functions

INT SocketInit();

VOID SocketFinish();

// open the socket and bind
BOOL SocketOpen(SOCKET *psocketToOpen, PSOCKADDR IpAddress, DWORD dwMulticastInterfaceIndex, PSOCKADDR fRecvMcast);

// close the socket
BOOL SocketClose(SOCKET socketToClose);

BOOL SocketReceive(SOCKET socket, CHAR **pszData, PSOCKADDR_STORAGE fromSocket);

VOID SocketSend(const CHAR *szBytes, SOCKET socket, PSOCKADDR RemoteAddress);

#ifdef NEVER
VOID SuspendListening();
#endif // NEVER

#endif // SSDPNETWORK
