//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <stressutils.h>
#include <stressutilsp.h>


#define tszInit				_T("InitializeStressModule")
#define tszDoStress			_T("DoStressIteration")
#define tszTerm				_T("TerminateStressModule")
#define tszInitThread		_T("InitializeTestThread")
#define tszCleanupThread	_T("CleanupTestThread")

#define THREAD_CREATE_TIMEOUT	(60 * 1000)
#define THREAD_WAIT_INTERVAL	(60 * 1000)


typedef BOOL  (WINAPI  *PFN_INITSTRESSMODULE)(MODULE_PARAMS*, UINT*);
typedef UINT  (WINAPI  *PFN_DOSTRESS)(HANDLE, DWORD, LPVOID);
typedef DWORD  (WINAPI  *PFN_TERMSTRESSMODULE)(void);
typedef UINT  (WINAPI  *PFN_INITTHREAD)(HANDLE, DWORD, int);
typedef UINT  (WINAPI  *PFN_CLEANUPTHREAD)(HANDLE, DWORD, int);


typedef struct _TEST_THREAD_PARAMS
{
	STRESS_RESULTS* pRes;
	int				index;
}
TEST_THREAD_PARAMS, *PTEST_THREAD_PARAMS;


	
PFN_INITSTRESSMODULE	g_pfnInitStress = NULL;
PFN_DOSTRESS			g_pfnDoStress = NULL;
PFN_TERMSTRESSMODULE	g_pfnTerminateStress = NULL;
PFN_INITTHREAD			g_pfnInitThread = NULL;
PFN_CLEANUPTHREAD		g_pfnCleanupThread = NULL;

HANDLE					g_hTimesUp = NULL;
HANDLE					g_hThreadCreate = NULL;
CRITICAL_SECTION		g_csResults;



///////////////////////////////////////////////
//
// Worker thread to do the acutal testing
//

DWORD WINAPI TestThreadProc (LPVOID pv)
{
	TEST_THREAD_PARAMS* pParams = (TEST_THREAD_PARAMS*) pv;
	int index = pParams->index;
	STRESS_RESULTS* pRes = pParams->pRes; // get a ptr to the module results struct
	STRESS_RESULTS res;
	HANDLE hThread = GetCurrentThread();
	DWORD dwId = GetCurrentThreadId();
	UINT ret;
	ITERATION_INFO	iterationInfo;

	//	JGLEN: changed to SetEvent from PulseEvent.
	SetEvent(g_hThreadCreate);


	iterationInfo.cbSize = sizeof(ITERATION_INFO);
	iterationInfo.index = index;
	iterationInfo.iteration = 0;

	ZeroMemory((void*) &res, sizeof(STRESS_RESULTS));
	
	
	// Call test thread's init function, if it exists
	if (g_pfnInitThread)
	{
		ret = g_pfnInitThread(hThread, dwId, index);

		if (ret == CESTRESS_ABORT)
		{
			SetEvent(g_hTimesUp);  // signal other threads to bail
			return CESTRESS_ABORT;
		}
	}



	// Do our testing until time's up is signaled
	while (WAIT_OBJECT_0 != WaitForSingleObject (g_hTimesUp, 0))
	{
		iterationInfo.iteration++;

		ret = g_pfnDoStress(hThread, dwId, &iterationInfo);

		if (ret == CESTRESS_ABORT)
		{
			SetEvent(g_hTimesUp);  // signal other threads to bail
			return CESTRESS_ABORT;
		}

		// Keep a local tally of this thread's results
		RecordIterationResults(&res, ret);
	}



	// Call test thread's cleanup function, if it exists
	if (g_pfnCleanupThread)
	{
		ret = g_pfnCleanupThread(hThread, dwId, index);

		if (ret != CESTRESS_PASS)
			RecordIterationResults(&res, ret);
	}


	// Add this thread's results to the module tally
	EnterCriticalSection(&g_csResults);

		AddResults(pRes, &res);

	LeaveCriticalSection(&g_csResults);

	return 0;
}



///////////////////////////////////////////////
//
// WinMain
//

