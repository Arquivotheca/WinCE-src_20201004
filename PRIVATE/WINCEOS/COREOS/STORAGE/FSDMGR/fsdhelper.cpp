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
#include "storeincludes.hpp"

LRESULT FsdGetLastError (LRESULT NoErrorResult)
{
    LRESULT lResult = GetLastError ();
    if (ERROR_SUCCESS == lResult) {
        lResult = NoErrorResult;
    }
    return lResult;
}

LRESULT FsdDuplicateString (WCHAR** ppDest, const WCHAR* pSource, size_t MaxLen)
{
    size_t SourceNameChars;
    if (FAILED (::StringCchLengthW (pSource, MaxLen, &SourceNameChars))) {
        return ERROR_INVALID_PARAMETER;
    }

    WCHAR* pDest = new WCHAR[SourceNameChars + 1];
    if (!pDest) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    if (FAILED (::StringCchCopyNW (pDest, SourceNameChars + 1, pSource, SourceNameChars))) {
        delete[] pDest;
        return ERROR_INVALID_PARAMETER;
    }

    *ppDest = pDest;
    return ERROR_SUCCESS;
}

size_t FsdStringFromGuid(const GUID *pGuid, __out_ecount(MaxLen) WCHAR* pString, size_t MaxLen)
{
    size_t Length = 0;
    if (SUCCEEDED (StringCchPrintfW (pString, MaxLen, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
            pGuid->Data1, pGuid->Data2, pGuid->Data3, pGuid->Data4[0], pGuid->Data4[1], pGuid->Data4[2],
            pGuid->Data4[3], pGuid->Data4[4], pGuid->Data4[5], pGuid->Data4[6], pGuid->Data4[7])) &&
        SUCCEEDED (StringCchLengthW (pString, MaxLen, &Length)))
    {
            return Length;
    }
    return 0;
}

#define GUID_FORMAT_W   L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"

BOOL FsdGuidFromString(LPCTSTR pszGuid,GUID *pGuid)
{
    UINT Data4[8];
    int  Count;

    if (swscanf(pszGuid,
              GUID_FORMAT_W,
              &pGuid->Data1, 
              &pGuid->Data2, 
              &pGuid->Data3, 
              &Data4[0], 
              &Data4[1], 
              &Data4[2], 
              &Data4[3], 
              &Data4[4], 
              &Data4[5], 
              &Data4[6], 
              &Data4[7]) != 11) 
    {
        return FALSE;
    }

    for(Count = 0; Count < sizeof(Data4)/sizeof(Data4[0]); Count++) 
    {
        pGuid->Data4[Count] = (UCHAR)Data4[Count];
    }

    return TRUE;
}

BOOL FsdLoadFlag(HKEY hKey,const TCHAR *szValueName, PDWORD pdwFlag, DWORD dwSet)
{
    DWORD dwValue;
    if (FsdGetRegistryValue(hKey, szValueName, &dwValue)){
        if (dwValue == 1) 
            *pdwFlag |= dwSet;
        else
            *pdwFlag &= ~dwSet;
        return TRUE;            
    }
    return FALSE;
}

#ifdef UNDER_CE

BOOL FsdGetRegistryValue(HKEY hKey, PCTSTR szValueName, PDWORD pdwValue)
{
    
    DWORD               dwValType, dwValLen;
    LONG                lStatus;
            
    dwValLen = sizeof(DWORD);

    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, NULL, &dwValLen);
        
    if ((lStatus != ERROR_SUCCESS) || (dwValType != REG_DWORD)) {           
        DEBUGMSG( ZONE_HELPER , (TEXT("FSDMGR!FsdGetRegistryValue: RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        *pdwValue = 0;
        return FALSE;
    } 

    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, (PBYTE)pdwValue, &dwValLen);

    if ((lStatus != ERROR_SUCCESS)) {           
        DEBUGMSG( ZONE_HELPER , (TEXT("FSDMGR!FsdGetRegistryValue: RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        *pdwValue = 0;
        return FALSE;
    } 
   
    DEBUGMSG( ZONE_HELPER, (TEXT("FSDMGR!FsdGetRegistryValue(%s): Value(%x) hKey: %x\r\n"), szValueName,*pdwValue,hKey));
    return TRUE;
}

BOOL FsdGetRegistryString( HKEY hKey, PCTSTR szValueName, __out_ecount(dwSize) PTSTR szValue,DWORD dwSize)
{
    DWORD             dwValType, dwValLen;
    LONG                lStatus;
    
    dwValLen = 0;
    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, NULL, &dwValLen);
        
    if ((lStatus != ERROR_SUCCESS) || ((dwValType != REG_SZ) && (dwValType != REG_MULTI_SZ))) {
        DEBUGMSG( ZONE_HELPER , (TEXT("FSDMGR!FsdGetRegistryString: RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        StringCchCopyW (szValue, dwSize, L"");
        return FALSE;
    }
    
    if (dwValLen > (dwSize * sizeof(WCHAR))) {
        StringCchCopyW (szValue, dwSize, L"");
        return FALSE;
    }    
    
    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, (PBYTE)szValue, &dwValLen);

    if (lStatus != ERROR_SUCCESS) {
        DEBUGMSG( ZONE_HELPER , (TEXT("FSDMGR!FsdGetRegistryString: RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        StringCchCopyW (szValue, dwSize, L"");
        return FALSE;
    }    
    DEBUGMSG( ZONE_HELPER, (TEXT("FSDMGR!FsdGetRegistryString(%s): Value(%s) hKey: %x\r\n"), szValueName, szValue, hKey));
    return TRUE;
}

DWORD FsdRegEnumKey(HKEY hKey, DWORD dwIndex, __out_ecount_opt(*lpcchName) LPWSTR lpName, __inout LPDWORD lpcchName)
{
    return RegEnumKeyEx(hKey, dwIndex, lpName, lpcchName, NULL, NULL, NULL, NULL);
}

DWORD FsdRegOpenSubKey(HKEY hKeyRoot, const TCHAR *szSubKey, HKEY *phKey)
{
    // open registry sub-key
    return RegOpenKeyEx(hKeyRoot, szSubKey, 0, 0, phKey);
}

DWORD FsdRegOpenKey(const TCHAR *szSubKey, HKEY *phKey)
{
    // fsdmgr keys are all under HKLM
    return RegOpenKeyEx(HKEY_LOCAL_MACHINE, szSubKey, 0, 0, phKey);
}

DWORD FsdRegCloseKey(HKEY hKey)
{
    return RegCloseKey(hKey);
}

FARPROC FsdGetProcAddress(HMODULE hModule, LPCWSTR lpProcName)
{
    return GetProcAddress(hModule, lpProcName);
}

BOOL FsdDuplicateHandle(
    HANDLE hSourceProcessHandle,
    HANDLE hSourceHandle,
    HANDLE hTargetProcessHandle,
    LPHANDLE lpTargetHandle,
    DWORD dwDesiredAccess,
    BOOL bInheritHandle,
    DWORD dwOptions
    )
{
    return DuplicateHandle (hSourceProcessHandle, hSourceHandle,
        hTargetProcessHandle, lpTargetHandle, dwDesiredAccess, 
        bInheritHandle, dwOptions);
}


#endif

