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
#include "..\server\scapi.h"

#if defined (SDK_BUILD)
#include <pstub.h>
#endif

#include <svsutil.hxx>

#define IMPLICIT_CALL PRIV_IMPLICIT_CALL

#define MACRO_MQCreateQueue IMPLICIT_DECL(HRESULT, iApiSetId, 2,		\
							(PSECURITY_DESCRIPTOR pSecurityDescriptor,	\
						    MQQUEUEPROPS *pQueueProps,					\
						    LPWSTR lpszFormatName,						\
						    LPDWORD pdwNameLen))

#define MACRO_MQDeleteQueue	IMPLICIT_DECL(HRESULT, iApiSetId, 3,		\
							(LPCWSTR lpszFormatName))

#define MACRO_MQGetQueueProperties IMPLICIT_DECL(HRESULT, iApiSetId, 4,	\
							(LPCWSTR lpszFormatName,						\
							MQQUEUEPROPS *pQueueProps))

#define MACRO_MQSetQueueProperties IMPLICIT_DECL(HRESULT, iApiSetId, 5,	\
							(LPCWSTR lpszFormatName,						\
							MQQUEUEPROPS *pQueueProps))

#define MACRO_MQOpenQueue IMPLICIT_DECL(HRESULT, iApiSetId, 6, 			\
							(LPCWSTR lpszFormatName,					\
							DWORD dwAccess,								\
							DWORD dwShareMode,							\
							QUEUEHANDLE *phQueue))

#define MACRO_MQCloseQueue IMPLICIT_DECL(HRESULT, iApiSetId, 7, (QUEUEHANDLE hQueue))

#define MACRO_MQCreateCursor IMPLICIT_DECL(HRESULT, iApiSetId, 8,		\
							(QUEUEHANDLE hQueue, HANDLE *phCursor))

#define MACRO_MQCloseCursor IMPLICIT_DECL(HRESULT, iApiSetId, 9, (HANDLE hCursor))


#define MACRO_MQHandleToFormatName IMPLICIT_DECL(HRESULT, iApiSetId, 10, \
							(QUEUEHANDLE hQueue,						 \
							WCHAR *lpszFormatName,						 \
							DWORD *pdwNameLen))

#define MACRO_MQPathNameToFormatName IMPLICIT_DECL(HRESULT, iApiSetId, 11,\
							(LPCWSTR lpszPathName,						 \
							WCHAR *lpszFormatName,						 \
							DWORD *pdwCount))

#define MACRO_MQSendMessage IMPLICIT_DECL(HRESULT, iApiSetId, 12,        \
							(QUEUEHANDLE hQueue,						 \
							MQMSGPROPS *pMsgProps,						 \
							void *pvTransact))


#define MACRO_MQReceiveMessage IMPLICIT_DECL(HRESULT, iApiSetId, 13,	 \
							(QUEUEHANDLE hQueue,						 \
						    DWORD dwTimeout,							 \
						    DWORD dwAction,								 \
						    MQMSGPROPS *pMsgProps,						 \
						    LPOVERLAPPED lpOverlapped,					 \
						    PMQRECEIVECALLBACK fnReceiveCallback,		 \
						    HANDLE hCursor,								 \
						    ITransaction *pTransaction))

#define MACRO_MQGetMachineProperties IMPLICIT_DECL(HRESULT, iApiSetId, 14,\
							(LPCWSTR, GUID *, MQQMPROPS *))

#define MACRO_MQBeginTransaction IMPLICIT_DECL(HRESULT, iApiSetId, 15,\
							(ITransaction **pTransact))

#define MACRO_MQGetQueueSecurity IMPLICIT_DECL(HRESULT, iApiSetId, 16,\
							(LPCWSTR lpwcsFormatName, SECURITY_INFORMATION RequestedInformation, \
    						PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, \
    						LPDWORD lpnLengthNeeded))

#define MACRO_MQSetQueueSecurity IMPLICIT_DECL(HRESULT, iApiSetId, 17,\
							(LPCWSTR lpwcsFormatName, SECURITY_INFORMATION SecurityInformation, \
    						PSECURITY_DESCRIPTOR pSecurityDescriptor))

