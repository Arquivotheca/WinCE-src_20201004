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

    scqman.cxx

Abstract:

    Small client queue manager class support


--*/
#include <sc.hxx>

#include <scqman.hxx>
#include <scqueue.hxx>
#include <scsman.hxx>
#include <scpacket.hxx>
#include <sccomp.hxx>
#include <scorder.hxx>

BOOL UriToQueueFormat(const WCHAR *szQueue, DWORD dwQueueChars, QUEUE_FORMAT *pQueue, WCHAR **ppszQueueBuffer);

static void scqman_Signal (void *pvData) {
	SetEvent ((HANDLE)pvData);
}

//
//	Constructor
//
ScQueueManager::ScQueueManager (unsigned int a_uiMessageID) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_INIT, L"Allocating queue manager...\n");
#endif

	pqlIncoming    = NULL;
	pqlOutgoing    = NULL;
	pQueueDLQ      = NULL;
	pQueueJournal  = NULL;
	pQueueOrderAck = NULL;
	pQueueOutFRS   = NULL;

	uiMessageID = a_uiMessageID;

	iPacketsWaitingOrderAck = 0;

	pHandleMem      = svsutil_AllocFixedMemDescr (sizeof(ScHandleInfo), SCQMAN_HANDLE_INCR);
	pHandles	    = new SVSSimpleHandleSystem (SC_MAX_HANDLES);
	hPacketExpired  = CreateEvent (NULL, FALSE, FALSE, NULL);
	hOrderAckTimer  = CreateEvent (NULL, FALSE, FALSE, NULL);
	SetEvent (hPacketExpired);
	SetEvent (hOrderAckTimer);

	fBusy = 0;

	hMainThread = NULL;
}

//
//	Destructor
//
ScQueueManager::~ScQueueManager(void) {
	SVSUTIL_ASSERT (gMem->IsLocked());
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_INIT, L"Deallocating queue manager...\n");
#endif

	SVSUTIL_ASSERT (! hMainThread);

	CloseAllHandles ();

	while (pqlIncoming) {
		ScQueueList *pNext = pqlIncoming->pqlNext;
		delete pqlIncoming->pQueue;
		delete pqlIncoming;
		pqlIncoming = pNext;
	}

	while (pqlOutgoing) {
		ScQueueList *pNext = pqlOutgoing->pqlNext;
		delete pqlOutgoing->pQueue;
		delete pqlOutgoing;
		pqlOutgoing = pNext;
	}

	if (pHandles)
		delete pHandles;

	if (pHandleMem)
		svsutil_ReleaseFixedNonEmpty (pHandleMem);

	if (hPacketExpired)
		::CloseHandle (hPacketExpired);

	if (hOrderAckTimer)
		::CloseHandle (hOrderAckTimer);
}

//
//	Locate routing (external) target
//
//
static int GetRoutingTarget (int fSecureSession, WCHAR *lpszFormatName, ScPacketImage *pImage, WCHAR **ppRouteTarget) { // FALSE == not allowed to route at all
	SVSUTIL_ASSERT(gMem->IsLocked());
	*ppRouteTarget = NULL;

	// First, attempt static routing table, first by target queue name...
	if (gMachine->RouteTo (lpszFormatName, ppRouteTarget))
		return TRUE;

	// Then by destination machine...
	GUID DestQM = *pImage->sect.pUserHeader->GetDestQM ();

	if (gMachine->RouteTo (&DestQM, ppRouteTarget))
		return TRUE;

	// And finally by source machine...
	GUID SourceQM = *pImage->sect.pUserHeader->GetSourceQM ();

	if (gMachine->RouteFrom (&SourceQM, ppRouteTarget))
		return TRUE;

	// If the queue is not specifically mentioned, we can still route it,
	// but only if it arrived on secure connection.
	return fSecureSession;	// if that, route everything
}

static int GetIpRoute (ScPacketImage *pImage, WCHAR **ppRouteTarget, DWORD dwFormatType) { // FALSE == not allowed
	if ((! pImage->flags.fHaveIpv4Addr) || (! gMachine->fResponseByIp))
		return FALSE;

	WCHAR *szFormat;

	if (dwFormatType == SCFILE_QP_FORMAT_HTTP)
		szFormat = MSMQ_SC_FORMAT_DIRECT_HTTP;
	else if (dwFormatType == SCFILE_QP_FORMAT_HTTPS)
		szFormat = MSMQ_SC_FORMAT_DIRECT_HTTPS;
	else
		szFormat = MSMQ_SC_FORMAT_DIRECT_TCP;

	WCHAR szBuffer[_MAX_PATH];

	if (dwFormatType == SCFILE_QP_FORMAT_HTTP || dwFormatType == SCFILE_QP_FORMAT_HTTPS)
		wsprintf (szBuffer, L"%s%d.%d.%d.%d/msmq\\private$\\route$$$",szFormat,pImage->ipSourceAddr.S_un.S_un_b.s_b1, pImage->ipSourceAddr.S_un.S_un_b.s_b2, pImage->ipSourceAddr.S_un.S_un_b.s_b3, pImage->ipSourceAddr.S_un.S_un_b.s_b4);
	else
		wsprintf (szBuffer, L"%s%d.%d.%d.%d\\private$\\route$$$",szFormat,pImage->ipSourceAddr.S_un.S_un_b.s_b1, pImage->ipSourceAddr.S_un.S_un_b.s_b2, pImage->ipSourceAddr.S_un.S_un_b.s_b3, pImage->ipSourceAddr.S_un.S_un_b.s_b4);

	return NULL != (*ppRouteTarget = svsutil_wcsdup (szBuffer));
}

// takes a string in URL form and finds end of Virtual Root portion.  Returns either beginning of queue name or NULL on failure.
static WCHAR * IncrementVirtualRoot(LPWSTR lpszURL) {
	SVSUTIL_ASSERT (gMem->IsLocked());
	LPWSTR szQueueBase = lpszURL;
	DWORD  ccVRootBase;
	int    i = 0;

	// Look at whatever is between first and 2 slashes as base of vroot, run through
	// table of known vroots that map to \SrmpIsapi.dll and if we hit a match return
	// pointer to string past the vroot (which is HTTPD specific) and to first part of MSMQ name.

	// skip past initial /
	if (*szQueueBase == L'\\' || *szQueueBase == L'/')
		szQueueBase++;

	while (*szQueueBase) {
		if ((*szQueueBase == L'\\') || (*szQueueBase == L'/'))
			break;

		szQueueBase++;
	}

	if (! (*szQueueBase))
		return NULL;

	szQueueBase++;
	if (! (*szQueueBase))
		return NULL;

	ccVRootBase = (szQueueBase - lpszURL);

	for (i = 0; i < MAX_VROOTS; i++) {
		if (! gMachine->VRootList[i].wszVRoot)
			return NULL;

		if ((ccVRootBase == gMachine->VRootList[i].ccVRoot) && (0 == wcsnicmp(gMachine->VRootList[i].wszVRoot,lpszURL,ccVRootBase)))
			return szQueueBase;
	}
	return NULL;
}

static ScQueue *CrackQueueName (WCHAR *lpszFormatName, ScQueueParms &qp, ScQueueList *pqlIncoming) {
	WCHAR *lpszQueueName = NULL;
	ScQueue *pQueue = NULL;
    WCHAR szHttpDup[_MAX_PATH];
	WCHAR *pszHttpDup = szHttpDup;

	SVSUTIL_ASSERT (qp.bIsIncoming == TRUE);

	if (gMachine->fUseSRMP && ((SCFILE_QP_FORMAT_HTTP == qp.bFormatType) || (SCFILE_QP_FORMAT_HTTPS == qp.bFormatType))) {
		

		int cchNameLen = wcslen (lpszFormatName) + 1;
		if (cchNameLen > SVSUTIL_ARRLEN(szHttpDup)) {
			pszHttpDup = (WCHAR *)g_funcAlloc (cchNameLen * sizeof(WCHAR), g_pvAllocData);
			if (! pszHttpDup) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_FATAL, L"Can't allocate %d bytes for queue name processing\n", cchNameLen * sizeof(WCHAR));
#endif
				return NULL;
			}
		}

		memcpy (pszHttpDup, lpszFormatName, cchNameLen * sizeof(WCHAR));
		scutil_ReplaceBackSlashesWithSlashes(pszHttpDup);

		lpszQueueName = pszHttpDup + ((SCFILE_QP_FORMAT_HTTP == qp.bFormatType) ? SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_HTTP) : SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_HTTPS));
		lpszQueueName = wcschr (lpszQueueName, L'/');	// skip past the host name
		if (lpszQueueName) {
			lpszQueueName = IncrementVirtualRoot(lpszQueueName);

			if (lpszQueueName)
				lpszQueueName = wcsrchr (lpszQueueName, L'/');
		}
	} else
		lpszQueueName = wcsrchr (lpszFormatName, L'\\');

	if (lpszQueueName) {
		lpszQueueName += 1;
		WCHAR *lpszHashed = svsutil_StringHashCheck (gMem->pStringHash, lpszQueueName);
		if (lpszHashed) {
			ScQueueList *pql = pqlIncoming;
			while (pql) {
				if ((pql->pQueue->lpszQueueName == lpszHashed) && (! pql->pQueue->qp.bIsJournal) &&
								(! pql->pQueue->qp.bIsDeadLetter) && (! pql->pQueue->qp.bIsMachineJournal)) {
					pQueue = pql->pQueue;
					break;
				}

				pql = pql->pqlNext;
			}
		}
	}

	if (pszHttpDup != szHttpDup)
		g_funcFree (pszHttpDup, g_pvFreeData);

	return pQueue;
}

