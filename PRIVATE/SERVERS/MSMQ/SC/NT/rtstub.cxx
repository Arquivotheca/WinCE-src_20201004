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

    rtstub.cxx

Abstract:

    IPC stubs for NT


--*/

#include "scapi.h"
#include <mq.h>
#include "..\msmqinc\mqmgmt.h"

//static int sg_fInitializedTimes = 0;
static void *sg_pvBindingString = NULL;

static void *APIENTRY MQInitClientAPI (void) {	
	if (sg_pvBindingString)
		return sg_pvBindingString;

    WCHAR *pszUuid             = NULL;
    WCHAR *pszProtocolSequence = L"ncalrpc";
    WCHAR *pszNetworkAddress   = NULL;
    WCHAR *pszEndpoint         = L"scmsmq";
    WCHAR *pszOptions          = NULL;
    WCHAR *pszStringBinding    = NULL;
 
    RPC_STATUS status = RpcStringBindingCompose(pszUuid,
                                     pszProtocolSequence,
                                     pszNetworkAddress,
                                     pszEndpoint,
                                     pszOptions,
                                     &pszStringBinding);
    if (status)
		return NULL;

    status = RpcBindingFromStringBinding(pszStringBinding,
                                         &scmsmq_IfHandle);
 
    if (status)
        return NULL;

	sg_pvBindingString = (void *)pszStringBinding;

	return sg_pvBindingString;
}

//
// HRESULT APIENTRY MQFreeClientAPI (void *pvArg) {
//	if (pvArg != sg_pvBindingString)
//		return MQ_ERROR;
//
//	WCHAR *pszStringBinding = (WCHAR *)pvArg;
//
//    RPC_STATUS status = RpcStringFree(&pszStringBinding); 
// 
//    if (status)
//		return MQ_ERROR;
// 
//    status = RpcBindingFree(&scmsmq_IfHandle);
// 
//    if (status)
//		return MQ_ERROR;
//
//	sg_pvBindingString = NULL;
//
//	return MQ_OK;
//}
//
//
//	Standard API set
//
HRESULT APIENTRY MQCreateQueue(
    PSECURITY_DESCRIPTOR pSecurityDescriptor,
    MQQUEUEPROPS *pQueueProps,
    LPWSTR lpszFormatName,
    LPDWORD dwNameLen
    ) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		DWORD dwStatusLen = 0;
		return scapi_MQCreateQueue2 (pQueueProps->cProp, pQueueProps->aPropID,
					pQueueProps->aPropVar, pQueueProps->aStatus, &dwStatusLen,
					lpszFormatName, *dwNameLen, dwNameLen);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

HRESULT	APIENTRY MQDeleteQueue (LPCWSTR lpszFormatName) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		return scapi_MQDeleteQueue ((WCHAR *)lpszFormatName);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

HRESULT APIENTRY MQGetMachineProperties (LPCWSTR lpszMachineName, const GUID *pguidMachineID, MQQMPROPS *pQMProps) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		DWORD dwStatusLen = 0;
		return scapi_MQGetMachineProperties2 ((WCHAR *)lpszMachineName, (GUID *)pguidMachineID, pQMProps->cProp,
						pQMProps->aPropID, pQMProps->aPropVar, pQMProps->aStatus, &dwStatusLen);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

HRESULT	APIENTRY MQGetQueueProperties (LPCWSTR lpszFormatName, MQQUEUEPROPS *pQueueProps) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		DWORD dwStatusLen = 0;
		return scapi_MQGetQueueProperties2 ((WCHAR *)lpszFormatName, pQueueProps->cProp, pQueueProps->aPropID,
			pQueueProps->aPropVar, pQueueProps->aStatus, &dwStatusLen);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

HRESULT APIENTRY MQSetQueueProperties (LPCWSTR lpszFormatName, MQQUEUEPROPS *pQueueProps) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		DWORD dwStatusLen = 0;
		return scapi_MQSetQueueProperties2 ((WCHAR *)lpszFormatName, pQueueProps->cProp, pQueueProps->aPropID, pQueueProps->aPropVar,
			pQueueProps->aStatus, &dwStatusLen);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

HRESULT	APIENTRY MQOpenQueue (LPCWSTR lpszFormatName, DWORD dwAccess, DWORD dwShareMode, QUEUEHANDLE *phQueue) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		return scapi_MQOpenQueue ((WCHAR *)lpszFormatName, dwAccess, dwShareMode, (SCHANDLE *)phQueue);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

HRESULT APIENTRY MQCloseQueue (QUEUEHANDLE hQueue) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		return scapi_MQCloseQueue ((SCHANDLE)hQueue);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

