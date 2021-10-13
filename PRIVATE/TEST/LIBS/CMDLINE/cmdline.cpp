//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
      @doc LIBRARY


Module Name:

  cmdline.cpp

 @module CommandLine - Contains generic command line parser functions. |

  This file is meant to contain a variety of generic helper functions to aid in command line
  parsing.

   <nl>Link:    cmdline.lib
   <nl>Include: cmdline.h

     @xref    <f SetOptionChars>
     @xref    <f WasOption>
	 @xref    <f StrictOptionsOnly>
     @xref    <f GetOption>
     @xref    <f GetOptionAsInt>
     @xref    <f IsStringInt>
     @xref    <f CommandLineToArgs>
*/

#include <cmdline.h>

// Global Variables and defines
#define CMDLINE_MAX_OPTION_CHARS	5
int g_iNumOptionChars = 2;
TCHAR g_OptionChars[CMDLINE_MAX_OPTION_CHARS] = {'-', '/', ':', '=', ';'};
BOOL g_bStrictOptionsOnly = FALSE;

// Internal Function Prototypes
BOOL IsOptionChar(TCHAR cChar);

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

extern "C" void SetOptionChars(int NumChars, TCHAR *CharArray)
{
	g_iNumOptionChars = 0;

	if(NumChars)
	{
		__try{
			for(int i = 0; i < NumChars && i < CMDLINE_MAX_OPTION_CHARS; i++)
			{
				g_OptionChars[i] = CharArray[i];
				g_iNumOptionChars++;
			}
		}
		__except(1)
		{
			// Error - Bad parameters
			return;
		}
	}
}

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
//  <f SetOptionChars> <f GetOption> <f GetOptonAsInt> <f CommandLineToArgs>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" int WasOption(int argc, LPTSTR argv[], LPCTSTR pszOption)
{
	int iRet = -1;

    // Check parameter validity
	if(!pszOption || !argv)
		return -2;

	// Go through the command line looking for a match
	for( int i = 0; i < argc; i++)
	{
		__try
		{
			if(g_iNumOptionChars)
			{
				// We're only looking at arguments with a special option character
				if(IsOptionChar(argv[i][0]))
				{
					if(g_bStrictOptionsOnly)
					{
						// Must not be a substring
						if(_tcsicmp(&argv[i][1], pszOption) == 0)
							iRet = i;
					}
					else
					{
						if(_tcsnicmp(&argv[i][1], pszOption, _tcslen(pszOption)) == 0)
						{
							// Matched all the chars in pszOption
							iRet = i;
						}
					}
				}
			}
			else
			{
				if(g_bStrictOptionsOnly)
				{
					// Must not be a substring
					if(_tcsicmp(argv[i], pszOption) == 0)
						iRet = i;
				}
				else
				{
					// We're looking at all the arguments
					if(_tcsnicmp(argv[i], pszOption, _tcslen(pszOption)) == 0)
					{
						// Matched all the chars in pszOption
						iRet = i;
					}
				}
			}
		}
		__except(1)
		{
			// Our parameters were bad
			return -2;
		}
	}

	return iRet;
}

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

extern "C" void StrictOptionsOnly(BOOL bStrict)
{
	g_bStrictOptionsOnly = bStrict;
}


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
//  <f WasOption> <f GetOptonAsInt> <f CommandLineToArgs>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" int GetOption(int argc, LPTSTR argv[], LPCTSTR pszOption, LPTSTR *ppszArgument)
{
	int iRet = -1, iOffset = 0;

    // Check parameters
	if(!ppszArgument)
		return -2;

	iRet = WasOption(argc, argv, pszOption);

	if(iRet >= 0)
	{
		// The Option was found and its index is iRet

		// There are two possibities for the option's argument
		// it could look like:
		// -svr157.59.29.17
		// or
		// -svr 157.59.29.17
		// First we'll look for the former and then for the latter

		if(g_iNumOptionChars)
			iOffset++;

		if(_tcslen(&argv[iRet][iOffset]) > _tcslen(pszOption))
		{
			// First case
			*ppszArgument = (LPTSTR) &(argv[iRet][_tcslen(pszOption) + iOffset]);
		}
		else
		{
			// Second case
			if(iRet == argc - 1)
			{
				// option was the last thing specified
				// null argument
				*ppszArgument = NULL;
				iRet = -1;
			}
			else
			{
				// Option is the next argument in the argv array
				*ppszArgument = (LPTSTR) argv[iRet + 1];
			}
		}
	}

	return iRet;
}


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

