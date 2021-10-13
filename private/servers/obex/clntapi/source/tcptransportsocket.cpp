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
#include "common.h"
#include "TCPTransportSocket.h"
#include "ObexTransportConnection.h"
#include "ObexTCPTransport.h"

#include "ObexStrings.h"


#include <Ws2tcpip.h>

/******globals******/    
    extern CRITICAL_SECTION g_TransportSocketCS;
    static TCP_CONNECTION_HOLDER *g_pConnectionList = 0;


 
CTCPTransportSocket::CTCPTransportSocket():
    _refCount(1)   
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CTCPTransportSocket::CTCPTransportSocket()\n"));  
    VariantInit(&m_varName);
    VariantInit(&m_varPort);

    V_BSTR(&m_varName) = NULL;
    V_BSTR(&m_varPort) = NULL;
     
    //initilize the socket libs
    WSADATA wsd;
    WSAStartup (MAKEWORD(1,1), &wsd); 
}

CTCPTransportSocket::~CTCPTransportSocket()
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CTCPTransportSocket::~CTCPTransportSocket()\n"));  
    Close();   
    VariantClear(&m_varName);
    VariantClear(&m_varPort);  

    //clean up winsock
    WSACleanup();
 }

HRESULT STDMETHODCALLTYPE
CTCPTransportSocket::Close()
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CTCPTransportSocket::Close()\n")); 
    CCritSection TransportCS(&g_TransportSocketCS); 
    TransportCS.Lock();
    
    HRESULT hRes = E_FAIL;
    
    TCP_CONNECTION_HOLDER *pTemp = 0;
    TCP_CONNECTION_HOLDER *pPrev = 0;
   
    if(g_pConnectionList)
    {
        pTemp = g_pConnectionList->m_pNext;
        pPrev = g_pConnectionList;
    }
    else
    {       
        hRes = S_OK;
        goto Done;
    }

    if(!V_BSTR(&this->m_varPort) || !V_BSTR(&this->m_varName))
    {
        hRes = S_OK;
        goto Done;
    }
    
    if(pPrev)
    {
        //its okay that we dont free up the memory pointed to here!
        // actually we CANT because we dont own a reference to it
        // to prevent a loop of refcounts
        if(0 == wcscmp(V_BSTR(&(this->m_varPort)), V_BSTR(&(pPrev->m_varPort))) &&
           0 == wcscmp(V_BSTR(&(this->m_varName)), V_BSTR(&(pPrev->m_varName))))
        {
            TCP_CONNECTION_HOLDER *pDel = pPrev;  
            g_pConnectionList = pPrev->m_pNext;
            VariantClear(&(pDel->m_varPort));
            VariantClear(&(pDel->m_varName));
            delete pDel;  
            pTemp = 0; 
            hRes = S_OK;
            goto Done;
        }
    }

    while(pTemp)
    {      
        //its okay that we dont free up the memory pointed to here!
        // actually we CANT because we dont own a reference to it
        // to prevent a loop of refcounts
        if(0 == wcscmp(V_BSTR(&(this->m_varPort)), V_BSTR(&(pTemp->m_varPort))) &&
           0 == wcscmp(V_BSTR(&(this->m_varName)), V_BSTR(&(pTemp->m_varName))))
        {
            TCP_CONNECTION_HOLDER *pDel = pTemp;
            pPrev->m_pNext = pTemp->m_pNext; 
           
            VariantClear(&(pDel->m_varPort));
            VariantClear(&(pDel->m_varName));
            
            delete pDel;
            hRes = S_OK;
            break;
        }

        pPrev = pTemp;
        pTemp = pTemp->m_pNext;      
    }
  
Done:       
    return hRes;   
}

