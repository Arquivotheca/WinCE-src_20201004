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

//
// This header contains internal function I/O packet manager.
//

#pragma once

#define IOCTL_FIOMGR_IOUPDATE_STATUS CTL_CODE(FILE_DEVICE_PSL, 0x100, METHOD_NEITHER, FILE_ANY_ACCESS)
//Input
typedef struct __FIOMGR_IOUPDATE_STATUS {
    DWORD dwBytesTransfered;
    DWORD dwLastErrorStatus;
} FIOMGR_IOUPDATE_STATUS,*PFIOMGR_IOUPDATE_STATUS;
#define IOCTL_FIOMGR_IOGET_TARGET_PROCID CTL_CODE(FILE_DEVICE_PSL, 0x101, METHOD_NEITHER, FILE_ANY_ACCESS)
// Output  DWORD, Target ProcID.
#ifdef __cplusplus
extern "C" {
#endif
HANDLE CreateIoPacketHandle();
BOOL  DeleteIoPacketHandle(); 

fsopendev_t * CreateAsyncFileObject();
BOOL DeleteAsncyFileObject(fsopendev_t * pfsopened);

BOOL DevReadFile(fsopendev_t *fsodev, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead, LPOVERLAPPED lpOverlapped);
BOOL DevWriteFile(fsopendev_t *fsodev, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten,LPOVERLAPPED lpOverlapped);
BOOL DevDeviceIoControl(fsopendev_t *fsodev, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
BOOL DevCancelIo(fsopendev_t *fsodev);
BOOL DevCancelIoEx(fsopendev_t *fsodev, LPOVERLAPPED lpOverlapped );

#ifdef __cplusplus
};
#endif



