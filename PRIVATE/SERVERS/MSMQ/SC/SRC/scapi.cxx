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

    scapi.cxx

Abstract:

    Small client driver


--*/
#include <sc.hxx>
#include <scqman.hxx>
#include <scqueue.hxx>
#include <scpacket.hxx>
#include <scfile.hxx>

#include <mq.h>
#include <mqformat.h>

#include <scapi.h>

#include <scoverio.hxx>
#include <sccomp.hxx>

#include <scsrmp.hxx>
#include <scsman.hxx>

#if defined (winCE) && defined (SDK_BUILD)
#include <pstub.h>
#endif

//
//	Service code section
//
#if defined (winCE)
void *scapi_GetOwnerProcId (void) {
	return GetCallerProcess();
}
#else
void *scapi_GetOwnerProcId (void) {
	return NULL;
}
#endif

ULONG CalcFwdRevViaSectionSize(CAPROPVARIANT *pEntry, HANDLE hCallerProc);
ULONG CalcFwdRevViaSectionSize(CDataHeader *pFwdRevEntry, DWORD *pdwElements);
void  SetFwdRevViaSection(CDataHeader *pFwdRevEntry, CAPROPVARIANT *pPropVar, DWORD dwElements);

static HRESULT scapi_FillQueueInfo (int iQueueLenNdx, int iQueueNdx, SCPROPVAR *pMsgProps, QUEUE_FORMAT *pqft, HANDLE hCallerProc) {
	HRESULT hRes = MQ_OK;

	if (iQueueLenNdx >= 0) {
		int ccLen = pMsgProps->aPropVar[iQueueLenNdx].ulVal;
		WCHAR *lpszQueueName = scutil_QFtoString (pqft);

		if (! lpszQueueName) {
			hRes = MQ_ERROR_INSUFFICIENT_RESOURCES;
			pMsgProps->aPropVar[iQueueLenNdx].ulVal = 0;
		} else {
			int ccLenReal = wcslen (lpszQueueName) + 1;

			pMsgProps->aPropVar[iQueueLenNdx].ulVal = ccLenReal;

			if (ccLenReal > ccLen) {
				hRes = MQ_ERROR_BUFFER_OVERFLOW;
				ccLenReal = ccLen;
			}

			if ((iQueueNdx >= 0) && (ccLenReal > 0)) {
#if defined (winCE)
				WCHAR *lpszDest = (WCHAR *)MapPtrToProcess((void *)pMsgProps->aPropVar[iQueueNdx].pwszVal, hCallerProc);
#else
				WCHAR *lpszDest = pMsgProps->aPropVar[iQueueNdx].pwszVal;
#endif
				memcpy (lpszDest, lpszQueueName, ccLenReal * sizeof(WCHAR));
				lpszDest[ccLenReal-1] = L'\0';
			}

			g_funcFree (lpszQueueName, g_pvFreeData);
		}
	} else if (iQueueNdx >= 0)
		hRes = MQ_ERROR_PROPERTY;

	if ((iQueueNdx >= 0) && (pMsgProps->aStatus))
		pMsgProps->aStatus[iQueueNdx] = hRes;

	return hRes;
}

static HRESULT scapi_GetPacket
(
SCHANDLE			hQueue,
SCHANDLE			hCursor,
DWORD				dwAction,
ScPacket			*&pPacket,
ScQueue				*&pQueue,
ScHandleInfo		*&pHInfo
) {
	SVSUTIL_ASSERT (gMem->IsLocked ());

	pPacket = NULL;
	pQueue  = NULL;
	pHInfo  = NULL;

	int	fDefaultCursor = ((SVSHandle)hCursor == SVSUTIL_HANDLE_INVALID);

	ScHandleInfo *pQueueHInfo = gQueueMan->QueryHandle ((SVSHandle)hQueue);

	if ((! pQueueHInfo) || (pQueueHInfo->uiHandleType != SCQMAN_HANDLE_QUEUE))
		return MQ_ERROR_INVALID_HANDLE;

	if (((pQueueHInfo->q.uiAccess != MQ_RECEIVE_ACCESS) && (pQueueHInfo->q.uiAccess != MQ_PEEK_ACCESS)) ||
		((pQueueHInfo->q.uiAccess != MQ_RECEIVE_ACCESS) && (dwAction == MQ_ACTION_RECEIVE)))
		return MQ_ERROR_ACCESS_DENIED;

	pQueue = pQueueHInfo->pQueue;

	if  (fDefaultCursor)
		hCursor = (SCHANDLE)pQueueHInfo->q.hDefaultCursor;

	pHInfo = gQueueMan->QueryHandle ((SVSHandle)hCursor);

	if ((! pHInfo) || (pHInfo->uiHandleType != SCQMAN_HANDLE_CURSOR) || (pHInfo->pQueue != pQueue)) {
		pHInfo = NULL;
		pQueue = NULL;

		return MQ_ERROR_INVALID_HANDLE;
	}

	if (! pHInfo->c.fPosValid) {
		if ((dwAction == MQ_ACTION_PEEK_NEXT) || (dwAction == MQ_ACTION_PEEK_PREV)) {
			pQueue = NULL;
			pHInfo = NULL;

			return MQ_ERROR_ILLEGAL_CURSOR_ACTION;
		}

		if (fDefaultCursor)
			pQueue->Reset (pHInfo);

		if (! pQueue->Advance (pHInfo)) {
			pHInfo = NULL;

			return MQ_OK;
		}

		pHInfo->c.fPosValid = TRUE;
	} else {
		int fSuccess = TRUE;
		if ((dwAction == MQ_ACTION_RECEIVE) || (dwAction == MQ_ACTION_PEEK_CURRENT)) {
			if (fDefaultCursor) {
				pQueue->Reset (pHInfo);

				if (! pQueue->Advance (pHInfo)) {
					pHInfo = NULL;

					return MQ_OK;
				}
			}

			ScPacket *pCandidate = (ScPacket *)SVSTree::GetData(pHInfo->c.pNode);
			if (! ((pCandidate->uiPacketState == SCPACKET_STATE_ALIVE) && did_not_expire (pCandidate->tExpirationTime, scutil_now ()))) {
				pQueue = NULL;
				pHInfo = NULL;

				return MQ_ERROR_MESSAGE_ALREADY_RECEIVED;
			}
		} else if (fDefaultCursor) {
			pQueue = NULL;
			pHInfo = NULL;

			return MQ_ERROR_ILLEGAL_CURSOR_ACTION;
		} else if (dwAction == MQ_ACTION_PEEK_NEXT)
			fSuccess = pQueue->Advance (pHInfo);
		else if (dwAction == MQ_ACTION_PEEK_PREV)
			fSuccess = pQueue->Backup (pHInfo);

		if (! fSuccess) {
			pHInfo = NULL;
			return MQ_OK;
		}
	}

	SVSUTIL_ASSERT (pHInfo->c.pNode);

	pPacket = (ScPacket *)SVSTree::GetData(pHInfo->c.pNode);

	return MQ_OK;
}

static HRESULT scapi_RetrievePacketInfo
(
ScPacket		*pPacket,
ScQueue			*pQueue,
ScHandleInfo	*pHInfo,
DWORD			dwAction,
SCPROPVAR		*pMsgPropsA,
HANDLE			hCallerProc
) {
	SVSUTIL_ASSERT (pPacket && pHInfo);
	SVSUTIL_ASSERT (gMem->IsLocked ());

#if defined (winCE)
	pMsgPropsA = MAP(pMsgPropsA, SCPROPVAR *);

	SCPROPVAR	sMsgPropsL;

	if (pMsgPropsA) {
		sMsgPropsL.cProp    = pMsgPropsA->cProp;
		sMsgPropsL.aPropID  = MAP(pMsgPropsA->aPropID, PROPID *);
		sMsgPropsL.aPropVar = MAP(pMsgPropsA->aPropVar,PROPVARIANT *);
		sMsgPropsL.aStatus  = MAP(pMsgPropsA->aStatus, HRESULT *);
	} else
		memset (&sMsgPropsL, 0, sizeof (sMsgPropsL));

	SCPROPVAR	*pMsgProps = &sMsgPropsL;
#else
	SCPROPVAR	sMsgPropsL;
	memset (&sMsgPropsL, 0, sizeof (sMsgPropsL));

	SCPROPVAR	*pMsgProps  = pMsgPropsA ? pMsgPropsA : &sMsgPropsL;
#endif

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_IO, L"Got a message!\n");
#endif
	//
	//	pPacket is here, the queue is locked and the cursor handle pointer is in pHInfo...
	//
	HRESULT hRes = MQ_OK;

	//
	//	Restore packet's image if necessary
	//
	if (! pQueue->BringToMemory (pPacket))
		return MQ_ERROR;

	SVSUTIL_ASSERT (pPacket->pImage && pPacket->pImage->sect.pBaseHeader && pPacket->pImage->sect.pUserHeader && pPacket->pImage->sect.pPropHeader);

	SVSUTIL_ASSERT (pPacket->uiAuditType == pPacket->pImage->sect.pUserHeader->GetAuditing ());
	SVSUTIL_ASSERT (pPacket->uiPacketState == SCPACKET_STATE_ALIVE);
	SVSUTIL_ASSERT (pPacket->uiAckType == pPacket->pImage->sect.pPropHeader->GetAckType ());

	if (pMsgProps->aStatus && pMsgProps->cProp > 0)
		memset (pMsgProps->aStatus, 0, sizeof (HRESULT) * pMsgProps->cProp);

	int iAdminQueueNdx     = -1;
	int iAdminQueueLenNdx  = -1;
	int iBodyNdx		   = -1;
	int iBodySizeNdx	   = -1;
	int iDestQueueNdx      = -1;
	int iDestQueueLenNdx   = -1;
	int iExtNdx			   = -1;
	int iExtSizeNdx        = -1;
	int iLabelNdx		   = -1;
	int iLabelLenNdx       = -1;
	int iRespQueueNdx      = -1;
	int iRespQueueLenNdx   = -1;
	int iSoapEnvNdx        = -1;
	int iSoapEnvLenNdx     = -1;
	int iCompoundMsgNdx    = -1;
	int iCompoundMsgLenNdx = -1;
	int iSoapFwdViaNdx     = -1;
	int iSoapFwdViaLenNdx  = -1;
	int iSoapRevViaNdx     = -1;
	int iSoapRevViaLenNdx  = -1;
	int iSoapFromNdx       = -1;
	int iSoapFromLenNdx    = -1;
	int iSoapRelatesToNdx    = -1;
	int iSoapRelatesToLenNdx = -1;


	CCompoundMessageHeader *pCompoundMessage = pPacket->pImage->sect.pUserHeader->SrmpIsIncluded() ? pPacket->pImage->sect.pCompoundMessageHeader : NULL;

	for (int i = 0 ; (! FAILED (hRes)) && (i < (int)pMsgProps->cProp) ; ++i) {
		hRes = MQ_OK;
		switch (pMsgProps->aPropID[i]) {
		case PROPID_M_ACKNOWLEDGE:
			if (pMsgProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].bVal = pPacket->pImage->sect.pPropHeader->GetAckType();
			break;

		case PROPID_M_ADMIN_QUEUE:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				iAdminQueueNdx = i;
			break;

		case PROPID_M_ADMIN_QUEUE_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iAdminQueueLenNdx = i;
			break;

		case PROPID_M_APPSPECIFIC:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				pMsgProps->aPropVar[i].ulVal = pPacket->pImage->sect.pPropHeader->GetApplicationTag();;
			break;

		case PROPID_M_AUTH_LEVEL:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				pMsgProps->aPropVar[i].ulVal = MQMSG_AUTH_LEVEL_NONE;
			break;

		case PROPID_M_BODY:
			if ((pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_UI1)) ||
				(pMsgProps->aPropVar[i].caub.cElems <= 0) ||
				(! pMsgProps->aPropVar[i].caub.pElems))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iBodyNdx = i;
			break;

		case PROPID_M_BODY_SIZE:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iBodySizeNdx = i;
			break;

		case PROPID_M_BODY_TYPE:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				pMsgProps->aPropVar[i].ulVal = pPacket->pImage->sect.pPropHeader->GetBodyType ();
			break;

		case PROPID_M_CLASS:
			if (pMsgProps->aPropVar[i].vt != VT_UI2)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].uiVal = pPacket->pImage->sect.pPropHeader->GetClass ();
			break;

		case PROPID_M_CORRELATIONID:
			if ((pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_UI1)) ||
				(pMsgProps->aPropVar[i].caub.cElems <= 0) ||
				(pMsgProps->aPropVar[i].caub.cElems > 20) ||
				(! pMsgProps->aPropVar[i].caub.pElems))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pPacket->pImage->sect.pPropHeader->GetCorrelationID(MAP(pMsgProps->aPropVar[i].caub.pElems, unsigned char *));
			break;

		case PROPID_M_DELIVERY:
			if (pMsgProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].bVal = pPacket->pImage->sect.pUserHeader->GetDelivery ();
			break;

		case PROPID_M_DEST_QUEUE:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iDestQueueNdx = i;
			break;

		case PROPID_M_DEST_QUEUE_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iDestQueueLenNdx = i;
			break;

		case PROPID_M_EXTENSION:
			if ((pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_UI1)) ||
				(pMsgProps->aPropVar[i].caub.cElems <= 0) ||
				(! pMsgProps->aPropVar[i].caub.pElems))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iExtNdx = i;
			break;

		case PROPID_M_EXTENSION_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iExtSizeNdx = i;
			break;

		case PROPID_M_JOURNAL:
			if (pMsgProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].bVal = pPacket->pImage->sect.pUserHeader->GetAuditing();
			break;

		case PROPID_M_LABEL:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iLabelNdx = i;
			break;

		case PROPID_M_LABEL_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iLabelLenNdx = i;
			break;

		case PROPID_M_PRIORITY:
			if ((pMsgProps->aPropVar[i].vt != VT_UI1))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].bVal = pPacket->pImage->sect.pBaseHeader->GetPriority();
			break;

		case PROPID_M_RESP_QUEUE:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iRespQueueNdx = i;
			break;

		case PROPID_M_RESP_QUEUE_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iRespQueueLenNdx = i;
			break;

		case PROPID_M_TIME_TO_BE_RECEIVED:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = pPacket->pImage->sect.pUserHeader->GetTimeToLiveDelta();
			break;

		case PROPID_M_TIME_TO_REACH_QUEUE:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = pPacket->pImage->sect.pBaseHeader->GetAbsoluteTimeToQueue();
			break;

		case PROPID_M_TRACE:
			if (pMsgProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].uiVal = pPacket->pImage->sect.pBaseHeader->GetTraced();
			break;

		case PROPID_M_XACT_STATUS_QUEUE:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				(MAP(pMsgProps->aPropVar[i].pwszVal, WCHAR *))[0] = L'\0';
			break;

		case PROPID_M_XACT_STATUS_QUEUE_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = 0;
			break;

		case PROPID_M_VERSION:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = pMsgProps->aPropVar[i].ulVal = pPacket->pImage->sect.pBaseHeader->GetVersion();
			break;

		case PROPID_M_SRC_MACHINE_ID:
			if (pMsgProps->aPropVar[i].vt != VT_CLSID)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				*MAP(pMsgProps->aPropVar[i].puuid, GUID *) = *pPacket->pImage->sect.pUserHeader->GetSourceQM();
			break;

		case PROPID_M_SIGNATURE:
			break;

		case PROPID_M_SIGNATURE_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = 0;
			break;

		case PROPID_M_SENTTIME:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = pMsgProps->aPropVar[i].ulVal = pPacket->pImage->sect.pUserHeader->GetSentTime();
			break;

		case PROPID_M_SENDERID_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = 0;
			break;

		case PROPID_M_SENDERID_TYPE:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = MQMSG_SENDERID_TYPE_NONE;
			break;

		case PROPID_M_SENDERID:
			break;

		case PROPID_M_SENDER_CERT_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = 0;
			break;

		case PROPID_M_SENDER_CERT:
			break;

		case PROPID_M_PROV_NAME_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = 0;
			break;

		case PROPID_M_PROV_NAME:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				(MAP(pMsgProps->aPropVar[i].pwszVal, WCHAR *))[0] = L'\0';
			break;

		case PROPID_M_PRIV_LEVEL:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = MQMSG_PRIV_LEVEL_NONE;
			break;

		case PROPID_M_MSGID:
			if ((pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_UI1)) ||
				(pMsgProps->aPropVar[i].caub.cElems <= 0) ||
				(pMsgProps->aPropVar[i].caub.cElems > 20) ||
				(! pMsgProps->aPropVar[i].caub.pElems))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pPacket->pImage->sect.pUserHeader->GetMessageID (MAP(pMsgProps->aPropVar[i].caub.pElems, OBJECTID *));
			break;

		case PROPID_M_ARRIVEDTIME:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = pPacket->tCreationTime;
			break;

		case PROPID_M_DEST_SYMM_KEY:
			break;

		case PROPID_M_DEST_SYMM_KEY_LEN:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				(MAP(pMsgProps->aPropVar[i].pwszVal, WCHAR *))[0] = L'\0';
			break;

		case PROPID_M_CONNECTOR_TYPE:
			if (pMsgProps->aPropVar[i].vt != VT_CLSID)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				memset (pMsgProps->aPropVar[i].puuid, 0, sizeof(GUID));
			break;

		case PROPID_M_AUTHENTICATED:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgProps->aPropVar[i].ulVal = 0;
			break;

		case PROPID_M_HASH_ALG:
		case PROPID_M_ENCRYPTION_ALG:
		case PROPID_M_PROV_TYPE:
		case PROPID_M_SECURITY_CONTEXT:
			hRes = MQ_ERROR_COMPUTER_DOES_NOT_SUPPORT_ENCRYPTION;
			break;

		case PROPID_M_FIRST_IN_XACT:
			if (pMsgProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else {
				CXactHeader *pXactHeader = pPacket->pImage->sect.pXactHeader;
				pMsgProps->aPropVar[i].ulVal = pXactHeader ? pXactHeader->GetFirstInXact() : FALSE;
			}
			break;

		case PROPID_M_LAST_IN_XACT:
			if (pMsgProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else {
				CXactHeader *pXactHeader = pPacket->pImage->sect.pXactHeader;
				pMsgProps->aPropVar[i].ulVal = pXactHeader ? pXactHeader->GetLastInXact() : FALSE;
			}
			break;

		case PROPID_M_XACTID:
			if (pMsgProps->aPropVar[i].vt  != (VT_UI1|VT_VECTOR))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else if (pMsgProps->aPropVar[i].caub.cElems != sizeof(OBJECTID))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_SIZE;
			else {
				CXactHeader *pXactHeader = pPacket->pImage->sect.pXactHeader;
				if (pXactHeader) {
					OBJECTID xactID;
					xactID.Lineage    = *pPacket->pImage->sect.pUserHeader->GetSourceQM();
					xactID.Uniquifier = pXactHeader->GetXactIndex();
					memcpy(MAP(pMsgProps->aPropVar[i].caub.pElems, CHAR*),&xactID,sizeof(OBJECTID));
				}
				else {
					memset(MAP(pMsgProps->aPropVar[i].caub.pElems, CHAR*),0,sizeof(OBJECTID));
				}
			}
			break;

		case PROPID_M_SOAP_ENVELOPE:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iSoapEnvNdx = i;
			break;


		case PROPID_M_SOAP_ENVELOPE_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iSoapEnvLenNdx = i;
			break;

		case PROPID_M_COMPOUND_MESSAGE:
			if (pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_UI1))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iCompoundMsgNdx = i;
			break;

		case PROPID_M_COMPOUND_MESSAGE_SIZE:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iCompoundMsgLenNdx = i;
			break;

		case PROPID_M_SOAP_FWD_VIA_SIZE:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iSoapFwdViaLenNdx = i;
			break;

		case PROPID_M_SOAP_FWD_VIA:
			if (pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_VARIANT))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iSoapFwdViaNdx = i;
			break;

		case PROPID_M_SOAP_REV_VIA_SIZE:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iSoapRevViaLenNdx = i;
			break;

		case PROPID_M_SOAP_REV_VIA:
			if (pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_VARIANT))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iSoapRevViaNdx = i;
			break;

		case PROPID_M_SOAP_FROM:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				iSoapFromNdx = i;
			break;

		case PROPID_M_SOAP_FROM_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iSoapFromLenNdx = i;
			break;

		case PROPID_M_SOAP_RELATES_TO:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				iSoapRelatesToNdx = i;
			break;

		case PROPID_M_SOAP_RELATES_TO_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				iSoapRelatesToLenNdx = i;
			break;

		default:
			hRes = MQ_ERROR_PROPERTY;
		}

		if (pMsgProps->aStatus)
			pMsgProps->aStatus[i] = hRes;
	}

	if (FAILED(hRes))
		return hRes;

	QUEUE_FORMAT qft;
	//
	//	Admin queue
	//
	//
	hRes = MQ_OK;

	if (pPacket->pImage->sect.pUserHeader->GetAdminQueue (&qft))
		hRes = scapi_FillQueueInfo (iAdminQueueLenNdx, iAdminQueueNdx, pMsgProps, &qft, hCallerProc);
	else if (iAdminQueueLenNdx >= 0)
		pMsgProps->aPropVar[iAdminQueueLenNdx].ulVal = 0;

	if (FAILED(hRes))
		return hRes;

	//
	//	Response queue
	//
	hRes = MQ_OK;

	if (pPacket->pImage->sect.pUserHeader->GetResponseQueue (&qft))
		hRes = scapi_FillQueueInfo (iRespQueueLenNdx, iRespQueueNdx, pMsgProps, &qft, hCallerProc);
	else if (iRespQueueLenNdx >= 0)
		pMsgProps->aPropVar[iRespQueueLenNdx].ulVal = 0;

	if (FAILED(hRes))
		return hRes;

	//
	//	Dest queue
	//
	hRes = MQ_OK;


	if (pPacket->pImage->sect.pUserHeader->GetDestinationQueue (&qft))
		hRes = scapi_FillQueueInfo (iDestQueueLenNdx, iDestQueueNdx, pMsgProps, &qft, hCallerProc);
	else if (iDestQueueLenNdx >= 0)
		pMsgProps->aPropVar[iDestQueueLenNdx].ulVal = 0;

	if (FAILED(hRes))
		return hRes;

	//
	//	Label
	//
	int ccLabelLen = (iLabelLenNdx < 0) ? 0 :  pMsgProps->aPropVar[iLabelLenNdx].ulVal;

	if (iLabelNdx >= 0)
		pPacket->pImage->sect.pPropHeader->GetTitle (MAP(pMsgProps->aPropVar[iLabelNdx].pwszVal, WCHAR *), ccLabelLen);

	if (iLabelLenNdx >= 0)
		pMsgProps->aPropVar[iLabelLenNdx].ulVal = pPacket->pImage->sect.pPropHeader->GetTitleLength();

	//
	//	Body
	//
	DWORD cbBodySize = pMsgProps->aPropVar[iBodyNdx].caub.cElems;

	if (iBodySizeNdx >= 0) {
		if (pCompoundMessage)
			pMsgProps->aPropVar[iBodySizeNdx].ulVal = pCompoundMessage->GetBodySizeInBytes();
		else
			pMsgProps->aPropVar[iBodySizeNdx].ulVal = pPacket->pImage->sect.pPropHeader->GetBodySize ();
	}
	
	if (iBodyNdx >= 0) {
		if (pCompoundMessage) {
			if (cbBodySize < pCompoundMessage->GetBodySizeInBytes())
				return MQ_ERROR_BUFFER_OVERFLOW;
			pCompoundMessage->GetBody(MAP(pMsgProps->aPropVar[iBodyNdx].caub.pElems, unsigned char *),cbBodySize);
		}
		else {
			if (cbBodySize < pPacket->pImage->sect.pPropHeader->GetBodySize ())
				return MQ_ERROR_BUFFER_OVERFLOW;
			pPacket->pImage->sect.pPropHeader->GetBody(MAP(pMsgProps->aPropVar[iBodyNdx].caub.pElems, unsigned char *), cbBodySize);
		}
	}

	//
	//	Extension
	//
	if (iExtNdx >= 0)
		pPacket->pImage->sect.pPropHeader->GetMsgExtension (MAP(pMsgProps->aPropVar[iExtNdx].caub.pElems, unsigned char *), pMsgProps->aPropVar[iBodyNdx].caub.cElems);

	if (iExtSizeNdx >= 0)
		pMsgProps->aPropVar[iExtSizeNdx].ulVal = pPacket->pImage->sect.pPropHeader->GetMsgExtensionSize ();

	//
	//  SRMP Envelope
	//
	ULONG ccSoapEnvLen = (iSoapEnvLenNdx < 0) ? 0 : pMsgProps->aPropVar[iSoapEnvLenNdx].ulVal;
	if (iSoapEnvLenNdx >= 0)
		pMsgProps->aPropVar[iSoapEnvLenNdx].ulVal = pPacket->pImage->sect.pSrmpEnvelopeHeader ? pPacket->pImage->sect.pSrmpEnvelopeHeader->GetDataLengthInWCHARs() : 0;

	if (iSoapEnvNdx >= 0 && pPacket->pImage->sect.pSrmpEnvelopeHeader) {
		// even in overflow copy as much data as possible into buffer.
		pPacket->pImage->sect.pSrmpEnvelopeHeader->GetData(MAP(pMsgProps->aPropVar[iSoapEnvNdx].pwszVal,WCHAR*),ccSoapEnvLen);
		if (ccSoapEnvLen < pPacket->pImage->sect.pSrmpEnvelopeHeader->GetDataLengthInWCHARs())
			return MQ_ERROR_BUFFER_OVERFLOW;
	}

	// 
	// Compound Message
	//
	ULONG ccCompoundMsgLen = (iCompoundMsgNdx < 0) ? 0 : pMsgProps->aPropVar[iCompoundMsgNdx].caub.cElems;
	if (iCompoundMsgLenNdx >= 0)
		pMsgProps->aPropVar[iCompoundMsgLenNdx].ulVal = pPacket->pImage->sect.pCompoundMessageHeader ? pPacket->pImage->sect.pCompoundMessageHeader->GetDataSizeInBytes() : 0;

	if (iCompoundMsgNdx >= 0 && pPacket->pImage->sect.pCompoundMessageHeader) {
		// even in overflow copy as much data as possible into buffer.
		pPacket->pImage->sect.pCompoundMessageHeader->GetData(MAP(pMsgProps->aPropVar[iCompoundMsgNdx].caub.pElems,unsigned char*),ccCompoundMsgLen);
		if (ccCompoundMsgLen < pPacket->pImage->sect.pCompoundMessageHeader->GetBodySizeInBytes())
			return MQ_ERROR_BUFFER_OVERFLOW;
	}

	//
	// SOAP <fwd><via> entries.
	//
	ULONG cbSoapFwdViaLen       = (iSoapFwdViaLenNdx < 0) ? 0 : pMsgProps->aPropVar[iSoapFwdViaLenNdx].ulVal;
	ULONG cbSoapFwdViaRequired  = 0;
	ULONG cFwdElements          = 0;

	if (iSoapFwdViaLenNdx >= 0) {
		cbSoapFwdViaRequired = CalcFwdRevViaSectionSize(pPacket->pImage->sect.pFwdViaHeader,&cFwdElements);
		pMsgProps->aPropVar[iSoapFwdViaLenNdx].ulVal = cbSoapFwdViaRequired;
	}

	if (iSoapFwdViaNdx >= 0 && pPacket->pImage->sect.pFwdViaHeader) {
		if (cbSoapFwdViaLen < cbSoapFwdViaRequired)
			return MQ_ERROR_BUFFER_OVERFLOW;

		SetFwdRevViaSection(pPacket->pImage->sect.pFwdViaHeader,&pMsgProps->aPropVar[iSoapFwdViaNdx].capropvar,cFwdElements);
	}

	//
	// SOAP <rev><via> entries.
	//
	ULONG cbSoapRevViaLen = (iSoapRevViaLenNdx < 0) ? 0 : pMsgProps->aPropVar[iSoapRevViaLenNdx].ulVal;
	ULONG cbSoapRevViaRequired  = 0;
	ULONG cRevElements          = 0;

	if (iSoapRevViaLenNdx >= 0) {
		cbSoapRevViaRequired = CalcFwdRevViaSectionSize(pPacket->pImage->sect.pRevViaHeader,&cRevElements);
		pMsgProps->aPropVar[iSoapRevViaLenNdx].ulVal = cbSoapRevViaRequired;
	}

	if (iSoapRevViaNdx >= 0 && pPacket->pImage->sect.pRevViaHeader) {
		if (cbSoapRevViaLen < cbSoapRevViaRequired)
			return MQ_ERROR_BUFFER_OVERFLOW;

		SetFwdRevViaSection(pPacket->pImage->sect.pRevViaHeader,&pMsgProps->aPropVar[iSoapRevViaNdx].capropvar,cRevElements);
	}

	//
	// SOAP <from> element
	//
	ULONG ccSoapFromLen = (iSoapFromLenNdx < 0) ? 0 : pMsgProps->aPropVar[iSoapFromLenNdx].ulVal;
	if (iSoapFromLenNdx >= 0)
		pMsgProps->aPropVar[iSoapFromLenNdx].ulVal = pPacket->pImage->sect.pSoapExtHeaderSection ? pPacket->pImage->sect.pSoapExtHeaderSection->GetFromLengthInWCHARs() : 0;

	if (iSoapFromNdx >= 0 && pPacket->pImage->sect.pSoapExtHeaderSection) {
		pPacket->pImage->sect.pSoapExtHeaderSection->GetFrom(MAP(pMsgProps->aPropVar[iSoapFromNdx].pwszVal,WCHAR*),ccSoapFromLen);
		if (ccSoapFromLen < pPacket->pImage->sect.pSoapExtHeaderSection->GetFromLengthInWCHARs())
			return MQ_ERROR_BUFFER_OVERFLOW;
	}

	// 
	// SOAP <relatesTo> element
	//
	ULONG ccSoapRelatesToLen = (iSoapRelatesToLenNdx < 0) ? 0 : pMsgProps->aPropVar[iSoapRelatesToLenNdx].ulVal;
	if (iSoapRelatesToLenNdx >= 0)
		pMsgProps->aPropVar[iSoapRelatesToLenNdx].ulVal = pPacket->pImage->sect.pSoapExtHeaderSection ? pPacket->pImage->sect.pSoapExtHeaderSection->GetRelatesToLengthInWCHARs() : 0;

	if (iSoapRelatesToNdx >= 0 && pPacket->pImage->sect.pSoapExtHeaderSection) {
		pPacket->pImage->sect.pSoapExtHeaderSection->GetRelatesTo(MAP(pMsgProps->aPropVar[iSoapRelatesToNdx].pwszVal,WCHAR*),ccSoapRelatesToLen);
		if (ccSoapRelatesToLen < pPacket->pImage->sect.pSoapExtHeaderSection->GetRelatesToLengthInWCHARs())
			return MQ_ERROR_BUFFER_OVERFLOW;
	}

	//
	//	We've read it, now we 
	//
	if ((hRes == MQ_OK) && (dwAction == MQ_ACTION_RECEIVE)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_IO, L"Message read successfully, accepting and advancing...\n");
