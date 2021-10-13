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
//  Copyright (c) Microsoft Corporation
//
//  Module:
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

//Test Base
#define API_BASE 100  //Base Test Number for API Tests 

///////////////////////////////////////////////////////////////////////////////
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

    FTE(0, "Power Manager System Power State Tests Usage Message", 1, 0, UsageMessage)

    FTH(0, "Power Manager System Power State Tests")
    FTE(1, "Test list of supported system power states  ", API_BASE + 1, 0, Tst_SystemPowerStateList)
    FTE(1, "Test SetSystemPowerstate and GetSystemPowerState APIs - set power to On, UserIdle, ScreenOff ", API_BASE + 2, 0, Tst_SetSystemPowerStates)

    FTE(1, "Test Setting SystemPowerstate to Reboot using the power state name ", API_BASE + 3, 1, Tst_SetSystemPowerStateReboot)
    FTE(1, "Test Setting SystemPowerstate to Reboot using the power state name and POWER_FORCE flag ", API_BASE + 4, 2, Tst_SetSystemPowerStateReboot)
    FTE(1, "Test Setting SystemPowerstate to ColdReboot using the power state name ", API_BASE + 5, 1, Tst_SetSystemPowerStateColdReboot)
    FTE(1, "Test Setting SystemPowerstate to ColdReboot using the power state name and POWER_FORCE flag ", API_BASE + 6, 2, Tst_SetSystemPowerStateColdReboot)
    FTE(1, "Test Setting SystemPowerstate to Off using the power state name ", API_BASE + 7, 1, Tst_SetSystemPowerStateOff)
    FTE(1, "Test Setting SystemPowerstate to Off using the power state name and POWER_FORCE flag ", API_BASE + 8, 2, Tst_SetSystemPowerStateOff)
    
    FTE(1, "Test Setting SystemPowerstate to Reset using the power state hint ", API_BASE + 9, 1, Tst_SetSystemPowerStateHintReset)
    FTE(1, "Test Setting SystemPowerstate to Reset using the power state hint and POWER_FORCE flag ", API_BASE + 10, 2, Tst_SetSystemPowerStateHintReset)
    FTE(1, "Test Setting SystemPowerstate to Off using the power state hint ", API_BASE + 11, 1, Tst_SetSystemPowerStateHintOff)
    FTE(1, "Test Setting SystemPowerstate to Off using the power state hint and POWER_FORCE flag ", API_BASE + 12, 2, Tst_SetSystemPowerStateHintOff)
    FTE(1, "Test Setting SystemPowerstate to Critical using the power state hint ", API_BASE + 13, 1, Tst_SetSystemPowerStateHintCritical)
    FTE(1, "Test Setting SystemPowerstate to Critical using the power state hint and POWER_FORCE flag ", API_BASE + 14, 2, Tst_SetSystemPowerStateHintCritical)

END_FTE

////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__
