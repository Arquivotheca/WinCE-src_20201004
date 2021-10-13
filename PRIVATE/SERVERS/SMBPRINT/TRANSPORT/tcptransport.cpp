//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "SMB_Globals.h"
#include "TCPTransport.h"
#include "Cracker.h"
#include "CriticalSection.h"
#include "Utils.h"

using namespace TCP_TRANSPORT;

ce::list<CONNECTION_HOLDER *, TCP_CONNECTION_HLDR_ALLOC > TCP_TRANSPORT::g_ConnectionList;
//ce::list<SOCKET, TCP_LISTEN_HANDLE_HLDR_ALLOC > TCP_TRANSPORT::g_SocketListenList;
ce::list<LISTEN_NODE, TCP_LISTEN_NODE_ALLOC > TCP_TRANSPORT::g_SocketListenList;



ce::fixed_block_allocator<10> TCP_TRANSPORT::g_ConnectionHolderAllocator;
CRITICAL_SECTION              TCP_TRANSPORT::g_csLockTCPTransportGlobals;
BOOL                          TCP_TRANSPORT::g_fStopped = TRUE;
const USHORT                  TCP_TRANSPORT::g_usTCPListenPort = 445;
const UINT                    TCP_TRANSPORT::g_uiTCPTimeoutInSeconds = 0xFFFFFFFF;//15;
UniqueID                      TCP_TRANSPORT::g_ConnectionID;


IFDBG(LONG                    TCP_TRANSPORT::g_lAliveSockets = 0);

//
// Forward declares
VOID DecrementConnectionCounter(CONNECTION_HOLDER *pMyConnection);
VOID IncrementConnectionCounter(CONNECTION_HOLDER *pMyConnection);

//
//  Start the TCP transport -- this includes
//     initing any global variables before threads get spun
//     etc
HRESULT StartTCPTransport()
{
    TRACEMSG(ZONE_INIT, (TEXT("SMBSRV:Starting TCP transport")));
    HRESULT hr = E_FAIL;
    WORD wVersionRequested = MAKEWORD( 2, 2 );
    WSADATA wsaData;

    //
    // Initialize globals
    ASSERT(0 == g_SocketListenList.size());

    g_fStopped = FALSE;

    InitializeCriticalSection(&TCP_TRANSPORT::g_csLockTCPTransportGlobals);

    if (0 != WSAStartup( wVersionRequested, &wsaData )) {
        TRACEMSG(ZONE_INIT, (TEXT("SMBSRV: error with WSAStartup: %d"), WSAGetLastError()));
        hr = E_UNEXPECTED;
        goto Done;
    }

    hr = S_OK;
    Done:
        if(FAILED(hr)) {
            DeleteCriticalSection(&TCP_TRANSPORT::g_csLockTCPTransportGlobals);
        }
        return hr;
}

HRESULT StartTCPListenThread(UINT uiIPAddress, BYTE LANA)
{
    HRESULT hr = E_FAIL;
    HANDLE h;
    CCritSection csLock(&TCP_TRANSPORT::g_csLockTCPTransportGlobals);
    csLock.Lock();
    LISTEN_NODE newNode;
    LISTEN_NODE *pNode;

    ASSERT(FALSE == g_fStopped);

    if(!g_SocketListenList.push_front(newNode)) {
        hr = E_OUTOFMEMORY;
        goto Done;
    }
    pNode = &(g_SocketListenList.front());

    if(NULL == (h = CreateThread(NULL, 0, SMBSRV_TCPListenThread, (LPVOID)pNode, CREATE_SUSPENDED, NULL))) {
       TRACEMSG(ZONE_INIT, (L"SMBSRV: CreateThread failed starting TCP Listen:%d", GetLastError()));
       ASSERT(FALSE);
       goto Done;
    }
    pNode->LANA = LANA;
    pNode->s = INVALID_SOCKET;
    pNode->h = h;
    pNode->uiIPAddress = uiIPAddress;

    hr = S_OK;
    Done:

        if(SUCCEEDED(hr)) {
            ResumeThread(pNode->h);
        }
        return hr;
}


HRESULT TerminateTCPListenThread(BYTE LANA) {
    CCritSection csLock(&TCP_TRANSPORT::g_csLockTCPTransportGlobals);
    ce::list<LISTEN_NODE, TCP_LISTEN_NODE_ALLOC >::iterator it;
    HANDLE h = NULL;
    HRESULT hr = E_FAIL;

    csLock.Lock();
        for(it=g_SocketListenList.begin(); it!=g_SocketListenList.end(); ++it) {
            if(it->LANA == LANA) {
                h = it->h;
                closesocket(it->s);
            }
        }
    csLock.UnLock();

    if(h) {
        if(WAIT_FAILED == WaitForSingleObject(h, INFINITE)) {
            TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV: error waiting for listen thread to die!!")));
            hr = E_UNEXPECTED;
            goto Done;
        }
        CloseHandle(h);
    }

    hr = S_OK;
    Done:
        return hr;
}