#endif

		pHInfo->c.fPosValid = FALSE;
		gQueueMan->ForwardTransactionalResponse (pPacket->pImage, MQMSG_CLASS_ACK_RECEIVE, NULL, NULL);
		gQueueMan->AcceptPacket (pPacket, MQMSG_CLASS_ACK_RECEIVE, pQueue);
		pQueue->DisposeOfPacket (pPacket);
	}

	return hRes;
}

static HRESULT scapi_MQReceiveMessageI
(
ScIoRequest *pReq
) {
	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	ScPacket	 *pPacket    = NULL;
	ScQueue		 *pQueue     = NULL;
	ScHandleInfo *pHInfo     = NULL;

	HRESULT hr = scapi_GetPacket (pReq->hQueue, pReq->hCursor, pReq->dwAction, pPacket, pQueue, pHInfo);

	if (hr != MQ_OK) {
		SVSUTIL_ASSERT ((! pHInfo));
		SVSUTIL_ASSERT ((! pPacket));
		SVSUTIL_ASSERT ((! pQueue));
		gMem->Unlock ();
		return hr;
	}

	SVSUTIL_ASSERT (pQueue);

	HANDLE hCallerProc = pReq->hCallerProc;

	if (pPacket) {
		SVSUTIL_ASSERT (pHInfo);

		__try {
			hr = scapi_RetrievePacketInfo (pPacket, pQueue, pHInfo, pReq->dwAction, pReq->pMsgProps, hCallerProc);
		} __except (1) {
			hr = MQ_ERROR_PROPERTY;
		}
		gMem->Unlock ();

		return hr;
	}

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_IO, L"Waiting (again) for message in the queue %s until %d ms.\n", pQueue->lpszFormatName, pReq->tExpirationTicks);
#endif

	HANDLE hQueueEvent = pQueue->hUpdateEvent;

	gMem->Unlock ();

	if (did_expire (pReq->tExpirationTicks, GetTickCount()))
		return MQ_ERROR_IO_TIMEOUT;

	return gOverlappedSupport->EnterOverlappedReceive (hQueueEvent, pReq);
}

//////////////////////////////////////////////////////////////////////
//
//
//	API code section
//
//
//////////////////////////////////////////////////////////////////////
HRESULT	scapi_MQCreateQueue
(
int				iNull,
SCPROPVAR		*pQueuePropsA,
WCHAR			*lpszFormatName,
DWORD			*dwNameLen
) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQCreateQueue\n");
#endif

	if (! pQueuePropsA)
		return MQ_ERROR_ILLEGAL_MQQUEUEPROPS;

	if (iNull != NULL)
		return MQ_ERROR_ILLEGAL_SECURITY_DESCRIPTOR;

#if defined (winCE)
	HANDLE			hCallerProc = (HANDLE)GetCallerProcess();
	SCPROPVAR		sQueuePropsL;

	sQueuePropsL.cProp    = pQueuePropsA->cProp;
	sQueuePropsL.aPropID  = MAP(pQueuePropsA->aPropID, PROPID *);
	sQueuePropsL.aPropVar = MAP(pQueuePropsA->aPropVar,PROPVARIANT *);
	sQueuePropsL.aStatus  = MAP(pQueuePropsA->aStatus, HRESULT *);

	SCPROPVAR		*pQueueProps = &sQueuePropsL;
#else
	SCPROPVAR		*pQueueProps = pQueuePropsA;
