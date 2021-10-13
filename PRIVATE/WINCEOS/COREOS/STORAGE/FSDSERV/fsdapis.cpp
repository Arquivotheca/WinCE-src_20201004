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

    apis.c

Abstract:

    This file contains the FSDMGR entry points for all supported file
    system APIs.

--*/

#define KEEP_SYSCALLS           // for kernel.h only
#define NO_DEFINE_KCALL         // for kernel.h only

#include "fsdmgrp.h"
#include "fsnotify.h"
#include "storemain.h"
#include "cdioctl.h"


inline BOOL IsValidVolume(PVOL pVol) 
{
    ASSERT(OWNCRITICALSECTION(&csFSD));
    
    // walk the global vol list to verify pVol has not been deregistered
    PVOL pVolNext = dlVOLList.pVolNext;
    while (pVolNext != (PVOL)&dlVOLList) {
        if (pVol == pVolNext) {
            return TRUE;
        }
        pVolNext = pVolNext->dlVol.pVolNext;
    }
    return FALSE;
}

void inline MarkVolume( PVOL pVol, DWORD dwFlags) 
{
    EnterCriticalSection(&csFSD);
    __try {
        if (IsValidVolume(pVol)) {
            pVol->dwFlags  |= dwFlags;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        ASSERT(0);
    }
    LeaveCriticalSection( &csFSD);
}    

void inline UnmarkVolume( PVOL pVol, DWORD dwFlags) 
{
    EnterCriticalSection(&csFSD);
    __try {
        if (IsValidVolume(pVol)) {
            pVol->dwFlags  &= ~dwFlags;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        ASSERT(0);
    }
    LeaveCriticalSection( &csFSD);
}    


void inline WaitForHandle(PHDL pHdl)
{
    BOOL fDelay = FALSE;
    extern DWORD g_dwWaitIODelay;    
    
    EnterCriticalSection(&csFSD);
    __try {
        if (!(pHdl->dwFlags & HDL_INVALID) && pHdl->pVol) {
            ASSERT(VALIDSIG(pHdl->pVol, VOL_SIG));
            // if the volume is currently unavailable, stall
            fDelay = (pHdl->pVol->dwFlags & VOL_UNAVAILABLE);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        ASSERT(0);
    }
    LeaveCriticalSection(&csFSD);
    
    if (fDelay) {
        // stall in case the device could come back online
        Sleep(g_dwWaitIODelay);
    }   
}

/*  TryEnterVolume - Tracks threads entering an FSD volume API
 *
 *  ENTRY
 *      pVol -> VOL structure
 *
 *  EXIT
 *      Returns TRUE if the volume can be entered, FALSE if the volume
 *      is not available.
 *
 *  NOTES
 *      We track threads on a per-volume basis so that we will block
 *      volume deinitialization until all threads have stopped accessing
 *      the volume.
 */

BOOL TryEnterVolume(PVOL pVol)
{
    BOOL bRet = FALSE;
    HANDLE hActivityEvent = NULL;
    EnterCriticalSection(&csFSD);
    DWORD dwErr;
    
    __try {
        // check to see that the volume is available
        if (!IsValidVolume(pVol) || (pVol->dwFlags & VOL_DEINIT)) {
            // Indicate to the caller that the device is has been permanently removed.
            dwErr = ERROR_DEVICE_REMOVED;

        } else if (pVol->dwFlags & VOL_UNAVAILABLE) {
            // Indicate to the caller that the device is temporarily not available, but
            // might come back online. This will be the case during a suspend/resume 
            // sequence, or if a device was removed but the PNPUnloadDelay timeout has 
            // not yet been exhausted.
            dwErr = ERROR_DEVICE_NOT_AVAILABLE;

        } else {

            ASSERT(VALIDSIG(pVol, VOL_SIG));
            hActivityEvent = pVol->pDsk->hActivityEvent;

            // indicate that a new thread has entered the volume
            pVol->cThreads++;
            bRet = TRUE;
        }
        
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_DEVICE_REMOVED;
    }
    LeaveCriticalSection( &csFSD);

    if (bRet && hActivityEvent) {
        // Set the Activity timer every time a volume is entered 
        // so that Power Manager will know this device is being used
        SetEvent(hActivityEvent);
    }

    if (!bRet) {
        // set the appropriate error code on failure
        SetLastError(dwErr);
    }    
    
    return bRet;
}    


/*  TryEnterHandle - Tracks threads entering an FSD handle API
 *
 *  ENTRY
 *      pHdl -> PHDL structure
 *
 *  EXIT
 *      Returns TRUE if the volume can be entered and the handle is
 *      valid, FALSE if the volume is not available.
 *
 *  NOTES
 *      We track threads on a per-volume basis so that we will block
 *      volume deinitialization until all threads have stopped accessing
 *      the volume.
 */

BOOL TryEnterHandle(PHDL pHdl)
{
    BOOL bRet = FALSE;
    HANDLE hActivityEvent = NULL;
    DWORD dwErr;

    EnterCriticalSection(&csFSD);
    __try {
        // check to see that the handle is valid and the volume is available
        // we don't need to call IsValidVolume here because it will have 
        // been set to NULL (and the handle will have been flagged INVALID)
        // during cleanup if the volume went away
        if ((pHdl->dwFlags & HDL_INVALID) || (pHdl->pVol->dwFlags & VOL_DEINIT)) {
            // Indicate to the caller that the device is has been permanently removed.
            dwErr = ERROR_DEVICE_REMOVED;

        } else if (pHdl->pVol->dwFlags & VOL_UNAVAILABLE) {
            // Indicate to the caller that the device is temporarily not available, but
            // might come back online. This will be the case during a suspend/resume 
            // sequence, or if a device was removed but the PNPUnloadDelay timeout has 
            // not yet been exhausted.
            dwErr = ERROR_DEVICE_NOT_AVAILABLE;

        } else {            
            ASSERT(VALIDSIG(pHdl->pVol, VOL_SIG));
            hActivityEvent = pHdl->pVol->pDsk->hActivityEvent;

            // indicate that a new thread has entered the volume
            pHdl->pVol->cThreads++;
            bRet = TRUE;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_DEVICE_REMOVED;
    }
    LeaveCriticalSection(&csFSD);
    
    if (bRet && hActivityEvent) {
        // set the Activity timer every time a volume is entered 
        // so that Power Manager will know this device is being used
        SetEvent(hActivityEvent);
    }
    
    if (!bRet) {
        // set the appropriate error code on failure
        SetLastError(dwErr);
    }
    
    return bRet;
}    

/*  ExitVolume - Tracks threads leaving an FSD (and can signal when the last one leaves)
 *
 *  ENTRY
 *      pVol -> VOL structure
 *
 *  EXIT
 *      None
 *
 *  NOTES
 *      See EnterVolume for notes.
 */

void ExitVolume(PVOL pVol)
{
    ASSERT(VALIDSIG(pVol, VOL_SIG));

    EnterCriticalSection(&csFSD);
    __try {
        pVol->cThreads--;

        if ((VOL_DEINIT & pVol->dwFlags) && (pVol->cThreads == 0)) {
            // this is the last thread leaving a de-allocated volume, so signal 
            // the final thread exit event so that volume cleanup will complete
            ASSERT(pVol->hThreadExitEvent != NULL);
            SetEvent(pVol->hThreadExitEvent);
        } 
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        ASSERT(0);
    }
    LeaveCriticalSection(&csFSD);
}

/*  ExitHandle - Tracks threads leaving an FSD handle call
 *
 *  ENTRY
 *      pHdl -> PHDL structure
 *
 *  EXIT
 *      None
 *
 *  NOTES
 *      See TryEnterHandle for notes.
 */

inline void ExitHandle(PHDL pHdl)
{
    // just exit the volume, there is no special exit code for handles
    ExitVolume(pHdl->pVol);
}

/*  FSDMGR_CloseVolume - Called when a volume is being deregistered
 *
 *  ENTRY
 *      pVol -> VOL structure
 *
 *  EXIT
 *      Always returns TRUE (that's what OUR stub always does, and it's
 *      expected that that's what FSDs will do too, if they even care to hook
 *      this entry point).
 */

BOOL FSDMGR_CloseVolume(PVOL pVol)
{
    BOOL f=FALSE;
    if (!pVol) {
        SetLastError(ERROR_DEVICE_NOT_AVAILABLE);
    } else {
        __try {
            f =  ((PCLOSEVOLUME)pVol->pDsk->pFSD->apfnAFS[AFSAPI_CLOSEVOLUME])(pVol->dwVolData);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_ACCESS_DENIED);
        }
    }    
    return f;
}


/*  FSDMGR_CreateDirectoryW - Create a new subdirectory
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      pwsPathName - pointer to name of new subdirectory
 *      pSecurityAttributes - pointer to security attributes (ignored)
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FSDMGR_CreateDirectoryW(PVOL pVol, PCWSTR pwsPathName, PSECURITY_ATTRIBUTES pSecurityAttributes)
{
    BOOL f=FALSE;

    if (TryEnterVolume(pVol)) {
        __try { 
            f = ((PCREATEDIRECTORYW)pVol->pDsk->pFSD->apfnAFS[AFSAPI_CREATEDIRECTORYW])(pVol->dwVolData, pwsPathName, pSecurityAttributes);
            if (f && pVol->hNotifyHandle) {
                NotifyPathChange(pVol->hNotifyHandle, pwsPathName, TRUE, FILE_ACTION_ADDED);
            }        
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_ACCESS_DENIED);
        }
        ExitVolume(pVol);
    }    
    return f;
}


/*  FSDMGR_RemoveDirectoryW - Destroy an existing subdirectory
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      pwsPathName - pointer to name of existing subdirectory
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

/*  FSDMGR_RemoveDirectoryW - Destroy an existing subdirectory
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      pwsPathName - pointer to name of existing subdirectory
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FSDMGR_RemoveDirectoryW(PVOL pVol, PCWSTR pwsPathName)
{
    BOOL f=FALSE;

    if (TryEnterVolume(pVol)) {
        __try {
            f = ((PREMOVEDIRECTORYW)pVol->pDsk->pFSD->apfnAFS[AFSAPI_REMOVEDIRECTORYW])(pVol->dwVolData, pwsPathName);
            if (f && pVol->hNotifyHandle) {
                NotifyPathChange(pVol->hNotifyHandle, pwsPathName, TRUE, FILE_ACTION_REMOVED);
            }        
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_ACCESS_DENIED);
        }
        ExitVolume(pVol);
    }    
    return f;
}


/*  FSDMGR_GetFileAttributesW - Get file/subdirectory attributes
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      pwsFileName - pointer to name of existing file/subdirectory
 *
 *  EXIT
 *      Attributes of file/subdirectory if it exists, 0xFFFFFFFF if it
 *      does not (call GetLastError for error code).
 */

DWORD FSDMGR_GetFileAttributesW(PVOL pVol, PCWSTR pwsFileName)
{
    DWORD dw=0xffffffff;

     if (TryEnterVolume(pVol)) {
        __try {
            if (wcscmp( pwsFileName, L"\\") == 0) {
                dw = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_TEMPORARY;
            } else {    
                dw = ((PGETFILEATTRIBUTESW)pVol->pDsk->pFSD->apfnAFS[AFSAPI_GETFILEATTRIBUTESW])(pVol->dwVolData, pwsFileName);
            }    
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_ACCESS_DENIED);
            dw = -1;
        }
        ExitVolume(pVol);
    }    
    return dw;
}


/*  FSDMGR_SetFileAttributesW - Set file/subdirectory attributes
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      pwsFileName - pointer to name of existing file/subdirectory
 *      dwAttributes - new attributes for file/subdirectory
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FSDMGR_SetFileAttributesW(PVOL pVol, PCWSTR pwsFileName, DWORD dwAttributes)
{
    BOOL f=FALSE;

    if (TryEnterVolume(pVol)) {
        __try {
            f = ((PSETFILEATTRIBUTESW)pVol->pDsk->pFSD->apfnAFS[AFSAPI_SETFILEATTRIBUTESW])(pVol->dwVolData, pwsFileName, dwAttributes);            
            if (f && pVol->hNotifyHandle) {
                NotifyPathChange(pVol->hNotifyHandle, pwsFileName, FALSE, FILE_ACTION_MODIFIED);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_ACCESS_DENIED);
        }
        ExitVolume(pVol);
    }    
    return f;
}


/*  FSDMGR_DeleteFileW - Delete file
 *
 *  ENTRY
 *      pVol -> VOL structure
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

BOOL FSDMGR_DeleteFileW(PVOL pVol, PCWSTR pwsFileName)
{
    BOOL f = FALSE;

    if (TryEnterVolume(pVol)) {
        __try {
            f = ((PDELETEFILEW)pVol->pDsk->pFSD->apfnAFS[AFSAPI_DELETEFILEW])(pVol->dwVolData, pwsFileName);
            if (f && pVol->hNotifyHandle) {
                NotifyPathChange(pVol->hNotifyHandle, pwsFileName, FALSE, FILE_ACTION_REMOVED);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_ACCESS_DENIED);
        }
        ExitVolume(pVol);
    }    
    return f;
}


/*  FSDMGR_MoveFileW
 *
 *  ENTRY
 *      pVol -> VOL structure
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

BOOL FSDMGR_MoveFileW(PVOL pVol, PCWSTR pwsOldFileName, PCWSTR pwsNewFileName)
{
    BOOL f = FALSE;

    if (TryEnterVolume(pVol)) {
        __try {
            f = ((PMOVEFILEW)pVol->pDsk->pFSD->apfnAFS[AFSAPI_MOVEFILEW])(pVol->dwVolData, pwsOldFileName, pwsNewFileName);
            if (f && pVol->hNotifyHandle) {
                NotifyMoveFile(pVol->hNotifyHandle, pwsOldFileName, pwsNewFileName);
            }        
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_ACCESS_DENIED);
        }
        ExitVolume(pVol);
    }    
    return f;
}


/*  FSDMGR_DeleteAndRenameFileW
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      pwsDestFileName - pointer to name of existing file
 *      pwsSourceFileName - pointer to name of file to be renamed to existing file
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FSDMGR_DeleteAndRenameFileW(PVOL pVol, PCWSTR pwsDestFileName, PCWSTR pwsSourceFileName)
{
    BOOL f=FALSE;

    if (TryEnterVolume(pVol)) {
        __try {
            f = ((PDELETEANDRENAMEFILEW)pVol->pDsk->pFSD->apfnAFS[AFSAPI_PRESTOCHANGOFILENAME])(pVol->dwVolData, pwsDestFileName, pwsSourceFileName);
            if (f && pVol->hNotifyHandle) {
                NotifyPathChange( pVol->hNotifyHandle, pwsDestFileName, FALSE, FILE_ACTION_REMOVED);
                NotifyMoveFile(pVol->hNotifyHandle, pwsSourceFileName, pwsDestFileName);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_ACCESS_DENIED);
        }
        ExitVolume(pVol);
    }    
    return f;
}


/*  FSDMGR_GetFreeDiskSpaceW
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      pwsPathName -> volume name (eg, "\Storage Card")
 *      pSectorsPerCluster -> DWORD to receive sectors/cluster
 *      pBytesPerSector -> DWORD to receive bytes/sector
 *      pFreeClusters -> DWORD to receive available clusters on volume
 *      pClusters -> DWORD to receive total clusters on volume
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FSDMGR_GetDiskFreeSpaceW(PVOL pVol, PCWSTR pwsPathName, PDWORD pSectorsPerCluster, PDWORD pBytesPerSector, PDWORD pFreeClusters, PDWORD pClusters)
{
    BOOL f = FALSE;

    if (TryEnterVolume(pVol)) {
        __try {
            f = ((PGETDISKFREESPACEW)pVol->pDsk->pFSD->apfnAFS[AFSAPI_GETDISKFREESPACE])(pVol->dwVolData, pwsPathName, pSectorsPerCluster, pBytesPerSector, pFreeClusters, pClusters);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_ACCESS_DENIED);
        }
        ExitVolume(pVol);
    }    
    return f;
}


/*  FSDMGR_CloseAllFiles (no FSD equivalent)
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      hProc == handle of terminating process
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FSDMGR_CloseAllFiles(PVOL pVol, HANDLE hProc)
{
    return TRUE;
}


/*  FSDMGR_CommitAllFiles (no FSD equivalent)
 *
 *  ENTRY
 *      pVol -> VOL structure
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FSDMGR_CommitAllFiles(PVOL pVol)
{
    PHDL pHdl;

    if (TryEnterVolume(pVol)) {
        __try {
            pHdl = pVol->dlHdlList.pHdlNext;
            while (pHdl != (PHDL)&pVol->dlHdlList) {
                ((PFLUSHFILEBUFFERS)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_FLUSHFILEBUFFERS])(pHdl->dwHdlData);
                pHdl = pHdl->dlHdl.pHdlNext;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_ACCESS_DENIED);
        }
        ExitVolume(pVol);
    }    
    return TRUE;
}


/*  FSDMGR_Notify - FSD notification handler
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      dwFlags == one or more FSNOTIFY_* flags
 *
 *  EXIT
 *      None.
 *
 *  NOTES
 *      This function is invoked once per registered volume every time the
 *      power goes off, comes back, or devices have finished initializing.  I'm
 *      only interested in hooking the FSNOTIFY_POWER_ON notification, so that I
 *      can flag all relevant disks as UNCERTAIN, but unfortunately, I cannot
 *      avoid being called for all events, not to mention multiple times per disk
 *      if there are multiple volumes per disk.
 */

void FSDMGR_Notify(PVOL pVol, DWORD dwFlags)
{
    extern DWORD g_dwWaitIODelay;
    ASSERT(VALIDSIG(pVol, VOL_SIG));

    if (dwFlags & FSNOTIFY_POWER_OFF) {
//        EnterCriticalSection(&csFSD);
        DEBUGMSG( ZONE_APIS, (L"FSDMGR_Notfiy - PowerDown on volume %08X\r\n", pVol));
        MarkVolume(pVol, VOL_POWERDOWN);
        ResetEvent( g_hResumeEvent);
        FSDMGR_CommitAllFiles(pVol);        // NOTE: This must come before setting the powerdown flag !!!
    } else 
    if (dwFlags & FSNOTIFY_POWER_ON) {
        DEBUGMSG( ZONE_APIS, (L"FSDMGR_Notfiy - Powerup on volume %08X\r\n", pVol));
        UnmarkVolume(pVol, VOL_POWERDOWN);
//        LeaveCriticalSection(&csFSD);
        SetEvent( g_hResumeEvent);
    }

    // Now pass the notification on to the FSD as well; in general, they shouldn't
    // need to do very much on these events, because we've tried to take care of the
    // hard problems already, and these events need to be responsive.

    // However, one common requirement is flushing all non-handle-based data (eg,
    // dirty buffers) on the FSNOTIFY_POWER_OFF event.  We could ask the FSDs to
    // export a special "Flush" interface that we can call from here, but that seems
    // like a pointless abstraction, especially given that other interesting FSNOTIFY
    // events may be added in the future and I'd rather not have to rev FSDMGR just
    // for that.

    



    ((PNOTIFY)pVol->pDsk->pFSD->apfnAFS[AFSAPI_NOTIFY])(pVol->dwVolData, dwFlags);
}


/*  FSDMGR_RegisterFileSystemFunction
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      pfn -> function address to call
 *
 *  EXIT
 *      TRUE if supported, FALSE if not
 */

BOOL FSDMGR_RegisterFileSystemFunction(PVOL pVol, SHELLFILECHANGEFUNC_t pfn)
{
    BOOL f=FALSE;

    if (TryEnterVolume(pVol)) {
         __try {
            f = ((PREGISTERFILESYSTEMFUNCTION)pVol->pDsk->pFSD->apfnAFS[AFSAPI_REGISTERFILESYSTEMFUNCTION])(pVol->dwVolData, pfn);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_ACCESS_DENIED);
        }
        ExitVolume(pVol);
    }    
    return f;
}


/*  FSDMGR_FindFirstFileW
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      hProc == handle of originating process
 *      pwsFileSpec -> name of file(s), wildcards allowed
 *      pfd -> WIN32_FIND_DATAW to be filled in on success
 *
 *  EXIT
 *      HANDLE to be used with subsequent FindNextFileW() and FindClose() APIs,
 *      or INVALID_HANDLE_VALUE if error.
 */

HANDLE FSDMGR_FindFirstFileW(PVOL pVol, HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd)
{
    HANDLE h=INVALID_HANDLE_VALUE;

    // Check caller buffer access
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pfd = (PWIN32_FIND_DATAW)MapCallerPtr(pfd, sizeof(WIN32_FIND_DATA));
    }

    if (TryEnterVolume(pVol)) {
        __try {
            h = ((PFINDFIRSTFILEW)pVol->pDsk->pFSD->apfnAFS[AFSAPI_FINDFIRSTFILEW])(pVol->dwVolData, hProc, pwsFileSpec, pfd);
            if (h != INVALID_HANDLE_VALUE) {
                h = AllocFSDHandle(pVol, hProc, NULL, (DWORD)h, HDL_SEARCH);
            }   
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            h = INVALID_HANDLE_VALUE;
            SetLastError(ERROR_ACCESS_DENIED);
        }
        ExitVolume(pVol);
    }    
    return h;
}


