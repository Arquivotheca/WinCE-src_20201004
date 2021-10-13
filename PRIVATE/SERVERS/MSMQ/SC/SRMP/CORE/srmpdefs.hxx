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

    scsrmp.hxx

Abstract:

    Contains strings and definitions related to SRMP messages.

--*/



#if ! defined (__srmpdefs_HXX__)
#define __srmpdefs_HXX__	1

#include <windows.h>

//
// For tokenizing strings and keeping track of state.
// 
inline BOOL TokEqual(const WCHAR* szElement, const WCHAR* szToken, const DWORD cElementChars, const DWORD cTokenChars) {
	return ((cTokenChars == cElementChars) && (0 == wcsncmp(szElement,szToken,cElementChars)));
}
#define TokenEqual(szElement, szToken, cElementChars) TokEqual(szElement,szToken,cElementChars,SVSUTIL_CONSTSTRLEN(szToken))

typedef enum {
	NAMESPACE_UNKNOWN,
	NAMESPACE_SOAP,
	NAMESPACE_SRMP,
	NAMESPACE_SOAP_RP,
	NAMESPACE_MSMQ
} SOAP_NAMESPACE;
SOAP_NAMESPACE MapNamespace(const WCHAR *szNS, const DWORD cbNS);


// To add a new state and/or token:
// 1) Update SRMP_STATE and SRMP_TOKEN.
// 2) Update state table mapping in CSrmpToMsmq::SetState()
// 3) Update data structure and handeling of the new type in SrmpAccept.cxx/.hxx and associated files.
// 4) Update the tables in SrmpAccept.cxx