#endif

	WCHAR			*lpszLabel     = NULL;
	WCHAR			*lpszPathName  = NULL;

	unsigned int	uiJournalQuota = (unsigned int)-1;

	ScQueueParms	qp;

	memset (&qp, 0, sizeof(qp));
	qp.uiPrivacyLevel = MQ_PRIV_LEVEL_NONE;
	qp.bIsIncoming = TRUE;

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	qp.uiQuotaK = gMachine->uiDefaultInQuotaK;

	gMem->Unlock ();

	if (pQueueProps->aStatus && pQueueProps->cProp > 0)
		memset (pQueueProps->aStatus, 0, sizeof (HRESULT) * pQueueProps->cProp);

	HRESULT hRes = MQ_OK;
	for (int i = 0 ; (! FAILED (hRes)) && (i < (int)pQueueProps->cProp); ++i) {
		hRes = MQ_OK;
		switch (pQueueProps->aPropID[i]) {
		case PROPID_Q_AUTHENTICATE:
			if (pQueueProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else if (pQueueProps->aPropVar[i].bVal != MQ_AUTHENTICATE_NONE)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			break;

		case PROPID_Q_BASEPRIORITY:
			if (pQueueProps->aPropVar[i].vt != VT_I2)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				qp.iBasePriority = pQueueProps->aPropVar[i].iVal;
			break;

		case PROPID_Q_CREATE_TIME:
		case PROPID_Q_INSTANCE:
		case PROPID_Q_MODIFY_TIME:
			hRes = MQ_ERROR_PROPERTY_NOTALLOWED;
			break;

		case PROPID_Q_JOURNAL:
			if (pQueueProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else if (pQueueProps->aPropVar[i].bVal == MQ_JOURNAL)
				qp.bIsJournalOn = qp.bHasJournal = TRUE;
			else if (pQueueProps->aPropVar[i].bVal != MQ_JOURNAL_NONE)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			break;

		case PROPID_Q_JOURNAL_QUOTA:
			if (pQueueProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				uiJournalQuota = pQueueProps->aPropVar[i].ulVal;
			break;

		case PROPID_Q_LABEL:
			if (pQueueProps->aPropVar[i].vt != VT_LPWSTR)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				lpszLabel = MAP(pQueueProps->aPropVar[i].pwszVal, WCHAR *);
			break;

		case PROPID_Q_PATHNAME:
			if (pQueueProps->aPropVar[i].vt != VT_LPWSTR)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				lpszPathName = MAP(pQueueProps->aPropVar[i].pwszVal, WCHAR *);
			break;

		case PROPID_Q_PRIV_LEVEL:
			if (pQueueProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else if (pQueueProps->aPropVar[i].ulVal != MQ_PRIV_LEVEL_NONE)
				return MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			break;

		case PROPID_Q_QUOTA:
			if (pQueueProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				qp.uiQuotaK = pQueueProps->aPropVar[i].ulVal;
			break;

		case PROPID_Q_TRANSACTION:
			if (pQueueProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else if (pQueueProps->aPropVar[i].bVal == MQ_TRANSACTIONAL_NONE)
				qp.bTransactional = FALSE;
			else if (pQueueProps->aPropVar[i].bVal == MQ_TRANSACTIONAL)
				qp.bTransactional = TRUE;
			else
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;

			break;

		case PROPID_Q_TYPE:
			if (pQueueProps->aPropVar[i].vt != VT_CLSID)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				qp.guidQueueType = *MAP(pQueueProps->aPropVar[i].puuid, GUID *);
			break;

		default:
			hRes = MQ_ERROR_ILLEGAL_PROPID;
		}

		if (pQueueProps->aStatus)
			pQueueProps->aStatus[i] = hRes;
	}

	if (FAILED(hRes))
		return hRes;

	if (! lpszPathName)
		return MQ_ERROR_INSUFFICIENT_PROPERTIES;

	if (lpszLabel && (wcslen(lpszLabel) > MQ_MAX_Q_LABEL_LEN))
		return MQ_ERROR_LABEL_TOO_LONG;

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	ScQueue *pQueue = gQueueMan->MakeIncomingQueue (lpszPathName, lpszLabel, &qp, uiJournalQuota, &hRes);

	if (! pQueue) {
		gMem->Unlock ();
		return hRes;
	}

	int iFormatChars = wcslen (pQueue->lpszFormatName) + 1;

	if ((int)*dwNameLen < iFormatChars) {
		if (lpszFormatName && (*dwNameLen > 0)) {
			memcpy (lpszFormatName, pQueue->lpszFormatName, (*dwNameLen - 1) * sizeof(WCHAR));
			lpszFormatName[*dwNameLen - 1] = L'\0';
		}

		gMem->Unlock ();
		return MQ_INFORMATION_FORMATNAME_BUFFER_TOO_SMALL;
	}

	if (lpszFormatName)
		memcpy (lpszFormatName, pQueue->lpszFormatName, iFormatChars * sizeof(WCHAR));

	*dwNameLen = iFormatChars;

	gMem->Unlock ();
	return hRes;
}

HRESULT	scapi_MQDeleteQueue
(
WCHAR *lpszFormatName
) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQDeleteQueue\n");
#endif

	if (! lpszFormatName)
		return MQ_ERROR_ILLEGAL_FORMATNAME;

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	ScQueue *pQueue = gQueueMan->FindIncomingByFormat (lpszFormatName);

	if (! pQueue) {
		gMem->Unlock ();

		return MQ_ERROR_QUEUE_NOT_FOUND;
	}

	if (pQueue->qp.bIsProtected || pQueue->qp.bIsJournal) {
		gMem->Unlock ();

		return MQ_ERROR_ACCESS_DENIED;
	}

	if (pQueue->pJournal)
		gQueueMan->DeleteQueue (pQueue->pJournal, TRUE);

	gQueueMan->DeleteQueue (pQueue, TRUE);

	gMem->Unlock ();

	return MQ_OK;
}

HRESULT	scapi_MQGetMachineProperties
(
WCHAR			*lpszMachineName,
GUID			*pGUID,
SCPROPVAR		*pMachinePropsA
) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQGetMachineProperties\n");
#endif

	if ((! pMachinePropsA) || (pMachinePropsA->cProp < 1))
		return MQ_ERROR_ILLEGAL_MQQMPROPS;

	if (lpszMachineName && pGUID)
		return MQ_ERROR_INVALID_PARAMETER;

#if defined (winCE)
	HANDLE			hCallerProc = (HANDLE)GetCallerProcess();
	SCPROPVAR		sMachinePropsL;

	sMachinePropsL.cProp    = pMachinePropsA->cProp;
	sMachinePropsL.aPropID  = MAP(pMachinePropsA->aPropID, PROPID *);
	sMachinePropsL.aPropVar = MAP(pMachinePropsA->aPropVar,PROPVARIANT *);
	sMachinePropsL.aStatus  = MAP(pMachinePropsA->aStatus, HRESULT *);

	SCPROPVAR		*pMachineProps = &sMachinePropsL;
#else
	SCPROPVAR		*pMachineProps = pMachinePropsA;
#endif

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	if (lpszMachineName && (wcsicmp (lpszMachineName, gMachine->lpszHostName) != 0)) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	if (pGUID && (*pGUID != gMachine->guid)) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	HRESULT hRes = MQ_OK;

	if (pMachineProps->aStatus && pMachineProps->cProp > 0)
		memset (pMachineProps->aStatus, 0, sizeof (HRESULT) * pMachineProps->cProp);

	for (int i = 0 ; (! FAILED(hRes)) && i < (int)pMachineProps->cProp; ++i) {
		hRes = MQ_OK;
		switch (pMachineProps->aPropID[i]) {
		case PROPID_QM_MACHINE_ID:
			{
				GUID *pTarget = pMachineProps->aPropVar[i].puuid;

				if (pMachineProps->aPropVar[i].vt == VT_NULL) {
					pTarget = (GUID *)scutil_OutOfProcAlloc ((void **)&pMachineProps->aPropVar[i].puuid, sizeof(GUID));
					pMachineProps->aPropVar[i].vt = VT_CLSID;
				}

				if (pMachineProps->aPropVar[i].vt == VT_CLSID)
					*pTarget = gMachine->guid;
				else
					hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			}
			break;

		default:
			hRes = MQ_ERROR_ILLEGAL_PROPID;
		}

		if (pMachineProps->aStatus)
			pMachineProps->aStatus[i] = hRes;
	}

	gMem->Unlock ();

	return hRes;
}

HRESULT	scapi_MQGetQueueProperties
(
WCHAR			*lpszFormatName,
SCPROPVAR		*pQueuePropsA
) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQGetQueueProperties\n");
#endif

	if (! lpszFormatName)
		return MQ_ERROR_ILLEGAL_FORMATNAME;

	if (! pQueuePropsA)
		return MQ_ERROR_ILLEGAL_MQQUEUEPROPS;

#if defined (winCE)
	HANDLE			hCallerProc = (HANDLE)GetCallerProcess();
	SCPROPVAR		sQueuePropsL;

	sQueuePropsL.cProp    = pQueuePropsA->cProp;
	sQueuePropsL.aPropID  = MAP(pQueuePropsA->aPropID, PROPID *);
	sQueuePropsL.aPropVar = MAP(pQueuePropsA->aPropVar,PROPVARIANT *);
	sQueuePropsL.aStatus  = MAP(pQueuePropsA->aStatus, HRESULT *);

	SCPROPVAR		*pQueueProps = &sQueuePropsL;
#else
	SCPROPVAR		*pQueueProps = pQueuePropsA;
#endif

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	ScQueue *pq = gQueueMan->FindIncomingByFormat (lpszFormatName);

	if (! pq) {
		gMem->Unlock ();

		return MQ_ERROR_QUEUE_NOT_FOUND;
	}

	HRESULT hRes = MQ_OK;

	if (pQueueProps->aStatus && pQueueProps->cProp > 0)
		memset (pQueueProps->aStatus, 0, sizeof (HRESULT) * pQueueProps->cProp);

	for (int i = 0 ; (! FAILED(hRes)) && i < (int)pQueueProps->cProp; ++i) {
		hRes = MQ_OK;
		switch (pQueueProps->aPropID[i]) {
		case PROPID_Q_AUTHENTICATE:
			if (pQueueProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else if (pq->qp.bAuthenticate)
				pQueueProps->aPropVar[i].bVal = MQ_AUTHENTICATE;
			else
				pQueueProps->aPropVar[i].bVal = MQ_AUTHENTICATE_NONE;
			break;

		case PROPID_Q_BASEPRIORITY:
			if (pQueueProps->aPropVar[i].vt != VT_I2)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				pQueueProps->aPropVar[i].iVal = pq->qp.iBasePriority;
			break;

		case PROPID_Q_CREATE_TIME:
			if (pQueueProps->aPropVar[i].vt != VT_I4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				pQueueProps->aPropVar[i].lVal = (int)pq->tCreation;
			break;

		case PROPID_Q_INSTANCE:
			hRes = MQ_INFORMATION_PROPERTY_IGNORED;
			break;

		case PROPID_Q_MODIFY_TIME:
			if (pQueueProps->aPropVar[i].vt != VT_I4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				pQueueProps->aPropVar[i].lVal = (int)pq->tModification;
			break;

		case PROPID_Q_JOURNAL:
			if (pQueueProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else if (pq->qp.bIsJournalOn)
				pQueueProps->aPropVar[i].bVal = MQ_JOURNAL;
			else
				pQueueProps->aPropVar[i].bVal = MQ_JOURNAL_NONE;

			break;

		case PROPID_Q_JOURNAL_QUOTA:
			if (pQueueProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pQueueProps->aPropVar[i].ulVal = pq->pJournal ? pq->pJournal->qp.uiQuotaK : 0;
			break;

		case PROPID_Q_PRIV_LEVEL:
			if (pQueueProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				pQueueProps->aPropVar[i].ulVal = pq->qp.uiPrivacyLevel;
			break;

		case PROPID_Q_QUOTA:
			if (pQueueProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				pQueueProps->aPropVar[i].ulVal = pq->qp.uiQuotaK;
			break;

		case PROPID_Q_TRANSACTION:
			if (pQueueProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				pQueueProps->aPropVar[i].bVal = pq->qp.bTransactional ? MQ_TRANSACTIONAL :
												MQ_TRANSACTIONAL_NONE;
			break;

		case PROPID_Q_TYPE:
			if (pQueueProps->aPropVar[i].vt != VT_CLSID)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else if (! pQueueProps->aPropVar[i].puuid)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				*MAP(pQueueProps->aPropVar[i].puuid, GUID *) = pq->qp.guidQueueType;
			break;

		case PROPID_Q_LABEL:
			if (pQueueProps->aPropVar[i].vt != VT_NULL)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else {
				pQueueProps->aPropVar[i].vt = VT_LPWSTR;
				pQueueProps->aPropVar[i].pwszVal = scutil_OutOfProcDup (pq->lpszQueueLabel ? pq->lpszQueueLabel : L"");
			}
			break;

		case PROPID_Q_PATHNAME:
			if (pQueueProps->aPropVar[i].vt != VT_NULL)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else {
				pQueueProps->aPropVar[i].vt = VT_LPWSTR;
				WCHAR *lpsz = scutil_MakePathName (pq->lpszQueueHost, pq->lpszQueueName, &pq->qp);

				pQueueProps->aPropVar[i].pwszVal = scutil_OutOfProcDup (lpsz);
				g_funcFree (lpsz, g_pvFreeData);
			}
			break;

		default:
			hRes = MQ_ERROR_ILLEGAL_PROPID;
		}

		if (pQueueProps->aStatus)
			pQueueProps->aStatus[i] = hRes;
	}

	gMem->Unlock ();

	return hRes;
}

HRESULT scapi_MQSetQueueProperties (WCHAR *lpszFormatName, SCPROPVAR *pQueuePropsA) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQSetQueueProperties\n");
#endif

	if (! lpszFormatName)
		return MQ_ERROR_ILLEGAL_FORMATNAME;

	if (! pQueuePropsA)
		return MQ_ERROR_ILLEGAL_MQQUEUEPROPS;

#if defined (winCE)
	HANDLE			hCallerProc = (HANDLE)GetCallerProcess();
	SCPROPVAR		sQueuePropsL;

	sQueuePropsL.cProp    = pQueuePropsA->cProp;
	sQueuePropsL.aPropID  = MAP(pQueuePropsA->aPropID, PROPID *);
	sQueuePropsL.aPropVar = MAP(pQueuePropsA->aPropVar,PROPVARIANT *);
	sQueuePropsL.aStatus  = MAP(pQueuePropsA->aStatus, HRESULT *);

	SCPROPVAR	*pQueueProps = &sQueuePropsL;
#else
	SCPROPVAR	*pQueueProps = pQueuePropsA;
#endif

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	ScQueue *pq = gQueueMan->FindIncomingByFormat (lpszFormatName);

	if (! pq) {
		gMem->Unlock ();

		return MQ_ERROR_QUEUE_NOT_FOUND;
	}

	HRESULT hRes = MQ_OK;

	if (pQueueProps->aStatus && pQueueProps->cProp > 0)
		memset (pQueueProps->aStatus, 0, sizeof (HRESULT) * pQueueProps->cProp);

	for (int i = 0 ; (! FAILED(hRes)) && i < (int)pQueueProps->cProp; ++i) {
		hRes = MQ_OK;
		switch (pQueueProps->aPropID[i]) {
		case PROPID_Q_AUTHENTICATE:
			if (pQueueProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else if (pQueueProps->aPropVar[i].bVal == MQ_AUTHENTICATE_NONE)
				pq->qp.bAuthenticate = FALSE;
			else
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			break;

		case PROPID_Q_BASEPRIORITY:
			if (pQueueProps->aPropVar[i].vt != VT_I2)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pq->qp.iBasePriority = pQueueProps->aPropVar[i].iVal;
			break;

		case PROPID_Q_JOURNAL:
			if (! pq->pJournal)
				hRes = MQ_ERROR_PROPERTY;
			else if (pQueueProps->aPropVar[i].bVal == MQ_JOURNAL)
				pq->qp.bIsJournalOn = TRUE;
			else if (pQueueProps->aPropVar[i].bVal == MQ_JOURNAL_NONE)
				pq->qp.bIsJournalOn = FALSE;
			else
				hRes = MQ_ERROR_PROPERTY;
			break;

		case PROPID_Q_JOURNAL_QUOTA:
			if (! pq->pJournal)
				hRes = MQ_ERROR_PROPERTY;
			else
				pq->pJournal->qp.uiQuotaK = pQueueProps->aPropVar[i].ulVal;
			break;

		case PROPID_Q_CREATE_TIME:
		case PROPID_Q_INSTANCE:
		case PROPID_Q_MODIFY_TIME:
		case PROPID_Q_PATHNAME:
		case PROPID_Q_TRANSACTION:
			hRes = MQ_ERROR_WRITE_NOT_ALLOWED;
			break;

		case PROPID_Q_LABEL:
			if (pQueueProps->aPropVar[i].vt != VT_LPWSTR)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else {
				if (pq->lpszQueueLabel)
					g_funcFree (pq->lpszQueueLabel, g_pvFreeData);

				WCHAR *lpszNewLabel = MAP(pQueueProps->aPropVar[i].pwszVal, WCHAR *);
				if (lpszNewLabel && (lpszNewLabel[0] != L'\0'))
					pq->lpszQueueLabel = svsutil_wcsdup (lpszNewLabel);
				else
					pq->lpszQueueLabel = NULL;
			}
			break;

		case PROPID_Q_PRIV_LEVEL:
			if (pQueueProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				pq->qp.uiPrivacyLevel = pQueueProps->aPropVar[i].ulVal;
			break;

		case PROPID_Q_QUOTA:
			if (pQueueProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				pq->qp.uiQuotaK = pQueueProps->aPropVar[i].ulVal;
			break;

		case PROPID_Q_TYPE:
			if ((pQueueProps->aPropVar[i].vt != VT_CLSID) || (! pQueueProps->aPropVar[i].puuid))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
			else
				pq->qp.guidQueueType = *pQueueProps->aPropVar[i].puuid;
			break;

		default:
			hRes = MQ_ERROR_ILLEGAL_PROPID;
		}
		if (pQueueProps->aStatus)
			pQueueProps->aStatus[i] = hRes;
	}

	if (! FAILED(hRes))
		pq->UpdateFile ();

	gMem->Unlock ();
	return hRes;
}

HRESULT	scapi_MQOpenQueue (WCHAR *lpszFormatName, DWORD dwAccess, DWORD dwShareMode, SCHANDLE *phQueue) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQOpenQueue\n");
#endif

	if (! phQueue)
		return MQ_ERROR;

	if (! lpszFormatName)
		return MQ_ERROR_ILLEGAL_FORMATNAME;

	if ((dwAccess != MQ_PEEK_ACCESS) && (dwAccess != MQ_SEND_ACCESS) && (dwAccess != MQ_RECEIVE_ACCESS))
		return MQ_ERROR_UNSUPPORTED_ACCESS_MODE;

	if ((dwAccess == MQ_SEND_ACCESS) && (dwShareMode != MQ_DENY_NONE))
		return MQ_ERROR_INVALID_PARAMETER;

	if (dwShareMode != MQ_DENY_NONE && dwShareMode != MQ_DENY_RECEIVE_SHARE)
		return MQ_ERROR_INVALID_PARAMETER;

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;	

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	WCHAR *p = wcschr (lpszFormatName, L';');
	if (p && (wcsicmp (p, L";XACTONLY") != 0))
		p = NULL;

	if (p)
		*p = L'\0';

	int uiQueueType;
	ScQueue *pq = gQueueMan->FindIncomingByFormat(lpszFormatName,&uiQueueType);

	if ((! pq) && (dwAccess == MQ_SEND_ACCESS)) {
		pq = gQueueMan->FindOutgoingByFormat (lpszFormatName);

		if (! pq) {
			ScQueueParms qp;
			memset (&qp, 0, sizeof(qp));
			qp.uiQuotaK = gMachine->uiDefaultOutQuotaK;
			qp.bTransactional = p != NULL;
			pq = gQueueMan->MakeOutgoingQueue (lpszFormatName, &qp, NULL);
		}
	}

	if (p)
		*p = L';';

	if (! pq) {
		gMem->Unlock ();

		return MQ_ERROR_QUEUE_NOT_FOUND;
	}

	if (((pq->fDenyAll) && (dwAccess == MQ_RECEIVE_ACCESS)) ||
		((pq->uiOpenRecv) && (dwShareMode == MQ_DENY_RECEIVE_SHARE)) || pq->qp.bIsInternal) {
		gMem->Unlock ();

		return MQ_ERROR_ACCESS_DENIED;
	}

	if (dwAccess == MQ_SEND_ACCESS) {
		if (pq->qp.bIsDeadLetter || pq->qp.bIsJournal || pq->qp.bIsMachineJournal || pq->qp.bIsOrderAck) {
			gMem->Unlock ();

			return MQ_ERROR_UNSUPPORTED_FORMATNAME_OPERATION;
		}

		if (! pq->qp.bIsIncoming) {
			if ( (pq->IsHttpOrHttps () && (! gMachine->fUseSRMP)) ||
				((! pq->IsHttpOrHttps ()) && (! gMachine->fUseBinary)) ) {
				gMem->Unlock ();
				return MQ_ERROR_QUEUE_NOT_AVAILABLE;
			}
		}
	}

	ScHandleInfo sHInfo;

	sHInfo.uiHandleType       = SCQMAN_HANDLE_QUEUE;
	sHInfo.pQueue             = pq;
	sHInfo.pProcId            = scapi_GetOwnerProcId ();
	sHInfo.q.uiShareMode      = dwShareMode;
	sHInfo.q.uiAccess         = dwAccess;
	sHInfo.q.uiQueueType      = uiQueueType;

	SCHANDLE hQueue = (SCHANDLE)gQueueMan->AllocHandle (&sHInfo);

	HRESULT hr = MQ_ERROR_INSUFFICIENT_RESOURCES;
	if ((SVSHandle)hQueue != SVSUTIL_HANDLE_INVALID) {
		hr = MQ_OK;

		ScHandleInfo *pHInfo = gQueueMan->QueryHandle ((SVSHandle)hQueue);
		SVSUTIL_ASSERT (pHInfo && (pHInfo->pQueue == pq) && (pHInfo->uiHandleType == SCQMAN_HANDLE_QUEUE));

		if (dwShareMode == MQ_DENY_RECEIVE_SHARE)
			pq->fDenyAll = TRUE;

		if (dwAccess != MQ_SEND_ACCESS)
			++pq->uiOpenRecv;

		pq->uiOpen++;

		if (pq->qp.bIsIncoming) {
			sHInfo.uiHandleType  = SCQMAN_HANDLE_CURSOR;
			sHInfo.pQueue        = pq;
			sHInfo.pProcId       = scapi_GetOwnerProcId ();
			sHInfo.c.hQueue      = (SVSHandle)hQueue;
			sHInfo.c.pNode		 = NULL;
			sHInfo.c.fPosValid   = FALSE;

			pHInfo->q.hDefaultCursor = gQueueMan->AllocHandle (&sHInfo);

			if (pHInfo->q.hDefaultCursor == SVSUTIL_HANDLE_INVALID) {
				gQueueMan->CloseHandle ((SVSHandle)hQueue);
				hr = MQ_ERROR_INSUFFICIENT_RESOURCES;
			}
		} else
			pHInfo->q.hDefaultCursor = SVSUTIL_HANDLE_INVALID;

		if (hr == MQ_OK) {
#if defined (winCE)
			*(SCHANDLE *)MapPtrToProcess (phQueue, (HANDLE)GetCallerProcess()) = hQueue;
#else
			*phQueue = hQueue;
#endif
		}
	}

	gMem->Unlock ();

	return hr;
}

HRESULT scapi_MQCloseQueue (SCHANDLE hQueue) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQCloseQueue\n");
#endif

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	HRESULT hr = MQ_OK;

	ScHandleInfo *pHInfo = gQueueMan->QueryHandle ((SVSHandle)hQueue);

	if (pHInfo && (pHInfo->uiHandleType == SCQMAN_HANDLE_QUEUE)) {
		gQueueMan->CloseAllHandles ((SVSHandle)hQueue);
		gQueueMan->CloseHandle ((SVSHandle)hQueue);
	} else
		hr = MQ_ERROR_INVALID_HANDLE;

	gMem->Unlock ();
	return hr;
}

HRESULT scapi_MQCreateCursor (SCHANDLE hQueue, SCHANDLE *phCursor) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQCreateCursor\n");
#endif

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	HRESULT hr = MQ_OK;

	ScHandleInfo *pHInfo = gQueueMan->QueryHandle ((SVSHandle)hQueue);

	if (pHInfo && (pHInfo->uiHandleType == SCQMAN_HANDLE_QUEUE) &&
									pHInfo->pQueue->qp.bIsIncoming) {
		ScHandleInfo sHInfo;
		sHInfo.uiHandleType = SCQMAN_HANDLE_CURSOR;
		sHInfo.pQueue       = pHInfo->pQueue;
		sHInfo.pProcId      = scapi_GetOwnerProcId ();
		sHInfo.c.hQueue     = (SVSHandle)hQueue;
		sHInfo.c.pNode		= NULL;
		sHInfo.c.fPosValid  = FALSE;

		SVSHandle hCursor = gQueueMan->AllocHandle (&sHInfo);

		if (hCursor == SVSUTIL_HANDLE_INVALID)
			hr = MQ_ERROR_INSUFFICIENT_RESOURCES;
		else
#if defined (winCE)
			*(SVSHandle *)MapPtrToProcess (phCursor, (HANDLE)GetCallerProcess()) = hCursor;
#else
			*phCursor = (SCHANDLE)hCursor;
#endif

	} else
		hr = MQ_ERROR_INVALID_HANDLE;


	gMem->Unlock ();

	return hr;
}

HRESULT scapi_MQCloseCursor (SCHANDLE hCursor) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQCloseCursor\n");
#endif

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	HRESULT hr = MQ_OK;

	ScHandleInfo *pHInfo = gQueueMan->QueryHandle ((SVSHandle)hCursor);

	if (pHInfo && (pHInfo->uiHandleType == SCQMAN_HANDLE_CURSOR))
		gQueueMan->CloseHandle ((SVSHandle)hCursor);
	else
		hr = MQ_ERROR_INVALID_HANDLE;

	gMem->Unlock ();

	return hr;
}

HRESULT scapi_MQHandleToFormatName (SCHANDLE hQueue, WCHAR *lpszFormatName, DWORD *dwNameLen) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQHandleToFormatName\n");
#endif

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	HRESULT hr = MQ_OK;

	ScHandleInfo *pHInfo = gQueueMan->QueryHandle ((SVSHandle)hQueue);

	if (pHInfo && (pHInfo->uiHandleType == SCQMAN_HANDLE_QUEUE)) {
		ScQueue *pQueue = pHInfo->pQueue;

		int ccFormatChars = wcslen (pQueue->lpszFormatName) + 1;
		int ccAddTransact  = (! pQueue->qp.bIsIncoming) && pQueue->qp.bTransactional ? SVSUTIL_CONSTSTRLEN(L";XACTONLY") : 0;

		if ((int)*dwNameLen < ccFormatChars) {
			memcpy (lpszFormatName, pQueue->lpszFormatName, (*dwNameLen - 1) * sizeof(WCHAR));
			lpszFormatName[*dwNameLen - 1] = L'\0';

			hr = MQ_INFORMATION_FORMATNAME_BUFFER_TOO_SMALL;
		} else if ((int)*dwNameLen < ccFormatChars + ccAddTransact) {
			memcpy (lpszFormatName, pQueue->lpszFormatName, ccFormatChars * sizeof(WCHAR));
			memcpy (&lpszFormatName[ccFormatChars], L";XACTONLY", (*dwNameLen - ccFormatChars - 1) * sizeof(WCHAR));
			lpszFormatName[*dwNameLen - 1] = L'\0';

			*dwNameLen = ccFormatChars + ccAddTransact;
			hr = MQ_INFORMATION_FORMATNAME_BUFFER_TOO_SMALL;
		} else {
			memcpy (lpszFormatName, pQueue->lpszFormatName, ccFormatChars * sizeof(WCHAR));

			if (ccAddTransact)
				memcpy (&lpszFormatName[ccFormatChars - 1], L";XACTONLY", (ccAddTransact + 1) * sizeof(WCHAR));

			*dwNameLen = ccFormatChars + ccAddTransact;
		}
	} else
		hr = MQ_ERROR_INVALID_HANDLE;

	gMem->Unlock ();

	return hr;
}

HRESULT scapi_MQPathNameToFormatName (WCHAR *lpszPathName, WCHAR *lpszFormatName, DWORD *pdwCount) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQPathNameToFormatName\n");
#endif

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	ScQueueParms qp;
	memset (&qp, 0, sizeof (qp));

	qp.bIsIncoming = TRUE;
	WCHAR *szFormatName = scutil_MakeFormatName (lpszPathName, &qp);

	if (! szFormatName)
		return MQ_ERROR_ILLEGAL_QUEUE_PATHNAME;

	unsigned int uiFormatChars = wcslen (szFormatName) + 1;

	if (*pdwCount < uiFormatChars) {
		if (lpszFormatName && *pdwCount > 0) {
			memcpy (lpszFormatName, szFormatName, (*pdwCount - 1) * sizeof(WCHAR));
			lpszFormatName[*pdwCount - 1] = L'\0';
		}

		g_funcFree (szFormatName, g_pvFreeData);
		return MQ_ERROR_FORMATNAME_BUFFER_TOO_SMALL;
	}

	if (lpszFormatName)
		memcpy (lpszFormatName, szFormatName, uiFormatChars * sizeof(WCHAR));

	*pdwCount = uiFormatChars;

	g_funcFree (szFormatName, g_pvFreeData);

	return MQ_OK;
}

HRESULT scapi_MQSendMessage (SCHANDLE hQueue, SCPROPVAR *pMsgPropsA, int iTransaction) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQSendMessage\n");
#endif

	if (! pMsgPropsA)
		return MQ_ERROR_INSUFFICIENT_PROPERTIES;

#if defined (winCE)
	HANDLE		hCallerProc = (HANDLE)GetCallerProcess();
	SCPROPVAR	sMsgPropsL;

	sMsgPropsL.cProp    = pMsgPropsA->cProp;
	sMsgPropsL.aPropID  = MAP(pMsgPropsA->aPropID, PROPID *);
	sMsgPropsL.aPropVar = MAP(pMsgPropsA->aPropVar,PROPVARIANT *);
	sMsgPropsL.aStatus  = MAP(pMsgPropsA->aStatus, HRESULT *);

	SCPROPVAR	*pMsgProps = &sMsgPropsL;
#else
	SCPROPVAR	*pMsgProps = pMsgPropsA;
#endif

	//
	//	Construct a packet
	//
	HRESULT hRes = MQ_OK;

	if (pMsgProps->aStatus && pMsgProps->cProp > 0)
		memset (pMsgProps->aStatus, 0, sizeof (HRESULT) * pMsgProps->cProp);

	unsigned int uiAckType     = MQMSG_ACKNOWLEDGMENT_NONE;
	unsigned int uiAppSpecific = 0;
	unsigned int uiBodyType    = 0;
	unsigned int uiMsgClass    = MQMSG_CLASS_NORMAL;
	unsigned int uiDelivery    = MQMSG_DELIVERY_EXPRESS;
	unsigned int uiJournalType = MQMSG_JOURNAL_NONE;
	unsigned int uiPriority    = 3;
	unsigned int uiTrace	   = MQMSG_TRACE_NONE;

	unsigned int tTTR		   = INFINITE;
	unsigned int tTRQ		   = INFINITE;

	CAUB  *pcaubBody   = NULL;
	CAUB  *pcaubCorrId = NULL;
	CAUB  *pcaubExt    = NULL;

	CAPROPVARIANT *pFwdVia   = NULL;
	CAPROPVARIANT *pRevVia   = NULL;
	int iSoapFwdVia          = 0;
	int iSoapRevVia          = 0;

	WCHAR *lpszAdminQueue	 = NULL;
	WCHAR *lpszLabel		 = NULL;
	WCHAR *lpszResponseQueue = NULL;
	WCHAR *lpszDestQueue     = NULL;
	WCHAR *lpszSoapBody      = NULL;
	WCHAR *lpszSoapHeader    = NULL;
	WCHAR *lpszSoapEnvelope  = NULL;
	WCHAR *lpszSoapRelatesTo = NULL;
	WCHAR *lpszSoapFrom      = NULL;

	OBJECTID	*pMsgID	     = NULL;

	for (int i = 0 ; (! FAILED (hRes)) && (i < (int)pMsgProps->cProp) ; ++i) {
		hRes = MQ_OK;
		switch (pMsgProps->aPropID[i]) {
		case PROPID_M_ACKNOWLEDGE:
			if (pMsgProps->aPropVar[i].vt != VT_UI1)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				uiAckType = pMsgProps->aPropVar[i].bVal;
			break;

		case PROPID_M_ADMIN_QUEUE:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				lpszAdminQueue = MAP(pMsgProps->aPropVar[i].pwszVal, WCHAR *);
			break;

		case PROPID_M_APPSPECIFIC:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				uiAppSpecific = pMsgProps->aPropVar[i].ulVal;
			break;

		case PROPID_M_AUTH_LEVEL:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else if (pMsgProps->aPropVar[i].ulVal != MQMSG_AUTH_LEVEL_NONE)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			break;

		case PROPID_M_BODY:
			if ((pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_UI1)))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else if (pMsgProps->aPropVar[i].caub.cElems && pMsgProps->aPropVar[i].caub.pElems)
				pcaubBody = &pMsgProps->aPropVar[i].caub;
			break;

		case PROPID_M_BODY_TYPE:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				uiBodyType = pMsgProps->aPropVar[i].ulVal;
			break;

		case PROPID_M_CLASS:
			if (pMsgProps->aPropVar[i].vt != VT_UI2)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				uiMsgClass = pMsgProps->aPropVar[i].uiVal;
			break;

		case PROPID_M_CORRELATIONID:
			if ((pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_UI1)) ||
				(pMsgProps->aPropVar[i].caub.cElems <= 0) ||
				(pMsgProps->aPropVar[i].caub.cElems > 20) ||
				(! pMsgProps->aPropVar[i].caub.pElems))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pcaubCorrId = &pMsgProps->aPropVar[i].caub;
			break;

		case PROPID_M_DELIVERY:
			if ((pMsgProps->aPropVar[i].vt != VT_UI1) ||
				((pMsgProps->aPropVar[i].bVal != MQMSG_DELIVERY_EXPRESS) &&
				(pMsgProps->aPropVar[i].bVal != MQMSG_DELIVERY_RECOVERABLE)))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				uiDelivery = pMsgProps->aPropVar[i].bVal;
			break;

		case PROPID_M_DEST_QUEUE:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				lpszDestQueue = MAP(pMsgProps->aPropVar[i].pwszVal, WCHAR *);
			break;

		case PROPID_M_EXTENSION:
			if ((pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_UI1)) ||
				(pMsgProps->aPropVar[i].caub.cElems <= 0) ||
				(! pMsgProps->aPropVar[i].caub.pElems))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pcaubExt = &pMsgProps->aPropVar[i].caub;
			break;

		case PROPID_M_JOURNAL:
			if ((pMsgProps->aPropVar[i].vt != VT_UI1) ||
				(pMsgProps->aPropVar[i].bVal & (~(MQMSG_DEADLETTER | MQMSG_JOURNAL | MQMSG_JOURNAL_NONE))))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				uiJournalType = pMsgProps->aPropVar[i].bVal;
			break;

		case PROPID_M_LABEL:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				lpszLabel = MAP(pMsgProps->aPropVar[i].pwszVal, WCHAR *);
			break;

		case PROPID_M_PRIORITY:
			if ((pMsgProps->aPropVar[i].vt != VT_UI1) || (pMsgProps->aPropVar[i].bVal > 7))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				uiPriority = pMsgProps->aPropVar[i].bVal;
			break;

		case PROPID_M_RESP_QUEUE:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				lpszResponseQueue = MAP(pMsgProps->aPropVar[i].pwszVal, WCHAR *);
			break;

		case PROPID_M_TIME_TO_BE_RECEIVED:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				tTTR = pMsgProps->aPropVar[i].ulVal;
			break;

		case PROPID_M_TIME_TO_REACH_QUEUE:
			if (pMsgProps->aPropVar[i].vt != VT_UI4)
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				tTRQ = pMsgProps->aPropVar[i].ulVal;
			break;

		case PROPID_M_TRACE:
			if ((pMsgProps->aPropVar[i].vt != VT_UI1) ||
				((pMsgProps->aPropVar[i].bVal != MQMSG_SEND_ROUTE_TO_REPORT_QUEUE) &&
				(pMsgProps->aPropVar[i].bVal != MQMSG_TRACE_NONE)))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				uiTrace = pMsgProps->aPropVar[i].bVal;
			break;

		case PROPID_M_MSGID:
			if ((pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_UI1)) ||
				(pMsgProps->aPropVar[i].caub.cElems <= 0) ||
				(pMsgProps->aPropVar[i].caub.cElems > 20) ||
				(! pMsgProps->aPropVar[i].caub.pElems))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				pMsgID = MAP(pMsgProps->aPropVar[i].caub.pElems, OBJECTID *);
			break;

		case PROPID_M_BODY_SIZE:
		case PROPID_M_EXTENSION_LEN:
		case PROPID_M_SENDERID_LEN:
		case PROPID_M_SENDERID_TYPE:
		case PROPID_M_SENDERID:
			hRes = MQ_INFORMATION_PROPERTY_IGNORED;
			break;


		case PROPID_M_SOAP_HEADER:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				lpszSoapHeader = MAP(pMsgProps->aPropVar[i].pwszVal, WCHAR *);
			break;

		case PROPID_M_SOAP_BODY:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				lpszSoapBody = MAP(pMsgProps->aPropVar[i].pwszVal, WCHAR *);
			break;

		case PROPID_M_SOAP_FWD_VIA:
			if ((pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_VARIANT)))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;

			if (hRes != MQ_OK)
				break;

			pFwdVia = &pMsgProps->aPropVar[i].capropvar;
			pFwdVia = MAP(pFwdVia,CAPROPVARIANT *);

			if (0 == (iSoapFwdVia = CalcFwdRevViaSectionSize(pFwdVia,hCallerProc)))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;

			break;

		case PROPID_M_SOAP_REV_VIA:
			if ((pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_VARIANT)))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;

			if (hRes != MQ_OK)
				break;

			pRevVia = &pMsgProps->aPropVar[i].capropvar;
			pRevVia = MAP(pRevVia,CAPROPVARIANT *);

			if (0 == (iSoapRevVia = CalcFwdRevViaSectionSize(pRevVia,hCallerProc)))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;

			break;

		case PROPID_M_SOAP_FROM:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				lpszSoapFrom = MAP(pMsgProps->aPropVar[i].pwszVal, WCHAR *);
			break;

		case PROPID_M_SOAP_RELATES_TO:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR))
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
			else
				lpszSoapRelatesTo = MAP(pMsgProps->aPropVar[i].pwszVal, WCHAR *);
			break;

		case PROPID_M_XACT_STATUS_QUEUE:
		case PROPID_M_XACT_STATUS_QUEUE_LEN:
		case PROPID_M_VERSION:
		case PROPID_M_SRC_MACHINE_ID:
		case PROPID_M_SIGNATURE:
		case PROPID_M_SIGNATURE_LEN:
		case PROPID_M_SENTTIME:
		case PROPID_M_SENDER_CERT_LEN:
		case PROPID_M_SENDER_CERT:
		case PROPID_M_SECURITY_CONTEXT:
		case PROPID_M_RESP_QUEUE_LEN:
		case PROPID_M_PROV_TYPE:
		case PROPID_M_PROV_NAME_LEN:
		case PROPID_M_PROV_NAME:
		case PROPID_M_PRIV_LEVEL:
		case PROPID_M_LABEL_LEN:
		case PROPID_M_HASH_ALG:
		case PROPID_M_ENCRYPTION_ALG:
		case PROPID_M_DEST_SYMM_KEY:
		case PROPID_M_DEST_SYMM_KEY_LEN:
		case PROPID_M_DEST_QUEUE_LEN:
		case PROPID_M_CONNECTOR_TYPE:
		case PROPID_M_AUTHENTICATED:
		case PROPID_M_ADMIN_QUEUE_LEN:
		case PROPID_M_ARRIVEDTIME:
		default:
			hRes = MQ_ERROR_PROPERTY;

		}

		if (pMsgProps->aStatus)
			pMsgProps->aStatus[i] = hRes;
	}

	if (FAILED(hRes))
		return hRes;

	if (lpszLabel && (wcslen (lpszLabel) > MQ_MAX_MSG_LABEL_LEN))
		return MQ_ERROR_LABEL_TOO_LONG;

	if (tTTR < tTRQ)
		tTRQ = tTTR;

	if ((tTRQ != INFINITE) && (tTTR != INFINITE))
		tTTR -= tTRQ;

	//
	//	Parameters parsed. Check the validity...
	//
	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	if ((! gMachine->fUseSRMP) &&
		(lpszSoapHeader || lpszSoapBody || pFwdVia || pRevVia || lpszSoapFrom || lpszSoapRelatesTo)) {
		gMem->Unlock ();
		return MQ_INFORMATION_UNSUPPORTED_PROPERTY;
	}

	ScHandleInfo *pHInfo = gQueueMan->QueryHandle ((SVSHandle)hQueue);

	if ((! pHInfo) || (pHInfo->uiHandleType != SCQMAN_HANDLE_QUEUE)) {
		gMem->Unlock ();

		return MQ_ERROR_INVALID_HANDLE;
	}

	if (pHInfo->q.uiAccess != MQ_SEND_ACCESS) {
		gMem->Unlock ();

		return MQ_ERROR_INVALID_HANDLE;
	}

	ScQueue *pQueue = pHInfo->pQueue;
	int fHttp       = pQueue->IsHttpOrHttps();
	int fUseSoap    = (lpszSoapHeader || lpszSoapBody);

	if (pQueue->qp.bTransactional) {
		uiDelivery = MQMSG_DELIVERY_RECOVERABLE;

		if (iTransaction != (int)MQ_SINGLE_MESSAGE)
			hRes = MQ_ERROR_TRANSACTION_USAGE;
	} else if (iTransaction)
		hRes = MQ_ERROR_TRANSACTION_USAGE;
	else if (!fHttp && fUseSoap)
		hRes = MQ_ERROR_ILLEGAL_PROPID;  // A soap header has been sent but we're not sending to an HTTP queue

	if (FAILED(hRes)) {
		gMem->Unlock ();

		return hRes;
	}

	unsigned int uiMsgId = gQueueMan->GetNextID();

	//
	//	Compute the size and alloc the buffer...
	//
	//	No - Xact header
	//	No - Security header
	//	No - Session section (added by session layer)
	//
	QUEUE_FORMAT qfDest;
	QUEUE_FORMAT qfAdmin;
	QUEUE_FORMAT qfResp;
	QUEUE_FORMAT qfDebug;

	if ((pQueue != gQueueMan->pQueueOutFRS && !fHttp) || (! lpszDestQueue))
		lpszDestQueue = pQueue->lpszFormatName;

	if ((! scutil_StringToQF (&qfDest, lpszDestQueue)) ||
		(lpszAdminQueue && (! scutil_StringToQF (&qfAdmin, lpszAdminQueue))) ||
		(lpszResponseQueue && (! scutil_StringToQF (&qfResp,  lpszResponseQueue))) ||
		(gMachine->lpszDebugQueueFormatName && (! scutil_StringToQF (&qfDebug, gMachine->lpszDebugQueueFormatName)))) {

		gMem->Unlock ();

		return MQ_ERROR;
	}

	int iHeaderSize = CBaseHeader::CalcSectionSize ();
	int iUserSize   = CUserHeader::CalcSectionSize (NULL, NULL, NULL,
								&qfDest, lpszAdminQueue ? &qfAdmin : NULL,
								lpszResponseQueue ? &qfResp : NULL);
	int iPropSize   = CPropertyHeader::CalcSectionSize (lpszLabel ? wcslen(lpszLabel) + 1 : 0,
								pcaubExt ? pcaubExt->cElems : 0,
								pcaubBody ? pcaubBody->cElems : 0);
	int iDebugSize  = gMachine->lpszDebugQueueFormatName &&
						(uiTrace == MQMSG_SEND_ROUTE_TO_REPORT_QUEUE) ?
									CDebugSection::CalcSectionSize (&qfDebug) : 0;
	int iXactSize   = (! pQueue->qp.bIsIncoming) && pQueue->qp.bTransactional ? CXactHeader::CalcSectionSize((void *)0xffffffff, NULL): 0;

	int iSoapHeader   = fUseSoap   ? CSoapSection::CalcSectionSize(lpszSoapHeader ? wcslen(lpszSoapHeader) : 0) : 0;
	int iSoapBody     = fUseSoap   ? CSoapSection::CalcSectionSize(lpszSoapBody   ? wcslen(lpszSoapBody)   : 0) : 0;

	int ccFrom        = lpszSoapFrom      ? wcslen(lpszSoapFrom) : 0;
	int ccRelatesTo   = lpszSoapRelatesTo ? wcslen(lpszSoapRelatesTo) : 0;
	int iSoapExt      = (lpszSoapRelatesTo || lpszSoapFrom) ? CSoapExtSection::CalcSectionSize(ccFrom, ccRelatesTo) : 0;

	unsigned int uiBaseSize = iHeaderSize + iUserSize + iPropSize + iDebugSize + iXactSize + iSoapHeader + iSoapBody;
	unsigned int uiPacketSize = uiBaseSize + iSoapFwdVia + iSoapRevVia + iSoapExt; // include base + ext size

	ScPacketImage *pPacketImage = (ScPacketImage *)g_funcAlloc (sizeof(ScPacketImage) + uiPacketSize, g_pvAllocData);

	if (! pPacketImage) {
		gMem->Unlock ();

		return MQ_ERROR_INSUFFICIENT_RESOURCES;
	}

	void *pvPacketBuffer = (void *)((unsigned char *)pPacketImage + sizeof(ScPacketImage));
	memset (&pPacketImage->sect, 0, sizeof (pPacketImage->sect));
	memset (&pPacketImage->ucSourceAddr, 0, sizeof(pPacketImage->ucSourceAddr));

	pPacketImage->allflags = 0;
	pPacketImage->pvBinary = NULL;
	pPacketImage->pvExt    = NULL;

	pPacketImage->hkOrderKey = ++pQueue->hkReceived;
	if (! pQueue->qp.bTransactional)
		pPacketImage->hkOrderKey |= uiPriority << SCPACKET_ORD_TIMEBITS;

	pPacketImage->flags.fSecureSession = TRUE;

	CBaseHeader *pBaseHeader = (CBaseHeader *)pvPacketBuffer;
	pBaseHeader->CBaseHeader::CBaseHeader (uiBaseSize);
	pBaseHeader->SetPriority (uiPriority);
	pBaseHeader->IncludeDebug (iDebugSize > 0);
	pBaseHeader->SetTrace (uiTrace);
	pBaseHeader->SetAbsoluteTimeToQueue (tTRQ);

	CUserHeader *pUserHeader = (CUserHeader *)pBaseHeader->GetNextSection ();

	GUID nullGuid;
	memset (&nullGuid, 0, sizeof(nullGuid));

	pUserHeader->CUserHeader::CUserHeader (&gMachine->guid, &nullGuid, &qfDest,
								lpszAdminQueue ? &qfAdmin : NULL, lpszResponseQueue ? &qfResp : NULL, uiMsgId);

	pUserHeader->SetTimeToLiveDelta (tTTR);
	pUserHeader->SetSentTime (scutil_now());
	pUserHeader->SetDelivery (uiDelivery);
	pUserHeader->IncludeProperty (TRUE);
	pUserHeader->SetAuditing (uiJournalType);

	CPropertyHeader *pPropHeader;
	if (iXactSize) {
        pUserHeader->IncludeXact(TRUE);
		CXactHeader *pXactHeader = (CXactHeader *)pUserHeader->GetNextSection ();
		pXactHeader->CXactHeader::CXactHeader (NULL);
		pXactHeader->SetSeqID (pQueue->llSeqID);
		pXactHeader->SetSeqN (++pQueue->uiSeqNum);

		pPropHeader = (CPropertyHeader *)pXactHeader->GetNextSection ();
	} else
		pPropHeader = (CPropertyHeader *)pUserHeader->GetNextSection ();

	pPropHeader->CPropertyHeader::CPropertyHeader ();

	pPropHeader->SetAckType (uiAckType);

	pPropHeader->SetClass (uiMsgClass);

	if (pcaubCorrId)
		pPropHeader->SetCorrelationID (MAP(pcaubCorrId->pElems, unsigned char *));

	pPropHeader->SetApplicationTag (uiAppSpecific);

	if (lpszLabel)
		pPropHeader->SetTitle (lpszLabel, wcslen (lpszLabel) + 1);

	if (pcaubExt)
		pPropHeader->SetMsgExtension (MAP(pcaubExt->pElems, unsigned char *), pcaubExt->cElems);

	if (pcaubBody)
		pPropHeader->SetBody (MAP(pcaubBody->pElems, unsigned char *), pcaubBody->cElems, pcaubBody->cElems);

	pPropHeader->SetBodyType (uiBodyType);
	pPropHeader->SetPrivLevel (MQMSG_PRIV_LEVEL_NONE);
	pPropHeader->SetHashAlg (0);
	pPropHeader->SetEncryptAlg (0);

	PVOID pNextSection = (PVOID) pPropHeader->GetNextSection ();

	if (iDebugSize > 0) {
		CDebugSection *pDebugSec = (CDebugSection *)pNextSection;
		pDebugSec->CDebugSection::CDebugSection (&qfDebug);
		pNextSection = (PVOID) pDebugSec->GetNextSection();
	}

	// Soap elements
	if (fUseSoap) {
		const USHORT x_SOAP_HEADER_SECTION_ID = 800;
        const USHORT x_SOAP_BODY_SECTION_ID = 900;
		pUserHeader->IncludeSoap(TRUE);

		CSoapSection *pSoapHeader = (CSoapSection *) pNextSection;
		pSoapHeader->CSoapSection::CSoapSection(lpszSoapHeader,lpszSoapHeader ? wcslen(lpszSoapHeader) : 0,x_SOAP_HEADER_SECTION_ID);

		CSoapSection *pSoapBody = (CSoapSection*) pSoapHeader->GetNextSection();
		pSoapBody->CSoapSection::CSoapSection(lpszSoapBody,lpszSoapBody ? wcslen(lpszSoapBody) : 0,x_SOAP_BODY_SECTION_ID);
		pNextSection = (PVOID) pSoapBody->GetNextSection();
	}

	if (pFwdVia) {
		pPacketImage->flags.fFwdIncluded = TRUE;
		CDataHeader *pFwd = (CDataHeader*) pNextSection;
		WCHAR *szTrav = (WCHAR*)pFwd->GetData();
		
		for (i = 0; i < (int) pFwdVia->cElems; ++i) {
			PROPVARIANT *pVar = MAP(&pFwdVia->pElems[i], PROPVARIANT *);
			WCHAR *szVia = MAP(pVar->pwszVal, WCHAR *);

			unsigned int uiLen = wcslen(szVia)+1;
			memcpy(szTrav,szVia,uiLen*2);
			szTrav += uiLen;
		}
		pFwd->SetDataLengthInWCHARs(szTrav - (WCHAR*)pFwd->GetData());
		pNextSection = (PVOID) pFwd->GetNextSection();
	}

	if (pRevVia) {
		pPacketImage->flags.fRevIncluded = TRUE;
		CDataHeader *pRev = (CDataHeader*) pNextSection;
		WCHAR *szTrav = (WCHAR*) pRev->GetData();

		for (i = 0; i < (int) pRevVia->cElems; ++i) {
			PROPVARIANT *pVar = MAP(&pRevVia->pElems[i], PROPVARIANT *);
			WCHAR *szVia = MAP(pVar->pwszVal, WCHAR *);

			unsigned int uiLen = wcslen(szVia)+1;
			memcpy(szTrav,szVia,uiLen*2);
			szTrav += uiLen;
		}
		pRev->SetDataLengthInWCHARs(szTrav - (WCHAR*)pRev->GetData());
		pNextSection = (PVOID) pRev->GetNextSection();
	}

	if (lpszSoapRelatesTo || lpszSoapFrom) {
		pPacketImage->flags.fSoapExtIncluded = TRUE;
		CSoapExtSection *pSoapExt = (CSoapExtSection*) pNextSection;
		
		pSoapExt->CSoapExtSection::CSoapExtSection(lpszSoapFrom,ccFrom,lpszSoapRelatesTo,ccRelatesTo);
		pNextSection = (PVOID) pSoapExt->GetNextSection();
	}

	if (! pPacketImage->PopulateSections ()) {
		g_funcFree (pPacketImage, g_pvFreeData);
		gMem->Unlock ();
		return MQ_ERROR;
	}

	if ((! pQueue->qp.bIsIncoming) && (! pQueue->qp.bIsOutFRS)) {
		ScQueue *pQueueDest = gQueueMan->FindQueueByPacketImage (pPacketImage, FALSE);

		if (pQueueDest)
			pQueue = pQueueDest;
	}

	ScPacket *pPacket = pQueue->MakePacket (pPacketImage, -1, TRUE);
	if (! pPacket) {
		g_funcFree (pPacketImage, g_pvFreeData);
		hRes = MQ_ERROR_INSUFFICIENT_RESOURCES;
	} else if (pQueue->qp.bIsIncoming) {
		//
		// Packet is only journalled when send is non-local
		//
		gQueueMan->AcceptPacket (pPacket, MQMSG_CLASS_ACK_REACH_QUEUE, pQueue);
	}

	if ((! FAILED(hRes)) && pMsgID) {
		pMsgID->Lineage = gMachine->guid;
		pMsgID->Uniquifier = uiMsgId;
	}

	gMem->Unlock ();

	return hRes;
}

