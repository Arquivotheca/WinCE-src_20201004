//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <fsdmgrp.h>

int FsdStringFromGuid(GUID *pGuid, LPTSTR pszBuf)
{
    return wsprintf(pszBuf, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", pGuid->Data1, 
            pGuid->Data2, pGuid->Data3, pGuid->Data4[0], pGuid->Data4[1], pGuid->Data4[2], 
            pGuid->Data4[3], pGuid->Data4[4], pGuid->Data4[5], pGuid->Data4[6], pGuid->Data4[7]);
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



BOOL FsdGetRegistryValue(HKEY hKey, PCTSTR szValueName, PDWORD pdwValue)
{
    
    DWORD               dwValType, dwValLen;
    LONG                lStatus;
            
    dwValLen = sizeof(DWORD);
        
    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, (PBYTE)pdwValue, &dwValLen);
        
    if ((lStatus != ERROR_SUCCESS) || (dwValType != REG_DWORD)) {           
        DEBUGMSG( ZONE_HELPER , (TEXT("FSDMGR: RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        *pdwValue = 0;
        return FALSE;
    } 
    DEBUGMSG( ZONE_HELPER, (TEXT("FSDMGR: FsdGetRegistryValue(%s) Value(%x) hKey: %x\r\n"), szValueName,*pdwValue,hKey));
    return TRUE;
}

BOOL FsdGetRegistryString( HKEY hKey, PCTSTR szValueName, PTSTR szValue,DWORD dwSize)
{
    DWORD             dwValType, dwValLen;
    LONG                lStatus;
    
    dwValLen = 0;
    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, NULL, &dwValLen);
        
    if ((lStatus != ERROR_SUCCESS) || (dwValType != REG_SZ)) {          
        DEBUGMSG( ZONE_HELPER , (TEXT("FSDMGR: RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        wcscpy( szValue, L"");        
        return FALSE;
    }
    
    if (dwValLen > (dwSize * sizeof(WCHAR))) {
        wcscpy( szValue, L"");        
        return FALSE;
    }    
    
    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, (PBYTE)szValue, &dwValLen);

    if (lStatus != ERROR_SUCCESS) {
        DEBUGMSG( ZONE_HELPER , (TEXT("FSDMGR: RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        wcscpy( szValue, L"");        
        return FALSE;
    }    
    DEBUGMSG( ZONE_HELPER, (TEXT("FSDMGR: FsdGetRegistryString(%s) Value(%s) hKey: %x\r\n"), szValueName, szValue, hKey));
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
