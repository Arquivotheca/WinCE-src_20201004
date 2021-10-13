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
#ifndef __TREGAPI_H__
#define __TREGAPI_H__


extern HKEY g_l_RegRoots[];    //  defined in create.cpp
#define TST_NUM_ROOTS		4
#define TST_MAX_LEVEL		16
#define HKEY_INVALID_TST	((HKEY)(ULONG_PTR)0x80000345)

//
// MACROS  
//
#define TEST_FAIL(X, dwErr)    {   \
		TRACE(TEXT("tRegApi TESTFAIL : %s Error:0x%x "), _T(#X), dwErr);\
		Hlp_DumpError(dwErr); \
		TRACE(TEXT(" at %s line %u \r\n"), _T(__FILE__), __LINE__); \
		g_dwFailCount++; \
		goto ErrorReturn; \
}

//
// GLOBALS AND CONSTANTS
//
extern DWORD g_dwFlags;
extern DWORD g_dwFailCount;
extern LPTSTR g_rgszKeyNames[];
extern LPTSTR g_rgszValueNames[];

#define TST_PREBOOT		0x01
#define TST_POSTBOOT		0x02 

#define TST_NUM_KEYS		0x02            //  Warning : This  2 should match g_rgszKeyNames
#define TST_NUM_VALUES	0x05

#define TST_LARGE_NUM_VALUES	4096
#define TST_LARGE_NUM_KEYS	2000
#define TST_VAL_BUF_SIZE		1024

extern DWORD g_rgdwRegTypes[];
#define TST_NUM_VALUE_TYPES 11



#define TST_SAVE_FILE	_T("RegSaveFile") 
#define TREGAPI_CLASS	_T("tregapi_class") 

//
// COMMON FUNCTIONS   
//
BOOL L_CreatePredefinedKeys(void);
BOOL L_CreatePredefinedValues(HKEY hKey);
BOOL L_DeletePredefinedKeys(void);
BOOL L_VerifyPredefinedKeys(void);
BOOL L_VerifyPredefinedValues(HKEY hKey);
BOOL L_WriteRandomVal(HKEY hKey);
#endif
