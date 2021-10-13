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

    file.c

Abstract:

    This file contains the handle based api routines for the FAT file system.

Revision History:

--*/

#include "fatfs.h"



#define KEEP_SYSCALLS
#define NO_DEFINE_KCALL

HANDLE FAT_CreateFileW(
PVOLUME pvol,
HANDLE hProc,
LPCWSTR lpFileName,
DWORD dwAccess,
DWORD dwShareMode,
LPSECURITY_ATTRIBUTES lpSecurityAttributes,
DWORD dwCreate,
DWORD dwFlagsAndAttributes,
HANDLE hTemplateFile)
{
    BYTE mode;
    int flName;
    HANDLE hFile;
    PFHANDLE pfh = NULL;
    PDSTREAM pstm = NULL;
    DWORD dwError = ERROR_SUCCESS;
    BOOL bExists = FALSE;
    DWORD dwSHCNE = 0;

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_CreateFileW(%d chars: %s)\n"), wcslen(lpFileName), lpFileName));

    if (!FATEnter(pvol, LOGID_CREATEFILE)) {
        DEBUGMSG(ZONE_APIS || ZONE_ERRORS,(DBGTEXT("FATFS!FAT_CreateFileW bailing...\n")));
        return INVALID_HANDLE_VALUE;
    }

    if (pvol->v_flags & (VOLF_UNMOUNTED | VOLF_FROZEN | VOLF_WRITELOCKED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    if (dwAccess & ~(GENERIC_READ | GENERIC_WRITE))
        goto invalidParm;

    if ((BYTE)dwFlagsAndAttributes == FILE_ATTRIBUTE_NORMAL)
        dwFlagsAndAttributes &= ~0xFF;
    else {
        dwFlagsAndAttributes &= ~FILE_ATTRIBUTE_NORMAL;
        if ((BYTE)dwFlagsAndAttributes & ~ATTR_CHANGEABLE)
            goto invalidParm;
    }

    dwFlagsAndAttributes |= FILE_ATTRIBUTE_ARCHIVE;

    if (dwShareMode & ~(FILE_SHARE_READ | FILE_SHARE_WRITE))
        goto invalidParm;

    // Validate creation flags and set flName for OpenName;  we allow
    // only files and volumes to be opened with this call (NOT directories).

    flName = NAME_FILE | NAME_VOLUME;

    switch (dwCreate) {

    case CREATE_NEW:
        flName |= NAME_NEW;
        // Fall into the CREATE_ALWAYS case now...

    case CREATE_ALWAYS:
        flName |= NAME_CREATE | NAME_TRUNCATE;

        // We don't simply fall into TRUNCATE_EXISTING, because
        // on Win95 at least, you're allowed to create a file without
        // specifying GENERIC_WRITE.  It makes the resulting handle
        // less than useful, but that's the way it works. -JTP

        break;

    case TRUNCATE_EXISTING:
        if (!(dwAccess & GENERIC_WRITE)) {
            dwError = ERROR_ACCESS_DENIED;
            goto exit;
        }
        flName |= NAME_TRUNCATE;
        break;

    case OPEN_ALWAYS:
        flName |= NAME_CREATE;
        // Fall into the OPEN_EXISTING case now...

    case OPEN_EXISTING:
        break;

    default:
    invalidParm:
        dwError = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    // If the volume is locked such that only read access is currently allowed
    // (eg, scan in progress), then deny all requests that could modify the volume's
    // state.  Note that if the call wouldn't have succeeded anyway (for example,
    // the caller specified CREATE_NEW but the file already exists), that error
    // will be masked by this error for the duration of the read-lock;  but handling
    // the lock this way is much safer/simpler, and few if any callers will even
    // really care. -JTP

    if ((pvol->v_flags & VOLF_READLOCKED) &&
        ((flName & (NAME_CREATE | NAME_TRUNCATE)) || (dwAccess & GENERIC_WRITE))) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    mode = (BYTE)(ShareToMode(dwShareMode) | AccessToMode(dwAccess));
    flName |= (BYTE)(dwFlagsAndAttributes) | (mode << NAME_MODE_SHIFT);

    pstm = OpenName((PDSTREAM)pvol, lpFileName, 0, &flName);
    if (!pstm) {
        DEBUGONLY(dwError = GetLastError());
        goto abort;
    }

    // Save the fact that we might have a successfull stream creation on a file that already exists.
    // OpenName sets the error and we save off the fact that it did.
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        bExists = TRUE;
    }    

    // OpenName will return the root stream IFF a special volume
    // name was specified.  Which means that if the stream is NOT the
    // root, then this is NOT a volume handle.

    if (pstm != pstm->s_pvol->v_pstmRoot) {
        flName &= ~NAME_VOLUME;
        if (!CheckStreamSharing(pstm, mode)) {
            dwError = ERROR_SHARING_VIOLATION;
            goto exit;
        }
    }

#ifdef DEMAND_PAGING_DEADLOCKS
    // If the stream we've opened can currently be demand-paged,
    // then we MUST temporarily release the stream's CS around these
    // memory manager calls.

    if (pstm->s_flags & STF_DEMANDPAGED)
        LeaveCriticalSection(&pstm->s_cs);
#endif

    pfh = LocalAlloc(LPTR, sizeof(FHANDLE));
    if (pfh) {
        if (!(hFile = FSDMGR_CreateFileHandle( pvol->v_hVol, hProc, (PFILE)pfh))) {
            DEBUGFREE(sizeof(FHANDLE));
            VERIFYNULL(LocalFree(pfh));
            pfh = NULL;
        }
    }
    if (!pfh)
        dwError = ERROR_OUTOFMEMORY;

#ifdef DEMAND_PAGING_DEADLOCKS
    if (pstm->s_flags & STF_DEMANDPAGED)
        EnterCriticalSection(&pstm->s_cs);
#endif

    if (dwError)
        goto exit;


    pfh->fh_h = hFile;
    pfh->fh_hProc = hProc;

    pfh->fh_flags |= FHF_FHANDLE;
    pfh->fh_mode = mode;
    pfh->fh_pstm = pstm;
    AddItem((PDLINK)&pstm->s_dlOpenHandles, (PDLINK)&pfh->fh_dlOpenHandles);

    




    if ((dwFlagsAndAttributes & FILE_FLAG_WRITE_THROUGH) || (pvol->v_flFATFS & FATFS_FORCE_WRITETHROUGH))
        pstm->s_flags |= STF_WRITETHRU;

#ifdef TFAT    
    if (pstm->s_pstmParent)
        LeaveCriticalSection(&pstm->s_cs);
#endif
    
    if (flName & NAME_VOLUME)
    {
        pfh->fh_flags |= FHF_VOLUME;
    }
    else if (flName & NAME_CREATED)
    {
        dwSHCNE = SHCNE_CREATE;
    }
    else if (flName & NAME_TRUNCATE)
    {

#ifdef TFAT
	// TFAT, now that we plan to shrink the file, we may clone the dir
	//	so enter the parent CS first before re-entering its own CS
        if(pstm->s_pstmParent) {
            EnterCriticalSection(&pstm->s_pstmParent->s_cs);
            EnterCriticalSection(&pstm->s_cs);
        }
#endif
        

        if (dwError = ResizeStream(pstm, 0, RESIZESTREAM_SHRINK|RESIZESTREAM_UPDATEFAT))
        {
#ifdef TFAT        
            // pstm->s_cs will be left on exit
            if(pstm->s_pstmParent)
                LeaveCriticalSection(&pstm->s_pstmParent->s_cs);
#endif
            goto exit;
        }
        
        // On NT and Win95, when an existing file is truncated, the attributes are
        // updated to match those passed to CreateFile (and yes, if no truncation was
        // required, then the existing file's attributes are preserved, so this really
        // is the right place to propagate attributes).  Also note that we don't need to 
        // explicitly mark the stream dirty, because ResizeStream already did that.

        //Need to force attributes to be updated on new/truncated file
        pstm->s_flags |= STF_DIRTY;
        pstm->s_attr = (BYTE)dwFlagsAndAttributes;

        





        CommitStream(pstm, FALSE);

#ifdef TFAT
        if (pvol->v_fTfat && (STF_WRITETHRU & pstm->s_flags))
        {
            // stream modified, commit transations if in write-through mode
            dwError = CommitTransactions (pstm->s_pvol);
        }
#endif        

        dwSHCNE = SHCNE_UPDATEITEM;

#ifdef TFAT
        if(pstm->s_pstmParent) {
            LeaveCriticalSection(&pstm->s_cs);
            LeaveCriticalSection(&pstm->s_pstmParent->s_cs);
        }
#endif
    }
    
#ifdef TFAT    
    if (!pstm->s_pstmParent)
#endif
        LeaveCriticalSection(&pstm->s_cs);


    // Don't send notification for special file handles (like VOL:)
    if (dwSHCNE && pstm->s_sid.sid_clusDir)
        FILESYSTEMNOTIFICATION(pvol, DB_CEOID_CHANGED, 0, dwSHCNE, &pstm->s_sid, &pstm->s_sidParent, NULL, NULL, NULL, DBGTEXTW("FAT_CreateFileW"));

  exit:
    if (dwError) {

        // If we got as far as creating a file handle, then leave the
        // stream's critical section and let FAT_CloseFile do all the cleanup
        // when it gets called (via the CloseFileHandle macro, which calls CloseHandle).

        // Otherwise, we just need to close the stream and return.  CloseStream
        // will take care of leaving the stream's critical section and freeing the
        // stream if necessary.
        
        if (pfh) {
            LeaveCriticalSection(&pstm->s_cs);
            pstm = NULL;
            CloseFileHandle(pfh);
            pfh = NULL;
        }

        if (pstm)
            CloseStream(pstm);

        SetLastError(dwError);
    } else {
        // On a succesfull creation on a file that already existed ..set the error 
        if (bExists)
            SetLastError(ERROR_ALREADY_EXISTS);
    }

  abort:
    FATExit(LOGID_CREATEFILE);

    DEBUGMSGW(ZONE_APIS || ZONE_ERRORS && !pfh, (DBGTEXTW("FATFS!FAT_CreateFileW(%-.64s) returned 0x%x (%d)\n"), lpFileName, pfh, dwError));

    return pfh? hFile : INVALID_HANDLE_VALUE;
}


BOOL FAT_CloseFile(PFHANDLE pfh)
{
    PDSTREAM pstm;
    PVOLUME  pvol;
#ifdef TFAT_TIMING_TEST
    DWORD dwTickCnt=GetTickCount();
#endif
    DSID sid, sidParent;

#ifdef TFAT
    PDSTREAM pstmParent;
#endif

    if (!BufEnter(TRUE))
        return FALSE;

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    pvol = pstm->s_pvol;
    ASSERT(pvol);

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_CloseFile(0x%x, %.11hs: %d)\n"), pfh, pfh->fh_pstm->s_achOEM, pfh->fh_pstm->s_clusFirst));

    if (pfh->fh_flags & FHF_LOCKED)
        UnlockVolume(pvol);

#ifdef TFAT
    // TFAT, if it was open for write, we may need to clone the parent dir
    pstmParent = pstm->s_pstmParent;
    if(pvol->v_fTfat && pstmParent && (pfh->fh_mode & FH_MODE_WRITE))
    {
        EnterCriticalSection(&pvol->v_csStms);
        pstmParent->s_refs++;	// prevent the parent stream to be closed
        LeaveCriticalSection(&pvol->v_csStms);

        EnterCriticalSection(&pstmParent->s_cs);
    }
#endif

    EnterCriticalSection(&pstm->s_cs);
    RemoveItem((PDLINK)&pfh->fh_dlOpenHandles);

#ifndef TFAT
    CommitBufferSet( (PDSTREAM)pvol, -1);
#endif

    memcpy (&sid, &pstm->s_sid, sizeof(DSID));
    memcpy (&sidParent, &pstm->s_sidParent, sizeof(DSID));

    CloseStream(pstm);

#ifdef TFAT
    if(pvol->v_fTfat && pstmParent && (pfh->fh_mode & FH_MODE_WRITE))
    {
        CloseStream(pstmParent);
    }

    // Sync FAT only if a file was opened for write
    if (pvol->v_fTfat && (pfh->fh_mode & FH_MODE_WRITE))
    {
#ifdef TFAT_TIMING_TEST
		RETAILMSG(1, (L"FAT_CloseFile takes %ld\n", GetTickCount()-dwTickCnt));
#endif
        CommitTransactions (pvol);
    }
    else
        CommitBufferSet( (PDSTREAM)pvol, -1);
#endif

    // Don't send notification for special file handles (like VOL:)
    if (sid.sid_clusDir && (pfh->fh_mode & FH_MODE_WRITE))
        FILESYSTEMNOTIFICATION(pvol, DB_CEOID_CHANGED, 0, SHCNE_UPDATEITEM, &sid, &sidParent, NULL, NULL, NULL, DBGTEXTW("CloseFile"));

    DEBUGFREE(sizeof(FHANDLE));
    VERIFYNULL(LocalFree(pfh));

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_CloseFile returned TRUE (0)\n")));

    BufExit();
    return TRUE;
}


DWORD FATFSReadFile(
PFHANDLE pfh,
LPVOID buffer,
DWORD nBytesToRead,
LPDWORD lpNumBytesRead,
LPOVERLAPPED lpOverlapped,
LPDWORD lpdwLowOffset,
LPDWORD lpdwHighOffset)
{
    PDSTREAM pstm;
    DWORD dwError;

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

#ifdef TFAT
    if(pstm->s_pvol->v_fTfat && pstm->s_pstmParent)
        EnterCriticalSection(&pstm->s_pstmParent->s_cs);
#endif

    EnterCriticalSection(&pstm->s_cs);
    __try {

        *lpNumBytesRead = 0;

        if (!(pfh->fh_mode & FH_MODE_READ) || (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)))
            dwError = ERROR_ACCESS_DENIED;
        else {
            
            if (!lpdwLowOffset) {
                dwError = ReadStreamData(pstm, pfh->fh_pos, buffer, nBytesToRead, lpNumBytesRead);
                pfh->fh_pos += *lpNumBytesRead;
            }
            else {
                dwError = ReadStreamData(pstm, *lpdwLowOffset, buffer, nBytesToRead, lpNumBytesRead);
            }
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }
    LeaveCriticalSection(&pstm->s_cs);

#ifdef TFAT
    if(pstm->s_pvol->v_fTfat && pstm->s_pstmParent)
        LeaveCriticalSection(&pstm->s_pstmParent->s_cs);
#endif

    return dwError;
}


DWORD FATFSWriteFile(
PFHANDLE pfh,
LPCVOID buffer,
DWORD nBytesToWrite,
LPDWORD lpNumBytesWritten,
LPOVERLAPPED lpOverlapped,
LPDWORD lpdwLowOffset,
LPDWORD lpdwHighOffset)
{
    PDSTREAM pstm;
    DWORD dwError;

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

#ifdef TFAT
    if(pstm->s_pstmParent)
        EnterCriticalSection(&pstm->s_pstmParent->s_cs);
#endif

    EnterCriticalSection(&pstm->s_cs);
    __try {

        *lpNumBytesWritten = 0;

        if (!(pfh->fh_mode & FH_MODE_WRITE) || (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)))
            dwError = ERROR_ACCESS_DENIED;
        else {
            
            if (!lpdwLowOffset) {
                dwError = WriteStreamData(pstm, pfh->fh_pos, buffer, nBytesToWrite, lpNumBytesWritten, TRUE);
                pfh->fh_pos += *lpNumBytesWritten;
            }
            else {
                dwError = WriteStreamData(pstm, *lpdwLowOffset, buffer, nBytesToWrite, lpNumBytesWritten, TRUE);
            }
            // TEST_BREAK
            PWR_BREAK_NOTIFY(11);
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }
    LeaveCriticalSection(&pstm->s_cs);

#ifdef TFAT
    if(pstm->s_pstmParent)
        LeaveCriticalSection(&pstm->s_pstmParent->s_cs);
#endif

    return dwError;
}


#if NUM_FILE_APIS == 13











































































































BOOL FAT_ReadFilePagein(
PFHANDLE pfh,
LPVOID buffer,
DWORD nBytesToRead,
LPDWORD lpNumBytesRead,
LPOVERLAPPED lpOverlapped)
{
#ifdef DEMAND_PAGING
    PDSTREAM pstm;
    DWORD dwError;

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    if (buffer == NULL && nBytesToRead == 0) {
        if (pstm->s_pvol->v_pdsk->d_diActive.di_flags & DISK_INFO_FLAG_PAGEABLE) {
            pstm->s_flags |= STF_DEMANDPAGED;
            return TRUE;            // yes, Pagein requests are supported
        }
        DEBUGMSG(TRUE,(DBGTEXT("FATFS!FAT_ReadFilePagein: driver declines to demand-page '%.11hs'\n"), pstm->s_achOEM));
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;               // but this little piggy cried all the way home
    }

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_ReadFilePagein(0x%x,0x%x bytes,position 0x%x)\n"), pfh, nBytesToRead, pfh->fh_pos));
    ASSERT(pstm->s_flags & STF_DEMANDPAGED);

    if (dwError = FATFSReadFile(pfh, buffer, nBytesToRead, lpNumBytesRead, lpOverlapped, NULL, NULL))
        SetLastError(dwError);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,
             (DBGTEXT("FATFS!FAT_ReadFilePagein(0x%x) returned 0x%x (%d, 0x%x bytes, pos 0x%x, size 0x%x)\n"), pfh, dwError == ERROR_SUCCESS, dwError, *lpNumBytesRead, pfh->fh_pos, pstm->s_size));

    return dwError == ERROR_SUCCESS;
#else
    DEBUGMSG(TRUE,(DBGTEXT("FATFS!FAT_ReadFilePagein(0x%x,0x%x bytes,position 0x%x) is hard-coded to FAIL!\n"), pfh, nBytesToRead, pfh->fh_pos));

    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
#endif
}

#endif


BOOL FAT_ReadFile(
PFHANDLE pfh,
LPVOID buffer,
DWORD nBytesToRead,
LPDWORD lpNumBytesRead,
LPOVERLAPPED lpOverlapped)
{
    PDSTREAM pstm;
    DWORD dwError;

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_ReadFile(0x%x,0x%x bytes,position 0x%x)\n"), pfh, nBytesToRead, pfh->fh_pos));

    if (!FATEnter(NULL, LOGID_NONE)) {
        DEBUGMSG(ZONE_APIS || ZONE_ERRORS,(DBGTEXT("FATFS!FAT_ReadFile bailing...\n")));
        return FALSE;
    }
    if (buffer)
        LockPages(buffer,nBytesToRead,0,LOCKFLAG_WRITE);
    if (dwError = FATFSReadFile(pfh, buffer, nBytesToRead, lpNumBytesRead, lpOverlapped, NULL, NULL))
        SetLastError(dwError);
    if (buffer)
        UnlockPages(buffer,nBytesToRead);

    FATExit(LOGID_NONE);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,
             (DBGTEXT("FATFS!FAT_ReadFile(0x%x) returned 0x%x (%d, 0x%x bytes, pos 0x%x, size 0x%x)\n"), pfh, dwError == ERROR_SUCCESS, dwError, *lpNumBytesRead, pfh->fh_pos, pstm->s_size));

    return dwError == ERROR_SUCCESS;
}


