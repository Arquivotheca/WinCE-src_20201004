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

    scsmgr.cxx

Abstract:

    Small client session manager

--*/

#include <sc.hxx>
#include <scqman.hxx>
#include <scqueue.hxx>
#include <scutil.hxx>
#include <scsman.hxx>
#include <scpacket.hxx>
#include <scnb.hxx>

#include <scping.hxx>

#include <sccomp.hxx>

#include <ph.h>
#include <phintr.h>
#include <_mqini.h>

#define SC_THREAD_WAIT				 5000		// 5 seconds
#define SC_THREAD_WAIT2				 500

HANDLE ScSessionManager::hNetUP = NULL;

ScSessionManager::ScSessionManager (void) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_INIT, L"Creating Session Manager...\n");
#endif

	fBusy = 0;

	fInitialized = FALSE;
	pSessList    = NULL;
	s_listen     = INVALID_SOCKET;
	iThreadPairs = 0;

	hAccThread = NULL;

	hBuzz = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (! hBuzz)
		return;

	if (gMachine->fNetworkTracking) {
		hNetUP = CreateEvent (NULL, FALSE, FALSE, NULL);
		if (! hNetUP)
			return;
	}

	if (! gMachine->fUseBinary) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Binary protocol disabled - NOT creating binary ports...\n");
#endif
		fInitialized = TRUE;
		return;
	}

	s_listen = socket (AF_INET, SOCK_STREAM, 0);

	if (s_listen == INVALID_SOCKET)
		return;

	sockaddr_in sa;
	memset (&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port   = htons (gMachine->uiPort);
	sa.sin_addr.S_un.S_addr = htonl (INADDR_ANY);

	if (bind (s_listen, (sockaddr *)&sa, sizeof(sa))) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"port binding error: %08x\n", WSAGetLastError ());
#endif
 		return;
	}

	if (listen (s_listen, 5)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"listen error: %08x\n", WSAGetLastError ());
#endif
		return;
	}

	fInitialized = TRUE;
}

void ScSessionManager::Start(void) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	if (! gMachine->fUseBinary) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Binary protocol disabled - NOT starting binary listener...\n");
#endif
		return;
	}

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Starting listener thread...\n");
#endif

	StartPingServer ();

	DWORD tid;
	hAccThread  = CreateThread (NULL, 0, ScSessionManager::AccThread_s,  NULL, 0, &tid);
}

void ScSessionManager::Stop(void) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	if (gMachine->fUseBinary)
		StopPingServer ();

	//
	//	First, close all sockets
	//
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Wrapping up communications...\n");
#endif

	if (s_listen != INVALID_SOCKET) {
		closesocket (s_listen);
		s_listen = INVALID_SOCKET;
	}

	ScSession *pList = pSessList;
	while (pList) {
		pList->fSessionState = SCSESSION_STATE_EXITING;		// Signal reader
		SetEvent (pList->hEvent);							// ...and writer

		if (pList->s != INVALID_SOCKET) {
			closesocket (pList->s);
			pList->s = INVALID_SOCKET;
			pList->ipPeerAddr.S_un.S_addr = INADDR_NONE;
		}

		// if we're using wininet, close connection from under it.
		if (pList->hInternetSession)
			pList->SRMPCloseSession();

		pList = pList->pNext;
	}

	//
	//	Now kill all threads
	//
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Stopping listener thread...\n");
#endif
	if (hAccThread) {
		if (WaitForSingleObject (hAccThread, SC_THREAD_WAIT) == WAIT_TIMEOUT) {
			scerror_Inform (MSMQ_SC_ERRMSG_LISTENKILLED);
			TerminateThread (hAccThread, 0);
		}

		CloseHandle (hAccThread);
	}

	hAccThread = NULL;

	//
	//	Stop all session threads...
	//
	pList = pSessList;
	while (pList) {
		if (pList->hServiceThreadR) {
#if defined (SC_VERBOSE)
		    scerror_DebugOut (VERBOSE_MASK_SESSION, L"Terminating receiver for %s...\n", pList->lpszHostName);
#endif
			if (WaitForSingleObject (pList->hServiceThreadR, SC_THREAD_WAIT) == WAIT_TIMEOUT) {
				scerror_Inform (MSMQ_SC_ERRMSG_READERKILLED, pList->lpszHostName);
				TerminateThread (pList->hServiceThreadR, 0);
			}

			CloseHandle (pList->hServiceThreadR);
		}
		pList->hServiceThreadR = NULL;

		if (pList->hServiceThreadW) {
#if defined (SC_VERBOSE)
		    scerror_DebugOut (VERBOSE_MASK_SESSION, L"Terminating sender for %s...\n", pList->lpszHostName);
#endif
			if (WaitForSingleObject (pList->hServiceThreadW, SC_THREAD_WAIT) == WAIT_TIMEOUT) {
				scerror_Inform (MSMQ_SC_ERRMSG_WRITERKILLED, pList->lpszHostName);
				TerminateThread (pList->hServiceThreadW, 0);
			}

			CloseHandle (pList->hServiceThreadW);
		}
		pList->hServiceThreadW = NULL;
		pList = pList->pNext;
	}
}

ScSessionManager::~ScSessionManager (void) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_INIT, L"Freeing Session Manager...\n");
#endif

	SVSUTIL_ASSERT (! hAccThread);

	if (hBuzz)
		CloseHandle (hBuzz);

	if (hNetUP)
		CloseHandle (hNetUP);

	hNetUP = NULL;

	ScSession *pList = pSessList;

	while (pList) {
		ScSession *pNext = pList->pNext;

		SVSUTIL_ASSERT (! pList->hServiceThreadW);
		SVSUTIL_ASSERT (! pList->hServiceThreadR);

		svsutil_StringHashFree (gMem->pStringHash, pList->lpszHostName);
		pList->lpszHostName = NULL;

		delete pList;

		pList = pNext;
	}

	if (s_listen != INVALID_SOCKET)
		closesocket (s_listen);
}

ScSession *ScSessionManager::GetSession (WCHAR *a_lpszHostName, int qType) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Acquiring session for session for %s... ", a_lpszHostName);
#endif

	svsutil_StringHashRef (gMem->pStringHash, a_lpszHostName);

	ScSession *pSess = pSessList;
	while (pSess) {
		if (pSess->lpszHostName == a_lpszHostName && qType == pSess->qType) {
			pSess->AddRef ();
			svsutil_StringHashFree (gMem->pStringHash, a_lpszHostName);		// compensate the ref

#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Failed...\n");
#endif
			return pSess;
		}
		pSess = pSess->pNext;
	}

	pSess = new ScSession (a_lpszHostName, pSessList, qType);
	if (pSess) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Created...\n");
#endif
		pSessList = pSess;
	} else {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Failed...\n");
#endif
		SVSUTIL_ASSERT (0);

		svsutil_StringHashFree (gMem->pStringHash, a_lpszHostName);		// compensate the ref
	}

	return pSess;
}

ScSession *ScSessionManager::GetInactiveOsSession (WCHAR *a_lpszHostName) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Creating new (duplicate check override) session for session for %s... ", a_lpszHostName);
#endif

	svsutil_StringHashRef (gMem->pStringHash, a_lpszHostName);

	ScSession *pSess = pSessList;
	while (pSess) {
		if ((pSess->lpszHostName == a_lpszHostName) &&
		    ((pSess->qType == SCFILE_QP_FORMAT_TCP) ||
		     (pSess->qType == SCFILE_QP_FORMAT_OS)) && 
			((pSess->fSessionState == SCSESSION_STATE_INACTIVE) ||
			 (pSess->fSessionState == SCSESSION_STATE_WAITING))) {

			pSess->AddRef ();
			svsutil_StringHashFree (gMem->pStringHash, a_lpszHostName);		// compensate the ref

#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Failed...\n");
#endif
			return pSess;
		}
		pSess = pSess->pNext;
	}

	pSess = new ScSession (a_lpszHostName, pSessList, SCFILE_QP_FORMAT_OS);

	if (pSess) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Created...\n");
#endif
		pSessList = pSess;
	} else {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Failed...\n");
#endif
		SVSUTIL_ASSERT (0);

		svsutil_StringHashFree (gMem->pStringHash, a_lpszHostName);		// compensate the ref
	}

	return pSess;
}

void ScSessionManager::ReleaseSession (ScSession *pSess) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Releasing session for %s\n", pSess->lpszHostName);
#endif
	pSess->DelRef ();
	if (pSess->GetRefCount() == 0) {
		if (pSess->pPrev == NULL) {
			SVSUTIL_ASSERT (pSess == pSessList);
			pSessList = pSessList->pNext;
			if (pSessList)
				pSessList->pPrev = NULL;
		} else {
			pSess->pPrev->pNext = pSess->pNext;

			if (pSess->pNext)
				pSess->pNext->pPrev = pSess->pPrev;
		}
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Permanently deleting session for %s\n", pSess->lpszHostName);
#endif
		svsutil_StringHashFree (gMem->pStringHash, pSess->lpszHostName);
		pSess->lpszHostName = NULL;
		delete pSess;

		return;
	}
}

