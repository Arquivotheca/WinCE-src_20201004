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
#include <atapi2.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <cdioctl.h>

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

#define DEVICE_NAME_LEN           4 // 3 characters + 1 digit (e.g. COM1, DSK2)

#define MAX_DISK                  4       // max number of dsk devices
#define CD_SECTOR_SIZE            2048    // sector size in bytes
#define CD_RAW_SECTOR_SIZE        2352    // for raw data reads
#define MAX_READ_SECTORS          ((128 * 1024) / CD_SECTOR_SIZE)
#define MAX_READ_RETRIES          5
#define CD_MAX_SECTORS            328220  // sector count for CD
#define DVD_MAX_SECTORS           3725375 // sector count for DVD

// PPFS file definitions
#define _O_RDONLY   0x0000  /* open for reading only */
#define _O_WRONLY   0x0001  /* open for writing only */
#define _O_RDWR     0x0002  /* open for reading and writing */
#define _O_APPEND   0x0008  /* writes done at eof */

#define _O_EXIST    0x0000  /* open existing file */
#define _O_CREAT    0x0100  /* create and open file */
#define _O_TRUNC    0x0200  /* open and truncate */
#define _O_EXCL     0x0400  /* open only if file doesn't already exist */

// ppsh file creation definitions for image file
#define PPSH_WRITE_ACCESS         _O_WRONLY | _O_CREAT // 0x00090001 // write access, create always
#define PPSH_READ_ACCESS          _O_RDONLY | _O_EXIST // 0x00000000 // read access, open existing

// default name for the cd image file
#define DEF_IMG_FILE_NAME         _T("cdsector.dat");

// getopt.cpp
INT WinMainGetOpt(LPCTSTR, LPCTSTR);

//
// test procedures
//
TESTPROCAPI TestUnitReady       (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestReadTOC         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestRead            (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestReadMulti       (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestCDRomInfo       (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestMultiSgRead     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestReadSize        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestMSFAudio        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestMSFAudioData    (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestEjectMedia      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestIssueInquiry    (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// 
// utility test procedures
//
TESTPROCAPI UtilMakeCDImage     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

//
// cdrom.cpp
//
HANDLE OpenCDRomDiskHandle();
BOOL ReadDiskSectors(HANDLE, DWORD, PBYTE, DWORD, LPDWORD);
BOOL ReadDiskMulti(HANDLE, DWORD, DWORD, PBYTE, DWORD, LPDWORD);

//
// tuxmain.cpp
//
extern CKato            *g_pKato;
extern SPS_SHELL_INFO   *g_pShellInfo;
extern TCHAR             g_szDiskName[];
extern TCHAR             g_szImgFile[];
extern HANDLE            g_hDisk;
extern BOOL              g_fIsDVD;
extern DWORD             g_endSector;
extern DWORD             g_cMaxSectors;
extern DWORD             g_cMaxReadRetries;
extern DWORD             g_cTrackNo;

#endif // __MAIN_H__