HRESULT  HaltTCPIncomingConnections(void)
{
    HRESULT hr = S_OK;
    TRACEMSG(ZONE_INIT, (L"Halting Incoming TCP connections"));

    CCritSection csLock(&TCP_TRANSPORT::g_csLockTCPTransportGlobals);
    ce::list<CONNECTION_HOLDER *, TCP_CONNECTION_HLDR_ALLOC>::iterator itConn;

    if(TRUE == g_fStopped) {
        ASSERT(0 == g_SocketListenList.size());
        return S_OK;
    }

    //
    // Render all threads worthless
    csLock.Lock();
        g_fStopped = TRUE;
        while(g_SocketListenList.size()) {
            TRACEMSG(ZONE_TCPIP, (L"SMBSRV-LSTNTHREAD: Closing listen socket: 0x%x", g_SocketListenList.front().s));
            csLock.UnLock();
            hr = TerminateTCPListenThread(g_SocketListenList.front().LANA);
            ASSERT(SUCCEEDED(hr));
            csLock.Lock();
        }



        //
        // Three phases
        //
        // 1.  kill all sockets and inc ref cnt so items dont go away from under us
        //     NOTE: this *PREVENTS* DecrementConnectionCounter from deleting their
        //     memory (AND entering the critical section)... so it also prevents
        //     deadlock -- ie, the list length is fixed
        // 2.  wait for all threads to stop (not under CS)
        // 3.  dec all ref cnts (under CS) -- done in StopTCPTransport (after the cracker
        //     has stopped)

        //PHASE1
        ASSERT(TRUE == csLock.IsLocked());
        for(itConn = g_ConnectionList.begin(); itConn != g_ConnectionList.end(); ++itConn) {
            CONNECTION_HOLDER *pToHalt = (*itConn);
            ASSERT(NULL != pToHalt);

            if(pToHalt) {
                //
                // get a refcnt to each connection in the list
                // we use this to gurantee our list CANT shrink.
                IncrementConnectionCounter(pToHalt);
                closesocket(pToHalt->sock);
                pToHalt->sock = INVALID_SOCKET;
            }
        }

        //PHASE2
        for(itConn = g_ConnectionList.begin(); itConn != g_ConnectionList.end(); ++itConn) {
            CONNECTION_HOLDER *pToHalt = (*itConn);

            if(NULL == pToHalt || WAIT_FAILED == WaitForSingleObject(pToHalt->hHandle, INFINITE)) {
                TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV: MAJOR error waiting for connection thread to exit!!")));
                ASSERT(FALSE);
                hr = E_UNEXPECTED;
                goto Done;
            }
        }
    csLock.UnLock();

    Done:
        g_fStopped = FALSE;  //mark us as not stopped... because all sockets (listen and recving)
                            //  have stopped we do this so STopTCPTransport can finish cleaning up
        ASSERT(0 == g_SocketListenList.size());
        return hr;
}

//TODO: WSAStartup called on init, but not on deconstruction
HRESULT StopTCPTransport(void)
{
    TRACEMSG(ZONE_INIT, (TEXT("SMBSRV:Stopping TCP transport")));
    HRESULT hr = S_OK;

    CCritSection csLock(&TCP_TRANSPORT::g_csLockTCPTransportGlobals);

    if(TRUE == g_fStopped) {
        return S_OK;
    } else {
        g_fStopped = TRUE;
    }

    csLock.Lock();
    ASSERT(0 == g_SocketListenList.size());

    //PHASE3
    //now that both the listen and cracker threads are dead, we know that no NEW sockets (or threads)
    //  can be created... so loop through them all
    ASSERT(FALSE == Cracker::g_fIsRunning);
    while(0 != g_ConnectionList.size()) {
        CONNECTION_HOLDER *pTemp = g_ConnectionList.front();
        if(NULL == pTemp) {
            ASSERT(FALSE); //internal ERROR!!! we should NEVER have null!! EVER!
            g_ConnectionList.pop_front();
            TRACEMSG(ZONE_TCPIP, (L"SMBSRV: TCP CONNECTIONLIST: %d", g_ConnectionList.size()));
            continue;
        }

        //
        // Do all cleanup for the holder here
        IFDBG(UINT uiPrevLen = g_ConnectionList.size());
        DecrementConnectionCounter(pTemp);
        IFDBG(ASSERT(uiPrevLen - 1 == g_ConnectionList.size()));
    }
    ASSERT(0 == g_ConnectionList.size());
    ASSERT(0 == TCP_TRANSPORT::g_ConnectionID.NumIDSOutstanding());
    csLock.UnLock();
    DeleteCriticalSection(&TCP_TRANSPORT::g_csLockTCPTransportGlobals);

    return S_OK;
}




