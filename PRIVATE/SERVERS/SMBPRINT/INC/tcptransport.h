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

#include "SMB_Globals.h"
#include "FixedAlloc.h"
struct SMB_PACKET;


#define TCP_CONNECTION_HLDR_ALLOC          pool_allocator<10, CONNECTION_HOLDER *>


/******************************************************************************/
/*  PUBLIC FUNCTIONS (used by anyone)                                         */
/******************************************************************************/

//starts the NETBIOS transport layer
HRESULT StartTCPTransport();

//stops the NETBIOS transport layer
HRESULT StopTCPTransport();
HRESULT HaltTCPIncomingConnections();
HRESULT QueueTCPPacketForSend(SMB_PACKET *pPacket, BOOL fDestruct); 


class CONNECTION_HOLDER {
   public:
        void * operator new(size_t size) { 
            return SMB_Globals::g_ConnectionHolderAllocator.allocate(size);
       }
       void   operator delete(void *mem) {
           SMB_Globals::g_ConnectionHolderAllocator.deallocate(mem);
       }   
       SOCKET sock;
       HANDLE hHandle;
       LONG   refCnt;
       CRITICAL_SECTION csSockLock;
};



/******************************************************************************/
/*  PRIVATE FUNCTIONS (used by TCPTranport.cpp)                               */
/*       NOT to be used by ANYONE else!!  just here for readability of .cpp   */
/******************************************************************************/
namespace TCP_TRANSPORT {     
     extern ce::list<CONNECTION_HOLDER *, TCP_CONNECTION_HLDR_ALLOC> g_ConnectionList;
     extern CRITICAL_SECTION              g_csConnectionList;
     extern SOCKET                        g_ListenSocket;
     extern HANDLE                        g_ListenThreadHandle;
     extern BOOL                          g_fShutDown;
     extern const USHORT                  g_usTCPListenPort;
     extern const UINT                    g_uiTCPTimeoutInSeconds;
     extern BOOL                          g_fIsRunning;
IFDBG(extern LONG                         g_lAliveSockets;);
     extern UniqueID                      g_ConnectionID;
    
    DWORD SMBSRV_TCPListenThread(LPVOID netnum);   
    DWORD SMBSRV_TCPProcessingThread(LPVOID netnum);
}


#endif
