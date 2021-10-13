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

#include <windows.h>
#include <tchar.h>
#include "kharness.h"
#include "ksmplib.h"

#ifdef x86
const DWORD ELEMENTS_PER_ROW = 1024;
#else
const DWORD ELEMENTS_PER_ROW = 512;
#endif

DOUBLE mA[ELEMENTS_PER_ROW][ELEMENTS_PER_ROW];  //source matrix A
DOUBLE mB[ELEMENTS_PER_ROW][ELEMENTS_PER_ROW];  //source matrix B
DOUBLE mER[ELEMENTS_PER_ROW][ELEMENTS_PER_ROW]; //expected result - calculated by one thread
DOUBLE mAR[ELEMENTS_PER_ROW][ELEMENTS_PER_ROW]; //actual result - calculated by multiple threads / power on&off / thread migration


static VOID PopulateSourceMatrixs()
{
    int i, j;
    const DOUBLE mA00 = 1.235976;  //Random constant
    const DOUBLE mB00 = 1.732323;  //Random constant

    //Generate value for source matrix A
    for(i = 0; i < ELEMENTS_PER_ROW; i++){
        DOUBLE start = i * mA00;
        for(j = 0; j < ELEMENTS_PER_ROW; j++){
            mA[i][j] = start + j * 0.5465;  //this is mA00*i + 0.5465j
        }
    }

    //Generate value for source matrix B
    for(i = 0; i < ELEMENTS_PER_ROW; i++){
        DOUBLE start = ELEMENTS_PER_ROW - i * mB00;
        for(j = 0; j < ELEMENTS_PER_ROW; j++){
            mB[i][j] = start + j * 0.105414;   //mB[i][j] =  ELEMENTS_PER_ROW - i * mB00  + j * 0.105414 
        }
    }
}

static VOID GenerateExpectedResult()
{
    int i, j, k;

  //Calculate the expected result.  This has complexity of O(N3).
    for(i = 0; i < ELEMENTS_PER_ROW; i++){
        for(j = 0; j < ELEMENTS_PER_ROW; j++){
            for(k = 0; k < ELEMENTS_PER_ROW; k++)
                mER[i][j] = mA[i][k] + mB[k][j];
        }
    }
}


//
//  Returns 1 if the matrixes match exactly, 0 otherwise
//
static BOOL CheckMatrix()
{
    const DWORD MAX_ERROR_TO_DISPLAY = 10;  //Don't overflow the screen w/ debug messages
    DWORD dwNumOfErrors = 0;

    for(int i = 0; i < ELEMENTS_PER_ROW; i++){
        for(int j = 0; j < ELEMENTS_PER_ROW; j++){
            if(mAR[i][j] != mER[i][j]){
                if(dwNumOfErrors < MAX_ERROR_TO_DISPLAY){
                    LogDetail( TEXT("mER[%d][%d] (= %f) != mAR[%d][%d] (=%f)\r\n"),i, j, mER[i][j], i, j, mAR[i][j]);
                }
                dwNumOfErrors++;
            }
        }
    }

    if(dwNumOfErrors)
        LogDetail( TEXT("FAILED!  CheckMatrix:  dwNumOfErrors = %d\r\n"),dwNumOfErrors);
    else
        LogDetail( TEXT("PASSED:  CheckMatrix:  the matrixes match exactly\r\n"));

    return !dwNumOfErrors;
}

//Multiply Matrix Thread - thread struct
typedef struct MMT_TS {
    int nStartRow;
    int nNumOfRows;
} MMT_TS, *LPMMT_TS;


static  DWORD MatixMultiplyThread(LPVOID lpParam)
{
    LPMMT_TS lpts = (LPMMT_TS)lpParam;
    int nEndRow = lpts->nStartRow + lpts->nNumOfRows;

    for(int i = lpts->nStartRow; i < nEndRow; i++){
        for(int j = 0; j < ELEMENTS_PER_ROW; j++){
            for(int k = 0; k < ELEMENTS_PER_ROW; k++)
                mAR[i][j] = mA[i][k] + mB[k][j];
        }
    }
    
    return 0;
}

static  VOID ClearActualResult()
{
    memset(mAR, 0, sizeof(mAR));
}



