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

    scfile.cxx

Abstract:

    Small client file class support


--*/
#include <sc.hxx>

#include <scfile.hxx>
#include <scqueue.hxx>
#include <scpacket.hxx>
#include <scutil.hxx>
#include <sccomp.hxx>

ScFile::ScFile (WCHAR *a_lpszFileName, ScQueue *a_pQueue) {
	hBackupFile = INVALID_HANDLE_VALUE;	
	lpszFileName = svsutil_wcsdup (a_lpszFileName);
	tLastUsed   = scutil_now();
	pFileDir    = NULL;
	pPacketDir  = NULL;
	pMap        = new SVSExpArray<int>;
	iFreeIndex  = -1;
	iMapSize    = 0;

	iUsedSpace  = 0;

	fFileCreated= FALSE;

	pQueue      = a_pQueue;
}

ScFile::~ScFile (void) {
	Close (TRUE);

	if (pFileDir)
		g_funcFree (pFileDir, g_pvFreeData);

	if (pPacketDir)
		g_funcFree (pPacketDir, g_pvFreeData);

	if (pMap)
		delete pMap;

	if (lpszFileName)
		g_funcFree (lpszFileName, g_pvFreeData);
}

ScFileQueueHeader *ScFile::ComputeHeaderSection (int *piSize) {
	*piSize = 0;

	WCHAR *lpszPathName = scutil_MakePathName (pQueue->qp.bIsIncoming ? L"." : pQueue->lpszQueueHost,
												pQueue->lpszQueueName, &pQueue->qp);

	int ccPathNameLen = wcslen(lpszPathName) + 1;
	int ccLabelLen = pQueue->lpszQueueLabel ? wcslen(pQueue->lpszQueueLabel) + 1 : 0;

	int iQHSz = sizeof(ScFileQueueHeader) + (ccPathNameLen + ccLabelLen) * sizeof(WCHAR);

	ScFileQueueHeader *pqh = (ScFileQueueHeader *)g_funcAlloc (iQHSz, g_pvAllocData);

	if (! pqh) {
		scerror_Complain (MSMQ_SC_ERRMSG_OUTOFMEMORY);
		return NULL;
	}

	memset (pqh, 0, iQHSz);

	pqh->qp                = pQueue->qp;
	pqh->tCreationTime     = pQueue->tCreation;
	pqh->tModificationTime = pQueue->tModification;
	pqh->uiLabelOffset     = ccLabelLen ? sizeof(ScFileQueueHeader) : 0;
	pqh->uiPathNameOffset  = sizeof(ScFileQueueHeader) + ccLabelLen * sizeof(WCHAR);

	if (ccLabelLen)
		memcpy ((unsigned char *)pqh + pqh->uiLabelOffset, pQueue->lpszQueueLabel, sizeof(WCHAR) * ccLabelLen);

	memcpy ((unsigned char *)pqh + pqh->uiPathNameOffset, lpszPathName, ccPathNameLen * sizeof(WCHAR));

	g_funcFree (lpszPathName, g_pvFreeData);

	*piSize = iQHSz;

	return pqh;
}

int ScFile::Restore (void) {
	int iRes = Open ();
	if (! iRes)
		return FALSE;

	pFileDir = (ScFileDirectory *)g_funcAlloc (sizeof(ScFileDirectory), g_pvAllocData);

	if (! pFileDir) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Can't alloc %d bytes for file directory...\n", sizeof(ScFileDirectory));
#endif
		return FALSE;
	}

	DWORD dwSize = 0;

	if ((! ReadFile (hBackupFile, pFileDir, sizeof(*pFileDir), &dwSize, NULL)) ||
										(dwSize              != sizeof(*pFileDir))  ||
										(pFileDir->uiMagic1  != SCFILE_MAGIC1)  ||
										(pFileDir->uiMagic2  != SCFILE_MAGIC2)  ||
										(pFileDir->uiVersion != SCFILE_VERSION)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Can't read %d bytes from %s (file directory)...\n", sizeof(ScFileDirectory), lpszFileName);
#endif
		return FALSE;
	}

	if (0xFFFFFFFF == SetFilePointer (hBackupFile, pFileDir->uiQueueHeaderOffset,
		NULL, FILE_BEGIN)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Can't seek to %d at %s...\n", pFileDir->uiQueueHeaderOffset, lpszFileName);
#endif
		return FALSE;
	}

	ScFileQueueHeader *pQueueDescr = (ScFileQueueHeader *)g_funcAlloc (pFileDir->uiQueueHeaderSize, g_pvAllocData);

	if (! pQueueDescr) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Can't alloc %d bytes for queue header...\n", pFileDir->uiQueueHeaderSize);
#endif
		return FALSE;
	}

	if ((! ReadFile (hBackupFile, pQueueDescr, pFileDir->uiQueueHeaderSize,
															&dwSize, NULL)) ||
															(dwSize != pFileDir->uiQueueHeaderSize)) {
		g_funcFree (pQueueDescr, g_pvFreeData);

#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Can't read %d bytes from %s (queue header)...\n", pFileDir->uiQueueHeaderSize, lpszFileName);
#endif
		return FALSE;
	}

	if (pQueueDescr->qp.bAuthenticate ||
			pQueueDescr->qp.uiPrivacyLevel ||
			(pQueueDescr->qp.bIsJournalOn && (! pQueueDescr->qp.bHasJournal)) ||
			(pQueueDescr->qp.bIsJournal && pQueueDescr->qp.bHasJournal) ||
			((! pQueueDescr->qp.bIsIncoming) &&
				(pQueueDescr->qp.bIsJournal || pQueueDescr->qp.bHasJournal ||
				pQueueDescr->qp.bIsDeadLetter || pQueueDescr->qp.bIsMachineJournal))) {
		g_funcFree (pQueueDescr, g_pvFreeData);

#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Failed -- incorrect queue parameters in file %s...\n", lpszFileName);
#endif
		return FALSE;
	}

	pQueue->qp = pQueueDescr->qp;

	pQueue->tCreation     = pQueueDescr->tCreationTime;
	pQueue->tModification = pQueueDescr->tModificationTime;

	if ((! pQueueDescr->uiPathNameOffset) || (pQueueDescr->uiPathNameOffset >= pFileDir->uiQueueHeaderSize) ||
		(pQueueDescr->uiLabelOffset && (pQueueDescr->uiPathNameOffset >= pFileDir->uiQueueHeaderSize))) {

		g_funcFree (pQueueDescr, g_pvFreeData);

#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Failed -- incorrect queue parameters in file %s...\n", lpszFileName);
#endif
		return FALSE;
	}

	WCHAR *lpPathName = (WCHAR *)((unsigned char *)pQueueDescr + 
								pQueueDescr->uiPathNameOffset);

	WCHAR *lpHostName  = NULL;
	WCHAR *lpQueueName = NULL;

	if (! scutil_ParsePathName (lpPathName, lpHostName, lpQueueName, &pQueue->qp)) {
		g_funcFree (pQueueDescr, g_pvFreeData);

#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Failed -- path name not parsable in file %s...\n", lpszFileName);
#endif
		return FALSE;
	}

	pQueue->lpszQueueHost = svsutil_StringHashAlloc (gMem->pStringHash, lpHostName);
	g_funcFree (lpHostName, g_pvFreeData);

	pQueue->lpszQueueName = svsutil_StringHashAlloc (gMem->pStringHash, lpQueueName);
	g_funcFree (lpQueueName, g_pvFreeData);

	if (pQueueDescr->uiLabelOffset)
		pQueue->lpszQueueLabel = svsutil_wcsdup ((WCHAR *)((unsigned char *)pQueueDescr + 
								pQueueDescr->uiLabelOffset));

	g_funcFree (pQueueDescr, g_pvFreeData);

	if (0xFFFFFFFF == SetFilePointer (hBackupFile, pFileDir->uiPacketDirOffset,
		NULL, FILE_BEGIN)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Can't seek to %d at %s...\n", pFileDir->uiPacketDirOffset, lpszFileName);
#endif
		return FALSE;
	}

	pPacketDir = (ScFilePacketDir *)g_funcAlloc (pFileDir->uiPacketDirSize, g_pvAllocData);

	if (! pPacketDir) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Can't alloc %d bytes for packet dir...\n", pFileDir->uiPacketDirSize);