void ScSessionManager::PacketInserted (ScSession *pSess) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION | VERBOSE_MASK_PACKETS | VERBOSE_MASK_QUEUE, L"Bzzzzz!!! Packet Inserted...\n");
#endif

	if ((! pSess) || (pSess->fSessionState == SCSESSION_STATE_INACTIVE))
		SetEvent (hBuzz);
	else if (pSess->fSessionState == SCSESSION_STATE_OPERATING)
		SetEvent (pSess->hEvent);
#if defined (SC_VERBOSE)
	else
		scerror_DebugOut (VERBOSE_MASK_SESSION | VERBOSE_MASK_PACKETS | VERBOSE_MASK_QUEUE, L"...silent buzz, actually...\n");
#endif
}

//
//	private support
//
int ScSessionManager::SpinSession (ScSession *pSess, SOCKET s) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	//
	//	If session already exists and active, we don't want to duplicate the
	//	activity. Let them just send through already existing connection!
	//
	if ((pSess->fSessionState != SCSESSION_STATE_INACTIVE) &&
		(pSess->fSessionState != SCSESSION_STATE_WAITING))
		return FALSE;

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Preparing to spin thread pair for %s...\n", pSess->lpszHostName);
#endif

	//
	//	We need thread pair to spin a connection. Do we have it?
	//
	if (! BookThreadPair()) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"No free thread pairs...\n");
#endif
		return FALSE;
	}

	pSess->s			 = s;
	pSess->fSessionState = SCSESSION_STATE_CONNECTING;
	pSess->hEvent        = CreateEvent (NULL, FALSE, FALSE, NULL);

	DWORD tid;

	if (pSess->qType == SCFILE_QP_FORMAT_HTTP || pSess->qType == SCFILE_QP_FORMAT_HTTPS) {
		pSess->hServiceThreadW = CreateThread (NULL, 0, ScSession::ServiceThreadHttpW_s,  pSess, 0, &tid);
		return TRUE;
	}
	
	if (! gMachine->fUseBinary) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Binary protocol disabled - session order ignored...\n");
#endif
		ReleaseThreadPair();
		return FALSE;
	}

	pSess->hServiceThreadR = CreateThread (NULL, 0, ScSession::ServiceThreadR_s,  pSess, 0, &tid);
	pSess->hServiceThreadW = CreateThread (NULL, 0, ScSession::ServiceThreadW_s,  pSess, 0, &tid);

	return TRUE;
}

void ScSessionManager::AccThread  (void) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"In listener...\n");
#endif

	while (s_listen != INVALID_SOCKET) {
		sockaddr_in addr;
		int			addrlen = sizeof (addr);

		SOCKET s = accept (s_listen, (sockaddr *)&addr, &addrlen);

		if (s == INVALID_SOCKET)
			continue;

		char  mbbuffer[MSMQ_SC_SMALLBUFFER];
		char  *l_lpmbHostName;

		if (! NBStatusQuery (mbbuffer, sizeof(mbbuffer), addr.sin_addr.S_un.S_addr)) {
			HOSTENT *pHost = gethostbyaddr ((char *)&addr.sin_addr, sizeof(addr.sin_addr), AF_INET);
			if (pHost)
				l_lpmbHostName = pHost->h_name;
			else {
				closesocket (s);
				scerror_Inform (MSMQ_SC_ERRMSG_UNNAMEDCONNECT, addr.sin_addr.S_un.S_un_b.s_b1,
					addr.sin_addr.S_un.S_un_b.s_b2, addr.sin_addr.S_un.S_un_b.s_b3,
					addr.sin_addr.S_un.S_un_b.s_b4);
				continue;
			}
		} else
			l_lpmbHostName = mbbuffer;

		int iWStrLen = MultiByteToWideChar (CP_ACP, 0, l_lpmbHostName, -1, NULL, 0) + 1;

		SVSUTIL_ASSERT (iWStrLen > 0);

		gMem->Lock ();

		WCHAR *l_lpszHostName = (WCHAR *)g_funcAlloc (iWStrLen * sizeof(WCHAR), g_pvAllocData);
		MultiByteToWideChar (CP_ACP, 0, l_lpmbHostName, -1, l_lpszHostName, iWStrLen);

#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Got socket connection for host %s\n", l_lpszHostName);
#endif

		WCHAR *lpszHashedName = svsutil_StringHashAlloc (gMem->pStringHash, l_lpszHostName);
		g_funcFree (l_lpszHostName, g_pvFreeData);
		ScSession *pSess = GetInactiveOsSession (lpszHashedName);

		int fSockClose = FALSE;

		if (! pSess)
			fSockClose = TRUE;
		else {
			if (! SpinSession (pSess, s)) {
				fSockClose = TRUE;
				ReleaseSession (pSess);
			} else
				pSess->ipPeerAddr = addr.sin_addr;
		}

		svsutil_StringHashFree (gMem->pStringHash, lpszHashedName);
		gMem->Unlock ();

		if (fSockClose)
			closesocket (s);
	}
}

void ScSessionManager::ConnService  (void) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	//
	//	Why do we want to rescan it?
	//	1. New message arrived to a queue which session may have been inactive (buzz)
	//	2. There are sessions waiting for retry and the time has come (timeout)
	//	3. We did not have the thread and now we do (buzz)
	//
	//
	//	How do we want to rescan? Check all queues for inactives and waiting with expired
	//  and spin them off...
	//
	scutil_IsLocalTCP (NULL);	// Reset IP table

	unsigned int uiNow = scutil_now ();
	unsigned int uiNextWakeup = 0;

	int iLanNum = -1;

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Connection creator scan, time %08x...\n", uiNow);
#endif

	ScQueueList *pql = gQueueMan->pqlOutgoing;

	while (pql) {
		ScSession *pSess = pql->pQueue->pSess;
		//
		//	What do we do here?
		//
		//	If session is waiting for retry and the time's up, retry.
		//	If session is inactive and the queue is not empty, retry.
		//
		if (! pql->pQueue->pPackets->IsEmpty()) {
			if (((pSess->fSessionState == SCSESSION_STATE_WAITING) && time_less_equal (pSess->uiNextAttemptTime, uiNow)) ||
				(pSess->fSessionState == SCSESSION_STATE_INACTIVE)) {

				if (iLanNum == -1)
					iLanNum = (gMachine->uiLanOffDelay == 0) ? 255 : scutil_GetLanNum();

#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_SESSION, L"Can spin for %s, status %d requested time %d, have %d lan interfaces (255 = zero port override)...\n",
							pSess->lpszHostName, pSess->fSessionState, pSess->uiNextAttemptTime, iLanNum);
#endif
				if (iLanNum) {
					pSess->AddRef ();
					if (! SpinSession (pSess, INVALID_SOCKET))
						ReleaseSession (pSess);
				} else {
					pSess->uiNextAttemptTime = uiNow + gMachine->uiLanOffDelay;
					pSess->fSessionState = SCSESSION_STATE_WAITING;
				}
			} else if (pSess->fSessionState == SCSESSION_STATE_OPERATING)
				SetEvent (pSess->hEvent);
		}

		if ((pSess->fSessionState == SCSESSION_STATE_WAITING) &&
			time_greater (pSess->uiNextAttemptTime, uiNow) &&
				((! uiNextWakeup) ||
				time_less (pSess->uiNextAttemptTime, uiNextWakeup)))
			uiNextWakeup = pSess->uiNextAttemptTime;

		pql = pql->pqlNext;
	}

	if (! uiNextWakeup)
		svsutil_ClearAttrTimer (gMem->pTimer, (SVSHandle)hBuzz);
	else {
		SVSUTIL_ASSERT (time_greater (uiNextWakeup, uiNow));
		svsutil_SetAttrTimer (gMem->pTimer, (SVSHandle)hBuzz, (uiNextWakeup - uiNow) * 1000);
	}
}

void ScSessionManager::ConnectToNet (unsigned int uiDelta) {
	gMem->Lock ();

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Reconnecting!\n");
#endif

	unsigned int uiNowS = scutil_now ();

	ScSession *pSess = gSessionMan->pSessList;

	while (pSess) {
		if (pSess->fSessionState == SCSESSION_STATE_WAITING)
			pSess->uiNextAttemptTime = uiNowS + uiDelta;

		pSess = pSess->pNext;
	}

	SetEvent (gSessionMan->hBuzz);

	scutil_IsLocalTCP (NULL);

	gMem->Unlock ();
}

DWORD WINAPI ScSessionManager::AccThread_s (void *arg) {
	gSessionMan->AccThread();
	return 0;
}

void ScSession::FailConnection (void) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	int i = InterlockedIncrement((long *)&iFailures) - 1;

	if (i >= (int)gMachine->uiRetrySchedule)
		i = gMachine->uiRetrySchedule - 1;

	ScQueueList *pQueueList = gQueueMan->pqlOutgoing;

	int fFoundWaiting = FALSE;

	while (pQueueList) {
		if ((pQueueList->pQueue->pSess == this) && (! pQueueList->pQueue->pPackets->IsEmpty())) {
			fFoundWaiting = TRUE;
			break;
		}

		pQueueList = pQueueList->pqlNext;
	}

	if (! fFoundWaiting) {
		fSessionState = SCSESSION_STATE_INACTIVE;
		return;
	}

	uiNextAttemptTime = scutil_now() + gMachine->asRetrySchedule[i];
	fSessionState     = SCSESSION_STATE_WAITING;

	SetEvent (gSessionMan->hBuzz);

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Connection to %s failed. Next attempt - at %08x\n", lpszHostName, uiNextAttemptTime);
#endif
}

