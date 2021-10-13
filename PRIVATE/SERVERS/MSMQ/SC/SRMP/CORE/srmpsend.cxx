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

    SrmpSend.cxx

Abstract:

    Creates an HTTP packet based on message and sends it to intented recepient.
 
--*/

#include <windows.h>
#include <wininet.h>
#include <mq.h>
#include <service.h>

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
#include "scqman.hxx"
#include "SrmpFwd.hxx"

// use SVSXMLEncode::Append() on values below because we don't want to escape the quote characters.
const WCHAR cszXMLHeader[]          = L"<?xml version=\"1.0\" ?>";
const WCHAR cszEnvelopeStartNS[]    = L"<se:Envelope xmlns:se=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns=\"http://schemas.xmlsoap.org/srmp/\">";
const WCHAR cszMustUnderstandNS[]   = L" se:mustUnderstand=\"1\">";
const WCHAR cszPathNS[]             = L"<path xmlns=\"http://schemas.xmlsoap.org/rp/\"";
const WCHAR cszPropertiesNS[]       = L"<properties";
const WCHAR cszServicesNS[]         = L"<services";
const WCHAR cszStreamNS[]           = L"<Stream";
const WCHAR cszMsmqNS[]             = L"<Msmq xmlns=\"msmq.namespace.xml\">";

const WCHAR cszHeaderNS[]           = L"se:Header";
const WCHAR cszBodyNS[]             = L"se:Body";
const WCHAR cszEnvelopeNS[]         = L"se:Envelope";

DWORD CalcSrmpFwdSize(WCHAR *szQueueName, ScPacketImage *pPacketImage);

BOOL  SendPath(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage);
BOOL  SendProps(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage);
BOOL  SendServices(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage);
BOOL  SendStream(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage);
BOOL  SendStreamRcpt(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage);
BOOL  SendDeliveryRcpt(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage);
BOOL  SendCommitRcpt(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage);
BOOL  SendMsmq(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage);
BOOL  SendUserHeader(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage);

DWORD GetExpiresFromPacketImage(ScPacketImage *pImage);
BOOL  SetMQCurrentTime(SVSXMLEncode *pXmlEncode, const WCHAR *szElementName);
BOOL  SetMsgID(SVSXMLEncode *pXmlEncode, OBJECTID *pMsgId);

int BuildHttpHeadersAndMimeBody(ScPacketImage *pImage, CHAR *szSoapEnv, DWORD ccSoapEnv, BOOL fSecure, PSrmpIOCTLPacket pIOPacket, SVSSimpleBuffer &cBodyBuf);
CHAR* BuildSoapEnvelope(ScPacketImage *pImage, DWORD *pccSendBuffer);


BOOL StatusCodeSuccess(DWORD dwStatus) {
	return (dwStatus == 200) || (dwStatus == 201) || (dwStatus == 202) || (dwStatus == 203) || (dwStatus == 204) || (dwStatus == 205);
}

BOOL StatusCodeRetriableError(DWORD dwStatus) {
	if (!gMachine->fUntrustedNetwork) {
		// on a trusted network we're willing to take chance that there could
		// be some other network error causing the status code to be incorrect.
		return ! StatusCodeSuccess(dwStatus);
	}

	// On an untrusted net it's possible a malicious app has pointed us off to
	// a non-existant on not setup machine, in which case we'll only retry on a 
	// certain set of errors.
	return (dwStatus == 408) || (dwStatus == 500) || (dwStatus == 502) || (dwStatus == 503) || (dwStatus == 504);
}


#define SRMP_INTERNET_TIMEOUT  (30*1000)  /* 30 seconds */

class CSrmpCallBack {
	friend void CALLBACK SrmpCallback(HINTERNET hInternet,DWORD_PTR dwContext,DWORD dwInternetStatus,LPVOID lpvStatusInformation,DWORD dwStatusInformationLength);
	HANDLE hEventComplete;
	HANDLE hEventClosed;

public:
	DWORD  dwResult;
	DWORD  dwError;

	CSrmpCallBack(void) {
		memset(this,0,sizeof(*this));
		dwResult = 500;
		hEventComplete = CreateEvent(NULL,FALSE,FALSE,NULL);
		hEventClosed   = CreateEvent(NULL,FALSE,FALSE,NULL);
	}

	BOOL IsInitialized(void) { return (hEventComplete && hEventClosed ) ? TRUE : FALSE; }

	BOOL Wait(DWORD dwTimeout) {
		return (WAIT_TIMEOUT != WaitForSingleObject(hEventComplete,dwTimeout));
	}

	void Failed(void) {
		// make sure we don't block forever on deinitializing + waiting for hEventClosed
		if (hEventClosed) {
			CloseHandle(hEventClosed);
			hEventClosed = 0;
		}
	}

	~CSrmpCallBack(void) {
		if (hEventClosed) {
			// make sure we get closed to make sure that our async caller isn't called after class is deallocated.
			WaitForSingleObject(hEventClosed,INFINITE);
			CloseHandle(hEventClosed);
		}

		if (hEventComplete)
			CloseHandle(hEventComplete);
	}
};

#if defined (InternetSetStatusCallback)
// Wininet defines this to InternetSetStatusCallback to (A|W) based on ANSI/UNICODE,
// httplite only exports InternetSetStatusCallback.
// 
#undef InternetSetStatusCallback
INTERNETAPI_(INTERNET_STATUS_CALLBACK) InternetSetStatusCallback(
																 IN HINTERNET hInternet,
																 IN INTERNET_STATUS_CALLBACK lpfnInternetCallback
																 );
#endif

int ScSession::InitializeWininet(CSrmpCallBack *pCallback) {
	DWORD          dwTimeout = SRMP_INTERNET_TIMEOUT;
	INTERNET_PORT  iPort     = (qType == SCFILE_QP_FORMAT_HTTPS) ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

	if (NULL == (hInternetSession = InternetOpenA(NULL,INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,INTERNET_FLAG_ASYNC))) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"InternetOpen fails, GLE=0x%08x\r\n",GetLastError()));
		fSessionState = SCSESSION_STATE_INACTIVE;
		return FALSE;
	}

	InternetSetOption(hInternetSession, INTERNET_OPTION_RECEIVE_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	InternetSetOption(hInternetSession, INTERNET_OPTION_SEND_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	InternetSetOption(hInternetSession, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	InternetSetOption(hInternetSession, INTERNET_OPTION_DATA_SEND_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	InternetSetOption(hInternetSession, INTERNET_OPTION_CONNECT_TIMEOUT , &dwTimeout,sizeof(dwTimeout));

	if (NULL == (hInternetConnect = InternetConnectA(hInternetSession,lpszmbHostName,iPort,NULL,NULL,INTERNET_SERVICE_HTTP,0,(DWORD)pCallback))) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"InternetConnect fails, GLE=0x%08x\r\n",GetLastError()));
		fSessionState = SCSESSION_STATE_INACTIVE;
		return FALSE;
	}

	if (INTERNET_INVALID_STATUS_CALLBACK == InternetSetStatusCallback(hInternetConnect,&SrmpCallback)) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"InternetConnect fails, GLE=0x%08x\r\n",GetLastError()));
		fSessionState = SCSESSION_STATE_INACTIVE;
		return FALSE;
	}

	return TRUE;
}

