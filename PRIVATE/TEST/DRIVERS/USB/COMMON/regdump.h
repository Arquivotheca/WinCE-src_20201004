//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __REGDUMP_H__
#define __REGDUMP_H__

#include    <windows.h>
#include    <tchar.h>

#ifdef  __cplusplus
extern "C" {
#endif

void RegDumpKey(HKEY hTopParentKey, LPTSTR szChildKeyName, int level);
void DumpChild(HKEY hKey, int level);

#ifdef  __cplusplus
}
#endif  // __cplusplus

#endif  // __REGDUMP_H__
