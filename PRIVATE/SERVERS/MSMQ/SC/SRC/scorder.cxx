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

    scorder.cxx

Abstract:

    Small client -- implementation of message ordering class


--*/
#include <sc.hxx>
#include <scorder.hxx>
#include <svsutil.hxx>
#include <scqueue.hxx>
#include <scqman.hxx>
#include <scsman.hxx>
#include <sccomp.hxx>

unsigned int ScSequenceCollection::Hash (GUID *pguid, GUID *pOtherGuid) {
	unsigned int uiRes = 0;

	unsigned int *p1 = (unsigned int *)pguid;
	unsigned int *p2 = (unsigned int *)&pOtherGuid;

	for (int i = 0 ; i < 4 ; ++i, ++p1, ++p2)
		uiRes += (*p1) ^ (*p2);

	return uiRes % SCSEQUENCE_HASH_BUCKETS;
}

unsigned int ScSequenceCollection::Hash (WCHAR *sz) {
	int uiRes = 0;

	for ( ; ; ) {
		if (sz[0] == '\0')
			break;

		if (sz[1] == '\0') {
			uiRes += towupper (sz[0]);
			break;
		}

		uiRes += towupper (sz[0]) ^ towupper (sz[1]);
		sz += 2;
	}

	return uiRes % SCSEQUENCE_HASH_BUCKETS;
}

unsigned int ScSequenceCollection::Hash (GUID *pGuid) {
	unsigned int uiRes = 0;

	unsigned int *p1 = (unsigned int *)pGuid;

	uiRes += p1[0] ^ p1[3] + p1[1] ^ p1[2];

	return uiRes % SCSEQUENCE_HASH_BUCKETS;
}

ScOrderSeq *ScSequenceCollection::HashOrderSequence (ScQueue *pQueue, GUID *pguidStreamId, GUID *pguidQueueName, GUID *pguidOrderAckName, unsigned int uiSequenceFlags) {
//	SVSUTIL_ASSERT (pQueue->qp.bIsIncoming);
//	SVSUTIL_ASSERT (pQueue->qp.bTransactional);

	int iPos = Hash (pguidStreamId, pguidQueueName);
	ScOrderSeq *pSeq = pSeqBuckets[iPos];
	while (pSeq) {
		//
		//	Same server can be known by different names, only guid equivalence is to be tested...
		//
		if (((! pQueue) || (pSeq->p->guidQ == pQueue->qp.guid)) && (pSeq->p->guidStreamId == *pguidStreamId) && (pSeq->p->guidQueueName == *pguidQueueName)) {
			return pSeq;
		}
		pSeq = pSeq->pNextHash;
	}

	if (! pguidOrderAckName)
		return NULL;

	pSeq = (ScOrderSeq *)svsutil_GetFixed (pfmOrderSeq);

	if (! pSeq)
		return NULL;

	pSeq->p = PersistBlock();
	if (! pSeq->p) {
		svsutil_FreeFixed (pSeq, pfmOrderSeq);
		return NULL;
	}

	pSeq->pQueue    = pQueue;
	pSeq->fActive   = FALSE;
	pSeq->ipAddr.S_un.S_addr = INADDR_NONE;

	pSeq->pPrevActive = NULL;
	pSeq->pNextActive = NULL;

	pSeq->pNextHash = pSeqBuckets[iPos];
	pSeqBuckets[iPos] = pSeq;

	pSeq->p->guidQ          = pQueue->qp.guid;
	pSeq->p->guidQueueName  = *pguidQueueName;
	pSeq->p->guidStreamId   = *pguidStreamId;
	pSeq->p->guidOrderAck   = *pguidOrderAckName;
	pSeq->p->llCurrentSeqID = 0;
	pSeq->p->uiCurrentSeqN  = 0;
	pSeq->p->uiLastAccessS  = 0;
	pSeq->p->uiBlockFlags   = uiSequenceFlags;

	return pSeq;
}

int ScSequenceCollection::SaveSequence (ScOrderSeq *pSeq) {
	SVSUTIL_ASSERT (pSeq->pQueue);
	SVSUTIL_ASSERT (pSeq->p);

	int iCluster = FindCluster (pSeq->p);
	if (iCluster < 0)
		return FALSE;

	pSeq->p->uiLastAccessS = scutil_now ();

	if ((! pSeq->fActive) && pSeq->p->llCurrentSeqID) {
		SVSUTIL_ASSERT (pSeq->p->uiCurrentSeqN);
		SVSUTIL_ASSERT(! pSeq->pPrevActive);
		SVSUTIL_ASSERT(! pSeq->pNextActive);

		pSeq->fActive     = TRUE;
		pSeq->pNextActive = pSeqActive;
		pSeqActive        = pSeq;

		if (pSeq->pNextActive)
			pSeq->pNextActive->pPrevActive = pSeq;
		else								// Only start pulsing on the first active event...
			svsutil_SetAttrTimer (gMem->pTimer, (SVSHandle)hSequencePulse, SCSEQUENCE_R_ACKSEND * gMachine->uiOrderAckScale * 1000);
	}

	ppClusters[iCluster]->iDirty = TRUE;

	return FlushBackup ();
}