DWORD 
TCP_TRANSPORT::SMBSRV_TCPListenThread(LPVOID _myNode)
{
    HRESULT hr = E_FAIL;
    SOCKADDR_IN my_addr;
    SOCKET s = INVALID_SOCKET;
    LISTEN_NODE *pMyNode = (LISTEN_NODE*)_myNode;

    UINT uiIPAddr = pMyNode->uiIPAddress;

    CCritSection csLock(&TCP_TRANSPORT::g_csLockTCPTransportGlobals);
    csLock.Lock();

    if(TRUE == g_fStopped) {
        hr = S_OK;
        goto Done;
    }

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-LSTNTHREAD: creating socket FAILED")));
        hr = E_UNEXPECTED;
        goto Done;
    }
    TRACEMSG(ZONE_TCPIP, (L"SMBSRV-LSTNTHREAD: Binding listen socket: 0x%x", s));

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(g_usTCPListenPort);
    my_addr.sin_addr.s_addr = uiIPAddr;

    if (SOCKET_ERROR == bind(s, (struct sockaddr *)&my_addr, sizeof(struct sockaddr))){
        TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-LSTNTHREAD: cant bind socket!!")));
        hr = E_UNEXPECTED;
        goto Done;
    }

    if (-1 == listen(s, 10)) {
        TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-LSTNTHREAD: cant listen!!")));
        hr = E_UNEXPECTED;
        goto Done;
    }

    //
    // Update the master list with our socket
    pMyNode->s = s;

    //
    // Now that we've added ourself to the global list, release our lock
    csLock.UnLock();

    //
    // Loop as long as we are not stopped
    while (FALSE == g_fStopped)
    {
        SOCKADDR_IN their_addr;
        int sin_size = sizeof(struct sockaddr_in);
        SOCKET newSock = accept(s, (struct sockaddr *)&their_addr, &sin_size);


        if(INVALID_SOCKET == newSock) {
            TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-LSTNTHREAD: got an invalid socket on accept!!!")));
            goto Done;
        } else {
            /*BOOL fDisable = TRUE;
            if (ERROR_SUCCESS != setsockopt(newSock,IPPROTO_TCP,TCP_NODELAY,(const char*)&fDisable,sizeof(BOOL))) {
                TRACEMSG(ZONE_ERROR,(L"HTTPD: setsockopt(0x%08x,TCP_NODELAY) failed",newSock));
                ASSERT(FALSE);
            }*/

            CONNECTION_HOLDER *pNewConn = new CONNECTION_HOLDER();

            TRACEMSG(ZONE_TCPIP, (L"SMBSRV-LSTNTHREAD: got new TCP connection!"));

            if(NULL == pNewConn) {
                TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-LSTNTHREAD: OOM -- cant make CONNECTION_HOLDER!!!")));
                closesocket(newSock);
                continue;
            }

            TRACEMSG(ZONE_TCPIP, (L"SMBSRV: Added TCP Connection -- Total Connections: %d", InterlockedIncrement(&TCP_TRANSPORT::g_lAliveSockets)));
            InitializeCriticalSection(&pNewConn->csSockLock);
            pNewConn->refCnt = 1; //NOTE: there is only one ref and 2 copies
                                  //  this is because ProcessingThread will call
                                  // DecrementConnectionCounter which removes
                                  // from the list
            pNewConn->sock = newSock;
            pNewConn->hHandle = CreateThread(NULL, 0, SMBSRV_TCPProcessingThread, (VOID *)pNewConn, CREATE_SUSPENDED, NULL);
            pNewConn->hOverlappedHandle = CreateEvent(NULL, TRUE, FALSE, NULL);

            if(NULL != pNewConn->hHandle) {
                EnterCriticalSection(&g_csLockTCPTransportGlobals);
                    if(!g_ConnectionList.push_front(pNewConn)) {
                        hr = E_OUTOFMEMORY;
                    }
                    TRACEMSG(ZONE_TCPIP, (L"SMBSRV: TCP CONNECTIONLIST-push: %d", g_ConnectionList.size()));
                LeaveCriticalSection(&g_csLockTCPTransportGlobals);
                ResumeThread(pNewConn->hHandle);
            }
        }
    }

    Done:
        TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-LSTNTHREAD:Exiting thread!")));

        //
        // Remove ourself from the listen list
        csLock.Lock();
        IFDBG(BOOL fFound = FALSE;);
        ce::list<LISTEN_NODE, TCP_LISTEN_NODE_ALLOC >::iterator itConn;
        for(itConn = g_SocketListenList.begin(); itConn!=g_SocketListenList.end();++itConn) {
            if(itConn->s == s) {
                closesocket(s);
                g_SocketListenList.erase(itConn);
                IFDBG(fFound = TRUE;);
                break;
            }
        }
        IFDBG(ASSERT(fFound));

        if(FAILED(hr))
            return -1;
        else
            return 0;
}


