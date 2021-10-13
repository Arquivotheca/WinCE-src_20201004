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
// Returns 0 if no error.  1 otherwise
//
static DWORD CacheFlushThread(LPVOID lpParam)
{
  volatile PBOOL pfDone = (PBOOL)lpParam;
  const DWORD BUFFER_SIZE = ONE_PAGE * 1024; 
  PDWORD pdwBuffer = (PDWORD)VirtualAlloc(NULL, BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  DWORD dwCnt = 0;

  if(NULL == pdwBuffer)
    return 1;

  while(FALSE == *pfDone){
        //Write every 4 DWORD, meaning we'll write every 16 bytes (cachelines
        //are usually 16 or 32 bytes, so we'll make all cachelines dirty)
        const DWORD WRITE_OFFSET = 4;
        const DWORD NUM_OF_WRITES = BUFFER_SIZE / (sizeof(DWORD) * WRITE_OFFSET);
        for(int i = 0; i < NUM_OF_WRITES; i++){
            *(pdwBuffer+ i* WRITE_OFFSET) = dwCnt;
        }
        CacheRangeFlush(pdwBuffer, BUFFER_SIZE, CACHE_SYNC_ALL);
        dwCnt = (0xFFFFFFFF == dwCnt) ? 0 : (dwCnt +1); //Write other stuff on the cache on the next iteration
  }

  VirtualFree(pdwBuffer, 0, MEM_RELEASE);
  return 0;

}


//
//  Cache Test - Multiple threads flushing its own user mode virtual address
//
TESTPROCAPI
SMP_DEPTH_CACHE_FLUSH(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    const DWORD NUM_OF_THREADS = 4;
    HANDLE hTest=NULL, hStep=NULL;
    BOOL bRC = TRUE, fDone = FALSE;
    DWORD dwCnt;
    HANDLE hThread[NUM_OF_THREADS] = {0};

    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    RestoreInitialCondition();

    hTest = StartTest( NULL);

    BEGINSTEP( hTest, hStep, TEXT("Creating the threads"));

        for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS; dwCnt++){
            hThread[dwCnt] =  CreateThread(NULL,
                                          0,
                                          (LPTHREAD_START_ROUTINE)CacheFlushThread,
                                          (LPVOID)&fDone,
                                          0,
                                          NULL);
            if(NULL == hThread[dwCnt]){
                bRC = FALSE;  //CHECKTRUE below will retrieve error code for us using GLE.
            }
        }
        CHECKTRUE(bRC);//Cannot use CHECKTRUE inside loop
    ENDSTEP(hTest,hStep);

    Sleep (30*1000);
    InterlockedExchange((LPLONG)&fDone, TRUE);

    BEGINSTEP( hTest, hStep, TEXT("Waiting for threads to finish"));
        CHECKTRUE(DoWaitForThread(&hThread[0], NUM_OF_THREADS, 0));
    ENDSTEP(hTest,hStep);

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}


//
// Regression test for MEDPG 324294
//
TESTPROCAPI
SMP_DEPTH_CACHE_FLUSH_SIMPLE(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{

    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    RestoreInitialCondition();

    LogDetail(TEXT("********************************************************************\r\n"));
    LogDetail(TEXT("* This test calls CacheRangeFlush() on a user-mode address.        *\r\n"));
    LogDetail(TEXT("* If the kernel crash, there might be a bug in the OAL.            *\r\n"));
    LogDetail(TEXT("* In a SMP environment, when we have to flush a user mode address  *\r\n"));
    LogDetail(TEXT("* the OAL should not blindly send IPI (Inter-Processor Interrupt)  *\r\n"));
    LogDetail(TEXT("* to other cores since the other cores might not necessary be      *\r\n"));
    LogDetail(TEXT("* running in the same virtual address space as this core.          *\r\n"));
    LogDetail(TEXT("* Solution 1:  When you get user mode address, ask the other       *\r\n"));
    LogDetail(TEXT("*              cores to flush the entire cache.                    *\r\n"));
    LogDetail(TEXT("* Solution 2:  When you get user mode address, pass the process    *\r\n"));
    LogDetail(TEXT("*              id (ASID in ARM) to the other cores and decide      *\r\n"));
    LogDetail(TEXT("*              whether a flush is necessary.                       *\r\n"));
    LogDetail(TEXT("********************************************************************\r\n"));

    DWORD dwArray = 12345;
    LogDetail(TEXT("dwArray = %d &dwArray = 0x%x\r\n"), dwArray, &dwArray);
    CacheRangeFlush(&dwArray, 4, CACHE_SYNC_ALL);

    RestoreInitialCondition();

    return TPR_PASS;

}