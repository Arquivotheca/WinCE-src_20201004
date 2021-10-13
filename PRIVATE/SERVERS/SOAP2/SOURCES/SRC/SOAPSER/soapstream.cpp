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

#include "SoapStream.h"



const int SA_BUFFER_SIZE = 4096;

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CStreamShell::CStreamShell()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CStreamShell::CStreamShell() :
    m_pIStream(NULL),
    m_pIResponse(NULL),
    m_psa(NULL),
    m_dwBuffered(0)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CStreamShell::~CStreamShell()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CStreamShell::~CStreamShell()
{
    clear();
    if (m_psa)
        ::SafeArrayDestroy(m_psa);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CStreamShell::IsInitialized() 
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CStreamShell::IsInitialized() 
{
    if (m_pIStream)
        return S_OK;
    if (m_pIResponse)
        return S_OK;
    return E_FAIL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CStreamShell::write(BYTE * pBuffer, ULONG ulLen)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CStreamShell::write(BYTE * pBuffer, ULONG ulLen)
{
    HRESULT hr(S_OK);
    
    CHK_BOOL(pBuffer, E_INVALIDARG);
    
    if (m_pIStream)
    {
        ULONG ulTemp; 
        
        return (m_pIStream->Write(pBuffer, ulLen, &ulTemp));
    }

    if (m_pIResponse)
        return (WriteResponse(pBuffer, ulLen)); 
    return E_FAIL;
    
Cleanup:
    return hr;
}

    
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CStreamShell::setExtension(WCHAR *pcExtension, VARIANT *pvar, BOOL * pbSuccess)
// 
//
//  parameters:
//
//  description:
//      used to set the HTTP status code in case we are writing into the RESPONSE object
//  returns: 
//      S_FALSE -> if no response object available
//      S_OK    -> if successfully set the response code
//      other  -> return code of put_status  response code
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CStreamShell::setExtension(WCHAR *pcExtension, VARIANT *pvar, BOOL * pbSuccess)
{
    HRESULT hr = E_FAIL;
    CAutoRefc<IResponse> pResponse;
    CAutoBSTR bstrInternalError;

    
    CHK_BOOL(pbSuccess, E_INVALIDARG);
    *pbSuccess = FALSE;

    CHK_BOOL(wcsicmp(szInternalError, pcExtension) == NULL, E_FAIL); // the only extension supported
    CHK(bstrInternalError.Assign((BSTR) g_szStatusInternalError));
    
    if (!m_pIResponse)
    {
        hr = m_pIStream->QueryInterface(IID_IResponse, (void**)&pResponse);
        if (FAILED(hr))
        {
            CAutoRefc<ISOAPIsapiResponse> pISAResponse;
            
        	// Check for SOAP ISAPI interface
        	hr = m_pIStream->QueryInterface(IID_ISOAPIsapiResponse, (void**)&pISAResponse);
        	if (hr == S_OK)
        	{
			    hr = pISAResponse->put_HTTPStatus(bstrInternalError);        
        	}
        	else
        	{
	            hr = S_FALSE;
        	}
	        goto Cleanup;
        }
    }
    else
    {
        assign(&pResponse, (IResponse*)m_pIResponse);
    }
    
    
    CHK( pResponse->put_Status(bstrInternalError));        
    
Cleanup:
    ASSERT(SUCCEEDED(hr));
    if (SUCCEEDED(hr))
        *pbSuccess = TRUE;
    else
        *pbSuccess = FALSE;
    return(hr);
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CStreamShell::setCharset(BSTR bstrCharset)
// 
//
//  parameters:
//      bstrCharset -> charset to set in the contenttype
//  description:
//      used to set the contentype charset in case we write to the response object
//  returns: 
//      S_FALSE -> if no response object available
//      S_OK    -> if successfully set the response code
//      other  -> return code of put_status  response code
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CStreamShell::setCharset(BSTR bstrCharset)
{
    HRESULT hr = E_FAIL;
    CAutoRefc<IResponse> pResponse;

    if (!m_pIResponse)
    {
        hr = m_pIStream->QueryInterface(IID_IResponse, (void**)&pResponse);
        if (FAILED(hr))
        {
            CAutoRefc<ISOAPIsapiResponse> pISAResponse;
            
        	// Check for SOAP ISAPI interface
        	hr = m_pIStream->QueryInterface(IID_ISOAPIsapiResponse, (void**)&pISAResponse);
        	if (hr == S_OK)
        	{
			    hr = pISAResponse->put_HTTPCharset(bstrCharset);        
        	}
        	else
        	{
	            hr = S_FALSE;
        	}
	        goto Cleanup;
        }
    }
    else
    {
        assign(&pResponse, (IResponse*)m_pIResponse);
    }
    
    // if we have a response object, set the complete contenttype
    {
        CAutoFormat af;
        CAutoBSTR bstrTemp;
        
        CHK(af.sprintf(L"text/xml; charset=\"%s\"", bstrCharset));
        CHK(bstrTemp.Assign((BSTR) &af));
        CHK(pResponse->put_ContentType(bstrTemp));        
    }    
    
Cleanup:
    ASSERT(SUCCEEDED(hr));
    return(hr);
    
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CStreamShell::flush()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CStreamShell::flush(void)
{
	if (m_pIStream)
		return m_pIStream->Commit(STGC_DEFAULT);

    if (m_pIResponse)
    {
        if (m_dwBuffered > 0)
            {
                ASSERT (m_psa);
                
                VARIANT vData;

                VariantInit(&vData);
                V_VT(&vData) = VT_ARRAY | VT_UI1;
                V_ARRAY(&vData) = m_psa;
                
                m_dwBuffered = 0;
            
                return m_pIResponse->BinaryWrite(vData);

            }
        return S_OK;
    }
    

	return (E_FAIL);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CStreamShell::clear()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStreamShell::clear()
{
    flush();
    m_pIStream.Clear();
    m_pIResponse.Clear();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CStreamShell::InitStream(IStream * pIStream)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CStreamShell::InitStream(IStream * pIStream)
{
    HRESULT     hr(S_OK);
    
    CHK_BOOL(pIStream, E_INVALIDARG);

    // release any possible contents
    clear();

    m_pIStream = pIStream;

Cleanup:
    ASSERT (hr == S_OK);
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CStreamShell::InitResponse(IResponse * pIResponse)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CStreamShell::InitResponse(IResponse * pIResponse)
{
    HRESULT     hr(S_OK);
    
    CHK_BOOL(pIResponse, E_INVALIDARG);

    // make sure our safearray exists to hold the data,
    if (!m_psa)
    {
        m_psa = SafeArrayCreateVector(VT_UI1, 0, SA_BUFFER_SIZE);
        m_dwBuffered = 0;
        CHK_BOOL(m_psa, E_OUTOFMEMORY);
    }

    // flush and release any possible contents
    clear();

    m_pIResponse = pIResponse;

Cleanup:
    ASSERT (hr == S_OK);
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CStreamShell::WriteResponse(BYTE * pBuffer, ULONG bytesTotal) 
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CStreamShell::WriteResponse(BYTE * pBuffer, ULONG bytesTotal) 
{
    ULONG pos           = 0;            
    ULONG remaining     = bytesTotal;
    BYTE* pSrcBytes     = pBuffer;
    HRESULT hr          = S_OK;


    ASSERT(m_pIResponse);
    ASSERT(m_psa);

    // Now, if the 'pv' buffer being written is bigger than our 4k
    // safearray, then write it out in 4k chunks - so we have a good
    // working set.  If the 'pv' buffer is just a few bytes then buffer 
    // up the bytes until we fill the 4k boundary - and then write the buffer.
    while (remaining > 0)
    {
        BYTE * pB;
        ULONG chunk = remaining;

        if (chunk + m_dwBuffered > SA_BUFFER_SIZE)
        {
            chunk = SA_BUFFER_SIZE - m_dwBuffered;
        }

        CHK ( SafeArrayAccessData(m_psa, (void **)&pB) );

        memcpy(&pB[m_dwBuffered], &pSrcBytes[pos], chunk);

        m_dwBuffered += chunk;
        m_psa->rgsabound[0].cElements = m_dwBuffered;

        SafeArrayUnaccessData(m_psa);

        remaining -= chunk;
        pos += chunk;

        if (m_dwBuffered >= SA_BUFFER_SIZE)
        {
            CHK (flush());
        }
    }

Cleanup:
    return hr;
}