// We'll try to send a message this many times before we end the session.
#define  SRMP_MAX_RETRIES             5
// Wait this many milliseconds on HTTP request recoverable failures.
#define  SRMP_RETRY_WAIT              (25*1000)

void ScSession::ServiceThreadHttpW(void) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Sending HTTP thread for %s\r\n", lpszHostName);
#endif

	int            iStatus   = 200;
	BOOL           fClearAttrTimer = FALSE;
	int            iRetries  = 0;

	fSessionState = SCSESSION_STATE_CONNECTING;

	CSrmpCallBack connectCallback;
	if (!connectCallback.IsInitialized()) {
#if defined (SC_VERBOSE)
		scerror_DebugOut(VERBOSE_MASK_SRMP,L"ServiceThreadHttpW cannot CreateEvent, GLE=0x%08x\r\n",GetLastError());
#endif
		return;
	}

	gMem->Lock();

	int uiSCSESSION_IDLE_TIMEOUT = gMachine->uiIdleTimeout * 1000;

	if (!InitializeWininet(&connectCallback)) {
		connectCallback.Failed();
		goto done;
	}

	// connectCallback.Wait(2000);

	fSessionState = SCSESSION_STATE_OPERATING;

	SetEvent(hEvent);	// Do the first scan...
	while (fSessionState == SCSESSION_STATE_OPERATING) {
		ScPacket *pPacket = NULL;
		ScQueue  *pQueue  = NULL;
		SVSUTIL_ASSERT (gMem->IsLocked ());

		gMem->Unlock();
		int iResp = WaitForSingleObject (hEvent, uiSCSESSION_IDLE_TIMEOUT);

		gMem->Lock ();
		if (fClearAttrTimer) {
			svsutil_ClearAttrTimer(gMem->pTimer,(SVSHandle)hEvent);
			fClearAttrTimer = FALSE;
		}
		
		if (iResp == WAIT_TIMEOUT)
			break;

		if (fSessionState == SCSESSION_STATE_EXITING) {
			// don't goto done; because another thread has initiated FinishSession()
			gMem->Unlock ();
			return; 
		}

		if (! ExtractNextPacket(pPacket, pQueue, pQueue))
			continue;

		// message to remote queue
		iStatus = BuildAndSendSRMP(pPacket,pQueue);
		SVSUTIL_ASSERT (gMem->IsLocked());

		if (StatusCodeRetriableError(iStatus)) {
			iRetries++;
			SVSUTIL_ASSERT(iRetries <= SRMP_MAX_RETRIES);

			if (iRetries == SRMP_MAX_RETRIES) {
				scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"ServiceThreadHttpW cannot successfully send message, closing down session to try again later\r\n"));
				break;
			}
		
			// On server errors we assume it's temporary and that we can try again later.
			BOOL fSwallowEvent = (pSentPackets || pSentRelPackets) ? TRUE : FALSE;

			ReturnUnacketPacketsToQueues();
			// we have to do this because when packet is inserted back 
			// into the queue SetEvent(hEvent) is called.
			if (fSwallowEvent)
				WaitForSingleObject(hEvent,0);
				
			if (fSessionState == SCSESSION_STATE_OPERATING) {
				fClearAttrTimer = TRUE;
				svsutil_SetAttrTimer(gMem->pTimer,(SVSHandle)hEvent,SRMP_RETRY_WAIT);
			}
			continue;
		}
		iRetries = 0;

		// either we succeeded or we encountered an non-recoverable error, in either
		// event discard the packet and move onto next message in queue.

		// Dispose of packet.  We only queue up one at a time so check is trivial.
		if (pSentPackets) {
			svsutil_FreeFixed(pSentPackets, gMem->pAckNodeMem);
			pSentPackets = NULL;
		}
		else if (pSentRelPackets) {
			svsutil_FreeFixed(pSentRelPackets, gMem->pAckNodeMem);
			pSentRelPackets = NULL;
		}
		gQueueMan->JournalPacket(pPacket);
		pQueue->DisposeOfPacket(pPacket);
		SetEvent(hEvent); // immediatly try to send next packet.
	}

done:
	SVSUTIL_ASSERT(gMem->IsLocked());

	if (fClearAttrTimer)
		svsutil_ClearAttrTimer(gMem->pTimer,(SVSHandle)hEvent);

	FinishSession();
	fSessionState = SCSESSION_STATE_INACTIVE;
	gSessionMan->ReleaseSession (this);
	gMem->Unlock();
}

// put a preceding '/' in front of name if needed.
PSTR AllocURL(PCWSTR wszURL) {
	PSTR pszOut = 0;
	int iAddSlash = (wszURL[0] != '\\' && wszURL[0] != '/') ? 1 : 0;

	int  iOutLen = WideCharToMultiByte(CP_ACP, 0, wszURL, -1, 0, 0, 0, 0);
	if(!iOutLen)
		goto error;

	iOutLen += iAddSlash;

	pszOut = (PSTR)g_funcAlloc(iOutLen*sizeof(WCHAR), g_pvAllocData);
	if(!pszOut)
		goto error;


	if (iAddSlash) {
		pszOut[0] = '/';
	}

	if(WideCharToMultiByte(CP_ACP, 0, wszURL, -1, pszOut+iAddSlash, iOutLen-iAddSlash, 0, 0))
		return pszOut;

error:
	if (pszOut)
		g_funcFree(pszOut,g_pvFreeData);
	return FALSE;
}


