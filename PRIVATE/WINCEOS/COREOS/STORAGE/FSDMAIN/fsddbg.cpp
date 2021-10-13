//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <fsddbg.h>

#ifdef DEBUG

DBGPARAM dpCurSettings = {
        TEXT("FSDMGR"), {
            TEXT("Init"),              // 0x0001
            TEXT("Errors"),            // 0x0002
            TEXT("Power"),             // 0x0004
            TEXT("Events"),            // 0x0008
            TEXT("Disk I/O"),          // 0x0010
            TEXT("APIs"),              // 0x0020
            TEXT("Helper"),            // 0x0040
            TEXT("StoreApi"),          // 0x0080
            TEXT("Undefined"),          // 0x0100
            TEXT("Undefined"),          // 0x0200
            TEXT("Undefined"),          // 0x0400
            TEXT("Undefined"),          // 0x0800
            TEXT("Undefined"),          // 0x1000
            TEXT("Undefined"),          // 0x2000
            TEXT("Undefined"),          // 0x4000
            TEXT("Undefined"),          // 0x8000
        },
        0
};


void DumpRegKey( DWORD dwZone, PCTSTR szKey, HKEY hKey)
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
