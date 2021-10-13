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

    SrmpDefs.cxx

Abstract:

    Contains strings and definitions related to SRMP messages.
 
--*/

#include <windows.h>
#include <msxml2.h>
#include "SrmpDefs.hxx"
#include "SrmpAccept.hxx"

//
// SRMP Tokens
//
// Envelope and top level
const WCHAR cszEnvelope[] = L"Envelope"; // SOAP_NAMESPACE
const WCHAR cszSoapEnv[]  = L"se";       //
const WCHAR cszBody[]     = L"Body";     // SOAP_NAMESPACE

// Begin Header List
const WCHAR cszHeader[]   = L"Header";   // SOAP_NAMESPACE


// Begin Path List.  All under SOAP_RP_NAMESPACE
const WCHAR cszPath[] =  L"path";
const WCHAR cszId[] =  L"id";
const WCHAR cszTo[] =  L"to";
const WCHAR cszRev[] = L"rev";
  const WCHAR cszVia[] = L"via";
const WCHAR cszFrom[] = L"from";
const WCHAR cszAction[] =  L"action";
const WCHAR cszRelatesTo[] =  L"relatesTo";
const WCHAR cszFixed[] = L"fixed";
const WCHAR cszFwd[] = L"fwd";
const WCHAR cszFault[] = L"fault";
// End Path List

// Begin Property list.  All SRMP_NAMESPACE, all 0,1.
const WCHAR cszProperties[] = L"properties";
const WCHAR cszExpiresAt[] =  L"expiresAt";
const WCHAR cszDuration[] =  L"duration";
const WCHAR cszSentAt[] =     L"sentAt";
const WCHAR cszInReplyTo[] = L"inReplyTo";
// End Property list.

// Begin Service list.  All SRMP_NAMESPACE
const WCHAR cszServices[] =  L"services";
const WCHAR cszDurable[] = L"durable" ;
const WCHAR cszDeliveryReceiptRequest[] = L"deliveryReceiptRequest";
  const WCHAR cszSendTo[] = L"sendTo";
  const WCHAR cszSendBy[] = L"SendBy";
const WCHAR cszFilterDuplicates[] = L"filterDuplicates";
const WCHAR cszCommitmentReceiptRequest[] = L"commitmentReceiptRequest";
  const WCHAR cszPositiveOnly[] = L"positiveOnly";
  const WCHAR cszNegativeOnly[] = L"negativeOnly";
//  const WCHAR cszSendBy[] =  L"SendBy";
//  const WCHAR cszSendTo[] = L"sendTo";
// End Service list.

// Begin Stream.  SRMP_NAMESPACE.
const WCHAR cszStream[] =  L"Stream";
const WCHAR cszStreamId[] =  L"streamId";
const WCHAR cszCurrent[] =  L"current";
const WCHAR cszPrevious[] = L"previous";
const WCHAR cszEnd[] = L"end";
const WCHAR cszStart[] = L"start";
  const WCHAR cszSendReceiptsTo[] = L"sendReceiptsTo";
//  const WCHAR cszExpiresAt[] =  L"expiresAt";
const WCHAR cszStreamReceiptRequest[] = L"streamReceiptRequest";
// End Stream

// Begin stream receipt.  SRMP_NAMESPACE
const WCHAR cszStreamReceipt[] = L"streamReceipt";
const WCHAR cszreceivedAt[] = L"receivedAt";
//const WCHAR cszStreamId[] =  L"streamId";
const WCHAR cszlastOrdinal[] = L"lastOrdinal" ;
//const WCHAR cszId[] =  L"id";
// End stream receipt.

// Begin deliveryReceipt.  SRMP_NAMESPACE
const WCHAR cszDeliveryReceipt[] = L"deliveryReceipt";
//const WCHAR cszreceivedAt[] = L"receivedAt";
//const WCHAR cszId[] =  L"id";
// End deliveryReceipt.

// Begin commitmentReceipt.  SRMP_NAMESPACE
const WCHAR cszCommitmentReceipt[] = L"commitmentReceipt";
const WCHAR cszDecidedAt[] = L"decidedAt";
const WCHAR cszDecision[] =   L"decision";
//const WCHAR cszId[] =  L"id";
const WCHAR cszCommitmentCode[] = L"commitmentCode";
const WCHAR cszXCommitmentDetail[] = L"xCommitmentDetail";
// End commitmentReceipt

