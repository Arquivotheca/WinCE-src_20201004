//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <drvdbg.h>

#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("DOSPART"), {
    TEXT("Store"),TEXT("Partition"),TEXT("Search"),TEXT("API"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Warning"), TEXT("Error") },
    0
    };

void DumpRegKey( DWORD dwZone, PTSTR szKey, HKEY hKey)
{
    DWORD dwIndex=0;
    WCHAR szValueName[MAX_PATH];
    DWORD dwValueNameSize = MAX_PATH;
    BYTE pValueData[256];
    DWORD dwType;
    DWORD dwValueDataSize = sizeof(pValueData);
    DEBUGMSG( dwZone, (TEXT("Dumping registry for key %s \r\n"), szKey));
    while(ERROR_SUCCESS == RegEnumValue( hKey, dwIndex, szValueName, &dwValueNameSize, NULL, &dwType, pValueData, &dwValueDataSize)) {
        if (REG_SZ == dwType) {
            DEBUGMSG( dwZone, (TEXT("\t\t%s = %s\r\n"), szValueName, (LPWSTR)pValueData));
        } else
        if (REG_DWORD == dwType) {
            DEBUGMSG( dwZone, (TEXT("\t\t%s = %08X\r\n"), szValueName, *(PDWORD)pValueData));
        }
        dwIndex++;
        dwValueDataSize = sizeof(pValueData);
        dwValueNameSize = MAX_PATH;
    }
}

#endif