#if NUM_FILE_APIS > 13

BOOL FAT_ReadFileWithSeek(
PFHANDLE pfh,
LPVOID buffer,
DWORD nBytesToRead,
LPDWORD lpNumBytesRead,
LPOVERLAPPED lpOverlapped,
DWORD dwLowOffset,
DWORD dwHighOffset)
{
#ifdef DEMAND_PAGING
    PDSTREAM pstm;
    DWORD dwError=ERROR_SUCCESS;

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_ReadFileWithSeek(0x%x,0x%x bytes,position 0x%x)\n"), pfh, nBytesToRead, dwLowOffset));

    if (buffer == NULL && nBytesToRead == 0) {
        if (pstm->s_pvol->v_pdsk->d_diActive.di_flags & DISK_INFO_FLAG_PAGEABLE) {
            pstm->s_flags |= STF_DEMANDPAGED;
            return TRUE;            // yes, Pagein requests are supported
        }
        DEBUGMSG(TRUE,(DBGTEXT("FATFS!FAT_ReadFileWithSeek: driver declines to demand-page '%.11hs'\n"), pstm->s_achOEM));
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;               // but this little piggy cried all the way home
    }
    if (buffer)
        LockPages(buffer,nBytesToRead,0,LOCKFLAG_WRITE);
    if (!FATEnter(NULL, LOGID_NONE))
        return FALSE;
    if ((pfh->fh_flags & FHF_FAILUI)){
        FATExit(LOGID_NONE);
    } else {
        dwError = FATFSReadFile(pfh, buffer, nBytesToRead, lpNumBytesRead, lpOverlapped, &dwLowOffset, &dwHighOffset);
        FATExit(LOGID_NONE);
        if (!dwError) {
            // If the read was successful make sure the UI is (re)enabled.
            pfh->fh_flags &= ~FHF_FAILUI; 
        } else {
            pfh->fh_flags |= FHF_FAILUI;
            SetLastError(dwError);
        }
    }    
    if (buffer)
        UnlockPages(buffer,nBytesToRead);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,
             (DBGTEXT("FATFS!FAT_ReadFileWithSeek(0x%x) returned 0x%x (%d, 0x%x bytes, size 0x%x)\n"), pfh, dwError == ERROR_SUCCESS, dwError, *lpNumBytesRead, pstm->s_size));

    return dwError == ERROR_SUCCESS;