// Reserve top 24 bits for high-level information.
typedef enum {
	// Highest level of Soap Envelope
	SRMP_ST_UNINITIALIZED,
	SRMP_ST_UNKNOWN,
	SRMP_ST_ENVELOPE,
	SRMP_ST_HEADER,
	SRMP_ST_BODY,

	// Elements directly under HEADER,
	SRMP_ST_PATH               = 0x00000100,
	SRMP_ST_PROPERTIES         = 0x00000200,
	SRMP_ST_SERVICES           = 0x00000400,
	SRMP_ST_STREAM             = 0x00000800,
	SRMP_ST_STREAMRCPT         = 0x00001000,
	SRMP_ST_DELIVERYRCPT       = 0x00002000,
	SRMP_ST_COMMITMENTRCPT     = 0x00004000,
	SRMP_ST_MSMQ               = 0x00008000,
	SRMP_ST_SIGNATURE          = 0x00010000,

	SRMP_ST_PATH_ID            = SRMP_ST_PATH+1,
	SRMP_ST_PATH_TO,
	SRMP_ST_PATH_REV,
	SRMP_ST_PATH_REV_VIA,
	SRMP_ST_PATH_FROM,
	SRMP_ST_PATH_ACTION,
	SRMP_ST_PATH_RELATESTO,
	SRMP_ST_PATH_FIXED,
	SRMP_ST_PATH_FWD,
	SRMP_ST_PATH_FWD_VIA,
	SRMP_ST_PATH_FAULT,

	SRMP_ST_PROPERTIES_EXPIRESAT = SRMP_ST_PROPERTIES+1,
	SRMP_ST_PROPERTIES_DURATION,
	SRMP_ST_PROPERTIES_SENTAT,
	SRMP_ST_PROPERTIES_INREPLYTO,

	SRMP_ST_SERVICES_DURABLE     = SRMP_ST_SERVICES+1,
	SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST,
	SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST_SENDTO,
	SRMP_ST_SERVICES_DELIVERYRECEIPTREQUEST_SENDBY,
	SRMP_ST_SERVICES_FILTERDUPLICATES,
	SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST,
	SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_POSITIVEONLY,
	SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_NEGATIVEONLY,
	SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_SENDBY,
	SRMP_ST_SERVICES_COMMITMENTRECEIPTREQUEST_SENDTO,

	SRMP_ST_STREAM_STREAMID      = SRMP_ST_STREAM+1,
	SRMP_ST_STREAM_CURRENT,
	SRMP_ST_STREAM_PREVIOUS,
	SRMP_ST_STREAM_END,
	SRMP_ST_STREAM_START,
	SRMP_ST_STREAM_START_SENDRECEIPTSTO,
	SRMP_ST_STREAM_START_EXPIRESAT,
	SRMP_ST_STREAM_STREAMRECEIPTREQUEST,

	SRMP_ST_STREAMRCPT_RECEIVEDAT = SRMP_ST_STREAMRCPT+1,
	SRMP_ST_STREAMRCPT_STREAMID,
	SRMP_ST_STREAMRCPT_LASTORDINAL,
	SRMP_ST_STREAMRCPT_ID,

	SRMP_ST_DELIVERYRCPT_RECEIVEDAT  = SRMP_ST_DELIVERYRCPT+1,
	SRMP_ST_DELIVERYRCPT_ID,

	SRMP_ST_COMMITMENTRCPT_DECIDEDAT  = SRMP_ST_COMMITMENTRCPT+1,
	SRMP_ST_COMMITMENTRCPT_DECISION,
	SRMP_ST_COMMITMENTRCPT_ID,
	SRMP_ST_COMMITMENTRCPT_COMMITMENTCODE,
	SRMP_ST_COMMITMENTRCPT_XCOMMITMENTDETAIL,

	SRMP_ST_MSMQ_CLASS                = SRMP_ST_MSMQ+1,
	SRMP_ST_MSMQ_PRIORITY,
	SRMP_ST_MSMQ_JOURNAL,
	SRMP_ST_MSMQ_DEADLETTER,
	SRMP_ST_MSMQ_CORRELATION,
	SRMP_ST_MSMQ_TRACE,
	SRMP_ST_MSMQ_CONNECTORTYPE,
	SRMP_ST_MSMQ_APP,
	SRMP_ST_MSMQ_BODYTYPE,
	SRMP_ST_MSMQ_HASHALGORITHM,
	SRMP_ST_MSMQ_EOD,
	SRMP_ST_MSMQ_EOD_FIRST,
	SRMP_ST_MSMQ_EOD_LAST,
	SRMP_ST_MSMQ_EOD_XCONNECTORID,
	SRMP_ST_MSMQ_PROVIDER,
	SRMP_ST_MSMQ_PROVIDER_TYPE,
	SRMP_ST_MSMQ_PROVIDER_NAME,
	SRMP_ST_MSMQ_SOURCEQMGUID,
	SRMP_ST_MSMQ_DESTINATIONMQF,
	SRMP_ST_MSMQ_ADMINMQF,
	SRMP_ST_MSMQ_RESPONSEMQF,
	SRMP_ST_MSMQ_TTRQ
} SRMP_STATE;

#define SRMP_CLASS_MASK        0xFFFFFF00
#define SrmpType(state)   ((state) & SRMP_CLASS_MASK)
#define SrmpIndex(state)  ((state) & ~(SRMP_CLASS_MASK))

#define SRMP_FLAG_IGNORE_CHARS     0x00000001   // Ignore data set in characters()


