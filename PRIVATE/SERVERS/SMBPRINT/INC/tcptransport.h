//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef TCP_TRANSPORT_H
#define TCP_TRANSPORT_H
#include <winsock2.h>
#include <list.hxx>
#include <block_allocator.hxx>

#include "SMB_Globals.h"
struct SMB_PACKET;

struct LISTEN_NODE {
    HANDLE h;
    SOCKET s;
    BYTE LANA;
    UINT uiIPAddress;
};

#ifndef NO_POOL_ALLOC
#define TCP_CONNECTION_HLDR_ALLOC          ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(CONNECTION_HOLDER*) >
#define TCP_LISTEN_NODE_ALLOC              ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(LISTEN_NODE) >
//#define TCP_LISTEN_SOCKET_HLDR_ALLOC       ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(SOCKET) >
#else
#define TCP_CONNECTION_HLDR_ALLOC         ce::allocator 
#define TCP_LISTEN_NODE_ALLOC             ce::allocator 
//#define TCP_LISTEN_SOCKET_HLDR_ALLOC      ce::allocator 
#endif


/******************************************************************************/
/*  PUBLIC FUNCTIONS (used by anyone)                                         */
/******************************************************************************/

//starts the NETBIOS transport layer
HRESULT StartTCPTransport();

//stops the NETBIOS transport layer
HRESULT StopTCPTransport();
HRESULT HaltTCPIncomingConnections();

HRESULT TCP_GetSocketName(SMB_PACKET *pPacket, struct sockaddr *pSockAddr, int *pNameLen);
HRESULT TCP_TerminateSession(ULONG ulConnectionID);



class CONNECTION_HOLDER;



/******************************************************************************/
/*  PRIVATE FUNCTIONS (used by TCPTranport.cpp)                               */
/*       NOT to be used by ANYONE else!!  just here for readability of .cpp   */
/******************************************************************************/
namespace TCP_TRANSPORT {     
     extern ce::list<CONNECTION_HOLDER *, TCP_CONNECTION_HLDR_ALLOC > g_ConnectionList;
     extern ce::list<LISTEN_NODE, TCP_LISTEN_NODE_ALLOC >          g_SocketListenList;
     extern CRITICAL_SECTION              g_csLockTCPTransportGlobals;
//     extern SOCKET                        g_ListenSocket;
//     extern HANDLE                        g_ListenThreadHandle;
     extern BOOL                          g_fStopped;
     extern const USHORT                  g_usTCPListenPort;
     extern const UINT                    g_uiTCPTimeoutInSeconds;
     IFDBG(extern LONG                         g_lAliveSockets;);
     extern UniqueID                      g_ConnectionID;     
     extern ce::fixed_block_allocator<10>          g_ConnectionHolderAllocator;
    
    DWORD SMBSRV_TCPListenThread(LPVOID netnum);   
    DWORD SMBSRV_TCPProcessingThread(LPVOID netnum);
    HRESULT QueueTCPPacketForSend(SMB_PACKET *pPacket, BOOL fDestruct); 
    HRESULT CopyTCPTransportToken(VOID *pToken, VOID **pNewToken);
    HRESULT DeleteTCPTransportToken(VOID *pToken);
}


class CONNECTION_HOLDER {
   public:
        void * operator new(size_t size) { 
            return TCP_TRANSPORT::g_ConnectionHolderAllocator.allocate(size);
       }
       void   operator delete(void *mem, size_t size) {
           TCP_TRANSPORT::g_ConnectionHolderAllocator.deallocate(mem, size);
       }   
       SOCKET sock;
       HANDLE hHandle;
       LONG   refCnt;
       CRITICAL_SECTION csSockLock;
       ULONG ulConnectionID;

       HANDLE hOverlappedHandle;
};

#endif


