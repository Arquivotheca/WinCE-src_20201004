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
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@doc LIBRARY


Module Name:
    netcmn.h

Abstract:
    Declaration of Networking QA common functions and variables.

Revision History:
    12-Feb-2001		Created

-------------------------------------------------------------------*/
#ifndef _NET_CMN_H_
#define _NET_CMN_H_

#include <windows.h>
#include <tchar.h>
#ifndef UNDER_CE
#include <stdio.h>
#endif
#include "cmdstub.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////////////
//				Functions for registering WATT variables
////////////////////////////////////////////////////////////////////////////////////////

#define MAX_FILTER_VARS				16


//
//  @func extern "C" void | RegisterWattFilter |  
//  Registers a list of variables that are OK to print to WATT.
//
//  @rdesc None.
//
//  @parm  IN LPTSTR       | ptszFilter |  Comma demlimited list of variables names
//
//  @comm:(LIBRARY)
//  This function is used in conjuction with PrintWattVar to limit the number of
//  variables registered with the Automation Multi-Machine WATT harness.  This
//  enables developers to only output the variables they need, yet write apps
//  that are flexible enough to output many different values.
//
//  IMPORTANT NOTE: This function edits the string in place, changing ','
//  characters into '\0' characters.  The string must be editable.
//
//  For example:
//  
//  TCHAR tszFilterString[64];
//  _tcscpy(tszFilterString, _T("VAR1,VAR3"));
//  RegisterWATTFilter(tszFilterString);
//  PrintWattVar(_T("VAR1"), _T("VAL1"));
//  PrintWattVar(_T("VAR2"), _T("VAL2"));
//  PrintWattVar(_T("VAR3"), _T("VAL3"));
//
//  Output:
//
//  SET_WATT_VAR VAR1=VAL1
//  SET_WATT_VAR VAR3=VAL3
//
// 
//  <f PrintWattVar>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.
void RegisterWattFilter(LPTSTR ptszFilter);

//
//  @func extern "C" void | PrintWattVar |  
//  Registers a variable and associated value with WATT.
//
//  @rdesc None.
//
//  @parm  IN LPCTSTR       | ptszVarName  |  variable name
//  @parm  IN LPCTSTR       | ptszVarValue |  value of the variable
//
//  @comm:(LIBRARY)
//  This function is used to register variables with the WATT automation
//  harness.  Variables are output to debug only.
//
//  For example:
//     PrintWattVar(_T("VAR1"), _T("VAL1"));
//
//  Output:
//  SET_WATT_VAR VAR1=VAL1
//
// 
//  <f PrintWattVar>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.
void PrintWattVar(LPCTSTR ptszVarName, LPCTSTR ptszVarValue);

////////////////////////////////////////////////////////////////////////////////////////
//				Print out WTT variable
////////////////////////////////////////////////////////////////////////////////////////

//
//  @func extern "C" void | PrintWttVar |  
//  Registers a variable and associated value with WTT.
//
//  @rdesc None.
//
//  @parm  IN LPCTSTR       | ptszVarName  |  variable name
//  @parm  IN LPCTSTR       | ptszVarValue |  value of the variable
//
//  @comm:(LIBRARY)
//  This function is used to register variables with the WTT automation
//  harness.  Variables are output to debug only.
//
//  For example:
//     PrintWttVar(_T("VAR1"), _T("VAL1"));
//
//  Output:
//  WTT_SET_PARAMETER Key="VAR1" Value="VAL1"\n")
//
// 
//  <f PrintWttVar>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.
void PrintWttVar(LPCTSTR ptszVarName, LPCTSTR ptszVarValue);

////////////////////////////////////////////////////////////////////////////////////////
//				Random Number Function - NT/CE
////////////////////////////////////////////////////////////////////////////////////////


//
//  @func extern "C" DWORD | GetRandomNumber |  
//  Returns a random number between 0 and the number given.
//
//  @rdesc None.
//
//  @parm  IN DWORD                  | dwRandMax |  The maximum number to return
//
//  @comm:(LIBRARY)
//  This function generates a random integer in the range [0, dwRandMax]
//  If dwRandMax is set to 0, the function will return 0.
//
//  For example:
//  
//  DWORD dwRand;
//  for(int i = 0; i < 3; i++)
//  {
//      dwRand = GetRandomNumber(2);
//      printf("%d\n", dwRand);
//  }
//
//  Output:
//
//  0
//  2
/// 1
// 
//  <f GetRandomRange>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.
DWORD GetRandomNumber(DWORD dwRandMax);

//
//  @func extern "C" DWORD | GetRandomRange |  
//  Returns a random number between min given and max given.
//
//  @rdesc None.
//
//  @parm  IN DWORD                  | dwRandMin |  The minimum number to return
//  @parm  IN DWORD                  | dwRandMax |  The maximum number to return
//
//  @comm:(LIBRARY)
//  This function returns a random integer in the range [dwRandMin, dwRandMax]
//  dwRandMax must be >= dwRandMin
//
//  For example:
//  
//  DWORD dwRand;
//  for(int i = 0; i < 3; i++)
//  {
//      dwRand = GetRandomRange(1, 3);
//      printf("%d\n", dwRand);
//  }
//
//  Output:
//
//  3
//  1
/// 2
// 
//  <f GetRandomNumber>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.
DWORD GetRandomRange(DWORD dwRandMin, DWORD dwRandMax);

//
//  @func extern "C" DWORD | GetLastErrorText |  
//  Returns a text translation of a GetLastError() code
//
//  @rdesc None.
//
//  @parm  none
//
//  @comm:(LIBRARY)
//  This function calls GetLastError and translated the error code into a defined TCHAR
//  text string for display purposes.
//
//  For example:
//  
//  printf("error %d = %s\n", GetLastError(), GetLastErrorText());
//
//  Output:
//
//  error 10054 = WSAECONNRESET
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.
TCHAR* GetLastErrorText();

//
//  @func extern "C" DWORD | ErrorToText |  
//  Returns a text translation of a Windows or WinSock error code
//
//  @rdesc None.
//
//  @parm  none
//
//  @comm:(LIBRARY)
//  This function translates the given error code into a defined TCHAR
//  text string for display purposes.
//
//  For example:
//  
//  dwError = GetLastError();
//  tprintf("error %d = %s\n", dwError, ErrorToText(dwError));
//
//  Output:
//
//  error 10054 = WSAECONNRESET
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.
TCHAR* ErrorToText(DWORD dwErrorCode);

#ifdef __cplusplus
}
#endif

#endif