int WINAPI WinMain ( 
					HINSTANCE hInstance,
					HINSTANCE hInstPrev,
#ifdef UNDER_CE
					LPWSTR wszCmdLine,
#else
					LPSTR szCmdLine,
#endif
					int nCmdShow
					)
{
	DWORD				dwStartTime = GetTickCount();
	LPTSTR				tszCmdLine;
	int					retValue = 0;  // Module return value.  You should return 0 unless you had to abort.
	MODULE_PARAMS		mp;            // Cmd line args arranged here.  Used to init the stress utils.
	STRESS_RESULTS		results;       // Cumulative test results for this module.
	TEST_THREAD_PARAMS	threadParams;
	UINT				nThreads = 1;
	DWORD				dwExitCode = 0;
	DWORD				dwEnd = 0;
	UINT				nExited = 0;
	HINSTANCE			hDll = NULL;
	HANDLE*				hThread = NULL;
	DWORD*				dwThreadId = NULL;
	UINT				i = 0;
	UINT				n = 0;
	UINT				count = 0;
	UINT				nRunningThreads = 0;


	ZeroMemory((void*) &mp, sizeof(MODULE_PARAMS));
	ZeroMemory((void*) &results, sizeof(STRESS_RESULTS));

	
	// NOTE:  Modules should be runnable on desktop whenever possible,
	//        so we need to convert to a common cmd line char width:

#ifdef UNDER_NT
	tszCmdLine =  GetCommandLine();
#else
	tszCmdLine  = wszCmdLine;
#endif



	// Extract DLL name from head of command line


	LPTSTR ptszDll = tszCmdLine;

	LPTSTR pCmdLine = _tcschr(tszCmdLine, _T(' '));

	if (!pCmdLine)
	{
		LogFail(_T("CommandLine is corrupt: %s"), tszCmdLine);
		return CESTRESS_ABORT; 
	}


	*pCmdLine = _T('\0');
	pCmdLine++;



	// Load the stress DLL


	hDll = LoadLibrary(ptszDll);

	if (!hDll)
	{
		LogFail(_T("Failed to load library: %s, err = %d"), ptszDll, GetLastError());
		return CESTRESS_ABORT; 
	}



	if ( !ParseCmdLine_WinMain (pCmdLine, &mp) )
	{
		LogFail (_T("Failure parsing the command line!  exiting ..."));
		
	    retValue = CESTRESS_ABORT;
	    goto cleanup; 
	}



	// Find Dll exports


	g_pfnInitStress = (PFN_INITSTRESSMODULE) GetProcAddress(hDll, tszInit);

	if (!g_pfnInitStress)
	{
		LogFail (_T("GetProcAddress failed to find %s, err = %d"), tszInit, GetLastError());
	
	    retValue = CESTRESS_ABORT;
	    goto cleanup;
	}

	g_pfnDoStress = (PFN_DOSTRESS) GetProcAddress(hDll, tszDoStress);

	if (!g_pfnDoStress)
	{
		LogFail (_T("GetProcAddress failed to find %s, err = %d"), tszDoStress, GetLastError());
	
	    retValue = CESTRESS_ABORT;
	    goto cleanup;
	}

	g_pfnTerminateStress = (PFN_TERMSTRESSMODULE) GetProcAddress(hDll, tszTerm);

	if (!g_pfnTerminateStress)
	{
		LogFail (_T("GetProcAddress failed to find %s, err = %d"), tszTerm, GetLastError());
	
	    retValue = CESTRESS_ABORT;
	    goto cleanup;
	}


	// Optional exports

	g_pfnInitThread = (PFN_INITTHREAD) GetProcAddress(hDll, tszInitThread);
	g_pfnCleanupThread = (PFN_CLEANUPTHREAD) GetProcAddress(hDll, tszCleanupThread);

	



	// Initialize the Dll


	if (!g_pfnInitStress(&mp, &nThreads))
	{
		LogFail (_T("Failed to initialize %s"), ptszDll);
	
	    retValue = CESTRESS_ABORT;
	    goto cleanup;
	}

	LogVerbose (_T("Starting at %d"), dwStartTime);
	



	///////////////////////////////////////////////////////
	// Main test loop
	//

	
	hThread = new HANDLE [nThreads];
	dwThreadId = new DWORD [nThreads];

	ZeroMemory((void*) hThread, sizeof(HANDLE) * nThreads);

	if (NULL == hThread || NULL == dwThreadId)
	{
	    LogWarn2(_T("Memory allocation failed.  Aborting."));
	    retValue = CESTRESS_ABORT;
	    goto cleanup;
	}

	InitializeCriticalSection (&g_csResults);


	// Spin one or more worker threads to do the actually testing.

	LogVerbose (_T("Creating %i worker threads"), nThreads);

	g_hTimesUp = CreateEvent (NULL, TRUE, FALSE, NULL);

	//	JGLEN: Changed to auto-reset event.
	g_hThreadCreate = CreateEvent (NULL, FALSE, FALSE, NULL);

	threadParams.pRes = &results;

	for (i = 0; i < nThreads; i++)
	{
		threadParams.index = i;

		hThread[i] = CreateThread (NULL, 0, TestThreadProc, &threadParams, 0, &dwThreadId[i]);


		if (hThread[i])
		{
			if (WAIT_OBJECT_0 != WaitForSingleObject(g_hThreadCreate, THREAD_CREATE_TIMEOUT))
			{
				LogFail(_T("Time-out on thread %i creation!"), i);

				DebugBreak();

				hThread[i] = NULL;
			}
			else
			{
				nRunningThreads++;
			}

			LogVerbose (_T("Thread %i (0x%x) successfully created"), i, hThread[i]);
		}
		else
		{
			LogFail (_T("Failed to create thread %i! Error = %d"), i, GetLastError());
		}
	}

	// Main thread is now the timer

	// Wake up every 30 secs to see if any thread aborted

	count = 0;
	while ( CheckTime(mp.dwDuration, dwStartTime) )
	{
		Sleep(30 * 1000);

		// Report results every 10 minutes
		if ((count % 10) == 0)
		{
			EnterCriticalSection(&g_csResults);

				ReportResults (&results);

			LeaveCriticalSection(&g_csResults);
		}
		count++;


		// See if we've been asked to quit
		if (WaitForSingleObject(g_hTimesUp, 1) == WAIT_OBJECT_0)
		{
			retValue = CESTRESS_ABORT;
			break;
		}
	}

	// Signal the worker threads that they must finish up
	SetEvent (g_hTimesUp);


	///////////////////////////////////////////////////////
	// Clean-up, report results, and exit
	//
	
	
	// Wait for all of the workers to return.
	
	LogComment (_T("Time's up signaled.  Waiting for threads to finish ..."));

	while (nExited < nRunningThreads)
	{
		for (n = 0; n < nThreads; n++)
		{
			if ( hThread[n] )
			{
				// Do not exit with running threads, and do not kill threads.
				// If we hang here, so be it.  The harness will detect and 
				// handle the hang according to user preferences.

				if (WAIT_OBJECT_0 == WaitForSingleObject (hThread[n], INFINITE)) // THREAD_WAIT_INTERVAL))
				{
					LogVerbose (_T("Thread %i (0x%x) finished"), n, hThread[n]);

					CloseHandle(hThread[n]);
					hThread[n] = NULL;
					nExited++;
				}
			}
		}
	}

	DeleteCriticalSection (&g_csResults);


	dwExitCode = g_pfnTerminateStress();

	dwEnd = GetTickCount();
	LogVerbose (_T("Leaving at %d, duration (ms) = %d, (sec) = %d, (min) = %d"),
					dwEnd,
					dwEnd - dwStartTime,
					(dwEnd - dwStartTime) / 1000,
					(dwEnd - dwStartTime) / 60000
					);

	
cleanup:

	ReportResults (&results);

	if (g_hTimesUp)
		CloseHandle(g_hTimesUp);

	if (g_hThreadCreate)
		CloseHandle(g_hThreadCreate);

	if (hThread)
		delete [] hThread;

	if (dwThreadId)
		delete [] dwThreadId;

	if (hDll)
		FreeLibrary(hDll);

	return retValue;
}