ScQueue *ScQueueManager::FindOrMakeOutgoingQueue(LPWSTR lpszFormatName) {
	ScQueue *pQueue = FindOutgoingByFormat(lpszFormatName);
	if (! pQueue) {
		ScQueueParms qp;
		memset (&qp, 0, sizeof(qp));

		qp.uiQuotaK = (unsigned int)gMachine->uiDefaultOutQuotaK;
		qp.bIsRouterQueue = TRUE;
		pQueue = MakeOutgoingQueue (lpszFormatName, &qp, NULL);
	}
	return pQueue;
}

ScQueue *ScQueueManager::FindQueueByPacketImage (ScPacketImage *pImage, int fResponse) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	QUEUE_FORMAT qf;
	WCHAR *lpszFwdQueue = NULL;
	BOOL  fFwdQueue     = FALSE;

	// We have an SRMP message with <fwd><via> specifying the next hop, this is queue to put packet into.
	WCHAR *szNextHop = pImage->sect.pFwdViaHeader ? GetNextHopOnFwdList(pImage) : NULL;

	if (szNextHop) {
		if (! UriToQueueFormat(szNextHop,wcslen(szNextHop),&qf,&lpszFwdQueue))
			return NULL;
	}
	else
		pImage->sect.pUserHeader->GetDestinationQueue (&qf);

	if (qf.Suffix() != QUEUE_SUFFIX_TYPE_NONE)
		return NULL;

	// We are direct now. 
	WCHAR *lpszFormatName = scutil_QFtoString (&qf);
	if (lpszFwdQueue) {
		g_funcFree(lpszFwdQueue,g_pvFreeData);
		fFwdQueue = TRUE;
	}

	if (! lpszFormatName)
		return NULL;

	ScQueue *pQueue = NULL;

	if (qf.GetType() != QUEUE_FORMAT_TYPE_DIRECT) {	// Non-direct queues can only be routed by endpoints or put into OutFRS.
		WCHAR *lpszRouteTarget = NULL;

		if (GetRoutingTarget (pImage->flags.fSecureSession, lpszFormatName, pImage, &lpszRouteTarget)) {
			if (lpszRouteTarget) {		// Since we can't host non-direct queues, don't bother looking for incoming...
				pQueue = FindOutgoingByFormat (lpszRouteTarget);

				if (! pQueue) {
					ScQueueParms qp;

					memset (&qp, 0, sizeof(qp));

					qp.uiQuotaK = (unsigned int)gMachine->uiDefaultOutQuotaK;
					qp.bIsRouterQueue = TRUE;

					pQueue = MakeOutgoingQueue (lpszRouteTarget, &qp, NULL);
				}
			} else if (!fFwdQueue)
				pQueue = pQueueOutFRS;
		}

		g_funcFree (lpszFormatName, g_pvFreeData);
		return pQueue;
	}

	ScQueueParms qp;

	memset (&qp, 0, sizeof(qp));

	WCHAR *lpszHostName = NULL;
	WCHAR *lpszQueueName = NULL;

	if (scutil_ParseNonLocalDirectFormatName (lpszFormatName, lpszHostName, lpszQueueName, &qp)) { // Failure == Local or error
		SVSUTIL_ASSERT (qp.bIsIncoming == FALSE);

		g_funcFree (lpszHostName, g_pvFreeData);
		g_funcFree (lpszQueueName, g_pvFreeData);

		WCHAR *lpszRouteTarget = NULL;
		int fFreeRouteTarget = FALSE;

		if (! GetRoutingTarget (pImage->flags.fSecureSession, lpszFormatName, pImage, &lpszRouteTarget)) {
			if (fFwdQueue || (!(fResponse && (fFreeRouteTarget = GetIpRoute (pImage, &lpszRouteTarget,qp.bFormatType))))) {
				g_funcFree (lpszFormatName, g_pvFreeData);
				return NULL;
			}
		}

		pQueue = FindOrMakeOutgoingQueue (lpszRouteTarget ? lpszRouteTarget : lpszFormatName);
		g_funcFree (lpszFormatName, g_pvFreeData);

		if (fFreeRouteTarget)
			g_funcFree (lpszRouteTarget, g_pvFreeData);

		return pQueue;
	}

	pQueue = CrackQueueName(lpszFormatName, qp, pqlIncoming);
	g_funcFree (lpszFormatName, g_pvFreeData);
	return pQueue;
}

//
//	Locate incoming queue by format name
//
ScQueue *ScQueueManager::FindIncomingByFormat (WCHAR *lpszFormatName, int *pfQueueType) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	WCHAR *lpszHashed = svsutil_StringHashCheck (gMem->pStringHash, lpszFormatName);

	if (! lpszHashed) {
		ScQueueParms qpp;
		ScQueue *pQueue = NULL;
		WCHAR *lpszHostName = NULL;
		WCHAR *lpszQueueName = NULL;
		memset (&qpp, 0, sizeof(qpp));

		// see if it's local HTTP queue
		if (scutil_ParseNonLocalDirectFormatName (lpszFormatName, lpszHostName, lpszQueueName, &qpp)) { // failure means it's local, or an error
			if (lpszHostName)
				g_funcFree (lpszHostName, g_pvFreeData);

			if (lpszQueueName)
				g_funcFree (lpszQueueName, g_pvFreeData);

			return NULL;
		}

		if (qpp.bIsIncoming)
			pQueue = CrackQueueName (lpszFormatName, qpp, pqlIncoming);

		if (pQueue && pfQueueType)
			*pfQueueType = qpp.bFormatType;

		return pQueue;
	}

	ScQueueList *pql = pqlIncoming;
	while (pql) {
		if (pql->pQueue->lpszFormatName == lpszHashed)
			return pql->pQueue;

		pql = pql->pqlNext;
	}

	return NULL;
}

//
//	Locate outgoing queue by format name
//
ScQueue *ScQueueManager::FindOutgoingByFormat (WCHAR *lpszFormatName) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	WCHAR *lpszHashed = svsutil_StringHashCheck (gMem->pStringHash, lpszFormatName);

	if (! lpszHashed)
		return NULL;

	ScQueueList *pql = pqlOutgoing;
	while (pql) {
		if (pql->pQueue->lpszFormatName == lpszHashed)
			return pql->pQueue;

		pql = pql->pqlNext;
	}

	return NULL;
}

ScQueue *ScQueueManager::FinishCreation (ScQueue *pQueue, ScQueue *pJournal, int fDelFiles) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	int fError = FALSE;

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_QUEUE, L"Finishing creation of a queue...\n");
#endif

	if ((! pQueue) || (! pQueue->fInitialized) || (pQueue->qp.bHasJournal &&
							((! pJournal) || (! pJournal->fInitialized) ||
							(! pJournal->qp.bIsJournal)))) {

#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_QUEUE, L"Queue creation failed because of %s...\n", (! pQueue) ? L"alloc failure" : ((! pQueue->fInitialized) ? L"init failure" : L"flag inconsistency"));
#endif
		fError = TRUE;
	}

	//
	//	Synchronization on queue is not required yet
	//	because queue pointer has not yet been published.
	//
	//	There is no way for threads to find out about it.
	//

	//
	//	Now publish it...
	//
	while (! fError) {
		if (pQueue->qp.bIsIncoming) {
			ScQueueList *pqlNew = new ScQueueList (pQueue, pqlIncoming);
			if (! pqlNew) {
				fError = TRUE;
				break;
			}

			pqlIncoming = pqlNew;

			if (pJournal) {
				pqlNew = new ScQueueList (pJournal, pqlIncoming);
				if (! pqlNew) {
					pqlNew = pqlIncoming;
					pqlIncoming = pqlIncoming->pqlNext;
					pqlIncoming->pqlPrev = NULL;

					delete pqlNew;
					fError = TRUE;
					break;
				}

				pqlIncoming = pqlNew;
				pQueue->pJournal = pJournal;
			}
		} else {
			SVSUTIL_ASSERT (! pJournal);
			ScQueueList *pqlNew = new ScQueueList (pQueue, pqlOutgoing);
			if (pqlNew)
				pqlOutgoing = pqlNew;
			else
				fError = TRUE;
		}

		break;
	}

	if (fError) {
		if (pQueue) {
			if (fDelFiles && pQueue->sFile)
				pQueue->sFile->Delete (FALSE);

			delete pQueue;
		}

		if (pJournal) {
			if (fDelFiles && pJournal->sFile)
				pJournal->sFile->Delete (FALSE);

			delete pJournal;
		}

		return NULL;
	}

	return pQueue;
}

ScQueue *ScQueueManager::MakeQueueFromFile (WCHAR *lpszFileName, HRESULT *phr) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_QUEUE, L"Making queue by file name\n", lpszFileName);
#endif

	ScQueue *pQueueProto = new ScQueue (lpszFileName, phr);
	ScQueue *pJournalProto = NULL;

	if (pQueueProto && pQueueProto->qp.bHasJournal) {
		WCHAR *lpszJournalName = svsutil_wcsdup (lpszFileName);
		WCHAR *lpszExt = wcsrchr (lpszJournalName, L'.');
		if (wcsicmp (lpszExt, MSMQ_SC_EXT_IQ) == 0) {
			wcscpy (lpszExt, MSMQ_SC_EXT_JQ);
			pJournalProto = new ScQueue (lpszJournalName, phr);
		}
		g_funcFree (lpszJournalName, g_pvFreeData);
	}
	return FinishCreation (pQueueProto, pJournalProto, FALSE);
}