HRESULT scapi_MQReceiveMessage
(
SCHANDLE			hQueue,
DWORD				dwTimeout,
DWORD				dwAction,
SCPROPVAR			*pMsgPropsA,
OVERLAPPED			*lpOverlappedA,
PMQRECEIVECALLBACK	pfnReceiveCallback,
SCHANDLE			hCursor,
int					iNull3
) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_API, L"Entered MQReceiveMessage\n");
#endif

#if defined (winNT)
	if (lpOverlappedA)
		return MQ_ERROR_ILLEGAL_OPERATION;

	if (pfnReceiveCallback != 0)
		return MQ_ERROR_ILLEGAL_OPERATION;
#endif
	if (iNull3 != 0)
		return MQ_ERROR_TRANSACTION_USAGE;

	if ((dwAction != MQ_ACTION_RECEIVE) && (dwAction != MQ_ACTION_PEEK_CURRENT) && (dwAction != MQ_ACTION_PEEK_NEXT))
		return MQ_ERROR_ILLEGAL_CURSOR_ACTION;

	if ((! hCursor) && (dwAction == MQ_ACTION_PEEK_NEXT))
		return MQ_ERROR_ILLEGAL_CURSOR_ACTION;

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	ScPacket	 *pPacket    = NULL;
	ScQueue		 *pQueue     = NULL;
	ScHandleInfo *pHInfo     = NULL;

	HRESULT hr = scapi_GetPacket (hQueue, hCursor, dwAction, pPacket, pQueue, pHInfo);

	if (hr != MQ_OK) {
		gMem->Unlock ();

		SVSUTIL_ASSERT ((! pHInfo));
		SVSUTIL_ASSERT ((! pPacket));
		SVSUTIL_ASSERT ((! pQueue));
		return hr;
	}

	SVSUTIL_ASSERT (pQueue);