int timed_recv(SOCKET s,char *buf,int len,int flags, UINT seconds, BOOL *pfTimedOut)
{
    int retValue = 0;
    int recvd = 0;

    if(INVALID_SOCKET == s) {
        return SOCKET_ERROR;
    }

    *pfTimedOut = FALSE;

    while(SOCKET_ERROR != retValue && len)
    {
        //
        // If we need to block for only a certain period of time do so
        if(0xFFFFFFFF != seconds) {
            FD_SET fd;
            FD_ZERO (&fd);
            FD_SET (s, &fd);
            timeval tv;
            tv.tv_sec = seconds;
            tv.tv_usec = 0;
            if (1 != select (0, &fd, NULL, NULL, &tv)) {
                TRACEMSG(ZONE_TCPIP, (L"SMB_SRV: TCP transport -- timed_recv failed on select() with %d", WSAGetLastError()));
                *pfTimedOut = TRUE;
                retValue = SOCKET_ERROR;
                goto Done;
            }
            //
            // If there is something available, fetch it
            if(FALSE == FD_ISSET(s, &fd)) {
                continue;
            }

        }

        int iJustRead = recv(s, (char *)buf, len, flags);
        TRACEMSG(ZONE_TCPIP, (L"SMB_SRV: TCP transport -- just recv'ed %d bytes", iJustRead));
        if(SOCKET_ERROR == iJustRead) {
            TRACEMSG(ZONE_TCPIP, (L"SMB_SRV: TCP transport -- timed_recv failed on recv() with %d", WSAGetLastError()));
            retValue = iJustRead;
            goto Done;
        }

        //
        // If our socket has been closed, return with error
        if(0 == iJustRead) {
            TRACEMSG(ZONE_TCPIP, (L"SMB_SRV: TCP transport -- timed_recv failed on recv() with 0 bytes %d -- sock prob closed", WSAGetLastError()));
            retValue = SOCKET_ERROR;
            goto Done;
        }
        ASSERT(iJustRead <= len);

        len -= iJustRead;
        recvd += iJustRead;
        buf += iJustRead;
    }

    retValue = recvd;
    Done:
        return retValue;
}

HRESULT writeAll(SOCKET s, char *buffer, int len)
{
    char *pPacketTemp = (char *)buffer;
    HRESULT hRet = S_OK;

    if(SOCKET_ERROR == s) {
        return E_INVALIDARG;
    }

    while(len)
    {
        int retCode = send(s, pPacketTemp, len, 0);

        if(retCode == 0 || retCode == SOCKET_ERROR) {
            TRACEMSG(ZONE_ERROR, (L"SMB_SRV: TCP transport -- Error(0x%08x)--send failed", WSAGetLastError()));
            hRet = E_FAIL;
            break;
        }

        pPacketTemp += retCode;
        len -= retCode;
    }
    return hRet;
}


HRESULT writeAll_BlockedOverLapped(SOCKET s, HANDLE h, WSABUF *pBufs, UINT uiNumBufs)
{
    HRESULT hRet = E_FAIL;
    DWORD dwSent = 0;
    WSAOVERLAPPED overlapped;
    DWORD dwLeft = 0;

    if(SOCKET_ERROR == s) {
        return E_INVALIDARG;
    }

    for(UINT i=0; i<uiNumBufs; i++) {
        dwLeft += pBufs[i].len;
    }

    while(dwLeft) {
        overlapped.hEvent = h;
        int retCode = WSASend(s, pBufs, uiNumBufs, &dwSent, 0, &overlapped, NULL);

        if(0 == retCode) {
            TRACEMSG(ZONE_TCPIP, (L"SMB_SRV: TCP transport -- no error on send, data went w/o overlapped call needed"));
        } else if(WSA_IO_PENDING == WSAGetLastError()) {
            DWORD dwFlags;
            TRACEMSG(ZONE_TCPIP, (L"SMB_SRV: TCP transport -- needed to block on overlapped WSASend()"));
            if(WAIT_OBJECT_0 != WaitForSingleObject(h, INFINITE)) {
                TRACEMSG(ZONE_TCPIP, (L"SMB_SRV: TCP transport -- error waiting on return event for WSASend()"));
                ASSERT(FALSE);
                goto Done;
            }

            //
            // See how much data we sent
            if(TRUE != WSAGetOverlappedResult(s, &overlapped, &dwSent, TRUE, &dwFlags)) {
                TRACEMSG(ZONE_ERROR, (L"SMB_SRV: TCP transport -- couldnt get overlapped results for WSASend() = GLE: %d", WSAGetLastError()));
                goto Done;
            }
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMB_SRV: TCP transport -- error code %d from WSASend()", WSAGetLastError()));
            goto Done;
        }

        if(dwSent > dwLeft) {
            TRACEMSG(ZONE_ERROR, (L"SMB_SRV: TCP transport -- more data sent that requested?! WSASend()"));
            ASSERT(FALSE);
            goto Done;
        }

        //
        // Adjust buffers to represent what was sent
        dwLeft -= dwSent;
        while(uiNumBufs && dwSent >= pBufs[0].len) {
            dwSent -= pBufs[0].len;
            pBufs++;
            uiNumBufs --;
        }

        if(uiNumBufs && dwSent) {
            ASSERT(dwSent < pBufs[0].len);
            pBufs[0].len -= dwSent;
            pBufs[0].buf += dwSent;
        }
    }

   hRet = S_OK;
   Done:
        return hRet;
}


