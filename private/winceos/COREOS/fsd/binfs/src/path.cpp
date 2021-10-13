//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include "binfs.h"


/*  FSD_CreateDirectoryW - Create a new subdirectory
 *
 *  ENTRY
 *      pVolume - pointer to VOLUME
 *      pwsPathName - pointer to name of new subdirectory
 *      lpSecurityAttributes - pointer to security attributes (ignored)
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

extern "C" BOOL FSD_CreateDirectoryW(BinVolume *pVolume, PCWSTR pwsPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    SetLastError( ERROR_ACCESS_DENIED);
    return FALSE;
}


/*  FSD_RemoveDirectoryW - Destroy an existing subdirectory
 *
 *  ENTRY
 *      pVolume - pointer to VOLUME
 *      pwsPathName - pointer to name of existing subdirectory
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */

extern "C" BOOL FSD_RemoveDirectoryW(BinVolume *pVolume, PCWSTR pwsPathName)
{
    SetLastError( ERROR_PATH_NOT_FOUND);
    return FALSE;
}


/*  FSD_GetFileAttributesW - Get file/subdirectory attributes
 *
 *  ENTRY
 *      pVolume - pointer to VOLUME
 *      pwsFileName - pointer to name of existing file/subdirectory
 *
 *  EXIT
 *      Attributes of file/subdirectory if it exists, 0xFFFFFFFF if it
 *      does not (call GetLastError for error code).
 */

extern "C" DWORD FSD_GetFileAttributesW(BinVolume *pVolume, PCWSTR pwsFileName)
{
    DWORD dwError =ERROR_FILE_NOT_FOUND;
    BinDirList *pDirectory = pVolume->pDirectory;
    DWORD dwRet = -1; // INVALID_FILE_ATTRIBUTES; // BUGBUG: Replace this once the header has it
    
    DEBUGMSG(ZONE_API, (L"BINFS: FSD_GetFileAttributesWW\n"));
    while ((*pwsFileName == L'\\') || (*pwsFileName == L'/'))
        pwsFileName++;
    
    if (wcslen(pwsFileName) > SYSTEM_DIR_LEN) {
        if ((wcsnicmp( pwsFileName, SYSTEM_DIR, SYSTEM_DIR_LEN) == 0) && ((*(pwsFileName+SYSTEM_DIR_LEN) == L'\\') || (*(pwsFileName+SYSTEM_DIR_LEN) == L'/'))) {
            pwsFileName += SYSTEM_DIR_LEN;
            while ((*pwsFileName == L'\\') || (*pwsFileName == L'/')) {
                pwsFileName++;
            }
        }
    }
    
    while(pDirectory) {
        if (wcsicmp( pDirectory->szFileName, pwsFileName) == 0) {
            break;
        }
        pDirectory = pDirectory->pNext;
    }
    if (pDirectory) {
        dwError = ERROR_SUCCESS;
        dwRet = pDirectory->dwAttributes;
    }
        
    if (dwError) {
        SetLastError( dwError);
    }

    return dwRet;
}


/*  FSD_SetFileAttributesW - Set file/subdirectory attributes
 *
 *  ENTRY
 *      pVolume - pointer to VOLUME
 *      pwsFileName - pointer to name of existing file/subdirectory
 *      dwAttributes - new attributes for file/subdirectory
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (call GetLastError for error code)
 */
    
extern "C" BOOL FSD_SetFileAttributesW(BinVolume *pVolume, PCWSTR pwsFileName, DWORD dwAttributes)
{
    SetLastError( ERROR_ACCESS_DENIED);
    return FALSE;
}


/*  FSD_DeleteFileW - Delete file
 *
 *  ENTRY
 *      pVolume - pointer to VOLUME
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

extern "C" BOOL FSD_DeleteFileW(BinVolume *pVolume, PCWSTR pwsFileName)
{
    SetLastError( ERROR_ACCESS_DENIED);
    return FALSE;
}


/*  FSD_MoveFileW
 *
 *  ENTRY
 *      pVolume - pointer to VOLUME
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

extern "C" BOOL FSD_MoveFileW(BinVolume *pVolume, PCWSTR pwsOldFileName, PCWSTR pwsNewFileName)
{
    SetLastError( ERROR_ACCESS_DENIED);
    return FALSE;
}


extern "C" BOOL FSD_RegisterFileSystemFunction(BinVolume *pVolume, SHELLFILECHANGEFUNC_t pfn)
{
    return TRUE;
}


extern "C" BOOL FSD_DeleteAndRenameFileW(BinVolume *pVolume, PCWSTR pwsOldFile, PCWSTR pwsNewFile)
{
    SetLastError( ERROR_ACCESS_DENIED);
    return FALSE;
}


extern "C" BOOL FSD_GetDiskFreeSpaceW(
BinVolume *pVolume,
PCWSTR pwsPathName,
PDWORD pSectorsPerCluster,
PDWORD pBytesPerSector,
PDWORD pFreeClusters,
PDWORD pClusters)
{
    FSD_DISK_INFO fdi;
    BOOL fSuccess = TRUE;
    DWORD dwError = ERROR_SUCCESS;
    FSDMGR_GetDiskInfo(pVolume->hDsk, &fdi);

    if (pSectorsPerCluster) 
        *pSectorsPerCluster = 1;
    if (pBytesPerSector)
        *pBytesPerSector = fdi.cbSector;
    if (pFreeClusters)
        *pFreeClusters = 0;
    if (pClusters)
        *pClusters = fdi.cSectors;
    if (ERROR_SUCCESS != dwError) {
        SetLastError( dwError);
    }
    return TRUE;
}


/*  FSD_Notify - Power off/device on notification handler
 *
 *  ENTRY
 *      pVolume - pointer to VOLUME (NULL if none)
 *      dwFlags - notification flags (see FSNOTIFY_*)
 *
 *  EXIT
 *      None
 */

extern "C" void FSD_Notify(BinVolume *pVolume, DWORD dwFlags)
{
    DEBUGMSG(ZONE_API, (L"BINFS: FSD_NotifyW\n"));
    return;
}


