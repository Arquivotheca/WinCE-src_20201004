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
#if (!defined(__FT_H__) || defined(__GLOBALS_CPP__))
#ifndef __FT_H__
#define __FT_H__
#endif

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifdef __DEFINE_FTE__
#undef BEGIN_FTE
#undef FTE
#undef FTH
#undef END_FTE
#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, c, d, e) { TEXT(b), a, d, c, e },
#define END_FTE { NULL, 0, 0, 0, NULL } };
#else // __DEFINE_FTE__
#ifdef __GLOBALS_CPP__
#define BEGIN_FTE
#else // __GLOBALS_CPP__
#define BEGIN_FTE extern FUNCTION_TABLE_ENTRY g_lpFTE[];
#endif // __GLOBALS_CPP__
#define FTH(a, b)
#define FTE(a, b, c, d, e) TESTPROCAPI e(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
#define END_FTE
#endif // __DEFINE_FTE__

#define THROUGHPUT_BASE         2001
#define CREATEDEEPTREE_BASE     3001
#define SETENDOFFILE_BASE       3004
#define SEEKSPEED_BASE          3005
#define READAPPENDFRAG_BASE     4001
#define COPYFILES_BASE          4002
#define FINDFIRSTNEXT_BASE      5001
#define FINDFIRSTNEXTRAND_BASE  6001
#define CREATEFILENEW_BASE      7001
#define CREATEFILEEXISTING_BASE 7002

////////////////////////////////////////////////////////////////////////////////
// TUX Function table

BEGIN_FTE
    FTH(0, "run all throughput tests, varying the read/write buffer size")
    FTE(1,      "run all tests with 128 B buffers",                 THROUGHPUT_BASE+0,  0x00000080, Tst_RunSuite)
    FTE(1,      "run all tests with 256 B buffers",                 THROUGHPUT_BASE+1,  0x00000100, Tst_RunSuite)
    FTE(1,      "run all tests with 512 B buffers",                 THROUGHPUT_BASE+2,  0x00000200, Tst_RunSuite)
    FTE(1,      "run all tests with 1 KB buffers",                  THROUGHPUT_BASE+3,  0x00000400, Tst_RunSuite)
    FTE(1,      "run all tests with 2 KB buffers",                  THROUGHPUT_BASE+4,  0x00000800, Tst_RunSuite)
    FTE(1,      "run all tests with 4 KB buffers",                  THROUGHPUT_BASE+5,  0x00001000, Tst_RunSuite)
    FTE(1,      "run all tests with 8 KB buffers",                  THROUGHPUT_BASE+6,  0x00002000, Tst_RunSuite)
    FTE(1,      "run all tests with 16 KB buffers",                 THROUGHPUT_BASE+7,  0x00004000, Tst_RunSuite)
    FTE(1,      "run all tests with 32 KB buffers",                 THROUGHPUT_BASE+8,  0x00008000, Tst_RunSuite)
    FTE(1,      "run all tests with 64 KB buffers",                 THROUGHPUT_BASE+9,  0x00010000, Tst_RunSuite)
    FTE(1,      "run all tests with 128 KB buffers",                THROUGHPUT_BASE+10, 0x00020000, Tst_RunSuite)
    FTE(1,      "run all tests with 256 KB buffers",                THROUGHPUT_BASE+11, 0x00040000, Tst_RunSuite)
    FTE(1,      "run all tests with 512 KB buffers",                THROUGHPUT_BASE+12, 0x00080000, Tst_RunSuite)
    FTE(1,      "run all tests with 1 MB buffers",                  THROUGHPUT_BASE+13, 0x00100000, Tst_RunSuite)
    FTH(0, "misc performance test cases")
    FTE(1,      "create a vertical directory tree 10 levels deep",  CREATEDEEPTREE_BASE+0, 10, Tst_CreateDeepTree)
    FTE(1,      "create a vertical directory tree 15 levels deep",  CREATEDEEPTREE_BASE+1, 15, Tst_CreateDeepTree)
    FTE(1,      "create a vertical directory tree 20 levels deep",  CREATEDEEPTREE_BASE+2, 20, Tst_CreateDeepTree)
    FTE(1,      "create a large (~500mb) file using SetEndOfFile()",SETENDOFFILE_BASE, 500000000, Tst_SetEndOfFile)
    FTE(1,      "perform random seeks in a file and read data",     SEEKSPEED_BASE+0, 1, Tst_SeekSpeed)
    FTE(1,      "perform random seeks in a file and write data",    SEEKSPEED_BASE+1, 0, Tst_SeekSpeed)
    FTH(0, "scenario-based perf testing")
    FTE(1,      "Read and write to a large fragmented file",        READAPPENDFRAG_BASE, 100000000, Tst_ReadAppendFragmentedFile)
    FTE(1,      "Copy a set of files from storage to device root",  COPYFILES_BASE+0, 1, Tst_CopyFiles)
    FTE(1,      "Copy a set of files from device root to storage",  COPYFILES_BASE+1, 0, Tst_CopyFiles)
    FTH(0, "FindFileFirst and FindFileNext for same type of files")
    FTE(1,      "Perform FindFileFirst and FindFileNext 50 files",  FINDFIRSTNEXT_BASE+0,  50, Tst_FindFirstNext)
    FTE(1,      "Perform FindFileFirst and FindFileNext 100 files", FINDFIRSTNEXT_BASE+1, 100, Tst_FindFirstNext)
    FTE(1,      "Perform FindFileFirst and FindFileNext 200 files", FINDFIRSTNEXT_BASE+2, 200, Tst_FindFirstNext)
    FTH(0, "FindFileFirst and FindFileNext for randomly same type of file ")
    FTE(1,      "Perform FindFileFirst and FindFileNext 50 files",  FINDFIRSTNEXTRAND_BASE+0,  50, Tst_FindFirstNextRandom)
    FTE(1,      "Perform FindFileFirst and FindFileNext 100 files", FINDFIRSTNEXTRAND_BASE+1, 100, Tst_FindFirstNextRandom)
    FTE(1,      "Perform FindFileFirst and FindFileNext 200 files", FINDFIRSTNEXTRAND_BASE+2, 200, Tst_FindFirstNextRandom)
    FTH(0, "CreateFile across directories and across different nested directories")
    FTE(1,      "CreateFile(CREATE_NEW) with 10 files ",            CREATEFILENEW_BASE,      10, Tst_CreateFileNew)
    FTE(1,      "CreateFile(OPEN_EXISTING) with 10 files",          CREATEFILEEXISTING_BASE, 10, Tst_CreateFileExisting)
END_FTE

////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__