#else
    DEBUGMSG(TRUE,(DBGTEXT("FATFS!FAT_ReadFileWithSeek(0x%x,0x%x bytes,position 0x%x) is hard-coded to FAIL!\n"), pfh, nBytesToRead, pfh->fh_pos));

    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
#endif
}

#endif


BOOL FAT_WriteFile(
PFHANDLE pfh,
LPCVOID buffer,
DWORD nBytesToWrite,
LPDWORD lpNumBytesWritten,
LPOVERLAPPED lpOverlapped)
{
    PDSTREAM pstm;
    DWORD dwError;

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_WriteFile(0x%x,0x%x bytes,position 0x%x)\n"), pfh, nBytesToWrite, pfh->fh_pos));

    if (!FATEnter(pfh->fh_pstm->s_pvol, LOGID_WRITEFILE))
        return FALSE;

    if (buffer)
        LockPages((LPVOID)buffer,nBytesToWrite,0,LOCKFLAG_READ);
    if (dwError = FATFSWriteFile(pfh, buffer, nBytesToWrite, lpNumBytesWritten, lpOverlapped, NULL, NULL)) {
        SetLastError(dwError);
    }
#ifdef TFAT    
    else if (pstm->s_pvol->v_fTfat && (STF_WRITETHRU & pstm->s_flags))
    {
        DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_WriteFile: Committing transactions")));
        // Write succeeded, commit transations if in write-through mode
        dwError = CommitTransactions (pstm->s_pvol);
    }
#endif    
    UnlockPages((LPVOID)buffer,nBytesToWrite);

    FATExit(LOGID_WRITEFILE);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,
             (DBGTEXT("FATFS!FAT_WriteFile(0x%x) returned 0x%x (%d, 0x%x bytes, pos 0x%x, size 0x%x)\n"), pfh, dwError == ERROR_SUCCESS, dwError, *lpNumBytesWritten, pfh->fh_pos, pstm->s_size));

    return dwError == ERROR_SUCCESS;
}


