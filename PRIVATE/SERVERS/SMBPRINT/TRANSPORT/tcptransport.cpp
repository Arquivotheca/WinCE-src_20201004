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
#include "FixedAlloc.h"


using namespace TCP_TRANSPORT;

ce::list<CONNECTION_HOLDER *, TCP_CONNECTION_HLDR_ALLOC > TCP_TRANSPORT::g_ConnectionList;
pool_allocator<10, CONNECTION_HOLDER>  SMB_Globals::g_ConnectionHolderAllocator;
CRITICAL_SECTION              TCP_TRANSPORT::g_csConnectionList;
SOCKET                        TCP_TRANSPORT::g_ListenSocket = INVALID_SOCKET;
HANDLE                        TCP_TRANSPORT::g_ListenThreadHandle;
BOOL                          TCP_TRANSPORT::g_fShutDown = FALSE;
const USHORT                  TCP_TRANSPORT::g_usTCPListenPort = 445;
const UINT                    TCP_TRANSPORT::g_uiTCPTimeoutInSeconds = 15;
BOOL                          TCP_TRANSPORT::g_fIsRunning = FALSE;
UniqueID                      TCP_TRANSPORT::g_ConnectionID;

IFDBG(LONG                    TCP_TRANSPORT::g_lAliveSockets = 0); 
 
//
// Forward declares
VOID DecrementConnectionCounter(CONNECTION_HOLDER *pMyConnection);
VOID IncrementConnectionCounter(CONNECTION_HOLDER *pMyConnection);
 
//
//  Start the netbios transport -- this includes 
//     initing any global variables before threads get spun 
//     etc
HRESULT StartTCPTransport() 
{
    TRACEMSG(ZONE_INIT, (TEXT("SMBSRV:Starting TCP transport")));
    HRESULT hr = E_FAIL;
    
    // 
    // Initialize globals  
    ASSERT(INVALID_SOCKET == g_ListenSocket);
    g_fShutDown = FALSE;
    g_ListenSocket = INVALID_SOCKET;
    
    InitializeCriticalSection(&TCP_TRANSPORT::g_csConnectionList);
    
    WORD wVersionRequested = MAKEWORD( 2, 2 );        
    WSADATA wsaData;
    
    if (0 != WSAStartup( wVersionRequested, &wsaData )) {
        TRACEMSG(ZONE_INIT, (TEXT("SMBSRV: error with WSAStartup: %d"), WSAGetLastError()));        
        hr = E_UNEXPECTED;
        goto Done;
    } 
    
    //
    // Spin threads to handle listening   
    if(NULL == (g_ListenThreadHandle = CreateThread(NULL, 0, SMBSRV_TCPListenThread, NULL, 0, NULL))) {
        TRACEMSG(ZONE_INIT, (L"SMBSRV: CreateThread failed starting TCP Listen:%d", GetLastError()));
        goto Done;
    }

    g_fIsRunning = TRUE;
    
    hr = S_OK;
    Done:     
        if(FAILED(hr))
            DeleteCriticalSection(&TCP_TRANSPORT::g_csConnectionList);
        return hr;
}


