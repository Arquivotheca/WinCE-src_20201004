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
// File:    soapstream.cpp
//
// Contents:
//
//  implementation for soapstream
//
//
//-----------------------------------------------------------------------------
#include "headers.h"

#include "charencoder.h"
#include "encStream.h"



static const BYTE    g_ByteOrderMarkUCS2[2] = {0xff, 0xfe};
static const BYTE    g_ByteOrderMarkUCS4[4] = {0xff, 0xfe, 0xff, 0xfe};


const WCHAR szInternalError[] = L"InternalServerError";



//-----------------------------------------------------------------------------
//
// CEncodingStream Implementation
//
//-----------------------------------------------------------------------------

// Initialize static members
const ULONG CEncodingStream::s_ulBufferSize = 4096; // 4K buffer size

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CEncodingStream::CEncodingStream()
//
//  parameters:
//
//  description:
//        Constructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CEncodingStream::CEncodingStream() :
    m_pSoapShell(NULL),
    m_pvBuffer(NULL),
    m_cchBuffer(0),
    m_maxCharSize(3),
    m_codepage(CP_UNDEFINED),
    m_pfnWideCharToMultiByte(NULL)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CEncodingStream::~CEncodingStream()
//
//  parameters:
//
//  description:
//        Destructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CEncodingStream::~CEncodingStream()
{
    flush();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CEncodingStream::SetDestination
//
//  parameters:
//
//  description: set the destination for the encoding.
//              destination can be a IStream or IResponse object
//
//  returns
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEncodingStream::Init(CSoapShell * pShell)
{
#ifndef CE_NO_EXCEPTIONS
    try
#else
    __try
#endif
    {
        HRESULT             hr = S_OK;

        CHK_BOOL(pShell, E_INVALIDARG);
        CHK_BOOL(m_pSoapShell == NULL, E_INVALIDARG);

        m_pSoapShell = pShell;

    Cleanup:
        ASSERT (hr == S_OK);
        return hr;   
    }
#ifndef CE_NO_EXCEPTIONS
    catch(...)
#else
    __except(1)
#endif    
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::put_Stream - Unhandled Exception");
        return E_FAIL;
    }


}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEncodingStream::setEncoding(BSTR bstrEncoding)
//
//  parameters:
//
//  description:
//        Sets stream's encoding
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEncodingStream::setEncoding(BSTR bstrEncoding)
{
    HRESULT hr(S_OK);
#ifndef UNDER_CE
    WCHAR wszEncoding[MAXBYTE];
#endif
    ULONG cchNewBuffer = 0;

    if ( (!bstrEncoding) || (!wcslen(bstrEncoding)) )
        return E_INVALIDARG;

    CHK_BOOL(m_pSoapShell, E_INVALIDARG);

    CHK(CharEncoder::getWideCharToMultiByteInfo(
        (WCHAR*)bstrEncoding,
        &m_codepage,
        &m_pfnWideCharToMultiByte,
        &m_maxCharSize));

    //
    // Compute the new buffer size
    //
    cchNewBuffer = s_ulBufferSize * m_maxCharSize;

    //
    // alloc new buffer if the current one is not big enough
    //
    if (cchNewBuffer > m_cchBuffer)
    {
        // delete the old one
        m_pvBuffer.Clear();

        m_pvBuffer = new BYTE[cchNewBuffer];
        CHK_BOOL (m_pvBuffer != NULL, E_OUTOFMEMORY);
    }

    CHK(m_pSoapShell->setCharset(bstrEncoding));

    //
    // Notice: If the codepage for the current encoding
    // is UCS-2, UCS-4
    //
    if (m_codepage == CP_UCS_2)
        CHK (m_pSoapShell->write((byte*)g_ByteOrderMarkUCS2, 2));

    if (m_codepage == CP_UCS_4)
        CHK (m_pSoapShell->write((byte*)g_ByteOrderMarkUCS4, 4));

    return hr;
Cleanup:
    m_pfnWideCharToMultiByte = NULL;
    m_codepage      = CP_UNDEFINED;
    m_maxCharSize   = 3;
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEncodingStream::write (BSTR buffer, long cbWideChar)
//
//  parameters:
//
//  description:
//        Write
//  returns:
//      E_OUTOFMEMORY if memory allocation failed
//      S_OK          if all the data is read
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEncodingStream::write (BSTR buffer, long cbWideChar)
{
    HRESULT hr = S_OK;
    ULONG cbWritten =0;
    ULONG uBufferPos =0;
    UINT bytesTotal =0, cbTemp=0;

    CHK_BOOL(buffer, E_INVALIDARG);
    CHK_BOOL(m_pSoapShell, E_INVALIDARG);

    CHK_BOOL(m_pfnWideCharToMultiByte, E_FAIL);


    if (cbWideChar == -1)
        cbWideChar = wcslen(buffer);

    while (cbWideChar)
    {
        DWORD dwMode(0);

        cbTemp     = min(cbWideChar, s_ulBufferSize);
        bytesTotal = cbTemp * m_maxCharSize;

        CHK ( m_pfnWideCharToMultiByte(
                            &dwMode, m_codepage,
                            (WCHAR *)( (buffer) + uBufferPos), &cbTemp,
                            m_pvBuffer, &bytesTotal))

        CHK_BOOL(cbTemp >0, E_FAIL);

        CHK (m_pSoapShell->write(m_pvBuffer, bytesTotal));

        cbWritten  += bytesTotal;
        cbWideChar -= cbTemp;
        uBufferPos += cbTemp;
    }
Cleanup:

    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEncodingStream::writeThrough(byte *buffer, long lenBuffer)
//
//  parameters:
//
//  description:
//        Write through
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEncodingStream::writeThrough(
            byte *buffer,
            long lenBuffer)
{
    HRESULT hr(S_OK);

    if ( (!buffer) || (!lenBuffer) )
        return S_OK;

    CHK_BOOL(m_pSoapShell, E_INVALIDARG);

    CHK (m_pSoapShell->write(buffer, lenBuffer));
Cleanup:
    return hr;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEncodingStream::setFaultCode(void)
//
//  parameters:
//
//  description:
//      see the CStreamShell implementation
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEncodingStream::setFaultCode(void)
{
    HRESULT hr(S_OK);
    VARIANT v;
    BOOL b;

    if (!m_pSoapShell)
        return E_INVALIDARG;

    hr = m_pSoapShell->setExtension((WCHAR*)szInternalError, &v, &b);
    if (b)
        return hr;
    if (SUCCEEDED(hr))
        return S_OK;
    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEncodingStream::flush(void)
//
//  parameters:
//
//  description:
//        flush
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEncodingStream::flush(void)
{
    if (!m_pSoapShell)
        return E_FAIL;

    return m_pSoapShell->flush();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CEncodingStream::IsInitialized
//
//  parameters:
//
//  description:
//
//  returns S_OK if the object has a destination stream defines, E_FAIL otherwise
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEncodingStream::IsInitialized(void)
{
    if (!m_pSoapShell)
        return E_FAIL;
    return (m_pSoapShell->IsInitialized());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CEncodingStream::Reset(void)
//
//  parameters:
//
//  description:
//        Reset
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEncodingStream::Reset(void)
{
    if (m_pSoapShell)
        m_pSoapShell->clear();
    m_pSoapShell.Clear();

    m_pfnWideCharToMultiByte = NULL;
    m_codepage      = CP_UNDEFINED;
    m_maxCharSize   = 3;
}