void ScSequenceCollection::DeleteQueue (ScQueue *pQueue) {
	SVSUTIL_ASSERT (gMem->IsLocked());

	if (! (pQueue->qp.bIsIncoming && pQueue->qp.bTransactional))
		return;

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_ORDER, L"Deleting sequences associated with %s...\n", pQueue->lpszFormatName);
#endif

	for (int i = 0 ; i < SCSEQUENCE_HASH_BUCKETS ; ++i) {
		ScOrderSeq *pSeq = pSeqBuckets[i];
		ScOrderSeq *pParentSeq = NULL;

		while (pSeq) {
			SVSUTIL_ASSERT (pSeq->p->uiLastAccessS);

			if (pSeq->pQueue == pQueue) {
				SVSUTIL_ASSERT (pSeq->p->guidQ == pQueue->qp.guid);

				if (pSeq->fActive) {
					if (pSeq == pSeqActive) {
						pSeqActive = pSeq->pNextActive;
						if (pSeqActive)
							pSeqActive->pPrevActive = NULL;
					} else {
						SVSUTIL_ASSERT (pSeq->pPrevActive);
						pSeq->pPrevActive->pNextActive = pSeq->pNextActive;
						if (pSeq->pNextActive)
							pSeq->pNextActive->pPrevActive = pSeq->pPrevActive;
					}
				} else
					SVSUTIL_ASSERT ((! pSeq->pPrevActive) && (! pSeq->pNextActive));

				if (! pParentSeq)
					pSeqBuckets[i] = pSeq->pNextHash;
				else
					pParentSeq->pNextHash = pSeq->pNextHash;

				int iCluster = FindCluster (pSeq->p);

				SVSUTIL_ASSERT ((iCluster >= 0) && (iCluster < (int)uiClustersCount));
				SVSUTIL_ASSERT ((ppClusters[iCluster]->h.uiFirstFreeBlock < SCSEQUENCE_ENTRIES_PER_BLOCK) || (ppClusters[iCluster]->h.uiFirstFreeBlock == -1));

				ppClusters[iCluster]->iNumFree++;
				ppClusters[iCluster]->iDirty = TRUE;

				pSeq->p->uiLastAccessS = 0;
				pSeq->p->uiCurrentSeqN = ppClusters[iCluster]->h.uiFirstFreeBlock;
				ppClusters[iCluster]->h.uiFirstFreeBlock = pSeq->p - ppClusters[iCluster]->pos;

				SVSUTIL_ASSERT (ppClusters[iCluster]->h.uiFirstFreeBlock < SCSEQUENCE_ENTRIES_PER_BLOCK);

				ScOrderSeq *pSeqNext = pSeq->pNextHash;
				svsutil_FreeFixed (pSeq, pfmOrderSeq);
				pSeq = pSeqNext;
				continue;
			}

			pParentSeq = pSeq;
			pSeq = pSeq->pNextHash;
		}
	}

	FlushBackup ();
}

int ScSequenceCollection::CloseBackup (void) {
	if (hBackupFile != INVALID_HANDLE_VALUE)
		CloseHandle (hBackupFile);

	hBackupFile = INVALID_HANDLE_VALUE;

	return TRUE;
}

int ScSequenceCollection::CloseStrings (void) {
	if (hStringFile != INVALID_HANDLE_VALUE)
		CloseHandle (hStringFile);

	hStringFile = INVALID_HANDLE_VALUE;

	return TRUE;
}

int ScSequenceCollection::OpenBackup (void) {
	if (hBackupFile != INVALID_HANDLE_VALUE)
		return TRUE;

	hBackupFile = CreateFile (lpszBackupName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);

	if (hBackupFile != INVALID_HANDLE_VALUE)
		return TRUE;

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_ORDER, L"Failed to open file %s\n", lpszBackupName);
#endif

	return FALSE;
}

int ScSequenceCollection::OpenStrings (void) {
	if (hStringFile != INVALID_HANDLE_VALUE)
		return TRUE;

	hStringFile = CreateFile (lpszStringName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);

	if (hStringFile != INVALID_HANDLE_VALUE) {
		if (SetFilePointer (hStringFile, 0, NULL, FILE_END) == 0xffffffff) {
			CloseStrings ();
			return FALSE;
		}

		return TRUE;
	}

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_ORDER, L"Failed to open file %s\n", lpszStringName);
#endif

	return FALSE;
}

int ScSequenceCollection::FlushBackup (void) {
	for (int i = 0 ; i < (int)uiClustersCount ; ++i) {
		if (ppClusters[i]->iDirty) {
			if (! OpenBackup ())
				return FALSE;

			if (SetFilePointer (hBackupFile, i * SC_FS_UNIT, 0, FILE_BEGIN) == 0xffffffff) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_ORDER, L"Can't seek to %d in %s\n", i * SC_FS_UNIT, lpszBackupName);
#endif
				return FALSE;
			}

			DWORD dwBytes = 0;

			if ((! WriteFile (hBackupFile, ppClusters[i]->ac, sizeof(ppClusters[i]->ac), &dwBytes, NULL)) ||
				(dwBytes != sizeof(ppClusters[i]->ac))) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_ORDER, L"Can't write %d bytes to %s\n", sizeof(ppClusters[i]->ac), lpszBackupName);
