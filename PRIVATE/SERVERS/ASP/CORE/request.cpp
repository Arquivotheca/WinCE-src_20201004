//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: request.cpp
Abstract: Implements Request script object
--*/

#include "aspmain.h"

/////////////////////////////////////////////////////////////////////////////
// CRequest


STDMETHODIMP CRequest::get_TotalBytes(long *pVal)
{
	DEBUGMSG(ZONE_REQUEST,(L"ASP: In get Total Bytes!!\r\n"));

	*pVal = (long) m_pASPState->m_pACB->cbTotalBytes;
	return S_OK;
}

//  Like GetServerVariable ISAPI call.
STDMETHODIMP CRequest::get_ServerVariables(IRequestDictionary **ppDictReturn)
{
	DEBUGMSG(ZONE_REQUEST,(L"ASP: get_ServerVariables!!\r\n"));
	if (m_pServerVariables)
		return m_pServerVariables->QueryInterface(IID_IRequestDictionary,(void **) ppDictReturn);

	return E_NOTIMPL;
}


STDMETHODIMP CRequest::get_QueryString(IRequestDictionary **ppDictReturn)
{
	DEBUGMSG(ZONE_REQUEST,(L"ASP: In get_QueryString!!\r\n"));

	if (m_pQueryString)
		return m_pQueryString->QueryInterface(IID_IRequestDictionary,(void **) ppDictReturn);

	return E_NOTIMPL;
}

STDMETHODIMP CRequest::get_Cookies(IRequestDictionary **ppDictReturn)
{
	DEBUGMSG(ZONE_REQUEST,(L"ASP: In CRequest::get_Cookies!!\r\n"));

	if (m_pCookie)
		return m_pCookie->QueryInterface(IID_IRequestDictionary,(void **) ppDictReturn);
	
	return E_NOTIMPL;
}

STDMETHODIMP CRequest::get_Form(IRequestDictionary **ppDictReturn)
{
	DEBUGMSG(ZONE_REQUEST,(L"ASP: In get_Form!!\r\n"));

	if (m_pForm) {
		PASP_CONTROL_BLOCK pACB = m_pASPState->m_pACB;

		if (! SetStateIfPossible(FORMCOLLECTIONONLY))
			return E_FAIL;

		if ((pACB->cbTotalBytes > pACB->cbAvailable) && !m_fParsedPost) {
			// There's bytes that still need to be read off the wire.  Do so now.

			// Note: For efficiency ReceiveCompleteRequest may update pszForm member of
			// the pACB structure, which points to the POST data in the raw HTTPD buffer
			// that has been (potentially) reallocated.  DO NOT use pszForm before making this
			// call, unless using a BinaryRead or IStream::Read, which 
			// will guarantee this will path never be called.
			if (! pACB->ReceiveCompleteRequest(pACB))
				return E_FAIL;
		}

		if (!m_fParsedPost) {
			m_pForm->m_pszRawData = pACB->pszForm;
			m_pForm->ParseInput();
			m_fParsedPost = TRUE;
		}
		
		return m_pForm->QueryInterface(IID_IRequestDictionary,(void **) ppDictReturn);
	}
	return E_NOTIMPL;
}