void ScSession::OkConnection (unsigned int uiConnectionType) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Connection to %s succeeded\n", lpszHostName);
#endif

	iFailures            = 0;
	fSessionState        = uiConnectionType;
}

void ScSession::InitConnection (void) {
	gMem->Lock ();

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Initializing connection to %s...\n", lpszHostName);
#endif
	uiAckTimeout         = 0;
	uiStoreAckTimeout    = 0;

	uiMyWindowSize       = MSMQ_DEFAULT_WINDOW_SIZE_PACKET;
	uiOtherWindowSize    = 0;

	usLastAckSent        = 0;
	usLastRelAckSent     = 0;

	usPacketsSent        = 0;
	usRelPacketsSent     = 0;

	usLastAckReceived    = 0;

	usPacketsReceived    = 0;
	usRelPacketsReceived = 0;

	uiAckDue             = 0;

	SVSUTIL_ASSERT (pSentPackets == NULL);
	SVSUTIL_ASSERT (pSentRelPackets == NULL);

	gMem->Unlock ();
}

int ScSession::SendECP (int fRefuseConnection) {
	SVSUTIL_ASSERT (! gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Sending Establish Connection Packet to %s...\n", lpszHostName);
#endif
	const int iPacketSize = sizeof(CBaseHeader) + sizeof(CInternalSection) + sizeof(CECSection);
	char __buf[iPacketSize];

	CBaseHeader *pBaseHeader = (CBaseHeader *)__buf;
	pBaseHeader->CBaseHeader::CBaseHeader(iPacketSize);
    pBaseHeader->SetType(FALCON_INTERNAL_PACKET);

	CInternalSection *pInternal = (CInternalSection *)pBaseHeader->GetNextSection();
	pInternal->CInternalSection::CInternalSection(INTERNAL_ESTABLISH_CONNECTION_PACKET);

	if (fRefuseConnection)
		pInternal->SetRefuseConnectionFlag ();

	CECSection *pECSection = (CECSection *)pInternal->GetNextSection ();

	if (fSessionState == SCSESSION_STATE_CONNECTED_CLIENT)
		pECSection->CECSection::CECSection(&gMachine->guid, &guidDest, FALSE);
	else
		pECSection->CECSection::CECSection(&guidDest, &gMachine->guid, uiConnectionStamp, FALSE);

	//    pECSession->CheckAllowNewSession(FALSE); Do we need this???

	int iTotalSent = 0;

	while (iTotalSent < iPacketSize) {
		int iSent = send (s, &__buf[iTotalSent], iPacketSize - iTotalSent, 0);
		if (iSent == SOCKET_ERROR) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP send to %s failed (%d bytes transmitted)...\n", lpszHostName, iTotalSent);
#endif
			return FALSE;
		}

		iTotalSent += iSent;
	}

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP send to %s succeeded (%d bytes transmitted)...\n", lpszHostName, iTotalSent);
#endif
	return TRUE;
}

int ScSession::RecvECP (void) {
	SVSUTIL_ASSERT (! gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Receiving Establish Connection Packet from %s...\n", lpszHostName);
#endif
	const int iPacketSize = sizeof(CBaseHeader) + sizeof(CInternalSection) + sizeof(CECSection);
	char __buf[iPacketSize];

	int iTotalRecvd = 0;

	while (iTotalRecvd < iPacketSize) {
		int iRecvd = recv (s, &__buf[iTotalRecvd], iPacketSize - iTotalRecvd, 0);
		if (iRecvd <= 0) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s failed (%d bytes transmitted)...\n", lpszHostName, iTotalRecvd);
#endif
			return FALSE;
		}
		iTotalRecvd += iRecvd;
	}

	CBaseHeader *pBaseHeader = (CBaseHeader *)__buf;
    if (! (pBaseHeader->SignatureIsValid() &&  pBaseHeader->VersionIsValid())) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s failed (bad sig or version)...\n", lpszHostName);
#endif
		return FALSE;
	}

    if (pBaseHeader->GetType() != FALCON_INTERNAL_PACKET) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s failed (bad type %08x)...\n", lpszHostName, pBaseHeader->GetType());
#endif
		return FALSE;
	}

    if  (pBaseHeader->GetPacketSize() != iPacketSize) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s failed (bad size %d, should be %d)...\n", lpszHostName, pBaseHeader->GetPacketSize(), iPacketSize);
#endif
		return FALSE;
	}

	CInternalSection *pInternal = (CInternalSection *)pBaseHeader->GetNextSection();

    if (pInternal->GetPacketType() != INTERNAL_ESTABLISH_CONNECTION_PACKET) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s failed (not a connection packet, type is %08x)...\n", lpszHostName, pInternal->GetPacketType());
#endif
		return FALSE;
	}

	CECSection *pECSection = (CECSection *)pInternal->GetNextSection ();

    if (fSessionState == SCSESSION_STATE_CONNECTED_CLIENT) {
		guidDest = *pECSection->GetServerQMGuid();
		if (pInternal->GetRefuseConnectionFlag()) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s failed (connection refused)...\n", lpszHostName);
#endif
			return FALSE;
		}
		uiConnectionDelta = GetTickCount() - pECSection->GetTimeStamp();
	} else {
		guidDest = *pECSection->GetClientQMGuid();
		uiConnectionStamp = pECSection->GetTimeStamp();
	}

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s succeeded (guid = " SC_GUID_FORMAT L")...\n", lpszHostName, SC_GUID_ELEMENTS((&guidDest)));
#endif

	//
	//	Disallow loopbacks - they break all possible relationships in XACT queues.
	//
	if (guidDest == gMachine->guid) {
		if (fSessionState == SCSESSION_STATE_CONNECTED_SERVER)
			SendECP (TRUE);

		return FALSE;
	}

	return TRUE;
}

int ScSession::SendCPP (void) {
	SVSUTIL_ASSERT (! gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Sending Connection Parameters Packet to %s...\n", lpszHostName);
#endif

    if (fSessionState == SCSESSION_STATE_CONNECTED_CLIENT) {
		uiAckTimeout      = uiConnectionDelta * 80 * 10;

		if (gMachine->uiMinAckTimeout > uiAckTimeout)
			uiAckTimeout = gMachine->uiMinAckTimeout;
		else if (uiAckTimeout > gMachine->uiMaxAckTimeout)
			uiAckTimeout = gMachine->uiMaxAckTimeout;

		uiStoreAckTimeout = uiConnectionDelta * 8;

        if (gMachine->uiMinStoreAckTimeout > uiStoreAckTimeout)
            uiStoreAckTimeout = gMachine->uiMinStoreAckTimeout;

        if (uiStoreAckTimeout > uiAckTimeout)
			uiStoreAckTimeout = uiAckTimeout;
	}

	const int iPacketSize = sizeof(CBaseHeader) + sizeof(CInternalSection) + sizeof(CCPSection);
	char __buf[iPacketSize];

    CBaseHeader *pBase = (CBaseHeader *)__buf;
	pBase->CBaseHeader::CBaseHeader (iPacketSize);

    pBase->SetType (FALCON_INTERNAL_PACKET);

    CInternalSection *pInternal = (CInternalSection *)pBase->GetNextSection ();
	pInternal->CInternalSection::CInternalSection(INTERNAL_CONNECTION_PARAMETER_PACKET);

    CCPSection *pCPSection = (CCPSection *)pInternal->GetNextSection();
	pCPSection->CCPSection::CCPSection (uiMyWindowSize, uiStoreAckTimeout,
														uiAckTimeout, 0);

	int iTotalSent = 0;

	while (iTotalSent < iPacketSize) {
		int iSent = send (s, &__buf[iTotalSent], iPacketSize - iTotalSent, 0);
		if (iSent == SOCKET_ERROR) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Connection Parameters Packet send to %s failed (%d bytes transmitted)...\n", lpszHostName, iTotalSent);
#endif
			return FALSE;
		}
		iTotalSent += iSent;
	}

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Connection Parameters Packet send to %s succeeded (%d bytes transmitted)...\n", lpszHostName, iTotalSent);
#endif
	return TRUE;
}

int ScSession::RecvCPP (void) {
	SVSUTIL_ASSERT (! gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Receiving Connection Parameters Packet from %s...\n", lpszHostName);
#endif

	const int iPacketSize = sizeof(CBaseHeader) + sizeof(CInternalSection) + sizeof(CCPSection);
	char __buf[iPacketSize];

	int iTotalRecvd = 0;

	while (iTotalRecvd < iPacketSize) {
		int iRecvd = recv (s, &__buf[iTotalRecvd], iPacketSize - iTotalRecvd, 0);
		if (iRecvd <= 0) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Connection Parameters Packet receive from %s failed (%d bytes transmitted)...\n", lpszHostName, iTotalRecvd);
#endif
			return FALSE;
		}

		iTotalRecvd += iRecvd;
	}

	CBaseHeader *pBaseHeader = (CBaseHeader *)__buf;
    if (! (pBaseHeader->SignatureIsValid() &&  pBaseHeader->VersionIsValid())) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s failed (bad sig or version)...\n", lpszHostName);
#endif
		return FALSE;
	}

    if (pBaseHeader->GetType() != FALCON_INTERNAL_PACKET) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s failed (bad packet type %d)...\n", lpszHostName, pBaseHeader->GetType());
