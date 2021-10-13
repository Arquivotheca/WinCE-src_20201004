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
//      WinInetResponseStream.cpp
//
// Contents:
//
//      CWinInetReposnseStream class implementation
//
//-----------------------------------------------------------------------------

#include "Headers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface map
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_INTERFACE_MAP(CWinInetResponseStream)
    ADD_IUNKNOWN(CWinInetResponseStream, IStream)
    ADD_INTERFACE(CWinInetResponseStream, IStream)
    ADD_INTERFACE(CWinInetResponseStream, ISequentialStream)
END_INTERFACE_MAP(CWinInetResponseStream)

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWinInetResponseStream::CWinInetResponseStream()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CWinInetResponseStream::CWinInetResponseStream()
{
    m_pOwner      = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWinInetResponseStream::~CWinInetResponseStream()
//
//  parameters:
//
//  description:
//          Destructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CWinInetResponseStream::~CWinInetResponseStream()
{
    ASSERT(m_pOwner == 0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWinInetResponseStream::set_Owner(CWinInetConnector *pOwner)
//
//  parameters:
//
//  description:
//          Sets the owner of CSoapOwnedObject
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWinInetResponseStream::set_Owner(CWinInetConnector *pOwner)
{
    ASSERT(m_pOwner == 0);
    ASSERT(pOwner   != 0);

    m_pOwner = pOwner;

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CWinInetResponseStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CWinInetResponseStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
{
    HRESULT   hr     = S_OK;
#ifndef UNDER_CE
    HINTERNET hFile  = 0;
#else
    async_internet_handle *hFile = 0;
#endif 
    DWORD     dwRead = 0;

    ASSERT(m_pOwner != 0);
    TRACE(("CWinInetResponse::Read\n"));

    hr = m_pOwner->get_RequestHandle(& hFile);
    if (FAILED(hr))
    {
        goto Cleanup;
    }

#ifndef UNDER_CE
    if (::InternetReadFile(hFile, pv, cb, &dwRead) != TRUE)
    {
        hr = MAP_WININET_ERROR();
        goto Cleanup;
    }
#else
    if (InternetReadFile((HINTERNET)(*hFile), pv, cb, &dwRead) != TRUE)
    {
        VARIANT v;
        VariantInit(&v);
        hr = m_pOwner->get_Property(L"Timeout", &v);
     
        if(SUCCEEDED(hr)) {
             hr = hFile->wait(v.lVal);                
             hr = MAP_WININET_ERROR_(HRESULT_FROM_WIN32(hr)); 
        }

        VariantClear(&v);
   
        goto Cleanup;
    }      
#endif /* UNDER_CE */


Cleanup:
    if (pcbRead)
        *pcbRead = dwRead;
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CWinInetResponseStream::Write(const void *pv, ULONG cb, ULONG *pcbWritten)
//
//  parameters:
//
//  description:
//          Does nothing as response stream is read only...
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CWinInetResponseStream::Write(const void *pv, ULONG cb, ULONG *pcbWritten)
{
    if (pcbWritten)
        *pcbWritten = 0;
    return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