// Begin MSMQ.  MSMQ_NAMESPACE
const WCHAR cszMsmq[]  = L"Msmq";
const WCHAR cszClass[] = L"Class";
const WCHAR cszPriority[] = L"Priority";
const WCHAR cszJournal[] =  L"Journal";
const WCHAR cszDeadLetter[] =  L"DeadLetter";
const WCHAR cszCorrelation[] =  L"Correlation";
const WCHAR cszTrace[] =  L"Trace";
const WCHAR cszConnectorType[] =  L"ConnectorType";
const WCHAR cszApp[] =  L"App";
const WCHAR cszBodyType[] = L"BodyType";
const WCHAR cszHashAlgorithm[] = L"HashAlgorithm";
const WCHAR cszEod[] =  L"Eod";
  const WCHAR cszFirst[] =  L"First";
  const WCHAR cszLast[] = L"Last";
  const WCHAR cszXConnectorId[] =  L"xConnectorId";
const WCHAR cszProvider[] =  L"Provider";
  const WCHAR cszType[] =  L"Type";
  const WCHAR cszName[] =  L"Name";
const WCHAR cszSourceQmGuid[] = L"SourceQmGuid";
const WCHAR cszDestinationMqf[] =  L"DestinationMqf";
const WCHAR cszAdminMqf[] =  L"AdminMqf";
const WCHAR cszResponseMqf[]= L"ResponseMqf";
const WCHAR cszTTrq[] = L"TTrq";
// End MSMQ

// One entry for Signature.  UNKNOWN_NAMESPACE
const WCHAR cszSignature[] = L"Signature";
// End Header list