#endif
				return FALSE;
			}

			ppClusters[i]->iDirty = FALSE;
		}
	}

	if (hBackupFile != INVALID_HANDLE_VALUE) {
		if (SetFilePointer (hBackupFile, i * SC_FS_UNIT, 0, FILE_BEGIN) == 0xffffffff) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"Can't seek to %d in %s\n", i * SC_FS_UNIT, lpszBackupName);
#endif
			return FALSE;
		}

		SetEndOfFile (hBackupFile);
		FlushFileBuffers (hBackupFile);

		uiLastBackupAccessT = GetTickCount ();
	}

	return TRUE;
}

int ScSequenceCollection::AddNewCluster (void) {
	if (uiClustersCount == uiClustersAlloc) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Adding new sequence cluster...\n");
#endif

		unsigned int l_uiClustersAlloc = uiClustersAlloc + SCSEQUENCE_CLUSTERALLOC;
		ScOrderCluster **l_ppClusters = (ScOrderCluster **)g_funcAlloc (sizeof(ScOrderCluster *) * l_uiClustersAlloc, g_pvAllocData);
		if (! l_ppClusters) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"Failed to expand cluster array - no memory...\n");
#endif
			return FALSE;
		}

		if (ppClusters) {
			memcpy (l_ppClusters, ppClusters, sizeof(ScOrderCluster *) * uiClustersAlloc);
			g_funcFree (ppClusters, g_pvFreeData);
		}

		ppClusters = l_ppClusters;
		uiClustersAlloc = l_uiClustersAlloc;
	}

	ScOrderCluster *pCluster = (ppClusters[uiClustersCount] = (ScOrderCluster *)g_funcAlloc (sizeof(ScOrderCluster), g_pvAllocData));

	if (! pCluster) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Failed to alloc new sequence cluster...\n");
#endif
		return FALSE;
	}

	++uiClustersCount;

	memset (&pCluster->ac, 0, sizeof(pCluster->ac));

	pCluster->iNumFree    = SCSEQUENCE_ENTRIES_PER_BLOCK;
	pCluster->iDirty      = TRUE;
	pCluster->h.uiFirstFreeBlock = 0;
	pCluster->h.uiMagic1  = SCSEQUENCE_MAGIC1;
	pCluster->h.uiMagic2  = SCSEQUENCE_MAGIC2;
	pCluster->h.uiVersion = SCSEQUENCE_VERSION;

	for (int i = 0 ; i < SCSEQUENCE_ENTRIES_PER_BLOCK - 1; ++i)
		pCluster->pos[i].uiCurrentSeqN = i + 1;

	pCluster->pos[i].uiCurrentSeqN = (DWORD)-1;

	return TRUE;
}

ScPersistOrderSeq *ScSequenceCollection::PersistBlock (void) {
	for (int i = 0 ; i < (int)uiClustersCount ; ++i) {
		if (ppClusters[i]->h.uiFirstFreeBlock != -1) {
			SVSUTIL_ASSERT (ppClusters[i]->h.uiFirstFreeBlock < SCSEQUENCE_ENTRIES_PER_BLOCK);

			ScPersistOrderSeq *p = &ppClusters[i]->pos[ppClusters[i]->h.uiFirstFreeBlock];

			SVSUTIL_ASSERT (p->uiLastAccessS == 0);
			SVSUTIL_ASSERT (p->uiCurrentSeqN < SCSEQUENCE_ENTRIES_PER_BLOCK);

			ppClusters[i]->iDirty = TRUE;
			ppClusters[i]->h.uiFirstFreeBlock = p->uiCurrentSeqN;

			memset (p, 0, sizeof(*p));
			return p;
		}
	}

	if (! AddNewCluster ())
		return NULL;

	return PersistBlock();
}

