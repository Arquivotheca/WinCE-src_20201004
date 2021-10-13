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
#include <windows.h>
#include "drvdbg.h"

#ifdef DEBUG
#ifdef UNDER_CE
DBGPARAM dpCurSettings = {
    TEXT("MSPART"), {
    TEXT("Store"),TEXT("Partition"),TEXT("Search"),TEXT("API"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Warning"), TEXT("Error") },
    ZONEMASK_ERROR
    };

void DumpRegKey( DWORD dwZone, PTSTR szKey, HKEY hKey)
{
    DWORD dwIndex=0;
    WCHAR szValueName[MAX_PATH];
    DWORD dwValueNameSize = MAX_PATH;
    DWORD pValueData[64];
    DWORD dwType;
    DWORD dwValueDataSize = sizeof(pValueData);
    DEBUGMSG( dwZone, (TEXT("Dumping registry for key %s \r\n"), szKey));
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
}
#endif // UNDER_CE
#endif // DEBUG