HRESULT STDMETHODCALLTYPE
CTCPTransportSocket::Listen(LPTRANSPORTCONNECTION *ppConnection)
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CTCPTransportSocket::Listen()\n")); 
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE
CTCPTransportSocket::Connect(LPPROPERTYBAG2 pDeviceProps,DWORD dwCapability, LPTRANSPORTCONNECTION * ppConnection)
{
#if defined (SDK_BUILD)
    return E_NOTIMPL;
#else
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CTCPTransportSocket::Connect()\n"));
    
    CCritSection TransportCS(&g_TransportSocketCS);
    TransportCS.Lock();
    
    PREFAST_ASSERT(pDeviceProps);
    *ppConnection = NULL;

    //set the default error to return
    HRESULT hRet = E_FAIL;

    //variables 
    SOCKET sMySock = INVALID_SOCKET;
    TCP_CONNECTION_HOLDER *pTemp = NULL;

    ADDRINFO    aiHints, *paiRemote=NULL, *paiTmp;

    char *pRemote = NULL;
    char *pPort = NULL;
    
    //fetch the device ID from the property bag
    VARIANT pNameVar;
    VARIANT pPortVar;
    VariantInit(&pNameVar);
    VariantInit(&pPortVar);

      //get the device name from the property bag (on error, exit)
    if(FAILED(pDeviceProps->Read(c_szDevicePropName, &pNameVar, 0)) || VT_BSTR != V_VT(&pNameVar))
    {
        hRet = E_FAIL;
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX] ----- could not read device name\n"));
        goto connect_error;
    }
   
    //read the port BSTR
    if(FAILED(pDeviceProps->Read(c_szPort, &pPortVar, 0)))
    {
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX] ----- could not read port name\n"));
        goto connect_error;
    }

    //see if the port is BSTR / NAME
    if(VT_BSTR != V_VT(&pNameVar) || 
       VT_BSTR != V_VT(&pPortVar))
    {
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX] ----- name or var is of incorrect type\n"));
        ASSERT(FALSE);
        goto connect_error;
    }
    
    //figure out if we already have this guy allocated...
    pTemp = g_pConnectionList;

    //if we DO have it allocated, this loop will return an instance
    while(pTemp)
    {
        if(0 == wcscmp(V_BSTR(&pPortVar), V_BSTR(&(pTemp->m_varPort))) &&
           0 == wcscmp(V_BSTR(&pNameVar), V_BSTR(&(pTemp->m_varName))))
        {
            DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - TCP] ----->  reusing connection!\n"));
            
            (*ppConnection) = pTemp->m_pItem;  
            (*ppConnection)->AddRef();            

            hRet = S_OK;
            goto connect_done;
        }
        pTemp = pTemp->m_pNext;   
    }
 

    //  
    //  Since we are here, there isnt an existing instance to the server -- go make one
    //
    

    //  call getaddrinfo to get the IP's (V6 or V4 depending on their setup)
    memset(&aiHints,0,sizeof(aiHints));

    aiHints.ai_family   = PF_UNSPEC;   // Accept v4 or v6
    aiHints.ai_socktype = SOCK_STREAM; // or SOCK_DGRAM for UDP

    int uiRemoteLen, uiPortLen;

    uiRemoteLen = WideCharToMultiByte(CP_ACP, 
                                      0, 
                                      V_BSTR(&pNameVar),
                                      -1,
                                      NULL,
                                      0,
                                      NULL,
                                      NULL);
    
    uiPortLen = WideCharToMultiByte(CP_ACP, 
                                      0, 
                                      V_BSTR(&pPortVar),
                                      -1,
                                      NULL,
                                      0,
                                      NULL,
                                      NULL);
    if(!uiRemoteLen || !uiPortLen)
    {
         DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - TCP] ----->  port or address is too short!\n"));
         ASSERT(FALSE);
         hRet = E_FAIL;
         goto connect_error;;
    }    
    
    pRemote = new char[uiRemoteLen + 1];
    pPort = new char[uiPortLen + 1];

    if(!pRemote || !pPort)
    {
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - TCP] ----->  error out of memory!\n"));

        hRet = E_FAIL;
        goto connect_error;
    }

    if(uiRemoteLen != WideCharToMultiByte(CP_ACP, 
                                      0, 
                                      V_BSTR(&pNameVar),
                                      -1,
                                      pRemote,
                                      uiRemoteLen+1,
                                      NULL,
                                      NULL))
    {
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - TCP] ----->  error doing multibyte conversion!\n"));

        hRet = E_FAIL;
        goto connect_error;
    }
    
    if(uiPortLen != WideCharToMultiByte(CP_ACP, 
                                      0, 
                                      V_BSTR(&pPortVar),
                                      -1,
                                      pPort,
                                      uiPortLen+1,
                                      NULL,
                                      NULL))
    {
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - TCP] ----->  error doing multibyte conversion!\n"));
        hRet = E_FAIL;
        goto connect_error;
    }

    if (getaddrinfo(pRemote,pPort,&aiHints,&paiRemote) != 0) 
    {
         DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - TCP] ----->  error getting addrinfo()!\n"));
         goto connect_error;
    }

    // Try each address returned until we connect
    for (paiTmp = paiRemote; paiTmp != NULL; paiTmp = paiTmp->ai_next) 
    {
         DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - TCP] ----->  creating socket()!\n"));         
         sMySock = socket( paiTmp->ai_family, paiTmp->ai_socktype, paiTmp->ai_protocol);

         if (sMySock == INVALID_SOCKET) 
         {            
               DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - TCP] ----->  invalid socket()!\n"));               
               continue;
         }

         DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - TCP] ----->  atempting connect()!\n")); 
         
         if (SOCKET_ERROR == connect(sMySock, paiTmp->ai_addr, paiTmp->ai_addrlen)) 
         {
               DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - TCP] ----->  connect failed (%d)!\n", WSAGetLastError()));              
               closesocket(sMySock);  
               sMySock = INVALID_SOCKET;
               continue;
         }
         
         break;
    }

    if (INVALID_SOCKET == sMySock) 
    {
        SetLastError (ERROR_DEVICE_NOT_CONNECTED);        
        hRet = E_FAIL;
        goto connect_error;
    }

    
    

    //get a copy of the name/port for our records
    if(FAILED(hRet = VariantCopy(&(this->m_varName), &pNameVar)))
    {  
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - TCP] ----->  error copying variant()!\n")); 
        goto connect_error;
    }
        
    if(FAILED(hRet = VariantCopy(&(this->m_varPort), &pPortVar)))
    {
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - TCP] ----->  error copying variant()!\n")); 
        goto connect_error;
    }
    
    //if we are here, we dont have a transport connecction, so make one
    *ppConnection = new CObexTransportConnection(this, V_BSTR(&pNameVar), sMySock);  
    if( !(*ppConnection) )
    {
        hRet = E_OUTOFMEMORY;        
        goto connect_error;
    }
        
    //create and insert a new connection holder
    pTemp = new TCP_CONNECTION_HOLDER();    
    if( !pTemp )
    {
        hRet = E_OUTOFMEMORY;        
        goto connect_error;
    }

    pTemp->m_pItem = *ppConnection;

    VariantInit(&(pTemp->m_varName));
    VariantInit(&(pTemp->m_varPort));
    
    //copy variants into 
    if(FAILED(hRet = VariantCopy(&(pTemp->m_varName), &pNameVar)))
        goto connect_error;
   
    if(FAILED(hRet = VariantCopy(&(pTemp->m_varPort), &pPortVar)))
        goto connect_error;
 
    
    //chain in the node
    pTemp->m_pNext = g_pConnectionList;
    g_pConnectionList = pTemp;
    
    hRet = S_OK;
    goto connect_done;
    
