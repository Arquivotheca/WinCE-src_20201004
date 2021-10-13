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
#define BUILDING_FS_STUBS
#include <windows.h>
#include <storemgr.h>
#include <fsioctl.h>

extern "C"
BOOL
WINAPI
xxx_DeviceIoControl(
    HANDLE          hDevice,
    DWORD           dwIoControlCode,
    LPVOID          lpInBuf,
    DWORD           nInBufSize,
    LPVOID          lpOutBuf,
    DWORD           nOutBufSize,
    LPDWORD         lpBytesReturned,
    LPOVERLAPPED    lpOverlapped
    )
{
    return DeviceIoControl_Trap(hDevice,dwIoControlCode,lpInBuf,nInBufSize,lpOutBuf,nOutBufSize,lpBytesReturned,lpOverlapped);
}

extern "C"
BOOL
xxx_ForwardDeviceIoControl(
    HANDLE          hDevice,
    DWORD           dwIoControlCode,
    LPVOID          lpInBuf,
    DWORD           nInBufSize,
    LPVOID          lpOutBuf,
    DWORD           nOutBufSize,
    LPDWORD         lpBytesReturned,
    LPOVERLAPPED    lpOverlapped
    )
{
    return ForwardDeviceIoControl_Trap(hDevice,dwIoControlCode,lpInBuf,nInBufSize,lpOutBuf,nOutBufSize,lpBytesReturned,lpOverlapped);
}



//------------------------------------------------------------------------------
// These functions are fsmain elements that are used by other (non-thunk) parts
// of coredll and therefore ALWAYS linked with coredll.  Separating them from
// tfsmain.c means that the functions in tfsmain.c are NOT always linked with
// coredll.  When in doubt about which file to put a function in, place it here.
//------------------------------------------------------------------------------

//
// SDK exports
//

extern "C"
DWORD WINAPI xxx_GetFileAttributesW(LPCWSTR lpFileName) {
    return GetFileAttributesW_Trap(lpFileName);
}

extern "C"
HANDLE WINAPI xxx_FindFirstFileW(LPCWSTR lpFileName,
                        LPWIN32_FIND_DATAW lpFindFileData) {
    return FindFirstFileW_Trap(lpFileName,lpFindFileData,sizeof(WIN32_FIND_DATAW));
}

extern "C"
HANDLE
WINAPI
xxx_CreateFileW(
    LPCWSTR                 lpFileName,
    DWORD                   dwDesiredAccess,
    DWORD                   dwShareMode,
    LPSECURITY_ATTRIBUTES   lpsa,
    DWORD                   dwCreationDisposition,
    DWORD                   dwFlagsAndAttributes,
    HANDLE                  hTemplateFile
    )
{
    HANDLE h = CreateFileW_Trap(lpFileName,dwDesiredAccess,dwShareMode,lpsa,dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);

    return h;
}

extern "C"
BOOL
WINAPI
xxx_ReadFile(
    HANDLE          hFile,
    LPVOID          lpBuffer,
    DWORD           nNumberOfBytesToRead,
    LPDWORD         lpNumberOfBytesRead,
    LPOVERLAPPED    lpOverlapped
    )
{
    return ReadFile_Trap(hFile,lpBuffer,nNumberOfBytesToRead,lpNumberOfBytesRead,lpOverlapped);
}

extern "C"
BOOL WINAPI xxx_WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten,
                     LPOVERLAPPED lpOverlapped) {
    return WriteFile_Trap(hFile,lpBuffer,nNumberOfBytesToWrite,lpNumberOfBytesWritten,lpOverlapped);
}

extern "C"
DWORD WINAPI xxx_SetFilePointer(HANDLE hFile, LONG lDistanceToMove,
                       PLONG lpDistanceToMoveHigh,
                       DWORD dwMoveMethod) {
    return SetFilePointer_Trap(hFile,lDistanceToMove,lpDistanceToMoveHigh,
              dwMoveMethod);
}

extern "C"
HANDLE xxx_FindFirstChangeNotificationW(LPCWSTR lpPath, BOOL bSubTree, DWORD dwFlags)
{
    return FindFirstChangeNotificationW_Trap( lpPath, bSubTree, dwFlags);
}

extern "C"
BOOL xxx_FindNextChangeNotification(HANDLE hNotify)
{
    return FindNextChangeNotification_Trap(hNotify);
}

extern "C"
BOOL xxx_FindCloseChangeNotification(HANDLE hNotify)
{
    return FindCloseChangeNotification_Trap(hNotify);
}

extern "C"
BOOL    xxx_CeGetFileNotificationInfo(HANDLE h, DWORD dwFlags, LPVOID lpBuffer, DWORD nBufferLength, LPDWORD lpBytesReturned, LPDWORD lpBytesAvailable)
{
    return CeGetFileNotificationInfo_Trap(h, dwFlags, lpBuffer, nBufferLength, lpBytesReturned, lpBytesAvailable);
}

extern "C"
BOOL    xxx_CeFsIoControlW(
    LPCWSTR pszRootPath, 
    DWORD dwIoctl, 
    LPVOID lpInBuf, 
    DWORD nInBufSize, 
    LPVOID lpOutBuf, 
    DWORD nOutBufSize, 
    LPDWORD lpBytesReturned, 
    LPOVERLAPPED lpOverlapped
    )
{
    return CeFsIoControlW_Trap(pszRootPath,dwIoctl,lpInBuf,nInBufSize,lpOutBuf,nOutBufSize,lpBytesReturned,lpOverlapped);
}

extern "C"
BOOL xxx_CeGetVolumeInfoW(
    LPCWSTR pszRootPath,
    CE_VOLUME_INFO_LEVEL InfoLevel,
    LPCE_VOLUME_INFO lpVolumeInfo
    )
{
    if (INVALID_FILE_ATTRIBUTES == xxx_GetFileAttributesW(pszRootPath))
    {
        // This path is invalid, so no volume information should be reported.
        return FALSE;
    }
    else
    {
        DWORD dwRet;
        return xxx_CeFsIoControlW( 
            pszRootPath, 
            FSCTL_GET_VOLUME_INFO, 
            &InfoLevel, 
            sizeof(CE_VOLUME_INFO_LEVEL), 
            lpVolumeInfo, 
            sizeof(CE_VOLUME_INFO),
            &dwRet,
            NULL);
    }
}

extern "C"
BOOL xxx_GetFileSecurityW (
    LPCWSTR pFileName,
    SECURITY_INFORMATION RequestedInformation,
    PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD Length,
    DWORD* pLengthNeeded
    )
{
    return GetFileSecurityW_Trap (pFileName, RequestedInformation, pSecurityDescriptor, Length, pLengthNeeded);
}

extern "C"
BOOL xxx_SetFileSecurityW (
    LPCWSTR pFileName,
    SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD Length
    )
{
    return SetFileSecurityW_Trap (pFileName, SecurityInformation, pSecurityDescriptor, Length);
}

extern "C"
BOOL xxx_CancelIo (
    HANDLE hFile
    )
{
    return CancelIo_Trap (hFile);
}

extern "C"
BOOL xxx_CancelIoEx (
    HANDLE hFile,
    LPOVERLAPPED lpOverlapped
    )
{
    return CancelIoEx_Trap (hFile, lpOverlapped, sizeof(OVERLAPPED));
}

extern "C"
BOOL xxx_SecureWipeAllVolumes()
{
    return SecureWipeAllVolumes_Trap();
}

//------------------------------------------------------------------------------
