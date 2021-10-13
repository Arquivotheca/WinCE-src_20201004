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
#include "ObexTCPTransport.h"
#include "TCPTransportSocket.h"
#include "CObex.h"
#include "PropertyBag.h"
#include "PropertyBagEnum.h"

#include "ObexStrings.h"

/*----------globals---------------*/
LPSOCKET CObexTCPTransport::pSocket = 0;

CObexTCPTransport::CObexTCPTransport() : _refCount(1), dwTimeOfLastEnum(0)
{
    DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexTCPTransport::CObexTCPTransport()\n"));

    //initilize the socket libs
    WSADATA wsd;
    WSAStartup (MAKEWORD(1,1), &wsd);   
}

CObexTCPTransport::~CObexTCPTransport()
{
    DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexTCPTransport::~CObexTCPTransport()\n"));
    
    //clean up winsock
    WSACleanup();
}

HRESULT STDMETHODCALLTYPE 
CObexTCPTransport::Init(void)
{
    DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexTCPTransport::Init()\n"));   
    return S_OK;
}


HRESULT STDMETHODCALLTYPE 
CObexTCPTransport::Shutdown(void)
{
        return S_OK;
}

HRESULT STDMETHODCALLTYPE
//singleton that holds a socket object 
CObexTCPTransport::CreateSocket(LPPROPERTYBAG2 pPropertyBag,
                                 LPSOCKET  *ppSocket)
{
    DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexTCPTransport::CreateSocket()\n"));
   
    *ppSocket = new CTCPTransportSocket(); 
    if( !(*ppSocket) )
	return E_OUTOFMEMORY;
	
    return S_OK;
}


HRESULT STDMETHODCALLTYPE 
CObexTCPTransport::CreateSocketBlob(unsigned long ulSize,
                                     byte __RPC_FAR *pbData,
                                     LPSOCKET __RPC_FAR *ppSocket)
{
        return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE 
CObexTCPTransport::EnumDevices(LPPROPERTYBAGENUM *ppDevices)
{
   	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE 
CObexTCPTransport::UpdateDeviceProperties(LPPROPERTYBAG2 __RPC_FAR pDevice, 
                                          IPropertyBagEnum **_ppNewBagEnum, 
                                          BOOL fGetJustEnoughToConnect, 
                                          UINT *uiUpdateStatus)
{ 
    *uiUpdateStatus = 0xFFFFFFFF;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE 
CObexTCPTransport::EnumProperties(LPPROPERTYBAG2 __RPC_FAR *ppProps)
{
     return E_NOTIMPL;
}

ULONG STDMETHODCALLTYPE 
CObexTCPTransport::AddRef() 
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObexTCPTransport::AddRef()\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE 
CObexTCPTransport::Release() 
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObexTCPTransport::Release()\n"));
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);    
    if(!ret) 
        delete this; 
    return ret;
}

HRESULT STDMETHODCALLTYPE 
CObexTCPTransport::QueryInterface(REFIID riid, void** ppv) 
{
    DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexTCPTransport::QueryInterface()\n"));
       if(!ppv) 
        return E_POINTER;
    else if(riid == IID_IUnknown) 
        *ppv = this;
    else if(riid == IID_IObexTransport) 
        *ppv = static_cast<IObexTransport*>(this);    
    else 
        return *ppv = 0, E_NOINTERFACE;

    return AddRef(), S_OK;    
}



