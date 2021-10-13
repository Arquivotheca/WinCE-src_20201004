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
#include "ksmplib.h"


//Returns 0 if no error; 1 otherwise
static DWORD SMP_AFFINITY2_THD(LPVOID lpParam)
{
    HANDLE hEvt = (HANDLE)lpParam;
    if(WAIT_OBJECT_0 == WaitForSingleObject(hEvt, INFINITE))
        return 0;
    return 1;

}

//
// Regression Test for MEDPG 126841
//    Basic idea:  1)  When we use CeSetProcessAffinity(), all the threads belong to this process should have their
//                     thread affinities updated to this new value.
//                 2)  New threads should inherit process's affinity (ie new thread's thread affinity == process affinity)
//
TESTPROCAPI
SMP_DEPTH_AFFINITY_2(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    HANDLE hEvent = NULL, hTest = NULL, hStep = NULL;
    const DWORD NUM_OF_THREADS = 4;
    HANDLE hThread[NUM_OF_THREADS]={0};
    DWORD dwCnt;
    BOOL bRC = TRUE;


    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    else if(FALSE == IsSMPCapable()){
        LogDetail(TEXT("This test should only be run on platforms with more than 1 core....Skipping\r\n"));
        return TPR_SKIP;
    }

    RestoreInitialCondition();

    hTest = StartTest( NULL);
    BEGINSTEP( hTest, hStep, TEXT("Creating event and setting process affinity to 0"));
        hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        CHECKTRUE(IsValidHandle(hEvent));
        CHECKTRUE(CeSetProcessAffinity(GetCurrentProcess(), 0)); //Set process affinity to known state
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Create half the threads before changing process affinity"));
        for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS /2; dwCnt++){
            hThread[dwCnt] =  CreateThread(NULL,
                                           0,
                                           (LPTHREAD_START_ROUTINE)SMP_AFFINITY2_THD,
                                           (LPVOID)hEvent,
                                           0,
                                           NULL);
            if(NULL == hThread[dwCnt]){
                bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
            }
        }
        CHECKTRUE(bRC);//Cannot use CHECKTRUE inside loop
        CHECKTRUE(CheckThreadsAffinity(&hThread[0], NUM_OF_THREADS/2 ,0));
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Changing Process Affinity to 1"));
        CHECKTRUE(CeSetProcessAffinity(GetCurrentProcess(), 1));
        //Do some checking....
        CHECKTRUE(CheckProcessAffinity(GetCurrentProcess(), 1));
        CHECKTRUE(CheckThreadAffinity(GetCurrentThread(), 1));
        //All threads created before (this thread and the NUM_OF_THREADS/2  threads will have the new thread affinity)
        CHECKTRUE(CheckThreadsAffinity(&hThread[0], NUM_OF_THREADS/2 ,1));
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Create the rest of the threads after changing process affinity"));
        for(dwCnt = NUM_OF_THREADS /2; bRC && dwCnt < NUM_OF_THREADS; dwCnt++){
            hThread[dwCnt] =  CreateThread(NULL,
                                           0,
                                           (LPTHREAD_START_ROUTINE)SMP_AFFINITY2_THD,
                                           (LPVOID)hEvent,
                                           0,
                                           NULL);
            if(NULL == hThread[dwCnt]){
                bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
            }
        }
        CHECKTRUE(bRC);//Cannot use CHECKTRUE inside loop
        CHECKTRUE(CheckProcessAffinity(GetCurrentProcess(), 1));
        CHECKTRUE(CheckThreadAffinity(GetCurrentThread(), 1));
        //All threads threads should inherit the process affinity
        CHECKTRUE(CheckThreadsAffinity(&hThread[0], NUM_OF_THREADS ,1));
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Change everything back to affinity 0 by changing Process Affinity"));
        CHECKTRUE(CeSetProcessAffinity(GetCurrentProcess(), 0));
        CHECKTRUE(CheckProcessAffinity(GetCurrentProcess(), 0));
        CHECKTRUE(CheckThreadAffinity(GetCurrentThread(), 0));
        CHECKTRUE(CheckThreadsAffinity(&hThread[0], NUM_OF_THREADS ,0));
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Wait for threads to finish"));
        CHECKTRUE(SetEvent(hEvent));
        CHECKTRUE(DoWaitForThread(&hThread[0], NUM_OF_THREADS, 0));
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

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}