extern "C" BOOL IsStringInt( LPCTSTR szNum )
{
	const TCHAR *pc = szNum;

	if(pc && *pc == _T('-'))
		pc++;

	for(;pc != NULL && *pc != '\0'; pc++ )
	{
		if(!isdigit(*pc))
			return FALSE;
	}

	if(pc == NULL)
		return FALSE;
	else
		return TRUE;
}

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
//  <f IsStringInt> <f WasOption> <f GetOption> <f CommandLineToArgs>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" int GetOptionAsInt(int argc, LPTSTR argv[], LPCTSTR pszOption, int *piArgument)
{
	int iRet = -1;
	LPTSTR pszArgument;

	// Check parameter validity
	if(!piArgument)
		return -2;

    iRet = GetOption(argc, argv, pszOption, &pszArgument);

	if(iRet >= 0)
	{
		// We got the option
		if(IsStringInt(pszArgument))
		{
			// The argument is a number
			*piArgument = _ttoi(pszArgument);
		}
		else
			iRet = -2;
	}

	return iRet;
}

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
//  int iValue = 0;
//  if(GetOptionAsDWORD(argc, argv, TEXT("thrd"), &dwValue) >= 0)
//		_tprintf(L"thrd = %u\n", dwValue);
//  if(GetOptionAsDWORD(argc, argv, TEXT("cycle"), &dwValue) >= 0)
//		_tprintf(L"cycle = %u\n", dwValue);
//
//  Ouput:
//  thrd = 2
//  cycle = 4294967295
//
//  <f IsStringInt> <f WasOption> <f GetOption> <f CommandLineToArgs>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" int GetOptionAsDWORD(int argc, LPTSTR argv[], LPCTSTR pszOption, DWORD *pdwArgument)
{
	int iRet = -1;
	LPTSTR pszArgument, pEndPtr;
	DWORD dwValue;

	// Check parameter validity
	if(!pdwArgument)
		return -2;

    iRet = GetOption(argc, argv, pszOption, &pszArgument);

	if(iRet >= 0)
	{
		if(_tcsncmp(pszArgument, _T("0x"), 2) == 0)
		{
			// probably hex
			dwValue = _tcstoul(pszArgument, &pEndPtr, 16);
		}
		else
		{
			// try decimal
			dwValue = _tcstoul(pszArgument, &pEndPtr, 10);
		}

		if(*pEndPtr == _T('\0'))
			*pdwArgument = dwValue;
		else
			return -2;
	}

	return iRet;
}

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
//  <f WasOption> <f GetOption> <f GetOptionAsInt>
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  None.

extern "C" void CommandLineToArgs(TCHAR szCommandLine[], int *argc, LPTSTR argv[]) 
{
    // Parse command line into individual arguments - arg/argv style.
	int i, iArgc = 0;
    BOOL fInQuotes, bEndFound = FALSE;
	TCHAR *p = szCommandLine;

	if(!szCommandLine || !argc || !argv)
		return;

    for(i = 0; i < *argc && !bEndFound; i++)
    {
        // Clear our quote flag
        fInQuotes = FALSE;

        // Move past and zero out any leading whitespace
        while( *p && _istspace(*p) )
            *(p++) = TEXT('\0');

        // If the next character is a quote, move over it and remember that we saw it
        if (*p == TEXT('\"'))
        {
            *(p++) = TEXT('\0');
            fInQuotes = TRUE;
        }

        // Point the current argument to our current location
        argv[i] = p;

        // If this argument contains some text, then update our argument count
        if (*p)
            iArgc = i + 1;

        // Move over valid text of this argument.
        while (*p)
        {
            if (fInQuotes)
            {
                // If in quotes, we only break out when we see another quote.
                if (*p == TEXT('\"'))
                {
                    *(p++) = TEXT('\0');
                    break;
                }
				
			}
			else
			{
				// If we are not in quotes and we see a quote, replace it with a space
				// and set "fInQuotes" to TRUE
				if (*p == TEXT('\"'))
				{
					*(p++) = TEXT(' ');
					fInQuotes = TRUE;
				}   // If we are not in quotes and we see whitespace, then we break out
				else if (_istspace(*p))
				{
					*(p++) = TEXT('\0');
					break;
				}
            }
            // Move to the next character
            p++;
        }

		if(!*p)
			bEndFound = TRUE;
    }

    *argc = iArgc;
}

////////////////////////////////////////////////////////////////////////////////////////
//				Internal Helper Functions
////////////////////////////////////////////////////////////////////////////////////////

BOOL IsOptionChar(TCHAR cChar)
{
	for(int i = 0; i < g_iNumOptionChars; i++)
	{
		if(g_OptionChars[i] == cChar)
			return TRUE;
	}

	return FALSE;
}