/*  FSDMGR_FindNextFileW
 *
 *  ENTRY
 *      pHdl -> HDL structure
 *      pfd -> WIN32_FIND_DATAW to be filled in on success
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FSDMGR_FindNextFileW(PHDL pHdl, PWIN32_FIND_DATAW pfd)
{
    BOOL f=FALSE;

    ASSERT(VALIDSIG(pHdl, HSEARCH_SIG));

    // Check caller buffer access
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pfd = (PWIN32_FIND_DATAW)MapCallerPtr(pfd, sizeof(WIN32_FIND_DATA));
    }

    if (pHdl->dwFlags & HDL_SEARCH) {
        WaitForHandle(pHdl);
        if (TryEnterHandle(pHdl)) {
            __try {
                f = ((PFINDNEXTFILEW)pHdl->pVol->pDsk->pFSD->apfnFind[FINDAPI_FINDNEXTFILEW])(pHdl->dwHdlData, pfd);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }    
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return f;
}


/*  FSDMGR_FindClose
 *
 *  ENTRY
 *      pHdl -> HDL structure
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL FSDMGR_FindClose(PHDL pHdl)
{
    BOOL f = TRUE;
 
    ASSERT(VALIDSIG(pHdl, HSEARCH_SIG));
        
    if (pHdl->dwFlags & HDL_SEARCH) {
        WaitForHandle(pHdl);
        if (TryEnterHandle(pHdl)) {
            __try {
                f = ((PFINDCLOSE)pHdl->pVol->pDsk->pFSD->apfnFind[FINDAPI_FINDCLOSE])(pHdl->dwHdlData);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }                
            ExitHandle(pHdl);
        }    
    }
    
    if (f) {
        DeallocFSDHandle(pHdl);
    }
        
    return f;
}


/*  FSDMGR_CreateFileW
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      hProc == handle of originating process
 *      pwsFileName
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

HANDLE FSDMGR_CreateFileW(PVOL pVol, HANDLE hProc, PCWSTR pwsFileName, DWORD dwAccess, DWORD dwShareMode, PSECURITY_ATTRIBUTES pSecurityAttributes, DWORD dwCreate, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    HANDLE h=INVALID_HANDLE_VALUE;
    BOOL bDevice = FALSE;
    HANDLE hNotify = NULL;

    if (TryEnterVolume(pVol)) {
        __try {
            DWORD dwAttr = FSDMGR_GetFileAttributesW(pVol, pwsFileName);
            h = ((PCREATEFILEW)pVol->pDsk->pFSD->apfnAFS[AFSAPI_CREATEFILEW])(pVol->dwVolData, hProc, pwsFileName, dwAccess, dwShareMode, pSecurityAttributes, dwCreate, dwFlagsAndAttributes, hTemplateFile);
            if (h != INVALID_HANDLE_VALUE) {        
                if (pwsFileName[wcslen(pwsFileName)-1] == L':') {
                    bDevice = TRUE;
                }
				if (!bDevice && pVol->hNotifyHandle) {
                    if (-1 == dwAttr) {
                        // new file was added because it did not exist prior to calling CreateFile
                        NotifyPathChange( pVol->hNotifyHandle, pwsFileName, FALSE, FILE_ACTION_ADDED);
					} else if ((TRUNCATE_EXISTING == dwCreate) || (CREATE_ALWAYS == dwCreate)){
                        // existing file was re-created
						NotifyPathChange( pVol->hNotifyHandle, pwsFileName, FALSE, FILE_ACTION_MODIFIED);
					}
					// else: an existing file was opened, so don't send a notification

					// create a notification handle to associate with the file handle
                        hNotify  = NotifyCreateFile(pVol->hNotifyHandle, pwsFileName);
				}
#ifndef DEBUG
                h = AllocFSDHandle(pVol, hProc, hNotify, (DWORD)h, HDL_FILE);
#else
                if ((wcsicmp(pwsFileName, L"\\reg:") == 0) || (wcsicmp( pwsFileName, L"\\con") == 0)) {
                    h = AllocFSDHandle(pVol, hProc, hNotify, (DWORD)h, HDL_FILE | HDL_CONSOLE);
                } else {
                    h = AllocFSDHandle(pVol, hProc, hNotify, (DWORD)h, HDL_FILE);
                }
#endif
            } else {
                if (pwsFileName && (wcscmp( pwsFileName, L"\\VOL:") == 0)) {
                    h = AllocFSDHandle( pVol, hProc, NULL, 0, HDL_PSUEDO);
                }
            }    
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            h = INVALID_HANDLE_VALUE;
            SetLastError(ERROR_ACCESS_DENIED);
        }
        ExitVolume(pVol);
    }    
    return h;
}


/*  FSDMGR_ReadFile
 */

