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

    scapi.h

Abstract:

    Small client - Windows CE nonport part/export library


--*/

#include <windows.h>

#include <objbase.h>
#include <objidl.h>
#include <wtypes.h>

#include <mq.h>
#include <mqmgmt.h>
#include "..\server\expapis.hxx"

#include <svsutil.hxx>
#include <psl_marshaler.hxx>

ce::psl_proxy<>* pProxy;

//
//  Callback information.
//

// Information blob for each perform callback call.
typedef struct _MSMQ_CALLBACK_INFO {
	PMQRECEIVECALLBACK pfnMQReceiveCallback; // function to call
	HANDLE hEvent; // Handle that MSMQ Core wakes us up on.

	// Paramaters passed to pfnMQReceiveCallback.  None of these paramaters should be
	// freed -- according to the spec, the calling application continues to own this memory.
	QUEUEHANDLE hSource;
	DWORD dwTimeout;
	DWORD dwAction;
	MQMSGPROPS* pMessageProps;
	LPOVERLAPPED lpOverlapped; // NOTE: This is not passed to MSMQ core, but just to application callback.
	HANDLE hCursor;
	ITransaction *pTransaction;
} MSMQ_CALLBACK_INFO, *PMSMQ_CALLBACK_INFO;

DWORD WINAPI PerformCallbackWorker(LPVOID lpv);

// List of MSMQ_CALLBACK_INFO structures
SVSLinkManager *g_pCallbackList;
// Thread pool of PerformCallbackWorker()'s.
SVSThreadPool *g_pThreadPool;
// Lock class.
SVSSynch *g_pSync;
// Are we running or no?  Used for shutdown processing.
BOOL g_fRunning;


//
//	Global private data
//

static HANDLE	hDevice   = INVALID_HANDLE_VALUE;

extern "C" int QueryAPISetID(char*);

HRESULT APIENTRY MQCreateQueue(
    PSECURITY_DESCRIPTOR pSecurityDescriptor,
    MQQUEUEPROPS *pQueueProps,
    LPWSTR lpszFormatName,
    LPDWORD pdwNameLen
    ) 
{
	if (pSecurityDescriptor)
		return MQ_ERROR_ILLEGAL_SECURITY_DESCRIPTOR;

	if (pdwNameLen == NULL)
		MQ_ERROR_INVALID_PARAMETER;

	return pProxy->call(MQAPI_CODE_MQCreateQueue,pQueueProps,
	                    ce::psl_buffer(lpszFormatName,*pdwNameLen),pdwNameLen);
}

HRESULT	APIENTRY MQDeleteQueue (LPCWSTR lpszFormatName) {
	return pProxy->call(MQAPI_CODE_MQDeleteQueue,lpszFormatName);
}

HRESULT	APIENTRY MQGetMachineProperties (LPCWSTR lpszMachineName, const GUID *pGuid, MQQMPROPS *pMachineProps) {
	return pProxy->call(MQAPI_CODE_MQGetMachineProperties,lpszMachineName,pGuid,pMachineProps);
}

HRESULT	APIENTRY MQGetQueueProperties (LPCWSTR lpszFormatName, MQQUEUEPROPS *pQueueProps) {
	return pProxy->call(MQAPI_CODE_MQGetQueueProperties,lpszFormatName,pQueueProps);
}

HRESULT APIENTRY MQSetQueueProperties (LPCWSTR lpszFormatName, MQQUEUEPROPS *pQueueProps) {
	return pProxy->call(MQAPI_CODE_MQSetQueueProperties,lpszFormatName,pQueueProps);
}

HRESULT	APIENTRY MQOpenQueue (LPCWSTR lpszFormatName, DWORD dwAccess, DWORD dwShareMode, QUEUEHANDLE *phQueue) {
	return pProxy->call(MQAPI_CODE_MQOpenQueue,lpszFormatName,dwAccess,dwShareMode,phQueue);
}

HRESULT APIENTRY MQCloseQueue (QUEUEHANDLE hQueue) {
	return pProxy->call(MQAPI_CODE_MQCloseQueue,hQueue);
}

HRESULT APIENTRY MQCreateCursor (QUEUEHANDLE hQueue, HANDLE *phCursor) {
	return pProxy->call(MQAPI_CODE_MQCreateCursor,hQueue,phCursor);
}

HRESULT APIENTRY MQCloseCursor (HANDLE hCursor) {
	return pProxy->call(MQAPI_CODE_MQCloseCursor,hCursor);
}

HRESULT APIENTRY MQHandleToFormatName (QUEUEHANDLE hQueue, WCHAR *lpszFormatName, DWORD *pdwNameLen) {
	if (pdwNameLen == NULL)
		return MQ_ERROR_INVALID_PARAMETER;

	return pProxy->call(MQAPI_CODE_MQHandleToFormatName,hQueue,
	                    ce::psl_buffer(lpszFormatName,*pdwNameLen),pdwNameLen);
}

