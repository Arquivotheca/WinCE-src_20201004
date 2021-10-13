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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// BthAPI.cpp : Implementation of DLL Exports.


// Note: Proxy/Stub Information
//      To merge the proxy/stub code into the object DLL, add the file 
//      dlldatax.c to the project.  Make sure precompiled headers 
//      are turned off for this file, and add _MERGE_PROXYSTUB to the 
//      defines for the project.  
//
//      If you are not running WinNT4.0 or Win95 with DCOM, then you
//      need to remove the following define from dlldatax.c
//      #define _WIN32_WINNT 0x0400
//
//      Further, if you are running MIDL without /Oicf switch, you also 
//      need to remove the following define from dlldatax.c.
//      #define USE_STUBLESS_PROXY
//
//      Modify the custom build rule for BthAPI.idl by adding the following 
//      files to the Outputs.
//          BthAPI_p.c
//          dlldata.c
//      To build a separate proxy/stub DLL, 
//      run nmake -f BthAPIps.mk in the project directory.

#include "stdafx.h"
#include "resource.h"
#if ! defined (UNDER_CE)
#include <initguid.h>
#endif
#include "BthAPI.h"
#include "dlldatax.h"

#if defined (UNDER_CE) || defined (WINCE_EMULATION)
#include <svsutil.hxx>
#include "bt_sdp.h"
// for componentization
extern "C" const BOOL g_fBthApiCOM = TRUE;
#endif // UNDER_CE

#include "sdplib.h"
#include "SdpNodeList.h"
#include "SdpNodeContainer.h"
#include "SdpRecord.h"
#include "SdpSearch.h"
#include "SdpStream.h"
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#include "ShellPropSheetExt.h"
#include "BluetoothDevice.h"
#include "BluetoothAuthenticate.h"
#endif


#if ! defined (UNDER_CE)
// On WinCE, sdpuser.dll and btdrt.dll are one.  On NT BT emulation there is no
// btdrt.dll, since all calls to BT core go through RPC and not through
// btdrt shim.  In this case define BthApiDllXXX functions to behave as regular DllXXX com
// exposed functions.
#define BthApiDllMain             DllMain
#define BthApiDllCanUnloadNow     DllCanUnloadNow
#define BthApiDllGetClassObject   DllGetClassObject
#define BthApiDllRegisterServer   DllRegisterServer
#define BthApiDllUnregisterServer DllUnregisterServer
#endif // UNDER_CE


#ifdef _MERGE_PROXYSTUB
extern "C" HINSTANCE hProxyDll;
#endif

HINSTANCE g_hInstance;

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
BOOL WINAPI BthApiDllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpReserved)
{
#ifdef _MERGE_PROXYSTUB
    if (!PrxDllMain((HINSTANCE)hInstance, dwReason, lpReserved))
        return FALSE;
#endif
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        svsutil_Initialize();
        DisableThreadLibraryCalls((HINSTANCE)hInstance);
        g_hInstance = (HINSTANCE)hInstance;
    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        svsutil_DeInitialize ();
    }


    return TRUE;    // ok
}

// 
//  Implements class factory and related functions for SDP
//
long g_cServerLocks = 0; // count of locks on the class factory.

struct SdpClassFactory;

typedef HRESULT (__stdcall *FNCREATEINSTANCE)(REFIID, void **);
typedef void (SdpClassFactory::*FNCLASSFACTORY)();

struct SdpClassFactory /* : IClassFactory */ // codebase class factory
{
public:
    const FNCLASSFACTORY * _vtbl;
    const CLSID *          _pclsid; 
    FNCREATEINSTANCE       _pfnCreateInstance;

public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** ppvObject);

    ULONG STDMETHODCALLTYPE AddRef()
        { return 1; }

    ULONG STDMETHODCALLTYPE Release()
        { return 1; }

    // IClassFactory methods
    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock);

    HRESULT STDMETHODCALLTYPE CreateInstance(
            IUnknown *pUnkOuter,
            REFIID iid,
            void **ppvObj);
};

const FNCLASSFACTORY vtblClassFactory[] = 
{
    (FNCLASSFACTORY)SdpClassFactory::QueryInterface,
    (FNCLASSFACTORY)SdpClassFactory::AddRef,
    (FNCLASSFACTORY)SdpClassFactory::Release,
    (FNCLASSFACTORY)SdpClassFactory::CreateInstance,
    (FNCLASSFACTORY)SdpClassFactory::LockServer
};


HRESULT STDMETHODCALLTYPE 
SdpClassFactory::LockServer(BOOL fLock)
{
    // As per INSIDE COM - we do not count class factories.
    // Instead we count calls to LockServer.
    if (fLock)
        ::InterlockedIncrement(&g_cServerLocks);
    else
        ::InterlockedDecrement(&g_cServerLocks);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE 
SdpClassFactory::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        *ppv = this;
        AddRef();
        return S_OK;    
    }
    else {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

HRESULT STDMETHODCALLTYPE 
SdpClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID iid, void **ppvObj)
{
    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;

    return (_pfnCreateInstance)(iid, ppvObj);
}

HRESULT CreateSdpNodeContainer(REFIID iid, void **ppvObj)
{
    CSdpNodeContainer *pContainer = new CSdpNodeContainer();
    if (! pContainer)
        return E_OUTOFMEMORY;

    return pContainer->QueryInterface(iid,ppvObj);
}

HRESULT CreadeSdpSearch(REFIID iid, void **ppvObj)
{
    CSdpSearch *pSearch = new CSdpSearch();
    if (! pSearch)
        return E_OUTOFMEMORY;

    return pSearch->QueryInterface(iid,ppvObj);
}

HRESULT CreateSdpStream(REFIID iid, void **ppvObj)
{
    CSdpStream *pStream = new CSdpStream();
    if (! pStream)
        return E_OUTOFMEMORY;

    return pStream->QueryInterface(iid,ppvObj);
}

HRESULT CreateSdpRecord(REFIID iid, void **ppvObj)
{
    CSdpRecord *pRecord = new CSdpRecord();
    if (! pRecord)
        return E_OUTOFMEMORY;

    return pRecord->QueryInterface(iid,ppvObj);
}


static const SdpClassFactory s_ComponentFactoryTable [] = 
{
    {vtblClassFactory, &CLSID_SdpNodeContainer, (FNCREATEINSTANCE)CreateSdpNodeContainer},
    {vtblClassFactory, &CLSID_SdpSearch, (FNCREATEINSTANCE)CreadeSdpSearch},
    {vtblClassFactory, &CLSID_SdpStream, (FNCREATEINSTANCE)CreateSdpStream},
    {vtblClassFactory, &CLSID_SdpRecord, (FNCREATEINSTANCE)CreateSdpRecord},
};


/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI BthApiDllCanUnloadNow(void)
{
    // NOTE - Currently leaking btdrt.dll to avoid possible ref count 
    // issues.
    return S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

STDAPI BthApiDllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    for (DWORD i = 0; i < SVSUTIL_ARRLEN(s_ComponentFactoryTable); i++) {
        if (! IsEqualCLSID(*(s_ComponentFactoryTable[i]._pclsid),rclsid))
            continue;

        return ((SdpClassFactory*)&s_ComponentFactoryTable[i])->QueryInterface(riid,ppv);
    }
    
    return CLASS_E_CLASSNOTAVAILABLE;
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI BthApiDllRegisterServer(void)
{
    // Not supported on CE BT, registry settings entered directly into ROM
    return S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI BthApiDllUnregisterServer(void)
{
    // Not supported on CE BT, registry settings entered directly into ROM
    return S_FALSE;
}



