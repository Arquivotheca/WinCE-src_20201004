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

    scqueue.cxx

Abstract:

    Small client queue class support


--*/
#include <sc.hxx>

#include <scpacket.hxx>
#include <scqman.hxx>
#include <scqueue.hxx>
#include <scsman.hxx>
#include <sccomp.hxx>

#include <mq.h>

void ScQueue::Init (void) {
	fInitialized	= FALSE;

	sFile			= NULL;
	pPackets		= NULL;

	pJournal		= NULL;

	lpszQueueHost	= NULL;
	lpszQueueName	= NULL;
	lpszFormatName	= NULL;
	lpszQueueLabel	= NULL;

	memset (&qp, 0, sizeof(qp));

	tCreation		= scutil_now ();
	tModification	= tCreation;

	iQueueSizeB     = 0;

	fDenyAll		= FALSE;
	uiOpenRecv		= 0;
	uiOpen			= 0;

	llSeqID         = 0;
	uiSeqNum        = 0;
	uiFirstUnackedSeqN = 0;

	hkReceived      = 0;

	hUpdateEvent	= CreateEvent (NULL, FALSE, FALSE, NULL);

	pSess    = NULL;
}


static BOOL CheckQueueTypeSupported(int qType, BOOL fLocalOnly) {
	SVSUTIL_ASSERT(gMem->IsLocked());

	if ((qType == SCFILE_QP_FORMAT_HTTPS) || (qType == SCFILE_QP_FORMAT_HTTP))
		return gMachine->fUseSRMP;
	else if ((qType == SCFILE_QP_FORMAT_TCP) || (qType == SCFILE_QP_FORMAT_OS)) {
		if (gMachine->fUseBinary)
			return TRUE;

		return fLocalOnly;  // Allow creation of local queues even if binary is disabled.
	}
	else {
		SVSUTIL_ASSERT(0);
	}

	return FALSE;	
}

void ScQueue::InitSequence (void) {
	if (qp.bIsIncoming || (! qp.bTransactional)) {
		SVSUTIL_ASSERT ((uiSeqNum == 0) && (llSeqID == 0));
		return;
	}

	if (llSeqID)
		return;

	llSeqID = ((LONGLONG)scutil_now ()) << 32;

	SVSUTIL_ASSERT (llSeqID != 0);
	SVSUTIL_ASSERT (uiSeqNum == 0);
}
//
//	Create new incoming or outgoing queue from backup file
//
ScQueue::ScQueue (WCHAR *a_lpszFileName, HRESULT *phr) {
	Init ();

	pPackets = new SVSTree (gMem->pTreeNodeMem);

	sFile = new ScFile (a_lpszFileName, this);

	if (! sFile->Restore ()) {
		if (phr)
			*phr = MQ_ERROR;

		return;
	}

	WCHAR *lpFormatName = scutil_MakeFormatName (lpszQueueHost, lpszQueueName, &qp);

	if (! lpFormatName) {
		if (phr)
			*phr = MQ_ERROR_ILLEGAL_QUEUE_PATHNAME;

		return;
	}

	lpszFormatName = svsutil_StringHashAlloc (gMem->pStringHash, lpFormatName);
	g_funcFree (lpFormatName, g_pvFreeData);

	if (! qp.bIsIncoming)
		pSess = gSessionMan->GetSession (lpszQueueHost,qp.bFormatType);

	InitSequence ();

	fInitialized = TRUE;
}