int ScSequenceCollection::Compact (void) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_ORDER, L"Compacting %s...\n", lpszBackupName);
#endif

	uiLastCompactT = GetTickCount ();

	int iUsedSeqs = 0;
	for (int i = 0 ; i < (int)uiClustersCount ; ++i)
		iUsedSeqs += SCSEQUENCE_ENTRIES_PER_BLOCK - ppClusters[i]->iNumFree;

	int iDeltaSeqs = SCSEQUENCE_ENTRIES_PER_BLOCK * uiClustersCount - iUsedSeqs;
	if (iDeltaSeqs < (int)(SCSEQUENCE_ENTRIES_PER_BLOCK * uiClustersCount / 3))
		return TRUE;

	if (iDeltaSeqs < SCSEQUENCE_ENTRIES_PER_BLOCK)
		return TRUE;

	//
	//	We will reset the whole structure now...
	//
	unsigned int	l_uiClustersCount = uiClustersCount;
	unsigned int	l_uiClustersAlloc = uiClustersAlloc;

	ScOrderCluster	**l_ppClusters	  = ppClusters;

	ppClusters      = NULL;
	uiClustersCount = 0;

	int fFaulted = FALSE;

	for (i = 0 ; i < SCSEQUENCE_HASH_BUCKETS ; ++i) {
		ScOrderSeq *pSeq = pSeqBuckets[i];

		while (pSeq) {
			SVSUTIL_ASSERT (pSeq->p->uiLastAccessS);

			ScPersistOrderSeq *p = PersistBlock ();
			if (! p) {
				fFaulted = TRUE;
				break;
			}

			*p = *pSeq->p;
			pSeq->p = p;
			pSeq = pSeq->pNextHash;
		}

		if (fFaulted)
			break;
	}

	if (fFaulted) {
		for (i = 0 ; i < (int)uiClustersCount ; ++i)
			g_funcFree (ppClusters[i], g_pvFreeData);

		g_funcFree (ppClusters, g_pvFreeData);

		ppClusters      = l_ppClusters;
		uiClustersCount = l_uiClustersCount;
		uiClustersAlloc = l_uiClustersAlloc;

		return FALSE;
	}

	for (i = 0 ; i < (int)l_uiClustersCount ; ++i)
		g_funcFree (l_ppClusters[i], g_pvFreeData);

	g_funcFree (l_ppClusters, g_pvFreeData);

	return FlushBackup ();
}

int ScSequenceCollection::Prune (void) {
	SVSUTIL_ASSERT (gMem->IsLocked());

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_ORDER, L"Pruning %s...\n", lpszBackupName);
#endif

	uiLastPruneT = GetTickCount ();

	int iChanges = 0;

	unsigned int uiNowS = scutil_now ();

	for (int i = 0 ; i < SCSEQUENCE_HASH_BUCKETS ; ++i) {
		ScOrderSeq *pSeq = pSeqBuckets[i];
		ScOrderSeq *pParentSeq = NULL;

		while (pSeq) {
			SVSUTIL_ASSERT (pSeq->p->uiLastAccessS);

			if (uiNowS - pSeq->p->uiLastAccessS > gMachine->uiSeqTimeout) {
				SVSUTIL_ASSERT (! pSeq->fActive);

				++iChanges;

				if (! pParentSeq)
					pSeqBuckets[i] = pSeq->pNextHash;
				else
					pParentSeq->pNextHash = pSeq->pNextHash;

				int iCluster = FindCluster (pSeq->p);

				SVSUTIL_ASSERT ((iCluster >= 0) && (iCluster < (int)uiClustersCount));
				SVSUTIL_ASSERT ((ppClusters[iCluster]->h.uiFirstFreeBlock < SCSEQUENCE_ENTRIES_PER_BLOCK) || (ppClusters[iCluster]->h.uiFirstFreeBlock == -1));

				ppClusters[iCluster]->iNumFree++;
				ppClusters[iCluster]->iDirty = TRUE;

				pSeq->p->uiLastAccessS = 0;
				pSeq->p->uiCurrentSeqN = ppClusters[iCluster]->h.uiFirstFreeBlock;
				ppClusters[iCluster]->h.uiFirstFreeBlock = pSeq->p - ppClusters[iCluster]->pos;
				
				SVSUTIL_ASSERT (ppClusters[iCluster]->h.uiFirstFreeBlock < SCSEQUENCE_ENTRIES_PER_BLOCK);

				ScOrderSeq *pSeqNext = pSeq->pNextHash;
				svsutil_FreeFixed (pSeq, pfmOrderSeq);
				pSeq = pSeqNext;
				continue;
			}

			pParentSeq = pSeq;
			pSeq = pSeq->pNextHash;
		}
	}

	int iRes = TRUE;

	if (iChanges) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Squeezed %d sequences out of %s\n", iChanges, lpszBackupName);
#endif
		iRes = Compact ();
		if (iRes)
			iRes = FlushBackup ();
	}

	PruneStringHash ();

	return iRes;
}

ScSequenceCollection::ScSequenceCollection (void) {
	hBackupFile = INVALID_HANDLE_VALUE;
	hStringFile = INVALID_HANDLE_VALUE;

	lpszBackupName = NULL;
    lpszStringName = NULL;

	uiLastBackupAccessT = uiLastStringAccessT = uiLastCompactT = uiLastPruneT = GetTickCount ();

	uiClustersCount = 0;
	uiClustersAlloc = 0;

	ppClusters = NULL;

	memset (pSeqBuckets, 0, sizeof(pSeqBuckets));
	memset (pGuidBuckets, 0, sizeof(pGuidBuckets));
	memset (pStringBuckets, 0, sizeof(pStringBuckets));

	pSeqActive = NULL;

	pfmOrderSeq = svsutil_AllocFixedMemDescr (sizeof (ScOrderCluster), SCSEQUENCE_BLOCKINCR);

	hSequencePulse = CreateEvent (NULL, FALSE, FALSE, NULL);

	fBusy = 0;
}

