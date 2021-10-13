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
#include <errno.h>
#include "kharness.h"
#include "ksmplib.h"


typedef struct SMP_SYNC_TS{
    HANDLE hEvtWait;
    HANDLE hEvtSignal;
    DWORD dwNumOfIterations;
    DWORD dwThreadNum; //For debugging only
}SMP_SYNC_TS, *LPSMP_SYNC_TS;


static DWORD Sync_Stress_Thread(LPVOID lpParam)
{
    LPSMP_SYNC_TS ts = (LPSMP_SYNC_TS)lpParam;
    DWORD dwNumOfCpus = CeGetTotalProcessors();
    DWORD dwThreadAffinity = Random() %  (dwNumOfCpus+1); //We want the range from 0 to dwNumOfCpus

    if(NULL == ts){
        LogDetail(TEXT("Invalid parameter.  ts is NULL\r\n"));
        return 1;
    }
    else if(FALSE == IsValidHandle(ts->hEvtWait)|| FALSE == IsValidHandle(ts->hEvtSignal)){
        LogDetail(TEXT("Invalid parameter.  ts->hEvtWait or ts->hEvtSignal is invalid\r\n"));
        return 1;
    }
    else if(FALSE == CeSetThreadAffinity(GetCurrentThread(), dwThreadAffinity)){
        LogDetail( TEXT("CeSetThreadAffinity() failed at line %ld (tid=0x%x, GLE=0x%x).\r\n"),__LINE__, GetCurrentThreadId(), GetLastError());
        return 1;
    }

    for(DWORD dwCnt = 0; dwCnt < ts->dwNumOfIterations; dwCnt++){
        DWORD dwWaitResult;
        if(WAIT_OBJECT_0 != (dwWaitResult = WaitForSingleObject(ts->hEvtWait, FIVE_SECONDS_TIMEOUT))){
            LogDetail( TEXT("WaitForSingleObject() failed at line %ld (tid=0x%x, GLE=0x%x).  dwWaitResult = 0x%x\r\n"),
                      __LINE__, GetCurrentThreadId(), GetLastError(), dwWaitResult);
            return 1;
        }
        else if(FALSE == SetEvent(ts->hEvtSignal)){
            LogDetail( TEXT("SetEvent() failed at line %ld (tid=0x%x, GLE=0x%x).\r\n"),__LINE__, GetCurrentThreadId(), GetLastError());
            return 1;
        }
    }

    LogDetail(TEXT("Thread %d is exiting....\r\n"), ts->dwThreadNum);
    return 0;
}

//
//  Synchronization test.
//  Description:      Create 100 threads and 100 events.  Thread x waits for event x and signals event X+1
//                    Assign affinity to each thread
//  Expected Result:  Should not deadlock / go out of sync
//
TESTPROCAPI
SMP_DEPTH_SYNC_STRESS(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    HANDLE hTest=NULL, hStep=NULL;
    BOOL bRC = TRUE;
    const DWORD NUM_OF_THREADS = 100;
    const DWORD NUM_OF_ITERATIONS = 1000;
    HANDLE hThread[NUM_OF_THREADS]={0};
    HANDLE hEvent[NUM_OF_THREADS]={0};
    SMP_SYNC_TS ts[NUM_OF_THREADS];
    DWORD dwCnt;


    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    else if(FALSE == IsSMPCapable()){
        LogDetail(TEXT("This test should only be run on platforms with more than 1 core....Skipping\r\n"));
        return TPR_SKIP;
    }

    RestoreInitialCondition();

    hTest = StartTest( NULL);
    BEGINSTEP( hTest, hStep, TEXT("Creating Events"));
      for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS; dwCnt++){
            hEvent[dwCnt] = CreateEvent(NULL, FALSE, FALSE, NULL);
            if(FALSE == IsValidHandle(hEvent[dwCnt])){
                bRC = FALSE;
            }
      }
      CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Creating threads"));
        for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS; dwCnt++){
            ts[dwCnt].hEvtWait = hEvent[dwCnt];
            ts[dwCnt].hEvtSignal = (dwCnt == (NUM_OF_THREADS-1))?
                                    hEvent[0]:
                                    hEvent[dwCnt+1];
            ts[dwCnt].dwNumOfIterations = NUM_OF_ITERATIONS;
            ts[dwCnt].dwThreadNum = dwCnt;
            hThread[dwCnt] =  CreateThread(NULL,
                                           0,
                                           (LPTHREAD_START_ROUTINE)Sync_Stress_Thread,
                                           (LPVOID)&ts[dwCnt],
                                           0,
                                           NULL);
            if(NULL == hThread[dwCnt]){
                bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
            }
        }
        CHECKTRUE(bRC);//Cannot use CHECKTRUE inside loop
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Waking up the first thread....."));
        Sleep(1000);  //Give them a chance to start
        CHECKTRUE(SetEvent(hEvent[0]));
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Waiting for threads to finish"));
        CHECKTRUE(DoWaitForThread(&hThread[0], NUM_OF_THREADS, 0));
    ENDSTEP(hTest,hStep);

    //Closing handles to thread should be done outside of BEGINSTEP / ENDSTEP
    //since we want to do it regardless of test result
    for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++){
        if(IsValidHandle(hThread[dwCnt])){
            CloseHandle(hThread[dwCnt]);
            hThread[dwCnt] = NULL;
        }

        if(IsValidHandle(hEvent[dwCnt])){
            CloseHandle(hEvent[dwCnt]);
            hEvent[dwCnt] = NULL;
        }
    }

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}

