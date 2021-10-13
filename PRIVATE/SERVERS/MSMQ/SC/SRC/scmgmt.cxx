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
#include <windows.h>
#include <mq.h>

#include <scapi.h>

#include <sc.hxx>
#include <scqman.hxx>
#include <scqueue.hxx>
#include <scpacket.hxx>
#include <scfile.hxx>
#include <scsman.hxx>
#include <scorder.hxx>

#include <mqmgmt.h>

HRESULT scmgmt_MQMgmtGetInfo2
(
LPCWSTR			pMachineName,
LPCWSTR			pObjectName,
DWORD			cp,
PROPID			aPropID[],
PROPVARIANT		aPropVar[]
) {
	if (pMachineName)
		return MQ_ERROR_MACHINE_NOT_FOUND;

	if (! pObjectName || (! cp) || (! aPropID) || (! aPropVar))
		return MQ_ERROR_INVALID_PARAMETER;

	if (! fApiInitialized)
		return MQ_ERROR_SERVICE_NOT_AVAILABLE;

	if (wcsicmp (pObjectName, MO_MACHINE_TOKEN) == 0) {
		//
		//	Validate parameters first
		//
		for (int i = 0 ; i < (int)cp ; ++i) {
			switch (aPropID[i]) {
			case PROPID_MGMT_MSMQ_ACTIVEQUEUES:
			case PROPID_MGMT_MSMQ_PRIVATEQ:
			case PROPID_MGMT_MSMQ_DSSERVER:
			case PROPID_MGMT_MSMQ_CONNECTED:
			case PROPID_MGMT_MSMQ_TYPE:
				if (aPropVar[i].vt != VT_NULL)
					return MQ_ERROR_PROPERTY;
				break;
			default:
				return MQ_ERROR_PROPERTY;
			}
		}

		//
		//	Do the work
		//
		if (! fApiInitialized)
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;

		gMem->Lock ();

		if (! fApiInitialized) {
			gMem->Unlock ();
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
		}

		for (i = 0 ; i < (int)cp ; ++i) {
			switch (aPropID[i]) {
			case PROPID_MGMT_MSMQ_ACTIVEQUEUES:
				{
					//
					//	Active are all queues
					//
					int iQueues = 0;
					ScQueueList *pql = gQueueMan->pqlIncoming;

					while (pql) {
						++iQueues;

						pql = pql->pqlNext;
					}

					pql = gQueueMan->pqlOutgoing;

					while (pql) {
						++iQueues;

						pql = pql->pqlNext;
					}

					if (! iQueues)
						break;

					WCHAR **ppsz = (WCHAR **)scutil_OutOfProcAlloc ((void **)&aPropVar[i].calpwstr.pElems, iQueues * sizeof(WCHAR *));
					if (! ppsz)
						break;

					aPropVar[i].vt = VT_VECTOR | VT_LPWSTR;
					aPropVar[i].calpwstr.cElems = iQueues;
					pql = gQueueMan->pqlIncoming;

					int iQueues2 = 0;

					while (pql) {
						ppsz[iQueues2++] = scutil_OutOfProcDup (pql->pQueue->lpszFormatName);

						pql = pql->pqlNext;
					}

					pql = gQueueMan->pqlOutgoing;

					while (pql) {
						ppsz[iQueues2++] = scutil_OutOfProcDup (pql->pQueue->lpszFormatName);

						pql = pql->pqlNext;
					}

					SVSUTIL_ASSERT (iQueues == iQueues2);
				}
				break;

			case PROPID_MGMT_MSMQ_PRIVATEQ:
				{
					//
					//	Only local (they are all private)
					//
					int iQueues = 0;
					ScQueueList *pql = gQueueMan->pqlIncoming;

					while (pql) {
						if (! (pql->pQueue->qp.bIsDeadLetter || pql->pQueue->qp.bIsInternal ||
							pql->pQueue->qp.bIsJournal || pql->pQueue->qp.bIsMachineJournal ||
							pql->pQueue->qp.bIsOrderAck || pql->pQueue->qp.bIsProtected))
							++iQueues;

						pql = pql->pqlNext;
					}

					if (! iQueues)
						break;

					WCHAR **ppsz = (WCHAR **)scutil_OutOfProcAlloc ((void **)&aPropVar[i].calpwstr.pElems, iQueues * sizeof(WCHAR *));
					if (! ppsz)
						break;

					aPropVar[i].vt = VT_VECTOR | VT_LPWSTR;
					aPropVar[i].calpwstr.cElems = iQueues;
					pql = gQueueMan->pqlIncoming;

					int iQueues2 = 0;

					while (pql) {
						if (! (pql->pQueue->qp.bIsDeadLetter || pql->pQueue->qp.bIsInternal ||
							pql->pQueue->qp.bIsJournal || pql->pQueue->qp.bIsMachineJournal ||
							pql->pQueue->qp.bIsOrderAck || pql->pQueue->qp.bIsProtected))
							ppsz[iQueues2++] = scutil_OutOfProcDup (pql->pQueue->lpszFormatName);

						pql = pql->pqlNext;
					}

					SVSUTIL_ASSERT (iQueues == iQueues2);
				}
				break;

			case PROPID_MGMT_MSMQ_DSSERVER:
				break;

			case PROPID_MGMT_MSMQ_CONNECTED:
				{
					int fConnected = FALSE;

					ScSession *pSess = gSessionMan->pSessList;

					while (pSess) {
						if (pSess->fSessionState == SCSESSION_STATE_OPERATING) {
							fConnected = TRUE;
							break;
						}

						pSess = pSess->pNext;
					}

					aPropVar[i].vt = VT_LPWSTR;
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (fConnected ? MSMQ_CONNECTED : MSMQ_DISCONNECTED);
				}
				break;

			case PROPID_MGMT_MSMQ_TYPE:
				{
					OSVERSIONINFO v;

					v.dwOSVersionInfoSize = sizeof (v);

					GetVersionEx (&v);

					WCHAR szBuffer[MSMQ_SC_BIGBUFFER];
					wsprintf (szBuffer, L"Windows CE %d.%d (Build %d, %d) - MSMQ %d.%d (Build %d), MSMQ_CE",
							v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber, v.dwPlatformId,
							SC_VERSION_MAJOR, SC_VERSION_MINOR, SC_VERSION_BUILD_NUMBER);

					aPropVar[i].vt = VT_LPWSTR;
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (szBuffer);
				}
				break;

			default:
				SVSUTIL_ASSERT (0);
			}
		}
		gMem->Unlock ();
	} else {
		if (wcsnicmp (pObjectName, MO_QUEUE_TOKEN, SVSUTIL_CONSTSTRLEN(MO_QUEUE_TOKEN)) != 0)
			return MQ_ERROR_INVALID_PARAMETER;

		WCHAR *lpszFormatName = (WCHAR *)pObjectName + SVSUTIL_CONSTSTRLEN(MO_QUEUE_TOKEN) + 1;

		//
		//	Validate parameters first
		//
		for (int i = 0 ; i < (int)cp ; ++i) {
			switch (aPropID[i]) {
			case PROPID_MGMT_QUEUE_BASE:
				break;

			case PROPID_MGMT_QUEUE_PATHNAME:
			case PROPID_MGMT_QUEUE_FORMATNAME:
			case PROPID_MGMT_QUEUE_TYPE:
			case PROPID_MGMT_QUEUE_LOCATION:
			case PROPID_MGMT_QUEUE_XACT:
			case PROPID_MGMT_QUEUE_FOREIGN:
			case PROPID_MGMT_QUEUE_MESSAGE_COUNT:
			case PROPID_MGMT_QUEUE_USED_QUOTA:
			case PROPID_MGMT_QUEUE_JOURNAL_MESSAGE_COUNT:
			case PROPID_MGMT_QUEUE_JOURNAL_USED_QUOTA:
			case PROPID_MGMT_QUEUE_STATE:
			case PROPID_MGMT_QUEUE_NEXTHOPS:
			case PROPID_MGMT_QUEUE_EOD_LAST_ACK:
			case PROPID_MGMT_QUEUE_EOD_LAST_ACK_TIME:
			case PROPID_MGMT_QUEUE_EOD_LAST_ACK_COUNT:
			case PROPID_MGMT_QUEUE_EOD_FIRST_NON_ACK:
			case PROPID_MGMT_QUEUE_EOD_LAST_NON_ACK:
			case PROPID_MGMT_QUEUE_EOD_NEXT_SEQ:
			case PROPID_MGMT_QUEUE_EOD_NO_READ_COUNT:
			case PROPID_MGMT_QUEUE_EOD_NO_ACK_COUNT:
			case PROPID_MGMT_QUEUE_EOD_RESEND_TIME:
			case PROPID_MGMT_QUEUE_EOD_RESEND_INTERVAL:
			case PROPID_MGMT_QUEUE_EOD_RESEND_COUNT:
			case PROPID_MGMT_QUEUE_EOD_SOURCE_INFO:
				if (aPropVar[i].vt != VT_NULL)
					return MQ_ERROR_PROPERTY;
				break;
			default:
				return MQ_ERROR_PROPERTY;
			}
		}

		if (! fApiInitialized)
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;

		gMem->Lock ();

		if (! fApiInitialized) {
			gMem->Unlock ();
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
		}

		ScQueue *pQueue = gQueueMan->FindIncomingByFormat (lpszFormatName);
		if (! pQueue)
			pQueue = gQueueMan->FindOutgoingByFormat (lpszFormatName);

		if (! pQueue) {
			gMem->Unlock ();
			return MQ_ERROR_QUEUE_NOT_FOUND;
		}

		//
		//	Do the work...
		//
		for (i = 0 ; i < (int)cp ; ++i) {
			switch (aPropID[i]) {
			case PROPID_MGMT_QUEUE_BASE:
				break;

			case PROPID_MGMT_QUEUE_PATHNAME:
				if (pQueue->qp.bIsDeadLetter || pQueue->qp.bIsMachineJournal ||
					pQueue->qp.bIsInternal || pQueue->qp.bIsOrderAck || pQueue->qp.bIsProtected)
					break;

				if (wcsnicmp (pQueue->lpszFormatName, MSMQ_SC_FORMAT_DIRECT_OS,
												SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_OS)) != 0)
					break;

				aPropVar[i].vt = VT_LPWSTR;
				aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (pQueue->lpszFormatName +
												SVSUTIL_CONSTSTRLEN(MSMQ_SC_FORMAT_DIRECT_OS));
				break;

			case PROPID_MGMT_QUEUE_FORMATNAME:
				aPropVar[i].vt = VT_LPWSTR;
				aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (pQueue->lpszFormatName);
				break;

			case PROPID_MGMT_QUEUE_TYPE:
				aPropVar[i].vt = VT_LPWSTR;
				if (pQueue->qp.bIsOrderAck || pQueue->qp.bIsDeadLetter || pQueue->qp.bIsMachineJournal)
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_TYPE_MACHINE);
				else if (pQueue->qp.bIsOutFRS)
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_TYPE_CONNECTOR);
				else if (pQueue->qp.bIsPublic)
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_TYPE_PUBLIC);
				else
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_TYPE_PRIVATE);
				break;

			case PROPID_MGMT_QUEUE_LOCATION:
				aPropVar[i].vt = VT_LPWSTR;
				if (pQueue->qp.bIsIncoming)
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_LOCAL_LOCATION);
				else
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_REMOTE_LOCATION);
				break;

			case PROPID_MGMT_QUEUE_XACT:
				aPropVar[i].vt = VT_LPWSTR;
				if (pQueue->qp.bTransactional)
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_CORRECT_TYPE);
				else
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_INCORRECT_TYPE);
				break;

			case PROPID_MGMT_QUEUE_FOREIGN:
				aPropVar[i].vt = VT_LPWSTR;
				aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_INCORRECT_TYPE);
				break;

			case PROPID_MGMT_QUEUE_MESSAGE_COUNT:
				aPropVar[i].vt    = VT_UI4;
				aPropVar[i].ulVal = pQueue->Size ();
				break;

			case PROPID_MGMT_QUEUE_USED_QUOTA:
				aPropVar[i].vt    = VT_UI4;
				aPropVar[i].ulVal = pQueue->iQueueSizeB;
				break;

			case PROPID_MGMT_QUEUE_JOURNAL_MESSAGE_COUNT:
				if (! pQueue->pJournal)
					break;

				aPropVar[i].vt    = VT_UI4;
				aPropVar[i].ulVal = pQueue->pJournal->Size ();
				break;

			case PROPID_MGMT_QUEUE_JOURNAL_USED_QUOTA:
				if (! pQueue->pJournal)
					break;

				aPropVar[i].vt    = VT_UI4;
				aPropVar[i].ulVal = pQueue->pJournal->iQueueSizeB;
				break;

			case PROPID_MGMT_QUEUE_STATE:
				aPropVar[i].vt = VT_LPWSTR;
				if (! pQueue->pSess)
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_STATE_LOCAL);
				else if ((pQueue->pSess->fSessionState == SCSESSION_STATE_INACTIVE) ||
					(pQueue->pSess->fSessionState == SCSESSION_STATE_WAITING))
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_STATE_NONACTIVE);
				else if (pQueue->pSess->fSessionState == SCSESSION_STATE_OPERATING)
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_STATE_CONNECTED);
				else
					aPropVar[i].pwszVal = (WCHAR *)scutil_OutOfProcDup (MGMT_QUEUE_STATE_WAITING);
				break;
				//
				//	No need to support for now. Defer until version 2.
				//
			case PROPID_MGMT_QUEUE_NEXTHOPS:
			case PROPID_MGMT_QUEUE_EOD_LAST_ACK:
			case PROPID_MGMT_QUEUE_EOD_LAST_ACK_TIME:
			case PROPID_MGMT_QUEUE_EOD_LAST_ACK_COUNT:
			case PROPID_MGMT_QUEUE_EOD_FIRST_NON_ACK:
			case PROPID_MGMT_QUEUE_EOD_LAST_NON_ACK:
			case PROPID_MGMT_QUEUE_EOD_NEXT_SEQ:
			case PROPID_MGMT_QUEUE_EOD_NO_READ_COUNT:
			case PROPID_MGMT_QUEUE_EOD_NO_ACK_COUNT:
			case PROPID_MGMT_QUEUE_EOD_RESEND_TIME:
			case PROPID_MGMT_QUEUE_EOD_RESEND_INTERVAL:
			case PROPID_MGMT_QUEUE_EOD_RESEND_COUNT:
			case PROPID_MGMT_QUEUE_EOD_SOURCE_INFO:
				break;

			default:
				SVSUTIL_ASSERT (0);
			}
		}

		gMem->Unlock ();
	}

	return MQ_OK;
}