#endif
		return FALSE;
	}

    if  (pBaseHeader->GetPacketSize() != iPacketSize) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s failed (bad packet type %d)...\n", lpszHostName, pBaseHeader->GetType());
#endif
		return FALSE;
	}

	CInternalSection *pInternal = (CInternalSection *)pBaseHeader->GetNextSection();

    if (pInternal->GetPacketType() != INTERNAL_CONNECTION_PARAMETER_PACKET) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s failed (bad internal packet type %d)...\n", lpszHostName, pInternal->GetPacketType());
#endif
		return FALSE;
	}

    CCPSection *pCPSection = (CCPSection *)pInternal->GetNextSection();

    if (fSessionState == SCSESSION_STATE_CONNECTED_SERVER) {
		uiAckTimeout      = pCPSection->GetAckTimeout();
		uiStoreAckTimeout = pCPSection->GetRecoverAckTimeout();
	} else if ((uiAckTimeout != pCPSection->GetAckTimeout()) ||
		(uiStoreAckTimeout != pCPSection->GetRecoverAckTimeout())) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s failed (inconsistent ack timing)...\n", lpszHostName, pInternal->GetPacketType());
#endif
		return FALSE;
	}

    uiOtherWindowSize = pCPSection->GetWindowSize();

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"ECP receive from %s succeeded. Timeout(Store) = %d(%d). Window size = %d...\n",
							lpszHostName, uiAckTimeout, uiStoreAckTimeout, uiOtherWindowSize);
#endif

	return TRUE;
}

int ScSession::HandleSessionSection (CSessionSection *pcSessionSection) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Handling session section for %s...\n", lpszHostName);
#endif

    unsigned short usSyncAckSequenceNo, usSyncAckRecoverNo;

    pcSessionSection->GetSyncNo(&usSyncAckSequenceNo, &usSyncAckRecoverNo);

    unsigned short usOtherReceived    = pcSessionSection->GetAcknowledgeNo();
    unsigned short usRelOtherReceived = pcSessionSection->GetStorageAckNo();
    unsigned int   uiRelBitfield      = pcSessionSection->GetStorageAckBitField();

	if ((usSyncAckSequenceNo != usPacketsReceived) || 
		(usSyncAckRecoverNo  != usRelPacketsReceived) ||
		sessnum_greater (usOtherReceived, usPacketsSent) ||
		sessnum_greater (usRelOtherReceived, usRelPacketsSent)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Error handling session section for %s - synchronization check failed...\n", lpszHostName);
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Our side: %d packets received, %d recoverable packets received,\n\t%d packets sent, %d reliable packets sent.\n",
					usPacketsReceived, usRelPacketsReceived, usPacketsSent, usRelPacketsSent);
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Other side: %d packets sent, %d recoverable packets sent,\n\t%d packets received, %d reliable packets received.\n",
					usSyncAckSequenceNo, usSyncAckRecoverNo, usOtherReceived, usRelOtherReceived);
#endif
		return FALSE;
	}

    uiOtherWindowSize = pcSessionSection->GetWindowSize();

	//
	//	Throw away acked packets. Do express first.
	//
	SentPacket	*pSentRunner = pSentPackets;
	SentPacket	*pSentParent = NULL;

	while (pSentRunner) {
		if ((signed short) (usOtherReceived - pSentRunner->usNum) >= 0)
			break;

		pSentParent = pSentRunner;
		pSentRunner = pSentRunner->pNext;
	}

	if (pSentRunner) {
		if (pSentParent)
			pSentParent->pNext = NULL;
		else
			pSentPackets = NULL;

		while (pSentRunner) {
			pSentParent = pSentRunner;
			pSentRunner = pSentRunner->pNext;

#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Recorded ack for express message %08x ptr %08x session number %d\n", pSentParent->pPacket->uiMessageID, pSentParent->pPacket, pSentParent->usNum);
#endif

			gQueueMan->JournalPacket (pSentParent->pPacket);

			ScQueue *pQueue = pSentParent->pPacket->pQueue;
			pQueue->DisposeOfPacket (pSentParent->pPacket);

			svsutil_FreeFixed (pSentParent, gMem->pAckNodeMem);
		}
	}

	//
	//	Now remove reliable packets...
	//
	if (usRelOtherReceived) {
		pSentRunner = pSentRelPackets;
		pSentParent = NULL;

		int iBit = 32;									// Bit count starts from 1
		unsigned short usRelPackNum = usRelOtherReceived + iBit;

		for ( ; ; ) {
			if ((iBit > 0) && ((uiRelBitfield & (1 << (iBit - 1))) == 0)) {
				--iBit;
				--usRelPackNum;
				continue;
			}

			while (pSentRunner && sessnum_greater (pSentRunner->usNum, usRelPackNum)) {
				pSentParent = pSentRunner;
				pSentRunner = pSentRunner->pNext;
			}

			if (! pSentRunner)
				break;

			if (usRelPackNum == (int)pSentRunner->usNum) {
				if (pSentParent)
					pSentParent->pNext = pSentRunner->pNext;
				else
					pSentRelPackets = pSentRunner->pNext;

				SentPacket *pSentX = pSentRunner;
				pSentRunner = pSentRunner->pNext;

#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_SESSION, L"Recorded ack for reliable message %08x ptr %08x session number %d\n", pSentX->pPacket->uiMessageID, pSentX->pPacket, pSentX->usNum);
#endif
				gQueueMan->JournalPacket (pSentX->pPacket);
				ScQueue *pQueue = pSentX->pPacket->pQueue;
				pQueue->DisposeOfPacket (pSentX->pPacket);

				svsutil_FreeFixed (pSentX, gMem->pAckNodeMem);
			}

			--iBit;
			--usRelPackNum;

			if (sessnum_less (usRelPackNum, usRelOtherReceived))
				break;
		}
	}

	usLastAckReceived = usOtherReceived;

	SetEvent (hEvent);

	return TRUE;
}

int ScSession::HandleInternalPacket (CBaseHeader *pBaseHeader) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	if (! pBaseHeader->SessionIsIncluded())
        return FALSE;

	SVSUTIL_ASSERT (pBaseHeader->GetType() == FALCON_INTERNAL_PACKET);

    CInternalSection *pInternalSect = (CInternalSection *)pBaseHeader->GetNextSection ();
    CSessionSection  *pSessionSect  = (CSessionSection *)pInternalSect->GetNextSection();

	return HandleSessionSection (pSessionSect);
}

int ScSession::HandleUserPacket (ScPacketImage *pImage, CSessionSection *pcSessionSection) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"User packet arrived from %s\n", lpszHostName);
#endif

	if (! pImage->PopulateSections()) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"User packet from %s is corrupted...\n", lpszHostName);
#endif
		g_funcFree (pImage, g_pvFreeData);
		return FALSE;
	}

	int fReliable = (pImage->sect.pUserHeader->GetDelivery() == MQMSG_DELIVERY_RECOVERABLE);

	++usPacketsReceived;
	if (! usPacketsReceived)
		++usPacketsReceived;

	unsigned int uiNow     = GetTickCount ();
	unsigned int uiAckNext = uiNow + uiAckTimeout / 2;

	if (fReliable) {
		++usRelPacketsReceived;
		if (! usRelPacketsReceived)
			++usRelPacketsReceived;

		uiAckNext = uiNow + uiStoreAckTimeout / 2;
	}

    if (sessnum_greater_equal (usPacketsReceived, (unsigned short)(usLastAckSent + uiOtherWindowSize)) ||
		sessnum_greater_equal (usRelPacketsReceived,  (unsigned short)(usLastRelAckSent + STORED_ACK_BITFIELD_SIZE)))
		uiAckNext = uiNow;

    if ((! uiAckDue) || (time_greater (uiAckDue, uiAckNext))) {
		uiAckDue = uiAckNext;
		SVSUTIL_ASSERT (time_less_equal (uiNow, uiAckDue));
		svsutil_SetAttrTimer (gMem->pTimer, (SVSHandle)hEvent, uiAckDue - uiNow);
	}

#if defined (SC_VERBOSE)
	if (uiAckDue && time_greater (uiNow, uiAckDue + 3000))
		scerror_DebugOut (VERBOSE_MASK_WARN, L"WARNING - underrunning more than 3 secs on ACK time! now %08x, due %08x\n", uiNow, uiAckDue);
#endif

    if (pcSessionSection) {
		SVSUTIL_ASSERT (pImage->sect.pBaseHeader->SessionIsIncluded());
		pImage->sect.pBaseHeader->IncludeSession (FALSE);

		if (! HandleSessionSection (pcSessionSection)) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Couldn't handle session sect for packet from %s...\n", lpszHostName);
#endif
			g_funcFree (pImage, g_pvFreeData);
			return FALSE;
		}
	}

	ScQueue *pQueue = NULL;

	if (! (pQueue = gQueueMan->FindQueueByPacketImage (pImage, FALSE))) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"User packet from %s arrived to non-existent queue...\n", lpszHostName);
