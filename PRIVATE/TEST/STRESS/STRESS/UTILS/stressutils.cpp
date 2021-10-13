//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stressutils.h>
#include <stressutilsp.h>
#include <resultsmmf.hxx>


// @doc STRESSUTILS


// Globals
MODULE_PARAMS g_mp;

WORD g_wLogSpace = 0xFFFF;
DWORD g_dwLogZones = 0xFFFFFFFF;

LPCTSTR g_tszModuleName = NULL;

BOOL g_bRandSeeded = FALSE;

HANDLE g_hTerminateSignal = NULL;


///////////////////////////////////////////////////////////
//
// @func	Initializes the stress utilities.
// 
// @rdesc	TRUE if successful, FALSE otherwise.
// 
// @parm	[in] LPCTSTR | tszModuleName | 
//          String containing the name of the module to be
//          used in logging.
// 
// @parm	[in] DWORD | dwDefaultLoggingZones |  
//          Use the LOGZONE() macro to register the logging space you wish to log under
//          (1 only), and the logging zones in that space that you wish to log under (mulltiple allowed).
// 
// @parm	[in] MODULE_PARAMS* | pmp | 
//          Pointer to the module params info obtained from one of the 
//          command line parsing functions: 
//			<f ParseCmdLine_WinMain> or <f ParseCmdLine_wmain>			
// 
// @comm    This function must be called before any CEStress utilities 
//          can be used.
//
// @ex		The following will produce log output for your module only when the GWES space is enabled
//          and either the Window Manager or Dialog Manager zones are turned on.  Each line of logging
//          will be tagged with the string "My Module": |
//			
//			MODULE_PARAMS mp;
//
//			InitializeStressUtils (
//					_T("My Module"),
//					LOGZONE(SLOG_GWES, SLOG_WINMGR | SLOG_DLGMGR),
//					&mp
//					);
//
//

BOOL InitializeStressUtils (
							LPCTSTR tszModuleName,
							DWORD dwDefaultLoggingZones,
							MODULE_PARAMS* pmp
							)
{
	int i;
	int iSpace = -1;
	DWORD dwZones = 0xFFFFFFFF;
	HKEY hKey = NULL;
	LONG lReturn;
	DWORD dwSize;
	TCHAR tszValue[32];


	if (tszModuleName)
	{
		g_tszModuleName = tszModuleName;
	}
	else
	{
		LogInternal (SLOG_WARN1, L"InitializeStressUtils() no module name given");
		g_tszModuleName = TEXT("Unknown Module");
	}


	// If no logging zones are requested log all by default (used by OEM stress)

	if (!dwDefaultLoggingZones)
	{
		g_dwLogZones = 0xFFFF;
		dwZones = 0xFFFF;
	}
	else
	{
		// Validate logging zones

	
		g_wLogSpace = HIWORD(dwDefaultLoggingZones);
		g_dwLogZones = (DWORD) LOWORD(dwDefaultLoggingZones);

		for (i = 0; i < 16; i++)
		{
			if (g_wLogSpace & (((WORD) 0x1) << i))
			{
				if (iSpace != -1)
				{
					LogInternal (SLOG_FAIL, L"InitializeStressUtils() failed!  You may not have more than one logging space");
					return FALSE;
				}

				iSpace = i;
			}
		}

		if (iSpace == -1)
		{
			LogInternal (SLOG_FAIL, L"InitializeStressUtils() failed!  You must declare a single logging space");
			return FALSE;
		}


		// Read logging zones from registry


		if ( ERROR_SUCCESS == (lReturn = RegOpenKeyEx(
														HKEY_CURRENT_USER,
														STRESS_LOGGING_ZONE_PATH,
														0,
														0,
														&hKey)) )
		{
			wsprintf(tszValue, _T("Space %i"), iSpace);

			dwSize = sizeof(DWORD);

			lReturn = RegQueryValueEx (
										hKey,
										tszValue,
										NULL,
										NULL,
										(LPBYTE) &dwZones,
										&dwSize
										);

			if (ERROR_SUCCESS != lReturn)
			{
				LogInternal(SLOG_FAIL, _T("RegQueryValueEx on value %s failed, err = %d, GLE = %d \n"),
									tszValue,
									lReturn,
									GetLastError()
									);
				
				dwZones = 0xFFFFFFFF;
			}

			RegCloseKey(hKey);
		}
		else 
		{
			LogInternal(SLOG_FAIL, _T("Failed to open reg key \"%s\", err = %d \n"), STRESS_LOGGING_ZONE_PATH, lReturn);
			dwZones = 0xFFFFFFFF;
		}
	}


	// Set zone
	
	pmp->dwLoggingZones = dwZones;


	// Get handle to global terminate event

	g_hTerminateSignal = CreateEvent(NULL, TRUE, FALSE, STRESS_MODULE_TERMINATE_EVENT);

	if (!g_hTerminateSignal)
	{
		LogInternal(SLOG_FAIL, _T("Failed to open module terminate event, err = %d \n"), GetLastError());
		return FALSE;
	}

	memcpy((void*) &g_mp, (void*) pmp, sizeof(MODULE_PARAMS));

	return TRUE;
}