ScSequenceCollection::~ScSequenceCollection (void) {
	CloseBackup ();
	CloseStrings ();

	CloseHandle (hSequencePulse);

	if (lpszBackupName)
		g_funcFree (lpszBackupName, g_pvFreeData);

	if (lpszStringName)
		g_funcFree (lpszStringName, g_pvFreeData);

	if (ppClusters) {
		for (int i = 0 ; i < (int)uiClustersCount ; ++i)
			g_funcFree (ppClusters[i], g_pvFreeData);

		g_funcFree (ppClusters, g_pvFreeData);
	}

	svsutil_ReleaseFixedNonEmpty (pfmOrderSeq);

	for (int i = 0 ; i < SCSEQUENCE_HASH_BUCKETS ; ++i) {
		while (pStringBuckets[i]) {
			StringGuidHash *pNext = pStringBuckets[i]->pNextInString;
			g_funcFree (pStringBuckets[i], g_pvFreeData);
			pStringBuckets[i] = pNext;
		}
	}

	memset (pGuidBuckets, 0, sizeof(pGuidBuckets));
}

int ScSequenceCollection::Load (void) {
	WCHAR szBuffer[_MAX_PATH];

	int iDirLen = wcslen (gMachine->lpszDirName);
	SVSUTIL_ASSERT (iDirLen > 0);
	SVSUTIL_ASSERT (iDirLen < _MAX_PATH - SVSUTIL_ARRLEN(SCSEQUENCE_FILENAME) - 1 - 4);	// Last "4" is for .new extension

	memcpy (szBuffer, gMachine->lpszDirName, sizeof(WCHAR) * iDirLen);

	szBuffer[iDirLen] = L'\\';
	++iDirLen;
	wcscpy (&szBuffer[iDirLen], SCSEQUENCE_FILENAME);

	lpszBackupName = svsutil_wcsdup (szBuffer);

	wcscpy (&szBuffer[iDirLen], SCSEQUENCE_FILENAME_STRING);

	lpszStringName = svsutil_wcsdup (szBuffer);

	if (! LoadStringHash ()) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Can't load %s\n", lpszStringName);
#endif
		return FALSE;
	}

	HANDLE hFile = CreateFile (lpszBackupName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		hFile = CreateFile (lpszBackupName, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_FLAG_RANDOM_ACCESS, NULL);

		if (hFile == INVALID_HANDLE_VALUE) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"Can't create %s\n", lpszBackupName);
#endif
			return FALSE;
		}

		CloseHandle (hFile);
		return TRUE;
	}

	for ( ; ; ) {
		if (! AddNewCluster()) {
			CloseHandle (hFile);
			return FALSE;
		}

		ScOrderCluster *pCluster = ppClusters[uiClustersCount - 1];

		DWORD dwBytes;

		if ((! ReadFile (hFile, pCluster->ac, sizeof(ppClusters[0]->ac), &dwBytes, NULL)) ||
			(dwBytes != sizeof(ppClusters[0]->ac))) {
			//
			//	Last cluster unused - delete...
			//
			--uiClustersCount;
			g_funcFree (pCluster, g_pvFreeData);
			break;
		}
		//
		//	Check integrity of a file
		//
		if ((pCluster->h.uiMagic1 != SCSEQUENCE_MAGIC1) ||
			(pCluster->h.uiMagic2 != SCSEQUENCE_MAGIC2) ||
			(pCluster->h.uiVersion != SCSEQUENCE_VERSION)) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"File %s corrupted - unrecognized cluster structure\n", lpszBackupName);
#endif
			CloseHandle (hFile);
			return FALSE;
		}

		pCluster->iNumFree = 0;
		pCluster->iDirty   = FALSE;

		int iFreePos = pCluster->h.uiFirstFreeBlock;
		while (iFreePos != -1) {
			if (iFreePos < 0 || iFreePos >= SCSEQUENCE_ENTRIES_PER_BLOCK) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_ORDER, L"File %s corrupted - free position %d out of range\n", lpszBackupName, iFreePos);
#endif
				CloseHandle (hFile);
				return FALSE;
			}

			if (pCluster->pos[iFreePos].uiLastAccessS) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_ORDER, L"File %s corrupted - free position %d not really free\n", lpszBackupName, iFreePos);
