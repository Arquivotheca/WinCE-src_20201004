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

    DEBUGMSGW(ZONE_APIS,(DBGTEXTW("FATFS!FAT_CreateFileW(%d chars: %s)\r\n"), wcslen(lpFileName), lpFileName));

    if (!FATEnter(pvol, LOGID_CREATEFILE)) {
        DEBUGMSG(ZONE_APIS || ZONE_ERRORS,(DBGTEXT("FATFS!FAT_CreateFileW bailing...\r\n")));
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
        if (!CheckStreamSharing(pstm, mode, flName & NAME_TRUNCATE)) {
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

    pfh = (PFHANDLE)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(FHANDLE));
    if (pfh) {
        if (!(hFile = FSDMGR_CreateFileHandle( pvol->v_hVol, hProc, (PFILE)pfh))) {
            DEBUGFREE(sizeof(FHANDLE));
            VERIFYTRUE(HeapFree(hHeap, 0, pfh));
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
    FATExit(pvol, LOGID_CREATEFILE);

    DEBUGMSGW(ZONE_APIS || ZONE_ERRORS && !pfh, (DBGTEXTW("FATFS!FAT_CreateFileW(%-.64s) returned 0x%x (%d)\r\n"), lpFileName, pfh, dwError));

    return pfh? hFile : INVALID_HANDLE_VALUE;
}


BOOL FAT_CloseFile(PFHANDLE pfh)
{
    DWORD dwReturn = ERROR_SUCCESS;
    DWORD dwError = ERROR_SUCCESS;
    PDSTREAM pstm;
    PVOLUME  pvol;
#ifdef TFAT_TIMING_TEST
    DWORD dwTickCnt=GetTickCount();
#endif
    DSID sid, sidParent;

#ifdef TFAT
    PDSTREAM pstmParent;
#endif

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    pvol = pstm->s_pvol;
    ASSERT(pvol);
    
    if (!BufEnter(pvol, TRUE))
        return FALSE;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_CloseFile(0x%x, %.11hs: %d)\r\n"), pfh, pfh->fh_pstm->s_achOEM, pfh->fh_pstm->s_clusFirst));

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

    // unlock locks owned by this handle
    if (!(pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED))) {
        FSDMGR_RemoveFileLockEx(AcquireFileLockState, ReleaseFileLockState, (DWORD)pfh);
    }

    RemoveItem((PDLINK)&pfh->fh_dlOpenHandles);

#ifndef TFAT
    dwError = CommitBufferSet( (PDSTREAM)pvol, -1);
    // If we get an error, attempt to continue the rest of the close process
    if (dwError != ERROR_SUCCESS) 
        dwReturn = dwError;
#endif

    memcpy (&sid, &pstm->s_sid, sizeof(DSID));
    memcpy (&sidParent, &pstm->s_sidParent, sizeof(DSID));

    dwError = CloseStream(pstm);

    if (dwError != ERROR_SUCCESS) 
        dwReturn = dwError;

#ifdef TFAT
    if(pvol->v_fTfat && pstmParent && (pfh->fh_mode & FH_MODE_WRITE))
    {
        dwError = CloseStream(pstmParent);
        if (dwError != ERROR_SUCCESS) 
            dwReturn = dwError;        
    }

    // Sync FAT only if a file was opened for write
    if (pvol->v_fTfat && (pfh->fh_mode & FH_MODE_WRITE))
    {
#ifdef TFAT_TIMING_TEST
		RETAILMSG(1, (L"FAT_CloseFile takes %ld\r\n", GetTickCount()-dwTickCnt));
#endif
        dwError = CommitTransactions (pvol);
    }
    else
        dwError = CommitBufferSet( (PDSTREAM)pvol, -1);
#endif

    if (dwError != ERROR_SUCCESS) 
        dwReturn = dwError;

    // Don't send notification for special file handles (like VOL:)
    if (sid.sid_clusDir && (pfh->fh_mode & FH_MODE_WRITE))
        FILESYSTEMNOTIFICATION(pvol, DB_CEOID_CHANGED, 0, SHCNE_UPDATEITEM, &sid, &sidParent, NULL, NULL, NULL, DBGTEXTW("CloseFile"));

    DEBUGFREE(sizeof(FHANDLE));
    VERIFYTRUE(HeapFree(hHeap, 0, pfh));

    BufExit(pvol);

    if (dwReturn != ERROR_SUCCESS)
    {
        DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_CloseFile returned FALSE (%d)\r\n"), dwReturn));
        SetLastError (dwReturn);
    }
    else
    {
        DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_CloseFile returned TRUE (0)\r\n")));
    }
    
    return (dwReturn == ERROR_SUCCESS);
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
    DWORD dwError = ERROR_GEN_FAILURE;
    DWORD cbRead = 0;

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

#ifdef TFAT
    if(pstm->s_pvol->v_fTfat && pstm->s_pstmParent)
        EnterCriticalSection(&pstm->s_pstmParent->s_cs);
#endif

    EnterCriticalSection(&pstm->s_cs);
    __try {

        if (!(pfh->fh_mode & FH_MODE_READ) || (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)))
            dwError = ERROR_ACCESS_DENIED;
        else {
            
            if (!lpdwLowOffset) {
                // authorize access
                if (FSDMGR_TestFileLockEx(AcquireFileLockState, ReleaseFileLockState, (DWORD)pfh, TRUE, nBytesToRead, pfh->fh_pos, 0)) {
                    dwError = ReadStreamData(pstm, pfh->fh_pos, buffer, nBytesToRead, &cbRead);
                    pfh->fh_pos += cbRead;
                }
                else {
                    dwError = ERROR_ACCESS_DENIED;
                }
            }
            else {
                // authorize access
                if (FSDMGR_TestFileLockEx(AcquireFileLockState, ReleaseFileLockState, (DWORD)pfh, TRUE, nBytesToRead, *lpdwLowOffset, 0)) {
                    dwError = ReadStreamData(pstm, *lpdwLowOffset, buffer, nBytesToRead, &cbRead);
                }
                else {
                    dwError = ERROR_ACCESS_DENIED;
                }
            }
        }

        if (lpNumBytesRead && (ERROR_SUCCESS == dwError)) {
            *lpNumBytesRead = cbRead;
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
    DWORD dwError = ERROR_GEN_FAILURE;
    DWORD cbWritten = 0;

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

#ifdef TFAT
    if(pstm->s_pstmParent)
        EnterCriticalSection(&pstm->s_pstmParent->s_cs);
#endif

    EnterCriticalSection(&pstm->s_cs);
    __try {

        if (!(pfh->fh_mode & FH_MODE_WRITE) || (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)))
            dwError = ERROR_ACCESS_DENIED;
        else {
            
            if (!lpdwLowOffset) {
                if (FSDMGR_TestFileLockEx(AcquireFileLockState, ReleaseFileLockState, (DWORD)pfh, FALSE, nBytesToWrite, pfh->fh_pos, 0)) {
                    dwError = WriteStreamData(pstm, pfh->fh_pos, buffer, nBytesToWrite, &cbWritten, TRUE);
                    pfh->fh_pos += cbWritten;
                }
                else {
                    dwError = ERROR_ACCESS_DENIED;
                }
            }
            else {
                if (FSDMGR_TestFileLockEx(AcquireFileLockState, ReleaseFileLockState, (DWORD)pfh, TRUE, nBytesToWrite, *lpdwLowOffset, 0)) {
                    dwError = WriteStreamData(pstm, *lpdwLowOffset, buffer, nBytesToWrite, &cbWritten, TRUE);
                }
                else {
                    dwError = ERROR_ACCESS_DENIED;
                }
            }

            if (lpNumBytesWritten && (ERROR_SUCCESS == dwError)) {
                *lpNumBytesWritten = cbWritten;
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
        DEBUGMSG(TRUE,(DBGTEXT("FATFS!FAT_ReadFilePagein: driver declines to demand-page '%.11hs'\r\n"), pstm->s_achOEM));
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;               // but this little piggy cried all the way home
    }

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_ReadFilePagein(0x%x,0x%x bytes,position 0x%x)\r\n"), pfh, nBytesToRead, pfh->fh_pos));
    ASSERT(pstm->s_flags & STF_DEMANDPAGED);

    if (dwError = FATFSReadFile(pfh, buffer, nBytesToRead, lpNumBytesRead, lpOverlapped, NULL, NULL))
        SetLastError(dwError);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,
             (DBGTEXT("FATFS!FAT_ReadFilePagein(0x%x) returned 0x%x (%d, 0x%x bytes, pos 0x%x, size 0x%x)\r\n"), pfh, dwError == ERROR_SUCCESS, dwError, *lpNumBytesRead, pfh->fh_pos, pstm->s_size));

    return dwError == ERROR_SUCCESS;
#else
    DEBUGMSG(TRUE,(DBGTEXT("FATFS!FAT_ReadFilePagein(0x%x,0x%x bytes,position 0x%x) is hard-coded to FAIL!\r\n"), pfh, nBytesToRead, pfh->fh_pos));

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

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_ReadFile(0x%x,0x%x bytes,position 0x%x)\r\n"), pfh, nBytesToRead, pfh->fh_pos));

    if (!FATEnter(NULL, LOGID_NONE)) {
        DEBUGMSG(ZONE_APIS || ZONE_ERRORS,(DBGTEXT("FATFS!FAT_ReadFile bailing...\r\n")));
        return FALSE;
    }
    if (buffer)
        LockPages(buffer,nBytesToRead,0,LOCKFLAG_WRITE);
    if (dwError = FATFSReadFile(pfh, buffer, nBytesToRead, lpNumBytesRead, lpOverlapped, NULL, NULL))
        SetLastError(dwError);
    if (buffer)
        UnlockPages(buffer,nBytesToRead);

    FATExit(pstm->s_pvol, LOGID_NONE);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,
             (DBGTEXT("FATFS!FAT_ReadFile(0x%x) returned 0x%x (%d, 0x%x bytes, pos 0x%x, size 0x%x)\r\n"), pfh, dwError == ERROR_SUCCESS, dwError, *lpNumBytesRead, pfh->fh_pos, pstm->s_size));

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

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_ReadFileWithSeek(0x%x,0x%x bytes,position 0x%x)\r\n"), pfh, nBytesToRead, dwLowOffset));

    if (buffer == NULL && nBytesToRead == 0) {
        if (pstm->s_pvol->v_pdsk->d_diActive.di_flags & DISK_INFO_FLAG_PAGEABLE) {
            pstm->s_flags |= STF_DEMANDPAGED;
            return TRUE;            // yes, Pagein requests are supported
        }
        DEBUGMSG(TRUE,(DBGTEXT("FATFS!FAT_ReadFileWithSeek: driver declines to demand-page '%.11hs'\r\n"), pstm->s_achOEM));
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;               // but this little piggy cried all the way home
    }
    if (buffer)
        LockPages(buffer,nBytesToRead,0,LOCKFLAG_WRITE);
    if (!FATEnter(NULL, LOGID_NONE))
        return FALSE;

    dwError = FATFSReadFile(pfh, buffer, nBytesToRead, lpNumBytesRead, lpOverlapped, &dwLowOffset, &dwHighOffset);
    FATExit(pstm->s_pvol, LOGID_NONE);
    if (dwError) {
        SetLastError(dwError);
    }

    if (buffer)
        UnlockPages(buffer,nBytesToRead);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,
             (DBGTEXT("FATFS!FAT_ReadFileWithSeek(0x%x) returned 0x%x (%d, 0x%x bytes, size 0x%x)\r\n"), pfh, dwError == ERROR_SUCCESS, dwError, *lpNumBytesRead, pstm->s_size));

    return dwError == ERROR_SUCCESS;
#else
    DEBUGMSG(TRUE,(DBGTEXT("FATFS!FAT_ReadFileWithSeek(0x%x,0x%x bytes,position 0x%x) is hard-coded to FAIL!\r\n"), pfh, nBytesToRead, pfh->fh_pos));

    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
#endif
}

