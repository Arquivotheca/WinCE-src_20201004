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
#include <windows.h>
#include <extfile.h>

BOOL
WINAPI
xxx_AFS_Unmount(
    HANDLE hFileSys
    )
{
#ifdef KCOREDLL
    return AFS_Unmount_Trap( hFileSys );
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif // KCOREDLL
}


BOOL
WINAPI
xxx_AFS_CreateDirectoryW(
    HANDLE hFileSys,
    LPCWSTR lpPathName,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD SecurityDescriptorSize
    )
{
#ifdef KCOREDLL
    return AFS_CreateDirectoryW_Trap( hFileSys, lpPathName, lpSecurityAttributes, pSecurityDescriptor, SecurityDescriptorSize );
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif // KCOREDLL
}



BOOL
WINAPI
xxx_AFS_RemoveDirectoryW(
    HANDLE hFileSys,
    LPCWSTR lpPathName
    )
{
#ifdef KCOREDLL
    return AFS_RemoveDirectoryW_Trap( hFileSys, lpPathName );
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif // KCOREDLL
}



DWORD
WINAPI
xxx_AFS_GetFileAttributesW(
    HANDLE hFileSys,
    LPCWSTR lpFileName
    )
{
#ifdef KCOREDLL
    return AFS_GetFileAttributesW_Trap( hFileSys, lpFileName );
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return INVALID_FILE_ATTRIBUTES;
#endif // KCOREDLL
}


BOOL
WINAPI
xxx_AFS_SetFileAttributesW(
    HANDLE hFileSys,
    LPCWSTR lpszName,
    DWORD dwAttributes
    )
{
#ifdef KCOREDLL
    return AFS_SetFileAttributesW_Trap( hFileSys, lpszName, dwAttributes );
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif // KCOREDLL
}


HANDLE
WINAPI
xxx_AFS_CreateFileW(
    HANDLE hFileSys,
    HANDLE  hProc,
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile,
    PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD SecurityDescriptorSize
    )
{
#ifdef KCOREDLL
    return AFS_CreateFileW_Trap(
                hFileSys,
                hProc,
                lpFileName,
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile,
                pSecurityDescriptor,
                SecurityDescriptorSize
                );
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return INVALID_HANDLE_VALUE;
#endif // KCOREDLL
}


BOOL
WINAPI
xxx_AFS_DeleteFileW(
    HANDLE hFileSys,
    LPCWSTR lpFileName
    )
{
#ifdef KCOREDLL
    return AFS_DeleteFileW_Trap( hFileSys, lpFileName );
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif // KCOREDLL
}


BOOL
WINAPI
xxx_AFS_MoveFileW(
    HANDLE hFileSys,
    LPCWSTR lpExistingFileName,
    LPCWSTR lpNewFileName
    )
{
#ifdef KCOREDLL
    return AFS_MoveFileW_Trap( hFileSys, lpExistingFileName, lpNewFileName );
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif // KCOREDLL
}


HANDLE
WINAPI
xxx_AFS_FindFirstFileW(
    HANDLE hFileSys,
    HANDLE hProc,
    LPCWSTR lpFileName,
    LPWIN32_FIND_DATAW lpFindFileData,
    DWORD SizeOfFindFileData
    )
{
#ifdef KCOREDLL
    return AFS_FindFirstFileW_Trap( hFileSys, hProc, lpFileName, lpFindFileData, SizeOfFindFileData);
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return INVALID_HANDLE_VALUE;
#endif // KCOREDLL
}



BOOL
WINAPI
xxx_AFS_RegisterFileSystemFunction(
    HANDLE hFileSys,
    SHELLFILECHANGEFUNC_t pFunc 
    )
{
#ifdef KCOREDLL
    return AFS_RegisterFileSystemFunction_Trap(hFileSys, pFunc);
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif // KCOREDLL
}



BOOL
WINAPI
xxx_AFS_PrestoChangoFileName(
    HANDLE hFileSys,
    LPCWSTR lpszOldFile,
    LPCWSTR lpszNewFile
    )
{
#ifdef KCOREDLL
    return AFS_PrestoChangoFileName_Trap( hFileSys, lpszOldFile, lpszNewFile );
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif // KCOREDLL
}



BOOL
WINAPI
xxx_AFS_CloseAllFileHandles(
    HANDLE hFileSys,
    HANDLE hProc
    )
{
#ifdef KCOREDLL
    return AFS_CloseAllFileHandles_Trap( hFileSys, hProc );
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif // KCOREDLL
}



BOOL
WINAPI
xxx_AFS_GetDiskFreeSpace(
    HANDLE hFileSys,
    LPCWSTR lpPathName,
    LPDWORD lpSectorsPerCluster,
    LPDWORD lpBytesPerSector,
    LPDWORD lpFreeClusters,
    LPDWORD lpClusters
    )
{
#ifdef KCOREDLL
    return AFS_GetDiskFreeSpace_Trap(
                hFileSys,
                lpPathName,
                lpSectorsPerCluster,
                lpBytesPerSector,
                lpFreeClusters,
                lpClusters
                );
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif // KCOREDLL
}



void
WINAPI
xxx_AFS_NotifyMountedFS(
    HANDLE hFileSys,
    DWORD flags
    )
{
#ifdef KCOREDLL
    AFS_NotifyMountedFS_Trap( hFileSys, flags );
#else
    SetLastError (ERROR_NOT_SUPPORTED);
#endif // KCOREDLL
}

HANDLE
WINAPI
xxx_AFS_FindFirstChangeNotificationW(
    HANDLE hFileSys,
    HANDLE hProc,
    LPCWSTR lpPathName,
    BOOL bWatchSubtree,
    DWORD dwNotifyFilter
    )
{
#ifdef KCOREDLL
    return AFS_FindFirstChangeNotificationW_Trap( hFileSys, hProc, lpPathName, bWatchSubtree, dwNotifyFilter);
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return INVALID_HANDLE_VALUE;
#endif // KCOREDLL
}    

BOOL
WINAPI
xxx_AFS_FsIoControlW(
    HANDLE hFileSys,
    HANDLE hProc,
    DWORD dwIoctl,
    LPVOID lpInBuf,
    DWORD nInBufSize,
    LPVOID lpOutBuf,
    DWORD nOutBufSize,
    LPDWORD lpBytesReturned,
    LPOVERLAPPED lpOverlapped
    )
{
#ifdef KCOREDLL
    return AFS_FsIoControlW_Trap(hFileSys, hProc, dwIoctl, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif // KCOREDLL
}

WINBASEAPI
BOOL
xxx_AFS_SetFileSecurityW(
    HANDLE hFileSys,
    __in const WCHAR* pFileName,
    SECURITY_INFORMATION SecurityInformation,
    __in_bcount (Length) PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD Length
    )
{
#ifdef KCOREDLL
    return AFS_SetFileSecurityW_Trap(hFileSys, pFileName, SecurityInformation, pSecurityDescriptor, Length);
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif
}

WINBASEAPI
BOOL
xxx_AFS_GetFileSecurityW(
    HANDLE hFileSys,
    __in const WCHAR* pFileName,
    SECURITY_INFORMATION RequestedInformation,
    __out_bcount_opt (Length) PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD Length,
    __out_opt DWORD* pLengthNeeded
    )
{
#ifdef KCOREDLL
    return AFS_GetFileSecurityW_Trap(hFileSys, pFileName, RequestedInformation, pSecurityDescriptor, Length, pLengthNeeded);
#else
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
#endif
}
