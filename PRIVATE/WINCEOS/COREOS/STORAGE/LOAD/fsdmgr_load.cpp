//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>


/*
[HKEY_LOCAL_MACHINE\Drivers\BuiltIn\StorageManager]
    "Dll"="fsdmgr.dll"
    "FriendlyName"="
    "Prefix"="FSD"
    "Ioctl"=dword:4
    "Order"=dword:0
*/


WCHAR *szRegKey = L"Drivers\\BuiltIn\\fsdmgr";
WCHAR *szPrefix = L"FSD";
WCHAR *szDLL = L"fsdmgr.dll";
WCHAR *szFriendlyName = L"Filesystem/Storage Manager";

#define WRITE_REG_SZ(Name, Value) RegSetValueEx( hKey, Name, 0, REG_SZ, (LPBYTE)Value, (wcslen(Value)+1)*sizeof(WCHAR))
#define WRITE_REG_DWORD(Name, Value) { DWORD dwValue = Value; RegSetValueEx( hKey, Name, 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD)); }

BOOL GetRegistryValue(HKEY hKey, PTSTR szValueName, PDWORD pdwValue)
{
	
	DWORD				dwValType, dwValLen;
	LONG				lStatus;
			
	dwValLen = sizeof(DWORD);
		
	lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, (PBYTE)pdwValue, &dwValLen);
		
	if ((lStatus != ERROR_SUCCESS) || (dwValType != REG_DWORD)) {			
		NKDbgPrintfW(L"RelFSD_Load::RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n", szValueName, lStatus, GetLastError());
		*pdwValue = -1;
		return FALSE;
	} 
	NKDbgPrintfW(L"RelFSD_Load::GetRegistryValue(%s) Value(%x) hKey: %x\r\n", szValueName,*pdwValue,hKey);
	return TRUE;
}



void DoRegSetup(WCHAR *szRegKey)
{
	HKEY hKey = NULL;
	DWORD dwDisp = 0;
	LONG nErr = RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey);
	if (nErr == ERROR_SUCCESS) {
			NKDbgPrintfW( L"Successfully opened key %s\r\n", szRegKey);
	} else {
		nErr = RegCreateKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, L"", 0, 0, NULL, &hKey, &dwDisp);
		if (nErr == ERROR_SUCCESS) {
			NKDbgPrintfW( L"Successfully created key %s\r\n", szRegKey);
		} else {
			NKDbgPrintfW( L"!!! Failed to create key %s\r\n", szRegKey);
		}
	}
	if (hKey) {
		WRITE_REG_SZ( L"Prefix", szPrefix);			
		WRITE_REG_SZ( L"Dll", szDLL);
		WRITE_REG_SZ( L"FriendlyName", szFriendlyName);
		WRITE_REG_DWORD( L"Order", 0);
		WRITE_REG_DWORD( L"Ioctl", 4);
		RegCloseKey( hKey);
	}	
}

void DoGetHandleValue(WCHAR *szRegKey, PHANDLE phDevice)
{

    HKEY hKey = NULL;
    *phDevice = NULL;
    LONG nErr = RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey);
    if (nErr == ERROR_SUCCESS) {
        if (!GetRegistryValue(hKey, L"HDEVICE", (PDWORD)phDevice)) {
            *phDevice = NULL;
        }   
    }    
    RegCloseKey( hKey);
}

void DoSetHandleValue(WCHAR *szRegKey, HANDLE hDevice)
{

    HKEY hKey = NULL;
    LONG nErr = RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey);
    if (nErr == ERROR_SUCCESS) {
        WRITE_REG_DWORD(L"HDEVICE", (DWORD)hDevice)
    }    
    RegCloseKey( hKey);
}

void DoDeleteHandleValue(WCHAR *szRegKey)
{

    HKEY hKey = NULL;
    LONG nErr = RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey);
    if (nErr == ERROR_SUCCESS) {
        RegDeleteValue( hKey, L"HDEVICE");
    }    
    RegCloseKey( hKey);
}



int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPreviousInstance, LPWSTR lpCommandLine,	int nCommandShow)
{
    HANDLE hDevice = NULL;

    DoGetHandleValue( szRegKey, &hDevice);
    if (lpCommandLine) {
        if (wcsicmp(lpCommandLine, L"load") == 0) {
            if (hDevice) {
                NKDbgPrintfW( L"The filesystem has already been loaded ..hDevice=%08X\r\n", hDevice);
                return 0;
            }
            DoRegSetup(szRegKey);
            hDevice = ActivateDevice( szRegKey, 0);
            DoSetHandleValue( szRegKey, hDevice);
        } else
        if (wcsicmp(lpCommandLine, L"unload") == 0) {
            if (!hDevice) {
                NKDbgPrintfW( L"The filesystem has not been loaded\r\n");
                return 0;
            }
            DeactivateDevice( hDevice);
            DoDeleteHandleValue(szRegKey);
        }
    }    
	return 0;
}
