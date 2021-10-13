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

    SrmpMsg.cxx

Abstract:

	After SAX interpretter is through with an item, calls into
	this function to process it.
     
--*/


#include <msxml2.h>
#include <winsock.h>

#include "SrmpAccept.hxx"
#include "SrmpParse.hxx"
#include "sc.hxx"
#include "MqProps.h"
#include "ph.h"
#include "scqman.hxx"
#include "scqueue.hxx"
#include "fntoken.h"
#include <service.h>


//
//  Interface functions.
//
BOOL BreakMsmqStreamId(const WCHAR *szBuffer, LONGLONG *pSeqId);


// Make sure an end node is visited one and only one time, mark it off as visited.
#define PathCheckAndSet(state)            CheckAndSet(cMsgProps.dwPathSet,   (state))
#define ServiceCheckAndSet(state)         CheckAndSet(cMsgProps.dwServiceSet,(state))
#define PropsCheckAndSet(state)           CheckAndSet(cMsgProps.dwPropertiesSet,(state))
#define StreamCheckAndSet(state)          CheckAndSet(cMsgProps.dwStreamSet,(state))
#define StreamRcptCheckAndSet(state)      CheckAndSet(cMsgProps.dwStreamRcptSet,(state))
#define DeliveryRcptCheckAndSet(state)    CheckAndSet(cMsgProps.dwDeliveryRcptSet,(state))
#define CommitRcptCheckAndSet(state)      CheckAndSet(cMsgProps.dwCommitRcptSet,(state))
#define MsmqRcptCheckAndSet(state)        CheckAndSet(cMsgProps.dwMsmqSet,(state))

CSrmpMessageProperties::CSrmpMessageProperties(PSrmpIOCTLPacket pIOCTL) {
	pHttpParams = pIOCTL;
	SVSUTIL_ASSERT(pHttpParams->contentType == CONTENT_TYPE_XML || pHttpParams->contentType == CONTENT_TYPE_MIME);

	absoluteTimeToLive   = INFINITE;
	absoluteTimeToQueue  = INFINITE;
	priority             = DEFAULT_M_PRIORITY;
	acknowledgeType      = DEFAULT_M_ACKNOWLEDGE;
	EodSeqId             = i64NoneMSMQSeqId;

	szAdminQueue = szResponseQueue = szDestQueue = szOrderQueue = szStreamIdBuf = szTitle = NULL;
	bstrEnvelope = NULL;

	fTrace = fLast = fFirst = auditing = delivery = 0;
	sentTime = 0;

	classValue = 0;
	bodyType = applicationTag = 0;
	pCorrelationID = 0;

	EodStreamId = 0;
	EodPrevSeqNo = EodSeqNo = 0;

	EodAckStreamId = 0;
	EodAckSeqId    = EodAckSeqNo = 0;

	szFrom = szRelatesTo  = 0;
	ccRelatesTo = ccFrom  = 0;

	// Keep track of what vars have been set already during SAX processing.
	dwHeaderSet = dwPathSet = dwServiceSet = dwPropertiesSet = dwStreamSet = dwStreamRcptSet =
	dwDeliveryRcptSet =  dwCommitRcptSet = dwMsmqSet = 0;

	memset(&messageId,0,sizeof(messageId));
	memset(&connectorType,0,sizeof(connectorType));
	memset(&connectorId,0,sizeof(connectorId));
	memset(&SourceQmGuid,0,sizeof(SourceQmGuid));
	memset(&destQueue,0,sizeof(destQueue));
	memset(&adminQueue,0,sizeof(adminQueue));
	memset(&responseQueue,0,sizeof(responseQueue));
}

CSrmpMessageProperties::~CSrmpMessageProperties() {
	if (szTitle)
		g_funcFree(szTitle, g_pvFreeData);

	if (szStreamIdBuf)
		g_funcFree(szStreamIdBuf,g_pvFreeData);

	if (szOrderQueue)
		g_funcFree(szOrderQueue,g_pvFreeData);

//	if (EodStreamId)  Do not free!  Points into szStreamIdBuf!
//		g_funcFree(EodStreamId,g_pvFreeData);

	if (pCorrelationID)
		g_funcFree(pCorrelationID,g_pvFreeData);

	if (EodAckStreamId)
		g_funcFree(EodAckStreamId,g_pvFreeData);

	if (szRelatesTo)
		g_funcFree(szRelatesTo,g_pvFreeData);

	if (szFrom)
		g_funcFree(szFrom,g_pvFreeData);

	if (bstrEnvelope)
		SysFreeString(bstrEnvelope);

	if (szDestQueue)
		g_funcFree(szDestQueue,g_pvFreeData);

	if (szResponseQueue)
		g_funcFree(szResponseQueue,g_pvFreeData);

	if (szAdminQueue)
		g_funcFree(szAdminQueue,g_pvFreeData);

	//if (szProviderName)
	//	g_funcFree(szProviderName,g_pvFreeData);
}


//
//  <path>
// 
HRESULT CSrmpToMsmq::HandlePath(void) {
	if (IsHeaderSet(SRMP_ST_PATH))
		return E_FAIL;
	SetHeader(SRMP_ST_PATH);

	if (! IsSetSrmp(cMsgProps.dwPathSet,SRMP_ST_PATH_ID) ||
	    ! IsSetSrmp(cMsgProps.dwPathSet,SRMP_ST_PATH_ACTION)) {
		return E_FAIL;
	}

	// if <to> isn't set, spec says 'ultimate destination is indicated by the last "via" in the "fwd" element'
	if (! IsSetSrmp(cMsgProps.dwPathSet,SRMP_ST_PATH_TO)) {
		if (! cMsgProps.fwdViaBuf.pBuffer)
			return E_FAIL;

		WCHAR *szStart = (WCHAR*) cMsgProps.fwdViaBuf.pBuffer;
		int   ccChars  = cMsgProps.fwdViaBuf.uiNextIn / sizeof(WCHAR);
		WCHAR *pszTrav = szStart;
		WCHAR *pszLastVisited;

		while (pszTrav - szStart <= ccChars) {
			pszLastVisited = pszTrav;
			pszTrav += wcslen(pszTrav)+1;
		}
		SVSUTIL_ASSERT(pszTrav-1-szStart == ccChars);

		if (! UriToQueueFormat(pszLastVisited,pszLastVisited-pszTrav-1,&cMsgProps.destQueue,&cMsgProps.szDestQueue))
			return E_FAIL;
		SetSrmp(cMsgProps.dwPathSet,SRMP_ST_PATH_TO);
	}
	
	return S_OK;
}

