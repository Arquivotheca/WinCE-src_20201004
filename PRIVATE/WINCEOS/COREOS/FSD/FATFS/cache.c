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

    cache.c

Abstract:

    This file contains FAT file system path/name cache functions.

Revision History:

--*/

#include "fatfs.h"


#ifdef PATH_CACHING

/*  PathCacheCreate - Create a new entry in a volume's path cache
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsPath - pointer to path
 *      len - length of path (ie, portion to cache)
 *      pstm - pointer to DSTREAM corresponding to path (other than root)
 *
 *  EXIT
 *      None.  If the entry can be added to the path cache, great;
 *      if not, oh well.
 *
 *  NOTES
 *      Pre-processed paths should include NEITHER a leading [back]slash
 *      NOR a trailing one (ie, only the characters inbetween).  Furthermore,
 *      callers should never try to cache the root;  it's pointless because
 *      the root stream is already persistent.
 *
 *      If the caller has NOT pre-processed the path according to the above
 *      rules, then he should pass len == 0 and we will take care of the
 *      rest.  In that case though, the path must be null-terminated.
 *
 *      Also, when streams are cached, their critical section is usually
 *      being held by the caller at the time.  So, after we take the *cache*
 *      critical section, it is important that we not attempt to take any
 *      *other* stream's critical section, since that could cause a deadlock.
 *
 *      This is why PathCacheDestroy has the option of returning the stream
 *      belonging to a destroyed cache entry;  if PathCacheDestroy was allowed
 *      to close its reference to the stream on the spot, it would have to
 *      briefly relinquish the *cache* critical section first, and we'd rather
 *      not do that in this particular function.
 */

void PathCacheCreate(PVOLUME pvol, PCWSTR pwsPath, int len, PDSTREAM pstm)
{
    PCACHE pcch;
    PDSTREAM pstmFree = NULL;    
    WCHAR wsNewPath[MAX_PATH];

    ASSERT(OWNCRITICALSECTION(&pstm->s_cs));

    if (pvol->v_flags & VOLF_UNMOUNTED)
        return;

    // Replace any forward slashes with backslashes.
    PathCanonicalize (pwsPath, wcslen (pwsPath), wsNewPath, MAX_PATH - 1);
    pwsPath = wsNewPath;

    EnterCriticalSection(&pvol->v_csStms);
    EnterCriticalSection(&pvol->v_csCaches);

    ASSERT(pvol->v_cCaches <= pvol->v_cMaxCaches && pstm != pvol->v_pstmRoot);

#ifdef TFAT
        if (pvol->v_flags & VOLF_TFAT_REDIR_ROOT)
        {
            // If we are redirecting the root dir, then remove the hidden dir from the path, only if the path is
            // not the hidden dir itself.  
            if (!wcsncmp (pwsPath, HIDDEN_TFAT_ROOT_DIR, HIDDEN_TFAT_ROOT_DIR_LEN) && 
                (len > (HIDDEN_TFAT_ROOT_DIR_LEN+1))) 
            {                
                pwsPath += (HIDDEN_TFAT_ROOT_DIR_LEN+1);
                len -= (HIDDEN_TFAT_ROOT_DIR_LEN+1);
            }       
        }
#endif


    // Cache this stream, if it isn't already cached

    if (!PathCacheFindStream(pvol, pstm)) {

        // If len <= 0, then the caller has an unprocessed null-terminated
        // path which we must calculate the length of;  in particular,
        // if len < 0, then -len is the number of elements to remove from the
        // end of the path being cached.

        if (len <= 0) {
            len = PathCacheLength(&pwsPath, -len);
            if (len <= 0)
                goto exit;
            if (pstm->s_cwPath == 0)
                pstm->s_cwPath = len + 1;
        }

#ifdef DEMAND_PAGING_DEADLOCKS
        // If the stream we've opened can currently be demand-paged,
        // then we MUST temporarily release the stream's CS around these
        // memory manager calls.

        if (pstm->s_flags & STF_DEMANDPAGED)
            LeaveCriticalSection(&pstm->s_cs);
#endif
        // If we've already got the (hard-coded) maximum number of cache
        // entries, then destroy the oldest one.  We don't free the stream
        // right away -- we wait until we've released the cache critical
        // section first, to avoid deadlocks.

        if (pvol->v_cCaches == pvol->v_cMaxCaches)
            pstmFree = PathCacheDestroy(pvol, pvol->v_dlCaches.pcchPrev, FALSE);

        // Now allocate a new entry.

        pcch = (PCACHE)LocalAlloc(LPTR, sizeof(CACHE)+(len+1-ARRAYSIZE(pcch->c_awcPath))*sizeof(WCHAR));

#ifdef DEMAND_PAGING_DEADLOCKS
        if (pstm->s_flags & STF_DEMANDPAGED)
            EnterCriticalSection(&pstm->s_cs);
#endif
        if (pcch) {

            DEBUGALLOC(pcch->c_cbAlloc = sizeof(CACHE)+(len+1-ARRAYSIZE(pcch->c_awcPath))*sizeof(WCHAR));

            pstm->s_refs++;
            pcch->c_pstm = pstm;
            pcch->c_flags = CACHE_PATH;

            pvol->v_cCaches++;
            AddItem((PDLINK)&pvol->v_dlCaches, (PDLINK)&pcch->c_dlink);

            __try {
                memcpy(pcch->c_awcPath, pwsPath, len*sizeof(WCHAR));
                pcch->c_cwPath = len;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                ;
            }
        }
    }
  exit:
    LeaveCriticalSection(&pvol->v_csCaches);
    LeaveCriticalSection(&pvol->v_csStms);

    if (pstmFree) {

// WINSE 25001, fix deadlock
// #ifdef DEMAND_PAGING_DEADLOCKS
        // if (pstm->s_flags & STF_DEMANDPAGED)
            LeaveCriticalSection(&pstm->s_cs);
// #endif

        EnterCriticalSection(&pstmFree->s_cs);
        CloseStream(pstmFree);

// #ifdef DEMAND_PAGING_DEADLOCKS
//         if (pstm->s_flags & STF_DEMANDPAGED)
            EnterCriticalSection(&pstm->s_cs);
// #endif
    }
}


