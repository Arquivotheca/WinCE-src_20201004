//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include <windows.h>
#include <tchar.h>
#include "kharness.h"
#include "ycalls.h"
#include "ksmplib.h"
#include "ksmpbvt.h"

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO ssi;


//******************************************************************************
//***** ShellProc()
//******************************************************************************

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) {

   switch (uMsg) {

      case SPM_LOAD_DLL: {
         // Sent once to the DLL immediately after it is loaded.  The DLL may
         // return SPR_FAIL to prevent the DLL from continuing to load.
         PRINT(TEXT("ShellProc(SPM_LOAD_DLL, ...) called\r\n"));
         CreateLog( TEXT("ksmpbvt"));
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
         // PRINT(TEXT("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));
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


typedef struct SMP_BVT1_TS{
    HANDLE hEvent;
    DWORD dwNumOfIterations;
    PLONG plSharedVariable;
    DWORD dwNumOfOverlaps;
}SMP_BVT1_TS, *LPSMP_BVT1_TS;

//
//  This thread uses InterlockIncrement to increment a shared variable (assuming other threads will
//  do the same).  On every loop iteration, we compare whether the difference (between loop iteration)
//  is one.  If the difference is one, this mean no other thread incremented the shared variable ,
//  otherwise we can infer another thread got to run between our InterlockIncrement.
//
//  Returns 0 if no error.  1 otherwise
//
static DWORD InterlockIncrement_Thread(LPVOID lpParam)
{
    LPSMP_BVT1_TS lpts = (LPSMP_BVT1_TS)lpParam;
    PLONG plShared = lpts->plSharedVariable;
    LONG lPrevValue, lNewValue;  //Do not need to be initialized here
    DWORD dwNumOfOverLaps = 0; //We increment this if (lNewValue - lPrevValue) > 1, ie another thread incremented the value before we can increment it again

    //Wait for the Event to get signalled, so all the threads can start at the same time
    if(WAIT_OBJECT_0 != WaitForSingleObject(lpts->hEvent, INFINITE)) {
        LogDetail( TEXT("WaitForSingleObject() failed at line %ld (tid=0x%x, GLE=0x%x)\r\n"),
                  __LINE__, GetCurrentThreadId(), GetLastError()  );
        return 1;
    }

    lPrevValue= *plShared;
    for(DWORD cnt = 0; cnt < lpts->dwNumOfIterations; cnt++){
        lNewValue = InterlockedIncrement(plShared);
        if(lNewValue - lPrevValue > 1){
          dwNumOfOverLaps++;
        }
        lPrevValue = lNewValue;
    }
    lpts->dwNumOfOverlaps = dwNumOfOverLaps;
    return 0;
}



//
//  Returns FALSE if there is any error.
//
static BOOL
TwoThreadsRunningInParallel(
  DWORD dwNumOfIterations,           /* IN  - number of iterations to loop*/
  PDWORD pdwMinIterationInParallel,  /* OUT - minimum # of iteraion we are running in parallel with the other thread */
  PDWORD pdwMaxIterationInParallel   /* OUT - maximum # of iteraion we are running in parallel with the other thread */
  )
{
    HANDLE hTest=NULL, hStep=NULL;
    BOOL bRC = TRUE;
    const DWORD NUM_OF_THREADS = 2;
    LONG lSharedVariable = 0;
    DWORD dwCnt = 0;
    HANDLE hThread[NUM_OF_THREADS]={0};
    HANDLE hEvent = NULL;
    SMP_BVT1_TS ts[NUM_OF_THREADS];

    //Parameter validation
    if(!pdwMinIterationInParallel || !pdwMaxIterationInParallel){
        return FALSE;
    }

    //Set to the absolute worst case, and let the loop update these value later
    *pdwMinIterationInParallel = dwNumOfIterations;
    *pdwMaxIterationInParallel = 0;

    LogDetail(TEXT("Each thread is going to spin for %d iterations\r\n"), dwNumOfIterations);

    hTest = StartTest( NULL);

    BEGINSTEP( hTest, hStep, TEXT("Create event"));
        hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);  //second parameter is true since we want this event to remain set (need to wakeup multiple threads with one Set Event)
        CHECKTRUE(hEvent);
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Create the threads"));
        for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS; dwCnt++){
            ts[dwCnt].hEvent = hEvent;
            ts[dwCnt].dwNumOfIterations = dwNumOfIterations;
            ts[dwCnt].plSharedVariable = &lSharedVariable;
            hThread[dwCnt] = CreateThread(NULL,
                                          0,
                                          (LPTHREAD_START_ROUTINE)InterlockIncrement_Thread,
                                          (LPVOID)&ts[dwCnt],
                                          0,
                                          NULL);
            if(NULL == hThread[dwCnt]){
                bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
            }

        }
        CHECKTRUE(bRC);//Cannot use CHECKTRUE inside loop
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Setting threads priority"));
        CHECKTRUE(CeSetThreadPriority(hThread[0], THREAD_PRIORITY_MEDIUM));
        CHECKTRUE(CeSetThreadPriority(hThread[1], THREAD_PRIORITY_HIGH));
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Signalling Event"));
          CHECKTRUE(SetEvent(hEvent));
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Waiting for threads to finish"));
        CHECKTRUE(DoWaitForThread(&hThread[0],NUM_OF_THREADS,0));
        for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++){
            //Update absolute max and min
            if(ts[dwCnt].dwNumOfOverlaps > *pdwMaxIterationInParallel)
                *pdwMaxIterationInParallel = ts[dwCnt].dwNumOfOverlaps;

            if(ts[dwCnt].dwNumOfOverlaps < *pdwMinIterationInParallel)
                *pdwMinIterationInParallel = ts[dwCnt].dwNumOfOverlaps;
        }
    ENDSTEP(hTest,hStep);

    //Closing handles to thread should be done outside of BEGINSTEP / ENDSTEP
    //since we want to do it regardless of test result
    for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++){
        if(IsValidHandle(hThread[dwCnt])){
            CloseHandle(hThread[dwCnt]);
            hThread[dwCnt] = NULL;
        }
    }

    if(IsValidHandle(hEvent)){
        CloseHandle(hEvent);
        hEvent = NULL;
    }

    return FinishTest(hTest) ? TRUE : FALSE;
}


