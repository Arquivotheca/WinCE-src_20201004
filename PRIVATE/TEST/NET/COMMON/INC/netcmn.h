/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@doc LIBRARY

Copyright (c) 2000-2001 Microsoft Corporation

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
#include "cmdstub.h"

#ifdef __cplusplus
extern "C" {
#endif


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

////////////////////////////////////////////////////////////////////////////////////////
//				Registry Editing Functions
////////////////////////////////////////////////////////////////////////////////////////
#define TEST_SERVER_CODE_NAME	(_T("testserver"))
BOOL GetServerName(TCHAR szName[], DWORD dwSize);

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

#ifdef __cplusplus
}
#endif

#endif