SRMP_TOKEN GetToken(const WCHAR *szElement, const DWORD cbElement) {
	switch (towupper(szElement[0])) {
		case L'A':
			if (TokenEqual(szElement,cszAction,cbElement)) return SRMP_TOK_ACTION;
			if (TokenEqual(szElement,cszApp,cbElement)) return  SRMP_TOK_APP;
			if (TokenEqual(szElement,cszAdminMqf,cbElement)) return  SRMP_TOK_ADMINMQF;
		break;

		case L'B':
			if (TokenEqual(szElement,cszBody,cbElement)) return  SRMP_TOK_BODY;
			if (TokenEqual(szElement,cszBodyType,cbElement)) return  SRMP_TOK_BODYTYPE;
		break;
		
		case L'C':
			if (TokenEqual(szElement,cszCommitmentReceipt,cbElement)) return  SRMP_TOK_COMMITMENTRECEIPT;
			if (TokenEqual(szElement,cszCommitmentCode,cbElement)) return  SRMP_TOK_COMMITMENTCODE;
			if (TokenEqual(szElement,cszCurrent,cbElement)) return  SRMP_TOK_CURRENT;
			if (TokenEqual(szElement,cszClass,cbElement)) return  SRMP_TOK_CLASS;
			if (TokenEqual(szElement,cszConnectorType,cbElement)) return  SRMP_TOK_CONNECTORTYPE;
			if (TokenEqual(szElement,cszCommitmentReceiptRequest,cbElement)) return  SRMP_TOK_COMMITMENTRECEIPTREQUEST;
			if (TokenEqual(szElement,cszCorrelation,cbElement)) return  SRMP_TOK_CORRELATION;
		break;
		
		case L'D':
			if (TokenEqual(szElement,cszDeadLetter,cbElement)) return SRMP_TOK_DEADLETTER;
			if (TokenEqual(szElement,cszDeliveryReceipt,cbElement)) return  SRMP_TOK_DELIVERYRECEIPT;
			if (TokenEqual(szElement,cszDecidedAt,cbElement)) return  SRMP_TOK_DECIDEDAT;
			if (TokenEqual(szElement,cszDecision,cbElement)) return  SRMP_TOK_DECISION;
			if (TokenEqual(szElement,cszDuration,cbElement)) return  SRMP_TOK_DURATION;
			if (TokenEqual(szElement,cszDurable,cbElement)) return  SRMP_TOK_DURABLE;
			if (TokenEqual(szElement,cszDeliveryReceiptRequest,cbElement)) return  SRMP_TOK_DELIVERYRECEIPTREQUEST;
			if (TokenEqual(szElement,cszDestinationMqf,cbElement)) return  SRMP_TOK_DESTINATIONMQF;
		break;

		case L'E':
			if (TokenEqual(szElement,cszEod,cbElement)) return  SRMP_TOK_EOD;
			if (TokenEqual(szElement,cszExpiresAt,cbElement)) return  SRMP_TOK_EXPIRESAT;
			if (TokenEqual(szElement,cszEnvelope,cbElement)) return  SRMP_TOK_ENVELOPE;
			if (TokenEqual(szElement,cszEnd,cbElement)) return  SRMP_TOK_END;
			if (TokenEqual(szElement,cszExpiresAt,cbElement)) return  SRMP_TOK_EXPIRESAT;
		break;

		case L'F':
			if (TokenEqual(szElement,cszFilterDuplicates,cbElement)) return  SRMP_TOK_FILTERDUPLICATES;
			if (TokenEqual(szElement,cszFirst,cbElement)) return  SRMP_TOK_FIRST;
			if (TokenEqual(szElement,cszFrom,cbElement)) return  SRMP_TOK_FROM;
			if (TokenEqual(szElement,cszFixed,cbElement)) return  SRMP_TOK_FIXED;
			if (TokenEqual(szElement,cszFwd,cbElement)) return  SRMP_TOK_FWD;
			if (TokenEqual(szElement,cszFault,cbElement)) return  SRMP_TOK_FAULT;
		break;

		case L'H':
			if (TokenEqual(szElement,cszHeader,cbElement)) return  SRMP_TOK_HEADER;
			if (TokenEqual(szElement,cszHashAlgorithm,cbElement)) return  SRMP_TOK_HASHALGORITHM;
		break;

		case L'I':
			if (TokenEqual(szElement,cszId,cbElement)) return  SRMP_TOK_ID;
			if (TokenEqual(szElement,cszInReplyTo,cbElement)) return  SRMP_TOK_INREPLYTO;
		break;

		case L'J':
			if (TokenEqual(szElement,cszJournal,cbElement)) return  SRMP_TOK_JOURNAL;
		break;

		case L'L':
			if (TokenEqual(szElement,cszlastOrdinal,cbElement)) return  SRMP_TOK_LASTORDINAL;
			if (TokenEqual(szElement,cszLast,cbElement)) return  SRMP_TOK_LAST;
		break;

		case L'M':
			if (TokenEqual(szElement,cszMsmq,cbElement)) return  SRMP_TOK_MSMQ;
		break;

		case L'N':
			if (TokenEqual(szElement,cszName,cbElement)) return  SRMP_TOK_NAME;
			if (TokenEqual(szElement,cszNegativeOnly,cbElement)) return SRMP_TOK_NEGATIVEONLY;
		break;

		case L'P':
			if (TokenEqual(szElement,cszPriority,cbElement)) return  SRMP_TOK_PRIORITY;
			if (TokenEqual(szElement,cszPrevious,cbElement)) return  SRMP_TOK_PREVIOUS;
			if (TokenEqual(szElement,cszPath,cbElement)) return  SRMP_TOK_PATH;
			if (TokenEqual(szElement,cszProperties,cbElement)) return  SRMP_TOK_PROPERTIES;
			if (TokenEqual(szElement,cszPositiveOnly,cbElement)) return  SRMP_TOK_POSITIVEONLY;
			if (TokenEqual(szElement,cszProvider,cbElement)) return  SRMP_TOK_PROVIDER;
		break;

		case L'R':
			if (TokenEqual(szElement,cszRev,cbElement)) return  SRMP_TOK_REV;
			if (TokenEqual(szElement,cszRelatesTo,cbElement)) return  SRMP_TOK_RELATESTO;
			if (TokenEqual(szElement,cszreceivedAt,cbElement)) return  SRMP_TOK_RECEIVEDAT;
			if (TokenEqual(szElement,cszResponseMqf,cbElement)) return  SRMP_TOK_RESPONSEMQF;
		break;

		case L'S':
			if (TokenEqual(szElement,cszSentAt,cbElement)) return  SRMP_TOK_SENTAT;
			if (TokenEqual(szElement,cszServices,cbElement)) return  SRMP_TOK_SERVICES;
			if (TokenEqual(szElement,cszSendTo,cbElement)) return  SRMP_TOK_SENDTO;
			if (TokenEqual(szElement,cszSendBy,cbElement)) return  SRMP_TOK_SENDBY;
			if (TokenEqual(szElement,cszSendTo,cbElement)) return  SRMP_TOK_SENDTO;
			if (TokenEqual(szElement,cszStream,cbElement)) return  SRMP_TOK_STREAM;
			if (TokenEqual(szElement,cszStreamId,cbElement)) return  SRMP_TOK_STREAMID;
			if (TokenEqual(szElement,cszStart,cbElement)) return  SRMP_TOK_START;
			if (TokenEqual(szElement,cszSendReceiptsTo,cbElement)) return  SRMP_TOK_SENDRECEIPTSTO;
			if (TokenEqual(szElement,cszStreamReceiptRequest,cbElement)) return  SRMP_TOK_STREAMRECEIPTREQUEST;
			if (TokenEqual(szElement,cszStreamReceipt,cbElement)) return  SRMP_TOK_STREAMRECEIPT;
			if (TokenEqual(szElement,cszStreamId,cbElement)) return  SRMP_TOK_STREAMID;
			if (TokenEqual(szElement,cszSignature,cbElement)) return  SRMP_TOK_SIGNATURE;
			if (TokenEqual(szElement,cszSourceQmGuid,cbElement)) return  SRMP_TOK_SOURCEQMGUID;
		break;

		case L'T':
			if (TokenEqual(szElement,cszTo,cbElement)) return  SRMP_TOK_TO;
			if (TokenEqual(szElement,cszTrace,cbElement)) return  SRMP_TOK_TRACE;
			if (TokenEqual(szElement,cszTTrq,cbElement)) return  SRMP_TOK_TTRQ;
			if (TokenEqual(szElement,cszType,cbElement)) return  SRMP_TOK_TYPE;
		break;

		case L'V':
			if (TokenEqual(szElement,cszVia,cbElement)) return  SRMP_TOK_VIA;
		break;

		case L'X':
			if (TokenEqual(szElement,cszXCommitmentDetail,cbElement)) return  SRMP_TOK_XCOMMITMENTDETAIL;
			if (TokenEqual(szElement,cszXConnectorId,cbElement)) return  SRMP_TOK_XCONNECTORID;
		break;

		default:
			return SRMP_TOK_UNKNOWN;
	}
	return SRMP_TOK_UNKNOWN;
}