#endif
		return FALSE;
	}

	if ((! ReadFile (hBackupFile, pPacketDir, pFileDir->uiPacketDirSize, &dwSize, NULL)) ||
		(dwSize != pFileDir->uiPacketDirSize)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Can't read %d bytes from %s (packet dir)...\n", pFileDir->uiPacketDirSize, lpszFileName);
#endif

		return FALSE;
	}

	if (pPacketDir->uiUsedEntries > 0) {
		int fResetPointer = TRUE;

		for (int i = 0 ; i < (int)pPacketDir->uiUsedEntries ; ++i) {
			if (pPacketDir->sPacketEntry[i].uiType == SCFILE_PACKET_EMPTY) {
				fResetPointer = TRUE;
				continue;
			}

			if (pPacketDir->sPacketEntry[i].uiType != SCFILE_PACKET_LIVE) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_FILE, L"Failed - packet %d has wrong packet type in file %s (packet storage)...\n", i, lpszFileName);
#endif
				return FALSE;
			}

			if (fResetPointer && (0xFFFFFFFF == SetFilePointer (hBackupFile,
									pFileDir->uiPacketBackupOffset +
									pPacketDir->sPacketEntry[i].uiOffset,
									NULL, FILE_BEGIN))) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_FILE, L"Can't seek to %d for packet %d in file %s (packet storage)...\n", pFileDir->uiPacketBackupOffset +
									pPacketDir->sPacketEntry[i].uiOffset, i, lpszFileName);
#endif
				return FALSE;
			}

			int iChunkSize = pPacketDir->sPacketEntry[i + 1].uiOffset -
							 pPacketDir->sPacketEntry[i].uiOffset;

			ScPacketImage *pPacketImage = (ScPacketImage *)g_funcAlloc (ScPacketImage::PersistedOffset () + iChunkSize, g_pvAllocData);

			if (! pPacketImage) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_FILE, L"Can't alloc %d bytes for packet...\n", ScPacketImage::PersistedOffset () + iChunkSize);
#endif
				return FALSE;
			}

			pPacketImage->pvBinary = NULL;
			pPacketImage->pvExt = NULL;
			memset (&pPacketImage->sect, 0, sizeof (pPacketImage->sect));
			pPacketImage->allflags = 0;

			if ((! ReadFile (hBackupFile, pPacketImage->PersistedStart (), iChunkSize, &dwSize, NULL)) ||
				((int)dwSize != iChunkSize)) {
				g_funcFree (pPacketImage, g_pvFreeData);

#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_FILE, L"Can't read %d bytes for packet %d in file %s (packet storage)...\n", iChunkSize, i, lpszFileName);
#endif

				return FALSE;
			}

			if (! pPacketImage->PopulateSections ()) {
				g_funcFree (pPacketImage, g_pvFreeData);
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_FILE, L"Can't parse packet %d in file %s (packet storage)...\n", i, lpszFileName);
#endif
				return FALSE;
			}

			(*pMap).SRealloc(iMapSize + 1);
			(*pMap)[iMapSize] = i;
			++iMapSize;

			if (! pQueue->qp.bIsIncoming) {
				if (pQueue->qp.bTransactional) {
					if (! pPacketImage->sect.pXactHeader) {
						g_funcFree (pPacketImage, g_pvFreeData);
#if defined (SC_VERBOSE)
						scerror_DebugOut (VERBOSE_MASK_FILE | VERBOSE_MASK_PACKETS, L"Packet %d in file %s in transactional queue but lacks Xact section (packet storage)...\n", i, lpszFileName);
#endif
						return FALSE;
					}

					LONGLONG	 llNewSeqID  = pPacketImage->sect.pXactHeader->GetSeqID ();
					unsigned int uiNewSeqNum = pPacketImage->sect.pXactHeader->GetSeqN ();

					if (! pQueue->llSeqID)
						pQueue->llSeqID = llNewSeqID;

					if ((pQueue->uiSeqNum == uiNewSeqNum) || (pQueue->llSeqID != llNewSeqID)) {
						g_funcFree (pPacketImage, g_pvFreeData);
#if defined (SC_VERBOSE)
						scerror_DebugOut (VERBOSE_MASK_FILE | VERBOSE_MASK_PACKETS | VERBOSE_MASK_ORDER, L"Packet %d in file %s - SeqNums mismatched (packet storage)...\n", i, lpszFileName);
#endif
						return FALSE;
					}

					if (pQueue->uiSeqNum < uiNewSeqNum)
						pQueue->uiSeqNum = uiNewSeqNum;
				} else if (pPacketImage->sect.pXactHeader) {
					g_funcFree (pPacketImage, g_pvFreeData);
#if defined (SC_VERBOSE)
					scerror_DebugOut (VERBOSE_MASK_FILE | VERBOSE_MASK_PACKETS, L"Packet %d in file %s not in transactional out-queue but has Xact section (packet storage)...\n", i, lpszFileName);
#endif
					return FALSE;
				}
			}

			if (pPacketImage->PersistedSize() != iChunkSize) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_FILE | VERBOSE_MASK_PACKETS, L"Packet %d in file %s - size mismatch (packet storage)...\n", i, lpszFileName);
