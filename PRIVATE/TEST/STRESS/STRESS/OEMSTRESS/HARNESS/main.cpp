//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
________________________________________________________________________________
THIS  CODE AND  INFORMATION IS  PROVIDED "AS IS"  WITHOUT WARRANTY  OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.

Module Name: main.cpp

Abstract: Entry point for the OEM stress harness

Notes: None

________________________________________________________________________________
*/

#include <windows.h>
#include <stressutils.h>
#include <stressutilsp.h>
#include "helpers.h"
#include "logger.h"
#include "stresslist.h"
#include "inifile.h"
#include "cecfg.h"


#define	OEMSTRESS_EVENT				L"OEMSTRESS_EVENT_MAIN"
#define	CESH_PROTOCOL_PREFIX		L"cesh:"
#define	DEFAULT_RESULTS_FILE		L"cesh:result.xml"
#define	DEFAULT_HISTORY_FILE		L"cesh:history.csv"
#define	DEFAULT_INI_FILE			L"cesh:oemstress.ini"
#define	DLLMODULE_LAUNCHER			L"stressmod.exe"


_modulelist_t 		g_ModList;
CRITICAL_SECTION 	g_csModList;
CRITICAL_SECTION 	g_csUpdateResults;
HANDLE 				g_hTerminateEvent = NULL;				// triggers cross process termination of the harness
HANDLE 				g_hModuleTerminate = NULL;				// named event used by stressutils to terminate modules

DWORD 				g_dwTickCountAtStart = 0;
MEMORYSTATUS 		g_msAtStart;
STORE_INFORMATION 	g_siAtStart;

BOOL 				g_fLogHangDetails = false;				// use only Interlocked* functions, shared between stresslist.cpp and main.cpp across multiple threads


namespace Options
{
	//__________________________________________________________________________
	// cmdline switches

	const LPCWSTR SWITCH_RESULTS			= L"-result";
	const LPCWSTR SWITCH_HISTORY			= L"-history";
	const LPCWSTR SWITCH_INI				= L"-ini:";
	const LPCWSTR SWITCH_HANG_TIME			= L"-ht:";
	const LPCWSTR SWITCH_HANG_OPTION_ANY	= L"-ho:any";
	const LPCWSTR SWITCH_HANG_OPTION_ALL	= L"-ho:all";
	const LPCWSTR SWITCH_RUN_TIME			= L"-rt:";
	const LPCWSTR SWITCH_RESULTS_INTERVAL	= L"-ri:";
	const LPCWSTR SWITCH_MODULE_DURATION	= L"-md:";
	const LPCWSTR SWITCH_MODULE_COUNT		= L"-mc:";
	const LPCWSTR SWITCH_RANDOM_SEED		= L"-seed:";
	const LPCWSTR SWITCH_VERBOSITY_LEVEL	= L"-vl";
	const LPCWSTR SWITCH_SERVER				= L"-server:";
	const LPCWSTR SWITCH_TERMINATE			= L"-terminate";
	const LPCWSTR SWITCH_WAIT_MODCOMPLETE	= L"-wait";
	const LPCWSTR SWITCH_AUTOOOM_ENABLE		= L"-autooom";
	const LPCWSTR SWITCH_TRACK_MEMORY		= L"-trackmem:";

	// other constants

	const INT MAX_MODULE_COUNT				= 16;

	/* if g_fModuleDuration is RANDOM & g_nHangTimeoutMinutes is non-zero,
	g_nMaxModuleRuntime is same as g_nHangTimeoutMinutes */

	UINT g_nMaxModuleRuntime				= INT_MAX;

	// options to be read from cmdline

	// if g_fAllModules is true, harness considers hang only if all modules ran for g_nTimeMinutes

	UINT g_nHangTimeoutMinutes 	= 30;					// optional, cmdline, zero meand indefinite			e.g. -ht:hang_time
	BOOL g_fAllModulesHang 		= true;					// optional, cmdline								e.g: -ho:per/all

	UINT g_nRunTimeMinutes 					= 900;		// optional, cmdline, zero means indefinite			e.g. -rt:run_time
	UINT g_nRefreshResultsIntervalMinutes	= 5;		// optional, cmdline	 							e.g. -ri:results_interval

	UINT g_nModuleDurationMinutes 		= 10;			// optional, cmdline, 'random', 'indefinite' or 'minimum'		e.g. -md:module_duration
	MODULE_DURATION g_fModuleDuration	= NORMAL;		// optional, see g_nModuleDurationMinutes comment

	UINT g_nProcessSlots = 4;							// optional, cmdline, zero uses default 			e.g. -mc:module_count
	UINT g_nRandomSeed = GetTickCount();				// optional, cmdline 								e.g. -seed:random_seed

	WCHAR g_szResultFile[MAX_FILE_NAME];				// optional, cmdline 								e.g. -result[:[cesh:]filepath]]
	WCHAR g_szHistoryFile[MAX_FILE_NAME];				// optional, cmdline								e.g. -histoty[:[cesh:]filepath]]
	WCHAR g_szIniFile[MAX_FILE_NAME];					// mandatory, cmdline, otherwise uses default		e.g. -ini[:[cesh:]filepath]]

	WCHAR g_szServer[MAX_FILE_NAME];					// cmdline, otherwise uses default					e.g. -server[:[cesh:]filepath]]

	_shared_t<DWORD> g_Verbosity(L"OEMSTRESS_SHARED");	// cmdline, can be dynamically modified			e.g. -vl:[a|f|w1|w2|c|v]

	BOOL g_fWaitForModuleCompletion = false;			/* cmdline: true value causes the harness to wait
														for completion of currently running module; e.g.
														-wait */

	BOOL g_fAutoOOM					= false;			// enables auto oom
	UINT g_nMemThreshold			= 0;
	BOOL g_fMemLevelPercent			= false;

	// other

	BOOL g_fDebuggerPresent = false;

	// ignored

	DWORD g_dwBreakLevel = 0;							// ignored in OEMStress

	// helper functions

	static BOOL UpdateVerbosityLevel(IN LPCWSTR parg);
	static BOOL ProcessArg(IN LPCWSTR parg);
	static void Dump();
	static void Usage();
};


namespace CETK
{
	HLOCAL g_hMemMissingFiles	= NULL;					// available only when launching from CETK
	static bool CheckModuleDependency(IN LPSTR lpszDependency);
	static bool AddUniqueFileName(IN OUT HLOCAL &hMemFileNames, IN LPWSTR wszFileName);

	/*  CETK special case: The result file should be created in the '\release'
	directory when the '\release' directory is available. At termination,
	OEMStress should copy the result file to the commandline specified location.
	This behaviour is only for filenames specified with undocumented '-f'
	cmdline switch. */

	WCHAR g_szLogFilePathCETK[MAX_FILE_NAME];		// store the requested result file path for future copying (see comment above)
	const LPCWSTR RELEASEDIR = L"\\release";

	inline static bool IsReleaseDirAvailable()
	{
		DWORD dwValue = GetFileAttributes(RELEASEDIR);
		return (FILE_ATTRIBUTE_DIRECTORY & dwValue) ? true : false;
	}
};


namespace Utilities
{
	const INT SPI_AUTO_OOM = 0xFFFF9999;

	static BOOL ModifyAutoOOM(IN BOOL fSet);
	static bool DetectDebugger();
	void BreakAtMemThreshold(IN _result_t *pResult, IN _history_t *pHistoty);
	LPSTR RemoveTrailingWhitespaceANSI(IN OUT LPSTR lpsz);
	bool FileNameFromPath(IN LPCWSTR lpszPath, IN OUT LPWSTR lpszFileName, IN INT nLength);
}