///////////////////////////////////////////////////////////
//
// @func	Command line parsing functions for UI based
//          modules that use the WinMain() entry point.
// 
// @rdesc	TRUE if successful, FALSE otherwise.  It this function
//          fails your module should immediately exit.
// 
// @parm	[in] LPTSTR | tszCmdLine |
//          The module command line. 
// 
// @parm	[out] MODULE_PARAMS* | pmp |
//          If this function parses the command line correctly,
//          this structure will be filled with information used 
//          by the module and the stress utilities throughout
//          the module's run. 
// 
// @comm    You should immediately pass the <f MODULE_PARAMS> 
//          structure to <f InitializeStressUtils> before using
//          any of the stress utility functions.
//
// @comm    <nl> NOTE: Modules that can be run on the desktop should
//          be sure to use GetCommandLine( ) to retrieve the 
//          command line in the appropriate char width before
//          passing it to this function.
//

BOOL ParseCmdLine_WinMain (
						   LPTSTR tszCmdLine,
						   MODULE_PARAMS* pmp
						   )
{
#define MAX_CHAR  32
	DWORD	dw;
	int		i;
	LPTSTR  pch;
	TCHAR   rgchar[MAX_CHAR];


	if ( !tszCmdLine || !*tszCmdLine)
	{
		LogFail(L"ParseCmdLine_WinMain() bad cmdline: 0x%x", tszCmdLine);
		return FALSE;
	}

	if ( !pmp )
	{
		LogFail(L"ParseCmdLine_WinMain() NULL module params");
		return FALSE;
	}

	
#ifdef UNDER_NT

    // nt version includes the exe, so we need to step over it
    for (; *tszCmdLine != (TCHAR)' '; tszCmdLine++)
	{
       if (*tszCmdLine == 0)
            return FALSE;
	}

#endif  // NT



	pch = tszCmdLine;
	
	// DURATION
	// LOGGING ZONES

	for (int loop = 0; loop < 3; loop++)
	{
		// step over the space
		while (*pch == (TCHAR)' ')
			pch++;

		if (*pch == (TCHAR)0)
		{
      		// TODO: Handle early NULL
			return FALSE;
		}

		memset((LPVOID) rgchar, 0, MAX_CHAR);

		i = 0;
		while (*pch != (TCHAR)' ' && *pch != (TCHAR)0)
		{
			rgchar[i++] = *pch;
				pch++ ;

			if (i == MAX_CHAR)
				break;
		}

		switch ( loop )
		{
			case 0:
				dw = (DWORD)_ttol(rgchar);

				// Convert duration to milli-secs, prevent overflow

				if (dw >= MODULE_LIFE_MAX_MINUTES)
					pmp->dwDuration = MODULE_LIFE_INDEFINITE;
				else
					pmp->dwDuration = dw * 60 * 1000;
				break;

			case 1:
				dw = (DWORD)_ttol(rgchar);
				
				pmp->uVerbosity = dw;
				break;

			case 2:
				dw = (DWORD)_ttol(rgchar);
				
				pmp->iSlot       = MASK_SLOT(dw);
				pmp->uBreakLevel = MASK_BREAK_LEVEL(dw);
				break;

			default:
				break;
		}
	}

	
	// SERVER 

	// step over the space
	while (*pch == (TCHAR)' ')
		pch++;

	if (*pch == (TCHAR)0)
	{
      	// TODO: Handle early NULL
		return FALSE;
	}

	pmp->tszServer = pch;

	// skip to next word break
	while (*pch != (TCHAR)' ')
	{
		if ( *pch == (TCHAR)0 )
		{
			// TODO: Log error
			return FALSE;
		}

		pch++;
	}

	*pch = 0x0;
	pch++;


	// RESERVED

	// step over space
	while (*pch == (TCHAR)' ')
		pch++;

	if (*pch == (TCHAR)0)
	{
      	// TODO: Handle early NULL
		return FALSE;
	}

	pmp->tszReserved = pch;

	// skip to next word break
	while (*pch != (TCHAR)' ')
	{
		if ( *pch == (TCHAR)0 )
		{
			// TODO: No User Command line
			break;
		}

		pch++;
	}

	if(*pch != (TCHAR)0)
	{
		*pch = 0x0;
		pch++;
	}


	// USER

	// step over space
	while (*pch == (TCHAR)' ')
		pch++;

	if (*pch == (TCHAR)0)
	{
      	pmp->tszUser = NULL;
	}
	else
	{
		pmp->tszUser = pch;
	}

	LogInternal(SLOG_VERBOSE, L"Parsed Cmd line:");
	LogInternal(SLOG_VERBOSE, L"  dwDuration = %d", pmp->dwDuration );
	LogInternal(SLOG_VERBOSE, L"  uVerbosity = %d", pmp->uVerbosity );
	LogInternal(SLOG_VERBOSE, L"  dwInfo = %d, 0x%x", dw, dw);
	LogInternal(SLOG_VERBOSE, L"  tszServer = %s", pmp->tszServer);
	LogInternal(SLOG_VERBOSE, L"  tszReserved = %s", pmp->tszReserved);
	LogInternal(SLOG_VERBOSE, L"  tszUser = %s", pmp->tszUser);

	return TRUE;
}



