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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

//
// This file contains prototypes for Device Manager system call handler functions
//

#pragma once

#include <guiddef.h>

#ifdef __cplusplus
extern "C" {
#endif	// __cplusplus

// Forward decl. This type is defined in <devload.h>.
typedef struct _REGINI const *LPCREGINI;
typedef struct _REGINI *LPREGINI;

// Device Manager APIs
BOOL WINAPI DM_Unsupported();
BOOL WINAPI DM_DeregisterDevice(HANDLE hDevice);
HANDLE WINAPI DM_ActivateDeviceEx(LPCWSTR lpszDevKey, LPCREGINI lpReg, DWORD cReg, LPVOID lpvParam);
HANDLE WINAPI EX_DM_ActivateDeviceEx(LPCWSTR lpszDevKey, LPCREGINI lpReg, DWORD cReg, LPVOID lpvParam);
BOOL WINAPI DM_DeactivateDevice(HANDLE hDevice);
BOOL WINAPI DM_CeResyncFilesys(HANDLE hDevice);
void WINAPI DM_DevProcNotify(DWORD flags, HPROCESS proc, HTHREAD thread);
HANDLE WINAPI DM_RegisterDevice(LPCWSTR lpszType, DWORD dwIndex, LPCWSTR lpszLib, DWORD dwInfo);
BOOL WINAPI DM_GetDeviceByIndex(DWORD dwIndex, LPWIN32_FIND_DATA lpFindFileData);
BOOL WINAPI DM_GetDeviceInformationByDeviceHandle(HANDLE hDevice, PDEVMGR_DEVICE_INFORMATION pdi);
BOOL WINAPI EX_DM_GetDeviceInformationByDeviceHandle(HANDLE hDevice, PDEVMGR_DEVICE_INFORMATION pdi);

// Device FileHandle APIs
HANDLE WINAPI DM_CreateDeviceHandle(LPCWSTR lpNew, DWORD dwAccess, DWORD dwShareMode, HPROCESS hProc);
BOOL WINAPI DM_DevCloseFileHandle(HANDLE fsodev);
BOOL WINAPI DM_DevPreCloseFileHandle(HANDLE fsodev);
BOOL WINAPI DM_DevReadFile(HANDLE fsodev, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead,
	LPOVERLAPPED lpOverlapped);
BOOL WINAPI DM_DevWriteFile(HANDLE fsodev, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten,
	LPOVERLAPPED lpOverlapped);
DWORD WINAPI DM_DevSetFilePointer(HANDLE fsodev, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh,
	DWORD dwMoveMethod);
BOOL WINAPI DM_DevDeviceIoControl(HANDLE fsodev, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
DWORD WINAPI DM_DevGetFileSize(HANDLE fsodev, LPDWORD lpFileSizeHigh);
BOOL WINAPI DM_DevGetDeviceInformationByFileHandle(HANDLE fsodev, PDEVMGR_DEVICE_INFORMATION pdi);
BOOL WINAPI EX_DM_DevGetDeviceInformationByFileHandle(HANDLE fsodev, PDEVMGR_DEVICE_INFORMATION pdi);
BOOL WINAPI DM_DevFlushFileBuffers(HANDLE fsodev);
BOOL WINAPI DM_DevGetFileTime(HANDLE fsodev, LPFILETIME lpCreation, LPFILETIME lpLastAccess, LPFILETIME lpLastWrite);
BOOL WINAPI EX_DM_DevGetFileTime(HANDLE fsodev, LPFILETIME lpCreation, LPFILETIME lpLastAccess, LPFILETIME lpLastWrite);
BOOL WINAPI DM_DevSetFileTime(HANDLE fsodev, CONST FILETIME *lpCreation, CONST FILETIME *lpLastAccess, CONST FILETIME *lpLastWrite);
BOOL WINAPI EX_DM_DevSetFileTime(HANDLE fsodev, CONST FILETIME *lpCreation, CONST FILETIME *lpLastAccess, CONST FILETIME *lpLastWrite);
BOOL WINAPI DM_DevSetEndOfFile(HANDLE fsodev);
BOOL WINAPI DM_EnumDeviceInterfaces(HANDLE h, DWORD dwIndex, GUID *pClass, LPWSTR pszNameBuf, LPDWORD lpdwNameBufSize);
BOOL WINAPI EX_DM_EnumDeviceInterfaces(HANDLE h, DWORD dwIndex, GUID *pClass, LPWSTR pszNameBuf, LPDWORD lpdwNameBufSize);

typedef struct _ServicesExeEnumServicesIntrnl ServicesExeEnumServicesIntrnl;
BOOL WINAPI DM_EnumServices(ServicesExeEnumServicesIntrnl *pServiceEnumInfo);
BOOL WINAPI EX_DM_EnumServices(ServicesExeEnumServicesIntrnl *pServiceEnumInfo);

#ifdef __cplusplus
}
#endif	// __cplusplus