BOOL FSDMGR_ReadFile(PHDL pHdl, PVOID pBuffer, DWORD cbRead, PDWORD pcbRead, OVERLAPPED *pOverlapped)
{
    BOOL f = FALSE;

#ifdef UNDER_CE
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pBuffer = MapCallerPtr(pBuffer, cbRead);
        pcbRead = (PDWORD)MapCallerPtr(pcbRead, sizeof(DWORD));
    }
#endif
    if (pHdl->dwFlags & HDL_FILE) {
        WaitForHandle(pHdl);
        if (TryEnterHandle(pHdl)) {
            __try {
                f = ((PREADFILE)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_READFILE])(pHdl->dwHdlData, pBuffer, cbRead, pcbRead, pOverlapped);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }    
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return f;
}


/*  FSDMGR_ReadFileWithSeek
 */

BOOL FSDMGR_ReadFileWithSeek(PHDL pHdl, PVOID pBuffer, DWORD cbRead, PDWORD pcbRead, OVERLAPPED *pOverlapped, DWORD dwLowOffset, DWORD dwHighOffset)
{
    BOOL f = FALSE;

#ifdef UNDER_CE
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pBuffer = MapCallerPtr(pBuffer, cbRead);
        pcbRead = (PDWORD)MapCallerPtr(pcbRead, sizeof(DWORD));
    }
