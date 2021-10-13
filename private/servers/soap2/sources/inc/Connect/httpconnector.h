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