//  NOTE:  IIS has does an async network reading on this.  No support in WinCE
//  CE HTTPD will read in all of HTTPD body (even if it's > 48K Post Read Size)
//  so doesn't make sense for this to call a read (so it's unlike ISAPI extetnions ReadClient()).
STDMETHODIMP CRequest::BinaryRead(VARIANT *pvarCount, VARIANT *pvarReturn)
{
	DEBUG_CODE_INIT;
	HRESULT ret = DISP_E_EXCEPTION;
    HRESULT hr = NOERROR;
    SAFEARRAYBOUND rgsabound[1];
    size_t cbToRead = 0;
    size_t cbRead = 0;
    BYTE *pbData = NULL;

	if (! SetStateIfPossible(BINARYREADONLY))
		myretleave(E_FAIL,149);

	// Break out without processing if the inputs are invalid,
	// or if there is no Form data, or if all the data has been read already
	// by previous BinaryRead requests.  We index into the read data with m_cbRead
	
	if (NULL == pvarCount || NULL == pvarReturn ||
		NULL == m_pASPState->m_pACB->pszForm ||
		m_cbRead >= m_pASPState->m_pACB->cbTotalBytes)
	{
		myretleave(S_OK,143);
	}

    // Set the variant type of the output parameter.
    V_VT(pvarReturn) = VT_ARRAY|VT_UI1;
    V_ARRAY(pvarReturn) = NULL;

    // Convert the byte count variant to a long
    if (FAILED(hr = VariantChangeTypeEx(pvarCount, pvarCount, m_pASPState->m_lcid, 0,  VT_I4)))
	{
        if ( hr == E_OUTOFMEMORY)
        {
        	ASP_ERR(IDS_E_NOMEM);
        	myleave(142);
        }
        ASP_ERR(IDS_E_PARSER);
        myleave(143);
    }

	//  pvarCount returns the # of bytes read to calling app.
    cbToRead = V_I4(pvarCount);
    V_I4(pvarCount) = 0;

    if (cbToRead == 0)
        myretleave(S_OK,0);

	if ((int) cbToRead < 0) {
		ASP_ERR(IDS_E_PARSER);
		myleave(148);
	}

    // If they've asked for more bytes then the request
    // contains, give them all the bytes in the request.
    if (cbToRead > m_pASPState->m_pACB->cbTotalBytes - m_cbRead)
        cbToRead = m_pASPState->m_pACB->cbTotalBytes - m_cbRead;

    rgsabound[0].lLbound = 0;
    rgsabound[0].cElements = cbToRead;

    // Allocate a SafeArray for the data
    V_ARRAY(pvarReturn) = SafeArrayCreate(VT_UI1, 1, rgsabound);
    if (V_ARRAY(pvarReturn) == NULL)
	{
		ASP_ERR(IDS_E_NOMEM);
		myleave(140);
	}

    if (FAILED(SafeArrayAccessData(V_ARRAY(pvarReturn), (void **) &pbData)))
		myleave(141);

	ret = ReadData(pbData,cbToRead);
	if (FAILED(ret))
		ASP_ERR(IDS_E_PARSER);

	SafeArrayUnaccessData(V_ARRAY(pvarReturn));
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: Request::BinaryRead failed with err = %d, GLE = 0x%08x\r\n"));
	if (FAILED(ret))
	    VariantClear(pvarReturn);

    return ret;
}

HRESULT CRequest::ReadData(BYTE *pv, ULONG cb) {
	PASP_CONTROL_BLOCK pACB = m_pASPState->m_pACB;
	DWORD cbToRead = cb;
	
	// caller should have performed this check before calling into us.
	DEBUGCHK(cbToRead + m_cbRead <= pACB->cbTotalBytes);

	if (cbToRead + m_cbRead > pACB->cbAvailable) {
		// We have or are about to read past the amount of POST data we've already recv'd.  Get more.
		DWORD cbOffset = 0;

		if (m_cbRead < pACB->cbAvailable) {
			// We still have some POST from initial httpd receive ready.
			cbOffset = pACB->cbAvailable - m_cbRead;
			memcpy(pv,pACB->pszForm,cbOffset);
			cbToRead -= cbOffset;
		}

		while (cbToRead) {
			DWORD cbAmountRead = cbToRead;

			if (! pACB->ReadClient(pACB->ConnID,pv+cbOffset,&cbAmountRead) || (cbAmountRead==0))
				return E_FAIL;

			cbOffset += cbAmountRead;
			cbToRead -= cbAmountRead;

			// hitting one of these indicates a .
			DEBUGCHK((int)cbToRead >= 0);
			DEBUGCHK(cbOffset <= cb);
		}

		// we've done calculations already, so if ReadClient claims success is better give
		// us exactly as many bytes as we asked for or there's a .
		DEBUGCHK(cbOffset == cb);
		m_cbRead += cbOffset;
	}
	else {
		// we have all the data we need in the buffer already.
		memcpy(pv,pACB->pszForm + m_cbRead,cbToRead);
		m_cbRead += cbToRead;
	}
	return S_OK;
}


#if defined (DEBUG)
const WCHAR *szStateArray[] = {
                            L"Available",
                            L"BinaryRead",
                            L"Request.Form",
                            L"IStream"
                             };
#endif