#endif
    if (pHdl->dwFlags & HDL_FILE) {
        if (TryEnterHandle(pHdl)) {            
            __try {
                if (pBuffer == NULL && cbRead == 0 && !(pHdl->pVol->pDsk->pFSD->dwFlags & FSD_FLAGS_PAGEABLE)) {
                    DEBUGMSG( ZONE_APIS, (L"FSDMGR_ReadFileWithSeek pDsk=%08X - Reporting back as paging not supported !!!\r\n", pHdl->pVol->pDsk));
                    SetLastError(ERROR_NOT_SUPPORTED);
                    f = FALSE;
                } else {
                    f = ((PREADFILEWITHSEEK)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_READFILEWITHSEEK])(pHdl->dwHdlData, pBuffer, cbRead, pcbRead, pOverlapped, dwLowOffset, dwHighOffset);
                }    
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }    
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return f;
}


/*  FSDMGR_WriteFile
 */

BOOL FSDMGR_WriteFile(PHDL pHdl, PCVOID pBuffer, DWORD cbWrite, PDWORD pcbWritten, OVERLAPPED *pOverlapped)
{
    BOOL f = FALSE;

#ifdef UNDER_CE
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pBuffer = MapCallerPtr((PVOID)pBuffer, cbWrite);
        pcbWritten = (PDWORD)MapCallerPtr(pcbWritten, sizeof(DWORD));
    }