ScQueue *ScQueueManager::MakeIncomingQueue (WCHAR *a_lpszPathName, WCHAR *a_lpszLabel, ScQueueParms *a_pqp, unsigned int uiJournalQuote, HRESULT *phr) {
	SVSUTIL_ASSERT (gMem->IsLocked());
	SVSUTIL_ASSERT (a_pqp->bIsIncoming);

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_QUEUE, L"Making incoming queue by pathname %s\n", a_lpszPathName);
#endif

	ScQueue *pJournalProto = NULL;
	ScQueue *pQueueProto = new ScQueue (a_lpszPathName, a_lpszLabel, a_pqp, phr);
	if (pQueueProto && pQueueProto->fInitialized) {
		if (! pQueueProto->qp.bIsIncoming)			// Can't be not incoming. Destroy it.
			pQueueProto->fInitialized = FALSE;
		else if (pQueueProto->qp.bHasJournal) {
			ScQueueParms qp = *a_pqp;
			qp.bIsJournal   = TRUE;
			qp.bHasJournal  = FALSE;
			qp.bIsJournalOn = FALSE;
			qp.uiQuotaK     = uiJournalQuote;

			pJournalProto = new ScQueue (a_lpszPathName, a_lpszLabel, &qp, phr);
		}
	}
	return FinishCreation (pQueueProto, pJournalProto, TRUE);
}

ScQueue *ScQueueManager::MakeOutgoingQueue (WCHAR *a_lpszFormatName, ScQueueParms *a_pqp, HRESULT *phr) {
	SVSUTIL_ASSERT (gMem->IsLocked());
	SVSUTIL_ASSERT (! a_pqp->bIsIncoming);

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_QUEUE, L"Making outgoing queue by format %s\n", a_lpszFormatName);
#endif

	return FinishCreation (new ScQueue (a_lpszFormatName, a_pqp, phr), NULL, TRUE);
}

SVSHandle ScQueueManager::AllocHandle (ScHandleInfo *a_pHInfo) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	SVSUTIL_ASSERT ((a_pHInfo->uiHandleType == SCQMAN_HANDLE_QUEUE) || (a_pHInfo->uiHandleType == SCQMAN_HANDLE_CURSOR));

	ScHandleInfo *pHInfo = (ScHandleInfo *)svsutil_GetFixed (pHandleMem);

	if (! pHInfo)
		return SVSUTIL_HANDLE_INVALID;

	*pHInfo = *a_pHInfo;

	SVSHandle h = pHandles->AllocHandle (pHInfo);

	if (h == SVSUTIL_HANDLE_INVALID)
		svsutil_FreeFixed (pHInfo, pHandleMem);

	return h;
}

int ScQueueManager::CloseHandle (SVSHandle h) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	CloseOverlappedHandle (h);

	ScHandleInfo *pHInfo = (ScHandleInfo *)pHandles->CloseHandle (h);

	if (! pHInfo)
		return FALSE;

	if (pHInfo->uiHandleType == SCQMAN_HANDLE_CURSOR) {
		if (pHInfo->c.pNode) {
			ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pHInfo->c.pNode);
			pPacket->DelRef ();
			if ((pPacket->GetRefCount() == 1) && (pPacket->uiPacketState == SCPACKET_STATE_DEAD))
				pPacket->pQueue->DisposeOfPacket (pPacket);
		}
	} else if (pHInfo->uiHandleType == SCQMAN_HANDLE_QUEUE) {
		if (pHInfo->q.uiAccess != MQ_SEND_ACCESS)
			--pHInfo->pQueue->uiOpenRecv;

		if (pHInfo->q.uiShareMode == MQ_DENY_RECEIVE_SHARE)
			pHInfo->pQueue->fDenyAll = FALSE;

		--pHInfo->pQueue->uiOpen;
	} else
		SVSUTIL_ASSERT (0);

	svsutil_FreeFixed (pHInfo, pHandleMem);

	return TRUE;
}

ScHandleInfo *ScQueueManager::QueryHandle (SVSHandle h) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	return (ScHandleInfo *)pHandles->TranslateHandle (h);
}

struct CloseAllHandlesExch {
	ScQueueManager	*pmgr;
	union {
		ScQueue		*pQueue;
		SVSHandle	hQueue;
		void		*pProcId;
	};
};

int ScQueueManager::CloseAllHandlesForQueue (SVSHandle h, void *pvArg) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	CloseAllHandlesExch *ps = (CloseAllHandlesExch *)pvArg;

	ScHandleInfo *pHInfo = (ScHandleInfo *)ps->pmgr->pHandles->TranslateHandle(h);

	if (! pHInfo)
		return 0;

	if (ps->pQueue && (ps->pQueue != pHInfo->pQueue))
		return 0;

	if (ps->pmgr->CloseHandle (h))
		return 1;

	return 0;
}

int ScQueueManager::CloseAllHandlesForProc (SVSHandle h, void *pvArg) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	CloseAllHandlesExch *ps = (CloseAllHandlesExch *)pvArg;

	ScHandleInfo *pHInfo = (ScHandleInfo *)ps->pmgr->pHandles->TranslateHandle(h);

	SVSUTIL_ASSERT (ps->pProcId);

	if (! pHInfo)
		return 0;

	if (ps->pProcId != pHInfo->pProcId)
		return 0;

	if (ps->pmgr->CloseHandle (h))
		return 1;

	return 0;
}

int ScQueueManager::CloseAllHandlesForQueueHandle (SVSHandle h, void *pvArg) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	CloseAllHandlesExch *ps = (CloseAllHandlesExch *)pvArg;

	ScHandleInfo *pHInfo = (ScHandleInfo *)ps->pmgr->pHandles->TranslateHandle(h);

	if ((! pHInfo) || (pHInfo->uiHandleType != SCQMAN_HANDLE_CURSOR))
		return 0;

	if (ps->hQueue != pHInfo->c.hQueue)
		return 0;

	if (ps->pmgr->CloseHandle (h))
		return 1;

	return 0;
}

void ScQueueManager::CloseAllHandles (ScQueue *pQueue) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	CloseAllHandlesExch s;
	s.pQueue = pQueue;
	s.pmgr = this;

	pHandles->FilterHandles (ScQueueManager::CloseAllHandlesForQueue, (void *)&s);
}

void ScQueueManager::CloseProcHandles (void *pProcId) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	CloseAllHandlesExch s;
	s.pProcId = pProcId;
	s.pmgr    = this;

	pHandles->FilterHandles (ScQueueManager::CloseAllHandlesForProc, (void *)&s);
}

void ScQueueManager::CloseAllHandles (void) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	CloseAllHandles ((ScQueue *)NULL);
}

void ScQueueManager::CloseAllHandles (SVSHandle hQueue) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	CloseAllHandlesExch s;
	s.hQueue = hQueue;
	s.pmgr = this;

	pHandles->FilterHandles (ScQueueManager::CloseAllHandlesForQueueHandle, (void *)&s);
}

struct PacketRefCounter {
	ScQueueManager  *pQueueMan;
	ScPacket		*pPacket;
	int				iNumber;
};

static int scqman_PacketRefCounter(SVSHandle h, void *pvData) {
	PacketRefCounter *pPRC = (PacketRefCounter *)pvData;
	ScHandleInfo     *pHI  = pPRC->pQueueMan->QueryHandle (h);
	if ((pHI->uiHandleType == SCQMAN_HANDLE_CURSOR) && pHI->c.pNode &&
			(SVSTree::GetData(pHI->c.pNode) == pPRC->pPacket))
		++pPRC->iNumber;

	return TRUE;
}

int ScQueueManager::CountPacketRefs (ScPacket *pPacket) {
	PacketRefCounter sPRC;

	sPRC.pQueueMan = this;
	sPRC.pPacket   = pPacket;
	sPRC.iNumber   = 0;

	pHandles->FilterHandles (scqman_PacketRefCounter, &sPRC);

	return sPRC.iNumber;
}

int ScQueueManager::DeleteQueue (ScQueue *pQueue, int fPerm) {	// Calling thread should not hold a ref to pQueue!
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_QUEUE, L"Deleting queue %s - %s\n", pQueue->lpszFormatName, fPerm ? L"permanently" : L"memory image only");
#endif

	CloseAllHandles (pQueue);

	if (fPerm) {
		pQueue->Purge (MQMSG_CLASS_NACK_Q_DELETED);
		gSeqMan->DeleteQueue (pQueue);
	}

	//
	//	Delete queue from the list
	//
	if (pQueue->qp.bIsIncoming) {
		ScQueueList *pql = pqlIncoming;
		while (pql) {
			if (pql->pQueue == pQueue) {
				if (pql->pqlPrev)
					pql->pqlPrev->pqlNext = pql->pqlNext;
				else
					pqlIncoming = pql->pqlNext;

				if (pql->pqlNext)
					pql->pqlNext->pqlPrev = pql->pqlPrev;

				delete pql;
				break;
			}

			pql = pql->pqlNext;
		}
		if (pQueue->qp.bIsJournal) {
			pql = pqlIncoming;
			while (pql) {
				if (pql->pQueue->pJournal == pQueue) {
					pql->pQueue->qp.bHasJournal  = FALSE;
					pql->pQueue->qp.bIsJournalOn = FALSE;
					pql->pQueue->pJournal = NULL;
					break;
				}
				pql = pql->pqlNext;
			}
		}
	} else {
		ScQueueList *pql = pqlOutgoing;
		while (pql) {
			if (pql->pQueue == pQueue) {
				if (pql->pqlPrev)
					pql->pqlPrev->pqlNext = pql->pqlNext;
				else
					pqlOutgoing = pql->pqlNext;

				if (pql->pqlNext)
					pql->pqlNext->pqlPrev = pql->pqlPrev;

				delete pql;
				break;
			}

			pql = pql->pqlNext;
		}
	}

	if (pQueue == pQueueDLQ)
		pQueueDLQ = NULL;
	else if (pQueue == pQueueJournal)
		pQueueJournal = NULL;
	else if (pQueue == pQueueOutFRS)
		pQueueOutFRS = NULL;
	else if (pQueue == pQueueOrderAck)
		pQueueOrderAck = NULL;

	if (fPerm)
		pQueue->sFile->Delete (TRUE);

	delete pQueue;

	return TRUE;
}

