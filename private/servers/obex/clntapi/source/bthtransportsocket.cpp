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
#include "BTHTransportSocket.h"
#include "obextransportconnection.h"

#include "ObexStrings.h"
#include "ObexBTHTransport.h"

#include <bt_api.h> // For BTSecurityLevel

//globals
extern CRITICAL_SECTION g_TransportSocketCS;
static BTH_CONNECTION_HOLDER *g_pConnectionList = 0;
 
CBTHTransportSocket::CBTHTransportSocket():
    _refCount(1),
    _btAddr(0),
    _dwPortNum(0)
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CBTHTransportSocket::CBTHTransportSocket()\n"));  

    //initilize the socket libs
    WSADATA wsd;
    WSAStartup (MAKEWORD(2,2), &wsd); 
}

CBTHTransportSocket::~CBTHTransportSocket()
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CBTHTransportSocket::~CBTHTransportSocket()\n"));
    Close();  
    
    //clean up winsock
    WSACleanup();
 }

HRESULT STDMETHODCALLTYPE
CBTHTransportSocket::Close()
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CBTHTransportSocket::Close()\n")); 
    
    CCritSection TransportCS(&g_TransportSocketCS);
    TransportCS.Lock();
    
    HRESULT hRes = E_FAIL;

    
    BTH_CONNECTION_HOLDER *pTemp = 0;
    BTH_CONNECTION_HOLDER *pPrev = 0;
   
    if(g_pConnectionList)
    {
        pTemp = g_pConnectionList->pNext;
        pPrev = g_pConnectionList;
    }
    else
    {       
        return S_OK;
    }

    if (pPrev)
    {
        //its okay that we dont free up the memory pointed to here!
        // actually we CANT because we dont own a reference to it
        // to prevent a loop of refcounts
        if(pPrev->btAddr == _btAddr && pPrev->dwPortNum == _dwPortNum)
        {
            BTH_CONNECTION_HOLDER *pDel = pPrev;  
            g_pConnectionList = pPrev->pNext;
            delete pDel;            
            pTemp = 0; 
            hRes = S_OK;
        }
    }

    while(pTemp)
    {      
        //its okay that we dont free up the memory pointed to here!
        // actually we CANT because we dont own a reference to it
        // to prevent a loop of refcounts
        if(pTemp->btAddr == _btAddr && pTemp->dwPortNum == _dwPortNum)
        {
            BTH_CONNECTION_HOLDER *pDel = pTemp;
            pPrev->pNext = pTemp->pNext;            
            
            delete pDel;
            hRes = S_OK;
            break;
        }

        pPrev = pTemp;
        pTemp = pTemp->pNext;      
    }
   
    return hRes;   
}