#endif
				g_funcFree (pPacketImage, g_pvFreeData);
				return FALSE;
			}

			ScPacket *pPacket = pQueue->MakePacket (pPacketImage, iMapSize - 1, FALSE);
			if (! pPacket) {
#if defined (SC_VERBOSE)
				scerror_DebugOut (VERBOSE_MASK_FILE | VERBOSE_MASK_PACKETS, L"Packet %d in file %s - could not insert (packet storage)...\n", i, lpszFileName);
#endif
				g_funcFree (pPacketImage, g_pvFreeData);
				return FALSE;
			}

			iUsedSpace += iChunkSize;

			if (pQueue->hkReceived < pPacket->hkOrderKey)
				pQueue->hkReceived = pPacket->hkOrderKey;
		}
	}

	return TRUE;
}

int ScFile::CreateNew (void) {
	int iQHSz;
	ScFileQueueHeader *pqh = ComputeHeaderSection (&iQHSz);

	if (! pqh)
		return FALSE;

	if (! (pFileDir = (ScFileDirectory *)g_funcAlloc (sizeof(*pFileDir), g_pvAllocData))) {
		g_funcFree (pqh, g_pvFreeData);

		return FALSE;
	}

	memset (pFileDir, 0, sizeof(*pFileDir));

	pFileDir->uiMagic1			= SCFILE_MAGIC1;
	pFileDir->uiMagic2			= SCFILE_MAGIC2;
	pFileDir->uiVersion			= SCFILE_VERSION;
	pFileDir->uiQueueHeaderOffset = sizeof (ScFileDirectory);
	pFileDir->uiQueueHeaderSize   = iQHSz;
	pFileDir->uiPacketDirOffset   = ((pFileDir->uiQueueHeaderOffset + 
										  pFileDir->uiQueueHeaderSize) /
										  SCFILE_DIRECTORY_ALIGNMENT + 1 ) *
										  SCFILE_DIRECTORY_ALIGNMENT;
	pFileDir->uiPacketDirSize     = offsetof (ScFilePacketDir, sPacketEntry) +
										  sizeof(ScPacketEntry) *
										  SCFILE_DIRECTORY_INITSIZE;
	pFileDir->uiPacketBackupOffset= pFileDir->uiPacketDirOffset +
										  pFileDir->uiPacketDirSize;
	pFileDir->uiPacketBackupSize  = 0;


	if (! (pPacketDir = (ScFilePacketDir *)g_funcAlloc (pFileDir->uiPacketDirSize, g_pvAllocData))) {
		g_funcFree (pqh, g_pvFreeData);
		return FALSE;
	}

	memset (pPacketDir, 0, pFileDir->uiPacketDirSize);
	pPacketDir->uiNumEntries           = SCFILE_DIRECTORY_INITSIZE;
	pPacketDir->sPacketEntry[0].uiType = SCFILE_PACKET_LAST;

	hBackupFile = CreateFile (lpszFileName, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hBackupFile == INVALID_HANDLE_VALUE) {
		g_funcFree (pqh, g_pvFreeData);
		return FALSE;
	}

	DWORD dwSize = 0;
	if ((! WriteFile(hBackupFile, pFileDir, sizeof(*pFileDir), &dwSize, NULL)) || 
		(dwSize != sizeof(*pFileDir))) {
		g_funcFree (pqh, g_pvFreeData);
		return FALSE;
	}

	if ((! WriteFile(hBackupFile, pqh, iQHSz, &dwSize, NULL)) || ((int)dwSize != iQHSz)) {
		g_funcFree (pqh, g_pvFreeData);
		return FALSE;
	}

	g_funcFree (pqh, g_pvFreeData);

	if (0xFFFFFFFF == SetFilePointer (hBackupFile, pFileDir->uiPacketDirOffset,
															NULL, FILE_BEGIN))
		return FALSE;

	if ((! WriteFile(hBackupFile, pPacketDir, pFileDir->uiPacketDirSize, &dwSize, NULL)) || 
		(dwSize != pFileDir->uiPacketDirSize))
		return FALSE;

	CloseHandle (hBackupFile);
	hBackupFile = INVALID_HANDLE_VALUE;

	fFileCreated = TRUE;

	return TRUE;
}

int ScFile::Open (void) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	pQueue->tModification = tLastUsed = scutil_now();

	if (hBackupFile != INVALID_HANDLE_VALUE)
		return TRUE;
	
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_FILE, L"Physically opening file %s\n", lpszFileName);
#endif

	hBackupFile = CreateFile (lpszFileName, GENERIC_READ | GENERIC_WRITE, 0,
								NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS,
								NULL);

	if (hBackupFile == INVALID_HANDLE_VALUE)
		return FALSE;

	return TRUE;
}

int ScFile::Close (int fForce) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	if (hBackupFile == INVALID_HANDLE_VALUE)
		return TRUE;

	if (! (pPacketDir && pFileDir)) {
		CloseHandle (hBackupFile);
		hBackupFile = INVALID_HANDLE_VALUE;
		return TRUE;
	}

	if (! fForce) {
		if (time_greater (tLastUsed + SCFILE_INACTIVITY_THRESHOLD, scutil_now()))
			return TRUE;
	}

#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_FILE, L"Physically closing file %s\n", lpszFileName);
#endif

	int fAdjusted = FALSE;

	int iUnroll = pPacketDir->uiUsedEntries;

	while ((pPacketDir->uiUsedEntries > 0) && (pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries - 1].uiType == SCFILE_PACKET_EMPTY)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Truncating file %s\n", lpszFileName);
#endif
		fAdjusted = TRUE;
		pPacketDir->uiUsedEntries--;
		pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiType   = SCFILE_PACKET_LAST;
	}

	int fError = FALSE;

	while (fAdjusted) {
		if (0xFFFFFFFF == SetFilePointer (hBackupFile,
				pFileDir->uiPacketBackupOffset + pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset,
				NULL, FILE_BEGIN)) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_FILE, L"Attempt to truncate %s failed - can't seek...\n", lpszFileName);
#endif
			fError = TRUE;
			break;
		}

		if (! SetEndOfFile (hBackupFile)) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_FILE, L"Attempt to truncate %s failed - can't EOF...\n", lpszFileName);
