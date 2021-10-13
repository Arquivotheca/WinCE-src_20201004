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

    stream.c

Abstract:

    This file contains routines for manipulating streams.

Revision History:

--*/

#include "fatfs.h"


/*  OpenStream - Open/create stream
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      clusFirst - first cluster of stream (the search criteria)
 *      psid - pointer to SID, NULL if none
 *      pstmParent - pointer to parent DSTREAM (NULL if none/unknown)
 *      pdi - pointer to DIRINFO (NULL if none/unknown)
 *      dwFlags - one or more OPENSTREAM_* flags (eg, OPENSTREAM_CREATE, OPENSTREAM_REFRESH)
 *
 *  EXIT
 *      pointer to DSTREAM if successful (and critical section left held),
 *      NULL if not (ie, out of memory)
 *
 *  NOTES
 *      If clusFirst is UNKNOWN_CLUSTER, then psid becomes the search criteria.
 *
 *      Streams for data that is not cluster-mapped (eg, FAT, root directory)
 *      must be completely contiguous;  their RUN info must be set to match
 *      the size of the stream, so that ReadStream will always call directly
 *      to ReadStreamBuffer without attempting to call UnpackRun (which knows
 *      only how to deal with cluster-mapped data).
 */

PDSTREAM OpenStream(PVOLUME pvol, DWORD clusFirst, PDSID psid, PDSTREAM pstmParent, PDIRINFO pdi, DWORD dwFlags)
{
    PDSTREAM pstm;

    // Assert that at least one search criteria appears valid

    ASSERT(clusFirst != UNKNOWN_CLUSTER? TRUE : (int)psid);

    EnterCriticalSection(&pvol->v_csStms);

    // If the caller wants to create a private stream, then skip the search

    if ((dwFlags & (OPENSTREAM_CREATE|OPENSTREAM_PRIVATE)) != (OPENSTREAM_CREATE|OPENSTREAM_PRIVATE)) {

        for (pstm = pvol->v_dlOpenStreams.pstmNext; pstm != (PDSTREAM)&pvol->v_dlOpenStreams; pstm = pstm->s_dlOpenStreams.pstmNext) {

            if (!(pstm->s_flags & STF_PRIVATE) != !(dwFlags & OPENSTREAM_PRIVATE))
                continue;

            if (clusFirst != UNKNOWN_CLUSTER &&
                    pstm->s_clusFirst == clusFirst ||
                clusFirst == UNKNOWN_CLUSTER &&
                    pstm->s_sid.sid_clusDir == psid->sid_clusDir && pstm->s_sid.sid_ordDir == psid->sid_ordDir) {

				ASSERT(pstm->s_refs != 0);
                goto init;
            }
        }
    }

    // The requested stream is not open.  Bail if the volume is unmounted and
    // has not yet been remounted or recycled, or if the volume is fine and we
    // simply cannot allocate memory for a new stream.

    if (!(dwFlags & OPENSTREAM_CREATE) ||
        (pvol->v_flags & (VOLF_UNMOUNTED | VOLF_REMOUNTED | VOLF_RECYCLED)) == VOLF_UNMOUNTED ||
        !(pstm = LocalAlloc(LPTR, sizeof(DSTREAM)))) {

        LeaveCriticalSection(&pvol->v_csStms);
        return NULL;
    }
    DEBUGALLOC(sizeof(DSTREAM));

    InitializeCriticalSection(&pstm->s_cs);
    DEBUGALLOC(DEBUGALLOC_CS);

    pstm->s_pvol = pvol;
    pstm->s_flags = STF_VOLUME;

    pstm->s_clusFirst = clusFirst;
    if (psid)
        pstm->s_sid = *psid;

    pstm->s_blkDir = INVALID_BLOCK;

    RewindStream(pstm, INVALID_POS);

    // If the caller wanted a PRIVATE root stream, then we have to clone
    // the appropriate fields from the volume's root stream.

    if (dwFlags & OPENSTREAM_PRIVATE) {
        ASSERT(clusFirst);
        pstm->s_flags |= STF_PRIVATE;
        if (clusFirst == ROOT_PSEUDO_CLUSTER) {
            pstm->s_run = pvol->v_pstmRoot->s_run;
            pstm->s_size = pvol->v_pstmRoot->s_size;
        }
    }

    InitList((PDLINK)&pstm->s_dlOpenHandles);
    AddItem((PDLINK)&pvol->v_dlOpenStreams, (PDLINK)&pstm->s_dlOpenStreams);

    // Common code for both cases (old stream, new stream).  An old
    // stream might have been created with minimal information by a thread
    // for its own minimal purposes, but if another thread requests the same
    // stream and provides more detailed information, we need to record/update
    // that information, so that the stream will satisfy both threads'
    // purposes.

  init:
    pstm->s_refs++;

#ifdef TFAT
    // TFAT, add a ref to the parent stream only if it was not added before
    if(pvol->v_fTfat && pstmParent)
    {
        if(pstm->s_pstmParent){
            ASSERT(pstm->s_pstmParent == pstmParent);
        }else{
            pstm->s_pstmParent = pstmParent;
            pstmParent->s_refs++;
        }
    }
#endif


    LeaveCriticalSection(&pvol->v_csStms);
    EnterCriticalSection(&pstm->s_cs);

    



    if (!pstm->s_clusFirst || pstm->s_clusFirst == UNKNOWN_CLUSTER)
        pstm->s_clusFirst = clusFirst;

    if (psid) {
        if (!pstm->s_sid.sid_clusDir || pstm->s_sid.sid_clusDir == UNKNOWN_CLUSTER) {
            pstm->s_sid = *psid;
        }
    }

    if (pdi && (!(pstm->s_flags & STF_INIT) || (dwFlags & OPENSTREAM_REFRESH))) {

        // Whenever pdi is provided, pstmParent must be provided too.

        ASSERT(pstmParent);

        // We save the OEM name in the DIRENTRY for validation during handle
        // regeneration.

        memcpy(pstm->s_achOEM, pdi->di_pde->de_name, sizeof(pdi->di_pde->de_name));
        pstm->s_attr = pdi->di_pde->de_attr;
        pstm->s_size = (pdi->di_pde->de_attr & ATTR_DIR_MASK) == ATTR_DIRECTORY? MAX_DIRSIZE : pdi->di_pde->de_size;

        pstm->s_sidParent = pstmParent->s_sid;
        pstm->s_cwPath = pdi->di_cwName + pstmParent->s_cwPath + 1;

        // We should never need to query or update a DIRECTORY stream's
        // DIRENTRY however (or the date/time values inside it).

        if (!(pstm->s_attr & ATTR_DIRECTORY)) {

            pstm->s_flags &= ~STF_VOLUME;

            // As an optimization, remember the physical block and offset
            // within that block of the stream's DIRENTRY.  This will save
            // us from having to perform a OpenStream/FindNext combination
            // in CommitStream whenever we need to update a stream's DIRENTRY.

            // For that information to be available and valid, however, all
            // callers must have a "lock" on the parent stream, and the stream
            // in turn must have a "current buffer".

            ASSERT(OWNCRITICALSECTION(&pstmParent->s_cs));
            ASSERT(pstmParent->s_pbufCur);

            if (pstmParent->s_pbufCur) {
                pstm->s_blkDir = pstmParent->s_pbufCur->b_blk;
                pstm->s_offDir = (PBYTE)pdi->di_pde - pstmParent->s_pbufCur->b_pdata;
            }

            // DOSTimeToFileTime can fail because it doesn't range-check any of
            // the date/time values in the directory entry;  it just pulls them apart
            // and calls SystemTimeToFileTime, which won't initialize the FILETIME
            // structure if there's an error.  Fortunately, all the FILETIMEs have been
            // zero-initialized, because that's what the Win32 API dictates when a time
            // is unknown/not supported.

            DOSTimeToFileTime(pdi->di_pde->de_createdDate,
                              pdi->di_pde->de_createdTime,
                              pdi->di_pde->de_createdTimeMsec, &pstm->s_ftCreate);

            DOSTimeToFileTime(pdi->di_pde->de_date, pdi->di_pde->de_time, 0, &pstm->s_ftWrite);

            DOSTimeToFileTime(pdi->di_pde->de_lastAccessDate, 0, 0, &pstm->s_ftAccess);
        }

        pstm->s_flags |= STF_INIT;

    }

#ifdef DEBUG
    else {
        if (clusFirst == FAT_PSEUDO_CLUSTER)
            memcpy(pstm->s_achOEM, "<FAT>", 5);
        else if (clusFirst == ROOT_PSEUDO_CLUSTER)
            memcpy(pstm->s_achOEM, "<ROOT>", 6);
    }
#endif

    if (pvol->v_flFATFS & FATFS_FORCE_WRITETHROUGH) 
        pstm->s_flags |= STF_WRITETHRU;

    DEBUGMSGW(ZONE_STREAMS,(DBGTEXTW("FATFS!OpenStream %s stream 0x%08x for '%.11hs'\n"), pstm->s_refs > 1? TEXTW("opened") : TEXTW("created"), pstm, pstm->s_achOEM));
    return pstm;
}