// Once BinaryRead, Form, or IStream has been accessed, this is only
// way that Request object may be accessed from there on.
BOOL CRequest::SetStateIfPossible(FormDataStatus newState) {
	DEBUGCHK(newState <= ISTREAMONLY);
	
	if ((m_formState != AVAILABLE) && (m_formState != newState)) {
		DEBUGMSG(ZONE_ERROR,(L"ASP: Attempting to perform a %s operation, state is currently %s and does not permit this\r\n",
		                        szStateArray[m_formState],szStateArray[newState]));

		ASP_ERR(IDS_E_PARSER);
		return FALSE;
	}

	m_formState = newState;
	return TRUE;
}


CRequest::CRequest()
{
	m_pASPState = (CASPState *) TlsGetValue(g_dwTlsSlot);
	DEBUGCHK(m_pASPState);
	
	if (NULL == m_pASPState)
		return;

	m_cbRead        = 0;
	m_formState     = AVAILABLE;
	m_fParsedPost   = FALSE;
	
	if ( c_fUseCollections )
	{
		m_pQueryString = CreateCRequestDictionary(QUERY_STRING_TYPE,m_pASPState,
							m_pASPState->m_pACB->pszQueryString);

		// we don't pass in m_pASPState->m_pACB->pszForm during this call because there's
		// no guarantee we've read in all POST data.  Only parse FORM information on 
		// first user call to Request.Form.
		m_pForm = CreateCRequestDictionary(FORM_TYPE,m_pASPState,NULL);
						
		m_pCookie= CreateCRequestDictionary(REQUEST_COOKIE_TYPE, m_pASPState,
							m_pASPState->m_pACB->pszCookie);


		m_pServerVariables = CreateCRequestDictionary(SERVER_VARIABLE_TYPE, m_pASPState,NULL);
	}
	else
	{
		m_pQueryString = NULL;
		m_pForm = NULL;
		m_pCookie = NULL;
		m_pServerVariables = NULL;
	}
}

CRequest::~CRequest()
{
	if (m_pQueryString)
		m_pQueryString->Release();

	if (m_pForm)
		m_pForm->Release();

	if (m_pCookie)
		m_pCookie->Release();

	if (m_pServerVariables)
		m_pServerVariables->Release();
}

// IStream
HRESULT STDMETHODCALLTYPE CRequest::Write(const void __RPC_FAR *pv,ULONG cb,ULONG __RPC_FAR *pcbWritten) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CRequest::Read(void __RPC_FAR *pv, ULONG cb, ULONG __RPC_FAR *pcbRead) {
	DWORD cbBytesRemainingToBeCopied = m_pASPState->m_pACB->cbTotalBytes - m_cbRead;
	DWORD cbBytesToCopy = min(cb,cbBytesRemainingToBeCopied);
	DEBUGCHK((int)cbBytesRemainingToBeCopied >= 0);

	if (!pcbRead || !pv || 0 == cb)
		return STG_E_INVALIDPOINTER;

	if (! SetStateIfPossible(ISTREAMONLY))
		return E_FAIL;

	// MSXML has a hard requirement on S_OK being returned at EOF.
	if (cbBytesRemainingToBeCopied == 0) {
		*pcbRead = 0;
		return S_OK;
	}

	if (SUCCEEDED(ReadData((BYTE*)pv,cbBytesToCopy))) {
		*pcbRead = cbBytesToCopy;
		return S_OK;
	}

	ASP_ERR(IDS_E_PARSER);
	return E_FAIL;
}

HRESULT STDMETHODCALLTYPE CRequest::Seek(LARGE_INTEGER dlibMove,DWORD dwOrigin,ULARGE_INTEGER __RPC_FAR *plibNewPosition) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CRequest::SetSize(ULARGE_INTEGER libNewSize) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CRequest::CopyTo(IStream __RPC_FAR *pstm,ULARGE_INTEGER cb,ULARGE_INTEGER __RPC_FAR *pcbRead,ULARGE_INTEGER __RPC_FAR *pcbWritten) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CRequest::Commit(DWORD grfCommitFlags) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CRequest::Revert(void) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CRequest::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,DWORD dwLockType) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CRequest::UnlockRegion(ULARGE_INTEGER libOffset,ULARGE_INTEGER cb,DWORD dwLockType) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CRequest::Stat(STATSTG __RPC_FAR *pstatstg, DWORD grfStatFlag) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CRequest::Clone(IStream __RPC_FAR *__RPC_FAR *ppstm) {
	return E_NOTIMPL;
}