int ScSession::BuildAndSendSRMP(ScPacket *pPacket, ScQueue *pQueue)  {
	CHAR            *szSoapEnv = NULL;
	DWORD           ccSoapEnv;
	int             iStatusCode = 503; // if we fail it's most likely because of malloc failure, we can try again
	CHAR            *szURL     = NULL;
	CHAR            *szTarget  = NULL;
	ScPacketImage   *pNewImage = NULL;
	SrmpIOCTLPacket ioPacket;
	CHAR szHeaders[2048];
	SVSSimpleBuffer cBodyBuf;

	// SendUP() stuff...
	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT (! pQueue->qp.bIsIncoming);

	scerror_DebugOutM (VERBOSE_MASK_SRMP, (L"Sending user packet %08x ptr %08x queue %s to %s\n", pPacket->uiMessageID, pPacket, pQueue->lpszFormatName, lpszHostName));

	if (pQueue->qp.bTransactional) {
		SVSUTIL_ASSERT (pPacket->iDirEntry >= 0);
		SVSUTIL_ASSERT (pPacket->pNode);

		SVSTNode *pPrevNode = pQueue->pPackets->Prev (pPacket->pNode);
		ScPacket *pPrev = pPrevNode ? (ScPacket *)SVSTree::GetData(pPrevNode) : NULL;

		pPacket->uiPacketState = SCPACKET_STATE_WAITORDERACK;
		pPacket->pImage->sect.pXactHeader->SetPrevSeqN (pPrev ? pPrev->uiSeqN : 0);
	} else {
		SentPacket *pSP = (SentPacket *)svsutil_GetFixed (gMem->pAckNodeMem);

		pSP->pPacket      = pPacket;

		++usPacketsSent;
		if (! usPacketsSent)
			++usPacketsSent;

		if (pPacket->iDirEntry >= 0) {
			++usRelPacketsSent;
			if (! usRelPacketsSent)
				++usRelPacketsSent;

			pSP->usNum = usRelPacketsSent;
			pSP->pNext = pSentRelPackets;

			pSentRelPackets = pSP;
		} else {
			pSP->usNum = usPacketsSent;
			pSP->pNext = pSentPackets;

			pSentPackets = pSP;
		}
	}

	unsigned int tTRQ = pPacket->pImage->sect.pBaseHeader->GetAbsoluteTimeToQueue ();
	unsigned int tBas = pPacket->pImage->sect.pUserHeader->GetSentTime ();
	unsigned int tNow = scutil_now ();

	if (tTRQ != INFINITE)
		pPacket->pImage->sect.pBaseHeader->SetAbsoluteTimeToQueue (tTRQ - (tNow - tBas));


	++gSessionMan->fBusy;
	pNewImage = scqman_DupImage(pPacket->pImage);

	pPacket->pImage->sect.pBaseHeader->SetAbsoluteTimeToQueue (tTRQ);
	pPacket->pImage->sect.pBaseHeader->IncludeSession (FALSE);

	pQueue->FreeFromMemory(pPacket);

	if (! pNewImage) {
		scerror_DebugOutM (VERBOSE_MASK_SESSION, (L"No memory - failing session %s...\n", lpszHostName));
		--gSessionMan->fBusy;
		return 0;
	}
	BOOL fSecure = IsSecure();

	// If we are to forward an SRMP message create and send it in ForwardSrmpMessage.
	if (pNewImage->sect.pUserHeader->SrmpIsIncluded()) {
		WCHAR *szQueueName;
		if (! gMachine->RouteLocalReverseLookup(pQueue->lpszQueueName,&szQueueName))
			szQueueName = pQueue->lpszQueueName;

		CSrmpFwd cSrmpFwd(this,szQueueName,pNewImage->sect.pFwdViaHeader ? (WCHAR*)pNewImage->sect.pFwdViaHeader->GetData() : L"");
		gMem->Unlock();

		if (!cSrmpFwd.IsInitailized())
			goto done;
		
		iStatusCode = ForwardSrmpMessage(&cSrmpFwd,pNewImage,pQueue->lpszQueueName);
		goto done;
	}

	gMem->Unlock();

	if (NULL == (szSoapEnv = BuildSoapEnvelope(pNewImage,&ccSoapEnv)))
		goto done;

	memset(&ioPacket,0,sizeof(ioPacket));
	ioPacket.pszHeaders = szHeaders;

	if (! BuildHttpHeadersAndMimeBody(pNewImage,szSoapEnv,ccSoapEnv,fSecure,&ioPacket,cBodyBuf))
		goto done;

	if (NULL == (szURL = AllocURL(pQueue->lpszQueueName)))
		goto done;

	iStatusCode = SendHttpMsg(szURL,fSecure,&ioPacket); 
done:
	SVSUTIL_ASSERT(! gMem->IsLocked());

	if (szURL)
		g_funcFree(szURL,g_pvFreeData);

	if (szSoapEnv)
		g_funcFree(szSoapEnv,g_pvFreeData);

	if (pNewImage)
		g_funcFree(pNewImage,g_pvFreeData);
	
	gMem->Lock();
	--gSessionMan->fBusy;
	return iStatusCode;
}


CHAR *BuildSoapEnvelope(ScPacketImage *pImage, DWORD *pccSendBuffer) {
	SVSXMLEncode    xmlEncode(4096);
	CUserHeader     *pUserHeader       = pImage->sect.pUserHeader;
	CSoapSection    *pSoapBodySection  = pImage->sect.pSoapBodySection;
	DWORD           cbAlloc = 16384;
	CHAR            *szSendBuffer;

	// Initially allocate a large buffer to hopefully eliminate need for reallocs in most cases.
	if (pUserHeader->SoapIsIncluded())
		cbAlloc += pSoapBodySection->GetDataLengthInWCHARs();

	if (!xmlEncode.AllocMem(cbAlloc))
		return NULL;

	xmlEncode.Append((CHAR*)cszXMLHeader,SVSUTIL_CONSTSTRLEN(cszXMLHeader)*sizeof(WCHAR));
	xmlEncode.Append((CHAR*)cszEnvelopeStartNS,SVSUTIL_CONSTSTRLEN(cszEnvelopeStartNS)*sizeof(WCHAR));
	xmlEncode.StartTag(cszHeaderNS);

	// <HEADER> section
	if (! SendPath(&xmlEncode,pImage))
		return NULL;

	if (! SendProps(&xmlEncode,pImage))
		return NULL;

	if (! SendServices(&xmlEncode,pImage))
		return NULL;

	if (! SendStream(&xmlEncode,pImage))
		return NULL;

	if (! SendStreamRcpt(&xmlEncode,pImage))
		return NULL;

	if (! SendDeliveryRcpt(&xmlEncode,pImage))
		return NULL;

	if (! SendCommitRcpt(&xmlEncode,pImage))
		return NULL;

	if (! SendMsmq(&xmlEncode,pImage))
		return NULL;

	if (! SendUserHeader(&xmlEncode,pImage))
		return NULL;

	if (! xmlEncode.EndTag(cszHeaderNS))
		return NULL;

	// <BODY> section
	if (! xmlEncode.StartTag(cszBodyNS))
		return NULL;

	if (pUserHeader->SoapIsIncluded()) {
		if (pSoapBodySection->GetPointerToData()) {
			if (!xmlEncode.Encode(pSoapBodySection->GetPointerToData()))
				return NULL;
		}
	}
	if (! xmlEncode.EndTag(cszBodyNS))
		return NULL;

	if (! xmlEncode.EndTag(cszEnvelopeNS))
		return NULL;

	if (! xmlEncode.Append((char*)L"\0",sizeof(WCHAR)))
		return NULL;

#if defined (DEBUG) || defined (_DEBUG)
	svsutil_validateXML((BSTR)xmlEncode.pBuffer);
#endif

	// covert SOAP Envelope to UTF8
	DWORD cp = CP_UTF8;

	if (0 == (*pccSendBuffer = WideCharToMultiByte(cp, 0, (WCHAR*)xmlEncode.pBuffer, -1, NULL, 0, 0, 0))) {
		cp = CP_ACP;
		if (0 == (*pccSendBuffer = WideCharToMultiByte(cp, 0, (WCHAR*)xmlEncode.pBuffer, -1, NULL, 0, 0, 0)))
			return NULL;
	}

	if (NULL == (szSendBuffer = (CHAR*) g_funcAlloc(*pccSendBuffer,g_pvAllocData)))
		return NULL;

	WideCharToMultiByte(cp, 0, (WCHAR*)xmlEncode.pBuffer, -1, szSendBuffer, *pccSendBuffer, 0, 0);
	*pccSendBuffer -= 1;
	return szSendBuffer;
}