VOID wmain(INT argc, WCHAR** argv)
{
	memset(Options::g_szResultFile, 0, sizeof Options::g_szResultFile);
	memset(Options::g_szHistoryFile, 0, sizeof Options::g_szHistoryFile);

	// default INI filename (not optional)

	memset(Options::g_szIniFile, 0, sizeof Options::g_szIniFile);
	wcscpy(Options::g_szIniFile, DEFAULT_INI_FILE);

	// create the harness termination event

	g_hTerminateEvent = CreateEvent(NULL, TRUE, FALSE, OEMSTRESS_EVENT);

	if (g_hTerminateEvent && ERROR_ALREADY_EXISTS == GetLastError())
	{
		//______________________________________________________________________
		/* there's already one instance of oemstress running (entering command
		or helper mode) */

		// process command line & modify shared variables from the main process

		if (2 == argc)
		{
			// nothing other than SWITCH_TERMINATE or SWITCH_VERBOSITY_LEVEL

			DebugDump(L"OEMSTRESS: Sending termination request to the main instance... \r\n");

			if (!_wcsicmp(argv[1], Options::SWITCH_TERMINATE))
			{
				// termination requested: trigger termination of other instance

				SetEvent(g_hTerminateEvent);
				goto done;
			}
			else
			{
				if (!Options::UpdateVerbosityLevel(argv[1]))
				{
					// verbosity specified in the wrong format

					DebugDump(L"#### OEMSTRESS: Error processing command line argument '%s' (command mode) ####\r\n", argv[1]);
					Options::Usage();
					goto done;
				}
				else
				{
					DebugDump(L"OEMSTRESS: Successfully changed logging verbosity (newly launched modules only)... \r\n");
				}
			}
		}
		else
		{
			DebugDump(L"#### OEMSTRESS: Invalid number of arguments (command mode) ####\r\n");
			Options::Usage();
			goto done;
		}

	done:
		CloseHandle(g_hTerminateEvent);
		return;
	}
	else if (NULL == g_hTerminateEvent)
	{
		DebugDump(L"#### OEMSTRESS: Error 0x%x creating termination event ####\r\n",
			GetLastError());
		return;
	}

	//__________________________________________________________________________
	// create the named event used by stressutils to terminate modules

	g_hModuleTerminate = CreateEvent(NULL, TRUE, FALSE, STRESS_MODULE_TERMINATE_EVENT);

	if (NULL == g_hModuleTerminate)
	{
		DebugDump(L"#### OEMSTRESS: Error 0x%x creating module termination event ####\r\n",
			GetLastError());
		return;
	}

	// verbosity: defaulted to 'comment'

	Options::g_Verbosity.SetValue(SLOG_COMMENT);

	//__________________________________________________________________________
	// **** process command line: normally no option gets set prior to this ****

	bool fXmlLog = false;
	bool fCETK = false;
	WCHAR szLogFileNameCETK[MAX_FILE_NAME];

	memset(szLogFileNameCETK, 0, sizeof szLogFileNameCETK);

	for (INT i = 1; i < argc; i++)
	{
		/* Special Case: CETK supplies only -f switch for the logfilename
		(logfile is always in xml in this case) */

		if (!wcsicmp(argv[i], L"-f"))
		{
			wcscpy(CETK::g_szLogFilePathCETK, argv[++i]);
			if (!Utilities::FileNameFromPath(CETK::g_szLogFilePathCETK, szLogFileNameCETK, MAX_FILE_NAME))
			{
				DebugDump(L"#### OEMSTRESS: Error extracting log filename from '%s' ####\r\n", CETK::g_szLogFilePathCETK);
				return;
			}
			fXmlLog = true;
			fCETK = true;
		}
		else
		{
			if (!Options::ProcessArg(argv[i]))
			{
				DebugDump(L"#### OEMSTRESS: Error processing command line argument '%s' ####\r\n", argv[i]);
				Options::Usage();
				return;
			}
		}
	}

	//__________________________________________________________________________
	// enable auto oom

	if (Options::g_fAutoOOM)
	{
		// Options::g_fAutoOOM contains false if operation fails

		Options::g_fAutoOOM = Utilities::ModifyAutoOOM(true);
	}

	//__________________________________________________________________________
	/* detect results file type based on the filename supplied (exception: CETK
	result is always in XML) */

	if (wcslen(szLogFileNameCETK) && fXmlLog)
	{
		// ignore any other results filename

		/* CETK Special Case: The result file is created in the \release
		directory which points to the FLATRELEASEDIR on the host machine only if
		\release directory is available */

		if (CETK::IsReleaseDirAvailable())
		{
			wcscpy(Options::g_szResultFile, CETK::RELEASEDIR);
			wcscat(Options::g_szResultFile, L"\\");
			wcscat(Options::g_szResultFile, szLogFileNameCETK);
		}
		else
		{
			wcscpy(Options::g_szResultFile, CETK::g_szLogFilePathCETK);
		}
	}
	else
	{
		// parse supplied results filename to see it has xml extension

		LPWSTR pch = &Options::g_szResultFile[wcslen(Options::g_szResultFile)];

		// pch points to the end of the string

		while ((pch != Options::g_szResultFile) && (*pch != L'.'))
		{
			pch--;
		}

		if ((L'.' == *pch) && !wcsicmp(pch, L".xml"))
		{
			fXmlLog = true;
		}
	}

	//__________________________________________________________________________
	/* adjust maximum module runtime here (useful when launching modules for
	indefinite amount of time) */

	if (Options::g_nRunTimeMinutes)
	{
		// maximum module runtime is same as requested harness runtime

		Options::g_nMaxModuleRuntime = Options::g_nRunTimeMinutes;
	}
	else
	{
		// devnote: '0' value in harness runtime causes it to run indefinitely

		Options::g_nMaxModuleRuntime = INT_MAX;
	}

	//__________________________________________________________________________
	/* if RANDOM module duration option is specified, adjust maximum module
	runtime or disable hang detection for INDEFINITE option */

	if (Options::RANDOM == Options::g_fModuleDuration)
	{
		if (Options::g_nHangTimeoutMinutes)
		{
			Options::g_nMaxModuleRuntime = Options::g_nHangTimeoutMinutes;
		}

		// disable harness wait for module completion

		Options::g_fWaitForModuleCompletion = false;
	}
	else if (Options::INDEFINITE == Options::g_fModuleDuration)
	{
		// disable hang detection

		Options::g_nHangTimeoutMinutes = false;

		// disable harness wait for module completion

		Options::g_fWaitForModuleCompletion = false;
	}
	else
	{
		// harness runtime is less than module duration: adjust module duration

		if (Options::g_nRunTimeMinutes < Options::g_nModuleDurationMinutes)
		{
			Options::g_nModuleDurationMinutes = Options::g_nRunTimeMinutes;
		}
	}

	if (!Options::g_nHangTimeoutMinutes)
	{
		// reset log-hang-details flag

		InterlockedExchange((LPLONG)&g_fLogHangDetails, false);
	}

	// parse 'cesh:' prefix from filename and create appropriate file objects

	_history_t *pHistoty = NULL;
	_result_t *pResult = NULL;
	_inifile_t *pINI = NULL;

	// results file (optional)

	if (*Options::g_szResultFile)
	{
		IFile *pResultFileObj = NULL;

		if (Options::g_szResultFile == wcsstr(Options::g_szResultFile, CESH_PROTOCOL_PREFIX))
		{
			pResultFileObj = new _ceshfile_t(
				&Options::g_szResultFile[wcslen(CESH_PROTOCOL_PREFIX)],
				U_O_WRONLY | U_O_CREAT | U_O_APPEND | U_O_TRUNC);
		}
		else
		{
			// write & truncate (binary)

			pResultFileObj = new _file_t(Options::g_szResultFile, L"wb");
		}

		if (pResultFileObj)
		{
			pResult = new _result_t(pResultFileObj, fXmlLog);
		}

		if (!pResultFileObj || !pResult)
		{
			DebugDump(L"#### OEMSTRESS: Error Creating Results File ####\r\n");
			goto cleanup;
		}

		pResultFileObj = NULL;	// no further use
	}
	else
	{
		// result information in the debug window is always enabled

		pResult = new _result_t(NULL, FALSE);
	}

	// history file (optional)

	if (*Options::g_szHistoryFile)
	{
		IFile *pHistoryFileObj = NULL;

		if (Options::g_szHistoryFile == wcsstr(Options::g_szHistoryFile, CESH_PROTOCOL_PREFIX))
		{
			pHistoryFileObj = new _ceshfile_t(
				&Options::g_szHistoryFile[wcslen(CESH_PROTOCOL_PREFIX)],
				U_O_WRONLY | U_O_CREAT | U_O_APPEND);
		}
		else
		{
			// default: create/append (binary)

			pHistoryFileObj = new _file_t(Options::g_szHistoryFile);
		}

		if (pHistoryFileObj)
		{
			pHistoty = new _history_t(pHistoryFileObj);
		}

		if (!pHistoryFileObj || !pHistoty)
		{
			DebugDump(L"#### OEMSTRESS: Error Creating History File ####\r\n");
			goto cleanup;
		}

		pHistoryFileObj = NULL;	// no further use
	}

	// initialization file (mandatory)

	if (*Options::g_szIniFile)
	{
		IFile *pIniFileObj = NULL;

		if (Options::g_szIniFile == wcsstr(Options::g_szIniFile, CESH_PROTOCOL_PREFIX))
		{
			pIniFileObj = new _ceshfile_t(&Options::g_szIniFile[wcslen(CESH_PROTOCOL_PREFIX)], U_O_RDONLY);
		}
		else
		{
			// reading in binary is important (readline fails otherwise)

			pIniFileObj = new _file_t(Options::g_szIniFile, L"rb");
		}

		if (pIniFileObj)
		{
			pINI = new _inifile_t(pIniFileObj);
		}

		if (!pIniFileObj || !pINI)
		{
			DebugDump(L"#### OEMSTRESS: Error opening initialization file ####\r\n");
			goto cleanup;
		}

		pIniFileObj = NULL;
	}
	else
	{
		DebugDump(L"#### OEMSTRESS: Fatal Error - Missing initialization file, terminating... ####\r\n");
		goto cleanup;
	}

	/* initialize all critical sections necessary to access global data (before
	any thread is launched) */

	InitializeCriticalSection(&g_csModList);
	InitializeCriticalSection(&g_csUpdateResults);

	// detect attached debugger

	Options::g_fDebuggerPresent = Utilities::DetectDebugger();

	// initialize random seed

	srand(Options::g_nRandomSeed);

	LPSTR pszModuleName;
	LPSTR pszCmdLine;
	LPSTR pszDependency;
	LPSTR pszOSComponents;
	UINT nMinRunTimeMinutes;

	pszModuleName = pszCmdLine = pszDependency = pszOSComponents = NULL;
	nMinRunTimeMinutes = 0;

	// read initialization file(s) and populate module list

	bool fModulesAvailable;
	fModulesAvailable = false;

	// OS component detection

	IFile *pCfgFile;
	pCfgFile = new _file_t(L"\\windows\\ceconfig.h", L"rb");

	if (!pCfgFile)
	{
		DebugDump(L"#### OEMSTRESS: Error accessing config file containing OS component information ####\r\n");
		goto cleanup;
	}

	_ceconfig_t *pCfg;
	pCfg = new _ceconfig_t(pCfgFile);

	if (!pCfg->_fCeCfgFileLoaded)
	{
		DebugDump(L"#### OEMSTRESS: Error loading config file containing OS component information ####\r\n");
		goto cleanup;
	}


	do
	{
		pszModuleName = NULL;
		pszCmdLine = NULL;

		HANDLE hMemOSComponents = NULL;
		HANDLE hMemDependency = NULL;

		fModulesAvailable = pINI->NextModuleDetails(pszModuleName, pszCmdLine, nMinRunTimeMinutes, hMemDependency, hMemOSComponents);

		if (fModulesAvailable)
		{
			pszDependency = NULL;

			if (hMemDependency)
			{
				pszDependency = (char *)LocalLock(hMemDependency);
			}

			// add the module to the list if the OS components is left empty

			pszOSComponents = NULL;

			if (hMemOSComponents)
			{
				pszOSComponents	= (char *)LocalLock(hMemOSComponents);
			}

			if (!pszOSComponents || !*pszOSComponents || pCfg->IsRunnable(pszOSComponents))
			{
				MODULE_DETAIL md;

				// perform file dependency check only if running from CETK

				if (fCETK)
				{
					if (!CETK::CheckModuleDependency(pszDependency))
					{
						LocalUnlock(hMemDependency);
						if (hMemDependency)
						{
							LocalFree(hMemDependency);
							hMemDependency = NULL;
						}


						LocalUnlock(hMemOSComponents);
						if (hMemOSComponents)
						{
							LocalFree(hMemOSComponents);
							hMemOSComponents = NULL;
						}
						continue;
					}
				}

				// do not store pszDependency and pszOSComponents in the global modlist

				if (!md.Init(pszModuleName, pszCmdLine, nMinRunTimeMinutes))
				{
					DebugDump(L"#### OEMSTRESS: Unable to allocate memory while reading module information ####\r\n");
					LocalUnlock(hMemDependency);
					LocalUnlock(hMemOSComponents);
					goto cleanup;
				}

				EnterCriticalSection(&g_csModList);
				g_ModList.Add(&md);
				LeaveCriticalSection(&g_csModList);
			}
			else
			{
				WCHAR szModuleName[MAX_PATH + 1];
				memset(szModuleName, 0, sizeof szModuleName);
				mbstowcs(szModuleName, pszModuleName, strlen(pszModuleName));
				DebugDump(L"OEMSTRESS: Module %s will not be launched...\r\n", szModuleName);
			}

			LocalUnlock(hMemDependency);
			if (hMemDependency)
			{
				LocalFree(hMemDependency);
				hMemDependency = NULL;
			}


			LocalUnlock(hMemOSComponents);
			if (hMemOSComponents)
			{
				LocalFree(hMemOSComponents);
				hMemOSComponents = NULL;
			}
		}

	}
	while (fModulesAvailable);

	if (!g_ModList.Length())
	{
		DebugDump(L"#### OEMSTRESS: Empty module list - No module will get launched ####\r\n");
		pResult->LogFailure(true);
		goto cleanup;
	}
	else if (fCETK && CETK::g_hMemMissingFiles)
	{
		// CETK only: there must be some missing files

		pResult->LogFailure(false);
		goto cleanup;
	}

	if (pCfg)
	{
		delete pCfg;
		pCfg = NULL;
	}

	// result refresh interval should not be more than runtime

	if (Options::g_nRefreshResultsIntervalMinutes > Options::g_nRunTimeMinutes)
	{
		Options::g_nRefreshResultsIntervalMinutes = Options::g_nRunTimeMinutes;
	}

	// print startup options in the debug output

	Options::Dump();

	// record system details: store memory & storage status at startup

	g_msAtStart.dwLength = sizeof MEMORYSTATUS;
	GlobalMemoryStatus(&g_msAtStart);
	GetStoreInformation(&g_siAtStart);

	// todo: Memory adjustments etc.

	// store startup tickcount

	g_dwTickCountAtStart = GetTickCount();

	// initialize runlist

	if (!_runlist_t::Init(Options::g_nProcessSlots, pResult))
	{
		DebugDump(L"#### OEMSTRESS: Unable to initialize runlist ####\r\n");
		goto cleanup;
	}

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

	while (true)
	{
		if (Options::g_nMemThreshold)
		{
			Utilities::BreakAtMemThreshold(pResult, pHistoty);
		}

		/* termination event might get triggered from other process: terminate
		the loop in that case */

		if (WAIT_OBJECT_0 == WaitForSingleObject(g_hTerminateEvent, Options::g_nRefreshResultsIntervalMinutes * MINUTE))
		{
			DebugDump(L"OEMSTRESS: Termination requested, triggering module termination...\r\n");

			SetEvent(g_hModuleTerminate);

			_runlist_t::LaunchNewModule() = false;	// stop launching new modules

			Sleep(MINUTE);	// wait for a minute to allow the modules to terminate

			if (pHistoty)
			{
				pHistoty->LogResults(&g_csUpdateResults);
			}

			if (pResult)
			{
				pResult->LogResults(&g_csUpdateResults,
					(BOOL)InterlockedExchange((LPLONG)&g_fLogHangDetails, (LONG)g_fLogHangDetails));
			}

			break;
		}

		if (pHistoty)
		{
			pHistoty->LogResults(&g_csUpdateResults);
		}

		if (pResult)
		{
			pResult->LogResults(&g_csUpdateResults,
				(BOOL)InterlockedExchange((LPLONG)&g_fLogHangDetails, (LONG)g_fLogHangDetails));
		}

		if ((UINT)(GetTickCount() - g_dwTickCountAtStart) > (Options::g_nRunTimeMinutes * MINUTE))
		{
			DebugDump(L"OEMSTRESS: Ran for requested duration, triggering module termination...\r\n");

			SetEvent(g_hModuleTerminate);

			_runlist_t::LaunchNewModule() = false;	// stop launching new modules

			if (Options::g_fWaitForModuleCompletion)
			{
				DebugDump(L"OEMSTRESS: Waiting for running modules to exit...\r\n");

				while (true)
				{
					// no running processes exist

					if (!_runlist_t::RunningModules())
					{
						SetEvent(g_hTerminateEvent);

						if (pHistoty)
						{
							pHistoty->LogResults(&g_csUpdateResults);
						}

						if (pResult)
						{
							pResult->LogResults(&g_csUpdateResults,
								(BOOL)InterlockedExchange((LPLONG)&g_fLogHangDetails, (LONG)g_fLogHangDetails),
								true);
						}

						DebugDump(L"OEMSTRESS: Completed, all modules exited successfully\r\n");

						goto cleanup;
					}

					if (Options::g_nMemThreshold)
					{
						Utilities::BreakAtMemThreshold(pResult, pHistoty);
					}

					Sleep(MINUTE);

					if (pHistoty)
					{
						pHistoty->LogResults(&g_csUpdateResults);
					}

					if (pResult)
					{
						pResult->LogResults(&g_csUpdateResults,
							(BOOL)InterlockedExchange((LPLONG)&g_fLogHangDetails, (LONG)g_fLogHangDetails));
					}
				}
			}
			else
			{
				Sleep(MINUTE);	// wait for a minute to allow the modules to terminate

				SetEvent(g_hTerminateEvent);

				if (pHistoty)
				{
					pHistoty->LogResults(&g_csUpdateResults);
				}

				if (pResult)
				{
					pResult->LogResults(&g_csUpdateResults,
						(BOOL)InterlockedExchange((LPLONG)&g_fLogHangDetails, (LONG)g_fLogHangDetails),
						true);
				}

				DebugDump(L"OEMSTRESS: Completed successfully\r\n");

				break;
			}
		}
	}

cleanup:
	DebugDump(L"OEMSTRESS: Cleaning up...\r\n");
	_runlist_t::Uninit();														// must call _runlist_t::Init before using RunList again
	DeleteCriticalSection(&g_csUpdateResults);
	DeleteCriticalSection(&g_csModList);

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

	if (pHistoty)
	{
		delete pHistoty;
	}

	if (pResult)
	{
		delete pResult;
	}

	if (pINI)
	{
		delete pINI;
	}

	if (pCfg)
	{
		delete pCfg;
	}

	if (g_hModuleTerminate)
	{
		ResetEvent(g_hModuleTerminate);
		CloseHandle(g_hModuleTerminate);
	}

	if (g_hTerminateEvent)
	{
		CloseHandle(g_hTerminateEvent);
	}

	if (CETK::g_hMemMissingFiles)
	{
		LocalFree(CETK::g_hMemMissingFiles);
		CETK::g_hMemMissingFiles = NULL;
	}

	/* CETK Special Case: The result file is created in the \release directory
	which points to the FLATRELEASEDIR on the host machine only if \release
	directory is available */

	if (fCETK && CETK::IsReleaseDirAvailable())
	{
		/* CETK Special Case: Copy the result file to the cmdline specified
		location from the FLATRELEASEDIR */

		SetLastError(0);
		if (!CopyFile(Options::g_szResultFile, CETK::g_szLogFilePathCETK, FALSE))
		{
			DebugDump(L"#### OEMSTRESS: Error 0x%x copying %s to %s ####\r\n",
				GetLastError(), Options::g_szResultFile, CETK::g_szLogFilePathCETK);
		}
	}

	DebugDump(L"OEMSTRESS: Exiting\r\n");
	return;
}