BOOL FAT_ReadFileScatter(
PFHANDLE pfh,
FILE_SEGMENT_ELEMENT aSegmentArray[],
DWORD nNumberOfBytesToRead,
LPDWORD lpReserved,
LPOVERLAPPED lpOverlapped)
{
    PDSTREAM pstm;
    DWORD dwError = ERROR_SUCCESS;
    SYSTEM_INFO SystemInfo;
    DWORD dwNumPages;
    DWORD iPage;

    // Interpret lpReserved as an array of 64-bit offsets if provided.
    FILE_SEGMENT_ELEMENT* aOffsetArray = (FILE_SEGMENT_ELEMENT*)lpReserved;
    
    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_ReadFileScatter(0x%x,0x%x bytes)\r\n"), pfh, nNumberOfBytesToRead));

    GetSystemInfo (&SystemInfo);

    if (!nNumberOfBytesToRead || nNumberOfBytesToRead % SystemInfo.dwPageSize) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    dwNumPages = nNumberOfBytesToRead / SystemInfo.dwPageSize;

    if (!FATEnter(NULL, LOGID_NONE)) {
        DEBUGMSG(ZONE_APIS || ZONE_ERRORS,(DBGTEXT("FATFS!FAT_ReadFile bailing...\r\n")));
        return FALSE;
    }

#ifdef TFAT
    if(pstm->s_pvol->v_fTfat && pstm->s_pstmParent)
        EnterCriticalSection(&pstm->s_pstmParent->s_cs);
#endif

    EnterCriticalSection(&pstm->s_cs);
    __try {

        if (!(pfh->fh_mode & FH_MODE_READ) || (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)))
            dwError = ERROR_ACCESS_DENIED;
        else {
            for (iPage = 0; iPage < dwNumPages; iPage++) {
                DWORD pos = aOffsetArray ? (DWORD)aOffsetArray[iPage].Alignment : pfh->fh_pos;
                dwError = ReadStreamData(pstm, pos, aSegmentArray[iPage].Buffer, SystemInfo.dwPageSize, NULL);
                if (dwError) 
                    break;                
                if (!aOffsetArray)
                    pfh->fh_pos += SystemInfo.dwPageSize;
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

    FATExit(pstm->s_pvol, LOGID_NONE);


    if (dwError)
        SetLastError (dwError);

    return dwError == ERROR_SUCCESS;
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

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_WriteFile(0x%x,0x%x bytes,position 0x%x)\r\n"), pfh, nBytesToWrite, pfh->fh_pos));

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

    FATExit(pstm->s_pvol, LOGID_WRITEFILE);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,
             (DBGTEXT("FATFS!FAT_WriteFile(0x%x) returned 0x%x (%d, 0x%x bytes, pos 0x%x, size 0x%x)\r\n"), pfh, dwError == ERROR_SUCCESS, dwError, *lpNumBytesWritten, pfh->fh_pos, pstm->s_size));

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

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_WriteFileWithSeek(0x%x,0x%x bytes,position 0x%x)\r\n"), pfh, nBytesToWrite, dwLowOffset));

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

    FATExit(pstm->s_pvol, LOGID_WRITEFILE);
    if (buffer)
        UnlockPages((LPVOID)buffer,nBytesToWrite);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,
             (DBGTEXT("FATFS!FAT_WriteFileWithSeek(0x%x) returned 0x%x (%d, 0x%x bytes, size 0x%x)\r\n"), pfh, dwError == ERROR_SUCCESS, dwError, *lpNumBytesWritten, pstm->s_size));

    return dwError == ERROR_SUCCESS;
#else
    DEBUGMSG(TRUE,(DBGTEXT("FATFS!FAT_WriteFileWithSeek(0x%x,0x%x bytes,position 0x%x) is hard-coded to FAIL!\r\n"), pfh, nBytesToWrite, pfh->fh_pos));

    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
#endif
}

