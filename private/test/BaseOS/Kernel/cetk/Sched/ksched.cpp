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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

/*****************************************************************************
 *
 *    Description:
 *    ============
 *
 *      This is a test of the Kernel scheduler and realtime behaviour of the
 *      system. In conjunction, the BSP timer and Real time clock (RTC) are
 *      exercised.
 *
 *      There are 2 tests in this suite:
 *
 *      (1) TickTest. This is a simple test of sleep, and system clocks.
 *      The test sets itself to a high realtime priority (default is 1),
 *      such that we should be unaffected by other threads on the system
 *      (For this test, ideally no threads should be running at a higher
 *      priority).
 *      We check that the behaviour of Sleep is consistent with the timer RTC
 *      and we test that the timer ticking is consistent with the system RTC
 *      (skew between timer and RTC should be small).
 *
 *      (2) SchedTest. This is a more complicated test of the scheduler as a
 *      system. The test creates a bunch of threads of different priorities
 *      which enter a loop of sleeping for random amounts of time:
 *          time1 = GetTickCount();
 *          Sleep( sleep_time );
 *          time2 = GetTickCount();
 *
 *      For the thread of highest priority, (there is only one), we check that
 *      Sleep is consistent with the system timer; i.e. the following assertion
 *      holds:
 *          sleep_time < (time2 - time1) <= sleep_time + tolerance
 *      Where tolerance is a tightly bounded amount of time for overhead of the
 *      Sleep and GetTickCount api calls and margin of error for the timer and
 *      some lenience to let the test run when other high priority (i.e. driver)
 *      threads are running on the system.
 *
 *    Important notes:
 *    ===============
 *
 *      This test makes a few assumptions about the system:
 *
 *      (a) The test is running at the highest priority on the system
 *          - Default is real-time priority 1.
 *          - Can be changed with /h flag
 *      (b) Running on a retail image.
 *          - We do not guarantee realtime behaviour on DEBUG code (this test
 *            will generally pass on a DEBUG image, however)
 *      (c) Debugger is not enabled
 *          - Debugger can affect real-time behaviour (set IMGNODEBUGGER=1)
 *      (d) System does not purposefully break realtime behaviour
 *          - For performance / simplicity, some systems purposefully break
 *            realtime constraints in BSP/driver code. This test may not pass
 *            under such conditions
 *            It is optimal to run the test from the device, without KITL.
 *            This test runs at a priority higher than the KITL default, so
 *            it should usually be unaffected by KITL.
 *
 *      Behaviour of the test may vary from run to run:
 *        - Because the behaviour of the test can depend on what else is
 *          running on the system, this test does not behave deterministically.
 *          Timing results can vary from one run to the next (though the test
 *          should consistently pass on systems meeting the above criteria)
 *
 *
 *      Different systems can have different acceptable tolerances. The test
 *      has a default tolerance of 2ms, which is generally appropriate. For
 *      some systems, a higher tolerance may be required. You should not see
 *      situations where the actual and expected sleep time is off by hundreds
 *      of milliseconds.
 *
 *
 *    How to use:
 *    ==========
 *      Basic: s tux -o -d ksched.dll (uses default values)
 *      Advanced: s tux -o -d ksched.dll -c"args"
 *      This test accepts a number of different command line parameters
 *      which affect the behaviour of the test. Most parameters only affect
 *      SchedTest. Without parameters, the test uses the following defaults:
 *
 *          No. of threads to run at once                       :100
 *          Max no. of errors to tolerate before test stops     :20
 *          Highest priority to be given to a thread            :5
 *          Lowest priority to be given to a thread             :255
 *          Max time to sleep for                               :10
 *          Test run time (ms)                                  :300000
 *          Print error messages                                :YES
 *          Run continuously                                    :NO
 *
 *      You can pass in the following information via command line to the
 *      tux dll:
 *          /e:num      maximum number of errors before we quit
 *          /h:num      highest prio thread (0-255)
 *          /l:num      lowest prio thread (0-255)
 *          /n:num      number of threads (0-1024)
 *          /s:num      upper bound of thread sleep time (in ms)
 *          /r:num      run time (in ms)
 *          /t:num      tolerance time (in ms)
 *          /c          run test continuously until failure (bounded by /e:)
 *
 *    History:
 *          2004: Started
 *          2004: Finished
 *          2006: Minor updates
 *          2007: Overhaul
 *
 *****************************************************************************/

