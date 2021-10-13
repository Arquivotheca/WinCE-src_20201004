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
#ifndef __VOLUMEAPI_HPP__
#define __VOLUMEAPI_HPP__

extern HANDLE hVolumeAPI;
LRESULT InitializeVolumeAPI ();

// Volume-based APIs. 
//
// These APIs take a MountedVolume_t object (already entered) and perform a file 
// system operation on the volume object. These APIs are analogous to the
// AFS_xxx APIs.
//
EXTERN_C BOOL FSDMGR_Close (MountedVolume_t* pVolume);
EXTERN_C BOOL FSDMGR_PreClose (MountedVolume_t* pVolume);
EXTERN_C BOOL FSDMGR_CreateDirectoryW (MountedVolume_t* pVolume, const WCHAR* pPathName, 
    SECURITY_ATTRIBUTES* pSecurityAttributes, PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD SecurityDescriptorSize);
EXTERN_C BOOL FSDMGR_RemoveDirectoryW (MountedVolume_t* pVolume, const WCHAR* pPathName);
EXTERN_C DWORD FSDMGR_GetFileAttributesW (MountedVolume_t* pVolume, const WCHAR* pPathName);
EXTERN_C BOOL FSDMGR_SetFileAttributesW (MountedVolume_t* pVolume, const WCHAR* pPathName, 
    DWORD Attributes);
EXTERN_C BOOL FSDMGR_DeleteFileW (MountedVolume_t* pVolume, const WCHAR* pFileName);
EXTERN_C BOOL FSDMGR_MoveFileW (MountedVolume_t* pVolume, const WCHAR* pOldFileName, 
    const WCHAR* pNewFileName);
EXTERN_C BOOL FSDMGR_DeleteAndRenameFileW (MountedVolume_t* pVolume, const WCHAR* pDestFileName, 
    const WCHAR* pSourceFileName);
EXTERN_C BOOL FSDMGR_CloseAllFileHandles (MountedVolume_t* pVolume, HANDLE hProcess);
EXTERN_C BOOL FSDMGR_GetDiskFreeSpaceW (MountedVolume_t* pVolume, const WCHAR* pPathName, 
    DWORD* pSectorsPerCluster, DWORD* pBytesPerSector, DWORD* pFreeClusters, DWORD* pClusters);
EXTERN_C BOOL FSDMGR_RegisterFileSystemFunction (MountedVolume_t* pVolume, 
    SHELLFILECHANGEFUNC_t pFn);
EXTERN_C HANDLE FSDMGR_FindFirstFileW (MountedVolume_t* pVolume, HANDLE hProcess, 
    const WCHAR* pPathName, WIN32_FIND_DATA* pFindData, DWORD SizeOfFindData);
EXTERN_C HANDLE FSDMGR_CreateFileW (MountedVolume_t* pVolume, HANDLE hProcess,
    const WCHAR* pPathName, DWORD Access, DWORD ShareMode, SECURITY_ATTRIBUTES* pSecurityAttributes,
    DWORD Create, DWORD FlagsAndAttributes, HANDLE hTemplate, PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD SecurityDescriptorSize);
EXTERN_C HANDLE FSDMGR_FindFirstChangeNotificationW (MountedVolume_t* pVolume, HANDLE hProc, 
    const WCHAR* pPathName, BOOL WatchSubtree, DWORD NotifyFilter);
EXTERN_C BOOL FSDMGR_FsIoControlW (MountedVolume_t* pVolume, HANDLE hProc, 
    DWORD Fsctl, void* pInBuf, DWORD InBufSize, void* pOutBuf, DWORD OutBufSize, 
    DWORD* pBytesReturned, OVERLAPPED* pOverlapped);
EXTERN_C void FSDMGR_NotifyMountedFS (MountedVolume_t *pVolume, DWORD Flags);
BOOL FSDMGR_GetVolumeInfo (MountedVolume_t* pVolume, CE_VOLUME_INFO_LEVEL InfoLevel, 
    void* pInfo, DWORD InfoBytes, DWORD* pBytesReturned);
EXTERN_C BOOL FSDMGR_GetFileSecurityW (MountedVolume_t* pVolume,
    const WCHAR* pPathName, SECURITY_INFORMATION RequestedInformation,
    PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD Length, DWORD* pLengthNeeded);
EXTERN_C BOOL FSDMGR_SetFileSecurityW (MountedVolume_t* pVolume,
    const WCHAR* pFileName, SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD Length);


#endif // __VOLUMEAPI_HPP__