///////////////////////////////////////////////////////////
//
// @func	Command line parsing function for non-UI based
//          modules that use the wmain() entry point.
// 
// @rdesc	TRUE if successful, FALSE otherwise.  It this function
//          fails your module should immediately exit.
// 
// @parm	[in] int | argc |
//          Number of command line arguments. 
//
// @parm	[in] WCHAR** | argv |
//          Array of command line arguments. 
// 
// @parm	[out] <f MODULE_PARAMS>* | pmp |
//          If this function parses the command line correctly,
//          this structure will be filled with information used 
//          by the module and the stress utilities throughout
//          the module's run. 
// 
// @comm    You should immediately pass the <f MODULE_PARAMS> 
//          structure to <f InitializeStressUtils> before using
//          any of the stress utility functions.
//

BOOL ParseCmdLine_wmain (
						 int argc, 
						 WCHAR** argv, 
						 MODULE_PARAMS* pmp
						 )
{
	return(ParseCmdLine_WinMain(GetCommandLine(), pmp));
}



///////////////////////////////////////////////////////////
//
// @func	Use this function in your module's main test 
//          loop to determine if the module may continue to
//          run or needs to exit.
// 
// @rdesc	TRUE if the module's duration has NOT expired.  
//          Returns FALSE if the module's time is up.  On a 
//          FALSE return the module should clean-up, report 
//          its results (see <f ReportResults>), and exit.
// 
// @parm	[in] DWORD | dwDuration |
//          Number of command line arguments. 
//
// @parm	[in] DWORD | dwStartTime |
//          Array of command line arguments. 
// 

BOOL CheckTime (
				DWORD dwDuration, 
				DWORD dwStartTime
				)
{
	if (WAIT_OBJECT_0 == WaitForSingleObject(g_hTerminateSignal, 0))
		return FALSE;

	if (dwDuration >= MODULE_LIFE_INDEFINITE)
		return TRUE;

	DWORD dwNow = GetTickCount();

	// Check for rollover.

	// We won' worry about a multiple rollovers as any duration
	// over MAX_TIME will be treated as a request for indefinite
	// module lifetime.

	if (dwNow < dwStartTime)
		return (((MAX_TIME - dwStartTime) + dwNow) < dwDuration);
	else
		return ((dwNow - dwStartTime) < dwDuration);
}



///////////////////////////////////////////////////////////
//
// @func    Use this function to tally module results.	
// 
// @parm	[in, out] STRESS_RESULTS | pRes |
//          Pointer to a structure used to tally the module's  
//          total results.
//
// @parm	[in] UINT | ret |
//          The result of a single iteration that is to be 
//          added to the tally.
//
// @comm    A stress module will generally maintain a <f STRESS_RESULTS> 
//          structure with module scope that is used to keep track 
//          of the results of all module iterations.
//

