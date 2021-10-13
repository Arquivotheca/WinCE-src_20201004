//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+-------------------------------------------------------------------------
//
//
//  File:       udfsapi.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------

#include "udfs.h"

// TODO: Validate pVol, pfh passed in ???

BOOL    ROFS_CreateDirectory( PUDFSDRIVER pVol, PCWSTR pwsPathName, LPSECURITY_ATTRIBUTES pSecurityAttributes)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

BOOL    ROFS_RemoveDirectory( PUDFSDRIVER pVol, PCWSTR pwsPathName)
{
    DWORD dwAttr;
    dwAttr = pVol->ROFS_GetFileAttributes(pwsPathName);
    if ((0xFFFFFFFF != dwAttr) && (dwAttr & FILE_ATTRIBUTE_DIRECTORY)){
        SetLastError(ERROR_ACCESS_DENIED);
    } else {
        SetLastError(ERROR_DIRECTORY);
    }    
    return FALSE;
}

BOOL    ROFS_SetFileAttributes( PUDFSDRIVER pVol, PCWSTR pwsFileName, DWORD dwAttributes)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

DWORD   ROFS_GetFileAttributes( PUDFSDRIVER pVol, PCWSTR pwsFileName)
{
    return pVol->ROFS_GetFileAttributes(pwsFileName);
}

HANDLE  ROFS_CreateFile( PUDFSDRIVER pVol, HANDLE hProc, PCWSTR pwsFileName, DWORD dwAccess, DWORD dwShareMode,
                             LPSECURITY_ATTRIBUTES pSecurityAttributes, DWORD dwCreate, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    return pVol->ROFS_CreateFile( hProc, pwsFileName, dwAccess, dwShareMode, pSecurityAttributes, dwCreate, dwFlagsAndAttributes, hTemplateFile);
}

BOOL    ROFS_DeleteFile( PUDFSDRIVER pVol, PCWSTR pwsFileName)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

BOOL    ROFS_MoveFile( PUDFSDRIVER pVol, PCWSTR pwsOldFileName, PCWSTR pwsNewFileName)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

HANDLE  ROFS_FindFirstFile( PUDFSDRIVER pvol, HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd)
{

    return pvol->ROFS_FindFirstFile( hProc, pwsFileSpec, pfd);
}

BOOL    ROFS_RegisterFileSystemFunction( PUDFSDRIVER pVol, SHELLFILECHANGEFUNC_t pShellFunc)
{
    return FALSE;
}

BOOL    ROFS_RegisterFileSystemNotification( PUDFSDRIVER pVol, HWND hWnd)
{
    return pVol->ROFS_RegisterFileSystemNotification(hWnd);
}

BOOL    ROFS_OidGetInfo( PUDFSDRIVER pVol, CEOID oid, CEOIDINFO *poi)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

BOOL    ROFS_DeleteAndRenameFile( PUDFSDRIVER pVol, PCWSTR pwsOldFile, PCWSTR pwsNewFile)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

BOOL    ROFS_CloseAllFileHandles( PUDFSDRIVER pVol, HANDLE hProc)
{
    return FALSE;
}

BOOL    ROFS_GetDiskFreeSpace( PUDFSDRIVER pVol, PCWSTR pwsPathName, PDWORD pSectorsPerCluster, PDWORD pBytesPerSector, PDWORD pFreeClusters, PDWORD pClusters)
{
    DEBUGMSG( ZONE_APIS, (TEXT("GetDiskFreeSpace on %s"), pwsPathName));
    DWORD dwAttr;
    dwAttr = pVol->ROFS_GetFileAttributes(pwsPathName);
    if ((wcscmp(pwsPathName, TEXT("\\")) == 0) || ((0xFFFFFFFF != dwAttr) && (dwAttr & FILE_ATTRIBUTE_DIRECTORY))){
        return pVol->ROFS_GetDiskFreeSpace( pSectorsPerCluster, pBytesPerSector, pFreeClusters, pClusters);
    } else {
        SetLastError(ERROR_DIRECTORY);
    }    
    return FALSE;
}