/*

@func:	CreateModuleCommandLine, Formats a command line as per the stress
harness specs to launch a module

@rdesc:	true if successful, false otherwise

@param:	IN PMODULE_DETAIL pmd: Module details

@param:	OUT LPWSTR szCmdLine: Formatted command line

@param:	IN DWORD dwSlot: Slot ID in the runlist where the module state is
maintained
during the run

@param:	IN INT nDurationMinutes: Number of minutes the module is requested
to run

@fault:	None

@pre:	All parameters should be valid as there's no check done inside.

@post:	None

@note:	None

*/

static BOOL CreateModuleCommandLine(IN PMODULE_DETAIL pmd, OUT LPWSTR szCmdLine, IN DWORD dwSlot, IN INT nDurationMinutes)
{
	DebugDump(L"CreateModuleCommandLine: Original Command line: %s\n", pmd->pCommandLine);

	INT nLen = wcslen(pmd->pCommandLine);
	nLen += CMDLINE_HARNESS_RESERVED; // For params added by harness

	if (nLen >= MAX_CMDLINE)
	{
		DebugDump(L"#### CreateModuleCommandLine: Command line length exceeded by %i ####\r\n",
			(MAX_CMDLINE - nLen));
		return FALSE;
	}

	// argv[1]  duration
	// argv[2]  logging zones
	// argv[3]  additional info
	// argv[4]  server name
	// argv[5]  reserved
	// argv[6]  user

	DWORD dwAddInfo = 0;
	dwAddInfo = ADD_SLOT(dwAddInfo, dwSlot);
	dwAddInfo = ADD_BREAK_LEVEL(dwAddInfo, Options::g_dwBreakLevel);

	wsprintf(
		szCmdLine,
		L"%u %u %u %s %s %s",
		nDurationMinutes,
		Options::g_Verbosity.GetValue(),
		dwAddInfo,
		(*Options::g_szServer) ? Options::g_szServer : L"localhost",
		L"reserved",
		pmd->pCommandLine
	);

	DebugDump(L"CreateModuleCommandLine: %s\r\n", szCmdLine);

	return TRUE;
}


