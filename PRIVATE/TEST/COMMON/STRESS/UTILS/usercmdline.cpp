//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
      @doc USERCMDLINEUTILS


Module Name:

  usercmdline.cpp

 @topic User command line parser functions. |

  StressUtils.dll contains a set of functions to assist in the parsing
  of the user portions of a module's command line.  These are not to be
  used on the raw command line sent to your module by the stress harness,
  but only on the tszUser portion in the MODULE_PARAMS struct returned by
  InitializeStressUtils() (see \\\\cestress\\docs\\stress\\stressutils.hlp).
  <nl>

   <nl>Link:    stressutils.lib
   <nl>Include: stressutuils.h

     @xref    <f InitUserCmdLineUtils>
     @xref    <f User_SetOptionChars>
     @xref    <f User_WasOption>
	 @xref    <f User_StrictOptionsOnly>
     @xref    <f User_GetOption>
     @xref    <f User_GetOptionAsInt>
*/

#include <windows.h>
#include <tchar.h>
#include <stressutils.h>
#include <stressutilsp.h>
#include <cmdline.h>

//
// This class exists so that we can supports command lines of any length
// It dynamically allocates the argv structure as well as a locally stored
//   copy of the command line on Init().  
// By declaring a global of this time
//   and freeing the allocated memory at destruct time, we don't need to
//   require developers to call a cleanup function to free the dynamically
//   allocated data.
//
class CmdLine
{
public:
	TCHAR *pszCmdLine;
	int argc;
	TCHAR **argv;

	CmdLine()
	{
		pszCmdLine = NULL;
		argc = 0;
		argv = NULL;
	}

	~CmdLine()
	{
		Deinit();
	}

	BOOL Init(LPTSTR pszIncCmdLine)
	{
		if(!pszIncCmdLine || *pszIncCmdLine == (TCHAR)0)
			return FALSE;

		pszCmdLine = (TCHAR *)malloc((_tcslen(pszIncCmdLine) + 1) * sizeof(TCHAR));

		if(!pszCmdLine)
			return FALSE;

		_tcscpy(pszCmdLine, pszIncCmdLine);

		// Count the number of arguments to determine the size of the argv array
		int nNumSpace, nNumAfterQuote;
		BOOL fInQuote, fSpaceWasLast;

		nNumSpace = nNumAfterQuote = 0;
		fInQuote = fSpaceWasLast = FALSE;

		for(DWORD i = 0; i < _tcslen(pszIncCmdLine); i++)
		{
			switch(pszIncCmdLine[i])
			{
			case _T(' '):
				if(!fSpaceWasLast)
				{
					if(fInQuote)
						nNumAfterQuote++;
					else
						nNumSpace++;
					fSpaceWasLast = TRUE;
				}
				break;
			case _T('"'):
				// Change InQuote state
				if(!fInQuote)
					fInQuote = TRUE;
				else
				{
					fInQuote = FALSE;
					nNumAfterQuote = 0;
				}
				
				fSpaceWasLast = FALSE;
				break;
			default:
				fSpaceWasLast = FALSE;
				break;
			}
		}

		if(fInQuote)
			nNumSpace += nNumAfterQuote;  // Quote had no match

		argc = nNumSpace + (fSpaceWasLast ? 0 : 1);
		argv = (TCHAR **)malloc(argc * sizeof(TCHAR *));

		if(!argv)
		{
			argc = 0;
			free(pszCmdLine);
			return FALSE;
		}

		return TRUE;
	}

	void Deinit()
	{
		if(pszCmdLine)
			free(pszCmdLine);

		if(argv)
			free(argv);

		argc = 0;
		argv = NULL;
		pszCmdLine = NULL;
	}
};

CmdLine g_CommandLine;

//
//  @func extern "C" void | User_SetOptionChars |  
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
//  @ex |
//  User Command Line: -svr 157.59.28.250 /thrd 2 :Cycle100 -f -x -h
//  
//
//	TCHAR OptionChars[3] = {'-', '/', ':'}; 
//
//  User_SetOptionChars(3, OptionChars);
//  if( User_WasOption(TEXT("x")) )
//		printf("'x' was specified\n");
//  if( User_WasOption(TEXT("thrd")) )
//		printf("'thrd' was specified\n");
//  if( User_WasOption(TEXT("cycle")) )
//		printf("'cycle' was specified\n");
//
//  Ouput:
//  'x' was specified
//  'thrd' was specified
//  'cycle' was specified
//
// @xref
//	<f User_WasOption> <nl> 
//	<f User_GetOption> <nl> 
//	<f User_GetOptionAsInt>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" void User_SetOptionChars(int NumChars, TCHAR *tszCharArray)
{
	SetOptionChars(NumChars, tszCharArray);
}