#endif
		gQueueMan->ForwardTransactionalResponse (pImage, MQMSG_CLASS_NACK_BAD_DST_Q, lpszHostName, &guidDest);
		gQueueMan->RejectPacket (pImage, MQMSG_CLASS_NACK_BAD_DST_Q);
		g_funcFree (pImage, g_pvFreeData);
	} else {
		//
		//	Put this thing in a queue...
		//
		//
		//	If we don't have resources, this will fail. In this case we'd
		//	better close the connection.
		//
		if (! pQueue->qp.bIsIncoming) {
			if (pImage->sect.pUserHeader->GetHopCount () > MSMQ_SC_MAX_HOP_COUNT) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_SESSION, L"User packet from %s exceeded hop count...\n", lpszHostName);
#endif
				gQueueMan->ForwardTransactionalResponse (pImage, MQMSG_CLASS_NACK_HOP_COUNT_EXCEEDED, lpszHostName, &guidDest);
				gQueueMan->RejectPacket (pImage, MQMSG_CLASS_NACK_HOP_COUNT_EXCEEDED);
				g_funcFree (pImage, g_pvFreeData);
			} else {
				pImage->sect.pUserHeader->IncHopCount ();
				if (! pQueue->MakePacket (pImage, -1, TRUE)) {
#if defined (SC_VERBOSE)
					scerror_DebugOut (VERBOSE_MASK_SESSION, L"User packet from %s cannot be put into %s...\n", lpszHostName, pQueue->lpszFormatName);
#endif
					gQueueMan->RejectPacket (pImage, MQMSG_CLASS_NACK_Q_EXCEED_QUOTA);
					g_funcFree (pImage, g_pvFreeData);
				}
			}
		} else if (pQueue->qp.bTransactional) {
			if (! gQueueMan->StoreTransactionalPacket (pQueue, pImage, lpszHostName, &guidDest))
				g_funcFree (pImage, g_pvFreeData);
		} else {
			pImage->hkOrderKey = ++pQueue->hkReceived;
			pImage->hkOrderKey |= pImage->sect.pBaseHeader->GetPriority() << SCPACKET_ORD_TIMEBITS;
			ScPacket *pPacket = pQueue->MakePacket (pImage, -1, TRUE);

			if (! pPacket) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_SESSION, L"User packet from %s cannot be put into %s...\n", lpszHostName, pQueue->lpszFormatName);
#endif
				gQueueMan->RejectPacket (pImage, MQMSG_CLASS_NACK_Q_EXCEED_QUOTA);
				g_funcFree (pImage, g_pvFreeData);
			} else
				gQueueMan->AcceptPacket (pPacket, MQMSG_CLASS_ACK_REACH_QUEUE, pQueue);
		}
	}

	return TRUE;
}

//
//	Get the connection to happen
//
int ScSession::Connect (void) {
	sockaddr_in sin;

	sin.sin_family = AF_INET;
	sin.sin_port   = htons (gMachine->uiPort);

	unsigned long ul_addr = inet_addr (lpszmbHostName);

	if (ul_addr != INADDR_NONE) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Connecting to %s via direct IP\n", lpszHostName);
#endif
		sin.sin_addr.S_un.S_addr = ul_addr;
		ipPeerAddr = sin.sin_addr;

		return (connect (s, (sockaddr *)&sin, sizeof(sin)) == 0);
	}

	//
	//	Do two passes - pinging and connecting...
	//
	unsigned long ul_wins[MSMQ_SC_IPBUFFER];
	unsigned long ul_wins_report[MSMQ_SC_IPBUFFER];
	int nWins = 0;

	for (int iPass = 0 ; iPass < 2 ; ++iPass) {
		if (iPass == 0) {
			HOSTENT *pHost = gethostbyname (lpszmbHostName);
			if (pHost)
				ul_addr = *(unsigned long *)pHost->h_addr_list[0];
		}

		if (ul_addr != INADDR_NONE) {
			sin.sin_addr.S_un.S_addr = ul_addr;
			ipPeerAddr = sin.sin_addr;

			if ((iPass == 0) && ping (ul_addr)) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_SESSION, L"Connecting to %s as %d.%d.%d.%d (gethostbyname, ping)\n", lpszHostName,
							sin.sin_addr.S_un.S_un_b.s_b1, sin.sin_addr.S_un.S_un_b.s_b2, sin.sin_addr.S_un.S_un_b.s_b3, sin.sin_addr.S_un.S_un_b.s_b4);
#endif
				return (connect (s, (sockaddr *)&sin, sizeof(sin)) == 0);
			}

			if ((iPass == 1) || (! gMachine->uiPingPort)) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_SESSION, L"Connecting to %s as %d.%d.%d.%d (gethostbyname)\n", lpszHostName,
								sin.sin_addr.S_un.S_un_b.s_b1, sin.sin_addr.S_un.S_un_b.s_b2, sin.sin_addr.S_un.S_un_b.s_b3, sin.sin_addr.S_un.S_un_b.s_b4);
#endif
				if (connect (s, (sockaddr *)&sin, sizeof(sin)) == 0)
					return TRUE;
			}
		}

		if (iPass == 0)
			nWins = NBGetWins (ul_wins, MSMQ_SC_IPBUFFER);

		for (int i = 0 ; i < nWins ; ++i) {
			if (iPass == 0) {
				ul_wins_report[i] = NBQueryWins (lpszmbHostName, ul_wins[i]);

#if defined (SC_VERBOSE)
				{
					IN_ADDR __sin1 = *(IN_ADDR *)&ul_wins[i];
					IN_ADDR __sin2 = *(IN_ADDR *)&ul_wins_report[i];
					scerror_DebugOut (VERBOSE_MASK_SESSION, L"WINS %d.%d.%d.%d reports %s as %d.%d.%d.%d (gethostbyname)\n",
									__sin1.S_un.S_un_b.s_b1, __sin1.S_un.S_un_b.s_b2,
									__sin1.S_un.S_un_b.s_b3, __sin1.S_un.S_un_b.s_b4,
									lpszHostName,
									__sin2.S_un.S_un_b.s_b1, __sin2.S_un.S_un_b.s_b2,
									__sin2.S_un.S_un_b.s_b3, __sin2.S_un.S_un_b.s_b4
									);
				}
#endif
			}

			int fFound = FALSE;

			if ((ul_wins_report[i] == INADDR_NONE) || (ul_wins_report[i] == ul_addr))
				fFound = TRUE;
			else {
				for (int j = 0 ; j < i ; ++j) {
					if (ul_wins_report[j] == ul_wins_report[i]) {
						fFound = TRUE;
						break;
					}
				}
			}

			if (! fFound) {
				sin.sin_addr.S_un.S_addr = ul_wins_report[i];
				ipPeerAddr = sin.sin_addr;

				if ((iPass == 0) && ping (ul_wins_report[i])) {
#if defined (SC_VERBOSE)
					scerror_DebugOut (VERBOSE_MASK_SESSION, L"Connecting to %s as %d.%d.%d.%d (WINS lookup, ping)\n", lpszHostName,
								sin.sin_addr.S_un.S_un_b.s_b1, sin.sin_addr.S_un.S_un_b.s_b2, sin.sin_addr.S_un.S_un_b.s_b3, sin.sin_addr.S_un.S_un_b.s_b4);
#endif

					return connect (s, (sockaddr *)&sin, sizeof(sin)) == 0;
				}

				if ((iPass == 1) || (! gMachine->uiPingPort)) {
#if defined (SC_VERBOSE)
					scerror_DebugOut (VERBOSE_MASK_SESSION, L"Connecting to %s as %d.%d.%d.%d (WINS lookup)\n", lpszHostName,
								sin.sin_addr.S_un.S_un_b.s_b1, sin.sin_addr.S_un.S_un_b.s_b2, sin.sin_addr.S_un.S_un_b.s_b3, sin.sin_addr.S_un.S_un_b.s_b4);
#endif
					if (connect (s, (sockaddr *)&sin, sizeof(sin)) == 0)
						return TRUE;
				}
			}
		}

		if (! gMachine->uiPingPort)
			break;
	}

	return FALSE;
}
//
//	Reading thread: establish the connection and start recv'ing
//	Writing thread: control timely establishment of connection and then
//	monitor situation and send stuff.
//
//	Finally, writing thread brings down reading thread, decrements thread
//	pair count and exits.
//
void ScSession::ServiceThreadR (void) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Started receive thread for %s\n", lpszHostName);
#endif

	if (fSessionState != SCSESSION_STATE_CONNECTING)
		return;

	InitConnection ();

	if (s == INVALID_SOCKET) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Creating socket connection for %s\n", lpszHostName);