#endif
				CloseHandle (hFile);
				return FALSE;
			}

			++pCluster->iNumFree;

			iFreePos = pCluster->pos[iFreePos].uiCurrentSeqN;
		}

		//
		//	Build hashes...
		//
		int iUsed = 0;

		for (int i = 0 ; i < SCSEQUENCE_ENTRIES_PER_BLOCK ; ++i) {
			if (pCluster->pos[i].uiLastAccessS) {
				++iUsed;

				ScQueueList *pql = gQueueMan->pqlIncoming;

				ScQueue *pQueueLocal = NULL;

				while (pql) {
					if (pql->pQueue->qp.guid == pCluster->pos[i].guidQ) {
						pQueueLocal = pql->pQueue;
						break;
					}

					pql = pql->pqlNext;
				}

				//
				//	Queue or endpoint bindings disappeared since; free the entry
				//
				if ((! pQueueLocal) || (! GetStringFromGUID (&pCluster->pos[i].guidOrderAck)) ||
				                       (! GetStringFromGUID (&pCluster->pos[i].guidQueueName))) {
#if defined (SC_VERBOSE)
					if (! pQueueLocal)
						scerror_DebugOut (VERBOSE_MASK_ORDER, L"Queue for guid " SC_GUID_FORMAT L" had disappeared. \n",
										SC_GUID_ELEMENTS((&(pCluster->pos[i].guidQ))));

					if (! GetStringFromGUID (&pCluster->pos[i].guidOrderAck))
						scerror_DebugOut (VERBOSE_MASK_ORDER, L"Order Ack Queue name for guid " SC_GUID_FORMAT L" had disappeared. \n",
										SC_GUID_ELEMENTS((&(pCluster->pos[i].guidOrderAck))));

					if (! GetStringFromGUID (&pCluster->pos[i].guidOrderAck))
						scerror_DebugOut (VERBOSE_MASK_ORDER, L"Queue name for guid " SC_GUID_FORMAT L" had disappeared. \n",
										SC_GUID_ELEMENTS((&(pCluster->pos[i].guidQueueName))));

					scerror_DebugOut (VERBOSE_MASK_ORDER, L"Element at position %d in cluster %d is now freed\n", i, uiClustersCount);
#endif
					--iUsed;
					++pCluster->iNumFree;

					pCluster->iDirty = TRUE;

					pCluster->pos[i].uiLastAccessS = 0;
					pCluster->pos[i].uiCurrentSeqN = pCluster->h.uiFirstFreeBlock;

					pCluster->h.uiFirstFreeBlock = i;

					continue;
				}

				SVSUTIL_ASSERT (pQueueLocal->qp.bIsIncoming && pQueueLocal->qp.bTransactional);

				ScOrderSeq *pSeq = (ScOrderSeq *)svsutil_GetFixed (pfmOrderSeq);

				if (! pSeq) {
#if defined (SC_VERBOSE)
					scerror_DebugOut (VERBOSE_MASK_ORDER, L"Can't get sequence - no memory\n");
#endif
					CloseHandle (hFile);
					return FALSE;
				}

				int iPos = Hash (&pCluster->pos[i].guidStreamId, &pCluster->pos[i].guidQueueName);

				pSeq->pNextHash = pSeqBuckets[iPos];
				pSeqBuckets[iPos] = pSeq;

				pSeq->fActive = FALSE;
				pSeq->ipAddr.S_un.S_addr = INADDR_NONE;

				pSeq->pNextActive = NULL;
				pSeq->pPrevActive = NULL;

				pSeq->pQueue      = pQueueLocal;

				pSeq->p = &pCluster->pos[i];
			}
		}
	}

	CloseHandle (hFile);

	PruneStringHash ();

	return TRUE;
}

void ScSequenceCollection::SendAcks (void) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_ORDER, L"Sending acks...\n");
#endif

	ScOrderSeq *pSeq = pSeqActive;

	unsigned int uiNowS = scutil_now ();

	while (pSeq) {
		SVSUTIL_ASSERT (pSeq->fActive);
		SVSUTIL_ASSERT (pSeq->p);
		SVSUTIL_ASSERT (pSeq->pQueue);

		ScOrderSeq *pSeqNext = pSeq->pNextActive;

		if (uiNowS - pSeq->p->uiLastAccessS < SCSEQUENCE_R_ACKPERIOD * gMachine->uiOrderAckScale) {
			SVSUTIL_ASSERT (pSeq->p->llCurrentSeqID && pSeq->p->uiCurrentSeqN);
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"Sending ack for sequence %016I64x:%d to ack queue %s from transact queue %s\n", pSeq->p->llCurrentSeqID, pSeq->p->uiCurrentSeqN, GetStringFromGUID(&pSeq->p->guidOrderAck), pSeq->pQueue->lpszFormatName);
#endif
			gQueueMan->SendOrderAck (pSeq);
		} else {	// Deactivate it...
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"Deactivating order sequence %016I64x:%d to ack queue %s from transact queue %s\n", pSeq->p->llCurrentSeqID, pSeq->p->uiCurrentSeqN, GetStringFromGUID(&pSeq->p->guidOrderAck), pSeq->pQueue->lpszFormatName);
#endif
			if (pSeq == pSeqActive) {
				SVSUTIL_ASSERT (! pSeq->pPrevActive);
				pSeqActive = pSeq->pNextActive;
			} else
				SVSUTIL_ASSERT (pSeq->pPrevActive);

			if (pSeq->pPrevActive)
				pSeq->pPrevActive->pNextActive = pSeq->pNextActive;

			if (pSeq->pNextActive)
				pSeq->pNextActive->pPrevActive = pSeq->pPrevActive;

			pSeq->fActive = FALSE;
			pSeq->pNextActive = pSeq->pPrevActive = NULL;
		}

		pSeq = pSeqNext;
	}

	if (pSeqActive)
		svsutil_SetAttrTimer (gMem->pTimer, (SVSHandle)hSequencePulse, SCSEQUENCE_R_ACKSEND * gMachine->uiOrderAckScale * 1000);
}