#endif    
    if (pHdl->dwFlags & HDL_FILE) {
        WaitForHandle(pHdl);
        if (TryEnterHandle(pHdl)) {
            __try {
                f = ((PWRITEFILE)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_WRITEFILE])(pHdl->dwHdlData, pBuffer, cbWrite, pcbWritten, pOverlapped);
                if (f && pHdl->pVol->hNotifyHandle && pHdl->hNotify) {
                    NotifyHandleChange(pHdl->pVol->hNotifyHandle, pHdl->hNotify, FILE_NOTIFY_CHANGE_LAST_WRITE);
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
                }
            ExitHandle(pHdl);
        }    
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return f;
}


/*  FSDMGR_WriteFileWithSeek
 */

BOOL FSDMGR_WriteFileWithSeek(PHDL pHdl, PCVOID pBuffer, DWORD cbWrite, PDWORD pcbWritten, OVERLAPPED *pOverlapped, DWORD dwLowOffset, DWORD dwHighOffset)
{
    BOOL f = FALSE;

#ifdef UNDER_CE
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pBuffer = MapCallerPtr((PVOID)pBuffer, cbWrite);
        pcbWritten = (PDWORD)MapCallerPtr(pcbWritten, sizeof(DWORD));
    }
#endif

    // do NOT call WaitForHandle on ReadFileWithSeek/WriteFileWithSeek
    if (pHdl->dwFlags & HDL_FILE) {
        if (TryEnterHandle(pHdl)) {
            __try {
                f = ((PWRITEFILEWITHSEEK)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_WRITEFILEWITHSEEK])(pHdl->dwHdlData, pBuffer, cbWrite, pcbWritten, pOverlapped, dwLowOffset, dwHighOffset);
                if (f && pHdl->pVol->hNotifyHandle && pHdl->hNotify) {
                    NotifyHandleChange(pHdl->pVol->hNotifyHandle, pHdl->hNotify, FILE_NOTIFY_CHANGE_LAST_WRITE);
                }            
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }    
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return f;
}


