/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@doc LIBRARY

Copyright (c) 2000-2001 Microsoft Corporation

Module Name:
    cmdline.h

Abstract:
    Declaration of command line parsing helper functions

Revision History:
    13-Aug-2001		coreyb		Created

-------------------------------------------------------------------*/
#ifndef __COMMAND_LINE_H__
#define __COMMAND_LINE_H__

#include <windows.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif

//
//  @func extern "C" void | SetOptionChars |  
//  Sets the characters interpreted as option characters (e.g. '-', '/', etc.)
//
//  @rdesc None.
//
//  @parm  IN int                    | NumChars  |  Number of arguments.
//  @parm  IN TCHAR *                | CharArray |  An array of characters
//
//  @comm:(LIBRARY)
//  This function sets the option characters interpreted by the option parsing functions (e.g. WasOption).
//  By default, the option characters are '-' and '/'.  A maximum of 5 option characters can be specified.
//	To enable parsing of every argument with the option functions, set NumChars to 0.
//
//  For example:
//  Command Line: testexe -svr 157.59.28.250 /thrd 2 :Cycle100 -f -x -h
//  
//  int i;
//	TCHAR OptionChars[3] = {'-', '/', ':'}; 
//
//  SetOptionChars(3, OptionChars);
//  if( (i = WasOption(argc, argv, TEXT("x"))) >= 0 )
//		printf("'x' was specified in argument %d\n", i);
//  if( (i = WasOption(argc, argv, TEXT("thrd"))) >= 0 )
//		printf("'thrd' was specified in argument %d\n", i);
//  if( (i = WasOption(argc, argv, TEXT("cycle"))) >= 0 )
//		printf("'cycle' was specified in argument %d\n", i);
//
//  Ouput:
//  'x' was specified in argument 7
//  'thrd' was specified in argument 3
//  'cycle' was specified in argument 5
//
//  <f WasOption> <f GetOption> <f GetOptonAsInt> <f CommandLineToArgs>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

void SetOptionChars(int NumChars, TCHAR *CharArray);

//
//  @func extern "C" int | WasOption |  
//  Returns whether or not the option specified was given on the command line. 
//
//  @rdesc The argument index of the last specified option if it was given
//	on the command line (case insensitive).  -1 if the option was
//	not specified.  A larger negative return value indicates an error occurred.
//
//  @parm  IN int                    | argc      |  Number of arguments.
//  @parm  IN LPTSTR *               | argv      |  An array of pointers to the null-terminated arguments.
//  @parm  IN LPCTSTR                | pszOption |  A pointer to the desired option.
//
//  @comm:(LIBRARY)
//  This function parses through the given command line (in argc/argv format) looking for the desired 
//  option.  It returns the index of the last instance of the option in the given argv array.
//
//  For example:
//  Command Line: testexe -svr 157.59.28.250 -thrd 2 -Cycle100 -f -x -h
//  
//  int i;
//  if( (i = WasOption(argc, argv, TEXT("x"))) >= 0 )
//		printf("'x' was specified in argument %d\n", i);
//  if( (i = WasOption(argc, argv, TEXT("thrd"))) >= 0 )
//		printf("'thrd' was specified in argument %d\n", i);
//
//  Ouput:
//  'x' was specified in argument 7
//  'thrd' was specified in argument 3
//
//  <f GetOption> <f GetOptonAsInt> <f CommandLineToArgs>

int WasOption(int argc, LPTSTR argv[], LPCTSTR pszOption);

