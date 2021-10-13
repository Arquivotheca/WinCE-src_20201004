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
//      HttpConnector.h
//
// Contents:
//
//      CHttpConnector - generic CHttpConnector class declaration
//
//-----------------------------------------------------------------------------

#ifndef __HTTPCONNECTOR_H_INCLUDED__
#define __HTTPCONNECTOR_H_INCLUDED__


class CHttpConnector : public IUnknown, public IInnerUnknown
{
private:
    struct ConnectorLibraryRecord
    {
        bool                bLoaded;
        HMODULE             hModDll;
        pfnCreateConnector  pCreate;
        pfnDllCanUnloadNow  pUnload;

        void Clear();
    };

protected:
    ULONG                   m_cRef;
    CComPointer<IUnknown>   m_pConnector;
    IUnknown               *m_pUnkOuter;

private:
    CHttpConnector(IUnknown *pUnkOuter = 0, ULONG cRef = 0);
    ~CHttpConnector();

    HRESULT InitializeConnector(IUnknown *pConnector);

public:
    //
    // IUnknown implementation
    //
    STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    //
    // IInnerUnknown implementation
    //
    STDMETHOD(InnerQueryInterface)(REFIID riid, void **ppvObject);
    STDMETHOD_(ULONG, InnerAddRef)(void);
    STDMETHOD_(ULONG, InnerRelease)(void);

private:
    static CHAR s_HttpLibLibrary[];
    static CHAR s_WinInetLibrary[];
    static CHAR s_XmlHttpLibrary[];

    static CRWLock                s_rwLock;
    static ConnectorLibraryRecord s_ConnLibRec;

    static HRESULT LoadConnectorLibrary(LPSTR pName, ConnectorLibraryRecord *pRecord);
    static HRESULT LoadConnectorLibrary();
    static HRESULT CreateConnector(REFCLSID rclsid, IUnknown *pUnkOuter, REFIID riid, void **ppvObject);

public:
    static HRESULT CanUnloadNow();
	static HRESULT WINAPI CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline void CHttpConnector::ConnectorLibraryRecord::Clear()
//
//  parameters:
//          
//  description:
//          Clear the members
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CHttpConnector::ConnectorLibraryRecord::Clear()
{
    bLoaded = false;
    hModDll = 0;
    pCreate = 0;
    pUnload = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT CHttpConnector::InitializeConnector(IUnknown *pConnector)
//
//  parameters:
//          
//  description:
//          Initialize owned Http Connector
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT CHttpConnector::InitializeConnector(IUnknown *pConnector)
{
    ASSERT(pConnector != 0);
    m_pConnector = pConnector;
    return S_OK;
}

#endif //__HTTPCONNECTOR_H_INCLUDED__