void RecordIterationResults (
							 STRESS_RESULTS* pRes,
							 UINT ret
							 )
{
	if ( !pRes )
	{
		LogFail (L"RecordIterationResult() - NULL results!");
		return;
	}

	pRes->nIterations++;

	switch (ret)
	{
		case CESTRESS_FAIL:
			pRes->nFail++;
			break;

		case CESTRESS_WARN1:
			pRes->nWarn1++;
			break;

		case CESTRESS_WARN2:
			pRes->nWarn2++;
			break;

		default:
			break;
	}
}



///////////////////////////////////////////////////////////
//
// AddResults
//

void AddResults (
				STRESS_RESULTS* pDest,
				STRESS_RESULTS* pSrc
				)
{

	pDest->nIterations += pSrc->nIterations;
	pDest->nFail       += pSrc->nFail;
	pDest->nWarn1      += pSrc->nWarn1;
	pDest->nWarn2      += pSrc->nWarn2;
}




///////////////////////////////////////////////////////////
//
// @func    Use this function to report the results of your
//          modules run to the harness.	
// 
// @parm	[out] STRESS_RESULTS | pRes |
//          Pointer to a structure contianing the tally of   
//          a module's total results (see <f STRESS_RESULTS>).
//
// @comm    This function should only be called once before module
//          exit.  The function writes the module results into a 
//          memory-mapped file that is read by the harness.
//

BOOL ReportResults (
					STRESS_RESULTS* pRes
					)
{
	CResultsMMFile resultsMMFile (RESULTS_MMDATA, 0);

	resultsMMFile.Record (pRes, g_mp.iSlot);

	return TRUE;
}




///////////////////////////////////////////////////////////
//
// @func    This is the core logging function for stress.	
// 
// @parm	[in] DWORD | dwVerbosity |
//          Indicates the verbosity level of this particular
//          log statement. 
//
// @parm	[in] DWORD | dwZone |
//          Indicates the logging zone of this particular
//          log statement. 
//
// @parm	[in] const TCHAR* | tszFormat |
//          Pointer to a null-terminated string that contains the format-control.
//          specifications. In addition to ordinary ASCII characters,
//          a format specification for each argument appears in this string.
//          For more information about the format specification, see the 
//          MSDN descriptionof the printf( ) function. 
//
// @parm	[in] vargs | ... |
//          Specifies one or more optional arguments. The number 
//          and type of argument parameters depend on the corresponding 
//          format-control specifications in the tszFormat parameter.
//
// @comm    Use this function to log if you want to explicitly
//          set the verbosity level and logging zone.
//
//          <nl>
//          <nl> see also:
//          <nl>    <f Log>
//          <nl>    <f LogFail>
//          <nl>    <f LogWarn1>
//          <nl>    <f LogWarn2>
//          <nl>    <f LogComment>
//          <nl>    <f LogVerbose>
//          <nl>    <f LogAll>
//

void LogEx (DWORD dwVerbosity, DWORD dwZones, const TCHAR *tszFormat, ...)
{
	if (dwVerbosity > g_mp.uVerbosity)
		return;
	
	if ((dwVerbosity > SLOG_FAIL) && !(dwZones & g_mp.dwLoggingZones))
		return;

	TCHAR tszDebugBuffer[1024] = _T("");
	TCHAR tszDateBuffer[64]    = _T("");
	TCHAR tszTimeBuffer[64]    = _T("");

	GetDateFormat(
				LOCALE_SYSTEM_DEFAULT, 
				0, 
				NULL,
				_T("M/dd"), 
				tszDateBuffer, 
				64
				);

	GetTimeFormat(
				LOCALE_SYSTEM_DEFAULT, 
				TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT,
                NULL, 
				NULL, 
				tszTimeBuffer, 
				64
				);

	wsprintf(tszDebugBuffer, _T("(%s, %s) %s: "), tszDateBuffer, tszTimeBuffer, g_tszModuleName);

	switch (dwVerbosity)
	{
		case SLOG_FAIL:
			_tcscat(tszDebugBuffer, _T("FAILURE - "));
			break;

		case SLOG_WARN1:
			_tcscat(tszDebugBuffer, _T("WARNING, Level 1 - "));
			break;

		case SLOG_WARN2:
			_tcscat(tszDebugBuffer, _T("WARNING, Level 2 - "));
			break;

		default:
			break;
	}


	va_list vl;
	va_start(vl, tszFormat);
		_vstprintf(tszDebugBuffer + _tcslen(tszDebugBuffer), tszFormat, vl);
	va_end(vl);

	_tcscat(tszDebugBuffer, _T("\r\n"));

#ifndef UNDER_CE
	_tprintf(tszDebugBuffer);
#else
	OutputDebugString(tszDebugBuffer);
#endif

	// TODO: Send to file via ppsh
}




