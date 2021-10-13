//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
  Definition specific to Windows CE
*/
#ifndef _CEDEFS_H_
#define _CEDEFS_H_

__inline void GetSystemTimeAsFileTime(LPFILETIME lpft)
{
    SYSTEMTIME systime;
    GetSystemTime(&systime);
    SystemTimeToFileTime(&systime,lpft);
}

__inline HANDLE OpenFileMapping (DWORD dwAccess, BOOL bInherit, LPCTSTR lpName) {
	return CreateFileMapping ((HANDLE)0xffffffff, NULL, 0, 0, 0, lpName);
}

#define lstrlenA strlen
#define lstrcpyA strcpy
#define wsprintfA sprintf
#define lstrcpyn _tcsncpy

// TraceTagIds are the identifiers for tracing areas, and are used in calls
// to TraceTag.  We need this defined outside of of ENABLETRACE so that
// calls to the TraceTag macro don't break when ENABLETRACE is not defined.
//
// Hungarian == ttid
//
extern BYTE TtidToZoneId[];

#define TTIDTOZONE(ttid) (dpCurSettings.ulZoneMask & (1 << TtidToZoneId[ttid]))
#define OutputDebugStringA(string) NKDbgPrintfW(L"%a",string)
#endif _CEDEFS_H_

