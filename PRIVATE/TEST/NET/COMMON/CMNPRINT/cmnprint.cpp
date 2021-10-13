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

  cmnprint.cpp

 @module CmnPrint - Provides a common printing infrastructure for helper libraries. |

  A common problem when writing a library is how to handle printing.  Sometimes applications
  use special logging functions like those with kato, or they use printf.  Sometimes
  the application doesn't want the library to print at all.

  CmnPrint provides a basic framework for libraries to abstract their logging and for the
  application writer to determine how printing will be handled.

  Registered print functions must take TCHAR arguments, for example _putts() instead of puts().

   <nl>Link:    cmnprint.lib
   <nl>Include: cmnprint.h

     @xref    <f CmnPrint_Initialize>
     @xref    <f CmnPrint_RegisterPrintFunction>
	 @xref    <f CmnPrint_RegisterPrintFunctionEx>
     @xref    <f CmnPrint_Cleanup>
     @xref    <f CmnPrint>
     @xref    <f PrintFunc>
*/
#include <cmnprint.h>

typedef struct {
	LPF_PRINT lpfPrintFunc;
	LPF_PRINTEX lpfPrintFuncEx;
	BOOL fAddEOL;
} PrintFunc;

PrintFunc g_rgPF[MAX_PRINT_TYPE + 1];
BOOL g_fCmnPrint_Initialized = FALSE;
CRITICAL_SECTION g_csCmnPrint;

//
//  @func extern "C" void | CmnPrint_Initialize |  
//  Initializes the CmnPrint settings.
//
//  @rdesc None.
//
//  @comm:(LIBRARY)
//  This function should be called by the application before registering print functions.  
//  CmnPrint_Initialize can be called again at any time to re-initialize the print settings.  
//  Initializes the CmnPrint settings.  By default, the CmnPrint function will not output anything.
//
//
// @ex		Simple usage: |
//
//			CmnPrint_Initialize();
//			CmnPrint_RegisterPrintFunction(PT_LOG, OutputDebugString, FALSE);
//			CmnPrint_Cleanup();
//
//**********************************************************************************
extern "C" void CmnPrint_Initialize(void)
{
	memset(g_rgPF, 0, sizeof(g_rgPF));
	if(!g_fCmnPrint_Initialized)
	{
		InitializeCriticalSection(&g_csCmnPrint);	// Only initialize the CS once
		g_fCmnPrint_Initialized = TRUE;
	}
}

//
//  @func extern "C" void | CmnPrint_Cleanup |  
//  Cleans up allocated memory and objects.
//
//  @rdesc None.
//
//  @comm:(LIBRARY)
//  This function should be called by the application when it is finished with the CmnPrint  
//  library.
//
//
// @ex		Simple usage: |
//
//			CmnPrint_Initialize();
//			CmnPrint_RegisterPrintFunction(PT_LOG, OutputDebugString, FALSE);
//			CmnPrint_Cleanup();
//
//**********************************************************************************
extern "C" void CmnPrint_Cleanup(void)
{
	if(g_fCmnPrint_Initialized)
	{
		memset(g_rgPF, 0, sizeof(g_rgPF));
		DeleteCriticalSection(&g_csCmnPrint);
		g_fCmnPrint_Initialized = FALSE;
	}
}