//
// Create new incoming queue based on path name
//
ScQueue::ScQueue (WCHAR *a_lpszPathName, WCHAR *a_lpszLabel, ScQueueParms *a_pqp, HRESULT *phr){
	SVSUTIL_ASSERT (a_pqp->bIsIncoming);

	Init ();

	WCHAR *l_lpszQueueHost, *l_lpszQueueName;
	if (! scutil_ParsePathName (a_lpszPathName, l_lpszQueueHost, l_lpszQueueName, a_pqp)) {
		if (phr)
			*phr = MQ_ERROR_ILLEGAL_QUEUE_PATHNAME;

		return;
	}

	if (! a_pqp->bIsIncoming) {
		g_funcFree (l_lpszQueueName, g_pvFreeData);
		g_funcFree (l_lpszQueueHost, g_pvFreeData);

		if (phr)
			*phr = MQ_ERROR_ILLEGAL_QUEUE_PATHNAME;

		return;
	}

	if (! CheckQueueTypeSupported(a_pqp->bFormatType,a_pqp->bIsIncoming)) {
		g_funcFree (l_lpszQueueName, g_pvFreeData);
		g_funcFree (l_lpszQueueHost, g_pvFreeData);

		if (phr)
			*phr = MQ_ERROR_QUEUE_NOT_AVAILABLE;

		return;
	}

	SVSUTIL_ASSERT ((! a_pqp->bIsJournal) || a_pqp->bIsIncoming);
	SVSUTIL_ASSERT (! (a_pqp->bIsJournal && a_pqp->bHasJournal));
	SVSUTIL_ASSERT ((! a_pqp->bIsJournalOn) || a_pqp->bHasJournal);

	lpszQueueName = svsutil_StringHashAlloc (gMem->pStringHash, l_lpszQueueName);
	g_funcFree (l_lpszQueueName, g_pvFreeData);

	lpszQueueHost = svsutil_StringHashAlloc (gMem->pStringHash, l_lpszQueueHost);
	g_funcFree (l_lpszQueueHost, g_pvFreeData);

	WCHAR *lpFormatName = scutil_MakeFormatName (a_lpszPathName, a_pqp);

	if (! lpFormatName) {
		if (phr)
			*phr = MQ_ERROR_ILLEGAL_QUEUE_PATHNAME;

		return;
	}

	lpszFormatName = svsutil_StringHashAlloc (gMem->pStringHash, lpFormatName);
	g_funcFree (lpFormatName, g_pvFreeData);

	qp = *a_pqp;

	scutil_uuidgen (&qp.guid);

	pPackets = new SVSTree (gMem->pTreeNodeMem);

	if (a_lpszLabel)
		lpszQueueLabel = svsutil_wcsdup(a_lpszLabel);

	tCreation = tModification = scutil_now();

	WCHAR *lpszFileName  = scutil_MakeFileName (lpszQueueHost, lpszQueueName, a_pqp);
	sFile = new ScFile (lpszFileName, this);
	g_funcFree (lpszFileName, g_pvFreeData);

	if (! sFile->CreateNew()) {
		if (phr) {
			if (gQueueMan->FindIncomingByFormat (lpszFormatName) || gQueueMan->FindOutgoingByFormat (lpszFormatName))
				*phr = MQ_ERROR_QUEUE_EXISTS;
			else
				*phr = MQ_ERROR_INSUFFICIENT_RESOURCES;
		}

		return;
	}

	InitSequence ();

	fInitialized = TRUE;
}