/******************************* include files *******************************/

#include <windows.h>
#include "kharness.h"
#include "ksched.h"

volatile long g_errCnt = 0 ;


/*************************** public data ***************************/

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO ssi;

/************************** private data ***************************/

volatile BOOL gfContinue = FALSE;

DWORD gdwNumThreads     = DEFAULT_NUM_THREADS;
DWORD gdwHighestPrio    = DEFAULT_HIGHEST_PRIO;
DWORD gdwLowestPrio     = DEFAULT_LOWEST_PRIO;
DWORD gdwRunTime        = DEFAULT_RUN_TIME;
DWORD gdwMaxErrors      = DEFAULT_MAX_ERRORS;
DWORD gdwSleepRange     = DEFAULT_SLEEP_RANGE;
DWORD gdwToleranceTime  = DEFAULT_TOLERANCE_TIME;

BOOL gfContinuous       = FALSE;
DWORD gnErrors          = 0;

PTHREADINFO pThreadInfo = NULL;
PSLEEPERRINFO pSleepErrInfo = NULL;

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
         CreateLog( TEXT("KSched"));

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
         if (!ParseCmdLine( (LPTSTR)(ssi.szDllCmdLine) ) ) {
             LogDetail( TEXT("Error parsing command line\r\n"));
             LogDetail (TEXT("%s"), gszHelp);
             return SPR_FAIL;
         }

         return SPR_HANDLED;
      }

      case SPM_REGISTER: {
         // This is the only ShellProc() message that a DLL is required to
         // handle.  This message is sent once to the DLL immediately after
         // the SPM_SHELL_INFO message to query the DLL for it's function table.
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
//  This function tests that GetTickCount() is working as expected. Our next
//  test runs on the assumption that it is. Incorrectly setting up the variable
//  tick scheduler can cause this test to fail. If this test fails, the results
//  of the next test should be disregarded.
//////////////////////////////////////////////////////////////////////////////

TESTPROCAPI
TickTest(
    UINT uMsg,
    TPPARAM /*tpParam*/,
    LPFUNCTION_TABLE_ENTRY /*lpFTE*/
    )
{
    HANDLE hTest=NULL, hStep=NULL;
    DWORD dwTime1 = 0, dwTime2 = 0;
    SYSTEMTIME st1 = { 0 } , st2 = { 0 };
    DWORD   dwTimerDiff = 0;
    long long qwRealDiff = 0;
    DWORD dwOldPrio = 0;

    if (uMsg != TPM_EXECUTE) {
        return TPR_NOT_HANDLED;
    }

    hTest = StartTest( TEXT("Tick Test"));

    BEGINSTEP( hTest, hStep, TEXT("Initialization"));

    // Give ourself the highest priority
    dwOldPrio = CeGetThreadPriority( GetCurrentThread() );
    CHECKTRUE( THREAD_PRIORITY_ERROR_RETURN != dwOldPrio );
    CHECKTRUE( CeSetThreadPriority (GetCurrentThread (), gdwHighestPrio ));
    CHECKTRUE( gdwHighestPrio == (DWORD)CeGetThreadPriority( GetCurrentThread() ) );
    LogDetail( TEXT("Set thread priority to %d\r\n"), CeGetThreadPriority (GetCurrentThread ()) );

    ENDSTEP( hTest, hStep);

    BEGINSTEP( hTest, hStep, TEXT("Test that sleep functionality behaves correctly according to system timer and realtime clock\r\n") );

    // Calibrate to start right after tick
    Sleep( 1 );
    CHECKTRUE(GetRealTime(&st1));

    dwTime1 = GetTickCount();

    Sleep (SLEEP_TIME);

    dwTime2 = GetTickCount();

    CHECKTRUE(GetRealTime(&st2));

    LogDetail(TEXT("Initial GetTickCount( ) : %d"), dwTime1);
    LogDetail(TEXT("End GetTickCount( ) : %d"), dwTime2);

    DumpSystemTime(&st1, TEXT("Initial GetRealTime( ) : "));
    DumpSystemTime(&st2, TEXT("End GetRealTime( ) : "));

    // Sanity check: Make sure dwTime1 and dwTime2 are not both 0
    // (one could be zero at rollover)
    CHECKTRUE( dwTime1 || dwTime2 );

    // Check that tick count does not go backwards
    // Note: math should handle wrap-around
    dwTimerDiff = dwTime2 - dwTime1;
    qwRealDiff = ElapsedTimeInms(&st1, &st2);

    LogPrint( TEXT("Timer delta: %dms | RTC delta: %dms\r\n"), dwTimerDiff, qwRealDiff );

    if( (int)dwTimerDiff < 0 ) {
        LogDetail( TEXT("ERROR: Consecutive measure of the timer via GetTickCount()\r\n") );
        LogDetail( TEXT("       are decreasing. Timer appears to be moving backwards\r\n"));
    }
    CHECKTRUE(((int)dwTimerDiff > 0));

    // Check that we sleep the expected time according to the timer
    // 1. Sleep at least as long as requested
    if( dwTimerDiff < SLEEP_TIME ) {
        LogDetail( TEXT("ERROR: Timer ticked less than sleep time\r\n") );
        LogDetail( TEXT("       Slept for %dms; timer ticked %dms\r\n"), SLEEP_TIME, dwTimerDiff );
    }
    CHECKTRUE( dwTimerDiff >= SLEEP_TIME );

    // 2. No more than a few ms for overhead
    if( dwTimerDiff >= SLEEP_TIME + gdwToleranceTime ) {
        LogDetail( TEXT("ERROR: Timer ticked more than sleep time\r\n") );
        LogDetail( TEXT("       Slept for %dms; timer ticked %dms\r\n"), SLEEP_TIME, dwTimerDiff );
    }
    CHECKTRUE( dwTimerDiff < SLEEP_TIME + gdwToleranceTime );

    // Check that tick and real time clock (RTC) should be in sync
    // ElapsedTime returns the time in ms between the 2 readings of the RTC

    // Check that we sleep the expected time according to the RTC
    // 1. Sleep at least as long as requested
    if( qwRealDiff < SLEEP_TIME ) {
        LogDetail( TEXT("ERROR: RealTime clock ticked less than sleep time\r\n") );
        LogDetail( TEXT("       Slept for %dms; RTC ticked %dms\r\n"), SLEEP_TIME, qwRealDiff );
    }
    CHECKTRUE( qwRealDiff >= SLEEP_TIME );

    // 2. No more than a few ms for overhead
    if( qwRealDiff >= SLEEP_TIME + gdwToleranceTime ) {
        LogDetail( TEXT("ERROR: RealTime clock ticked more than sleep time\r\n") );
        LogDetail( TEXT("       Slept for %dms; RTC ticked %dms\r\n"), SLEEP_TIME, qwRealDiff );
    }
    CHECKTRUE( qwRealDiff < SLEEP_TIME + gdwToleranceTime );

    // Check that RTC and Timer approximately agree
    // Difference between RTC and Timer should be off by no more than 2ms
    // Allow some overhead for jitter / or out of sync
    // But, we only sleep for a short time, so skew should be small
    if( (DWORD)ABS( (int)qwRealDiff - (int)dwTimerDiff ) > gdwToleranceTime ) {
        LogDetail( TEXT("ERROR: Clock skew between timer and realtime clock detected during sleep\r\n") );
        LogDetail( TEXT("       timer ticked %dms and RTC ticked %dms during sleep time of %dms\r\n"), dwTimerDiff, qwRealDiff, SLEEP_TIME );
    }
    CHECKTRUE( ABS( (int)qwRealDiff - (int)dwTimerDiff ) <= 1 );

    ENDSTEP( hTest, hStep);

    //revert to original priority
    CeSetThreadPriority( GetCurrentThread(), dwOldPrio );

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}


///////////////////////////////////////////////////////////////////////////////
// This is the main body of the scheduler test. Here we create a bunch of
// threads of varying priorities and wait for them to finish execution
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI
SchedTest(
    UINT uMsg,
    TPPARAM /*tpParam*/,
    LPFUNCTION_TABLE_ENTRY /*lpFTE*/
    )
{
    HANDLE hTest=NULL, hStep=NULL;
    DWORD dwThreadId = 0;
    DWORD dwPrio = 0;
    DWORD i = 0;
    DWORD dwNumThreadsRun = 0;
    DWORD dwOldPrio = 0;
    int nTimeDiff = 0, nShortSleep = 0, nWarnOverSleep = 0, nErrOverSleep = 0;
    HANDLE hGoEvt = NULL;

    if (uMsg != TPM_EXECUTE) {
        return TPR_NOT_HANDLED;
    }

    hTest = StartTest( TEXT("Scheduler Test"));

    BEGINSTEP( hTest, hStep, TEXT("Initialization"));

    gnErrors = 0;

    hGoEvt = CreateEvent( NULL, TRUE, FALSE, GOEVT );
    CHECKTRUE( hGoEvt );

    // Give ourself the highest priority
    dwOldPrio = CeGetThreadPriority( GetCurrentThread() );
    CHECKTRUE( THREAD_PRIORITY_ERROR_RETURN != dwOldPrio );
    CHECKTRUE( CeSetThreadPriority (GetCurrentThread (), gdwHighestPrio  ));
    CHECKTRUE( gdwHighestPrio == (DWORD)CeGetThreadPriority( GetCurrentThread() ) );
    LogDetail( TEXT("Set driver (current) thread priority to %d\r\n"), CeGetThreadPriority (GetCurrentThread ()) );

    // Allocate memory to store all the thread information

    pThreadInfo = (PTHREADINFO) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, gdwNumThreads * sizeof( THREADINFO ) );

    pSleepErrInfo = (PSLEEPERRINFO) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, gdwMaxErrors * sizeof( SLEEPERRINFO ) );

    if(!pThreadInfo || !pSleepErrInfo) {
        NKDbgPrintfW(TEXT("ERROR: allocating memory for thread information. Is the system out of memory?\r\n") );
    }
    CHECKTRUE(pThreadInfo && pSleepErrInfo);

    ENDSTEP( hTest, hStep);

    BEGINSTEP( hTest, hStep, TEXT("Create Threads"));

    LogDetail(TEXT("Creating %u threads...\r\n"), gdwNumThreads);
    LogDetail(TEXT("Highest prio: %u\r\n"), gdwHighestPrio);

    // We are creating all the threads suspended and assigning everyone
    // a random priority based on the priority range provided to us. The
    // last thread we create has the highest priority.

    for (i = 0; i < gdwNumThreads; i++) {
        pThreadInfo[i].hThread = CreateThread (NULL, 0, SleeperThread, (LPVOID) i, CREATE_SUSPENDED, & dwThreadId);
        if (!pThreadInfo[i].hThread || pThreadInfo[i].hThread == INVALID_HANDLE_VALUE) {
            LogDetail(TEXT("ERROR: creating thread #%u. Out of memory?\r\n"), i);
            break;
        }
        dwPrio = (i == gdwNumThreads - 1) ? (gdwHighestPrio + 1) : rand_range (gdwHighestPrio + 2, gdwLowestPrio);
        pThreadInfo[i].dwThreadPrio = dwPrio;
        if(!CeSetThreadPriority (pThreadInfo[i].hThread, dwPrio)) {
            LogDetail( TEXT("ERROR: setting priority!\r\n") );
            break;
        }
        pThreadInfo[i].dwThreadId = dwThreadId;
        LogDetail (TEXT("Created thread %u [0x%08x - pri=%u]\r\n"), i, pThreadInfo[i].dwThreadId, pThreadInfo[i].dwThreadPrio);
    }
    CHECKTRUE( i == gdwNumThreads );

    LogDetail (TEXT("Finished Creating threads & Setting priority. Now Resuming threads.\r\n"));

    for (i = 0; i < gdwNumThreads; i++) {
            if (0xFFFFFFFF == ResumeThread (pThreadInfo[i].hThread)) {
            LogDetail( TEXT("ERROR: resuming thread %d (0x%x)\r\n"), i, pThreadInfo[i].hThread );
            break;
        }
    }
    CHECKTRUE( i == gdwNumThreads );
    LogDetail(TEXT("Done resuming threads.\r\n"));

    ENDSTEP( hTest, hStep);

    BEGINSTEP( hTest, hStep, TEXT("Running the test"));

    // This is where the core of the test runs.
    // This thread ( the driver thread ) waits on the highest priority
    // SleeperThread.
    // It will stop waiting either when the SleeperThread returns (due to
    // some error) or the timeout expires (if not in continuous mode)
    // Each of the threads is spinning and sleeping until the gfContinue
    // flag gets set to FALSE
    LogDetail (TEXT("Driver thread: waiting (%d seconds) on the others.\r\n"), gdwRunTime/1000);
    LogDetail (TEXT("Note: This test is running at high realtime priority. The system may appear hung until this timeout expires. Please be patient.\r\n") );

    // Threads keep running while this flag is true
    gfContinue = TRUE;

    // NOTE: WE CANNOT USE KITL WHILE SleeperThreads ARE SPINNING
    //       THIS MEANS NO DEBUG OUTPUT (after SetEvent)

    // Signal the threads to go
    SetEvent( hGoEvt );

    WaitForSingleObject (pThreadInfo[gdwNumThreads-1].hThread, gfContinuous ? INFINITE : gdwRunTime);

    // We're done, signal all the other threads to stop
    gfContinue = FALSE;

    // NOTE: WE CAN NOW USE KITL AGAIN
    LogDetail (TEXT("Driver thread awake. Test over.\r\n"));

    // Give threads time to finish

    ENDSTEP( hTest, hStep);

    BEGINSTEP( hTest, hStep, TEXT("Wait for threads to finish"));

    LogDetail (TEXT("Waiting for threads to terminate...\r\n"));

    // Waiting for all the threads to finish execution

    for (i = 0; i < gdwNumThreads; i++) {
        if(!(WaitForSingleObject (pThreadInfo[i].hThread, 10000)==WAIT_OBJECT_0)) {
            LogDetail (TEXT("ERROR: Thread %d did not return in time\r\n"), i);
            break;
        }
        if(!CloseHandle (pThreadInfo[i].hThread)) {
            LogDetail (TEXT("ERROR: Closing thread %d (handle 0x%x) failed with error code 0x%x\r\n"), i, pThreadInfo[i].hThread, GetLastError() );
            break;
        }
        pThreadInfo[i].hThread = 0;
    }
    CHECKTRUE( i == gdwNumThreads );

    if( i == gdwNumThreads ) {
        LogDetail( TEXT("All threads finished gracefully\r\n") );
    }

    ENDSTEP( hTest, hStep );

    BEGINSTEP( hTest, hStep, TEXT("Statistics and realtime constrainst") );

    // Print out stats about all the threads

    for (i = 0; i < gdwNumThreads; i++) {
        if (pThreadInfo[i].dwRunCount) {
            LogDetail (TEXT("Thread #%04u at prio %03u ran %05u times\r\n"), i, pThreadInfo[i].dwThreadPrio, pThreadInfo[i].dwRunCount);
            dwNumThreadsRun++;
        }
        else {
            LogDetail( TEXT("Thread #%04u at prio %03u did not run!\r\n"), i, pThreadInfo[i].dwThreadPrio );
        }
    }
    if(dwNumThreadsRun < 5) {
        LogDetail( TEXT("ERROR: Fewer than 5 threads (%d) had the chance to run. Something is wrong!\r\n"), dwNumThreadsRun );
        LogDetail( TEXT("       Analyze the running threads on your system\r\n") );
    }
    CHECKTRUE( dwNumThreadsRun >= 5 )

    for(i = 0; i < gdwMaxErrors; i++ ) {
        if(pSleepErrInfo[i].dwThreadNum) {
            nTimeDiff = pSleepErrInfo[i].dwTimerTime - pSleepErrInfo[i].dwSleepTime;
            if( nTimeDiff < 0 ) {
                LogDetail( TEXT("ERROR: Thread 0x%08x (pri=%d): Timer ticked less than sleep time\r\n"), pSleepErrInfo[i].dwThreadNum, pSleepErrInfo[i].dwThreadPri );
                LogDetail( TEXT("       Slept for %dms; timer ticked %dms\r\n"), pSleepErrInfo[i].dwSleepTime, pSleepErrInfo[i].dwTimerTime );
                nShortSleep++;
            }
            else if( nTimeDiff < (int)gdwToleranceTime ) {
                LogDetail( TEXT("WARNING: Thread 0x%08x (pri=%d): Timer ticked more than sleep time (but within the bounds of our tolerance %dms)\r\n"), pSleepErrInfo[i].dwThreadNum, pSleepErrInfo[i].dwThreadPri, gdwToleranceTime );
                LogDetail( TEXT("       Slept for %dms; timer ticked %dms\r\n"), pSleepErrInfo[i].dwSleepTime, pSleepErrInfo[i].dwTimerTime );
                nWarnOverSleep++;
            }
            else if (nTimeDiff >= (int)gdwToleranceTime ) {
                LogDetail( TEXT("ERROR: Thread 0x%08x (pri=%d): Timer ticked more than sleep time (outside the bounds of our tolerance %dms)\r\n"), pSleepErrInfo[i].dwThreadNum, pSleepErrInfo[i].dwThreadPri, gdwToleranceTime );
                LogDetail( TEXT("       Slept for %dms; timer ticked %dms\r\n"), pSleepErrInfo[i].dwSleepTime, pSleepErrInfo[i].dwTimerTime );
                nErrOverSleep++;
            }
        }
    }

    LogDetail ( TEXT("Summary information:"));
    LogDetail ( TEXT("    Max delay we did not consider an error: %ums\r\n"), gdwToleranceTime);
    LogDetail ( TEXT("    # of times we woke up before expected: %u\r\n"), nShortSleep);
    LogDetail ( TEXT("    # of times highest priority thread was slow to wake up: %u\r\n"), nErrOverSleep );
    LogDetail ( TEXT("    # of times highest priority thread was slow to wake up within tolerances: %u\r\n"), nWarnOverSleep);
    LogDetail ( TEXT("    # total errors: %u\r\n"), (nShortSleep + nErrOverSleep ) );

    if( nShortSleep + nErrOverSleep ) {
        LogDetail( TEXT("ERROR: This test did not meet realtime constraints\r\n") );
    }
    CHECKFALSE( nShortSleep );
    CHECKFALSE( nErrOverSleep );

    ENDSTEP( hTest, hStep);

    // cleanup on failure
    for (i = 0; i < gdwNumThreads; i++ ) {
        if (pThreadInfo[i].hThread ) {
            TerminateThread( pThreadInfo[i].hThread, 0 );
            CloseHandle( pThreadInfo[i].hThread );
        }
    }
    HeapFree( GetProcessHeap(), 0, pThreadInfo );
    HeapFree( GetProcessHeap(), 0, pSleepErrInfo );
    CeSetThreadPriority (GetCurrentThread (), dwOldPrio );
    CloseHandle( hGoEvt );

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}