BOOL IsBodyIncluded(ScPacketImage *pImage) {
	CUserHeader            *pUserHeader             = pImage->sect.pUserHeader;
	CCompoundMessageHeader *pCompoundMessageHeader  = pImage->sect.pCompoundMessageHeader;
	CPropertyHeader        *pPropHeader             = pImage->sect.pPropHeader;

	if (pUserHeader->SrmpIsIncluded())
		return (pCompoundMessageHeader && pCompoundMessageHeader->GetBodySizeInBytes() != 0);
	return (pPropHeader->GetBodySize() != 0);
}


#define BOUNDARY_VALUE "MSMQ - SOAP boundary, %d"
const char cszApplicationContentType[]  = "application/octet-stream";
const char cszMultipartContentType[]    = "multipart/related";
#define MIME_ID_FMT_A		"%.*s" GUID_FORMAT_A

// MIME_ID_FMT_A

int BuildHttpHeadersAndMimeBody(ScPacketImage *pImage, CHAR *szSoapEnv, DWORD ccSoapEnv, BOOL fSecure, PSrmpIOCTLPacket pIOPacket, SVSSimpleBuffer &cBodyBuf) {
	CUserHeader     *pUserHeader = pImage->sect.pUserHeader;
	CPropertyHeader *pPropHeader = pImage->sect.pPropHeader;
	const UCHAR* szBody = NULL;
	DWORD        cbBody = 0;
	const UCHAR* szExtension = NULL;
	DWORD        cbExtension = 0;

	pIOPacket->fSSL = fSecure;
	SVSUTIL_ASSERT(pIOPacket->pszHeaders); // write buffer passed into us by caller

	if (IsBodyIncluded(pImage)) {
		CCompoundMessageHeader *pCompoundMessageHeader  = pImage->sect.pCompoundMessageHeader;
		szBody = pUserHeader->SrmpIsIncluded() ? pCompoundMessageHeader->GetPointerToBody()   : pPropHeader->GetBodyPtr();
		cbBody = pUserHeader->SrmpIsIncluded() ? pCompoundMessageHeader->GetBodySizeInBytes() : pPropHeader->GetBodySize();
	}

	if (pPropHeader->GetMsgExtensionSize()) {
		szExtension = pPropHeader->GetMsgExtensionPtr();
		cbExtension = pPropHeader->GetMsgExtensionSize();
	}

	if (szBody || szExtension) {  // complex data type
		srand (GetTickCount());

		DWORD  dwBoundaryId = rand();
		DWORD  cbAlloc = ccSoapEnv + cbBody + cbExtension + 2048;

		// Get all we need at once so there's no chance of needing to realloc
		if (! cBodyBuf.AllocMem(cbAlloc))
			return FALSE;

		// MIME Body
		cBodyBuf.uiNextIn = sprintf(cBodyBuf.pBuffer,BOUNDARY_HYPHEN BOUNDARY_VALUE "\r\n"
		                      "Content-Type: %s; charset=UTF-8\r\nContent-Length: %d\r\n\r\n",
		                      dwBoundaryId,cszTextXML,ccSoapEnv);

		cBodyBuf.Append(szSoapEnv,ccSoapEnv);

		if (szBody) {
			cBodyBuf.uiNextIn += sprintf(cBodyBuf.pBuffer + cBodyBuf.uiNextIn,
			                             BOUNDARY_HYPHEN BOUNDARY_VALUE "\r\n"
  			                             "Content-Type: %s\r\n"
			                             "Content-Length: %d\r\n"
			                             "Content-Id: " MIME_ID_FMT_A "\r\n\r\n",
			                             dwBoundaryId,cszApplicationContentType,
			                             cbBody,
			                             ccMimeBodyIdLen, cszMimeBodyId,
			                             GUID_ELEMENTS((&gMachine->guid)));
			memcpy(cBodyBuf.pBuffer+cBodyBuf.uiNextIn,szBody,cbBody);
			cBodyBuf.uiNextIn += cbBody;
		}

		if (szExtension) {
			cBodyBuf.uiNextIn += sprintf(cBodyBuf.pBuffer + cBodyBuf.uiNextIn,
			                             BOUNDARY_HYPHEN BOUNDARY_VALUE "\r\n"
  			                             "Content-Type: %s\r\n"
			                             "Content-Length: %d\r\n"
			                             "Content-Id: " MIME_ID_FMT_A "\r\n\r\n",
			                             dwBoundaryId,cszApplicationContentType,
			                             cbExtension,
			                             ccMimeExtensionIdLen,cszMimeExtensionId,
			                             GUID_ELEMENTS((&gMachine->guid)));

			memcpy(cBodyBuf.pBuffer+cBodyBuf.uiNextIn,szExtension,cbExtension);
			cBodyBuf.uiNextIn += cbExtension;
		}

		cBodyBuf.uiNextIn += sprintf(cBodyBuf.pBuffer + cBodyBuf.uiNextIn,BOUNDARY_HYPHEN BOUNDARY_VALUE BOUNDARY_HYPHEN "\r\n",dwBoundaryId);

		// HTTP headers
		pIOPacket->cbHeaders = sprintf(pIOPacket->pszHeaders,
		                    "Content-Type: %s; boundary=\"" BOUNDARY_VALUE "\"; type=text/xml\r\n"
		                    "SOAPAction: \"MSMQMessage\"\r\n"
		                    "Proxy-Accept: NonInteractiveClient\r\n",
		                    cszMultipartContentType,dwBoundaryId
		                    );

		pIOPacket->pszPost     = cBodyBuf.pBuffer;
		pIOPacket->cbPost      = cBodyBuf.uiNextIn;
		pIOPacket->contentType = CONTENT_TYPE_MIME;

	}
	else { // simple type
		pIOPacket->cbHeaders = sprintf(pIOPacket->pszHeaders,
                            "Content-Type: %s\r\n"
							"SOAPAction: \"MSMQMessage\"\r\n" 
                            "Proxy-Accept: NonInteractiveClient\r\n",
                             cszTextXML
                            );

		pIOPacket->pszPost     = szSoapEnv;
		pIOPacket->cbPost      = ccSoapEnv;
		pIOPacket->contentType = CONTENT_TYPE_XML;
	}
	return TRUE;
}

int ScSession::SendHttpMsg(CHAR *szURL, BOOL fSecure, PSrmpIOCTLPacket pIOPacket) {
	HANDLE hRequest;
	DWORD  dwFlags    = 0;
	int    iStatusCode=500;

	CSrmpCallBack openCallback;

	dwFlags  =  (g_fUseWininet ? INTERNET_FLAG_KEEP_CONNECTION : 0);
	dwFlags  |= (fSecure       ? INTERNET_FLAG_SECURE          : 0);

	gMem->Lock();
	if (glServiceState != SERVICE_STATE_ON) {
		openCallback.Failed();
		gMem->Unlock();
		return 503;
	}
	hRequest = HttpOpenRequestA(hInternetConnect,"POST",szURL,NULL,NULL,NULL,dwFlags,(DWORD)&openCallback);

	if (!hRequest) {
		openCallback.Failed();
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"HttpOpenRequest fails, GLE=0x%08x\r\n",GetLastError()));
	}
	gMem->Unlock();

	if (hRequest) {
		DWORD cbStatus = sizeof(iStatusCode);
		
		if (!HttpSendRequestA(hRequest,pIOPacket->pszHeaders,pIOPacket->cbHeaders,pIOPacket->pszPost,pIOPacket->cbPost))
			openCallback.Wait(30000);

		if (openCallback.dwError == 0)  {
			if (!HttpQueryInfo(hRequest,HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &iStatusCode,&cbStatus,NULL))
				iStatusCode = 500;
		}
#if defined (SC_VERBOSE)
		else 
			scerror_DebugOut(VERBOSE_MASK_SRMP,L"HttpSendRequest fails, openCallback.dwResult=%d, GLE=0x%08x\r\n",openCallback.dwResult,openCallback.dwError);
#endif
		gMem->Lock();
		InternetCloseHandle(hRequest);
		hRequest = 0;
		gMem->Unlock();
	}

	return iStatusCode;
}