#if defined (winCE)
	HANDLE hCallerProc = GetCallerProcess ();
	DWORD  dwProcPerm  = GetCurrentPermissions ();
	OVERLAPPED *lpOverlapped = MAP(lpOverlappedA, OVERLAPPED *);
#else
	HANDLE		hCallerProc = NULL;
	DWORD		dwProcPerm  = 0;
	OVERLAPPED	*lpOverlapped = lpOverlappedA;
#endif

	if (pPacket) {
		SVSUTIL_ASSERT (pHInfo);

		hr = scapi_RetrievePacketInfo (pPacket, pQueue, pHInfo, dwAction, pMsgPropsA, hCallerProc);

		gMem->Unlock ();

		if (pfnReceiveCallback) {
#if defined (winNT)
					SVSUTIL_ASSERT(0);
#endif
#if defined (winCE)
			CALLBACKINFO cbi;
			cbi.hProc  = hCallerProc;
			cbi.pfn    = (FARPROC)pfnReceiveCallback;
			cbi.pvArg0 = (void *)hr;

			PerformCallBack (&cbi, hQueue, dwTimeout, dwAction, pMsgPropsA,
									lpOverlappedA, hCursor);
#endif
		} else if (lpOverlapped)
			SetEvent (lpOverlapped->hEvent);

		return hr;
	}

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_IO, L"Waiting for message in the queue %s for %d ms.\n", pQueue->lpszFormatName, dwTimeout);
#endif

	HANDLE hQueueEvent = pQueue->hUpdateEvent;

	gMem->Unlock ();

	if (dwTimeout == 0)
		return MQ_ERROR_IO_TIMEOUT;

	if (lpOverlapped || pfnReceiveCallback) {
#if defined (winNT)
		SVSUTIL_ASSERT (0);
#endif
		return gOverlappedSupport->EnterOverlappedReceive (hQueueEvent,
					(lpOverlapped && (! pfnReceiveCallback)) ? lpOverlapped->hEvent : NULL,
					hCallerProc, dwProcPerm,
					lpOverlapped ? &lpOverlapped->Internal : NULL, pfnReceiveCallback,
					lpOverlappedA, hQueue, hCursor, dwTimeout, dwAction, pMsgPropsA);
	}

	HANDLE hWaitEvent = CreateEvent (NULL, FALSE, FALSE, NULL);

	HRESULT hr2 = MQ_OK;

	hr = gOverlappedSupport->EnterOverlappedReceive (hQueueEvent, hWaitEvent,
			hCallerProc, dwProcPerm, (DWORD *)&hr2, pfnReceiveCallback, NULL, hQueue,
			hCursor, dwTimeout, dwAction, pMsgPropsA);

	if (hr == MQ_INFORMATION_OPERATION_PENDING) {
		if (WaitForSingleObject (hWaitEvent, INFINITE) != WAIT_OBJECT_0)
			hr = MQ_ERROR_OPERATION_CANCELLED;
		else
			hr = hr2;
	}

	CloseHandle (hWaitEvent);

	return hr;
}


void scapi_ProcExit (void *pvProcIdent) {
	if (pvProcIdent) {
		if (! fApiInitialized)
			return;

		gMem->Lock ();

		if (fApiInitialized)
			gQueueMan->CloseProcHandles (pvProcIdent);

		gMem->Unlock ();
	}
}

//////////////////////////////////////////////////////////////////////
//
//
//	Unimplemented APIs
//
//
//////////////////////////////////////////////////////////////////////

HRESULT scapi_MQBeginTransaction (ITransaction **ppTransaction) {
    return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT scapi_MQGetQueueSecurity
(
WCHAR 					*lpszFormatName,
SECURITY_INFORMATION	SecurityInformation,
SECURITY_DESCRIPTOR		*pSecurityDescriptor,
DWORD					nLength,
DWORD					*dwlengthNeeded
) {
    return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT scapi_MQSetQueueSecurity
(
WCHAR 					*lpszFormatName,
SECURITY_INFORMATION	SecurityInformation,
SECURITY_DESCRIPTOR		*pSecurityDescriptor
) {
    return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT scapi_MQGetSecurityContext
(
VOID   *lpCertBuffer,
DWORD  dwCertBufferLength,
HANDLE *hSecurityContext
) {
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

void scapi_MQFreeSecurityContext (HANDLE hUnused) {
}

HRESULT scapi_MQInstanceToFormatName
(
GUID  *pGUID,
WCHAR *lpszFormatName,
DWORD *lpdwCount
) {
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT scapi_MQLocateBegin
(
WCHAR			*lpszContext,
MQRESTRICTION	*pRestriction,
MQCOLUMNSET		*pColumns,
MQSORTSET		*pSort,
HANDLE			*hEnum
) {
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT scapi_MQLocateNext
(
HANDLE 		hEnum,
DWORD  		*pcProps,
PROPVARIANT	aPropVar[]
) {
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT scapi_MQLocateEnd
(
HANDLE			hEnum
) {
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

//////////////////////////////////////////////////////////////////////
//
//
//	Overlapped support section
//
//
//////////////////////////////////////////////////////////////////////
void CloseOverlappedHandle (SVSHandle h) {
	gOverlappedSupport->CloseHandle (h);
}

void ScOverlappedSupport::CloseHandle (SCHANDLE h) {
	SVSUTIL_ASSERT (gMem->IsLocked ());

	for (int i = 0 ; i < iNumPendingRequests ; ++i) {
		ScIoRequest *pReq = apPendingRequests[i];
		while (pReq) {
			ScIoRequest *pNext = pReq->pNext;
			if ((pReq->hCursor == h) || (pReq->hQueue == h)) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_IO, L"Handle closed: cancel overlapped read request for queue handle %08x\n\texpiration time %08x action %08x properties ptr %08x\n\tcallback ptr %08x event %08x cursor handle %08x\n",
					pReq->hQueue, pReq->tExpirationTicks, pReq->dwAction, pReq->pMsgProps, pReq->pfnReceiveCallback, pReq->hEvent, pReq->hCursor);
#endif

				//
				//	If this is the last one, the index will be compressed - need to rescan the position
				//
				if (apPendingRequests[i]->pNext == NULL)
					--i;

				Deregister (pReq, MQ_ERROR_OPERATION_CANCELLED);
			}
			pReq = pNext;
		}
	}
}

void ScOverlappedSupport::SelfDestruct (void) {
	SVSUTIL_ASSERT (gMem->IsLocked ());

	if (hThread)
		TerminateThread (hThread, 0);

	if (ahQueueEvents[0])
		::CloseHandle (ahQueueEvents[0]);

	if (pTimeTree)
		delete pTimeTree;

	for (int i = 0 ; i < iNumPendingRequests ; ++i) {
		ScIoRequest *pReq = apPendingRequests[i];
		while (pReq) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_IO, L"Cancelled overlapped read request for queue handle %08x\n\texpiration time %08x action %08x properties ptr %08x\n\tcallback ptr %08x event %08x cursor handle %08x\n",
					pReq->hQueue, pReq->tExpirationTicks, pReq->dwAction, pReq->pMsgProps, pReq->pfnReceiveCallback, pReq->hEvent, pReq->hCursor);
#endif

			ScIoRequest *pNext = pReq->pNext;
#if defined (winCE)
			DWORD dwCurrentPerm = SetProcPermissions(pReq->dwProcPerm);
#endif
			__try {
				if (pReq->pdwStatus)
					*pReq->pdwStatus = MQ_ERROR_OPERATION_CANCELLED;

				if (pReq->pfnReceiveCallback) {
#if defined (winNT)
					SVSUTIL_ASSERT(0);
#endif
#if defined (winCE)
					CALLBACKINFO cbi;
					cbi.hProc  = pReq->hCallerProc;
					cbi.pfn    = (FARPROC)pReq->pfnReceiveCallback;
					cbi.pvArg0 = (void *)MQ_ERROR_OPERATION_CANCELLED;

					PerformCallBack (&cbi, pReq->hQueue, pReq->dwTimeout, pReq->dwAction,
									pReq->pMsgProps, pReq->lpOverlapped, pReq->hCursor);
#endif
				}
			 } __except (1) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_IO, L"Target application must have been shut down...\n");
#endif
			}

			if (pReq->hEvent)
				SetEvent (pReq->hEvent);

#if defined (winCE)
			SetProcPermissions(dwCurrentPerm);
#endif
			delete pReq;
			pReq = pNext;
		}
	}
}

void ScOverlappedSupport::CompressIndex (int iNdx) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	//
	//	We need to
	//		1) move arrays one position forward, and
	//		2) change indices of all moved items.
	//
	for (int i = iNdx + 1 ; i < iNumPendingRequests ; ++i) {
		pahQueueEvents[i-1] = pahQueueEvents[i];

		ScIoRequest *pIO = apPendingRequests[i];
		apPendingRequests[i-1] = pIO;
		SVSUTIL_ASSERT (pIO);
		while (pIO) {
			SVSUTIL_ASSERT (pIO->iNdx == i);
			pIO->iNdx = i - 1;
			pIO = pIO->pNext;
		}
	}

	--iNumPendingRequests;
}

void ScOverlappedSupport::Deregister (ScIoRequest *pRequest, HRESULT hr) {
	if (pRequest->pdwStatus || pRequest->pfnReceiveCallback) {
#if defined (winCE)
		DWORD dwCurrentPerm = SetProcPermissions(pRequest->dwProcPerm);
#endif
		__try {
			if (pRequest->pdwStatus)
				*pRequest->pdwStatus = hr;

			if (pRequest->pfnReceiveCallback) {
#if defined (winCE)
				CALLBACKINFO cbi;
				cbi.hProc  = pRequest->hCallerProc;
				cbi.pfn    = (FARPROC)pRequest->pfnReceiveCallback;
				cbi.pvArg0 = (void *)hr;

				PerformCallBack (&cbi, pRequest->hQueue, pRequest->dwTimeout,
									pRequest->dwAction, pRequest->pMsgProps,
									pRequest->lpOverlapped, pRequest->hCursor);
#else
				ASSERT(0);
#endif
			}
		} __except(1) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_IO, L"Caught exception while setting status...\n");
#endif
		}

		if (pRequest->hEvent)
			SetEvent (pRequest->hEvent);
#if defined (winCE)
		SetProcPermissions(dwCurrentPerm);
#endif
	}

	if (pRequest->pNode) {
		SVSUTIL_ASSERT (gMem->IsLocked ());
		pTimeTree->Delete (pRequest->pNode);
	}

	if (pRequest->iNdx >= 0) {
		SVSUTIL_ASSERT (gMem->IsLocked ());
		ScIoRequest *pParent = NULL;
		ScIoRequest *pRunner = apPendingRequests[pRequest->iNdx];
		while (pRunner) {
			if (pRunner == pRequest) {
				if (! pParent) {
					if (! pRequest->pNext) {	// Compress the array...
						CompressIndex (pRequest->iNdx);
					} else
						apPendingRequests[pRequest->iNdx] = pRequest->pNext;
				} else
					pParent->pNext = pRequest->pNext;
				break;
			}
			pParent = pRunner;
			pRunner = pRunner->pNext;
		}
	}

	delete pRequest;
}

