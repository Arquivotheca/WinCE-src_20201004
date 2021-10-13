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


typedef struct SMP_LB_TS{
    DWORD dwNumOfIterations; /* IN */
    DWORD dwNumOfProcessors; /* IN */
    PDWORD pdResultArray;    /* OUT */
#ifdef DEBUG
    LPCRITICAL_SECTION lpcs;
#endif
}SMP_LB_TS, *LPSMP_LB_TS;


static DWORD LoadBalance_Thread(LPVOID lpParam)
{
    LPSMP_LB_TS ts = (LPSMP_LB_TS)lpParam;
    if(NULL == ts || NULL == ts->pdResultArray){
        return 1;
    }
    
    //We don't have to zero out pdResultArray since memory allocated by VirtualAlloc is initialized to zero (MSDN)

    for(DWORD dwCnt = 0; dwCnt < ts->dwNumOfIterations; dwCnt++){
        //GetCurrentProcessorNumber returns value between 1 to N  (where N is the number of processors)
        DWORD dwProcNum = GetCurrentProcessorNumber();
        (*(ts->pdResultArray + dwProcNum-1))++;
        Sleep(0);  //Reschedule
    }

#ifdef DEBUG
    EnterCriticalSection(ts->lpcs);
    LogDetail(TEXT("***************************************************************\r\n"));
    for(DWORD i = 0; i < ts->dwNumOfProcessors; i++){
        LogDetail(TEXT("pdwResult[%d] = %d\r\n"), i+1, *(ts->pdResultArray + i));
    }
    LogDetail(TEXT("***************************************************************\r\n"));
    LeaveCriticalSection(ts->lpcs);
#endif

    return 0;
}

//
//  Load Balance Test
//
//  Description:  Spawns 100 threads, and each thread does the following:
//                        -Find out which processor it is running on, record this information, then Sleep(0)
//                Note:  Sleep(0) effective asks the kernel to re-schedule us
//                At the end of the test, each core should have  (# of threads * # of Sleep(0) per thread) / (# of cores)
//                     +/- an acceptable error margin
//
TESTPROCAPI
SMP_DEPTH_LOAD_BALANCE(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    else if(FALSE == IsSMPCapable()){
        LogDetail(TEXT("This test should only be run on platforms with more than 1 core....Skipping\r\n"));
        return TPR_SKIP;
    }

    HANDLE hTest=NULL, hStep=NULL;
    BOOL bRC = TRUE;
    const DOUBLE MAX_DELTA = 5;  //in percent
    const DWORD NUM_OF_THREADS = 100;
    const DWORD NUM_OF_ITERATIONS = 50000;
    HANDLE hThread[NUM_OF_THREADS]={0};
    DWORD dwNumOfCPUs = CeGetTotalProcessors();
    DWORD dwCnt;
    PDWORD pdwResult = NULL, pdwFinalResult = NULL;  //we need to allocate sizeof(DWORD) * dwNumOfCPUs * NUM_OF_THREADS bytes for pdwResult, and sizeof(DWORD) * dwNumOfCPUs for pdwFinalResult

#ifdef DEBUG
    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);
