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

#include <multiperflogger.h>
#include <disk_common.h>

void LOG(LPCTSTR szFormat, ...);

// getopt.cpp
INT WinMainGetOpt(LPCTSTR, LPCTSTR);

// disk.cpp
BOOL OpenDevice();

BOOL FormatMedia(HANDLE);

BOOL ReadWriteDisk(HANDLE, DWORD, PDISK_INFO, DWORD, INT, PBYTE); 

BOOL ReadDiskSg(HANDLE, PDISK_INFO, DWORD, DWORD, PBYTE, DWORD);

 BOOL MakeJunkBuffer(PBYTE, DWORD);
//
// test procedures
//
TESTPROCAPI TestReadWriteSeq   (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestReadWriteSeq_Common   (DWORD, BOOL bAllSectors = FALSE);

TESTPROCAPI TestReadWriteMulti   (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestReadWriteMulti_Common   (DWORD);

LPTSTR GetManufacturer(UCHAR code);

VOID Calibrate(void);

//
// tuxmain.cpp
//
// disk handle
extern HANDLE			g_hDisk;

#endif // __MAIN_H__
