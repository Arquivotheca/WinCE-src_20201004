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
#ifndef __PERF_REG_H__



#include "tuxmain.h"
#include "reghlpr.h"

#define PERF_APP_NAME                   _T("perf_regEx")
#define PERF_DEF_CPU_POOL_INTERVAL_MS   100
#define PERF_DEF_CPU_CALIBRATION_MS     100
#define PERF_CPU_MEM_LOG_ITERATION      10

extern DWORD g_dwPoolIntervalMs;
extern DWORD g_dwCPUCalibrationMs;

extern DWORD g_dwRegCache_MAXKEYS;
extern BOOL g_bWatchKey;
//+ ------------------------------------------
//  PERF COUNTERS/Markers DEFINITION
//- ------------------------------------------
#define MARK_TEST               0
//  REGKEY operations
#define MARK_KEY_CLOSE          1
#define MARK_KEY_CREATE         2
#define MARK_KEY_DELETE         3
#define MARK_KEY_FLUSH          4
#define MARK_KEY_QUERYINFO      5
#define MARK_KEY_SAVE           6
#define MARK_KEY_REPLACE        7
#define MARK_KEY_ENUM           8
#define MARK_KEY_OPEN           9

//  Registry Value functions
#define MARK_VALUE_DELETE       10
#define MARK_VALUE_SET          11
#define MARK_VALUE_QUERY        12
#define MARK_VALUE_ENUM         13


#define MARK_VALUE_ENUM_5     15
#define MARK_VALUE_ENUM_10    16
#define MARK_VALUE_ENUM_50    17
#define MARK_VALUE_ENUM_100   18

#define MARK_KEY_ENUM_5         19
#define MARK_KEY_ENUM_10        20
#define MARK_KEY_ENUM_50        21
#define MARK_KEY_ENUM_100       22

//  Multithreaded registry tests
#define DEFAULT_MAX_REGKEYS     10
#define TEST_MAX_REGKEYS        20
#define DEFAULT_MAX_REGVALUES   10
#define TEST_MAX_REGVALUES      20
    //perf markers
#define MARK_QUERY_5_KEYS_1_THREAD       23
#define MARK_QUERY_5_KEYS_8_THREADS      24
#define MARK_QUERY_MAX_KEYS_1_THREAD     25
#define MARK_QUERY_MAX_KEYS_8_THREADS    26
#define MARK_QUERY_OVERMAX_KEYS_1_THREAD 27
#define MARK_QUERY_OVERMAX_KEYS_8_THREAD 28

#define MARK_OPEN_5_KEYS_1_THREAD       29
#define MARK_OPEN_5_KEYS_8_THREADS      30
#define MARK_OPEN_MAX_KEYS_1_THREAD     31
#define MARK_OPEN_MAX_KEYS_8_THREADS    32
#define MARK_OPEN_OVERMAX_KEYS_1_THREAD 33
#define MARK_OPEN_OVERMAX_KEYS_8_THREAD 34

#define MARK_QUERY_5_KEYS_HACKY_1_THREAD       35
#define MARK_QUERY_5_KEYS_HACKY_8_THREADS      36
#define MARK_QUERY_MAX_KEYS_HACKY_1_THREAD     37
#define MARK_QUERY_MAX_KEYS_HACKY_8_THREADS    38
#define MARK_QUERY_OVERMAX_KEYS_HACKY_1_THREAD 39
#define MARK_QUERY_OVERMAX_KEYS_HACKY_8_THREAD 40


//  Test constants.
#define PERF_KEY            _T("Perf_reg_Key")
#define PERF_KEY_14_LEVEL   _T("Level1\\Level2\\Level3\\Level4\\Level5\\Level6\\Level7\\Level8\\Level9\\Level10\\Level11\\Level12\\Level13\\Level14")
#define PERF_KEY_LONG_NAME  _T("This_is_a_very_long_key_name_that_is_the_root_Name_to_be_used_for_some_of_the_Perf_tests")
#define PERF_KEY_SHORT_NAME _T("Short_key")
#define PERF_VAL_LONG_NAME  _T("This_is_a_very_long_value_name_that_is_the_root_Name_to_be_used_for_some_of_the_Perf_tests")
#define PERF_VAL_SHORT_NAME _T("Short_Value")

//  Test flag bits that are using for controlling
//  code paths in the test.
#define PERF_BIT_LONG_NAME      0x1
#define PERF_BIT_SHORT_NAME     0x2
#define PERF_BIT_PATH_NAME      0x4
#define PERF_BIT_SIMILAR_NAMES  0x8
#define PERF_BIT_SMALL_VALUE    0x10
#define PERF_BIT_LARGE_VALUE    0x20
#define PERF_BIT_MANY_VALUES    0x40
#define PERF_BIT_FEW_VALUES     0x80
#define PERF_BIT_ONE_KEY        0x100
#define PERF_BIT_MULTIPLE_KEYS  0x200
#define PERF_BIT_OPENLIST       0x400

//  Values used in the test.
#define PERF_REG_MAX_DATA_SIZE  100
#define PERF_MANY_KEYS          50
#define PERF_FEW_KEYS           10
#define PERF_LONG_NAME_LEN      250     //  sizes of key names
#define PERF_SHORT_NAME_LEN     10
#define PERF_NUM_KEYS_IN_SET    30

#define PERF_MANY_VALUES        50
#define PERF_FEW_VALUES         10
#define PERF_LARGE_VALUE        512
#define PERF_SMALL_VALUE        20
#define PERF_OPEN_KEY_LIST_LEN  1000

#define PERF_TREE_ITER_COUNT    20


//  More flags.
#define PERF_OPEN_PATHS         1
#define PERF_OPEN_LEAVES        2

#define PERF_SAME_KEY           1
#define PERF_DIFFERENT_KEY      2

#define COMMON_TUX_HEADER {     \
    if( TPM_EXECUTE != uMsg )   \
        return TPR_NOT_HANDLED; \
}

//Testproc declarations
//
TESTPROCAPI Tst_CreateKey(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE ) ;
TESTPROCAPI Tst_SetValue(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE ) ;
TESTPROCAPI Tst_OpenKey(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE ) ;
TESTPROCAPI Tst_DeleteTree(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE ) ;
TESTPROCAPI Tst_FlushTree(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE ) ;
TESTPROCAPI Tst_RegEnumPerf(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE ) ;

//Multithreaded Tests
TESTPROCAPI Tst_MultiThreadRegQueryValueEx(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE ) ;
TESTPROCAPI Tst_MultiThreadRegQueryValueEx_hacky(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE ) ;
TESTPROCAPI Tst_MultiThreadRegOpenKeyEx(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE ) ;


// --------------------------------------------------------------------
// Tux testproc function table
//
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {

    _T("Registry Perf Tests"    ),      0,      0,          0,  NULL,
    _T( "CreateKey"             ),      1,     10,       1001,  Tst_CreateKey,
    _T( "Values"                ),      1,     10,       1002,  Tst_SetValue,
    _T( "OpenKey"               ),      1,     10,       1003,  Tst_OpenKey,
    _T( "DeleteTree"            ),      1,     10,       1004,  Tst_DeleteTree,
    _T( "FlushTree"             ),      1,     10,       1005,  Tst_FlushTree,
    _T( "RegEnumPerf"           ),      1,     10,       1006,  Tst_RegEnumPerf,

    NULL,   0,  0,  0,  NULL
};

#endif // __TESTPROC_H__