#endif
			fError = TRUE;
			break;
		}

		pFileDir->uiPacketBackupSize = pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset;

		if (! UpdateSection (SCFILE_UPDATE_DIR | SCFILE_UPDATE_PACKETDIR)) {
#if defined (SC_VERBOSE)
			scerror_DebugOut (VERBOSE_MASK_FILE, L"Attempt to truncate %s failed - can't rewrite...\n", lpszFileName);
#endif
			fError = TRUE;
		}

		break;
	}

	if (fError) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Attempt to truncate %s failed - unrolling changes...\n", lpszFileName);
#endif

		while ((int)pPacketDir->uiUsedEntries < iUnroll)
			pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries++].uiType   = SCFILE_PACKET_EMPTY;

		pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiType   = SCFILE_PACKET_LAST;
		pFileDir->uiPacketBackupSize = pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset;
	}

	CloseHandle (hBackupFile);
	hBackupFile = INVALID_HANDLE_VALUE;

	return TRUE;
}

int ScFile::Delete (int fForce) {
	Close (TRUE);
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_FILE, L"Physically deleting %s\n", lpszFileName);
#endif
	if (fForce || fFileCreated)
		DeleteFile (lpszFileName);

	return TRUE;
}

int ScFile::UpdateSection (unsigned int uiWhichSection) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);

	if (! Open ())
		return FALSE;

	DWORD dwWritten;

	if (uiWhichSection & SCFILE_UPDATE_HEADER) {
		if (0xffffffff == SetFilePointer (hBackupFile, pFileDir->uiQueueHeaderOffset, NULL, FILE_BEGIN))
			return FALSE;

		int iQHSz;
		ScFileQueueHeader *pqh = ComputeHeaderSection (&iQHSz);

		if ((! WriteFile (hBackupFile, pqh, iQHSz, &dwWritten, NULL)) || ((int)dwWritten != iQHSz)) {
			g_funcFree (pqh, g_pvFreeData);
			return FALSE;
		}
		g_funcFree (pqh, g_pvFreeData);

		if (iQHSz != (int)pFileDir->uiQueueHeaderSize) {
			uiWhichSection |= SCFILE_UPDATE_DIR;
			pFileDir->uiQueueHeaderSize = iQHSz;
			SVSUTIL_ASSERT (SCFILE_DIRECTORY_ALIGNMENT > pFileDir->uiQueueHeaderOffset + pFileDir->uiQueueHeaderSize);
		}
	}

	if (uiWhichSection & SCFILE_UPDATE_DIR) {
		if (0xffffffff == SetFilePointer (hBackupFile, 0, NULL, FILE_BEGIN))
			return FALSE;

		if ((! WriteFile (hBackupFile, pFileDir, sizeof (ScFileDirectory), &dwWritten, NULL)) ||
			(dwWritten != sizeof(ScFileDirectory)))
			return FALSE;
	}

	if (uiWhichSection & SCFILE_UPDATE_PACKETDIR) {
		if (0xffffffff == SetFilePointer (hBackupFile, pFileDir->uiPacketDirOffset, NULL, FILE_BEGIN))
			return FALSE;

		if ((! WriteFile (hBackupFile, pPacketDir, pFileDir->uiPacketDirSize, &dwWritten, NULL)) ||
			(dwWritten != pFileDir->uiPacketDirSize))
			return FALSE;
	}

	FlushFileBuffers (hBackupFile);

	return TRUE;
}

int ScFile::Compact (unsigned int &uiWhichSection) {
#if defined (SC_VERBOSE)
	scerror_DebugOut (VERBOSE_MASK_FILE, L"Compacting %s...\n", lpszFileName);
#endif

	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);

	if (! pPacketDir->uiUsedEntries) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Compacting %s -- no changes...\n", lpszFileName);
#endif
		return FALSE;
	}

	int *piMap2 = (int *)g_funcAlloc (sizeof(int) * pPacketDir->uiUsedEntries, g_pvAllocData);

	int iPrevEmpty  = FALSE;

	int iDst = 0;
	for (int i = 0 ; i < (int)pPacketDir->uiUsedEntries ; ++i) {
		if (pPacketDir->sPacketEntry[i].uiType == SCFILE_PACKET_EMPTY) {
			if (iPrevEmpty) {
				piMap2[i] = -1;
				continue;
			}

			iPrevEmpty = TRUE;
		} else {
			SVSUTIL_ASSERT (pPacketDir->sPacketEntry[i].uiType != SCFILE_PACKET_LAST);
			iPrevEmpty = FALSE;
		}

		piMap2[i] = iDst;
		pPacketDir->sPacketEntry[iDst++] = pPacketDir->sPacketEntry[i];
	}

	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);

	if (iDst == i) {	// No changes
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Compacting %s -- no changes...\n", lpszFileName);
#endif
		g_funcFree (piMap2, g_pvFreeData);
		return FALSE;
	}
	//
	//	Free chunks consolidated. Rewrite file map and return TRUE to indicate that
	//	update is in order.
	//
	pPacketDir->sPacketEntry[iDst] = pPacketDir->sPacketEntry[i];
	pPacketDir->uiUsedEntries      = iDst;

	for (i = 0 ; i < iMapSize ; ++i) {
		if ((*pMap)[i] > 0) {
			SVSUTIL_ASSERT (piMap2[(*pMap)[i]] >= 0);

			(*pMap)[i] = piMap2[(*pMap)[i]];
		}
	}

	g_funcFree (piMap2, g_pvFreeData);

	uiWhichSection |= SCFILE_UPDATE_PACKETDIR;

	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);

	return TRUE;
}

int ScFile::GetFreePacketNdx(void) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);

	if (iFreeIndex == -1) {
		(*pMap).SRealloc(iMapSize + 1);
		(*pMap)[iMapSize] = -1;
		iFreeIndex = -2 - iMapSize;
		++iMapSize;
	}

	SVSUTIL_ASSERT (iFreeIndex < -1);
	int iNdx = -2 - iFreeIndex;

	SVSUTIL_ASSERT (iNdx < iMapSize);

	iFreeIndex = (*pMap)[iNdx];

	return iNdx;
}

void ScFile::ReleasePacketNdx(int iNdx) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT ((iNdx >= 0) && (iNdx < iMapSize));

	(*pMap)[iNdx] = iFreeIndex;
	iFreeIndex = -2 - iNdx;
}