/************************ private functions *************************/

/////////////////////////////////////////////////////////////////////
// Helper function, dumps the time onto the debug console
/////////////////////////////////////////////////////////////////////

void DumpSystemTime( LPSYSTEMTIME lpSysTime, LPTSTR szHeading )
{
    LogDetail (TEXT("%s %02u-%02u-%02u  %02u:%02u:%02u:%02u \r\n"),
        szHeading,
        lpSysTime->wMonth, lpSysTime->wDay, lpSysTime->wYear,
        lpSysTime->wHour, lpSysTime->wMinute, lpSysTime->wSecond, lpSysTime->wMilliseconds);

}

//////////////////////////////////////////////////////////////////////////////
// Helper function, calculated how much time passed between two System Times
//////////////////////////////////////////////////////////////////////////////
long long ElapsedTimeInms (LPSYSTEMTIME lpst1, LPSYSTEMTIME lpst2)
{
    FILETIME ft1 = { 0 };
    FILETIME ft2 = { 0 };

    ULARGE_INTEGER u1 = { 0 };
    ULARGE_INTEGER u2 = { 0 };

    SystemTimeToFileTime( lpst1, &ft1 );
    SystemTimeToFileTime( lpst2, &ft2 );

    memcpy( &u1, &ft1, sizeof( u1 ) );
    memcpy( &u2, &ft2, sizeof( u2 ) );

    return (u2.QuadPart - u1.QuadPart ) / 10000; // MS_TO_100NS = 10000
}