static ScIoRequest *scapi_ReverseReqList (ScIoRequest *pReqList) {
	ScIoRequest *pNewHead = NULL;
	while (pReqList) {
		ScIoRequest *pNext = pReqList->pNext;

		pReqList->pNext = pNewHead;
		pNewHead = pReqList;

		pReqList = pNext;
	}

	return pNewHead;
}

void ScOverlappedSupport::OverlappedSupportThread(void) {
	gMem->Lock ();

	SVSUTIL_ASSERT (! fBusy);
	SVSUTIL_ASSERT (iNumPendingRequests > 0);

	for ( ; ; ) {
		DWORD dwTimeout = INFINITE;
		unsigned int	uiS;
		unsigned int	uiMS;
		svsutil_GetAbsTime (&uiS, &uiMS);

		SVSTNode *pNode = pTimeTree->Min ();

		while (pNode) {
			SVSTNode *pNextNode = pTimeTree->Next (pNode);

			ScIoRequest *pReq = (ScIoRequest *)SVSTree::GetData (pNode);
			if ((pReq->tExpirationS > uiS) || ((pReq->tExpirationS == uiS) && (pReq->tExpirationMS > uiMS))) {
				DWORD dwTimeout2 = (pReq->tExpirationS - uiS) * 1000 + (pReq->tExpirationMS - uiMS);

				if ((dwTimeout == INFINITE) || (dwTimeout > dwTimeout2))
					dwTimeout = dwTimeout2;

				if (pReq->tExpirationS > uiS + 1)
					break;
			} else {
				//
				//	Wait expired here...
				//
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_IO, L"Expired overlapped read request for queue handle %08x\n\texpiration time %08x action %08x properties ptr %08x\n\tcallback ptr %08x event %08x cursor handle %08x\n",
					pReq->hQueue, pReq->tExpirationTicks, pReq->dwAction, pReq->pMsgProps, pReq->pfnReceiveCallback, pReq->hEvent, pReq->hCursor);
#endif
				Deregister (pReq, MQ_ERROR_IO_TIMEOUT);
			}

			pNode = pNextNode;
		}

		DWORD dwRes;

		if (iNumPendingRequests <= 0) {
			gMem->Unlock ();
			dwRes = WaitForSingleObject (ahQueueEvents[0], SC_OVER_THREAD_TIMEOUT);
			gMem->Lock ();

			if ((iNumPendingRequests > 0) || (dwRes == WAIT_OBJECT_0))
				continue;

			break;
		}

		int      iNum = iNumPendingRequests + 1;

		gMem->Unlock ();
		dwRes = WaitForMultipleObjects (iNum, ahQueueEvents, FALSE, dwTimeout);
		gMem->Lock ();

		if (dwRes == WAIT_OBJECT_0)	// Just refresh
			continue;

		//
		//	Someone signalled?
		//
		if ((dwRes >= (WAIT_OBJECT_0 + 1)) && (dwRes <= (DWORD)(WAIT_OBJECT_0 + iNumPendingRequests))) {
			int iNdx = dwRes - WAIT_OBJECT_0 - 1;
			ScIoRequest *pReq = apPendingRequests[iNdx];
			CompressIndex (iNdx);

			ScIoRequest *pReqSav = pReq;

			while (pReq) {
				pTimeTree->Delete (pReq->pNode);

				pReq->iNdx  = -1;
				pReq->pNode = NULL;
				pReq = pReq->pNext;
			}

			pReq = scapi_ReverseReqList (pReqSav);

			fBusy++;
			gMem->Unlock ();

			DWORD dwNow = GetTickCount ();

			while (pReq) {
				ScIoRequest *pNext = pReq->pNext;
				DWORD dwCallTimeout = INFINITE;

#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_IO, L"At %08x : Processing overlapped read request for queue handle %08x\n\texpiration time %08x action %08x properties ptr %08x\n\tcallback ptr %08x event %08x cursor handle %08x\n",
					dwNow , pReq->hQueue, pReq->tExpirationTicks, pReq->dwAction, pReq->pMsgProps, pReq->pfnReceiveCallback, pReq->hEvent, pReq->hCursor);
#endif
				if (pReq->dwTimeout == INFINITE)
					dwCallTimeout = INFINITE;
				else if (time_greater (pReq->tExpirationTicks, dwNow))
					dwCallTimeout = pReq->tExpirationTicks - dwNow;
				else
					dwCallTimeout = 0;

#if defined (winCE)
				DWORD dwCurrentPerm = SetProcPermissions(pReq->dwProcPerm);
#endif
				HRESULT hr = scapi_MQReceiveMessageI (pReq);

#if defined (winCE)
				SetProcPermissions (dwCurrentPerm);
#endif

				if (hr != MQ_INFORMATION_OPERATION_PENDING) {
#if defined (SC_VERBOSE)
					scerror_DebugOut (VERBOSE_MASK_IO, L"Overlapped I/O call result %08x\n", hr);
#endif
					Deregister (pReq, hr);
				}

				pReq = pNext;
			}

			gMem->Lock ();
			fBusy--;
		}

		//
		//	Someone abandoned?
		//
		if ((dwRes >= (WAIT_ABANDONED_0 + 1)) && (dwRes <= (DWORD)(WAIT_ABANDONED_0 + iNumPendingRequests))) {
			int iNdx = dwRes - WAIT_ABANDONED_0 - 1;
			ScIoRequest *pReq = apPendingRequests[iNdx];
			CompressIndex (iNdx);
			while (pReq) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_IO, L"Abandoned overlapped read request for queue handle %08x\n\texpiration time %08x action %08x properties ptr %08x\n\tcallback ptr %08x event %08x cursor handle %08x\n",
					pReq->hQueue, pReq->tExpirationTicks, pReq->dwAction, pReq->pMsgProps, pReq->pfnReceiveCallback, pReq->hEvent, pReq->hCursor);
#endif
				ScIoRequest *pNext = pReq->pNext;
				pReq->iNdx = -1;
				Deregister (pReq, MQ_ERROR_OPERATION_CANCELLED);
				pReq = pNext;
			}
		}
	}

	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT (iNumPendingRequests == 0);
	SVSUTIL_ASSERT (! fBusy);

	hThread = NULL;

	gMem->Unlock ();
}

DWORD WINAPI ScOverlappedSupport::OverlappedSupportThread_s (void *pvParam) {
	ScOverlappedSupport *pOS = (ScOverlappedSupport *)pvParam;
	pOS->OverlappedSupportThread ();

	return 0;
}

HRESULT ScOverlappedSupport::EnterOverlappedReceive
(
HANDLE				hQueueEvent,
HANDLE				hWaitEvent,
HANDLE				hCallerProc,
DWORD				dwProcPerm,
DWORD				*pdwStatus,
PMQRECEIVECALLBACK	pfnReceiveCallback,
OVERLAPPED			*lpOverlapped,
SCHANDLE			hQueue,
SCHANDLE			hCursor,
DWORD				dwTimeout,
DWORD				dwAction,
SCPROPVAR			*pMsgProps
) {
	if (hWaitEvent && (! ResetEvent (hWaitEvent)))
		return MQ_ERROR_INVALID_HANDLE;

	HRESULT hr = MQ_INFORMATION_OPERATION_PENDING;
	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	int iNdx = -1;
	for (int i = 0 ; i < iNumPendingRequests ; ++i) {
		if (pahQueueEvents[i] == hQueueEvent) {
			iNdx = i;
			break;
		}
	}

	if ((iNdx < 0) && (iNumPendingRequests < (MAXIMUM_WAIT_OBJECTS - 1))) {
		iNdx = iNumPendingRequests;
		apPendingRequests[iNdx] = NULL;
	}

	if (iNdx >= 0) {
		ScIoRequest *pIO = new ScIoRequest(hWaitEvent, hCallerProc, dwProcPerm, pdwStatus, pfnReceiveCallback, lpOverlapped, hQueue, hCursor, dwTimeout, dwAction, pMsgProps);
		if (pIO) {
			pIO->pNode = pTimeTree->Insert (pIO->tExpirationS, pIO);
			if (pIO->pNode) {
				pIO->pNext = apPendingRequests[iNdx];

				apPendingRequests[iNdx] = pIO;
				pahQueueEvents[iNdx]    = hQueueEvent;

				pIO->iNdx = iNdx;

				if (iNdx == iNumPendingRequests)
					++iNumPendingRequests;

#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_IO, L"Entering async read request for queue handle %08x\n\texpiration time %08x action %08x properties ptr %08x\n\tcallback ptr %08x event %08x cursor handle %08x\n",
					pIO->hQueue, pIO->tExpirationTicks, pIO->dwAction, pIO->pMsgProps, pIO->pfnReceiveCallback, pIO->hEvent, pIO->hCursor);
#endif
				SpinThread ();
			} else {
				delete pIO;
				hr = MQ_ERROR_INSUFFICIENT_RESOURCES;
			}
		} else
			hr = MQ_ERROR_INSUFFICIENT_RESOURCES;
	} else
		hr = MQ_ERROR_INSUFFICIENT_RESOURCES;

	gMem->Unlock ();

	return hr;
}

HRESULT ScOverlappedSupport::EnterOverlappedReceive
(
HANDLE			hQueueEvent,
ScIoRequest		*pIO
) {
	HRESULT hr = MQ_INFORMATION_OPERATION_PENDING;
	gMem->Lock ();

	if (! fApiInitialized) {
		gMem->Unlock ();
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}

	int iNdx = -1;
	for (int i = 0 ; i < iNumPendingRequests ; ++i) {
		if (pahQueueEvents[i] == hQueueEvent) {
			iNdx = i;
			break;
		}
	}

	if ((iNdx < 0) && (iNumPendingRequests < (MAXIMUM_WAIT_OBJECTS - 1))) {
		iNdx = iNumPendingRequests;
		apPendingRequests[iNdx] = NULL;
	}

	if (iNdx >= 0) {
		pIO->pNode = pTimeTree->Insert (pIO->tExpirationS, pIO);
		if (pIO->pNode) {
			pIO->pNext = apPendingRequests[iNdx];

			apPendingRequests[iNdx] = pIO;
			pahQueueEvents[iNdx]    = hQueueEvent;

			pIO->iNdx = iNdx;

			if (iNdx == iNumPendingRequests)
				++iNumPendingRequests;

#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_IO, L"Re-Entering async read request for queue handle %08x\n\texpiration time %08x action %08x properties ptr %08x\n\tcallback ptr %08x event %08x cursor handle %08x\n",
				pIO->hQueue, pIO->tExpirationTicks, pIO->dwAction, pIO->pMsgProps, pIO->pfnReceiveCallback, pIO->hEvent, pIO->hCursor);
#endif
			SpinThread ();
		} else
			hr = MQ_ERROR_INSUFFICIENT_RESOURCES;
	} else
		hr = MQ_ERROR_INSUFFICIENT_RESOURCES;

	gMem->Unlock ();

	return hr;
}

//
//	Console, debug and NT RPC support
//
#if defined (SC_INCLUDE_CONSOLE) && (! defined (MSMQ_ANCIENT_OS))
#include <stdio.h>
#include <scsman.hxx>
#include <scorder.hxx>

#if defined (winCE)
typedef _CRTIMP int (*wprintf_t) (const wchar_t *, ...);
typedef _CRTIMP int (*printf_t) (const char *, ...);
typedef _CRTIMP wchar_t *(*getws_t)(wchar_t *);
typedef _CRTIMP int (*putws_t)(const wchar_t *);

static wprintf_t ptr_wprintf;
static printf_t ptr_printf;
static getws_t ptr_getws;
static putws_t ptr_putws;

#define wprintf ptr_wprintf
#define _putws ptr_putws
#define _getws ptr_getws
#define printf ptr_printf
#endif

void scapi_DumpMem (unsigned char *p, int iSz) {
	int iOut = 0;
	unsigned char *pSav = p;
	wprintf (L"%08x |  ", p - pSav);
	while (--iSz >= 0) {
		wprintf (L"%02x ", *p);
		++iOut;
		++p;
		if (iOut == 8) {
			wprintf (L" |  ");
			p -= iOut;
			for (int i = 0 ; i < iOut ; ++i, ++p)
				wprintf (L"%c", (*p >= ' ' && *p <= 126) ? *p : '.');
			wprintf (L"\n%08x |  ", p - pSav);
			iOut = 0;
		}
	}

	for (int i = iOut ; i < 8 ; ++i )
		wprintf (L"   ");

	wprintf (L" |  ");
	p -= iOut;
	for (i = 0 ; i < iOut ; ++i, ++p)
		wprintf (L"%c", (*p >= ' ' && *p <= 126) ? *p : '.');
	wprintf (L"\n");
}

int scapi_HandleDumper (SVSHandle h, void *pUnused) {
	wprintf (L"Handle %08x ", h);

	ScHandleInfo *pHInfo = gQueueMan->QueryHandle (h);
	if (! pHInfo) {
		wprintf (L": can't query - system is badly corrupted\n", h);
		return 0;
	}

	if (pHInfo->uiHandleType == SCQMAN_HANDLE_QUEUE) {
		wprintf (L" [Queue %s] Share mode %d Access %d Default Cursor %08x\n", pHInfo->pQueue->lpszFormatName, pHInfo->q.uiShareMode, pHInfo->q.uiAccess, pHInfo->q.hDefaultCursor);
	} else if (pHInfo->uiHandleType == SCQMAN_HANDLE_CURSOR) {
		wprintf (L" [Cursor for Queue %s] Queue Handle %08x Current Position %08x Pos Valid %d", pHInfo->pQueue->lpszFormatName, pHInfo->c.hQueue, pHInfo->c.pNode, pHInfo->c.fPosValid);
	} else {
		wprintf (L"Unknown handle...\n");
		return 0;
	}

	return 1;
}

void scapi_TimerDump (unsigned int dwWhen, SVSHandle hWhat, void *pUnused) {
	wprintf (L"At %08x signal %08x\n", dwWhen, hWhat);
}

