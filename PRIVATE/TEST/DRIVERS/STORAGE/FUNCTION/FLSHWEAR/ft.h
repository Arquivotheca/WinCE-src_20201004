//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
    FTE(1,      " 0%% filled linearly",  2000,  0, Tst_LinearFillDiskReadWrite)
    FTE(1,      " 5%% filled linearly",  2005,  5, Tst_LinearFillDiskReadWrite)
    FTE(1,      "10%% filled linearly",  2010, 10, Tst_LinearFillDiskReadWrite)
    FTE(1,      "15%% filled linearly",  2015, 15, Tst_LinearFillDiskReadWrite)
    FTE(1,      "20%% filled linearly",  2020, 20, Tst_LinearFillDiskReadWrite)
    FTE(1,      "25%% filled linearly",  2025, 25, Tst_LinearFillDiskReadWrite)
    FTE(1,      "30%% filled linearly",  2030, 30, Tst_LinearFillDiskReadWrite)
    FTE(1,      "35%% filled linearly",  2035, 35, Tst_LinearFillDiskReadWrite)
    FTE(1,      "40%% filled linearly",  2040, 40, Tst_LinearFillDiskReadWrite)
    FTE(1,      "45%% filled linearly",  2045, 45, Tst_LinearFillDiskReadWrite)
    FTE(1,      "50%% filled linearly",  2050, 50, Tst_LinearFillDiskReadWrite)
    FTE(1,      "55%% filled linearly",  2055, 55, Tst_LinearFillDiskReadWrite)
    FTE(1,      "60%% filled linearly",  2060, 60, Tst_LinearFillDiskReadWrite)
    FTE(1,      "65%% filled linearly",  2065, 65, Tst_LinearFillDiskReadWrite)    
    FTE(1,      "70%% filled linearly",  2070, 70, Tst_LinearFillDiskReadWrite)    
    FTE(1,      "75%% filled linearly",  2075, 75, Tst_LinearFillDiskReadWrite)
    FTE(1,      "80%% filled linearly",  2080, 80, Tst_LinearFillDiskReadWrite)
    FTE(1,      "85%% filled linearly",  2085, 85, Tst_LinearFillDiskReadWrite)
    FTE(1,      "90%% filled linearly",  2090, 90, Tst_LinearFillDiskReadWrite)
    FTE(1,      "95%% filled linearly",  2095, 95, Tst_LinearFillDiskReadWrite)

    FTH(0, "fill FLASH randomly and test read/write throughput")
    FTE(1,      " 0%% filled randomly",  3000,  0, Tst_RandFillDiskReadWrite)
    FTE(1,      " 5%% filled randomly",  3005,  5, Tst_RandFillDiskReadWrite)
    FTE(1,      "10%% filled randomly",  3010, 10, Tst_RandFillDiskReadWrite)
    FTE(1,      "15%% filled randomly",  3015, 15, Tst_RandFillDiskReadWrite)
    FTE(1,      "20%% filled randomly",  3020, 20, Tst_RandFillDiskReadWrite)
    FTE(1,      "25%% filled randomly",  3025, 25, Tst_RandFillDiskReadWrite)
    FTE(1,      "30%% filled randomly",  3030, 30, Tst_RandFillDiskReadWrite)
    FTE(1,      "35%% filled randomly",  3035, 35, Tst_RandFillDiskReadWrite)
    FTE(1,      "40%% filled randomly",  3040, 40, Tst_RandFillDiskReadWrite)
    FTE(1,      "45%% filled randomly",  3045, 45, Tst_RandFillDiskReadWrite)
    FTE(1,      "50%% filled randomly",  3050, 50, Tst_RandFillDiskReadWrite)
    FTE(1,      "55%% filled randomly",  3055, 55, Tst_RandFillDiskReadWrite)
    FTE(1,      "60%% filled randomly",  3060, 60, Tst_RandFillDiskReadWrite)
    FTE(1,      "65%% filled randomly",  3065, 65, Tst_RandFillDiskReadWrite)
    FTE(1,      "70%% filled randomly",  3070, 70, Tst_RandFillDiskReadWrite)
    FTE(1,      "75%% filled randomly",  3075, 75, Tst_RandFillDiskReadWrite)
    FTE(1,      "80%% filled randomly",  3080, 80, Tst_RandFillDiskReadWrite)
    FTE(1,      "85%% filled randomly",  3085, 85, Tst_RandFillDiskReadWrite)
    FTE(1,      "90%% filled randomly",  3090, 90, Tst_RandFillDiskReadWrite)
    FTE(1,      "95%% filled randomly",  3095, 95, Tst_RandFillDiskReadWrite)

    FTH(0, "fill FLASH fragmented and test read/write throughput")
    FTE(1,      " 0%% filled w/maximum fragmentation",  4000,  0, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      " 5%% filled w/maximum fragmentation",  4005,  5, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "10%% filled w/maximum fragmentation",  4010, 10, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "15%% filled w/maximum fragmentation",  4015, 15, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "20%% filled w/maximum fragmentation",  4020, 20, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "25%% filled w/maximum fragmentation",  4025, 25, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "30%% filled w/maximum fragmentation",  4030, 30, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "35%% filled w/maximum fragmentation",  4035, 35, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "40%% filled w/maximum fragmentation",  4040, 40, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "45%% filled w/maximum fragmentation",  4045, 45, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "50%% filled w/maximum fragmentation",  4050, 50, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "55%% filled w/maximum fragmentation",  4055, 55, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "60%% filled w/maximum fragmentation",  4060, 60, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "65%% filled w/maximum fragmentation",  4065, 65, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "70%% filled w/maximum fragmentation",  4070, 70, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "75%% filled w/maximum fragmentation",  4075, 75, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "80%% filled w/maximum fragmentation",  4080, 80, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "85%% filled w/maximum fragmentation",  4085, 85, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "90%% filled w/maximum fragmentation",  4090, 90, Tst_FragmentedFillDiskReadWrite)
    FTE(1,      "95%% filled w/maximum fragmentation",  4095, 95, Tst_FragmentedFillDiskReadWrite)

    FTH(0, "fill FLASH completely, free linearly and test read/write throughput")
    FTE(1,      " 0%% freed linearly",  5000,  0, Tst_LinearFreeDiskReadWrite)
    FTE(1,      " 5%% freed linearly",  5005,  5, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "10%% freed linearly",  5010, 10, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "15%% freed linearly",  5015, 15, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "20%% freed linearly",  5020, 20, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "25%% freed linearly",  5025, 25, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "30%% freed linearly",  5030, 30, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "35%% freed linearly",  5035, 35, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "40%% freed linearly",  5040, 40, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "45%% freed linearly",  5045, 45, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "50%% freed linearly",  5050, 50, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "55%% freed linearly",  5055, 55, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "60%% freed linearly",  5060, 60, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "65%% freed linearly",  5065, 65, Tst_LinearFreeDiskReadWrite)    
    FTE(1,      "70%% freed linearly",  5070, 70, Tst_LinearFreeDiskReadWrite)    
    FTE(1,      "75%% freed linearly",  5075, 75, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "80%% freed linearly",  5080, 80, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "85%% freed linearly",  5085, 85, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "90%% freed linearly",  5090, 90, Tst_LinearFreeDiskReadWrite)
    FTE(1,      "95%% freed linearly",  5095, 95, Tst_LinearFreeDiskReadWrite)

    FTH(0, "fill FLASH completely, free randomly and test read/write throughput")
    FTE(1,      " 0%% freed randomly",  6000,  0, Tst_RandFreeDiskReadWrite)
    FTE(1,      " 5%% freed randomly",  6005,  5, Tst_RandFreeDiskReadWrite)
    FTE(1,      "10%% freed randomly",  6010, 10, Tst_RandFreeDiskReadWrite)
    FTE(1,      "15%% freed randomly",  6015, 15, Tst_RandFreeDiskReadWrite)
    FTE(1,      "20%% freed randomly",  6020, 20, Tst_RandFreeDiskReadWrite)
    FTE(1,      "25%% freed randomly",  6025, 25, Tst_RandFreeDiskReadWrite)
    FTE(1,      "30%% freed randomly",  6030, 30, Tst_RandFreeDiskReadWrite)
    FTE(1,      "35%% freed randomly",  6035, 35, Tst_RandFreeDiskReadWrite)
    FTE(1,      "40%% freed randomly",  6040, 40, Tst_RandFreeDiskReadWrite)
    FTE(1,      "45%% freed randomly",  6045, 45, Tst_RandFreeDiskReadWrite)
    FTE(1,      "50%% freed randomly",  6050, 50, Tst_RandFreeDiskReadWrite)
    FTE(1,      "55%% freed randomly",  6055, 55, Tst_RandFreeDiskReadWrite)
    FTE(1,      "60%% freed randomly",  6060, 60, Tst_RandFreeDiskReadWrite)
    FTE(1,      "65%% freed randomly",  6065, 65, Tst_RandFreeDiskReadWrite)
    FTE(1,      "70%% freed randomly",  6070, 70, Tst_RandFreeDiskReadWrite)
    FTE(1,      "75%% freed randomly",  6075, 75, Tst_RandFreeDiskReadWrite)
    FTE(1,      "80%% freed randomly",  6080, 80, Tst_RandFreeDiskReadWrite)
    FTE(1,      "85%% freed randomly",  6085, 85, Tst_RandFreeDiskReadWrite)
    FTE(1,      "90%% freed randomly",  6090, 90, Tst_RandFreeDiskReadWrite)
    FTE(1,      "95%% freed randomly",  6095, 95, Tst_RandFreeDiskReadWrite)

    FTH(0, "fill FLASH completely, free fragmented and test read/write throughput")
    FTE(1,      " 0%% freed w/maximum fragmentation",  7000,  0, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      " 5%% freed w/maximum fragmentation",  7005,  5, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "10%% freed w/maximum fragmentation",  7010, 10, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "15%% freed w/maximum fragmentation",  7015, 15, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "20%% freed w/maximum fragmentation",  7020, 20, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "25%% freed w/maximum fragmentation",  7025, 25, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "30%% freed w/maximum fragmentation",  7030, 30, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "35%% freed w/maximum fragmentation",  7035, 35, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "40%% freed w/maximum fragmentation",  7040, 40, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "45%% freed w/maximum fragmentation",  7045, 45, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "50%% freed w/maximum fragmentation",  7050, 50, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "55%% freed w/maximum fragmentation",  7055, 55, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "60%% freed w/maximum fragmentation",  7060, 60, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "65%% freed w/maximum fragmentation",  7065, 65, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "70%% freed w/maximum fragmentation",  7070, 70, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "75%% freed w/maximum fragmentation",  7075, 75, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "80%% freed w/maximum fragmentation",  7080, 80, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "85%% freed w/maximum fragmentation",  7085, 85, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "90%% freed w/maximum fragmentation",  7090, 90, Tst_FragmentedFreeDiskReadWrite)
    FTE(1,      "95%% freed w/maximum fragmentation",  7095, 95, Tst_FragmentedFreeDiskReadWrite)
    
END_FTE
////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__
