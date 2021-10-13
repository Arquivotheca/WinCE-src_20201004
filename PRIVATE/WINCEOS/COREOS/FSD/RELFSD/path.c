//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "relfsd.h"
#include "cefs.h"

DWORD RELFSD_GetFileAttributesW(PVOLUME pvol, PCWSTR pwsFileName);


/*  RELFSD_CreateDirectoryW - Create a new subdirectory
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsPathName - pointer to name of new subdirectory
 *      lpSecurityAttributes - pointer to security attributes (ignored)
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */



BOOL RELFSD_CreateDirectoryW(PVOLUME pvol, PCWSTR pwsPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    BOOL fSuccess = TRUE;
    DWORD dwAttr;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : CreateDirectory")));

    dwAttr = RELFSD_GetFileAttributesW(pvol, pwsPathName);
    if ((dwAttr != -1) && (dwAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        SetLastError(ERROR_ALREADY_EXISTS);
        return FALSE;
    }    
    EnterCriticalSectionMM(g_pcsMain);
    fSuccess = rcreatedir(pwsPathName);
    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}


/*  RELFSD_RemoveDirectoryW - Destroy an existing subdirectory
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsPathName - pointer to name of existing subdirectory
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL RELFSD_RemoveDirectoryW(PVOLUME pvol, PCWSTR pwsPathName)
{
    BOOL fSuccess = TRUE;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : RemoveDirectory")));

    EnterCriticalSectionMM(g_pcsMain);
    fSuccess = rremovedir(pwsPathName);
    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}


/*  RELFSD_GetFileAttributesW - Get file/subdirectory attributes
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsFileName - pointer to name of existing file/subdirectory
 *
 *  EXIT
 *      Attributes of file/subdirectory if it exists, 0xFFFFFFFF if it
 *      does not (call GetLastError for error code).
 */

DWORD RELFSD_GetFileAttributesW(PVOLUME pvol, PCWSTR pwsFileName)
{
    int result = -1;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : GetFileAttributesW")));

    EnterCriticalSectionMM(g_pcsMain);
    result = rgetfileattrib(pwsFileName);
    LeaveCriticalSectionMM(g_pcsMain);

    if (-1 == result) {
        SetLastError( ERROR_FILE_NOT_FOUND);
    }
    
    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : Leaving GetFileAttributesW")));

    return result;
}


/*  RELFSD_SetFileAttributesW - Set file/subdirectory attributes
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsFileName - pointer to name of existing file/subdirectory
 *      dwAttributes - new attributes for file/subdirectory
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

BOOL RELFSD_SetFileAttributesW(PVOLUME pvol, PCWSTR pwsFileName, DWORD dwAttributes)
{
    BOOL fSuccess;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : SetFileAttributesW")));

    EnterCriticalSectionMM(g_pcsMain);
    fSuccess = rsetfileattrib(pwsFileName, dwAttributes);
    LeaveCriticalSectionMM(g_pcsMain);
    
    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : Leaving SetFileAttributesW")));

    return fSuccess;
}


/*  RELFSD_DeleteFileW - Delete file
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

BOOL RELFSD_DeleteFileW(PVOLUME pvol, PCWSTR pwsFileName)
{
    BOOL fSuccess = TRUE;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : DeleteFileW ")));

    EnterCriticalSectionMM(g_pcsMain);
    fSuccess = rdelete (pwsFileName);
    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}


/*  RELFSD_MoveFileW
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

BOOL RELFSD_MoveFileW(PVOLUME pvol, PCWSTR pwsOldFileName, PCWSTR pwsNewFileName)
{
    BOOL fSuccess = TRUE;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : MoveFileW ")));

    EnterCriticalSectionMM(g_pcsMain);
    fSuccess = rmove (pwsOldFileName, pwsNewFileName);
    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}


BOOL RELFSD_RegisterFileSystemFunction(PVOLUME pvol, SHELLFILECHANGEFUNC_t pfn)
{
    BOOL fSuccess = TRUE;
    DWORD dwError =ERROR_NOT_SUPPORTED;
    
    if (dwError) {
        SetLastError( dwError);
    }
    return fSuccess;
}


BOOL RELFSD_DeleteAndRenameFileW(PVOLUME pvol, PCWSTR pwsOldFile, PCWSTR pwsNewFile)
{
    BOOL fSuccess = TRUE;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : DeleteAndRenameFileW ")));

    EnterCriticalSectionMM(g_pcsMain);
    fSuccess = rmove (pwsOldFile, pwsNewFile);
    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}


BOOL RELFSD_GetDiskFreeSpaceW(
PVOLUME pvol,
PCWSTR pwsPathName,
PDWORD pSectorsPerCluster,
PDWORD pBytesPerSector,
PDWORD pFreeClusters,
PDWORD pClusters)
{
    BOOL fSuccess = TRUE;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : GetDiskFreeSpaceW ")));

    EnterCriticalSectionMM(g_pcsMain);
    fSuccess = rgetdiskfree (pwsPathName, pSectorsPerCluster, pBytesPerSector,
        pFreeClusters, pClusters);
    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}


/*  RELFSD_Notify - Power off/device on notification handler
 *
 *  ENTRY
 *      pvol - pointer to VOLUME (NULL if none)
 *      dwFlags - notification flags (see FSNOTIFY_*)
 *
 *  EXIT
 *      None
 */

void RELFSD_Notify(PVOLUME pvol, DWORD dwFlags)
{
    return;
}


