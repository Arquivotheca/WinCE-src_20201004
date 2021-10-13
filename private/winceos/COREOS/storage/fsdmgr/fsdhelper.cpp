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
#include "storeincludes.hpp"
#include <intsafe.h>

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
    if (FAILED (::StringCchLengthW (pSource, MaxLen, &SourceNameChars))
        || FAILED (::SizeTAdd (1, SourceNameChars, &SourceNameChars))) {
        return ERROR_INVALID_PARAMETER;
    }

    WCHAR* pDest = new WCHAR[SourceNameChars];
    if (!pDest) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    if (FAILED (::StringCchCopyW (pDest, SourceNameChars, pSource))) {
        delete[] pDest;
        return ERROR_INVALID_PARAMETER;
    }

    *ppDest = pDest;
    return ERROR_SUCCESS;
}

template<typename T>
static inline BOOL IsInRange(T id, T idFirst, T idLast)
{
    return ((UINT)((id)-(idFirst)) <= (UINT)((idLast)-(idFirst)));
}

size_t FsdStringFromGuid(const GUID* pGuid, __out_ecount(MaxLen) WCHAR* psz, size_t MaxLen)
{
    static const size_t c_GuidStringMax = 1 + 8 + 1 + 4 + 1 + 4 + 1 + 4 + 1 + 12 + 1 + 1; // {00000000-0000-0000-0000-000000000000}\0
    static const WCHAR  c_wszDigits[]   = L"0123456789ABCDEF";

    // Defines which byte of a GUID structure goes in each character position in the guid
    // string. '-' characters should be replaced by L'-' instead of GUID data.
    static const BYTE   c_GuidMap[]     = { '{', 3, 2, 1, 0, '-', 5, 4, '-', 7, 6, '-', 8, 9, '-', 10, 11, 12, 13, 14, 15, '}' };

    const BYTE* pBytes = (const BYTE*) pGuid;

    if (MaxLen < c_GuidStringMax)
    {
        return 0;
    }

    size_t pos = 0;
    for (int i = 0; (i < sizeof(c_GuidMap)) && (pos < MaxLen - 1); i++)
    {
        BYTE b = c_GuidMap[i];
        if (IsInRange(b, (BYTE)0, (BYTE)(sizeof(GUID)-1)))
        {
            // b is a byte offset into the GUID structure; convert the byte at
            // that offset into a pair of characters using c_GuidMap
            psz[pos++] = c_wszDigits[(pBytes[b] & 0xF0) >> 4];
            if (pos < MaxLen - 1) {
                psz[pos++] = c_wszDigits[(pBytes[b] & 0x0F)];
            }
        }
        else
        {
            // b is a literal character value
            psz[pos++] = (WCHAR)b;
        }
    }

    psz[pos] = '\0';

    // Return the number of WCHARs copied to output buffer excluding terminating null (like sprintf).
    return c_GuidStringMax - 1;
}

// scan psz for a number of hex digits (at most 8); update psz, return
// value in Value; check for chDelim; return TRUE for success.
static BOOL HexStringToDword(__inout LPCWSTR * ppsz, __out DWORD* pdwValue, int cDigits, WCHAR wchDelim)
{
    int ich;
    LPCWSTR psz = *ppsz;
    DWORD Value = 0;
    BOOL fRet = TRUE;

    for (ich = 0; ich < cDigits; ich++)
    {
        WCHAR ch = psz[ich];

        if (IsInRange(ch, L'0', L'9'))
        {
            Value = (Value << 4) + ch - L'0';
        }
        else if (IsInRange( (ch |= (L'a' - L'A')), L'a', L'f') )
        {
            Value = (Value << 4) + ch - L'a' + 10;
        }
        else
        {
            return FALSE;
        }
    }

    if (wchDelim)
    {
        fRet = (psz[ich++] == wchDelim);
    }

    *pdwValue = Value;
    *ppsz = psz + ich;

    return fRet;
}

BOOL FsdGuidFromString(__in LPCWSTR psz, __out GUID *pguid)
{
    DWORD dw = 0;

    if (*psz++ != L'{')
    {
        return FALSE;
    }

    if (!HexStringToDword(&psz, &pguid->Data1, sizeof(DWORD)*2, L'-'))
    {
        return FALSE;
    }

    if (!HexStringToDword(&psz, &dw, sizeof(WORD)*2, L'-'))
    {
        return FALSE;
    }

    pguid->Data2 = (WORD)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(WORD)*2, L'-'))
    {
        return FALSE;
    }

    pguid->Data3 = (WORD)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, 0))
    {
        return FALSE;
    }

    pguid->Data4[0] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, L'-'))
    {
        return FALSE;
    }

    pguid->Data4[1] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, 0))
    {
        return FALSE;
    }

    pguid->Data4[2] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, 0))
    {
        return FALSE;
    }

    pguid->Data4[3] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, 0))
    {
        return FALSE;
    }

    pguid->Data4[4] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, 0))
    {
        return FALSE;
    }

    pguid->Data4[5] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, 0))
    {
        return FALSE;
    }

    pguid->Data4[6] = (BYTE)dw;

    if (!HexStringToDword(&psz, &dw, sizeof(BYTE)*2, L'}'))
    {
        return FALSE;
    }

    pguid->Data4[7] = (BYTE)dw;

    return !(*psz); // should have reached the end of the string
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

