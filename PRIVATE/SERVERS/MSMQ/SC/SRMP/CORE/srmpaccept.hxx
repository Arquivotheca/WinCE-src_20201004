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

    SrmpAccept.hxx

Abstract:

    Convert SRMP packet to MSMQ packet.


--*/


#if ! defined (__srmpaccept_HXX__)
#define __srmpaccept_HXX__	1

#include <windows.h>
#include <svsutil.hxx>
#include <oleauto.h>

#include "scsrmp.hxx"
#include "SrmpMsg.hxx"
#include "SrmpUtil.hxx"

class CSrmpMessageProperties;

typedef struct _SRMP_STATE_INFO {
	SRMP_STATE state;
	DWORD      dwStateFlags;
} SRMP_STATE_INFO;

extern BOOL g_fUseWininet;

SRMP_TOKEN GetToken(const WCHAR *szElement, const DWORD chElement);

PSTR FindMimeBoundary(PSTR pszHttpHeaders, BOOL *pfBadRequest=NULL);
BOOL ReadSoapEnvelopeFromMIME(PSTR pszBoundary, DWORD ccBoundary, PSTR pszPost, DWORD cbPost, PSTR *ppszStartEnv, PSTR *ppszEndEnv);
BOOL UriToQueueFormat(const WCHAR *szQueue, DWORD dwQueueChars, QUEUE_FORMAT *pQueue, WCHAR **ppszQueueBuffer);
	
class CSrmpToMsmq : public SVSSAXContentHandler {
private:
	BOOL ConvertXML(void);
	BOOL ConvertMIME(void);
	PSTR ReadMIMESection(PSTR pszMime);

	// SAX parsing related
	SVSSimpleBuffer    m_Chars;        // holds character data as it is passed in
	SVSStack           m_State;
	FixedMemDescr      *m_pfmdState;
	
	CSrmpMessageProperties  cMsgProps;
	DWORD                   dwStatusCode;  // HTTP code returned to client.

	BOOL         MustUnderstand(ISAXAttributes *pAttributes);

	SRMP_STATE   GetState(void)             { SRMP_STATE_INFO *st = (SRMP_STATE_INFO *)m_State.Peek(); return st->state; }
	DWORD        GetStateFlags(void)        { SRMP_STATE_INFO *st = (SRMP_STATE_INFO *)m_State.Peek(); return st->dwStateFlags; }
	void         PopState(void)             { SRMP_STATE_INFO *st = (SRMP_STATE_INFO *)m_State.Pop(); svsutil_FreeFixed(st,m_pfmdState); }
	BOOL         SetState(SRMP_TOKEN token, const WCHAR * szNS, const DWORD cbNS);
	BOOL         SetState(SRMP_STATE state, DWORD dwFlags);
	DWORD        GetStateFlags(SRMP_STATE state);
	// endElement appends a L'\0' to the buffer always, take into account in determing if an element is empty.
	BOOL         IsEmpty(void)              { return (m_Chars.uiNextIn == sizeof(WCHAR)); }

	WCHAR *      AllocCharData(DWORD dwOffset=0) {
		WCHAR *szBuf = (WCHAR*) m_Chars.pBuffer;
		WCHAR *sz = svsutil_wcsdup(szBuf+dwOffset);
		if (!sz)
			SetInternalError();
		return sz;
	}

	// m_Chars.pBuffer is defined as a CHAR *, however we're storing UNICODE chars in it.
	WCHAR * GetBuffer()   { return ((WCHAR*) m_Chars.pBuffer); }
	DWORD   GetNumChars() { return ((m_Chars.uiNextIn)/sizeof(WCHAR)-1); }

	HRESULT EndPath(SRMP_STATE endState);
	HRESULT EndProperties(SRMP_STATE endState);
	HRESULT EndServices(SRMP_STATE endState);
	HRESULT EndStream(SRMP_STATE endState);
	HRESULT EndStreamRcpt(SRMP_STATE endState);
	HRESULT EndDeliveryRcpt(SRMP_STATE endState);
	HRESULT EndCommitmentRcpt(SRMP_STATE endState);
	HRESULT EndMsmq(SRMP_STATE endState);
	HRESULT EndSignature(SRMP_STATE endState);

	BOOL PutMessageInQueue(ScPacketImage *pPacketImage);

	// ISAXContentHandler
	virtual HRESULT STDMETHODCALLTYPE startElement(const wchar_t * pwchNamespaceUri, int cchNamespaceUri, const wchar_t * pwchLocalName, 
	                     int cchLocalName, const wchar_t * pwchQName, int cchQName, ISAXAttributes * pAttributes);
	virtual HRESULT STDMETHODCALLTYPE endElement(const wchar_t  *pwchNamespaceUri, int cchNamespaceUri, const wchar_t  *pwchLocalName,
	                                             int cchLocalName, const wchar_t  *pwchQName, int cchQName);
	virtual HRESULT STDMETHODCALLTYPE characters(const wchar_t  *pwchChars, int cchChars);

public:
	// Put the message together
	HRESULT HandlePath(void);
	HRESULT HandlePathId(void);
	HRESULT HandlePathTo(void);
	HRESULT HandlePathRev(void);
	HRESULT HandlePathRevVia(void);
	HRESULT HandlePathFrom(void);
	HRESULT HandlePathAction(void);
	HRESULT HandlePathRelatesTo(void);
	HRESULT HandlePathFixed(void);
	HRESULT HandlePathFwd(void);
	HRESULT HandlePathFwdVia(void);
	HRESULT HandlePathFault(void);

