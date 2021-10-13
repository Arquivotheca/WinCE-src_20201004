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
#include <windows.h>
#include <tchar.h>
#include "netmain.h"
#include "ndp_lib.h"
#include <strsafe.h>
#include "utils.h"

#define TIMEOUT_SECONDS             10
#define DEFAULT_PORT                8080
#define DEFAULT_ITERATIONS          1000
#define DEFAULT_SERVER              TEXT("wcenet-ipvx")

extern "C" LPTSTR gszMainProgName  = _T("octhrdstres2_vx");
extern "C" DWORD  gdwMainInitFlags = INIT_NET_CONNECTION;

DWORD g_logVerbosity = LOG_PASS;
DWORD WINAPI ThrdProc(LPVOID pIn);

//----------------------------------------------------------------------
class CTHREAD
{
public:
    HANDLE m_hThHandle; 

    virtual DWORD WINAPI m_ThreadProc(LPVOID pIn)=0;
    DWORD m_dwThreadId;
    BOOL m_fExit;
    CTHREAD() ;
    ~CTHREAD();
    void StartThread() ;

};

//----------------------------------------------------------------------
CTHREAD::CTHREAD()
{
    m_fExit = FALSE;
    m_hThHandle = NULL;
    m_dwThreadId = 0;

    StartThread();
}

//----------------------------------------------------------------------
void CTHREAD::StartThread()
{
    m_fExit = FALSE;
    m_hThHandle = CreateThread( NULL,                    // lpThreadAttributes : must be NULL
                                0,                        // dwStackSize : ignored
                                ThrdProc,                // lpStartAddress : pointer to function
                                (LPVOID)this,            // lpParameter : pointer passed to thread
                                0,                        // dwCreationFlags 
                                &m_dwThreadId             // lpThreadId 
                                );
    if (!m_hThHandle) 
    {
        m_dwThreadId = 0;
        QAError(TEXT("CTHREAD::CTHREAD Failed to create thread. Error code:[0x%x]"),GetLastError());
    }

    QAMessage(TEXT("CTHREAD::CTHREAD created with thread ID: [0x%x]"), m_dwThreadId);
}

//----------------------------------------------------------------------
CTHREAD::~CTHREAD()
{
    m_fExit = TRUE;
    INT iRet = 0;
    DWORD dwWait=WaitForSingleObject(m_hThHandle, TIMEOUT_SECONDS*1000);
    if (dwWait == WAIT_OBJECT_0)
    {
        GetExitCodeThread(m_hThHandle, PDWORD(&iRet));
        QAMessage(TEXT("CTHREAD::~CTHREAD thread ID [0x%x] exited with exit value: [%d]"),m_dwThreadId,iRet);
    }
    else
    {
        if(dwWait==WAIT_TIMEOUT)
        {
            QAError(TEXT("Wait for thread exit timed out"));
        }
        TerminateThread(m_hThHandle,iRet);
        QAMessage(TEXT("CTHREAD::~CTHREAD thread ID [0x%x] terminated...."),m_dwThreadId);
    }

   CloseHandle(m_hThHandle);
   
   m_fExit = FALSE;
   m_hThHandle = NULL;
   m_dwThreadId = 0;
}

//----------------------------------------------------------------------
class CSend : public CTHREAD
{
public:
    CSend(SOCKET sock, INT iSocketType, SOCKADDR_IN saRemoteAddr):
        m_Sock(sock), 
        m_iSocketType(iSocketType),
        m_saRemoteAddr(saRemoteAddr) {};

    ~CSend() {};

private:
    SOCKET m_Sock;
    int m_iSocketType;
    SOCKADDR_IN m_saRemoteAddr;
    DWORD m_dwSentBytes;
    DWORD WINAPI m_ThreadProc(LPVOID pIn);
};