/*

@func:	CreateRandomProcess, Launches a rando process from the module list

@rdesc:	Handle to the launched process if successful, NULL otherwise

@param:	OUT PMODULE_DETAIL *ppmd: Node pointer to the module launched in the
global module list

@param:	IN OUT LPDWORD lpdwTickCountAtLaunch: Precise moment when the module is
launched

@param:	IN INT iSlot: Slot ID in the runlist where the module state is to be
maintained during the run

@fault:	None

@pre:	g_ModList must have, at least, one module entry

@post:	None

@note:	If the stress module is a DLL, a helper launcher is used instead

*/

HANDLE CreateRandomProcess(OUT PMODULE_DETAIL *ppmd, IN OUT LPDWORD lpdwTickCountAtLaunch, IN INT iSlot)
{
	PROCESS_INFORMATION pi;
	HANDLE hProcess = NULL;
	MODULE_DETAIL *pmd = NULL;
	TCHAR szCmdLine[MAX_CMDLINE];
	INT nRunTime = 0;

	// todo: OOM check

	memset(&pi, 0, sizeof pi);

	pmd = (PMODULE_DETAIL)g_ModList.RandomNode();

	// todo: include random seed in the command line

	if (Options::RANDOM == Options::g_fModuleDuration)
	{
		if (Options::g_nMaxModuleRuntime < pmd->nMinRunTimeMinutes)
		{
			return NULL;
		}

		nRunTime = RANDOM_INT(Options::g_nMaxModuleRuntime, pmd->nMinRunTimeMinutes);
	}
	else if (Options::MINIMUM == Options::g_fModuleDuration)
	{
		nRunTime = pmd->nMinRunTimeMinutes;
	}
	else if (Options::INDEFINITE == Options::g_fModuleDuration)
	{
		if (Options::g_nMaxModuleRuntime < pmd->nMinRunTimeMinutes)
		{
			return NULL;
		}

		nRunTime = Options::g_nMaxModuleRuntime;
	}
	else
	{
		if (Options::g_nModuleDurationMinutes)
		{
			if (Options::g_nModuleDurationMinutes > pmd->nMinRunTimeMinutes)
			{
				nRunTime = Options::g_nModuleDurationMinutes;
			}
			else
			{
				nRunTime = pmd->nMinRunTimeMinutes;
			}
		}
	}

	// detect DLL module

	LPWSTR pch = &pmd->pModuleName[wcslen(pmd->pModuleName)];

	// pch points to the end of the string

	BOOL fDllStressModule = false;

	while ((pch != pmd->pModuleName) && (*pch != L'.'))
	{
		pch--;
	}

	if ((L'.' == *pch) && !wcsicmp(pch, L".dll"))
	{
		fDllStressModule = true;
	}

	// modulename should be part of the cmdline in case of dll modules

	if (fDllStressModule)
	{
		wcscpy(szCmdLine, pmd->pModuleName);
		wcscat(szCmdLine, L" ");
	}

	if (!CreateModuleCommandLine(pmd, &szCmdLine[wcslen(szCmdLine)], iSlot, nRunTime))
	{
		DebugDump(L"#### OEMSTRESS::CreateRandomProcess: Command line generation failed ####\r\n");
		return NULL;
	}

	DebugDump(L"OEMSTRESS::CreateRandomProcess: [%i] Launching %s for %u minute(s)...\r\n",
		iSlot, pmd->pModuleName, nRunTime);

	// use a helper launcher application to launch dll module

	SetLastError(0);
	if (CreateProcess(fDllStressModule ? DLLMODULE_LAUNCHER : pmd->pModuleName, szCmdLine, 0, 0, FALSE, 0, 0, 0, 0, &pi))
	{
		*ppmd = pmd;
		pmd->nLaunchCount++;
		pmd->dwTickCountLastLaunched = GetTickCount();
		*lpdwTickCountAtLaunch = pmd->dwTickCountLastLaunched;

		if (!pmd->dwTickCountFirstLaunched)
		{
			pmd->dwTickCountFirstLaunched = pmd->dwTickCountLastLaunched;
		}

		hProcess = pi.hProcess;

		/* following line helps stressutils determine how many instances of a
		particular module is running */

		IncrementRunningModuleCount(pmd->pModuleName);
	}
	else
	{
		hProcess = NULL;
	}

	return hProcess;
}


