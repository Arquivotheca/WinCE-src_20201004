//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

/*******************************************************************
 *
 *    Description: 
 *		This is a test that tests that the schedular is working properly.
 *		After making appropriate changes to use the variable tick scheduler, 
 *		this test is still passing, then everything has been implemented correctly. 
 *		This test creates a bunch of threads of differnt piorities. These threads each 
 *		sleeps in a loop and wakes up to check if was woken up when it expected.
 *		For the higher priority threads this difference should be non existent to very 
 *		very small but for the lower priority threads this differnce can be higher if 
 *		there are more threads competing for scheduler time.
 *		
 *	How to use:
 *		This test runs with the following defaults if no parameters are given:
 *			No. of threads to run at once					:100
 *			Max no. of errors to tolerate before test stops	:20
 *			Highest priority to be given to a thread			:160
 *			Lowest priority to be given to a thread			:255
 *			Max time to sleep for						:10
 *			Test run time (ms)							:300000
 *			Print error messages						:YES
 *			Run continuously							:NO
 *
 *		You can pass in the following information via command line to the tux dll:
 *			/e:num		maximum number of errors
 *			/h:num		highest prio thread
 *			/l:num		lowest prio thread
 *			/n:num		number of threads
 *			/r:num		sleep range
 *			/t:num		run time
 *			/q			quiet mode (no dbg messages)
 *			/c			continuous running
 *
 *    (fkardar)
 *
 *    History: 
 *			2004: Started
 *			2004: Finished
 *
 *******************************************************************/

/*************************** include files *************************/

#include <windows.h>
#include "harness.h"
#include "ksched.h"

/************************* local definitions ***********************/

#define Debug LogDetail

#define LOG(x)	Debug(TEXT("%s == %d\n"), TEXT(#x), x)
#define LOGX(x)	Debug(TEXT("%s == 0x%08x\n"), TEXT(#x), x)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define SLEEP_TICK 1000

#define STORE_SIZE  (256*1024)

/*************************** public data ***************************/

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO ssi;

/************************** private data ***************************/

volatile BOOL gfContinue = FALSE;

DWORD gdwNumThreads = DEFAULT_NUM_THREADS;
DWORD gdwHighestPrio = DEFAULT_HIGHEST_PRIO;
DWORD gdwLowestPrio = DEFAULT_LOWEST_PRIO;
DWORD gdwRunTime = DEFAULT_RUN_TIME;
DWORD gdwMaxErrors = MAX_NUM_ERRORS;
DWORD gdwSleepRange = DEFAULT_SLEEP_RANGE;

DWORD gnErrors, gnWarnings, gnShortSleep;
DWORD gdwMaxWarningDev = 0;

BOOL gfQuiet = FALSE;
BOOL gfContinuous = FALSE;

DWORD ThreadHasRun [512] = { 0 };
DWORD ThreadPrio [512] = { 0 };

