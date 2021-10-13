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

    find.c

Abstract:

    This file contains the FAT file system FindFirst/FindNext/FindClose
    routines.

Revision History:

--*/

#include "fatfs.h"


HANDLE FAT_FindFirstFileW(PVOLUME pvol, HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd)
{
    DWORD len;
    PCWSTR pwsName;
    PSHANDLE psh = NULL;
    PDSTREAM pstm = NULL;
    DWORD dwError = ERROR_SUCCESS;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_FindFirstFileW(%d chars: %s)\r\n"), wcslen(pwsFileSpec), pwsFileSpec));

    if (!FATEnter(NULL, LOGID_NONE))
        return INVALID_HANDLE_VALUE;

    if (pvol->v_flags & (VOLF_INVALID | VOLF_UNMOUNTED | VOLF_FROZEN | VOLF_WRITELOCKED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    __try {
        pstm = OpenPath(pvol, pwsFileSpec, &pwsName, &len, NAME_DIR, UNKNOWN_CLUSTER);
        if (!pstm) {
            DEBUGONLY(dwError = GetLastError());
            goto abort;
        }

        // Perform a wildcard simplification up front...

        if (len == 3 && wcscmp(pwsName, TEXTW("*.*")) == 0)
            len = 1;

        psh = (PSHANDLE)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(SHANDLE)+(len+1-ARRAYSIZE(psh->sh_awcPattern))*sizeof(WCHAR));
        if (!psh) {
            dwError = ERROR_OUTOFMEMORY;
            goto exit;
        }

        DEBUGALLOC(psh->sh_cbAlloc = sizeof(SHANDLE)+(len+1-ARRAYSIZE(psh->sh_awcPattern))*sizeof(WCHAR));

        AddItem((PDLINK)&pstm->s_dlOpenHandles, (PDLINK)&psh->sh_dlOpenHandles);
        psh->sh_pstm = pstm;
        psh->sh_flags = SHF_BYNAME | SHF_WILD;
        psh->sh_cwPattern = (WORD)len;
        memcpy(psh->sh_awcPattern, pwsName, len*sizeof(WCHAR));

        if (len == 0) {

            // The caller must be trying to find '\\'.  That's normally
            // a no-no, but in this case we'll do our best to pretend that
            // it worked, because our root directory is not a root directory
            // in the host file system;  it's treated like another folder.

            memset(pfd, 0, sizeof(*pfd));

            pfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
#ifdef UNDER_CE
            // NT does not have OID defined        
            pfd->dwOID = OIDFROMSID(pvol, NULL);
#endif
            wcscpy(pfd->cFileName, pwsFileSpec);

            // Prevent the caller from searching any further with this pattern

            psh->sh_pos = INVALID_POS;
        }
        else {
            dwError = FindNext(psh, NULL, pfd);
            if (dwError == ERROR_FILE_NOT_FOUND)
                dwError = ERROR_NO_MORE_FILES;            
            if (dwError)
                goto exit;
        }

        if (!(hFind = FSDMGR_CreateSearchHandle(pvol->v_hVol, hProc, (PSEARCH)psh))) {
            hFind = INVALID_HANDLE_VALUE;
            dwError = ERROR_OUTOFMEMORY;
            goto exit;
        }

        psh->sh_h = hFind;
        psh->sh_hProc = hProc;

        // Don't be disturbed by this LeaveCriticalSection call inside the
        // try/except block;  if an exception occurs, dwError will be set, and
        // so we'll call CloseStream below, which again takes care of leaving
        // (and freeing) the stream as appropriate.

        // We don't want to call CloseStream in the success path, because the
        // stream needs to be left open for as long as the search handle exists.

        LeaveCriticalSection(&pstm->s_cs);

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }

  exit:
    if (dwError) {
        if (psh) {
            RemoveItem((PDLINK)&psh->sh_dlOpenHandles);
            DEBUGFREE(psh->sh_cbAlloc);
            VERIFYTRUE(HeapFree(hHeap, 0, (HLOCAL)psh));
#ifdef DEBUG
            psh = NULL;
#endif
        }
        CloseStream(pstm);
        SetLastError(dwError);
    }

  abort:
    FATExit(pvol, LOGID_NONE);

    DEBUGMSGW(ZONE_APIS || ZONE_ERRORS && hFind == INVALID_HANDLE_VALUE,
                        (DBGTEXTW("FATFS!FAT_FindFirstFileW(0x%08x,%-.64s) returned 0x%x \"%s\" (%d)\r\n"),
                         psh, pwsFileSpec, hFind, hFind != INVALID_HANDLE_VALUE? pfd->cFileName : TEXTW(""), dwError));

    return hFind;
}


BOOL FAT_FindNextFileW(PSHANDLE psh, PWIN32_FIND_DATAW pfd)
{
    PDSTREAM pstm;
    DWORD dwError;

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_FindNextFile(0x%08x,%s)\r\n"), psh, psh->sh_awcPattern));

    if (!FATEnter(NULL, LOGID_NONE))
        return FALSE;

    pstm = psh->sh_pstm;
    ASSERT(pstm);

    __try {

        EnterCriticalSection(&pstm->s_cs);
        dwError = FindNext(psh, NULL, pfd);
        LeaveCriticalSection(&pstm->s_cs);

        if (dwError == ERROR_FILE_NOT_FOUND)
            dwError = ERROR_NO_MORE_FILES;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }

    if (dwError)
        SetLastError(dwError);

    FATExit(pstm->s_pvol, LOGID_NONE);

    DEBUGMSGW(ZONE_APIS /* || ZONE_ERRORS && dwError */,
                        (DBGTEXTW("FATFS!FAT_FindNextFile(0x%08x,%s) returned 0x%x \"%s\" (%d)\r\n"),
                         psh,
                         psh->sh_awcPattern,
                         dwError == ERROR_SUCCESS,
                         dwError == ERROR_SUCCESS? pfd->cFileName : TEXTW(""),
                         dwError));
    return dwError == ERROR_SUCCESS;
}


BOOL FAT_FindClose(PSHANDLE psh)
{
    PDSTREAM pstm;
    PVOLUME  pvol;

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_FindClose(0x%08x,%s)\r\n"), psh, psh->sh_awcPattern));

    if (!FATEnter(NULL, LOGID_NONE))
        return FALSE;

    pstm = psh->sh_pstm;
    ASSERT(pstm);

    pvol = pstm->s_pvol;
    ASSERT (pvol);

    EnterCriticalSection(&pstm->s_cs);
    RemoveItem((PDLINK)&psh->sh_dlOpenHandles);
    CloseStream(pstm);

    DEBUGFREE(psh->sh_cbAlloc);
    VERIFYTRUE(HeapFree(hHeap, 0, (HLOCAL)psh));

    FATExit(pvol, LOGID_NONE);

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_FindClose returned TRUE (0)\r\n")));

    return TRUE;
}


/*  OpenRoot - Open stream for the root directory on a volume
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *
 *  EXIT
 *      pointer to the root directory stream, NULL if none;  there may not
 *      be one if the volume hasn't been formatted yet (for example).
 */

PDSTREAM OpenRoot(PVOLUME pvol)
{
    PDSTREAM pstm = pvol->v_pstmRoot;

    if (pstm) {

        if (pstm->s_flags & STF_UNMOUNTED)
            return NULL;

        EnterCriticalSection(&pvol->v_csStms);

        pstm->s_refs++;
        LeaveCriticalSection(&pvol->v_csStms);

        EnterCriticalSection(&pstm->s_cs);
    }
    return pstm;
}


/*  CloseRoot - Close stream for the root directory on a volume
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *
 *  EXIT
 *      ERROR_SUCCESS (or other error code).  The only time an error
 *      would be reported however is if there were still dirty buffers
 *      associated with the root stream that could not be committed by
 *      this call to CloseStream -- but such an error would have normally
 *      already been reported by an *earlier* call to CloseStream.
 */

DWORD CloseRoot(PVOLUME pvol)
{
    return CloseStream(pvol->v_pstmRoot);
}


/*  OpenPath - Open stream for next-to-last element of a path
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsPath - pointer to path
 *      ppwsTail - address of pointer to last element (returned)
 *      plen - address of length of last element (returned)
 *      flName - zero or more NAME_* flags (eg, NAME_FILE, NAME_DIR)
 *      clusFail - cluster of stream not allowed in path
 *      (normally, this is UNKNOWN_CLUSTER, to perform unrestricted opens)
 *
 *  EXIT
 *      pointer to stream, NULL if error (call GetLastError for error code);
 *      rely on *ppwsTail and *plen only if there is no error.
 *
 *  NOTES
 *      There can be no "last element" if the specified path is the root
 *      directory;  in that case, *plen is set to ZERO.
 */

PDSTREAM OpenPath(PVOLUME pvol, PCWSTR pwsPath, PCWSTR *ppwsTail, int *plen, int flName, DWORD clusFail)
{
    int len = 0;
	int flTmp = 0;
    PCWSTR pws;
    LPTSTR pwsTempPath = NULL, pwsOrig = NULL;
    DWORD dwError = ERROR_SUCCESS;
    PDSTREAM pstm=NULL, pstmPrev = NULL ;

    ASSERT(pwsPath);

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!OpenPath (%s)\r\n"), pwsPath));

    __try {

        // Ignore leading slash
        if (*pwsPath == TEXTW('\\') || *pwsPath == TEXTW('/'))
            ++pwsPath;

        // Ignore empty paths
        if (*pwsPath == 0) {
            SetLastError(ERROR_ALREADY_EXISTS);
            goto exit;
        }

        pstm = OpenRoot(pvol);
        if (!pstm) {
            SetLastError(ERROR_ACCESS_DENIED);
            goto exit;
        }

        pwsTempPath = (LPTSTR)pwsPath;
        
#ifdef TFAT
        // Prepend the hidden directory to the path.  Ignore VOL:
        if (pvol->v_flags & VOLF_TFAT_REDIR_ROOT && _wcsicmp(pwsPath, TEXTW("VOL:")))
        {
            DWORD cbSize = (HIDDEN_TFAT_ROOT_DIR_LEN  + _tcslen(pwsPath) + 2) * sizeof(TCHAR);
            if (!(pwsTempPath = (LPTSTR) HeapAlloc (hHeap, 0 ,cbSize))) {
                goto exit;
            }
            _tcscpy (pwsTempPath, HIDDEN_TFAT_ROOT_DIR);
            _tcscat (pwsTempPath, TEXT("\\"));
            _tcscat (pwsTempPath, pwsPath);            
        }
#endif

        pwsOrig = pwsTempPath;

#ifdef PATH_CACHING
        // If clusFail is set to a specific cluster, we have walk
        // the path the hard way, making sure that no part of the path
        // references that cluster.  Otherwise, we can search the cache.

        if (clusFail == UNKNOWN_CLUSTER) {
            if (pstmPrev = PathCacheSearch(pvol, &pwsTempPath)) {
                CloseStream(pstm);
                pstm = pstmPrev;
            }
        }
#endif
        do {
            // pws points to current element

            pws = pwsTempPath;

            // Find end of current element

            while (*pwsTempPath && *pwsTempPath != TEXTW('\\') && *pwsTempPath != TEXTW('/'))
                ++pwsTempPath;

            // Determine size of current element

            if (len = pwsTempPath - pws) {

                if (len > MAX_LFN_LEN) {
                    dwError = ERROR_FILENAME_EXCED_RANGE;
                    break;
                }

                // If this is the last element, exit.  We know that the
                // current character is either a NULL or slash.  If NULL,
                // we're done;  if not NULL, then if the character after
                // the current slash is NULL *and* the caller is looking
                // for a directory, again we're done.

                if (*pwsTempPath == 0)
                    break;

                if (*(pwsTempPath+1) == 0 && (flName & NAME_DIR))
                    break;

                flTmp = NAME_DIR;
                pstm = OpenName(pstmPrev = pstm, pws, len, &flTmp);
                ASSERT(!pstm || !pstm->s_cwPath || pstm->s_cwPath > pstmPrev->s_cwPath + 1);

#ifdef DEMAND_PAGING_DEADLOCKS
                // If the stream we've opened can currently be demand-paged,
                // then we MUST temporarily release the stream's CS around the
                // memory manager calls that CloseStream could make (eg, LocalFree).

                if (pstm && (pstm->s_flags & STF_DEMANDPAGED))
                    LeaveCriticalSection(&pstm->s_cs);
#endif
                CloseStream(pstmPrev);

                if (!pstm)
                    break;

#ifdef DEMAND_PAGING_DEADLOCKS
                if (pstm->s_flags & STF_DEMANDPAGED)
                    EnterCriticalSection(&pstm->s_cs);
#endif

#ifdef PATH_CACHING
                





                PathCacheCreate(pvol, pwsOrig, pwsTempPath-pwsOrig, pstm);
#endif
                // If the starting cluster of the stream we just opened
                // matches clusFail, someone is trying to force a stream
                // to become its own parent.  Close the current stream
                // and fail the call.

                if (pstm->s_clusFirst == clusFail) {
                    dwError = ERROR_ACCESS_DENIED;
                    break;
                }
            }
        } while (*pwsTempPath++);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }

    // The only time the length of the final element is allowed to be zero
    // is when there is no previous stream (ie, the caller specified the root)

    if (len == 0 && pstmPrev != NULL)
        dwError = ERROR_PATH_NOT_FOUND;

    if (dwError) {
        SetLastError(dwError);
        if (pstm) {
            CloseStream(pstm);
            pstm = NULL;
        }
    }

    // ASSERT(!pstm || !pstm->s_cwPath || pstm->s_cwPath + (DWORD)len + 1 >= wcslen(pwsOrig) + (pwsOrig[0] != TEXTW('\\')) - (pws[len] == TEXTW('\\')));

    *ppwsTail = pwsPath + wcslen(pwsPath) - len;

    // Adjust for a trailing backslash or forwardslash in a dir path.
    if ((*pwsTempPath == L'\\' || *pwsTempPath == L'/') && *(pwsTempPath+1) == 0 && (flName & NAME_DIR))
        (*ppwsTail)--;

    *plen = len;

#ifdef TFAT
    if (pwsOrig != pwsPath) {
        HeapFree (hHeap, 0, pwsOrig);
    }
#endif        

exit:

    return pstm;
}


/*  OpenName - Open stream for last element of a path
 *
 *  ENTRY
 *      pstmDir - pointer to directory stream (or volume)
 *      pwsName - pointer to name
 *      cwName - length of name (0 if pstmDir is a volume instead)
 *      pflName - pointer to NAME_* flags (eg, NAME_FILE, NAME_DIR),
 *      along with initial file attributes (eg, ATTR_DIRECTORY) if NAME_CREATE
 *
 *  EXIT
 *      pointer to stream, NULL if error (call GetLastError for error code)
 */

PDSTREAM OpenName(PDSTREAM pstmDir, PCWSTR pwsName, int cwName, int *pflName)
{
    SHANDLE sh;
    DIRINFO di;
    WIN32_FIND_DATAW fd;
    PDSTREAM pstmTmp = NULL;
    PDSTREAM pstmRet = NULL;
    int flName = *pflName;
    DWORD dwError = ERROR_SUCCESS;

    ASSERT(pstmDir && cwName <= MAX_LFN_LEN);

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!OpenName (%s, %d)\r\n"), pwsName, cwName));

    if (cwName == 0) {

        pstmTmp = OpenPath((PVOLUME)pstmDir, pwsName, &pwsName, &cwName, *pflName, UNKNOWN_CLUSTER);
        if (pstmTmp == NULL)
            goto exit;

        // cwName is zero when the caller tries to open the root dir.
        // We allow the root to be FOUND in some cases, but never OPENED.

        if (cwName == 0) {
            dwError = ERROR_ACCESS_DENIED;
            goto exit;
        }

        // If the length of the existing path, plus the length of the proposed
        // filename, plus 3 (for a drive letter, colon and separating backslash)
        // or the length of the WINCE root, whichever is larger, exceeds the
        // maximum allowed path, fail the call.  WINCE doesn't use drive letters,
        // but down-level systems may need to.

        if (pstmTmp->s_cwPath + max(3,pstmTmp->s_pvol->v_cwsHostRoot) + cwName + 2 > MAX_PATH) {
            dwError = ERROR_FILENAME_EXCED_RANGE;
            goto exit;
        }
        pstmDir = pstmTmp;
    }

    __try {

        // Check for special volume names now, if the caller allows them.
        // We allow them, however, only if they're specified in the root.
        // Furthermore, since they aren't true streams, we return the root
        // stream, since it simplifies our life if all handles are always
        // associated with some stream.

        if ((flName & NAME_VOLUME) && ISROOTDIR(pstmDir)) {

            if (_wcsicmp(pwsName, TEXTW("VOL:")) == 0) {
                pstmRet = pstmDir;
                EnterCriticalSection(&pstmRet->s_cs);
                pstmRet->s_refs++;  // bump root stream ref and cs counts, too
                goto exit;
            }
        }

        // If the volume isn't valid, special volume names are the only names
        // allowed at this point, so fail the call.

        if (pstmDir->s_pvol->v_flags & VOLF_INVALID) {
            dwError = ERROR_ACCESS_DENIED;
            goto exit;
        }

        sh.sh_pstm = pstmDir;
        sh.sh_pos = 0;
        sh.sh_flags = SHF_BYNAME;

        // If this is a potential "creation" situation, tell the search code

        if (flName & NAME_CREATE)
            sh.sh_flags |= SHF_CREATE;

        sh.sh_cwPattern = min((WORD)cwName,ARRAYSIZE(sh.sh_awcPattern));
        memcpy(sh.sh_awcPattern, pwsName, sh.sh_cwPattern*sizeof(WCHAR));
        sh.sh_awcPattern[sh.sh_cwPattern] = 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    dwError = FindNext(&sh, &di, &fd);

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!OpenName. %s starts at cluster %d\r\n"), pwsName, di.di_clusEntry));

    if (dwError) {

        // No matching entry found.  If creation of a new entry is allowed,
        // then attempt to create one, using the DIRINFO saved from the
        // FindNext call.  If successful, it will update the DIRINFO structure,
        // and the rest of the code will simply think that FindNext succeeded.

        SetLastError(ERROR_SUCCESS);
        if (flName & NAME_CREATE) {

            if (dwError != ERROR_INVALID_NAME) {

                dwError = CreateName(pstmDir, sh.sh_awcPattern, &di, NULL, flName);
                // TEST_BREAK
                PWR_BREAK_NOTIFY(21);

                // Force the NAME_NEW bit off now, so that if CreateName
                // succeeded, we don't outsmart ourselves and fool the code
                // below into thinking that the file already existed.

                // Ditto for the NAME_MODE_WRITE (aka GENERIC_WRITE) bit,
                // because callers are allowed to create and write to new R/O
                // files.  It's only *existing* R/O files they're not allowed
                // to write to.

                flName &= ~(NAME_NEW | NAME_MODE_WRITE);
                if (!dwError) {
                    *pflName |= NAME_CREATED;
                    ASSERT(pstmDir->s_pbufCur);
                }
            }
        }
        else {
            dwError = (flName & NAME_DIR? ERROR_PATH_NOT_FOUND : ERROR_FILE_NOT_FOUND);
            
        }

    }
    else {

        // If file already exists and OPEN_ALWAYS or CREATE_ALWAYS was specified
        // we need to set last error to ERROR_ALREADY_EXISTS for NT compatibility.

        if ((flName & NAME_CREATE) || (flName & NAME_TRUNCATE)) 
            SetLastError(ERROR_ALREADY_EXISTS);
                    
        // If the file is read-only, then don't allow truncation.
        if ((flName & NAME_TRUNCATE) && (di.di_pde->de_attr & ATTR_READ_ONLY)) 
            dwError = ERROR_ACCESS_DENIED;

        ASSERT(pstmDir->s_pbufCur);
    }

    if (!dwError) {

        // NOTE that in this code path, the directory stream's buffer remains
        // held (either because FindNext succeeded, or because it failed but
        // CreateName succeeded).  So don't forget to call ReleaseStreamBuffer!

        if (flName & NAME_NEW) {

            // Even though ERROR_FILE_EXISTS seems a more logical error to return
            // when someone tries to create a new directory and a file already exists
            // with the same name, both Win95 and NT return ERROR_ALREADY_EXISTS,
            // so I'm blindly following their convention. -JTP

            dwError = (di.di_pde->de_attr & ATTR_DIRECTORY) || (flName & NAME_DIR)?
                        ERROR_ALREADY_EXISTS : ERROR_FILE_EXISTS;
        }
        else if (!(di.di_pde->de_attr & ATTR_DIRECTORY) && (flName & NAME_DIR)) {
            dwError = ERROR_PATH_NOT_FOUND;
        }
        else if ((di.di_pde->de_attr & ATTR_DIRECTORY) && !(flName & NAME_DIR)) {
            dwError = ERROR_ACCESS_DENIED;
        }
        else if ((di.di_pde->de_attr & ATTR_READ_ONLY) && (flName & NAME_MODE_WRITE)) {
            dwError = ERROR_ACCESS_DENIED;
        }
        else {

            ASSERT(di.di_sid.sid_clusDir != 0 &&
                   di.di_clusEntry != FAT_PSEUDO_CLUSTER && di.di_clusEntry != ROOT_PSEUDO_CLUSTER);

            pstmRet = OpenStream(pstmDir->s_pvol,
                                 di.di_clusEntry,
                                 &di.di_sid,
                                 pstmDir, &di, OPENSTREAM_CREATE);

            if (!pstmRet)
                dwError = ERROR_OUTOFMEMORY;
            else
                ASSERT(!pstmRet->s_cwPath || pstmRet->s_cwPath > pstmDir->s_cwPath + di.di_cwName);
        }

        ReleaseStreamBuffer(pstmDir, FALSE);
    }

  exit:
    if (dwError)
        SetLastError(dwError);

    // If we opened the directory stream on the caller's behalf, close it now;
    // otherwise, it's his, so leave it alone.

    if (pstmTmp)
        CloseStream(pstmTmp);

    return pstmRet;
}



















DWORD CreateName(PDSTREAM pstmDir, PCWSTR pwsName, PDIRINFO pdi, PDIRENTRY pdeClone, int flName)
{
    PVOLUME pvol;
    PDIRENTRY pde, pdeEnd;
    DWORD pos, posEnd, clde;
    PLFNDIRENTRY plde, pldeEnd;
    BYTE chksum;
    int cw, cwTail;
    PCWSTR pwsTail;
    DWORD cbOldSize;
    int cdeNeed = pdi->di_cdeNeed;
    DWORD dwError = ERROR_SUCCESS;

    ASSERT(cdeNeed >= 1 && pdi->di_clusEntry == UNKNOWN_CLUSTER && pdeClone != INVALID_PTR);

    


    if (pdi->di_achOEM[0] == 0)
        return ERROR_DISK_FULL;

    pvol = pstmDir->s_pvol;

    


    if (pvol->v_flags & VOLF_READONLY)
        return ERROR_WRITE_PROTECT;

#ifdef TFAT
    if (pvol->v_fTfat)
        LockFAT (pvol);
#endif    

    // To simplify CreateDirectory, if this is a new ATTR_DIRECTORY entry,
    // pre-allocate the one cluster that the directory will want later anyway.
    // This will save us work later (for example, we won't have to recommit
    // the DIRENTRY just to update the clusFirst field).

    if ((flName & (NAME_CREATE | ATTR_DIRECTORY)) == (NAME_CREATE | ATTR_DIRECTORY)) {

#ifdef TFAT
        if (!pvol->v_fTfat)
#endif        
            LockFAT(pvol);

        dwError = NewCluster(pvol, UNKNOWN_CLUSTER, &pdi->di_clusEntry);
        if (dwError == ERROR_SUCCESS && pdi->di_clusEntry == UNKNOWN_CLUSTER)
            dwError = ERROR_DISK_FULL;
        if (dwError) {
            UnlockFAT(pvol);
            return dwError;
        }

        DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!CreateName: new cluster for %s returned %d'\r\n"), pwsName, pdi->di_clusEntry));

        // We must mark this cluster USED right now, because if we have
        // to grow the directory containing this new directory, we don't
        // want it scarfing up the same cluster.  Besides, this will likely
        // save us a redundant write to the FAT in that case.

        dwError = PACK(pvol, pdi->di_clusEntry, EOF_CLUSTER, NULL);

        if (dwError)
            goto exit;
    }

    





    pos = pdi->di_pos;
    if (pdi->di_cdeFree)
        pos = pdi->di_posFree;

    // Remember the original size of the directory stream so we can tell
    // later if ResizeStream grew it.

    cbOldSize = pstmDir->s_size;
    posEnd = pos + cdeNeed * sizeof(DIRENTRY);

    if (posEnd > pdi->di_pos) {

        // Directory may need to grow.  First, make sure we're not
        // trying to push the directory beyond MAX_DIRENTRIES.

        if (posEnd/sizeof(DIRENTRY) > MAX_DIRENTRIES) {
            dwError = ERROR_DISK_FULL;  
            goto exit;
        }

        // Now make sure directory is large enough.  Make a special
        // call to ResizeStream to grow if necessary but never shrink.  We
        // treat this as a general error rather than a write error, because
        // ResizeStream can fail simply because the disk is full.

        dwError = ResizeStream(pstmDir, posEnd, RESIZESTREAM_NONE);
        if (dwError)
            goto exit;
    }

    clde = 0;
    plde = pldeEnd = NULL;

    cwTail = pdi->di_cwTail;
    pwsTail = pdi->di_pwsTail;

    // Advance cwTail by 1 character to encompass the NULL that OpenName and
    // FindFirst stored.  We don't need to store a NULL if the name fits into
    // the LFN entries perfectly however.

    if (cwTail < LDE_NAME_LEN)
        cwTail++;

    while (cdeNeed > 1) {

        if( plde >= pldeEnd ) {
            if( dwError = ReadStream( pstmDir, pos, &plde, &pldeEnd ))
                goto exit;

#ifdef TFAT
            // clone the buffer; If this existed before...
            if(pvol->v_fTfat)
            {
                dwError = CloneDirCluster( pvol, pstmDir, pstmDir->s_pbufCur->b_blk, NULL );
                if( ERROR_DISK_FULL == dwError ) {                    
                    goto exit;
                }
                else if (CLONE_CLUSTER_COMPLETE == dwError) {                    
                    // The current buffer might be changed in CloneDirCluster
                    // update plde to with the new buffer
                    if( dwError = ReadStream( pstmDir, pos, &plde, &pldeEnd )) {
                        DEBUGMSG (TRUE, (TEXT("FATFSD!TFAT: ERROR!!!! ReadStream failed after CloneDirCluster\n")));
                        ASSERT (0);   // It is bad if happens
                        goto exit;
                    }
                }

                // CloneDirCluster succeeded...let's assert it's right
                ASSERT( pos >= pstmDir->s_runList.r_StartPosition &&
                        pos < pstmDir->s_runList.r_EndPosition);
            }
#endif            
        }

        dwError = ModifyStreamBuffer(pstmDir, plde, sizeof(LFNDIRENTRY));
        if (dwError)
            goto exit;

        memset(plde, 0xFF, sizeof(LFNDIRENTRY));

        plde->lde_ord = (BYTE)(cdeNeed-1);
        if (clde++ == 0) {

            // The first LFN entry must be specially marked.  This is
            // also a good time to compute the checksum of the short name.

            chksum = ChkSumName(pdi->di_achOEM);
            plde->lde_ord |= LN_ORD_LAST_MASK;
        }

        plde->lde_attr = ATTR_LONG;
        plde->lde_flags = 0;
        plde->lde_chksum = chksum;
        plde->lde_clusFirst = 0;

        cw = cwTail >= LDE_LEN1? LDE_LEN1 : cwTail;
        memcpy(plde->lde_name1, pwsTail, cw*sizeof(WCHAR));

        cw = cwTail >= LDE_LEN1+LDE_LEN2? LDE_LEN2 : cwTail-LDE_LEN1;
        if (cw > 0)
            memcpy(plde->lde_name2, pwsTail+LDE_LEN1, cw*sizeof(WCHAR));

        cw = cwTail >= LDE_LEN1+LDE_LEN2+LDE_LEN3? LDE_LEN3 : cwTail-LDE_LEN1-LDE_LEN2;
        if (cw > 0)
            memcpy(plde->lde_name3, pwsTail+LDE_LEN1+LDE_LEN2, cw*sizeof(WCHAR));

        pwsTail -= LDE_NAME_LEN;
        cwTail = LDE_NAME_LEN;

        plde++;
        pos += sizeof(LFNDIRENTRY);
        cdeNeed--;
    }

    // Now write the primary DIRENTRY

    pde = (PDIRENTRY)plde;

    if( pde >= ( PDIRENTRY )pldeEnd ) {
        if( dwError = ReadStream( pstmDir, pos, &pde, &pdeEnd ))
            goto exit;

#ifdef TFAT
        // clone the buffer, if it wasn't just added by resizestream
        if(pvol->v_fTfat)
        {
            dwError = CloneDirCluster( pvol, pstmDir, pstmDir->s_pbufCur->b_blk, NULL );
            if( ERROR_DISK_FULL == dwError ) {
                goto exit;
            }
            else if (CLONE_CLUSTER_COMPLETE == dwError) {
                // The current buffer might be changed in CloneDirCluster
                // update plde with the new buffer
                if( dwError = ReadStream( pstmDir, pos, &pde, &pdeEnd )) {
                    DEBUGMSG (TRUE, (TEXT("FATFSD!TFAT: ERROR!!!! ReadStream failed after CloneDirCluster\n")));
                    ASSERT (0);   // It is bad if happens
                    goto exit;
                }
            }
        }
#endif        
    }

    dwError = ModifyStreamBuffer(pstmDir, pde, sizeof(DIRENTRY));
    if (dwError)
        goto exit;

    CreateDirEntry(pstmDir, pde, pdeClone, (BYTE)flName, pdi->di_clusEntry);
    memcpy(pde->de_name, pdi->di_achOEM, sizeof(pde->de_name));

    // TEST_BREAK
    PWR_BREAK_NOTIFY(23);

    // If ResizeStream had to add any new clusters, then any portion
    // we haven't written yet needs to be zeroed.

    if (cbOldSize != pstmDir->s_size && posEnd < pstmDir->s_size) {

        PBUF pbuf = pstmDir->s_pbufCur;

        // Because WriteStreamData (usually doesn't but) may involve filling a
        // new buffer, and we don't want to lose the hold we've got on the current
        // stream buffer, we have to apply another hold.

        HoldBuffer(pbuf);
        WriteStreamData(pstmDir, posEnd, NULL, pstmDir->s_size-posEnd, NULL, FALSE);
        ReleaseStreamBuffer(pstmDir, FALSE);
        AssignStreamBuffer(pstmDir, pbuf, FALSE);
        UnholdBuffer(pbuf);
    }

    // Commit all buffers associated with the directory stream, held or not,
    // and don't release the current stream buffer either.

    dwError = WriteStreamBuffers(pstmDir);

  exit:
    if (pdi->di_clusEntry != UNKNOWN_CLUSTER) {

        // Since we pre-allocated a cluster for this new stream (namely di_clusEntry),
        // if there was an error, then we want to de-allocate that cluster.

        if (dwError) {
            PACK(pvol, pdi->di_clusEntry, FREE_CLUSTER, NULL);
            pdi->di_clusEntry = UNKNOWN_CLUSTER;
        }
        
#ifdef TFAT
        if (!pvol->v_fTfat)
#endif        
            UnlockFAT(pvol);

    }

    // Make sure any changes made to the FAT are written now, too.  Even if we
    // didn't have to pre-allocate a cluster for the new stream, we might have had to
    // resize the directory containing it (which would have affected the FAT as well),
    // and since we call ResizeStream without RESIZESTREAM_UPDATEFAT in order to minimize
    // writes to the FAT, it's a good idea to have an explicit write somewhere....

    if (!dwError) {
        WriteStreamBuffers(pvol->v_pstmFAT);
    }

#ifdef TFAT
    if (pvol->v_fTfat)
        UnlockFAT (pvol);
#endif    


    if (!dwError) {

        // Update the DIRINFO structure, so that the caller has all
        // the same info that a successful FindNext would have returned.

        pdi->di_pde = pde;
        pdi->di_pos = pos;
        SETSID(&pdi->di_sid, pos, pstmDir->s_clusFirst);

        // I can tell you're wondering why we're reloading di_clusEntry
        // from the DIRENTRY, since we passed di_clusEntry to CreateDirEntry
        // in the first place, so wouldn't they already be the same?  The
        // answer is NO if CreateDirEntry was also given a pde to clone;
        // in that case, we DO need to reload di_clusEntry from the DIRENTRY.

        pdi->di_clusEntry = GETDIRENTRYCLUSTER(pvol,pde);
        if (pdi->di_clusEntry == NO_CLUSTER)
            pdi->di_clusEntry = UNKNOWN_CLUSTER;
    }
    return dwError;
}


/*  DestroyName - destroy a DIRENTRY in the specified directory stream
 *
 *  ENTRY
 *      pstmDir - pointer to directory stream
 *      pdi - pointer to DIRINFO (information returned by search)
 *
 *  EXIT
 *      Error code (0 if success, non-zero if error)
 *
 *  NOTES
 *      The DIRINFO structure identifies what DIRENTRY to blow away.  The
 *      stream's current buffer must be held and must contain the entry to
 *      be deleted.
 *
 *      A deliberate peculiarity of this function is that it does not extract
 *      clusFirst from the DIRENTRY to determine what clusters to decommit.
 *      It relies on the (normally identical) clusEntry field in the DIRINFO
 *      structure instead.  The reason is to give caller's the option of leaving
 *      the cluster chain intact, in case they're simply destroying a name as
 *      part of, say, a RENAME operation.
 */

DWORD DestroyName(PDSTREAM pstmDir, PDIRINFO pdi)
{
    DWORD dwError;
    PDIRENTRY pde, pdeEnd;
    PVOLUME pvol = pstmDir->s_pvol;
    
#ifdef DEBUG
    char achName[sizeof(pdi->di_pde->de_name)];
#endif

#ifdef TFAT
    DWORD blkNew;
#endif

    // We kill the directory entries first and THEN any clusters
    // so that in the event of a crash, you're more likely to end up
    // with orphaned clusters than cross-linkable clusters.  However,
    // *guaranteeing* that would require changing the very first
    // ModifyStreamBuffer's commit parm to TRUE, which is undesirable.

    // Since the stream's buffer has been held for us, and that buffer
    // contains the primary DIRENTRY for this file, kill that entry first.

    ASSERT(pdi->di_pde);

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!Enter DestroyName (%.11hs)\r\n"), pdi->di_pde->de_name));

    // TFAT, LockFAT around WriteAndReleaseStreamBuffers(pstmDir) and FreezeClusters
    // the direntry matches what's in the FAT
    LockFAT(pvol);


#ifdef TFAT
    if (pvol->v_fTfat) {
        // we need to clone the current buffer
        // before we make any modifications!
        if (CLONE_CLUSTER_COMPLETE == CloneDirCluster( pvol, pstmDir, pstmDir->s_pbufCur->b_blk, &blkNew ))
        {
            // The current buffer might be changed in CloneDirCluster
            // update plde to with the new buffer
            if( dwError = ReadStream( pstmDir, pdi->di_pos, &pdi->di_pde, NULL))
                goto exit;
        }
    }
#endif

    dwError = ModifyStreamBuffer(pstmDir, pdi->di_pde, sizeof(DIRENTRY));
    if (dwError)
        goto exit;

#ifdef DEBUG
    memcpy(achName, pdi->di_pde->de_name, sizeof(pdi->di_pde->de_name));
#endif
    pdi->di_pde->de_name[0] = DIRFREE;

    // Now see if there were also LFN entries that need to be killed;
    // since all the LFN entries have to be associated with the entry above,
    // we can just kill every entry we find until we reach a non-LFN entry.

    if (pdi->di_posLFN != INVALID_POS) {

        




        


        while(( dwError = ReadStream( pstmDir, pdi->di_posLFN, &pde, &pdeEnd )) == ERROR_SUCCESS ) {
#ifdef TFAT     
            if (pvol->v_fTfat) {
                // We need to check if we need to clone the block for TFAT vols.
                if(pstmDir->s_pbufCur->b_blk != blkNew ) {
                    // If we read in a different block than we replaced
                    // earlier, we need to replace this one as well.
    			if (CLONE_CLUSTER_COMPLETE == CloneDirCluster( pvol, pstmDir, pstmDir->s_pbufCur->b_blk, &blkNew ))
    			{
                        // The current buffer might be changed in CloneDirCluster
                        // update plde to with the new buffer
                        if( dwError = ReadStream( pstmDir, pdi->di_posLFN, &pde, &pdeEnd )) {
                            RETAILMSG (TRUE, (TEXT("FATFSD!TFAT: ERROR!!!! ReadStream failed after CloneDirCluster\n")));
                            ASSERT (0);  // It is bad if happens
                            goto exit;
                        }
                        
                        // CloneDirCluster succeeded
                        ASSERT( blkNew == pstmDir->s_pbufCur->b_blk );
    			}
                    
                    /* NOTE: blkNew will now be the blk of the current buffer,
                     * so this loop should only come again if there is
                     * another change in blocks (which there shouldn't be)
                     */
                }
            }
#endif            
            do {
                if( !ISLFNDIRENTRY( pde ))
                    goto lfndone;
                if(( dwError = ModifyStreamBuffer( pstmDir, pde, sizeof( DIRENTRY ))) != ERROR_SUCCESS )
                    goto lfndone;
                pde->de_name[ 0 ] = DIRFREE;

                // TEST_BREAK
                PWR_BREAK_NOTIFY(24);            
                
            } while( pdi->di_posLFN += sizeof( DIRENTRY ), ++pde < pdeEnd );
        }
       lfndone:
        ;
    }


    dwError = WriteAndReleaseStreamBuffers(pstmDir);

#ifdef TFAT
    // WriteAndReleaseStreamBuffers may not commit buffer if file is not write-through
    if (pvol->v_fTfat) {
        CommitStreamBuffers (pstmDir);
    }
#endif

    // Now kill all the cluster entries associated with this file.
    // We enter the FAT's critical section even though we're just freeing
    // clusters, because in the process, we could alter the FAT stream's
    // current buffer, and that could mess up someone else accessing the FAT.

    if (dwError == ERROR_SUCCESS && pdi->di_clusEntry != UNKNOWN_CLUSTER) {
        DWORD dwData = FREE_CLUSTER;

#ifdef DEBUG
#ifdef FATUI
        if (ZONE_PROMPTS) {
            int i;
            FATUIDATA fui;
            fui.dwSize = sizeof(fui);
            fui.dwFlags = FATUI_YES | FATUI_NO | FATUI_CANCEL | FATUI_DEBUGMESSAGE;
            fui.idsEvent = (UINT)TEXTW("Preparing to delete clusters for '%1!.11hs!'.\r\n\r\nSelect Yes to delete, No to mark bad, or Cancel to leave allocated.");
            fui.idsEventCaption = IDS_FATUI_WARNING;
            fui.cuiParams = 1;
            fui.auiParams[0].dwType = UIPARAM_VALUE;
            fui.auiParams[0].dwValue = (DWORD)achName;

            i = FATUIEvent(hFATFS, pvol->v_pwsHostRoot+1, &fui);
            if (i == FATUI_CANCEL)
                goto exit;
            if (i == FATUI_NO)
                dwData = FAT32_BAD;
        }
#endif                
#endif

#ifdef TFAT
        if (!pvol->v_fTfat)
#endif                
        {
            while (pdi->di_clusEntry >= DATA_CLUSTER && !ISEOF(pvol, pdi->di_clusEntry)) {
                FreeClustersOnDisk(pvol, pdi->di_clusEntry, 1);
                if (dwError = PACK(pvol, pdi->di_clusEntry, dwData, &pdi->di_clusEntry))
                    break;
            }
        }
#ifdef TFAT
        if (pvol->v_fTfat)
        // TFAT - don't delete the cluster, freeze it instead
        {
            DWORD clusFirst = NO_CLUSTER, clusNext = pdi->di_clusEntry, clusEnd;

            // Get the last cluster in the chain
            dwError = ERROR_SUCCESS;
            while (!dwError && clusNext >= DATA_CLUSTER && !ISEOF(pvol, clusNext)) 
            {
                if (NO_CLUSTER == clusFirst)
                    clusFirst = clusNext;

                clusEnd = clusNext;

                // get next cluster
                dwError = UNPACK( pvol, clusNext, &clusNext);
            }

            if (NO_CLUSTER != clusFirst){
                FreezeClusters (pvol, clusFirst, clusEnd);
            }
        }
#endif

        WriteAndReleaseStreamBuffers(pvol->v_pstmFAT);
        
    }

exit:
    UnlockFAT(pvol);    
    return dwError;
}


/*  FindFirst - find the directory stream containing the specified file
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsPath - pointer to pathname
 *      psh - pointer to SHANDLE (required)
 *      pdi - pointer to DIRINFO (optional, NULL if not needed)
 *      pfd - pointer to WIN32_FIND_DATAW structure (required)
 *      flName - zero or more NAME_* flags (eg, NAME_FILE, NAME_DIR)
 *      clusFail - cluster of stream not allowed in path
 *      (normally, this is UNKNOWN_CLUSTER, to perform unrestricted opens)
 *
 *      Additionally, psh->sh_flags must be set appropriately.  At a minimum,
 *      it must contain SHF_BYNAME, but can also contain SHF_WILD, SHF_DOTDOT,
 *      and SHF_CREATE.
 *
 *  EXIT
 *      If successful, it will return the attributes of the file from
 *      the file's DIRENTRY, and if a pdi was supplied, it will be filled
 *      in and current stream buffer left held.
 *
 *      If not successful, the attributes will be set to INVALID_ATTR,
 *      and the current stream buffer will not held.
 *
 *      Finally, if the stream didn't exist, or no pdi was supplied, no
 *      stream will be returned.  Otherwise, the directory stream containing
 *      the specified file will be returned.
 */

PDSTREAM FindFirst(PVOLUME pvol, PCWSTR pwsPath, PSHANDLE psh, PDIRINFO pdi, PWIN32_FIND_DATAW pfd, int flName, DWORD clusFail)
{
    PWSTR pwsName;
    DWORD len;
    DWORD dwError = ERROR_SUCCESS;

    psh->sh_pstm = NULL;

    __try {
        pfd->dwFileAttributes = INVALID_ATTR;

        psh->sh_pstm = OpenPath(pvol, pwsPath, &pwsName, &len, flName, clusFail);
        if (psh->sh_pstm == NULL) {
            if (ERROR_ALREADY_EXISTS == GetLastError()) {
                SetLastError(ERROR_PATH_NOT_FOUND);
            }
            goto exit;
        }

        if (len == 0) {

            // The caller must be trying to find '\\'.  That's normally
            // a no-no.  FAT_FindFirstFileW makes an exception, but it makes
            // no sense internally, because the root is never contained by
            // another directory stream.

            dwError = ERROR_FILE_NOT_FOUND;
            goto exit;
        }

        psh->sh_pos = 0;
        psh->sh_cwPattern = (WORD)len;
        memcpy(psh->sh_awcPattern, pwsName, len*sizeof(WCHAR));
        psh->sh_awcPattern[len] = 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    dwError = FindNext(psh, pdi, pfd);

  exit:
    if (dwError == ERROR_FILE_NOT_FOUND && (flName & (NAME_FILE|NAME_DIR)) == NAME_DIR)
        dwError = ERROR_PATH_NOT_FOUND;

    if (dwError)
        SetLastError(dwError);

    if (!pdi) {
        if (psh->sh_pstm) {
            CloseStream(psh->sh_pstm);
            psh->sh_pstm = NULL;
        }
    }
    return psh->sh_pstm;
}


/*  MatchNext - worker for FindNext
 *
 *  ENTRY
 *      psh - pointer to SHANDLE structure
 *      len - length of current name
 *      pws - pointer to current name
 *      pde - pointer to DIRENTRY to read from if match
 *
 *  EXIT
 *      TRUE if (another) file was found, FALSE if not
 */

__inline BOOL MatchNext(PSHANDLE psh, PWSTR pws, int len, PDIRENTRY pde)
{
    int fMask = psh->sh_flags & SHF_BYMASK;

    // Check the match criteria, and then check for that kind of match

    ASSERT(fMask);              // make sure flags are set to something

    if (psh->sh_pstm->s_cwPath + max(3,psh->sh_pstm->s_pvol->v_cwsHostRoot) + len + 2 > MAX_PATH)
        return FALSE;

    if (len > MAX_PATH)
        return FALSE;

    return(
        fMask == SHF_BYNAME &&
        MatchesWildcardMask(psh->sh_cwPattern, psh->sh_awcPattern, len, pws)
      ||
        fMask == SHF_BYORD &&
        (DWORD)(psh->sh_h) == psh->sh_pos/sizeof(DIRENTRY)
      ||
        fMask == SHF_BYCLUS &&
        (DWORD)(psh->sh_h) == GETDIRENTRYCLUSTER(psh->sh_pstm->s_pvol, pde)
    );
}


/*  CopyTimes - worker for FindNext
 *
 *  ENTRY
 *      pfd - pointer to WIN32_FIND_DATAW structure
 *      pde - pointer to DIRENTRY containing timestamps to transfer
 *
 *  EXIT
 *      None
 */

__inline void CopyTimes(PWIN32_FIND_DATAW pfd, PDIRENTRY pde)
{
    // DOSTimeToFileTime can fail because it doesn't range-check any of
    // the date/time values in the directory entry;  it just pulls them apart
    // and calls SystemTimeToFileTime, which won't initialize the FILETIME
    // structure if there's an error.  So we preset all the FILETIMEs to zero,
    // because that's what the Win32 API dictates when a time is unknown/not
    // supported.

    memset(&pfd->ftCreationTime, 0, sizeof(pfd->ftCreationTime) +
                                    sizeof(pfd->ftLastAccessTime) +
                                    sizeof(pfd->ftLastWriteTime));

    DOSTimeToFileTime(pde->de_createdDate,
                      pde->de_createdTime,
                      pde->de_createdTimeMsec, &pfd->ftCreationTime);

    DOSTimeToFileTime(pde->de_date, pde->de_time, 0, &pfd->ftLastWriteTime);

    DOSTimeToFileTime(pde->de_lastAccessDate, 0, 0, &pfd->ftLastAccessTime);
}


/*  FindNext - worker for FindFirstFileW, FindNextFileW, etc.
 *
 *  Callers need to supply a WIN32_FIND_DATAW structure even if they're
 *  not interested in "find data".  This is because the search may involve
 *  comparisons against LFN directory entries, which have to be built a piece
 *  at a time, and a wildcard comparison cannot be performed until the entire
 *  name has been built.  NON-wildcard searches can perform their comparisons
 *  a piece at a time, and don't necessarily need the MAX_PATH buffer that
 *  WIN32_FIND_DATAW provides us, but using the same buffer strategy in both
 *  cases yields better code.
 *
 *  ENTRY
 *      psh - pointer to SHANDLE structure
 *      pdi - pointer to DIRINFO (optional, NULL if not needed)
 *      pfd - pointer to user's WIN32_FIND_DATAW structure (required)
 *
 *  EXIT
 *      0 if successful, error code if not
 *
 *  On success, most of the fields in the WIN32_FIND_DATAW structure are filled
 *  in (eg, dwFileAttributes, nFileSizeHigh, nFileSizeLow, cFileName, and some
 *  CE-specific fields like dwOID).  Some are only filled in depending on the
 *  type of search however, like all the FILETIME fields (eg, ftCreationTime,
 *  ftLastAccessTime, ftLastWriteTime).  For example, SHF_BYCLUS searches, which
 *  are only used and supported internally, don't care about those fields, so we
 *  don't waste time filling them in.
 */

DWORD FindNext(PSHANDLE psh, PDIRINFO pdi, PWIN32_FIND_DATAW pfd)
{

    PDIRENTRY pdeEnd;           // ptr to ending directory entry
    BYTE chksum;                // LFN entry checksum of 8.3 name
    int seqLFN = -1;            // next LFN DIRENTRY seq # expected
    PCWSTR pwsComp;
    PWSTR pwsBuff;
    BOOL fComp = FALSE;
    DIRINFO di;
    DWORD dwError = ERROR_SUCCESS;
    PDSTREAM pstmDir = psh->sh_pstm;
    PDWORD pdwShortNameBitArray = NULL;

    ERRFALSE(sizeof(pfd->cFileName) >= sizeof(WCHAR [LDE_NAME_LEN*LN_MAX_ORD_NUM]));
    ASSERT(pstmDir);
    ASSERT(psh->sh_flags);      // make sure flags are set to something

    ASSERT(OWNCRITICALSECTION(&pstmDir->s_cs));

    memset(&di, 0, sizeof(DIRINFO));

    di.di_pos = psh->sh_pos;
    di.di_posLFN = INVALID_POS;
    di.di_clusEntry = UNKNOWN_CLUSTER;
    di.di_cdeNeed = -1;

    if (psh->sh_pos == INVALID_POS) {
        dwError = ERROR_FILE_NOT_FOUND;
        goto norelease;
    }

    if ((psh->sh_flags & SHF_BYMASK) == SHF_BYNAME) {

        di.di_cdeNeed = UniToOEMName(psh->sh_pstm->s_pvol, di.di_achOEM, psh->sh_awcPattern, psh->sh_cwPattern, psh->sh_pstm->s_pvol->v_nCodePage);

        // If the name is invalid (-2), or contains wildcards when wildcards
        // are not allowed, fail immediately.

        if (di.di_cdeNeed == -2 || di.di_cdeNeed == -1 && !(psh->sh_flags & SHF_WILD)) {
            dwError = ERROR_INVALID_NAME;
            goto norelease;
        }

        // If the name is OEM, then force the CREATE flag off.  Otherwise,
        // we'll leave the flag on if the caller turned it on (meaning he
        // wants us to check every normal DIRENTRY for a collision with the
        // short name in achOEM and set it to some name that doesn't conflict).

        if (di.di_cdeNeed >= 1) {

            di.di_flags |= DIRINFO_OEM;
            di.di_chksum = ChkSumName(di.di_achOEM);

            if (di.di_cdeNeed == 1) {
                psh->sh_flags &= ~SHF_CREATE;
            }

            // If the name fits in achOEM but contains lower-case, then cdeNeed
            // is already correct (2), but cwTail and pwsTail still need to be set.

            else if (di.di_cdeNeed == 2) {
                di.di_cwTail = psh->sh_cwPattern;
                di.di_pwsTail = psh->sh_awcPattern;
            }

            // If there's some uncertainty about the charset mapping, then revert to
            // the UNICODE case.

            else {
                ASSERT(di.di_cdeNeed == 3);
                di.di_cdeNeed = 0;
            }
        }

        // If the name is UNICODE (0), then cdeNeed needs to be recomputed,
        // based on the length of the UNICODE name.  Furthermore, unlike the
        // wildcard case, we will be able to compare it piece by piece.
        // pwsTail points to the end-most piece to compare, and cwTail is its
        // length.

        if (di.di_cdeNeed == 0) {
            fComp = TRUE;
            di.di_pwsTail = psh->sh_awcPattern + psh->sh_cwPattern;
            di.di_cdeNeed = ((psh->sh_cwPattern+LDE_NAME_LEN-1)/LDE_NAME_LEN) + 1;
            if ((di.di_cwTail = psh->sh_cwPattern % LDE_NAME_LEN) == 0)
                di.di_cwTail = LDE_NAME_LEN;
            di.di_pwsTail -= di.di_cwTail;
        }

        // Also record the complete length of the name.  If we actually
        // find a match, we will update this field again (since the match
        // might have been based on a short name and we always return
        // long names), but if we don't find a match, this still needs to
        // be set in case the caller wants to create it (via CreateName).

        di.di_cwName = psh->sh_cwPattern;
    }

    // Create a short name conflict bit array if the caller may need
    // to create a new LFN entry.  We must add 1 to MAX_SHORT_NAMES because
    // the set of addressible bits in the array starts with bit 0.

    if (psh->sh_flags & SHF_CREATE) {
        DWORD cb = (((MAX_SHORT_NAMES+1)+31)/32 + 2)*sizeof(DWORD);     
        InitNumericTail(&di);
        // CreateBitArray(pdwShortNameBitArray, MAX_SHORT_NAMES+1);
        if (cb > 2*sizeof(DWORD)) {                         
            pdwShortNameBitArray = (PDWORD)HeapAlloc (hHeap, HEAP_ZERO_MEMORY, cb);
        }                                                   
        if (pdwShortNameBitArray) {                                  
            pdwShortNameBitArray[0] = MAX_SHORT_NAMES+1;                       
        }                                                   
        
    }

    while (!dwError) {

        dwError = ReadStream(pstmDir, di.di_pos, &di.di_pde, &pdeEnd);
        if (dwError) {
            if (dwError == ERROR_HANDLE_EOF) {
                if (di.di_pos == 0) {
                    DEBUGMSG(ZONE_ERRORS,(TEXT("FATFS!FindNext: 0-length directory???\r\n")));
                    dwError = ERROR_INVALID_DATA;
                }
                else
                    dwError = ERROR_FILE_NOT_FOUND;
            }
            else
                DEBUGMSG(ZONE_ERRORS,(TEXT("FATFS!FindNext: unexpected error (%d)\r\n"), dwError));
            di.di_posLFN = INVALID_POS;
            break;
        }

        // Let's do a little more error-checking before we dive in;  in particular,
        // if this is not a root directory, then it must have "dot" and "dotdot" entries
        // at the top, else the directory is corrupt.

        if (!ISROOTDIR(pstmDir) && di.di_pos == 0 && di.di_pde+1 < pdeEnd) {

            DWORD dwClus;

            if (*(PDWORD)di.di_pde->de_name != 0x2020202e ||
                *(PDWORD)(di.di_pde+1)->de_name != 0x20202e2e) {
                DEBUGMSG(ZONE_ERRORS,(TEXT("FATFS!FindNext: bogus directory!\r\n")));
                dwError = ERROR_INVALID_DATA;
                break;
            }

            // NOTE: I'm reserving ERROR_FILE_CORRUPT to indicate "correctable" directory
            // errors.  All the caller has to do is plug in correct cluster values.  See the
            // code in SCAN.C for more details.

            dwClus = GETDIRENTRYCLUSTER(pstmDir->s_pvol, di.di_pde);
            if (dwClus != pstmDir->s_clusFirst) {
                DEBUGMSG(ZONE_ERRORS,(TEXT("FATFS!FindNext: bogus '.' cluster (0x%08x instead of 0x%08x)\r\n"), dwClus, pstmDir->s_clusFirst));
                dwError = ERROR_FILE_CORRUPT;
                break;
            }

            dwClus = GETDIRENTRYCLUSTER(pstmDir->s_pvol, di.di_pde+1);
            if (!ISPARENTROOTDIR(pstmDir) || dwClus != NO_CLUSTER) {
                if (pstmDir->s_sid.sid_clusDir >= DATA_CLUSTER &&
                    pstmDir->s_sid.sid_clusDir != UNKNOWN_CLUSTER && dwClus != pstmDir->s_sid.sid_clusDir) {
                    DEBUGMSG(ZONE_ERRORS,(TEXT("FATFS!FindNext: bogus '..' cluster (0x%08x instead of 0x%08x)\r\n"), dwClus, pstmDir->s_sid.sid_clusDir));
                    dwError = ERROR_FILE_CORRUPT;
                    break;
                }
            }
        }

      __try {

        do {
            // Check for a deleted entry.

            if (di.di_pde->de_name[0] == DIRFREE) {

                // Deleted file entry.  If this is the first one,
                // remember its location.

                if (di.di_cdeFree < di.di_cdeNeed && di.di_cdeFree++ == 0)
                    di.di_posFree = di.di_pos;

                // Reset LFN sequence/position info
                goto resetlfn;
            }

            // Check for an ending entry.

            if (di.di_pde->de_name[0] == DIREND) {

                // Empty file entry.  If this is the first one,
                // remember its location.

                if (di.di_cdeFree < di.di_cdeNeed && di.di_cdeFree++ == 0)
                    di.di_posFree = di.di_pos;

                di.di_posLFN = INVALID_POS;
                dwError = ERROR_FILE_NOT_FOUND;
                goto release;
            }

            // Check for an LFN entry.

            if (ISLFNDIRENTRY(di.di_pde)) {

                int cwComp = LDE_NAME_LEN;
                PLFNDIRENTRY plde = (PLFNDIRENTRY)di.di_pde;
                int cbName3 = sizeof(plde->lde_name3);

                // If lde_flags is not zero, our assumption must be that
                // this is some new kind of LFN entry that we don't fully
                // understand, and must therefore ignore....

                if (plde->lde_flags != 0)
                    goto resetall;

                // Initialization code for initial LFN entry

                if (plde->lde_ord & LN_ORD_LAST_MASK) {

                    seqLFN = plde->lde_ord & ~LN_ORD_LAST_MASK;

                    if (seqLFN > LN_MAX_ORD_NUM) {

                        // This LFN sequence has TOO MANY entries, which also
                        // means it won't fit in a MAX_PATH buffer.  We could
                        // treat the sequence like orphaned LFN entries, but
                        // that seems a rather risky proposition, since we may be
                        // talking directory corruption here.  Safer to just ignore
                        // them and rely on SCANDSKW to fix things up. -JTP
                        goto resetall;
                    }

                    if (!(di.di_flags & DIRINFO_OEM) && di.di_cdeNeed > 1 && seqLFN != di.di_cdeNeed-1) {

                        // Optimization: this LFN sequence does not have the requisite
                        // number of entries, so it can't possibly be a match.
                        goto resetall;
                    }

                    chksum = plde->lde_chksum;

                    // If this LFN sequence has the wrong OEM chksum, it might simply
                    // be due to charset differences between the system where the OEM name
                    // was originally created and the current system, so unlike previous
                    // releases, we will no longer bail out and 'goto resetall';  we will
                    // instead continue to collect all the LFN chunks and do the full name
                    // comparison.  In other words, we are no longer dependent on the
                    // accuracy of the OEM version of the name (as computed by UniToOEMName). -JTP

                    DEBUGMSGW(di.di_cdeNeed == 1 && chksum != di.di_chksum && ZONE_APIS && ZONE_ERRORS,(DBGTEXTW("FATFS!FindNext: lfn chksum (%x) doesn't match OEM chksum (%x) for '%.11hs'\r\n"), chksum, di.di_chksum, di.di_achOEM));

                    // Remember this LFN's starting position
                    di.di_posLFN = di.di_pos;

                    cwComp = di.di_cwTail;
                    pwsComp = di.di_pwsTail;
                    pwsBuff = &pfd->cFileName[LDE_NAME_LEN*LN_MAX_ORD_NUM];
                    *--pwsBuff = 0;     // guarantee a NULL-terminator

                    // If this LFN contains the max number of LFNDIRENTRYs,
                    // we have to scoot pwsBuff up one WCHAR and reduce the
                    // amount of the final memcpy (of lde_name3) by one WCHAR,
                    // to insure that the NULL-terminator above is preserved.

                    if (seqLFN == LN_MAX_ORD_NUM) {
                        pwsBuff++;
                        cbName3 -= sizeof(WCHAR);
                    }
                }

                // Skip LFN entries we're not interested in

                else if (seqLFN == -1) {
                    goto resetall;
                }

                // Recover LFN entries that are inconsistent

                else if (seqLFN != plde->lde_ord || chksum != plde->lde_chksum) {

                    // All preceding LFN entries are deemed bogus/recoverable.
                    // If they can be merged with the free count, do it, otherwise
                    // leave them recorded as unrecoverable.

                    DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!FindNext: LFN sequence (0x%x vs. 0x%x) or LFN chksum (0x%x vs. 0x%x) error\n"), seqLFN, plde->lde_ord, chksum, plde->lde_chksum));

                    di.di_posOrphan = di.di_posLFN;
                    di.di_cdeOrphan = (di.di_pos - di.di_posLFN)/sizeof(LFNDIRENTRY);

                    if (di.di_posFree + di.di_cdeFree * sizeof(LFNDIRENTRY) == di.di_posLFN) {
                        di.di_cdeFree += di.di_cdeOrphan;
                        di.di_cdeOrphan = 0;
                    }
                    goto resetall;
                }

                if (di.di_cdeFree < di.di_cdeNeed)
                    di.di_cdeFree = 0;

                // LFN directory entry in sequence. Copy the name entry by
                // entry into a scratch buffer to be matched when all of the
                // entries in the name have been processed.

                pwsBuff -= LDE_NAME_LEN;
                memcpy(pwsBuff, plde->lde_name1, sizeof(plde->lde_name1));
                memcpy(pwsBuff+LDE_LEN1, plde->lde_name2, sizeof(plde->lde_name2));
                memcpy(pwsBuff+LDE_LEN1+LDE_LEN2, plde->lde_name3, cbName3);

                // If we can compare individual pieces, do it now

                if (fComp) {

                    // We perform a quick comparison on the first
                    // character past the end of the LFN (if one exists)
                    // and if that character isn't a NULL, then this
                    // entry isn't a match;  otherwise, compare entire piece.

                    if (cwComp != LDE_NAME_LEN && pwsBuff[cwComp] != 0 ||
                        _wcsnicmp(pwsBuff, pwsComp, cwComp) != 0) {
                        goto resetlfn;
                    }
                    pwsComp -= LDE_NAME_LEN;
                }

                --seqLFN;
                continue;
            }

#ifdef TFAT
            // As an optimization for TFAT, if the first cluster in the directory contains
            // a volume label, then we can skip this cluster because the entire cluster is
            // filled with volume labels.
            
            if (pstmDir->s_pvol->v_fTfat && ((di.di_pde->de_attr & ATTR_DIR_MASK) == ATTR_VOLUME_ID) && 
                (di.di_pos < pstmDir->s_pvol->v_cbClus) && (di.di_cdeFree == 0) && (di.di_posLFN == INVALID_POS))
            {
                // Set the position a cluster into the directory and ReadStream again.
                di.di_pos = pstmDir->s_pvol->v_cbClus;
                break;
            }
#endif

            // Check for a "normal" directory entry.

            if ((di.di_pde->de_attr & ATTR_DIR_MASK) != ATTR_VOLUME_ID) {

                int len;

                



                if (!(psh->sh_flags & SHF_DOTDOT) &&
                    (*(PDWORD)di.di_pde->de_name == 0x2020202e ||
                     *(PDWORD)di.di_pde->de_name == 0x20202e2e)) {
                    goto resetall;
                }

                // Normal directory entry.  If preceeding LFN
                // directory entries matched, handle this entry
                // as a match.  Otherwise, if the name is 8.3
                // compatible, then try to match the 8.3 OEM name
                // contained in this entry.

                if (seqLFN == 0) {

                    if (chksum == ChkSumName(di.di_pde->de_name)) {

                        len = wcslen(pwsBuff);

                        // We processed all the LFN chunks for the current entry,
                        // (since seqLFN is now 0), so if fComp is TRUE, that means
                        // we compared every chunk as we went, and they must have
                        // all matched, so let's just 'goto match'

                        if (fComp)
                            goto match;

                        // If cdeNeed == -1, we're dealing with wildcards *or* a
                        // a non-name-based search, so we have to explicitly check
                        // for a match.

                        if (di.di_cdeNeed == -1)
                            goto checkmatch;

                        // Well, we must be performing a name-based search, and since
                        // we didn't compare the LFN chunks as we went, we did at least
                        // collect them all in pwsBuff, so let's see if the full LFN
                        // matches....
                        //
                        // Note: we didn't *always* used to do this, which meant that we
                        // always fell into the "normal directory entry" case below, which
                        // which only compared the OEM forms of the filenames;  normally,
                        // that was sufficient, but in cases where the OEM name was created
                        // on a system with different charset mappings, that comparison could
                        // fail.  We should rely on the 8.3 OEM name IFF there are no LFN
                        // entries -- and in this case, there *is*.... -JTP

                        if (len == psh->sh_cwPattern && _wcsnicmp(pwsBuff, psh->sh_awcPattern, len) == 0)
                            goto match;
                    }
                    else {

                        // All preceding LFN entries are deemed bogus/recoverable.
                        // If they can be merged with the free count, do it, otherwise
                        // leave them recorded as unrecoverable.

                        DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!FindNext: LFN chksum (0x%x vs. 0x%x) error\n"), chksum, ChkSumName(di.di_pde->de_name)));

                        di.di_posOrphan = di.di_posLFN;
                        di.di_cdeOrphan = (di.di_pos - di.di_posLFN)/sizeof(LFNDIRENTRY);

                        if (di.di_posFree + di.di_cdeFree * sizeof(LFNDIRENTRY) != di.di_posLFN) {
                            di.di_cdeFree = 0;
                        } else {
                            di.di_cdeFree += di.di_cdeOrphan;
                            di.di_cdeOrphan = 0;
                        }

                        // We used to bail at this point (goto checkshort), but the right
                        // thing to do is just treat the current DIRENTRY as a short-name only.
                        seqLFN = -1;
                    }
                }

                // Normal directory entry, and either there were no preceding LFN
                // entries, or there were but we couldn't make use of them.

                if (di.di_cdeNeed == -1 ||
                    (di.di_flags & DIRINFO_OEM) &&
                    memcmp(di.di_pde->de_name,
                           di.di_achOEM,
                           sizeof(di.di_pde->de_name)) == 0) {
                    
                    // If seqLFN == 0, then the preceding LFN entries probably
                    // provide better name information, so use the OEM name only if
                    // we have to.

                    if (seqLFN != 0) {
                        pwsBuff = pfd->cFileName;
                        len = OEMToUniName(pwsBuff, di.di_pde->de_name, psh->sh_pstm->s_pvol->v_nCodePage);
                    }
                    if (di.di_flags & DIRINFO_OEM)
                        goto match;
                    else
                        goto checkmatch;
                }
                goto checkshort;

              checkmatch:
                // Resync the SHANDLE pos with DIRINFO pos, for MatchNext
                psh->sh_pos = di.di_pos;

                if (MatchNext(psh, pwsBuff, len, di.di_pde)) {

                  match:
                    SETSID(&di.di_sid, di.di_pos, pstmDir->s_clusFirst);
#ifdef UNDER_CE
                    // NT does not have OID defined                            
                    pfd->dwOID = OIDFROMSID(pstmDir->s_pvol, &di.di_sid);
#endif
                    // Mask off any invalid attributes
                    pfd->dwFileAttributes = di.di_pde->de_attr & ~ATTR_INVALID;
                    if (pfd->dwFileAttributes == 0)
                        pfd->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

                    pfd->nFileSizeHigh = 0;
                    pfd->nFileSizeLow = di.di_pde->de_size;

                    // If the name was built up from a series of LFN entries,
                    // it may need to be slid down to the start of pfd->cFilename
                    // (since we built it up from the top down).

                    if (pwsBuff != pfd->cFileName) {
                        memcpy(pfd->cFileName, pwsBuff, len*sizeof(WCHAR));
                        pfd->cFileName[len] = 0;
                    }
                    // If the caller passed us a DIRINFO, then leave the
                    // the stream buffer held and let him have the address
                    // of the DIRENTRY (di.di_pde).  We do not need to advance
                    // the search position or fill in the entire WIN32_FIND_DATAW
                    // for such callers.

                    if (pdi) {
                        di.di_clusEntry =
                            GETDIRENTRYCLUSTER(pstmDir->s_pvol, di.di_pde);
                        if (di.di_clusEntry == NO_CLUSTER)
                            di.di_clusEntry = UNKNOWN_CLUSTER;
                        di.di_cwName = len;
                        goto norelease;
                    }

                    // We don't need to waste time converting/copying all the
                    // file times for certain types of searches (eg, SHF_BYCLUS
                    // or SHF_DOTDOT).

                    if ((psh->sh_flags & SHF_BYMASK) != SHF_BYCLUS && !(psh->sh_flags & SHF_DOTDOT))
                        CopyTimes(pfd, di.di_pde);

                    di.di_pde = NULL;
                    psh->sh_pos = di.di_pos + sizeof(DIRENTRY);
                    goto release;
                }

                // We just can't seem to do anything with this
                // directory entry.  Well, if the caller may need to
                // create a new LFN entry, determine if the current
                // entry conflicts with any of the possible short names
                // we can generate.

              checkshort:
                if (pdwShortNameBitArray)
                    CheckNumericTail(&di, pdwShortNameBitArray);
            }

          resetall:
            if (di.di_cdeFree < di.di_cdeNeed)
                di.di_cdeFree = 0;

          resetlfn:
            seqLFN = -1;                // reset LFN match sequence #
            di.di_posLFN = INVALID_POS;

        } while ((di.di_pos += sizeof(DIRENTRY)), ++di.di_pde < pdeEnd);

      } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
        di.di_pde = NULL;
      }
    }

  release:
    ReleaseStreamBuffer(pstmDir, FALSE);

  norelease:
    if (pdi) {

        // Before copying the DIRINFO structure, see if the caller was
        // searching on behalf of a CREATE, and try to generate a non-existent
        // short name that he can use to perform the creation.

        if (pdwShortNameBitArray)
            GenerateNumericTail(&di, pdwShortNameBitArray);

        *pdi = di;
    }

    if (dwError)
        psh->sh_pos = INVALID_POS;

    if (pdwShortNameBitArray) {                                  
        HeapFree (hHeap, 0, pdwShortNameBitArray);
    }

    return dwError;
}


void CreateDirEntry(PDSTREAM pstmDir, PDIRENTRY pde, PDIRENTRY pdeClone, BYTE bAttr, DWORD clusFirst)
{
    FILETIME ft;
    SYSTEMTIME st;

    if (pdeClone) {
        *pde = *pdeClone;
        return;
    }

    memset(pde->de_name, ' ', sizeof(pde->de_name));
    memset(&pde->de_attr, 0, sizeof(DIRENTRY)-sizeof(pde->de_name));

    pde->de_attr = bAttr;

    if (clusFirst != UNKNOWN_CLUSTER && clusFirst >= DATA_CLUSTER)
        SETDIRENTRYCLUSTER(pstmDir->s_pvol, pde, clusFirst);

    GetSystemTimeAsFileTime(&ft);

    // Directories don't really benefit from creation and access
    // timestamps, but we'll set them anyway.  MS-DOS only sets access
    // and modified dates, but Win95's VFAT sets all three, so so will we.

    FileTimeToDOSTime(&ft,
                      &pde->de_createdDate,
                      &pde->de_createdTime,
                      &pde->de_createdTimeMsec);

    FileTimeToDOSTime(&ft, &pde->de_date, &pde->de_time, NULL);

    // This is simpler than truncating the local system time to midnight,
    // converting it to a local file time, then converting it to a non-local
    // file time, then calling FileTimeToDOSTime, which turns around and
    // undoes most of that.

    GetLocalTime(&st);

    DEBUGMSG(ZONE_STREAMS,(DBGTEXT("FATFS!CreateDirEntry: local file system ACCESS time: y=%d,m=%d,d=%d,h=%d,m=%d,s=%d\r\n"), st.wYear, st.wMonth, st.wDay, 0, 0, 0));

    pde->de_lastAccessDate = ((st.wYear-1980) << 9) | (st.wMonth << 5) | (st.wDay);
}