#define MACRO_MQGetSecurityContext IMPLICIT_DECL(HRESULT, iApiSetId, 18,\
							(VOID *lpCertBuffer, DWORD dwCertBufferLength, HANDLE *hSecurityContext))


#define MACRO_MQFreeSecurityContext IMPLICIT_DECL(HRESULT, iApiSetId, 19, (HANDLE h))


#define MACRO_MQInstanceToFormatName IMPLICIT_DECL(HRESULT, iApiSetId, 20,\
							(GUID  *pGUID, WCHAR *lpszFormatName, DWORD *lpdwCount))

#define MACRO_MQLocateBegin IMPLICIT_DECL(HRESULT, iApiSetId, 21,\
							(LPCWSTR lpwcsContext, MQRESTRICTION* pRestriction, \
							MQCOLUMNSET* pColumns, MQSORTSET* pSort, PHANDLE phEnum))

#define MACRO_MQLocateNext IMPLICIT_DECL(HRESULT, iApiSetId, 22,\
							(HANDLE hEnum, DWORD* pcProps, MQPROPVARIANT aPropVar[]))

#define MACRO_MQLocateEnd IMPLICIT_DECL(HRESULT, iApiSetId, 23,\
							(HANDLE hEnum))


#define MACRO_MQMgmtGetInfo2 IMPLICIT_DECL(HRESULT, iApiSetId, 24, \
							(LPCWSTR pMachineName, LPCWSTR pObjectName, DWORD cp, \
							PROPID aPropID[], PROPVARIANT aPropVar[]))

#define MACRO_MQMgmtAction IMPLICIT_DECL(HRESULT, iApiSetId, 25, \
							(LPCWSTR pMachineName, LPCWSTR pObjectName, LPCWSTR pAction))



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

static int		fMSMQInit = FALSE;
static int		iApiSetId = -1;
static HANDLE	hDevice   = NULL;

extern "C" int QueryAPISetID(char*);


static int MQInitClientAPI (void) {
    iApiSetId = QueryAPISetID ("MSMQ");

	if (iApiSetId == -1) {
		//
		//	Try device driver...
		//
		hDevice = CreateFile (L"MMQ1:", 0, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (hDevice == INVALID_HANDLE_VALUE)
			return FALSE;
	}

	fMSMQInit = TRUE;
	return TRUE;
}

HRESULT APIENTRY MQCreateQueue(
    PSECURITY_DESCRIPTOR pSecurityDescriptor,
    MQQUEUEPROPS *pQueueProps,
    LPWSTR lpszFormatName,
    LPDWORD pdwNameLen
    ) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (pSecurityDescriptor != NULL)
		return MQ_ERROR_ILLEGAL_SECURITY_DESCRIPTOR;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQCreateQueue, pQueueProps, 0, lpszFormatName, 0, pdwNameLen, NULL);

	return MACRO_MQCreateQueue (0, pQueueProps, lpszFormatName, pdwNameLen);
}

HRESULT	APIENTRY MQDeleteQueue (LPCWSTR lpszFormatName) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQDeleteQueue, (void *)lpszFormatName, 0, NULL, 0, NULL, NULL);

	return MACRO_MQDeleteQueue (lpszFormatName);
}

HRESULT	APIENTRY MQGetMachineProperties (LPCWSTR lpszMachineName, const GUID *pGuid, MQQMPROPS *pMachineProps) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQGetMachineProperties, (void *)lpszMachineName, 0, (void *)pGuid, 0, (DWORD *)pMachineProps, NULL);

	return MACRO_MQGetMachineProperties (lpszMachineName, (GUID *)pGuid, pMachineProps);
}

HRESULT	APIENTRY MQGetQueueProperties (LPCWSTR lpszFormatName, MQQUEUEPROPS *pQueueProps) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQGetQueueProperties, (void *)lpszFormatName, 0, pQueueProps, 0, NULL, NULL);

	return MACRO_MQGetQueueProperties (lpszFormatName, pQueueProps);
}

HRESULT APIENTRY MQSetQueueProperties (LPCWSTR lpszFormatName, MQQUEUEPROPS *pQueueProps) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQSetQueueProperties, (void *)lpszFormatName, 0, pQueueProps, 0, NULL, NULL);

	return MACRO_MQSetQueueProperties (lpszFormatName, pQueueProps);
}