HRESULT STDMETHODCALLTYPE
CBTHTransportSocket::Listen(LPTRANSPORTCONNECTION *ppConnection)
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CBTHTransportSocket::Listen()\n")); 
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
CBTHTransportSocket::Connect(LPPROPERTYBAG2 pDeviceProps,DWORD dwCapability, LPTRANSPORTCONNECTION * ppConnection)
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CBTHTransportSocket::Connect()\n"));
    PREFAST_ASSERT(pDeviceProps);
    *ppConnection = NULL;
   
    VARIANT DeviceVar;
    VARIANT NameVar;
    VARIANT Channel;
    HRESULT hr = E_FAIL;
    SOCKET sMySock = INVALID_SOCKET;
    BTH_CONNECTION_HOLDER *pTemp = NULL;

    VariantInit(&DeviceVar);
    VariantInit(&NameVar);
    VariantInit(&Channel);

    CCritSection TransportCS(&g_TransportSocketCS);

    if( FAILED(pDeviceProps->Read(c_szDevicePropAddress, &DeviceVar, 0)) ||        
        FAILED(pDeviceProps->Read(c_szPort, &Channel, 0)) 
        )
    {
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - BT] ERROR - not enough params to connect\n")); 
        hr = E_FAIL;
        goto Done;
    }

    if(FAILED(pDeviceProps->Read(c_szDevicePropName, &NameVar, 0)))
    {
        NameVar.vt = VT_BSTR;
        NameVar.bstrVal = SysAllocString(L"UNKNOWN_BT_NAME");    
    }

    //make sure the params are of the right type
    if(!(VT_I4   == V_VT(&Channel) &&
          VT_BSTR == V_VT(&NameVar) &&
          VT_BSTR == V_VT(&DeviceVar)))
    {
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - BT] a parameter is of the wrong type!\n"));
        hr = E_FAIL;
        goto Done;
    }

    TransportCS.Lock();
    
    //figure out if we already have this guy allocated...
    pTemp = g_pConnectionList;

    //if we DO have it allocated, this loop will return an instance
    while(pTemp)
    {
        BT_ADDR btAddr = {0};
        
        if(CObexBTHTransport::GetBA(DeviceVar.bstrVal, &btAddr) &&
            pTemp->btAddr == btAddr && pTemp->dwPortNum == (DWORD) Channel.lVal)
        {
            //get a copy of the id for our records
            _btAddr = pTemp->btAddr;
            _dwPortNum = pTemp->dwPortNum;

            DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - BT] ----->  reusing connection!\n"));
            
            (*ppConnection) = pTemp->pItem;  
            (*ppConnection)->AddRef();  
            
            hr = S_OK;
            goto Done;
        }
        pTemp = pTemp->pNext;   
    }
 
    //since we DONT have the instance, make one create a socket
    sMySock = socket(AF_BT, SOCK_STREAM, BTHPROTO_RFCOMM);
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX-BT] -->Created Socket\n"));
    if(INVALID_SOCKET == sMySock) {
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - BT] ERROR - invalid socket\n"));  
        SetLastError (ERROR_DEVICE_NOT_CONNECTED); 
        hr = E_FAIL;
        goto Done;
    }

    //information on the BTH to connect to
    SOCKADDR_BTH    DestSockAddr; 
    memset(&DestSockAddr, 0, sizeof(SOCKADDR_BTH));
    
    //move in the appropriate values (telling the service name and address family)
    DestSockAddr.addressFamily = AF_BT;

    // move in the device id for the selected device 
    if(!CObexBTHTransport::GetBA(DeviceVar.bstrVal, &DestSockAddr.btAddr))
    {
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX-BT] Error getting BT_ADDR from BSTR!\n"));
        hr = E_FAIL;
        goto Done;
    }
            
    DestSockAddr.port = Channel.lVal;    

    // Security level 1 requires no authentication for pre-SSP connections (that is, no PIN entry required).
    // For SSP, Security Level 1 uses the "Just Works" procedure.
    ULONG SecurityLevel = BTSecurityLevel_1;
    if (SOCKET_ERROR == setsockopt(sMySock, SOL_RFCOMM, SO_BTH_SET_SECURITY_LEVEL, (char *)&SecurityLevel, sizeof(SecurityLevel)))
    {
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX-BT] Error authenticating: 0x%x\n", WSAGetLastError()));
        hr = E_FAIL;
        goto Done;
    }

    //connect to that device   
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX-BT] Connecting to: %s:%d\n", DeviceVar.bstrVal, Channel.lVal));
   
    if (SOCKET_ERROR == connect(sMySock, (const struct sockaddr *) &DestSockAddr,
        sizeof(SOCKADDR_BTH))) {    
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX-BT] Error connecting: 0x%x\n", WSAGetLastError()));
        hr = E_FAIL;
        goto Done;
    }

    //if we are here, we dont have a connecction, so make one
    *ppConnection = new CObexTransportConnection(this, NameVar.bstrVal, sMySock);  
    if( !(*ppConnection) )
    {
        hr = E_OUTOFMEMORY;
        goto Done;
    } 
    
    //create and insert a new connection holder
    pTemp = new BTH_CONNECTION_HOLDER();
    if( !pTemp )
    {
        delete *ppConnection;
        *ppConnection = NULL;
        hr = E_OUTOFMEMORY;
        goto Done;
    } 
    
    _btAddr = DestSockAddr.btAddr;
    _dwPortNum = DestSockAddr.port;
   
    pTemp->pItem = *ppConnection;   
    pTemp->btAddr = _btAddr;
    pTemp->dwPortNum = _dwPortNum;

    pTemp->pNext = g_pConnectionList;
    g_pConnectionList = pTemp;

    
    hr = S_OK;
    
Done:
    if(FAILED(hr) && INVALID_SOCKET != sMySock) {
        closesocket(sMySock);        
    }

    VariantClear(&DeviceVar);
    VariantClear(&NameVar);
    VariantClear(&Channel);
    return hr;
}

HRESULT STDMETHODCALLTYPE 
CBTHTransportSocket::GetProperties(LPPROPERTYBAG2 * ppListenProps)
{
    return E_NOTIMPL;
}

ULONG STDMETHODCALLTYPE 
CBTHTransportSocket::AddRef() 
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CBTHTransportSocket::AddRef()\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE 
CBTHTransportSocket::Release() 
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CBTHTransportSocket::Release()\n"));
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);    
    if(!ret) 
        delete this; 
    return ret;
}

HRESULT STDMETHODCALLTYPE 
CBTHTransportSocket::QueryInterface(REFIID riid, void** ppv) 
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CBTHTransportSocket::QueryInterface()\n"));
    if(!ppv) 
        return E_POINTER;
    if(riid == IID_IPropertyBag) 
        *ppv = static_cast<IObexTransportSocket*>(this);    
    else 
        return *ppv = 0, E_NOINTERFACE;
    
    return AddRef(), S_OK;    
}




