//
//  @func extern "C" BOOL | User_WasOption |  
//  Returns whether or not the option specified was given on the command line. 
//
//  @rdesc Returns TRUE if the option was specified on the command line.  FALSE if it
//  was not.
//
//  @parm  IN LPCTSTR                | pszOption |  A pointer to the desired option.
//
//  @comm:(LIBRARY)
//  This function parses through the user command line looking for the desired 
//  option.  It returns the index of the last instance of the option in the given argv array.
//
//  @ex |
//  User Command Line: -svr 157.59.28.250 -thrd 2 -Cycle100 -f -x -h
//
//  if( User_WasOption(TEXT("x")) )
//		printf("'x' was specified\n");
//  if( User_WasOption(TEXT("thrd")) )
//		printf("'thrd' was specified\n");
//
//  Ouput:
//  'x' was specified
//  'thrd' was specified
//
// @xref
//	<f User_SetOptionChars><nl> 
//	<f User_GetOption> <nl> 
//	<f User_GetOptionAsInt>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" BOOL User_WasOption(LPCTSTR pszOption)
{
	return (WasOption(g_CommandLine.argc, g_CommandLine.argv, pszOption) >= 0);
}

//
//  @func extern "C" void | User_StrictOptionsOnly |  
//  Forces the use of only strict options on the command line.
//
//  @rdesc None.
//
//  @parm  IN BOOL	|	bStrict	|  TRUE for using strict options only.
//
//  @comm:(LIBRARY)
//  If bStrict is TRUE, future calls to the GetOption and GetOptionAsInt will only
//  recognize options of the form "<option flag><option code> <option>" instead of both those and
//  those of the form "<option flag><option code><option>".  
//
//  Set this option if you want to have option codes which are sub-strings of other option codes.
//  Otherwise the GetOption functions cannot tell the difference between -srv and -sRV (in the
//  first case the option code is "srv" and there is no option, in the second the option code is
//  "s" and the option is "RV")
//
//  @ex |
//  Command Line: -svr 157.59.28.250 -thrd 2 -Cycle100 -f -x -h
//
//  LPTSTR szArg = NULL;
//  if( User_GetOption(TEXT("cycle"), &szArg) )
//  	_tprintf(TEXT("cycle = %s\n"), szArg);
//  else
//      _tprintf(TEXT("No cycle option found.\n"));
//
//	User_StrictOptionsOnly(TRUE);
//  
//  if( User_GetOption(TEXT("cycle"), &szArg) )
//		_tprintf(TEXT("cycle = %s\n"), szArg);
//  else
//      _tprintf(TEXT("No cycle option found.\n"));
//
//  Ouput:
//  cycle = 100
//  No cycle option found.
//
// @xref
//	<f User_WasOption><nl> 
//	<f User_GetOpton> <nl> 
//	<f User_GetOptionAsInt>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" void User_StrictOptionsOnly(BOOL bStrict)
{
	StrictOptionsOnly(bStrict);
}


//
//  @func extern "C" BOOL | User_GetOption |  
//  Returns the argument of a specified option.
//
//  @rdesc Returns TRUE if the option was specified.  FALSE if the option could not be found.
//
//  @parm  IN LPCTSTR                | pszOption    |  A pointer to the desired option.
//  @parm  OUT LPTSTR *              | ppszArgument |  Address of the pointer that will point to the argument of the desired option.
//
//  @comm:(LIBRARY)
//  This function parses through the user command line looking for the desired 
//  option.  It returns (via the OUT parameter) a pointer to the argument of the specified option.
//
//  @ex |
//  User Command Line: -svr 157.59.28.250 -thrd 2 -Cycle100 -f -x -h
//
//  LPTSTR szArg = NULL;
//  if( User_GetOption(TEXT("svr"), &szArg) )
//  	_tprintf(TEXT("svr = %s\n"), szArg);
//  if( User_GetOption(TEXT("cycle"), &szArg) )
//		_tprintf(TEXT("cycle = %s\n"), szArg);
//
//  Ouput:
//  svr = 157.59.28.250
//  cycle = 100
//
// @xref
//	<f User_WasOption> <nl> 
//	<f User_GetOptionAsInt>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" BOOL User_GetOption(LPCTSTR pszOption, LPTSTR *ppszArgument)
{
	return(GetOption(g_CommandLine.argc, g_CommandLine.argv, pszOption, ppszArgument) >= 0);
}

