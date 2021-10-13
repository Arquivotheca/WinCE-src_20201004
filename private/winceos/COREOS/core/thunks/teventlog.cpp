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


#define BUILDING_FS_STUBS
#include <windows.h>

extern "C"
BOOL WINAPI xxx_ReportEventW(HANDLE hEventLog, WORD wType, WORD wCategory, DWORD dwEventID,  
                 PSID lpUserSid, WORD wNumStrings, DWORD dwDataSize, 
                 LPCWSTR* lpStrings, LPVOID lpRawData)
{
    // The PSL expects DWORDs, not WORDs. 
    DWORD dwType = (DWORD)wType;
    DWORD dwCategory = (DWORD)wCategory;
    DWORD dwNumStrings = (DWORD)wNumStrings;
    return ReportEventW_Trap(hEventLog, dwType, dwCategory, dwEventID,  lpUserSid, dwNumStrings, dwDataSize,  lpStrings, lpRawData);
}

extern "C"
HANDLE WINAPI xxx_RegisterEventSourceW(LPCWSTR lpUNCServerName,  LPCWSTR lpSourceName) {
    return RegisterEventSourceW_Trap(lpUNCServerName,  lpSourceName);
}

extern "C"
BOOL WINAPI xxx_DeregisterEventSource(HANDLE hEventLog) {
    return DeregisterEventSource_Trap(hEventLog);
}


extern "C"
BOOL WINAPI xxx_ClearEventLogW(HANDLE hEventLog, LPCWSTR lpBackupFileName) {
    return ClearEventLogW_Trap( hEventLog, lpBackupFileName) ;
}

extern "C"
HANDLE xxx_OpenEventLogW(LPCTSTR lpUNCServerName, LPCTSTR lpSourceName)
{
    return OpenEventLog_Trap(lpUNCServerName, lpSourceName);
}

extern "C"
BOOL xxx_CloseEventLog(HANDLE hEventLog)
{
    return CloseEventLog_Trap(hEventLog);
}

extern "C"
BOOL xxx_BackupEventLogW(HANDLE hEventLog, LPCTSTR szBackupFileName)
{
    return BackupEventLogW_Trap(hEventLog, szBackupFileName);
}

extern "C"
BOOL xxx_LockEventLog(HANDLE hEventLog)
{
    return LockEventLog_Trap(hEventLog);
}

extern "C"
BOOL xxx_UnLockEventLog(HANDLE hEventLog)
{
    return UnLockEventLog_Trap(hEventLog);
}

extern "C"
BOOL xxx_ReadEventLogRaw(HANDLE hEventLog, BYTE *pReadBuffer, DWORD dwReadBufferSize, DWORD *pdwBytesRead)
{
    return ReadEventLogRaw_Trap(hEventLog, pReadBuffer, dwReadBufferSize, pdwBytesRead);
}