void    ROFS_Notify( PUDFSDRIVER pVol, DWORD dwFlags)
{
	// Clear cached directory structures on power on
	if (dwFlags & FSNOTIFY_POWER_ON) 
		pVol->Clean();
}

BOOL    ROFS_CloseFileHandle( PFILE_HANDLE_INFO pfh)
{
    return pfh->pVol->ROFS_CloseFileHandle(pfh);
}

BOOL    ROFS_ReadFile( PFILE_HANDLE_INFO pfh, LPVOID buffer, DWORD nBytesToRead, DWORD * pNumBytesRead, OVERLAPPED * pOverlapped)
{
    return pfh->pVol->ROFS_ReadFile( pfh, buffer, nBytesToRead, pNumBytesRead, pOverlapped);
}

BOOL    ROFS_ReadFileWithSeek( PFILE_HANDLE_INFO pfh, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead, 
                                      LPOVERLAPPED lpOverlapped, DWORD dwLowOffset, DWORD dwHighOffset)
{
    return pfh->pVol->ROFS_ReadFileWithSeek( pfh, buffer, nBytesToRead, lpNumBytesRead, lpOverlapped, dwLowOffset, dwHighOffset);
}

BOOL ROFS_WriteFileWithSeek( PFILE_HANDLE_INFO pfh,  LPCVOID buffer,  DWORD nBytesToWrite,  LPDWORD lpNumBytesWritten, 
                                    LPOVERLAPPED lpOverlapped,  DWORD dwLowOffset, DWORD dwHighOffset)
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}


BOOL    ROFS_WriteFile( PFILE_HANDLE_INFO pfh, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD pNumBytesWritten, LPOVERLAPPED pOverlapped)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

DWORD   ROFS_GetFileSize( PFILE_HANDLE_INFO pfh, PDWORD pFileSizeHigh)
{
    //
    //  Move this into the class
    //
    return pfh->pVol->ROFS_GetFileSize(pfh, pFileSizeHigh);
}

DWORD   ROFS_SetFilePointer( PFILE_HANDLE_INFO pfh, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
    return pfh->pVol->ROFS_SetFilePointer( pfh, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
}

BOOL    ROFS_GetFileInformationByHandle( PFILE_HANDLE_INFO pfh, LPBY_HANDLE_FILE_INFORMATION pFileInfo)
{
    return pfh->pVol->ROFS_GetFileInformationByHandle( pfh, pFileInfo);
}

BOOL    ROFS_FlushFileBuffers( PFILE_HANDLE_INFO pfh)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

BOOL    ROFS_GetFileTime( PFILE_HANDLE_INFO pfh, LPFILETIME pCreation, LPFILETIME pLastAccess, LPFILETIME pLastWrite)
{
    return pfh->pVol->ROFS_GetFileTime( pfh, pCreation, pLastAccess, pLastWrite);
}

BOOL    ROFS_SetFileTime( PFILE_HANDLE_INFO pfh, CONST FILETIME *pCreation, CONST FILETIME *pLastAccess, CONST FILETIME *pLastWrite)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

BOOL    ROFS_SetEndOfFile( PFILE_HANDLE_INFO pfh)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

// DeviceIOControl


BOOL ROFS_FindClose( PFIND_HANDLE_INFO psfr)
{
    return psfr->pVol->ROFS_FindClose(psfr);
}


BOOL ROFS_FindNextFile( PFIND_HANDLE_INFO psfr, PWIN32_FIND_DATAW pfd)
{
    return psfr->pVol->ROFS_FindNextFile( psfr, pfd);
}

BOOL ROFS_DeviceIoControl( PFILE_HANDLE_INFO pfh, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, 
                                     DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    // TODO: Have the ability to create a dummy file like "\\CDROM"
    return pfh->pVol->ROFS_DeviceIoControl( pfh, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
}

BOOL ROFS_RegisterFileSystemNotification(HWND hWnd) 
{ 
    // For now we don't support filesystem notifications
    return FALSE; 
}