//
//	Persistent String-GUID map
//
StringGuidHash *ScSequenceCollection::GetHashBucket (WCHAR *sz) {
	unsigned int uiNdx = Hash (sz);
	StringGuidHash *pH = pStringBuckets[uiNdx];

	while (pH) {
		if (wcsicmp (pH->szString, sz) == 0)
			return pH;
		pH = pH->pNextInString;
	}

	return NULL;
}

StringGuidHash *ScSequenceCollection::GetHashBucket (GUID *pguid) {
	unsigned int uiNdx = Hash (pguid);
	StringGuidHash *pH = pGuidBuckets[uiNdx];

	while (pH) {
		if (pH->guid == *pguid)
			return pH;
		pH = pH->pNextInGUID;
	}

	return NULL;
}

#if defined (MSMQ_ANCIENT_OS) && ! defined (ERROR_DUPLICATE_TAG)
#define ERROR_DUPLICATE_TAG              2014L
#endif

int ScSequenceCollection::AddNewPairToHash (WCHAR *szString, GUID *pguid, int fAddToFile) {
	if (GetHashBucket (szString)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Duplicate string %s in hash\n", szString);
#endif
		return ERROR_DUPLICATE_TAG;
	}

	if (GetHashBucket (pguid)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Duplicate guid " SC_GUID_FORMAT L"in hash\n", SC_GUID_ELEMENTS(pguid));
#endif
		return ERROR_DUPLICATE_TAG;
	}

	if (fAddToFile) {
		if (! OpenStrings ())
			return FALSE;

		uiLastStringAccessT = GetTickCount ();

		ScStringStoreEntry e;

		e.guid = *pguid;
		e.uiStringSize = sizeof(WCHAR) * (1 + wcslen  (szString));

		DWORD dwCurrentFilePosition = SetFilePointer (hStringFile, 0, NULL, FILE_CURRENT);

		DWORD dwWrit = 0;
		if ((! WriteFile (hStringFile, &e, offsetof(ScStringStoreEntry, szString), &dwWrit, NULL)) ||
										(dwWrit != offsetof(ScStringStoreEntry, szString)) ||
			(! WriteFile (hStringFile, szString, e.uiStringSize, &dwWrit, NULL)) ||
										(dwWrit != e.uiStringSize)) {
			if (dwCurrentFilePosition != 0xffffffff) {
				SetFilePointer (hStringFile, dwCurrentFilePosition, NULL, FILE_BEGIN);
				SetEndOfFile (hStringFile);
				FlushFileBuffers (hStringFile);
			}

			return FALSE;
		}
		FlushFileBuffers (hStringFile);
	}

	unsigned int cStringHash = Hash (szString);
	unsigned int cGuidHash = Hash (pguid);

	StringGuidHash *pH = (StringGuidHash *)g_funcAlloc (offsetof (StringGuidHash, szString) + sizeof(WCHAR)*(1 + wcslen (szString)),g_pvAllocData);
	if (! pH) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Can't alloc StringGuidHash! - out of memory\n");
#endif
		return ERROR_OUTOFMEMORY;
	}

	pH->guid = *pguid;
	pH->uiRef = 0;
	wcscpy (pH->szString, szString);

	pH->pNextInGUID = pGuidBuckets[cGuidHash];
	pGuidBuckets[cGuidHash] = pH;

	pH->pNextInString = pStringBuckets[cStringHash];
	pStringBuckets[cStringHash] = pH;

	return ERROR_SUCCESS;
}

int ScSequenceCollection::LoadStringHash (void) {
	DWORD dwAttrib = GetFileAttributes (lpszStringName);
	if (dwAttrib == 0xffffffff) {	// Does not exist. Check for .new file
		WCHAR szNewName[_MAX_PATH];
		wcscpy (szNewName, lpszStringName);
		wcscat (szNewName, L".new");
		if (0xffffffff != GetFileAttributes (szNewName))
			MoveFile (szNewName, lpszStringName);
	}

	HANDLE hFile = CreateFile (lpszStringName, GENERIC_READ, 0, NULL, OPEN_ALWAYS, FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	int iRes = FALSE;

	WCHAR szString [MSMQ_SC_MAX_URI_BYTES/sizeof(WCHAR)];

	for ( ; ; ) {
		ScStringStoreEntry e;
		DWORD dwRead = 0;
		if (! ReadFile (hFile, &e, offsetof (ScStringStoreEntry, szString), &dwRead, NULL)) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"String store corrupt: read failed (header)\n");
#endif
			break;
		}

		if (dwRead == 0) {
			iRes = TRUE;
			break;
		}

		if (dwRead != offsetof (ScStringStoreEntry, szString)) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"String store corrupt: failed reading %d bytes of header\n", offsetof (ScStringStoreEntry, szString));
#endif
			break;
		}

		if ((e.uiStringSize > sizeof(szString)) || (e.uiStringSize == 0) || (e.uiStringSize & 1)) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"String store corrupt: too big or 0-sized string (%d bytes)\n", e.uiStringSize);