#ifdef TFAT
// determine if there is sufficient space left on the device to safely complete a transacted WFG operation
static DWORD CheckWriteFileGatherSpace(PVOLUME pvol, FILE_SEGMENT_ELEMENT *aSegmentArray, FILE_SEGMENT_ELEMENT *aOffsetArray, DWORD dwNumPages, DWORD dwPageSize)
{
    DWORD dwError = ERROR_SUCCESS;

    // If cluster size is greater than page size, assume one page required per cluster.
    // For larger disks, this is a conservative estimate of the requirements since several contiguous
    // pages may map to the same cluster.
    DWORD dwClustersRequiredPerPage = (dwPageSize > pvol->v_cbClus) ? (dwPageSize / pvol->v_cbClus) : 1;

    // determine the number of free clusters in the file system if it has not already been determined
    if (UNKNOWN_CLUSTER == pvol->v_cclusFree) {   
        if (!CalculateFreeClustersInRAM(pvol)) {
            // If we couldn't do it in RAM, try it through the FAT buffers.
            dwError = CalculateFreeClustersFromBuffers(pvol);
            if (dwError) {
                goto exit;
            }
        }
    }

    DEBUGCHK(pvol->v_cclusFree != UNKNOWN_CLUSTER);

    // being conservative, there must be more free clusters than required clusters
    dwError = (pvol->v_cclusFree > (dwClustersRequiredPerPage * dwNumPages)) ? ERROR_SUCCESS : ERROR_DISK_FULL;
    
exit:
    return dwError;
}
#endif // TFAT

