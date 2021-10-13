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

    SrmpAccept.cxx

Abstract:

    Processes SRMP messages from an HTTP request and puts them in an MSMQ queue.
 
--*/


#include <windows.h>
#include <mq.h>


#include "sc.hxx"
#include "scsrmp.hxx"
#include "scpacket.hxx"
#include "SrmpAccept.hxx"
#include "scdefs.hxx"
#include "srmpparse.hxx"
#include "scsman.hxx"
#include "scutil.hxx"
#include <creg.hxx>
#include <wininet.h>
#include <service.h>

const BOOL g_fHaveSRMP = TRUE;

//
//  Exposed functions
//
DWORD WINAPI ScSession::ServiceThreadHttpW_s(void *arg) {
	ScSession        *pSess    = (ScSession *)arg;
	pSess->ServiceThreadHttpW ();
	return 0;
}

void ScSession::SRMPCloseSession(void) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	if (hRequest) {
		InternetCloseHandle(hRequest);
		hRequest = 0;
	}

	if (hInternetConnect) {
		InternetCloseHandle(hInternetConnect);
		hInternetConnect = 0;
	}

	if (hInternetSession) {
		InternetCloseHandle(hInternetSession);
		hInternetSession = 0;
	}
}

void ReadMSMQVRoots(void) {
	// determine what VRoot map to MSMQ is based on where SrmpIsapi.dll is mapped to in HTTPD vroot table.
	CReg  reg;
	CReg  subReg;
	WCHAR szBuf[MAX_PATH+1];
	WCHAR szURL[MAX_PATH+3];
	int   i = 0;

	SVSUTIL_ASSERT(!gMachine->VRootList[0].wszVRoot);

	if (! reg.Open(HKEY_LOCAL_MACHINE,MSMQ_SC_HTTPD_VROOT_REG_KEY))
		return;

	// walk through VROOTS looking for first mappings to \srmpIsapi.dll.
	// We allow multiple VRoots to be mapped to SRMPISAPI to allow for different queue names
	// (i.e. /Msmq/Private$/Foo and /MyMsmq/Private$/Foo can both both map to queue
	// Private$/Foo if webserver VRoot table is setup right.)

	while (reg.EnumKey(szURL, SVSUTIL_ARRLEN(szURL)-2)) { // -1 saves a place to append a char
		if (subReg.Open(reg,szURL) && subReg.ValueSZ(NULL,szBuf,SVSUTIL_ARRLEN(szBuf)) &&
			scutil_StrStrWI(szBuf,MSMQ_SC_HTTPD_ISAPI_NAME)) {

			// tack '/' to end if not there already.
			DWORD ccURL = wcslen(szURL);
			if (szURL[ccURL-1] != '/') {
				szURL[ccURL++] = '/';
				szURL[ccURL] = 0;
			}

			if (NULL == (gMachine->VRootList[i].wszVRoot = svsutil_wcsdup(szURL)))
				return;

			gMachine->VRootList[i].ccVRoot  = ccURL;
			i++;

			if (i >= MAX_VROOTS)
				return;
		}
		subReg.Reset();
	}
}

void HttpdStart(void) {
	HANDLE  hFile = CreateFile(L"HTP0:", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (INVALID_HANDLE_VALUE == hFile)
		return;

	DeviceIoControl(hFile, IOCTL_SERVICE_START, NULL, 0, NULL ,0, NULL, NULL);
	CloseHandle(hFile);
}



// true = wininet is in image, false = using httplite
BOOL g_fUseWininet;
void SrmpInit(void) {
	ReadMSMQVRoots();
	HttpdStart();

	// if wininet is in the image we can do keep alives, httplite does not support keep-alives.
	// full Wininet supports INTERNET_OPEN_TYPE_PRECONFIG_WITH_NO_AUTOPROXY, httplite does not, easy check.
	HANDLE hInternet = InternetOpenA(NULL,INTERNET_OPEN_TYPE_PRECONFIG_WITH_NO_AUTOPROXY,0,0,0);
	if (hInternet) {
		InternetCloseHandle(hInternet);
		g_fUseWininet = TRUE;
	}
	else
		g_fUseWininet = FALSE;
}

void SrmpAcceptHttpRequest(PSrmpIOCTLPacket pSrmpPacket, DWORD *pdwHttpStatus) {
	ScPacket *pPacket = NULL;
	CSrmpToMsmq cSrmpToMsmq(pSrmpPacket);
	BOOL fDecrementBusy = FALSE;

	// check fApiInitialized to verify gMem is valid.  If it is 
	if (!fApiInitialized) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: MSMQ not active, failing HTTP request\r\n"));
		cSrmpToMsmq.SetUnavailable();
		goto done;
	}

	gMem->Lock();
	if ((glServiceState != SERVICE_STATE_ON) || (! gMachine) || (! gMachine->fUseSRMP)) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: MSMQ not active, failing HTTP request\r\n"));
		cSrmpToMsmq.SetUnavailable();
		gMem->Unlock();
		goto done;
	}

	fDecrementBusy = TRUE;
	++gSessionMan->fBusy;
	gMem->Unlock();

	if (! cSrmpToMsmq.IsInitialized())
		goto done;

	if (! cSrmpToMsmq.ParseMimeType())
		goto done;

	if (! cSrmpToMsmq.ParseEnvelopeAndAttachments())
		goto done;

	cSrmpToMsmq.CreateAndInsertPacket();
done:
	if (fDecrementBusy && fApiInitialized) {
		gMem->Lock ();
		--gSessionMan->fBusy;
		gMem->Unlock ();
	}

	*pdwHttpStatus = cSrmpToMsmq.GetStatusCode();
	return;
}

//
// Parse the HTTP request and hand it off to SAX parser.
//
PSTR ReadBoundaryDelimiter(PSTR pszBody, PSTR pszBoundary, DWORD ccBoundary, BOOL *pfAtEnd);

