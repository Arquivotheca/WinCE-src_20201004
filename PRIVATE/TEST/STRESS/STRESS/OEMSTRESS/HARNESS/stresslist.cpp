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

Module Name: stresslist.cpp

Abstract: Contains core harness functions and class definitions including module
list and run list

Notes: None

________________________________________________________________________________
*/

#include <windows.h>
#include <stressutils.h>
#include <stressutilsp.h>
#include <resultsmmf.hxx>
#include "helpers.h"
#include "logger.h"
#include "stresslist.h"


extern CRITICAL_SECTION g_csModList;
extern HANDLE g_hTerminateEvent;
extern CRITICAL_SECTION g_csUpdateResults;
extern BOOL g_fLogHangDetails;					// use only Interlocked* functions


//______________________________________________________________________________

_modulelist_t::_modulelist_t()
{
	_pHead = _pCurrent = NULL;
	_nLength = 0;
}


_modulelist_t::~_modulelist_t()
{
	PNODE pNext, pFirst;

	pFirst = _pHead;

	while (pFirst)
	{
		pNext = pFirst->pNext;

		delete pFirst;

		pFirst = pNext;
	}
}


VOID _modulelist_t::Add(IN PNODE pNew)
{
	PNODE pEntry;

	pNew->pNext = NULL;

	if (!_pHead)
	{
		pEntry = new NODE();
		*pEntry = *pNew;
		_pHead = pEntry;
		_nLength++;
		return;
	}

	PNODE pTemp = _pHead;

	while (pTemp->pNext)
	{
		pTemp = pTemp->pNext;
	}

	pEntry = new NODE;
	*pEntry = *pNew;
	pTemp->pNext = pEntry;

	_nLength++;

	return;
}


PNODE _modulelist_t::RandomNode()
{
	PNODE pEntry = _pHead;

	int nRandom = rand() % _nLength;

	while (nRandom)
	{
		pEntry = pEntry->pNext;
		nRandom--;
	}

	return pEntry;
}


VOID _modulelist_t::Reset()
{
	_pCurrent = _pHead;
}


PNODE _modulelist_t::Next()
{
	if (_pCurrent)
	{
		_pCurrent = _pCurrent->pNext;
	}

	return _pCurrent;
}


#ifdef DEBUG
VOID _modulelist_t::DumpStatus()
{
	EnterCriticalSection(&g_csModList);

	Reset();
	DebugDump(L"================================================================================");

	do
	{
		if (_pCurrent)
		{
			DebugDump(L"\r\n");
			DebugDump(L"OEMSTRESS::Module				:	%s\r\n", ((PMODULE_DETAIL)_pCurrent)->pModuleName);
			DebugDump(L"OEMSTRESS::Launched				:	%u\r\n", ((PMODULE_DETAIL)_pCurrent)->nLaunchCount);
			DebugDump(L"OEMSTRESS::Iterations			:	%u\r\n", ((PMODULE_DETAIL)_pCurrent)->nIterations);
			DebugDump(L"OEMSTRESS::Passed				:	%u\r\n", ((PMODULE_DETAIL)_pCurrent)->nPassCount);
			DebugDump(L"OEMSTRESS::Warning1				:	%u\r\n", ((PMODULE_DETAIL)_pCurrent)->nLevel1WarningCount);
			DebugDump(L"OEMSTRESS::Warning2				:	%u\r\n", ((PMODULE_DETAIL)_pCurrent)->nLevel2WarningCount);
			DebugDump(L"OEMSTRESS::Failed				:	%u\r\n", ((PMODULE_DETAIL)_pCurrent)->nFailCount);
			DebugDump(L"OEMSTRESS::Aborted				:	%u\r\n", ((PMODULE_DETAIL)_pCurrent)->nAbortCount);
			DebugDump(L"OEMSTRESS::Terminated			:	%u\r\n", ((PMODULE_DETAIL)_pCurrent)->nTerminateCount);
			DebugDump(L"OEMSTRESS::Avg Duration			:	%.2f sec\r\n", ((double)((PMODULE_DETAIL)_pCurrent)->dwAverageDuration / SECOND));
			DebugDump(L"OEMSTRESS::Max Duration			:	%.2f sec\r\n", ((double)((double)((PMODULE_DETAIL)_pCurrent)->dwMaxDuration / SECOND)));
			DebugDump(L"OEMSTRESS::Min Duration			:	%.2f sec\r\n",
				(UINT_MAX == ((PMODULE_DETAIL)_pCurrent)->dwMinDuration) ? 0 : ((double)((PMODULE_DETAIL)_pCurrent)->dwMinDuration / SECOND));
			DebugDump(L"OEMSTRESS::First Launched At	:	%u ticks\r\n", ((PMODULE_DETAIL)_pCurrent)->dwTickCountFirstLaunched);
			DebugDump(L"OEMSTRESS::Last Launched At		:	%u ticks\r\n", ((PMODULE_DETAIL)_pCurrent)->dwTickCountLastLaunched);
			DebugDump(L"\r\n");
		}
	}
	while (Next());

	DebugDump(L"================================================================================");

	LeaveCriticalSection(&g_csModList);
}
#endif /* DEBUG */