//
//  @func extern "C" BOOL | User_GetOptionAsInt |  
//  Returns the argument of a specified option.
//
//  @rdesc Returns TRUE if the option was specified.  FALSE if the option could not be found.
//
//  @parm  IN LPCTSTR                | pszOption  |  A pointer to the desired option.
//  @parm  OUT int *                 | piArgument |  Pointer to an integer that will take the argument of the desired option.
//
//  @comm:(LIBRARY)
//  This function parses through the user command line looking for the desired 
//  option.  It returns (via the OUT parameter) the value of the option's argument as an integer.
//
//  @ex |
//  User Command Line: -svr 157.59.28.250 -thrd 2 -Cycle100 -f -x -h
//  
//  int iValue = 0;
//  if(User_GetOptionAsInt(TEXT("thrd"), &iValue) >= 0)
//		_tprintf(TEXT("thrd = %d\n"), iValue);
//  if(User_GetOptionAsInt(TEXT("cycle"), &iValue) >= 0)
//		_tprintf(TEXT("cycle = %d\n"), iValue);
//
//  Ouput:
//  thrd = 2
//  cycle = 100
//
// @xref
//	<f User_WasOption> <nl> 
//	<f User_GetOption>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" BOOL User_GetOptionAsInt(LPCTSTR pszOption, int *piArgument)
{
	return(GetOptionAsInt(g_CommandLine.argc, g_CommandLine.argv, pszOption, piArgument) >= 0);
}

//
//  @func extern "C" BOOL | User_GetOptionAsDWORD |  
//  Returns the argument of a specified option.
//
//  @rdesc Returns TRUE if the option was specified.  FALSE if the option could not be found.
//
//  @parm  IN LPCTSTR                | pszOption   |  A pointer to the desired option.
//  @parm  OUT DWORD *               | pdwArgument |  Pointer to a DWORD that will take the argument of the desired option.
//
//  @comm:(LIBRARY)
//  This function parses through the user command line looking for the desired 
//  option.  It returns (via the OUT parameter) the value of the option's argument as a DWORD.
//
//  @ex |
//  User Command Line: -svr 157.59.28.250 -thrd 2 -Cycle0xFFFFFFFF -f -x -h
//  
//  int iValue = 0;
//  if(User_GetOptionAsDWORD(TEXT("thrd"), &dwValue) >= 0)
//		_tprintf(TEXT("thrd = %u\n"), dwValue);
//  if(User_GetOptionAsDWORD(TEXT("cycle"), &dwValue) >= 0)
//		_tprintf(TEXT("cycle = %u\n"), dwValue);
//
//  Ouput:
//  thrd = 2
//  cycle = 4294967295
//
// @xref 
//  <f User_WasOption><nl> 
//	<f User_GetOption><nl> 
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" BOOL User_GetOptionAsDWORD(LPCTSTR pszOption, DWORD *pdwArgument)
{
	return(GetOptionAsDWORD(g_CommandLine.argc, g_CommandLine.argv, pszOption, pdwArgument) >= 0);
}



//
//  @func extern "C" int | InitUserCmdLineUtils |  
//  Initializes the user command line utilities.
//
//  @rdesc Returns the number of arguments, argc.
//
//  @parm  IN LPTSTR                | szCommandLine |  The command line.
//  @parm  OUT LPTSTR               | argv[] |  The command line in argv form. Can be NULL.
//
//  @comm:(LIBRARY)
//  This function must be called before any of the other user command
//	line utilities functions.  This function parses through the given command line 
//	and breaks it up into an argc/argv format (stored internally for use
//	by subsequent calls to user command line utilitiy functions).
//
//  Note: You do not need to allocate any space for argv[].  See example.
//
//  @ex |
//  
//  int argc;
//  LPTSTR *argv;
//  
//  argc = InitUserCmdLineUtils(lpszCommandline, &argv);
//  _tprintf(TEXT("There are %d arguments, the first one is %s\n"), 
//      argc, (argc > 0) ? argv[0] : TEXT(""));
//
// @xref
//	<f User_WasOption> <nl> 
//	<f User_GetOption> <nl> 
//	<f User_GetOptionAsInt>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" int InitUserCmdLineUtils(LPTSTR tszUserCommandLine, LPTSTR *argv[]) 
{
	if(g_CommandLine.argc != 0)
		g_CommandLine.Deinit();
	
	// Parse command line into individual arguments - arg/argv style.
	if(!g_CommandLine.Init(tszUserCommandLine))
		return 0;

	CommandLineToArgs(g_CommandLine.pszCmdLine, &(g_CommandLine.argc), g_CommandLine.argv);

	if(argv)
		*argv = g_CommandLine.argv;

	return g_CommandLine.argc;
}
