//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    SrmpFwd.cxx

Abstract:

    Forwards an SRMP data packet without modifying contents.
 
--*/


#include <windows.h>
#include <wininet.h>
#include <mq.h>

#include "sc.hxx"
#include "scsrmp.hxx"
#include "scsman.hxx"
#include "scpacket.hxx"
#include "SrmpAccept.hxx"
#include "scdefs.hxx"
#include "srmpparse.hxx"
#include "fntoken.h"
#include "scutil.hxx"
#include "scqueue.hxx"
#include "SrmpFwd.hxx"

BOOL CSrmpFwd::SetState(SRMP_TOKEN tok) {
	SRMP_STATE curState = GetState();
	SRMP_STATE newState = SRMP_ST_UNKNOWN;

	// we only care about a very small subset, no namespace checking needed if it made it this far.
	if (curState == SRMP_ST_UNINITIALIZED && tok == SRMP_TOK_ENVELOPE)
		newState = SRMP_ST_ENVELOPE;
	else if (curState == SRMP_ST_ENVELOPE && tok == SRMP_TOK_HEADER)
		newState = SRMP_ST_HEADER;
	else if (curState == SRMP_ST_HEADER && tok == SRMP_TOK_PATH)
		newState = SRMP_ST_PATH;
	else if (curState == SRMP_ST_PATH && tok == SRMP_TOK_FWD)
		newState = SRMP_ST_PATH_FWD;
	else if (curState == SRMP_ST_PATH_FWD && tok == SRMP_TOK_VIA)
		newState = SRMP_ST_PATH_FWD_VIA;
	else if (curState == SRMP_ST_PATH && tok == SRMP_TOK_REV)
		newState = SRMP_ST_PATH_REV;
	else if (curState == SRMP_ST_PATH_REV && tok == SRMP_TOK_VIA)
		newState = SRMP_ST_PATH_REV_VIA;

	if (! m_State.Push((void*)newState))
		return FALSE;

	return TRUE;
}


HRESULT STDMETHODCALLTYPE CSrmpFwd::endElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName)
{
	SRMP_STATE oldState = GetState();
	HRESULT hr = E_FAIL;

	m_State.Pop();
	if (fSkipOutput) {
		fSkipOutput = FALSE;
		return S_OK;
	}

	// <rev> is present but is empty (i.e. <rev/>), then add <via>
	if (oldState == SRMP_ST_PATH_REV && !fFoundRevVia) {
		fFoundRevVia = TRUE;
		if (S_OK != characters(0,0) ||
		    FAILED(hr = WriteRevEntry(TRUE)))
			return hr;
	}

	/* If we ever break this code into public or have some other mechanism of injecting
	// extra XML tags into a forwarded envelope, now would be the time to do it.
	if (oldState == SRMP_ST_ENVELOPE) {
		// We're about to close </envelope>.  User may append whatever extra XML tags
		// they desire now.
	}
	*/
	return writeBuffer.endElement(pwchNamespaceUri,cchNamespaceUri,pwchLocalName,cchLocalName,pwchQName,cchQName);
}

void GetHttpPrefix(BOOL fSecure, WCHAR **ppszPrefix, DWORD *pccPrefix) {
	if (fSecure) {
		*ppszPrefix = (WCHAR*) cszHttpsPrefix;
		*pccPrefix  = ccHttpsPrefix;
	}
	else {
		*ppszPrefix = (WCHAR*) cszHttpPrefix;
		*pccPrefix  = ccHttpPrefix;
	}
}

