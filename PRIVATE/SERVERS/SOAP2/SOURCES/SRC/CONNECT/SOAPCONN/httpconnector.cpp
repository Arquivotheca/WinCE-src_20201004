//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      HttpConnector.cpp
//
// Contents:
//
//      CHttpConnector - Universal Http Connector implementation
//
//-----------------------------------------------------------------------------

#include "Headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
// Static member initialization
////////////////////////////////////////////////////////////////////////////////////////////////////

CRWLock CHttpConnector::s_rwLock;
CHttpConnector::ConnectorLibraryRecord CHttpConnector::s_ConnLibRec = { false, 0, 0 };

CHAR CHttpConnector::s_HttpLibLibrary[] = "HlSc10.dll";
CHAR CHttpConnector::s_WinInetLibrary[] = "WiSc10.dll";
CHAR CHttpConnector::s_XmlHttpLibrary[] = "XhSc10.dll";

#ifdef _DEBUG
static CHAR s_DebugHttpLibLibrary[] = "Connect\\HLibConn\\objd\\i386\\HlSc10.dll";
static CHAR s_DebugWinInetLibrary[] = "Connect\\WIConn\\objd\\i386\\WiSc10.dll";
static CHAR s_DebugXmlHttpLibrary[] = "Connect\\XConn\\objd\\i386\\XhSc10.dll";
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CHttpConnector::CHttpConnector(IUnknown *pUnkOuter, ULONG cRef)
//
//  parameters:
//          
//  description:
//          Constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CHttpConnector::CHttpConnector(IUnknown *pUnkOuter, ULONG cRef)
: m_cRef(cRef)
{
    ASSERT(cRef == 0 || cRef == INITIAL_REFERENCE);

    m_pUnkOuter = pUnkOuter ? pUnkOuter : reinterpret_cast<IUnknown*>(static_cast<IInnerUnknown *>(this));

    OBJECT_CREATED();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CHttpConnector::~CHttpConnector()
//
//  parameters:
//          
//  description:
//          Destructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CHttpConnector::~CHttpConnector()
{
    m_pUnkOuter = 0;

    OBJECT_DELETED();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CHttpConnector::QueryInterface(REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          QueryInterface
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CHttpConnector::QueryInterface(REFIID riid, void **ppvObject)
{
    ASSERT(m_pUnkOuter);
    return m_pUnkOuter->QueryInterface(riid, ppvObject);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP_(ULONG) CHttpConnector::AddRef()
//
//  parameters:
//          
//  description:
//          AddRef
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP_(ULONG) CHttpConnector::AddRef()
{
    ASSERT(m_pUnkOuter);
    return m_pUnkOuter->AddRef();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP_(ULONG) CHttpConnector::Release()
//
//  parameters:
//          
//  description:
//          Release
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP_(ULONG) CHttpConnector::Release()
{
    ASSERT(m_pUnkOuter);
    return m_pUnkOuter->Release();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CHttpConnector::InnerQueryInterface(REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Inner query interface - aggregation
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CHttpConnector::InnerQueryInterface(REFIID riid, void **ppvObject)
{
    if (! ppvObject)
    {
        return E_INVALIDARG;
    }

    if (riid == IID_IUnknown)
    {
        AddRef();
        *ppvObject = reinterpret_cast<void **>(static_cast<IUnknown *>(this));
        return S_OK;
    }

    if (m_pConnector)
    {
        return m_pConnector->QueryInterface(riid, ppvObject);
    }
    else
    {
        return E_NOINTERFACE;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP_(ULONG) CHttpConnector::InnerAddRef(void)
//
//  parameters:
//          
//  description:
//          Inner AddRef - aggregation
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP_(ULONG) CHttpConnector::InnerAddRef(void)
{
    return ::AtomicIncrement(reinterpret_cast<LPLONG>(&m_cRef));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP_(ULONG) CHttpConnector::InnerRelease(void)
//
//  parameters:
//          
//  description:
//          Inner Release - aggregation
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP_(ULONG) CHttpConnector::InnerRelease(void)
{
    ULONG cRef = ::AtomicDecrement(reinterpret_cast<LPLONG>(&m_cRef));
    if (! cRef)
    {
        ::AtomicIncrement(reinterpret_cast<LPLONG>(&m_cRef));
        delete this;
    }
    return cRef;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnector::LoadConnectorLibrary(LPWSTR pName, ConnectorLibraryRecord *pRecord)
//
//  parameters:
//          
//  description:
//          Attempts to load given HTTP connector library and retrieves function addresses from it
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnector::LoadConnectorLibrary(LPSTR pName, ConnectorLibraryRecord *pRecord)
{
    HRESULT             hr        = S_OK;
    HMODULE             hModDll   = 0;
    pfnCreateConnector  pCreate   = 0;
    pfnDllCanUnloadNow  pUnload   = 0;
    DWORD               dwNameLen = 0;
    CHAR                pModName[MAX_PATH + 1] = { 0 };
    CHAR               *pNameEnd  = 0;

    ASSERT(pName != 0);
    ASSERT(pName == s_HttpLibLibrary || pName == s_WinInetLibrary || pName == s_XmlHttpLibrary);
    
    if ((dwNameLen = GetModuleFileNameA(g_hInstance, pModName, MAX_PATH)) == 0)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Cleanup;
    }

    pNameEnd = pModName + dwNameLen;

    while (-- pNameEnd >= pModName && *pNameEnd != '\\')
    {
        ;
    }

    pNameEnd ++;

    dwNameLen = strlen(pName);

    if (pNameEnd - pModName + dwNameLen > MAX_PATH)
    {
        hr = E_FAIL;
        goto Cleanup;
    }

    strcpy(pNameEnd, pName);    

    if ((hModDll = ::LoadLibraryExA(pModName, 0, LOAD_WITH_ALTERED_SEARCH_PATH)) == 0)
    {
#ifdef _DEBUG
        for (int counter = 0; counter < 4; counter ++)
        {
            while (-- pNameEnd >= pModName && *pNameEnd != '\\')
            {
                ;
            }
        }

        pNameEnd ++;


        if (pName == s_HttpLibLibrary)
        {
            pName = s_DebugHttpLibLibrary;
        }
        else if (pName == s_WinInetLibrary)
        {
            pName = s_DebugWinInetLibrary;
        }
        else if (pName == s_XmlHttpLibrary)
        {
            pName = s_DebugXmlHttpLibrary;
        }

        strcpy(pNameEnd, pName);

        if ((hModDll = ::LoadLibraryExA(pModName, 0, LOAD_WITH_ALTERED_SEARCH_PATH)) == 0)
        {
            hr = E_FAIL;
            goto Cleanup;
        }
#else
        hr = E_FAIL;
        goto Cleanup;
#endif
    }

#ifndef UNDER_CE
    pCreate = reinterpret_cast<pfnCreateConnector>(GetProcAddress(hModDll, "CreateConnector"));
    pUnload = reinterpret_cast<pfnDllCanUnloadNow>(GetProcAddress(hModDll, "DllCanUnloadNow"));
#else
    pCreate = reinterpret_cast<pfnCreateConnector>(GetProcAddress(hModDll, L"CreateConnector"));
    pUnload = reinterpret_cast<pfnDllCanUnloadNow>(GetProcAddress(hModDll, L"DllCanUnloadNow"));
#endif

    if (pCreate == 0 || pUnload == 0)
    {
        hr = E_FAIL;
        goto Cleanup;
    }

    pRecord->bLoaded = true;
    pRecord->hModDll = hModDll; 
    pRecord->pCreate = pCreate;
    pRecord->pUnload = pUnload;
    hModDll          = 0;

Cleanup:
    if (hModDll != 0)
        ::FreeLibrary(hModDll);

    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnector::LoadConnectorLibrary()
//
//  parameters:
//          
//  description:
//          Makes sure the appropriate HTTP connector library is loaded
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnector::LoadConnectorLibrary()
{
    HRESULT                hr = E_FAIL;

    ConnectorLibraryRecord record;

    record.Clear();
   
#ifndef UNDER_CE
    OSVERSIONINFOA         vi;
    vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    
    if (::GetVersionExA(&vi) && vi.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
        TRACE(("Loading HttpLib Connector"));
        hr = LoadConnectorLibrary(s_HttpLibLibrary, &record);
    }

    if (FAILED(hr))
    {
        TRACE(("Loading WinInet Connector"));
        hr = LoadConnectorLibrary(s_WinInetLibrary, &record);
    }

    if (FAILED(hr))
    {
        TRACE(("Loading XmlHttp Connector"));
        hr = LoadConnectorLibrary(s_XmlHttpLibrary, &record);
    }
#else
    TRACE(("Loading WinInet Connector"));
    hr = LoadConnectorLibrary(s_WinInetLibrary, &record);
    
    if (FAILED(hr))
    {
        TRACE(("Loading XmlHttp Connector"));
        hr = LoadConnectorLibrary(s_XmlHttpLibrary, &record);
    }
#endif


    if (FAILED(hr))
    {
        goto Cleanup;
    }

    ASSERT(record.bLoaded);
    ASSERT(record.hModDll);
    ASSERT(record.pCreate);
    ASSERT(record.pUnload);

    ASSERT(s_ConnLibRec.bLoaded == false);
    ASSERT(s_ConnLibRec.hModDll == 0);
    ASSERT(s_ConnLibRec.pCreate == 0);
    ASSERT(s_ConnLibRec.pUnload == 0);

    s_ConnLibRec = record;

    hr = S_OK;

Cleanup:
    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnector::CreateConnector(REFCLSID rclsid, IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Function to create platform specific HTTP connector (inner one)
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnector::CreateConnector(REFCLSID rclsid, IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
{
    ASSERT(pUnkOuter);
    ASSERT(ppvObject);

    HRESULT hr = S_OK;

    s_rwLock.AcquireReaderLock();

    if (s_ConnLibRec.bLoaded == false)
    {
        s_rwLock.UpgradeToWriterLock();

        if (s_ConnLibRec.bLoaded == false)              // Nobody loaded in-between
        {
#ifdef CE_NO_EXCEPTIONS
            __try{
            hr = LoadConnectorLibrary();
            }
            __except(1)
            {
                hr = E_FAIL;
            }
#else
            try   
            {
                hr = LoadConnectorLibrary();
            }

            catch(...)
            {
            }
#endif

        }

        if (FAILED(hr))
        {
            s_rwLock.ReleaseWriterLock();
            goto Cleanup;
        }
        else
        {
            s_rwLock.DowngradeFromWriterLock();
        }
    }

    // Still have reader lock

    ASSERT(s_ConnLibRec.bLoaded);
    ASSERT(s_ConnLibRec.hModDll);
    ASSERT(s_ConnLibRec.pCreate);
    ASSERT(s_ConnLibRec.pUnload);
    
#ifdef CE_NO_EXCEPTIONS
    __try
#else
    try
#endif
    {
        hr = s_ConnLibRec.pCreate(rclsid, pUnkOuter, riid, ppvObject);
    }

#ifdef CE_NO_EXCEPTIONS
    __except(1)
#else
    catch(...)
#endif

    {
    }

    s_rwLock.ReleaseReaderLock();

Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT WINAPI CHttpConnector::CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Create and ininitialize CHttpConnector depending on the plaftorm
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT WINAPI CHttpConnector::CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
{
    if (! ppvObject)
        return E_INVALIDARG;
    if (pUnkOuter && riid != IID_IUnknown)
        return CLASS_E_NOAGGREGATION;

    *ppvObject = 0;

    HRESULT         hr    = S_OK;
    IUnknown       *pConn = 0;
    CHttpConnector *pObj  = new CHttpConnector(pUnkOuter, INITIAL_REFERENCE);
    if (!pObj)
        return E_OUTOFMEMORY;

    if (pUnkOuter == 0)
    {
        pUnkOuter = static_cast<IUnknown *>(pObj);
    }

    CHK(CreateConnector(CLSID_NULL, pUnkOuter, IID_IUnknown, reinterpret_cast<void **>(&pConn)));
    CHK(pObj->InitializeConnector(pConn));
    CHK(pObj->QueryInterface(riid, ppvObject));

Cleanup:
    ::ReleaseInterface(pObj);
    ::ReleaseInterface(pConn);
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnector::CanUnloadNow()
//
//  parameters:
//          
//  description:
//          Decides whether Connector platform specific DLL can be unloaded
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnector::CanUnloadNow()
{
    HRESULT hr = S_OK;

    s_rwLock.AcquireWriterLock();

    if (s_ConnLibRec.bLoaded)
    {
        ASSERT(s_ConnLibRec.hModDll);
        ASSERT(s_ConnLibRec.pCreate);
        ASSERT(s_ConnLibRec.pUnload);

#ifdef CE_NO_EXCEPTIONS
        __try
#else
        try
#endif
        {
            hr = s_ConnLibRec.pUnload();
        }
#ifdef CE_NO_EXCEPTIONS
        __except(1)
#else
        catch (...)
#endif
        {
            s_ConnLibRec.Clear();
            hr = S_OK;
        }

        if (hr == S_OK)
        {
            if (s_ConnLibRec.hModDll)
                ::FreeLibrary(s_ConnLibRec.hModDll);

            s_ConnLibRec.Clear();
        }
    }

    s_rwLock.ReleaseWriterLock();

    return hr;
}