typedef enum {
	SRMP_TOK_ACTION,
	SRMP_TOK_APP,
	SRMP_TOK_ADMINMQF,
	SRMP_TOK_BODY,
	SRMP_TOK_BODYTYPE,
	SRMP_TOK_COMMITMENTRECEIPT,
	SRMP_TOK_COMMITMENTCODE,
	SRMP_TOK_CURRENT,
	SRMP_TOK_CLASS,
	SRMP_TOK_CONNECTORTYPE,
	SRMP_TOK_COMMITMENTRECEIPTREQUEST,
	SRMP_TOK_CORRELATION,
	SRMP_TOK_DEADLETTER,
	SRMP_TOK_DELIVERYRECEIPT,
	SRMP_TOK_DECIDEDAT,
	SRMP_TOK_DECISION,
	SRMP_TOK_DURATION,
	SRMP_TOK_DURABLE,
	SRMP_TOK_DELIVERYRECEIPTREQUEST,
	SRMP_TOK_DESTINATIONMQF,
	SRMP_TOK_EOD,
	SRMP_TOK_EXPIRESAT,
	SRMP_TOK_ENVELOPE,
	SRMP_TOK_END,
	SRMP_TOK_FILTERDUPLICATES,
	SRMP_TOK_FIRST,
	SRMP_TOK_FROM,
	SRMP_TOK_FIXED,
	SRMP_TOK_FWD,
	SRMP_TOK_FAULT,
	SRMP_TOK_HEADER,
	SRMP_TOK_HASHALGORITHM,
	SRMP_TOK_ID,
	SRMP_TOK_INREPLYTO,
	SRMP_TOK_JOURNAL,
	SRMP_TOK_LASTORDINAL,
	SRMP_TOK_LAST,
	SRMP_TOK_MSMQ,
	SRMP_TOK_NAME,
	SRMP_TOK_NEGATIVEONLY,
	SRMP_TOK_PRIORITY,
	SRMP_TOK_PREVIOUS,
	SRMP_TOK_PATH,
	SRMP_TOK_PROPERTIES,
	SRMP_TOK_POSITIVEONLY,
	SRMP_TOK_PROVIDER,
	SRMP_TOK_REV,
	SRMP_TOK_RELATESTO,
	SRMP_TOK_RECEIVEDAT,
	SRMP_TOK_RESPONSEMQF,
	SRMP_TOK_SENTAT,
	SRMP_TOK_SERVICES,
	SRMP_TOK_SENDTO,
	SRMP_TOK_SENDBY,
	SRMP_TOK_STREAM,
	SRMP_TOK_STREAMID,
	SRMP_TOK_START,
	SRMP_TOK_SENDRECEIPTSTO,
	SRMP_TOK_STREAMRECEIPTREQUEST,
	SRMP_TOK_STREAMRECEIPT,
	SRMP_TOK_SIGNATURE,
	SRMP_TOK_SOURCEQMGUID,
	SRMP_TOK_TO,
	SRMP_TOK_TRACE,
	SRMP_TOK_TTRQ,
	SRMP_TOK_TYPE,
	SRMP_TOK_VIA,
	SRMP_TOK_XCOMMITMENTDETAIL,
	SRMP_TOK_XCONNECTORID,
	SRMP_TOK_UNKNOWN
} SRMP_TOKEN;

//
// Misc Constants
//

#define UUIDREFERENCE_PREFIX     L"uuid:"
#define UUIDREFERENCE_SEPERATOR  L"@"

extern const IID GUID_NULL;
extern const LONGLONG i64NoneMSMQSeqId;

extern const char cszPrefixMimeAttachment[];
extern const DWORD ccPrefixMimeAttachmentLen;

extern const char cszMimeBodyId[];

extern const char cszEnvelopeId[];
extern const DWORD ccEnvelopeIdLen;

extern const char cszMimeBodyId[];
extern const DWORD ccMimeBodyIdLen;

extern const char cszMimeSenderCertificateId[];
extern const DWORD ccMimeSenderCertificateIdLen;

extern const char cszMimeExtensionId[];
extern const DWORD ccMimeExtensionIdLen;

extern const char cszContentType[];
extern const char cszContentLength[];
extern const char cszContentId[];
extern const char cszBoundary[];
extern const char cszBoundaryHyphen[];

extern const char     cszCRLF[];
extern const char     cszCRLF2[];

extern const WCHAR cszPositive[];
extern const WCHAR cszNegative[];

extern const WCHAR cszMSMQVroot[];

// Strings as they are sent across the wire

// Envelope and top level
extern const WCHAR cszEnvelope[];
extern const WCHAR cszSoapEnv[];
extern const WCHAR cszBody[];

// Begin Header List
extern const WCHAR cszHeader[];


// Begin Path List.  All under SOAP_RP_NAMESPACE
extern const WCHAR cszPath[];
extern const WCHAR cszId[];
extern const WCHAR cszTo[];
extern const WCHAR cszRev[];
  extern const WCHAR cszVia[];
