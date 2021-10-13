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
#include "ksmpdepth.h"

typedef struct SMP_CS1_PHIGH_TS{
    LPCRITICAL_SECTION lpcs;
    PBOOL pfDone;
    BOOL fDifferentProcessors;
}SMP_CS1_PHIGH_TS, *LPSMP_CS1_PHIGH_TS;


static DWORD CS1_Spinning_Thread(LPVOID lpParam)
{
    volatile PBOOL pfDone = (PBOOL)lpParam;
    while(FALSE == *pfDone)
      ; //spin

    return 1;
}

//
//  Returns 1 if no error.  0 otherwise
//
static DWORD CS1_PHigh_Thread(LPVOID lpParam)
{
    LPSMP_CS1_PHIGH_TS lpts = (LPSMP_CS1_PHIGH_TS)lpParam;

    if(lpts->fDifferentProcessors && FALSE == CeSetThreadAffinity(GetCurrentThread(), 2)){
        LogDetail(TEXT("CeSetThreadAffinity() failed\r\n"));
        return 0;
    }
    
    Sleep(30000);

    EnterCriticalSection(lpts->lpcs);
    //Primary thread called Leave CS, so we get to run again
    //Tell the spinning threads to exit
    InterlockedExchange((LPLONG)lpts->pfDone,TRUE);

    LeaveCriticalSection(lpts->lpcs);

    return 1;
}



//
//  Returns 1 if no error.  0 otherwise
//
static DWORD CS1_PNormal_Thread(LPVOID lpParam)
{
    LPCRITICAL_SECTION lpcs = (LPCRITICAL_SECTION)lpParam;

    EnterCriticalSection(lpcs);
    // No op
    LeaveCriticalSection(lpcs);

    return 1;
}


VOID CS1_GET_PARAM(
      DWORD dwParameter,
      PBOOL pfDifferentProc,
      PDWORD pdwNumOfNOrPriThds
      )
{
    if(!pfDifferentProc || !pdwNumOfNOrPriThds)
        return;
    *pfDifferentProc = (dwParameter & CS1_DIFFERENT_PROC_FLAG)?
                        TRUE : FALSE;
    *pdwNumOfNOrPriThds = (dwParameter >> CS1_NUM_OF_NORMAL_PRI_THD_SHIFT_MASK) & 0xFFFF ;
}


//
//  Critical Section Test #1
//
//  There will be 4 type of threads in this test (ordered by priority level):
//                a)  (H)igh prioirty thread
//                    -This thread wants to enter the CS and this CS is owned by one of the Normal Priority threads
//                    -Priority level:  CE_THREAD_PRIO_256_TIME_CRITICAL
//                b)  (P)rimary thread (that's the main tux thread)
//                    -Only one, responsible for spawning other threads
//                    -Priority level:  CE_THREAD_PRIO_256_BELOW_NORMAL
//                c)  (S)pinning thread (they use to keep the other cores busy)
//                    -There will be n of these where n is the number of cores (just to make sure the cores are busy if nothing above this priority level is running)
//                    -They spin until a the flag is set.
//                    -Priority Level:  CE_THREAD_PRIO_256_ABOVE_NORMAL
//                d)  (N)ormal Priority thread
//                    -Can have multiple of these (the number of normal priority thread is a parameter we retrieve from the tux function table)
//                    -Enters the CS, records the time and exits
//                    -Priority Level:  CE_THREAD_PRIO_256_NORMAL
//
//  Description:  Make sure Priority donation works
//      1)        P =>  Initialize and Enter Critical Section (cs)
//      2)        P =>  Set its own priority to CE_THREAD_PRIO_256_NORMAL
//      3)        P =>  Starts x number of normal priority thread, where x is given by the tux function table set their priority to CE_THREAD_PRIO_256_NORMAL
//                      -if these threads get to run, they will Enter the CS, Leave the CS then finish
//      4)        P =>  Starts high priority thread, sets its priority to CE_THREAD_PRIO_256_TIME_CRITICAL
//                      -high priority thread sleeps for 10 seconds and wakes up
//                      -high priority thread wants to enter the CS owned by primary thread, high priority thread will get blocked and donates its priority to primary thread
//      5)        P =>  Starts n spinning threads and set their priority level to CE_THREAD_PRIO_256_ABOVE_NORMAL
//                ....
//                ....  P and N should be blocked and all the spinning threads are spinning...........
//                ....
//                ....  *When the high priority thread wakes up, it'll preempt one of the spinning threads, and tries to enter the CS
//                      -Since primary thread is holding the CS wanted by the high priority thread, priority donation should happen here and we should be running at CE_THREAD_PRIO_256_TIME_CRITICAL
//      6)        P =>  Leaves the CS
//      7)        H =>  Gets to run, signals spinning threads to exit
//                ....  (everything gets to run, normal priority thread enters then exits the CS)
//      8)        P =>  Finishes
//
//   If Priority donation does not happen, at *, all the spinning threads will consume all the cores and we'll spin forever.
//
//   Expected Results:   -No DeadLock
//                       
//