static HRESULT scmgmt_MQMgmtAction_Internal
(
LPCWSTR			pMachineName,
LPCWSTR			pObjectName,
LPCWSTR			pAction
) {
	if (pMachineName)
		return MQ_ERROR_MACHINE_NOT_FOUND;

	if (! pObjectName || (! pAction))
		return MQ_ERROR_INVALID_PARAMETER;

	if (wcsicmp (pObjectName, MO_MACHINE_TOKEN) == 0) {

		//
		//	Pre-API test...
		//
		if (wcsicmp (pAction, MACHINE_ACTION_SHUTDOWN) == 0) {
			return scmain_Shutdown (TRUE) ? MQ_OK : MQ_ERROR;
		} else if (wcsicmp (pAction, MACHINE_ACTION_EXIT) == 0) {
			return scmain_ForceExit ()  ? MQ_OK : MQ_ERROR;
		} else if (wcsicmp (pAction, MACHINE_ACTION_CONSOLE) == 0) {
			return scapi_Console ();
		} else if (wcsicmp (pAction, MACHINE_ACTION_STARTUP) == 0)
			return scmain_Init () ? MQ_OK : MQ_ERROR;

		if (! fApiInitialized)
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;

		gMem->Lock ();

		if (! fApiInitialized) {
			gMem->Unlock ();
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
		}

		if (wcsicmp (pAction, MACHINE_ACTION_CONNECT) == 0)
			gSessionMan->ConnectToNet (0);
		else if (wcsicmp (pAction, MACHINE_ACTION_DISCONNECT) == 0) {
			ScSession *pSess = gSessionMan->pSessList;
			while (pSess) {
				if (pSess->fSessionState == SCSESSION_STATE_OPERATING)
					closesocket (pSess->s);

				pSess = pSess->pNext;
			}
		} else if (wcsicmp (pAction, MACHINE_ACTION_TIDY) == 0) {
			gSeqMan->uiLastCompactT = 0;		// Force compaction
			gSeqMan->uiLastPruneT   = 0;		// Force compaction
			gQueueMan->PeriodicCheck ();
		} else {
			gMem->Unlock ();
			return MQ_ERROR_INVALID_PARAMETER;
		}

		gMem->Unlock ();
	} else {
		if (wcsnicmp (pObjectName, MO_QUEUE_TOKEN, SVSUTIL_CONSTSTRLEN(MO_QUEUE_TOKEN)) != 0)
			return MQ_ERROR_INVALID_PARAMETER;

		if (! fApiInitialized)
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;

		gMem->Lock ();

		if (! fApiInitialized) {
			gMem->Unlock ();
			return MQ_ERROR_SERVICE_NOT_AVAILABLE;
		}

		WCHAR *lpszFormatName = (WCHAR *)pObjectName + SVSUTIL_CONSTSTRLEN(MO_QUEUE_TOKEN) + 1;

		int iRes = MQ_OK;

		ScQueue *pQueue = gQueueMan->FindIncomingByFormat (lpszFormatName);
		if (! pQueue)
			pQueue = gQueueMan->FindOutgoingByFormat (lpszFormatName);

		if (! pQueue)
			iRes = MQ_ERROR_QUEUE_NOT_FOUND;
		else {
			if (wcsicmp (pAction, QUEUE_ACTION_PURGE) == 0) {
				pQueue->Purge (MQMSG_CLASS_NACK_Q_PURGED);
			} else if (wcsnicmp (pAction, QUEUE_ACTION_DELMSG, SVSUTIL_CONSTSTRLEN(QUEUE_ACTION_DELMSG)) == 0) {
				pAction += SVSUTIL_CONSTSTRLEN(QUEUE_ACTION_DELMSG);
				GUID  g;
				DWORD u;
				int d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11;

				if (swscanf (pAction, SC_GUID_FORMAT L" %08x", &d1, &d2, &d3, &d4, &d5, &d6,
															&d7, &d8, &d9, &d10, &d11, &u) == 12) {
					g.Data1 = d1;
					g.Data2 = d2;
					g.Data3 = d3;
					g.Data4[0] = d4;
					g.Data4[1] = d5;
					g.Data4[2] = d6;
					g.Data4[3] = d7;
					g.Data4[4] = d8;
					g.Data4[5] = d9;
					g.Data4[6] = d10;
					g.Data4[7] = d11;

					OBJECTID oid;
					oid.Lineage    = g;
					oid.Uniquifier = u;
					if (! pQueue->PurgeMessage (&oid, MQMSG_CLASS_NACK_Q_PURGED))
						iRes = MQ_ERROR_MESSAGE_ALREADY_RECEIVED;

				} else
					iRes = MQ_ERROR_INVALID_PARAMETER;
			} else
				iRes = MQ_ERROR_INVALID_PARAMETER;
		}

		gMem->Unlock ();

		return iRes;
	}

	return MQ_OK;
}