void CALLBACK SrmpCallback(HINTERNET hInternet,DWORD_PTR dwContext,DWORD dwInternetStatus,LPVOID lpvStatusInformation,DWORD dwStatusInformationLength) {
	CSrmpCallBack* pThis = (CSrmpCallBack*)(dwContext);

	switch(dwInternetStatus) {
		case INTERNET_STATUS_REQUEST_COMPLETE: 	{
			INTERNET_ASYNC_RESULT* pAsyncResult = reinterpret_cast<INTERNET_ASYNC_RESULT*>(lpvStatusInformation);

			pThis->dwError  = pAsyncResult->dwError;
			pThis->dwResult = pAsyncResult->dwResult;
			SetEvent(pThis->hEventComplete);
		}
		break;

		case INTERNET_STATUS_HANDLE_CLOSING:
			SetEvent(pThis->hEventClosed);
		break;
	}
}

BOOL IsDirectHttpFormatName(const QUEUE_FORMAT* pQueueFormat) {
	DIRECT_TOKEN_TYPE ddt;

	if (pQueueFormat->GetType() == QUEUE_FORMAT_TYPE_DIRECT && 
	    ParseDirectTokenString(pQueueFormat->DirectID(),ddt)) {
		return (ddt == DT_HTTP || ddt == DT_HTTPS);
	}

	return FALSE;
}



BOOL QueueFormatToUri(SVSXMLEncode *pXmlEncode, QUEUE_FORMAT *pqf) {
	WCHAR szBuf[MAX_PATH];
	WCHAR *pszTrav = szBuf;
	WCHAR *szURI   = NULL;
	WCHAR *szFormatName;
	BOOL  fRet = FALSE;

	switch (pqf->GetType()) {
		case QUEUE_FORMAT_TYPE_DIRECT : {
			DWORD ccFormatName = SCUTIL_DIRECT_PREFIX_SZ + wcslen(pqf->DirectID()) + 1;
			if (ccFormatName > SVSUTIL_CONSTSTRLEN(szBuf)) {
				if (NULL == (szFormatName = (WCHAR*) g_funcAlloc(sizeof(WCHAR)*ccFormatName,g_pvAllocData)))
					goto done;
			}
			else
				szFormatName = szBuf;

			wcscpy(szFormatName,SCUTIL_DIRECT_PREFIX);
			wcscpy(szFormatName+SCUTIL_DIRECT_PREFIX_SZ,pqf->DirectID());
		}
		break;

		case QUEUE_FORMAT_TYPE_PUBLIC : {
			wcscpy(szBuf,MSMQ_SC_FORMAT_PUBLIC);
			const GUID *pGuid = &pqf->PublicID();
			wsprintf (szBuf+wcslen(MSMQ_SC_FORMAT_PUBLIC),GUID_FORMAT,GUID_ELEMENTS(pGuid));
			szFormatName = szBuf;
		}
		break;

		case QUEUE_FORMAT_TYPE_PRIVATE: {
			wcscpy(szBuf,MSMQ_SC_FORMAT_PRIVATE);
			pszTrav += wcslen(MSMQ_SC_FORMAT_PRIVATE);

			const OBJECTID *pObj = &pqf->PrivateID();
			const GUID *pGuid = &pObj->Lineage;
			pszTrav += wsprintf (pszTrav,GUID_FORMAT FN_PRIVATE_SEPERATOR,GUID_ELEMENTS(pGuid));

			wsprintf(pszTrav,L"%x",pObj->Uniquifier);
			szFormatName = szBuf;
		}
		break;

		default:
			SVSUTIL_ASSERT(0);
	}

	gMem->Lock();
	if (gMachine->RouteLocalReverseLookup(szFormatName,&szURI)) {
		// if we've hit a reverse in lookup table then we originally received
		// message as a free-form URI.  Send it out like this too.
		gMem->Unlock();
		fRet = pXmlEncode->Encode(szURI);
		goto done;
	}
	gMem->Unlock();

	// send direct HTTP without modifications.
	if (IsDirectHttpFormatName(pqf)) 
		fRet = pXmlEncode->Encode(pqf->DirectID());
	// append MSMQ: in front of remaining types.
	else if (pXmlEncode->Encode(cszMSMQPrefix,ccMSMQPrefix))
		fRet = pXmlEncode->Encode(szBuf);
done:
	SVSUTIL_ASSERT(! gMem->IsLocked());

	if (szFormatName != szBuf)
		g_funcFree(szFormatName,g_pvFreeData);
	
	return fRet;
}


BOOL CreateViaList(SVSXMLEncode *pXmlEncode, CDataHeader *pViaHeader, BOOL fSkipFirstElement) {
	int   iLen      = pViaHeader->GetDataLengthInWCHARs();
	WCHAR *szStart  = (WCHAR*) pViaHeader->GetData();
	WCHAR *pszTrav;

	if (fSkipFirstElement) {
		// skip past 1st entry, which is current machine (not forwarded along) in some cases.
		int ccFirstElement = wcslen(szStart) + 1;
		szStart += ccFirstElement;
		iLen    -= ccFirstElement;

		SVSUTIL_ASSERT(iLen > 0); // if there's only one element in fwd list, this function shouldn't have been called.
	}
	pszTrav  = szStart;

	// VIA list is stored as a NULL separated multi-string.  Elements
	// may be set to \0 to indicate an empty tag.
	while (pszTrav - szStart < iLen) {
		if (*pszTrav) {
			if (! pXmlEncode->SetElement(cszVia,pszTrav))
				return FALSE;
		}
		else if (! pXmlEncode->SetEmptyElement(cszVia))
			return FALSE;

		pszTrav += wcslen(pszTrav)+1;
	}
	SVSUTIL_ASSERT(pszTrav-szStart == iLen);
	return TRUE;
}


DWORD GetExpiresFromPacketImage(ScPacketImage *pImage) {
	DWORD dwDelta;
	DWORD dwTimeout;
	CBaseHeader     *pBaseHeader = pImage->sect.pBaseHeader;
	CUserHeader     *pUserHeader = pImage->sect.pUserHeader;

	dwDelta = pUserHeader->GetTimeToLiveDelta();
	if (dwDelta == INFINITE)
		dwTimeout = INFINITE;
	else
		dwTimeout = dwDelta + pBaseHeader->GetAbsoluteTimeToQueue();

	return min(dwTimeout, LONG_MAX);
}