/*  FSDMGR_SetFilePointer
 */

DWORD FSDMGR_SetFilePointer(PHDL pHdl, LONG lDistanceToMove, PLONG pDistanceToMoveHigh, DWORD dwMoveMethod)
{
    DWORD dw=(DWORD)-1; // Should be INVALID_SET_FILE_POINTER but this is not defined in CE headers yet

    // Check caller buffer access
#ifdef UNDER_CE    
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pDistanceToMoveHigh = (PLONG)MapCallerPtr(pDistanceToMoveHigh, sizeof(LONG));
    }
#endif    
    if (pHdl->dwFlags & HDL_FILE) {
        WaitForHandle(pHdl);       
        if (TryEnterHandle(pHdl)) {        
            __try {
                dw = ((PSETFILEPOINTER)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_SETFILEPOINTER])(pHdl->dwHdlData, lDistanceToMove, pDistanceToMoveHigh, dwMoveMethod);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                dw = -1;
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }    
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return dw;
}


/*  FSDMGR_GetFileSize
 */

DWORD FSDMGR_GetFileSize(PHDL pHdl, PDWORD pFileSizeHigh)
{
    DWORD dw=INVALID_FILE_SIZE;
    // Check caller buffer access
#ifdef UNDER_CE    
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pFileSizeHigh = (PDWORD)MapCallerPtr(pFileSizeHigh, sizeof(DWORD));
    }