BOOL ReadSoapEnvelopeFromMIME(PSTR pszBoundary, DWORD ccBoundary, PSTR pszPost, DWORD cbPost, PSTR *ppszStartEnv, PSTR *ppszEndEnv) {
	PSTR  pszEndHeader;
	PSTR  pszContentLength;
	DWORD cbEnvelopeSize;
	PSTR  pszEndHttpBody = pszPost + cbPost;
	DWORD cbMimeHeader;

	pszEndHeader = strstr(pszPost,cszCRLF2);
	if (!pszEndHeader) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: Message contains ill-formatted Mime data, failing request\r\n"));
		return FALSE;
	}
	pszEndHeader += ccCRLF2;
	cbMimeHeader = pszEndHeader - pszPost;

	pszContentLength = FindHeader(pszPost,cszContentLength);

	// make sure content-length we found is within MIME header
	if ((NULL==pszContentLength) || (pszEndHeader < pszContentLength)) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: Message contains ill-formatted Mime data, failing request\r\n"));
		return FALSE;
	}

	if (0 == (cbEnvelopeSize = atoi(pszContentLength))) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: Message contains ill-formatted Mime data (content-length invalid), failing request\r\n"));
		return FALSE;
	}

	*ppszStartEnv =  pszPost + cbMimeHeader;
	*ppszEndEnv   =  *ppszStartEnv + cbEnvelopeSize;
	if (*ppszEndEnv > pszEndHttpBody) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: Message contains ill-formatted Mime data, failing request\r\n"));
		return FALSE;
	}

	return TRUE;
}