VOID IncrementConnectionCounter(CONNECTION_HOLDER *pMyConnection)
{
    InterlockedIncrement(&pMyConnection->refCnt);
}

VOID DecrementConnectionCounter(CONNECTION_HOLDER *pMyConnection)
{
    BOOL fFound = FALSE;

    if(0 != InterlockedDecrement(&pMyConnection->refCnt)) {
        return;
    }

    CCritSection csLock(&TCP_TRANSPORT::g_csLockTCPTransportGlobals);
    csLock.Lock();
        ce::list<CONNECTION_HOLDER *, TCP_CONNECTION_HLDR_ALLOC>::iterator it;
        ce::list<CONNECTION_HOLDER *, TCP_CONNECTION_HLDR_ALLOC>::iterator itEnd = g_ConnectionList.end();

        for(it = g_ConnectionList.begin(); it != itEnd; ++it) {
            if(pMyConnection == *it) {
                g_ConnectionList.erase(it++);
                TRACEMSG(ZONE_TCPIP, (L"SMBSRV: TCP CONNECTIONLIST - DecCntr: %d", g_ConnectionList.size()));
                fFound = TRUE;
                break;
            }
       }
    csLock.UnLock();

    //
    //if we dont find it, thats bad -- leak memory and quit!!
    if(!fFound) {
        ASSERT(FALSE);
        return;
    }

    ASSERT(FALSE == csLock.IsLocked());
    ASSERT(0 == pMyConnection->refCnt);

    DeleteCriticalSection(&pMyConnection->csSockLock);

    if(INVALID_SOCKET != pMyConnection->sock) {
        closesocket(pMyConnection->sock);
    }

    CloseHandle(pMyConnection->hHandle);
    CloseHandle(pMyConnection->hOverlappedHandle);
    pMyConnection->hHandle = INVALID_HANDLE_VALUE;
    pMyConnection->hOverlappedHandle = INVALID_HANDLE_VALUE;
    delete pMyConnection;
}


HRESULT
TCP_TRANSPORT::DeleteTCPTransportToken(VOID *pToken)
{
    CONNECTION_HOLDER *pNew = (CONNECTION_HOLDER *)pToken;
    DecrementConnectionCounter(pNew);
    return S_OK;
}


HRESULT
TCP_TRANSPORT::CopyTCPTransportToken(VOID *pToken, VOID **pNewToken)
{
    CONNECTION_HOLDER *pNew = (CONNECTION_HOLDER *)pToken;
    IncrementConnectionCounter(pNew);
    *pNewToken = (VOID *)pNew;
    return S_OK;
}