BOOL FAT_WriteFileGather(
PFHANDLE pfh,
FILE_SEGMENT_ELEMENT aSegmentArray[],
DWORD nNumberOfBytesToWrite,
LPDWORD lpReserved,
LPOVERLAPPED lpOverlapped)
{
    PDSTREAM pstm;
    DWORD dwError = ERROR_SUCCESS;
    SYSTEM_INFO SystemInfo;
    DWORD dwNumPages;
    DWORD iPage;
    
    // Interpret lpReserved as an array of 64-bit offsets if provided.
    FILE_SEGMENT_ELEMENT* aOffsetArray = (FILE_SEGMENT_ELEMENT*)lpReserved;
    
    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_WriteFileGather(0x%x,0x%x bytes)\r\n"), pfh, nNumberOfBytesToWrite));

    GetSystemInfo (&SystemInfo);

    if (!nNumberOfBytesToWrite || nNumberOfBytesToWrite % SystemInfo.dwPageSize) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    dwNumPages = nNumberOfBytesToWrite / SystemInfo.dwPageSize;

    if (!FATEnter(pfh->fh_pstm->s_pvol, LOGID_WRITEFILE))
        return FALSE;

#ifdef TFAT
    if(pstm->s_pvol->v_fTfat && pstm->s_pstmParent)
        EnterCriticalSection(&pstm->s_pstmParent->s_cs);
#endif

    EnterCriticalSection(&pstm->s_cs);
    LockFAT(pstm->s_pvol);
    
    __try {

        if (!(pfh->fh_mode & FH_MODE_WRITE) || (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)))
            dwError = ERROR_ACCESS_DENIED;
        else {

#ifdef TFAT
            if (pstm->s_pvol->v_fTfat && (STF_TRANSDATA & pstm->s_flags)) {
                dwError = CheckWriteFileGatherSpace(pstm->s_pvol, aSegmentArray, aOffsetArray, dwNumPages, SystemInfo.dwPageSize);
            }
#endif            
            if (!dwError) {
                for (iPage = 0; iPage < dwNumPages; iPage++) {
                    DWORD pos = aOffsetArray ? (DWORD)aOffsetArray[iPage].Alignment : pfh->fh_pos;
                    dwError = WriteStreamData(pstm, pos, aSegmentArray[iPage].Buffer, SystemInfo.dwPageSize, NULL, TRUE);
                    if (dwError)
                        break;    
                    if (!aOffsetArray)
                        pfh->fh_pos += SystemInfo.dwPageSize;
                }
            }
        }

#ifdef TFAT
        if (!dwError && pstm->s_pvol->v_fTfat && (STF_WRITETHRU & pstm->s_flags))
        {
            DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_WriteFileGather: Committing transactions")));
            // Write succeeded, commit transations if in write-through mode
            dwError = CommitTransactions (pstm->s_pvol);
        }    
#endif    


    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }

    UnlockFAT(pstm->s_pvol);    
    LeaveCriticalSection(&pstm->s_cs);