HRESULT	APIENTRY MQOpenQueue (LPCWSTR lpszFormatName, DWORD dwAccess, DWORD dwShareMode, QUEUEHANDLE *phQueue) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQOpenQueue, (void *)lpszFormatName, dwAccess, phQueue, dwShareMode, NULL, NULL);

	return MACRO_MQOpenQueue (lpszFormatName, dwAccess, dwShareMode, phQueue);
}

HRESULT APIENTRY MQCloseQueue (QUEUEHANDLE hQueue) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQCloseQueue, NULL, (DWORD)hQueue, NULL, 0, NULL, NULL);

	return MACRO_MQCloseQueue (hQueue);
}

HRESULT APIENTRY MQCreateCursor (QUEUEHANDLE hQueue, HANDLE *phCursor) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQCreateCursor, phCursor, (DWORD)hQueue, NULL, 0, NULL, NULL);


	return MACRO_MQCreateCursor (hQueue, phCursor);
}

HRESULT APIENTRY MQCloseCursor (HANDLE hCursor) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQCloseCursor, NULL, (DWORD)hCursor, NULL, 0, NULL, NULL);

	return MACRO_MQCloseCursor (hCursor);
}

HRESULT APIENTRY MQHandleToFormatName (QUEUEHANDLE hQueue, WCHAR *lpszFormatName, DWORD *pdwNameLen) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQHandleToFormatName, lpszFormatName, (DWORD)hQueue, pdwNameLen, 0, NULL, NULL);

	return MACRO_MQHandleToFormatName (hQueue, lpszFormatName, pdwNameLen);
}

HRESULT APIENTRY MQPathNameToFormatName (LPCWSTR lpszPathName, WCHAR *lpszFormatName, DWORD *pdwCount) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQPathNameToFormatName, (void *)lpszPathName, 0, lpszFormatName, 0, pdwCount, NULL);

	return MACRO_MQPathNameToFormatName (lpszPathName, lpszFormatName, pdwCount);
}

void APIENTRY MQFreeMemory (void *pvPtr) {
	LocalFree (pvPtr);
}

HRESULT APIENTRY MQSendMessage (QUEUEHANDLE hQueue, MQMSGPROPS *pMsgProps, void *pvTransact) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQSendMessage, pMsgProps, (DWORD)hQueue, NULL, (DWORD)pvTransact, NULL, NULL);

	return MACRO_MQSendMessage (hQueue, pMsgProps, pvTransact);
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
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;


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

	if (hDevice) {
		DataMQReceiveMessage dm;
		dm.hQueue		= (SCHANDLE)hQueue;
		dm.dwTimeout	= dwTimeout;
		dm.dwAction		= dwAction;
		dm.pMsgProps	= (SCPROPVAR *)pMsgProps;
		dm.lpOverlapped = lpOverlapped;
		dm.pfnReceiveCallback = NULL;
		dm.hCursor      = (SCHANDLE)hCursor;
		dm.iNull3       = (int)pTransaction;

		return DeviceIoControl (hDevice, MQAPI_CODE_MQReceiveMessage, &dm, 0, NULL, 0, NULL, NULL);
	}

	return MACRO_MQReceiveMessage (hQueue, dwTimeout, dwAction, pMsgProps, lpOverlapped, fnReceiveCallback, hCursor, 0);
}

HRESULT APIENTRY MQBeginTransaction (ITransaction **pTransact) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQBeginTransaction, pTransact, 0, NULL, 0, NULL, NULL);

    return MACRO_MQBeginTransaction (pTransact);
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
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQGetQueueSecurity, (void *)lpwcsFormatName, RequestedInformation, pSecurityDescriptor, nLength, lpnLengthNeeded, NULL);

    return MACRO_MQGetQueueSecurity ((WCHAR *)lpwcsFormatName, RequestedInformation, pSecurityDescriptor, nLength, lpnLengthNeeded);
}

