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
#include "kharness.h"
#include "ksmplib.h"

DWORD MatrixMultiply16Threads(BOOL fCheckLoadBalance, BOOL fPowerOffCores);

static VOID PowerOnAllCores()
{
    const DWORD MAX_WAIT = 20;
    DWORD dwNumOfCores = CeGetTotalProcessors();
    for(DWORD dwCnt = 2; dwCnt <= dwNumOfCores; dwCnt++){
        DWORD dwWaitCnt = 0;
        
        LogDetail(TEXT("Powering on core %d\r\n"), dwCnt);
        CePowerOnProcessor(dwCnt);
        
        //Wait till the core is ON
        while(CE_PROCESSOR_STATE_POWERED_ON != CeGetProcessorState(dwCnt) && dwWaitCnt++ < MAX_WAIT){
            Sleep(1000);
        }
        
        if(CE_PROCESSOR_STATE_POWERED_ON != CeGetProcessorState(dwCnt)) {
            LogDetail(TEXT("WARNING:  Cannot power on Core %d\r\n"), dwCnt);
        }
    }
}

//
//  Do basic API testing to make sure we can CEPowerOnProcessor and CEPowerOffProcessor return appropriate value
//
TESTPROCAPI
SMP_DEPTH_POWER_1(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    HANDLE hTest = NULL, hStep = NULL;
    const BOOL fCanPowerOffSecCores = CanPowerOffSecondaryProcessors(); //This shouldn't change during test execution
    const DWORD dwNumOfProcessors = CeGetTotalProcessors();            //This shouldn't change during test execution
    DWORD dwCnt;
    BOOL bRC = TRUE;

    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    else if(FALSE == IsSMPCapable()){
        LogDetail(TEXT("This test should only be run on platforms with more than 1 core....Skipping\r\n"));
        return TPR_SKIP;
    }
    else if(FALSE == InKernelMode()){
        LogDetail(TEXT("This test should only be run in kernel mode...Skipping\r\n"));
        return TPR_SKIP;
    }

    //
    //  We'll just power on all processors (in case they were not on before)
    //
    PowerOnAllCores();

    hTest = StartTest(NULL);

    BEGINSTEP( hTest, hStep, TEXT("Calling CePowerOnProcessor and CePowerOffProcessor on invalid index"));
        //Cores are 1-indexed, 0 is always an invalid parameter
        CHECKFALSE(CePowerOffProcessor(0, 1));
        CHECKFALSE(CePowerOnProcessor(0));

        //Valid processor number is 1 to dwNumOfProcessors inclusive
        CHECKFALSE(CePowerOffProcessor(dwNumOfProcessors+1, 1));
        CHECKFALSE(CePowerOnProcessor(dwNumOfProcessors+1));
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Should not be able to power on / off primary core"));
        //Should not be able to power off primary core
        CHECKFALSE(CePowerOffProcessor(1, 1));
        CHECKFALSE(CePowerOnProcessor(1));
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Trying to power off / on secondary cores"));
        if(fCanPowerOffSecCores){
            LogDetail( TEXT("We should be able to power off secondary cores on this system\r\n"));
            for(dwCnt = 2; bRC && dwCnt <= dwNumOfProcessors; dwCnt++){
                if(FALSE == (bRC = CePowerOffProcessor(dwCnt, 1))){
                    LogDetail( TEXT("Failed to power off processor at line %ld , dwCnt = %d(tid=0x%x, GLE=0x%x).\r\n"),
                        __LINE__, dwCnt, GetCurrentThreadId(), GetLastError());
                }
                else if(TRUE == CeSetThreadAffinity(GetCurrentThread(), dwCnt)){
                    LogDetail( TEXT("CeSetThreadAffinity returns TRUE after powering off processor at line %ld , dwCnt = %d(tid=0x%x, GLE=0x%x).\r\n"),
                        __LINE__, dwCnt, GetCurrentThreadId(), GetLastError());
                    bRC = FALSE;
                }
                else if(FALSE != CePowerOffProcessor(dwCnt, 1)){  //Try to power off the processor again after it's already off
                    LogDetail( TEXT("Power off processor twice at line %ld , dwCnt = %d(tid=0x%x, GLE=0x%x).\r\n"),
                        __LINE__, dwCnt, GetCurrentThreadId(), GetLastError());
                    bRC = FALSE;
                }
                else if(dwNumOfProcessors != CeGetTotalProcessors()){ //Even though we powered off a core, the number of processor should not change
                    LogDetail( TEXT("dwNumOfProcessors != CeGetTotalProcessors() at line %ld , dwCnt = %d.\r\n"), __LINE__, dwCnt);
                    bRC = FALSE;
                }
                else{
                    Sleep(1000);  //Give it a chance to fully power off
                    if(FALSE == (bRC = CePowerOnProcessor(dwCnt))){
                        LogDetail( TEXT("Failed on power off processor at line %ld , dwCnt = %d(tid=0x%x, GLE=0x%x).\r\n"),
                            __LINE__, dwCnt, GetCurrentThreadId(), GetLastError());
                    }
                }
            }
        }
        else{
            LogDetail( TEXT("We should not be able to power off secondary cores on this system\r\n"));
            for(dwCnt = 2; bRC && dwCnt <= dwNumOfProcessors; dwCnt++){
                if(TRUE == CePowerOffProcessor(dwCnt, 1)){
                    LogDetail( TEXT("Powered off processor at line %ld , dwCnt = %d(tid=0x%x, GLE=0x%x).\r\n"),
                        __LINE__, dwCnt, GetCurrentThreadId(), GetLastError());
                    bRC = FALSE;
                }
                else if(FALSE == (bRC = CeSetThreadAffinity(GetCurrentThread(), dwCnt))){
                    LogDetail( TEXT("CeSetThreadAffinity returns FALSEat line %ld , dwCnt = %d(tid=0x%x, GLE=0x%x).\r\n"),
                        __LINE__, dwCnt, GetCurrentThreadId(), GetLastError());
                }
                else if(dwNumOfProcessors != CeGetTotalProcessors()){
                    LogDetail( TEXT("dwNumOfProcessors != CeGetTotalProcessors() at line %ld , dwCnt = %d.\r\n"), __LINE__, dwCnt);
                    bRC = FALSE;
                }
                else if(TRUE == CePowerOnProcessor(dwCnt)){
                    LogDetail( TEXT("Powered on a processor that is already on at line %ld , dwCnt = %d(tid=0x%x, GLE=0x%x).\r\n"),
                        __LINE__, dwCnt, GetCurrentThreadId(), GetLastError());
                }
            }
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);

    RestoreInitialCondition();
    PowerOnAllCores();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}

