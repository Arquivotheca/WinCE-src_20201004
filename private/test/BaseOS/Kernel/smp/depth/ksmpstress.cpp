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
//  Returns 1 if no error.  0 otherwise
//
static DWORD Stress_Thread(LPVOID lpParam)
{
    DWORD dwResult = WaitForSingleObject((HANDLE)lpParam, INFINITE);
    return (WAIT_OBJECT_0 == dwResult)? 1: 0;
}


const DWORD MAX_NUM_OF_THREADS = 65536; //Max number of handles we can have in kernel
HANDLE hThread[MAX_NUM_OF_THREADS]={0};  //global, causes too many pages on the stack

const DWORD MAX_NUM_OF_PROCESSES = 65536; //Max number of handles we can have in kernel
HANDLE hProcess[MAX_NUM_OF_PROCESSES]={0};  //global, causes too many pages on the stack


//
//    Returns the number of threads we created before CreateThread failed
//
static DWORD DoThreadStress1(PBOOL pfError)
{
    DWORD dwCnt, dwNumOfThreadsCreated = 0;
    HANDLE hEvt = CreateEvent(NULL, TRUE, FALSE, NULL); //Second parameter is true because we need to wake up all the waiters with 1 setevent

    if(FALSE == IsValidHandle(hEvt)){
        LogDetail(TEXT("CreateEvent() failed \r\n"));
        *pfError = TRUE;
        return 0;
    }


    for(dwCnt = 0; dwCnt < MAX_NUM_OF_THREADS; dwCnt++){
        hThread[dwCnt] =  CreateThread(NULL,
                                       0,
                                      (LPTHREAD_START_ROUTINE)Stress_Thread,
                                      (LPVOID)hEvt,
                                       0,
                                       NULL);
        if(NULL == hThread[dwCnt]){
             //CreateThread() is going to fail eventually, so this is an expected error
             LogDetail(TEXT("CreateThread() failed at iteration %d with error code: 0x%x\r\n"), dwCnt, GetLastError());
             break;
        }
        else if(FALSE == CeSetThreadAffinity(hThread[dwCnt], GenerateAffinity(TRUE))){
            LogDetail(TEXT("CeSetThreadAffinity() failed \r\n"));
            *pfError = TRUE;
            break;
        }
   }
   dwNumOfThreadsCreated = dwCnt;

   //Tell the threads to Exit
   if(FALSE == SetEvent(hEvt) ||
      FALSE == DoWaitForThread(&hThread[0], dwNumOfThreadsCreated, 1)){
     *pfError = TRUE;
   }

    for(dwCnt = 0; dwCnt < dwNumOfThreadsCreated; dwCnt++){
      CloseHandle(hThread[dwCnt]);
    }

    CloseHandle(hEvt);
    Sleep(60*1000);  //Sleep for 60 seconds to give the threads a chance to fully exit

    return dwNumOfThreadsCreated;
}