HRESULT APIENTRY MQCreateCursor (QUEUEHANDLE hQueue, HANDLE *phCursor) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		return scapi_MQCreateCursor ((SCHANDLE)hQueue, (SCHANDLE *)phCursor);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

HRESULT APIENTRY MQCloseCursor (HANDLE hCursor) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		return scapi_MQCloseCursor ((SCHANDLE)hCursor);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

HRESULT APIENTRY MQHandleToFormatName (QUEUEHANDLE hQueue, WCHAR *lpszFormatName, DWORD *dwNameLen) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		return scapi_MQHandleToFormatName2 ((SCHANDLE)hQueue, lpszFormatName, *dwNameLen, dwNameLen);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

HRESULT APIENTRY MQPathNameToFormatName (LPCWSTR lpszPathName, WCHAR *lpszFormatName, DWORD *pdwCount) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		return scapi_MQPathNameToFormatName2 ((WCHAR *)lpszPathName, lpszFormatName, *pdwCount, pdwCount);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

void APIENTRY MQFreeMemory (void *pvPtr) {
	midl_user_free (pvPtr);
}

HRESULT APIENTRY MQSendMessage (QUEUEHANDLE hQueue, MQMSGPROPS *pMsgProps, void *pvTrans) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		DWORD dwStatusLen = 0;
		OBJECTID oiMsgID;
		HRESULT hr = scapi_MQSendMessage2 ((SCHANDLE)hQueue, pMsgProps->cProp, pMsgProps->aPropID, pMsgProps->aPropVar,
			pMsgProps->aStatus, &dwStatusLen, (int)pvTrans, &oiMsgID);

		if (! FAILED(hr)) {
			for (int i = 0 ; i < (int)pMsgProps->cProp ; ++i) {
				if (pMsgProps->aPropID[i] == PROPID_M_MSGID) {
					*(OBJECTID *)pMsgProps->aPropVar[i].caub.pElems = oiMsgID;
					break;
				}
			}
		}
		return hr;
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