HRESULT
APIENTRY
MQSetQueueSecurity(
    LPCWSTR lpwcsFormatName,
    SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR pSecurityDescriptor
    ) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQSetQueueSecurity, (void *)lpwcsFormatName, SecurityInformation, pSecurityDescriptor, 0, NULL, NULL);

	return MACRO_MQSetQueueSecurity (lpwcsFormatName, SecurityInformation, pSecurityDescriptor);
}

HRESULT APIENTRY MQGetSecurityContext
(
VOID   *lpCertBuffer,
DWORD  dwCertBufferLength,
HANDLE *hSecurityContext
) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQGetSecurityContext, lpCertBuffer, dwCertBufferLength, hSecurityContext, 0, NULL, NULL);

	return MACRO_MQGetSecurityContext (lpCertBuffer, dwCertBufferLength, hSecurityContext);
}

void APIENTRY MQFreeSecurityContext (HANDLE h) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return;

	if (hDevice) {
		DeviceIoControl (hDevice, MQAPI_CODE_MQFreeSecurityContext, NULL, (DWORD)h, NULL, 0, NULL, NULL);
		return;
	}

	MACRO_MQFreeSecurityContext (h);
}

HRESULT APIENTRY MQInstanceToFormatName
(
GUID  *pGUID,
WCHAR *lpszFormatName,
DWORD *lpdwCount
) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQInstanceToFormatName, pGUID, 0, lpszFormatName, 0, lpdwCount, NULL);

    return MACRO_MQInstanceToFormatName (pGUID, lpszFormatName, lpdwCount);
}

HRESULT APIENTRY MQLocateBegin(LPCWSTR lpwcsContext, MQRESTRICTION* pRestriction,
							MQCOLUMNSET* pColumns, MQSORTSET* pSort, PHANDLE phEnum) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQLocateBegin, (void *)lpwcsContext, (DWORD)pSort, pRestriction, (DWORD)phEnum, (DWORD *)pColumns, NULL);

	return MACRO_MQLocateBegin (lpwcsContext, pRestriction, pColumns, pSort, phEnum);
}

HRESULT APIENTRY MQLocateNext(HANDLE hEnum, DWORD* pcProps, MQPROPVARIANT aPropVar[]) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQLocateNext, pcProps, (DWORD)hEnum, aPropVar, 0, NULL, NULL);

	return MACRO_MQLocateNext (hEnum, pcProps, aPropVar);
}

HRESULT APIENTRY MQLocateEnd (HANDLE hEnum) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQLocateEnd, NULL, (DWORD)hEnum, NULL, 0, NULL, NULL);

    return MACRO_MQLocateEnd(hEnum);
}

HRESULT APIENTRY MQMgmtGetInfo(LPCWSTR pMachineName, LPCWSTR pObjectName, MQMGMTPROPS *pMgmtProps) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	memset (pMgmtProps->aPropVar, 0, sizeof(pMgmtProps->aPropVar[0]) * pMgmtProps->cProp);

	for (int i = 0 ; i < (int)pMgmtProps->cProp ; ++i)
		pMgmtProps->aPropVar[i].vt = VT_NULL;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQMgmtGetInfo2, (void *)pMachineName, (DWORD)pMgmtProps->cProp, (void *)pObjectName, (DWORD)pMgmtProps->aPropVar, (DWORD *)pMgmtProps->aPropID, NULL);

	return MACRO_MQMgmtGetInfo2 (pMachineName, pObjectName, pMgmtProps->cProp, pMgmtProps->aPropID, pMgmtProps->aPropVar);
}


HRESULT APIENTRY MQMgmtAction(LPCWSTR pMachineName, LPCWSTR pObjectName, LPCWSTR pAction) {
    if ((! fMSMQInit) && (! MQInitClientAPI()))
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (hDevice)
		return DeviceIoControl (hDevice, MQAPI_CODE_MQMgmtAction, (void *)pMachineName, 0, (void *)pObjectName, 0, (DWORD *)pAction, NULL);

	return MACRO_MQMgmtAction (pMachineName, pObjectName, pAction);
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
			// we're shutting down due to app dying.  No notification in this case.
			goto done;
		}
		PerformCallbackMSMQ(pCallback,ov.Internal); // MSMQ puts status in Internal param
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

			g_fRunning = TRUE;
		break;

		case DLL_PROCESS_DETACH:
			g_fRunning = FALSE;

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