///////////////////////////////////////////////////////////
//
// @func    This logging function uses the default logging zone.	
// 
// @parm	[in] DWORD | dwVerbosity |
//          Indicates the verbosity level of this particular
//          log statement. 
//
// @parm	[in] const TCHAR* | tszFormat |
//          Pointer to a null-terminated string that contains the format-control.
//          specifications. In addition to ordinary ASCII characters,
//          a format specification for each argument appears in this string.
//          For more information about the format specification, see the 
//          MSDN descriptionof the printf( ) function. 
//
// @parm	[in] vargs | ... |
//          Specifies one or more optional arguments. The number 
//          and type of argument parameters depend on the corresponding 
//          format-control specifications in the tszFormat parameter.
//
// @comm    Use this function to log if you want to explicitly set 
//          the verbosity level while using the default logging zone.
//          For more on the default logging zone see <f InitializeStressUtils>.
//
//

void Log (DWORD dwVerbosity, const TCHAR *tszFormat, ...)
{
	TCHAR tszLogBuffer [1024] = _T("");

	va_list vl;
	va_start(vl, tszFormat);
		_vstprintf(tszLogBuffer, tszFormat, vl);
	va_end(vl);
		
	LogEx (dwVerbosity, g_dwLogZones, tszLogBuffer);
}




///////////////////////////////////////////////////////////
//
// @func    This function will write to the log stream regardless
//          of verbosity level or logging zone.	
// 
// @parm	[in] const TCHAR* | tszFormat |
//          Pointer to a null-terminated string that contains the format-control.
//          specifications. In addition to ordinary ASCII characters,
//          a format specification for each argument appears in this string.
//          For more information about the format specification, see the 
//          MSDN descriptionof the printf( ) function. 
//
// @parm	[in] vargs | ... |
//          Specifies one or more optional arguments. The number 
//          and type of argument parameters depend on the corresponding 
//          format-control specifications in the tszFormat parameter.
//
// @comm    Use this sparingly!!
//

void LogAll (const TCHAR *tszFormat, ...)
{
	TCHAR tszLogBuffer [1024] = _T("");

	va_list vl;
	va_start(vl, tszFormat);
		_vstprintf(tszLogBuffer, tszFormat, vl);
	va_end(vl);
		
	LogEx (0, SLOG_ALL, tszLogBuffer);
}



///////////////////////////////////////////////////////////
//
// @func    This function will write to the logging stream
//          using the default logging zone when the verbosity level
//          is set to log failures only.	
// 
// @parm	[in] const TCHAR* | tszFormat |
//          Pointer to a null-terminated string that contains the format-control.
//          specifications. In addition to ordinary ASCII characters,
//          a format specification for each argument appears in this string.
//          For more information about the format specification, see the 
//          MSDN descriptionof the printf( ) function. 
//
// @parm	[in] vargs | ... |
//          Specifies one or more optional arguments. The number 
//          and type of argument parameters depend on the corresponding 
//          format-control specifications in the tszFormat parameter.
//
// @comm    NOTE:  This function will print "FAILURE" in the log line
//          but does not do any record of results.  This is for informational
//          purposes only.
//

void LogFail (const TCHAR *tszFormat, ...)
{
	TCHAR tszLogBuffer [1024] = _T("");

	va_list vl;
	va_start(vl, tszFormat);
		_vstprintf(tszLogBuffer, tszFormat, vl);
	va_end(vl);
		
	LogEx (SLOG_FAIL, SLOG_ALL, tszLogBuffer);
}