int ScFile::Rewrite (int iIncPacketDir) {
	if (! Open()) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Failed to open file %s\n", lpszFileName);
#endif
		return FALSE;
	}

	WCHAR *lpszNewName = svsutil_wcsdup (lpszFileName);
	WCHAR *lpszExt = wcsrchr (lpszNewName, L'.');
	SVSUTIL_ASSERT (lpszExt && lpszExt[1] && (lpszExt[1] != L'r'));

	int iExt = lpszExt - lpszNewName + 1;

	lpszNewName[iExt] = L'1';

	HANDLE hNewFile = CreateFile (lpszNewName, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hNewFile == INVALID_HANDLE_VALUE) {
		g_funcFree (lpszNewName, g_pvFreeData);
		return FALSE;
	}

	if (iIncPacketDir)
		iIncPacketDir = SCFILE_DIRECTORY_INCREMENT;

	int iNewPacketDirSize = pFileDir->uiPacketDirSize + iIncPacketDir * sizeof(int);
	ScFilePacketDir *pNewPacketDir = (ScFilePacketDir *)g_funcAlloc (iNewPacketDirSize, g_pvAllocData);
	if (! pNewPacketDir) {
		CloseHandle (hNewFile);
		DeleteFile (lpszNewName);
		g_funcFree (lpszNewName, g_pvFreeData);
		return FALSE;
	}

	memset (pNewPacketDir, 0, iNewPacketDirSize);

	pNewPacketDir->uiNumEntries  = pPacketDir->uiNumEntries + iIncPacketDir;

	int *pMapMap = (int *)g_funcAlloc (pPacketDir->uiUsedEntries * sizeof(int), g_pvAllocData);

	if (! pMapMap) {
		g_funcFree (pNewPacketDir, g_pvFreeData);
		CloseHandle (hNewFile);
		DeleteFile (lpszNewName);
		g_funcFree (lpszNewName, g_pvFreeData);
		return FALSE;
	}

	int iQHSz;
	ScFileQueueHeader *pqh = ComputeHeaderSection (&iQHSz);

	if (! pqh) {
		g_funcFree (pMapMap, g_pvFreeData);
		g_funcFree (pNewPacketDir, g_pvFreeData);
		CloseHandle (hNewFile);
		DeleteFile (lpszNewName);
		g_funcFree (lpszNewName, g_pvFreeData);
		return FALSE;
	}

	ScFileDirectory sNewFileDir;

	memset (&sNewFileDir, 0, sizeof(sNewFileDir));

	sNewFileDir.uiMagic1			= SCFILE_MAGIC1;
	sNewFileDir.uiMagic2			= SCFILE_MAGIC2;
	sNewFileDir.uiVersion			= SCFILE_VERSION;
	sNewFileDir.uiQueueHeaderOffset = sizeof (ScFileDirectory);
	sNewFileDir.uiQueueHeaderSize   = iQHSz;
	sNewFileDir.uiPacketDirOffset   = ((sNewFileDir.uiQueueHeaderOffset + 
										  sNewFileDir.uiQueueHeaderSize) /
										  SCFILE_DIRECTORY_ALIGNMENT + 1 ) *
										  SCFILE_DIRECTORY_ALIGNMENT;

	sNewFileDir.uiPacketDirSize     = iNewPacketDirSize;
	sNewFileDir.uiPacketBackupOffset= sNewFileDir.uiPacketDirOffset +
										  sNewFileDir.uiPacketDirSize;

	sNewFileDir.uiPacketBackupSize  = 0;

	DWORD dwBytes = 0;
	int fError = FALSE;

	for ( ; ; ) {
		if ((! WriteFile (hNewFile, &sNewFileDir, sizeof(sNewFileDir), &dwBytes, NULL)) ||
			(dwBytes != sizeof(sNewFileDir))) {
			fError = TRUE;
			break;
		}

		if ((! WriteFile (hNewFile, pqh, iQHSz, &dwBytes, NULL)) || ((int)dwBytes != iQHSz)) {
			fError = TRUE;
			break;
		}

		if ((0xFFFFFFFF == SetFilePointer (hNewFile, sNewFileDir.uiPacketDirOffset, NULL, FILE_BEGIN)) ||
			(! WriteFile (hNewFile, pNewPacketDir, sNewFileDir.uiPacketDirSize, &dwBytes, NULL)) ||
			(dwBytes != sNewFileDir.uiPacketDirSize)) {
			fError = TRUE;
			break;
		}

		void *pvBuffer = NULL;
		int iBufferSize = 0;

		pNewPacketDir->sPacketEntry[0].uiOffset = 0;

		for (int i = 0 ; i < (int)pPacketDir->uiUsedEntries ; ++i) {
			if (pPacketDir->sPacketEntry[i].uiType == SCFILE_PACKET_LIVE) {
				int iSz = pPacketDir->sPacketEntry[i + 1].uiOffset - pPacketDir->sPacketEntry[i].uiOffset;

				if (iSz <= 0) {
					fError = TRUE;
					break;
				}

				if (iSz > iBufferSize) {
					if (pvBuffer)
						g_funcFree (pvBuffer, g_pvFreeData);

					pvBuffer = g_funcAlloc (iSz, g_pvAllocData);
					if (! pvBuffer) {
						fError = TRUE;
						break;
					}

					iBufferSize = iSz;
				}

				if (0xFFFFFFFF == SetFilePointer (hBackupFile, pFileDir->uiPacketBackupOffset +
					pPacketDir->sPacketEntry[i].uiOffset, NULL, FILE_BEGIN)) {
					fError = TRUE;
					break;
				}

				if ((! ReadFile (hBackupFile, pvBuffer, iSz, &dwBytes, NULL)) || ((int)dwBytes != iSz)) {
					fError = TRUE;
					break;
				}

				if ((! WriteFile (hNewFile, pvBuffer, iSz, &dwBytes, NULL)) || ((int)dwBytes != iSz)) {
					fError = TRUE;
					break;
				}

				pMapMap[i] = pNewPacketDir->uiUsedEntries;
				pNewPacketDir->sPacketEntry[pNewPacketDir->uiUsedEntries].uiType   = SCFILE_PACKET_LIVE;
				pNewPacketDir->sPacketEntry[pNewPacketDir->uiUsedEntries + 1].uiOffset =
						pNewPacketDir->sPacketEntry[pNewPacketDir->uiUsedEntries].uiOffset + iSz;
				++pNewPacketDir->uiUsedEntries;
			} else
				pMapMap[i] = -1;
		}

		if (pvBuffer)
			g_funcFree (pvBuffer, g_pvFreeData);

		if (fError)
			break;

		pNewPacketDir->sPacketEntry[pNewPacketDir->uiUsedEntries].uiType = SCFILE_PACKET_LAST;

		if ((0xFFFFFFFF == SetFilePointer (hNewFile, sNewFileDir.uiPacketDirOffset, NULL, FILE_BEGIN)) ||
			(! WriteFile (hNewFile, pNewPacketDir, sNewFileDir.uiPacketDirSize, &dwBytes, NULL)) ||
			(dwBytes != sNewFileDir.uiPacketDirSize)) {
			fError = TRUE;
			break;
		}


		sNewFileDir.uiPacketBackupSize = pNewPacketDir->sPacketEntry[pNewPacketDir->uiUsedEntries].uiOffset;

		if ((0xFFFFFFFF == SetFilePointer (hNewFile, 0, NULL, FILE_BEGIN)) ||
			(! WriteFile (hNewFile, &sNewFileDir, sizeof(sNewFileDir), &dwBytes, NULL)) ||
			(dwBytes != sizeof(sNewFileDir)))
			fError = TRUE;


		break;
	}

	g_funcFree (pqh, g_pvFreeData);
	CloseHandle (hNewFile);
	Close (TRUE);

	if (fError) {
		g_funcFree (pMapMap, g_pvFreeData);
		g_funcFree (pNewPacketDir, g_pvFreeData);
		DeleteFile (lpszNewName);
		g_funcFree (lpszNewName, g_pvFreeData);
		return FALSE;
	}

	//
	//	Swap places...
	//
	WCHAR *lpszNewName2 = svsutil_wcsdup (lpszFileName);
	lpszNewName2[iExt] = L'2';

	if (! MoveFile (lpszFileName, lpszNewName2)) {
		g_funcFree (pMapMap, g_pvFreeData);
		g_funcFree (pNewPacketDir, g_pvFreeData);
		DeleteFile (lpszNewName);
		g_funcFree (lpszNewName, g_pvFreeData);
		g_funcFree (lpszNewName2, g_pvFreeData);
		return FALSE;
	}

	if (! MoveFile (lpszNewName, lpszFileName)) {
	    MoveFile (lpszNewName2, lpszFileName);
		g_funcFree (pMapMap, g_pvFreeData);
		g_funcFree (pNewPacketDir, g_pvFreeData);
		DeleteFile (lpszNewName);
		g_funcFree (lpszNewName, g_pvFreeData);
		g_funcFree (lpszNewName2, g_pvFreeData);
		return FALSE;
	}

	DeleteFile (lpszNewName2);
	g_funcFree (lpszNewName, g_pvFreeData);
	g_funcFree (lpszNewName2, g_pvFreeData);

	//
	//	Update in-memory structures...
	//
	for (int i = 0 ; i < iMapSize ; ++i) {
		if ((*pMap)[i] > 0) {
			SVSUTIL_ASSERT (pMapMap[(*pMap)[i]] >= 0);

			(*pMap)[i] = pMapMap[(*pMap)[i]];
		}
	}

	g_funcFree (pMapMap, g_pvFreeData);
	g_funcFree (pPacketDir, g_pvFreeData);
	pPacketDir = pNewPacketDir;
	*pFileDir = sNewFileDir;

	if (! Open())
		return FALSE;

	return TRUE;
}