//______________________________________________________________________________


_result_t *_runlist_t::_pResult;
PMODULE_DETAIL *_runlist_t::ppmd;
DWORD *_runlist_t::pdwTickCountAtLaunch;
INT _runlist_t::nRunning;
INT _runlist_t::nLength;
HTHREAD _runlist_t::_hThread;
DWORD _runlist_t::_dwThreadID;
HANDLE *_runlist_t::_phProcess;
HTHREAD _runlist_t::_hHangDetectorThread;
DWORD _runlist_t::_dwHangDetectorThreadID;
HANDLE _runlist_t::_hHangEvent;
CRITICAL_SECTION _runlist_t::csRunList;
bool _runlist_t::_fLaunchNewModule;
bool _runlist_t::_fInitializedCriticalSection;


const RESERVED_HANDLES = 2;	// terminate event & hang event
const INDEX_TERMINATION_EVENT = 0;
const INDEX_HANG_DETECTION_EVENT = 1;


CResultsMMFile g_ResultsMMF(RESULTS_MMDATA, 0);	// important: keep it global


/*

@func:	_runlist_t.Init, Initializes runlist, launches refresh and hang detector thread

@rdesc:	true if successful, false otherwise

@param:	IN INT nMaxProcessCount: Maximum number of processes to launch at a given
instance

@param:	IN _result_t *pResult, Pointer to the result file object, defaulted to NULL

@fault:	None

@pre:	None

@post:	Uninit must be called before calling Init again

@note:	**** This is a static function ****

*/