///////////////////////////////////////////////////////////
//
// @func    This function will write to the logging stream
//          using the default logging zone when the verbosity level
//          is set to log warning level 1 or lower.	
// 
// @parm	[in] const TCHAR* | tszFormat |
//          Pointer to a null-terminated string that contains the format-control.
//          specifications. In addition to ordinary ASCII characters,
//          a format specification for each argument appears in this string.
//          For more information about the format specification, see the 
//          MSDN descriptionof the printf( ) function. 
//
// @parm	[in] vargs | ... |
//          Specifies one or more optional arguments. The number 
//          and type of argument parameters depend on the corresponding 
//          format-control specifications in the tszFormat parameter.
//
// @comm    NOTE:  This function will print "WARNING 1" in the log line
//          but does not do any record of results.  This is for informational
//          purposes only.
//

void LogWarn1 (const TCHAR *tszFormat, ...)
{
	TCHAR tszLogBuffer [1024] = _T("");

	va_list vl;
	va_start(vl, tszFormat);
		_vstprintf(tszLogBuffer, tszFormat, vl);
	va_end(vl);

	LogEx (SLOG_WARN1, g_dwLogZones, tszLogBuffer);
}



///////////////////////////////////////////////////////////
//
// @func    This function will write to the logging stream
//          using the default logging zone when the verbosity level
//          is set to log warning level 2 or lower.	
// 
// @parm	[in] const TCHAR* | tszFormat |
//          Pointer to a null-terminated string that contains the format-control.
//          specifications. In addition to ordinary ASCII characters,
//          a format specification for each argument appears in this string.
//          For more information about the format specification, see the 
//          MSDN descriptionof the printf( ) function. 
//
// @parm	[in] vargs | ... |
//          Specifies one or more optional arguments. The number 
//          and type of argument parameters depend on the corresponding 
//          format-control specifications in the tszFormat parameter.
//
// @comm    NOTE:  This function will print "WARNING 2" in the log line
//          but does not do any record of results.  This is for informational
//          purposes only.
//

void LogWarn2 (const TCHAR *tszFormat, ...)
{
	TCHAR tszLogBuffer [1024] = _T("");

	va_list vl;
	va_start(vl, tszFormat);
		_vstprintf(tszLogBuffer, tszFormat, vl);
	va_end(vl);

	LogEx (SLOG_WARN2, g_dwLogZones, tszLogBuffer);
}



///////////////////////////////////////////////////////////
//
// @func    This function will write to the logging stream
//          using the default logging zone when the verbosity level
//          is set to comment or lower.	
// 
// @parm	[in] const TCHAR* | tszFormat |
//          Pointer to a null-terminated string that contains the format-control.
//          specifications. In addition to ordinary ASCII characters,
//          a format specification for each argument appears in this string.
//          For more information about the format specification, see the 
//          MSDN descriptionof the printf( ) function. 
//
// @parm	[in] vargs | ... |
//          Specifies one or more optional arguments. The number 
//          and type of argument parameters depend on the corresponding 
//          format-control specifications in the tszFormat parameter.
//
// @comm    Use this function to log general purpose comments that are
//          interesting for debugging purposes.  For example, log module
//          start and exit, the beginning and/or end of significant test 
//          cases, etc.  Do not use this for logging minor details or information 
//          that is not useful in general cases.  For minor details use <f LogVerbose>
//

void LogComment (const TCHAR *tszFormat, ...)
{
	TCHAR tszLogBuffer [1024] = _T("");

	va_list vl;
	va_start(vl, tszFormat);
		_vstprintf(tszLogBuffer, tszFormat, vl);
	va_end(vl);

	LogEx (SLOG_COMMENT, g_dwLogZones, tszLogBuffer);
}


///////////////////////////////////////////////////////////
//
// @func    This function will write to the logging stream
//          using the default logging zone when the verbosity level
//          is set to verbose or lower.	
// 
// @parm	[in] const TCHAR* | tszFormat |
//          Pointer to a null-terminated string that contains the format-control.
//          specifications. In addition to ordinary ASCII characters,
//          a format specification for each argument appears in this string.
//          For more information about the format specification, see the 
//          MSDN descriptionof the printf( ) function. 
//
// @parm	[in] vargs | ... |
//          Specifies one or more optional arguments. The number 
//          and type of argument parameters depend on the corresponding 
//          format-control specifications in the tszFormat parameter.
//
// @comm    Use this function to all minor details that may be of use when
//          debugging specific problems, but that should be filtered out in 
//          normal cases.  For general purpose comments use <f LogComment>.
//