static WCHAR *AllocOrderQueue (WCHAR *szHostName, GUID *pguid) {
	WCHAR *pszOrderQueue = NULL;
	WCHAR *szDirectType = MSMQ_SC_FORMAT_DIRECT_OS;

	if (szHostName) {	// Immediate source! Use direct={os|tcp}:hostname\private\order$
		//
		//	Construct format name for response queue...
		//
		int iSize = (wcslen (szDirectType) +							// direct={os|tcp}:
				wcslen (szHostName) +									// hostname       //
				1 +														// \              //
				SVSUTIL_CONSTSTRLEN (MSMQ_SC_PATHNAME_PRIVATE) +		// private$       //
				1 +														// \              //
				SVSUTIL_CONSTSTRLEN (MSMQ_SC_ORDERQUEUENAME) +			// order_queue$   //
				1														// \0
				) * sizeof(WCHAR);

		pszOrderQueue = (WCHAR *)g_funcAlloc (iSize, g_pvAllocData);

		if (! pszOrderQueue) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"Can't allocate %d bytes for hostname of transactional package\n", iSize);
#endif
			return NULL;
		}

		wcscpy (pszOrderQueue, szDirectType);
		wcscat (pszOrderQueue, szHostName);
		wcscat (pszOrderQueue, L"\\" MSMQ_SC_PATHNAME_PRIVATE L"\\" MSMQ_SC_ORDERQUEUENAME);
	} else {	// Routed source. Use private=GUID\order$
		int iSize = (SVSUTIL_CONSTSTRLEN (MSMQ_SC_FORMAT_PRIVATE) +	// private=     //
				SC_GUID_STR_LENGTH +									// Machine GUID	//
				1 +														// \			//
				8 +														// Hex Index	//
				1														// \0			//
				) * sizeof(WCHAR);

		pszOrderQueue = (WCHAR *)g_funcAlloc (iSize, g_pvAllocData);

		if (! pszOrderQueue) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"Can't allocate %d bytes for hostname of transactional package\n", iSize);
#endif
			return NULL;
		}

		wsprintf (pszOrderQueue, MSMQ_SC_FORMAT_PRIVATE SC_GUID_FORMAT L"\\%08x", SC_GUID_ELEMENTS(pguid), ORDER_QUEUE_PRIVATE_INDEX);
	}

	return pszOrderQueue;
}

int ScQueueManager::StoreTransactionalPacket (ScQueue *pQueue, ScPacketImage *pImage, WCHAR *lpszHostName, GUID *pSourceQM) {
	SVSUTIL_ASSERT (pQueue->qp.bIsIncoming && pQueue->qp.bTransactional);

	if (! pImage->sect.pXactHeader)
		return FALSE;

	CEodHeader *pEodHeader = pImage->sect.pEodHeader;

	LONGLONG	 llNewSeqID   = pImage->sect.pXactHeader->GetSeqID ();
	unsigned int uiNewSeqNum  = pImage->sect.pXactHeader->GetSeqN ();
	unsigned int uiPrevSeqNum = pImage->sect.pXactHeader->GetPrevSeqN ();

	GUID guidStreamId;
	GUID guidQueueName;

	// 1. Extract all names, convert to GUIDs.

	if (pEodHeader) {
		WCHAR *pStreamURI = (WCHAR *)pEodHeader->GetPointerToStreamId ();
		if (! pStreamURI)
			return FALSE;

		if (! gSeqMan->HashStringToGUID (pStreamURI, &guidStreamId))
			return FALSE;
	} else {	// Must be binary!
		if ((! llNewSeqID) || (! uiNewSeqNum) || (uiPrevSeqNum >= uiNewSeqNum))
			return FALSE;
		guidStreamId = *(GUID *)pImage->sect.pUserHeader->GetSourceQM ();
	}

	QUEUE_FORMAT qf;
	pImage->sect.pUserHeader->GetDestinationQueue (&qf);
	if (qf.Suffix() != QUEUE_SUFFIX_TYPE_NONE)
		return FALSE;

	WCHAR *lpszTransactName = scutil_QFtoString (&qf);
	if (! lpszTransactName)
		return FALSE;

	if (! gSeqMan->HashStringToGUID (lpszTransactName, &guidQueueName)) {
		g_funcFree (lpszTransactName, g_pvFreeData);
		return FALSE;
	}

	g_funcFree (lpszTransactName, g_pvFreeData);

	ScOrderSeq *pSeq = gSeqMan->HashOrderSequence (pQueue, &guidStreamId, &guidQueueName, NULL, 0);
	if (! pSeq) {	// Need to create order ack name
		GUID guidOrderAck;

		WCHAR *pszOrderQueue;
		int fFreeMe = FALSE;

		if (pEodHeader && pEodHeader->GetOrderQueueSizeInBytes())
			pszOrderQueue = (WCHAR *)pEodHeader->GetPointerToOrderQueue ();
		else if ((!pSourceQM) || pEodHeader)
			return FALSE;
		else { // Binary protocol - build string for order queue out of host name and guid.
			pszOrderQueue = AllocOrderQueue (guidStreamId == *pSourceQM ? lpszHostName : NULL, &guidStreamId);
			if (! pszOrderQueue)
				return FALSE;

			fFreeMe = TRUE;
		}

		if (! gSeqMan->HashStringToGUID (pszOrderQueue, &guidOrderAck)) {
			if (fFreeMe)
				g_funcFree (pszOrderQueue, g_pvFreeData);
			return FALSE;
		}

		if (fFreeMe)
			g_funcFree (pszOrderQueue, g_pvFreeData);

		unsigned int uiSequenceFlags = 0;

		if (pImage->flags.fSecureSession)
			uiSequenceFlags |= SCPERSISTORDER_FLAGS_SECURESESSION;

		pSeq = gSeqMan->HashOrderSequence (pQueue, &guidStreamId, &guidQueueName, &guidOrderAck, uiSequenceFlags);
	}

	if (! pSeq) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Can't create sequence...\n");
#endif
		return FALSE;
	}

	if (pImage->flags.fHaveIpv4Addr)
		pSeq->ipAddr = pImage->ipSourceAddr;
	else
		pSeq->ipAddr.S_un.S_addr = INADDR_NONE;

	if ((pEodHeader && (uiPrevSeqNum <= pSeq->p->uiCurrentSeqN) && (uiNewSeqNum > pSeq->p->uiCurrentSeqN)) ||
		((pSeq->p->llCurrentSeqID < llNewSeqID) && (uiPrevSeqNum == 0)) ||
		((pSeq->p->llCurrentSeqID == llNewSeqID) &&
		(uiPrevSeqNum <= pSeq->p->uiCurrentSeqN) &&
		(uiNewSeqNum > pSeq->p->uiCurrentSeqN))) {
		pSeq->p->llCurrentSeqID = llNewSeqID;
		pSeq->p->uiCurrentSeqN  = uiNewSeqNum;

		pImage->hkOrderKey = ++pQueue->hkReceived;
		pImage->hkOrderKey |= pImage->sect.pBaseHeader->GetPriority() << SCPACKET_ORD_TIMEBITS;
		ScPacket *pPacket = pQueue->MakePacket (pImage, -1, TRUE);

		if (! pPacket) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"User packet from %s cannot be put into %s...\n", lpszHostName, pQueue->lpszFormatName);
#endif
			RejectPacket (pImage, MQMSG_CLASS_NACK_Q_EXCEED_QUOTA);
			ForwardTransactionalResponse (pImage, MQMSG_CLASS_NACK_Q_EXCEED_QUOTA, NULL, NULL);
			g_funcFree (pImage, g_pvFreeData);
		} else
			gQueueMan->AcceptPacket (pPacket, MQMSG_CLASS_ACK_REACH_QUEUE, pQueue);
	} else
		g_funcFree (pImage, g_pvFreeData);

	gSeqMan->SaveSequence (pSeq);

	return TRUE;
}