#endif
		//
		//	Create socket.
		//

		s = socket (AF_INET, SOCK_STREAM, 0);

		if ((s == INVALID_SOCKET) || (! Connect ())) {
#if defined (SC_VERBOSE)
			if (s == INVALID_SOCKET)
				scerror_DebugOut (VERBOSE_MASK_SESSION, L"Can't open a socket for %s\n", lpszHostName);
			else
				scerror_DebugOut (VERBOSE_MASK_SESSION, L"Can't connect to %s\n", lpszHostName);
#endif
			gMem->Lock ();
			FailConnection ();
			SetEvent (hEvent);
			CloseHandle (hServiceThreadR);
			hServiceThreadR = NULL;
			gMem->Unlock ();
			return;
		}

		gMem->Lock ();
		OkConnection (SCSESSION_STATE_CONNECTED_CLIENT);
		gMem->Unlock ();

		//
		//	Send establish connection packet
		//
		if ((! SendECP(FALSE)) || (! RecvECP())) {
			gMem->Lock ();
			FailConnection ();
			SetEvent (hEvent);
			CloseHandle (hServiceThreadR);
			hServiceThreadR = NULL;
			gMem->Unlock ();
			return;
		}

		//
		//	Get ack.
		//
		if ((! SendCPP()) || (! RecvCPP())) {
			gMem->Lock ();
			FailConnection ();
			SetEvent (hEvent);
			CloseHandle (hServiceThreadR);
			hServiceThreadR = NULL;
			gMem->Unlock ();
			return;
		}
	} else {
		gMem->Lock ();
		OkConnection (SCSESSION_STATE_CONNECTED_SERVER);
		gMem->Unlock ();

		//
		//	Receive establishing connection packet.
		//
		if ((! RecvECP()) || (! SendECP(FALSE))) {
			gMem->Lock ();
			FailConnection ();
			SetEvent (hEvent);
			CloseHandle (hServiceThreadR);
			hServiceThreadR = NULL;
			gMem->Unlock ();
			return;
		}
		//
		//	Send ack
		//
		if ((! RecvCPP()) || (! SendCPP())) {
			gMem->Lock ();
			FailConnection ();
			SetEvent (hEvent);
			CloseHandle (hServiceThreadR);
			hServiceThreadR = NULL;
			gMem->Unlock ();
			return;
		}
	}

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Session with %s initialized successfully\n", lpszHostName);
#endif

	fSessionState = SCSESSION_STATE_OPERATING;
	SetEvent (hEvent);

	while (fSessionState == SCSESSION_STATE_OPERATING) {
		const int iHeaderSize = sizeof(CBaseHeader);
		const int iBufferSize = sizeof (CBaseHeader) + sizeof (CDebugSection) + sizeof(CSessionSection) +
			sizeof (CECSection) + sizeof (CCPSection) + sizeof (CInternalSection);

		char __buf[iBufferSize];

		ScPacketImage *pImage = NULL;
		char		  *pcBuffer = NULL;

		int			  fError = FALSE;

		int iTotalRecvd = 0;

		while (iTotalRecvd < iHeaderSize) {
			int iRecvd = recv (s, &__buf[iTotalRecvd], iHeaderSize - iTotalRecvd, 0);
			if (iRecvd <= 0) {
				fError = TRUE;
				break;
			}

			iTotalRecvd += iRecvd;
		}

		if (fError) {
			// The other side had just closed cconnection. If we are not done and are writing,
			// the writer thread will error out. Otherwise, we just become inactive...
			if ((iTotalRecvd == 0) && (fSessionState == SCSESSION_STATE_OPERATING)) {
				fSessionState = SCSESSION_STATE_INACTIVE;
				break;
			}

#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Error receiving a packet from %s (%d bytes received)\n", lpszHostName, iTotalRecvd);
#endif
			break;
		}

#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Received a packet from %s\n", lpszHostName);
#endif
		CBaseHeader *pBaseHeader = (CBaseHeader *)__buf;

		if (! (pBaseHeader->SignatureIsValid() &&  pBaseHeader->VersionIsValid())) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Bad packet from %s: incorrect version or signature\n", lpszHostName);
#endif
			break;
		}

        int iPacketSize;	// Has to be multiple of 4 and more than just header
		if (((iPacketSize = pBaseHeader->GetPacketSize()) <= iHeaderSize) ||
			(iPacketSize & 3)) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Bad packet from %s: too small (%d bytes)\n", lpszHostName, iPacketSize);
#endif
			break;
		}

		CSessionSection *pcSessionSection = NULL;

		int iTotalSize = iPacketSize;

		if (pBaseHeader->GetType() != FALCON_INTERNAL_PACKET) {
			iTotalSize += (pBaseHeader->SessionIsIncluded() ? CSessionSection::CalcSectionSize(): 0);

			pImage = (ScPacketImage *)g_funcAlloc (sizeof (*pImage) + iTotalSize, g_pvAllocData);
			if (! pImage)
				break;

			gMem->Lock ();
			++gSessionMan->fBusy;
			gMem->Unlock ();

			memset (&pImage->sect, 0, sizeof(pImage->sect));
			memset (&pImage->ucSourceAddr, 0, sizeof(pImage->ucSourceAddr));

			pImage->allflags = 0;
			pImage->flags.fSecureSession = ! gMachine->fUntrustedNetwork;
			pImage->hkOrderKey = 0;
			pImage->ipSourceAddr   = ipPeerAddr;
			pImage->pvBinary = NULL;
			pImage->pvExt    = NULL;

			void *pvBinaryStart = ((char *)pImage + sizeof(*pImage));
			memcpy (pvBinaryStart, __buf, iHeaderSize);

			pImage->flags.fHaveIpv4Addr = TRUE;

			pBaseHeader = (CBaseHeader *)pvBinaryStart;
			pcBuffer    = (char *)pBaseHeader;

			if (pBaseHeader->SessionIsIncluded())
				pcSessionSection = (CSessionSection *)(pcBuffer + iPacketSize);
		} else
			pcBuffer = __buf;

		while (iTotalRecvd < iTotalSize) {
			int iRecvd = recv (s, &pcBuffer[iTotalRecvd], iTotalSize - iTotalRecvd, 0);
			if (iRecvd <= 0) {
				fError = TRUE;
				break;
			}

			iTotalRecvd += iRecvd;
		}

		if (fError) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Error receiving a packet from %s (%d bytes out of %d received)\n", lpszHostName, iTotalRecvd, iTotalSize);
#endif
			if (pImage) {
				g_funcFree (pImage, g_pvFreeData);
				gMem->Lock ();
				--gSessionMan->fBusy;
				gMem->Unlock ();
			}

			break;
		}

		gMem->Lock ();

		int fRes;

		if (pImage) {
			fRes = HandleUserPacket (pImage, pcSessionSection);
			--gSessionMan->fBusy;
		} else
			fRes = HandleInternalPacket (pBaseHeader);

		gMem->Unlock ();

		if (! fRes)
			break;
	}

	if (fSessionState == SCSESSION_STATE_EXITING)
		return;

	gMem->Lock ();
	if (fSessionState == SCSESSION_STATE_OPERATING)
		FailConnection ();

	CloseHandle (hServiceThreadR);
	hServiceThreadR = NULL;
	SetEvent (hEvent);					// Thread exited
	gMem->Unlock ();
}

void ScSession::ReturnUnacketPacketsToQueues(void) {
	//
	//	Unacked packets return to the respective queues...
	//
    while (pSentPackets) {
        SentPacket *pSentNext = pSentPackets->pNext;
		SVSUTIL_ASSERT (! pSentPackets->pPacket->pQueue->qp.bIsIncoming);
#if defined (SC_VERBOSE)
        scerror_DebugOut (VERBOSE_MASK_SESSION, L"Returning express unacked message %08x ptr %08x to %s\n", pSentPackets->pPacket->uiMessageID, pSentPackets->pPacket, pSentPackets->pPacket->pQueue->lpszFormatName);
#endif
        pSentPackets->pPacket->pQueue->InsertPacket (pSentPackets->pPacket);
        svsutil_FreeFixed (pSentPackets, gMem->pAckNodeMem);
        pSentPackets = pSentNext;
    }

    while (pSentRelPackets) {
        SentPacket *pSentNext = pSentRelPackets->pNext;
		SVSUTIL_ASSERT (! pSentRelPackets->pPacket->pQueue->qp.bIsIncoming);
#if defined (SC_VERBOSE)
        scerror_DebugOut (VERBOSE_MASK_SESSION, L"Returning reliable unacked message %08x ptr %08x to %s\n", pSentRelPackets->pPacket->uiMessageID, pSentRelPackets->pPacket, pSentRelPackets->pPacket->pQueue->lpszFormatName);
#endif
        pSentRelPackets->pPacket->pQueue->InsertPacket (pSentRelPackets->pPacket);
        svsutil_FreeFixed (pSentRelPackets, gMem->pAckNodeMem);
        pSentRelPackets = pSentNext;
    }
}