#ifdef TFAT
    if(pstm->s_pvol->v_fTfat && pstm->s_pstmParent)
        LeaveCriticalSection(&pstm->s_pstmParent->s_cs);
#endif

    FATExit(pstm->s_pvol, LOGID_WRITEFILE);

    if (dwError)
        SetLastError (dwError);

    return dwError == ERROR_SUCCESS;
}
#endif


DWORD FAT_SetFilePointer(
PFHANDLE pfh,
LONG lDistanceToMove,
PLONG lpDistanceToMoveHigh,
DWORD dwMoveMethod)
{
    __int64 newPos = 0;
    PDSTREAM pstm;
    DWORD dwError;
    DWORD dwReturn = (DWORD)-1;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_SetFilePointer(0x%x,offset 0x%x,method %d)\r\n"), pfh, lDistanceToMove, dwMoveMethod));

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

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_SetFilePointer returned 0x%x (%d)\r\n"), dwReturn, dwError));

    return dwReturn;
}


DWORD FAT_GetFileSize(PFHANDLE pfh, LPDWORD lpFileSizeHigh)
{
    PDSTREAM pstm;
    DWORD dwError;
    DWORD dwReturn = (DWORD)-1;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_GetFileSize(0x%x)\r\n"), pfh));

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

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_GetFileSize returned 0x%x (%d)\r\n"), dwReturn, dwError));

    return dwReturn;
}


BOOL FAT_GetFileInformationByHandle(
PFHANDLE pfh,
LPBY_HANDLE_FILE_INFORMATION lpFileInfo)
{
    PDSTREAM pstm;
    DWORD dwError;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_GetFileInformationByHandle(0x%x)\r\n"), pfh));

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
#ifdef UNDER_CE
        // NT does not have OID defined        
        lpFileInfo->dwOID = OIDFROMSTREAM(pstm);    // TODO: YG - Investigate
#endif        
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }
    LeaveCriticalSection(&pstm->s_cs);

  exit:
    if (dwError)
        SetLastError(dwError);

    FATExitQuick();

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_GetFileInformationByHandle returned 0x%x (%d)\r\n"), dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}