/*

@func:	DebugDump, OutputDebugString with formatting

@rdesc:	void

@param:	[in] const wchar_t *pszFormat, ...

@fault:	None

@pre:	None

@post:	None

@note:	None

*/

void DebugDump(const wchar_t *pszFormat, ...)
{
	va_list arglist;

	const DUMP_BUFFER = 0x400;
	WCHAR szBuffer[DUMP_BUFFER];

	if (!pszFormat || !*pszFormat)
	{
		return;
	}

	memset(szBuffer, 0, sizeof szBuffer);

	va_start(arglist, pszFormat);
	wvsprintf(szBuffer, pszFormat, arglist);
	OutputDebugString(szBuffer);
	va_end(arglist);

	return;
}


/*

@func:	Options.UpdateVerbosityLevel, Updates global (cross-process shared)
variable holding verbosity information after processing passed cmdline argument

@rdesc:	true if successful, false otherwise

@param:	IN LPCWSTR parg: Command line argument to extract verbosity information
from

@fault:	None

@pre:	parg must point to a valid zero terminated string

@post:	Cross-process shared variable, Options::g_Verbosity gets updated if the
passed argument is in valid format

@note:	None

*/

static BOOL Options::UpdateVerbosityLevel(IN LPCWSTR parg)
{
	BOOL fOk = false;

	if (!wcsnicmp(parg, SWITCH_VERBOSITY_LEVEL, wcslen(SWITCH_VERBOSITY_LEVEL)))
	{
		if (parg[wcslen(SWITCH_VERBOSITY_LEVEL)] == L':')
		{
			INT ich = wcslen(SWITCH_VERBOSITY_LEVEL) + 1;

			if (!wcsicmp(&parg[ich], L"abort") || !wcsicmp(&parg[ich], L"a"))
			{
				g_Verbosity.SetValue(SLOG_ABORT);
				fOk = true;
				goto done;
			}
			else if (!wcsicmp(&parg[ich], L"fail") || !wcsicmp(&parg[ich], L"f"))
			{
				g_Verbosity.SetValue(SLOG_FAIL);
				fOk = true;
				goto done;
			}
			else if (!wcsicmp(&parg[ich], L"warn1") || !wcsicmp(&parg[ich], L"warning1") ||
				!wcsicmp(&parg[ich], L"w1"))
			{
				g_Verbosity.SetValue(SLOG_WARN1);
				fOk = true;
				goto done;
			}
			else if (!wcsicmp(&parg[ich], L"warn2") || !wcsicmp(&parg[ich], L"warning2") ||
				!wcsicmp(&parg[ich], L"w2"))
			{
				g_Verbosity.SetValue(SLOG_WARN2);
				fOk = true;
				goto done;
			}
			else if (!wcsicmp(&parg[ich], L"comment") || !wcsicmp(&parg[ich], L"c"))
			{
				g_Verbosity.SetValue(SLOG_COMMENT);
				fOk = true;
				goto done;
			}
			else if (!wcsicmp(&parg[ich], L"verbose") || !wcsicmp(&parg[ich], L"v"))
			{
				g_Verbosity.SetValue(SLOG_VERBOSE);
				fOk = true;
				goto done;
			}
			else
			{
				fOk = false;
				goto done;
			}
		}
		else	// defaulted to SLOG_ALL
		{
			g_Verbosity.SetValue(SLOG_ALL);
			fOk = true;
			goto done;
		}
	}

done:
	return fOk;
}


/*

@func:	Options::ProcessArg, Processes passed cmdline argument and updated
global options variables

@rdesc:	true if successful, false otherwise

@param:	IN LPCWSTR parg: Command line argument to extract options information
from

@fault:	None

@pre:	parg must point to a valid zero terminated string

@post:	Global variables belonging to 'Options' namespace gets updated

@note:	None

*/