HRESULT APIENTRY MQPathNameToFormatName (LPCWSTR lpszPathName, WCHAR *lpszFormatName, DWORD *pdwCount) {
	if (pdwCount == NULL)
		return MQ_ERROR_INVALID_PARAMETER;

	return pProxy->call(MQAPI_CODE_MQPathNameToFormatName,lpszPathName,
	                    ce::psl_buffer(lpszFormatName,*pdwCount),pdwCount);
}

void APIENTRY MQFreeMemory (void *pvPtr) {
	pProxy->call(MQAPI_CODE_MQFreeMemory,pvPtr);
}

HRESULT APIENTRY MQSendMessage (QUEUEHANDLE hQueue, MQMSGPROPS *pMsgProps, void *pvTransact) {
	return pProxy->call(MQAPI_CODE_MQSendMessage,hQueue,pMsgProps,(int)pvTransact);
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

	if (fnReceiveCallback) {
		// If app passes in a callback function, in the past we used to call
		// PerformCallback() from MSMQ core in services.exe.  However PerformCallback()
		// is an unsafe function.  The calling app, possibly untrusted, could muck with 
		// services.exe call stack.

		// To get the same behavior for application in safe manner, we spin up 
		// a thread in application space, have that thread call into MQReceiveMessage 
		// with an event that will be signaled once MSMQ has something to report, 
		// then finally call into the app's callback once this has occurred.
		HANDLE hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
		if (NULL==hEvent)
			return MQ_ERROR_INSUFFICIENT_RESOURCES;

		g_pSync->Lock();
		PMSMQ_CALLBACK_INFO pCallback = (PMSMQ_CALLBACK_INFO) g_pCallbackList->AllocEntry();
		if (!pCallback) {
			g_pSync->Unlock();
			CloseHandle(hEvent);
			return MQ_ERROR_INSUFFICIENT_RESOURCES;
		}

		pCallback->hEvent               = hEvent;
		pCallback->pfnMQReceiveCallback = fnReceiveCallback;
		pCallback->hSource              = hQueue;
		pCallback->dwTimeout            = dwTimeout;
		pCallback->dwAction             = dwAction;
		pCallback->pMessageProps        = pMsgProps;
		pCallback->lpOverlapped         = lpOverlapped;
		pCallback->hCursor              = hCursor;
		pCallback->pTransaction         = pTransaction;

		if (! g_pThreadPool->ScheduleEvent(PerformCallbackWorker,pCallback)) {
			g_pCallbackList->RemoveEntry(pCallback); // This automatically closes hEvent via DeleteCallbackInfo().
			g_pSync->Unlock();
			return MQ_ERROR_INSUFFICIENT_RESOURCES;
		}

		g_pSync->Unlock();
		return MQ_INFORMATION_OPERATION_PENDING;
	}

	return pProxy->call(MQAPI_CODE_MQReceiveMessage,hQueue,dwTimeout,dwAction,pMsgProps,
	                    lpOverlapped,hCursor,(int)pTransaction);
}