connect_error:
     ASSERT(FAILED(hRet));
     
     if(*ppConnection)       
            (*ppConnection)->Release();
     if(pTemp)
            delete pTemp;
            
     *ppConnection = NULL;

connect_done:
    VariantClear(&pNameVar);
    VariantClear(&pPortVar);

    if(paiRemote)
        freeaddrinfo(paiRemote);

    if(pRemote)
        delete [] pRemote;
    if(pPort)
        delete [] pPort;
        
    return hRet; 
#endif
}

HRESULT STDMETHODCALLTYPE 
CTCPTransportSocket::GetProperties(LPPROPERTYBAG2 * ppListenProps)
{
    return E_NOTIMPL;
}

ULONG STDMETHODCALLTYPE 
CTCPTransportSocket::AddRef() 
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CTCPTransportSocket::AddRef()\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE 
CTCPTransportSocket::Release() 
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CTCPTransportSocket::Release()\n"));
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);    
    if(!ret) 
        delete this; 
    return ret;
}

HRESULT STDMETHODCALLTYPE 
CTCPTransportSocket::QueryInterface(REFIID riid, void** ppv) 
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CTCPTransportSocket::QueryInterface()\n"));
    if(!ppv) 
        return E_POINTER;
    if(riid == IID_IPropertyBag) 
        *ppv = static_cast<IObexTransportSocket*>(this);    
    else 
        return *ppv = 0, E_NOINTERFACE;
    
    return AddRef(), S_OK;    
}




