int ScQueueManager::SendOrderAck (ScOrderSeq *pSeq) {
// Spec limitation: SRMP does not attach a <fwd> path to an admin queue. We can only send directly, which means that
// transactional queues does not work in WS-routing environment.

	WCHAR *szOrderQueueName = gSeqMan->GetStringFromGUID (&pSeq->p->guidOrderAck);

	if (! szOrderQueueName)
		return FALSE;

	unsigned int uiSeqN = pSeq->p->uiCurrentSeqN;
	LONGLONG llSeqID = pSeq->p->llCurrentSeqID;
	WCHAR *lpszSequenceId = gSeqMan->GetStringFromGUID (&pSeq->p->guidStreamId);
	ScQueue *pQueueOrig = pSeq->pQueue;

	SVSUTIL_ASSERT (gMem->IsLocked());
	SVSUTIL_ASSERT (pQueueOrig->qp.bIsIncoming);

	ScOrderAckBody oab;
	memset (&oab, 0, sizeof(oab));
	oab.m_liSeqID = llSeqID;
	oab.m_ulSeqN  = uiSeqN;

	QUEUE_FORMAT qfDest;
	QUEUE_FORMAT qfResp;

	if ((! scutil_StringToQF (&qfDest, szOrderQueueName)) ||
		(! scutil_StringToQF (&qfResp,  pQueueOrig->lpszFormatName))) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Sending order ACK - can't parse either %s or %s\n", szOrderQueueName, pQueueOrig->lpszFormatName);
#endif
		return FALSE;
	}

	int iHeaderSize = CBaseHeader::CalcSectionSize ();
	int iUserSize   = CUserHeader::CalcSectionSize (NULL, NULL, NULL, &qfDest, NULL, &qfResp);
	int iPropSize   = CPropertyHeader::CalcSectionSize (SVSUTIL_ARRLEN (SCQMAN_ORDER_ACK_TITLE), 0, sizeof(oab));
	int iEodAckSize = lpszSequenceId ? CEodAckHeader::CalcSectionSize (sizeof(WCHAR) * (1 + wcslen(lpszSequenceId))) : 0;

	unsigned int uiPacketSize = iHeaderSize + iUserSize + iPropSize + iEodAckSize;
	ScPacketImage *pPacketImage = (ScPacketImage *)g_funcAlloc (sizeof(ScPacketImage) + uiPacketSize, g_pvAllocData);

	if (! pPacketImage) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Sending order ACK - can't alloc %d bytes for packet image\n", sizeof(ScPacketImage) + uiPacketSize);
#endif

		return FALSE;
	}

	void *pvPacketBuffer = (void *)((unsigned char *)pPacketImage + sizeof(ScPacketImage));
	memset (&pPacketImage->sect, 0, sizeof (pPacketImage->sect));
	pPacketImage->allflags = 0;
	pPacketImage->flags.fSecureSession = (pSeq->p->uiBlockFlags & SCPERSISTORDER_FLAGS_SECURESESSION) ? TRUE : FALSE;

	memset (pPacketImage->ucSourceAddr, 0, sizeof(pPacketImage->ucSourceAddr));

	if (pSeq->ipAddr.S_un.S_addr != INADDR_NONE) {
		pPacketImage->flags.fHaveIpv4Addr = TRUE;
		pPacketImage->ipSourceAddr = pSeq->ipAddr;
	}

	CBaseHeader *pBaseHeader = (CBaseHeader *)pvPacketBuffer;
	pBaseHeader->CBaseHeader::CBaseHeader (uiPacketSize);
	pBaseHeader->SetPriority (0);
	pBaseHeader->IncludeDebug (FALSE);
	pBaseHeader->SetTrace (FALSE);
	pBaseHeader->SetAbsoluteTimeToQueue (INFINITE);

	CUserHeader *pUserHeader = (CUserHeader *)pBaseHeader->GetNextSection ();

	GUID nullGuid;
	memset (&nullGuid, 0, sizeof(nullGuid));

	unsigned int uiMsgId = gQueueMan->GetNextID();

	pUserHeader->CUserHeader::CUserHeader (&gMachine->guid, &nullGuid, &qfDest, NULL, &qfResp, uiMsgId);

	pUserHeader->SetTimeToLiveDelta (INFINITE);
	pUserHeader->SetSentTime (scutil_now());
	pUserHeader->SetDelivery (MQMSG_DELIVERY_EXPRESS);
	pUserHeader->IncludeProperty (TRUE);

	CPropertyHeader *pPropHeader = (CPropertyHeader *)pUserHeader->GetNextSection ();

	pPropHeader->CPropertyHeader::CPropertyHeader ();

	pPropHeader->SetAckType (MQMSG_ACKNOWLEDGMENT_NONE);

	pPropHeader->SetTitle (SCQMAN_ORDER_ACK_TITLE, SVSUTIL_ARRLEN (SCQMAN_ORDER_ACK_TITLE));

	pPropHeader->SetBody ((unsigned char *)&oab, sizeof(oab), sizeof(oab));

	pPropHeader->SetPrivLevel (MQMSG_PRIV_LEVEL_NONE);
	pPropHeader->SetHashAlg (0);
	pPropHeader->SetEncryptAlg (0);
	pPropHeader->SetClass (MQMSG_CLASS_ORDER_ACK);

	if (lpszSequenceId) {
		CEodAckHeader *pEodAckHeader = (CEodAckHeader *)pPropHeader->GetNextSection ();
        const USHORT x_EOD_ACK_HEADER_ID = 700;
		LONGLONG llnum = uiSeqN;
		pEodAckHeader->CEodAckHeader::CEodAckHeader (x_EOD_ACK_HEADER_ID, &llSeqID, &llnum, sizeof (WCHAR) * (1 + wcslen (lpszSequenceId)), (unsigned char *)lpszSequenceId);
	}

	if (! pPacketImage->PopulateSections ()) {
		g_funcFree (pPacketImage, g_pvFreeData);
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Send Order Ack -- Can't parse packet...\n");
#endif
		return FALSE;
	}

	ScQueue *pQueueDest = FindQueueByPacketImage (pPacketImage, TRUE);

	if (pQueueDest) {
		pPacketImage->hkOrderKey = ++pQueueDest->hkReceived;
		if (pQueueDest->MakePacket (pPacketImage, -1, TRUE))
			return TRUE;
	}

	g_funcFree (pPacketImage, g_pvFreeData);
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_ORDER, L"Send Order Ack -- Packet creation failed...\n");
#endif
	return FALSE;
}

ScPacketImage *scqman_DupImage (ScPacketImage *pProto) {
	ScPacketImage *pPacketImage = (ScPacketImage *)g_funcAlloc (pProto->AllocSize (), g_pvAllocData);

	if (! pPacketImage)
		return NULL;

	memset (&pPacketImage->sect, 0, sizeof (pPacketImage->sect));
	pPacketImage->allflags = 0;
	pPacketImage->pvBinary = NULL;
	pPacketImage->pvExt    = NULL;

	memcpy (pPacketImage->PersistedStart (), pProto->PersistedStart (), pProto->PersistedSize ());

	if (! pPacketImage->PopulateSections ()) {
		g_funcFree (pPacketImage, g_pvFreeData);
		return NULL;
	}

	return pPacketImage;
}

static ScPacketImage *scqman_ConstructAckImage (ScPacketImage *pProto, QUEUE_FORMAT *pqfAdmin, int fWhy, int fIncludeBody) {
// Spec limitation: SRMP does not attach a <fwd> path to an admin queue. We can only send directly, which means that
// using ACKs/NACKs does not work in WS-routing environment.
	int iBodySize = 0;

	if (fIncludeBody)
		iBodySize = pProto->sect.pPropHeader->GetBodySize ();
	else
		iBodySize = 0;

	int iExtSize   = pProto->sect.pPropHeader->GetMsgExtensionSize ();
	int iTitleSize = pProto->sect.pPropHeader->GetTitleLength ();

	int iHeaderSize = CBaseHeader::CalcSectionSize ();
	int iUserSize   = CUserHeader::CalcSectionSize (NULL, NULL, NULL,
								pqfAdmin, NULL, NULL);
	int iPropSize   = CPropertyHeader::CalcSectionSize (iTitleSize, iExtSize, iBodySize);

	unsigned int uiPacketSize = iHeaderSize + iUserSize + iPropSize;

	ScPacketImage *pPacketImage = (ScPacketImage *)g_funcAlloc (sizeof(ScPacketImage) + uiPacketSize, g_pvAllocData);

	if (! pPacketImage)
		return NULL;

	void *pvPacketBuffer = (void *)((unsigned char *)pPacketImage + sizeof(ScPacketImage));
	memset (&pPacketImage->sect, 0, sizeof (pPacketImage->sect));

	pPacketImage->pvBinary = NULL;
	pPacketImage->pvExt    = NULL;

	pPacketImage->allflags = 0;
	pPacketImage->flags.fSecureSession = pProto->flags.fSecureSession;
	pPacketImage->flags.fHaveIpv4Addr  = pProto->flags.fHaveIpv4Addr;

	memcpy (pPacketImage->ucSourceAddr, pProto->ucSourceAddr, sizeof(pPacketImage->ucSourceAddr));

	pPacketImage->hkOrderKey = pProto->hkOrderKey;

	CBaseHeader *pBaseHeader = (CBaseHeader *)pvPacketBuffer;
	pBaseHeader->CBaseHeader::CBaseHeader (uiPacketSize);
	pBaseHeader->SetPriority (pProto->sect.pBaseHeader->GetPriority());
	pBaseHeader->SetAbsoluteTimeToQueue (INFINITE);

	CUserHeader *pUserHeader = (CUserHeader *)pBaseHeader->GetNextSection ();

	GUID outGuid;
	memset (&outGuid, 0, sizeof(outGuid));

	if (pqfAdmin->GetType () == QUEUE_FORMAT_TYPE_PRIVATE) {
		_OBJECTID qoid = pqfAdmin->PrivateID();
		outGuid = qoid.Lineage;
	}

	pUserHeader->CUserHeader::CUserHeader (&gMachine->guid, &outGuid, pqfAdmin, NULL, NULL, 0);

	pUserHeader->SetTimeToLiveDelta (INFINITE);
	pUserHeader->SetSentTime (scutil_now());
	pUserHeader->SetDelivery (pProto->sect.pUserHeader->GetDelivery());
	pUserHeader->IncludeProperty (TRUE);

	CPropertyHeader *pPropHeader = (CPropertyHeader *)pUserHeader->GetNextSection ();
	pPropHeader->CPropertyHeader::CPropertyHeader ();
	pPropHeader->SetClass (fWhy);
	pPropHeader->SetAckType (MQMSG_ACKNOWLEDGMENT_NONE);
	pPropHeader->SetPrivLevel (MQMSG_PRIV_LEVEL_NONE);
	pPropHeader->SetHashAlg (0);
	pPropHeader->SetEncryptAlg (0);

	OBJECTID msgid;
	pProto->sect.pUserHeader->GetMessageID (&msgid);
	pPropHeader->SetCorrelationID ((unsigned char *)&msgid);
	pPropHeader->SetApplicationTag (pProto->sect.pPropHeader->GetApplicationTag());

	if (iTitleSize > 0)
		pPropHeader->SetTitle (pProto->sect.pPropHeader->GetTitlePtr (), iTitleSize);

	if (iExtSize > 0)
		pPropHeader->SetMsgExtension (pProto->sect.pPropHeader->GetMsgExtensionPtr(), iExtSize);

	if (iBodySize > 0)
		pPropHeader->SetBody (pProto->sect.pPropHeader->GetBodyPtr(), iBodySize, iBodySize);

	pPropHeader->SetBodyType (pProto->sect.pPropHeader->GetBodyType());

	if (! pPacketImage->PopulateSections ()) {
		g_funcFree (pPacketImage, g_pvFreeData);
		return NULL;
	}

	return pPacketImage;
}