TESTPROCAPI
SMP_DEPTH_CS_1(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    HANDLE hTest=NULL, hStep=NULL;
    BOOL bRC = TRUE, fDone = FALSE, fDifferentProcessors = FALSE;
    DWORD dwNumOfNorPriThreads, dwCnt;
    DWORD dwNumOfSpinningThds = 2 *  CeGetTotalProcessors() ;
    HANDLE hPHighThread = NULL;
    PHANDLE pHndSpnThds = NULL;
    PHANDLE pHndNorPriThds = NULL;

    CRITICAL_SECTION cs;

    CS1_GET_PARAM(lpFTE->dwUserData, &fDifferentProcessors, &dwNumOfNorPriThreads);

    SMP_CS1_PHIGH_TS ts = {&cs, &fDone ,fDifferentProcessors};

    LogDetail(TEXT("fDifferentProcessors = %s, dwNumOfNorPriThreads = %d\r\n"),
              fDifferentProcessors?L"TRUE":L"FALSE",
              dwNumOfNorPriThreads);

    if(fDifferentProcessors && FALSE == IsSMPCapable()){
        //If we're doing priority donation across different processors, we need at least 2 cores
        LogDetail(TEXT("SKIP:  This test requires at least 2 cores\r\n"));
        return TPR_SKIP;
    }

    hTest = StartTest( NULL);
    

    RestoreInitialCondition();
    
    //Initialize and Enter Critical Section (cs)
    InitializeCriticalSection(&cs);
    EnterCriticalSection(&cs);
    
    //Set its own priority to CE_THREAD_PRIO_256_NORMAL
    BEGINSTEP( hTest, hStep, TEXT("Setting Primary thread prioirty level to CE_THREAD_PRIO_256_NORMAL"));
        CHECKTRUE(CeSetThreadPriority(GetCurrentThread(), CE_THREAD_PRIO_256_NORMAL));
    ENDSTEP(hTest,hStep);
    
    //Starts x number of normal priority thread, where x is given by the tux function table set their priority to CE_THREAD_PRIO_256_NORMAL
    BEGINSTEP( hTest, hStep, TEXT("Creating normal priority threads"));
        LogDetail(TEXT("Creating %d normal thread(s)...\r\n"),dwNumOfNorPriThreads);
        if(dwNumOfNorPriThreads > 0){
            pHndNorPriThds = (PHANDLE)LocalAlloc(LPTR, dwNumOfNorPriThreads * sizeof(HANDLE));

            CHECKTRUE(pHndNorPriThds);
            for(dwCnt = 0; bRC && dwCnt < dwNumOfNorPriThreads; dwCnt++){
                PHANDLE pCurHandle = pHndNorPriThds + dwCnt;
                *pCurHandle =  CreateThread(NULL,
                                            0,
                                            (LPTHREAD_START_ROUTINE)CS1_PNormal_Thread,
                                            (LPVOID)&cs,
                                            0,
                                            NULL);
                if(NULL == *pCurHandle){
                    bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
                }
                else if(FALSE == CeSetThreadPriority(*pCurHandle, CE_THREAD_PRIO_256_NORMAL)){
                    LogDetail(TEXT("ERROR:  CeSetThreadPriority() failed on iteration %d.  GetLastError = 0x%x\r\n"),dwCnt, GetLastError());
                    bRC = FALSE;
                }
            }
            CHECKTRUE(bRC);
        }
    ENDSTEP(hTest,hStep);
    
    //Starts high priority thread, sets its priority to CE_THREAD_PRIO_256_TIME_CRITICAL
    BEGINSTEP( hTest, hStep, TEXT("Creating high priority thread"));
        if(fDifferentProcessors){
            CHECKTRUE(CeSetThreadAffinity(GetCurrentThread(), 1));
        }
        hPHighThread = CreateThread(NULL,
                                    0,
                                    (LPTHREAD_START_ROUTINE)CS1_PHigh_Thread,
                                    (LPVOID)&ts,
                                    0,
                                    NULL);

        if(FALSE == IsValidHandle(hPHighThread)){
            LogDetail(TEXT("ERROR:  Failed to create High priority thread\r\n"));
            bRC = FALSE;
        }
        else if(FALSE == CeSetThreadPriority(hPHighThread, CE_THREAD_PRIO_256_TIME_CRITICAL)){
            LogDetail(TEXT("ERROR:  Failed to set priority for high priority thread\r\n"));
            bRC = FALSE;
        }
        //We should be blocked here until hPHighThread calls EnterCS and get blocked
        //Now drop our priority level, if priority donation works, we should still be running at hPHighThread's priority level
        else if(FALSE == CeSetThreadPriority(GetCurrentThread(), CE_THREAD_PRIO_256_BELOW_NORMAL)){
            LogDetail(TEXT("CeSetThreadPriority() failed\r\n"));
            bRC = FALSE;
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);
    
    //High prioirty thread gets to run, and calls Sleep(30000)
    
    //Starts n spinning threads and set their priority level to CE_THREAD_PRIO_256_ABOVE_NORMAL
    BEGINSTEP( hTest, hStep, TEXT("Creating spinning threads"))
        LogDetail(TEXT("Creating %d spinning thread(s)...\r\n"),dwNumOfSpinningThds);
        pHndSpnThds = (PHANDLE)LocalAlloc(LPTR, dwNumOfSpinningThds * sizeof(HANDLE));
        CHECKTRUE(pHndSpnThds);
        for(dwCnt = 0; bRC && dwCnt < dwNumOfSpinningThds; dwCnt++){
            PHANDLE pCurHandle = pHndSpnThds + dwCnt;
            *pCurHandle =  CreateThread(NULL,
                                        0,
                                       (LPTHREAD_START_ROUTINE)CS1_Spinning_Thread,
                                       (LPVOID)&fDone,
                                       0,
                                       NULL);
            if(NULL == *pCurHandle){
                bRC = FALSE;
            }
            else if(FALSE == CeSetThreadPriority(*pCurHandle, CE_THREAD_PRIO_256_ABOVE_NORMAL)){
                LogDetail(TEXT("ERROR:  CeSetThreadPriority() failed on iteration %d.  GetLastError = 0x%x\r\n"),dwCnt, GetLastError());
                bRC = FALSE;
            }
            LogDetail(TEXT("Created spinning thread %d...\r\n"),dwCnt);
            //
            //  We'll get blocked after we created n spinning threads since they are all higher priority than us
            // but it's ok.....when high priority thread wakes up, we'll get to run again, and create the rest of the spinning threads
            //
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);
    
    
    LogDetail(TEXT("Primary Thread leaving CS...\r\n"));

    LeaveCriticalSection(&cs);

    //
    //  CS1_PHigh_Thread is unblocked and we could be blocked.
    //


    BEGINSTEP( hTest, hStep, TEXT("Wait for threads to finish"));
        CHECKTRUE(DoWaitForThread(pHndSpnThds, dwNumOfSpinningThds, 1));
        CHECKTRUE(DoWaitForThread(&hPHighThread, 1, 1));
        if(dwNumOfNorPriThreads > 0){
            CHECKTRUE(DoWaitForThread(pHndNorPriThds, dwNumOfNorPriThreads, 1));
        }
    ENDSTEP(hTest,hStep);


    //Closing handles to thread should be done outside of BEGINSTEP / ENDSTEP
    //since we want to do it regardless of test result
    if(pHndSpnThds){
        for(dwCnt = 0; dwCnt < dwNumOfSpinningThds; dwCnt++){
            PHANDLE pCurHandle = pHndSpnThds + dwCnt;
            if(NULL != *pCurHandle){
                CloseHandle(*pCurHandle);
                *pCurHandle = NULL;
            }
        }
        LocalFree((HLOCAL)pHndSpnThds);
        pHndSpnThds = NULL;
    }

    if(pHndNorPriThds){
        for(dwCnt = 0; dwCnt < dwNumOfNorPriThreads; dwCnt++){
            PHANDLE pCurHandle = pHndNorPriThds + dwCnt;
            if(NULL != *pCurHandle){
                CloseHandle(*pCurHandle);
                *pCurHandle = NULL;
            }
        }
        LocalFree((HLOCAL)pHndNorPriThds);
        pHndSpnThds = NULL;
    }

    if(IsValidHandle(hPHighThread)){
        CloseHandle(hPHighThread);
        hPHighThread = NULL;
    }

    DeleteCriticalSection(&cs);

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}