#if NUM_FILE_APIS > 13

BOOL FAT_WriteFileWithSeek(
PFHANDLE pfh,
LPCVOID buffer,
DWORD nBytesToWrite,
LPDWORD lpNumBytesWritten,
LPOVERLAPPED lpOverlapped,
DWORD dwLowOffset,
DWORD dwHighOffset)
{
#ifdef DEMAND_PAGING
    PDSTREAM pstm;
    DWORD dwError;

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_WriteFileWithSeek(0x%x,0x%x bytes,position 0x%x)\n"), pfh, nBytesToWrite, dwLowOffset));

    if (buffer)
        LockPages((LPVOID)buffer,nBytesToWrite,0,LOCKFLAG_READ);
    if (!FATEnter(pfh->fh_pstm->s_pvol, LOGID_WRITEFILE)) {
        if (buffer)
            UnlockPages((LPVOID)buffer,nBytesToWrite);
        return FALSE;
    }

    dwError = FATFSWriteFile(pfh, buffer, nBytesToWrite, lpNumBytesWritten, lpOverlapped, &dwLowOffset, &dwHighOffset);
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }
    
#ifdef TFAT    
    else if (pstm->s_pvol->v_fTfat && (STF_WRITETHRU & pstm->s_flags) && !(pstm->s_pvol->v_flFATFS & FATFS_WFWS_NOWRITETHRU))
    {
        // Write succeeded, commit transations if in write-through mode
        dwError = CommitTransactions (pstm->s_pvol);
    }