void LogVerbose (const TCHAR *tszFormat, ...)
{
	TCHAR tszLogBuffer [1024] = _T("");

	va_list vl;
	va_start(vl, tszFormat);
		_vstprintf(tszLogBuffer, tszFormat, vl);
	va_end(vl);

	LogEx (SLOG_VERBOSE, g_dwLogZones, tszLogBuffer);
}






///////////////////////////////////////////////////////////
//
// LogInternal
// 

void LogInternal (DWORD dwVerbosity, const TCHAR *tszFormat, ...)
{
//	if (dwVerbosity > g_mp.uHarnessLogLevel)
//		return;

	TCHAR tszDebugBuffer[1024] = _T("");
	TCHAR tszDateBuffer[64]    = _T("");
	TCHAR tszTimeBuffer[64]    = _T("");

	GetDateFormat(
				LOCALE_SYSTEM_DEFAULT, 
				DATE_SHORTDATE, 
				NULL,
				NULL, 
				tszDateBuffer, 
				64
				);

	GetTimeFormat(
				LOCALE_SYSTEM_DEFAULT, 
				TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT,
                NULL, 
				NULL, 
				tszTimeBuffer, 
				64
				);

	wsprintf(tszDebugBuffer, _T("CESTRESS (%s %s): "), tszDateBuffer, tszTimeBuffer);

	va_list vl;
	va_start(vl, tszFormat);
		_vstprintf(tszDebugBuffer + _tcslen(tszDebugBuffer), tszFormat, vl);
	va_end(vl);


#ifndef UNDER_CE
	_tcscat(tszDebugBuffer, _T("\r\n"));
	_tprintf(tszDebugBuffer);
#else
	OutputDebugString(tszDebugBuffer);
#endif

	// TODO: Send to file via ppsh
}




///////////////////////////////////////////////////////////
//
// @func	Returns the number of modules of that calling
//          module's type (i.e. same exe or dll) that are currently
//          running.
// 
// @rdesc	Number of modules of the calling type currently
//          running.
// 
// @parm	[in] HANDLE | hInstance | Handle to your module's instance.
//

LONG GetRunningModuleCount (HANDLE hInst)
{
	TCHAR tsz[MAX_PATH];

	tsz[0] = _T('\0');

	GetModuleFileName((HINSTANCE) hInst, tsz, MAX_PATH);

	if (*tsz == _T('\0'))
		return -1;

	TCHAR* ptsz = tsz;

	TCHAR* pNext = _tcschr(tsz, _T('\\'));
	while (pNext)
	{
		ptsz = pNext + 1;
		pNext = _tcschr(ptsz, _T('\\'));
	}

//	LogInternal(SLOG_COMMENT, _T("GetRunningModuleCount(), module = %s"), ptsz);

	LONG count = 0;
	SetLastError(0);
	HANDLE hSemaphore = CreateSemaphore (
										NULL,
										0,
										0,
										ptsz
										);


	if (!hSemaphore)
	{
		LogFail(_T("GetRunningModuleCount(), Failed to create semaphore (%s). err = %d"), ptsz, GetLastError());
		return -1;
	}

	// Need to increment to get the current count
	// (incrementing by 0 doesn't work)

	if (!ReleaseSemaphore(hSemaphore, 1, &count)) 
	{
		LogFail(_T("GetRunningModuleCount(), ReleaseSemaphore failed! (%s). err = %d"), ptsz, GetLastError());
		return -1;
	}
	
	// Now decrement again to get back to the actual count

	if (WAIT_OBJECT_0 != WaitForSingleObject(hSemaphore, 1))
	{
		LogFail(_T("GetRunningModuleCount(), Failed to decrement semaphore! (%s). err = %d"), ptsz, GetLastError());
		return -1;
	}

//	LogInternal(SLOG_COMMENT, _T("GetRunningModuleCount(), (%s) count = %i"), ptsz, count);
		
	return count;
}



///////////////////////////////////////////////////////////
//
// IncrementRunningModuleCount - Internal
//