//
// <Path> Section.
//
BOOL SendPath(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage) {
	CPropertyHeader *pPropHeader = pImage->sect.pPropHeader;
	CUserHeader     *pUserHeader = pImage->sect.pUserHeader;
	QUEUE_FORMAT    qf;
	OBJECTID MessageId;

	if (! pXmlEncode->Append((CHAR*)cszPathNS,SVSUTIL_CONSTSTRLEN(cszPathNS)*sizeof(WCHAR)) ||
	    ! pXmlEncode->Append((CHAR*)cszMustUnderstandNS,SVSUTIL_CONSTSTRLEN(cszMustUnderstandNS)*sizeof(WCHAR)))
		return FALSE;

	// <action>
	if (! pXmlEncode->StartTag(cszAction))
		return FALSE;

	// even if there's no title we put <action>MSMQ:</action>, required since <action> is a required field.  Like XP.
	if (! pXmlEncode->Encode(cszMSMQPrefix))
		return FALSE;

	if (pPropHeader->GetTitleLength()) {
		if (! pXmlEncode->Encode(pPropHeader->GetTitlePtr()))
			return FALSE;
	}

	if (! pXmlEncode->EndTag(cszAction))
		return FALSE;

	//<to>
	pUserHeader->GetDestinationQueue(&qf);
	if (! pXmlEncode->StartTag(cszTo))
		return FALSE;

	if (!QueueFormatToUri(pXmlEncode,&qf))
		return FALSE;

	if (! pXmlEncode->EndTag(cszTo))
		return FALSE;

	// <id>
	pUserHeader->GetMessageID(&MessageId);
	if (! SetMsgID(pXmlEncode,&MessageId))
		return FALSE;


	if (pImage->flags.fSoapExtIncluded) {
		CSoapExtSection *pSoapExt = pImage->sect.pSoapExtHeaderSection;

		if (pSoapExt->GetFromLengthInWCHARs()) {
			if (! pXmlEncode->SetElement(cszFrom,pSoapExt->GetFrom(),pSoapExt->GetFromLengthInWCHARs()))
				return FALSE;
		}

		if (pSoapExt->GetRelatesToLengthInWCHARs()) {
			if (! pXmlEncode->SetElement(cszRelatesTo,pSoapExt->GetRelatesTo(),pSoapExt->GetRelatesToLengthInWCHARs()))
				return FALSE;
		}
	}

	if (pImage->sect.pFwdViaHeader && GetNextHopOnFwdList(pImage)) {
		if (! pXmlEncode->StartTag(cszFwd) ||
		    ! CreateViaList(pXmlEncode,pImage->sect.pFwdViaHeader,pImage->flags.fSRMPGenerated) ||
		    ! pXmlEncode->EndTag(cszFwd))
			return FALSE;
	}

	if (pImage->sect.pRevViaHeader) {
		if (! pXmlEncode->StartTag(cszRev) ||
		    ! CreateViaList(pXmlEncode,pImage->sect.pRevViaHeader,FALSE) ||
		    ! pXmlEncode->EndTag(cszRev))
			return FALSE;
	}

	// <rev><via>
	// messages generated on local device (not routed) use MSMQ msg props
	pUserHeader->GetResponseQueue(&qf);

	if (qf.GetType() != QUEUE_FORMAT_TYPE_UNKNOWN) {
		if (! pXmlEncode->StartTag(cszRev) || ! pXmlEncode->StartTag(cszVia))
			return FALSE;

		if (!QueueFormatToUri(pXmlEncode,&qf))
			return FALSE;

		if (! pXmlEncode->EndTag(cszVia) || ! pXmlEncode->EndTag(cszRev))
			return FALSE;
	}
	return pXmlEncode->EndTag(cszPath);
}


//
// <properties> Section.
//
BOOL SendProps(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage) {
	CUserHeader     *pUserHeader = pImage->sect.pUserHeader;
	WCHAR szBuf[MAX_PATH];
	DWORD dw;

	if (! pXmlEncode->Append((CHAR*)cszPropertiesNS,SVSUTIL_CONSTSTRLEN(cszPropertiesNS)*sizeof(WCHAR)) ||
	    ! pXmlEncode->Append((CHAR*)cszMustUnderstandNS,SVSUTIL_CONSTSTRLEN(cszMustUnderstandNS)*sizeof(WCHAR)))
		return FALSE;

	// <expiresAt>
	dw = GetExpiresFromPacketImage(pImage);
	if (! UtlTimeToIso860Time(dw,szBuf))
		return FALSE;
	
	if (! pXmlEncode->SetElement(cszExpiresAt,szBuf))
		return FALSE;

	// <sentAt>
	if (! UtlTimeToIso860Time(pUserHeader->GetSentTime(),szBuf))
		return FALSE;

	if (! pXmlEncode->SetElement(cszSentAt,szBuf))
		return FALSE;

	return pXmlEncode->EndTag(cszProperties);
}


//
// <Services> Section
//
BOOL SendServices(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage) {
	CPropertyHeader *pPropHeader = pImage->sect.pPropHeader;
	CUserHeader     *pUserHeader = pImage->sect.pUserHeader;

	USHORT            ackType = pPropHeader->GetAckType();
	QUEUE_FORMAT      adminQueue;
	QUEUE_FORMAT_TYPE qType;
	UCHAR             delivery = pUserHeader->GetDelivery();

	pUserHeader->GetAdminQueue(&adminQueue);
	qType = adminQueue.GetType();

	// Do we have required data to fill <services> tag, if not we're done?
	if (delivery != MQMSG_DELIVERY_RECOVERABLE && 
	    (0 == (ackType & (MQMSG_DELIVERY_RECOVERABLE | MQMSG_ACKNOWLEDGMENT_POS_ARRIVAL | MQMSG_ACKNOWLEDGMENT_NEG_RECEIVE)) ||
	    (qType == QUEUE_FORMAT_TYPE_UNKNOWN)))
		return TRUE;
	
	// <services...> root
	if (! pXmlEncode->Append((CHAR*)cszServicesNS,SVSUTIL_CONSTSTRLEN(cszServicesNS)*sizeof(WCHAR)) ||
	    ! pXmlEncode->Append((CHAR*)cszMustUnderstandNS,SVSUTIL_CONSTSTRLEN(cszMustUnderstandNS)*sizeof(WCHAR)))
		return FALSE;

	// <durable>
	if (delivery == MQMSG_DELIVERY_RECOVERABLE) {
		if (! pXmlEncode->SetEmptyElement(cszDurable))
			return FALSE;
	}

	//
	// <deliveryReceiptRequest> section.
	//
	if ((ackType & MQMSG_ACKNOWLEDGMENT_POS_ARRIVAL) && (qType != QUEUE_FORMAT_TYPE_UNKNOWN)) {
		if (! pXmlEncode->StartTag(cszDeliveryReceiptRequest) ||
		    ! pXmlEncode->StartTag(cszSendTo))
			return FALSE;

		if (! QueueFormatToUri(pXmlEncode,&adminQueue))
			return FALSE;

		if (! pXmlEncode->EndTag(cszSendTo) ||
		    ! pXmlEncode->EndTag(cszDeliveryReceiptRequest))
			return FALSE;
	}

	//
	// <commitReceiptRequest> section.
	//
	if (ackType & (MQMSG_ACKNOWLEDGMENT_POS_RECEIVE | MQMSG_ACKNOWLEDGMENT_NEG_RECEIVE) && (qType != QUEUE_FORMAT_TYPE_UNKNOWN)) {
		if (! pXmlEncode->StartTag(cszCommitmentReceiptRequest))
			return FALSE;

		if (ackType & MQMSG_ACKNOWLEDGMENT_POS_RECEIVE) {
			if (! pXmlEncode->SetEmptyElement(cszPositiveOnly))
				return FALSE;
		}

		if (ackType & MQMSG_ACKNOWLEDGMENT_NEG_RECEIVE) {
			if (! pXmlEncode->SetEmptyElement(cszNegativeOnly))
				return FALSE;
		}

		if (! pXmlEncode->StartTag(cszSendTo)          ||
		    ! QueueFormatToUri(pXmlEncode,&adminQueue) ||
		    ! pXmlEncode->EndTag(cszSendTo))
			return FALSE;

		if (! pXmlEncode->EndTag(cszCommitmentReceiptRequest))
			return FALSE;
	}
	
	return pXmlEncode->EndTag(cszServices);
}