static int scqman_NeedAck (unsigned int uiAckType, int iAckClass) {
	if ((iAckClass == MQMSG_CLASS_ACK_REACH_QUEUE) && (uiAckType & MQMSG_ACKNOWLEDGMENT_POS_ARRIVAL))
		return TRUE;

	if ((iAckClass == MQMSG_CLASS_ACK_RECEIVE) && (uiAckType & MQMSG_ACKNOWLEDGMENT_POS_RECEIVE))
		return TRUE;

	if ((uiAckType & (MQMSG_ACKNOWLEDGMENT_NEG_ARRIVAL | MQMSG_ACKNOWLEDGMENT_NEG_RECEIVE)) &&
	    ((iAckClass == MQMSG_CLASS_NACK_BAD_DST_Q) ||
		 (iAckClass == MQMSG_CLASS_NACK_PURGED) ||
		 (iAckClass == MQMSG_CLASS_NACK_REACH_QUEUE_TIMEOUT) ||
		 (iAckClass == MQMSG_CLASS_NACK_Q_EXCEED_QUOTA) ||
		 (iAckClass == MQMSG_CLASS_NACK_ACCESS_DENIED) ||
		 (iAckClass == MQMSG_CLASS_NACK_HOP_COUNT_EXCEEDED) ||
		 (iAckClass == MQMSG_CLASS_NACK_BAD_SIGNATURE) ||
		 (iAckClass == MQMSG_CLASS_NACK_BAD_ENCRYPTION) ||
		 (iAckClass == MQMSG_CLASS_NACK_COULD_NOT_ENCRYPT) ||
		 (iAckClass == MQMSG_CLASS_NACK_NOT_TRANSACTIONAL_Q) ||
		 (iAckClass == MQMSG_CLASS_NACK_NOT_TRANSACTIONAL_MSG) ||
		 (iAckClass == MQMSG_CLASS_NACK_NOT_TRANSACTIONAL_Q) ||
		 (iAckClass == MQMSG_CLASS_NACK_Q_DELETED) ||
		 (iAckClass == MQMSG_CLASS_NACK_Q_PURGED)))
		return TRUE;

	if ((uiAckType & (MQMSG_ACKNOWLEDGMENT_FULL_REACH_QUEUE | MQMSG_ACKNOWLEDGMENT_NACK_REACH_QUEUE)) &&
		(iAckClass == MQMSG_CLASS_NACK_RECEIVE_TIMEOUT))
		return TRUE;

	return FALSE;
}

int ScQueueManager::RejectPacket (ScPacketImage *pImage, int fWhy) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	{
		OBJECTID __ooid;
		pImage->sect.pUserHeader->GetMessageID (&__ooid);
		scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Rejecting message ID %08x " SC_GUID_FORMAT L"\n",
				__ooid.Uniquifier, SC_GUID_ELEMENTS((&__ooid.Lineage)));
	}
#endif

	//
	//	Put in dead letter queue
	//
	if (pQueueDLQ && (pImage->sect.pUserHeader->GetAuditing() & MQMSG_DEADLETTER)) {
		ScPacketImage *pdup = scqman_DupImage (pImage);
		if (pdup) {
			pdup->hkOrderKey = ++pQueueDLQ->hkReceived;
			if (! pQueueDLQ->MakePacket (pdup, -1, TRUE))
				g_funcFree (pdup, g_pvFreeData);
		}
	}

	//
	//	Generate the ack...
	//
	if (! scqman_NeedAck (pImage->sect.pPropHeader->GetAckType (), fWhy))
		return TRUE;

	QUEUE_FORMAT qfAdmin;
	if (! pImage->sect.pUserHeader->GetAdminQueue (&qfAdmin))
		return FALSE;

	ScPacketImage *pPacketImage = scqman_ConstructAckImage (pImage, &qfAdmin, fWhy, TRUE);

	if (! pPacketImage)
		return FALSE;

	ForwardAck (pPacketImage);

	return TRUE;
}

int ScQueueManager::RejectPacket (ScPacket *pPacket, int fWhy, ScQueue *pQueue) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Rejecting packet ptr %08x\n", pPacket);
#endif

	if (pQueue->qp.bIsJournal || pQueue->qp.bIsDeadLetter || pQueue->qp.bIsMachineJournal)
		return FALSE;

	if ((pQueueDLQ && (pPacket->uiAuditType & MQMSG_DEADLETTER)) || scqman_NeedAck (pPacket->uiAckType, fWhy)) {
		if (pQueue->BringToMemory (pPacket))
			return RejectPacket (pPacket->pImage, fWhy);
	}

	return FALSE;
}

int ScQueueManager::JournalPacket (ScPacket *pPacket) {
	if ((pPacket->uiAuditType & MQMSG_JOURNAL) && pQueueJournal) {
		int fWeBroughtItIn = FALSE;

		if (! pPacket->pImage) {
			fWeBroughtItIn = TRUE;
			if (! pPacket->pQueue->BringToMemory (pPacket))
				return FALSE;
		}
#if defined (SC_VERBOSE)
		{
			OBJECTID __ooid;
			pPacket->pImage->sect.pUserHeader->GetMessageID (&__ooid);
			scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Putting in machine journal: message ID %08x " SC_GUID_FORMAT L"\n",
				__ooid.Uniquifier, SC_GUID_ELEMENTS((&__ooid.Lineage)));
		}
#endif

		ScPacketImage *pdup = scqman_DupImage (pPacket->pImage);
		if (pdup) {
			pdup->hkOrderKey = ++pQueueJournal->hkReceived;
			if (! pQueueJournal->MakePacket (pdup, -1, TRUE))
				g_funcFree (pdup, g_pvFreeData);
		}

		if (fWeBroughtItIn)
			pPacket->pQueue->FreeFromMemory (pPacket);
	}

	return TRUE;
}

int ScQueueManager::AcceptPacket (ScPacket *pPacket, int fHow, ScQueue *pQueue) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Accepting packet ptr %08x\n", pPacket);
#endif

	if (pQueue->qp.bIsRouterQueue) {
		gSessionMan->PacketInserted (pQueue->pSess);
		return TRUE;
	}

	if (pQueue->qp.bIsJournal || pQueue->qp.bIsDeadLetter || pQueue->qp.bIsMachineJournal || pQueue->qp.bIsOutFRS)
		return FALSE;

	int fWeBroughtItIn = FALSE;

	//
	//	Put in journal
	//
	ScQueue *pJournal = pQueue->pJournal;
	if (pQueue->qp.bIsJournalOn && (fHow == MQMSG_CLASS_ACK_RECEIVE)) {
		if (! pPacket->pImage) {
			fWeBroughtItIn = TRUE;
			if (! pQueue->BringToMemory (pPacket))
				return FALSE;
		}

#if defined (SC_VERBOSE)
		{
			OBJECTID __ooid;
			pPacket->pImage->sect.pUserHeader->GetMessageID (&__ooid);
			scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Putting in journal: message ID %08x " SC_GUID_FORMAT L"\n",
				__ooid.Uniquifier, SC_GUID_ELEMENTS((&__ooid.Lineage)));
		}
#endif

		ScPacketImage *pdup = scqman_DupImage (pPacket->pImage);
		if (pdup) {
			pdup->hkOrderKey = ++pJournal->hkReceived;
			if (! pJournal->MakePacket (pdup, -1, TRUE))
				g_funcFree (pdup, g_pvFreeData);
		}
	}

	//
	//	Generate the ack...
	//
	if (! scqman_NeedAck (pPacket->uiAckType, fHow)) {
		if (fWeBroughtItIn)
			pQueue->FreeFromMemory (pPacket);
		return TRUE;
	}

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Generating ACK...\n");
#endif

	if (! pPacket->pImage) {
		fWeBroughtItIn = TRUE;
		if (! pQueue->BringToMemory (pPacket))
			return FALSE;
	}

