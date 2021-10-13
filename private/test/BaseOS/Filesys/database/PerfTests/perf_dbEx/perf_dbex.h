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
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef __PERF_DB_H__

#include "tuxmain.h"

#define COMMON_TUX_HEADER {	\
	if( TPM_EXECUTE != uMsg )		\
		return TPR_NOT_HANDLED;	\
}

#ifdef SUPER_PERF
#define PERF_APP_NAME		_T("perf_dbEx_long")
#else
#define PERF_APP_NAME		_T("perf_dbEx")
#endif

#define PERF_DEF_CPU_POOL_INTERVAL_MS	100
#define PERF_DEF_CPU_CALIBRATION_MS		100
#define PERF_CPU_MEM_LOG_ITERATION		10

//  PERF COUNTERS/Markers DEFINITION
#define MARK_TEST_START	0
//  Volume operations 
#define MARK_VOL_MOUNT	1
#define MARK_VOL_UNMOUNT	2
#define MARK_VOL_ENUM		3
#define MARK_VOL_FLUSH	4

//  Database operations
#define MARK_DBCREATE		5
#define MARK_DBOPEN		6
#define MARK_DBCLOSE		7
#define MARK_DBDELETE		8
#define MARK_DBENUM		9

//  Record operations
#define MARK_RECWRITE		10
#define MARK_RECREAD		11
#define MARK_RECDELETE	12
#define MARK_REC_SEEK		13

//  DBINFO operations
#define MARK_DBINFO_GET	14
#define MARK_DBINFO_SET	15
#define MARK_DBFLUSH		16
#define MARK_RECORDCACHEOID	17
#define MARK_RECORDMODIFY		18
#define MARK_RECORDDEL			19
#define MARK_RECORDADD		20


#define DB_FLUSH_RECORDS 				2000
#define DB_FLUSH_MOD_PERCENTAGE 		10
#define DB_FLUSH_RECORDSIZE_BIG 		1024
#define DB_FLUSH_RECORDSIZE_SMALL 	512
#define DB_FLUSH_ITERATIONS			10

#define PERF_MAX_STRING	100
#define PERF_START_HIGH_DATE		0x01c125e6  //  This translats roughly to Aug 15th 2001. 
#define PERF_START_LOW_DATE		0x5319f380

#define MANY_RECORDS		250
#define FEW_RECORDS		50
#define VOL_NAME			_T("Perf_DBEx_Vol")
#define DB_NAME			_T("Perf_DBEx")

#define DEFAULT_ROOT_PATH	_T("\\")


//  THESE PROPERTIES WILL BE USED AS SORT KEYS ONLY
#define GLOBAL_SORT_KEY_SHORT	0xBAAD<<16 | CEVT_I2
#define GLOBAL_SORT_KEY_USHORT	0xBAAD<<16 | CEVT_UI2
#define GLOBAL_SORT_KEY_LONG		0xBAAD<<16 | CEVT_I4
#define GLOBAL_SORT_KEY_ULONG	0xBAAD<<16 | CEVT_UI4
#define GLOBAL_SORT_KEY_FILETIME	0xBAAD<<16 | CEVT_FILETIME
#define GLOBAL_SORT_KEY_LPWSTR	0xBAAD<<16 | CEVT_LPWSTR
#define GLOBAL_SORT_KEY_BOOL		0xBAAD<<16 | CEVT_BOOL
#define GLOBAL_SORT_KEY_DOUBLE	0xBAAD<<16 | CEVT_R8

#define PERF_STORE_PERCENT	70
#define PERF_RAM_PERCENT		30

#define PERF_BIT_SORT_DESCENDING		1 
#define PERF_BIT_DB_UNCOMPRESSED		2
#define PERF_BIT_NO_INDEX				4
#define PERF_BIT_NO_MEASURE			8

#define PERF_SEEK_NEXT		1
#define PERF_SEEK_PREV		2
#define PERF_SEEK_OID		3 
#define PERF_SEEK_VALUE	4

extern WCHAR g_szDbVolume[];
extern WCHAR g_szDb[];
extern WCHAR g_szRootPath[];
extern BOOL g_Flag_Sleep;
extern DWORD g_dwPoolIntervalMs;
extern DWORD g_dwCPUCalibrationMs;

static CEPROPID rgPropId_Table[] = {	CEVT_I2,
									CEVT_UI2,
									CEVT_I4,
									CEVT_UI4,
									CEVT_FILETIME,
									CEVT_LPWSTR,
									CEVT_BLOB,
									CEVT_BOOL,
									CEVT_R8 };

#define SORT_PROPS_AVAILABLE   (sizeof(rgPropId_Table)/sizeof(CEPROPID) )


//Testproc declarations
//
TESTPROCAPI Tst_SortOnNothing (UINT, TPPARAM, const LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_SortOnDWORD (UINT, TPPARAM, const LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_SortOnString (UINT, TPPARAM, const LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_SortOnFiletime (UINT, TPPARAM, const LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_DBFlush (UINT, TPPARAM, const LPFUNCTION_TABLE_ENTRY);


// --------------------------------------------------------------------
// Tux testproc function table
//
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {

	_T("Performance Test Template"	),  0,  0,  0,  NULL,
	_T( "No sort"					),  1,  10, 1001,  Tst_SortOnNothing,
	_T( "DWORD sort"				),  1,  10, 1002,  Tst_SortOnDWORD,
	_T( "String sort"				),  1,  10, 1003,  Tst_SortOnString,
	_T( "FileTime sort"				),  1,  10, 1004,  Tst_SortOnFiletime,
	_T( "Flush"					),  1,  10, 1005,  Tst_DBFlush,
	NULL,   0,  0,  0,  NULL
};

#endif // __PERF_DB_H__