HRESULT APIENTRY MQReceiveMessage(
    QUEUEHANDLE			hQueue,
    DWORD				dwTimeout,
    DWORD				dwAction,
    MQMSGPROPS			*pMsgProps,
    LPOVERLAPPED		lpOverlapped,
    PMQRECEIVECALLBACK	fnReceiveCallback,
    HANDLE				hCursor,
    ITransaction		*pTransaction
    ) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (! pMsgProps)
		return MQ_ERROR_INVALID_PARAMETER;

	RECVPROPHELPER rph;

	memset (&rph, 0, sizeof(rph));

	rph.hQueue         = (SCHANDLE)hQueue;
	rph.hCursor        = (SCHANDLE)hCursor;
	rph.dwTimeout      = dwTimeout;
	rph.dwAction       = dwAction;

	rph.iAdminQueueNdx = -1;
	rph.iDestQueueNdx  = -1;
	rph.iLabelNdx      = -1;
	rph.iRespQueueNdx  = -1;
	rph.iBodyNdx       = -1;
	rph.iExtNdx        = -1;

	WCHAR *lpszAdminQueue = NULL;
	WCHAR *lpszDestQueue  = NULL;
	WCHAR *lpszLabel      = NULL;
	WCHAR *lpszRespQueue  = NULL;

	BYTE *pBody = NULL;
	BYTE *pExt  = NULL;

	HRESULT hRes = MQ_OK;

	if (pMsgProps->aStatus && pMsgProps->cProp > 0)
		memset (pMsgProps->aStatus, 0, sizeof (HRESULT) * pMsgProps->cProp);

	for (int i = 0 ; i < (int)pMsgProps->cProp ; ++i) {
		switch (pMsgProps->aPropID[i]) {
		case PROPID_M_ACKNOWLEDGE:
		case PROPID_M_APPSPECIFIC:
		case PROPID_M_BODY_SIZE:
		case PROPID_M_BODY_TYPE:
		case PROPID_M_CLASS:
		case PROPID_M_CORRELATIONID:
		case PROPID_M_DELIVERY:
		case PROPID_M_EXTENSION_LEN:
		case PROPID_M_JOURNAL:
		case PROPID_M_PRIORITY:
		case PROPID_M_TIME_TO_BE_RECEIVED:
		case PROPID_M_TIME_TO_REACH_QUEUE:
		case PROPID_M_TRACE:
		case PROPID_M_VERSION:
		case PROPID_M_MSGID:
		case PROPID_M_ARRIVEDTIME:
		case PROPID_M_SENTTIME:
		case PROPID_M_SRC_MACHINE_ID:
		case PROPID_M_PRIV_LEVEL:
			break;

		case PROPID_M_ADMIN_QUEUE:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal)) {
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VT;
				break;
			}

			rph.iAdminQueueNdx             = i;
			lpszAdminQueue                 = pMsgProps->aPropVar[i].pwszVal;
			pMsgProps->aPropVar[i].pwszVal = NULL;
			pMsgProps->aPropVar[i].vt      = VT_NULL;
			break;

		case PROPID_M_ADMIN_QUEUE_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4) {
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
				break;
			}

			rph.ccAdminQueue = pMsgProps->aPropVar[i].ulVal;
			break;

		case PROPID_M_BODY:
			if ((pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_UI1)) ||
				(pMsgProps->aPropVar[i].caub.cElems <= 0) ||
				(! pMsgProps->aPropVar[i].caub.pElems)) {
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
				break;
			}

			rph.iBodyNdx = i;
			rph.ccBody   = pMsgProps->aPropVar[i].caub.cElems;
			pBody        = pMsgProps->aPropVar[i].caub.pElems;

			pMsgProps->aPropVar[i].vt = VT_NULL;
			pMsgProps->aPropVar[i].caub.cElems = 0;
			pMsgProps->aPropVar[i].caub.pElems = NULL;
			break;

		case PROPID_M_DEST_QUEUE:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) ||
								(! pMsgProps->aPropVar[i].pwszVal)) {
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
				break;
			}

			rph.iDestQueueNdx = i;
			lpszDestQueue = pMsgProps->aPropVar[i].pwszVal;

			pMsgProps->aPropVar[i].pwszVal = NULL;
			pMsgProps->aPropVar[i].vt      = VT_NULL;
			break;

		case PROPID_M_DEST_QUEUE_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4) {
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
				break;
			}

			rph.ccDestQueue = pMsgProps->aPropVar[i].ulVal;
			break;

		case PROPID_M_EXTENSION:
			if ((pMsgProps->aPropVar[i].vt != (VT_VECTOR | VT_UI1)) ||
				(pMsgProps->aPropVar[i].caub.cElems <= 0) ||
				(! pMsgProps->aPropVar[i].caub.pElems)) {
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
				break;
			}

			rph.iExtNdx = i;
			rph.ccExt   = pMsgProps->aPropVar[i].caub.cElems;
			pExt        = pMsgProps->aPropVar[i].caub.pElems;

			pMsgProps->aPropVar[i].vt = VT_NULL;
			pMsgProps->aPropVar[i].caub.cElems = 0;
			pMsgProps->aPropVar[i].caub.pElems = NULL;
			break;

		case PROPID_M_LABEL:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) || (! pMsgProps->aPropVar[i].pwszVal)) {
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
				break;
			}

			rph.iLabelNdx = i;
			lpszLabel = pMsgProps->aPropVar[i].pwszVal;

			pMsgProps->aPropVar[i].pwszVal = NULL;
			pMsgProps->aPropVar[i].vt      = VT_NULL;
			break;

		case PROPID_M_LABEL_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4) {
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
				break;
			}

			rph.ccLabel = pMsgProps->aPropVar[i].ulVal;
			break;

		case PROPID_M_RESP_QUEUE:
			if ((pMsgProps->aPropVar[i].vt != VT_LPWSTR) ||
							(! pMsgProps->aPropVar[i].pwszVal)) {
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
				break;
			}

			rph.iRespQueueNdx = i;
			lpszRespQueue     = pMsgProps->aPropVar[i].pwszVal;

			pMsgProps->aPropVar[i].pwszVal = NULL;
			pMsgProps->aPropVar[i].vt      = VT_NULL;
			break;

		case PROPID_M_RESP_QUEUE_LEN:
			if (pMsgProps->aPropVar[i].vt != VT_UI4) {
				hRes = MQ_ERROR_ILLEGAL_PROPERTY_VALUE;
				break;
			}

			rph.ccRespQueue = pMsgProps->aPropVar[i].ulVal;
			break;

		case PROPID_M_XACT_STATUS_QUEUE:
		case PROPID_M_XACT_STATUS_QUEUE_LEN:
		case PROPID_M_CONNECTOR_TYPE:
			hRes = MQ_ERROR_PROPERTY_NOTALLOWED;
			break;

		case PROPID_M_AUTH_LEVEL:
		case PROPID_M_SIGNATURE:
		case PROPID_M_SIGNATURE_LEN:
		case PROPID_M_SENDERID_LEN:
		case PROPID_M_SENDERID_TYPE:
		case PROPID_M_SENDERID:
		case PROPID_M_SENDER_CERT_LEN:
		case PROPID_M_SENDER_CERT:
		case PROPID_M_PROV_NAME_LEN:
		case PROPID_M_PROV_NAME:
		case PROPID_M_DEST_SYMM_KEY:
		case PROPID_M_DEST_SYMM_KEY_LEN:
		case PROPID_M_AUTHENTICATED:
		case PROPID_M_HASH_ALG:
		case PROPID_M_ENCRYPTION_ALG:
		case PROPID_M_PROV_TYPE:
		case PROPID_M_SECURITY_CONTEXT:
			hRes = MQ_ERROR_COMPUTER_DOES_NOT_SUPPORT_ENCRYPTION;
			break;

		default:
			hRes = MQ_ERROR_PROPERTY;
		}

		if (pMsgProps->aStatus)
			pMsgProps->aStatus[i] = hRes;
	}

	if (rph.iAdminQueueNdx < 0)
		rph.ccAdminQueue    = EMPTY_ALLOC_SIZE;
	else if (! rph.ccAdminQueue)
		hRes = MQ_ERROR_INVALID_PARAMETER;

	if (rph.iDestQueueNdx < 0)
		rph.ccDestQueue     = EMPTY_ALLOC_SIZE;
	else if (! rph.ccDestQueue)
		hRes = MQ_ERROR_INVALID_PARAMETER;

	if (rph.iLabelNdx < 0)
		rph.ccLabel         = EMPTY_ALLOC_SIZE;
	else if (! rph.ccLabel)
		hRes = MQ_ERROR_INVALID_PARAMETER;

	if (rph.iRespQueueNdx < 0)
		rph.ccRespQueue     = EMPTY_ALLOC_SIZE;
	else if (! rph.ccRespQueue)
		hRes = MQ_ERROR_INVALID_PARAMETER;

	if (rph.iBodyNdx < 0)
		rph.ccBody			= EMPTY_ALLOC_SIZE;
	else if (! rph.ccBody)
		hRes = MQ_ERROR_INVALID_PARAMETER;

	if (rph.iExtNdx < 0)
		rph.ccExt			= EMPTY_ALLOC_SIZE;
	else if (! rph.ccExt)
		hRes = MQ_ERROR_INVALID_PARAMETER;

	if (! FAILED (hRes)) {
		int iBodyLen = 0;
		int iExtLen  = 0;

		RpcTryExcept {
			DWORD dwStatusLen = 0;

			hRes = scapi_MQReceiveMessage2 (pMsgProps->cProp, pMsgProps->aPropID, pMsgProps->aPropVar,
				pMsgProps->aStatus, &dwStatusLen, &rph);
		} RpcExcept (TRUE) {
			if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
				hRes = MQ_ERROR_INVALID_PARAMETER;
			else
				hRes = MQ_ERROR_SERVICE_NOT_AVAILABLE;
		}
		RpcEndExcept;
	}

	//
	//	Restore
	//
	if ((rph.iAdminQueueNdx >= 0) && rph.lpszAdminQueue) {
		memcpy (lpszAdminQueue, rph.lpszAdminQueue, rph.ccAdminQueue * sizeof(WCHAR));
		midl_user_free (rph.lpszAdminQueue);

		pMsgProps->aPropVar[rph.iAdminQueueNdx].vt      = VT_LPWSTR;
		pMsgProps->aPropVar[rph.iAdminQueueNdx].pwszVal = lpszAdminQueue;
	}

	if ((rph.iBodyNdx >= 0) && rph.pBody) {
		memcpy (pBody, rph.pBody, rph.ccBody);
		midl_user_free (rph.pBody);

		pMsgProps->aPropVar[rph.iBodyNdx].vt          = VT_VECTOR | VT_UI1;
		pMsgProps->aPropVar[rph.iBodyNdx].caub.cElems = rph.ccBody;
		pMsgProps->aPropVar[rph.iBodyNdx].caub.pElems = pBody;
	}

	if ((rph.iDestQueueNdx >= 0) && rph.lpszDestQueue) {
		memcpy (lpszDestQueue, rph.lpszDestQueue, rph.ccDestQueue * sizeof(WCHAR));
		midl_user_free (rph.lpszDestQueue);

		pMsgProps->aPropVar[rph.iDestQueueNdx].vt      = VT_LPWSTR;
		pMsgProps->aPropVar[rph.iDestQueueNdx].pwszVal = lpszDestQueue;
	}

	if ((rph.iExtNdx >= 0) && rph.pExt) {
		memcpy (pExt, rph.pExt, rph.ccBody);
		midl_user_free (rph.pExt);

		pMsgProps->aPropVar[rph.iExtNdx].vt		   = VT_VECTOR | VT_UI1;
		pMsgProps->aPropVar[rph.iExtNdx].caub.cElems = rph.ccExt;
		pMsgProps->aPropVar[rph.iExtNdx].caub.pElems = pExt;
	}

	if ((rph.iLabelNdx >= 0) && rph.lpszLabel) {
		memcpy (lpszLabel, rph.lpszLabel, rph.ccLabel * sizeof(WCHAR));
		midl_user_free (rph.lpszLabel);

		pMsgProps->aPropVar[rph.iLabelNdx].vt      = VT_LPWSTR;
		pMsgProps->aPropVar[rph.iLabelNdx].pwszVal = lpszLabel;
	}

	if ((rph.iRespQueueNdx >= 0) && rph. lpszRespQueue) {
		memcpy (lpszRespQueue, rph.lpszRespQueue, rph.ccRespQueue * sizeof(WCHAR));
		midl_user_free (rph.lpszRespQueue);

		pMsgProps->aPropVar[rph.iRespQueueNdx].vt      = VT_LPWSTR;
		pMsgProps->aPropVar[rph.iRespQueueNdx].pwszVal = lpszRespQueue;
	}

	return hRes;
}

