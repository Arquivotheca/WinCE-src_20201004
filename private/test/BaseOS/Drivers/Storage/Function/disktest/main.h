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
#pragma once

#include <windows.h>
#include <diskio.h>
#include <atapi.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <storemgr.h>
#include <devutils.h>
#include <clparse.h>
#include <disk_common.h>


//
// global macros
//
#define INVALID_HANDLE(X)   (INVALID_HANDLE_VALUE == X || NULL == X)
#define VALID_HANDLE(X)     (INVALID_HANDLE_VALUE != X && NULL != X)

#define MAX_DWORD		0xFFFFFFFF

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

// getopt.cpp
INT WinMainGetOpt(LPCTSTR, LPCTSTR);

// disk.cpp
HANDLE OpenDiskHandle(VOID);

BOOL FormatMedia(HANDLE);

BOOL DeleteSectors(HANDLE, DWORD, DWORD);

BOOL ReadWriteDisk(HANDLE, DWORD, PDISK_INFO, DWORD, INT, PBYTE, LPDWORD); 

BOOL ReadWriteDiskMulti(HANDLE, DWORD, PDISK_INFO, PSG_REQ, LPDWORD);

BOOL ReadDiskSg(HANDLE, PDISK_INFO, DWORD, DWORD, PBYTE, DWORD);

BOOL MakeJunkBuffer(PBYTE, DWORD);

DWORD MaxSectorPerOp(PDISK_INFO pDiskInfo);

//
// test procedures
//
TESTPROCAPI	TestGetDiskName (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI	TestFormatMedia (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestReadWriteSeq   (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestReadWriteSize   (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestReadWriteMulti(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestReadPastEnd(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestReadOffDisk(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestWritePastEnd(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestWriteOffDisk(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestSGBoundsCheck(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestInvalidDeleteSectors(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI 
TestSGWrite(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
TestSGRead(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
TestReadBufferTypes(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
TestWriteBufferTypes(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
TestReadWriteInvalid(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
TestReadWriteInvalidMaxSG(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
TestReadWriteInvalidSG(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
TestReadWriteInvalidIoctl(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE );

//
// tuxmain.cpp
//
extern CKato            *g_pKato;
extern SPS_SHELL_INFO   *g_pShellInfo;
extern HANDLE            g_hDisk;
extern BOOL              g_fOldIoctls;
extern DWORD			 g_dwMaxSectorsPerOp;