BOOL CSrmpToMsmq::ConvertMIME(void) {
	PSTR    pszBoundary = NULL;
	DWORD   ccBound;
	PSTR    pszTrav;
	PSTR    pszStartEnv;
	PSTR    pszEndEnv;
	BOOL    fAtEnd = FALSE;
	BOOL    fRet   = FALSE;
	CHAR    cSave;
	BOOL    fBadRequest;

	if (NULL == (pszBoundary = FindMimeBoundary(cMsgProps.pHttpParams->pszHeaders,&fBadRequest))) {
		if (fBadRequest) {
			scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: Message contains ill-formatted Mime data (can't find mimeboundary), failing request\r\n"));
			SetBadRequest();
		}
		goto done;
	}

	ccBound = strlen(pszBoundary);
	if (! ReadSoapEnvelopeFromMIME(pszBoundary,ccBound,cMsgProps.pHttpParams->pszPost,cMsgProps.pHttpParams->cbPost,&pszStartEnv,&pszEndEnv)) {
		SetBadRequest();
		goto done;
	}

	// MIME information begins after the SOAP envelope.
	pszTrav = pszEndEnv;
	while (1) {
		if (NULL == ReadBoundaryDelimiter(pszTrav, pszBoundary,ccBound,&fAtEnd)) {
			scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: Message contains ill-formatted Mime (problems with boundary delimiters), failing request\r\n"));
			SetBadRequest();
			goto done;
		}

		if (fAtEnd)
			break;

		if (NULL == (pszTrav = ReadMIMESection(pszTrav)))
			goto done;
	}

	cSave = *pszEndEnv;
	*pszEndEnv = 0; // bstrEnvelope should only contain <envelope> data

	if (! UTF8ToBSTR(pszStartEnv,&cMsgProps.bstrEnvelope))
		goto done;

	*pszEndEnv = cSave;
	fRet = TRUE;
done:
	if (pszBoundary)
		g_funcFree(pszBoundary, g_pvFreeData);

	return fRet;
}


BOOL CSrmpToMsmq::ParseEnvelopeAndAttachments(void) {
	HRESULT hr = E_FAIL;
	VARIANT varInput;
	BOOL    fCoUnInit = FALSE;

	ISAXXMLReader*  pReader = NULL;

	if (FAILED(hr = CoInitializeEx(NULL,COINIT_MULTITHREADED))) {
		scerror_DebugOutM (VERBOSE_MASK_SRMP, (L"SRMP: CoInitialize() failed, error=0x%08x",hr));
		goto done;
	}
	fCoUnInit = TRUE;

	if (FAILED(hr = CoCreateInstance(CLSID_SAXXMLReader, NULL, CLSCTX_INPROC_SERVER, IID_ISAXXMLReader, (void**)&pReader))) {
		scerror_DebugOutM (VERBOSE_MASK_SRMP, (L"SRMP: CoCreateInstance(CLSID_SAXXMLReader) failed, error=0x%08x\r\n",hr));
		goto done;
	}

	if (FAILED(hr = pReader->putContentHandler(this))) {
		scerror_DebugOutM (VERBOSE_MASK_SRMP, (L"SRMP: ISAXXMLReader::putContentHandler failed, error=0x%08x\r\n",hr));
		goto done;
	}
	SVSUTIL_ASSERT(cMsgProps.bstrEnvelope);

	memset(&varInput,0,sizeof(varInput));
	varInput.vt          = VT_BSTR;
	varInput.bstrVal     = cMsgProps.bstrEnvelope;
	cMsgProps.ccEnvelope = wcslen(cMsgProps.bstrEnvelope);

	// set error=to "400 bad request" temporarily.  If SAX fails on invalid XML 
	// then we've been sent corrutped data and this is correct err code.  If 
	// there's a failure during processing that's temporary (memory not allocating) 
	// then we'll reset err code with SetInternalError() during SAX processing.
	SetBadRequest();
	if (FAILED(hr = pReader->parse(varInput))) {
		scerror_DebugOutM (VERBOSE_MASK_SRMP, (L"SRMP: ISAXXMLReader::parse().  Error = 0x%08x\r\n",hr));
		goto done;
	}
	SetInternalError();

	hr = S_OK;
done:
	if (pReader)
		pReader->Release();

	if (fCoUnInit)
		CoUninitialize();

	return (hr == S_OK);
}


// 
// Utility functions for parsing raw MIME and XML data
//
PSTR CSrmpToMsmq::ReadMIMESection(PSTR pszMime) {
	PSTR   pszHeaderEnd;
	PSTR   pszContentId;
	PSTR   pszContentLength;
	DWORD  cbContentLength;
	DWORD  dwStatus = 400; // bad request

	pszHeaderEnd = strstr(pszMime,cszCRLF2);
	if (!pszHeaderEnd || pszHeaderEnd >= cMsgProps.pHttpParams->pszPost + cMsgProps.pHttpParams->cbPost)
		goto error;

	if (NULL == (pszContentId = FindHeader(pszMime,cszContentId)))
		goto error;

	pszContentId = RemoveLeadingSpace(pszContentId,pszHeaderEnd);

	if ((NULL == (pszContentLength = FindHeader(pszMime,cszContentLength))) ||
	    (0 == (cbContentLength = atoi(pszContentLength))))
		goto error;

	if (pszHeaderEnd + cbContentLength > cMsgProps.pHttpParams->pszPost + cMsgProps.pHttpParams->cbPost)
		goto error;

	if (!cMsgProps.cMime.CreateNewAttachment(pszContentId,cbContentLength,pszHeaderEnd+ccCRLF2)) {
		dwStatus = 500;  // couldn't allocate
		goto error;
	}

	return pszHeaderEnd + cbContentLength + ccCRLF2;
error:
	SetStatusCode(dwStatus);
	return NULL;
}


PSTR ReadBoundaryDelimiter(PSTR pszBody, PSTR pszBoundary, DWORD ccBound, BOOL *pfAtEnd) {
	if (strncmp(pszBody,cszBoundaryHyphen,ccBoundaryHyphen))
		return NULL;

	pszBody += ccBoundaryHyphen;

	if (strncmp(pszBody,pszBoundary,ccBound))
		return NULL;

	pszBody += ccBound;

	if (0 == strncmp(pszBody,cszBoundaryHyphen,ccBoundaryHyphen)) {
		*pfAtEnd = TRUE;
		pszBody += ccBoundaryHyphen;
	}

	pszBody = RemoveLeadingSpace(pszBody,pszBody + strlen(pszBody));
	return pszBody;
}


//
// ISAXContentHandler
//
HRESULT STDMETHODCALLTYPE CSrmpToMsmq::startElement(const wchar_t * pwchNamespaceUri, int cchNamespaceUri, const wchar_t * pwchLocalName, 
                         int cchLocalName, const wchar_t * pwchQName, int cchQName, ISAXAttributes * pAttributes) {
	SRMP_TOKEN token;

	// If current state is reading characters, no other elements may come in the middle
	// of it, as per the way the spec works out (ie no complex elemenst like <foo>Data<bar>More data</bar></foo>.
	// If we have this situation it means something is formatted incorrectly, so fail.
	if (! (GetStateFlags() & SRMP_FLAG_IGNORE_CHARS)) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: Invalid XML in message.  An element that was supposed to contain no child elements contains child element <<%s>>\r\n",pwchLocalName));
		return E_FAIL;
	}
	
	token = GetToken(pwchLocalName,cchLocalName);
	m_Chars.Reset();

	if (! SetState(token,pwchNamespaceUri,cchNamespaceUri)) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: Unable to transition to state (caused by mem alloc failue or by invalid name space)\r\n"));
		return E_FAIL;
	}

	if (GetState() == SRMP_ST_UNKNOWN) {
		if (MustUnderstand(pAttributes)) {
			scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: cannot understand SRMP packet type %s and it is marked MustUnderstand, failing request\r\n",pwchLocalName));
			return E_FAIL;
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CSrmpToMsmq::endElement(const wchar_t  *pwchNamespaceUri, int cchNamespaceUri, const wchar_t  *pwchLocalName,
	                                             int cchLocalName, const wchar_t  *pwchQName, int cchQName) {
	SRMP_STATE endState = GetState();
	HRESULT hr = S_OK;

	if (! m_Chars.Append((char*)L"\0",sizeof(WCHAR)))
		return E_FAIL;

	switch (SrmpType(endState)) {
		case SRMP_ST_PATH:
			hr = EndPath(endState);
		break;

		case SRMP_ST_PROPERTIES:
			hr = EndProperties(endState);
		break;
		
		case SRMP_ST_SERVICES:
			hr = EndServices(endState);
		break;

		case SRMP_ST_STREAM:
			hr = EndStream(endState);
		break;

		case SRMP_ST_STREAMRCPT :
			hr = EndStreamRcpt(endState);
		break;

		case SRMP_ST_DELIVERYRCPT:
			hr = EndDeliveryRcpt(endState);
		break;

		case SRMP_ST_COMMITMENTRCPT:
			hr = EndCommitmentRcpt(endState);
		break;

		case SRMP_ST_MSMQ:
			hr = EndMsmq(endState);
		break;

		case SRMP_ST_SIGNATURE:
			hr = EndSignature(endState);
		break;

		default:
		break;
	}
	PopState();

#if defined (SC_VERBOSE)
	if (FAILED(hr))
		scerror_DebugOut(VERBOSE_MASK_SRMP,L"SRMP: failed to parse element tag %s, unable to process request\r\n",pwchLocalName);
#endif

	return hr;
}

HRESULT STDMETHODCALLTYPE CSrmpToMsmq::characters(const wchar_t  *pwchChars, int cchChars) {
	if (GetStateFlags() & SRMP_FLAG_IGNORE_CHARS)
		return S_OK;

	if (m_Chars.Append((PSTR) pwchChars, cchChars*sizeof(WCHAR)))
		return S_OK;

	SetInternalError(); // resets to 500 rather than 400, since we're failing not on bad data but due to a internal server error (no memory).
	return E_FAIL;
}

//
//  SAX Utility functions.
//

const WCHAR cszMustUnderstand[] = L"mustUnderstand";

BOOL CSrmpToMsmq::MustUnderstand(ISAXAttributes *pAttributes) {
	int   iNumElements;
	const WCHAR * szDummy;
	const WCHAR * szValue;
	int   iLen, iDummy;

	if (!pAttributes)
		return FALSE;

	if (FAILED(pAttributes->getLength(&iNumElements)))
		return FALSE;

	for (int i=0; i < iNumElements; i++) {
		if (FAILED(pAttributes->getName (i,&szDummy,&iDummy,&szValue,&iLen,&szDummy,&iDummy)))
			continue;

		if (!TokenEqual(szValue,cszMustUnderstand,iLen))
			continue;

		if (FAILED(pAttributes->getValue(i,&szValue,&iLen)))
			return TRUE;  // if it's not set correctly, assume the worst and that we must understand it.

		return (! (iLen == 1 && szValue[0] == '0'));
	}
	return FALSE;
}

//
// Convert data.
//

typedef long (WINAPI *PFN_END)(CSrmpToMsmq *pThis);

HRESULT WINAPI EndPath(CSrmpToMsmq *pThis) { return pThis->HandlePath(); }
HRESULT WINAPI EndPathId(CSrmpToMsmq *pThis) { return pThis->HandlePathId(); }
HRESULT WINAPI EndPathTo(CSrmpToMsmq *pThis) { return pThis->HandlePathTo(); }
HRESULT WINAPI EndPathRev(CSrmpToMsmq *pThis) { return pThis->HandlePathRev(); }
HRESULT WINAPI EndPathRevVia(CSrmpToMsmq *pThis) { return pThis->HandlePathRevVia(); }
HRESULT WINAPI EndPathFrom(CSrmpToMsmq *pThis) { return pThis->HandlePathFrom(); }
HRESULT WINAPI EndPathAction(CSrmpToMsmq *pThis) { return pThis->HandlePathAction(); }
HRESULT WINAPI EndPathRelatesTo(CSrmpToMsmq *pThis) { return pThis->HandlePathRelatesTo(); }
HRESULT WINAPI EndPathFixed(CSrmpToMsmq *pThis) { return pThis->HandlePathFixed(); }
HRESULT WINAPI EndPathFwd(CSrmpToMsmq *pThis) { return pThis->HandlePathFwd(); }
HRESULT WINAPI EndPathFault(CSrmpToMsmq *pThis) { return pThis->HandlePathFault(); }
HRESULT WINAPI EndPathFwdVia(CSrmpToMsmq *pThis) { return pThis->HandlePathFwdVia(); }

HRESULT WINAPI EndServices(CSrmpToMsmq *pThis) { return pThis->HandleServices(); }
HRESULT WINAPI EndServicesDurable(CSrmpToMsmq *pThis) { return pThis->HandleServicesDurable(); }
HRESULT WINAPI EndServicesDelRcptRequest(CSrmpToMsmq *pThis) { return pThis->HandleServicesDelRcptRequest(); }
HRESULT WINAPI EndServicesDelRcptRequestSendTo(CSrmpToMsmq *pThis) { return pThis->HandleServicesDelRcptRequestSendTo(); }
HRESULT WINAPI EndServicesDelRcptRequestSendBy(CSrmpToMsmq *pThis) { return pThis->HandleServicesDelRcptRequestSendBy(); }
HRESULT WINAPI EndServicesFilterDups(CSrmpToMsmq *pThis) { return pThis->HandleServicesFilterDups(); }
HRESULT WINAPI EndServicesCommitRcpt(CSrmpToMsmq *pThis) { return pThis->HandleServicesCommitRcpt(); }
HRESULT WINAPI EndServicesCommitRcptPositevOnly(CSrmpToMsmq *pThis) { return pThis->HandleServicesCommitRcptPositevOnly(); }
HRESULT WINAPI EndServicesCommitRcptNegativeOnly(CSrmpToMsmq *pThis) { return pThis->HandleServicesCommitRcptNegativeOnly(); }
HRESULT WINAPI EndServicesCommitRcptSendBy(CSrmpToMsmq *pThis) { return pThis->HandleServicesCommitRcptSendBy(); }
HRESULT WINAPI EndServicesCommitRcptSendTo(CSrmpToMsmq *pThis) { return pThis->HandleServicesCommitRcptSendTo(); }

HRESULT WINAPI EndProps(CSrmpToMsmq *pThis) { return pThis->HandleProps(); }
HRESULT WINAPI EndPropsExpiresAt(CSrmpToMsmq *pThis) { return pThis->HandlePropsExpiresAt(); }
HRESULT WINAPI EndPropsDuration(CSrmpToMsmq *pThis) { return pThis->HandlePropsDuration(); }
HRESULT WINAPI EndPropsSentAt(CSrmpToMsmq *pThis) { return pThis->HandlePropsSentAt(); }
HRESULT WINAPI EndPropsInReplyTo(CSrmpToMsmq *pThis) { return pThis->HandlePropsInReplyTo(); }

HRESULT WINAPI EndStream(CSrmpToMsmq *pThis) { return pThis->HandleStream(); }
HRESULT WINAPI EndStreamStreamID(CSrmpToMsmq *pThis) { return pThis->HandleStreamStreamID(); }
HRESULT WINAPI EndStreamCurrent(CSrmpToMsmq *pThis) { return pThis->HandleStreamCurrent(); }
HRESULT WINAPI EndStreamPrevious(CSrmpToMsmq *pThis) { return pThis->HandleStreamPrevious(); }
HRESULT WINAPI EndStreamEnd(CSrmpToMsmq *pThis) { return pThis->HandleStreamEnd(); }
HRESULT WINAPI EndStreamStart(CSrmpToMsmq *pThis) { return pThis->HandleStreamStart(); }
HRESULT WINAPI EndStreamStartSendRcptTo(CSrmpToMsmq *pThis) { return pThis->HandleStreamStartSendRcptTo(); }
HRESULT WINAPI EndStreamStartExpiresAt(CSrmpToMsmq *pThis) { return pThis->HandleStreamStartExpiresAt(); }
HRESULT WINAPI EndStreamStreamRcptRequest(CSrmpToMsmq *pThis) { return pThis->HandleStreamStreamRcptRequest(); }
HRESULT WINAPI EndStreamRcpt(CSrmpToMsmq *pThis) { return pThis->HandleStreamRcpt(); }
HRESULT WINAPI EndStreamRcptRecvdAt(CSrmpToMsmq *pThis) { return pThis->HandleStreamRcptRecvdAt(); }
HRESULT WINAPI EndStreamRcptStreamId(CSrmpToMsmq *pThis) { return pThis->HandleStreamRcptStreamId(); }
HRESULT WINAPI EndStreamRcptLastOrdinal(CSrmpToMsmq *pThis) { return pThis->HandleStreamRcptLastOrdinal(); }
HRESULT WINAPI EndStreamRcptId(CSrmpToMsmq *pThis) { return pThis->HandleStreamRcptId(); }

HRESULT WINAPI EndDeliveryRcpt(CSrmpToMsmq *pThis) { return pThis->HandleDeliveryRcpt(); }
HRESULT WINAPI EndDeliveryRcptReceivedAt(CSrmpToMsmq *pThis) { return pThis->HandleDeliveryRcptReceivedAt(); }
HRESULT WINAPI EndDeliveryRcptId(CSrmpToMsmq *pThis) { return pThis->HandleDeliveryRcptId(); }

HRESULT WINAPI EndCommitRcpt(CSrmpToMsmq *pThis) { return pThis->HandleCommitRcpt(); }
HRESULT WINAPI EndCommitRcptDecidedAt(CSrmpToMsmq *pThis) { return pThis->HandleCommitRcptDecidedAt(); }
HRESULT WINAPI EndCommitRcptDecision(CSrmpToMsmq *pThis) { return pThis->HandleCommitRcptDecision(); }
HRESULT WINAPI EndCommitRcptId(CSrmpToMsmq *pThis) { return pThis->HandleCommitRcptId(); }
HRESULT WINAPI EndCommitRcptCommiteCode(CSrmpToMsmq *pThis) { return pThis->HandleCommitRcptCommiteCode(); }
HRESULT WINAPI EndCommitRcptXCommitDetail(CSrmpToMsmq *pThis) { return pThis->HandleCommitRcptXCommitDetail(); }

HRESULT WINAPI EndMsmq(CSrmpToMsmq *pThis) { return pThis->HandleMsmq(); }
HRESULT WINAPI EndMsmqClass(CSrmpToMsmq *pThis) { return pThis->HandleMsmqClass(); }
HRESULT WINAPI EndMsmqPrio(CSrmpToMsmq *pThis) { return pThis->HandleMsmqPrio(); }
HRESULT WINAPI EndMsmqJournal(CSrmpToMsmq *pThis) { return pThis->HandleMsmqJournal(); }
HRESULT WINAPI EndMsmqDeadLetter(CSrmpToMsmq *pThis) { return pThis->HandleMsmqDeadLetter(); }
HRESULT WINAPI EndMsmqCorrelation(CSrmpToMsmq *pThis) { return pThis->HandleMsmqCorrelation(); }
HRESULT WINAPI EndMsmqTrace(CSrmpToMsmq *pThis) { return pThis->HandleMsmqTrace(); }
HRESULT WINAPI EndMsmqConnectorType(CSrmpToMsmq *pThis) { return pThis->HandleMsmqConnectorType(); }
HRESULT WINAPI EndMsmqApp(CSrmpToMsmq *pThis) { return pThis->HandleMsmqApp(); }
HRESULT WINAPI EndMsmqBodyType(CSrmpToMsmq *pThis) { return pThis->HandleMsmqBodyType(); }
HRESULT WINAPI EndMsmqHashAlg(CSrmpToMsmq *pThis) { return pThis->HandleMsmqHashAlg(); }
HRESULT WINAPI EndMsmqEod(CSrmpToMsmq *pThis) { return pThis->HandleMsmqEod(); }
HRESULT WINAPI EndMsmqEodFirst(CSrmpToMsmq *pThis) { return pThis->HandleMsmqEodFirst(); }
HRESULT WINAPI EndMsmqEodLast(CSrmpToMsmq *pThis) { return pThis->HandleMsmqEodLast(); }
HRESULT WINAPI EndMsmqEodXConnectorId(CSrmpToMsmq *pThis) { return pThis->HandleMsmqEodXConnectorId(); }
HRESULT WINAPI EndMsmqProvider(CSrmpToMsmq *pThis) { return pThis->HandleMsmqProvider(); }
HRESULT WINAPI EndMsmqProviderType(CSrmpToMsmq *pThis) { return pThis->HandleMsmqProviderType(); }
HRESULT WINAPI EndMsmqProviderName(CSrmpToMsmq *pThis) { return pThis->HandleMsmqProviderName(); }
HRESULT WINAPI EndMsmqSourceQMGuid(CSrmpToMsmq *pThis) { return pThis->HandleMsmqSourceQMGuid(); }
HRESULT WINAPI EndMsmqDestinationMQF(CSrmpToMsmq *pThis) { return pThis->HandleMsmqDestinationMQF(); }
HRESULT WINAPI EndMsmqAdminMQF(CSrmpToMsmq *pThis) { return pThis->HandleMsmqAdminMQF(); }
HRESULT WINAPI EndMsmqResponseMQF(CSrmpToMsmq *pThis) { return pThis->HandleMsmqResponseMQF(); }
HRESULT WINAPI EndMsmqTTRQ(CSrmpToMsmq *pThis) { return pThis->HandleMsmqTTRQ(); }

typedef struct _SrmpTableEntry {
	PFN_END pfnEnd;
	DWORD dwFlags;
} SrmpTableEntry, *PSrmpTableEntry;

const SrmpTableEntry pathTable[] = {
	{EndPath,           SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_PATH
	{EndPathId,         0},                       // SRMP_ST_PATH_ID
	{EndPathTo,         0},                       // SRMP_ST_PATH_TO,
	{EndPathRev,        SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_PATH_REV,
	{EndPathRevVia,     0},                       // SRMP_ST_PATH_REV_VIA,
	{EndPathFrom,       0},                       // SRMP_ST_PATH_FROM,
	{EndPathAction,     0},                       // SRMP_ST_PATH_ACTION,
	{EndPathRelatesTo,  0},                       // SRMP_ST_PATH_RELATESTO,
	{EndPathFixed,      SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_PATH_FIXED,
	{EndPathFwd,        SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_PATH_FWD,
	{EndPathFwdVia,     0},                       // SRMP_ST_PATH_FWD_VIA,
	{EndPathFault,      SRMP_FLAG_IGNORE_CHARS}   // SRMP_ST_PATH_FAULT,
};

const SrmpTableEntry serviceTable[] = {
	{EndServices,                      SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_SERVICES
	{EndServicesDurable,               0},                       // SRMP_ST_SERVICES_DURABLE,
	{EndServicesDelRcptRequest,        SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST,
	{EndServicesDelRcptRequestSendTo,  0},  // SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST_SENDTO,
	{EndServicesDelRcptRequestSendBy,  0},                       // SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST_SENDBY,
	{EndServicesFilterDups,            SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_SERVICES_FILTERDUPLICATES,
	{EndServicesCommitRcpt,            SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST,
	{EndServicesCommitRcptPositevOnly, 0},                       // SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_POSITIVEONLY,
	{EndServicesCommitRcptNegativeOnly,0},                       // SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_NEGATIVEONLY,
	{EndServicesCommitRcptSendBy,      SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_SENDBY,
	{EndServicesCommitRcptSendTo,      0}                        // SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_SENDTO,
};

const SrmpTableEntry propertiesTable[] = {
	{EndProps,           SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_PROPERTIES,
	{EndPropsExpiresAt,  0},                       // SRMP_ST_PROPERTIES_EXPIRESAT,
	{EndPropsDuration,   0},                       // SRMP_ST_PROPERTIES_DURATION,
	{EndPropsSentAt,     0},                       // SRMP_ST_PROPERTIES_SENTAT,
	{EndPropsInReplyTo,  0}                        // SRMP_ST_PROPERTIES_INREPLYTO
};


const SrmpTableEntry streamTable[] = {
	{EndStream,                   SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_STREAM
	{EndStreamStreamID,           0},                       // SRMP_ST_STREAM_STREAMID,
	{EndStreamCurrent,            0},                       // SRMP_ST_STREAM_CURRENT,
	{EndStreamPrevious,           0},                       // SRMP_ST_STREAM_PREVIOUS,
	{EndStreamEnd,                SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_STREAM_END,
	{EndStreamStart,              SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_STREAM_START,
	{EndStreamStartSendRcptTo,    0},                       // SRMP_ST_STREAM_START_SENDRECEIPTSTO,
	{EndStreamStartExpiresAt,     0},                       // SRMP_ST_STREAM_START_EXPIRESAT,
	{EndStreamStreamRcptRequest,  SRMP_FLAG_IGNORE_CHARS}   // SRMP_ST_STREAM_STREAMRECEIPTREQUEST
};

const SrmpTableEntry streamRcptTable[] = {
	{EndStreamRcpt,             SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_STREAMRCPT
	{EndStreamRcptRecvdAt,      SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_STREAMRCPT_RECEIVEDAT,
	{EndStreamRcptStreamId,     0},                       // SRMP_ST_STREAMRCPT_STREAMID,
	{EndStreamRcptLastOrdinal,  0},                       // SRMP_ST_STREAMRCPT_LASTORDINAL,
	{EndStreamRcptId,           SRMP_FLAG_IGNORE_CHARS}   // SRMP_ST_STREAMRCPT_ID,
};


const SrmpTableEntry deliveryTable[] = {
	{EndDeliveryRcpt,           SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_DELIVERYRCPT
	{EndDeliveryRcptReceivedAt, SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_DELIVERYRCPT_RECEIVEDAT,
	{EndDeliveryRcptId,         SRMP_FLAG_IGNORE_CHARS}   // SRMP_ST_DELIVERYRCPT_ID,
};

const SrmpTableEntry commitTable[] = {
	{EndCommitRcpt,               SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_COMMITMENTRCPT
	{EndCommitRcptDecidedAt,      SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_COMMITMENTRCPT_DECIDEDAT,
	{EndCommitRcptDecision,       SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_COMMITMENTRCPT_DECISION,
	{EndCommitRcptId,             SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_COMMITMENTRCPT_ID,
	{EndCommitRcptCommiteCode,    SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_COMMITMENTRCPT_COMMITMENTCODE,
	{EndCommitRcptXCommitDetail,  SRMP_FLAG_IGNORE_CHARS}   // SRMP_ST_COMMITMENTRCPT_XCOMMITMENTDETAIL,
};

const SrmpTableEntry msmqTable[] = {
	{EndMsmq,                SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_MSMQ,
	{EndMsmqClass,           0},                       // SRMP_ST_MSMQ_CLASS,
	{EndMsmqPrio,            0},                       // SRMP_ST_MSMQ_PRIORITY,
	{EndMsmqJournal,         0},                       // SRMP_ST_MSMQ_JOURNAL,
	{EndMsmqDeadLetter,      0},                       // SRMP_ST_MSMQ_DEADLETTER,
	{EndMsmqCorrelation,     0},                       // SRMP_ST_MSMQ_CORRELATION,
	{EndMsmqTrace,           0},                       // SRMP_ST_MSMQ_TRACE,
	{EndMsmqConnectorType,   0},                       // SRMP_ST_MSMQ_CONNECTORTYPE,
	{EndMsmqApp,             0},                       // SRMP_ST_MSMQ_APP,
	{EndMsmqBodyType,        0},                       // SRMP_ST_MSMQ_BODYTYPE,
	{EndMsmqHashAlg,         0},                       // SRMP_ST_MSMQ_HASHALGORITHM,
	{EndMsmqEod,             SRMP_FLAG_IGNORE_CHARS},  // SRMP_ST_MSMQ_EOD,
	{EndMsmqEodFirst,        0},                       // SRMP_ST_MSMQ_EOD_FIRST,
	{EndMsmqEodLast,         0},                       // SRMP_ST_MSMQ_EOD_LAST,
	{EndMsmqEodXConnectorId, 0},                       // SRMP_ST_MSMQ_EOD_XCONNECTORID,
	{EndMsmqProvider,        0},                       // SRMP_ST_MSMQ_PROVIDER,
	{EndMsmqProviderType,    0},                       // SRMP_ST_MSMQ_PROVIDER_TYPE,
	{EndMsmqProviderName,    0},                       // SRMP_ST_MSMQ_PROVIDER_NAME,
	{EndMsmqSourceQMGuid,    0},                       // SRMP_ST_MSMQ_SOURCEQMGUID,
	{EndMsmqDestinationMQF,  0},                       // SRMP_ST_MSMQ_DESTINATIONMQF,
	{EndMsmqAdminMQF,        0},                       // SRMP_ST_MSMQ_ADMINMQF,
	{EndMsmqResponseMQF,     0},                       // SRMP_ST_MSMQ_RESPONSEMQF,
	{EndMsmqTTRQ,            0}                        // SRMP_ST_MSMQ_TTRQ
};


HRESULT CSrmpToMsmq::EndPath(SRMP_STATE endState) {
	return (pathTable[SrmpIndex(endState)].pfnEnd)(this);
}

HRESULT CSrmpToMsmq::EndProperties(SRMP_STATE endState) {
	return (propertiesTable[SrmpIndex(endState)].pfnEnd)(this);
}

HRESULT CSrmpToMsmq::EndServices(SRMP_STATE endState) {
	return (serviceTable[SrmpIndex(endState)].pfnEnd)(this);
}

HRESULT CSrmpToMsmq::EndStream(SRMP_STATE endState) {
	return (streamTable[SrmpIndex(endState)].pfnEnd)(this);
}

HRESULT CSrmpToMsmq::EndStreamRcpt(SRMP_STATE endState) {
	return (streamRcptTable[SrmpIndex(endState)].pfnEnd)(this);
}

HRESULT CSrmpToMsmq::EndDeliveryRcpt(SRMP_STATE endState) {
	return (deliveryTable[SrmpIndex(endState)].pfnEnd)(this);
}

HRESULT CSrmpToMsmq::EndCommitmentRcpt(SRMP_STATE endState) {
	return (commitTable[SrmpIndex(endState)].pfnEnd)(this);
}

HRESULT CSrmpToMsmq::EndMsmq(SRMP_STATE endState) {
	return (msmqTable[SrmpIndex(endState)].pfnEnd)(this);
}

HRESULT CSrmpToMsmq::EndSignature(SRMP_STATE endState) {
	// Signature has no subfields in desktop implementation, and WinCE
	// doesn't support security at this level.
	return S_OK;
}


#define TokToState(tk,state)  case tk:  { newState = state; break; }
#define TokToStateNs(tk,state,requiredNS)  case tk: { if (requiredNS != ns) return FALSE; newState = state; break; }

BOOL CSrmpToMsmq::SetState(SRMP_TOKEN token, const WCHAR * szNS, const DWORD cbNS) {
	SRMP_STATE      curState = GetState();
	SRMP_STATE      newState = SRMP_ST_UNKNOWN;
	SOAP_NAMESPACE  ns       = MapNamespace(szNS,cbNS);


	switch (curState) {
		case SRMP_ST_UNINITIALIZED:
			switch (token) {
				TokToState(SRMP_TOK_ENVELOPE,SRMP_ST_ENVELOPE);
				default: goto done; // only check name space on elements we know about
			}
			if (ns != NAMESPACE_SOAP)
				return FALSE;
		break;

		case SRMP_ST_ENVELOPE:
			switch (token) {
				TokToState(SRMP_TOK_BODY,SRMP_ST_BODY);
				TokToState(SRMP_TOK_HEADER,SRMP_ST_HEADER);
				default: goto done; 
			}
			if (ns != NAMESPACE_SOAP)
				return FALSE;
		break;

		case SRMP_ST_HEADER:
			// HEADER sub-children don't all share the same NS, so perform the check here.
			switch (token) {
				TokToStateNs(SRMP_TOK_PATH,SRMP_ST_PATH,NAMESPACE_SOAP_RP);
				TokToStateNs(SRMP_TOK_PROPERTIES,SRMP_ST_PROPERTIES,NAMESPACE_SRMP);
				TokToStateNs(SRMP_TOK_SERVICES,SRMP_ST_SERVICES,NAMESPACE_SRMP);
				TokToStateNs(SRMP_TOK_STREAM,SRMP_ST_STREAM,NAMESPACE_SRMP);
				TokToStateNs(SRMP_TOK_STREAMRECEIPT,SRMP_ST_STREAMRCPT,NAMESPACE_SRMP);
				TokToStateNs(SRMP_TOK_DELIVERYRECEIPT,SRMP_ST_DELIVERYRCPT,NAMESPACE_SRMP);
				TokToStateNs(SRMP_TOK_COMMITMENTRECEIPT,SRMP_ST_COMMITMENTRCPT,NAMESPACE_SRMP);
				TokToStateNs(SRMP_TOK_MSMQ,SRMP_ST_MSMQ,NAMESPACE_MSMQ);
				TokToState(SRMP_TOK_SIGNATURE,SRMP_ST_SIGNATURE);
				default: goto done;
			}
		break;

		// Immediate children of header element.
		case SRMP_ST_PATH:
			switch (token) {
				TokToState(SRMP_TOK_ID,SRMP_ST_PATH_ID);
				TokToState(SRMP_TOK_TO,SRMP_ST_PATH_TO);
				TokToState(SRMP_TOK_REV,SRMP_ST_PATH_REV);
				TokToState(SRMP_TOK_FROM,SRMP_ST_PATH_FROM);
				TokToState(SRMP_TOK_ACTION,SRMP_ST_PATH_ACTION);
				TokToState(SRMP_TOK_RELATESTO,SRMP_ST_PATH_RELATESTO);
				TokToState(SRMP_TOK_FIXED,SRMP_ST_PATH_FIXED);
				TokToState(SRMP_TOK_FWD,SRMP_ST_PATH_FWD);
				TokToState(SRMP_TOK_FAULT,SRMP_ST_PATH_FAULT);
				default: goto done;
			}
			if (ns != NAMESPACE_SOAP_RP)
				return FALSE;

		break;

		case SRMP_ST_PROPERTIES:
			switch (token) {
				TokToState(SRMP_TOK_EXPIRESAT,SRMP_ST_PROPERTIES_EXPIRESAT);
				TokToState(SRMP_TOK_DURATION,SRMP_ST_PROPERTIES_DURATION);
				TokToState(SRMP_TOK_SENTAT,SRMP_ST_PROPERTIES_SENTAT);
				TokToState(SRMP_TOK_INREPLYTO ,SRMP_ST_PROPERTIES_INREPLYTO);
				default: goto done;
			}
			if (ns != NAMESPACE_SRMP)
				return FALSE;

		break;
		
		case SRMP_ST_SERVICES:
			switch (token) {
				TokToState(SRMP_TOK_DURABLE,SRMP_ST_SERVICES_DURABLE);
				TokToState(SRMP_TOK_DELIVERYRECEIPTREQUEST,SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST);
				TokToState(SRMP_TOK_FILTERDUPLICATES,SRMP_ST_SERVICES_FILTERDUPLICATES);
				TokToState(SRMP_TOK_COMMITMENTRECEIPTREQUEST,SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST);
				default: goto done;
			}
			if (ns != NAMESPACE_SRMP)
				return FALSE;
		break;

		case SRMP_ST_STREAM:
			switch (token) {
				TokToState(SRMP_TOK_STREAM,SRMP_ST_STREAM);
				TokToState(SRMP_TOK_STREAMID,SRMP_ST_STREAM_STREAMID);
				TokToState(SRMP_TOK_CURRENT,SRMP_ST_STREAM_CURRENT);
				TokToState(SRMP_TOK_PREVIOUS,SRMP_ST_STREAM_PREVIOUS);
				TokToState(SRMP_TOK_END,SRMP_ST_STREAM_END);
				TokToState(SRMP_TOK_START,SRMP_ST_STREAM_START);
				default: goto done;
			}
			if (ns != NAMESPACE_SRMP)
				return FALSE;
		break;

		case SRMP_ST_STREAMRCPT :
			switch (token) {
				TokToState(SRMP_TOK_RECEIVEDAT,SRMP_ST_STREAMRCPT_RECEIVEDAT);
				TokToState(SRMP_TOK_STREAMID,SRMP_ST_STREAMRCPT_STREAMID);
				TokToState(SRMP_TOK_LASTORDINAL,SRMP_ST_STREAMRCPT_LASTORDINAL);
				TokToState(SRMP_TOK_ID,SRMP_ST_STREAMRCPT_ID);
				default: goto done;
			}
			if (ns != NAMESPACE_SRMP)
				return FALSE;
		break;

		case SRMP_ST_DELIVERYRCPT:
			switch (token) {
				TokToState(SRMP_TOK_RECEIVEDAT,SRMP_ST_DELIVERYRCPT_RECEIVEDAT);
				TokToState(SRMP_TOK_ID,SRMP_ST_DELIVERYRCPT_ID);
				default: goto done;
			}
			if (ns != NAMESPACE_SRMP)
				return FALSE;
		break;

		case SRMP_ST_COMMITMENTRCPT:
			switch (token) {
				TokToState(SRMP_TOK_DECIDEDAT,SRMP_ST_COMMITMENTRCPT_DECIDEDAT);
				TokToState(SRMP_TOK_ID,SRMP_ST_COMMITMENTRCPT_ID);
				TokToState(SRMP_TOK_COMMITMENTCODE,SRMP_ST_COMMITMENTRCPT_COMMITMENTCODE);
				TokToState(SRMP_TOK_XCOMMITMENTDETAIL,SRMP_ST_COMMITMENTRCPT_XCOMMITMENTDETAIL);
				default: goto done;
			}
			if (ns != NAMESPACE_SRMP)
				return FALSE;
		break;

		case SRMP_ST_MSMQ:
			switch (token) {
				TokToState(SRMP_TOK_CLASS,SRMP_ST_MSMQ_CLASS);
				TokToState(SRMP_TOK_PRIORITY,SRMP_ST_MSMQ_PRIORITY);
				TokToState(SRMP_TOK_JOURNAL,SRMP_ST_MSMQ_JOURNAL);
				TokToState(SRMP_TOK_DEADLETTER,SRMP_ST_MSMQ_DEADLETTER);
				TokToState(SRMP_TOK_CORRELATION,SRMP_ST_MSMQ_CORRELATION);
				TokToState(SRMP_TOK_TRACE,SRMP_ST_MSMQ_TRACE);
				TokToState(SRMP_TOK_CONNECTORTYPE,SRMP_ST_MSMQ_CONNECTORTYPE);
				TokToState(SRMP_TOK_APP,SRMP_ST_MSMQ_APP);
				TokToState(SRMP_TOK_BODYTYPE,SRMP_ST_MSMQ_BODYTYPE);
				TokToState(SRMP_TOK_HASHALGORITHM,SRMP_ST_MSMQ_HASHALGORITHM);
				TokToState(SRMP_TOK_EOD,SRMP_ST_MSMQ_EOD);
				TokToState(SRMP_TOK_PROVIDER,SRMP_ST_MSMQ_PROVIDER);
				TokToState(SRMP_TOK_SOURCEQMGUID,SRMP_ST_MSMQ_SOURCEQMGUID);
				TokToState(SRMP_TOK_DESTINATIONMQF,SRMP_ST_MSMQ_DESTINATIONMQF);
				TokToState(SRMP_TOK_ADMINMQF,SRMP_ST_MSMQ_ADMINMQF);
				TokToState(SRMP_TOK_RESPONSEMQF,SRMP_ST_MSMQ_RESPONSEMQF);
				TokToState(SRMP_TOK_TTRQ,SRMP_ST_MSMQ_TTRQ);
				default: goto done;
			}
			if (ns != NAMESPACE_MSMQ)
				return FALSE;
		break;

		case SRMP_ST_SIGNATURE:
			// No Namespace check and no supported sub elements.  Like desktop MSMQ.
			newState = SRMP_ST_UNKNOWN;
		break;

		// Various sub-children
		case SRMP_ST_PATH_REV:
			switch (token) {
				TokToState(SRMP_TOK_VIA,SRMP_ST_PATH_REV_VIA);
				default: goto done;
			}
			if (ns != NAMESPACE_SOAP_RP)
				return FALSE;
		break;

		case SRMP_ST_PATH_FWD:
			switch (token) {
				TokToState(SRMP_TOK_VIA,SRMP_ST_PATH_FWD_VIA);
				default: goto done;
			}
			if (ns != NAMESPACE_SOAP_RP)
				return FALSE;
		break;
		
		case SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST:
			switch (token) {
				TokToState(SRMP_TOK_SENDTO,SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST_SENDTO);
				TokToState(SRMP_TOK_SENDBY,SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST_SENDBY);
				default: goto done;
			}
			if (ns != NAMESPACE_SRMP)
				return FALSE;
		break;

		case SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST:
			switch (token) {
				TokToState(SRMP_TOK_POSITIVEONLY,SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_POSITIVEONLY);
				TokToState(SRMP_TOK_NEGATIVEONLY,SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_NEGATIVEONLY);
				TokToState(SRMP_TOK_SENDBY,SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_SENDBY);
				TokToState(SRMP_TOK_SENDTO,SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_SENDTO);
				default: goto done;
			}
			if (ns != NAMESPACE_SRMP)
				return FALSE;
		break;

		case SRMP_ST_STREAM_START:
			switch (token) {
				TokToState(SRMP_TOK_SENDRECEIPTSTO,SRMP_ST_STREAM_START_SENDRECEIPTSTO);
				TokToState(SRMP_TOK_EXPIRESAT,SRMP_ST_STREAM_START_EXPIRESAT);
				default: goto done;
			}
			if (ns != NAMESPACE_SRMP)
				return FALSE;
		break;

		case SRMP_ST_MSMQ_EOD:
			switch (token) {
				TokToState(SRMP_TOK_FIRST,SRMP_ST_MSMQ_EOD_FIRST);
				TokToState(SRMP_TOK_LAST,SRMP_ST_MSMQ_EOD_LAST);
				TokToState(SRMP_TOK_XCONNECTORID,SRMP_ST_MSMQ_EOD_XCONNECTORID);
				default: goto done;
			}
			if (ns != NAMESPACE_MSMQ)
				return FALSE;
		break;

		case SRMP_ST_MSMQ_PROVIDER:
			switch (token) {
				TokToState(SRMP_TOK_TYPE,SRMP_ST_MSMQ_PROVIDER_TYPE);
				TokToState(SRMP_TOK_NAME,SRMP_ST_MSMQ_PROVIDER_NAME);
				default: goto done;
			}
			if (ns != NAMESPACE_MSMQ)
				return FALSE;
		break;

		default:
			newState = SRMP_ST_UNKNOWN;
		break;
	}

done:
	return SetState(newState,GetStateFlags(newState));
}

BOOL CSrmpToMsmq::SetState(SRMP_STATE state, DWORD dwFlags) {
	SRMP_STATE_INFO *pStateInfo;

	if (NULL == (pStateInfo = (SRMP_STATE_INFO*) svsutil_GetFixed(m_pfmdState)))
		return FALSE;

	pStateInfo->state        = state;
	pStateInfo->dwStateFlags = dwFlags;

	if (! m_State.Push(pStateInfo)) {
		svsutil_FreeFixed(pStateInfo,m_pfmdState);
		return FALSE;
	}
	return TRUE;
}


DWORD CSrmpToMsmq::GetStateFlags(SRMP_STATE state) {
	switch (SrmpType(state)) {
	case SRMP_ST_PATH:
		return pathTable[SrmpIndex(state)].dwFlags;
	break;

	case SRMP_ST_PROPERTIES:
		return propertiesTable[SrmpIndex(state)].dwFlags;
	break;

	case SRMP_ST_SERVICES:
		return serviceTable[SrmpIndex(state)].dwFlags;
	break;

	case SRMP_ST_STREAM:
		return streamTable[SrmpIndex(state)].dwFlags;
	break;

	case SRMP_ST_STREAMRCPT:
		return streamRcptTable[SrmpIndex(state)].dwFlags;
	break;

	case SRMP_ST_DELIVERYRCPT:
		return deliveryTable[SrmpIndex(state)].dwFlags;
	break;

	case SRMP_ST_COMMITMENTRCPT:
		return commitTable[SrmpIndex(state)].dwFlags;
	break;

	case SRMP_ST_MSMQ:
		return msmqTable[SrmpIndex(state)].dwFlags;
	break;	

	case SRMP_ST_SIGNATURE:
		return 0;
	break;	

	default:
		return SRMP_FLAG_IGNORE_CHARS;
	break;
	}
}