static BOOL Options::ProcessArg(IN LPCWSTR parg)
{
	BOOL fOk = false;

	if (!parg || !*parg)
	{
		return false;
	}

	// results file

	if (!wcsnicmp(parg, SWITCH_RESULTS, wcslen(SWITCH_RESULTS)))
	{
		// enable results

		if (parg[wcslen(SWITCH_RESULTS)] && (parg[wcslen(SWITCH_RESULTS)] == L':') &&
			parg[wcslen(SWITCH_RESULTS) + 1])
		{
			// results filename supplied

			wcscpy(g_szResultFile, &parg[wcslen(SWITCH_RESULTS) + 1]);
			fOk = true;
		}
		else if (!parg[wcslen(SWITCH_RESULTS)])
		{
			// use default results filename

			wcscpy(g_szResultFile, DEFAULT_RESULTS_FILE);
			fOk = true;
		}
		else
		{
			fOk = false;
		}
	}

	// history file

	else if (!wcsnicmp(parg, SWITCH_HISTORY, wcslen(SWITCH_HISTORY)))
	{
		// enable history

		if (parg[wcslen(SWITCH_HISTORY)] && (parg[wcslen(SWITCH_HISTORY)] == L':') &&
			parg[wcslen(SWITCH_HISTORY) + 1])
		{
			// history filename supplied

			wcscpy(g_szHistoryFile, &parg[wcslen(SWITCH_HISTORY) + 1]);
			fOk = true;
		}
		else if (!parg[wcslen(SWITCH_HISTORY)])
		{
			// use default hiatory filename

			wcscpy(g_szHistoryFile, DEFAULT_HISTORY_FILE);
			fOk = true;
		}
		else
		{
			fOk = false;
		}
	}

	// INI file

	else if (!wcsnicmp(parg, SWITCH_INI, wcslen(SWITCH_INI)))
	{
		// non-defailt INI file

		if (parg[wcslen(SWITCH_INI)])
		{
			wcscpy(g_szIniFile, &parg[wcslen(SWITCH_INI)]);
			fOk = true;
		}
		else
		{
			fOk = false;
		}
	}

	// verbosity level

	else if (!wcsnicmp(parg, SWITCH_VERBOSITY_LEVEL, wcslen(SWITCH_VERBOSITY_LEVEL)))
	{
		if (!UpdateVerbosityLevel(parg))
		{
			fOk = false;
		}
		else
		{
			fOk = true;
		}
	}

	// random seed

	else if (!wcsnicmp(parg, SWITCH_RANDOM_SEED, wcslen(SWITCH_RANDOM_SEED)))
	{
		g_nRandomSeed = _wtoi(&parg[wcslen(SWITCH_RANDOM_SEED)]);
		fOk = true;
	}

	// refresh results interval

	else if (!wcsnicmp(parg, SWITCH_RESULTS_INTERVAL, wcslen(SWITCH_RESULTS_INTERVAL)))
	{
		UINT nRefreshResultsIntervalMinutes = _wtoi(&parg[wcslen(SWITCH_RESULTS_INTERVAL)]);

		if (nRefreshResultsIntervalMinutes)
		{
			g_nRefreshResultsIntervalMinutes = nRefreshResultsIntervalMinutes;
			fOk = true;
		}
		else
		{
			fOk = false;
		}
	}

	// module duration

	else if (!wcsnicmp(parg, SWITCH_MODULE_DURATION, wcslen(SWITCH_MODULE_DURATION)))
	{
		if (!wcsicmp(&parg[wcslen(SWITCH_MODULE_DURATION)], L"random"))
		{
			// launch module for random amount of time

			g_fModuleDuration = RANDOM;
			fOk = true;
		}
		else if (!wcsicmp(&parg[wcslen(SWITCH_MODULE_DURATION)], L"indefinite"))
		{
			// launch module for indefinite amount of time

			g_fModuleDuration = INDEFINITE;
			fOk = true;
		}
		else if (!wcsicmp(&parg[wcslen(SWITCH_MODULE_DURATION)], L"minimum"))
		{
			// launch module for minimum amount of time

			g_fModuleDuration = MINIMUM;
			fOk = true;
		}
		else
		{
			UINT nModuleDurationMinutes = _wtoi(&parg[wcslen(SWITCH_MODULE_DURATION)]);

			if (nModuleDurationMinutes)
			{
				g_nModuleDurationMinutes = nModuleDurationMinutes;
				g_fModuleDuration = NORMAL;
				fOk = true;
			}
			else
			{
				fOk = false;
			}
		}
	}

	// module count

	else if (!wcsnicmp(parg, SWITCH_MODULE_COUNT, wcslen(SWITCH_MODULE_COUNT)))
	{
		UINT nModuleCount = _wtoi(&parg[wcslen(SWITCH_MODULE_COUNT)]);

		if (nModuleCount && (nModuleCount <= MAX_MODULE_COUNT))
		{
			g_nProcessSlots = nModuleCount;
			fOk = true;
		}
		else
		{
			fOk = false;
		}
	}

	// run time

	else if (!wcsnicmp(parg, SWITCH_RUN_TIME, wcslen(SWITCH_RUN_TIME)))
	{
		UINT nRunTimeMinutes = _wtoi(&parg[wcslen(SWITCH_RUN_TIME)]);

		if (nRunTimeMinutes)
		{
			g_nRunTimeMinutes = nRunTimeMinutes;
			fOk = true;
		}
		else
		{
			g_nRunTimeMinutes = UINT_MAX;
			fOk = true;
		}
	}

	// hang time

	else if (!wcsnicmp(parg, SWITCH_HANG_TIME, wcslen(SWITCH_HANG_TIME)))
	{
		UINT nHangTimeMinutes = _wtoi(&parg[wcslen(SWITCH_HANG_TIME)]);

		if (nHangTimeMinutes)
		{
			g_nHangTimeoutMinutes = nHangTimeMinutes;
			fOk = true;
		}
		else
		{
			fOk = false;
		}
	}

	// hang module detection option

	else if (!wcsnicmp(parg, SWITCH_HANG_OPTION_ANY, wcslen(SWITCH_HANG_OPTION_ANY)))
	{
		// hang option (any-module)

		g_fAllModulesHang = false;
		fOk = true;
	}
	else if (!wcsnicmp(parg, SWITCH_HANG_OPTION_ALL, wcslen(SWITCH_HANG_OPTION_ALL)))
	{
		// hang option (all-modules, default)

		g_fAllModulesHang = true;
		fOk = true;
	}

	// server

	else if (!wcsnicmp(parg, SWITCH_SERVER, wcslen(SWITCH_SERVER)))
	{
		// server specified

		if (parg[wcslen(SWITCH_SERVER)])
		{
			wcscpy(g_szServer, &parg[wcslen(SWITCH_SERVER)]);
			fOk = true;
		}
		else
		{
			fOk = false;
		}
	}

	// wait for module completion

	else if (!wcsnicmp(parg, SWITCH_WAIT_MODCOMPLETE, wcslen(SWITCH_WAIT_MODCOMPLETE)))
	{
		// enable waiting for module completion

		g_fWaitForModuleCompletion = true;
		fOk = true;
	}

	// auto oom

	else if (!wcsnicmp(parg, SWITCH_AUTOOOM_ENABLE, wcslen(SWITCH_AUTOOOM_ENABLE)))
	{
		// enable auto oom

		g_fAutoOOM = true;
		fOk = true;
	}

	// track available memory and break if goes below mentioned minimum

	else if (!wcsnicmp(parg, SWITCH_TRACK_MEMORY, wcslen(SWITCH_TRACK_MEMORY)))
	{
		// minimum memory threshold

		LPWSTR pchStart = (LPWSTR)&parg[wcslen(SWITCH_TRACK_MEMORY)];
		LPWSTR pchEnd = pchStart;

		if (pchStart && *pchStart && pchEnd && *pchEnd)							// null check
		{
			while (pchEnd && iswdigit(*pchEnd))
			{
				pchEnd++;
			}

			if ((!*pchEnd || (L'%' == *pchEnd) || (L'B' == towupper(*pchEnd)) ||
				(L'K' == towupper(*pchEnd)) || (L'M' == towupper(*pchEnd))) &&
				(pchStart != pchEnd))
			{
				WCHAR sz[MAX_PATH];
				memset(sz, 0, sizeof sz);
				wcsncpy(sz, pchStart, (pchEnd - pchStart));
				g_nMemThreshold = _wtoi(sz);

				if (!*pchEnd)
				{
					// case: -trackmem:10, defaults to bytes

					fOk = true;
				}
				else
				{
					if (*pchEnd == L'%')
					{
						// case: -trackmem:10%

						if (g_nMemThreshold <= 100)
						{
							// percent value should not exceed 100

							if ((pchEnd + 1) && !*(pchEnd + 1))
							{
								// nothing should follow %

								g_fMemLevelPercent = true;
								fOk = true;
							}
							else
							{
								fOk = false;
							}
						}
						else
						{
							fOk = false;
						}
					}
					else if (towupper(*pchEnd) == L'K')
					{
						// case: -trackmem:10K(B)

						g_fMemLevelPercent = false;

						/*

							eliminate other possibilities: only following two
							allowed

							-trackmem:1024K
							-trackmem:1024KB
						*/

						if ((pchEnd + 1) && *(pchEnd + 1) && (towupper(*(pchEnd + 1)) != L'B'))
						{
							fOk = false;
						}
						else
						{
							g_nMemThreshold *= 1024;
							fOk = true;
						}
					}
					else if (towupper(*pchEnd) == L'M')
					{
						// case: -trackmem:10M(B)

						g_fMemLevelPercent = false;

						/*
							eliminate other possibilities: only following two
							allowed

							-trackmem:1024M
							-trackmem:1024MB
						*/

						if ((pchEnd + 1) && *(pchEnd + 1) && (towupper(*(pchEnd + 1)) != L'B'))
						{
							fOk = false;
						}
						else
						{
							g_nMemThreshold *= (1024 * 1024);
							fOk = true;
						}
					}
					else if (towupper(*pchEnd) == L'B' && !*(pchEnd + 1))
					{
						// case: -trackmem:10(B)

						g_fMemLevelPercent = false;
						fOk = true;
					}
					else
					{
						fOk = false;
					}
				}
			}
			else
			{
				fOk = false;
			}
		}
		else
		{
			fOk = false;
		}
	}

	// the big else

	else
	{
		fOk = false;
	}

	return fOk;
}


/*

@func:	Options.Dump, Function to dump startup options in the debug window, for
debugging purposes only

@rdesc:	void

@param:	void

@fault:	None

@pre:	None

@post:	None

@note:	This is a debugging only function and should not be used in the release
build

*/

