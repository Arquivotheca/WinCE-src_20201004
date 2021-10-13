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
#ifndef __FSDHELPER_HPP__
#define __FSDHELPER_HPP__

// Miscellaneous internal fsdmgr helpers. These functions are not exported.
LRESULT FsdGetLastError (LRESULT NoErrorResult = ERROR_GEN_FAILURE);
LRESULT FsdDuplicateString (WCHAR** ppDest, const WCHAR* pSource, size_t MaxLen);
size_t FsdStringFromGuid(const GUID *pGuid, __out_ecount(MaxLen) WCHAR* pString, size_t MaxLen);
BOOL FsdGuidFromString(LPCTSTR pszGuid,GUID *pGuid);

// Registry helper functions. Under NT, these parse the .ini file instead.
BOOL FsdLoadFlag(HKEY hKey,const TCHAR *szValueName, PDWORD pdwFlag, DWORD dwSet);
BOOL FsdGetRegistryValue(HKEY hKey, PCTSTR szValueName, PDWORD pdwValue);
BOOL FsdGetRegistryString( HKEY hKey, PCTSTR szValueName, __out_ecount(dwSize) PTSTR szValue,DWORD dwSize);
DWORD FsdRegEnumKey(HKEY hKey, DWORD dwIndex, __out_ecount_opt(*lpcchName) LPWSTR lpName, __inout LPDWORD lpcchName);
DWORD FsdRegOpenSubKey(HKEY hKeyRoot, const TCHAR *szSubKey, HKEY *phKey);
DWORD FsdRegOpenKey(const TCHAR *szSubKey, HKEY *phKey);
DWORD FsdRegCloseKey(HKEY hKey);
FARPROC FsdGetProcAddress(HMODULE hModule, LPCWSTR lpProcName);
BOOL FsdDuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle,
    LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);

#endif // __FSDHELPER_HPP__