/*  PathCacheSearch - Search for a path in a volume's path cache
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      ppwsPath - address of pointer to path
 *
 *  EXIT
 *      Pointer to DSTREAM if found, NULL if not
 *
 *  NOTES
 *      This will advance the caller's pwsPath up to the final
 *      [back]slash in the specified path.  Also, we don't bother with
 *      exception handling around the first bit of code because the only
 *      caller (OpenPath) already has a handler.
 */

PDSTREAM PathCacheSearch(PVOLUME pvol, PCWSTR *ppwsPath)
{
    int len;
    PCACHE pcch, pcchEnd;
    WCHAR wsNewPath[MAX_PATH];
    PCWSTR pwsOldPath = *ppwsPath;
    PWSTR pwsNewPath = wsNewPath;
    PDSTREAM pstmreturn = NULL;

    // Determine the path length we need to compare.  To do so,
    // we must find the final [back]slash.

    len = wcslen (pwsOldPath);
    if (len <= 1)
        return NULL;

    // Replace any forward slashes with backslashes.
    PathCanonicalize (pwsOldPath, len, pwsNewPath, MAX_PATH - 1);

    len--;

    // Ignore a trailing slash
    if (pwsNewPath[len] == L'\\' || pwsNewPath[len] == L'/') {
        len--;
    }

    // Find the nearest slash from the current pointer
    while (len && pwsNewPath[len] != L'\\' && pwsNewPath[len] != L'/') {
        len--;
    }

    // We don't cache the root (not much point), so get out if that's
    // the case...

    if (len == 0)
        return NULL;

#ifdef TFAT
        if (pvol->v_flags & VOLF_TFAT_REDIR_ROOT)
        {
            // If we are redirecting the root dir, then remove the hidden dir from the path, only if the path is
            // not the hidden dir itself.  
            if (!wcsncmp (pwsNewPath, HIDDEN_TFAT_ROOT_DIR, HIDDEN_TFAT_ROOT_DIR_LEN) && 
                (len > (HIDDEN_TFAT_ROOT_DIR_LEN+1))) 
            {                
                pwsOldPath += (HIDDEN_TFAT_ROOT_DIR_LEN+1);
                pwsNewPath += (HIDDEN_TFAT_ROOT_DIR_LEN+1);
                len -= (HIDDEN_TFAT_ROOT_DIR_LEN+1);
            }       
        }
#endif

    EnterCriticalSection(&pvol->v_csStms);
    EnterCriticalSection(&pvol->v_csCaches);

    __try {
        pcch = pvol->v_dlCaches.pcchNext;
        pcchEnd = (PCACHE)&pvol->v_dlCaches;

        while (pcch != pcchEnd) {

            if (len == pcch->c_cwPath && _wcsnicmp(pcch->c_awcPath, pwsNewPath, len) == 0) {

                PDSTREAM pstm = pcch->c_pstm;

                // Since this entry is a match, bump it up in the list

                RemoveItem((PDLINK)&pcch->c_dlink);
                AddItem((PDLINK)&pvol->v_dlCaches, (PDLINK)&pcch->c_dlink);

                *ppwsPath = pwsOldPath + len + 1;

                pstm->s_refs++;
                pstmreturn = pstm;
                break;
            }
            pcch = pcch->c_dlink.pcchNext;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ;
    }

    LeaveCriticalSection(&pvol->v_csCaches);
    LeaveCriticalSection(&pvol->v_csStms);

    if (pstmreturn) {
        EnterCriticalSection(&pstmreturn->s_cs);
    }
    return pstmreturn;
}


/*  PathCacheInvalidate - Invalidate all cached paths containing this path
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsPath - pointer to path to invalidate
 *
 *  EXIT
 *      TRUE if any cache entries were invalidated, FALSE if not
 */

BOOL PathCacheInvalidate(PVOLUME pvol, PCWSTR pwsPath)
{
    int len;
    BOOL fInvalidate;
    WCHAR wsNewPath[MAX_PATH];

    // Replace any forward slashes with backslashes.
    PathCanonicalize (pwsPath, wcslen (pwsPath), wsNewPath, MAX_PATH - 1);
    pwsPath = wsNewPath;

    len = PathCacheLength(&pwsPath, 0);
    if (len == 0)
        return FALSE;

    fInvalidate = FALSE;

    EnterCriticalSection(&pvol->v_csCaches);

    __try {
        PCACHE pcch, pcchEnd;

        pcch = pvol->v_dlCaches.pcchNext;
        pcchEnd = (PCACHE)&pvol->v_dlCaches;

        while (pcch != pcchEnd) {

            if (len <= pcch->c_cwPath && _wcsnicmp(pcch->c_awcPath, pwsPath, len) == 0 &&
                (pcch->c_awcPath[len] == 0 || pcch->c_awcPath[len] == TEXTW('\\') || pcch->c_awcPath[len] == TEXTW('/'))) {
                PathCacheDestroy(pvol, pcch, TRUE);
                pcch = pvol->v_dlCaches.pcchNext;
                fInvalidate = TRUE;
                continue;
            }
            pcch = pcch->c_dlink.pcchNext;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ;
    }

    LeaveCriticalSection(&pvol->v_csCaches);

    return fInvalidate;
}


/*  PathCacheLength - Calculate the length of a path
 *
 *  ENTRY
 *      ppwsPath - pointer to pointer to path
 *      celRemove - count of elements to remove from end
 *
 *  EXIT
 *      length of path (leading and terminating slashes ignored);
 *      *ppwsPath is updated as well
 */

int PathCacheLength(PCWSTR *ppwsPath, int celRemove)
{
    int len;
    PCWSTR pwsEnd, pwsPath = *ppwsPath;

    // Ignore leading slash

    __try {
        if (*pwsPath == TEXTW('\\') || *pwsPath == TEXTW('/'))
            ++pwsPath;

        len = wcslen(pwsPath);
        if (len > 0) {

            pwsEnd = pwsPath + len - 1;

            // Ignore trailing slash

            if (*pwsEnd == TEXTW('\\') || *pwsEnd == TEXTW('/')) {
                pwsEnd--;
                len--;
            }

            while (celRemove > 0 && pwsEnd > pwsPath) {
                if (*pwsEnd == TEXTW('\\') || *pwsEnd == TEXTW('/'))
                    celRemove--;
                pwsEnd--;
                len--;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        len = 0;
    }
    *ppwsPath = pwsPath;
    return len;
}


/*  PathCacheFindStream - Search for specific stream in path cache
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pstm - pointer to DSTREAM
 *
 *  EXIT
 *      Pointer to CACHE entry, NULL if none.  This function is really for
 *      internal use only.
 */

PCACHE PathCacheFindStream(PVOLUME pvol, PDSTREAM pstm)
{
    PCACHE pcch, pcchEnd;

    ASSERT(OWNCRITICALSECTION(&pstm->s_cs));
    ASSERT(OWNCRITICALSECTION(&pvol->v_csCaches));

    pcch = pvol->v_dlCaches.pcchNext;
    pcchEnd = (PCACHE)&pvol->v_dlCaches;

    while (pcch != pcchEnd) {
        if (pcch->c_pstm == pstm)
            return pcch;
        pcch = pcch->c_dlink.pcchNext;
    }
    return NULL;
}


/*  PathCacheDestroy - Free a specific path cache entry
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pcch - pointer to CACHE entry
 *      fClose - TRUE to close associated stream, FALSE to return
 *
 *  EXIT
 *      None.  This function is really for internal use only.
 */

PDSTREAM PathCacheDestroy(PVOLUME pvol, PCACHE pcch, BOOL fClose)
{
    PDSTREAM pstm = pcch->c_pstm;

    ASSERT(OWNCRITICALSECTION(&pvol->v_csCaches));

    pvol->v_cCaches--;
    RemoveItem((PDLINK)&pcch->c_dlink);

    DEBUGFREE(pcch->c_cbAlloc);
    VERIFYNULL(LocalFree((HLOCAL)pcch));

    if (fClose) {
        LeaveCriticalSection(&pvol->v_csCaches);
        EnterCriticalSection(&pstm->s_cs);
        CloseStream(pstm);
        EnterCriticalSection(&pvol->v_csCaches);
        return NULL;
    }
    return pstm;
}


/*  PathCacheDestroyAll - Free all path cache entries for a volume
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *
 *  EXIT
 *      None.  Calls PathCacheDestroy for each cache entry until the
 *      cache is empty.
 */

void PathCacheDestroyAll(PVOLUME pvol)
{
    PCACHE pcch, pcchEnd;

    EnterCriticalSection(&pvol->v_csCaches);

    pcchEnd = (PCACHE)&pvol->v_dlCaches;

    do {
        pcch = pvol->v_dlCaches.pcchNext;

        if (pcch == pcchEnd)
            break;

        PathCacheDestroy(pvol, pcch, TRUE);

    } while (TRUE);

    ASSERT(pvol->v_cCaches == 0);

    LeaveCriticalSection(&pvol->v_csCaches);
}


/*  PathCanonicalize - Replace any forward slashes with backslashes.
 *
 *  ENTRY
 *      Old path and new path, with lengths.
 *
 *  EXIT
 *      None.  
 */

void PathCanonicalize (PCWSTR pwsOldPath, DWORD dwOldPathLen, PWSTR pwsNewPath, DWORD dwNewPathLen)
{
    DWORD i;
    DWORD dwLength = min (dwOldPathLen, dwNewPathLen);

    for (i = 0; i < dwLength; i++) {

        if (pwsOldPath[i] == L'/') {
            pwsNewPath[i] = L'\\';
        } else {
            pwsNewPath[i] = pwsOldPath[i];
        }
    }

    pwsNewPath[dwLength] = 0;
}


#endif  // PATH_CACHING