void ScSession::FinishSession (void) {
	SVSUTIL_ASSERT (gMem->IsLocked ());

#if defined  (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Finishing session...\n");
#endif

	int fSessStateSav = fSessionState;
	fSessionState = SCSESSION_STATE_EXITING;	// Temporarily so that it will exit without locking
	ipPeerAddr.S_un.S_addr = INADDR_NONE;

	if (s != INVALID_SOCKET) {
		closesocket (s);
		s = INVALID_SOCKET;
	}

	if (hServiceThreadR) {
		if (WaitForSingleObject (hServiceThreadR, SC_THREAD_WAIT2) == WAIT_TIMEOUT) {
			scerror_Inform (MSMQ_SC_ERRMSG_READERKILLED, lpszHostName);
			TerminateThread (hServiceThreadR, 0);
		}

		CloseHandle (hServiceThreadR);
	}

	hServiceThreadR = NULL;

	fSessionState = fSessStateSav;

	if (gMachine->fUseSRMP)
		SRMPCloseSession();

	gSessionMan->ReleaseThreadPair ();

	CloseHandle (hEvent);
	hEvent = NULL;
	CloseHandle (hServiceThreadW);
	hServiceThreadW = NULL;

	memset (&guidDest, 0, sizeof(guidDest));

	uiConnectionStamp	= 0;
	uiAckTimeout		= 0;
	uiStoreAckTimeout	= 0;
	uiMyWindowSize		= 0;
	uiOtherWindowSize   = 0;

	ReturnUnacketPacketsToQueues();

    SVSUTIL_ASSERT (! pSentPackets);
    SVSUTIL_ASSERT (! pSentRelPackets);
}

int ScSession::MakeSessionSect (CSessionSection *pSessSect, ScPacket *pPacket, int fForce) {
	SVSUTIL_ASSERT (sessnum_greater_equal (usRelPacketsReceived, usLastRelAckSent));

	unsigned short usRelFirstUnacked = usLastRelAckSent + 1;
	if (! usRelFirstUnacked)
		++usRelFirstUnacked;

	int iRelPacketsToAck = (signed short)((signed short)usRelPacketsReceived - (signed short)usRelFirstUnacked + 1);
	if (iRelPacketsToAck > STORED_ACK_BITFIELD_SIZE)
		iRelPacketsToAck = STORED_ACK_BITFIELD_SIZE;

	unsigned int   uiRelBitMask = (iRelPacketsToAck < 2) ? 0 : ~((-1) << (iRelPacketsToAck - 1));

	pSessSect->CSessionSection::CSessionSection(usPacketsReceived, iRelPacketsToAck ? usRelFirstUnacked : 0, uiRelBitMask,
												usPacketsSent, usRelPacketsSent, uiMyWindowSize);

    int iPacketSize = 0;

	if (pPacket)
		iPacketSize = pPacket->pImage->sect.pBaseHeader->GetPacketSize();

	if ((fForce) || ((uiAckDue && time_greater_equal (GetTickCount(), uiAckDue)) ||
		sessnum_greater_equal (usPacketsReceived, (unsigned short)(usLastAckSent + uiOtherWindowSize)) ||
		sessnum_greater_equal (usRelPacketsReceived, (unsigned short)(usLastRelAckSent + STORED_ACK_BITFIELD_SIZE)))) {

		if (pPacket) {
			pPacket->pImage->sect.pBaseHeader->IncludeSession(TRUE);

			iPacketSize += CSessionSection::CalcSectionSize();
		}

		usLastAckSent = usPacketsReceived;
		usLastRelAckSent = usRelFirstUnacked + iRelPacketsToAck - 1;

        if (usLastRelAckSent == usRelPacketsReceived) {
			uiAckDue      = 0;
			svsutil_ClearAttrTimer (gMem->pTimer, (SVSHandle)hEvent);
		}
	}

	return iPacketSize;
}

int ScSession::SendAck (void) {
	for ( ; ; ) {
		SVSUTIL_ASSERT (gMem->IsLocked ());

        if (((! uiAckDue) || time_less (GetTickCount(), uiAckDue)) &&
			sessnum_less (usPacketsReceived, (unsigned short)(usLastAckSent + uiOtherWindowSize)) &&
			sessnum_less (usRelPacketsReceived,  (unsigned short)(usLastRelAckSent + STORED_ACK_BITFIELD_SIZE)))
			return TRUE;

#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Sending ACK for %d(%d)th packet(s) for %s\n", usPacketsReceived, usRelPacketsReceived, lpszHostName);
#endif

		const int iPacketSize = sizeof(CBaseHeader) + sizeof(CInternalSection) + sizeof(CSessionSection);
		char __buf[iPacketSize];

		CBaseHeader *pBase = (CBaseHeader *)__buf;

		pBase->CBaseHeader::CBaseHeader (iPacketSize);
		pBase->SetType(FALCON_INTERNAL_PACKET);
		pBase->IncludeSession(TRUE);

		CInternalSection *pInternalSect = (CInternalSection *)pBase->GetNextSection ();
		pInternalSect->CInternalSection::CInternalSection (INTERNAL_SESSION_PACKET);

		CSessionSection *pSessionSect = (CSessionSection *)pInternalSect->GetNextSection();

		MakeSessionSect (pSessionSect, NULL, TRUE);

		int iTotalSent = 0;

		gMem->Unlock ();

		while (iTotalSent < iPacketSize) {
			int iSent = send (s, &__buf[iTotalSent], iPacketSize - iTotalSent, 0);
			if (iSent == SOCKET_ERROR) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_SESSION, L"Send failure to %s\n", lpszHostName);
#endif
				gMem->Lock ();
				return FALSE;
			}
			iTotalSent += iSent;
		}

		gMem->Lock ();
	}

	return TRUE;
}

int ScSession::SendUP (ScPacket *pPacket, ScQueue *pQueue) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT (! pQueue->qp.bIsIncoming);

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Sending user packet %08x ptr %08x queue %s to %s\n", pPacket->uiMessageID, pPacket, pQueue->lpszFormatName, lpszHostName);
#endif

	if (pQueue->qp.bTransactional) {
		SVSUTIL_ASSERT (pPacket->iDirEntry >= 0);
		SVSUTIL_ASSERT (pPacket->pNode);

		SVSTNode *pPrevNode = pQueue->pPackets->Prev (pPacket->pNode);
		ScPacket *pPrev = pPrevNode ? (ScPacket *)SVSTree::GetData(pPrevNode) : NULL;

		pPacket->uiPacketState = SCPACKET_STATE_WAITORDERACK;

		++gQueueMan->iPacketsWaitingOrderAck;
		if (gQueueMan->iPacketsWaitingOrderAck == 1)
			svsutil_SetAttrTimer (gMem->pTimer, (SVSHandle)gQueueMan->hOrderAckTimer, gMachine->uiOrderAckScale * SCQMAN_ORDERACKPERIODIC * 1000);

		pPacket->pImage->sect.pXactHeader->SetPrevSeqN (pPrev ? pPrev->uiSeqN : 0);

		++usPacketsSent;
		if (! usPacketsSent)
			++usPacketsSent;

		++usRelPacketsSent;
		if (! usRelPacketsSent)
			++usRelPacketsSent;
	} else {
		SentPacket *pSP = (SentPacket *)svsutil_GetFixed (gMem->pAckNodeMem);

		pSP->pPacket      = pPacket;

		++usPacketsSent;
		if (! usPacketsSent)
			++usPacketsSent;

		if (pPacket->iDirEntry >= 0) {
			++usRelPacketsSent;
			if (! usRelPacketsSent)
				++usRelPacketsSent;

			pSP->usNum = usRelPacketsSent;
			pSP->pNext = pSentRelPackets;

			pSentRelPackets = pSP;
		} else {
			pSP->usNum = usPacketsSent;
			pSP->pNext = pSentPackets;

			pSentPackets = pSP;
		}
	}

	CSessionSection sSessionSect;

	int iBaseSize   = pPacket->pImage->sect.pBaseHeader->GetPacketSize();
	int iPacketSize = MakeSessionSect (&sSessionSect, pPacket, FALSE);

	unsigned int tTRQ = pPacket->pImage->sect.pBaseHeader->GetAbsoluteTimeToQueue ();
	unsigned int tBas = pPacket->pImage->sect.pUserHeader->GetSentTime ();
	unsigned int tNow = scutil_now ();

	if (tTRQ != INFINITE)
		pPacket->pImage->sect.pBaseHeader->SetAbsoluteTimeToQueue (tTRQ - (tNow - tBas));

	//
	//	Duplicate image...
	//
	++gSessionMan->fBusy;

	char *__buf = (char *)g_funcAlloc (iBaseSize, g_pvAllocData);

	if (__buf)
		memcpy (__buf, pPacket->pImage->pvBinary, iBaseSize);

	pPacket->pImage->sect.pBaseHeader->SetAbsoluteTimeToQueue (tTRQ);
	pPacket->pImage->sect.pBaseHeader->IncludeSession (FALSE);

	pQueue->FreeFromMemory (pPacket);

	if (! __buf) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"No memory - failing session %s...\n", lpszHostName);