//
// <stream> and <stream><start>
BOOL SendStream(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage) {
	CUserHeader     *pUserHeader   = pImage->sect.pUserHeader;
	CXactHeader     *pXactHeader   = pImage->sect.pXactHeader;

	WCHAR szBuf[2048];
	const GUID *pGuid = NULL;

	if (! pUserHeader->IsOrdered())
		return TRUE;

	if (! pXmlEncode->Append((CHAR*)cszStreamNS,SVSUTIL_CONSTSTRLEN(cszStreamNS)*sizeof(WCHAR)) ||
	    ! pXmlEncode->Append((CHAR*)cszMustUnderstandNS,SVSUTIL_CONSTSTRLEN(cszMustUnderstandNS)*sizeof(WCHAR)))
		return FALSE;

	// <streamID>
	pGuid = pUserHeader->GetSourceQM();
	wsprintf(szBuf,L"uri:" GUID_FORMAT L"\\%I64d", GUID_ELEMENTS(pGuid),pXactHeader ? pXactHeader->GetSeqID() : 0);
	if (! pXmlEncode->SetElement(cszStreamId,szBuf))
		return FALSE;

	// <current>
	wsprintf(szBuf,L"%I64d",pXactHeader ? pXactHeader->GetSeqN() : 0);
	if (! pXmlEncode->SetElement(cszCurrent,szBuf))
		return FALSE;

	// <previous>
	wsprintf(szBuf,L"%I64d",pXactHeader ? pXactHeader->GetPrevSeqN() : 0);
	if (! pXmlEncode->SetElement(cszPrevious,szBuf))
		return FALSE;

	//
	// <stream><start>
	//
	if (pXactHeader && (0 == pXactHeader->GetPrevSeqN())) {
		static const WCHAR szOrderQueue[] = MSMQ_SC_PATHNAME_PRIVATE L"/" MSMQ_SC_ORDERQUEUENAME;
		WCHAR *szURI;
		if (! pXmlEncode->StartTag(cszStart))
			return FALSE;

		gMem->Lock();
		const WCHAR *szVroot = gMachine->VRootList[0].wszVRoot ? gMachine->VRootList[0].wszVRoot : cszMSMQVroot;
		// Prints out in format http://ceMachineName/MSMQ\PRIVATE$/order_queue$
		// VROot is of the form "/VRootName/" so don't add extra "/"'s in sprintf.
		wsprintf(szBuf,L"HTTP://%s%s%s",gMachine->lpszHostName,szVroot,szOrderQueue);

		if (! gMachine->RouteLocalReverseLookup((WCHAR*)szBuf,&szURI))
			szURI = szBuf;

		gMem->Unlock();

		if (! pXmlEncode->SetElement(cszSendReceiptsTo,szURI))
			return FALSE;

		if (! pXmlEncode->EndTag(cszStart))
			return FALSE;
	}

	return pXmlEncode->EndTag(cszStream);
}

//
// <streamReceipt> section.
//
BOOL SendStreamRcpt(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage) {
	CUserHeader     *pUserHeader   = pImage->sect.pUserHeader;
	CEodAckHeader   *pEodAckHeader = pImage->sect.pEodAckHeader;
	WCHAR szBuf[MAX_PATH];
	WCHAR *szStreamID = szBuf;
	WCHAR *szTrav;
	BOOL fRet = FALSE;

	if (! pUserHeader->EodAckIsIncluded())
		return TRUE;

	// <streamReceipt>
	if (! pXmlEncode->StartTag(cszStreamReceipt))
		goto done;

	SVSUTIL_ASSERT(pEodAckHeader->GetStreamIdSizeInBytes() % 2 == 0); // should be even since we're storing UNICODE

	if (pEodAckHeader->GetStreamIdSizeInBytes() > sizeof(szBuf)) {
		if (NULL == (szStreamID = (WCHAR*) g_funcAlloc(pEodAckHeader->GetStreamIdSizeInBytes() + 1,g_pvAllocData)))
			goto done;
	}
	wcscpy(szStreamID,(WCHAR*)pEodAckHeader->GetPointerToStreamId());
	szTrav = szStreamID + (pEodAckHeader->GetStreamIdSizeInBytes() / 2);

	if (pEodAckHeader->GetSeqId() != i64NoneMSMQSeqId)
		wsprintf(szTrav,L"\\%I64d",pEodAckHeader->GetSeqId());

	if (!pXmlEncode->SetElement(cszStreamId,szStreamID))
		goto done;

	// <lastOrdinal>
	wsprintf(szBuf,L"%I64d",pEodAckHeader->GetSeqNum());
	if (!pXmlEncode->SetElement(cszlastOrdinal,szBuf))
		goto done;

	if (! pXmlEncode->EndTag(cszStreamReceipt))
		goto done;

	fRet = TRUE;
done:
	if (szStreamID && szStreamID != szBuf)
		g_funcFree(szStreamID,g_pvFreeData);

	return fRet;
}


BOOL SetMsgCurrentTime(SVSXMLEncode *pXmlEncode, const WCHAR *szElementName) {
	WCHAR szBuf[MAX_PATH];

	if (! UtlTimeToIso860Time(scutil_now(),szBuf))
		return FALSE;

	return pXmlEncode->SetElement((WCHAR*)szElementName,szBuf);
}

BOOL SetMsgID(SVSXMLEncode *pXmlEncode, OBJECTID *pMsgId) {
	WCHAR szBuf[MAX_PATH];
	const GUID *pGuid = &pMsgId->Lineage;

	wsprintf(szBuf,UUIDREFERENCE_PREFIX L"%d" UUIDREFERENCE_SEPERATOR GUID_FORMAT,
	         pMsgId->Uniquifier,GUID_ELEMENTS(pGuid));

	return pXmlEncode->SetElement(cszId,szBuf);
}

