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
// Definitions that are needed for simulating out-of-proc COM using raw RPC
// Proxy (client) Side

#pragma once

#include <windows.h>
#include <strsafe.h>

// RpcComProxy.h cannot use standard DEBUGMSG zones because its compiled statically
// into DLL/EXE & doesn't know which zone to use.  Therefore, to enable debugging
// at compile time app needs to set DEBUG_RPC_COM_PROXY.

#ifdef DEBUG_RPC_COM_PROXY
#define DEBUGMSG_RPC_COM_PROXY(x)  RETAILMSG(1,x)
#else
#define DEBUGMSG_RPC_COM_PROXY(x)
#endif


// Wrap the RPC RpcTryExcept/RpcExcept/RpcEndExcept around all calls.  Most
// CE programming does not use exceptions, so just swallow them here and convert
// to HRESULT rather than forcing every caller to special case this.
#define RPC_COM_PROXY_MAKE_CALL_BASE(FunctionName,FunctionArguments) \
    RpcTryExcept { \
        hr = FunctionName FunctionArguments; \
    } \
    RpcExcept (1) { \
        DEBUGMSG_RPC_COM_PROXY((L"RPCCOM PROXY: Call to remote proc <%S> failed, hr=<0x%08x>\r\n",#FunctionName,RpcExceptionCode())); \
        /* Unfortunately we have to return the borderline useless E_FAIL, rather */ \
        /*  than real code, because a common construct in COM programming is to check */ \
        /* if (FAILED(hr)), and since RPC error codes are < 0x80000000, it would treat*/ \
        /* this as a successful connection. */ \
        hr = E_FAIL; \
    } \
    RpcEndExcept \
 
// Wraps call to RPC_COM_PROXY_MAKE_CALL_BASE, including basic checks and
// direct return.  Allows a proxy method to be written in 1 line in simple case
#define RPC_COM_PROXY_MAKE_CALL(FunctionName,FunctionArguments) \
    HRESULT hr; \
    if (GetRefCount() == 0) { \
        DEBUGMSG(1,(L"ERROR: RPC Server Context Not Set prior to RPC call\r\n")); \
        ASSERT(0);     \
        return E_FAIL; \
    } \
    RPC_COM_PROXY_MAKE_CALL_BASE(FunctionName,FunctionArguments) \
    return hr;


//
//  Abstracts out logic for connecting to RPC server.  Clients need to create
//  a global RpcProxyConnector for each RPC endpoint they will connect to (typically
//  1) early at init stage.  Then they will call member CreateInstance() to actually
//  create the COM object on the server, and to create a proxy object for use on client.
//
//  Creating and deleting RpcProxyConnector object is DllMain safe
//  Calling EstablishConnection and CloseConnection are not.
//
class RpcProxyConnector
{
protected:
    handle_t        *m_pInterfacePointer;
    WCHAR            m_szEndpointName[MAX_PATH];

public:
    RpcProxyConnector(WCHAR *szEndpoint, handle_t *pIfHandle)
    {
        ASSERT(pIfHandle != NULL);

        m_pInterfacePointer = pIfHandle;

        // Note: Copy end point name, rather than doing a RpcStringBindingCompose directly,
        // so that constructor can be called from DLLMain (RPC isn't DllMain safe).
        StringCchCopy(m_szEndpointName,_countof(m_szEndpointName),szEndpoint);
    }

    // Destructor must be DLLMain safe, so no RPC cleanup here.
    ~RpcProxyConnector()
    {
        ;
    }

public:
    // Establishs RPC binding to server.  Not DLLMain safe.  This must
    // be called by app prior to first attempt to use RPC server.
    HRESULT EstablishConnection()
    {
        DWORD dw;
        WCHAR  *szBindingString = NULL;

        // Build up binding string for connection
        dw = RpcStringBindingCompose(NULL,L"ncalrpc",NULL,m_szEndpointName,NULL,&szBindingString);
        if (RPC_S_OK != dw)
        {
            DEBUGMSG_RPC_COM_PROXY((L"RPCCOM PROXY: RpcStringBindingCompose <endpoint=%s> failed, err=<0x%08x>\r\n",m_szEndpointName,dw));
            // We need to convert the code to a COM style error code so FAILED(hr) will
            // work on this.
            goto done;
        }

        // Actually connect to RPC server
        if (RPC_S_OK != (dw = RpcBindingFromStringBinding(szBindingString, m_pInterfacePointer)))
        {
            DEBUGMSG_RPC_COM_PROXY((L"RPCCOM PROXY: RpcBindingFromStringBinding <binding=%s> failed, err=<0x%08x>\r\n",szBindingString,dw));
            goto done;
        }
done:
        if (szBindingString)
            RpcStringFree(&szBindingString);

        return (dw==RPC_S_OK) ? S_OK : E_FAIL;
    }

    // Called to force RPC connection to be closed.  This will invalidate
    // any RPC connections to proxies referencing this object.  It is used
    // in case proxies didn't properly clean up after themselves.  Not DLLMain
    // safe, but appropriate to be used at some point of app/dll shutdown prior to that.
    HRESULT CloseConnection()
    {
        return RpcBindingFree(m_pInterfacePointer);
    }

    // Call this function to actually instantiate an object via CoCreateInstance
    // on the server side.
    template<class ObjectType>
    HRESULT CreateInstance(CLSID clsid, REFIID iid, ObjectType **ppNewObject)
    {
        PCONTEXT_HANDLE_TYPE ctxType = NULL;
        HRESULT hr;

        RPC_COM_PROXY_MAKE_CALL_BASE(RpcProxy_CreateObject,(clsid,iid,&ctxType));

        if (RPC_S_OK != hr)
            return hr;

        *ppNewObject = new ObjectType(this,ctxType);
        if (NULL == (*ppNewObject))
        {
            // Release the object on the server in error case
            RPC_COM_PROXY_MAKE_CALL_BASE(RpcProxy_Release,(&ctxType));
            return E_OUTOFMEMORY;
        }

        return S_OK;
    }
};

//
//  Implements root of all objects that use these classes.
//  Your COM object implementing proxy needs to inherit from this class.
//  Inspired by ATL's CComObjectRoot.
//
class RpcComProxyRoot
{
protected:
    // When using multiple inheritance, multiple instances of RpcComProxyRoot
    // may be brought in.  To avoid multiple copies of data, we define virtual
    // functions in this root object and actually implement them in RpcComProxyObject.
    virtual ULONG GetRefCount() = 0;
    virtual PCONTEXT_HANDLE_TYPE GetCtxHandle() = 0;
    virtual RpcProxyConnector* GetRpcConnector() = 0;

public:
    RpcComProxyRoot()
    {
        ;
    }

    virtual ~RpcComProxyRoot()
    {
        ;
    }
};

//
// Actual implementation of the proxy object - inherits from the class that you
// declare with details about your COM proxy.  In particular defines the IUnknown functions -
// we do it in this class (in theory) to support COM proxies inheriting from
// multiple interfaces.  Inspired by ATL's CComObject.
//
template<class Base>
class RpcComProxyObject : public Base
{
protected:
    // Reference count
    LONG                  m_dwRefCount;
    // "Handle" to actual Interface object in server
    PCONTEXT_HANDLE_TYPE  m_ServerContext;
    // Pointer to class that actually establishes RPC connection.
    RpcProxyConnector    *m_pRpcConnector;
    // Indicates whether this class owns the m_ServerContext and is responsible for telling the RPC server to destroy it.
    // There are instances where multiple RpcComProxyObject may be instantiated and pointed at the same serverContext,
    // see as an example the Inheritance sample of the DogProxy containing Animal & Purchase interfaces.
    BOOL                  m_fOwnsServerContext;

    //
    // Implement RpcComProxyRoot virtual functions, so there's only one copy
    // of member variables per proxy object
    //
    ULONG GetRefCount()
    {
        return m_dwRefCount;
    }

    PCONTEXT_HANDLE_TYPE GetCtxHandle()
    {
        return m_ServerContext;
    }

    RpcProxyConnector* GetRpcConnector()
    {
        return m_pRpcConnector;
    }


public:
    RpcComProxyObject(RpcProxyConnector *pRpcConnector, PCONTEXT_HANDLE_TYPE serverContext, BOOL fOwnsServerContext=TRUE) :
            Base(pRpcConnector, serverContext)
    {
        m_pRpcConnector       = pRpcConnector;
        m_ServerContext       = serverContext;
        m_dwRefCount          = 1;
        m_fOwnsServerContext  = fOwnsServerContext;
    }

    ~RpcComProxyObject()
    {
        HRESULT hr;

        if (m_fOwnsServerContext)
        {
            RPC_COM_PROXY_MAKE_CALL_BASE(RpcProxy_Release,(&m_ServerContext));
        }
    }

    //
    //  Implement IUnknown.  Note that we don't marshall AddRef/Release to the
    //  server.  We add 1 reference on creation of server object and during
    //  destructor release this ref.
    //
    ULONG STDMETHODCALLTYPE AddRef()
    {
        ULONG ulRef = InterlockedIncrement(&m_dwRefCount);
        return ulRef;
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        unsigned cRef = InterlockedDecrement(&m_dwRefCount);

        if (cRef != 0)
            return cRef;

        delete this;
        return 0;
    }

    // Caller must implement QueryInterface.  In ATL world, the BEGIN_COM_MAP &
    // COM_INTERFACE_ENTRY macros do his automatically, but that's over-kill for RpcComProxy.
};