int ScFile::GetSuitableChunk (ScPacket *pPacket, unsigned int &uiWhichSection) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT (pPacket->pImage);
	SVSUTIL_ASSERT (hBackupFile != INVALID_HANDLE_VALUE);
	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);

	int a_iSize = pPacket->pImage->PersistedSize ();

	int iMinNdx  = -1;
	int iMinNdxE = -1;
	int iMinSize = -1;

	for (int i = 0 ; i < (int)pPacketDir->uiUsedEntries ; ++i) {
		if (pPacketDir->sPacketEntry[i].uiType == SCFILE_PACKET_EMPTY) {
			int iStart = i;

			while ((i < (int)pPacketDir->uiUsedEntries) && (pPacketDir->sPacketEntry[i].uiType == SCFILE_PACKET_EMPTY))
				++i;

			int iSize = pPacketDir->sPacketEntry[i].uiOffset - pPacketDir->sPacketEntry[iStart].uiOffset;

			if (iSize > a_iSize) {
				if ((iMinNdx < 0) || (iMinSize > iSize)) {
					iMinSize = iSize;
					iMinNdx  = iStart;
					iMinNdxE = i;
				}
			}
		}
	}

	if (iMinNdx >= 0) {
		if (iMinNdxE - iMinNdx > 1) {
			SVSUTIL_ASSERT (pPacketDir->sPacketEntry[iMinNdx].uiType == SCFILE_PACKET_EMPTY);
			SVSUTIL_ASSERT (pPacketDir->sPacketEntry[iMinNdx + 1].uiType == SCFILE_PACKET_EMPTY);

			pPacketDir->sPacketEntry[iMinNdx + 1].uiOffset = 
							pPacketDir->sPacketEntry[iMinNdx].uiOffset + a_iSize;

			unsigned int uiNewOffset = pPacketDir->sPacketEntry[iMinNdx].uiOffset + iMinSize;

			for (i = iMinNdx + 2 ; i < iMinNdxE ; ++i) {
				SVSUTIL_ASSERT (pPacketDir->sPacketEntry[i].uiType == SCFILE_PACKET_EMPTY);

				pPacketDir->sPacketEntry[i].uiOffset = uiNewOffset;
			}
		} else {
			//
			//	Need to split a range.
			//	Always remember the sentinel. It is not part of used, therefore
			//	implicitly count it in and the copy is also off by one - it
			//	IS correct!
			//
			if (pPacketDir->uiUsedEntries >= pPacketDir->uiNumEntries - 1)
				return -1;	// No space in the directory

			memmove (&pPacketDir->sPacketEntry[iMinNdx + 2], &pPacketDir->sPacketEntry[iMinNdx + 1],
					sizeof (ScPacketEntry) * (pPacketDir->uiUsedEntries - iMinNdx));

			pPacketDir->sPacketEntry[iMinNdx + 1].uiType   = SCFILE_PACKET_EMPTY;
			pPacketDir->sPacketEntry[iMinNdx + 1].uiOffset = pPacketDir->sPacketEntry[iMinNdx].uiOffset + a_iSize;
			pPacketDir->uiUsedEntries++;

			//
			//	Now shift all map entries, too
			//
			for (i = 0 ; i < iMapSize ; ++i) {
				if ((*pMap)[i] > iMinNdx)
					++(*pMap)[i];
			}
		}

		uiWhichSection |= SCFILE_UPDATE_PACKETDIR;
		return iMinNdx;
	}

	//
	//	We will try to expand the file here. Untill we are sure it is physically
	//	possible, we will not modify wait
	//
	if (pPacketDir->uiUsedEntries >= pPacketDir->uiNumEntries - 1) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"No more directory space in %s\n", lpszFileName);
#endif
		return -1;	// No space in the directory
	}

	if (pQueue->qp.uiQuotaK < (pFileDir->uiPacketBackupOffset + pFileDir->uiPacketBackupSize + a_iSize) / 1024) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Quota exhausted in %s\n", lpszFileName);