HRESULT APIENTRY MQBeginTransaction (ITransaction **pTransact) {
	// Not implemented - don't bother calling into services.exe
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT
APIENTRY
MQGetQueueSecurity(
    LPCWSTR lpwcsFormatName,
    SECURITY_INFORMATION RequestedInformation,
    PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD nLength,
    LPDWORD lpnLengthNeeded
) {
	// Not implemented - don't bother calling into servicesd.exe
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT
APIENTRY
MQSetQueueSecurity(
    LPCWSTR lpwcsFormatName,
    SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR pSecurityDescriptor
) {
	// Not implemented - don't bother calling into servicesd.exe
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT APIENTRY MQGetSecurityContext
(
VOID   *lpCertBuffer,
DWORD  dwCertBufferLength,
HANDLE *hSecurityContext
) {
	// Not implemented - don't bother calling into servicesd.exe
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

void APIENTRY MQFreeSecurityContext (HANDLE h) {
	// Not implemented - don't bother calling into servicesd.exe
	return; 
}

HRESULT APIENTRY MQInstanceToFormatName
(
GUID  *pGUID,
WCHAR *lpszFormatName,
DWORD *lpdwCount
) {
	// Not implemented - don't bother calling into servicesd.exe
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT APIENTRY MQLocateBegin(LPCWSTR lpwcsContext, MQRESTRICTION* pRestriction,
							MQCOLUMNSET* pColumns, MQSORTSET* pSort, PHANDLE phEnum) {
	// Not implemented - don't bother calling into servicesd.exe
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT APIENTRY MQLocateNext(HANDLE hEnum, DWORD* pcProps, MQPROPVARIANT aPropVar[]) {
	// Not implemented - don't bother calling into servicesd.exe
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT APIENTRY MQLocateEnd (HANDLE hEnum) {
	// Not implemented - don't bother calling into servicesd.exe
	return MQ_ERROR_SERVICE_NOT_AVAILABLE;
}

HRESULT APIENTRY MQMgmtGetInfo(LPCWSTR pMachineName, LPCWSTR pObjectName, MQMGMTPROPS *pMgmtProps) {
	memset (pMgmtProps->aPropVar, 0, sizeof(pMgmtProps->aPropVar[0]) * pMgmtProps->cProp);

	for (int i = 0 ; i < (int)pMgmtProps->cProp ; ++i)
		pMgmtProps->aPropVar[i].vt = VT_NULL;

	return pProxy->call(MQAPI_CODE_MQMgmtGetInfo2,pMachineName,pObjectName,pMgmtProps);
}


HRESULT APIENTRY MQMgmtAction(LPCWSTR pMachineName, LPCWSTR pObjectName, LPCWSTR pAction) {
	return pProxy->call(MQAPI_CODE_MQMgmtAction,pMachineName,pObjectName,pAction); 
}


//
// Code related to making callbacks work on MQReceiveMessage()
//

void PerformCallbackMSMQ(MSMQ_CALLBACK_INFO *pCallback, HRESULT hr) {
	__try {
		pCallback->pfnMQReceiveCallback(hr, pCallback->hSource, pCallback->dwTimeout,
		                                pCallback->dwAction,pCallback->pMessageProps,
		                                pCallback->lpOverlapped,pCallback->hCursor);
	}
	__except (1) {
		;
	}
}

// Worker thread for actually doing the callback itself. 
DWORD WINAPI PerformCallbackWorker(LPVOID lpv) {
	PMSMQ_CALLBACK_INFO pCallback;
	g_pSync->Lock();

	// Got removed from list before we could get started
	pCallback = (PMSMQ_CALLBACK_INFO)g_pCallbackList->FindEntry(lpv);
	if (NULL == pCallback) {
		SVSUTIL_ASSERT(0); // shouldn't be able to happen
		return 0;
	}

	g_pSync->Unlock();

	OVERLAPPED ov;
	memset(&ov,0,sizeof(ov));
	ov.hEvent   = pCallback->hEvent;
	ov.Internal = MQ_ERROR;

	HRESULT hr = MQReceiveMessage(pCallback->hSource,pCallback->dwTimeout,pCallback->dwAction,
	                              pCallback->pMessageProps,&ov,NULL,pCallback->hCursor,
	                              pCallback->pTransaction);

	if (hr != MQ_INFORMATION_OPERATION_PENDING) {
		// There is already a message waiting to be received or there was an error.
		PerformCallbackMSMQ(pCallback,hr);
		goto done;
	}

	hr = WaitForSingleObject(pCallback->hEvent,INFINITE);

	if (hr == WAIT_OBJECT_0) {
		if (! g_fRunning) {
			// The application is dying (g_fRunning is only set = FALSE in 
			// DllMain DLL_PROCESS_DETACH processing).  In this case,
			// we want to exit thread ASAP so do not call into callback.
			goto done;
		}

		// MSMQ puts status in Internal param
		PerformCallbackMSMQ(pCallback,ov.Internal); 
	}
	// If there was some other error, do not perform callback...

done:
	CloseHandle(pCallback->hEvent);

	g_pSync->Lock();
	// Remove ourselves from the list.
	g_pCallbackList->RemoveEntry(pCallback);
	g_pSync->Unlock();
	return 0;
}

void WINAPI DeleteCallbackInfo(void *pvData) {
	PMSMQ_CALLBACK_INFO pCallback = (PMSMQ_CALLBACK_INFO) pvData;
	if (pCallback->hEvent)
		CloseHandle(pCallback->hEvent);
}

extern "C" BOOL WINAPI DllMain(HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			svsutil_Initialize();
			DisableThreadLibraryCalls((HMODULE)hInstDll);
			g_pSync = new SVSSynch();
			if (NULL == g_pSync)
				return FALSE;

			g_pCallbackList = new SVSLinkManager(sizeof(MSMQ_CALLBACK_INFO),DeleteCallbackInfo);
			if (NULL == g_pCallbackList)
				return FALSE;

			g_pThreadPool = new SVSThreadPool();
			if (NULL == g_pThreadPool)
				return FALSE;

			pProxy = new ce::psl_proxy<>(L"MMQ1:", IOCTL_MSMQ_INVOKE, NULL);
			if (NULL == pProxy)
				return FALSE;

			g_fRunning = TRUE;
		break;

		case DLL_PROCESS_DETACH:
			g_fRunning = FALSE;

			if (pProxy)
				delete pProxy;

			if (g_pThreadPool)
				delete g_pThreadPool;

			if (g_pCallbackList)
				delete g_pCallbackList;

			if (g_pSync)
				delete g_pSync;

			svsutil_DeInitialize();
		break;
	}

	return TRUE;
}


