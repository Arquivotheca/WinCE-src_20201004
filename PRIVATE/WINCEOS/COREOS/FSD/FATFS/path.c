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

    path.c

Abstract:

    This file contains the path based API routines for the FAT file system.

Revision History:

--*/


#ifdef PATH_CACHING
#define PARENT_PATH_CACHING
#endif

#include "fatfs.h"

CONST WCHAR szDotDot[3] = TEXTW("..");


/*  FAT_CreateDirectoryW - Create a new subdirectory
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsPathName - pointer to name of new subdirectory
 *      lpSecurityAttributes - pointer to security attributes (ignored)
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FAT_CreateDirectoryW(PVOLUME pvol, PCWSTR pwsPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    int len;
    PWSTR pwsLast;
    PDSTREAM pstm, pstmDir;
    DIRENTRY adeDir[2];
    DWORD dwError;
    int flName = NAME_DIR | NAME_NEW | NAME_CREATE | ATTR_DIRECTORY;
    DSID sid, sidParent;
    DWORD cchFilePath = 0;
    
    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_CreateDirectoryW(%d chars: %-.64s)\r\n"), wcslen(pwsPathName), pwsPathName));

    if (!FATEnter(pvol, LOGID_CREATEDIRECTORY))
        return FALSE;

    if (pvol->v_flags & (VOLF_INVALID | VOLF_UNMOUNTED | VOLF_FROZEN | VOLF_LOCKED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto error;
    }

    if (pvol->v_flags & VOLF_READONLY) {
        dwError = ERROR_WRITE_PROTECT;
        goto error;
    }

    // The following code would be simpler than OpenPath+OpenName:
    //
    //      OpenName((PDSTREAM)pvol, pwsPathName, 0, &flName);
    //
    // but we also need to know some information about the parent directory
    // stream, so we have to open the path first.  And we keep it open across
    // creation of the "." and ".." entries, to insure that the cluster of the
    // parent directory doesn't change.

    pstmDir = OpenPath(pvol, pwsPathName, &pwsLast, &len, flName, UNKNOWN_CLUSTER);
    if (!pstmDir) {
        FATExit(pvol, LOGID_CREATEDIRECTORY);
        DEBUGMSGW(ZONE_APIS || ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_CreateDirectoryW(%d chars: %-.64s) returned FALSE (%d)\r\n"), wcslen(pwsPathName), pwsPathName, GetLastError()));
        return FALSE;
    }

    ASSERT(pvol == pstmDir->s_pvol);

    // If the length of the last element is zero, the caller must be trying to
    // (re)create the root directory, which of course already exists.

    if (len == 0) {
        dwError = ERROR_ALREADY_EXISTS;
        goto exit;
    }

    // If the length of the existing path, plus the length of the proposed
    // new directory, plus a separating backslash and a null terminator exceeds the maximum allowed
    // directory path, fail the call.

    // ASSERT(pstmDir->s_cwPath + (DWORD)len + 1 == wcslen(pwsPathName) + ((pwsPathName[0] != TEXTW('\\')) && (pwsPathName[0] != TEXTW('/'))) - ((pwsLast[len] == TEXT('\\')) || (pwsLast[len] == TEXT('/'))));

    cchFilePath = pvol->v_cwsHostRoot + pstmDir->s_cwPath + len + 2;
    if (cchFilePath > MAX_PATH || pvol->v_cwsHostRoot > MAX_PATH || len > MAX_PATH) {
        dwError = ERROR_FILENAME_EXCED_RANGE;
        goto exit;
    }

    dwError = ERROR_SUCCESS;

#ifdef TFAT
    if (pvol->v_fTfat)
        LockFAT (pvol);
#endif    

    PREFAST_SUPPRESS (508, "");
    pstm = OpenName(pstmDir, pwsLast, len, &flName);
    if (!pstm) {
        CloseStream(pstmDir);
        FATExit(pvol, LOGID_CREATEDIRECTORY);
#ifdef TFAT
        if (pvol->v_fTfat)
            UnlockFAT (pvol);
#endif            
        DEBUGMSGW(ZONE_APIS || ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_CreateDirectoryW(%d chars: %-.64s) returned FALSE (%d)\r\n"), wcslen(pwsPathName), pwsPathName, GetLastError()));
        return FALSE;
    }

    // When OpenStream (called by OpenName) initialized the stream structure
    // for this new directory, since it was a directory, it initialized s_size
    // to MAX_DIRSIZE, because the size field in a directory's DIRENTRY is
    // never valid.  In this case however, we KNOW the size of the stream is
    // zero, so we set it to zero to help WriteStreamData realize that it doesn't
    // need to read the cluster it's about to write.

    pstm->s_size = 0;
    
    // Build the "." and ".." entries now.  WriteStreamData is the best choice,
    // because it handles lots of stuff, including resizing and not reading
    // data we're going to overwrite anyway.  NOTE: The FAT32 specification
    // dictates that ".." entries that point to their volume's root directory
    // should contain 0 in their cluster field, just like FAT12 and FAT16 volumes,
    // for better application compatibility. -JTP

    CreateDirEntry(pstm, &adeDir[0], NULL, ATTR_DIRECTORY, pstm->s_clusFirst);
    CreateDirEntry(pstm, &adeDir[1], NULL, ATTR_DIRECTORY, ISROOTDIR(pstmDir)? NO_CLUSTER : pstmDir->s_clusFirst);

    adeDir[0].de_name[0] =
    adeDir[1].de_name[0] = adeDir[1].de_name[1] = '.';

    WriteStreamData(pstm, 0, adeDir, sizeof(adeDir), NULL, FALSE);

#ifdef TFAT
        // TFAT: File the rest of the cluster with volumes,  In order to avoid modify the first cluster, 
        // so new file/dir can be created in the second cluster, moving directories is restricted.
    if (pvol->v_fTfat) {
        DIRENTRY adeVolume;
        DWORD    offset;
        int	     iVolumeNo;

        CreateDirEntry( pstm, &adeVolume, NULL, ATTR_VOLUME_ID, NO_CLUSTER );
        memcpy(adeVolume.de_name, "DONT_DEL000", sizeof (adeVolume.de_name));
        		
        for (offset = sizeof(adeDir), iVolumeNo = 0; offset < pvol->v_cbClus; offset += sizeof(adeVolume), iVolumeNo++)
        {
            adeVolume.de_name[8]  = '0' + (iVolumeNo / 100);
            adeVolume.de_name[9]  = '0' + ((iVolumeNo / 10) % 10);
            adeVolume.de_name[10] = '0' + (iVolumeNo % 10);

            WriteStreamData(pstm, offset, &adeVolume, sizeof(adeVolume), NULL, FALSE);
        }
        CommitStream (pstm, TRUE);
    }
    else 

#endif
    {
        // ZERO the rest of the cluster
        WriteStreamData(pstm, sizeof(adeDir), NULL, pvol->v_cbClus-sizeof(adeDir), NULL, FALSE);
    }

    CloseStream(pstmDir);
    pstmDir = NULL;

#ifdef TFAT
    if (pvol->v_fTfat) {
        UnlockFAT(pvol);
    }
#endif

#ifdef PATH_CACHING
    PathCacheCreate(pvol, pwsPathName, 0, pstm);
#endif

    memcpy (&sid, &pstm->s_sid, sizeof(DSID));
    memcpy (&sidParent, &pstm->s_sidParent, sizeof(DSID));

    CloseStream(pstm);
    FILESYSTEMNOTIFICATION(pvol, DB_CEOID_CREATED, 0, SHCNE_MKDIR, &sid, &sidParent, NULL, NULL, NULL, DBGTEXTW("FAT_CreateDirectoryW"));


  exit:
    if (pstmDir)
        CloseStream(pstmDir);

    if (dwError) {
      error:
        SetLastError(dwError);
    }

    FATExit(pvol, LOGID_CREATEDIRECTORY);

#ifdef TFAT
    if (pvol->v_fTfat && (dwError == ERROR_SUCCESS)) {
        dwError = CommitTransactions (pvol);
    }
#endif

    DEBUGMSGW(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXTW("FATFS!FAT_CreateDirectoryW(%d chars: %-.64s) returned 0x%x (%d)\r\n"), wcslen(pwsPathName), pwsPathName, dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}


/*  FAT_RemoveDirectoryW - Destroy an existing subdirectory
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsPathName - pointer to name of existing subdirectory
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FAT_RemoveDirectoryW(PVOLUME pvol, PCWSTR pwsPathName)
{
    PDSTREAM pstmDir;
    SHANDLE sh;
    DIRINFO di;
    WIN32_FIND_DATAW fd;
    DWORD dwError = ERROR_SUCCESS;
#ifdef SHELL_CALLBACK_NOTIFICATION
    CEOIDINFO oiOld;
#endif
    DWORD dwSHCNE = 0;
    DSID sidParent; 

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_RemoveDirectoryW(%d chars: %-.64s)\r\n"), wcslen(pwsPathName), pwsPathName));

    if (!FATEnter(pvol, LOGID_REMOVEDIRECTORY))
        return FALSE;

    if (pvol->v_flags & (VOLF_INVALID | VOLF_UNMOUNTED | VOLF_FROZEN | VOLF_LOCKED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto error;
    }

    if (pvol->v_flags & VOLF_READONLY) {
        dwError = ERROR_WRITE_PROTECT;
        goto error;
    }

    // FindFirst will call SetLastError appropriately, so
    // all we have to do is bail if it doesn't return a stream.

    sh.sh_flags = SHF_BYNAME;
    pstmDir = FindFirst(pvol, pwsPathName, &sh, &di, &fd, NAME_DIR, UNKNOWN_CLUSTER);
    if (!pstmDir) {
        FATExit(pvol, LOGID_REMOVEDIRECTORY);
        DEBUGMSGW(ZONE_APIS || ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_RemoveDirectoryW(%d chars: %-.64s) returned FALSE (%d)\r\n"), wcslen(pwsPathName), pwsPathName, GetLastError()));
        return FALSE;
    }

    if (fd.dwFileAttributes == INVALID_ATTR) {
        CloseStream(pstmDir);
        FATExit(pvol, LOGID_REMOVEDIRECTORY);
        DEBUGMSGW(ZONE_APIS || ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_RemoveDirectoryW(%d chars: %-.64s) returned FALSE (%d)\r\n"), wcslen(pwsPathName), pwsPathName, GetLastError()));
        return FALSE;
    }

    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    // Retrieve the fully-qualified pathname of the source, in case we need
    // it for the FILESYSTEMNOTIFICATION at the end of the function.  We have to
    // do the wacky buffer holding/unholding because pstmDir's current buffer is
    // in a known state, and we can't tolerate GetSIDInfo mucking that state up.

#ifdef SHELL_CALLBACK_NOTIFICATION
    if (pfnShell && ZONE_SHELLMSGS) {
        PBUF pbuf = pstmDir->s_pbufCur;
        HoldBuffer(pbuf);
        GetSIDInfo(pvol, &di.di_sid, &oiOld);
        ReleaseStreamBuffer(pstmDir, FALSE);
        AssignStreamBuffer(pstmDir, pbuf, FALSE);
        UnholdBuffer(pbuf);
    }
#endif

    // Before we can kill the DIRENTRY, we need to open it as a
    // stream and call FindNext to see if there are any files in it.

    if (di.di_clusEntry != UNKNOWN_CLUSTER) {

        ASSERT(di.di_clusEntry != FAT_PSEUDO_CLUSTER &&
               di.di_clusEntry != ROOT_PSEUDO_CLUSTER);

        sh.sh_pstm = OpenStream(pstmDir->s_pvol,
                                di.di_clusEntry,
                                &di.di_sid,
                                pstmDir, &di, OPENSTREAM_CREATE);
        if (sh.sh_pstm) {
            sh.sh_pos = 0;
            sh.sh_flags = SHF_BYNAME | SHF_WILD;
            sh.sh_cwPattern = 1;
            sh.sh_awcPattern[0] = TEXTW('*');

            dwError = FindNext(&sh, NULL, &fd);
            if (!dwError) {
                CloseStream(sh.sh_pstm);
                dwError = ERROR_DIR_NOT_EMPTY;
                goto exit;
            }

            // Check for search handles to this directory
            if (sh.sh_pstm->s_dlOpenHandles.pfhNext != (PFHANDLE)&sh.sh_pstm->s_dlOpenHandles) {
                CloseStream(sh.sh_pstm);
                dwError = ERROR_SHARING_VIOLATION;
                goto exit;
            }
            
            CloseStream(sh.sh_pstm);

        }
    }


    // If we're still here, then the directory stream is empty (except for
    // "." and ".." entries, which we didn't ask FindNext to find anyway), so
    // we can remove the DIRENTRY and its associated clusters.

    dwError = DestroyName(pstmDir, &di);
    // TEST_BREAK
    PWR_BREAK_NOTIFY(31);

    if (!dwError) {

        dwSHCNE = SHCNE_RMDIR;
        memcpy (&sidParent, &pstmDir->s_sid, sizeof(DSID));

#ifdef PATH_CACHING
        if (PathCacheInvalidate(pvol, pwsPathName)) {

#ifdef PARENT_PATH_CACHING            
            if (!ISROOTDIR(pstmDir)) {

                // Since we just lost a cache entry, let's make up for
                // it by generating a cache entry for the current directory's
                // parent directory.  BUGBUG: it would be more efficient if
                // FindFirst could have done this for us. -JTP

                sh.sh_pstm = pstmDir;
                sh.sh_pos = 0;
                sh.sh_flags = SHF_BYNAME | SHF_DOTDOT;
                sh.sh_cwPattern = ARRAYSIZE(szDotDot)-1;
                memcpy(sh.sh_awcPattern, szDotDot, sizeof(szDotDot));

                if (FindNext(&sh, &di, &fd) == ERROR_SUCCESS &&
                    di.di_clusEntry != UNKNOWN_CLUSTER) {

                    sh.sh_pstm = OpenStream(pstmDir->s_pvol,
                                            di.di_clusEntry, NULL,
                                            NULL, NULL, OPENSTREAM_CREATE);

                    // BUGBUG: This doesn't tell us the parent directory's
                    // name, attr, or parent OID.  To get all that, we'd
                    // have to open the parent's parent and perform a BYCLUS
                    // search on di.di_clusEntry. -JTP

                    if (sh.sh_pstm) {
                        PathCacheCreate(pvol, pwsPathName, -2, sh.sh_pstm);
                        CloseStream(sh.sh_pstm);
                    }
                }
            }
#endif  // PARENT_PATH_CACHING
        }
#endif  // PATH_CACHING
    }

  exit:

    CloseStream(pstmDir);

    if (dwSHCNE)
        FILESYSTEMNOTIFICATION(pvol, 0, DB_CEOID_DIRECTORY_DELETED, dwSHCNE, NULL, NULL, &di.di_sid, &sidParent, &oiOld, DBGTEXTW("FAT_RemoveDirectoryW"));

    if (dwError) {
      error:
        SetLastError(dwError);
    }

    FATExit(pvol, LOGID_REMOVEDIRECTORY);

#ifdef TFAT
    if (pvol->v_fTfat && (dwError == ERROR_SUCCESS))
        dwError = CommitTransactions (pvol);
#endif

    DEBUGMSGW(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXTW("FATFS!FAT_RemoveDirectoryW(%d chars: %-.64s) returned 0x%x (%d)\r\n"), wcslen(pwsPathName), pwsPathName, dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}


/*  FAT_GetFileAttributesW - Get file/subdirectory attributes
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsFileName - pointer to name of existing file/subdirectory
 *
 *  EXIT
 *      Attributes of file/subdirectory if it exists, 0xFFFFFFFF if it
 *      does not (call GetLastError for error code).
 */

