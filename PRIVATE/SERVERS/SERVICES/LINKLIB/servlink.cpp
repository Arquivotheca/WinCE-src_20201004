//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/* File:    servlink.c
 *
 * Purpose: WinCE service manager.  
 *          This file creates a library that services written prior 
 *          to WinCE 4.0 can link to to call into services routines.  This
 *          library is required because on WinCE 4.0 and above services changes
 *          were made in coredll.  Note that running services.exe on devices
 *          prior to CE 4.0 was for internal development and test purposes only.
 *
 */

#define WINCEMACRO
#define _LINKLIST_H_
#include "serv.h"
#include <windef.h>
#include <service.h>


extern "C" {
HANDLE xxx_ActivateService(LPCWSTR lpszDevKey, DWORD dwClientInfo) {
    return ActivateService(lpszDevKey, dwClientInfo);
}

HANDLE xxx_RegisterService(LPCWSTR lpszType, DWORD dwIndex, LPCWSTR lpszLib, DWORD dwInfo) {
    return RegisterService(lpszType, dwIndex, lpszLib, dwInfo);
}

BOOL xxx_DeregisterService(HANDLE hDevice) {
	return DeregisterService(hDevice);
}

void xxx_CloseAllServiceHandles(HPROCESS proc) {
    CloseAllServiceHandles(proc);
}

HANDLE xxx_CreateServiceHandle(LPCWSTR lpNew, DWORD dwAccess, DWORD dwShareMode, HPROCESS hProc) {
    return CreateServiceHandle(lpNew, dwAccess, dwShareMode, hProc);
}

BOOL xxx_GetServiceByIndex(DWORD dwIndex, LPWIN32_FIND_DATA lpFindFileData) {
    return GetServiceByIndex(dwIndex, lpFindFileData);
}

BOOL xxx_ServiceIoControl(HANDLE hService, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
    return ServiceIoControl(hService, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
}

BOOL xxx_ServiceAddPort(HANDLE hService, SOCKADDR *pSockAddr, int cbSockAddr, int iProtocol, WCHAR *szRegistryPath) {
	return ServiceAddPort(hService, pSockAddr, cbSockAddr, iProtocol,szRegistryPath);
}

BOOL xxx_ServiceUnbindPorts(HANDLE hService) {
	return ServiceUnbindPorts(hService);
}

BOOL xxx_EnumServices(PBYTE pBuffer, DWORD *pdwServiceEntries, DWORD *pdwBufferLen) {
	return EnumServices(pBuffer, pdwServiceEntries, pdwBufferLen);
}

HANDLE xxx_GetServiceHandle(LPWSTR szPrefix, LPWSTR szDllName, DWORD *pdwDllBuf) {
	return GetServiceHandle(szPrefix,szDllName,pdwDllBuf);
}

BOOL xxx_ServiceClosePort(HANDLE hService, SOCKADDR *pSockAddr, int cbSockAddr, int iProtocol, BOOL fRemoveFromRegistry) {
	return ServiceClosePort(hService, pSockAddr, cbSockAddr, iProtocol, fRemoveFromRegistry);
}

};  // extern "C"