DWORD CloseStream(PDSTREAM pstm)
{
    DWORD dwError = ERROR_SUCCESS;

    if (pstm) {

        PVOLUME pvol = pstm->s_pvol;

        ASSERT(OWNCRITICALSECTION(&pstm->s_cs));

        // When there is no lazy-writer, CloseStream may be the last opportunity we
        // have to visit buffers for this stream, so we need to call CommitStream(TRUE),
        // lest any buffers languish dirty indefinitely -- which would in turn increase
        // the chances of the media appearing inconsistent if it was removed unexpectedly.
        // Of course, we warn the user to put the media back in when that happens, but
        // it's best to prevent such warnings in the first place.
        //
        // When there IS a lazy-writer, we can let it take care of buffers for path-based
        // and volume-based streams, and invoke CommitStream(TRUE) only for handle-based streams.
        //
        // This logic is somewhat arbitrary -- we could always call CommitStream(FALSE) -- but
        // since files are often closed as a result of some explicit user action, and since such
        // actions often precede unexpected media removal, I'm not exactly being paranoid for
        // no reason here.... -JTP

        dwError = CommitStream(pstm, TRUE);

        ASSERT(!(pstm->s_flags & STF_BUFCURHELD) && pstm->s_pbufCur == NULL);

        LeaveCriticalSection(&pstm->s_cs);
        EnterCriticalSection(&pvol->v_csStms);

        if (--pstm->s_refs == 0) {

#ifdef TFAT
            //	TFAT, lower the refcount on the parent stream
            if(pvol->v_fTfat && pstm->s_pstmParent)
            {
                ASSERT(pstm->s_pstmParent->s_refs);
                if(1 == pstm->s_pstmParent->s_refs){
                    EnterCriticalSection(&pstm->s_pstmParent->s_cs);
                    CloseStream(pstm->s_pstmParent);
                } else{
                    --pstm->s_pstmParent->s_refs;
                }
            }
#endif
            
            DEBUGMSG(ZONE_STREAMS,(DBGTEXT("FATFS!CloseStream destroying stream 0x%08x for '%.11hs'\n"), pstm, pstm->s_achOEM));
            RemoveItem((PDLINK)&pstm->s_dlOpenStreams);
            DEBUGFREE(DEBUGALLOC_CS);
            DeleteCriticalSection(&pstm->s_cs);
            DEBUGFREE(sizeof(DSTREAM));
            VERIFYNULL(LocalFree(pstm));
        }

        LeaveCriticalSection(&pvol->v_csStms);
    }
    return dwError;
}


/*  CommitStream - flush buffers (and DIRENTRY) associated with this stream
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      fAll - TRUE to commit all stream buffers, FALSE to just update DIRENTRY
 *
 *  EXIT
 *      ERROR_SUCCESS (0) if successful, non-zero if not
 *
 *  NOTES
 *      As an optimization, OpenStream remembers the physical block and
 *      within that block of the stream's DIRENTRY.  This will save
 *      us from having to perform a OpenStream/FindNext combination
 *      in CommitStream every time we need to update a stream's DIRENTRY.
 *
 *      As an added optimization, OpenStream also records the parent's OID
 *      if a parent stream was specified when the stream was created.  Thus,
 *      CommitStream has all the information it needs to post a CEOID_CHANGED
 *      message if the stream changed, again without having to open the
 *      parent stream.
 */