HRESULT  HaltTCPIncomingConnections(void)
{
    HRESULT hr = S_OK;
    TRACEMSG(ZONE_INIT, (L"Halting Incoming TCP connections"));

    CCritSection csLock(&TCP_TRANSPORT::g_csConnectionList);
    ce::list<CONNECTION_HOLDER *, TCP_CONNECTION_HLDR_ALLOC>::iterator it;
    
    if(FALSE == g_fIsRunning) {
        ASSERT(INVALID_SOCKET == g_ListenSocket);
        return S_OK;
    }
    
    //
    // Render threads worthless
    TRACEMSG(ZONE_TCPIP, (L"SMBSRV-LSTNTHREAD: Closing listen socket: 0x%x", g_ListenSocket));
    g_fShutDown = TRUE;  
    closesocket(g_ListenSocket);
    g_ListenSocket = INVALID_SOCKET;
    
    //
    // Now wait for the ListenThread to end -- at this point the 
    //   g_ConnectionList list can only decrease in size
    if(WAIT_FAILED == WaitForSingleObject(g_ListenThreadHandle, INFINITE)) {
        TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV: error waiting for listen thread to die!!")));
        ASSERT(FALSE);
        return E_UNEXPECTED;
    }
    ASSERT(INVALID_SOCKET == g_ListenSocket);
      
    //
    // Three phases
    //
    // 1.  kill all sockets and inc ref cnt so items dont go away from under us
    //     NOTE: this *PREVENTS* DecrementConnectionCounter from deleting them
    //     memory (AND entering the critical section)... so it also prevents
    //     deadlock -- ie, the list length is fixed
    // 2.  wait for all threads to stop (not under CS)
    // 3.  dec all ref cnts (under CS) -- done in StopTCPTransport (after the cracker
    //     has stopped)
    csLock.Lock();
    //PHASE1
    for(it = g_ConnectionList.begin(); it != g_ConnectionList.end(); ++it) {
        CONNECTION_HOLDER *pToHalt = (*it);
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
    for(it = g_ConnectionList.begin(); it != g_ConnectionList.end(); ++it) {
        CONNECTION_HOLDER *pToHalt = (*it);
        
        if(NULL == pToHalt || WAIT_FAILED == WaitForSingleObject(pToHalt->hHandle, INFINITE)) {
            TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV: MAJOR error waiting for connection thread to exit!!")));
            ASSERT(FALSE);
            hr = E_UNEXPECTED;
            goto Done;
        }       
    }
    csLock.UnLock();
    
    Done:
        ASSERT(INVALID_SOCKET == g_ListenSocket);
        return hr;
}

//TODO: WSAStartup called on init, but not on deconstruction
HRESULT StopTCPTransport(void)
{
    TRACEMSG(ZONE_INIT, (TEXT("SMBSRV:Stopping TCP transport")));    
    HRESULT hr = S_OK;
    ASSERT(INVALID_SOCKET == g_ListenSocket);

    if(FALSE == g_fIsRunning) {
        return S_OK;
    } else {
        g_fIsRunning = FALSE;
    }
    
    //PHASE3
    //now that both the listen and cracker threads are dead, we know that no NEW sockets (or threads)
    //  can be created... so loop through them all 
    ASSERT(FALSE == Cracker::g_fIsRunning);
    CCritSection csLock(&TCP_TRANSPORT::g_csConnectionList);
    csLock.Lock();
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
    csLock.UnLock();
    ASSERT(0 == g_ConnectionList.size()); 
    ASSERT(0 == TCP_TRANSPORT::g_ConnectionID.NumIDSOutstanding());
    DeleteCriticalSection(&TCP_TRANSPORT::g_csConnectionList);
    
    return S_OK;
}




DWORD //TODO: are we supposed to WSAInit() on each thread? if so do it!
TCP_TRANSPORT::SMBSRV_TCPListenThread(LPVOID _pMyAdapter)
{    
    HRESULT hr = E_FAIL; 
    SOCKADDR_IN my_addr;  

    ASSERT(INVALID_SOCKET == g_ListenSocket);    
    if ((g_ListenSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-LSTNTHREAD: creating socket FAILED")));
        hr = E_UNEXPECTED;
        goto Done;
    }
    TRACEMSG(ZONE_TCPIP, (L"SMBSRV-LSTNTHREAD: Binding listen socket: 0x%x", g_ListenSocket));
            
    my_addr.sin_family = AF_INET;       
    my_addr.sin_port = htons(g_usTCPListenPort);     
    my_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (SOCKET_ERROR == bind(g_ListenSocket, (struct sockaddr *)&my_addr, sizeof(struct sockaddr))){
        TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-LSTNTHREAD: cant bind socket!!")));
        hr = E_UNEXPECTED;
        goto Done;
    }
          
    if (-1 == listen(g_ListenSocket, 10)) {
        TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-LSTNTHREAD: cant listen!!")));
        hr = E_UNEXPECTED;
        goto Done;
    }
              
    //
    // Loop as long as we are not shutting down
    while (FALSE == g_fShutDown) 
    {
        SOCKADDR_IN their_addr;
        int sin_size = sizeof(struct sockaddr_in);
        SOCKET newSock = accept(g_ListenSocket, (struct sockaddr *)&their_addr, &sin_size);
        
        
        if(INVALID_SOCKET == newSock) {
            TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-LSTNTHREAD: got an invalid socket on accept!!!")));
        } else {
            BOOL fDisable = TRUE;
            if (ERROR_SUCCESS != setsockopt(newSock,IPPROTO_TCP,TCP_NODELAY,(const char*)&fDisable,sizeof(BOOL))) {
                TRACEMSG(ZONE_ERROR,(L"HTTPD: setsockopt(0x%08x,TCP_NODELAY) failed",newSock));
                ASSERT(FALSE);
            }
        
            //PERFPERF: investigate using a free list!!! (it should be faster)
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
            
            if(NULL != pNewConn->hHandle) {
                EnterCriticalSection(&g_csConnectionList);                    
                    g_ConnectionList.push_front(pNewConn);
                    TRACEMSG(ZONE_TCPIP, (L"SMBSRV: TCP CONNECTIONLIST-push: %d", g_ConnectionList.size()));
                LeaveCriticalSection(&g_csConnectionList);                
                ResumeThread(pNewConn->hHandle);
            }            
        }  
    } 

    Done:           
        TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-LSTNTHREAD:Exiting thread!")));
        if(FAILED(hr))
            return -1;
        else 
            return 0;
}


int timed_recv(SOCKET s,char *buf,int len,int flags, UINT seconds)
{
    int retValue = 0;
    int recvd = 0;

    if(INVALID_SOCKET == s) {
        return SOCKET_ERROR;
    }
    seconds = 0xFFFFFFFF;
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

//PERFPERF: this could be done with interlockinc/dec rather than
//  using the Crit section.  -- I suspect any gain would be minimal
//  however -- check out in profiler
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
    
    CCritSection csLock(&TCP_TRANSPORT::g_csConnectionList);
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
    pMyConnection->hHandle = INVALID_HANDLE_VALUE;
    delete pMyConnection;
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
    USHORT  sUniqueID;
    ULONG   UniqueID;
    //
    // Build up a unique connection ID
    if(FAILED(g_ConnectionID.GetID(&sUniqueID))) {
        sUniqueID = 0xFFFF;
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }
    UniqueID = (ULONG)(SMB_Globals::TCP_TRANSPORT << 16) | sUniqueID;
    
    while(!g_fShutDown)
    {
        DWORD dwHeader;
        CHAR *pHeader = (CHAR *)&dwHeader;
        SMB_PACKET *pNewPacket = NULL;
        
        //
        // Block for the packet header (NOTE: we will block forever)
        if(4 != timed_recv(sSock, pHeader, 4, 0, 0xFFFFFFFF)) {
            TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCP -- reading SMB TCP header timed out!! closing connection")));  
            hr = E_FAIL;      
            goto Done;
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
        
        //prep to read the real packet
        pHeader[0] = 0;
        dwHeader = ntohl(dwHeader);
        
        //
        // Get some memory out of our free pool
        if(NULL == (pNewPacket = SMB_Globals::g_SMB_Pool.Alloc())) {
            hr = E_OUTOFMEMORY;            
            goto Done;
        }  
        
        if(dwHeader > sizeof(pNewPacket->InSMB)) {
             TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCPRECV: this packet is too large (%d) for our buffers!!  -- drop connection"), dwHeader));
             hr = E_OUTOFMEMORY;
             goto Done;        
        }
        
                  
        //
        // Read one SMB packet (wait up to 'g_uiTCPTimeoutInSeconds' seconds for this)
        if(dwHeader != (DWORD)timed_recv(sSock, (char *)(pNewPacket->InSMB), dwHeader, 0, g_uiTCPTimeoutInSeconds)) {
            TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCP -- reading SMB TCP packet of %d bytes timed out!! -- closing connection"), dwHeader));
            hr = E_FAIL;
            goto Done;
        }        
        
        //
        // Give the cracker a reference (will be dec'ed by QueueTCPPacketForSend)
        IncrementConnectionCounter(pMyConnection);
        
        pNewPacket->pInSMB = (SMB_HEADER *)pNewPacket->InSMB;
        pNewPacket->pToken = (void *)pMyConnection;
        pNewPacket->pOutSMB = NULL;
        pNewPacket->uiOutSize = 0;
        pNewPacket->uiInSize = dwHeader;
        pNewPacket->pfnQueueFunction = QueueTCPPacketForSend;
        pNewPacket->ulConnectionID = UniqueID;
        pNewPacket->uiPacketType = SMB_NORMAL_PACKET;
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
        }           
    }
       
    hr = S_OK;    
    Done:        
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
            SMB_PACKET *pNewPacket = NULL;
            
            //
            // Get some memory out of our free pool
            if(NULL == (pNewPacket = SMB_Globals::g_SMB_Pool.Alloc())) {
                hr = E_OUTOFMEMORY;            
                goto Done;
            }     
        
            if(NULL != pNewPacket) {
                memset(pNewPacket, 0, sizeof(SMB_PACKET));                
                pNewPacket->uiPacketType = SMB_CONNECTION_DROPPED;
                pNewPacket->ulConnectionID = UniqueID;
                pNewPacket->pToken = (void *)pMyConnection;
                pNewPacket->pfnQueueFunction = QueueTCPPacketForSend;
                pNewPacket->dwDelayBeforeSending = 0;
                
        #ifdef DEBUG        
                pNewPacket->uiPacketNumber = InterlockedIncrement(&SMB_Globals::g_PacketID);
        #endif        
                
                TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-TCPRECV: sending out connection dropped for %d"), UniqueID));
                
                //
                // Hand off the packet to the SMB cracker
                if(FAILED(CrackPacket(pNewPacket))) {
                    //this should *NEVER* happen... Crack should handle its own errors
                    //  and when there is one it should return back an error code to the client
                    TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-NBRECV: UNEXPECTED ERROR IN CRACK()!")));                               
                    ASSERT(FALSE);
                    hr = E_UNEXPECTED;
                    goto Exit;
                } 
            }
        } else {
            TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-TCPRECV: cracker thread is dead -- just exiting")));
            ASSERT(FALSE);
        }

    Exit:        
        //
        // Dec our threads ref to the connection
        DecrementConnectionCounter(pMyConnection); 
        
        //
        // Give back our id
        if(0xFFFF != sUniqueID) {
           if(FAILED(g_ConnectionID.RemoveID(sUniqueID))) {
                ASSERT(FALSE);
           }
        }   
        
        TRACEMSG(ZONE_TCPIP, (TEXT("SMBSRV-TCPRECV:Exiting thread!")));   
        TRACEMSG(ZONE_TCPIP, (L"SMBSRV: Removed TCP Connection -- Total Connections: %d", InterlockedDecrement(&TCP_TRANSPORT::g_lAliveSockets)));         
        return 0;   
}




// 
//  This function is the transport specific callback for transmitting 
//   packets -- in the event an unrecoverable error occurs, we will
//   just shut down the socket -- this is okay because it will
//   cause all other transmissions to that socket to fail (there
//   could be more in the cracker queue waiting to be sent) -- eventually
//   the refcnt for the connection will reach 0 and will be terminated
HRESULT 
QueueTCPPacketForSend(SMB_PACKET *pPacket, BOOL fDestruct)
{
    HRESULT hr = S_OK;
    DWORD dwSendSize;
    char *pDWPtr = NULL;
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
    //
    //  NOTE!! from here up we have to return and not goto Done (b/c of critsect)
    //
    //


    //
    // If the connection has dropped delete memory
    if(SMB_CONNECTION_DROPPED == pPacket->uiPacketType) {
        TRACEMSG(ZONE_TCPIP, (L"TCPIP: connection dropped packet -- deleting memory"));
        hr = S_OK;
        goto Done;
    }
    

    ASSERT(pPacket->uiOutSize <= SMB_Globals::MAX_PACKET_SIZE); 
  
    dwSendSize = htonl(pPacket->uiOutSize);
    pDWPtr = (CHAR *)&dwSendSize;
    pDWPtr[0] = 0;

    if(g_fShutDown) {
        TRACEMSG(ZONE_ERROR, (TEXT("SMBSRV-TCP -- we have shut down.. dont send the packet")));    
        hr = E_FAIL;
        goto Done;
    }

    if(FAILED(writeAll(pMyConnection->sock, pDWPtr, 4))) {
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
    }

    Done:
        
#ifdef DEBUG   
        ASSERT(TRUE == Cracker::g_fIsRunning);
        if(TRUE == Cracker::g_fIsRunning && SMB_CONNECTION_DROPPED != pPacket->uiPacketType) {
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