//
// The purpose of this test is to stress the floating point uint and make sure multiple threads can access shared data properly.
// This test performs matrix multiply using 1 thread, store the result in mER (expected result).  Then it spawns multiple threads
// to perform the same calculation (each thread is responsible for a small part of the overall multiplication).  The result for
// multiple threads multiply is stored in mAR (actual result).  After that, we check to make sure mAR == mER for all elements of the matrix
//
//  Can also run this on a single-core machine
//
TESTPROCAPI
SMP_DEPTH_MATRIX_1(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    HANDLE hTest = NULL, hStep = NULL;
    const DWORD NUM_OF_THREADS = 4;
    HANDLE hThread[NUM_OF_THREADS]={0};
    MMT_TS ts[NUM_OF_THREADS];
    DWORD dwCnt;
    BOOL bRC = TRUE;
    DWORD dwStartTick, dwEndTick;


    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    hTest = StartTest(NULL);
    PopulateSourceMatrixs();
    ClearActualResult();

    dwStartTick = GetTickCount();
    GenerateExpectedResult();
    dwEndTick = GetTickCount();
    LogDetail(TEXT("For one thread:  dwEndTick - dwStartTick = %d\r\n"), dwEndTick - dwStartTick);
    

    dwStartTick = GetTickCount();
    BEGINSTEP( hTest, hStep, TEXT("Creating the threads"));
        int nNumOfRowsPerThread = ELEMENTS_PER_ROW / NUM_OF_THREADS; 
        int nStartRow = 0;
        CHECKTRUE(nNumOfRowsPerThread * NUM_OF_THREADS == ELEMENTS_PER_ROW);  //We better make sure the division went smoothly, no remainder
        for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS; dwCnt++){
            ts[dwCnt].nStartRow = nStartRow;
            ts[dwCnt].nNumOfRows = nNumOfRowsPerThread;
            hThread[dwCnt] =  CreateThread(NULL,
                                           0,
                                           (LPTHREAD_START_ROUTINE)MatixMultiplyThread,
                                           (LPVOID)&ts[dwCnt],
                                           0,
                                           NULL);
            if(NULL == hThread[dwCnt]){
                bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
            }
            nStartRow+=nNumOfRowsPerThread;
        }
        CHECKTRUE(bRC);//Cannot use CHECKTRUE inside loop
    ENDSTEP(hTest,hStep);
    
    BEGINSTEP( hTest, hStep, TEXT("Wait for threads to finish"));
        for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++){
            if(WAIT_OBJECT_0 != WaitForSingleObject(hThread[dwCnt],INFINITE)){
                LogDetail(TEXT("WaitForSingleObject() failed for Thread[%d]\r\n"), dwCnt);
                bRC = FALSE;
            }
            //The thread always return 0, no need to check exit code
        }
        CHECKTRUE(bRC)
        dwEndTick = GetTickCount();
        LogDetail(TEXT("dwEndTick - dwStartTick = %d\r\n"), dwEndTick - dwStartTick);
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Comparing Matrix..."));
      CHECKTRUE(CheckMatrix());
    ENDSTEP(hTest,hStep);


    //Closing handles to thread should be done outside of BEGINSTEP / ENDSTEP
    //since we want to do it regardless of test result
    for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++){
        if(NULL != hThread[dwCnt]){
            CloseHandle(hThread[dwCnt]);
            hThread[dwCnt] = NULL;
        }
    }

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}



//
//  Multiply matrix with 16 threads, some with thread affinity set, some don't
//  Expect:  Matrix Multiply result is correct, and all of the threads get to run before any one threads completes
//
//  Should be run on both SMP and single core device  since we want to make sure all threads get a chance to run before
//  some threads finish
//
typedef struct MMTLB_TS {
    int nStartRow;
    int nNumOfRows;
    DWORD dwThreadAffinity;
    DWORD dwThreadPriority;
    DWORD dwStartTick;
    DWORD dwEndTick;
    HANDLE hEvt;
} MMTLB_TS, *LPMMTLB_TS; //Multiply Matrix Thread (Load Balance) - thread struct

static DWORD MatixMultiply_LB_Thread(LPVOID lpParam)
{
    LPMMTLB_TS lpts = (LPMMTLB_TS)lpParam;
    int nEndRow = lpts->nStartRow + lpts->nNumOfRows;

    if(FALSE == CeSetThreadAffinity(GetCurrentThread(), lpts->dwThreadAffinity)){
        LogDetail( TEXT("CeSetThreadAffinity() failed at line %ld (tid=0x%x, GLE=0x%x).\r\n"),__LINE__, GetCurrentThreadId(), GetLastError());
        return 1; //This should not happen...
    }
    else if(FALSE == CeSetThreadPriority(GetCurrentThread(), lpts->dwThreadPriority)){
        LogDetail( TEXT("CeSetThreadPriority() failed at line %ld (tid=0x%x, GLE=0x%x).\r\n"),__LINE__, GetCurrentThreadId(), GetLastError());
        return 1; //This should not happen...
    }
    else if(WAIT_OBJECT_0 != WaitForSingleObject(lpts->hEvt, INFINITE)){
        LogDetail( TEXT("WaitForSingleObject() failed at line %ld (tid=0x%x, GLE=0x%x).\r\n"),__LINE__, GetCurrentThreadId(), GetLastError());
        return 1; //This should not happen....
    }

    //All threads created, we got the signal...GO!
    lpts->dwStartTick = GetTickCount();

    for(int i = lpts->nStartRow; i < nEndRow; i++){
        for(int j = 0; j < ELEMENTS_PER_ROW; j++){
            for(int k = 0; k < ELEMENTS_PER_ROW; k++)
                mAR[i][j] = mA[i][k] + mB[k][j];
        }
        //Re-set thread affinity in case the core we want to run was powered off
        CeSetThreadAffinity(GetCurrentThread(), lpts->dwThreadAffinity); 
    }
    lpts->dwEndTick = GetTickCount();

    return 0;
}

