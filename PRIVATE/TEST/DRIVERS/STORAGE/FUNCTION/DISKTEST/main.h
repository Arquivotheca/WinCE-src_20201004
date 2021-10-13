//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

BOOL ReadWriteDisk(HANDLE, DWORD, PDISK_INFO, DWORD, INT, PBYTE); 

BOOL ReadWriteDiskMulti(HANDLE, DWORD, PDISK_INFO, PSG_REQ);

BOOL ReadDiskSg(HANDLE, PDISK_INFO, DWORD, DWORD, PBYTE, DWORD);

BOOL MakeJunkBuffer(PBYTE, DWORD);
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




//
// tuxmain.cpp
//
extern CKato            *g_pKato;
extern SPS_SHELL_INFO   *g_pShellInfo;
extern TCHAR             g_szDiskName[];
extern TCHAR 			 g_szProfile[];
extern HANDLE            g_hDisk;
extern DISK_INFO         g_diskInfo;
extern BOOL              g_fOldIoctls;
extern BOOL              g_fOpenAsStore;
extern DWORD			 g_dwMaxSectorsPerOp;

#endif // __MAIN_H__
