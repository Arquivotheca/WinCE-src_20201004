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

#if defined(DEBUG) 

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
            TEXT("StoreMgr"),          // 0x0100
            TEXT("Verbose"),           // 0x0200
            TEXT("Security"),          // 0x0400
            TEXT("Undefined"),          // 0x0800
            TEXT("Undefined"),          // 0x1000
            TEXT("Undefined"),          // 0x2000
            TEXT("Undefined"),          // 0x4000
            TEXT("Undefined"),          // 0x8000
        },
        ZONEMASK_DEFAULT
};


void DumpRegKey( DWORD dwZone, PCTSTR szKey, HKEY hKey)
{
#ifdef UNDER_CE
    DWORD dwIndex=0;
    WCHAR szValueName[MAX_PATH];
    DWORD dwValueNameSize = MAX_PATH;
    DWORD pValueData[64];
    DWORD dwType;
    DWORD dwValueDataSize = sizeof(pValueData);
    DEBUGMSG( dwZone, (TEXT("FSDMGR!DumpRegKey: Dumping registry for key %s \r\n"), szKey));
    while(ERROR_SUCCESS == RegEnumValue( hKey, dwIndex, szValueName, &dwValueNameSize, NULL, &dwType, (BYTE*)pValueData, &dwValueDataSize)) {
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
#endif // UNDER_CE
}

#endif // defined(DEBUG)