extern const WCHAR cszFrom[];
extern const WCHAR cszAction[];
extern const WCHAR cszRelatesTo[];
extern const WCHAR cszFixed[];
extern const WCHAR cszFwd[];
extern const WCHAR cszFault[];
// End Path List

// Begin Property list.  All SRMP_NAMESPACE, all 0,1.
extern const WCHAR cszProperties[];
extern const WCHAR cszExpiresAt[];
extern const WCHAR cszDuration[];
extern const WCHAR cszSentAt[];
extern const WCHAR cszInReplyTo[];
// End Property list.

// Begin Service list.  All SRMP_NAMESPACE
extern const WCHAR cszServices[];
extern const WCHAR cszDurable[];
extern const WCHAR cszDeliveryReceiptRequest[];
  extern const WCHAR cszSendTo[];
  extern const WCHAR cszSendBy[];
extern const WCHAR cszFilterDuplicates[];
extern const WCHAR cszCommitmentReceiptRequest[];
  extern const WCHAR cszPositiveOnly[];
  extern const WCHAR cszNegativeOnly[];
//  extern const WCHAR cszSendBy[];
//  extern const WCHAR cszSendTo[];
// End Service list.

// Begin Stream.  SRMP_NAMESPACE.
extern const WCHAR cszStream[];
extern const WCHAR cszStreamId[];
extern const WCHAR cszCurrent[];
extern const WCHAR cszPrevious[];
extern const WCHAR cszEnd[];
extern const WCHAR cszStart[];
  extern const WCHAR cszSendReceiptsTo[];
//  extern const WCHAR cszExpiresAt[];
extern const WCHAR cszStreamReceiptRequest[];
// End Stream

// Begin stream receipt.  SRMP_NAMESPACE
extern const WCHAR cszStreamReceipt[];
extern const WCHAR cszreceivedAt[];
//extern const WCHAR cszStreamId[];
extern const WCHAR cszlastOrdinal[];
//extern const WCHAR cszId[];
// End stream receipt.

// Begin deliveryReceipt.  SRMP_NAMESPACE
extern const WCHAR cszDeliveryReceipt[];
//extern const WCHAR cszreceivedAt[];
//extern const WCHAR cszId[];
// End deliveryReceipt.

// Begin commitmentReceipt.  SRMP_NAMESPACE
extern const WCHAR cszCommitmentReceipt[];
extern const WCHAR cszDecidedAt[];
extern const WCHAR cszDecision[];
//extern const WCHAR cszId[];
extern const WCHAR cszCommitmentCode[];
extern const WCHAR cszXCommitmentDetail[];
// End commitmentReceipt

// Begin MSMQ.  MSMQ_NAMESPACE
extern const WCHAR cszMsmq[];
extern const WCHAR cszClass[];
extern const WCHAR cszPriority[];
extern const WCHAR cszJournal[];
extern const WCHAR cszDeadLetter[];
extern const WCHAR cszCorrelation[];
extern const WCHAR cszTrace[];
extern const WCHAR cszConnectorType[];
extern const WCHAR cszApp[];
extern const WCHAR cszBodyType[];
extern const WCHAR cszHashAlgorithm[];
extern const WCHAR cszEod[];
  extern const WCHAR cszFirst[];
  extern const WCHAR cszLast[];
  extern const WCHAR cszXConnectorId[];
extern const WCHAR cszProvider[];
  extern const WCHAR cszType[];
  extern const WCHAR cszName[];
extern const WCHAR cszSourceQmGuid[];
extern const WCHAR cszDestinationMqf[];
extern const WCHAR cszAdminMqf[];
extern const WCHAR cszResponseMqf[];
extern const WCHAR cszTTrq[];
// End MSMQ

extern const DWORD  ccCRLF;
extern const DWORD  ccCRLF2;
extern const DWORD  ccBoundary;
extern const DWORD  ccBoundaryHyphen;
extern const DWORD  ccContentLength;
extern const DWORD  ccContentType;

// One entry for Signature.  UNKNOWN_NAMESPACE
extern const WCHAR cszSignature[];
// End Header list

#endif