static void Options::Dump()
{
	DebugDump(L"________________________________________________________________________________\r\n");
	DebugDump(L"Debugger Present		: %s\r\n", Utilities::DetectDebugger() ? L"TRUE" : L"FALSE");
	DebugDump(L"Hang Time				: %u min(s)\r\n", g_nHangTimeoutMinutes);
	DebugDump(L"Hung All Modules		: %s\r\n", g_fAllModulesHang ? L"TRUE" : L"FALSE");
	DebugDump(L"Run Time				: %u min(s)\r\n", g_nRunTimeMinutes);
	if ((Options::INDEFINITE == Options::g_fModuleDuration) || (Options::RANDOM == Options::g_fModuleDuration))
	{
		DebugDump(L"Wait for Modules to Exit: DISABLED\r\n");
	}
	else
	{
		DebugDump(L"Wait for Modules to Exit: %s\r\n", g_fWaitForModuleCompletion ? L"YES" : L"NO");
	}
	DebugDump(L"Results Refresh Interval: %u min(s)\r\n", g_nRefreshResultsIntervalMinutes);
	if (Options::INDEFINITE == Options::g_fModuleDuration)
	{
		DebugDump(L"Module Duration			: INDEFINITE\r\n");
	}
	else if (Options::RANDOM == Options::g_fModuleDuration)
	{
		DebugDump(L"Module Duration			: RANDOM\r\n");
	}
	else if (Options::MINIMUM == Options::g_fModuleDuration)
	{
		DebugDump(L"Module Duration			: MINIMUM\r\n");
	}
	else	// Options::Normal
	{
		DebugDump(L"Module Duration			: %u min(s)\r\n", g_nModuleDurationMinutes);
	}
	DebugDump(L"Module Count			: %u\r\n", g_nProcessSlots);
	DebugDump(L"Random Seed				: %u\r\n", g_nRandomSeed);
	DebugDump(L"AutoOOM					: %s\r\n",
		Options::g_fAutoOOM ? L"ENABLED" : L"DISABLED");
	DebugDump(L"MemTracking Threshold	: %d%s\r\n", Options::g_nMemThreshold,
		Options::g_fMemLevelPercent ? L"% (when memory usage exceeds)" : L" bytes (when available memory falls below)");
	DebugDump(L"Results File			: %s\r\n", g_szResultFile);
	DebugDump(L"History File			: %s\r\n", g_szHistoryFile);
	DebugDump(L"INI File				: %s\r\n", g_szIniFile);

	switch (g_Verbosity.GetValue())		// OEMStress does not use zones etc.
	{
	case SLOG_ABORT:
		DebugDump(L"Verbosity				: SLOG_ABORT\r\n");
		break;
	case SLOG_FAIL:
		DebugDump(L"Verbosity				: SLOG_FAIL\r\n");
		break;
	case SLOG_WARN1:
		DebugDump(L"Verbosity				: SLOG_WARN1\r\n");
		break;
	case SLOG_WARN2:
		DebugDump(L"Verbosity				: SLOG_WARN2\r\n");
		break;
	case SLOG_COMMENT:
		DebugDump(L"Verbosity				: SLOG_COMMENT\r\n");
		break;
	case SLOG_VERBOSE:
		DebugDump(L"Verbosity				: SLOG_VERBOSE\r\n");
		break;
	case SLOG_ALL:
		DebugDump(L"Verbosity				: SLOG_ALL\r\n");
		break;
	default:
		DebugDump(L"Verbosity				: UNDEFINED (0x%x)\r\n", g_Verbosity.GetValue());
		break;
	}
	DebugDump(L"________________________________________________________________________________\r\n");
	return;
}


static void Options::Usage()
{
	DebugDump(L"\r\nUsage: oemstress [-result[:file_path]] [-wait] [-history[:file_path]] [-ini:file_path]\r\n");
	DebugDump(L"       [-ht:hang_trigger_time] [-rt:requested_run_time] [-ri:result_refresh_interval]\r\n");
	DebugDump(L"       [-md:module_duration] [-mc:module_count] [-seed:random_seed] [-server:server_name]\r\n");
	DebugDump(L"       [-ho:(any|all)] [-vl[:(abort|fail|warning1|warning2|comment|verbose)]] [-autooom]\r\n");
	DebugDump(L"       -trackmem:(high_watermark%|low_watermark[B|KB|MB])\r\n");
	DebugDump(L"\r\nCommand mode (use only one): [-terminate] [-vl[:(abort|fail|warning1|warning2|comment|verbose)]]\r\n");
	DebugDump(L"\r\nPlease refer to the readme.txt for detailed usage information. Subsequent instances of the\r\n");
	DebugDump(L"harness run in 'command mode' when already one instance of the harness is running. In command mode, the\r\n");
	DebugDump(L"harness accepts only one command line argument. Option '-terminate' can only be used in command mode.\r\n");
	DebugDump(L"No white space is allowed between command line switch and corresponding value. File paths with spaces, \r\n");
	DebugDump(L"should be placed inside double quotes.\r\n\r\n");
	return;
}


/*

@func:	CETK.CheckModuleDependency, Looks in the WinCE root directory for the
dependent files for each module

@rdesc:	true if all files are available, false otherwise

@param:	IN LPSTR lpszDependency: List of filenames separated by '*'

@fault:	None

@pre:	None

@post:	Internally allocates g_hMemMissingFiles and appends names of the
unavailable files

@note:	Use only when launched from CETK

*/

static bool CETK::CheckModuleDependency(IN LPSTR lpszDependency)
{
	bool fOk = true;
	char szFileName[MAX_PATH + 1];
	WCHAR wszFileName[MAX_PATH + 1];
	char *pStart = lpszDependency;
	char *pEnd = lpszDependency;

	if (!lpszDependency)
	{
		fOk &= true;
		goto done;
	}

	while (pStart && *pStart && pEnd && *pEnd)
	{
		// skip leading whitespaces and '*'

		while (pStart && *pStart && (('*' == *pStart) || (' ' == *pStart) || ('\t' == *pStart)))
		{
			pStart++;
		}

		if (!pStart || !*pStart)
		{
			break;
		}

		pEnd = pStart;

		// find end of the filename

		while (pEnd && *pEnd && (*pEnd != '*'))
		{
			// dependent filenames are separated by '*' (no quotes)

			pEnd++;
		}

		if ((pEnd - pStart) > MAX_PATH)
		{
			fOk &= false;
			break;
		}

		memset(szFileName, 0, sizeof szFileName);
		memcpy(szFileName, pStart, (pEnd - pStart));

		// convert to widechar

		memset(wszFileName, 0, sizeof wszFileName);
		mbstowcs(wszFileName, szFileName, strlen(szFileName));

		if (0xFFFFFFFF == GetFileAttributes(wszFileName))
		{
			// file does not exist: add unique filename to the list

			AddUniqueFileName(g_hMemMissingFiles, wszFileName);
			DebugDump(L"OEMSTRESS: Missing %s (helper file)...\r\n", wszFileName);
			fOk &= false;
		}

		pStart = pEnd;
	}

done:
	return fOk;
}


/*

@func:	CETK.AddUniqueFileName, Appends supplied unique filename to list
pointed by hMemFileNames only if

@rdesc:	true if successful, false otherwise

@param:	IN OUT HLOCAL &hMemFileNames: Handle to the memory block containing the
list, if

@param:	IN LPWSTR wszFileName: Name of the file to be appended to the global
list if unique

@fault:	None

@pre:	Supply a valid variable of type HGLOBAL as first argument. If
hMemFileNames is NULL, it uses LocalAlloc; LocalReAlloc is used if
hMemFileNames is not NULL.

@post:	Block pointed by hMemFileNames must be freed by the caller or elsewhere

@note:	Internal function, call only from CETK::CheckModuleDependency

*/

static bool CETK::AddUniqueFileName(IN OUT HLOCAL &hMemFileNames, IN LPWSTR wszFileName)
{
	bool fOk = true;

	const EXTRA = 2;		// 2 '*'s (front & trailing)
	WCHAR wsz[MAX_PATH + EXTRA + 1];
	LPWSTR pwsz = NULL;

	memset(wsz, 0, sizeof wsz);
	wcscpy(wsz, L"*");
	wcscat(wsz, _wcslwr(wszFileName));	// always in lowercase
	wcscat(wsz, L"*");

	if (hMemFileNames)
	{
		pwsz = NULL;
		pwsz = (LPWSTR)LocalLock(hMemFileNames);
		if (wcsstr(pwsz, wsz))
		{
			// filename already part of the list

			fOk = false;
			LocalUnlock(hMemFileNames);
			goto done;
		}
		INT nLength = wcslen(pwsz) + wcslen(wsz) + 1;
		LocalUnlock(hMemFileNames);

		HLOCAL hMemTmp = LocalReAlloc(hMemFileNames, (nLength * sizeof(WCHAR)),
			LMEM_MOVEABLE | LMEM_ZEROINIT);

		if (!hMemTmp)
		{
			fOk = false;
			goto done;
		}

		hMemFileNames = hMemTmp;
	}
	else	// first time
	{
		hMemFileNames = LocalAlloc(LMEM_ZEROINIT, ((wcslen(wsz) + 1) * sizeof(WCHAR)));

		if (!hMemFileNames)
		{
			fOk = false;
			goto done;
		}
	}

	// at this point hMemFileNames is (re)allocated and wszFileName is unique

	pwsz = NULL;
	pwsz = (LPWSTR)LocalLock(hMemFileNames);
	wcscat(pwsz, wsz);	// store in lowercase (converted already)
	LocalUnlock(hMemFileNames);

done:
	return fOk;
}