//
// <deliveryReceipt> section
//
BOOL SendDeliveryRcpt(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage) {
	CPropertyHeader *pPropHeader   = pImage->sect.pPropHeader;

	// cszDeliveryReceipt
	if (pPropHeader->GetClass() != MQMSG_CLASS_ACK_REACH_QUEUE)
		return TRUE;

	if (! pXmlEncode->StartTag(cszDeliveryReceipt))
		return FALSE;
	
	// <receivedAt>
	if (! SetMsgCurrentTime(pXmlEncode,cszreceivedAt))
		return FALSE;

	// <id>
	if (! SetMsgID(pXmlEncode,(OBJECTID*)pPropHeader->GetCorrelationID()))
		return FALSE;

	return pXmlEncode->EndTag(cszDeliveryReceipt);
}



//
// <commitReceipt> section
//
BOOL SendCommitRcpt(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage) {
	CPropertyHeader *pPropHeader   = pImage->sect.pPropHeader;

	if (!MQCLASS_RECEIVE(pPropHeader->GetClass()))
		return TRUE;

	if (! pXmlEncode->StartTag(cszCommitmentReceipt))
		return FALSE;

	// <decidedAt>
	if (!SetMsgCurrentTime(pXmlEncode,cszDecidedAt))
		return FALSE;

	// <decision>
	const WCHAR* szDecideValue = MQCLASS_POS_RECEIVE(pPropHeader->GetClass()) ? cszPositive : cszNegative;
	if (! pXmlEncode->SetElement(cszDecision,szDecideValue))
		return FALSE;

	// <id>
	if (! SetMsgID(pXmlEncode,(OBJECTID*)pPropHeader->GetCorrelationID()))
		return FALSE;
	
	return pXmlEncode->EndTag(cszCommitmentReceipt);
}


//
// <msmq> section
//
// no <Provider> element since there's no security.
// no <DestinationMqf>, <AdminMqf>, <ResponseMqf> since there's no multiqueues.
//
BOOL SendMsmq(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage) {
	CPropertyHeader *pPropHeader   = pImage->sect.pPropHeader;
	CUserHeader     *pUserHeader   = pImage->sect.pUserHeader;
	CBaseHeader     *pBaseHeader   = pImage->sect.pBaseHeader;
	CXactHeader     *pXactHeader   = pImage->sect.pXactHeader;
	WCHAR           szBuf[MAX_PATH];
	UCHAR           auditing       = pUserHeader->GetAuditing();
	WCHAR           *szCorrelation;
	DWORD           dw;
	const GUID      *pGuid;

	if (! pXmlEncode->Append((CHAR*)cszMsmqNS,SVSUTIL_CONSTSTRLEN(cszMsmqNS)*sizeof(WCHAR)))
		return FALSE;

	wsprintf(szBuf,L"%d",pPropHeader->GetClass());
	if (! pXmlEncode->SetElement(cszClass,szBuf))
		return FALSE;

	wsprintf(szBuf,L"%d",pBaseHeader->GetPriority());
	if (! pXmlEncode->SetElement(cszPriority,szBuf))
		return FALSE;

	if (auditing & MQMSG_JOURNAL) {
		if (!pXmlEncode->SetEmptyElement(cszJournal))
			return FALSE;
	}

	if (auditing & MQMSG_DEADLETTER) {
		if (!pXmlEncode->SetEmptyElement(cszDeadLetter))
			return FALSE;
	}

	if (NULL == (szCorrelation = Octet2Base64W(pPropHeader->GetCorrelationID(),PROPID_M_CORRELATIONID_SIZE,&dw)))
		return FALSE;

	if (! pXmlEncode->SetElement(cszCorrelation,szCorrelation)) {
		g_funcFree(szCorrelation,g_pvFreeData);
		return FALSE;
	}
	g_funcFree(szCorrelation,g_pvFreeData);

	if (pBaseHeader->GetTraced() & MQMSG_SEND_ROUTE_TO_REPORT_QUEUE) {
		if (! pXmlEncode->SetEmptyElement(cszTrace))
			return FALSE;
	}

	if (pUserHeader->ConnectorTypeIsIncluded()) {
		pGuid = pUserHeader->GetConnectorType();
		wsprintf(szBuf,GUID_FORMAT,GUID_ELEMENTS(pGuid));
		if (! pXmlEncode->SetElement(cszConnectorType,szBuf))
			return FALSE;
	}

	wsprintf(szBuf,L"%d",pPropHeader->GetApplicationTag());
	if (! pXmlEncode->SetElement(cszApp,szBuf))
		return FALSE;

	wsprintf(szBuf,L"%d",pPropHeader->GetBodyType());
	if (! pXmlEncode->SetElement(cszBodyType,szBuf))
		return FALSE;

	wsprintf(szBuf,L"%d",pPropHeader->GetHashAlg());
	if (! pXmlEncode->SetElement(cszHashAlgorithm,szBuf))
		return FALSE;

	// <eod> section
	if (pUserHeader->IsOrdered()) {
		if (! pXmlEncode->StartTag(cszEod))
			return FALSE;
		
		if (pXactHeader->ConnectorQMIncluded()) {
			pGuid = pXactHeader->GetConnectorQM();
			wsprintf(szBuf,GUID_FORMAT,GUID_ELEMENTS(pGuid));
			if (! pXmlEncode->SetElement(cszXConnectorId,szBuf))
				return FALSE;
		}

		if (pXactHeader->GetFirstInXact()) {
			if (! pXmlEncode->SetEmptyElement(cszFirst))
				return FALSE;
		}

		if (pXactHeader->GetLastInXact()) {
			if (! pXmlEncode->SetEmptyElement(cszLast))
				return FALSE;
		}

		if (! pXmlEncode->EndTag(cszEod))
			return FALSE;
	}

	pGuid = pUserHeader->GetSourceQM();
	wsprintf(szBuf,GUID_FORMAT,GUID_ELEMENTS(pGuid));
	if (! pXmlEncode->SetElement(cszSourceQmGuid,szBuf))
		return FALSE;

	dw = min(pBaseHeader->GetAbsoluteTimeToQueue(), LONG_MAX);
	if (! UtlTimeToIso860Time(dw,szBuf) || 
	    ! pXmlEncode->SetElement(cszTTrq,szBuf))
		return FALSE;
	
	return pXmlEncode->EndTag(cszMsmq);
}


// Adds elements set by user.
BOOL SendUserHeader(SVSXMLEncode *pXmlEncode, ScPacketImage *pImage) {
	CUserHeader     *pUserHeader   = pImage->sect.pUserHeader;
	CSoapSection    *pSoapSection  = pImage->sect.pSoapHeaderSection;

	if (! pUserHeader->SoapIsIncluded() || !pSoapSection || !pSoapSection->GetPointerToData())
		return TRUE;

	// do *not* encode the soap section, as it contains XML tags in it already.
	// It's application's responsibility to make sure all is encoded correctly, just
	// as on the desktop.

	// GetDataLengthInWCHARs() returns strlen+1 for \0, don't append this to buffer.
	return pXmlEncode->Append((CHAR*)pSoapSection->GetPointerToData(),(pSoapSection->GetDataLengthInWCHARs()-1)*sizeof(WCHAR));
}
