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
////////////////////////////////////////////////////////////////////////////////
//
//  FLSHWEAR TUX DLL
//
//  Module: globals.h
//          Declares all global variables and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES global
//          variables, including the function table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifdef __GLOBALS_CPP__
#define GLOBAL
#define INIT(x) = x
#else // __GLOBALS_CPP__
#define GLOBAL  extern
#define INIT(x)
#endif // __GLOBALS_CPP__

////////////////////////////////////////////////////////////////////////////////
// Global macros

#define countof(x)  (sizeof(x)/sizeof(*(x)))

////////////////////////////////////////////////////////////////////////////////
// Global function prototypes

void            Debug(LPCTSTR, ...);
SHELLPROCAPI    ShellProc(UINT, SPPARAM);

////////////////////////////////////////////////////////////////////////////////
// TUX Function table

#include "ft.h"

////////////////////////////////////////////////////////////////////////////////
// Globals

// Global CKato logging object. Set while processing SPM_LOAD_DLL message.
GLOBAL CKato            *g_pKato INIT(NULL);

// Global shell info structure. Set while processing SPM_SHELL_INFO message.
GLOBAL SPS_SHELL_INFO   *g_pShellInfo;

// The number of writes to perform per disk sector (e.g. total writes = writeCount * totalSectors)
GLOBAL DWORD            g_cRepeat;

// The number of sectors to write simultaneously
GLOBAL DWORD            g_cSectors;

// The number of sectors to write in total
GLOBAL DWORD            g_totalWrites;

// Flag which will enable IOCTL_DISK_DELETE_SECTORS call after each write
GLOBAL BOOL             g_fDelete;

GLOBAL PerfLogStub      *g_pReadPerfLog;
GLOBAL PerfLogStub      *g_pWritePerfLog;

GLOBAL BOOL             g_filled[100];

//flag which will enable IOCTL_FLASH* ioctls instead of normal IOCTL_DISK
GLOBAL BOOL             g_bUseFlashIOCTLs;

//structure containing information about the current flash partition
//will only be used if /flash is sent via the cmd line
//contains PARTITION_ID necessary for flash IOCTLs
GLOBAL FLASH_PARTITION_INFO
                        g_FlashPartInfo;

GLOBAL FLASH_REGION_INFO 
                        g_FlashRegionInfo;

// Add more globals of your own here. There are two macros available for this:
//  GLOBAL  Precede each declaration/definition with this macro.
//  INIT    Use this macro to initialize globals, instead of typing "= ..."
//
// For example, to declare two DWORDs, one uninitialized and the other
// initialized to 0x80000000, you could enter the following code:
//
//  GLOBAL DWORD        g_dwUninit,
//                      g_dwInit INIT(0x80000000);
////////////////////////////////////////////////////////////////////////////////

#endif // __GLOBALS_H__