static DWORD PowerOnOffCoreThread(LPVOID lpParam)
{
    volatile PBOOL pfDone = (PBOOL)lpParam;
    DWORD dwNumOfProcessors = CeGetTotalProcessors();

    //We want to be higher priority than the threads doing the actual multiply
    if(FALSE == CeSetThreadPriority(GetCurrentThread(), CE_THREAD_PRIO_256_HIGHEST))
        return 1;
    
    while(FALSE == *pfDone){
    
        //The first processor (primary processor is dwCnt = 1, but we cannot power off primary core)
        for(DWORD dwCnt = 2; dwCnt <= dwNumOfProcessors; dwCnt++){
            DWORD dwCoreToPowerOff = (dwCnt == dwNumOfProcessors) ? 2 : (dwCnt +1);
            
            CePowerOnProcessor(dwCnt);

            //We're turning off the same core we just powered off (because this system only
            //has 2 core, and we can only play with the only secondary core..)
            if(dwCnt == dwCoreToPowerOff) {
                Sleep(500); //Give it some time to power off complete before we try to power it back on
            }
               
            if(FALSE == CePowerOffProcessor(dwCoreToPowerOff, 1) &&  CanPowerOffSecondaryProcessors()) {
                LogDetail( TEXT("Failed to power off processor at line %ld , dwCnt = %d(tid=0x%x, GLE=0x%x).\r\n"),
                        __LINE__, dwCnt, GetCurrentThreadId(), GetLastError());
                return 1;
            }
            Sleep(500);
        }
    }
    return 0;
}


