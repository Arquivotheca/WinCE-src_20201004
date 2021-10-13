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
//  Scheduler Test #1
//
//  Description:   Start a High priority process (KSMP_SCHL1_P1_EXE_NAME).  This process spawns n high priority spinning
//                 threads (where n = number of cores), signals the parent process, and let the high priority threads spins for
//                 20 seconds.
//                 The parent process (ie this process), receives the signal, starts a normal priority process (KSMP_SCHL1_P2_EXE_NAME),
//                 wait for both processes to finish and get their exit time.
//
//  Expect Result: The exit time first thread that exits in the high priority process should
//                 be less than or equal to the exit time for normal priority process.
//
//  Note:          When one of the threads from the high priority process exits, we have a free core and
//                 the normal priority process can run (and exit) while the other threads from the high priority threads are exiting.
//
//
TESTPROCAPI
SMP_DEPTH_SCHL_1(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    HANDLE hTest=NULL, hStep=NULL;
    HANDLE hHighPriProc = NULL, hNormalPriProc = NULL;
    HANDLE hQueue = NULL, hEvt = NULL;
    FILETIME ftHighPriProcExit = {0};

    MSGQUEUEOPTIONS msgOpt = { sizeof(MSGQUEUEOPTIONS),          //dwSize
                               MSGQUEUE_ALLOW_BROKEN,            //dwFlags
                               0,                                //dwMaxMessages
                               sizeof(FILETIME),                 //cbMaxMessage;
                               TRUE                              //bReadAccess;
                              };

    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    hTest = StartTest( NULL);

    RestoreInitialCondition();

    BEGINSTEP( hTest, hStep, TEXT("Creating Message Queue and Event"));
        hQueue = CreateMsgQueue(KSMP_SCHL1_MSGQ_NAME, &msgOpt);
        CHECKTRUE(IsValidHandle(hQueue));
        hEvt = CreateEvent(NULL, FALSE, FALSE, KSMP_SCHL1_EVT_NAME);
        CHECKTRUE(IsValidHandle(hEvt));
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Starting processes"));
        //Must increase our own priority level so we get to start the normal priority process, otherwise we can get
        //preempted by the spinning threads
        PROCESS_INFORMATION pi;
        DWORD dwWaitRet;
        CHECKTRUE(CeSetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGH));

        //Start the high priority process
        CHECKTRUE(CreateProcess(KSMP_SCHL1_P1_EXE_NAME, NULL,NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi));
        CloseHandle(pi.hThread);  //only need the handle to the process, not the thread
        hHighPriProc = pi.hProcess;
        CHECKTRUE(IsValidHandle(hHighPriProc));

        //Wait to make sure all spinning threads are started, hHighPriProc will signal hEvt after it's ready
        dwWaitRet = WaitForSingleObject(hEvt, INFINITE);
        CHECKTRUE(WAIT_OBJECT_0 == dwWaitRet);

        //Start the normal priority process
        CHECKTRUE(CreateProcess(KSMP_SCHL1_P2_EXE_NAME, NULL,NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi));
        CloseHandle(pi.hThread);  //only need the handle to the process, not the thread
        hNormalPriProc = pi.hProcess;
        CHECKTRUE(IsValidHandle(hNormalPriProc));
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Waiting for Processes to finish"));
        CHECKTRUE(DoWaitForProcess(&hHighPriProc, 1, 1));
        CHECKTRUE(DoWaitForProcess(&hNormalPriProc, 1, 1));
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Reading from Message Queue")); //Get the exit time for the last spinning thread from high priority process
        DWORD dwNumOfBytesRead = 0, dwFlags;
        CHECKTRUE(ReadMsgQueue(hQueue,(LPVOID)&ftHighPriProcExit, sizeof(FILETIME), &dwNumOfBytesRead, 2000, &dwFlags));
        CHECKTRUE(sizeof(FILETIME) == dwNumOfBytesRead);
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Getting exit time for Normal Priority process"));
        FILETIME ftPNormalCreateTime , ftPNormalExitTime , ftPNormalKernelTime, ftPNormalUserTime;
        CHECKTRUE(GetProcessTimes(hNormalPriProc, &ftPNormalCreateTime , &ftPNormalExitTime , &ftPNormalKernelTime, &ftPNormalUserTime));
        NKD(TEXT("High Priority process  :  dwHighDateTime = 0x%x  dwLowDateTime = 0x%x"), ftHighPriProcExit.dwHighDateTime, ftHighPriProcExit.dwLowDateTime);
        NKD(TEXT("Normal Priority process:  dwHighDateTime = 0x%x  dwLowDateTime = 0x%x"), ftPNormalExitTime.dwHighDateTime, ftPNormalExitTime.dwLowDateTime);
        //ftHighPriProcExit  should be less than or equal to ftPNormalExitTime
        CHECKTRUE(-1 != CompareFileTime(&ftPNormalExitTime, &ftHighPriProcExit));
    ENDSTEP(hTest,hStep);

    if(IsValidHandle(hQueue)){
        CloseHandle(hQueue);
        hQueue = NULL;
    }

    if(IsValidHandle(hEvt)){
        CloseHandle(hEvt);
        hEvt = NULL;
    }

    if(IsValidHandle(hHighPriProc)){
        CloseHandle(hHighPriProc);
        hHighPriProc = NULL;
    }

    if(IsValidHandle(hNormalPriProc)){
        CloseHandle(hNormalPriProc);
        hNormalPriProc = NULL;
    }

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}