//----------------------------------------------------------------------
DWORD CSend::m_ThreadProc(LPVOID pIn)
{
    BYTE pBuf[10];    
    DWORD dwLen=sizeof(pBuf);
    m_dwSentBytes = 0;
    INT iBytesSent=0;

    QAMessage(TEXT("CSend::m_ThreadProc thread ID [0x%x] Started"),m_dwThreadId);
    while (!m_fExit)
    {
        //Send data.
        iBytesSent = sendto(m_Sock, (char *) pBuf, dwLen, 0, (SOCKADDR *)&m_saRemoteAddr, sizeof(m_saRemoteAddr));
        
        if(iBytesSent==SOCKET_ERROR)
        {
            DWORD dwError = WSAGetLastError() ;
            QAMessage(TEXT("CSend::m_ThreadProc ID [0x%x] sendto() failed with error %d"), m_dwThreadId, dwError);
            if( dwError == WSAENOTSOCK)
                return (DWORD)-1;
        }
        else
        {
            m_dwSentBytes+=iBytesSent;
        }

    }
    QAMessage(TEXT("CSend::m_ThreadProc thread id [0x%x] Sent %d Byte"),m_dwThreadId, m_dwSentBytes);
    return 0;
}

//----------------------------------------------------------------------
DWORD WINAPI ThrdProc(LPVOID pIn)
{
    CTHREAD * pThread = (CTHREAD *)pIn;
    return pThread->m_ThreadProc(NULL);    
}

//----------------------------------------------------------------------
void RunTest(int iThreadsNo, int iSocketNo, SOCKADDR_IN RemoteAddr)
{
    INT i = 0, j = 0 ;

    //------------------------------------------------------------
    // Create Sockets Array  initialize to Invalid_Socket
    SOCKET* pSock = new SOCKET[iSocketNo] ;
    
    for( i=0; i<iSocketNo; i++ )
        pSock[i] = INVALID_SOCKET;

    //------------------------------------------------------------
    // Create Send Threads - we use here non empty constructor, so actual creation is later
    CSend** ppSendThread = new CSend* [iThreadsNo*iSocketNo];

    for( i=0; i<iThreadsNo*iSocketNo; i++ )
        ppSendThread[i] = NULL ;
    
    //------------------------------------------------------------
    // Create a Threads for each socket - repeat for desired thread count
    for( i=0; i<iSocketNo; i++ )
    {
        pSock[i] = socket(AF_INET, SOCK_DGRAM, 0);

        if(pSock[i] == INVALID_SOCKET)
        {
            QAError(TEXT("Socket(%d) failed with error %d"), i, WSAGetLastError());
            goto cleanup ;
        }
        
        for( j=0; j<iThreadsNo; j++ )
        {
            ppSendThread[j+i*iThreadsNo] = new CSend( pSock[i], SOCK_DGRAM, RemoteAddr ) ;
            if( ppSendThread[j+i*iThreadsNo] == NULL )
            {
                QAError(TEXT("Thread(%d) failed with error %d"), j+i*iThreadsNo, GetLastError());
                goto cleanup ;
            }
            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            QAMessage( TEXT("Spawning Thread No. [%d], With Socket No. [%d]"), j+i*iThreadsNo+1, i+1 ) ;
            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        }
    }


    // Give the threads some time to send data
    Sleep(5000);

//------------------------------------------------------------
cleanup:
//------------------------------------------------------------

    for( i=0; i<iThreadsNo*iSocketNo; i++ )
    {   
        if(ppSendThread[i] != NULL)
        {
            delete( ppSendThread[i] ) ;
            ppSendThread[i] = NULL;
        }
    }
    
    delete []ppSendThread ;
    // Close the sockets suddenly on the threads
    for( i=0; i<iSocketNo; i++ )
    {
        if(pSock[i] != INVALID_SOCKET)
        {
            closesocket( pSock[i] ) ;
            pSock[i] = INVALID_SOCKET;
        }
    }
    
    delete []pSock ;
      
}

