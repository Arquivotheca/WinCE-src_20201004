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
#ifndef __PATHAPI_HPP__
#define __PATHAPI_HPP__

// Path-based APIs.
//
// These APIs take a file path, translate it to a MountedVolume_t object, and 
// invoke the coresponding volume-based API. These APIs are exported as the
// Win32 path-based file APIs to applications via the file system API set.
//
EXTERN_C BOOL FSINT_CreateDirectoryW (const WCHAR* pPathName, 
    SECURITY_ATTRIBUTES* pSecurityAttributes);
EXTERN_C BOOL FSEXT_CreateDirectoryW (const WCHAR* pPathName, 
    SECURITY_ATTRIBUTES* pSecurityAttributes);
EXTERN_C BOOL FS_RemoveDirectoryW (const WCHAR* pPathName);
EXTERN_C DWORD FS_GetFileAttributesW (const WCHAR* pPathName);
EXTERN_C BOOL FS_SetFileAttributesW (const WCHAR* pPathName, DWORD Attributes);
EXTERN_C BOOL FS_DeleteFileW (const WCHAR* pFileName);
EXTERN_C BOOL FS_MoveFileW (const WCHAR* pSourceFileName, const WCHAR* pDestFileName);
EXTERN_C BOOL FS_DeleteAndRenameFileW (const WCHAR* pDestFileName, 
    const WCHAR* pSourceFileName);
EXTERN_C BOOL FS_GetDiskFreeSpaceExW(const WCHAR* pPathName, ULARGE_INTEGER* pFreeBytesAvailableToCaller,
    ULARGE_INTEGER* pTotalNumberOfBytes, ULARGE_INTEGER* pTotalNumberOfFreeBytes);
EXTERN_C HANDLE FSEXT_FindFirstFileW (const WCHAR* pPathName, 
    WIN32_FIND_DATA* pFindData, DWORD SizeOfFindData);
EXTERN_C HANDLE FSINT_FindFirstFileW (const WCHAR* pPathName, 
    WIN32_FIND_DATA* pFindData, DWORD SizeOfFindData);
EXTERN_C HANDLE FSEXT_CreateFileW (const WCHAR* pPathName, 
    DWORD Access, DWORD ShareMode, SECURITY_ATTRIBUTES* pSecurityAttributes, 
    DWORD Create, DWORD FlagsAndAttributes, HANDLE hTemplate);
EXTERN_C HANDLE FSINT_CreateFileW (const WCHAR* pPathName, 
    DWORD Access, DWORD ShareMode, SECURITY_ATTRIBUTES* pSecurityAttributes, 
    DWORD Create, DWORD FlagsAndAttributes, HANDLE hTemplate);
EXTERN_C BOOL STOREMGR_FsIoControlW (HANDLE hProc, const WCHAR* pPathName, DWORD Fsctl, 
    void* pInBuf, DWORD InBufSize, void* pOutBuf, DWORD OutBufSize, 
    DWORD* pBytesReturned, OVERLAPPED* pOverlapped);
EXTERN_C BOOL FS_IsSystemFileW (const WCHAR* pFileName);
EXTERN_C HANDLE FSEXT_FindFirstChangeNotificationW (const WCHAR* pPathName,
    BOOL fWatchSubTree, DWORD NotifyFilter);
EXTERN_C HANDLE FSINT_FindFirstChangeNotificationW (const WCHAR* pPathName,
    BOOL fWatchSubTree, DWORD NotifyFilter);   
EXTERN_C BOOL FSEXT_FindNextChangeNotification (HANDLE h);
EXTERN_C BOOL FSINT_FindNextChangeNotification (HANDLE h);
EXTERN_C BOOL FSEXT_FindCloseChangeNotification (HANDLE h);
EXTERN_C BOOL FSINT_FindCloseChangeNotification (HANDLE h);
EXTERN_C BOOL FSEXT_GetFileNotificationInfoW (HANDLE h, DWORD Flags, void* pBuffer,
    DWORD BufferLength, DWORD* pBytesReturned, DWORD* pBytesAvailable);
EXTERN_C BOOL FSINT_GetFileNotificationInfoW (HANDLE h, DWORD Flags, void* pBuffer,
    DWORD BufferLength, DWORD* pBytesReturned, DWORD* pBytesAvailable);
EXTERN_C BOOL FS_GetFileSecurityW (
    __in const WCHAR* pFileName,
    SECURITY_INFORMATION RequestedInformation,
    __out_bcount_opt (Length) PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD Length,
    __out_opt DWORD* pLengthNeeded
    );
EXTERN_C BOOL FS_SetFileSecurityW (
    __in const WCHAR* pFileName,
    SECURITY_INFORMATION SecurityInformation,
    __in_bcount (Length) PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD Length
    );

#endif // __PATHAPI_HPP__

