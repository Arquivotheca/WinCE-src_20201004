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
	FTH(0, "RegCreateKey API")
	FTE(1, "verify key level limit", 1001, 0, Tst_LevelLimit)
	FTE(1, "create keys with varying sub-keys", 1002, 0, Tst_Subkey_Variations)
	FTE(1, "key creation with invalid parameters", 1003, 0, Tst_Invalid_Params)
	FTE(1, "key creation stress", 1004, 0, Tst_Stress)

	FTH(0, "RegDeleteKey API")
	FTE(1, "delete tree", 2001, 0, Tst_DeleteTree)
	FTE(1, "delete keys with varying sub-keys", 2002, 0, Tst_SubKeyVariations)
	FTE(1, "delete keys with invalid parameters", 2003, 0, Tst_InvalidParams)
	FTE(1, "miscellaneous delete key functionality", 2004, 0, Tst_MiscFunctionals)
	FTE(1, "delete child key that is open but invalid", 2005, 0, Tst_DelInvalidSubKey)

	FTH(0, "RegEnumKeyEx API Tests")
	FTE(1, "enumerate keys bvt", 3001, 0, Tst_EnumKeyBVT)
	FTE(1, "enumerate keys varying HKEY parameter", 3002, 0, Tst_EnumKey_hKey)
	FTE(1, "enumerate keys varying index parameter", 3003, 0, Tst_EnumKey_index)
	FTE(1, "enumerate keys varying key name parameter", 3004, 0, Tst_EnumKey_Name)
	FTE(1, "miscellaneous  key enumeration functionality", 3005, 0, Tst_EnumKey_Misc)

	FTH(0, "RegEnumVal API Tests")
	FTE(1, "enumerate values varying HKEY parameter", 4001, 0, Tst_EnumVal_hKey)
	FTE(1, "enumerate values varying index parameter ", 4002, 0, Test_EnumVal_index)
	FTE(1, "enumerate values varying value name parameter", 4003, 0, Tst_EnumVal_ValName)
	FTE(1, "enumerate all value types", 4004, 0, Tst_EnumVal_Type)
	FTE(1, "enumerate and verify returned byte count", 4005, 0, Tst_EnumVal_PbCb)

	FTH(0, "RegOpenKeyEx API Tests")
	FTE(1, "open keys varying HKEY parameter", 5001, 0, Tst_RegOpenKey_hKey)
	FTE(1, "open keys varying sub-key name parameter", 5002, 0, Tst_RegOpenKey_pszSubKey)

	FTH(0, "RegQueryInfoKey API Tests")
	FTE(1, "verify class name info", 6001, 0, Tst_RegQueryInfo_Class)
	FTE(1, "verify sub-key count info", 6002, 0, Tst_RegQueryInfo_cSubKey)
	FTE(1, "verify max sub-key length info", 6003, 0, Tst_RegQueryInfo_cbMaxSubKey)
	FTE(1, "verify value count", 6004, 0, Tst_RegQueryInfo_cValues)
	FTE(1, "verify functionality of CeRegGetInfo", 6005, 0, Tst_CeRegGetInfo_Funct)
	FTE(1, "verify error handling of CeRegGetInfo", 6006, 0, Tst_CeRegGetInfo_Error)

	
	FTH(0, "RegSetValueEx API Tests")
	FTE(1, "basic set functionality", 7001, 0, Tst_RegSetValue_Functionals)
	FTE(1, "set value name variations", 7002, 0, Tst_RegSetVal_NameVariations)
	FTE(1, "verify set value limits", 7003, 0, Tst_RegSetVal_Limits)
	FTE(1, "set value data variations", 7004, 0, Tst_RegSetVal_DataVariations)
	FTE(1, "set values with invalid parameters", 7005, 0, Tst_RegSetVal_BadParams)

	FTH(0, "RegDeleteValue API Tests")
	FTE(1, "basic delete functionality", 8001, 0, Tst_RegDeleteVal_Functional)
	FTE(1, "delete value name variations", 8002, 0, Tst_RegDeleteVal_NameVariations)
	FTE(1, "delete value stress", 8003, 0, Tst_RegDeleteVal_Stress)
	FTE(1, "delete values with invalid parameters", 8004, 0, Tst_RegDeleteVal_BadParams)
		
	FTH(0, "RegQueryValueEx API Tests")
	FTE(1, "query values varying HKEY parameter", 9001, 0, Tst_RegQueryValue_hKey)
	FTE(1, "query values varying value name parameter", 9002, 0, Tst_RegQueryValue_ValName)
	FTE(1, "query all value types", 9003, 0, Tst_RegQueryValue_Type)
	FTE(1, "query and verify returned byte count", 9004, 0, Tst_RegQueryValue_pbcb)
END_FTE

////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__