LONG IncrementRunningModuleCount (LPCTSTR tszModule)
{
	LONG count = 0;

	LogInternal(SLOG_COMMENT, _T("IncrementRunningModuleCount(), module = %s"), tszModule);

	SetLastError(0);
	HANDLE hSemaphore = CreateSemaphore (
										NULL,
										0,
										32,
										tszModule
										);


	if (!hSemaphore)
	{
		LogFail(_T("IncrementRunningModuleCount(), Failed to create semaphore (%s). err = %d"), tszModule, GetLastError());
		return -1;
	}

	// Need to increment to get the current count
	// (incrementing by 0 doesn't work)

	if (!ReleaseSemaphore(hSemaphore, 1, &count)) 
	{
		LogFail(_T("IncrementRunningModuleCount(), ReleaseSemaphore failed! (%s). err = %d"), tszModule, GetLastError());
		return -1;
	}

//	LogInternal(SLOG_COMMENT, _T("IncrementRunningModuleCount(), (%s) count = %i"), tszModule, count + 1);
		
	return count;
}




///////////////////////////////////////////////////////////
//
// DecrementRunningModuleCount - Internal
//

LONG DecrementRunningModuleCount (LPCTSTR tszModule)
{
	LogInternal(SLOG_COMMENT, _T("DecrementRunningModuleCount(), module = %s"), tszModule);

	SetLastError(0);
	HANDLE hSemaphore = CreateSemaphore (
										NULL,
										0,
										32,
										tszModule
										);


	if (!hSemaphore)
	{
		LogFail(_T("DecrementRunningModuleCount(), Failed to create semaphore (%s). err = %d"), tszModule, GetLastError());
		return -1;
	}

	if (WAIT_OBJECT_0 != WaitForSingleObject(hSemaphore, 1))
	{
		LogFail(_T("DecrementRunningModuleCount(), Failed to decrement semaphore! (%s). err = %d"), tszModule, GetLastError());
		return -1;
	}

//	LogInternal(SLOG_COMMENT, _T("DecrementRunningModuleCount(), (%s)"), tszModule);
		
	return 0;
}




///////////////////////////////////////////////////////////
//
// @func	inline void | RandomSeed |
//			Seeds the random number generator.
// 
// @parm	[in] DWORD | dwSeed | Value used to seed the random 
//			number generator.
//

void RandomSeed(DWORD dwSeed)
{
	srand(dwSeed);
}



///////////////////////////////////////////////////////////
//
// @func	DWORD | GetRandomNumber |
//			Generates a random number.
//
// @rdesc	Random number in the range 0 to dwRandMax.
//
// @parm	[in] DWORD | dwRandMax | 
//			Maximum value for the of the random number to be generated.  
//			To set the lower range use <f GetRandomRange>.
//
// @comm	Uses the Windows CE rand() function.  However, rand() will only 
//			generate numbers between 0x0 and 0x7FFF.  This function allows
//			you to generate numbers between 0x0 and 0xFFFFFFFF.
//

DWORD GetRandomNumber(DWORD dwRandMax)
{
	DWORD dwRand = 0;

	if(dwRandMax == 0)
		return 0;

#ifdef UNDER_CE
      dwRand = Random();
#else

	if(!g_bRandSeeded)
	{
		srand(GetTickCount());
		g_bRandSeeded = TRUE;
	}

#define NT_RAND_BIT_MASK      0x000000FF

	// maximum rand() is 0x7fff, here we make it 0xffffffff
	for(int i = 3; i >= 0; i--)
		dwRand |= ((rand() & NT_RAND_BIT_MASK) << (8 * i));

#endif

	if(dwRandMax == 0xffffffff)
		return( dwRand );
	else
		return( dwRand % (dwRandMax + 1) );
}

 


///////////////////////////////////////////////////////////
//
// @func	DWORD | GetRandomRange |
//			Generates a random number in the given range.
// 
// @rdesc	Random number in the range dwRandMin to dwRandMax.
//
// @parm	[in] DWORD | dwRandMin | 
//			Minimum value for the of the random number to be generated.
//
// @parm	[in] DWORD | dwRandMax | 
//			Maximum value for the of the random number to be generated.
//
// @comm	Uses the Windows CE rand() function.  However, rand() will only 
//			generate numbers between 0x0 and 0x7FFF.  This function allows
//			you to generate numbers between 0x0 and 0xFFFFFFFF.
//

DWORD GetRandomRange(DWORD dwRandMin, DWORD dwRandMax)
{
	if(dwRandMax < dwRandMin)
		return 0;
	return(GetRandomNumber(dwRandMax - dwRandMin) + dwRandMin );
}



///////////////////////////////////////////////////////////
//
// DllMain
//

BOOL WINAPI DllMain(
					HANDLE hInstance, 
					ULONG dwReason, 
					LPVOID lpReserved
					)
{
    return TRUE;
}