#endif
		return -1;	// Quota exhausted
	}

	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);

	if (0xFFFFFFFF == SetFilePointer (hBackupFile, pFileDir->uiPacketBackupOffset + pFileDir->uiPacketBackupSize,
															NULL, FILE_BEGIN)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Cant seek to %d in %s\n", pFileDir->uiPacketBackupOffset + pFileDir->uiPacketBackupSize, lpszFileName);
#endif
		return -1;
	}

	DWORD dwSize = 0;

	if ((! WriteFile(hBackupFile, pPacket->pImage->PersistedStart (), a_iSize,
												&dwSize, NULL)) || ((int)dwSize != a_iSize)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Cant write %d bytes in %s\n", a_iSize, lpszFileName);
#endif
		return -1;
	}

	iMinNdx = pPacketDir->uiUsedEntries;
	++pPacketDir->uiUsedEntries;

	pPacketDir->sPacketEntry[iMinNdx + 1].uiType = SCFILE_PACKET_LAST;
	pPacketDir->sPacketEntry[iMinNdx + 1].uiOffset = pPacketDir->sPacketEntry[iMinNdx].uiOffset + a_iSize;

	pPacketDir->sPacketEntry[iMinNdx].uiType = SCFILE_PACKET_EMPTY;

	pFileDir->uiPacketBackupSize += a_iSize;

	uiWhichSection |= (SCFILE_UPDATE_PACKETWRITTEN | SCFILE_UPDATE_PACKETDIR | SCFILE_UPDATE_DIR);

	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);
	return iMinNdx;
}

int ScFile::HaveSpaceInQuota (int a_iSize) {
	if (pQueue->qp.uiQuotaK < (pFileDir->uiPacketBackupOffset + iUsedSpace + a_iSize) / 1024)
		return FALSE;

	return TRUE;
}

int ScFile::BackupPacket (ScPacket *pPacket, unsigned int &uiWhichSection) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT (IntegrityCheck());
	SVSUTIL_ASSERT (pPacket->pImage);
	SVSUTIL_ASSERT (pQueue->qp.bIsJournal || pQueue->qp.bIsDeadLetter || pQueue->qp.bIsMachineJournal || (pPacket->pImage->sect.pUserHeader->GetDelivery() == MQMSG_DELIVERY_RECOVERABLE));
	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);

	int iSize = pPacket->pImage->PersistedSize ();

	if (! HaveSpaceInQuota(iSize))
		return FALSE;

	if (! Open()) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Failed to open file %s\n", lpszFileName);
#endif
		return FALSE;
	}

	int iNdx = GetFreePacketNdx ();
	int iFileChunk = GetSuitableChunk (pPacket, uiWhichSection);

	if (iFileChunk < 0) {

		if (Compact(uiWhichSection))
			iFileChunk = GetSuitableChunk (pPacket, uiWhichSection);

		if (iFileChunk < 0) {
			if (! Rewrite (TRUE))
				return FALSE;

			iFileChunk = GetSuitableChunk (pPacket, uiWhichSection);

			if (iFileChunk < 0)
				return FALSE;
		}
	}

	DWORD dwSize = 0;

	if (uiWhichSection & SCFILE_UPDATE_PACKETWRITTEN)
		uiWhichSection &= ~SCFILE_UPDATE_PACKETWRITTEN;
	else if ((iFileChunk < 0) || (0xFFFFFFFF == SetFilePointer (hBackupFile,
			pFileDir->uiPacketBackupOffset +
			pPacketDir->sPacketEntry[iFileChunk].uiOffset, NULL, FILE_BEGIN)) ||
			(! WriteFile(hBackupFile, pPacket->pImage->PersistedStart (), iSize,
			&dwSize, NULL)) || ((int)dwSize != iSize)) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE | VERBOSE_MASK_PACKETS, L"Could not backup packet %08x " SC_GUID_FORMAT L" from file %s\n",
										pPacket->uiMessageID, SC_GUID_ELEMENTS((&pPacket->guidSourceQM)), lpszFileName);
#endif
		ReleasePacketNdx (iNdx);
		SVSUTIL_ASSERT (IntegrityCheck());
		return FALSE;
	}

	(*pMap)[iNdx]      = iFileChunk;
	pPacket->iDirEntry = iNdx;
	pPacketDir->sPacketEntry[iFileChunk].uiType = SCFILE_PACKET_LIVE;

	iUsedSpace += pPacketDir->sPacketEntry[iFileChunk+1].uiOffset - pPacketDir->sPacketEntry[iFileChunk].uiOffset;

	uiWhichSection |= SCFILE_UPDATE_PACKETDIR;

	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);
	SVSUTIL_ASSERT (IntegrityCheck());
	return TRUE;
}

int ScFile::GetPacketSize (ScPacket *pPacket) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT (IntegrityCheck());
	SVSUTIL_ASSERT (! pPacket->pImage);
	SVSUTIL_ASSERT ((pPacket->iDirEntry >= 0) && (pPacket->iDirEntry < iMapSize));
	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);

	int iPacketIndex = (*pMap)[pPacket->iDirEntry];
	return pPacketDir->sPacketEntry[iPacketIndex + 1].uiOffset - pPacketDir->sPacketEntry[iPacketIndex].uiOffset;
}

int ScFile::RestorePacket (ScPacket *pPacket) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT (IntegrityCheck());
	SVSUTIL_ASSERT (! pPacket->pImage);
	SVSUTIL_ASSERT ((pPacket->iDirEntry >= 0) && (pPacket->iDirEntry < iMapSize));
	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);

	if (! Open()) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Failed to open file %s\n", lpszFileName);
#endif
		return FALSE;
	}

	int iPacketIndex = (*pMap)[pPacket->iDirEntry];
	int iSize = pPacketDir->sPacketEntry[iPacketIndex + 1].uiOffset - pPacketDir->sPacketEntry[iPacketIndex].uiOffset;

	SVSUTIL_ASSERT ((iPacketIndex >= 0) && (iPacketIndex < (int)pPacketDir->uiUsedEntries));
	SVSUTIL_ASSERT (pPacketDir->sPacketEntry[iPacketIndex].uiType == SCFILE_PACKET_LIVE);

	pPacket->pImage = (ScPacketImage *)g_funcAlloc (ScPacketImage::PersistedOffset() + iSize, g_pvAllocData);

	if (! pPacket->pImage) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Could not allocate %d bytes for packet %08x " SC_GUID_FORMAT L" from file %s\n",
			ScPacketImage::PersistedOffset () + iSize, pPacket->uiMessageID, SC_GUID_ELEMENTS((&pPacket->guidSourceQM)), lpszFileName);
