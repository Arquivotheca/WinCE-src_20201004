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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

/*
      @doc LIBRARY

Module Name:

  cmnwrap.cpp

 @module CmnWrap - Contains QA Wrappers for the NetCmn functions. |

  This file is meant to hold wrapper functions for the NetCmn functions which handle
  command line parsin and other common code.

  This is part of the NetMain module

   <nl>Link:    netmain.lib
   <nl>Include: netmain.h

     @xref    <f _tmain>
     @xref    <f mymain>
*/

#include "netmain.h"
#include "netcmn.h"

#ifndef ASSERT
#include <assert.h>
#define ASSERT assert
#endif

//
// Global Variables
//
extern int g_argc;
extern LPTSTR *g_argv;

//
//  @func extern "C" void | QASetOptionChars |  
//  Sets the characters interpreted as option characters (e.g. '-', '/', etc.)
//
//  @rdesc None.
//
//  @parm  IN int                    | NumChars  |  Number of arguments.
//  @parm  IN TCHAR *                | CharArray |  An array of characters
//
//  @comm:(LIBRARY)
//  This function is a wrapper for QASetOptionChars in NetCmn.lib
//
//  For example:
//  Command Line: testexe -svr 157.59.28.250 /thrd 2 :Cycle100 -f -x -h
//  
//  int i;
//	TCHAR OptionChars[3] = {'-', '/', ':'}; 
//
//  SetOptionChars(3, OptionChars);
//  if( QAWasOption(_T("x")) )
//		printf("'x' was specified\n");
//  if( QAWasOption(_T("thrd")) )
//		printf("'thrd' was specified\n");
//  if( QAWasOption(_T("cycle")) )
//		printf("'cycle' was specified\n");
//
//  Ouput:
//  'x' was specified
//  'thrd' was specified
//  'cycle' was specified
//
//  <f QAWasOption> <f QAGetOption> <f QAGetOptonAsInt>
void QASetOptionChars(int NumChars, TCHAR *CharArray)
{
	SetOptionChars(NumChars, CharArray);
}

//
//  @func BOOL | QAWasOption |  
//  Returns whether or not the option specified was given on the command line. 
//
//  @rdesc TRUE for SUCCESS.  FALSE if the option was not specified or if an error occurred.
//
//  @parm  IN LPCTSTR                | pszOption |  A pointer to the desired option.
//
//  @comm:(LIBRARY)
//  This function is a wrapper for WasOption in NetCmn.lib
//
//  For example:
//  Command Line: testexe -svr 157.59.28.250 -thrd 2 -Cycle100 -f -x -h
//  
//  if( QAWasOption(_T("x")) )
//		printf("'x' was specified\n");
//  if( QAWasOption(_T("thrd")) )
//		printf("'thrd' was specified\n");
//
//  Ouput:
//  'x' was specified
//  'thrd' was specified
//
//  <f QAGetOption> <f QAGetOptonAsInt>

BOOL QAWasOption(LPCTSTR pszOption)
{
	if(WasOption(g_argc, g_argv, pszOption) >= 0)
		return TRUE;
	else
		return FALSE;
}

//
//  @func BOOL | QAGetOption |  
//  Returns the argument of a specified option.
//
//  @rdesc TRUE for SUCCESS.  FALSE if the option was not specified or if an error occurred.
//
//  @parm  IN LPCTSTR                | pszOption    |  A pointer to the desired option.
//  @parm  OUT LPTSTR *              | ppszArgument |  Address of the pointer that will point to the argument of the desired option.
//
//  @comm:(LIBRARY)
//  This function is a wrapper for GetOption in NetCmn.lib
//
//  For example:
//  Command Line: testexe -svr 157.59.28.250 -thrd 2 -Cycle100 -f -x -h
//  
//  LPTSTR szArg = NULL;
//  if( QAGetOption(TEXT("svr"), &szArg) )
//  	_tprintf(_T("svr = %s\n"), szArg);
//  if( QAGetOption(TEXT("cycle"), &szArg) )
//		_tprintf(_T("cycle = %s\n"), szArg);
//
//  Ouput:
//  svr = 157.59.28.250
//  cycle = 100
//
//  <f QAWasOption> <f QAGetOptonAsInt> <f QAGetOptionAsDWORD>

BOOL QAGetOption(LPCTSTR pszOption, LPTSTR *ppszArgument)
{
	if(GetOption(g_argc, g_argv, pszOption, ppszArgument) >= 0)
		return TRUE;
	else
		return FALSE;
}

//
//  @func BOOL | QAGetOptionAsInt |  
//  Returns the argument of a specified option as an integer.
//
//  @rdesc TRUE for SUCCESS.  FALSE if the option was not specified or if an error occurred.
//
//  @parm  IN LPCTSTR                | pszOption  |  A pointer to the desired option.
//  @parm  OUT int *                 | piArgument |  Pointer to an integer that will take the argument of the desired option.
//
//  @comm:(LIBRARY)
//  This function is a wrapper for GetOptionAsInt in NetCmn.lib
//
//  For example:
//  Command Line: testexe -svr 157.59.28.250 -thrd 2 -Cycle100 -f -x -h
//  
//  int iValue = 0;
//  if( QAGetOptionAsInt(TEXT("thrd"), &iValue) )
//		_tprintf(_T("thrd = %d\n"), iValue);
//  if( QAGetOptionAsInt(TEXT("cycle"), &iValue) )
//		_tprintf(_T("cycle = %d\n"), iValue);
//
//  Ouput:
//  thrd = 2
//  cycle = 100
//
//  <f QAWasOption> <f QAGetOption> <f QAGetOptionAsDWORD>

BOOL QAGetOptionAsInt(LPCTSTR pszOption, int *piArgument)
{
	if(GetOptionAsInt(g_argc, g_argv, pszOption, piArgument) >= 0)
		return TRUE;
	else
		return FALSE;
}

//
//  @func BOOL | QAGetOptionAsDWORD |  
//  Returns the argument of a specified option as a DWORD.  This works for arguments
//  given in both decimal and hexidecimal.
//
//  @rdesc TRUE for SUCCESS.  FALSE if the option was not specified or if an error occurred.
//
//  @parm  IN LPCTSTR                | pszOption   |  A pointer to the desired option.
//  @parm  OUT DWORD *               | pdwArgument |  Pointer to a DWORD that will take the argument of the desired option.
//
//  @comm:(LIBRARY)
//  This function is a wrapper for GetOptionAsDWORD in NetCmn.lib
//
//  For example:
//  Command Line: testexe -svr 157.59.28.250 -thrd 2 -Cycle0xFFFFFFFF -f -x -h
//  
//  DWORD dwValue = 0;
//  if( QAGetOptionAsDWORD(TEXT("thrd"), &dwValue) )
//		_tprintf(_T("thrd = %u\n"), dwValue);
//  if( QAGetOptionAsDWORD(TEXT("cycle"), &dwValue) )
//		_tprintf(_T("cycle = %u\n"), dwValue);
//
//  Ouput:
//  thrd = 2
//  cycle = 4294967295
//
//  <f QAWasOption> <f QAGetOption> <f QAGetOptionAsInt>

BOOL QAGetOptionAsDWORD(LPCTSTR pszOption, DWORD *pdwArgument)
{
	if(GetOptionAsDWORD(g_argc, g_argv, pszOption, pdwArgument) >= 0)
		return TRUE;
	else
		return FALSE;
}