//////////////////////////////////////////////////////////////////
// calculates a random number in between the range given
//////////////////////////////////////////////////////////////////

int rand_range (int nLowest, int nHighest)
{
    return Random () % (nHighest - nLowest) + nLowest;
}

//////////////////////////////////////////////////////////////////
// Parses the command line and extracts all the information
//////////////////////////////////////////////////////////////////

BOOL ParseCmdLine (LPWSTR lpCmdLine)
{
    WCHAR *ptChar = NULL, *ptNext = NULL;
    WCHAR szSeps[] = TEXT(" \r\n");

    //in the option was "-h", just print help and return

    if ( wcsstr (lpCmdLine, TEXT("-h"))) {
        LogDetail (TEXT("%s"), gszHelp);
        return FALSE;
    }

    //else extract all the options (there can be more than one at a time)

    ptChar = wcstok_s (lpCmdLine, szSeps, &ptNext);
    while (ptChar) {
        LogDetail (TEXT("Token: %s\r\n"), ptChar);
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
                case _T('s'):
                    gdwSleepRange = _ttol (ptChar + 3);
                    break;
                case _T('r'):
                    gdwRunTime = _ttol (ptChar + 3);
                    break;
                case _T('t'):
                    gdwToleranceTime = _ttol (ptChar + 3);
                    break;
                default:
                    break;
            }
        }

        ptChar = wcstok_s (NULL, szSeps, &ptNext);
    }

    // Print the options we will be running under

    LogDetail (TEXT("Run time variables:\r\n"));
    LogDetail (TEXT("   Max errors before giving up: %u\r\n"), gdwMaxErrors);
    LogDetail (TEXT("   Highest thread priority: %u\r\n"), gdwHighestPrio);
    LogDetail (TEXT("   Lowest thread priority: %u\r\n"), gdwLowestPrio);
    LogDetail (TEXT("   Number of threads: %u\r\n"), gdwNumThreads);
    LogDetail (TEXT("   SleepRange: %ums\r\n"), gdwSleepRange);
    LogDetail (TEXT("   RunTime: %ums\r\n"), gdwRunTime);
    LogDetail (TEXT("   Tolerance Time: %ums\r\n"), gdwToleranceTime);
    LogDetail (TEXT("   continuous run: %s\r\n"), gfContinuous ? _T("YES") : _T("NO"));

    return TRUE;
}

