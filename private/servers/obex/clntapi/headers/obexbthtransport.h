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
#ifndef OBEX_BTH_TRANSPORT_H 
#define OBEX_BTH_TRANSPORT_H

#define BTH_INQUIRY_LEN		5
#define BTH_INQUIRY_SIZE	256

struct NAME_CACHE_NODE
{
	BT_ADDR ba; 
	VARIANT var;
	NAME_CACHE_NODE *pNext;
};

class CObexBTHTransport : public IObexTransport, public IObexAbortTransportEnumeration
{
public:
    CObexBTHTransport();
    ~CObexBTHTransport();

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
    
    ULONG STDMETHODCALLTYPE AddRef();        
    ULONG STDMETHODCALLTYPE Release();        
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);   


    // IObexAbortTransportEnumeration  
    HRESULT STDMETHODCALLTYPE Abort();
    HRESULT STDMETHODCALLTYPE Resume();
    HRESULT STDMETHODCALLTYPE IsAborting(BOOL *fIsAborting);

	static int GetBA(WCHAR *pp, BT_ADDR *pba);
	
private:
     ULONG _refCount;
     BOOL fIsAborting;
     static LPSOCKET pSocket; 
     NAME_CACHE_NODE *pNameCache;

     HRESULT NameQueryHelper(LPPROPERTYBAG2 pPropBag, 
     						 VARIANT *pNameVar, 
     						 BT_ADDR ba, 
     						 UINT *uiUpdateStatus);

     HRESULT ChannelQueryHelper(LPPROPERTYBAG2 pPropBag, 
     							VARIANT *pNameVar, 
     							VARIANT *pNameCompletedVar, 
     							VARIANT *pAddressVar,
     							BT_ADDR ba, 
     							IPropertyBagEnum **_ppNewBagEnum, 
     							UINT *uiUpdateStatus,
     							BOOL fGetJustEnoughToConnect);


};





#endif