BOOL FAT_FlushFileBuffers(PFHANDLE pfh)
{
    PDSTREAM pstm;
    DWORD dwError;

    pstm = pfh->fh_pstm;
    ASSERT(pstm);

    if (!BufEnter(pstm->s_pvol, TRUE))
        return FALSE;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_FlushFileBuffers(0x%x)\r\n"), pfh));

    if (!(pfh->fh_mode & FH_MODE_WRITE) || (pfh->fh_flags & FHF_UNMOUNTED)) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

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

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_FlushFileBuffers returned 0x%x (%d)\r\n"), dwError == ERROR_SUCCESS, dwError));

    BufExit(pstm->s_pvol);

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

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_GetFileTime(0x%x)\r\n"), pfh));

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

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_GetFileTime returned 0x%x (%d)\r\n"), dwError == ERROR_SUCCESS, dwError));

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

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_SetFileTime(0x%x)\r\n"), pfh));

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

    FATExit(pstm->s_pvol, LOGID_NONE);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_SetFileTime returned 0x%x (%d)\r\n"), dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}


BOOL FAT_SetEndOfFile(PFHANDLE pfh)
{
    PDSTREAM pstm = pfh->fh_pstm;
    DWORD dwError;

    ASSERT(pstm);
    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_SetEndOfFile(0x%x)\r\n"), pfh));

    if (!FATEnter(pstm->s_pvol, LOGID_SETENDOFFILE))
        return FALSE;

    if (!(pfh->fh_mode & FH_MODE_WRITE) || (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED))) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    EnterCriticalSection(&pstm->s_cs);
    dwError = ResizeStream(pstm, pfh->fh_pos, RESIZESTREAM_SHRINK|RESIZESTREAM_UPDATEFAT);
    LeaveCriticalSection(&pstm->s_cs);

  exit:
    if (dwError) {
        SetLastError(dwError);
    }

    FATExit(pstm->s_pvol, LOGID_SETENDOFFILE);

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_SetEndOfFile returned 0x%x (%d)\r\n"), dwError == ERROR_SUCCESS, dwError));

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
    PVOLUME pvol = pfh->fh_pstm->s_pvol;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_DeviceIoControl(0x%x,0x%x)\r\n"), pfh, dwIoControlCode));

    // Deny all IOCTL access to untrusted applications
    if (OEM_CERTIFY_TRUST != PSLGetCallerTrust()) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }

    



    // FATEnterQuick();
    