//******************************************************************************
//***** ShellProc()
//******************************************************************************

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) {

   switch (uMsg) {

      case SPM_LOAD_DLL: {
         // Sent once to the DLL immediately after it is loaded.  The DLL may
         // return SPR_FAIL to prevent the DLL from continuing to load.
         PRINT(TEXT("ShellProc(SPM_LOAD_DLL, ...) called\r\n"));
		 SPS_LOAD_DLL* spsLoadDll = (SPS_LOAD_DLL*)spParam;
		spsLoadDll->fUnicode = TRUE; 
	  CreateLog( TEXT("KSCHED"));		 
         return SPR_HANDLED;
      }

      case SPM_UNLOAD_DLL: {
         // Sent once to the DLL immediately before it is unloaded.
         PRINT(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called\r\n"));
	  CloseLog();
         return SPR_HANDLED;
      }

      case SPM_SHELL_INFO: {
         // Sent once to the DLL immediately after SPM_LOAD_DLL to give the 
         // DLL some useful information about its parent shell.  The spParam 
         // parameter will contain a pointer to a SPS_SHELL_INFO structure.
         // This pointer is only temporary and should not be referenced after
         // the processing of this message.  A copy of the structure should be
         // stored if there is a need for this information in the future.
         PRINT(TEXT("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));
         ssi = *(LPSPS_SHELL_INFO)spParam;
         return SPR_HANDLED;
      }

      case SPM_REGISTER: {
         // This is the only ShellProc() message that a DLL is required to 
         // handle.  This message is sent once to the DLL immediately after
         // the SPM_SHELL_INFO message to query the DLL for it’s function table.
         // The spParam will contain a pointer to a SPS_REGISTER structure.
         // The DLL should store its function table in the lpFunctionTable 
         // member of the SPS_REGISTER structure return SPR_HANDLED.  If the
         // function table contains UNICODE strings then the SPF_UNICODE flag
         // must be OR'ed with the return value; i.e. SPR_HANDLED | SPF_UNICODE
         // MyPrintf(TEXT("ShellProc(SPM_REGISTER, ...) called\r\n"));
         ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
         #ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
         #else
            return SPR_HANDLED;
         #endif
      }

      case SPM_START_SCRIPT: {
         // Sent to the DLL immediately before a script is started.  It is 
         // sent to all DLLs, including loaded DLLs that are not in the script.
         // All DLLs will receive this message before the first TestProc() in
         // the script is called.
         // MyPrintf(TEXT("ShellProc(SPM_START_SCRIPT, ...) called\r\n"));
         return SPR_HANDLED;
      }

      case SPM_STOP_SCRIPT: {
         // Sent to the DLL when the script has stopped.  This message is sent
         // when the script reaches its end, or because the user pressed 
         // stopped prior to the end of the script.  This message is sent to
         // all DLLs, including loaded DLLs that are not in the script.
         // MyPrintf(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called\r\n"));
         return SPR_HANDLED;
      }

      case SPM_BEGIN_GROUP: {
         // Sent to the DLL before a group of tests from that DLL is about to
         // be executed.  This gives the DLL a time to initialize or allocate
         // data for the tests to follow.  Only the DLL that is next to run
         // receives this message.  The prior DLL, if any, will first receive
         // a SPM_END_GROUP message.  For global initialization and 
         // de-initialization, the DLL should probably use SPM_START_SCRIPT
         // and SPM_STOP_SCRIPT, or even SPM_LOAD_DLL and SPM_UNLOAD_DLL.
         // MyPrintf(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n"));
         return SPR_HANDLED;
      }

      case SPM_END_GROUP: {
         // Sent to the DLL after a group of tests from that DLL has completed
         // running.  This gives the DLL a time to cleanup after it has been 
         // run.  This message does not mean that the DLL will not be called
         // again; it just means that the next test to run belongs to a 
         // different DLL.  SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
         // to track when it is active and when it is not active.
         // MyPrintf(TEXT("ShellProc(SPM_END_GROUP, ...) called\r\n"));
         return SPR_HANDLED;
      }

      case SPM_BEGIN_TEST: {
         // Sent to the DLL immediately before a test executes.  This gives
         // the DLL a chance to perform any common action that occurs at the
         // beginning of each test, such as entering a new logging level.
         // The spParam parameter will contain a pointer to a SPS_BEGIN_TEST
         // structure, which contains the function table entry and some other
         // useful information for the next test to execute.  If the ShellProc
         // function returns SPR_SKIP, then the test case will not execute.
         // MyPrintf(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called\r\n"));
         LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
         return SPR_HANDLED;
      }

      case SPM_END_TEST: {
         // Sent to the DLL after a single test executes from the DLL.
         // This gives the DLL a time perform any common action that occurs at
         // the completion of each test, such as exiting the current logging
         // level.  The spParam parameter will contain a pointer to a 
         // SPS_END_TEST structure, which contains the function table entry and
         // some other useful information for the test that just completed.
         // MyPrintf(TEXT("ShellProc(SPM_END_TEST, ...) called\r\n"));
         LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;

         return SPR_HANDLED;
      }

      case SPM_EXCEPTION: {
         // Sent to the DLL whenever code execution in the DLL causes and
         // exception fault.  TUX traps all exceptions that occur while
         // executing code inside a test DLL.
         // MyPrintf(TEXT("ShellProc(SPM_EXCEPTION, ...) called\r\n"));
         return SPR_HANDLED;
      }
   }

   return SPR_NOT_HANDLED;
}

//******************************************************************************
//***** TestProc()'s
//******************************************************************************

//////////////////////////////////////////////////////////////////////////////
//	This function tests that GetTickCount() is working as expected. Our next test runs on the 
//	assumption that it is. Incorrectly setting up the variable tick scheduler can cause this 
//	test to fail. If this test fails, the results of the next test should be disregarded.
//////////////////////////////////////////////////////////////////////////////

TESTPROCAPI 
TickTest(
	UINT uMsg, 
	TPPARAM tpParam, 
	LPFUNCTION_TABLE_ENTRY lpFTE
	) 
{
	HANDLE hTest=NULL, hStep=NULL;
	DWORD dwTime1 = 0, dwTime2 = 0;
	SYSTEMTIME st1 = {0} , st2 = {0};
	DWORD dwCountDiff = 0, dwRealDiff = 0;

 	if (uMsg != TPM_EXECUTE) {
		return TPR_NOT_HANDLED;
	}
	   
	hTest = StartTest( TEXT("Tick Test"));

	BEGINSTEP( hTest, hStep, TEXT("Initialization"));

	//give ourselves the highest priority	
	CHECKTRUE(SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_HIGHEST));
	LOG(GetThreadPriority (GetCurrentThread ()));

	ENDSTEP( hTest, hStep);
	
	BEGINSTEP( hTest, hStep, TEXT("Test that tick does not go backwards, and that real time and tick count are in sync."));

	CHECKTRUE(GetRealTime(&st1));

	dwTime1 = GetTickCount();

	Sleep (SLEEP_TICK);

	dwTime2 = GetTickCount();

	CHECKTRUE(GetRealTime(&st2));

	Debug(TEXT("Initial GetTickCount( ) : %d"), dwTime1);
	Debug(TEXT("End GetTickCount( ) : %d"), dwTime2);

	DumpSystemTime(&st1, TEXT("Initial GetRealTime( ) : "));
	DumpSystemTime(&st2, TEXT("End GetRealTime( ) : "));		

	// tick count should never be negative

	CHECKTRUE((dwTime1 >0));

	CHECKTRUE((dwTime2 >0));

	//tick count should not be going backwards

	dwCountDiff = dwTime2 - dwTime1; 

	CHECKTRUE((dwCountDiff > 0));

	//tick count and sleep should be in sync, the diff can be no more than 1 ms

	CHECKTRUE(((dwCountDiff - SLEEP_TICK) < 3));	

	//tick and real time should be in sync

	dwRealDiff = ElapsedTime(&st1, &st2);

	//elapsed time should be valid

	CHECKTRUE(dwRealDiff);

	//elapsed time should be in sync with sleep

	CHECKTRUE(((dwRealDiff - SLEEP_TICK) < 3));

	/*elapsed time should be in sync with tick count, the diff can be no more than 1 ms
	GetTickCount() has millisec resolution but GetRealTime() has second resolution 
	(unless the device hardware supports millisec or the millisec feature is turned on
	explicitly)*/

	CHECKTRUE(((dwRealDiff == dwCountDiff)||(dwRealDiff==(dwCountDiff+1))||(dwRealDiff==(dwCountDiff-1))));

	ENDSTEP( hTest, hStep);

	return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;	

}

/////////////////////////////////////////////////////////////////////////////////////
// This is the main body of the test. Here we create a bunch of threads of varying priorities 
// and wait for them to finish execution
/////////////////////////////////////////////////////////////////////////////////////


TESTPROCAPI 
SchedTest(
	UINT uMsg, 
	TPPARAM tpParam, 
	LPFUNCTION_TABLE_ENTRY lpFTE
	) 
{
	HANDLE hTest=NULL, hStep=NULL;
	LPHANDLE hThreads = NULL;
	DWORD dwThreadId = 0;
	DWORD dwPrio = 0;
	DWORD i = 0;
	DWORD dwStorePages = 0, dwOldStorePages = 0;
	DWORD dwRamPages = 0;
	DWORD dwPageSize = 0;
	DWORD dwNumThreadsRun = 0;
	WCHAR* szCmdLine = NULL;
	DWORD dwSize = 0;

 	if (uMsg != TPM_EXECUTE) {
		return TPR_NOT_HANDLED;
	}
	   
	hTest = StartTest( TEXT("Scheduler Test"));

	BEGINSTEP( hTest, hStep, TEXT("Initialization"));

	//extract the command line parameters passed to us from tux

	dwSize = wcslen(ssi.szDllCmdLine);
	szCmdLine = new WCHAR [dwSize + 1];
	wcscpy(szCmdLine, ssi.szDllCmdLine);

	//parse the command line

	if (!ParseCmdLine (szCmdLine)) {
		Debug (TEXT("PASS : Scheduler Test\n"));
		return TPR_PASS;
	}

	//changing memory distribution between the object store and program memory
	//so that we can get the max memory possible, reduceing chances to OOM

	GetSystemMemoryDivision (& dwOldStorePages, & dwRamPages, & dwPageSize);
	dwStorePages = STORE_SIZE / dwPageSize;
	while (SYSMEM_FAILED == SetSystemMemoryDivision (dwStorePages))
		dwStorePages++;

	Debug (TEXT("Reset dwStorePages to %u\n"), dwStorePages);

	//give ourselves the highest priority
	
	CHECKTRUE(CeSetThreadPriority (GetCurrentThread (), gdwHighestPrio));
	LOG(CeGetThreadPriority (GetCurrentThread ()));

	//allocate memory to store all the thread information

	hThreads = (LPHANDLE) malloc (gdwNumThreads * sizeof (HANDLE));

	CHECKTRUE(hThreads);

	//we want the threads to continue running until we tell them

	gfContinue = TRUE;

	ENDSTEP( hTest, hStep);

	BEGINSTEP( hTest, hStep, TEXT("Create Threads"));

	Debug(TEXT("Creating %u threads...\n"), gdwNumThreads);
	Debug (TEXT("Highest prio: %u\n"), gdwHighestPrio);

	//we are creating all the threads as suspended and assigning everyone a random priority
	//based on the priority range provided to us. The last thread we create has the highest priority.
	
	for (i = 0; i < gdwNumThreads; i++) {
		hThreads[i] = CreateThread (NULL, 0, ThreadProc, (LPVOID) i, CREATE_SUSPENDED, & dwThreadId);
		if (hThreads[i] == INVALID_HANDLE_VALUE) {
			Debug (TEXT("Unable to create thread %u\n"), i);
			gdwNumThreads = i - 1;
			CHECKTRUE(FALSE);
		}
		dwPrio = (i == gdwNumThreads - 1) ? EDBG_PRIO - 1 : rand_range (gdwHighestPrio + 1, gdwLowestPrio);
		ThreadPrio [i] = dwPrio;
		CHECKTRUE(CeSetThreadPriority (hThreads[i], dwPrio));
		Debug (TEXT("Created thread %u [0x%08x - %u]\n"), i, hThreads[i], dwPrio);
	}

	Debug (TEXT("Resuming threads...\n"));

	for (i = 0; i < gdwNumThreads; i++) {
		ResumeThread (hThreads[i]);
	}
	Debug(TEXT("Done resuming threads. Waiting for test to complete...\n"));

	ENDSTEP( hTest, hStep);

	BEGINSTEP( hTest, hStep, TEXT("Running the test"));

	//sleep for a bit and then wake up an wait on threads, this call will return if our highest priority thread
	//stops or when the test run time is exceded. If we had set it to run continuously, it will only return 
	//if our highest priority thread ends

	Sleep (1000);
	WaitForSingleObject (hThreads[gdwNumThreads-1], gfContinuous ? INFINITE : gdwRunTime);

	//now signal all the other threads to stop
	
	gfContinue = FALSE;

	ENDSTEP( hTest, hStep);

	BEGINSTEP( hTest, hStep, TEXT("Wait for threads to finish"));
	
	Debug (TEXT("Waiting for threads to terminate...\n"));

	//waiting for all the threads to finish execution

	for (i = 0; i < gdwNumThreads; i++) {
		CHECKTRUE((WaitForSingleObject (hThreads[i], 10000)==WAIT_OBJECT_0));
		CHECKTRUE(CloseHandle (hThreads[i]));
	}

	LOG (gnShortSleep);
	LOG (gnErrors);
	LOG (gnWarnings);
	LOG (gdwMaxWarningDev);

	//print out stats about all the threads

	for (i = 0; i < gdwNumThreads; i++) {
		if (ThreadHasRun [i]) {
			Debug (TEXT("ThreadHasRun [%u:%u]: %u\n"), i, ThreadPrio [i], ThreadHasRun [i]);
			dwNumThreadsRun++;
		}
	}

	LOG(dwNumThreadsRun);

	ENDSTEP( hTest, hStep);

	BEGINSTEP( hTest, hStep, TEXT("Cleanup"));

	Debug (TEXT("Total Errors:%u\n"), gnShortSleep + gnErrors);

	//free memory before checking for failure as we want this to happen even if we fail

	free (hThreads);

	delete []szCmdLine;

	//revert memory to its original configuration

	SetSystemMemoryDivision(dwOldStorePages);

	//test fails if there were any errors or if we ever woke up sooner than expected
	
	CHECKFALSE(gnErrors);

	CHECKFALSE(gnShortSleep)

	ENDSTEP( hTest, hStep);

	return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;	
}

/************************ private functions *************************/

/////////////////////////////////////////////////////////////////////
//Helper function, dumps the time onto the debug console
////////////////////////////////////////////////////////////////////

void DumpSystemTime( LPSYSTEMTIME lpSysTime, LPTSTR szHeading )
{
	Debug (TEXT("%s "), szHeading);
	Debug (TEXT("%02u-%02u-%02u  %02u:%02u:%02u:%02u \n"),
		lpSysTime->wMonth, lpSysTime->wDay, lpSysTime->wYear,
        lpSysTime->wHour, lpSysTime->wMinute, lpSysTime->wSecond, lpSysTime->wMilliseconds);

}

///////////////////////////////////////////////////////////////////////
//Helper function, calculated how much time passed between two System Times
//////////////////////////////////////////////////////////////////////

INT ElapsedTime (SYSTEMTIME *pst1, SYSTEMTIME *pst2)
{
    INT nElapsedSeconds = 0;

    nElapsedSeconds = pst2->wMilliseconds - pst1->wMilliseconds;
	
    if (pst2->wSecond!= pst1->wSecond) {
        nElapsedSeconds += ((pst2->wSecond - pst1->wSecond) * 1000);
    }

    if (pst2->wMinute != pst1->wMinute) {
        nElapsedSeconds += ((pst2->wMinute - pst1->wMinute) * 60 * 1000);
    }
    if (pst2->wHour != pst1->wHour) {
        nElapsedSeconds += ((pst2->wHour - pst1->wHour) * 60 * 60 * 1000);
    }
    if (pst2->wDay != pst1->wDay) {
	nElapsedSeconds += ((pst2->wDay - pst1->wDay) * 60 * 60 * 24 * 1000);
    }

    if (pst2->wMonth != pst1->wMonth) {
	nElapsedSeconds += ((pst2->wMonth - pst1->wMonth) * 60 * 60 * 24 * pst1->wDay * 1000);
    }

    if (pst2->wYear != pst1->wYear) {
	nElapsedSeconds += ((pst2->wYear - pst1->wYear) * 60 * 60 * 24 * 365 * 1000);
    }


    LogDetail (_T("%d milli seconds elapsed from %02u-%02u-%02u  %02u:%02u:%02u:%02u to %02u-%02u-%02u  %02u:%02u:%02u:%02u\n"),
        nElapsedSeconds,
        pst1->wMonth, pst1->wDay, pst1->wYear, pst1->wHour, pst1->wMinute, pst1->wSecond, pst1->wMilliseconds,
        pst2->wMonth, pst2->wDay, pst2->wYear, pst2->wHour, pst2->wMinute, pst2->wSecond, pst2->wMilliseconds);

    return nElapsedSeconds;
}


//////////////////////////////////////////////////////
//calculates a random number in between the range given
/////////////////////////////////////////////////////

int rand_range (int nLowest, int nHighest)
{
	return rand () % (nHighest - nLowest) + nLowest;
}

//////////////////////////////////////////////////////////////////
//	parses the command line and extracts all the information
//////////////////////////////////////////////////////////////////

BOOL ParseCmdLine (LPWSTR lpCmdLine)
{
	WCHAR* ptChar = NULL;
	WCHAR szSeps[] = TEXT(" \r\n");

	//in the option was "-h", just print help and return

	if ( wcsstr (lpCmdLine, TEXT("-h"))) {
		Debug (TEXT("%s"), gszHelp);
		return FALSE;
	}

	//else extract all the options (there can be more than one at a time)

	ptChar = wcstok (lpCmdLine, szSeps);
	while (ptChar) {
		Debug (TEXT("Token: %s\n"), ptChar);
		if (ptChar[0] == '/') {
			switch (ptChar[1]) {
				case _T('c'):
					gfContinuous = TRUE;
					break;
				case _T('e'):
					gdwMaxErrors = _ttol (ptChar + 3);
					break;					
				case _T('h'):
					gdwHighestPrio = _ttol (ptChar + 3);
					break;
				case _T('l'):
					gdwLowestPrio = _ttol (ptChar + 3);
					break;
				case _T('n'):
					gdwNumThreads = _ttol (ptChar + 3);
					break;
				case _T('q'):
					gfQuiet = TRUE;
					break;
				case _T('r'):
					gdwSleepRange = _ttol (ptChar + 3);
					break;
				case _T('t'):
					gdwRunTime = _ttol (ptChar + 3);
					break;
				default:
					break;
			}
		}			
		
		ptChar = wcstok (NULL, szSeps);
	}

	//print the options we will be running under

	Debug (TEXT("Run time variables:\n"));
	Debug (TEXT("   gdwNumThreads: %u\n"), gdwNumThreads);
	Debug (TEXT("   gdwMaxErrors: %u\n"), gdwMaxErrors);
	Debug (TEXT("   gdwHighestPrio: %u\n"), gdwHighestPrio);
	Debug (TEXT("   gdwLowestPrio: %u\n"), gdwLowestPrio);
	Debug (TEXT("   gdwSleepRange: %u\n"), gdwSleepRange);
	Debug (TEXT("   gdwRunTime: %u\n"), gdwRunTime);
	Debug (TEXT("   error messages: %s\n"), gfQuiet ? _T("NO") : _T("YES"));
	Debug (TEXT("   continuous run: %s\n"), gfContinuous ? _T("YES") : _T("NO"));
	
	return TRUE;
}

//////////////////////////////////////////////////////////////////
//	Thread proc for all the threads running
//////////////////////////////////////////////////////////////////

DWORD WINAPI ThreadProc (LPVOID lpvParam)
{
	DWORD dwCount1 = 0;
	DWORD dwCount2 = 0;
	DWORD dwSleepTime = 0;
	DWORD dwThreadNum = (DWORD) lpvParam;
	DWORD dwUS = 0;

	while (gfContinue) {
		
		//add to the number of times this thread has run
		ThreadHasRun[dwThreadNum]++;

		//calculate the time for which we will sleep
		dwSleepTime = rand () % gdwSleepRange + 1;

		//sleep and check the time before and after
		dwCount1 = GetTickCount();		
		Sleep (dwSleepTime);
		dwCount2 = GetTickCount();

		//calculate the difference
		dwUS = dwCount2 - dwCount1;

		//you should not wake up before the sleep time is complete
		if (dwUS < dwSleepTime) {
			InterlockedIncrement ((LPLONG)&gnShortSleep);
		}
        	// Only check Sleep () accuracy if we are the highest prio thread, we only guarentee that we will wake up
        	// in the exact time if we are the highest priority thread in the OS
		if (dwThreadNum == gdwNumThreads - 1) {
			if (dwUS > (dwSleepTime + 2) ) {
				if (!gfQuiet) {
					Debug (TEXT("dwSleepTime: %3u   dwUS: %u MyPriority: %d \n"), dwSleepTime, dwUS, CeGetThreadPriority(GetCurrentThread()));
				}
				gnErrors ++;
			} else if (dwUS > (dwSleepTime + 1) ) {
				//consider this a warning because a difference of 1 ms can happen just in context switches and such
				gnWarnings ++;
				gdwMaxWarningDev = MAX(dwUS, gdwMaxWarningDev);
			}
			//if we have exceeded the number of errors then we need to stop as the test has failed
			if ((gnErrors + gnShortSleep > gdwMaxErrors) && gdwMaxErrors) {
				Debug (TEXT("Hit too many errors. Aborting.\n"));
				gfContinue = FALSE;
			}
		}
	}

	return 0;
}