//PERFPERF: we could have one thread for all tcp instead of one per connection
//   there isnt any real reason not to do that (just batch them up in a big array and block
//   on all of them)
//
//   As for the refcount (IncrementConnectionCounter) -- this thread proc
//      owns one by default (from pre CreateThread call).  then we inc this
//      everytime we send a connection to the cracker.  its dec'ed in our
//      own memory deconstruction
DWORD
TCP_TRANSPORT::SMBSRV_TCPProcessingThread(LPVOID _pAdapter)
{
    CONNECTION_HOLDER *pMyConnection = (CONNECTION_HOLDER *)_pAdapter;
    SOCKET sSock = pMyConnection->sock;
    HRESULT hr = E_FAIL;
    USHORT  sConnectionID;
    ULONG   ConnectionID;
    SMB_PACKET *pNewPacket = NULL;

    //
    // Build up a unique connection ID
    if(FAILED(g_ConnectionID.GetID(&sConnectionID))) {
        sConnectionID = 0xFFFF;
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }
    ConnectionID = (ULONG)(SMB_Globals::TCP_TRANSPORT << 16) | sConnectionID;
    pMyConnection->ulConnectionID = ConnectionID;

    while(FALSE == g_fStopped)
    {
        DWORD dwHeader;
        CHAR *pHeader = (CHAR *)&dwHeader;
        BOOL fTimedOut;

        //
        // Get some memory out of our free pool
        ASSERT(NULL == pNewPacket);
        if(NULL == (pNewPacket = SMB_Globals::g_SMB_Pool.Alloc())) {
            hr = E_OUTOFMEMORY;
            goto Done;
        }

#ifdef DEBUG
        pNewPacket->lpszTransport = L"TCPIP";
#endif

        //
        // Block for the packet header, on timeout inform the packet cracker
        if(4 != timed_recv(sSock, pHeader, 4, 0, g_uiTCPTimeoutInSeconds, &fTimedOut)) {
            if(fTimedOut) {
                TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCP -- reading SMB TCP header timed out!! telling the cracker")));
                pNewPacket->uiPacketType = SMB_CLIENT_IDLE_TIMEOUT;
                pNewPacket->pInSMB = 0;
                pNewPacket->uiInSize = 0;
                goto PacketReadyToSend;
            } else {
                TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCP -- reading SMB TCP failed!! closing connection")));
                hr = E_FAIL;
                goto Done;
            }
        }

        //
        // Special case keep alives -- there is no reason to wake up the Cracker
        //   for these.  (keep alive is opeation 0x85)
        if(pHeader[0] == 0x85) {
            TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-TCPRECV: its a keepalive... dont do anything")));
            continue;
        } else if (pHeader[0] != 0x00) { //who knows what this is... close up shop
            TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCPRECV: got a unexpected packet header (%d)"), pHeader[0]));
            hr = E_FAIL;
            goto Done;
        }

        //
        // Prepare to read the real packet
        pHeader[0] = 0;
        dwHeader = ntohl(dwHeader);

        if(dwHeader > sizeof(pNewPacket->InSMB)) {
             TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCPRECV: this packet is too large (%d) for our buffers!!  -- drop connection"), dwHeader));
             hr = E_OUTOFMEMORY;
             goto Done;
        }

        //
        // Read one SMB packet (wait up to 'g_uiTCPTimeoutInSeconds' seconds for this)
        if(dwHeader != (DWORD)timed_recv(sSock, (char *)(pNewPacket->InSMB), dwHeader, 0, g_uiTCPTimeoutInSeconds, &fTimedOut)) {
            TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCP -- reading SMB TCP packet of %d bytes timed out!! -- closing connection"), dwHeader));
            hr = E_FAIL;
            goto Done;
        }

        //
        // Set packet fields that are specific to 'normal' packets only
        IFDBG(pNewPacket->PerfStartTimer());
        pNewPacket->uiPacketType = SMB_NORMAL_PACKET;
        pNewPacket->pInSMB = (SMB_HEADER *)pNewPacket->InSMB;
        pNewPacket->uiInSize = dwHeader;



        PacketReadyToSend:
            //
            // Give the cracker a reference (will be dec'ed by QueueTCPPacketForSend)
            IncrementConnectionCounter(pMyConnection);

            pNewPacket->pToken = (void *)pMyConnection;
            pNewPacket->pOutSMB = NULL;
            pNewPacket->uiOutSize = 0;
            pNewPacket->pfnQueueFunction = QueueTCPPacketForSend;
            pNewPacket->pfnCopyTranportToken = CopyTCPTransportToken;
            pNewPacket->pfnDeleteTransportToken = DeleteTCPTransportToken;
            //pNewPacket->pfnGetSocketName = TCP_GetSocketName;
            pNewPacket->ulConnectionID = ConnectionID;
            pNewPacket->dwDelayBeforeSending = 0;
#ifdef DEBUG
            pNewPacket->uiPacketNumber = InterlockedIncrement(&SMB_Globals::g_PacketID);
#endif


            TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-TCPRECV:Got a TCP packet! (%d bytes--id %d)"), dwHeader, pNewPacket->uiPacketNumber));

            //hand off the packet to the SMB cracker
            if(FAILED(CrackPacket(pNewPacket))) {
                //this should *NEVER* happen... Crack should handle its own errors
                //  and when there is one it should return back an error code to the client
                TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-TCPRECV: UNEXPECTED ERROR IN CRACK()!")));
                ASSERT(FALSE);
                hr = E_UNEXPECTED;
                goto Done;
            } else {
                pNewPacket = NULL;
            }
    }

    hr = S_OK;
    Done:

        if(pNewPacket) {
            SMB_Globals::g_SMB_Pool.Free(pNewPacket);
            pNewPacket = NULL;
        }

        if(FAILED(hr)) {
            closesocket(pMyConnection->sock);
            pMyConnection->sock = INVALID_SOCKET;
        }

        //
        // Push a packet into the cracker that kills off anything we might
        //   have opened
        if(TRUE == Cracker::g_fIsRunning) {
            //
            // Give the cracker a reference (will be dec'ed by QueueTCPPacketForSend)
            IncrementConnectionCounter(pMyConnection);

            //
            // Get some memory out of our free pool
            ASSERT(NULL == pNewPacket);
            if(NULL == (pNewPacket = SMB_Globals::g_SMB_Pool.Alloc())) {
                hr = E_OUTOFMEMORY;
                goto Done;
            }

            if(NULL != pNewPacket) {
                memset(pNewPacket, 0, sizeof(SMB_PACKET));
                pNewPacket->uiPacketType = SMB_CONNECTION_DROPPED;
                pNewPacket->ulConnectionID = ConnectionID;
                pNewPacket->pToken = (void *)pMyConnection;
                pNewPacket->pfnQueueFunction = QueueTCPPacketForSend;
                pNewPacket->pfnCopyTranportToken = CopyTCPTransportToken;
                pNewPacket->pfnDeleteTransportToken = DeleteTCPTransportToken;
                //pNewPacket->pfnGetSocketName = TCP_GetSocketName;
                pNewPacket->dwDelayBeforeSending = 0;

        #ifdef DEBUG
                pNewPacket->uiPacketNumber = InterlockedIncrement(&SMB_Globals::g_PacketID);
        #endif

                TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-TCPRECV: sending out connection dropped for %d"), ConnectionID));

                //
                // Hand off the packet to the SMB cracker
                if(FAILED(CrackPacket(pNewPacket))) {
                    //this should *NEVER* happen... Crack should handle its own errors
                    //  and when there is one it should return back an error code to the client
                    TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-NBRECV: UNEXPECTED ERROR IN CRACK()!")));
                    ASSERT(FALSE);
                    hr = E_UNEXPECTED;
                    goto Exit;
                } else {
                    pNewPacket = NULL;
                }
            }
        } else {
            TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-TCPRECV: cracker thread is dead -- just exiting")));
            ASSERT(FALSE);
        }

    Exit:

        if(pNewPacket) {
            SMB_Globals::g_SMB_Pool.Free(pNewPacket);
            pNewPacket = NULL;
        }

        //
        // Dec our threads ref to the connection
        DecrementConnectionCounter(pMyConnection);

        //
        // Give back our id
        if(0xFFFF != sConnectionID) {
           if(FAILED(g_ConnectionID.RemoveID(sConnectionID))) {
                ASSERT(FALSE);
           }
        }

        TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-TCPRECV:Exiting thread!")));
        TRACEMSG(ZONE_TCPIP, (L"SMBSRV: Removed TCP Connection -- Total Connections: %d", InterlockedDecrement(&TCP_TRANSPORT::g_lAliveSockets)));
        return 0;
}


