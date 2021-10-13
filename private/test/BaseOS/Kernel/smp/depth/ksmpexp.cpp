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


//
//  Exception Test #1.
//  -------------------------------
//  Description:      Set thread affinity, cause exception and change affinity in exception handler
//  Expected result:  Thread affinity will change in exception handler
//
TESTPROCAPI
SMP_DEPTH_EXCEPTION_1(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    DWORD dwOldAffinity=0, dwNewAffinity=0;
    DWORD dwCurrentProcNum;
    HANDLE hTest=NULL, hStep=NULL;
    BOOL bRC = TRUE;
    DWORD dwCnt;
    const DWORD NUM_OF_CHECKS = 20;

    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    else if(FALSE == IsSMPCapable()){
        LogDetail(TEXT("This test should only be run on platforms with more than 1 core....Skipping\r\n"));
        return TPR_SKIP;
    }

    RestoreInitialCondition();

    hTest = StartTest( NULL);

    BEGINSTEP( hTest, hStep, TEXT("Generating affinity...."));
        dwOldAffinity = dwNewAffinity = GenerateAffinity(FALSE);
        while(dwOldAffinity == dwNewAffinity){
            dwNewAffinity=GenerateAffinity(FALSE); //We don't want these two values to be the same
        }
        LogDetail(TEXT("dwNumOfCpus = %d, dwOldAffinity = %d, dwNewAffinity = %d\r\n"), CeGetTotalProcessors(),dwOldAffinity, dwNewAffinity);
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Setting affinity to dwOldAffinity"));
        CHECKTRUE(CeSetThreadAffinity(GetCurrentThread(), dwOldAffinity));
        for(dwCnt = 0; bRC && dwCnt < NUM_OF_CHECKS; dwCnt++){
            if((dwCurrentProcNum = GetCurrentProcessorNumber()) != dwOldAffinity){
                LogDetail( TEXT("dwCurrentProcNum(%d) != dwOldAffinity(%d)\r\n"),dwCurrentProcNum,dwOldAffinity);
                bRC = FALSE;
            }
            else{
                //No error, asks the scheduler to rescheudle us
                Sleep(0);
            }
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Causing Exception....intentionally"));
        LogDetail( TEXT("****************************************************************\r\n"));
        LogDetail( TEXT("**Going to cause exception.  The exception below is expected!***\r\n"));
        LogDetail( TEXT("****************************************************************\r\n"));

        __try {
              PDWORD pdw = NULL;
              *pdw = 5;  //Should cause exception
              
              //Should never get here
              LogDetail( TEXT("ERROR:  Did not cause exception.....\r\n"));
              bRC = FALSE;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
              CHECKTRUE(CeSetThreadAffinity(GetCurrentThread(), dwNewAffinity));
              if((dwCurrentProcNum = GetCurrentProcessorNumber()) != dwNewAffinity){
                  LogDetail( TEXT("dwCurrentProcNum(%d) != dwNewAffinity(%d)\r\n"),dwCurrentProcNum,dwNewAffinity);
                  bRC = FALSE;
              }
              LogDetail( TEXT("Changed Affinity to %d\r\n"),dwNewAffinity);
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Checking whether new affinity comes in effect"));
      for(dwCnt = 0; bRC && dwCnt < NUM_OF_CHECKS; dwCnt++){
          if((dwCurrentProcNum = GetCurrentProcessorNumber()) != dwNewAffinity){
              LogDetail( TEXT("dwCurrentProcNum(%d) != dwNewAffinity(%d)\r\n"),dwCurrentProcNum,dwNewAffinity);
              bRC = FALSE;
          }
          else{
              //No error, ask the scheduler to reschedule us
              Sleep(0);
          }
      }
      CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}



//
//    Exception Test - 2
//    Description:      One thread that goes on excepting (hits exception).  Scheduler another thread that is
//                      higher priority.
//    Expected Result:  Higher priority thread should be running in parallel with the
//                      excepting thread
//
typedef struct SMP_EXP2_TS{
    PBOOL lpDone;
    PLONG lpSharedVar;
}SMP_EXP2_TS, *LPSMP_EXP2_TS;

//
//  Returns 0 if no error.
//
static DWORD SpinningThread(LPVOID lpParam)
{
    LPSMP_EXP2_TS lpts = (LPSMP_EXP2_TS)lpParam;
    volatile PBOOL lpDone = lpts->lpDone;
    PLONG lpSharedVar = lpts->lpSharedVar;

    while(FALSE == *lpDone){
        (*lpSharedVar)++;
    }

    return 0;
}

TESTPROCAPI
SMP_DEPTH_EXCEPTION_2(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    HANDLE hTest=NULL, hStep=NULL;
    BOOL bRC = TRUE, fDone = FALSE;
    DWORD dwNumOfNonOverlap = 0;
    const DWORD NUM_OF_CHECKS = 5000;
    const DWORD MAX_NUM_OF_NON_OVERLAP = 1000;
    DWORD dwCnt;
    LONG lSharedVar = 0;
    HANDLE hThread = NULL;
    SMP_EXP2_TS ts = {&fDone,
                      &lSharedVar
                     };

    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    else if(FALSE == IsSMPCapable()){
        LogDetail(TEXT("This test should only be run on platforms with more than 1 core....Skipping\r\n"));
        return TPR_SKIP;
    }

    RestoreInitialCondition();

    hTest = StartTest( NULL);

    BEGINSTEP( hTest, hStep, TEXT("Starting higher priority spinning thread...."));
        const DWORD HIGH_PRIORITY = 249; //This is high enough for our purpose, don't need to starve KITL
        hThread =  CreateThread(NULL,
                                0,
                                (LPTHREAD_START_ROUTINE)SpinningThread,
                                (LPVOID)&ts,
                                0,
                                NULL);
        CHECKTRUE(IsValidHandle(hThread));
        CHECKTRUE(CeSetThreadPriority(hThread,HIGH_PRIORITY));
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Causing Exception....intentionally"));
        LogDetail( TEXT("****************************************************************\r\n"));
        LogDetail( TEXT("**Going to cause exception.  The exception below is expected!***\r\n"));
        LogDetail( TEXT("****************************************************************\r\n"));

        __try {
            PDWORD pdw = NULL;
            *pdw = 5;  //Should cause exception

            //Should not get here!
            LogDetail( TEXT("ERROR:  Did not cause exception %ld (tid=0x%x)\r\n"), __LINE__, GetCurrentThreadId());
            bRC = FALSE;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            //Make sure the other thread is running
            DWORD lOldValue = 0, lCurValue = 0;
            
            volatile PLONG pSharedVal = &lSharedVar;
            //We'll use the InterlockedIncrement on dwCnt to force memory barrier
            for(dwCnt = 0; dwCnt < NUM_OF_CHECKS; InterlockedIncrement((PLONG)&dwCnt)){
                lCurValue = *pSharedVal;
                if(lCurValue < lOldValue){
                    
                    LogDetail( TEXT("ERROR:  lCurValue = 0x%x,  lOldValue = 0x%x  LINE:  %ld (tid=0x%x, GLE=0x%x)\r\n"),
                              lCurValue, lOldValue, __LINE__, GetCurrentThreadId(), GetLastError());
                    bRC = FALSE;
                    break;
                }
                else if(lCurValue < lOldValue){
                    dwNumOfNonOverlap++;
                }
                lOldValue = lCurValue;
            }
        }
        LogDetail( TEXT("NUM_OF_CHECKS = %d, MAX_NUM_OF_NON_OVERLAP = %d \r\n"),NUM_OF_CHECKS, MAX_NUM_OF_NON_OVERLAP);
        LogDetail( TEXT("dwNumOfNonOverlap = %d \r\n"),dwNumOfNonOverlap);
        CHECKTRUE(dwNumOfNonOverlap < MAX_NUM_OF_NON_OVERLAP);
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);


    //Always clean up
    if(IsValidHandle(hThread)){
        InterlockedIncrement((PLONG)&fDone);
        DoWaitForThread(&hThread, 1, 0); //Don't care about thread return code since we can only return 1 value
        CloseHandle(hThread);
    }

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}

