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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef __MAIN_H__
#define __MAIN_H__

#include <windows.h>
#include <diskio.h>
#include <atapi.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <storemgr.h>
#include <devutils.h>
#include <clparse.h>
#include <FatfspowerUtil.h>
#include <storehlp.h>

//
// global macros
//
#define INVALID_HANDLE(X)   (INVALID_HANDLE_VALUE == X || NULL == X)
#define VALID_HANDLE(X)     (INVALID_HANDLE_VALUE != X && NULL != X)

#define CALIBRATION_COUNT 10

#ifdef DEBUG
#define NKDBG               NKDbgPrintfW
#else
#define NKDBG               (VOID)
#endif

#define NKMSG               NKDbgPrintfW

#define MAX_DISK            10 // maximum # of disks to check (e.g. DSK1, DSK2)

#define MAX_BUFFER_SIZE     (1024 * 1024) // 1MB buffer size
#define READ_LOCATIONS      1000 // tests will hit every 1/READ_LOCATIONS position on disk

#define DEF_MAX_SECTORS		128
#define lptStrUnknownManufacturer TEXT("Unknown Manufacturer")
// getopt.cpp
INT WinMainGetOpt(LPCTSTR, LPCTSTR);

// disk.cpp
HANDLE OpenDiskHandle(VOID);

BOOL FormatMedia(HANDLE);

BOOL DeleteSectors(HANDLE, DWORD, DWORD);

BOOL ReadWriteDisk(HANDLE, DWORD, PDISK_INFO, DWORD, INT, PBYTE); 

BOOL ReadWriteDiskMulti(HANDLE, DWORD, PDISK_INFO, PSG_REQ);

BOOL ReadDiskSg(HANDLE, PDISK_INFO, DWORD, DWORD, PBYTE, DWORD);

BOOL MakeJunkBuffer(PBYTE, DWORD);
//
// test procedures
//

TESTPROCAPI	TestReadWritePower (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

LPTSTR GetManufacturer(UCHAR code);

VOID Calibrate(void);
//
// tuxmain.cpp
//
extern CKato            *g_pKato;
extern SPS_SHELL_INFO   *g_pShellInfo;
extern CFSInfo          *g_pFsInfo;

extern TCHAR             g_szPath[];
extern DWORD             g_dwFileSize;
extern DWORD             g_dwStartFileSize;
extern DWORD             g_dwEndFileSize;
extern DWORD             g_dwStartTransferSize;
extern DWORD             g_dwEndTransferSize;
extern DWORD             g_dwStartDiskUsage;
extern DWORD             g_dwEndDiskUsage;
extern DWORD             g_dwStartClusterSize;
extern DWORD             g_dwEndClusterSize;
extern DWORD             g_dwStartFATCacheSize;
extern DWORD             g_dwEndFATCacheSize;
extern DWORD             g_dwStartDATACacheSize;
extern DWORD             g_dwEndDATACacheSize;
extern BOOL              g_fWriteThrough;
extern BOOL              g_fUseCache;

extern BOOL              g_fFATCache;
extern BOOL              g_fDATACache;
extern BOOL              g_fSetFilePointerEOF;
extern BOOL              g_fTFAT;
#endif // __MAIN_H__
