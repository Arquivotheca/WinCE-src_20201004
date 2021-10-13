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
//
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    fscall.h - internal kernel header, defines all functions calls to filesys
//

#ifndef _NK_FSCALL_H_
#define _NK_FSCALL_H_

#include "kerncmn.h"
#include <iri.h>

//
// FS api set
//
HANDLE FSCreateFile (LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
void   FSNotifyMountedFS (DWORD dwFlags);
HANDLE FSOpenModule (LPCWSTR ModuleName, DWORD Flags);
LONG   NKRegCloseKey (HKEY hKey);
LONG   NKRegCreateKeyExW (HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES lpsa, PHKEY phkResult, LPDWORD lpdwDisp);
LONG   NKRegOpenKeyExW (HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
LONG   NKRegQueryValueExW (HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
LONG   NKRegSetValueExW (HKEY hKey, LPCWSTR lpValueName, DWORD dwReserved, DWORD dwType, LPBYTE lpData, DWORD cbData);
LONG   NKRegTestSetValueW (HKEY hKey, LPCWSTR lpValueName, DWORD dwType, LPBYTE lpOldData, DWORD cbOldData, LPBYTE lpNewData, DWORD cbNewData, DWORD dwFlags);
LONG   NKRegFlushKey (HKEY hKey);

// Unicode only
#define NKRegCreateKeyEx    NKRegCreateKeyExW
#define NKRegOpenKeyEx      NKRegOpenKeyExW
#define NKRegQueryValueEx   NKRegQueryValueExW
#define NKRegSetValueEx     NKRegSetValueExW
#define NKRegTestSetValue   NKRegTestSetValueW

//
// Handle based FS calls
//
DWORD FSSetFilePointer (HANDLE, LONG, PLONG, DWORD);
BOOL  FSReadFile (HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
BOOL  FSWriteFile (HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
BOOL  FSReadFileWithSeek (HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED, DWORD, DWORD);
BOOL  FSWriteFileWithSeek (HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED, DWORD, DWORD);
BOOL  FSSetEndOfFile (HANDLE);
BOOL  FSIoctl (HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
BOOL  FSGetFileInfo (HANDLE, PBY_HANDLE_FILE_INFORMATION);

//
// function to call handle based API with PHDATA
//
DWORD PHD_SetFilePointer (PHDATA, LONG, PLONG, DWORD);
BOOL  PHD_ReadFile (PHDATA phdFile, LPVOID lpBuffer, DWORD cbToRead, LPDWORD pcbRead, LPOVERLAPPED lpOvp);
BOOL  PHD_WriteFile (PHDATA phdFile, LPCVOID lpBuffer, DWORD cbToWrite, LPDWORD pcbWritten, LPOVERLAPPED lpOvp);
BOOL  PHD_ReadFileWithSeek (PHDATA, LPVOID, DWORD, LPDWORD, LPOVERLAPPED, DWORD, DWORD);
BOOL  PHD_WriteFileWithSeek (PHDATA, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED, DWORD, DWORD);
BOOL  PHD_FlushFileBuffers (PHDATA);
BOOL  PHD_SetEndOfFile (PHDATA);
BOOL  PHD_FSIoctl (PHDATA, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
BOOL  PHD_GetFileInfo (PHDATA, PBY_HANDLE_FILE_INFORMATION);

// Helpers for very large files
BOOL  PHD_SetLargeFilePointer (PHDATA, ULARGE_INTEGER);
BOOL  PHD_GetLargeFileSize (PHDATA, ULARGE_INTEGER*);

// PHD_CloseHandle is generic to all handles, not just filesys
BOOL  PHD_CloseHandle (PHDATA phd);
BOOL  PHD_PreCloseHandle (PHDATA phd);

#endif  // _NK_FSCALL_H_

