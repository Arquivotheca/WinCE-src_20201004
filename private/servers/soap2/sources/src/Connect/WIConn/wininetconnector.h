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
//      WinInetConnector.h
//
// Contents:
//
//      CWinInetConnector class declaration
//
//-----------------------------------------------------------------------------

#ifndef __WININETCONNECTOR_H_INCLUDED__
#define __WININETCONNECTOR_H_INCLUDED__


#define MAP_WININET_ERROR_(hr) CWinInetConnector::WinInetErrorToHResult(hr)
#define MAP_WININET_ERROR()    MAP_WININET_ERROR_(HRESULT_FROM_WIN32(GetLastError()))


#ifdef UNDER_CE
#include "httprequest.h"
#endif

class CWinInetConnector : public CDispatchImpl<CHttpConnBase> 
{
private:
    HINTERNET               m_hInternet;
#ifdef UNDER_CE
    async_internet_handle   m_hConnect;
    async_internet_handle   m_hRequest;
#else
    HINTERNET   m_hConnect;
    HINTERNET   m_hRequest;
#endif 


    CWinInetRequestStream  *m_pRequest;
    CWinInetResponseStream *m_pResponse;

protected:
    CWinInetConnector();
    ~CWinInetConnector();

public:
    //
    // ISoapConnector
    //

    STDMETHOD(get_InputStream)(IStream **pVal);
    STDMETHOD(get_OutputStream)(IStream **pVal);

    //
    // Internal methods
    //
#ifndef UNDER_CE
    HRESULT get_RequestHandle(HINTERNET *pHandle);
#else
	HRESULT get_RequestHandle(async_internet_handle **pHandle);
#endif 


private:
    //
    // Implementation code
    //

    STDMETHOD(EstablishConnection)();
    STDMETHOD(ShutdownConnection)();
    STDMETHOD(SendRequest)();
    STDMETHOD(ReceiveResponse)();
    STDMETHOD(EmptyBuffers)();

public:
    static HRESULT WinInetErrorToHResult(HRESULT hr);

private:
#ifdef _DEBUG
    STDMETHOD_(bool, IsConnected)();
#endif

    HRESULT AddHeaders();

    static const WCHAR s_szSoapAction[];
    static const WCHAR s_szNewLine[];
    static const WCHAR s_Quote;

    DECLARE_INTERFACE_MAP;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT CWinInetConnector::get_RequestHandle(HINTERNET *pHandle)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef UNDER_CE
inline HRESULT CWinInetConnector::get_RequestHandle(HINTERNET *pHandle)
#else
inline HRESULT CWinInetConnector::get_RequestHandle(async_internet_handle **pHandle)
#endif 
{
    ASSERT(pHandle);

#ifndef UNDER_CE
    *pHandle = m_hRequest;
#else
	*pHandle = &m_hRequest;
#endif 

    return S_OK;
}


#endif //__WININETCONNECTOR_H_INCLUDED__
