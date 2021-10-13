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

    scoverio.hxx

Abstract:

    Small client overlapped I/O support


--*/
#if ! defined (__scoverio_HXX__)
#define __scoverio_HXX__	1

#include <svsutil.hxx>
#include <mq.h>

#define SC_OVER_THREAD_TIMEOUT	5000

class ScIoRequest : public SVSAllocClass {
private:
	HANDLE				hEvent;
	HANDLE				hCallerProc;

	CMarshalOverlapped  *pOverlapped;
	DWORD				*pdwStatus;

	SCHANDLE			hQueue;
	SCHANDLE			hCursor;
	DWORD				tExpirationTicks;
	unsigned int 		tExpirationS;
	unsigned int 		tExpirationMS;
	DWORD				dwTimeout;
	DWORD				dwAction;
	ce::copy_in<SCPROPVAR*> *pMsgProps;

	SVSTNode			*pNode;
	ScIoRequest			*pNext;
	int					iNdx;

	ScIoRequest		(HANDLE				a_hWaitEvent,
					 HANDLE				a_hCallerProc,
					 DWORD				*a_pdwStatus,
					 CMarshalOverlapped *a_pOverlapped,
					 SCHANDLE			a_hQueue,
					 SCHANDLE			a_hCursor,
					 DWORD				a_dwTimeout,
					 DWORD				a_dwAction,
					 ce::copy_in<SCPROPVAR*> *a_pMsgProps) {
		hEvent       = a_hWaitEvent;
		hCallerProc  = a_hCallerProc;
		pdwStatus    = a_pdwStatus;

		pOverlapped  = a_pOverlapped;

		hQueue       = a_hQueue;
		hCursor      = a_hCursor;
		dwTimeout    = a_dwTimeout;

		if (dwTimeout == INFINITE) {
			tExpirationTicks = tExpirationS = 0xffffffff;
			tExpirationMS = 999;
		} else {
			svsutil_GetAbsTime (&tExpirationS, &tExpirationMS);
			tExpirationMS += dwTimeout;
			tExpirationS  += tExpirationMS / 1000;
			tExpirationMS %= 1000;

			tExpirationTicks = GetTickCount () + a_dwTimeout;
		}

		dwAction               = a_dwAction;
		pMsgProps              = a_pMsgProps;

		pNode = NULL;
		pNext = NULL;
		iNdx  = -1;
	}

	// Set status code and event to alert application that request is over.
	void SetStatusAndEvent(HRESULT hr) {
		// Either we should be using overlapped structure from app OR params
		// specified by MSMQ PSL thread itself (exclusive OR)
		SVSUTIL_ASSERT((pOverlapped && (NULL==pdwStatus) && (NULL==hEvent)) ||
		               (!pOverlapped && pdwStatus && hEvent));

		if (pdwStatus)
			*pdwStatus = hr;

		if (hEvent)
			SetEvent(hEvent);

		if (pOverlapped) {
			pOverlapped->SetOverlappedEvent();
			pOverlapped->SetStatus(hr);
			delete pOverlapped;
			pOverlapped = NULL;
			// We delete pMsgProps only if pOverlapped was set, because
			// this indicates that scapi_MQReceiveMessage has returned and
			// is relying on the worker thread here to take care of deallocation.
			delete pMsgProps;
			pMsgProps = NULL;
		}
    }

	friend class ScOverlappedSupport;
	friend static HRESULT scapi_MQReceiveMessageI   (ScIoRequest *pReq);
	friend static ScIoRequest *scapi_ReverseReqList (ScIoRequest *pReqList);
	friend DWORD WINAPI scapi_UserControlThread     (LPVOID lpParameter);
};

class ScOverlappedSupport : public SVSAllocClass {
private:
	HANDLE		hThread;
	SVSTree		*pTimeTree;

	int			fInitialized;

	int				iNumPendingRequests;
	HANDLE			ahQueueEvents[MAXIMUM_WAIT_OBJECTS];
	HANDLE			*pahQueueEvents;
	ScIoRequest		*apPendingRequests[MAXIMUM_WAIT_OBJECTS - 1];

	void SpinThread (void) {
		SVSUTIL_ASSERT (iNumPendingRequests > 0);

		if (! hThread) {
			DWORD dwTID;
			hThread = CreateThread (NULL, 0, OverlappedSupportThread_s, this, 0, &dwTID);
		}

		if (hThread)
			SetEvent (ahQueueEvents[0]);
	}

	void CompressIndex (int iNdx);
	void Deregister (ScIoRequest *pRequest, HRESULT hr);
	void OverlappedSupportThread (void);
	void SelfDestruct (void);

	static DWORD WINAPI OverlappedSupportThread_s (void *pvParam);

public:
	int		fBusy;

	ScOverlappedSupport (void) {
		iNumPendingRequests = 0;

		pahQueueEvents   = &ahQueueEvents[1];
		hThread          = NULL;
		ahQueueEvents[0] = CreateEvent (NULL, FALSE, FALSE, NULL);
		pTimeTree        = SVSNewTree(gMem->pTreeNodeMem);
		fInitialized     = pTimeTree && ahQueueEvents[0];
		fBusy            = 0;
	}


	~ScOverlappedSupport (void) {
		SelfDestruct ();
	}

	HRESULT EnterOverlappedReceive(
		HANDLE				hQueueEvent,
		HANDLE				hWaitEvent,
		HANDLE				hCallerProc,
		DWORD				*pdwStatus,
		CMarshalOverlapped	*pOverlapped,
		SCHANDLE			hQueue,
		SCHANDLE			hCursor,
		DWORD				dwTimeout,
		DWORD				dwAction,
		ce::copy_in<SCPROPVAR*> *pMsgProps);

	HRESULT EnterOverlappedReceive(
		HANDLE				hQueueEvent,
		ScIoRequest			*pIO);

	void CloseHandle (SCHANDLE h);

	friend DWORD WINAPI scapi_UserControlThread (LPVOID lpParameter);
};

#endif
