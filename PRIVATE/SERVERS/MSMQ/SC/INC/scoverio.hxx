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

	PMQRECEIVECALLBACK	pfnReceiveCallback;
	OVERLAPPED			*lpOverlapped;
	DWORD				*pdwStatus;

	DWORD				dwProcPerm;

	SCHANDLE			hQueue;
	SCHANDLE			hCursor;
	DWORD				tExpirationTicks;
	unsigned int 		tExpirationS;
	unsigned int 		tExpirationMS;
	DWORD				dwTimeout;
	DWORD				dwAction;
	SCPROPVAR			*pMsgProps;

	SVSTNode			*pNode;
	ScIoRequest			*pNext;
	int					iNdx;

	ScIoRequest		(HANDLE				a_hWaitEvent,
					 HANDLE				a_hCallerProc,
					 DWORD				a_dwProcPerm,
					 DWORD				*a_pdwStatus,
					 PMQRECEIVECALLBACK	a_pfnReceiveCallback,
					 OVERLAPPED			*a_lpOverlapped,
					 SCHANDLE			a_hQueue,
					 SCHANDLE			a_hCursor,
					 DWORD				a_dwTimeout,
					 DWORD				a_dwAction,
					 SCPROPVAR			*a_pMsgProps) {
		hEvent       = a_hWaitEvent;
		hCallerProc  = a_hCallerProc;
		dwProcPerm   = a_dwProcPerm;
		pdwStatus    = a_pdwStatus;

		pfnReceiveCallback  = a_pfnReceiveCallback;
		lpOverlapped        = a_lpOverlapped;

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

		dwAction     = a_dwAction;
		pMsgProps    = a_pMsgProps;

		pNode = NULL;
		pNext = NULL;
		iNdx  = -1;
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
		pTimeTree        = new SVSTree (gMem->pTreeNodeMem);
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
		DWORD				dwProcPerm,
		DWORD				*pdwStatus,
		PMQRECEIVECALLBACK	pfnReceiveCallback,
		OVERLAPPED			*lpOverlapped,
		SCHANDLE			hQueue,
		SCHANDLE			hCursor,
		DWORD				dwTimeout,
		DWORD				dwAction,
		SCPROPVAR			*pMsgProps);

	HRESULT EnterOverlappedReceive(
		HANDLE				hQueueEvent,
		ScIoRequest			*pIO);

	void CloseHandle (SCHANDLE h);

	friend DWORD WINAPI scapi_UserControlThread (LPVOID lpParameter);
};

#endif
