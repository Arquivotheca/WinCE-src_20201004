//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#include "stdafx.h"

// DEBUG_RPC_COM_PROXY gets extra debugspew in error cases
#define DEBUG_RPC_COM_PROXY

const CLSID CLSID_MediaPlayerControl = { 0x7f5bebd0, 0x184a, 0x4796, { 0xbe, 0x37, 0xa6, 0x13, 0xfc, 0x5e, 0xc1, 0x3f } };

RpcProxyConnector* g_pProxyConnector[3];

class MediaPlayerControlProxy : public RpcComProxyRoot,
                                public IMediaPlayerControl
{
public:
    MediaPlayerControlProxy(RpcProxyConnector *pRpcConnector, PCONTEXT_HANDLE_TYPE initialContext)
    {
        ;
    }

    STDMETHODIMP Play(LPWSTR pszName)
    {
        RPC_COM_PROXY_MAKE_CALL(IMediaPlayerControl_Play,(GetCtxHandle(), pszName));
    }

    STDMETHODIMP Stop()
    {
        RPC_COM_PROXY_MAKE_CALL(IMediaPlayerControl_Stop,(GetCtxHandle()));
    }

    STDMETHODIMP Pause()
    {
        RPC_COM_PROXY_MAKE_CALL(IMediaPlayerControl_Pause,(GetCtxHandle()));
    }

    STDMETHODIMP SetVolume(unsigned short Volume)
    {
        RPC_COM_PROXY_MAKE_CALL(IMediaPlayerControl_SetVolume,(GetCtxHandle(), Volume));
    }

    STDMETHODIMP SetMetadata(INT type, LPCWSTR pszTitle)
    {
        RPC_COM_PROXY_MAKE_CALL(IMediaPlayerControl_SetMetadata,(GetCtxHandle(), type, pszTitle));
    }

    STDMETHODIMP GetVolume(unsigned short* pVolume)
    {
        RPC_COM_PROXY_MAKE_CALL(IMediaPlayerControl_GetVolume,(GetCtxHandle(), pVolume));
    }

    STDMETHODIMP GetCurrentPosition(__int64 *pllPosition)
    {
        RPC_COM_PROXY_MAKE_CALL(IMediaPlayerControl_GetCurrentPosition,(GetCtxHandle(), pllPosition));
    }

    STDMETHODIMP SetPositions(__int64 *pllStart, unsigned long dwStartFlags, __int64 *pllStop, unsigned long dwStopFlags)
    {
        RPC_COM_PROXY_MAKE_CALL(IMediaPlayerControl_SetPositions,(GetCtxHandle(), pllStart, dwStartFlags, pllStop, dwStopFlags));
    }

    virtual ~MediaPlayerControlProxy() 
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) 
    { 
        if (riid == IID_IUnknown) 
        { 
            *ppvObject = reinterpret_cast<IUnknown*>(this); 
            AddRef(); 
            return S_OK; 
        }
        else if (riid == __uuidof(IMediaPlayerControl)) 
        { 
            *ppvObject = (IMediaPlayerControl*)this; 
            AddRef(); 
            return S_OK; 
        }

        *ppvObject = NULL; 
        return E_NOTIMPL; 
    }
};

typedef RpcComProxyObject<MediaPlayerControlProxy> MediaPlayerControlProxyObject;

HRESULT MediaPlayerProxyInit(INT index, LPWSTR endpoint)
{
    HRESULT hr = S_OK;
    CPREx(endpoint, E_FAIL);
    g_pProxyConnector[index] = new RpcProxyConnector(endpoint,&MediaPlayerControlRemote_IfHandle);
    if (!g_pProxyConnector[index]) 
    {
        return E_OUTOFMEMORY;
    }

    hr = g_pProxyConnector[index]->EstablishConnection();
    if (FAILED(hr)) 
    {
        delete g_pProxyConnector[index];
        g_pProxyConnector[index] = NULL;
    }
Error:
    return hr;
}

//
// Called during shutdown to tear-down RPC connection.
//
HRESULT MediaPlayerProxyDeInit(INT index) 
{
    if (g_pProxyConnector[index]) 
    {
        g_pProxyConnector[index]->CloseConnection();
        delete g_pProxyConnector[index];
        g_pProxyConnector[index] = NULL;
    }
    return S_OK;
}

HRESULT CreateMediaPlayerProxy(INT index, IMediaPlayerControl **ppControl) 
{
    HRESULT hr;
    CPREx(ppControl, E_FAIL);
    *ppControl = NULL;
    MediaPlayerControlProxyObject *pProxy;
    hr = g_pProxyConnector[index]->CreateInstance<MediaPlayerControlProxyObject>(GUID_NULL,GUID_NULL,&pProxy);
    if (FAILED(hr)) 
    {
        return hr;
    }
    *ppControl = pProxy;
Error:
    return hr;
}

//
//  MIDL alloc & delete routines, no need to override in this case.
//
extern "C" void __RPC_FAR * __RPC_API midl_user_allocate (size_t len) {
    return malloc(len);
}

extern "C" void __RPC_API midl_user_free (void * __RPC_FAR ptr) {
    free (ptr);
}


