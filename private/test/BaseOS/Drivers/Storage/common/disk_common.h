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
#ifndef __DISK_COMMON_H__
#define __DISK_COMMON_H__

#include <windows.h>
#include <diskio.h>
#include <atapi.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <storemgr.h>
#include <devutils.h>
#include <clparse.h>


//
// global macros
//
#define INVALID_HANDLE(X)   (INVALID_HANDLE_VALUE == X || NULL == X)
#define VALID_HANDLE(X)     (INVALID_HANDLE_VALUE != X && NULL != X)

#ifdef DEBUG
#define NKDBG               NKDbgPrintfW
#else
#define NKDBG               (VOID)
#endif
#define NKMSG               NKDbgPrintfW

#define COMMON_USAGE_STRING _T("-c\"/disk <disk> /profile <profile>")
// disk.cpp
HANDLE OpenDiskHandleByDiskName(CKato* pKato,BOOL bOpenAsStore,TCHAR* szDiskName,BOOL bOpenAsPartition=FALSE);
HANDLE OpenDiskHandleByProfile(CKato* pKato,BOOL bOpenAsStore,TCHAR* szProfile,BOOL bOpenAsPartition=FALSE,LPTSTR szDiskName=NULL,DWORD dwDiskNameSize=0);
HANDLE OpenDevice(LPCTSTR pszDiskName,BOOL OpenAsStore,BOOL OpenAsPartition=FALSE);

HANDLE OpenDiskHandleByDiskNameRW(CKato* pKato,BOOL bOpenAsStore,TCHAR* szDiskName,BOOL bOpenAsPartition=FALSE);
HANDLE OpenDiskHandleByProfileRW(CKato* pKato,BOOL bOpenAsStore,TCHAR* szProfile,BOOL bOpenAsPartition=FALSE,LPTSTR szDiskName=NULL,DWORD dwDiskNameSize=0);
HANDLE OpenDeviceRW(LPCTSTR pszDiskName,BOOL OpenAsStore,BOOL OpenAsPartition=FALSE);


BOOL GetDiskInfo( HANDLE hDisk,LPWSTR pszDisk,DISK_INFO *pDiskInfo,CKato* pKato);
void ProcessCmdLineCommon(LPCTSTR szCmdLine,CKato* pKato );
void UsageCommon(LPCTSTR szTestName,CKato* pKato);

extern TCHAR             g_szDiskName[];
extern TCHAR 			 g_szProfile[];
extern BOOL              g_fOpenAsStore;
extern BOOL              g_fOpenAsPartition;
extern DISK_INFO         g_diskInfo;
#endif // __DISK_COMMON_H__