HRESULT TCP_TerminateSession(ULONG ulConnectionID)
{
    CCritSection csLock(&TCP_TRANSPORT::g_csLockTCPTransportGlobals);
    ce::list<CONNECTION_HOLDER *, TCP_CONNECTION_HLDR_ALLOC>::iterator itConn;
    HRESULT hr = E_FAIL;

    csLock.Lock();
    for(itConn = g_ConnectionList.begin(); itConn != g_ConnectionList.end(); ++itConn) {
        CONNECTION_HOLDER *pConn = (*itConn);
        CCritSection csConnLock(&pConn->csSockLock);

        csConnLock.Lock();
        if(ulConnectionID == pConn->ulConnectionID) {
            TRACEMSG(ZONE_TCPIP, (L"SMBSRV: TCPTRANS -- terminating connection %d", ulConnectionID));
            closesocket(pConn->sock);
            hr = S_OK;
            goto Done;
        }
    }

    Done:
        return hr;
}


HRESULT TCP_GetSocketName(SMB_PACKET *pPacket, struct sockaddr *pSockAddr, int *pNameLen)
{
    CONNECTION_HOLDER *pMyConnection = NULL;
    HRESULT hr = E_FAIL;

    //
    // Make sure they (the cracker) have given us memory!
    if(NULL == pPacket || NULL == pPacket->pToken) {
        ASSERT(FALSE); //internal error -- this should NEVER happen
        return E_UNEXPECTED;
    }
    pMyConnection = (CONNECTION_HOLDER *)pPacket->pToken;

    //
    // If the connection has dropped delete memory
    if(SMB_CONNECTION_DROPPED == pPacket->uiPacketType) {
        TRACEMSG(ZONE_TCPIP, (L"TCPIP: connection dropped packet -- deleting memory"));
        hr = S_OK;
        goto Done;
    }

    if(0 != getsockname(pMyConnection->sock, pSockAddr, pNameLen)) {
        ASSERT(FALSE);
        hr = E_FAIL;
    }

    Done:
        return hr;
}