#endif
		return FALSE;
	}

	gMem->Unlock ();

	int iRes = TRUE;

	int iTotalSent = 0;

	while (iTotalSent < iBaseSize) {
		int iSent = send (s, &__buf[iTotalSent], iBaseSize - iTotalSent, 0);
		if (iSent == SOCKET_ERROR) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Send failure to %s\n", lpszHostName);
#endif
			iRes = FALSE;
			break;
		}

		iTotalSent += iSent;
	}

	g_funcFree (__buf, g_pvFreeData);

	if (iRes) {
		__buf = (char *)&sSessionSect;

		while (iTotalSent < iPacketSize) {
			int iSent = send (s, &__buf[iTotalSent - iBaseSize], iPacketSize - iTotalSent, 0);
			if (iSent == SOCKET_ERROR) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_SESSION, L"Send failure to %s [session section]\n", lpszHostName);
#endif
				iRes = FALSE;
				break;
			}

			iTotalSent += iSent;
		}
	}

	gMem->Lock ();

	--gSessionMan->fBusy;

	return iRes;
}

int ScSession::ExtractNextPacket (ScPacket *&pPacket, ScQueue *&pQueue, ScQueue *pPrevQueue) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT ((! pPrevQueue) || (pPrevQueue->pSess == this));

	pQueue  = NULL;
	pPacket = NULL;

	//
	//	Find non-empty pQueue, and ref it. Reference is kept by this thread until
	//	session end or an ack.
	//
	unsigned int uiNow = scutil_now();

	ScQueueList *pql = gQueueMan->pqlOutgoing;
	if (pPrevQueue) {
		while (pql && pql->pQueue != pPrevQueue)
			pql = pql->pqlNext;

		if (pql)
			pql = pql->pqlNext;

		if (! pql)
			pql = gQueueMan->pqlOutgoing;
	}

	if (! pql)
		return FALSE;

	ScQueueList *pqlStart = pql;

	for ( ; ; ) {
		SVSUTIL_ASSERT (! pql->pQueue->qp.bIsIncoming);
		if ((pql->pQueue->pSess == this) && (! pql->pQueue->pPackets->IsEmpty())) {
			pQueue = pql->pQueue;

			SVSTNode *pMinNode = pQueue->pPackets->Min ();
			int      iMsgBypassed = 0;

			for ( ; ; ) {
				if (! pMinNode)
					break;

				pPacket = (ScPacket *)SVSTree::GetData (pMinNode);

				SVSUTIL_ASSERT ((pPacket->uiPacketState != SCPACKET_STATE_WAITORDERACK) || pQueue->qp.bTransactional);

				if ((pPacket->uiPacketState != SCPACKET_STATE_ALIVE) || did_expire (pPacket->tExpirationTime, uiNow)) {
#if defined (SC_VERBOSE)
					scerror_DebugOut (VERBOSE_MASK_SESSION, L"Packet time-out or non-active %08x ptr %08x queue %s - skipping\n", pPacket->uiMessageID, pPacket, pQueue->lpszFormatName);
#endif
					if (pPacket->uiPacketState == SCPACKET_STATE_WAITORDERACK)
						++iMsgBypassed;

					pMinNode = pQueue->pPackets->Next (pMinNode);
					continue;
				}

				if (iMsgBypassed > (int)gMachine->uiOrderAckWindow) {
#if defined (SC_VERBOSE)
					scerror_DebugOut (VERBOSE_MASK_SESSION, L"Order ack windows is exhausted for queue %s\n", pQueue->lpszFormatName);
#endif
					break;
				}

				if (! iMsgBypassed)					// First message for transactional queue, doesn't mean anything for nontransactional
					pQueue->uiFirstUnackedSeqN = 0;

				SVSUTIL_ASSERT (pPacket->uiPacketState == SCPACKET_STATE_ALIVE);

#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_SESSION, L"Preparing to send packet %08x ptr %08x queue %s\n", pPacket->uiMessageID, pPacket, pQueue->lpszFormatName);
#endif

				if (! pQueue->qp.bTransactional) {
					pQueue->pPackets->Delete (pPacket->pNode);
					pPacket->pNode = NULL;

					if (pPacket->pTimeoutNode) {
						gMem->pTimeoutTree->Delete (pPacket->pTimeoutNode);
						pPacket->pTimeoutNode = NULL;
					}
				}

				return pQueue->BringToMemory (pPacket);
			}
		}

		pql = pql->pqlNext;

		if (! pql)
			pql = gQueueMan->pqlOutgoing;

		if (pql == pqlStart)
			break;
	}

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"No more packets for session %s...\n", lpszHostName);
#endif

	return FALSE;
}

void ScSession::ServiceThreadW (void) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_SESSION, L"Sending thread for %s\n", lpszHostName);
#endif

	//
	//	Note that we can wake up on buzzes from monitoring threads when
	//	messages are arriving from API. Therefore the complexity to make sure
	//	we are waiting out the period.
	//
	unsigned int uiTicks = GetTickCount ();
	gMem->Lock ();
	unsigned int uiSCSESSION_INITIAL_TIMEOUT = gMachine->uiConnectionTimeout * 1000;
	unsigned int uiSCSESSION_IDLE_TIMEOUT = gMachine->uiIdleTimeout * 1000;
	gMem->Unlock ();

	unsigned int uiTimeOut = uiSCSESSION_INITIAL_TIMEOUT;

	for ( ; ; ) {
		int iResp = WaitForSingleObject (hEvent, uiTimeOut);

		if (fSessionState == SCSESSION_STATE_EXITING)
			return;

		if (fSessionState == SCSESSION_STATE_OPERATING)
			break;

		if ((iResp == WAIT_TIMEOUT) ||
				((fSessionState != SCSESSION_STATE_CONNECTING) &&
				 (fSessionState != SCSESSION_STATE_CONNECTED_CLIENT) &&
				 (fSessionState != SCSESSION_STATE_CONNECTED_SERVER))) {
			gMem->Lock ();
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Initialization time-out in sending thread for %s\n", lpszHostName);
#endif
			FinishSession ();

			if (fSessionState != SCSESSION_STATE_WAITING)
				FailConnection ();

			gSessionMan->ReleaseSession (this);
			gMem->Unlock ();
			return;
		}

		unsigned int uiTimeElapsed = GetTickCount() - uiTicks;
		if (uiTimeElapsed >= uiSCSESSION_INITIAL_TIMEOUT) {
			gMem->Lock ();
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Initialization time-out in sending thread for %s\n", lpszHostName);
#endif
			FinishSession ();
			gSessionMan->ReleaseSession (this);
			gMem->Unlock ();
			return;
		}

		uiTimeOut = uiSCSESSION_INITIAL_TIMEOUT - uiTimeElapsed;
	}

	SetEvent (hEvent);	// Do the first scan...

	for ( ; ; ) {
		//
		//	There are three things why we want to wake up. First, we may
		//	need to ack messages.
		//
		//	Second, if no user messages are sent, we want to update the
		//	session info, such as window size.
		//
		//	Third, if there are any user messages, we want to send them,
		//	up to current window size...
		//
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_SESSION, L"Blocking in writing thread servicing session with %s\n", lpszHostName);
#endif

		int iResp = WaitForSingleObject (hEvent, uiSCSESSION_IDLE_TIMEOUT);

		if (fSessionState == SCSESSION_STATE_EXITING)
			return;

		//
		//	If we have time-outed and no activity has been registered, 
		//	we probably just need to shut the connection down.
		//
		gMem->Lock ();

		if (iResp == WAIT_TIMEOUT) {
			fSessionState = SCSESSION_STATE_INACTIVE;
			FinishSession ();
			gSessionMan->ReleaseSession (this);
			gMem->Unlock ();
			return;
		}

		if (fSessionState != SCSESSION_STATE_OPERATING) {
			FinishSession ();
			gSessionMan->ReleaseSession (this);
			gMem->Unlock ();
			return;
		}

		int fSendMore = ((int)uiOtherWindowSize > (signed short)((signed short)usPacketsSent - (signed short)usLastAckReceived));

		ScQueue  *pQueue = NULL;
		while (fSendMore) {
			ScPacket *pPacket;

			if (fSessionState != SCSESSION_STATE_OPERATING) {
				FinishSession ();
				gSessionMan->ReleaseSession (this);
				gMem->Unlock ();
				return;
			}

			if (! ExtractNextPacket (pPacket, pQueue, pQueue))
				break;

			if (! SendUP(pPacket, pQueue)) {
				FinishSession ();
				FailConnection ();
				gSessionMan->ReleaseSession (this);
				gMem->Unlock ();
				return;
			}

			fSendMore = ((int)uiOtherWindowSize > (signed short)((signed short)usPacketsSent - (signed short)usLastAckReceived));
		}

#if defined (SC_VERBOSE)
		if (! fSendMore)
			scerror_DebugOut (VERBOSE_MASK_SESSION, L"Window exhausted for %s\n", lpszHostName);
#endif

		//
		//	Ack is due?
		//
		if (! SendAck()) {
			FinishSession ();
			FailConnection ();
			gSessionMan->ReleaseSession (this);
			gMem->Unlock ();
			return;
		}

		gMem->Unlock ();
	}
}

DWORD WINAPI ScSession::ServiceThreadR_s (void *arg) {
	ScSession        *pSess    = (ScSession *)arg;
	pSess->ServiceThreadR ();
	return 0;
}

DWORD WINAPI ScSession::ServiceThreadW_s (void *arg) {
	ScSession        *pSess    = (ScSession *)arg;
	pSess->ServiceThreadW ();
	return 0;
}


