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

////////////////////////////////////////////////////////////////////////////////
// TUX Function table

BEGIN_FTE
    FTH(0, "file system performance test cases")
    FTE(1,      "write a new file",                                 1001, 0, Tst_WriteFile)
    FTE(1,      "read existing file",                               1002, 0, Tst_ReadFile)
    FTE(1,      "write a new file at random offsets",               1003, 0, Tst_WriteFileRandom)
    FTE(1,      "read existing file from random offsets",           1004, 0, Tst_ReadFileRandom)
    FTE(1,      "reverse read a file",                              1005, 0, Tst_ReverseReadFile)
    FTH(0, "run all throughput tests, varying the read/write buffer size")
    FTE(1,      "run all tests with 128 B buffers",                 2001, 0x00000080, Tst_RunSuite)
    FTE(1,      "run all tests with 256 B buffers",                 2002, 0x00000100, Tst_RunSuite)
    FTE(1,      "run all tests with 512 B buffers",                 2003, 0x00000200, Tst_RunSuite)
    FTE(1,      "run all tests with 1 KB buffers",                  2004, 0x00000400, Tst_RunSuite)
    FTE(1,      "run all tests with 2 KB buffers",                  2005, 0x00000800, Tst_RunSuite)
    FTE(1,      "run all tests with 4 KB buffers",                  2006, 0x00001000, Tst_RunSuite)
    FTE(1,      "run all tests with 8 KB buffers",                  2007, 0x00002000, Tst_RunSuite)
    FTE(1,      "run all tests with 16 KB buffers",                 2008, 0x00004000, Tst_RunSuite)
    FTE(1,      "run all tests with 32 KB buffers",                 2009, 0x00008000, Tst_RunSuite)
    FTE(1,      "run all tests with 64 KB buffers",                 2010, 0x00010000, Tst_RunSuite)
    FTE(1,      "run all tests with 128 KB buffers",                2011, 0x00020000, Tst_RunSuite)
    FTE(1,      "run all tests with 256 KB buffers",                2012, 0x00040000, Tst_RunSuite)
    FTE(1,      "run all tests with 512 KB buffers",                2013, 0x00080000, Tst_RunSuite)
    FTE(1,      "run all tests with 1 MB buffers",                  2014, 0x00100000, Tst_RunSuite)
    FTH(0, "misc performance test cases")
    FTE(1,      "create a vertical directory tree 10 levels deep",  3001, 10, Tst_CreateDeepTree)
    FTE(1,      "create a vertical directory tree 15 levels deep",  3002, 15, Tst_CreateDeepTree)
    FTE(1,      "create a vertical directory tree 20 levels deep",  3003, 20, Tst_CreateDeepTree)
    FTE(1,      "create a large (~500mb) file using SetEndOfFile()",3004, 500000000, Tst_SetEndOfFile)
    FTE(1,      "perform random seeks in a file and read data",     3005, 1, Tst_SeekSpeed)
    FTE(1,      "perform random seeks in a file and write data",    3006, 0, Tst_SeekSpeed)
    FTH(0, "scenario-based perf testing")
    FTE(1,      "Read and write to a large fragmented file",        4001, 100000000, Tst_ReadAppendFragmentedFile)
    FTE(1,      "Copy a set of files from storage to device root",  4002, 1, Tst_CopyFiles)
    FTE(1,      "Copy a set of files from device root to storage",  4003, 0, Tst_CopyFiles)
END_FTE

////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__