typedef struct SMP_AFFINITY1_TS_PNORMAL{
    DWORD dwStartTick;
}SMP_AFFINITY1_TS_PNORMAL, *LPSMP_AFFINITY1_TS_PNORMAL;

typedef struct SMP_AFFINITY1_TS_P0{
    volatile PBOOL pfDone;
    DWORD dwStartTick;
    DWORD dwEndTick;
    DWORD dwProcNum;          //Which processor is this thread running on?
    HANDLE hP0StartedEvent;     //Use this to tell Main thread we already set the processor number and is spinning
}SMP_AFFINITY1_TS_P0, *LPSMP_AFFINITY1_TS_P0;


//
//  Priority 0 thread:  Set Thread affinity to the proce
//  Return 0 if error.  1 otherwise
//
static DWORD SMP_AFFINITY1_THD_P0(LPVOID lpParam)
{
    LPSMP_AFFINITY1_TS_P0 lpts = (LPSMP_AFFINITY1_TS_P0)lpParam;
    
    CeSetThreadAffinity(GetCurrentThread(), 2);  //make sure we don't block primary core

    lpts->dwStartTick = GetTickCount();
    lpts->dwProcNum = GetCurrentProcessorNumber();

    //prevents thread migration
    if(FALSE == CeSetThreadAffinity(GetCurrentThread(), lpts->dwProcNum)){
        LogDetail( TEXT("CeSetThreadAffinity() failed at line %ld (tid=0x%x, GLE=0x%x).\r\n"),
                __LINE__, GetCurrentThreadId(), GetLastError());
        return 0;
    }
    else{
        LogDetail( TEXT("lpts->dwProcNum = %d"),lpts->dwProcNum);
    }

    SetEvent(lpts->hP0StartedEvent); //Tell the main thread that we already set the processor number
    while(FALSE == *(lpts->pfDone))
        ;  //spin


    lpts->dwEndTick = GetTickCount();

    return 1;
}

//
//  Return 0 if error.  1 otherwise
//
static DWORD SMP_AFFINITY1_THD_PNORMAL(LPVOID lpParam)
{
    LPSMP_AFFINITY1_TS_PNORMAL lpts = (LPSMP_AFFINITY1_TS_PNORMAL)lpParam;
    lpts->dwStartTick = GetTickCount();

    return 1;
}