#if defined (SC_VERBOSE)
	{
		OBJECTID __ooid;
		pPacket->pImage->sect.pUserHeader->GetMessageID (&__ooid);
		scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Generating ACK for message ID %08x " SC_GUID_FORMAT L"\n",
			__ooid.Uniquifier, SC_GUID_ELEMENTS((&__ooid.Lineage)));
	}
#endif

	QUEUE_FORMAT qfAdmin;
	if (! pPacket->pImage->sect.pUserHeader->GetAdminQueue (&qfAdmin)) {
		if (fWeBroughtItIn)
			pQueue->FreeFromMemory (pPacket);
		return FALSE;
	}

	ScPacketImage *pPacketImage = scqman_ConstructAckImage (pPacket->pImage, &qfAdmin, fHow, FALSE);
	if (fWeBroughtItIn)
		pQueue->FreeFromMemory (pPacket);

	if (! pPacketImage)
		return FALSE;

	ForwardAck (pPacketImage);

	return TRUE;
}

void ScQueueManager::ForwardTransactionalResponse (ScPacketImage *pImage, int fWhat, WCHAR *szHostName, GUID *pSourceQM) {
	if (! pImage->sect.pXactHeader)
		return;

	if (fWhat == MQMSG_CLASS_NACK_BAD_DST_Q) {
		if (! (pImage->flags.fSecureSession || (gMachine->fResponseByIp && pImage->flags.fHaveIpv4Addr)) )
			return;
	}

	CEodHeader *pEodHeader = pImage->sect.pEodHeader;

	GUID guidStreamId;
	GUID guidQueueName;

	// 1. Extract all names, convert to GUIDs.

	if (pEodHeader) {
		WCHAR *pStreamURI = (WCHAR *)pEodHeader->GetPointerToStreamId ();
		if (! pStreamURI) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Forwarding ack - no stream id\n");
#endif
			return;
		}

		if (! gSeqMan->HashStringToGUID (pStreamURI, &guidStreamId)) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Forwarding ack - stream id doesn't hash\n");
#endif
			return;
		}
	} else	// Must be binary!
		guidStreamId = *(GUID *)pImage->sect.pUserHeader->GetSourceQM ();

	QUEUE_FORMAT qf;
	pImage->sect.pUserHeader->GetDestinationQueue (&qf);

	WCHAR *lpszTransactName = scutil_QFtoString (&qf);

	if (! lpszTransactName) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Forwarding ack - destination name doesn't parse\n");
#endif
		return;
	}

	if (! gSeqMan->HashStringToGUID (lpszTransactName, &guidQueueName)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Forwarding ack - destination name doesn't hash\n");
#endif
		g_funcFree (lpszTransactName, g_pvFreeData);
		return;
	}

	g_funcFree (lpszTransactName, g_pvFreeData);

	WCHAR *szAckQueueName = NULL;
	int   fFreeAckQueueName = FALSE;

	ScOrderSeq *pSeq = gSeqMan->HashOrderSequence (NULL, &guidStreamId, &guidQueueName, NULL, 0);
	if (! pSeq) {
		if (pEodHeader && pEodHeader->GetOrderQueueSizeInBytes()) {
			// SRMP message already has queue name in it.
			szAckQueueName = (WCHAR*) pEodHeader->GetPointerToOrderQueue();
		} else if (pEodHeader) {
			szAckQueueName = NULL;
		}
		else {
			szAckQueueName = AllocOrderQueue ((pSourceQM && (guidStreamId == *pSourceQM)) ? szHostName : NULL, &guidStreamId);
			fFreeAckQueueName = TRUE;
		}
	} else
		szAckQueueName = gSeqMan->GetStringFromGUID (&pSeq->p->guidOrderAck);

	if (! szAckQueueName) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Forwarding ack - don't have order queue name\n");
#endif
		return;
	}

	QUEUE_FORMAT qfa;
	if (! scutil_StringToQF (&qfa, szAckQueueName)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Forwarding ack - can't parse %s\n", szAckQueueName);
#endif
		if (fFreeAckQueueName)
			g_funcFree (szAckQueueName, g_pvFreeData);

		return;
	}

	if (fFreeAckQueueName)
		g_funcFree (szAckQueueName, g_pvFreeData);

	ScPacketImage *pAckImage = scqman_ConstructAckImage (pImage, &qfa, fWhat, FALSE);
	if (! pAckImage) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Forwarding ack - can't construct ack image\n");
#endif
		return;
	}

	pAckImage->sect.pUserHeader->SetSourceQM (&gMachine->guid);

	OBJECTID mid;
	mid.Lineage    = gMachine->guid;
	mid.Uniquifier = gQueueMan->GetNextID();
	pAckImage->sect.pUserHeader->SetMessageID (&mid);

	ScQueue *pQueueAck = FindQueueByPacketImage (pAckImage, TRUE);

	if (pQueueAck) {
		pAckImage->hkOrderKey = ++pQueueAck->hkReceived;

#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Sending to order queue %s...\n", pQueueAck->lpszFormatName);
#endif
	}

	if (! (pQueueAck && pQueueAck->MakePacket (pAckImage, -1, TRUE))) {
		g_funcFree (pAckImage, g_pvFreeData);
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Packet creation failed...\n");
#endif
	}
}

void ScQueueManager::ForwardAck (ScPacketImage *pImage) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Forwarding ACK/NACK...\n");
#endif

	int fFreeNeeded = TRUE;

	for ( ; ; ) {
#if defined (SC_VERBOSE)
		{
			OBJECTID __ooid;
			pImage->sect.pPropHeader->GetCorrelationID ((unsigned char *)&__ooid);
			scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Processing ACK/NACK class %08x correlation ID %08x " SC_GUID_FORMAT L"\n",
				pImage->sect.pPropHeader->GetClass(), __ooid.Uniquifier, SC_GUID_ELEMENTS((&__ooid.Lineage)));
		}
#endif
		QUEUE_FORMAT qf;

		pImage->sect.pUserHeader->GetDestinationQueue (&qf);

		if (qf.Suffix() != QUEUE_SUFFIX_TYPE_NONE)
			break;

		pImage->sect.pUserHeader->SetSourceQM (&gMachine->guid);

		OBJECTID mid;
		mid.Lineage    = gMachine->guid;
		mid.Uniquifier = gQueueMan->GetNextID();
		pImage->sect.pUserHeader->SetMessageID (&mid);

		ScQueue *pQueue = FindQueueByPacketImage (pImage, TRUE);

		if (! pQueue) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_PACKETS | VERBOSE_MASK_QUEUE, L"Cannot find/create admin queue\n");
#endif
			break;
		}

		pImage->hkOrderKey = ++pQueue->hkReceived;

#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Sending to admin queue %s...\n", pQueue->lpszFormatName);
#endif

		if (pQueue->MakePacket (pImage, -1, TRUE))
			fFreeNeeded = FALSE;
		else {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Packet creation failed...\n");
#endif
		}

		break;
	}

	if (fFreeNeeded)
		g_funcFree (pImage, g_pvFreeData);
}

void ScQueueManager::ExpirationCheck (void) {
	SVSUTIL_ASSERT (gMem->IsLocked ());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_PACKETS, L"Checking message expiration...\n");
#endif
	//
	//	Garbage collection takes on two entities:
	//	outgoing queues and incoming queues that
	//	are neither journals nor DLQ
	//
	//	First, check all outgoing queues
	//
	DWORD dwTimeout = INFINITE;
	unsigned int	uiTime = scutil_now ();

	for ( ; ; ) {
		SVSTNode *pNode = gMem->pTimeoutTree->Min ();
		if (! pNode)
			break;

		ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pNode);
		if (did_not_expire (pPacket->tExpirationTime, uiTime)) {
			dwTimeout = (pPacket->tExpirationTime - uiTime) * 1000;
			if (dwTimeout == 0)
				dwTimeout = 500;

			break;
		}

		SVSUTIL_ASSERT (pPacket->pTimeoutNode == pNode);
		gMem->pTimeoutTree->Delete (pNode);
		pPacket->pTimeoutNode = NULL;

		SVSUTIL_ASSERT ((pPacket->uiPacketState == SCPACKET_STATE_ALIVE) || ((pPacket->uiPacketState == SCPACKET_STATE_WAITORDERACK) && pPacket->pQueue->qp.bTransactional && (! pPacket->pQueue->qp.bIsIncoming)));
		SVSUTIL_ASSERT (pPacket->pQueue);
		SVSUTIL_ASSERT (pPacket->pNode);

#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_PACKETS | VERBOSE_MASK_QUEUE, L"Packet ID%08x from queue %s has expired.\n", pPacket->uiMessageID, pPacket->pQueue->lpszFormatName);
#endif

		if (pPacket->pQueue->qp.bTransactional) {
			if (pPacket->pQueue->BringToMemory (pPacket))
				ForwardTransactionalResponse (pPacket->pImage, MQMSG_CLASS_NACK_RECEIVE_TIMEOUT, NULL, NULL);
		}

		RejectPacket (pPacket, MQMSG_CLASS_NACK_RECEIVE_TIMEOUT, pPacket->pQueue);
		pPacket->pQueue->DisposeOfPacket (pPacket);
	}

	if (dwTimeout != INFINITE)
		svsutil_SetAttrTimer (gMem->pTimer, (SVSHandle)hPacketExpired, dwTimeout);
}