BOOL _runlist_t::Init(IN INT nMaxProcessCount, IN _result_t *pResult)
{
	BOOL fOk = true;

	_runlist_t::_fInitializedCriticalSection = false;
	InitializeCriticalSection(&_runlist_t::csRunList);
	_runlist_t::_fInitializedCriticalSection = true;

	_runlist_t::_hHangEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (NULL == _runlist_t::_hHangEvent)
	{
		DebugDump(L"#### OEMSTRESS: _runlist_t::Init: Error 0x%x creating hang event ####\r\n",
			GetLastError());
		fOk = FALSE;
		goto done;
	}

	_runlist_t::nLength = nMaxProcessCount;

	_runlist_t::ppmd = new PMODULE_DETAIL [_runlist_t::nLength];

	if (!_runlist_t::ppmd)
	{
		DebugDump(L"#### OEMSTRESS: _runlist_t::Init: Unable to allocate memory for module detail ####\r\n");
		fOk = false;
		goto done;
	}

	memset(_runlist_t::ppmd, 0, (sizeof(PMODULE_DETAIL) * _runlist_t::nLength));
	_runlist_t::nRunning = 0;

	_runlist_t::_phProcess = new HANDLE [RESERVED_HANDLES + _runlist_t::nLength];

	if (!_runlist_t::_phProcess)
	{
		DebugDump(L"#### OEMSTRESS: _runlist_t::Init: Unable to allocate memory for process handles ####\r\n");
		fOk = false;
		goto done;
	}

	memset(_runlist_t::_phProcess, 0, (sizeof(HANDLE) * (RESERVED_HANDLES + _runlist_t::nLength)));
	_runlist_t::_phProcess[INDEX_TERMINATION_EVENT] = g_hTerminateEvent;
	_runlist_t::_phProcess[INDEX_HANG_DETECTION_EVENT] = _runlist_t::_hHangEvent;

	_runlist_t::pdwTickCountAtLaunch = new DWORD [_runlist_t::nLength];

	if (!_runlist_t::pdwTickCountAtLaunch)
	{
		DebugDump(L"#### OEMSTRESS: _runlist_t::Init: Unable to allocate memory for started time buffer ####\r\n");
		fOk = false;
		goto done;
	}

	memset(_runlist_t::pdwTickCountAtLaunch, 0, (sizeof(HANDLE) * _runlist_t::nLength));

	_pResult = pResult;

	_runlist_t::LaunchNewModule() = true;	// important: must be set to true at this point

	// refresh thread

	SetLastError(0);
	_runlist_t::_hThread = CreateThread(0, 0, _runlist_t::RefreshThreadProc, (LPVOID)0,
		0, &_runlist_t::_dwThreadID);

	if (NULL == _runlist_t::_hThread)
	{
		DebugDump(L"#### OEMSTRESS: _runlist_t::Init: Error 0x%x creating refresh thread ####\r\n",
			GetLastError());
		fOk = false;
		goto done;
	}

	// hang detector thread

	SetLastError(0);
	_runlist_t::_hHangDetectorThread = CreateThread(0, 0, _runlist_t::HangDetectorThreadProc,
		(LPVOID)0, 0, &_runlist_t::_dwHangDetectorThreadID);

	if (NULL == _runlist_t::_hHangDetectorThread)
	{
		DebugDump(L"#### OEMSTRESS: _runlist_t::Init: Error 0x%x creating hang detector thread ####\r\n",
			GetLastError());
		fOk = false;
		goto done;
	}

	if (_runlist_t::_hThread)
	{
		SetThreadPriority(_runlist_t::_hThread, THREAD_PRIORITY_ABOVE_NORMAL);
	}

done:
	return fOk;
}


/*

@func:	_runlist_t.Uninit, Uninitializes runlist, waits for refresh and hang
detector threads to complete

@rdesc:	true if successful, false otherwise

@fault:	None

@pre:	None

@post:	Init must be called before calling Uninit again

@note:	**** This is a static function ****

*/

BOOL _runlist_t::Uninit()
{
	BOOL fOk = true;
	DWORD dwExitCode = 0;

	// refresh thread

	if (_runlist_t::_hThread)														// wait for the thread to complete execution
	{
		do
		{
			GetExitCodeThread(_runlist_t::_hThread, &dwExitCode);
			Sleep(THREAD_COMPLETION_DELAY);
		}
		while (dwExitCode == STILL_ACTIVE);

		fOk &= CloseHandle(_runlist_t::_hThread);
		_runlist_t::_hThread = NULL;
	}

	// hang detector thread

	dwExitCode = 0;

	if (_runlist_t::_hHangDetectorThread)											// wait for the thread to complete execution
	{
		do
		{
			GetExitCodeThread(_runlist_t::_hHangDetectorThread, &dwExitCode);
			Sleep(THREAD_COMPLETION_DELAY);
		}
		while (dwExitCode == STILL_ACTIVE);

		fOk &= CloseHandle(_runlist_t::_hHangDetectorThread);
		_runlist_t::_hHangDetectorThread = NULL;
	}

	delete [] _runlist_t::ppmd;
	delete [] _runlist_t::_phProcess;
	delete [] _runlist_t::pdwTickCountAtLaunch;

 	if (_runlist_t::_hHangEvent)
 	{
 		fOk &= CloseHandle(_runlist_t::_hHangEvent);
		_runlist_t::_hHangEvent = NULL;
	}

	if (_runlist_t::_fInitializedCriticalSection)
	{
		EnterCriticalSection(&_runlist_t::csRunList);
		_runlist_t::nRunning = 0;
		LeaveCriticalSection(&_runlist_t::csRunList);

		DeleteCriticalSection(&_runlist_t::csRunList);
	}

	_runlist_t::_fInitializedCriticalSection = false;

	return fOk;
}