//
//  This function is the transport specific callback for transmitting
//   packets -- in the event an unrecoverable error occurs, we will
//   just shut down the socket -- this is okay because it will
//   cause all other transmissions to that socket to fail (there
//   could be more in the cracker queue waiting to be sent) -- eventually
//   the refcnt for the connection will reach 0 and will be terminated
HRESULT
TCP_TRANSPORT::QueueTCPPacketForSend(SMB_PACKET *pPacket, BOOL fDestruct)
{
    HRESULT hr = S_OK;
    DWORD dwSendSize;
    //char *pDWPtr = NULL;
    CONNECTION_HOLDER *pMyConnection = NULL;

    //
    // Make sure they (the cracker) have given us memory!
    if(NULL == pPacket || NULL == pPacket->pToken) {
        ASSERT(FALSE); //internal error -- this should NEVER happen
        return E_UNEXPECTED;
    }
    pMyConnection = (CONNECTION_HOLDER *)pPacket->pToken;

    EnterCriticalSection(&pMyConnection->csSockLock); //serialize writes!!
    TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-TCP -- sending response for packet: %d on socket %d"), pPacket->uiPacketNumber, pMyConnection->sock));


    //
    // If the packet isnt normal just return (dont send data)
    if(SMB_NORMAL_PACKET != pPacket->uiPacketType) {
        TRACEMSG(ZONE_TCPIP, (L"TCPIP: connection dropped/timed out packet -- deleting memory"));
        hr = S_OK;
        goto Done;
    }


    ASSERT(pPacket->uiOutSize <= SMB_Globals::MAX_PACKET_SIZE);

    dwSendSize = htonl(pPacket->uiOutSize);
    //pDWPtr = (CHAR *)&dwSendSize;
    //pDWPtr[0] = 0;

    if(TRUE == g_fStopped) {
        TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCP -- we have shut down.. dont send the packet")));
        hr = E_FAIL;
        goto Done;
    }

    WSABUF myBufs[2];
    myBufs[0].len = sizeof(DWORD);
    myBufs[0].buf = (CHAR *)&dwSendSize;
    myBufs[1].len = pPacket->uiOutSize;
    myBufs[1].buf = (CHAR *)pPacket->pOutSMB;

    if(FAILED(writeAll_BlockedOverLapped(pMyConnection->sock, pMyConnection->hOverlappedHandle, myBufs, 2))) {
        TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCP -- sending SMB header  over TCP failed!! closing connection")));
        closesocket(pMyConnection->sock);
        pMyConnection->sock = INVALID_SOCKET;
        hr = E_FAIL;
        goto Done;
    }

    /*if(FAILED(writeAll(pMyConnection->sock, pDWPtr, 4))) {
        TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCP -- sending SMB header  over TCP failed!! closing connection")));
        closesocket(pMyConnection->sock);
        pMyConnection->sock = INVALID_SOCKET;
        hr = E_FAIL;
        goto Done;
    }
    if(FAILED(writeAll(pMyConnection->sock, (char *)pPacket->pOutSMB, pPacket->uiOutSize))) {
        TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCP -- sending SMB over TCP failed!! closing connection")));
        closesocket(pMyConnection->sock);
        pMyConnection->sock = INVALID_SOCKET;
        hr = E_FAIL;
        goto Done;
    }*/

    Done:

#ifdef DEBUG
        if(SMB_NORMAL_PACKET == pPacket->uiPacketType && pPacket->pInSMB) {
            UCHAR SMB = pPacket->pInSMB->Command;
            pPacket->PerfStopTimer(SMB);
            DWORD dwProcessing = pPacket->TimeProcessing();
            CCritSection csLock(&Cracker::g_csPerfLock);
            csLock.Lock();
                DOUBLE dwNew = (Cracker::g_dblPerfAvePacketTime[SMB] * Cracker::g_dwPerfPacketsProcessed[SMB]);
                Cracker::g_dwPerfPacketsProcessed[SMB] ++;
                dwNew += dwProcessing;
                dwNew /= Cracker::g_dwPerfPacketsProcessed[SMB];
                Cracker::g_dblPerfAvePacketTime[SMB] = dwNew;
            csLock.UnLock();
        }
#endif

        if(TRUE == fDestruct) {
            SMB_Globals::g_SMB_Pool.Free(pPacket);
        }
        LeaveCriticalSection(&pMyConnection->csSockLock);

        if(TRUE == fDestruct) {
            DecrementConnectionCounter(pMyConnection);
        }
        return hr;
}