#endif
    if (pHdl->dwFlags & HDL_FILE) {
        WaitForHandle(pHdl);               
        if (TryEnterHandle(pHdl)) {            
            __try {
                dw = ((PGETFILESIZE)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_GETFILESIZE])(pHdl->dwHdlData, pFileSizeHigh);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                dw = INVALID_FILE_SIZE;
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return dw;
}


/*  FSDMGR_GetFileInformationByHandle
 */

BOOL FSDMGR_GetFileInformationByHandle(PHDL pHdl, PBY_HANDLE_FILE_INFORMATION pFileInfo)
{
    BOOL f=FALSE;


    // Check caller buffer access
#ifdef UNDER_CE
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pFileInfo = (PBY_HANDLE_FILE_INFORMATION)MapCallerPtr(pFileInfo, sizeof(BY_HANDLE_FILE_INFORMATION));
    }
#endif
    if (pHdl->dwFlags & HDL_FILE) {
        WaitForHandle(pHdl);
        if (TryEnterHandle(pHdl)) {
            __try {
                f = ((PGETFILEINFORMATIONBYHANDLE)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_GETFILEINFORMATIONBYHANDLE])(pHdl->dwHdlData, pFileInfo);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }   
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return f;
}


/*  FSDMGR_FlushFileBuffers
 */

BOOL FSDMGR_FlushFileBuffers(PHDL pHdl)
{
    BOOL f=FALSE;

    if (pHdl->dwFlags & HDL_FILE) {
        WaitForHandle(pHdl);
        if (TryEnterHandle(pHdl)) {
            __try {
                f = ((PFLUSHFILEBUFFERS)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_FLUSHFILEBUFFERS])(pHdl->dwHdlData);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }    
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return f;
}


/*  FSDMGR_GetFileTime
 */

BOOL FSDMGR_GetFileTime(PHDL pHdl, FILETIME *pCreation, FILETIME *pLastAccess, FILETIME *pLastWrite)
{
    BOOL f=FALSE;

#ifdef UNDER_CE
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pCreation = (FILETIME *)MapCallerPtr(pCreation, sizeof(FILETIME));
        pLastAccess = (FILETIME *)MapCallerPtr(pLastAccess, sizeof(FILETIME));
        pLastWrite = (FILETIME *)MapCallerPtr(pLastWrite, sizeof(FILETIME));
    }
#endif

    if (pHdl->dwFlags & HDL_FILE) {
        WaitForHandle(pHdl); 
        if (TryEnterHandle(pHdl)) {
            __try {
                f = ((PGETFILETIME)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_GETFILETIME])(pHdl->dwHdlData, pCreation, pLastAccess, pLastWrite);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }    
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return f;
}


/*  FSDMGR_SetFileTime
 */

BOOL FSDMGR_SetFileTime(PHDL pHdl, CONST FILETIME *pCreation, CONST FILETIME *pLastAccess, CONST FILETIME *pLastWrite)
{
    BOOL f=FALSE;

#ifdef UNDER_CE
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pCreation = (CONST FILETIME *)MapCallerPtr((PVOID)pCreation, sizeof(FILETIME));
        pLastAccess = (CONST FILETIME *)MapCallerPtr((PVOID)pLastAccess, sizeof(FILETIME));
        pLastWrite = (CONST FILETIME *)MapCallerPtr((PVOID)pLastWrite, sizeof(FILETIME));
    }
#endif    
    if (pHdl->dwFlags & HDL_FILE) {
        WaitForHandle(pHdl);
        if (TryEnterHandle(pHdl)) {            
            __try {
                f = ((PSETFILETIME)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_SETFILETIME])(pHdl->dwHdlData, pCreation, pLastAccess, pLastWrite);
                if (f && pHdl->pVol->hNotifyHandle && pHdl->hNotify) {
                    NotifyHandleChange(pHdl->pVol->hNotifyHandle, pHdl->hNotify, FILE_NOTIFY_CHANGE_LAST_WRITE);
                }            
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }    
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return f;
}


/*  FSDMGR_SetEndOfFile
 */

BOOL FSDMGR_SetEndOfFile(PHDL pHdl)
{
    BOOL f=FALSE;

    if (pHdl->dwFlags & HDL_FILE) {
        WaitForHandle(pHdl);
        if (TryEnterHandle(pHdl)) {
            __try {
                f = ((PSETENDOFFILE)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_SETENDOFFILE])(pHdl->dwHdlData);
                if (f && pHdl->pVol->hNotifyHandle && pHdl->hNotify) {
                    NotifyHandleChange(pHdl->pVol->hNotifyHandle, pHdl->hNotify, FILE_NOTIFY_CHANGE_LAST_WRITE);
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }    
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return f;
}


/*  FSDMGR_DeviceIoControl
 */

BOOL FSDMGR_DeviceIoControl(PHDL pHdl, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned, OVERLAPPED *pOverlapped)
{
    BOOL f=FALSE;


    if (pHdl->dwFlags & HDL_FILE) {
        WaitForHandle(pHdl);
        if (TryEnterHandle(pHdl)) {
            __try {
                f = ((PDEVICEIOCONTROL)pHdl->pVol->pDsk->pFSD->apfnFile[FILEAPI_DEVICEIOCONTROL])(pHdl->dwHdlData, dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, pOverlapped);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }    
    } else {
        PDSK pDsk;
        switch(dwIoControlCode) {
            case IOCTL_DISK_WRITE:
            case IOCTL_DISK_READ:
            case DISK_IOCTL_READ:
            case DISK_IOCTL_WRITE:
            case IOCTL_DISK_READ_CDROM:
            case IOCTL_CDROM_READ_SG:
                // Massage the buffers after validation in the future.  For now fail the call.
                return(FALSE);
            break;
        default:
            break;
        }
        if (TryEnterHandle(pHdl) && (pDsk = pHdl->pVol->pDsk)) {
        // pass directly to the disk
            __try {
                f = pDsk->pDeviceIoControl((DWORD)pDsk->hDsk, dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, pOverlapped);
                if (!f) {
                    if (ERROR_INVALID_HANDLE_STATE == GetLastError()) {
                        SetLastError(ERROR_DEVICE_NOT_AVAILABLE);
                    }
                }    
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }
            ExitHandle(pHdl);
        }
    }   
    return f;
}


/*  FSDMGR_CloseFile
 */

BOOL FSDMGR_CloseFile(PHDL pHdl)
{
    BOOL f = TRUE;
    PVOL pVol = pHdl->pVol;

    ASSERT(VALIDSIG(pHdl, HFILE_SIG));

    if (pHdl->dwFlags & HDL_FILE) {
        WaitForHandle(pHdl);
        if (TryEnterHandle(pHdl)) {
            if (!(pHdl->pVol->dwFlags & VOL_UNAVAILABLE)) {
                __try {
                    pHdl->dwFlags |= HDL_INVALID;
                    f = ((PCLOSEFILE)pVol->pDsk->pFSD->apfnFile[FILEAPI_CLOSEFILE])(pHdl->dwHdlData);
                    if (f) {
                        if (pHdl->hNotify && pHdl->pVol->hNotifyHandle)
                            NotifyCloseHandle(pHdl->pVol->hNotifyHandle, pHdl->hNotify);
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    SetLastError(ERROR_ACCESS_DENIED);
                }
            } else {
                // the device was probably removed
                SetLastError(ERROR_DEVICE_REMOVED);
            }
            ExitHandle(pHdl);
        }
    }

    if (f) {
        DeallocFSDHandle(pHdl);
    }
    
    return f;
}

HANDLE  FSDMGR_FindFirstChangeNotificationW(PVOL pVol, HANDLE hProc, LPCWSTR lpPathName, BOOL bWatchSubtree, DWORD dwNotifyFilter)
{
    HANDLE h = INVALID_HANDLE_VALUE;
    if (TryEnterVolume(pVol)) {
        __try {
            if (pVol->hNotifyHandle && ((wcscmp( lpPathName, L"\\") == 0) || (((PGETFILEATTRIBUTESW)pVol->pDsk->pFSD->apfnAFS[AFSAPI_GETFILEATTRIBUTESW])(pVol->dwVolData, lpPathName) != -1))) {
                h = NotifyCreateEvent(pVol->hNotifyHandle, hProc, lpPathName, bWatchSubtree, dwNotifyFilter);
            }    
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            h = INVALID_HANDLE_VALUE;
        }    
        ExitVolume(pVol);
    }
    return h;
}