//----------------------------------------------------------------------
BOOL CreateRemoteAddressStructure( CONST TCHAR* ptszServer, SOCKADDR_IN* pRemoteAddr )
{
    DWORD dwIPAddr = INADDR_ANY;
    char szServerASCII[256];
    size_t retVal = 0;

#if defined UNICODE
    wcstombs_s(&retVal, szServerASCII, 256, ptszServer, sizeof(szServerASCII));
#else
    strncpy(szServerASCII, ptszServer, sizeof(szServerASCII));
#endif

    // perform name resolution
    dwIPAddr = inet_addr( szServerASCII ) ;
    if( dwIPAddr == INADDR_NONE )
    {
        // remote server is not a dotted decimal IP address
        hostent    *h=NULL;
        h = gethostbyname(szServerASCII);
        if(h != NULL)
        {
            memcpy( &dwIPAddr, h->h_addr_list[0], sizeof(dwIPAddr) );
        }
        else
        {
            QAError(TEXT("Invalid address parameter = %s"), ptszServer);
            return FALSE ;
        }
    }

    // Here we obtained Remote Server IP - set RemoteAdrr struct
    pRemoteAddr->sin_family = AF_INET;
    pRemoteAddr->sin_addr.s_addr = dwIPAddr;
    pRemoteAddr->sin_port = htons( DEFAULT_PORT ) ;

    return TRUE ;
}


//----------------------------------------------------------------------
VOID Usage()
{
    QAMessage( TEXT("octhreads_vx Usage:") ) ;
    QAMessage( TEXT("") ) ;
    QAMessage( TEXT("[-Server] Remote Server (Name/IP) to send data to")) ;
    QAMessage( TEXT("[-Threads] No of threads sharing the same socket to send the data [Default = 1]") ) ;
    QAMessage( TEXT("[-Sockets] No of Sockets to send on data [Default = 1]") ) ;
    QAMessage( TEXT("[-Iterates] Total times to run the app (in Seconds) [Default = 1000]") ) ;
    QAMessage( TEXT("") ) ;
    QAMessage( TEXT("Example: s octhreads_vx -Server ranm01 -Threads 5 -Sockets 10 -Iterates 10") ) ;
}

int mymain (int argc, TCHAR *argv[])
{
    //-------------------------------------
    // Start WSA
    WSADATA     WSAData;
    WORD        WSAVerReq = MAKEWORD(2,2);

    if(WSAStartup(WSAVerReq, &WSAData) != 0)
    {
        QAError(TEXT("WSAStartup Failed. Exiting"));
        return -1;
    }

    //-------------------------------------
    BOOL bUsage = GetOptionFlag( &argc, argv, TEXT("?") ) ;
    if( bUsage )
    {
        Usage() ;
        return 0 ;
    }

    //-------------------------------------
    // Init Threads count and Sockets count
    LONG lThreadsNo = GetOptionLong( &argc, argv, 1, TEXT("Threads") ) ;
    LONG lSocketsNo = GetOptionLong( &argc, argv, 1, TEXT("Sockets") ) ;
    DWORD dwIterates = (DWORD)GetOptionLong( &argc, argv, DEFAULT_ITERATIONS, TEXT("Iterates") ) ;
    
    QAMessage( TEXT("Starting app with [%d] Threads and [%d] Sockets"), lThreadsNo*lSocketsNo, lSocketsNo ) ;

    //-------------------------------------
    // Remote Server
    TCHAR* ptszServer = GetOptionString( &argc, argv, TEXT("Server") ) ;
    if( ptszServer == NULL )
    {
        ptszServer = DEFAULT_SERVER;
    }

    SOCKADDR_IN saRemoteAddr = {0} ;
    if( !CreateRemoteAddressStructure(ptszServer, &saRemoteAddr) )
        return -1 ;

    //-------------------------------------
    // Run Test

    for(DWORD dwLoops=0;dwLoops<dwIterates;dwLoops++)
    {
        RunTest(lThreadsNo, lSocketsNo, saRemoteAddr);
    }
    //-------------------------------------
    WSACleanup();

    return 0;
}



