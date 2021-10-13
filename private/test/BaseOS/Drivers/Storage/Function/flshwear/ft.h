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
//  TUXTEST TUX DLL
//
//  Module: ft.h
//          Declares the TUX function table and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES the function
//          table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

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
//  To create the function table and function prototypes, two macros are
//  available:
//
//      FTH(level, description)
//          (Function Table Header) Used for entries that don't have functions,
//          entered only as headers (or comments) into the function table.
//
//      FTE(level, description, code, param, function)
//          (Function Table Entry) Used for all functions. DON'T use this macro
//          if the "function" field is NULL. In that case, use the FTH macro.
//
//  You must not use the TEXT or _T macros here. This is done by the FTH and FTE
//  macros.
//
//  In addition, the table must be enclosed by the BEGIN_FTE and END_FTE macros.

BEGIN_FTE
    
    FTH(0, "basic flash read and write tests")
    FTE(1,      "repeat at sector zero",        1001, 0, Tst_RepeatWriteSame)
    FTE(1,      "repeat sector sequential",     1002, 0, Tst_RepeatWriteSequential)    
    FTE(1,      "repeat sector random",         1003, 0, Tst_RepeatWriteRandom)
    FTE(1,      "repeat write on filled disk",  1004, 0, Tst_RepeatWriteFull)

    FTH(0, "fill FLASH linearly, and test read/write throughput")
    FTE(1,      "filled linearly",  2000,  0, Tst_LinearFillDiskReadWrite)

    FTH(0, "fill FLASH randomly and test read/write throughput")
    FTE(1,      "filled randomly",  3000,  0, Tst_RandFillDiskReadWrite)

    FTH(0, "fill FLASH fragmented and test read/write throughput")
    FTE(1,      "filled w/maximum fragmentation",  4000,  0, Tst_FragmentedFillDiskReadWrite)

    FTH(0, "fill FLASH completely, free linearly and test read/write throughput")
    FTE(1,      "freed linearly",  5000,  0, Tst_LinearFreeDiskReadWrite)

    FTH(0, "fill FLASH completely, free randomly and test read/write throughput")
    FTE(1,      "freed randomly",  6000,  0, Tst_RandFreeDiskReadWrite)

    FTH(0, "fill FLASH completely, free fragmented and test read/write throughput")
    FTE(1,      "freed w/maximum fragmentation",  7000,  0, Tst_FragmentedFreeDiskReadWrite)
    
END_FTE
////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__