/*

@func:	Utilities.DetectDebugger, Detects the presence of attached debugger

@rdesc:	true if attached debugger detected, false otherwise

@param:	void

@fault:	None

@pre:	None

@post:	None

@note:	Function uses toolhelp APIs but there's no static linkage

*/

static bool Utilities::DetectDebugger()
{
	typedef HANDLE (*PFN_CREATE_TOOLHELP32SNAPSHOT)(DWORD, DWORD);
	typedef BOOL (*PFN_MODULE32)(HANDLE, LPMODULEENTRY32);
	typedef	BOOL (*PFN_CLOSE_TOOLHELP32SNAPSHOT)(HANDLE);

	bool fOk = true;
	bool fFoundKernelDebugger = false;
	PFN_CREATE_TOOLHELP32SNAPSHOT pfnCreateToolhelp32Snapshot = NULL;
	PFN_MODULE32 pfnModule32First = NULL, pfnModule32Next = NULL;
	PFN_CLOSE_TOOLHELP32SNAPSHOT pfnCloseToolhelp32Snapshot = NULL;
	HANDLE hModuleSnapshot = NULL;

	HINSTANCE hToolHelp = LoadLibrary(L"toolhelp.dll");

	if (!hToolHelp)
	{
		fOk = false;
		goto done;
	}

   	pfnCreateToolhelp32Snapshot = (PFN_CREATE_TOOLHELP32SNAPSHOT)GetProcAddress(hToolHelp, L"CreateToolhelp32Snapshot");
   	pfnModule32First = (PFN_MODULE32)GetProcAddress(hToolHelp, L"Module32First");
   	pfnModule32Next = (PFN_MODULE32)GetProcAddress(hToolHelp, L"Module32Next");
   	pfnCloseToolhelp32Snapshot = (PFN_CLOSE_TOOLHELP32SNAPSHOT)GetProcAddress(hToolHelp, L"CloseToolhelp32Snapshot");

	if (!pfnCreateToolhelp32Snapshot || !pfnModule32First ||
		!pfnModule32Next || !pfnCloseToolhelp32Snapshot)
	{
		fOk = false;
		goto done;
	}

	hModuleSnapshot = pfnCreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_GETALLMODS, 0);

	if (hModuleSnapshot == (HANDLE) -1)
	{
		fOk = false;
		goto done;
	}
	else
	{
		MODULEENTRY32 me32 = {0};
		fFoundKernelDebugger = false;

		me32.dwSize = sizeof(MODULEENTRY32);

		if (pfnModule32First(hModuleSnapshot, &me32))
		{
			do
			{
				if (!wcscmp(L"kd.dll", me32.szModule))
				{
					fFoundKernelDebugger = true;
					break;
				}
			} while (pfnModule32Next(hModuleSnapshot, &me32));
		}

		if (!pfnCloseToolhelp32Snapshot(hModuleSnapshot))
		{
			fOk = false;
			goto done;
		}
	}

done:
	if (hToolHelp)
	{
		FreeLibrary(hToolHelp);
	}

	return (fOk && fFoundKernelDebugger);
}


/*

@func:	Utilities.ModifyAutoOOM, set or reset device Auto OOM status

@rdesc:	true if successful, false otherwise

@param:	IN BOOL fSet: true falue sets the Auto OOM, false resets

@fault:	None

@pre:	None

@post:	None

@note:	None

*/

static BOOL Utilities::ModifyAutoOOM(IN BOOL fSet)
{
	if (SystemParametersInfo(SPI_AUTO_OOM, fSet, NULL, FALSE))
	{
		if (fSet)
		{
			DebugDump(L"OEMSTRESS: Auto OOM enabled\r\n");
		}
		else
		{
			DebugDump(L"OEMSTRESS: Auto OOM disabled\r\n");
		}
	}
	else
	{
		DebugDump(L"#### OEMSTRESS: SystemParametersInfo failed changing Auto OOM - Error: 0x%x ####\r\n",
			GetLastError());
		return false;
	}

	return true;
}


/*

@func:	Utilities.BreakAtMemThreshold, Logs and breaks into debugger (if present)
when the available falls below specified threshold (or usage percentage crosses
specified percentage value)

@rdesc:	void

@param:	IN _result_t *pResult: Results file object pointer

@param:	IN _history_t *pHistoty: History file object pointer

@fault:	None

@pre:	None

@post:	None

@note:	The function breaks if available memory falls below specified absolute
value OR if the specified usage percentage crosses specified percentage value

*/

void Utilities::BreakAtMemThreshold(IN _result_t *pResult, IN _history_t *pHistoty)
{
	MEMORYSTATUS ms;

	ms.dwLength = sizeof MEMORYSTATUS;
	GlobalMemoryStatus(&ms);

	BOOL fThresholdCrossed = false;

	if (Options::g_fMemLevelPercent)
	{
		if (ms.dwMemoryLoad > Options::g_nMemThreshold)
		{
			fThresholdCrossed = true;
		}
	}
	else
	{
		if (ms.dwAvailPhys < Options::g_nMemThreshold)
		{
			fThresholdCrossed = true;
		}
	}

	if (fThresholdCrossed)
	{
		if (pHistoty)
		{
			pHistoty->LogResults(&g_csUpdateResults);
		}

		if (pResult)
		{
			pResult->LogResults(&g_csUpdateResults,
				(BOOL)InterlockedExchange((LPLONG)&g_fLogHangDetails, (LONG)g_fLogHangDetails),
				false, true);
		}

		if (Options::g_fDebuggerPresent)
		{
			EnterCriticalSection(&g_csUpdateResults);
			DebugBreak();
			LeaveCriticalSection(&g_csUpdateResults);
		}
	}

	return;
}


/*

@func:	RemoveTrailingWhitespaceANSI, Removes trailing whitespaces (if any) from
the passed ANSI string

@rdesc:	Pointer to the same string if successful, NULL otherwise

@param:	IN OUT LPSTR lpsz: Pointer to the string to remove the trailing spaces
from

@fault:	None

@pre:	None

@post:	None

@note:	Whitespace: Any combination of space or tab characters (ANSI function)

*/

LPSTR Utilities::RemoveTrailingWhitespaceANSI(IN OUT LPSTR lpsz)
{
	if (!lpsz)
	{
		return NULL;
	}
	else if (!*lpsz)
	{
		return lpsz;
	}
	else
	{
		LPSTR pch = &lpsz[strlen(lpsz)];

		while ((pch >= lpsz) && ((*pch == ' ') || (*pch == '\t') || (*pch == '\0')))
		{
			*pch = NULL;
			pch--;
		}
	}

	return lpsz;
}


/*

@func:	FileNameFromPath, Extracts trailing file name from the given path. It'll
extract 'result.log' as the filename in following three cases:

	- lpszPath is cesh:result.log
	- lpszPath is \\computername\share\result.log
	- lpszPath is \release\result.log

@rdesc:	true if successful, false otherwise

@param:	IN LPCWSTR lpszPath: Complete file path as in the above mentioned
example

@param:	IN OUT LPWSTR lpszFileName: Allocated widechar buffer to hold the
retrieved file name, must be large enough

@param:	IN INT nLength: Length of the previous buffer (number of allocated
widechars)

@fault:	None

@pre:	lpszPath should be valid as the function performs minimal error checking

@post:	lpszFileName should contain the extracted filename if successful

@note:	None

*/

bool Utilities::FileNameFromPath(IN LPCWSTR lpszPath, IN OUT LPWSTR lpszFileName, IN INT nLength)
{
	if (!lpszPath || !*lpszPath || !lpszFileName)
	{
		return false;
	}

	memset(lpszFileName, 0, sizeof(WCHAR) * nLength);

	LPWSTR pch = (LPWSTR)&lpszPath[wcslen(lpszPath)];

	while ((pch > lpszPath) && !((*(pch - 1) == L'\\') || (*(pch - 1) == L':')))
	{
		pch--;
	}

	wcscpy(lpszFileName, pch);

	return true;
}