//
//	Create non-local queue from format name
//
ScQueue::ScQueue (WCHAR *a_lpszFormatName, ScQueueParms *a_pqp, HRESULT *phr) {
	SVSUTIL_ASSERT (! a_pqp->bHasJournal);
	SVSUTIL_ASSERT (! a_pqp->bIsJournalOn);
	SVSUTIL_ASSERT (! a_pqp->bIsDeadLetter);
	SVSUTIL_ASSERT (! a_pqp->bIsMachineJournal);
	SVSUTIL_ASSERT (! a_pqp->bIsIncoming);
	SVSUTIL_ASSERT (! a_pqp->bIsJournal);

	Init ();

	qp = *a_pqp;
	scutil_uuidgen (&qp.guid);

	WCHAR *l_lpszQueueHost, *l_lpszQueueName;
	if (! scutil_ParseNonLocalDirectFormatName (a_lpszFormatName, l_lpszQueueHost, l_lpszQueueName, &qp)) {
		if (phr)
			*phr = MQ_ERROR_ILLEGAL_QUEUE_PATHNAME;

		return;
	}

	if (wcsicmp (l_lpszQueueHost, gMachine->lpszHostName) == 0) {
		g_funcFree (l_lpszQueueName, g_pvFreeData);
		g_funcFree (l_lpszQueueHost, g_pvFreeData);

		if (phr)
			*phr = MQ_ERROR_ILLEGAL_QUEUE_PATHNAME;

		return;
	}

	if (! CheckQueueTypeSupported(qp.bFormatType,a_pqp->bIsIncoming)) {
		g_funcFree (l_lpszQueueName, g_pvFreeData);
		g_funcFree (l_lpszQueueHost, g_pvFreeData);

		if (phr)
			*phr = MQ_ERROR_QUEUE_NOT_AVAILABLE;

		return;
	}

	lpszQueueName = svsutil_StringHashAlloc (gMem->pStringHash, l_lpszQueueName);
	g_funcFree (l_lpszQueueName, g_pvFreeData);

	lpszQueueHost = svsutil_StringHashAlloc (gMem->pStringHash, l_lpszQueueHost);
	g_funcFree (l_lpszQueueHost, g_pvFreeData);

	//
	//	Override the quota setting...
	//
	qp.uiQuotaK = gMachine->uiDefaultOutQuotaK;

	WCHAR *lpszFileName  = scutil_MakeFileName (lpszQueueHost, lpszQueueName, &qp);
	sFile = new ScFile (lpszFileName, this);
	g_funcFree (lpszFileName, g_pvFreeData);

	lpszFormatName = svsutil_StringHashAlloc (gMem->pStringHash, a_lpszFormatName);

	pPackets = new SVSTree (gMem->pTreeNodeMem);

	tCreation = tModification = scutil_now();

	if (! sFile->CreateNew()) {
		if (phr) {
			if (gQueueMan->FindIncomingByFormat (lpszFormatName) || gQueueMan->FindOutgoingByFormat (lpszFormatName))
				*phr = MQ_ERROR_QUEUE_EXISTS;
			else
				*phr = MQ_ERROR_INSUFFICIENT_RESOURCES;
		}

		return;
	}

	SVSUTIL_ASSERT (! qp.bIsIncoming);

	pSess = gSessionMan->GetSession (lpszQueueHost,qp.bFormatType);

	InitSequence ();

	fInitialized = TRUE;
}

//
//	Delete queue...
//
static void freePacketData (void *pvData, void *pvArg) {
	ScPacket *pPacket = (ScPacket *)pvData;

	if (pPacket->pImage)
		g_funcFree (pPacket->pImage, g_pvFreeData);
}

ScQueue::~ScQueue (void) {
	if (sFile)
		delete sFile;

	if (lpszQueueHost)
		svsutil_StringHashFree (gMem->pStringHash, lpszQueueHost);

	if (lpszQueueName)
		svsutil_StringHashFree (gMem->pStringHash, lpszQueueName);

	if (lpszFormatName)
		svsutil_StringHashFree (gMem->pStringHash, lpszFormatName);

	if (pSess)
		gSessionMan->ReleaseSession (pSess);

	if (pPackets) {
		pPackets->Empty (freePacketData, NULL);
		delete pPackets;
	}

	if (lpszQueueLabel)
		g_funcFree (lpszQueueLabel, g_pvFreeData);

	if (hUpdateEvent)
		CloseHandle (hUpdateEvent);
}

ScPacket *ScQueue::InsertPacket (ScPacket *pPacket) {
	SVSUTIL_ASSERT (! pPacket->pNode);
	SVSUTIL_ASSERT (! pPacket->pTimeoutNode);
	SVSUTIL_ASSERT (pPacket->uiPacketState == SCPACKET_STATE_ALIVE);

	int iRes = (NULL != (pPacket->pNode = pPackets->Insert (pPacket->hkOrderKey, pPacket)));

	if (! iRes) {
		unsigned int uiWhichSection = 0;

		sFile->DeletePacket (pPacket, uiWhichSection);
		sFile->UpdateSection (uiWhichSection);

		svsutil_FreeFixed (pPacket, gMem->pPacketMem);

		return NULL;
	}

	pPacket->pQueue = this;

	if (! qp.bIsIncoming)
		gSessionMan->PacketInserted (pSess);

	if (pPacket->tExpirationTime != INFINITE) {
		pPacket->pTimeoutNode = gMem->pTimeoutTree->Insert (pPacket->tExpirationTime, pPacket);
		SetEvent (gQueueMan->hPacketExpired);
	}

	return pPacket;
}