DWORD FAT_GetFileAttributesW(PVOLUME pvol, PCWSTR pwsFileName)
{
    SHANDLE sh;
    WIN32_FIND_DATAW fd;
    WCHAR   wsFName[MAX_PATH+1];

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_GetFileAttributesW(%d chars: %s)\r\n"), wcslen(pwsFileName), pwsFileName));

    if (!FATEnter(NULL, LOGID_NONE))
        return INVALID_ATTR;

    if (pvol->v_flags & (VOLF_INVALID | VOLF_UNMOUNTED | VOLF_FROZEN | VOLF_WRITELOCKED)) {
        SetLastError(ERROR_ACCESS_DENIED);
        FATExit(pvol, LOGID_NONE);
        DEBUGMSGW(ZONE_APIS || ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_GetFileAttributesW(%-.64s) returned 0x%x (%d)\r\n"), pwsFileName, INVALID_ATTR, ERROR_ACCESS_DENIED));
        return INVALID_ATTR;
    }

    // FindFirst will call SetLastError appropriately, so
    // all we have to do is bail if it doesn't return a stream.

    wcsncpy (wsFName, pwsFileName, MAX_PATH);
    wsFName[MAX_PATH] = TEXT('\0'); // Ensure null
    // Desktop will remove trailing '\' && '/'
    while (wcslen(wsFName) && ((wsFName[wcslen(wsFName)-1] == TEXT('\\')) ||
                               (wsFName[wcslen(wsFName)-1] == TEXT('/')))) {
        // Trim trailing slash
        wsFName[wcslen(wsFName)-1] = TEXT('\0');
    }

    sh.sh_flags = SHF_BYNAME;
    FindFirst(pvol, wsFName, &sh, NULL, &fd, NAME_FILE | NAME_DIR, UNKNOWN_CLUSTER);

    FATExit(pvol, LOGID_NONE);

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_GetFileAttributesW(%-.64s) returned 0x%x (%d)\r\n"), pwsFileName, fd.dwFileAttributes, fd.dwFileAttributes != INVALID_ATTR? 0 : GetLastError()));

    return fd.dwFileAttributes;
}


/*  FAT_SetFileAttributesW - Set file/subdirectory attributes
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsFileName - pointer to name of existing file/subdirectory
 *      dwAttributes - new attributes for file/subdirectory
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FAT_SetFileAttributesW(PVOLUME pvol, PCWSTR pwsFileName, DWORD dwAttributes)
{
    PDSTREAM pstmDir;
    SHANDLE sh;
    DIRINFO di;
    DWORD dw, dwError = ERROR_SUCCESS;
    WIN32_FIND_DATAW fd;
    DWORD dwSHCNE = 0;
    DSID sidParent;

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_SetFileAttributesW(0x%x,%d chars: %s)\r\n"), dwAttributes, wcslen(pwsFileName), pwsFileName));

    // NOTE: Even though this call modifies the volume, it never has any impact
    // on file system integrity, so we don't waste time logging it (ie, LOGID_NONE).

    if (!FATEnter(NULL, LOGID_NONE))
        return FALSE;

    if (pvol->v_flags & (VOLF_INVALID | VOLF_UNMOUNTED | VOLF_FROZEN | VOLF_LOCKED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto error;
    }

    if (pvol->v_flags & VOLF_READONLY) {
        dwError = ERROR_WRITE_PROTECT;
        goto error;
    }

    // FindFirst will call SetLastError appropriately, so
    // all we have to do is bail if it doesn't return a stream.

    sh.sh_flags = SHF_BYNAME;

    pstmDir = FindFirst(pvol, pwsFileName, &sh, &di, &fd, NAME_FILE | NAME_DIR, UNKNOWN_CLUSTER);
    if (!pstmDir) {
        FATExit(pvol, LOGID_NONE);
        DEBUGMSGW(ZONE_APIS || ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_SetFileAttributesW(%-.64s) returned FALSE (%d)\r\n"), pwsFileName, GetLastError()));
        return FALSE;
    }

    // If FindFirst was successful...

    if (fd.dwFileAttributes != INVALID_ATTR) {

        // Clear any bits that are not changable
        dwAttributes &= ATTR_CHANGEABLE;

        // NOTE: We pass 0 for the size instead of sizeof(DIRENTRY) to disable logging
        // of this modification, since we are not logging these particular calls (by virtue
        // of LOGID_NONE above).

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dwAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        }

#ifdef TFAT
    if (pvol->v_fTfat && (pvol->v_flFATFS & FATFS_TFAT_NONATOMIC_SECTOR)) {
        // we need to clone the current buffer
        // before we make any modifications!
        LockFAT(pvol);
        
        if (CLONE_CLUSTER_COMPLETE == CloneDirCluster( pvol, pstmDir, pstmDir->s_pbufCur->b_blk, NULL ))
        {
            // The current buffer might be changed in CloneDirCluster
            // update plde to with the new buffer
            dwError = ReadStream( pstmDir, di.di_pos, &di.di_pde, NULL);
        }
    }
#endif

        if (!dwError)
            dwError = ModifyStreamBuffer(pstmDir, di.di_pde, 0);
        if (!dwError)
            di.di_pde->de_attr = (BYTE)dwAttributes;

        dwSHCNE = SHCNE_UPDATEITEM;
        memcpy (&sidParent, &pstmDir->s_sid, sizeof(DSID));

#ifdef TFAT
    if (pvol->v_fTfat && (pvol->v_flFATFS & FATFS_TFAT_NONATOMIC_SECTOR)) {
        if (!dwError)
            dwError = CommitStreamBuffers (pstmDir);
        if (!dwError)
            dwError = CommitTransactions (pvol);
        UnlockFAT(pvol);
    }
#endif
        
    }


    dw = CloseStream(pstmDir);
    if (!dwError)
        dwError = dw;

    if (dwSHCNE)
        FILESYSTEMNOTIFICATION(pvol, DB_CEOID_CHANGED, 0, dwSHCNE, &di.di_sid, &sidParent, NULL, NULL, NULL, DBGTEXTW("FAT_SetFileAttributesW"));


    if (dwError) {
      error:
        SetLastError(dwError);
    }

    FATExit(pvol, LOGID_NONE);

    DEBUGMSGW(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXTW("FATFS!FAT_SetFileAttributesW(0x%x,%-.64s) returned 0x%x (%d)\r\n"), dwAttributes, pwsFileName, dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}


/*  FAT_DeleteFileW - Delete file
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsFileName - pointer to name of existing file
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 *
 *  NOTES
 *      A file marked FILE_ATTRIBUTE_READONLY cannot be deleted.  You have to
 *      remove that attribute first, with SetFileAttributes.
 *
 *      An open file cannot be deleted.  All open handles must be closed first.
 *
 *      A subdirectory cannot be deleted with this call either.  You have to
 *      use RemoveDirectory instead.
 */

BOOL FAT_DeleteFileW(PVOLUME pvol, PCWSTR pwsFileName)
{
    PDSTREAM pstmDir;
    SHANDLE sh;
    DIRINFO di;
    WIN32_FIND_DATAW fd;
    DWORD dwError = ERROR_SUCCESS;
#ifdef SHELL_CALLBACK_NOTIFICATION
    CEOIDINFO oiOld;
#endif
    DSID sidParent;

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_DeleteFileW(%d chars: %s)\r\n"), wcslen(pwsFileName), pwsFileName));

    if (!FATEnter(pvol, LOGID_DELETEFILE))
        return FALSE;

    if (pvol->v_flags & (VOLF_INVALID | VOLF_UNMOUNTED | VOLF_FROZEN | VOLF_LOCKED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto error;
    }

    if (pvol->v_flags & VOLF_READONLY) {
        dwError = ERROR_WRITE_PROTECT;
        goto error;
    }

    // FindFirst will call SetLastError appropriately, so
    // all we have to do is bail if it doesn't return a stream.

    sh.sh_flags = SHF_BYNAME;
    pstmDir = FindFirst(pvol, pwsFileName, &sh, &di, &fd, NAME_FILE, UNKNOWN_CLUSTER);
    if (!pstmDir) {
        FATExit(pvol, LOGID_DELETEFILE);
        DEBUGMSGW(ZONE_APIS || ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_DeleteFileW(%-.64s) returned FALSE (%d)\r\n"), pwsFileName, GetLastError()));
        return FALSE;
    }

    // If the source file doesn't exist, fail

    if (fd.dwFileAttributes == INVALID_ATTR) {
        CloseStream(pstmDir);
        FATExit(pvol, LOGID_DELETEFILE);
        DEBUGMSGW(ZONE_APIS || ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_DeleteFileW(%-.64s) returned FALSE (%d)\r\n"), pwsFileName, GetLastError()));
        return FALSE;
    }

    // If the source file is R/O or a directory, fail
    //
    // NOTE: Win95 reports ERROR_FILE_NOT_FOUND if the name is an existing
    // directory, whereas NT reports ERROR_ACCESS_DENIED.  I find the latter
    // to be more sensible. -JTP

    if (fd.dwFileAttributes & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY)) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    // If the source file has any open handles, fail

    if (CheckStreamHandles(pstmDir->s_pvol, &di.di_sid)) {
        dwError = ERROR_SHARING_VIOLATION;
        goto exit;
    }

    // Retrieve the fully-qualified pathname of the source, in case we need
    // it for the FILESYSTEMNOTIFICATION at the end of the function.  We have to
    // do the wacky buffer holding/unholding because pstmDir's current buffer is
    // in a known state, and we can't tolerate GetSIDInfo mucking that state up.

#ifdef SHELL_CALLBACK_NOTIFICATION
    if (pfnShell && ZONE_SHELLMSGS) {
        PBUF pbuf = pstmDir->s_pbufCur;
        HoldBuffer(pbuf);
        GetSIDInfo(pvol, &di.di_sid, &oiOld);
        ReleaseStreamBuffer(pstmDir, FALSE);
        AssignStreamBuffer(pstmDir, pbuf, FALSE);
        UnholdBuffer(pbuf);
    }
#endif

    dwError = DestroyName(pstmDir, &di);
    // TEST_BREAK
    PWR_BREAK_NOTIFY(31);

    if (!dwError)
        memcpy (&sidParent, &pstmDir->s_sid, sizeof (DSID));

  exit:    
    CloseStream(pstmDir);
    
    if (!dwError)
        FILESYSTEMNOTIFICATION(pvol, 0, DB_CEOID_FILE_DELETED, SHCNE_DELETE, NULL, NULL, &di.di_sid, &sidParent, &oiOld, DBGTEXTW("FAT_DeleteFileW"));

    if (dwError) {
      error:
        SetLastError(dwError);
    }

#ifdef TFAT    
    else if (pvol->v_fTfat)
        dwError = CommitTransactions (pvol);
#endif

    FATExit(pvol, LOGID_DELETEFILE);

    DEBUGMSGW(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXTW("FATFS!FAT_DeleteFileW(%-.64s) returned 0x%x (%d)\r\n"), pwsFileName, dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}


/*  FAT_MoveFileW
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsOldFileName - pointer to name of existing file
 *      pwsNewFileName - pointer to new name for file
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 *
 *  NOTES
 *      We call FindFirst once to obtain the source directory stream for the
 *      for the existing file, and if it really exists, we call FindFirst
 *      again to obtain the destination directory stream for the new file,
 *      verifying that the new name does NOT exist.  Then we create the new
 *      name and destroy the old.
 *
 *      When moving a directory, we must make sure that our traversal
 *      of the destination path does not cross the source directory, otherwise
 *      we will end up creating a circular directory chain.
 */

BOOL FAT_MoveFileW(PVOLUME pvol, PCWSTR pwsOldFileName, PCWSTR pwsNewFileName)
{
    PBUF pbufSrc;
    BYTE bAttrSrc;
    WIN32_FIND_DATAW fd;
    SHANDLE shSrc, shDst;
    DIRINFO diSrc, diDst;
    PDSTREAM pstmSrc, pstmDst;
    DIRENTRY deClone, *pdeClone;
    DWORD dwError = ERROR_SUCCESS;
    DWORD clusFail = UNKNOWN_CLUSTER;
#ifdef SHELL_CALLBACK_NOTIFICATION
    CEOIDINFO oiOld;
#endif
    DSID sidSrcParent, sidDstParent;


    DEBUGONLY(pdeClone = INVALID_PTR);

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_MoveFileW(%d->%d chars: %s->%s)\r\n"), wcslen(pwsOldFileName), wcslen(pwsNewFileName), pwsOldFileName, pwsNewFileName));

    if (!FATEnter(pvol, LOGID_MOVEFILE))
        return FALSE;

    if (pvol->v_flags & (VOLF_INVALID | VOLF_UNMOUNTED | VOLF_FROZEN | VOLF_LOCKED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto error;
    }

    if (pvol->v_flags & VOLF_READONLY) {
        dwError = ERROR_WRITE_PROTECT;
        goto error;
    }

    // We add an explicit call to OpenRoot because this function performs
    // two path traversals starting at the root, with a lock on the source
    // directory while it traverses the destination.  So if another thread
    // tried to open a file in that source directory at the same time, it would
    // block waiting for MoveFile to finish, but MoveFile couldn't finish if
    // the other thread currently had a stream open in MoveFile's destination
    // path.  The explicit OpenRoot prevents all other path-based calls from
    // even starting down that path.

    OpenRoot(pvol);

    shSrc.sh_flags = SHF_BYNAME;
    pstmSrc = FindFirst(pvol, pwsOldFileName, &shSrc, &diSrc, &fd, NAME_FILE | NAME_DIR, UNKNOWN_CLUSTER);
    if (!pstmSrc) {
        CloseRoot(pvol);
        FATExit(pvol, LOGID_MOVEFILE);
        DEBUGMSGW(ZONE_APIS || ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_MoveFileW(%s->%s) returned FALSE (%d)\r\n"), pwsOldFileName, pwsNewFileName, GetLastError()));
        return FALSE;
    }

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_MoveFileW.  Source (%s) located at cluster %d\r\n"), pwsOldFileName, diSrc.di_clusEntry));

    // If the source file doesn't exist, fail

    if (fd.dwFileAttributes == INVALID_ATTR) {
        CloseStream(pstmSrc);
        CloseRoot(pvol);
        FATExit(pvol, LOGID_MOVEFILE);
        DEBUGMSGW(ZONE_APIS || ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_MoveFileW(%s->%s) returned FALSE (%d)\r\n"), pwsOldFileName, pwsNewFileName, GetLastError()));
        return FALSE;
    }


    // If the source file has any open handles, fail
    if (StreamOpenedForExclAccess(pstmSrc->s_pvol, &diSrc.di_sid)) {
        dwError = ERROR_SHARING_VIOLATION;
        goto exit3;
    }

    bAttrSrc = (BYTE)fd.dwFileAttributes;

#ifdef TFAT
    if (pvol->v_fTfat && (pvol->v_flFATFS & FATFS_TFAT_DISABLE_MOVEDIR) && (bAttrSrc & ATTR_DIRECTORY)) {
        // Moving a directory has been disabled on this volume
        dwError = ERROR_ACCESS_DENIED;
        goto exit3;
    }
#endif

    // Retrieve the fully-qualified pathname of the source, in case we need
    // it for the FILESYSTEMNOTIFICATION at the end of the function.  We have to
    // do the wacky buffer holding/unholding because pstmSrc's current buffer is
    // in a known state, and we can't tolerate GetSIDInfo mucking that state up.

#ifdef SHELL_CALLBACK_NOTIFICATION
    if (pfnShell && ZONE_SHELLMSGS) {
        PBUF pbuf = pstmSrc->s_pbufCur;
        HoldBuffer(pbuf);
        GetSIDInfo(pvol, &diSrc.di_sid, &oiOld);
        ReleaseStreamBuffer(pstmSrc, FALSE);
        AssignStreamBuffer(pstmSrc, pbuf, FALSE);
        UnholdBuffer(pbuf);
    }
#endif

    // We don't need the source pattern anymore, but we *might* need
    // the complete source filename (if we had to delete the source first,
    // and then got an error creating the destination) in that rare case
    // where we have to recreate the source.  See "that rare case" below.

    memcpy(shSrc.sh_awcPattern, fd.cFileName, sizeof(shSrc.sh_awcPattern));

    // Because the second FindFirst (below) could operate on the same
    // stream that the first FindFirst (above) just operated on, and because
    // we don't want to lose the first FindFirst's current buffer, we record
    // what that buffer is and apply our own hold to it.

    pbufSrc = pstmSrc->s_pbufCur;
    HoldBuffer(pbufSrc);

    // If the source is a directory, don't allow the destination path to
    // contain the source stream's cluster.

    if (bAttrSrc & ATTR_DIRECTORY)
        clusFail = diSrc.di_clusEntry;

    shDst.sh_flags = SHF_BYNAME | SHF_CREATE;
    pstmDst = FindFirst(pvol, pwsNewFileName, &shDst, &diDst, &fd, NAME_FILE | NAME_DIR, clusFail);
    if (!pstmDst) {
        dwError = GetLastError();
        goto exit2;
    }
    if (pvol->v_cwsHostRoot+pstmDst->s_cwPath+shDst.sh_cwPattern+2 > MAX_PATH) {
        dwError = ERROR_FILENAME_EXCED_RANGE;
        goto exit2;
    }

    // Good, both directory streams exist, and we already know that the file
    // DOES exist in the source.  Confirm it does NOT exist in the destination.

    if (fd.dwFileAttributes != INVALID_ATTR) {

        // There is one case where the destination is allowed to exist,
        // and that is when the destination DIRENTRY is the same as the source,
        // but the original names are NOT the same, implying that the caller simply
        // wants to change some portion of the *case* of the (long) filename.

        if (diSrc.di_sid.sid_clusDir == diDst.di_sid.sid_clusDir &&
            diSrc.di_sid.sid_ordDir == diDst.di_sid.sid_ordDir &&
            memcmp(fd.cFileName, shDst.sh_awcPattern, (shDst.sh_cwPattern+1)*sizeof(WCHAR)) != 0) {

            // Since the destination exists, we shall destroy the source and search
            // again.  We copy the source DIRENTRY first, so that CreateName will still
            // have a valid DIRENTRY to clone, and then we zap diSrc.di_clusEntry so that
            // DestroyName doesn't try to decommit the file's clusters (if any).

            deClone = *diSrc.di_pde;
            pdeClone = &deClone;

            diSrc.di_clusEntry = UNKNOWN_CLUSTER;
            if (dwError = DestroyName(pstmSrc, &diSrc))
                goto exit1;
            
            // We know that FindNext should fail now, but we still need to call
            // again so that it will fill diDst with the latest available DIRENTRY
            // information (which CreateName needs to do its job).

            shDst.sh_pos = 0;
            shDst.sh_flags = SHF_BYNAME | SHF_CREATE;
            if ((dwError = FindNext(&shDst, &diDst, &fd)) != ERROR_FILE_NOT_FOUND)
                goto exit1;
        }
        else {
            dwError = ERROR_ALREADY_EXISTS;
            goto exit1;
        }
    }
    else {

        // BUGBUG: Since the destination file didn't exist, FindFirst must
        // have called SetLastError, so clear the error just to be safe -JTP

        pdeClone = diSrc.di_pde;
        SetLastError(0);
    }

    // Good, the new name does NOT (or no longer) exists in the destination,
    // so we can now attempt to create it.

    // I always try to create the new name first because there are more
    // reasons why CreateName could fail (ie, the destination directory might
    // have to grow and the disk might be full) than DestroyName.
    //
    // BUGBUG: Either way, there can be some ugly side-effects.  First,
    // if the create succeeds but then the disk gets yanked, both DIRENTRYs
    // will exist and you'll see cross-linked files;  depending on what
    // the user does next, that could either be better or worse than ending
    // up with lost clusters (ie, if we deleted the old name first and *then*
    // the disk was yanked).  Hopefully, the user would run SCANDISK before
    // trying to delete one of the copies....
    //
    // Second, when the source and destination directories are the same
    // (a common occurrence), renames inside full or nearly full directories
    // can cause the directory to grow, even when the new name is equal to or
    // shorter than the old name.  That's less of a concern than the first
    // side-effect however. -JTP

#ifdef TFAT    
    // For TFAT, lock the FAT so that a SyncFATs operation cannot be done
    // by another thread in between the CreateName of the dst and the 
    // DestroyName of the src.
    if (pvol->v_fTfat)
        LockFAT (pvol);
#endif

    if (dwError = CreateName(pstmDst, shDst.sh_awcPattern, &diDst, pdeClone, bAttrSrc)) {

        // If we had to delete the original name to make way for the new
        // name, then we must try to recreate the original.  Whatever error
        // occurs is irrelevant (and hopefully, NO error will occur, or the
        // user is going to be upset with us -JTP).

        if (pdeClone == &deClone) {

            // Cross your fingers and handle "that rare case" now...

            shSrc.sh_pos = 0;
            shSrc.sh_flags = SHF_BYNAME | SHF_CREATE;
            if (FindNext(&shSrc, &diSrc, &fd) == ERROR_FILE_NOT_FOUND)
                CreateName(pstmSrc, shSrc.sh_awcPattern, &diSrc, pdeClone, bAttrSrc);
        }
#ifdef TFAT    
        if (pvol->v_fTfat)
            UnlockFAT (pvol);
#endif        
        goto exit1;
    }
    // TEST_BREAK
    PWR_BREAK_NOTIFY(32);

    // If the source still exists (ie, if pdeClone is still pointing
    // to the original source DIRENTRY), then it is time to destroy it.

    if (pdeClone == diSrc.di_pde) {

        // Hold the destination's current buffer in case there's an error
        // on the DestroyName (we want to guarantee we can clean up).

        PBUF pbufDst = pstmDst->s_pbufCur;
        HoldBuffer(pbufDst);

        // Before we can operate on the source again, we need to make sure
        // its current buffer is still assigned (ie, held).  If FindFirst
        // for the destination had to walk THROUGH the source stream, the
        // source's current buffer could have become unheld, or it might be
        // holding a different buffer now (that can happen when both source
        // and destination streams are the SAME).

        ReleaseStreamBuffer(pstmSrc, FALSE);
        AssignStreamBuffer(pstmSrc, pbufSrc, FALSE);

        // Ready to destroy the old name (if we haven't already destroyed it).
        // Zap clusEntry so it doesn't try to decommit the file's clusters too.

        diSrc.di_clusEntry = UNKNOWN_CLUSTER;
        if (dwError = DestroyName(pstmSrc, &diSrc)) {

            // An error occurred.  Destroy the new name, taking care to zap
            // clusEntry so it doesn't try to decommit the file's clusters too.
            // We deliberately ignore any error destroying the new name, because
            // we're already in an error recovery path.

            ReleaseStreamBuffer(pstmDst, FALSE);
            AssignStreamBuffer(pstmDst, pbufDst, FALSE);

            diDst.di_clusEntry = UNKNOWN_CLUSTER;
            DestroyName(pstmDst, &diDst);
        }
        UnholdBuffer(pbufDst);
    }


#ifdef TFAT    
    if (pvol->v_fTfat)
        UnlockFAT (pvol);
#endif


    // If a DIRENTRY for a directory moved to a different stream,
    // then we have to fix up the ".." entry inside that directory too.

    // Note that I now perform this step at the very *end* of MoveFile, instead
    // of earlier (like we did in previous versions, before deleting the original
    // name), and I drop any errors from this point onward on the floor, because we
    // don't have the code to completely undo everything we did above should this
    // step fail (BUGBUG: Leaves directory in inconsistent state -JTP)

    if (!dwError && (bAttrSrc & ATTR_DIR_MASK) == ATTR_DIRECTORY) {

        PDSTREAM pstm;
        DWORD dwFlags = OPENSTREAM_REFRESH;

#ifdef PATH_CACHING
        PathCacheInvalidate(pvol, pwsOldFileName);
#endif
        ASSERT(diDst.di_clusEntry != UNKNOWN_CLUSTER &&
               diDst.di_clusEntry != FAT_PSEUDO_CLUSTER &&
               diDst.di_clusEntry != ROOT_PSEUDO_CLUSTER);

        // If the directory that was just moved has an open stream,
        // then update its parent information
        UpdateSourceStream (pvol, &diSrc.di_sid, &diDst, pstmDst);

        // We always open the directory stream now, to refresh the stream
        // if it's currently open (or cached), and to create it if we also
        // need to change the ".." entry (ie, if the directory moved).  That's
        // why we check the condition (pstmSrc != pstmDst) *after* calling
        // OpenStream instead of *before*.  It may look inefficient, but there's
        // an intentional side-effect.

        if (pstmSrc != pstmDst)
            dwFlags |= OPENSTREAM_CREATE;

        if (pstm = OpenStream(pstmDst->s_pvol,
                              diDst.di_clusEntry,
                              &diDst.di_sid,
                              pstmDst, &diDst, dwFlags)) {

            if (pstmSrc != pstmDst) {
                PDIRENTRY pde;
#ifdef TFAT                
                if (pvol->v_fTfat)
                    DEBUGMSG (TRUE, (TEXT("FATFSD!FAT_MoveFileW: Moving folder to a different location is not transaction safe!!\r\n")));
#endif
                if (ReadStream(pstm, sizeof(DIRENTRY), &pde, NULL) == ERROR_SUCCESS) {
                    DWORD dw = ModifyStreamBuffer(pstm, pde, sizeof(DIRENTRY));
                    if (!dw) {
                        DWORD clusFirst = (ISROOTDIR(pstmDst)? NO_CLUSTER : pstmDst->s_clusFirst);
                        SETDIRENTRYCLUSTER(pstmDst->s_pvol, pde, clusFirst);
                        // TEST_BREAK
                        PWR_BREAK_NOTIFY(33);                        
                    }
                }
            }
            CloseStream(pstm);
        }
    }


    if (!(bAttrSrc & ATTR_DIR_MASK))  {
        shDst.sh_pos = 0;
        shDst.sh_flags = SHF_BYNAME | SHF_CREATE;
        dwError = FindNext(&shDst, &diDst, &fd);
        UpdateSourceStream (pvol, &diSrc.di_sid, &diDst, pstmDst);
    }
        

    memcpy (&sidDstParent, &pstmDst->s_sid, sizeof(DSID));
    memcpy (&sidSrcParent, &pstmSrc->s_sid, sizeof(DSID));
    
  exit1:
    CloseStream(pstmDst);

  exit2:
    UnholdBuffer(pbufSrc);

  exit3:
    CloseStream(pstmSrc);

    CloseRoot(pvol);

    if (dwError) {
      error:
        SetLastError(dwError);
    }
    else {

        FILESYSTEMNOTIFICATION(pvol,
                               DB_CEOID_CREATED,
                               bAttrSrc & ATTR_DIRECTORY? DB_CEOID_DIRECTORY_DELETED : DB_CEOID_FILE_DELETED,
                               bAttrSrc & ATTR_DIRECTORY? SHCNE_RENAMEFOLDER : SHCNE_RENAMEITEM,
                               &diDst.di_sid,
                               &sidDstParent,
                               &diSrc.di_sid,
                               &sidSrcParent,
                               &oiOld,
                               DBGTEXTW("FAT_MoveFileW"));

#ifdef TFAT
        if (pvol->v_fTfat)
            dwError = CommitTransactions (pvol);
#endif
    }
    
    FATExit(pvol, LOGID_MOVEFILE);

    DEBUGMSGW(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXTW("FATFS!FAT_MoveFileW(%s->%s) returned 0x%x (%d)\r\n"), pwsOldFileName, pwsNewFileName, dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}


BOOL FAT_RegisterFileSystemNotification(PVOLUME pvol, HWND hwnd)
{
    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_RegisterFileSystemNotification(0x%x)\r\n"), hwnd));

#ifdef SHELL_MESSAGE_NOTIFICATION
    hwndShellNotify = hwnd;
    return TRUE;
#else
    return FALSE;
#endif
}


BOOL FAT_RegisterFileSystemFunction(PVOLUME pvol, SHELLFILECHANGEFUNC_t pfn)
{
    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_RegisterFileSystemFunction(0x%x)\r\n"), pfn));

#ifdef SHELL_CALLBACK_NOTIFICATION
    pfnShell = pfn;
    return TRUE;
#else
    return FALSE;
#endif
}


/*  FAT_OidGetInfo - Get file information by OID
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      oid - OID of interest
 *      poi - pointer to OID information to return
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FAT_OidGetInfo(PVOLUME pvol, CEOID oid, CEOIDINFO *poi)
{
    DWORD dwError = ERROR_INVALID_FUNCTION;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_OidGetInfo(0x%x)\r\n"), oid));

    if (!FATEnter(NULL, LOGID_NONE))
        return FALSE;

#ifndef DISABLE_OIDS
    if (pvol->v_flags & (VOLF_INVALID | VOLF_UNMOUNTED | VOLF_FROZEN | VOLF_WRITELOCKED))
        dwError = ERROR_ACCESS_DENIED;
    else if (AFSFROMOID(oid) != (DWORD)(int)pvol->v_volID)
        dwError = ERROR_INVALID_PARAMETER;
    else {
        DSID sid;
        SETSIDFROMOID(&sid, oid);
        dwError = GetSIDInfo(pvol, &sid, poi);
    }
    if (!dwError) {
        FATExit(pvol, LOGID_NONE);
        DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_OidGetInfo returned TRUE (0x%x,0x%x,0x%x,%d chars: %s)\r\n"), poi->wObjType, poi->infFile.dwAttributes, poi->infFile.oidParent, wcslen(poi->infFile.szFileName), poi->infFile.szFileName));
        return TRUE;
    }
#endif

    SetLastError(dwError);
    FATExit(pvol, LOGID_NONE);
    DEBUGMSG(ZONE_APIS || ZONE_ERRORS,(DBGTEXT("FATFS!FAT_OidGetInfo(0x%x) returned FALSE (%d)\r\n"), oid, dwError));
    return FALSE;
}


BOOL FAT_PrestoChangoFileName(PVOLUME pvol, PCWSTR pwsOldFile, PCWSTR pwsNewFile)
{
    BOOL fSuccess;

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_PrestoChangoFileName(%s<-%s)\r\n"), pwsOldFile, pwsNewFile));

    if (pvol->v_flags & (VOLF_INVALID | VOLF_UNMOUNTED | VOLF_FROZEN | VOLF_LOCKED)) {
        SetLastError(ERROR_ACCESS_DENIED);
        fSuccess = FALSE;
    }
    else {

        // BUGBUG: For now, implement this the simple and obvious way, using
        // existing APIs.  Determine later if this is sufficient.  It would be preferable,
        // for example, to first rename the old file to a unique temporary name, so that
        // we could rename it back to its original name if the MoveFile failed.  Logging
        // doesn't save us, because the log isn't maintained across complete operations like
        // these. -JTP

        if (fSuccess = FAT_DeleteFileW(pvol, pwsOldFile))
            fSuccess = FAT_MoveFileW(pvol, pwsNewFile, pwsOldFile);
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && !fSuccess,(DBGTEXT("FATFS!FAT_PrestoChangoFileName returned 0x%x (%d)\r\n"), fSuccess, fSuccess? 0 : GetLastError()));
    return fSuccess;
}

BOOL CalculateFreeClustersInRAM(PVOLUME pvol)
{
    LPBYTE pFATBuffer = NULL;
    LPBYTE pCurEntry = NULL;
    DWORD dwClus;    
    DWORD dwFATSize = pvol->v_csecFAT << pvol->v_log2cbSec;
    BOOL fFAT16 = ((pvol->v_flags & VOLF_16BIT_FAT) != 0);
    DWORD cbFATEntry;
    DWORD dwBufferSize;
    DWORD dwSectorsRead = 0;
    BOOL fRet = FALSE;
    
    // Don't do this for FAT12 because the FAT is small anyway.
    if (pvol->v_flags & VOLF_12BIT_FAT) {
        return FALSE;
    }

    // Check for overflow of dwFATSize
    if ((0xffffffff >> pvol->v_log2cbSec) < pvol->v_csecFAT) {
        return FALSE;
    }

    cbFATEntry = (fFAT16) ? sizeof(WORD) : sizeof(DWORD);

    // Check for invalid FAT size 
    if (dwFATSize / cbFATEntry < pvol->v_clusMax) {
        ASSERT(0);
        return FALSE;
    }

    dwBufferSize = min (dwFATSize, MAX_FREE_SPACE_CHUNK_SIZE);

    pFATBuffer = (LPBYTE)VirtualAlloc (NULL, dwBufferSize, MEM_COMMIT, PAGE_READWRITE);
    if (!pFATBuffer) {
        goto exit;
    }

    // Ensure that the free list size is a multiple of a DWORD
    // Failed allocation means that optimization is not used
    pvol->v_clusterListSize = (pvol->v_clusMax + CLUSTERS_PER_LIST_ENTRY - 1) / CLUSTERS_PER_LIST_ENTRY;
    pvol->v_clusterListSize = (pvol->v_clusterListSize + sizeof(DWORD) - 1) & ~(sizeof(DWORD) - 1);
    pvol->v_pFreeClusterList = (LPBYTE)HeapAlloc (hHeap, HEAP_ZERO_MEMORY, pvol->v_clusterListSize);

    dwSectorsRead = dwBufferSize >> pvol->v_log2cbSec;
    if (ReadVolume (pvol, 0, dwSectorsRead, pFATBuffer) != ERROR_SUCCESS) {
        goto exit;
    }

    // Start looking at the DATA_CLUSTER
    pCurEntry = pFATBuffer + cbFATEntry * DATA_CLUSTER;

    pvol->v_cclusFree = 0;

    for (dwClus = DATA_CLUSTER; dwClus <= pvol->v_clusMax;  dwClus++) {

        if ((DWORD)(pCurEntry - pFATBuffer) >= dwBufferSize) {

            // We've reached the end of the buffer, read the next chunk of data
            DWORD dwSectorsToRead = min (pvol->v_csecFAT - dwSectorsRead, (DWORD)MAX_FREE_SPACE_CHUNK_SIZE >> pvol->v_log2cbSec);

            if (ReadVolume (pvol, dwSectorsRead, dwSectorsToRead, pFATBuffer) != ERROR_SUCCESS) {
                goto exit;
            }            
            pCurEntry = pFATBuffer;  
            dwSectorsRead += dwSectorsToRead;
        }

        if (fFAT16) {
            if ((*(PWORD)pCurEntry) == FREE_CLUSTER) {
                IncrementFreeClusterCount(pvol, dwClus);
            }
        } else {
            if ((*(PDWORD)pCurEntry) == FREE_CLUSTER) {
                IncrementFreeClusterCount(pvol, dwClus);
            }
        }

        pCurEntry += cbFATEntry;
    }

    fRet = TRUE;

exit:    
    if (pFATBuffer) {
        VirtualFree (pFATBuffer, 0, MEM_RELEASE);
    }
    return fRet;
}

DWORD CalculateFreeClustersFromBuffers(PVOLUME pvol)
{
    DWORD dwClus, dwData;
    DWORD dwError = ERROR_SUCCESS;

    // BUGBUG: it would be faster to get a pointer to each FAT
    // buffer and walk through it ourselves, but this is smaller and more
    // expedient. -JTP

    pvol->v_cclusFree = 0;
    for (dwClus=DATA_CLUSTER; dwClus <= pvol->v_clusMax;  dwClus++) {
        dwError = UNPACK(pvol, dwClus, &dwData);
        if (dwError)
            break;
        if (dwData == FREE_CLUSTER)
            pvol->v_cclusFree++;
    }

    return dwError;
}

BOOL FAT_GetDiskFreeSpaceW(
PVOLUME pvol,
PCWSTR pwsPathName,
PDWORD pSectorsPerCluster,
PDWORD pBytesPerSector,
PDWORD pFreeClusters,
PDWORD pClusters)
{
    BOOL fSuccess = FALSE;
    DWORD dwError = ERROR_SUCCESS;

    // BUGBUG: Currently, there is no validation of pswPathName.  If
    // the file system routed this call to us, the first part of the path
    // must be OK, so we'll assume the entire thing is OK too (for now). -JTP

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_GetDiskFreeSpace(%s)\r\n"), pwsPathName? pwsPathName : TEXTW("")));

    LockFAT(pvol);

    if (pvol->v_flags & (VOLF_INVALID | VOLF_UNMOUNTED | VOLF_FROZEN | VOLF_WRITELOCKED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    // We enter the FAT's critical section even though we're just reading
    // the FAT, because in process of reading, we could alter the FAT stream's
    // current buffer, and that could mess up someone else accessing the FAT.

    if (pvol->v_cclusFree == UNKNOWN_CLUSTER) {

        // We currently call FATEnter/FATExit just at this point,
        // because it's the only path in this call that might generate any I/O.

        if (!FATEnter(NULL, LOGID_NONE))
            goto exit;
    
        if (!CalculateFreeClustersInRAM(pvol)) {
            // If we couldn't do it in RAM, try it through the FAT buffers.
            dwError = CalculateFreeClustersFromBuffers(pvol);
        }
        
        FATExit(pvol, LOGID_NONE);
    }

    __try {
        if (!dwError) {
            if (pSectorsPerCluster)
                *pSectorsPerCluster = (1 << pvol->v_log2csecClus);
            if (pBytesPerSector)
                *pBytesPerSector = (1 << pvol->v_log2cbSec);
            if (pFreeClusters)
                *pFreeClusters = pvol->v_cclusFree;            
            if (pClusters)
                *pClusters = pvol->v_clusMax-1;
            fSuccess = TRUE;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }

  exit:

    UnlockFAT(pvol);

    DEBUGMSG(ZONE_APIS || ZONE_CLUSTERS || ZONE_ERRORS && !fSuccess,(DBGTEXT("FATFS!FAT_GetDiskFreeSpace returned 0x%x (%d), 0x%x clusters free\r\n"), fSuccess, dwError, pvol->v_cclusFree));

    if (dwError)
        SetLastError(dwError);

    return fSuccess;
}



/*  GetSIDInfo - Get file information by SID (worker for FAT_OidGetInfo)
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      psid - pointer to SID of interest
 *      poi - pointer to OID information to return
 *
 *  EXIT
 *      Error code (ERROR_SUCCESS if none).  This code was removed from FAT_OidGetInfo
 *      and put into this worker function, because we cannot tolerate FAT_* entry points
 *      calling other FAT_* entry points (like FAT_OidGetInfo!), because then we can
 *      get into situations where an outer call is blocked by an inner call that is in
 *      turn blocked in FATEnter (because power just went off or something....)
 *
 *      That particular case wasn't a problem in v1.0, because the shell notification
 *      mechanism was SHELL_MESSAGE_NOTIFICATION, not SHELL_CALLBACK_NOTIFICATION;  the latter
 *      now requires us to provide information while we're still inside the FAT APIs.
 */

DWORD GetSIDInfo(PVOLUME pvol, PDSID psid, CEOIDINFO *poi)
{
    SHANDLE sh;
    PWSTR pwsCur;
    DWORD len, dwError;
    BYTE fSearch;
    DWORD dwSearch;
    DWORD dwNext;
    DWORD oidParent;
    DWORD clusParent;
    DIRINFO di;
    WIN32_FIND_DATAW fd;

    fSearch = SHF_BYORD;
    dwSearch = psid->sid_ordDir;
    oidParent = 0;
    clusParent = psid->sid_clusDir;

    // Assume the OID is valid
    dwError = ERROR_SUCCESS;

    if (psid->sid_clusDir == UNKNOWN_CLUSTER)
        dwError = ERROR_INVALID_PARAMETER;

    // Find/create a stream for the object's directory

    // NOTE: this form of OpenStream doesn't initialize the stream's name,
    // attr, size, parent OID, or path length.  However, lower level functions
    // we call, like FindNext (which calls ReadStream), don't depend on that
    // information, and since we're creating the stream as a PRIVATE stream, no
    // one else will either.

    while (!dwError && clusParent && (sh.sh_pstm = OpenStream(pvol, clusParent, NULL, NULL, NULL, OPENSTREAM_CREATE|OPENSTREAM_PRIVATE))) {

        __try {

            dwNext = clusParent;
            dwError = ERROR_SUCCESS;

            // Prepare to build the full name from the top down

            // Find the object's DIRENTRY within that stream now.
            // But first, since the directory's "." and ".." entries
            // should appear first, let's look for them first.  This
            // will reduce the amount of oscillation within the stream.

            if (!ISROOTDIR(sh.sh_pstm)) {

                sh.sh_pos = 0;
                sh.sh_flags = SHF_BYNAME | SHF_DOTDOT;
                sh.sh_cwPattern = ARRAYSIZE(szDotDot)-1;
                memcpy(sh.sh_awcPattern, szDotDot, sizeof(szDotDot));

                dwError = FindNext(&sh, &di, &fd);
                if (!dwError) {

                    // Because we supplied a DIRINFO to FindNext,
                    // the stream buffer did NOT automatically get unheld.

                    ReleaseStreamBuffer(sh.sh_pstm, FALSE);

                    clusParent = di.di_clusEntry;

                    // The absence of a cluster (on non-FAT32 volumes anyway)
                    // means that this directory's parent is the ROOT.

                    if (di.di_clusEntry == UNKNOWN_CLUSTER)
                        clusParent = pvol->v_pstmRoot->s_clusFirst;
                }
                else {
                    DEBUGMSG(ZONE_ERRORS,(TEXT("FATFS!GetSIDInfo: FindNext(%x) for clusParent 0x%x returned %d\r\n"), sh.sh_flags, dwNext, dwError));

                    // FindNext already set dwError to something, so let's pass it on;
                    // this is more important for "driver errors", like ERROR_GEN_FAILURE,
                    // because those errors will insure that any lengthy scan in-progress
                    // will get cancelled instead of continuing (see ScanDirectory in scan.c)

                    // dwError = ERROR_INVALID_PARAMETER;
                    goto close;
                }
            }
            else
                clusParent = NO_CLUSTER;

            // OK, now we can go look for the object's DIRENTRY

            sh.sh_pos = 0;
            sh.sh_flags = fSearch;
            sh.sh_h = (HANDLE)dwSearch;

            dwError = FindNext(&sh, NULL, &fd);
            if (!dwError) {

                // The first DIRENTRY search is BYORD because that's
                // all the OID tells us.  Subsequent DIRENTRY searches are
                // BYCLUS because that's all the ".." entries tell us.

                // The first (BYORD) search is also the one that gives us
                // the attribute, size, etc info we need, so record that now.

                if (fSearch == SHF_BYORD) {
                    poi->infFile.dwAttributes = fd.dwFileAttributes;
                    poi->infFile.ftLastChanged = fd.ftLastWriteTime;
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        poi->wObjType = OBJTYPE_DIRECTORY;
                    else {
                        poi->wObjType = OBJTYPE_FILE;
                        poi->infFile.dwLength = fd.nFileSizeLow;
                    }
                    pwsCur = &poi->infFile.szFileName[MAX_PATH-1];
                    *pwsCur = 0;
                }

                // If this is the first BYCLUS search (ie, if oidParent
                // is still zero), then record this object as the parent.

                else if (oidParent == 0)
#ifdef UNDER_CE
                    // NT does not have OID defined                            
                    oidParent = fd.dwOID;
#endif

                // fd.cFileName has the name from the object's DIRENTRY
                // Ignore the hidden root directory.
                if (wcscmp(fd.cFileName, HIDDEN_TFAT_ROOT_DIR) != 0) {
                    len = wcslen(fd.cFileName);
                    pwsCur -= len;
                }

                if (pwsCur > poi->infFile.szFileName) {

                    if (wcscmp(fd.cFileName, HIDDEN_TFAT_ROOT_DIR) != 0) {
                        memcpy(pwsCur, fd.cFileName, len*sizeof(WCHAR));
                        *--pwsCur = TEXTW('\\');
                    }
                    if (clusParent) {
                        // Get ready for next search, since this one is done now
                        fSearch = SHF_BYCLUS;
                        dwSearch = dwNext;
                    }
                    else if (pvol->v_pwsHostRoot) {
                        pwsCur -= pvol->v_cwsHostRoot;
                        if (pwsCur >= poi->infFile.szFileName)
                            memcpy(pwsCur, pvol->v_pwsHostRoot, pvol->v_cwsHostRoot*sizeof(WCHAR));
                        else
                            dwError = ERROR_BUFFER_OVERFLOW;
                    }
                }
                else
                    dwError = ERROR_BUFFER_OVERFLOW;
            }
            else {
                DEBUGMSG(ZONE_ERRORS,(TEXT("FATFS!GetSIDInfo: FindNext(%x) for clusParent 0x%x returned %d\r\n"), sh.sh_flags, dwNext, dwError));
                dwError = ERROR_INVALID_PARAMETER;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            dwError = ERROR_INVALID_PARAMETER;
        }

      close:
        CloseStream(sh.sh_pstm);

        if (dwError)
            break;
    }

    if (!dwError && !sh.sh_pstm) {
        dwError = ERROR_BAD_UNIT;  // device must have been removed
    }

    if (!dwError) {

        if (oidParent == 0)
            oidParent = OIDFROMSID(pvol, NULL);

        __try {
            poi->infFile.oidParent = oidParent;

            // Slide the completed filename down (if it isn't already)

            if (pwsCur != poi->infFile.szFileName)
                memcpy(poi->infFile.szFileName, pwsCur, (wcslen(pwsCur)+1)*sizeof(WCHAR));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            dwError = ERROR_INVALID_PARAMETER;
        }
    }
    return dwError;
}


#ifdef SHELL_MESSAGE_NOTIFICATION

#ifndef DEBUG
void PostFileSystemMessage(PVOLUME pvol, UINT uMsg, PDSID psid, PDSID psidParent)
#else
void PostFileSystemMessage(PVOLUME pvol, UINT uMsg, PDSID psid, PDSID psidParent, PWSTR pwsCaller)
#endif
{
    if (hwndShellNotify && ZONE_SHELLMSGS) {
        PostMessage(hwndShellNotify, uMsg, OIDFROMSID(pvol,psid), OIDFROMSID(pvol,psidParent));
        DEBUGMSGW(ZONE_MSGS,(DBGTEXTW("FATFS!%s posted(0x%08x,0x%08x,0x%08x)\r\n"), pwsCaller, uMsg, OIDFROMSID(pvol,psid), OIDFROMSID(pvol,psidParent)));
    }
}

#endif  // SHELL_MESSAGE_NOTIFICATION


#ifdef SHELL_CALLBACK_NOTIFICATION

#ifndef DEBUG
void CallFileSystemFunction(PVOLUME pvol, DWORD dwSHCNE, PDSID psid, PDSID psidOld, CEOIDINFO *poiOld)
#else
void CallFileSystemFunction(PVOLUME pvol, DWORD dwSHCNE, PDSID psid, PDSID psidOld, CEOIDINFO *poiOld, PWSTR pwsCaller)
#endif
{
    if (pfnShell && ZONE_SHELLMSGS) {

        DWORD dwError;
        CEOIDINFO oi;
        FILECHANGEINFO fci;

        memset(&oi, 0, sizeof(oi));

        DEBUGMSG(ZONE_ERRORS && psid == NULL && psidOld == NULL,(DBGTEXT("FATFS!CallFileSystemFunction(0x%08x,0x%08x): called with invalid PDSIDs\r\n"), pvol, dwSHCNE));

        if (psid && (dwError = GetSIDInfo(pvol, psid, &oi))) {
            DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!CallFileSystemFunction(0x%08x,0x%08x): GetSIDInfo unexpectedly failed (%d)\r\n"), pvol, dwSHCNE, dwError));
            return;
        }

        fci.cbSize = sizeof(fci);
        fci.wEventId = dwSHCNE;
        fci.uFlags = SHCNF_PATH | SHCNF_FLUSHNOWAIT;
        fci.dwItem1 = psid? (DWORD)&oi.infFile.szFileName : 0;
        fci.dwItem2 = 0;
        fci.dwAttributes = oi.infFile.dwAttributes;
        fci.ftModified = oi.infFile.ftLastChanged;
        fci.nFileSize = oi.infFile.dwLength;
        if (poiOld) {
            fci.dwItem2 = fci.dwItem1;
            fci.dwItem1 = psidOld? (DWORD)&poiOld->infFile.szFileName : 0;
        }

        __try {
            (*pfnShell)(&fci);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!CallFileSystemFunction(0x%08x,0x%08x): 0x%08x blew up\r\n"), pvol, dwSHCNE, pfnShell));
            return;
        }

        DEBUGMSGW(ZONE_MSGS,(DBGTEXTW("FATFS!%s notified function 0x%08x(0x%08x,\"%s\",\"%s\")\r\n"), pwsCaller, pfnShell, dwSHCNE, fci.dwItem1, fci.dwItem2));
    }
}

#endif  // SHELL_CALLBACK_NOTIFICATION