/*

@func:	_runlist_t.GetProcessHandle, Returns a process handle running in a
particular slot

@rdesc:	Handle to the process if successful, NULL otherwise

@param:	IN INT iSlot: Slot ID of the module is question

@fault:	None

@pre:	None

@post:	None

@note:	None

*/

HANDLE _runlist_t::GetProcessHandle(IN INT iSlot)
{
	if ((iSlot >= 0) && _runlist_t::RunningModules() && (iSlot < _runlist_t::RunningModules()))
	{
		return _runlist_t::_phProcess[RESERVED_HANDLES + iSlot];
	}

	return NULL;
}


/*

@func:	_runlist_t.RefreshThreadProc, Module refresher threadproc, responsible
for launching and computing statistical information upon completion.

@fault:	None

@pre:	None

@post:	None

@note:	None

*/

DWORD WINAPI _runlist_t::RefreshThreadProc(IN LPVOID lpParam)
{
	while (true)
	{
		if (_runlist_t::LaunchNewModule())
		{
			while (_runlist_t::nRunning < _runlist_t::nLength)							// launch stress modules for each empty slot
			{
				for (INT iSlot = 0; iSlot < _runlist_t::nLength; iSlot++)				// find an empty slot
				{
					CRITICAL_SECTION_TAKER RunListCS(&_runlist_t::csRunList);

					if (!_runlist_t::ppmd[iSlot])										// don't need critical_section here (g_csModList)
					{
						break;															// found empty slot
					}
				}

				if (iSlot != _runlist_t::nLength)										// launch a new random process
				{
					// devnote: deadlock alert

					HANDLE hProcess = NULL;
					EnterCriticalSection(&csRunList);
					EnterCriticalSection(&g_csModList);
					hProcess = CreateRandomProcess((PMODULE_DETAIL *)&_runlist_t::ppmd[iSlot],
						&_runlist_t::pdwTickCountAtLaunch[iSlot], iSlot);
					LeaveCriticalSection(&g_csModList);
					LeaveCriticalSection(&csRunList);

					if (hProcess)
					{
						CRITICAL_SECTION_TAKER RunListCS(&_runlist_t::csRunList);
						CRITICAL_SECTION_TAKER ModListCS(&g_csModList);

						_runlist_t::nRunning++;											// one more process running
						_runlist_t::_phProcess[RESERVED_HANDLES + iSlot] = hProcess;	// update handles list

						DebugDump(L"OEMSTRESS: [%i] Module %s Launched\r\n",
							iSlot, ((PMODULE_DETAIL)_runlist_t::ppmd[iSlot])->pModuleName);
					}
					else
					{
						_runlist_t::ppmd[iSlot] = NULL;
						_runlist_t::RearrangeLists(iSlot);

						// stop launching modules

						break;															// ref# 0
					}
				}
			}

			if (!_runlist_t::nRunning)
			{
				continue;																// spawn more modules (side effect from ref# 0)
			}
		}																				// while (_runlist_t::nRunning < _runlist_t::nLength)
		else
		{
			_runlist_t::RearrangeLists(0);
		}

		if (_runlist_t::nRunning < 0)													// impossible case: detects logic error
		{
			DebugDump(L"#### OEMSTRESS: FATAL ERROR: Bad running process count: %u ####\r\n",
				_runlist_t::nRunning);
			SetEvent(_runlist_t::_phProcess[INDEX_TERMINATION_EVENT]);					// fire termination event
			break;																		// exit from RefreshThreadProc
		}

		// at this point first consecutive _runlist_t::nRunning slots should be full

		// refine wait period based on options

		DWORD dwTimeout;

		 if (Options::g_nHangTimeoutMinutes && Options::g_fAllModulesHang)
		{
			dwTimeout = Options::g_nHangTimeoutMinutes * MINUTE;
		}
		else
		{
			dwTimeout = INFINITE;
		}

		DWORD dwWaitObject = WaitForMultipleObjects((RESERVED_HANDLES + _runlist_t::nRunning),
			_runlist_t::_phProcess, FALSE, dwTimeout);

		if (WAIT_OBJECT_0 == dwWaitObject)											// terminate event triggered
		{
			// devnote: **** do not reset _runlist_t::_phProcess[INDEX_TERMINATION_EVENT] event ****

			DebugDump(L"OEMSTRESS: RefreshThreadProc: Termination request received\r\n");

			/*******************************************************************
			following 2 lines prevent an unlikely hang:

				- harness is in wait state (no new modules launched)
				- '-terminate' is used at the command line
				- all other threads terminate but mainthread loops forever as
				running process count remains non-zero

			devnote: Since _runlist_t::nRunning is being set to '0', a true
			g_fLogHangDetails might cause ONLY the hang table header to be
			printed in the text output (in case of xml, we filter this out in
			xslt). It is not a common scenario when the harness detects a module
			hang but the user decided to use '-terminate' command line option to
			conclude stress run. The final result (generated before harness
			exits) will indicate module hang but it will not display the list of
			hanging modules.
			*******************************************************************/

			CRITICAL_SECTION_TAKER cs(&csRunList);
			_runlist_t::nRunning = 0;												// reset running process count
			break;																	// exit refresh loop and threadproc
		}
		else if ((WAIT_OBJECT_0 + 1) == dwWaitObject)								// hang event triggered
		{
			if (Options::g_nHangTimeoutMinutes && !Options::g_fAllModulesHang)
			{
				InterlockedExchange((LPLONG)&g_fLogHangDetails, true);	// set flag to log hang information in the logging thread

				EnterCriticalSection(&g_csUpdateResults);
				DebugDump(L"OEMSTRESS: ********************************************************************************\r\n");
				DebugDump(L"OEMSTRESS: ********************************************************************************\r\n");
				DebugDump(L"OEMSTRESS: *****                                                                      *****\r\n");
				DebugDump(L"OEMSTRESS: *****                   AT LEAST ONE MODULE IS HUNG                        *****\r\n");
				DebugDump(L"OEMSTRESS: *****                                                                      *****\r\n");
				DebugDump(L"OEMSTRESS: ********************************************************************************\r\n");
				DebugDump(L"OEMSTRESS: ********************************************************************************\r\n");
				LeaveCriticalSection(&g_csUpdateResults);

				// update results with hang status

				if (_runlist_t::_pResult)
				{
					_runlist_t::_pResult->LogResults(&g_csUpdateResults, true);
				}

				if (Options::g_fDebuggerPresent)	// break if debugger is attached
				{
					EnterCriticalSection(&g_csUpdateResults);
					DebugBreak();
					LeaveCriticalSection(&g_csUpdateResults);
				}
			}
		}
		else if (WAIT_TIMEOUT == dwWaitObject)
		{
			if (Options::g_nHangTimeoutMinutes && Options::g_fAllModulesHang && !_runlist_t::SlotAvailable())
			{
				InterlockedExchange((LPLONG)&g_fLogHangDetails, true);	// set flag to log hang information in the logging thread

				EnterCriticalSection(&g_csUpdateResults);
				DebugDump(L"OEMSTRESS: ********************************************************************************\r\n");
				DebugDump(L"OEMSTRESS: ********************************************************************************\r\n");
				DebugDump(L"OEMSTRESS: *****                                                                      *****\r\n");
				DebugDump(L"OEMSTRESS: *****                       ALL THE MODULES ARE HUNG                       *****\r\n");
				DebugDump(L"OEMSTRESS: *****                                                                      *****\r\n");
				DebugDump(L"OEMSTRESS: ********************************************************************************\r\n");
				DebugDump(L"OEMSTRESS: ********************************************************************************\r\n");
				LeaveCriticalSection(&g_csUpdateResults);

				// update results with hang status anyway

				if (_runlist_t::_pResult)
				{
					_runlist_t::_pResult->LogResults(&g_csUpdateResults, true);
				}

				if (Options::g_fDebuggerPresent)	// break if debugger is attached
				{
					EnterCriticalSection(&g_csUpdateResults);
					DebugBreak();
					LeaveCriticalSection(&g_csUpdateResults);
				}
			}
		}
		else if (WAIT_FAILED == dwWaitObject)
		{
			DebugDump(L"#### OEMSTRESS: _runlist_t::RefreshThreadProc: WaitForMultipleObjects failed - Error 0x%x ####\r\n",
				GetLastError());
			SetEvent(_runlist_t::_phProcess[INDEX_TERMINATION_EVENT]);				// trigger terminate event (timestamp loop should terminate)
			break;
		}
		else
		{
			if (Options::g_nHangTimeoutMinutes)	// zero value disables the hang detection & hang logging
			{
				if (Options::g_fAllModulesHang)
				{
					InterlockedExchange((LPLONG)&g_fLogHangDetails, false);	// reset log-hang-details in case of all modules hang
				}
				else
				{
					CRITICAL_SECTION_TAKER RunListCS(&_runlist_t::csRunList);

					for (INT i = 0; i < _runlist_t::nRunning; i++)
					{
						if (_runlist_t::ppmd[i])
						{
							if ((GetTickCount() - _runlist_t::pdwTickCountAtLaunch[i]) >= (Options::g_nHangTimeoutMinutes * MINUTE))
							{
								break;
							}
						}
					}

					if (_runlist_t::nRunning == i)	// none of the apps are hanging, reset flag
					{
						InterlockedExchange((LPLONG)&g_fLogHangDetails, false);	// reset log-hang-details flag
					}
				}
			}

			const FILLED_SLOTS = _runlist_t::RunningModules();	// slots are always filled from the start

			for (INT iSlot = 0, iHandle = RESERVED_HANDLES + iSlot; iSlot < FILLED_SLOTS; iSlot++, iHandle++)
			{
				CRITICAL_SECTION_TAKER RunListCS(&_runlist_t::csRunList);
				CRITICAL_SECTION_TAKER ModListCS(&g_csModList);

				DWORD dwExitCode = 0;
				BOOL fReturn = GetExitCodeProcess(_runlist_t::_phProcess[iHandle], &dwExitCode);

				if (fReturn && (STILL_ACTIVE != dwExitCode))
				{
					if (ppmd[iSlot])
					{
						if (CESTRESS_ABORT == dwExitCode)
						{
							DebugDump(L"[%u] Module %s (Handle: 0x%x) aborted!",
								iSlot, ppmd[iSlot]->pModuleName, _runlist_t::_phProcess[iHandle]);
							ppmd[iSlot]->nAbortCount++;
						}
						else
						{
							STRESS_RESULTS StressResults;

							g_ResultsMMF.Read(&StressResults, iSlot);

							_runlist_t::ppmd[iSlot]->nPassCount += (StressResults.nIterations -
								(StressResults.nFail + StressResults.nWarn1 + StressResults.nWarn2));
							_runlist_t::ppmd[iSlot]->nIterations += StressResults.nIterations;
							_runlist_t::ppmd[iSlot]->nLevel1WarningCount += StressResults.nWarn1;
							_runlist_t::ppmd[iSlot]->nLevel2WarningCount += StressResults.nWarn2;
							_runlist_t::ppmd[iSlot]->nFailCount += StressResults.nFail;

							g_ResultsMMF.Clear(iSlot);

							DebugDump(L"[%i] Module %s Ended\r\n",
								iSlot, ((PMODULE_DETAIL)_runlist_t::ppmd[iSlot])->pModuleName);
						}

						/* following line helps stressutils determine how many instances of a
						particular module is running */

						DecrementRunningModuleCount(_runlist_t::ppmd[iSlot]->pModuleName);

						DWORD dwTickCountNow = GetTickCount();
						DWORD dwTickCountTaken = dwTickCountNow - _runlist_t::pdwTickCountAtLaunch[iSlot];
						_runlist_t::nRunning--;

						_runlist_t::ppmd[iSlot]->dwAverageDuration = _runlist_t::ppmd[iSlot]->dwAverageDuration ?
							((_runlist_t::ppmd[iSlot]->dwAverageDuration + dwTickCountTaken) / 2) : dwTickCountTaken;
						_runlist_t::ppmd[iSlot]->dwMaxDuration = __max(_runlist_t::ppmd[iSlot]->dwMaxDuration, dwTickCountTaken);
						_runlist_t::ppmd[iSlot]->dwMinDuration = __min(_runlist_t::ppmd[iSlot]->dwMinDuration, dwTickCountTaken);
						_runlist_t::ppmd[iSlot] = NULL;
						_runlist_t::_phProcess[iHandle] = NULL;
						_runlist_t::pdwTickCountAtLaunch[iSlot] = 0;
					}
				}
			}
		}
	}

	DebugDump(L"OEMSTRESS: RefreshThreadProc: Exiting\r\n");

	return FALSE;
}