ScPacket *ScQueue::MakePacket (ScPacketImage *pPacketImage, int iDirEntry, int fPutInFile) {
	SVSUTIL_ASSERT (iQueueSizeB >= 0);

	int iSize = pPacketImage->PersistedSize ();

	if ((unsigned int)(iSize + iQueueSizeB) > qp.uiQuotaK * 1024)
		return NULL;

	if ((unsigned int)(gMachine->iMachineQuotaUsedB + iSize) > gMachine->uiMachineQuotaK * 1024)
		return NULL;

	ScPacket *pPacket = (ScPacket *)svsutil_GetFixed (gMem->pPacketMem);

	if (! pPacket)
		return NULL;

	pPacket->ScPacket::ScPacket();

	pPacket->hkOrderKey = pPacketImage->hkOrderKey;

	pPacket->tCreationTime   = pPacketImage->sect.pUserHeader->GetSentTime();

	unsigned int uiNow = scutil_now ();

	if (qp.bIsJournal || qp.bIsDeadLetter || qp.bIsMachineJournal)
		pPacket->tExpirationTime = INFINITE;
	else {
		unsigned int tTime;

		if (qp.bIsIncoming) {
			tTime = pPacketImage->sect.pUserHeader->GetTimeToLiveDelta ();
			if (tTime != INFINITE)
				tTime += pPacketImage->sect.pBaseHeader->GetAbsoluteTimeToQueue ();
		} else
			tTime = pPacketImage->sect.pBaseHeader->GetAbsoluteTimeToQueue ();

		if (tTime != INFINITE)
			tTime += uiNow;

		pPacket->tExpirationTime = tTime;
	}

	OBJECTID oid;
	pPacketImage->sect.pUserHeader->GetMessageID (&oid);

#if defined (SC_VERBOSE) || defined (SC_INCLUDE_CONSOLE)
	pPacket->uiMessageID     = oid.Uniquifier;
	pPacket->guidSourceQM	 = oid.Lineage;
#endif

	pPacket->uiAckType       = pPacketImage->sect.pPropHeader->GetAckType ();
	pPacket->uiAuditType     = pPacketImage->sect.pUserHeader->GetAuditing ();
	pPacket->iDirEntry	     = iDirEntry;
	pPacket->pImage			 = pPacketImage;

	pPacket->uiSeqN          = pPacketImage->sect.pXactHeader ? pPacketImage->sect.pXactHeader->GetSeqN () : 0;

	if (qp.bIsJournal || qp.bIsDeadLetter || qp.bIsMachineJournal || (pPacket->pImage->sect.pUserHeader->GetDelivery() == MQMSG_DELIVERY_RECOVERABLE)) {
		if (fPutInFile) {
			SVSUTIL_ASSERT (iDirEntry == -1);

			unsigned int uiWhichSection = 0;

			if ((! sFile->BackupPacket (pPacket, uiWhichSection)) ||
				(! sFile->UpdateSection (uiWhichSection))) {
				svsutil_FreeFixed (pPacket, gMem->pPacketMem);
				return NULL;
			}
		}
	} else
		SVSUTIL_ASSERT (iDirEntry == -1);

	tModification = uiNow;

	if (qp.bIsJournal || qp.bIsDeadLetter || qp.bIsMachineJournal || (pPacket->pImage->sect.pUserHeader->GetDelivery() == MQMSG_DELIVERY_RECOVERABLE))
		pPacket->pImage = NULL;

	SetEvent (hUpdateEvent);

	ScPacket *pPacket2 = InsertPacket (pPacket);

	if (pPacket2) {
		if (! pPacket->pImage)
			g_funcFree (pPacketImage, g_pvFreeData);

		iQueueSizeB += iSize;
		gMachine->iMachineQuotaUsedB += iSize;
	} else 
		svsutil_FreeFixed (pPacket, gMem->pPacketMem);

	return pPacket2;
}