//
//  Use 16 threads to multiple a 1024*1024 element matrix.  We'll assign affinity to the threads and during the multiply
//  we'll use CePowerOnProcessor and CePowerOffProcessors to power on / off cores.
//
TESTPROCAPI
SMP_DEPTH_POWER_2(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    DWORD dwRet;
    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    else if(FALSE == IsSMPCapable()){
        LogDetail(TEXT("This test should only be run on platforms with more than 1 core....Skipping\r\n"));
        return TPR_SKIP;
    }
    else if(FALSE == InKernelMode()){
        LogDetail(TEXT("This test should only be run in kernel mode...Skipping\r\n"));
        return TPR_SKIP;
    }

    RestoreInitialCondition();
    PowerOnAllCores();

    dwRet = MatrixMultiply16Threads(FALSE, TRUE);

    RestoreInitialCondition();
    PowerOnAllCores();

    return dwRet;
}

static DWORD PowerOnOffCoreThread(LPVOID lpParam)
{
    volatile PBOOL pfDone = (PBOOL)lpParam;
    UINT uiTempRand = 0;
    DWORD dwNumOfCores = CeGetTotalProcessors();

    while(FALSE == *pfDone){
        rand_s(&uiTempRand);
        BOOL fPowerOn = (uiTempRand & 0x1)? TRUE : FALSE;
        uiTempRand = 0;
        rand_s(&uiTempRand);
        DWORD dwCoreToOperate = (dwNumOfCores > 2) ?
                                ((uiTempRand % (dwNumOfCores -1)) +2):  //Pick one of the secondary cores (2 to dwNumOfCores)
                                2;  //Well we only have 2 cores, play with the only secondary core
        //Don't need to check return value, since we can have multiple threads trying to operate on the same core
        if(fPowerOn){
            CePowerOnProcessor(dwCoreToOperate);
        }
        else{
            CePowerOffProcessor(dwCoreToOperate,1);
        }
    }
    return 0;
}

//
//  Multiple threads trying to power on / off core at the same time
//
TESTPROCAPI
SMP_DEPTH_POWER_3(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    BOOL fDone = FALSE, bRC = TRUE;
    const DWORD NUM_OF_THREADS = 16;
    HANDLE hTest = NULL, hStep = NULL;
    HTHREAD hThread[NUM_OF_THREADS] = {NULL};
    DWORD dwCnt;

    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    else if(FALSE == IsSMPCapable()){
        LogDetail(TEXT("This test should only be run on platforms with more than 1 core....Skipping\r\n"));
        return TPR_SKIP;
    }
    else if(FALSE == InKernelMode()){
        LogDetail(TEXT("This test should only be run in kernel mode...Skipping\r\n"));
        return TPR_SKIP;
    }

    RestoreInitialCondition();
    PowerOnAllCores();

    hTest = StartTest(NULL);

    BEGINSTEP( hTest, hStep, TEXT("Creating threads"));
        CHECKTRUE(CeSetThreadPriority(GetCurrentThread(),CE_THREAD_PRIO_256_HIGHEST)); //Make sure we are higher priority than the threads we spawn

        for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS; dwCnt++){
            hThread[dwCnt] =  CreateThread(NULL,
                                           0,
                                           (LPTHREAD_START_ROUTINE)PowerOnOffCoreThread,
                                           (LPVOID)&fDone,
                                           0,
                                           NULL);
            if(NULL == hThread[dwCnt]){
                bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
            }
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);

    Sleep(60000); //Sleep for 1 minute to let the threads power on / off the core
    InterlockedExchange((LPLONG)&fDone, TRUE);

    BEGINSTEP( hTest, hStep, TEXT("Waiting for threads"));
        CHECKTRUE(DoWaitForThread(&hThread[0], NUM_OF_THREADS,0));
        InterlockedExchange((LPLONG)&fDone, TRUE);
    ENDSTEP(hTest,hStep);

    //Closing handles to thread should be done outside of BEGINSTEP / ENDSTEP
    //since we want to do it regardless of test result
    for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++){
        if(NULL != hThread[dwCnt]){
            CloseHandle(hThread[dwCnt]);
            hThread[dwCnt] = NULL;
        }
    }



    RestoreInitialCondition();
    PowerOnAllCores();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}