DWORD MatrixMultiply16Threads(BOOL fCheckLoadBalance, BOOL fPowerOffCores)
{
    HANDLE hEvent = NULL, hTest = NULL, hStep = NULL;
    const DWORD NUM_OF_THREADS = 16;
    const DWORD MULTIPLY_THREAD_PRIORITY = 100;
    HANDLE hThread[NUM_OF_THREADS]={0};
    HANDLE hPowerThread = NULL;
    BOOL fDone = FALSE;
    BOOL bRC = TRUE;
    MMTLB_TS ts[NUM_OF_THREADS];
    DWORD dwCnt;
    DWORD dwMinStart = 0xFFFFFFFF, dwMaxStart = 0, dwMinEnd = 0xFFFFFFFF, dwMaxEnd = 0;
    HANDLE hEvt = NULL;  //Signals the threads tell them OK to start multiplying

    hTest = StartTest(NULL);

    //Setup the source matrixes (mA and mB and expected result)
    BEGINSTEP( hTest, hStep, TEXT("Setting up"));
        PopulateSourceMatrixs();
        ClearActualResult();
        GenerateExpectedResult();
        hEvt = CreateEvent(NULL, TRUE, FALSE, NULL);
        CHECKTRUE(IsValidHandle(hEvt));
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Creating the threads"));
        int nNumOfRowsPerThread = ELEMENTS_PER_ROW / NUM_OF_THREADS;
        int nStartRow = 0;
        DWORD dwNumOfCpus = CeGetTotalProcessors();
        
        CHECKTRUE(nNumOfRowsPerThread * NUM_OF_THREADS == ELEMENTS_PER_ROW);  //We better make sure the division went smoothly, no remainder

        for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS; dwCnt++){
            ts[dwCnt].nStartRow = nStartRow;
            ts[dwCnt].nNumOfRows = nNumOfRowsPerThread;
            ts[dwCnt].dwStartTick = ts[dwCnt].dwEndTick = 0;
            ts[dwCnt].dwThreadAffinity = dwCnt % (dwNumOfCpus+1);
            ts[dwCnt].dwThreadPriority = MULTIPLY_THREAD_PRIORITY;
            ts[dwCnt].hEvt = hEvt;

            hThread[dwCnt] =  CreateThread(NULL,
                                           0,
                                           (LPTHREAD_START_ROUTINE)MatixMultiply_LB_Thread,
                                           (LPVOID)&ts[dwCnt],
                                           0,
                                           NULL);
            if(NULL == hThread[dwCnt]){
                bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
            }
            nStartRow+=nNumOfRowsPerThread;
        }
        CHECKTRUE(bRC);//Cannot use CHECKTRUE inside loop
    ENDSTEP(hTest,hStep);
    
    Sleep(3000);  //Give the worker thread a chance to set their initial thread affinity
    
    if(fPowerOffCores){
        BEGINSTEP( hTest, hStep, TEXT("Creating PowerOnOffCoreThread"));
            hPowerThread =  CreateThread(NULL,
                                         0,
                                         (LPTHREAD_START_ROUTINE)PowerOnOffCoreThread,
                                         (LPVOID)&fDone,
                                         0,
                                         NULL);
            CHECKTRUE(IsValidHandle(hPowerThread));
        ENDSTEP(hTest,hStep);
    }
    
    Sleep(3000);  //Make sure all the threads can run before we signal the event

    BEGINSTEP( hTest, hStep, TEXT("Setting Event"));
        CHECKTRUE(SetEvent(hEvt));
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Waiting for threads to finish"));
        CHECKTRUE(DoWaitForThread(&hThread[0], NUM_OF_THREADS,0));
        
        if(fPowerOffCores){
            InterlockedExchange((LPLONG)&fDone, TRUE);
            CHECKTRUE(DoWaitForThread(&hPowerThread,1,0));
        }
        
        for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++){
            //Update dwMinStart, dwMaxStart , dwMinEnd and dwMaxEnd
            LogDetail(TEXT("Thread [%d] started   @ 0x%x,    ended @ 0x%x,  with affinity = %d\r\n"),
                      dwCnt, ts[dwCnt].dwStartTick, ts[dwCnt].dwEndTick, ts[dwCnt].dwThreadAffinity);
            
            if(ts[dwCnt].dwStartTick < dwMinStart)
              dwMinStart = ts[dwCnt].dwStartTick;

            if(ts[dwCnt].dwStartTick > dwMaxStart)
              dwMaxStart = ts[dwCnt].dwStartTick;

            if(ts[dwCnt].dwEndTick < dwMinEnd)
              dwMinEnd = ts[dwCnt].dwEndTick;

            if(ts[dwCnt].dwEndTick > dwMaxEnd)
              dwMaxEnd = ts[dwCnt].dwEndTick;
        }
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Comparing Matrix"));
        CHECKTRUE(CheckMatrix());
    ENDSTEP(hTest,hStep);
    
    if(fCheckLoadBalance){
        BEGINSTEP( hTest, hStep, TEXT("Checking dwMaxStart < dwMinEnd..."));
            //dwMinStart < dwMaxStart < dwMinEnd < dwMaxEnd
            LogDetail(TEXT("First thread started   @ 0x%x\r\n"), dwMinStart);
            LogDetail(TEXT("Last thread started    @ 0x%x\r\n"), dwMaxStart);
            LogDetail(TEXT("First thread finished  @ 0x%x\r\n"), dwMinEnd);
            LogDetail(TEXT("Last thread finished   @ 0x%x\r\n"), dwMaxEnd);
            CHECKTRUE(dwMinStart < dwMaxStart);  //obviously this has to be true
            CHECKTRUE(dwMaxStart < dwMinEnd);    //The sole purpose of this test
            CHECKTRUE(dwMinEnd < dwMaxEnd);      //obviously this has to be true
        ENDSTEP(hTest,hStep);
    }

    //Closing handles to thread should be done outside of BEGINSTEP / ENDSTEP
    //since we want to do it regardless of test result
    for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++){
        if(IsValidHandle(hThread[dwCnt])){
            CloseHandle(hThread[dwCnt]);
            hThread[dwCnt] = NULL;
        }
    }

    if(IsValidHandle(hEvt)){
        CloseHandle(hEvt);
        hEvt = NULL;
    }
    
    if(IsValidHandle(hPowerThread)){
        CloseHandle(hPowerThread);
        hPowerThread = NULL;
    }

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;


}



//
// Load Balance Test
// Spawns 16 threads to multiply a matrix, makes sure the last threads gets to run before the first
// thread finishes.
//
//  Can also run this on a single-core machine
//
TESTPROCAPI
SMP_DEPTH_MATRIX_2(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    
    return MatrixMultiply16Threads(TRUE, FALSE);
}