DWORD CommitStream(PDSTREAM pstm, BOOL fAll)
{
    DWORD dwError = ERROR_SUCCESS;

    ASSERT(OWNCRITICALSECTION(&pstm->s_cs));

    // NOTE: CommitStream refuses to update an unmounted stream because
    // the volume the stream was associated with may have been unmounted and
    // RECYCLED, in which case this stream is dead and its directory entry
    // can never be safely updated.  The stream will eventually go away when
    // all the apps with handles to the stream close their handles.

    if (!(pstm->s_flags & STF_UNMOUNTED)) {

        // Commit all dirty buffers associated with this stream.  There
        // is no return value from this function.  Any held stream buffer will
        // be released as well.

        if (!fAll)
            dwError = ReleaseStreamBuffer(pstm, FALSE);
        else
            dwError = CommitAndReleaseStreamBuffers(pstm);

        // The rest of this is for non-FAT, non-root, non-directory
        // streams only (ie, streams with DIRENTRY's that should be updated).

        if (!(pstm->s_flags & STF_VOLUME)) {

            SYSTEMTIME stLocal;

            GetLocalTime(&stLocal);

            // A stream gets a new access time if it's a new day, unless
            // STF_ACCESS_TIME is set, in which case someone has already set a
            // specific access time.

            



            if ((pstm->s_pvol->v_flags & VOLF_UPDATE_ACCESS) && !(pstm->s_flags & STF_ACCESS_TIME)) {

                SYSTEMTIME stTmp;
                FILETIME ftTmp1, ftTmp2;

                stTmp = stLocal;
                stTmp.wDayOfWeek = stTmp.wHour = stTmp.wMinute = stTmp.wSecond = stTmp.wMilliseconds = 0;

                // Now that I've truncated the local system time to midnight,
                // I need to switch back to non-local time to perform the
                // necessary comparison.

                DEBUGMSG(ZONE_STREAMS && ZONE_LOGIO,(DBGTEXT("FATFS!CommitStream: current (truncated) local system time: y=%d,m=%d,d=%d,h=%d,m=%d,s=%d\n"), stTmp.wYear, stTmp.wMonth, stTmp.wDay, stTmp.wHour, stTmp.wMinute, stTmp.wSecond));
                SystemTimeToFileTime(&stTmp, &ftTmp1);

                DEBUGMSG(ZONE_STREAMS && ZONE_LOGIO,(DBGTEXT("FATFS!CommitStream: current (truncated) local file time: %08x,%08x\n"), ftTmp1.dwLowDateTime, ftTmp1.dwHighDateTime));
                LocalFileTimeToFileTime(&ftTmp1, &ftTmp2);

                DEBUGMSG(ZONE_STREAMS && ZONE_LOGIO,(DBGTEXT("FATFS!CommitStream: current (truncated) file time: %08x,%08x\n"), ftTmp2.dwLowDateTime, ftTmp2.dwHighDateTime));
                if (pstm->s_ftAccess.dwLowDateTime != ftTmp2.dwLowDateTime ||
                    pstm->s_ftAccess.dwHighDateTime != ftTmp2.dwHighDateTime) {

                    pstm->s_ftAccess = ftTmp2;
                    pstm->s_flags |= STF_ACCESS_TIME | STF_DIRTY_INFO;
                }
            }

            if (pstm->s_flags & STF_DIRTY) {

                PBUF pbuf;

                // We don't specify a stream for FindBuffer, because we're
                // not trying to find a buffer for THIS stream, but rather its
                // directory's stream.

                // Also note this means we need to call CommitBuffer and UnholdBuffer
                // when we're done (instead of the usual ModifyStreamBuffer).

                if (pstm->s_blkDir == INVALID_BLOCK) {
                    DEBUGMSGBREAK(TRUE,(DBGTEXT("FATFS!CommitStream: invalid blkDir!\n")));
                    dwError = ERROR_INVALID_BLOCK;
                }
                else
#ifdef TFAT
                    if (!pstm->s_pvol->v_fTfat)
#endif        
                        dwError = FindBuffer(pstm->s_pvol, pstm->s_blkDir, NULL, FALSE, &pbuf);

                if (dwError == ERROR_SUCCESS) 
                {
                    PDIRENTRY pde;
                    DWORD     clusFirst;
                
#ifdef TFAT
                    if (pstm->s_pvol->v_fTfat) {                       
                        DWORD     blkDir;

                        LockFAT (pstm->s_pvol);

                        blkDir = pstm->s_blkDir;
                        ASSERT(pstm->s_offDir<pstm->s_pvol->v_cbClus);
                        
                        // A dir cluster is being modified, lets clone it first
                        CloneDirCluster(pstm->s_pvol, pstm->s_pstmParent, blkDir, &blkDir);

                        if (!(dwError = FindBuffer(pstm->s_pvol, blkDir, NULL, FALSE, &pbuf)))
                        {
                            pde = (PDIRENTRY)(pbuf->b_pdata + pstm->s_offDir);
                            dwError = ModifyBuffer(pbuf, pde, sizeof(DIRENTRY));
                        }

                        UnlockFAT (pstm->s_pvol);                       
                    }
                    else 
#endif
                    {
                        pde = (PDIRENTRY)(pbuf->b_pdata + pstm->s_offDir);
                        dwError = ModifyBuffer(pbuf, pde, sizeof(DIRENTRY));               
                    }

                    if (!dwError) {

                        pstm->s_attr |= ATTR_ARCHIVE;
                        pde->de_attr = pstm->s_attr;

                        pde->de_size = pstm->s_size;

                        clusFirst = pstm->s_clusFirst;
                        if (clusFirst == UNKNOWN_CLUSTER)
                            clusFirst = NO_CLUSTER;
                        SETDIRENTRYCLUSTER(pstm->s_pvol, pde, clusFirst);

                        if (pstm->s_flags & STF_CREATE_TIME) {
                            FileTimeToDOSTime(&pstm->s_ftCreate,
                                            &pde->de_createdDate,
                                            &pde->de_createdTime,
                                            &pde->de_createdTimeMsec);
                            pstm->s_flags &= ~STF_CREATE_TIME;
                        }

                        // A dirty data stream always gets a new write time, unless
                        // STF_WRITE_TIME is set, in which case someone has already set a
                        // specific write time.

                        if ((pstm->s_flags & (STF_DIRTY_DATA | STF_WRITE_TIME)) == STF_DIRTY_DATA) {

                            // We could simply call GetSystemTimeAsFileTime,
                            // with s_ftWrite as the destination, but if both
                            // last access and last write times are being updated,
                            // I prefer to insure that both times are identical.

                            FILETIME ftTmp;
                            SystemTimeToFileTime(&stLocal, &ftTmp);
                            LocalFileTimeToFileTime(&ftTmp, &pstm->s_ftWrite);
                            pstm->s_flags |= STF_WRITE_TIME;
                        }

                        if (pstm->s_flags & STF_WRITE_TIME) {
                            FileTimeToDOSTime(&pstm->s_ftWrite,
                                            &pde->de_date,
                                            &pde->de_time, NULL);
                            pstm->s_flags &= ~STF_WRITE_TIME;
                        }

                        if (pstm->s_flags & STF_ACCESS_TIME) {
                            FileTimeToDOSTime(&pstm->s_ftAccess,
                                            &pde->de_lastAccessDate, NULL, NULL);
                            pstm->s_flags &= ~STF_ACCESS_TIME;
                        }

                        if (fAll)
                            dwError = CommitBuffer(pbuf, FALSE);
                    }
                    // TEST_BREAK
                    PWR_BREAK_NOTIFY(41);

                    UnholdBuffer(pbuf);

#if 0
                    if (!dwError)
                        FILESYSTEMNOTIFICATION(pstm->s_pvol, DB_CEOID_CHANGED, 0, SHCNE_UPDATEITEM, &pstm->s_sid, &pstm->s_sidParent, NULL, NULL, NULL, DBGTEXTW("CommitStream"));
#endif                  

                    // Even if the buffer couldn't be written, the dirt has still
                    // moved from the stream's data structure to the buffer

                    pstm->s_flags &= ~(STF_DIRTY | STF_DIRTY_CLUS);
                }

                // If there was some error getting a buffer however, note that the
                // stream remains dirty.
            }
        }
    }
    else {

        // CommitAndReleaseStreamBuffers would have insured the release for us,
        // but since we can't do a commit, we'll have to insure the release ourselves.

        ReleaseStreamBuffer(pstm, FALSE);
        dwError = !(pstm->s_flags & STF_DIRTY)? ERROR_SUCCESS : ERROR_ACCESS_DENIED;
    }

    DEBUGMSG(ZONE_STREAMS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!CommitStream returned %d for '%.11hs'\n"), dwError, pstm->s_achOEM));
    return dwError;
}


/*  RewindStream
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      pos - position within DSTREAM to rewind to, or INVALID_POS to reinitialize
 *
 *  EXIT
 *      None
 */

void RewindStream(PDSTREAM pstm, DWORD pos)
{
    

    if ((pos == INVALID_POS) || (pos < pstm->s_run.r_start) ||
        ((pstm->s_run.r_clusNext == UNKNOWN_CLUSTER) && (pstm->s_clusFirst != UNKNOWN_CLUSTER))) {
       pstm->s_run.r_start = 0;
       pstm->s_run.r_end = 0;
       pstm->s_run.r_clusThis = UNKNOWN_CLUSTER;
       pstm->s_run.r_clusNext = pstm->s_clusFirst;
       pstm->s_run.r_clusPrev = NO_CLUSTER;
    }
}


/*  SeekStream
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      pos - position within DSTREAM to seek to
 *
 *  EXIT
 *      0 if successful, error code if not
 *      (eg, ERROR_HANDLE_EOF if end of cluster chain reached)
 */

DWORD SeekStream(PDSTREAM pstm, DWORD pos)
{
    DWORD dwError;
    ASSERT(OWNCRITICALSECTION(&pstm->s_cs));

    if ((pos == INVALID_POS) || (pos < pstm->s_run.r_start) ||
    ((pstm->s_run.r_clusNext == UNKNOWN_CLUSTER) && (pstm->s_clusFirst != UNKNOWN_CLUSTER))) {

        RewindStream(pstm, pos);
    }

    // Locate the data run which contains the position requested.

    do {
        

        if (pos < pstm->s_run.r_end)
            return ERROR_SUCCESS;

        DEBUGMSG(ZONE_LOGIO && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!SeekStream(DP): begin unpack\n")));
        dwError = UnpackRun(pstm);
        DEBUGMSG(ZONE_LOGIO && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!SeekStream(DP): end unpack\n")));

        if (dwError)
            break;

    } while (TRUE);

    return dwError;
}


/*  PositionStream - Find cluster containing (or preceding) specified position
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      pos - offset within stream to seek to
 *
 *  EXIT
 *      There are three general cases that callers are concerned about:
 *
 *          UNKNOWN_CLUSTER:    the specified position does not lie
 *                              within a cluster (ie, is beyond EOF)
 *
 *          NO_CLUSTER:         the specified position does not require a
 *                              cluster (in other words, pos is ZERO)
 *
 *          Valid cluster:      There are two cases here:  if pos is on a
 *                              cluster boundary, we will return the cluster
 *                              PRECEDING the one containing pos, otherwise
 *                              we will return the cluster CONTAINING pos.
 */

DWORD PositionStream(PDSTREAM pstm, DWORD pos, PDWORD pdwClus)
{
    DWORD dwError = ERROR_SUCCESS;

    ASSERT(OWNCRITICALSECTION(&pstm->s_cs));

    if (!pdwClus)
        return ERROR_INVALID_DATA;

    if ((pos == INVALID_POS) || (pos < pstm->s_run.r_start) ||
    ((pstm->s_run.r_clusNext == UNKNOWN_CLUSTER) && (pstm->s_clusFirst != UNKNOWN_CLUSTER))) {

        RewindStream(pstm, pos);
    }

    // Locate the data run which contains the position requested.

    do {
        if (pos == pstm->s_run.r_start) {
            *pdwClus = pstm->s_run.r_clusPrev;
            break;
        }

        if (pos <= pstm->s_run.r_end) {

            *pdwClus = pstm->s_run.r_clusThis +
                   ((pos - pstm->s_run.r_start - 1) >>
                    (pstm->s_pvol->v_log2cblkClus + BLOCK_LOG2));
            break;
        }

        // TEST_BREAK
        PWR_BREAK_NOTIFY(42);

        dwError = UnpackRun (pstm);
        if (dwError != ERROR_SUCCESS) {

            // This is OK, just means position is past EOF.
            if (dwError == ERROR_HANDLE_EOF)
                dwError = ERROR_SUCCESS;
            
            *pdwClus = UNKNOWN_CLUSTER;
            break;
        }

    } while (TRUE);

    return dwError;
}


/*  ReadStream
 *
 *  Returns a pointer (to a buffer) containing data from the requested
 *  stream position, and the amount of data available in the buffer.  ppvEnd
 *  is optional as long as the caller knows that the size and alignment of
 *  its data is such that all the data will be buffered;  directory streams
 *  are an example of this:  as long as the caller calls ReadStream before
 *  examining each DIRENTRY record, the caller can be assured that the entire
 *  DIRENTRY is in the buffer (that would not, however, be a particularly
 *  efficient way to scan a directory stream. -JTP)
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      pos - position within DSTREAM to read from
 *      ppvStart - pointer to pointer to receive starting address of data
 *      ppvEnd - pointer to pointer to receive ending address of data
 *
 *  EXIT
 *      0 if successful, error code if not (eg, error reading the disk, or
 *      ERROR_HANDLE_EOF if the end of the stream has been reached).
 */

DWORD ReadStream(PDSTREAM pstm, DWORD pos, PVOID *ppvStart, PVOID *ppvEnd)
{
    DWORD dwError;

    ASSERT(OWNCRITICALSECTION(&pstm->s_cs));

    dwError = SeekStream(pstm, pos);

    if (!dwError)
        dwError = ReadStreamBuffer(pstm, pos, 0, ppvStart, ppvEnd);

    return dwError;
}


/*  ReadStreamData
 *
 *  Whereas ReadStream returns a pointer (to a buffer) and a length,
 *  and then the caller reads whatever data it wants up to that length,
 *  this function takes a memory address and a length and simply reads
 *  that many bytes directly into that address, avoiding the use of
 *  disk buffers whenever possible.
 *
 *  Thus, unlike ReadStream, where the caller can never get more than a
 *  buffer's worth of data back at a time and probably needs to put a loop
 *  around its ReadStream call, ReadStreamData can read any amount of data
 *  with one call (no external loop).
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      pos - position within DSTREAM to read from
 *      pvData - pointer to memory to read to
 *      len - length of data to read
 *      plenRead - pointer to DWORD for length read (optional)
 *
 *  EXIT
 *      0 if successful, error code if not (eg, error reading the disk)
 *
 *  NOTES
 *      To avoid using buffers
 */

DWORD ReadStreamData(PDSTREAM pstm, DWORD pos, PVOID pvData, DWORD len, PDWORD plenRead)
{
    DWORD cb;
    PBYTE pbStart, pbEnd;
    DWORD dwError = ERROR_SUCCESS;

    ASSERT(OWNCRITICALSECTION(&pstm->s_cs));

    // Use cb to temporarily hold the maximum number of bytes that
    // can be read, and then adjust the length of the read downward as needed.

    cb = pos > pstm->s_size? 0 : pstm->s_size - pos;
    if (len > cb)
        len = cb;

    while (len) {

        // If we can lock a range of blocks outside the buffer pool,
        // in order to read their contents directly from disk to memory
        // instead of through the pool, do so.

        DWORD blk, cblk;

        if (dwError = SeekStream(pstm, pos))
            break;

        DEBUGMSG(ZONE_LOGIO && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!ReadStreamData(DP): begin lockblocks for '%.11hs'\n"), pstm->s_achOEM));
        if (LockBlocks(pstm, pos, len, &blk, &cblk, FALSE)) {

            __try {
                DEBUGMSG(ZONE_LOGIO && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!ReadStreamData(DP): begin readvolume\n")));
                dwError = ReadVolume(pstm->s_pvol, blk, cblk, pvData);
                DEBUGMSG(ZONE_LOGIO && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!ReadStreamData(DP): end readvolume\n")));
                if (!dwError) {
                    cb = cblk << BLOCK_LOG2;
                    (PBYTE)pvData += cb;
                    if (plenRead)
                        *plenRead += cb;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                dwError = ERROR_INVALID_PARAMETER;
            }

            UnlockBlocks(blk, cblk, FALSE);
            if (dwError)
                break;
        }
        else {

            DEBUGMSG(ZONE_LOGIO && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!ReadStreamData(DP): begin readstream\n")));
            dwError = ReadStream(pstm, pos, &pbStart, &pbEnd);
            DEBUGMSG(ZONE_LOGIO && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!ReadStreamData(DP): begin readstream\n")));
            if (dwError)
                break;

            cb = pbEnd - pbStart;
            if (cb > len)
                cb = len;

            __try {
                memcpy(pvData, pbStart, cb);
                (PBYTE)pvData += cb;
                if (plenRead)
                    *plenRead += cb;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                dwError = ERROR_INVALID_PARAMETER;
            }

            ReleaseStreamBuffer(pstm, FALSE);
            if (dwError)
                break;
        }

#ifdef DEBUG
        if (ZONE_READVERIFY && ZONE_LOGIO) {
            DWORD i, j;
            for (i=0; i<cb; i++) {
                DEBUGMSG(TRUE,(DBGTEXT("%02x "), pbStart[i]));
                j = i+1;
                if (j == cb || j % 16 == 0) {
                    j = j % 16;
                    if (j == 0)
                        j = i - 15;
                    else
                        j = i - j + 1;
                    while (j <= i) {
                        DEBUGMSG(TRUE,(DBGTEXT("%c"), pbStart[j]<' ' || pbStart[j]>=0x7F? '.' : pbStart[j]));
                        j++;
                    }
                    DEBUGMSG(TRUE,(DBGTEXT("\n")));
                }
            }
        }
#endif
        len -= cb;
        pos += cb;
    }
    return dwError;
}


/*  WriteStreamData
 *
 *  Whereas ReadStream returns a pointer (to a buffer) and a length,
 *  and then the caller reads whatever data it wants up to that length
 *  (or modifies data up to that length and calls ModifyStreamBuffer), this
 *  function takes a pointer to data and its length, the position to write
 *  it, and simply writes it.
 *
 *  Thus, unlike ReadStream, where the caller can never get more than a
 *  buffer's worth of data back at a time and probably needs to put a loop
 *  around its ReadStream call, WriteStreamData can write any amount of data
 *  with one call (no external loop).
 *
 *  The same effect could be achieved with a ReadStream/ModifyStreamBuffer
 *  loop, but there are two important differences:  WriteStreamData can avoid
 *  the overhead of reading data that it's simply going to overwrite anyway,
 *  and it takes care of extending the stream if needed.
 *
 *  So, I've changed ReadStreamBuffer to accept an "advisory" lenMod parm,
 *  allowing us to advise it how many bytes we plan to modify.  This
 *  information percolates down to FindBuffer, allowing FindBuffer to avoid
 *  filling a buffer with disk data if the entire buffer contents are going
 *  to be modified anyway. -JTP
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      pos - position within DSTREAM to write to
 *      pvData - pointer to data to write (or NULL to write zeros)
 *      len - length of data to write
 *      plenWritten - pointer to DWORD for length written (optional)
 *      fCommit - TRUE to commit buffers as they are written
 *
 *  EXIT
 *      0 if successful, error code if not (eg, disk full, or error reading
 *      the disk to fill the non-modified portion of a buffer)
 *
 *  NOTES
 *      If fCommit is FALSE, note that the last buffer modified will still
 *      be held (ie, the stream's current buffer).
 */

#define MAX_UNPACK_RETRIES 20

DWORD WriteStreamData(PDSTREAM pstm, DWORD pos, PCVOID pvData, DWORD len, PDWORD plenWritten, BOOL fCommit)
{
    DWORD dwError, dwUnpackError;
    PBYTE pbStart, pbEnd;
    DWORD dwRetries;
 
    dwUnpackError = ERROR_SUCCESS;
    dwRetries = 0;
    ASSERT(OWNCRITICALSECTION(&pstm->s_cs));

    if (pstm->s_pvol->v_flags & VOLF_READONLY)
        return ERROR_WRITE_PROTECT;

    dwError = ERROR_SUCCESS;

    // A write of 0 bytes always succeeds, and is not supposed to do
    // anything other than cause the "last write" timestamp to eventually
    // get updated.

    if (len == 0) {
        pstm->s_flags |= STF_DIRTY_DATA;
        pstm->s_flags &= ~STF_WRITE_TIME;
        goto exit;
    }

#ifdef TFAT
     // If CloneStream fails (most likely because no room to copy), then fail the write
    if (pstm->s_pvol->v_fTfat && (pstm->s_pvol->v_flFATFS & FATFS_TRANS_DATA)) {
        dwError = CloneStream(pstm, pos, len );
        if (dwError != ERROR_SUCCESS)
            goto exit;
    }
#endif

    do {
        RewindStream(pstm, pos);

        do {
            // Locate the data run which contains the bytes requested.

            while (pos < pstm->s_run.r_end) {
                DWORD cb, blk, cblk;

                dwUnpackError = ERROR_SUCCESS;
                
                if (pvData && LockBlocks(pstm, pos, len, &blk, &cblk, TRUE)) {

                    __try {                        
                        dwError = WriteVolume(pstm->s_pvol, blk, cblk, (PVOID)pvData);
                        if (!dwError) {
                            cb = cblk << BLOCK_LOG2;
                            (PBYTE)pvData += cb;
                            if (plenWritten)
                                *plenWritten += cb;
                        }
                        else
                            DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!WriteStreamData: WriteVolume(block %d, %d blocks) failed (%d)\n"), blk, cblk, dwError));
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) {
                        dwError = ERROR_INVALID_PARAMETER;
                    }

                    UnlockBlocks(blk, cblk, TRUE);
                    if (dwError)
                        goto exit;
                }
                else {

                    dwError = ReadStreamBuffer(pstm,
                                               pos,
                                               pos+len < pstm->s_size? len : pstm->s_run.r_end - pos,
                                               &pbStart,
                                               &pbEnd);

                    if (dwError) {
                        DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!WriteStreamData: ReadStreamBuffer(block %d) failed (%d)\n"), pstm->s_run.r_blk + ((pos - pstm->s_run.r_start) >> BLOCK_LOG2), dwError));
                        goto exit;
                    }

                    cb = pbEnd - pbStart;
                    if (cb > len)
                        cb = len;

                    dwError = ModifyStreamBuffer(pstm, pbStart, cb);
                    if (dwError)
                        goto exit;

                    if (pvData) {
                        __try {
                            memcpy(pbStart, pvData, cb);
                            (PBYTE)pvData += cb;
                            if (plenWritten)
                                *plenWritten += cb;
                        }
                        __except (EXCEPTION_EXECUTE_HANDLER) {
                            dwError = ERROR_INVALID_PARAMETER;
                            goto exit;
                        }
                    }
                    else
                        memset(pbStart, 0, cb);
                }

                len -= cb;
                pos += cb;

                // Once we get this far at least once through the loop,
                // we need to make sure the stream data is flagged as dirty.

                pstm->s_flags |= STF_DIRTY;
                pstm->s_flags &= ~STF_WRITE_TIME;

                // Furthermore, if we just wrote beyond the "official"
                // size of the stream, then update the "official" size

                if (pos > pstm->s_size)
                    pstm->s_size = pos;

                // This is where all successful WriteStreamData calls
                // end up (except when len is ZERO to begin with, of course).

                if (len == 0)
                    goto exit;
            }

            dwError = UnpackRun(pstm);
			if (dwError == ERROR_DEVICE_NOT_AVAILABLE) {
				goto exit;
			}
            else if ((dwError == ERROR_INVALID_DATA) || ((dwUnpackError ==  ERROR_HANDLE_EOF) && (dwError == ERROR_HANDLE_EOF))){
                if (dwRetries++ == MAX_UNPACK_RETRIES) {
                    dwError = ERROR_DISK_FULL;
                    goto exit;
                }    
             }    
            dwUnpackError = dwError;

        } while (dwUnpackError == ERROR_SUCCESS);

        // We've run out of cluster runs, and we still have len bytes
        // to write at pos.  Resize the stream to encompass that boundary
        // and try to continue.

    } while ((dwError = ResizeStream(pstm, pos + len, RESIZESTREAM_SHRINK|RESIZESTREAM_UPDATEFAT)) == ERROR_SUCCESS);

    DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!WriteStreamData failed (%d)\n"), dwError));

  exit:
    if (fCommit) {

        DWORD dw;

        



























        if (pstm->s_flags & STF_DIRTY_CLUS) {
            dw = CommitStream(pstm, FALSE);
            if (!dwError)
                dwError = dw;
        }

        dw = ReleaseStreamBuffer(pstm, FALSE);
        if (!dwError)
            dwError = dw;
    }

    return dwError;
}


/*  ResizeStream - truncate or extend a stream
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      cbNew - new size of stream
 *      dwResizeFlags - zero or more RESIZESTREAM flags;  RESIZESTREAM_SHRINK
 *      forces shrinking (instead of simply insuring that the stream is at least
 *      as large as cbNew), and RESIZESTREAM_UPDATEFAT requests that changes to 
 *      the FAT be written.
 *
 *  EXIT
 *      ERROR_SUCCESS (0) if successful, non-zero if not
 *
 *      Truncating streams should only fail due to "hard" error conditions
 *      (eg, write-protected media), whereas extending a stream can also fail
 *      due to "soft" error conditions (eg, disk is full, or non-cluster-mapped
 *      stream -- like the root directory -- is full).
 */

DWORD ResizeStream (PDSTREAM pstm, DWORD cbNew, DWORD dwResizeFlags)
{
    PVOLUME pvol;
    DWORD dwError = ERROR_SUCCESS;
    DWORD posEnd, clus, clusEnd, clusPrev;
    DWORD clusRun, clusRunEnd, cclusRun, cbRun;
    BOOL bFirstClusChanged = FALSE;

    ASSERT(OWNCRITICALSECTION(&pstm->s_cs));

    DEBUGMSG(ZONE_STREAMS,(DBGTEXT("FATFS!ResizeStream: changing size from 0x%x to 0x%x for '%.11hs'\n"), pstm->s_size, cbNew, pstm->s_achOEM));

    // Early exit if stream size already matches desired size.

    if (pstm->s_size == cbNew)
        return ERROR_SUCCESS;

    




    

    if (pstm->s_clusFirst < DATA_CLUSTER)
        return cbNew <= pstm->s_run.r_end? ERROR_SUCCESS : ERROR_DISK_FULL;

    pvol = pstm->s_pvol;

    if (pvol->v_flags & VOLF_READONLY)
        return ERROR_WRITE_PROTECT;

    dwError = PositionStream(pstm, cbNew, &clusEnd);
    if (dwError) {
        ASSERT (0);  // Something wrong with cluster chain or FAT
        return dwError;
    }

    // If clusEnd is UNKNOWN_CLUSTER, then the new size was past EOF, so
    // we need to add clusters until we have enough to reach the specified size.

    if (clusEnd == UNKNOWN_CLUSTER) {

        // pstm->s_run.r_clusThis should contain the first cluster of the
        // last run in a file's cluster chain.  pstm->s_run.r_end is the
        // position immediately past the last cluster (which we store in posEnd).

        posEnd = pstm->s_run.r_end;

        // Assert that the reason we're here is that the stream is not
        // large enough.

        ASSERT(posEnd < cbNew);

        // The RUN only remembers a starting cluster (r_clusThis) and a
        // length (pstm->s_run.r_end - pstm->s_run.r_start), which we must
        // convert into an ending cluster (clusEnd).  If clusEnd is
        // UNKNOWN_CLUSTER, then the stream currently contains no clusters!

        clusEnd = pstm->s_run.r_clusThis;

        if (clusEnd != UNKNOWN_CLUSTER) {
            ASSERT(pstm->s_run.r_clusThis != 0);
            ASSERT(pstm->s_run.r_end > pstm->s_run.r_start);

            clusEnd = pstm->s_run.r_clusThis +
                      ((pstm->s_run.r_end - pstm->s_run.r_start-1) >>
                       (pvol->v_log2cblkClus + BLOCK_LOG2));
        }

        LockFAT(pvol);

        cclusRun = cbRun = 0;
        clusRunEnd = UNKNOWN_CLUSTER;
        clusPrev = clusRun = NO_CLUSTER;

        // If we have the number of free clusters cached, then let's calculate
        // the minimum number of clusters we'll need, and if there are not enough
        // free clusters to satisfy the request, then force an error.

        if (pvol->v_cclusFree != UNKNOWN_CLUSTER) {
            if (pvol->v_cclusFree == 0 || pvol->v_cclusFree < (cbNew - posEnd + pvol->v_cbClus - 1) / pvol->v_cbClus)
                dwError = ERROR_DISK_FULL;
        }

        do {
            if (dwError) {
                DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!ResizeStream: ran out of free clusters\n")));
                clus = UNKNOWN_CLUSTER;
                break;
            }

            dwError = NewCluster(pvol, clusEnd, &clus);
            if (dwError == ERROR_SUCCESS && clus == UNKNOWN_CLUSTER)
                dwError = ERROR_DISK_FULL;
            if (dwError != ERROR_SUCCESS) {
                DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!ResizeStream: cannot allocate new cluster (%d)\n"), dwError));
                break;          // can't allocate any more clusters
            }

            DEBUGMSG(ZONE_STREAMS,(DBGTEXT("FATFS!ResizeStream: new cluster for %.11hs returned %d'\n"), pstm->s_achOEM, clus));        

            // Link clus onto the end of the stream's cluster chain.  If
            // the previous end was UNKNOWN_CLUSTER, then record clus in the
            // stream's clusFirst field;  otherwise, link clus onto clusEnd.

            if (clusEnd == UNKNOWN_CLUSTER) {
                pstm->s_clusFirst = clus;
                pstm->s_flags |= STF_DIRTY | STF_DIRTY_CLUS;
                bFirstClusChanged = TRUE;
            }
            else {

                // If a PACK fails, we're having trouble writing to the FAT

                if (PACK(pvol, clusEnd, clus, NULL) != ERROR_SUCCESS) {
                    DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!ResizeStream: trouble growing new FAT chain\n")));
                    clus = UNKNOWN_CLUSTER;
                    break;
                }
            }

            // posEnd advances, and clus becomes the new clusEnd

            posEnd += pvol->v_cbClus;

            // Remember the first new cluster we allocate, and keep
            // track of the extent to which the new cluster(s) form a run.

            if (clusRunEnd == UNKNOWN_CLUSTER) {

                if (clusRun == NO_CLUSTER)
                    clusRun = clus;

                if (clusRun + cclusRun == clus) {
                    cclusRun++;
                    cbRun += pvol->v_cbClus;
                }
                else
                    clusRunEnd = clus;          // end of the RUN
            }

            if (clusEnd != UNKNOWN_CLUSTER)
                clusPrev = clusEnd;

            clusEnd = clus;

        } while (posEnd < cbNew);

        // If the stream has any new clusters at all...

#ifdef TFAT
        if (!dwError && clusEnd != UNKNOWN_CLUSTER) {
#else
        if (clusEnd != UNKNOWN_CLUSTER) {
#endif

            // If a PACK fails, we're having trouble writing to the FAT

            if (clus != UNKNOWN_CLUSTER) {
                if (PACK(pvol, clusEnd, EOF_CLUSTER, NULL) != ERROR_SUCCESS) {
                    DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!ResizeStream: trouble terminating new FAT chain\n")));
                    clus = UNKNOWN_CLUSTER;
                }
            }

            // If we couldn't allocate all the clusters we needed,
            // then (try to) resize the stream to its original size.

            if (clus == UNKNOWN_CLUSTER) {
                DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!ResizeStream: shrinking FAT chain back down...\n")));
                ResizeStream(pstm, pstm->s_run.r_end, dwResizeFlags | RESIZESTREAM_SHRINK);
                dwError = ERROR_DISK_FULL;
            }
            else {

                // It's important that we record the cluster-granular
                // size for directories, because when CreateName grows a
                // directory, it uses this size to calculate how much data
                // has to be zeroed.  For all other streams, it's important
                // to record only the exact number of bytes requested.

                if (!(pstm->s_attr & ATTR_DIRECTORY))
                    posEnd = cbNew;

                // If there's no net change in size, don't dirty the stream.

                ASSERT(pstm->s_size != posEnd); 

                if (pstm->s_size != posEnd) {
                    pstm->s_size = posEnd;
                    pstm->s_flags |= STF_DIRTY;
                }

                // We need to update the stream's RUN info;  the easiest
                // way to do this is call RewindStream, but that can be quite
                // expensive later, in terms of unnecessary calls to UnpackRun.

                pstm->s_run.r_start = pstm->s_run.r_end;
                pstm->s_run.r_end = pstm->s_run.r_end + cbRun;
                pstm->s_run.r_blk = CLUSTERTOBLOCK(pvol, clusRun);
                pstm->s_run.r_clusPrev = pstm->s_run.r_clusThis;
                if (pstm->s_run.r_clusPrev == UNKNOWN_CLUSTER)
                    pstm->s_run.r_clusPrev = NO_CLUSTER;
                pstm->s_run.r_clusThis = clusRun;
                pstm->s_run.r_clusNext = clusRunEnd;
            }
        }

#ifdef TFAT
        // need to update the parent dir
        if (pvol->v_fTfat && bFirstClusChanged)
            UpdateDirEntryCluster(pstm);
#endif

        // Commit and release FAT buffers now

        if (dwResizeFlags & RESIZESTREAM_UPDATEFAT)
            WriteAndReleaseStreamBuffers(pvol->v_pstmFAT);

        UnlockFAT(pvol);

        DEBUGMSG(ZONE_STREAMS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!ResizeStream(GROW) returned %d for '%.11hs'\n"), dwError, pstm->s_achOEM));
        return dwError;
    }

    // If shrinking was not enabled, then we're done

    if (!(dwResizeFlags & RESIZESTREAM_SHRINK))
        return ERROR_SUCCESS;

    // We enter the FAT's critical section even though we're just freeing
    // clusters, because in the process, we could alter the FAT stream's
    // current buffer, and that could mess up someone else accessing the FAT.

    LockFAT(pvol);

    // If clusEnd is NO_CLUSTER, then all the file's clusters can be freed;
    // set pstm->s_clusFirst to UNKNOWN_CLUSTER, which will get propagated
    // as ZERO to the DIRENTRY when the stream is committed (CommitStream will
    // eventually do that, because we also set STF_DIRTY, below).

    if (clusEnd == NO_CLUSTER) {

        // clusEnd becomes the first cluster to free.

        



        if (pstm->s_clusFirst != UNKNOWN_CLUSTER)
            pstm->s_flags |= STF_DIRTY_CLUS;

        clusEnd = pstm->s_clusFirst;
        pstm->s_clusFirst = UNKNOWN_CLUSTER;
        bFirstClusChanged = TRUE;
    }
    else
        dwError = PACK(pvol, clusEnd, EOF_CLUSTER, &clusEnd);

    // Now free all the remaining clusters in the chain.

#ifdef TFAT    
    // TFAT - don't delete the cluster, freeze it instead
    if (pstm->s_pvol->v_fTfat) {
        DWORD clusFirst = NO_CLUSTER, clusNext = clusEnd;

        // Get the last cluster in the chain
        while (!dwError && clusNext >= DATA_CLUSTER && !ISEOF(pvol, clusNext))
        {
            if (NO_CLUSTER == clusFirst)
                clusFirst = clusEnd;
            
            clusEnd = clusNext;
            
            // get next cluster
            dwError = UNPACK( pvol, clusNext, &clusNext );
        }

        if (NO_CLUSTER != clusFirst)
            FreezeClusters (pvol, clusFirst, clusEnd);
        
        // need to update the parent dir
        if (bFirstClusChanged)
            UpdateDirEntryCluster(pstm);
    }
    else 

#endif
    {
        while (!dwError && clusEnd >= DATA_CLUSTER && !ISEOF(pvol, clusEnd)) {
            FreeClustersOnDisk(pvol, clusEnd, 1);
            dwError = PACK(pvol, clusEnd, FREE_CLUSTER, &clusEnd);
        }

    }

    // Commit and release FAT buffers now.

    if (dwResizeFlags & RESIZESTREAM_UPDATEFAT)
        WriteAndReleaseStreamBuffers(pvol->v_pstmFAT);

    UnlockFAT(pvol);

    pstm->s_size = cbNew;
    pstm->s_flags |= STF_DIRTY;

    // We need to update the stream's RUN info, too;  the easiest
    // way to do this is call RewindStream, but since cbNew should still
    // be within the current run, we can save work by simply shrinking the run.

    ASSERT(cbNew >= pstm->s_run.r_start && cbNew <= pstm->s_run.r_end);

    // r_start and r_clusPrev are still valid, but r_end and r_clusNext
    // need to be updated, and r_blk and r_clusThis *may* need to be updated
    // if the current run was completely eliminated.  r_clusNext simply
    // becomes UNKNOWN_CLUSTER, since we just truncated the file, and r_end
    // is the number of bytes from r_start to cbNew, converted to whole clusters.

    pstm->s_run.r_clusNext = UNKNOWN_CLUSTER;
    pstm->s_run.r_end = (cbNew + pvol->v_cbClus-1) & ~(pvol->v_cbClus-1);

    // Now, if the current run is empty, we must also set r_clusThis to
    // UNKNOWN_CLUSTER (and although no one ever pays attention to r_blk when
    // r_clusThis is invalid, we will also set r_blk to INVALID_BLOCK in DEBUG).

    if (pstm->s_run.r_start == pstm->s_run.r_end) {
        pstm->s_run.r_clusThis = UNKNOWN_CLUSTER;
        DEBUGONLY(pstm->s_run.r_blk = INVALID_BLOCK);
    }

    DEBUGMSG(ZONE_STREAMS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!ResizeStream(SHRINK) returned %d for '%.11hs'\n"), dwError, pstm->s_achOEM));
    return dwError;
}


/*  CheckStreamHandles - Check for stream with open handles
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      psid - pointer to SID, or NULL to check all streams for open handles
 *
 *  EXIT
 *      TRUE if stream exists and has open handles, FALSE if not
 *
 *  NOTES
 *      This check excludes VOLUME-based handles, because it excludes VOLUME-
 *      based streams (ie, the FAT and root directory).
 */

BOOL CheckStreamHandles(PVOLUME pvol, PDSID psid)
{
    PDSTREAM pstm;
    BOOL fOpen = FALSE;

    EnterCriticalSection(&pvol->v_csStms);

    for (pstm = pvol->v_dlOpenStreams.pstmNext; pstm != (PDSTREAM)&pvol->v_dlOpenStreams; pstm = pstm->s_dlOpenStreams.pstmNext) {

        if (pstm->s_flags & STF_VOLUME)
            continue;

        if (psid == NULL || pstm->s_sid.sid_clusDir == psid->sid_clusDir && pstm->s_sid.sid_ordDir == psid->sid_ordDir) {
            fOpen = (pstm->s_dlOpenHandles.pfhNext != (PFHANDLE)&pstm->s_dlOpenHandles);
            if (psid || fOpen)
                break;
        }
    }
    LeaveCriticalSection(&pvol->v_csStms);
    return fOpen;
}


/*  UpdateSourceStream - Updates source stream after a MoveFile
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      psidSrc - pointer to SID of the source stream
 *      pdiDst - pointer to DIRINFO of destination file
 *      pstmDstParent - pointer to stream of parent directory of destination file
 *
 *  EXIT
 *      None
 *
 *  NOTES
 *      This is necessary in case there are open file handles that point to the
 *      source stream.
 */

VOID UpdateSourceStream (PVOLUME pvol, PDSID psidSrc, PDIRINFO pdiDst, PDSTREAM pstmDstParent)
{
    // Search through any open handles that carry the source stream
    // and update the stream to point to the destination
    PDSTREAM pstmSrc;

    EnterCriticalSection(&pvol->v_csStms);


    // Search for a stream that matches stream ID of source
    for (pstmSrc = pvol->v_dlOpenStreams.pstmNext; pstmSrc != (PDSTREAM)&pvol->v_dlOpenStreams;
       pstmSrc = pstmSrc->s_dlOpenStreams.pstmNext) {
        
        if (pstmSrc->s_sid.sid_clusDir == psidSrc->sid_clusDir && pstmSrc->s_sid.sid_ordDir == psidSrc->sid_ordDir) 
            break;
    }

    pstmSrc->s_refs++;
    LeaveCriticalSection(&pvol->v_csStms);

    // If we found a match, update stream's info so it refers to the destination file now
    if (pstmSrc != (PDSTREAM)&pvol->v_dlOpenStreams) {
        EnterCriticalSection (&pstmSrc->s_cs);

        memcpy(pstmSrc->s_achOEM, pdiDst->di_pde->de_name, sizeof(pdiDst->di_pde->de_name));
        pstmSrc->s_sid = pdiDst->di_sid;
        pstmSrc->s_sidParent = pstmDstParent->s_sid;
        pstmSrc->s_cwPath = pdiDst->di_cwName + pstmDstParent->s_cwPath + 1;

        if (pstmDstParent->s_pbufCur) {
            pstmSrc->s_blkDir = pstmDstParent->s_pbufCur->b_blk;
            pstmSrc->s_offDir = (PBYTE)pdiDst->di_pde - pstmDstParent->s_pbufCur->b_pdata;
        }

        LeaveCriticalSection (&pstmSrc->s_cs);
    }

    pstmSrc->s_refs--;

}


/*  StreamOpenedForExclAccess - Check if file is already open for exclusive access
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *
 *  EXIT
 *      TRUE if file is already open for exclusive access, FALSE if not
 *
 *  NOTES
 *      This check excludes VOLUME-based handles, because it excludes VOLUME-
 *      based streams (ie, the FAT and root directory).
 */

BOOL StreamOpenedForExclAccess(PVOLUME pvol, PDSID psid)
{
    PDSTREAM pstm;
    PFHANDLE pfh, pfhEnd;
    BOOL fOpenedForExclAccess = FALSE;

    ASSERT(psid);

    EnterCriticalSection(&pvol->v_csStms);

    for (pstm = pvol->v_dlOpenStreams.pstmNext; 
           !fOpenedForExclAccess && pstm != (PDSTREAM)&pvol->v_dlOpenStreams;
           pstm = pstm->s_dlOpenStreams.pstmNext) {

        if (pstm->s_sid.sid_clusDir == psid->sid_clusDir && pstm->s_sid.sid_ordDir == psid->sid_ordDir) {

            // Walk the handle list. Return TRUE if any are opened for exclusive access.
            for(pfh = pstm->s_dlOpenHandles.pfhNext, pfhEnd = (PFHANDLE)&pstm->s_dlOpenHandles;
                  pfh != pfhEnd; pfh = pfh->fh_dlOpenHandles.pfhNext) {
                
                if ( (!(pfh->fh_mode & FH_MODE_SHARE_READ) || (pfh->fh_mode & FH_MODE_WRITE)) && 
                       !(pfh->fh_mode & FH_MODE_SHARE_WRITE)) {
                    fOpenedForExclAccess = TRUE;
                    break;
                }
            }
        }
    }
    LeaveCriticalSection(&pvol->v_csStms);
    return fOpenedForExclAccess;
}

/*  CheckStreamSharing - Check requested mode against stream
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      mode - requested mode
 *
 *  EXIT
 *      TRUE if all handles for stream permit the specified mode, FALSE if not
 */

BOOL CheckStreamSharing(PDSTREAM pstm, int mode)
{
    BYTE bRequired = 0;
    PFHANDLE pfh, pfhEnd;

    ASSERT(pstm);
    ASSERT(OWNCRITICALSECTION(&pstm->s_cs));

    if (mode & FH_MODE_READ)
        bRequired |= FH_MODE_SHARE_READ;

    if (mode & FH_MODE_WRITE)
        bRequired |= FH_MODE_SHARE_WRITE;

    pfh = pstm->s_dlOpenHandles.pfhNext;
    pfhEnd = (PFHANDLE)&pstm->s_dlOpenHandles;

    while (pfh != pfhEnd) {
        if ((pfh->fh_mode & bRequired) != bRequired)
            return FALSE;
        pfh = pfh->fh_dlOpenHandles.pfhNext;
    }
    return TRUE;
}