//
//Description:
//Scheduling two threads. First one with normal priority spinning. Now start a second thread with higher priority.
//
//Expected Result:
//Make sure both threads are running on different processors (Assuming more than 1 processor).
//

TESTPROCAPI
SMP_BVT_THREAD_1(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    const DWORD NUM_OF_ITERATONS = 100000000;
    const DWORD PASS_THRESHOLD_PERCENTAGE = 20;  //We expect at least 20% of the thread runs will be overlapped.  From my test runs, the overlap should be in the 30 to 50% range when I ran on NE1TB with 2 threads
    const DWORD MIN_ITERATIONS_IN_PARALLEL = (NUM_OF_ITERATONS * PASS_THRESHOLD_PERCENTAGE) / 100;
    DWORD dwMinNumOfIterationsInParallel = 0, dwMaxNumOfIterationsInParallel = 0;

    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    else if(FALSE == IsSMPCapable()){
        LogDetail(TEXT("This test should only be run on platforms with more than 1 core....Skipping\r\n"));
        return TPR_SKIP;
    }

    RestoreInitialCondition();  //Restore initial condition before test run

    LogDetail(TEXT("NUM_OF_ITERATONS = %d\r\n"), NUM_OF_ITERATONS);
    LogDetail(TEXT("PASS_THRESHOLD_PERCENTAGE = %d\r\n"), PASS_THRESHOLD_PERCENTAGE);
    LogDetail(TEXT("MIN_ITERATIONS_IN_PARALLEL = %d\r\n"), MIN_ITERATIONS_IN_PARALLEL);


    if(FALSE == TwoThreadsRunningInParallel(NUM_OF_ITERATONS, &dwMinNumOfIterationsInParallel, &dwMaxNumOfIterationsInParallel)){
        return TPR_FAIL;
    }
    
    RestoreInitialCondition();  //Restore initial condition after test run

    LogDetail(TEXT("dwMinNumOfIterationsInParallel = %d\r\n"), dwMinNumOfIterationsInParallel);
    LogDetail(TEXT("dwMaxNumOfIterationsInParallel = %d\r\n"), dwMaxNumOfIterationsInParallel);

    if(dwMinNumOfIterationsInParallel > dwMaxNumOfIterationsInParallel){
        //This should never happen.  If this does, something is wrong with TwoThreadsRunningInParallel
        LogDetail(TEXT("ERROR:  dwMinNumOfIterationsInParallel > dwMaxNumOfIterationsInParallel\r\n"));
        return TPR_FAIL;
    }
    else if (dwMinNumOfIterationsInParallel < MIN_ITERATIONS_IN_PARALLEL){
        LogDetail(TEXT("ERROR:  dwMinNumOfIterationsInParallel < MIN_ITERATIONS_IN_PARALLEL\r\n"));
        return TPR_FAIL;
    }

    return TPR_PASS;
}

