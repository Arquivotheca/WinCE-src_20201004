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
#define HOST_BVT_TESTBASE 100
#define BVT_FUNC    HOST_BVT_TESTBASE
#define BVT_PWR     HOST_BVT_TESTBASE+100
#define BVT_STRESS   HOST_BVT_TESTBASE+200

    

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
    FTH(0, "USB Host BVT Tests")
    FTE(1,      "Get USBD Version Test", BVT_FUNC, 0, TestProc)
    FTE(1,      "Register Client Test", BVT_FUNC+1, 0, TestProc)
    FTE(1,      "Registry Test for Registered Client", BVT_FUNC+2, 0, TestProc)
    FTE(1,      "Open Registry Test for Registered Client", BVT_FUNC+3, 0, TestProc)
    FTE(1,      "Get Client Registry Path for Registered Client Test", BVT_FUNC+4, 0, TestProc)
    FTE(1,      "Unregister Client Test", BVT_FUNC+5, 0, TestProc)
    FTE(1,      "Registry Test for Unregistered Client ", BVT_FUNC+6, 0, TestProc)
    FTE(1,      "Open Registry Test for UnRegistered Client", BVT_FUNC+7, 0, TestProc)
    FTE(1,      "Get Client Registry Path for UnRegistered Client Test", BVT_FUNC+8, 0, TestProc)

    //BVT post Suspend resume cycle.
    FTH(0, "USB Host BVT Suspend-Resume Tests")
    FTE(1,      "Suspend/Resume Get USBD Version Test", BVT_PWR, 0, TestProc)
    FTE(1,      "Suspend/Resume Registered Client Test", BVT_PWR+1, 0, TestProc)
    FTE(1,      "Suspend/Resume UnRegistered Client Test", BVT_PWR+2, 0, TestProc)
    

    //Stress BVT
    /*FTH(0, "USB Host BVT Stress Tests")
    FTE(1,      "Stress Register Client Test", BVT_STRESS+1, 0, TestProc)*/
END_FTE

////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__