//////////////////////////////////////////////////////////////////
//  Thread proc for all the threads running
//////////////////////////////////////////////////////////////////

void AddErrorInfo( DWORD dwSleepTime, DWORD dwTimerTicks ) {
    DWORD idx = InterlockedIncrement( &g_errCnt ) - 1;

    pSleepErrInfo[idx].dwThreadNum = GetCurrentThreadId();
    pSleepErrInfo[idx].dwThreadPri = CeGetThreadPriority( GetCurrentThread() );
    pSleepErrInfo[idx].dwSleepTime = dwSleepTime;
    pSleepErrInfo[idx].dwTimerTime = dwTimerTicks;
}

// NOTE: SleeperThread runs at higher priority than KITL
//       This means no debug output can be done in this thread or you
//       will hang (kitl thread will never be scheduled)
DWORD WINAPI SleeperThread (LPVOID lpvParam)
{
    DWORD dwCount1 = 0;
    DWORD dwCount2 = 0;
    DWORD dwSleepTime = 0;
    DWORD dwThreadNum = (DWORD) lpvParam;
    DWORD dwUS = 0;
    UINT uNumber;
    volatile WORD wCnt = 0;

    HANDLE hGoEvt = NULL;

    hGoEvt = CreateEvent( NULL, TRUE, FALSE, GOEVT );
    WaitForSingleObject( hGoEvt, INFINITE );

    while (gfContinue) {

        // spin a little work
        while( ++wCnt );

        // Count how many times we've run the loop
        pThreadInfo[dwThreadNum].dwRunCount++;

        // Determine a random time to sleep (normal range: 1-10);
        rand_s (&uNumber);
        dwSleepTime = uNumber % gdwSleepRange + 1;

        // Calibrate to be just beyond tick boundary when we start to measure
        // We only care for highest priority thread
        if (dwThreadNum == (gdwNumThreads -1) ) {
            Sleep( 1 );
        }

        // Sleep and check the time before and after
        dwCount1 = GetTickCount();
        Sleep (dwSleepTime);
        dwCount2 = GetTickCount();

        // Calculate the difference
        dwUS = dwCount2 - dwCount1;

        // You should not wake up before the sleep time is complete
        if (dwUS < dwSleepTime && gfContinue) {
            AddErrorInfo( dwSleepTime, dwUS );
            gnErrors++;
        }

        // For highest priority thread, check that we wake up (soon) after
        // specified sleep time
        if (dwThreadNum == (gdwNumThreads - 1) && (dwUS > (dwSleepTime + 1)) && gfContinue) {
            AddErrorInfo( dwSleepTime, dwUS );
            if( dwUS >= dwSleepTime + gdwToleranceTime ) {
                gnErrors++;
            }
        }

        // Break on too many errors
        if ( gnErrors >= gdwMaxErrors ) {
            gfContinue = FALSE;
            break;
        }
    }

    //LogDetail (TEXT("Thread: %02d   is exiting\r\n"), dwThreadNum);

    CloseHandle( hGoEvt );
    return 0;
}