/*

@func:	_runlist_t.RearrangeLists, Rearranges _runlist_t::_phProcess[],
_runlist_t::ppmd[] & _runlist_t::pdwTickCountAtLaunch[] by leftshifting the
array elements so that there's no gap in the middle, starting from the specified
slot

@rdesc: void

@param:	IN INT iSlot: Starting slot index, the compaction starts from here
(target)

@fault:	None

@pre:	None

@post:	None

@note: Private function, must be called ONLY from RefreshThreadProc

*/

void _runlist_t::RearrangeLists(IN INT iSlot)
{
	INT iTarget = iSlot, iSource = iSlot + 1;
	CRITICAL_SECTION_TAKER RunListCS(&_runlist_t::csRunList);

	while (iSource < _runlist_t::nLength)
	{
		if (iSource < iTarget)
		{
			iSource++;
			continue;
		}

		if (_runlist_t::ppmd[iTarget])
		{
			iTarget++;
			continue;
		}

		if (!_runlist_t::ppmd[iSource])
		{
			iSource++;
			continue;
		}

		// don't need critical_section g_csModList

		_runlist_t::ppmd[iTarget] = _runlist_t::ppmd[iSource];
		_phProcess[RESERVED_HANDLES + iTarget] = _phProcess[RESERVED_HANDLES + iSource];
		_runlist_t::pdwTickCountAtLaunch[iTarget] = _runlist_t::pdwTickCountAtLaunch[iSource];
		_runlist_t::ppmd[iSource] = NULL;

		iSource++;
		iTarget++;
	}

	for (INT i = _runlist_t::nRunning; i <_runlist_t::nLength; i++)
	{
		// don't need critical_section g_csModList

		_phProcess[RESERVED_HANDLES + i] = NULL;
		_runlist_t::ppmd[i] = NULL;
		_runlist_t::pdwTickCountAtLaunch[i] = NULL;
	}

	return;
}