//
//	Stubbed unimplemented functions
//
HRESULT APIENTRY MQBeginTransaction (ITransaction **ppTransaction) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

    return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT APIENTRY MQGetQueueSecurity
(
LPCWSTR					lpszFormatName,
SECURITY_INFORMATION	SecurityInformation,
PSECURITY_DESCRIPTOR	pSecurityDescriptor,
DWORD					nLength,
DWORD					*dwlengthNeeded
) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

    return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT APIENTRY MQSetQueueSecurity
(
LPCWSTR					lpszFormatName,
SECURITY_INFORMATION	SecurityInformation,
PSECURITY_DESCRIPTOR	pSecurityDescriptor
) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

    return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT APIENTRY MQGetSecurityContext
(
VOID   *lpCertBuffer,
DWORD  dwCertBufferLength,
HANDLE *hSecurityContext
) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

    *hSecurityContext = NULL;
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

void APIENTRY MQFreeSecurityContext (HANDLE hUnused) {
}

HRESULT APIENTRY MQInstanceToFormatName
(
GUID  *pGUID,
WCHAR *lpszFormatName,
DWORD *lpdwCount
) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

    *lpdwCount = 0;
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT APIENTRY MQLocateBegin
(
LPCWSTR			lpszContext,
MQRESTRICTION	*pRestriction,
MQCOLUMNSET		*pColumns,
MQSORTSET		*pSort,
HANDLE			*hEnum
) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

    *hEnum = NULL;
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT APIENTRY MQLocateNext
(
HANDLE 		hEnum,
DWORD  		*pcProps,
PROPVARIANT	aPropVar[]
) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

    *pcProps = 0;
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT APIENTRY MQLocateEnd
(
HANDLE			hEnum
) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT APIENTRY MQMgmtGetInfo(LPCWSTR pMachineName, LPCWSTR pObjectName, MQMGMTPROPS *pMgmtProps) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	memset (pMgmtProps->aPropVar, 0, sizeof(pMgmtProps->aPropVar[0]) * pMgmtProps->cProp);

	for (int i = 0 ; i < (int)pMgmtProps->cProp ; ++i)
		pMgmtProps->aPropVar[i].vt = VT_NULL;

	RpcTryExcept {
		return scmgmt_MQMgmtGetInfo2 (pMachineName, pObjectName, pMgmtProps->cProp, pMgmtProps->aPropID, pMgmtProps->aPropVar);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}


HRESULT APIENTRY MQMgmtAction(LPCWSTR pMachineName, LPCWSTR pObjectName, LPCWSTR pAction) {
    if (! MQInitClientAPI ())
    	return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	RpcTryExcept {
		return scmgmt_MQMgmtAction (pMachineName, pObjectName, pAction);
	} RpcExcept (TRUE) {
		if (RpcExceptionCode() == RPC_X_NULL_REF_POINTER)
			return MQ_ERROR_INVALID_PARAMETER;
		else
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
	}
	RpcEndExcept;
}

void __RPC_FAR * __RPC_API midl_user_allocate(size_t cBytes) { 
    return malloc(cBytes);
}

void __RPC_API midl_user_free(void __RPC_FAR * p) {
	free(p);
}