HRESULT STDMETHODCALLTYPE CSrmpFwd::characters( 
    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
	if (! fSkipOutput)
		return writeBuffer.characters(pwchChars,cchChars);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CSrmpFwd::startElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes __RPC_FAR *pAttributes)
{
	SVSUTIL_ASSERT(!fSkipOutput);

	SRMP_TOKEN token = GetToken(pwchLocalName,cchLocalName);
	HRESULT    hr;

	if (! SetState(token))
		return E_OUTOFMEMORY;

	SRMP_STATE curState = GetState();

	switch (curState) {
		case SRMP_ST_PATH_FWD_VIA:
			if (fwdViaState == SRMP_FWD_ONFIRST) {
				// swallow first <fwd><via> element (points to this CE server), allow all others to pass through.
				fwdViaState = SRMP_FWD_ONSECOND;
				fSkipOutput = TRUE;
				return S_OK;
			}
			else if (fwdViaState == SRMP_FWD_ONSECOND) {
				fwdViaState = SRMP_FWD_PASTSECOND;
				// the second <fwd><via> entry was used to setup the queue that we're sending to currently (will be first in this forwarded packet).
				// Use szQueueName rather than the original entry in case the original entry has been routed to a new destination.

				gMem->Lock();

				WCHAR *szHttpPref;
				DWORD ccHttpPref;
				GetHttpPrefix(pSession->IsSecure(),&szHttpPref,&ccHttpPref);

				// 1st call is to characters() due to way MXXMLWriter works; needs to possibly fixup element immediatly preceeding this one in buffer.
				if (S_OK != characters(0,0) ||
				    ! writeBuffer.StartTag(cszVia) ||
				    ! writeBuffer.AppendWSTR(szHttpPref,ccHttpPref) || 
				    ! writeBuffer.AppendWSTR(pSession->lpszHostName,wcslen(pSession->lpszHostName)) ||
				    ! writeBuffer.AppendWSTR(L"/",1) ||
				    ! writeBuffer.AppendWSTR(szQueueName,wcslen(szQueueName)) ||
				    ! writeBuffer.EndTag(cszVia)) {
				    gMem->Unlock();
				    return E_OUTOFMEMORY;
				}

				fSkipOutput = TRUE;
				gMem->Unlock();
				return S_OK;
			}
		break;

		case SRMP_ST_PATH_REV_VIA:
			// put name of CE box as 1st via entry
			if (!fFoundRevVia) {
				fFoundRevVia = TRUE;

				if ((FAILED(hr = writeBuffer.startElement(pwchNamespaceUri,cchNamespaceUri,pwchLocalName,cchLocalName,pwchQName,cchQName,pAttributes))))
					return hr;

				if (S_OK != characters(0,0))
					return E_OUTOFMEMORY;

				if (FAILED(hr = WriteRevEntry(FALSE)))
					return hr;

				if (FAILED(hr = writeBuffer.endElement(pwchNamespaceUri,cchNamespaceUri,pwchLocalName,cchLocalName,pwchQName,cchQName)))
					return hr;
				// "fall through" - add <rev><via> entry SAX has come across just like normal.
			}

		// fall through
		default:
			fSkipOutput = FALSE;
	}

	return writeBuffer.startElement(pwchNamespaceUri,cchNamespaceUri,pwchLocalName,cchLocalName,pwchQName,cchQName,pAttributes);
}

HRESULT CSrmpFwd::WriteRevEntry(BOOL fWriteRevTag) {
	if (fWriteRevTag) {
		if (! writeBuffer.StartTag(cszVia))
			return E_OUTOFMEMORY;
	}

	if (! writeBuffer.Encode(szRevEntry))
		return E_OUTOFMEMORY;

	if (fWriteRevTag) {
		if ( ! writeBuffer.EndTag(cszVia))
			return E_OUTOFMEMORY;
	}
	return S_OK;
}

const char cszHostHttpHeader[] = "Host:";
const char cszTextXML[]        = "text/xml";

BOOL SkipHeader(PSTR szHeader) {
	if (0 == _strnicmp(szHeader,cszHostHttpHeader,SVSUTIL_CONSTSTRLEN(cszHostHttpHeader)) ||
	    0 == _strnicmp(szHeader,cszContentLength, ccContentLength))
		return TRUE;
	return FALSE;
}