//
//  @func extern "C" void | StrictOptionsOnly |  
//  Forces the use of only strict options on the command line.
//
//  @rdesc None.
//
//  @parm  IN BOOL                    |      bStrict    |  TRUE for using strict options only.
//
//  @comm:(LIBRARY)
//  If bStrict is TRUE, future calls to the GetOption and GetOptionAsInt will only
//  recognize options of the form "<option flag><option code> <option>" instead of both those and
//  those of the form "<option flag><option code> <option>".  
//
//  Set this option if you want to have option code which are sub-strings of other option codes.
//  Otherwise the GetOption functions cannot tell the difference between -srv and -sRV (in the
//  first case the option code is "srv" and there is no option, in the second the option code is
//  "s" and the option is "RV")
//
//  For example:
//  Command Line: testexe -svr 157.59.28.250 -thrd 2 -Cycle100 -f -x -h
//  
//  LPTSTR szArg = NULL;
//  if(GetOption(argc, argv, TEXT("cycle"), &szArg) >= 0)
//  	_tprintf(L"cycle = %s\n", szArg);
//  else
//      _tprintf(L"No cycle option found.\n");
//
//	StrictOptionsOnly(TRUE);
//  
//  if(GetOption(argc, argv, TEXT("cycle"), &szArg) >= 0)
//		_tprintf(L"cycle = %s\n", szArg);
//  else
//      _tprintf(L"No cycle option found.\n");
//
//  Ouput:
//  cycle = 100
//  No cycle option found.
//
//  <f WasOption> <f GetOpton> <f GetOptonAsInt> <f CommandLineToArgs>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

void StrictOptionsOnly(BOOL bStrict);

//
//  @func extern "C" int | GetOption |  
//  Returns the argument of a specified option.
//
//  @rdesc A non-negative number for SUCCESS.  -1 if the option/argument was
//	not specified.  A larger negative return value indicates an error occurred.
//
//  @parm  IN int                    | argc         |  Number of arguments.
//  @parm  IN LPTSTR *               | argv         |  An array of pointers to the null-terminated arguments.
//  @parm  IN LPCTSTR                | pszOption    |  A pointer to the desired option.
//  @parm  OUT LPTSTR *              | ppszArgument |  Address of the pointer that will point to the argument of the desired option.
//
//  @comm:(LIBRARY)
//  This function parses through the given command line (in argc/argv format) looking for the desired 
//  option.  It returns (via the OUT parameter) a pointer to the argument of the specified option.
//
//  For example:
//  Command Line: testexe -svr 157.59.28.250 -thrd 2 -Cycle100 -f -x -h
//  
//  LPTSTR szArg = NULL;
//  if(GetOption(argc, argv, TEXT("svr"), &szArg) >= 0)
//  	_tprintf(L"svr = %s\n", szArg);
//  if(GetOption(argc, argv, TEXT("cycle"), &szArg) >= 0)
//		_tprintf(L"cycle = %s\n", szArg);
//
//  Ouput:
//  svr = 157.59.28.250
//  cycle = 100
//
//  <f WasOption> <f GetOptonAsInt> <f GetOptionAsDWORD> <f CommandLineToArgs>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

int GetOption(int argc, LPTSTR argv[], LPCTSTR pszOption, LPTSTR *ppszArgument);

//
//  @func extern "C" BOOL | IsStringInt |  
//  Checks to see if the given string is an integer.
//
//  @rdesc TRUE if the given string is an integer.  FALSE if it is not, or if
//	an error occurred.
//
//  @parm  IN LPCTSTR                | pszNum   |  A pointer to the string.
//
//  @comm:(LIBRARY)
//  This function checks to see if the string passed in is a valid integer ( Ok to pass to _ttoi ).
//
//  For example:
//  
//  LPTSTR szNum1 = _T("1000");
//  LPTSTR szNum2 = _T("-2000");
//  LPTSTR szNum3 = _T("1a");
//  if(IsStringInt(szNum1))
//  	_tprintf(L"%s\n", szNum1);
//  if(IsStringInt(szNum2))
//		_tprintf(L"%s\n", szNum2);
//  if(IsStringInt(szNum3))
//		_tprintf(L"%s\n", szNum3);
//
//  Ouput:
//  1000
//  -2000
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

BOOL IsStringInt( LPCTSTR szNum );