DWORD WINAPI scapi_UserControlThread (LPVOID lpParameter) {
#if defined (winCE)
	HINSTANCE hLib = LoadLibrary (L"coredll.dll");
	if (! hLib)
		return 1;

	ptr_wprintf = (wprintf_t)GetProcAddress (hLib, L"wprintf");
	ptr_printf = (printf_t)GetProcAddress (hLib, L"printf");
	ptr_getws = (getws_t)GetProcAddress (hLib, L"_getws");
	ptr_putws = (putws_t)GetProcAddress (hLib, L"_putws");

	if (! (ptr_wprintf && ptr_printf && ptr_getws && ptr_putws)) {
		FreeLibrary (hLib);
		return 1;
	}
#endif

	_putws (L"MSMQ Small Client Monitor V1.0.");
	for ( ; ; ) {
		WCHAR szBuffer[500];
		wprintf (L"Ticks = %08x Secs = %08x MSMQ> ", GetTickCount(), scutil_now());
		if ((! _getws (szBuffer)) || (wcsicmp (szBuffer, L"exit") == 0))
			break;
		else if (wcsnicmp (szBuffer, L"dump ", 5) == 0) {
			if (! fApiInitialized) {
				wprintf (L"MSMQ service not started.\n");
				continue;
			}

			gMem->Lock ();
			if (! fApiInitialized) {
				gMem->Unlock ();
				wprintf (L"MSMQ service not started.\n");
				continue;
			}
			switch (szBuffer[5]) {
			case L'x': {
				if (! gSeqMan) {
					wprintf (L"Sequence manager not initialized\n");
					break;
				}

				wprintf (L"Sequence Pulse     : %s initialized\n", (! gSeqMan->hSequencePulse) ? L"not" : L"");
				wprintf (L"File               : %s initialized\n", (gSeqMan->hBackupFile == INVALID_HANDLE_VALUE) ? L"not" : L"");
				wprintf (L"Strings            : %s initialized\n", (gSeqMan->hStringFile == INVALID_HANDLE_VALUE) ? L"not" : L"");
				wprintf (L"File name          : %s\n", gSeqMan->lpszBackupName);
				wprintf (L"String name        : %s\n", gSeqMan->lpszStringName);
				wprintf (L"Last backup access : %08x\n", gSeqMan->uiLastBackupAccessT);
				wprintf (L"Last string access : %08x\n", gSeqMan->uiLastStringAccessT);
				wprintf (L"Last compact       : %08x\n", gSeqMan->uiLastCompactT);
				wprintf (L"Last prune         : %08x\n", gSeqMan->uiLastPruneT);

				wprintf (L"Clusters           : %d\n", gSeqMan->uiClustersCount);
				wprintf (L"Clusters allocated : %d\n", gSeqMan->uiClustersAlloc);
				wprintf (L"Clusters:\n");
				for (int i = 0 ; i < (int)gSeqMan->uiClustersCount ; ++i) {
					ScOrderCluster *pCluster = gSeqMan->ppClusters[i];
					wprintf (L"Cluster %d\n", i);
					wprintf (L"    Free Entries: %d\n", pCluster->iNumFree);
					wprintf (L"    Dirty       : %s\n", pCluster->iDirty ? L"yes" : L"no");
					wprintf (L"    First Free  : %d\n", pCluster->h.uiFirstFreeBlock);
					wprintf (L"    Sequences in cluster:\n");
					for (int i = 0 ; i < SCSEQUENCE_ENTRIES_PER_BLOCK ; ++i) {
						if (! pCluster->pos[i].uiLastAccessS)
							wprintf (L"        Cluster %d <<< FREE >>> next free %d\n", i, pCluster->pos[i].uiCurrentSeqN);
						else {
							wprintf (L"        Cluster %d\n", i);
							wprintf (L"            Local Queue GUID    : " SC_GUID_FORMAT L"\n", SC_GUID_ELEMENTS((&pCluster->pos[i].guidQ)));
							wprintf (L"            Local Name GUID     : " SC_GUID_FORMAT L"\n", SC_GUID_ELEMENTS((&pCluster->pos[i].guidQueueName)));
							wprintf (L"            Stream Id GUID      : " SC_GUID_FORMAT L"\n", SC_GUID_ELEMENTS((&pCluster->pos[i].guidStreamId)));
							wprintf (L"            Ack Queue Name GUID : " SC_GUID_FORMAT L"\n", SC_GUID_ELEMENTS((&pCluster->pos[i].guidOrderAck)));
							wprintf (L"            Current Sequence ID : %016I64x\n", pCluster->pos[i].llCurrentSeqID);
							wprintf (L"            Current Sequence N  : %d\n", pCluster->pos[i].uiCurrentSeqN);
							wprintf (L"            Last Access (sec)   : %08x\n", pCluster->pos[i].uiLastAccessS);
						}
					}
				}
				wprintf (L"Active sequences:\n");
				ScOrderSeq *pSeq = gSeqMan->pSeqActive;
				while (pSeq) {
					wprintf (L"    Local transactional queue : %s\n", pSeq->pQueue->lpszFormatName);
					wprintf (L"    Local queue guid          : " SC_GUID_FORMAT L"\n", SC_GUID_ELEMENTS ((&pSeq->p->guidQ)));
					wprintf (L"    Local queue name          : %s\n", gSeqMan->GetStringFromGUID(&pSeq->p->guidQueueName));
					wprintf (L"    Local queue name guid     : " SC_GUID_FORMAT L"\n", SC_GUID_ELEMENTS ((&pSeq->p->guidQueueName)));
					wprintf (L"    StreamId                  : %s\n", gSeqMan->GetStringFromGUID(&pSeq->p->guidStreamId));
					wprintf (L"    Stream Id GUID            : " SC_GUID_FORMAT L"\n", SC_GUID_ELEMENTS ((&pSeq->p->guidStreamId)));
					wprintf (L"    Order queue name          : %s\n", gSeqMan->GetStringFromGUID(&pSeq->p->guidOrderAck));
					wprintf (L"    Order queue guid          : " SC_GUID_FORMAT L"\n", SC_GUID_ELEMENTS ((&pSeq->p->guidOrderAck)));
					wprintf (L"    Active                    : %s\n", pSeq->fActive ? L"yes" : L"no");
					wprintf (L"    Current Sequence ID       : %016I64x\n", pSeq->p->llCurrentSeqID);
					wprintf (L"    Current Sequence Number   : %d\n", pSeq->p->uiCurrentSeqN);
					wprintf (L"    Last Access (sec)         : %08x\n\n", pSeq->p->uiLastAccessS);

					pSeq = pSeq->pNextActive;
				}
			}
				break;

			case L'o': {
					if (! gOverlappedSupport) {
						wprintf (L"Support for overlapped I/O not initialized\n");
						break;
					}
					wprintf (L"Overlapped I/O support:\n\n");
					wprintf (L"Structure init: %s\n", gOverlappedSupport->fInitialized ? L"completed" : L"failed");
					wprintf (L"I/O Thread    : %08x\n", gOverlappedSupport->hThread);
					wprintf (L"Pending queues: %d\n", gOverlappedSupport->iNumPendingRequests);
					wprintf (L"Refresh semaphore %08x\n", gOverlappedSupport->ahQueueEvents[0]);
					for (int i = 0 ; i < gOverlappedSupport->iNumPendingRequests ; ++i) {
						wprintf (L"    Queue semaphore %08x\n", gOverlappedSupport->pahQueueEvents[i]);
						ScIoRequest *pIO = gOverlappedSupport->apPendingRequests[i];

						while (pIO) {
#if defined (winCE)
							DWORD dwCurrentPerm = SetProcPermissions(pIO->dwProcPerm);
#endif
							wprintf (L"        Request %08x\n", pIO);
							wprintf (L"            Overlapped event: %08x\n", pIO->hEvent);
							wprintf (L"            Calling process : %08x\n", pIO->hCallerProc);
							wprintf (L"            Callback ptr    : %08x\n", pIO->pfnReceiveCallback);
							wprintf (L"            Overlapped ptr  : %08x\n", pIO->lpOverlapped);
							wprintf (L"            Status ptr      : %08x\n", pIO->pdwStatus);
							wprintf (L"            Permissions     : %08x\n", pIO->dwProcPerm);
							wprintf (L"            Queue Handle    : %d\n", pIO->hQueue);
							wprintf (L"            Cursor Handle   : %d\n", pIO->hCursor);
							wprintf (L"            Expiration Time : %08x\n", pIO->tExpirationTicks);
							wprintf (L"            Expiration (s)  : %08x\n", pIO->tExpirationS);
							wprintf (L"            Expiration (ms) : %08x\n", pIO->tExpirationMS);
							wprintf (L"            Timeout         : %08x\n", pIO->dwTimeout);
							wprintf (L"            Action          : %08x\n", pIO->dwAction);
							wprintf (L"            Node            : %08x\n", pIO->pNode);
							wprintf (L"            Properties ptr  : %08x\n", pIO->pMsgProps);
							wprintf (L"            Index           : %08x\n", pIO->iNdx);
#if defined (winCE)
							SetProcPermissions(dwCurrentPerm);
#endif
							pIO = pIO->pNext;
						}
					}
					wprintf (L"Tree enumeration:\n");
					SVSTNode *pMinNode = gOverlappedSupport->pTimeTree->Min();
					while (pMinNode) {
						wprintf (L"    Node %08x Data %08x Key %08x\n", pMinNode,
								SVSTree::GetData (pMinNode), SVSTree::GetKey (pMinNode));
						pMinNode = gOverlappedSupport->pTimeTree->Next(pMinNode);
					}
					break;
				}
			case L'g': {
					_putws (L"Global Info Dump:\n\n");
					wprintf (L" Globa Data : %s\n", gMem->fInitialized ? L"Initialized" : L"Not initialized");

					wprintf (L" Packet Memory    : %s\n", gMem->pPacketMem ? L"Initialized" : L"Not initialized");
					wprintf (L" Tree Node Memory : %s\n", gMem->pTreeNodeMem ? L"Initialized" : L"Not initialized");
					wprintf (L" Ack  Node Memory : %s\n", gMem->pAckNodeMem ? L"Initialized" : L"Not initialized");
					wprintf (L" String Hash      : %s\n", gMem->pStringHash ? L"Initialized" : L"Not initialized");

					if (gMem->pTimer) {
						wprintf (L"System timer  : initialized. Contents follow:\n");
						svsutil_WalkAttrTimer (gMem->pTimer, scapi_TimerDump, NULL);
					} else
						wprintf (L"System timer  : not initialized\n");

					wprintf    (L"Protocol port   : %d\n", gMachine->uiPort);
					wprintf    (L"Ping     port   : %d\n", gMachine->uiPingPort);
					wprintf    (L"QM Server       : %s\n", gMachine->lpszHostName);
					wprintf    (L"DefaultInQuota  : %d K\n", gMachine->uiDefaultInQuotaK);
					wprintf    (L"DefaultOutQuota : %d K\n", gMachine->uiDefaultOutQuotaK);
					wprintf    (L"MachineQuota    : %d K\n", gMachine->uiMachineQuotaK);
					wprintf    (L"MachineQuotaUsed: %d K\n", gMachine->iMachineQuotaUsedB);
					wprintf    (L"Start UID       : %08x\n", gMachine->uiStartID);
					wprintf    (L"Local QM GUID   : " GUID_FORMAT L"\n", GUID_ELEMENTS((&gMachine->guid)));
					wprintf    (L"QM Directory    : %s\n", gMachine->lpszDirName);
					wprintf    (L"Host Name       : %s\n", gMachine->lpszHostName);
					wprintf    (L"Debug Queue     : %s\n", gMachine->lpszDebugQueueFormatName);
				}
				break;

			case L't': {
					_putws (L"Time-out tree dump\n");
					SVSTNode *pNode = gMem->pTimeoutTree->Min();
					while (pNode) {
						ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pNode);
						wprintf (L"    Packet ptr %08x UID %08x Expiration %08x Queue %s\n", pPacket, pPacket->uiMessageID, pPacket->tExpirationTime, pPacket->pQueue ? pPacket->pQueue->lpszFormatName : L"None");
						pNode = gMem->pTimeoutTree->Next(pNode);
					}
					break;
				}
			case L'q': {
					_putws (L"Queue Manager Dump:\n");
					ScQueueManager *pqm = gQueueMan;
					wprintf (L"Current Message ID : %08x\n", pqm->uiMessageID);
					wprintf (L"Handle System      : %s\n", pqm->pHandles    ? L"initialized" : L"not initialized");
					wprintf (L"Handle Memory      : %s\n", pqm->pHandleMem  ? L"initialized" : L"not initialized");
					wprintf (L"Main Thread        : %s\n", pqm->hMainThread ? L"initialized" : L"not initialized");
					wprintf (L"Garbage collector  : %s\n", pqm->hPacketExpired ? L"initialized" : L"not initialized");
					wprintf (L"Dead letter queue  : %s\n", pqm->pQueueDLQ      ? pqm->pQueueDLQ->lpszFormatName      : L"not initialized");
					wprintf (L"Machine journal    : %s\n", pqm->pQueueJournal  ? pqm->pQueueJournal->lpszFormatName  : L"not initialized");
					wprintf (L"Out-FRS     queue  : %s\n", pqm->pQueueOutFRS   ? pqm->pQueueOutFRS->lpszFormatName   : L"not initialized");
					wprintf (L"Order ACK   queue  : %s\n", pqm->pQueueOrderAck ? pqm->pQueueOrderAck->lpszFormatName : L"not initialized");

					_putws (L"Handle Table Dump:\n");
					gQueueMan->pHandles->FilterHandles (scapi_HandleDumper, NULL);

					int i = 0;
					ScQueueList *pql = pqm->pqlIncoming;
					_putws (L"Incoming Queues:");

					if (! pql)
						_putws (L"    None...");

					while (pql) {
						wprintf (L"    %d. %s\n", ++i, pql->pQueue->lpszFormatName);
						pql = pql->pqlNext;
					}

					pql = pqm->pqlOutgoing;
					_putws (L"Outgoing Queues:");

					if (! pql)
						_putws (L"    None...");

					while (pql) {
						wprintf (L"    %d. %s\n", ++i, pql->pQueue->lpszFormatName);
						pql = pql->pqlNext;
					}
				}
				break;
			case L's': {
					ScSessionManager *pSessMan = gSessionMan;
					_putws (L"Session Manager Dump:\n");
					wprintf (L"Session manager    : %s\n", pSessMan->fInitialized ? L"initialized" : L"not initialized");
					wprintf (L"Accepting  Thread  : %08x\n", pSessMan->hAccThread);
					wprintf (L"Event              : %08x\n", pSessMan->hBuzz);
					wprintf (L"Socket             : %08x\n", pSessMan->s_listen);
					wprintf (L"Active pairs       : %d\n", pSessMan->iThreadPairs);

					_putws (L"Sessions Dump:");
					ScSession *pSessRun = pSessMan->pSessList;
					if (! pSessRun)
						_putws (L"    none...");

					while (pSessRun) {
						wprintf  (L"    Session %s\n", pSessRun->lpszHostName);
						printf    ("    (ASCII) %s\n", pSessRun->lpszmbHostName);
						wprintf  (L"        State                   : %s\n",
							 (pSessRun->fSessionState == SCSESSION_STATE_INACTIVE)	       ? L"Inactive" :
							((pSessRun->fSessionState == SCSESSION_STATE_CONNECTING)       ? L"Connecting" :
							((pSessRun->fSessionState == SCSESSION_STATE_CONNECTED_CLIENT) ? L"Connected/Client" :
							((pSessRun->fSessionState == SCSESSION_STATE_CONNECTED_SERVER) ? L"Connected/Server" :
							((pSessRun->fSessionState == SCSESSION_STATE_OPERATING)        ? L"Operating" :
							((pSessRun->fSessionState == SCSESSION_STATE_EXITING)          ? L"Exiting" :
							((pSessRun->fSessionState == SCSESSION_STATE_WAITING)          ? L"Waiting" : L"Undefined")))))));
						wprintf  (L"        Failures                : %d\n", pSessRun->iFailures);
						wprintf  (L"        Next Connection Attempt : %08x\n", pSessRun->uiNextAttemptTime);
						wprintf  (L"        Event                   : %08x\n", pSessRun->hEvent);
						wprintf  (L"        Sending Thread          : %08x\n", pSessRun->hServiceThreadW);
						wprintf  (L"        Receiving Thread        : %08x\n", pSessRun->hServiceThreadR);
						wprintf  (L"        Socket                  : %d\n", pSessRun->s);
						wprintf  (L"        GUID                    : " GUID_FORMAT L"\n", GUID_ELEMENTS((&pSessRun->guidDest)));
						wprintf  (L"        Connection stamp/delta  : %08x\n", pSessRun->uiConnectionStamp);
						wprintf  (L"        ACK Timeout             : %d\n", pSessRun->uiAckTimeout);
						wprintf  (L"        Store ACK Timeout       : %d\n", pSessRun->uiStoreAckTimeout);
						wprintf  (L"        Local Window Size       : %d\n", pSessRun->uiMyWindowSize);
						wprintf  (L"        Remote Window Size      : %d\n", pSessRun->uiOtherWindowSize);
						wprintf  (L"        Last ACK sent           : %d\n", pSessRun->usLastAckSent);
						wprintf  (L"        Last reliable ACK sent  : %d\n", pSessRun->usLastRelAckSent);
						wprintf  (L"        Packets sent            : %d\n", pSessRun->usPacketsSent);
						wprintf  (L"        Reliable Packets sent   : %d\n", pSessRun->usRelPacketsSent);
						wprintf  (L"        Last Ack recvd          : %d\n", pSessRun->usLastAckReceived);
						wprintf  (L"        Packets recvd           : %d\n", pSessRun->usPacketsReceived);
						wprintf  (L"        Reliable Packets recvd  : %d\n", pSessRun->usRelPacketsReceived);
						wprintf  (L"        ACK Due                 : %08x\n", pSessRun->uiAckDue);
						SentPacket *pSP = pSessRun->pSentPackets;
						while (pSP) {
							wprintf  (L"            Pending express packet ptr %08x UID %08x from queue %s : sent no %d\n",
									pSP->pPacket, pSP->pPacket->uiMessageID, pSP->pPacket->pQueue->lpszFormatName, pSP->usNum);

							pSP = pSP->pNext;
						}
						pSP = pSessRun->pSentRelPackets;
						while (pSP) {
							wprintf  (L"            Pending reliable packet ptr %x UID %08x from queue %s : sent no %d\n",
									pSP->pPacket, pSP->pPacket->uiMessageID, pSP->pPacket->pQueue->lpszFormatName, pSP->usNum);

							pSP = pSP->pNext;
						}
						pSessRun = pSessRun->pNext;
					}
				}
				break;

			case L'p': {
					int i = 0;
					ScQueueList *pql = gQueueMan->pqlIncoming;
					wprintf (L"Total\t\tReliable\t\tQueue Format Name\n");
					while (pql) {
						SVSTNode *pNode = pql->pQueue->pPackets->Min();

						int iPackets    = 0;
						int iRelPackets = 0;

						while (pNode) {
							ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pNode);

							if (pPacket->iDirEntry != -1)
								++iRelPackets;

							++iPackets;
							pNode = pql->pQueue->pPackets->Next(pNode);
						}

						wprintf (L"%d\t\t%d\t\t%s\n", iPackets, iRelPackets, pql->pQueue->lpszFormatName);
						pql = pql->pqlNext;
					}

					pql = gQueueMan->pqlOutgoing;

					while (pql) {
						SVSTNode *pNode = pql->pQueue->pPackets->Min();

						int iPackets    = 0;
						int iRelPackets = 0;

						while (pNode) {
							ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pNode);

							if (pPacket->iDirEntry != -1)
								++iRelPackets;

							++iPackets;
							pNode = pql->pQueue->pPackets->Next(pNode);
						}

						wprintf (L"%d\t\t%d\t\t%s\n", iPackets, iRelPackets, pql->pQueue->lpszFormatName);
						pql = pql->pqlNext;
					}
				}
				break;

			case L'b': {
			case L'f':
				int iQueueNum = 0;
				WCHAR *pX = &szBuffer[6];
				while (*pX == L' ')
					++pX;
				while ((*pX >= L'0') && (*pX <= L'9')) {
					iQueueNum = iQueueNum * 10 + (*pX - L'0');
					++pX;
				}

				ScQueueManager *pqm = gQueueMan;
				int i = 0;
				ScQueueList *pql = pqm->pqlIncoming;

				while (pql) {
					if (++i == iQueueNum)
						break;
					pql = pql->pqlNext;
				}

				if (! pql) {
					pql = pqm->pqlOutgoing;

					while (pql) {
						if (++i == iQueueNum)
							break;

						pql = pql->pqlNext;
					}
				}
				if (! pql) {
					wprintf (L"Queue %d was not found.\n", iQueueNum);
					break;
				}

				ScQueue *pQueue = pql->pQueue;

				wprintf (L"Dumping queue %s\n", pQueue->lpszFormatName);
				wprintf (L"    Initialized            : %s\n", pQueue->fInitialized ? L"yes" : L"no");
				wprintf (L"    File                   : %s\n", pQueue->sFile->lpszFileName);
				wprintf (L"        Open               : %s\n", pQueue->sFile->hBackupFile == INVALID_HANDLE_VALUE ? L"no" : L"yes");
				wprintf (L"        Last used          : %08x\n", pQueue->sFile->tLastUsed);
				wprintf (L"        Header Offset      : %d\n", pQueue->sFile->pFileDir->uiQueueHeaderOffset);
				wprintf (L"        Header Size        : %d\n", pQueue->sFile->pFileDir->uiQueueHeaderSize);
				wprintf (L"        Dir Offset         : %d\n", pQueue->sFile->pFileDir->uiPacketDirOffset);
				wprintf (L"        Dir Size           : %d\n", pQueue->sFile->pFileDir->uiPacketDirSize);
				wprintf (L"        Backup Offset      : %d\n", pQueue->sFile->pFileDir->uiPacketBackupOffset);
				wprintf (L"        Backup Size        : %d\n", pQueue->sFile->pFileDir->uiPacketBackupSize);
				wprintf (L"        Dir Entries        : %d\n", pQueue->sFile->pPacketDir->uiNumEntries);
				wprintf (L"        Dir Entries (Used) : %d\n", pQueue->sFile->pPacketDir->uiUsedEntries);
				wprintf (L"        First Free in Map  : %d\n", pQueue->sFile->iFreeIndex);
				wprintf (L"        Map Size           : %d\n", pQueue->sFile->iMapSize);
				wprintf (L"        Queue Inited       : %s\n", pQueue->sFile->pQueue ? L"yes" : L"no");
				wprintf (L"    Packet Storage Inited  : %s\n", pQueue->pPackets      ? L"yes" : L"no");
				wprintf (L"    Journal                : %s\n", pQueue->pJournal ? pQueue->pJournal->lpszFormatName : L"none");
				wprintf (L"    Host                   : %s\n", pQueue->lpszQueueHost);
				wprintf (L"    Name                   : %s\n", pQueue->lpszQueueName);
				wprintf (L"    Label                  : %s\n", pQueue->lpszQueueLabel);
				wprintf (L"    Incoming               : %s\n", pQueue->qp.bIsIncoming    ? L"yes" : L"no");
				wprintf (L"    Internal               : %s\n", pQueue->qp.bIsInternal    ? L"yes" : L"no");
				wprintf (L"    Public                 : %s\n", pQueue->qp.bIsPublic      ? L"yes" : L"no");
				wprintf (L"    Protected              : %s\n", pQueue->qp.bIsProtected   ? L"yes" : L"no");
				wprintf (L"    Is Journal             : %s\n", pQueue->qp.bIsJournal     ? L"yes" : L"no");
				wprintf (L"    Is Dead Letter Queue   : %s\n", pQueue->qp.bIsDeadLetter  ? L"yes" : L"no");
				wprintf (L"    Is Machine Journal     : %s\n", pQueue->qp.bIsMachineJournal ? L"yes" : L"no");
				wprintf (L"    Is Order Ack           : %s\n", pQueue->qp.bIsOrderAck    ? L"yes" : L"no");
				wprintf (L"    Is Out-FRS             : %s\n", pQueue->qp.bIsOutFRS      ? L"yes" : L"no");
				wprintf (L"    Has Journal            : %s\n", pQueue->qp.bHasJournal    ? L"yes" : L"no");
				wprintf (L"    Journal On             : %s\n", pQueue->qp.bIsJournalOn   ? L"yes" : L"no");
				wprintf (L"    Authenticate           : %s\n", pQueue->qp.bAuthenticate  ? L"yes" : L"no");
				wprintf (L"    Transactional          : %s\n", pQueue->qp.bTransactional ? L"yes" : L"no");
				wprintf (L"    Quota                  : %d K\n", pQueue->qp.uiQuotaK);
				wprintf (L"    Privacy Level          : %d\n", pQueue->qp.uiPrivacyLevel);
				wprintf (L"    Base Priority          : %d\n", pQueue->qp.iBasePriority);
				wprintf (L"    GUID                   : " GUID_FORMAT L"\n", GUID_ELEMENTS((&pQueue->qp.guid)));
				wprintf (L"    Type GUID              : " GUID_FORMAT L"\n", GUID_ELEMENTS((&pQueue->qp.guidQueueType)));
				wprintf (L"    Creation Time          : %08x\n", pQueue->tCreation);
				wprintf (L"    Last Modification Time : %08x\n", pQueue->tModification);
				wprintf (L"    Currently DENYALL?     : %s\n", pQueue->fDenyAll ? L"yes" : L"no");
				wprintf (L"    Opened for receiving   : %d\n", pQueue->uiOpenRecv);
				wprintf (L"    Opened				  : %d\n", pQueue->uiOpen);
				wprintf (L"    Event Inited           : %s\n", pQueue->hUpdateEvent ? L"yes" : L"no");
				wprintf (L"    Session Inited         : %s\n", pQueue->pSess ? L"yes" : L"no");
				wprintf (L"    Sequence ID            : %016I64x\n", pQueue->llSeqID);
				wprintf (L"    Sequence Number        : %08x\n", pQueue->uiSeqNum);
				wprintf (L"    Received messages      : %08x\n", pQueue->hkReceived);

				{
					SVSTNode *pNode = pQueue->pPackets->Min();
					int iPackets    = 0;
					int iRelPackets = 0;

					while (pNode) {
						ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pNode);

						if (pPacket->iDirEntry != -1)
							++iRelPackets;

						++iPackets;
						pNode = pQueue->pPackets->Next(pNode);
					}
					wprintf (L"    Packets                : %d\n", iPackets);
					wprintf (L"    Reliable packets       : %d\n", iRelPackets);

				}

				if (szBuffer[5] == L'f') {
					_putws (L"Message directory dump:");
					_putws (L"File:");
					for (int i = 0 ; i <= (int)pQueue->sFile->pPacketDir->uiUsedEntries ; ++i) {
						wprintf (L"    [Entry %3d] Offset %08x [%9d] Code %02x [%s]\n", i,
							pQueue->sFile->pPacketDir->sPacketEntry[i].uiOffset,
							pQueue->sFile->pPacketDir->sPacketEntry[i].uiOffset,
							pQueue->sFile->pPacketDir->sPacketEntry[i].uiType,
							(pQueue->sFile->pPacketDir->sPacketEntry[i].uiType == SCFILE_PACKET_EMPTY) ? L"Empty" :
							((pQueue->sFile->pPacketDir->sPacketEntry[i].uiType == SCFILE_PACKET_LIVE) ? L"Live" :
							((pQueue->sFile->pPacketDir->sPacketEntry[i].uiType == SCFILE_PACKET_LAST) ? L"Last" : L"ERROR")));

						if (pQueue->sFile->pPacketDir->sPacketEntry[i].uiType == SCFILE_PACKET_LIVE) {
							static int iBufLen = 0;
							static char *pBuffer = NULL;

							wprintf (L"Packet dump:\n");
							int iLen = pQueue->sFile->pPacketDir->sPacketEntry[i + 1].uiOffset -
								pQueue->sFile->pPacketDir->sPacketEntry[i].uiOffset;
							if (iLen <= 0) {
								wprintf (L"Error: message size 0 or negative - nothing to dump!\n");
								continue;
							}

							if (iBufLen < iLen) {
								char *pBuffer2 = (char *)realloc (pBuffer, iLen);
								if (! pBuffer2)
									wprintf (L"No memory or file corruption!\n");
								else {
									iBufLen = iLen;
									pBuffer = pBuffer2;
								}
							}

							DWORD dwSize = 0;

							if ((0xFFFFFFFF == SetFilePointer (pQueue->sFile->hBackupFile,
								pQueue->sFile->pFileDir->uiPacketBackupOffset +
								pQueue->sFile->pPacketDir->sPacketEntry[i].uiOffset,
								NULL, FILE_BEGIN)) || (! ReadFile (pQueue->sFile->hBackupFile, pBuffer,
								iLen, &dwSize, NULL)) || ((int)dwSize != iLen)) {
								wprintf (L"File read error. Directory or file corrupted\n");
								continue;
							}
							scapi_DumpMem ((unsigned char *)pBuffer, iLen);
						}
					}
					_putws (L"Memory:");
					SVSTNode *pNode = pQueue->pPackets->Min();
					while (pNode) {
						ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pNode);

						wprintf (L"    Packet ptr %08x UID %08x " SC_GUID_FORMAT L":\n", pPacket, pPacket->uiMessageID, SC_GUID_ELEMENTS((&pPacket->guidSourceQM)));
						wprintf (L"        State      : %d [%s]\n", pPacket->uiPacketState,
								(pPacket->uiPacketState == SCPACKET_STATE_ALIVE ? L"Alive" :
						(pPacket->uiPacketState == SCPACKET_STATE_DEAD ? L"Dead" : (pPacket->uiPacketState == SCPACKET_STATE_DEAD ? L"WaitOrderAck" : L"Error"))));
						wprintf (L"        Node       : %08x\n", pPacket->pNode);
						wprintf (L"        T/O Node   : %08x\n", pPacket->pTimeoutNode);
						wprintf (L"        Queue      : %s\n", pPacket->pQueue? pPacket->pQueue->lpszFormatName : L"None");
						wprintf (L"        Image      : %08x\n", pPacket->pImage);
						wprintf (L"        Creation   : %08x\n", pPacket->tCreationTime);
						wprintf (L"        Expiration : %08x\n", pPacket->tExpirationTime);
						wprintf (L"        Ack        : %08x [%s %s %s %s %s]\n", pPacket->uiAckType,
								(pPacket->uiAckType & MQMSG_ACKNOWLEDGMENT_FULL_REACH_QUEUE) ? L"Full Reach" : L"",
								(pPacket->uiAckType & MQMSG_ACKNOWLEDGMENT_FULL_RECEIVE)     ? L"Full Receive" : L"",
								(pPacket->uiAckType & MQMSG_ACKNOWLEDGMENT_NACK_REACH_QUEUE) ? L"Nack Reach" : L"",
								(pPacket->uiAckType & MQMSG_ACKNOWLEDGMENT_NACK_RECEIVE)     ? L"Nack Receive" : L"",
								(pPacket->uiAckType == MQMSG_ACKNOWLEDGMENT_NONE)			 ? L"None" : L"");
						wprintf (L"        Dir Entry  : %d\n", pPacket->iDirEntry);
						if (pPacket->iDirEntry >= 0)
							wprintf (L"        File Index : %d\n", (*pQueue->sFile->pMap)[pPacket->iDirEntry]);
						wprintf (L"        Size       : %d\n", pPacket->pImage->BinarySize());
						wprintf (L"        Order Key  : %08x\n", pPacket->hkOrderKey);
						if (pPacket->pImage) {
							wprintf (L"    Memory dump (bin):\n");
							scapi_DumpMem ((unsigned char *)pPacket->pImage->pvBinary, pPacket->pImage->BinarySize());
							if (pPacket->pImage->pvExt) {
								wprintf (L"    Memory dump (ext):\n");
								scapi_DumpMem ((unsigned char *)pPacket->pImage->pvExt, pPacket->pImage->ExtensionsSize());
							}
						} else
							wprintf (L"    File-bound\n");

						pNode = pQueue->pPackets->Next(pNode);
					}
					wprintf (L"Map:\n        ");
					for (int j = 0 ; j < pQueue->sFile->iMapSize ; ++j) {
						wprintf (L"%d ", (*pQueue->sFile->pMap)[j]);
						if ((j % 8) == 7)
							wprintf (L"\n        ");
					}
					wprintf (L"\n");
				}

				break;
				}
			default:
				_putws (L"Unknown command. Type HELP for help on available commands.");
			}

			gMem->Unlock ();
		} else if (wcsicmp (szBuffer, L"break") == 0)
			DebugBreak();
		else if (wcsicmp (szBuffer, L"help") == 0) {
			_putws (L"The following commands are available:");
			_putws (L"exit                Terminate MSMQ service and exit.");
			_putws (L"break               User break into the debugger (int3 or the like).");
			_putws (L"dump g              Dump global MSMQ information.");
			_putws (L"dump q              Dump queue manager information.");
			_putws (L"dump t              Dump packet time/out information.");
			_putws (L"dump s              Dump session manager information.");
			_putws (L"dump o              Dump overlapped I/O information.");
			_putws (L"dump x              Dump in-order and transactopnal info.");
			_putws (L"dump p              Dump current number of packets in the system");
			_putws (L"dump b <Queue #>    Dump brief queue info.");
			_putws (L"dump f <Queue #>    Dump full queue info (including messages).");
		} else
			_putws (L"Unknown command. Type HELP for help on available commands.");
	}