typedef struct SMP_CS2_TS{
    DWORD dwNumOfIterations;
    DWORD dwNumOfIncrementsPerIteration;
    PLONG plSharedData;
    LPCRITICAL_SECTION lpcs;
}SMP_CS2_TS, *LPSMP_CS2_TS;

//
//  Returns 1 if no error.  0 otherwise
//  Multiple CS2_Worker_Thread will use InterlockedIncrement to increment a share variable.  The idea is that
//  we read the initial value of the shared variable when we are inside the CS, use InterlockedIncrement to increment it,
//  but after each increment, we do a Sleep(1) before the next increment.  Worst yet, we don't care about the return value
//  by InterlockedIncrement(), we'll only read the shared varaible again after we're done incrementing lpts->dwNumOfIncrementsPerIteration times.
//  If CS doesn't work, the value we read back will NOT be lInitialValue +  lpts->dwNumOfIncrementsPerIteration  (the other threads will probably increment
//  the shared variable while we're doing Sleep(1).
//
static DWORD CS2_Worker_Thread(LPVOID lpParam)
{
    LPSMP_CS2_TS lpts = (LPSMP_CS2_TS)lpParam;

    for(DWORD dwCnt = 0; dwCnt < lpts->dwNumOfIterations; dwCnt++){

        EnterCriticalSection(lpts->lpcs);
        LONG lInitialValue = *(lpts->plSharedData), lFinalValue;
        for(DWORD dwCnt2 = 0; dwCnt2 < lpts->dwNumOfIncrementsPerIteration; dwCnt2++){
          InterlockedIncrement(lpts->plSharedData);
          Sleep(1);
        }
        lFinalValue = *(lpts->plSharedData);
        LeaveCriticalSection(lpts->lpcs);


        if((lInitialValue +  (LONG)(lpts->dwNumOfIncrementsPerIteration)) != lFinalValue){
            LogDetail(TEXT("ERROR:  lInitialValue = %d, lpts->dwNumOfIncrementsPerIteration = %d, lFinalValue = %d\r\n"),
                         lInitialValue, lpts->dwNumOfIncrementsPerIteration, lFinalValue);
            return 0;
        }
    }
    return 1;
}