//
//  @func extern "C" int | GetOptionAsInt |  
//  Returns the argument of a specified option.
//
//  @rdesc A non-negative number for SUCCESS.  -1 if the option/argument was
//	not specified.  A larger negative return value indicates an error occurred.
//
//  @parm  IN int                    | argc       |  Number of arguments.
//  @parm  IN LPTSTR *               | argv       |  An array of pointers to the null-terminated arguments.
//  @parm  IN LPCTSTR                | pszOption  |  A pointer to the desired option.
//  @parm  OUT int *                 | piArgument |  Pointer to an integer that will take the argument of the desired option.
//
//  @comm:(LIBRARY)
//  This function parses through the given command line (in argc/argv format) looking for the desired 
//  option.  It returns (via the OUT parameter) the value of the option's argument as an integer.
//
//  For example:
//  Command Line: testexe -svr 157.59.28.250 -thrd 2 -Cycle100 -f -x -h
//  
//  int iValue = 0;
//  if(GetOptionAsInt(argc, argv, TEXT("thrd"), &iValue) >= 0)
//		_tprintf(L"thrd = %d\n", iValue);
//  if(GetOptionAsInt(argc, argv, TEXT("cycle"), &iValue) >= 0)
//		_tprintf(L"cycle = %d\n", iValue);
//
//  Ouput:
//  thrd = 2
//  cycle = 100
//
//  <f IsStringInt> <f WasOption> <f GetOption> <f GetOptionAsDWORD> <f CommandLineToArgs>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

int GetOptionAsInt(int argc, LPTSTR argv[], LPCTSTR pszOption, int *piArgument);

//
//  @func extern "C" int | GetOptionAsDWORD |  
//  Returns the argument of a specified option.
//
//  @rdesc A non-negative number for SUCCESS.  -1 if the option/argument was
//	not specified.  A larger negative return value indicates an error occurred.
//
//  @parm  IN int                    | argc        |  Number of arguments.
//  @parm  IN LPTSTR *               | argv        |  An array of pointers to the null-terminated arguments.
//  @parm  IN LPCTSTR                | pszOption   |  A pointer to the desired option.
//  @parm  OUT DWORD *               | pdwArgument |  Pointer to a DWORD that will take the argument of the desired option.
//
//  @comm:(LIBRARY)
//  This function parses through the given command line (in argc/argv format) looking for the desired 
//  option.  It returns (via the OUT parameter) the value of the option's argument as a DWORD.
//
//  For example:
//  Command Line: testexe -svr 157.59.28.250 -thrd 2 -Cycle0xFFFFFFFF -f -x -h
//  
//  DWORD dwValue = 0;
//  if(GetOptionAsDWORD(argc, argv, TEXT("thrd"), &dwValue) >= 0)
//		_tprintf(L"thrd = %u\n", dwValue);
//  if(GetOptionAsDWORD(argc, argv, TEXT("cycle"), &dwValue) >= 0)
//		_tprintf(L"cycle = %u\n", dwValue);
//
//  Ouput:
//  thrd = 2
//  cycle = 4294967295
//
//  <f IsStringInt> <f WasOption> <f GetOption> <f GetOptionAsInt> <f CommandLineToArgs>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" int GetOptionAsDWORD(int argc, LPTSTR argv[], LPCTSTR pszOption, DWORD *pdwArgument);

//
//  @func extern "C" void | CommandLineToArgs |  
//  Parses the given command line into an arc/argv format.
//
//  @rdesc None.
//
//  @parm  IN TCHAR *                | szCommandLine |  The command line.
//  @parm  IN OUT int *              | argc          |  Number of arguments.
//  @parm  OUT LPTSTR *              | argv          |  An array of string pointers.
//
//  @comm:(LIBRARY)
//  This function parses through the given command line and breaks it up (in place)
//  into an argc/argv format.  The argc parameter should be initially set to the number
//  of pointers in the argv array.
//
//  For example:
//  
//  int argc;
//  LPTSTR argv[64];
//  argc = 64;
//  CommandLineToArgs(lpszCommandline, &argc, argv);
//
//  <f WasOption> <f GetOption> <f GetOptionAsInt> <f GetOptionAsDWORD>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

void CommandLineToArgs(TCHAR szCommandLine[], int *argc, LPTSTR argv[]);

#ifdef __cplusplus
}
#endif

#endif // __COMMAND_LINE_H__