struct MgmtParams {
	LPCWSTR pMachineName;
	LPCWSTR pObjectName;
	LPCWSTR pAction;
	DWORD   dwProcPermissions;
};

static DWORD WINAPI scmgmt_MQMgmtAction_Thread (LPVOID lpVoid) {
	MgmtParams *pParams = (MgmtParams *)lpVoid;

	DWORD dwPerms = SetProcPermissions (pParams->dwProcPermissions);

	HRESULT hr = scmgmt_MQMgmtAction_Internal (pParams->pMachineName, pParams->pObjectName, pParams->pAction);

	SetProcPermissions (dwPerms);

	return hr;
}

HRESULT scmgmt_MQMgmtAction
(
LPCWSTR			pMachineName,
LPCWSTR			pObjectName,
LPCWSTR			pAction
) {
	MgmtParams *pParams = (MgmtParams *)LocalAlloc (LMEM_FIXED, sizeof(MgmtParams));
	if (! pParams)
		return E_OUTOFMEMORY;

	pParams->pMachineName = pMachineName;
	pParams->pObjectName = pObjectName;
	pParams->pAction = pAction;

	pParams->dwProcPermissions = GetCurrentPermissions ();

	HANDLE hThread = CreateThread (NULL, 0, scmgmt_MQMgmtAction_Thread, pParams, 0, NULL);

	HRESULT hr = MQ_ERROR;

	if (hThread) {
		if (WaitForSingleObject (hThread, INFINITE) == WAIT_OBJECT_0)
			GetExitCodeThread (hThread, (DWORD *)&hr);
		CloseHandle (hThread);
	}

	LocalFree (pParams);

	return hr;
}