static BOOL CreateEventHandle(int nEvtNum, PHANDLE phEvt)
{
    const DWORD NUM_CONVERSION_BUFFER = 16;
    TCHAR szEvtName[MAX_PATH] = {0};
    TCHAR szNumConversion[NUM_CONVERSION_BUFFER] = {0};
    BOOL bRC = TRUE;

    if(!phEvt){
        bRC = FALSE;
    }
    else if( S_OK != StringCchCat(szEvtName, MAX_PATH, KSMP_SYNC1_EVT_PREFIX) ||
            EINVAL == _itot_s(nEvtNum, szNumConversion, NUM_CONVERSION_BUFFER, 10 ) ||
            S_OK != StringCchCat(szEvtName, MAX_PATH, szNumConversion)){
        bRC = FALSE;
    }
    else if(NULL == (*phEvt = CreateEvent(NULL, FALSE, FALSE, szEvtName))){
        bRC = FALSE;
    }
    return bRC;
}

//
//  Synchronization Test #1
//  Description:      Create multiple process (and each process creates multiple threads).
//                    We'll use SetEvent() and WaitForSingleObject() to signal across different threads / processes
//  Expected Result:  Should not deadlock
//
TESTPROCAPI
SMP_DEPTH_SYNC_1(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    const DWORD NUM_OF_EVT = KSMP_SYNC1_NUM_OF_PROCESS * KSMP_SYNC1_THREADS_PER_PROCESS;
    HANDLE hTest=NULL, hStep=NULL;
    HANDLE hProcess[KSMP_SYNC1_NUM_OF_PROCESS] = {0};
    HANDLE hEvent[NUM_OF_EVT] = {0};
    DWORD dwCnt;
    BOOL bRC = TRUE;

    RestoreInitialCondition();

    hTest = StartTest( NULL);
    BEGINSTEP( hTest, hStep, TEXT("Creating Events"));
        for(dwCnt = 0; bRC && dwCnt < NUM_OF_EVT; dwCnt++){
            if(FALSE == CreateEventHandle(dwCnt, &hEvent[dwCnt])){
                bRC = FALSE;
            }
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Creating the processes"));
        for(dwCnt = 0; bRC && dwCnt < KSMP_SYNC1_NUM_OF_PROCESS; dwCnt++){
            const DWORD BUFFER_SIZE = 10;
            PROCESS_INFORMATION pi = {0};
            TCHAR szParam[BUFFER_SIZE] = {0}; //used to pass dwCnt  to child process so they know what number they are (between 0 and KSMP_SYNC1_NUM_OF_PROCESS-1, inclusive)

            if(EINVAL == _ltow_s(dwCnt, szParam, BUFFER_SIZE, 10)){
                bRC = FALSE;
            }
            else{
                bRC = CreateProcess(KSMP_SYNC1_EXE_NAME,
                                    szParam,
                                    NULL, NULL, FALSE, 0,
                                    NULL, NULL, NULL,
                                    &pi);
               if(bRC){
                  CloseHandle(pi.hThread);  //only need the handle to the process, not the thread
                  hProcess[dwCnt] = pi.hProcess;
               }
            }
        }
        CHECKTRUE(bRC);//Cannot use CHECKTRUE inside loop
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Waking up the first process....."));
        Sleep(1000);  //Give them a chance to start
        CHECKTRUE(SetEvent(hEvent[0]));
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Wait for processes to finish"));
        CHECKTRUE(DoWaitForProcess(&hProcess[0], KSMP_SYNC1_NUM_OF_PROCESS ,1));
    ENDSTEP(hTest,hStep);

    //Closing handles to thread should be done outside of BEGINSTEP / ENDSTEP
    //since we want to do it regardless of test result
    for(dwCnt = 0; dwCnt < KSMP_SYNC1_NUM_OF_PROCESS; dwCnt++){
        if(IsValidHandle(hProcess[dwCnt])){
            CloseHandle(hProcess[dwCnt]);
            hProcess[dwCnt] = NULL;
        }
    }

    for(dwCnt = 0; dwCnt < NUM_OF_EVT; dwCnt++){
        if(IsValidHandle(hEvent[dwCnt])){
            CloseHandle(hEvent[dwCnt]);
            hEvent[dwCnt] = NULL;
        }
    }

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}