//
//  Spins until the flag is set, returns 1 if no error.  0 otherwise
//
static DWORD SCHL2_Spinning_Thread(LPVOID lpParam)
{
    volatile PBOOL pfDone = (PBOOL)lpParam;

    if(FALSE == CeSetThreadQuantum(GetCurrentThread(), 0)){
        LogDetail(TEXT("CeSetThreadQuantum() failed\r\n"));
        return 0;
    }

    while(FALSE == *pfDone)
        ; //spin

    return 1;
}

static DWORD SCHL2_HighPri_Thread(LPVOID lpParam)
{
    //All we do here is to set the flag to true telling the spinning (normal priority) thread to exit
    LPLONG lpflag = (LPLONG )lpParam;
    InterlockedExchange(lpflag, TRUE);
    return 1;
}

//
//  Scheduler Test #2
//
//  Description:    Starting 2n spinning threads where n is the number of cores with quantum 0.
//                  Now start a new thread with priority above normal.
//
//  Expect Result:  High priority thread should run.
//
//  Note:           In the current implementation, the normal priority spinning threads will spin until
//                  a flag is set, and this flag is set by the high priority process.  In other words,
//                  if the high priority thread does not get to run, the spinning threads will spin forever
//                  and the test will never finish.  Since we're just making sure setting thread quantum to 0
//                  does not affect prevent high priority thread from running (and not how soon the high priority
//                  thread must run after it's spawned), we don't need to keep track of any timing info.
//                  If the high priority thread doesn't get to run, this test will not complete.
//
TESTPROCAPI
SMP_DEPTH_SCHL_2(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    HANDLE hTest=NULL, hStep=NULL;
    BOOL bRC = TRUE, fDone = FALSE;  //Tell the spinning threads when to stop;
    HANDLE hPHighThread = NULL;
    PHANDLE pHandles = NULL; //Not sure how many spinning threads (T2) do we need
    DWORD dwNumOfCpus = CeGetTotalProcessors();
    DWORD dwNumOfSpinningThds = dwNumOfCpus * 2;
    DWORD dwCnt;

    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    hTest = StartTest( NULL);

    RestoreInitialCondition();

    BEGINSTEP( hTest, hStep, TEXT("Creating spinning threads"));
        LogDetail(TEXT("Creating %d spinning thread(s)...\r\n"),dwNumOfSpinningThds);
        pHandles = (PHANDLE)LocalAlloc(LPTR, dwNumOfSpinningThds * sizeof(HANDLE));
        CHECKTRUE(pHandles);
        CHECKTRUE(CeSetThreadQuantum(GetCurrentThread(),0)); //Set ourselves to quantum 0 as well, so other threads w/ equal priority cannot preempt us
        for(dwCnt = 0; bRC && dwCnt < dwNumOfSpinningThds; dwCnt++){
            PHANDLE pCurHandle = pHandles + dwCnt;
            *pCurHandle =  CreateThread(NULL,
                                        0,
                                        (LPTHREAD_START_ROUTINE)SCHL2_Spinning_Thread,
                                        (LPVOID)&fDone,
                                        0,
                                        NULL);
            if(NULL == *pCurHandle){
                bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
            }
        }
        CHECKTRUE(bRC);//Cannot use CHECKTRUE inside loop
    ENDSTEP(hTest,hStep);


    //Since we're creating 2n spinning thread, and we actually need (n-1) of them, we should be reasonable sure we have enough
    //spinning threads spinning by now (note:  we're using the same API to start the high priority thread, so these thread should
    //already be created)
    BEGINSTEP( hTest, hStep, TEXT("Creating high priority thread"));
        hPHighThread = CreateThread(NULL,
                                    0,
                                    (LPTHREAD_START_ROUTINE)SCHL2_HighPri_Thread,
                                    (LPVOID)&fDone,
                                    0,
                                    NULL);

        //We should not use CHECKTRUE here, otherwise we might never leave the CS
        if(FALSE == IsValidHandle(hPHighThread)){
            LogDetail(TEXT("ERROR:  Failed to create High priority thread\r\n"));
            bRC = FALSE;
        }
        else if(FALSE == CeSetThreadPriority(hPHighThread, CE_THREAD_PRIO_256_TIME_CRITICAL)){
            LogDetail(TEXT("ERROR:  Failed to set priority for high priority thread\r\n"));
            bRC = FALSE;
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Waiting for threads to finish"));
        CHECKTRUE(DoWaitForThread(pHandles, dwNumOfSpinningThds, 1));
        CHECKTRUE(DoWaitForThread(&hPHighThread, 1, 1));
    ENDSTEP(hTest,hStep);


    //Closing handles to thread should be done outside of BEGINSTEP / ENDSTEP
    //since we want to do it regardless of test result
    if(pHandles){
        for(dwCnt = 0; dwCnt < dwNumOfSpinningThds; dwCnt++){
            PHANDLE pCurHandle = pHandles + dwCnt;
            if(NULL != *pCurHandle){
                CloseHandle(*pCurHandle);
                *pCurHandle = NULL;
            }
        }
        LocalFree((HLOCAL)pHandles);
        pHandles = NULL;
    }


    if(IsValidHandle(hPHighThread)){
        CloseHandle(hPHighThread);
        hPHighThread = NULL;
    }

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}



typedef struct SMP_SCHL3_TS_SPIN{
    volatile PBOOL pfDone;
    DWORD dwThdId;
    HANDLE hEvt;
}SMP_SCHL3_TS_SPIN, *LPSMP_SCHL3_TS_SPIN;

typedef struct SMP_SCHL3_TS_HIGHPRI{
    DWORD dwTickCount;
    HANDLE hEvt;
}SMP_SCHL3_TS_HIGHPRI, *LPSMP_SCHL3_TS_HIGHPRI;


static VOID Spin()
{
  DWORD dwCnt;
  const DWORD NUM_OF_SPINS = 100000000;
  volatile PDWORD pdwCnt = &dwCnt;  //Tell compiler not to mess with our loop

  for(*pdwCnt = 0; *pdwCnt < NUM_OF_SPINS; (*pdwCnt)++)
      ; //spin

}


//
//  These are the normal prioirty threads.  Thread 0 is responsible for waking up the high priorities threads!
//
static DWORD SCHL3_Spinning_Thread(LPVOID lpParam)
{
    volatile PBOOL pfDone = ((LPSMP_SCHL3_TS_SPIN)lpParam)->pfDone;
    DWORD dwThdId = ((LPSMP_SCHL3_TS_SPIN)lpParam)->dwThdId;
    HANDLE hEvt = ((LPSMP_SCHL3_TS_SPIN)lpParam)->hEvt;

    if(0==dwThdId){
      Sleep(3000);
      SetEvent(hEvt);
    }

    while(FALSE == *pfDone)
        ; //spin

    return 1;
}

//
//  High Priority thread (we will have 2 of them).  Both threads will wait for the same event, when it's signaled
//  by normal priority thread (#0), both high pri threads will get the time and return
//
static DWORD SCHL3_HighPri_Thread(LPVOID lpParam)
{
    LPSMP_SCHL3_TS_HIGHPRI pts = (LPSMP_SCHL3_TS_HIGHPRI)lpParam;

    if(WAIT_OBJECT_0 != WaitForSingleObject(pts->hEvt, INFINITE)){
        LogDetail(TEXT("ERROR: WaitForSingleObject() Failed\r\n"));
        return 0;
    }

    //Get here because we got waken up
    pts->dwTickCount = GetTickCount();

    Spin();  //Spin for a while before exit, so hopefully we force the other high priority thread to run on another core (instead of us exiting and let that thread schedule on this core)

    return 1;
}


//
//  Scheduler Test #3
//
//  Description:     Three threads-  T1 and T2 have priority 0. T3 has priority 251.
//                       Thread 1:   run(1)     wait(3)             runnable(7)
//                       Thread 2:     run(2)          wait(5)   runnable(6)
//                       Thread 3:                  run(4)
//                   Numbers in brackets indicate the sequence whicht he actions should occur.
//
//  Expected Result: Thread 1 at (7) may not get to run immediately since the current design only
//                   gurantees the highest priority thread (which is any of the 2 pri 0 threads)
//                   will be running.
//
//  Note:  We'll check by making sure Thread 1 gets to run within 1 quantum after it becomes runnable.
//         We can gurantee this.
//
TESTPROCAPI
SMP_DEPTH_SCHL_3(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    const DWORD NUM_OF_HIGH_PRI_THEADS = 2;
    HANDLE hTest=NULL, hStep=NULL;
    HANDLE hEvt = NULL;
    BOOL bRC = TRUE, fDone = FALSE;  //Tell the spinning threads when to stop;


    HANDLE hHighPriThds[NUM_OF_HIGH_PRI_THEADS] = {0};
    SMP_SCHL3_TS_HIGHPRI tsHighPri[NUM_OF_HIGH_PRI_THEADS] = {0};

    PHANDLE pHandles = NULL; //Not sure how many spinning threads do we need, but we need them so as soon as the high pri threads are blocked, the cpu should be running something else
    LPSMP_SCHL3_TS_SPIN pThreadStruct = NULL;

    DWORD dwNumOfCpus = CeGetTotalProcessors();
    DWORD dwNumOfSpinningThds = dwNumOfCpus * 2; //This will be our normal priority thread, the very first thread is special
    DWORD dwCnt;

    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    hTest = StartTest( NULL);

    RestoreInitialCondition();

    BEGINSTEP( hTest, hStep, TEXT("Creating Event"));
        hEvt = CreateEvent(NULL, TRUE, FALSE, NULL);
        CHECKTRUE(IsValidHandle(hEvt));
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Creating spinning threads"));
        LogDetail(TEXT("Creating %d spinning thread(s)...\r\n"),dwNumOfSpinningThds);
        pHandles = (PHANDLE)LocalAlloc(LPTR, dwNumOfSpinningThds * sizeof(HANDLE));
        pThreadStruct = (LPSMP_SCHL3_TS_SPIN)LocalAlloc(LPTR, dwNumOfSpinningThds * sizeof(SMP_SCHL3_TS_SPIN));
        CHECKTRUE(pHandles);
        CHECKTRUE(pThreadStruct);

        for(dwCnt = 0; bRC && dwCnt < dwNumOfSpinningThds; dwCnt++){
            PHANDLE pCurHandle = pHandles + dwCnt;
            LPSMP_SCHL3_TS_SPIN pCurThreadStruct = pThreadStruct + dwCnt;

            pCurThreadStruct->pfDone = &fDone;
            pCurThreadStruct->dwThdId = dwCnt;
            pCurThreadStruct->hEvt = hEvt;

            *pCurHandle =  CreateThread(NULL,
                                        0,
                                       (LPTHREAD_START_ROUTINE)SCHL3_Spinning_Thread,  //Can reuse the spinning threads
                                       (LPVOID)pCurThreadStruct,
                                        0,
                                        NULL);
            if(NULL == *pCurHandle){
                bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
            }
        }
        CHECKTRUE(bRC);//Cannot use CHECKTRUE inside loop
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Creating high priority thread"));
        for(dwCnt = 0; dwCnt < NUM_OF_HIGH_PRI_THEADS; dwCnt++){
            tsHighPri[dwCnt].hEvt = hEvt;
            hHighPriThds[dwCnt] = CreateThread(NULL,
                                                0,
                                                (LPTHREAD_START_ROUTINE)SCHL3_HighPri_Thread,
                                                (LPVOID)&tsHighPri[dwCnt],
                                                0,
                                                NULL);

            if(FALSE == IsValidHandle(hHighPriThds[dwCnt])){
              LogDetail(TEXT("ERROR:  Failed to create High priority thread\r\n"));
              bRC = FALSE;
            }
            else if(FALSE == CeSetThreadPriority(hHighPriThds[dwCnt], 0)){
              LogDetail(TEXT("ERROR:  Failed to set priority for high priority thread\r\n"));
              bRC = FALSE;
            }
        }
        CHECKTRUE(bRC);
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Waiting for threads to finish"));
        CHECKTRUE(DoWaitForThread(&hHighPriThds[0], NUM_OF_HIGH_PRI_THEADS, 1));
        //Tell the spinning threads to exit
        InterlockedExchange((LPLONG)&fDone, TRUE);
        CHECKTRUE(DoWaitForThread(pHandles, dwNumOfSpinningThds, 1));
    ENDSTEP(hTest,hStep);


    BEGINSTEP( hTest, hStep, TEXT("Checking Results"));
        DWORD dwQuantum = CeGetThreadQuantum(GetCurrentThread());  //This should be 100ms
        LogDetail(TEXT("tsHighPri[0].dwTickCount = 0x%x\r\n"), tsHighPri[0].dwTickCount);
        LogDetail(TEXT("tsHighPri[1].dwTickCount = 0x%x\r\n"), tsHighPri[1].dwTickCount);
        // The only gurantee we can have is the high priority threads got run within 100ms (one thread quantum) within each other
        if(tsHighPri[0].dwTickCount > tsHighPri[1].dwTickCount)   {
          CHECKTRUE((tsHighPri[0].dwTickCount - tsHighPri[1].dwTickCount) <= dwQuantum);
        }
        else if(tsHighPri[1].dwTickCount > tsHighPri[0].dwTickCount)   {
          CHECKTRUE((tsHighPri[1].dwTickCount - tsHighPri[0].dwTickCount) <= dwQuantum);
        }
    ENDSTEP(hTest,hStep);



    //Closing handles to thread should be done outside of BEGINSTEP / ENDSTEP
    //since we want to do it regardless of test result
    if(pHandles){
        for(dwCnt = 0; dwCnt < dwNumOfSpinningThds; dwCnt++){
            PHANDLE pCurHandle = pHandles + dwCnt;
            if(NULL != *pCurHandle){
                CloseHandle(*pCurHandle);
                *pCurHandle = NULL;
            }
        }
        LocalFree((HLOCAL)pHandles);
        pHandles = NULL;
    }

    if(pThreadStruct){
        LocalFree((HLOCAL)pThreadStruct);
        pThreadStruct = NULL;
    }


    for(dwCnt = 0; dwCnt < NUM_OF_HIGH_PRI_THEADS; dwCnt++){
        if(IsValidHandle(hHighPriThds[dwCnt])){
            CloseHandle(hHighPriThds[dwCnt]);
            hHighPriThds[dwCnt] = NULL;
        }
    }

    if(IsValidHandle(hEvt)){
        CloseHandle(hEvt);
        hEvt = NULL;
    }

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}