// Converts <path><id>uuid:XXXXX</id></path>
HRESULT CSrmpToMsmq::HandlePathId(void) {
	PathCheckAndSet(SRMP_ST_PATH_ID);
	WCHAR *szBuf = GetBuffer();

	if (IsEmpty())
		return E_FAIL;

	DWORD nscan;
	int n = swscanf(szBuf,UUIDREFERENCE_PREFIX L"%d" UUIDREFERENCE_SEPERATOR L"%n",
	                &cMsgProps.messageId.Uniquifier,&nscan);

	if (n == 1) {
		if (! StringToGuid(szBuf + nscan,&cMsgProps.messageId.Lineage))
			return E_FAIL;
		return S_OK;
	}

	const GUID  xNonMSMQMessageIdGuid  = {0x75d1aae4,0x8e23,0x400a,0x8c,0x33,0x99,0x64,0x8e,0x35,0xe7,0xb9};
	const ULONG xNonMSMQMessageIdIndex = 1;

	cMsgProps.messageId.Uniquifier = xNonMSMQMessageIdIndex;
	cMsgProps.messageId.Lineage    = xNonMSMQMessageIdGuid;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandlePathTo(void) {
	PathCheckAndSet(SRMP_ST_PATH_TO);
	if (IsEmpty())
		return E_FAIL;

	return UriToQueueFormat(GetBuffer(),GetNumChars(),&cMsgProps.destQueue,&cMsgProps.szDestQueue) ? S_OK : E_FAIL;
}

HRESULT CSrmpToMsmq::HandlePathRev(void) {
	PathCheckAndSet(SRMP_ST_PATH_REV);
	return S_OK;
}

const WCHAR szEmpty[] = L"";

HRESULT CSrmpToMsmq::HandlePathRevVia(void) {
	// It is acceptable to have multiple VIA entries
	SetSrmp(cMsgProps.dwPathSet,SRMP_ST_PATH_REV_VIA);
	const WCHAR *szVia = GetBuffer();
	DWORD ccVia        = GetNumChars();

	// 1st element in <rev><via> list has special meaning in desktop MSMQ, preserve this while at same time store in revViaBuf.
	if (! IsEmpty() && (NULL == cMsgProps.revViaBuf.pBuffer)) {
		if (!UriToQueueFormat(GetBuffer(),GetNumChars(),&cMsgProps.responseQueue,&cMsgProps.szResponseQueue))
			return E_FAIL;
	}

	// +1 makes sure \0 is included.
	return cMsgProps.revViaBuf.AppendWSTR(szVia,ccVia+1) ? S_OK : E_FAIL;
}

HRESULT CSrmpToMsmq::HandlePathFrom(void) {
	PathCheckAndSet(SRMP_ST_PATH_FROM);
	if (IsEmpty())
		return E_FAIL;

	if (NULL == (cMsgProps.szFrom = AllocCharData()))
		return E_FAIL;

	cMsgProps.ccFrom = GetNumChars() + 1;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandlePathAction(void) {
	PathCheckAndSet(SRMP_ST_PATH_ACTION);
	// Like XP: We require action to begin with "MSMQ:", if it doesn't we 
	// won't save it but we won't halt parsing, either.
	if (GetNumChars() <= ccMSMQPrefix)
		return S_OK;

	if (wcsncmp(GetBuffer(),cszMSMQPrefix,ccMSMQPrefix) != 0)
		return S_OK;

	if (NULL == (cMsgProps.szTitle = AllocCharData(ccMSMQPrefix)))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandlePathRelatesTo(void) {
	PathCheckAndSet(SRMP_ST_PATH_RELATESTO);
	if (IsEmpty())
		return E_FAIL;

	if (NULL == (cMsgProps.szRelatesTo = AllocCharData()))
		return E_FAIL;

	cMsgProps.ccRelatesTo = GetNumChars() + 1;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandlePathFixed(void) {
	PathCheckAndSet(SRMP_ST_PATH_FIXED);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandlePathFwd(void) {
	PathCheckAndSet(SRMP_ST_PATH_FWD);
	return S_OK;
}

// need to have a URL and a FORMAT version of this...
//int IsURLLocalMachine(const WCHAR *szURL, BOOL fGeneratedLocally) {
//	if (0 == wcsicmp(gMachine->lpszHostName,szURL))
//		return TRUE;
//
//	if (fGeneratedLocally && (0==wcsicmp(gMachine->lpszHostName,L"localhost")))
//		return TRUE;
//
//	return FALSE;
//}

HRESULT CSrmpToMsmq::HandlePathFwdVia(void) {
	// It is acceptable to have multiple VIA entries
//	BOOL  fFirst       = ! (IsSetSrmp(cMsgProps.dwPathSet,SRMP_ST_PATH_FWD_VIA));
	const WCHAR *szVia = GetBuffer();
	DWORD ccVia        = GetNumChars();

	SetSrmp(cMsgProps.dwPathSet,SRMP_ST_PATH_FWD_VIA);
/*	if (fFirst) {
		// check that 1st element is local device or is empty.
//		if (!IsEmpty() && !IsURLLocalMachine(szVia))
//			return E_FAIL;

		// first <via> element is discarded, return immediatly.
		return S_OK;
	}
*/

	// +1 to make sure NULL is included.
	return cMsgProps.fwdViaBuf.AppendWSTR(szVia,ccVia+1) ? S_OK : E_FAIL;
}

HRESULT CSrmpToMsmq::HandlePathFault(void) {
	PathCheckAndSet(SRMP_ST_PATH_FAULT);
	return S_OK;
}

//
// <services>
// 
HRESULT CSrmpToMsmq::HandleServices(void) {
	if (IsHeaderSet(SRMP_ST_SERVICES))
		return E_FAIL;
	SetHeader(SRMP_ST_SERVICES);

	return S_OK;
}

HRESULT CSrmpToMsmq::HandleServicesDurable(void) {
	ServiceCheckAndSet(SRMP_ST_SERVICES_DURABLE);
	cMsgProps.delivery = MQMSG_DELIVERY_RECOVERABLE;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleServicesFilterDups(void) {
	ServiceCheckAndSet(SRMP_ST_SERVICES_FILTERDUPLICATES);
	return S_OK;
}


//
// <services><commitmentReceiptRequest>
//
HRESULT CSrmpToMsmq::HandleServicesCommitRcpt(void) {
	ServiceCheckAndSet(SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST);

	if (! IsSetSrmp(cMsgProps.dwServiceSet,SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_SENDTO))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandleServicesCommitRcptPositevOnly(void) {
	ServiceCheckAndSet(SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_POSITIVEONLY);
	cMsgProps.acknowledgeType |= MQMSG_ACKNOWLEDGMENT_POS_RECEIVE;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleServicesCommitRcptNegativeOnly(void) {
	ServiceCheckAndSet(SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_NEGATIVEONLY);
	cMsgProps.acknowledgeType |= MQMSG_ACKNOWLEDGMENT_NEG_RECEIVE;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleServicesCommitRcptSendBy(void) {
	ServiceCheckAndSet(SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_SENDBY);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleServicesCommitRcptSendTo(void) {
	ServiceCheckAndSet(SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_SENDTO);

	if (!UriToQueueFormat(GetBuffer(),GetNumChars(),&cMsgProps.adminQueue,&cMsgProps.szAdminQueue))
		return E_FAIL;
	return S_OK;
}

//
// <services><deliveryReceiptRequest>
//
HRESULT CSrmpToMsmq::HandleServicesDelRcptRequest(void) {
	ServiceCheckAndSet(SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST);
	
	if (! IsSetSrmp(cMsgProps.dwServiceSet,SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST_SENDTO))
		return E_FAIL;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleServicesDelRcptRequestSendTo(void) {
	ServiceCheckAndSet(SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST_SENDTO);

	cMsgProps.acknowledgeType |= MQMSG_ACKNOWLEDGMENT_POS_ARRIVAL;
	if (!UriToQueueFormat(GetBuffer(),GetNumChars(),&cMsgProps.adminQueue,&cMsgProps.szAdminQueue))
		return E_FAIL;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleServicesDelRcptRequestSendBy(void) {
	ServiceCheckAndSet(SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST_SENDBY);
	return S_OK;
}

//
// <Properties>
//
HRESULT CSrmpToMsmq::HandleProps(void) {
	if (IsHeaderSet(SRMP_ST_PROPERTIES))
		return E_FAIL;
	SetHeader(SRMP_ST_PROPERTIES);

	// Note: XP MSMQ router does not check for expiresAt, but latest version of spec requires it.
	if (! IsSetSrmp(cMsgProps.dwPropertiesSet,SRMP_ST_PROPERTIES_EXPIRESAT))
		return E_FAIL;
	
	return S_OK;
}

HRESULT CSrmpToMsmq::HandlePropsExpiresAt(void) {
	PropsCheckAndSet(SRMP_ST_PROPERTIES_EXPIRESAT);
	if (IsEmpty())
		return E_FAIL;

   	SYSTEMTIME SysTime;
	if (! UtlIso8601TimeToSystemTime(GetBuffer(),GetNumChars(),&SysTime))
		return E_FAIL;

	if (! UtlSystemTimeToCrtTime(&SysTime,&cMsgProps.absoluteTimeToLive))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandlePropsDuration(void) {
	PropsCheckAndSet(SRMP_ST_PROPERTIES_DURATION);
	if (IsEmpty())
		return E_FAIL;

	DWORD dwRelativeTimeOut;
	if (!UtlIso8601TimeDuration(GetBuffer(),GetNumChars(),&dwRelativeTimeOut))
		return E_FAIL;

	DWORD dwAbsoluteTimeout = scutil_now() + dwRelativeTimeOut;
	if (dwAbsoluteTimeout < dwRelativeTimeOut)
		cMsgProps.absoluteTimeToLive = INFINITE;
	else
		cMsgProps.absoluteTimeToLive = dwAbsoluteTimeout;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandlePropsSentAt(void) {
	PropsCheckAndSet(SRMP_ST_PROPERTIES_SENTAT);
	if (IsEmpty())
		return E_FAIL;

   	SYSTEMTIME SysTime;
	if (! UtlIso8601TimeToSystemTime(GetBuffer(),GetNumChars(),&SysTime))
		return E_FAIL;

	if (! UtlSystemTimeToCrtTime(&SysTime,&cMsgProps.sentTime))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandlePropsInReplyTo(void) {
	PropsCheckAndSet(SRMP_ST_PROPERTIES_INREPLYTO); // empty by design.
	return S_OK;
}

//
// <Stream>
//
HRESULT CSrmpToMsmq::HandleStream(void) {
	if (IsHeaderSet(SRMP_ST_STREAM))
		return E_FAIL;
	SetHeader(SRMP_ST_STREAM);

	if (! IsSetSrmp(cMsgProps.dwStreamSet,SRMP_ST_STREAM_STREAMID) ||
	    ! IsSetSrmp(cMsgProps.dwStreamSet,SRMP_ST_STREAM_CURRENT))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandleStreamStreamID(void) {
	StreamCheckAndSet(SRMP_ST_STREAM_STREAMID);
	if (IsEmpty())
		return E_FAIL;

	// Only allocate for now.  How we process depends on whether on not
	// MSMQ information is in this packet, which we may not know at time of parse.
	if (NULL == (cMsgProps.szStreamIdBuf = AllocCharData()))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandleStreamCurrent(void) {
	StreamCheckAndSet(SRMP_ST_STREAM_CURRENT);
	if (IsEmpty())
		return E_FAIL;

	cMsgProps.EodSeqNo = _wtoi((WCHAR*) m_Chars.pBuffer);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleStreamPrevious(void) {
	StreamCheckAndSet(SRMP_ST_STREAM_PREVIOUS);
	if (IsEmpty())
		return E_FAIL;

	cMsgProps.EodPrevSeqNo = _wtoi((WCHAR*) m_Chars.pBuffer);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleStreamEnd(void) {
	StreamCheckAndSet(SRMP_ST_STREAM_END);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleStreamStreamRcptRequest(void) {
	StreamCheckAndSet(SRMP_ST_STREAM_STREAMRECEIPTREQUEST);
	return S_OK;
}

// <Stream><start>
HRESULT CSrmpToMsmq::HandleStreamStart(void) {
	StreamCheckAndSet(SRMP_ST_STREAM_START);
	if (! IsSetSrmp(cMsgProps.dwStreamSet,SRMP_ST_STREAM_START_SENDRECEIPTSTO))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandleStreamStartSendRcptTo(void) {
	StreamCheckAndSet(SRMP_ST_STREAM_START_SENDRECEIPTSTO);
	if (IsEmpty())
		return E_FAIL;

	if (NULL == (cMsgProps.szOrderQueue = (WCHAR*) g_funcAlloc((GetNumChars()+FN_DIRECT_TOKEN_LEN+2)*sizeof(WCHAR),g_pvAllocData))) {
		SetInternalError();
		return E_FAIL;
	}

	wsprintf(cMsgProps.szOrderQueue,L"%s=%s",FN_DIRECT_TOKEN,GetBuffer());
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleStreamStartExpiresAt(void) {
	StreamCheckAndSet(SRMP_ST_STREAM_START_EXPIRESAT);
	return S_OK;
}

//
// <streamReceipt>
//
HRESULT CSrmpToMsmq::HandleStreamRcpt(void) {
	if (IsHeaderSet(SRMP_ST_STREAMRCPT))
		return E_FAIL;
	SetHeader(SRMP_ST_STREAMRCPT);

	if (! IsSetSrmp(cMsgProps.dwStreamRcptSet,SRMP_ST_STREAMRCPT_ID) ||
	    ! IsSetSrmp(cMsgProps.dwStreamRcptSet,SRMP_ST_STREAMRCPT_LASTORDINAL))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandleStreamRcptRecvdAt(void) {
	StreamRcptCheckAndSet(SRMP_ST_STREAMRCPT_RECEIVEDAT);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleStreamRcptStreamId(void) {
	StreamRcptCheckAndSet(SRMP_ST_STREAMRCPT_ID);
	if (IsEmpty())
		return E_FAIL;

	if (NULL == (cMsgProps.EodAckStreamId = AllocCharData()))
		return E_FAIL;
	
	if (! BreakMsmqStreamId(cMsgProps.EodAckStreamId,&cMsgProps.EodAckSeqId))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandleStreamRcptLastOrdinal(void) {
	StreamRcptCheckAndSet(SRMP_ST_STREAMRCPT_LASTORDINAL);
	if (IsEmpty())
		return E_FAIL;

	if (0 == (cMsgProps.EodAckSeqNo = _wtoi64(GetBuffer())))
		return E_FAIL;
	
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleStreamRcptId(void) {
	StreamRcptCheckAndSet(SRMP_ST_STREAMRCPT_ID);  // do nothing
	return S_OK;
}

//
// <deliveryReceipt>
//
// Delivery receipt information is ignored.
HRESULT CSrmpToMsmq::HandleDeliveryRcpt(void) {
	if (IsHeaderSet(SRMP_ST_DELIVERYRCPT))
		return E_FAIL;
	SetHeader(SRMP_ST_DELIVERYRCPT);

	if (! IsSetSrmp(cMsgProps.dwDeliveryRcptSet,SRMP_ST_DELIVERYRCPT_RECEIVEDAT) ||
	    ! IsSetSrmp(cMsgProps.dwDeliveryRcptSet,SRMP_ST_DELIVERYRCPT_ID))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandleDeliveryRcptReceivedAt(void) {
	DeliveryRcptCheckAndSet(SRMP_ST_DELIVERYRCPT_RECEIVEDAT);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleDeliveryRcptId(void) {
	DeliveryRcptCheckAndSet(SRMP_ST_DELIVERYRCPT_ID);
	return S_OK;
}

//
// <commiteReceipt>
//
// Commit Receipt information is ignored
HRESULT CSrmpToMsmq::HandleCommitRcpt(void) {
	if (IsHeaderSet(SRMP_ST_COMMITMENTRCPT))
		return E_FAIL;
	SetHeader(SRMP_ST_COMMITMENTRCPT);

	if (! IsSetSrmp(cMsgProps.dwCommitRcptSet,SRMP_ST_COMMITMENTRCPT_DECIDEDAT) ||
	    ! IsSetSrmp(cMsgProps.dwCommitRcptSet,SRMP_ST_COMMITMENTRCPT_DECISION) ||
	    ! IsSetSrmp(cMsgProps.dwCommitRcptSet,SRMP_ST_COMMITMENTRCPT_ID))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandleCommitRcptDecidedAt(void) {
	CommitRcptCheckAndSet(SRMP_ST_COMMITMENTRCPT_DECIDEDAT);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleCommitRcptDecision(void) {
	CommitRcptCheckAndSet(SRMP_ST_COMMITMENTRCPT_DECISION);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleCommitRcptId(void) {
	CommitRcptCheckAndSet(SRMP_ST_COMMITMENTRCPT_ID);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleCommitRcptCommiteCode(void) {
	CommitRcptCheckAndSet(SRMP_ST_COMMITMENTRCPT_COMMITMENTCODE);	
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleCommitRcptXCommitDetail(void) {
	CommitRcptCheckAndSet(SRMP_ST_COMMITMENTRCPT_XCOMMITMENTDETAIL);
	return S_OK;
}

//
// <msmq>
//
HRESULT CSrmpToMsmq::HandleMsmq(void) {
	if (IsHeaderSet(SRMP_ST_MSMQ))
		return E_FAIL;
	SetHeader(SRMP_ST_MSMQ);

	if (! IsSetSrmp(cMsgProps.dwMsmqSet,SRMP_ST_MSMQ_CLASS) ||
	    ! IsSetSrmp(cMsgProps.dwMsmqSet,SRMP_ST_MSMQ_PRIORITY) ||
	    ! IsSetSrmp(cMsgProps.dwMsmqSet,SRMP_ST_MSMQ_BODYTYPE) ||
	    ! IsSetSrmp(cMsgProps.dwMsmqSet,SRMP_ST_MSMQ_SOURCEQMGUID) ||
	    ! IsSetSrmp(cMsgProps.dwMsmqSet,SRMP_ST_MSMQ_TTRQ))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqClass(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_CLASS);
	if (IsEmpty())
		return E_FAIL;

	cMsgProps.classValue = (USHORT) _wtoi(GetBuffer());
	if (! MQCLASS_IS_VALID(cMsgProps.classValue))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqPrio(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_PRIORITY);
	if (IsEmpty())
		return E_FAIL; 

	cMsgProps.priority = (UCHAR)(_wtoi((WCHAR*)m_Chars.pBuffer));

	if (cMsgProps.priority > MQ_MAX_PRIORITY)
		return E_FAIL;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqJournal(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_JOURNAL);
	cMsgProps.auditing |= MQMSG_JOURNAL;
	return S_OK;
}


HRESULT CSrmpToMsmq::HandleMsmqDeadLetter(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_DEADLETTER);
	cMsgProps.auditing  |= MQMSG_DEADLETTER;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqCorrelation(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_CORRELATION);
	if (IsEmpty())
		return E_FAIL;

	DWORD dwConverted;
	if (NULL == (cMsgProps.pCorrelationID = Base642OctetW(GetBuffer(),GetNumChars(),&dwConverted)) ||
	   (dwConverted != PROPID_M_CORRELATIONID_SIZE)) {
		return E_FAIL;
	}
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqTrace(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_TRACE);
	cMsgProps.fTrace = TRUE;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqConnectorType(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_CONNECTORTYPE);
	if (IsEmpty())
		return E_FAIL;

	return StringToGuid(GetBuffer(),&cMsgProps.connectorType) ? S_OK : E_FAIL;
}

HRESULT CSrmpToMsmq::HandleMsmqApp(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_APP);
	if (IsEmpty())
		return E_FAIL;

	cMsgProps.applicationTag = _wtoi(GetBuffer());
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqBodyType(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_BODYTYPE);
	if (IsEmpty())
		return E_FAIL;

	cMsgProps.bodyType = _wtoi(GetBuffer());
	return S_OK;
}

// WinCE doesn't support security.
HRESULT CSrmpToMsmq::HandleMsmqHashAlg(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_HASHALGORITHM);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqSourceQMGuid(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_SOURCEQMGUID);
	if (IsEmpty())
		return E_FAIL;

	return StringToGuid(GetBuffer(),&cMsgProps.SourceQmGuid) ? S_OK: E_FAIL;
}

// DestinationMQF, AdminMQF, and ResponseMQF all relate to Multi-queue format
// that WinCE doesn't support.
HRESULT CSrmpToMsmq::HandleMsmqDestinationMQF(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_DESTINATIONMQF);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqAdminMQF(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_ADMINMQF);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqResponseMQF(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_RESPONSEMQF);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqTTRQ(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_TTRQ);
	if (IsEmpty())
		return E_FAIL;

	SYSTEMTIME SysTime;
	if (! UtlIso8601TimeToSystemTime(GetBuffer(),GetNumChars(),&SysTime))
		return E_FAIL;

	if (! UtlSystemTimeToCrtTime(&SysTime,&cMsgProps.absoluteTimeToQueue))
		return E_FAIL;

	return S_OK;
}


// <msmq><eod>
HRESULT CSrmpToMsmq::HandleMsmqEod(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_EOD);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqEodFirst(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_EOD_FIRST);
	cMsgProps.fFirst = TRUE;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqEodLast(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_EOD_LAST);
	cMsgProps.fLast = TRUE;
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqEodXConnectorId(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_EOD_XCONNECTORID);
	if (IsEmpty())
		return E_FAIL;

	return StringToGuid(GetBuffer(),&cMsgProps.connectorId) ? S_OK : E_FAIL;
}

// <msmq><provider>.  Security related, not supported on WinCE.
HRESULT CSrmpToMsmq::HandleMsmqProvider(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_PROVIDER);

	if (! IsSetSrmp(cMsgProps.dwMsmqSet,SRMP_ST_MSMQ_PROVIDER_TYPE) ||
	    ! IsSetSrmp(cMsgProps.dwMsmqSet,SRMP_ST_MSMQ_PROVIDER_NAME))
		return E_FAIL;

	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqProviderType(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_PROVIDER_TYPE);
	return S_OK;
}

HRESULT CSrmpToMsmq::HandleMsmqProviderName(void) {
	MsmqRcptCheckAndSet(SRMP_ST_MSMQ_PROVIDER_NAME);
	return S_OK;
}

DWORD CSrmpMessageProperties::CalculatePacketSize(DWORD *pdwBaseSize) {
	const QUEUE_FORMAT  *pAdminQueue     = (adminQueue.GetType() != QUEUE_FORMAT_TYPE_UNKNOWN) ? &adminQueue : NULL;
	const QUEUE_FORMAT  *pResponseQueue  = (responseQueue.GetType() != QUEUE_FORMAT_TYPE_UNKNOWN) ? &responseQueue : NULL;
	const GUID          *pGuidConnType   = (connectorType != GUID_NULL) ? &connectorType : NULL;
	const GUID          *pGuidConnId     = (connectorId   != GUID_NULL) ? &connectorId   : NULL;
	DWORD cbData = 0;
	PSTR  szData = 0; 

	DWORD cbPacket = 0;

	cbPacket  = CBaseHeader::CalcSectionSize ();
	cbPacket += CUserHeader::CalcSectionSize (&SourceQmGuid,&GUID_NULL,pGuidConnType,
	                                          &destQueue,pAdminQueue, pResponseQueue);

	if (IsXActIncluded())
		cbPacket += CXactHeader::CalcSectionSize(&EodSeqId,pGuidConnId);

	cMime.GetMimeAttachment(cszEnvelopeId,&szData,&cbData);
	cbPacket += CPropertyHeader::CalcSectionSize(szTitle ? wcslen(szTitle)+1 : 0,cbData,0);

	cbPacket += CSrmpEnvelopeHeader::CalcSectionSize(ccEnvelope);

	cbPacket += CCompoundMessageHeader::CalcSectionSize(pHttpParams->cbHeaders,pHttpParams->cbPost);

	if (EodStreamId) {
		DWORD ccEodStreamId = (wcslen(EodStreamId)+1)*sizeof(WCHAR);
		DWORD ccOrderQueue  = szOrderQueue ? (wcslen(szOrderQueue)+1)*sizeof(WCHAR) : 0;
		cbPacket += CEodHeader::CalcSectionSize(ccEodStreamId,ccOrderQueue);
	}

	if (EodAckStreamId)
		cbPacket += CEodAckHeader::CalcSectionSize((wcslen(EodAckStreamId)+1)*sizeof(WCHAR));

	*pdwBaseSize = cbPacket;

	if (revViaBuf.uiNextIn)
		cbPacket += CDataHeader::CalcSectionSize(revViaBuf.uiNextIn / sizeof(WCHAR),TRUE);

	if (fwdViaBuf.uiNextIn)
		cbPacket += CDataHeader::CalcSectionSize(fwdViaBuf.uiNextIn / sizeof(WCHAR),TRUE);

	if (szFrom || szRelatesTo)
		cbPacket += CSoapExtSection::CalcSectionSize(ccFrom,ccRelatesTo);

	return cbPacket;
}

BOOL CSrmpToMsmq::PutMessageInQueue(ScPacketImage *pImage) {
	ScQueue             *pQueue          = NULL;

	if (! (pQueue = gQueueMan->FindQueueByPacketImage (pImage, FALSE))) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"User packet arrived to non-existent queue...\n");
#endif
		gQueueMan->ForwardTransactionalResponse (pImage, MQMSG_CLASS_NACK_BAD_DST_Q, NULL, NULL);
		gQueueMan->RejectPacket (pImage, MQMSG_CLASS_NACK_BAD_DST_Q);
	} else {
		//
		//	Put this thing in a queue...
		//
		//
		//	If we don't have resources, this will fail. In this case we'd
		//	better close the connection.
		//
		if (pQueue->qp.bTransactional) {
			if (! gQueueMan->StoreTransactionalPacket (pQueue, pImage, NULL, NULL))
				return FALSE;
		} else {
			pImage->hkOrderKey = ++pQueue->hkReceived;
			pImage->hkOrderKey |= pImage->sect.pBaseHeader->GetPriority() << SCPACKET_ORD_TIMEBITS;
			ScPacket *pPacket = pQueue->MakePacket (pImage, -1, TRUE);

			if (! pPacket) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_SESSION, L"User packet from cannot be put into %s...\n", pQueue->lpszFormatName);
#endif
				gQueueMan->RejectPacket (pImage, MQMSG_CLASS_NACK_Q_EXCEED_QUOTA);
				return FALSE;
			} else
				gQueueMan->AcceptPacket (pPacket, MQMSG_CLASS_ACK_REACH_QUEUE, pQueue);
		}
	}
	return pQueue ? TRUE : FALSE;
}

BOOL CSrmpToMsmq::CreateAndInsertPacket(void) {
	ScPacket       *pPacket      = NULL;
	ScPacketImage  *pPacketImage = NULL;
	BOOL           fRet          = FALSE;

	if (! cMsgProps.VerifyProperties()) {
		SetBadRequest();
		return FALSE;
	}

	if (!fApiInitialized) {
		SetUnavailable();
		return FALSE;
	}

	gMem->Lock();
	if (glServiceState != SERVICE_STATE_ON) {
		SetUnavailable();
		goto done;
	}

	if (NULL == (pPacketImage = cMsgProps.BuildPacketImage()))
		goto done;

	SetSuccess(); // once we process message (regardless of other possible errors) we're done.
	if (! PutMessageInQueue(pPacketImage))
		goto done;

	fRet = TRUE;
done:
	if (!fRet && pPacketImage)
		g_funcFree (pPacketImage, g_pvFreeData);

	gMem->Unlock();
	return fRet;
}

// Convert cMsgProps to an MSMQ packet (including MIME info)
ScPacketImage * CSrmpMessageProperties::BuildPacketImage(void) {
	BOOL                fRet                 = FALSE;
	ScPacketImage       *pPacketImage        = NULL;
	CBaseHeader         *pBaseHeader         = NULL;
	CUserHeader         *pUserHeader         = NULL;
	CPropertyHeader     *pPropHeader         = NULL;
	CEodHeader          *pEodHeader          = NULL;
	CEodAckHeader       *pEodAckHeader       = NULL;
	CSrmpEnvelopeHeader* pSrmpEnvelopeHeader = NULL;
	CCompoundMessageHeader* pCmpndMsgHeader  = NULL;
	CDataHeader         *pFwdHeader          = NULL; 
	CDataHeader         *pRevHeader          = NULL; 
	CDataHeader         *pOrigURL            = NULL; 
	CSoapExtSection     *pSoapExtSection     = NULL;
	const QUEUE_FORMAT  *pAdminQueue     = (adminQueue.GetType() != QUEUE_FORMAT_TYPE_UNKNOWN) ? &adminQueue : NULL;
    const QUEUE_FORMAT  *pResponseQueue  = (responseQueue.GetType() != QUEUE_FORMAT_TYPE_UNKNOWN) ? &responseQueue : NULL;
	const GUID          *pGuidConnType   = (connectorType != GUID_NULL) ? &connectorType : NULL;
	const GUID          *pGuidConnId     = (connectorId   != GUID_NULL) ? &connectorId   : NULL;


	PSTR  szMimeBody    = NULL;
	DWORD cbMimeBody    = 0;
	DWORD cbMimeOffset;
	DWORD cbBaseSize = 0;

	DWORD               cbPacket;
	CHAR                *szData = NULL;
	DWORD               cbData  = 0;
	void                *pvPacketBuffer;

	const USHORT x_SRMP_ENVELOPE_ID = 400;
	const USHORT x_COMPOUND_MESSAGE_ID = 500;

	if (0 == (cbPacket = CalculatePacketSize(&cbBaseSize)))
		goto done;

	if (NULL == (pPacketImage = (ScPacketImage *)g_funcAlloc (sizeof(ScPacketImage) + cbPacket, g_pvAllocData))) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"SRMP: unable to allocate memory!\r\n"));
		goto done;
	}

	pvPacketBuffer = (void *)((unsigned char *)pPacketImage + sizeof(ScPacketImage));
	memset (&pPacketImage->sect, 0, sizeof (pPacketImage->sect));
	memset (&pPacketImage->ucSourceAddr, 0, sizeof(pPacketImage->ucSourceAddr));

	pPacketImage->pvBinary = NULL;
	pPacketImage->pvExt    = NULL;
	pPacketImage->allflags = 0;
	// fSecureSession means not only over SSL but that client is using 
	// an authenticated client certificate, which currently CE web server doesn't support.
	// Note: . If this is set to FALSE, then no nacks can be generated
	// (because it is possible to cause DOS attack with acks).
	pPacketImage->flags.fSecureSession=!gMachine->fUntrustedNetwork;
	pPacketImage->flags.fSRMPGenerated     = TRUE;  // determines rules for handeling fwds
	pPacketImage->flags.fHaveIpv4Addr      = (pHttpParams->dwIP4Addr != INADDR_NONE);

	if (pPacketImage->flags.fHaveIpv4Addr)
		pPacketImage->ipSourceAddr.s_addr = pHttpParams->dwIP4Addr;

	// Base
	pBaseHeader = (CBaseHeader *)pvPacketBuffer;
	pBaseHeader->CBaseHeader::CBaseHeader (cbBaseSize);
	pBaseHeader->SetPriority (priority);
	pBaseHeader->SetTrace (fTrace);
	pBaseHeader->SetAbsoluteTimeToQueue (IsMSMQIncluded() ? absoluteTimeToQueue : absoluteTimeToLive);

	// UserHeader
	pUserHeader = (CUserHeader *)pBaseHeader->GetNextSection ();

	pUserHeader->CUserHeader::CUserHeader (&SourceQmGuid,
	                                       &GUID_NULL, 
	                                       &destQueue,
	                                       pAdminQueue,
	                                       pResponseQueue,
	                                       messageId.Uniquifier);

    if (pGuidConnType)
	    pUserHeader->SetConnectorType(pGuidConnType);

	pUserHeader->SetTimeToLiveDelta (IsMSMQIncluded() ? absoluteTimeToLive - absoluteTimeToQueue : 0);
	pUserHeader->SetSentTime (sentTime);
	pUserHeader->SetDelivery (delivery);
	pUserHeader->IncludeProperty (TRUE);
	pUserHeader->SetAuditing (auditing);

	// XAct
	if (IsXActIncluded()) {
		pUserHeader->IncludeXact(TRUE);
		CXactHeader *pXactHeader = (CXactHeader *)pUserHeader->GetNextSection ();

		pXactHeader->CXactHeader::CXactHeader (NULL);
		pXactHeader->SetCancelFollowUp(TRUE);
		pXactHeader->SetSeqID(EodSeqId);
		pXactHeader->SetSeqN(EodSeqNo);
		pXactHeader->SetPrevSeqN(EodPrevSeqNo);
		pXactHeader->SetFirstInXact(fFirst);
		pXactHeader->SetLastInXact(fLast);

		if (pGuidConnId)
			pXactHeader->SetConnectorQM(pGuidConnId);

		pPropHeader = (CPropertyHeader *)pXactHeader->GetNextSection ();
	} else
		pPropHeader = (CPropertyHeader *)pUserHeader->GetNextSection ();

	// Properties
	pPropHeader->CPropertyHeader::CPropertyHeader ();
	pPropHeader->SetClass (classValue);
	if (pCorrelationID)
		pPropHeader->SetCorrelationID (pCorrelationID);

	pPropHeader->SetAckType (acknowledgeType);
	pPropHeader->SetApplicationTag (applicationTag);
	pPropHeader->SetBodyType (bodyType);

	if (szTitle)
		pPropHeader->SetTitle (szTitle, wcslen (szTitle) + 1);

	if (cMime.GetMimeAttachment(cszEnvelopeId,&szData,&cbData))
		pPropHeader->SetMsgExtension ((const UCHAR*)szData, cbData);
	
	pPropHeader->SetHashAlg (0);
	pPropHeader->SetPrivLevel (MQMSG_PRIV_LEVEL_NONE);
	pPropHeader->SetEncryptAlg (0);


	// SrmpEnvelopeHeader section.
	pUserHeader->IncludeSrmp(TRUE);
	pSrmpEnvelopeHeader = (CSrmpEnvelopeHeader*) pPropHeader->GetNextSection();
	pSrmpEnvelopeHeader->CSrmpEnvelopeHeader::CSrmpEnvelopeHeader(bstrEnvelope,ccEnvelope,x_SRMP_ENVELOPE_ID);
	pCmpndMsgHeader = (CCompoundMessageHeader*) pSrmpEnvelopeHeader->GetNextSection();


	// CCompoundMessageHeader section.
	cMime.GetMimeAttachment(cszMimeBodyId,&szMimeBody,&cbMimeBody);
	cbMimeOffset = szMimeBody ? szMimeBody-pHttpParams->pszPost : 0;

	pCmpndMsgHeader->CCompoundMessageHeader::CCompoundMessageHeader((UCHAR*)pHttpParams->pszHeaders,pHttpParams->cbHeaders,(UCHAR*)pHttpParams->pszPost,
	                                         pHttpParams->cbPost,cbMimeBody,cbMimeOffset,x_COMPOUND_MESSAGE_ID);

	// EODHeader Section, <stream> information.
	if (EodStreamId) {
		pUserHeader->IncludeEod(TRUE);
		pEodHeader = (CEodHeader *) pCmpndMsgHeader->GetNextSection();

		const USHORT x_EOD_ID = 600;
		DWORD ccEodStreamId = (wcslen(EodStreamId)+1)*sizeof(WCHAR);
		DWORD ccOrderQueue  = szOrderQueue ? (wcslen(szOrderQueue)+1)*sizeof(WCHAR) : 0;

		pEodHeader->CEodHeader::CEodHeader(x_EOD_ID,ccEodStreamId,(UCHAR*)EodStreamId,
		                                   ccOrderQueue,(UCHAR*)szOrderQueue);

		pEodAckHeader = (CEodAckHeader *) pEodHeader->GetNextSection();
	}
	else
		pEodAckHeader = (CEodAckHeader *) pCmpndMsgHeader->GetNextSection();

	// <streamReceipt> information.
	if (EodAckStreamId) {
		pUserHeader->IncludeEodAck(TRUE);
		const USHORT x_EOD_ACK_ID = 700;
		pEodAckHeader->CEodAckHeader::CEodAckHeader(x_EOD_ACK_ID,&EodAckSeqId,&EodAckSeqNo,
		                                            (wcslen(EodAckStreamId)+1)*sizeof(WCHAR),(UCHAR*)EodAckStreamId);

		pFwdHeader = (CDataHeader *) pEodAckHeader->GetNextSection();
	}
	else
		pFwdHeader = (CDataHeader *) pEodAckHeader;

	// <fwd><via> elements
	if (fwdViaBuf.uiNextIn) {
		pPacketImage->flags.fFwdIncluded = TRUE;
		pFwdHeader->CDataHeader::CDataHeader((WCHAR*)fwdViaBuf.pBuffer,fwdViaBuf.uiNextIn / sizeof(WCHAR));
		pRevHeader = (CDataHeader*) pFwdHeader->GetNextSection();
	}
	else
		pRevHeader = (CDataHeader*) pFwdHeader;

	// <rev><via> 
	if (revViaBuf.uiNextIn) {
		pPacketImage->flags.fRevIncluded = TRUE;
		pRevHeader->CDataHeader::CDataHeader((WCHAR*)revViaBuf.pBuffer,revViaBuf.uiNextIn / sizeof(WCHAR));
		pSoapExtSection = (CSoapExtSection*) pRevHeader->GetNextSection();
	}
	else
		pSoapExtSection = (CSoapExtSection*) pRevHeader;

	if (szFrom || szRelatesTo) {
		pPacketImage->flags.fSoapExtIncluded = TRUE;
		pSoapExtSection->CSoapExtSection::CSoapExtSection(szFrom,ccFrom,szRelatesTo,ccRelatesTo);
		pOrigURL = (CDataHeader*) pSoapExtSection->GetNextSection();
	}
	else
		pOrigURL = (CDataHeader*) pSoapExtSection;

//	pPacketImage->flags.fOriginalURLIncluded = TRUE;
	if (! pPacketImage->PopulateSections ())
		goto done;

	fRet = TRUE;
done:
	if (!fRet && pPacketImage) {
		g_funcFree(pPacketImage, g_pvFreeData);
		return NULL;
	}
	return pPacketImage;
}

BOOL BreakMsmqStreamId(const WCHAR *szBuffer, LONGLONG *pSeqId) {
	static const WCHAR cDivider = L'\\';
	WCHAR *szTrav = wcschr(szBuffer,cDivider);
	if (! szTrav)
		return FALSE;

	*szTrav       = 0;
	*pSeqId       = _wtoi64(szTrav+1);
	return TRUE;
}

// Now that all data has been read, there's some extra fixups we need to perform.
BOOL CSrmpMessageProperties::VerifyProperties(void) {
	if (IsMSMQIncluded() && absoluteTimeToQueue > absoluteTimeToLive)
		return FALSE;

	if (!(dwHeaderSet & SRMP_ST_PATH) || !( dwHeaderSet & SRMP_ST_PROPERTIES))
		return FALSE;

	// Handle <stream>.  We can only properly process it at this stage since
	// handeling differs depending on whether <MSMQ>...</MSMQ> was included.
	if (szStreamIdBuf && IsMSMQIncluded()) {
		if (!BreakMsmqStreamId(szStreamIdBuf,&EodSeqId))
			return FALSE;
	}
	if (szStreamIdBuf)
		EodStreamId = szStreamIdBuf;

	return TRUE;
}

BOOL CSrmpMime::GetMimeAttachment(const CHAR *szContentId, CHAR **ppszData, DWORD *pcbData) {
	PMIME_ATTACHMENT pTrav = m_pMime;
	DWORD cbContentId      = strlen(szContentId);
	SVSUTIL_ASSERT(*ppszData == NULL && *pcbData == 0);

	while (pTrav) {
		if (0 == _strnicmp(szContentId,pTrav->szContentId,cbContentId)) {
			*ppszData = pTrav->szBody;
			*pcbData  = pTrav->cbBody;
			return TRUE;
		}
		pTrav = pTrav->pNext;
	}
	return FALSE;
}