int ScQueue::DisposeOfPacket (ScPacket *pPacket) {
	SVSUTIL_ASSERT (pPacket->pQueue == this);

	// The packet might be already gone. In this case the size has already been subtracted
	// from the queue.

	int iSize = 0;
	if (pPacket->pImage)
		iSize = pPacket->pImage->PersistedSize ();
	else if (pPacket->iDirEntry >= 0)
		iSize = sFile->GetPacketSize (pPacket);

	iQueueSizeB -= iSize;
	SVSUTIL_ASSERT (iQueueSizeB >= 0);

	gMachine->iMachineQuotaUsedB -= iSize;
	SVSUTIL_ASSERT (gMachine->iMachineQuotaUsedB >= 0);

	pPacket->uiPacketState = SCPACKET_STATE_DEAD;
	pPacket->tCreationTime = pPacket->tExpirationTime = 0;

#if defined (SC_VERBOSE) || defined (SC_INCLUDE_CONSOLE)
	pPacket->uiMessageID = 0;
	memset (&pPacket->guidSourceQM, 0, sizeof(pPacket->guidSourceQM));
#endif

	pPacket->uiAckType = 0;

	if (pPacket->iDirEntry >= 0) {
		unsigned int uiWhichSection = 0;
		sFile->DeletePacket (pPacket, uiWhichSection);
		sFile->UpdateSection (uiWhichSection);
		pPacket->iDirEntry = -1;
	}

	if (pPacket->pImage) {
		g_funcFree (pPacket->pImage, g_pvFreeData);
		pPacket->pImage = NULL;
	}

	if (pPacket->pTimeoutNode) {
		gMem->pTimeoutTree->Delete (pPacket->pTimeoutNode);
		pPacket->pTimeoutNode = NULL;
		SetEvent (gQueueMan->hPacketExpired);
	}

	if (pPacket->GetRefCount() > 1)
		return TRUE;

	if (pPacket->pNode) {
		pPackets->Delete (pPacket->pNode);
		pPacket->pNode = NULL;

		if (pPackets->IsEmpty())		// Reset the counter...
			hkReceived = 0;
	}

	pPacket->DelRef ();

	svsutil_FreeFixed (pPacket, gMem->pPacketMem);

	tModification = scutil_now ();

	return TRUE;
}

int ScQueue::Advance (ScHandleInfo *pHInfo) {
	SVSUTIL_ASSERT (pHInfo->uiHandleType == SCQMAN_HANDLE_CURSOR);
	SVSUTIL_ASSERT (this == pHInfo->pQueue);
	SVSUTIL_ASSERT (qp.bIsIncoming);

	//
	//	We will be looking for next after base
	//
	SVSTNode *pBaseNode = (! pHInfo->c.pNode) ? pPackets->Min() : pPackets->Next(pHInfo->c.pNode);

	unsigned int uiTime = scutil_now ();

	while (pBaseNode) {
		ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pBaseNode);

		if ((pPacket->uiPacketState == SCPACKET_STATE_ALIVE) && did_not_expire (pPacket->tExpirationTime, uiTime))
			break;

		pBaseNode = pPackets->Next (pBaseNode);
	}

	if (! pBaseNode)
		return FALSE;

	ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pBaseNode);
	SVSUTIL_ASSERT ((pPacket->uiPacketState == SCPACKET_STATE_ALIVE) && did_not_expire (pPacket->tExpirationTime, uiTime));

	if (pHInfo->c.pNode) {
		ScPacket *pPrevPacket = (ScPacket *)SVSTree::GetData (pHInfo->c.pNode);
		pPrevPacket->DelRef ();
		if ((pPrevPacket->GetRefCount() == 1) && (pPrevPacket->uiPacketState == SCPACKET_STATE_DEAD))
			DisposeOfPacket (pPrevPacket);
	}

	pPacket->AddRef ();
	pHInfo->c.pNode = pBaseNode;

	return TRUE;
}