//
//Description:
//Scheduling two processes. First one with normal priority spinning. Now start a second process with higher priority.
//
//Expected Result:
//Make sure both processes are running on different processors (Assuming more than 1 processor).
//
TESTPROCAPI
SMP_BVT_PROCESS_1(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    const DWORD NUM_OF_ITERATONS = 100000000;
    const DWORD NUM_OF_PROCESSES = 2;
    const DWORD PASS_THRESHOLD_PERCENTAGE = 20;  //We expect at least 20% of the thread runs will be overlapped.  From my test runs, the overlap should be in the 30 to 50% range when I ran on NE1TB with 2 threads
    const DWORD MIN_ITERATIONS_IN_PARALLEL = (NUM_OF_ITERATONS * PASS_THRESHOLD_PERCENTAGE) / 100;
    DWORD dwMinNumOfIterationsInParallel = 0, dwMaxNumOfIterationsInParallel = 0;
    DWORD dwCnt, dwBytesRead, dwFlags;
    HANDLE hTest=NULL, hStep=NULL;
    HANDLE hEvent = NULL, hMsgQ = NULL;
    HANDLE hProcesses[NUM_OF_PROCESSES] = {0};
    BOOL bRC = TRUE;
    DWORD dwTestResult = TPR_FAIL;
    DWORD dwNumOfOverlaps[NUM_OF_PROCESSES] ={0};

    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    else if(FALSE == IsSMPCapable()){
        LogDetail(TEXT("This test should only be run on platforms with more than 1 core....Skipping\r\n"));
        return TPR_SKIP;
    }

    LogDetail(TEXT("NUM_OF_ITERATONS = %d\r\n"), NUM_OF_ITERATONS);
    LogDetail(TEXT("PASS_THRESHOLD_PERCENTAGE = %d\r\n"), PASS_THRESHOLD_PERCENTAGE);
    LogDetail(TEXT("MIN_ITERATIONS_IN_PARALLEL = %d\r\n"), MIN_ITERATIONS_IN_PARALLEL);

    RestoreInitialCondition();

    hTest = StartTest( NULL);

    BEGINSTEP( hTest, hStep, TEXT("Creating Message Queue"));
        //message queue is used to retrieve data from child processes
        MSGQUEUEOPTIONS msgOpt = {sizeof(MSGQUEUEOPTIONS),   //dwSize
                                 MSGQUEUE_ALLOW_BROKEN,      //dwFlags;
                                 0,                          //dwMaxMessages
                                 sizeof(DWORD),              //cbMaxMessage;
                                 TRUE                        //bReadAccess;
                                };
        hMsgQ = CreateMsgQueue(KSMP_P1_MSGQ_NAME, &msgOpt);
        CHECKTRUE(IsValidHandle(hMsgQ));
    ENDSTEP(hTest,hStep);

    //Create the processes
    BEGINSTEP( hTest, hStep, TEXT("Creating Process"));
        PROCESS_INFORMATION pi;
        TCHAR szParam[MAX_PATH]= {0};
        CHECKTRUE(0 == _ltow_s(NUM_OF_ITERATONS, szParam, MAX_PATH,10));
                
        for(dwCnt = 0; bRC && dwCnt < NUM_OF_PROCESSES; dwCnt++){
            bRC = CreateProcess(KSMP_P1_EXE_NAME,
                                szParam,
                                NULL, NULL, FALSE, 0,
                                NULL, NULL, NULL,
                                &pi);
            hProcesses[dwCnt] = pi.hProcess;
            
            //We only need the process handle
            if(IsValidHandle(pi.hThread)){
                CloseHandle(pi.hThread);
            }
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);

    //Sleep for 3 seconds to make sure everything is ready
    Sleep(3000);


    //Signal the Event
    BEGINSTEP( hTest, hStep, TEXT("Signaling Event..."));
        hEvent = CreateEvent(NULL, TRUE, FALSE, KSMP_P1_EVENT_NAME);
        CHECKTRUE(IsValidHandle(hEvent));
        CHECKTRUE(SetEvent(hEvent));
    ENDSTEP(hTest,hStep);


    //Wait for the processes to finish
    BEGINSTEP( hTest, hStep, TEXT("Waiting for child processes to finish..."));
        bRC = DoWaitForProcess(&hProcesses[0], NUM_OF_PROCESSES, 0);
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);


    //Read from message queue and check whether there are enough overlaps
    BEGINSTEP( hTest, hStep, TEXT("Reading from Message Queue..."));
        //Reset these values to the absolute worst
        dwMinNumOfIterationsInParallel = NUM_OF_ITERATONS;
        dwMaxNumOfIterationsInParallel = 0;
        for(dwCnt = 0; dwCnt < NUM_OF_PROCESSES; dwCnt++){
            bRC = ReadMsgQueue(hMsgQ, &dwNumOfOverlaps[dwCnt], sizeof(DWORD), &dwBytesRead, 2000, &dwFlags);
            if(FALSE == bRC){
                LogDetail( TEXT("ReadMsgQueue failed at line %ld (tid=0x%x)\r\n"), __LINE__, GetCurrentThreadId());
                break;  //Break out of the loop and let bRC takes care of error reporting (cannot use CHECKTRUE inside any loop)
            }
            else if(sizeof(DWORD) != dwBytesRead){
                LogDetail( TEXT("sizeof(DWORD) != dwBytesRead (0x%x) at line %ld (tid=0x%x)\r\n"),dwBytesRead, __LINE__, GetCurrentThreadId());
                bRC = FALSE;
                break;
            }
            else{
                //We get a DWORD out from the message queue
                LogDetail( TEXT("At iteration %d, read %d from message queue\r\n"), dwCnt, dwNumOfOverlaps[dwCnt]);
                if(dwNumOfOverlaps[dwCnt] > dwMaxNumOfIterationsInParallel){
                    dwMaxNumOfIterationsInParallel = dwNumOfOverlaps[dwCnt];
                }

                if(dwNumOfOverlaps[dwCnt] < dwMinNumOfIterationsInParallel){
                    dwMinNumOfIterationsInParallel = dwNumOfOverlaps[dwCnt];
                }
            }
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest, hStep);


    //Finish test execution, clean up (always execute)
    dwTestResult = FinishTest(hTest) ? TPR_PASS : TPR_FAIL; //Deinit the test harness

    //Close Message Queue
    if(IsValidHandle(hMsgQ)){
        CloseMsgQueue(hMsgQ);
        hMsgQ = NULL;
    }

    //Close Event
    if(IsValidHandle(hEvent)){
        CloseHandle(hEvent);
        hEvent = NULL;
    }

    for(dwCnt = 0; dwCnt < NUM_OF_PROCESSES; dwCnt++){
        if(IsValidHandle(hProcesses[dwCnt])){
            CloseHandle(hProcesses[dwCnt]);
        }
    }

    LogDetail(TEXT("dwMinNumOfIterationsInParallel = %d\r\n"), dwMinNumOfIterationsInParallel);
    LogDetail(TEXT("dwMaxNumOfIterationsInParallel = %d\r\n"), dwMaxNumOfIterationsInParallel);

    if(dwMinNumOfIterationsInParallel > dwMaxNumOfIterationsInParallel){
        LogDetail(TEXT("ERROR:  dwMinNumOfIterationsInParallel > dwMaxNumOfIterationsInParallel\r\n"));
        dwTestResult = TPR_FAIL;
    }
    else if (dwMinNumOfIterationsInParallel < MIN_ITERATIONS_IN_PARALLEL){
        LogDetail(TEXT("ERROR:  dwMinNumOfIterationsInParallel < MIN_ITERATIONS_IN_PARALLEL\r\n"));
        dwTestResult = TPR_FAIL;
    }

    RestoreInitialCondition();

    return dwTestResult;
}