#endif

    PVOID pThdStructArray = NULL;

    RestoreInitialCondition();


    hTest = StartTest( NULL);
    BEGINSTEP( hTest, hStep, TEXT("Allocating memory"));
        pThdStructArray = VirtualAlloc(NULL, NUM_OF_THREADS * sizeof(SMP_LB_TS), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        pdwResult = (PDWORD)VirtualAlloc(NULL, NUM_OF_THREADS * dwNumOfCPUs * sizeof(DWORD), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        pdwFinalResult = (PDWORD)VirtualAlloc(NULL, dwNumOfCPUs * sizeof(DWORD), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        CHECKTRUE(pThdStructArray);
        CHECKTRUE(pdwResult);
        CHECKTRUE(pdwFinalResult);
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Creating the threads"));
        LPSMP_LB_TS pts = (LPSMP_LB_TS)pThdStructArray;
        PDWORD pdResultArray = (PDWORD)pdwResult;

        for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS; dwCnt++){
            pts->dwNumOfIterations = NUM_OF_ITERATIONS;
            pts->dwNumOfProcessors = dwNumOfCPUs;
            pts->pdResultArray = pdResultArray;
#ifdef DEBUG
            pts->lpcs = &cs;
#endif
            hThread[dwCnt] =  CreateThread(NULL,
                                          0,
                                          (LPTHREAD_START_ROUTINE)LoadBalance_Thread,
                                          (LPVOID)pts,
                                          0,
                                          NULL);
            if(NULL == hThread[dwCnt]){
                bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
            }
            pts++;
            pdResultArray += dwNumOfCPUs;
        }
        CHECKTRUE(bRC);//Cannot use CHECKTRUE inside loop
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Wait for threads to finish"));
        CHECKTRUE(DoWaitForThread(&hThread[0], NUM_OF_THREADS, 0));

        LPSMP_LB_TS pts = (LPSMP_LB_TS)pThdStructArray;
        for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++){
            //Record the result from this thread into pdwFinalResult array
            for(DWORD i = 0; i < dwNumOfCPUs; i++){
                PDWORD pdwThdResultArray = pts->pdResultArray;
                pdwFinalResult[i] += pdwThdResultArray[i];
            }
        }
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Displaying and checking result...."));
        DWORD dwExpectedRun = (NUM_OF_THREADS * NUM_OF_ITERATIONS) / dwNumOfCPUs;
        for(DWORD i = 0; i < dwNumOfCPUs; i++){
            double delta = ((double)pdwFinalResult[i] - (double)dwExpectedRun) * 100 / dwExpectedRun;
            //Since we have other threads running on the system, and they can spin on any CPU, we cannot really expect
            //load balance to work. As long as each processor gets to run for a iteration (to make sure it's actually running)
            //we'll pass the test.
            if(0 == pdwFinalResult[i]){
                //Now this is bad, none of the threads got to run on this processor.....fail the test
                LogDetail(TEXT("**FAILED**:  pdwFinalResult[%d] = %d    Expected:  %d    Delta = %5.2f   MAX_DELTA = %5.2f\r\n"), i+1, pdwFinalResult[i], dwExpectedRun, delta, MAX_DELTA); 
                bRC = FALSE;
            }
            else if(delta > MAX_DELTA || delta < -MAX_DELTA){
                LogDetail(TEXT("**WARNING**:  pdwFinalResult[%d] = %d    Expected:  %d    Delta = %5.2f   MAX_DELTA = %5.2f\r\n"), i+1, pdwFinalResult[i], dwExpectedRun, delta, MAX_DELTA);
            }
            else{
                LogDetail(TEXT("PASSED:  pdwFinalResult[%d] = %d    Expected:  %d    Delta = %5.2f   MAX_DELTA = %5.2f\r\n"), i+1, pdwFinalResult[i], dwExpectedRun, delta, MAX_DELTA);
            }
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);

    //Closing handles to thread should be done outside of BEGINSTEP / ENDSTEP
    //since we want to do it regardless of test result
    for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++){
        if(NULL != hThread[dwCnt]){
            CloseHandle(hThread[dwCnt]);
            hThread[dwCnt] = NULL;
        }
    }

#ifdef DEBUG
    DeleteCriticalSection(&cs);
#endif

    if(pThdStructArray){
        VirtualFree(pThdStructArray,0,MEM_RELEASE);
        pThdStructArray = NULL;
    }

    if(pdwResult){
        VirtualFree(pdwResult,0,MEM_RELEASE);
        pdwResult = NULL;
    }

    if(pdwFinalResult){
        VirtualFree(pdwFinalResult,0,MEM_RELEASE);
        pdwFinalResult = NULL;
    }

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}