int ScSession::ForwardSrmpMessage(CSrmpFwd *pSrmpFwd, ScPacketImage *pPacketImage, WCHAR *wszURL) {
	CUserHeader            *pUserHeader             = pPacketImage->sect.pUserHeader;
	CCompoundMessageHeader *pCompoundMessageHeader  = pPacketImage->sect.pCompoundMessageHeader;
	CPropertyHeader        *pPropHeader             = pPacketImage->sect.pPropHeader;
	CSrmpEnvelopeHeader    *pSrmpEnvelopeHeader     = pPacketImage->sect.pSrmpEnvelopeHeader;
	int iStatusCode = 503;
	CHAR *szSoapStart = NULL;
	CHAR *szSoapEnd   = NULL;

	// note: if I don't have pCompoundMessageHeader (ie user generated fwd) need to do something more intelligent!
	const char *szOrigHttpHeaders = (CHAR*) pCompoundMessageHeader->GetPointerToData();
	CHAR  *pszRead;
	CHAR  *pszWrite;
	DWORD ccHeaderBuf    = 0;
	DWORD ccBodyBuf      = 0;
	SrmpIOCTLPacket ioPacket;
	ISAXXMLReader*  pReader = NULL;
	VARIANT varInput;
	BOOL fSecure = (qType == SCFILE_QP_FORMAT_HTTPS);
	CHAR  cSave;
	CHAR *szURL = NULL;
	DWORD cbMime = 0;
	DWORD cbSoapEnv;
	DWORD cp = CP_UTF8;
	CHAR *szBoundary = NULL;
	CONTENT_TYPE contentType = CONTENT_TYPE_UNKNOWN;
	HRESULT hr;
	BOOL    fCoUninit = FALSE;

	memset(&ioPacket,0,sizeof(ioPacket));
	memset(&varInput,0,sizeof(varInput));

	SVSUTIL_ASSERT(pSrmpEnvelopeHeader && pSrmpEnvelopeHeader->GetData());

	//
	// Build HTTP headers up based on original values.
	//
	CHAR *szHeaderEnd  = strstr(szOrigHttpHeaders,cszCRLF2);
	if (!szHeaderEnd) {
		SVSUTIL_ASSERT(0); // message won't be stored unless in valid format.
		goto done; 
	}

	if (NULL == (ioPacket.pszHeaders = (CHAR*) g_funcAlloc((szHeaderEnd - szOrigHttpHeaders),g_pvAllocData)))
		goto done;

	pszWrite = ioPacket.pszHeaders;
	pszRead  = (CHAR*)szOrigHttpHeaders;

	// copy headers to send straight along to next hop, except excluded ones.
	while (pszRead < szHeaderEnd) {
		if (!SkipHeader(pszRead)) {
			// copy header
			CHAR *pszReadEnd = strstr(pszRead,"\r\n") + 2;
			memcpy(pszWrite,pszRead,pszReadEnd-pszRead);
			pszWrite += (pszReadEnd - pszRead);

			// look for content-type to determine how to handle next stage.
			if (0 == _strnicmp(pszRead,cszContentType, ccContentType)) {
				CHAR *szContentType = pszRead + ccContentType + 1;
				while ((*szContentType == ' ') || (*szContentType == '\t'))
					szContentType++;

				if (0 == _strnicmp(szContentType,cszTextXML,SVSUTIL_CONSTSTRLEN(cszTextXML)))
					contentType = CONTENT_TYPE_XML;
				else {
#if defined (DEBUG) || defined (_DEBUG)
					// our HTTP processing in SrmpIsapi is only capable of handeling "text/xml" and "multipart/related"; 
					// any errors should've been caught long before we reach this stage.
					static const char cszMime[] = "multipart/related";
					SVSUTIL_ASSERT(0 == _strnicmp(szContentType,cszMime,SVSUTIL_CONSTSTRLEN(cszMime)));
#endif
					contentType = CONTENT_TYPE_MIME;
				}
			}
			pszRead = pszReadEnd;
		}
		else {
			pszRead = strstr(pszRead,"\r\n") + 2;
		}
	}
	SVSUTIL_ASSERT(contentType != CONTENT_TYPE_UNKNOWN);
	ioPacket.cbHeaders = pszWrite - ioPacket.pszHeaders;

	//
	// Build POST body.  Keep everything the same except for updating the 
	//  <fwd> and <rev> fields, as appropriate.
	//
	if (contentType == CONTENT_TYPE_MIME) {
		if (NULL == (szBoundary = pSrmpFwd->SetupMIMEBuffer(pPacketImage,&szSoapStart,&szSoapEnd,&cbMime)))
			goto done;

		// make a temporary string.
		cSave = *szSoapEnd;
		*szSoapEnd = 0;
	}
	else {
		// text/xml begins immediatly in POST data.
		CHAR *szHttpData = (CHAR*) pCompoundMessageHeader->GetPointerToData();
		if (NULL == (szSoapStart = strstr(szHttpData,cszCRLF2))) {
			SVSUTIL_ASSERT(0);
			goto done;
		}
	}

	// Initialize SAX
	if (FAILED(CoInitializeEx(NULL,COINIT_MULTITHREADED)))
		goto done;

	fCoUninit = TRUE;

	if (FAILED(CoCreateInstance(CLSID_SAXXMLReader, NULL, CLSCTX_INPROC_SERVER, IID_ISAXXMLReader, (void**)&pReader)))
		goto done;

	if (FAILED(pReader->putContentHandler(pSrmpFwd))) 
		goto done;

	memset(&varInput,0,sizeof(varInput));
	varInput.vt          = VT_BSTR;
	varInput.bstrVal     = (BSTR) svsutil_MultiByteToWideChar(szSoapStart,CP_UTF8,TRUE);

	if (!varInput.bstrVal)
		goto done;

	if (szSoapEnd) // reset the string.
		*szSoapEnd = cSave;

	if (FAILED(hr = pReader->parse(varInput))) {
		SVSUTIL_ASSERT(0);
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: SAX Parse error on attempt to forward message, error=0x%08x\r\n",hr));
		goto done;
	}

	SVSUTIL_ASSERT(S_OK == svsutil_validateXML((BSTR)pSrmpFwd->writeBuffer.pBuffer));
	
	if (0 == (cbSoapEnv = WideCharToMultiByte(cp, 0, (WCHAR*)pSrmpFwd->writeBuffer.pBuffer, -1, NULL, 0, 0, 0))) {
		cp = CP_ACP;
		if (0 == (cbSoapEnv = WideCharToMultiByte(cp, 0, (WCHAR*)pSrmpFwd->writeBuffer.pBuffer, -1, NULL, 0, 0, 0)))
			goto done;
	}

	// +1024 to contain the initial MIME headers.
	if (NULL == (ioPacket.pszPost = (CHAR*)g_funcAlloc(cbMime+cbSoapEnv+1024,g_pvAllocData)))
		goto done;

	if (NULL == (szURL = AllocURL(wszURL)))
		goto done;

	// write out MIME information for SOAP envelope.
	// use (cbSoapEnv-1) because we don't write out NULL character in pSrmpFwd->writeBuffer.pBuffer.
	if (contentType == CONTENT_TYPE_MIME) {
		pszWrite = ioPacket.pszPost + sprintf(ioPacket.pszPost,BOUNDARY_HYPHEN "%s\r\n"
		                   "Content-Type: %s; charset=UTF-8\r\nContent-Length: %d\r\n\r\n",
		                   szBoundary,cszTextXML,cbSoapEnv-1);

		WideCharToMultiByte(cp, 0, (WCHAR*)pSrmpFwd->writeBuffer.pBuffer, -1, pszWrite, cbSoapEnv, 0, 0);
		memcpy(pszWrite + cbSoapEnv-1,szSoapEnd,cbMime);
		ioPacket.cbPost = (pszWrite + cbSoapEnv - 1 + cbMime - ioPacket.pszPost);
	}
	else {
		// simple "text/xml" case.
		WideCharToMultiByte(cp, 0, (WCHAR*)pSrmpFwd->writeBuffer.pBuffer, -1, ioPacket.pszPost, cbSoapEnv, 0, 0);
		ioPacket.cbPost = cbSoapEnv;
	}

	iStatusCode = SendHttpMsg(szURL,fSecure,&ioPacket);
done:
	if (pReader)
		pReader->Release();

	if (ioPacket.pszHeaders)
		g_funcFree(ioPacket.pszHeaders,g_pvFreeData);

	if (varInput.bstrVal)
		g_funcFree(varInput.bstrVal,g_pvFreeData);

	if (ioPacket.pszPost)
		g_funcFree(ioPacket.pszPost,g_pvFreeData);

	if (szBoundary)
		g_funcFree(szBoundary,g_pvFreeData);

	if (szURL)
		g_funcFree(szURL,g_pvFreeData);

	if (fCoUninit)
		CoUninitialize();

	return iStatusCode;
}