//
//  Critical Section Test #2:
//
//  Description:       Multiple threads with different priorities on different processors trying to enter
//                     the same Critical Section.
//
//  Expected Result:   We want to make sure only 1 thread is inside the CS at any given time
//
//  Note:              We cannot gurantee the "highest" priority thread is the one
//                     that enters the CS if the CS is not owned by any thread.
//
TESTPROCAPI
SMP_DEPTH_CS_2(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    HANDLE hTest=NULL, hStep=NULL;
    BOOL bRC = TRUE;
    const DWORD NUM_OF_THREADS = 16;
    const DWORD NUM_OF_ITERATIONS = 100;
    const DWORD NUM_OF_INCREMENT_PER_ITERATION = 20;
    HANDLE hThread[NUM_OF_THREADS]={0};
    DWORD dwCnt;
    SMP_CS2_TS ts; //We will share this thread struct between all the threads since we don't expect any thread to write to this TS, one copy is enough
    CRITICAL_SECTION cs;
    LONG lShared = 0;

    RestoreInitialCondition();

    hTest = StartTest( NULL);
    InitializeCriticalSection(&cs);

    BEGINSTEP( hTest, hStep, TEXT("Creating threads"));
        ts.dwNumOfIterations = NUM_OF_ITERATIONS;
        ts.dwNumOfIncrementsPerIteration = NUM_OF_INCREMENT_PER_ITERATION;
        ts.plSharedData = &lShared;
        ts.lpcs = &cs;

        for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS; dwCnt++){
            hThread[dwCnt] =  CreateThread(NULL,
                                          0,
                                          (LPTHREAD_START_ROUTINE)CS2_Worker_Thread,
                                          (LPVOID)&ts,
                                          CREATE_SUSPENDED,  //Create the thread as suspended so we can change their priority and affinity before they get to run.
                                          NULL);
            if(NULL == hThread[dwCnt]){
                bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
            }
            else if(FALSE == CeSetThreadPriority(hThread[dwCnt], GenerateUserAppThreadPriority())){
                LogDetail(TEXT("ERROR:  CeSetThreadPriority() failed on iteration %d.  GetLastError = 0x%x\r\n"),dwCnt, GetLastError());
                bRC = FALSE;
            }
            else if(FALSE == CeSetThreadAffinity(hThread[dwCnt], GenerateAffinity(TRUE))){
                LogDetail(TEXT("ERROR:  CeSetThreadAffinity() failed on iteration %d.  GetLastError = 0x%x\r\n"),dwCnt, GetLastError());
                bRC = FALSE;
            }
        }
        CHECKTRUE(bRC);//Cannot use CHECKTRUE inside loop
    ENDSTEP(hTest,hStep);

    CeSetThreadPriority(GetCurrentThread(), CE_THREAD_PRIO_256_TIME_CRITICAL-1);  //We need to be the highest priority so the CS2_Worker_Thread threads cannot stop us from resuming all the threads

    //This should be outside BEGINSTEP / ENDSTEP, in case anything went wrong, we still want to resume the threads otherwise we can wait forever
    for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++){
        if(IsValidHandle(hThread[dwCnt])){
            ResumeThread(hThread[dwCnt]);
        }
    }

    BEGINSTEP( hTest, hStep, TEXT("Waiting for threads to finish"));
        CHECKTRUE(DoWaitForThread(&hThread[0], NUM_OF_THREADS, 1));
    ENDSTEP(hTest,hStep);


    //Closing handles to thread should be done outside of BEGINSTEP / ENDSTEP
    //since we want to do it regardless of test result
    for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++){
        if(NULL != hThread[dwCnt]){
            CloseHandle(hThread[dwCnt]);
            hThread[dwCnt] = NULL;
        }
    }


    BEGINSTEP( hTest, hStep, TEXT("Checking for contentions"));
        CHECKTRUE(cs.dwContentions > 0);  //There better be contentions
        LogDetail(TEXT("cs.dwContentions: %d\r\n"),cs.dwContentions);
    ENDSTEP(hTest,hStep);


    DeleteCriticalSection(&cs);

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}