int ScQueue::Backup (ScHandleInfo *pHInfo) {
	SVSUTIL_ASSERT (pHInfo->uiHandleType == SCQMAN_HANDLE_CURSOR);
	SVSUTIL_ASSERT (this == pHInfo->pQueue);
	SVSUTIL_ASSERT (qp.bIsIncoming);

	//
	//	We will be looking for prev before base
	//
	if (! pHInfo->c.pNode)
		return FALSE;

	SVSTNode *pBaseNode = pHInfo->c.pNode;

	SVSUTIL_ASSERT (pBaseNode);

	pBaseNode = pPackets->Prev(pBaseNode);

	unsigned int uiTime = scutil_now ();

	while (pBaseNode) {
		ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pBaseNode);

		if ((pPacket->uiPacketState == SCPACKET_STATE_ALIVE) && did_not_expire (pPacket->tExpirationTime, uiTime))
			break;

		pBaseNode = pPackets->Prev (pBaseNode);
	}

	if (! pBaseNode)
		return FALSE;

	ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pBaseNode);
	SVSUTIL_ASSERT ((pPacket->uiPacketState == SCPACKET_STATE_ALIVE) && did_not_expire (pPacket->tExpirationTime, uiTime));

	ScPacket *pPrevPacket = (ScPacket *)SVSTree::GetData (pHInfo->c.pNode);
	pPrevPacket->DelRef ();
	if ((pPrevPacket->GetRefCount() == 1) && (pPrevPacket->uiPacketState == SCPACKET_STATE_DEAD))
		DisposeOfPacket (pPrevPacket);

	pPacket->AddRef ();
	pHInfo->c.pNode = pBaseNode;

	return TRUE;
}

int ScQueue::Reset (ScHandleInfo *pHInfo) {
	if (pHInfo->c.pNode) {
		ScPacket *pPrevPacket = (ScPacket *)SVSTree::GetData (pHInfo->c.pNode);
		pPrevPacket->DelRef ();
		if ((pPrevPacket->GetRefCount() == 1) && (pPrevPacket->uiPacketState == SCPACKET_STATE_DEAD))
			DisposeOfPacket (pPrevPacket);
	}

	pHInfo->c.pNode = NULL;

	return TRUE;
}

int ScQueue::BringToMemory (ScPacket *pPacket) {
	if ((pPacket->iDirEntry >= 0) && (! pPacket->pImage))
		return sFile->RestorePacket(pPacket);

	if (pPacket->pImage)
		return TRUE;

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_FATAL, L"Could not retrieve packet information.\n");
#endif

	return FALSE;
}

void ScQueue::FreeFromMemory (ScPacket *pPacket) {
	if ((pPacket->iDirEntry >= 0) && pPacket->pImage) {
		g_funcFree (pPacket->pImage, g_pvFreeData);
		pPacket->pImage = NULL;
	}
}

void ScQueue::UpdateFile (void) {
	sFile->UpdateSection (SCFILE_UPDATE_HEADER);
}

void ScQueue::Purge (unsigned int uiPurgeType) {
	//
	//	Properly reject all messages...
	//
	unsigned int l_uiQuotaK = qp.uiQuotaK;
	qp.uiQuotaK = 0;

	SVSTNode *pMinNode = NULL;

	for ( ; ; ) {
		SVSTNode *pNode = pMinNode ? pPackets->Next (pMinNode) : pPackets->Min();
		if (! pNode)
			break;

		ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pNode);

		if (pPacket->uiPacketState != SCPACKET_STATE_ALIVE) {
			pMinNode = pNode;
			continue;
		}

		if (qp.bTransactional) {
			if (BringToMemory (pPacket))
				gQueueMan->ForwardTransactionalResponse (pPacket->pImage, uiPurgeType, NULL, NULL);
		}

		gQueueMan->RejectPacket (pPacket, uiPurgeType, pPacket->pQueue);
		DisposeOfPacket (pPacket);
	}

	qp.uiQuotaK = l_uiQuotaK;
}

int ScQueue::PurgeMessage (OBJECTID *poid, unsigned int uiPurgeType) {
	//
	//	Properly reject all messages...
	//

	SVSTNode *pNode = pPackets->Min();

	while (pNode) {
		ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pNode);
		if (BringToMemory (pPacket)) {
			OBJECTID oid2;
			pPacket->pImage->sect.pUserHeader->GetMessageID (&oid2);
			if (memcmp (&oid2, poid, sizeof (oid2)) == 0) {
				if (qp.bTransactional)
					gQueueMan->ForwardTransactionalResponse (pPacket->pImage, uiPurgeType, NULL, NULL);

				gQueueMan->RejectPacket (pPacket, uiPurgeType, pPacket->pQueue);
				DisposeOfPacket (pPacket);

				return TRUE;
			}
			FreeFromMemory (pPacket);
		}

		pNode = pPackets->Next (pNode);
	}

	return FALSE;
}