#endif    

    FATExit(LOGID_WRITEFILE);
    if (buffer)
        UnlockPages((LPVOID)buffer,nBytesToWrite);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,
             (DBGTEXT("FATFS!FAT_WriteFileWithSeek(0x%x) returned 0x%x (%d, 0x%x bytes, size 0x%x)\n"), pfh, dwError == ERROR_SUCCESS, dwError, *lpNumBytesWritten, pstm->s_size));

    return dwError == ERROR_SUCCESS;
#else
    DEBUGMSG(TRUE,(DBGTEXT("FATFS!FAT_WriteFileWithSeek(0x%x,0x%x bytes,position 0x%x) is hard-coded to FAIL!\n"), pfh, nBytesToWrite, pfh->fh_pos));

    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
#endif
}

#endif


DWORD FAT_SetFilePointer(
PFHANDLE pfh,
LONG lDistanceToMove,
PLONG lpDistanceToMoveHigh,
DWORD dwMoveMethod)
{
    __int64 newPos;
    PDSTREAM pstm;
    DWORD dwError;
    DWORD dwReturn = (DWORD)-1;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_SetFilePointer(0x%x,offset 0x%x,method %d)\n"), pfh, lDistanceToMove, dwMoveMethod));

    FATEnterQuick();
    
    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    if (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    dwError = ERROR_SUCCESS;

    __try {
        if (lpDistanceToMoveHigh) {
            SetLastError(NO_ERROR);
            newPos = (__int64)*lpDistanceToMoveHigh<<32 | (DWORD)lDistanceToMove;
            *lpDistanceToMoveHigh = 0;
        } else {

            // As per the Win32 spec, if the FILE_BEGIN method is specified,
            // the distance to move is interpreted as 32 (or 64)-bit UNSIGNED
            // value, hence the DWORD cast to insure that the upper 32 bits
            // of the __int64 newPos don't get set due to bogus sign-extension.

            if (dwMoveMethod == FILE_BEGIN)
                newPos = (DWORD)lDistanceToMove;
            else
                newPos = lDistanceToMove;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    switch (dwMoveMethod) {
    case FILE_BEGIN:
        break;

    case FILE_CURRENT:
        newPos += pfh->fh_pos;
        break;

    case FILE_END:
        newPos += pstm->s_size;
        break;

    default:
        dwError = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    



    if ((newPos >> 32) != 0)
        dwError = ERROR_NEGATIVE_SEEK;
    else
        dwReturn = pfh->fh_pos = (DWORD)newPos;

  exit:
    if (dwError)
        SetLastError(dwError);

    FATExitQuick();

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_SetFilePointer returned 0x%x (%d)\n"), dwReturn, dwError));

    return dwReturn;
}


DWORD FAT_GetFileSize(PFHANDLE pfh, LPDWORD lpFileSizeHigh)
{
    PDSTREAM pstm;
    DWORD dwError;
    DWORD dwReturn = (DWORD)-1;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_GetFileSize(0x%x)\n"), pfh));

    FATEnterQuick();

    if (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    dwError = ERROR_SUCCESS;

    __try {
        if (lpFileSizeHigh) {
            SetLastError(NO_ERROR);
            *lpFileSizeHigh= 0;
        }
        dwReturn = pstm->s_size;

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }

  exit:
    if (dwError)
        SetLastError(dwError);

    FATExitQuick();

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_GetFileSize returned 0x%x (%d)\n"), dwReturn, dwError));

    return dwReturn;
}


BOOL FAT_GetFileInformationByHandle(
PFHANDLE pfh,
LPBY_HANDLE_FILE_INFORMATION lpFileInfo)
{
    PDSTREAM pstm;
    DWORD dwError;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_GetFileInformationByHandle(0x%x)\n"), pfh));

    FATEnterQuick();

    if (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    dwError = ERROR_SUCCESS;

    EnterCriticalSection(&pstm->s_cs);
    __try {
        lpFileInfo->dwFileAttributes = pstm->s_attr;
        lpFileInfo->ftCreationTime = pstm->s_ftCreate;
        lpFileInfo->ftLastAccessTime = pstm->s_ftAccess;
        lpFileInfo->ftLastWriteTime = pstm->s_ftWrite;
        lpFileInfo->dwVolumeSerialNumber = pstm->s_pvol->v_serialID;
        lpFileInfo->nFileSizeHigh = 0;
        lpFileInfo->nFileSizeLow = pstm->s_size;
        lpFileInfo->nNumberOfLinks = 1;
        lpFileInfo->nFileIndexHigh = pstm->s_sid.sid_clusDir;
        lpFileInfo->nFileIndexLow = pstm->s_sid.sid_ordDir;
        lpFileInfo->dwOID = OIDFROMSTREAM(pstm);    // TODO: YG - Investigate
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }
    LeaveCriticalSection(&pstm->s_cs);

  exit:
    if (dwError)
        SetLastError(dwError);

    FATExitQuick();

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_GetFileInformationByHandle returned 0x%x (%d)\n"), dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}


BOOL FAT_FlushFileBuffers(PFHANDLE pfh)
{
    PDSTREAM pstm;
    DWORD dwError;

    if (!BufEnter(TRUE))
        return FALSE;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_FlushFileBuffers(0x%x)\n"), pfh));

    if (!(pfh->fh_mode & FH_MODE_WRITE) || (pfh->fh_flags & FHF_UNMOUNTED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    EnterCriticalSection(&pstm->s_cs);
    dwError = CommitStream(pstm, TRUE);
    LeaveCriticalSection(&pstm->s_cs);

#ifdef TFAT
    if (pstm->s_pvol->v_fTfat && (dwError == ERROR_SUCCESS))
        dwError = CommitTransactions (pstm->s_pvol);
#endif

  exit:
    if (dwError) {
        SetLastError(dwError);
    }

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_FlushFileBuffers returned 0x%x (%d)\n"), dwError == ERROR_SUCCESS, dwError));

    BufExit();

    return dwError == ERROR_SUCCESS;
}


BOOL FAT_GetFileTime(
PFHANDLE pfh,
LPFILETIME lpCreation,
LPFILETIME lpLastAccess,
LPFILETIME lpLastWrite)
{
    PDSTREAM pstm;
    DWORD dwError;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_GetFileTime(0x%x)\n"), pfh));

    FATEnterQuick();

    if (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    dwError = ERROR_SUCCESS;

    EnterCriticalSection(&pstm->s_cs);
    __try {
        if (lpCreation)
            *lpCreation = pstm->s_ftCreate;
        if (lpLastAccess)
            *lpLastAccess = pstm->s_ftAccess;
        if (lpLastWrite)
            *lpLastWrite = pstm->s_ftWrite;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }
    LeaveCriticalSection(&pstm->s_cs);

  exit:
    if (dwError)
        SetLastError(dwError);

    FATExitQuick();

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_GetFileTime returned 0x%x (%d)\n"), dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}


BOOL FAT_SetFileTime(
PFHANDLE pfh,
CONST FILETIME *lpCreation,
CONST FILETIME *lpLastAccess,
CONST FILETIME *lpLastWrite)
{
    PDSTREAM pstm;
    DWORD dwError;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_SetFileTime(0x%x)\n"), pfh));

    if (!FATEnter(NULL, LOGID_NONE))
        return FALSE;

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    if (!(pfh->fh_mode & FH_MODE_WRITE) || (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED))) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    if (pstm->s_pvol->v_flags & VOLF_READONLY) {
        dwError = ERROR_WRITE_PROTECT;
        goto exit;
    }

    dwError = ERROR_SUCCESS;

    EnterCriticalSection(&pstm->s_cs);
    __try {
        if (lpCreation) {
            pstm->s_ftCreate = *lpCreation;
            pstm->s_flags |= STF_CREATE_TIME | STF_DIRTY_INFO;
        }
        if (lpLastAccess) {
            pstm->s_ftAccess = *lpLastAccess;
            pstm->s_flags |= STF_ACCESS_TIME | STF_DIRTY_INFO;
        }
        if (lpLastWrite) {
            pstm->s_ftWrite = *lpLastWrite;
            pstm->s_flags |= STF_WRITE_TIME | STF_DIRTY_INFO;
        }

#ifdef WASTE_OF_TIME

        // This is a waste of time, because when the recipient of this message
        // turns around and calls FAT_OidGetInfo, it's not going to see the new
        // time anyway.  New file times don't get written to disk until a commit
        // or close, and FAT_OidGetInfo only reports file information currently
        // stored on disk.  Since we always post DB_CEOID_CHANGED when a file's
        // directory entry is updated, this DB_CEOID_CHANGED is superfluous.
        //
        // You might think that FAT_OidGetInfo should look for and extract
        // information from an open stream (if one exists), but that would be
        // inconsistent with how all other APIs have historically worked.  For
        // example, FindFirstFile always returns what's recorded in a file's
        // directory entry, regardless of what another thread or process may be
        // doing to that file at the same time. -JTP

        FILESYSTEMNOTIFICATION(pvol, DB_CEOID_CHANGED, 0, SHCNE_UPDATEITEM, &pstm->s_sid, &pstm->s_sidParent, NULL, NULL, NULL, DBGTEXTW("FAT_SetFileTime"));

#endif  // WASTE_OF_TIME

    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }
    LeaveCriticalSection(&pstm->s_cs);

  exit:
    if (dwError)
        SetLastError(dwError);

    FATExit(LOGID_NONE);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_SetFileTime returned 0x%x (%d)\n"), dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}


BOOL FAT_SetEndOfFile(PFHANDLE pfh)
{
    PDSTREAM pstm;
    DWORD dwError;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_SetEndOfFile(0x%x)\n"), pfh));

    if (!FATEnter(pfh->fh_pstm->s_pvol, LOGID_SETENDOFFILE))
        return FALSE;

    if (!(pfh->fh_mode & FH_MODE_WRITE) || (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED))) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    EnterCriticalSection(&pstm->s_cs);
    dwError = ResizeStream(pstm, pfh->fh_pos, RESIZESTREAM_SHRINK|RESIZESTREAM_UPDATEFAT);
    LeaveCriticalSection(&pstm->s_cs);

  exit:
    if (dwError) {
        SetLastError(dwError);
    }

    FATExit(LOGID_SETENDOFFILE);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_SetEndOfFile returned 0x%x (%d)\n"), dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}


BOOL FAT_DeviceIoControl(
PFHANDLE pfh,
DWORD dwIoControlCode,
LPVOID lpInBuf, DWORD nInBufSize,
LPVOID lpOutBuf, DWORD nOutBufSize,
LPDWORD lpBytesReturned,
LPOVERLAPPED lpOverlapped)
{
    DWORD dwError = ERROR_INVALID_PARAMETER;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_DeviceIoControl(0x%x,0x%x)\n"), pfh, dwIoControlCode));

    



    // FATEnterQuick();

    if ((pfh->fh_flags & FHF_VOLUME) && !(pfh->fh_flags & FHF_UNMOUNTED)) {

        PVOLUME pvol = pfh->fh_pstm->s_pvol;

      __try {

        *lpBytesReturned = 0;

        if ((dwIoControlCode == IOCTL_DISK_GET_DEVICE_PARAMETERS ||
             dwIoControlCode == IOCTL_DISK_GET_VOLUME_PARAMETERS) &&
            nOutBufSize >= sizeof(DEVPB)) {

            QueryVolumeParameters(pvol, (PDEVPB)lpOutBuf, dwIoControlCode == IOCTL_DISK_GET_VOLUME_PARAMETERS);

            *lpBytesReturned = sizeof(DEVPB);
            dwError = ERROR_SUCCESS;
            goto exit;
        }

        if (dwIoControlCode == IOCTL_DISK_GET_FREE_SPACE && nOutBufSize >= sizeof(FREEREQ)) {

            PFREEREQ pfr = (PFREEREQ)lpOutBuf;

            if (FAT_GetDiskFreeSpaceW(pvol,
                                     NULL,
                                     &pfr->fr_SectorsPerCluster,
                                     &pfr->fr_BytesPerSector,
                                     &pfr->fr_FreeClusters,
                                     &pfr->fr_Clusters)) {
                *lpBytesReturned = sizeof(FREEREQ);
                dwError = ERROR_SUCCESS;
            }
            goto exit;
        }


        // NOTE: Callers of IOCTL_DISK_FORMAT_VOLUME and IOCTL_DISK_SCAN_VOLUME
        // need only pass the first (flags) DWORD of the corresponding FMTVOLREQ and
        // SCANVOLREQ packets;  the flags will indicate whether or not any of the
        // additional fields that we may or may not add over time are actually present.

        if (dwIoControlCode == IOCTL_DISK_FORMAT_VOLUME) {
            if (nInBufSize < sizeof(DWORD))
                lpInBuf = NULL;
            dwError = FormatVolume(pvol, (PFMTVOLREQ)lpInBuf);
            *lpBytesReturned = 0;
            goto exit;
        }

        if (dwIoControlCode == IOCTL_DISK_SCAN_VOLUME) {
#ifdef USE_FATUTIL
            DWORD dwScanVol = SCANVOL_QUICK;
            if (nInBufSize >= sizeof(DWORD) && lpInBuf) {
                dwScanVol = ((PSCANVOLREQ)lpInBuf)->sv_dwScanVol;
            }
            dwError = ScanVolume(pvol, dwScanVol);
            *lpBytesReturned = 0;            
#else
            DWORD dwScanVol = SCANVOL_QUICK;
            PSCANRESULTS psr = NULL;
            if (nInBufSize >= sizeof(DWORD) && lpInBuf) {
                dwScanVol = ((PSCANVOLREQ)lpInBuf)->sv_dwScanVol;
            }
            if (nOutBufSize >= sizeof(SCANRESULTS) && lpOutBuf) {
                psr = lpOutBuf;
                psr->sr_dwScanErr = 0;
            }
            dwError = ScanVolume(pvol, dwScanVol, NULL, psr);
            if (psr)
                *lpBytesReturned = sizeof(SCANRESULTS);
#endif            
            goto exit;
        }

#ifdef DEBUG
        if (dwIoControlCode == IOCTL_DISK_SET_DEBUG_ZONES && nInBufSize >= sizeof(DWORD)) {
            dpCurSettings.ulZoneMask = *(PDWORD)lpInBuf;
            dwError = ERROR_SUCCESS;
            *lpBytesReturned = 0;
            goto exit;
        }
#endif

// Last ditch attempt... if all else fails and the dwIoControlCode is
// in the valid range, pass it on to the device driver and hope for the best

        if (((dwIoControlCode >> 16) & 0xffff) != IOCTL_DISK_BASE ||
            ((dwIoControlCode >> 2) & 0xfff) < 0x080 ||
            ((dwIoControlCode >> 2) & 0xfff) >= 0x700) {
            if (!FSDMGR_DiskIoControl((HDSK)pvol->v_pdsk->d_hdsk, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped)) {
                dwError = GetLastError();
            } else {
                dwError = ERROR_SUCCESS;
            }
            goto exit;
        }

      } __except (EXCEPTION_EXECUTE_HANDLER) {
        ;
      }
    }

  exit:
    if (dwError)
        SetLastError(dwError);

    

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_DeviceIoControl returned 0x%x (%d)\n"), dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}