void ScQueueManager::PeriodicCheck (void) {
	//
	//	Close all files - each queue will actually manage closeure automagically
	//
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_INIT, L"Running periodic checks...\n");
#endif

	ScQueueList *pql = pqlIncoming;
	while (pql) {
		pql->pQueue->sFile->Close (FALSE);
		pql = pql->pqlNext;
	}

	unsigned int uiNow = scutil_now();
	pql = pqlOutgoing;
	while (pql) {
		ScQueueList *pqlNext = pql->pqlNext;
		ScQueue *pQueue = pql->pQueue;

		pQueue->sFile->Close (FALSE);
		if (time_less (pQueue->tModification + SCQMAN_OUTQUEUE_DELETION, uiNow) &&
			pQueue->pPackets->IsEmpty() && (! pQueue->uiOpen) && (! pQueue->qp.bIsProtected) &&
			((pQueue->pSess->fSessionState == SCSESSION_STATE_INACTIVE) || (pQueue->pSess->fSessionState == SCSESSION_STATE_WAITING)))
			DeleteQueue (pQueue, TRUE);

		pql = pqlNext;
	}

	gSeqMan->Maintain ();
}

void ScQueueManager::OrderAckCheck (void) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_ORDER, L"Checking for order messages for which order ack has not been received.\n");
#endif

	ScQueueList *pql = pqlOutgoing;
	while (pql) {
		ScQueueList *pqlNext = pql->pqlNext;
		ScQueue *pQueue = pql->pQueue;

		if (pQueue->qp.bTransactional) {
			SVSTNode *pNode = pQueue->pPackets->Min ();
			if (pNode) {
				ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pNode);
				if (pPacket->uiPacketState == SCPACKET_STATE_WAITORDERACK) {
					if (pQueue->uiFirstUnackedSeqN && (pPacket->uiSeqN == pQueue->uiFirstUnackedSeqN)) {
						//
						//	We should now resend the sequence...
						//
						while (pPacket) {
							if (pPacket->uiPacketState == SCPACKET_STATE_WAITORDERACK) {
#if defined (SC_VERBOSE)
								scerror_DebugOut (VERBOSE_MASK_ORDER, L"Returning packet " SC_GUID_FORMAT L":%08x, queue %s to active state\n", 
									SC_GUID_ELEMENTS ((&pPacket->guidSourceQM)), pPacket->uiMessageID, pQueue->lpszFormatName);
#endif
								pPacket->uiPacketState = SCPACKET_STATE_ALIVE;

								--iPacketsWaitingOrderAck;

								SVSUTIL_ASSERT (iPacketsWaitingOrderAck >= 0);
							}

							pNode = pQueue->pPackets->Next (pNode);
							pPacket = pNode ? (ScPacket *)SVSTree::GetData (pNode) : NULL;
						}

						gSessionMan->PacketInserted (pQueue->pSess);
					} else
						pQueue->uiFirstUnackedSeqN = pPacket->uiSeqN;
				} else
					pQueue->uiFirstUnackedSeqN = 0;
			}
		}

		pql = pqlNext;
	}

	if (iPacketsWaitingOrderAck)
		svsutil_SetAttrTimer (gMem->pTimer, (SVSHandle)hOrderAckTimer, gMachine->uiOrderAckScale * SCQMAN_ORDERACKPERIODIC * 1000);
}

//
//	This gets called when order ack is received for a queue...
//
void ScQueueManager::ReceiveOrderAck (void) {
	for ( ; ; ) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_ORDER, L"Receiving order ack...\n");
#endif

		SVSTNode *pNode = pQueueOrderAck->pPackets->Min ();
		if (! pNode)
			break;

		ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pNode);
		if (pQueueOrderAck->BringToMemory (pPacket) && pPacket->pImage->sect.pPropHeader->GetClass() == MQMSG_CLASS_ORDER_ACK) {
			QUEUE_FORMAT qf;
			WCHAR *lpszFormatName = NULL;
			ScQueue *pQueue = NULL;
			if ((pPacket->pImage->sect.pPropHeader->GetBodySize() == sizeof(ScOrderAckBody)) &&
				pPacket->pImage->sect.pUserHeader->GetResponseQueue (&qf) &&
				(lpszFormatName = scutil_QFtoString (&qf)) &&
				(pQueue = FindOutgoingByFormat (lpszFormatName))) {
				ScOrderAckBody oa;
				pPacket->pImage->sect.pPropHeader->GetBody ((unsigned char *)&oa, sizeof(oa));
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_ORDER, L"Received order %016I64x:%d\n", oa.m_liSeqID, oa.m_ulSeqN);
#endif
				if (oa.m_liSeqID == pQueue->llSeqID) {
					for ( ; ; ) {
						SVSTNode *pNode2   = pQueue->pPackets->Min ();
						if (! pNode2)
							break;

						ScPacket *pPacket2 = (ScPacket *)SVSTree::GetData (pNode2);
						SVSUTIL_ASSERT (pPacket2->uiSeqN);
						if (pPacket2->uiSeqN > oa.m_ulSeqN)
							break;
#if defined (SC_VERBOSE)
						scerror_DebugOut (VERBOSE_MASK_ORDER, L"Order ack matches packet ptr %08x uuid %08x order no %016I64x:%d\n", pPacket2, pPacket2->uiMessageID, pQueue->llSeqID, pPacket2->uiSeqN);
#endif
						gQueueMan->JournalPacket (pPacket2);
						pQueue->DisposeOfPacket (pPacket2);
					}
				}
#if defined (SC_VERBOSE)
				else {
					scerror_DebugOut (VERBOSE_MASK_ORDER, L"Order ACK seqID %016I64x does not match queue's %016I64x\n", oa.m_liSeqID, pQueue->llSeqID);
				}
#endif
			}
#if defined (SC_VERBOSE)
			else
				scerror_DebugOut (VERBOSE_MASK_ORDER, L"Misplaced order ack - wrong sized body or unknown queue (%s)\n", lpszFormatName ? lpszFormatName : L"no queue");
#endif
			if (lpszFormatName)
				g_funcFree (lpszFormatName, g_pvFreeData);
		}
#if defined (SC_VERBOSE)
		else if (pPacket->pImage)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"Transactional response (class %d) received in queue %s is ignored\n", pPacket->pImage->sect.pPropHeader->GetClass(), pQueueOrderAck->lpszFormatName);
		else
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"No image for packet " SC_GUID_FORMAT L"::%08x in order ack queue %s\n", SC_GUID_ELEMENTS ((&pPacket->guidSourceQM)), pPacket->uiMessageID, pQueueOrderAck->lpszFormatName);
#endif
		pQueueOrderAck->DisposeOfPacket (pPacket);
	}
}

void ScQueueManager::MainThread(void) {
	HANDLE ah[6];

	gMem->Lock ();
	ah[0] = gSessionMan->hBuzz;
	ah[1] = hPacketExpired;
	ah[2] = hOrderAckTimer;
	ah[3] = pQueueOrderAck->hUpdateEvent;
	ah[4] = gSeqMan->hSequencePulse;

	ah[SVSUTIL_ARRLEN (ah) - 1] = ScSessionManager::hNetUP;	// THIS MUST BE THE LAST ONE!!!

	DWORD c = SVSUTIL_ARRLEN (ah);
	if (! ah[c-1])
		--c;

	gMem->Unlock ();

	for ( ; ; ) {
		DWORD dwHowCalled = WaitForMultipleObjects (c, ah, FALSE, SCQMAN_PERIODIC);

		if (dwHowCalled == WAIT_FAILED) {
			SVSUTIL_ASSERT (0);
			break;
		}

		gMem->Lock ();

		if (dwHowCalled == WAIT_OBJECT_0)
			gSessionMan->ConnService ();

		if (dwHowCalled == (WAIT_OBJECT_0 + 1))
			ExpirationCheck ();

		if (dwHowCalled == (WAIT_OBJECT_0 + 2))
			OrderAckCheck ();

		if (dwHowCalled == (WAIT_OBJECT_0 + 3))
			ReceiveOrderAck ();

		if (dwHowCalled == (WAIT_OBJECT_0 + 4))
			gSeqMan->SendAcks ();

		if (dwHowCalled == (WAIT_OBJECT_0 + 5))
			gSessionMan->ConnectToNet (SCSESSION_LANAUP_DELAY);

		if (dwHowCalled == WAIT_TIMEOUT)
			PeriodicCheck ();

		gMem->Unlock ();
	}
}

DWORD WINAPI ScQueueManager::MainThread_s (void *arg) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_INIT, L"Queue manager thread has started...\n");
#endif

	ScQueueManager *pQueueMan = (ScQueueManager *)arg;
	pQueueMan->MainThread();
	return 0;
}

void ScQueueManager::Start (void) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_INIT, L"Starting queue manager main thread...\n");
#endif

	DWORD tid;
	hMainThread = CreateThread (NULL, 0, ScQueueManager::MainThread_s, this, 0, &tid);
}

void ScQueueManager::Stop (void) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_INIT, L"Stopping queue manager main thread...\n");
#endif

	if (hMainThread) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_INIT, L"Terminating queue manager thread...\n");
#endif
		TerminateThread (hMainThread, 0);
		::CloseHandle (hMainThread);
	}

	hMainThread = NULL;
}

void ScQueueManager::SaveMessageID (void) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	HKEY hKey;
	LONG hr = RegOpenKeyEx (HKEY_LOCAL_MACHINE, MSMQ_SC_REGISTRY_KEY, 0, KEY_WRITE, &hKey);

	if (hr != ERROR_SUCCESS)
		return;

	RegSetValueEx (hKey, L"MessageID", 0, REG_DWORD, (BYTE *)&uiMessageID, sizeof(DWORD));

	RegCloseKey (hKey);
}