//
//  SMP Depth - Affinity Test #1
//  ---------------------------------------------------------------
//  1)  Create a thread with priority 0 with no affinity set.
//  2)  Find out the processor this thread is running on.
//  3)  Start a lower priority thread with affinity set to the processor of affinity set to the
//      processor of priority 0 thread
//
//  Implementation:
//                   Main thread      - creates Pri 0 thread and Pri normal thread (Pri normal thread is created with CREATE_SUSPENDED flag)
//                   Pri 0 thread     - finds out which processor it is running at
//                                    - set this as global, signals the event tell Main Thread that we know which proc we're running on
//                   Main thread      - read which proc Pri 0 thread is running on, set Pri normal thread's affinity to this processor
//                                    - call ResumeThread() on Pri normal thread
//                   Pri normal thread - as soon as it gets to run, it records the start tick and exit
//
//
TESTPROCAPI
SMP_DEPTH_AFFINITY_1(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    else if(FALSE == IsSMPCapable()){
        //If there is only 1 processor, skip test otherwise we'll end up spawning a p0 thread spinning forever
        LogDetail(TEXT("This test should only be run on platforms with more than 1 core....Skipping\r\n"));
        return TPR_SKIP;
    }

    RestoreInitialCondition();

    HANDLE hTest=NULL, hStep=NULL;
    HANDLE hThdP0 = NULL, hThdPNormal = NULL;
    SMP_AFFINITY1_TS_P0 p0ts = {0}; //thread sturct for P0 thread
    SMP_AFFINITY1_TS_PNORMAL pNormalts = {0}; //thread struct for PNormal thread
    HANDLE hP0StartedEvent = NULL;
    BOOL fStopSpinning = FALSE;


    hTest = StartTest( NULL);

    BEGINSTEP( hTest, hStep, TEXT("Creating Event"));
        hP0StartedEvent=CreateEvent(NULL, FALSE, FALSE, NULL);
        CHECKTRUE(IsValidHandle(hP0StartedEvent));
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Creating the threads"));
        //Create P0 thread
        p0ts.pfDone= &fStopSpinning;
        p0ts.hP0StartedEvent= hP0StartedEvent;
        hThdP0 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SMP_AFFINITY1_THD_P0, &p0ts, 0,NULL);
        hThdPNormal = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SMP_AFFINITY1_THD_PNORMAL, &pNormalts,CREATE_SUSPENDED,NULL); //This thread is created w/ the suspend thread set
        CHECKTRUE(IsValidHandle(hThdP0));
        CHECKTRUE(IsValidHandle(hThdPNormal));
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Resuming normal priority thread"));
        DWORD dwWaitResult = WaitForSingleObject(hP0StartedEvent, INFINITE);
        int nMyThreadPrioirty = CeGetThreadPriority(GetCurrentThread());
        
        CHECKTRUE(WAIT_OBJECT_0==dwWaitResult);
        CHECKTRUE(CeSetThreadPriority(hThdP0,0));
        CHECKTRUE(CeSetThreadPriority(hThdPNormal,nMyThreadPrioirty));  //Main thread and normal priority thread have same prioirty
        
        //P0 thread signaled meaning it already recorded the proc number in thread sturct
        //Since we're using synchronization mechanism provided by OS, we don't need to worry about memory barriers

        //
        //  After promoting a high priority spinning thread, we need to avoid any KITL activity here, otherwise we can deadlock (if p0 thrad is spinning on core 0, KITL IST get run)
        //  If we send any message, we'll deadlock (from what I can tell, we can send message to PB, but after that message arrives at PB, the
        //  device cannot ack it (to ACK, the KITL IST running at priority 130 must move to core 0, but core 0 might be occupied by our spinning thread), so it'll be our "last words".
        //  In current code, we limit how long the P0 thread can spin for, so we don't deadlock, but P0 thread returns FAIL if we reach max spinning count
        //
        CHECKTRUE(CeSetThreadAffinity(hThdPNormal, p0ts.dwProcNum)); //Set thread affinity of Normal priority thread
        CHECKTRUE(ResumeThread(hThdPNormal)); //SetEvent to wake up normal priority thread
    ENDSTEP(hTest,hStep);

    //Do a Sleep(0) 5 times.  That means we'll grab and release the scheduler lock 5 times.
    //If the Pnormal thread is going to run, it'll run by now.  Remember, Mainthread (us) and the normal priority thread have the same thread prioirty.
    //The following code should not be inside a BEGINSTEP / ENDSTEP block, because we always want to stop the spinning thread via the InterlockedIncrement()
    const DWORD NUM_OF_RESCHEDULES = 5;
    for(DWORD dwCnt = 0; dwCnt < NUM_OF_RESCHEDULES; dwCnt++){
      Sleep(0);
    }
    InterlockedIncrement((LPLONG)&fStopSpinning);

    BEGINSTEP( hTest, hStep, TEXT("Waiting for the threads to exit"));
        CHECKTRUE(DoWaitForThread(&hThdP0, 1, 1));
        CHECKTRUE(DoWaitForThread(&hThdPNormal, 1, 1));
    ENDSTEP(hTest,hStep);

    //check for test results
    BEGINSTEP( hTest, hStep, TEXT("Checking test result"));
        LogDetail(TEXT("p0ts.dwStartTick = 0x%x"), p0ts.dwStartTick);
        LogDetail(TEXT("p0ts.dwEndTick = 0x%x"), p0ts.dwEndTick);
        LogDetail(TEXT("pNormalts.dwStartTick = 0x%x"), pNormalts.dwStartTick);
        CHECKTRUE(p0ts.dwStartTick <= p0ts.dwEndTick); //Start should be before end
        CHECKTRUE(p0ts.dwEndTick <= pNormalts.dwStartTick); //Main purpose of this test
    ENDSTEP(hTest,hStep);
    

    //Clean up
    if(IsValidHandle(hP0StartedEvent)){
        CloseHandle(hP0StartedEvent);
        hP0StartedEvent = NULL;
    }

    if(IsValidHandle(hThdP0)){
        CloseHandle(hThdP0);
        hThdP0 = NULL;
    }

    if(IsValidHandle(hThdPNormal)){
        CloseHandle(hThdPNormal);
        hThdPNormal = NULL;
    }

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}