//
// SOAP namespaces
//
static const WCHAR cszSoapNamespace[]   = L"http://schemas.xmlsoap.org/soap/envelope/";
static const WCHAR cszSrmpNamespace[]   = L"http://schemas.xmlsoap.org/srmp/";
static const WCHAR cszSoapRpNamespace[] = L"http://schemas.xmlsoap.org/rp/";
static const WCHAR cszMSMQNamespace[]   = L"msmq.namespace.xml";

SOAP_NAMESPACE MapNamespace(const WCHAR *szNS, const DWORD cbNS) {
	if (!szNS)
		return NAMESPACE_UNKNOWN;

	if (TokenEqual(szNS,cszSoapNamespace,cbNS))
		return NAMESPACE_SOAP;
	if (TokenEqual(szNS,cszSrmpNamespace,cbNS))
		return NAMESPACE_SRMP;
	if (TokenEqual(szNS,cszSoapRpNamespace,cbNS))
		return NAMESPACE_SOAP_RP;
	if (TokenEqual(szNS,cszMSMQNamespace,cbNS))
		return NAMESPACE_MSMQ;

	return NAMESPACE_UNKNOWN;
}


//
// Misc Constants
//

const IID GUID_NULL = {0};
const LONGLONG i64NoneMSMQSeqId = _I64_MAX;

const char cszPrefixMimeAttachment[] = "cid:";
// const WCHAR xPrefixMimeAttachmentW[] = L"cid:";
const DWORD ccPrefixMimeAttachmentLen = SVSUTIL_CONSTSTRLEN(cszPrefixMimeAttachment);

const char cszEnvelopeId[] = "envelope@";
// const WCHAR xEnvelopeIdW[] = L"envelope@";
const DWORD ccEnvelopeIdLen = SVSUTIL_CONSTSTRLEN(cszEnvelopeId);

const char cszMimeBodyId[] = "body@";
// const WCHAR xMimeBodyIdW[] = L"body@";
const DWORD ccMimeBodyIdLen = SVSUTIL_CONSTSTRLEN(cszMimeBodyId);

const char cszMimeSenderCertificateId[] = "certificate@";
// const WCHAR xMimeSenderCertificateIdW[] = L"certificate@";
const DWORD ccMimeSenderCertificateIdLen = SVSUTIL_CONSTSTRLEN(cszMimeSenderCertificateId);

const char cszMimeExtensionId[] = "extension@";
// const WCHAR xMimeExtensionIdW[] = L"extension@";
const DWORD ccMimeExtensionIdLen = SVSUTIL_CONSTSTRLEN(cszMimeExtensionId);

const WCHAR cszPositive[] = L"positive";
const WCHAR cszNegative[] = L"negative";

const char cszContentType[]     = "Content-Type:";
const char cszContentLength[]   = "Content-Length:";
const char cszContentId[]       = "Content-Id:";
const char cszBoundary[]        = "boundary=";
const char cszBoundaryHyphen[]  = "--";

const char   cszCRLF[]          = "\r\n";
const char   cszCRLF2[]         = "\r\n\r\n";
const DWORD  ccCRLF             = SVSUTIL_CONSTSTRLEN(cszCRLF);
const DWORD  ccCRLF2            = SVSUTIL_CONSTSTRLEN(cszCRLF2);
const DWORD  ccBoundary         = SVSUTIL_CONSTSTRLEN(cszBoundary);
const DWORD  ccBoundaryHyphen   = SVSUTIL_CONSTSTRLEN(cszBoundaryHyphen);
const DWORD  ccContentLength    = SVSUTIL_CONSTSTRLEN(cszContentLength);
const DWORD  ccContentType      = SVSUTIL_CONSTSTRLEN(cszContentType);
const WCHAR  cszMSMQVroot[]     = L"/Msmq/";
