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
#ifndef OBEX_IRDA_TRANSPORT_H 
#define OBEX_IRDA_TRANSPORT_H


//the number of attempts to find a server
//  that will be made
extern UINT g_uiIRDAMaxRetries;

//max # devices to list
#define DEVICE_LIST_LEN   10


#include "PropertyBagEnum.h"

class CObexIRDATransport : public IObexTransport, public IObexAbortTransportEnumeration
{
public:
    CObexIRDATransport();
    ~CObexIRDATransport();

    // IObexTransport
    HRESULT STDMETHODCALLTYPE Init( void);      
    HRESULT STDMETHODCALLTYPE Shutdown( void);
    
    HRESULT STDMETHODCALLTYPE CreateSocket( 
        LPPROPERTYBAG2 pPropertyBag,
        LPSOCKET __RPC_FAR *ppSocket);
    
    HRESULT STDMETHODCALLTYPE CreateSocketBlob( 
        unsigned long ulSize,
        byte __RPC_FAR *pbData,
        LPSOCKET __RPC_FAR *ppSocket);
    
    HRESULT STDMETHODCALLTYPE EnumDevices( 
        LPPROPERTYBAGENUM __RPC_FAR *ppDevices);
    
    HRESULT STDMETHODCALLTYPE EnumProperties( 
        LPPROPERTYBAG2 __RPC_FAR *ppProps);
    
    HRESULT STDMETHODCALLTYPE UpdateDeviceProperties(
        LPPROPERTYBAG2 __RPC_FAR pDevice, 
        IPropertyBagEnum **ppNewlyDiscoveredDeviceBag,
        BOOL fGetJustEnoughToConnect, 
        UINT *uiUpdateStatus);


    // IObexAbortTransportEnumeration  
    HRESULT STDMETHODCALLTYPE Abort();
    HRESULT STDMETHODCALLTYPE Resume();
    HRESULT STDMETHODCALLTYPE IsAborting(BOOL *fIsAborting);
    
       
    // IUnknown
    ULONG STDMETHODCALLTYPE AddRef();        
    ULONG STDMETHODCALLTYPE Release();        
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);   

private:
     BOOL fIsAborting;
     ULONG _refCount;
     static LPSOCKET pSocket; 
     DWORD dwTimeOfLastEnum;
};


#endif