#endif
			break;
		}

		if ((! ReadFile (hFile, szString, e.uiStringSize, &dwRead, NULL)) || (dwRead != e.uiStringSize)) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"String store corrupt: read failed (string)\n");
#endif
			break;
		}

		int cChar = e.uiStringSize / sizeof(WCHAR);
		if (szString[cChar - 1] != '\0') {	// MUST zero-terminate!
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"String store corrupt: string non-zero-terminated\n");
#endif
			break;
		}

		if (ERROR_SUCCESS != AddNewPairToHash (szString, &e.guid, FALSE))
			break;
	}

	CloseHandle (hFile);
	return iRes;
}

void ScSequenceCollection::PruneStringHash (void) {
	for (int i = 0 ; i < SCSEQUENCE_HASH_BUCKETS ; ++i) {
		StringGuidHash *p = pStringBuckets[i];
		while (p) {
			p->uiRef = 0;
			p = p->pNextInString;
		}
	}

	for (i = 0 ; i < SCSEQUENCE_HASH_BUCKETS ; ++i) {
		ScOrderSeq *pSeq = pSeqBuckets[i];
		while (pSeq) {
			StringGuidHash *pH = GetHashBucket (&pSeq->p->guidOrderAck);
			if (pH)
				pH->uiRef++;
			else {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_ORDER, L"Error: no string for queue name guid" SC_GUID_FORMAT L"\n", SC_GUID_ELEMENTS((&pSeq->p->guidOrderAck)));
#endif
				SVSUTIL_ASSERT (0);
			}

			pH = GetHashBucket (&pSeq->p->guidQueueName);
			if (pH)
				pH->uiRef++;
			else {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_ORDER, L"Error: no string for queue name guid" SC_GUID_FORMAT L"\n", SC_GUID_ELEMENTS((&pSeq->p->guidQueueName)));
#endif
				SVSUTIL_ASSERT (0);
			}

			pH = GetHashBucket (&pSeq->p->guidStreamId);
			if (pH)
				pH->uiRef++;
			else {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_ORDER, L"Error: no string for streamid guid" SC_GUID_FORMAT L"\n", SC_GUID_ELEMENTS((&pSeq->p->guidStreamId)));
#endif
				SVSUTIL_ASSERT (0);
			}

			pSeq = pSeq->pNextHash;
		}
	}

	int iEmpty = 0;
	int iTotal = 0;

	for (i = 0 ; i < SCSEQUENCE_HASH_BUCKETS ; ++i) {
		StringGuidHash *p = pStringBuckets[i];
		while (p) {
			++iTotal;
			if (p->uiRef == 0)
				++iEmpty;

			p = p->pNextInString;
		}
	}

	if (iEmpty * SCSEQUENCE_STALEFACTOR > iTotal) {		// Rewrite
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_ORDER, L"Rewriting string hash database\n");
#endif

		CloseStrings ();

		WCHAR szNewFile[_MAX_PATH];
		wcscpy (szNewFile, lpszStringName);
		wcscat (szNewFile, L".new");

		HANDLE hFile = CreateFile (szNewFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_ORDER, L"Can't recreate %s\n", szNewFile);
#endif
			return;
		}

		int fError = FALSE;

		for (i = 0 ; (i < SCSEQUENCE_HASH_BUCKETS) && (! fError); ++i) {
			StringGuidHash *p = pStringBuckets[i];
			while (p) {
				++iTotal;
				if (p->uiRef != 0) {
					ScStringStoreEntry e;

					e.guid = p->guid;
					e.uiStringSize = sizeof(WCHAR) * (1 + wcslen  (p->szString));
 
					DWORD dwWrit = 0;
					if ((! WriteFile (hStringFile, &e, offsetof(ScStringStoreEntry, szString), &dwWrit, NULL)) ||
										(dwWrit != offsetof(ScStringStoreEntry, szString)) ||
						(! WriteFile (hStringFile, p->szString, e.uiStringSize, &dwWrit, NULL)) ||
										(dwWrit != e.uiStringSize)) {
						fError = TRUE;
						break;
					}
				}

				p = p->pNextInString;
			}
		}

		CloseHandle (hFile);

		if (fError) 
			DeleteFile (szNewFile);
		else {
			DeleteFile (lpszStringName);
			MoveFile (szNewFile, lpszStringName);
		}
	}
}

WCHAR *ScSequenceCollection::GetStringFromGUID (GUID *pguid) {
	StringGuidHash *pH = GetHashBucket (pguid);
	if (! pH)
		return NULL;
	return pH->szString;
}

int ScSequenceCollection::HashStringToGUID (WCHAR *szString, GUID *pGuid) {
	StringGuidHash *pH = GetHashBucket (szString);
	if (pH) {
		*pGuid = pH->guid;
		return TRUE;
	}

	int iRes;
	do {
		scutil_uuidgen (pGuid);
		iRes = AddNewPairToHash (szString, pGuid, TRUE);
	} while (iRes == ERROR_DUPLICATE_TAG);

	return iRes == ERROR_SUCCESS ? TRUE : FALSE;
}