	HRESULT HandleServices(void);
	HRESULT HandleServicesDurable(void);
	HRESULT HandleServicesDelRcptRequest(void);
	HRESULT HandleServicesDelRcptRequestSendTo(void);
	HRESULT HandleServicesDelRcptRequestSendBy(void);
	HRESULT HandleServicesFilterDups(void);
	HRESULT HandleServicesCommitRcpt(void);
	HRESULT HandleServicesCommitRcptPositevOnly(void);
	HRESULT HandleServicesCommitRcptNegativeOnly(void);
	HRESULT HandleServicesCommitRcptSendBy(void);
	HRESULT HandleServicesCommitRcptSendTo(void);

	HRESULT HandleProps(void);
	HRESULT HandlePropsExpiresAt(void);
	HRESULT HandlePropsDuration(void);
	HRESULT HandlePropsSentAt(void);
	HRESULT HandlePropsInReplyTo(void);

	HRESULT HandleStream(void);
	HRESULT HandleStreamStreamID(void);
	HRESULT HandleStreamCurrent(void);
	HRESULT HandleStreamPrevious(void);
	HRESULT HandleStreamEnd(void);
	HRESULT HandleStreamStart(void);
	HRESULT HandleStreamStartSendRcptTo(void);
	HRESULT HandleStreamStartExpiresAt(void);
	HRESULT HandleStreamStreamRcptRequest(void);

	HRESULT HandleStreamRcpt(void);
	HRESULT HandleStreamRcptRecvdAt(void);
	HRESULT HandleStreamRcptStreamId(void);
	HRESULT HandleStreamRcptLastOrdinal(void);
	HRESULT HandleStreamRcptId(void);

	HRESULT HandleDeliveryRcpt(void);
	HRESULT HandleDeliveryRcptReceivedAt(void);
	HRESULT HandleDeliveryRcptId(void);

	HRESULT HandleCommitRcpt(void);
	HRESULT HandleCommitRcptDecidedAt(void);
	HRESULT HandleCommitRcptDecision(void);
	HRESULT HandleCommitRcptId(void);
	HRESULT HandleCommitRcptCommiteCode(void);
	HRESULT HandleCommitRcptXCommitDetail(void);

	HRESULT HandleMsmq(void);
	HRESULT HandleMsmqClass(void);
	HRESULT HandleMsmqPrio(void);
	HRESULT HandleMsmqJournal(void);
	HRESULT HandleMsmqDeadLetter(void);
	HRESULT HandleMsmqCorrelation(void);
	HRESULT HandleMsmqTrace(void);
	HRESULT HandleMsmqConnectorType(void);
	HRESULT HandleMsmqApp(void);
	HRESULT HandleMsmqBodyType(void);
	HRESULT HandleMsmqHashAlg(void);
	HRESULT HandleMsmqEod(void);
	HRESULT HandleMsmqEodFirst(void);
	HRESULT HandleMsmqEodLast(void);
	HRESULT HandleMsmqEodXConnectorId(void);
	HRESULT HandleMsmqProvider(void);
	HRESULT HandleMsmqProviderType(void);
	HRESULT HandleMsmqProviderName(void);
	HRESULT HandleMsmqSourceQMGuid(void);
	HRESULT HandleMsmqDestinationMQF(void);
	HRESULT HandleMsmqAdminMQF(void);
	HRESULT HandleMsmqResponseMQF(void);
	HRESULT HandleMsmqTTRQ(void);

public:
	CSrmpToMsmq(PSrmpIOCTLPacket pIOCTL) : cMsgProps(pIOCTL) {
		m_pfmdState = svsutil_AllocFixedMemDescr(sizeof(SRMP_STATE_INFO), 10);
		if (!m_pfmdState)
			return;

		SetState(SRMP_ST_UNINITIALIZED,GetStateFlags(SRMP_ST_UNINITIALIZED));
		SetInternalError(); // reset to 200 at end of request.
	}

	BOOL IsInitialized(void) { return (cMsgProps.IsInitialized() && m_pfmdState) ? TRUE : FALSE; }

	~CSrmpToMsmq() {
		while (! m_State.IsEmpty())
			PopState();

		if (m_pfmdState)
			svsutil_ReleaseFixedNonEmpty(m_pfmdState);
	}

	BOOL ParseEnvelopeAndAttachments(void);
	BOOL CreateAndInsertPacket(void);

	inline BOOL ParseMimeType(void) {
		if (cMsgProps.pHttpParams->contentType == CONTENT_TYPE_XML)
			return UTF8ToBSTR(cMsgProps.pHttpParams->pszPost, &cMsgProps.bstrEnvelope);
		else
			return ConvertMIME();
	}

	DWORD GetStatusCode(void)      { return dwStatusCode; }
	void  SetStatusCode(DWORD dw)  { dwStatusCode = dw;   }
	void  SetSuccess(void)         { dwStatusCode = 200;  }
	void  SetBadRequest(void)      { dwStatusCode = 400;  }
	void  SetInternalError(void)   { dwStatusCode = 500;  }
	void  SetUnavailable(void)     { dwStatusCode = 503;  } // Server temporarily unavailable, when MSMQ subsystem isn't running.
};
#endif