//
//  @func extern "C" BOOL | CmnPrint_RegisterPrintFunction |  
//  Registers a print function to use for a given log type.
//
//  @rdesc TRUE if the function was successfully registered.  False otherwise.
//
//  @parm  IN DWORD                    | dwType    |  Print Type
//  @parm  IN LPF_PRINT                | lpfPrint  |  Print function pointer
//  @parm  IN BOOL                     | fAddEOL   |  Flag to add end-of-line to each output
//
//  @comm:(LIBRARY)
//  This function registers simple print functions like OutputDebugString or _putts() for the
//  specified print type.
//
//  Set fAddEOL to TRUE if you want the library to automatically add an end of line character
//  for each print call.  For example, OutputDebugString would probably not need this set
//  as it automatically line feeds on each call, but _tprintf would since it does not.
//
//
// @ex		Simple usage: |
//
//			CmnPrint_Initialize();
//			CmnPrint_RegisterPrintFunction(PT_ALL, OutputDebugString, FALSE);
//			CmnPrint(PT_LOG, TEXT("Happy %s"), TEXT("birthday!"));
//			CmnPrint_Cleanup();
//
//			Output:
//				Happy birthday!
//
//**********************************************************************************
extern "C" BOOL CmnPrint_RegisterPrintFunction(DWORD dwType, LPF_PRINT lpfPrint, BOOL fAddEOL)
{
	BOOL bSuccess = FALSE;

	if(!g_fCmnPrint_Initialized)
		CmnPrint_Initialize();

	EnterCriticalSection(&g_csCmnPrint);

	if(dwType == PT_ALL || dwType <= MAX_PRINT_TYPE)
	{
		__try
		{
			//
			// The lines below (and the try/except) were originally included
			// to verify that the print function was working.  Unfortunately
			// some print functions like QAError, automatically record a 
			// failure when they are called so we cannot call the function
			// here to verify that it works.
			//

			//if(lpfPrint)
			//{
			//	lpfPrint(TEXT("Successfully registered print function"));
			//	if(fAddEOL)
			//		lpfPrint(TEXT("\n"));
			//}

			if(dwType == PT_ALL)
			{
				for( int i = 0; i <= MAX_PRINT_TYPE; i++)
				{
					g_rgPF[i].lpfPrintFuncEx = NULL;
					g_rgPF[i].lpfPrintFunc = lpfPrint;
					g_rgPF[i].fAddEOL = fAddEOL;
				}
			}
			else
			{
				g_rgPF[dwType].lpfPrintFuncEx = NULL;
				g_rgPF[dwType].lpfPrintFunc = lpfPrint;
				g_rgPF[dwType].fAddEOL = fAddEOL;
			}

			bSuccess = TRUE;
		}
		__except(1)
		{
			DebugBreak();
		}	
	}

	LeaveCriticalSection(&g_csCmnPrint);

	return bSuccess;
}

//
//  @func extern "C" BOOL | CmnPrint_RegisterPrintFunctionEx |  
//  Registers an advanced print function to use for a given log type.
//
//  @rdesc TRUE if the function was successfully registered.  False otherwise.
//
//  @parm  IN DWORD                    | dwType    |  Print Type
//  @parm  IN LPF_PRINT                | lpfPrint  |  Print function pointer
//  @parm  IN BOOL                     | fAddEOL   |  Flag to add end-of-line to each output
//
//  @comm:(LIBRARY)
//  This function registers advanced print functions like _tprintf that take in a formatted
//  string for the specified print type.
//
//  Set fAddEOL to TRUE if you want the library to automatically add an end of line character
//  for each print call.  For example, OutputDebugString would probably not need this set
//  as it automatically line feeds on each call, but _tprintf would since it does not.
//
// @ex		Simple usage: |
//
//			CmnPrint_Initialize();
//			CmnPrint_RegisterPrintFunctionEx(PT_ALL, _tprintf, FALSE);
//			CmnPrint(PT_LOG, TEXT("Happy %s"), TEXT("birthday!"));
//			CmnPrint_Cleanup();
//
//			Output:
//				Happy birthday!
//
//**********************************************************************************
extern "C" BOOL CmnPrint_RegisterPrintFunctionEx(DWORD dwType, LPF_PRINTEX lpfPrint, BOOL fAddEOL)
{
	BOOL bSuccess = FALSE;

	if(!g_fCmnPrint_Initialized)
		CmnPrint_Initialize();

	EnterCriticalSection(&g_csCmnPrint);

	if(dwType == PT_ALL || dwType <= MAX_PRINT_TYPE)
	{
		__try
		{
			//
			// The lines below (and the try/except) were originally included
			// to verify that the print function was working.  Unfortunately
			// some print functions like QAError, automatically record a 
			// failure when they are called so we cannot call the function
			// here to verify that it works.
			//

			//if(lpfPrint)
			//{
			//	lpfPrint(TEXT("Successfully registered print function.  Test %d, %d, %d..."), 1, 2, 3);
			//	if(fAddEOL)
			//		lpfPrint(TEXT("\n"));
			//}

			if(dwType == PT_ALL)
			{
				for( int i = 0; i <= MAX_PRINT_TYPE; i++)
				{
					g_rgPF[i].lpfPrintFunc = NULL;
					g_rgPF[i].lpfPrintFuncEx = lpfPrint;
					g_rgPF[i].fAddEOL = fAddEOL;
				}
			}
			else
			{
				g_rgPF[dwType].lpfPrintFunc = NULL;
				g_rgPF[dwType].lpfPrintFuncEx = lpfPrint;
				g_rgPF[dwType].fAddEOL = fAddEOL;
			}

			bSuccess = TRUE;
		}
		__except(1)
		{
			DebugBreak();
		}	
	}

	LeaveCriticalSection(&g_csCmnPrint);

	return bSuccess;
}


