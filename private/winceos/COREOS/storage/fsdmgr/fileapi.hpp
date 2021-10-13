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
#ifndef __FILEAPI_HPP__
#define __FILEAPI_HPP__

// API set handle for these APIs.
extern HANDLE hFileAPI;
LRESULT InitializeFileAPI ();

// File Handle-based APIs.
//
// These APIs take a file handle. These APIs parallel the HT_FILE API set
// type and are registered as such during initialization of the storage
// manager.
//
EXTERN_C BOOL FSDMGR_CloseFile (FileSystemHandle_t* pHandle);
EXTERN_C BOOL FSDMGREXT_ReadFile (FileSystemHandle_t* pHandle, void* pBuffer, DWORD BufferSize, 
    DWORD* pBytesRead, OVERLAPPED* pOverlapped);
EXTERN_C BOOL FSDMGRINT_ReadFile (FileSystemHandle_t* pHandle, void* pBuffer, DWORD BufferSize, 
    DWORD* pBytesRead, OVERLAPPED* pOverlapped);
EXTERN_C BOOL FSDMGREXT_WriteFile (FileSystemHandle_t* pHandle, const void* pBuffer, 
    DWORD BufferSize, DWORD* pBytesWritten, OVERLAPPED* pOverlapped);
EXTERN_C BOOL FSDMGRINT_WriteFile (FileSystemHandle_t* pHandle, const void* pBuffer, 
    DWORD BufferSize, DWORD* pBytesWritten, OVERLAPPED* pOverlapped);
EXTERN_C DWORD FSDMGR_GetFileSize (FileSystemHandle_t* pHandle, DWORD* pFileSizeHigh);
EXTERN_C DWORD FSDMGR_SetFilePointer (FileSystemHandle_t* pHandle, LONG DistanceToMove,
    LONG* pDistanceToMoveHigh, DWORD MoveMethod);
EXTERN_C BOOL FSDMGR_GetFileInformationByHandle (FileSystemHandle_t* pHandle,
    BY_HANDLE_FILE_INFORMATION* pFileInfo, DWORD SizeOfFileInfo);
EXTERN_C BOOL FSDMGR_FlushFileBuffers (FileSystemHandle_t* pHandle);
EXTERN_C BOOL FSDMGR_GetFileTime (FileSystemHandle_t* pHandle, FILETIME *pCreation, 
    FILETIME *pLastAccess, FILETIME *pLastWrite);
EXTERN_C BOOL FSDMGR_SetFileTime (FileSystemHandle_t* pHandle, const FILETIME* pCreation, 
    const FILETIME* pLastAccess, const FILETIME* pLastWrite);
EXTERN_C BOOL FSDMGR_SetEndOfFile (FileSystemHandle_t* pHandle);
EXTERN_C BOOL FSDMGREXT_DeviceIoControl (FileSystemHandle_t* pHandle, DWORD dwIoControlCode, 
    void* pInBuf, DWORD InBufSize, void* pOutBuf, DWORD OutBufSize, 
    DWORD* pBytesReturned, OVERLAPPED* pOverlapped);
EXTERN_C BOOL FSDMGRINT_DeviceIoControl (FileSystemHandle_t* pHandle, DWORD dwIoControlCode, 
    void* pInBuf, DWORD InBufSize, void* pOutBuf, DWORD OutBufSize, 
    DWORD* pBytesReturned, OVERLAPPED* pOverlapped);
EXTERN_C BOOL FSDMGREXT_ReadFileWithSeek (FileSystemHandle_t* pHandle, void* pBuffer, DWORD BufferSize, 
    DWORD* pBytesRead, OVERLAPPED* pOverlapped, DWORD LowOffset, DWORD HighOffset);
EXTERN_C BOOL FSDMGRINT_ReadFileWithSeek (FileSystemHandle_t* pHandle, void* pBuffer, DWORD BufferSize, 
    DWORD* pBytesRead, OVERLAPPED* pOverlapped, DWORD LowOffset, DWORD HighOffset);
EXTERN_C BOOL FSDMGREXT_WriteFileWithSeek (FileSystemHandle_t* pHandle, const void* pBuffer, 
    DWORD BufferSize, DWORD* pBytesWritten, OVERLAPPED* pOverlapped, DWORD LowOffset, 
    DWORD HighOffset);
EXTERN_C BOOL FSDMGRINT_WriteFileWithSeek (FileSystemHandle_t* pHandle, const void* pBuffer, 
    DWORD BufferSize, DWORD* pBytesWritten, OVERLAPPED* pOverlapped, DWORD LowOffset, 
    DWORD HighOffset);
EXTERN_C BOOL FSDMGREXT_LockFileEx (FileSystemHandle_t* pHandle, DWORD Flags, DWORD Reserved, 
    DWORD NumberOfBytesToLockLow, DWORD NumberOfBytesToLockHigh, OVERLAPPED* pOverlapped);
EXTERN_C BOOL FSDMGRINT_LockFileEx (FileSystemHandle_t* pHandle, DWORD Flags, DWORD Reserved, 
    DWORD NumberOfBytesToLockLow, DWORD NumberOfBytesToLockHigh, OVERLAPPED* pOverlapped);
EXTERN_C BOOL FSDMGREXT_UnlockFileEx (FileSystemHandle_t* pHandle, DWORD Reserved, DWORD NumberOfBytesToLockLow, 
    DWORD NumberOfBytesToLockHigh, OVERLAPPED* pOverlapped);
EXTERN_C BOOL FSDMGRINT_UnlockFileEx (FileSystemHandle_t* pHandle, DWORD Reserved, DWORD NumberOfBytesToLockLow, 
    DWORD NumberOfBytesToLockHigh, OVERLAPPED* pOverlapped);
EXTERN_C BOOL FSDMGR_CancelIoEx (FileSystemHandle_t* pHandle, OVERLAPPED* pOverlapped, DWORD SizeOfOverlapped);
EXTERN_C BOOL FSDMGR_CancelIo (FileSystemHandle_t* pHandle);

#endif // __FILEAPI_HPP__