CHAR* CSrmpFwd::SetupMIMEBuffer(ScPacketImage *pPacketImage, CHAR **ppszSoapStart, CHAR **ppszSoapEnd, DWORD *pcbMime) {
	CSrmpEnvelopeHeader    *pSrmpEnvelopeHeader    = pPacketImage->sect.pSrmpEnvelopeHeader;
	CCompoundMessageHeader *pCompoundMessageHeader = pPacketImage->sect.pCompoundMessageHeader;

	CHAR *szHttpHeaders = (CHAR*) pCompoundMessageHeader->GetPointerToData();
	CHAR *szPost;
	CHAR *szBoundary = NULL;
	DWORD cbPost;
	BOOL  fRet = FALSE;

	// allocate an large enough buffer in hopes we can save a realloc.
	DWORD cbAlloc = (wcslen(szQueueName)+pSrmpEnvelopeHeader->GetDataLengthInWCHARs()+1024)*sizeof(WCHAR);
	if (! writeBuffer.AllocMem(cbAlloc))
		goto done;

	szPost = strstr(szHttpHeaders,cszCRLF2);
	if (!szPost) {
		SVSUTIL_ASSERT(0); // shouldn't have made it this far with corrupt data
		goto done;
	}
	szPost = szPost + ccCRLF2;

	if (! pCompoundMessageHeader->GetBodySizeInBytes()) {
		// simple SOAP envelope (no MIME body).  Should've had "content-type: text/xml" set already.
		SVSUTIL_ASSERT(0);
		goto done;
	}

	// MIME message.  Since we're forwarding directly we want to perserve 
	// all the original information except SOAP headers, which are modified.
	szBoundary = FindMimeBoundary(szHttpHeaders);
	if (!szBoundary)
		goto done;

	cbPost = pCompoundMessageHeader->GetDataSizeInBytes() - (szPost - szHttpHeaders);
	if (! ReadSoapEnvelopeFromMIME(szBoundary,strlen(szBoundary),szPost,cbPost,ppszSoapStart,ppszSoapEnd)) {
		SVSUTIL_ASSERT(0);
		goto done;
	}
	*pcbMime = cbPost - (*ppszSoapEnd-szPost);
	
	fRet = TRUE;
done:
	if (!fRet && szBoundary) {
		g_funcFree(szBoundary,g_pvFreeData);
		return NULL;
	}

	return szBoundary;
}