#if defined (winNT)
	RpcMgmtStopServerListening (NULL);
#else
	scce_Shutdown ();
#endif

#if defined (winCE)
	FreeLibrary (hLib);
#endif

	return 0;
}
#endif

HRESULT scapi_Console (void) {
#if defined (SC_INCLUDE_CONSOLE) && (! defined (MSMQ_ANCIENT_OS))
	DWORD tid;

	HANDLE hThread = CreateThread (NULL, 0, scapi_UserControlThread, NULL, 0, &tid);
	if (hThread) {
		CloseHandle (hThread);
		return MQ_OK;
	}
#endif
	return MQ_ERROR;
}

void scapi_EnterInputLoop (void) {
#if defined (winNT)
	if (RPC_S_OK != RpcServerUseProtseqEp (L"ncalrpc", RPC_C_LISTEN_MAX_CALLS_DEFAULT, L"scmsmq", NULL)) {
		scerror_Complain (MSMQ_SC_ERRMSG_RPCPROTOFAILED);
		return;
	}

	if (RPC_S_OK != RpcServerRegisterIf (scmsmq_v1_0_s_ifspec, NULL, NULL)) {
		scerror_Complain (MSMQ_SC_ERRMSG_RPCSERVREGFAILED);
		return;
	}

	if (RPC_S_OK != RpcServerListen (0, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE)) {
		scerror_Complain (MSMQ_SC_ERRMSG_RPCLISTENFAILED);
		return;
	}
#endif

#if defined (winNT)
	RpcMgmtWaitServerListen();
#else
	scce_Listen ();
#endif
}

#if defined (winNT)
HRESULT	scapi_MQCreateQueue2
(
DWORD			cProp,
PROPID			*aPropID,
PROPVARIANT		*aPropVar,
HRESULT			*aStatus,
DWORD			*pdwStatusLen,

WCHAR			*lpszFormatName,
DWORD			ccBufferSz,
DWORD			*pccNameLen
) {
	SVSUTIL_ASSERT (*pdwStatusLen == 0);

	SCPROPVAR	sQueueProps;

	sQueueProps.cProp    = cProp;
	sQueueProps.aPropID  = aPropID;
	sQueueProps.aPropVar = aPropVar;
	sQueueProps.aStatus  = aStatus;

	*lpszFormatName = L'\0';
	*pccNameLen = ccBufferSz;

	HRESULT hr = scapi_MQCreateQueue (NULL, &sQueueProps, lpszFormatName, pccNameLen);

	if (aStatus)
		*pdwStatusLen = cProp;

	return hr;
}

HRESULT	scapi_MQGetQueueProperties2
(
WCHAR			*lpszFormatName,

DWORD			cProp,
PROPID			*aPropID,
PROPVARIANT		*aPropVar,

HRESULT			*aStatus,
DWORD			*pdwStatusLen
) {
	SVSUTIL_ASSERT (*pdwStatusLen == 0);

	SCPROPVAR	sQueueProps;

	sQueueProps.cProp    = cProp;
	sQueueProps.aPropID  = aPropID;
	sQueueProps.aPropVar = aPropVar;
	sQueueProps.aStatus  = aStatus;

	HRESULT hr = scapi_MQGetQueueProperties (lpszFormatName, &sQueueProps);

	if (aStatus)
		*pdwStatusLen = cProp;

	return hr;
}


HRESULT	scapi_MQSetQueueProperties2
(
WCHAR			*lpszFormatName,

DWORD			cProp,
PROPID			*aPropID,
PROPVARIANT		*aPropVar,

HRESULT			*aStatus,
DWORD			*pdwStatusLen
) {
	SVSUTIL_ASSERT (*pdwStatusLen == 0);

	SCPROPVAR	sQueueProps;

	sQueueProps.cProp    = cProp;
	sQueueProps.aPropID  = aPropID;
	sQueueProps.aPropVar = aPropVar;
	sQueueProps.aStatus  = aStatus;

	HRESULT hr = scapi_MQSetQueueProperties (lpszFormatName, &sQueueProps);

	if (aStatus)
		*pdwStatusLen = cProp;

	return hr;
}


HRESULT scapi_MQHandleToFormatName2
(
SCHANDLE		hQueue,
WCHAR			*lpszFormatName,
DWORD			ccBufferSz,
DWORD			*pccNameLen
) {
	*pccNameLen = ccBufferSz;
	*lpszFormatName = L'\0';

	return scapi_MQHandleToFormatName (hQueue, lpszFormatName, pccNameLen);
}

HRESULT scapi_MQPathNameToFormatName2
(
WCHAR			*lpszPathName,
WCHAR			*lpszFormatName,
DWORD			ccBufferSz,
DWORD			*pccNameLen
) {
	*pccNameLen = ccBufferSz;
	*lpszFormatName = L'\0';

	return scapi_MQPathNameToFormatName (lpszPathName, lpszFormatName, pccNameLen);
}

HRESULT scapi_MQSendMessage2
(
SCHANDLE		hQueue,

DWORD			cProp,
PROPID			*aPropID,
PROPVARIANT		*aPropVar,

HRESULT			*aStatus,
DWORD			*pdwStatusLen,

int				iTransaction,

OBJECTID		*pObjectID
) {
	SVSUTIL_ASSERT (*pdwStatusLen == 0);

	SCPROPVAR	sMsgProps;

	sMsgProps.cProp    = cProp;
	sMsgProps.aPropID  = aPropID;
	sMsgProps.aPropVar = aPropVar;
	sMsgProps.aStatus  = aStatus;

	HRESULT hr = scapi_MQSendMessage (hQueue, &sMsgProps, iTransaction);

	if (aStatus)
		*pdwStatusLen = cProp;

	if (! FAILED(hr)) {
		for (int i = 0 ; i < (int)cProp ; ++i) {
			if (aPropID[i] == PROPID_M_MSGID) {
				*pObjectID = *(OBJECTID *)aPropVar[i].caub.pElems;
				break;
			}
		}
	}

	return hr;
}


HRESULT scapi_MQReceiveMessage2
(
DWORD			cProp,
PROPID			*aPropID,
PROPVARIANT		*aPropVar,

HRESULT			*aStatus,
DWORD			*pdwStatusLen,

RECVPROPHELPER	*prph
) {
	SVSUTIL_ASSERT (*pdwStatusLen == 0);

	SCPROPVAR	sMsgProps;

	sMsgProps.cProp    = cProp;
	sMsgProps.aPropID  = aPropID;
	sMsgProps.aPropVar = aPropVar;
	sMsgProps.aStatus  = aStatus;

	if (prph->iAdminQueueNdx >= 0) {
		prph->lpszAdminQueue = (WCHAR *)midl_user_allocate (prph->ccAdminQueue * sizeof(WCHAR));

		*prph->lpszAdminQueue = L'\0';

		aPropVar[prph->iAdminQueueNdx].vt      = VT_LPWSTR;
		aPropVar[prph->iAdminQueueNdx].pwszVal = prph->lpszAdminQueue;
	}

	if (prph->iDestQueueNdx >= 0) {
		prph->lpszDestQueue = (WCHAR *)midl_user_allocate (prph->ccDestQueue * sizeof(WCHAR));

		*prph->lpszDestQueue = L'\0';

		aPropVar[prph->iDestQueueNdx].vt      = VT_LPWSTR;
		aPropVar[prph->iDestQueueNdx].pwszVal = prph->lpszDestQueue;
	}

	if (prph->iLabelNdx >= 0) {
		prph->lpszLabel = (WCHAR *)midl_user_allocate (prph->ccLabel * sizeof(WCHAR));

		*prph->lpszLabel = L'\0';

		aPropVar[prph->iLabelNdx].vt      = VT_LPWSTR;
		aPropVar[prph->iLabelNdx].pwszVal = prph->lpszLabel;
	}

	if (prph->iRespQueueNdx >= 0) {
		prph->lpszRespQueue = (WCHAR *)midl_user_allocate (prph->ccRespQueue * sizeof(WCHAR));

		*prph->lpszRespQueue = L'\0';

		aPropVar[prph->iRespQueueNdx].vt      = VT_LPWSTR;
		aPropVar[prph->iRespQueueNdx].pwszVal = prph->lpszRespQueue;
	}

	if (prph->iBodyNdx >= 0) {
		prph->pBody = (BYTE *)midl_user_allocate (prph->ccBody);

		aPropVar[prph->iBodyNdx].vt          = VT_VECTOR | VT_UI1;
		aPropVar[prph->iBodyNdx].caub.cElems = prph->ccBody;
		aPropVar[prph->iBodyNdx].caub.pElems = prph->pBody;
	}

	if (prph->iExtNdx >= 0) {
		prph->pExt = (BYTE *)midl_user_allocate (prph->ccExt);

		aPropVar[prph->iExtNdx].vt		   = VT_VECTOR | VT_UI1;
		aPropVar[prph->iExtNdx].caub.cElems = prph->ccExt;
		aPropVar[prph->iExtNdx].caub.pElems = prph->pExt;
	}

	HRESULT hRes = scapi_MQReceiveMessage (prph->hQueue, prph->dwTimeout, prph->dwAction, &sMsgProps, 0, 0, prph->hCursor, NULL);

	if (prph->iAdminQueueNdx >= 0) {
		aPropVar[prph->iAdminQueueNdx].vt      = VT_NULL;
		aPropVar[prph->iAdminQueueNdx].pwszVal = NULL;
	}

	if (prph->iDestQueueNdx >= 0) {
		aPropVar[prph->iDestQueueNdx].vt      = VT_NULL;
		aPropVar[prph->iDestQueueNdx].pwszVal = NULL;
	}

	if (prph->iLabelNdx >= 0) {
		aPropVar[prph->iLabelNdx].vt      = VT_NULL;
		aPropVar[prph->iLabelNdx].pwszVal = NULL;
	}

	if (prph->iRespQueueNdx >= 0) {
		aPropVar[prph->iRespQueueNdx].vt      = VT_NULL;
		aPropVar[prph->iRespQueueNdx].pwszVal = NULL;
	}

	if (prph->iBodyNdx >= 0) {
		aPropVar[prph->iBodyNdx].vt          = VT_NULL;
		aPropVar[prph->iBodyNdx].caub.cElems = 0;
		aPropVar[prph->iBodyNdx].caub.pElems = NULL;
	}

	if (prph->iExtNdx >= 0) {
		aPropVar[prph->iExtNdx].vt		    = VT_NULL;
		aPropVar[prph->iExtNdx].caub.cElems = 0;
		aPropVar[prph->iExtNdx].caub.pElems = NULL;
	}

	return hRes;
}

HRESULT scapi_MQGetMachineProperties2
(
WCHAR			*lpszMachineName,
GUID			*pguidMachineID,

DWORD			cProp,
PROPID			*aPropID,
PROPVARIANT		*aPropVar,

HRESULT			*aStatus,
DWORD			*pdwStatusLen
) {
	SVSUTIL_ASSERT (*pdwStatusLen == 0);

	SCPROPVAR	sMachineProps;

	sMachineProps.cProp    = cProp;
	sMachineProps.aPropID  = aPropID;
	sMachineProps.aPropVar = aPropVar;
	sMachineProps.aStatus  = aStatus;

	HRESULT hr = scapi_MQGetMachineProperties (lpszMachineName, pguidMachineID, &sMachineProps);

	if (aStatus)
		*pdwStatusLen = cProp;

	return hr;
}

void __RPC_FAR * __RPC_API midl_user_allocate(size_t cBytes) 
{ 
    return g_funcAlloc (cBytes, g_pvAllocData); 
} 

void __RPC_API midl_user_free(void __RPC_FAR * p) 
{ 
    g_funcFree (p, g_pvFreeData); 
} 

#endif