#ifdef UNDER_CE
    if (dwIoControlCode == FSCTL_COPY_EXTERNAL_START) {

        if (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)) {
            dwError = ERROR_ACCESS_DENIED;
            goto exit;
        }
        
        if (!lpInBuf || nInBufSize < sizeof(FILE_COPY_EXTERNAL)) {
            dwError = ERROR_INVALID_PARAMETER;
            goto exit;
        }
        
        dwError = CopyFileExternal (pfh->fh_pstm, (PFILE_COPY_EXTERNAL)lpInBuf, lpOutBuf,nOutBufSize);
        goto exit;
    }

    if (dwIoControlCode == FSCTL_COPY_EXTERNAL_COMPLETE) {
        return FSDMGR_DiskIoControl((HDSK)pvol->v_pdsk->d_hdsk, IOCTL_DISK_COPY_EXTERNAL_COMPLETE, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
    }

#endif
    

    if ((pfh->fh_flags & FHF_VOLUME) && !(pfh->fh_flags & FHF_UNMOUNTED)) {

      __try {

        if ((dwIoControlCode == IOCTL_DISK_GET_DEVICE_PARAMETERS ||
             dwIoControlCode == IOCTL_DISK_GET_VOLUME_PARAMETERS) &&
            nOutBufSize >= sizeof(DEVPB)) {

            QueryVolumeParameters(pvol, (PDEVPB)lpOutBuf, dwIoControlCode == IOCTL_DISK_GET_VOLUME_PARAMETERS);

            if (lpBytesReturned) {
                *lpBytesReturned = sizeof(DEVPB);
            }

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
                if (lpBytesReturned) {
                    *lpBytesReturned = sizeof(FREEREQ);
                }
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
            if (lpBytesReturned) {
                *lpBytesReturned = 0;
            }
            goto exit;
        }

        if (dwIoControlCode == IOCTL_DISK_SCAN_VOLUME) {
            DWORD dwScanVol = SCANVOL_QUICK;
            if (nInBufSize >= sizeof(DWORD) && lpInBuf) {
                dwScanVol = ((PSCANVOLREQ)lpInBuf)->sv_dwScanVol;
            }
            dwError = ScanVolume(pvol, dwScanVol);
            if (lpBytesReturned) {
                *lpBytesReturned = 0;
            }
            goto exit;
        }
        
#if defined(DEBUG) && defined(UNDER_CE)
        if (dwIoControlCode == IOCTL_DISK_SET_DEBUG_ZONES && nInBufSize >= sizeof(DWORD)) {
            dpCurSettings.ulZoneMask = *(PDWORD)lpInBuf;
            dwError = ERROR_SUCCESS;
            if (lpBytesReturned) {
                *lpBytesReturned = 0;
            }
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

    

    DEBUGMSG(ZONE_APIS || ZONE_ERRORS && dwError,(DBGTEXT("FATFS!FAT_DeviceIoControl returned 0x%x (%d)\r\n"), dwError == ERROR_SUCCESS, dwError));

    return dwError == ERROR_SUCCESS;
}

#ifdef UNDER_CE
BOOL FAT_LockFileEx(PFHANDLE pfh, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped)
{
    PREFAST_DEBUGCHK(pfh);

    return FSDMGR_InstallFileLock(AcquireFileLockState, ReleaseFileLockState, (DWORD)pfh, dwFlags, dwReserved, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, lpOverlapped);
}

BOOL FAT_UnlockFileEx(PFHANDLE pfh, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped)
{
    PREFAST_DEBUGCHK(pfh);

    return FSDMGR_RemoveFileLock(AcquireFileLockState, ReleaseFileLockState, (DWORD)pfh, dwReserved, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, lpOverlapped);
}

BOOL AcquireFileLockState(DWORD dwPfh, PFILELOCKSTATE *ppFileLockState)
{
    PFHANDLE pfh;
    PDSTREAM pstm;
    BOOL fRet;

    pfh = (PFHANDLE)dwPfh;

    PREFAST_DEBUGCHK(pfh);
    PREFAST_DEBUGCHK(ppFileLockState);

    FATEnterQuick();

    pstm = pfh->fh_pstm;
    PREFAST_DEBUGCHK(pstm);

    if (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)) {
        fRet = FALSE;
        goto exit;
    }

    if (NULL == pstm->s_filelockstate.lpcs) {
        fRet = FALSE;
        goto exit;
    }

    // TODO: what protects pfh?

    // acquire file lock state; exit in ReleaseFileLockState
    EnterCriticalSection(pstm->s_filelockstate.lpcs);

    // fetch file position
    pstm->s_filelockstate.dwPosLow = pfh->fh_pos;
    pstm->s_filelockstate.dwPosHigh = 0;

    // fetch create access
    pstm->s_filelockstate.dwAccess = 0;
    if (pfh->fh_mode & AccessToMode(GENERIC_WRITE)) {
        pstm->s_filelockstate.dwAccess |= GENERIC_WRITE;
    }
    if (pfh->fh_mode & AccessToMode(GENERIC_READ)){
        pstm->s_filelockstate.dwAccess |= GENERIC_READ;
    }

    // fetch file lock container
    *ppFileLockState = &pstm->s_filelockstate;

    fRet = TRUE;

exit:

    FATExitQuick();

    return fRet;
}


BOOL ReleaseFileLockState(DWORD dwPfh, PFILELOCKSTATE *ppFileLockState)
{
    PFHANDLE pfh;
    PDSTREAM pstm;
    BOOL fRet;

    pfh = (PFHANDLE)dwPfh;

    PREFAST_DEBUGCHK(pfh);
    PREFAST_DEBUGCHK(ppFileLockState);

    FATEnterQuick();

    pstm = pfh->fh_pstm;
    PREFAST_DEBUGCHK(pstm);

    // if volume is unmounted, then fail
    if (pfh->fh_flags & (FHF_VOLUME | FHF_UNMOUNTED)) {
        fRet = FALSE;
        goto exit;
    }

    if (NULL == pstm->s_filelockstate.lpcs) {
        fRet = FALSE;
        goto exit;
    }

    // release file lock state; enter s_filelockstate.cs in AcquireFileLockState
    LeaveCriticalSection(pstm->s_filelockstate.lpcs);

    fRet = TRUE;

exit:

    FATExitQuick();

    return fRet;
}

DWORD CopyFileExternal(PDSTREAM pstm, PFILE_COPY_EXTERNAL pInCopyReq, LPVOID lpOutBuf, DWORD nOutBufSize)
{
    DWORD dwRuns = 0;
    PDISK_COPY_EXTERNAL pOutCopyReq = NULL;
    PSECTOR_LIST_ENTRY pEntry, pSectorList;
    PVOLUME pvol = pstm->s_pvol;
    DWORD dwSize;
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwPosition = 0;

    // Validate input parameters
    if (!pInCopyReq->pUserData && pInCopyReq->cbUserDataSize) {
        return ERROR_INVALID_PARAMETER;
    }

    FATEnterQuick();
    EnterCriticalSection(&pstm->s_cs);    
       
    // Determine the number of sector runs
    RewindStream (pstm, INVALID_POS);
    while (UnpackRun (pstm) != ERROR_HANDLE_EOF) {
        dwRuns++;    
    }

    dwSize = sizeof(DISK_COPY_EXTERNAL) + dwRuns * sizeof(SECTOR_LIST_ENTRY);
    if ((dwSize - sizeof(DISK_COPY_EXTERNAL)) / dwRuns != sizeof(SECTOR_LIST_ENTRY)) {
        // Integer overflow
        dwError = ERROR_INVALID_PARAMETER;
        goto exit;
    }
        
    pOutCopyReq = HeapAlloc (hHeap, 0, dwSize);
    if (!pOutCopyReq) {
        DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!CopyFileExternal: Out of memory\r\n")));
        dwError = ERROR_OUTOFMEMORY;
        goto exit;
    }

    // Initialize the output copy request structure by copying the input
    pOutCopyReq->cbSize = sizeof(DISK_COPY_EXTERNAL);
    pOutCopyReq->dwDirection = pInCopyReq->dwDirection;
    pOutCopyReq->pUserData = pInCopyReq->pUserData;
    pOutCopyReq->cbUserDataSize = pInCopyReq->cbUserDataSize;
    _tcsncpy (pOutCopyReq->szCancelEventName, pInCopyReq->szCancelEventName, MAX_PATH-1);
    pOutCopyReq->szCancelEventName[MAX_PATH-1] = 0;

    pOutCopyReq->hCallerProc = GetCallerProcess();
    pOutCopyReq->cbSectorListSize = dwRuns * sizeof(SECTOR_LIST_ENTRY);

    // Set the file size.
    pOutCopyReq->ulFileSize.LowPart = pstm->s_size;
    pOutCopyReq->ulFileSize.HighPart = 0;

    // The sector list starts at the end of the structure.
    pSectorList = (PSECTOR_LIST_ENTRY)(pOutCopyReq + 1);
    pEntry = pSectorList;

    // Loop through each of the cluster runs and add each one to the sector list.
    RewindStream (pstm, INVALID_POS);
    dwPosition = 0;
    while (UnpackRun (pstm) != ERROR_HANDLE_EOF) {      

        RunInfo RunInfo;
        dwError = GetRunInfo (&pstm->s_runList, dwPosition, &RunInfo);
        if (dwError != ERROR_SUCCESS) {
            goto exit;
        }

        pEntry->dwStartSector = (RunInfo.StartBlock >> pvol->v_log2cblkSec) + pvol->v_secBlkBias;
        ASSERT (RunInfo.StartPosition <= RunInfo.EndPosition);
        pEntry->dwNumSectors = (RunInfo.EndPosition - RunInfo.StartPosition + pvol->v_pdsk->d_diActive.di_bytes_per_sect - 1) / 
            pvol->v_pdsk->d_diActive.di_bytes_per_sect;
        pEntry++;

        dwPosition += RunInfo.StartPosition;        
    }    

    // For a write, invalidate the cache for these sectors, since copy external will not go 
    // through the cache.  For a read, flush any dirty sectors first, so that the correct
    // data is read from disk.
    if (pvol->v_DataCacheId != INVALID_CACHE_ID) {
        if (pInCopyReq->dwDirection == COPY_EXTERNAL_READ) {
            FSDMGR_FlushCache (pvol->v_DataCacheId, pSectorList, dwRuns, 0);
        } else {
            FSDMGR_InvalidateCache (pvol->v_DataCacheId, pSectorList, dwRuns, 0);
        }
    }

    if (!FSDMGR_DiskIoControl((HDSK)pvol->v_pdsk->d_hdsk, IOCTL_DISK_COPY_EXTERNAL_START, (LPBYTE)pOutCopyReq, dwSize, lpOutBuf, nOutBufSize, NULL, NULL)) {
        dwError = GetLastError();
    }

exit:

    if (pOutCopyReq)
        HeapFree (hHeap, 0, (HLOCAL)pOutCopyReq);

    LeaveCriticalSection(&pstm->s_cs);    
    FATExitQuick();

    return dwError;
    
}

#else

BOOL FAT_LockFileEx(PFHANDLE pfh, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped)
{
    return TRUE;
}

BOOL FAT_UnlockFileEx(PFHANDLE pfh, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped)
{
    return TRUE;
}
BOOL AcquireFileLockState(DWORD dwPfh, PFILELOCKSTATE *ppFileLockState)
{
    return TRUE;
}

BOOL ReleaseFileLockState(DWORD dwPfh, PFILELOCKSTATE *ppFileLockState)
{
    return TRUE;
}
BOOL LockPages(LPVOID lpvAddress, DWORD cbSize, PDWORD pPFNs, int fOptions)
{
    return TRUE;
}
BOOL UnlockPages(LPVOID lpvAddress, DWORD cbSize)
{
    return TRUE;
}
BOOL SetHandleOwner(HANDLE h, HANDLE hProc)
{
    return TRUE;
}

#endif


BOOL FAT_GetVolumeInfo(PVOLUME pvol, FSD_VOLUME_INFO *pInfo)
{
    pInfo->dwBlockSize = pvol->v_cbClus;
    if (pvol->v_flags & VOLF_READONLY)
        pInfo->dwAttributes = FSD_ATTRIBUTE_READONLY;
    pInfo->dwFlags = FSD_FLAG_LOCKFILE_SUPPORTED | FSD_FLAG_WFSC_SUPPORTED;
#ifdef TFAT    
    if (pvol->v_fTfat)
        pInfo->dwFlags |= FSD_FLAG_TRANSACTION_SAFE;
    if (pvol->v_flFATFS  & FATFS_TRANS_DATA)
        pInfo->dwFlags |= FSD_FLAG_TRANSACT_WRITES;
#endif        
    return TRUE;
}