//
//  @func extern "C" void | CmnPrint |  
//  Prints text via the registered function for the specified print type.
//
//  @rdesc None.
//
//  @parm  IN DWORD                    | dwType    |  Print Type
//  @parm  IN const TCHAR *            | pFormat   |  Formatted text (printf-style)
//  @parm  ...
//
//  @comm:(LIBRARY)
//  This function actually makes the printing calls to the registered print functions.
//  It should be called by the hepler libraries whenever logging/printing is required.
//  It does not hold a critical section while calling into the printing function.
//
//  Callers of this function do not need a line-feed at the end of the formatted string.
//  For example, use "My Text Here" instead of "My Text Here\n".  The function will
//  automatically add an end of line char if the application requests it.
//
// @ex		Simple usage: |
//
//			CmnPrint_Initialize();
//			CmnPrint_RegisterPrintFunctionEx(PT_ALL, _tprintf, FALSE);
//			CmnPrint(PT_LOG, TEXT("Happy %s"), TEXT("birthday!"));
//			CmnPrint_Cleanup();
//
//			Output:
//				Happy birthday!
//
//**********************************************************************************
extern "C" void CmnPrint(
    DWORD dwType,
	const TCHAR *pFormat, 
	...)
{
	va_list ArgList;
	TCHAR	Buffer[MAX_PRINT_CHARS];
	LPF_PRINT lpfPrintFunc;
	LPF_PRINTEX lpfPrintFuncEx;
	BOOL fAddEOL;

	if(!g_fCmnPrint_Initialized)
		return;						// Don't call Initialize() here, because no one may call Cleanup()

	if(dwType > MAX_PRINT_TYPE)
		return;						// Bad type

	va_start (ArgList, pFormat);

	(void)wvsprintf (Buffer, pFormat, ArgList);

	// Get the print function data out of the global array.
	EnterCriticalSection(&g_csCmnPrint);
	lpfPrintFunc = g_rgPF[dwType].lpfPrintFunc;
	lpfPrintFuncEx = g_rgPF[dwType].lpfPrintFuncEx;
	fAddEOL = g_rgPF[dwType].fAddEOL;
	LeaveCriticalSection(&g_csCmnPrint);

	// Add EOL (End-of-Line) char if necessary
	if( (fAddEOL || (!lpfPrintFunc && !lpfPrintFuncEx)) && 
		(MAX_PRINT_CHARS - _tcslen(Buffer) > 2) )
		_tcscat(&Buffer[0], TEXT("\r\n"));

	if(lpfPrintFuncEx)
		lpfPrintFuncEx(TEXT("%s"), &Buffer[0]);
	else if(lpfPrintFunc)
		lpfPrintFunc(&Buffer[0]);
	else
	{
		// If no print function has been registered we don't print anything
		//OutputDebugString(&Buffer[0]);
	}

	va_end(ArgList);
}