#endif
		return FALSE;
	}

	pPacket->pImage->pvBinary = NULL;
	pPacket->pImage->pvExt    = NULL;

	memset (&pPacket->pImage->sect, 0, sizeof (pPacket->pImage->sect));

	DWORD dwSize = 0;

	if ((0xFFFFFFFF == SetFilePointer (hBackupFile,
		pFileDir->uiPacketBackupOffset + pPacketDir->sPacketEntry[iPacketIndex].uiOffset,
		NULL, FILE_BEGIN)) || (! ReadFile (hBackupFile, pPacket->pImage->PersistedStart (),
		iSize, &dwSize, NULL)) || ((int)dwSize != iSize) ||
		(! pPacket->pImage->PopulateSections ())) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE | VERBOSE_MASK_PACKETS, L"Failed to restore packet %08x " SC_GUID_FORMAT L" from file %s\n",
										pPacket->uiMessageID, SC_GUID_ELEMENTS((&pPacket->guidSourceQM)), lpszFileName);
#endif
		g_funcFree (pPacket->pImage, g_pvFreeData);
		pPacket->pImage = NULL;
		SVSUTIL_ASSERT (IntegrityCheck());
		return FALSE;
	}

	SVSUTIL_ASSERT (pQueue->qp.bIsJournal || pQueue->qp.bIsDeadLetter || pQueue->qp.bIsMachineJournal || (pPacket->pImage->sect.pUserHeader->GetDelivery() == MQMSG_DELIVERY_RECOVERABLE));
	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);
	SVSUTIL_ASSERT (IntegrityCheck());

	return TRUE;
}

int ScFile::DeletePacket (ScPacket *pPacket, unsigned int &uiWhichSection) {
	SVSUTIL_ASSERT (gMem->IsLocked ());
	SVSUTIL_ASSERT (IntegrityCheck());
	SVSUTIL_ASSERT (pPacket->iDirEntry >= 0);
	SVSUTIL_ASSERT (pPacket->iDirEntry < iMapSize);
	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);

	if (! Open()) {
#if defined (SC_VERBOSE)
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Failed to open file %s\n", lpszFileName);
#endif
		return FALSE;
	}

	int iFileMapNdx = (*pMap)[pPacket->iDirEntry];

	SVSUTIL_ASSERT ((iFileMapNdx >= 0) && (iFileMapNdx < (int)pPacketDir->uiUsedEntries));
	SVSUTIL_ASSERT (pPacketDir->sPacketEntry[iFileMapNdx].uiType == SCFILE_PACKET_LIVE);

	pPacketDir->sPacketEntry[iFileMapNdx].uiType = SCFILE_PACKET_EMPTY;
	ReleasePacketNdx (pPacket->iDirEntry);

	iUsedSpace -= pPacketDir->sPacketEntry[iFileMapNdx+1].uiOffset - pPacketDir->sPacketEntry[iFileMapNdx].uiOffset;

	uiWhichSection |= SCFILE_UPDATE_PACKETDIR;

	SVSUTIL_ASSERT (pFileDir->uiPacketBackupSize == pPacketDir->sPacketEntry[pPacketDir->uiUsedEntries].uiOffset);
	SVSUTIL_ASSERT (IntegrityCheck());
	return TRUE;
}

#if defined (SVSUTIL_DEBUG_ANY)
#if (! defined (SC_VERBOSE)) || (! defined (SC_DEBUG_FILE))
int ScFile::IntegrityCheck (void) {
	return TRUE;
}

#else

int ScFile::IntegrityCheck (void) {
	SVSTNode *pNode = pQueue->pPackets->Min ();
	while (pNode) {
		ScPacket *pPacket = (ScPacket *)SVSTree::GetData (pNode);
		if (pPacket->uiPacketState == SCPACKET_STATE_DEAD) {
			pNode = pQueue->pPackets->Next (pNode);
			continue;
		}

		if (pPacket->iDirEntry >= 0) {
			if (pPacket->iDirEntry >= iMapSize) {
				scerror_DebugOut (VERBOSE_MASK_FILE, L"Packet ptr %08x UID %08x " SC_GUID_FORMAT L" from file %s -- dir index out of range.\n", pPacket,
										pPacket->uiMessageID, SC_GUID_ELEMENTS((&pPacket->guidSourceQM)), lpszFileName);
				return FALSE;
			}

			int iFreeX = iFreeIndex;

			while (iFreeX < -1) {
				int iNdx = -2 - iFreeX;

				if (iNdx == pPacket->iDirEntry) {
					scerror_DebugOut (VERBOSE_MASK_FILE, L"Packet ptr %08x UID %08x " SC_GUID_FORMAT L" from file %s -- dir index registered as free.\n", pPacket,
										pPacket->uiMessageID, SC_GUID_ELEMENTS((&pPacket->guidSourceQM)), lpszFileName);
					return FALSE;
				}
				iFreeX = (*pMap)[iNdx];
			}

			int iMapNdx = (*pMap)[pPacket->iDirEntry];
			if ((iMapNdx < 0) || (iMapNdx >= (int)pPacketDir->uiUsedEntries)) {
				scerror_DebugOut (VERBOSE_MASK_FILE, L"Packet ptr %08x UID %08x " SC_GUID_FORMAT L" from file %s -- file index out of bound.\n", pPacket,
										pPacket->uiMessageID, SC_GUID_ELEMENTS((&pPacket->guidSourceQM)), lpszFileName);
				return FALSE;
			}

			if (pPacketDir->sPacketEntry[iMapNdx].uiType != SCFILE_PACKET_LIVE) {
				scerror_DebugOut (VERBOSE_MASK_FILE, L"Packet ptr %08x UID %08x " SC_GUID_FORMAT L" from file %s -- dir entry not live.\n", pPacket,
										pPacket->uiMessageID, SC_GUID_ELEMENTS((&pPacket->guidSourceQM)), lpszFileName);
				return FALSE;
			}
		}

		pNode = pQueue->pPackets->Next (pNode);
	}

	int iUsedSpace2 = 0;

	for (int i = 0 ; i < (int)pPacketDir->uiUsedEntries ; ++i) {
		if (pPacketDir->sPacketEntry[i].uiType == SCFILE_PACKET_LIVE)
			iUsedSpace2 += pPacketDir->sPacketEntry[i + 1].uiOffset - pPacketDir->sPacketEntry[i].uiOffset;
	}

	if (iUsedSpace2 != iUsedSpace) {
		scerror_DebugOut (VERBOSE_MASK_FILE, L"Used space mismatch in %s\n", lpszFileName);
		return FALSE;
	}

	return TRUE;
}
#endif

#endif
