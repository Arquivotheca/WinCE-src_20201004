//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __STRESSUTILS_H__
#define __STRESSUTILS_H__

#include "logging.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


// @doc STRESSUTILS SAMPLESTRESSDLL


// TODO: Doc
#define LOGZONE(space, zone)	MAKELONG(zone, space)

#define STRESS_MODULE_TERMINATE_EVENT	_T("StressModuleTerminateEvent")


// Pass/Fail results

// @enum CESTRESS return value | Return values
enum {
	CESTRESS_PASS = 0,		// @emem Your module's test iteration was successful.
	CESTRESS_FAIL,			// @emem The target of your testing has failed in an unexpected manner.
	CESTRESS_WARN1,			// @emem Used to indicate a problem that is most likely external to the target of your testing, but whose cause is unknown.
							// For example, your test has lost connection to the server which returned an ambiguous error code.  
	CESTRESS_WARN2,			// @emem Used to indicate a minor failure in your module, usually due to temporary, expected external circumstances.  
							// For example, your module connect connect to a server component because the server has reported that it has exceeded
							// it's connection limit.  Or your module cannot access a resource (e.g. a file) with exclusive access rights as it is owned by another process.	  							
	CESTRESS_ABORT			// @emem Returning CESTRESS_ABORT will cause the harness to attempt to unload you module.  
							// Normal shutdown procedures will be followed.
};



//////////////////////////////////////////////////////////
//
// @struct STRESS_RESULTS |
// Structure used to count and record the number of iterations
// run by a stress module and the results of those iterations.
// You may use <f RecordIterationResults> to keep track of results
// and you must use <f ReportResults> to report your results to the
// harness before your module exits.
//
// @comm <nl> NOTE: A test iteration can only produce a single
// result: it either passes, fails, or produces some level of
// warning.
//
typedef struct _STRESS_RESULTS
{
	DWORD	dwModuleID;   // @field (Harness use only) Unique module identifier.
	UINT	nIterations;  // @field Number of iterations run.
	UINT	nFail;        // @field Number of iterations that failed.
	UINT	nWarn1;       // @field Number of iterations that produced level 1 (serious) warnings.
	UINT	nWarn2;       // @field Number of iterations that produced level 2 (minor) warnings.
}
STRESS_RESULTS, *PSTRESS_RESULTS;



//////////////////////////////////////////////////////////
//
// @struct MODULE_PARAMS |
// Structure containing information passed to the module
// by the stress harness on the module's command line.
// Use the <f ParseCmdLine_WinMain> or <f ParseCmdLine_wmain>
// functions to retrieve this info from the command line.
// This struct should then immediately be used to initialize
// the stress utilities using <f InitializeStressUtils>.
//
typedef struct _MODULE_PARAMS
{
	DWORD	dwDuration;         // @field Requested duration of the module.
	DWORD	dwLoggingZones;     // @field Specifies the logging zones enbabled for this run.
	UINT	uVerbosity;         // @field Specifies the logging verbosity level for this run.
	UINT	uHarnessLogLevel;   // @field Specifies the logging level used by the harness.
	UINT	uBreakLevel;        // @field (harness use only) Specifies the level of action to take on failure.
	UINT	iSlot;              // @field (harness use only) Indicates the slot used for reporting pass/fail info.
	LPTSTR	tszServer;          // @field (MacCallan only) The name or address of a test server for the module to communicate with.
	LPTSTR	tszReserved;        // @field (harness use only) Reserved
	LPTSTR	tszUser;            // @field Module specific command line params as read from stressme.ini.
}
MODULE_PARAMS, *PMODULE_PARAMS;



//////////////////////////////////////////////////////////
//
// @struct ITERATION_INFO | 
// Structure containing information about a test thread iteration.
// Passed (via the LPVOID param) to DoStressIteration( ) in DLL 
// modules.
//
typedef struct _ITERATION_INFO
{
	UINT	cbSize;             // @field Size in bytes of this structure.
	int		index;				// @field Zero-based index of the calling thread.  Useful for indexing arrays of per-thread data.
	int		iteration;			// @field The number of this particular iteration (in the context of the calling thread only).
}
ITERATION_INFO, *PITERATION_INFO;




// Init and Command line
BOOL InitializeStressUtils (LPCTSTR tszModuleName, DWORD dwDefaultLoggingZones, MODULE_PARAMS* pmp);
BOOL ParseCmdLine_WinMain (LPTSTR tszCmdLine, MODULE_PARAMS* pmp);
BOOL ParseCmdLine_wmain (int argc, WCHAR** argv, MODULE_PARAMS* pmp);

// Duration
BOOL CheckTime (DWORD dwDuration, DWORD dwStartTime);

// Results reporting
void RecordIterationResults (STRESS_RESULTS* pRes, UINT ret);
BOOL ReportResults (STRESS_RESULTS* pRes);
void AddResults (STRESS_RESULTS* pDest, STRESS_RESULTS* pSrc);

// Logging
void LogEx (DWORD dwLevel, DWORD dwSubzone, const TCHAR *tszFormat, ...);
void Log (DWORD dwLevel, const TCHAR *tszFormat, ...);
void LogFail (const TCHAR *tszFormat, ...);
void LogWarn1 (const TCHAR *tszFormat, ...);
void LogWarn2 (const TCHAR *tszFormat, ...);
void LogComment (const TCHAR *tszFormat, ...);
void LogVerbose (const TCHAR *tszFormat, ...);
void LogAll (const TCHAR *tszFormat, ...);
void LogInternal (DWORD dwLevel, const TCHAR *tszFormat, ...);

// Misc
LONG GetRunningModuleCount (HANDLE hInst);
void RandomSeed (DWORD dwSeed);
DWORD GetRandomNumber (DWORD dwRandMax);
DWORD GetRandomRange (DWORD dwRandMin, DWORD dwRandMax);

// User Command line Parsing
int InitUserCmdLineUtils(LPTSTR tszUserCommandLine, LPTSTR *argv[]);
void User_SetOptionChars (int NumChars, TCHAR *CharArray);
void User_StrictOptionsOnly (BOOL bStrict);
BOOL User_WasOption (LPCTSTR pszOption);
BOOL User_GetOption (LPCTSTR pszOption, LPTSTR *ppszArgument);
BOOL User_GetOptionAsInt (LPCTSTR pszOption, int *piArgument);
BOOL User_GetOptionAsDWORD (LPCTSTR pszOption, DWORD *pdwArgument);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __STRESSUTILS_H__
