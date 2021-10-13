//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++


Module Name:

    srmpmsg.hxx

Abstract:

    Convert SRMP packet to MSMQ packet.


--*/


#if ! defined (__srmpmsg_HXX__)
#define __srmpmsg_HXX__	1

#include "SrmpMime.hxx"
#include "SrmpDefs.hxx"
#include "qformat.h"
#include "ScPacket.hxx"

#define IsSetSrmp(variable,state)    ((variable) &  (1 << SrmpIndex((state))))
#define SetSrmp(variable,state)      ((variable) |= (1 << SrmpIndex((state))))
#define CheckAndSet(variable,state)  if (IsSetSrmp((variable),(state))) return E_FAIL; SetSrmp((variable),(state));

#define IsHeaderSet(state)                (cMsgProps.dwHeaderSet & (state))
#define SetHeader(state)                  (cMsgProps.dwHeaderSet |= (state)) 

class CSrmpMessageProperties {
friend class CSrmpToMsmq;
	// Basic envelope validation and conversion from UTF8.  Do not free!
	PSrmpIOCTLPacket   pHttpParams;

	// Parsed from SRMP message.
	OBJECTID messageId;
	CSrmpMime cMime;
	WCHAR *szTitle;
	WCHAR *szStreamIdBuf;
	WCHAR *szOrderQueue;

	BSTR bstrEnvelope;
	DWORD ccEnvelope;

	UCHAR delivery;
	UCHAR acknowledgeType;
	UCHAR fFirst;
	UCHAR fLast;
	UCHAR fTrace;
	UCHAR auditing; 
	UCHAR priority;
	
	DWORD absoluteTimeToLive;
	DWORD absoluteTimeToQueue;
	DWORD sentTime;

	USHORT   classValue;
    DWORD    applicationTag;
    DWORD    bodyType;
	UCHAR    *pCorrelationID;

	GUID           connectorType;
	GUID           connectorId;
	GUID           SourceQmGuid;
	QUEUE_FORMAT   destQueue;
	QUEUE_FORMAT   adminQueue;
	QUEUE_FORMAT   responseQueue;

	WCHAR          *EodStreamId; // DO NOT FREE!  Points into szStreamIdBuf.
	LONGLONG       EodSeqId;
	DWORD          EodSeqNo;
	DWORD          EodPrevSeqNo;

	WCHAR          *EodAckStreamId;
	LONGLONG       EodAckSeqId;
	LONGLONG       EodAckSeqNo;

//	DWORD          providerType;
//	WCHAR          *szProviderName;
//  DWORD          hashAlgorithm;

	WCHAR          *szDestQueue;
	WCHAR          *szResponseQueue;
	WCHAR          *szAdminQueue;

	// Additional variables that are SOAP router specific
	WCHAR          *szRelatesTo;
	WCHAR          *szFrom;
	DWORD          ccRelatesTo;
	DWORD          ccFrom;

	SVSSimpleBuffer  revViaBuf;
	SVSSimpleBuffer  fwdViaBuf;

	// Keep track of what vars have been set already during SAX processing.
	DWORD    dwHeaderSet;
	DWORD    dwPathSet;
	DWORD    dwServiceSet;
	DWORD    dwPropertiesSet;
	DWORD    dwStreamSet;
	DWORD    dwStreamRcptSet;
	DWORD    dwDeliveryRcptSet;
	DWORD    dwCommitRcptSet;
	DWORD    dwMsmqSet;

public:
	CSrmpMessageProperties(PSrmpIOCTLPacket pIOCTL);
	~CSrmpMessageProperties();

	BOOL IsInitialized(void)   { return (cMime.IsInitialized()) ? TRUE : FALSE; }

	BOOL IsMSMQIncluded(void) { return (dwHeaderSet & SRMP_ST_MSMQ);   }
	BOOL IsXActIncluded(void) { return (dwHeaderSet & SRMP_ST_STREAM); }

	BOOL      VerifyProperties(void);
	DWORD     CalculatePacketSize(DWORD *pdwBaseSize);
	ScPacketImage * BuildPacketImage(void);
};



#endif