//
//  Stress test:  CreateThread till we run out of memory while assigning affinities
//
TESTPROCAPI
SMP_DEPTH_STRESS_THREAD_1(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    HANDLE hTest=NULL, hStep=NULL;
    const DWORD NUM_OF_ITERATIONS = 10;
    DWORD  dwNumOfThreads[NUM_OF_ITERATIONS]={0};

    RestoreInitialCondition();

    hTest = StartTest(NULL);

    BEGINSTEP( hTest, hStep, TEXT("Begin stress test - thread"));
        BOOL fError = FALSE;
        for(DWORD dwCnt = 0; !fError && dwCnt < NUM_OF_ITERATIONS; dwCnt++){
            dwNumOfThreads[dwCnt] = DoThreadStress1(&fError);
            LogDetail(TEXT("Iteration %d:   dwNumOfThreads[%d] = %d, fError = %s\r\n"),
                      dwCnt, dwCnt, dwNumOfThreads[dwCnt], fError?L"TRUE":L"FALSE");
        }
        CHECKTRUE(FALSE == fError);//Cannot use CHECKTRUE inside loop
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Checking result"));
        const DWORD MIN_THREADS_EXPECTED = 1500;
        BOOL bRC = TRUE;
        for(DWORD dwCnt = 0; dwCnt < NUM_OF_ITERATIONS; dwCnt++){
            
            if(0 == dwNumOfThreads[dwCnt]){
                LogDetail(TEXT("ERROR:  dwNumOfThreads[%d] = 0\r\n"),dwCnt);
                bRC = FALSE;
            }
            else if(dwNumOfThreads[dwCnt] < MIN_THREADS_EXPECTED){
                LogDetail(TEXT("WARNING:  dwNumOfThreads[%d] = %d < MIN_THREADS_EXPECTED = %d\r\n"),dwCnt, dwNumOfThreads[dwCnt], MIN_THREADS_EXPECTED);
            }
            else{
                LogDetail(TEXT("dwNumOfThreads[%d] = %d\r\n"),dwCnt, dwNumOfThreads[dwCnt]);
            }
               
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}


//
//  Worker function to do the actual work.
//  Returns the number of processes we created before CreateProcess failed
//
static DWORD DoProcessStress1(PBOOL pfError)
{
    DWORD dwCnt, dwNumOfProcessesCreated = 0;
    HANDLE hEvt = CreateEvent(NULL, TRUE, FALSE, KSMP_P2_EVENT_NAME);

    if(FALSE == IsValidHandle(hEvt)){
        LogDetail(TEXT("CreateEvent() failed \r\n"));
        *pfError = TRUE;
        return 0;
    }


    for(dwCnt = 0; dwCnt < MAX_NUM_OF_PROCESSES; dwCnt++){
        PROCESS_INFORMATION pi = {0};
        if(FALSE == CreateProcess(KSMP_P2_EXE_NAME, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)){
             LogDetail(TEXT("CreateProcess() failed at iteration %d with error code: 0x%x\r\n"), dwCnt, GetLastError());
             break;
        }

        hProcess[dwCnt] = pi.hProcess;
        CloseHandle(pi.hThread);  //We don't need the thread handle of the primary thread
        if(FALSE == CeSetProcessAffinity(hProcess[dwCnt], GenerateAffinity(TRUE))){
            LogDetail(TEXT("CeSetProcessAffinity() failed \r\n"));
            *pfError = TRUE;
            break;
        }
    }
    dwNumOfProcessesCreated = dwCnt;

    //Tell the processes to Exit
    if(FALSE == SetEvent(hEvt)){
       *pfError = TRUE;
    }
    else{
        //We are not going to use DoWaitForProcess() here since we do not want to check the return code
        //Our child processes can fail if they run out of memory to CreateEvent (or OpenEvent) and return a different exit code
        //Since both 0 and 1 are acceptable return code in this situation we'll just ignore the return code
        for(dwCnt = 0; dwCnt < dwNumOfProcessesCreated; dwCnt++){
             if(WAIT_OBJECT_0 != WaitForSingleObject(hProcess[dwCnt], INFINITE)){
                LogDetail( TEXT("WaitForSingleObject() failed at line %ld (tid=0x%x, GLE=0x%x) at iteration = %d\r\n"),
                          __LINE__, GetCurrentThreadId(), GetLastError(), dwCnt);
                *pfError = TRUE; //We should not return here, should at least try to wait for the rest of the processes
            }
        }
    }

    for(dwCnt = 0; dwCnt < dwNumOfProcessesCreated; dwCnt++){
        CloseHandle(hProcess[dwCnt]);
    }

    CloseHandle(hEvt);
    Sleep(60*1000);  //Sleep for 60 seconds to give the threads a chance to fully exit

    return dwNumOfProcessesCreated;
}


//
//  Stress test:  CreateProcess till we run out of memory while assigning affinities
//
TESTPROCAPI
SMP_DEPTH_STRESS_PROCESS_1(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    HANDLE hTest=NULL, hStep=NULL;
    const DWORD NUM_OF_ITERATIONS = 10;
    DWORD  dwNumOfProcess[NUM_OF_ITERATIONS]={0};

    RestoreInitialCondition();

    hTest = StartTest(NULL);

    BEGINSTEP( hTest, hStep, TEXT("Begin stress test - process"));
        BOOL fError = FALSE;
        for(DWORD dwCnt = 0; !fError && dwCnt < NUM_OF_ITERATIONS; dwCnt++){
            dwNumOfProcess[dwCnt] = DoProcessStress1(&fError);
            LogDetail(TEXT("Iteration %d:   dwNumOfProcess[%d] = %d, fError = %s\r\n"),
                      dwCnt, dwCnt, dwNumOfProcess[dwCnt], fError?L"TRUE":L"FALSE");
        }
        CHECKTRUE(FALSE == fError);//Cannot use CHECKTRUE inside loop
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Checking result"));
        const DWORD MIN_PROCESSES_EXPECTED = 200;
        BOOL bRC = TRUE;
        for(DWORD dwCnt = 0; dwCnt < NUM_OF_ITERATIONS; dwCnt++){
            if(0 == dwNumOfProcess[dwCnt]){
                LogDetail(TEXT("ERROR:  dwNumOfProcess[%d] = 0 \r\n"),dwCnt);
                bRC = FALSE;
            }
            else if(dwNumOfProcess[dwCnt] < MIN_PROCESSES_EXPECTED) {
                LogDetail(TEXT("WARNING:  dwNumOfProcess[%d] = %d < MIN_PROCESSES_EXPECTED = %d\r\n"),dwCnt, dwNumOfProcess[dwCnt], MIN_PROCESSES_EXPECTED);
            }
            else{
                LogDetail(TEXT("dwNumOfProcess[%d] = %d\r\n"),dwCnt, dwNumOfProcess[dwCnt]);
            }
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}