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

#ifndef BASEBVT_H     
#define BASEBVT_H

#include "kharness.h"
#include "ycalls.h"

#define    VARIATION    DWORD

#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14

static WCHAR gszHelp [] = {
    TEXT("/p:num        number to be multiplied with the page size for heap allocation\n")    
    TEXT("/v:num        number to be multiplied with the page size for virtual alloc\n")
    TEXT("/l:num        number to be multiplied with the page size for local alloc \n")
    TEXT("/d:string     name of the directory to be created\n")
    TEXT("/f:string     name of the file to be created\n")
};

BOOL ParseCmdLine (LPWSTR);

// Macros for suppressing/enabling exception notifications while debugging
#define DISABLEFAULTS() (UTlsPtr () [TLSSLOT_KERNEL] |= (TLSKERN_NOFAULTMSG | TLSKERN_NOFAULT))
#define ENABLEFAULTS()  (UTlsPtr () [TLSSLOT_KERNEL] &= ~(TLSKERN_NOFAULTMSG | TLSKERN_NOFAULT))

// Test function prototypes (TestProc's)
TESTPROCAPI BaseFileIoTest         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BaseTaskTest           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BaseMemoryTest         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BaseHeapTest           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BaseModuleTest         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every 
// test case within the entire team to be uniquely identified.

#define BASE 0x00010000

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
   TEXT("Kernel BVT"               ), 0,   0,         0,      NULL,
   TEXT( "Memory Test"             ), 1,   0,         BASE+1, BaseMemoryTest,
   TEXT( "Heap Test"               ), 1,   0,         BASE+2, BaseHeapTest,
   TEXT( "File System"             ), 1,   0,         BASE+3, BaseFileIoTest,
   TEXT( "Tasks"                   ), 1,   0,         BASE+4, BaseTaskTest,
   TEXT( "Modules"                 ), 1,    0,        BASE+5, BaseModuleTest,
   NULL,                              0,   0,         0,      NULL  // marks end of list
};

#endif   