/*

@func:	_runlist_t::HangDetectorThreadProc, Hang detector threadproc,
responsible for triggering the hang event when a single application is considered
to be hung.

@fault:	None

@pre:	None

@post:	None

@note:	None

*/

DWORD WINAPI _runlist_t::HangDetectorThreadProc(IN LPVOID lpParam)
{
	DWORD dwWaitObject = WAIT_FAILED;

	while (true)
	{
		dwWaitObject = WaitForSingleObject(g_hTerminateEvent, 0);

		if (WAIT_FAILED == dwWaitObject)
		{
			DebugDump(L"#### OEMSTRESS: _runlist_t::HangDetectorThreadProc: WaitForSingleObject failed - Error: 0x%x, terminating hang detector thread... ####\r\n",
				GetLastError());
			break;
		}
		else if (WAIT_OBJECT_0 == dwWaitObject)											// terminate event triggered
		{
			// **** do not reset event ****

			DebugDump(L"OEMSTRESS: HangDetectorThreadProc: Termination request received\r\n");
			break;
		}

		Sleep(MINUTE);

		if (Options::g_nHangTimeoutMinutes && !Options::g_fAllModulesHang)	// zero value disables the hang detection & hang logging
		{
			CRITICAL_SECTION_TAKER RunListCS(&_runlist_t::csRunList);

			for (INT i = 0; i < _runlist_t::nRunning; i++)
			{
				if (_runlist_t::ppmd[i])
				{
					if ((GetTickCount() - _runlist_t::pdwTickCountAtLaunch[i]) >= (Options::g_nHangTimeoutMinutes * MINUTE))
					{
						PulseEvent(_runlist_t::_hHangEvent);
						break;
					}
				}
			}
		}
	}

	DebugDump(L"OEMSTRESS: HangDetectorThreadProc: Exiting\r\n");
	return FALSE;
}

//______________________________________________________________________________
